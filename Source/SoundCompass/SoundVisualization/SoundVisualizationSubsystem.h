#pragma once

#include "CoreMinimal.h"
#include "AudioDeviceHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "SoundVisualizationSubsystem.generated.h"

class UAudioComponent;
class USoundBase;
class USoundAttenuation;
class USoundSubmix;
class APlayerController;
class FSoundCompassBufferListener;

UENUM(BlueprintType)
enum class ESoundCompassDisplayMode : uint8
{
    ImportantOnly UMETA(DisplayName="Important sounds only"),
    AllAudible UMETA(DisplayName="All audible sounds (comparison)")
};

/** One playback instance. One-shots retain their emission location. */
USTRUCT(BlueprintType)
struct FSoundCompassSourceFrame
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Sound Compass") int32 SourceId = 0;
    UPROPERTY(BlueprintReadOnly, Category="Sound Compass") FName Category;
    UPROPERTY(BlueprintReadOnly, Category="Sound Compass") FVector WorldLocation = FVector::ZeroVector;
    /** 0 = front, +90 = right, -90 = left, +/-180 = behind the audio listener. */
    UPROPERTY(BlueprintReadOnly, Category="Sound Compass") float AzimuthDegrees = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Sound Compass") float DistanceMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Sound Compass") float ElevationMeters = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Sound Compass") float SourceEnvelope = 0.0f;
    /** Estimate using the measured envelope, distance curve, and an occlusion trace. */
    UPROPERTY(BlueprintReadOnly, Category="Sound Compass") float EstimatedListenerLevel = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Sound Compass") float EstimatedListenerDb = -80.0f;
    /** Author-supplied semantic importance, independent of playback volume. */
    UPROPERTY(BlueprintReadOnly, Category="Sound Compass") float BaseImportance = 0.0f;
    /** Normalised project policy score: semantic importance, proximity, and loudness. */
    UPROPERTY(BlueprintReadOnly, Category="Sound Compass") float ImportanceScore = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Sound Compass") bool bOccluded = false;
    UPROPERTY(BlueprintReadOnly, Category="Sound Compass") bool bPlaying = false;
};

/** Measured PCM from the main output submix, after spatialisation and attenuation. */
USTRUCT(BlueprintType)
struct FSoundCompassSignalFrame
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Sound Compass") bool bReceivingAudio = false;
    UPROPERTY(BlueprintReadOnly, Category="Sound Compass") int32 SampleRate = 0;
    UPROPERTY(BlueprintReadOnly, Category="Sound Compass") int32 Channels = 0;
    UPROPERTY(BlueprintReadOnly, Category="Sound Compass") float Rms = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Sound Compass") float Peak = 0.0f;
    /** Digital dBFS, not a measurement of physical sound pressure (dB SPL). */
    UPROPERTY(BlueprintReadOnly, Category="Sound Compass") float RmsDb = -80.0f;
    UPROPERTY(BlueprintReadOnly, Category="Sound Compass") float DominantFrequencyHz = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="Sound Compass") TArray<float> Waveform;
    UPROPERTY(BlueprintReadOnly, Category="Sound Compass") TArray<float> SpectrumDb;
    UPROPERTY(BlueprintReadOnly, Category="Sound Compass") TArray<float> FrequenciesHz;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSoundCompassVisualizationUpdated);

/** One subsystem per game world; no editor-preview or dedicated-server capture. */
UCLASS(Config=Game)
class SOUNDCOMPASS_API USoundVisualizationSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    USoundVisualizationSubsystem();
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual void Deinitialize() override;

    /** Bind BEFORE calling Play so Unreal enables live envelope callbacks. */
    UFUNCTION(BlueprintCallable, Category="Sound Compass")
    int32 RegisterAudioComponent(UAudioComponent* Audio, FName Category, AActor* SourceOwner = nullptr,
        float ImportanceOverride = -1.0f);

    UFUNCTION(BlueprintCallable, Category="Sound Compass")
    void UnregisterAudioComponent(UAudioComponent* Audio);

    /** Creates a spatial one-shot, binds analysis, then starts it exactly once. */
    UFUNCTION(BlueprintCallable, Category="Sound Compass", meta=(AdvancedDisplay="Volume,Pitch,SourceOwner,ImportanceOverride"))
    UAudioComponent* PlayVisualizedSound(USoundBase* Sound, FVector Location, FName Category,
        USoundAttenuation* Attenuation = nullptr, float Volume = 1.0f, float Pitch = 1.0f,
        AActor* SourceOwner = nullptr, float ImportanceOverride = -1.0f);

    /** All audible candidates, ordered by importance. No Top-K filtering here. */
    UFUNCTION(BlueprintPure, Category="Sound Compass")
    TArray<FSoundCompassSourceFrame> GetSourceFrames(APlayerController* Listener) const;

    UFUNCTION(BlueprintPure, Category="Sound Compass")
    TArray<FSoundCompassSourceFrame> SelectSourceFrames(const TArray<FSoundCompassSourceFrame>& Candidates) const;

    UFUNCTION(BlueprintPure, Category="Sound Compass")
    TArray<FSoundCompassSourceFrame> GetVisualizationFrames(APlayerController* Listener) const;

    UFUNCTION(BlueprintCallable, Category="Sound Compass")
    void ToggleDisplayMode();

    UFUNCTION(BlueprintPure, Category="Sound Compass")
    FSoundCompassSignalFrame GetSignalFrame() const { return SignalFrame; }

    UPROPERTY(BlueprintAssignable, Category="Sound Compass")
    FSoundCompassVisualizationUpdated OnVisualizationUpdated;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Sound Compass")
    float SourceThresholdDb = -60.0f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Sound Compass", meta=(ClampMin="0.0"))
    float MarkerHoldSeconds = 0.35f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Sound Compass", meta=(ClampMin="1", ClampMax="512"))
    int32 MaxTrackedSources = 128;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Sound Compass|Importance")
    ESoundCompassDisplayMode DisplayMode = ESoundCompassDisplayMode::ImportantOnly;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Sound Compass|Importance", meta=(ClampMin="1", ClampMax="8"))
    int32 MaxImportantSources = 3;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Sound Compass|Importance", meta=(ClampMin="0.0", ClampMax="1.0"))
    float ImportanceThreshold = 0.50f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Sound Compass|Importance")
    TMap<FName, float> CategoryImportance;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Sound Compass|Importance", meta=(ClampMin="0.0", ClampMax="1.0"))
    float SemanticWeight = 0.70f;
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Sound Compass|Importance", meta=(ClampMin="0.0", ClampMax="1.0"))
    float ProximityWeight = 0.20f;
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Sound Compass|Importance", meta=(ClampMin="0.0", ClampMax="1.0"))
    float LoudnessWeight = 0.10f;
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Sound Compass|Importance", meta=(ClampMin="0.01", Units="m"))
    float ProximityRangeMeters = 20.0f;

private:
    struct FTrackedSource
    {
        int32 Id = 0;
        TWeakObjectPtr<UAudioComponent> Audio;
        TWeakObjectPtr<AActor> Owner;
        FName Category;
        float ImportanceOverride = -1.0f;
        FTransform Transform;
        float Envelope = 0.0f;
        double LastEnvelopeTime = 0.0;
        double StoppedTime = -1.0;
        bool bReceivedEnvelope = false;
        bool bEverPlayed = false;
        bool bPlaying = false;
        bool bHasAttenuation = false;
        TSharedPtr<struct FSoundAttenuationSettings> Attenuation;
    };

    void HandleEnvelope(const UAudioComponent* Audio, const class USoundWave* Wave, float Value);
    void StartCapture();
    void UpdateSignal(float DeltaTime);
    void RemoveSource(int32 Index);

    TArray<FTrackedSource> Sources;
    int32 NextSourceId = 1;
    float UpdateAccumulator = 0.0f;
    float AmbientDiscoveryTime = 1.0f;
    float BufferAge = 0.0f;
    uint64 LastBufferRevision = 0;
    FAudioDeviceHandle CaptureDevice;
    TSharedPtr<FSoundCompassBufferListener, ESPMode::ThreadSafe> BufferListener;

    UPROPERTY(Transient) TObjectPtr<USoundSubmix> CaptureSubmix;
    UPROPERTY(Transient) FSoundCompassSignalFrame SignalFrame;
};
