// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsManager.h"
#include "LearningAgentsManagerComponent.h"

#include "LearningArray.h"
#include "LearningArrayMap.h"
#include "LearningLog.h"

#include "UObject/ScriptInterface.h"
#include "EngineDefines.h"

ALearningAgentsManager::ALearningAgentsManager() : Super(FObjectInitializer::Get()) {}
ALearningAgentsManager::ALearningAgentsManager(FVTableHelper& Helper) : Super(Helper) {}
ALearningAgentsManager::~ALearningAgentsManager() = default;

void ALearningAgentsManager::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	// Allocate Instance Data
	InstanceData = MakeShared<UE::Learning::FArrayMap>();

	// Pre-populate the vacant ids
	OnEventAgentIds.Reserve(MaxAgentNum);
	OccupiedAgentIds.Reserve(MaxAgentNum);
	VacantAgentIds.Reserve(MaxAgentNum);
	Agents.Reserve(MaxAgentNum);
	for (int32 i = MaxAgentNum - 1; i >= 0; i--)
	{
		VacantAgentIds.Push(i);
		Agents.Add(nullptr);
	}

	UpdateAgentSets();
}

int32 ALearningAgentsManager::GetMaxAgentNum() const
{
	return MaxAgentNum;
}

int32 ALearningAgentsManager::AddAgent(UObject* Agent)
{
	if (Agent == nullptr)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Attempted to add an agent but agent is nullptr."), *GetName());
		return INDEX_NONE;
	}

	if (VacantAgentIds.IsEmpty())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Attempting to add an agent but we have no more vacant ids. Increase MaxAgentNum (%d) or remove unused agents."), *GetName(), MaxAgentNum);
		return INDEX_NONE;
	}

	// Add Agent

	const int32 AgentId = VacantAgentIds.Pop();
	Agents[AgentId] = Agent;
	OccupiedAgentIds.Add(AgentId);
	UpdateAgentSets();

	OnEventAgentIds.SetNumUninitialized(0, false);
	OnEventAgentIds.Add(AgentId);

	// Call OnAgentsAdded Event

	UE_LOG(LogLearning, Display, TEXT("%s: Adding Agent %s with id %i."), *GetName(), *Agent->GetName(), AgentId);

	TInlineComponentArray<ULearningAgentsManagerComponent*> ManagerComponents;
	GetComponents<ULearningAgentsManagerComponent>(ManagerComponents);

	for (ULearningAgentsManagerComponent* ManagerComponent : ManagerComponents)
	{
		ManagerComponent->OnAgentsAdded(OnEventAgentIds);
	}

	// Return new id

	return AgentId;
}

void ALearningAgentsManager::AddAgents(TArray<int32>& OutAgentIds, const TArray<UObject*>& InAgents)
{
	OnEventAgentIds.SetNumUninitialized(0, false);
	OutAgentIds.SetNumUninitialized(InAgents.Num());

	for (int32 AgentIdx = 0; AgentIdx < InAgents.Num(); AgentIdx++)
	{
		if (InAgents[AgentIdx] == nullptr)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Attempted to add an agent but agent is nullptr."), *GetName());
			OutAgentIds[AgentIdx] = INDEX_NONE;
			continue;
		}

		if (VacantAgentIds.IsEmpty())
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Attempting to add an agent but we have no more vacant ids. Increase MaxAgentNum (%d) or remove unused agents."), *GetName(), MaxAgentNum);
			OutAgentIds[AgentIdx] = INDEX_NONE;
			continue;
		}

		// Add Agent

		OutAgentIds[AgentIdx] = VacantAgentIds.Pop();
		Agents[OutAgentIds[AgentIdx]] = InAgents[AgentIdx];
		OccupiedAgentIds.Add(OutAgentIds[AgentIdx]);
		OnEventAgentIds.Add(OutAgentIds[AgentIdx]);
	}

	UpdateAgentSets();

	// Call OnAgentsAdded Event

	UE_LOG(LogLearning, Display, TEXT("%s: Adding Agents %s."), *GetName(), *UE::Learning::Array::FormatInt32(OnEventAgentIds));

	TInlineComponentArray<ULearningAgentsManagerComponent*> ManagerComponents;
	GetComponents<ULearningAgentsManagerComponent>(ManagerComponents);

	for (ULearningAgentsManagerComponent* ManagerComponent : ManagerComponents)
	{
		ManagerComponent->OnAgentsAdded(OnEventAgentIds);
	}
}

void ALearningAgentsManager::RemoveAgent(const int32 AgentId)
{
	if (AgentId == INDEX_NONE)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Attempting to remove an agent with id of -1."), *GetName());
		return;
	}

	const int32 RemovedCount = OccupiedAgentIds.RemoveSingleSwap(AgentId, false);

	if (RemovedCount == 0)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Trying to remove an agent with id of %i but it was not found."), *GetName(), AgentId);
		return;
	}

	UE_LEARNING_CHECKF(RemovedCount == 1, TEXT("Somehow agent id was included multiple times in set."));

	// Remove Agent

	VacantAgentIds.Push(AgentId);
	Agents[AgentId] = nullptr;
	UpdateAgentSets();

	OnEventAgentIds.SetNumUninitialized(0, false);
	OnEventAgentIds.Add(AgentId);

	// Call OnAgentsRemoved Event

	UE_LOG(LogLearning, Display, TEXT("%s: Removing Agent %i."), *GetName(), AgentId);

	TInlineComponentArray<ULearningAgentsManagerComponent*> ManagerComponents;
	GetComponents<ULearningAgentsManagerComponent>(ManagerComponents);

	for (ULearningAgentsManagerComponent* ManagerComponent : ManagerComponents)
	{
		ManagerComponent->OnAgentsRemoved(OnEventAgentIds);
	}
}

void ALearningAgentsManager::RemoveAgents(const TArray<int32>& AgentIds)
{
	OnEventAgentIds.SetNumUninitialized(0, false);

	for (int32 AgentIdx = 0; AgentIdx < AgentIds.Num(); AgentIdx++)
	{
		if (AgentIds[AgentIdx] == INDEX_NONE)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Attempting to remove an agent with id of -1."), *GetName());
			continue;
		}

		const int32 RemovedCount = OccupiedAgentIds.RemoveSingleSwap(AgentIds[AgentIdx], false);

		if (RemovedCount == 0)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Trying to remove an agent with id of %i but it was not found."), *GetName(), AgentIds[AgentIdx]);
			continue;
		}

		UE_LEARNING_CHECKF(RemovedCount == 1, TEXT("Somehow agent id was included multiple times in set."));

		// Remove Agent

		VacantAgentIds.Push(AgentIds[AgentIdx]);
		Agents[AgentIds[AgentIdx]] = nullptr;
		OnEventAgentIds.Add(AgentIds[AgentIdx]);
	}

	UpdateAgentSets();

	// Call OnAgentsRemoved Event

	UE_LOG(LogLearning, Display, TEXT("%s: Removing Agents %s."), *GetName(), *UE::Learning::Array::FormatInt32(OnEventAgentIds));

	TInlineComponentArray<ULearningAgentsManagerComponent*> ManagerComponents;
	GetComponents<ULearningAgentsManagerComponent>(ManagerComponents);

	for (ULearningAgentsManagerComponent* ManagerComponent : ManagerComponents)
	{
		ManagerComponent->OnAgentsRemoved(OnEventAgentIds);
	}
}

void ALearningAgentsManager::RemoveAllAgents()
{
	// We need to make a copy as OccupiedAgentIds will be modified during removal.
	const TArray<int32> RemoveAgentIds = OccupiedAgentIds;
	RemoveAgents(RemoveAgentIds);
}

void ALearningAgentsManager::ResetAgent(const int32 AgentId)
{
	if (AgentId == INDEX_NONE)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Attempting to reset an agent with id of -1."), *GetName());
		return;
	}

	if (!HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Trying to reset an agent with id of %i but it was not found."), *GetName(), AgentId);
		return;
	}

	OnEventAgentIds.SetNumUninitialized(0, false);
	OnEventAgentIds.Add(AgentId);

	// Call OnAgentsReset Event

	UE_LOG(LogLearning, Display, TEXT("%s: Resetting Agent %i."), *GetName(), AgentId);

	TInlineComponentArray<ULearningAgentsManagerComponent*> ManagerComponents;
	GetComponents<ULearningAgentsManagerComponent>(ManagerComponents);

	for (ULearningAgentsManagerComponent* ManagerComponent : ManagerComponents)
	{
		ManagerComponent->OnAgentsReset(OnEventAgentIds);
	}
}

void ALearningAgentsManager::ResetAgents(const TArray<int32>& AgentIds)
{
	OnEventAgentIds.SetNumUninitialized(0, false);

	for (int32 AgentIdx = 0; AgentIdx < AgentIds.Num(); AgentIdx++)
	{
		if (AgentIds[AgentIdx] == INDEX_NONE)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Attempting to reset an agent with id of -1."), *GetName());
			continue;
		}

		if (!HasAgent(AgentIds[AgentIdx]))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Trying to reset an agent with id of %i but it was not found."), *GetName(), AgentIds[AgentIdx]);
			continue;
		}

		OnEventAgentIds.Add(AgentIds[AgentIdx]);
	}

	// Call OnAgentsReset Event

	UE_LOG(LogLearning, Display, TEXT("%s: Resetting Agents %s."), *GetName(), *UE::Learning::Array::FormatInt32(OnEventAgentIds));

	TInlineComponentArray<ULearningAgentsManagerComponent*> ManagerComponents;
	GetComponents<ULearningAgentsManagerComponent>(ManagerComponents);

	for (ULearningAgentsManagerComponent* ManagerComponent : ManagerComponents)
	{
		ManagerComponent->OnAgentsReset(OnEventAgentIds);
	}
}

void ALearningAgentsManager::ResetAllAgents()
{
	ResetAgents(OccupiedAgentIds);
}

UObject* ALearningAgentsManager::GetAgent(const int32 AgentId, const TSubclassOf<UObject> AgentClass) const
{
	if (!HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found. Be sure to only use AgentIds returned by AddAgent and check that the agent has not be removed."), *GetName(), AgentId);
		return nullptr;
	}
	else
	{
		return Agents[AgentId];
	}
}

void ALearningAgentsManager::GetAgents(TArray<UObject*>& OutAgents, const TArray<int32>& AgentIds, const TSubclassOf<UObject> AgentClass) const
{
	OutAgents.SetNumUninitialized(AgentIds.Num());

	for (int32 AgentIdIdx = 0; AgentIdIdx < AgentIds.Num(); AgentIdIdx++)
	{
		if (!HasAgent(AgentIds[AgentIdIdx]))
		{
			UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found. Be sure to only use AgentIds returned by AddAgent and check that the agent has not be removed."), *GetName(), AgentIds[AgentIdIdx]);
			OutAgents[AgentIdIdx] = nullptr;
		}
		else
		{
			OutAgents[AgentIdIdx] = Agents[AgentIds[AgentIdIdx]];
		}
	}
}

void ALearningAgentsManager::GetAllAgents(TArray<UObject*>& OutAgents, TArray<int32>& OutAgentIds, const TSubclassOf<UObject> AgentClass) const
{
	OutAgentIds = OccupiedAgentIds;
	GetAgents(OutAgents, OutAgentIds, AgentClass);
}

int32 ALearningAgentsManager::GetAgentId(UObject* Agent) const
{
	const int32 AgentId = Agents.Find(Agent);

	if (AgentId == INDEX_NONE)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Cannot get id for Agent %s, as it was not found in added agent set."), *GetName(), *Agent->GetName());
		return INDEX_NONE;
	}
	else
	{
		return AgentId;
	}
}

void ALearningAgentsManager::GetAgentIds(TArray<int32>& OutAgentIds, const TArray<UObject*>& InAgents) const
{
	OutAgentIds.SetNumUninitialized(InAgents.Num());

	for (int32 AgentIdx = 0; AgentIdx < InAgents.Num(); AgentIdx++)
	{
		const int32 AgentId = Agents.Find(InAgents[AgentIdx]);

		if (AgentId == INDEX_NONE)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Cannot get id for Agent %s, as it was not found in added agent set."), *GetName(), *InAgents[AgentIdx]->GetName());
			OutAgentIds[AgentIdx] = INDEX_NONE;
		}
		else
		{
			OutAgentIds[AgentIdx] = AgentId;
		}
	}
}

int32 ALearningAgentsManager::GetAgentNum() const
{
	return OccupiedAgentIds.Num();
}

bool ALearningAgentsManager::HasAgentObject(UObject* Agent) const
{
	return Agents.Contains(Agent);
}

bool ALearningAgentsManager::HasAgent(const int32 AgentId) const
{
	return OccupiedAgentSet.Contains(AgentId);
}

void ALearningAgentsManager::AddManagerAsTickPrerequisiteOfAgents(const TArray<AActor*>& InAgents)
{
	for (AActor* Agent : InAgents)
	{
		Agent->AddTickPrerequisiteActor(this);
	}
}

void ALearningAgentsManager::AddAgentsAsTickPrerequisiteOfManager(const TArray<AActor*>& InAgents)
{
	for (AActor* Agent : InAgents)
	{
		this->AddTickPrerequisiteActor(Agent);
	}
}

const UObject* ALearningAgentsManager::GetAgent(const int32 AgentId) const
{
	UE_LEARNING_CHECK(HasAgent(AgentId));
	return Agents[AgentId];
}

UObject* ALearningAgentsManager::GetAgent(const int32 AgentId)
{
	UE_LEARNING_CHECK(HasAgent(AgentId));
	return Agents[AgentId];
}

const TArray<int32>& ALearningAgentsManager::GetAllAgentIds() const
{
	return OccupiedAgentIds;
}

UE::Learning::FIndexSet ALearningAgentsManager::GetAllAgentSet() const
{
	return OccupiedAgentSet;
}

const TSharedPtr<UE::Learning::FArrayMap>& ALearningAgentsManager::GetInstanceData() const
{
	return InstanceData;
}

TConstArrayView<TObjectPtr<UObject>> ALearningAgentsManager::GetAgents() const
{
	return Agents;
}

void ALearningAgentsManager::UpdateAgentSets()
{
	// Keep OccupiedAgentIds sorted in ascending order for better memory access patterns.
	OccupiedAgentIds.Sort();
	OccupiedAgentSet = OccupiedAgentIds;
	OccupiedAgentSet.TryMakeSlice();

	// Keep VacantAgentIds sorted in descending order so that lowest ids are popped first.
	VacantAgentIds.Sort([](const int32& Lhs, const int32& Rhs) { return Lhs > Rhs; });
	VacantAgentSet = VacantAgentIds;
	VacantAgentSet.TryMakeSlice();
}