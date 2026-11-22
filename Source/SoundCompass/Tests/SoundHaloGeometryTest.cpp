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
    const FProjected Front = Project(RingPoint(0), View);
    const FProjected Back = Project(RingPoint(180), View);
    const FProjected Right = Project(RingPoint(90), View);

    TestTrue(TEXT("Origin projects to the ring centre"), Origin.Screen.IsNearlyZero(1e-4));
    TestTrue(TEXT("Front of the ring is drawn above the centre"), Front.Screen.Y < 0.0);
    TestTrue(TEXT("Back of the ring is drawn below the centre"), Back.Screen.Y > 0.0);
    TestTrue(TEXT("Front band is thinner than the back band"), Front.Scale < Back.Scale);
    TestTrue(TEXT("Front is farther than back"), FarAmount(Front.Depth, View) > FarAmount(Back.Depth, View));
    TestTrue(TEXT("Right is drawn on the right"), Right.Screen.X > 0.0);

    // The outer ellipse uses the ring's convention, so ring and markers agree on every bearing.
    const FVector2D Half(800.0, 400.0);
    TestTrue(TEXT("Edge: front is the top"), EdgePoint(0, Half).Equals(FVector2D(0.0, -400.0), 1e-3));
    TestTrue(TEXT("Edge: right is the right"), EdgePoint(90, Half).Equals(FVector2D(800.0, 0.0), 1e-3));
    TestTrue(TEXT("Edge: behind is the bottom"), EdgePoint(180, Half).Equals(FVector2D(0.0, 400.0), 1e-3));
    for (const float Bearing : {30.0f, 120.0f, -60.0f, -150.0f})
    {
        const FVector2D Edge = EdgePoint(Bearing, Half);
        const FVector2D Ring = Project(RingPoint(Bearing), View).Screen;
        TestTrue(FString::Printf(TEXT("Edge and ring agree on side at %.0f deg"), Bearing),
            FMath::Sign(Edge.X) == FMath::Sign(Ring.X) && FMath::Sign(Edge.Y) == FMath::Sign(Ring.Y));
    }

    TestTrue(TEXT("0 m sits on the ring"), FMath::IsNearlyEqual(DistanceToRadius(0, 2.6f), 1.0f));
    TestTrue(TEXT("Distance grows monotonically"), DistanceToRadius(5, 2.6f) < DistanceToRadius(15, 2.6f));
    TestTrue(TEXT("Far sounds are clamped"), FMath::IsNearlyEqual(DistanceToRadius(1000, 2.6f), 2.6f));
    TestTrue(TEXT("RadiusToDistance inverts DistanceToRadius"), FMath::IsNearlyEqual(RadiusToDistance(DistanceToRadius(12, 9)), 12.0f, 1e-3f));

    TestTrue(TEXT("Pointer: level front sound points straight ahead"), PointerDirection(0, 10, 0).Equals(FVector(1, 0, 0), 1e-4));
    const FVector Upstairs = PointerDirection(90, 1.5f, 5);
    TestTrue(TEXT("Pointer: sound above-right tilts up and right"), Upstairs.Z > 0.5f && Upstairs.Y > 0.0f);
    const FVector Basement = PointerDirection(180, 5, -5);
    TestTrue(TEXT("Pointer: sound below-behind tilts down and back"), Basement.Z < 0.0f && Basement.X < 0.0f);
    TestTrue(TEXT("Pointer: straight up is capped below vertical"),
        PointerDirection(0, 0, 100).Z <= FMath::Sin(FMath::DegreesToRadians(75.0f)) + 1e-4f);
    TestTrue(TEXT("Pointer: unit length"), FMath::IsNearlyEqual(PointerDirection(37, 3, 2).Size(), 1.0f, 1e-4f));

    TestTrue(TEXT("Nearness is 1 at the listener"), FMath::IsNearlyEqual(Nearness(0, 40), 1.0f));
    TestTrue(TEXT("Nearness is 0 at the far limit and beyond"), FMath::IsNearlyZero(Nearness(40, 40)) && FMath::IsNearlyZero(Nearness(500, 40)));
    TestTrue(TEXT("Nearness falls with distance"), Nearness(3, 40) > Nearness(10, 40) && Nearness(10, 40) > Nearness(30, 40));

    // Three markers on the same spot.
    TArray<FMarkerLayout> Markers;
    for (int32 Index = 0; Index < 3; ++Index)
    {
        FMarkerLayout Item;
        Item.Anchor = FVector2D(400.0, 300.0);
        Item.IconRadius = 20.0f;
        Item.LabelSize = FVector2D(70.0, 30.0);
        Markers.Add(Item);
    }
    LayoutMarkers(Markers);
    bool bClear = true;
    for (int32 A = 0; A < Markers.Num(); ++A)
    {
        const FBox2D LabelA(Markers[A].LabelTopLeft, Markers[A].LabelTopLeft + Markers[A].LabelSize);
        for (int32 B = 0; B < Markers.Num(); ++B)
        {
            bClear &= !Overlaps(LabelA, IconBox(Markers[B]));
            if (B != A)
            {
                bClear &= (Markers[A].IconPos - Markers[B].IconPos).Size() >= 38.0f; // radii sum 40, relaxation tolerance
                bClear &= !Overlaps(LabelA, FBox2D(Markers[B].LabelTopLeft, Markers[B].LabelTopLeft + Markers[B].LabelSize));
            }
        }
    }
    TestTrue(TEXT("Stacked markers end up with no icon or label overlaps"), bClear);
    bool bBelow = true;
    for (const FMarkerLayout& Item : Markers)
    {
        bBelow &= Item.LabelTopLeft.Y > Item.IconPos.Y + Item.IconRadius
            && FMath::IsNearlyEqual(Item.LabelTopLeft.X + 0.5 * Item.LabelSize.X, Item.IconPos.X, 1e-3);
    }
    TestTrue(TEXT("Every label stays centred under its own icon"), bBelow);
    return true;
}

#endif
