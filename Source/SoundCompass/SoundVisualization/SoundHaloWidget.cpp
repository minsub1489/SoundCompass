#include "SoundVisualization/SoundHaloWidget.h"
#include "SoundVisualization/SoundCategory.h"
#include "SoundVisualization/SoundHaloGeometry.h"
#include "Brushes/SlateRoundedBoxBrush.h"
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
FLinearColor CategoryColor(FName Category)
{
    const FName Kind = SoundCompassCategory(Category);
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
    // The team's category icons (also used by the original HUD).
    const TPair<const TCHAR*, const TCHAR*> Paths[] = {
        {TEXT("Gunshot"), TEXT("Ariel_Bullet")}, {TEXT("Explosion"), TEXT("Ariel_Grenade")},
        {TEXT("Footstep"), TEXT("Ariel_Footprint")}, {TEXT("Door"), TEXT("Ariel_Door")},
        {TEXT("Noise"), TEXT("Ariel_Wheel")}, {TEXT("Ambience"), TEXT("Ariel_Airplane")},
        {TEXT("GlassBreak"), TEXT("Ariel_GlassBreak")}, {TEXT("Throw"), TEXT("Ariel_Throw")}
    };
    for (const auto& Path : Paths)
    {
        const FString Asset = FString::Printf(TEXT("/Game/Ariel_NewMaterial/%s.%s"), Path.Value, Path.Value);
        ConstructorHelpers::FObjectFinder<UTexture2D> Texture(*Asset);
        if (!Texture.Succeeded()) { continue; }
        FSlateBrush Brush;
        Brush.SetResourceObject(Texture.Object);
        Brush.ImageSize = FVector2D(64.0f, 64.0f);
        Icons.Add(FName(Path.Key), Brush);
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
    if (USoundVisualizationSubsystem* System = GetWorld() ? GetWorld()->GetSubsystem<USoundVisualizationSubsystem>() : nullptr)
    {
        Frames = System->GetVisualizationFrames(GetOwningPlayer());
    }
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
    const FView View{ViewPitchDegrees, CameraDistance};
    auto ToScreen = [&](const FProjected& P) { return Center + P.Screen * Radius; };

    TArray<FBandQuad> Quads;
    // Path in listener space -> projected band quads with depth cues baked into the colours.
    // FarAlpha: how much of the opacity survives on the far side.
    auto AddPath = [&](const TArray<FVector>& Path, const TArray<FLinearColor>& Colors, const TArray<float>& Thickness,
        bool bClosed, bool bGlow, float FarAlpha)
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
            Points.Add({ToScreen(P), FVector2D::ZeroVector, 0.5f * Thickness[Index] * P.Scale * PixelScale, Color});
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
    auto AddLoop = [&](const FVector& Centre, float LoopRadius, int32 Segments, const FLinearColor& Color, float Thickness, float FarAlpha)
    {
        TArray<FVector> Path;
        for (int32 Index = 0; Index < Segments; ++Index)
        {
            const float Theta = 2.0f * PI * Index / Segments;
            Path.Add(Centre + FVector(FMath::Cos(Theta), FMath::Sin(Theta), 0.0f) * LoopRadius);
        }
        TArray<FLinearColor> Colors;
        Colors.Init(Color, Segments);
        TArray<float> Widths;
        Widths.Init(Thickness, Segments);
        AddPath(Path, Colors, Widths, true, false, FarAlpha);
    };

    struct FLabel { FVector2D Pos; FString Text; float Alpha; int32 FontSize; };
    TArray<FLabel> Labels;

    // Range rings on the ground: read a sound's distance by where its drop line lands.
    for (const float Meters : {5.0f, 15.0f, 30.0f})
    {
        const float RangeRadius = DistanceToRadius(Meters, MaxRangeRadius);
        if (RangeRadius >= MaxRangeRadius) { continue; }
        AddLoop(FVector::ZeroVector, RangeRadius, 96, FLinearColor(0.85f, 0.95f, 1.0f, 0.30f), 2.0f, 0.5f);
        Labels.Add({ToScreen(Project(FVector(0.0f, RangeRadius, 0.0f), View)) + FVector2D(4.0f, -8.0f),
            FString::Printf(TEXT("%dm"), FMath::RoundToInt(Meters)), 0.7f, 9});
    }

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
            FLinearColor Color(0.85f, 0.95f, 1.0f, 0.85f);
            float Bulge = 0.0f;
            for (int32 Source = 0; Source < Frames.Num(); ++Source)
            {
                const FSoundCompassSourceFrame& Frame = Frames[Source];
                const float Delta = FMath::Abs(FMath::FindDeltaAngleDegrees(FMath::RadiansToDegrees(Theta), Frame.AzimuthDegrees));
                // Overhead / underfoot sounds barely light a bearing; the centre arrows carry them instead.
                const float Weight = FMath::Square(FMath::Clamp(1.0f - Delta / 28.0f, 0.0f, 1.0f)) * Frame.VisualAlpha
                    * (Source == Nearest ? 1.0f : 0.6f) * FMath::Square(HorizontalShare(Frame));
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

    // Per sound: icon at its scaled position, drop line to the ground, and a cone of donuts that start
    // at the sound and shrink into the ring. Donuts carry the sound's own envelope, so each pulses in its rhythm.
    struct FIcon { FName Kind; FProjected At; FLinearColor Color; float Near; };
    TArray<FIcon> IconsToDraw;
    constexpr int32 DonutSegments = 48;
    const float HalfAngle = FMath::DegreesToRadians(ConeHalfAngleDegrees);
    for (const FSoundCompassSourceFrame& Frame : Frames)
    {
        const FVector Point = SourcePoint(Frame.AzimuthDegrees, Frame.DistanceMeters, Frame.ElevationMeters, MaxRangeRadius, HeightScale);
        const float Length = FMath::Max(1.0f, Point.Size());
        const FVector Axis = Point.GetSafeNormal(KINDA_SMALL_NUMBER, FVector::ForwardVector);
        const float Inner = FMath::Min(1.0f, Length - 0.35f);
        const float Near = FMath::Clamp(1.0f - (Length - 1.0f) / (MaxRangeRadius - 1.0f), 0.0f, 1.0f);
        const float Level = LevelOf(Frame);
        const FLinearColor Color = Desaturate(SourceColor(Frame), FMath::Lerp(0.65f, 1.0f, Near));

        for (int32 Donut = 0; Donut < DonutsPerCone; ++Donut)
        {
            const float Phase = FMath::Frac(static_cast<float>(Donut) / DonutsPerCone + WaveTime * WaveSpeed);
            const float Fade = FMath::Min(1.0f, Phase * 6.0f) * FMath::Min(1.0f, (1.0f - Phase) * 4.0f);
            const float Alpha = Fade * Frame.VisualAlpha * (0.55f + 0.45f * Level) * (0.45f + 0.55f * PulseOf(Frame, Phase));
            if (Alpha <= 0.02f) { continue; }
            const float Along = FMath::Lerp(Length, Inner, Phase);
            TArray<FVector> Path;
            for (int32 Index = 0; Index < DonutSegments; ++Index)
            {
                Path.Add(ConePoint(Axis, Along, HalfAngle, 2.0f * PI * Index / DonutSegments));
            }
            TArray<FLinearColor> Colors;
            Colors.Init(Color.CopyWithNewOpacity(Alpha), DonutSegments);
            TArray<float> Widths;
            Widths.Init(RingThickness * (0.55f + 0.45f * Level), DonutSegments);
            AddPath(Path, Colors, Widths, true, false, 0.65f);
        }

        // Drop line and ground mark: where it lands on the range rings is the distance, its length the height.
        const FVector Ground(Point.X, Point.Y, 0.0f);
        const FLinearColor LineColor = Color.CopyWithNewOpacity(0.75f * Frame.VisualAlpha);
        if (FMath::Abs(Point.Z) > 0.03f)
        {
            TArray<FLinearColor> Colors;
            Colors.Init(LineColor, 2);
            TArray<float> Widths;
            Widths.Init(5.0f, 2);
            AddPath({Point, Ground}, Colors, Widths, false, false, 0.8f);
        }
        AddLoop(Ground, 0.07f, 20, LineColor, 4.0f, 0.8f);

        const FProjected At = Project(Point, View);
        IconsToDraw.Add({SoundCompassCategory(Frame.Category), At, Color.CopyWithNewOpacity(Frame.VisualAlpha), Near});
    }

    // Painter's order: far first, so near band overlaps far band. One draw call keeps that order.
    Quads.Sort([](const FBandQuad& L, const FBandQuad& R) { return L.Depth > R.Depth; });
    TArray<FSlateVertex> Verts;
    TArray<SlateIndex> Indexes;
    Verts.Reserve(Quads.Num() * 24);
    Indexes.Reserve(Quads.Num() * 54);
    const FSlateRenderTransform Transform = Geometry.GetAccumulatedRenderTransform();
    const FLinearColor Black(0.0f, 0.0f, 0.0f, 1.0f);
    for (const FBandQuad& Quad : Quads)
    {
        if (Quad.bGlow) { EmitQuad(Verts, Indexes, Transform, Quad, 4.0f, 7.0f, &Black, 0.28f); } // soft shadow on bright scenes
        EmitQuad(Verts, Indexes, Transform, Quad, 1.5f, 1.5f, &Black, 0.55f);                    // crisp outline
        EmitQuad(Verts, Indexes, Transform, Quad, 0.0f, 1.0f, nullptr, 1.0f);
    }

    // Above / below arrows over the head and under the feet: always faintly present, lit in the colour
    // of the strongest sound above or below and beating with it.
    auto AddTriangle = [&](const FVector2D (&Points)[3], const FLinearColor& Color)
    {
        const SlateIndex Base = Verts.Num();
        for (const FVector2D& Point : Points)
        {
            Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Transform, FVector2f(Point), FVector2f(0.5f, 0.5f),
                Color.ToFColor(true)));
        }
        Indexes.Append({Base, static_cast<SlateIndex>(Base + 1), static_cast<SlateIndex>(Base + 2)});
    };
    for (const float Sign : {1.0f, -1.0f})
    {
        const FSoundCompassSourceFrame* Strongest = nullptr;
        float StrongestAlpha = 0.0f;
        for (const FSoundCompassSourceFrame& Frame : Frames)
        {
            const float Alpha = Frame.VisualAlpha * (0.5f + 0.5f * PulseOf(Frame, 0.0f));
            if (Sign * Frame.ElevationMeters >= VerticalThresholdMeters && Alpha > StrongestAlpha)
            {
                Strongest = &Frame;
                StrongestAlpha = Alpha;
            }
        }
        const FVector2D At = ToScreen(Project(FVector(0.0f, 0.0f, Sign > 0.0f ? 1.15f : -0.75f), View));
        const float Half = 15.0f * PixelScale * (Strongest ? 1.3f : 1.0f);
        const FLinearColor Color = Strongest
            ? SourceColor(*Strongest).CopyWithNewOpacity(FMath::Max(0.35f, StrongestAlpha)) : FLinearColor(1.0f, 1.0f, 1.0f, 0.2f);
        for (const float Grow : {1.35f, 1.0f})
        {
            const float S = Half * Grow;
            const FVector2D Points[3] = {At + FVector2D(0.0f, -Sign * S), At + FVector2D(-S, Sign * S * 0.6f), At + FVector2D(S, Sign * S * 0.6f)};
            AddTriangle(Points, Grow > 1.0f ? FLinearColor(0.0f, 0.0f, 0.0f, 0.6f * Color.A) : Color);
        }
    }
    if (Verts.Num() > 0 && FSlateApplication::IsInitialized())
    {
        const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(
            *FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")));
        FSlateDrawElement::MakeCustomVerts(Elements, Layer, Handle, Verts, Indexes, nullptr, 0, 0);
    }

    // Icons: coloured rim, dark disc, lightened icon. Nearer sounds get bigger icons.
    static const FSlateRoundedBoxBrush Disc(FLinearColor::White, 999.0f);
    auto DrawDisc = [&](const FVector2D& Pos, float Diameter, int32 DiscLayer, const FLinearColor& Color)
    {
        FSlateDrawElement::MakeBox(Elements, DiscLayer, Geometry.ToPaintGeometry(FVector2D(Diameter, Diameter),
            FSlateLayoutTransform(Pos - FVector2D(Diameter, Diameter) * 0.5f)), &Disc, ESlateDrawEffect::None, Color);
    };
    const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    for (int32 Index = 0; Index < IconsToDraw.Num(); ++Index)
    {
        const FIcon& Icon = IconsToDraw[Index];
        const float IconPixels = IconSize * PixelScale * FMath::Lerp(0.8f, 1.25f, Icon.Near) * FMath::Clamp(Icon.At.Scale, 0.75f, 1.35f);
        const FVector2D Pos = ToScreen(Icon.At);
        DrawDisc(Pos, IconPixels * 1.5f, Layer + 1, Icon.Color);
        DrawDisc(Pos, IconPixels * 1.32f, Layer + 2, FLinearColor(0.02f, 0.03f, 0.05f, 0.85f * Icon.Color.A));
        const FSlateBrush* Brush = Icons.Find(Icon.Kind);
        if (!Brush) { Brush = Icons.Find(Icon.Kind == TEXT("Alert") ? TEXT("Ambience") : TEXT("Noise")); }
        if (Brush)
        {
            FSlateDrawElement::MakeBox(Elements, Layer + 3, Geometry.ToPaintGeometry(FVector2D(IconPixels, IconPixels),
                FSlateLayoutTransform(Pos - FVector2D(IconPixels, IconPixels) * 0.5f)), Brush, ESlateDrawEffect::None,
                FMath::Lerp(Icon.Color, FLinearColor::White, 0.35f).CopyWithNewOpacity(Icon.Color.A));
        }
        const FSoundCompassSourceFrame& Frame = Frames[Index];
        FString Text = FString::Printf(TEXT("%dm"), FMath::RoundToInt(Frame.DistanceMeters));
        if (FMath::Abs(Frame.ElevationMeters) >= VerticalThresholdMeters)
        {
            Text += FString::Printf(TEXT("  %s %dm"), Frame.ElevationMeters > 0.0f ? TEXT("위") : TEXT("아래"),
                FMath::RoundToInt(FMath::Abs(Frame.ElevationMeters)));
        }
        Labels.Add({Pos + FVector2D(0.0f, IconPixels * 0.75f + 2.0f), Text, Icon.Color.A, 12});
    }

    // Labels centred on their anchor (horizontally), with a dark drop shadow.
    for (const FLabel& Label : Labels)
    {
        const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", Label.FontSize);
        const FVector2D TextSize = Measure->Measure(Label.Text, Font);
        const FVector2D TopLeft = Label.Pos - FVector2D(TextSize.X * 0.5f, 0.0f);
        FSlateDrawElement::MakeText(Elements, Layer + 4, Geometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(TopLeft + FVector2D(1.0f, 1.0f))),
            Label.Text, Font, ESlateDrawEffect::None, FLinearColor(0.0f, 0.0f, 0.0f, 0.8f * Label.Alpha));
        FSlateDrawElement::MakeText(Elements, Layer + 5, Geometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(TopLeft)),
            Label.Text, Font, ESlateDrawEffect::None, FLinearColor(0.95f, 0.97f, 1.0f, Label.Alpha));
    }
    return Layer + 5;
}
