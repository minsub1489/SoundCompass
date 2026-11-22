#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SoundVisualization/SoundVisualizationSubsystem.h"
#include "SoundHaloWidget.generated.h"

/**
 * Screen overlay: a 3D ring around the screen centre seen from a virtual camera, like a small radar.
 * Each sound sits at its log-scaled distance with an icon, a drop line to the ground and a cone of
 * shrinking donuts pointing at the listener. The ring itself is a directional level meter.
 * Only reads source frames plus the character's position and the camera's facing, so it can move
 * outside the game later. Bearings are re-measured from the character (the ring's centre on screen),
 * not from the audio listener at the camera. The far side is thinner, paler
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
    /** Virtual camera pitch when not following the game camera, and the fallback without one. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="5.0", ClampMax="80.0", Units="deg"))
    float ViewPitchDegrees = 30.0f;
    /** Tilt the ring with the game camera so the ring plane matches the floor on screen. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo")
    bool bFollowCameraPitch = true;
    /** Added to the game camera's downward pitch; keeps the ring open enough to read. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="0.0", ClampMax="40.0", Units="deg"))
    float CameraPitchOffset = 10.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="5.0", ClampMax="80.0", Units="deg"))
    float MinViewPitchDegrees = 18.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="5.0", ClampMax="85.0", Units="deg"))
    float MaxViewPitchDegrees = 65.0f;
    /** Virtual camera distance in ring radii. Smaller = stronger perspective. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="3.0", ClampMax="10.0"))
    float CameraDistance = 4.5f;
    /** Band thickness in pixels at the ring centre's depth. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="1.0", ClampMax="30.0"))
    float RingThickness = 12.0f;
    /** Extra band thickness at a sound's bearing at full level, as a multiple of RingThickness. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="0.0", ClampMax="4.0"))
    float LevelBulge = 2.2f;
    /** Category icon size in pixels at the ring centre's depth. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="16.0", ClampMax="128.0"))
    float IconSize = 40.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="5.0", ClampMax="45.0", Units="deg"))
    float ConeHalfAngleDegrees = 16.0f;
    /** Farthest drawn distance in ring radii (2.6 ~ 45 m); farther sounds sit on this edge. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="1.5", ClampMax="3.0"))
    float MaxRangeRadius = 2.6f;
    /** Exaggerates heights so above / below reads clearly against the flat ring. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="1.0", ClampMax="4.0"))
    float HeightScale = 1.5f;
    /** Height difference (m) from which a sound counts as above / below for labels and the rim badge. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="0.5", ClampMax="10.0", Units="m"))
    float VerticalThresholdMeters = 2.0f;
    /** Same-category sounds closer than this are one marker (repeating emitters, a walker's steps). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="0.0", ClampMax="20.0", Units="m"))
    float MergeRadiusMeters = 3.0f;
    /** Only the most important sounds get donut cones; the rest show icon, label and ring colour. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="0", ClampMax="8"))
    int32 MaxConeSources = 3;
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
    float CurrentPitch = 30.0f;
};
