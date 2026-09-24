#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "PDUs/EntityInfoFamily/GRILL_EntityStatePDU.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PDURouterDISTanks.generated.h"

class ATankDISTanks;

/** Bookkeeping for one remote entity mirrored as a local ghost actor. */
struct FGhostEntryDISTanks
{
	TWeakObjectPtr<ATankDISTanks> GhostTank;
	double LastHeardWorldSeconds = 0.0;
	double LastTimestampSecondsInHour = -1.0;
};

/** Sole GRILL DIS touchpoint: owns the UDP sockets, publishes the local tank's entity state with dead-reckoning thresholds, and routes received PDUs to ghost actors. */
UCLASS()
class DISMULTIPLAYERTANK_API UPDURouterDISTanks : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Creates the local identity, opens sockets, binds PDU handlers, and starts the per-frame ticker. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Stops the ticker, unbinds handlers, and closes all sockets. */
	virtual void Deinitialize() override;

	/** Registers the locally-owned tank whose state this instance publishes. */
	void RegisterLocalTank(ATankDISTanks* NewLocalTank);

	/** Returns this instance's tank entity ID. */
	const FEntityID& GetLocalTankEntityID() const { return LocalTankEntityID; }

	/** Returns the spawn slot to use until peer negotiation lands in phase 5, overridable with -PlayerSlot=. */
	int32 GetInterimSlotIndex() const;

protected:
	/** Routes a received Entity State PDU to the matching ghost tank, spawning one for unknown remote entities. */
	UFUNCTION()
	void HandleEntityStatePDU(FEntityStatePDU EntityStatePDU);

	/** Opens the broadcast send socket and the shared-port loopback-friendly receive socket. */
	void OpenSockets();

	/** Publishes the local tank when the mirrored dead-reckoning prediction drifts past a threshold or the heartbeat elapses. */
	void EvaluateLocalTankPublish();

	/** Builds and emits the local tank's Entity State PDU and refreshes the mirrored prediction baseline. */
	void SendLocalTankPDU(double NowWorldSeconds, const FVector& LocationMeters, const FVector& VelocityMetersPerSec, float YawDegrees);

	/** Destroys ghost tanks that have not been heard from within the timeout. */
	void RemoveStaleGhosts(double NowWorldSeconds);

	/** Per-frame driver for publishing and ghost timeout checks. */
	bool HandleTicker(float DeltaSeconds);

	/** Seconds between forced heartbeat publishes of the local tank. */
	UPROPERTY(EditDefaultsOnly, Category = "DIS")
	float HeartbeatSeconds = 1.0f;

	/** Positional dead-reckoning error in meters that triggers a publish. */
	UPROPERTY(EditDefaultsOnly, Category = "DIS")
	float PositionThresholdMeters = 0.1f;

	/** Yaw drift in degrees that triggers a publish. */
	UPROPERTY(EditDefaultsOnly, Category = "DIS")
	float YawThresholdDegrees = 2.0f;

	/** Seconds without updates after which a ghost tank is removed. */
	UPROPERTY(EditDefaultsOnly, Category = "DIS")
	float GhostTimeoutSeconds = 10.0f;

private:
	FEntityID LocalTankEntityID;
	TWeakObjectPtr<ATankDISTanks> LocalTank;
	TMap<FEntityID, FGhostEntryDISTanks> GhostTanks;
	FTSTicker::FDelegateHandle TickerHandle;
	FString BroadcastAddress = TEXT("255.255.255.255");
	int32 UdpPort = 3000;
	FVector LastSentLocationMeters = FVector::ZeroVector;
	FVector LastSentVelocityMetersPerSec = FVector::ZeroVector;
	float LastSentYawDegrees = 0.0f;
	double LastSentWorldSeconds = 0.0;
	bool bHasPublishedBefore = false;
};
