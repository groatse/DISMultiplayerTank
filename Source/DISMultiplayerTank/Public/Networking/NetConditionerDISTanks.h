#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

/** One delayed PDU delivery held by the conditioner. */
struct FPendingDeliveryDISTanks
{
	double DueWorldSeconds = 0.0;
	TFunction<void()> Deliver;
};

/** Command-line-driven inbound network conditioner applying seeded loss, latency, jitter, and duplication to received PDUs; jitter also produces natural reordering. */
struct FNetConditionerDISTanks
{
	/** Parses the -NetSim* arguments and enables the conditioner when any are present. */
	void InitFromCommandLine();

	/** Returns true when conditioning is active. */
	bool IsEnabled() const { return bEnabled; }

	/** Rolls whether the incoming PDU is dropped. */
	bool ShouldDrop();

	/** Rolls whether the incoming PDU is delivered a second time. */
	bool ShouldDuplicate();

	/** Queues one delivery after the rolled latency plus jitter. */
	void EnqueueDelivery(double NowWorldSeconds, TFunction<void()> DeliverFunction);

	/** Runs every queued delivery whose due time has passed. */
	void DrainDueDeliveries(double NowWorldSeconds);

	bool bEnabled = false;
	float LatencySeconds = 0.0f;
	float JitterSeconds = 0.0f;
	float LossFraction = 0.0f;
	float DuplicateFraction = 0.0f;
	FRandomStream ConditionerRandom;
	TArray<FPendingDeliveryDISTanks> PendingDeliveries;
};
