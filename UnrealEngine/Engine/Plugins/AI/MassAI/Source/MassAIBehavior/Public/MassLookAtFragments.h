// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "MassCommonTypes.h"
#include "ZoneGraphTypes.h"
#include "Containers/StaticArray.h"
#include "MassLookAtFragments.generated.h"

/** Primary look at mode, gazing can be applied on top. */
UENUM()
enum class EMassLookAtMode : uint8
{
	/** Look forward */
	LookForward,
	/** Look along the current path */
	LookAlongPath,
	/** Track specified entity */
	LookAtEntity,
};

/** Gifferent gaze modes applied on top of the look at mode. */
UENUM()
enum class EMassLookAtGazeMode : uint8
{
	/** No gazing */
	None,
	/** Look constantly in gaze direction until next gaze target is picked. */
	Constant,
	/** Quick look at gaze target, ease in back to main look direction. */
	Glance,
};

/**
 * Struct that holds all parameters of the current entity look at 
 */
USTRUCT()
struct MASSAIBEHAVIOR_API FMassLookAtFragment : public FMassFragment
{
	GENERATED_BODY()

	FMassLookAtFragment()
		: bRandomGazeEntities(false)
	{
	}
	
	void Reset()
	{
		Direction = FVector::ForwardVector;
		GazeDirection = FVector::ForwardVector;
		TrackedEntity.Reset();
		GazeTrackedEntity.Reset();
		GazeStartTime = 0.0f;
		GazeDuration = 0.0f;
		LastSeenActionID = 0;
		LookAtMode = EMassLookAtMode::LookForward;
		RandomGazeMode = EMassLookAtGazeMode::None;
		RandomGazeYawVariation = 0;
		RandomGazePitchVariation = 0;
		bRandomGazeEntities = false;
	}

	/** Current look at direction (with gaze applied). */
	UPROPERTY(Transient)
	FVector Direction = FVector::ForwardVector;

	/** Current gaze direction, applied on top of look at direction based on gaze mode. */
	UPROPERTY(Transient)
	FVector GazeDirection = FVector::ForwardVector;

	/** Specific entity that is being tracked as primary look at. */
	UPROPERTY(Transient)
	FMassEntityHandle TrackedEntity;

	/** Entity that is tracked as part of gazing. */
	UPROPERTY(Transient)
	FMassEntityHandle GazeTrackedEntity;

	/** Start time of the current gaze. */
	UPROPERTY(Transient)
	float GazeStartTime = 0.0f;

	/** Duration of the current gaze. */
	UPROPERTY(Transient)
	float GazeDuration = 0.0f;

	/** Last seen action ID, used to check when look at trajectory needs to be updated. */
	UPROPERTY(Transient)
	uint16 LastSeenActionID = 0;

	/** Primary look at mode. */
	UPROPERTY(Transient)
	EMassLookAtMode LookAtMode = EMassLookAtMode::LookForward;

	/** Gaze look at mode. */
	UPROPERTY(Transient)
	EMassLookAtGazeMode RandomGazeMode = EMassLookAtGazeMode::None;

	/** Random gaze angle yaw variation (in degrees). */
	UPROPERTY(Transient)
	uint8 RandomGazeYawVariation = 0;

	/** Random gaze angle pitch variation (in degrees). */
	UPROPERTY(Transient)
	uint8 RandomGazePitchVariation = 0;

	/** Tru if random gaze can also pick interesting entities to look at. */
	UPROPERTY(Transient)
	uint8 bRandomGazeEntities : 1;
};

/**
 * Special tag to mark an entity that could be tracked by the LookAt
 */
USTRUCT()
struct MASSAIBEHAVIOR_API FMassLookAtTargetTag : public FMassTag
{
	GENERATED_BODY()
};


USTRUCT()
struct MASSAIBEHAVIOR_API FMassLookAtTrajectoryPoint
{
	GENERATED_BODY()

	void Set(const FVector InPosition, const FVector2D InTangent, const float InDistanceAlongLane)
	{
		Position = InPosition;
		Tangent.Set(InTangent);
		DistanceAlongLane.Set(InDistanceAlongLane);
	}
	
	/** Position of the path. */
	FVector Position = FVector::ZeroVector;
	
	/** Tangent direction of the path. */
	FMassSnorm8Vector2D Tangent;
	
	/** Position of the point along the original path. (Could potentially be uint16 at 10cm accuracy) */
	FMassInt16Real10 DistanceAlongLane = FMassInt16Real10(0.0f);
};

USTRUCT()
struct MASSAIBEHAVIOR_API FMassLookAtTrajectoryFragment : public FMassFragment
{
	GENERATED_BODY()

	FMassLookAtTrajectoryFragment() = default;
	
	static constexpr uint8 MaxPoints = 3;

	void Reset()
	{
		NumPoints = 0;
	}

	bool AddPoint(const FVector Position, const FVector2D Tangent, const float DistanceAlongLane)
	{
		if (NumPoints < MaxPoints)
		{
			FMassLookAtTrajectoryPoint& Point = Points[NumPoints++];
			Point.Set(Position, Tangent, DistanceAlongLane);
			return true;
		}
		return false;
	}

	FVector GetPointAtDistanceExtrapolated(const float DistanceAlongPath) const;
	
	/** Path points */
	TStaticArray<FMassLookAtTrajectoryPoint, MaxPoints> Points;

	/** Lane handle the trajectory was build for. */
	FZoneGraphLaneHandle LaneHandle;

	/** Number of points on path. */
	uint8 NumPoints = 0;

	bool bMoveReverse = false;
};
