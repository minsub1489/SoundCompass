// Renders the halo into a 1600x900 WIC bitmap and checks pixels.
#include "Check.h"
#include "Geometry.h"
#include "Painter.h"

#include <d2d1.h>
#include <wincodec.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
using namespace halo;

namespace
{
constexpr UINT kW = 1600, kH = 900;

struct Canvas
{
    ComPtr<IWICImagingFactory> wic;
    ComPtr<ID2D1Factory> factory;
    ComPtr<IWICBitmap> bitmap;
    ComPtr<ID2D1RenderTarget> target;

    Canvas()
    {
        CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic));
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory.GetAddressOf());
        wic->CreateBitmap(kW, kH, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &bitmap);
        factory->CreateWicBitmapRenderTarget(bitmap.Get(), D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)), &target);
    }

    void Paint(const std::vector<Mark>& marks, bool labels)
    {
        Painter painter(target.Get(), wic.Get(), HALO_ICONS_DIR);
        target->BeginDraw();
        target->Clear(D2D1::ColorF(0, 0, 0, 0));
        PaintParams params;
        params.sizePixels = Vec2{kW, kH};
        params.labels = labels;
        painter.Paint(marks, params);
        CHECK(SUCCEEDED(target->EndDraw()));
    }

    std::vector<BYTE> Pixels()
    {
        std::vector<BYTE> out(kW * kH * 4);
        WICRect rect{0, 0, static_cast<INT>(kW), static_cast<INT>(kH)};
        bitmap->CopyPixels(&rect, kW * 4, static_cast<UINT>(out.size()), out.data());
        return out;
    }

    void Save(const wchar_t* path)
    {
        ComPtr<IWICStream> stream;
        ComPtr<IWICBitmapEncoder> encoder;
        ComPtr<IWICBitmapFrameEncode> frame;
        wic->CreateStream(&stream);
        stream->InitializeFromFilename(path, GENERIC_WRITE);
        wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
        encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
        encoder->CreateNewFrame(&frame, nullptr);
        frame->Initialize(nullptr);
        frame->WriteSource(bitmap.Get(), nullptr);
        frame->Commit();
        encoder->Commit();
    }
};

BYTE AlphaAt(const std::vector<BYTE>& px, int x, int y) { return px[(static_cast<size_t>(y) * kW + x) * 4 + 3]; }

bool AnyAlpha(const std::vector<BYTE>& px, Vec2 centre, int half)
{
    for (int y = static_cast<int>(centre.y) - half; y <= static_cast<int>(centre.y) + half; ++y)
    {
        for (int x = static_cast<int>(centre.x) - half; x <= static_cast<int>(centre.x) + half; ++x)
        {
            if (x >= 0 && y >= 0 && x < static_cast<int>(kW) && y < static_cast<int>(kH) && AlphaAt(px, x, y) > 0) { return true; }
        }
    }
    return false;
}

Mark MakeMark(const char* cls, float az, std::optional<float> dist, float fbHalf = 1.0f)
{
    Mark m;
    m.cls = cls;
    m.az = az;
    m.distM = dist;
    m.alpha = fbHalf;
    m.onsetAge = 2.0f;   // settled, no onset wave
    m.marker = true;
    m.importance = 0.8f;
    m.trail.Observe(0.6f, 0.0, 0.0f, 0.6f, 0.8f);
    return m;
}

const Vec2 kCentre{kW * 0.5, kH * 0.5};
}

static void empty_scene_is_transparent()
{
    Canvas canvas;
    canvas.Paint({}, false);
    const auto px = canvas.Pixels();
    CHECK(AlphaAt(px, 800, 450) == 0);
    CHECK(AlphaAt(px, 0, 0) == 0 && AlphaAt(px, kW - 1, 0) == 0 && AlphaAt(px, 0, kH - 1) == 0 && AlphaAt(px, kW - 1, kH - 1) == 0);
    // Front ring point: ring radius 130 px at 900 px tall, default pitch 30, camera distance 4.5.
    const geom::Projected front = geom::Project(geom::RingPoint(0), geom::View{30.0f, 4.5f});
    CHECK(AnyAlpha(px, kCentre + front.screen * 130.0, 2));
}

static void centre_stays_clear()
{
    Canvas canvas;
    canvas.Paint({MakeMark("Gunshot", 0, 2.0f)}, false);
    CHECK(AlphaAt(canvas.Pixels(), 800, 450) == 0);
}

static void right_sound_draws_on_right()
{
    Canvas canvas;
    canvas.Paint({MakeMark("Gunshot", 90, 30.0f)}, false);
    const auto px = canvas.Pixels();
    // Edge ellipse at 0.75 of the half screen, nearness of 30 m pulls it slightly inward.
    const Vec2 edge = kCentre + geom::EdgePoint(90, Vec2{kW * 0.5 * 0.75, kH * 0.5 * 0.75});
    const Vec2 mirrored{2 * kCentre.x - edge.x, edge.y};
    bool right = false;
    for (int dx = -120; dx <= 0 && !right; dx += 10) { right = AnyAlpha(px, edge + Vec2{static_cast<double>(dx), 0}, 20); }
    bool left = false;
    for (int dx = 0; dx <= 120 && !left; dx += 10) { left = AnyAlpha(px, mirrored + Vec2{static_cast<double>(dx), 0}, 20); }
    CHECK(right);
    CHECK(!left);
}

static void no_text_by_default()
{
    Canvas plain, labelled;
    plain.Paint({MakeMark("Door", -45, 8.0f)}, false);
    labelled.Paint({MakeMark("Door", -45, 8.0f)}, true);
    CHECK(plain.Pixels() != labelled.Pixels());
}

static void sample_image()
{
    Canvas canvas;
    std::vector<Mark> marks = {MakeMark("Gunshot", 0, 30.0f), MakeMark("Footstep", 90, 4.0f),
        MakeMark("Door", -90, 12.0f), MakeMark("Explosion", 170, 20.0f)};
    Mark front = MakeMark("Voice", 40, std::nullopt, 0.5f);
    front.mirrored = true;
    Mark back = front;
    back.az = 140;
    marks.push_back(front);
    marks.push_back(back);
    canvas.Paint(marks, false);
    canvas.Save(L"painter_sample.png");
}

int main()
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    RUN(empty_scene_is_transparent);
    RUN(centre_stays_clear);
    RUN(right_sound_draws_on_right);
    RUN(no_text_by_default);
    if (g_failures == 0) { sample_image(); }
    return g_failures;
}
