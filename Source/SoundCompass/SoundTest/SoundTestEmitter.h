// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoundTestEmitter.generated.h"

class UAudioComponent;
class UBoxComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class USoundBase;
class USoundAttenuation;
class UAnimationAsset;

/** How a sound test emitter changes its position during play */
UENUM(BlueprintType)
enum class ESoundTestMovement : uint8
{
	/** Stays where it was placed */
	Static,

	/** Jumps to a random point inside the move area */
	Teleport,

	/** Walks between random points inside the move area */
	Wander,

	/** Walks after FollowTarget and stays FollowDistance away from it */
	Follow
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSoundTestEmittedSignature, ASoundTestEmitter*, Emitter, USoundBase*, Sound, FVector, Location);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSoundTestTeleportedSignature, ASoundTestEmitter*, Emitter, FVector, NewLocation);

/**
 *  Sound source for the sound test map.
 *  Plays a looping sound and/or random one-shot sounds, and moves around inside a box around its placed location.
 *  An optional skeletal mesh (character) or static mesh (prop) gives the source a visible body.
 */
UCLASS()
class ASoundTestEmitter : public AActor
{
	GENERATED_BODY()

	/** Root of the emitter; the actor's forward vector is the facing direction */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Root;

	/** Optional character body */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> Body;

	/** Optional prop body */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Prop;

	/** Plays LoopSound */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAudioComponent> Audio;

	/** Editor-only preview of the area the emitter moves in */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> MoveAreaPreview;

public:

	/** Constructor */
	ASoundTestEmitter();

	/** Label of the sound type, e.g. Gunshot, Crying, Conversation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Sound")
	FName SoundCategory;

	/** Sound played continuously while the emitter is active. Should be a looping sound */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Sound")
	TObjectPtr<USoundBase> LoopSound;

	/** If true, the loop starts at a random position so several emitters with the same loop don't play in unison */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Sound")
	bool bRandomLoopStart = true;

	/** Sounds picked at random and played at the emitter's location every OneShotInterval seconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Sound")
	TArray<TObjectPtr<USoundBase>> OneShotSounds;

	/** Min / max seconds between one-shot sounds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Sound")
	FVector2D OneShotInterval = FVector2D(3.0, 6.0);

	/** Step sounds picked at random and played at the feet while the emitter walks (Wander / Follow) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Sound")
	TArray<TObjectPtr<USoundBase>> FootstepSounds;

	/** Distance covered per step; with WalkSpeed this sets the step rate */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Sound", meta = (ClampMin = 10, Units="cm"))
	float StrideLength = 75.0f;

	/** Attenuation used by the loop and the one-shot sounds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Sound")
	TObjectPtr<USoundAttenuation> Attenuation;

	/** Volume multiplier for all sounds of this emitter */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Sound", meta = (ClampMin = 0))
	float Volume = 1.0f;

	/** Height of the sound above the actor origin (mouth / muzzle height) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Sound", meta = (Units="cm"))
	float SoundHeight = 150.0f;

	/** How the emitter moves during play */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Movement")
	ESoundTestMovement Movement = ESoundTestMovement::Static;

	/** Half size of the move area, centered on the placed location + MoveAreaOffset. Z is ignored */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Movement")
	FVector MoveAreaExtent = FVector(600.0, 600.0, 0.0);

	/** Offset of the move area center from the placed location, in world axes. Z is ignored */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Movement")
	FVector MoveAreaOffset = FVector::ZeroVector;

	/** Min / max seconds between teleports (used when the teleport is not tied to one-shots) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Movement|Teleport")
	FVector2D TeleportInterval = FVector2D(4.0, 8.0);

	/** If true and there are one-shot sounds, the emitter teleports right before each one-shot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Movement|Teleport")
	bool bTeleportBeforeOneShot = true;

	/** A teleport moves at least this far, so each jump is noticeable */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Movement|Teleport", meta = (Units="cm"))
	float MinTeleportDistance = 300.0f;

	/** Walking speed for Wander and Follow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Movement|Walk", meta = (Units="cm/s"))
	float WalkSpeed = 150.0f;

	/** Min / max seconds to stand still between walks */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Movement|Walk")
	FVector2D WanderPause = FVector2D(1.0, 3.0);

	/** Turning speed while walking */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Movement|Walk", meta = (Units="deg/s"))
	float TurnSpeed = 360.0f;

	/** Animation looped on the body while standing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Movement|Walk")
	TObjectPtr<UAnimationAsset> IdleAnimation;

	/** Animation looped on the body while walking */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Movement|Walk")
	TObjectPtr<UAnimationAsset> WalkAnimation;

	/** Actor to walk after in Follow mode */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Movement|Follow")
	TObjectPtr<AActor> FollowTarget;

	/** Distance kept from FollowTarget */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Movement|Follow", meta = (Units="cm"))
	float FollowDistance = 160.0f;

	/** Called whenever a one-shot sound starts (and once when the loop sound starts) */
	UPROPERTY(BlueprintAssignable, Category="Sound Test")
	FSoundTestEmittedSignature OnSoundEmitted;

	/** Called after every teleport */
	UPROPERTY(BlueprintAssignable, Category="Sound Test")
	FSoundTestTeleportedSignature OnTeleported;

	/** World location the sounds come from */
	UFUNCTION(BlueprintPure, Category="Sound Test")
	FVector GetSoundLocation() const;

	/** Moves the emitter to a random point inside the move area */
	UFUNCTION(BlueprintCallable, Category="Sound Test")
	void TeleportToRandomPoint();

protected:

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Plays a random one-shot sound and schedules the next one */
	void PlayOneShot();

	/** Schedules the next teleport */
	void ScheduleTeleport();

	/** Wander / Follow movement */
	void TickWalk(float DeltaSeconds);

	/** Plays footsteps while walking */
	void TickFootsteps(float DeltaSeconds);

	/** Switches the body animation when the walking state changes */
	void SetWalking(bool bNewWalking);

	/** Random point in the move area */
	FVector RandomPointInArea() const;

	/** Center of the move area (the placed location) */
	FVector AreaCenter;

	/** Current wander destination */
	FVector WalkTarget;

	/** Seconds left to stand still before the next walk */
	float PauseTimeLeft = 0.0f;

	/** True while the body is walking */
	bool bWalking = false;

	/** True once a walk animation state was applied */
	bool bAnimStateSet = false;

	/** Seconds until the next footstep */
	float NextStepTime = 0.0f;

	FTimerHandle OneShotTimer;
	FTimerHandle TeleportTimer;
};
