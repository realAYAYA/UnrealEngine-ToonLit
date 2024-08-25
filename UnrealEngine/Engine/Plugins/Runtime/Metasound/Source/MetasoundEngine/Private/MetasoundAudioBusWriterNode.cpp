// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioMixerDevice.h"
#include "AudioBusSubsystem.h"
#include "AudioDevice.h"
#include "Internationalization/Text.h"
#include "MediaPacket.h"
#include "MetasoundAudioBuffer.h"
#include "MetasoundAudioBus.h"
#include "MetasoundEngineNodesNames.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundStandardNodesCategories.h"

#define LOCTEXT_NAMESPACE "MetasoundAudioBusWriterNode"

namespace Metasound
{
	namespace AudioBusWriterNode
	{
		METASOUND_PARAM(InParamAudioBusOutput, "Audio Bus", "Audio Bus Asset.");
		METASOUND_PARAM(InParamAudio, "In {0}", "Audio input for channel {0}.");
	}

	template<uint32 NumChannels>
	class TAudioBusWriterOperator : public TExecutableOperator<TAudioBusWriterOperator<NumChannels>>
	{
	public:
		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto InitNodeInfo = []() -> FNodeClassMetadata
			{
				FName OperatorName = *FString::Printf(TEXT("Audio Bus Writer (%d)"), NumChannels);
				FText NodeDisplayName = METASOUND_LOCTEXT_FORMAT("AudioBusWriterDisplayNamePattern", "Audio Bus Writer ({0})", NumChannels);

				FNodeClassMetadata Info;
				Info.ClassName = { EngineNodes::Namespace, OperatorName, TEXT("") };
				Info.MajorVersion = 1;
				Info.MinorVersion = 0;
				Info.DisplayName = NodeDisplayName;
				Info.Description = METASOUND_LOCTEXT("AudioBusWriter_Description", "Sends audio data to the audio bus asset.");
				Info.Author = PluginAuthor;
				Info.PromptIfMissing = PluginNodeMissingPrompt;
				Info.DefaultInterface = GetVertexInterface();
				Info.CategoryHierarchy.Emplace(NodeCategories::Io);
				return Info;
			};

			static const FNodeClassMetadata Info = InitNodeInfo();

			return Info;
		}

		static const FVertexInterface& GetVertexInterface()
		{
			using namespace AudioBusWriterNode;

			auto CreateVertexInterface = []() -> FVertexInterface
			{
				FInputVertexInterface InputInterface;
				InputInterface.Add(TInputDataVertex<FAudioBusAsset>(METASOUND_GET_PARAM_NAME_AND_METADATA(InParamAudioBusOutput)));
				for (uint32 i = 0; i < NumChannels; ++i)
				{
					InputInterface.Add(TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_WITH_INDEX_AND_METADATA(InParamAudio, i)));
				}

				FOutputVertexInterface OutputInterface;

				return FVertexInterface(InputInterface, OutputInterface);
			};

			static const FVertexInterface Interface = CreateVertexInterface();
			return Interface;
		}

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			using namespace Frontend;
			using namespace AudioBusWriterNode;

			const FInputVertexInterfaceData& InputData = InParams.InputData;

			bool bHasEnvironmentVars = InParams.Environment.Contains<Audio::FDeviceId>(SourceInterface::Environment::DeviceID);
			bHasEnvironmentVars &= InParams.Environment.Contains<int32>(SourceInterface::Environment::AudioMixerNumOutputFrames);

			if (bHasEnvironmentVars)
			{
				FAudioBusAssetReadRef AudioBusIn = InputData.GetOrConstructDataReadReference<FAudioBusAsset>(METASOUND_GET_PARAM_NAME(InParamAudioBusOutput));

				TArray<FAudioBufferReadRef> AudioInputs;
				for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
				{
					AudioInputs.Add(InputData.GetOrConstructDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME_WITH_INDEX(InParamAudio, ChannelIndex), InParams.OperatorSettings));
				}

				FString GraphName;
				if (InParams.Environment.Contains<FString>(SourceInterface::Environment::GraphName))
				{
					GraphName = InParams.Environment.GetValue<FString>(SourceInterface::Environment::GraphName);
				}
				else
				{
					GraphName = TEXT("<Unknown>");
				}

				return MakeUnique<TAudioBusWriterOperator<NumChannels>>(InParams, MoveTemp(AudioBusIn), MoveTemp(AudioInputs), MoveTemp(GraphName));
			}
			else
			{
				UE_LOG(LogMetaSound, Warning, TEXT("Audio bus writer node requires audio device ID '%s' and audio mixer num output frames '%s' environment variables")
					, *SourceInterface::Environment::DeviceID.ToString(), *SourceInterface::Environment::AudioMixerNumOutputFrames.ToString());
				return nullptr;
			}
		}

		TAudioBusWriterOperator(const FBuildOperatorParams& InParams, FAudioBusAssetReadRef InAudioBusAsset, TArray<FAudioBufferReadRef> InAudioInputs, FString InGraphName)
			: AudioBusAsset(MoveTemp(InAudioBusAsset))
			, AudioInputs(MoveTemp(InAudioInputs))
			, GraphName(MoveTemp(InGraphName))
		{
			Reset(InParams);
		}

		void CreatePatchInput()
		{
			const FAudioBusProxyPtr& AudioBusProxy = AudioBusAsset->GetAudioBusProxy();
			if (AudioBusProxy.IsValid())
			{
				if (FAudioDeviceManager* ADM = FAudioDeviceManager::Get())
				{
					if (FAudioDevice* AudioDevice = ADM->GetAudioDeviceRaw(AudioDeviceId))
					{
						UAudioBusSubsystem* AudioBusSubsystem = AudioDevice->GetSubsystem<UAudioBusSubsystem>();
						check(AudioBusSubsystem);
						
						AudioBusId = AudioBusProxy->AudioBusId;
						const Audio::FAudioBusKey AudioBusKey = Audio::FAudioBusKey(AudioBusId);
						
						// Start the audio bus in case it's not already started					
						AudioBusChannels = AudioBusProxy->NumChannels;
						AudioBusSubsystem->StartAudioBus(AudioBusKey, AudioBusChannels, false);

						InterleavedBuffer.Reset();
						InterleavedBuffer.Reserve(NumBlocksToNumSamples(InitialNumBlocks()));

						// Create a bus patch input with enough room for the number of samples we expect and some buffering
						AudioBusPatchInput = AudioBusSubsystem->AddPatchInputForAudioBus(AudioBusKey, BlockSizeFrames, AudioBusChannels);

						ConnectionState = EConnectionState::Disconnected;
					}
				}
			}
		}
		
		void Reset(const IOperator::FResetParams& InParams)
		{
			using namespace Frontend;
			using namespace AudioBusWriterNode;

			bool bHasEnvironmentVars = InParams.Environment.Contains<Audio::FDeviceId>(SourceInterface::Environment::DeviceID);
			bHasEnvironmentVars &= InParams.Environment.Contains<int32>(SourceInterface::Environment::AudioMixerNumOutputFrames);

			if (bHasEnvironmentVars)
			{
				SampleRate = InParams.OperatorSettings.GetSampleRate();
				AudioDeviceId = InParams.Environment.GetValue<Audio::FDeviceId>(SourceInterface::Environment::DeviceID);
				AudioMixerOutputFrames = InParams.Environment.GetValue<int32>(SourceInterface::Environment::AudioMixerNumOutputFrames);
			}
			else
			{
				UE_LOG(LogMetaSound, Warning, TEXT("Audio bus writer node requires audio device ID '%s' and audio mixer num output frames '%s' environment variables")
					, *SourceInterface::Environment::DeviceID.ToString(), *SourceInterface::Environment::AudioMixerNumOutputFrames.ToString());
			}
			
			BlockSizeFrames = InParams.OperatorSettings.GetNumFramesPerBlock();

			bWasUnderrunReported = false;

			CreatePatchInput();
		}


		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace AudioBusWriterNode;

			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InParamAudioBusOutput), AudioBusAsset);

			for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
			{
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME_WITH_INDEX(InParamAudio, ChannelIndex), AudioInputs[ChannelIndex]);
			}
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
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

		void Execute()
		{
			const FAudioBusProxyPtr& BusProxy = AudioBusAsset->GetAudioBusProxy();
			if (BusProxy.IsValid() && BusProxy->AudioBusId != AudioBusId)
			{
				AudioBusPatchInput.Reset();
			}
			
			if (!AudioBusPatchInput.IsValid())
			{
				// if environment vars & a valid audio bus have been set since starting, try to create the patch now
				if (SampleRate > 0.f && BusProxy.IsValid())
				{
					CreatePatchInput();
				}
				else
				{
					return;
				}
			}

			if (!AudioBusPatchInput.IsOutputStillActive())
			{
				return;
			}

			// Retrieve input and interleaved buffer pointers
			const float* AudioInputBufferPtrs[NumChannels];
			for (uint32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
			{
				AudioInputBufferPtrs[ChannelIndex] = AudioInputs[ChannelIndex]->GetData();
			}
			int32 InterleavedBufferOffset = InterleavedBuffer.Num();
			InterleavedBuffer.SetNum(InterleavedBufferOffset + BlockSizeFrames * NumChannels);
			float* InterleavedBufferPtr = InterleavedBuffer.GetData() + InterleavedBufferOffset;

			if (AudioBusChannels == 1)
			{
				FMemory::Memcpy(InterleavedBufferPtr, AudioInputBufferPtrs[0], BlockSizeFrames * sizeof(float));
			}
			else
			{
				// Interleave the inputs
				// Writing the channels of the interleaved buffer sequentially should improve
				// cache utilization compared to writing each input's frames sequentially.
				// There is more likely to be a cache line for each buffer than for the
				// entirety of the interleaved buffer.
				uint32 MinChannels = FMath::Min(AudioBusChannels, NumChannels);
				for (int32 FrameIndex = 0; FrameIndex < BlockSizeFrames; ++FrameIndex)
				{
					for (uint32 ChannelIndex = 0; ChannelIndex < MinChannels; ++ChannelIndex)
					{
						InterleavedBufferPtr[ChannelIndex] = *AudioInputBufferPtrs[ChannelIndex]++;
					}
					InterleavedBufferPtr += AudioBusChannels;
				}
			}

			switch (ConnectionState)
			{
			case EConnectionState::Disconnected:
				{
					// Wait until InterleavedBuffer contains enough samples to satisfy any mix eventualities!
					// A single mix requires enough MetaSound executions to satisfy its buffer size.
					// Mixing can occur concurrently with the first MetaSound execution intended for the next mix,
					// and can steal that execution's patch output.
					if (InterleavedBuffer.Num() < NumBlocksToNumSamples(InitialNumBlocks()))
					{
						return;
					}

					ConnectionState = EConnectionState::ConnectionPending;
					break;
				}

			case EConnectionState::ConnectionPending:
				{
					int32 InitialNumSamples = NumBlocksToNumSamples(InitialNumBlocks());

					// Determine if the pending connection has been established, by detecting if samples have been consumed.
					if (AudioBusPatchInput.GetNumSamplesAvailable() == InitialNumSamples)
					{
						// If the connection hasn't been established by the time as many executions have occurred again,
						// something has probably gone wrong.
						if (InterleavedBuffer.Num() == InitialNumSamples)
						{
							UE_LOG(LogMetaSound, Warning, TEXT("Graph %s: Writer node executed before mixer patch connection established, with buffer size %d."), *GraphName, InterleavedBuffer.Num());
							return;
						}
						return;
					}

					ConnectionState = EConnectionState::Connected;
					break;
				}
			}

			// Pushes the interleaved data to the audio bus
			const int32 SamplesPushed = AudioBusPatchInput.PushAudio(InterleavedBuffer.GetData(), InterleavedBuffer.Num());
			if (SamplesPushed < InterleavedBuffer.Num() && !bWasUnderrunReported)
			{
				UE_LOG(LogMetaSound, Warning, TEXT("Underrun detected in audio bus writer node."));
				bWasUnderrunReported = true;
			}
			InterleavedBuffer.Reset();
		}

	private:
		int32 InitialNumBlocks() const
		{
			if (AudioMixerOutputFrames == BlockSizeFrames)
			{
				return 1;
			}

			// We need enough blocks for as many executions as the mixer can consume at once, plus one more,
			// because the last execution can be concurrent with the mixer, and could contribute to either the
			// current mix or the next.  That could leave us with one block too few if we didn't have an extra.
			int32 MaxSizeFrames = FMath::Max(AudioMixerOutputFrames, BlockSizeFrames), MinSizeFrames = FMath::Min(AudioMixerOutputFrames, BlockSizeFrames);
			return 1 + FMath::DivideAndRoundUp(MaxSizeFrames, MinSizeFrames);
		}

		int32 NumBlocksToNumSamples(int32 NumBlocks) const
		{
			return NumBlocks * BlockSizeFrames * AudioBusChannels;
		}

		FAudioBusAssetReadRef AudioBusAsset;
		TArray<FAudioBufferReadRef> AudioInputs;
		FString GraphName;

		TArray<float> InterleavedBuffer;
		int32 AudioMixerOutputFrames = INDEX_NONE;
		Audio::FDeviceId AudioDeviceId = INDEX_NONE;
		float SampleRate = 0.0f;
		Audio::FPatchInput AudioBusPatchInput;
		uint32 AudioBusChannels = INDEX_NONE;
		uint32 AudioBusId = 0;
		bool bWasUnderrunReported = false;
		int32 BlockSizeFrames = 0;
		enum class EConnectionState : uint8
		{
			Disconnected,
			ConnectionPending,
			Connected
		}
		ConnectionState = EConnectionState::Disconnected;
	};

	template<uint32 NumChannels>
	class TAudioBusWriterNode : public FNodeFacade
	{
	public:
		TAudioBusWriterNode(const FNodeInitData& InitData)
			: FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<TAudioBusWriterOperator<NumChannels>>())
		{
		}
	};

#define REGISTER_AUDIO_BUS_WRITER_NODE(ChannelCount) \
	using FAudioBusWriterNode_##ChannelCount = TAudioBusWriterNode<ChannelCount>; \
	METASOUND_REGISTER_NODE(FAudioBusWriterNode_##ChannelCount) \

	REGISTER_AUDIO_BUS_WRITER_NODE(1);
	REGISTER_AUDIO_BUS_WRITER_NODE(2);
	REGISTER_AUDIO_BUS_WRITER_NODE(4);
	REGISTER_AUDIO_BUS_WRITER_NODE(6);
	REGISTER_AUDIO_BUS_WRITER_NODE(8);
}

#undef LOCTEXT_NAMESPACE
