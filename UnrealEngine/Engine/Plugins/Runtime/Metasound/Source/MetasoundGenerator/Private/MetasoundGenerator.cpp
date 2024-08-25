// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundGenerator.h"

#include "AudioParameter.h"
#include "MetasoundDynamicOperatorTransactor.h"
#include "MetasoundFrontendDataTypeRegistry.h"
#include "MetasoundGeneratorBuilder.h"
#include "MetasoundGeneratorModuleImpl.h"
#include "MetasoundOperatorBuilder.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundOutputNode.h"
#include "MetasoundOperatorCache.h"
#include "MetasoundParameterTransmitter.h"
#include "MetasoundRouter.h"
#include "MetasoundTrace.h"
#include "MetasoundTrigger.h"
#include "MetasoundVertexData.h"
#include "Analysis/MetasoundFrontendAnalyzerFactory.h"
#include "Analysis/MetasoundFrontendAnalyzerRegistry.h"
#include "Analysis/MetasoundFrontendVertexAnalyzer.h"
#include "DSP/FloatArrayMath.h"
#include "Interfaces/MetasoundFrontendSourceInterface.h"
#include "Modules/ModuleManager.h"

#ifndef ENABLE_METASOUNDGENERATOR_INVALID_SAMPLE_VALUE_LOGGING 
#define ENABLE_METASOUNDGENERATOR_INVALID_SAMPLE_VALUE_LOGGING !UE_BUILD_SHIPPING
#endif

CSV_DECLARE_CATEGORY_MODULE_EXTERN(METASOUNDGRAPHCORE_API, Audio_Metasound);

namespace Metasound
{
	namespace ConsoleVariables
	{
		static bool bEnableAsyncMetaSoundGeneratorBuilder = true;
		static bool bEnableExperimentalAutoCachingForOneShotOperators = false;
		static bool bEnableExperimentalAutoCachingForAllOperators = false;
#if ENABLE_METASOUNDGENERATOR_INVALID_SAMPLE_VALUE_LOGGING
		static bool bEnableMetaSoundGeneratorNonFiniteLogging = false;
		static bool bEnableMetaSoundGeneratorInvalidSampleValueLogging = false;
		static float MetasoundGeneratorSampleValueThreshold = 2.f;
#endif // if ENABLE_METASOUNDGENERATOR_INVALID_SAMPLE_VALUE_LOGGING
	}

	namespace MetasoundGeneratorPrivate
	{
#if ENABLE_METASOUNDGENERATOR_INVALID_SAMPLE_VALUE_LOGGING
		void LogInvalidAudioSampleValues(const FString& InMetaSoundName, const TArray<FAudioBufferReadRef>& InAudioBuffers)
		{
			if (ConsoleVariables::bEnableMetaSoundGeneratorNonFiniteLogging || ConsoleVariables::bEnableMetaSoundGeneratorInvalidSampleValueLogging)
			{
				const int32 NumChannels = InAudioBuffers.Num();

				// Check outputs for non finite values if any sample value logging is enabled.
				for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ChannelIndex++)
				{
					const FAudioBuffer& Buffer = *InAudioBuffers[ChannelIndex];
					const float* Data = Buffer.GetData();
					const int32 Num = Buffer.Num();

					for (int32 i = 0; i < Num; i++)
					{
						if (!FMath::IsFinite(Data[i]))
						{
							UE_LOG(LogMetaSound, Error, TEXT("Found non-finite sample (%f) in channel %d of MetaSound %s"), Data[i], ChannelIndex, *InMetaSoundName);
							break;
						}
					}

					// Only check threshold if explicitly enabled
					if (ConsoleVariables::bEnableMetaSoundGeneratorInvalidSampleValueLogging)
					{
						const float Threshold = FMath::Abs(ConsoleVariables::MetasoundGeneratorSampleValueThreshold);
						const float MaxAbsValue = Audio::ArrayMaxAbsValue(Buffer);
						if (MaxAbsValue > Threshold)
						{
							UE_LOG(LogMetaSound, Warning, TEXT("Found sample (absolute value: %f) exceeding threshold (%f) in channel %d of MetaSound %s"), MaxAbsValue, Threshold, ChannelIndex, *InMetaSoundName);

						}
					}
				}
			}
		}
#endif // if ENABLE_METASOUNDGENERATOR_INVALID_SAMPLE_VALUE_LOGGING

		struct FRenderTimer
		{
			FRenderTimer(const FOperatorSettings& InSettings, double InAnalysisDuration)
			{
				// Use single pole IIR filter to smooth data.
				
				const double AnalysisAudioFrameCount = FMath::Max(1.0, InAnalysisDuration * InSettings.GetSampleRate());
				const double AnalysisRenderBlockCount = FMath::Max(1.0, AnalysisAudioFrameCount / FMath::Max(1, InSettings.GetNumFramesPerBlock()));
				const double DigitalCutoff = 1. / AnalysisRenderBlockCount;

				SmoothingAlpha = 1. - FMath::Exp(-UE_PI * DigitalCutoff);
				SmoothingAlpha = FMath::Clamp(SmoothingAlpha, 0.0, 1.0 - UE_DOUBLE_SMALL_NUMBER);
				SecondsOfAudioProducedPerBlock = static_cast<double>(InSettings.GetNumFramesPerBlock()) / FMath::Max(1., static_cast<double>(InSettings.GetSampleRate()));
			}

			double UpdateCPUCoreUtilization()
			{
				double CPUSecondsToRenderBlock = FPlatformTime::ToSeconds64(AccumulatedCycles);
				if (CPUSecondsToRenderBlock > 0.0)
				{
					double NewCPUUtil = CPUSecondsToRenderBlock / SecondsOfAudioProducedPerBlock;
					if (CPUCoreUtilization >= 0.0)
					{
						CPUCoreUtilization = SmoothingAlpha * NewCPUUtil + (1. - SmoothingAlpha) * CPUCoreUtilization;
					}
					else
					{
						CPUCoreUtilization = NewCPUUtil;
					}
				}
				AccumulatedCycles = 0;
				return CPUCoreUtilization;
			}

			FORCEINLINE void AccumulateCycles(uint64 Cycles)
			{
				AccumulatedCycles += Cycles;
			}

		private:
			uint64 AccumulatedCycles = 0;
			double CPUCoreUtilization = -1.0;
			double SmoothingAlpha = 1.0;
			double SecondsOfAudioProducedPerBlock = 0.0;
		};

		struct FBlockRenderScope
		{
			FBlockRenderScope(FRenderTimer* InTimer)
			: Timer(InTimer)
			{
				StartCycle = FPlatformTime::Cycles64();
			}

			~FBlockRenderScope()
			{
				uint64 EndCycle = FPlatformTime::Cycles64();
				if (Timer)
				{
					Timer->AccumulateCycles(EndCycle - StartCycle);
				}
			}
			
		private:
			uint64 StartCycle = 0;
			FRenderTimer* Timer = nullptr;
		};

#ifndef ENABLE_METASOUND_CONSTANT_OUTPUT_VERTEX_ERROR_LOG
#define ENABLE_METASOUND_CONSTANT_OUTPUT_VERTEX_ERROR_LOG DO_CHECK
#endif

#if ENABLE_METASOUND_CONSTANT_OUTPUT_VERTEX_ERROR_LOG
		// Check whether a vertex name represents an output audio buffer. MetaSound
		// Generators cannot dynamically update their output audio buffers. This
		// check is here to inform future developers of this limitation.
		static const FLazyName ParameterNameComponentUE = "UE";
		static const FLazyName ParameterNameComponentOutputFormat = "OutputFormat";
		bool IsAudioOutputVertex(const FVertexName& InVertexName)
		{
			FName Namespace;
			FName Remaining;
			Audio::FParameterPath::SplitName(InVertexName, Namespace, Remaining);
			if (ParameterNameComponentUE == Namespace)
			{
				FName Group;
				FName Name;
				Audio::FParameterPath::SplitName(Remaining, Group, Name);
				return (ParameterNameComponentOutputFormat == Name);
			}
			return false;
		}

		void LogErrorIfIsAudioVertex(const FVertexName& InVertexName, const TCHAR* Message)
		{
			if (IsAudioOutputVertex(InVertexName))
			{
				UE_LOG(LogMetaSound, Error, TEXT("Cannot modify vertex %s. %s"), *InVertexName.ToString(), Message);
			}
		};
#else
		void LogErrorIfIsAudioVertex(const FVertexName& InVertexName, const TCHAR* Message)
		{
		}
#endif
	}	
}

FAutoConsoleVariableRef CVarMetaSoundEnableAsyncGeneratorBuilder(
	TEXT("au.MetaSound.EnableAsyncGeneratorBuilder"),
	Metasound::ConsoleVariables::bEnableAsyncMetaSoundGeneratorBuilder,
	TEXT("Enables async building of FMetaSoundGenerators\n")
	TEXT("Default: true"),
	ECVF_Default);

FAutoConsoleVariableRef CVarMetaSoundEnableExperimentalAutoCachingForOneShotOperators(
	TEXT("au.MetaSound.Experimental.EnableAutoCachingForOneShotOperators"),
	Metasound::ConsoleVariables::bEnableExperimentalAutoCachingForOneShotOperators,
	TEXT("Enables auto-caching of MetaSound operators using the OneShot source interface.\n")
	TEXT("(see MetasoundOperatorCacheSubsystem.h for manual path).\n")
	TEXT("Default: false"),
	ECVF_Default);

FAutoConsoleVariableRef CVarMetaSoundEnableExperimentalAutoCachingForAllOperators(
	TEXT("au.MetaSound.Experimental.EnableAutoCachingForAllOperators"),
	Metasound::ConsoleVariables::bEnableExperimentalAutoCachingForAllOperators,
	TEXT("Enables auto-caching of all MetaSound operators.\n")
	TEXT("(see MetasoundOperatorCacheSubsystem.h for manual path).\n")
	TEXT("Default: false"),
	ECVF_Default);

#if ENABLE_METASOUNDGENERATOR_INVALID_SAMPLE_VALUE_LOGGING

FAutoConsoleVariableRef CVarMetaSoundEnableGeneratorNonFiniteLogging(
	TEXT("au.MetaSound.EnableGeneratorNonFiniteLogging"),
	Metasound::ConsoleVariables::bEnableMetaSoundGeneratorNonFiniteLogging,
	TEXT("Enables logging of non-finite (NaN/inf) audio samples values produced from a FMetaSoundGenerator\n")
	TEXT("Default: false"),
	ECVF_Default);

FAutoConsoleVariableRef CVarMetaSoundEnableGeneratorInvalidSampleValueLogging(
	TEXT("au.MetaSound.EnableGeneratorInvalidSampleValueLogging"),
	Metasound::ConsoleVariables::bEnableMetaSoundGeneratorInvalidSampleValueLogging,
	TEXT("Enables logging of audio samples values produced from a FMetaSoundGenerator which exceed the absolute sample value threshold\n")
	TEXT("Default: false"),
	ECVF_Default);

FAutoConsoleVariableRef CVarMetaSoundGeneratorSampleValueThrehshold(
	TEXT("au.MetaSound.GeneratorSampleValueThreshold"),
	Metasound::ConsoleVariables::MetasoundGeneratorSampleValueThreshold,
	TEXT("If invalid sample value logging is enabled, this sets the maximum abs value threshold for logging samples\n")
	TEXT("Default: 2.0"),
	ECVF_Default);

#endif // if !UE_BUILD_SHIPPING

namespace Metasound
{
	namespace MetasoundGeneratorPrivate
	{
		bool HasOneShotInterface(const FVertexInterface& Interface)
		{
			return Interface.GetOutputInterface().Contains(Frontend::SourceOneShotInterface::Outputs::OnFinished);
		}
	}

	void FMetasoundGeneratorInitParams::Reset(FMetasoundGeneratorInitParams& InParams)
	{
		InParams.Environment = {};
		InParams.MetaSoundName = {};
		InParams.AudioOutputNames = {};
		InParams.DefaultParameters = {};
		InParams.DataChannel.Reset();
	}

	void FMetasoundDynamicGraphGeneratorInitParams::Reset(FMetasoundDynamicGraphGeneratorInitParams& InParams)
	{
		FMetasoundGeneratorInitParams::Reset(InParams);
		InParams.TransformQueue.Reset();
	}

	FMetasoundGenerator::FMetasoundGenerator(const FOperatorSettings& InOperatorSettings)
		: OperatorSettings(InOperatorSettings)
#if ENABLE_METASOUND_GENERATOR_INSTANCE_COUNTING
		, InstanceCounter(FModuleManager::GetModuleChecked<FMetasoundGeneratorModule>("MetasoundGenerator").GetOperatorInstanceCounterManager())
#endif // if ENABLE_METASOUND_GENERATOR_INSTANCE_COUNTING
		, bIsFinishTriggered(false)
		, bIsFinished(false)
		, bPendingGraphTrigger(true)
		, bIsNewGraphPending(false)
		, bIsWaitingForFirstGraph(true)
		, NumChannels(0)
		, NumFramesPerExecute(InOperatorSettings.GetNumFramesPerBlock())
		, NumSamplesPerExecute(0)
		, OnFinishedTriggerRef(FTriggerWriteRef::CreateNew(InOperatorSettings))
		, RenderTime(0.0)
#if ENABLE_METASOUND_GENERATOR_RENDER_TIMING
		, bDoRuntimeRenderTiming(true)
#else
		, bDoRuntimeRenderTiming(false)
#endif // if ENABLE_METASOUND_GENERATOR_RENDER_TIMING
	{
	}

	FMetasoundGenerator::~FMetasoundGenerator()
	{
		// Remove routing for parameter packs
		ParameterPackReceiver.Reset();
		FDataTransmissionCenter::Get().UnregisterDataChannelIfUnconnected(ParameterPackSendAddress);
	}

	FDelegateHandle FMetasoundGenerator::AddGraphSetCallback(FOnSetGraph::FDelegate&& Delegate)
	{
		FScopeLock SetPendingGraphLock(&PendingGraphMutex);
		// If we already have a graph give the delegate an initial call
		if (!bIsNewGraphPending && !bIsWaitingForFirstGraph)
		{
			Delegate.ExecuteIfBound();
		}
		return OnSetGraph.Add(Delegate);
	}

	void FMetasoundGenerator::InitBase(const FMetasoundGeneratorInitParams& InInitParams)
	{
		MetasoundName = InInitParams.MetaSoundName;
		NumChannels = InInitParams.AudioOutputNames.Num();
		NumSamplesPerExecute = NumChannels * NumFramesPerExecute;

#if ENABLE_METASOUND_GENERATOR_INSTANCE_COUNTING
		InstanceCounter.Init(MetasoundName);
#endif // if ENABLE_METASOUND_GENERATOR_INSTANCE_COUNTING

		// Data channels
		ParameterQueue = InInitParams.DataChannel;

		// Create the routing for parameter packs
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		ParameterPackSendAddress = UMetasoundParameterPack::CreateSendAddressFromEnvironment(InInitParams.Environment);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		ParameterPackReceiver = FDataTransmissionCenter::Get().RegisterNewReceiver<FMetasoundParameterStorageWrapper>(ParameterPackSendAddress, FReceiverInitParams{ OperatorSettings });
	}



	bool FMetasoundGenerator::RemoveGraphSetCallback(const FDelegateHandle& Handle)
	{
		return OnSetGraph.Remove(Handle);
	}

	void FMetasoundGenerator::SetPendingGraph(MetasoundGeneratorPrivate::FMetasoundGeneratorData&& InData, bool bTriggerGraph)
	{
		using namespace MetasoundGeneratorPrivate;

		FScopeLock SetPendingGraphLock(&PendingGraphMutex);
		{
			PendingGraphData = MakeUnique<FMetasoundGeneratorData>(MoveTemp(InData));
			bPendingGraphTrigger = bTriggerGraph;
			bIsNewGraphPending = true;
		}
	}

	void FMetasoundGenerator::SetPendingGraphBuildFailed()
	{
		FScopeLock SetPendingGraphLock(&PendingGraphMutex);
		{
			PendingGraphData.Reset();
			bPendingGraphTrigger = false;
			bIsNewGraphPending = true;
		}
	}

	bool FMetasoundGenerator::UpdateGraphIfPending()
	{
		FScopeLock GraphLock(&PendingGraphMutex);
		if (bIsNewGraphPending)
		{
			SetGraph(MoveTemp(PendingGraphData), bPendingGraphTrigger);
			bIsNewGraphPending = false;
			return true;
		}

		return false;
	}

	void FMetasoundGenerator::SetGraph(TUniquePtr<MetasoundGeneratorPrivate::FMetasoundGeneratorData>&& InData, bool bTriggerGraph)
	{
		if (!InData.IsValid())
		{
			return;
		}

		InterleavedAudioBuffer.Reset();

		// Copy off all vertex interface data
		VertexInterfaceData = MoveTemp(InData->VertexInterfaceData);

		GraphOutputAudio.Reset();
		if (InData->OutputBuffers.Num() == NumChannels)
		{
			if (InData->OutputBuffers.Num() > 0)
			{
				GraphOutputAudio.Append(InData->OutputBuffers.GetData(), InData->OutputBuffers.Num());
			}
		}
		else
		{
			int32 FoundNumChannels = InData->OutputBuffers.Num();

			UE_LOG(LogMetaSound, Warning, TEXT("Metasound generator expected %d number of channels, found %d"), NumChannels, FoundNumChannels);

			int32 NumChannelsToCopy = FMath::Min(FoundNumChannels, NumChannels);
			int32 NumChannelsToCreate = NumChannels - NumChannelsToCopy;

			if (NumChannelsToCopy > 0)
			{
				GraphOutputAudio.Append(InData->OutputBuffers.GetData(), NumChannelsToCopy);
			}
			for (int32 i = 0; i < NumChannelsToCreate; i++)
			{
				GraphOutputAudio.Add(TDataReadReference<FAudioBuffer>::CreateNew(InData->OperatorSettings));
			}
		}

		OnFinishedTriggerRef = InData->TriggerOnFinishRef;

		// The graph operator and graph audio output contain all the values needed
		// by the sound generator.
		RootExecuter.SetOperator(MoveTemp(InData->GraphOperator));


		// Query the graph output to get the number of output audio channels.
		// Multichannel version:
		check(NumChannels == GraphOutputAudio.Num());

		if (NumSamplesPerExecute > 0)
		{
			// Preallocate interleaved buffer as it is necessary for any audio generation calls.
			InterleavedAudioBuffer.AddUninitialized(NumSamplesPerExecute);
		}

		GraphAnalyzer = MoveTemp(InData->GraphAnalyzer);

		ParameterSetters = MoveTemp(InData->ParameterSetters);
		ParameterPackSetters = MoveTemp(InData->ParameterPackSetters);

		if (bTriggerGraph)
		{
			InData->TriggerOnPlayRef->TriggerFrame(0);
		}

		bIsWaitingForFirstGraph = false;
		OnSetGraph.Broadcast();
		bVertexInterfaceHasChanged.store(true);
	}

	TUniquePtr<IOperator> FMetasoundGenerator::ReleaseGraphOperator()
	{
		return RootExecuter.ReleaseOperator();
	}

	FInputVertexInterfaceData FMetasoundGenerator::ReleaseInputVertexData()
	{
		return MoveTemp(VertexInterfaceData.GetInputs());
	}

	void FMetasoundGenerator::ClearGraph()
	{
		RootExecuter.ReleaseOperator();
		VertexInterfaceData = FVertexInterfaceData();
		GraphOutputAudio.Reset();
		OnFinishedTriggerRef = TDataWriteReference<FTrigger>::CreateNew(OperatorSettings);
		GraphAnalyzer.Reset();
		ParameterSetters.Reset();
		ParameterPackSetters.Reset();

		bVertexInterfaceHasChanged.store(true);
	}

	void FMetasoundGenerator::QueueParameterPack(TSharedPtr<FMetasoundParameterPackStorage> ParameterPack)
	{
		ParameterPackQueue.Enqueue(ParameterPack);
	}

	int32 FMetasoundGenerator::GetNumChannels() const
	{
		return NumChannels;
	}

	bool AnalyzerAddressesReferToSameGeneratorOutput(const Frontend::FAnalyzerAddress& Lhs, const Frontend::FAnalyzerAddress& Rhs)
	{
		return Lhs.OutputName == Rhs.OutputName && Lhs.AnalyzerName == Rhs.AnalyzerName;
	}
	
	void FMetasoundGenerator::AddOutputVertexAnalyzer(const Frontend::FAnalyzerAddress& AnalyzerAddress)
	{
		OutputAnalyzerModificationQueue.Enqueue([this, AnalyzerAddress]
		{
			if (OutputAnalyzers.ContainsByPredicate([AnalyzerAddress](const TUniquePtr<Frontend::IVertexAnalyzer>& Analyzer)
			{
				return AnalyzerAddressesReferToSameGeneratorOutput(AnalyzerAddress, Analyzer->GetAnalyzerAddress());
			}))
			{
				return;
			}
			
			const FAnyDataReference* OutputReference = VertexInterfaceData.GetOutputs().FindDataReference(AnalyzerAddress.OutputName);
			
			if (nullptr == OutputReference)
			{
				return;
			}
			
			const Frontend::IVertexAnalyzerFactory* Factory = Frontend::IVertexAnalyzerRegistry::Get().FindAnalyzerFactory(AnalyzerAddress.AnalyzerName);
			
			if (nullptr == Factory)
			{
				return;
			}
			
			TUniquePtr<Frontend::IVertexAnalyzer> Analyzer = Factory->CreateAnalyzer({ AnalyzerAddress, OperatorSettings, *OutputReference});
			
			if (nullptr == Analyzer)
			{
				return;
			}

			if (!Analyzer->OnOutputDataChanged.IsBound())
			{
				Analyzer->OnOutputDataChanged.BindLambda(
					[this, AnalyzerAddress](const FName AnalyzerOutputName, TSharedPtr<IOutputStorage> OutputData)
					{
						OnOutputChanged.Broadcast(
							AnalyzerAddress.AnalyzerName,
							AnalyzerAddress.OutputName,
							AnalyzerOutputName,
							OutputData);
					});
			}
			
			OutputAnalyzers.Emplace(MoveTemp(Analyzer));
		});
	}

	void FMetasoundGenerator::RemoveOutputVertexAnalyzer(const Frontend::FAnalyzerAddress& AnalyzerAddress)
	{
		OutputAnalyzerModificationQueue.Enqueue([this, AnalyzerAddress]
		{
			OutputAnalyzers.RemoveAll([AnalyzerAddress](const TUniquePtr<Frontend::IVertexAnalyzer>& Analyzer)
			{
				return AnalyzerAddressesReferToSameGeneratorOutput(AnalyzerAddress, Analyzer->GetAnalyzerAddress());
			});
		});
	}

	int32 FMetasoundGenerator::OnGenerateAudio(float* OutAudio, int32 NumSamplesRemaining)
	{
		METASOUND_LLM_SCOPE;
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("MetasoundGenerator::OnGenerateAudio %s"), *MetasoundName));
		CSV_SCOPED_TIMING_STAT(Audio_Metasound, OnGenerateAudio);

		// Defer finishing the metasound generator one block
		if (bIsFinishTriggered)
		{
			bIsFinished = true;
		}

		if (NumSamplesRemaining <= 0)
		{
			return 0;
		}

		UpdateGraphIfPending();

		HandleRenderTimingEnableDisable();

		// Output silent audio if we're still building a graph
		if (bIsWaitingForFirstGraph)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("MetasoundGenerator::OnGenerateAudio::MissedRenderDeadline %s"), *MetasoundName));
			CSV_CUSTOM_STAT(Audio_Metasound, WaitingForGraphBuildOnPlaybackCount, 1, ECsvCustomStatOp::Accumulate);
			FMemory::Memset(OutAudio, 0, sizeof(float)* NumSamplesRemaining);
			return NumSamplesRemaining;
		}

		// If no longer pending and executer is no-op, kill the MetaSound.
		// Covers case where there was an error when building, resulting in
		// Executer operator being assigned to NoOp.
		else if (RootExecuter.IsNoOp() || NumSamplesPerExecute < 1)
		{
			bIsFinished = true;
			FMemory::Memset(OutAudio, 0, sizeof(float) * NumSamplesRemaining);
			return NumSamplesRemaining;
		}

		// Modify the output analyzers as needed
		{
			while (TOptional<TUniqueFunction<void()>> ModFn = OutputAnalyzerModificationQueue.Dequeue())
			{
				(*ModFn)();
			}
		}

		// If we have any audio left in the internal overflow buffer from 
		// previous calls, write that to the output before generating more audio.
		int32 NumSamplesWritten = FillWithBuffer(OverflowBuffer, OutAudio, NumSamplesRemaining);

		if (NumSamplesWritten > 0)
		{
			NumSamplesRemaining -= NumSamplesWritten;
			OverflowBuffer.RemoveAtSwap(0 /* Index */, NumSamplesWritten /* Count */, EAllowShrinking::No);
		}

		while (NumSamplesRemaining > 0)
		{
			// Create a scoped timed section for the bulk of the processing...
			{
				MetasoundGeneratorPrivate::FBlockRenderScope RuntimelBlockRenderScope(RenderTimer.Get());

				ApplyPendingUpdatesToInputs();

				// Call metasound graph operator.
				RootExecuter.Execute();
				
				// Check if generated finished during this execute call
				if (*OnFinishedTriggerRef)
				{
					FinishSample = ((*OnFinishedTriggerRef)[0] * NumChannels);
				}

				// Interleave audio because ISoundGenerator interface expects interleaved audio.
				InterleaveGeneratedAudio();


				// Add audio generated during graph execution to the output buffer.
				int32 ThisLoopNumSamplesWritten = FillWithBuffer(InterleavedAudioBuffer, &OutAudio[NumSamplesWritten], NumSamplesRemaining);

				NumSamplesRemaining -= ThisLoopNumSamplesWritten;
				NumSamplesWritten += ThisLoopNumSamplesWritten;

				// If not all the samples were written, then we have to save the 
				// additional samples to the overflow buffer.
				if (ThisLoopNumSamplesWritten < InterleavedAudioBuffer.Num())
				{
					int32 OverflowCount = InterleavedAudioBuffer.Num() - ThisLoopNumSamplesWritten;

					OverflowBuffer.Reset();
					OverflowBuffer.AddUninitialized(OverflowCount);

					FMemory::Memcpy(OverflowBuffer.GetData(), &InterleavedAudioBuffer.GetData()[ThisLoopNumSamplesWritten], OverflowCount * sizeof(float));
				}

				// Execute the output analyzers
				for (const TUniquePtr<Frontend::IVertexAnalyzer>& Analyzer : OutputAnalyzers)
				{
					Analyzer->Execute();
				}
			}

			// Don't time the graph analyzer. It is only used for graph visualization.

			if (GraphAnalyzer.IsValid())
			{
				GraphAnalyzer->Execute();
			}

			// Create a scoped timed section for the post processing...
			{
				MetasoundGeneratorPrivate::FBlockRenderScope RuntimelBlockRenderScope(RenderTimer.Get());
				RootExecuter.PostExecute();
			}

			// Update timer if there is one...
			if (RenderTimer)
			{
				RenderTime = RenderTimer->UpdateCPUCoreUtilization();
			}
		}

		// If the vertex interface changed, notify listeners
		{
			if (bVertexInterfaceHasChanged.exchange(false))
			{
				OnVertexInterfaceDataUpdated.Broadcast(VertexInterfaceData);
			}
		}

		return NumSamplesWritten;
	}

	int32 FMetasoundGenerator::GetDesiredNumSamplesToRenderPerCallback() const
	{
		return NumFramesPerExecute * NumChannels;
	}

	bool FMetasoundGenerator::IsFinished() const
	{
		return bIsFinished;
	}

	double FMetasoundGenerator::GetCPUCoreUtilization() const
	{
		return RenderTime;
	}

	int32 FMetasoundGenerator::FillWithBuffer(const Audio::FAlignedFloatBuffer& InBuffer, float* OutAudio, int32 MaxNumOutputSamples)
	{
		int32 InNum = InBuffer.Num();

		if (InNum > 0)
		{
			int32 NumSamplesToCopy = FMath::Min(InNum, MaxNumOutputSamples);
			FMemory::Memcpy(OutAudio, InBuffer.GetData(), NumSamplesToCopy * sizeof(float));

			if (FinishSample != INDEX_NONE)
			{
				FinishSample -= NumSamplesToCopy;
				if (FinishSample <= 0)
				{
					bIsFinishTriggered = true;
					FinishSample = INDEX_NONE;
				}
			}

			return NumSamplesToCopy;
		}

		return 0;
	}

	void FMetasoundGenerator::InterleaveGeneratedAudio()
	{
		// Prepare output buffer
		InterleavedAudioBuffer.Reset();

		if (NumSamplesPerExecute > 0)
		{
			InterleavedAudioBuffer.AddUninitialized(NumSamplesPerExecute);
		}

#if ENABLE_METASOUNDGENERATOR_INVALID_SAMPLE_VALUE_LOGGING
		MetasoundGeneratorPrivate::LogInvalidAudioSampleValues(MetasoundName, GraphOutputAudio);
#endif // if ENABLE_METASOUNDGENERATOR_INVALID_SAMPLE_VALUE_LOGGING

		// Iterate over channels
		for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ChannelIndex++)
		{
			const FAudioBuffer& InputBuffer = *GraphOutputAudio[ChannelIndex];

			const float* InputPtr = InputBuffer.GetData();
			float* OutputPtr = &InterleavedAudioBuffer.GetData()[ChannelIndex];

			// Assign values to output for single channel.
			for (int32 FrameIndex = 0; FrameIndex < NumFramesPerExecute; FrameIndex++)
			{
				*OutputPtr = InputPtr[FrameIndex];
				OutputPtr += NumChannels;
			}
		}
		// TODO: memcpy for single channel. 
	}

	void FMetasoundGenerator::ApplyPendingUpdatesToInputs()
	{
		using namespace Frontend;
		using namespace MetasoundGeneratorPrivate;

		auto ProcessPack = [&](FMetasoundParameterPackStorage* Pack)
			{
				for (auto Walker = Pack->begin(); Walker != Pack->end(); ++Walker)
				{
					if (const FParameterPackSetter* ParameterPackSetter = ParameterPackSetters.Find(Walker->Name))
					{
						if (ParameterPackSetter->DataType == Walker->TypeName)
						{
							ParameterPackSetter->SetParameterWithPayload(Walker->GetPayload());
						}
					}
				}
			};

		// Handle parameters from the FMetasoundParameterTransmitter
		if (ParameterQueue.IsValid())
		{
			TOptional<FMetaSoundParameterTransmitter::FParameter> Parameter;
			while ((Parameter = ParameterQueue->Dequeue()))
			{
				if (FParameterSetter* Setter = ParameterSetters.Find(Parameter->Name))
				{
					Setter->Assign(OperatorSettings, Parameter->Value, Setter->DataReference);
				}
			}
		}

		// Handle parameter packs that have come from the IAudioParameterInterface system...
		if (ParameterPackReceiver.IsValid())
		{
			FMetasoundParameterStorageWrapper Pack;
			while (ParameterPackReceiver->CanPop())
			{
				ParameterPackReceiver->Pop(Pack);
				ProcessPack(Pack.Get().Get());
			}
		}

		// Handle parameter packs that came from QueueParameterPack...
		TSharedPtr<FMetasoundParameterPackStorage> QueuedParameterPack;
		while (ParameterPackQueue.Dequeue(QueuedParameterPack))
		{
			ProcessPack(QueuedParameterPack.Get());
		}
	}

	void FMetasoundGenerator::HandleRenderTimingEnableDisable()
	{
		if (bDoRuntimeRenderTiming && !RenderTimer)
		{
			RenderTimer = MakeUnique<MetasoundGeneratorPrivate::FRenderTimer>(OperatorSettings, 1. /* AnalysisPeriod */);
		}
		else if (!bDoRuntimeRenderTiming && RenderTimer)
		{
			RenderTimer = nullptr;
			RenderTime = 0.0;
		}
	}

	FMetasoundConstGraphGenerator::FMetasoundConstGraphGenerator(FMetasoundGeneratorInitParams&& InInitParams)
	: FMetasoundGenerator(InInitParams.OperatorSettings)
	{
		Init(MoveTemp(InInitParams));
	}

	FMetasoundConstGraphGenerator::FMetasoundConstGraphGenerator(const FOperatorSettings& InOperatorSettings)
	: FMetasoundGenerator(InOperatorSettings)
	{
	}

	static FThreadSafeCounter NumActiveConstGraphs;
	void FMetasoundConstGraphGenerator::Init(FMetasoundGeneratorInitParams&& InParams)
	{
		NumActiveConstGraphs.Increment();
		CSV_CUSTOM_STAT(Audio_Metasound, NumActiveConstGraphs, NumActiveConstGraphs.GetValue(), ECsvCustomStatOp::Set);

		InitBase(InParams);
		// attempt to use operator cache instead of building a new operator.
		bool bDidUseCachedOperator = false;
		const bool bIsOperatorPoolEnabled = ConsoleVariables::bEnableExperimentalAutoCachingForOneShotOperators || ConsoleVariables::bEnableExperimentalAutoCachingForAllOperators;
		// Dynamic operators cannot use the operator cache because they can change their internal structure. 
		// The operator cache assumes that the operator is unchanged from it's original structure. 
		if (bIsOperatorPoolEnabled)
		{
			bUseOperatorPool = ConsoleVariables::bEnableExperimentalAutoCachingForAllOperators || MetasoundGeneratorPrivate::HasOneShotInterface(InParams.Graph->GetVertexInterface());
		}

		// check the cache for manually pre-cached operators
		// even if the cvars disable automatic cache population.
		bDidUseCachedOperator = TryUseCachedOperator(InParams, true /* bTriggerGenerator */);

		if (!bDidUseCachedOperator)
		{
			BuildGraph(MoveTemp(InParams));
		}
	}

	FMetasoundConstGraphGenerator::~FMetasoundConstGraphGenerator()
	{
		NumActiveConstGraphs.Decrement();
		CSV_CUSTOM_STAT(Audio_Metasound, NumActiveConstGraphs, NumActiveConstGraphs.GetValue(), ECsvCustomStatOp::Set);

		if (BuilderTask.IsValid())
		{
			BuilderTask->EnsureCompletion();
			BuilderTask = nullptr;
		}

		if (bUseOperatorPool)
		{
			ReleaseOperatorToCache();
		}
	}

	bool FMetasoundConstGraphGenerator::TryUseCachedOperator(FMetasoundGeneratorInitParams& InInitParams, bool bInTriggerGraph)
	{
		using namespace MetasoundGeneratorPrivate;
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(FMetasoundConstGraphGenerator::TryUseCachedOperator);

		FMetasoundGeneratorModule& Module = FModuleManager::GetModuleChecked<FMetasoundGeneratorModule>("MetasoundGenerator");
		TSharedPtr<FOperatorPool> OperatorPool = Module.GetOperatorPool();
		if (OperatorPool.IsValid())
		{
			// See if the cache has an operator with matching OperatorID
			OperatorID = InInitParams.Graph->GetInstanceID();
			FOperatorAndInputs GraphOperatorAndInputs = OperatorPool->ClaimOperator(OperatorID);
			if (GraphOperatorAndInputs.Operator.IsValid())
			{
				UE_LOG(LogMetasoundGenerator, VeryVerbose, TEXT("Using cached operator %s for MetaSound %s"), *LexToString(OperatorID), *InInitParams.MetaSoundName);
				bUseOperatorPool = true; // raise this flag to make sure we put the operator back in the pool (regardless of CVAR state)

				// Apply and default inputs to the operator.
				GeneratorBuilder::ResetGraphOperatorInputs(OperatorSettings, MoveTemp(InInitParams.DefaultParameters), GraphOperatorAndInputs.Inputs);

				// Reset operator internal state before playing it.
				if (IOperator::FResetFunction Reset = GraphOperatorAndInputs.Operator->GetResetFunction())
				{
					EnvironmentPtr = MakeUnique<FMetasoundEnvironment>(InInitParams.Environment);
					IOperator::FResetParams ResetParams {OperatorSettings, *EnvironmentPtr};
					Reset(GraphOperatorAndInputs.Operator.Get(), ResetParams);
				}
				
				// Create data needed for MetaSound Generator
				FMetasoundGeneratorData Data = GeneratorBuilder::BuildGeneratorData(OperatorSettings, InInitParams, MoveTemp(GraphOperatorAndInputs), nullptr);

				SetGraph(MakeUnique<FMetasoundGeneratorData>(MoveTemp(Data)), bInTriggerGraph);
				
				return true;
			}
		}

		return false;
	}

	void FMetasoundConstGraphGenerator::ReleaseOperatorToCache()
	{
		using namespace MetasoundGeneratorPrivate;
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(FMetasoundConstGraphGenerator::ReleaseOperatorToCache);

		if (OperatorID.IsValid())
		{
			FMetasoundGeneratorModule& Module = FModuleManager::GetModuleChecked<FMetasoundGeneratorModule>("MetasoundGenerator");
			TSharedPtr<FOperatorPool> OperatorPool = Module.GetOperatorPool();
			TUniquePtr<IOperator> GraphOperator = ReleaseGraphOperator();

			if (OperatorPool.IsValid() && GraphOperator.IsValid())
			{
				// Release graph operator and input data to the cache
				UE_LOG(LogMetasoundGenerator, VeryVerbose, TEXT("Caching operator %s"), *LexToString(OperatorID));

				// give the operator a chance to reduce its memory footprint before being cached
				// in the future this should be a conanical phase of an operator's lifecycle (i.e. Reset, Execute, Hybernate)
				if (EnvironmentPtr.IsValid())
				{
					if (IOperator::FResetFunction Reset = GraphOperator->GetResetFunction())
					{
						IOperator::FResetParams ResetParams {OperatorSettings, *EnvironmentPtr.Get()};
						Reset(GraphOperator.Get(), ResetParams);
					}
				}

				OperatorPool->AddOperator(OperatorID, MoveTemp(GraphOperator), ReleaseInputVertexData());

			}
		}

		// Clear out any internal references to graph data
		ClearGraph();
	}

	void FMetasoundConstGraphGenerator::BuildGraph(FMetasoundGeneratorInitParams&& InInitParams)
	{
		const bool bBuildAsync = !InInitParams.bBuildSynchronous && Metasound::ConsoleVariables::bEnableAsyncMetaSoundGeneratorBuilder;

		BuilderTask = MakeUnique<FAsyncTask<FAsyncMetaSoundBuilder>>(this, MoveTemp(InInitParams), true /* bTriggerGenerator */);
		
		if (bBuildAsync)
		{
			// Build operator asynchronously
			BuilderTask->StartBackgroundTask(GBackgroundPriorityThreadPool);
		}
		else
		{
			// Build operator synchronously
			BuilderTask->StartSynchronousTask();
			BuilderTask = nullptr;
			UpdateGraphIfPending();
		}
	}

	FMetasoundDynamicGraphGenerator::FMetasoundDynamicGraphGenerator(const FOperatorSettings& InOperatorSettings)
	: FMetasoundGenerator(InOperatorSettings)
	{
	}

	static FThreadSafeCounter NumActiveDynamicGraphs;
	FMetasoundDynamicGraphGenerator::~FMetasoundDynamicGraphGenerator()
	{
		NumActiveDynamicGraphs.Decrement();
		CSV_CUSTOM_STAT(Audio_Metasound, NumActiveDynamicGraphs, NumActiveDynamicGraphs.GetValue(), ECsvCustomStatOp::Set);

		if (BuilderTask.IsValid())
		{
			BuilderTask->EnsureCompletion();
			BuilderTask = nullptr;
		}
	}
	
	void FMetasoundDynamicGraphGenerator::Init(FMetasoundDynamicGraphGeneratorInitParams&& InParams)
	{
		NumActiveDynamicGraphs.Increment();
		CSV_CUSTOM_STAT(Audio_Metasound, NumActiveDynamicGraphs, NumActiveDynamicGraphs.GetValue(), ECsvCustomStatOp::Set);

		InitBase(InParams);
		AudioOutputNames = InParams.AudioOutputNames;
		BuildGraph(MoveTemp(InParams));
	}

	void FMetasoundDynamicGraphGenerator::BuildGraph(FMetasoundDynamicGraphGeneratorInitParams&& InParams)
	{
		const bool bBuildAsync = !InParams.bBuildSynchronous && Metasound::ConsoleVariables::bEnableAsyncMetaSoundGeneratorBuilder;

		BuilderTask = MakeUnique<FAsyncTask<FAsyncDynamicMetaSoundBuilder>>(this, MoveTemp(InParams), true /* bTriggerGenerator */);

		if (bBuildAsync)
		{
			// Build operator asynchronously
			BuilderTask->StartBackgroundTask(GBackgroundPriorityThreadPool);
		}
		else
		{
			// Build operator synchronously
			BuilderTask->StartSynchronousTask();
			BuilderTask = nullptr;
			UpdateGraphIfPending();
		}
	}

	void FMetasoundDynamicGraphGenerator::OnInputAdded(const FVertexName& InVertexName, const FInputVertexInterfaceData& InGraphInputs)
	{
		// Update vertex data and set parameters. 
		VertexInterfaceData.GetInputs() = InGraphInputs;
		GeneratorBuilder::AddParameterSetterIfWritable(InVertexName, VertexInterfaceData.GetInputs(), ParameterSetters, ParameterPackSetters);

		bVertexInterfaceHasChanged.store(true);
	}

	void FMetasoundDynamicGraphGenerator::OnInputRemoved(const FVertexName& InVertexName, const FInputVertexInterfaceData& InGraphInputs)
	{
		VertexInterfaceData.GetInputs() = InGraphInputs;
		ParameterSetters.Remove(InVertexName);
		ParameterPackSetters.Remove(InVertexName);
		bVertexInterfaceHasChanged.store(true);
	}

	void FMetasoundDynamicGraphGenerator::OnOutputAdded(const FVertexName& InVertexName, const FOutputVertexInterfaceData& InGraphOutputs)
	{
		MetasoundGeneratorPrivate::LogErrorIfIsAudioVertex(InVertexName, TEXT("The MetaSound Generator cannot dynamically add audio outputs"));
		VertexInterfaceData.GetOutputs() = InGraphOutputs;

		if (Frontend::SourceOneShotInterface::Outputs::OnFinished == InVertexName)
		{
			OnFinishedTriggerRef = InGraphOutputs.GetDataReadReference<FTrigger>(InVertexName);
		}
		bVertexInterfaceHasChanged.store(true);
	}

	void FMetasoundDynamicGraphGenerator::OnOutputUpdated(const FVertexName& InVertexName, const FOutputVertexInterfaceData& InGraphOutputs)
	{
		VertexInterfaceData.GetOutputs() = InGraphOutputs;

		// If it's an output audio buffer, update the internal reference	
		int32 AudioOutputIndex = AudioOutputNames.IndexOfByKey(InVertexName);
		if (INDEX_NONE != AudioOutputIndex)
		{
			GraphOutputAudio[AudioOutputIndex] = InGraphOutputs.GetDataReadReference<FAudioBuffer>(InVertexName);
		}
		// If it's the on-finished trigger, update the internal reference
		else if (Frontend::SourceOneShotInterface::Outputs::OnFinished == InVertexName)
		{
			OnFinishedTriggerRef = InGraphOutputs.GetDataReadReference<FTrigger>(InVertexName);
		}
		
		bVertexInterfaceHasChanged.store(true);
	}

	void FMetasoundDynamicGraphGenerator::OnOutputRemoved(const FVertexName& InVertexName, const FOutputVertexInterfaceData& InGraphOutputs)
	{
		MetasoundGeneratorPrivate::LogErrorIfIsAudioVertex(InVertexName, TEXT("The MetaSound Generator cannot dynamically remove audio outputs"));

		VertexInterfaceData.GetOutputs() = InGraphOutputs;
		bVertexInterfaceHasChanged.store(true);
	}

	TUniquePtr<IOperator> FMetasoundDynamicGraphGenerator::ReleaseGraphOperator()
	{
		FMetasoundGenerator::ReleaseGraphOperator();

		// Never allow the dynamic operator to leave this object because it contains 
		// callbacks pointing to this object instance. If the dynamic operator left
		// this object, it could attempt to execute member functions on this object
		// after this object has been destroyed. (X╭╮X) 
		return nullptr;
	}

} 
