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

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			using namespace ArrayNodeRandomGetVertexNames;
			using namespace MetasoundArrayNodesPrivate;

			const FInputVertexInterfaceData& InputData = InParams.InputData;

			FTriggerReadRef InTriggerNext = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTriggerNextValue), InParams.OperatorSettings);
			FTriggerReadRef InTriggerReset = InputData.GetOrCreateDefaultDataReadReference<FTrigger>(METASOUND_GET_PARAM_NAME(InputTriggerResetSeed), InParams.OperatorSettings);
			FArrayDataReadReference InInputArray = InputData.GetOrCreateDefaultDataReadReference<ArrayType>(METASOUND_GET_PARAM_NAME(InputRandomArray), InParams.OperatorSettings);
			FArrayWeightReadReference InInputWeightsArray = InputData.GetOrCreateDefaultDataReadReference<WeightsArrayType>(METASOUND_GET_PARAM_NAME(InputWeights), InParams.OperatorSettings);
			FInt32ReadRef InSeedValue = InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputSeed), InParams.OperatorSettings);
			FInt32ReadRef InNoRepeatOrder = InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputNoRepeatOrder), InParams.OperatorSettings);
			FBoolReadRef bInEnableSharedState = InputData.GetOrCreateDefaultDataReadReference<bool>(METASOUND_GET_PARAM_NAME(InputEnableSharedState), InParams.OperatorSettings);

			return MakeUnique<TArrayRandomGetOperator<ArrayType>>(InParams, InTriggerNext, InTriggerReset, InInputArray, InInputWeightsArray, InSeedValue, InNoRepeatOrder, bInEnableSharedState);
		}

		TArrayRandomGetOperator(
			const FBuildOperatorParams& InParams,
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
			, TriggerOnNext(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
			, TriggerOnReset(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
			, OutValue(TDataWriteReferenceFactory<ElementType>::CreateAny(InParams.OperatorSettings))
			, bEnableSharedState(bInEnableSharedState)
		{
			using namespace Frontend;

			SharedStateUniqueId = InParams.Node.GetInstanceID();

			Reset(InParams);
		}

		virtual ~TArrayRandomGetOperator() = default;

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace ArrayNodeRandomGetVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerNextValue), TriggerNext);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputTriggerResetSeed), TriggerReset);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputRandomArray), InputArray);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputWeights), InputWeightsArray);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputSeed), SeedValue);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputNoRepeatOrder), NoRepeatOrder);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputEnableSharedState), bEnableSharedState);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace ArrayNodeRandomGetVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputTriggerOnNext), TriggerOnNext);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputTriggerOnReset), TriggerOnReset);
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(ShuffleOutputValue), OutValue);
		}

		virtual FDataReferenceCollection GetInputs() const override
		{
			// This should never be called. Bind(...) is called instead. This method
			// exists as a stop-gap until the API can be deprecated and removed.
			checkNoEntry();
			return {};
		}

		virtual FDataReferenceCollection GetOutputs() const override
		{
			// This should never be called. Bind(...) is called instead. This method
			// exists as a stop-gap until the API can be deprecated and removed.
			checkNoEntry();
			return {};
		}

		void Reset(const IOperator::FResetParams& InParams)
		{
			using namespace Frontend;
#if WITH_METASOUND_DEBUG_ENVIRONMENT
			if (InParams.Environment.Contains<FString>(SourceInterface::Environment::GraphName))
			{
				GraphName = *InParams.Environment.GetValue<FString>(SourceInterface::Environment::GraphName);
			}
#endif // WITH_METASOUND_DEBUG_ENVIRONMENT
			bIsPreviewSound = InParams.Environment.GetValue<bool>(SourceInterface::Environment::IsPreview);

			// Check to see if this is a global shuffler or a local one. 
			// Global shuffler will use a namespace to opt into it.
			PrevSeedValue = *SeedValue;
			
			WeightsArray = *InputWeightsArray;

			const ArrayType& InputArrayRef = *InputArray;
			PrevArraySize = InputArrayRef.Num();
			PrevNoRepeatOrder = FMath::Clamp(*NoRepeatOrder, 0, PrevArraySize - 1);

			*OutValue = TDataTypeFactory<ElementType>::CreateAny(InParams.OperatorSettings);
			TriggerOnNext->Reset();
			TriggerOnReset->Reset();
		}

		bool UseSharedState() const
		{
			return *bEnableSharedState && bSharedStateInitialized;
		}

		void Execute()
		{
			TriggerOnNext->AdvanceBlock();
			TriggerOnReset->AdvanceBlock();

			const ArrayType& InputArrayRef = *InputArray;
			if (InputArrayRef.Num() == 0)
			{
#if WITH_METASOUND_DEBUG_ENVIRONMENT
				if (!bHasLoggedEmptyArrayWarning)
				{
					UE_LOG(LogMetaSound, Verbose, TEXT("Array Random Get: empty array input (Graph '%s')"), *GraphName);
					bHasLoggedEmptyArrayWarning = true;
				}
#endif // WITH_METASOUND_DEBUG_ENVIRONMENT
				return;
			}
			
 			TriggerReset->ExecuteBlock(
				[&](int32 StartFrame, int32 EndFrame)
				{
				},
				[this](int32 StartFrame, int32 EndFrame)
				{
					if (IsStateInitializationNeeded())
					{
						InitializeState(PrevArraySize);
					}

					if (UseSharedState())
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
					ExecuteTriggerNext(StartFrame);
				}
			);
		}

	private:
		void ExecuteTriggerNext(int32 StartFrame)
		{
			const ArrayType& InputArrayRef = *InputArray;
			int32 OutRandomIndex = INDEX_NONE;

			const bool bIsStateReinitializationNeeded = IsStateInitializationNeeded();
			const bool bIsArraySizeChanged = PrevArraySize != InputArrayRef.Num(); 
			const bool bSeedValueChanged = PrevSeedValue != *SeedValue;
			const bool bNoRepeatOrderChanged = PrevNoRepeatOrder != *NoRepeatOrder;
			const bool bWeightsArrayChanged = WeightsArray != *InputWeightsArray;

			// Update cached values if changed
			if (bIsArraySizeChanged)
			{
				PrevArraySize = InputArrayRef.Num();
			}
			if (bSeedValueChanged)
			{
				PrevSeedValue = *SeedValue;
			}
			if (bNoRepeatOrderChanged)
			{
				PrevNoRepeatOrder = *NoRepeatOrder;
			}
			if (bWeightsArrayChanged)
			{
				WeightsArray = *InputWeightsArray;
			}

			// Reinitialize state (with new values) if needed 
			if (bIsStateReinitializationNeeded)
			{
				InitializeState(PrevArraySize);
			}

			// Update other state (which was not necessarily changed by new state initialization)
			if (bSeedValueChanged)
			{
				if (UseSharedState())
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
			if (bNoRepeatOrderChanged)
			{
				if (UseSharedState())
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
			if (bWeightsArrayChanged)
			{
				if (UseSharedState())
				{
					FSharedStateRandomGetManager& RGM = FSharedStateRandomGetManager::Get();
					RGM.SetRandomWeights(SharedStateUniqueId, WeightsArray);
				}
				else
				{
					check(ArrayRandomGet.IsValid());
					ArrayRandomGet->SetRandomWeights(WeightsArray);
				}
			}

			// Get next value 
			if (UseSharedState())
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
#if WITH_METASOUND_DEBUG_ENVIRONMENT
			UE_LOG(LogMetaSound, Verbose, TEXT("Array Random Get: Index chosen: '%u'"), OutRandomIndex);
#endif // WITH_METASOUND_DEBUG_ENVIRONMENT

			// The input array size may have changed, so make sure it's wrapped into range of the input array
			*OutValue = InputArrayRef[OutRandomIndex % InputArrayRef.Num()];

			TriggerOnNext->TriggerFrame(StartFrame);
		}

		bool IsStateInitializationNeeded()
		{
			const ArrayType& InputArrayRef = *InputArray;
			const bool bIsArrayNonEmpty = InputArrayRef.Num() != 0; // Skip reinit if the array is empty because it represents an invalid state for this node.
			const bool bIsArraySizeChanged = PrevArraySize != InputArrayRef.Num(); // Need to reinit for array size changes. 
			const bool bIsSharedStateEnablementInconsistent = (*bEnableSharedState != bSharedStateInitialized); // Need to reinit if the shared state enablement has been updated.
			const bool bIsNonSharedStateInitializationNeeded = !*bEnableSharedState && !ArrayRandomGet.IsValid(); // For the first initialization of the non shared state random get (bIsSharedStateEnablementInconsistent will take care of that for shared state)

			return (bIsArrayNonEmpty && (bIsArraySizeChanged || bIsSharedStateEnablementInconsistent || bIsNonSharedStateInitializationNeeded));
		}

		void InitializeState(int32 InArraySize)
		{
			bSharedStateInitialized = false;
			if (InArraySize > 0)
			{
				if (*bEnableSharedState)
				{
					// Get the environment variable for the unique ID of the sound
					check(SharedStateUniqueId.IsValid());
					FSharedStateRandomGetManager& RGM = FSharedStateRandomGetManager::Get();

					InitSharedStateArgs Args;
					Args.SharedStateId = SharedStateUniqueId;
					Args.Seed = PrevSeedValue;
					Args.NumElements = PrevArraySize;
					Args.NoRepeatOrder = PrevNoRepeatOrder;
					Args.bIsPreviewSound = bIsPreviewSound;
					Args.Weights = WeightsArray;

					RGM.InitSharedState(Args);

					bSharedStateInitialized = true;
				}
				else
				{
					ArrayRandomGet = MakeUnique<FArrayRandomGet>(PrevSeedValue, PrevArraySize, WeightsArray, PrevNoRepeatOrder);
				}
			}
			else
			{
				ArrayRandomGet = MakeUnique<FArrayRandomGet>();
#if WITH_METASOUND_DEBUG_ENVIRONMENT
 				UE_LOG(LogMetaSound, Verbose, TEXT("Array Random Get: Can't retrieve random elements from an empty array in graph '%s'"), *GraphName);
#endif // WITH_METASOUND_DEBUG_ENVIRONMENT
			}
		}

		// Inputs
		FTriggerReadRef TriggerNext;
		FTriggerReadRef TriggerReset;
		FArrayDataReadReference InputArray;
		TDataReadReference<WeightsArrayType> InputWeightsArray;
		FInt32ReadRef SeedValue;
		FInt32ReadRef NoRepeatOrder;

		// Outputs
		FTriggerWriteRef TriggerOnNext;
		FTriggerWriteRef TriggerOnReset;
		TDataWriteReference<ElementType> OutValue;

#if WITH_METASOUND_DEBUG_ENVIRONMENT
		FString GraphName;
		bool bHasLoggedEmptyArrayWarning = false;
#endif // WITH_METASOUND_DEBUG_ENVIRONMENT

		// Data
		TUniquePtr<FArrayRandomGet> ArrayRandomGet;
		TArray<float> WeightsArray;
		int32 PrevSeedValue = INDEX_NONE;
		int32 PrevNoRepeatOrder = INDEX_NONE;
		FGuid SharedStateUniqueId;
		int32 PrevArraySize = 0;
		bool bIsPreviewSound = false;
		FBoolReadRef bEnableSharedState;
		bool bSharedStateInitialized = false;
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
