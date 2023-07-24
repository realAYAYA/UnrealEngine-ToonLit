// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundTriggerRepeatNode.h"

#include "CoreMinimal.h"

#include "MetasoundBuilderInterface.h"
#include "MetasoundDataReferenceCollection.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNode.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundPrimitives.h"
#include "MetasoundSampleCounter.h"
#include "MetasoundStandardNodesNames.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundParamHelper.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_TriggerRepeatNode"

namespace Metasound
{
	METASOUND_REGISTER_NODE(FTriggerRepeatNode)

	namespace TriggerRepeatVertexNames
	{
		METASOUND_PARAM(InputStart, "Start", "Starts executing periodic output triggers.");
		METASOUND_PARAM(InputStop, "Stop", "Stops executing periodic output triggers.");
		METASOUND_PARAM(InputPeriod, "Period", "The period to trigger in seconds.");
		METASOUND_PARAM(RepeatOutputOnTrigger, "RepeatOut", "The periodically generated output trigger.");
	}

	class FTriggerRepeatOperator : public TExecutableOperator<FTriggerRepeatOperator>
	{
		public:
			static const FNodeClassMetadata& GetNodeInfo();
			static const FVertexInterface& GetVertexInterface();
			static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors);

			FTriggerRepeatOperator(const FOperatorSettings& InSettings, const FTriggerReadRef& InTriggerEnable, const FTriggerReadRef& InTriggerDisable, const FTimeReadRef& InPeriod);

			virtual FDataReferenceCollection GetInputs() const override;

			virtual FDataReferenceCollection GetOutputs() const override;

			void Execute();

		private:
			bool bEnabled;

			FTriggerWriteRef TriggerOut;

			FTriggerReadRef TriggerEnable;
			FTriggerReadRef TriggerDisable;

			FTimeReadRef Period;

			FSampleCount BlockSize;
			FSampleCounter SampleCounter;
	};

	FTriggerRepeatOperator::FTriggerRepeatOperator(const FOperatorSettings& InSettings, const FTriggerReadRef& InTriggerEnable, const FTriggerReadRef& InTriggerDisable, const FTimeReadRef& InPeriod)
		: bEnabled(false)
		, TriggerOut(FTriggerWriteRef::CreateNew(InSettings))
		, TriggerEnable(InTriggerEnable)
		, TriggerDisable(InTriggerDisable)
		, Period(InPeriod)
		, BlockSize(InSettings.GetNumFramesPerBlock())
		, SampleCounter(0, InSettings.GetSampleRate())
	{
	}

	FDataReferenceCollection FTriggerRepeatOperator::GetInputs() const
	{
		using namespace TriggerRepeatVertexNames;

		FDataReferenceCollection InputDataReferences;
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputStart), TriggerEnable);
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputStop), TriggerDisable);
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputPeriod), Period);
		return InputDataReferences;
	}

	FDataReferenceCollection FTriggerRepeatOperator::GetOutputs() const
	{
		using namespace TriggerRepeatVertexNames;

		FDataReferenceCollection OutputDataReferences;
		OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(RepeatOutputOnTrigger), TriggerOut);

		return OutputDataReferences;
	}

	void FTriggerRepeatOperator::Execute()
	{
		TriggerEnable->ExecuteBlock([](int32, int32) { },
			[this](int32 StartFrame, int32 EndFrame)
			{
				bEnabled = true;
				SampleCounter.SetNumSamples(0);
			}
		);

		TriggerDisable->ExecuteBlock([](int32, int32) { },
			[this](int32 StartFrame, int32 EndFrame)
			{
				bEnabled = false;
			}
		);

		// Advance internal counter to get rid of old triggers.
		TriggerOut->AdvanceBlock();

		if (bEnabled)
		{
			FSampleCount PeriodInSamples = FSampleCounter::FromTime(*Period, SampleCounter.GetSampleRate()).GetNumSamples();

			// Time must march on, can't stay in the now forever.
			PeriodInSamples = FMath::Max(static_cast<FSampleCount>(1), PeriodInSamples);

			while ((SampleCounter - BlockSize).GetNumSamples() <= 0)
			{
				const int32 StartOffset = static_cast<int32>(SampleCounter.GetNumSamples());
				TriggerOut->TriggerFrame(StartOffset);
				SampleCounter += PeriodInSamples;
			}

			SampleCounter -= BlockSize;
		}
	}


	TUniquePtr<IOperator> FTriggerRepeatOperator::CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors)
	{
		using namespace TriggerRepeatVertexNames;

		const FTriggerRepeatNode& PeriodicTriggerNode = static_cast<const FTriggerRepeatNode&>(InParams.Node);
		const FInputVertexInterface& InputInterface = GetVertexInterface().GetInputInterface();

		FTriggerReadRef TriggerEnable = InParams.InputDataReferences.GetDataReadReferenceOrConstruct<FTrigger>(METASOUND_GET_PARAM_NAME(InputStart), InParams.OperatorSettings);
		FTriggerReadRef TriggerDisable = InParams.InputDataReferences.GetDataReadReferenceOrConstruct<FTrigger>(METASOUND_GET_PARAM_NAME(InputStop), InParams.OperatorSettings);
		FTimeReadRef Period = InParams.InputDataReferences.GetDataReadReferenceOrConstructWithVertexDefault<FTime>(InputInterface, METASOUND_GET_PARAM_NAME(InputPeriod), InParams.OperatorSettings);

		return MakeUnique<FTriggerRepeatOperator>(InParams.OperatorSettings, TriggerEnable, TriggerDisable, Period);
	}

	const FVertexInterface& FTriggerRepeatOperator::GetVertexInterface()
	{
		using namespace TriggerRepeatVertexNames;

		static const FVertexInterface Interface(
			FInputVertexInterface(
				TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputStart)),
				TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputStop)),
				TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputPeriod), 0.2f)
			),
			FOutputVertexInterface(
				TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(RepeatOutputOnTrigger))
			)
		);

		return Interface;
	}

	const FNodeClassMetadata& FTriggerRepeatOperator::GetNodeInfo()
	{
		auto InitNodeInfo = []() -> FNodeClassMetadata
		{
			FNodeClassMetadata Info;
			Info.ClassName = { StandardNodes::Namespace, TEXT("TriggerRepeat"), TEXT("") };
			Info.MajorVersion = 1;
			Info.MinorVersion = 0;
			Info.DisplayName = METASOUND_LOCTEXT("Metasound_PeriodicTriggerNodeDisplayName", "Trigger Repeat");
			Info.Description = METASOUND_LOCTEXT("Metasound_PeriodicTriggerNodeDescription", "Emits a trigger periodically based on the period duration given.");
			Info.Author = PluginAuthor;
			Info.PromptIfMissing = PluginNodeMissingPrompt;
			Info.DefaultInterface = GetVertexInterface();
			Info.CategoryHierarchy.Emplace(NodeCategories::Trigger);
			Info.Keywords.Add(METASOUND_LOCTEXT("TriggerRepeatLoopKeyword", "Loop"));

			return Info;
		};

		static const FNodeClassMetadata Info = InitNodeInfo();

		return Info;
	}

	FTriggerRepeatNode::FTriggerRepeatNode(const FNodeInitData& InInitData)
	:	FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<FTriggerRepeatOperator>())
	{
	}
}

#undef LOCTEXT_NAMESPACE //MetasoundPeriodicTriggerNode
