#if WITH_DEV_AUTOMATION_TESTS

#include "SoundVisualization/SoundHaloGeometry.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoundHaloGeometryTest, "SoundCompass.SoundHalo.Geometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSoundHaloGeometryTest::RunTest(const FString& Parameters)
{
    using namespace SoundHaloGeometry;
    const FView View;
    const FProjected Origin = Project(FVector::ZeroVector, View);
    const FProjected Front = Project(FVector(1, 0, 0), View);
    const FProjected Back = Project(FVector(-1, 0, 0), View);
    const FProjected Right = Project(FVector(0, 1, 0), View);
    const FProjected Up = Project(FVector(0, 0, 1), View);

    TestTrue(TEXT("Origin projects to the ring centre"), Origin.Screen.IsNearlyZero(1e-4));
    TestTrue(TEXT("Front of the ring is drawn above the centre"), Front.Screen.Y < 0.0);
    TestTrue(TEXT("Back of the ring is drawn below the centre"), Back.Screen.Y > 0.0);
    TestTrue(TEXT("Front band is thinner than the back band"), Front.Scale < Back.Scale);
    TestTrue(TEXT("Front is farther than back"), FarAmount(Front.Depth, View) > FarAmount(Back.Depth, View));
    TestTrue(TEXT("Right is drawn on the right"), Right.Screen.X > 0.0);
    TestTrue(TEXT("Up is drawn above the centre"), Up.Screen.Y < 0.0);
    // Overhead and flat-front both go up on screen; depth tells them apart.
    TestTrue(TEXT("Overhead is nearer than flat front"), Up.Depth < Front.Depth);

    TestTrue(TEXT("0 m sits on the ring"), FMath::IsNearlyEqual(DistanceToRadius(0, 2.4f), 1.0f));
    TestTrue(TEXT("Distance grows monotonically"), DistanceToRadius(5, 2.4f) < DistanceToRadius(15, 2.4f));
    TestTrue(TEXT("Far sounds are clamped"), FMath::IsNearlyEqual(DistanceToRadius(1000, 2.4f), 2.4f));
    const FVector Right10 = SourcePoint(90, 10, 0, 2.4f);
    TestTrue(TEXT("Azimuth +90 is on the right, on the ground"), Right10.Y > 1.0f && FMath::IsNearlyZero(Right10.X, 1e-4f) && FMath::IsNearlyZero(Right10.Z));
    TestTrue(TEXT("Ground radius uses horizontal distance"),
        FMath::IsNearlyEqual(SourcePoint(0, 5, 3, 2.4f).X, DistanceToRadius(4, 2.4f), 1e-4f));
    const FVector Overhead = SourcePoint(0, 4, 4, 2.4f);
    TestTrue(TEXT("Overhead sits above the ring"), Overhead.Z > 0.3f && FMath::IsNearlyEqual(Overhead.X, 1.0f, 1e-4f));
    TestTrue(TEXT("Below is negative height"), SourcePoint(0, 6, -4, 2.4f).Z < 0.0f);
    TestTrue(TEXT("Zero distance is safe"), !SourcePoint(0, 0, 5, 2.4f).ContainsNaN());

    const FVector Axis(0, 1, 0);
    const float Half = FMath::DegreesToRadians(20.0f);
    const FVector P = ConePoint(Axis, 2.0f, Half, 1.3f);
    TestTrue(TEXT("Donut centre lies on the axis"), FMath::IsNearlyEqual(FVector::DotProduct(P, Axis), 2.0f, 1e-4f));
    TestTrue(TEXT("Donut radius grows with distance"),
        FMath::IsNearlyEqual((P - Axis * 2.0f).Size(), 2.0f * FMath::Tan(Half), 1e-4f));
    TestTrue(TEXT("Vertical axis is safe"), !ConePoint(FVector::UpVector, 1.0f, Half, 0.0f).ContainsNaN());
    return true;
}

#endif
