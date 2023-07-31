// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprint/StateTreeNodeBlueprintBase.h"
#include "AIController.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeNodeBlueprintBase)

UWorld* UStateTreeNodeBlueprintBase::GetWorld() const
{
	// The items are duplicated as the StateTreeExecution context as outer, so this should be essentially the same as GetWorld() on StateTree context.
	// The CDO is used by the BP editor to check for certain functionality, make it return nullptr so that the GetWorld() passes as overridden. 
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
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

void UStateTreeNodeBlueprintBase::SendEvent(const FStateTreeEvent& Event)
{
	if (CurrentContext == nullptr)
	{
		UE_VLOG_UELOG(this, LogStateTree, Error, TEXT("Trying to call SendEvent() outside StateTree tick. Use SendEvent() on UStateTreeComponent instead for sending signals externally."));
		return;
	}
	CurrentContext->SendEvent(Event);
}

