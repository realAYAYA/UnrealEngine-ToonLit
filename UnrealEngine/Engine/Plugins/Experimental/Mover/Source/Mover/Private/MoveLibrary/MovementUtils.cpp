// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoveLibrary/MovementUtils.h"
#include "MoveLibrary/MovementRecord.h"
#include "MoverLog.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/ScopedMovementUpdate.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "MoverComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovementUtils)

namespace UE::MoverUtils
{
	const double SMALL_MOVE_DISTANCE = 1e-3;
}

bool UMovementUtils::IsExceedingMaxSpeed(const FVector& Velocity, float InMaxSpeed)
{
	InMaxSpeed = FMath::Max(0.f, InMaxSpeed);
	const float MaxSpeedSquared = FMath::Square(InMaxSpeed);

	// Allow 1% error tolerance, to account for numeric imprecision.
	const float OverVelocityPercent = 1.01f;
	return (Velocity.SizeSquared() > MaxSpeedSquared * OverVelocityPercent);
}


FVector UMovementUtils::ComputeVelocity(const FComputeVelocityParams& InParams)
{
	const FVector ControlAcceleration = InParams.MoveDirectionIntent.GetClampedToMaxSize(1.f);
	FVector Velocity = InParams.InitialVelocity;

	const float AnalogInputModifier = (ControlAcceleration.SizeSquared() > 0.f ? ControlAcceleration.Size() : 0.f);
	const float MaxPawnSpeed = InParams.MaxSpeed * AnalogInputModifier;
	const bool bExceedingMaxSpeed = IsExceedingMaxSpeed(Velocity, MaxPawnSpeed);

	if (AnalogInputModifier > 0.f && !bExceedingMaxSpeed)
	{
		// Apply change in velocity direction
		if (Velocity.SizeSquared() > 0.f)
		{
			// Change direction faster than only using acceleration, but never increase velocity magnitude.
			const float TimeScale = FMath::Clamp(InParams.DeltaSeconds * InParams.TurningBoost, 0.f, 1.f);
			Velocity = Velocity + (ControlAcceleration * Velocity.Size() - Velocity) * FMath::Min(TimeScale * InParams.Friction, 1.f);
		}
	}
	else
	{
		// Dampen velocity magnitude based on deceleration.
		if (Velocity.SizeSquared() > 0.f)
		{
			const FVector OldVelocity = Velocity;
			const float VelSize = FMath::Max(Velocity.Size() - FMath::Abs(InParams.Friction * Velocity.Size() + InParams.Deceleration) * InParams.DeltaSeconds, 0.f);
			Velocity = Velocity.GetSafeNormal() * VelSize;

			// Don't allow braking to lower us below max speed if we started above it.
			if (bExceedingMaxSpeed && Velocity.SizeSquared() < FMath::Square(MaxPawnSpeed))
			{
				Velocity = OldVelocity.GetSafeNormal() * MaxPawnSpeed;
			}
		}
	}

	// Apply acceleration and clamp velocity magnitude.
	const float NewMaxSpeed = (IsExceedingMaxSpeed(Velocity, MaxPawnSpeed)) ? Velocity.Size() : MaxPawnSpeed;
	Velocity += ControlAcceleration * FMath::Abs(InParams.Acceleration) * InParams.DeltaSeconds;
	Velocity = Velocity.GetClampedToMaxSize(NewMaxSpeed);

	return Velocity;
}

FVector UMovementUtils::ComputeCombinedVelocity(const FComputeCombinedVelocityParams& InParams)
{
	const FVector ControlAcceleration = InParams.MoveDirectionIntent.GetClampedToMaxSize(1.f);
	FVector Velocity = InParams.InitialVelocity;

	const float AnalogInputModifier = (ControlAcceleration.SizeSquared() > 0.f ? ControlAcceleration.Size() : 0.f);
	
	const float MaxInputSpeed = InParams.MaxSpeed * AnalogInputModifier;
	const float MaxSpeed = FMath::Max(InParams.OverallMaxSpeed, MaxInputSpeed);
	
	const bool bExceedingMaxSpeed = IsExceedingMaxSpeed(Velocity, MaxSpeed);

	if ((AnalogInputModifier > KINDA_SMALL_NUMBER || InParams.ExternalAcceleration.Size() > KINDA_SMALL_NUMBER) && !bExceedingMaxSpeed)
	{
		// Apply change in velocity direction
		if (Velocity.SizeSquared() > 0.f)
		{
			// Change direction faster than only using acceleration, but never increase velocity magnitude.
			const float TimeScale = FMath::Clamp(InParams.DeltaSeconds * InParams.TurningBoost, 0.f, 1.f);
			Velocity = Velocity + (ControlAcceleration * Velocity.Size() - Velocity) * FMath::Min(TimeScale * InParams.Friction, 1.f);
		}
	}
	else
	{
		// Dampen velocity magnitude based on deceleration.
		if (Velocity.SizeSquared() > 0.f)
		{
			const FVector OldVelocity = Velocity;
			const float VelSize = FMath::Max(Velocity.Size() - FMath::Abs(InParams.Friction * Velocity.Size() + InParams.Deceleration) * InParams.DeltaSeconds, 0.f);
			Velocity = Velocity.GetSafeNormal() * VelSize;

			// Don't allow braking to lower us below max speed if we started above it.
			if (bExceedingMaxSpeed && Velocity.SizeSquared() < FMath::Square(MaxSpeed))
			{
				Velocity = OldVelocity.GetSafeNormal() * MaxSpeed;
			}
		}
	}

	// Apply input acceleration and clamp velocity magnitude.
	const float NewMaxInputSpeed = (IsExceedingMaxSpeed(Velocity, MaxInputSpeed)) ? Velocity.Size() : MaxInputSpeed;
	Velocity += ControlAcceleration * FMath::Abs(InParams.Acceleration) * InParams.DeltaSeconds;
	Velocity = Velocity.GetClampedToMaxSize(NewMaxInputSpeed);

	// Apply move requested acceleration
	const float NewMaxMoveSpeed = (IsExceedingMaxSpeed(Velocity, InParams.OverallMaxSpeed)) ? Velocity.Size() : InParams.OverallMaxSpeed;
	Velocity += InParams.ExternalAcceleration * InParams.DeltaSeconds;
	Velocity = Velocity.GetClampedToMaxSize(NewMaxMoveSpeed);

	return Velocity;
}

bool UMovementUtils::CanEscapeGravity(const FVector& PriorVelocity, const FVector& NewVelocity, const FVector& GravityAccel, float DeltaSeconds)
{
	if (DeltaSeconds > UE_SMALL_NUMBER)
	{
		const FVector VelocityDelta = NewVelocity - PriorVelocity;
		const FVector Acceleration = VelocityDelta / DeltaSeconds;
		const FVector AccelOntoGravity = Acceleration.ProjectOnTo(GravityAccel);

		// If acceleration opposes gravity and is stronger, then it can escape
		if (AccelOntoGravity.Dot(GravityAccel) < 0.0 &&
			AccelOntoGravity.SizeSquared() > GravityAccel.SizeSquared())
		{
			return true;
		}
	}

	return false;
}

FVector UMovementUtils::ConstrainToPlane(const FVector& Vector, const FPlane& MovementPlane, bool bMaintainMagnitude)
{
	FVector ConstrainedResult = FVector::PointPlaneProject(Vector, MovementPlane);

	if (bMaintainMagnitude)
	{
		ConstrainedResult = ConstrainedResult.GetSafeNormal() * Vector.Size();
	}

	return ConstrainedResult;
}

FVector UMovementUtils::ComputeSlideDelta(const FVector& Delta, const float PctOfDeltaToMove, const FVector& Normal, const FHitResult& Hit)
{
	return FVector::VectorPlaneProject(Delta, Normal) * PctOfDeltaToMove;
}

FVector UMovementUtils::ComputeTwoWallAdjustedDelta(const FVector& MoveDelta, const FHitResult& Hit, const FVector& OldHitNormal)
{
	FVector Delta = MoveDelta;
	const FVector HitNormal = Hit.Normal;

	if ((OldHitNormal | HitNormal) <= 0.f) //90 or less corner, so use cross product for direction
	{
		const FVector DesiredDir = Delta;
		FVector NewDir = (HitNormal ^ OldHitNormal);
		NewDir = NewDir.GetSafeNormal();
		Delta = (Delta | NewDir) * (1.f - Hit.Time) * NewDir;
		if ((DesiredDir | Delta) < 0.f)
		{
			Delta = -1.f * Delta;
		}
	}
	else //adjust to new wall
	{
		const FVector DesiredDir = Delta;
		Delta = UMovementUtils::ComputeSlideDelta(Delta, 1.f - Hit.Time, HitNormal, Hit);
		if ((Delta | DesiredDir) <= 0.f)
		{
			Delta = FVector::ZeroVector;
		}
		else if (FMath::Abs((HitNormal | OldHitNormal) - 1.f) < KINDA_SMALL_NUMBER)
		{
			// we hit the same wall again even after adjusting to move along it the first time
			// nudge away from it (this can happen due to precision issues)
			Delta += HitNormal * 0.01f;
		}
	}

	return Delta;
}

float UMovementUtils::TryMoveToSlideAlongSurface(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, UMoverComponent* MoverComponent, const FVector& Delta, float PctOfDeltaToMove, const FQuat Rotation, const FVector& Normal, FHitResult& Hit, bool bHandleImpact, FMovementRecord& MoveRecord)
{
	if (!Hit.bBlockingHit)
	{
		return 0.f;
	}

	float PctOfTimeUsed = 0.f;
	const FVector OldHitNormal = Normal;

	FVector SlideDelta = UMovementUtils::ComputeSlideDelta(Delta, PctOfDeltaToMove, Normal, Hit);

	if ((SlideDelta | Delta) > 0.f)
	{
		TrySafeMoveUpdatedComponent(UpdatedComponent, UpdatedPrimitive, SlideDelta, Rotation, true, Hit, ETeleportType::None, MoveRecord);

		PctOfTimeUsed = Hit.Time;

		if (Hit.IsValidBlockingHit())
		{
			// Notify first impact
			if (MoverComponent && bHandleImpact)
			{
				FMoverOnImpactParams ImpactParams(NAME_None, Hit, SlideDelta);
				MoverComponent->HandleImpact(ImpactParams);
			}

			// Compute new slide normal when hitting multiple surfaces.
			SlideDelta = UMovementUtils::ComputeTwoWallAdjustedDelta(SlideDelta, Hit, OldHitNormal);

			// Only proceed if the new direction is of significant length and not in reverse of original attempted move.
			if (!SlideDelta.IsNearlyZero(UE::MoverUtils::SMALL_MOVE_DISTANCE) && (SlideDelta | Delta) > 0.f)
			{
				// Perform second move
				TrySafeMoveUpdatedComponent(UpdatedComponent, UpdatedPrimitive, SlideDelta, Rotation, true, Hit, ETeleportType::None, MoveRecord);
				PctOfTimeUsed += (Hit.Time * (1.f - PctOfTimeUsed));

				// Notify second impact
				if (MoverComponent && bHandleImpact && Hit.bBlockingHit)
				{
					FMoverOnImpactParams ImpactParams(NAME_None, Hit, SlideDelta);
					MoverComponent->HandleImpact(ImpactParams);
				}
			}
		}

		return FMath::Clamp(PctOfTimeUsed, 0.f, 1.f);
	}

	return 0.f;
}


static const FName SafeMoveSubstepName = "SafeMove";

bool UMovementUtils::TrySafeMoveUpdatedComponent(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult& OutHit, ETeleportType Teleport, FMovementRecord& MoveRecord)
{
	if (UpdatedComponent == nullptr)
	{
		OutHit.Reset(1.f);
		return false;
	}

	bool bMoveResult = false;
	EMoveComponentFlags MoveComponentFlags = MOVECOMP_NoFlags;

	FVector PreviousCompPos = UpdatedComponent->GetComponentLocation();

	// Scope for move flags
	{
		// Conditionally ignore blocking overlaps (based on CVar)
		const EMoveComponentFlags IncludeBlockingOverlapsWithoutEvents = (MOVECOMP_NeverIgnoreBlockingOverlaps | MOVECOMP_DisableBlockingOverlapDispatch);
		//TGuardValue<EMoveComponentFlags> ScopedFlagRestore(MoveComponentFlags, MovementComponentCVars::MoveIgnoreFirstBlockingOverlap ? MoveComponentFlags : (MoveComponentFlags | IncludeBlockingOverlapsWithoutEvents));
		MoveComponentFlags = (MoveComponentFlags | IncludeBlockingOverlapsWithoutEvents);
		bMoveResult = TryMoveUpdatedComponent_Internal(UpdatedComponent, Delta, NewRotation, bSweep, MoveComponentFlags, &OutHit, Teleport);

		if (UpdatedComponent)
		{
			UE_LOG(LogMover, VeryVerbose, TEXT("TrySafeMove: %s (role %i) Delta=%s DidMove=%i"), 
				*GetNameSafe(UpdatedComponent->GetOwner()), UpdatedComponent->GetOwnerRole(), *Delta.ToCompactString(), bMoveResult);
		}
	}

	// Handle initial penetrations
	if (OutHit.bStartPenetrating && UpdatedComponent)
	{
		const FVector RequestedAdjustment = ComputePenetrationAdjustment(OutHit);
		if (TryMoveToResolvePenetration(UpdatedComponent, UpdatedPrimitive, MoveComponentFlags, RequestedAdjustment, OutHit, NewRotation, MoveRecord))
		{
			PreviousCompPos = UpdatedComponent->GetComponentLocation();

			// Retry original move
			bMoveResult = TryMoveUpdatedComponent_Internal(UpdatedComponent, Delta, NewRotation, bSweep, MoveComponentFlags, &OutHit, Teleport);

			UE_LOG(LogMover, VeryVerbose, TEXT("TrySafeMove retry: %s (role %i) Delta=%s DidMove=%i"),
				*GetNameSafe(UpdatedComponent->GetOwner()), UpdatedComponent->GetOwnerRole(), *Delta.ToCompactString(), bMoveResult);
		}
	}

	if (bMoveResult && UpdatedComponent)
	{
		MoveRecord.Append( FMovementSubstep(SafeMoveSubstepName, UpdatedComponent->GetComponentLocation()-PreviousCompPos, true) );
	}


	return bMoveResult;
}

FVector UMovementUtils::ComputePenetrationAdjustment(const FHitResult& Hit)
{
	if (!Hit.bStartPenetrating)
	{
		return FVector::ZeroVector;
	}

	FVector Result;
	const float PullBackDistance = 0.125f; //FMath::Abs(BaseMovementCVars::PenetrationPullbackDistance);
	const float PenetrationDepth = (Hit.PenetrationDepth > 0.f ? Hit.PenetrationDepth : 0.125f);

	Result = Hit.Normal * (PenetrationDepth + PullBackDistance);

	return Result;
}


static const FName PenetrationResolutionSubstepName = "ResolvePenetration";

bool UMovementUtils::TryMoveToResolvePenetration(USceneComponent* UpdatedComponent, UPrimitiveComponent* UpdatedPrimitive, EMoveComponentFlags MoveComponentFlags, const FVector& ProposedAdjustment, const FHitResult& Hit, const FQuat& NewRotationQuat, FMovementRecord& MoveRecord)
{
	// SceneComponent can't be in penetration, so this function really only applies to PrimitiveComponent.
	const FVector Adjustment = ProposedAdjustment; //ConstrainDirectionToPlane(ProposedAdjustment);
	if (!Adjustment.IsZero() && UpdatedPrimitive)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_BaseMovementComponent_ResolvePenetration);
		// See if we can fit at the adjusted location without overlapping anything.
		AActor* ActorOwner = UpdatedComponent->GetOwner();
		if (!ActorOwner)
		{
			return false;
		}

		const FVector OriginalCompPos = UpdatedComponent->GetComponentLocation();

		// We really want to make sure that precision differences or differences between the overlap test and sweep tests don't put us into another overlap,
		// so make the overlap test a bit more restrictive.
		const float OverlapInflation = 0.1f; //BaseMovementCVars::PenetrationOverlapCheckInflation;
		bool bEncroached = OverlapTest(UpdatedComponent, UpdatedPrimitive, Hit.TraceStart + Adjustment, NewRotationQuat, UpdatedPrimitive->GetCollisionObjectType(), UpdatedPrimitive->GetCollisionShape(OverlapInflation), ActorOwner);
		if (!bEncroached)
		{
			// Move without sweeping.
			const bool bDidMove = TryMoveUpdatedComponent_Internal(UpdatedComponent, Adjustment, NewRotationQuat, false, MoveComponentFlags, nullptr, ETeleportType::TeleportPhysics);

			UE_LOG(LogMover, VeryVerbose, TEXT("TryMoveToResolvePenetration unencroached: %s (role %i) Adjustment=%s DidMove=%i"),
				*GetNameSafe(UpdatedComponent->GetOwner()), UpdatedComponent->GetOwnerRole(), *Adjustment.ToCompactString(), bDidMove);

			MoveRecord.Append( FMovementSubstep(PenetrationResolutionSubstepName, UpdatedComponent->GetComponentLocation()-OriginalCompPos, false) );
			return true;
		}
		else
		{
			// Disable MOVECOMP_NeverIgnoreBlockingOverlaps if it is enabled, otherwise we wouldn't be able to sweep out of the object to fix the penetration.
			TGuardValue<EMoveComponentFlags> ScopedFlagRestore(MoveComponentFlags, EMoveComponentFlags(MoveComponentFlags & (~MOVECOMP_NeverIgnoreBlockingOverlaps)));

			// Try sweeping as far as possible...
			FHitResult SweepOutHit(1.f);
			bool bMoved = TryMoveUpdatedComponent_Internal(UpdatedComponent, Adjustment, NewRotationQuat, true, MoveComponentFlags, &SweepOutHit, ETeleportType::TeleportPhysics);

			UE_LOG(LogMover, VeryVerbose, TEXT("TryMoveToResolvePenetration: %s (role %i) Adjustment=%s DidMove=%i"),
				*GetNameSafe(UpdatedComponent->GetOwner()), UpdatedComponent->GetOwnerRole(), *Adjustment.ToCompactString(), bMoved);

			// Still stuck?
			if (!bMoved && SweepOutHit.bStartPenetrating)
			{
				// Combine two MTD results to get a new direction that gets out of multiple surfaces.
				const FVector SecondMTD = ComputePenetrationAdjustment(SweepOutHit);
				const FVector CombinedMTD = Adjustment + SecondMTD;
				if (SecondMTD != Adjustment && !CombinedMTD.IsZero())
				{
					bMoved = TryMoveUpdatedComponent_Internal(UpdatedComponent, CombinedMTD, NewRotationQuat, true, MoveComponentFlags, nullptr, ETeleportType::TeleportPhysics);

					UE_LOG(LogMover, VeryVerbose, TEXT("TryMoveToResolvePenetration combined: %s (role %i) CombinedAdjustment=%s DidMove=%i"),
						*GetNameSafe(UpdatedComponent->GetOwner()), UpdatedComponent->GetOwnerRole(), *CombinedMTD.ToCompactString(), bMoved);
				}
			}

			// Still stuck?
			if (!bMoved)
			{
				// Try moving the proposed adjustment plus the attempted move direction. This can sometimes get out of penetrations with multiple objects
				const FVector MoveDelta = (Hit.TraceEnd - Hit.TraceStart); //ConstrainDirectionToPlane(Hit.TraceEnd - Hit.TraceStart);
				if (!MoveDelta.IsZero())
				{
					const FVector AdjustAndMoveDelta = Adjustment + MoveDelta;
					bMoved = TryMoveUpdatedComponent_Internal(UpdatedComponent, AdjustAndMoveDelta, NewRotationQuat, true, MoveComponentFlags, nullptr, ETeleportType::TeleportPhysics);

					UE_LOG(LogMover, VeryVerbose, TEXT("TryMoveToResolvePenetration multiple: %s (role %i) AdjustAndMoveDelta=%s DidMove=%i"),
						*GetNameSafe(UpdatedComponent->GetOwner()), UpdatedComponent->GetOwnerRole(), *AdjustAndMoveDelta.ToCompactString(), bMoved);
				}
			}

			if (bMoved)
			{
				MoveRecord.Append( FMovementSubstep(PenetrationResolutionSubstepName, UpdatedComponent->GetComponentLocation()-OriginalCompPos, false) );
			}
			
			return bMoved;
		}
	}

	return false;
}


void UMovementUtils::InitCollisionParams(const UPrimitiveComponent* UpdatedPrimitive, FCollisionQueryParams& OutParams, FCollisionResponseParams& OutResponseParam)
{
	if (UpdatedPrimitive)
	{
		UpdatedPrimitive->InitSweepCollisionParams(OutParams, OutResponseParam);
	}
}


bool UMovementUtils::OverlapTest(const USceneComponent* UpdatedComponent, const UPrimitiveComponent* UpdatedPrimitive, const FVector& Location, const FQuat& RotationQuat, const ECollisionChannel CollisionChannel, const FCollisionShape& CollisionShape, const AActor* IgnoreActor)
{
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MovementOverlapTest), false, IgnoreActor);
	FCollisionResponseParams ResponseParam;
	InitCollisionParams(UpdatedPrimitive, QueryParams, ResponseParam);
	return UpdatedComponent->GetWorld()->OverlapBlockingTestByChannel(Location, RotationQuat, CollisionChannel, CollisionShape, QueryParams, ResponseParam);
}

FVector UMovementUtils::ComputeVelocityFromPositions(const FVector& FromPos, const FVector& ToPos, float DeltaSeconds)
{
	if (DeltaSeconds > 0.f)
	{ 
		return (ToPos - FromPos) / DeltaSeconds;
	}
	
	return FVector::ZeroVector;
}

FRotator UMovementUtils::ComputeAngularVelocity(const FRotator& FromOrientation, const FRotator& ToOrientation, float DeltaSeconds, float TurningRateLimit)
{
	FRotator AngularVelocityDpS(FRotator::ZeroRotator);

	if (DeltaSeconds > 0.f)
	{
		FRotator AngularDelta = (ToOrientation-FromOrientation);
		FRotator Winding, Remainder;

		AngularDelta.GetWindingAndRemainder(Winding, Remainder);	// to find the fastest turn, just get the (-180,180] remainder

		AngularVelocityDpS = Remainder * (1.f / DeltaSeconds);

		if (TurningRateLimit >= 0.f)
		{
			AngularVelocityDpS.Yaw   = FMath::Clamp(AngularVelocityDpS.Yaw,   -TurningRateLimit, TurningRateLimit);
			AngularVelocityDpS.Pitch = FMath::Clamp(AngularVelocityDpS.Pitch, -TurningRateLimit, TurningRateLimit);
			AngularVelocityDpS.Roll  = FMath::Clamp(AngularVelocityDpS.Roll,  -TurningRateLimit, TurningRateLimit);
		}
	}

	return AngularVelocityDpS;
}


FVector UMovementUtils::ComputeDirectionIntent(const FVector& MoveInput, EMoveInputType MoveInputType)
{
	FVector ResultDirIntent(FVector::ZeroVector);

	switch (MoveInputType)
	{
		default: break;

		case EMoveInputType::DirectionalIntent:
			ResultDirIntent = MoveInput;
			break;

		case EMoveInputType::Velocity:	// note: if we had access to the max velocity, we could scale intent to match
			ResultDirIntent = MoveInput.GetSafeNormal();
			break;
	}

	return ResultDirIntent;
}

bool UMovementUtils::TryMoveUpdatedComponent_Internal(USceneComponent* UpdatedComponent, const FVector& Delta, const FQuat& NewRotation, bool bSweep, EMoveComponentFlags MoveComponentFlags, FHitResult* OutHit, ETeleportType Teleport)
{

	if (UpdatedComponent)
	{
		// TODO: Consider whether we need NewDelta... seems pointless in this context
		const FVector NewDelta = Delta;
		return UpdatedComponent->MoveComponent(NewDelta, NewRotation, bSweep, OutHit, MoveComponentFlags, Teleport);
	}

	return false;
}
