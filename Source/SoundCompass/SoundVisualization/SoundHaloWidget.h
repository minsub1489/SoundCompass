#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SoundVisualization/SoundVisualizationSubsystem.h"
#include "SoundHaloWidget.generated.h"

/**
 * Screen overlay: a 3D ring around the screen centre seen from a virtual camera, like a small radar.
 * Each sound sits at its log-scaled distance with an icon, a drop line to the ground and a cone of
 * shrinking donuts pointing at the listener. The ring itself is a directional level meter.
 * Only reads source frames, so it can move outside the game later. The far side is thinner, paler
 * and more transparent so it reads as behind the character; the near side reads as in front.
 */
UCLASS()
class SOUNDCOMPASS_API USoundHaloWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    USoundHaloWidget(const FObjectInitializer& ObjectInitializer);

    /** Ring radius in pixels on a 900 px tall viewport. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="60.0", ClampMax="400.0"))
    float RingRadius = 130.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="5.0", ClampMax="80.0", Units="deg"))
    float ViewPitchDegrees = 30.0f;
    /** Virtual camera distance in ring radii. Smaller = stronger perspective. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="3.0", ClampMax="10.0"))
    float CameraDistance = 4.5f;
    /** Band thickness in pixels at the ring centre's depth. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="1.0", ClampMax="30.0"))
    float RingThickness = 12.0f;
    /** Extra band thickness at a sound's bearing at full level, as a multiple of RingThickness. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="0.0", ClampMax="4.0"))
    float LevelBulge = 1.6f;
    /** Category icon size in pixels at the ring centre's depth. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="16.0", ClampMax="128.0"))
    float IconSize = 56.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="5.0", ClampMax="45.0", Units="deg"))
    float ConeHalfAngleDegrees = 16.0f;
    /** Farthest drawn distance in ring radii (2.4 ~ 33 m); farther sounds sit on this edge. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="1.5", ClampMax="3.0"))
    float MaxRangeRadius = 2.4f;
    /** Exaggerates heights so above / below reads clearly against the flat ring. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="1.0", ClampMax="4.0"))
    float HeightScale = 1.8f;
    /** Height difference (m) from which a sound counts as above / below for labels and the centre arrows. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="0.5", ClampMax="10.0", Units="m"))
    float VerticalThresholdMeters = 2.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="1", ClampMax="12"))
    int32 DonutsPerCone = 4;
    /** Times per second each donut travels the full cone. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="0.0", ClampMax="5.0"))
    float WaveSpeed = 0.7f;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
        const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    UPROPERTY() TMap<FName, FSlateBrush> Icons;
    UPROPERTY(Transient) TArray<FSoundCompassSourceFrame> Frames;
    float WaveTime = 0.0f;
};
