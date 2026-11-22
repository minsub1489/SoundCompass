#pragma once

#include "CoreMinimal.h"

/**
 * Screen geometry for the halo overlay, independent of the game camera.
 * Ring: listener space X = front, Y = right, Z = up, listener at the origin, ring radius = 1, seen by a
 * virtual camera behind and above the listener. Markers: on an ellipse near the screen edge, using the
 * same bearing convention as the ring (front = up, right = right, behind = down).
 */
namespace SoundHaloGeometry
{
struct FView
{
    float PitchDegrees = 30.0f;   // camera looks down by this much
    float CameraDistance = 3.0f;  // in ring radii; smaller = stronger perspective
};

struct FProjected
{
    FVector2D Screen = FVector2D::ZeroVector; // offset from the ring centre, in ring radii (Y down)
    float Depth = 0.0f;                       // distance along the view axis
    float Scale = 1.0f;                       // perspective scale, 1 at the origin's depth
};

inline FVector CameraPosition(const FView& View)
{
    const float Pitch = FMath::DegreesToRadians(View.PitchDegrees);
    return FVector(-View.CameraDistance * FMath::Cos(Pitch), 0.0f, View.CameraDistance * FMath::Sin(Pitch));
}

inline FProjected Project(const FVector& P, const FView& View)
{
    const float Pitch = FMath::DegreesToRadians(View.PitchDegrees);
    const float D = View.CameraDistance;
    const FVector Camera = CameraPosition(View);
    const FVector Forward(FMath::Cos(Pitch), 0.0f, -FMath::Sin(Pitch));
    const FVector Up(FMath::Sin(Pitch), 0.0f, FMath::Cos(Pitch));
    const FVector V = P - Camera;
    const float Depth = FMath::Max(0.05f, static_cast<float>(FVector::DotProduct(V, Forward)));
    const float Scale = D / Depth;
    return {FVector2D(V.Y, -FVector::DotProduct(V, Up)) * Scale, Depth, Scale};
}

/** 0 at the nearest point of the ring, 1 at the farthest. */
inline float FarAmount(float Depth, const FView& View)
{
    return FMath::Clamp((Depth - (View.CameraDistance - 1.0f)) * 0.5f, 0.0f, 1.0f);
}

/** Log-scaled ground distance in ring radii: 0 m = the ring, 5 m ~ 1.66, 15 m ~ 2.08, 30 m ~ 2.37. */
inline float DistanceToRadius(float Meters, float MaxRadius)
{
    return FMath::Min(MaxRadius, 1.0f + 0.45f * FMath::Loge(1.0f + FMath::Max(0.0f, Meters) / 1.5f));
}

/** Inverse of DistanceToRadius (without the clamp): the distance drawn at a given radius. */
inline float RadiusToDistance(float Radius)
{
    return 1.5f * (FMath::Exp((Radius - 1.0f) / 0.45f) - 1.0f);
}

/** Point on the ring for a bearing (0 = front, +90 = right). */
inline FVector RingPoint(float AzimuthDegrees)
{
    const float Azimuth = FMath::DegreesToRadians(AzimuthDegrees);
    return FVector(FMath::Cos(Azimuth), FMath::Sin(Azimuth), 0.0f);
}

/**
 * Unit 3D direction a ring pointer aims along: the bearing, tilted by the elevation angle. The angle is
 * exaggerated (one floor up should visibly tilt) and capped so a pointer never stands fully upright.
 */
inline FVector PointerDirection(float AzimuthDegrees, float HorizontalMeters, float ElevationMeters, float Exaggeration = 1.3f)
{
    const float Cap = FMath::DegreesToRadians(75.0f);
    const float Elevation = FMath::Clamp(FMath::Atan2(ElevationMeters, FMath::Max(HorizontalMeters, 0.01f)) * Exaggeration, -Cap, Cap);
    const float Azimuth = FMath::DegreesToRadians(AzimuthDegrees);
    return FVector(FMath::Cos(Elevation) * FMath::Cos(Azimuth), FMath::Cos(Elevation) * FMath::Sin(Azimuth), FMath::Sin(Elevation));
}

/** Point on the outer marker ellipse for a bearing, relative to the screen centre (Y down). */
inline FVector2D EdgePoint(float AzimuthDegrees, const FVector2D& HalfExtent)
{
    const float Azimuth = FMath::DegreesToRadians(AzimuthDegrees);
    return FVector2D(FMath::Sin(Azimuth) * HalfExtent.X, -FMath::Cos(Azimuth) * HalfExtent.Y);
}

/** 1 at the listener, 0 at FarMeters and beyond, log-shaped so near distances differ most. */
inline float Nearness(float Meters, float FarMeters)
{
    const float Far = FMath::Loge(1.0f + FMath::Max(1.0f, FarMeters) / 1.5f);
    return 1.0f - FMath::Clamp(FMath::Loge(1.0f + FMath::Max(0.0f, Meters) / 1.5f) / Far, 0.0f, 1.0f);
}

/** One sound's marker on screen: icon circle plus an optional label box below it. Pixels. */
struct FMarkerLayout
{
    FVector2D Anchor = FVector2D::ZeroVector;       // where the bearing puts it
    float IconRadius = 0.0f;
    FVector2D LabelSize = FVector2D::ZeroVector;
    FVector2D IconPos = FVector2D::ZeroVector;      // out: Anchor, nudged off other markers
    FVector2D LabelTopLeft = FVector2D::ZeroVector; // out
};

inline FBox2D IconBox(const FMarkerLayout& Item)
{
    return FBox2D(Item.IconPos - FVector2D(Item.IconRadius), Item.IconPos + FVector2D(Item.IconRadius));
}

inline bool Overlaps(const FBox2D& A, const FBox2D& B)
{
    return A.Min.X < B.Max.X && B.Min.X < A.Max.X && A.Min.Y < B.Max.Y && B.Min.Y < A.Max.Y;
}

/** Icon plus its label (always centred below it) as one block. */
inline FBox2D MarkerBox(const FMarkerLayout& Item, float Gap)
{
    const float HalfWidth = FMath::Max(Item.IconRadius, 0.5f * static_cast<float>(Item.LabelSize.X));
    return FBox2D(Item.IconPos + FVector2D(-HalfWidth, -Item.IconRadius),
        Item.IconPos + FVector2D(HalfWidth, Item.IconRadius + Gap + Item.LabelSize.Y));
}

/**
 * Screen-space de-overlap. Items are in priority order (most important first). Each marker (icon with its
 * label below) moves as one block, so a label never drifts to another icon. Overlapping blocks are pushed
 * apart along the axis that needs the shorter move; the more important one moves less.
 */
inline void LayoutMarkers(TArray<FMarkerLayout>& Items, float Gap = 4.0f)
{
    for (FMarkerLayout& Item : Items) { Item.IconPos = Item.Anchor; }
    for (int32 Pass = 0; Pass < 24; ++Pass)
    {
        for (int32 A = 0; A < Items.Num(); ++A)
        {
            for (int32 B = A + 1; B < Items.Num(); ++B)
            {
                const FBox2D BoxA = MarkerBox(Items[A], Gap), BoxB = MarkerBox(Items[B], Gap);
                const float OverlapX = FMath::Min(BoxA.Max.X, BoxB.Max.X) - FMath::Max(BoxA.Min.X, BoxB.Min.X) + Gap;
                const float OverlapY = FMath::Min(BoxA.Max.Y, BoxB.Max.Y) - FMath::Max(BoxA.Min.Y, BoxB.Min.Y) + Gap;
                if (OverlapX <= 0.0f || OverlapY <= 0.0f) { continue; }
                const FVector2D Delta = BoxB.GetCenter() - BoxA.GetCenter();
                FVector2D Push = OverlapX <= OverlapY ? FVector2D(OverlapX, 0.0f) : FVector2D(0.0f, OverlapY);
                const double Along = OverlapX <= OverlapY ? Delta.X : Delta.Y;
                // Coincident blocks: alternate sides so a stack fans out.
                if (Along < 0.0 || (FMath::IsNearlyZero(Along) && B % 2 == 0)) { Push = -Push; }
                Items[A].IconPos -= Push * 0.3f;
                Items[B].IconPos += Push * 0.7f;
            }
        }
    }
    for (FMarkerLayout& Item : Items)
    {
        Item.LabelTopLeft = Item.IconPos + FVector2D(-0.5f * Item.LabelSize.X, Item.IconRadius + Gap);
    }
}
}
