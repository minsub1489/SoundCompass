// Copyright Epic Games, Inc. All Rights Reserved.

#include "SoundTest/SoundTestEmitter.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundAttenuation.h"
#include "Animation/AnimationAsset.h"
#include "Engine/World.h"
#include "TimerManager.h"

ASoundTestEmitter::ASoundTestEmitter()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// mannequin meshes face +Y, rotate them so the actor's forward vector is the facing direction
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);

	Prop = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Prop"));
	Prop->SetupAttachment(Root);
	Prop->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Audio = CreateDefaultSubobject<UAudioComponent>(TEXT("Audio"));
	Audio->SetupAttachment(Root);
	Audio->SetRelativeLocation(FVector(0.0f, 0.0f, SoundHeight));
	Audio->bAutoActivate = false;

	MoveAreaPreview = CreateDefaultSubobject<UBoxComponent>(TEXT("MoveAreaPreview"));
	MoveAreaPreview->SetupAttachment(Root);
	MoveAreaPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MoveAreaPreview->SetHiddenInGame(true);
	MoveAreaPreview->ShapeColor = FColor(255, 170, 0);
	MoveAreaPreview->bIsEditorOnly = true;
}

void ASoundTestEmitter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	Audio->SetRelativeLocation(FVector(0.0f, 0.0f, SoundHeight));

	const bool bUsesArea = Movement == ESoundTestMovement::Teleport || Movement == ESoundTestMovement::Wander;
	MoveAreaPreview->SetVisibility(bUsesArea);
	MoveAreaPreview->SetBoxExtent(FVector(MoveAreaExtent.X, MoveAreaExtent.Y, 10.0f));
	MoveAreaPreview->SetWorldLocationAndRotation(GetActorLocation() + FVector(MoveAreaOffset.X, MoveAreaOffset.Y, 0.0f), FRotator::ZeroRotator);

	// preview the idle pose in the editor
	if (IdleAnimation && Body->GetSkeletalMeshAsset())
	{
		Body->OverrideAnimationData(IdleAnimation, true, true, 0.0f, 1.0f);
	}
}

void ASoundTestEmitter::BeginPlay()
{
	Super::BeginPlay();

	AreaCenter = GetActorLocation() + FVector(MoveAreaOffset.X, MoveAreaOffset.Y, 0.0f);

	Audio->AttenuationSettings = Attenuation;
	Audio->SetVolumeMultiplier(Volume);

	if (LoopSound)
	{
		float StartTime = 0.0f;
		if (bRandomLoopStart)
		{
			if (const USoundWave* Wave = Cast<USoundWave>(LoopSound))
			{
				StartTime = FMath::FRandRange(0.0f, Wave->Duration);
			}
		}

		Audio->SetSound(LoopSound);
		Audio->Play(StartTime);
		OnSoundEmitted.Broadcast(this, LoopSound, GetSoundLocation());
	}

	if (OneShotSounds.Num() > 0)
	{
		GetWorldTimerManager().SetTimer(OneShotTimer, this, &ASoundTestEmitter::PlayOneShot, FMath::FRandRange(0.5f, OneShotInterval.Y), false);
	}

	if (Movement == ESoundTestMovement::Teleport && !(bTeleportBeforeOneShot && OneShotSounds.Num() > 0))
	{
		ScheduleTeleport();
	}

	PauseTimeLeft = FMath::FRandRange(WanderPause.X, WanderPause.Y);
	SetWalking(false);
}

void ASoundTestEmitter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(OneShotTimer);
	GetWorldTimerManager().ClearTimer(TeleportTimer);

	Super::EndPlay(EndPlayReason);
}

void ASoundTestEmitter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Movement == ESoundTestMovement::Wander || Movement == ESoundTestMovement::Follow)
	{
		TickWalk(DeltaSeconds);
		TickFootsteps(DeltaSeconds);
	}
}

void ASoundTestEmitter::TickFootsteps(float DeltaSeconds)
{
	if (!bWalking || FootstepSounds.Num() == 0)
	{
		// the first step lands shortly after starting to walk
		NextStepTime = 0.15f;
		return;
	}

	NextStepTime -= DeltaSeconds;
	if (NextStepTime > 0.0f)
	{
		return;
	}

	NextStepTime += StrideLength / FMath::Max(WalkSpeed, 1.0f);

	if (USoundBase* Step = FootstepSounds[FMath::RandRange(0, FootstepSounds.Num() - 1)])
	{
		const FVector Feet = GetActorLocation();
		UGameplayStatics::SpawnSoundAtLocation(this, Step, Feet, FRotator::ZeroRotator, Volume, FMath::FRandRange(0.95f, 1.05f), 0.0f, Attenuation);
		OnSoundEmitted.Broadcast(this, Step, Feet);
	}
}

FVector ASoundTestEmitter::GetSoundLocation() const
{
	return Audio->GetComponentLocation();
}

void ASoundTestEmitter::TeleportToRandomPoint()
{
	const FVector Current = GetActorLocation();
	FVector NewLocation = RandomPointInArea();

	// retry a few times to get a jump that is clearly noticeable
	for (int32 Try = 0; Try < 8 && FVector::Dist2D(NewLocation, Current) < MinTeleportDistance; ++Try)
	{
		NewLocation = RandomPointInArea();
	}

	SetActorLocationAndRotation(NewLocation, FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f));
	OnTeleported.Broadcast(this, NewLocation);
}

void ASoundTestEmitter::PlayOneShot()
{
	if (Movement == ESoundTestMovement::Teleport && bTeleportBeforeOneShot)
	{
		TeleportToRandomPoint();
	}

	if (USoundBase* Sound = OneShotSounds[FMath::RandRange(0, OneShotSounds.Num() - 1)])
	{
		// spawned at a fixed location so the tail stays where the sound happened, even if the emitter moves on
		UGameplayStatics::SpawnSoundAtLocation(this, Sound, GetSoundLocation(), FRotator::ZeroRotator, Volume, 1.0f, 0.0f, Attenuation);
		OnSoundEmitted.Broadcast(this, Sound, GetSoundLocation());
	}

	GetWorldTimerManager().SetTimer(OneShotTimer, this, &ASoundTestEmitter::PlayOneShot, FMath::FRandRange(OneShotInterval.X, OneShotInterval.Y), false);
}

void ASoundTestEmitter::ScheduleTeleport()
{
	GetWorldTimerManager().SetTimer(TeleportTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		TeleportToRandomPoint();
		ScheduleTeleport();
	}), FMath::FRandRange(TeleportInterval.X, TeleportInterval.Y), false);
}

void ASoundTestEmitter::TickWalk(float DeltaSeconds)
{
	const FVector Location = GetActorLocation();

	if (Movement == ESoundTestMovement::Follow)
	{
		if (!FollowTarget)
		{
			return;
		}

		FVector FromTarget = Location - FollowTarget->GetActorLocation();
		FromTarget.Z = 0.0f;
		const float Distance = FromTarget.Size();

		if (Distance > FollowDistance + 60.0f)
		{
			WalkTarget = FollowTarget->GetActorLocation() + FromTarget.GetSafeNormal() * FollowDistance;
			WalkTarget.Z = Location.Z;
			SetWalking(true);
		}
		else if (Distance <= FollowDistance + 10.0f)
		{
			SetWalking(false);
		}

		if (!bWalking)
		{
			// face the partner while standing
			const FRotator Facing(0.0f, (-FromTarget).Rotation().Yaw, 0.0f);
			SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), Facing, DeltaSeconds, TurnSpeed));
			return;
		}
	}
	else if (!bWalking)
	{
		PauseTimeLeft -= DeltaSeconds;
		if (PauseTimeLeft > 0.0f)
		{
			return;
		}

		WalkTarget = RandomPointInArea();
		for (int32 Try = 0; Try < 8 && FVector::Dist2D(WalkTarget, Location) < 150.0f; ++Try)
		{
			WalkTarget = RandomPointInArea();
		}
		SetWalking(true);
	}

	FVector ToTarget = WalkTarget - Location;
	ToTarget.Z = 0.0f;
	const float Remaining = ToTarget.Size();

	if (Remaining < 10.0f)
	{
		SetWalking(false);
		PauseTimeLeft = FMath::FRandRange(WanderPause.X, WanderPause.Y);
		return;
	}

	const FVector Direction = ToTarget / Remaining;
	SetActorLocation(Location + Direction * FMath::Min(WalkSpeed * DeltaSeconds, Remaining));

	const FRotator Facing(0.0f, Direction.Rotation().Yaw, 0.0f);
	SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), Facing, DeltaSeconds, TurnSpeed));
}

void ASoundTestEmitter::SetWalking(bool bNewWalking)
{
	if (bAnimStateSet && bNewWalking == bWalking)
	{
		return;
	}

	bWalking = bNewWalking;
	bAnimStateSet = true;

	UAnimationAsset* Anim = bWalking ? WalkAnimation : IdleAnimation;
	if (Anim && Body->GetSkeletalMeshAsset())
	{
		Body->PlayAnimation(Anim, true);
	}
}

FVector ASoundTestEmitter::RandomPointInArea() const
{
	return AreaCenter + FVector(FMath::FRandRange(-MoveAreaExtent.X, MoveAreaExtent.X), FMath::FRandRange(-MoveAreaExtent.Y, MoveAreaExtent.Y), 0.0f);
}
