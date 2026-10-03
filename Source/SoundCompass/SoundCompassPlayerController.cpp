// Copyright Epic Games, Inc. All Rights Reserved.


#include "SoundCompassPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "SoundCompass.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "SoundVisualization/SoundVisualizationWidget.h"

void ASoundCompassPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalPlayerController() && bEnableSoundVisualization)
	{
		const TSubclassOf<USoundVisualizationWidget> WidgetClass = SoundVisualizationWidgetClass
			? SoundVisualizationWidgetClass.Get() : USoundVisualizationWidget::StaticClass();
		SoundVisualizationWidget = CreateWidget<USoundVisualizationWidget>(this, WidgetClass);
		if (SoundVisualizationWidget)
		{
			SoundVisualizationWidget->AddToPlayerScreen(10);
		}
	}

	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogSoundCompass, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void ASoundCompassPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::V, IE_Pressed, this, &ASoundCompassPlayerController::ToggleSoundVisualization);
	InputComponent->BindKey(EKeys::B, IE_Pressed, this, &ASoundCompassPlayerController::ToggleSoundDisplayMode);
	InputComponent->BindKey(EKeys::F1, IE_Pressed, this, &ASoundCompassPlayerController::ToggleSoundDiagnostics);

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
}

bool ASoundCompassPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void ASoundCompassPlayerController::ToggleSoundVisualization()
{
	if (SoundVisualizationWidget)
	{
		SoundVisualizationWidget->SetVisibility(SoundVisualizationWidget->IsVisible()
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void ASoundCompassPlayerController::ToggleSoundDisplayMode()
{
	if (USoundVisualizationSubsystem* System = GetWorld()->GetSubsystem<USoundVisualizationSubsystem>())
	{
		System->ToggleDisplayMode();
	}
}

void ASoundCompassPlayerController::ToggleSoundDiagnostics()
{
	if (SoundVisualizationWidget) { SoundVisualizationWidget->ToggleDiagnostics(); }
}
