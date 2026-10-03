// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SoundCompassPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class USoundVisualizationWidget;

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
 */
UCLASS(abstract)
class ASoundCompassPlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Automatically shows the character-centred sound visualization in the test map. */
	UPROPERTY(EditAnywhere, Category="Sound Compass")
	bool bEnableSoundVisualization = true;

	UPROPERTY(EditAnywhere, Category="Sound Compass")
	TSubclassOf<USoundVisualizationWidget> SoundVisualizationWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<USoundVisualizationWidget> SoundVisualizationWidget;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** V switches the visualization overlay on or off during an experiment. */
	void ToggleSoundVisualization();
	void ToggleSoundDisplayMode();
	void ToggleSoundDiagnostics();

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

};
