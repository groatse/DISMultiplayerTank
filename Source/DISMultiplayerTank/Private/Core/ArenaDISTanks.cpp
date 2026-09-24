#include "Core/ArenaDISTanks.h"

#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AArenaDISTanks::AArenaDISTanks()
{
	PrimaryActorTick.bCanEverTick = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("ArenaRoot")));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* CubeMesh = CubeMeshFinder.Object;

	// Floor top surface sits at Z=0; playfield is 1500 (X) by 2000 (Y).
	FloorMesh = CreateWallComponent(TEXT("FloorMesh"), CubeMesh, FVector(0.0f, 0.0f, -50.0f), FVector(15.0f, 20.0f, 1.0f));

	WallMeshes.Add(CreateWallComponent(TEXT("WallNorth"), CubeMesh, FVector(725.0f, 0.0f, 75.0f), FVector(0.5f, 20.0f, 1.5f)));
	WallMeshes.Add(CreateWallComponent(TEXT("WallSouth"), CubeMesh, FVector(-725.0f, 0.0f, 75.0f), FVector(0.5f, 20.0f, 1.5f)));
	WallMeshes.Add(CreateWallComponent(TEXT("WallEast"), CubeMesh, FVector(0.0f, 975.0f, 75.0f), FVector(14.0f, 0.5f, 1.5f)));
	WallMeshes.Add(CreateWallComponent(TEXT("WallWest"), CubeMesh, FVector(0.0f, -975.0f, 75.0f), FVector(14.0f, 0.5f, 1.5f)));
	WallMeshes.Add(CreateWallComponent(TEXT("ObstacleEast"), CubeMesh, FVector(0.0f, 350.0f, 75.0f), FVector(6.0f, 0.5f, 1.5f)));
	WallMeshes.Add(CreateWallComponent(TEXT("ObstacleWest"), CubeMesh, FVector(0.0f, -350.0f, 75.0f), FVector(6.0f, 0.5f, 1.5f)));

	ArenaLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("ArenaLight"));
	ArenaLight->SetupAttachment(GetRootComponent());
	ArenaLight->SetMobility(EComponentMobility::Movable);
	ArenaLight->SetRelativeRotation(FRotator(-55.0f, 40.0f, 0.0f));

	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCamera->SetupAttachment(GetRootComponent());
	TopDownCamera->SetRelativeLocation(FVector(0.0f, 0.0f, CameraHeightCm));
	TopDownCamera->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	TopDownCamera->SetFieldOfView(50.0f);

	// Perimeter spawn positions ordered so the classic two-player duel uses the mid-edge pair first.
	SpawnPositions.Add(FVector(0.0f, -800.0f, 60.0f));
	SpawnPositions.Add(FVector(0.0f, 800.0f, 60.0f));
	SpawnPositions.Add(FVector(-550.0f, 0.0f, 60.0f));
	SpawnPositions.Add(FVector(550.0f, 0.0f, 60.0f));
	SpawnPositions.Add(FVector(-550.0f, -800.0f, 60.0f));
	SpawnPositions.Add(FVector(550.0f, 800.0f, 60.0f));
	SpawnPositions.Add(FVector(550.0f, -800.0f, 60.0f));
	SpawnPositions.Add(FVector(-550.0f, 800.0f, 60.0f));
}

FTransform AArenaDISTanks::GetSpawnTransform(int32 SlotIndex) const
{
	if (SpawnPositions.Num() == 0 || SlotIndex < 0)
	{
		return FTransform::Identity;
	}

	const FVector SpawnLocation = SpawnPositions[SlotIndex % SpawnPositions.Num()];
	const FVector TowardCenter(-SpawnLocation.X, -SpawnLocation.Y, 0.0f);
	return FTransform(FRotator(0.0f, TowardCenter.Rotation().Yaw, 0.0f), SpawnLocation);
}

UStaticMeshComponent* AArenaDISTanks::CreateWallComponent(const TCHAR* ComponentName, UStaticMesh* CubeMesh, const FVector& RelativeCenter, const FVector& CubeScale)
{
	UStaticMeshComponent* WallComponent = CreateDefaultSubobject<UStaticMeshComponent>(ComponentName);
	WallComponent->SetupAttachment(GetRootComponent());
	WallComponent->SetStaticMesh(CubeMesh);
	WallComponent->SetMobility(EComponentMobility::Movable);
	WallComponent->SetRelativeLocation(RelativeCenter);
	WallComponent->SetRelativeScale3D(CubeScale);
	WallComponent->SetCollisionProfileName(TEXT("BlockAll"));
	return WallComponent;
}
