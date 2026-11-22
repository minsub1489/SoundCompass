#include "SoundVisualization/SoundHaloWidget.h"
#include "SoundVisualization/SoundCategory.h"
#include "SoundVisualization/SoundHaloGeometry.h"
#include "SoundVisualization/SoundSignalAnalysis.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
// Same hues as the original HUD so categories stay recognisable across both overlays.
// Named sub-kinds of the catch-all Noise category (Helicopter, Jackhammer...) each get a stable hue of
// their own, so two different noises never look alike.
FLinearColor CategoryColor(FName Category)
{
    const FName Kind = SoundCompassCategory(Category);
    if (Kind == TEXT("Noise") && Category != Kind)
    {
        static const FLinearColor NoiseHues[] = {FLinearColor(0.25f, 0.83f, 0.90f), FLinearColor(0.70f, 0.95f, 0.25f),
            FLinearColor(0.60f, 0.55f, 1.0f), FLinearColor(1.0f, 0.75f, 0.55f), FLinearColor(0.40f, 1.0f, 0.85f)};
        return NoiseHues[FCrc::StrCrc32(*Category.ToString().ToLower()) % UE_ARRAY_COUNT(NoiseHues)];
    }
    if (Kind == TEXT("Gunshot")) { return FLinearColor(1.0f, 0.25f, 0.20f); }
    if (Kind == TEXT("Explosion")) { return FLinearColor(1.0f, 0.64f, 0.12f); }
    if (Kind == TEXT("Footstep")) { return FLinearColor(0.25f, 0.90f, 0.54f); }
    if (Kind == TEXT("Door")) { return FLinearColor(0.35f, 0.65f, 1.0f); }
    if (Kind == TEXT("Conversation")) { return FLinearColor(0.80f, 0.50f, 1.0f); }
    if (Kind == TEXT("Alert")) { return FLinearColor(1.0f, 0.92f, 0.20f); }
    if (Kind == TEXT("Ambience")) { return FLinearColor(1.0f, 0.45f, 0.78f); }
    if (Kind == TEXT("GlassBreak")) { return FLinearColor(0.90f, 0.95f, 1.0f); }
    if (Kind == TEXT("Throw")) { return FLinearColor(0.72f, 0.52f, 0.32f); }
    return FLinearColor(0.25f, 0.83f, 0.90f);
}

/** Team icon (Ariel_<name>) for an authored category; named noise sub-kinds pick the closest one. */
FName IconFor(FName Category)
{
    const FString Name = Category.ToString().ToLower();
    if (Name.Contains(TEXT("heli")) || Name.Contains(TEXT("plane")) || Name.Contains(TEXT("aircraft"))) { return TEXT("Airplane"); }
    if (Name.Contains(TEXT("jackhammer")) || Name.Contains(TEXT("drill")) || Name.Contains(TEXT("engine"))
        || Name.Contains(TEXT("traffic")) || Name.Contains(TEXT("car"))) { return TEXT("Wheel"); }
    if (Name.Contains(TEXT("upstairs")) || Name.Contains(TEXT("stomp"))) { return TEXT("Footprint"); }
    const FName Kind = SoundCompassCategory(Category);
    if (Kind == TEXT("Gunshot")) { return TEXT("Bullet"); }
    if (Kind == TEXT("Explosion")) { return TEXT("Grenade"); }
    if (Kind == TEXT("Footstep")) { return TEXT("Footprint"); }
    if (Kind == TEXT("Door")) { return TEXT("Door"); }
    if (Kind == TEXT("GlassBreak")) { return TEXT("GlassBreak"); }
    if (Kind == TEXT("Throw")) { return TEXT("Throw"); }
    if (Kind == TEXT("Ambience")) { return TEXT("Airplane"); }
    if (Kind == TEXT("Alert")) { return TEXT("Alert"); } // drawn as "!"
    return TEXT("Wave"); // conversation, generic noise
}

/** "GlassBreak" -> "Glass Break": authored category names are PascalCase identifiers. */
FString DisplayName(FName Category)
{
    const FString Raw = Category.ToString();
    FString Out;
    for (int32 Index = 0; Index < Raw.Len(); ++Index)
    {
        if (Index > 0 && FChar::IsUpper(Raw[Index]) && FChar::IsLower(Raw[Index - 1])) { Out.AppendChar(TEXT(' ')); }
        Out.AppendChar(Raw[Index]);
    }
    return Out;
}

FLinearColor Desaturate(const FLinearColor& Color, float Saturation, float Value = 1.0f)
{
    FLinearColor Hsv = Color.LinearRGBToHSV();
    Hsv.G *= Saturation;
    Hsv.B *= Value;
    return Hsv.HSVToLinearRGB().CopyWithNewOpacity(Color.A);
}

FLinearColor SourceColor(const FSoundCompassSourceFrame& Frame)
{
    // Behind a wall: keep the hue so the category stays readable, but wash it out.
    const FLinearColor Color = CategoryColor(Frame.Category);
    return Frame.bOccluded ? Desaturate(Color, 0.30f, 0.60f) : Color;
}

/** 0.2..1 from the estimated level at the listener. */
float LevelOf(const FSoundCompassSourceFrame& Frame)
{
    return FMath::Clamp((Frame.EstimatedListenerDb + 60.0f) / 60.0f, 0.2f, 1.0f);
}

/** 1 for a sound on the listener's horizontal plane, 0 straight above or below. */
float HorizontalShare(const FSoundCompassSourceFrame& Frame)
{
    return Frame.DistanceMeters > KINDA_SMALL_NUMBER
        ? FMath::Sqrt(FMath::Max(0.0f, 1.0f - FMath::Square(Frame.ElevationMeters / Frame.DistanceMeters))) : 1.0f;
}

/** Envelope sample relative to the source's retained peak; Age 0 = newest, 1 = oldest. */
float PulseOf(const FSoundCompassSourceFrame& Frame, float Age)
{
    const TArray<float>& History = Frame.EnvelopeHistory;
    const float Peak = History.Num() > 0 ? FMath::Max(History) : 0.0f;
    return Peak > 0.0f ? History[FMath::RoundToInt((1.0f - Age) * (History.Num() - 1))] / Peak : 1.0f;
}

struct FBandPoint
{
    FVector2D Pos;
    FVector2D Normal;
    float Half;
    FLinearColor Color;
};

struct FBandQuad
{
    FBandPoint A, B;
    float Depth;
    bool bGlow;
};

/** Screen-space triangles for one MakeCustomVerts call (flat shapes: discs, arrows, pills, glyphs). */
struct FShapeBuffer
{
    TArray<FSlateVertex> Verts;
    TArray<SlateIndex> Indexes;
    FSlateRenderTransform Transform;

    SlateIndex Add(const FVector2D& Pos, const FLinearColor& Color)
    {
        Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, FVector2f(Pos), FVector2f(0.5f, 0.5f), Color.ToFColor(true)));
        return static_cast<SlateIndex>(Verts.Num() - 1);
    }
    void Triangle(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Color)
    {
        Indexes.Append({Add(A, Color), Add(B, Color), Add(C, Color)});
    }
    void Rect(const FVector2D& Min, const FVector2D& Max, const FLinearColor& Color)
    {
        const SlateIndex A = Add(Min, Color), B = Add(FVector2D(Max.X, Min.Y), Color), C = Add(Max, Color), D = Add(FVector2D(Min.X, Max.Y), Color);
        Indexes.Append({A, B, C, A, C, D});
    }
    /** Filled circle with a 1 px feathered rim. */
    void Disc(const FVector2D& Centre, float Radius, const FLinearColor& Color, int32 Segments = 32)
    {
        const SlateIndex Middle = Add(Centre, Color);
        const SlateIndex First = static_cast<SlateIndex>(Verts.Num());
        for (int32 Index = 0; Index < Segments; ++Index)
        {
            const float Theta = 2.0f * PI * Index / Segments;
            const FVector2D Dir(FMath::Cos(Theta), FMath::Sin(Theta));
            Add(Centre + Dir * Radius, Color);
            Add(Centre + Dir * (Radius + 1.0f), Color.CopyWithNewOpacity(0.0f));
        }
        for (int32 Index = 0; Index < Segments; ++Index)
        {
            const SlateIndex In0 = First + 2 * Index, Out0 = In0 + 1;
            const SlateIndex In1 = First + 2 * ((Index + 1) % Segments), Out1 = In1 + 1;
            Indexes.Append({Middle, In0, In1, In0, Out0, Out1, In0, Out1, In1});
        }
    }
    /** Arc stroke from angle A0 to A1 (radians, screen space: 0 = right, +pi/2 = down). */
    void Arc(const FVector2D& Centre, float Radius, float A0, float A1, float Thickness, const FLinearColor& Color, int32 Segments = 10)
    {
        SlateIndex PrevIn = 0, PrevOut = 0;
        for (int32 Index = 0; Index <= Segments; ++Index)
        {
            const float Theta = FMath::Lerp(A0, A1, static_cast<float>(Index) / Segments);
            const FVector2D Dir(FMath::Cos(Theta), FMath::Sin(Theta));
            const SlateIndex In = Add(Centre + Dir * (Radius - 0.5f * Thickness), Color);
            const SlateIndex Out = Add(Centre + Dir * (Radius + 0.5f * Thickness), Color);
            if (Index > 0) { Indexes.Append({PrevIn, PrevOut, Out, PrevIn, Out, In}); }
            PrevIn = In;
            PrevOut = Out;
        }
    }
    /** Open stroke through Points, mitred by averaging neighbour directions. */
    void Polyline(const TArray<FVector2D>& Points, float Thickness, const FLinearColor& Color)
    {
        SlateIndex PrevA = 0, PrevB = 0;
        for (int32 Index = 0; Index < Points.Num(); ++Index)
        {
            const FVector2D Tangent = Points[FMath::Min(Index + 1, Points.Num() - 1)] - Points[FMath::Max(Index - 1, 0)];
            const FVector2D Normal = FVector2D(-Tangent.Y, Tangent.X).GetSafeNormal() * (0.5f * Thickness);
            const SlateIndex A = Add(Points[Index] + Normal, Color), B = Add(Points[Index] - Normal, Color);
            if (Index > 0) { Indexes.Append({PrevA, PrevB, B, PrevA, B, A}); }
            PrevA = A;
            PrevB = B;
        }
    }
    void Flush(FSlateWindowElementList& Elements, int32 Layer) const
    {
        if (Verts.Num() == 0 || !FSlateApplication::IsInitialized()) { return; }
        const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(
            *FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")));
        FSlateDrawElement::MakeCustomVerts(Elements, Layer, Handle, Verts, Indexes, nullptr, 0, 0);
    }
};

/** One band quad as three strips (feather, core, feather) so edges are anti-aliased and joints seamless. */
void EmitQuad(TArray<FSlateVertex>& Verts, TArray<SlateIndex>& Indexes, const FSlateRenderTransform& Transform,
    const FBandQuad& Quad, float Extra, float Feather, const FLinearColor* Override, float AlphaScale)
{
    const SlateIndex Base = Verts.Num();
    for (const FBandPoint* Point : {&Quad.A, &Quad.B})
    {
        const float Half = Point->Half + Extra;
        const FLinearColor Color = (Override ? *Override : Point->Color).CopyWithNewOpacity(Point->Color.A * AlphaScale);
        const float Offsets[4] = {-(Half + Feather), -Half, Half, Half + Feather};
        const float Alphas[4] = {0.0f, 1.0f, 1.0f, 0.0f};
        for (int32 Row = 0; Row < 4; ++Row)
        {
            const FVector2D Pos = Point->Pos + Point->Normal * Offsets[Row];
            Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, FVector2f(Pos), FVector2f(0.5f, 0.5f),
                Color.CopyWithNewOpacity(Color.A * Alphas[Row]).ToFColor(true)));
        }
    }
    for (SlateIndex Row = 0; Row < 3; ++Row)
    {
        const SlateIndex A0 = Base + Row, A1 = A0 + 1, B0 = Base + 4 + Row, B1 = B0 + 1;
        Indexes.Append({A0, A1, B1, A0, B1, B0});
    }
}
}

USoundHaloWidget::USoundHaloWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // The team's icons (Ariel_<name>), keyed by <name> as IconFor returns it. Ariel_Wave is a material,
    // not a texture, so "Wave" is drawn procedurally instead.
    for (const TCHAR* Name : {TEXT("Airplane"), TEXT("Bullet"), TEXT("Door"), TEXT("Footprint"), TEXT("GlassBreak"),
        TEXT("Grenade"), TEXT("Throw"), TEXT("Wheel")})
    {
        const FString Asset = FString::Printf(TEXT("/Game/Ariel_NewMaterial/Ariel_%s.Ariel_%s"), Name, Name);
        ConstructorHelpers::FObjectFinder<UTexture2D> Texture(*Asset);
        if (!Texture.Succeeded()) { continue; }
        FSlateBrush Brush;
        Brush.SetResourceObject(Texture.Object);
        Brush.ImageSize = FVector2D(64.0f, 64.0f);
        Icons.Add(FName(Name), Brush);
    }
}

void USoundHaloWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

void USoundHaloWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    Frames.Reset();
    OnsetAges.Reset();
    UWorld* World = GetWorld();
    APlayerController* Player = GetOwningPlayer();
    USoundVisualizationSubsystem* System = World ? World->GetSubsystem<USoundVisualizationSubsystem>() : nullptr;
    if (!System) { return; }

    // The subsystem reports every playback (each footstep, each thud) and retains it while it fades, so
    // one repeating emitter shows up several times. Merge same-category frames within MergeRadiusMeters
    // into one marker: the freshest playback represents it, in the slot of the most important one.
    auto Near = [this](const FName Category, const FVector& Location, const FName OtherCategory, const FVector& OtherLocation)
    {
        return Category == OtherCategory && FVector::DistSquared(Location, OtherLocation) < FMath::Square(MergeRadiusMeters * 100.0f);
    };
    for (const FSoundCompassSourceFrame& Frame : System->GetVisualizationFrames(Player))
    {
        FSoundCompassSourceFrame* Same = Frames.FindByPredicate([&](const FSoundCompassSourceFrame& Other)
        {
            return Near(Frame.Category, Frame.WorldLocation, Other.Category, Other.WorldLocation);
        });
        if (!Same) { Frames.Add(Frame); }
        else if (Frame.VisualAlpha > Same->VisualAlpha) { *Same = Frame; }
    }

    // Track merged markers over time so the onset wave plays once when a marker appears.
    const double Now = World->GetTimeSeconds();
    for (const FSoundCompassSourceFrame& Frame : Frames)
    {
        FMarkerTrack* Track = Tracks.FindByPredicate([&](const FMarkerTrack& Other)
        {
            return Near(Frame.Category, Frame.WorldLocation, Other.Category, Other.Location);
        });
        if (!Track) { Track = &Tracks.Add_GetRef({Frame.Category, Frame.WorldLocation, Now, Now}); }
        Track->Location = Frame.WorldLocation;
        Track->LastSeen = Now;
        OnsetAges.Add(static_cast<float>(Now - Track->OnsetTime));
    }
    Tracks.RemoveAll([Now](const FMarkerTrack& Track) { return Now - Track.LastSeen > 0.25; });

    const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
    const APlayerCameraManager* Camera = Player ? Player->PlayerCameraManager.Get() : nullptr;
    if (!Pawn || !Camera)
    {
        CurrentPitch = ViewPitchDegrees;
        return;
    }
    // The audio listener sits at the camera, behind the character, but the ring is drawn around the
    // character: re-measure every bearing from the character, facing where the camera looks.
    const FRotator CameraRotation = Camera->GetCameraRotation().GetNormalized();
    const FVector Front = FRotator(0.0f, CameraRotation.Yaw, 0.0f).Vector();
    const FVector Right = FRotator(0.0f, CameraRotation.Yaw + 90.0f, 0.0f).Vector();
    const FVector Origin = Pawn->GetActorLocation();
    for (FSoundCompassSourceFrame& Frame : Frames)
    {
        const FVector Offset = Frame.WorldLocation - Origin;
        const auto Bearing = SoundCompassSignal::CalculateBearing(
            FVector::DotProduct(Offset, Front), FVector::DotProduct(Offset, Right), Offset.Z);
        Frame.AzimuthDegrees = Bearing.Degrees;
        Frame.DistanceMeters = Bearing.DistanceMeters;
        Frame.ElevationMeters = Bearing.ElevationMeters;
    }
    const float Target = bFollowCameraPitch
        ? FMath::Clamp(-CameraRotation.Pitch + CameraPitchOffset, MinViewPitchDegrees, MaxViewPitchDegrees) : ViewPitchDegrees;
    CurrentPitch = FMath::FInterpTo(CurrentPitch, Target, InDeltaTime, 6.0f);
}

int32 USoundHaloWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
    const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 LayerId,
    const FWidgetStyle& Style, bool bEnabled) const
{
    using namespace SoundHaloGeometry;
    const int32 Layer = Super::NativePaint(Args, Geometry, CullingRect, Elements, LayerId, Style, bEnabled) + 1;
    const FVector2D Size = Geometry.GetLocalSize();
    const FVector2D Center = Size * 0.5f;
    const float PixelScale = FMath::Min(1.0f, static_cast<float>(Size.Y / 900.0));
    const float Radius = RingRadius * PixelScale;
    const FView View{CurrentPitch, CameraDistance};
    const FSlateRenderTransform Transform = Geometry.GetAccumulatedRenderTransform();
    auto ToScreen = [&](const FProjected& P) { return Center + P.Screen * Radius; };

    // ---- Centre ring: a quiet band that lights up, swells and beats at each sound's bearing.
    // Overhead / underfoot sounds barely light a bearing; their tilted pointer carries them instead.
    int32 Nearest = INDEX_NONE;
    for (int32 Index = 0; Index < Frames.Num(); ++Index)
    {
        if (Nearest == INDEX_NONE || Frames[Index].DistanceMeters < Frames[Nearest].DistanceMeters) { Nearest = Index; }
    }
    // Closed loop on the ground plane -> projected band quads with the depth cue baked in: the far side is
    // thinner (perspective), paler and fainter, so it reads as behind the character.
    TArray<FBandQuad> Quads;
    auto AddLoop = [&](float LoopRadius, const TArray<FLinearColor>& Colors, const TArray<float>& Widths, bool bGlow, float FarAlpha)
    {
        const int32 Count = Colors.Num();
        TArray<FBandPoint> Points;
        TArray<float> Depths;
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const FProjected P = Project(RingPoint(360.0f * Index / Count) * LoopRadius, View);
            const float Far = FarAmount(P.Depth, View);
            FLinearColor Color = Desaturate(Colors[Index], FMath::Lerp(1.0f, 0.45f, Far));
            Color.A *= FMath::Lerp(1.0f, FarAlpha, Far);
            Points.Add({ToScreen(P), FVector2D::ZeroVector, 0.5f * Widths[Index] * FMath::Min(P.Scale, 1.5f) * PixelScale, Color});
            Depths.Add(P.Depth);
        }
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const FVector2D Tangent = Points[(Index + 1) % Count].Pos - Points[(Index + Count - 1) % Count].Pos;
            Points[Index].Normal = FVector2D(-Tangent.Y, Tangent.X).GetSafeNormal();
        }
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const int32 Next = (Index + 1) % Count;
            Quads.Add({Points[Index], Points[Next], 0.5f * (Depths[Index] + Depths[Next]), bGlow});
        }
    };

    // Range rings on the ground: a pointer standing between two rings reads as "between those distances".
    // The outermost (dashed) ring is the limit; anything farther stands on it.
    TArray<TPair<FVector2D, FString>> RangeLabels;
    auto AddRangeRing = [&](float RangeRadius, bool bDashed, const FString& Label)
    {
        constexpr int32 Segments = 120;
        TArray<FLinearColor> Colors;
        for (int32 Index = 0; Index < Segments; ++Index)
        {
            Colors.Add(FLinearColor(0.85f, 0.95f, 1.0f, bDashed && (Index / 2) % 2 == 1 ? 0.0f : 0.28f));
        }
        TArray<float> Widths;
        Widths.Init(2.0f, Segments);
        AddLoop(RangeRadius, Colors, Widths, false, 0.5f);
        // Labels (F1 only) on the near side, back-right, where perspective spreads the rings apart.
        RangeLabels.Add({ToScreen(Project(RingPoint(145.0f) * RangeRadius, View)), Label});
    };
    for (const float Meters : {5.0f, 15.0f, 30.0f})
    {
        const float RangeRadius = DistanceToRadius(Meters, MaxRangeRadius);
        if (RangeRadius < MaxRangeRadius - 0.15f) { AddRangeRing(RangeRadius, false, FString::Printf(TEXT("%dm"), FMath::RoundToInt(Meters))); }
    }
    AddRangeRing(MaxRangeRadius, true, FString::Printf(TEXT("%dm+"), FMath::FloorToInt(RadiusToDistance(MaxRangeRadius) / 5.0f) * 5));

    constexpr int32 RingSegments = 160;
    TArray<FLinearColor> RingColors;
    TArray<float> RingWidths;
    for (int32 Index = 0; Index < RingSegments; ++Index)
    {
        const float Bearing = 360.0f * Index / RingSegments;
        FLinearColor Color(0.85f, 0.95f, 1.0f, IdleRingOpacity);
        float Bulge = 0.0f;
        for (int32 Source = 0; Source < Frames.Num(); ++Source)
        {
            const FSoundCompassSourceFrame& Frame = Frames[Source];
            const float Delta = FMath::Abs(FMath::FindDeltaAngleDegrees(Bearing, Frame.AzimuthDegrees));
            const float Weight = FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(1.0f - Delta / 40.0f, 0.0f, 1.0f)) * Frame.VisualAlpha
                * (Source == Nearest ? 1.0f : 0.75f) * FMath::Square(HorizontalShare(Frame));
            if (Weight <= 0.0f) { continue; }
            // Dangerous sounds (gunshots, explosions: high authored importance) light the ring harder.
            const float Threat = FMath::Clamp(Frame.BaseImportance, 0.0f, 1.0f);
            Color = FMath::Lerp(Color, SourceColor(Frame).CopyWithNewOpacity(1.0f), FMath::Min(1.0f, Weight * FMath::Lerp(1.0f, 1.8f, Threat)));
            Bulge = FMath::Max(Bulge, Weight * LevelOf(Frame) * (0.4f + 0.6f * PulseOf(Frame, 0.0f)) * FMath::Lerp(0.6f, 1.3f, Threat));
            // Behind a wall: hatched.
            if (Frame.bOccluded && Weight > 0.15f && (Index / 3) % 2 == 1) { Color.A *= 0.30f; }
        }
        RingColors.Add(Color);
        RingWidths.Add(RingThickness * (1.0f + LevelBulge * Bulge));
    }
    AddLoop(1.0f, RingColors, RingWidths, true, 0.4f);
    // Ring segments and pointer faces share one depth-sorted list, so the near half of the ring covers
    // pointers behind the character and pointers in front cover the ring.
    struct FDrawItem { float Depth; int32 Quad; FVector2D Tri[3]; FLinearColor Color; };
    TArray<FDrawItem> Items;
    for (int32 Index = 0; Index < Quads.Num(); ++Index) { Items.Add({Quads[Index].Depth, Index, {}, {}}); }

    // Pointers: an elongated tetrahedron for each marked sound, standing on the ground at its distance
    // (between the range rings) and aimed along its 3D direction. Tilt shows above / below, perspective
    // shows front / back (aiming away = short and thin), flat shading from a fixed light makes it a solid.
    const FVector CameraAt = CameraPosition(View);
    const FVector Light = FVector(-0.3f, 0.35f, 1.0f).GetSafeNormal();
    for (int32 Source = 0; Source < FMath::Min(Frames.Num(), MaxEdgeMarkers); ++Source)
    {
        const FSoundCompassSourceFrame& Frame = Frames[Source];
        const float Horizontal = FMath::Sqrt(FMath::Max(0.0f, FMath::Square(Frame.DistanceMeters) - FMath::Square(Frame.ElevationMeters)));
        const FVector Aim = PointerDirection(Frame.AzimuthDegrees, Horizontal, Frame.ElevationMeters);
        const float Threat = FMath::Clamp(Frame.BaseImportance, 0.0f, 1.0f);
        const float Length = PointerLength * FMath::Lerp(0.85f, 1.2f, Threat);
        const float Width = 0.2f * PointerLength;
        const FVector Base = RingPoint(Frame.AzimuthDegrees) * FMath::Max(1.04f, DistanceToRadius(Horizontal, MaxRangeRadius));
        const FVector Side = (FMath::Abs(Aim.Z) > 0.95f ? FVector::CrossProduct(Aim, FVector::ForwardVector)
            : FVector::CrossProduct(Aim, FVector::UpVector)).GetSafeNormal();
        const FVector Lift = FVector::CrossProduct(Side, Aim); // roughly "up" across the base
        const FVector Corners[4] = {Base + Aim * Length,                                   // tip
            Base + Lift * Width,                                                            // top ridge
            Base + (-Lift * 0.5f + Side * 0.866f) * Width, Base + (-Lift * 0.5f - Side * 0.866f) * Width};
        const FVector Centroid = (Corners[0] + Corners[1] + Corners[2] + Corners[3]) * 0.25f;
        const int32 Faces[4][3] = {{0, 1, 2}, {0, 2, 3}, {0, 3, 1}, {1, 3, 2}};
        const FLinearColor Color = SourceColor(Frame);
        for (const auto& Face : Faces)
        {
            const FVector A = Corners[Face[0]], B = Corners[Face[1]], C = Corners[Face[2]];
            const FVector FaceCentre = (A + B + C) / 3.0f;
            FVector Normal = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
            if (FVector::DotProduct(Normal, FaceCentre - Centroid) < 0.0f) { Normal = -Normal; }
            // Convex solid: drawing only the faces turned to the camera needs no sorting inside it.
            if (FVector::DotProduct(Normal, CameraAt - FaceCentre) <= 0.0f) { continue; }
            const FProjected PA = Project(A, View), PB = Project(B, View), PC = Project(C, View);
            const float Depth = (PA.Depth + PB.Depth + PC.Depth) / 3.0f;
            const float Shade = 0.35f + 0.65f * FMath::Max(0.0f, static_cast<float>(FVector::DotProduct(Normal, Light)));
            FLinearColor Lit = FMath::Lerp(FLinearColor::Black, Color, Shade);
            // Same depth cue as the ring: behind the character = fainter.
            Lit.A = Frame.VisualAlpha * FMath::Lerp(1.0f, 0.45f, FarAmount(Depth, View));
            Items.Add({Depth, INDEX_NONE, {ToScreen(PA), ToScreen(PB), ToScreen(PC)}, Lit});
        }
    }

    // Painter's order: far first.
    Items.Sort([](const FDrawItem& L, const FDrawItem& R) { return L.Depth > R.Depth; });
    FShapeBuffer Band{{}, {}, Transform};
    const FLinearColor Black(0.0f, 0.0f, 0.0f, 1.0f);
    for (const FDrawItem& Item : Items)
    {
        if (Item.Quad != INDEX_NONE)
        {
            const FBandQuad& Quad = Quads[Item.Quad];
            EmitQuad(Band.Verts, Band.Indexes, Transform, Quad, 3.0f, 5.0f, &Black, 0.22f); // soft shadow on bright scenes
            EmitQuad(Band.Verts, Band.Indexes, Transform, Quad, 1.0f, 1.0f, &Black, 0.5f);  // crisp outline
            EmitQuad(Band.Verts, Band.Indexes, Transform, Quad, 0.0f, 1.0f, nullptr, 1.0f);
            continue;
        }
        // Face with a dark edge: the same triangle grown 1 px outward first.
        const FVector2D Mid = (Item.Tri[0] + Item.Tri[1] + Item.Tri[2]) / 3.0f;
        auto Grow = [&](const FVector2D& V) { return V + (V - Mid).GetSafeNormal() * 1.2f; };
        Band.Triangle(Grow(Item.Tri[0]), Grow(Item.Tri[1]), Grow(Item.Tri[2]), FLinearColor(0.0f, 0.0f, 0.0f, 0.6f * Item.Color.A));
        Band.Triangle(Item.Tri[0], Item.Tri[1], Item.Tri[2], Item.Color);
    }
    Band.Flush(Elements, Layer);

    // ---- Markers for the most important sounds (frames arrive ordered by importance).
    // Distance is position: a near sound sits just outside the ring, a far one out at the edge ellipse.
    const FVector2D HalfExtent = Size * 0.5f * EdgeEllipseScale;
    auto MarkerAnchor = [&](float Bearing, float Closeness)
    {
        const FVector2D Inner = Center + (ToScreen(Project(RingPoint(Bearing), View)) - Center) * NearMarkerRadius;
        return FMath::Lerp(Center + EdgePoint(Bearing, HalfExtent), Inner, Closeness);
    };
    const FSlateFontInfo SmallFont = FCoreStyle::GetDefaultFontStyle("Bold", 11);
    const FSlateFontInfo NumberFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);
    const FVector2D PillPad(4.0f, 1.0f);
    const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    struct FMarker { int32 Frame; float Pixels; FLinearColor Color; float Closeness; FString Name, Distance; };
    TArray<FMarker> Markers;
    TArray<FMarkerLayout> Layout;
    for (int32 Index = 0; Index < FMath::Min(Frames.Num(), MaxEdgeMarkers); ++Index)
    {
        const FSoundCompassSourceFrame& Frame = Frames[Index];
        // Size is mostly danger (authored importance), a little nearness; full colour at any distance so
        // a far gunshot never looks like a fading one. Fading markers shrink a little on the way out.
        const float Closeness = Nearness(Frame.DistanceMeters, FarDistanceMeters);
        const float Threat = FMath::Clamp(Frame.BaseImportance, 0.0f, 1.0f);
        const float Pixels = IconSize * PixelScale * FMath::Lerp(0.85f, 1.25f, Threat) * FMath::Lerp(0.9f, 1.1f, Closeness)
            * FMath::Lerp(0.75f, 1.0f, Frame.VisualAlpha);
        // Opaque while a sound is new (to catch the eye), then see-through so it never hides an opponent.
        const float Age = OnsetAges.IsValidIndex(Index) ? OnsetAges[Index] : TNumericLimits<float>::Max();
        const float Settled = FMath::SmoothStep(OnsetWaveSeconds, OnsetWaveSeconds + 0.5f, Age);
        const FLinearColor Color = SourceColor(Frame).CopyWithNewOpacity(Frame.VisualAlpha * FMath::Lerp(1.0f, IconOpacity, Settled));
        FMarker Marker{Index, Pixels, Color, Closeness, DisplayName(Frame.Category),
            FString::Printf(TEXT("%dm"), FMath::RoundToInt(Frame.DistanceMeters))};
        if (FMath::Abs(Frame.ElevationMeters) >= VerticalThresholdMeters)
        {
            // Horizontal distance, then the height with an arrow: "4m ↓5m".
            const float Horizontal = FMath::Sqrt(FMath::Max(0.0f, FMath::Square(Frame.DistanceMeters) - FMath::Square(Frame.ElevationMeters)));
            Marker.Distance = FString::Printf(TEXT("%dm %s%dm"), FMath::RoundToInt(Horizontal),
                Frame.ElevationMeters > 0.0f ? TEXT("↑") : TEXT("↓"), FMath::RoundToInt(FMath::Abs(Frame.ElevationMeters)));
        }
        FMarkerLayout Item;
        Item.Anchor = MarkerAnchor(Frame.AzimuthDegrees, Closeness);
        Item.IconRadius = 0.84f * Pixels; // outline radius
        if (bShowLabels)
        {
            const FVector2D NameSize = Measure->Measure(Marker.Name, SmallFont);
            const FVector2D DistanceSize = Measure->Measure(Marker.Distance, NumberFont);
            Item.LabelSize = FVector2D(FMath::Max(NameSize.X, DistanceSize.X), NameSize.Y + DistanceSize.Y) + PillPad * 2.0f;
        }
        Markers.Add(Marker);
        Layout.Add(Item);
    }
    LayoutMarkers(Layout);

    FShapeBuffer Under{{}, {}, Transform};
    FShapeBuffer Over{{}, {}, Transform};
    struct FText { FVector2D TopLeft; FString Text; FSlateFontInfo Font; FVector2D Size; float Alpha; };
    TArray<FText> Texts;
    for (int32 Index = 0; Index < Markers.Num(); ++Index)
    {
        const FMarker& Marker = Markers[Index];
        const FSoundCompassSourceFrame& Frame = Frames[Marker.Frame];
        const FMarkerLayout& Item = Layout[Index];
        const FVector2D Pos = Item.IconPos;
        const float P = Marker.Pixels;
        const float A = Marker.Color.A;

        // Onset wave: once, when the marker appears, a short arc runs from the icon in to the ring and shrinks.
        const float Age = OnsetAges.IsValidIndex(Marker.Frame) ? OnsetAges[Marker.Frame] : OnsetWaveSeconds;
        if (Age < OnsetWaveSeconds)
        {
            const float T = Age / OnsetWaveSeconds;
            const float Travel = 1.0f - FMath::Square(1.0f - T); // fast start, settles onto the ring
            TArray<FVector2D> Arc;
            for (int32 Step = 0; Step <= 8; ++Step)
            {
                const float Bearing = Frame.AzimuthDegrees + FMath::Lerp(-12.0f, 12.0f, Step / 8.0f);
                Arc.Add(FMath::Lerp(MarkerAnchor(Bearing, Marker.Closeness), ToScreen(Project(RingPoint(Bearing), View)), Travel));
            }
            const FLinearColor WaveColor = Marker.Color.CopyWithNewOpacity(A * (1.0f - T) * FMath::Min(1.0f, T * 8.0f));
            Under.Polyline(Arc, FMath::Lerp(7.0f, 3.0f, T) * PixelScale + 2.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.4f * WaveColor.A));
            Under.Polyline(Arc, FMath::Lerp(7.0f, 3.0f, T) * PixelScale, WaveColor);
        }

        if ((Pos - Item.Anchor).Size() > 3.0f)
        {
            // Nudged off another marker: a dot at the true bearing and a thin line to the icon.
            const FVector2D Dir = (Pos - Item.Anchor).GetSafeNormal();
            const FVector2D Side(-Dir.Y, Dir.X);
            const FLinearColor Leader = Marker.Color.CopyWithNewOpacity(0.8f * A);
            Under.Triangle(Item.Anchor + Side, Item.Anchor - Side, Pos - Side, Leader);
            Under.Triangle(Item.Anchor + Side, Pos - Side, Pos + Side, Leader);
            Under.Disc(Item.Anchor, 3.0f * PixelScale, Leader, 12);
        }
        // The team's icons are black line art, so they sit on a saturated category-colour disc (not pastel:
        // that vanished on clouds); a dark outline separates it from bright backgrounds.
        Under.Disc(Pos, 0.84f * P, FLinearColor(0.0f, 0.0f, 0.0f, 0.75f * A));
        Under.Disc(Pos, 0.78f * P, Marker.Color);
        Under.Disc(Pos, 0.68f * P, FMath::Lerp(Marker.Color, FLinearColor::White, 0.25f).CopyWithNewOpacity(A));

        // Procedural glyphs, dark like the texture icons.
        const FName Kind = IconFor(Frame.Category);
        const FLinearColor Glyph(0.05f, 0.06f, 0.09f, A);
        if (Kind == TEXT("Wave"))
        {
            // Speaker-style wave: a dot and three arcs opening to the right.
            const FVector2D Origin = Pos - FVector2D(0.3f * P, 0.0f);
            Over.Disc(Origin, 0.08f * P, Glyph, 12);
            for (const float R : {0.25f, 0.45f, 0.65f})
            {
                Over.Arc(Origin, R * P, -0.8f, 0.8f, 0.09f * P, Glyph);
            }
        }
        else if (Kind == TEXT("Alert"))
        {
            // Exclamation mark.
            Over.Rect(Pos + FVector2D(-0.07f, -0.38f) * P, Pos + FVector2D(0.07f, 0.12f) * P, Glyph);
            Over.Disc(Pos + FVector2D(0.0f, 0.28f) * P, 0.08f * P, Glyph, 12);
        }

        if (bShowLabels)
        {
            // Label block: one pill, name above distance. Labels go first when a marker fades.
            const float TextAlpha = FMath::SmoothStep(0.45f, 0.85f, Frame.VisualAlpha);
            const FVector2D TopLeft = Item.LabelTopLeft;
            Over.Rect(TopLeft, TopLeft + Item.LabelSize, FLinearColor(0.02f, 0.03f, 0.05f, 0.7f * TextAlpha));
            const FVector2D NameSize = Measure->Measure(Marker.Name, SmallFont);
            const FVector2D DistanceSize = Measure->Measure(Marker.Distance, NumberFont);
            const float MiddleX = TopLeft.X + 0.5f * Item.LabelSize.X;
            Texts.Add({FVector2D(MiddleX - 0.5f * NameSize.X, TopLeft.Y + PillPad.Y), Marker.Name, SmallFont, NameSize, 0.85f * TextAlpha});
            Texts.Add({FVector2D(MiddleX - 0.5f * DistanceSize.X, TopLeft.Y + PillPad.Y + NameSize.Y), Marker.Distance, NumberFont,
                DistanceSize, TextAlpha});
        }
    }
    if (bShowLabels)
    {
        for (const TPair<FVector2D, FString>& Range : RangeLabels)
        {
            const FVector2D TextSize = Measure->Measure(Range.Value, SmallFont);
            const FVector2D TopLeft = Range.Key + FVector2D(-0.5f * TextSize.X, 3.0f);
            Over.Rect(TopLeft - PillPad, TopLeft + TextSize + PillPad, FLinearColor(0.02f, 0.03f, 0.05f, 0.45f));
            Texts.Add({TopLeft, Range.Value, SmallFont, TextSize, 0.9f});
        }
    }
    Under.Flush(Elements, Layer + 1);

    for (int32 Index = 0; Index < Markers.Num(); ++Index)
    {
        if (const FSlateBrush* Brush = Icons.Find(IconFor(Frames[Markers[Index].Frame].Category)))
        {
            // White tint keeps the texture's own black line art.
            const FVector2D IconBoxSize(Markers[Index].Pixels, Markers[Index].Pixels);
            FSlateDrawElement::MakeBox(Elements, Layer + 2, Geometry.ToPaintGeometry(IconBoxSize,
                FSlateLayoutTransform(Layout[Index].IconPos - IconBoxSize * 0.5f)), Brush, ESlateDrawEffect::None,
                FLinearColor(1.0f, 1.0f, 1.0f, Markers[Index].Color.A));
        }
    }
    Over.Flush(Elements, Layer + 3);

    for (const FText& Text : Texts)
    {
        FSlateDrawElement::MakeText(Elements, Layer + 4, Geometry.ToPaintGeometry(Text.Size, FSlateLayoutTransform(Text.TopLeft)),
            Text.Text, Text.Font, ESlateDrawEffect::None, FLinearColor(0.95f, 0.97f, 1.0f, Text.Alpha));
    }
    return Layer + 4;
}
