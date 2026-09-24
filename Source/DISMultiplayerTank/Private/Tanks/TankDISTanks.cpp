#include "Tanks/TankDISTanks.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DISMultiplayerTank.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Networking/PDURouterDISTanks.h"
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

	if (bIsGhost)
	{
		TickGhostInterpolation(DeltaSeconds);
		return;
	}

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

		UGameInstance* GameInstance = GetWorld()->GetGameInstance();
		UPDURouterDISTanks* Router = GameInstance ? GameInstance->GetSubsystem<UPDURouterDISTanks>() : nullptr;
		if (Router && !bIsGhost)
		{
			Router->NotifyLocalShellFired(NewShell);
		}
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
		CurrentVelocityCmPerSec = FVector::ZeroVector;
		return;
	}

	const FVector LocationBeforeStep = GetActorLocation();

	// The death sequence overrides player control with the forced slide.
	if (bDeathSequenceActive)
	{
		AddActorWorldOffset(DeathSlideDirection * (DeathSlideSpeedCmPerSec * StepSeconds), true);
		DeathSequenceRemainingSeconds -= StepSeconds;
		if (DeathSequenceRemainingSeconds <= 0.0f)
		{
			bDeathSequenceActive = false;
			SetDestroyedVisual(false);
			UE_LOG(LogDISTanks, Log, TEXT("TankRespawned Tank=%s Location=%s"), *GetName(), *GetActorLocation().ToCompactString());
		}
	}
	else
	{
		const float YawDeltaDegrees = CurrentTurnInput * TurnRateDegPerSec * StepSeconds;
		AddActorWorldRotation(FRotator(0.0f, YawDeltaDegrees, 0.0f));

		// Swept move so walls block the tank instead of being penetrated.
		const FVector MoveDelta = GetActorForwardVector() * (CurrentThrustInput * MoveSpeedCmPerSec * StepSeconds);
		AddActorWorldOffset(MoveDelta, true);
	}

	// Post-sweep velocity feeds the published dead-reckoning state.
	CurrentVelocityCmPerSec = (GetActorLocation() - LocationBeforeStep) / StepSeconds;
}

void ATankDISTanks::InitAsGhost()
{
	bIsGhost = true;
}

void ATankDISTanks::HandleConfirmedKill()
{
	UE_LOG(LogDISTanks, Log, TEXT("TankDeath Victim=%s Shell=RemoteDetonation"), *GetName());
	StartDeathSequence();
}

void ATankDISTanks::SetTintColor(const FLinearColor& NewTintColor)
{
	BaseTintColor = NewTintColor;
	EnsureTintMaterials();
	const FLinearColor AppliedColor = bShowingDestroyedVisual ? BaseTintColor * 0.15f : BaseTintColor;
	BodyMaterialInstance->SetVectorParameterValue(TEXT("Color"), AppliedColor);
	BarrelMaterialInstance->SetVectorParameterValue(TEXT("Color"), AppliedColor);
}

void ATankDISTanks::SetDestroyedVisual(bool bNewDestroyedVisual)
{
	if (bShowingDestroyedVisual == bNewDestroyedVisual)
	{
		return;
	}

	bShowingDestroyedVisual = bNewDestroyedVisual;
	SetTintColor(BaseTintColor);
}

void ATankDISTanks::EnsureTintMaterials()
{
	if (!BodyMaterialInstance)
	{
		BodyMaterialInstance = BodyMesh->CreateAndSetMaterialInstanceDynamic(0);
	}
	if (!BarrelMaterialInstance)
	{
		BarrelMaterialInstance = BarrelMesh->CreateAndSetMaterialInstanceDynamic(0);
	}
}

void ATankDISTanks::ApplyRemoteTankState(const FVector& NewBaseLocation, float NewBaseYawDegrees, const FVector& NewVelocityCmPerSec, double ReceiveWorldSeconds)
{
	RemoteBaseLocation = NewBaseLocation;
	RemoteBaseYawDegrees = NewBaseYawDegrees;
	RemoteVelocityCmPerSec = NewVelocityCmPerSec;
	RemoteBaseWorldSeconds = ReceiveWorldSeconds;
	bHasRemoteState = true;
}

void ATankDISTanks::TickGhostInterpolation(float DeltaSeconds)
{
	if (!bHasRemoteState || !GetWorld())
	{
		return;
	}

	// Constant-velocity dead reckoning with capped extrapolation, then smoothing toward the prediction.
	const float ExtrapolationSeconds = FMath::Min(static_cast<float>(GetWorld()->GetTimeSeconds() - RemoteBaseWorldSeconds), MaxExtrapolationSeconds);
	const FVector TargetLocation = RemoteBaseLocation + RemoteVelocityCmPerSec * ExtrapolationSeconds;
	SetActorLocation(FMath::VInterpTo(GetActorLocation(), TargetLocation, DeltaSeconds, GhostSmoothingSpeed));

	const FRotator TargetRotation(0.0f, RemoteBaseYawDegrees, 0.0f);
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRotation, DeltaSeconds, GhostSmoothingSpeed));
}

void ATankDISTanks::StartDeathSequence()
{
	bDeathSequenceActive = true;
	DeathSequenceRemainingSeconds = DeathSequenceSeconds;
	SetDestroyedVisual(true);

	const float SlideAngleDegrees = FMath::FRandRange(0.0f, 360.0f);
	DeathSlideDirection = FRotator(0.0f, SlideAngleDegrees, 0.0f).Vector();
}
