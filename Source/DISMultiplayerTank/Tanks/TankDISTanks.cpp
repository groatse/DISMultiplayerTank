#include "Tanks/TankDISTanks.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ATankDISTanks::ATankDISTanks()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	CollisionBox->SetBoxExtent(FVector(60.0f, 45.0f, 25.0f));
	CollisionBox->SetCollisionProfileName(TEXT("Pawn"));
	SetRootComponent(CollisionBox);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(CollisionBox);
	BodyMesh->SetStaticMesh(CubeMeshFinder.Object);
	BodyMesh->SetRelativeScale3D(FVector(1.2f, 0.9f, 0.5f));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BarrelMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrelMesh"));
	BarrelMesh->SetupAttachment(CollisionBox);
	BarrelMesh->SetStaticMesh(CubeMeshFinder.Object);
	BarrelMesh->SetRelativeScale3D(FVector(0.9f, 0.12f, 0.12f));
	BarrelMesh->SetRelativeLocation(FVector(95.0f, 0.0f, 0.0f));
	BarrelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ATankDISTanks::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Cap the accumulator so a long hitch cannot trigger a catch-up spiral.
	MovementTimeAccumulator = FMath::Min(MovementTimeAccumulator + DeltaSeconds, 0.25f);
	while (MovementTimeAccumulator >= FixedStepSeconds)
	{
		MovementTimeAccumulator -= FixedStepSeconds;
		SimulateMovementStep(FixedStepSeconds);
	}
}

void ATankDISTanks::SetThrustInput(float NewThrustInput)
{
	CurrentThrustInput = FMath::Clamp(NewThrustInput, -1.0f, 1.0f);
}

void ATankDISTanks::SetTurnInput(float NewTurnInput)
{
	CurrentTurnInput = FMath::Clamp(NewTurnInput, -1.0f, 1.0f);
}

void ATankDISTanks::SetMovementFrozen(bool bNewMovementFrozen)
{
	bMovementFrozen = bNewMovementFrozen;
}

void ATankDISTanks::RequestFire()
{
	UE_LOG(LogTemp, Log, TEXT("RequestFire: shells arrive in phase 2."));
}

void ATankDISTanks::SimulateMovementStep(float StepSeconds)
{
	if (bMovementFrozen)
	{
		return;
	}

	const float YawDeltaDegrees = CurrentTurnInput * TurnRateDegPerSec * StepSeconds;
	AddActorWorldRotation(FRotator(0.0f, YawDeltaDegrees, 0.0f));

	// Swept move so walls block the tank instead of being penetrated.
	const FVector MoveDelta = GetActorForwardVector() * (CurrentThrustInput * MoveSpeedCmPerSec * StepSeconds);
	AddActorWorldOffset(MoveDelta, true);
}
