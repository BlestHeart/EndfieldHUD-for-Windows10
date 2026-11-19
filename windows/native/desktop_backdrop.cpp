#include "native/desktop_backdrop.hpp"
#include "native/renderer.hpp"
#include "core/source_animation.hpp"
#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#include <winternl.h>
#include <d3d11.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <windows.ui.composition.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Composition.Desktop.h>
#include <string>
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif
#endif

namespace endfield::native {
namespace {
using Microsoft::WRL::ComPtr;
void need(bool value,const char*message){if(!value)throw std::invalid_argument(message);}
void unit(double value,const char*message){need(std::isfinite(value)&&value>=0&&value<=1,message);}
DesktopBackdropTrack track(const ehud::data::Json& value){
    const auto&curve=value["curve"];
    need(curve["group"].string()=="m_FloatCurves"&&curve["path"].string()=="BlurBG"&&curve["attribute"].string()=="m_Alpha","Unsupported original WatchBlur alpha binding");
    const auto parsed=core::source::ScalarCurve::fromJson(curve["raw"]["curve"]);
    const auto keys=parsed.keys();
    need(keys.size()==2&&keys[0].time==0&&keys[1].time>0&&keys[0].weightedMode==0&&keys[1].weightedMode==0&&keys[0].value!=keys[1].value&&std::isfinite(keys[0].outSlope)&&std::isfinite(keys[1].inSlope),"Unsupported original WatchBlur alpha track");
    unit(keys[0].value,"WatchBlur alpha outside unit range");unit(keys[1].value,"WatchBlur alpha outside unit range");
    const auto length=keys[1].time,delta=keys[1].value-keys[0].value;
    DesktopBackdropTrack result{length,keys[0].value,keys[1].value,
        {1.f/3.f,static_cast<float>(keys[0].outSlope*length/(3*delta)),2.f/3.f,static_cast<float>(1-keys[1].inSlope*length/(3*delta))}};
    for(const auto point:result.controlPoints)need(std::isfinite(point),"WatchBlur timing cannot fit source Float controls");
    return result;
}
[[maybe_unused]] void validate(const DesktopBackdropState&s){
    need(s.pixelWidth>0&&s.pixelHeight>0&&s.pixelWidth<=16384&&s.pixelHeight<=16384,"Backdrop dimensions must be in1..16384");
    unit(s.blurAmount,"Backdrop blur opacity outside unit range");unit(s.backgroundDarkness,"Backdrop tint opacity outside unit range");unit(s.sourceOpacity,"Backdrop source opacity outside unit range");
}
}
DesktopBackdropStyle desktopBackdropStyle(bool dark,bool projectionPlane) noexcept{
    if(projectionPlane)return DesktopBackdropStyle{};
    DesktopBackdropStyle result;result.tintWhite=dark?.015:.90;result.vignetteAlpha=dark?std::array<double,3>{0,.06,.38}:std::array<double,3>{0,.02,.14};return result;
}
double DesktopBackdropTrack::alpha(double elapsed)const{
    need(std::isfinite(elapsed)&&std::isfinite(duration)&&duration>0,"Invalid finite WatchBlur time");
    if(elapsed<=0)return startAlpha;if(elapsed>=duration)return endAlpha;
    const core::CubicTiming easing{controlPoints[0],controlPoints[1],controlPoints[2],controlPoints[3]};
    return startAlpha+(endAlpha-startAlpha)*easing.value(elapsed/duration);
}
DesktopBackdropAnimation DesktopBackdropAnimation::fromSource(const ehud::data::Json& source){
    need(source["wrapper"]["_options"]["animEase"].number()==1&&source["default_speed"].number()==1,"Unsupported WatchBlur wrapper clock");
    return {track(source["entrance"]),track(source["exit"])};
}

#ifdef _WIN32
namespace {
namespace wc=winrt::Windows::UI::Composition;
namespace wd=winrt::Windows::UI::Composition::Desktop;
namespace ws=winrt::Windows::System;
winrt::Windows::UI::Color gray(double white,double alpha){
    const auto byte=[](double x){return static_cast<std::uint8_t>(std::lround(std::clamp(x,0.,1.)*255));};
    const auto w=byte(white);return {byte(alpha),w,w,w};
}
template<class T>void close(T&value)noexcept{if(value){try{value.Close();}catch(...){}value=nullptr;}}
void checkResult(HRESULT result,const wchar_t*stage){if(FAILED(result))throw winrt::hresult_error(result,stage);}
// A downsampled, box-blurred capture of the desktop under the hidden HUD.
struct BackdropCapture { std::uint32_t width{},height{}; std::vector<std::uint8_t> pixels; };
bool captureDesktop(HWND window,BackdropCapture& out,bool desktopWindow=false){
    RECT rect{};if(!window||!GetWindowRect(window,&rect))return false;
    const int sourceWidth=rect.right-rect.left,sourceHeight=rect.bottom-rect.top;
    if(sourceWidth<=0||sourceHeight<=0)return false;
    constexpr int divisor=8;
    const int width=std::max(1,sourceWidth/divisor),height=std::max(1,sourceHeight/divisor);
    const HWND sourceWindow=desktopWindow?GetDesktopWindow():nullptr;
    HDC screen=GetDC(sourceWindow);if(!screen)return false;
    HDC memory=CreateCompatibleDC(screen);if(!memory){ReleaseDC(sourceWindow,screen);return false;}
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;
    info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* bits=nullptr;HBITMAP bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!bitmap||!bits){if(bitmap)DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(sourceWindow,screen);return false;}
    const auto previous=SelectObject(memory,bitmap);
    SetStretchBltMode(memory,HALFTONE);SetBrushOrgEx(memory,0,0,nullptr);
    const BOOL copied=StretchBlt(memory,0,0,width,height,screen,rect.left,rect.top,sourceWidth,sourceHeight,SRCCOPY|CAPTUREBLT);
    if(copied){out.width=static_cast<std::uint32_t>(width);out.height=static_cast<std::uint32_t>(height);
        out.pixels.assign(static_cast<std::uint8_t*>(bits),static_cast<std::uint8_t*>(bits)+static_cast<std::size_t>(width)*height*4);}
    SelectObject(memory,previous);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(sourceWindow,screen);return copied!=FALSE;
}
void blurCapture(BackdropCapture& capture){
    if(capture.width<2||capture.height<2)return;
    const int radius=std::max(2,std::min<int>(capture.width,capture.height)/12);
    std::vector<std::uint8_t> temporary(capture.pixels.size());
    const auto pass=[&](const std::vector<std::uint8_t>& source,std::vector<std::uint8_t>& destination,bool horizontal){
        const int width=static_cast<int>(capture.width),height=static_cast<int>(capture.height);
        for(int y=0;y<height;++y)for(int x=0;x<width;++x){
            std::uint32_t r=0,g=0,b=0;int count=0;
            for(int d=-radius;d<=radius;++d){
                const int sx=horizontal?std::clamp(x+d,0,width-1):x;
                const int sy=horizontal?y:std::clamp(y+d,0,height-1);
                const auto* p=&source[(static_cast<std::size_t>(sy)*width+sx)*4];
                r+=p[0];g+=p[1];b+=p[2];++count;
            }
            auto* q=&destination[(static_cast<std::size_t>(y)*width+x)*4];
            q[0]=static_cast<std::uint8_t>(r/count);q[1]=static_cast<std::uint8_t>(g/count);q[2]=static_cast<std::uint8_t>(b/count);q[3]=255;
        }
    };
    pass(capture.pixels,temporary,true);pass(temporary,capture.pixels,false);
}
// The host backdrop attribute exists only on Windows 11 build 22000+. Test the
// real build number: version shims and compatibility modes do not affect
// RtlGetVersion, and on Windows 10 the DWM attribute may report success while
// silently doing nothing.
bool hostBackdropOs() noexcept{
    using RtlGetVersionFn=LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    const auto ntdll=GetModuleHandleW(L"ntdll.dll");
    const auto get=ntdll?reinterpret_cast<RtlGetVersionFn>(reinterpret_cast<void*>(GetProcAddress(ntdll,"RtlGetVersion"))):nullptr;
    if(!get)return false;
    RTL_OSVERSIONINFOW version{};version.dwOSVersionInfoSize=sizeof(version);
    if(get(&version)!=0)return false;
    return version.dwMajorVersion>10||(version.dwMajorVersion==10&&version.dwBuildNumber>=22000);
}
}
struct DesktopBackdrop::Impl {
    DWORD thread{GetCurrentThreadId()};HWND window{};BOOL previousHost{};bool changedAttribute{};
    bool hostBackdrop{};bool compositionFallback{};bool capturedFallback{};bool tintedFallback{};
    wc::Compositor compositor{nullptr};wd::DesktopWindowTarget target{nullptr};
    wc::ContainerVisual root{nullptr},tone{nullptr};wc::SpriteVisual blur{nullptr},tint{nullptr},vignette{nullptr};
    wc::CompositionBackdropBrush hostBrush{nullptr};wc::CompositionColorBrush tintBrush{nullptr};
    wc::CompositionColorBrush fallbackTintBrush{nullptr};
    ComPtr<ID3D11Device> captureDevice;ComPtr<ID3D11DeviceContext> captureContext;
    wc::CompositionGraphicsDevice graphicsDevice{nullptr};wc::CompositionDrawingSurface drawingSurface{nullptr};
    wc::CompositionSurfaceBrush captureBrush{nullptr};std::uint32_t captureWidth{},captureHeight{};
    bool captureExcluded{};std::uint64_t lastLiveCapture{};
    wc::CompositionRadialGradientBrush vignetteBrush{nullptr};
    wc::LayerVisual panel{nullptr};wc::SpriteVisual foreground{nullptr};
    wc::CompositionSurfaceBrush foregroundBrush{nullptr};wc::ICompositionSurface foregroundSurface{nullptr};
    std::shared_ptr<RendererCompositionSurface>rendererSurface;Renderer*renderer{};
    std::array<wc::CompositionColorGradientStop,3>stops{nullptr,nullptr,nullptr};
    DesktopBackdropState state;DesktopBackdropStats counts;
    void onThread()const{if(GetCurrentThreadId()!=thread)throw std::logic_error("Desktop backdrop used outside its owner UI thread");}
    bool detachBridge()noexcept{
        if(!rendererSurface)return true;bool restored=true;
        rendererSurface->unbindResetObserver(this);
        try{if(target)target.Root(nullptr);if(panel)panel.Children().RemoveAll();if(target&&root)target.Root(root);}catch(...){restored=false;}
        try{if(foreground)foreground.Brush(nullptr);if(foregroundBrush)foregroundBrush.Surface(nullptr);}catch(...){restored=false;}
        close(foreground);close(foregroundBrush);foregroundSurface=nullptr;close(panel);
        if(rendererSurface->valid()&&renderer){counts.foregroundRestorationAttempted=true;
            try{renderer->restoreComposition(*rendererSurface);counts.foregroundRestored=!renderer->stats().compositionSuspended;restored&=counts.foregroundRestored;}catch(...){counts.foregroundRestored=false;restored=false;}}
        rendererSurface.reset();renderer=nullptr;counts.foregroundAttached=false;counts.panelOpacity=1;++counts.foregroundDetachments;return restored;
    }
    void clear()noexcept{
        // Caller must destroy on owner thread, before HWND / DispatcherQueue.
        (void)detachBridge();
        if(target)try{target.Root(nullptr);}catch(...){}
        close(target);close(blur);close(tint);close(vignette);close(tone);close(root);
        close(hostBrush);close(fallbackTintBrush);close(tintBrush);close(vignetteBrush);for(auto&stop:stops)close(stop);close(compositor);
        if(changedAttribute){
            counts.hostAttributeRestorationAttempted=true;
            const auto result=window&&IsWindow(window)
                ?DwmSetWindowAttribute(window,DWMWA_USE_HOSTBACKDROPBRUSH,&previousHost,sizeof(previousHost))
                :HRESULT_FROM_WIN32(ERROR_INVALID_WINDOW_HANDLE);
            counts.hostAttributeRestoreResult=static_cast<std::int32_t>(result);
            counts.hostAttributeRestored=SUCCEEDED(result);
        }
        window=nullptr;changedAttribute=false;hostBackdrop=false;compositionFallback=false;capturedFallback=false;tintedFallback=false;captureExcluded=false;lastLiveCapture=0;
        counts.initialized=counts.hostBackdropEnabled=counts.compositionBackdropEnabled=counts.capturedBackdropEnabled=counts.tintedBackdropEnabled=counts.lowerCompositionTarget=false;
    }
    ~Impl(){clear();}
    bool drawCapturedSurface(const BackdropCapture& capture){
        const auto surfaceInterop=drawingSurface.as<ABI::Windows::UI::Composition::ICompositionDrawingSurfaceInterop>();
        ComPtr<ID2D1DeviceContext> context;POINT offset{};
        checkResult(surfaceInterop->BeginDraw(nullptr,__uuidof(ID2D1DeviceContext),reinterpret_cast<void**>(context.GetAddressOf()),&offset),L"Backdrop: begin drawing the captured desktop");
        D2D1_BITMAP_PROPERTIES properties{};properties.pixelFormat=D2D1_PIXEL_FORMAT{DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_IGNORE};properties.dpiX=properties.dpiY=96;
        ComPtr<ID2D1Bitmap> bitmap;
        checkResult(context->CreateBitmap(D2D1_SIZE_U{capture.width,capture.height},capture.pixels.data(),capture.width*4,&properties,bitmap.GetAddressOf()),L"Backdrop: create captured bitmap");
        context->DrawBitmap(bitmap.Get(),D2D1_RECT_F{0,0,static_cast<float>(capture.width),static_cast<float>(capture.height)},1.0f,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,nullptr);
        checkResult(surfaceInterop->EndDraw(),L"Backdrop: end drawing the captured desktop");
        captureWidth=capture.width;captureHeight=capture.height;return true;
    }
    bool createCapturedBackdrop(const DesktopBackdropState&state){
        BackdropCapture capture;if(!captureDesktop(window,capture))return false;blurCapture(capture);
        const HRESULT deviceResult=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT|D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS,
            nullptr,0,D3D11_SDK_VERSION,captureDevice.GetAddressOf(),nullptr,captureContext.GetAddressOf());
        if(FAILED(deviceResult))return false;
        ComPtr<IDXGIDevice> dxgiDevice;if(FAILED(captureDevice.As(&dxgiDevice)))return false;
        ComPtr<ID2D1Factory1> factory;D2D1_FACTORY_OPTIONS factoryOptions{};
        const HRESULT factoryResult=D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,__uuidof(ID2D1Factory1),&factoryOptions,reinterpret_cast<void**>(factory.GetAddressOf()));
        if(FAILED(factoryResult))return false;
        ComPtr<ID2D1Device> d2dDevice;if(FAILED(factory->CreateDevice(dxgiDevice.Get(),d2dDevice.GetAddressOf())))return false;
        const auto interop=compositor.as<ABI::Windows::UI::Composition::ICompositorInterop>();
        winrt::com_ptr<ABI::Windows::UI::Composition::ICompositionGraphicsDevice> graphicsAbi;
        try{checkResult(interop->CreateGraphicsDevice(d2dDevice.Get(),graphicsAbi.put()),L"Backdrop: create composition graphics device");}
        catch(...){return false;}
        graphicsDevice=graphicsAbi.as<wc::CompositionGraphicsDevice>();
        drawingSurface=graphicsDevice.CreateDrawingSurface(winrt::Windows::Foundation::Size{static_cast<float>(capture.width),static_cast<float>(capture.height)},
            winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
            winrt::Windows::Graphics::DirectX::DirectXAlphaMode::Premultiplied);
        if(!drawCapturedSurface(capture))return false;
        captureBrush=compositor.CreateSurfaceBrush(drawingSurface);captureBrush.Stretch(wc::CompositionStretch::Fill);blur.Brush(captureBrush);
        return true;
    }
    bool refreshCapturedBackdrop(){
        BackdropCapture capture;if(!captureDesktop(window,capture,true))return false;blurCapture(capture);
        if(!drawCapturedSurface(capture))return false;
        lastLiveCapture=GetTickCount64();return true;
    }
    void apply(const DesktopBackdropState&next,bool initial){
        if(initial||next.pixelWidth!=state.pixelWidth||next.pixelHeight!=state.pixelHeight){const winrt::Windows::Foundation::Numerics::float2 size{static_cast<float>(next.pixelWidth),static_cast<float>(next.pixelHeight)};root.Size(size);++counts.propertyWrites;
            if(panel){panel.Size(size);foreground.Size(size);counts.propertyWrites+=2;}}
        if(initial||next.dark!=state.dark||next.projectionPlane!=state.projectionPlane){
            const auto style=desktopBackdropStyle(next.dark,next.projectionPlane);tintBrush.Color(gray(style.tintWhite,1));++counts.propertyWrites;
            if(fallbackTintBrush)fallbackTintBrush.Color(gray(next.dark ? .04 : .96,1));
            for(std::size_t i=0;i<stops.size();++i){stops[i].Color(gray(0,style.vignetteAlpha[i]));++counts.propertyWrites;}
        }
        if(initial||next.sourceOpacity!=state.sourceOpacity){tone.Opacity(static_cast<float>(next.sourceOpacity));++counts.propertyWrites;}
        if(initial||next.backgroundDarkness!=state.backgroundDarkness){tint.Opacity(static_cast<float>(next.backgroundDarkness));++counts.propertyWrites;}
        if(initial||next.sourceOpacity!=state.sourceOpacity||next.blurAmount!=state.blurAmount||next.lowPower!=state.lowPower){
            const auto opacity=next.lowPower?0:next.blurAmount*next.sourceOpacity;blur.Opacity(static_cast<float>(opacity));blur.IsVisible(opacity>0);counts.propertyWrites+=2;
        }
        if(initial||(next.sourceOpacity>0)!=(state.sourceOpacity>0)){tone.IsVisible(next.sourceOpacity>0);++counts.propertyWrites;}
        state=next;++counts.updates;
    }
};
DesktopBackdrop::DesktopBackdrop():impl_(std::make_unique<Impl>()){}
DesktopBackdrop::~DesktopBackdrop()=default;
void DesktopBackdrop::initialize(void*handle,const DesktopBackdropState&state,const DesktopBackdropInitialization&options){
    impl_->onThread();validate(state);need(!impl_->counts.initialized,"Reset desktop backdrop before another initialization");
    const auto window=static_cast<HWND>(handle);DWORD process{};
    need(window&&IsWindow(window)&&GetWindowThreadProcessId(window,&process)==GetCurrentThreadId()&&process==GetCurrentProcessId(),"Backdrop must own a current-thread process HWND");
    need((GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_NOREDIRECTIONBITMAP)!=0,"Backdrop HWND requires WS_EX_NOREDIRECTIONBITMAP");
    APTTYPE apartment{};APTTYPEQUALIFIER qualifier{};checkResult(CoGetApartmentType(&apartment,&qualifier),L"Backdrop: inspect caller COM apartment");
    need(apartment==APTTYPE_STA||apartment==APTTYPE_MAINSTA,"Backdrop requires caller-owned STA or ASTA");
    need(bool(ws::DispatcherQueue::GetForCurrentThread()),"Backdrop requires a caller-owned current-thread DispatcherQueue");
    auto candidate=std::make_unique<Impl>();candidate->window=window;
    // The explicit option asserts a state successfully set by this HWND owner.
    // A getter is not documented for this attribute; absent that assertion, a
    // failed getter still leaves the existing HWND completely unmodified.
    // Windows 10 rejects the host backdrop attribute: fall through to the
    // compositor fallback rather than making the HUD unable to start.
    bool hostBackdropAvailable=hostBackdropOs();
    if(hostBackdropAvailable){
        if(options.callerEstablishedHostBackdrop.has_value())candidate->previousHost=*options.callerEstablishedHostBackdrop?TRUE:FALSE;
        else if(SUCCEEDED(DwmGetWindowAttribute(window,DWMWA_USE_HOSTBACKDROPBRUSH,&candidate->previousHost,sizeof(BOOL)))){}
        else {candidate->previousHost=FALSE;}
        candidate->counts.originalHostBackdropEnabled=candidate->previousHost!=FALSE;
        const BOOL enabled=TRUE;
        if(SUCCEEDED(DwmSetWindowAttribute(window,DWMWA_USE_HOSTBACKDROPBRUSH,&enabled,sizeof(enabled)))){candidate->changedAttribute=true;}
        else {hostBackdropAvailable=false;}
    }
    const wchar_t*stage=L"Backdrop: create caller-thread compositor";
    try{
    candidate->compositor=wc::Compositor();
    stage=L"Backdrop: create lower desktop composition target";
    const auto interop=candidate->compositor.as<ABI::Windows::UI::Composition::Desktop::ICompositorDesktopInterop>();
    checkResult(interop->CreateDesktopWindowTarget(window,FALSE,reinterpret_cast<ABI::Windows::UI::Composition::Desktop::IDesktopWindowTarget**>(winrt::put_abi(candidate->target))),stage);
    stage=L"Backdrop: create retained composition visuals";
    candidate->root=candidate->compositor.CreateContainerVisual();candidate->tone=candidate->compositor.CreateContainerVisual();
    candidate->blur=candidate->compositor.CreateSpriteVisual();candidate->tint=candidate->compositor.CreateSpriteVisual();candidate->vignette=candidate->compositor.CreateSpriteVisual();
    candidate->counts.visualAllocations=5;
    candidate->tone.RelativeSizeAdjustment({1,1});candidate->blur.RelativeSizeAdjustment({1,1});candidate->tint.RelativeSizeAdjustment({1,1});candidate->vignette.RelativeSizeAdjustment({1,1});
    stage=L"Backdrop: create host backdrop brush";
    if(hostBackdropAvailable)try{
        candidate->hostBrush=candidate->compositor.CreateHostBackdropBrush();candidate->blur.Brush(candidate->hostBrush);candidate->hostBackdrop=true;
    }catch(...){hostBackdropAvailable=false;}
    if(!hostBackdropAvailable){
        // Windows 10 fallback 1: reproduce the host material from a blurred
        // capture of the desktop beneath this hidden window. This keeps the
        // original dark/light tint and vignette layers exactly over it.
        try{
            if(!candidate->createCapturedBackdrop(state))throw std::runtime_error("captured backdrop unavailable");
            candidate->capturedFallback=true;
        }catch(...){
            // Windows 10 fallback 2: a normal composition backdrop brush.
            try{
                candidate->hostBrush=candidate->compositor.CreateBackdropBrush();candidate->blur.Brush(candidate->hostBrush);candidate->compositionFallback=true;
            }catch(...){
                // Windows 10 fallback 3 (last resort): a neutral tinted brush.
                candidate->fallbackTintBrush=candidate->compositor.CreateColorBrush();
                candidate->fallbackTintBrush.Color(gray(state.dark ? .04 : .96,1));
                candidate->blur.Brush(candidate->fallbackTintBrush);candidate->tintedFallback=true;
            }
        }
    }
    stage=L"Backdrop: create tint and radial gradient brushes";
    candidate->tintBrush=candidate->compositor.CreateColorBrush();candidate->tint.Brush(candidate->tintBrush);
    candidate->vignetteBrush=candidate->compositor.CreateRadialGradientBrush();candidate->vignette.Brush(candidate->vignetteBrush);candidate->counts.brushAllocations=3;
    const auto style=desktopBackdropStyle(state.dark,state.projectionPlane);candidate->vignetteBrush.MappingMode(wc::CompositionMappingMode::Relative);
    candidate->vignetteBrush.EllipseCenter({static_cast<float>(style.vignetteStart[0]),static_cast<float>(style.vignetteStart[1])});
    // Feasibility mapping only: source CAGradientLayer.start/end are preserved
    // above, but radial falloff/color/group opacity need an actual pixel oracle.
    candidate->vignetteBrush.EllipseRadius({static_cast<float>(style.vignetteEnd[0]-style.vignetteStart[0]),static_cast<float>(style.vignetteEnd[1]-style.vignetteStart[1])});
    candidate->vignetteBrush.GradientOriginOffset({0,0});candidate->vignetteBrush.InterpolationSpace(wc::CompositionColorSpace::Rgb);
    for(std::size_t i=0;i<candidate->stops.size();++i){candidate->stops[i]=candidate->compositor.CreateColorGradientStop(static_cast<float>(style.vignetteLocations[i]),gray(0,style.vignetteAlpha[i]));candidate->vignetteBrush.ColorStops().Append(candidate->stops[i]);}
    candidate->tone.Children().InsertAtBottom(candidate->tint);candidate->tone.Children().InsertAtTop(candidate->vignette);
    candidate->root.Children().InsertAtBottom(candidate->blur);candidate->root.Children().InsertAtTop(candidate->tone);
    stage=L"Backdrop: apply retained state and attach lower root";
    candidate->apply(state,true);candidate->target.Root(candidate->root);
    candidate->counts.initialized=candidate->counts.lowerCompositionTarget=true;
    candidate->counts.hostBackdropEnabled=candidate->hostBackdrop;
    candidate->counts.compositionBackdropEnabled=candidate->compositionFallback;
    candidate->counts.capturedBackdropEnabled=candidate->capturedFallback;
    candidate->counts.tintedBackdropEnabled=candidate->tintedFallback;
    }catch(const winrt::hresult_error&error){throw winrt::hresult_error(error.code(),std::wstring(stage)+L": "+error.message().c_str());}
    impl_=std::move(candidate);
}
bool DesktopBackdrop::update(const DesktopBackdropState&state){
    auto&i=*impl_;i.onThread();validate(state);need(i.counts.initialized,"Backdrop is not initialized");
    if(i.rendererSurface)need(i.rendererSurface->valid()&&i.rendererSurface->width()==state.pixelWidth&&i.rendererSurface->height()==state.pixelHeight&&state.projectionPlane,"Resize the Projection renderer before updating its bridged backdrop");
    const auto liveDue=[&]{return state.liveCapture&&i.capturedFallback&&IsWindowVisible(i.window)&&GetTickCount64()-i.lastLiveCapture>=100;};
    if(i.state==state){if(liveDue()&&i.refreshCapturedBackdrop())return true;return false;}
    try{i.apply(state,false);}catch(...){i.clear();throw;}
    if(liveDue())try{(void)i.refreshCapturedBackdrop();}catch(...){}
    return true;
}
bool DesktopBackdrop::attachForeground(Renderer&renderer){
    auto&i=*impl_;i.onThread();need(i.counts.initialized&&i.state.projectionPlane,"Foreground bridge requires an initialized Projection backdrop");
    if(i.rendererSurface){need(i.renderer==&renderer&&i.rendererSurface->valid(),"Backdrop already has another foreground renderer");return false;}
    need(!IsWindowVisible(i.window),"Foreground handoff requires a hidden owner HWND");
    auto borrowed=renderer.borrowCompositionSurface();need(borrowed->window()==i.window&&borrowed->width()==i.state.pixelWidth&&borrowed->height()==i.state.pixelHeight,"Foreground renderer must match backdrop HWND and physical dimensions");
    need(!renderer.stats().compositionSuspended,"Foreground renderer binding is already suspended");
    wc::ICompositionSurface surface{nullptr};wc::CompositionSurfaceBrush brush{nullptr};wc::SpriteVisual foreground{nullptr};wc::LayerVisual panel{nullptr};bool suspended{};
    try{
        const auto interop=i.compositor.as<ABI::Windows::UI::Composition::ICompositorInterop>();
        checkResult(interop->CreateCompositionSurfaceForSwapChain(static_cast<IUnknown*>(borrowed->swapChain()),reinterpret_cast<ABI::Windows::UI::Composition::ICompositionSurface**>(winrt::put_abi(surface))),L"Projection: wrap existing renderer swap chain");
        brush=i.compositor.CreateSurfaceBrush(surface);brush.Stretch(wc::CompositionStretch::None);brush.HorizontalAlignmentRatio(0);brush.VerticalAlignmentRatio(0);
        foreground=i.compositor.CreateSpriteVisual();foreground.Brush(brush);panel=i.compositor.CreateLayerVisual();
        const winrt::Windows::Foundation::Numerics::float2 size{static_cast<float>(i.state.pixelWidth),static_cast<float>(i.state.pixelHeight)};
        foreground.Size(size);panel.Size(size);panel.Opacity(1);
        // Only the two existing target bindings change; no additional HWND,
        // swap chain, bitmap, device, or fullscreen application texture exists.
        suspended=renderer.suspendComposition(*borrowed);i.target.Root(nullptr);
        panel.Children().InsertAtBottom(i.root);panel.Children().InsertAtTop(foreground);i.target.Root(panel);
    }catch(...){
        const auto failure=std::current_exception();try{if(i.target)i.target.Root(nullptr);if(panel)panel.Children().RemoveAll();if(i.target)i.target.Root(i.root);}catch(...){i.clear();}
        try{if(foreground)foreground.Brush(nullptr);if(brush)brush.Surface(nullptr);}catch(...){}close(foreground);close(brush);surface=nullptr;close(panel);
        if(suspended&&borrowed->valid())try{renderer.restoreComposition(*borrowed);}catch(...){}std::rethrow_exception(failure);
    }
    i.renderer=&renderer;i.rendererSurface=std::move(borrowed);i.panel=std::move(panel);i.foreground=std::move(foreground);i.foregroundBrush=std::move(brush);i.foregroundSurface=std::move(surface);
    try{i.rendererSurface->bindResetObserver(&i,[](void*context)noexcept{auto&owner=*static_cast<Impl*>(context);++owner.counts.rendererInvalidations;owner.clear();});}catch(...){(void)i.detachBridge();throw;}
    i.counts.foregroundAttached=true;i.counts.panelOpacity=1;i.counts.foregroundRestorationAttempted=i.counts.foregroundRestored=false;++i.counts.foregroundAttachments;++i.counts.foregroundSurfaceAllocations;i.counts.visualAllocations+=2;++i.counts.brushAllocations;return true;
}
bool DesktopBackdrop::setPanelOpacity(double opacity){
    auto&i=*impl_;i.onThread();unit(opacity,"Projection panel opacity outside unit range");need(i.counts.foregroundAttached&&i.rendererSurface&&i.rendererSurface->valid(),"Attach Projection foreground before changing panel opacity");if(i.counts.panelOpacity==opacity)return false;
    try{i.panel.Opacity(static_cast<float>(opacity));i.counts.panelOpacity=opacity;++i.counts.propertyWrites;return true;}catch(...){i.clear();throw;}
}
bool DesktopBackdrop::detachForeground(){auto&i=*impl_;i.onThread();if(!i.rendererSurface)return false;need(!IsWindowVisible(i.window)||i.counts.panelOpacity==0,"Foreground restoration requires a hidden or fully faded owner");if(!i.detachBridge()){i.clear();throw std::runtime_error("Projection foreground restoration failed; bridge retired");}return true;}
void DesktopBackdrop::reset()noexcept{impl_->clear();}
DesktopBackdropStats DesktopBackdrop::stats()const noexcept{return impl_->counts;}
#endif
} // namespace endfield::native
