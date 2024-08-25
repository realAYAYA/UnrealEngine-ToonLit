// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundParamHelper.h"
#include "MetasoundSampleCounter.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundVertex.h"
#include "HarmonixMetasound/Common.h"
#include "HarmonixMetasound/DataTypes/MidiClock.h"
#include "HarmonixMetasound/DataTypes/MusicTransport.h"

#define LOCTEXT_NAMESPACE "HarmonixMetaSound"

namespace HarmonixMetasound
{
	using namespace Metasound;

	namespace TransportWavePlayerControllerVertexNames
	{
		METASOUND_PARAM(OutputStartTime, "Start Time", "Time into the wave asset to start (seek) the wave asset.")
	}

	class FTransportWavePlayerControllerOperator : public TExecutableOperator<FTransportWavePlayerControllerOperator>, public FMusicTransportControllable
	{
	public:
		FTransportWavePlayerControllerOperator(const FOperatorSettings& InSettings,
									const FMusicTransportEventStreamReadRef& InTransport,
									const FMidiClockReadRef& InMidiClock) :
			FMusicTransportControllable(EMusicPlayerTransportState::Prepared),
			TransportInPin(InTransport),
			MidiClockInPin(InMidiClock),
			PlayOutPin(FTriggerWriteRef::CreateNew(InSettings)),
			StopOutPin(FTriggerWriteRef::CreateNew(InSettings)),
			StartTimeOutPin(FTimeWriteRef::CreateNew()),
			BlockSizeFrames(InSettings.GetNumFramesPerBlock()),
			SampleRate(InSettings.GetSampleRate())
		{
		}

		static const FVertexInterface& GetVertexInterface()
		{
			using namespace CommonPinNames;
			using namespace TransportWavePlayerControllerVertexNames;

			static const FVertexInterface Interface(
				FInputVertexInterface(
					TInputDataVertex<FMusicTransportEventStream>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::Transport)),
					TInputDataVertex<FMidiClock>(METASOUND_GET_PARAM_NAME_AND_METADATA(Inputs::MidiClock))
				),
				FOutputVertexInterface(
					TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(Outputs::TransportPlay)),
					TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(Outputs::TransportStop)),
					TOutputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputStartTime))
				)
			);

			return Interface;
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto InitNodeInfo = []() -> FNodeClassMetadata
			{
				FNodeClassMetadata Info;
				Info.ClassName = { HarmonixNodeNamespace, TEXT("TransportWavePlayerController"), TEXT("") };
				Info.MajorVersion = 0;
				Info.MinorVersion = 1;
				Info.DisplayName = METASOUND_LOCTEXT("TransportWavePlayerControllerNode_DisplayName", "Music Transport Wave Player Controller");
				Info.Description = METASOUND_LOCTEXT("TransportWavePlayerControllerNode_Description", "An interface between a music transport and a wave player");
				Info.Author = PluginAuthor;
				Info.PromptIfMissing = PluginNodeMissingPrompt;
				Info.DefaultInterface = GetVertexInterface();
				Info.CategoryHierarchy = { MetasoundNodeCategories::Harmonix, NodeCategories::Generators };
				return Info;
			};

			static const FNodeClassMetadata Info = InitNodeInfo();

			return Info;
		}

		virtual void BindInputs(FInputVertexInterfaceData& InVertexData) override
		{
			using namespace CommonPinNames;

			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::Transport), TransportInPin);
			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Inputs::MidiClock), MidiClockInPin);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InVertexData) override
		{
			using namespace CommonPinNames;
			using namespace TransportWavePlayerControllerVertexNames;

			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Outputs::TransportPlay), PlayOutPin);
			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(Outputs::TransportStop), StopOutPin);
			InVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputStartTime), StartTimeOutPin);
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

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			using namespace CommonPinNames;

			const FInputVertexInterfaceData& InputData = InParams.InputData;
			FMusicTransportEventStreamReadRef InTransport = InputData.GetOrConstructDataReadReference<FMusicTransportEventStream>(METASOUND_GET_PARAM_NAME(Inputs::Transport), InParams.OperatorSettings);
			FMidiClockReadRef InMidiClock = InputData.GetOrConstructDataReadReference<FMidiClock>(METASOUND_GET_PARAM_NAME(Inputs::MidiClock), InParams.OperatorSettings);
			
			return MakeUnique<FTransportWavePlayerControllerOperator>(InParams.OperatorSettings, InTransport, InMidiClock);
		}

		void Execute()
		{
			// advance the outputs
			PlayOutPin->AdvanceBlock();
			StopOutPin->AdvanceBlock();

			TransportSpanProcessor TransportHandler = [this](int32 StartFrameIndex, int32 EndFrameIndex, EMusicPlayerTransportState CurrentState)
			{
				switch (CurrentState)
				{
				case EMusicPlayerTransportState::Invalid:
				case EMusicPlayerTransportState::Preparing:
					return EMusicPlayerTransportState::Prepared;

				case EMusicPlayerTransportState::Prepared:
					return EMusicPlayerTransportState::Prepared;

				case EMusicPlayerTransportState::Starting:
					if (!ReceivedSeekWhileStopped())
					{
						// Play from the beginning if we haven't received a seek call while we were stopped...
						*StartTimeOutPin = FTime();
					}
					PlayOutPin->TriggerFrame(StartFrameIndex);
					bPlaying = true;
					return EMusicPlayerTransportState::Playing;

				case EMusicPlayerTransportState::Playing:
					return EMusicPlayerTransportState::Playing;

				case EMusicPlayerTransportState::Seeking:
					if (ReceivedSeekWhileStopped())
					{
						// Assumes the MidiClock is stopped for the remainder of the block.
						*StartTimeOutPin = FTime(MidiClockInPin->GetCurrentHiResMs() * 0.001f);
					}
					else
					{
						StopOutPin->TriggerFrame(StartFrameIndex);
						int32 PlayFrameIndex = FMath::Min(StartFrameIndex + 1, EndFrameIndex);

						// Assumes the MidiClock is playing for the remainder of the block.
						*StartTimeOutPin = FTime(MidiClockInPin->GetCurrentHiResMs() * 0.001f - (BlockSizeFrames - PlayFrameIndex) / SampleRate);
						PlayOutPin->TriggerFrame(PlayFrameIndex);
					}
					// Here we will return that we want to be in the same state we were in before this request to 
					// seek since we can seek "instantaneously"...
					return GetTransportState();

				case EMusicPlayerTransportState::Continuing:
					// Assumes the StartTimeOutPin won't change for the remainder of the block.
					PlayOutPin->TriggerFrame(StartFrameIndex);
					bPlaying = true;
					return EMusicPlayerTransportState::Playing;

				case EMusicPlayerTransportState::Pausing:
					bPlaying = false;
					StopOutPin->TriggerFrame(StartFrameIndex);

					// Assumes the MidiClock is paused for the remainder of the block.
					*StartTimeOutPin = FTime(MidiClockInPin->GetCurrentHiResMs() * 0.001f);
					return EMusicPlayerTransportState::Paused;

				case EMusicPlayerTransportState::Paused:
					return EMusicPlayerTransportState::Paused;

				case EMusicPlayerTransportState::Stopping:
				case EMusicPlayerTransportState::Killing:
					if (bPlaying)
					{
						bPlaying = false;
						StopOutPin->TriggerFrame(StartFrameIndex);
					}
					*StartTimeOutPin = FTime();
					return EMusicPlayerTransportState::Prepared;

				default:
					checkNoEntry();
					return EMusicPlayerTransportState::Invalid;
				}
			};
			ExecuteTransportSpans(TransportInPin, BlockSizeFrames, TransportHandler);
		}

		void Reset(const IOperator::FResetParams& InParams)
		{
			PlayOutPin->Reset();
			StopOutPin->Reset();
			*StartTimeOutPin = FTime();

			BlockSizeFrames = InParams.OperatorSettings.GetNumFramesPerBlock();
			SampleRate = InParams.OperatorSettings.GetSampleRate();

			bPlaying = false;
		}

	private:

		// Inputs
		FMusicTransportEventStreamReadRef TransportInPin;
		FMidiClockReadRef MidiClockInPin;

		// Outputs
		FTriggerWriteRef PlayOutPin;
		FTriggerWriteRef StopOutPin;
		FTimeWriteRef StartTimeOutPin;

		int32 BlockSizeFrames;
		float SampleRate;

		bool bPlaying = false;
	};

	class FTransportWavePlayerControllerNode : public FNodeFacade
	{
	public:
		FTransportWavePlayerControllerNode(const FNodeInitData& InitData)
			: FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FTransportWavePlayerControllerOperator>())
		{
		}
	};

	METASOUND_REGISTER_NODE(FTransportWavePlayerControllerNode);
}

#undef LOCTEXT_NAMESPACE
