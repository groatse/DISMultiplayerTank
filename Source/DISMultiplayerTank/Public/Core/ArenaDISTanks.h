#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaDISTanks.generated.h"

class UCameraComponent;
class UDirectionalLightComponent;
class UStaticMeshComponent;

/** Procedurally assembled top-down arena providing floor, walls, lighting, the fixed camera, and spawn slots. */
UCLASS()
class DISMULTIPLAYERTANK_API AArenaDISTanks : public AActor
{
	GENERATED_BODY()

public:
	AArenaDISTanks();

	/** Returns the world-space spawn transform for any player slot, facing the arena center and wrapping past the position count. */
	FTransform GetSpawnTransform(int32 SlotIndex) const;

	/** Returns the number of distinct spawn positions. */
	int32 GetSpawnSlotCount() const { return SpawnPositions.Num(); }

protected:
	/** Creates one cube-based wall component with the given name, center, and scale. */
	UStaticMeshComponent* CreateWallComponent(const TCHAR* ComponentName, UStaticMesh* CubeMesh, const FVector& RelativeCenter, const FVector& CubeScale);

	/** Placeholder cube stretched into the arena floor. */
	UPROPERTY(VisibleAnywhere, Category = "Arena")
	TObjectPtr<UStaticMeshComponent> FloorMesh;

	/** Boundary and obstacle wall components. */
	UPROPERTY(VisibleAnywhere, Category = "Arena")
	TArray<TObjectPtr<UStaticMeshComponent>> WallMeshes;

	/** Single movable light for the whole arena. */
	UPROPERTY(VisibleAnywhere, Category = "Arena")
	TObjectPtr<UDirectionalLightComponent> ArenaLight;

	/** Fixed top-down camera used as the view target for all players. */
	UPROPERTY(VisibleAnywhere, Category = "Arena")
	TObjectPtr<UCameraComponent> TopDownCamera;

	/** Camera altitude above the arena floor in cm. */
	UPROPERTY(EditAnywhere, Category = "Arena")
	float CameraHeightCm = 3200.0f;

	/** Spawn positions around the arena perimeter, indexed by player slot. */
	UPROPERTY(EditAnywhere, Category = "Arena")
	TArray<FVector> SpawnPositions;
};
