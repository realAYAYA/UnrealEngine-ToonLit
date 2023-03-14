// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/MetasoundFrontendSourceInterface.h"
#include "Internationalization/Text.h"
#include "MetasoundArrayNodes.h"
#include "MetasoundBuilderInterface.h"
#include "MetasoundDataFactory.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundLog.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundTrigger.h"
#include "MetasoundVertex.h"
#include "Misc/ScopeLock.h"

#include <type_traits>

#define LOCTEXT_NAMESPACE "MetasoundFrontend"


namespace Metasound
{
	/** Shuffle Node Vertex Names */
	namespace ArrayNodeShuffleVertexNames
	{
		METASOUND_PARAM(InputTriggerNext, "Next", "Trigger to get the next value in the shuffled array.")
		METASOUND_PARAM(InputTriggerShuffle, "Shuffle", "Trigger to shuffle the array manually.")
		METASOUND_PARAM(InputTriggerReset, "Reset Seed", "Trigger to reset the random seed stream of the shuffle node.")
		METASOUND_PARAM(InputShuffleArray, "In Array", "Input Array.")
		METASOUND_PARAM(InputShuffleSeed, "Seed", "Seed to use for the the random shuffle.")
		METASOUND_PARAM(InputAutoShuffle, "Auto Shuffle", "Set to true to automatically shuffle when the array has been read.")
		METASOUND_PARAM(InputShuffleEnableSharedState, "Enable Shared State", "Set to enabled shared state across instances of this metasound.")

		METASOUND_PARAM(OutputTriggerOnNext, "On Next", "Triggers when the \"Next\" input is triggered.")
		METASOUND_PARAM(OutputTriggerOnShuffle, "On Shuffle", "Triggers when the \"Shuffle\" input is triggered or if the array is auto-shuffled.")
		METASOUND_PARAM(OutputTriggerOnResetSeed, "On Reset Seed", "Triggers when the \"Reset Seed\" input is triggered.")
		METASOUND_PARAM(OutputShuffledValue, "Value", "Value of the current shuffled element.")
	}

	class METASOUNDFRONTEND_API FArrayIndexShuffler
	{
	public:
		FArrayIndexShuffler() = default;
		FArrayIndexShuffler(int32 InSeed, int32 MaxIndices);

		void Init(int32 InSeed, int32 MaxIndices);
		void SetSeed(int32 InSeed);
		void ResetSeed();

		// Returns the next value in the array indices. Returns true if the array was re-shuffled automatically.
		bool NextValue(bool bAutoShuffle, int32& OutIndex);

		// Shuffle the array with the given max indices
		void ShuffleArray();

	private:
		// Helper function to swap the current index with a random index
		void RandomSwap(int32 InCurrentIndex, int32 InStartIndex, int32 InEndIndex);

		// The current index into the array of indicies (wraps between 0 and ShuffleIndices.Num())
		int32 CurrentIndex = 0;

		// The previously returned value. Used to avoid repeating the last value on shuffle.
		int32 PrevValue = INDEX_NONE;

		// Array of indices (in order 0 to Num), shuffled
		TArray<int32> ShuffleIndices;

		// Random stream to use to randomize the shuffling
		FRandomStream RandomStream;
	};

	class FSharedStateShuffleManager
	{
	public:
		static FSharedStateShuffleManager& Get()
		{
			static FSharedStateShuffleManager GSM;
			return GSM;
		}

		void InitSharedState(uint32 InSharedStateId, int32 InSeed, int32 InNumElements)
		{
			FScopeLock Lock(&CritSect);

			if (!Shufflers.Contains(InSharedStateId))
			{
				Shufflers.Add(InSharedStateId, MakeUnique<FArrayIndexShuffler>(InSeed, InNumElements));
			}
		}

		bool NextValue(uint32 InSharedStateId, bool bAutoShuffle, int32& OutIndex)
		{
			FScopeLock Lock(&CritSect);
			TUniquePtr<FArrayIndexShuffler>* Shuffler = Shufflers.Find(InSharedStateId);
			return (*Shuffler)->NextValue(bAutoShuffle, OutIndex);
		}

		void SetSeed(uint32 InSharedStateId, int32 InSeed)
		{
			FScopeLock Lock(&CritSect);
			TUniquePtr<FArrayIndexShuffler>* Shuffler = Shufflers.Find(InSharedStateId);
			(*Shuffler)->SetSeed(InSeed);
		}

		void ResetSeed(uint32 InSharedStateId)
		{
			FScopeLock Lock(&CritSect);
			TUniquePtr<FArrayIndexShuffler>* Shuffler = Shufflers.Find(InSharedStateId);
			(*Shuffler)->ResetSeed();
		}

		void ShuffleArray(uint32 InSharedStateId)
		{
			FScopeLock Lock(&CritSect);
			TUniquePtr<FArrayIndexShuffler>* Shuffler = Shufflers.Find(InSharedStateId);
			(*Shuffler)->ShuffleArray();
		}

	private:
		FSharedStateShuffleManager() = default;
		~FSharedStateShuffleManager() = default;

		FCriticalSection CritSect;

		TMap<uint32, TUniquePtr<FArrayIndexShuffler>> Shufflers;	
	};

	/** TArrayShuffleOperator shuffles an array on trigger and outputs values sequentially on "next". It avoids repeating shuffled elements and supports auto-shuffling.*/
	template<typename ArrayType>
	class TArrayShuffleOperator : public TExecutableOperator<TArrayShuffleOperator<ArrayType>>
	{
	public:
		using FArrayDataReadReference = TDataReadReference<ArrayType>;
		using ElementType = typename MetasoundArrayNodesPrivate::TArrayElementType<ArrayType>::Type;
		using FElementTypeWriteReference = TDataWriteReference<ElementType>;

		static const FVertexInterface& GetDefaultInterface()
		{
			using namespace ArrayNodeShuffleVertexNames;

			static const FVertexInterface DefaultInterface(
				FInputVertexInterface(
					TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerNext)),
					TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerShuffle)),
					TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTriggerReset)),
					TInputDataVertex<ArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputShuffleArray)),
					TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputShuffleSeed), -1),
					TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAutoShuffle), true),
					TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputShuffleEnableSharedState), false)
					),
				FOutputVertexInterface(
					TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerOnNext)),
					TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerOnShuffle)),
					TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputTriggerOnResetSeed)),
					TOutputDataVertex<ElementType>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputShuffledValue))
				)
			);

			return DefaultInterface;
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
			{
				const FName DataTypeName = GetMetasoundDataTypeName<ArrayType>();
				const FName OperatorName = "Shuffle";
				const FText NodeDisplayName = METASOUND_LOCTEXT_FORMAT("ArrayOpArrayShuffleDisplayNamePattern", "Shuffle ({0})", GetMetasoundDataTypeDisplayText<ArrayType>());
				const FText NodeDescription = METASOUND_LOCTEXT("ArrayOpArrayShuffleDescription", "Output next element of a shuffled array on trigger.");
				const FVertexInterface NodeInterface = GetDefaultInterface();

				return MetasoundArrayNodesPrivate::CreateArrayNodeClassMetadata(DataTypeName, OperatorName, NodeDisplayName, NodeDescription, NodeInterface);
			};

			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();

			return Metadata;
		}

		static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, TArray<TUniquePtr<IOperatorBuildError>>& OutErrors)
		{
			using namespace ArrayNodeShuffleVertexNames;
			using namespace MetasoundArrayNodesPrivate;

			const FInputVertexInterface& Inputs = InParams.Node.GetVertexInterface().GetInputInterface();

			TDataReadReference<FTrigger> InTriggerNext = InParams.InputDataReferences.GetDataReadReferenceOrConstructWithVertexDefault<FTrigger>(Inputs, METASOUND_GET_PARAM_NAME(InputTriggerNext), InParams.OperatorSettings);
			TDataReadReference<FTrigger> InTriggerShuffle = InParams.InputDataReferences.GetDataReadReferenceOrConstructWithVertexDefault<FTrigger>(Inputs, METASOUND_GET_PARAM_NAME(InputTriggerShuffle), InParams.OperatorSettings);
			TDataReadReference<FTrigger> InTriggerReset = InParams.InputDataReferences.GetDataReadReferenceOrConstructWithVertexDefault<FTrigger>(Inputs, METASOUND_GET_PARAM_NAME(InputTriggerReset), InParams.OperatorSettings);
			FArrayDataReadReference InInputArray = InParams.InputDataReferences.GetDataReadReferenceOrConstructWithVertexDefault<ArrayType>(Inputs, METASOUND_GET_PARAM_NAME(InputShuffleArray), InParams.OperatorSettings);
			TDataReadReference<int32> InSeedValue = InParams.InputDataReferences.GetDataReadReferenceOrConstructWithVertexDefault<int32>(Inputs, METASOUND_GET_PARAM_NAME(InputShuffleSeed), InParams.OperatorSettings);
			TDataReadReference<bool> bInAutoShuffle = InParams.InputDataReferences.GetDataReadReferenceOrConstructWithVertexDefault<bool>(Inputs, METASOUND_GET_PARAM_NAME(InputAutoShuffle), InParams.OperatorSettings);
			TDataReadReference<bool> bInEnableSharedState = InParams.InputDataReferences.GetDataReadReferenceOrConstructWithVertexDefault<bool>(Inputs, METASOUND_GET_PARAM_NAME(InputShuffleEnableSharedState), InParams.OperatorSettings);

			return MakeUnique<TArrayShuffleOperator>(InParams, InTriggerNext, InTriggerShuffle, InTriggerReset, InInputArray, InSeedValue, bInAutoShuffle, bInEnableSharedState);
		}

		TArrayShuffleOperator(
			const FCreateOperatorParams& InParams,
			const TDataReadReference<FTrigger>& InTriggerNext, 
			const TDataReadReference<FTrigger>& InTriggerShuffle,
			const TDataReadReference<FTrigger>& InTriggerReset,
			const FArrayDataReadReference& InInputArray,
			const TDataReadReference<int32>& InSeedValue,
			const TDataReadReference<bool>& bInAutoShuffle, 
			const TDataReadReference<bool>& bInEnableSharedState)
			: TriggerNext(InTriggerNext)
			, TriggerShuffle(InTriggerShuffle)
			, TriggerReset(InTriggerReset)
			, InputArray(InInputArray)
			, SeedValue(InSeedValue)
			, bAutoShuffle(bInAutoShuffle)
			, bEnableSharedState(bInEnableSharedState)
			, TriggerOnNext(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
			, TriggerOnShuffle(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
			, TriggerOnReset(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
			, OutValue(TDataWriteReferenceFactory<ElementType>::CreateAny(InParams.OperatorSettings))
		{
			using namespace Frontend;

			// Check to see if this is a global shuffler or a local one. 
			// Global shuffler will use a namespace to opt into it.
			PrevSeedValue = *SeedValue;

			const ArrayType& InputArrayRef = *InputArray;
			int32 ArraySize = InputArrayRef.Num();

			if (ArraySize > 0)
			{
				if (*bEnableSharedState)
				{
					// Get the environment variable for the unique ID of the sound
					SharedStateUniqueId = InParams.Environment.GetValue<uint32>(SourceInterface::Environment::SoundUniqueID);
					check(SharedStateUniqueId != INDEX_NONE);

					FSharedStateShuffleManager& SM = FSharedStateShuffleManager::Get();
					SM.InitSharedState(SharedStateUniqueId, PrevSeedValue, ArraySize);
				}
				else
				{
					ArrayIndexShuffler = MakeUnique<FArrayIndexShuffler>(PrevSeedValue, ArraySize);
				}
			}
			else 
			{
				UE_LOG(LogMetaSound, Error, TEXT("Array Shuffle: Can't shuffle an empty array"));
			}
		}

		virtual ~TArrayShuffleOperator() = default;

		virtual FDataReferenceCollection GetInputs() const override
		{
			using namespace ArrayNodeShuffleVertexNames;

			FDataReferenceCollection Inputs;
			Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTriggerNext), TriggerNext);
			Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTriggerShuffle), TriggerShuffle);
			Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTriggerReset), TriggerReset);
			Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputShuffleArray), InputArray);
			Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputShuffleSeed), SeedValue);
			Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputAutoShuffle), bAutoShuffle);
			Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputShuffleEnableSharedState), bEnableSharedState);

			return Inputs;
		}

		virtual FDataReferenceCollection GetOutputs() const override
		{
			using namespace ArrayNodeShuffleVertexNames;

			FDataReferenceCollection Outputs;
			Outputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputTriggerOnNext), TriggerOnNext);
			Outputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputTriggerOnShuffle), TriggerOnShuffle);
			Outputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputTriggerOnResetSeed), TriggerOnReset);
			Outputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputShuffledValue), OutValue);

			return Outputs;
		}

		void Execute()
		{
			TriggerOnNext->AdvanceBlock();
			TriggerOnShuffle->AdvanceBlock();
			TriggerOnReset->AdvanceBlock();

			const ArrayType& InputArrayRef = *InputArray;
 
			// Check for a seed change
			if (PrevSeedValue != *SeedValue)
			{
				PrevSeedValue = *SeedValue;

				if (SharedStateUniqueId != INDEX_NONE)
				{
					FSharedStateShuffleManager& SM = FSharedStateShuffleManager::Get();
					SM.SetSeed(SharedStateUniqueId, PrevSeedValue);
				}
				else
				{
					check(ArrayIndexShuffler.IsValid());
					ArrayIndexShuffler->SetSeed(PrevSeedValue);
				}
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
					if (SharedStateUniqueId != INDEX_NONE)
					{
						FSharedStateShuffleManager& SM = FSharedStateShuffleManager::Get();
						SM.ResetSeed(SharedStateUniqueId);
					}
					else
					{
						check(ArrayIndexShuffler.IsValid());
						ArrayIndexShuffler->ResetSeed();
					}
					TriggerOnReset->TriggerFrame(StartFrame);
				}
			);

			TriggerShuffle->ExecuteBlock(
				[&](int32 StartFrame, int32 EndFrame)
				{
				},
				[this](int32 StartFrame, int32 EndFrame)
				{
					if (SharedStateUniqueId != INDEX_NONE)
					{
						FSharedStateShuffleManager& SM = FSharedStateShuffleManager::Get();
						SM.ShuffleArray(SharedStateUniqueId);
					}
					else
					{
						check(ArrayIndexShuffler.IsValid());
						ArrayIndexShuffler->ShuffleArray();
					}
					TriggerOnShuffle->TriggerFrame(StartFrame);
				}
			);

			TriggerNext->ExecuteBlock(
				[&](int32 StartFrame, int32 EndFrame)
				{
				},
				[this](int32 StartFrame, int32 EndFrame)
				{
					const ArrayType& InputArrayRef = *InputArray;

					bool bShuffleTriggered = false;
					int32 OutShuffleIndex = INDEX_NONE;

					if (SharedStateUniqueId != INDEX_NONE)
					{
						FSharedStateShuffleManager& SM = FSharedStateShuffleManager::Get();
						bShuffleTriggered = SM.NextValue(SharedStateUniqueId, *bAutoShuffle, OutShuffleIndex);
					}
					else
					{
						check(ArrayIndexShuffler.IsValid());
						bShuffleTriggered = ArrayIndexShuffler->NextValue(*bAutoShuffle, OutShuffleIndex);
					}

					check(OutShuffleIndex != INDEX_NONE);

					// The input array size may have changed, so make sure it's wrapped into range of the input array
					*OutValue = InputArrayRef[OutShuffleIndex % InputArrayRef.Num()];

					TriggerOnNext->TriggerFrame(StartFrame);

					// Trigger out if the array was auto-shuffled
					if (bShuffleTriggered)
					{
						TriggerOnShuffle->TriggerFrame(StartFrame);
					}
				}
			);
		}

	private:

		// Inputs
		TDataReadReference<FTrigger> TriggerNext;
		TDataReadReference<FTrigger> TriggerShuffle;
		TDataReadReference<FTrigger> TriggerReset;
		FArrayDataReadReference InputArray;
		TDataReadReference<int32> SeedValue;
		TDataReadReference<bool> bAutoShuffle;
		TDataReadReference<bool> bEnableSharedState;

		// Outputs
		TDataWriteReference<FTrigger> TriggerOnNext;
		TDataWriteReference<FTrigger> TriggerOnShuffle;
		TDataWriteReference<FTrigger> TriggerOnReset;
		TDataWriteReference<ElementType> OutValue;

		// Data
		TUniquePtr<FArrayIndexShuffler> ArrayIndexShuffler;
		int32 PrevSeedValue = INDEX_NONE;
		uint32 SharedStateUniqueId = INDEX_NONE;
	};

	template<typename ArrayType>
	class TArrayShuffleNode : public FNodeFacade
	{
	public:
		TArrayShuffleNode(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<TArrayShuffleOperator<ArrayType>>())
		{
		}

		virtual ~TArrayShuffleNode() = default;
	};
}

#undef LOCTEXT_NAMESPACE
