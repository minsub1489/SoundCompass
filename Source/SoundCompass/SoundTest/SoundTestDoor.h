// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoundTestDoor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USoundBase;
class USoundAttenuation;

/** State of a sound test door */
UENUM(BlueprintType)
enum class ESoundTestDoorState : uint8
{
	Closed,
	Opening,
	Open,
	Closing
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSoundTestDoorStateSignature, ASoundTestDoor*, Door, ESoundTestDoorState, NewState);

/**
 *  Swing door for the sound test map that opens and closes by itself on a timer.
 *  The panel blocks sound (occlusion traces) while closed. An invisible blocker in the doorway
 *  stops pawns at all times but lets sound through, so nobody can enter even while the door is open.
 *  The actor origin is the hinge; the closed panel extends along the actor's +X axis.
 */
UCLASS()
class ASoundTestDoor : public AActor
{
	GENERATED_BODY()

	/** Root at the hinge */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Root;

	/** Rotates around the hinge */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Hinge;

	/** Door panel */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Panel;

	/** Invisible doorway blocker: blocks pawns only */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> Blocker;

public:

	/** Constructor */
	ASoundTestDoor();

	/** Size of the door panel */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test Door", meta = (Units="cm"))
	FVector DoorSize = FVector(240.0, 10.0, 300.0);

	/** Thickness of the wall the door sits in; the blocker covers this depth */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test Door", meta = (Units="cm"))
	float WallThickness = 100.0f;

	/** Swing angle when fully open. Negative values swing to the other side */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test Door", meta = (Units="deg"))
	float OpenAngle = 100.0f;

	/** Seconds the door takes to open or close */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test Door", meta = (ClampMin = 0.1, Units="s"))
	float MoveTime = 1.2f;

	/** Min / max seconds the door stays closed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test Door")
	FVector2D ClosedTime = FVector2D(5.0, 8.0);

	/** Min / max seconds the door stays open */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test Door")
	FVector2D OpenTime = FVector2D(5.0, 8.0);

	/** Played when the door starts to open */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test Door")
	TObjectPtr<USoundBase> OpenSound;

	/** Played when the door shuts */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test Door")
	TObjectPtr<USoundBase> CloseSound;

	/** Attenuation for the door sounds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test Door")
	TObjectPtr<USoundAttenuation> Attenuation;

	/** Called whenever the door changes state */
	UPROPERTY(BlueprintAssignable, Category="Sound Test Door")
	FSoundTestDoorStateSignature OnDoorStateChanged;

	/** Current state */
	UFUNCTION(BlueprintPure, Category="Sound Test Door")
	ESoundTestDoorState GetDoorState() const { return State; }

	/** 0 = closed, 1 = fully open */
	UFUNCTION(BlueprintPure, Category="Sound Test Door")
	float GetOpenAmount() const { return OpenAmount; }

protected:

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Switches state, plays the matching sound and notifies listeners */
	void SetState(ESoundTestDoorState NewState);

	/** Plays a door sound at the door's center */
	void PlayDoorSound(USoundBase* Sound) const;

	ESoundTestDoorState State = ESoundTestDoorState::Closed;

	/** Seconds spent in the current state */
	float StateTime = 0.0f;

	/** Seconds to stay in the current Closed / Open state */
	float StateDuration = 0.0f;

	float OpenAmount = 0.0f;
};
