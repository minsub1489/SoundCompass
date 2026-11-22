#if WITH_DEV_AUTOMATION_TESTS

#include "SoundVisualization/SoundVisualizationSubsystem.h"
#include "SoundCompass.h"
#include "ActiveSound.h"
#include "AudioDevice.h"
#include "AudioThread.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

namespace
{
struct FVoiceSnapshot
{
    bool bOccluded = false;
    bool bHasSettings = false;
    float Gain = 1.0f;
    float LowPass = 20000.0f;
};

// Read the live audio-thread values used by the renderer, not just our UI trace.
void ReadVoice(UAudioComponent* Audio, TFunction<void(FVoiceSnapshot)> Callback)
{
    if (!Audio || !Audio->GetAudioDevice()) { return; }
    Audio->GetAudioDevice()->SendCommandToActiveSounds(Audio->GetAudioComponentID(),
        [Callback = MoveTemp(Callback)](FActiveSound& Voice) mutable
    {
        const FVoiceSnapshot Snapshot{Voice.bIsOccluded, !!Voice.bHasAttenuationSettings,
            Voice.CurrentOcclusionVolumeAttenuation.GetValue(), Voice.CurrentOcclusionFilterFrequency.GetValue()};
        FAudioThread::RunCommandOnGameThread([Callback = MoveTemp(Callback), Snapshot]() mutable
        {
            Callback(Snapshot);
        });
    });
}

struct FOcclusionTest : TSharedFromThis<FOcclusionTest>
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<APlayerController> Player;
    TWeakObjectPtr<AActor> Owner;
    TWeakObjectPtr<AActor> WallActor;
    TWeakObjectPtr<UBoxComponent> Wall;
    TWeakObjectPtr<UAudioComponent> EarlyAudio;
    TWeakObjectPtr<UAudioComponent> LateAudio;
    FTimerHandle Watchdog;
    bool bPassed = true;
    bool bFinished = false;

    void Check(bool bCondition, const TCHAR* Description)
    {
        bPassed &= bCondition;
        if (!bCondition) { UE_LOG(LogSoundCompass, Error, TEXT("SoundCompass occlusion check FAILED: %s"), Description); }
    }

    UAudioComponent* MakeAudio()
    {
        UAudioComponent* Audio = NewObject<UAudioComponent>(Owner.Get());
        Audio->bAutoActivate = false;
        Audio->RegisterComponentWithWorld(World.Get());
        return Audio;
    }

    void CheckSettings(USoundVisualizationSubsystem* System)
    {
        UAudioComponent* Audio = MakeAudio();
        FSoundAttenuationSettings Original;
        Original.FalloffDistance = 1234.0f;
        Original.bEnableOcclusion = false;
        Audio->AdjustAttenuation(Original);
        Audio->OcclusionCheckInterval = 0.7f;
        Audio->SetVolumeMultiplier(0.42f);
        const int32 Id = System->RegisterAudioComponent(Audio, TEXT("SettingsTest"));
        Check(Id != INDEX_NONE && Audio->AttenuationOverrides.bEnableOcclusion &&
            Audio->AttenuationOverrides.FalloffDistance == 1234.0f && Audio->VolumeMultiplier == 0.42f,
            TEXT("registration preserves distance falloff and volume"));
        Check(System->RegisterAudioComponent(Audio, TEXT("SettingsTest")) == Id,
            TEXT("duplicate registration keeps a single playback instance"));
        System->UnregisterAudioComponent(Audio);
        Check(Audio->bOverrideAttenuation && !Audio->AttenuationOverrides.bEnableOcclusion &&
            Audio->AttenuationOverrides.FalloffDistance == 1234.0f && Audio->OcclusionCheckInterval == 0.7f,
            TEXT("unregister restores authored settings and check interval"));

        Original.bEnableOcclusion = true;
        Original.OcclusionVolumeAttenuation = 0.12f;
        Original.OcclusionLowPassFilterFrequency = 700.0f;
        Audio->AdjustAttenuation(Original);
        System->RegisterAudioComponent(Audio, TEXT("AuthoredTest"));
        Check(Audio->AttenuationOverrides.OcclusionVolumeAttenuation == 0.12f &&
            Audio->AttenuationOverrides.OcclusionLowPassFilterFrequency == 700.0f &&
            Audio->OcclusionCheckInterval == 0.7f, TEXT("authored occlusion is preserved"));
        System->UnregisterAudioComponent(Audio);

        Original.bEnableOcclusion = false;
        Original.bSpatialize = false;
        Audio->AdjustAttenuation(Original);
        System->RegisterAudioComponent(Audio, TEXT("NonSpatialTest"));
        Check(!Audio->AttenuationOverrides.bEnableOcclusion, TEXT("non-spatial sources are untouched"));
        System->UnregisterAudioComponent(Audio);
        Audio->bOverrideAttenuation = false;
        Audio->bIsUISound = true;
        System->RegisterAudioComponent(Audio, TEXT("UITest"));
        Check(!Audio->bOverrideAttenuation, TEXT("UI sounds are untouched"));
        System->UnregisterAudioComponent(Audio);
        Audio->bIsUISound = false;
        const bool bEnabled = System->bEnableRaycastOcclusion;
        System->bEnableRaycastOcclusion = false;
        System->RegisterAudioComponent(Audio, TEXT("DisabledTest"));
        Check(!Audio->bOverrideAttenuation, TEXT("disabled defaults leave playback unchanged"));
        System->UnregisterAudioComponent(Audio);
        System->bEnableRaycastOcclusion = bEnabled;
        Audio->DestroyComponent();
    }

    UBoxComponent* MakeBox(AActor* Actor, FVector Location, FVector Extent)
    {
        UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
        Actor->SetRootComponent(Box);
        Box->SetBoxExtent(Extent);
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Box->SetCollisionResponseToAllChannels(ECR_Ignore);
        Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Box->RegisterComponent();
        Actor->SetActorLocation(Location);
        return Box;
    }

    void Begin(UWorld* TestWorld)
    {
        World = TestWorld;
        Player = TestWorld->GetFirstPlayerController();
        USoundVisualizationSubsystem* System = TestWorld->GetSubsystem<USoundVisualizationSubsystem>();
        Check(Player.IsValid() && System, TEXT("game world has a player and visualization subsystem"));
        if (!Player.IsValid() || !System) { Finish(); return; }
        Check(System->bEnableRaycastOcclusion && System->OcclusionTraceChannel == ECC_Visibility,
            TEXT("test requires default raycast configuration"));
        Player->SetIgnoreLookInput(true);
        Player->SetIgnoreMoveInput(true);
        FVector Location, Front, Right;
        Player->GetAudioListenerPosition(Location, Front, Right);
        FVector Direction;
        FCollisionQueryParams Query;
        Query.AddIgnoredActor(Player->GetPawn());
        for (const FVector Candidate : {Right, -Right, FVector::UpVector, Front, -Front})
        {
            if (!TestWorld->LineTraceTestByChannel(Location, Location + Candidate * 300.0, ECC_Visibility, Query))
            {
                Direction = Candidate.GetSafeNormal();
                break;
            }
        }
        Check(!Direction.IsNearlyZero(), TEXT("clear baseline ray available"));
        if (Direction.IsNearlyZero()) { Finish(); return; }
        Owner = TestWorld->SpawnActor<AActor>();
        // This collider must be ignored by both the native audio ray and the UI ray.
        MakeBox(Owner.Get(), Location + Direction * 300.0, FVector(30.0));
        CheckSettings(System);
        WallActor = TestWorld->SpawnActor<AActor>();
        Wall = MakeBox(WallActor.Get(), Location + Direction * 150.0, FVector(10, 100, 100));
        WallActor->SetActorRotation(Direction.Rotation());
        Wall->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
        USoundBase* Sound = LoadObject<USoundBase>(nullptr,
            TEXT("/Game/SoundTest/Audio/Loops/SW_Loop_Noise_PoliceSiren.SW_Loop_Noise_PoliceSiren"));
        EarlyAudio = System->PlayVisualizedSound(Sound, Location + Direction * 300.0,
            TEXT("OcclusionEarly"), nullptr, 0.2f, 1.0f, Owner.Get(), 1.0f);
        // Model ambient discovery: an unattenuated voice is already running at registration.
        FAudioDevice::FCreateComponentParams Params(TestWorld, Owner.Get());
        Params.SetLocation(Location + Direction * 300.0);
        Params.bPlay = false;
        LateAudio = FAudioDevice::CreateComponent(Sound, Params);
        if (LateAudio.IsValid())
        {
            LateAudio->SetVolumeMultiplier(0.2f);
            LateAudio->Play();
            System->RegisterAudioComponent(LateAudio.Get(), TEXT("OcclusionLate"), Owner.Get(), 1.0f);
        }
        Check(EarlyAudio.IsValid() && LateAudio.IsValid(), TEXT("live audio fixtures created"));
        if (!EarlyAudio.IsValid() || !LateAudio.IsValid()) { Finish(); return; }
        const auto Self = AsShared();
        TestWorld->GetTimerManager().SetTimer(Watchdog, FTimerDelegate::CreateLambda([Self]()
        {
            Self->Check(false, TEXT("audio-thread snapshot timed out"));
            Self->Finish();
        }), 12.0f, false);
        Schedule(0);
    }

    void Schedule(int32 Stage)
    {
        if (!World.IsValid() || bFinished) { return; }
        FTimerHandle Timer;
        const auto Self = AsShared();
        World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([Self, Stage]()
        {
            ReadVoice(Self->EarlyAudio.Get(), [Self, Stage](FVoiceSnapshot Early)
            {
                ReadVoice(Self->LateAudio.Get(), [Self, Stage, Early](FVoiceSnapshot Late)
                {
                    Self->Inspect(Stage, Early, Late);
                });
            });
        }), 0.8f, false);
    }

    void Inspect(int32 Stage, FVoiceSnapshot Early, FVoiceSnapshot Late)
    {
        if (!World.IsValid() || bFinished) { return; }
        auto* System = World->GetSubsystem<USoundVisualizationSubsystem>();
        const bool bBlocked = Stage == 1;
        const float Gain = bBlocked ? System->OccludedVolumeMultiplier : 1.0f;
        const float LowPass = bBlocked ? System->OccludedLowPassFrequencyHz : 20000.0f;
        const auto Frames = System->GetSourceFrames(Player.Get());
        const auto* Frame = Frames.FindByPredicate([](const auto& Source) { return Source.Category == TEXT("OcclusionEarly"); });
        const bool bNativePass = Early.bHasSettings && Late.bHasSettings &&
            Early.bOccluded == bBlocked && Late.bOccluded == bBlocked &&
            FMath::IsNearlyEqual(Early.Gain, Gain, 0.01f) && FMath::IsNearlyEqual(Late.Gain, Gain, 0.01f) &&
            FMath::IsNearlyEqual(Early.LowPass, LowPass, 5.0f) && FMath::IsNearlyEqual(Late.LowPass, LowPass, 5.0f);
        const bool bFramePass = Frame && Frame->bOccluded == bBlocked &&
            FMath::IsNearlyEqual(Frame->OcclusionGain, Gain, 0.001f) &&
            FMath::IsNearlyEqual(Frame->EstimatedListenerLevel, Frame->SourceEnvelope * Gain, 0.00001f);
        Check(bNativePass, TEXT("native renderer gain and filter follow the wall"));
        Check(bFramePass, TEXT("visualization applies occlusion exactly once"));
        const TCHAR* Names[] = {TEXT("clear"), TEXT("blocked"), TEXT("channel-ignore"), TEXT("removed")};
        UE_LOG(LogSoundCompass, Display, TEXT("SoundCompass occlusion stage: %s %s nativeGain=%.3f lateGain=%.3f lowpass=%.0fHz frameGain=%.3f"),
            Names[Stage], bNativePass && bFramePass ? TEXT("PASS") : TEXT("FAIL"), Early.Gain, Late.Gain, Early.LowPass,
            Frame ? Frame->OcclusionGain : -1.0f);
        if (Stage == 0) { Wall->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); }
        else if (Stage == 1) { Wall->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore); }
        else if (Stage == 2) { WallActor->Destroy(); }
        else { Finish(); return; }
        Schedule(Stage + 1);
    }

    void Finish()
    {
        if (bFinished) { return; }
        bFinished = true;
        if (UWorld* TestWorld = World.Get())
        {
            TestWorld->GetTimerManager().ClearTimer(Watchdog);
            auto* System = TestWorld->GetSubsystem<USoundVisualizationSubsystem>();
            for (const auto WeakAudio : {EarlyAudio, LateAudio})
            {
                if (auto* Audio = WeakAudio.Get())
                {
                    const bool bPlaying = Audio->IsPlaying();
                    System->UnregisterAudioComponent(Audio);
                    Check(!Audio->bOverrideAttenuation && Audio->VolumeMultiplier == 0.2f && Audio->IsPlaying() == bPlaying,
                        TEXT("live unregister restores defaults without volume changes or restart"));
                    Audio->Stop();
                    if (WeakAudio == LateAudio) { Audio->DestroyComponent(); }
                }
            }
            if (Owner.IsValid()) { Owner->Destroy(); }
            if (WallActor.IsValid()) { WallActor->Destroy(); }
        }
        if (Player.IsValid())
        {
            Player->SetIgnoreLookInput(false);
            Player->SetIgnoreMoveInput(false);
        }
        UE_LOG(LogSoundCompass, Display, TEXT("SoundCompass occlusion test: %s"), bPassed ? TEXT("PASS") : TEXT("FAIL"));
        if (FParse::Param(FCommandLine::Get(), TEXT("SoundCompassExitAfterOcclusionTest")))
        {
            FPlatformMisc::RequestExit(false);
        }
    }
};

FAutoConsoleCommandWithWorld SoundCompassOcclusionTest(TEXT("SoundCompass.OcclusionTest"),
    TEXT("Tests real raycast attenuation, filtering, source-owner exclusion, ambient registration and restoration."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if (!World || !World->HasBegunPlay()) { return; }
        const TWeakObjectPtr<UWorld> WeakWorld(World);
        FTimerHandle Timer;
        World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([WeakWorld]()
        {
            if (auto* TestWorld = WeakWorld.Get()) { MakeShared<FOcclusionTest>()->Begin(TestWorld); }
        }), 2.0f, false);
    }));
}
#endif
