#include "Shells/ShellDISTanks.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DISMultiplayerTank.h"
#include "Tanks/TankDISTanks.h"
#include "UObject/ConstructorHelpers.h"

AShellDISTanks::AShellDISTanks()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(12.0f);
	CollisionSphere->SetCollisionProfileName(TEXT("BlockAll"));
	// Shells pass through other shells like in the original game.
	CollisionSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
	SetRootComponent(CollisionSphere);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	ShellMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShellMesh"));
	ShellMesh->SetupAttachment(CollisionSphere);
	ShellMesh->SetStaticMesh(SphereMeshFinder.Object);
	ShellMesh->SetRelativeScale3D(FVector(0.24f));
	ShellMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AShellDISTanks::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	LifetimeSeconds += DeltaSeconds;
	if (LifetimeSeconds > MaxLifetimeSeconds)
	{
		Destroy();
		return;
	}

	// Cap the accumulator so a long hitch cannot trigger a catch-up spiral.
	FlightTimeAccumulator = FMath::Min(FlightTimeAccumulator + DeltaSeconds, 0.25f);
	while (FlightTimeAccumulator >= FixedStepSeconds)
	{
		FlightTimeAccumulator -= FixedStepSeconds;
		SimulateFlightStep(FixedStepSeconds);
		if (!IsValid(this))
		{
			return;
		}
	}
}

void AShellDISTanks::InitShell(ATankDISTanks* FiringTank)
{
	OwnerTank = FiringTank;
	SetOwner(FiringTank);
	if (FiringTank)
	{
		CollisionSphere->MoveIgnoreActors.Add(FiringTank);
	}
}

void AShellDISTanks::SimulateFlightStep(float StepSeconds)
{
	if (OwnerTank.IsValid())
	{
		const float SteerDeltaDegrees = OwnerTank->GetTurnInput() * SteerRateDegPerSec * StepSeconds;
		AddActorWorldRotation(FRotator(0.0f, SteerDeltaDegrees, 0.0f));
	}

	const FVector FlightDelta = GetActorForwardVector() * (FlightSpeedCmPerSec * StepSeconds);
	FHitResult BlockingHit;
	AddActorWorldOffset(FlightDelta, true, &BlockingHit);
	if (BlockingHit.bBlockingHit)
	{
		HandleImpact(BlockingHit);
	}
}

void AShellDISTanks::HandleImpact(const FHitResult& ImpactHit)
{
	ATankDISTanks* HitTank = Cast<ATankDISTanks>(ImpactHit.GetActor());
	if (HitTank && HitTank != OwnerTank.Get())
	{
		HitTank->HandleShellHit(this);
	}

	UE_LOG(LogDISTanks, Log, TEXT("ShellImpact Location=%s HitActor=%s"), *GetActorLocation().ToCompactString(), *GetNameSafe(ImpactHit.GetActor()));
	Destroy();
}
