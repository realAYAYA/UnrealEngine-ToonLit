// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsPolicy.h"

#include "LearningAgentsManager.h"
#include "LearningAgentsInteractor.h"
#include "LearningFeatureObject.h"
#include "LearningNeuralNetwork.h"
#include "LearningNeuralNetworkObject.h"
#include "LearningLog.h"

#include "UObject/Package.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "GameFramework/Actor.h"

ULearningAgentsPolicy::ULearningAgentsPolicy() : Super(FObjectInitializer::Get()) {}
ULearningAgentsPolicy::ULearningAgentsPolicy(FVTableHelper& Helper) : Super(Helper) {}
ULearningAgentsPolicy::~ULearningAgentsPolicy() = default;

void ULearningAgentsPolicy::SetupPolicy(
	ULearningAgentsInteractor* InInteractor, 
	const FLearningAgentsPolicySettings& PolicySettings,
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

	// Setup Neural Network

	if (NeuralNetworkAsset)
	{
		// Use Existing Neural Network Asset

		if (NeuralNetworkAsset->NeuralNetwork)
		{
			if (NeuralNetworkAsset->NeuralNetwork->GetInputNum() != Interactor->GetObservationFeature().DimNum() ||
				NeuralNetworkAsset->NeuralNetwork->GetOutputNum() != 2 * Interactor->GetActionFeature().DimNum())
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Neural Network Asset provided during Setup is incorrect size: Inputs and outputs don't match what is required."), *GetName());
				return;
			}

			if (NeuralNetworkAsset->NeuralNetwork->GetHiddenNum() != PolicySettings.HiddenLayerSize ||
				NeuralNetworkAsset->NeuralNetwork->GetLayerNum() != PolicySettings.LayerNum ||
				NeuralNetworkAsset->NeuralNetwork->ActivationFunction != UE::Learning::Agents::GetActivationFunction(PolicySettings.ActivationFunction))
			{
				UE_LOG(LogLearning, Warning, TEXT("%s: Neural Network Asset settings don't match those given by PolicySettings"), *GetName());
			}

			Network = NeuralNetworkAsset;
		}
		else
		{
			Network = NeuralNetworkAsset;
			Network->NeuralNetwork = MakeShared<UE::Learning::FNeuralNetwork>();
			Network->NeuralNetwork->Resize(
				Interactor->GetObservationFeature().DimNum(),
				2 * Interactor->GetActionFeature().DimNum(),
				PolicySettings.HiddenLayerSize,
				PolicySettings.LayerNum);
			Network->NeuralNetwork->ActivationFunction = UE::Learning::Agents::GetActivationFunction(PolicySettings.ActivationFunction);
		}
	}
	else
	{
		// Create New Neural Network Asset

		const FName UniqueName = MakeUniqueObjectName(this, ULearningAgentsNeuralNetwork::StaticClass(), TEXT("PolicyNetwork"), EUniqueObjectNameOptions::GloballyUnique);

		Network = NewObject<ULearningAgentsNeuralNetwork>(this, UniqueName);
		Network->NeuralNetwork = MakeShared<UE::Learning::FNeuralNetwork>();
		Network->NeuralNetwork->Resize(
			Interactor->GetObservationFeature().DimNum(),
			2 * Interactor->GetActionFeature().DimNum(),
			PolicySettings.HiddenLayerSize,
			PolicySettings.LayerNum);
		Network->NeuralNetwork->ActivationFunction = UE::Learning::Agents::GetActivationFunction(PolicySettings.ActivationFunction);
	}

	// Create Policy Object
	UE::Learning::FNeuralNetworkPolicyFunctionSettings PolicyFunctionSettings;
	PolicyFunctionSettings.ActionNoiseMin = PolicySettings.ActionNoiseMin;
	PolicyFunctionSettings.ActionNoiseMax = PolicySettings.ActionNoiseMax;
	PolicyFunctionSettings.ActionNoiseScale = PolicySettings.ActionNoiseScale;

	PolicyObject = MakeShared<UE::Learning::FNeuralNetworkPolicyFunction>(
		TEXT("PolicyObject"),
		Manager->GetInstanceData().ToSharedRef(),
		Manager->GetMaxAgentNum(),
		Network->NeuralNetwork.ToSharedRef(),
		PolicySettings.ActionNoiseSeed,
		PolicyFunctionSettings);

	Manager->GetInstanceData()->Link(Interactor->GetObservationFeature().FeatureHandle, PolicyObject->InputHandle);
	Manager->GetInstanceData()->Link(PolicyObject->OutputHandle, Interactor->GetActionFeature().FeatureHandle);

	bIsSetup = true;

	OnAgentsAdded(Manager->GetAllAgentIds());
}

ULearningAgentsNeuralNetwork* ULearningAgentsPolicy::GetNetworkAsset()
{
	return Network;
}

UE::Learning::FNeuralNetwork& ULearningAgentsPolicy::GetPolicyNetwork()
{
	return *Network->NeuralNetwork;
}

UE::Learning::FNeuralNetworkPolicyFunction& ULearningAgentsPolicy::GetPolicyObject()
{
	return *PolicyObject;
}

void ULearningAgentsPolicy::LoadPolicyFromSnapshot(const FFilePath& File)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	Network->LoadNetworkFromSnapshot(File);
}

void ULearningAgentsPolicy::SavePolicyToSnapshot(const FFilePath& File) const
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	Network->SaveNetworkToSnapshot(File);
}

void ULearningAgentsPolicy::UsePolicyFromAsset(ULearningAgentsNeuralNetwork* NeuralNetworkAsset)
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
	PolicyObject->NeuralNetwork = Network->NeuralNetwork.ToSharedRef();
}

void ULearningAgentsPolicy::LoadPolicyFromAsset(ULearningAgentsNeuralNetwork* NeuralNetworkAsset)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	Network->LoadNetworkFromAsset(NeuralNetworkAsset);
}

void ULearningAgentsPolicy::SavePolicyToAsset(ULearningAgentsNeuralNetwork* NeuralNetworkAsset)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	Network->SaveNetworkToAsset(NeuralNetworkAsset);
}

void ULearningAgentsPolicy::EvaluatePolicy()
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsPolicy::EvaluatePolicy);

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
			UE_LOG(LogLearning, Warning, TEXT("%s: Agent with id %i has not made observations so policy will not be evaluated for it."), *GetName(), AgentId);
			continue;
		}

		ValidAgentIds.Add(AgentId);
	}

	ValidAgentSet = ValidAgentIds;
	ValidAgentSet.TryMakeSlice();

	// Evaluate Policy

	PolicyObject->Evaluate(ValidAgentSet);

	// Increment Action Encoding Iteration

	for (const int32 AgentId : ValidAgentSet)
	{
		Interactor->GetActionEncodingAgentIteration()[AgentId]++;
	}

	// Visual Logger

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	VisualLog(ValidAgentSet);
#endif
}

void ULearningAgentsPolicy::RunInference()
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsPolicy::RunInference);

	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	Interactor->EncodeObservations();
	EvaluatePolicy();
	Interactor->DecodeActions();
}
float ULearningAgentsPolicy::GetActionNoiseScale() const
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return 0.0f;
	}

	return PolicyObject->InstanceData->ConstView(PolicyObject->ActionNoiseScaleHandle)[0];
}

void ULearningAgentsPolicy::SetActionNoiseScale(const float ActionNoiseScale)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}


	UE::Learning::Array::Set(PolicyObject->InstanceData->View(PolicyObject->ActionNoiseScaleHandle), ActionNoiseScale);
}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
void ULearningAgentsPolicy::VisualLog(const UE::Learning::FIndexSet AgentSet) const
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsPolicy::VisualLog);

	const TLearningArrayView<2, const float> InputView = PolicyObject->InstanceData->ConstView(PolicyObject->InputHandle);
	const TLearningArrayView<2, const float> OutputView = PolicyObject->InstanceData->ConstView(PolicyObject->OutputHandle);
	const TLearningArrayView<2, const float> OutputMeanView = PolicyObject->InstanceData->ConstView(PolicyObject->OutputMeanHandle);
	const TLearningArrayView<2, const float> OutputStdView = PolicyObject->InstanceData->ConstView(PolicyObject->OutputStdHandle);
	const TLearningArrayView<1, const float> ActionNoiseScaleView = PolicyObject->InstanceData->ConstView(PolicyObject->ActionNoiseScaleHandle);

	for (const int32 AgentId : AgentSet)
	{
		if (const AActor* Actor = Cast<AActor>(GetAgent(AgentId)))
		{
			UE_LEARNING_AGENTS_VLOG_STRING(this, LogLearning, Display,
				Actor->GetActorLocation(),
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nAction Noise Scale: [% 6.3f]\nInput: %s\nInput Stats (Min/Max/Mean/Std): %s\nOutput Mean: %s\nOutput Std: %s\nOutput Sample: %s\nOutput Stats (Min/Max/Mean/Std): %s"),
				AgentId,
				ActionNoiseScaleView[AgentId],
				*UE::Learning::Array::FormatFloat(InputView[AgentId]),
				*UE::Learning::Agents::Debug::FloatArrayToStatsString(InputView[AgentId]),
				*UE::Learning::Array::FormatFloat(OutputMeanView[AgentId]),
				*UE::Learning::Array::FormatFloat(OutputStdView[AgentId]),
				*UE::Learning::Array::FormatFloat(OutputView[AgentId]),
				*UE::Learning::Agents::Debug::FloatArrayToStatsString(OutputView[AgentId]));

		}
	}
}
#endif
