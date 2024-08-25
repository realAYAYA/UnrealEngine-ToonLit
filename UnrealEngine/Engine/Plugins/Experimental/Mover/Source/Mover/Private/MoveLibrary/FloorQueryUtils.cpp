// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/MovementUtils.h"
#include "Engine/World.h"

namespace UE::FloorQueryUtility
{
	const float MIN_FLOOR_DIST = 1.9f;
	const float MAX_FLOOR_DIST = 2.4f;
	const float SWEEP_EDGE_REJECT_DISTANCE = 0.15f;
}

void UFloorQueryUtils::FindFloor(const USceneComponent* UpdatedComponent, const UPrimitiveComponent* UpdatedPrimitive, float FloorSweepDistance, float MaxWalkSlopeCosine, const FVector& Location, FFloorCheckResult& OutFloorResult)
{
	if (!UpdatedComponent || !UpdatedComponent->IsQueryCollisionEnabled())
	{
		OutFloorResult.Clear();
		return;
	}

	// Sweep for the floor
	// TODO: Might need to plug in a different value for LineTraceDistance - using the same value as FloorSweepDistance for now - function takes both so we can plug in different values if needed
	ComputeFloorDist(UpdatedComponent, UpdatedPrimitive,FloorSweepDistance, FloorSweepDistance, MaxWalkSlopeCosine, Location, OutFloorResult);
}

void UFloorQueryUtils::ComputeFloorDist(const USceneComponent* UpdatedComponent, const UPrimitiveComponent* UpdatedPrimitive, float LineTraceDistance, float FloorSweepDistance, float MaxWalkSlopeCosine, const FVector& Location, FFloorCheckResult& OutFloorResult)
{
	OutFloorResult.Clear();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ComputeFloorDist), false, UpdatedPrimitive->GetOwner());
	FCollisionResponseParams ResponseParam;
	UMovementUtils::InitCollisionParams(UpdatedPrimitive, QueryParams, ResponseParam);
	const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();

	// TODO: pluggable shapes
	float PawnRadius = 0.0f;
	float PawnHalfHeight = 0.0f;
	UpdatedPrimitive->CalcBoundingCylinder(PawnRadius, PawnHalfHeight);

	bool bBlockingHit = false;
	
	// Sweep test
	if (FloorSweepDistance > 0.f)
	{
		// Use a shorter height to avoid sweeps giving weird results if we start on a surface.
		// This also allows us to adjust out of penetrations.
		const float ShrinkScale = 0.9f;
		const float ShrinkScaleOverlap = 0.1f;
		float ShrinkHeight = (PawnHalfHeight - PawnRadius) * (1.f - ShrinkScale);
		float TraceDist = FloorSweepDistance + ShrinkHeight;
		FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(PawnRadius, PawnHalfHeight - ShrinkHeight);

		FHitResult Hit(1.f);
		// TODO: arbitrary direction
		FVector SweepDirection = FVector(0.f, 0.f, -TraceDist);
		bBlockingHit = FloorSweepTest(UpdatedPrimitive, Hit, Location, Location + SweepDirection, CollisionChannel, CapsuleShape, QueryParams, ResponseParam);

		if (bBlockingHit)
		{
			// Reject hits adjacent to us, we only care about hits on the bottom portion of our capsule.
			// Check 2D distance to impact point, reject if within a tolerance from radius.
			if (Hit.bStartPenetrating || !IsWithinEdgeTolerance(Location, Hit.ImpactPoint, CapsuleShape.Capsule.Radius))
			{
				// Use a capsule with a slightly smaller radius and shorter height to avoid the adjacent object.
				// Capsule must not be nearly zero or the trace will fall back to a line trace from the start point and have the wrong length.
				CapsuleShape.Capsule.Radius = FMath::Max(0.f, CapsuleShape.Capsule.Radius - UE::FloorQueryUtility::SWEEP_EDGE_REJECT_DISTANCE - KINDA_SMALL_NUMBER);
				if (!CapsuleShape.IsNearlyZero())
				{
					ShrinkHeight = (PawnHalfHeight - PawnRadius) * (1.f - ShrinkScaleOverlap);
					TraceDist = FloorSweepDistance + ShrinkHeight;
					SweepDirection = FVector(0.f, 0.f, -TraceDist);
					CapsuleShape.Capsule.HalfHeight = FMath::Max(PawnHalfHeight - ShrinkHeight, CapsuleShape.Capsule.Radius);
					Hit.Reset(1.f, false);

					bBlockingHit = FloorSweepTest(UpdatedPrimitive, Hit, Location, Location + SweepDirection, CollisionChannel, CapsuleShape, QueryParams, ResponseParam);
				}
			}

			// Reduce hit distance by ShrinkHeight because we shrank the capsule for the trace.
			// We allow negative distances here, because this allows us to pull out of penetrations.
			// JAH TODO: move magic numbers to a common location
			const float MaxPenetrationAdjust = FMath::Max(UE::FloorQueryUtility::MAX_FLOOR_DIST, PawnRadius);
			const float SweepResult = FMath::Max(-MaxPenetrationAdjust, Hit.Time * TraceDist - ShrinkHeight);

			OutFloorResult.SetFromSweep(Hit, SweepResult, false);
			if (Hit.IsValidBlockingHit() && IsHitSurfaceWalkable(Hit, MaxWalkSlopeCosine))
			{
				if (SweepResult <= FloorSweepDistance)
				{
					// Hit within test distance.
					OutFloorResult.bWalkableFloor = true;
					return;
				}
			}
		}
	}

	// Since we require a longer sweep than line trace, we don't want to run the line trace if the sweep missed everything.
	// We do however want to try a line trace if the sweep was stuck in penetration.
	if (!OutFloorResult.bBlockingHit && !OutFloorResult.HitResult.bStartPenetrating)
	{
		OutFloorResult.FloorDist = FloorSweepDistance;
		return;
	}

	// Line trace
	if (LineTraceDistance > 0.f)
	{
		const float ShrinkHeight = PawnHalfHeight;
		const FVector LineTraceStart = Location;	
		const float TraceDist = LineTraceDistance + ShrinkHeight;
		const FVector Down = FVector(0.f, 0.f, -TraceDist);
		QueryParams.TraceTag = SCENE_QUERY_STAT_NAME_ONLY(FloorLineTrace);

		FHitResult Hit(1.f);
		bBlockingHit = UpdatedComponent->GetWorld()->LineTraceSingleByChannel(Hit, LineTraceStart, LineTraceStart + Down, CollisionChannel, QueryParams, ResponseParam);
		
		if (bBlockingHit && Hit.Time > 0.f)
		{
			// Reduce hit distance by ShrinkHeight because we started the trace higher than the base.
			// We allow negative distances here, because this allows us to pull out of penetrations.
			const float MaxPenetrationAdjust = FMath::Max(UE::FloorQueryUtility::MAX_FLOOR_DIST, PawnRadius);
			const float LineResult = FMath::Max(-MaxPenetrationAdjust, Hit.Time * TraceDist - ShrinkHeight);
			
			OutFloorResult.bBlockingHit = true;
			if (LineResult <= LineTraceDistance && IsHitSurfaceWalkable(Hit, MaxWalkSlopeCosine))
			{
				OutFloorResult.SetFromLineTrace(Hit, OutFloorResult.FloorDist, LineResult, true);
				return;
			}
		}
	}

	// No hits were acceptable.
	OutFloorResult.bWalkableFloor = false;
}

bool UFloorQueryUtils::FloorSweepTest(const UPrimitiveComponent* UpdatedPrimitive, FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const struct FCollisionShape& CollisionShape, const struct FCollisionQueryParams& Params, const struct FCollisionResponseParams& ResponseParam)
{
	bool bBlockingHit = false;

	if (UpdatedPrimitive)
	{
		bBlockingHit = UpdatedPrimitive->GetWorld()->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, TraceChannel, CollisionShape, Params, ResponseParam);
	}

	return bBlockingHit;
}

bool UFloorQueryUtils::IsHitSurfaceWalkable(const FHitResult& Hit, float MaxWalkSlopeCosine)
{
	// JAH TODO: refactor this to support arbitrary movement planes (currently assumes it's the Z=0 plane)

	if (!Hit.IsValidBlockingHit())
	{
		// No hit, or starting in penetration
		return false;
	}

	// Never walk up vertical surfaces.
	if (Hit.ImpactNormal.Z < KINDA_SMALL_NUMBER)
	{
		return false;
	}

	float TestWalkableZ = MaxWalkSlopeCosine;

	// See if this component overrides the walkable floor z.
	const UPrimitiveComponent* HitComponent = Hit.Component.Get();
	if (HitComponent)
	{
		const FWalkableSlopeOverride& SlopeOverride = HitComponent->GetWalkableSlopeOverride();
		TestWalkableZ = SlopeOverride.ModifyWalkableFloorZ(TestWalkableZ);
	}

	// Can't walk on this surface if it is too steep.
	if (Hit.ImpactNormal.Z < TestWalkableZ)
	{
		return false;
	}

	return true;
}

bool UFloorQueryUtils::IsWithinEdgeTolerance(const FVector& CapsuleLocation, const FVector& TestImpactPoint, float CapsuleRadius)
{
	const float DistFromCenterSq = (TestImpactPoint - CapsuleLocation).SizeSquared2D();
	const float ReducedRadiusSq = FMath::Square(FMath::Max(UE::FloorQueryUtility::SWEEP_EDGE_REJECT_DISTANCE + UE_KINDA_SMALL_NUMBER, CapsuleRadius - UE::FloorQueryUtility::SWEEP_EDGE_REJECT_DISTANCE));
	return DistFromCenterSq < ReducedRadiusSq;
}

void FFloorCheckResult::SetFromSweep(const FHitResult& InHit, const float InSweepFloorDist, const bool bIsWalkableFloor)
{
	bBlockingHit = InHit.IsValidBlockingHit();
	bWalkableFloor = bIsWalkableFloor;
	FloorDist = InSweepFloorDist;
	HitResult = InHit;
	bLineTrace = false;
	LineDist = 0.f;
}

void FFloorCheckResult::SetFromLineTrace(const FHitResult& InHit, const float InSweepFloorDist, const float InLineDist, const bool bIsWalkableFloor)
{
	// We require a sweep that hit if we are going to use a line result.
	ensure(HitResult.bBlockingHit);
	if (HitResult.bBlockingHit && InHit.bBlockingHit)
	{
		// Override most of the sweep result with the line result, but save some values
		FHitResult OldHit(HitResult);
		HitResult = InHit;

		// Restore some of the old values. We want the new normals and hit actor, however.
		HitResult.Time = OldHit.Time;
		HitResult.ImpactPoint = OldHit.ImpactPoint;
		HitResult.Location = OldHit.Location;
		HitResult.TraceStart = OldHit.TraceStart;
		HitResult.TraceEnd = OldHit.TraceEnd;
		bLineTrace = true;
		LineDist = InLineDist;
		
		FloorDist = InSweepFloorDist;
		bWalkableFloor = bIsWalkableFloor;
	}
}
