// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsController.h"

#include "LearningAgentsManager.h"
#include "LearningAgentsInteractor.h"
#include "LearningAgentsActions.h"
#include "LearningLog.h"

ULearningAgentsController::ULearningAgentsController() : Super(FObjectInitializer::Get()) {}
ULearningAgentsController::ULearningAgentsController(FVTableHelper& Helper) : Super(Helper) {}
ULearningAgentsController::~ULearningAgentsController() = default;

void ULearningAgentsController::EvaluateAgentController_Implementation(
	FLearningAgentsActionObjectElement& OutActionObjectElement,
	ULearningAgentsActionObject* InActionObject,
	const ULearningAgentsObservationObject* InObservationObject,
	const FLearningAgentsObservationObjectElement& InObservationObjectElement,
	const int32 AgentIds)
{
	UE_LOG(LogLearning, Error, TEXT("%s: EvaluateAgentController function must be overridden!"), *GetName());
	OutActionObjectElement = FLearningAgentsActionObjectElement();
}

void ULearningAgentsController::EvaluateAgentControllers_Implementation(
	TArray<FLearningAgentsActionObjectElement>& OutActionObjectElements,
	ULearningAgentsActionObject* InActionObject,
	const ULearningAgentsObservationObject* InObservationObject,
	const TArray<FLearningAgentsObservationObjectElement>& InObservationObjectElements,
	const TArray<int32>& AgentIds)
{
	const int32 AgentNum = AgentIds.Num();

	if (AgentNum != InObservationObjectElements.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Not enough Observation Objects. Expected %i, Got %i."), *GetName(), AgentNum, InObservationObjectElements.Num());
		return;
	}

	OutActionObjectElements.Empty(AgentNum);
	for (int32 AgentIdx = 0; AgentIdx < AgentNum; AgentIdx++)
	{
		FLearningAgentsActionObjectElement OutActionObjectElement;
		EvaluateAgentController(OutActionObjectElement, InActionObject, InObservationObject, InObservationObjectElements[AgentIdx], AgentIds[AgentIdx]);
		OutActionObjectElements.Add(OutActionObjectElement);
	}
}

ULearningAgentsController* ULearningAgentsController::MakeController(
	ULearningAgentsManager* InManager, 
	ULearningAgentsInteractor* InInteractor, 
	TSubclassOf<ULearningAgentsController> Class,
	const FName Name)
{
	if (!InManager)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeController: InManager is nullptr."));
		return nullptr;
	}

	if (!Class)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeController: Class is nullptr."));
		return nullptr;
	}

	const FName UniqueName = MakeUniqueObjectName(InManager, Class, Name, EUniqueObjectNameOptions::GloballyUnique);

	ULearningAgentsController* Controller = NewObject<ULearningAgentsController>(InManager, Class, UniqueName);
	if (!Controller) { return nullptr; }
	
	Controller->SetupController(InManager, InInteractor);

	return Controller->IsSetup() ? Controller : nullptr;
}

void ULearningAgentsController::SetupController(ULearningAgentsManager* InManager, ULearningAgentsInteractor* InInteractor)
{
	if (IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup already run!"), *GetName());
		return;
	}

	if (!InManager)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: InManager is nullptr."), *GetName());
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

	Manager = InManager;
	Interactor = InInteractor;
	bIsSetup = true;

	InManager->AddListener(this);
}

void ULearningAgentsController::EvaluateController()
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsController::EvaluateController);

	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	if (Manager->GetAgentNum() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: No agents added to Manager."), *GetName());
	}

	if (Manager->GetAgentNum() != Interactor->ObservationObjectElements.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Not enough Observation Objects added by GatherAgentObservations. Expected %i, Got %i."), *GetName(), Manager->GetAgentNum(), Interactor->ObservationObjectElements.Num());
		return;
	}

	// Run EvaluateAgentControllers Callback

	Interactor->ActionObject->ActionObject.Reset();
	Interactor->ActionObjectElements.Empty(Manager->GetMaxAgentNum());
	EvaluateAgentControllers(
		Interactor->ActionObjectElements, 
		Interactor->ActionObject, 
		Interactor->ObservationObject,
		Interactor->ObservationObjectElements,
		Manager->GetAllAgentIds());

	if (Manager->GetAgentNum() != Interactor->ActionObjectElements.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Not enough Action Objects added by EvaluateAgentControllers. Expected %i, Got %i."), *GetName(), Manager->GetAgentNum(), Interactor->ActionObjectElements.Num());
		return;
	}

	// Check Action Objects are Valid and if so convert to action vectors

	for (int32 AgentIdx = 0; AgentIdx < Manager->GetAgentNum(); AgentIdx++)
	{
		if (ULearningAgentsActions::ValidateActionObjectMatchesSchema(
			Interactor->ActionSchema,
			Interactor->ActionSchemaElement, 
			Interactor->ActionObject, 
			Interactor->ActionObjectElements[AgentIdx]))
		{
			UE::Learning::Action::SetVectorFromObject(
				Interactor->ActionVectors[Manager->GetAllAgentSet()[AgentIdx]],
				Interactor->ActionSchema->ActionSchema,
				Interactor->ActionSchemaElement.SchemaElement,
				Interactor->ActionObject->ActionObject,
				Interactor->ActionObjectElements[AgentIdx].ObjectElement);

			Interactor->ActionVectorIteration[Manager->GetAllAgentSet()[AgentIdx]]++;
		}
	}
}

ULearningAgentsInteractor* ULearningAgentsController::GetInteractor(const TSubclassOf<ULearningAgentsInteractor> InteractorClass) const
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
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

	Interactor->GatherObservations();
	EvaluateController();
	Interactor->PerformActions();
}
