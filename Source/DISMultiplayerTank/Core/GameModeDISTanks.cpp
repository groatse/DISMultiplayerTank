#include "Core/GameModeDISTanks.h"

#include "Core/ArenaDISTanks.h"
#include "Core/PlayerControllerDISTanks.h"
#include "Tanks/TankDISTanks.h"

AGameModeDISTanks::AGameModeDISTanks()
{
	DefaultPawnClass = ATankDISTanks::StaticClass();
	PlayerControllerClass = APlayerControllerDISTanks::StaticClass();
	ArenaClass = AArenaDISTanks::StaticClass();
}

APawn* AGameModeDISTanks::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	// Slot 0 until the peer registry assigns negotiated slots in phase 5.
	AArenaDISTanks* Arena = EnsureArenaSpawned();
	const FTransform SpawnTransform = Arena ? Arena->GetSpawnTransform(0) : FTransform::Identity;
	return SpawnDefaultPawnAtTransform(NewPlayer, SpawnTransform);
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

AArenaDISTanks* AGameModeDISTanks::EnsureArenaSpawned()
{
	if (!ArenaActor && GetWorld())
	{
		const TSubclassOf<AArenaDISTanks> ClassToSpawn = ArenaClass ? ArenaClass : TSubclassOf<AArenaDISTanks>(AArenaDISTanks::StaticClass());
		ArenaActor = GetWorld()->SpawnActor<AArenaDISTanks>(ClassToSpawn, FTransform::Identity);
	}
	return ArenaActor;
}
