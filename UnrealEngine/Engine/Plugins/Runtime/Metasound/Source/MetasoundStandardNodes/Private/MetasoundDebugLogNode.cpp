// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interfaces/MetasoundFrontendSourceInterface.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundStandardNodesNames.h"
#include "MetasoundTrigger.h"


#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_DebugLogNode"

namespace Metasound
{
	namespace MetasoundPrintLogNodePrivate
	{
		//Creates Metadata for the Print Log Node
		FNodeClassMetadata CreateNodeClassMetadata(const FName& InDataTypeName, const FName& InOperatorName, const FText& InDisplayName, const FText& InDescription, const FVertexInterface& InDefaultInterface)
		{
			FNodeClassMetadata Metadata
			{
				FNodeClassName { "Print Log", InOperatorName, InDataTypeName },
				1, // Major Version
				0, // Minor Version
				InDisplayName,
				InDescription,
				PluginAuthor,
				PluginNodeMissingPrompt,
				InDefaultInterface,
				{ NodeCategories::Debug },
				{ },
				FNodeDisplayStyle()
			};

			return Metadata;
		}
	}

	//Getters for the name of each parameter used in Print Log
	namespace PrintLogVertexNames
	{
		METASOUND_PARAM(InputTrigger, "Trigger", "Trigger to write the set value to the log.")
		METASOUND_PARAM(InputLabel, "Label", "The label to attach to the value that will be logged.")
		METASOUND_PARAM(InputValueToLog, "Value To Log", "The value to record to the log when triggered.")
	}


	template<typename PrintLogType>
	class TPrintLogOperator : public TExecutableOperator<TPrintLogOperator<PrintLogType>>
	{
		public:
			using FArrayDataReadReference = TDataReadReference<PrintLogType>;

			static const FVertexInterface& GetDefaultInterface()
			{
				using namespace PrintLogVertexNames;
				static const FVertexInterface DefaultInterface(
					FInputVertexInterface(

						TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTrigger)),
						TInputDataVertex<FString>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputLabel)),
						TInputDataVertex<PrintLogType>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputValueToLog))
					),
					FOutputVertexInterface(
					)
				);

				return DefaultInterface;
			}

			static const FNodeClassMetadata& GetNodeInfo()
			{
				auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
				{
					FName DataTypeName = GetMetasoundDataTypeName<PrintLogType>();
					FName OperatorName = TEXT("Print Log");
					FText NodeDisplayName = METASOUND_LOCTEXT_FORMAT("PrintLogDisplayNamePattern", "Print Log ({0})", GetMetasoundDataTypeDisplayText<PrintLogType>());
					const FText NodeDescription = METASOUND_LOCTEXT("PrintLogOpDescription", "Used to record values to the log, on trigger");
					FVertexInterface NodeInterface = GetDefaultInterface();

					return MetasoundPrintLogNodePrivate::CreateNodeClassMetadata(DataTypeName, OperatorName, NodeDisplayName, NodeDescription, NodeInterface);
				};

				static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
				return Metadata;
			}

			static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, TArray<TUniquePtr<IOperatorBuildError>>& OutErrors)
			{
				using namespace PrintLogVertexNames;

				const FInputVertexInterface& InputInterface = InParams.Node.GetVertexInterface().GetInputInterface();
				const FDataReferenceCollection& InputCollection = InParams.InputDataReferences;

				FTriggerReadRef Trigger = InputCollection.GetDataReadReferenceOrConstruct<FTrigger>(METASOUND_GET_PARAM_NAME(InputTrigger), InParams.OperatorSettings);

				TDataReadReference<FString> Label = InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<FString>(InputInterface, METASOUND_GET_PARAM_NAME(InputLabel), InParams.OperatorSettings);
				TDataReadReference<PrintLogType> ValueToLog = InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<PrintLogType>(InputInterface, METASOUND_GET_PARAM_NAME(InputValueToLog), InParams.OperatorSettings);

				FString GraphNameFull = InParams.Environment.GetValue<FString>(Frontend::SourceInterface::Environment::GraphName);
				TArray<FString> ParsedString;
				GraphNameFull.ParseIntoArray(ParsedString, TEXT("."), true);

				const FString& GraphName = ParsedString.Last();

				return MakeUnique<TPrintLogOperator<PrintLogType>>(InParams.OperatorSettings, Trigger, Label, ValueToLog, GraphName);
			}


			TPrintLogOperator(const FOperatorSettings& InSettings, TDataReadReference<FTrigger> InTrigger, TDataReadReference<FString> InLabelPrintLog, TDataReadReference<PrintLogType> InValueToLogPrintLog, const FString& InGraphName)
				: Trigger(InTrigger)
				, Label(InLabelPrintLog)
				, ValueToLog(InValueToLogPrintLog)
				, GraphName(InGraphName)
			{
			}

			virtual ~TPrintLogOperator() = default;


			virtual FDataReferenceCollection GetInputs() const override
			{
				using namespace PrintLogVertexNames;
				FDataReferenceCollection Inputs;

				Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTrigger), Trigger);
				Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputLabel), Label);
				Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputValueToLog), ValueToLog);

				return Inputs;
			}

			virtual FDataReferenceCollection GetOutputs() const override
			{
				FDataReferenceCollection Outputs;

				return Outputs;
			}

			void Execute()
			{
				if (*Trigger)
				{
					UE_LOG(LogMetaSound, Display, TEXT("[%s]: %s %s"), *GraphName, *(*Label), *LexToString(*ValueToLog));
				}
			}

		private:

			TDataReadReference<FTrigger> Trigger;
			TDataReadReference<FString> Label;
			TDataReadReference<PrintLogType> ValueToLog;

			FString GraphName;
	};

	/** TPrintLogNode
	 *
	 *  Records a value to the log when triggered
	 */
	template<typename PrintLogType>
	class METASOUNDSTANDARDNODES_API TPrintLogNode : public FNodeFacade
	{
		public:
			/**
			 * Constructor used by the Metasound Frontend.
			 */
			TPrintLogNode(const FNodeInitData& InInitData)
				: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<TPrintLogOperator<PrintLogType>>())
			{}

			virtual ~TPrintLogNode() = default;
	};

	using FPrintLogNodeInt32 = TPrintLogNode<int32>;
	METASOUND_REGISTER_NODE(FPrintLogNodeInt32)

	using FPrintLogNodeFloat = TPrintLogNode<float>;
	METASOUND_REGISTER_NODE(FPrintLogNodeFloat)

	using FPrintLogNodeBool = TPrintLogNode<bool>;
	METASOUND_REGISTER_NODE(FPrintLogNodeBool)

	using FPrintLogNodeString = TPrintLogNode<FString>;
	METASOUND_REGISTER_NODE(FPrintLogNodeString)
}

#undef LOCTEXT_NAMESPACE
