#include "Networking/PDURouterDISTanks.h"

#include "DISMultiplayerTank.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Networking/PDUTypesDISTanks.h"
#include "PDUProcessor.h"
#include "Tanks/TankDISTanks.h"
#include "UDPSubsystem.h"

void UPDURouterDISTanks::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency(UUDPSubsystem::StaticClass());
	Collection.InitializeDependency(UPDUProcessor::StaticClass());

	// Random application ID keeps two instances distinct until phase 5 negotiation replaces it.
	LocalTankEntityID = FEntityID(DISTanksProtocol::SiteID, FMath::RandRange(1, 65535), DISTanksProtocol::TankEntityNumber);

	if (UPDUProcessor* Processor = GetGameInstance()->GetSubsystem<UPDUProcessor>())
	{
		Processor->OnEntityStatePDUProcessed.AddDynamic(this, &UPDURouterDISTanks::HandleEntityStatePDU);
	}

	OpenSockets();

	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UPDURouterDISTanks::HandleTicker), 0.0f);

	UE_LOG(LogDISTanks, Log, TEXT("RouterInitialized EntityID=%s Port=%d"), *DISTanksProtocol::EntityIDToString(LocalTankEntityID), UdpPort);
}

void UPDURouterDISTanks::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);

	if (UPDUProcessor* Processor = GetGameInstance()->GetSubsystem<UPDUProcessor>())
	{
		Processor->OnEntityStatePDUProcessed.RemoveDynamic(this, &UPDURouterDISTanks::HandleEntityStatePDU);
	}

	if (UUDPSubsystem* UdpSubsystem = GetGameInstance()->GetSubsystem<UUDPSubsystem>())
	{
		UdpSubsystem->CloseAllSendSockets();
		UdpSubsystem->CloseAllReceiveSockets();
	}

	Super::Deinitialize();
}

void UPDURouterDISTanks::RegisterLocalTank(ATankDISTanks* NewLocalTank)
{
	LocalTank = NewLocalTank;
	bHasPublishedBefore = false;
}

int32 UPDURouterDISTanks::GetInterimSlotIndex() const
{
	int32 SlotIndex = LocalTankEntityID.Application % 2;
	FParse::Value(FCommandLine::Get(), TEXT("PlayerSlot="), SlotIndex);
	return SlotIndex;
}

void UPDURouterDISTanks::HandleEntityStatePDU(FEntityStatePDU EntityStatePDU)
{
	if (EntityStatePDU.ExerciseID != DISTanksProtocol::ExerciseID || EntityStatePDU.EntityID == LocalTankEntityID)
	{
		return;
	}

	if (EntityStatePDU.EntityID.Entity != DISTanksProtocol::TankEntityNumber)
	{
		return;
	}

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	const double NowWorldSeconds = World->GetTimeSeconds();
	const double PduSecondsInHour = DISTanksProtocol::TimestampToSecondsInHour(EntityStatePDU.Timestamp);

	FGhostEntryDISTanks& GhostEntry = GhostTanks.FindOrAdd(EntityStatePDU.EntityID);
	if (GhostEntry.GhostTank.IsValid() && !DISTanksProtocol::IsTimestampNewer(PduSecondsInHour, GhostEntry.LastTimestampSecondsInHour))
	{
		return;
	}

	const FVector GhostLocationCm = DISTanksProtocol::ToUnrealCm(EntityStatePDU.EcefLocation);
	const float GhostYawDegrees = FMath::RadiansToDegrees(EntityStatePDU.EntityOrientation.Yaw);

	ATankDISTanks* GhostTank = GhostEntry.GhostTank.Get();
	if (!GhostTank)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		GhostTank = World->SpawnActor<ATankDISTanks>(ATankDISTanks::StaticClass(), FTransform(FRotator(0.0f, GhostYawDegrees, 0.0f), GhostLocationCm), SpawnParameters);
		if (!GhostTank)
		{
			return;
		}
		GhostTank->InitAsGhost();
		GhostEntry.GhostTank = GhostTank;
		UE_LOG(LogDISTanks, Log, TEXT("GhostTankSpawned Entity=%s Location=%s"), *DISTanksProtocol::EntityIDToString(EntityStatePDU.EntityID), *GhostLocationCm.ToCompactString());
	}

	GhostTank->ApplyRemoteTankState(GhostLocationCm, GhostYawDegrees, DISTanksProtocol::ToUnrealCm(EntityStatePDU.EntityLinearVelocity), NowWorldSeconds);
	GhostEntry.LastTimestampSecondsInHour = PduSecondsInHour;
	GhostEntry.LastHeardWorldSeconds = NowWorldSeconds;
}

void UPDURouterDISTanks::OpenSockets()
{
	UdpPort = DISTanksProtocol::DefaultUdpPort;
	FParse::Value(FCommandLine::Get(), TEXT("DISPort="), UdpPort);
	FParse::Value(FCommandLine::Get(), TEXT("DISBroadcastAddress="), BroadcastAddress);

	UUDPSubsystem* UdpSubsystem = GetGameInstance()->GetSubsystem<UUDPSubsystem>();
	if (!UdpSubsystem)
	{
		return;
	}

	FSendSocketSettings SendSettings;
	SendSettings.SendSocketConnectionType = EConnectionType::Broadcast;
	SendSettings.SocketDescription = TEXT("DISTanks-Send");
	int32 SendSocketID = 0;
	const bool bSendOpen = UdpSubsystem->OpenSendSocket(SendSettings, SendSocketID, BroadcastAddress, UdpPort);

	// Loopback must stay enabled so a second instance on the same machine is heard.
	FReceiveSocketSettings ReceiveSettings;
	ReceiveSettings.SocketDescription = TEXT("DISTanks-Receive");
	ReceiveSettings.bUseMulticast = false;
	ReceiveSettings.bAllowLoopback = true;
	ReceiveSettings.bReceiveDataOnGameThread = true;
	int32 ReceiveSocketID = 0;
	const bool bReceiveOpen = UdpSubsystem->OpenReceiveSocket(ReceiveSettings, ReceiveSocketID, TEXT("0.0.0.0"), UdpPort);

	UE_LOG(LogDISTanks, Log, TEXT("SocketsOpened Send=%d Receive=%d Address=%s Port=%d"), bSendOpen, bReceiveOpen, *BroadcastAddress, UdpPort);
}

void UPDURouterDISTanks::EvaluateLocalTankPublish()
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	ATankDISTanks* Tank = LocalTank.Get();
	if (!World || !Tank)
	{
		return;
	}

	const double NowWorldSeconds = World->GetTimeSeconds();
	const FVector LocationMeters = DISTanksProtocol::ToDISMeters(Tank->GetActorLocation());
	const FVector VelocityMetersPerSec = DISTanksProtocol::ToDISMeters(Tank->GetSimVelocityCmPerSec());
	const float YawDegrees = Tank->GetActorRotation().Yaw;

	bool bShouldPublish = !bHasPublishedBefore;
	if (!bShouldPublish)
	{
		// Mirror the receivers' constant-velocity prediction and publish only when reality drifts from it.
		const double ElapsedSeconds = NowWorldSeconds - LastSentWorldSeconds;
		const FVector PredictedLocationMeters = LastSentLocationMeters + LastSentVelocityMetersPerSec * ElapsedSeconds;
		bShouldPublish = ElapsedSeconds >= HeartbeatSeconds
			|| FVector::Dist(PredictedLocationMeters, LocationMeters) > PositionThresholdMeters
			|| FMath::Abs(FMath::FindDeltaAngleDegrees(LastSentYawDegrees, YawDegrees)) > YawThresholdDegrees;
	}

	if (bShouldPublish)
	{
		SendLocalTankPDU(NowWorldSeconds, LocationMeters, VelocityMetersPerSec, YawDegrees);
	}
}

void UPDURouterDISTanks::SendLocalTankPDU(double NowWorldSeconds, const FVector& LocationMeters, const FVector& VelocityMetersPerSec, float YawDegrees)
{
	UUDPSubsystem* UdpSubsystem = GetGameInstance()->GetSubsystem<UUDPSubsystem>();
	if (!UdpSubsystem)
	{
		return;
	}

	FEntityStatePDU TankPDU;
	TankPDU.ExerciseID = DISTanksProtocol::ExerciseID;
	TankPDU.Timestamp = FTimestamp::GenerateRelativeTimestamp(NowWorldSeconds);
	TankPDU.EntityID = LocalTankEntityID;
	TankPDU.ForceID = EForceID::Friendly;
	TankPDU.Marking = TEXT("TANK");
	TankPDU.EcefLocation = LocationMeters;
	TankPDU.EntityOrientation.Yaw = FMath::DegreesToRadians(YawDegrees);
	TankPDU.EntityLinearVelocity = VelocityMetersPerSec;
	TankPDU.EntityType.EntityKind = 1;
	TankPDU.EntityType.Domain = 1;
	TankPDU.DeadReckoningParameters.DeadReckoningAlgorithm = EDeadReckoningAlgorithm::FPW;

	UdpSubsystem->EmitBytes(TankPDU.ToBytes());

	LastSentLocationMeters = LocationMeters;
	LastSentVelocityMetersPerSec = VelocityMetersPerSec;
	LastSentYawDegrees = YawDegrees;
	LastSentWorldSeconds = NowWorldSeconds;
	bHasPublishedBefore = true;
}

void UPDURouterDISTanks::RemoveStaleGhosts(double NowWorldSeconds)
{
	for (auto GhostIterator = GhostTanks.CreateIterator(); GhostIterator; ++GhostIterator)
	{
		const bool bTimedOut = NowWorldSeconds - GhostIterator.Value().LastHeardWorldSeconds > GhostTimeoutSeconds;
		if (!bTimedOut)
		{
			continue;
		}

		if (ATankDISTanks* GhostTank = GhostIterator.Value().GhostTank.Get())
		{
			GhostTank->Destroy();
		}
		UE_LOG(LogDISTanks, Log, TEXT("GhostTankTimeout Entity=%s"), *DISTanksProtocol::EntityIDToString(GhostIterator.Key()));
		GhostIterator.RemoveCurrent();
	}
}

bool UPDURouterDISTanks::HandleTicker(float DeltaSeconds)
{
	EvaluateLocalTankPublish();

	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		RemoveStaleGhosts(World->GetTimeSeconds());
	}

	return true;
}
