// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioDevice.h"
#include "Sound/AudioBus.h"
#include "AudioBusSubsystem.h"
#include "DSP/ConvertDeinterleave.h"
#include "Internationalization/Text.h"
#include "MetasoundAudioBuffer.h"
#include "MetasoundAudioBus.h"
#include "MetasoundEngineNodesNames.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundStandardNodesCategories.h"

#define LOCTEXT_NAMESPACE "MetasoundAudioBusNode"

static int32 AudioBusReaderNodePatchWaitTimeout = 3;
FAutoConsoleVariableRef CVarAudioBusReaderNodePatchWaitTimeout(
	TEXT("au.BusReaderPatchWaitTimeout"),
	AudioBusReaderNodePatchWaitTimeout,
	TEXT("The maximum amount of time the audio bus reader node will wait for its patch output to receive samples."),
	ECVF_Default);

namespace Metasound
{
	namespace AudioBusReaderNode
	{
		METASOUND_PARAM(InParamAudioBusInput, "Audio Bus", "Audio Bus Asset.")

		METASOUND_PARAM(OutParamAudio, "Out {0}", "Audio bus output for channel {0}.");
	}

	template <uint32 NumChannels>
	class TAudioBusReaderOperator : public TExecutableOperator<TAudioBusReaderOperator<NumChannels>>
	{
	public:

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto InitNodeInfo = []() -> FNodeClassMetadata
			{
				FName OperatorName = *FString::Printf(TEXT("Audio Bus Reader (%d)"), NumChannels);
				FText NodeDisplayName = METASOUND_LOCTEXT_FORMAT("AudioBusReaderDisplayNamePattern", "Audio Bus Reader ({0})", NumChannels);
				
				FNodeClassMetadata Info;
				Info.ClassName = { EngineNodes::Namespace, OperatorName, TEXT("") };
				Info.MajorVersion = 1;
				Info.MinorVersion = 0;
				Info.DisplayName = NodeDisplayName;
				Info.Description = METASOUND_LOCTEXT("AudioBusReader_Description", "Outputs audio data from the audio bus asset.");
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
			using namespace AudioBusReaderNode;

			auto CreateVertexInterface = []() -> FVertexInterface
			{
				FInputVertexInterface InputInterface;
				InputInterface.Add(TInputDataVertex<FAudioBusAsset>(METASOUND_GET_PARAM_NAME_AND_METADATA(InParamAudioBusInput)));

				FOutputVertexInterface OutputInterface;
				for (uint32 i = 0; i < NumChannels; ++i)
				{
					OutputInterface.Add(TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_WITH_INDEX_AND_METADATA(OutParamAudio, i)));
				}

				return FVertexInterface(InputInterface, OutputInterface);
			};
			
			static const FVertexInterface Interface = CreateVertexInterface();
			return Interface;
		}

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			using namespace Frontend;
			
			using namespace AudioBusReaderNode; 
			const FInputVertexInterfaceData& InputData = InParams.InputData;

			bool bHasEnvironmentVars = InParams.Environment.Contains<Audio::FDeviceId>(SourceInterface::Environment::DeviceID);
			bHasEnvironmentVars &= InParams.Environment.Contains<int32>(SourceInterface::Environment::AudioMixerNumOutputFrames);
			
			if (bHasEnvironmentVars)
			{
				FAudioBusAssetReadRef AudioBusIn = InputData.GetOrConstructDataReadReference<FAudioBusAsset>(METASOUND_GET_PARAM_NAME(InParamAudioBusInput));
				return MakeUnique<TAudioBusReaderOperator<NumChannels>>(InParams, AudioBusIn);
			}
			else
			{
				UE_LOG(LogMetaSound, Warning, TEXT("Audio bus reader node requires audio device ID '%s' and audio mixer num output frames '%s' environment variables")
					, *SourceInterface::Environment::DeviceID.ToString(), *SourceInterface::Environment::AudioMixerNumOutputFrames.ToString());
				return nullptr;
			}
		}

		TAudioBusReaderOperator(const FBuildOperatorParams& InParams, const FAudioBusAssetReadRef& InAudioBusAsset) : AudioBusAsset(InAudioBusAsset)
		{
			for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
			{
				AudioOutputs.Add(FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings));
			}

			Reset(InParams);
		}

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace AudioBusReaderNode;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InParamAudioBusInput), AudioBusAsset);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace AudioBusReaderNode;
			for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
			{
				InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME_WITH_INDEX(OutParamAudio, ChannelIndex), AudioOutputs[ChannelIndex]);
			}
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
			const int32 BlockSizeFrames = AudioOutputs[0]->Num();
			const int32 NumSamplesToPop = BlockSizeFrames * AudioBusChannels;

			const FAudioBusProxyPtr& BusProxy = AudioBusAsset->GetAudioBusProxy();
			if (BusProxy.IsValid() && BusProxy->AudioBusId != AudioBusId)
			{
				AudioBusPatchOutput.Reset();
			}
			
			if (!AudioBusPatchOutput.IsValid())
			{
				// if environment vars & a valid audio bus have been set since starting, try to create the patch now
				if (SampleRate > 0.f && BusProxy.IsValid())
				{
					CreatePatchOutput(BlockSizeFrames);
					if (!AudioBusPatchOutput.IsValid())
					{
						return;
					}
				}
				else
				{
					return;
				}
			}

			bool bPerformPop = false;

			if (bFirstBlock)
			{
				// Ensure there are enough samples in the patch output to support the maximum metasound executions the mixer requires to fill its output frames before popping begins.
				if (uint32(AudioBusPatchOutput->GetNumSamplesAvailable()) >= FMath::DivideAndRoundUp(AudioMixerOutputFrames, BlockSizeFrames) * BlockSizeFrames * AudioBusChannels)
				{
					bPerformPop = true;

					InterleavedBuffer.Reset();
					InterleavedBuffer.AddUninitialized(NumSamplesToPop);
				}
			}
			else
			{
				// Give input patch samples some time to arrive.
				if (AudioBusPatchOutput->WaitUntilNumSamplesAvailable(NumSamplesToPop, uint32(AudioBusReaderNodePatchWaitTimeout)))
				{
					bPerformPop = true;
				}
			}

			if (bPerformPop)
			{
				bFirstBlock = false;

				// Pop off the interleaved data from the audio bus
				int32 SamplesPopped = AudioBusPatchOutput->PopAudio(InterleavedBuffer.GetData(), NumSamplesToPop, false);
				if (SamplesPopped < NumSamplesToPop)
				{
					UE_LOG(LogMetaSound, Warning, TEXT("Underrun detected in audio bus reader node."));
				}

				const uint32 MinChannels = FMath::Min(NumChannels, AudioBusChannels);
				for (uint32 ChannelIndex = 0; ChannelIndex < MinChannels; ++ChannelIndex)
				{
					float* AudioOutputBufferPtr = AudioOutputs[ChannelIndex]->GetData();
					for (int32 FrameIndex = 0; FrameIndex < BlockSizeFrames; ++FrameIndex)
					{
						AudioOutputBufferPtr[FrameIndex] = InterleavedBuffer[FrameIndex * AudioBusChannels + ChannelIndex];
					}
				}
			}
			else
			{
				UE_CLOG(!bFirstBlock, LogMetaSound, Warning, TEXT("Underrun detected in audio bus reader node."));
			}
		}

		void CreatePatchOutput(const int32 BlockSizeFrames)
		{
			const FAudioBusProxyPtr& AudioBusProxy = AudioBusAsset->GetAudioBusProxy();
			if (AudioBusProxy.IsValid())
			{
				if (FAudioDeviceManager* ADM = FAudioDeviceManager::Get())
				{
					if (FAudioDevice* AudioDevice = ADM->GetAudioDeviceRaw(AudioDeviceId))
					{
						// Start the audio bus in case it's not already started
						AudioBusChannels = static_cast<uint32>(FMath::Min(AudioBusProxy->NumChannels, static_cast<int32>(EAudioBusChannels::MaxChannelCount)));
						AudioBusId = AudioBusProxy->AudioBusId;
						
						const Audio::FAudioBusKey AudioBusKey = Audio::FAudioBusKey(AudioBusId);
						UAudioBusSubsystem* AudioBusSubsystem = AudioDevice->GetSubsystem<UAudioBusSubsystem>();
						
						check(AudioBusSubsystem);
						
						AudioBusSubsystem->StartAudioBus(AudioBusKey, AudioBusChannels, false);

						// Create a bus patch output with enough room for the number of samples we expect and some buffering
						AudioBusPatchOutput = AudioBusSubsystem->AddPatchOutputForAudioBus(AudioBusKey, BlockSizeFrames, AudioBusChannels);
					}
				}
			}

			bFirstBlock = true;
		}
		
		void Reset(const IOperator::FResetParams& InParams)
		{
			using namespace Frontend;
			using namespace AudioBusReaderNode;

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
				UE_LOG(LogMetaSound, Warning, TEXT("Audio bus reader node requires audio device ID '%s' and audio mixer num output frames '%s' environment variables")
					, *SourceInterface::Environment::DeviceID.ToString(), *SourceInterface::Environment::AudioMixerNumOutputFrames.ToString());
			}
			
			for(const FAudioBufferWriteRef& Buffer : AudioOutputs)
			{
				Buffer->Zero();
			}
			
			// Wait until Execute to connect the AudioBusPatchOutput.
			// Otherwise the mixer will start filling the patch before the MetaSound starts.
			AudioBusPatchOutput.Reset();
		}

	private:
		FAudioBusAssetReadRef AudioBusAsset;
		TArray<FAudioBufferWriteRef> AudioOutputs;

		TArray<float> InterleavedBuffer;
		int32 AudioMixerOutputFrames = INDEX_NONE;
		Audio::FDeviceId AudioDeviceId = INDEX_NONE;
		uint32 AudioBusId = 0;
		float SampleRate = 0.0f;
		Audio::FPatchOutputStrongPtr AudioBusPatchOutput;
		TUniquePtr<Audio::IConvertDeinterleave> ConvertDeinterleave;
		Audio::FMultichannelBuffer DeinterleavedBuffer;
		uint32 AudioBusChannels = INDEX_NONE;
		bool bFirstBlock = true;
	};

	template<uint32 NumChannels>
	class TAudioBusReaderNode : public FNodeFacade
	{
	public:
		TAudioBusReaderNode(const FNodeInitData& InitData)
			: FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<TAudioBusReaderOperator<NumChannels>>())
		{
		}
	};

#define REGISTER_AUDIO_BUS_READER_NODE(ChannelCount) \
	using FAudioBusReaderNode_##ChannelCount = TAudioBusReaderNode<ChannelCount>; \
	METASOUND_REGISTER_NODE(FAudioBusReaderNode_##ChannelCount) \

	
	REGISTER_AUDIO_BUS_READER_NODE(1);
	REGISTER_AUDIO_BUS_READER_NODE(2);
	REGISTER_AUDIO_BUS_READER_NODE(4);
	REGISTER_AUDIO_BUS_READER_NODE(6);
	REGISTER_AUDIO_BUS_READER_NODE(8);
}

#undef LOCTEXT_NAMESPACE
