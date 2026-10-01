// Copyright Epic Games, Inc. All Rights Reserved.

#include "SoundTest/SoundTestDoor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"

ASoundTestDoor::ASoundTestDoor()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(Root);

	// the panel blocks everything, including the Visibility traces used for sound occlusion
	Panel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Panel"));
	Panel->SetupAttachment(Hinge);
	Panel->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	// the blocker only stops pawns, so sound still passes through an open doorway
	Blocker = CreateDefaultSubobject<UBoxComponent>(TEXT("Blocker"));
	Blocker->SetupAttachment(Root);
	Blocker->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName);
	Blocker->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Blocker->SetCollisionObjectType(ECC_WorldStatic);
	Blocker->SetCollisionResponseToAllChannels(ECR_Ignore);
	Blocker->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Blocker->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	Blocker->SetHiddenInGame(true);
	Blocker->ShapeColor = FColor(255, 40, 40);
}

void ASoundTestDoor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// fit the panel mesh to DoorSize: spans X [0, width], Y [-thickness/2, thickness/2], Z [0, height] from the hinge
	if (const UStaticMesh* Mesh = Panel->GetStaticMesh())
	{
		const FBox Bounds = Mesh->GetBoundingBox();
		const FVector MeshSize = Bounds.GetSize().ComponentMax(FVector(KINDA_SMALL_NUMBER));
		const FVector Scale = DoorSize / MeshSize;
		Panel->SetRelativeScale3D(Scale);
		Panel->SetRelativeLocation(-Bounds.Min * Scale + FVector(0.0f, -DoorSize.Y * 0.5f, 0.0f));
	}

	Blocker->SetBoxExtent(FVector(DoorSize.X * 0.5f, WallThickness * 0.5f, DoorSize.Z * 0.5f));
	Blocker->SetRelativeLocation(FVector(DoorSize.X * 0.5f, 0.0f, DoorSize.Z * 0.5f));

	Hinge->SetRelativeRotation(FRotator::ZeroRotator);
}

void ASoundTestDoor::BeginPlay()
{
	Super::BeginPlay();

	State = ESoundTestDoorState::Closed;
	StateTime = 0.0f;
	StateDuration = FMath::FRandRange(ClosedTime.X, ClosedTime.Y);
	OpenAmount = 0.0f;
}

void ASoundTestDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	StateTime += DeltaSeconds;

	switch (State)
	{
	case ESoundTestDoorState::Closed:
		if (StateTime >= StateDuration)
		{
			SetState(ESoundTestDoorState::Opening);
		}
		break;

	case ESoundTestDoorState::Opening:
		OpenAmount = FMath::Clamp(StateTime / MoveTime, 0.0f, 1.0f);
		if (OpenAmount >= 1.0f)
		{
			SetState(ESoundTestDoorState::Open);
		}
		break;

	case ESoundTestDoorState::Open:
		if (StateTime >= StateDuration)
		{
			SetState(ESoundTestDoorState::Closing);
		}
		break;

	case ESoundTestDoorState::Closing:
		OpenAmount = 1.0f - FMath::Clamp(StateTime / MoveTime, 0.0f, 1.0f);
		if (OpenAmount <= 0.0f)
		{
			SetState(ESoundTestDoorState::Closed);
		}
		break;
	}

	Hinge->SetRelativeRotation(FRotator(0.0f, OpenAngle * FMath::SmoothStep(0.0f, 1.0f, OpenAmount), 0.0f));
}

void ASoundTestDoor::SetState(ESoundTestDoorState NewState)
{
	State = NewState;
	StateTime = 0.0f;

	if (NewState == ESoundTestDoorState::Closed)
	{
		StateDuration = FMath::FRandRange(ClosedTime.X, ClosedTime.Y);
		PlayDoorSound(CloseSound);
	}
	else if (NewState == ESoundTestDoorState::Open)
	{
		StateDuration = FMath::FRandRange(OpenTime.X, OpenTime.Y);
	}
	else if (NewState == ESoundTestDoorState::Opening)
	{
		PlayDoorSound(OpenSound);
	}

	OnDoorStateChanged.Broadcast(this, NewState);
}

void ASoundTestDoor::PlayDoorSound(USoundBase* Sound) const
{
	if (Sound)
	{
		const FVector Center = Root->GetComponentTransform().TransformPosition(FVector(DoorSize.X * 0.5f, 0.0f, DoorSize.Z * 0.5f));
		UGameplayStatics::SpawnSoundAtLocation(this, Sound, Center, FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f, Attenuation);
	}
}
