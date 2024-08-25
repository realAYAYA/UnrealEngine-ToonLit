// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsPolicy.h"

#include "LearningAgentsManager.h"
#include "LearningAgentsInteractor.h"
#include "LearningNeuralNetwork.h"
#include "LearningPolicy.h"
#include "LearningLog.h"
#include "LearningRandom.h"

#include "UObject/Package.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "GameFramework/Actor.h"

#include "NNERuntimeBasicCpuBuilder.h"

namespace UE::Learning::Agents::Policy::Private
{
	static inline UE::NNE::RuntimeBasic::FModelBuilder::EActivationFunction GetBuilderActivationFunction(const ELearningAgentsActivationFunction ActivationFunction)
	{
		switch (ActivationFunction)
		{
		case ELearningAgentsActivationFunction::ReLU: return UE::NNE::RuntimeBasic::FModelBuilder::EActivationFunction::ReLU;
		case ELearningAgentsActivationFunction::ELU: return UE::NNE::RuntimeBasic::FModelBuilder::EActivationFunction::ELU;
		case ELearningAgentsActivationFunction::TanH: return UE::NNE::RuntimeBasic::FModelBuilder::EActivationFunction::TanH;
		default:
			UE_LOG(LogLearning, Error, TEXT("Unknown Activation Function"));
			return UE::NNE::RuntimeBasic::FModelBuilder::EActivationFunction::ReLU;
		}
	}
}

ULearningAgentsPolicy::ULearningAgentsPolicy() : Super(FObjectInitializer::Get()) {}
ULearningAgentsPolicy::ULearningAgentsPolicy(FVTableHelper& Helper) : Super(Helper) {}
ULearningAgentsPolicy::~ULearningAgentsPolicy() = default;

ULearningAgentsPolicy* ULearningAgentsPolicy::MakePolicy(
	ULearningAgentsManager* InManager,
	ULearningAgentsInteractor* InInteractor,
	TSubclassOf<ULearningAgentsPolicy> Class,
	const FName Name,
	ULearningAgentsNeuralNetwork* EncoderNeuralNetworkAsset,
	ULearningAgentsNeuralNetwork* PolicyNeuralNetworkAsset,
	ULearningAgentsNeuralNetwork* DecoderNeuralNetworkAsset,
	const bool bReinitializeEncoderNetwork,
	const bool bReinitializePolicyNetwork,
	const bool bReinitializeDecoderNetwork,
	const FLearningAgentsPolicySettings& PolicySettings,
	const int32 Seed)
{
	if (!InManager)
	{
		UE_LOG(LogLearning, Error, TEXT("MakePolicy: InManager is nullptr."));
		return nullptr;
	}

	if (!Class)
	{
		UE_LOG(LogLearning, Error, TEXT("MakePolicy: Class is nullptr."));
		return nullptr;
	}

	const FName UniqueName = MakeUniqueObjectName(InManager, Class, Name, EUniqueObjectNameOptions::GloballyUnique);

	ULearningAgentsPolicy* Policy = NewObject<ULearningAgentsPolicy>(InManager, Class, UniqueName);
	if (!Policy) { return nullptr; }

	Policy->SetupPolicy(
		InManager,
		InInteractor,
		EncoderNeuralNetworkAsset,
		PolicyNeuralNetworkAsset,
		DecoderNeuralNetworkAsset,
		bReinitializeEncoderNetwork,
		bReinitializePolicyNetwork,
		bReinitializeDecoderNetwork,
		PolicySettings,
		Seed);

	return Policy->IsSetup() ? Policy : nullptr;
}

void ULearningAgentsPolicy::SetupPolicy(
	ULearningAgentsManager* InManager,
	ULearningAgentsInteractor* InInteractor,
	ULearningAgentsNeuralNetwork* EncoderNeuralNetworkAsset,
	ULearningAgentsNeuralNetwork* PolicyNeuralNetworkAsset,
	ULearningAgentsNeuralNetwork* DecoderNeuralNetworkAsset,
	const bool bReinitializeEncoderNetwork,
	const bool bReinitializePolicyNetwork,
	const bool bReinitializeDecoderNetwork,
	const FLearningAgentsPolicySettings& PolicySettings,
	const int32 Seed)
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

	// Warn if Network Assets are not unique

	if (PolicyNeuralNetworkAsset && PolicyNeuralNetworkAsset == EncoderNeuralNetworkAsset)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Identical Network Assets given as both Policy and Encoder: %s."), *GetName(), *PolicyNeuralNetworkAsset->GetName());
	}


	if (PolicyNeuralNetworkAsset && PolicyNeuralNetworkAsset == DecoderNeuralNetworkAsset)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Identical Network Assets given as both Policy and Decoder: %s."), *GetName(), *PolicyNeuralNetworkAsset->GetName());
	}

	if (EncoderNeuralNetworkAsset && EncoderNeuralNetworkAsset == DecoderNeuralNetworkAsset)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Identical Network Assets given as both Encoder and Decoder: %s."), *GetName(), *EncoderNeuralNetworkAsset->GetName());
	}

	// Begin Setup

	Manager = InManager;
	Interactor = InInteractor;

	// Compatibility Hashes and Sizes

	const int32 MemoryStateSize = PolicySettings.bUseMemory ? PolicySettings.MemoryStateSize : 0;
	const int32 ObservationVectorSize = Interactor->GetObservationVectorSize();
	const int32 ObservationEncodedVectorSize = Interactor->GetObservationEncodedVectorSize();
	const int32 ActionEncodedVectorSize = Interactor->GetActionEncodedVectorSize();
	const int32 ActionDistributionVectorSize = Interactor->GetActionDistributionVectorSize();

	const int32 ObservationCompatibilityHash = UE::Learning::Observation::GetSchemaObjectsCompatibilityHash(Interactor->GetObservationSchema(), Interactor->GetObservationSchemaElement());
	const int32 ActionCompatibilityHash = UE::Learning::Action::GetSchemaObjectsCompatibilityHash(Interactor->GetActionSchema(), Interactor->GetActionSchemaElement());
	
	const int32 PolicyHashData[3] = { MemoryStateSize, ObservationEncodedVectorSize, ActionEncodedVectorSize };
	const int32 PolicyCompatibilityHash = CityHash32((const char*)PolicyHashData, 3 * sizeof(int32));

	// Encoder

	if (EncoderNeuralNetworkAsset)
	{
		EncoderNetwork = EncoderNeuralNetworkAsset;

		if (EncoderNeuralNetworkAsset->NeuralNetworkData && !bReinitializeEncoderNetwork)
		{
			if (EncoderNeuralNetworkAsset->NeuralNetworkData->GetCompatibilityHash() != ObservationCompatibilityHash)
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Encoder Network Asset provided during Setup is incompatible with Schema. Network hash is %i vs Schema hash %i."), *GetName(),
					EncoderNeuralNetworkAsset->NeuralNetworkData->GetCompatibilityHash(),
					ObservationCompatibilityHash);
				return;
			}

			if (EncoderNeuralNetworkAsset->NeuralNetworkData->GetInputSize() != ObservationVectorSize ||
				EncoderNeuralNetworkAsset->NeuralNetworkData->GetOutputSize() != ObservationEncodedVectorSize)
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Encoder Network Asset provided during Setup is incorrect size: Got inputs of size %i, expected %i. Got outputs of size %i, expected %i."), *GetName(),
					EncoderNeuralNetworkAsset->NeuralNetworkData->GetInputSize(), ObservationVectorSize,
					EncoderNeuralNetworkAsset->NeuralNetworkData->GetOutputSize(), ObservationEncodedVectorSize);
				return;
			}
		}
	}

	if (!EncoderNetwork)
	{
		const FName UniqueName = MakeUniqueObjectName(this, ULearningAgentsNeuralNetwork::StaticClass(), TEXT("EncoderNetwork"), EUniqueObjectNameOptions::GloballyUnique);
		EncoderNetwork = NewObject<ULearningAgentsNeuralNetwork>(this, UniqueName);
	}

	if (!EncoderNetwork->NeuralNetworkData || bReinitializeEncoderNetwork)
	{
		if (!EncoderNetwork->NeuralNetworkData)
		{
			EncoderNetwork->NeuralNetworkData = NewObject<ULearningNeuralNetworkData>(EncoderNetwork);
		}

		TArray<uint8> FileData;
		uint32 EncoderInputSize, EncoderOutputSize;
		UE::Learning::Observation::GenerateEncoderNetworkFileDataFromSchema(
			FileData,
			EncoderInputSize,
			EncoderOutputSize,
			Interactor->GetObservationSchema(),
			Interactor->GetObservationSchemaElement(),
			UE::Learning::Random::Int(Seed ^ 0x658868dd));

		UE_LEARNING_CHECK(EncoderInputSize == ObservationVectorSize);
		UE_LEARNING_CHECK(EncoderOutputSize == ObservationEncodedVectorSize);

		EncoderNetwork->NeuralNetworkData->Init(EncoderInputSize, EncoderOutputSize, ObservationCompatibilityHash, FileData);
		EncoderNetwork->ForceMarkDirty();
	}

	// Policy

	if (PolicyNeuralNetworkAsset)
	{
		PolicyNetwork = PolicyNeuralNetworkAsset;

		if (PolicyNeuralNetworkAsset->NeuralNetworkData && !bReinitializePolicyNetwork)
		{
			if (PolicyNeuralNetworkAsset->NeuralNetworkData->GetCompatibilityHash() != PolicyCompatibilityHash)
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Policy Network Asset provided during Setup is incompatible with Schema. Network hash is %i vs Schema hash %i."), *GetName(),
					PolicyNeuralNetworkAsset->NeuralNetworkData->GetCompatibilityHash(),
					PolicyCompatibilityHash);
				return;
			}

			if (PolicyNeuralNetworkAsset->NeuralNetworkData->GetInputSize() != ObservationEncodedVectorSize + MemoryStateSize ||
				PolicyNeuralNetworkAsset->NeuralNetworkData->GetOutputSize() != ActionEncodedVectorSize + MemoryStateSize)
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Policy Network Asset provided during Setup is incorrect size: Got inputs of size %i, expected %i. Got outputs of size %i, expected %i."), *GetName(),
					PolicyNeuralNetworkAsset->NeuralNetworkData->GetInputSize(), ObservationEncodedVectorSize + MemoryStateSize,
					PolicyNeuralNetworkAsset->NeuralNetworkData->GetOutputSize(), ActionEncodedVectorSize + MemoryStateSize);
				return;
			}
		}
	}

	if (!PolicyNetwork)
	{
		const FName UniqueName = MakeUniqueObjectName(this, ULearningAgentsNeuralNetwork::StaticClass(), TEXT("PolicyNetwork"), EUniqueObjectNameOptions::GloballyUnique);
		PolicyNetwork = NewObject<ULearningAgentsNeuralNetwork>(this, UniqueName);
	}

	if (!PolicyNetwork->NeuralNetworkData || bReinitializePolicyNetwork)
	{
		if (!PolicyNetwork->NeuralNetworkData)
		{
			PolicyNetwork->NeuralNetworkData = NewObject<ULearningNeuralNetworkData>(PolicyNetwork);
		}

		UE::NNE::RuntimeBasic::FModelBuilder Builder(UE::Learning::Random::Int(Seed ^ 0x69315bf9));

		const int32 PolicyHiddenLayerSize = PolicySettings.HiddenLayerSize;
		const int32 PolicyHiddenLayerNum = PolicySettings.HiddenLayerNum;
		const ELearningAgentsActivationFunction PolicyActivationFunction = PolicySettings.ActivationFunction;
		const float PolicyInitialEncodedActionScale = PolicySettings.InitialEncodedActionScale;

		TArray<uint8> FileData;
		uint32 PolicyInputSize, PolicyOutputSize;
		Builder.WriteFileDataAndReset(FileData, PolicyInputSize, PolicyOutputSize,
			Builder.MakeMemoryBackbone(
				MemoryStateSize,
				Builder.MakeMLPWithRandomKaimingWeights(
					ObservationEncodedVectorSize,
					PolicyHiddenLayerSize,
					PolicyHiddenLayerSize,
					PolicyHiddenLayerNum / 2 + 2, // Add 2 to account for input and output layers
					UE::Learning::Agents::Policy::Private::GetBuilderActivationFunction(PolicyActivationFunction),
					true),
				Builder.MakeMemoryCellWithLinearRandomKaimingWeights(
					PolicyHiddenLayerSize,
					PolicyHiddenLayerSize,
					MemoryStateSize,
					0.1f),
				Builder.MakeSequence({
					Builder.MakeMLPWithRandomKaimingWeights(
						PolicyHiddenLayerSize,
						ActionEncodedVectorSize,
						PolicyHiddenLayerSize,
						PolicyHiddenLayerNum / 2 + 2, // Add 2 to account for input and output layers
						UE::Learning::Agents::Policy::Private::GetBuilderActivationFunction(PolicyActivationFunction)),
					Builder.MakeDenormalize(
						ActionEncodedVectorSize,
						Builder.MakeWeightsZero(ActionEncodedVectorSize),
						Builder.MakeWeightsConstant(ActionEncodedVectorSize, PolicyInitialEncodedActionScale))
					})
			));

		UE_LEARNING_CHECK(PolicyInputSize == ObservationEncodedVectorSize + MemoryStateSize);
		UE_LEARNING_CHECK(PolicyOutputSize == ActionEncodedVectorSize + MemoryStateSize);

		PolicyNetwork->NeuralNetworkData->Init(PolicyInputSize, PolicyOutputSize, PolicyCompatibilityHash, FileData);
		PolicyNetwork->ForceMarkDirty();
	}

	// Decoder

	if (DecoderNeuralNetworkAsset)
	{
		DecoderNetwork = DecoderNeuralNetworkAsset;

		if (DecoderNeuralNetworkAsset->NeuralNetworkData && !bReinitializeDecoderNetwork)
		{
			if (DecoderNeuralNetworkAsset->NeuralNetworkData->GetCompatibilityHash() != ActionCompatibilityHash)
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Decoder Network Asset provided during Setup is incompatible with Schema. Network hash is %i vs Schema hash %i."), *GetName(),
					DecoderNeuralNetworkAsset->NeuralNetworkData->GetCompatibilityHash(),
					ActionCompatibilityHash);
				return;
			}

			if (DecoderNeuralNetworkAsset->NeuralNetworkData->GetInputSize() != ActionEncodedVectorSize ||
				DecoderNeuralNetworkAsset->NeuralNetworkData->GetOutputSize() != ActionDistributionVectorSize)
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Decoder Network Asset provided during Setup is incorrect size: Got inputs of size %i, expected %i. Got outputs of size %i, expected %i."), *GetName(),
					DecoderNeuralNetworkAsset->NeuralNetworkData->GetInputSize(), ActionEncodedVectorSize,
					DecoderNeuralNetworkAsset->NeuralNetworkData->GetOutputSize(), ActionDistributionVectorSize);
				return;
			}
		}
	}

	if (!DecoderNetwork)
	{
		const FName UniqueName = MakeUniqueObjectName(this, ULearningAgentsNeuralNetwork::StaticClass(), TEXT("DecoderNetwork"), EUniqueObjectNameOptions::GloballyUnique);
		DecoderNetwork = NewObject<ULearningAgentsNeuralNetwork>(this, UniqueName);
	}

	if (!DecoderNetwork->NeuralNetworkData || bReinitializeDecoderNetwork)
	{
		if (!DecoderNetwork->NeuralNetworkData)
		{
			DecoderNetwork->NeuralNetworkData = NewObject<ULearningNeuralNetworkData>(DecoderNetwork);
		}

		TArray<uint8> FileData;
		uint32 DecoderInputSize, DecoderOutputSize;
		UE::Learning::Action::GenerateDecoderNetworkFileDataFromSchema(
			FileData,
			DecoderInputSize,
			DecoderOutputSize,
			Interactor->GetActionSchema(),
			Interactor->GetActionSchemaElement(),
			UE::Learning::Random::Int(Seed ^ 0xfa88bb7f));

		UE_LEARNING_CHECK(DecoderInputSize == ActionEncodedVectorSize);
		UE_LEARNING_CHECK(DecoderOutputSize == ActionDistributionVectorSize);

		DecoderNetwork->NeuralNetworkData->Init(DecoderInputSize, DecoderOutputSize, ActionCompatibilityHash, FileData);
		DecoderNetwork->ForceMarkDirty();
	}

	// Create Encoder / Policy / Decoder Objects

	EncoderObject = MakeShared<UE::Learning::FNeuralNetworkFunction>(
		Manager->GetMaxAgentNum(),
		EncoderNetwork->NeuralNetworkData->GetNetwork(),
		UE::Learning::FNeuralNetworkInferenceSettings());

	PolicyObject = MakeShared<UE::Learning::FNeuralNetworkPolicy>(
		Manager->GetMaxAgentNum(),
		ObservationEncodedVectorSize,
		ActionEncodedVectorSize,
		MemoryStateSize,
		PolicyNetwork->NeuralNetworkData->GetNetwork(),
		UE::Learning::FNeuralNetworkInferenceSettings());

	DecoderObject = MakeShared<UE::Learning::FNeuralNetworkFunction>(
		Manager->GetMaxAgentNum(),
		DecoderNetwork->NeuralNetworkData->GetNetwork(),
		UE::Learning::FNeuralNetworkInferenceSettings());

	// State Variables

	GlobalSeed = Seed;
	Seeds.SetNumUninitialized({ Manager->GetMaxAgentNum() });
	UE::Learning::Array::Set<1, uint32>(Seeds, INDEX_NONE);

	ObservationVectorsEncoded.SetNumUninitialized({ Manager->GetMaxAgentNum(), ObservationEncodedVectorSize });
	ActionVectorsEncoded.SetNumUninitialized({ Manager->GetMaxAgentNum(), ActionEncodedVectorSize });
	ActionDistributionVectors.SetNumUninitialized({ Manager->GetMaxAgentNum(), ActionDistributionVectorSize });
	UE::Learning::Array::Set(ObservationVectorsEncoded, FLT_MAX);
	UE::Learning::Array::Set(ActionVectorsEncoded, FLT_MAX);
	UE::Learning::Array::Set(ActionDistributionVectors, FLT_MAX);

	PreEvaluationMemoryState.SetNumUninitialized({ Manager->GetMaxAgentNum(), MemoryStateSize });
	MemoryState.SetNumUninitialized({ Manager->GetMaxAgentNum(), MemoryStateSize });
	UE::Learning::Array::Set(PreEvaluationMemoryState, FLT_MAX);
	UE::Learning::Array::Set(MemoryState, FLT_MAX);

	ObservationVectorEncodedIteration.SetNumUninitialized({ Manager->GetMaxAgentNum() });
	ActionVectorEncodedIteration.SetNumUninitialized({ Manager->GetMaxAgentNum() });
	MemoryStateIteration.SetNumUninitialized({ Manager->GetMaxAgentNum() });
	UE::Learning::Array::Set<1, uint64>(ObservationVectorEncodedIteration, INDEX_NONE);
	UE::Learning::Array::Set<1, uint64>(ActionVectorEncodedIteration, INDEX_NONE);
	UE::Learning::Array::Set<1, uint64>(MemoryStateIteration, INDEX_NONE);

	bIsSetup = true;

	Manager->AddListener(this);
}

void ULearningAgentsPolicy::OnAgentsAdded_Implementation(const TArray<int32>& AgentIds)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	UE::Learning::Random::SampleIntArray(Seeds, GlobalSeed, AgentIds);
	UE::Learning::Array::Set<2, float>(ObservationVectorsEncoded, FLT_MAX, AgentIds);
	UE::Learning::Array::Set<2, float>(ActionVectorsEncoded, FLT_MAX, AgentIds);
	UE::Learning::Array::Set<2, float>(ActionDistributionVectors, FLT_MAX, AgentIds);
	UE::Learning::Array::Set<2, float>(PreEvaluationMemoryState, 0.0f, AgentIds);
	UE::Learning::Array::Set<2, float>(MemoryState, 0.0f, AgentIds);
	UE::Learning::Array::Set<1, uint64>(ObservationVectorEncodedIteration, 0, AgentIds);
	UE::Learning::Array::Set<1, uint64>(ActionVectorEncodedIteration, 0, AgentIds);
	UE::Learning::Array::Set<1, uint64>(MemoryStateIteration, 0, AgentIds);
}

void ULearningAgentsPolicy::OnAgentsRemoved_Implementation(const TArray<int32>& AgentIds)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	UE::Learning::Random::SampleIntArray(Seeds, GlobalSeed, AgentIds);
	UE::Learning::Array::Set<2, float>(ObservationVectorsEncoded, FLT_MAX, AgentIds);
	UE::Learning::Array::Set<2, float>(ActionVectorsEncoded, FLT_MAX, AgentIds);
	UE::Learning::Array::Set<2, float>(ActionDistributionVectors, FLT_MAX, AgentIds);
	UE::Learning::Array::Set<2, float>(PreEvaluationMemoryState, FLT_MAX, AgentIds);
	UE::Learning::Array::Set<2, float>(MemoryState, FLT_MAX, AgentIds);
	UE::Learning::Array::Set<1, uint64>(ObservationVectorEncodedIteration, INDEX_NONE, AgentIds);
	UE::Learning::Array::Set<1, uint64>(ActionVectorEncodedIteration, INDEX_NONE, AgentIds);
	UE::Learning::Array::Set<1, uint64>(MemoryStateIteration, INDEX_NONE, AgentIds);
}

void ULearningAgentsPolicy::OnAgentsReset_Implementation(const TArray<int32>& AgentIds)
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	UE::Learning::Random::SampleIntArray(Seeds, GlobalSeed, AgentIds);
	UE::Learning::Array::Set<2, float>(ObservationVectorsEncoded, FLT_MAX, AgentIds);
	UE::Learning::Array::Set<2, float>(ActionVectorsEncoded, FLT_MAX, AgentIds);
	UE::Learning::Array::Set<2, float>(ActionDistributionVectors, FLT_MAX, AgentIds);
	UE::Learning::Array::Set<2, float>(PreEvaluationMemoryState, 0.0f, AgentIds);
	UE::Learning::Array::Set<2, float>(MemoryState, 0.0f, AgentIds);
	UE::Learning::Array::Set<1, uint64>(MemoryStateIteration, 0, AgentIds);
	UE::Learning::Array::Set<1, uint64>(ObservationVectorEncodedIteration, 0, AgentIds);
	UE::Learning::Array::Set<1, uint64>(ActionVectorEncodedIteration, 0, AgentIds);
}

UE::Learning::FNeuralNetworkFunction& ULearningAgentsPolicy::GetEncoderObject()
{
	return *EncoderObject;
}

UE::Learning::FNeuralNetworkPolicy& ULearningAgentsPolicy::GetPolicyObject()
{
	return *PolicyObject;
}

UE::Learning::FNeuralNetworkFunction& ULearningAgentsPolicy::GetDecoderObject()
{
	return *DecoderObject;
}

ULearningAgentsNeuralNetwork* ULearningAgentsPolicy::GetEncoderNetworkAsset()
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return nullptr;
	}

	return EncoderNetwork;
}

ULearningAgentsNeuralNetwork* ULearningAgentsPolicy::GetPolicyNetworkAsset()
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return nullptr;
	}

	return PolicyNetwork;
}

ULearningAgentsNeuralNetwork* ULearningAgentsPolicy::GetDecoderNetworkAsset()
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return nullptr;
	}

	return DecoderNetwork;
}

void ULearningAgentsPolicy::EncodeObservations()
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsPolicy::EncodeObservations);

	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	if (Manager->GetAgentNum() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: No agents added to Manager."), *GetName());
	}

	// Check which agents have had their observation vector set

	ValidAgentIds.Empty(Manager->GetMaxAgentNum());
	for (const int32 AgentId : Manager->GetAllAgentSet())
	{
		if (Interactor->ObservationVectorIteration[AgentId] == 0)
		{
			UE_LOG(LogLearning, Warning, TEXT("%s: Agent with id %i does not have an observation vector ready be encoded. Was GatherObservations run without error?"), *GetName(), AgentId);
			continue;
		}

		ValidAgentIds.Add(AgentId);
	}

	ValidAgentSet = ValidAgentIds;
	ValidAgentSet.TryMakeSlice();

	// Encode Observations

	if (EncoderObject->GetNeuralNetwork()->GetInputSize() != Interactor->ObservationVectors.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Encoder Network Input size doesn't match. Network input size is %i but Encoder expects %i."), *GetName(),
			EncoderObject->GetNeuralNetwork()->GetInputSize(), Interactor->ObservationVectors.Num<1>());
		return;
	}

	if (EncoderObject->GetNeuralNetwork()->GetOutputSize() != ObservationVectorsEncoded.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Encoder Network Output size don't match. Network output size is %i but Encoder expects %i."), *GetName(),
			EncoderObject->GetNeuralNetwork()->GetOutputSize(), ObservationVectorsEncoded.Num<1>());
		return;
	}

	EncoderObject->Evaluate(ObservationVectorsEncoded, Interactor->ObservationVectors, ValidAgentSet);

	for (const int32 AgentId : ValidAgentSet)
	{
		ObservationVectorEncodedIteration[AgentId]++;
	}
}

void ULearningAgentsPolicy::EvaluatePolicy()
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsPolicy::EvaluatePolicy);

	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	if (Manager->GetAgentNum() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: No agents added to Manager."), *GetName());
	}

	// Check agents actually have encoded observations

	ValidAgentIds.Empty(Manager->GetMaxAgentNum());

	for (const int32 AgentId : Manager->GetAllAgentSet())
	{
		if (ObservationVectorEncodedIteration[AgentId] == 0)
		{
			UE_LOG(LogLearning, Warning, TEXT("%s: Agent with id %i has not encoded observations so policy will not be evaluated for it. Was EncodeObservations run without error?"), *GetName(), AgentId);
			continue;
		}

		ValidAgentIds.Add(AgentId);
	}

	ValidAgentSet = ValidAgentIds;
	ValidAgentSet.TryMakeSlice();

	// Copy pre-evaluation memory state

	UE::Learning::Array::Copy<2, float>(PreEvaluationMemoryState, MemoryState, ValidAgentSet);

	// Evaluate policy

	if (PolicyObject->GetNeuralNetwork()->GetInputSize() != ObservationVectorsEncoded.Num<1>() + MemoryState.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Policy Network Input size doesn't match. Network input size is %i but Policy expects %i."), *GetName(),
			PolicyObject->GetNeuralNetwork()->GetInputSize(), ObservationVectorsEncoded.Num<1>() + MemoryState.Num<1>());
		return;
	}

	if (PolicyObject->GetNeuralNetwork()->GetOutputSize() != ActionVectorsEncoded.Num<1>() + MemoryState.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Policy Network Output size don't match. Network output size is %i but Policy expects %i."), *GetName(),
			PolicyObject->GetNeuralNetwork()->GetOutputSize(), ActionVectorsEncoded.Num<1>() + MemoryState.Num<1>());
		return;
	}

	PolicyObject->Evaluate(
		ActionVectorsEncoded,
		MemoryState,
		ObservationVectorsEncoded,
		PreEvaluationMemoryState,
		ValidAgentSet);

	// Increment policy evaluation and action generation iteration

	for (const int32 AgentId : ValidAgentSet)
	{
		MemoryStateIteration[AgentId]++;
		ActionVectorEncodedIteration[AgentId]++;
	}
}

void ULearningAgentsPolicy::DecodeAndSampleActions(const float ActionNoiseScale)
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsPolicy::DecodeAndSampleActions);

	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	if (Manager->GetAgentNum() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: No agents added to Manager."), *GetName());
	}

	// Check which agents have had their encoded action vector set

	ValidAgentIds.Empty(Manager->GetMaxAgentNum());

	for (const int32 AgentId : Manager->GetAllAgentSet())
	{
		if (ActionVectorEncodedIteration[AgentId] == 0)
		{
			UE_LOG(LogLearning, Warning, TEXT("%s: Agent with id %i does not have an encoded action so actions will not be decoded for it. Was EvaluatePolicy run without error?"), *GetName(), AgentId);
			continue;
		}

		ValidAgentIds.Add(AgentId);
	}

	ValidAgentSet = ValidAgentIds;
	ValidAgentSet.TryMakeSlice();

	// Decode to produce action distribution vectors

	if (DecoderObject->GetNeuralNetwork()->GetInputSize() != ActionVectorsEncoded.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Decoder Network Input size doesn't match. Network input size is %i but Decoder expects %i."), *GetName(),
			DecoderObject->GetNeuralNetwork()->GetInputSize(), ActionVectorsEncoded.Num<1>());
		return;
	}

	if (DecoderObject->GetNeuralNetwork()->GetOutputSize() != ActionDistributionVectors.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Decoder Network Output size don't match. Network output size is %i but Decoder expects %i."), *GetName(),
			DecoderObject->GetNeuralNetwork()->GetOutputSize(), ActionDistributionVectors.Num<1>());
		return;
	}

	DecoderObject->Evaluate(ActionDistributionVectors, ActionVectorsEncoded, ValidAgentSet);

	// Sample actions from action distribution vectors

	for (const int32 AgentId : ValidAgentSet)
	{
		UE::Learning::Action::SampleVectorFromDistributionVector(
			Seeds[AgentId],
			Interactor->ActionVectors[AgentId],
			ActionDistributionVectors[AgentId],
			Interactor->ActionSchema->ActionSchema,
			Interactor->ActionSchemaElement.SchemaElement,
			ActionNoiseScale);
	}

	for (const int32 AgentId : ValidAgentSet)
	{
		Interactor->ActionVectorIteration[AgentId]++;
	}
}

void ULearningAgentsPolicy::RunInference(const float ActionNoiseScale)
{
	UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(ULearningAgentsPolicy::RunInference);

	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return;
	}

	Interactor->GatherObservations();
	EncodeObservations();
	EvaluatePolicy();
	DecodeAndSampleActions(ActionNoiseScale);
	Interactor->PerformActions();
}

void ULearningAgentsPolicy::GetMemoryState(TArray<float>& OutMemoryState, const int32 AgentId) const
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		OutMemoryState.Empty();
		return;
	}

	if (!HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		OutMemoryState.Empty();
		return;
	}

	OutMemoryState.SetNumUninitialized(MemoryState.Num<1>());
	UE::Learning::Array::Copy<1, float>(OutMemoryState, MemoryState[AgentId]);
}

void ULearningAgentsPolicy::SetMemoryState(const int32 AgentId, const TArray<float>& InMemoryState)
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

	if (InMemoryState.Num() != MemoryState.Num<1>())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Memory State is incorrect size. Expected %i, got %i."), *GetName(), MemoryState.Num<1>(), InMemoryState.Num());
		return;
	}

	UE::Learning::Array::Copy<1, float>(MemoryState[AgentId], InMemoryState);
}

int32 ULearningAgentsPolicy::GetMemoryStateSize() const
{
	if (!IsSetup())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Setup not complete."), *GetName());
		return 0;
	}

	return MemoryState.Num<1>();
}
