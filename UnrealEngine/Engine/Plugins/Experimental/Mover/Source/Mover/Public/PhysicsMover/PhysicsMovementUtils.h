// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/ParticleHandleFwd.h"
#include "Containers/Array.h"
#include "Math/MathFwd.h"

struct FFloorCheckResult;
struct FWaterCheckResult;
struct FHitResult;
class UPrimitiveComponent;

/**
 * PhysicsMovementUtils: a collection of stateless static functions for a variety of physics movement-related operations
 */
class MOVER_API UPhysicsMovementUtils
{
public:
	static void FloorSweep(const FVector& Location, const FVector& DeltaPos, const UPrimitiveComponent* UpdatedPrimitive, const FVector& UpDir,
		float QueryRadius, float QueryDistance, float MaxWalkSlopeCosine, float TargetHeight, FFloorCheckResult& OutFloorResult, FWaterCheckResult& OutWaterResult);

	// If the hit result hit something, return the particle handle
	static const Chaos::FPBDRigidParticleHandle* GetRigidParticleHandleFromHitResult(const FHitResult& HitResult);

	// Checks if any hit is with water and, if so, fills in the OutWaterResult
	static bool GetWaterResultFromHitResults(const TArray<FHitResult>& Hits, const FVector& Location, const float TargetHeight, FWaterCheckResult& OutWaterResult);

	// Returns the current ground velocity at the character position
	static FVector ComputeGroundVelocityFromHitResult(const FVector& CharacterPosition, const FHitResult& FloorHit, const float DeltaSeconds);

	// Returns the integrated with gravity velocity of the ground at the character position
	static FVector ComputeIntegratedGroundVelocityFromHitResult(const FVector& CharacterPosition, const FHitResult& FloorHit, const float DeltaSeconds);
};