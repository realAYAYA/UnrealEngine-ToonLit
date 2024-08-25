// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultMovementSet/Modes/FallingMode.h"
#include "Components/SkeletalMeshComponent.h"
#include "MoverComponent.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "MoveLibrary/AirMovementUtils.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(FallingMode)

UFallingMode::UFallingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	  AirControlPercentage(0.4f),
	  FallingDeceleration(200.0f),
	  OverTerminalSpeedFallingDeceleration(800.0f),
	  TerminalMovementPlaneSpeed(1500.0f),
	  bShouldClampTerminalVerticalSpeed(true),
	  VerticalFallingDeceleration(4000.0f),
	  TerminalVerticalSpeed(2000.0f)
{
	SharedSettingsClass = UCommonLegacyMovementSettings::StaticClass();
}

constexpr float VERTICAL_SLOPE_NORMAL_Z = 0.001f; // Slope is vertical if Abs(Normal.Z) <= this threshold. Accounts for precision problems that sometimes angle normals slightly off horizontal for vertical surface.

void UFallingMode::OnGenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	const FCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	const float DeltaSeconds = TimeStep.StepMs * 0.001f;

	// We don't want velocity limits to take the falling velocity component into account, since it is handled 
	//   separately by the terminal velocity of the environment.
	const FVector StartVelocity = StartingSyncState->GetVelocity_WorldSpace();
	const FVector StartHorizontalVelocity = FVector(StartVelocity.X, StartVelocity.Y, 0.f);

	FFreeMoveParams Params;
	if (CharacterInputs)
	{
		Params.MoveInputType = CharacterInputs->GetMoveInputType();
		Params.MoveInput = CharacterInputs->GetMoveInput();
	}
	else
	{
		Params.MoveInputType = EMoveInputType::Invalid;
		Params.MoveInput = FVector::ZeroVector;
	}

	FRotator IntendedOrientation_WorldSpace;
	// If there's no intent from input to change orientation, use the current orientation
	if (!CharacterInputs || CharacterInputs->OrientationIntent.IsNearlyZero())
	{
		IntendedOrientation_WorldSpace = StartingSyncState->GetOrientation_WorldSpace();
	}
	else
	{
		IntendedOrientation_WorldSpace = CharacterInputs->GetOrientationIntentDir_WorldSpace().ToOrientationRotator();
	}

	Params.OrientationIntent = IntendedOrientation_WorldSpace;
	Params.PriorVelocity = StartHorizontalVelocity;
	Params.PriorOrientation = StartingSyncState->GetOrientation_WorldSpace();
	Params.DeltaSeconds = DeltaSeconds;
	Params.TurningRate = CommonLegacySettings->TurningRate;
	Params.TurningBoost = CommonLegacySettings->TurningBoost;
	Params.MaxSpeed = CommonLegacySettings->MaxSpeed;
	Params.Acceleration = CommonLegacySettings->Acceleration;
	Params.Deceleration = FallingDeceleration;

	Params.MoveInput *= AirControlPercentage;
	// Don't care about Z axis input since falling - if Z input matters that should probably be a different movement mode
	Params.MoveInput.Z = 0;

	// Check if any current velocity values are over our terminal velocity - if so limit the move input in that direction and apply OverTerminalVelocityFallingDeceleration
	if (Params.MoveInput.Dot(StartVelocity) > 0 && StartVelocity.Size2D() >= TerminalMovementPlaneSpeed)
	{
		const FPlane MovementNormalPlane(StartVelocity, StartVelocity.GetSafeNormal());
		Params.MoveInput = Params.MoveInput.ProjectOnTo(MovementNormalPlane);

		Params.Deceleration = OverTerminalSpeedFallingDeceleration;
	}
	
	UMoverBlackboard* SimBlackboard = GetBlackboard_Mutable();
	FFloorCheckResult LastFloorResult;
	// limit our moveinput based on the floor we're on
	if (SimBlackboard && SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, LastFloorResult))
	{
		if (LastFloorResult.HitResult.IsValidBlockingHit() && LastFloorResult.HitResult.Normal.Z > VERTICAL_SLOPE_NORMAL_Z && !LastFloorResult.IsWalkableFloor())
		{
			// If acceleration is into the wall, limit contribution.
			if (FVector::DotProduct(Params.MoveInput, LastFloorResult.HitResult.Normal) < 0.f)
			{
				// Allow movement parallel to the wall, but not into it because that may push us up.
				const FVector Normal2D = LastFloorResult.HitResult.Normal.GetSafeNormal2D();
				Params.MoveInput = FVector::VectorPlaneProject(Params.MoveInput, Normal2D);
			}
		}
	}
	
	OutProposedMove = UAirMovementUtils::ComputeControlledFreeMove(Params);
	const FVector VelocityWithGravity = StartVelocity + UMovementUtils::ComputeVelocityFromGravity(GetMoverComponent()->GetGravityAcceleration(), DeltaSeconds);

	//  If we are going faster than TerminalVerticalVelocity apply VerticalFallingDeceleration otherwise reset Z velocity to before we applied deceleration 
	if (VelocityWithGravity.GetAbs().Z > TerminalVerticalSpeed)
	{
		if (bShouldClampTerminalVerticalSpeed)
		{
			OutProposedMove.LinearVelocity.Z = FMath::Sign(VelocityWithGravity.Z) * TerminalVerticalSpeed;
		}
		else
		{
			float DesiredDeceleration = FMath::Abs(TerminalVerticalSpeed - FMath::Abs(VelocityWithGravity.Z)) / DeltaSeconds;
			float DecelerationToApply = FMath::Min(DesiredDeceleration, VerticalFallingDeceleration);
			DecelerationToApply = FMath::Sign(VelocityWithGravity.Z) * DecelerationToApply * DeltaSeconds;
			OutProposedMove.LinearVelocity.Z = VelocityWithGravity.Z - DecelerationToApply;
		}
	}
	else
	{
		OutProposedMove.LinearVelocity.Z = VelocityWithGravity.Z;
	}
}

void UFallingMode::OnSimulationTick(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	const FMoverTickStartData& StartState = Params.StartState;
	USceneComponent* UpdatedComponent = Params.UpdatedComponent;
	UPrimitiveComponent* UpdatedPrimitive = Params.UpdatedPrimitive;
	FProposedMove ProposedMove = Params.ProposedMove;

	const FCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

	//const FVector GravityAccel = FVector(0.0f, 0.0f, -9800.0f);	// -9.8 m / sec^2
	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;
	float PctTimeApplied = 0.f;

	// Instantaneous movement changes that are executed and we exit before consuming any time
	if (ProposedMove.bHasTargetLocation && AttemptTeleport(UpdatedComponent, ProposedMove.TargetLocation, UpdatedComponent->GetComponentRotation(), StartingSyncState->GetVelocity_WorldSpace(), OutputSyncState))
	{
		OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs; 	// Give back all the time
		return;
	}

	FMovementRecord MoveRecord;
	MoveRecord.SetDeltaSeconds(DeltaSeconds);
	
	UMoverBlackboard* SimBlackboard = GetBlackboard_Mutable();

	SimBlackboard->Invalidate(CommonBlackboard::LastFloorResult);	// falling = no valid floor
	SimBlackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);

	OutputSyncState.MoveDirectionIntent = (ProposedMove.bHasDirIntent ? ProposedMove.DirectionIntent : FVector::ZeroVector);


	// Use the orientation intent directly. If no intent is provided, use last frame's orientation. Note that we are assuming rotation changes can't fail. 
	FRotator TargetOrient = StartingSyncState->GetOrientation_WorldSpace();

	// Apply orientation changes (if any)
	if (!ProposedMove.AngularVelocity.IsZero())
	{
		TargetOrient += (ProposedMove.AngularVelocity * DeltaSeconds);
	}
	
	const FVector PriorFallingVelocity = StartingSyncState->GetVelocity_WorldSpace();

	//FVector MoveDelta = 0.5f * (PriorFallingVelocity + ProposedMove.LinearVelocity) * DeltaSeconds; 	// TODO: revive midpoint integration
	FVector MoveDelta = ProposedMove.LinearVelocity * DeltaSeconds;

	FHitResult Hit(1.f);
	const FQuat TargetOrientQuat = TargetOrient.Quaternion();

	UMovementUtils::TrySafeMoveUpdatedComponent(UpdatedComponent, UpdatedPrimitive, MoveDelta, TargetOrientQuat, true, Hit, ETeleportType::None, MoveRecord);

	// Compute final velocity based on how long we actually go until we get a hit.
	FVector NewFallingVelocity = StartingSyncState->GetVelocity_WorldSpace();
	//NewFallingVelocity.Z = 0.5f * (PriorFallingVelocity.Z + (ProposedMove.LinearVelocity.Z * Hit.Time));	// TODO: revive midpoint integration
	NewFallingVelocity.Z = ProposedMove.LinearVelocity.Z * Hit.Time;

	FFloorCheckResult LandingFloor;

	// Handle impact, whether it's a landing surface or something to slide on
	if (Hit.IsValidBlockingHit() && UpdatedPrimitive)
	{
		float LastMoveTimeSlice = DeltaSeconds;
		float SubTimeTickRemaining = LastMoveTimeSlice * (1.f - Hit.Time);

		PctTimeApplied += Hit.Time * (1.f - PctTimeApplied);

		// Check for hitting a landing surface
		if (UAirMovementUtils::IsValidLandingSpot(UpdatedComponent, UpdatedPrimitive, UpdatedPrimitive->GetComponentLocation(), 
			Hit, CommonLegacySettings->FloorSweepDistance, CommonLegacySettings->MaxWalkSlopeCosine, OUT LandingFloor))
		{
			CaptureFinalState(UpdatedComponent, *StartingSyncState, LandingFloor, DeltaSeconds, DeltaSeconds * PctTimeApplied, OutputSyncState, OutputState, MoveRecord);
			return;
		}
		
		LandingFloor.HitResult = Hit;
		SimBlackboard->Set(CommonBlackboard::LastFloorResult, LandingFloor);

		UMoverComponent* MoverComponent = GetMoverComponent();
		FMoverOnImpactParams ImpactParams(DefaultModeNames::Falling, Hit, MoveDelta);
		MoverComponent->HandleImpact(ImpactParams);

		// We didn't land on a walkable surface, so let's try to slide along it
		UAirMovementUtils::TryMoveToFallAlongSurface(UpdatedComponent, UpdatedPrimitive, MoverComponent, MoveDelta, 
			(1.f - Hit.Time), TargetOrientQuat, Hit.Normal, Hit, true,
			CommonLegacySettings->FloorSweepDistance, CommonLegacySettings->MaxWalkSlopeCosine, LandingFloor, MoveRecord);

		PctTimeApplied += Hit.Time * (1.f - PctTimeApplied);

		if (LandingFloor.IsWalkableFloor())
		{
			CaptureFinalState(UpdatedComponent, *StartingSyncState, LandingFloor, DeltaSeconds, DeltaSeconds * PctTimeApplied, OutputSyncState, OutputState, MoveRecord);
			return;
		}
	}
	else
	{
		// This indicates an unimpeded full move
		PctTimeApplied = 1.f;
	}
	
	CaptureFinalState(UpdatedComponent, *StartingSyncState, LandingFloor, DeltaSeconds, DeltaSeconds* PctTimeApplied, OutputSyncState, OutputState, MoveRecord);
}


void UFallingMode::OnRegistered(const FName ModeName)
{
	Super::OnRegistered(ModeName);

	CommonLegacySettings = GetMoverComponent()->FindSharedSettings<UCommonLegacyMovementSettings>();
	ensureMsgf(CommonLegacySettings, TEXT("Failed to find instance of CommonLegacyMovementSettings on %s. Movement may not function properly."), *GetPathNameSafe(this));
}


void UFallingMode::OnUnregistered()
{
	CommonLegacySettings = nullptr;

	Super::OnUnregistered();
}


bool UFallingMode::AttemptTeleport(USceneComponent* UpdatedComponent, const FVector& TeleportPos, const FRotator& TeleportRot, const FVector& PriorVelocity, FMoverDefaultSyncState& OutputSyncState)
{
	if (UpdatedComponent->GetOwner()->TeleportTo(TeleportPos, TeleportRot))
	{
		OutputSyncState.SetTransforms_WorldSpace( UpdatedComponent->GetComponentLocation(),
												  UpdatedComponent->GetComponentRotation(),
												  PriorVelocity,
												  nullptr); // no movement base

		UpdatedComponent->ComponentVelocity = PriorVelocity;
		return true;
	}

	return false;
}

void UFallingMode::ProcessLanded(const FFloorCheckResult& FloorResult, FVector& Velocity, FRelativeBaseInfo& BaseInfo, FMoverTickEndData& TickEndData) const
{
	UMoverBlackboard* SimBlackboard = GetBlackboard_Mutable();

	FName NextMovementMode = NAME_None; 
	// if we can walk on the floor we landed on
	if (FloorResult.IsWalkableFloor())
	{
		// Transfer to LandingMovementMode (usually walking), and cache any floor / movement base info
		Velocity.Z = 0.0;
		NextMovementMode = CommonLegacySettings->GroundMovementModeName;

		SimBlackboard->Set(CommonBlackboard::LastFloorResult, FloorResult);

		if (UBasedMovementUtils::IsADynamicBase(FloorResult.HitResult.GetComponent()))
		{
			BaseInfo.SetFromFloorResult(FloorResult);
		}
	}
	// we could check for other surfaces here (i.e. when swimming is implemented we can check the floor hit here and see if we need to go into swimming)

	// This would also be a good spot for implementing some falling physics interactions (i.e. falling into a movable object and pushing it based off of this actors velocity)
	
	// if a new mode was set go ahead and switch to it after this tick and broadcast we landed
	if (!NextMovementMode.IsNone())
	{
		TickEndData.MovementEndState.NextModeName = NextMovementMode;
		OnLanded.Broadcast(NextMovementMode, FloorResult.HitResult);
	}
}

void UFallingMode::CaptureFinalState(USceneComponent* UpdatedComponent, const FMoverDefaultSyncState& StartSyncState, const FFloorCheckResult& FloorResult, float DeltaSeconds, float DeltaSecondsUsed, FMoverDefaultSyncState& OutputSyncState, FMoverTickEndData& TickEndData, FMovementRecord& Record) const
{
	UMoverBlackboard* SimBlackboard = GetBlackboard_Mutable();

	const FVector FinalLocation = UpdatedComponent->GetComponentLocation();

	// Check for time refunds
	constexpr float MinRemainingSecondsToRefund = 0.0001f;	// If we have this amount of time (or more) remaining, give it to the next simulation step.

	if ((DeltaSeconds - DeltaSecondsUsed) >= MinRemainingSecondsToRefund)
	{
		const float PctOfTimeRemaining = (1.0f - (DeltaSecondsUsed / DeltaSeconds));
		TickEndData.MovementEndState.RemainingMs = PctOfTimeRemaining * DeltaSeconds * 1000.f;
	}
	else
	{
		TickEndData.MovementEndState.RemainingMs = 0.f;
	}
	
	Record.SetDeltaSeconds( DeltaSecondsUsed );
	
	FVector EffectiveVelocity = Record.GetRelevantVelocity();
	// TODO: Update Main/large movement record with substeps from our local record

	FRelativeBaseInfo MovementBaseInfo;
	ProcessLanded(FloorResult, EffectiveVelocity, MovementBaseInfo, TickEndData);

	if (MovementBaseInfo.HasRelativeInfo())
	{
		SimBlackboard->Set(CommonBlackboard::LastFoundDynamicMovementBase, MovementBaseInfo);

		OutputSyncState.SetTransforms_WorldSpace( FinalLocation,
												  UpdatedComponent->GetComponentRotation(),
												  EffectiveVelocity,
												  MovementBaseInfo.MovementBase.Get(), MovementBaseInfo.BoneName);
	}
	else
	{
		OutputSyncState.SetTransforms_WorldSpace( FinalLocation,
												  UpdatedComponent->GetComponentRotation(),
												  EffectiveVelocity,
												  nullptr); // no movement base
	}

	UpdatedComponent->ComponentVelocity = EffectiveVelocity;
}
