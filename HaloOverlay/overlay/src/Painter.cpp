#include "Painter.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "Geometry.h"

using Microsoft::WRL::ComPtr;

namespace halo
{
namespace
{
using Rgba = Painter::Rgba;

float Lerp(float a, float b, float t) { return a + (b - a) * t; }
Vec2 Lerp(Vec2 a, Vec2 b, double t) { return a + (b - a) * t; }
Rgba Lerp(const Rgba& a, const Rgba& b, float t) { return {Lerp(a.r, b.r, t), Lerp(a.g, b.g, t), Lerp(a.b, b.b, t), Lerp(a.a, b.a, t)}; }
Rgba WithAlpha(Rgba c, float a) { c.a = a; return c; }
float SmoothStep(float lo, float hi, float x)
{
    if (x <= lo) { return 0.0f; }
    if (x >= hi) { return 1.0f; }
    const float t = (x - lo) / (hi - lo);
    return t * t * (3.0f - 2.0f * t);
}
Vec2 SafeNormal(Vec2 v)
{
    const double length = Length(v);
    return length > 1e-9 ? v * (1.0 / length) : Vec2{};
}
Vec3 Cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }

/** Scale saturation and value in HSV, like FLinearColor::LinearRGBToHSV round trips in the widget. */
Rgba Desaturate(const Rgba& c, float saturation, float value = 1.0f)
{
    const float maxC = std::max({c.r, c.g, c.b}), minC = std::min({c.r, c.g, c.b});
    float h = 0.0f;
    const float delta = maxC - minC;
    if (delta > 1e-6f)
    {
        if (maxC == c.r) { h = std::fmod((c.g - c.b) / delta, 6.0f); }
        else if (maxC == c.g) { h = (c.b - c.r) / delta + 2.0f; }
        else { h = (c.r - c.g) / delta + 4.0f; }
        if (h < 0.0f) { h += 6.0f; }
    }
    const float s = (maxC > 1e-6f ? delta / maxC : 0.0f) * saturation;
    const float v = maxC * value;
    const float chroma = v * s;
    const float x = chroma * (1.0f - std::fabs(std::fmod(h, 2.0f) - 1.0f));
    float r = 0, g = 0, b = 0;
    switch (static_cast<int>(h) % 6)
    {
    case 0: r = chroma; g = x; break;
    case 1: r = x; g = chroma; break;
    case 2: g = chroma; b = x; break;
    case 3: g = x; b = chroma; break;
    case 4: r = x; b = chroma; break;
    default: r = chroma; b = x; break;
    }
    const float m = v - chroma;
    return {r + m, g + m, b + m, c.a};
}

Rgba ColorOf(const std::string& cls)
{
    const ClassStyle& style = StyleOf(cls);
    return {style.r, style.g, style.b, 1.0f};
}

float ThreatOf(const Mark& mark) { return std::clamp(StyleOf(mark.cls).baseImportance, 0.0f, 1.0f); }

/** Newest measured level, 0.2..1 (the analyzer's level is already the dB-mapped 0..1 value). */
float LevelOf(const Mark& mark)
{
    const auto& trail = mark.trail;
    return std::clamp(trail.Count > 0 ? trail.Sample(trail.Count - 1) : 0.0f, 0.2f, 1.0f);
}

/** Newest envelope sample relative to the retained peak. */
float PulseOf(const Mark& mark)
{
    const auto& trail = mark.trail;
    if (trail.Count == 0) { return 1.0f; }
    float peak = 0.0f;
    for (int i = 0; i < trail.Count; ++i) { peak = std::max(peak, trail.Sample(i)); }
    return peak > 0.0f ? trail.Sample(trail.Count - 1) / peak : 1.0f;
}

float HorizontalMeters(const Mark& mark)
{
    const float d = mark.distM.value_or(0.0f);
    const float e = mark.elevM.value_or(0.0f);
    return std::sqrt(std::max(0.0f, d * d - e * e));
}

/** 1 for a sound on the listener's horizontal plane, 0 straight above or below. */
float HorizontalShare(const Mark& mark)
{
    if (!mark.distM || !mark.elevM || *mark.distM <= 1e-4f) { return 1.0f; }
    const float ratio = *mark.elevM / *mark.distM;
    return std::sqrt(std::max(0.0f, 1.0f - ratio * ratio));
}

/** "GlassBreak" -> "Glass Break". */
std::wstring DisplayName(const std::string& cls)
{
    std::wstring out;
    for (size_t i = 0; i < cls.size(); ++i)
    {
        if (i > 0 && std::isupper(static_cast<unsigned char>(cls[i])) && std::islower(static_cast<unsigned char>(cls[i - 1]))) { out += L' '; }
        out += static_cast<wchar_t>(cls[i]);
    }
    return out;
}

D2D1_POINT_2F P(Vec2 v) { return D2D1::Point2F(static_cast<float>(v.x), static_cast<float>(v.y)); }
}

Painter::Painter(ID2D1RenderTarget* target, IWICImagingFactory* wic, const std::filesystem::path& iconsDir) : rt_(target)
{
    rt_->GetFactory(&factory_);
    rt_->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &brush_);
    const auto stroke = [&](D2D1_CAP_STYLE cap, ComPtr<ID2D1StrokeStyle>& out)
    {
        factory_->CreateStrokeStyle(D2D1::StrokeStyleProperties(cap, cap, cap, D2D1_LINE_JOIN_ROUND), nullptr, 0, &out);
    };
    stroke(D2D1_CAP_STYLE_FLAT, flat_);
    stroke(D2D1_CAP_STYLE_ROUND, round_);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(dwrite_.GetAddressOf()));

    // The team's icons, same names as IconFor in SoundHaloWidget.cpp; missing files fall back to the wave glyph.
    for (const wchar_t* name : {L"Airplane", L"Bullet", L"Door", L"Footprint", L"GlassBreak", L"Grenade", L"Throw", L"Wheel"})
    {
        ComPtr<IWICBitmapDecoder> decoder;
        ComPtr<IWICBitmapFrameDecode> frame;
        ComPtr<IWICFormatConverter> converter;
        ComPtr<ID2D1Bitmap> bitmap;
        const std::wstring path = (iconsDir / (std::wstring(name) + L".png")).wstring();
        if (FAILED(wic->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder)) ||
            FAILED(decoder->GetFrame(0, &frame)) || FAILED(wic->CreateFormatConverter(&converter)) ||
            FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0,
                WICBitmapPaletteTypeCustom)) ||
            FAILED(rt_->CreateBitmapFromWicBitmap(converter.Get(), nullptr, &bitmap))) { continue; }
        icons_[name] = bitmap;
    }
}

void Painter::Line(Vec2 a, Vec2 b, float width, const Rgba& c, bool round)
{
    if (c.a <= 0.0f || width <= 0.0f) { return; }
    brush_->SetColor(D2D1::ColorF(c.r, c.g, c.b, c.a));
    rt_->DrawLine(P(a), P(b), brush_.Get(), width, round ? round_.Get() : flat_.Get());
}

void Painter::Polyline(const std::vector<Vec2>& points, float width, const Rgba& c)
{
    if (points.size() < 2 || c.a <= 0.0f) { return; }
    ComPtr<ID2D1PathGeometry> path;
    ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(factory_->CreatePathGeometry(&path)) || FAILED(path->Open(&sink))) { return; }
    sink->BeginFigure(P(points[0]), D2D1_FIGURE_BEGIN_HOLLOW);
    for (size_t i = 1; i < points.size(); ++i) { sink->AddLine(P(points[i])); }
    sink->EndFigure(D2D1_FIGURE_END_OPEN);
    sink->Close();
    brush_->SetColor(D2D1::ColorF(c.r, c.g, c.b, c.a));
    rt_->DrawGeometry(path.Get(), brush_.Get(), width, round_.Get());
}

void Painter::Triangle(Vec2 a, Vec2 b, Vec2 c, const Rgba& color)
{
    if (color.a <= 0.0f) { return; }
    ComPtr<ID2D1PathGeometry> path;
    ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(factory_->CreatePathGeometry(&path)) || FAILED(path->Open(&sink))) { return; }
    sink->BeginFigure(P(a), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(P(b));
    sink->AddLine(P(c));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    brush_->SetColor(D2D1::ColorF(color.r, color.g, color.b, color.a));
    rt_->FillGeometry(path.Get(), brush_.Get());
}

void Painter::Polygon(const std::vector<Vec2>& points, const Rgba& color)
{
    if (points.size() < 3 || color.a <= 0.0f) { return; }
    ComPtr<ID2D1PathGeometry> path;
    ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(factory_->CreatePathGeometry(&path)) || FAILED(path->Open(&sink))) { return; }
    sink->BeginFigure(P(points[0]), D2D1_FIGURE_BEGIN_FILLED);
    for (size_t i = 1; i < points.size(); ++i) { sink->AddLine(P(points[i])); }
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    sink->Close();
    brush_->SetColor(D2D1::ColorF(color.r, color.g, color.b, color.a));
    rt_->FillGeometry(path.Get(), brush_.Get());
}

void Painter::LoopShape(const std::vector<Vec2>& outer, const std::vector<Vec2>& inner, const std::vector<float>& alpha,
    bool filled, float alphaScale, float strokeWidth)
{
    // Segment i joins point i and i+1; runs of visible segments (dashed rings have gaps) become figures.
    const size_t count = alpha.size();
    float sum = 0.0f;
    size_t visible = 0;
    size_t start = count; // a visible segment whose predecessor is hidden
    for (size_t i = 0; i < count; ++i)
    {
        if (alpha[i] <= 0.0f) { continue; }
        sum += alpha[i];
        ++visible;
        if (start == count && alpha[(i + count - 1) % count] <= 0.0f) { start = i; }
    }
    if (visible == 0) { return; }
    const bool whole = visible == count;
    if (whole) { start = 0; }

    ComPtr<ID2D1PathGeometry> path;
    ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(factory_->CreatePathGeometry(&path)) || FAILED(path->Open(&sink))) { return; }
    sink->SetFillMode(D2D1_FILL_MODE_ALTERNATE);
    const auto edge = [&](const std::vector<Vec2>& line, size_t from, size_t segmentsInRun, bool closed)
    {
        sink->BeginFigure(P(line[from]), filled ? D2D1_FIGURE_BEGIN_FILLED : D2D1_FIGURE_BEGIN_HOLLOW);
        for (size_t k = 1; k <= segmentsInRun; ++k) { sink->AddLine(P(line[(from + k) % count])); }
        sink->EndFigure(closed ? D2D1_FIGURE_END_CLOSED : D2D1_FIGURE_END_OPEN);
    };
    for (size_t i = start, done = 0; done < count;)
    {
        if (alpha[i] <= 0.0f) { i = (i + 1) % count; ++done; continue; }
        size_t run = 0;
        while (done + run < count && alpha[(i + run) % count] > 0.0f) { ++run; }
        if (whole)
        {
            edge(outer, 0, count - 1, true);
            edge(inner, 0, count - 1, true); // alternate fill: the band between the two closed edges
        }
        else if (filled)
        {
            sink->BeginFigure(P(outer[i]), D2D1_FIGURE_BEGIN_FILLED);
            for (size_t k = 1; k <= run; ++k) { sink->AddLine(P(outer[(i + k) % count])); }
            for (size_t k = run + 1; k-- > 0;) { sink->AddLine(P(inner[(i + k) % count])); }
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        }
        else
        {
            edge(outer, i, run, false);
            edge(inner, i, run, false);
        }
        i = (i + run) % count;
        done += run;
    }
    sink->Close();
    brush_->SetColor(D2D1::ColorF(0, 0, 0, alphaScale * sum / visible));
    if (filled) { rt_->FillGeometry(path.Get(), brush_.Get()); }
    else { rt_->DrawGeometry(path.Get(), brush_.Get(), strokeWidth, round_.Get()); }
}

void Painter::Disc(Vec2 centre, float radius, const Rgba& c)
{
    if (c.a <= 0.0f) { return; }
    brush_->SetColor(D2D1::ColorF(c.r, c.g, c.b, c.a));
    rt_->FillEllipse(D2D1::Ellipse(P(centre), radius, radius), brush_.Get());
}

void Painter::Rect(Vec2 min, Vec2 max, const Rgba& c)
{
    if (c.a <= 0.0f) { return; }
    brush_->SetColor(D2D1::ColorF(c.r, c.g, c.b, c.a));
    rt_->FillRectangle(D2D1::RectF(static_cast<float>(min.x), static_cast<float>(min.y), static_cast<float>(max.x),
        static_cast<float>(max.y)), brush_.Get());
}

void Painter::Arc(Vec2 centre, float radius, float a0, float a1, float thickness, const Rgba& c)
{
    std::vector<Vec2> points;
    for (int i = 0; i <= 10; ++i)
    {
        const double theta = Lerp(a0, a1, i / 10.0f);
        points.push_back(centre + Vec2{std::cos(theta), std::sin(theta)} * radius);
    }
    Polyline(points, thickness, c);
}

Vec2 Painter::MeasureText(const std::wstring& text, IDWriteTextFormat* format) const
{
    ComPtr<IDWriteTextLayout> layout;
    DWRITE_TEXT_METRICS metrics{};
    if (FAILED(dwrite_->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()), format, 4096, 4096, &layout)) ||
        FAILED(layout->GetMetrics(&metrics))) { return {}; }
    return {metrics.widthIncludingTrailingWhitespace, metrics.height};
}

void Painter::Text(Vec2 topLeft, const std::wstring& text, IDWriteTextFormat* format, float alpha)
{
    brush_->SetColor(D2D1::ColorF(0.95f, 0.97f, 1.0f, alpha));
    rt_->DrawText(text.c_str(), static_cast<UINT32>(text.size()), format,
        D2D1::RectF(static_cast<float>(topLeft.x), static_cast<float>(topLeft.y), static_cast<float>(topLeft.x) + 4096,
            static_cast<float>(topLeft.y) + 4096), brush_.Get());
}

void Painter::Paint(const std::vector<Mark>& marks, const PaintParams& params)
{
    using namespace geom;
    const Vec2 size = params.sizePixels;
    const Vec2 centre = size * 0.5;
    // Slate units are 96-DPI pixels; the widget caps its scale at 1 on tall viewports.
    const float dpi = std::max(0.5f, params.dpiScale);
    const float pixelScale = std::min(1.0f, static_cast<float>(size.y / dpi / 900.0)) * dpi;
    const float radius = kRingRadius * pixelScale;
    const View view{params.pitchDegrees, kCameraDistance};
    const auto toScreen = [&](const Projected& p) { return centre + p.screen * radius; };

    if (fontScale_ != pixelScale && dwrite_)
    {
        fontScale_ = pixelScale;
        // Slate font sizes are points at 96 DPI: 11 pt and 13 pt bold.
        dwrite_->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 11.0f * 96.0f / 72.0f * pixelScale, L"", &small_);
        dwrite_->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, 13.0f * 96.0f / 72.0f * pixelScale, L"", &number_);
    }

    // ---- Centre ring and range rings: closed loops on the ground plane, cut into depth-sorted segments.
    // Direct2D has no per-vertex alpha, so the widget's feathered strips become: one anti-aliased shadow
    // shape per loop (under everything), aliased segment quads sharing exact edges (no seams), and one
    // anti-aliased edge stroke per loop on top that smooths the aliased edges.
    struct Segment { Vec2 quad[4]; Rgba color; float depth; };
    struct Loop { std::vector<Vec2> outer, inner, outerShadow, innerShadow; std::vector<float> alpha; };
    std::vector<Segment> segments;
    std::vector<Loop> loops;
    const auto addLoop = [&](float loopRadius, const std::vector<Rgba>& colors, const std::vector<float>& widths, float farAlpha)
    {
        const size_t count = colors.size();
        std::vector<Vec2> points(count);
        std::vector<float> halves(count), depths(count);
        std::vector<Rgba> tints(count);
        for (size_t i = 0; i < count; ++i)
        {
            const Projected p = Project(RingPoint(360.0f * i / count) * loopRadius, view);
            const float farAmount = FarAmount(p.depth, view);
            Rgba color = Desaturate(colors[i], Lerp(1.0f, 0.45f, farAmount));
            color.a *= Lerp(1.0f, farAlpha, farAmount);
            points[i] = toScreen(p);
            halves[i] = 0.5f * widths[i] * std::min(p.scale, 1.5f) * pixelScale;
            tints[i] = color;
            depths[i] = p.depth;
        }
        Loop loop;
        for (size_t i = 0; i < count; ++i)
        {
            const Vec2 tangent = points[(i + 1) % count] - points[(i + count - 1) % count];
            const Vec2 normal = SafeNormal(Vec2{-tangent.y, tangent.x});
            loop.outer.push_back(points[i] + normal * halves[i]);
            loop.inner.push_back(points[i] - normal * halves[i]);
            loop.outerShadow.push_back(points[i] + normal * (halves[i] + 5.5));
            loop.innerShadow.push_back(points[i] - normal * (halves[i] + 5.5));
        }
        for (size_t i = 0; i < count; ++i)
        {
            const size_t next = (i + 1) % count;
            const Rgba color = Lerp(tints[i], tints[next], 0.5f);
            loop.alpha.push_back(color.a);
            segments.push_back({{loop.outer[i], loop.outer[next], loop.inner[next], loop.inner[i]}, color, 0.5f * (depths[i] + depths[next])});
        }
        loops.push_back(std::move(loop));
    };

    std::vector<std::pair<Vec2, std::wstring>> rangeLabels;
    const auto addRangeRing = [&](float rangeRadius, bool dashed, const std::wstring& label)
    {
        constexpr int kSegments = 120;
        std::vector<Rgba> colors;
        for (int i = 0; i < kSegments; ++i) { colors.push_back({0.85f, 0.95f, 1.0f, dashed && (i / 2) % 2 == 1 ? 0.0f : 0.28f}); }
        addLoop(rangeRadius, colors, std::vector<float>(kSegments, 2.0f), 0.5f);
        rangeLabels.push_back({toScreen(Project(RingPoint(145.0f) * rangeRadius, view)), label});
    };
    for (const float meters : {5.0f, 15.0f, 30.0f})
    {
        const float rangeRadius = DistanceToRadius(meters, kMaxRangeRadius);
        if (rangeRadius < kMaxRangeRadius - 0.15f) { addRangeRing(rangeRadius, false, std::to_wstring(static_cast<int>(meters)) + L"m"); }
    }
    addRangeRing(kMaxRangeRadius, true, std::to_wstring(static_cast<int>(RadiusToDistance(kMaxRangeRadius) / 5.0f) * 5) + L"m+");

    // The nearest sound lights the ring fully; sounds without a distance never count as nearest.
    int nearest = -1;
    for (size_t i = 0; i < marks.size(); ++i)
    {
        if (marks[i].distM && (nearest < 0 || *marks[i].distM < *marks[nearest].distM)) { nearest = static_cast<int>(i); }
    }
    constexpr int kRingSegments = 160;
    std::vector<Rgba> ringColors;
    std::vector<float> ringWidths;
    for (int i = 0; i < kRingSegments; ++i)
    {
        const float bearing = 360.0f * i / kRingSegments;
        Rgba color{0.85f, 0.95f, 1.0f, kIdleRingOpacity};
        float bulge = 0.0f;
        for (size_t s = 0; s < marks.size(); ++s)
        {
            const Mark& mark = marks[s];
            const float delta = static_cast<float>(std::fabs(AngleDiff(bearing, mark.az)));
            const float weight = SmoothStep(0.0f, 1.0f, std::clamp(1.0f - delta / 40.0f, 0.0f, 1.0f)) * mark.alpha *
                (static_cast<int>(s) == nearest ? 1.0f : 0.75f) * HorizontalShare(mark) * HorizontalShare(mark);
            if (weight <= 0.0f) { continue; }
            const float threat = ThreatOf(mark);
            color = Lerp(color, ColorOf(mark.cls), std::min(1.0f, weight * Lerp(1.0f, 1.8f, threat)));
            bulge = std::max(bulge, weight * LevelOf(mark) * (0.4f + 0.6f * PulseOf(mark)) * Lerp(0.6f, 1.3f, threat));
        }
        ringColors.push_back(color);
        ringWidths.push_back(kRingThickness * (1.0f + kLevelBulge * bulge));
    }
    addLoop(1.0f, ringColors, ringWidths, 0.4f);

    // ---- Pointers: a shaded tetrahedron per marked sound with a distance, standing between the range rings.
    struct Face { Vec2 tri[3]; Rgba color; float depth; };
    std::vector<Face> faces;
    const Vec3 cameraAt = CameraPosition(view);
    const Vec3 light = Normalized(Vec3{-0.3, 0.35, 1.0});
    for (const Mark& mark : marks)
    {
        if (!mark.marker || !mark.distM) { continue; }
        const float horizontal = HorizontalMeters(mark);
        const Vec3 aim = PointerDirection(mark.az, horizontal, mark.elevM.value_or(0.0f));
        const float threat = ThreatOf(mark);
        const float length = kPointerLength * Lerp(0.85f, 1.2f, threat);
        const float width = 0.2f * kPointerLength;
        const Vec3 base = RingPoint(mark.az) * std::max(1.04f, DistanceToRadius(horizontal, kMaxRangeRadius));
        const Vec3 side = Normalized(std::fabs(aim.z) > 0.95 ? Cross(aim, Vec3{1, 0, 0}) : Cross(aim, Vec3{0, 0, 1}));
        const Vec3 lift = Cross(side, aim);
        const Vec3 corners[4] = {base + aim * length, base + lift * width,
            base + (lift * -0.5 + side * 0.866) * width, base + (lift * -0.5 - side * 0.866) * width};
        const Vec3 centroid = (corners[0] + corners[1] + corners[2] + corners[3]) * 0.25;
        static constexpr int kFaces[4][3] = {{0, 1, 2}, {0, 2, 3}, {0, 3, 1}, {1, 3, 2}};
        const Rgba color = ColorOf(mark.cls);
        for (const auto& f : kFaces)
        {
            const Vec3 a = corners[f[0]], b = corners[f[1]], c = corners[f[2]];
            const Vec3 faceCentre = (a + b + c) * (1.0 / 3.0);
            Vec3 normal = Normalized(Cross(b - a, c - a));
            if (Dot(normal, faceCentre - centroid) < 0.0) { normal = normal * -1.0; }
            if (Dot(normal, cameraAt - faceCentre) <= 0.0) { continue; } // convex: back faces never show
            const Projected pa = Project(a, view), pb = Project(b, view), pc = Project(c, view);
            const float depth = (pa.depth + pb.depth + pc.depth) / 3.0f;
            const float shade = 0.35f + 0.65f * static_cast<float>(std::max(0.0, Dot(normal, light)));
            Rgba lit = Lerp(Rgba{0, 0, 0, 1}, color, shade);
            lit.a = mark.alpha * Lerp(1.0f, 0.45f, FarAmount(depth, view));
            faces.push_back({{toScreen(pa), toScreen(pb), toScreen(pc)}, lit, depth});
        }
    }

    // Painter's order, far first: the near half of the ring covers pointers behind the listener.
    struct Item { float depth; int segment; int face; };
    std::vector<Item> items;
    for (size_t i = 0; i < segments.size(); ++i) { items.push_back({segments[i].depth, static_cast<int>(i), -1}); }
    for (size_t i = 0; i < faces.size(); ++i) { items.push_back({faces[i].depth, -1, static_cast<int>(i)}); }
    std::stable_sort(items.begin(), items.end(), [](const Item& l, const Item& r) { return l.depth > r.depth; });
    for (const Loop& loop : loops) { LoopShape(loop.outerShadow, loop.innerShadow, loop.alpha, true, 0.22f, 0.0f); } // soft shadow
    for (const Item& item : items)
    {
        if (item.segment >= 0)
        {
            const Segment& s = segments[item.segment];
            rt_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
            Polygon({s.quad[0], s.quad[1], s.quad[2], s.quad[3]}, s.color);
            rt_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            continue;
        }
        const Face& f = faces[item.face];
        const Vec2 mid = (f.tri[0] + f.tri[1] + f.tri[2]) * (1.0 / 3.0);
        const auto grow = [&](Vec2 v) { return v + SafeNormal(v - mid) * 1.2; };
        Triangle(grow(f.tri[0]), grow(f.tri[1]), grow(f.tri[2]), Rgba{0, 0, 0, 0.6f * f.color.a});
        Triangle(f.tri[0], f.tri[1], f.tri[2], f.color);
    }
    for (const Loop& loop : loops) { LoopShape(loop.outer, loop.inner, loop.alpha, false, 0.5f, 1.5f); } // crisp edges

    // ---- Markers: distance is position (near = just outside the ring, far = the edge ellipse).
    const Vec2 halfExtent = size * (0.5 * kEdgeEllipseScale);
    const auto markerAnchor = [&](float bearing, float closeness)
    {
        const Vec2 inner = centre + (toScreen(Project(RingPoint(bearing), view)) - centre) * kNearMarkerRadius;
        return Lerp(centre + EdgePoint(bearing, halfExtent), inner, closeness);
    };
    const Vec2 pillPad{4.0 * pixelScale, 1.0 * pixelScale};
    struct Marker { const Mark* mark; float pixels; Rgba color; float closeness; std::wstring name, distance; };
    std::vector<Marker> markers;
    std::vector<MarkerLayout> layout;
    for (const Mark& mark : marks)
    {
        if (!mark.marker) { continue; }
        // No distance: halfway between the near circle and the edge ellipse.
        const float closeness = mark.distM ? Nearness(*mark.distM, kFarDistanceMeters) : 0.5f;
        const float threat = ThreatOf(mark);
        const float pixels = kIconSize * pixelScale * Lerp(0.85f, 1.25f, threat) * Lerp(0.9f, 1.1f, closeness) * Lerp(0.75f, 1.0f, mark.alpha);
        // Opaque while new to catch the eye, then see-through so it never hides an opponent.
        const float settled = SmoothStep(kOnsetWaveSeconds, kOnsetWaveSeconds + 0.5f, mark.onsetAge);
        const Rgba color = WithAlpha(ColorOf(mark.cls), mark.alpha * Lerp(1.0f, kIconOpacity, settled));
        std::wstring distance = mark.distM ? std::to_wstring(static_cast<int>(std::lround(*mark.distM))) + L"m" : L"?m";
        if (mark.distM && mark.elevM && std::fabs(*mark.elevM) >= kVerticalThresholdMeters)
        {
            distance = std::to_wstring(static_cast<int>(std::lround(HorizontalMeters(mark)))) + L"m " +
                (*mark.elevM > 0.0f ? L"↑" : L"↓") + std::to_wstring(static_cast<int>(std::lround(std::fabs(*mark.elevM)))) + L"m";
        }
        MarkerLayout item;
        item.anchor = markerAnchor(mark.az, closeness);
        item.iconRadius = 0.84f * pixels;
        Marker marker{&mark, pixels, color, closeness, DisplayName(mark.cls), distance};
        if (params.labels && small_ && number_)
        {
            const Vec2 nameSize = MeasureText(marker.name, small_.Get());
            const Vec2 distanceSize = MeasureText(marker.distance, number_.Get());
            item.labelSize = Vec2{std::max(nameSize.x, distanceSize.x), nameSize.y + distanceSize.y} + pillPad * 2.0;
        }
        markers.push_back(marker);
        layout.push_back(item);
    }
    LayoutMarkers(layout);

    for (size_t i = 0; i < markers.size(); ++i)
    {
        const Marker& marker = markers[i];
        const Mark& mark = *marker.mark;
        const MarkerLayout& item = layout[i];
        const Vec2 pos = item.iconPos;
        const float p = marker.pixels;
        const float a = marker.color.a;

        // Onset wave: once, a short arc runs from the icon in to the ring and shrinks.
        if (mark.onsetAge < kOnsetWaveSeconds)
        {
            const float t = mark.onsetAge / kOnsetWaveSeconds;
            const float travel = 1.0f - (1.0f - t) * (1.0f - t);
            std::vector<Vec2> arc;
            for (int step = 0; step <= 8; ++step)
            {
                const float bearing = mark.az + Lerp(-12.0f, 12.0f, step / 8.0f);
                arc.push_back(Lerp(markerAnchor(bearing, marker.closeness), toScreen(Project(RingPoint(bearing), view)), travel));
            }
            const Rgba wave = WithAlpha(marker.color, a * (1.0f - t) * std::min(1.0f, t * 8.0f));
            Polyline(arc, Lerp(7.0f, 3.0f, t) * pixelScale + 2.0f, Rgba{0, 0, 0, 0.4f * wave.a});
            Polyline(arc, Lerp(7.0f, 3.0f, t) * pixelScale, wave);
        }
        if (Length(pos - item.anchor) > 3.0)
        {
            // Nudged off another marker: a dot at the true bearing and a thin line to the icon.
            const Rgba leader = WithAlpha(marker.color, 0.8f * a);
            Line(item.anchor, pos, 2.0f, leader, true);
            Disc(item.anchor, 3.0f * pixelScale, leader);
        }
        // Black line-art icons sit on a saturated class-colour disc with a dark outline.
        Disc(pos, 0.84f * p, Rgba{0, 0, 0, 0.75f * a});
        Disc(pos, 0.78f * p, marker.color);
        Disc(pos, 0.68f * p, WithAlpha(Lerp(marker.color, Rgba{1, 1, 1, 1}, 0.25f), a));

        const Rgba glyph{0.05f, 0.06f, 0.09f, a};
        const wchar_t* icon = StyleOf(mark.cls).icon;
        const auto bitmap = icon ? icons_.find(icon) : icons_.end();
        if (icon && std::wstring(icon) == L"!")
        {
            Rect(pos + Vec2{-0.07, -0.38} * p, pos + Vec2{0.07, 0.12} * p, glyph);
            Disc(pos + Vec2{0.0, 0.28} * p, 0.08f * p, glyph);
        }
        else if (bitmap != icons_.end())
        {
            const float h = 0.5f * p;
            rt_->DrawBitmap(bitmap->second.Get(), D2D1::RectF(static_cast<float>(pos.x) - h, static_cast<float>(pos.y) - h,
                static_cast<float>(pos.x) + h, static_cast<float>(pos.y) + h), a, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        }
        else
        {
            // Speaker-style wave: a dot and three arcs opening to the right.
            const Vec2 origin = pos - Vec2{0.3 * p, 0.0};
            Disc(origin, 0.08f * p, glyph);
            for (const float r : {0.25f, 0.45f, 0.65f}) { Arc(origin, r * p, -0.8f, 0.8f, 0.09f * p, glyph); }
        }

        if (params.labels && small_ && number_)
        {
            const float textAlpha = SmoothStep(0.45f, 0.85f, mark.alpha);
            const Vec2 topLeft = item.labelTopLeft;
            Rect(topLeft, topLeft + item.labelSize, Rgba{0.02f, 0.03f, 0.05f, 0.7f * textAlpha});
            const Vec2 nameSize = MeasureText(marker.name, small_.Get());
            const Vec2 distanceSize = MeasureText(marker.distance, number_.Get());
            const double middleX = topLeft.x + 0.5 * item.labelSize.x;
            Text(Vec2{middleX - 0.5 * nameSize.x, topLeft.y + pillPad.y}, marker.name, small_.Get(), 0.85f * textAlpha);
            Text(Vec2{middleX - 0.5 * distanceSize.x, topLeft.y + pillPad.y + nameSize.y}, marker.distance, number_.Get(), textAlpha);
        }
    }
    if (params.labels && small_)
    {
        for (const auto& [at, label] : rangeLabels)
        {
            const Vec2 textSize = MeasureText(label, small_.Get());
            const Vec2 topLeft = at + Vec2{-0.5 * textSize.x, 3.0};
            Rect(topLeft - pillPad, topLeft + textSize + pillPad, Rgba{0.02f, 0.03f, 0.05f, 0.45f});
            Text(topLeft, label, small_.Get(), 0.9f);
        }
    }
}
}
