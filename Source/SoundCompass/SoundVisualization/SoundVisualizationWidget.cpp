#include "SoundVisualization/SoundVisualizationWidget.h"
#include "SoundVisualization/SoundCategory.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
float HudScale(const FVector2D& Size)
{
    return FMath::Min(1.0f, FMath::Min(static_cast<float>(Size.X / 1000.0), static_cast<float>(Size.Y / 650.0)));
}

FString Kind(FName Category)
{
    return SoundCompassCategory(Category).ToString();
}
}

USoundVisualizationWidget::USoundVisualizationWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    static ConstructorHelpers::FClassFinder<UUserWidget> Marker(TEXT("/Game/Ariel_NewMaterial/Ariel_WBP_Marker"));
    MarkerClass = Marker.Class;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Circle(TEXT("/Game/Ariel_NewMaterial/Ariel_Circle_Middle.Ariel_Circle_Middle"));
    RingMaterial = Circle.Object;
    const TPair<const TCHAR*, const TCHAR*> Paths[] = {
        {TEXT("Gunshot"), TEXT("Ariel_Bullet")}, {TEXT("Explosion"), TEXT("Ariel_Grenade")},
        {TEXT("Footstep"), TEXT("Ariel_Footprint")}, {TEXT("Door"), TEXT("Ariel_Door")},
        {TEXT("Noise"), TEXT("Ariel_Wheel")}, {TEXT("Ambience"), TEXT("Ariel_Airplane")}
    };
    for (const auto& Path : Paths)
    {
        const FString Asset = FString::Printf(TEXT("/Game/Ariel_NewMaterial/%s.%s"), Path.Value, Path.Value);
        ConstructorHelpers::FObjectFinder<UTexture2D> Texture(*Asset);
        if (Texture.Succeeded()) { Icons.Add(FName(Path.Key), Texture.Object); }
    }
}

void USoundVisualizationWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SoundCompassCanvas"));
    WidgetTree->RootWidget = RootCanvas;
    CenterRing = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("SoundCompassRing"));
    if (RingMaterial) { CenterRing->SetBrushFromMaterial(RingMaterial); }
    CenterRing->SetRenderOpacity(0.12f);
    if (UMaterialInstanceDynamic* Circle = CenterRing->GetDynamicMaterial())
    {
        Circle->SetVectorParameterValue(TEXT("LineColor"), FLinearColor(0.25f, 0.75f, 0.85f, 0.6f));
        Circle->SetScalarParameterValue(TEXT("Radius"), 0.42f);
        Circle->SetScalarParameterValue(TEXT("Thickness"), 0.008f);
    }
    RootCanvas->AddChildToCanvas(CenterRing);
}

void USoundVisualizationWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

UTexture2D* USoundVisualizationWidget::GetIcon(FName Category) const
{
    if (const TObjectPtr<UTexture2D>* Texture = Icons.Find(FName(*Kind(Category)))) { return Texture->Get(); }
    if (Kind(Category) == TEXT("Alert")) { return Icons.FindRef(TEXT("Ambience")).Get(); }
    if (const TObjectPtr<UTexture2D>* Texture = Icons.Find(TEXT("Noise"))) { return Texture->Get(); }
    return nullptr;
}

FLinearColor USoundVisualizationWidget::GetCategoryColor(FName Category)
{
    const FString Name = Kind(Category);
    if (Name == TEXT("Gunshot")) { return FLinearColor(1.0f, 0.25f, 0.20f); }
    if (Name == TEXT("Explosion")) { return FLinearColor(1.0f, 0.64f, 0.12f); }
    if (Name == TEXT("Footstep")) { return FLinearColor(0.25f, 0.90f, 0.54f); }
    if (Name == TEXT("Door")) { return FLinearColor(0.35f, 0.65f, 1.0f); }
    if (Name == TEXT("Conversation")) { return FLinearColor(0.80f, 0.50f, 1.0f); }
    if (Name == TEXT("Alert")) { return FLinearColor(1.0f, 0.92f, 0.20f); }
    if (Name == TEXT("Ambience")) { return FLinearColor(1.0f, 0.45f, 0.78f); }
    return FLinearColor(0.25f, 0.83f, 0.90f);
}

FString USoundVisualizationWidget::GetCategoryLabel(FName Category)
{
    if (Category == TEXT("SmokeTest")) { return TEXT("검증음"); }
    const FString Name = Kind(Category);
    if (Name == TEXT("Gunshot")) { return TEXT("총소리"); }
    if (Name == TEXT("Explosion")) { return TEXT("폭발"); }
    if (Name == TEXT("Footstep")) { return TEXT("발소리"); }
    if (Name == TEXT("Door")) { return TEXT("문소리"); }
    if (Name == TEXT("Conversation")) { return TEXT("대화"); }
    if (Name == TEXT("Ambience")) { return TEXT("환경음"); }
    if (Name == TEXT("Alert")) { return TEXT("경고음"); }
    return TEXT("소음");
}

void USoundVisualizationWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshTime += InDeltaTime;
    if (!RootCanvas || !GetWorld()) { return; }
    if (RefreshTime >= 1.0f / 30.0f)
    {
        RefreshTime = 0.0f;
        if (USoundVisualizationSubsystem* System = GetWorld()->GetSubsystem<USoundVisualizationSubsystem>())
        {
            CandidateFrames = System->GetSourceFrames(GetOwningPlayer());
            SourceFrames = System->SelectSourceFrames(CandidateFrames);
            DisplayMode = System->DisplayMode;
            SignalFrame = System->GetSignalFrame();
        }
    }
    UpdateMarkers(MyGeometry);
    InvalidateLayoutAndVolatility();
}

void USoundVisualizationWidget::UpdateMarkers(const FGeometry& MyGeometry)
{
    const FVector2D Size = MyGeometry.GetLocalSize();
    const float Scale = HudScale(Size);
    // Follow the projected pawn, including camera motion and viewport DPI scaling.
    RingCenter = Size * 0.5;
    if (APlayerController* Player = GetOwningPlayer())
    {
        if (APawn* Pawn = Player->GetPawn())
        {
            FVector2D Projected;
            if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Player,
                Pawn->GetActorLocation(), Projected, true)) { RingCenter = Projected; }
        }
    }
    const FVector2D Center = RingCenter;
    const float Radius = FMath::Clamp(RingRadius, 80.0f, 400.0f) * Scale;
    if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(CenterRing->Slot))
    {
        Slot->SetAlignment(FVector2D(0.5, 0.5));
        Slot->SetPosition(Center);
        Slot->SetSize(FVector2D(Radius / 0.42f, Radius / 0.42f));
    }
    CenterRing->SetVisibility(bShowCompass ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    TSet<int32> ActiveIds;
    const float IconExtent = FMath::Clamp(IconSize, 32.0f, 96.0f);
    const float MarkerExtent = IconExtent + 16.0f;
    const int32 Count = bShowCompass ? SourceFrames.Num() : 0;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const FSoundCompassSourceFrame& Source = SourceFrames[Index];
        ActiveIds.Add(Source.SourceId);
        TObjectPtr<UUserWidget>& Marker = Markers.FindOrAdd(Source.SourceId);
        if (!Marker && MarkerClass)
        {
            Marker = CreateWidget<UUserWidget>(GetOwningPlayer(), MarkerClass);
            if (Marker)
            {
                RootCanvas->AddChildToCanvas(Marker);
                Marker->WidgetTree->ForEachWidget([MarkerExtent](UWidget* Child)
                {
                    if (USizeBox* Box = Cast<USizeBox>(Child))
                    {
                        Box->SetWidthOverride(MarkerExtent);
                        Box->SetHeightOverride(MarkerExtent);
                    }
                    if (UImage* Image = Cast<UImage>(Child))
                    {
                        if (UCanvasPanelSlot* ImageSlot = Cast<UCanvasPanelSlot>(Image->Slot))
                        {
                            ImageSlot->SetAnchors(FAnchors(0.5f, 0.5f));
                            ImageSlot->SetAlignment(FVector2D(0.5f, 0.5f));
                            ImageSlot->SetPosition(FVector2D::ZeroVector);
                            ImageSlot->SetSize(FVector2D(MarkerExtent, MarkerExtent));
                            ImageSlot->SetAutoSize(false);
                        }
                    }
                });
                if (UImage* Icon = Cast<UImage>(Marker->GetWidgetFromName(TEXT("Image_Icon"))))
                {
                    if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(Icon->GetParent()))
                    {
                        UImage* Badge = Marker->WidgetTree->ConstructWidget<UImage>(
                            UImage::StaticClass(), TEXT("SoundCompassCategoryBadge"));
                        FSlateRoundedBoxBrush BadgeBrush(FLinearColor::White,
                            FLinearColor(0.02f, 0.035f, 0.06f), 1.5f);
                        BadgeBrush.OutlineSettings.bUseBrushTransparency = true;
                        Badge->SetBrush(BadgeBrush);
                        UCanvasPanelSlot* BadgeSlot = Canvas->AddChildToCanvas(Badge);
                        BadgeSlot->SetAnchors(FAnchors(0.5f, 0.5f));
                        BadgeSlot->SetAlignment(FVector2D(0.5f, 0.5f));
                        BadgeSlot->SetPosition(FVector2D::ZeroVector);
                        BadgeSlot->SetZOrder(-1);
                    }
                }
                Marker->SetVisibility(ESlateVisibility::HitTestInvisible);
            }
        }
        if (!Marker) { continue; }
        const float Angle = FMath::DegreesToRadians(Source.AzimuthDegrees);
        UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Marker->Slot);
        Slot->SetAlignment(FVector2D(0.5, 0.5));
        Slot->SetPosition(Center + FVector2D(FMath::Sin(Angle), -FMath::Cos(Angle)) *
            (Radius + (Source.SourceId % 3) * 8.0f * Scale));
        Slot->SetSize(FVector2D(MarkerExtent, MarkerExtent) * Scale);
        const FLinearColor Color = GetCategoryColor(Source.Category);
        if (UImage* Icon = Cast<UImage>(Marker->GetWidgetFromName(TEXT("Image_Icon"))))
        {
            Icon->SetBrushFromTexture(GetIcon(Source.Category));
            // Original textures have black strokes: preserve them over a coloured badge.
            Icon->SetColorAndOpacity(FLinearColor::White);
            if (UCanvasPanelSlot* IconSlot = Cast<UCanvasPanelSlot>(Icon->Slot))
            {
                IconSlot->SetSize(FVector2D(IconExtent, IconExtent));
            }
        }
        if (UImage* Badge = Cast<UImage>(Marker->GetWidgetFromName(TEXT("SoundCompassCategoryBadge"))))
        {
            Badge->SetColorAndOpacity(Color);
            if (UCanvasPanelSlot* BadgeSlot = Cast<UCanvasPanelSlot>(Badge->Slot))
            {
                BadgeSlot->SetSize(FVector2D(MarkerExtent, MarkerExtent));
            }
        }
        if (UImage* Wave = Cast<UImage>(Marker->GetWidgetFromName(TEXT("Image_Wave"))))
        {
            // The measured directional trail below replaces the looping marker animation.
            Wave->SetVisibility(ESlateVisibility::Collapsed);
        }
        // Quiet sounds keep a readable full-size icon throughout the hold period.
        Marker->SetRenderScale(FVector2D(0.70f + Source.VisualAlpha * 0.30f));
        Marker->SetRenderOpacity(Source.VisualAlpha * (Source.bOccluded ? 0.80f : 1.0f));
    }
    for (auto It = Markers.CreateIterator(); It; ++It)
    {
        if (!ActiveIds.Contains(It.Key()))
        {
            if (It.Value()) { It.Value()->RemoveFromParent(); }
            It.RemoveCurrent();
        }
    }
}

int32 USoundVisualizationWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,
    const FSlateRect& CullingRect, FSlateWindowElementList& Elements, int32 LayerId,
    const FWidgetStyle& Style, bool bEnabled) const
{
    const int32 Layer = Super::NativePaint(Args, Geometry, CullingRect, Elements, LayerId, Style, bEnabled) + 1;
    const FVector2D Size = Geometry.GetLocalSize();
    const float Scale = HudScale(Size);
    const FSlateBrush* Brush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
    const FLinearColor White(0.9f, 0.95f, 1.0f);
    const FLinearColor Muted(0.55f, 0.65f, 0.75f);
    const FLinearColor Cyan(0.25f, 0.90f, 0.95f);
    auto Box = [&](FVector2D Position, FVector2D Extent, FLinearColor Color)
    {
        FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(Extent,
            FSlateLayoutTransform(Position)), Brush, ESlateDrawEffect::None, Color);
    };
    auto Text = [&](FVector2D Position, const FString& Value, FLinearColor Color, int32 FontSize = 11)
    {
        FSlateDrawElement::MakeText(Elements, Layer + 1, Geometry.ToPaintGeometry(FVector2D(1, 1),
            FSlateLayoutTransform(Position)), Value, FCoreStyle::GetDefaultFontStyle(TEXT("Regular"),
            FMath::Max(8, FMath::RoundToInt(FontSize * Scale))), ESlateDrawEffect::None, Color);
    };
    auto Line = [&](const TArray<FVector2D>& Points, FLinearColor Color, float Thickness = 1.0f)
    {
        FSlateDrawElement::MakeLines(Elements, Layer + 1, Geometry.ToPaintGeometry(), Points,
            ESlateDrawEffect::None, Color, true, Thickness * Scale);
    };

    if (bShowCompass)
    {
        const FVector2D Center = RingCenter;
        const float Radius = FMath::Clamp(RingRadius, 80.0f, 400.0f) * Scale;
        TArray<FVector2D> Circle;
        for (int32 Point = 0; Point <= 64; ++Point)
        {
            const float Angle = 2.0f * PI * Point / 64.0f;
            Circle.Add(Center + FVector2D(FMath::Sin(Angle), FMath::Cos(Angle)) * Radius);
        }
        Line(Circle, Cyan.CopyWithNewOpacity(0.18f));
        // Each fixed-width arc uses this source's measured envelope history only.
        // Keep the captured shape during silence; amplitude and opacity fade together.
        for (const FSoundCompassSourceFrame& Source : SourceFrames)
        {
            if (Source.EnvelopeHistory.IsEmpty() || Source.VisualAlpha <= 0.0f) { continue; }
            float Peak = UE_SMALL_NUMBER;
            for (const float Value : Source.EnvelopeHistory) { Peak = FMath::Max(Peak, Value); }
            const float Strength = FMath::Clamp((Source.EstimatedListenerDb + 60.0f) / 60.0f, 0.0f, 1.0f);
            const float Height = (10.0f + 26.0f * Strength) * Source.VisualAlpha * Scale;
            const float SourceRadius = Radius + (Source.SourceId % 3) * 8.0f * Scale;
            TArray<FVector2D> Arc, OuterWave, InnerWave;
            constexpr int32 Points = 64;
            for (int32 Point = 0; Point <= Points; ++Point)
            {
                const float Fraction = static_cast<float>(Point) / Points;
                const float Angle = FMath::DegreesToRadians(SoundCompassPresentation::ArcAngleDegrees(
                    Source.AzimuthDegrees, WaveformArcDegrees, Fraction));
                const float SampleIndex = Fraction * (Source.EnvelopeHistory.Num() - 1);
                const int32 Left = FMath::FloorToInt(SampleIndex);
                const int32 Right = FMath::Min(Left + 1, Source.EnvelopeHistory.Num() - 1);
                const float Value = FMath::Lerp(Source.EnvelopeHistory[Left], Source.EnvelopeHistory[Right], SampleIndex - Left);
                const float Sample = FMath::Clamp(Value / Peak * (WaveformGain / 12.0f), 0.0f, 1.0f);
                const float Offset = Sample * Height * SoundCompassPresentation::ArcEdgeWeight(Fraction);
                const FVector2D Direction(FMath::Sin(Angle), -FMath::Cos(Angle));
                Arc.Add(Center + Direction * SourceRadius);
                OuterWave.Add(Center + Direction * (SourceRadius + Offset));
                InnerWave.Add(Center + Direction * (SourceRadius - Offset));
            }
            const FLinearColor Color = GetCategoryColor(Source.Category).CopyWithNewOpacity(
                Source.VisualAlpha * (Source.bOccluded ? 0.80f : 1.0f));
            Line(Arc, Color.CopyWithNewOpacity(Color.A * 0.35f), 3.0f);
            Line(OuterWave, Color, 2.0f);
            Line(InnerWave, Color.CopyWithNewOpacity(Color.A * 0.65f), 1.5f);
        }
        const float IconExtent = FMath::Clamp(IconSize, 32.0f, 96.0f);
        const float LabelGap = (IconExtent * 0.5f + 26.0f) * Scale;
        Text(Center + FVector2D(-7 * Scale, -Radius - LabelGap), TEXT("앞"), Muted);
        Text(Center + FVector2D(-7 * Scale, Radius + LabelGap - 10 * Scale), TEXT("뒤"), Muted);
        Text(Center + FVector2D(-Radius - LabelGap, -5 * Scale), TEXT("좌"), Muted);
        Text(Center + FVector2D(Radius + LabelGap, -5 * Scale), TEXT("우"), Muted);
        const FVector2D Info(Size.X - 310.0 * Scale, 24.0 * Scale);
        Box(Info, FVector2D(286, 86 + FMath::Min(SourceFrames.Num(), 6) * 24) * Scale,
            FLinearColor(0.01f, 0.025f, 0.045f, 0.75f));
        Text(Info + FVector2D(14, 10) * Scale, DisplayMode == ESoundCompassDisplayMode::ImportantOnly ?
            TEXT("중요 소리 선별") : TEXT("전체 소리 표시"), White, 14);
        Text(Info + FVector2D(14, 34) * Scale, FString::Printf(TEXT("표시 %d / 감지 %d"),
            SourceFrames.Num(), CandidateFrames.Num()), Cyan);
        for (int32 Index = 0; Index < FMath::Min(SourceFrames.Num(), 6); ++Index)
        {
            const auto& Source = SourceFrames[Index];
            Text(Info + FVector2D(14, 60 + Index * 24) * Scale,
                FString::Printf(TEXT("%d. %s  %.1fm  중요도 %.0f"), Index + 1,
                    *GetCategoryLabel(Source.Category), Source.DistanceMeters, Source.ImportanceScore * 100.0f),
                GetCategoryColor(Source.Category).CopyWithNewOpacity(Source.VisualAlpha));
        }
        Text(FVector2D(24 * Scale, Size.Y - 22 * Scale), TEXT("B: 전체/선별   F1: 파형·주파수 분석   V: 표시 켜기/끄기"), Muted, 10);
        for (int32 Index = 0; Index < SourceFrames.Num(); ++Index)
        {
            const auto& Source = SourceFrames[Index];
            const float Angle = FMath::DegreesToRadians(Source.AzimuthDegrees);
            const FVector2D Position = Center + FVector2D(FMath::Sin(Angle), -FMath::Cos(Angle)) *
                (Radius + (Source.SourceId % 3) * 8.0f * Scale);
            Text(Position + FVector2D(IconExtent * 0.5f + 16, -IconExtent * 0.5f - 16) * Scale, FString::FromInt(Index + 1),
                GetCategoryColor(Source.Category).CopyWithNewOpacity(Source.VisualAlpha), 12);
        }
        if (!MarkerClass)
        {
            for (int32 Index = 0; Index < SourceFrames.Num(); ++Index)
            {
                const auto& Source = SourceFrames[Index];
                const float Angle = FMath::DegreesToRadians(Source.AzimuthDegrees);
                const FVector2D Position = Center + FVector2D(FMath::Sin(Angle), -FMath::Cos(Angle)) * Radius;
                const float Extent = FMath::Clamp(IconSize, 32.0f, 96.0f) * Source.VisualAlpha;
                Box(Position - FVector2D(Extent * 0.5f) * Scale, FVector2D(Extent) * Scale,
                    GetCategoryColor(Source.Category).CopyWithNewOpacity(Source.VisualAlpha));
            }
        }
    }

    if (bShowSourceList)
    {
        const int32 Count = FMath::Min(CandidateFrames.Num(), 10);
        const FVector2D Origin(24.0 * Scale, 24.0 * Scale);
        Box(Origin, FVector2D(375, 58 + 32 * Count) * Scale, FLinearColor(0.01f, 0.025f, 0.045f, 0.82f));
        Text(Origin + FVector2D(14, 10) * Scale, TEXT("후보 소리 / 중요도"), White, 14);
        if (!Count) { Text(Origin + FVector2D(14, 34) * Scale, TEXT("소리가 재생되면 표시됩니다"), Muted); }
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const auto& Source = CandidateFrames[Index];
            const FVector2D Row = Origin + FVector2D(14, 36 + Index * 32) * Scale;
            const FLinearColor Color = GetCategoryColor(Source.Category);
            Text(Row, FString::Printf(TEXT("%.0f  %s  %.1fm  %+.0f°%s"), Source.ImportanceScore * 100.0f,
                *GetCategoryLabel(Source.Category), Source.DistanceMeters, Source.AzimuthDegrees,
                Source.bOccluded ? TEXT("  가려짐") : TEXT("")), Color);
            Box(Row + FVector2D(0, 19) * Scale, FVector2D(200, 3) * Scale, FLinearColor(0.1f, 0.15f, 0.2f));
            const float Strength = FMath::Clamp((Source.EstimatedListenerDb + 60.0f) / 60.0f, 0.0f, 1.0f);
            Box(Row + FVector2D(0, 19) * Scale, FVector2D(200 * Strength, 3) * Scale, Color);
            Text(Row + FVector2D(220, 15) * Scale, FString::Printf(TEXT("추정 %.0f dB"), Source.EstimatedListenerDb), Muted, 9);
        }
    }

    if (bShowSignal)
    {
        const FVector2D Origin(24.0 * Scale, Size.Y - 238.0 * Scale);
        Box(Origin, FVector2D(440, 214) * Scale, FLinearColor(0.01f, 0.025f, 0.045f, 0.88f));
        Text(Origin + FVector2D(14, 10) * Scale, TEXT("실시간 오디오 분석"), White, 14);
        Text(Origin + FVector2D(230, 12) * Scale, SignalFrame.bReceivingAudio ?
            FString::Printf(TEXT("%.1f dBFS · %.0f Hz"), SignalFrame.RmsDb, SignalFrame.DominantFrequencyHz) :
            TEXT("오디오 입력 대기"), Cyan);
        const FVector2D WaveOrigin = Origin + FVector2D(14, 42) * Scale;
        Line({WaveOrigin + FVector2D(0, 32) * Scale, WaveOrigin + FVector2D(412, 32) * Scale}, Muted.CopyWithNewOpacity(0.25f));
        if (SignalFrame.Waveform.Num() > 1)
        {
            TArray<FVector2D> Points;
            for (int32 Index = 0; Index < SignalFrame.Waveform.Num(); ++Index)
            {
                Points.Add(WaveOrigin + FVector2D(412.0 * Index / (SignalFrame.Waveform.Num() - 1),
                    32.0 - FMath::Clamp(SignalFrame.Waveform[Index], -1.0f, 1.0f) * 30.0) * Scale);
            }
            Line(Points, Cyan, 1.5f);
        }
        Text(Origin + FVector2D(14, 108) * Scale, TEXT("주파수 스펙트럼"), Muted, 10);
        const int32 Bands = SignalFrame.SpectrumDb.Num();
        for (int32 Band = 0; Band < Bands; ++Band)
        {
            const float Height = FMath::Clamp((SignalFrame.SpectrumDb[Band] + 80.0f) / 80.0f, 0.0f, 1.0f) * 56.0f;
            Box(Origin + FVector2D(14.0f + 412.0f * Band / Bands, 184.0f - Height) * Scale,
                FVector2D(FMath::Max(1.0f, 412.0f / Bands - 3.0f), Height) * Scale, Cyan);
        }
        Text(Origin + FVector2D(14, 192) * Scale, TEXT("40 Hz"), Muted, 9);
        Text(Origin + FVector2D(350, 192) * Scale, FString::Printf(TEXT("%.0f Hz"),
            FMath::Min(16000.0f, SignalFrame.SampleRate > 0 ? SignalFrame.SampleRate * 0.5f : 16000.0f)), Muted, 9);
    }
    return Layer + 1;
}

void USoundVisualizationWidget::NativeDestruct()
{
    Markers.Empty();
    SourceFrames.Empty();
    CandidateFrames.Empty();
    Super::NativeDestruct();
}

void USoundVisualizationWidget::ToggleDiagnostics()
{
    bShowSignal = !bShowSignal;
    bShowSourceList = bShowSignal;
}
