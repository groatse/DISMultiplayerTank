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

	/** Marks this shell as a ghost mirroring a remote instance's shell, with collision disabled. */
	void InitAsGhost();

	/** Returns true when this shell mirrors a remote instance's shell. */
	bool IsGhost() const { return bIsGhost; }

	/** Applies a freshly received remote state as the new dead-reckoning baseline. */
	void ApplyRemoteShellState(const FVector& NewBaseLocation, float NewBaseYawDegrees, const FVector& NewVelocityCmPerSec, double ReceiveWorldSeconds);

	/** Returns the current flight velocity in cm/s. */
	FVector GetFlightVelocityCmPerSec() const;

protected:
	/** Applies one fixed step of steering and swept flight, detonating on any blocking hit. */
	void SimulateFlightStep(float StepSeconds);

	/** Handles a blocking impact by damaging a hit tank, reporting the detonation, and destroying the shell. */
	void HandleImpact(const FHitResult& ImpactHit);

	/** Moves a ghost toward its dead-reckoned target with smoothing. */
	void TickGhostInterpolation(float DeltaSeconds);

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

	/** How quickly a ghost shell closes on its dead-reckoned target. */
	UPROPERTY(EditAnywhere, Category = "Shell|Ghost")
	float GhostSmoothingSpeed = 12.0f;

	/** Maximum seconds a ghost shell extrapolates past its last received update. */
	UPROPERTY(EditAnywhere, Category = "Shell|Ghost")
	float MaxExtrapolationSeconds = 0.5f;

private:
	TWeakObjectPtr<ATankDISTanks> OwnerTank;
	FVector RemoteBaseLocation = FVector::ZeroVector;
	FVector RemoteVelocityCmPerSec = FVector::ZeroVector;
	float RemoteBaseYawDegrees = 0.0f;
	double RemoteBaseWorldSeconds = 0.0;
	float FlightTimeAccumulator = 0.0f;
	float LifetimeSeconds = 0.0f;
	bool bIsGhost = false;
	bool bHasRemoteState = false;
};
