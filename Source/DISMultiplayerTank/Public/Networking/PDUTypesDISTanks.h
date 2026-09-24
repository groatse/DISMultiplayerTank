#pragma once

#include "CoreMinimal.h"
#include "EnumsAndStructs/DISEnumsAndStructs.h"

/** Shared DIS conventions for this prototype: both peers ride arena-local coordinates (meters, Unreal axes) in the ECEF fields, a documented simplification instead of true geodesy. */
namespace DISTanksProtocol
{
	/** DIS exercise every instance of this game joins. */
	constexpr uint8 ExerciseID = 1;

	/** Common DIS site ID for all instances of this game. */
	constexpr int32 SiteID = 1;

	/** Entity number every instance uses for its own tank. */
	constexpr int32 TankEntityNumber = 1;

	/** Default UDP port for DIS traffic, overridable with -DISPort=. */
	constexpr int32 DefaultUdpPort = 3000;

	/** First entity number used for shells; numbers at or above this are munitions. */
	constexpr int32 ShellEntityNumberBase = 100;

	/** Returns true when an entity ID belongs to a shell rather than a tank. */
	inline bool IsShellEntity(const FEntityID& EntityID) { return EntityID.Entity >= ShellEntityNumberBase; }

	/** Converts Unreal centimeters to DIS meters. */
	inline FVector ToDISMeters(const FVector& UnrealCm) { return UnrealCm * 0.01; }

	/** Converts DIS meters to Unreal centimeters. */
	inline FVector ToUnrealCm(const FVector& DISMeters) { return DISMeters * 100.0; }

	/** Flattens a DIS relative timestamp to seconds within its hour for ordering. */
	inline double TimestampToSecondsInHour(const FTimestamp& Timestamp)
	{
		return Timestamp.Minutes * 60.0 + Timestamp.Seconds + Timestamp.Milliseconds / 1000.0 + Timestamp.Microseconds / 1000000.0;
	}

	/** Returns true when a candidate timestamp is newer than the reference, treating large negative gaps as hour wrap-around. */
	inline bool IsTimestampNewer(double CandidateSecondsInHour, double ReferenceSecondsInHour)
	{
		const double DifferenceSeconds = CandidateSecondsInHour - ReferenceSecondsInHour;
		return DifferenceSeconds > 0.0 || DifferenceSeconds < -1800.0;
	}

	/** Formats an entity ID as site:application:entity for log markers. */
	inline FString EntityIDToString(const FEntityID& EntityID)
	{
		return FString::Printf(TEXT("%d:%d:%d"), EntityID.Site, EntityID.Application, EntityID.Entity);
	}
}

/** Mirrors the receivers' constant-velocity prediction for one published entity and decides when a fresh ESPDU is due. */
struct FPublishTrackerDISTanks
{
	FVector LastLocationMeters = FVector::ZeroVector;
	FVector LastVelocityMetersPerSec = FVector::ZeroVector;
	float LastYawDegrees = 0.0f;
	double LastSentWorldSeconds = 0.0;
	bool bHasPublished = false;
	bool bLastSentDestroyed = false;

	/** Returns true when drift from the mirrored prediction, a destroyed-state flip, or the heartbeat requires publishing. */
	bool ShouldPublish(double NowWorldSeconds, const FVector& LocationMeters, float YawDegrees, bool bDestroyed, float HeartbeatSeconds, float PositionThresholdMeters, float YawThresholdDegrees) const
	{
		if (!bHasPublished || bDestroyed != bLastSentDestroyed)
		{
			return true;
		}

		const double ElapsedSeconds = NowWorldSeconds - LastSentWorldSeconds;
		const FVector PredictedLocationMeters = LastLocationMeters + LastVelocityMetersPerSec * ElapsedSeconds;
		return ElapsedSeconds >= HeartbeatSeconds
			|| FVector::Dist(PredictedLocationMeters, LocationMeters) > PositionThresholdMeters
			|| FMath::Abs(FMath::FindDeltaAngleDegrees(LastYawDegrees, YawDegrees)) > YawThresholdDegrees;
	}

	/** Records the state just published as the new prediction baseline. */
	void MarkPublished(double NowWorldSeconds, const FVector& LocationMeters, float YawDegrees, const FVector& VelocityMetersPerSec, bool bDestroyed)
	{
		LastLocationMeters = LocationMeters;
		LastVelocityMetersPerSec = VelocityMetersPerSec;
		LastYawDegrees = YawDegrees;
		LastSentWorldSeconds = NowWorldSeconds;
		bHasPublished = true;
		bLastSentDestroyed = bDestroyed;
	}
};
