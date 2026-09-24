#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PeerRegistryDISTanks.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnPeerSetChangedDISTanks);
DECLARE_MULTICAST_DELEGATE(FOnScoreChangedDISTanks);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnRoundChangedDISTanks, int32);

/** One known remote peer instance with its last published score state. */
struct FPeerDISTanks
{
	int32 ApplicationID = 0;
	double LastHeardWorldSeconds = 0.0;
	int32 RoundNumber = 1;
	TMap<int32, int32> DeathsByKiller;
};

/** Owns the local session identity and the discovered peer set, deriving deterministic player slots from token ranking. */
UCLASS()
class DISMULTIPLAYERTANK_API UPeerRegistryDISTanks : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Generates the random session token that doubles as this instance's DIS application ID. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Kills needed to win a round and reset the map. */
	static constexpr int32 KillsToWin = 10;

	/** Fires whenever a peer joins or leaves. */
	FOnPeerSetChangedDISTanks OnPeerSetChanged;

	/** Fires whenever any kill counter changes. */
	FOnScoreChangedDISTanks OnScoreChanged;

	/** Fires with the new round number when the round resets. */
	FOnRoundChangedDISTanks OnRoundChanged;

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

	/** Records that the local tank died to the given killer this round. */
	void RecordLocalDeath(int32 KillerApplicationID);

	/** Merges a peer's published round number and per-killer death counters with take-max semantics. */
	void ApplyPeerScoreState(int32 PeerApplicationID, int32 PeerRoundNumber, const TMap<int32, int32>& PeerDeathsByKiller);

	/** Returns the kill count credited to an application this round. */
	int32 GetScoreForApplication(int32 ApplicationID) const;

	/** Returns the current round number. */
	int32 GetCurrentRoundNumber() const { return CurrentRoundNumber; }

	/** Returns the local tank's per-killer death counters this round. */
	const TMap<int32, int32>& GetLocalDeathsByKiller() const { return LocalDeathsByKiller; }

	/** Returns every known application ID ordered by slot. */
	TArray<int32> GetAllApplicationIDsBySlot() const;

private:
	/** Starts the given round, clearing every counter on this instance. */
	void AdvanceRound(int32 NewRoundNumber);

	/** Advances the round when any application has reached the kill target. */
	void CheckRoundWin();

	TMap<int32, FPeerDISTanks> Peers;
	TMap<int32, int32> LocalDeathsByKiller;
	int32 LocalApplicationID = 0;
	int32 CurrentRoundNumber = 1;
};
