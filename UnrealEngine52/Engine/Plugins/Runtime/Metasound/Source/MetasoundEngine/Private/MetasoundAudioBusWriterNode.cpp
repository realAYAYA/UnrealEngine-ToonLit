// Copyright Epic Games, Inc. All Rights Reserved.

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

// MetaSound Audio Bus writer node disabled in UE 5.2 due to existing issues (UE-170575).
#define ENABLE_METASOUND_AUDIOBUS_WRITER_NODE 0

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

		static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors)
		{
			using namespace Frontend;

			using namespace AudioBusWriterNode;
			const FDataReferenceCollection& InputCollection = InParams.InputDataReferences;

			Audio::FDeviceId AudioDeviceId = InParams.Environment.GetValue<Audio::FDeviceId>(SourceInterface::Environment::DeviceID);
			int32 AudioMixerOutputFrames = InParams.Environment.GetValue<int32>(SourceInterface::Environment::AudioMixerNumOutputFrames);

			FAudioBusAssetReadRef AudioBusIn = InputCollection.GetDataReadReferenceOrConstruct<FAudioBusAsset>(METASOUND_GET_PARAM_NAME(InParamAudioBusOutput));

			TArray<FAudioBufferReadRef> AudioInputs;
			for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
			{
				AudioInputs.Add(InputCollection.GetDataReadReferenceOrConstruct<FAudioBuffer>(METASOUND_GET_PARAM_NAME_WITH_INDEX(InParamAudio, ChannelIndex), InParams.OperatorSettings));
			}

			return MakeUnique<TAudioBusWriterOperator<NumChannels>>(InParams, AudioMixerOutputFrames, AudioDeviceId, MoveTemp(AudioBusIn), MoveTemp(AudioInputs));
		}

		TAudioBusWriterOperator(const FCreateOperatorParams& InParams, int32 InAudioMixerOutputFrames, Audio::FDeviceId InAudioDeviceId, FAudioBusAssetReadRef InAudioBusAsset, TArray<FAudioBufferReadRef> InAudioInputs) :
			AudioBusAsset(MoveTemp(InAudioBusAsset)),
			AudioInputs(MoveTemp(InAudioInputs)),
			AudioMixerOutputFrames(InAudioMixerOutputFrames),
			AudioDeviceId(InAudioDeviceId),
			SampleRate(InParams.OperatorSettings.GetSampleRate())
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
						Audio::FAudioBusKey AudioBusKey = Audio::FAudioBusKey(AudioBusProxy->AudioBusId);	
						// Start the audio bus in case it's not already started					
						AudioBusChannels = AudioBusProxy->NumChannels;
						AudioBusSubsystem->StartAudioBus(AudioBusKey, AudioBusChannels, false);

						BlockSizeFrames = InParams.OperatorSettings.GetNumFramesPerBlock();
						InterleavedBuffer.AddZeroed(BlockSizeFrames * AudioBusChannels);

						// Create a bus patch input with enough room for the number of samples we expect and some buffering
						AudioBusPatchInput = AudioBusSubsystem->AddPatchInputForAudioBus(AudioBusKey, BlockSizeFrames, AudioBusChannels);
					}
				}
			}
		}

		virtual ~TAudioBusWriterOperator()
		{
		}

		virtual FDataReferenceCollection GetInputs() const override
		{
			using namespace AudioBusWriterNode;

			FDataReferenceCollection InputDataReferences;

			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InParamAudioBusOutput), AudioBusAsset);

			for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
			{
				InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME_WITH_INDEX(InParamAudio, ChannelIndex), FAudioBufferReadRef(AudioInputs[ChannelIndex]));
			}

			return InputDataReferences;
		}

		virtual FDataReferenceCollection GetOutputs() const override
		{
			using namespace AudioBusWriterNode;

			FDataReferenceCollection OutputDataReferences;
			return OutputDataReferences;
		}

		void Execute()
		{
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
			float* InterleavedBufferPtr = InterleavedBuffer.GetData();

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

			// Pushes the interleaved data to the audio bus
			int32 SamplesPushed = AudioBusPatchInput.PushAudio(InterleavedBuffer.GetData(), InterleavedBuffer.Num());
			if (SamplesPushed < InterleavedBuffer.Num())
			{
				UE_LOG(LogMetaSound, Warning, TEXT("Underrun detected in audio bus writer node."));
			}
		}

	private:
		FAudioBusAssetReadRef AudioBusAsset;
		TArray<FAudioBufferReadRef> AudioInputs;

		TArray<float> InterleavedBuffer;
		int32 AudioMixerOutputFrames = INDEX_NONE;
		Audio::FDeviceId AudioDeviceId = INDEX_NONE;
		float SampleRate = 0.0f;
		Audio::FPatchInput AudioBusPatchInput;
		uint32 AudioBusChannels = INDEX_NONE;
		int32 BlockSizeFrames = 0;
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

#if ENABLE_METASOUND_AUDIOBUS_WRITER_NODE
	REGISTER_AUDIO_BUS_WRITER_NODE(1);
	REGISTER_AUDIO_BUS_WRITER_NODE(2);
	REGISTER_AUDIO_BUS_WRITER_NODE(4);
	REGISTER_AUDIO_BUS_WRITER_NODE(6);
	REGISTER_AUDIO_BUS_WRITER_NODE(8);
#endif // #if ENABLE_METASOUND_AUDIOBUS_WRITER_NODE

}

#undef LOCTEXT_NAMESPACE
