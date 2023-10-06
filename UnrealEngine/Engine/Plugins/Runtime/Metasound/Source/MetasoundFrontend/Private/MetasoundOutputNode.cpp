// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundOutputNode.h"
#include "MetasoundFrontendDataTypeRegistry.h"

#define LOCTEXT_NAMESPACE "MetasoundGraphCore"

namespace Metasound
{
	namespace OutputNodePrivate
	{
		static const FLazyName ValueVertexAccessVariantName("Constructor");
		static const FLazyName ReferenceVertexAccessVariantName("");

		FOutputOperator::FOutputOperator(const FVertexName& InVertexName, const FAnyDataReference& InDataReference)
		: VertexName(InVertexName)
		, DataReference(InDataReference)
		{
		}

		FDataReferenceCollection FOutputOperator::GetInputs() const
		{
			// Slated for deprecation
			return {};
		}

		FDataReferenceCollection FOutputOperator::GetOutputs() const
		{
			// Slated for deprecation
			return {};
		}

		void FOutputOperator::BindInputs(FInputVertexInterfaceData& InVertexData) 
		{
			InVertexData.BindVertex(VertexName, DataReference);
		}

		void FOutputOperator::BindOutputs(FOutputVertexInterfaceData& InVertexData) 
		{
			InVertexData.BindVertex(VertexName, DataReference);
		}

		FOutputNode::FOutputOperatorFactory::FOutputOperatorFactory(const FVertexName& InVertexName, const FName& InDataTypeName, EVertexAccessType InVertexAccessType)
		: VertexName(InVertexName)
		, DataTypeName(InDataTypeName)
		, VertexAccessType(InVertexAccessType)
		{
		}

		TUniquePtr<IOperator> FOutputNode::FOutputOperatorFactory::CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			using namespace OutputNodePrivate;

			const FOutputNode& OutputNode = static_cast<const FOutputNode&>(InParams.Node);

			// Use data reference if it is passed in. 
			if (const FAnyDataReference* Ref = InParams.InputData.FindDataReference(VertexName))
			{
				checkf(Ref->GetDataTypeName() == DataTypeName, TEXT("Mismatched data type names on output node (%s). Expected (%s), received (%s)."), *VertexName.ToString(), *DataTypeName.ToString(), *(Ref->GetDataTypeName().ToString()));
				return MakeUnique<FOutputOperator>(VertexName, *Ref);
			}
			else 
			{
				// Make data reference if none are passed in. 
				Frontend::IDataTypeRegistry& DataTypeRegistry = Frontend::IDataTypeRegistry::Get();
				if (const Frontend::IDataTypeRegistryEntry* Entry = DataTypeRegistry.FindDataTypeRegistryEntry(DataTypeName))
				{
					FLiteral DefaultLiteral = DataTypeRegistry.CreateDefaultLiteral(DataTypeName);
					TOptional<FAnyDataReference> DataReference = Entry->CreateDataReference(EDataReferenceAccessType::Value, DefaultLiteral, InParams.OperatorSettings);
					if (DataReference.IsSet())
					{
						return MakeUnique<FOutputOperator>(VertexName, *DataReference);
					}
				}
			}

			// Do not make output operator if no data reference is available. 
			OutResults.Errors.Emplace(MakeUnique<FMissingOutputNodeInputReferenceError>(InParams.Node));
			return TUniquePtr<IOperator>(nullptr);
		}

		FName FOutputNode::GetVariantName(EVertexAccessType InVertexAccessType)
		{
			if (EVertexAccessType::Value == InVertexAccessType)
			{
				return ValueVertexAccessVariantName;
			}
			else
			{
				return ReferenceVertexAccessVariantName;
			}
		}

		FVertexInterface FOutputNode::GetVertexInterface(const FVertexName& InVertexName, const FName& InDataTypeName, EVertexAccessType InVertexAccessType)
		{
			static const FText VertexDescription = METASOUND_LOCTEXT("Metasound_OutputVertexDescription", "Output from the parent Metasound graph.");

			return FVertexInterface(
				FInputVertexInterface(
					FInputDataVertex(InVertexName, InDataTypeName, FDataVertexMetadata{VertexDescription}, InVertexAccessType)
				),
				FOutputVertexInterface(
					FOutputDataVertex(InVertexName, InDataTypeName, FDataVertexMetadata{VertexDescription}, InVertexAccessType)
				)
			);
		}

		FNodeClassMetadata FOutputNode::GetNodeInfo(const FVertexName& InVertexName, const FName& InDataTypeName, EVertexAccessType InVertexAccessType)
		{
			FNodeClassMetadata Info;

			Info.ClassName = { "Output", InDataTypeName, GetVariantName(InVertexAccessType)};
			Info.MajorVersion = 1;
			Info.MinorVersion = 0;
			Info.Description = METASOUND_LOCTEXT("Metasound_OutputNodeDescription", "Output from the parent Metasound graph.");
			Info.Author = PluginAuthor;
			Info.PromptIfMissing = PluginNodeMissingPrompt;
			Info.DefaultInterface = GetVertexInterface(InVertexName, InDataTypeName, InVertexAccessType);

			return Info;
		}

		FOutputNode::FOutputNode(const FName& InInstanceName, const FGuid& InInstanceID, const FVertexName& InVertexName, const FName& InDataTypeName, EVertexAccessType InVertexAccessType)
		:	FNode(InInstanceName, InInstanceID, GetNodeInfo(InVertexName, InDataTypeName, InVertexAccessType))
		,	Factory(MakeShared<FOutputOperatorFactory, ESPMode::ThreadSafe>(InVertexName, InDataTypeName, InVertexAccessType))
		{
		}

		const FVertexInterface& FOutputNode::GetVertexInterface() const
		{
			return GetMetadata().DefaultInterface;
		}

		bool FOutputNode::SetVertexInterface(const FVertexInterface& InInterface)
		{
			return GetVertexInterface() == InInterface;
		}

		bool FOutputNode::IsVertexInterfaceSupported(const FVertexInterface& InInterface) const 
		{
			return GetVertexInterface() == InInterface;
		}

		TSharedRef<IOperatorFactory, ESPMode::ThreadSafe> FOutputNode::GetDefaultOperatorFactory() const
		{
			return Factory;
		}
	}
}

#undef LOCTEXT_NAMESPACE // MetasoundOutputNode
