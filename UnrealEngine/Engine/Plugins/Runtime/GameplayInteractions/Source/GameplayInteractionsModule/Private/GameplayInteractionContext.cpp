// Copyright Epic Games, Inc. All Rights Reserved.


#include "GameplayInteractionContext.h"
#include "GameplayInteractionSmartObjectBehaviorDefinition.h"
#include "GameplayInteractionsTypes.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Subsystems/WorldSubsystem.h"
#include "StateTreeReference.h"
#include "VisualLogger/VisualLogger.h"
#include "GameplayInteractionStateTreeSchema.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayInteractionContext)

bool FGameplayInteractionContext::Activate(const UGameplayInteractionSmartObjectBehaviorDefinition& InDefinition)
{
	Definition = &InDefinition;
	check(Definition);
	
	const FStateTreeReference& StateTreeReference = Definition->StateTreeReference;
	const UStateTree* StateTree = StateTreeReference.GetStateTree();

	if (!IsValid())
	{
		UE_LOG(LogGameplayInteractions, Error, TEXT("Failed to activate interaction. Context is not properly setup."));
		return false;
	}
	
	if (StateTree == nullptr)
	{
		UE_VLOG_UELOG(ContextActor, LogGameplayInteractions, Error,
			TEXT("Failed to activate interaction for %s. Definition %s doesn't point to a valid StateTree asset."),
			*GetNameSafe(ContextActor),
			*Definition.GetFullName());
		return false;
	}

	FStateTreeExecutionContext StateTreeContext(*ContextActor, *StateTree, StateTreeInstanceData);

	if (!StateTreeContext.IsValid())
	{
		UE_VLOG_UELOG(ContextActor, LogGameplayInteractions, Error,
			TEXT("Failed to activate interaction for %s. Unable to initialize StateTree execution context for StateTree asset: %s."),
			*GetNameSafe(ContextActor),
			*StateTree->GetFullName());
		return false;
	}

	if (!ValidateSchema(StateTreeContext))
	{
		return false;
	}
	
	if (!SetContextRequirements(StateTreeContext))
	{
		UE_VLOG_UELOG(ContextActor, LogGameplayInteractions, Error,
			TEXT("Failed to activate interaction for %s. Unable to provide all external data views for StateTree asset: %s."),
			*GetNameSafe(ContextActor),
			*StateTree->GetFullName());
		return false;
	}

	StateTreeContext.Start();
	
	return true;
}

bool FGameplayInteractionContext::Tick(const float DeltaTime)
{
	if (Definition == nullptr || !IsValid())
	{
		return false;
	}
	
	const FStateTreeReference& StateTreeReference = Definition->StateTreeReference;
	const UStateTree* StateTree = StateTreeReference.GetStateTree();
	FStateTreeExecutionContext StateTreeContext(*ContextActor, *StateTree, StateTreeInstanceData);

	EStateTreeRunStatus RunStatus = EStateTreeRunStatus::Unset;
	if (SetContextRequirements(StateTreeContext))
	{
		RunStatus = StateTreeContext.Tick(DeltaTime);
	}

	return RunStatus == EStateTreeRunStatus::Running;
}

void FGameplayInteractionContext::Deactivate()
{
	if (Definition == nullptr)
	{
		return;
	}
	
	const FStateTreeReference& StateTreeReference = Definition->StateTreeReference;
	const UStateTree* StateTree = StateTreeReference.GetStateTree();
	FStateTreeExecutionContext StateTreeContext(*ContextActor, *StateTree, StateTreeInstanceData);

	if (SetContextRequirements(StateTreeContext))
	{
		StateTreeContext.Stop();
	}
}

void FGameplayInteractionContext::SendEvent(const FStateTreeEvent& Event)
{
	if (Definition == nullptr)
	{
		return;
	}
	
	const FStateTreeReference& StateTreeReference = Definition->StateTreeReference;
	const UStateTree* StateTree = StateTreeReference.GetStateTree();
	FStateTreeExecutionContext StateTreeContext(*ContextActor, *StateTree, StateTreeInstanceData);
	StateTreeContext.SendEvent(Event);
}

bool FGameplayInteractionContext::ValidateSchema(const FStateTreeExecutionContext& StateTreeContext) const
{
	// Ensure that the actor and smart object match the schema.
	const UGameplayInteractionStateTreeSchema* Schema = Cast<UGameplayInteractionStateTreeSchema>(StateTreeContext.GetStateTree()->GetSchema());
	if (Schema == nullptr)
	{
		UE_VLOG_UELOG(ContextActor, LogGameplayInteractions, Error,
			TEXT("Failed to activate interaction for %s. Expecting %s schema for StateTree asset: %s."),
			*GetNameSafe(ContextActor),
			*GetNameSafe(UGameplayInteractionStateTreeSchema::StaticClass()),
			*GetFullNameSafe(StateTreeContext.GetStateTree()));

		return false;
	}
	if (!ContextActor || !ContextActor->IsA(Schema->GetContextActorClass()))
	{
		UE_VLOG_UELOG(ContextActor, LogGameplayInteractions, Error,
			TEXT("Failed to activate interaction for %s. Expecting Actor to be of type %s (found %s) for StateTree asset: %s."),
			*GetNameSafe(ContextActor),
			*GetNameSafe(Schema->GetContextActorClass()),
			*GetNameSafe(ContextActor ? ContextActor->GetClass() : nullptr),
			*GetFullNameSafe(StateTreeContext.GetStateTree()));

		return false;
	}
	if (!SmartObjectActor || !SmartObjectActor->IsA(Schema->GetSmartObjectActorClass()))
	{
		UE_VLOG_UELOG(ContextActor, LogGameplayInteractions, Error,
			TEXT("Failed to activate interaction for %s. SmartObject Actor to be of type %s (found %s) for StateTree asset: %s."),
			*GetNameSafe(ContextActor),
			*GetNameSafe(Schema->GetSmartObjectActorClass()),
			*GetNameSafe(SmartObjectActor ? SmartObjectActor->GetClass() : nullptr),
			*GetFullNameSafe(StateTreeContext.GetStateTree()));

		return false;
	}

	return true;
}

bool FGameplayInteractionContext::SetContextRequirements(FStateTreeExecutionContext& StateTreeContext) const
{
	if (!StateTreeContext.IsValid())
	{
		return false;
	}

	if (!::IsValid(Definition))
	{
		return false;
	}
	const FStateTreeReference& StateTreeReference = Definition->StateTreeReference;
	StateTreeContext.SetParameters(StateTreeReference.GetParameters());

	for (const FStateTreeExternalDataDesc& ItemDesc : StateTreeContext.GetContextDataDescs())
	{
		if (ItemDesc.Name == UE::GameplayInteraction::Names::ContextActor)
		{
			StateTreeContext.SetExternalData(ItemDesc.Handle, FStateTreeDataView(ContextActor));
		}
		else if (ItemDesc.Name == UE::GameplayInteraction::Names::SmartObjectActor)
		{
			StateTreeContext.SetExternalData(ItemDesc.Handle, FStateTreeDataView(SmartObjectActor));
		}
		else if (ItemDesc.Name == UE::GameplayInteraction::Names::SmartObjectClaimedHandle)
        {
            StateTreeContext.SetExternalData(ItemDesc.Handle, FStateTreeDataView(FSmartObjectClaimHandle::StaticStruct(), (uint8*)&ClaimedHandle));
        }
		else if (ItemDesc.Name == UE::GameplayInteraction::Names::AbortContext)
		{
			StateTreeContext.SetExternalData(ItemDesc.Handle, FStateTreeDataView(FGameplayInteractionAbortContext::StaticStruct(), (uint8*)&AbortContext));
		}
	}

	checkf(ContextActor != nullptr, TEXT("Should never reach this point with an invalid InteractorActor since it is required to get a valid StateTreeContext."));
	const UWorld* World = ContextActor->GetWorld();
	for (const FStateTreeExternalDataDesc& ItemDesc : StateTreeContext.GetExternalDataDescs())
	{
		if (ItemDesc.Struct != nullptr)
		{
			if (World != nullptr && ItemDesc.Struct->IsChildOf(UWorldSubsystem::StaticClass()))
			{
				UWorldSubsystem* Subsystem = World->GetSubsystemBase(Cast<UClass>(const_cast<UStruct*>(ToRawPtr(ItemDesc.Struct))));
				StateTreeContext.SetExternalData(ItemDesc.Handle, FStateTreeDataView(Subsystem));
			}
			else if (ItemDesc.Struct->IsChildOf(AActor::StaticClass()))
			{
				StateTreeContext.SetExternalData(ItemDesc.Handle, FStateTreeDataView(ContextActor));
			}
		}
	}

	return StateTreeContext.AreExternalDataViewsValid();
}
