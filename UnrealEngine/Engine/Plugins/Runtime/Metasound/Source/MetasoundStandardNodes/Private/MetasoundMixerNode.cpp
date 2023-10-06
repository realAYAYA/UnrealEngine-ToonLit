// Copyright Epic Games, Inc. All Rights Reserved.
#include "DSP/Dsp.h"
#include "DSP/BufferVectorOperations.h"
#include "DSP/FloatArrayMath.h"

#include "MetasoundAudioBuffer.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundOperatorSettings.h"
#include "MetasoundPrimitives.h"
#include "MetasoundStandardNodesNames.h"
#include "MetasoundTrigger.h"
#include "MetasoundVertex.h"
#include "MetasoundStandardNodesCategories.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_MixerNode"


namespace Metasound
{
# pragma region Operator Declaration
	template<uint32 NumInputs, uint32 NumChannels>
	class TAudioMixerNodeOperator : public TExecutableOperator<TAudioMixerNodeOperator<NumInputs, NumChannels>>
	{
	public:
		// ctor
		TAudioMixerNodeOperator(const FCreateOperatorParams& InParams, const TArray<FAudioBufferReadRef>&& InInputBuffers, const TArray<FFloatReadRef>&& InGainValues)
			: Gains(InGainValues)
			, Inputs (InInputBuffers)
		{
			// create write refs
			for (uint32 i = 0; i < NumChannels; ++i)
			{
				Outputs.Add(FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings));
			}

			// init previous gains to current values
			PrevGains.Reset();
			PrevGains.AddUninitialized(NumInputs);

			Reset(InParams);
		}

		// dtor
		virtual ~TAudioMixerNodeOperator() = default;

		static const FVertexInterface& GetDefaultInterface()
		{
			auto CreateDefaultInterface = []()-> FVertexInterface
			{
				// inputs
				FInputVertexInterface InputInterface;
				for (uint32 InputIndex = 0; InputIndex < NumInputs; ++InputIndex)
				{
					// audio channels
					for (uint32 ChanIndex = 0; ChanIndex < NumChannels; ++ChanIndex)
					{
#if WITH_EDITOR
						const FDataVertexMetadata AudioInputMetadata
						{
							GetAudioInputDescription(InputIndex, ChanIndex),
							GetAudioInputDisplayName(InputIndex, ChanIndex)
						};
#else 
						const FDataVertexMetadata AudioInputMetadata;
#endif // WITH_EDITOR
						InputInterface.Add(TInputDataVertex<FAudioBuffer>(GetAudioInputName(InputIndex, ChanIndex), AudioInputMetadata));
					}

					// gain scalar
#if WITH_EDITOR
					FDataVertexMetadata GainPinMetaData
					{
						GetGainInputDescription(InputIndex),
						GetGainInputDisplayName(InputIndex)
					};
#else 
					FDataVertexMetadata GainPinMetaData;
#endif // WITH_EDITOR
					TInputDataVertex<float>GainVertexModel(GetGainInputName(InputIndex), GainPinMetaData, 1.0f);

					InputInterface.Add(GainVertexModel);
				}

				// outputs
				FOutputVertexInterface OutputInterface;
				for (uint32 i = 0; i < NumChannels; ++i)
				{
#if WITH_EDITOR
					const FDataVertexMetadata AudioOutputMetadata
					{
						GetAudioOutputDescription(i),
						GetAudioOutputDisplayName(i)
					};
#else 
					const FDataVertexMetadata AudioOutputMetadata;
#endif // WITH_EDITOR
					OutputInterface.Add(TOutputDataVertex<FAudioBuffer>(GetAudioOutputName(i), AudioOutputMetadata));
				}

				return FVertexInterface(InputInterface, OutputInterface);
			}; // end lambda: CreateDefaultInterface()

			static const FVertexInterface DefaultInterface = CreateDefaultInterface();
			return DefaultInterface;
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			// used if NumChannels == 1
			auto CreateNodeClassMetadataMono = []() -> FNodeClassMetadata
			{
				FName OperatorName = *FString::Printf(TEXT("Audio Mixer (Mono, %d)"), NumInputs);
				FText NodeDisplayName = METASOUND_LOCTEXT_FORMAT("MonoMixer", "Mono Mixer ({0})", NumInputs);
				const FText NodeDescription = METASOUND_LOCTEXT("MixerDescription1", "Will scale input channels by their corresponding gain value and sum them together.");
				FVertexInterface NodeInterface = GetDefaultInterface();

				return CreateNodeClassMetadata(OperatorName, NodeDisplayName, NodeDescription, NodeInterface);
			};

			// used if NumChannels == 2
			auto CreateNodeClassMetadataStereo = []() -> FNodeClassMetadata
			{
				FName OperatorName = *FString::Printf(TEXT("Audio Mixer (Stereo, %d)"), NumInputs);
				FText NodeDisplayName = METASOUND_LOCTEXT_FORMAT("StereoMixer", "Stereo Mixer ({0})", NumInputs);
				const FText NodeDescription = METASOUND_LOCTEXT("MixerDescription2", "Will scale input channels by their corresponding gain value and sum them together.");
				FVertexInterface NodeInterface = GetDefaultInterface();

				return  CreateNodeClassMetadata(OperatorName, NodeDisplayName, NodeDescription, NodeInterface);
			};

			// used if NumChannels > 2
			auto CreateNodeClassMetadataMultiChan = []() -> FNodeClassMetadata
			{
				FName OperatorName = *FString::Printf(TEXT("Audio Mixer (%d-Channel, %d)"), NumChannels, NumInputs);
				FText NodeDisplayName = METASOUND_LOCTEXT_FORMAT("NChannelMixer", "{0}-channel Mixer ({1})", NumChannels, NumInputs);
				const FText NodeDescription = METASOUND_LOCTEXT("MixerDescription3", "Will scale input audio by their corresponding gain value and sum them together.");
				FVertexInterface NodeInterface = GetDefaultInterface();

				return  CreateNodeClassMetadata(OperatorName, NodeDisplayName, NodeDescription, NodeInterface);
			};

			static const FNodeClassMetadata Metadata = (NumChannels == 1)? CreateNodeClassMetadataMono()
														: (NumChannels == 2)? CreateNodeClassMetadataStereo() : CreateNodeClassMetadataMultiChan();
			return Metadata;
		}

		static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, TArray<TUniquePtr<IOperatorBuildError>>& OutErrors)
		{
			const FInputVertexInterface& InputInterface = InParams.Node.GetVertexInterface().GetInputInterface();
			const FDataReferenceCollection& InputCollection = InParams.InputDataReferences;

			TArray<FAudioBufferReadRef> InputBuffers;
			TArray<FFloatReadRef> InputGains;

			for (uint32 i = 0; i < NumInputs; ++i)
			{
				for (uint32 Chan = 0; Chan < NumChannels; ++Chan)
				{
					InputBuffers.Add(InputCollection.GetDataReadReferenceOrConstruct<FAudioBuffer>(GetAudioInputName(i, Chan), InParams.OperatorSettings));
				}

				InputGains.Add(InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, GetGainInputName(i), InParams.OperatorSettings));
			}

			return MakeUnique<TAudioMixerNodeOperator<NumInputs, NumChannels>>(InParams, MoveTemp(InputBuffers), MoveTemp(InputGains));
		}

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			for (uint32 i = 0; i < NumInputs; ++i)
			{
				for (uint32 Chan = 0; Chan < NumChannels; ++Chan)
				{
					InOutVertexData.BindReadVertex(GetAudioInputName(i, Chan), Inputs[i * NumChannels + Chan]);
				}

				InOutVertexData.BindReadVertex(GetGainInputName(i), Gains[i]);
			}
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			for (uint32 i = 0; i < NumChannels; ++i)
			{
				InOutVertexData.BindReadVertex(GetAudioOutputName(i), Outputs[i]);
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

		void Reset(const IOperator::FResetParams& InParams)
		{
			for (uint32 i = 0; i < NumInputs; ++i)
			{
				PrevGains[i] = *Gains[i];
			}

			Execute();
		}

		void Execute()
		{
			// zero the outputs
			for (uint32 i = 0; i < NumChannels; ++i)
			{
				Outputs[i]->Zero();
			}

			// for each input
			for (uint32 InputIndex = 0; InputIndex < NumInputs; ++InputIndex)
			{
				const float NextGain = *Gains[InputIndex];
				const float PrevGain = PrevGains[InputIndex];

				// for each channel of audio
				for (uint32 ChanIndex = 0; ChanIndex < NumChannels; ++ChanIndex)
				{
					// Outputs[Chan] += Gains[i] * Inputs[i][Chan]
					Audio::ArrayMixIn(*Inputs[InputIndex * NumChannels + ChanIndex], *Outputs[ChanIndex], PrevGain, NextGain);
				}

				PrevGains[InputIndex] = NextGain;
			}
		}


	private:
		TArray<FFloatReadRef> Gains;
		TArray<FAudioBufferReadRef> Inputs;
		TArray<FAudioBufferWriteRef> Outputs;

		TArray<float> PrevGains;

		static FNodeClassMetadata CreateNodeClassMetadata(const FName& InOperatorName, const FText& InDisplayName, const FText& InDescription, const FVertexInterface& InDefaultInterface)
		{
			FNodeClassMetadata Metadata
			{
				FNodeClassName { "AudioMixer", InOperatorName, FName() },
				1, // Major Version
				0, // Minor Version
				InDisplayName,
				InDescription,
				PluginAuthor,
				PluginNodeMissingPrompt,
				InDefaultInterface,
				{ NodeCategories::Mix },
				{ METASOUND_LOCTEXT("Metasound_AudioMixerKeyword", "Mixer") },
				FNodeDisplayStyle{}
			};

			return Metadata;
		}

#pragma region Name Gen
		static const FVertexName GetAudioInputName(uint32 InputIndex, uint32 ChannelIndex)
		{
			if (NumChannels == 1)
			{
				return *FString::Printf(TEXT("In %i"), InputIndex);
			}
			else if (NumChannels == 2)
			{
				return *FString::Printf(TEXT("In %i %s"), InputIndex, (ChannelIndex == 0) ? TEXT("L") : TEXT("R"));
			}

			return *FString::Printf(TEXT("In %i, %i"), InputIndex, ChannelIndex);
		}

		static const FVertexName GetGainInputName(uint32 InputIndex)
		{
			return *FString::Printf(TEXT("Gain %i"), InputIndex);
		}

		static const FVertexName GetAudioOutputName(uint32 ChannelIndex)
		{
			if (NumChannels == 1)
			{
				return TEXT("Out");
			}
			else if (NumChannels == 2)
			{
				return *FString::Printf(TEXT("Out %s"), (ChannelIndex == 0) ? TEXT("L") : TEXT("R"));
			}

			return *FString::Printf(TEXT("Out %i"), ChannelIndex);
		}

#if WITH_EDITOR
		static const FText GetAudioInputDescription(uint32 InputIndex, uint32 ChannelIndex)
		{
			return METASOUND_LOCTEXT_FORMAT("AudioMixerAudioInputDescription", "Audio Input #: {0}, Channel: {1}", InputIndex, ChannelIndex);
		}

		static const FText GetAudioInputDisplayName(uint32 InputIndex, uint32 ChannelIndex)
		{
			if (NumChannels == 1)
			{
				return METASOUND_LOCTEXT_FORMAT("AudioMixerAudioInput1In", "In {0}", InputIndex);
			}
			else if (NumChannels == 2)
			{
				if (ChannelIndex == 0)
				{
					return METASOUND_LOCTEXT_FORMAT("AudioMixerAudioInput2InL", "In {0} L", InputIndex);
				}
				else
				{
					return METASOUND_LOCTEXT_FORMAT("AudioMixerAudioInput2InR", "In {0} R", InputIndex);
				}
			}
			return METASOUND_LOCTEXT_FORMAT("AudioMixerAudioInputIn", "In {0}, {0}", InputIndex, ChannelIndex);
		}

		static const FText GetGainInputDisplayName(uint32 InputIndex)
		{
			return METASOUND_LOCTEXT_FORMAT("AudioMixerGainInputDisplayName", "Gain {0} (Lin)", InputIndex);
		}

		static const FText GetGainInputDescription(uint32 InputIndex)
		{
			return METASOUND_LOCTEXT_FORMAT("AudioMixerGainInputDescription", "Gain Input #: {0}", InputIndex);
		}

		static const FText GetAudioOutputDisplayName(uint32 ChannelIndex)
		{
			if (NumChannels == 1)
			{
				return METASOUND_LOCTEXT("AudioMixerAudioOutput1Out", "Out");
			}
			else if (NumChannels == 2)
			{
				if (ChannelIndex == 0)
				{
					return METASOUND_LOCTEXT("AudioMixerAudioOutput2OutL", "Out L");
				}
				else
				{
					return METASOUND_LOCTEXT("AudioMixerAudioOutput2OutR", "Out R");
				}
			}
			return METASOUND_LOCTEXT_FORMAT("AudioMixerAudioOutputOut", "Out {0}", ChannelIndex);
		}

		static const FText GetAudioOutputDescription(uint32 ChannelIndex)
		{
			return METASOUND_LOCTEXT_FORMAT("AudioMixerAudioOutputDescription", "Summed output for channel: {0}", ChannelIndex);
		}
#endif // WITH_EDITOR
#pragma endregion
	}; // class TAudioMixerNodeOperator
#pragma endregion


#pragma region Node Definition
	template<uint32 NumInputs, uint32 NumChannels>
	class METASOUNDSTANDARDNODES_API TAudioMixerNode : public FNodeFacade
	{
	public:
		/**
		 * Constructor used by the Metasound Frontend.
		 */
		TAudioMixerNode(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<TAudioMixerNodeOperator<NumInputs, NumChannels>>())
		{}

		virtual ~TAudioMixerNode() = default;
	};
#pragma endregion


#pragma region Node Registration
	#define REGISTER_AUDIOMIXER_NODE(A, B) \
		using FAudioMixerNode_##A ## _ ##B = TAudioMixerNode<A, B>; \
		METASOUND_REGISTER_NODE(FAudioMixerNode_##A ## _ ##B) \


	// mono
	REGISTER_AUDIOMIXER_NODE(2, 1)
	REGISTER_AUDIOMIXER_NODE(3, 1)
	REGISTER_AUDIOMIXER_NODE(4, 1)
	REGISTER_AUDIOMIXER_NODE(5, 1)
	REGISTER_AUDIOMIXER_NODE(6, 1)
	REGISTER_AUDIOMIXER_NODE(7, 1)
	REGISTER_AUDIOMIXER_NODE(8, 1)

	// stereo
 	REGISTER_AUDIOMIXER_NODE(2, 2)
	REGISTER_AUDIOMIXER_NODE(3, 2)
	REGISTER_AUDIOMIXER_NODE(4, 2)
	REGISTER_AUDIOMIXER_NODE(5, 2)
	REGISTER_AUDIOMIXER_NODE(6, 2)
	REGISTER_AUDIOMIXER_NODE(7, 2)
	REGISTER_AUDIOMIXER_NODE(8, 2)

	// test
//	REGISTER_AUDIOMIXER_NODE(8, 6)

#pragma endregion




} // namespace Metasound

#undef LOCTEXT_NAMESPACE // "MetasoundStandardNodes_MixerNode"
