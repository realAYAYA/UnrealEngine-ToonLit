// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Analysis/MetasoundFrontendGraphAnalyzer.h"
#include "Async/Async.h"
#include "DSP/Dsp.h"
#include "MetasoundAudioFormats.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFrontendController.h"
#include "MetasoundGraphOperator.h"
#include "MetasoundOperatorBuilder.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundPrimitives.h"
#include "MetasoundRouter.h"
#include "MetasoundTrigger.h"
#include "MetasoundVertex.h"
#include "MetasoundVertexData.h"
#include "Sound/SoundGenerator.h"
#include "Tickable.h"


namespace Metasound
{
	// Struct needed for building the metasound graph
	struct METASOUNDGENERATOR_API FMetasoundGeneratorInitParams
	{
		FOperatorSettings OperatorSettings;
		FOperatorBuilderSettings BuilderSettings;
		TSharedPtr<const IGraph, ESPMode::ThreadSafe> Graph;
		FMetasoundEnvironment Environment;
		FString MetaSoundName;
		TArray<FVertexName> AudioOutputNames;
		TArray<FAudioParameter> DefaultParameters;

		void Release();
	};

	struct FMetasoundGeneratorData
	{
		FOperatorSettings OperatorSettings;
		TUniquePtr<IOperator> GraphOperator;
		TUniquePtr<Frontend::FGraphAnalyzer> GraphAnalyzer;
		TArray<TDataReadReference<FAudioBuffer>> OutputBuffers;
		FTriggerWriteRef TriggerOnPlayRef;
		FTriggerReadRef TriggerOnFinishRef;
	};

	class FMetasoundGenerator;

	class FAsyncMetaSoundBuilder : public FNonAbandonableTask
	{
	public:
		FAsyncMetaSoundBuilder(FMetasoundGenerator* InGenerator, FMetasoundGeneratorInitParams&& InInitParams, bool bInTriggerGenerator);

		~FAsyncMetaSoundBuilder() = default;

		void DoWork();

		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncMetaSoundBuilder, STATGROUP_ThreadPoolAsyncTasks);
		}

	private:
		TUniquePtr<IOperator> BuildGraphOperator(TArray<FAudioParameter>&& InParameters, FBuildResults& OutBuildResults) const;
		TUniquePtr<Frontend::FGraphAnalyzer> BuildGraphAnalyzer(TMap<FGuid, FDataReferenceCollection>&& InInternalDataReferences) const;
		void LogBuildErrors(const FBuildResults& InBuildResults) const;
		TArray<FAudioBufferReadRef> FindOutputAudioBuffers(const FVertexInterfaceData& InVertexData) const;

		FMetasoundGenerator* Generator;
		FMetasoundGeneratorInitParams InitParams;
		bool bTriggerGenerator;
	};

	/** FMetasoundGenerator generates audio from a given metasound IOperator
	 * which produces a multichannel audio output.
	 */
	class METASOUNDGENERATOR_API FMetasoundGenerator : public ISoundGenerator
	{
	public:
		using FOperatorUniquePtr = TUniquePtr<Metasound::IOperator>;
		using FAudioBufferReadRef = Metasound::FAudioBufferReadRef;

		FString MetasoundName;

		/** Create the generator with a graph operator and an output audio reference.
		 *
		 * @param InGraphOperator - Unique pointer to the IOperator which executes the entire graph.
		 * @param InGraphOutputAudioRef - Read reference to the audio buffer filled by the InGraphOperator.
		 */
		FMetasoundGenerator(FMetasoundGeneratorInitParams&& InParams);

		virtual ~FMetasoundGenerator();

		/** Set the value of a graph's input data using the assignment operator.
		 *
		 * @param InName - The name of the graph's input data reference.
		 * @param InData - The value to assign.
		 */
		template<typename DataType>
		void SetInputValue(const FString& InName, DataType InData)
		{
			typedef TDataWriteReference< typename TDecay<DataType>::Type > FDataWriteRef;

			const FDataReferenceCollection& InputCollection = RootExecuter.GetInputs();

			// Check if an input data reference with the given name and type exist in the graph.
			bool bContainsWriteReference = InputCollection.ContainsDataWriteReference< typename TDecay<DataType>::Type >(InName);
			if (ensureMsgf(bContainsWriteReference, TEXT("Operator does not contain write reference name \"%s\" of type \"%s\""), *InName, *GetMetasoundDataTypeName<DataType>().ToString()))
			{
				FDataWriteRef WriteRef = InputCollection.GetDataWriteReference<DataType>(InName);

				// call assignment operator of DataType.
				*WriteRef = InData;
			}
		}

		/** Apply a function to the graph's input data.
		 *
		 * @param InName - The name of the graph's input data reference.
		 * @param InFunc - A function which takes the DataType as an input.
		 */ 
		template<typename DataType>
		void ApplyToInputValue(const FString& InName, TUniqueFunction<void(DataType&)> InFunc)
		{
			// Get decayed type as InFunc could take a const qualified type.
			typedef TDataWriteReference< typename TDecay<DataType>::Type > FDataWriteRef;

			const FDataReferenceCollection& InputCollection = RootExecuter.GetInputs();

			// Check if an input data reference with the given name and type exists in the graph.
			bool bContainsWriteReference = InputCollection.ContainsDataWriteReference< typename TDecay<DataType>::Type >(InName);
			if (ensureMsgf(bContainsWriteReference, TEXT("Operator does not contain write reference name \"%s\" of type \"%s\""), *InName, *GetMetasoundDataTypeName<DataType>().ToString()))
			{
				FDataWriteRef WriteRef = InputCollection.GetDataWriteReference<DataType>(InName);

				// Apply function to DataType
				InFunc(*WriteRef);
			}
		}

		/** Return the number of audio channels. */
		int32 GetNumChannels() const;

		//~ Begin FSoundGenerator
		virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;
		int32 GetDesiredNumSamplesToRenderPerCallback() const override;
		bool IsFinished() const override;
		//~ End FSoundGenerator
		
		/** Update the current graph operator with a new graph operator. The number of channels
		 * of InGraphOutputAudioRef must match the existing number of channels reported by
		 * GetNumChannels() in order for this function to successfully replace the graph operator.
		 *
		 * @param InData - Metasound data of built graph.
		 * @param bTriggerGraph - If true, "OnPlay" will be triggered on the new graph.
		 */
		void SetPendingGraph(FMetasoundGeneratorData&& InData, bool bTriggerGraph);
		void SetPendingGraphBuildFailed();

	private:
		bool UpdateGraphIfPending();

		// Internal set graph after checking compatibility.
		void SetGraph(TUniquePtr<FMetasoundGeneratorData>&& InData, bool bTriggerGraph);

		// Fill OutAudio with data in InBuffer, up to maximum number of samples.
		// Returns the number of samples used.
		int32 FillWithBuffer(const Audio::FAlignedFloatBuffer& InBuffer, float* OutAudio, int32 MaxNumOutputSamples);

		// Metasound creates deinterleaved audio while sound generator requires interleaved audio.
		void InterleaveGeneratedAudio();
		
		FExecuter RootExecuter;

		bool bIsGraphBuilding;
		bool bIsFinishTriggered;
		bool bIsFinished;

		int32 FinishSample = INDEX_NONE;
		int32 NumChannels;
		int32 NumFramesPerExecute;
		int32 NumSamplesPerExecute;

		TArray<FAudioBufferReadRef> GraphOutputAudio;

		// Triggered when metasound is played
		FTriggerWriteRef OnPlayTriggerRef;

		// Triggered when metasound is finished
		FTriggerReadRef OnFinishedTriggerRef;

		Audio::FAlignedFloatBuffer InterleavedAudioBuffer;

		Audio::FAlignedFloatBuffer OverflowBuffer;

		typedef FAsyncTask<FAsyncMetaSoundBuilder> FBuilderTask;
		TUniquePtr<FBuilderTask> BuilderTask;

		FCriticalSection PendingGraphMutex;
		TUniquePtr<FMetasoundGeneratorData> PendingGraphData;
		bool bPendingGraphTrigger;
		bool bIsNewGraphPending;
		bool bIsWaitingForFirstGraph;

		TUniquePtr<Frontend::FGraphAnalyzer> GraphAnalyzer;
	};
}
