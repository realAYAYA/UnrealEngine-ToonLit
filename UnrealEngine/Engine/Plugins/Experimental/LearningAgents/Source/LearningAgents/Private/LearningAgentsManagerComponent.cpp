// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsManagerComponent.h"
#include "LearningAgentsManager.h"
#include "LearningAgentsHelpers.h"
#include "LearningLog.h"

ULearningAgentsManagerComponent::ULearningAgentsManagerComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
ULearningAgentsManagerComponent::ULearningAgentsManagerComponent(FVTableHelper& Helper) : Super(Helper) {}
ULearningAgentsManagerComponent::~ULearningAgentsManagerComponent() = default;

void ULearningAgentsManagerComponent::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		return;
	}

	Manager = GetOwner<ALearningAgentsManager>();

	if (!Manager)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Must be attached to a LearningAgentsManager Actor."), *GetName());
	}
}

void ULearningAgentsManagerComponent::AgentsAdded_Implementation(const TArray<int32>& AgentIds)
{
	// Can be overridden for additional logic when agents are added
}

void ULearningAgentsManagerComponent::AgentsRemoved_Implementation(const TArray<int32>& AgentIds)
{
	// Can be overridden for additional logic when agents are removed
}

void ULearningAgentsManagerComponent::AgentsReset_Implementation(const TArray<int32>& AgentIds)
{
	// Can be overridden for additional logic when agents are reset
}

void ULearningAgentsManagerComponent::OnAgentsAdded(const TArray<int32>& AgentIds)
{
	if (IsSetup())
	{
		for (ULearningAgentsHelper* Helper : HelperObjects)
		{
			Helper->OnAgentsAdded(AgentIds);
		}

		AgentsAdded(AgentIds);
	}
}

void ULearningAgentsManagerComponent::OnAgentsRemoved(const TArray<int32>& AgentIds)
{
	if (IsSetup())
	{
		for (ULearningAgentsHelper* Helper : HelperObjects)
		{
			Helper->OnAgentsRemoved(AgentIds);
		}

		AgentsRemoved(AgentIds);
	}
}

void ULearningAgentsManagerComponent::OnAgentsReset(const TArray<int32>& AgentIds)
{
	if (IsSetup())
	{
		for (ULearningAgentsHelper* Helper : HelperObjects)
		{
			Helper->OnAgentsReset(AgentIds);
		}	
	
		AgentsReset(AgentIds);
	}
}

UObject* ULearningAgentsManagerComponent::GetAgent(const int32 AgentId, const TSubclassOf<UObject> AgentClass) const
{
	if (!Manager)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Must be attached to a LearningAgentsManager Actor."), *GetName());
		return nullptr;
	}

	return Manager->GetAgent(AgentId, AgentClass);
}

void ULearningAgentsManagerComponent::GetAgents(const TArray<int32>& AgentIds, const TSubclassOf<UObject> AgentClass, TArray<UObject*>& OutAgents) const
{
	if (!Manager)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Must be attached to a LearningAgentsManager Actor."), *GetName());
		OutAgents.Empty();
		return;
	}

	Manager->GetAgents(OutAgents, AgentIds, AgentClass);
}

void ULearningAgentsManagerComponent::GetAllAgents(TArray<UObject*>& OutAgents, TArray<int32>& OutAgentIds, const TSubclassOf<UObject> AgentClass) const
{
	if (!Manager)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Must be attached to a LearningAgentsManager Actor."), *GetName());
		OutAgents.Empty();
		return;
	}

	Manager->GetAllAgents(OutAgents, OutAgentIds, AgentClass);
}

ALearningAgentsManager* ULearningAgentsManagerComponent::GetAgentManager(const TSubclassOf<ALearningAgentsManager> AgentManagerClass) const
{
	if (!Manager)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Must be attached to a LearningAgentsManager Actor."), *GetName());
		return nullptr;
	}
	
	return Manager;
}

const UObject* ULearningAgentsManagerComponent::GetAgent(const int32 AgentId) const
{
	UE_LEARNING_CHECKF(Manager, TEXT("Must be attached to a LearningAgentsManager Actor."));
	return Manager->GetAgent(AgentId);
}

UObject* ULearningAgentsManagerComponent::GetAgent(const int32 AgentId)
{
	UE_LEARNING_CHECKF(Manager, TEXT("Must be attached to a LearningAgentsManager Actor."));
	return Manager->GetAgent(AgentId);
}

bool ULearningAgentsManagerComponent::HasAgent(const int32 AgentId) const
{
	UE_LEARNING_CHECKF(Manager, TEXT("Must be attached to a LearningAgentsManager Actor."));
	return Manager->HasAgent(AgentId);
}

const ALearningAgentsManager* ULearningAgentsManagerComponent::GetAgentManager() const
{
	UE_LEARNING_CHECKF(Manager, TEXT("Must be attached to a LearningAgentsManager Actor."));
	return Manager;
}

ALearningAgentsManager* ULearningAgentsManagerComponent::GetAgentManager()
{
	UE_LEARNING_CHECKF(Manager, TEXT("Must be attached to a LearningAgentsManager Actor."));
	return Manager;
}

bool ULearningAgentsManagerComponent::HasAgentManager() const
{
	return Manager != nullptr;
}

bool ULearningAgentsManagerComponent::IsSetup() const
{
	return bIsSetup;
}

void ULearningAgentsManagerComponent::AddHelper(TObjectPtr<ULearningAgentsHelper> Object)
{
	UE_LEARNING_CHECK(Object);
	HelperObjects.Add(Object);
}