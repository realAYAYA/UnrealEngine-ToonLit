// Copyright Epic Games, Inc. All Rights Reserved.

#include "Internationalization/Text.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundStandardNodesNames.h"
#include "MetasoundTrigger.h"
#include "MetasoundTime.h"
#include "MetasoundAudioBuffer.h"
#include "DSP/Delay.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundFacade.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes"

namespace Metasound
{
	namespace Delay
	{
		METASOUND_PARAM(InParamAudioInput, "In", "Audio input.")
		METASOUND_PARAM(InParamDelayTime, "Delay Time", "The amount of time to delay the audio.")
		METASOUND_PARAM(InParamDryLevel, "Dry Level", "The dry level of the delay.")
		METASOUND_PARAM(InParamWetLevel, "Wet Level", "The wet level of the delay.")
		METASOUND_PARAM(InParamFeedbackAmount, "Feedback", "Feedback amount.")
		METASOUND_PARAM(InParamMaxDelayTime, "Max Delay Time", "The maximum amount of time to delay the audio.")
		METASOUND_PARAM(OutParamAudio, "Out", "Audio output.")

		static constexpr float MinMaxDelaySeconds = 0.001f;
		static constexpr float MaxMaxDelaySeconds = 1000.f;
		static constexpr float DefaultMaxDelaySeconds = 5.0f;
	}

	class FDelayOperator : public TExecutableOperator<FDelayOperator>
	{
	public:

		static const FNodeClassMetadata& GetNodeInfo();
		static const FVertexInterface& GetVertexInterface();
		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults);

		FDelayOperator(const FBuildOperatorParams& InParams, 
			const FAudioBufferReadRef& InAudioInput, 
			const FTimeReadRef& InDelayTime, 
			const FFloatReadRef& InDryLevel, 
			const FFloatReadRef& InWetLevel, 
			const FFloatReadRef& InFeedback,
			float InMaxDelayTimeSeconds);

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override;
		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override;
		virtual FDataReferenceCollection GetInputs() const override;
		virtual FDataReferenceCollection GetOutputs() const override;
		void Reset(const IOperator::FResetParams& InParams);
		void Execute();

	private:
		float GetInputDelayTimeMsec() const;

		// The input audio buffer
		FAudioBufferReadRef AudioInput;

		// The amount of delay time
		FTimeReadRef DelayTime;

		// The dry level
		FFloatReadRef DryLevel;

		// The wet level
		FFloatReadRef WetLevel;

		// The feedback amount
		FFloatReadRef Feedback;

		// The audio output
		FAudioBufferWriteRef AudioOutput;

		// The internal delay buffer
		Audio::FDelay DelayBuffer;

		// The previous delay time
		float PrevDelayTimeMsec = 0.f;

		// Feedback sample
		float FeedbackSample = 0.f;

		// Maximum delay time
		float MaxDelayTimeSeconds = Delay::DefaultMaxDelaySeconds;
	};

	FDelayOperator::FDelayOperator(const FBuildOperatorParams& InParams,
		const FAudioBufferReadRef& InAudioInput,
		const FTimeReadRef& InDelayTime,
		const FFloatReadRef& InDryLevel,
		const FFloatReadRef& InWetLevel,
		const FFloatReadRef& InFeedback,
		float InMaxDelayTimeSeconds)

		: AudioInput(InAudioInput)
		, DelayTime(InDelayTime)
		, DryLevel(InDryLevel)
		, WetLevel(InWetLevel)
		, Feedback(InFeedback)
		, AudioOutput(FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings))
	{
		MaxDelayTimeSeconds = FMath::Clamp(InMaxDelayTimeSeconds, Delay::MinMaxDelaySeconds, Delay::MaxMaxDelaySeconds);

		Reset(InParams);
	}


	void FDelayOperator::BindInputs(FInputVertexInterfaceData& InOutVertexData)
	{
		using namespace Delay;

		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InParamAudioInput), AudioInput);
		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InParamDelayTime), DelayTime);
		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InParamDryLevel), DryLevel);
		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InParamWetLevel), WetLevel);
		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InParamFeedbackAmount), Feedback);
		InOutVertexData.SetValue(METASOUND_GET_PARAM_NAME(InParamMaxDelayTime), FTime::FromSeconds(MaxDelayTimeSeconds));
	}

	void FDelayOperator::BindOutputs(FOutputVertexInterfaceData& InOutVertexData)
	{
		using namespace Delay;
		InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutParamAudio), AudioOutput);
	}

	FDataReferenceCollection FDelayOperator::GetInputs() const
	{
		// This should never be called. Bind(...) is called instead. This method
		// exists as a stop-gap until the API can be deprecated and removed.
		checkNoEntry();
		return {};
	}

	FDataReferenceCollection FDelayOperator::GetOutputs() const
	{
		// This should never be called. Bind(...) is called instead. This method
		// exists as a stop-gap until the API can be deprecated and removed.
		checkNoEntry();
		return {};
	}

	float FDelayOperator::GetInputDelayTimeMsec() const
	{
		// Clamp the delay time to the max delay allowed
		return 1000.0f * FMath::Clamp((float)DelayTime->GetSeconds(), 0.0f, MaxDelayTimeSeconds);
	}

	void FDelayOperator::Reset(const IOperator::FResetParams& InParams)
	{
		FeedbackSample = 0.f;
		PrevDelayTimeMsec = GetInputDelayTimeMsec();

		DelayBuffer.Init(InParams.OperatorSettings.GetSampleRate(), MaxDelayTimeSeconds);
		DelayBuffer.SetDelayMsec(PrevDelayTimeMsec);

		AudioOutput->Zero();
	}

	void FDelayOperator::Execute()
	{
		// Get clamped delay time
		float CurrentInputDelayTime = GetInputDelayTimeMsec();

		// Check to see if our delay amount has changed
		if (!FMath::IsNearlyEqual(PrevDelayTimeMsec, CurrentInputDelayTime))
		{
			PrevDelayTimeMsec = CurrentInputDelayTime;
			DelayBuffer.SetEasedDelayMsec(PrevDelayTimeMsec);
		}

		const float* InputAudio = AudioInput->GetData();

		float* OutputAudio = AudioOutput->GetData();
		int32 NumFrames = AudioInput->Num();

		// Clamp the feedback amount to make sure it's bounded. Clamp to a number slightly less than 1.0
		float FeedbackAmount = FMath::Clamp(*Feedback, 0.0f, 1.0f - SMALL_NUMBER);
		float CurrentDryLevel = FMath::Clamp(*DryLevel, 0.0f, 1.0f);
		float CurrentWetLevel = FMath::Clamp(*WetLevel, 0.0f, 1.0f);

		if (FMath::IsNearlyZero(FeedbackAmount))
		{
			FeedbackSample = 0.0f;

			for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
			{
				OutputAudio[FrameIndex] = CurrentWetLevel * DelayBuffer.ProcessAudioSample(InputAudio[FrameIndex]) + CurrentDryLevel * InputAudio[FrameIndex];
			}
		}
		else
		{
			// There is some amount of feedback so we do the feedback mixing
			for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
			{
				OutputAudio[FrameIndex] = CurrentWetLevel * DelayBuffer.ProcessAudioSample(InputAudio[FrameIndex] + FeedbackSample * FeedbackAmount) + CurrentDryLevel * InputAudio[FrameIndex];
				FeedbackSample = OutputAudio[FrameIndex];
			}
		}
	}

	const FVertexInterface& FDelayOperator::GetVertexInterface()
	{
		using namespace Delay;

		FDataVertexMetadata MaxDelayTimeMetadata = METASOUND_GET_PARAM_METADATA(InParamMaxDelayTime);
		MaxDelayTimeMetadata.bIsAdvancedDisplay = true;

		static const FVertexInterface Interface(
			FInputVertexInterface(
				TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InParamAudioInput)),
				TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InParamDelayTime), 1.0f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InParamDryLevel), 0.0f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InParamWetLevel), 1.0f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InParamFeedbackAmount), 0.0f),
				TInputConstructorVertex<FTime>(METASOUND_GET_PARAM_NAME(InParamMaxDelayTime), MaxDelayTimeMetadata, DefaultMaxDelaySeconds)
			),
			FOutputVertexInterface(
				TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutParamAudio))
			)
		);

		return Interface;
	}

	const FNodeClassMetadata& FDelayOperator::GetNodeInfo()
	{
		auto InitNodeInfo = []() -> FNodeClassMetadata
		{
			FNodeClassMetadata Info;
			Info.ClassName = { StandardNodes::Namespace, "Delay", StandardNodes::AudioVariant };
			Info.MajorVersion = 1;
			Info.MinorVersion = 1;
			Info.DisplayName = METASOUND_LOCTEXT("DelayNode_DisplayName", "Delay");
			Info.Description = METASOUND_LOCTEXT("DelayNode_Description", "Delays an audio buffer by the specified amount.");
			Info.Author = PluginAuthor;
			Info.PromptIfMissing = PluginNodeMissingPrompt;
			Info.DefaultInterface = GetVertexInterface();
			Info.CategoryHierarchy.Emplace(NodeCategories::Delays);
			return Info;
		};

		static const FNodeClassMetadata Info = InitNodeInfo();

		return Info;
	}

	TUniquePtr<IOperator> FDelayOperator::CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
	{
		using namespace Delay; 

		const FInputVertexInterfaceData& InputData = InParams.InputData;

		FAudioBufferReadRef AudioIn = InputData.GetOrConstructDataReadReference<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InParamAudioInput), InParams.OperatorSettings);
		FTimeReadRef DelayTime = InputData.GetOrCreateDefaultDataReadReference<FTime>(METASOUND_GET_PARAM_NAME(InParamDelayTime), InParams.OperatorSettings);
		FFloatReadRef DryLevel = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InParamDryLevel), InParams.OperatorSettings);
		FFloatReadRef WetLevel = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InParamWetLevel), InParams.OperatorSettings);
		FFloatReadRef Feedback = InputData.GetOrCreateDefaultDataReadReference<float>(METASOUND_GET_PARAM_NAME(InParamFeedbackAmount), InParams.OperatorSettings);
		FTime MaxDelayTime = InputData.GetOrCreateDefaultValue<FTime>(METASOUND_GET_PARAM_NAME(InParamMaxDelayTime), InParams.OperatorSettings);

		return MakeUnique<FDelayOperator>(InParams, AudioIn, DelayTime, DryLevel, WetLevel, Feedback, MaxDelayTime.GetSeconds());
	}

	class FDelayNode : public FNodeFacade
	{
	public:
		/**
		 * Constructor used by the Metasound Frontend.
		 */
		FDelayNode(const FNodeInitData& InitData)
			: FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FDelayOperator>())
		{
		}
	};


	METASOUND_REGISTER_NODE(FDelayNode)
}

#undef LOCTEXT_NAMESPACE
