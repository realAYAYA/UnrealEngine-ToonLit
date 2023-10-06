// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsCritic.h"

#include "LearningAgentsManager.h"
#include "LearningAgentsInteractor.h"
#include "LearningAgentsHelpers.h"
#include "LearningFeatureObject.h"
#include "LearningNeuralNetwork.h"
#include "LearningNeuralNetworkObject.h"
#include "LearningLog.h"

#include "UObject/Package.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "GameFramework/Actor.h"

ULearningAgentsCritic::ULearningAgentsCritic() : Super(FObjectInitializer::Get()) {}
ULearningAgentsCritic::ULearningAgentsCritic(FVTableHelper& Helper) : Super(Helper) {}
ULearningAgentsCritic::~ULearningAgentsCritic() = default;

void ULearningAgentsCritic::SetupCritic(
	ULearningAgentsInteractor* InInteractor, 
	const FLearningAgentsCriticSettings& CriticSettings,
	ULearningAgentsNeuralNetwork* NeuralNetworkAsset)
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

	if (NeuralNetworkAsset)
	{
		// Use Existing Neural Network Asset

		if (NeuralNetworkAsset->NeuralNetwork)
		{
			if (NeuralNetworkAsset->NeuralNetwork->GetInputNum() != Interactor->GetObservationFeature().DimNum() ||
				NeuralNetworkAsset->NeuralNetwork->GetOutputNum() != 2 * Interactor->GetActionFeature().DimNum())
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Neural Network Asset provided during Setup is incorrect size: Inputs and outputs don't match."), *GetName());
				return;
			}

			if (NeuralNetworkAsset->NeuralNetwork->GetHiddenNum() != CriticSettings.HiddenLayerSize ||
				NeuralNetworkAsset->NeuralNetwork->GetLayerNum() != CriticSettings.LayerNum ||
				NeuralNetworkAsset->NeuralNetwork->ActivationFunction != UE::Learning::Agents::GetActivationFunction(CriticSettings.ActivationFunction))
			{
				UE_LOG(LogLearning, Warning, TEXT("%s: Neural Network Asset settings don't match those given by CriticSettings"), *GetName());
			}

			Network = NeuralNetworkAsset;
		}
		else
		{
			Network = NeuralNetworkAsset;
			Network->NeuralNetwork = MakeShared<UE::Learning::FNeuralNetwork>();
			Network->NeuralNetwork->Resize(
				Interactor->GetObservationFeature().DimNum(),
				1,
				CriticSettings.HiddenLayerSize,
				CriticSettings.LayerNum);
			Network->NeuralNetwork->ActivationFunction = UE::Learning::Agents::GetActivationFunction(CriticSettings.ActivationFunction);
		}
	}
	else
	{
		// Create New Neural Network Asset

		const FName UniqueName = MakeUniqueObjectName(this, ULearningAgentsNeuralNetwork::StaticClass(), TEXT("CriticNetwork"), EUniqueObjectNameOptions::GloballyUnique);

		Network = NewObject<ULearningAgentsNeuralNetwork>(this, UniqueName);
		Network->NeuralNetwork = MakeShared<UE::Learning::FNeuralNetwork>();
		Network->NeuralNetwork->Resize(
			Interactor->GetObservationFeature().DimNum(),
			1,
			CriticSettings.HiddenLayerSize,
			CriticSettings.LayerNum);
		Network->NeuralNetwork->ActivationFunction = UE::Learning::Agents::GetActivationFunction(CriticSettings.ActivationFunction);
	}

	// Create Critic Object
	CriticObject = MakeShared<UE::Learning::FNeuralNetworkCriticFunction>(
		TEXT("CriticObject"),
		Manager->GetInstanceData().ToSharedRef(),
		Manager->GetMaxAgentNum(),
		Network->NeuralNetwork.ToSharedRef());

	Manager->GetInstanceData()->Link(Interactor->GetObservationFeature().FeatureHandle, CriticObject->InputHandle);

	DiscountedReturnAgentIteration.SetNumUninitialized({ Manager->GetMaxAgentNum() });
	UE::Learning::Array::Set<1, uint64>(DiscountedReturnAgentIteration, INDEX_NONE);

	bIsSetup = true;

	OnAgentsAdded(Manager->GetAllAgentIds());
}

void ULearningAgentsCritic::OnAgentsAdded(const TArray<int32>& AgentIds)
{
	if (IsSetup())
	{
		UE::Learning::Array::Set<1, uint64>(DiscountedReturnAgentIteration, 0, AgentIds);

		for (ULearningAgentsHelper* Helper : HelperObjects)
		{
			Helper->OnAgentsAdded(AgentIds);
		}

		AgentsAdded(AgentIds);
	}
}

void ULearningAgentsCritic::OnAgentsRemoved(const TArray<int32>& AgentIds)
{
	if (IsSetup())
	{
		UE::Learning::Array::Set<1, uint64>(DiscountedReturnAgentIteration, INDEX_NONE, AgentIds);

		for (ULearningAgentsHelper* Helper : HelperObjects)
		{
			Helper->OnAgentsRemoved(AgentIds);
		}

		AgentsRemoved(AgentIds);
	}
}

void ULearningAgentsCritic::OnAgentsReset(const TArray<int32>& AgentIds)
{
	if (IsSetup())
	{
		UE::Learning::Array::Set<1, uint64>(DiscountedReturnAgentIteration, 0, AgentIds);

		for (ULearningAgentsHelper* Helper : HelperObjects)
		{
			Helper->OnAgentsReset(AgentIds);
		}

		AgentsReset(AgentIds);
	}
}

ULearningAgentsNeuralNetwork* ULearningAgentsCritic::GetNetworkAsset()
{
	return Network;
}

UE::Learning::FNeuralNetwork& ULearningAgentsCritic::GetCriticNetwork()
{
	return *Network->NeuralNetwork;
}

UE::Learning::FNeuralNetworkCriticFunction& ULearningAgentsCritic::GetCriticObject()
{
	return *CriticObject;
}

void ULearningAgentsCritic::LoadCriticFromSnapshot(const FFilePath& File)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	Network->LoadNetworkFromSnapshot(File);
}

void ULearningAgentsCritic::SaveCriticToSnapshot(const FFilePath& File) const
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	Network->SaveNetworkToSnapshot(File);
}

void ULearningAgentsCritic::UseCriticFromAsset(ULearningAgentsNeuralNetwork* NeuralNetworkAsset)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	if (!NeuralNetworkAsset || !NeuralNetworkAsset->NeuralNetwork)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Asset is invalid."), *GetName());
		return;
	}

	if (NeuralNetworkAsset == Network)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Asset is same as the current network."), *GetName());
		return;
	}

	if (NeuralNetworkAsset->NeuralNetwork->GetInputNum() != Network->NeuralNetwork->GetInputNum() ||
		NeuralNetworkAsset->NeuralNetwork->GetOutputNum() != Network->NeuralNetwork->GetOutputNum() ||
		NeuralNetworkAsset->NeuralNetwork->GetLayerNum() != Network->NeuralNetwork->GetLayerNum() ||
		NeuralNetworkAsset->NeuralNetwork->ActivationFunction != Network->NeuralNetwork->ActivationFunction)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Failed to use asset as network settings don't match."), *GetName());
		return;
	}

	Network = NeuralNetworkAsset;
	CriticObject->NeuralNetwork = Network->NeuralNetwork.ToSharedRef();
}

void ULearningAgentsCritic::LoadCriticFromAsset(ULearningAgentsNeuralNetwork* NeuralNetworkAsset)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	Network->LoadNetworkFromAsset(NeuralNetworkAsset);
}

void ULearningAgentsCritic::SaveCriticToAsset(ULearningAgentsNeuralNetwork* NeuralNetworkAsset)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	Network->SaveNetworkToAsset(NeuralNetworkAsset);
}

void ULearningAgentsCritic::EvaluateCritic()
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsCritic::EvaluateCritic);

	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	// Check Agents actually have encoded observations.

	ValidAgentIds.Empty(Manager->GetAgentNum());

	for (const int32 AgentId : Manager->GetAllAgentSet())
	{
		if (Interactor->GetObservationEncodingAgentIteration()[AgentId] == 0)
		{
			UE_LOG(LogLearning, Warning, TEXT("%s: Agent with id %i has not made observations so critic will not be evaluated for it."), *GetName(), AgentId);
			continue;
		}

		ValidAgentIds.Add(AgentId);
	}

	ValidAgentSet = ValidAgentIds;
	ValidAgentSet.TryMakeSlice();

	// Evaluate Critic

	CriticObject->Evaluate(ValidAgentSet);

	// Increment Discounted Return Iteration

	for (const int32 AgentId : ValidAgentSet)
	{
		DiscountedReturnAgentIteration[AgentId]++;
	}
		
	// Visual Logger

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	VisualLog(ValidAgentSet);
#endif
}

float ULearningAgentsCritic::GetEstimatedDiscountedReturn(const int32 AgentId) const
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return 0.0f;
	}

	if (!HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return 0.0f;
	}

	if (DiscountedReturnAgentIteration[AgentId] == 0)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Agent with id %d has not yet computed the estimated discounted return. Did you run EvaluateCritic?"), *GetName(), AgentId);
		return 0.0f;
	}

	return CriticObject->InstanceData->ConstView(CriticObject->OutputHandle)[AgentId];
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void ULearningAgentsCritic::VisualLog(const UE::Learning::FIndexSet AgentSet) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsCritic::VisualLog);

	const TLearningArrayView<2, const float> InputView = CriticObject->InstanceData->ConstView(CriticObject->InputHandle);
	const TLearningArrayView<1, const float> OutputView = CriticObject->InstanceData->ConstView(CriticObject->OutputHandle);

	for (const int32 AgentId : AgentSet)
	{
		if (const AActor* Actor = Cast<AActor>(GetAgent(AgentId)))
		{
			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation(),
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nInput: %s\nInput Stats (Min/Max/Mean/Std): %s\nOutput: [% 6.3f]"),
				AgentId,
				*UE::Learning::Array::FormatFloat(InputView[AgentId]),
				*UE::Learning::Agents::Debug::FloatArrayToStatsString(InputView[AgentId]),
				OutputView[AgentId]);
		}
	}
}
#endif
