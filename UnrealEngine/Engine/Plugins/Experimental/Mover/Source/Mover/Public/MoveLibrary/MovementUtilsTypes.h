// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovementUtilsTypes.generated.h"



UENUM()
enum class EMoveMixMode : uint8
{
	/** Velocity (linear and angular) is intended to be added with other sources */
	AdditiveVelocity = 0,
	/** Velocity (linear and angular) should override others */
	OverrideVelocity = 1,
	/** All move parameters should override others */
	OverrideAll      = 2,
};


/** Encapsulates info about an intended move that hasn't happened yet */
USTRUCT(BlueprintType)
struct MOVER_API FProposedMove
{
	GENERATED_USTRUCT_BODY()

	FProposedMove() : 
		bHasDirIntent(false),
		bHasTargetLocation(false)
	{}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	EMoveMixMode MixMode = EMoveMixMode::AdditiveVelocity;		// Determines how this move should resolve with other moves

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FName	PreferredMode = NAME_None;					// Indicates that we should switch to a particular movement mode before the next simulation step is performed.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	uint8	 bHasDirIntent : 1;							// Signals whether there was any directional intent specified
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	uint8	 bHasTargetLocation : 1;					// Signals whether the proposed move should move to a target location, regardless of other fields

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector  DirectionIntent = FVector::ZeroVector;		// Directional, per-axis magnitude [-1, 1] in world space (length of 1 indicates max speed intent). Only valid if bHasDirIntent is set.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector  LinearVelocity = FVector::ZeroVector;		// Units per second, world space, possibly mapped onto walking surface
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FRotator AngularVelocity = FRotator::ZeroRotator;	// Degrees per second, local space

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector  MovePlaneVelocity = FVector::ZeroVector;	// Units per second, world space, always along the movement plane

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector  TargetLocation = FVector::ZeroVector;		// World space go-to position. Only valid if bHasTargetLocation is set. Used for movement like teleportation.
};