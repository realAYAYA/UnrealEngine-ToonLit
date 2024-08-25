// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultMovementSet/Modes/WalkingMode.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "MoveLibrary/ModularMovement.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/GroundMovementUtils.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "MoverLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WalkingMode)


UWalkingMode::UWalkingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SharedSettingsClass = UCommonLegacyMovementSettings::StaticClass();
}

void UWalkingMode::OnGenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	const UMoverComponent* MoverComp = GetMoverComponent();
	const FCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);


	const float DeltaSeconds = TimeStep.StepMs * 0.001f;
	FFloorCheckResult LastFloorResult;
	FVector MovementNormal;

	UMoverBlackboard* SimBlackboard = GetBlackboard_Mutable();

	// Try to use the floor as the basis for the intended move direction (i.e. try to walk along slopes, rather than into them)
	if (SimBlackboard && SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, LastFloorResult) && LastFloorResult.IsWalkableFloor())
	{
		MovementNormal = LastFloorResult.HitResult.ImpactNormal;
	}
	else
	{
		MovementNormal = MoverComp->GetUpDirection();
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

	FGroundMoveParams Params;

	if (CharacterInputs)
	{
		Params.MoveInputType = CharacterInputs->GetMoveInputType();
		Params.MoveInput = CharacterInputs->GetMoveInput_WorldSpace();
	}
	else
	{
		Params.MoveInputType = EMoveInputType::Invalid;
		Params.MoveInput = FVector::ZeroVector;
	}

	Params.OrientationIntent = IntendedOrientation_WorldSpace;
	Params.PriorVelocity = FVector::VectorPlaneProject(StartingSyncState->GetVelocity_WorldSpace(), MovementNormal);
	Params.PriorOrientation = StartingSyncState->GetOrientation_WorldSpace();
	Params.GroundNormal = MovementNormal;
	Params.TurningRate = CommonLegacySettings->TurningRate;
	Params.TurningBoost = CommonLegacySettings->TurningBoost;
	Params.MaxSpeed = CommonLegacySettings->MaxSpeed;
	Params.Acceleration = CommonLegacySettings->Acceleration;
	Params.Deceleration = CommonLegacySettings->Deceleration;
	Params.DeltaSeconds = DeltaSeconds;
	
	if (Params.MoveInput.SizeSquared() > 0.f && !UMovementUtils::IsExceedingMaxSpeed(Params.PriorVelocity, CommonLegacySettings->MaxSpeed))
	{
		Params.Friction = CommonLegacySettings->GroundFriction;
	}
	else
	{
		Params.Friction = CommonLegacySettings->bUseSeparateBrakingFriction ? CommonLegacySettings->BrakingFriction : CommonLegacySettings->GroundFriction;
		Params.Friction *= CommonLegacySettings->BrakingFrictionFactor;
	}

	OutProposedMove = UGroundMovementUtils::ComputeControlledGroundMove(Params);

	if (TurnGenerator)
	{
		OutProposedMove.AngularVelocity = ITurnGeneratorInterface::Execute_GetTurn(TurnGenerator, IntendedOrientation_WorldSpace, StartState, *StartingSyncState, TimeStep, OutProposedMove, SimBlackboard);
	}
}

void UWalkingMode::OnSimulationTick(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	const UMoverComponent* MoverComp = GetMoverComponent();
	const FMoverTickStartData& StartState = Params.StartState;
	USceneComponent* UpdatedComponent = Params.UpdatedComponent;
	UPrimitiveComponent* UpdatedPrimitive = Params.UpdatedPrimitive;
	FProposedMove ProposedMove = Params.ProposedMove;

	if (!UpdatedComponent || !UpdatedPrimitive)
	{
		return;
	}

	const FCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();


	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;

	// Instantaneous movement changes that are executed and we exit before consuming any time
	if ( (ProposedMove.bHasTargetLocation && AttemptTeleport(UpdatedComponent, ProposedMove.TargetLocation, UpdatedComponent->GetComponentRotation(), StartingSyncState->GetVelocity_WorldSpace(), OutputState)) ||	// Teleport
		 (CharacterInputs && CharacterInputs->bIsJumpJustPressed && AttemptJump(CommonLegacySettings->JumpUpwardsSpeed, OutputState)) )	// Jump
	{
		UpdatedComponent->ComponentVelocity = StartingSyncState->GetVelocity_WorldSpace();
		OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs; 	// Give back all the time
		return;
	}

	const FVector OrigMoveDelta = ProposedMove.LinearVelocity * DeltaSeconds;

	TObjectPtr<AActor> OwnerActor = UpdatedComponent->GetOwner();
	check(OwnerActor);
	TObjectPtr<USkeletalMeshComponent> OwnerSkeletalMesh = OwnerActor->FindComponentByClass<USkeletalMeshComponent>();

	FMovementRecord MoveRecord;
	MoveRecord.SetDeltaSeconds(DeltaSeconds);

	FFloorCheckResult CurrentFloor;
	UMoverBlackboard* SimBlackboard = GetBlackboard_Mutable();

	// If we don't have cached floor information, we need to search for it again
	if (!SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, CurrentFloor))
	{
		UFloorQueryUtils::FindFloor(UpdatedComponent, UpdatedPrimitive,
			CommonLegacySettings->FloorSweepDistance, CommonLegacySettings->MaxWalkSlopeCosine,
			UpdatedPrimitive->GetComponentLocation(), CurrentFloor);
	}

	OutputSyncState.MoveDirectionIntent = (ProposedMove.bHasDirIntent ? ProposedMove.DirectionIntent : FVector::ZeroVector);

	const FRotator StartingOrient = StartingSyncState->GetOrientation_WorldSpace();
	FRotator TargetOrient = StartingOrient;

	bool bIsOrientationChanging = false;

	// Apply orientation changes (if any)
	if (!ProposedMove.AngularVelocity.IsZero())
	{ 
		TargetOrient += (ProposedMove.AngularVelocity * DeltaSeconds);
		bIsOrientationChanging = (TargetOrient != StartingOrient);
	}

	const FQuat TargetOrientQuat = TargetOrient.Quaternion();
	FHitResult MoveHitResult(1.f);
	
	FVector CurMoveDelta = OrigMoveDelta;
	
	FOptionalFloorCheckResult StepUpFloorResult;	// passed to sub-operations, so we can use their final floor results if they did a test

	bool bDidAttemptMovement = false;

	float PercentTimeAppliedSoFar = MoveHitResult.Time;
	bool bWasFirstMoveBlocked = false;

	if (!CurMoveDelta.IsNearlyZero() || bIsOrientationChanging)
	{
		// Attempt to move the full amount first
		bDidAttemptMovement = true;
		bool bMoved = UMovementUtils::TrySafeMoveUpdatedComponent(UpdatedComponent, UpdatedPrimitive, CurMoveDelta, TargetOrientQuat, true, MoveHitResult, ETeleportType::None, MoveRecord);
		float LastMoveSeconds = DeltaSeconds;

		if (MoveHitResult.bStartPenetrating)
		{
			// We started by being stuck in geometry and need to resolve it first
			// TODO: try to resolve starting stuck in geometry
			bWasFirstMoveBlocked = true;
		}
		else if (MoveHitResult.IsValidBlockingHit())
		{
			// We impacted something (possibly a ramp, possibly a barrier)
			bWasFirstMoveBlocked = true;
			PercentTimeAppliedSoFar = MoveHitResult.Time;		

			// Check if the blockage is a walkable ramp rising in front of us
			// JAH TODO: change this normal.Z test to consider vs the movement plane (not just Z=0 plane)
			if ((MoveHitResult.Time > 0.f) && (MoveHitResult.Normal.Z > UE_KINDA_SMALL_NUMBER) && 
			    UFloorQueryUtils::IsHitSurfaceWalkable(MoveHitResult, CommonLegacySettings->MaxWalkSlopeCosine))
			{
				// It's a walkable ramp, so cut up the move and attempt to move the remainder of it along the ramp's surface, possibly generating another hit
				const float PercentTimeRemaining = 1.f - PercentTimeAppliedSoFar;
				CurMoveDelta = UGroundMovementUtils::ComputeDeflectedMoveOntoRamp(CurMoveDelta * PercentTimeRemaining, MoveHitResult, CommonLegacySettings->MaxWalkSlopeCosine, CurrentFloor.bLineTrace);
				UMovementUtils::TrySafeMoveUpdatedComponent(UpdatedComponent, UpdatedPrimitive, CurMoveDelta, TargetOrientQuat, true, MoveHitResult, ETeleportType::None, MoveRecord);
				LastMoveSeconds = PercentTimeRemaining * LastMoveSeconds;

				const float SecondHitPercent = MoveHitResult.Time * PercentTimeRemaining;
				PercentTimeAppliedSoFar = FMath::Clamp(PercentTimeAppliedSoFar + SecondHitPercent, 0.f, 1.f);
			}

			if (MoveHitResult.IsValidBlockingHit())
			{
				// If still blocked, try to step up onto the blocking object OR slide along it

				UMoverComponent* MoverComponent = GetMoverComponent();
				// JAH TODO: Take movement bases into account
				if (UGroundMovementUtils::CanStepUpOnHitSurface(MoveHitResult)) // || (CharacterOwner->GetMovementBase() != nullptr && Hit.HitObjectHandle == CharacterOwner->GetMovementBase()->GetOwner()))
				{
					// hit a barrier or unwalkable surface, try to step up and onto it
					const FVector PreStepUpLocation = UpdatedComponent->GetComponentLocation();
					const FVector DownwardDir = -MoverComp->GetUpDirection();

					if (!UGroundMovementUtils::TryMoveToStepUp(UpdatedComponent, UpdatedPrimitive, MoverComponent, DownwardDir, CommonLegacySettings->MaxStepHeight, CommonLegacySettings->MaxWalkSlopeCosine, CommonLegacySettings->FloorSweepDistance, OrigMoveDelta * (1.f - PercentTimeAppliedSoFar), MoveHitResult, CurrentFloor, false, &StepUpFloorResult, MoveRecord))
					{
						FMoverOnImpactParams ImpactParams(DefaultModeNames::Walking, MoveHitResult, OrigMoveDelta);
						MoverComponent->HandleImpact(ImpactParams);
						float PercentAvailableToSlide = 1.f - PercentTimeAppliedSoFar;
						float SlideAmount = UGroundMovementUtils::TryWalkToSlideAlongSurface(UpdatedComponent, UpdatedPrimitive, MoverComponent, OrigMoveDelta, PercentAvailableToSlide, TargetOrientQuat, MoveHitResult.Normal, MoveHitResult, true, MoveRecord, CommonLegacySettings->MaxWalkSlopeCosine, CommonLegacySettings->MaxStepHeight);
						PercentTimeAppliedSoFar += PercentAvailableToSlide * SlideAmount;
					}
				}
				else if (MoveHitResult.Component.IsValid() && !MoveHitResult.Component.Get()->CanCharacterStepUp(Cast<APawn>(MoveHitResult.GetActor())))
				{
					FMoverOnImpactParams ImpactParams(DefaultModeNames::Walking, MoveHitResult, OrigMoveDelta);
					MoverComponent->HandleImpact(ImpactParams);
					float PercentAvailableToSlide = 1.f - PercentTimeAppliedSoFar;
					float SlideAmount = UGroundMovementUtils::TryWalkToSlideAlongSurface(UpdatedComponent, UpdatedPrimitive, MoverComponent, OrigMoveDelta, 1.f - PercentTimeAppliedSoFar, TargetOrientQuat, MoveHitResult.Normal, MoveHitResult, true, MoveRecord, CommonLegacySettings->MaxWalkSlopeCosine, CommonLegacySettings->MaxStepHeight);
					PercentTimeAppliedSoFar += PercentAvailableToSlide * SlideAmount;
				}
			}
		}

		// Search for the floor we've ended up on
		UFloorQueryUtils::FindFloor(UpdatedComponent, UpdatedPrimitive,
			CommonLegacySettings->FloorSweepDistance, CommonLegacySettings->MaxWalkSlopeCosine,
			UpdatedPrimitive->GetComponentLocation(), CurrentFloor);

		if (CurrentFloor.IsWalkableFloor())
		{
			UGroundMovementUtils::TryMoveToAdjustHeightAboveFloor(UpdatedComponent, UpdatedPrimitive, CurrentFloor, CommonLegacySettings->MaxWalkSlopeCosine, MoveRecord);
		}
    
		if (!CurrentFloor.IsWalkableFloor() && !CurrentFloor.HitResult.bStartPenetrating)
		{
			// No floor or not walkable, so let's let the airborne movement mode deal with it
			OutputState.MovementEndState.NextModeName = CommonLegacySettings->AirMovementModeName;
			OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs - (Params.TimeStep.StepMs * PercentTimeAppliedSoFar);
			MoveRecord.SetDeltaSeconds((Params.TimeStep.StepMs - OutputState.MovementEndState.RemainingMs) * 0.001f);
			CaptureFinalState(UpdatedComponent, bDidAttemptMovement, CurrentFloor, MoveRecord, OutputSyncState);
			return;
		}
	}
	else
	{
		// If the actor isn't moving we still need to check if they have a valid floor
		UFloorQueryUtils::FindFloor(UpdatedComponent, UpdatedPrimitive,
			CommonLegacySettings->FloorSweepDistance, CommonLegacySettings->MaxWalkSlopeCosine,
			UpdatedPrimitive->GetComponentLocation(), CurrentFloor);
		
		FHitResult Hit(CurrentFloor.HitResult);
		if (Hit.bStartPenetrating)
		{
			// The floor check failed because it started in penetration
			// We do not want to try to move downward because the downward sweep failed, rather we'd like to try to pop out of the floor.
			Hit.TraceEnd = Hit.TraceStart + FVector(0.f, 0.f, 2.4f);
			FVector RequestedAdjustment = UMovementUtils::ComputePenetrationAdjustment(Hit);
			
			const EMoveComponentFlags IncludeBlockingOverlapsWithoutEvents = (MOVECOMP_NeverIgnoreBlockingOverlaps | MOVECOMP_DisableBlockingOverlapDispatch);
			EMoveComponentFlags MoveComponentFlags = MOVECOMP_NoFlags;
			MoveComponentFlags = (MoveComponentFlags | IncludeBlockingOverlapsWithoutEvents);
			UMovementUtils::TryMoveToResolvePenetration(UpdatedComponent, UpdatedPrimitive, MoveComponentFlags, RequestedAdjustment, Hit, UpdatedComponent->GetComponentQuat(), MoveRecord);
		}
		
		if (!CurrentFloor.IsWalkableFloor() && !Hit.bStartPenetrating)
		{
			// No floor or not walkable, so let's let the airborne movement mode deal with it
			OutputState.MovementEndState.NextModeName = CommonLegacySettings->AirMovementModeName;
			OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs;
			MoveRecord.SetDeltaSeconds((Params.TimeStep.StepMs - OutputState.MovementEndState.RemainingMs) * 0.001f);
			CaptureFinalState(UpdatedComponent, bDidAttemptMovement, CurrentFloor, MoveRecord, OutputSyncState);
			return;
		}
	}

	CaptureFinalState(UpdatedComponent, bDidAttemptMovement, CurrentFloor, MoveRecord, OutputSyncState);

}

UObject* UWalkingMode::GetTurnGenerator()
{
	return TurnGenerator;
}

void UWalkingMode::SetTurnGeneratorClass(TSubclassOf<UObject> TurnGeneratorClass)
{
	if (TurnGeneratorClass)
	{
		TurnGenerator = NewObject<UObject>(this, TurnGeneratorClass);
	}
	else
	{
		TurnGenerator = nullptr; // Clearing the turn generator is valid - will go back to the default turn generation
	}
}


void UWalkingMode::OnRegistered(const FName ModeName)
{
	Super::OnRegistered(ModeName);

	CommonLegacySettings = GetMoverComponent()->FindSharedSettings<UCommonLegacyMovementSettings>();
	ensureMsgf(CommonLegacySettings, TEXT("Failed to find instance of CommonLegacyMovementSettings on %s. Movement may not function properly."), *GetPathNameSafe(this));
}

void UWalkingMode::OnUnregistered()
{
	CommonLegacySettings = nullptr;

	Super::OnUnregistered();
}

bool UWalkingMode::AttemptJump(float UpwardsSpeed, FMoverTickEndData& OutputState)
{
	// TODO: This should check if a jump is even allowed
	TSharedPtr<FLayeredMove_JumpImpulse> JumpMove = MakeShared<FLayeredMove_JumpImpulse>();
	JumpMove->UpwardsSpeed = UpwardsSpeed;
	OutputState.SyncState.LayeredMoves.QueueLayeredMove(JumpMove);
	OutputState.MovementEndState.NextModeName = CommonLegacySettings->AirMovementModeName;
	return true;
}

bool UWalkingMode::AttemptTeleport(USceneComponent* UpdatedComponent, const FVector& TeleportPos, const FRotator& TeleportRot, const FVector& PriorVelocity, FMoverTickEndData& Output)
{
	if (UpdatedComponent->GetOwner()->TeleportTo(TeleportPos, TeleportRot))
	{
		FMoverDefaultSyncState& OutputSyncState = Output.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

		OutputSyncState.SetTransforms_WorldSpace( UpdatedComponent->GetComponentLocation(),
												  UpdatedComponent->GetComponentRotation(),
												  PriorVelocity,
												  nullptr ); // no movement base
		
		// TODO: instead of invalidating it, consider checking for a floor. Possibly a dynamic base?
		if (UMoverBlackboard* SimBlackboard = GetBlackboard_Mutable())
		{
			SimBlackboard->Invalidate(CommonBlackboard::LastFloorResult);
			SimBlackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);
		}

		return true;
	}

	return false;
}

void UWalkingMode::CaptureFinalState(USceneComponent* UpdatedComponent, bool bDidAttemptMovement, const FFloorCheckResult& FloorResult, const FMovementRecord& Record, FMoverDefaultSyncState& OutputSyncState) const
{
	FRelativeBaseInfo PriorBaseInfo;

	UMoverBlackboard* SimBlackboard = GetBlackboard_Mutable();

	const bool bHasPriorBaseInfo = SimBlackboard->TryGet(CommonBlackboard::LastFoundDynamicMovementBase, PriorBaseInfo);

	FRelativeBaseInfo CurrentBaseInfo = UpdateFloorAndBaseInfo(FloorResult);

	// If we're on a dynamic base and we're not trying to move, keep using the same relative actor location. This prevents slow relative 
	//  drifting that can occur from repeated floor sampling as the base moves through the world.
	if (CurrentBaseInfo.HasRelativeInfo() 
		&& bHasPriorBaseInfo && !bDidAttemptMovement 
		&& PriorBaseInfo.UsesSameBase(CurrentBaseInfo))
	{
		CurrentBaseInfo.ContactLocalPosition = PriorBaseInfo.ContactLocalPosition;
	}

	// TODO: Update Main/large movement record with substeps from our local record
	
	if (CurrentBaseInfo.HasRelativeInfo())
	{
		SimBlackboard->Set(CommonBlackboard::LastFoundDynamicMovementBase, CurrentBaseInfo);

		OutputSyncState.SetTransforms_WorldSpace( UpdatedComponent->GetComponentLocation(),
												  UpdatedComponent->GetComponentRotation(),
												  Record.GetRelevantVelocity(),
												  CurrentBaseInfo.MovementBase.Get(), CurrentBaseInfo.BoneName);
	}
	else
	{
		SimBlackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);

		OutputSyncState.SetTransforms_WorldSpace( UpdatedComponent->GetComponentLocation(),
												  UpdatedComponent->GetComponentRotation(),
												  Record.GetRelevantVelocity(),
												  nullptr);	// no movement base
	}

	UpdatedComponent->ComponentVelocity = OutputSyncState.GetVelocity_WorldSpace();
}


FRelativeBaseInfo UWalkingMode::UpdateFloorAndBaseInfo(const FFloorCheckResult& FloorResult) const
{
	FRelativeBaseInfo ReturnBaseInfo;

	UMoverBlackboard* SimBlackboard = GetBlackboard_Mutable();

	SimBlackboard->Set(CommonBlackboard::LastFloorResult, FloorResult);

	if (FloorResult.IsWalkableFloor() && UBasedMovementUtils::IsADynamicBase(FloorResult.HitResult.GetComponent()))
	{
		ReturnBaseInfo.SetFromFloorResult(FloorResult);
	}

	return ReturnBaseInfo;
}
