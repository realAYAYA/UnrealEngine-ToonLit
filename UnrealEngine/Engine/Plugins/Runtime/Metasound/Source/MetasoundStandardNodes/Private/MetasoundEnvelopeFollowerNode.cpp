// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundEnvelopeFollowerNode.h"

#include "Algo/MaxElement.h"
#include "Internationalization/Text.h"
#include "MetasoundAudioBuffer.h"
#include "MetasoundEnvelopeFollowerTypes.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundEnumRegistrationMacro.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundDataTypeRegistrationMacro.h"
#include "MetasoundOperatorSettings.h"
#include "MetasoundPrimitives.h"
#include "MetasoundStandardNodesNames.h"
#include "MetasoundTrigger.h"
#include "MetasoundTime.h"
#include "MetasoundVertex.h"
#include "MetasoundParamHelper.h"
#include "DSP/EnvelopeFollower.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_EnvelopeFollower"

namespace Metasound
{
	namespace EnvelopeFollowerVertexNames
	{
		METASOUND_PARAM(InParamAudioInput, "In", "Audio input.")
		METASOUND_PARAM(InParamAttackTime, "Attack Time", "The attack time of the envelope follower.")
		METASOUND_PARAM(InParamReleaseTime,"Release Time" , "The release time of the envelope follower.")
		METASOUND_PARAM(InParamFollowMode, "Peak Mode", "The following-method of the envelope follower.")
		METASOUND_PARAM(OutParamEnvelope, "Envelope", "The output envelope value of the audio signal.")
		METASOUND_PARAM(OutputAudioEnvelope, "Audio Envelope", "The output envelope value of the audio signal (audio rate).");
	}

	class FEnvelopeFollowerOperator : public TExecutableOperator<FEnvelopeFollowerOperator>
	{
	public:

		static const FNodeClassMetadata& GetNodeInfo();
		static const FVertexInterface& GetVertexInterface();
		static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors);

		FEnvelopeFollowerOperator(const FOperatorSettings& InSettings,
			const FAudioBufferReadRef& InAudioInput,
			const FTimeReadRef& InAttackTime,
			const FTimeReadRef& InReleaseTime,
			const FEnvelopePeakModeReadRef& InEnvelopeMode);

		virtual FDataReferenceCollection GetInputs() const override;
		virtual FDataReferenceCollection GetOutputs() const override;
		void Execute();

	private:
		// The input audio buffer
		FAudioBufferReadRef AudioInput;

		// The amount of attack time
		FTimeReadRef AttackTimeInput;

		// The amount of release time
		FTimeReadRef ReleaseTimeInput;

		// The Envelope-Following method
		FEnvelopePeakModeReadRef FollowModeInput;

		// The envelope outputs
		FFloatWriteRef EnvelopeFloatOutput;
		FAudioBufferWriteRef EnvelopeAudioOutput;

		// The envelope follower DSP object
		Audio::FEnvelopeFollower EnvelopeFollower;

		double PrevAttackTime = 0.0;
		double PrevReleaseTime = 0.0;
		EEnvelopePeakMode PrevFollowMode = EEnvelopePeakMode::Peak;
	};

	FEnvelopeFollowerOperator::FEnvelopeFollowerOperator(const FOperatorSettings& InSettings,
		const FAudioBufferReadRef& InAudioInput,
		const FTimeReadRef& InAttackTime,
		const FTimeReadRef& InReleaseTime,
		const FEnvelopePeakModeReadRef& InEnvelopeMode)
		: AudioInput(InAudioInput)
		, AttackTimeInput(InAttackTime)
		, ReleaseTimeInput(InReleaseTime)
		, FollowModeInput(InEnvelopeMode)
		, EnvelopeFloatOutput(FFloatWriteRef::CreateNew())
		, EnvelopeAudioOutput(FAudioBufferWriteRef::CreateNew(InSettings))
	{
		PrevAttackTime = FMath::Max(FTime::ToMilliseconds(*AttackTimeInput), 0.0);
		PrevReleaseTime = FMath::Max(FTime::ToMilliseconds(*ReleaseTimeInput), 0.0);

		Audio::FEnvelopeFollowerInitParams EnvelopeParamsInitParams;
	
		EnvelopeParamsInitParams.SampleRate = InSettings.GetSampleRate();
		EnvelopeParamsInitParams.NumChannels = 1;
		EnvelopeParamsInitParams.AttackTimeMsec = PrevAttackTime;
		EnvelopeParamsInitParams.ReleaseTimeMsec = PrevReleaseTime;

		EnvelopeFollower.Init(EnvelopeParamsInitParams);
	}

	FDataReferenceCollection FEnvelopeFollowerOperator::GetInputs() const
	{
		using namespace EnvelopeFollowerVertexNames;

		FDataReferenceCollection InputDataReferences;
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InParamAudioInput), AudioInput);
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InParamAttackTime), AttackTimeInput);
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InParamReleaseTime), ReleaseTimeInput);
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InParamFollowMode), FollowModeInput);

		return InputDataReferences;
	}

	FDataReferenceCollection FEnvelopeFollowerOperator::GetOutputs() const
	{
		using namespace EnvelopeFollowerVertexNames;

		FDataReferenceCollection OutputDataReferences;
		OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutParamEnvelope), EnvelopeFloatOutput);
		OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputAudioEnvelope), EnvelopeAudioOutput);
		return OutputDataReferences;
	}

	void FEnvelopeFollowerOperator::Execute()
	{
		// Check for any input changes
		double CurrentAttackTime = FMath::Max(FTime::ToMilliseconds(*AttackTimeInput), 0.0);
		if (!FMath::IsNearlyEqual(CurrentAttackTime, PrevAttackTime))
		{
			PrevAttackTime = CurrentAttackTime;
			EnvelopeFollower.SetAttackTime(CurrentAttackTime);
		}

		double CurrentReleaseTime = FMath::Max(FTime::ToMilliseconds(*ReleaseTimeInput), 0.0);
		if (!FMath::IsNearlyEqual(CurrentReleaseTime, PrevReleaseTime))
		{
			PrevReleaseTime = CurrentReleaseTime;
			EnvelopeFollower.SetReleaseTime(CurrentReleaseTime);
		}

		if (PrevFollowMode != *FollowModeInput)
		{
			PrevFollowMode = *FollowModeInput;
			switch (PrevFollowMode)
			{
			case EEnvelopePeakMode::MeanSquared:
			default:
				EnvelopeFollower.SetMode(Audio::EPeakMode::Type::MeanSquared);
				break;
			case EEnvelopePeakMode::RootMeanSquared:
				EnvelopeFollower.SetMode(Audio::EPeakMode::Type::RootMeanSquared);
				break;
			case EEnvelopePeakMode::Peak:
				EnvelopeFollower.SetMode(Audio::EPeakMode::Type::Peak);
				break;
			}
		}

		// Process the audio through the envelope follower
		EnvelopeFollower.ProcessAudio(AudioInput->GetData(), AudioInput->Num(), EnvelopeAudioOutput->GetData());
		if (const float* MaxElement = Algo::MaxElement(EnvelopeFollower.GetEnvelopeValues()))
		{
			*EnvelopeFloatOutput = *MaxElement;
		}
		else
		{
			*EnvelopeFloatOutput = 0.f;
		}
	}

	const FVertexInterface& FEnvelopeFollowerOperator::GetVertexInterface()
	{
		using namespace EnvelopeFollowerVertexNames;

		static const FVertexInterface Interface(
			FInputVertexInterface(
				TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InParamAudioInput)),
				TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InParamAttackTime), 0.01f),
				TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InParamReleaseTime), 0.1f),
				TInputDataVertex<FEnumEnvelopePeakMode>(METASOUND_GET_PARAM_NAME_AND_METADATA(InParamFollowMode))
			),
			FOutputVertexInterface(
				TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutParamEnvelope)),
				TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputAudioEnvelope))
			)
		);

		return Interface;
	}

	const FNodeClassMetadata& FEnvelopeFollowerOperator::GetNodeInfo()
	{
		auto InitNodeInfo = []() -> FNodeClassMetadata
		{
			FNodeClassMetadata Info;
			Info.ClassName = { StandardNodes::Namespace, TEXT("Envelope Follower"), TEXT("") };
			Info.MajorVersion = 1;
			Info.MinorVersion = 3;
			Info.DisplayName = METASOUND_LOCTEXT("Metasound_EnvelopeFollowerDisplayName", "Envelope Follower");
			Info.Description = METASOUND_LOCTEXT("Metasound_EnvelopeFollowerDescription", "Outputs an envelope from an input audio signal.");
			Info.Author = PluginAuthor;
			Info.PromptIfMissing = PluginNodeMissingPrompt;
			Info.DefaultInterface = GetVertexInterface();

			return Info;
		};

		static const FNodeClassMetadata Info = InitNodeInfo();

		return Info;
	}

	TUniquePtr<IOperator> FEnvelopeFollowerOperator::CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors)
	{
		using namespace EnvelopeFollowerVertexNames;

		const FEnvelopeFollowerNode& EnvelopeFollowerNode = static_cast<const FEnvelopeFollowerNode&>(InParams.Node);
		const FDataReferenceCollection& InputCollection = InParams.InputDataReferences;
		const FInputVertexInterface& InputInterface = GetVertexInterface().GetInputInterface();

		FAudioBufferReadRef AudioIn = InputCollection.GetDataReadReferenceOrConstruct<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InParamAudioInput), InParams.OperatorSettings);
		FTimeReadRef AttackTime = InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<FTime>(InputInterface, METASOUND_GET_PARAM_NAME(InParamAttackTime), InParams.OperatorSettings);
		FTimeReadRef ReleaseTime = InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<FTime>(InputInterface, METASOUND_GET_PARAM_NAME(InParamReleaseTime), InParams.OperatorSettings);
		FEnvelopePeakModeReadRef EnvelopeModeIn = InputCollection.GetDataReadReferenceOrConstruct<FEnumEnvelopePeakMode>(METASOUND_GET_PARAM_NAME(InParamFollowMode));


		return MakeUnique<FEnvelopeFollowerOperator>(InParams.OperatorSettings, AudioIn, AttackTime, ReleaseTime, EnvelopeModeIn);
	}

	FEnvelopeFollowerNode::FEnvelopeFollowerNode(const FNodeInitData& InitData)
		: FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FEnvelopeFollowerOperator>())
	{
	}

	METASOUND_REGISTER_NODE(FEnvelopeFollowerNode)
}

#undef LOCTEXT_NAMESPACE
