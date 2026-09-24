#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameModeDISTanks.generated.h"

class AArenaDISTanks;

/** Per-instance match orchestrator that assembles the arena and spawns the local tank. */
UCLASS()
class DISMULTIPLAYERTANK_API AGameModeDISTanks : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGameModeDISTanks();

	/** Spawns the default pawn at the arena's slot spawn point instead of a PlayerStart. */
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;

	/** Starts the new player and switches their view to the fixed arena camera. */
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

protected:
	/** Spawns the arena once on demand and returns it. */
	AArenaDISTanks* EnsureArenaSpawned();

	/** Spawns an unpossessed target tank at slot 1 for local hit testing until ghost tanks arrive in phase 3. */
	void SpawnPracticeTarget();

	/** Arena class to spawn, exposed for tuning or Blueprint variants. */
	UPROPERTY(EditDefaultsOnly, Category = "Match")
	TSubclassOf<AArenaDISTanks> ArenaClass;

	/** Whether to spawn the temporary practice target tank. */
	UPROPERTY(EditDefaultsOnly, Category = "Match")
	bool bSpawnPracticeTarget = true;

	UPROPERTY()
	TObjectPtr<AArenaDISTanks> ArenaActor;
};
