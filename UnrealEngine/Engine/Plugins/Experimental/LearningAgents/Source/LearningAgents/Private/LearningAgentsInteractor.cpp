// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsInteractor.h"

#include "LearningAgentsManager.h"
#include "LearningAgentsObservations.h"
#include "LearningAgentsActions.h"
#include "LearningAgentsHelpers.h"

#include "LearningArray.h"
#include "LearningArrayMap.h"
#include "LearningFeatureObject.h"
#include "LearningLog.h"
#include "EngineDefines.h"

ULearningAgentsInteractor::ULearningAgentsInteractor() : Super(FObjectInitializer::Get()) {}
ULearningAgentsInteractor::ULearningAgentsInteractor(FVTableHelper& Helper) : Super(Helper) {}
ULearningAgentsInteractor::~ULearningAgentsInteractor() = default;

void ULearningAgentsInteractor::SetupInteractor()
{
	if (IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup already run!"), *GetName());
		return;
	}

	if (!Manager)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Must be attached to a LearningAgentsManager Actor."), *GetName());
		return;
	}

	// Setup Observations
	ObservationObjects.Empty();
	ObservationFeatures.Empty();
	SetupObservations();

	if (ObservationObjects.Num() == 0)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: No observations added to Interactor during SetupObservations."), *GetName());
		return;
	}

	Observations = MakeShared<UE::Learning::FConcatenateFeature>(*(GetName() + TEXT("Observations")),
		TLearningArrayView<1, const TSharedRef<UE::Learning::FFeatureObject>>(ObservationFeatures),
		Manager->GetInstanceData().ToSharedRef(),
		Manager->GetMaxAgentNum());

	if (Observations->DimNum() == 0)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation vector is zero-sized - all added observations have no size."), *GetName());
		return;
	}

	// Setup Actions
	ActionObjects.Empty();
	ActionFeatures.Empty();
	SetupActions();

	if (ActionObjects.Num() == 0)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: No actions added to Interactor during SetupActions."), *GetName());
		return;
	}

	Actions = MakeShared<UE::Learning::FConcatenateFeature>(*(GetName() + TEXT("Actions")),
		TLearningArrayView<1, const TSharedRef<UE::Learning::FFeatureObject>>(ActionFeatures),
		Manager->GetInstanceData().ToSharedRef(),
		Manager->GetMaxAgentNum());

	if (Actions->DimNum() == 0)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Action vector is zero-sized -  all added actions have no size."), *GetName());
		return;
	}

	// Reset Agent iteration

	ObservationEncodingAgentIteration.SetNumUninitialized({ Manager->GetMaxAgentNum() });
	ActionEncodingAgentIteration.SetNumUninitialized({ Manager->GetMaxAgentNum() });
	UE::Learning::Array::Set<1, uint64>(ObservationEncodingAgentIteration, INDEX_NONE);
	UE::Learning::Array::Set<1, uint64>(ActionEncodingAgentIteration, INDEX_NONE);

	bIsSetup = true;

	OnAgentsAdded(Manager->GetAllAgentIds());
}

void ULearningAgentsInteractor::OnAgentsAdded(const TArray<int32>& AgentIds)
{
	if (IsSetup())
	{
		UE::Learning::Array::Set<1, uint64>(ObservationEncodingAgentIteration, 0, AgentIds);
		UE::Learning::Array::Set<1, uint64>(ActionEncodingAgentIteration, 0, AgentIds);

		for (ULearningAgentsHelper* Helper : HelperObjects)
		{
			Helper->OnAgentsAdded(AgentIds);
		}

		for (ULearningAgentsObservation* ObservationObject : ObservationObjects)
		{
			ObservationObject->OnAgentsAdded(AgentIds);
		}

		for (ULearningAgentsAction* ActionObject : ActionObjects)
		{
			ActionObject->OnAgentsAdded(AgentIds);
		}

		AgentsAdded(AgentIds);
	}
}

void ULearningAgentsInteractor::OnAgentsRemoved(const TArray<int32>& AgentIds)
{
	if (IsSetup())
	{
		UE::Learning::Array::Set<1, uint64>(ObservationEncodingAgentIteration, INDEX_NONE, AgentIds);
		UE::Learning::Array::Set<1, uint64>(ActionEncodingAgentIteration, INDEX_NONE, AgentIds);

		for (ULearningAgentsHelper* Helper : HelperObjects)
		{
			Helper->OnAgentsRemoved(AgentIds);
		}

		for (ULearningAgentsObservation* ObservationObject : ObservationObjects)
		{
			ObservationObject->OnAgentsRemoved(AgentIds);
		}

		for (ULearningAgentsAction* ActionObject : ActionObjects)
		{
			ActionObject->OnAgentsRemoved(AgentIds);
		}

		AgentsRemoved(AgentIds);
	}
}

void ULearningAgentsInteractor::OnAgentsReset(const TArray<int32>& AgentIds)
{
	if (IsSetup())
	{
		UE::Learning::Array::Set<1, uint64>(ObservationEncodingAgentIteration, 0, AgentIds);
		UE::Learning::Array::Set<1, uint64>(ActionEncodingAgentIteration, 0, AgentIds);
		
		for (ULearningAgentsHelper* Helper : HelperObjects)
		{
			Helper->OnAgentsReset(AgentIds);
		}

		for (ULearningAgentsObservation* ObservationObject : ObservationObjects)
		{
			ObservationObject->OnAgentsReset(AgentIds);
		}

		for (ULearningAgentsAction* ActionObject : ActionObjects)
		{
			ActionObject->OnAgentsReset(AgentIds);
		}

		AgentsReset(AgentIds);
	}
}

UE::Learning::FFeatureObject& ULearningAgentsInteractor::GetObservationFeature() const
{
	return *Observations;
}

UE::Learning::FFeatureObject& ULearningAgentsInteractor::GetActionFeature() const
{
	return *Actions;
}

TConstArrayView<ULearningAgentsObservation*> ULearningAgentsInteractor::GetObservationObjects() const
{
	return ObservationObjects;
}

TConstArrayView<ULearningAgentsAction*> ULearningAgentsInteractor::GetActionObjects() const
{
	return ActionObjects;
}

TLearningArrayView<1, uint64> ULearningAgentsInteractor::GetObservationEncodingAgentIteration()
{
	return ObservationEncodingAgentIteration;
}

TLearningArrayView<1, uint64> ULearningAgentsInteractor::GetActionEncodingAgentIteration()
{
	return ActionEncodingAgentIteration;
}

void ULearningAgentsInteractor::SetupObservations_Implementation()
{
	// Can be overridden to setup observations without blueprints
}

void ULearningAgentsInteractor::SetObservations_Implementation(const TArray<int32>& AgentIds)
{
	// Can be overridden to set observations without blueprints
}

void ULearningAgentsInteractor::AddObservation(TObjectPtr<ULearningAgentsObservation> Object, const TSharedRef<UE::Learning::FFeatureObject>& Feature)
{
	UE_LEARNING_CHECK(!IsSetup());
	UE_LEARNING_CHECK(Object);
	ObservationObjects.Add(Object);
	ObservationFeatures.Add(Feature);
}

void ULearningAgentsInteractor::SetupActions_Implementation()
{
	// Can be overridden to setup actions without blueprints
}

void ULearningAgentsInteractor::GetActions_Implementation(const TArray<int32>& AgentIds)
{
	// Can be overridden to get actions without blueprints
}

void ULearningAgentsInteractor::AddAction(TObjectPtr<ULearningAgentsAction> Object, const TSharedRef<UE::Learning::FFeatureObject>& Feature)
{
	UE_LEARNING_CHECK(!IsSetup());
	UE_LEARNING_CHECK(Object);
	ActionObjects.Add(Object);
	ActionFeatures.Add(Feature);
}

void ULearningAgentsInteractor::EncodeObservations(const UE::Learning::FIndexSet AgentSet)
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsInteractor::EncodeObservations);

	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	// Run Set Observations Callback

	ValidAgentIds.Empty(AgentSet.Num());
	for (const int32 AgentId : AgentSet)
	{
		ValidAgentIds.Add(AgentId);
	}

	SetObservations(ValidAgentIds);

	// Check that all observations have had their setter run

	ValidAgentStatus.SetNumUninitialized(Manager->GetMaxAgentNum());
	ValidAgentStatus.SetRange(0, Manager->GetMaxAgentNum(), true);

	for (ULearningAgentsObservation* ObservationObject : ObservationObjects)
	{
		for (const int32 AgentId : AgentSet)
		{
			if (ObservationObject->GetAgentIteration(AgentId) <= ObservationEncodingAgentIteration[AgentId])
			{
				UE_LOG(LogLearning, Warning, TEXT("%s: Observation %s for agent with id %i has not been set (got iteration %i, expected iteration %i) and so agent will not have observations encoded."), *GetName(), *ObservationObject->GetName(), AgentId, ObservationObject->GetAgentIteration(AgentId), ObservationEncodingAgentIteration[AgentId] + 1);
				ValidAgentStatus[AgentId] = false;
				continue;
			}

			if (ObservationObject->GetAgentIteration(AgentId) > ObservationEncodingAgentIteration[AgentId] + 1)
			{
				UE_LOG(LogLearning, Warning, TEXT("%s: Observation %s for agent with id %i appears to have been set multiple times (got iteration %i, expected iteration %i) and so agent will not have observations encoded."), *GetName(), *ObservationObject->GetName(), AgentId, ObservationObject->GetAgentIteration(AgentId), ObservationEncodingAgentIteration[AgentId] + 1);
				ValidAgentStatus[AgentId] = false;
				continue;
			}

			if (ObservationObject->GetAgentIteration(AgentId) != ObservationEncodingAgentIteration[AgentId] + 1)
			{
				UE_LOG(LogLearning, Warning, TEXT("%s: Observation %s for agent with id %i does not have a matching iteration number (got iteration %i, expected iteration %i) and so agent will not have observations encoded."), *GetName(), *ObservationObject->GetName(), AgentId, ObservationObject->GetAgentIteration(AgentId), ObservationEncodingAgentIteration[AgentId] + 1);
				ValidAgentStatus[AgentId] = false;
				continue;
			}
		}
	}

	ValidAgentIds.Empty(Manager->GetAgentNum());

	for (const int32 AgentId : AgentSet)
	{
		if (ValidAgentStatus[AgentId]) { ValidAgentIds.Add(AgentId); }
	}

	ValidAgentSet = ValidAgentIds;
	ValidAgentSet.TryMakeSlice();

	// Encode Observations

	Observations->Encode(ValidAgentSet);

	// Increment Observation Encoding Iteration

	for (const int32 AgentId : ValidAgentSet)
	{
		ObservationEncodingAgentIteration[AgentId]++;
	}

	// Visual Logger

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	for (const ULearningAgentsObservation* ObservationObject : ObservationObjects)
	{
		ObservationObject->VisualLog(ValidAgentSet);
	}
#endif
}

void ULearningAgentsInteractor::DecodeActions(const UE::Learning::FIndexSet AgentSet)
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsInteractor::DecodeActions);

	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	// Check Agents actually have encoded actions.

	ValidAgentIds.Empty(Manager->GetAgentNum());

	for (const int32 AgentId : AgentSet)
	{
		if (ActionEncodingAgentIteration[AgentId] == 0)
		{
			UE_LOG(LogLearning, Warning, TEXT("%s: Agent with id %i does not have an encoded action vector so actions will not be decoded for it. Was EvaluatePolicy or EncodeActions run?"), *GetName(), AgentId);
			continue;
		}

		ValidAgentIds.Add(AgentId);
	}

	ValidAgentSet = ValidAgentIds;
	ValidAgentSet.TryMakeSlice();

	// Decode Actions

	Actions->Decode(ValidAgentSet);

	// Run Get Actions Callback

	GetActions(ValidAgentIds);

	// Check that all actions have had their getter run

	for (ULearningAgentsAction* ActionObject : ActionObjects)
	{
		for (const int32 AgentId : ValidAgentSet)
		{
			if (ActionObject->GetAgentGetIteration(AgentId) == ActionEncodingAgentIteration[AgentId] - 1)
			{
				UE_LOG(LogLearning, Warning, TEXT("%s: Action %s for agent with id %i appears to have not been got."), *GetName(), *ActionObject->GetName(), AgentId);
				continue;
			}
				
			if (ActionObject->GetAgentGetIteration(AgentId) > ActionEncodingAgentIteration[AgentId])
			{
				UE_LOG(LogLearning, Warning, TEXT("%s: Action %s for agent with id %i appears to have been got multiple times."), *GetName(), *ActionObject->GetName(), AgentId);
				continue;
			}
				
			if (ActionObject->GetAgentGetIteration(AgentId) != ActionEncodingAgentIteration[AgentId])
			{
				UE_LOG(LogLearning, Warning, TEXT("%s: Action %s for agent with id %i does not have a matching iteration number (got %i, expected %i)."), *GetName(), *ActionObject->GetName(), AgentId, ActionObject->GetAgentGetIteration(AgentId), ActionEncodingAgentIteration[AgentId]);
				continue;
			}
		}
	}

	// Visual Logger

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	for (const ULearningAgentsAction* ActionObject : ActionObjects)
	{
		ActionObject->VisualLog(ValidAgentSet);
	}
#endif
}

void ULearningAgentsInteractor::EncodeObservations()
{
	EncodeObservations(Manager->GetAllAgentSet());
}

void ULearningAgentsInteractor::DecodeActions()
{
	DecodeActions(Manager->GetAllAgentSet());
}
void ULearningAgentsInteractor::GetObservationVector(const int32 AgentId, TArray<float>& OutObservationVector) const
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		OutObservationVector.Empty();
		return;
	}

	if (!HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		OutObservationVector.Empty();
		return;
	}

	if (ObservationEncodingAgentIteration[AgentId] == 0)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Agent with id %d has not yet computed an observation vector. Have you run EncodeObservations?"), *GetName(), AgentId);
		OutObservationVector.Empty();
		return;
	}

	OutObservationVector.SetNumUninitialized(Observations->DimNum());
	UE::Learning::Array::Copy<1, float>(OutObservationVector, Observations->FeatureBuffer()[AgentId]);
}

void ULearningAgentsInteractor::GetActionVector(const int32 AgentId, TArray<float>& OutActionVector) const
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		OutActionVector.Empty();
		return;
	}

	if (!HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		OutActionVector.Empty();
		return;
	}

	if (ActionEncodingAgentIteration[AgentId] == 0)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Agent with id %d has not yet computed an action vector. Have you run EvaluatePolicy or EncodeActions?"), *GetName(), AgentId);
		OutActionVector.Empty();
		return;
	}

	OutActionVector.SetNumUninitialized(Actions->DimNum());
	UE::Learning::Array::Copy<1, float>(OutActionVector, Actions->FeatureBuffer()[AgentId]);
}