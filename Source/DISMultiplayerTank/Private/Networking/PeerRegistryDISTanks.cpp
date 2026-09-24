#include "Networking/PeerRegistryDISTanks.h"

#include "DISMultiplayerTank.h"

void UPeerRegistryDISTanks::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// The random token doubles as the DIS application ID; a 1-in-65535 collision is an accepted prototype limitation.
	LocalApplicationID = FMath::RandRange(1, 65535);
	UE_LOG(LogDISTanks, Log, TEXT("PeerRegistryInitialized Token=%d"), LocalApplicationID);
}

void UPeerRegistryDISTanks::NotifyPeerHeard(int32 ApplicationID, double NowWorldSeconds)
{
	FPeerDISTanks* KnownPeer = Peers.Find(ApplicationID);
	if (KnownPeer)
	{
		KnownPeer->LastHeardWorldSeconds = NowWorldSeconds;
		return;
	}

	FPeerDISTanks NewPeer;
	NewPeer.ApplicationID = ApplicationID;
	NewPeer.LastHeardWorldSeconds = NowWorldSeconds;
	Peers.Add(ApplicationID, NewPeer);

	UE_LOG(LogDISTanks, Log, TEXT("PeerJoined App=%d PeerCount=%d"), ApplicationID, Peers.Num());
	OnPeerSetChanged.Broadcast();
}

void UPeerRegistryDISTanks::NotifyPeerLost(int32 ApplicationID)
{
	if (Peers.Remove(ApplicationID) > 0)
	{
		UE_LOG(LogDISTanks, Log, TEXT("PeerLost App=%d PeerCount=%d"), ApplicationID, Peers.Num());
		OnPeerSetChanged.Broadcast();
	}
}

void UPeerRegistryDISTanks::RecordLocalDeath(int32 KillerApplicationID)
{
	LocalDeathsByKiller.FindOrAdd(KillerApplicationID)++;
	UE_LOG(LogDISTanks, Log, TEXT("ScoreDeath Killer=%d Deaths=%d Round=%d"), KillerApplicationID, LocalDeathsByKiller[KillerApplicationID], CurrentRoundNumber);
	OnScoreChanged.Broadcast();
	CheckRoundWin();
}

void UPeerRegistryDISTanks::ApplyPeerScoreState(int32 PeerApplicationID, int32 PeerRoundNumber, const TMap<int32, int32>& PeerDeathsByKiller)
{
	// A higher round number from any peer means the reset already happened elsewhere; adopt it.
	if (PeerRoundNumber > CurrentRoundNumber)
	{
		AdvanceRound(PeerRoundNumber);
	}

	FPeerDISTanks* Peer = Peers.Find(PeerApplicationID);
	if (!Peer || PeerRoundNumber < CurrentRoundNumber)
	{
		return;
	}

	Peer->RoundNumber = PeerRoundNumber;

	// Take-max merge keeps counters immune to dropped, duplicated, or reordered packets.
	bool bAnyCounterChanged = false;
	for (const TPair<int32, int32>& CounterPair : PeerDeathsByKiller)
	{
		int32& StoredDeaths = Peer->DeathsByKiller.FindOrAdd(CounterPair.Key);
		if (CounterPair.Value > StoredDeaths)
		{
			StoredDeaths = CounterPair.Value;
			bAnyCounterChanged = true;
		}
	}

	if (bAnyCounterChanged)
	{
		OnScoreChanged.Broadcast();
		CheckRoundWin();
	}
}

int32 UPeerRegistryDISTanks::GetScoreForApplication(int32 ApplicationID) const
{
	int32 TotalKills = 0;
	if (const int32* LocalDeaths = LocalDeathsByKiller.Find(ApplicationID))
	{
		TotalKills += *LocalDeaths;
	}
	for (const TPair<int32, FPeerDISTanks>& PeerPair : Peers)
	{
		if (const int32* PeerDeaths = PeerPair.Value.DeathsByKiller.Find(ApplicationID))
		{
			TotalKills += *PeerDeaths;
		}
	}
	return TotalKills;
}

TArray<int32> UPeerRegistryDISTanks::GetAllApplicationIDsBySlot() const
{
	TArray<int32> AllApplicationIDs;
	Peers.GenerateKeyArray(AllApplicationIDs);
	AllApplicationIDs.Add(LocalApplicationID);
	AllApplicationIDs.Sort();
	return AllApplicationIDs;
}

void UPeerRegistryDISTanks::AdvanceRound(int32 NewRoundNumber)
{
	if (NewRoundNumber <= CurrentRoundNumber)
	{
		return;
	}

	CurrentRoundNumber = NewRoundNumber;
	LocalDeathsByKiller.Empty();
	for (TPair<int32, FPeerDISTanks>& PeerPair : Peers)
	{
		PeerPair.Value.DeathsByKiller.Empty();
	}

	UE_LOG(LogDISTanks, Log, TEXT("RoundReset Round=%d"), CurrentRoundNumber);
	OnRoundChanged.Broadcast(CurrentRoundNumber);
	OnScoreChanged.Broadcast();
}

void UPeerRegistryDISTanks::CheckRoundWin()
{
	for (const int32 ApplicationID : GetAllApplicationIDsBySlot())
	{
		if (GetScoreForApplication(ApplicationID) >= KillsToWin)
		{
			UE_LOG(LogDISTanks, Log, TEXT("RoundWon Winner=%d Round=%d"), ApplicationID, CurrentRoundNumber);
			AdvanceRound(CurrentRoundNumber + 1);
			return;
		}
	}
}

int32 UPeerRegistryDISTanks::GetSlotForApplication(int32 ApplicationID) const
{
	// Every instance sorts the same token set, so the ranking is identical everywhere without a handshake.
	TArray<int32> AllApplicationIDs;
	Peers.GenerateKeyArray(AllApplicationIDs);
	AllApplicationIDs.Add(LocalApplicationID);
	AllApplicationIDs.Sort();
	return AllApplicationIDs.IndexOfByKey(ApplicationID);
}

FLinearColor UPeerRegistryDISTanks::GetSlotColor(int32 SlotIndex)
{
	static const FLinearColor ClassicSlotColors[] = {
		FLinearColor(0.05f, 0.45f, 0.05f),
		FLinearColor(0.55f, 0.05f, 0.05f),
		FLinearColor(0.05f, 0.15f, 0.55f),
		FLinearColor(0.55f, 0.45f, 0.05f)
	};

	if (SlotIndex >= 0 && SlotIndex < static_cast<int32>(UE_ARRAY_COUNT(ClassicSlotColors)))
	{
		return ClassicSlotColors[SlotIndex];
	}

	// Slots past the classic four get golden-angle-spaced hues so any N stays distinguishable.
	const float HueDegrees = FMath::Fmod(FMath::Max(SlotIndex, 0) * 137.5f, 360.0f);
	return FLinearColor::MakeFromHSV8(static_cast<uint8>(HueDegrees / 360.0f * 255.0f), 220, 130);
}
