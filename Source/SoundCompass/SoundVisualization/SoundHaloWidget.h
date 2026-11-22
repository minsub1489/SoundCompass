#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SoundVisualization/SoundVisualizationSubsystem.h"
#include "SoundHaloWidget.generated.h"

/**
 * Gameplay sound overlay, built to stay out of the way:
 * - Centre: a thin 3D ring around the character seen from a virtual camera (far side thinner, paler and
 *   fainter so it reads as behind the character). Quiet by default; each sound's bearing lights up in its
 *   colour, swells with its level and beats with its envelope.
 * - Markers: one icon per sound, same bearing convention as the ring (front = top, behind = bottom).
 *   No text: distance is position (near = just outside the ring, far = out at the edge ellipse), danger is
 *   size. A new sound sends one short wave from its icon in to the ring. Names and distances show only on
 *   request (F1).
 * - Range rings on the ground (5 / 15 / 30 m, dashed 40 m+ limit) and a shaded 3D tetrahedron for each
 *   marked sound standing at its distance between them, aimed along its 3D direction:
 *   tilt = above / below, perspective = front / back.
 * Bearings are re-measured from the character (facing where the camera looks), not from the audio
 * listener at the camera. Only reads source frames plus the character's position and the camera's
 * facing, so it can move outside the game later.
 */
UCLASS()
class SOUNDCOMPASS_API USoundHaloWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    USoundHaloWidget(const FObjectInitializer& ObjectInitializer);

    /** Shows names and distances under the edge icons (for tuning, spectating, debugging). */
    UFUNCTION(BlueprintCallable, Category="Sound Halo")
    void ToggleLabels() { bShowLabels = !bShowLabels; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo")
    bool bShowLabels = false;

    /** Ring radius in pixels on a 900 px tall viewport. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="60.0", ClampMax="400.0"))
    float RingRadius = 130.0f;
    /** Band thickness in pixels at the ring centre's depth. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="1.0", ClampMax="30.0"))
    float RingThickness = 7.0f;
    /** Extra band thickness at a sound's bearing at full level, as a multiple of RingThickness. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="0.0", ClampMax="4.0"))
    float LevelBulge = 2.0f;
    /** Opacity of the band where no sound is. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="0.0", ClampMax="1.0"))
    float IdleRingOpacity = 0.3f;

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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="2.2", ClampMax="10.0"))
    float CameraDistance = 4.5f; // far enough that the 40 m ring behind the character stays on screen
    /** Outermost range ring in ring radii (2.6 ~ 40 m); pointers for farther sounds stand on it. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="1.5", ClampMax="3.0"))
    float MaxRangeRadius = 2.6f;

    /** Ellipse far sounds sit on, as a fraction of the half screen; leaves a safe margin for game HUDs. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="0.3", ClampMax="1.0"))
    float EdgeEllipseScale = 0.75f;
    /** Where the nearest sounds sit, as a multiple of the ring's on-screen radius (clear of the character). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="1.1", ClampMax="3.0"))
    float NearMarkerRadius = 1.6f;
    /** Icon size in pixels on a 900 px tall viewport; dangerous sounds are drawn larger. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="16.0", ClampMax="96.0"))
    float IconSize = 40.0f;
    /** Icon opacity once a sound is no longer new; new sounds start fully opaque, then settle to this. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="0.1", ClampMax="1.0"))
    float IconOpacity = 0.1f;
    /** Sounds this far or farther sit on the edge ellipse. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="5.0", ClampMax="200.0", Units="m"))
    float FarDistanceMeters = 40.0f;
    /** Only the most important sounds get an edge icon; the rest still light the ring. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="0", ClampMax="12"))
    int32 MaxEdgeMarkers = 4;
    /** Seconds the onset wave takes to travel from the edge icon to the ring. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="0.1", ClampMax="3.0", Units="s"))
    float OnsetWaveSeconds = 0.6f;

    /** Ring pointer length in ring radii (dangerous sounds a little longer). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="0.1", ClampMax="1.0"))
    float PointerLength = 0.4f;
    /** Height difference (m) from which the F1 labels show the height ("4m ↓5m"). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="0.5", ClampMax="10.0", Units="m"))
    float VerticalThresholdMeters = 2.0f;
    /** Same-category sounds closer than this are one marker (repeating emitters, a walker's steps). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Halo", meta=(ClampMin="0.0", ClampMax="20.0", Units="m"))
    float MergeRadiusMeters = 3.0f;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
        const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    /** A merged marker over time, so a repeating emitter waves once when it starts, not on every playback. */
    struct FMarkerTrack
    {
        FName Category;
        FVector Location;
        double OnsetTime = 0.0;
        double LastSeen = 0.0;
    };

    UPROPERTY() TMap<FName, FSlateBrush> Icons;
    UPROPERTY(Transient) TArray<FSoundCompassSourceFrame> Frames;
    /** Seconds since each frame's marker appeared (parallel to Frames). */
    TArray<float> OnsetAges;
    TArray<FMarkerTrack> Tracks;
    float CurrentPitch = 30.0f;
};
