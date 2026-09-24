#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameModeDISTanks.generated.h"

class AArenaDISTanks;
class ATankDISTanks;

/** Per-instance match orchestrator: assembles the arena, spawns the local tank, and starts play when peers are discovered. */
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

	/** Applies the current peer situation: waiting-frozen with no peers, or repositioned/tinted/unfrozen at the negotiated slot. */
	void RefreshMatchState();

	/** Aborts any death sequence and repositions the local tank when the round resets. */
	void HandleRoundChanged(int32 NewRoundNumber);

	/** Arena class to spawn, exposed for tuning or Blueprint variants. */
	UPROPERTY(EditDefaultsOnly, Category = "Match")
	TSubclassOf<AArenaDISTanks> ArenaClass;

	/** Whether to spawn the temporary practice target tank, superseded by DIS ghost tanks. */
	UPROPERTY(EditDefaultsOnly, Category = "Match")
	bool bSpawnPracticeTarget = false;

	UPROPERTY()
	TObjectPtr<AArenaDISTanks> ArenaActor;

private:
	TWeakObjectPtr<ATankDISTanks> LocalTankPawn;
	int32 LiveSlotIndex = -1;
	bool bMatchLive = false;
	bool bHasAnnouncedWaiting = false;
};
