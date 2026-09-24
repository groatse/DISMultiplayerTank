#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Math/RandomStream.h"
#include "AutoPilotDISTanks.generated.h"

class ATankDISTanks;

/** Test-only driver that feeds the owning tank seeded random maneuvers and periodic fire so instances can fight unattended. */
UCLASS()
class DISMULTIPLAYERTANK_API UAutoPilotDISTanks : public UActorComponent
{
	GENERATED_BODY()

public:
	UAutoPilotDISTanks();

	/** Seeds the maneuver stream. */
	void InitAutoPilot(int32 Seed);

	/** Applies the current maneuver and rolls new maneuvers and shots on their timers. */
	virtual void TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	/** Rolls the next thrust/turn combination and its duration. */
	void PickNextManeuver(double NowWorldSeconds);

	/** Shortest and longest maneuver hold time in seconds. */
	UPROPERTY(EditAnywhere, Category = "AutoPilot")
	FVector2D ManeuverHoldSecondsRange = FVector2D(0.5, 2.5);

	/** Shortest and longest pause between shots in seconds. */
	UPROPERTY(EditAnywhere, Category = "AutoPilot")
	FVector2D FirePauseSecondsRange = FVector2D(1.5, 4.0);

private:
	FRandomStream ManeuverRandom;
	double NextManeuverWorldSeconds = 0.0;
	double NextFireWorldSeconds = 0.0;
	float CurrentThrustInput = 0.0f;
	float CurrentTurnInput = 0.0f;
};
