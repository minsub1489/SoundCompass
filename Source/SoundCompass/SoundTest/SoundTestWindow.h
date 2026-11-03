// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SoundTestWindow.generated.h"

class UStaticMeshComponent;
class USoundBase;
class USoundAttenuation;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSoundTestWindowSignature, ASoundTestWindow*, Window, FVector, Location);

/**
 *  Glass pane for the sound test map that shatters on a timer and is restored after a while.
 *  While intact the pane blocks pawns and sound (occlusion traces); once broken the opening is free
 *  and a few glass shards fall to the ground.
 *  The actor origin is the bottom center of the pane; the pane spans the actor's X (width) and Z (height) axes.
 */
UCLASS()
class ASoundTestWindow : public AActor
{
	GENERATED_BODY()

	/** Root at the bottom center of the pane */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> Root;

	/** The glass pane */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Pane;

public:

	/** Constructor */
	ASoundTestWindow();

	/** Width (X), thickness (Y) and height (Z) of the pane */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test Window", meta = (Units="cm"))
	FVector PaneSize = FVector(300.0, 4.0, 200.0);

	/** Min / max seconds the pane stays intact */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test Window")
	FVector2D IntactTime = FVector2D(8.0, 16.0);

	/** Min / max seconds the window stays broken before it is restored */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test Window")
	FVector2D BrokenTime = FVector2D(5.0, 8.0);

	/** Picked at random when the pane shatters */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test Window")
	TArray<TObjectPtr<USoundBase>> BreakSounds;

	/** Attenuation for the break sound */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test Window")
	TObjectPtr<USoundAttenuation> Attenuation;

	/** Volume multiplier for the break sound */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test Window", meta = (ClampMin = 0))
	float Volume = 1.0f;

	/** Number of falling shards */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test Window", meta = (ClampMin = 0, ClampMax = 30))
	int32 ShardCount = 10;

	/** Side to throw the shards to: +1 along the actor's +Y, -1 along -Y */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound Test Window")
	float ShardDirection = 1.0f;

	/** Called when the pane shatters */
	UPROPERTY(BlueprintAssignable, Category="Sound Test Window")
	FSoundTestWindowSignature OnWindowBroken;

	/** Called when the pane is restored */
	UPROPERTY(BlueprintAssignable, Category="Sound Test Window")
	FSoundTestWindowSignature OnWindowRestored;

	/** Shatters the pane now */
	UFUNCTION(BlueprintCallable, Category="Sound Test Window")
	void Break();

	/** Restores the pane now */
	UFUNCTION(BlueprintCallable, Category="Sound Test Window")
	void Restore();

	UFUNCTION(BlueprintPure, Category="Sound Test Window")
	bool IsBroken() const { return bBroken; }

protected:

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Center of the pane in world space */
	FVector GetPaneCenter() const;

	void ClearShards();

	bool bBroken = false;

	FTimerHandle StateTimer;

	/** Shards spawned by the last break */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Shards;
};
