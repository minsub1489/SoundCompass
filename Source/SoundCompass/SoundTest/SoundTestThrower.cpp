// Copyright Epic Games, Inc. All Rights Reserved.

#include "SoundTest/SoundTestThrower.h"
#include "SoundVisualization/SoundVisualizationSubsystem.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Animation/AnimationAsset.h"

ASoundTestThrower::ASoundTestThrower()
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

	// moved in world space while flying, so it does not follow the thrower
	Projectile = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Projectile"));
	Projectile->SetupAttachment(Root);
	Projectile->SetUsingAbsoluteLocation(true);
	Projectile->SetUsingAbsoluteRotation(true);
	Projectile->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Projectile->SetHiddenInGame(true);

	TargetAreaPreview = CreateDefaultSubobject<UBoxComponent>(TEXT("TargetAreaPreview"));
	TargetAreaPreview->SetupAttachment(Root);
	TargetAreaPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TargetAreaPreview->SetHiddenInGame(true);
	TargetAreaPreview->ShapeColor = FColor(255, 60, 60);
	TargetAreaPreview->bIsEditorOnly = true;
}

void ASoundTestThrower::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Editor-only components are stripped during standalone play and packaging.
	if (TargetAreaPreview)
	{
		TargetAreaPreview->SetBoxExtent(FVector(TargetAreaExtent.X, TargetAreaExtent.Y, 10.0f));
		TargetAreaPreview->SetWorldLocationAndRotation(GetActorLocation() + FVector(TargetAreaOffset.X, TargetAreaOffset.Y, 0.0f), FRotator::ZeroRotator);
	}

	if (IdleAnimation && Body->GetSkeletalMeshAsset())
	{
		Body->OverrideAnimationData(IdleAnimation, true, true, 0.0f, 1.0f);
	}
}

void ASoundTestThrower::BeginPlay()
{
	Super::BeginPlay();

	State = EState::Waiting;
	StateTime = 0.0f;
	StateDuration = FMath::FRandRange(1.0f, ThrowInterval.Y);
	Projectile->SetHiddenInGame(true);
}

void ASoundTestThrower::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (LingerAudio.IsValid())
	{
		LingerAudio->Stop();
	}

	Super::EndPlay(EndPlayReason);
}

const FSoundTestThrowable* ASoundTestThrower::Current() const
{
	return Throwables.IsValidIndex(ThrowIndex) ? &Throwables[ThrowIndex] : nullptr;
}

void ASoundTestThrower::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Throwables.Num() == 0)
	{
		return;
	}

	StateTime += DeltaSeconds;
	const float Alpha = StateDuration > 0.0f ? FMath::Clamp(StateTime / StateDuration, 0.0f, 1.0f) : 1.0f;

	switch (State)
	{
	case EState::Waiting:
		if (Alpha >= 1.0f)
		{
			StartThrow();
		}
		break;

	case EState::Preparing:
		if (Alpha >= 1.0f)
		{
			// release: the throwable leaves the hand on an arc
			const FSoundTestThrowable* T = Current();
			Projectile->SetWorldLocation(ThrowStart);
			Projectile->SetHiddenInGame(false);
			PlaySoundAt(T->ThrowSound, ThrowStart, TEXT("Throw"));
			Report(ESoundTestThrowPhase::Release, ThrowStart);

			State = EState::Flying;
			StateTime = 0.0f;
			StateDuration = FMath::Clamp(FVector::Dist2D(ThrowStart, ThrowEnd) / FMath::Max(ThrowSpeed, 1.0f), 0.5f, 2.5f);
		}
		break;

	case EState::Flying:
	{
		const FVector Position = FMath::Lerp(ThrowStart, ThrowEnd, Alpha) + FVector(0.0f, 0.0f, ArcHeight * 4.0f * Alpha * (1.0f - Alpha));
		Projectile->SetWorldLocationAndRotation(Position, FRotator(StateTime * -720.0f, GetActorRotation().Yaw, 0.0f));
		if (Alpha >= 1.0f)
		{
			const FSoundTestThrowable* T = Current();
			if (T->BounceSounds.Num() == 0)
			{
				// nothing to bounce (molotov): it breaks where it lands
				RollEnd = ThrowEnd;
				Projectile->SetWorldLocation(RollEnd);
				State = EState::Fusing;
				StateTime = 0.0f;
				StateDuration = T->FuseTime;
				break;
			}

			PlaySoundAt(T->BounceSounds[FMath::RandRange(0, T->BounceSounds.Num() - 1)], ThrowEnd, TEXT("Throw"));
			Report(ESoundTestThrowPhase::Bounce, ThrowEnd);

			State = EState::Rolling;
			StateTime = 0.0f;
			StateDuration = 0.45f;
		}
		break;
	}

	case EState::Rolling:
	{
		// a short second hop before it settles
		const FVector Position = FMath::Lerp(ThrowEnd, RollEnd, Alpha) + FVector(0.0f, 0.0f, 25.0f * 4.0f * Alpha * (1.0f - Alpha));
		Projectile->SetWorldLocation(Position);
		if (Alpha >= 1.0f)
		{
			const FSoundTestThrowable* T = Current();
			PlaySoundAt(T->BounceSounds[FMath::RandRange(0, T->BounceSounds.Num() - 1)], RollEnd, TEXT("Throw"), 0.5f);

			State = EState::Fusing;
			StateTime = 0.0f;
			StateDuration = Current()->FuseTime;
		}
		break;
	}

	case EState::Fusing:
		if (Alpha >= 1.0f)
		{
			Detonate();
		}
		break;

	case EState::Lingering:
		if (Alpha >= 1.0f)
		{
			if (LingerAudio.IsValid())
			{
				LingerAudio->FadeOut(1.0f, 0.0f);
			}
			Report(ESoundTestThrowPhase::LingerEnd, RollEnd);
			FinishThrow();
		}
		break;
	}
}

void ASoundTestThrower::StartThrow()
{
	ThrowIndex = (ThrowIndex + 1) % Throwables.Num();
	const FSoundTestThrowable* T = Current();

	// pick a landing spot far enough away to hear the throwable travel
	const FVector Center = GetActorLocation() + FVector(TargetAreaOffset.X, TargetAreaOffset.Y, 0.0f);
	FVector Target = Center;
	for (int32 Try = 0; Try < 10; ++Try)
	{
		Target = Center + FVector(FMath::FRandRange(-TargetAreaExtent.X, TargetAreaExtent.X), FMath::FRandRange(-TargetAreaExtent.Y, TargetAreaExtent.Y), 0.0f);
		if (FVector::Dist2D(Target, GetActorLocation()) >= MinThrowDistance)
		{
			break;
		}
	}
	Target.Z = GetActorLocation().Z + 5.0f;

	SetActorRotation(FRotator(0.0f, (Target - GetActorLocation()).Rotation().Yaw, 0.0f));
	ThrowStart = GetActorLocation() + FVector(0.0f, 0.0f, HandHeight) + GetActorForwardVector() * 30.0f;
	ThrowEnd = Target;
	RollEnd = Target + (Target - ThrowStart).GetSafeNormal2D() * FMath::FRandRange(40.0f, 90.0f);
	ArcHeight = 150.0f + FVector::Dist2D(ThrowStart, ThrowEnd) * 0.2f;

	if (T->Mesh)
	{
		Projectile->SetStaticMesh(T->Mesh);
	}
	Projectile->SetWorldScale3D(T->MeshScale);

	if (ThrowAnimation && Body->GetSkeletalMeshAsset())
	{
		Body->PlayAnimation(ThrowAnimation, false);
	}

	PlaySoundAt(T->PrepareSound, ThrowStart, TEXT("Throw"));
	Report(ESoundTestThrowPhase::Prepare, ThrowStart);

	State = EState::Preparing;
	StateTime = 0.0f;
	StateDuration = PrepareTime;
}

void ASoundTestThrower::Detonate()
{
	const FSoundTestThrowable* T = Current();

	if (T->DetonateSounds.Num() > 0)
	{
		if (T->bRandomDetonateSound)
		{
			PlaySoundAt(T->DetonateSounds[FMath::RandRange(0, T->DetonateSounds.Num() - 1)], RollEnd, T->DetonateCategory);
		}
		else
		{
			for (USoundBase* Sound : T->DetonateSounds)
			{
				PlaySoundAt(Sound, RollEnd, T->DetonateCategory);
			}
		}
	}
	Report(ESoundTestThrowPhase::Detonate, RollEnd);

	if (T->LingerSound && T->LingerTime > 0.0f)
	{
		LingerAudio = PlaySoundAt(T->LingerSound, RollEnd + FVector(0.0f, 0.0f, 30.0f), T->LingerCategory);
		Projectile->SetHiddenInGame(!T->bKeepMeshWhileLingering);

		State = EState::Lingering;
		StateTime = 0.0f;
		StateDuration = T->LingerTime;
		return;
	}

	FinishThrow();
}

void ASoundTestThrower::FinishThrow()
{
	Projectile->SetHiddenInGame(true);

	if (IdleAnimation && Body->GetSkeletalMeshAsset())
	{
		Body->PlayAnimation(IdleAnimation, true);
	}

	State = EState::Waiting;
	StateTime = 0.0f;
	StateDuration = FMath::FRandRange(ThrowInterval.X, ThrowInterval.Y);
}

UAudioComponent* ASoundTestThrower::PlaySoundAt(USoundBase* Sound, const FVector& Location, FName Category, float VolumeScale)
{
	if (!Sound)
	{
		return nullptr;
	}

	if (USoundVisualizationSubsystem* Visualization = GetWorld()->GetSubsystem<USoundVisualizationSubsystem>())
	{
		return Visualization->PlayVisualizedSound(Sound, Location, Category, Attenuation, Volume * VolumeScale, 1.0f, this);
	}

	return UGameplayStatics::SpawnSoundAtLocation(this, Sound, Location, FRotator::ZeroRotator, Volume * VolumeScale, 1.0f, 0.0f, Attenuation);
}

void ASoundTestThrower::Report(ESoundTestThrowPhase Phase, const FVector& Location)
{
	if (const FSoundTestThrowable* T = Current())
	{
		OnThrowPhase.Broadcast(this, Phase, T->Name, Location);
	}
}
