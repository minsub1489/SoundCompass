// Copyright Epic Games, Inc. All Rights Reserved.

#include "SoundTest/SoundTestWindow.h"
#include "SoundVisualization/SoundVisualizationSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "TimerManager.h"

ASoundTestWindow::ASoundTestWindow()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// the intact pane blocks everything, including the Visibility traces used for sound occlusion
	Pane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pane"));
	Pane->SetupAttachment(Root);
	Pane->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Pane->SetCastShadow(false);
	Pane->bDisallowNanite = true;   // translucent glass materials are not supported by Nanite
}

void ASoundTestWindow::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// fit the pane mesh to PaneSize: spans X [-w/2, w/2], Y [-t/2, t/2], Z [0, h] around the root
	if (const UStaticMesh* Mesh = Pane->GetStaticMesh())
	{
		const FBox Bounds = Mesh->GetBoundingBox();
		const FVector MeshSize = Bounds.GetSize().ComponentMax(FVector(KINDA_SMALL_NUMBER));
		const FVector Scale = PaneSize / MeshSize;
		Pane->SetRelativeScale3D(Scale);
		Pane->SetRelativeLocation(-Bounds.Min * Scale - FVector(PaneSize.X * 0.5f, PaneSize.Y * 0.5f, 0.0f));
	}
}

void ASoundTestWindow::BeginPlay()
{
	Super::BeginPlay();

	Restore();
}

void ASoundTestWindow::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(StateTimer);
	ClearShards();

	Super::EndPlay(EndPlayReason);
}

FVector ASoundTestWindow::GetPaneCenter() const
{
	return Root->GetComponentTransform().TransformPosition(FVector(0.0f, 0.0f, PaneSize.Z * 0.5f));
}

void ASoundTestWindow::Break()
{
	if (bBroken)
	{
		return;
	}
	bBroken = true;

	Pane->SetVisibility(false);
	Pane->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	const FVector Center = GetPaneCenter();
	if (BreakSounds.Num() > 0)
	{
		if (USoundBase* Sound = BreakSounds[FMath::RandRange(0, BreakSounds.Num() - 1)])
		{
			if (USoundVisualizationSubsystem* Visualization = GetWorld()->GetSubsystem<USoundVisualizationSubsystem>())
			{
				Visualization->PlayVisualizedSound(Sound, Center, TEXT("GlassBreak"), Attenuation, Volume, FMath::FRandRange(0.95f, 1.05f), this);
			}
			else
			{
				UGameplayStatics::SpawnSoundAtLocation(this, Sound, Center, FRotator::ZeroRotator, Volume, 1.0f, 0.0f, Attenuation);
			}
		}
	}

	// shards: small copies of the pane mesh that fall and tumble
	ClearShards();
	const FTransform& Transform = Root->GetComponentTransform();
	for (int32 i = 0; i < ShardCount && Pane->GetStaticMesh(); ++i)
	{
		UStaticMeshComponent* Shard = NewObject<UStaticMeshComponent>(this);
		Shard->SetStaticMesh(Pane->GetStaticMesh());
		Shard->SetMaterial(0, Pane->GetMaterial(0));
		Shard->SetCastShadow(false);
		Shard->bDisallowNanite = true;
		Shard->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
		Shard->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Shard->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);

		const FVector Local(FMath::FRandRange(-0.45f, 0.45f) * PaneSize.X, 0.0f, FMath::FRandRange(0.1f, 0.9f) * PaneSize.Z);
		const FVector MeshSize = Pane->GetStaticMesh()->GetBoundingBox().GetSize().ComponentMax(FVector(KINDA_SMALL_NUMBER));
		const FVector ShardSize(FMath::FRandRange(15.0f, 40.0f), PaneSize.Y, FMath::FRandRange(15.0f, 40.0f));
		Shard->SetWorldTransform(FTransform(Transform.GetRotation() * FQuat(FVector::YAxisVector, FMath::FRandRange(-PI, PI)),
			Transform.TransformPosition(Local), ShardSize / MeshSize));
		Shard->RegisterComponent();
		Shard->SetSimulatePhysics(true);
		Shard->SetMassOverrideInKg(NAME_None, 0.3f, true);
		Shard->AddImpulse(Transform.TransformVector(FVector(FMath::FRandRange(-60.0f, 60.0f), ShardDirection * FMath::FRandRange(80.0f, 220.0f), 40.0f)), NAME_None, true);
		Shards.Add(Shard);
	}

	OnWindowBroken.Broadcast(this, Center);
	GetWorldTimerManager().SetTimer(StateTimer, this, &ASoundTestWindow::Restore, FMath::FRandRange(BrokenTime.X, BrokenTime.Y), false);
}

void ASoundTestWindow::Restore()
{
	const bool bWasBroken = bBroken;
	bBroken = false;

	ClearShards();
	Pane->SetVisibility(true);
	Pane->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

	if (bWasBroken)
	{
		OnWindowRestored.Broadcast(this, GetPaneCenter());
	}
	GetWorldTimerManager().SetTimer(StateTimer, this, &ASoundTestWindow::Break, FMath::FRandRange(IntactTime.X, IntactTime.Y), false);
}

void ASoundTestWindow::ClearShards()
{
	for (UStaticMeshComponent* Shard : Shards)
	{
		if (Shard)
		{
			Shard->DestroyComponent();
		}
	}
	Shards.Reset();
}
