// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundGenerator.h"

#include "Analysis/MetasoundFrontendVertexAnalyzer.h"
#include "AudioParameter.h"
#include "DSP/FloatArrayMath.h"
#include "Interfaces/MetasoundFrontendSourceInterface.h"
#include "MetasoundOperatorBuilder.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundOutputNode.h"
#include "MetasoundParameterTransmitter.h"
#include "MetasoundRouter.h"
#include "MetasoundTrace.h"
#include "MetasoundTrigger.h"
#include "MetasoundVertexData.h"
#include "MetasoundFrontendDataTypeRegistry.h"

#ifndef METASOUNDGENERATOR_ENABLE_INVALID_SAMPLE_VALUE_LOGGING 
#define METASOUNDGENERATOR_ENABLE_INVALID_SAMPLE_VALUE_LOGGING !UE_BUILD_SHIPPING
#endif

namespace Metasound
{
	namespace ConsoleVariables
	{
		static bool bEnableAsyncMetaSoundGeneratorBuilder = true;
#if METASOUNDGENERATOR_ENABLE_INVALID_SAMPLE_VALUE_LOGGING
		static bool bEnableMetaSoundGeneratorNonFiniteLogging = false;
		static bool bEnableMetaSoundGeneratorInvalidSampleValueLogging = false;
		static float MetasoundGeneratorSampleValueThreshold = 2.f;
#endif // #if METASOUNDGENERATOR_ENABLE_INVALID_SAMPLE_VALUE_LOGGING
	}

#if METASOUNDGENERATOR_ENABLE_INVALID_SAMPLE_VALUE_LOGGING
	namespace MetasoundGeneratorPrivate
	{
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
	}
#endif // #if METASOUNDGENERATOR_ENABLE_INVALID_SAMPLE_VALUE_LOGGING
}

FAutoConsoleVariableRef CVarMetaSoundEnableAsyncGeneratorBuilder(
	TEXT("au.MetaSound.EnableAsyncGeneratorBuilder"),
	Metasound::ConsoleVariables::bEnableAsyncMetaSoundGeneratorBuilder,
	TEXT("Enables async building of FMetaSoundGenerators\n")
	TEXT("Default: true"),
	ECVF_Default);

#if !UE_BUILD_SHIPPING

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

#endif // #if !UE_BUILD_SHIPPING

namespace Metasound
{
	void FMetasoundGeneratorInitParams::Release()
	{
		Graph.Reset();
		Environment = {};
		MetaSoundName = {};
		AudioOutputNames = {};
		DefaultParameters = {};
	}

	FAsyncMetaSoundBuilder::FAsyncMetaSoundBuilder(FMetasoundGenerator* InGenerator, FMetasoundGeneratorInitParams&& InInitParams, bool bInTriggerGenerator)
		: Generator(InGenerator)
		, InitParams(MoveTemp(InInitParams))
		, bTriggerGenerator(bInTriggerGenerator)
	{
	}

	void FAsyncMetaSoundBuilder::DoWork()
	{
		using namespace Audio;
		using namespace Frontend;

		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("AsyncMetaSoundBuilder::DoWork %s"), *InitParams.MetaSoundName));

		FBuildResults BuildResults;

		// Create an instance of the new graph
		TUniquePtr<IOperator> GraphOperator = BuildGraphOperator(MoveTemp(InitParams.DefaultParameters), BuildResults);
		LogBuildErrors(BuildResults);

		if (GraphOperator.IsValid())
		{
			// Create graph analyzer
			TUniquePtr<FGraphAnalyzer> GraphAnalyzer = BuildGraphAnalyzer(MoveTemp(BuildResults.InternalDataReferences));
	
			// Collect data for generator
			FMetasoundGeneratorData GeneratorData = BuildGeneratorData(InitParams, MoveTemp(GraphOperator), MoveTemp(GraphAnalyzer));

			// Update generator with new metasound graph
			Generator->SetPendingGraph(MoveTemp(GeneratorData), bTriggerGenerator);
		}
		else 
		{
			UE_LOG(LogMetaSound, Error, TEXT("Failed to build Metasound operator from graph in MetasoundSource [%s]"), *InitParams.MetaSoundName);
			// Set null generator data to inform that generator failed to build. 
			// Otherwise, generator will continually wait for a new generator.
			Generator->SetPendingGraphBuildFailed();
		}

		InitParams.Release();
	}

	FMetasoundGeneratorData FAsyncMetaSoundBuilder::BuildGeneratorData(const FMetasoundGeneratorInitParams& InInitParams, TUniquePtr<IOperator> InGraphOperator, TUniquePtr<Frontend::FGraphAnalyzer> InAnalyzer) const
	{
		using namespace Audio;
		using namespace Frontend;

		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(TEXT("AsyncMetaSoundBuilder::BuildGeneratorData"));

		checkf(InGraphOperator.IsValid(), TEXT("Graph operator must be a valid object"));

		// Gather relevant input and output references
		FVertexInterfaceData VertexData(InInitParams.Graph->GetVertexInterface());
		InGraphOperator->Bind(VertexData);

		// Get inputs
		FTriggerWriteRef PlayTrigger = VertexData.GetInputs().GetOrConstructDataWriteReference<FTrigger>(SourceInterface::Inputs::OnPlay, InInitParams.OperatorSettings, false);

		// Get outputs
		TArray<FAudioBufferReadRef> OutputBuffers = FindOutputAudioBuffers(VertexData);
		FTriggerReadRef FinishTrigger = TDataReadReferenceFactory<FTrigger>::CreateExplicitArgs(InInitParams.OperatorSettings, false);

		if (InInitParams.Graph->GetVertexInterface().GetOutputInterface().Contains(SourceOneShotInterface::Outputs::OnFinished))
		{
			// Only attempt to retrieve the on finished trigger if it exists.
			// Attempting to retrieve a data reference from a non-existent vertex 
			// will log an error. 
			FinishTrigger = VertexData.GetOutputs().GetOrConstructDataReadReference<FTrigger>(SourceOneShotInterface::Outputs::OnFinished, InitParams.OperatorSettings, false);
		}

		// Create the parameter setter map so parameter packs can be cracked
		// open and distributed as appropriate...
		TMap<FName, FParameterSetter> ParameterSetters;
		FInputVertexInterfaceData& GraphInputs = VertexData.GetInputs();
		for (auto InputIterator = GraphInputs.begin(); InputIterator != GraphInputs.end(); ++InputIterator)
		{
			const FInputDataVertex& InputVertex = (*InputIterator).GetVertex();
			const Frontend::IParameterAssignmentFunction& Setter = IDataTypeRegistry::Get().GetRawAssignmentFunction(InputVertex.DataTypeName);
			if (Setter)
			{
				FParameterSetter ParameterSetter(InputVertex.DataTypeName,
					(*InputIterator).GetDataReference()->GetRaw(),
					Setter);
				ParameterSetters.Add(InputVertex.VertexName, ParameterSetter);
			}
		}

		// Set data needed for graph
		return FMetasoundGeneratorData 
		{
			InInitParams.OperatorSettings,
			MoveTemp(InGraphOperator),
			MoveTemp(ParameterSetters),
			MoveTemp(InAnalyzer),
			MoveTemp(OutputBuffers),
			MoveTemp(PlayTrigger),
			MoveTemp(FinishTrigger),
		};
	}

	TUniquePtr<IOperator> FAsyncMetaSoundBuilder::BuildGraphOperator(TArray<FAudioParameter>&& InParameters, FBuildResults& OutBuildResults) const
	{
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(TEXT("AsyncMetaSoundBuilder::BuildGraphOperator"));
		using namespace Frontend;

		// Set input data based on the input parameters and the input interface
		const FInputVertexInterface& InputInterface = InitParams.Graph->GetVertexInterface().GetInputInterface();
		FInputVertexInterfaceData InputData(InputInterface);

		IDataTypeRegistry& DataRegistry = IDataTypeRegistry::Get();
		for (FAudioParameter& Parameter : InParameters)
		{
			const FName ParamName = Parameter.ParamName;
			if (const FInputDataVertex* InputVertex = InputInterface.Find(ParamName))
			{
				FLiteral Literal = Frontend::ConvertParameterToLiteral(MoveTemp(Parameter));
				TOptional<FAnyDataReference> DataReference = DataRegistry.CreateDataReference(InputVertex->DataTypeName, EDataReferenceAccessType::Value, Literal, InitParams.OperatorSettings);

				if (DataReference)
				{
					InputData.BindVertex(ParamName, *DataReference);
				}
				else if(MetaSoundParameterEnableWarningOnIgnoredParameterCVar)
				{
					UE_LOG(LogMetaSound, Warning, TEXT("Failed to create initial input data reference from parameter %s of type %s on graph in MetaSoundSource [%s]"), *ParamName.ToString(), *InputVertex->DataTypeName.ToString(), *InitParams.MetaSoundName);
				}
			}
			else if(MetaSoundParameterEnableWarningOnIgnoredParameterCVar)
			{
				UE_LOG(LogMetaSound, Warning, TEXT("Failed to set initial input parameter %s on graph in MetaSoundSource [%s]"), *ParamName.ToString(), *InitParams.MetaSoundName);
			}

		}

		// Reset as elements in array have been moved.
		InParameters.Reset();

		// Create an instance of the new graph
		FBuildGraphOperatorParams BuildParams { *InitParams.Graph, InitParams.OperatorSettings, InputData, InitParams.Environment };
		return FOperatorBuilder(InitParams.BuilderSettings).BuildGraphOperator(BuildParams, OutBuildResults);
	}

	TUniquePtr<Frontend::FGraphAnalyzer> FAsyncMetaSoundBuilder::BuildGraphAnalyzer(TMap<FGuid, FDataReferenceCollection>&& InInternalDataReferences) const
	{
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(TEXT("AsyncMetaSoundBuilder::BuildGraphAnalyzer"));
		using namespace Frontend;

		if (InitParams.BuilderSettings.bPopulateInternalDataReferences)
		{
			const uint64 InstanceID = InitParams.Environment.GetValue<uint64>(SourceInterface::Environment::TransmitterID);
			return MakeUnique<FGraphAnalyzer>(InitParams.OperatorSettings, InstanceID, MoveTemp(InInternalDataReferences));
		}
		return nullptr;
	}

	void FAsyncMetaSoundBuilder::LogBuildErrors(const FBuildResults& InBuildResults) const
	{
		// Log build errors
		for (const IOperatorBuilder::FBuildErrorPtr& Error : InBuildResults.Errors)
		{
			if (Error.IsValid())
			{
				UE_LOG(LogMetaSound, Warning, TEXT("MetasoundSource [%s] build error [%s] \"%s\""), *InitParams.MetaSoundName, *(Error->GetErrorType().ToString()), *(Error->GetErrorDescription().ToString()));
			}
		}
	}

	TArray<FAudioBufferReadRef> FAsyncMetaSoundBuilder::FindOutputAudioBuffers(const FVertexInterfaceData& InVertexData) const
	{
		TArray<FAudioBufferReadRef> OutputBuffers;

		const FOutputVertexInterfaceData& OutputVertexData = InVertexData.GetOutputs();

		// Get output audio buffers.
		for (const FVertexName& AudioOutputName : InitParams.AudioOutputNames)
		{
			if (!OutputVertexData.IsVertexBound(AudioOutputName))
			{
				UE_LOG(LogMetaSound, Warning, TEXT("MetasoundSource [%s] does not contain audio output [%s] in output"), *InitParams.MetaSoundName, *AudioOutputName.ToString());
			}
			OutputBuffers.Add(OutputVertexData.GetOrConstructDataReadReference<FAudioBuffer>(AudioOutputName, InitParams.OperatorSettings));
		}

		return OutputBuffers;
	}

	FMetasoundGenerator::FMetasoundGenerator(FMetasoundGeneratorInitParams&& InParams)
		: MetasoundName(InParams.MetaSoundName)
		, bIsFinishTriggered(false)
		, bIsFinished(false)
		, NumChannels(0)
		, NumFramesPerExecute(0)
		, NumSamplesPerExecute(0)
		, OnPlayTriggerRef(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
		, OnFinishedTriggerRef(FTriggerWriteRef::CreateNew(InParams.OperatorSettings))
		, bPendingGraphTrigger(true)
		, bIsNewGraphPending(false)
		, bIsWaitingForFirstGraph(true)
	{
		NumChannels = InParams.AudioOutputNames.Num();
		NumFramesPerExecute = InParams.OperatorSettings.GetNumFramesPerBlock();
		NumSamplesPerExecute = NumChannels * NumFramesPerExecute;

		// Create the routing for parameter packs
		ParameterPackSendAddress = UMetasoundParameterPack::CreateSendAddressFromEnvironment(InParams.Environment);
		ParameterPackReceiver = FDataTransmissionCenter::Get().RegisterNewReceiver<FMetasoundParameterStorageWrapper>(ParameterPackSendAddress, FReceiverInitParams{ InParams.OperatorSettings });

		BuilderTask = MakeUnique<FBuilderTask>(this, MoveTemp(InParams), true /* bTriggerGenerator */);
		
		if (Metasound::ConsoleVariables::bEnableAsyncMetaSoundGeneratorBuilder)
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
			bIsWaitingForFirstGraph = false;
		}
	}

	FMetasoundGenerator::~FMetasoundGenerator()
	{
		if (BuilderTask.IsValid())
		{
			BuilderTask->EnsureCompletion();
			BuilderTask = nullptr;
		}

		// Remove routing for parameter packs
		ParameterPackReceiver.Reset();
		FDataTransmissionCenter::Get().UnregisterDataChannelIfUnconnected(ParameterPackSendAddress);
	}

	void FMetasoundGenerator::SetPendingGraph(FMetasoundGeneratorData&& InData, bool bTriggerGraph)
	{
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

	void FMetasoundGenerator::SetGraph(TUniquePtr<FMetasoundGeneratorData>&& InData, bool bTriggerGraph)
	{
		if (!InData.IsValid())
		{
			return;
		}

		InterleavedAudioBuffer.Reset();

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

		OnPlayTriggerRef = InData->TriggerOnPlayRef;
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

		if (bTriggerGraph)
		{
			OnPlayTriggerRef->TriggerFrame(0);
		}
	}

	void FMetasoundGenerator::QueueParameterPack(TSharedPtr<FMetasoundParameterPackStorage> ParameterPack)
	{
		ParameterPackQueue.Enqueue(ParameterPack);
	}

	int32 FMetasoundGenerator::GetNumChannels() const
	{
		return NumChannels;
	}

	int32 FMetasoundGenerator::OnGenerateAudio(float* OutAudio, int32 NumSamplesRemaining)
	{
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("MetasoundGenerator::OnGenerateAudio %s"), *MetasoundName));

		// Defer finishing the metasound generator one block
		if (bIsFinishTriggered)
		{
			bIsFinished = true;
		}

		if (NumSamplesRemaining <= 0)
		{
			return 0;
		}

		const bool bDidUpdateGraph = UpdateGraphIfPending();
		bIsWaitingForFirstGraph = bIsWaitingForFirstGraph && !bDidUpdateGraph;

		// Output silent audio if we're still building a graph
		if (bIsWaitingForFirstGraph)
		{
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

		// If we have any audio left in the internal overflow buffer from 
		// previous calls, write that to the output before generating more audio.
		int32 NumSamplesWritten = FillWithBuffer(OverflowBuffer, OutAudio, NumSamplesRemaining);

		if (NumSamplesWritten > 0)
		{
			NumSamplesRemaining -= NumSamplesWritten;
			OverflowBuffer.RemoveAtSwap(0 /* Index */, NumSamplesWritten /* Count */, false /* bAllowShrinking */);
		}

		while (NumSamplesRemaining > 0)
		{
			UnpackAndTransmitUpdatedParameters();

			// Call metasound graph operator.
			RootExecuter.Execute();

			if (GraphAnalyzer.IsValid())
			{
				GraphAnalyzer->Execute();
			}

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

#if METASOUNDGENERATOR_ENABLE_INVALID_SAMPLE_VALUE_LOGGING
		MetasoundGeneratorPrivate::LogInvalidAudioSampleValues(MetasoundName, GraphOutputAudio);
#endif // #if METASOUNDGENERATOR_ENABLE_INVALID_SAMPLE_VALUE_LOGGING
		

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

	void FMetasoundGenerator::UnpackAndTransmitUpdatedParameters()
	{
		using namespace Frontend;

		TUniqueFunction<void(FMetasoundParameterPackStorage*)> ProcessPack = [&](FMetasoundParameterPackStorage* Pack)
			{
				for (auto Walker = Pack->begin(); Walker != Pack->end(); ++Walker)
				{
					if (const FParameterSetter* ParameterSetter = ParameterSetters.Find(Walker->Name))
					{
						if (ParameterSetter->DataType == Walker->TypeName)
						{
							ParameterSetter->SetParameterWithPayload(Walker->GetPayload());
						}
					}
				}
			};

		// First handle packs that have come from the IAudioParameterInterface system...
		if (ParameterPackReceiver.IsValid())
		{
			FMetasoundParameterStorageWrapper Pack;
			while (ParameterPackReceiver->CanPop())
			{
				ParameterPackReceiver->Pop(Pack);
				ProcessPack(Pack.Get().Get());
			}
		}

		// Now handle packs that came from QueueParameterPack...
		TSharedPtr<FMetasoundParameterPackStorage> QueuedParameterPack;
		while (ParameterPackQueue.Dequeue(QueuedParameterPack))
		{
			ProcessPack(QueuedParameterPack.Get());
		}
	}
} 
