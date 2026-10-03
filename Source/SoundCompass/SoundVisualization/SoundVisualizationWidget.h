#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SoundVisualization/SoundVisualizationSubsystem.h"
#include "SoundVisualizationWidget.generated.h"

class UCanvasPanel;
class UImage;
class UTexture2D;
class UMaterialInterface;

/** Ready-to-run HUD using AHNINTAE's original marker Blueprint and icon assets. */
UCLASS()
class SOUNDCOMPASS_API USoundVisualizationWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    USoundVisualizationWidget(const FObjectInitializer& ObjectInitializer);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Compass") bool bShowCompass = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Compass") bool bShowSignal = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Compass") bool bShowSourceList = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Compass", meta=(ClampMin="80.0", ClampMax="400.0"))
    float RingRadius = 220.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sound Compass", meta=(ClampMin="0.0"))
    float WaveformGain = 12.0f;

    UFUNCTION(BlueprintCallable, Category="Sound Compass")
    void ToggleDiagnostics();

    UFUNCTION(BlueprintPure, Category="Sound Compass")
    FVector2D GetRingCenter() const { return RingCenter; }

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
        const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
    virtual void NativeDestruct() override;

private:
    UTexture2D* GetIcon(FName Category) const;
    static FLinearColor GetCategoryColor(FName Category);
    static FString GetCategoryLabel(FName Category);

    UPROPERTY(Transient) TObjectPtr<UCanvasPanel> RootCanvas;
    UPROPERTY(Transient) TObjectPtr<UImage> CenterRing;
    UPROPERTY() TSubclassOf<UUserWidget> MarkerClass;
    UPROPERTY() TObjectPtr<UMaterialInterface> RingMaterial;
    UPROPERTY() TMap<FName, TObjectPtr<UTexture2D>> Icons;
    UPROPERTY(Transient) TMap<int32, TObjectPtr<UUserWidget>> Markers;
    UPROPERTY(Transient) TArray<FSoundCompassSourceFrame> SourceFrames;
    UPROPERTY(Transient) TArray<FSoundCompassSourceFrame> CandidateFrames;
    UPROPERTY(Transient) FSoundCompassSignalFrame SignalFrame;
    FVector2D RingCenter = FVector2D::ZeroVector;
    ESoundCompassDisplayMode DisplayMode = ESoundCompassDisplayMode::ImportantOnly;
    float RefreshTime = 0.0f;
};
