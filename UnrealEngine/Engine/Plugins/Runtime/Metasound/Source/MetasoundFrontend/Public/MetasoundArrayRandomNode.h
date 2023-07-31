// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/CircularQueue.h"
#include "Interfaces/MetasoundFrontendSourceInterface.h"
#include "MetasoundArrayNodes.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundParamHelper.h"
#include "MetasoundTrigger.h"

#define LOCTEXT_NAMESPACE "MetasoundFrontend"

namespace Metasound
{
	namespace ArrayNodeRandomGetVertexNames
	{
		METASOUND_PARAM(InputTriggerNextValue, "Next", "Trigger to get the next value in the randomized array.")
		METASOUND_PARAM(InputTriggerResetSeed, "Reset", "Trigger to reset the seed for the randomized array.")
		METASOUND_PARAM(InputRandomArray, "In Array", "Input array to randomized.")
		METASOUND_PARAM(InputWeights, "Weights", "Input array of weights to use for random selection. Will repeat if this array is shorter than the input array to select from.")
		METASOUND_PARAM(InputSeed, "Seed", "Seed to use for the random shuffle.")
		METASOUND_PARAM(InputNoRepeatOrder, "No Repeats", "The number of elements to track to avoid repeating in a row.")
		METASOUND_PARAM(InputEnableSharedState, "Enable Shared State", "Set to enabled to share state across instances of this MetaSound.")
		METASOUND_PARAM(OutputTriggerOnNext, "On Next", "Triggers when the \"Next\" input is triggered.")
		METASOUND_PARAM(OutputTriggerOnReset, "On Reset", "Triggers when the \"Shuffle\" input is triggered or if the array is auto-shuffled.")
		METASOUND_PARAM(ShuffleOutputValue, "Value", "Value of the current shuffled element.")
	}

	class METASOUNDFRONTEND_API FArrayRandomGet
	{
	public:
		FArrayRandomGet() = default;
		FArrayRandomGet(int32 InSeed, int32 InMaxIndex, const TArray<float>& InWeights, int32 InNoRepeatOrder);
		~FArrayRandomGet() = default;

		void Init(int32 InSeed, int32 InMaxIndex, const TArray<float>& InWeights, int32 InNoRepeatOrder);
		void SetSeed(int32 InSeed);
		void SetNoRepeatOrder(int32 InNoRepeatOrder);
		void SetRandomWeights(const TArray<float>& InRandomWeights);
		void ResetSeed();
		int32 NextValue();

		int32 GetNoRepeatOrder() const { return NoRepeatOrder; }
		int32 GetMaxIndex() const { return MaxIndex; }

	private:
		float ComputeTotalWeight();

		// The current index into the array of indicies (wraps between 0 and ShuffleIndices.Num())
		TArray<int32> PreviousIndices;
		TUniquePtr<TCircularQueue<int32>> PreviousIndicesQueue;
		int32 NoRepeatOrder = INDEX_NONE;

		// Array of indices (in order 0 to Num)
		int32 MaxIndex = 0;
		TArray<float> RandomWeights;

		// Random stream to use to randomize the shuffling
		FRandomStream RandomStream;
	};

	struct InitSharedStateArgs
	{
		FGuid SharedStateId;
		int32 Seed = INDEX_NONE;
		int32 NumElements = 0;
		int32 NoRepeatOrder = 0;
		bool bIsPreviewSound = false;
		TArray<float> Weights;
	};

	class METASOUNDFRONTEND_API FSharedStateRandomGetManager
	{
	public:
		static FSharedStateRandomGetManager& Get();

		void InitSharedState(InitSharedStateArgs& InArgs);
		int32 NextValue(const FGuid& InSharedStateId);
		void SetSeed(const FGuid& InSharedStateId, int32 InSeed);
		void SetNoRepeatOrder(const FGuid& InSharedStateId, int32 InNoRepeatOrder);
		void SetRandomWeights(const FGuid& InSharedStateId, const TArray<float>& InRandomWeights);
		void ResetSeed(const FGuid& InSharedStateId);

	private:
		FSharedStateRandomGetManager() = default;
		~FSharedStateRandomGetManager() = default;

		FCriticalSection CritSect;

		TMap<FGuid, TUniquePtr<FArrayRandomGet>> RandomGets;
	};

	template<typename ArrayType>
	class TArrayRandomGetOperator : public TExecutableOperator<TArrayRandomGetOperator<ArrayType>>
	{
	public:
		using FArrayDataReadReference = TDataReadReference<ArrayType>;
		using FArrayWeightReadReference = TDataReadReference<TArray<float>>;
		using WeightsArrayType = TArray<float>;
		using ElementType = typename MetasoundArrayNodesPrivate::TArrayElementType<ArrayType>::Type;
		using FElementTypeWriteReference = TDataWriteReference<ElementType>;

		static const FVertexInterface& GetDefaultInterface()
		{
			using namespace ArrayNodeRandomGetVertexNames;

			static const FVertexInterface DefaultInterface(
				FInputVertexInterface(
					TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerNextValue)),
					TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerResetSeed)),
					TInputDataVertex<ArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputRandomArray)),
					TInputDataVertex<WeightsArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputWeights)),
					TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSeed), -1),
					TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputNoRepeatOrder), 1),
					TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputEnableSharedState), false)
				),
				FOutputVertexInterface(
					TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerOnNext)),
					TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerOnReset)),
					TOutputDataVertex<ElementType>(METASOUND_GET_PARAM_NAME_AND_METADATA(ShuffleOutputValue))
				)
			);

			return DefaultInterface;
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
			{
				FName DataTypeName = GetMetasoundDataTypeName<ArrayType>();
				FName OperatorName = "Random Get";
				FText NodeDisplayName = METASOUND_LOCTEXT_FORMAT("RandomArrayGetNode_OpDisplayNamePattern", "Random Get ({0})", GetMetasoundDataTypeDisplayText<ArrayType>());
				FText NodeDescription = METASOUND_LOCTEXT("RandomArrayGetNode_Description", "Randomly retrieve data from input array using the supplied weights.");
				FVertexInterface NodeInterface = GetDefaultInterface();

				return MetasoundArrayNodesPrivate::CreateArrayNodeClassMetadata(DataTypeName, OperatorName, NodeDisplayName, NodeDescription, NodeInterface);
			};

			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();

			return Metadata;
		}

		static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, TArray<TUniquePtr<IOperatorBuildError>>& OutErrors)
		{
			using namespace ArrayNodeRandomGetVertexNames;
			using namespace MetasoundArrayNodesPrivate;

			const FInputVertexInterface& Inputs = InParams.Node.GetVertexInterface().GetInputInterface();

			FTriggerReadRef InTriggerNext = InParams.InputDataReferences.GetDataReadReferenceOrConstructWithVertexDefault<FTrigger>(Inputs, METASOUND_GET_PARAM_NAME(InputTriggerNextValue), InParams.OperatorSettings);
			FTriggerReadRef InTriggerReset = InParams.InputDataReferences.GetDataReadReferenceOrConstructWithVertexDefault<FTrigger>(Inputs, METASOUND_GET_PARAM_NAME(InputTriggerResetSeed), InParams.OperatorSettings);
			FArrayDataReadReference InInputArray = InParams.InputDataReferences.GetDataReadReferenceOrConstructWithVertexDefault<ArrayType>(Inputs, METASOUND_GET_PARAM_NAME(InputRandomArray), InParams.OperatorSettings);
			FArrayWeightReadReference InInputWeightsArray = InParams.InputDataReferences.GetDataReadReferenceOrConstructWithVertexDefault<WeightsArrayType>(Inputs, METASOUND_GET_PARAM_NAME(InputWeights), InParams.OperatorSettings);
			FInt32ReadRef InSeedValue = InParams.InputDataReferences.GetDataReadReferenceOrConstructWithVertexDefault<int32>(Inputs, METASOUND_GET_PARAM_NAME(InputSeed), InParams.OperatorSettings);
			FInt32ReadRef InNoRepeatOrder = InParams.InputDataReferences.GetDataReadReferenceOrConstructWithVertexDefault<int32>(Inputs, METASOUND_GET_PARAM_NAME(InputNoRepeatOrder), InParams.OperatorSettings);
			FBoolReadRef bInEnableSharedState = InParams.InputDataReferences.GetDataReadReferenceOrConstructWithVertexDefault<bool>(Inputs, METASOUND_GET_PARAM_NAME(InputEnableSharedState), InParams.OperatorSettings);

			return MakeUnique<TArrayRandomGetOperator<ArrayType>>(InParams, InTriggerNext, InTriggerReset, InInputArray, InInputWeightsArray, InSeedValue, InNoRepeatOrder, bInEnableSharedState);
		}

		TArrayRandomGetOperator(
			const FCreateOperatorParams& InParams,
			const FTriggerReadRef& InTriggerNext,
			const FTriggerReadRef& InTriggerReset,
			const FArrayDataReadReference& InInputArray,
			const TDataReadReference<WeightsArrayType>& InInputWeightsArray,
			const FInt32ReadRef& InSeedValue,
			const FInt32ReadRef& InNoRepeatOrder,
			const FBoolReadRef& bInEnableSharedState)
			: TriggerNext(InTriggerNext)
			, TriggerReset(InTriggerReset)
			, InputArray(InInputArray)
			, InputWeightsArray(InInputWeightsArray)
			, SeedValue(InSeedValue)
			, NoRepeatOrder(InNoRepeatOrder)
			, bEnableSharedState(bInEnableSharedState)
			, TriggerOnNext(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
			, TriggerOnReset(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
			, OutValue(TDataWriteReferenceFactory<ElementType>::CreateAny(InParams.OperatorSettings))
		{
			using namespace Frontend;

#if WITH_METASOUND_DEBUG_ENVIRONMENT
			GraphName = *InParams.Environment.GetValue<FString>(SourceInterface::Environment::GraphName);
#endif // WITH_METASOUND_DEBUG_ENVIRONMENT

			// Check to see if this is a global shuffler or a local one. 
			// Global shuffler will use a namespace to opt into it.
			PrevSeedValue = *SeedValue;
			PrevNoRepeatOrder = FMath::Max(*NoRepeatOrder, 0);

			WeightsArray = *InputWeightsArray;

			const ArrayType& InputArrayRef = *InputArray;
			PrevArraySize = InputArrayRef.Num();

			if (PrevArraySize > 0)
			{
				if (*bEnableSharedState)
				{
					// Get the environment variable for the unique ID of the sound
					SharedStateUniqueId = InParams.Node.GetInstanceID();
					check(SharedStateUniqueId.IsValid());

					bIsPreviewSound = InParams.Environment.GetValue<bool>(SourceInterface::Environment::IsPreview);

					FSharedStateRandomGetManager& RGM = FSharedStateRandomGetManager::Get();

					InitSharedStateArgs Args;
					Args.SharedStateId = SharedStateUniqueId;
					Args.Seed = PrevSeedValue;
					Args.NumElements = PrevArraySize;
					Args.NoRepeatOrder = PrevNoRepeatOrder;
					Args.bIsPreviewSound = bIsPreviewSound;
					Args.Weights = WeightsArray;

					RGM.InitSharedState(Args);
				}
				else
				{
					ArrayRandomGet = MakeUnique<FArrayRandomGet>(PrevSeedValue, PrevArraySize, WeightsArray, PrevNoRepeatOrder);
				}
			}
#if WITH_METASOUND_DEBUG_ENVIRONMENT
			else
			{
 				UE_LOG(LogMetaSound, Verbose, TEXT("Array Random Get: Can't retrieve random elements from an empty array in graph '%s'"), *GraphName);
			}
#endif // WITH_METASOUND_DEBUG_ENVIRONMENT
		}

		virtual ~TArrayRandomGetOperator() = default;

		virtual FDataReferenceCollection GetInputs() const override
		{
			using namespace ArrayNodeRandomGetVertexNames;

			FDataReferenceCollection Inputs;
			Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTriggerNextValue), TriggerNext);
			Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTriggerResetSeed), TriggerReset);
			Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputRandomArray), InputArray);
			Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputWeights), InputWeightsArray);
			Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputSeed), SeedValue);
			Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputNoRepeatOrder), NoRepeatOrder);
			Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputEnableSharedState), bEnableSharedState);

			return Inputs;
		}

		virtual FDataReferenceCollection GetOutputs() const override
		{
			using namespace ArrayNodeRandomGetVertexNames;

			FDataReferenceCollection Outputs;
			Outputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputTriggerOnNext), TriggerOnNext);
			Outputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputTriggerOnReset), TriggerOnReset);
			Outputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(ShuffleOutputValue), OutValue);

			return Outputs;
		}

		void Execute()
		{
			TriggerOnNext->AdvanceBlock();
			TriggerOnReset->AdvanceBlock();

			const ArrayType& InputArrayRef = *InputArray;

			// If the array size has changed, we need to reinit before getting the next value
			if (PrevArraySize != InputArrayRef.Num())
			{
				PrevArraySize = InputArrayRef.Num();
				if (PrevArraySize != 0)
				{
					if (!ArrayRandomGet.IsValid())
					{
						ArrayRandomGet = MakeUnique<FArrayRandomGet>(PrevSeedValue, PrevArraySize, WeightsArray, PrevNoRepeatOrder);
					}
					ArrayRandomGet->Init(PrevSeedValue, PrevArraySize, WeightsArray, PrevNoRepeatOrder);
				}
			}

			if (PrevArraySize == 0)
			{
				if (!bHasLoggedEmptyArrayWarning)
				{
#if WITH_METASOUND_DEBUG_ENVIRONMENT
					UE_LOG(LogMetaSound, Verbose, TEXT("Array Random Get: empty array input (Graph '%s')"), *GraphName);
#endif // WITH_METASOUND_DEBUG_ENVIRONMENT
					bHasLoggedEmptyArrayWarning = true;
				}
				return;
			}

			// Check for a seed change
			if (PrevSeedValue != *SeedValue)
			{
				PrevSeedValue = *SeedValue;

				if (SharedStateUniqueId.IsValid())
				{
					FSharedStateRandomGetManager& RGM = FSharedStateRandomGetManager::Get();
					RGM.SetSeed(SharedStateUniqueId, PrevSeedValue);
				}
				else
				{
					check(ArrayRandomGet.IsValid());
					ArrayRandomGet->SetSeed(PrevSeedValue);
				}
			}

			if (PrevNoRepeatOrder != *NoRepeatOrder)
			{
				PrevNoRepeatOrder = *NoRepeatOrder;
				if (SharedStateUniqueId.IsValid())
				{
					FSharedStateRandomGetManager& RGM = FSharedStateRandomGetManager::Get();
					RGM.SetNoRepeatOrder(SharedStateUniqueId, PrevNoRepeatOrder);
				}
				else
				{
					check(ArrayRandomGet.IsValid());
					ArrayRandomGet->SetNoRepeatOrder(PrevNoRepeatOrder);
				}
			}

			WeightsArray = *InputWeightsArray;
			if (SharedStateUniqueId.IsValid())
			{
				FSharedStateRandomGetManager& RGM = FSharedStateRandomGetManager::Get();
				RGM.SetRandomWeights(SharedStateUniqueId, WeightsArray);
			}
			else
			{
				check(ArrayRandomGet.IsValid());
				ArrayRandomGet->SetRandomWeights(WeightsArray);
			}
 
			// Don't do anything if our array is empty
			if (InputArrayRef.Num() == 0)
			{
				return;
			}
 
 			TriggerReset->ExecuteBlock(
				[&](int32 StartFrame, int32 EndFrame)
				{
				},
				[this](int32 StartFrame, int32 EndFrame)
				{
					if (SharedStateUniqueId.IsValid())
					{
						FSharedStateRandomGetManager& RGM = FSharedStateRandomGetManager::Get();
						RGM.ResetSeed(SharedStateUniqueId);
					}
					else
					{
						check(ArrayRandomGet.IsValid());
						ArrayRandomGet->ResetSeed();
					}
					TriggerOnReset->TriggerFrame(StartFrame);
				}
			);
 
			TriggerNext->ExecuteBlock(
				[&](int32 StartFrame, int32 EndFrame)
				{
				},
				[this](int32 StartFrame, int32 EndFrame)
				{
					const ArrayType& InputArrayRef = *InputArray;
					int32 OutRandomIndex = INDEX_NONE;

					if (SharedStateUniqueId.IsValid())
					{
						FSharedStateRandomGetManager& RGM = FSharedStateRandomGetManager::Get();
						OutRandomIndex = RGM.NextValue(SharedStateUniqueId);
					}
					else
					{
						check(ArrayRandomGet.IsValid());
						OutRandomIndex = ArrayRandomGet->NextValue();
					}

					check(OutRandomIndex != INDEX_NONE);

					// The input array size may have changed, so make sure it's wrapped into range of the input array
					*OutValue = InputArrayRef[OutRandomIndex % InputArrayRef.Num()];

					TriggerOnNext->TriggerFrame(StartFrame);
				}
			);
		}

	private:

		// Inputs
		FTriggerReadRef TriggerNext;
		FTriggerReadRef TriggerReset;
		FArrayDataReadReference InputArray;
		TDataReadReference<WeightsArrayType> InputWeightsArray;
		FInt32ReadRef SeedValue;
		FInt32ReadRef NoRepeatOrder;
		FBoolReadRef bEnableSharedState;

		// Outputs
		FTriggerWriteRef TriggerOnNext;
		FTriggerWriteRef TriggerOnReset;
		TDataWriteReference<ElementType> OutValue;

#if WITH_METASOUND_DEBUG_ENVIRONMENT
		FString GraphName;
#endif // WITH_METASOUND_DEBUG_ENVIRONMENT

		// Data
		TUniquePtr<FArrayRandomGet> ArrayRandomGet;
		TArray<float> WeightsArray;
		int32 PrevSeedValue = INDEX_NONE;
		int32 PrevNoRepeatOrder = INDEX_NONE;
		FGuid SharedStateUniqueId;
		int32 PrevArraySize = 0;
		bool bIsPreviewSound = false;
		bool bHasLoggedEmptyArrayWarning = false;
	};

	template<typename ArrayType>
	class TArrayRandomGetNode : public FNodeFacade
	{
	public:
		TArrayRandomGetNode(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<TArrayRandomGetOperator<ArrayType>>())
		{
		}

		virtual ~TArrayRandomGetNode() = default;
	};
}
#undef LOCTEXT_NAMESPACE
