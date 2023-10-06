// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "Blueprint/StateTreeNodeBlueprintBase.h"
#include "StateTreeExecutionContext.h"
#include "BlueprintNodeHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeTaskBlueprintBase)

//----------------------------------------------------------------------//
//  UStateTreeTaskBlueprintBase
//----------------------------------------------------------------------//

UStateTreeTaskBlueprintBase::UStateTreeTaskBlueprintBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bShouldStateChangeOnReselect(true)
	, bShouldCallTick(true)
	, bShouldCallTickOnlyOnEvents(false)
	, bShouldCopyBoundPropertiesOnTick(true)
	, bShouldCopyBoundPropertiesOnExitState(true)
{
	bHasExitState = BlueprintNodeHelpers::HasBlueprintFunction(TEXT("ReceiveExitState"), *this, *StaticClass());
	bHasStateCompleted = BlueprintNodeHelpers::HasBlueprintFunction(TEXT("ReceiveStateCompleted"), *this, *StaticClass());
	bHasLatentEnterState = BlueprintNodeHelpers::HasBlueprintFunction(TEXT("ReceiveLatentEnterState"), *this, *StaticClass());
	bHasLatentTick = BlueprintNodeHelpers::HasBlueprintFunction(TEXT("ReceiveLatentTick"), *this, *StaticClass());
PRAGMA_DISABLE_DEPRECATION_WARNINGS		
	bHasEnterState_DEPRECATED = BlueprintNodeHelpers::HasBlueprintFunction(TEXT("ReceiveEnterState"), *this, *StaticClass());
	bHasTick_DEPRECATED = BlueprintNodeHelpers::HasBlueprintFunction(TEXT("ReceiveTick"), *this, *StaticClass());
PRAGMA_ENABLE_DEPRECATION_WARNINGS	
}

EStateTreeRunStatus UStateTreeTaskBlueprintBase::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition)
{
	// Task became active, cache event queue and owner.
	SetCachedInstanceDataFromContext(Context);

	// Reset status to running since the same task may be restarted.
	RunStatus = EStateTreeRunStatus::Running;

	if (bHasLatentEnterState)
	{
		// Note: the name contains latent just to differentiate it from the deprecated version (the old version did not allow latent actions to be started).
		ReceiveLatentEnterState(Transition);
	}
PRAGMA_DISABLE_DEPRECATION_WARNINGS		
	else if (bHasEnterState_DEPRECATED)
	{
		return ReceiveEnterState(Transition);
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
	return RunStatus;
}

void UStateTreeTaskBlueprintBase::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition)
{
	if (bHasExitState)
	{
		ReceiveExitState(Transition);
	}

	if (UWorld* CurrentWorld = GetWorld())
	{
		CurrentWorld->GetLatentActionManager().RemoveActionsForObject(this);
		CurrentWorld->GetTimerManager().ClearAllTimersForObject(this);
	}

	// Task became inactive, clear cached event queue and owner.
	ClearCachedInstanceData();
}

void UStateTreeTaskBlueprintBase::StateCompleted(FStateTreeExecutionContext& Context, const EStateTreeRunStatus CompletionStatus, const FStateTreeActiveStates& CompletedActiveStates)
{
	if (bHasStateCompleted)
	{
		ReceiveStateCompleted(CompletionStatus, CompletedActiveStates);
	}
}

EStateTreeRunStatus UStateTreeTaskBlueprintBase::Tick(FStateTreeExecutionContext& Context, const float DeltaTime)
{
	if (bHasLatentTick)
	{
		// Note: the name contains latent just to differentiate it from the deprecated version (the old version did not allow latent actions to be started).
		ReceiveLatentTick(DeltaTime);
	}
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	else if (bHasTick_DEPRECATED)
	{
		return ReceiveTick(DeltaTime);
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
	return RunStatus;
}

//----------------------------------------------------------------------//
//  FStateTreeBlueprintTaskWrapper
//----------------------------------------------------------------------//

EDataValidationResult FStateTreeBlueprintTaskWrapper::Compile(FStateTreeDataView InstanceDataView, TArray<FText>& ValidationMessages)
{
	const UStateTreeTaskBlueprintBase& InstanceData = InstanceDataView.Get<UStateTreeTaskBlueprintBase>();
	
	// Copy over ticking related options.
	bShouldStateChangeOnReselect = InstanceData.bShouldStateChangeOnReselect;
	
	bShouldCallTick = InstanceData.bShouldCallTick || InstanceData.bHasLatentTick;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	bShouldCallTick |= InstanceData.bHasTick_DEPRECATED;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	bShouldCallTickOnlyOnEvents = InstanceData.bShouldCallTickOnlyOnEvents;
	bShouldCopyBoundPropertiesOnTick = InstanceData.bShouldCopyBoundPropertiesOnTick;
	bShouldCopyBoundPropertiesOnExitState = InstanceData.bShouldCopyBoundPropertiesOnExitState;

	return EDataValidationResult::Valid;
}

EStateTreeRunStatus FStateTreeBlueprintTaskWrapper::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	UStateTreeTaskBlueprintBase* Instance = Context.GetInstanceDataPtr<UStateTreeTaskBlueprintBase>(*this);
	check(Instance);
	return Instance->EnterState(Context, Transition);
}

void FStateTreeBlueprintTaskWrapper::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	UStateTreeTaskBlueprintBase* Instance = Context.GetInstanceDataPtr<UStateTreeTaskBlueprintBase>(*this);
	check(Instance);
	Instance->ExitState(Context, Transition);
}

void FStateTreeBlueprintTaskWrapper::StateCompleted(FStateTreeExecutionContext& Context, const EStateTreeRunStatus CompletionStatus, const FStateTreeActiveStates& CompletedActiveStates) const
{
	UStateTreeTaskBlueprintBase* Instance = Context.GetInstanceDataPtr<UStateTreeTaskBlueprintBase>(*this);
	check(Instance);
	Instance->StateCompleted(Context, CompletionStatus, CompletedActiveStates);
}

EStateTreeRunStatus FStateTreeBlueprintTaskWrapper::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	UStateTreeTaskBlueprintBase* Instance = Context.GetInstanceDataPtr<UStateTreeTaskBlueprintBase>(*this);
	check(Instance);
	return Instance->Tick(Context, DeltaTime);
}

