#pragma once

#include <d2d1_1.h>
#include <d3d11.h>
#include <dcomp.h>
#include <dxgi1_2.h>
#include <windows.h>
#include <wrl/client.h>

#include "HaloMath.h"

namespace halo
{
/**
 * Full-monitor, click-through, topmost window that never takes focus. Content is a premultiplied-alpha
 * swap chain attached through DirectComposition and drawn with Direct2D.
 */
class OverlayWindow
{
public:
    static constexpr DWORD kExStyle = WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE |
        WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP;

    bool Create(int monitor);
    HWND Hwnd() const { return hwnd_; }
    ID2D1DeviceContext* Begin();   // cleared to transparent
    void End();
    Vec2 SizePixels() const { return {static_cast<double>(width_), static_cast<double>(height_)}; }
    ID2D1Factory1* Factory() const { return factory_.Get(); }

private:
    HWND hwnd_ = nullptr;
    UINT width_ = 0, height_ = 0;
    Microsoft::WRL::ComPtr<ID3D11Device> d3d_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain_;
    Microsoft::WRL::ComPtr<IDCompositionDevice> dcomp_;
    Microsoft::WRL::ComPtr<IDCompositionTarget> target_;
    Microsoft::WRL::ComPtr<IDCompositionVisual> visual_;
    Microsoft::WRL::ComPtr<ID2D1Factory1> factory_;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> dc_;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> backBuffer_;
};
}
