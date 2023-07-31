// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassMovementFragments.h"
#include "MassSteeringFragments.generated.h"

class UWorld;

/** Steering fragment. */
USTRUCT()
struct MASSNAVIGATION_API FMassSteeringFragment : public FMassFragment
{
	GENERATED_BODY()

	void Reset()
	{
		DesiredVelocity = FVector::ZeroVector;
	}

	/** Cached desired velocity from steering. Note: not used for moving the entity. */
	FVector DesiredVelocity = FVector::ZeroVector;
};

/** Standing steering. */
USTRUCT()
struct MASSNAVIGATION_API FMassStandingSteeringFragment : public FMassFragment
{
	GENERATED_BODY()

	/** Selected steer target based on ghost, updates periodically. */
	FVector TargetLocation = FVector::ZeroVector;

	/** Used during target update to see when the target movement stops */
	float TrackedTargetSpeed = 0.0f;

	/** Cooldown between target updates */
	float TargetSelectionCooldown = 0.0f;

	/** True if the target is being updated */
	bool bIsUpdatingTarget = false;

	/** True if we just entered from move action */
	bool bEnteredFromMoveAction = false;
};


/** Steering related movement parameters. */
USTRUCT()
struct MASSNAVIGATION_API FMassMovingSteeringParameters : public FMassSharedFragment
{
	GENERATED_BODY()

	/** Steering reaction time in seconds. */
	UPROPERTY(config, EditAnywhere, Category = "Moving", meta = (ClampMin = "0.05", ForceUnits="s"))
	float ReactionTime = 0.3f;

	/** How much we look ahead when steering. Affects how steeply we steer towards the goal and when to start to slow down at the end of the path. */
	UPROPERTY(EditAnywhere, Category = "Moving", meta = (ClampMin = "0", ForceUnits="s"))
	float LookAheadTime = 1.0f;
};

USTRUCT()
struct FMassStandingSteeringParameters : public FMassSharedFragment
{
	GENERATED_BODY()

	/** Steering reaction time in seconds. */
	UPROPERTY(EditAnywhere, Category = "Standing", meta = (ClampMin = "0.05", ForceUnits="s"))
	float ReactionTime = 0.3f;

	/** How much the target should deviate from the ghost location before update */
	UPROPERTY(EditAnywhere, Category = "Standing", meta = (ClampMin = "0.05", ForceUnits="cm"))
	float TargetMoveThreshold = 15.0f;
	
	UPROPERTY(EditAnywhere, Category = "Standing")
	float TargetMoveThresholdVariance = 0.1f;

	/** If the velocity is below this threshold, it is clamped to 0. This allows to prevent jittery movement when trying to be stationary. */
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0", ForceUnits="cm/s"))
	float LowSpeedThreshold = 3.0f;

	/** How much the max speed can drop before we stop tracking it. */
	UPROPERTY(EditAnywhere, Category = "Standing", meta = (ClampMin = "0.05", ForceUnits="x"))
	float TargetSpeedHysteresisScale = 0.85f;

	/** Time between updates, varied randomly. */
	UPROPERTY(EditAnywhere, Category = "Standing", meta = (ClampMin = "0.05", ForceUnits="s"))
	float TargetSelectionCooldown = 1.5f;
	
	UPROPERTY(EditAnywhere, Category = "Standing")
	float TargetSelectionCooldownVariance = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Standing", meta = (ForceUnits="cm"))
	float DeadZoneRadius = 15.0f;
};
