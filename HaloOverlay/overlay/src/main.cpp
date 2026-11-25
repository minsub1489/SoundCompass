#include <fstream>
#include <string>

#include <wincodec.h>
#include <timeapi.h>

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

/** --stats: every 10 s, frames per second and CPU time of Painter::Paint appended to halo_stats.log. */
struct Stats
{
    bool on = false;
    double start = 0.0, paintSum = 0.0, paintMax = 0.0;
    int frames = 0;

    void Frame(double now, double paintSeconds)
    {
        if (!on) { return; }
        if (frames == 0) { start = now - 1e-9; }
        ++frames;
        paintSum += paintSeconds;
        paintMax = std::max(paintMax, paintSeconds);
        if (now - start < 10.0) { return; }
        std::ofstream(ExeDir() / "halo_stats.log", std::ios::app) << "fps=" << frames / (now - start)
            << " paint_ms_mean=" << 1000.0 * paintSum / frames << " paint_ms_max=" << 1000.0 * paintMax << "\n";
        frames = 0;
        paintSum = paintMax = 0.0;
    }
};

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
    if (!halo::RegisterHotkeyText(window.Hwnd(), kQuit, config.quitHotkey))
    {
        // No taskbar entry and no focus: without its quit key the overlay could only be ended from Task Manager.
        const std::wstring key(config.quitHotkey.begin(), config.quitHotkey.end());
        MessageBoxW(nullptr, (L"The quit hotkey " + key + L" is invalid or used by another program.\n"
            L"Set another quit_hotkey in halo_overlay.json next to HaloOverlay.exe.").c_str(), L"SoundCompass Halo", MB_ICONERROR);
        return 1;
    }
    if (!halo::RegisterHotkeyText(window.Hwnd(), kLabels, config.labelsHotkey))
    {
        OutputDebugStringA(("HaloOverlay: labels hotkey " + config.labelsHotkey + " unavailable, continuing without it\n").c_str());
    }
    Stats stats;
    stats.on = std::wstring(commandLine).find(L"--stats") != std::wstring::npos;

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
    constexpr double kFramePeriod = 1.0 / 60.0;
    double lastFrame = -1.0;
    timeBeginPeriod(1); // 1 ms wait resolution for the frame pacing; ended by process exit
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
        const double sinceFrame = now - lastFrame;
        if ((!idle || !drawnIdle) && sinceFrame < kFramePeriod)
        {
            // About 60 Hz whatever the monitor runs at (a 180 Hz monitor would triple the work next to a game).
            const HANDLE socketEvent = receiver.Event();
            const auto waitMs = static_cast<DWORD>((kFramePeriod - sinceFrame) * 1000.0);
            if (waitMs > 0) { MsgWaitForMultipleObjects(1, &socketEvent, FALSE, waitMs, QS_ALLINPUT); }
            continue;
        }
        if (!idle || !drawnIdle)
        {
            lastFrame = now;
            params.labels = labels;
            params.now = now;
            window.Begin();
            const double paintStart = Now();
            painter.Paint(scene.Snapshot(now), params);
            const double paintSeconds = Now() - paintStart;
            window.End(); // blocks until vsync while sounds are visible
            stats.Frame(now, paintSeconds);
            drawnIdle = idle;
        }
        if (idle)
        {
            const HANDLE socketEvent = receiver.Event();
            MsgWaitForMultipleObjects(1, &socketEvent, FALSE, INFINITE, QS_ALLINPUT);
        }
    }
}
