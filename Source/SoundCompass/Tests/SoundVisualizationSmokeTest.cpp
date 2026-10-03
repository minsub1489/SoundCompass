#if WITH_DEV_AUTOMATION_TESTS

#include "SoundVisualization/SoundVisualizationSubsystem.h"
#include "SoundVisualization/SoundVisualizationWidget.h"
#include "SoundCompass.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

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
        TEXT("SmokeTest"), nullptr, 0.1f, 1.0f, nullptr, 1.0f);
    TArray<TWeakObjectPtr<UAudioComponent>> Fixtures;
    Fixtures.Add(Audio);
    Fixtures.Add(System->PlayVisualizedSound(Sound, Location + FVector(0, -250, 0),
        TEXT("SmokeTest2"), nullptr, 0.1f, 1.0f, nullptr, 0.90f));
    Fixtures.Add(System->PlayVisualizedSound(Sound, Location + FVector(250, 0, 0),
        TEXT("SmokeTest3"), nullptr, 0.1f, 1.0f, nullptr, 0.80f));
    Fixtures.Add(System->PlayVisualizedSound(Sound, Location + FVector(-250, 0, 0),
        TEXT("SmokeTest4"), nullptr, 0.1f, 1.0f, nullptr, 0.70f));
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
            return Source.Category == TEXT("SmokeTest");
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
            Selected.ContainsByPredicate([](const auto& Source) { return Source.Category == TEXT("SmokeTest"); }) &&
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
        const bool Pass = AudioPass && DirectionPass && HudPass && SelectionPass && BaselinePass && CenterPass && DiagnosticsPass && RulesPass;
        UE_LOG(LogSoundCompass, Display, TEXT("SoundCompass smoke test: %s audio=%d direction=%d hud=%d priority=%d baseline=%d characterCenter=%d diagnostics=%d categoryRules=%d candidates=%d selected=%d RMS=%.5f rate=%d channels=%d"),
            Pass ? TEXT("PASS") : TEXT("FAIL"), AudioPass, DirectionPass, HudPass, SelectionPass, BaselinePass,
            CenterPass, DiagnosticsPass, RulesPass, Sources.Num(), Selected.Num(), Signal.Rms, Signal.SampleRate, Signal.Channels);
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
                CleanupWorld->GetSubsystem<USoundVisualizationSubsystem>()->DisplayMode = OriginalMode;
                if (FParse::Param(FCommandLine::Get(), TEXT("SoundCompassExitAfterSmokeTest")))
                {
                    FPlatformMisc::RequestExit(false);
                }
                else if (auto* CleanupPlayer = CleanupWorld->GetFirstPlayerController())
                {
                    CleanupPlayer->SetIgnoreLookInput(false);
                    CleanupPlayer->SetIgnoreMoveInput(false);
                }
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
