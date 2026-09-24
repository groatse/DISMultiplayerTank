#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShellDISTanks.generated.h"

class ATankDISTanks;
class USphereComponent;
class UStaticMeshComponent;

/** Kinematic tank shell that flies on a fixed timestep and steers with its owner tank's turn input. */
UCLASS()
class DISMULTIPLAYERTANK_API AShellDISTanks : public AActor
{
	GENERATED_BODY()

public:
	AShellDISTanks();

	/** Advances the fixed-timestep flight simulation. */
	virtual void Tick(float DeltaSeconds) override;

	/** Binds the shell to its firing tank and starts flight along the tank's facing. */
	void InitShell(ATankDISTanks* FiringTank);

protected:
	/** Applies one fixed step of steering and swept flight, detonating on any blocking hit. */
	void SimulateFlightStep(float StepSeconds);

	/** Handles a blocking impact by damaging a hit tank and destroying the shell. */
	void HandleImpact(const FHitResult& ImpactHit);

	/** Collision sphere that sweeps against walls and tanks. */
	UPROPERTY(VisibleAnywhere, Category = "Shell")
	TObjectPtr<USphereComponent> CollisionSphere;

	/** Placeholder sphere for the shot. */
	UPROPERTY(VisibleAnywhere, Category = "Shell")
	TObjectPtr<UStaticMeshComponent> ShellMesh;

	/** Flight speed in cm/s. */
	UPROPERTY(EditAnywhere, Category = "Shell")
	float FlightSpeedCmPerSec = 900.0f;

	/** Steering rate in degrees/s applied from the owner tank's turn input. */
	UPROPERTY(EditAnywhere, Category = "Shell")
	float SteerRateDegPerSec = 90.0f;

	/** Length of one fixed simulation step in seconds. */
	UPROPERTY(EditAnywhere, Category = "Shell")
	float FixedStepSeconds = 1.0f / 60.0f;

	/** Failsafe lifetime in seconds after which the shell self-destructs. */
	UPROPERTY(EditAnywhere, Category = "Shell")
	float MaxLifetimeSeconds = 5.0f;

private:
	TWeakObjectPtr<ATankDISTanks> OwnerTank;
	float FlightTimeAccumulator = 0.0f;
	float LifetimeSeconds = 0.0f;
};
