// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundFacade.h"

#include "Internationalization/Text.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundStandardNodesNames.h"
#include "MetasoundTrigger.h"
#include "MetasoundTime.h"
#include "DSP/VolumeFader.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_InterpNode"

namespace Metasound
{
	namespace InterpToVertexNames
	{
		METASOUND_PARAM(InParamTarget, "Target", "Target value.")
		METASOUND_PARAM(InParamInterpTime, "Interp Time", "The time to interpolate from the current value to the target value.")
		METASOUND_PARAM(OutParamValue, "Value", "The current value.")
	}

	/** FInterpToNode
	*
	*  Interpolates to a target value over a given time.
	*/
	class METASOUNDSTANDARDNODES_API FInterpToNode : public FNodeFacade
	{
	public:
		/**
		 * Constructor used by the Metasound Frontend.
		 */
		FInterpToNode(const FNodeInitData& InitData);
	};

	class FInterpToOperator : public TExecutableOperator<FInterpToOperator>
	{
	public:

		static const FNodeClassMetadata& GetNodeInfo();
		static const FVertexInterface& GetVertexInterface();
		static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors);

		FInterpToOperator(const FOperatorSettings& InSettings, const FFloatReadRef& InTargetValue, const FTimeReadRef& InInterpTime);

		virtual FDataReferenceCollection GetInputs() const override;
		virtual FDataReferenceCollection GetOutputs() const override;
		void Execute();

	private:
		// The target value of the lerp. The output will lerp from it's current value to the output value.
		FFloatReadRef TargetValue;

		// The amount of time to do lerp
		FTimeReadRef InterpTime;

		// The current output value.
		FFloatWriteRef ValueOutput;

		// Volume fader object which performs the interpolating
		Audio::FVolumeFader VolumeFader;

		// The time-delta per block
		float BlockTimeDelta = 0.0f;

		// The previous target value
		float PreviousTargetValue = 0.0f;
	};

	FInterpToOperator::FInterpToOperator(const FOperatorSettings& InSettings, const FFloatReadRef& InTargetValue, const FTimeReadRef& InInterpTime)
		: TargetValue(InTargetValue)
		, InterpTime(InInterpTime)
		, ValueOutput(FFloatWriteRef::CreateNew(*TargetValue))
	{
		// Set the fade to start at the value specified in the current value
		VolumeFader.SetVolume(*TargetValue);

		float BlockRate = InSettings.GetActualBlockRate();
		BlockTimeDelta = 1.0f / BlockRate;

		PreviousTargetValue = *TargetValue;

		*ValueOutput = *TargetValue;
	}

	FDataReferenceCollection FInterpToOperator::GetInputs() const
	{
		using namespace InterpToVertexNames;

		FDataReferenceCollection InputDataReferences;
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InParamTarget), FFloatReadRef(TargetValue));
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InParamInterpTime), FTimeReadRef(InterpTime));

		return InputDataReferences;
	}

	FDataReferenceCollection FInterpToOperator::GetOutputs() const
	{
		using namespace InterpToVertexNames;

		FDataReferenceCollection OutputDataReferences;
		OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutParamValue), FFloatReadRef(ValueOutput));
		return OutputDataReferences;
	}

	void FInterpToOperator::Execute()
	{
		// Update the value output with the current value in case it was changed
		if (!FMath::IsNearlyEqual(PreviousTargetValue, *TargetValue))
		{
			PreviousTargetValue = *TargetValue;
			// Start the volume fader on the interp trigger
			float FadeSeconds = (float)InterpTime->GetSeconds();
			VolumeFader.StartFade(*TargetValue, FadeSeconds, Audio::EFaderCurve::Linear);
		}

		// Perform the fading
		if (VolumeFader.IsFading())
		{
			VolumeFader.Update(BlockTimeDelta);
		}

		// Update the fader w/ the current volume
		*ValueOutput = VolumeFader.GetVolume();
	}

	const FVertexInterface& FInterpToOperator::GetVertexInterface()
	{
		using namespace InterpToVertexNames;

		static const FVertexInterface Interface(
			FInputVertexInterface(
				TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InParamInterpTime), 0.1f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InParamTarget), 1.0f)
			),
			FOutputVertexInterface(
				TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutParamValue))
			)
		);

		return Interface;
	}

	const FNodeClassMetadata& FInterpToOperator::GetNodeInfo()
	{
		auto InitNodeInfo = []() -> FNodeClassMetadata
		{
			FNodeClassMetadata Info;
			Info.ClassName = { StandardNodes::Namespace, TEXT("InterpTo"), StandardNodes::AudioVariant };
			Info.MajorVersion = 1;
			Info.MinorVersion = 0;
			Info.DisplayName = METASOUND_LOCTEXT("Metasound_InterpDisplayName", "InterpTo");
			Info.Description = METASOUND_LOCTEXT("Metasound_InterpNodeDescription", "Interpolates between the current value and a target value over the specified time.");
			Info.Author = PluginAuthor;
			Info.PromptIfMissing = PluginNodeMissingPrompt;
			Info.DefaultInterface = GetVertexInterface();
			Info.Keywords.Add(METASOUND_LOCTEXT("LerpKeyword", "Lerp"));

			return Info;
		};

		static const FNodeClassMetadata Info = InitNodeInfo();

		return Info;
	}


	FInterpToNode::FInterpToNode(const FNodeInitData& InitData)
		: FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FInterpToOperator>())
	{
	}

	TUniquePtr<IOperator> FInterpToOperator::CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors)
	{
		using namespace InterpToVertexNames; 

		const FInterpToNode& InterpToNode = static_cast<const FInterpToNode&>(InParams.Node);
		const FDataReferenceCollection& InputCollection = InParams.InputDataReferences;
		const FInputVertexInterface& InputInterface = GetVertexInterface().GetInputInterface();

		FFloatReadRef TargetValue = InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, METASOUND_GET_PARAM_NAME(InParamTarget), InParams.OperatorSettings);
		FTimeReadRef InterpTime = InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<FTime>(InputInterface, METASOUND_GET_PARAM_NAME(InParamInterpTime), InParams.OperatorSettings);

		return MakeUnique<FInterpToOperator>(InParams.OperatorSettings, TargetValue, InterpTime);
	}


	METASOUND_REGISTER_NODE(FInterpToNode)
}

#undef LOCTEXT_NAMESPACE //MetasoundStandardNodes_InterpNode
