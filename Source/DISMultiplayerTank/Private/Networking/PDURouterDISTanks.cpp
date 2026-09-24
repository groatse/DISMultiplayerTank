#include "Networking/PDURouterDISTanks.h"

#include "DISMultiplayerTank.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PDUProcessor.h"
#include "Shells/ShellDISTanks.h"
#include "Tanks/TankDISTanks.h"
#include "UDPSubsystem.h"

void UPDURouterDISTanks::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency(UUDPSubsystem::StaticClass());
	Collection.InitializeDependency(UPDUProcessor::StaticClass());

	// Random application ID keeps two instances distinct until phase 5 negotiation replaces it.
	LocalTankEntityID = FEntityID(DISTanksProtocol::SiteID, FMath::RandRange(1, 65535), DISTanksProtocol::TankEntityNumber);
	NextShellEntityNumber = DISTanksProtocol::ShellEntityNumberBase;

	if (UPDUProcessor* Processor = GetGameInstance()->GetSubsystem<UPDUProcessor>())
	{
		Processor->OnEntityStatePDUProcessed.AddDynamic(this, &UPDURouterDISTanks::HandleEntityStatePDU);
		Processor->OnFirePDUProcessed.AddDynamic(this, &UPDURouterDISTanks::HandleFirePDU);
		Processor->OnDetonationPDUProcessed.AddDynamic(this, &UPDURouterDISTanks::HandleDetonationPDU);
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
		Processor->OnFirePDUProcessed.RemoveDynamic(this, &UPDURouterDISTanks::HandleFirePDU);
		Processor->OnDetonationPDUProcessed.RemoveDynamic(this, &UPDURouterDISTanks::HandleDetonationPDU);
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
	TankTracker = FPublishTrackerDISTanks();
}

void UPDURouterDISTanks::NotifyLocalShellFired(AShellDISTanks* NewLocalShell)
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World || !NewLocalShell)
	{
		return;
	}

	LocalShell = NewLocalShell;
	ShellTracker = FPublishTrackerDISTanks();
	LocalShellEntityID = FEntityID(DISTanksProtocol::SiteID, LocalTankEntityID.Application, NextShellEntityNumber);
	NextShellEntityNumber = NextShellEntityNumber >= 59999 ? DISTanksProtocol::ShellEntityNumberBase : NextShellEntityNumber + 1;

	const double NowWorldSeconds = World->GetTimeSeconds();

	FFirePDU FirePDU;
	FirePDU.ExerciseID = DISTanksProtocol::ExerciseID;
	FirePDU.Timestamp = FTimestamp::GenerateRelativeTimestamp(NowWorldSeconds);
	FirePDU.FiringEntityID = LocalTankEntityID;
	FirePDU.MunitionEntityID = LocalShellEntityID;
	FirePDU.EventID = NextEventID();
	FirePDU.EcefLocation = DISTanksProtocol::ToDISMeters(NewLocalShell->GetActorLocation());
	FirePDU.Velocity = DISTanksProtocol::ToDISMeters(NewLocalShell->GetFlightVelocityCmPerSec());
	EmitPDUBytes(FirePDU.ToBytes());

	UE_LOG(LogDISTanks, Log, TEXT("FirePublished Shooter=%s Munition=%s"), *DISTanksProtocol::EntityIDToString(LocalTankEntityID), *DISTanksProtocol::EntityIDToString(LocalShellEntityID));

	EvaluateLocalShellPublish();
}

void UPDURouterDISTanks::NotifyLocalShellDetonated(AShellDISTanks* DetonatedShell, const FVector& ImpactLocationCm, AActor* HitActor)
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World || DetonatedShell != LocalShell.Get())
	{
		return;
	}

	FEntityID TargetEntityID;
	const bool bHitGhostTank = FindGhostTankEntityID(HitActor, TargetEntityID);

	FDetonationPDU DetonationPDU;
	DetonationPDU.ExerciseID = DISTanksProtocol::ExerciseID;
	DetonationPDU.Timestamp = FTimestamp::GenerateRelativeTimestamp(World->GetTimeSeconds());
	DetonationPDU.FiringEntityID = LocalTankEntityID;
	DetonationPDU.MunitionEntityID = LocalShellEntityID;
	DetonationPDU.TargetEntityID = TargetEntityID;
	DetonationPDU.EventID = NextEventID();
	DetonationPDU.EcefLocation = DISTanksProtocol::ToDISMeters(ImpactLocationCm);
	DetonationPDU.DetonationResult = bHitGhostTank ? EDetonationResult::EntityImpact : (HitActor ? EDetonationResult::EnvironmentObjectImpact : EDetonationResult::Dud);
	EmitPDUBytes(DetonationPDU.ToBytes());

	UE_LOG(LogDISTanks, Log, TEXT("DetonationPublished Munition=%s Target=%s Result=%d"), *DISTanksProtocol::EntityIDToString(LocalShellEntityID), *DISTanksProtocol::EntityIDToString(TargetEntityID), static_cast<int32>(DetonationPDU.DetonationResult));

	LocalShell = nullptr;
}

int32 UPDURouterDISTanks::GetInterimSlotIndex() const
{
	int32 SlotIndex = LocalTankEntityID.Application % 2;
	FParse::Value(FCommandLine::Get(), TEXT("PlayerSlot="), SlotIndex);
	return SlotIndex;
}

void UPDURouterDISTanks::HandleEntityStatePDU(FEntityStatePDU EntityStatePDU)
{
	if (EntityStatePDU.ExerciseID != DISTanksProtocol::ExerciseID || EntityStatePDU.EntityID.Application == LocalTankEntityID.Application)
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

	if (DISTanksProtocol::IsShellEntity(EntityStatePDU.EntityID))
	{
		HandleRemoteShellState(EntityStatePDU, NowWorldSeconds, PduSecondsInHour);
	}
	else
	{
		HandleRemoteTankState(EntityStatePDU, NowWorldSeconds, PduSecondsInHour);
	}
}

void UPDURouterDISTanks::HandleFirePDU(FFirePDU FirePDU)
{
	if (FirePDU.ExerciseID != DISTanksProtocol::ExerciseID || FirePDU.FiringEntityID.Application == LocalTankEntityID.Application)
	{
		return;
	}

	UE_LOG(LogDISTanks, Log, TEXT("FireReceived Shooter=%s Munition=%s"), *DISTanksProtocol::EntityIDToString(FirePDU.FiringEntityID), *DISTanksProtocol::EntityIDToString(FirePDU.MunitionEntityID));

	// Seed the ghost shell immediately so remote shots appear one latency earlier than their first ESPDU.
	FEntityStatePDU SeedState;
	SeedState.ExerciseID = FirePDU.ExerciseID;
	SeedState.Timestamp = FirePDU.Timestamp;
	SeedState.EntityID = FirePDU.MunitionEntityID;
	SeedState.EcefLocation = FirePDU.EcefLocation;
	SeedState.EntityLinearVelocity = FirePDU.Velocity;
	SeedState.EntityOrientation.Yaw = FMath::Atan2(FirePDU.Velocity.Y, FirePDU.Velocity.X);

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (World)
	{
		HandleRemoteShellState(SeedState, World->GetTimeSeconds(), DISTanksProtocol::TimestampToSecondsInHour(FirePDU.Timestamp));
	}
}

void UPDURouterDISTanks::HandleDetonationPDU(FDetonationPDU DetonationPDU)
{
	if (DetonationPDU.ExerciseID != DISTanksProtocol::ExerciseID || DetonationPDU.MunitionEntityID.Application == LocalTankEntityID.Application)
	{
		return;
	}

	UE_LOG(LogDISTanks, Log, TEXT("DetonationReceived Munition=%s Target=%s"), *DISTanksProtocol::EntityIDToString(DetonationPDU.MunitionEntityID), *DISTanksProtocol::EntityIDToString(DetonationPDU.TargetEntityID));

	// The shooter's shell is spent; remove its ghost right away.
	if (FGhostEntryDISTanks* ShellEntry = GhostShells.Find(DetonationPDU.MunitionEntityID))
	{
		if (AActor* GhostShell = ShellEntry->GhostActor.Get())
		{
			GhostShell->Destroy();
		}
		GhostShells.Remove(DetonationPDU.MunitionEntityID);
	}

	// Victim-side adjudication: the kill only counts if the detonation lands close to where we really are.
	ATankDISTanks* Tank = LocalTank.Get();
	if (!Tank || DetonationPDU.TargetEntityID != LocalTankEntityID)
	{
		return;
	}

	const FVector DetonationLocationCm = DISTanksProtocol::ToUnrealCm(DetonationPDU.EcefLocation);
	const float DistanceCm = FVector::Dist(DetonationLocationCm, Tank->GetActorLocation());

	if (Tank->IsDeathSequenceActive())
	{
		UE_LOG(LogDISTanks, Log, TEXT("DetonationRejected Reason=AlreadyDying Distance=%.0f"), DistanceCm);
		return;
	}

	if (DistanceCm > DetonationToleranceCm)
	{
		UE_LOG(LogDISTanks, Log, TEXT("DetonationRejected Reason=OutOfTolerance Distance=%.0f"), DistanceCm);
		return;
	}

	UE_LOG(LogDISTanks, Log, TEXT("KillConfirmed Victim=%s Killer=%s Distance=%.0f"), *DISTanksProtocol::EntityIDToString(LocalTankEntityID), *DISTanksProtocol::EntityIDToString(DetonationPDU.FiringEntityID), DistanceCm);
	Tank->HandleConfirmedKill();
	EvaluateLocalTankPublish();
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
	const bool bDestroyed = Tank->IsDeathSequenceActive();

	if (!TankTracker.ShouldPublish(NowWorldSeconds, LocationMeters, YawDegrees, bDestroyed, TankHeartbeatSeconds, PositionThresholdMeters, YawThresholdDegrees))
	{
		return;
	}

	FEntityStatePDU TankPDU = BuildEntityStatePDU(LocalTankEntityID, NowWorldSeconds, LocationMeters, YawDegrees, VelocityMetersPerSec);
	TankPDU.Marking = TEXT("TANK");
	TankPDU.EntityType.EntityKind = 1;
	TankPDU.EntityType.Domain = 1;
	TankPDU.EntityAppearance.Damage = bDestroyed ? EEntityDamage::Destroyed : EEntityDamage::NoDamage;
	EmitPDUBytes(TankPDU.ToBytes());

	TankTracker.MarkPublished(NowWorldSeconds, LocationMeters, YawDegrees, VelocityMetersPerSec, bDestroyed);
}

void UPDURouterDISTanks::EvaluateLocalShellPublish()
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	AShellDISTanks* Shell = LocalShell.Get();
	if (!World || !Shell)
	{
		return;
	}

	const double NowWorldSeconds = World->GetTimeSeconds();
	const FVector LocationMeters = DISTanksProtocol::ToDISMeters(Shell->GetActorLocation());
	const FVector VelocityMetersPerSec = DISTanksProtocol::ToDISMeters(Shell->GetFlightVelocityCmPerSec());
	const float YawDegrees = Shell->GetActorRotation().Yaw;

	if (!ShellTracker.ShouldPublish(NowWorldSeconds, LocationMeters, YawDegrees, false, ShellHeartbeatSeconds, PositionThresholdMeters, YawThresholdDegrees))
	{
		return;
	}

	FEntityStatePDU ShellPDU = BuildEntityStatePDU(LocalShellEntityID, NowWorldSeconds, LocationMeters, YawDegrees, VelocityMetersPerSec);
	ShellPDU.Marking = TEXT("SHELL");
	ShellPDU.EntityType.EntityKind = 2;
	EmitPDUBytes(ShellPDU.ToBytes());

	ShellTracker.MarkPublished(NowWorldSeconds, LocationMeters, YawDegrees, VelocityMetersPerSec, false);
}

void UPDURouterDISTanks::HandleRemoteTankState(const FEntityStatePDU& EntityStatePDU, double NowWorldSeconds, double PduSecondsInHour)
{
	FGhostEntryDISTanks& GhostEntry = GhostTanks.FindOrAdd(EntityStatePDU.EntityID);
	if (GhostEntry.GhostActor.IsValid() && !DISTanksProtocol::IsTimestampNewer(PduSecondsInHour, GhostEntry.LastTimestampSecondsInHour))
	{
		return;
	}

	const FVector GhostLocationCm = DISTanksProtocol::ToUnrealCm(EntityStatePDU.EcefLocation);
	const float GhostYawDegrees = FMath::RadiansToDegrees(EntityStatePDU.EntityOrientation.Yaw);

	ATankDISTanks* GhostTank = Cast<ATankDISTanks>(GhostEntry.GhostActor.Get());
	if (!GhostTank)
	{
		UWorld* World = GetGameInstance()->GetWorld();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		GhostTank = World->SpawnActor<ATankDISTanks>(ATankDISTanks::StaticClass(), FTransform(FRotator(0.0f, GhostYawDegrees, 0.0f), GhostLocationCm), SpawnParameters);
		if (!GhostTank)
		{
			return;
		}
		GhostTank->InitAsGhost();
		GhostEntry.GhostActor = GhostTank;
		UE_LOG(LogDISTanks, Log, TEXT("GhostTankSpawned Entity=%s Location=%s"), *DISTanksProtocol::EntityIDToString(EntityStatePDU.EntityID), *GhostLocationCm.ToCompactString());
	}

	GhostTank->ApplyRemoteTankState(GhostLocationCm, GhostYawDegrees, DISTanksProtocol::ToUnrealCm(EntityStatePDU.EntityLinearVelocity), NowWorldSeconds);
	GhostTank->SetDestroyedVisual(EntityStatePDU.EntityAppearance.Damage == EEntityDamage::Destroyed);
	GhostEntry.LastTimestampSecondsInHour = PduSecondsInHour;
	GhostEntry.LastHeardWorldSeconds = NowWorldSeconds;
}

void UPDURouterDISTanks::HandleRemoteShellState(const FEntityStatePDU& EntityStatePDU, double NowWorldSeconds, double PduSecondsInHour)
{
	FGhostEntryDISTanks& GhostEntry = GhostShells.FindOrAdd(EntityStatePDU.EntityID);
	if (GhostEntry.GhostActor.IsValid() && !DISTanksProtocol::IsTimestampNewer(PduSecondsInHour, GhostEntry.LastTimestampSecondsInHour))
	{
		return;
	}

	const FVector GhostLocationCm = DISTanksProtocol::ToUnrealCm(EntityStatePDU.EcefLocation);
	const float GhostYawDegrees = FMath::RadiansToDegrees(EntityStatePDU.EntityOrientation.Yaw);

	AShellDISTanks* GhostShell = Cast<AShellDISTanks>(GhostEntry.GhostActor.Get());
	if (!GhostShell)
	{
		UWorld* World = GetGameInstance()->GetWorld();
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		GhostShell = World->SpawnActor<AShellDISTanks>(AShellDISTanks::StaticClass(), FTransform(FRotator(0.0f, GhostYawDegrees, 0.0f), GhostLocationCm), SpawnParameters);
		if (!GhostShell)
		{
			return;
		}
		GhostShell->InitAsGhost();
		GhostEntry.GhostActor = GhostShell;
		UE_LOG(LogDISTanks, Log, TEXT("GhostShellSpawned Entity=%s Location=%s"), *DISTanksProtocol::EntityIDToString(EntityStatePDU.EntityID), *GhostLocationCm.ToCompactString());
	}

	GhostShell->ApplyRemoteShellState(GhostLocationCm, GhostYawDegrees, DISTanksProtocol::ToUnrealCm(EntityStatePDU.EntityLinearVelocity), NowWorldSeconds);
	GhostEntry.LastTimestampSecondsInHour = PduSecondsInHour;
	GhostEntry.LastHeardWorldSeconds = NowWorldSeconds;
}

void UPDURouterDISTanks::RemoveStaleGhosts(double NowWorldSeconds)
{
	for (auto GhostIterator = GhostTanks.CreateIterator(); GhostIterator; ++GhostIterator)
	{
		if (NowWorldSeconds - GhostIterator.Value().LastHeardWorldSeconds <= GhostTankTimeoutSeconds)
		{
			continue;
		}

		if (AActor* GhostActor = GhostIterator.Value().GhostActor.Get())
		{
			GhostActor->Destroy();
		}
		UE_LOG(LogDISTanks, Log, TEXT("GhostTankTimeout Entity=%s"), *DISTanksProtocol::EntityIDToString(GhostIterator.Key()));
		GhostIterator.RemoveCurrent();
	}

	for (auto GhostIterator = GhostShells.CreateIterator(); GhostIterator; ++GhostIterator)
	{
		if (NowWorldSeconds - GhostIterator.Value().LastHeardWorldSeconds <= GhostShellTimeoutSeconds)
		{
			continue;
		}

		if (AActor* GhostActor = GhostIterator.Value().GhostActor.Get())
		{
			GhostActor->Destroy();
		}
		UE_LOG(LogDISTanks, Log, TEXT("GhostShellTimeout Entity=%s"), *DISTanksProtocol::EntityIDToString(GhostIterator.Key()));
		GhostIterator.RemoveCurrent();
	}
}

bool UPDURouterDISTanks::HandleTicker(float DeltaSeconds)
{
	EvaluateLocalTankPublish();
	EvaluateLocalShellPublish();

	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		RemoveStaleGhosts(World->GetTimeSeconds());
	}

	return true;
}

FEntityStatePDU UPDURouterDISTanks::BuildEntityStatePDU(const FEntityID& EntityID, double NowWorldSeconds, const FVector& LocationMeters, float YawDegrees, const FVector& VelocityMetersPerSec) const
{
	FEntityStatePDU StatePDU;
	StatePDU.ExerciseID = DISTanksProtocol::ExerciseID;
	StatePDU.Timestamp = FTimestamp::GenerateRelativeTimestamp(NowWorldSeconds);
	StatePDU.EntityID = EntityID;
	StatePDU.ForceID = EForceID::Friendly;
	StatePDU.EcefLocation = LocationMeters;
	StatePDU.EntityOrientation.Yaw = FMath::DegreesToRadians(YawDegrees);
	StatePDU.EntityLinearVelocity = VelocityMetersPerSec;
	StatePDU.DeadReckoningParameters.DeadReckoningAlgorithm = EDeadReckoningAlgorithm::FPW;
	return StatePDU;
}

void UPDURouterDISTanks::EmitPDUBytes(const TArray<uint8>& PduBytes)
{
	if (UUDPSubsystem* UdpSubsystem = GetGameInstance()->GetSubsystem<UUDPSubsystem>())
	{
		UdpSubsystem->EmitBytes(PduBytes);
	}
}

bool UPDURouterDISTanks::FindGhostTankEntityID(const AActor* GhostActor, FEntityID& OutEntityID) const
{
	if (!GhostActor)
	{
		return false;
	}

	for (const TPair<FEntityID, FGhostEntryDISTanks>& GhostPair : GhostTanks)
	{
		if (GhostPair.Value.GhostActor.Get() == GhostActor)
		{
			OutEntityID = GhostPair.Key;
			return true;
		}
	}
	return false;
}

FEventID UPDURouterDISTanks::NextEventID()
{
	FEventID EventID;
	EventID.Site = DISTanksProtocol::SiteID;
	EventID.Application = LocalTankEntityID.Application;
	EventID.EventNumber = NextEventNumber++;
	return EventID;
}
