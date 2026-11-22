#include "SoundVisualization/SoundHaloWidget.h"
#include "SoundVisualization/SoundCategory.h"
#include "SoundVisualization/SoundHaloGeometry.h"
#include "SoundVisualization/SoundSignalAnalysis.h"
#include "Brushes/SlateRoundedBoxBrush.h"
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
    WaveTime += InDeltaTime;
    Frames.Reset();
    APlayerController* Player = GetOwningPlayer();
    if (USoundVisualizationSubsystem* System = GetWorld() ? GetWorld()->GetSubsystem<USoundVisualizationSubsystem>() : nullptr)
    {
        // The subsystem reports every playback (each footstep, each thud) and retains it while it fades,
        // so one repeating emitter shows up several times. Merge same-category frames within
        // MergeRadiusMeters into one marker: the freshest playback represents it, in the slot of the
        // most important one.
        TArray<FSoundCompassSourceFrame> Merged;
        for (const FSoundCompassSourceFrame& Frame : System->GetVisualizationFrames(Player))
        {
            FSoundCompassSourceFrame* Same = Merged.FindByPredicate([&](const FSoundCompassSourceFrame& Other)
            {
                return Other.Category == Frame.Category
                    && FVector::DistSquared(Other.WorldLocation, Frame.WorldLocation) < FMath::Square(MergeRadiusMeters * 100.0f);
            });
            if (!Same) { Merged.Add(Frame); }
            else if (Frame.VisualAlpha > Same->VisualAlpha) { *Same = Frame; }
        }
        Frames = MoveTemp(Merged);
    }
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
    auto ToScreen = [&](const FProjected& P) { return Center + P.Screen * Radius; };
    // 1 outside the character's rough on-screen silhouette (a tall ellipse at the centre), 0.3 inside it.
    auto CharacterClearance = [&](const FVector2D& Pos)
    {
        const FVector2D Offset = Pos - Center;
        const float Inside = FMath::Square(Offset.X / (0.42f * Radius)) + FMath::Square((Offset.Y - 0.1f * Radius) / (1.5f * Radius));
        return FMath::Lerp(0.3f, 1.0f, FMath::SmoothStep(0.6f, 1.1f, Inside));
    };

    TArray<FBandQuad> Quads;
    // Path in listener space -> projected band quads with depth cues baked into the colours.
    // FarAlpha: how much of the opacity survives on the far side. bYield: fade over the character so
    // sound cues (mostly those behind, which land at the bottom centre) don't hide it.
    auto AddPath = [&](const TArray<FVector>& Path, const TArray<FLinearColor>& Colors, const TArray<float>& Thickness,
        bool bClosed, bool bGlow, float FarAlpha, bool bYield = false)
    {
        const int32 Count = Path.Num();
        TArray<FBandPoint> Points;
        TArray<float> Depths;
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const FProjected P = Project(Path[Index], View);
            const float Far = FarAmount(P.Depth, View);
            // Far side: thinner (perspective), paler and fainter, so it reads as behind the character.
            FLinearColor Color = Desaturate(Colors[Index], FMath::Lerp(1.0f, 0.45f, Far));
            Color.A *= FMath::Lerp(1.0f, FarAlpha, Far);
            const FVector2D Pos = ToScreen(P);
            if (bYield) { Color.A *= CharacterClearance(Pos); }
            // Cap the perspective boost: points near the virtual camera would otherwise balloon.
            Points.Add({Pos, FVector2D::ZeroVector, 0.5f * Thickness[Index] * FMath::Min(P.Scale, 1.5f) * PixelScale, Color});
            Depths.Add(P.Depth);
        }
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const int32 Prev = bClosed ? (Index + Count - 1) % Count : FMath::Max(0, Index - 1);
            const int32 Next = bClosed ? (Index + 1) % Count : FMath::Min(Count - 1, Index + 1);
            const FVector2D Tangent = Points[Next].Pos - Points[Prev].Pos;
            Points[Index].Normal = FVector2D(-Tangent.Y, Tangent.X).GetSafeNormal();
        }
        for (int32 Index = 0; Index < (bClosed ? Count : Count - 1); ++Index)
        {
            const int32 Next = (Index + 1) % Count;
            Quads.Add({Points[Index], Points[Next], 0.5f * (Depths[Index] + Depths[Next]), bGlow});
        }
    };
    // bDashed: every other pair of segments is transparent.
    auto AddLoop = [&](const FVector& Centre, float LoopRadius, int32 Segments, const FLinearColor& Color, float Thickness,
        float FarAlpha, bool bDashed = false)
    {
        TArray<FVector> Path;
        TArray<FLinearColor> Colors;
        for (int32 Index = 0; Index < Segments; ++Index)
        {
            const float Theta = 2.0f * PI * Index / Segments;
            Path.Add(Centre + FVector(FMath::Cos(Theta), FMath::Sin(Theta), 0.0f) * LoopRadius);
            Colors.Add(bDashed && (Index / 2) % 2 == 1 ? Color.CopyWithNewOpacity(0.0f) : Color);
        }
        TArray<float> Widths;
        Widths.Init(Thickness, Segments);
        AddPath(Path, Colors, Widths, true, false, FarAlpha);
    };

    // Text drawn last, each line on a dark pill unless it sits inside a block that has its own.
    // Range labels are kept apart: they draw under the markers and hide where a marker covers them.
    struct FLabel { FVector2D TopLeft; FString Text; FSlateFontInfo Font; FVector2D Size; float Alpha; float PillAlpha; };
    TArray<FLabel> Labels, RangeLabels;
    TArray<TPair<FBox2D, float>> Pills;
    const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    const FSlateFontInfo SmallFont = FCoreStyle::GetDefaultFontStyle("Bold", 11);
    const FSlateFontInfo NumberFont = FCoreStyle::GetDefaultFontStyle("Bold", 13);
    const FVector2D PillPad(4.0f, 1.0f);

    // Range rings on the ground: read a sound's distance by where its drop line lands. Labels sit on the
    // near side (back-right), where perspective spreads the rings apart, so they don't pile up.
    const float LabelBearing = FMath::DegreesToRadians(145.0f);
    auto AddRangeLabel = [&](float RangeRadius, const FString& Text)
    {
        const FVector LabelPoint(FMath::Cos(LabelBearing) * RangeRadius, FMath::Sin(LabelBearing) * RangeRadius, 0.0f);
        const FVector2D Size = Measure->Measure(Text, SmallFont);
        const FVector2D TopLeft = ToScreen(Project(LabelPoint, View)) + FVector2D(-0.5f * Size.X, 3.0f);
        RangeLabels.Add({TopLeft, Text, SmallFont, Size, 0.9f, 0.45f});
    };
    // The edge sits ~40 m out; a ring closer than 0.2 radii to it would only crowd its label.
    const float EdgeMeters = FMath::FloorToFloat(RadiusToDistance(MaxRangeRadius) / 5.0f) * 5.0f;
    for (const float Meters : {5.0f, 15.0f, 30.0f})
    {
        const float RangeRadius = DistanceToRadius(Meters, MaxRangeRadius);
        if (RangeRadius > MaxRangeRadius - 0.2f) { continue; }
        AddLoop(FVector::ZeroVector, RangeRadius, 96, FLinearColor(0.85f, 0.95f, 1.0f, 0.30f), 2.0f, 0.5f);
        AddRangeLabel(RangeRadius, FString::Printf(TEXT("%dm"), FMath::RoundToInt(Meters)));
    }
    // Dashed edge: sounds beyond it are pinned here, marked with an outward arrow and their real distance.
    AddLoop(FVector::ZeroVector, MaxRangeRadius, 120, FLinearColor(0.85f, 0.95f, 1.0f, 0.40f), 2.0f, 0.5f, true);
    AddRangeLabel(MaxRangeRadius, FString::Printf(TEXT("%dm+"), FMath::RoundToInt(EdgeMeters)));

    // Main ring = directional level meter: at each sound's bearing the band takes its colour,
    // swells with its level and beats with its current envelope. Occluded sounds show as a hatched section.
    int32 Nearest = INDEX_NONE;
    for (int32 Index = 0; Index < Frames.Num(); ++Index)
    {
        if (Nearest == INDEX_NONE || Frames[Index].DistanceMeters < Frames[Nearest].DistanceMeters) { Nearest = Index; }
    }
    {
        constexpr int32 RingSegments = 160;
        TArray<FVector> Path;
        TArray<FLinearColor> Colors;
        TArray<float> Widths;
        for (int32 Index = 0; Index < RingSegments; ++Index)
        {
            const float Theta = 2.0f * PI * Index / RingSegments;
            // Plain band stays quiet so the lit bearings stand out.
            FLinearColor Color(0.85f, 0.95f, 1.0f, 0.60f);
            float Bulge = 0.0f;
            for (int32 Source = 0; Source < Frames.Num(); ++Source)
            {
                const FSoundCompassSourceFrame& Frame = Frames[Source];
                const float Delta = FMath::Abs(FMath::FindDeltaAngleDegrees(FMath::RadiansToDegrees(Theta), Frame.AzimuthDegrees));
                // Overhead / underfoot sounds barely light a bearing; the centre arrows carry them instead.
                const float Weight = FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(1.0f - Delta / 40.0f, 0.0f, 1.0f)) * Frame.VisualAlpha
                    * (Source == Nearest ? 1.0f : 0.75f) * FMath::Square(HorizontalShare(Frame));
                if (Weight <= 0.0f) { continue; }
                Color = FMath::Lerp(Color, SourceColor(Frame).CopyWithNewOpacity(1.0f), FMath::Min(1.0f, Weight * 1.5f));
                Bulge = FMath::Max(Bulge, Weight * LevelOf(Frame) * (0.4f + 0.6f * PulseOf(Frame, 0.0f)));
                if (Frame.bOccluded && Weight > 0.15f && (Index / 3) % 2 == 1) { Color.A *= 0.30f; }
            }
            Path.Add(FVector(FMath::Cos(Theta), FMath::Sin(Theta), 0.0f));
            Colors.Add(Color);
            Widths.Add(RingThickness * (1.0f + LevelBulge * Bulge));
        }
        AddPath(Path, Colors, Widths, true, true, 0.40f);
    }

    // Per sound: icon at its scaled position, drop line to the ground and, for the most important few,
    // a cone of donuts that start at the sound and shrink into the ring. Donuts carry the sound's own
    // envelope, so each pulses in its rhythm. Frames arrive ordered by importance.
    struct FIcon { FName Kind; FVector2D Anchor; float Pixels; FLinearColor Color; bool bEdge; float Vertical; };
    TArray<FIcon> IconsToDraw;
    constexpr int32 DonutSegments = 48;
    constexpr float MaxHeightRadius = 1.0f; // keeps very high / deep sounds' drop lines from crossing the screen
    const float HalfAngle = FMath::DegreesToRadians(ConeHalfAngleDegrees);
    for (int32 Source = 0; Source < Frames.Num(); ++Source)
    {
        const FSoundCompassSourceFrame& Frame = Frames[Source];
        FVector Point = SourcePoint(Frame.AzimuthDegrees, Frame.DistanceMeters, Frame.ElevationMeters, MaxRangeRadius, HeightScale);
        Point.Z = FMath::Clamp(Point.Z, -MaxHeightRadius, MaxHeightRadius);
        const float Length = FMath::Max(1.0f, Point.Size());
        const FVector Axis = Point.GetSafeNormal(KINDA_SMALL_NUMBER, FVector::ForwardVector);
        const float Inner = FMath::Min(1.0f, Length - 0.35f);
        const float Near = FMath::Clamp(1.0f - (Length - NearRadius) / (MaxRangeRadius - NearRadius), 0.0f, 1.0f);
        const float Level = LevelOf(Frame);
        const FLinearColor Color = Desaturate(SourceColor(Frame), FMath::Lerp(0.65f, 1.0f, Near));
        // Pinned to the edge: only its bearing matters, so no cone.
        const bool bEdge = FVector2D(Point.X, Point.Y).Size() >= MaxRangeRadius - KINDA_SMALL_NUMBER;
        const int32 Donuts = (Source < MaxConeSources && !bEdge) ? DonutsPerCone : 0;

        for (int32 Donut = 0; Donut < Donuts; ++Donut)
        {
            const float Phase = FMath::Frac(static_cast<float>(Donut) / DonutsPerCone + WaveTime * WaveSpeed);
            const float Fade = FMath::Min(1.0f, Phase * 6.0f) * FMath::Min(1.0f, (1.0f - Phase) * 4.0f);
            // Kept light and thin: the cone is a direction hint behind the icon, not the main read.
            const float Alpha = 0.7f * Fade * Frame.VisualAlpha * (0.55f + 0.45f * Level) * (0.45f + 0.55f * PulseOf(Frame, Phase));
            if (Alpha <= 0.02f) { continue; }
            const float Along = FMath::Lerp(Length, Inner, Phase);
            TArray<FVector> Path;
            for (int32 Index = 0; Index < DonutSegments; ++Index)
            {
                // Capped so long cones (far sounds) don't grow donuts that swallow the screen.
                Path.Add(ConePoint(Axis, Along, HalfAngle, 2.0f * PI * Index / DonutSegments, 0.32f));
            }
            TArray<FLinearColor> Colors;
            Colors.Init(Color.CopyWithNewOpacity(Alpha), DonutSegments);
            TArray<float> Widths;
            Widths.Init(RingThickness * (0.35f + 0.3f * Level), DonutSegments);
            AddPath(Path, Colors, Widths, true, false, 0.65f, true);
        }

        // Drop line and ground mark: where it lands on the range rings is the distance, its length the height.
        const FVector Ground(Point.X, Point.Y, 0.0f);
        // Kept thin and faint: the label's "↑6m" carries the number, this only shows where it stands.
        const FLinearColor LineColor = Color.CopyWithNewOpacity(0.5f * Frame.VisualAlpha);
        if (FMath::Abs(Point.Z) > 0.03f)
        {
            TArray<FLinearColor> Colors;
            Colors.Init(LineColor, 2);
            TArray<float> Widths;
            Widths.Init(2.5f, 2);
            AddPath({Point, Ground}, Colors, Widths, false, false, 0.8f, true);
        }
        AddLoop(Ground, 0.07f, 20, LineColor, 3.0f, 0.8f);

        // Size says distance only (no perspective boost, which made far sounds near the virtual camera
        // as big as close ones); fading markers also shrink a little on their way out.
        const FProjected At = Project(Point, View);
        const float IconPixels = IconSize * PixelScale * FMath::Lerp(0.85f, 1.2f, Near) * FMath::Lerp(0.75f, 1.0f, Frame.VisualAlpha);
        const float Vertical = FMath::Abs(Frame.ElevationMeters) >= VerticalThresholdMeters ? FMath::Sign(Frame.ElevationMeters) : 0.0f;
        IconsToDraw.Add({IconFor(Frame.Category), ToScreen(At), IconPixels, Color.CopyWithNewOpacity(Frame.VisualAlpha), bEdge, Vertical});
    }

    // Painter's order: far first, so near band overlaps far band. One draw call keeps that order.
    Quads.Sort([](const FBandQuad& L, const FBandQuad& R) { return L.Depth > R.Depth; });
    const FSlateRenderTransform Transform = Geometry.GetAccumulatedRenderTransform();
    FShapeBuffer Band{{}, {}, Transform};
    Band.Verts.Reserve(Quads.Num() * 24);
    Band.Indexes.Reserve(Quads.Num() * 54);
    const FLinearColor Black(0.0f, 0.0f, 0.0f, 1.0f);
    for (const FBandQuad& Quad : Quads)
    {
        if (Quad.bGlow) { EmitQuad(Band.Verts, Band.Indexes, Transform, Quad, 4.0f, 7.0f, &Black, 0.28f); } // soft shadow on bright scenes
        EmitQuad(Band.Verts, Band.Indexes, Transform, Quad, 1.5f, 1.5f, &Black, 0.55f);                    // crisp outline
        EmitQuad(Band.Verts, Band.Indexes, Transform, Quad, 0.0f, 1.0f, nullptr, 1.0f);
    }
    // Outlined triangle: dark copy 35% larger behind the coloured one.
    auto OutlinedTriangle = [](FShapeBuffer& Buffer, const FVector2D& Centre, const FVector2D& Tip, const FVector2D& Side,
        const FLinearColor& Color)
    {
        for (const float Grow : {1.35f, 1.0f})
        {
            Buffer.Triangle(Centre + Tip * Grow, Centre + (Side - Tip * 0.6f) * Grow, Centre + (-Side - Tip * 0.6f) * Grow,
                Grow > 1.0f ? FLinearColor(0.0f, 0.0f, 0.0f, 0.6f * Color.A) : Color);
        }
    };

    // Markers: measure the two label lines, then de-overlap icons and labels on screen.
    TArray<FMarkerLayout> Layout;
    TArray<FString> Names, Distances;
    for (int32 Index = 0; Index < IconsToDraw.Num(); ++Index)
    {
        const FSoundCompassSourceFrame& Frame = Frames[Index];
        FString Distance = FString::Printf(TEXT("%dm"), FMath::RoundToInt(Frame.DistanceMeters));
        if (IconsToDraw[Index].Vertical != 0.0f)
        {
            // Horizontal distance, then the height with an arrow: "4m ↓5m".
            const float Horizontal = FMath::Sqrt(FMath::Max(0.0f, FMath::Square(Frame.DistanceMeters) - FMath::Square(Frame.ElevationMeters)));
            Distance = FString::Printf(TEXT("%dm %s%dm"), FMath::RoundToInt(Horizontal),
                Frame.ElevationMeters > 0.0f ? TEXT("↑") : TEXT("↓"), FMath::RoundToInt(FMath::Abs(Frame.ElevationMeters)));
        }
        Names.Add(DisplayName(Frame.Category));
        Distances.Add(Distance);
        const FVector2D NameSize = Measure->Measure(Names.Last(), SmallFont);
        const FVector2D DistanceSize = Measure->Measure(Distance, NumberFont);
        FMarkerLayout Item;
        Item.Anchor = IconsToDraw[Index].Anchor;
        Item.IconRadius = 0.75f * IconsToDraw[Index].Pixels;
        Item.LabelSize = FVector2D(FMath::Max(NameSize.X, DistanceSize.X), NameSize.Y + DistanceSize.Y) + PillPad * 2.0f;
        Layout.Add(Item);
    }
    LayoutMarkers(Layout);

    // Range labels under the markers, dropped where a marker covers them.
    for (const FLabel& Label : RangeLabels)
    {
        const FBox2D Box(Label.TopLeft - PillPad, Label.TopLeft + Label.Size + PillPad);
        if (Layout.ContainsByPredicate([&](const FMarkerLayout& Item) { return Overlaps(Box, MarkerBox(Item, 4.0f)); })) { continue; }
        Band.Rect(Box.Min, Box.Max, FLinearColor(0.02f, 0.03f, 0.05f, Label.PillAlpha));
        FSlateDrawElement::MakeText(Elements, Layer + 1, Geometry.ToPaintGeometry(Label.Size, FSlateLayoutTransform(Label.TopLeft)),
            Label.Text, Label.Font, ESlateDrawEffect::None, FLinearColor(0.95f, 0.97f, 1.0f, Label.Alpha));
    }
    Band.Flush(Elements, Layer);

    // Under the icons: leader lines for nudged icons, outline, rim and light disc, edge arrows.
    FShapeBuffer Under{{}, {}, Transform};
    FShapeBuffer Over{{}, {}, Transform};
    for (int32 Index = 0; Index < IconsToDraw.Num(); ++Index)
    {
        const FIcon& Icon = IconsToDraw[Index];
        const FMarkerLayout& Item = Layout[Index];
        const FVector2D Pos = Item.IconPos;
        const float P = Icon.Pixels;
        if ((Pos - Item.Anchor).Size() > 3.0f)
        {
            // The true position keeps a dot; a thin line leads to where the icon was moved.
            const FVector2D Dir = (Pos - Item.Anchor).GetSafeNormal();
            const FVector2D Side(-Dir.Y, Dir.X);
            const FLinearColor Leader = Icon.Color.CopyWithNewOpacity(0.8f * Icon.Color.A);
            Under.Triangle(Item.Anchor + Side, Item.Anchor - Side, Pos - Side, Leader);
            Under.Triangle(Item.Anchor + Side, Pos - Side, Pos + Side, Leader);
            Under.Disc(Item.Anchor, 3.0f * PixelScale, Leader, 12);
        }
        // The team's icons are black line art, so they sit on a light, category-tinted disc;
        // a thin dark outline separates the disc from bright backgrounds.
        Under.Disc(Pos, 0.80f * P, FLinearColor(0.0f, 0.0f, 0.0f, 0.5f * Icon.Color.A));
        Under.Disc(Pos, 0.75f * P, Icon.Color);
        Under.Disc(Pos, 0.65f * P, FMath::Lerp(Icon.Color, FLinearColor::White, 0.65f).CopyWithNewOpacity(0.95f * Icon.Color.A));
        if (Icon.bEdge)
        {
            // Points away from the centre: "farther than the edge".
            const FVector2D Away = Pos - Center;
            const FVector2D Out = Away.IsNearlyZero() ? FVector2D(0.0f, -1.0f) : Away.GetSafeNormal();
            const float S = 8.0f * PixelScale;
            OutlinedTriangle(Under, Pos + Out * (0.85f * P + S), Out * (S * 1.6f), FVector2D(-Out.Y, Out.X) * S, Icon.Color);
        }

        // Procedural glyphs, dark like the texture icons.
        const FLinearColor Glyph(0.05f, 0.06f, 0.09f, Icon.Color.A);
        if (Icon.Kind == TEXT("Wave"))
        {
            // Speaker-style wave: a dot and three arcs opening to the right.
            const FVector2D Origin = Pos - FVector2D(0.3f * P, 0.0f);
            Over.Disc(Origin, 0.08f * P, Glyph, 12);
            for (const float R : {0.25f, 0.45f, 0.65f})
            {
                Over.Arc(Origin, R * P, -0.8f, 0.8f, 0.09f * P, Glyph);
            }
        }
        else if (Icon.Kind == TEXT("Alert"))
        {
            // Exclamation mark.
            Over.Rect(Pos + FVector2D(-0.07f, -0.38f) * P, Pos + FVector2D(0.07f, 0.12f) * P, Glyph);
            Over.Disc(Pos + FVector2D(0.0f, 0.28f) * P, 0.08f * P, Glyph, 12);
        }
        if (Icon.Vertical != 0.0f)
        {
            // Above / below badge on the rim, so same-icon sounds (Footstep vs Upstairs) differ at a glance.
            const float S = 0.2f * P;
            OutlinedTriangle(Over, Pos + FVector2D(0.62f * P, -Icon.Vertical * 0.45f * P), FVector2D(0.0f, -Icon.Vertical * S),
                FVector2D(S, 0.0f), FLinearColor(1.0f, 1.0f, 1.0f, Icon.Color.A));
        }

        // Label block: one pill, name above distance. Labels go first when a marker fades, so
        // fading markers don't leave text smudges behind.
        const float TextAlpha = FMath::SmoothStep(0.45f, 0.85f, Icon.Color.A);
        const FVector2D TopLeft = Item.LabelTopLeft;
        Pills.Add({FBox2D(TopLeft, TopLeft + Item.LabelSize), 0.7f * TextAlpha});
        const FVector2D NameSize = Measure->Measure(Names[Index], SmallFont);
        const FVector2D DistanceSize = Measure->Measure(Distances[Index], NumberFont);
        const float MiddleX = TopLeft.X + 0.5f * Item.LabelSize.X;
        Labels.Add({FVector2D(MiddleX - 0.5f * NameSize.X, TopLeft.Y + PillPad.Y), Names[Index], SmallFont, NameSize, 0.85f * TextAlpha, 0.0f});
        Labels.Add({FVector2D(MiddleX - 0.5f * DistanceSize.X, TopLeft.Y + PillPad.Y + NameSize.Y), Distances[Index], NumberFont,
            DistanceSize, TextAlpha, 0.0f});
    }
    Under.Flush(Elements, Layer + 2);

    for (int32 Index = 0; Index < IconsToDraw.Num(); ++Index)
    {
        const FIcon& Icon = IconsToDraw[Index];
        if (const FSlateBrush* Brush = Icons.Find(Icon.Kind))
        {
            // White tint keeps the texture's own black line art.
            const FVector2D IconBoxSize(Icon.Pixels, Icon.Pixels);
            FSlateDrawElement::MakeBox(Elements, Layer + 3, Geometry.ToPaintGeometry(IconBoxSize,
                FSlateLayoutTransform(Layout[Index].IconPos - IconBoxSize * 0.5f)), Brush, ESlateDrawEffect::None,
                FLinearColor(1.0f, 1.0f, 1.0f, Icon.Color.A));
        }
    }

    // Dark pills behind text so it reads on clouds, the floor and the white character.
    for (const TPair<FBox2D, float>& Pill : Pills)
    {
        Over.Rect(Pill.Key.Min, Pill.Key.Max, FLinearColor(0.02f, 0.03f, 0.05f, Pill.Value));
    }
    Over.Flush(Elements, Layer + 4);

    for (const FLabel& Label : Labels)
    {
        FSlateDrawElement::MakeText(Elements, Layer + 5, Geometry.ToPaintGeometry(Label.Size, FSlateLayoutTransform(Label.TopLeft)),
            Label.Text, Label.Font, ESlateDrawEffect::None, FLinearColor(0.95f, 0.97f, 1.0f, Label.Alpha));
    }
    return Layer + 5;
}
