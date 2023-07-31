// Copyright Epic Games, Inc. All Rights Reserved.

#include "Internationalization/Text.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundStandardNodesNames.h"
#include "MetasoundAudioBuffer.h"
#include "DSP/TapDelayPitchShifter.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundFacade.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodesDopplerPitchShift"

namespace Metasound
{
	namespace DopplerPitchShift
	{
		METASOUND_PARAM(InParamAudioInput, "In", "Input audio buffer.")
		METASOUND_PARAM(InParamPitchShift, "Pitch Shift", "The amount to pitch shift the audio signal, in semitones.")
		METASOUND_PARAM(InParamDelayLength, "Delay Length", "The delay length of the internal delay buffer in milliseconds (10 ms to 100 ms). Changing this can reduce artifacts in certain pitch shift regions.")
		METASOUND_PARAM(OutParamAudio, "Out", "Output audio buffer.")
	}

	class FDopplerPitchShiftOperator : public TExecutableOperator<FDopplerPitchShiftOperator>
	{
	public:
		static const FNodeClassMetadata& GetNodeInfo();
		static const FVertexInterface& GetVertexInterface();
		static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors);

		FDopplerPitchShiftOperator(const FOperatorSettings& InSettings, const FAudioBufferReadRef& InAudioInput, const FFloatReadRef& InPitchShift, const FFloatReadRef& InDelayLength);

		virtual FDataReferenceCollection GetInputs() const override;
		virtual FDataReferenceCollection GetOutputs() const override;
		void Execute();

	private:
		FAudioBufferReadRef AudioInput;
		FFloatReadRef PitchShift;
		FFloatReadRef DelayLength;
		FAudioBufferWriteRef AudioOutput;
		Audio::FDelay DelayBuffer;
		Audio::FTapDelayPitchShifter DelayPitchShifter;
	};

	FDopplerPitchShiftOperator::FDopplerPitchShiftOperator(const FOperatorSettings& InSettings,
		const FAudioBufferReadRef& InAudioInput,
		const FFloatReadRef& InPitchShift,
		const FFloatReadRef& InDelayLength)

		: AudioInput(InAudioInput)
		, PitchShift(InPitchShift)
		, DelayLength(InDelayLength)
		, AudioOutput(FAudioBufferWriteRef::CreateNew(InSettings))
	{
		DelayBuffer.Init(InSettings.GetSampleRate(), 0.001f * Audio::FTapDelayPitchShifter::MaxDelayLength);
		DelayPitchShifter.Init(InSettings.GetSampleRate(), *PitchShift, *DelayLength);
	}

	FDataReferenceCollection FDopplerPitchShiftOperator::GetInputs() const
	{
		using namespace DopplerPitchShift;

		FDataReferenceCollection InputDataReferences;
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InParamAudioInput), FAudioBufferReadRef(AudioInput));
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InParamPitchShift), FFloatReadRef(PitchShift));
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InParamDelayLength), FFloatReadRef(DelayLength));

		return InputDataReferences;
	}

	FDataReferenceCollection FDopplerPitchShiftOperator::GetOutputs() const
	{
		using namespace DopplerPitchShift;

		FDataReferenceCollection OutputDataReferences;
		OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutParamAudio), FAudioBufferReadRef(AudioOutput));
		return OutputDataReferences;
	}

	void FDopplerPitchShiftOperator::Execute()
	{
		DelayPitchShifter.SetDelayLength(*DelayLength);
		DelayPitchShifter.SetPitchShift(*PitchShift);
		DelayPitchShifter.ProcessAudio(DelayBuffer, AudioInput->GetData(), AudioInput->Num(), AudioOutput->GetData());
	}

	const FVertexInterface& FDopplerPitchShiftOperator::GetVertexInterface()
	{
		using namespace DopplerPitchShift;

		static const FVertexInterface Interface(
			FInputVertexInterface(
				TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InParamAudioInput)),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InParamPitchShift), 0.0f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InParamDelayLength), 30.0f)
			),
			FOutputVertexInterface(
				TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutParamAudio))
			)
		);

		return Interface;
	}

	const FNodeClassMetadata& FDopplerPitchShiftOperator::GetNodeInfo()
	{
		auto InitNodeInfo = []() -> FNodeClassMetadata
		{
			FNodeClassMetadata Info;
			Info.ClassName = { StandardNodes::Namespace, "Delay Pitch Shift", StandardNodes::AudioVariant };
			Info.MajorVersion = 1;
			Info.MinorVersion = 0;
			Info.DisplayName = METASOUND_LOCTEXT("DopperDelayPitchShift_DisplayName", "Delay Pitch Shift");
			Info.Description = METASOUND_LOCTEXT("DopperDelayPitchShift_Description", "Pitch shifts the audio buffer using a delay-based doppler-shift method.");
			Info.Author = PluginAuthor;
			Info.PromptIfMissing = PluginNodeMissingPrompt;
			Info.DefaultInterface = GetVertexInterface();
			Info.CategoryHierarchy.Emplace(NodeCategories::Delays);
			return Info;
		};

		static const FNodeClassMetadata Info = InitNodeInfo();

		return Info;
	}

	TUniquePtr<IOperator> FDopplerPitchShiftOperator::CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors)
	{
		using namespace DopplerPitchShift;

		const FDataReferenceCollection& InputCollection = InParams.InputDataReferences;
		const FInputVertexInterface& InputInterface = GetVertexInterface().GetInputInterface();

		FAudioBufferReadRef AudioIn = InputCollection.GetDataReadReferenceOrConstruct<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InParamAudioInput), InParams.OperatorSettings);
		FFloatReadRef PitchShift = InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, METASOUND_GET_PARAM_NAME(InParamPitchShift), InParams.OperatorSettings);
		FFloatReadRef DelayLength = InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, METASOUND_GET_PARAM_NAME(InParamDelayLength), InParams.OperatorSettings);

		return MakeUnique<FDopplerPitchShiftOperator>(InParams.OperatorSettings, AudioIn, PitchShift, DelayLength);
	}

	class FDopperPitchShiftNode : public FNodeFacade
	{
	public:
		FDopperPitchShiftNode(const FNodeInitData& InitData)
			: FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FDopplerPitchShiftOperator>())
		{
		}
	};


	METASOUND_REGISTER_NODE(FDopperPitchShiftNode)
}

#undef LOCTEXT_NAMESPACE
