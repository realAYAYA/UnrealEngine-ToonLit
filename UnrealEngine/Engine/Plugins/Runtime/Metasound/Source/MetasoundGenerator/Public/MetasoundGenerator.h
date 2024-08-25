// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetasoundExecutableOperator.h"
#include "MetasoundGraphOperator.h"
#include "MetasoundInstanceCounter.h"
#include "MetasoundOperatorBuilder.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundParameterPack.h"
#include "MetasoundRouter.h"
#include "MetasoundTrigger.h"
#include "MetasoundVertex.h"
#include "MetasoundVertexData.h"

#include "Analysis/MetasoundFrontendGraphAnalyzer.h"
#include "Async/AsyncWork.h"
#include "Containers/MpscQueue.h"
#include "Containers/SpscQueue.h"
#include "Sound/SoundGenerator.h"

#ifndef ENABLE_METASOUND_GENERATOR_RENDER_TIMING
#define ENABLE_METASOUND_GENERATOR_RENDER_TIMING WITH_EDITOR
#endif // ifndef ENABLE_METASOUND_GENERATOR_RENDER_TIMING

#ifndef ENABLE_METASOUND_GENERATOR_INSTANCE_COUNTING
#define ENABLE_METASOUND_GENERATOR_INSTANCE_COUNTING UE_TRACE_ENABLED && !UE_BUILD_SHIPPING
#endif // ifndef ENABLE_METASOUND_GENERATOR_INSTANCE_COUNTING
namespace Metasound
{
	namespace DynamicGraph
	{
		class IDynamicOperatorTransform;
	}

	namespace MetasoundGeneratorPrivate
	{
		struct FRenderTimer;

		struct FParameterSetter
		{
			Frontend::FLiteralAssignmentFunction Assign;
			FAnyDataReference DataReference;
		};

		// In order to use FName as a key to a TSortedMap you have to explicitly 
		// choose which comparison implementation you want to use.  Declaring the
		// type here helps minimize the confusion over why there are so many 
		// template arguments. 
		using FParameterSetterSortedMap  = TSortedMap<FName, FParameterSetter, FDefaultAllocator, FNameFastLess>;

		// A struct that provides a method of pushing "raw" data from a parameter pack into a specific metasound input node.
		struct FParameterPackSetter
		{
			FName DataType;
			void* Destination;
			const Frontend::IParameterAssignmentFunction& Setter;
			FParameterPackSetter(FName InDataType, void* InDestination, const Frontend::IParameterAssignmentFunction& InSetter)
				: DataType(InDataType)
				, Destination(InDestination)
				, Setter(InSetter)
			{}
			void SetParameterWithPayload(const void* ParameterPayload) const
			{
				Setter(ParameterPayload, Destination);
			}
		};

		struct FMetasoundGeneratorData
		{
			FOperatorSettings OperatorSettings;
			TUniquePtr<IOperator> GraphOperator;
			FVertexInterfaceData VertexInterfaceData;
			FParameterSetterSortedMap ParameterSetters;
			TMap<FName, FParameterPackSetter> ParameterPackSetters;
			TUniquePtr<Frontend::FGraphAnalyzer> GraphAnalyzer;
			TArray<TDataReadReference<FAudioBuffer>> OutputBuffers;
			FTriggerWriteRef TriggerOnPlayRef;
			FTriggerReadRef TriggerOnFinishRef;
		};
	}

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
		bool bBuildSynchronous = false;
		TSharedPtr<TSpscQueue<FMetaSoundParameterTransmitter::FParameter>> DataChannel;

		static void Reset(FMetasoundGeneratorInitParams& InParams);
	};

	DECLARE_TS_MULTICAST_DELEGATE(FOnSetGraph);

	class METASOUNDGENERATOR_API FMetasoundGenerator : public ISoundGenerator
	{
	public:
		using FOperatorUniquePtr = TUniquePtr<Metasound::IOperator>;
		using FAudioBufferReadRef = Metasound::FAudioBufferReadRef;

		FString MetasoundName;

		const FOperatorSettings OperatorSettings;

		/** Create the generator with a graph operator and an output audio reference.
		 *
		 * @param InParams - The generator initialization parameters
		 */

		explicit FMetasoundGenerator(const FOperatorSettings& InOperatorSettings);

		virtual ~FMetasoundGenerator();

		/** Set the value of a graph's input data using the assignment operator.
		 *
		 * @param InName - The name of the graph's input data reference.
		 * @param InData - The value to assign.
		 */
		template<typename DataType>
		void SetInputValue(const FVertexName& InName, DataType InData)
		{
			if (const FAnyDataReference* Ref = VertexInterfaceData.GetInputs().FindDataReference(InName))
			{
				*(Ref->GetDataWriteReference<typename TDecay<DataType>::Type>()) = InData;
			}
		}

		/** Apply a function to the graph's input data.
		 *
		 * @param InName - The name of the graph's input data reference.
		 * @param InFunc - A function which takes the DataType as an input.
		 */ 
		template<typename DataType>
		void ApplyToInputValue(const FVertexName& InName, TFunctionRef<void(DataType&)> InFunc)
		{
			if (const FAnyDataReference* Ref = VertexInterfaceData.GetInputs().FindDataReference(InName))
			{
				InFunc(*(Ref->GetDataWriteReference<typename TDecay<DataType>::Type>()));
			}
		}

		void QueueParameterPack(TSharedPtr<FMetasoundParameterPackStorage> ParameterPack);

		/**
		 * Get a write reference to one of the generator's inputs, if it exists.
		 * NOTE: This reference is only safe to use immediately on the same thread that this generator's
		 * OnGenerateAudio() is called.
		 *
		 * @tparam DataType - The expected data type of the input
		 * @param InputName - The user-defined name of the input
		 */
		template<typename DataType>
		TOptional<TDataWriteReference<DataType>> GetInputWriteReference(const FVertexName InputName)
		{
			TOptional<TDataWriteReference<DataType>> WriteRef;
			
			if (const FAnyDataReference* Ref = VertexInterfaceData.GetInputs().FindDataReference(InputName))
			{
				WriteRef = Ref->GetDataWriteReference<typename TDecay<DataType>::Type>();
			}
			
			return WriteRef;
		}
		
		/**
		 * Get a read reference to one of the generator's outputs, if it exists.
		 * NOTE: This reference is only safe to use immediately on the same thread that this generator's
		 * OnGenerateAudio() is called.
		 *
		 * @tparam DataType - The expected data type of the output
		 * @param OutputName - The user-defined name of the output
		 */
		template<typename DataType>
		TOptional<TDataReadReference<DataType>> GetOutputReadReference(const FVertexName OutputName)
		{
			TOptional<TDataReadReference<DataType>> ReadRef;

			if (const FAnyDataReference* Ref = VertexInterfaceData.GetOutputs().FindDataReference(OutputName))
			{
				ReadRef = Ref->GetDataReadReference<typename TDecay<DataType>::Type>();
			}
			
			return ReadRef;
		}

		DECLARE_TS_MULTICAST_DELEGATE_OneParam(FOnVertexInterfaceDataUpdated, FVertexInterfaceData);
		FOnVertexInterfaceDataUpdated OnVertexInterfaceDataUpdated;

		/**
		 * Add a vertex analyzer for a named output with the given address info.
		 *
		 * @param AnalyzerAddress - Address information for the analyzer
		 */
		void AddOutputVertexAnalyzer(const Frontend::FAnalyzerAddress& AnalyzerAddress);
		
		/**
		 * Remove a vertex analyzer for a named output
		 *
		 * @param AnalyzerAddress - Address information for the analyzer
		 */
		void RemoveOutputVertexAnalyzer(const Frontend::FAnalyzerAddress& AnalyzerAddress);
		
		DECLARE_TS_MULTICAST_DELEGATE_FourParams(FOnOutputChanged, FName, FName, FName, TSharedPtr<IOutputStorage>);
		FOnOutputChanged OnOutputChanged;
		
		/** Return the number of audio channels. */
		int32 GetNumChannels() const;

		//~ Begin FSoundGenerator
		virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;
		int32 GetDesiredNumSamplesToRenderPerCallback() const override;
		bool IsFinished() const override;
		//~ End FSoundGenerator

		/** Enables the performance timing of the metasound rendering process. You
		 * must call this before "GetCPUCoreUtilization" or the results will
		 * always be 0.0.
		 */
		void EnableRuntimeRenderTiming(bool Enable) { bDoRuntimeRenderTiming = Enable; }

		/** Fraction of a single CPU core used to render audio on a scale of 0.0 to 1.0 */
		double GetCPUCoreUtilization() const;


		// Called when a new graph has been "compiled" and set up as this generator's graph.
		// Note: We don't allow direct assignment to the FOnSetGraph delegate
		// because we want to give the Delegate an initial immediate callback if the generator 
		// already has a graph. 
		FDelegateHandle AddGraphSetCallback(FOnSetGraph::FDelegate&& Delegate);
		bool RemoveGraphSetCallback(const FDelegateHandle& Handle);

		// Enqueues a command for this generator to execute when its next buffer is
		// requested by the mixer.  Enqueued commands are executed before OnGenerateAudio,
		// and on the same thread.  They can safely access generator state.
		void OnNextBuffer(TUniqueFunction<void(FMetasoundGenerator&)> Command)
		{
			SynthCommand([this, Command = MoveTemp(Command)]() { Command(*this); });
		}

	protected:

		void InitBase(const FMetasoundGeneratorInitParams& InInitParams);


		/** SetGraph directly sets graph. Callers must ensure that no race conditions exist. */
		void SetGraph(TUniquePtr<MetasoundGeneratorPrivate::FMetasoundGeneratorData>&& InData, bool bTriggerGraph);


		virtual TUniquePtr<IOperator> ReleaseGraphOperator();
		FInputVertexInterfaceData ReleaseInputVertexData();
#if ENABLE_METASOUND_GENERATOR_INSTANCE_COUNTING
		FConcurrentInstanceCounter InstanceCounter; 
#endif // if ENABLE_METASOUND_GENERATOR_INSTANCE_COUNTING

		/** Release the graph operator and remove any references to data owned by
		 * the graph operator.
		 */
		void ClearGraph();
		bool UpdateGraphIfPending();

		std::atomic<bool> bVertexInterfaceHasChanged{ false };

	private:

		friend class FAsyncMetaSoundBuilderBase;

		void SetPendingGraphBuildFailed();

		/** Update the current graph operator with a new graph operator. The number of channels
		 * of InGraphOutputAudioRef must match the existing number of channels reported by
		 * GetNumChannels() in order for this function to successfully replace the graph operator.
		 *
		 * @param InData - Metasound data of built graph.
		 * @param bTriggerGraph - If true, "OnPlay" will be triggered on the new graph.
		 */
		void SetPendingGraph(MetasoundGeneratorPrivate::FMetasoundGeneratorData&& InData, bool bTriggerGraph);

		// Fill OutAudio with data in InBuffer, up to maximum number of samples.
		// Returns the number of samples used.
		int32 FillWithBuffer(const Audio::FAlignedFloatBuffer& InBuffer, float* OutAudio, int32 MaxNumOutputSamples);

		// Metasound creates deinterleaved audio while sound generator requires interleaved audio.
		void InterleaveGeneratedAudio();
		
		void ApplyPendingUpdatesToInputs();

		void HandleRenderTimingEnableDisable();

		bool bIsGraphBuilding;
		bool bIsFinishTriggered;
		bool bIsFinished;
		bool bPendingGraphTrigger;
		bool bIsNewGraphPending;
		bool bIsWaitingForFirstGraph;

		int32 FinishSample = INDEX_NONE;
		int32 NumChannels;
		int32 NumFramesPerExecute;
		int32 NumSamplesPerExecute;

	protected:
		FExecuter RootExecuter;
		FVertexInterfaceData VertexInterfaceData;

		TArray<FAudioBufferReadRef> GraphOutputAudio;

		// Triggered when metasound is finished
		FTriggerReadRef OnFinishedTriggerRef;
	private:

		Audio::FAlignedFloatBuffer InterleavedAudioBuffer;

		Audio::FAlignedFloatBuffer OverflowBuffer;

		FCriticalSection PendingGraphMutex;
		TUniquePtr<MetasoundGeneratorPrivate::FMetasoundGeneratorData> PendingGraphData;

		TUniquePtr<Frontend::FGraphAnalyzer> GraphAnalyzer;

		TSharedPtr<TSpscQueue<FMetaSoundParameterTransmitter::FParameter>> ParameterQueue;
	protected:

		MetasoundGeneratorPrivate::FParameterSetterSortedMap ParameterSetters;
	private:

		// These next items are needed to provide a destination for the FAudioDevice, etc. to
		// send parameter packs to. Every playing metasound will have a parameter destination
		// that can accept parameter packs.
		FSendAddress ParameterPackSendAddress;
		TReceiverPtr<FMetasoundParameterStorageWrapper> ParameterPackReceiver;
		
	protected:
		// This map provides setters for all of the input nodes in the metasound graph. 
		// It is used when processing named parameters in a parameter pack.
		TMap<FName, MetasoundGeneratorPrivate::FParameterPackSetter> ParameterPackSetters;
	private:

		// While parameter packs may arrive via the IAudioParameterInterface system,
		// a faster method of sending parameters is via the QueueParameterPack function 
		// and this queue.
		TMpscQueue<TSharedPtr<FMetasoundParameterPackStorage>> ParameterPackQueue;

		TMpscQueue<TUniqueFunction<void()>> OutputAnalyzerModificationQueue;
		TArray<TUniquePtr<Frontend::IVertexAnalyzer>> OutputAnalyzers;

		FOnSetGraph OnSetGraph;

		double RenderTime;
		bool bDoRuntimeRenderTiming;
		TUniquePtr<MetasoundGeneratorPrivate::FRenderTimer> RenderTimer;
	};

	/** FMetasoundConstGraphGenerator generates audio from a given metasound IOperator
	 * which produces a multichannel audio output.
	 */
	class METASOUNDGENERATOR_API FMetasoundConstGraphGenerator : public FMetasoundGenerator
	{
	public:

		explicit FMetasoundConstGraphGenerator(FMetasoundGeneratorInitParams&& InParams);

		explicit FMetasoundConstGraphGenerator(const FOperatorSettings& InOperatorSettings);

		void Init(FMetasoundGeneratorInitParams&& InParams);

		virtual ~FMetasoundConstGraphGenerator() override;

	private:
		friend class FAsyncMetaSoundBuilder;
		void BuildGraph(FMetasoundGeneratorInitParams&& InInitParams);
		bool TryUseCachedOperator(FMetasoundGeneratorInitParams& InParams, bool bTriggerGenerator);
		void ReleaseOperatorToCache();

		TUniquePtr<FMetasoundEnvironment> EnvironmentPtr;
		TUniquePtr<FAsyncTaskBase> BuilderTask;
		FGuid OperatorID;
		bool bUseOperatorPool = false;
	};

	struct METASOUNDGENERATOR_API FMetasoundDynamicGraphGeneratorInitParams : FMetasoundGeneratorInitParams
	{
		TSharedPtr<TSpscQueue<TUniquePtr<DynamicGraph::IDynamicOperatorTransform>>> TransformQueue;
		static void Reset(FMetasoundDynamicGraphGeneratorInitParams& InParams);
	};

	/** FMetasoundDynamicGraphGenerator generates audio from the given a dynamic operator. It also
	 * reacts to updates to inputs and outputs of the dynamic operator.
	 */
	class METASOUNDGENERATOR_API FMetasoundDynamicGraphGenerator : public FMetasoundGenerator
	{
	public:

		/** Create the generator with a graph operator and an output audio reference.
		 *
		 * @param InParams - The generator initialization parameters
		 */
		explicit FMetasoundDynamicGraphGenerator(const FOperatorSettings& InOperatorSettings);

		void Init(FMetasoundDynamicGraphGeneratorInitParams&& InParams);

		virtual ~FMetasoundDynamicGraphGenerator();

		// The callbacks are executed when the equivalent change happens on the owned dynamic operator.
		void OnInputAdded(const FVertexName& InVertexName, const FInputVertexInterfaceData& InInputData);
		void OnInputRemoved(const FVertexName& InVertexName, const FInputVertexInterfaceData& InInputData);
		void OnOutputAdded(const FVertexName& InVertexName, const FOutputVertexInterfaceData& InOutputData);
		void OnOutputUpdated(const FVertexName& InVertexName, const FOutputVertexInterfaceData& InOutputData);
		void OnOutputRemoved(const FVertexName& InVertexName, const FOutputVertexInterfaceData& InOutputData);

	protected:
		virtual TUniquePtr<IOperator> ReleaseGraphOperator() override;

	private:
		void BuildGraph(FMetasoundDynamicGraphGeneratorInitParams&& InParams);


		TArray<FVertexName> AudioOutputNames;
		TUniquePtr<FAsyncTaskBase> BuilderTask;
	};
}
