#pragma once

#include <vector>

#include "HaloMath.h"

/**
 * Port of Source/SoundCompass/SoundVisualization/SoundHaloGeometry.h with the Unreal types replaced.
 * Ring: listener space X = front, Y = right, Z = up, listener at the origin, ring radius = 1, seen by a
 * virtual camera behind and above the listener. Markers: on an ellipse near the screen edge, using the
 * same bearing convention as the ring (front = up, right = right, behind = down).
 */
namespace halo::geom
{
struct View
{
    float pitchDegrees = 30.0f;   // camera looks down by this much
    float cameraDistance = 3.0f;  // in ring radii; smaller = stronger perspective
};

struct Projected
{
    Vec2 screen;          // offset from the ring centre, in ring radii (Y down)
    float depth = 0.0f;   // distance along the view axis
    float scale = 1.0f;   // perspective scale, 1 at the origin's depth
};

inline Vec3 CameraPosition(const View& view)
{
    const double pitch = DegToRad(view.pitchDegrees);
    return {-view.cameraDistance * std::cos(pitch), 0.0, view.cameraDistance * std::sin(pitch)};
}

inline Projected Project(const Vec3& p, const View& view)
{
    const double pitch = DegToRad(view.pitchDegrees);
    const double d = view.cameraDistance;
    const Vec3 camera = CameraPosition(view);
    const Vec3 forward{std::cos(pitch), 0.0, -std::sin(pitch)};
    const Vec3 up{std::sin(pitch), 0.0, std::cos(pitch)};
    const Vec3 v = p - camera;
    const float depth = static_cast<float>(std::max(0.05, Dot(v, forward)));
    const float scale = static_cast<float>(d / depth);
    return {Vec2{v.y, -Dot(v, up)} * scale, depth, scale};
}

/** 0 at the nearest point of the ring, 1 at the farthest. */
inline float FarAmount(float depth, const View& view)
{
    return std::clamp((depth - (view.cameraDistance - 1.0f)) * 0.5f, 0.0f, 1.0f);
}

/** Log-scaled ground distance in ring radii: 0 m = the ring, 5 m ~ 1.66, 15 m ~ 2.08, 30 m ~ 2.37. */
inline float DistanceToRadius(float meters, float maxRadius)
{
    return std::min(maxRadius, 1.0f + 0.45f * std::log(1.0f + std::max(0.0f, meters) / 1.5f));
}

/** Inverse of DistanceToRadius (without the clamp): the distance drawn at a given radius. */
inline float RadiusToDistance(float radius)
{
    return 1.5f * (std::exp((radius - 1.0f) / 0.45f) - 1.0f);
}

/** Point on the ring for a bearing (0 = front, +90 = right). */
inline Vec3 RingPoint(float azimuthDegrees)
{
    const double azimuth = DegToRad(azimuthDegrees);
    return {std::cos(azimuth), std::sin(azimuth), 0.0};
}

/**
 * Unit 3D direction a ring pointer aims along: the bearing, tilted by the elevation angle. The angle is
 * exaggerated (one floor up should visibly tilt) and capped so a pointer never stands fully upright.
 */
inline Vec3 PointerDirection(float azimuthDegrees, float horizontalMeters, float elevationMeters, float exaggeration = 1.3f)
{
    const double cap = DegToRad(75.0);
    const double elevation = std::clamp(std::atan2(static_cast<double>(elevationMeters),
        std::max(static_cast<double>(horizontalMeters), 0.01)) * exaggeration, -cap, cap);
    const double azimuth = DegToRad(azimuthDegrees);
    return {std::cos(elevation) * std::cos(azimuth), std::cos(elevation) * std::sin(azimuth), std::sin(elevation)};
}

/** Point on the outer marker ellipse for a bearing, relative to the screen centre (Y down). */
inline Vec2 EdgePoint(float azimuthDegrees, const Vec2& halfExtent)
{
    const double azimuth = DegToRad(azimuthDegrees);
    return {std::sin(azimuth) * halfExtent.x, -std::cos(azimuth) * halfExtent.y};
}

/** 1 at the listener, 0 at farMeters and beyond, log-shaped so near distances differ most. */
inline float Nearness(float meters, float farMeters)
{
    const float far = std::log(1.0f + std::max(1.0f, farMeters) / 1.5f);
    return 1.0f - std::clamp(std::log(1.0f + std::max(0.0f, meters) / 1.5f) / far, 0.0f, 1.0f);
}

struct Box2
{
    Vec2 min, max;
    Vec2 Centre() const { return (min + max) * 0.5; }
};

/** One sound's marker on screen: icon circle plus an optional label box below it. Pixels. */
struct MarkerLayout
{
    Vec2 anchor;            // where the bearing puts it
    float iconRadius = 0.0f;
    Vec2 labelSize;
    Vec2 iconPos;           // out: anchor, nudged off other markers
    Vec2 labelTopLeft;      // out
};

inline Box2 IconBox(const MarkerLayout& item)
{
    const Vec2 r{item.iconRadius, item.iconRadius};
    return {item.iconPos - r, item.iconPos + r};
}

inline bool Overlaps(const Box2& a, const Box2& b)
{
    return a.min.x < b.max.x && b.min.x < a.max.x && a.min.y < b.max.y && b.min.y < a.max.y;
}

/** Icon plus its label (always centred below it) as one block. */
inline Box2 MarkerBox(const MarkerLayout& item, float gap)
{
    const double halfWidth = std::max(static_cast<double>(item.iconRadius), 0.5 * item.labelSize.x);
    return {item.iconPos + Vec2{-halfWidth, -item.iconRadius},
        item.iconPos + Vec2{halfWidth, item.iconRadius + gap + item.labelSize.y}};
}

/**
 * Screen-space de-overlap. Items are in priority order (most important first). Each marker (icon with its
 * label below) moves as one block, so a label never drifts to another icon. Overlapping blocks are pushed
 * apart along the axis that needs the shorter move; the more important one moves less.
 */
inline void LayoutMarkers(std::vector<MarkerLayout>& items, float gap = 4.0f)
{
    for (MarkerLayout& item : items) { item.iconPos = item.anchor; }
    for (int pass = 0; pass < 24; ++pass)
    {
        for (size_t a = 0; a < items.size(); ++a)
        {
            for (size_t b = a + 1; b < items.size(); ++b)
            {
                const Box2 boxA = MarkerBox(items[a], gap), boxB = MarkerBox(items[b], gap);
                const double overlapX = std::min(boxA.max.x, boxB.max.x) - std::max(boxA.min.x, boxB.min.x) + gap;
                const double overlapY = std::min(boxA.max.y, boxB.max.y) - std::max(boxA.min.y, boxB.min.y) + gap;
                if (overlapX <= 0.0 || overlapY <= 0.0) { continue; }
                const Vec2 delta = boxB.Centre() - boxA.Centre();
                Vec2 push = overlapX <= overlapY ? Vec2{overlapX, 0.0} : Vec2{0.0, overlapY};
                const double along = overlapX <= overlapY ? delta.x : delta.y;
                // Coincident blocks: alternate sides so a stack fans out.
                if (along < 0.0 || (std::fabs(along) < 1e-8 && b % 2 == 0)) { push = -push; }
                items[a].iconPos -= push * 0.3;
                items[b].iconPos += push * 0.7;
            }
        }
    }
    for (MarkerLayout& item : items)
    {
        item.labelTopLeft = item.iconPos + Vec2{-0.5 * item.labelSize.x, item.iconRadius + gap};
    }
}
}
