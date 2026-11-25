#include "OverlayWindow.h"

#include <vector>

using Microsoft::WRL::ComPtr;

namespace halo
{
namespace
{
LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_NCHITTEST: return HTTRANSPARENT;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    default: return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

BOOL CALLBACK CollectMonitor(HMONITOR, HDC, LPRECT rect, LPARAM data)
{
    reinterpret_cast<std::vector<RECT>*>(data)->push_back(*rect);
    return TRUE;
}

RECT MonitorRect(int index)
{
    std::vector<RECT> rects;
    EnumDisplayMonitors(nullptr, nullptr, CollectMonitor, reinterpret_cast<LPARAM>(&rects));
    if (index >= 0 && index < static_cast<int>(rects.size()) && index != 0) { return rects[index]; }
    MONITORINFO info{sizeof(info)};
    GetMonitorInfoW(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY), &info);
    return info.rcMonitor; // 0 and out-of-range indices: the primary monitor
}
}

bool OverlayWindow::Create(int monitor)
{
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"SoundCompassHaloOverlay";
    RegisterClassExW(&wc);

    const RECT rect = MonitorRect(monitor);
    width_ = static_cast<UINT>(rect.right - rect.left);
    height_ = static_cast<UINT>(rect.bottom - rect.top);
    hwnd_ = CreateWindowExW(kExStyle, wc.lpszClassName, L"SoundCompass Halo", WS_POPUP, rect.left, rect.top,
        static_cast<int>(width_), static_cast<int>(height_), nullptr, nullptr, instance, nullptr);
    if (!hwnd_) { return false; }
    SetLayeredWindowAttributes(hwnd_, 0, 255, LWA_ALPHA); // layered + transparent = mouse clicks pass through

    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr, 0, D3D11_SDK_VERSION, &d3d_, nullptr, nullptr))) { return false; }
    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIFactory2> dxgiFactory;
    if (FAILED(d3d_.As(&dxgiDevice)) || FAILED(dxgiDevice->GetAdapter(&adapter)) ||
        FAILED(adapter->GetParent(IID_PPV_ARGS(&dxgiFactory)))) { return false; }

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = width_;
    desc.Height = height_;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    if (FAILED(dxgiFactory->CreateSwapChainForComposition(d3d_.Get(), &desc, nullptr, &swapChain_))) { return false; }

    if (FAILED(DCompositionCreateDevice(dxgiDevice.Get(), IID_PPV_ARGS(&dcomp_))) ||
        FAILED(dcomp_->CreateTargetForHwnd(hwnd_, TRUE, &target_)) || FAILED(dcomp_->CreateVisual(&visual_)) ||
        FAILED(visual_->SetContent(swapChain_.Get())) || FAILED(target_->SetRoot(visual_.Get())) ||
        FAILED(dcomp_->Commit())) { return false; }

    ComPtr<ID2D1Device> d2dDevice;
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory_.GetAddressOf())) ||
        FAILED(factory_->CreateDevice(dxgiDevice.Get(), &d2dDevice)) ||
        FAILED(d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc_))) { return false; }
    ComPtr<IDXGISurface> surface;
    const D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    if (FAILED(swapChain_->GetBuffer(0, IID_PPV_ARGS(&surface))) ||
        FAILED(dc_->CreateBitmapFromDxgiSurface(surface.Get(), &props, &backBuffer_))) { return false; }
    dc_->SetTarget(backBuffer_.Get());

    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    return true;
}

ID2D1DeviceContext* OverlayWindow::Begin()
{
    dc_->BeginDraw();
    dc_->Clear(D2D1::ColorF(0, 0, 0, 0));
    return dc_.Get();
}

void OverlayWindow::End()
{
    dc_->EndDraw();
    swapChain_->Present(1, 0); // vsync paces the redraw loop
}
}
