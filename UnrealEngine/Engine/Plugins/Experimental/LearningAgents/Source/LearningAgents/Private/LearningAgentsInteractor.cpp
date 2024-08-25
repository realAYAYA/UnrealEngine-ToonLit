// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsInteractor.h"

#include "LearningAgentsManager.h"
#include "LearningAgentsObservations.h"
#include "LearningAgentsActions.h"
#include "LearningAgentsNeuralNetwork.h"

#include "LearningNeuralNetwork.h"
#include "LearningArray.h"
#include "LearningLog.h"

#include "EngineDefines.h"

ULearningAgentsInteractor::ULearningAgentsInteractor() : Super(FObjectInitializer::Get()) {}
ULearningAgentsInteractor::ULearningAgentsInteractor(FVTableHelper& Helper) : Super(Helper) {}
ULearningAgentsInteractor::~ULearningAgentsInteractor() = default;

ULearningAgentsInteractor* ULearningAgentsInteractor::MakeInteractor(
	ULearningAgentsManager* InManager,
	TSubclassOf<ULearningAgentsInteractor> Class,
	const FName Name)
{
	if (!InManager)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeInteractor: InManager is nullptr."));
		return nullptr;
	}

	if (!Class)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeInteractor: Class is nullptr."));
		return nullptr;
	}

	const FName UniqueName = MakeUniqueObjectName(InManager, Class, Name, EUniqueObjectNameOptions::GloballyUnique);

	ULearningAgentsInteractor* Interactor = NewObject<ULearningAgentsInteractor>(InManager, Class, UniqueName);
	if (!Interactor) { return nullptr; }

	Interactor->SetupInteractor(InManager);

	return Interactor->IsSetup() ? Interactor : nullptr;
}

void ULearningAgentsInteractor::SetupInteractor(ULearningAgentsManager* InManager)
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

	Manager = InManager;

	// Specify Observations

	const FName ObservationSchemaUniqueName = MakeUniqueObjectName(this, ULearningAgentsObservationSchema::StaticClass(), TEXT("ObservationSchema"), EUniqueObjectNameOptions::GloballyUnique);

	ObservationSchema = NewObject<ULearningAgentsObservationSchema>(this, ObservationSchemaUniqueName);
	SpecifyAgentObservation(ObservationSchemaElement, ObservationSchema);

	if (!ObservationSchema->ObservationSchema.IsValid(ObservationSchemaElement.SchemaElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid observation provided to Interactor during SpecifyObservations."), *GetName());
		return;
	}

	const int32 ObservationVectorSize = ObservationSchema->ObservationSchema.GetObservationVectorSize(ObservationSchemaElement.SchemaElement);

	if (ObservationVectorSize == 0)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation vector is zero-sized - specified observations have no size."), *GetName());
		return;
	}

	const int32 ObservationEncodedVectorSize = ObservationSchema->ObservationSchema.GetEncodedVectorSize(ObservationSchemaElement.SchemaElement);

	if (ObservationEncodedVectorSize == 0)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation encoded vector is zero-sized - observations map to empty encoding."), *GetName());
		return;
	}

	ObservationVectors.SetNumUninitialized({ Manager->GetMaxAgentNum(), ObservationVectorSize  });
	ObservationCompatibilityHash = UE::Learning::Observation::GetSchemaObjectsCompatibilityHash(ObservationSchema->ObservationSchema, ObservationSchemaElement.SchemaElement);

	const FName ObservationObjectUniqueName = MakeUniqueObjectName(this, ULearningAgentsObservationObject::StaticClass(), TEXT("ObservationObject"), EUniqueObjectNameOptions::GloballyUnique);

	ObservationObject = NewObject<ULearningAgentsObservationObject>(this, ObservationObjectUniqueName);
	ObservationObjectElements.Empty(Manager->GetMaxAgentNum());

	// Specify Actions

	const FName ActionSchemaUniqueName = MakeUniqueObjectName(this, ULearningAgentsActionSchema::StaticClass(), TEXT("ActionSchema"), EUniqueObjectNameOptions::GloballyUnique);

	ActionSchema = NewObject<ULearningAgentsActionSchema>(this, ActionSchemaUniqueName);
	SpecifyAgentAction(ActionSchemaElement, ActionSchema);

	if (!ActionSchema->ActionSchema.IsValid(ActionSchemaElement.SchemaElement))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Invalid action provided to Interactor during SpecifyActions."), *GetName());
		return;
	}

	const int32 ActionVectorSize = ActionSchema->ActionSchema.GetActionVectorSize(ActionSchemaElement.SchemaElement);

	if (ActionVectorSize == 0)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Action vector is zero-sized - specified actions have no size."), *GetName());
		return;
	}

	const int32 ActionEncodedVectorSize = ActionSchema->ActionSchema.GetEncodedVectorSize(ActionSchemaElement.SchemaElement);

	if (ActionEncodedVectorSize == 0)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Action encoded vector is zero-sized - actions map to empty encoding."), *GetName());
		return;
	}

	const int32 ActionDistributionVectorSize = ActionSchema->ActionSchema.GetActionDistributionVectorSize(ActionSchemaElement.SchemaElement);

	ActionVectors.SetNumUninitialized({ Manager->GetMaxAgentNum(), ActionVectorSize });
	ActionCompatibilityHash = UE::Learning::Action::GetSchemaObjectsCompatibilityHash(ActionSchema->ActionSchema, ActionSchemaElement.SchemaElement);

	const FName ActionObjectUniqueName = MakeUniqueObjectName(this, ULearningAgentsActionObject::StaticClass(), TEXT("ActionObject"), EUniqueObjectNameOptions::GloballyUnique);

	ActionObject = NewObject<ULearningAgentsActionObject>(this, ActionObjectUniqueName);
	ActionObjectElements.Empty(Manager->GetMaxAgentNum());

	// Reset Agent iteration

	ObservationVectorIteration.SetNumUninitialized({ Manager->GetMaxAgentNum() });
	ActionVectorIteration.SetNumUninitialized({ Manager->GetMaxAgentNum() });

	UE::Learning::Array::Set<1, uint64>(ObservationVectorIteration, INDEX_NONE);
	UE::Learning::Array::Set<1, uint64>(ActionVectorIteration, INDEX_NONE);

	bIsSetup = true;

	Manager->AddListener(this);
}

void ULearningAgentsInteractor::OnAgentsAdded_Implementation(const TArray<int32>& AgentIds)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	UE::Learning::Array::Set<1, uint64>(ObservationVectorIteration, 0, AgentIds);
	UE::Learning::Array::Set<1, uint64>(ActionVectorIteration, 0, AgentIds);
}

void ULearningAgentsInteractor::OnAgentsRemoved_Implementation(const TArray<int32>& AgentIds)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	UE::Learning::Array::Set<1, uint64>(ObservationVectorIteration, INDEX_NONE, AgentIds);
	UE::Learning::Array::Set<1, uint64>(ActionVectorIteration, INDEX_NONE, AgentIds);
}

void ULearningAgentsInteractor::OnAgentsReset_Implementation(const TArray<int32>& AgentIds)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	UE::Learning::Array::Set<1, uint64>(ObservationVectorIteration, 0, AgentIds);
	UE::Learning::Array::Set<1, uint64>(ActionVectorIteration, 0, AgentIds);
}

void ULearningAgentsInteractor::SpecifyAgentObservation_Implementation(FLearningAgentsObservationSchemaElement& OutObservationSchemaElement, ULearningAgentsObservationSchema* InObservationSchema)
{
	UE_LOG(LogLearning, Error, TEXT("%s: SpecifyAgentObservation function must be overridden!"), *GetName());
	OutObservationSchemaElement = FLearningAgentsObservationSchemaElement();
}

void ULearningAgentsInteractor::GatherAgentObservation_Implementation(FLearningAgentsObservationObjectElement& OutObservationObjectElement, ULearningAgentsObservationObject* InObservationObject, const int32 AgentId)
{
	UE_LOG(LogLearning, Error, TEXT("%s: GatherAgentObservation function must be overridden!"), *GetName());
	OutObservationObjectElement = FLearningAgentsObservationObjectElement();
}

void ULearningAgentsInteractor::GatherAgentObservations_Implementation(TArray<FLearningAgentsObservationObjectElement>& OutObservationObjectElements, ULearningAgentsObservationObject* InObservationObject, const TArray<int32>& AgentIds)
{
	OutObservationObjectElements.Empty(AgentIds.Num());
	for (const int32 AgentId : AgentIds)
	{
		FLearningAgentsObservationObjectElement OutElement;
		GatherAgentObservation(OutElement, InObservationObject, AgentId);
		OutObservationObjectElements.Add(OutElement);
	}
}

void ULearningAgentsInteractor::SpecifyAgentAction_Implementation(FLearningAgentsActionSchemaElement& OutActionSchemaElement, ULearningAgentsActionSchema* InActionSchema)
{
	UE_LOG(LogLearning, Error, TEXT("%s: SpecifyAgentAction function must be overridden!"), *GetName());
	OutActionSchemaElement = FLearningAgentsActionSchemaElement();
}

void ULearningAgentsInteractor::PerformAgentAction_Implementation(const ULearningAgentsActionObject* InActionObject, const FLearningAgentsActionObjectElement& InActionObjectElement, const int32 AgentId)
{
	UE_LOG(LogLearning, Error, TEXT("%s: PerformAgentAction function must be overridden!"), *GetName());
}

void ULearningAgentsInteractor::PerformAgentActions_Implementation(const ULearningAgentsActionObject* InActionObject, const TArray<FLearningAgentsActionObjectElement>& InActionObjectElements, const TArray<int32>& AgentIds)
{
	const int32 AgentNum = AgentIds.Num();

	if (AgentNum != InActionObjectElements.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Not enough Action Objects. Expected %i, Got %i."), *GetName(), AgentNum, InActionObjectElements.Num());
		return;
	}

	for (int32 AgentIdx = 0; AgentIdx < AgentNum; AgentIdx++)
	{
		PerformAgentAction(InActionObject, InActionObjectElements[AgentIdx], AgentIds[AgentIdx]);
	}
}

void ULearningAgentsInteractor::GatherObservations(const UE::Learning::FIndexSet AgentSet, bool bIncrementIteration)
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsInteractor::GatherObservations);

	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	// Run GatherAgentObservations Callback

	ValidAgentIds.Empty(Manager->GetMaxAgentNum());
	for (const int32 AgentId : AgentSet)
	{
		ValidAgentIds.Add(AgentId);
	}

	ObservationObject->ObservationObject.Reset();
	ObservationObjectElements.Empty(Manager->GetMaxAgentNum());
	GatherAgentObservations(ObservationObjectElements, ObservationObject, ValidAgentIds);

	if (ValidAgentIds.Num() != ObservationObjectElements.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Not enough Observation Objects added by GatherAgentObservations. Expected %i, Got %i."), *GetName(), ValidAgentIds.Num(), ObservationObjectElements.Num());
		return;
	}

	// Check Observation Objects are Valid and if so convert to observation vectors

	for (int32 AgentIdx = 0; AgentIdx < AgentSet.Num(); AgentIdx++)
	{
		if (ULearningAgentsObservations::ValidateObservationObjectMatchesSchema(
			ObservationSchema, 
			ObservationSchemaElement, 
			ObservationObject, 
			ObservationObjectElements[AgentIdx]))
		{
			UE::Learning::Observation::SetVectorFromObject(
				ObservationVectors[AgentSet[AgentIdx]],
				ObservationSchema->ObservationSchema,
				ObservationSchemaElement.SchemaElement,
				ObservationObject->ObservationObject,
				ObservationObjectElements[AgentIdx].ObjectElement);

			if (bIncrementIteration)
			{
				ObservationVectorIteration[AgentSet[AgentIdx]]++;
			}
		}
	}
}

void ULearningAgentsInteractor::PerformActions(const UE::Learning::FIndexSet AgentSet)
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsInteractor::PerformActions);

	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	// Check which agents have had their action vector set

	ValidAgentIds.Empty(Manager->GetMaxAgentNum());

	for (const int32 AgentId : AgentSet)
	{
		if (ActionVectorIteration[AgentId] == 0)
		{
			UE_LOG(LogLearning, Warning, TEXT("%s: Agent with id %i does not have an action vector so actions will not be scattered for it. Was DecodeAndSampleActions run without error?"), *GetName(), AgentId);
			continue;
		}

		ValidAgentIds.Add(AgentId);
	}

	ValidAgentSet = ValidAgentIds;
	ValidAgentSet.TryMakeSlice();

	// Generate Action Objects

	ActionObject->ActionObject.Reset();
	ActionObjectElements.Empty(Manager->GetMaxAgentNum());

	for (int32 AgentIdx = 0; AgentIdx < ValidAgentSet.Num(); AgentIdx++)
	{
		FLearningAgentsActionObjectElement ActionObjectElement;

		UE::Learning::Action::GetObjectFromVector(
			ActionObject->ActionObject,
			ActionObjectElement.ObjectElement,
			ActionSchema->ActionSchema,
			ActionSchemaElement.SchemaElement,
			ActionVectors[ValidAgentSet[AgentIdx]]);

		ActionObjectElements.Add(ActionObjectElement);
	}

	// Perform Action Objects

	PerformAgentActions(ActionObject, ActionObjectElements, ValidAgentIds);
}

void ULearningAgentsInteractor::GatherObservations()
{
	if (Manager->GetAgentNum() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: No agents added to Manager."), *GetName());
	}

	GatherObservations(Manager->GetAllAgentSet());
}

void ULearningAgentsInteractor::PerformActions()
{
	if (Manager->GetAgentNum() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: No agents added to Manager."), *GetName());
	}

	PerformActions(Manager->GetAllAgentSet());
}


void ULearningAgentsInteractor::GetObservationVector(TArray<float>& OutObservationVector, int32& OutObservationCompatibilityHash, const int32 AgentId)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		OutObservationVector.Empty();
		OutObservationCompatibilityHash = 0;
		return;
	}

	if (!HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		OutObservationVector.Empty();
		OutObservationCompatibilityHash = 0;
		return;
	}

	if (ObservationVectorIteration[AgentId] == 0)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation vector not set for agent %i."), *GetName(), AgentId);
		OutObservationVector.Empty();
		OutObservationCompatibilityHash = 0;
		return;
	}

	OutObservationVector.SetNumUninitialized(GetObservationVectorSize());
	UE::Learning::Array::Copy<1, float>(OutObservationVector, ObservationVectors[AgentId]);
	OutObservationCompatibilityHash = ObservationCompatibilityHash;
}

void ULearningAgentsInteractor::GetActionVector(TArray<float>& OutActionVector, int32& OutActionCompatibilityHash, const int32 AgentId)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		OutActionVector.Empty();
		OutActionCompatibilityHash = 0;
		return;
	}

	if (!HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		OutActionVector.Empty();
		OutActionCompatibilityHash = 0;
		return;
	}

	if (ActionVectorIteration[AgentId] == 0)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Action vector not set for agent %i."), *GetName(), AgentId);
		OutActionVector.Empty();
		OutActionCompatibilityHash = 0;
		return;
	}

	OutActionVector.SetNumUninitialized(GetActionVectorSize());
	UE::Learning::Array::Copy<1, float>(OutActionVector, ActionVectors[AgentId]);
	OutActionCompatibilityHash = ActionCompatibilityHash;
}

void ULearningAgentsInteractor::SetObservationVector(const TArray<float>& ObservationVector, const int32 InObservationCompatibilityHash, const int32 AgentId, bool bIncrementIteration)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	if (!HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	if (InObservationCompatibilityHash != ObservationCompatibilityHash)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation Compatibility hash incompatible. Got %i, expected %i."), *GetName(), InObservationCompatibilityHash, ObservationCompatibilityHash);
		return;
	}

	if (ObservationVector.Num() != GetObservationVectorSize())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Observation Vector size incompatible. Got %i, expected %i."), *GetName(), ObservationVector.Num(), GetObservationVectorSize());
		return;
	}

	UE::Learning::Array::Copy<1, float>(ObservationVectors[AgentId], ObservationVector);
	if (bIncrementIteration) { ObservationVectorIteration[AgentId]++; }
}

void ULearningAgentsInteractor::SetActionVector(const TArray<float>& ActionVector, const int32 InActionCompatibilityHash, const int32 AgentId, bool bIncrementIteration)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	if (!HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	if (InActionCompatibilityHash != ActionCompatibilityHash)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Action Compatibility hash incompatible. Got %i, expected %i."), *GetName(), InActionCompatibilityHash, ActionCompatibilityHash);
		return;
	}

	if (ActionVector.Num() != GetObservationVectorSize())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Action Vector size incompatible. Got %i, expected %i."), *GetName(), ActionVector.Num(), GetActionVectorSize());
		return;
	}

	UE::Learning::Array::Copy<1, float>(ActionVectors[AgentId], ActionVector);
	if (bIncrementIteration) { ActionVectorIteration[AgentId]++; }
}

bool ULearningAgentsInteractor::HasObservationVector(const int32 AgentId) const
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return false;
	}

	if (!HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return false;
	}

	return ObservationVectorIteration[AgentId] > 0;
}

bool ULearningAgentsInteractor::HasActionVector(const int32 AgentId) const
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return false;
	}

	if (!HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return false;
	}

	return ActionVectorIteration[AgentId] > 0;
}

int32 ULearningAgentsInteractor::GetObservationVectorSize() const
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return 0;
	}

	return ObservationSchema->ObservationSchema.GetObservationVectorSize(ObservationSchemaElement.SchemaElement);
}

int32 ULearningAgentsInteractor::GetObservationEncodedVectorSize() const
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return 0;
	}

	return ObservationSchema->ObservationSchema.GetEncodedVectorSize(ObservationSchemaElement.SchemaElement);
}

int32 ULearningAgentsInteractor::GetActionVectorSize() const
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return 0;
	}

	return ActionSchema->ActionSchema.GetActionVectorSize(ActionSchemaElement.SchemaElement);
}

int32 ULearningAgentsInteractor::GetActionDistributionVectorSize() const
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return 0;
	}

	return ActionSchema->ActionSchema.GetActionDistributionVectorSize(ActionSchemaElement.SchemaElement);
}

int32 ULearningAgentsInteractor::GetActionEncodedVectorSize() const
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return 0;
	}

	return ActionSchema->ActionSchema.GetEncodedVectorSize(ActionSchemaElement.SchemaElement);
}

const UE::Learning::Observation::FSchema& ULearningAgentsInteractor::GetObservationSchema() const
{
	return ObservationSchema->ObservationSchema;
}

UE::Learning::Observation::FSchemaElement ULearningAgentsInteractor::GetObservationSchemaElement() const
{
	return ObservationSchemaElement.SchemaElement;
}

const UE::Learning::Action::FSchema& ULearningAgentsInteractor::GetActionSchema() const
{
	return ActionSchema->ActionSchema;
}

UE::Learning::Action::FSchemaElement ULearningAgentsInteractor::GetActionSchemaElement() const
{
	return ActionSchemaElement.SchemaElement;
}