#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Networking/PDUTypesDISTanks.h"
#include "PDUs/EntityInfoFamily/GRILL_EntityStatePDU.h"
#include "PDUs/WarfareFamily/GRILL_DetonationPDU.h"
#include "PDUs/WarfareFamily/GRILL_FirePDU.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PDURouterDISTanks.generated.h"

class AShellDISTanks;
class ATankDISTanks;

/** Bookkeeping for one remote entity mirrored as a local ghost actor. */
struct FGhostEntryDISTanks
{
	TWeakObjectPtr<AActor> GhostActor;
	double LastHeardWorldSeconds = 0.0;
	double LastTimestampSecondsInHour = -1.0;
};

/** Sole GRILL DIS touchpoint: owns the UDP sockets, publishes the local tank and shell with dead-reckoning thresholds, routes ghosts, and adjudicates kills on the victim's side. */
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

	/** Assigns the new local shell its entity ID, emits the Fire PDU, and starts publishing its state. */
	void NotifyLocalShellFired(AShellDISTanks* NewLocalShell);

	/** Emits the Detonation PDU for the local shell, targeting the hit ghost tank's entity when there is one. */
	void NotifyLocalShellDetonated(AShellDISTanks* DetonatedShell, const FVector& ImpactLocationCm, AActor* HitActor);

	/** Returns this instance's tank entity ID. */
	const FEntityID& GetLocalTankEntityID() const { return LocalTankEntityID; }

protected:
	/** Reapplies slot colors to all ghost tanks after the peer ranking changes. */
	void RetintGhostTanks();

	/** Routes a received Entity State PDU to the matching ghost tank or shell, spawning one for unknown remote entities. */
	UFUNCTION()
	void HandleEntityStatePDU(FEntityStatePDU EntityStatePDU);

	/** Spawns the ghost shell for a remote fire event before its first Entity State PDU arrives. */
	UFUNCTION()
	void HandleFirePDU(FFirePDU FirePDU);

	/** Removes the ghost shell for the detonated munition and adjudicates the hit when the local tank is the target. */
	UFUNCTION()
	void HandleDetonationPDU(FDetonationPDU DetonationPDU);

	/** Opens the broadcast send socket and the shared-port loopback-friendly receive socket. */
	void OpenSockets();

	/** Publishes the local tank when its mirrored dead-reckoning prediction drifts, its destroyed state flips, or the heartbeat elapses. */
	void EvaluateLocalTankPublish();

	/** Publishes the local shell on its faster heartbeat and steering-sensitive thresholds. */
	void EvaluateLocalShellPublish();

	/** Applies a remote tank state to its ghost, spawning the ghost when first heard. */
	void HandleRemoteTankState(const FEntityStatePDU& EntityStatePDU, double NowWorldSeconds, double PduSecondsInHour);

	/** Applies a remote shell state to its ghost, spawning the ghost when first heard. */
	void HandleRemoteShellState(const FEntityStatePDU& EntityStatePDU, double NowWorldSeconds, double PduSecondsInHour);

	/** Destroys ghost actors that have not been heard from within their timeout. */
	void RemoveStaleGhosts(double NowWorldSeconds);

	/** Per-frame driver for publishing and ghost timeout checks. */
	bool HandleTicker(float DeltaSeconds);

	/** Seconds between forced heartbeat publishes of the local tank. */
	UPROPERTY(EditDefaultsOnly, Category = "DIS")
	float TankHeartbeatSeconds = 1.0f;

	/** Seconds between forced heartbeat publishes of the local shell. */
	UPROPERTY(EditDefaultsOnly, Category = "DIS")
	float ShellHeartbeatSeconds = 0.1f;

	/** Positional dead-reckoning error in meters that triggers a publish. */
	UPROPERTY(EditDefaultsOnly, Category = "DIS")
	float PositionThresholdMeters = 0.1f;

	/** Yaw drift in degrees that triggers a publish. */
	UPROPERTY(EditDefaultsOnly, Category = "DIS")
	float YawThresholdDegrees = 2.0f;

	/** Seconds without updates after which a ghost tank is removed. */
	UPROPERTY(EditDefaultsOnly, Category = "DIS")
	float GhostTankTimeoutSeconds = 10.0f;

	/** Seconds without updates after which a ghost shell is removed. */
	UPROPERTY(EditDefaultsOnly, Category = "DIS")
	float GhostShellTimeoutSeconds = 2.0f;

	/** Maximum distance in cm between a detonation and the local tank for the hit to be accepted. */
	UPROPERTY(EditDefaultsOnly, Category = "DIS")
	float DetonationToleranceCm = 250.0f;

private:
	/** Builds the shared header and kinematics of an Entity State PDU. */
	FEntityStatePDU BuildEntityStatePDU(const FEntityID& EntityID, double NowWorldSeconds, const FVector& LocationMeters, float YawDegrees, const FVector& VelocityMetersPerSec) const;

	/** Emits any PDU's bytes over the open send sockets. */
	void EmitPDUBytes(const TArray<uint8>& PduBytes);

	/** Finds the entity ID of a registered ghost tank actor, or unset when the actor is not one. */
	bool FindGhostTankEntityID(const AActor* GhostActor, FEntityID& OutEntityID) const;

	/** Allocates the next fire event ID. */
	FEventID NextEventID();

	FEntityID LocalTankEntityID;
	FEntityID LocalShellEntityID;
	TWeakObjectPtr<ATankDISTanks> LocalTank;
	TWeakObjectPtr<AShellDISTanks> LocalShell;
	FPublishTrackerDISTanks TankTracker;
	FPublishTrackerDISTanks ShellTracker;
	TMap<FEntityID, FGhostEntryDISTanks> GhostTanks;
	TMap<FEntityID, FGhostEntryDISTanks> GhostShells;
	FTSTicker::FDelegateHandle TickerHandle;
	FString BroadcastAddress = TEXT("255.255.255.255");
	int32 UdpPort = 3000;
	int32 NextShellEntityNumber = 0;
	int32 NextEventNumber = 1;
	float SmoothedFrameSeconds = 1.0f / 60.0f;
	double LastHealthLogSeconds = 0.0;
};
