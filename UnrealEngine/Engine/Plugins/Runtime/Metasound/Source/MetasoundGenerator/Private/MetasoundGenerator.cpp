// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundGenerator.h"

#include "Analysis/MetasoundFrontendVertexAnalyzer.h"
#include "AudioParameter.h"
#include "DSP/Dsp.h"
#include "Interfaces/MetasoundFrontendSourceInterface.h"
#include "MetasoundGraph.h"
#include "MetasoundInputNode.h"
#include "MetasoundOperatorBuilder.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundOutputNode.h"
#include "MetasoundRouter.h"
#include "MetasoundTrace.h"
#include "MetasoundTrigger.h"
#include "MetasoundVertexData.h"


namespace Metasound
{
	namespace ConsoleVariables
	{
		static bool bEnableAsyncMetaSoundGeneratorBuilder = true;
	}
}

FAutoConsoleVariableRef CVarMetaSoundEnableAsyncGeneratorBuilder(
	TEXT("au.MetaSound.EnableAsyncGeneratorBuilder"),
	Metasound::ConsoleVariables::bEnableAsyncMetaSoundGeneratorBuilder,
	TEXT("Enables async building of FMetaSoundGenerators\n")
	TEXT("Default: true"),
	ECVF_Default);


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

			// Gather relevant input and output references
			FVertexInterfaceData VertexData(InitParams.Graph->GetVertexInterface());
			GraphOperator->Bind(VertexData);

			// Get inputs
			FTriggerWriteRef PlayTrigger = VertexData.GetInputs().GetOrConstructDataWriteReference<FTrigger>(SourceInterface::Inputs::OnPlay, InitParams.OperatorSettings, false);

			// Get outputs
			TArray<FAudioBufferReadRef> OutputBuffers = FindOutputAudioBuffers(VertexData);
			FTriggerReadRef FinishTrigger = TDataReadReferenceFactory<FTrigger>::CreateExplicitArgs(InitParams.OperatorSettings, false);

			if (InitParams.Graph->GetVertexInterface().GetOutputInterface().Contains(SourceOneShotInterface::Outputs::OnFinished))
			{
				// Only attempt to retrieve the on finished trigger if it exists.
				// Attempting to retrieve a data reference from a non-existent vertex 
				// will log an error. 
				FinishTrigger = VertexData.GetOutputs().GetOrConstructDataReadReference<FTrigger>(SourceOneShotInterface::Outputs::OnFinished, InitParams.OperatorSettings, false);
			}

			// Set data needed for graph
			FMetasoundGeneratorData GeneratorData
			{
				InitParams.OperatorSettings,
				MoveTemp(GraphOperator),
				MoveTemp(GraphAnalyzer),
				MoveTemp(OutputBuffers),
				MoveTemp(PlayTrigger),
				MoveTemp(FinishTrigger),
			};

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

	TUniquePtr<IOperator> FAsyncMetaSoundBuilder::BuildGraphOperator(TArray<FAudioParameter>&& InParameters, FBuildResults& OutBuildResults) const
	{
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
				else
				{
					UE_LOG(LogMetaSound, Error, TEXT("Failed to create initial input data reference from parameter %s of type %s on graph in MetaSoundSource [%s]"), *ParamName.ToString(), *InputVertex->DataTypeName.ToString(), *InitParams.MetaSoundName);
				}
			}
			else
			{
				UE_LOG(LogMetaSound, Error, TEXT("Failed to set initial input parameter %s on graph in MetaSoundSource [%s]"), *ParamName.ToString(), *InitParams.MetaSoundName);
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

		if (bTriggerGraph)
		{
			OnPlayTriggerRef->TriggerFrame(0);
		}
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
} 
