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
