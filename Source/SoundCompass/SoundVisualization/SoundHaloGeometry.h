#pragma once

#include "CoreMinimal.h"

/**
 * Virtual 3D scene for the halo overlay, independent of the game camera.
 * Listener space: X = front, Y = right, Z = up, listener at the origin, ring radius = 1.
 * A virtual camera sits behind and above the listener and looks at the origin.
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

inline FProjected Project(const FVector& P, const FView& View)
{
    const float Pitch = FMath::DegreesToRadians(View.PitchDegrees);
    const float D = View.CameraDistance;
    const FVector Camera(-D * FMath::Cos(Pitch), 0.0f, D * FMath::Sin(Pitch));
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

/** 0 m maps here: just outside the ring, so near sounds don't sit on the character's body. */
constexpr float NearRadius = 1.2f;

/** Log-scaled distance in ring radii: 0 m = 1.2, 5 m ~ 1.82, 15 m ~ 2.21, 30 m ~ 2.48. */
inline float DistanceToRadius(float Meters, float MaxRadius)
{
    return FMath::Min(MaxRadius, NearRadius + 0.42f * FMath::Loge(1.0f + FMath::Max(0.0f, Meters) / 1.5f));
}

/** Inverse of DistanceToRadius (without the clamp): the distance drawn at a given radius. */
inline float RadiusToDistance(float Radius)
{
    return 1.5f * (FMath::Exp((Radius - NearRadius) / 0.42f) - 1.0f);
}

/**
 * Scene position of a source from the subsystem's bearing (0 = front, +90 = right).
 * Ground radius comes from the horizontal distance so the range rings read true;
 * height uses the same log scale on the vertical offset.
 */
inline FVector SourcePoint(float AzimuthDegrees, float DistanceMeters, float ElevationMeters, float MaxRadius,
    float HeightScale = 1.0f)
{
    const float Horizontal = FMath::Sqrt(FMath::Max(0.0f, DistanceMeters * DistanceMeters - ElevationMeters * ElevationMeters));
    const float Ground = DistanceToRadius(Horizontal, MaxRadius);
    const float Height = HeightScale * FMath::Sign(ElevationMeters)
        * (DistanceToRadius(FMath::Abs(ElevationMeters), MaxRadius) - NearRadius);
    const float Azimuth = FMath::DegreesToRadians(AzimuthDegrees);
    return FVector(FMath::Cos(Azimuth) * Ground, FMath::Sin(Azimuth) * Ground, Height);
}

/** One sound's marker on screen: icon circle plus a label box. Positions in pixels. */
struct FMarkerLayout
{
    FVector2D Anchor = FVector2D::ZeroVector;      // projected sound position
    float IconRadius = 0.0f;
    FVector2D LabelSize = FVector2D::ZeroVector;
    FVector2D IconPos = FVector2D::ZeroVector;     // out: Anchor, nudged off other markers
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

/** Point on a donut of the cone around Axis: centre at Along * Axis, radius Along * tan(HalfAngle), capped. */
inline FVector ConePoint(const FVector& Axis, float Along, float HalfAngleRadians, float Theta, float MaxRadius = 1e6f)
{
    const FVector A = (FMath::Abs(Axis.Z) > 0.95f ? FVector::CrossProduct(Axis, FVector::ForwardVector)
        : FVector::CrossProduct(Axis, FVector::UpVector)).GetSafeNormal();
    const FVector B = FVector::CrossProduct(Axis, A);
    const float Radius = FMath::Min(MaxRadius, Along * FMath::Tan(HalfAngleRadians));
    return Axis * Along + (A * FMath::Cos(Theta) + B * FMath::Sin(Theta)) * Radius;
}
}
