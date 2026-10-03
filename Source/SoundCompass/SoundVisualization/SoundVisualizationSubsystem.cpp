#include "SoundVisualization/SoundVisualizationSubsystem.h"
#include "SoundVisualization/SoundSignalAnalysis.h"
#include "SoundVisualization/SoundImportancePolicy.h"
#include "SoundVisualization/SoundCategory.h"
#include "SoundCompass.h"
#include "AudioDevice.h"
#include "ActiveSound.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "ISubmixBufferListener.h"
#include "Misc/ScopeRWLock.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundSubmix.h"
#include "Sound/AmbientSound.h"

/** Owns only PCM, never game objects; mixer commands keep its shared reference alive. */
class FSoundCompassBufferListener final : public ISubmixBufferListener
{
public:
    virtual const FString& GetListenerName() const override { return Name; }

    virtual void OnNewSubmixBuffer(const USoundSubmix*, float* AudioData, int32 NumSamples,
        int32 NumChannels, const int32 InSampleRate, double) override
    {
        if (!AudioData || NumChannels < 1 || NumChannels > 8 || InSampleRate < 1000) { return; }
        FWriteScopeLock Guard(Lock);
        if (Channels != NumChannels || SampleRate != InSampleRate)
        {
            Channels = NumChannels;
            SampleRate = InSampleRate;
            WriteFrame = 0;
            ValidFrames = 0;
        }
        const int32 Frames = NumSamples / Channels;
        for (int32 Frame = 0; Frame < Frames; ++Frame)
        {
            for (int32 Channel = 0; Channel < Channels; ++Channel)
            {
                Ring[WriteFrame * Channels + Channel] = AudioData[Frame * Channels + Channel];
            }
            WriteFrame = (WriteFrame + 1) % SoundCompassSignal::WindowSize;
            ValidFrames = FMath::Min(ValidFrames + 1, SoundCompassSignal::WindowSize);
        }
        ++Revision;
    }

    bool Snapshot(TArray<float>& Samples, int32& OutChannels, int32& OutRate, uint64& OutRevision)
    {
        FReadScopeLock Guard(Lock);
        OutRevision = Revision;
        if (ValidFrames != SoundCompassSignal::WindowSize) { return false; }
        OutChannels = Channels;
        OutRate = SampleRate;
        Samples.SetNumUninitialized(SoundCompassSignal::WindowSize * Channels);
        for (int32 Frame = 0; Frame < SoundCompassSignal::WindowSize; ++Frame)
        {
            const int32 ReadFrame = (WriteFrame + Frame) % SoundCompassSignal::WindowSize;
            FMemory::Memcpy(Samples.GetData() + Frame * Channels, Ring.data() + ReadFrame * Channels,
                sizeof(float) * Channels);
        }
        return true;
    }

private:
    FString Name = TEXT("SoundCompassOutput");
    FRWLock Lock;
    std::array<float, SoundCompassSignal::WindowSize * 8> Ring{};
    int32 Channels = 0;
    int32 SampleRate = 0;
    int32 WriteFrame = 0;
    int32 ValidFrames = 0;
    uint64 Revision = 0;
};

USoundVisualizationSubsystem::USoundVisualizationSubsystem()
{
    CategoryImportance = {
        {TEXT("Explosion"), 1.0f}, {TEXT("Gunshot"), 0.95f}, {TEXT("Alert"), 0.90f},
        {TEXT("Footstep"), 0.75f}, {TEXT("Conversation"), 0.70f}, {TEXT("Door"), 0.55f},
        {TEXT("Noise"), 0.35f}, {TEXT("Ambience"), 0.10f}
    };
}

bool USoundVisualizationSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId USoundVisualizationSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(USoundVisualizationSubsystem, STATGROUP_Tickables);
}

void USoundVisualizationSubsystem::StartCapture()
{
    if (BufferListener || !GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer) { return; }
    CaptureDevice = GetWorld()->GetAudioDevice();
    if (!CaptureDevice.IsValid() || !CaptureDevice->bAudioMixerModuleLoaded) { return; }
    CaptureSubmix = &CaptureDevice->GetMainSubmixObject();
    BufferListener = MakeShared<FSoundCompassBufferListener, ESPMode::ThreadSafe>();
    CaptureDevice->RegisterSubmixBufferListener(BufferListener.ToSharedRef(), *CaptureSubmix);
}

int32 USoundVisualizationSubsystem::RegisterAudioComponent(UAudioComponent* Audio, FName Category,
    AActor* SourceOwner, float ImportanceOverride)
{
    if (!IsValid(Audio) || Audio->GetWorld() != GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer)
    {
        return INDEX_NONE;
    }
    for (FTrackedSource& Source : Sources)
    {
        if (Source.Audio == Audio)
        {
            Source.Category = Category;
            if (FMath::IsFinite(ImportanceOverride) && ImportanceOverride >= 0.0f)
            {
                Source.ImportanceOverride = FMath::Clamp(ImportanceOverride, 0.0f, 1.0f);
            }
            return Source.Id;
        }
    }
    // Bound memory, while giving finished instances priority for eviction.
    const int32 Limit = FMath::Clamp(MaxTrackedSources, 1, 512);
    if (Sources.Num() >= Limit)
    {
        const int32 Finished = Sources.IndexOfByPredicate([](const FTrackedSource& Source)
        {
            const UAudioComponent* Existing = Source.Audio.Get();
            return !Existing || (Source.bEverPlayed && !Existing->IsPlaying());
        });
        if (Finished == INDEX_NONE) { return INDEX_NONE; }
        RemoveSource(Finished);
    }
    FTrackedSource& Source = Sources.AddDefaulted_GetRef();
    Source.Id = NextSourceId++;
    Source.Audio = Audio;
    Source.Owner = SourceOwner ? SourceOwner : Audio->GetOwner();
    Source.Category = Category.IsNone() ? FName(TEXT("Sound")) : Category;
    Source.ImportanceOverride = FMath::IsFinite(ImportanceOverride) ? ImportanceOverride : -1.0f;
    Source.Transform = Audio->GetComponentTransform();
    Source.LastEnvelopeTime = GetWorld()->GetTimeSeconds();
    if (const FSoundAttenuationSettings* Settings = Audio->GetAttenuationSettingsToApply())
    {
        Source.Attenuation = MakeShared<FSoundAttenuationSettings>(*Settings);
        Source.bHasAttenuation = true;
    }
    Audio->OnAudioSingleEnvelopeValueNative.AddUObject(this, &USoundVisualizationSubsystem::HandleEnvelope);
    // AmbientSound can already be playing when a streamed actor is discovered.
    // Enable updates on the active voice without restarting or duplicating playback.
    if (FAudioDevice* Device = Audio->GetAudioDevice())
    {
        Device->SendCommandToActiveSounds(Audio->GetAudioComponentID(), [](FActiveSound& ActiveSound)
        {
            ActiveSound.bUpdateSingleEnvelopeValue = true;
        });
    }
    return Source.Id;
}

void USoundVisualizationSubsystem::RemoveSource(int32 Index)
{
    if (UAudioComponent* Audio = Sources[Index].Audio.Get())
    {
        Audio->OnAudioSingleEnvelopeValueNative.RemoveAll(this);
    }
    Sources.RemoveAtSwap(Index);
}

void USoundVisualizationSubsystem::UnregisterAudioComponent(UAudioComponent* Audio)
{
    for (int32 Index = Sources.Num() - 1; Index >= 0; --Index)
    {
        if (Sources[Index].Audio == Audio) { RemoveSource(Index); }
    }
}

UAudioComponent* USoundVisualizationSubsystem::PlayVisualizedSound(USoundBase* Sound, FVector Location,
    FName Category, USoundAttenuation* Attenuation, float Volume, float Pitch, AActor* SourceOwner,
    float ImportanceOverride)
{
    if (!Sound || !GetWorld()->bAllowAudioPlayback || GetWorld()->GetNetMode() == NM_DedicatedServer)
    {
        return nullptr;
    }
    FAudioDevice::FCreateComponentParams Params(GetWorld());
    Params.SetLocation(Location);
    Params.AttenuationSettings = Attenuation;
    Params.bPlay = false;
    UAudioComponent* Audio = FAudioDevice::CreateComponent(Sound, Params);
    if (Audio)
    {
        Audio->bAutoDestroy = true;
        Audio->bStopWhenOwnerDestroyed = false;
        Audio->SetVolumeMultiplier(FMath::Max(0.0f, Volume));
        Audio->SetPitchMultiplier(FMath::Max(0.01f, Pitch));
        Audio->EnvelopeFollowerAttackTime = 10;
        Audio->EnvelopeFollowerReleaseTime = 80;
        RegisterAudioComponent(Audio, Category, SourceOwner, ImportanceOverride);
        Audio->Play();
    }
    return Audio;
}

void USoundVisualizationSubsystem::HandleEnvelope(const UAudioComponent* Audio, const USoundWave*, float Value)
{
    // Unreal dispatches this native delegate on the game thread.
    for (FTrackedSource& Source : Sources)
    {
        if (Source.Audio == Audio)
        {
            Source.Envelope = FMath::IsFinite(Value) ? FMath::Max(0.0f, Value) : 0.0f;
            Source.LastEnvelopeTime = GetWorld()->GetTimeSeconds();
            Source.bReceivedEnvelope = true;
            Source.bEverPlayed = true;
            return;
        }
    }
}

void USoundVisualizationSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!GetWorld()->HasBegunPlay() || GetWorld()->GetNetMode() == NM_DedicatedServer) { return; }
    AmbientDiscoveryTime += DeltaTime;
    if (AmbientDiscoveryTime >= 1.0f)
    {
        AmbientDiscoveryTime = 0.0f;
        for (TActorIterator<AAmbientSound> It(GetWorld()); It; ++It)
        {
            UAudioComponent* Audio = It->GetAudioComponent();
            // Preserve explicit metadata supplied by Blueprint/UI integration.
            if (Audio && !Sources.ContainsByPredicate([Audio](const FTrackedSource& Source)
                { return Source.Audio == Audio; }))
            {
                RegisterAudioComponent(Audio, TEXT("Ambience"), *It);
            }
        }
    }
    const double Now = GetWorld()->GetTimeSeconds();
    for (int32 Index = Sources.Num() - 1; Index >= 0; --Index)
    {
        FTrackedSource& Source = Sources[Index];
        UAudioComponent* Audio = Source.Audio.Get();
        if (Audio)
        {
            Source.Transform = Audio->GetComponentTransform();
            Source.bPlaying = Audio->IsPlaying() && !Audio->bIsVirtualized &&
                Audio->GetPlayState() != EAudioComponentPlayState::Paused;
            Source.bEverPlayed |= Audio->IsPlaying();
        }
        else { Source.bPlaying = false; }
        // Paused/virtualised loops may become audible again; retain their registration.
        if (Audio && Audio->IsPlaying()) { Source.StoppedTime = -1.0; }
        else if (Source.StoppedTime < 0.0) { Source.StoppedTime = Now; }
        if (Source.StoppedTime >= 0.0 && Now - Source.StoppedTime > FMath::Max(0.0f, MarkerHoldSeconds))
        {
            RemoveSource(Index);
        }
    }
    UpdateAccumulator += DeltaTime;
    if (UpdateAccumulator < 1.0f / 30.0f) { return; }
    const float Elapsed = UpdateAccumulator;
    UpdateAccumulator = 0.0f;
    StartCapture();
    UpdateSignal(Elapsed);
    OnVisualizationUpdated.Broadcast();
}

void USoundVisualizationSubsystem::UpdateSignal(float DeltaTime)
{
    BufferAge += DeltaTime;
    TArray<float> Samples;
    int32 Channels = 0;
    int32 Rate = 0;
    uint64 Revision = 0;
    if (BufferListener && BufferListener->Snapshot(Samples, Channels, Rate, Revision) && Revision != LastBufferRevision)
    {
        LastBufferRevision = Revision;
        BufferAge = 0.0f;
        const SoundCompassSignal::Analysis Analysis = SoundCompassSignal::Analyze(
            Samples.GetData(), SoundCompassSignal::WindowSize, Channels, Rate);
        SignalFrame.bReceivingAudio = Analysis.Valid;
        SignalFrame.SampleRate = Rate;
        SignalFrame.Channels = Channels;
        SignalFrame.Rms = Analysis.Rms;
        SignalFrame.Peak = Analysis.Peak;
        SignalFrame.RmsDb = Analysis.RmsDb;
        SignalFrame.DominantFrequencyHz = Analysis.DominantFrequencyHz;
        SignalFrame.Waveform.Reset(SoundCompassSignal::WaveformSize);
        SignalFrame.Waveform.Append(Analysis.Waveform.data(), SoundCompassSignal::WaveformSize);
        SignalFrame.SpectrumDb.Reset(SoundCompassSignal::BandCount);
        SignalFrame.SpectrumDb.Append(Analysis.SpectrumDb.data(), SoundCompassSignal::BandCount);
        SignalFrame.FrequenciesHz.Reset(SoundCompassSignal::BandCount);
        SignalFrame.FrequenciesHz.Append(Analysis.FrequenciesHz.data(), SoundCompassSignal::BandCount);
    }
    else if (BufferAge > 0.25f) { SignalFrame = FSoundCompassSignalFrame(); }
}

TArray<FSoundCompassSourceFrame> USoundVisualizationSubsystem::GetSourceFrames(APlayerController* Listener) const
{
    TArray<FSoundCompassSourceFrame> Frames;
    if (!IsValid(Listener) || Listener->GetWorld() != GetWorld() || !Listener->IsLocalController()) { return Frames; }
    FVector Location, Front, Right;
    Listener->GetAudioListenerPosition(Location, Front, Right);
    // Keep the compass horizontal even when looking upwards or downwards.
    FVector HorizontalFront = FVector(Front.X, Front.Y, 0.0).GetSafeNormal();
    if (HorizontalFront.IsNearlyZero())
    {
        HorizontalFront = FVector(Right.Y, -Right.X, 0.0).GetSafeNormal();
    }
    const FVector HorizontalRight(-HorizontalFront.Y, HorizontalFront.X, 0.0);
    const double Now = GetWorld()->GetTimeSeconds();
    for (const FTrackedSource& Source : Sources)
    {
        if (!Source.bReceivedEnvelope) { continue; }
        if (const UAudioComponent* Audio = Source.Audio.Get())
        {
            if (Audio->bIsVirtualized || Audio->GetPlayState() == EAudioComponentPlayState::Paused) { continue; }
        }
        const double Age = FMath::Max(0.0, Now - Source.LastEnvelopeTime);
        // Age out a stale envelope: no invented amplitude for silent or virtualised sources.
        const float Envelope = Source.Envelope * FMath::Exp(-FMath::Max(0.0, Age - 0.08) * 12.0);
        if (SoundCompassSignal::Decibels(Envelope) < SourceThresholdDb) { continue; }
        FSoundCompassSourceFrame Frame;
        Frame.SourceId = Source.Id;
        Frame.Category = Source.Category;
        Frame.WorldLocation = Source.Transform.GetLocation();
        const FVector Offset = Frame.WorldLocation - Location;
        const auto Bearing = SoundCompassSignal::CalculateBearing(
            FVector::DotProduct(Offset, HorizontalFront), FVector::DotProduct(Offset, HorizontalRight), Offset.Z);
        Frame.AzimuthDegrees = Bearing.Degrees;
        Frame.DistanceMeters = Bearing.DistanceMeters;
        Frame.ElevationMeters = Bearing.ElevationMeters;
        Frame.SourceEnvelope = Envelope;
        float Gain = 1.0f;
        if (Source.bHasAttenuation)
        {
            const FSoundAttenuationSettings& Settings = *Source.Attenuation;
            if (Settings.bAttenuate) { Gain = Settings.Evaluate(Source.Transform, Location, 1.0f); }
            if (Settings.bEnableOcclusion && Gain > UE_SMALL_NUMBER)
            {
                FCollisionQueryParams Query(SCENE_QUERY_STAT(SoundCompassOcclusion), Settings.bUseComplexCollisionForOcclusion);
                if (Source.Owner.IsValid()) { Query.AddIgnoredActor(Source.Owner.Get()); }
                if (Listener->GetPawn()) { Query.AddIgnoredActor(Listener->GetPawn()); }
                Frame.bOccluded = GetWorld()->LineTraceTestByChannel(Location, Frame.WorldLocation,
                    Settings.OcclusionTraceChannel, Query);
                if (Frame.bOccluded) { Gain *= Settings.OcclusionVolumeAttenuation; }
            }
        }
        Frame.EstimatedListenerLevel = FMath::Max(0.0f, Envelope * Gain);
        Frame.EstimatedListenerDb = SoundCompassSignal::Decibels(Frame.EstimatedListenerLevel);
        const float* CategoryValue = CategoryImportance.Find(Source.Category);
        if (!CategoryValue) { CategoryValue = CategoryImportance.Find(SoundCompassCategory(Source.Category)); }
        Frame.BaseImportance = Source.ImportanceOverride >= 0.0f ?
            SoundCompassImportance::Unit(Source.ImportanceOverride) :
            SoundCompassImportance::Unit(CategoryValue ? *CategoryValue : 0.35f);
        const SoundCompassImportance::Weights Weights{SemanticWeight, ProximityWeight, LoudnessWeight,
            ProximityRangeMeters, SourceThresholdDb};
        Frame.ImportanceScore = SoundCompassImportance::Score(Frame.BaseImportance,
            Frame.DistanceMeters, Frame.EstimatedListenerDb, Weights);
        Frame.bPlaying = Source.bPlaying;
        if (Frame.EstimatedListenerDb >= SourceThresholdDb) { Frames.Add(Frame); }
    }
    Frames.Sort([](const FSoundCompassSourceFrame& A, const FSoundCompassSourceFrame& B)
    {
        return A.ImportanceScore == B.ImportanceScore ? A.SourceId < B.SourceId :
            A.ImportanceScore > B.ImportanceScore;
    });
    return Frames;
}

TArray<FSoundCompassSourceFrame> USoundVisualizationSubsystem::SelectSourceFrames(
    const TArray<FSoundCompassSourceFrame>& Candidates) const
{
    std::vector<SoundCompassImportance::Candidate> Scores;
    Scores.reserve(Candidates.Num());
    for (const auto& Frame : Candidates) { Scores.push_back({Frame.SourceId, Frame.ImportanceScore}); }
    const bool bAll = DisplayMode == ESoundCompassDisplayMode::AllAudible;
    const auto Indices = SoundCompassImportance::Select(Scores, bAll ? 0.0f : ImportanceThreshold,
        bAll ? Candidates.Num() : FMath::Clamp(MaxImportantSources, 1, 8));
    TArray<FSoundCompassSourceFrame> Selected;
    Selected.Reserve(static_cast<int32>(Indices.size()));
    for (const int Index : Indices) { Selected.Add(Candidates[Index]); }
    return Selected;
}

TArray<FSoundCompassSourceFrame> USoundVisualizationSubsystem::GetVisualizationFrames(APlayerController* Listener) const
{
    return SelectSourceFrames(GetSourceFrames(Listener));
}

void USoundVisualizationSubsystem::ToggleDisplayMode()
{
    DisplayMode = DisplayMode == ESoundCompassDisplayMode::ImportantOnly ?
        ESoundCompassDisplayMode::AllAudible : ESoundCompassDisplayMode::ImportantOnly;
    OnVisualizationUpdated.Broadcast();
    UE_LOG(LogSoundCompass, Display, TEXT("SoundCompass condition: %s TopK=%d threshold=%.2f"),
        DisplayMode == ESoundCompassDisplayMode::ImportantOnly ? TEXT("ImportantOnly") : TEXT("AllAudible"),
        MaxImportantSources, ImportanceThreshold);
}

void USoundVisualizationSubsystem::Deinitialize()
{
    for (int32 Index = Sources.Num() - 1; Index >= 0; --Index) { RemoveSource(Index); }
    if (CaptureDevice.IsValid() && CaptureSubmix && BufferListener)
    {
        CaptureDevice->UnregisterSubmixBufferListener(BufferListener.ToSharedRef(), *CaptureSubmix);
    }
    BufferListener.Reset();
    CaptureSubmix = nullptr;
    CaptureDevice.Reset();
    SignalFrame = FSoundCompassSignalFrame();
    OnVisualizationUpdated.Clear();
    Super::Deinitialize();
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld DumpSoundCompass(
    TEXT("SoundCompass.Dump"), TEXT("Logs current measured output and sound direction data."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if (!World) { return; }
        if (USoundVisualizationSubsystem* System = World->GetSubsystem<USoundVisualizationSubsystem>())
        {
            const auto Signal = System->GetSignalFrame();
            const auto Frames = System->GetSourceFrames(World->GetFirstPlayerController());
            const auto Selected = System->SelectSourceFrames(Frames);
            UE_LOG(LogSoundCompass, Display, TEXT("SoundCompass output: ready=%d rate=%d channels=%d rms=%.5f dBFS=%.1f peak=%.4f dominant=%.1fHz waveform=%d bands=%d sources=%d"),
                Signal.bReceivingAudio, Signal.SampleRate, Signal.Channels, Signal.Rms, Signal.RmsDb,
                Signal.Peak, Signal.DominantFrequencyHz, Signal.Waveform.Num(), Signal.SpectrumDb.Num(), Frames.Num());
            UE_LOG(LogSoundCompass, Display, TEXT("SoundCompass selection: condition=%s candidates=%d visible=%d TopK=%d threshold=%.2f"),
                System->DisplayMode == ESoundCompassDisplayMode::ImportantOnly ? TEXT("ImportantOnly") : TEXT("AllAudible"),
                Frames.Num(), Selected.Num(), System->MaxImportantSources, System->ImportanceThreshold);
            for (const auto& Frame : Frames)
            {
                UE_LOG(LogSoundCompass, Display, TEXT("SoundCompass source: id=%d category=%s bearing=%.1f distance=%.2fm envelope=%.5f estimated=%.1fdB importance=%.3f base=%.2f occluded=%d"),
                    Frame.SourceId, *Frame.Category.ToString(), Frame.AzimuthDegrees, Frame.DistanceMeters,
                    Frame.SourceEnvelope, Frame.EstimatedListenerDb, Frame.ImportanceScore, Frame.BaseImportance, Frame.bOccluded);
            }
        }
    }));
#endif
