// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoveLibrary/AirMovementUtils.h"

#include "MoverComponent.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/MovementUtils.h"

FProposedMove UAirMovementUtils::ComputeControlledFreeMove(const FFreeMoveParams& InParams)
{
	FProposedMove OutMove;

	const FPlane MovementPlane(FVector::ZeroVector, FVector::UpVector);

	OutMove.DirectionIntent = UMovementUtils::ComputeDirectionIntent(InParams.MoveInput, InParams.MoveInputType);
	OutMove.bHasDirIntent = !OutMove.DirectionIntent.IsNearlyZero();

	FComputeVelocityParams ComputeVelocityParams;
	ComputeVelocityParams.DeltaSeconds = InParams.DeltaSeconds;
	ComputeVelocityParams.InitialVelocity = InParams.PriorVelocity;
	ComputeVelocityParams.MoveDirectionIntent = InParams.MoveInput;
	ComputeVelocityParams.MaxSpeed = InParams.MaxSpeed;
	ComputeVelocityParams.TurningBoost = InParams.TurningBoost;
	ComputeVelocityParams.Deceleration = InParams.Deceleration;
	ComputeVelocityParams.Acceleration = InParams.Acceleration;
	
	OutMove.LinearVelocity = UMovementUtils::ComputeVelocity(ComputeVelocityParams);
	OutMove.MovePlaneVelocity = UMovementUtils::ConstrainToPlane(OutMove.LinearVelocity, MovementPlane, false);

	// JAH TODO: this is where we can perform turning, based on aux settings. For now, just snap to the intended final orientation.
	FVector IntendedFacingDir = InParams.OrientationIntent.RotateVector(FVector::ForwardVector).GetSafeNormal();
	OutMove.AngularVelocity = UMovementUtils::ComputeAngularVelocity(InParams.PriorOrientation, IntendedFacingDir.ToOrientationRotator(), InParams.DeltaSeconds, InParams.TurningRate);

	return OutMove;
}

bool UAirMovementUtils::IsValidLandingSpot(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, const FVector& Location, const FHitResult& Hit, float FloorSweepDistance, float WalkableFloorZ, FFloorCheckResult& OutFloorResult)
{
	OutFloorResult.Clear();

	if (!Hit.bBlockingHit)
	{
		return false;
	}

	if (Hit.bStartPenetrating)
	{
		return false;
	}

	// Reject unwalkable floor normals.
	if (!UFloorQueryUtils::IsHitSurfaceWalkable(Hit, WalkableFloorZ))
	{
		return false;
	}

	// Make sure floor test passes here.
	UFloorQueryUtils::FindFloor(UpdatedComponent, UpdatedPrimitive, 
		FloorSweepDistance, WalkableFloorZ,
		Location, OutFloorResult);

	if (!OutFloorResult.IsWalkableFloor())
	{
		return false;
	}

	return true;
}

float UAirMovementUtils::TryMoveToFallAlongSurface(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, UMoverComponent* MoverComponent, const FVector& Delta, float PctOfDeltaToMove, const FQuat Rotation, const FVector& Normal, FHitResult& Hit, bool bHandleImpact, float FloorSweepDistance, float MaxWalkSlopeCosine, FFloorCheckResult& OutFloorResult, FMovementRecord& MoveRecord)
{
	OutFloorResult.Clear();

	if (!Hit.bBlockingHit)
	{
		return 0.f;
	}

	float PctOfTimeUsed = 0.f;
	const FVector OldHitNormal = Normal;

	FVector SlideDelta = UMovementUtils::ComputeSlideDelta(Delta, PctOfDeltaToMove, Normal, Hit);

	if ((SlideDelta | Delta) > 0.f)
	{
		// First sliding attempt along surface
		UMovementUtils::TrySafeMoveUpdatedComponent(UpdatedComponent, UpdatedPrimitive, SlideDelta, Rotation, true, Hit, ETeleportType::None, MoveRecord);

		PctOfTimeUsed = Hit.Time;
		if (Hit.IsValidBlockingHit())
		{
			// Notify first impact
			if (MoverComponent && bHandleImpact)
			{
				FMoverOnImpactParams ImpactParams(NAME_None, Hit, SlideDelta);
				MoverComponent->HandleImpact(ImpactParams);
			}

			// Check if we landed
			if (!IsValidLandingSpot(UpdatedComponent, UpdatedPrimitive, UpdatedPrimitive->GetComponentLocation(),
				Hit, FloorSweepDistance, MaxWalkSlopeCosine, OutFloorResult))
			{
				// We've hit another surface during our first move, so let's try to slide along both of them together

				// Compute new slide normal when hitting multiple surfaces.
				SlideDelta = UMovementUtils::ComputeTwoWallAdjustedDelta(SlideDelta, Hit, OldHitNormal);

				// Only proceed if the new direction is of significant length and not in reverse of original attempted move.
				if (!SlideDelta.IsNearlyZero(UE::MoverUtils::SMALL_MOVE_DISTANCE) && (SlideDelta | Delta) > 0.f)
				{
					// Perform second move, taking 2 walls into account
					UMovementUtils::TrySafeMoveUpdatedComponent(UpdatedComponent, UpdatedPrimitive, SlideDelta, Rotation, true, Hit, ETeleportType::None, MoveRecord);
					PctOfTimeUsed += (Hit.Time * (1.f - PctOfTimeUsed));

					// Notify second impact
					if (MoverComponent && bHandleImpact && Hit.bBlockingHit)
					{
						FMoverOnImpactParams ImpactParams(NAME_None, Hit, SlideDelta);
						MoverComponent->HandleImpact(ImpactParams);
					}

					// Check if we've landed, to acquire floor result
					IsValidLandingSpot(UpdatedComponent, UpdatedPrimitive, UpdatedPrimitive->GetComponentLocation(),
						Hit, FloorSweepDistance, MaxWalkSlopeCosine, OutFloorResult);
				}
			}
		}

		return FMath::Clamp(PctOfTimeUsed, 0.f, 1.f);
	}

	return 0.f;
}
