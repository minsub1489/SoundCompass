#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>

#include "HaloMath.h"
#include "Scene.h"

namespace halo
{
struct PaintParams
{
    Vec2 sizePixels;
    float pitchDegrees = 30.0f;
    bool labels = false;
    double now = 0.0;
    float dpiScale = 1.0f;  // monitor DPI / 96; sizes below are in 96-DPI units like the in-game Slate widget
};

/**
 * Direct2D port of USoundHaloWidget::NativePaint (SoundHaloWidget.cpp). Same layout, sizes and colours;
 * the differences for running outside the game are in the design doc, section 8.
 */
class Painter
{
public:
    // Sizes from SoundHaloWidget.h defaults, in pixels on a 900 px tall viewport.
    static constexpr float kRingRadius = 130.0f, kRingThickness = 7.0f, kLevelBulge = 2.0f, kIdleRingOpacity = 0.3f;
    static constexpr float kCameraDistance = 4.5f, kMaxRangeRadius = 2.6f, kEdgeEllipseScale = 0.75f, kNearMarkerRadius = 1.6f;
    static constexpr float kIconSize = 40.0f, kIconOpacity = 0.1f, kFarDistanceMeters = 40.0f, kOnsetWaveSeconds = 0.6f;
    static constexpr float kPointerLength = 0.4f, kVerticalThresholdMeters = 2.0f;

    Painter(ID2D1RenderTarget* target, IWICImagingFactory* wic, const std::filesystem::path& iconsDir);
    void Paint(const std::vector<Mark>& marks, const PaintParams& params);

    struct Rgba { float r = 0, g = 0, b = 0, a = 0; };

private:
    void Line(Vec2 a, Vec2 b, float width, const Rgba& color, bool round);
    void Polyline(const std::vector<Vec2>& points, float width, const Rgba& color);
    void Triangle(Vec2 a, Vec2 b, Vec2 c, const Rgba& color);
    void Polygon(const std::vector<Vec2>& points, const Rgba& color);
    /** Black shadow (filled band) or edge strokes for one ground loop, skipping its hidden (dashed) segments. */
    void LoopShape(const std::vector<Vec2>& outer, const std::vector<Vec2>& inner, const std::vector<float>& alpha,
        bool filled, float alphaScale, float strokeWidth);
    void Disc(Vec2 centre, float radius, const Rgba& color);
    void Rect(Vec2 min, Vec2 max, const Rgba& color);
    void Arc(Vec2 centre, float radius, float a0, float a1, float thickness, const Rgba& color);
    Vec2 MeasureText(const std::wstring& text, IDWriteTextFormat* format) const;
    void Text(Vec2 topLeft, const std::wstring& text, IDWriteTextFormat* format, float alpha);

    ID2D1RenderTarget* rt_;
    Microsoft::WRL::ComPtr<ID2D1Factory> factory_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
    Microsoft::WRL::ComPtr<ID2D1StrokeStyle> flat_, round_;
    Microsoft::WRL::ComPtr<IDWriteFactory> dwrite_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> small_, number_;
    float fontScale_ = 0.0f;
    std::map<std::wstring, Microsoft::WRL::ComPtr<ID2D1Bitmap>> icons_;
};
}
