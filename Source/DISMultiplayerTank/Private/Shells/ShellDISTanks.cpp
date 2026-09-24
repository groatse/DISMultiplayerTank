#include "Shells/ShellDISTanks.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DISMultiplayerTank.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Networking/PDURouterDISTanks.h"
#include "Tanks/TankDISTanks.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Fetches the PDU router subsystem for an actor, or null outside a game instance. */
	UPDURouterDISTanks* GetRouterForActor(const AActor* Actor)
	{
		UGameInstance* GameInstance = Actor && Actor->GetWorld() ? Actor->GetWorld()->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UPDURouterDISTanks>() : nullptr;
	}
}

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
	// The engine sphere's default slot is the parameterless WorldGridMaterial, so assign the parameterized shape material for tinting.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TintableMaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	ShellMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShellMesh"));
	ShellMesh->SetupAttachment(CollisionSphere);
	ShellMesh->SetStaticMesh(SphereMeshFinder.Object);
	ShellMesh->SetMaterial(0, TintableMaterialFinder.Object);
	// Oversized visual relative to the 12cm collision sphere so the fast shell stays readable from the top-down camera.
	ShellMesh->SetRelativeScale3D(FVector(0.5f));
	ShellMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AShellDISTanks::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bIsGhost)
	{
		TickGhostInterpolation(DeltaSeconds);
		return;
	}

	LifetimeSeconds += DeltaSeconds;
	if (LifetimeSeconds > MaxLifetimeSeconds)
	{
		if (UPDURouterDISTanks* Router = GetRouterForActor(this))
		{
			Router->NotifyLocalShellDetonated(this, GetActorLocation(), nullptr);
		}
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

void AShellDISTanks::InitAsGhost()
{
	bIsGhost = true;
	SetActorEnableCollision(false);
}

void AShellDISTanks::ApplyRemoteShellState(const FVector& NewBaseLocation, float NewBaseYawDegrees, const FVector& NewVelocityCmPerSec, double ReceiveWorldSeconds)
{
	RemoteBaseLocation = NewBaseLocation;
	RemoteBaseYawDegrees = NewBaseYawDegrees;
	RemoteVelocityCmPerSec = NewVelocityCmPerSec;
	RemoteBaseWorldSeconds = ReceiveWorldSeconds;
	bHasRemoteState = true;
}

FVector AShellDISTanks::GetFlightVelocityCmPerSec() const
{
	return bIsGhost ? RemoteVelocityCmPerSec : GetActorForwardVector() * FlightSpeedCmPerSec;
}

void AShellDISTanks::SetTintColor(const FLinearColor& NewTintColor)
{
	if (!ShellMaterialInstance)
	{
		ShellMaterialInstance = ShellMesh->CreateAndSetMaterialInstanceDynamic(0);
	}
	ShellMaterialInstance->SetVectorParameterValue(TEXT("Color"), NewTintColor);
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

	// Ghost tanks are adjudicated by their owning instance via the Detonation PDU, not hit locally.
	if (HitTank && HitTank != OwnerTank.Get() && !HitTank->IsGhost())
	{
		HitTank->HandleShellHit(this);
	}

	if (UPDURouterDISTanks* Router = GetRouterForActor(this))
	{
		Router->NotifyLocalShellDetonated(this, GetActorLocation(), ImpactHit.GetActor());
	}

	UE_LOG(LogDISTanks, Log, TEXT("ShellImpact Location=%s HitActor=%s"), *GetActorLocation().ToCompactString(), *GetNameSafe(ImpactHit.GetActor()));
	Destroy();
}

void AShellDISTanks::TickGhostInterpolation(float DeltaSeconds)
{
	if (!bHasRemoteState || !GetWorld())
	{
		return;
	}

	// Constant-velocity dead reckoning with capped extrapolation, then smoothing toward the prediction.
	const float ExtrapolationSeconds = FMath::Min(static_cast<float>(GetWorld()->GetTimeSeconds() - RemoteBaseWorldSeconds), MaxExtrapolationSeconds);
	const FVector TargetLocation = RemoteBaseLocation + RemoteVelocityCmPerSec * ExtrapolationSeconds;
	SetActorLocation(FMath::VInterpTo(GetActorLocation(), TargetLocation, DeltaSeconds, GhostSmoothingSpeed));
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0.0f, RemoteBaseYawDegrees, 0.0f), DeltaSeconds, GhostSmoothingSpeed));
}
