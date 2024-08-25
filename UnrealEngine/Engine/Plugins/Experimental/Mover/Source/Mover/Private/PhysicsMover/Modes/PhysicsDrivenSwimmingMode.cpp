// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/Modes/PhysicsDrivenSwimmingMode.h"

#include "Chaos/Character/CharacterGroundConstraint.h"
#include "DefaultMovementSet/LayeredMoves/BasicLayeredMoves.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "GameFramework/PhysicsVolume.h"
#include "Math/UnitConversion.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/WaterMovementUtils.h"
#include "MoverComponent.h"
#include "PhysicsMover/PhysicsMovementUtils.h"
#if WITH_EDITOR
#include "Backends/MoverNetworkPhysicsLiaison.h"
#include "Internationalization/Text.h"
#include "Misc/DataValidation.h"
#endif // WITH_EDITOR

#include UE_INLINE_GENERATED_CPP_BY_NAME(PhysicsDrivenSwimmingMode)

#define LOCTEXT_NAMESPACE "PhysicsDrivenSwimmingMode"

UPhysicsDrivenSwimmingMode::UPhysicsDrivenSwimmingMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPhysicsDrivenSwimmingMode::UpdateConstraintSettings(Chaos::FCharacterGroundConstraint& Constraint) const
{
	Constraint.SetSwingTorqueLimit(FUnitConversion::Convert(3000.0f, EUnit::NewtonMeters, EUnit::KilogramCentimetersSquaredPerSecondSquared));
	Constraint.SetRadialForceLimit(0.0f);
	Constraint.SetFrictionForceLimit(0.0f);
	Constraint.SetTwistTorqueLimit(0.0f);
}

#if WITH_EDITOR
EDataValidationResult UPhysicsDrivenSwimmingMode::IsDataValid(FDataValidationContext& Context) const
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

void UPhysicsDrivenSwimmingMode::OnSimulationTick(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	const UMoverComponent* MoverComp = GetMoverComponent();

	const FMoverTickStartData& StartState = Params.StartState;
	USceneComponent* UpdatedComponent = Params.UpdatedComponent;
	UPrimitiveComponent* UpdatedPrimitive = Params.UpdatedPrimitive;
	FProposedMove ProposedMove = Params.ProposedMove;
	
	const FVector UpDir = GetMoverComponent()->GetUpDirection();
	
	const FCharacterDefaultInputs* CharacterInputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();

	const FMoverDefaultSyncState* StartingSyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	check(StartingSyncState);

	FMoverDefaultSyncState& OutputSyncState = OutputState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();

	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;

	if ((ProposedMove.bHasTargetLocation && AttemptTeleport(UpdatedComponent, ProposedMove.TargetLocation, UpdatedComponent->GetComponentRotation(), StartingSyncState->GetVelocity_WorldSpace(), OutputState)) ||	// Teleport
		(CharacterInputs->bIsJumpJustPressed && AttemptJump(SurfaceSwimmingWaterControlSettings.JumpMultiplier*CommonLegacySettings->JumpUpwardsSpeed, OutputState)))
	{
		OutputState.MovementEndState.RemainingMs = Params.TimeStep.StepMs;
		return;
	}
	
	UMoverBlackboard* SimBlackboard = GetBlackboard_Mutable();
	if (!SimBlackboard)
	{
		OutputSyncState = *StartingSyncState;
		return;
	}
	SimBlackboard->Invalidate(CommonBlackboard::LastFloorResult);
	SimBlackboard->Invalidate(CommonBlackboard::LastWaterResult);
	
	OutputSyncState.MoveDirectionIntent = ProposedMove.bHasDirIntent ? ProposedMove.DirectionIntent : FVector::ZeroVector;

	// Floor query
	FFloorCheckResult FloorResult;
	FWaterCheckResult WaterResult;
	
	float PawnHalfHeight;
	float PawnRadius;
	UpdatedPrimitive->CalcBoundingCylinder(PawnRadius, PawnHalfHeight);

	const float QueryDistance= 2.0f * PawnHalfHeight;
	
	UPhysicsMovementUtils::FloorSweep(StartingSyncState->GetLocation_WorldSpace(), StartingSyncState->GetVelocity_WorldSpace() * DeltaSeconds,
		UpdatedPrimitive, UpDir, PawnRadius, QueryDistance, CommonLegacySettings->MaxWalkSlopeCosine, TargetHeight, FloorResult, WaterResult);

	SimBlackboard->Set(CommonBlackboard::LastFloorResult, FloorResult);
	SimBlackboard->Set(CommonBlackboard::LastWaterResult, WaterResult);
	
	if (WaterResult.IsSwimmableVolume())
	{
		const bool bIsWithinReach = FloorResult.FloorDist <= TargetHeight;
		const bool bWalkTrigger = WaterResult.WaterSplineData.ImmersionDepth < CommonLegacySettings->SwimmingStopImmersionDepth;
		const bool bFallTrigger = FMath::Clamp((WaterResult.WaterSplineData.ImmersionDepth + TargetHeight) / (2 * TargetHeight), -2.f, 2.f) < -1.f;
	
		FRotator TargetOrient = StartingSyncState->GetOrientation_WorldSpace();
		if (!ProposedMove.AngularVelocity.IsZero())
		{
			TargetOrient += (ProposedMove.AngularVelocity * DeltaSeconds);
		}

		FVector TargetVel = ProposedMove.LinearVelocity;		
		if (const APhysicsVolume* CurPhysVolume = UpdatedComponent->GetPhysicsVolume())
		{
			// Discount G Forces as Buoyancy accounts for it
			TargetVel -= (CurPhysVolume->GetGravityZ() * FVector::UpVector * DeltaSeconds);
		}
		
		FVector TargetPos = StartingSyncState->GetLocation_WorldSpace();
		TargetPos += TargetVel * DeltaSeconds;
	
		OutputSyncState.SetTransforms_WorldSpace(
			TargetPos,
			TargetOrient,
			TargetVel);
	
		if (bWalkTrigger && bIsWithinReach)
		{
			OutputState.MovementEndState.NextModeName = DefaultModeNames::Walking;
		}
		else if (bFallTrigger)
		{
			OutputState.MovementEndState.NextModeName = DefaultModeNames::Falling;
		}
		else
		{
			OutputState.MovementEndState.NextModeName = DefaultModeNames::Swimming;
		}
	}
	else
	{
		OutputState.MovementEndState.NextModeName = DefaultModeNames::Falling;
	}
	
	OutputState.MovementEndState.RemainingMs = 0.0f;
}

bool UPhysicsDrivenSwimmingMode::AttemptJump(float UpwardsSpeed, FMoverTickEndData& OutputState)
{
	// TODO: This should check if a jump is even allowed
 	TSharedPtr<FLayeredMove_JumpImpulse> JumpMove = MakeShared<FLayeredMove_JumpImpulse>();
	JumpMove->UpwardsSpeed = UpwardsSpeed;
	OutputState.SyncState.LayeredMoves.QueueLayeredMove(JumpMove);
	OutputState.MovementEndState.NextModeName = DefaultModeNames::Falling;
	return true;
}

bool UPhysicsDrivenSwimmingMode::AttemptTeleport(USceneComponent* UpdatedComponent, const FVector& TeleportPos, const FRotator& TeleportRot, const FVector& PriorVelocity, FMoverTickEndData& Output)
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

#undef LOCTEXT_NAMESPACE