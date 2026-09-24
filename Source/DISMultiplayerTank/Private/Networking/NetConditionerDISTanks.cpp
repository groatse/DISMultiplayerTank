#include "Networking/NetConditionerDISTanks.h"

#include "DISMultiplayerTank.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void FNetConditionerDISTanks::InitFromCommandLine()
{
	float LatencyMs = 0.0f;
	float JitterMs = 0.0f;
	float LossPct = 0.0f;
	float DupPct = 0.0f;
	int32 Seed = 42;
	const bool bAnyArgumentPresent =
		FParse::Value(FCommandLine::Get(), TEXT("NetSimLatencyMs="), LatencyMs) |
		FParse::Value(FCommandLine::Get(), TEXT("NetSimJitterMs="), JitterMs) |
		FParse::Value(FCommandLine::Get(), TEXT("NetSimLossPct="), LossPct) |
		FParse::Value(FCommandLine::Get(), TEXT("NetSimDupPct="), DupPct);
	FParse::Value(FCommandLine::Get(), TEXT("NetSimSeed="), Seed);

	if (!bAnyArgumentPresent)
	{
		return;
	}

	bEnabled = true;
	LatencySeconds = LatencyMs / 1000.0f;
	JitterSeconds = JitterMs / 1000.0f;
	LossFraction = FMath::Clamp(LossPct / 100.0f, 0.0f, 1.0f);
	DuplicateFraction = FMath::Clamp(DupPct / 100.0f, 0.0f, 1.0f);
	ConditionerRandom.Initialize(Seed);

	UE_LOG(LogDISTanks, Log, TEXT("NetSimEnabled LatencyMs=%.0f JitterMs=%.0f LossPct=%.0f DupPct=%.0f Seed=%d"), LatencyMs, JitterMs, LossPct, DupPct, Seed);
}

bool FNetConditionerDISTanks::ShouldDrop()
{
	return ConditionerRandom.FRand() < LossFraction;
}

bool FNetConditionerDISTanks::ShouldDuplicate()
{
	return ConditionerRandom.FRand() < DuplicateFraction;
}

void FNetConditionerDISTanks::EnqueueDelivery(double NowWorldSeconds, TFunction<void()> DeliverFunction)
{
	FPendingDeliveryDISTanks PendingDelivery;
	PendingDelivery.DueWorldSeconds = NowWorldSeconds + LatencySeconds + ConditionerRandom.FRandRange(0.0f, JitterSeconds);
	PendingDelivery.Deliver = MoveTemp(DeliverFunction);
	PendingDeliveries.Add(MoveTemp(PendingDelivery));
}

void FNetConditionerDISTanks::DrainDueDeliveries(double NowWorldSeconds)
{
	for (int32 DeliveryIndex = PendingDeliveries.Num() - 1; DeliveryIndex >= 0; --DeliveryIndex)
	{
		if (PendingDeliveries[DeliveryIndex].DueWorldSeconds <= NowWorldSeconds)
		{
			// Copy out before removal so the array can shrink safely during delivery.
			TFunction<void()> Deliver = MoveTemp(PendingDeliveries[DeliveryIndex].Deliver);
			PendingDeliveries.RemoveAtSwap(DeliveryIndex);
			Deliver();
		}
	}
}
