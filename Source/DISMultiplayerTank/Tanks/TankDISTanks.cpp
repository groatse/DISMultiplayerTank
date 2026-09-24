#include "Tanks/TankDISTanks.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DISMultiplayerTank.h"
#include "Shells/ShellDISTanks.h"
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

	ShellClass = AShellDISTanks::StaticClass();
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
	if (ActiveShell.IsValid() || bDeathSequenceActive || !ShellClass || !GetWorld())
	{
		return;
	}

	const FTransform MuzzleTransform(GetActorRotation(), GetActorLocation() + GetActorForwardVector() * MuzzleOffsetCm);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AShellDISTanks* NewShell = GetWorld()->SpawnActor<AShellDISTanks>(ShellClass, MuzzleTransform, SpawnParameters);
	if (NewShell)
	{
		NewShell->InitShell(this);
		ActiveShell = NewShell;
		UE_LOG(LogDISTanks, Log, TEXT("Fire Shooter=%s Location=%s"), *GetName(), *GetActorLocation().ToCompactString());
	}
}

void ATankDISTanks::HandleShellHit(AShellDISTanks* HittingShell)
{
	if (bDeathSequenceActive)
	{
		return;
	}

	UE_LOG(LogDISTanks, Log, TEXT("TankDeath Victim=%s Shell=%s"), *GetName(), *GetNameSafe(HittingShell));
	StartDeathSequence();
}

void ATankDISTanks::SimulateMovementStep(float StepSeconds)
{
	if (bMovementFrozen)
	{
		return;
	}

	// The death sequence overrides player control with the forced slide.
	if (bDeathSequenceActive)
	{
		AddActorWorldOffset(DeathSlideDirection * (DeathSlideSpeedCmPerSec * StepSeconds), true);
		DeathSequenceRemainingSeconds -= StepSeconds;
		if (DeathSequenceRemainingSeconds <= 0.0f)
		{
			bDeathSequenceActive = false;
			UE_LOG(LogDISTanks, Log, TEXT("TankRespawned Tank=%s Location=%s"), *GetName(), *GetActorLocation().ToCompactString());
		}
		return;
	}

	const float YawDeltaDegrees = CurrentTurnInput * TurnRateDegPerSec * StepSeconds;
	AddActorWorldRotation(FRotator(0.0f, YawDeltaDegrees, 0.0f));

	// Swept move so walls block the tank instead of being penetrated.
	const FVector MoveDelta = GetActorForwardVector() * (CurrentThrustInput * MoveSpeedCmPerSec * StepSeconds);
	AddActorWorldOffset(MoveDelta, true);
}

void ATankDISTanks::StartDeathSequence()
{
	bDeathSequenceActive = true;
	DeathSequenceRemainingSeconds = DeathSequenceSeconds;

	const float SlideAngleDegrees = FMath::FRandRange(0.0f, 360.0f);
	DeathSlideDirection = FRotator(0.0f, SlideAngleDegrees, 0.0f).Vector();
}
