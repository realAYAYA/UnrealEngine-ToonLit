// Copyright Epic Games, Inc. All Rights Reserved.

#include "DSP/LinkwitzRileyBandSplitter.h"
#include "Internationalization/Text.h"
#include "MetasoundAudioBuffer.h"
#include "MetasoundEnumRegistrationMacro.h"
#include "MetasoundEnvelopeFollowerTypes.h"
#include "MetasoundDataTypeRegistrationMacro.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundStandardNodesNames.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_Bandsplitter"

namespace Metasound
{
	namespace BandSplitterNode
	{
		METASOUND_PARAM(InputFilterOrder, "Filter Order", "The steepness of the crossover filters.");
		METASOUND_PARAM(InputPhaseCompensate, "Phase Compensate", "Whether or not to phase compensate each band so they can be summed back together correctly.");
	}

	enum class EBandSplitterFilterOrder : int32
	{
		TwoPole,
		FourPole,
		SixPole,
		EightPole
	};

	DECLARE_METASOUND_ENUM(EBandSplitterFilterOrder, EBandSplitterFilterOrder::FourPole, METASOUNDSTANDARDNODES_API,
		FEnumBandSplitterFilterOrder, FEnumBandSplitterFilterOrderInfo, FEnumBandSplitterFilterOrderReadRef, FEnumBandSplitterFilterOrderWriteRef);

	DEFINE_METASOUND_ENUM_BEGIN(EBandSplitterFilterOrder, FEnumBandSplitterFilterOrder, "Filter Order")
		DEFINE_METASOUND_ENUM_ENTRY(EBandSplitterFilterOrder::TwoPole, "BandSplitterFilterOrderTwoPoleDescription", "Two Pole", "EnvelopePeakModeMSDescriptionTT", "Envelope follows a running Mean Squared of the audio signal."),
		DEFINE_METASOUND_ENUM_ENTRY(EBandSplitterFilterOrder::FourPole, "BandSplitterFilterOrderFourPoleDescription", "Four Pole", "EnvelopePeakModeRMSDescriptionTT", "Envelope follows a running Root Mean Squared of the audio signal."),
		DEFINE_METASOUND_ENUM_ENTRY(EBandSplitterFilterOrder::SixPole, "BandSplitterFilterOrderSixPoleDescription", "Six Pole", "EnvelopePeakModePeakDescriptionTT", "Envelope follows the peaks in the audio signal."),
		DEFINE_METASOUND_ENUM_ENTRY(EBandSplitterFilterOrder::EightPole, "BandSplitterFilterOrderEightPoleDescription", "Eight Pole", "EnvelopePeakModePeakDescriptionTT", "Envelope follows the peaks in the audio signal."),
	DEFINE_METASOUND_ENUM_END()

	template<uint32 NumBands, uint32 NumChannels>
	class METASOUNDSTANDARDNODES_API TBandSplitterOperator : public TExecutableOperator<TBandSplitterOperator<NumBands, NumChannels>>
	{
	public:
		TBandSplitterOperator(
			const FOperatorSettings& InSettings
			, const TArray<FAudioBufferReadRef>&& InInputBuffers
			, const TArray<FFloatReadRef>&& InCrossoverFrequencies
			, const FEnumBandSplitterFilterOrderReadRef& InFilterOrder
			, const FBoolReadRef& InPhaseCompensate)
			: AudioInputs(InInputBuffers)
			, CrossoverFrequencies(InCrossoverFrequencies)
			, FilterOrder(InFilterOrder)
			, bPhaseCompensate(InPhaseCompensate)
			, Settings(InSettings)
		{
			Crossovers.Reset();
			Crossovers.AddUninitialized(NumBands - 1);

			for (int32 i = 0; i < InCrossoverFrequencies.Num(); ++i)
			{
				Crossovers[i] = *InCrossoverFrequencies[i];
			}

			bPrevPhaseCompensate = *bPhaseCompensate;
			PrevFilterOrder = GetFilterOrder(*InFilterOrder);

			for (uint32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
			{
				MultiBandBuffers.AddDefaulted();
				Filters.AddDefaulted();

				MultiBandBuffers[ChannelIndex].SetBands(NumBands);
				Filters[ChannelIndex].Init(1, InSettings.GetSampleRate(), PrevFilterOrder, bPrevPhaseCompensate, Crossovers);

				for (uint32 BandIndex = 0; BandIndex < NumBands; ++BandIndex)
				{
					AudioOutputs.Add(FAudioBufferWriteRef::CreateNew(InSettings));
				}
			}
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			// used if NumChannels == 1
			auto CreateNodeClassMetadataMono = []() -> FNodeClassMetadata
			{
				FName OperatorName = *FString::Printf(TEXT("Band Splitter (Mono, %d)"), NumBands);
				FText NodeDisplayName = METASOUND_LOCTEXT_FORMAT("BandSplitterDisplayNamePattern1", "Mono Band Splitter ({0})", NumBands);
				const FText NodeDescription = METASOUND_LOCTEXT("BandSplitterDescription1", "Will split incoming audio into separate frequency bands.");
				FVertexInterface NodeInterface = GetDefaultInterface();

				return CreateNodeClassMetadata(OperatorName, NodeDisplayName, NodeDescription, NodeInterface);
			};

			// used if NumChannels == 2
			auto CreateNodeClassMetadataStereo = []() -> FNodeClassMetadata
			{
				FName OperatorName = *FString::Printf(TEXT("Band Splitter (Stereo, %d)"), NumBands);
				FText NodeDisplayName = METASOUND_LOCTEXT_FORMAT("BandSplitterDisplayNamePattern2", "Stereo Band Splitter ({0})", NumBands);
				const FText NodeDescription = METASOUND_LOCTEXT("BandSplitterDescription2", "Will split incoming audio into separate frequency bands.");
				FVertexInterface NodeInterface = GetDefaultInterface();

				return  CreateNodeClassMetadata(OperatorName, NodeDisplayName, NodeDescription, NodeInterface);
			};

			// used if NumChannels > 2
			auto CreateNodeClassMetadataMultiChan = []() -> FNodeClassMetadata
			{
				FName OperatorName = *FString::Printf(TEXT("Band Splitter (%d-Channel, %d)"), NumChannels, NumBands);
				FText NodeDisplayName = METASOUND_LOCTEXT_FORMAT("BandSplitterDisplayNamePattern3", "{0}-channel Band Splitter ({1})", NumChannels, NumBands);
				const FText NodeDescription = METASOUND_LOCTEXT("BandSplitterDescription3", "Will split incoming audio into separate frequency bands.");
				FVertexInterface NodeInterface = GetDefaultInterface();

				return  CreateNodeClassMetadata(OperatorName, NodeDisplayName, NodeDescription, NodeInterface);
			};

			static const FNodeClassMetadata Metadata = (NumChannels == 1) ? CreateNodeClassMetadataMono()
				: (NumChannels == 2) ? CreateNodeClassMetadataStereo() : CreateNodeClassMetadataMultiChan();

			return Metadata;
		}

		virtual FDataReferenceCollection GetInputs() const override
		{
			using namespace BandSplitterNode;

			FDataReferenceCollection InputPins;

			for (uint32 Chan = 0; Chan < NumChannels; ++Chan)
			{
				InputPins.AddDataReadReference(GetAudioInputName(Chan), AudioInputs[Chan]);
			}

			InputPins.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputFilterOrder), FilterOrder);
			InputPins.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputPhaseCompensate), bPhaseCompensate);

			for (uint32 BandIndex = 0; BandIndex < NumBands - 1; ++BandIndex)
			{
				InputPins.AddDataReadReference(GetCrossoverInputName(BandIndex), CrossoverFrequencies[BandIndex]);
			}

			return InputPins;
		}

		virtual FDataReferenceCollection GetOutputs() const override
		{
			FDataReferenceCollection OutputPins;

			for (uint32 BandIndex = 0; BandIndex < NumBands; ++BandIndex)
			{
				for (uint32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
				{
					OutputPins.AddDataReadReference(GetAudioOutputName(BandIndex, ChannelIndex), AudioOutputs[BandIndex * NumChannels + ChannelIndex]);
				}
			}

			return OutputPins;
		}

		static const FVertexInterface& GetDefaultInterface()
		{
			using namespace BandSplitterNode;

			auto CreateDefaultInterface = []()-> FVertexInterface
			{
				// inputs
				FInputVertexInterface InputInterface;

				// audio channels
				for (uint32 ChannelIndex = 0; ChannelIndex < NumChannels; ChannelIndex++)
				{
#if WITH_EDITOR
					const FDataVertexMetadata AudioInputMetadata
					{
						GetAudioInputDescription(ChannelIndex),
						GetAudioInputDisplayName(ChannelIndex)
					};
#else 
					const FDataVertexMetadata AudioInputMetadata;
#endif // WITH_EDITOR

					InputInterface.Add(TInputDataVertex<FAudioBuffer>(GetAudioInputName(ChannelIndex), AudioInputMetadata));
				}

				InputInterface.Add(TInputDataVertex<FEnumBandSplitterFilterOrder>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputFilterOrder)));
				InputInterface.Add(TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputPhaseCompensate), true));

				// Crossover frequencies
				for (uint32 InputIndex = 0; InputIndex < NumBands - 1; InputIndex++)
				{
#if WITH_EDITOR
					const FDataVertexMetadata CrossoverInputMetadata
					{
						GetCrossoverInputDescription(InputIndex),
						GetCrossoverInputDisplayName(InputIndex)
					};
#else
					const FDataVertexMetadata CrossoverInputMetadata;
#endif // WITH_EDITOR

					InputInterface.Add(TInputDataVertex<float>(GetCrossoverInputName(InputIndex), CrossoverInputMetadata, (1 + InputIndex) * 500.0f));
				}

				// outputs
				FOutputVertexInterface OutputInterface;
				for (uint32 OutputIndex = 0; OutputIndex < NumBands; OutputIndex++)
				{
					for (uint32 ChannelIndex = 0; ChannelIndex < NumChannels; ChannelIndex++)
					{
#if WITH_EDITOR
						const FDataVertexMetadata AudioOutputMetadata
						{
							GetAudioOutputDescription(OutputIndex, ChannelIndex),
							GetAudioOutputDisplayName(OutputIndex, ChannelIndex)
						};
#else 
						const FDataVertexMetadata AudioOutputMetadata;
#endif // WITH_EDITOR 

						OutputInterface.Add(TOutputDataVertex<FAudioBuffer>(GetAudioOutputName(OutputIndex, ChannelIndex), AudioOutputMetadata));
					}
				}

				return FVertexInterface(InputInterface, OutputInterface);
			}; // end lambda: CreateDefaultInterface()

			static const FVertexInterface DefaultInterface = CreateDefaultInterface();
			return DefaultInterface;
		}

		static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors)
		{
			using namespace BandSplitterNode;

			const FDataReferenceCollection& Inputs = InParams.InputDataReferences;
			const FInputVertexInterface& InputInterface = InParams.Node.GetVertexInterface().GetInputInterface();

			TArray<FAudioBufferReadRef> InputBuffers;
			TArray<FFloatReadRef> InputCrossovers;

			FEnumBandSplitterFilterOrderReadRef FilterOrderIn = Inputs.GetDataReadReferenceOrConstructWithVertexDefault<FEnumBandSplitterFilterOrder>(InputInterface, METASOUND_GET_PARAM_NAME(InputFilterOrder), InParams.OperatorSettings);
			FBoolReadRef bPhaseCompensateIn = Inputs.GetDataReadReferenceOrConstructWithVertexDefault<bool>(InputInterface, METASOUND_GET_PARAM_NAME(InputPhaseCompensate), InParams.OperatorSettings);

			for (uint32 Chan = 0; Chan < NumChannels; Chan++)
			{
				InputBuffers.Add(Inputs.GetDataReadReferenceOrConstruct<FAudioBuffer>(GetAudioInputName(Chan), InParams.OperatorSettings));
			}

			for (uint32 Band = 0; Band < NumBands - 1; Band++)
			{
				InputCrossovers.Add(Inputs.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, GetCrossoverInputName(Band), InParams.OperatorSettings));
			}

			return MakeUnique<TBandSplitterOperator<NumBands, NumChannels>>(InParams.OperatorSettings, MoveTemp(InputBuffers), MoveTemp(InputCrossovers), FilterOrderIn, bPhaseCompensateIn);
		}

		void UpdateFilterSettings()
		{
			bool bNeedsReinitFilters = false;
			bool bNeedsUpdateCrossovers = false;

			for (int32 CrossoverIndex = 0; CrossoverIndex < Crossovers.Num(); CrossoverIndex++)
			{
				if (Crossovers[CrossoverIndex] != *CrossoverFrequencies[CrossoverIndex])
				{
					bNeedsUpdateCrossovers = true;
					Crossovers[CrossoverIndex] = *CrossoverFrequencies[CrossoverIndex];
				}
			}

			if (*bPhaseCompensate != bPrevPhaseCompensate)
			{
				bNeedsReinitFilters = true;
				bPrevPhaseCompensate = *bPhaseCompensate;
			}

			if (PrevFilterOrder != GetFilterOrder(*FilterOrder))
			{
				bNeedsReinitFilters = true;
				PrevFilterOrder = GetFilterOrder(*FilterOrder);
			}

			if (bNeedsReinitFilters)
			{
				for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ChannelIndex++)
				{
					Filters[ChannelIndex].Init(1, Settings.GetSampleRate(), PrevFilterOrder, bPrevPhaseCompensate, Crossovers);
				}
			}
			else if (bNeedsUpdateCrossovers)
			{
				for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ChannelIndex++)
				{
					Filters[ChannelIndex].SetCrossovers(Crossovers);
				}
			}
		}

		void Execute()
		{
			UpdateFilterSettings();

			for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ChannelIndex++)
			{
				const int32 NumFrames = AudioInputs[ChannelIndex]->Num();

				MultiBandBuffers[ChannelIndex].SetSamples(NumFrames);

				Filters[ChannelIndex].ProcessAudioBuffer(AudioInputs[ChannelIndex]->GetData(), MultiBandBuffers[ChannelIndex], NumFrames);

				for (int32 BandIndex = 0; BandIndex < NumBands; BandIndex++)
				{
					FMemory::Memcpy(AudioOutputs[BandIndex * NumChannels + ChannelIndex]->GetData(), MultiBandBuffers[ChannelIndex][BandIndex], NumFrames * sizeof(float));
				}
			}
		}

	private:
		TArray<FAudioBufferReadRef> AudioInputs;
		TArray<FFloatReadRef> CrossoverFrequencies;

		TArray<float> Crossovers;

		FEnumBandSplitterFilterOrderReadRef FilterOrder;
		FBoolReadRef bPhaseCompensate;

		bool bPrevPhaseCompensate;
		Audio::EFilterOrder PrevFilterOrder;

		TArray<FAudioBufferWriteRef> AudioOutputs;

		TArray<Audio::FLinkwitzRileyBandSplitter> Filters;
		TArray<Audio::FMultibandBuffer> MultiBandBuffers;

		FOperatorSettings Settings;

		static FNodeClassMetadata CreateNodeClassMetadata(const FName& InOperatorName, const FText& InDisplayName, const FText& InDescription, const FVertexInterface& InDefaultInterface)
		{
			FNodeClassMetadata Metadata
			{
				FNodeClassName { "BandSplitter", InOperatorName, FName() },
				1, // Major Version
				0, // Minor Version
				InDisplayName,
				InDescription,
				PluginAuthor,
				PluginNodeMissingPrompt,
				InDefaultInterface,
				{ NodeCategories::Filters },
				{ },
				FNodeDisplayStyle{}
			};

			return Metadata;
		}

		static const FVertexName GetAudioInputName(uint32 ChannelIndex)
		{
			if (NumChannels == 1)
			{
				return *FString::Printf(TEXT("In"));
			}
			else if (NumChannels == 2)
			{
				return *FString::Printf(TEXT("In %s"), (ChannelIndex == 0) ? TEXT("L") : TEXT("R"));
			}

			return *FString::Printf(TEXT("In %i"), ChannelIndex);
		}

		static const FVertexName GetCrossoverInputName(uint32 InputIndex)
		{
			return *FString::Printf(TEXT("Crossover %i"), InputIndex);
		}

		static const FVertexName GetAudioOutputName(uint32 OutputIndex, uint32 ChannelIndex)
		{
			if (NumChannels == 1)
			{
				return *FString::Printf(TEXT("Band %i Out"), OutputIndex);
			}
			else if (NumChannels == 2)
			{
				return *FString::Printf(TEXT("Band %i %s"), OutputIndex, (ChannelIndex == 0) ? TEXT("L") : TEXT("R"));
			}

			return *FString::Printf(TEXT("Band %i Out %i"), OutputIndex, ChannelIndex);
		}

#if WITH_EDITOR
		static const FText GetAudioInputDescription(uint32 ChannelIndex)
		{
			return METASOUND_LOCTEXT_FORMAT("BandSplitterAudioInputDescription", "Audio Channel: {0}", ChannelIndex);
		}

		static const FText GetAudioInputDisplayName(uint32 ChannelIndex)
		{
			if (NumChannels == 1)
			{
				return METASOUND_LOCTEXT("BandsplitterAudioInputIn1", "In");
			}
			else if (NumChannels == 2)
			{
				if (ChannelIndex == 0)
				{
					return METASOUND_LOCTEXT("BandsplitterAudioInputIn2L", "In L");
				}
				else
				{
					return METASOUND_LOCTEXT("BandsplitterAudioInputIn2R", "In R");
				}
			}

			return METASOUND_LOCTEXT_FORMAT("BandsplitterAudioInputIn", "In {0}", ChannelIndex);
		}

		static const FText GetCrossoverInputDisplayName(uint32 InputIndex)
		{
			return METASOUND_LOCTEXT_FORMAT("BandsplitterCrossoverInputDisplayName", "Crossover {0}", InputIndex);
		}

		static const FText GetCrossoverInputDescription(uint32 InputIndex)
		{
			return METASOUND_LOCTEXT_FORMAT("BandSplitterCrossoverInputDescription", "Crossover Input #: {0}", InputIndex);
		}

		static const FText GetAudioOutputDescription(uint32 OutputIndex, uint32 ChannelIndex)
		{
			return METASOUND_LOCTEXT_FORMAT("BandSplitterAudioOutputDescription", "Channel {0} Output for Band {1}", ChannelIndex, OutputIndex);
		}

		static const FText GetAudioOutputDisplayName(uint32 OutputIndex, uint32 ChannelIndex)
		{
			if (NumChannels == 1)
			{
				return METASOUND_LOCTEXT_FORMAT("BandsplitterAudioOutputOut1", "Band {0} Out", OutputIndex);
			}
			else if (NumChannels == 2)
			{
				if (ChannelIndex == 0)
				{
					return METASOUND_LOCTEXT_FORMAT("BandsplitterAudioOutputOut2L", "Band {0} L", OutputIndex);
				}
				else
				{
					return METASOUND_LOCTEXT_FORMAT("BandsplitterAudioOutputOut2R", "Band {0} R", OutputIndex);
				}
			}

			return METASOUND_LOCTEXT_FORMAT("BandsplitterAudioOutputOut", "Band {0} Out {1}", OutputIndex, ChannelIndex);
		}
#endif // WITH_EDITOR

		FORCEINLINE Audio::EFilterOrder GetFilterOrder(EBandSplitterFilterOrder InFilterOrder) const
		{
			using namespace Audio;

			switch (InFilterOrder)
			{
			case EBandSplitterFilterOrder::TwoPole:
				return EFilterOrder::TwoPole;
				break;
			case EBandSplitterFilterOrder::FourPole:
				return EFilterOrder::FourPole;
				break;
			case EBandSplitterFilterOrder::SixPole:
				return EFilterOrder::SixPole;
				break;
			case EBandSplitterFilterOrder::EightPole:
				return EFilterOrder::EightPole;
				break;
			default:
				ensureMsgf(false, TEXT("Unhandled case converting EBandSplitterFilterOrder!"));
				break;
			}

			return EFilterOrder::TwoPole;
		}
	};

	// Node Class
	template<uint32 NumBands, uint32 NumChannels>
	class METASOUNDSTANDARDNODES_API TBandSplitterNode : public FNodeFacade
	{
	public:
		TBandSplitterNode(const FNodeInitData& InitData)
			: FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<TBandSplitterOperator<NumBands, NumChannels>>())
		{
		}
	};

#define REGISTER_BANDSPLITTER_NODE(A, B) \
	using FBandSplitterNode_##A ## _ ##B = TBandSplitterNode<A, B>; \
	METASOUND_REGISTER_NODE(FBandSplitterNode_##A ## _ ##B) \

	// monos
	REGISTER_BANDSPLITTER_NODE(2, 1)
	REGISTER_BANDSPLITTER_NODE(3, 1)
	REGISTER_BANDSPLITTER_NODE(4, 1)
	REGISTER_BANDSPLITTER_NODE(5, 1)

	// stereo
	REGISTER_BANDSPLITTER_NODE(2, 2)
	REGISTER_BANDSPLITTER_NODE(3, 2)
	REGISTER_BANDSPLITTER_NODE(4, 2)
	REGISTER_BANDSPLITTER_NODE(5, 2)

} // namespace Metasound

#undef LOCTEXT_NAMESPACE
