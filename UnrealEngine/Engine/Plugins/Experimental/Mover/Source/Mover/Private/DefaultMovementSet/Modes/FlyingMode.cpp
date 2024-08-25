// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultMovementSet/Modes/FlyingMode.h"
#include "MoveLibrary/AirMovementUtils.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoverComponent.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FlyingMode)


UFlyingMode::UFlyingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UFlyingMode::OnGenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
	const FCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	const float DeltaSeconds = TimeStep.StepMs * 0.001f;

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
	Params.PriorVelocity = StartingSyncState->GetVelocity_WorldSpace();
	Params.PriorOrientation = StartingSyncState->GetOrientation_WorldSpace();
	Params.TurningRate = CommonLegacySettings->TurningRate;
	Params.TurningBoost = CommonLegacySettings->TurningBoost;
	Params.MaxSpeed = CommonLegacySettings->MaxSpeed;
	Params.Acceleration = CommonLegacySettings->Acceleration;
	Params.Deceleration = CommonLegacySettings->Deceleration;
	Params.DeltaSeconds = DeltaSeconds;
	
	OutProposedMove = UAirMovementUtils::ComputeControlledFreeMove(Params);
}

void UFlyingMode::OnSimulationTick(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	const FMoverTickStartData& StartState = Params.StartState;
	USceneComponent* UpdatedComponent = Params.UpdatedComponent;
	UPrimitiveComponent* UpdatedPrimitive = Params.UpdatedPrimitive;
	FProposedMove ProposedMove = Params.ProposedMove;

	const FCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;

	// Instantaneous movement changes that are executed and we exit before consuming any time
	if (ProposedMove.bHasTargetLocation && AttemptTeleport(UpdatedComponent, ProposedMove.TargetLocation, UpdatedComponent->GetComponentRotation(), *StartingSyncState, OutputState))
	{
		OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs; 	// Give back all the time
		return;
	}

	FMovementRecord MoveRecord;
	MoveRecord.SetDeltaSeconds(DeltaSeconds);

	UMoverBlackboard* SimBlackboard = GetBlackboard_Mutable();

	SimBlackboard->Invalidate(CommonBlackboard::LastFloorResult);	// flying = no valid floor
	SimBlackboard->Invalidate(CommonBlackboard::LastFoundDynamicMovementBase);

	OutputSyncState.MoveDirectionIntent = (ProposedMove.bHasDirIntent ? ProposedMove.DirectionIntent : FVector::ZeroVector);

	// Use the orientation intent directly. If no intent is provided, use last frame's orientation. Note that we are assuming rotation changes can't fail. 
	const FRotator StartingOrient = StartingSyncState->GetOrientation_WorldSpace();
	FRotator TargetOrient = StartingOrient;

	bool bIsOrientationChanging = false;

	// Apply orientation changes (if any)
	if (!ProposedMove.AngularVelocity.IsZero())
	{
		TargetOrient += (ProposedMove.AngularVelocity * DeltaSeconds);
		bIsOrientationChanging = (TargetOrient != StartingOrient);
	}
	
	FVector MoveDelta = ProposedMove.LinearVelocity * DeltaSeconds;
	const FQuat OrientQuat = TargetOrient.Quaternion();
	FHitResult Hit(1.f);

	if (!MoveDelta.IsNearlyZero() || bIsOrientationChanging)
	{
		UMovementUtils::TrySafeMoveUpdatedComponent(UpdatedComponent, UpdatedPrimitive, MoveDelta, OrientQuat, true, Hit, ETeleportType::None, MoveRecord);
	}

	if (Hit.IsValidBlockingHit())
	{
		UMoverComponent* MoverComponent = GetMoverComponent();
		FMoverOnImpactParams ImpactParams(DefaultModeNames::Flying, Hit, MoveDelta);
		MoverComponent->HandleImpact(ImpactParams);
		// Try to slide the remaining distance along the surface.
		UMovementUtils::TryMoveToSlideAlongSurface(UpdatedComponent, UpdatedPrimitive, MoverComponent, MoveDelta, 1.f - Hit.Time, OrientQuat, Hit.Normal, Hit, true, MoveRecord);
	}

	CaptureFinalState(UpdatedComponent, MoveRecord, *StartingSyncState, OutputSyncState, DeltaSeconds);
}


bool UFlyingMode::AttemptTeleport(USceneComponent* UpdatedComponent, const FVector& TeleportPos, const FRotator& TeleportRot, const FMoverDefaultSyncState& StartingSyncState, FMoverTickEndData& Output)
{
	if (UpdatedComponent->GetOwner()->TeleportTo(TeleportPos, TeleportRot))
	{
		FMoverDefaultSyncState& OutputSyncState = Output.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

		OutputSyncState.SetTransforms_WorldSpace( UpdatedComponent->GetComponentLocation(),
												  UpdatedComponent->GetComponentRotation(),
												  StartingSyncState.GetVelocity_WorldSpace(),
												  nullptr); // no movement base

		UpdatedComponent->ComponentVelocity = StartingSyncState.GetVelocity_WorldSpace();

		GetBlackboard_Mutable()->Invalidate(CommonBlackboard::LastFloorResult);

		return true;
	}

	return false;
}


// TODO: replace this function with simply looking at/collapsing the MovementRecord
void UFlyingMode::CaptureFinalState(USceneComponent* UpdatedComponent, FMovementRecord& Record, const FMoverDefaultSyncState& StartSyncState, FMoverDefaultSyncState& OutputSyncState, const float DeltaSeconds) const
{
	const FVector FinalLocation = UpdatedComponent->GetComponentLocation();
	const FVector FinalVelocity = Record.GetRelevantVelocity();
	
	// TODO: Update Main/large movement record with substeps from our local record

	OutputSyncState.SetTransforms_WorldSpace(FinalLocation,
											  UpdatedComponent->GetComponentRotation(),
											  FinalVelocity,
											  nullptr); // no movement base

	UpdatedComponent->ComponentVelocity = FinalVelocity;
}

void UFlyingMode::OnRegistered(const FName ModeName)
{
	Super::OnRegistered(ModeName);

	CommonLegacySettings = GetMoverComponent()->FindSharedSettings<UCommonLegacyMovementSettings>();
	ensureMsgf(CommonLegacySettings, TEXT("Failed to find instance of CommonLegacyMovementSettings on %s. Movement may not function properly."), *GetPathNameSafe(this));
}


void UFlyingMode::OnUnregistered()
{
	CommonLegacySettings = nullptr;

	Super::OnUnregistered();
}