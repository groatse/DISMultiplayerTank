#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "TankDISTanks.generated.h"

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

	/** Fires a shell if none of ours is currently alive (implemented in phase 2). */
	void RequestFire();

protected:
	/** Applies one fixed step of rotation and swept translation. */
	void SimulateMovementStep(float StepSeconds);

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

private:
	float CurrentThrustInput = 0.0f;
	float CurrentTurnInput = 0.0f;
	float MovementTimeAccumulator = 0.0f;
	bool bMovementFrozen = false;
};
