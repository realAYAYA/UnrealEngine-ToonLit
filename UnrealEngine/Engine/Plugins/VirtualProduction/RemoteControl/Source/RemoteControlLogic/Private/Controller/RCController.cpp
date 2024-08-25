// Copyright Epic Games, Inc. All Rights Reserved.

#include "Controller/RCController.h"

#include "RCVirtualPropertyContainer.h"
#include "RemoteControlPreset.h"
#include "Action/RCActionContainer.h"
#include "Behaviour/RCBehaviour.h"
#include "Behaviour/RCBehaviourNode.h"

#define LOCTEXT_NAMESPACE "RCController"

void URCController::UpdateEntityIds(const TMap<FGuid, FGuid>& InEntityIdMap)
{
	for (URCBehaviour* Behaviour : Behaviours)
	{
		if (Behaviour)
		{
			Behaviour->UpdateEntityIds(InEntityIdMap);
		}
	}
	
	Super::UpdateEntityIds(InEntityIdMap);
}

#if WITH_EDITOR
void URCController::PostEditUndo()
{
	Super::PostEditUndo();

	OnBehaviourListModified.Broadcast();
}
#endif

URCBehaviour* URCController::AddBehaviour(TSubclassOf<URCBehaviourNode> InBehaviourNodeClass)
{
	URCBehaviour* NewBehaviour = CreateBehaviour(InBehaviourNodeClass);
	if (!ensure(NewBehaviour))
	{
		return nullptr;
	}

	NewBehaviour->Initialize();
	
	Behaviours.Add(NewBehaviour);

	return NewBehaviour;
}

URCBehaviour* URCController::CreateBehaviour(TSubclassOf<URCBehaviourNode> InBehaviourNodeClass)
{
	const URCBehaviourNode* DefaultBehaviourNode = Cast<URCBehaviourNode>(InBehaviourNodeClass->GetDefaultObject());
	
	URCBehaviour* NewBehaviour = NewObject<URCBehaviour>(this, DefaultBehaviourNode->GetBehaviourClass(), NAME_None, RF_Transactional);
	NewBehaviour->BehaviourNodeClass = InBehaviourNodeClass;
	NewBehaviour->Id = FGuid::NewGuid();
	NewBehaviour->ActionContainer->PresetWeakPtr = PresetWeakPtr;
	NewBehaviour->ControllerWeakPtr = this;
	
	if (!DefaultBehaviourNode->IsSupported(NewBehaviour))
	{
		return nullptr;
	}
	
	return NewBehaviour;
}

URCBehaviour* URCController::CreateBehaviourWithoutCheck(TSubclassOf<URCBehaviourNode> InBehaviourNodeClass)
{
	const URCBehaviourNode* DefaultBehaviourNode = Cast<URCBehaviourNode>(InBehaviourNodeClass->GetDefaultObject());
	
	URCBehaviour* NewBehaviour = NewObject<URCBehaviour>(this, DefaultBehaviourNode->GetBehaviourClass(), NAME_None, RF_Transactional);
	NewBehaviour->BehaviourNodeClass = InBehaviourNodeClass;
	NewBehaviour->Id = FGuid::NewGuid();
	NewBehaviour->ActionContainer->PresetWeakPtr = PresetWeakPtr;
	NewBehaviour->ControllerWeakPtr = this;
	
	return NewBehaviour;
}

int32 URCController::RemoveBehaviour(URCBehaviour* InBehaviour)
{
	return Behaviours.Remove(InBehaviour);
}

int32 URCController::RemoveBehaviour(const FGuid InBehaviourId)
{
	int32 RemovedCount = 0;
	
	for (auto BehaviourIt = Behaviours.CreateIterator(); BehaviourIt; ++BehaviourIt)
	{
		if (const URCBehaviour* Behaviour = *BehaviourIt; Behaviour->Id == InBehaviourId)
		{
			BehaviourIt.RemoveCurrent();
			RemovedCount++;
		}
	}

	return RemovedCount;
}

void URCController::EmptyBehaviours()
{
	Behaviours.Empty();
}

void URCController::ExecuteBehaviours(const bool bIsPreChange/* = false*/)
{
	for (URCBehaviour* Behaviour : Behaviours)
	{
		if (!Behaviour->bIsEnabled)
		{
			continue;
		}

		if (bIsPreChange && !Behaviour->bExecuteBehavioursDuringPreChange)
		{
			continue;
		}

		Behaviour->Execute();

	}
}

void URCController::OnPreChangePropertyValue()
{
	constexpr bool bIsPreChange = true;
	ExecuteBehaviours(bIsPreChange);
}

void URCController::OnModifyPropertyValue()
{
	ExecuteBehaviours();
}

URCBehaviour* URCController::DuplicateBehaviour(URCController* InController, URCBehaviour* InBehaviour)
{
	URCBehaviour* NewBehaviour = DuplicateObject<URCBehaviour>(InBehaviour, InController);

	NewBehaviour->ControllerWeakPtr = InController;

	InController->Behaviours.Add(NewBehaviour);

	return NewBehaviour;
}

#undef LOCTEXT_NAMESPACE /* RCController */ 