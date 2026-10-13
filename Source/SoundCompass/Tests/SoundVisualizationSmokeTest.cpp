#if WITH_DEV_AUTOMATION_TESTS

#include "SoundVisualization/SoundVisualizationSubsystem.h"
#include "SoundVisualization/SoundVisualizationWidget.h"
#include "SoundCompass.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/AudioComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "TimerManager.h"

static void FinishSoundCompassSmokeTest(UWorld* World, ESoundCompassDisplayMode OriginalMode)
{
    World->GetSubsystem<USoundVisualizationSubsystem>()->DisplayMode = OriginalMode;
    if (FParse::Param(FCommandLine::Get(), TEXT("SoundCompassExitAfterSmokeTest")))
    {
        FPlatformMisc::RequestExit(false);
    }
    else if (auto* Player = World->GetFirstPlayerController())
    {
        Player->SetIgnoreLookInput(false);
        Player->SetIgnoreMoveInput(false);
    }
}

/** Exercise a real short auto-destroying voice, with unrelated map audio still playing. */
static void TestSoundCompassRetention(UWorld* World, ESoundCompassDisplayMode OriginalMode)
{
    auto* System = World->GetSubsystem<USoundVisualizationSubsystem>();
    FVector Location, Front, Right;
    World->GetFirstPlayerController()->GetAudioListenerPosition(Location, Front, Right);
    auto* Sound = LoadObject<USoundWave>(nullptr,
        TEXT("/Game/SoundTest/Audio/OneShots/Footsteps/SW_Footstep_Concrete_1.SW_Footstep_Concrete_1"));
    UAudioComponent* Audio = System->PlayVisualizedSound(Sound, Location + Right * 150.0,
        TEXT("FootstepRetentionTest"), nullptr, 0.5f, 1.0f, nullptr, 1.0f);
    if (!Audio || !Sound)
    {
        UE_LOG(LogSoundCompass, Error, TEXT("SoundCompass retention test: FAIL fixture missing"));
        FinishSoundCompassSmokeTest(World, OriginalMode);
        return;
    }
    const int32 Id = System->RegisterAudioComponent(Audio, TEXT("FootstepRetentionTest"), nullptr, 1.0f);
    const TWeakObjectPtr<UWorld> WeakWorld(World);
    const TWeakObjectPtr<UAudioComponent> WeakAudio(Audio);
    const float Duration = Sound->Duration;
    const float Hold = System->MarkerHoldSeconds;
    const float Fade = System->MarkerFadeSeconds;
    const auto HoldPassed = MakeShared<bool>(false);
    const auto FadePassed = MakeShared<bool>(false);
    auto Stage = [WeakWorld, WeakAudio, Id, HoldPassed, FadePassed](bool bHold)
    {
        auto* TestWorld = WeakWorld.Get();
        if (!TestWorld) { return; }
        const auto Frames = TestWorld->GetSubsystem<USoundVisualizationSubsystem>()->GetSourceFrames(
            TestWorld->GetFirstPlayerController());
        const auto* Frame = Frames.FindByPredicate([Id](const auto& Source) { return Source.SourceId == Id; });
        const bool Stopped = !WeakAudio.IsValid() || !WeakAudio->IsPlaying();
        const bool Pass = Stopped && Frame && !Frame->EnvelopeHistory.IsEmpty() &&
            (bHold ? Frame->VisualAlpha > 0.99f : Frame->VisualAlpha > 0.01f && Frame->VisualAlpha < 0.95f);
        *(bHold ? HoldPassed : FadePassed) = Pass;
        UE_LOG(LogSoundCompass, Display, TEXT("SoundCompass retention stage: %s %s stopped=%d alpha=%.3f samples=%d"),
            bHold ? TEXT("hold") : TEXT("fade"), Pass ? TEXT("PASS") : TEXT("FAIL"), Stopped,
            Frame ? Frame->VisualAlpha : 0.0f, Frame ? Frame->EnvelopeHistory.Num() : 0);
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() /
            (bHold ? TEXT("SoundCompassHold.png") : TEXT("SoundCompassFade.png")), true, false);
    };
    FTimerHandle HoldTimer, FadeTimer, EndTimer;
    World->GetTimerManager().SetTimer(HoldTimer, FTimerDelegate::CreateLambda([Stage]() { Stage(true); }),
        Duration + Hold * 0.4f, false);
    World->GetTimerManager().SetTimer(FadeTimer, FTimerDelegate::CreateLambda([Stage]() { Stage(false); }),
        Duration + Hold + Fade * 0.5f, false);
    World->GetTimerManager().SetTimer(EndTimer, FTimerDelegate::CreateLambda(
        [WeakWorld, Id, HoldPassed, FadePassed, OriginalMode]()
    {
        auto* TestWorld = WeakWorld.Get();
        if (!TestWorld) { return; }
        const auto Frames = TestWorld->GetSubsystem<USoundVisualizationSubsystem>()->GetSourceFrames(
            TestWorld->GetFirstPlayerController());
        const bool Removed = !Frames.ContainsByPredicate([Id](const auto& Source) { return Source.SourceId == Id; });
        UE_LOG(LogSoundCompass, Display, TEXT("SoundCompass retention test: %s hold=%d fade=%d removed=%d"),
            *HoldPassed && *FadePassed && Removed ? TEXT("PASS") : TEXT("FAIL"), *HoldPassed, *FadePassed, Removed);
        FinishSoundCompassSmokeTest(TestWorld, OriginalMode);
    }), Duration + Hold + Fade + 0.3f, false);
}

static void RunSoundCompassSmokeTest(UWorld* World)
{
    if (!World || !World->HasBegunPlay())
    {
        UE_LOG(LogSoundCompass, Error, TEXT("SoundCompass smoke test: game world required."));
        return;
    }
    USoundVisualizationSubsystem* System = World->GetSubsystem<USoundVisualizationSubsystem>();
    APlayerController* Player = World->GetFirstPlayerController();
    if (!System || !Player) { return; }
    const auto OriginalMode = System->DisplayMode;
    System->DisplayMode = ESoundCompassDisplayMode::ImportantOnly;
    FVector Location, Front, Right;
    Player->GetAudioListenerPosition(Location, Front, Right);
    FVector Direction(-Front.Y, Front.X, 0.0);
    Direction.Normalize();
    USoundBase* Sound = LoadObject<USoundBase>(nullptr,
        TEXT("/Game/SoundTest/Audio/Loops/SW_Loop_Noise_PoliceSiren.SW_Loop_Noise_PoliceSiren"));
    UAudioComponent* Audio = System->PlayVisualizedSound(Sound, Location + Direction * 150.0,
        TEXT("GunshotSmokeTest"), nullptr, 0.1f, 1.0f, nullptr, 1.0f);
    TArray<TWeakObjectPtr<UAudioComponent>> Fixtures;
    Fixtures.Add(Audio);
    Fixtures.Add(System->PlayVisualizedSound(Sound, Location - Direction * 250.0,
        TEXT("ExplosionSmokeTest"), nullptr, 0.1f, 1.0f, nullptr, 0.95f));
    Fixtures.Add(System->PlayVisualizedSound(Sound, Location + FVector(Front.X, Front.Y, 0).GetSafeNormal() * 250.0,
        TEXT("FootstepSmokeTest"), nullptr, 0.1f, 1.0f, nullptr, 0.94f));
    Fixtures.Add(System->PlayVisualizedSound(Sound, Location - FVector(Front.X, Front.Y, 0).GetSafeNormal() * 250.0,
        TEXT("DoorSmokeTest"), nullptr, 0.1f, 1.0f, nullptr, 0.70f));
    Fixtures.Add(System->PlayVisualizedSound(Sound, Location + Direction * 100.0,
        TEXT("AmbienceSmokeTest"), nullptr, 0.7f, 1.0f, nullptr, 0.10f));
    UE_LOG(LogSoundCompass, Display, TEXT("SoundCompass fixture placement: listener=%s front=%s expected=%s actual=%s"),
        *Location.ToString(), *Front.ToString(), *(Location + Direction * 150.0).ToString(),
        Audio ? *Audio->GetComponentLocation().ToString() : TEXT("null"));
    const TWeakObjectPtr<UWorld> WeakWorld(World);
    FTimerHandle Timer;
    World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateLambda([WeakWorld, Fixtures, OriginalMode]()
    {
        UWorld* TestWorld = WeakWorld.Get();
        if (!TestWorld) { return; }
        auto* TestSystem = TestWorld->GetSubsystem<USoundVisualizationSubsystem>();
        const auto Signal = TestSystem->GetSignalFrame();
        const auto Sources = TestSystem->GetSourceFrames(TestWorld->GetFirstPlayerController());
        const auto* Controlled = Sources.FindByPredicate([](const auto& Source)
        {
            return Source.Category == TEXT("GunshotSmokeTest");
        });
        const auto Selected = TestSystem->SelectSourceFrames(Sources);
        const auto* LowPriority = Sources.FindByPredicate([](const auto& Source)
        {
            return Source.Category == TEXT("AmbienceSmokeTest");
        });
        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(TestWorld, Widgets,
            USoundVisualizationWidget::StaticClass(), true);
        const bool AudioPass = Signal.bReceivingAudio && Signal.Rms > 0.00001f &&
            Signal.Waveform.Num() == 256 && Signal.SpectrumDb.Num() == 32 &&
            Signal.FrequenciesHz.Num() == 32 && Signal.SampleRate > 0 && FMath::IsFinite(Signal.RmsDb);
        const bool DirectionPass = Controlled &&
            FMath::Abs(Controlled->AzimuthDegrees - 90.0f) < 2.0f &&
            FMath::Abs(Controlled->DistanceMeters - 1.5f) < 0.05f;
        auto* Hud = Widgets.Num() > 0 ? Cast<USoundVisualizationWidget>(Widgets[0]) : nullptr;
        const bool HudPass = Hud && Hud->IsVisible();
        const bool SelectionPass = Controlled && LowPriority && Sources.Num() > Selected.Num() &&
            Selected.Num() == FMath::Clamp(TestSystem->MaxImportantSources, 1, 8) &&
            Selected.ContainsByPredicate([](const auto& Source) { return Source.Category == TEXT("GunshotSmokeTest"); }) &&
            !Selected.ContainsByPredicate([](const auto& Source) { return Source.Category == TEXT("AmbienceSmokeTest"); }) &&
            LowPriority->SourceEnvelope > Controlled->SourceEnvelope &&
            LowPriority->ImportanceScore < Controlled->ImportanceScore;
        TestSystem->ToggleDisplayMode();
        const bool BaselinePass = TestSystem->SelectSourceFrames(Sources).Num() == Sources.Num();
        TestSystem->ToggleDisplayMode();
        FVector2D Projected;
        auto* TestPlayer = TestWorld->GetFirstPlayerController();
        const bool CenterPass = Hud && TestPlayer->GetPawn() &&
            UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(TestPlayer,
                TestPlayer->GetPawn()->GetActorLocation(), Projected, true) &&
            FVector2D::Distance(Hud->GetRingCenter(), Projected) < 2.0;
        bool DiagnosticsPass = false;
        if (Hud)
        {
            Hud->ToggleDiagnostics();
            DiagnosticsPass = Hud->bShowSignal && Hud->bShowSourceList;
            Hud->ToggleDiagnostics();
            DiagnosticsPass &= !Hud->bShowSignal && !Hud->bShowSourceList;
        }
        const bool RulesPass = TestSystem->CategoryImportance.Num() == 8 &&
            TestSystem->CategoryImportance.FindRef(TEXT("Gunshot")) > TestSystem->CategoryImportance.FindRef(TEXT("Ambience")) &&
            TestSystem->CategoryImportance.FindRef(TEXT("Footstep")) > TestSystem->CategoryImportance.FindRef(TEXT("Ambience"));
        bool IconPass = false;
        if (Hud)
        {
            int32 ReadableIcons = 0;
            TArray<FLinearColor> Colors;
            if (auto* Canvas = Cast<UCanvasPanel>(Hud->WidgetTree->RootWidget))
            {
                for (auto* Child : Canvas->GetAllChildren())
                {
                    auto* Marker = Cast<UUserWidget>(Child);
                    auto* Icon = Marker ? Cast<UImage>(Marker->GetWidgetFromName(TEXT("Image_Icon"))) : nullptr;
                    auto* Badge = Marker ? Cast<UImage>(Marker->GetWidgetFromName(TEXT("SoundCompassCategoryBadge"))) : nullptr;
                    auto* Slot = Icon ? Cast<UCanvasPanelSlot>(Icon->Slot) : nullptr;
                    if (Slot && Badge && Badge->GetBrush().OutlineSettings.bUseBrushTransparency &&
                        Slot->GetSize().X >= 52.0f && Marker->GetRenderTransform().Scale.X >= 0.99f)
                    {
                        ++ReadableIcons;
                        Colors.AddUnique(Badge->GetColorAndOpacity());
                    }
                }
            }
            IconPass = ReadableIcons >= 3 && Colors.Num() >= 3 && Hud->WaveformArcDegrees == 50.0f;
        }
        const bool Pass = AudioPass && DirectionPass && HudPass && SelectionPass && BaselinePass && CenterPass && DiagnosticsPass && RulesPass && IconPass;
        UE_LOG(LogSoundCompass, Display, TEXT("SoundCompass smoke test: %s audio=%d direction=%d hud=%d priority=%d baseline=%d characterCenter=%d diagnostics=%d categoryRules=%d readableIcons=%d candidates=%d selected=%d RMS=%.5f rate=%d channels=%d"),
            Pass ? TEXT("PASS") : TEXT("FAIL"), AudioPass, DirectionPass, HudPass, SelectionPass, BaselinePass,
            CenterPass, DiagnosticsPass, RulesPass, IconPass, Sources.Num(), Selected.Num(), Signal.Rms, Signal.SampleRate, Signal.Channels);
        if (Controlled && LowPriority)
        {
            UE_LOG(LogSoundCompass, Display, TEXT("SoundCompass priority check: important=%.3f envelope=%.4f; ambience=%.3f envelope=%.4f"),
                Controlled->ImportanceScore, Controlled->SourceEnvelope, LowPriority->ImportanceScore, LowPriority->SourceEnvelope);
        }
        if (Controlled)
        {
            FVector ListenerLocation, ListenerFront, ListenerRight;
            TestWorld->GetFirstPlayerController()->GetAudioListenerPosition(ListenerLocation, ListenerFront, ListenerRight);
            UE_LOG(LogSoundCompass, Display, TEXT("SoundCompass controlled source: angle=%.2f distance=%.3fm"),
                Controlled->AzimuthDegrees, Controlled->DistanceMeters);
            UE_LOG(LogSoundCompass, Display, TEXT("SoundCompass fixture check: listener=%s front=%s source=%s"),
                *ListenerLocation.ToString(), *ListenerFront.ToString(), *Controlled->WorldLocation.ToString());
        }
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("SoundCompassSmokeTest.png"), true, false);
        // Capture the fixtures before restoring the map's original state.
        FTimerHandle Cleanup;
        TestWorld->GetTimerManager().SetTimer(Cleanup, FTimerDelegate::CreateLambda([WeakWorld, Fixtures, OriginalMode]()
        {
            for (const auto& Fixture : Fixtures) { if (auto* TestAudio = Fixture.Get()) { TestAudio->Stop(); } }
            if (auto* CleanupWorld = WeakWorld.Get())
            {
                TestSoundCompassRetention(CleanupWorld, OriginalMode);
            }
        }), 3.0f, false);
    }), 4.0f, false);
}

// Wait for possession and the initial camera update before placing the fixture.
static FAutoConsoleCommandWithWorld SoundCompassSmokeTest(
    TEXT("SoundCompass.SmokeTest"), TEXT("Tests audio, direction, importance selection, baseline, character-centred ring and HUD; writes a screenshot to Saved."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if (!World) { return; }
        if (APlayerController* Player = World->GetFirstPlayerController())
        {
            Player->SetIgnoreLookInput(true);
            Player->SetIgnoreMoveInput(true);
        }
        const TWeakObjectPtr<UWorld> WeakWorld(World);
        FTimerHandle StartTimer;
        World->GetTimerManager().SetTimer(StartTimer, FTimerDelegate::CreateLambda([WeakWorld]()
        {
            if (UWorld* TestWorld = WeakWorld.Get()) { RunSoundCompassSmokeTest(TestWorld); }
        }), 2.0f, false);
    }));

#endif
