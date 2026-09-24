#include "Testing/AutoPilotDISTanks.h"

#include "DISMultiplayerTank.h"
#include "Tanks/TankDISTanks.h"

UAutoPilotDISTanks::UAutoPilotDISTanks()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UAutoPilotDISTanks::InitAutoPilot(int32 Seed)
{
	ManeuverRandom.Initialize(Seed);
	UE_LOG(LogDISTanks, Log, TEXT("AutoPilotEngaged Seed=%d"), Seed);
}

void UAutoPilotDISTanks::TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);

	ATankDISTanks* Tank = Cast<ATankDISTanks>(GetOwner());
	UWorld* World = GetWorld();
	if (!Tank || !World)
	{
		return;
	}

	const double NowWorldSeconds = World->GetTimeSeconds();
	if (NowWorldSeconds >= NextManeuverWorldSeconds)
	{
		PickNextManeuver(NowWorldSeconds);
	}

	Tank->SetThrustInput(CurrentThrustInput);
	Tank->SetTurnInput(CurrentTurnInput);

	// Firing keeps the current turn running so shots exercise the steerable-shell path.
	if (NowWorldSeconds >= NextFireWorldSeconds)
	{
		Tank->RequestFire();
		NextFireWorldSeconds = NowWorldSeconds + ManeuverRandom.FRandRange(FirePauseSecondsRange.X, FirePauseSecondsRange.Y);
	}
}

void UAutoPilotDISTanks::PickNextManeuver(double NowWorldSeconds)
{
	// Bias toward forward drive so the tanks roam the arena instead of idling.
	CurrentThrustInput = ManeuverRandom.FRandRange(-0.4f, 1.0f);
	CurrentTurnInput = ManeuverRandom.FRandRange(-1.0f, 1.0f);
	NextManeuverWorldSeconds = NowWorldSeconds + ManeuverRandom.FRandRange(ManeuverHoldSecondsRange.X, ManeuverHoldSecondsRange.Y);
}
