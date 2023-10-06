// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsController.h"

#include "LearningAgentsManager.h"
#include "LearningAgentsInteractor.h"
#include "LearningAgentsActions.h"
#include "LearningFeatureObject.h"
#include "LearningLog.h"

ULearningAgentsController::ULearningAgentsController() : Super(FObjectInitializer::Get()) {}
ULearningAgentsController::ULearningAgentsController(FVTableHelper& Helper) : Super(Helper) {}
ULearningAgentsController::~ULearningAgentsController() = default;

void ULearningAgentsController::SetActions_Implementation(const TArray<int32>& AgentIds)
{
	// Can be overridden to get actions without blueprints
}

void ULearningAgentsController::SetupController(ULearningAgentsInteractor* InInteractor)
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

	if (!InInteractor)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: InInteractor is nullptr."), *GetName());
		return;
	}

	if (!InInteractor->IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: %s's Setup must be run before it can be used."), *GetName(), *InInteractor->GetName());
		return;
	}

	Interactor = InInteractor;

	bIsSetup = true;

	OnAgentsAdded(Manager->GetAllAgentIds());
}

void ULearningAgentsController::EncodeActions()
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsController::EncodeActions);

	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	const TLearningArrayView<1, uint64> ActionEncodingAgentIteration = Interactor->GetActionEncodingAgentIteration();

	// Run Set Actions Callback

	SetActions(Manager->GetAllAgentIds());

	// Check that all actions have had their setter run

	ValidAgentStatus.SetNumUninitialized(Manager->GetMaxAgentNum());
	ValidAgentStatus.SetRange(0, Manager->GetMaxAgentNum(), true);

	for (ULearningAgentsAction* ActionObject : Interactor->GetActionObjects())
	{
		for (const int32 AgentId : Manager->GetAllAgentSet())
		{
			if (ActionObject->GetAgentSetIteration(AgentId) == ActionEncodingAgentIteration[AgentId])
			{
				UE_LOG(LogLearning, Warning, TEXT("%s: Action %s for agent with id %i has not been set (got iteration %i, expected iteration %i) and so agent will not have actions encoded."), *GetName(), *ActionObject->GetName(), AgentId, ActionObject->GetAgentSetIteration(AgentId), ActionEncodingAgentIteration[AgentId] + 1);
				ValidAgentStatus[AgentId] = false;
				continue;
			}

			if (ActionObject->GetAgentSetIteration(AgentId) > ActionEncodingAgentIteration[AgentId] + 1)
			{
				UE_LOG(LogLearning, Warning, TEXT("%s: Action %s for agent with id %i appears to have been set multiple times (got iteration %i, expected iteration %i) and so agent will not have actions encoded."), *GetName(), *ActionObject->GetName(), AgentId, ActionObject->GetAgentSetIteration(AgentId), ActionEncodingAgentIteration[AgentId] + 1);
				ValidAgentStatus[AgentId] = false;
				continue;
			}

			if (ActionObject->GetAgentSetIteration(AgentId) != ActionEncodingAgentIteration[AgentId] + 1)
			{
				UE_LOG(LogLearning, Warning, TEXT("%s: Action %s for agent with id %i does not have a matching iteration number (got iteration %i, expected iteration %i) and so agent will not have actions encoded."), *GetName(), *ActionObject->GetName(), AgentId, ActionObject->GetAgentSetIteration(AgentId), ActionEncodingAgentIteration[AgentId] + 1);
				ValidAgentStatus[AgentId] = false;
				continue;
			}
		}
	}

	ValidAgentIds.Empty(Manager->GetAgentNum());

	for (const int32 AgentId : Manager->GetAllAgentSet())
	{
		if (ValidAgentStatus[AgentId]) { ValidAgentIds.Add(AgentId); }
	}

	ValidAgentSet = ValidAgentIds;
	ValidAgentSet.TryMakeSlice();

	// Encode Actions

	Interactor->GetActionFeature().Encode(ValidAgentSet);

	// Increment Action Encoding Iteration

	for (const int32 AgentId : ValidAgentSet)
	{
		ActionEncodingAgentIteration[AgentId]++;
	}

	// Visual Logger

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	for (const ULearningAgentsAction* ActionObject : Interactor->GetActionObjects())
	{
		ActionObject->VisualLog(ValidAgentSet);
	}
#endif
}

ULearningAgentsInteractor* ULearningAgentsController::GetInteractor(const TSubclassOf<ULearningAgentsInteractor> InteractorClass) const
{
	if (!Interactor)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Interactor is nullptr. Did we forget to call Setup on this component?"), *GetName());
		return nullptr;
	}

	return Interactor;
}

void ULearningAgentsController::RunController()
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsController::RunController);

	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	Interactor->EncodeObservations();
	EncodeActions();
	Interactor->DecodeActions();
}
