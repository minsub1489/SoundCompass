#include <string>

#include <wincodec.h>

#include "Config.h"
#include "OverlayWindow.h"
#include "Painter.h"
#include "Protocol.h"
#include "Scene.h"
#include "UdpReceiver.h"

namespace
{
enum HotkeyId { kLabels = 1, kQuit = 2 };

double Now()
{
    static const double frequency = []
    {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return static_cast<double>(f.QuadPart);
    }();
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return static_cast<double>(counter.QuadPart) / frequency;
}

std::filesystem::path ExeDir()
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
}

void Register(HWND hwnd, int id, const std::string& text)
{
    UINT mods = 0, vk = 0;
    if (!halo::ParseHotkey(text, mods, vk) || !RegisterHotKey(hwnd, id, mods | MOD_NOREPEAT, vk))
    {
        OutputDebugStringA(("HaloOverlay: hotkey " + text + " unavailable, continuing without it\n").c_str());
    }
}

/** Exit code 0 when the window has every overlay style and did not take the foreground. */
int SelfTestWindow(const halo::Config& config)
{
    halo::OverlayWindow window;
    if (!window.Create(config.monitor)) { return 1; }
    window.Begin();
    window.End();
    const auto style = static_cast<DWORD>(GetWindowLongPtrW(window.Hwnd(), GWL_EXSTYLE));
    const bool styled = (style & halo::OverlayWindow::kExStyle) == halo::OverlayWindow::kExStyle;
    return styled && GetForegroundWindow() != window.Hwnd() ? 0 : 1;
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR commandLine, int)
{
    // Physical pixels: the overlay must cover the monitor exactly and stay sharp at any scaling.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const halo::Config config = halo::LoadConfig(ExeDir() / "halo_overlay.json");
    if (std::wstring(commandLine).find(L"--selftest-window") != std::wstring::npos) { return SelfTestWindow(config); }

    halo::UdpReceiver receiver(config.port);
    if (!receiver.Ok())
    {
        const std::wstring text = L"Cannot listen on 127.0.0.1:" + std::to_wstring(config.port) + L". Is another overlay running?";
        MessageBoxW(nullptr, text.c_str(), L"SoundCompass Halo", MB_ICONERROR);
        return 1;
    }
    halo::OverlayWindow window;
    if (!window.Create(config.monitor))
    {
        MessageBoxW(nullptr, L"Cannot create the overlay window (Direct3D 11 / DirectComposition).", L"SoundCompass Halo", MB_ICONERROR);
        return 1;
    }
    Register(window.Hwnd(), kLabels, config.labelsHotkey);
    Register(window.Hwnd(), kQuit, config.quitHotkey);

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Microsoft::WRL::ComPtr<IWICImagingFactory> wic;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic));
    halo::Painter painter(window.Begin(), wic.Get(), ExeDir() / "icons");
    window.End();
    halo::PaintParams params;
    params.sizePixels = window.SizePixels();
    params.pitchDegrees = config.viewPitchDegrees;
    params.dpiScale = static_cast<float>(GetDpiForWindow(window.Hwnd())) / 96.0f;

    halo::SceneConfig sceneConfig;
    sceneConfig.srcFilter = config.src;
    halo::Scene scene(sceneConfig);
    bool labels = false;
    bool drawnIdle = false;
    for (;;)
    {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) { return 0; }
            if (msg.message == WM_HOTKEY && msg.wParam == kQuit) { DestroyWindow(window.Hwnd()); }
            if (msg.message == WM_HOTKEY && msg.wParam == kLabels) { labels = !labels; drawnIdle = false; }
            DispatchMessageW(&msg);
        }
        const double now = Now();
        for (const std::string& datagram : receiver.Drain())
        {
            if (auto message = halo::ParseMessage(datagram)) { scene.Apply(*message, now); }
        }

        const bool idle = scene.Idle(now);
        if (!idle || !drawnIdle)
        {
            params.labels = labels;
            params.now = now;
            window.Begin();
            painter.Paint(scene.Snapshot(now), params);
            window.End(); // blocks until vsync while sounds are visible
            drawnIdle = idle;
        }
        if (idle)
        {
            const HANDLE socketEvent = receiver.Event();
            MsgWaitForMultipleObjects(1, &socketEvent, FALSE, INFINITE, QS_ALLINPUT);
        }
    }
}
