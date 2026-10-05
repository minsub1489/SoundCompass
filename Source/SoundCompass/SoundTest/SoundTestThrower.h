// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoundTestThrower.generated.h"

class UAudioComponent;
class UBoxComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;
class USoundBase;
class USoundAttenuation;
class UAnimationAsset;

/** One kind of throwable (frag grenade, flashbang, smoke, molotov...) and the sounds of each phase */
USTRUCT(BlueprintType)
struct FSoundTestThrowable
{
	GENERATED_BODY()

	/** Display name, e.g. Frag, Flashbang */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throwable")
	FName Name;

	/** Category reported for the detonation sounds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throwable")
	FName DetonateCategory = TEXT("Explosion");

	/** Mesh flying through the air */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throwable")
	TObjectPtr<UStaticMesh> Mesh;

	/** Scale of the flying mesh */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throwable")
	FVector MeshScale = FVector(0.12);

	/** Played at the hand before the throw (pin pull, lighter...) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throwable")
	TObjectPtr<USoundBase> PrepareSound;

	/** Played at the hand on release */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throwable")
	TObjectPtr<USoundBase> ThrowSound;

	/** Picked at random each time the throwable hits the ground */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throwable")
	TArray<TObjectPtr<USoundBase>> BounceSounds;

	/** Seconds between landing and detonation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throwable", meta = (ClampMin = 0, Units="s"))
	float FuseTime = 1.5f;

	/** All played together at detonation; the first one is picked at random if bRandomDetonateSound */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throwable")
	TArray<TObjectPtr<USoundBase>> DetonateSounds;

	/** Play one random detonation sound instead of all of them */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throwable")
	bool bRandomDetonateSound = false;

	/** Sound kept running at the landing spot after detonation (smoke hiss, fire); stopped after LingerTime */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throwable")
	TObjectPtr<USoundBase> LingerSound;

	/** Category reported for the linger sound */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throwable")
	FName LingerCategory = TEXT("Noise");

	/** Seconds the linger sound runs */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throwable", meta = (ClampMin = 0, Units="s"))
	float LingerTime = 0.0f;

	/** Keep the mesh visible on the ground while the linger sound runs (smoke canister) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throwable")
	bool bKeepMeshWhileLingering = false;
};

/** Phases of a throw, reported by OnThrowPhase */
UENUM(BlueprintType)
enum class ESoundTestThrowPhase : uint8
{
	Prepare,
	Release,
	Bounce,
	Detonate,
	LingerEnd
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FSoundTestThrowPhaseSignature, ASoundTestThrower*, Thrower, ESoundTestThrowPhase, Phase, FName, ThrowableName, FVector, Location);

/**
 *  NPC that throws grenades, flashbangs, smoke or molotovs on a timer.
 *  The throwable flies on a visible arc to a random point inside the target area, bounces, and detonates
 *  after its fuse, so every phase of the throw is heard from a different place.
 */
UCLASS()
class ASoundTestThrower : public AActor
{
	GENERATED_BODY()

	/** Root at the thrower's feet; the actor's forward vector is the facing direction */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Root;

	/** Character body */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> Body;

	/** The throwable in flight / on the ground */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Projectile;

	/** Editor-only preview of the target area */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> TargetAreaPreview;

public:

	/** Constructor */
	ASoundTestThrower();

	/** Throwables used in turn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Throw")
	TArray<FSoundTestThrowable> Throwables;

	/** Min / max seconds between the end of one throw and the next */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Throw")
	FVector2D ThrowInterval = FVector2D(4.0, 8.0);

	/** Half size of the area the throwables land in. Z is ignored; landings are at the thrower's height */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Throw")
	FVector TargetAreaExtent = FVector(800.0, 800.0, 0.0);

	/** Offset of the target area center from the thrower, in world axes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Throw")
	FVector TargetAreaOffset = FVector(0.0, 1000.0, 0.0);

	/** Throws land at least this far from the thrower */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Throw", meta = (Units="cm"))
	float MinThrowDistance = 500.0f;

	/** Horizontal flight speed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Throw", meta = (Units="cm/s"))
	float ThrowSpeed = 1100.0f;

	/** Seconds between the prepare sound and the release */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Throw", meta = (Units="s"))
	float PrepareTime = 0.8f;

	/** Height of the hand above the actor origin */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Throw", meta = (Units="cm"))
	float HandHeight = 150.0f;

	/** Attenuation for every sound of the throw */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Throw")
	TObjectPtr<USoundAttenuation> Attenuation;

	/** Volume multiplier for every sound of the throw */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Throw", meta = (ClampMin = 0))
	float Volume = 1.0f;

	/** Animation looped while waiting */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Throw")
	TObjectPtr<UAnimationAsset> IdleAnimation;

	/** Animation played once for the throw */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test|Throw")
	TObjectPtr<UAnimationAsset> ThrowAnimation;

	/** Called for every phase of every throw */
	UPROPERTY(BlueprintAssignable, Category="Sound Test|Throw")
	FSoundTestThrowPhaseSignature OnThrowPhase;

protected:

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	enum class EState : uint8 { Waiting, Preparing, Flying, Rolling, Fusing, Lingering };

	/** Starts a new throw with the next throwable */
	void StartThrow();

	/** Detonation of the current throwable */
	void Detonate();

	/** Ends the throw and waits for the next one */
	void FinishThrow();

	/** Plays a sound through the visualization subsystem when available */
	UAudioComponent* PlaySoundAt(USoundBase* Sound, const FVector& Location, FName Category, float VolumeScale = 1.0f);

	void Report(ESoundTestThrowPhase Phase, const FVector& Location);

	const FSoundTestThrowable* Current() const;

	EState State = EState::Waiting;
	float StateTime = 0.0f;
	float StateDuration = 0.0f;

	int32 ThrowIndex = -1;
	FVector ThrowStart;
	FVector ThrowEnd;
	FVector RollEnd;
	float ArcHeight = 200.0f;

	/** Running linger sound */
	TWeakObjectPtr<UAudioComponent> LingerAudio;
};
