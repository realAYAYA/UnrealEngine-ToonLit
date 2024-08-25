// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/Modes/PhysicsDrivenWalkingMode.h"

#include "Chaos/Character/CharacterGroundConstraint.h"
#include "Chaos/Character/CharacterGroundConstraintContainer.h"
#include "Chaos/ContactModification.h"
#include "Chaos/PhysicsObject.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "Engine/World.h"
#include "GameFramework/PhysicsVolume.h"
#include "Math/UnitConversion.h"
#include "MoverComponent.h"
#include "MoveLibrary/GroundMovementUtils.h"
#include "MoveLibrary/WaterMovementUtils.h"
#include "PhysicsMover/PhysicsMovementUtils.h"
#include "PhysicsMover/PhysicsMoverSimulationTypes.h"
#if WITH_EDITOR
#include "Backends/MoverNetworkPhysicsLiaison.h"
#include "Internationalization/Text.h"
#include "Misc/DataValidation.h"
#endif // WITH_EDITOR

#include UE_INLINE_GENERATED_CPP_BY_NAME(PhysicsDrivenWalkingMode)

extern FPhysicsDrivenMotionDebugParams GPhysicsDrivenMotionDebugParams;

#define LOCTEXT_NAMESPACE "PhysicsDrivenWalkingMode"

UPhysicsDrivenWalkingMode::UPhysicsDrivenWalkingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPhysicsDrivenWalkingMode::UpdateConstraintSettings(Chaos::FCharacterGroundConstraint& Constraint) const
{
	Constraint.SetRadialForceLimit(FUnitConversion::Convert(RadialForceLimit, EUnit::Newtons, EUnit::KilogramCentimetersPerSecondSquared));
	Constraint.SetFrictionForceLimit(FUnitConversion::Convert(FrictionForceLimit, EUnit::Newtons, EUnit::KilogramCentimetersPerSecondSquared));
	Constraint.SetTwistTorqueLimit(FUnitConversion::Convert(TwistTorqueLimit, EUnit::NewtonMeters, EUnit::KilogramCentimetersSquaredPerSecondSquared));
	Constraint.SetSwingTorqueLimit(FUnitConversion::Convert(SwingTorqueLimit, EUnit::NewtonMeters, EUnit::KilogramCentimetersSquaredPerSecondSquared));
	Constraint.SetTargetHeight(TargetHeight);
	Constraint.SetDampingFactor(GroundDamping);
}

void UPhysicsDrivenWalkingMode::OnContactModification_Internal(const FPhysicsMoverSimulationContactModifierParams& Params, Chaos::FCollisionContactModifier& Modifier) const
{
	Chaos::EnsureIsInPhysicsThreadContext();

	if (!Params.ConstraintHandle || !Params.UpdatedPrimitive)
	{
		return;
	}

	Chaos::FPBDRigidParticleHandle* CharacterParticle = Params.ConstraintHandle->GetCharacterParticle()->CastToRigidParticle();
	if (!CharacterParticle || CharacterParticle->Disabled())
	{
		return;
	}

	const Chaos::FGeometryParticleHandle* GroundParticle = Params.ConstraintHandle->GetGroundParticle();
	if (!GroundParticle)
	{
		return;
	}

	float PawnHalfHeight;
	float PawnRadius;
	Params.UpdatedPrimitive->CalcBoundingCylinder(PawnRadius, PawnHalfHeight);

	const float CharacterHeight = CharacterParticle->GetX().Z;
	const float EndCapHeight = CharacterHeight - PawnHalfHeight + PawnRadius;

	const float CosThetaMax = 0.707f;

	float MinContactHeightStepUps = CharacterHeight - 1.0e10f;
	const float StepDistance = FMath::Abs(TargetHeight - Params.ConstraintHandle->GetData().GroundDistance);
	if (StepDistance >= GPhysicsDrivenMotionDebugParams.MinStepUpDistance)
	{
		MinContactHeightStepUps = CharacterHeight - TargetHeight + CommonLegacySettings->MaxStepHeight;
	}

	for (Chaos::FContactPairModifier& PairModifier : Modifier.GetContacts(CharacterParticle))
	{
		const int32 CharacterIdx = CharacterParticle == PairModifier.GetParticlePair()[0] ? 0 : 1;
		const int32 OtherIdx = CharacterIdx == 0 ? 1 : 0;

		for (int32 Idx = 0; Idx < PairModifier.GetNumContacts(); ++Idx)
		{
			Chaos::FVec3 Point0, Point1;
			PairModifier.GetWorldContactLocations(Idx, Point0, Point1);
			Chaos::FVec3 CharacterPoint = CharacterIdx == 0 ? Point0 : Point1;

			Chaos::FVec3 ContactNormal = PairModifier.GetWorldNormal(Idx);
			if ((ContactNormal.Z > CosThetaMax) && CharacterPoint.Z < EndCapHeight)
			{
				// Disable any nearly vertical contact with the end cap of the capsule
				// This will be handled by the character ground constraint
				PairModifier.SetContactPointDisabled(Idx);
			}
			else if ((CharacterPoint.Z < MinContactHeightStepUps) && (GroundParticle == PairModifier.GetParticlePair()[OtherIdx]))
			{
				// In the case of steps ups disable all contacts below the max step height
				PairModifier.SetContactPointDisabled(Idx);
			}
		}
	}
}

bool UPhysicsDrivenWalkingMode::CanStepUpOnHitSurface(const FFloorCheckResult& FloorResult) const
{
	const float StepHeight = TargetHeight - FloorResult.FloorDist;

	bool bWalkable = StepHeight <= CommonLegacySettings->MaxStepHeight;
	constexpr float MinStepHeight = 2.0f;
	const bool SteppingUp = StepHeight > MinStepHeight;
	if (bWalkable && SteppingUp)
	{
		bWalkable = UGroundMovementUtils::CanStepUpOnHitSurface(FloorResult.HitResult);
	}

	return bWalkable;
}

void UPhysicsDrivenWalkingMode::FloorCheck(const FMoverDefaultSyncState& SyncState, const FProposedMove& ProposedMove, UPrimitiveComponent* UpdatedPrimitive, float DeltaSeconds,
	FFloorCheckResult& OutFloorResult, FWaterCheckResult& OutWaterResult, FVector& OutDeltaPos) const
{
	const FVector UpDir = GetMoverComponent()->GetUpDirection();

	FVector DeltaPos = ProposedMove.LinearVelocity * DeltaSeconds;
	OutDeltaPos = DeltaPos;

	float PawnHalfHeight;
	float PawnRadius;
	UpdatedPrimitive->CalcBoundingCylinder(PawnRadius, PawnHalfHeight);

	const float FloorSweepDistance = TargetHeight + CommonLegacySettings->MaxStepHeight;
	const float ShrinkRadius = 1.0f;
	const float QueryRadius = FMath::Max(PawnRadius - ShrinkRadius, 0.0f);
	UPhysicsMovementUtils::FloorSweep(SyncState.GetLocation_WorldSpace(), DeltaPos, UpdatedPrimitive, UpDir,
		QueryRadius, FloorSweepDistance, CommonLegacySettings->MaxWalkSlopeCosine, TargetHeight, OutFloorResult, OutWaterResult);

	if (!OutFloorResult.bBlockingHit)
	{
		// Floor not found
		return;
	}

	bool bWalkableFloor = OutFloorResult.bWalkableFloor && CanStepUpOnHitSurface(OutFloorResult);
	if (bWalkableFloor)
	{
		// Walkable floor found
		OutFloorResult.bWalkableFloor = true;
		return;
	}

	// Hit something but not walkable. Try a new query to find a walkable surface
	const float StepBlockedHeight = TargetHeight - PawnHalfHeight + PawnRadius;
	const float StepHeight = TargetHeight - OutFloorResult.FloorDist;

	if (StepHeight > StepBlockedHeight)
	{
		// Collision should prevent movement. Just try to find ground at start of movement
		const float ShrinkMultiplier = 0.75f;
		UPhysicsMovementUtils::FloorSweep(SyncState.GetLocation_WorldSpace(), DeltaPos, UpdatedPrimitive, UpDir,
			ShrinkMultiplier * QueryRadius, FloorSweepDistance, CommonLegacySettings->MaxWalkSlopeCosine, TargetHeight, OutFloorResult, OutWaterResult);

		OutFloorResult.bWalkableFloor = OutFloorResult.bWalkableFloor && CanStepUpOnHitSurface(OutFloorResult);
		return;
	}

	if (DeltaPos.SizeSquared() < UE_SMALL_NUMBER)
	{
		// Stationary
		OutDeltaPos = FVector::ZeroVector;
		return;
	}

	// Try to limit the movement to remain on a walkable surface
	FVector NewDeltaPos = DeltaPos;
	float NewQueryRadius = QueryRadius;

	FVector HorizSurfaceDir = FVector::VectorPlaneProject(OutFloorResult.HitResult.ImpactNormal, UpDir);
	float HorizSurfaceDirSizeSq = HorizSurfaceDir.SizeSquared();
	bool bFoundOutwardDir = false;
	if (HorizSurfaceDirSizeSq > UE_SMALL_NUMBER)
	{
		HorizSurfaceDir *= FMath::InvSqrt(HorizSurfaceDirSizeSq);
		bFoundOutwardDir = true;
	}
	else
	{
		// Flat unwalkable surface. Try and get the horizontal direction from the normal instead
		HorizSurfaceDir = FVector::VectorPlaneProject(OutFloorResult.HitResult.Normal, UpDir);
		HorizSurfaceDirSizeSq = HorizSurfaceDir.SizeSquared();

		if (HorizSurfaceDirSizeSq > UE_SMALL_NUMBER)
		{
			HorizSurfaceDir *= FMath::InvSqrt(HorizSurfaceDirSizeSq);
			bFoundOutwardDir = true;
		}
	}

	if (bFoundOutwardDir)
	{
		// If we're moving away try a ray query at the end of the motion
		const float DP = DeltaPos.Dot(HorizSurfaceDir);
		if (DP > 0.0f)
		{
			NewQueryRadius = 0.0f;
		}
		else
		{
			NewQueryRadius = 0.75f * QueryRadius;
			NewDeltaPos = DeltaPos - DP * HorizSurfaceDir;
		}

		UPhysicsMovementUtils::FloorSweep(SyncState.GetLocation_WorldSpace(), NewDeltaPos, UpdatedPrimitive, UpDir,
			NewQueryRadius, FloorSweepDistance, CommonLegacySettings->MaxWalkSlopeCosine, TargetHeight, OutFloorResult, OutWaterResult);

		OutFloorResult.bWalkableFloor = OutFloorResult.bWalkableFloor && CanStepUpOnHitSurface(OutFloorResult);
		if (OutFloorResult.bWalkableFloor)
		{
			OutDeltaPos = NewDeltaPos;
		}
		else
		{
			OutFloorResult.bWalkableFloor = false;
		}
	}
	else
	{
		// Try a query at the start of the movement to find a walkable surface and prevent movement

		NewDeltaPos = FVector::ZeroVector;
		NewQueryRadius = 0.75f * QueryRadius;

		UPhysicsMovementUtils::FloorSweep(SyncState.GetLocation_WorldSpace(), NewDeltaPos, UpdatedPrimitive, UpDir,
			NewQueryRadius, FloorSweepDistance, CommonLegacySettings->MaxWalkSlopeCosine, TargetHeight, OutFloorResult, OutWaterResult);

		OutFloorResult.bWalkableFloor = OutFloorResult.bWalkableFloor && CanStepUpOnHitSurface(OutFloorResult);
		OutDeltaPos = NewDeltaPos;
	}
}

#if WITH_EDITOR
EDataValidationResult UPhysicsDrivenWalkingMode::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	const UClass* BackendClass = GetMoverComponent()->BackendClass;
	if (BackendClass && !BackendClass->IsChildOf<UMoverNetworkPhysicsLiaisonComponent>())
	{
		Context.AddError(LOCTEXT("PhysicsMovementModeHasValidPhysicsLiaison", "Physics movement modes need to have a backend class that supports physics (UMoverNetworkPhysicsLiaisonComponent)."));
		Result = EDataValidationResult::Invalid;
	}
		
	return Result;
}
#endif // WITH_EDITOR

void UPhysicsDrivenWalkingMode::OnSimulationTick(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	const UMoverComponent* MoverComp = GetMoverComponent();
	const FMoverTickStartData& StartState = Params.StartState;
	USceneComponent* UpdatedComponent = Params.UpdatedComponent;
	UPrimitiveComponent* UpdatedPrimitive = Params.UpdatedPrimitive;
	FProposedMove ProposedMove = Params.ProposedMove;

	const FVector UpDir = GetMoverComponent()->GetUpDirection();

	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;

	// Instantaneous movement changes that are executed and we exit before consuming any time
	if ((ProposedMove.bHasTargetLocation && AttemptTeleport(UpdatedComponent, ProposedMove.TargetLocation, UpdatedComponent->GetComponentRotation(), StartingSyncState->GetVelocity_WorldSpace(), OutputState)))
	{
		OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs; 	// Give back all the time
		return;
	}

	UMoverBlackboard* SimBlackboard = GetBlackboard_Mutable();
	if (!SimBlackboard)
	{
		OutputSyncState = *StartingSyncState;
		return;
	}

	// Store the previous ground normal that was used to compute the proposed move
	FFloorCheckResult PrevFloorResult;
	FVector PrevGroundNormal = UpDir;
	if (SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, PrevFloorResult))
	{
		PrevGroundNormal = PrevFloorResult.HitResult.ImpactNormal;
	}

	// Floor query
	FFloorCheckResult FloorResult;
	FWaterCheckResult WaterResult;
	FVector OutDeltaPos;

	FloorCheck(*StartingSyncState, ProposedMove, UpdatedPrimitive, DeltaSeconds, FloorResult, WaterResult, OutDeltaPos);

	ProposedMove.LinearVelocity = OutDeltaPos / DeltaSeconds;

	SimBlackboard->Set(CommonBlackboard::LastFloorResult, FloorResult);
	SimBlackboard->Set(CommonBlackboard::LastWaterResult, WaterResult);

	const bool bStartSwimming = WaterResult.WaterSplineData.ImmersionDepth > CommonLegacySettings->SwimmingStartImmersionDepth;
	
	if (WaterResult.IsSwimmableVolume() && bStartSwimming)
	{
		SwitchToState(DefaultModeNames::Swimming, Params, OutputState);
	}
	else if (FloorResult.IsWalkableFloor())
	{
		const FVector StartGroundVelocity = UPhysicsMovementUtils::ComputeGroundVelocityFromHitResult(StartingSyncState->GetLocation_WorldSpace(), FloorResult.HitResult, DeltaSeconds);
		
		FVector TargetVelocity = StartingSyncState->GetVelocity_WorldSpace();
		FVector TargetPosition = StartingSyncState->GetLocation_WorldSpace();
		if (FloorResult.bWalkableFloor)
		{
			const FVector ProposedMovePlaneVelocity = ProposedMove.LinearVelocity - ProposedMove.LinearVelocity.ProjectOnToNormal(PrevGroundNormal);
			const FVector StartingMovePlaneVelocity = StartingSyncState->GetVelocity_WorldSpace() - StartingSyncState->GetVelocity_WorldSpace().ProjectOnToNormal(PrevGroundNormal);

			// If there is velocity intent in the normal direction then use the velocity from the proposed move. Otherwise
			// retain the previous vertical velocity
			FVector ProposedNormalVelocity = ProposedMove.LinearVelocity - ProposedMovePlaneVelocity;
			if (ProposedNormalVelocity.SizeSquared() <= UE_SMALL_NUMBER)
			{
				ProposedNormalVelocity = StartingSyncState->GetVelocity_WorldSpace() - StartingMovePlaneVelocity;
			}

			TargetVelocity = FMath::Lerp(StartingMovePlaneVelocity, ProposedMovePlaneVelocity, FractionalVelocityToTarget) + ProposedNormalVelocity;
			TargetPosition += ProposedMovePlaneVelocity * DeltaSeconds;
		}

		// Check if the proposed velocity would lift off the movement surface.
		bool bIsLiftingOffSurface = false;

		float CharacterGravity = 0.0f;
		if (const APhysicsVolume* PhysVolume = UpdatedComponent->GetPhysicsVolume())
		{
			CharacterGravity = PhysVolume->GetGravityZ();
		}
		const FVector ProjectedVelocity = TargetVelocity + CharacterGravity * FVector::UpVector * DeltaSeconds;
		const FVector ProjectedGroundVelocity = UPhysicsMovementUtils::ComputeIntegratedGroundVelocityFromHitResult(StartingSyncState->GetLocation_WorldSpace(), FloorResult.HitResult, DeltaSeconds);

		const float ProjectedRelativeVerticalVelocity = FloorResult.HitResult.ImpactNormal.Dot(ProjectedVelocity - ProjectedGroundVelocity);
		const float VerticalVelocityLimit = 2.0f / DeltaSeconds;
		if (ProjectedRelativeVerticalVelocity > VerticalVelocityLimit)
		{
			bIsLiftingOffSurface = true;
		}

		// Determine if the character is stepping up or stepping down.
		// If stepping up make sure that the step height is less than the max step height
		// and the new surface has CanCharacterStepUpOn set to true.
		// If stepping down make sure the step height is less than the max step height.
		const float StartHeightAboveGround = FloorResult.FloorDist - TargetHeight;
		const float EndHeightAboutGround = StartHeightAboveGround + UpDir.Dot(ProjectedVelocity - ProjectedGroundVelocity) * DeltaSeconds ;
		const bool bIsSteppingDown = StartHeightAboveGround > GPhysicsDrivenMotionDebugParams.MinStepUpDistance;
		const bool bIsWithinReach = EndHeightAboutGround <= CommonLegacySettings->MaxStepHeight;

		// If the character is unsupported allow some grace period before falling
		bool bIsSupported = bIsWithinReach && !bIsLiftingOffSurface;
		float TimeSinceSupported = MaxUnsupportedTimeBeforeFalling;
		SimBlackboard->TryGet(CommonBlackboard::TimeSinceSupported, TimeSinceSupported);
		if (bIsSupported)
		{
			SimBlackboard->Set(CommonBlackboard::TimeSinceSupported, 0.0f);
		}
		else
		{
			TimeSinceSupported += DeltaSeconds;
			SimBlackboard->Set(CommonBlackboard::TimeSinceSupported, TimeSinceSupported);
			bIsSupported = TimeSinceSupported < MaxUnsupportedTimeBeforeFalling;
		}

		// Apply vertical velocity to target if stepping down
		const bool bNeedsVerticalVelocityToTarget = bIsSupported && bIsSteppingDown && (EndHeightAboutGround > 0.0f) && !bIsLiftingOffSurface;
		if (bNeedsVerticalVelocityToTarget)
		{
			TargetVelocity -= FractionalDownwardVelocityToTarget * (EndHeightAboutGround / DeltaSeconds) * UpDir;
		}

		// Target orientation
		// This is always applied regardless of whether the character is supported
		FRotator TargetOrientation = StartingSyncState->GetOrientation_WorldSpace();
		if (!ProposedMove.AngularVelocity.IsZero())
		{
			TargetOrientation += (ProposedMove.AngularVelocity * DeltaSeconds);
		}

		if (bIsSupported)
		{
			OutputState.MovementEndState.NextModeName = DefaultModeNames::Walking;
			OutputState.MovementEndState.RemainingMs = 0.0f;

			OutputSyncState.MoveDirectionIntent = ProposedMove.bHasDirIntent ? ProposedMove.DirectionIntent : FVector::ZeroVector;
			OutputSyncState.SetTransforms_WorldSpace(
				TargetPosition,
				TargetOrientation,
				TargetVelocity,
				nullptr);
		}
		else
		{
			// Blocking hit but not supported
			SwitchToState(DefaultModeNames::Falling, Params, OutputState);
		}
	}
	else
	{
		// No water or floor not found
		SwitchToState(DefaultModeNames::Falling, Params, OutputState);
	}
}

bool UPhysicsDrivenWalkingMode::AttemptTeleport(USceneComponent* UpdatedComponent, const FVector& TeleportPos, const FRotator& TeleportRot, const FVector& PriorVelocity, FMoverTickEndData& Output)
{
	FMoverDefaultSyncState& OutputSyncState = Output.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

	OutputSyncState.SetTransforms_WorldSpace(TeleportPos,
		TeleportRot,
		PriorVelocity,
		nullptr); // no movement base

	// TODO: instead of invalidating it, consider checking for a floor. Possibly a dynamic base?
	if (UMoverBlackboard* SimBlackboard = GetBlackboard_Mutable())
	{
		SimBlackboard->Invalidate(CommonBlackboard::LastFloorResult);
		SimBlackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);
	}

	return true;
}

void UPhysicsDrivenWalkingMode::SwitchToState(const FName& StateName, const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs;
	OutputState.MovementEndState.NextModeName = StateName;

	const FMoverDefaultSyncState* StartingSyncState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
	OutputSyncState.SetTransforms_WorldSpace(
		StartingSyncState->GetLocation_WorldSpace(),
		StartingSyncState->GetOrientation_WorldSpace(),
		StartingSyncState->GetVelocity_WorldSpace(),
		nullptr);
}

#undef LOCTEXT_NAMESPACE