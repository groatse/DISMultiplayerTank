#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "TankDISTanks.generated.h"

class AShellDISTanks;
class UBoxComponent;
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

protected:
	/** Applies one fixed step of rotation and swept translation, or the forced death slide. */
	void SimulateMovementStep(float StepSeconds);

	/** Freezes player control and starts the forced slide in a random direction. */
	void StartDeathSequence();

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

private:
	TWeakObjectPtr<AShellDISTanks> ActiveShell;
	FVector DeathSlideDirection = FVector::ZeroVector;
	float DeathSequenceRemainingSeconds = 0.0f;
	float CurrentThrustInput = 0.0f;
	float CurrentTurnInput = 0.0f;
	float MovementTimeAccumulator = 0.0f;
	bool bMovementFrozen = false;
	bool bDeathSequenceActive = false;
};
