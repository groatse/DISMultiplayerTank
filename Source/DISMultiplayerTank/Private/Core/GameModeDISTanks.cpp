#include "Core/GameModeDISTanks.h"

#include "Core/ArenaDISTanks.h"
#include "Core/PlayerControllerDISTanks.h"
#include "DISMultiplayerTank.h"
#include "Networking/PDURouterDISTanks.h"
#include "Networking/PeerRegistryDISTanks.h"
#include "Tanks/TankDISTanks.h"
#include "UI/HUDDISTanks.h"

AGameModeDISTanks::AGameModeDISTanks()
{
	DefaultPawnClass = ATankDISTanks::StaticClass();
	PlayerControllerClass = APlayerControllerDISTanks::StaticClass();
	HUDClass = AHUDDISTanks::StaticClass();
	ArenaClass = AArenaDISTanks::StaticClass();
}

APawn* AGameModeDISTanks::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	UGameInstance* GameInstance = GetGameInstance();
	UPeerRegistryDISTanks* Registry = GameInstance ? GameInstance->GetSubsystem<UPeerRegistryDISTanks>() : nullptr;
	const int32 SpawnSlotIndex = Registry ? Registry->GetLocalSlotIndex() : 0;

	AArenaDISTanks* Arena = EnsureArenaSpawned();
	const FTransform SpawnTransform = Arena ? Arena->GetSpawnTransform(SpawnSlotIndex) : FTransform::Identity;
	APawn* NewPawn = SpawnDefaultPawnAtTransform(NewPlayer, SpawnTransform);
	LocalTankPawn = Cast<ATankDISTanks>(NewPawn);

	if (UPDURouterDISTanks* Router = GameInstance ? GameInstance->GetSubsystem<UPDURouterDISTanks>() : nullptr)
	{
		Router->RegisterLocalTank(LocalTankPawn.Get());
	}

	if (Registry)
	{
		Registry->OnPeerSetChanged.AddUObject(this, &AGameModeDISTanks::RefreshMatchState);
		Registry->OnRoundChanged.AddUObject(this, &AGameModeDISTanks::HandleRoundChanged);
	}
	RefreshMatchState();

	return NewPawn;
}

void AGameModeDISTanks::HandleRoundChanged(int32 NewRoundNumber)
{
	// A new round aborts any running death sequence and repositions the local tank at its slot.
	if (ATankDISTanks* Tank = LocalTankPawn.Get())
	{
		Tank->CancelDeathSequence();
	}
	LiveSlotIndex = -1;
	RefreshMatchState();
}

void AGameModeDISTanks::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	if (AArenaDISTanks* Arena = EnsureArenaSpawned())
	{
		NewPlayer->SetViewTargetWithBlend(Arena);
	}

	SpawnPracticeTarget();
}

void AGameModeDISTanks::SpawnPracticeTarget()
{
	if (!bSpawnPracticeTarget || !GetWorld())
	{
		return;
	}

	AArenaDISTanks* Arena = EnsureArenaSpawned();
	if (!Arena)
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	GetWorld()->SpawnActor<ATankDISTanks>(ATankDISTanks::StaticClass(), Arena->GetSpawnTransform(1), SpawnParameters);
}

void AGameModeDISTanks::RefreshMatchState()
{
	UPeerRegistryDISTanks* Registry = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPeerRegistryDISTanks>() : nullptr;
	ATankDISTanks* Tank = LocalTankPawn.Get();
	if (!Registry || !Tank)
	{
		return;
	}

	if (!Registry->IsMatchReady())
	{
		if (bMatchLive || !bHasAnnouncedWaiting)
		{
			Tank->SetMovementFrozen(true);
			UE_LOG(LogDISTanks, Log, TEXT("WaitingForPeers App=%d"), Registry->GetLocalApplicationID());
		}
		bMatchLive = false;
		bHasAnnouncedWaiting = true;
		return;
	}

	// Every instance repositions only its own tank; ghosts follow through dead reckoning and snap.
	const int32 SlotIndex = Registry->GetLocalSlotIndex();
	if (!bMatchLive || SlotIndex != LiveSlotIndex)
	{
		AArenaDISTanks* Arena = EnsureArenaSpawned();
		const FTransform SlotTransform = Arena ? Arena->GetSpawnTransform(SlotIndex) : FTransform::Identity;
		Tank->SetActorLocationAndRotation(SlotTransform.GetLocation(), SlotTransform.GetRotation());
		Tank->SetTintColor(UPeerRegistryDISTanks::GetSlotColor(SlotIndex));
		Tank->SetMovementFrozen(false);
		bMatchLive = true;
		LiveSlotIndex = SlotIndex;
		UE_LOG(LogDISTanks, Log, TEXT("MatchStarted Slot=%d App=%d Peers=%d"), SlotIndex, Registry->GetLocalApplicationID(), Registry->GetPeerCount());
	}
}

AArenaDISTanks* AGameModeDISTanks::EnsureArenaSpawned()
{
	if (!ArenaActor && GetWorld())
	{
		const TSubclassOf<AArenaDISTanks> ClassToSpawn = ArenaClass ? ArenaClass : TSubclassOf<AArenaDISTanks>(AArenaDISTanks::StaticClass());
		ArenaActor = GetWorld()->SpawnActor<AArenaDISTanks>(ClassToSpawn, FTransform::Identity);
	}
	return ArenaActor;
}
