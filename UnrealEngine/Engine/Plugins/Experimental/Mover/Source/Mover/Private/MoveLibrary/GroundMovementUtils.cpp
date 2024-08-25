// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoveLibrary/GroundMovementUtils.h"

#include "MoverComponent.h"
#include "MoverLog.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Pawn.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/MovementRecord.h"
#include "MoveLibrary/MovementUtils.h"

FProposedMove UGroundMovementUtils::ComputeControlledGroundMove(const FGroundMoveParams& InParams)
{
	FProposedMove OutMove;

	const FVector MoveDirIntent = UMovementUtils::ComputeDirectionIntent(InParams.MoveInput, InParams.MoveInputType);

	const FPlane MovementPlane(FVector::ZeroVector, FVector::UpVector);
	FVector MoveDirIntentInMovementPlane = UMovementUtils::ConstrainToPlane(MoveDirIntent, MovementPlane, true);

	const FPlane GroundSurfacePlane(FVector::ZeroVector, InParams.GroundNormal);
	OutMove.DirectionIntent = UMovementUtils::ConstrainToPlane(MoveDirIntentInMovementPlane, GroundSurfacePlane, true);
	
	OutMove.bHasDirIntent = !OutMove.DirectionIntent.IsNearlyZero();

	FComputeVelocityParams ComputeVelocityParams;
	ComputeVelocityParams.DeltaSeconds = InParams.DeltaSeconds;
	ComputeVelocityParams.InitialVelocity = InParams.PriorVelocity;
	ComputeVelocityParams.MoveDirectionIntent = InParams.MoveInput;
	ComputeVelocityParams.MaxSpeed = InParams.MaxSpeed;
	ComputeVelocityParams.TurningBoost = InParams.TurningBoost;
	ComputeVelocityParams.Deceleration = InParams.Deceleration;
	ComputeVelocityParams.Acceleration = InParams.Acceleration;
	ComputeVelocityParams.Friction = InParams.Friction;
	
	// Figure out linear velocity
	OutMove.MovePlaneVelocity = UMovementUtils::ComputeVelocity(ComputeVelocityParams);
	OutMove.LinearVelocity = UMovementUtils::ConstrainToPlane(OutMove.MovePlaneVelocity, GroundSurfacePlane, true);

	// Linearly rotate in place
	OutMove.AngularVelocity = UMovementUtils::ComputeAngularVelocity(InParams.PriorOrientation, InParams.OrientationIntent, InParams.DeltaSeconds, InParams.TurningRate);

	return OutMove;
}

static const FName StepUpSubstepName = "StepUp";
static const FName StepFwdSubstepName = "StepFwd";
static const FName StepDownSubstepName = "StepDown";
static const FName SlideSubstepName = "SlideFromStep";

bool UGroundMovementUtils::TryMoveToStepUp(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, UMoverComponent* MoverComponent, const FVector& GravDir, float MaxStepHeight, float MaxWalkSlopeCosine, float FloorSweepDistance, const FVector& MoveDelta, const FHitResult& MoveHitResult, const FFloorCheckResult& CurrentFloor, bool bIsFalling, FOptionalFloorCheckResult* OutFloorTestResult, FMovementRecord& MoveRecord)
{
	UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(UpdatedPrimitive);

	if (CapsuleComponent == nullptr || !CanStepUpOnHitSurface(MoveHitResult) || MaxStepHeight <= 0.f)
	{
		return false;
	}

	TArray<FMovementSubstep> QueuedSubsteps;	// keeping track of substeps before committing, because some moves can be backed out


	const FVector OldLocation = UpdatedPrimitive->GetComponentLocation();
	FVector LastComponentLocation = OldLocation;

	float PawnRadius, PawnHalfHeight;

	CapsuleComponent->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

	// Don't bother stepping up if top of capsule is hitting something.
	const float InitialImpactZ = MoveHitResult.ImpactPoint.Z;
	if (InitialImpactZ > OldLocation.Z + (PawnHalfHeight - PawnRadius))
	{
		UE_LOG(LogMover, VeryVerbose, TEXT("Not stepping up due to top of capsule hitting something"));
		return false;
	}

	// TODO: We should rely on movement plane normal, rather than gravity direction
	if (GravDir.IsZero())
	{
		UE_LOG(LogMover, VeryVerbose, TEXT("Not stepping up because there's no gravity"));
		return false;
	}

	// Gravity should be a normalized direction
	ensure(GravDir.IsNormalized());

	float StepTravelUpHeight = MaxStepHeight;
	float StepTravelDownHeight = StepTravelUpHeight;
	const float StepSideZ = -1.f * FVector::DotProduct(MoveHitResult.ImpactNormal, GravDir);
	float PawnInitialFloorBaseZ = OldLocation.Z - PawnHalfHeight;
	float PawnFloorPointZ = PawnInitialFloorBaseZ;

	//if (IsMovingOnGround() && CurrentFloor.IsWalkableFloor())
	if (CurrentFloor.IsWalkableFloor())
	{
		// Since we float a variable amount off the floor, we need to enforce max step height off the actual point of impact with the floor.
		const float FloorDist = FMath::Max(0.f, CurrentFloor.GetDistanceToFloor());
		PawnInitialFloorBaseZ -= FloorDist;
		StepTravelUpHeight = FMath::Max(StepTravelUpHeight - FloorDist, 0.f);
		StepTravelDownHeight = (MaxStepHeight + UE::FloorQueryUtility::MAX_FLOOR_DIST * 2.f);

		const bool bHitVerticalFace = !UFloorQueryUtils::IsWithinEdgeTolerance(MoveHitResult.Location, MoveHitResult.ImpactPoint, PawnRadius);
		if (!CurrentFloor.bLineTrace && !bHitVerticalFace)
		{
			PawnFloorPointZ = CurrentFloor.HitResult.ImpactPoint.Z;
		}
		else
		{
			// Base floor point is the base of the capsule moved down by how far we are hovering over the surface we are hitting.
			PawnFloorPointZ -= CurrentFloor.FloorDist;
		}
	}

	// Don't step up if the impact is below us, accounting for distance from floor.
	if (InitialImpactZ <= PawnInitialFloorBaseZ)
	{
		UE_LOG(LogMover, VeryVerbose, TEXT("Not stepping up because the impact is below us"));
		return false;
	}

	// Scope our movement updates, and do not apply them until all intermediate moves are completed.
	FScopedMovementUpdate ScopedStepUpMovement(UpdatedComponent, EScopedUpdate::DeferredUpdates);

	// step up - treat as vertical wall
	FHitResult SweepUpHit(1.f);
	const FQuat PawnRotation = UpdatedComponent->GetComponentQuat();

	const FVector UpAdjustment = -GravDir * StepTravelUpHeight;
	const bool bDidStepUp = UMovementUtils::TryMoveUpdatedComponent_Internal(UpdatedComponent, UpAdjustment, PawnRotation, true, MOVECOMP_NoFlags, &SweepUpHit, ETeleportType::None);

	UE_LOG(LogMover, VeryVerbose, TEXT("TryMoveToStepUp Up: %s (role %i) UpAdjustment=%s DidMove=%i"),
		*GetNameSafe(UpdatedComponent->GetOwner()), UpdatedComponent->GetOwnerRole(), *UpAdjustment.ToCompactString(), bDidStepUp);

	if (SweepUpHit.bStartPenetrating)
	{
		// Undo movement
		UE_LOG(LogMover, VeryVerbose, TEXT("Reverting step-up attempt because we started in a penetrating state"));
		ScopedStepUpMovement.RevertMove();
		return false;
	}

	// Cache upwards substep
	QueuedSubsteps.Add(FMovementSubstep(StepUpSubstepName, UpdatedPrimitive->GetComponentLocation()-LastComponentLocation, false));
	LastComponentLocation = UpdatedPrimitive->GetComponentLocation();

	// step fwd
	FHitResult StepFwdHit(1.f);
	const bool bDidStepFwd = UMovementUtils::TryMoveUpdatedComponent_Internal(UpdatedComponent, MoveDelta, PawnRotation, true, MOVECOMP_NoFlags, &StepFwdHit, ETeleportType::None);

	UE_LOG(LogMover, VeryVerbose, TEXT("TryMoveToStepUp Fwd: %s (role %i) MoveDelta=%s DidMove=%i"),
		*GetNameSafe(UpdatedComponent->GetOwner()), UpdatedComponent->GetOwnerRole(), *MoveDelta.ToCompactString(), bDidStepFwd);

	// Check result of forward movement
	if (StepFwdHit.bBlockingHit)
	{
		if (StepFwdHit.bStartPenetrating)
		{
			// Undo movement
			UE_LOG(LogMover, VeryVerbose, TEXT("Reverting step-fwd attempt during step-up, because we started in a penetrating state"));
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// If we hit something above us and also something ahead of us, we should notify about the upward hit as well.
		// The forward hit will be handled later (in the bSteppedOver case below).
		// In the case of hitting something above but not forward, we are not blocked from moving so we don't need the notification.
		if (MoverComponent && SweepUpHit.bBlockingHit && StepFwdHit.bBlockingHit)
		{
			FMoverOnImpactParams ImpactParams(NAME_None, SweepUpHit, MoveDelta);
			MoverComponent->HandleImpact(ImpactParams);
		}

		// pawn ran into a wall
		if (MoverComponent)
		{
			FMoverOnImpactParams ImpactParams(NAME_None, StepFwdHit, MoveDelta);
			MoverComponent->HandleImpact(ImpactParams);
		}
		
		if (bIsFalling)
		{
			QueuedSubsteps.Add( FMovementSubstep(StepFwdSubstepName, UpdatedComponent->GetComponentLocation()-LastComponentLocation, true) );

			// Commit queued substeps to movement record
			for (FMovementSubstep Substep : QueuedSubsteps)
			{
				MoveRecord.Append(Substep);
			}

			return true;
		}

		// Cache forwards substep before the slide attempt
		QueuedSubsteps.Add(FMovementSubstep(StepFwdSubstepName, UpdatedPrimitive->GetComponentLocation() - LastComponentLocation, true));
		LastComponentLocation = UpdatedPrimitive->GetComponentLocation();

		// adjust and try again
		const float ForwardHitTime = StepFwdHit.Time;

		// locking relevancy so velocity isn't added until it is needed to (adding it to the QueuedSubsteps so it can get added later)
		MoveRecord.LockRelevancy(false);
		const float ForwardSlideAmount = TryWalkToSlideAlongSurface(UpdatedComponent, UpdatedPrimitive, MoverComponent, MoveDelta, 1.f - StepFwdHit.Time, PawnRotation, StepFwdHit.Normal, StepFwdHit, true, MoveRecord, MaxWalkSlopeCosine, MaxStepHeight);
		QueuedSubsteps.Add( FMovementSubstep(SlideSubstepName, UpdatedComponent->GetComponentLocation()-LastComponentLocation, true) );
		LastComponentLocation = UpdatedPrimitive->GetComponentLocation();
		MoveRecord.UnlockRelevancy();

		if (bIsFalling)
		{
			UE_LOG(LogMover, VeryVerbose, TEXT("Reverting step-fwd attempt during step-up, because we could not adjust without falling"));
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// If both the forward hit and the deflection got us nowhere, there is no point in this step up.
		if (ForwardHitTime == 0.f && ForwardSlideAmount == 0.f)
		{
			UE_LOG(LogMover, VeryVerbose, TEXT("Reverting step-fwd attempt during step-up, because no movement differences occurred"));
			ScopedStepUpMovement.RevertMove();
			return false;
		}
	}
	else
	{
		// Our forward move attempt was unobstructed - cache it
		QueuedSubsteps.Add(FMovementSubstep(StepFwdSubstepName, UpdatedPrimitive->GetComponentLocation() - LastComponentLocation, true));
		LastComponentLocation = UpdatedPrimitive->GetComponentLocation();
	}


	// Step down
	const FVector StepDownAdjustment = GravDir * StepTravelDownHeight;
	const bool bDidStepDown = UMovementUtils::TryMoveUpdatedComponent_Internal(UpdatedComponent, StepDownAdjustment, UpdatedComponent->GetComponentQuat(), true, MOVECOMP_NoFlags, &StepFwdHit, ETeleportType::None);

	UE_LOG(LogMover, VeryVerbose, TEXT("TryMoveToStepUp Down: %s (role %i) StepDownAdjustment=%s DidMove=%i"),
		*GetNameSafe(UpdatedComponent->GetOwner()), UpdatedComponent->GetOwnerRole(), *StepDownAdjustment.ToCompactString(), bDidStepDown);


	// If step down was initially penetrating abort the step up
	if (StepFwdHit.bStartPenetrating)
	{
		UE_LOG(LogMover, VeryVerbose, TEXT("Reverting step-down attempt during step-up/step-fwd, because we started in a penetrating state"));
		ScopedStepUpMovement.RevertMove();
		return false;
	}

	FOptionalFloorCheckResult StepDownResult;
	if (StepFwdHit.IsValidBlockingHit())
	{
		// See if this step sequence would have allowed us to travel higher than our max step height allows.
		const float DeltaZ = StepFwdHit.ImpactPoint.Z - PawnFloorPointZ;
		if (DeltaZ > MaxStepHeight)
		{
			UE_LOG(LogMover, VeryVerbose, TEXT("Reject step-down attempt during step-up/step-fwd, because it made us travel too high (too high Height %.3f) up from floor base %f to %f"), DeltaZ, PawnInitialFloorBaseZ, StepFwdHit.ImpactPoint.Z);
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// Reject unwalkable surface normals here.
		if (!UFloorQueryUtils::IsHitSurfaceWalkable(StepFwdHit, MaxWalkSlopeCosine))
		{
			// Reject if normal opposes movement direction
			const bool bNormalTowardsMe = (MoveDelta | StepFwdHit.ImpactNormal) < 0.f;
			if (bNormalTowardsMe)
			{
				UE_LOG(LogMover, VeryVerbose, TEXT("Reject step-down attempt during step-up/step-fwd, due to unwalkable normal %s opposed to movement"), *StepFwdHit.ImpactNormal.ToString());
				ScopedStepUpMovement.RevertMove();
				return false;
			}

			// Also reject if we would end up being higher than our starting location by stepping down.
			// It's fine to step down onto an unwalkable normal below us, we will just slide off. Rejecting those moves would prevent us from being able to walk off the edge.
			if (StepFwdHit.Location.Z > OldLocation.Z)
			{
				UE_LOG(LogMover, VeryVerbose, TEXT("Reject step-down attempt during step-up/step-fwd, due to unwalkable normal %s above old position)"), *StepFwdHit.ImpactNormal.ToString());
				ScopedStepUpMovement.RevertMove();
				return false;
			}
		}

		// Reject moves where the downward sweep hit something very close to the edge of the capsule. This maintains consistency with FindFloor as well.
		if (!UFloorQueryUtils::IsWithinEdgeTolerance(StepFwdHit.Location, StepFwdHit.ImpactPoint, PawnRadius))
		{
			UE_LOG(LogMover, VeryVerbose, TEXT("Reject step-down attempt during step-up/step-fwd, due to being outside edge tolerance"));
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// Don't step up onto invalid surfaces if traveling higher.
		if (DeltaZ > 0.f && !CanStepUpOnHitSurface(StepFwdHit))
		{
			UE_LOG(LogMover, VeryVerbose, TEXT("Reject step-down attempt during step-up/step-fwd, due to being up onto surface with !CanStepUpOnHitSurface")); 
			ScopedStepUpMovement.RevertMove();
			return false;
		}

		// See if we can validate the floor as a result of this step down. In almost all cases this should succeed, and we can avoid computing the floor outside this method.
		if (OutFloorTestResult != NULL)
		{

			UFloorQueryUtils::FindFloor(UpdatedComponent, UpdatedPrimitive,
				FloorSweepDistance, MaxWalkSlopeCosine,
				UpdatedComponent->GetComponentLocation(), StepDownResult.FloorTestResult);

			// Reject unwalkable normals if we end up higher than our initial height.
			// It's fine to walk down onto an unwalkable surface, don't reject those moves.
			if (StepFwdHit.Location.Z > OldLocation.Z)
			{
				// We should reject the floor result if we are trying to step up an actual step where we are not able to perch (this is rare).
				// In those cases we should instead abort the step up and try to slide along the stair.
				const float MAX_STEP_SIDE_Z = 0.08f; // TODO: Move magic numbers elsewhere
				if (!StepDownResult.FloorTestResult.bBlockingHit && StepSideZ < MAX_STEP_SIDE_Z)
				{
					UE_LOG(LogMover, VeryVerbose, TEXT("Reject step-down attempt during step-up/step-fwd, due to it being an unperchable step")); 
					ScopedStepUpMovement.RevertMove();
					return false;
				}
			}

			StepDownResult.bHasFloorResult = true;
		}
	}

	// Cache downwards substep
	QueuedSubsteps.Add(FMovementSubstep(StepDownSubstepName, UpdatedPrimitive->GetComponentLocation() - LastComponentLocation, false));
	LastComponentLocation = UpdatedPrimitive->GetComponentLocation();

	// Copy step down result.
	if (OutFloorTestResult != NULL)
	{
		*OutFloorTestResult = StepDownResult;
	}

	// Don't recalculate velocity based on this height adjustment, if considering vertical adjustments.
	//bJustTeleported |= !bMaintainHorizontalGroundVelocity;

	// Commit queued substeps to movement record
	for (FMovementSubstep Substep : QueuedSubsteps)
	{
		MoveRecord.Append(Substep);
	}

	return true;

}

bool UGroundMovementUtils::TryMoveToAdjustHeightAboveFloor(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, FFloorCheckResult& CurrentFloor, float MaxWalkSlopeCosine, FMovementRecord& MoveRecord)
{
	// If we have a floor check that hasn't hit anything, don't adjust height.
	if (!CurrentFloor.IsWalkableFloor())
	{
		return false;
	}

	float OldFloorDist = CurrentFloor.FloorDist;
	if (CurrentFloor.bLineTrace)
	{
		if (OldFloorDist < UE::FloorQueryUtility::MIN_FLOOR_DIST && CurrentFloor.LineDist >= UE::FloorQueryUtility::MIN_FLOOR_DIST)
		{
			// This would cause us to scale unwalkable walls
			return false;
		}
		// Falling back to a line trace means the sweep was unwalkable (or in penetration). Use the line distance for the vertical adjustment.
		OldFloorDist = CurrentFloor.LineDist;
	}

	// Move up or down to maintain floor height.
	if (OldFloorDist < UE::FloorQueryUtility::MIN_FLOOR_DIST || OldFloorDist > UE::FloorQueryUtility::MAX_FLOOR_DIST)
	{
		FHitResult AdjustHit(1.f);
		const float InitialZ = UpdatedComponent->GetComponentLocation().Z;
		const float AvgFloorDist = (UE::FloorQueryUtility::MIN_FLOOR_DIST + UE::FloorQueryUtility::MAX_FLOOR_DIST) * 0.5f;
		const float MoveDist = AvgFloorDist - OldFloorDist;

		MoveRecord.LockRelevancy(false);

		UMovementUtils::TrySafeMoveUpdatedComponent(UpdatedComponent, UpdatedPrimitive,
			FVector(0.f, 0.f, MoveDist), UpdatedComponent->GetComponentQuat(), 
			true, AdjustHit, ETeleportType::None, MoveRecord);

		MoveRecord.UnlockRelevancy();

		if (!AdjustHit.IsValidBlockingHit())
		{
			CurrentFloor.FloorDist += MoveDist;
		}
		else if (MoveDist > 0.f)
		{
			const float CurrentZ = UpdatedComponent->GetComponentLocation().Z;
			CurrentFloor.FloorDist += CurrentZ - InitialZ;
		}
		else
		{
			checkSlow(MoveDist < 0.f);
			const float CurrentZ = UpdatedComponent->GetComponentLocation().Z;
			CurrentFloor.FloorDist = CurrentZ - AdjustHit.Location.Z;
			if (UFloorQueryUtils::IsHitSurfaceWalkable(AdjustHit, MaxWalkSlopeCosine))
			{
				CurrentFloor.SetFromSweep(AdjustHit, CurrentFloor.FloorDist, true);
			}
		}

		return true;
	}

	return false;
}

float UGroundMovementUtils::TryWalkToSlideAlongSurface(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, UMoverComponent* MovementComponent, const FVector& Delta, float PctOfDeltaToMove, const FQuat Rotation, const FVector& Normal, FHitResult& Hit, bool bHandleImpact, FMovementRecord& MoveRecord, float MaxWalkSlopeCosine, float MaxStepHeight)
{
	if (!Hit.bBlockingHit)
	{
		return 0.f;
	}
	
	FVector SafeWalkNormal(Normal);

	// We don't want to be pushed up an unwalkable surface.
	if (SafeWalkNormal.Z > 0.f && !UFloorQueryUtils::IsHitSurfaceWalkable(Hit, MaxWalkSlopeCosine))
	{
		SafeWalkNormal = SafeWalkNormal.GetSafeNormal2D();
	}

	float PctOfTimeUsed = 0.f;
	const FVector OldSafeHitNormal = SafeWalkNormal;

	FVector SlideDelta = UMovementUtils::ComputeSlideDelta(Delta, PctOfDeltaToMove, SafeWalkNormal, Hit);

	if (SlideDelta.Dot(Delta) > 0.f)
	{
		UMovementUtils::TrySafeMoveUpdatedComponent(UpdatedComponent, UpdatedPrimitive, SlideDelta, Rotation, true, Hit, ETeleportType::None, MoveRecord);

		PctOfTimeUsed = Hit.Time;

		if (Hit.IsValidBlockingHit())
		{
			// Notify first impact
			if (MovementComponent && bHandleImpact)
			{
				FMoverOnImpactParams ImpactParams(NAME_None, Hit, SlideDelta);
				MovementComponent->HandleImpact(ImpactParams);
			}

			// Compute new slide normal when hitting multiple surfaces.
			SlideDelta = UMovementUtils::ComputeTwoWallAdjustedDelta(SlideDelta, Hit, OldSafeHitNormal);
			if (SlideDelta.Z > 0.f && UFloorQueryUtils::IsHitSurfaceWalkable(Hit, MaxWalkSlopeCosine) && Hit.Normal.Z > UE_KINDA_SMALL_NUMBER)
			{
				// Maintain horizontal velocity
				const float Time = (1.f - Hit.Time);
				const FVector ScaledDelta = SlideDelta.GetSafeNormal() * SlideDelta.Size();
				SlideDelta = FVector(SlideDelta.X, SlideDelta.Y, ScaledDelta.Z / Hit.Normal.Z) * Time;
				// Should never exceed MaxStepHeight in vertical component, so rescale if necessary.
				// This should be rare (Hit.Normal.Z above would have been very small) but we'd rather lose horizontal velocity than go too high.
				if (SlideDelta.Z > MaxStepHeight)
				{
					const float Rescale = MaxStepHeight / SlideDelta.Z;
					SlideDelta *= Rescale;
				}
			}
			else
			{
				SlideDelta.Z = 0.f;
			}
			
			// Only proceed if the new direction is of significant length and not in reverse of original attempted move.
			if (!SlideDelta.IsNearlyZero(UE::MoverUtils::SMALL_MOVE_DISTANCE) && (SlideDelta | Delta) > 0.f)
			{
				// Perform second move
				UMovementUtils::TrySafeMoveUpdatedComponent(UpdatedComponent, UpdatedPrimitive, SlideDelta, Rotation, true, Hit, ETeleportType::None, MoveRecord);
				PctOfTimeUsed += (Hit.Time * (1.f - PctOfTimeUsed));

				// Notify second impact
				if (MovementComponent && bHandleImpact && Hit.bBlockingHit)
				{
					FMoverOnImpactParams ImpactParams(NAME_None, Hit, SlideDelta);
					MovementComponent->HandleImpact(ImpactParams);
				}
			}
		}

		return FMath::Clamp(PctOfTimeUsed, 0.f, 1.f);
	}

	return 0.f;
}

FVector UGroundMovementUtils::ComputeDeflectedMoveOntoRamp(const FVector& OrigMoveDelta, const FHitResult& RampHitResult, float MaxWalkSlopeCosine, const bool bHitFromLineTrace)
{
	const FVector FloorNormal = RampHitResult.ImpactNormal;
	const FVector ContactNormal = RampHitResult.Normal;

	// JAH TODO: Change these Z tests to take movement plane into account, rather than assuming we're using the Z=0 plane
	if (FloorNormal.Z < (1.f - UE_KINDA_SMALL_NUMBER) && FloorNormal.Z > UE_KINDA_SMALL_NUMBER && 
		ContactNormal.Z > UE_KINDA_SMALL_NUMBER && 
		UFloorQueryUtils::IsHitSurfaceWalkable(RampHitResult, MaxWalkSlopeCosine) && !bHitFromLineTrace)
	{
		// Compute a vector that moves parallel to the surface, by projecting the horizontal movement direction onto the ramp.
		const FPlane RampSurfacePlane(FVector::ZeroVector, FloorNormal);
		return UMovementUtils::ConstrainToPlane(OrigMoveDelta, RampSurfacePlane, true);
	}

	return OrigMoveDelta;
}

bool UGroundMovementUtils::CanStepUpOnHitSurface(const FHitResult& Hit)
{
	if (!Hit.IsValidBlockingHit())
	{
		return false;
	}

	// No component for "fake" hits when we are on a known good base.
	const UPrimitiveComponent* HitComponent = Hit.Component.Get();
	if (!HitComponent)
	{
		return true;
	}

	APawn* PawnOwner = Cast<APawn>(Hit.GetActor());

	if (!HitComponent->CanCharacterStepUp(PawnOwner))
	{
		return false;
	}

	// No actor for "fake" hits when we are on a known good base.

	if (!Hit.HitObjectHandle.IsValid())
	{
		return true;
	}

	const AActor* HitActor = Hit.HitObjectHandle.GetManagingActor();
	if (!HitActor->CanBeBaseForCharacter(PawnOwner))
	{
		return false;
	}

	return true;
}
