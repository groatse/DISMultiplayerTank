#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PeerRegistryDISTanks.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnPeerSetChangedDISTanks);

/** One known remote peer instance. */
struct FPeerDISTanks
{
	int32 ApplicationID = 0;
	double LastHeardWorldSeconds = 0.0;
};

/** Owns the local session identity and the discovered peer set, deriving deterministic player slots from token ranking. */
UCLASS()
class DISMULTIPLAYERTANK_API UPeerRegistryDISTanks : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Generates the random session token that doubles as this instance's DIS application ID. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Fires whenever a peer joins or leaves. */
	FOnPeerSetChangedDISTanks OnPeerSetChanged;

	/** Returns the local session token / DIS application ID. */
	int32 GetLocalApplicationID() const { return LocalApplicationID; }

	/** Records that a peer was heard, firing the change event on first contact. */
	void NotifyPeerHeard(int32 ApplicationID, double NowWorldSeconds);

	/** Removes a silent peer and fires the change event. */
	void NotifyPeerLost(int32 ApplicationID);

	/** Returns true when at least one remote peer is known. */
	bool IsMatchReady() const { return Peers.Num() > 0; }

	/** Returns the number of known remote peers. */
	int32 GetPeerCount() const { return Peers.Num(); }

	/** Returns the local instance's slot: the rank of its token among all known tokens. */
	int32 GetLocalSlotIndex() const { return GetSlotForApplication(LocalApplicationID); }

	/** Returns the slot any application ID occupies under the shared token ranking. */
	int32 GetSlotForApplication(int32 ApplicationID) const;

	/** Returns the classic Combat color for a slot. */
	static FLinearColor GetSlotColor(int32 SlotIndex);

private:
	TMap<int32, FPeerDISTanks> Peers;
	int32 LocalApplicationID = 0;
};
