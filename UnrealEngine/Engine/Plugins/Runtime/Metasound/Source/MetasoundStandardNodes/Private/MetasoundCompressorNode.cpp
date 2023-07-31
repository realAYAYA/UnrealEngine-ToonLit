// Copyright Epic Games, Inc. All Rights Reserved.

#include "DSP/Dsp.h"
#include "DSP/DynamicsProcessor.h"
#include "DSP/FloatArrayMath.h"
#include "DSP/IntegerDelay.h"
#include "Internationalization/Text.h"
#include "MetasoundAudioBuffer.h"
#include "MetasoundEnumRegistrationMacro.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundEnvelopeFollowerTypes.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundStandardNodesNames.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_CompressorNode"

namespace Metasound
{
	/* Mid-Side Encoder */
	namespace CompressorVertexNames
	{
		METASOUND_PARAM(InputAudio, "Audio", "Incoming audio signal to compress.");
		METASOUND_PARAM(InputRatio, "Ratio", "Amount of gain reduction. 1 = no reduction, higher = more reduction.");
		METASOUND_PARAM(InputThreshold, "Threshold dB", "Amplitude threshold (dB) above which gain will be reduced.");
		METASOUND_PARAM(InputAttackTime, "Attack Time", "How long it takes for audio above the threshold to reach its compressed volume level.");
		METASOUND_PARAM(InputReleaseTime, "Release Time", "How long it takes for audio below the threshold to return to its original volume level.");
		METASOUND_PARAM(InputLookaheadTime, "Lookahead Time", "How much time the compressor has to lookahead and catch peaks. This delays the signal.");
		METASOUND_PARAM(InputKnee, "Knee", "How hard or soft the gain reduction blends from no gain reduction to gain reduction. 0 dB = no blending.");
		METASOUND_PARAM(InputSidechain, "Sidechain", "(Optional) External audio signal to control the compressor with. If empty, uses the input audio signal.");
		METASOUND_PARAM(InputEnvelopeMode, "Envelope Mode", "The envelope-following method the compressor will use for gain detection.");
		METASOUND_PARAM(InputIsAnalog, "Analog Mode", "Enable Analog Mode for the compressor's envelope follower.");
		METASOUND_PARAM(InputIsUpwards, "Upwards Mode", "Enable to switch from a standard downwards compresser to an upwards compressor.");
		METASOUND_PARAM(InputWetDryMix, "Wet/Dry", "Ratio between the processed/wet signal and the unprocessed/dry signal. 0 is full dry, 1 is full wet, and 0.5 is 50/50.");

		METASOUND_PARAM(OutputAudio, "Audio", "The output audio signal.");
		METASOUND_PARAM(OutputEnvelope, "Gain Envelope", "The compressor's gain being applied to the signal.");
	}

	// Operator Class
	class FCompressorOperator : public TExecutableOperator<FCompressorOperator>
	{
	public:

		FCompressorOperator(const FOperatorSettings& InSettings,
			const FAudioBufferReadRef& InAudio,
			const FFloatReadRef& InRatio,
			const FFloatReadRef& InThresholdDb,
			const FTimeReadRef& InAttackTime,
			const FTimeReadRef& InReleaseTime,
			const FTimeReadRef& InLookaheadTime,
			const FFloatReadRef& InKnee,
			const bool& bInUseSidechain,
			const FAudioBufferReadRef& InSidechain,
			const FEnvelopePeakModeReadRef& InEnvelopeMode,
			const FBoolReadRef& bInIsAnalog,
			const FBoolReadRef& bInIsUpwards,
			const FFloatReadRef& InWetDryMix)
			: AudioInput(InAudio)
			, RatioInput(InRatio)
			, ThresholdDbInput(InThresholdDb)
			, AttackTimeInput(InAttackTime)
			, ReleaseTimeInput(InReleaseTime)
			, LookaheadTimeInput(InLookaheadTime)
			, KneeInput(InKnee)
			, SidechainInput(InSidechain)
			, EnvelopeModeInput(InEnvelopeMode)
			, bIsAnalogInput(bInIsAnalog)
			, bIsUpwardsInput(bInIsUpwards)
			, WetDryMixInput(InWetDryMix)
			, AudioOutput(FAudioBufferWriteRef::CreateNew(InSettings))
			, EnvelopeOutput(FAudioBufferWriteRef::CreateNew(InSettings))
			, InputDelay(FMath::CeilToInt(InSettings.GetSampleRate() * Compressor.GetMaxLookaheadMsec() / 1000.f) + 1, InSettings.GetSampleRate() * 10.0f / 1000.0f)
			, DelayedInputSignal(InSettings.GetNumFramesPerBlock())
			, bUseSidechain(bInUseSidechain)
			, MsToSamples(InSettings.GetSampleRate() / 1000.0f)
			, PrevAttackTime(FMath::Max(FTime::ToMilliseconds(*InAttackTime), 0.0))
			, PrevReleaseTime(FMath::Max(FTime::ToMilliseconds(*InReleaseTime), 0.0))
			, PrevLookaheadTime(FMath::Max(FTime::ToMilliseconds(*InLookaheadTime), 0.0))
		{
			Compressor.Init(InSettings.GetSampleRate(), 1);
			Compressor.SetKeyNumChannels(1);
			Compressor.SetRatio(FMath::Max(*InRatio, 1.0f));
			Compressor.SetThreshold(*ThresholdDbInput);
			Compressor.SetAttackTime(PrevAttackTime);
			Compressor.SetReleaseTime(PrevReleaseTime);
			Compressor.SetLookaheadMsec(PrevLookaheadTime);
			Compressor.SetKneeBandwidth(*KneeInput);

			if (*bIsUpwardsInput)
			{
				Compressor.SetProcessingMode(Audio::EDynamicsProcessingMode::UpwardsCompressor);
			}
			else
			{
				Compressor.SetProcessingMode(Audio::EDynamicsProcessingMode::Compressor);
			}

			switch (*EnvelopeModeInput)
			{
			default:
			case EEnvelopePeakMode::MeanSquared:
				Compressor.SetPeakMode(Audio::EPeakMode::MeanSquared);
				break;
			case EEnvelopePeakMode::RootMeanSquared:
				Compressor.SetPeakMode(Audio::EPeakMode::RootMeanSquared);
				break;
			case EEnvelopePeakMode::Peak:
				Compressor.SetPeakMode(Audio::EPeakMode::Peak);
				break;
			}

		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
			{
				FVertexInterface NodeInterface = DeclareVertexInterface();

				FNodeClassMetadata Metadata
				{
					FNodeClassName { StandardNodes::Namespace, "Compressor", StandardNodes::AudioVariant },
					1, // Major Version
					0, // Minor Version
					METASOUND_LOCTEXT("CompressorDisplayName", "Compressor"),
					METASOUND_LOCTEXT("CompressorDesc", "Lowers the dynamic range of a signal."),
					PluginAuthor,
					PluginNodeMissingPrompt,
					NodeInterface,
					{ NodeCategories::Dynamics },
					{ METASOUND_LOCTEXT("SidechainKeyword", "Sidechain"), METASOUND_LOCTEXT("UpwardsKeyword", "Upwards Compressor")},
					FNodeDisplayStyle()
				};

				return Metadata;
			};

			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
			return Metadata;
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			using namespace CompressorVertexNames;

			static const FVertexInterface Interface(
				FInputVertexInterface(
					TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAudio)),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputRatio), 1.5f),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputThreshold), -6.0f),
					TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAttackTime), 0.01f),
					TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputReleaseTime), 0.1f),
					TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputLookaheadTime), 0.01f),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputKnee), 10.0f),
					TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputSidechain)),
					TInputDataVertex<FEnumEnvelopePeakMode>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputEnvelopeMode)),
					TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputIsAnalog), true),
					TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputIsUpwards), false),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputWetDryMix), 1.0f)
				),
				FOutputVertexInterface(
					TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputAudio)),
					TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputEnvelope))
				)
			);

			return Interface;
		}

		virtual FDataReferenceCollection GetInputs() const override
		{
			using namespace CompressorVertexNames;

			FDataReferenceCollection InputDataReferences;

			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputAudio), AudioInput);
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputRatio), RatioInput);
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputThreshold), ThresholdDbInput);
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputAttackTime), AttackTimeInput);
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputReleaseTime), ReleaseTimeInput);
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputLookaheadTime), LookaheadTimeInput);
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputKnee), KneeInput);
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputSidechain), SidechainInput);
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputEnvelopeMode), EnvelopeModeInput);
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputIsUpwards), bIsUpwardsInput);
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputIsAnalog), bIsAnalogInput);
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputWetDryMix), WetDryMixInput);

			return InputDataReferences;

		}

		virtual FDataReferenceCollection GetOutputs() const override
		{
			using namespace CompressorVertexNames;

			FDataReferenceCollection OutputDataReferences;

			OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputAudio), AudioOutput);
			OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputEnvelope), EnvelopeOutput);

			return OutputDataReferences;
		}

		static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors)
		{
			using namespace CompressorVertexNames;
			const FDataReferenceCollection& Inputs = InParams.InputDataReferences;
			const FInputVertexInterface& InputInterface = DeclareVertexInterface().GetInputInterface();

			FAudioBufferReadRef AudioIn = Inputs.GetDataReadReferenceOrConstruct<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputAudio), InParams.OperatorSettings);
			FFloatReadRef RatioIn = Inputs.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, METASOUND_GET_PARAM_NAME(InputRatio), InParams.OperatorSettings);
			FFloatReadRef ThresholdDbIn = Inputs.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, METASOUND_GET_PARAM_NAME(InputThreshold), InParams.OperatorSettings);
			FTimeReadRef AttackTimeIn = Inputs.GetDataReadReferenceOrConstructWithVertexDefault<FTime>(InputInterface, METASOUND_GET_PARAM_NAME(InputAttackTime), InParams.OperatorSettings);
			FTimeReadRef ReleaseTimeIn = Inputs.GetDataReadReferenceOrConstructWithVertexDefault<FTime>(InputInterface, METASOUND_GET_PARAM_NAME(InputReleaseTime), InParams.OperatorSettings);
			FTimeReadRef LookaheadTimeIn = Inputs.GetDataReadReferenceOrConstructWithVertexDefault<FTime>(InputInterface, METASOUND_GET_PARAM_NAME(InputLookaheadTime), InParams.OperatorSettings);
			FFloatReadRef KneeIn = Inputs.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, METASOUND_GET_PARAM_NAME(InputKnee), InParams.OperatorSettings);
			FAudioBufferReadRef SidechainIn = Inputs.GetDataReadReferenceOrConstruct<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputSidechain), InParams.OperatorSettings);
			FEnvelopePeakModeReadRef EnvelopeModeIn = Inputs.GetDataReadReferenceOrConstruct<FEnumEnvelopePeakMode>(METASOUND_GET_PARAM_NAME(InputEnvelopeMode));
			FBoolReadRef bIsAnalogIn = Inputs.GetDataReadReferenceOrConstructWithVertexDefault<bool>(InputInterface, METASOUND_GET_PARAM_NAME(InputIsAnalog), InParams.OperatorSettings);
			FBoolReadRef bIsUpwardsIn = Inputs.GetDataReadReferenceOrConstructWithVertexDefault<bool>(InputInterface, METASOUND_GET_PARAM_NAME(InputIsUpwards), InParams.OperatorSettings);
			FFloatReadRef WetDryMixIn = Inputs.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, METASOUND_GET_PARAM_NAME(InputWetDryMix), InParams.OperatorSettings);

			bool bIsSidechainConnected = Inputs.ContainsDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputSidechain));

			return MakeUnique<FCompressorOperator>(InParams.OperatorSettings, AudioIn, RatioIn, ThresholdDbIn, AttackTimeIn, ReleaseTimeIn, LookaheadTimeIn, KneeIn, bIsSidechainConnected, SidechainIn, EnvelopeModeIn, bIsAnalogIn, bIsUpwardsIn, WetDryMixIn);
		}

		void Execute()
		{
			/* Update parameters */
			
			// For a compressor, ratio values should be 1 or greater
			Compressor.SetRatio(FMath::Max(*RatioInput, 1.0f));
			Compressor.SetThreshold(*ThresholdDbInput);

			// Attack time cannot be negative
			float CurrAttack = FMath::Max(FTime::ToMilliseconds(*AttackTimeInput), 0.0f);
			if (FMath::IsNearlyEqual(CurrAttack, PrevAttackTime) == false)
			{
				Compressor.SetAttackTime(CurrAttack);
				PrevAttackTime = CurrAttack;
			}

			// Release time cannot be negative
			float CurrRelease = FMath::Max(FTime::ToMilliseconds(*ReleaseTimeInput), 0.0f);
			if (FMath::IsNearlyEqual(CurrRelease, PrevReleaseTime) == false)
			{
				Compressor.SetReleaseTime(CurrRelease);
				PrevReleaseTime = CurrRelease;
			}

			float CurrLookahead = FMath::Clamp(FTime::ToMilliseconds(*LookaheadTimeInput), 0.0f, Compressor.GetMaxLookaheadMsec());
			if (FMath::IsNearlyEqual(CurrLookahead, PrevLookaheadTime) == false)
			{
				Compressor.SetLookaheadMsec(CurrLookahead);
				PrevLookaheadTime = CurrLookahead;
			}
			InputDelay.SetDelayLengthSamples(CurrLookahead * MsToSamples);

			Compressor.SetKneeBandwidth(*KneeInput);

			if (*bIsUpwardsInput)
			{
				Compressor.SetProcessingMode(Audio::EDynamicsProcessingMode::UpwardsCompressor);
			}
			else
			{
				Compressor.SetProcessingMode(Audio::EDynamicsProcessingMode::Compressor);
			}
				
			switch (*EnvelopeModeInput)
			{
			default:
			case EEnvelopePeakMode::MeanSquared:
				Compressor.SetPeakMode(Audio::EPeakMode::MeanSquared);
				break;

			case EEnvelopePeakMode::RootMeanSquared:
				Compressor.SetPeakMode(Audio::EPeakMode::RootMeanSquared);
				break;

			case EEnvelopePeakMode::Peak:
				Compressor.SetPeakMode(Audio::EPeakMode::Peak);
				break;
			}

			// Apply lookahead delay to dry signal
			InputDelay.ProcessAudio(*AudioInput, DelayedInputSignal);

			if (bUseSidechain)
			{
				Compressor.ProcessAudio(AudioInput->GetData(), AudioInput->Num(), AudioOutput->GetData(), SidechainInput->GetData(), EnvelopeOutput->GetData());
			}
			else
			{
				Compressor.ProcessAudio(AudioInput->GetData(), AudioInput->Num(), AudioOutput->GetData(), nullptr, EnvelopeOutput->GetData());
			}

			// Calculate Wet/Dry mix
			float NewWetDryMix = FMath::Clamp(*WetDryMixInput, 0.0f, 1.0f);
			// Wet signal
			Audio::ArrayMultiplyByConstantInPlace(*AudioOutput, NewWetDryMix);
			// Add Dry signal
			Audio::ArrayMultiplyAddInPlace(DelayedInputSignal, 1.0f - NewWetDryMix, *AudioOutput);
			
		}

	private:
		// Audio input and output
		FAudioBufferReadRef AudioInput;
		FFloatReadRef RatioInput;
		FFloatReadRef ThresholdDbInput;
		FTimeReadRef AttackTimeInput;
		FTimeReadRef ReleaseTimeInput;
		FTimeReadRef LookaheadTimeInput;
		FFloatReadRef KneeInput;
		FAudioBufferReadRef SidechainInput;
		FEnvelopePeakModeReadRef EnvelopeModeInput;
		FBoolReadRef bIsAnalogInput;
		FBoolReadRef bIsUpwardsInput;
		FFloatReadRef WetDryMixInput;

		FAudioBufferWriteRef AudioOutput;
		// The gain being applied to the input buffer
		FAudioBufferWriteRef EnvelopeOutput;

		// Internal DSP Compressor
		Audio::FDynamicsProcessor Compressor;
		// Compressor does not have a wet/dry signal built in, so
		// we need to account for lookahead delay in the input to prevent phase issues.
		Audio::FIntegerDelay InputDelay;
		FAudioBuffer DelayedInputSignal;

		// Whether or not to use sidechain input (is false if no input pin is connected to sidechain input)
		bool bUseSidechain;

		// Conversion from milliseconds to samples
		float MsToSamples;

		// Cached variables
		float PrevAttackTime;
		float PrevReleaseTime;
		float PrevLookaheadTime;
	};

	// Node Class
	class FCompressorNode : public FNodeFacade
	{
	public:
		FCompressorNode(const FNodeInitData& InitData)
			: FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FCompressorOperator>())
		{
		}
	};

	// Register node
	METASOUND_REGISTER_NODE(FCompressorNode)

}

#undef LOCTEXT_NAMESPACE
