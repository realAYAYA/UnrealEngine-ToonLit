// Copyright Epic Games, Inc. All Rights Reserved.

#include "Internationalization/Text.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundEnumRegistrationMacro.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundPrimitives.h"
#include "MetasoundStandardNodesNames.h"
#include "MetasoundAudioBuffer.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundFacade.h"
#include "MetasoundParamHelper.h"
#include "DSP/Dsp.h"
#include "DSP/DynamicsProcessor.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_LimiterNode"

namespace Metasound
{
	/* Mid-Side Encoder */
	namespace LimiterVertexNames
	{
		METASOUND_PARAM(InputAudio, "Audio", "Incoming audio signal to compress.");
		METASOUND_PARAM(InputInGainDb, "Input Gain dB", "Gain to apply to the input before limiting, in decibels. Maximum 100 dB. ");
		METASOUND_PARAM(InputThresholdDb, "Threshold dB", "Amplitude threshold above which gain will be reduced.");
		METASOUND_PARAM(InputReleaseTime, "Release Time", "How long it takes for audio below the threshold to return to its original volume level.");
		METASOUND_PARAM(InputKneeMode, "Knee", "Whether the limiter uses a hard or soft knee.");

		METASOUND_PARAM(OutputAudio, "Audio", "The output audio signal.");
	}

	enum class EKneeMode
	{
		Hard = 0,
		Soft,
	};

	DECLARE_METASOUND_ENUM(EKneeMode, EKneeMode::Hard, METASOUNDSTANDARDNODES_API,
	FEnumKneeMode, FEnumKneeModeInfo, FKneeModeReadRef, FEnumKneeModeWriteRef);

	DEFINE_METASOUND_ENUM_BEGIN(EKneeMode, FEnumKneeMode, "KneeMode")
		DEFINE_METASOUND_ENUM_ENTRY(EKneeMode::Hard, "KneeModeHardDescription", "Hard", "KneeModeHardDescriptionTT", "Only audio strictly above the threshold is affected by the limiter."),
		DEFINE_METASOUND_ENUM_ENTRY(EKneeMode::Soft, "KneeModeSoftDescription", "Soft", "KneeModeSoftDescriptionTT", "Limiter activates more smoothly near the threshold."),
		DEFINE_METASOUND_ENUM_END()

	// Operator Class
	class FLimiterOperator : public TExecutableOperator<FLimiterOperator>
	{
	public:

		static constexpr float HardKneeBandwitdh = 0.0f;
		static constexpr float SoftKneeBandwitdh = 10.0f;
		static constexpr float MaxInputGain = 100.0f;

		FLimiterOperator(const FOperatorSettings& InSettings,
			const FAudioBufferReadRef& InAudio,
			const FFloatReadRef& InGainDb,
			const FFloatReadRef& InThresholdDb,
			const FTimeReadRef& InReleaseTime,
			const FKneeModeReadRef& InKneeMode)
			: AudioInput(InAudio)
			, InGainDbInput(InGainDb)
			, ThresholdDbInput(InThresholdDb)
			, ReleaseTimeInput(InReleaseTime)
			, KneeModeInput(InKneeMode)
			, AudioOutput(FAudioBufferWriteRef::CreateNew(InSettings))
			, Limiter()
			, PrevInGainDb(*InGainDb)
			, PrevThresholdDb(*InThresholdDb)
			, PrevReleaseTime(FMath::Max(FTime::ToMilliseconds(*InReleaseTime), 0.0))
		{
			Limiter.Init(InSettings.GetSampleRate(), 1);
			Limiter.SetProcessingMode(Audio::EDynamicsProcessingMode::Limiter);
			Limiter.SetInputGain(FMath::Min(*InGainDbInput, MaxInputGain));
			Limiter.SetThreshold(*ThresholdDbInput);
			Limiter.SetAttackTime(0.0f);
			Limiter.SetReleaseTime(PrevReleaseTime);
			Limiter.SetPeakMode(Audio::EPeakMode::Peak);

			switch (*KneeModeInput)
			{
			default:
			case EKneeMode::Hard:
				Limiter.SetKneeBandwidth(HardKneeBandwitdh);
				break;
			case EKneeMode::Soft:
				Limiter.SetKneeBandwidth(SoftKneeBandwitdh);
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
					FNodeClassName{StandardNodes::Namespace, TEXT("Limiter"), StandardNodes::AudioVariant},
					1, // Major Version
					0, // Minor Version
					METASOUND_LOCTEXT("LimiterDisplayName", "Limiter"),
					METASOUND_LOCTEXT("LimiterDesc", "Prevents a signal from going above a given threshold."),
					PluginAuthor,
					PluginNodeMissingPrompt,
					NodeInterface,
					{NodeCategories::Dynamics},
					{},
					FNodeDisplayStyle{}
				};

				return Metadata;
			};

			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
			return Metadata;
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			using namespace LimiterVertexNames;

			static const FVertexInterface Interface(
				FInputVertexInterface(
					TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAudio)),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputInGainDb), 0.0f),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputThresholdDb), 0.0f),
					TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputReleaseTime), 0.1f),
					TInputDataVertex<FEnumKneeMode>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputKneeMode))
				),
				FOutputVertexInterface(
					TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputAudio))
				)
			);

			return Interface;
		}

		virtual FDataReferenceCollection GetInputs() const override
		{
			using namespace LimiterVertexNames;

			FDataReferenceCollection InputDataReferences;

			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputAudio), AudioInput);
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputInGainDb), InGainDbInput);
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputThresholdDb), ThresholdDbInput);
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputReleaseTime), ReleaseTimeInput);
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputKneeMode), KneeModeInput);

			return InputDataReferences;
		}

		virtual FDataReferenceCollection GetOutputs() const override
		{
			using namespace LimiterVertexNames;

			FDataReferenceCollection OutputDataReferences;

			OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputAudio), AudioOutput);

			return OutputDataReferences;
		}

		static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors)
		{
			using namespace LimiterVertexNames;
			const FDataReferenceCollection& Inputs = InParams.InputDataReferences;
			const FInputVertexInterface& InputInterface = DeclareVertexInterface().GetInputInterface();

			FAudioBufferReadRef AudioIn = Inputs.GetDataReadReferenceOrConstruct<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputAudio), InParams.OperatorSettings);
			FFloatReadRef InGainDbIn = Inputs.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, METASOUND_GET_PARAM_NAME(InputInGainDb), InParams.OperatorSettings);
			FFloatReadRef ThresholdDbIn = Inputs.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, METASOUND_GET_PARAM_NAME(InputThresholdDb), InParams.OperatorSettings);
			FTimeReadRef ReleaseTimeIn = Inputs.GetDataReadReferenceOrConstructWithVertexDefault<FTime>(InputInterface, METASOUND_GET_PARAM_NAME(InputReleaseTime), InParams.OperatorSettings);
			FKneeModeReadRef KneeModeIn = Inputs.GetDataReadReferenceOrConstruct<FEnumKneeMode>(METASOUND_GET_PARAM_NAME(InputKneeMode));

			return MakeUnique<FLimiterOperator>(InParams.OperatorSettings, AudioIn, InGainDbIn, ThresholdDbIn, ReleaseTimeIn, KneeModeIn);
		}
		void Execute()
		{
			/* Update parameters */
			if (!FMath::IsNearlyEqual(*InGainDbInput, PrevInGainDb))
			{
				Limiter.SetInputGain(FMath::Min(*InGainDbInput, MaxInputGain));
			}
			if (!FMath::IsNearlyEqual(*ThresholdDbInput, PrevThresholdDb))
			{
				Limiter.SetThreshold(*ThresholdDbInput);
			}
			// Release time cannot be negative
			double CurrRelease = FMath::Max(FTime::ToMilliseconds(*ReleaseTimeInput), 0.0f);
			if (!FMath::IsNearlyEqual(CurrRelease, PrevReleaseTime))
			{
				Limiter.SetReleaseTime(CurrRelease);
				PrevReleaseTime = CurrRelease;
			}

			switch (*KneeModeInput)
			{
			default:
			case EKneeMode::Hard:
				Limiter.SetKneeBandwidth(HardKneeBandwitdh);
				break;
			case EKneeMode::Soft:
				Limiter.SetKneeBandwidth(SoftKneeBandwitdh);
				break;
			}			

			Limiter.ProcessAudio(AudioInput->GetData(), AudioInput->Num(), AudioOutput->GetData());
		}

	private:
		FAudioBufferReadRef AudioInput;
		FFloatReadRef InGainDbInput;
		FFloatReadRef ThresholdDbInput;
		FTimeReadRef ReleaseTimeInput;
		FKneeModeReadRef KneeModeInput;

		FAudioBufferWriteRef AudioOutput;

		// Internal DSP Limiter
		Audio::FDynamicsProcessor Limiter;

		// Cached variables
		float PrevInGainDb;
		float PrevThresholdDb;
		double PrevReleaseTime;
		float PrevKnee;
	};

	// Node Class
	class FLimiterNode : public FNodeFacade
	{
	public:
		FLimiterNode(const FNodeInitData& InitData)
			: FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FLimiterOperator>())
		{
		}
	};

	// Register node
	METASOUND_REGISTER_NODE(FLimiterNode)
}

#undef LOCTEXT_NAMESPACE
