// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprint/StateTreeNodeBlueprintBase.h"
#include "AIController.h"
#include "StateTreeExecutionContext.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeNodeBlueprintBase)

UWorld* UStateTreeNodeBlueprintBase::GetWorld() const
{
	// The items are duplicated as the State Tree execution context as outer, so this should be essentially the same as GetWorld() on StateTree context.
	// The CDO is used by the BP editor to check for certain functionality, make it return nullptr so that the GetWorld() passes as overridden. 
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		if (CachedOwner != nullptr)
		{
			return CachedOwner->GetWorld();
		}
		if (UObject* Outer = GetOuter())
		{
			return Outer->GetWorld();
		}
	}
	
	return nullptr;
}

AActor* UStateTreeNodeBlueprintBase::GetOwnerActor(const FStateTreeExecutionContext& Context) const
{
	if (const AAIController* Controller = Cast<AAIController>(Context.GetOwner()))
	{
		return Controller->GetPawn();
	}
	
	return Cast<AActor>(Context.GetOwner());
}

void UStateTreeNodeBlueprintBase::SetCachedInstanceDataFromContext(const FStateTreeExecutionContext& Context) const
{
	if (FStateTreeInstanceData* InstanceData = Context.GetMutableInstanceData())
	{
		InstanceStorage = &InstanceData->GetMutableStorage();
	}
	CachedState = Context.GetCurrentlyProcessedState();
	CachedOwner = Context.GetOwner();
}

void UStateTreeNodeBlueprintBase::ClearCachedInstanceData() const
{
	InstanceStorage = nullptr;
	CachedState = FStateTreeStateHandle::Invalid;
	CachedOwner = nullptr;
}

void UStateTreeNodeBlueprintBase::SendEvent(const FStateTreeEvent& Event)
{
	if (InstanceStorage == nullptr || CachedOwner == nullptr)
	{
		UE_VLOG_UELOG(this, LogStateTree, Error, TEXT("Trying to call SendEvent() while node is not active. Use SendEvent() on UStateTreeComponent instead for sending signals externally."));
		return;
	}
	InstanceStorage->GetMutableEventQueue().SendEvent(CachedOwner, Event.Tag, Event.Payload, Event.Origin);
}

void UStateTreeNodeBlueprintBase::RequestTransition(const FStateTreeStateLink& TargetState, const EStateTreeTransitionPriority Priority)
{
	if (InstanceStorage == nullptr || CachedOwner == nullptr)
	{
		UE_VLOG_UELOG(this, LogStateTree, Error, TEXT("Trying to call SendEvent() while node is not active. Use SendEvent() on UStateTreeComponent instead for sending signals externally."));
		return;
	}

	FStateTreeTransitionRequest Request;
	Request.SourceState = CachedState;
	Request.TargetState = TargetState.StateHandle;
	Request.Priority = Priority;
	
	InstanceStorage->AddTransitionRequest(CachedOwner, Request);
}
