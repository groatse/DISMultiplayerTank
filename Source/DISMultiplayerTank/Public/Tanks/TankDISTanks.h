#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "TankDISTanks.generated.h"

class AShellDISTanks;
class UBoxComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/** Player tank pawn with classic Combat-style movement simulated on a fixed timestep. */
UCLASS()
class DISMULTIPLAYERTANK_API ATankDISTanks : public APawn
{
	GENERATED_BODY()

public:
	ATankDISTanks();

	/** Advances the fixed-timestep movement simulation. */
	virtual void Tick(float DeltaSeconds) override;

	/** Stores the current forward/back thrust input in the range [-1, 1]. */
	void SetThrustInput(float NewThrustInput);

	/** Stores the current turn input in the range [-1, 1], positive turning right. */
	void SetTurnInput(float NewTurnInput);

	/** Returns the current turn input so shells can steer with their owner tank. */
	float GetTurnInput() const { return CurrentTurnInput; }

	/** Blocks or unblocks movement simulation, used by the death sequence. */
	void SetMovementFrozen(bool bNewMovementFrozen);

	/** Fires a shell if none of ours is currently alive and we are not dying. */
	void RequestFire();

	/** Reacts to being hit by an enemy shell by starting the death sequence. */
	void HandleShellHit(AShellDISTanks* HittingShell);

	/** Returns true while the freeze-and-slide death sequence is running. */
	bool IsDeathSequenceActive() const { return bDeathSequenceActive; }

	/** Marks this tank as a ghost mirroring a remote instance's tank through dead reckoning. */
	void InitAsGhost();

	/** Returns true when this tank mirrors a remote instance's tank. */
	bool IsGhost() const { return bIsGhost; }

	/** Applies a freshly received remote state as the new dead-reckoning baseline. */
	void ApplyRemoteTankState(const FVector& NewBaseLocation, float NewBaseYawDegrees, const FVector& NewVelocityCmPerSec, double ReceiveWorldSeconds);

	/** Returns the velocity produced by the last simulation step in cm/s. */
	FVector GetSimVelocityCmPerSec() const { return CurrentVelocityCmPerSec; }

	/** Starts the death sequence for a kill adjudicated from a remote Detonation PDU, bouncing away from the killer's position. */
	void HandleConfirmedKill(const FVector& KillerLocationCm);

	/** Applies a base tint to the hull and barrel placeholder meshes. */
	void SetTintColor(const FLinearColor& NewTintColor);

	/** Returns the tank's current base tint. */
	const FLinearColor& GetTintColor() const { return BaseTintColor; }

	/** Toggles the darkened destroyed look mirrored from a remote tank's published appearance. */
	void SetDestroyedVisual(bool bNewDestroyedVisual);

	/** Aborts a running death sequence, used when the round resets. */
	void CancelDeathSequence();

protected:
	/** Applies one fixed step of rotation and swept translation, or the forced death slide. */
	void SimulateMovementStep(float StepSeconds);

	/** Freezes player control and starts the forced bounce away from the threat with a random spin. */
	void StartDeathSequence(const FVector& ThreatLocationCm);

	/** Moves a ghost toward its dead-reckoned target with smoothing. */
	void TickGhostInterpolation(float DeltaSeconds);

	/** Collision box that sweeps against walls and receives shell hits. */
	UPROPERTY(VisibleAnywhere, Category = "Tank")
	TObjectPtr<UBoxComponent> CollisionBox;

	/** Placeholder cube for the hull. */
	UPROPERTY(VisibleAnywhere, Category = "Tank")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** Placeholder cube for the hull-fixed forward barrel. */
	UPROPERTY(VisibleAnywhere, Category = "Tank")
	TObjectPtr<UStaticMeshComponent> BarrelMesh;

	/** Forward drive speed in cm/s. */
	UPROPERTY(EditAnywhere, Category = "Tank|Movement")
	float MoveSpeedCmPerSec = 350.0f;

	/** Turn rate in degrees/s. */
	UPROPERTY(EditAnywhere, Category = "Tank|Movement")
	float TurnRateDegPerSec = 110.0f;

	/** Length of one fixed simulation step in seconds. */
	UPROPERTY(EditAnywhere, Category = "Tank|Movement")
	float FixedStepSeconds = 1.0f / 60.0f;

	/** Shell class to spawn when firing. */
	UPROPERTY(EditDefaultsOnly, Category = "Tank|Combat")
	TSubclassOf<AShellDISTanks> ShellClass;

	/** Muzzle distance from the tank center in cm. */
	UPROPERTY(EditAnywhere, Category = "Tank|Combat")
	float MuzzleOffsetCm = 165.0f;

	/** Duration of the freeze-and-slide death sequence in seconds. */
	UPROPERTY(EditAnywhere, Category = "Tank|Combat")
	float DeathSequenceSeconds = 1.5f;

	/** Forced slide speed during the death sequence in cm/s. */
	UPROPERTY(EditAnywhere, Category = "Tank|Combat")
	float DeathSlideSpeedCmPerSec = 300.0f;

	/** Half-angle in degrees of the away-from-killer arc the death bounce direction is drawn from. */
	UPROPERTY(EditAnywhere, Category = "Tank|Combat")
	float DeathArcHalfAngleDegrees = 60.0f;

	/** Distance in cm beyond which a ghost snaps to its dead-reckoned target instead of smoothing. */
	UPROPERTY(EditAnywhere, Category = "Tank|Ghost")
	float GhostSnapDistanceCm = 400.0f;

	/** How quickly a ghost closes on its dead-reckoned target. */
	UPROPERTY(EditAnywhere, Category = "Tank|Ghost")
	float GhostSmoothingSpeed = 8.0f;

	/** Maximum seconds a ghost extrapolates past its last received update. */
	UPROPERTY(EditAnywhere, Category = "Tank|Ghost")
	float MaxExtrapolationSeconds = 1.0f;

	/** Lazily creates the dynamic material instances used for tinting. */
	void EnsureTintMaterials();

private:
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterialInstance;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BarrelMaterialInstance;

	FLinearColor BaseTintColor = FLinearColor(0.5f, 0.5f, 0.5f);
	bool bShowingDestroyedVisual = false;

	TWeakObjectPtr<AShellDISTanks> ActiveShell;
	FVector DeathSlideDirection = FVector::ZeroVector;
	float DeathSpinRateDegPerSec = 0.0f;
	FVector CurrentVelocityCmPerSec = FVector::ZeroVector;
	FVector RemoteBaseLocation = FVector::ZeroVector;
	FVector RemoteVelocityCmPerSec = FVector::ZeroVector;
	float RemoteBaseYawDegrees = 0.0f;
	double RemoteBaseWorldSeconds = 0.0;
	float DeathSequenceRemainingSeconds = 0.0f;
	float CurrentThrustInput = 0.0f;
	float CurrentTurnInput = 0.0f;
	float MovementTimeAccumulator = 0.0f;
	bool bMovementFrozen = false;
	bool bDeathSequenceActive = false;
	bool bIsGhost = false;
	bool bHasRemoteState = false;
};
