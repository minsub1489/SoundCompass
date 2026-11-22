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

/** Log-scaled distance in ring radii: 0 m = ring, 5 m ~ 1.66, 15 m ~ 2.08, 30 m ~ 2.37. */
inline float DistanceToRadius(float Meters, float MaxRadius)
{
    return FMath::Min(MaxRadius, 1.0f + 0.45f * FMath::Loge(1.0f + FMath::Max(0.0f, Meters) / 1.5f));
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
        * (DistanceToRadius(FMath::Abs(ElevationMeters), MaxRadius) - 1.0f);
    const float Azimuth = FMath::DegreesToRadians(AzimuthDegrees);
    return FVector(FMath::Cos(Azimuth) * Ground, FMath::Sin(Azimuth) * Ground, Height);
}

/** Point on a donut of the cone around Axis: centre at Along * Axis, radius Along * tan(HalfAngle). */
inline FVector ConePoint(const FVector& Axis, float Along, float HalfAngleRadians, float Theta)
{
    const FVector A = (FMath::Abs(Axis.Z) > 0.95f ? FVector::CrossProduct(Axis, FVector::ForwardVector)
        : FVector::CrossProduct(Axis, FVector::UpVector)).GetSafeNormal();
    const FVector B = FVector::CrossProduct(Axis, A);
    const float Radius = Along * FMath::Tan(HalfAngleRadians);
    return Axis * Along + (A * FMath::Cos(Theta) + B * FMath::Sin(Theta)) * Radius;
}
}
