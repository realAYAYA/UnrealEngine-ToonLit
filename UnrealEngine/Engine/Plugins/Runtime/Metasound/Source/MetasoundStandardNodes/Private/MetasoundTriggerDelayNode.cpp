// Copyright Epic Games, Inc. All Rights Reserved.
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
#include "MetasoundStandardNodesNames.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundParamHelper.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_DelayNode"

namespace Metasound
{
	namespace TriggerDelayVertexNames
	{
		METASOUND_PARAM(InputInTriggerDelay, "In", "Input trigger which results in a delayed trigger.");
		METASOUND_PARAM(InputResetDelay, "Reset", "Resets the trigger delay, clearing the execution task if pending.");
		METASOUND_PARAM(InputDelayTime, "Delay Time", "Time to delay and execute deferred trigger in seconds.");
		METASOUND_PARAM(OutputOnTrigger, "Out", "The delayed output trigger.");
	}

	class FTriggerDelayOperator : public TExecutableOperator<FTriggerDelayOperator>
	{
		public:
			static const FNodeClassMetadata& GetNodeInfo();
			static const FVertexInterface& GetVertexInterface();
			static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors);

			FTriggerDelayOperator(const FOperatorSettings& InSettings, const FTriggerReadRef& InTriggerReset, const FTriggerReadRef& InTriggerIn, const FTimeReadRef& InDelay);

			virtual FDataReferenceCollection GetInputs() const override;
			virtual FDataReferenceCollection GetOutputs() const override;

			void Execute();

		private:
			float SampleRate;
			FTriggerReadRef TriggerIn;
			FTriggerReadRef TriggerReset;
			FTriggerWriteRef TriggerOut;

			FTimeReadRef DelayTime;
	};

	FTriggerDelayOperator::FTriggerDelayOperator(const FOperatorSettings& InSettings, const FTriggerReadRef& InTriggerReset, const FTriggerReadRef& InTriggerIn, const FTimeReadRef& InTimeDelay)
	: SampleRate(InSettings.GetSampleRate())
	, TriggerIn(InTriggerIn)
	, TriggerReset(InTriggerReset)
	, TriggerOut(FTriggerWriteRef::CreateNew(InSettings))
	, DelayTime(InTimeDelay)
	{
	}

	FDataReferenceCollection FTriggerDelayOperator::GetInputs() const
	{
		using namespace TriggerDelayVertexNames;

		FDataReferenceCollection InputDataReferences;
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputInTriggerDelay), TriggerIn);
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputResetDelay), TriggerReset);
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputDelayTime), DelayTime);
		return InputDataReferences;
	}

	FDataReferenceCollection FTriggerDelayOperator::GetOutputs() const
	{
		using namespace TriggerDelayVertexNames;

		FDataReferenceCollection OutputDataReferences;
		OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputOnTrigger), TriggerOut);

		return OutputDataReferences;
	}

	void FTriggerDelayOperator::Execute()
	{
		// Advance internal counter to get rid of old triggers.
		TriggerOut->AdvanceBlock();

		TriggerIn->ExecuteBlock(
			[&](int32 StartFrame, int32 EndFrame)
			{
			},
			[this](int32 StartFrame, int32 EndFrame)
			{
				const int32 FrameToTrigger = FMath::Max(0, FMath::RoundToInt(DelayTime->GetSeconds() * SampleRate)) + StartFrame;
				TriggerOut->TriggerFrame(FrameToTrigger);
			}
		);

		TriggerReset->ExecuteBlock(
			[&](int32 StartFrame, int32 EndFrame)
			{
			},
			[this](int32 StartFrame, int32 EndFrame)
			{
				TriggerOut->RemoveAfter(StartFrame);
			}
		);
	}

	TUniquePtr<IOperator> FTriggerDelayOperator::CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors)
	{
		using namespace TriggerDelayVertexNames;

		const FInputVertexInterface& InputInterface = GetVertexInterface().GetInputInterface();

		FTimeReadRef Delay = InParams.InputDataReferences.GetDataReadReferenceOrConstructWithVertexDefault<FTime>(InputInterface, METASOUND_GET_PARAM_NAME(InputDelayTime), InParams.OperatorSettings);

		FTriggerReadRef TriggerIn = InParams.InputDataReferences.GetDataReadReferenceOrConstruct<FTrigger>(METASOUND_GET_PARAM_NAME(InputInTriggerDelay), InParams.OperatorSettings);
		FTriggerReadRef TriggerReset = InParams.InputDataReferences.GetDataReadReferenceOrConstruct<FTrigger>(METASOUND_GET_PARAM_NAME(InputResetDelay), InParams.OperatorSettings);

		return MakeUnique<FTriggerDelayOperator>(InParams.OperatorSettings, TriggerReset, TriggerIn, Delay);
	}

	const FVertexInterface& FTriggerDelayOperator::GetVertexInterface()
	{
		using namespace TriggerDelayVertexNames;

		static const FVertexInterface Interface(
			FInputVertexInterface(
				TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputInTriggerDelay)),
				TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputResetDelay)),
				TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDelayTime), 1.0f)
			),
			FOutputVertexInterface(
				TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputOnTrigger))
		)
		);

		return Interface;
	}

	const FNodeClassMetadata& FTriggerDelayOperator::GetNodeInfo()
	{
		auto InitNodeInfo = []() -> FNodeClassMetadata
		{
			FNodeClassMetadata Info;
			Info.ClassName = {StandardNodes::Namespace, TEXT("Trigger Delay"), TEXT("")};
			Info.MajorVersion = 1;
			Info.MinorVersion = 0;
			Info.DisplayName = METASOUND_LOCTEXT("Metasound_TriggerDelayNodeDisplayName", "Trigger Delay");
			Info.Description = METASOUND_LOCTEXT("Metasound_TriggerDelayNodeDescription", "Executes output trigger after the given delay time from the most recent execution of the input trigger .");
			Info.Author = PluginAuthor;
			Info.PromptIfMissing = PluginNodeMissingPrompt;
			Info.DefaultInterface = GetVertexInterface();
			Info.CategoryHierarchy.Emplace(NodeCategories::Trigger);

			return Info;
		};

		static const FNodeClassMetadata Info = InitNodeInfo();
		return Info;
	}

	class METASOUNDSTANDARDNODES_API FTriggerDelayNode : public FNodeFacade
	{
	public:
		FTriggerDelayNode(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<FTriggerDelayOperator>())
		{
		}
	};

	METASOUND_REGISTER_NODE(FTriggerDelayNode)
}

#undef LOCTEXT_NAMESPACE // MetasoundTriggerDelayNode
