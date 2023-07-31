// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundEditorGraphSchema.h"

#include "Algo/AnyOf.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GraphEditor.h"
#include "GraphEditorActions.h"
#include "GraphEditorSettings.h"
#include "HAL/IConsoleManager.h"
#include "Layout/SlateRect.h"
#include "Metasound.h"
#include "MetasoundAssetBase.h"
#include "MetasoundAssetManager.h"
#include "MetasoundDataReference.h"
#include "MetasoundEditor.h"
#include "MetasoundEditorCommands.h"
#include "MetasoundEditorGraph.h"
#include "MetasoundEditorGraphBuilder.h"
#include "MetasoundEditorGraphInputNode.h"
#include "MetasoundEditorGraphNode.h"
#include "MetasoundEditorGraphValidation.h"
#include "MetasoundEditorModule.h"
#include "MetasoundEditorSettings.h"
#include "MetasoundFrontend.h"
#include "MetasoundFrontendGraphLinter.h"
#include "MetasoundFrontendNodesCategories.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundFrontendSearchEngine.h"
#include "MetasoundLiteral.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundUObjectRegistry.h"
#include "MetasoundVariableNodes.h"
#include "NodeTemplates/MetasoundFrontendNodeTemplateReroute.h"
#include "ScopedTransaction.h"
#include "Styling/SlateStyleRegistry.h"
#include "Toolkits/ToolkitManager.h"
#include "ToolMenus.h"
#include "UObject/NoExportTypes.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetasoundEditorGraphSchema)

#define LOCTEXT_NAMESPACE "MetaSoundEditor"

static int32 EnableAllVersionsMetaSoundNodeClassCreationCVar = 0;
FAutoConsoleVariableRef CVarEnableAllVersionsMetaSoundNodeClassCreation(
	TEXT("au.MetaSound.EnableAllVersionsNodeClassCreation"),
	EnableAllVersionsMetaSoundNodeClassCreationCVar,
	TEXT("Enable creating nodes for major versions of deprecated MetaSound classes in the Editor.\n")
	TEXT("0: Disabled (default), !0: Enabled"),
	ECVF_Default);

namespace Metasound
{
	namespace Editor
	{
		const FText& GetContextGroupDisplayName(EPrimaryContextGroup InContextGroup)
		{
			switch (InContextGroup)
			{
				case EPrimaryContextGroup::Inputs:
					return NodeCategories::Inputs;

				case EPrimaryContextGroup::Outputs:
					return NodeCategories::Outputs;

				case EPrimaryContextGroup::Graphs:
					return NodeCategories::Graphs;

				case EPrimaryContextGroup::Functions:
					return NodeCategories::Functions;

				case EPrimaryContextGroup::Conversions:
					return NodeCategories::Conversions;

				case EPrimaryContextGroup::Variables:
					return NodeCategories::Variables;

				case EPrimaryContextGroup::Common:
				default:
				{
					return FText::GetEmpty();
				}
			}
		}

		namespace SchemaPrivate
		{
			static const FText CategoryDelim = LOCTEXT("MetaSoundActionsCategoryDelim", "|");
			static const FText KeywordDelim = LOCTEXT("MetaSoundKeywordDelim", " ");

			static const FText InputDisplayNameFormat = LOCTEXT("DisplayNameAddInputFormat", "Get {0}");
			static const FText InputTooltipFormat = LOCTEXT("TooltipAddInputFormat", "Adds a getter for the input '{0}' to the graph.");

			static const FText OutputDisplayNameFormat = LOCTEXT("DisplayNameAddOutputFormat", "Set {0}");
			static const FText OutputTooltipFormat = LOCTEXT("TooltipAddOutputFormat", "Adds a setter for the output '{0}' to the graph.");

			static const FText VariableAccessorDisplayNameFormat = LOCTEXT("DisplayNameAddVariableAccessorFormat", "Get {0}");
			static const FText VariableAccessorTooltipFormat = LOCTEXT("TooltipAddVariableAccessorFormat", "Adds a getter for the variable '{0}' to the graph.");

			static const FText VariableDeferredAccessorDisplayNameFormat = LOCTEXT("DisplayNameAddVariableDeferredAccessorFormat", "Get Delayed {0}");
			static const FText VariableDeferredAccessorTooltipFormat = LOCTEXT("TooltipAddVariableDeferredAccessorFormat", "Adds a delayed getter for the variable '{0}' to the graph.");

			static const FText VariableMutatorDisplayNameFormat = LOCTEXT("DisplayNameAddVariableMutatorFormat", "Set {0}");
			static const FText VariableMutatorTooltipFormat = LOCTEXT("TooltipAddVariableMutatorFormat", "Adds a setter for the variable '{0}' to the graph.");

			bool DataTypeSupportsAssetTypes(const Metasound::Frontend::FDataTypeRegistryInfo& InRegistryInfo, const TArray<FAssetData>& InAssets)
			{
				if (InRegistryInfo.PreferredLiteralType != Metasound::ELiteralType::UObjectProxy)
				{
					return false;
				}

				const IMetasoundEditorModule& EditorModule = FModuleManager::GetModuleChecked<IMetasoundEditorModule>("MetaSoundEditor");
				return Algo::AnyOf(InAssets, [&EditorModule, &InRegistryInfo](const FAssetData& Asset)
				{
					if (InRegistryInfo.ProxyGeneratorClass)
					{
						if (const UClass* Class = Asset.GetClass())
						{
							if (EditorModule.IsExplicitProxyClass(*InRegistryInfo.ProxyGeneratorClass))
							{
								return Class == InRegistryInfo.ProxyGeneratorClass;
							}
							else
							{
								return Class->IsChildOf(InRegistryInfo.ProxyGeneratorClass);
							}
						}
					}

					return false;
				});
			}

			// Connects to first pin with the same DataType
			bool TryConnectNewNodeToMatchingDataTypePin(UEdGraphNode& NewGraphNode, UEdGraphPin* FromPin)
			{
				using namespace Metasound::Frontend;

				if (!FromPin)
				{
					return false;
				}

				if (FromPin->Direction == EGPD_Input)
				{
					FInputHandle InputHandle = FGraphBuilder::GetInputHandleFromPin(FromPin);
					for (UEdGraphPin* Pin : NewGraphNode.Pins)
					{
						if (Pin->Direction == EGPD_Output)
						{
							FOutputHandle OutputHandle = FGraphBuilder::GetOutputHandleFromPin(Pin);
							if (OutputHandle->IsValid() && 
								InputHandle->CanConnectTo(*OutputHandle).Connectable == FConnectability::EConnectable::Yes)
							{
								if (ensure(FGraphBuilder::ConnectNodes(*FromPin, *Pin, true /* bConnectEdPins */)))
								{
									return true;
								}
							}
						}
					}
				}

				if (FromPin->Direction == EGPD_Output)
				{
					FOutputHandle OutputHandle = FGraphBuilder::GetOutputHandleFromPin(FromPin);
					for (UEdGraphPin* Pin : NewGraphNode.Pins)
					{
						if (Pin->Direction == EGPD_Input)
						{
							FInputHandle InputHandle = FGraphBuilder::GetInputHandleFromPin(Pin);
							if (InputHandle->IsValid() && 
								InputHandle->CanConnectTo(*OutputHandle).Connectable == FConnectability::EConnectable::Yes)
							{
								if (ensure(FGraphBuilder::ConnectNodes(*Pin, *FromPin, true /* bConnectEdPins */)))
								{
									return true;
								}
							}
						}
					}
				}

				return false;
			}

			struct FDataTypeActionQuery
			{
				FGraphActionMenuBuilder& ActionMenuBuilder;
				const TArray<Frontend::FConstNodeHandle>& NodeHandles;
				FInterfaceNodeFilterFunction Filter;
				EPrimaryContextGroup ContextGroup;
				const FText& DisplayNameFormat;
				const FText& TooltipFormat;
				bool bShowSelectedActions = false;
			};

			template <typename TAction>
			void GetDataTypeActions(const FDataTypeActionQuery& InQuery)
			{
				using namespace Editor;
				using namespace Frontend;

				for (const FConstNodeHandle& NodeHandle : InQuery.NodeHandles)
				{
					if (!InQuery.Filter || InQuery.Filter(NodeHandle))
					{
						constexpr bool bIncludeNamespace = true;

						const FText& GroupName = GetContextGroupDisplayName(InQuery.ContextGroup);
						const FText NodeDisplayName = FGraphBuilder::GetDisplayName(*NodeHandle, bIncludeNamespace);
						const FText Tooltip = FText::Format(InQuery.TooltipFormat, NodeDisplayName);
						const FText DisplayName = FText::Format(InQuery.DisplayNameFormat, NodeDisplayName);
						TSharedPtr<TAction> NewNodeAction = MakeShared<TAction>(GroupName, DisplayName, NodeHandle->GetID(), Tooltip, InQuery.ContextGroup);
						InQuery.ActionMenuBuilder.AddAction(NewNodeAction);
					}
				}
			}

			void SelecteNodeInEditor(UMetasoundEditorGraph& InMetaSoundGraph, UMetasoundEditorGraphNode& InNode)
			{
				TSharedPtr<FEditor> MetasoundEditor = FGraphBuilder::GetEditorForGraph(InMetaSoundGraph);
				if (MetasoundEditor.IsValid())
				{
					MetasoundEditor->ClearSelectionAndSelectNode(&InNode);
				}
			}

			UEdGraphNode* PromoteToVariable(const FName BaseName, UEdGraphPin& FromPin, const FName DataType, const FMetasoundFrontendClass& InVariableClass, const FVector2D& InLocation, bool bSelectNode)
			{
				using namespace Frontend;

				UEdGraphNode* ConnectedNode = Cast<UEdGraphNode>(FromPin.GetOwningNode());
				if (!ensure(ConnectedNode))
				{
					return nullptr;
				}

				const FMetasoundFrontendClassName& ClassName = InVariableClass.Metadata.GetClassName();

				UMetasoundEditorGraph* MetaSoundGraph = CastChecked<UMetasoundEditorGraph>(ConnectedNode->GetGraph());
				const FName NodeName = FGraphBuilder::GenerateUniqueVariableName(MetaSoundGraph->GetGraphHandle(), BaseName.ToString());

				const FScopedTransaction Transaction(FText::Format(
					LOCTEXT("PromoteNodeVertexToGraphVariableFormat", "Promote MetaSound Node {0} to {1}"),
					FText::FromName(NodeName),
					FText::FromName(ClassName.Namespace)));

				UObject& ParentMetaSound = MetaSoundGraph->GetMetasoundChecked();
				ParentMetaSound.Modify();
				MetaSoundGraph->Modify();

				// Cache the default literal from the pin if connecting to an input
				FMetasoundFrontendLiteral DefaultValue;
				if (FromPin.Direction == EGPD_Input)
				{
					FGraphBuilder::GetPinLiteral(FromPin, DefaultValue);
				}

				FVariableHandle VariableHandle = FGraphBuilder::AddVariableHandle(ParentMetaSound, DataType);
				if (ensure(VariableHandle->IsValid()))
				{
					UMetasoundEditorGraphVariable* Variable = MetaSoundGraph->FindOrAddVariable(VariableHandle);
					if (ensure(Variable))
					{
						constexpr bool bPostSubTransaction = false;
						Variable->SetMemberName(NodeName, bPostSubTransaction);

						FNodeHandle NodeHandle = FGraphBuilder::AddVariableNodeHandle(ParentMetaSound, Variable->GetVariableID(), ClassName.ToNodeClassName());

						if (UMetasoundEditorGraphVariableNode* NewGraphNode = FGraphBuilder::AddVariableNode(ParentMetaSound, NodeHandle, InLocation))
						{
							// Set the literal using the new value if connecting to an input
							if (FromPin.Direction == EGPD_Input)
							{
								UMetasoundEditorGraphMemberDefaultLiteral* DefaultLiteral = Variable->GetLiteral();
								if (ensure(DefaultLiteral))
								{
									DefaultLiteral->SetFromLiteral(DefaultValue);
								}

								// Ensures the setter node value is synced with the editor literal value
								constexpr bool bPostTransaction = false;
								Variable->UpdateFrontendDefaultLiteral(bPostTransaction);
							}

							UEdGraphNode* EdGraphNode = CastChecked<UEdGraphNode>(NewGraphNode);
							if (ensure(SchemaPrivate::TryConnectNewNodeToMatchingDataTypePin(*EdGraphNode, &FromPin)))
							{
								FGraphBuilder::RegisterGraphWithFrontend(ParentMetaSound);
								SelecteNodeInEditor(*MetaSoundGraph, *NewGraphNode);
								return EdGraphNode;
							}
						}
					}
				}

				return nullptr;
			}

			bool WillAddingVariableAccessorCauseLoop(const Frontend::IVariableController& InVariable, const Frontend::IInputController& InInput)
			{
				using namespace Metasound::Frontend;

				// A variable mutator node must come before a variable accessor node,
				// or else the nodes will create a loop from the hidden variable pin. 
				// To determine if adding an accessor node will cause a loop (before actually
				// adding an accessor node), we check whether an existing mutator can
				// reach the node upstream which wants to connect to the accessor node.
				//
				// Example:
				// Will cause loop:
				// 	[AccessorNode]-->[DestinationNode]-->[Node]-->[MutatorNode] 
				// 	       ^-------------------------------------------|
				//
				// Will not cause loop
				//  [Node]-->[MutatorNode]-->[AccessorNode]-->[DestinationNode]
				//       |                                        ^ 
				//       |----------------------------------------|
				FConstNodeHandle MutatorNode = InVariable.FindMutatorNode();
				FConstNodeHandle DestinationNode = InInput.GetOwningNode();
				return FGraphLinter::IsReachableUpstream(*MutatorNode, *DestinationNode);
			}

			bool WillAddingVariableMutatorCauseLoop(const Frontend::IVariableController& InVariable, const Frontend::IOutputController& InOutput)
			{
				using namespace Metasound::Frontend;

				// A variable mutator node must come before a variable accessor node,
				// or else the nodes will create a loop from the hidden variable pin. 
				// To determine if adding a mutator node will cause a loop (before actually
				// adding a mutator node), we check whether any existing accessor can
				// reach the node downstream which wants to connect to the mutator node.
				//
				// Example:
				// Will cause loop:
				// 	[AccessorNode]-->[Node]-->[SourceNode]-->[MutatorNode] 
				// 	     ^---------------------------------------|
				//
				// Will not cause loop
				//  [SourceNode]-->[MutatorNode]-->[AccessorNode]-->[Node]
				//       |                                            ^ 
				//       |--------------------------------------------|
				TArray<FConstNodeHandle> AccessorNodes = InVariable.FindAccessorNodes();
				FConstNodeHandle SourceNode = InOutput.GetOwningNode();

				auto IsSourceNodeReachableDownstream = [&SourceNode](const FConstNodeHandle& AccessorNode)
				{
					return FGraphLinter::IsReachableDownstream(*AccessorNode, *SourceNode);
				};

				return Algo::AnyOf(AccessorNodes, IsSourceNodeReachableDownstream);
			}
		} // namespace SchemaPrivate
	} // namespace Editor
} // namespace Metasound

UEdGraphNode* FMetasoundGraphSchemaAction_NodeWithMultipleOutputs::PerformAction(class UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	using namespace Metasound::Editor;
	
	UEdGraphNode* ResultNode = NULL;

	if (FromPins.Num() > 0)
	{
		ResultNode = PerformAction(ParentGraph, FromPins[0], Location, bSelectNewNode);

		if (ResultNode)
		{
			// Try autowiring the rest of the pins
			for (int32 Index = 1; Index < FromPins.Num(); ++Index)
			{
				ResultNode->AutowireNewNode(FromPins[Index]);
			}
		}
	}
	else
	{
		ResultNode = PerformAction(ParentGraph, NULL, Location, bSelectNewNode);
	}

	TSharedPtr<FEditor> MetasoundEditor = FGraphBuilder::GetEditorForGraph(*ParentGraph);
	if (MetasoundEditor.IsValid() && bSelectNewNode)
	{
		MetasoundEditor->ClearSelectionAndSelectNode(ResultNode);
	}

	return ResultNode;
}

const FSlateBrush* FMetasoundGraphSchemaAction_NewNode::GetIconBrush() const
{
	using namespace Metasound::Frontend;

	const FNodeRegistryKey RegistryKey = NodeRegistryKey::CreateKey(ClassMetadata);
	const bool bIsClassNative = FMetasoundFrontendRegistryContainer::Get()->IsNodeNative(RegistryKey);
	if (bIsClassNative)
	{
		return &Metasound::Editor::Style::GetSlateBrushSafe("MetasoundEditor.Graph.Node.Class.Native");
	}

	return &Metasound::Editor::Style::GetSlateBrushSafe("MetasoundEditor.Graph.Node.Class.Graph");
}

const FLinearColor& FMetasoundGraphSchemaAction_NewNode::GetIconColor() const
{
	using namespace Metasound::Frontend;

	if (const UMetasoundEditorSettings* EditorSettings = GetDefault<UMetasoundEditorSettings>())
	{
		const FNodeRegistryKey RegistryKey = NodeRegistryKey::CreateKey(ClassMetadata);
		const bool bIsClassNative = FMetasoundFrontendRegistryContainer::Get()->IsNodeNative(RegistryKey);
		if (bIsClassNative)
		{
			return EditorSettings->NativeNodeTitleColor;
		}

		return EditorSettings->AssetReferenceNodeTitleColor;
	}

	return Super::GetIconColor();
}

UEdGraphNode* FMetasoundGraphSchemaAction_NewNode::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode /* = true */)
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	const FScopedTransaction Transaction(LOCTEXT("AddNewNode", "Add New MetaSound Node"));
	UMetasoundEditorGraph* MetaSoundGraph = CastChecked<UMetasoundEditorGraph>(ParentGraph);
	UObject& ParentMetasound = MetaSoundGraph->GetMetasoundChecked();
	ParentMetasound.Modify();
	ParentGraph->Modify();

	if (UMetasoundEditorGraphExternalNode* NewGraphNode = FGraphBuilder::AddExternalNode(ParentMetasound, ClassMetadata, Location, bSelectNewNode))
	{
		NewGraphNode->Modify();
		SchemaPrivate::TryConnectNewNodeToMatchingDataTypePin(*NewGraphNode, FromPin);
		SchemaPrivate::SelecteNodeInEditor(*MetaSoundGraph, *NewGraphNode);
		return NewGraphNode;
	}

	return nullptr;
}

FMetasoundGraphSchemaAction_NewInput::FMetasoundGraphSchemaAction_NewInput(FText InNodeCategory, FText InDisplayName, FGuid InNodeID, FText InToolTip, Metasound::Editor::EPrimaryContextGroup InGroup)
	: FMetasoundGraphSchemaAction_NodeWithMultipleOutputs(MoveTemp(InNodeCategory), MoveTemp(InDisplayName), MoveTemp(InToolTip), InGroup)
	, NodeID(InNodeID)
{
}

const FSlateBrush* FMetasoundGraphSchemaAction_NewInput::GetIconBrush() const
{
	return &Metasound::Editor::Style::GetSlateBrushSafe("MetasoundEditor.Graph.Node.Class.Input");
}

const FLinearColor& FMetasoundGraphSchemaAction_NewInput::GetIconColor() const
{
	if (const UMetasoundEditorSettings* EditorSettings = GetDefault<UMetasoundEditorSettings>())
	{
		return EditorSettings->InputNodeTitleColor;
	}

	return Super::GetIconColor();
}

UEdGraphNode* FMetasoundGraphSchemaAction_NewInput::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D InLocation, bool bSelectNewNode /* = true */)
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;


	UMetasoundEditorGraph* MetasoundGraph = CastChecked<UMetasoundEditorGraph>(ParentGraph);
	UObject& ParentMetasound = MetasoundGraph->GetMetasoundChecked();

	UMetasoundEditorGraphInput* Input = MetasoundGraph->FindInput(NodeID);
	if (!ensure(Input))
	{
		return nullptr;
	}

	FNodeHandle NodeHandle = Input->GetNodeHandle();
	if (!ensure(NodeHandle->IsValid()))
	{
		return nullptr;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddNewInputNode", "Add New MetaSound Input Node"));
	ParentMetasound.Modify();
	MetasoundGraph->Modify();
	Input->Modify();

	if (UMetasoundEditorGraphInputNode* NewGraphNode = FGraphBuilder::AddInputNode(ParentMetasound, NodeHandle, InLocation))
	{
		NewGraphNode->Modify();
		SchemaPrivate::TryConnectNewNodeToMatchingDataTypePin(*NewGraphNode, FromPin);
		FGraphBuilder::RegisterGraphWithFrontend(ParentMetasound);
		return NewGraphNode;
	}

	return nullptr;
}

FMetasoundGraphSchemaAction_PromoteToInput::FMetasoundGraphSchemaAction_PromoteToInput()
	: FMetasoundGraphSchemaAction_NodeWithMultipleOutputs(
		FText(),
		LOCTEXT("PromoteToInputName", "Promote To Graph Input"),
		LOCTEXT("PromoteToInputTooltip2", "Promotes node input to graph input"),
		Metasound::Editor::EPrimaryContextGroup::Common)
{
}

UEdGraphNode* FMetasoundGraphSchemaAction_PromoteToInput::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D InLocation, bool bSelectNewNode /* = true */)
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	FConstInputHandle InputHandle = FGraphBuilder::GetConstInputHandleFromPin(FromPin);
	if (!ensure(InputHandle->IsValid()))
	{
		return nullptr;
	}

	const FScopedTransaction Transaction(LOCTEXT("PromoteNodeInputToGraphInput", "Promote MetaSound Node Input to Graph Input"));
	UMetasoundEditorGraph* MetasoundGraph = CastChecked<UMetasoundEditorGraph>(ParentGraph);
	UObject& ParentMetasound = MetasoundGraph->GetMetasoundChecked();
	ParentMetasound.Modify();
	MetasoundGraph->Modify();

	FMetasoundFrontendLiteral DefaultValue;
	FGraphBuilder::GetPinLiteral(*FromPin, DefaultValue);

	const FName InputName = InputHandle->GetName();
	// The promoted input must have the same vertex access type in order for it to be connectible
	const FCreateNodeVertexParams VertexParams = { InputHandle->GetDataType(), InputHandle->GetVertexAccessType() };

	FNodeHandle NodeHandle = FGraphBuilder::AddInputNodeHandle(ParentMetasound, VertexParams, &DefaultValue, &InputName);
	if (ensure(NodeHandle->IsValid()))
	{
		UMetasoundEditorGraphInput* Input = MetasoundGraph->FindOrAddInput(NodeHandle);
		if (ensure(Input))
		{
			if (UMetasoundEditorGraphInputNode* NewGraphNode = FGraphBuilder::AddInputNode(ParentMetasound, NodeHandle, InLocation))
			{
				UEdGraphNode* EdGraphNode = CastChecked<UEdGraphNode>(NewGraphNode);

				if (ensure(SchemaPrivate::TryConnectNewNodeToMatchingDataTypePin(*EdGraphNode, FromPin)))
				{
					FGraphBuilder::RegisterGraphWithFrontend(ParentMetasound);
					SchemaPrivate::SelecteNodeInEditor(*MetasoundGraph, *NewGraphNode);
					return EdGraphNode;
				}
			}
		}
	}

	return nullptr;
}

FMetasoundGraphSchemaAction_PromoteToVariable_AccessorNode::FMetasoundGraphSchemaAction_PromoteToVariable_AccessorNode()
	: FMetasoundGraphSchemaAction_NodeWithMultipleOutputs(
		FText(),
		LOCTEXT("PromoteToVariableGetterName", "Promote To Graph Variable"),
		LOCTEXT("PromoteToInputTooltip3", "Promotes node input to graph variable and creates a getter node"),
		Metasound::Editor::EPrimaryContextGroup::Common)
{
}

UEdGraphNode* FMetasoundGraphSchemaAction_PromoteToVariable_AccessorNode::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D InLocation, bool bSelectNewNode /* = true */)
{
	using namespace Metasound;
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	FInputHandle InputHandle = FGraphBuilder::GetInputHandleFromPin(FromPin);
	if (!ensure(InputHandle->IsValid()))
	{
		return nullptr;
	}

	const FName NodeName = InputHandle->GetName();
	const FName DataType = InputHandle->GetDataType();
	FMetasoundFrontendClass VariableClass;
	if (ensure(IDataTypeRegistry::Get().GetFrontendVariableAccessorClass(DataType, VariableClass)))
	{
		return SchemaPrivate::PromoteToVariable(NodeName, *FromPin, DataType, VariableClass, InLocation, bSelectNewNode);
	}

	return nullptr;
}

FMetasoundGraphSchemaAction_PromoteToVariable_DeferredAccessorNode::FMetasoundGraphSchemaAction_PromoteToVariable_DeferredAccessorNode()
	: FMetasoundGraphSchemaAction_NodeWithMultipleOutputs(
		FText(),
		LOCTEXT("PromoteToVariableDeferredGetterName", "Promote To Graph Variable (Deferred)"),
		LOCTEXT("PromoteToInputTooltip1", "Promotes node input to graph variable and creates a deferred getter node"),
		Metasound::Editor::EPrimaryContextGroup::Common)
{
}

UEdGraphNode* FMetasoundGraphSchemaAction_PromoteToVariable_DeferredAccessorNode::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D InLocation, bool bSelectNewNode /* = true */)
{
	using namespace Metasound;
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	FInputHandle InputHandle = FGraphBuilder::GetInputHandleFromPin(FromPin);
	if (!ensure(InputHandle->IsValid()))
	{
		return nullptr;
	}

	const FName NodeName = InputHandle->GetName();
	const FName DataType = InputHandle->GetDataType();
	FMetasoundFrontendClass VariableClass;
	if (ensure(IDataTypeRegistry::Get().GetFrontendVariableDeferredAccessorClass(DataType, VariableClass)))
	{
		return SchemaPrivate::PromoteToVariable(NodeName, *FromPin, DataType, VariableClass, InLocation, bSelectNewNode);
	}

	return nullptr;
}

FMetasoundGraphSchemaAction_PromoteToVariable_MutatorNode::FMetasoundGraphSchemaAction_PromoteToVariable_MutatorNode()
	: FMetasoundGraphSchemaAction(
		FText(),
		LOCTEXT("PromoteToVariableSetterName", "Promote To Graph Variable"),
		LOCTEXT("PromoteToVariableSetterTooltip2", "Promotes node input to graph variable and creates a setter node"),
		Metasound::Editor::EPrimaryContextGroup::Common)
{
}

UEdGraphNode* FMetasoundGraphSchemaAction_PromoteToVariable_MutatorNode::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D InLocation, bool bSelectNewNode /* = true */)
{
	using namespace Metasound;
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	FOutputHandle OutputHandle = FGraphBuilder::GetOutputHandleFromPin(FromPin);
	if (!ensure(OutputHandle->IsValid()))
	{
		return nullptr;
	}

	const FName NodeName = OutputHandle->GetName();
	const FName DataType = OutputHandle->GetDataType();
	FMetasoundFrontendClass VariableClass;
	if (ensure(IDataTypeRegistry::Get().GetFrontendVariableMutatorClass(DataType, VariableClass)))
	{
		return SchemaPrivate::PromoteToVariable(NodeName, *FromPin, DataType, VariableClass, InLocation, bSelectNewNode);
	}

	return nullptr;
}

FMetasoundGraphSchemaAction_NewOutput::FMetasoundGraphSchemaAction_NewOutput(FText InNodeCategory, FText InDisplayName, FGuid InOutputNodeID, FText InToolTip, Metasound::Editor::EPrimaryContextGroup InGroup)
	: FMetasoundGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InDisplayName), MoveTemp(InToolTip), InGroup)
	, NodeID(InOutputNodeID)
{
}

const FSlateBrush* FMetasoundGraphSchemaAction_NewOutput::GetIconBrush() const
{
	return &Metasound::Editor::Style::GetSlateBrushSafe("MetasoundEditor.Graph.Node.Class.Output");
}

const FLinearColor& FMetasoundGraphSchemaAction_NewOutput::GetIconColor() const
{
	if (const UMetasoundEditorSettings* EditorSettings = GetDefault<UMetasoundEditorSettings>())
	{
		return EditorSettings->OutputNodeTitleColor;
	}

	return Super::GetIconColor();
}

UEdGraphNode* FMetasoundGraphSchemaAction_NewOutput::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode /* = true */)
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	UMetasoundEditorGraph* MetasoundGraph = CastChecked<UMetasoundEditorGraph>(ParentGraph);
	UObject& ParentMetasound = MetasoundGraph->GetMetasoundChecked();

	UMetasoundEditorGraphOutput* Output = MetasoundGraph->FindOutput(NodeID);
	if (!ensure(Output))
	{
		return nullptr;
	}

	FNodeHandle NodeHandle = Output->GetNodeHandle();
	if (!ensure(NodeHandle->IsValid()))
	{
		return nullptr;
	}

	const FScopedTransaction Transaction(LOCTEXT("AddNewOutputNode2", "Add New MetaSound Output Node"));
	ParentMetasound.Modify();
	ParentGraph->Modify();

	if (UMetasoundEditorGraphOutputNode* NewGraphNode = FGraphBuilder::AddOutputNode(ParentMetasound, NodeHandle, Location, bSelectNewNode))
	{
		SchemaPrivate::TryConnectNewNodeToMatchingDataTypePin(*NewGraphNode, FromPin);
		FGraphBuilder::RegisterGraphWithFrontend(ParentMetasound);
		return NewGraphNode;
	}

	return nullptr;
}

FMetasoundGraphSchemaAction_PromoteToOutput::FMetasoundGraphSchemaAction_PromoteToOutput()
	: FMetasoundGraphSchemaAction(
		FText(),
		LOCTEXT("PromoteToOutputName", "Promote To Graph Output"),
		LOCTEXT("PromoteToOutputTooltip", "Promotes node output to graph output"),
		Metasound::Editor::EPrimaryContextGroup::Common)
{
}

UEdGraphNode* FMetasoundGraphSchemaAction_PromoteToOutput::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D InLocation, bool bSelectNewNode /* = true */)
{
	using namespace Metasound;
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	FOutputHandle OutputHandle = FGraphBuilder::GetOutputHandleFromPin(FromPin);
	if (!ensure(OutputHandle->IsValid()))
	{
		return nullptr;
	}

	const FScopedTransaction Transaction(LOCTEXT("PromoteNodeOutputToGraphOutput", "Promote MetaSound Node Output to Graph Output"));
	UMetasoundEditorGraph* MetasoundGraph = CastChecked<UMetasoundEditorGraph>(ParentGraph);
	UObject& ParentMetasound = MetasoundGraph->GetMetasoundChecked();
	ParentMetasound.Modify();
	MetasoundGraph->Modify();

	const FString OutputName = OutputHandle->GetName().ToString();
	const FVertexName NewNodeName = FGraphBuilder::GenerateUniqueNameByClassType(ParentMetasound, EMetasoundFrontendClassType::Output, OutputName);
	const FCreateNodeVertexParams VertexParams = { OutputHandle->GetDataType(), OutputHandle->GetVertexAccessType() };

	FNodeHandle NodeHandle = FGraphBuilder::AddOutputNodeHandle(ParentMetasound, VertexParams, &NewNodeName);
	if (ensure(NodeHandle->IsValid()))
	{
		UMetasoundEditorGraphOutput* Output = MetasoundGraph->FindOrAddOutput(NodeHandle);
		if (ensure(Output))
		{
			if (UMetasoundEditorGraphOutputNode* NewGraphNode = FGraphBuilder::AddOutputNode(ParentMetasound, NodeHandle, InLocation))
			{ 
				UEdGraphNode* EdGraphNode = CastChecked<UEdGraphNode>(NewGraphNode);

				if (ensure(SchemaPrivate::TryConnectNewNodeToMatchingDataTypePin(*EdGraphNode, FromPin)))
				{
					FGraphBuilder::RegisterGraphWithFrontend(ParentMetasound);
					SchemaPrivate::SelecteNodeInEditor(*MetasoundGraph, *NewGraphNode);
					return EdGraphNode;
				}
			}
		}
	}

	return nullptr;
}

FMetasoundGraphSchemaAction_NewVariableNode::FMetasoundGraphSchemaAction_NewVariableNode(FText InNodeCategory, FText InDisplayName, FGuid InVariableID, FText InToolTip)
	: FMetasoundGraphSchemaAction_NodeWithMultipleOutputs(MoveTemp(InNodeCategory), MoveTemp(InDisplayName), MoveTemp(InToolTip), Metasound::Editor::EPrimaryContextGroup::Variables)
	, VariableID(InVariableID)
{
}

UEdGraphNode* FMetasoundGraphSchemaAction_NewVariableNode::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	if (UMetasoundEditorGraph* MetasoundGraph = CastChecked<UMetasoundEditorGraph>(ParentGraph))
	{
		if (UObject* ParentMetasound = MetasoundGraph->GetMetasound())
		{
			if (UMetasoundEditorGraphVariable* Variable = MetasoundGraph->FindVariable(VariableID))
			{
				const FScopedTransaction Transaction(LOCTEXT("AddNewVariableAccessorNode", "Add New MetaSound Variable Accessor Node"));
				ParentMetasound->Modify();
				MetasoundGraph->Modify();
				Variable->Modify();
				
				FNodeHandle FrontendNode = CreateFrontendVariableNode(MetasoundGraph->GetGraphHandle(), VariableID);
				if (ensure(FrontendNode->IsValid()))
				{
					if (UMetasoundEditorGraphVariableNode* NewGraphNode = FGraphBuilder::AddVariableNode(*ParentMetasound, FrontendNode, Location, bSelectNewNode))
					{
						NewGraphNode->Modify();
						SchemaPrivate::TryConnectNewNodeToMatchingDataTypePin(*NewGraphNode, FromPin);
						return NewGraphNode;
					}
				}
			}
		}
	}

	return nullptr;
}

const FSlateBrush* FMetasoundGraphSchemaAction_NewVariableNode::GetIconBrush() const
{
	return &Metasound::Editor::Style::GetSlateBrushSafe("MetasoundEditor.Graph.Node.Class.Variable");
}

const FLinearColor& FMetasoundGraphSchemaAction_NewVariableNode::GetIconColor() const
{
	if (const UMetasoundEditorSettings* EditorSettings = GetDefault<UMetasoundEditorSettings>())
	{
		return EditorSettings->VariableNodeTitleColor;
	}

	return Super::GetIconColor();
}

FMetasoundGraphSchemaAction_NewVariableAccessorNode::FMetasoundGraphSchemaAction_NewVariableAccessorNode(FText InNodeCategory, FText InDisplayName, FGuid InVariableID, FText InToolTip)
: FMetasoundGraphSchemaAction_NewVariableNode(MoveTemp(InNodeCategory), MoveTemp(InDisplayName), MoveTemp(InVariableID), MoveTemp(InToolTip))
{
}

Metasound::Frontend::FNodeHandle FMetasoundGraphSchemaAction_NewVariableAccessorNode::CreateFrontendVariableNode(const Metasound::Frontend::FGraphHandle& InFrontendGraph, const FGuid& InVariableID) const
{
	return InFrontendGraph->AddVariableAccessorNode(InVariableID);
}

FMetasoundGraphSchemaAction_NewVariableDeferredAccessorNode::FMetasoundGraphSchemaAction_NewVariableDeferredAccessorNode(FText InNodeCategory, FText InDisplayName, FGuid InVariableID, FText InToolTip)
: FMetasoundGraphSchemaAction_NewVariableNode(MoveTemp(InNodeCategory), MoveTemp(InDisplayName), MoveTemp(InVariableID), MoveTemp(InToolTip))
{
}

Metasound::Frontend::FNodeHandle FMetasoundGraphSchemaAction_NewVariableDeferredAccessorNode::CreateFrontendVariableNode(const Metasound::Frontend::FGraphHandle& InFrontendGraph, const FGuid& InVariableID) const
{
	return InFrontendGraph->AddVariableDeferredAccessorNode(InVariableID);
}

FMetasoundGraphSchemaAction_NewVariableMutatorNode::FMetasoundGraphSchemaAction_NewVariableMutatorNode(FText InNodeCategory, FText InDisplayName, FGuid InVariableID, FText InToolTip)
: FMetasoundGraphSchemaAction_NewVariableNode(MoveTemp(InNodeCategory), MoveTemp(InDisplayName), MoveTemp(InVariableID), MoveTemp(InToolTip))
{
}

Metasound::Frontend::FNodeHandle FMetasoundGraphSchemaAction_NewVariableMutatorNode::CreateFrontendVariableNode(const Metasound::Frontend::FGraphHandle& InFrontendGraph, const FGuid& InVariableID) const
{
	using namespace Metasound::Frontend;

	// Only one mutator node should exist per variable. Check to
	// make sure that one does not already exist for this variable.
	FConstVariableHandle Variable = InFrontendGraph->FindVariable(InVariableID);
	FConstNodeHandle MutatorNode = Variable->FindMutatorNode();
	if (ensure(!MutatorNode->IsValid()))
	{
		return InFrontendGraph->FindOrAddVariableMutatorNode(InVariableID);
	}
	return INodeController::GetInvalidHandle();
}

UEdGraphNode* FMetasoundGraphSchemaAction_NewFromSelected::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode /* = true*/)
{
	// TODO: Implement
	return nullptr;
}

FMetasoundGraphSchemaAction_NewReroute::FMetasoundGraphSchemaAction_NewReroute(const FLinearColor* InIconColor, bool bInShouldTransact /* = true */)
	: FMetasoundGraphSchemaAction(
		FText(),
		LOCTEXT("RerouteName", "Add Reroute Node..."),
		LOCTEXT("RerouteTooltip", "Reroute Node (reroutes wires)"),
		Metasound::Editor::EPrimaryContextGroup::Common)
	, IconColor(InIconColor ? *InIconColor : FLinearColor::White)
	, bShouldTransact(bInShouldTransact)
{
}

UEdGraphNode* FMetasoundGraphSchemaAction_NewReroute::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode /* = true*/)
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	const FName DataType = FGraphBuilder::GetPinDataType(FromPin);
	if (DataType.IsNone())
	{
		return nullptr;
	}

	const FScopedTransaction Transaction(FText::Format(LOCTEXT("AddNewRerouteNode", "Add {0} Reroute Node"), FText::FromName(DataType)));
	UMetasoundEditorGraph* MetaSoundGraph = CastChecked<UMetasoundEditorGraph>(ParentGraph);
	UObject& ParentMetasound = MetaSoundGraph->GetMetasoundChecked();
	ParentMetasound.Modify();
	ParentGraph->Modify();

	FMetasoundAssetBase* MetaSoundAsset = Metasound::IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&ParentMetasound);
	check(MetaSoundAsset);

	const FNodeRegistryKey& RerouteTemplateKey = FRerouteNodeTemplate::GetRegistryKey();
	FMetasoundFrontendNodeInterface NodeInterface = FRerouteNodeTemplate::CreateNodeInterfaceFromDataType(DataType);
	FNodeHandle NodeHandle = MetaSoundAsset->GetRootGraphHandle()->AddTemplateNode(RerouteTemplateKey, MoveTemp(NodeInterface));

	if (UMetasoundEditorGraphExternalNode* NewGraphNode = FGraphBuilder::AddExternalNode(ParentMetasound, NodeHandle, Location, bSelectNewNode))
	{
		NewGraphNode->Modify();
		SchemaPrivate::TryConnectNewNodeToMatchingDataTypePin(*NewGraphNode, FromPin);
		MetaSoundGraph->GetModifyContext().AddNodeIDsModified({ NewGraphNode->GetNodeID()});

		TSharedPtr<FEditor> ParentEditor = FGraphBuilder::GetEditorForMetasound(ParentMetasound);
		if (ParentEditor.IsValid() && bSelectNewNode)
		{
			ParentEditor->ClearSelectionAndSelectNode(NewGraphNode);
		}

		return NewGraphNode;
	}

	return nullptr;
}

const FSlateBrush* FMetasoundGraphSchemaAction_NewReroute::GetIconBrush() const
{
	return &Metasound::Editor::Style::GetSlateBrushSafe("MetasoundEditor.Graph.Node.Class.Reroute");
}

const FLinearColor& FMetasoundGraphSchemaAction_NewReroute::GetIconColor() const
{
	return IconColor;
}

UEdGraphNode* FMetasoundGraphSchemaAction_NewComment::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	using namespace Metasound::Editor;

	const FScopedTransaction Transaction(LOCTEXT("AddNewOutputNode1", "Add Comment to MetaSound Graph"));
	ParentGraph->Modify();

	UEdGraphNode_Comment* CommentTemplate = NewObject<UEdGraphNode_Comment>();

	FSlateRect Bounds;
	FVector2D SpawnLocation = Location;
	TSharedPtr<FEditor> MetasoundEditor = FGraphBuilder::GetEditorForGraph(*ParentGraph);

	if (MetasoundEditor.IsValid() && MetasoundEditor->GetBoundsForSelectedNodes(Bounds, 50.0f))
	{
		CommentTemplate->SetBounds(Bounds);
		SpawnLocation.X = CommentTemplate->NodePosX;
		SpawnLocation.Y = CommentTemplate->NodePosY;
	}

	return FEdGraphSchemaAction_NewNode::SpawnNodeFromTemplate<UEdGraphNode_Comment>(ParentGraph, CommentTemplate, SpawnLocation);
}

const FSlateBrush* FMetasoundGraphSchemaAction_NewComment::GetIconBrush() const
{
	// TODO: Implement (Find icon & rig up)
	return Super::GetIconBrush();
}

const FLinearColor& FMetasoundGraphSchemaAction_NewComment::GetIconColor() const
{
	// TODO: Implement (Set to white when icon found)
	return Super::GetIconColor();
}

UEdGraphNode* FMetasoundGraphSchemaAction_Paste::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	using namespace Metasound::Editor;

	TSharedPtr<FEditor> MetasoundEditor = FGraphBuilder::GetEditorForGraph(*ParentGraph);
	if (MetasoundEditor.IsValid())
	{
		MetasoundEditor->PasteNodes(&Location);
	}

	return nullptr;
}

UMetasoundEditorGraphSchema::UMetasoundEditorGraphSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UMetasoundEditorGraphSchema::ConnectionCausesLoop(const UEdGraphPin* InputPin, const UEdGraphPin* OutputPin) const
{
	using namespace Metasound::Editor;

	bool bCausesLoop = false;

	if ((nullptr != InputPin) && (nullptr != OutputPin))
	{
		UEdGraphNode* InputNode = InputPin->GetOwningNode();
		UEdGraphNode* OutputNode = OutputPin->GetOwningNode();

		// Sets bCausesLoop if the input node already has a path to the output node
		//
		FGraphBuilder::DepthFirstTraversal(InputNode, [&](UEdGraphNode* Node) -> TSet<UEdGraphNode*>
			{
				TSet<UEdGraphNode*> Children;

				if (OutputNode == Node)
				{
					// If the input node can already reach the output node, then this 
					// connection will cause a loop.
					bCausesLoop = true;
				}

				if (!bCausesLoop)
				{
					// Only produce children if no loop exists to avoid wasting unnecessary CPU
					if (nullptr != Node)
					{
						Node->ForEachNodeDirectlyConnectedToOutputs([&](UEdGraphNode* ChildNode) 
							{ 
								Children.Add(ChildNode);
							}
						);
					}
				}

				return Children;
			}
		);
	}
	
	return bCausesLoop;
}

void UMetasoundEditorGraphSchema::GetPaletteActions(FGraphActionMenuBuilder& ActionMenuBuilder) const
{
	GetCommentAction(ActionMenuBuilder);
	GetFunctionActions(ActionMenuBuilder);
}

void UMetasoundEditorGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	using namespace Metasound;
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	FActionClassFilters ClassFilters;
	FConstGraphHandle GraphHandle = IGraphController::GetInvalidHandle();
	EMetasoundFrontendVertexAccessType OutputAccessType = EMetasoundFrontendVertexAccessType::Unset;
	if (const UEdGraphPin* FromPin = ContextMenuBuilder.FromPin)
	{
		if (FromPin->Direction == EGPD_Input)
		{
			FConstInputHandle InputHandle = FGraphBuilder::GetConstInputHandleFromPin(ContextMenuBuilder.FromPin);
			OutputAccessType = InputHandle->GetVertexAccessType();

			ClassFilters.OutputFilterFunction = [InputHandle](const FMetasoundFrontendClassOutput& InOutput)
			{
				return InOutput.TypeName == InputHandle->GetDataType() && 
					FMetasoundFrontendClassVertex::CanConnectVertexAccessTypes(InOutput.AccessType, InputHandle->GetVertexAccessType());
			};

			// Show only input nodes as output nodes can only connected if FromPin is input
			GraphHandle = InputHandle->GetOwningNode()->GetOwningGraph();
			GetDataTypeInputNodeActions(ContextMenuBuilder, GraphHandle, [InputHandle](FConstNodeHandle NodeHandle)
			{
				bool bHasConnectableOutput = false;
				NodeHandle->IterateConstOutputs([&](FConstOutputHandle PotentialOutputHandle)
				{
					bHasConnectableOutput |= (InputHandle->CanConnectTo(*PotentialOutputHandle).Connectable == FConnectability::EConnectable::Yes);
				});
				return bHasConnectableOutput;
			});

			FGraphActionMenuBuilder& ActionMenuBuilder = static_cast<FGraphActionMenuBuilder&>(ContextMenuBuilder);
			ActionMenuBuilder.AddAction(MakeShared<FMetasoundGraphSchemaAction_PromoteToInput>());

			// Constructor outputs cannot be promoted to variables
			if (OutputAccessType != EMetasoundFrontendVertexAccessType::Value)
			{
				ActionMenuBuilder.AddAction(MakeShared<FMetasoundGraphSchemaAction_PromoteToVariable_AccessorNode>());
				ActionMenuBuilder.AddAction(MakeShared<FMetasoundGraphSchemaAction_PromoteToVariable_DeferredAccessorNode>());
			}

			const FLinearColor IconColor = GetPinTypeColor(FromPin->PinType);
			ActionMenuBuilder.AddAction(MakeShared<FMetasoundGraphSchemaAction_NewReroute>(&IconColor));
		}

		if (FromPin->Direction == EGPD_Output)
		{
			FConstOutputHandle OutputHandle = FGraphBuilder::GetConstOutputHandleFromPin(FromPin);
			ClassFilters.InputFilterFunction = [OutputHandle](const FMetasoundFrontendClassInput& InInput)
			{
				return InInput.TypeName == OutputHandle->GetDataType() &&
					FMetasoundFrontendClassVertex::CanConnectVertexAccessTypes(OutputHandle->GetVertexAccessType(), InInput.AccessType);
			};

			// Show only output nodes as input nodes can only connected if FromPin is output
			GraphHandle = OutputHandle->GetOwningNode()->GetOwningGraph();
			GetDataTypeOutputNodeActions(ContextMenuBuilder, GraphHandle, [OutputHandle](FConstNodeHandle NodeHandle)
			{
				bool bHasConnectableInput = false;
				NodeHandle->IterateConstInputs([&](FConstInputHandle PotentialInputHandle)
				{
					bHasConnectableInput |= (PotentialInputHandle->CanConnectTo(*OutputHandle).Connectable == FConnectability::EConnectable::Yes);
				});
				return bHasConnectableInput;
			});

			FGraphActionMenuBuilder& ActionMenuBuilder = static_cast<FGraphActionMenuBuilder&>(ContextMenuBuilder);
			ActionMenuBuilder.AddAction(MakeShared<FMetasoundGraphSchemaAction_PromoteToOutput>());
			ActionMenuBuilder.AddAction(MakeShared<FMetasoundGraphSchemaAction_PromoteToVariable_MutatorNode>());

			const FLinearColor IconColor = GetPinTypeColor(FromPin->PinType);
			ActionMenuBuilder.AddAction(MakeShared<FMetasoundGraphSchemaAction_NewReroute>(&IconColor));
		}
	}
	else
	{
		TSharedPtr<FEditor> MetasoundEditor = FGraphBuilder::GetEditorForGraph(*ContextMenuBuilder.CurrentGraph);
		if (MetasoundEditor.IsValid() && MetasoundEditor->CanPasteNodes())
		{
			TSharedPtr<FMetasoundGraphSchemaAction_Paste> NewAction = MakeShared<FMetasoundGraphSchemaAction_Paste>(FText::GetEmpty(), LOCTEXT("PasteHereAction", "Paste here"), FText::GetEmpty(), EPrimaryContextGroup::Common);
			ContextMenuBuilder.AddAction(NewAction);
		}

		GetCommentAction(ContextMenuBuilder, ContextMenuBuilder.CurrentGraph);
		if (UObject* Metasound = MetasoundEditor->GetMetasoundObject())
		{
			FMetasoundAssetBase* MetasoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(Metasound);
			check(MetasoundAsset);
			GraphHandle = MetasoundAsset->GetRootGraphHandle();

			GetDataTypeInputNodeActions(ContextMenuBuilder, GraphHandle);
			GetDataTypeOutputNodeActions(ContextMenuBuilder, GraphHandle);
		}
	}

	GetFunctionActions(ContextMenuBuilder, ClassFilters, true /* bShowSelectedActions */, GraphHandle);

	// Variable and conversion actions are always by reference so are incompatible with constructor outputs 
	if (OutputAccessType != EMetasoundFrontendVertexAccessType::Value)
	{
		GetVariableActions(ContextMenuBuilder, ClassFilters, true /* bShowSelectedActions */, GraphHandle);
		GetConversionActions(ContextMenuBuilder, ClassFilters);
	}
}

void UMetasoundEditorGraphSchema::GetContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	if (!Context->Pin && Context->Node && Context->Node->IsA<UMetasoundEditorGraphNode>())
	{
		FToolMenuSection& Section = Menu->AddSection("MetasoundGraphSchemaNodeActions", LOCTEXT("NodeActionsMenuHeader", "Node Actions"));
		Section.AddMenuEntry(FGenericCommands::Get().Delete);
		Section.AddMenuEntry(FGenericCommands::Get().Cut);
		Section.AddMenuEntry(FGenericCommands::Get().Copy);
		Section.AddMenuEntry(FGenericCommands::Get().Duplicate);
		Section.AddMenuEntry(FGenericCommands::Get().Rename);
		Section.AddMenuEntry(FGraphEditorCommands::Get().BreakNodeLinks);

		// Only display update ability if node is of type external
		// and node registry is reporting a major update is available.
		if (const UMetasoundEditorGraphExternalNode* ExternalNode = Cast<UMetasoundEditorGraphExternalNode>(Context->Node))
		{
			FMetasoundFrontendVersionNumber HighestVersion = ExternalNode->FindHighestVersionInRegistry();
			Metasound::Frontend::FConstNodeHandle NodeHandle = ExternalNode->GetConstNodeHandle();
			const FMetasoundFrontendClassMetadata& Metadata = NodeHandle->GetClassMetadata();
			const bool bHasNewVersion = HighestVersion.IsValid() && HighestVersion > Metadata.GetVersion();

			const FNodeRegistryKey RegistryKey = NodeRegistryKey::CreateKey(Metadata);
			const bool bIsClassNative = FMetasoundFrontendRegistryContainer::Get()->IsNodeNative(RegistryKey);

			if (bHasNewVersion || !bIsClassNative)
			{
				Section.AddMenuEntry(FEditorCommands::Get().UpdateNodeClass);
			}
		}
	}

	Super::GetContextMenuActions(Menu, Context);
}

void UMetasoundEditorGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	const int32 RootNodeHeightOffset = -58;

	// Create the result node
	FGraphNodeCreator<UMetasoundEditorGraphNode> NodeCreator(Graph);
	UMetasoundEditorGraphNode* ResultRootNode = NodeCreator.CreateNode();
	ResultRootNode->NodePosY = RootNodeHeightOffset;
	NodeCreator.Finalize();
	SetNodeMetaData(ResultRootNode, FNodeMetadata::DefaultGraphNode);
}

const FPinConnectionResponse UMetasoundEditorGraphSchema::CanCreateConnection(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const
{
	using namespace Metasound;

	// Make sure the pins are not on the same node
	if (PinA->GetOwningNode() == PinB->GetOwningNode())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionSameNode", "Both are on the same node"));
	}

	// Compare the directions
	const UEdGraphPin* InputPin = nullptr;
	const UEdGraphPin* OutputPin = nullptr;

	if (!CategorizePinsByDirection(PinA, PinB, InputPin, OutputPin))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionIncompatible", "Directions are not compatible"));
	}

	if (ConnectionCausesLoop(InputPin, OutputPin))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionLoop2", "Connection causes loop"));
	}

	if (InputPin->PinType.PinCategory != OutputPin->PinType.PinCategory)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionTypeIncorrect", "Connection pin types do not match"));
	}

	bool bConnectingNodesWithErrors = false;
	UEdGraphNode* InputNode = InputPin->GetOwningNode();
	if (ensure(InputNode))
	{
		if (InputNode->ErrorType == EMessageSeverity::Error)
		{
			bConnectingNodesWithErrors = true;
		}
	}
	UEdGraphNode* OutputNode = InputPin->GetOwningNode();
	if (ensure(OutputNode))
	{
		if (OutputNode->ErrorType == EMessageSeverity::Error)
		{
			bConnectingNodesWithErrors = true;
		}
	}

	Frontend::FInputHandle InputHandle = Editor::FGraphBuilder::GetInputHandleFromPin(InputPin);
	Frontend::FOutputHandle OutputHandle = Editor::FGraphBuilder::GetOutputHandleFromPin(OutputPin);

	const bool bInputValid = InputHandle->IsValid();
	const bool bOutputValid = OutputHandle->IsValid();
	if (bInputValid && bOutputValid)
	{
		// TODO: Implement YesWithConverterNode to provide conversion options
		Frontend::FConnectability Connectability = InputHandle->CanConnectTo(*OutputHandle);
		if (Connectability.Connectable != Frontend::FConnectability::EConnectable::Yes)
		{
			if ((Frontend::FConnectability::EReason::IncompatibleDataTypes == Connectability.Reason) || (Connectability.Connectable == Frontend::FConnectability::EConnectable::YesWithConverterNode))
			{
				const FName InputType = InputHandle->GetDataType();
				const FName OutputType = OutputHandle->GetDataType();
				return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, FText::Format(
					LOCTEXT("ConnectionTypeIncompatibleFormat", "Output pin of type '{0}' cannot be connected to input pin of type '{1}'"),
					FText::FromName(OutputType),
					FText::FromName(InputType)
				));
			}
			else if (Frontend::FConnectability::EReason::CausesLoop == Connectability.Reason)
			{
				return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionLoop1", "Connection causes loop"));
			}
			else if (Frontend::FConnectability::EReason::IncompatibleAccessTypes == Connectability.Reason)
			{
				return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionIncompatibleAccessTypes", "Cannot create connection between incompatible access types. Constructor input pins can only be connected to constructor output pins."));
			}
			else
			{
				return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionNotAllowed", "Output pin cannot be connected to input pin."));
			}
		}

		// Break existing connections on inputs only - multiple output connections are acceptable
		if (!InputPin->LinkedTo.IsEmpty())
		{
			ECanCreateConnectionResponse ReplyBreakOutputs;
			if (InputPin == PinA)
			{
				ReplyBreakOutputs = CONNECT_RESPONSE_BREAK_OTHERS_A;
			}
			else
			{
				ReplyBreakOutputs = CONNECT_RESPONSE_BREAK_OTHERS_B;
			}
			return FPinConnectionResponse(ReplyBreakOutputs, LOCTEXT("ConnectionReplace", "Replace existing connections"));
		}

		return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, TEXT(""));
	}
	else if (bConnectingNodesWithErrors)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionCannotContainErrorNode", "Cannot create new connections with node containing errors."));
	}
	else
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionInternalError", "Internal error. Metasound node vertex handle mismatch."));
	}
}

void UMetasoundEditorGraphSchema::OnPinConnectionDoubleCicked(UEdGraphPin* PinA, UEdGraphPin* PinB, const FVector2D& GraphPosition) const
{
	using namespace Metasound::Editor;

	if (!PinA || !PinB)
	{
		return;
	}

	//@TODO: This constant is duplicated from inside of SGraphNodeKnot
	const FVector2D NodeSpacerSize(42.0f, 24.0f);
	const FVector2D KnotTopLeft = GraphPosition - (NodeSpacerSize * 0.5f);

	UMetasoundEditorGraph* ParentGraph = Cast<UMetasoundEditorGraph>(PinA->GetOwningNode()->GetGraph());
	if (ParentGraph->IsEditable())
	{
		const FName DataType = FGraphBuilder::GetPinDataType(PinA);
		const FScopedTransaction Transaction(FText::Format(LOCTEXT("AddConnectNewRerouteNode", "Add & Connect {0} Reroute Node"), FText::FromName(DataType)));

		UMetasoundEditorGraph* MetaSoundGraph = CastChecked<UMetasoundEditorGraph>(ParentGraph);
		UObject& ParentMetasound = MetaSoundGraph->GetMetasoundChecked();
		ParentMetasound.Modify();
		ParentGraph->Modify();

		UEdGraphPin* OutputPin = PinA->Direction == EGPD_Output ? PinA : PinB;

		const FLinearColor* IconColor = nullptr;
		constexpr bool bShouldTransact = false;
		TSharedPtr<FMetasoundGraphSchemaAction_NewReroute> RerouteAction = MakeShared<FMetasoundGraphSchemaAction_NewReroute>(IconColor, bShouldTransact);

		UEdGraphNode& Node = *PinA->GetOwningNode();
		UEdGraphNode* NewNode = RerouteAction->PerformAction(Node.GetGraph(), OutputPin, GraphPosition, true);

		if (ensure(NewNode))
		{
			UEdGraphPin** RerouteOutputPtr = NewNode->Pins.FindByPredicate([](const UEdGraphPin* Candidate) { return Candidate->Direction == EGPD_Output; });

			if (ensure(*RerouteOutputPtr))
			{
				constexpr bool bShouldBreakSingleTransact = false;
				BreakSinglePinLink(PinA, PinB, bShouldBreakSingleTransact);

				UEdGraphPin* InputPin = PinA->Direction == EGPD_Input ? PinA : PinB;
				ensure(TryCreateConnection(InputPin, *RerouteOutputPtr));
			}
		}
	}
}

bool UMetasoundEditorGraphSchema::TryCreateConnection(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	using namespace Metasound::Frontend;

	if (!ensure(PinA && PinB))
	{
		return false;
	}

	UEdGraphPin* InputPin = nullptr;
	UEdGraphPin* OutputPin = nullptr;
	if (!CategorizePinsByDirection(PinA, PinB, InputPin, OutputPin))
	{
		return false;
	}

	if (!ensure(InputPin && OutputPin))
	{
		return false;
	}

	// TODO: Implement YesWithConverterNode with selected conversion option

	// Must mark Metasound object as modified to avoid desync issues ***before*** attempting to create a connection
	// so that transaction stack observes Frontend changes last if rolled back (i.e. undone).  UEdGraphSchema::TryCreateConnection
	// intrinsically marks the respective pin EdGraphNodes as modified.
	UEdGraphNode* PinANode = PinA->GetOwningNode();
	UMetasoundEditorGraph* Graph = CastChecked<UMetasoundEditorGraph>(PinANode->GetGraph());
	Graph->GetMetasoundChecked().Modify();

	// This call to parent takes care of marking respective nodes for modification.
	if (!UEdGraphSchema::TryCreateConnection(PinA, PinB))
	{
		return false;
	}

	if (!Metasound::Editor::FGraphBuilder::ConnectNodes(*InputPin, *OutputPin, false /* bConnectEdPins */))
	{
		return false;
	}

	Graph->GetModifyContext().SetDocumentModified();

	return true;
}

void UMetasoundEditorGraphSchema::TrySetDefaultObject(UEdGraphPin& Pin, UObject* NewDefaultObject, bool bInMarkAsModified) const
{
	using namespace Metasound::Editor;

	if (UMetasoundEditorGraphNode* Node = Cast<UMetasoundEditorGraphNode>(Pin.GetOwningNode()))
	{
		if (Node->GetPinDataTypeInfo(Pin).PreferredLiteralType == Metasound::ELiteralType::UObjectProxy)
		{
			TrySetDefaultValue(Pin, NewDefaultObject ? NewDefaultObject->GetPathName() : FString(), bInMarkAsModified);
			return;
		}
	}

	Super::TrySetDefaultObject(Pin, NewDefaultObject, bInMarkAsModified);
}

void UMetasoundEditorGraphSchema::TrySetDefaultValue(UEdGraphPin& Pin, const FString& InNewDefaultValue, bool bInMarkAsModified) const
{
	if (UMetasoundEditorGraphNode* Node = Cast<UMetasoundEditorGraphNode>(Pin.GetOwningNode()))
	{
		if (Node->GetPinDataTypeInfo(Pin).PreferredLiteralType == Metasound::ELiteralType::UObjectProxy)
		{
			FSoftObjectPath Path = InNewDefaultValue;
			const TSet<FString> DisallowedClassNames = Node->GetDisallowedPinClassNames(Pin);
			if (UObject* Object = Path.TryLoad())
			{
				if (UClass* Class = Object->GetClass())
				{
					if (DisallowedClassNames.Contains(Class->GetClassPathName().ToString()))
					{
						return;
					}
				}
			}
		}
	}

	return Super::TrySetDefaultValue(Pin, InNewDefaultValue, bInMarkAsModified);
}

bool UMetasoundEditorGraphSchema::SafeDeleteNodeFromGraph(UEdGraph* Graph, UEdGraphNode* InNodeToDelete) const
{
	using namespace Metasound::Editor;

	if (!InNodeToDelete || !Graph || InNodeToDelete->GetGraph() != Graph)
	{
		return false;
	}

	if (UObject* MetaSound = Graph->GetOutermostObject())
	{
		MetaSound->Modify();
	}
	Graph->Modify();

	return FGraphBuilder::DeleteNode(*InNodeToDelete);
}

bool UMetasoundEditorGraphSchema::ShouldHidePinDefaultValue(UEdGraphPin* Pin) const
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	if (!Pin)
	{
		return true;
	}

	if (Pin->Direction == EGPD_Input)
	{
		FInputHandle InputHandle = FGraphBuilder::GetInputHandleFromPin(Pin);
		return !InputHandle->GetOwningNode()->GetClassStyle().Display.bShowLiterals;
	}

	if (Pin->Direction == EGPD_Output)
	{
		FOutputHandle OutputHandle = FGraphBuilder::GetOutputHandleFromPin(Pin);
		return !OutputHandle->GetOwningNode()->GetClassStyle().Display.bShowLiterals;
	}

	// TODO: Determine if should be hidden from doc data
	return false;
}

FText UMetasoundEditorGraphSchema::GetPinDisplayName(const UEdGraphPin* Pin) const
{
	using namespace Metasound::Frontend;
	using namespace Metasound::Editor;

	check(Pin);

	UMetasoundEditorGraphNode* Node = CastChecked<UMetasoundEditorGraphNode>(Pin->GetOwningNode());
	FConstNodeHandle NodeHandle = Node->GetConstNodeHandle();
	const EMetasoundFrontendClassType ClassType = NodeHandle->GetClassMetadata().GetType();

	switch (ClassType)
	{
		case EMetasoundFrontendClassType::Input:
		case EMetasoundFrontendClassType::Output:
		case EMetasoundFrontendClassType::Variable:
		case EMetasoundFrontendClassType::VariableAccessor:
		case EMetasoundFrontendClassType::VariableDeferredAccessor:
		case EMetasoundFrontendClassType::VariableMutator:
		{
			UMetasoundEditorGraphMemberNode* MemberNode = Cast<UMetasoundEditorGraphMemberNode>(Node);
			if (ensure(MemberNode))
			{
				UMetasoundEditorGraphMember* Member = MemberNode->GetMember();
				if (ensure(Member))
				{
					return MemberNode->GetMember()->GetDisplayName();
				}
			}
			return Super::GetPinDisplayName(Pin);
		}

		case EMetasoundFrontendClassType::Literal:
		case EMetasoundFrontendClassType::External:
		case EMetasoundFrontendClassType::Template:
		{
			if (Pin->Direction == EGPD_Input)
			{
				FConstInputHandle InputHandle = NodeHandle->GetConstInputWithVertexName(Pin->GetFName());
				if (InputHandle->IsValid())
				{
					return FGraphBuilder::GetDisplayName(*InputHandle);
				}
			}
			else
			{
				FConstOutputHandle OutputHandle = NodeHandle->GetConstOutputWithVertexName(Pin->GetFName());
				if (OutputHandle->IsValid())
				{
					return FGraphBuilder::GetDisplayName(*OutputHandle);
				}
			}

			return Super::GetPinDisplayName(Pin);
		}

		case EMetasoundFrontendClassType::Graph:
		case EMetasoundFrontendClassType::Invalid:
		default:
		{
			static_assert(static_cast<int32>(EMetasoundFrontendClassType::Invalid) == 10, "Possible missing EMetasoundFrontendClassType case coverage");
			return Super::GetPinDisplayName(Pin);
		}
	}
}

FLinearColor UMetasoundEditorGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return Metasound::Editor::FGraphBuilder::GetPinCategoryColor(PinType);
}

void UMetasoundEditorGraphSchema::BreakNodeLinks(UEdGraphNode& TargetNode) const
{
	BreakNodeLinks(TargetNode, true /* bShouldActuallyTransact */);
}

void UMetasoundEditorGraphSchema::BreakNodeLinks(UEdGraphNode& TargetNode, bool bShouldActuallyTransact) const
{
	using namespace Metasound::Editor;

	const FScopedTransaction Transaction(LOCTEXT("BreakNodeLinks", "Break Node Links"), bShouldActuallyTransact);
	UMetasoundEditorGraph* Graph = CastChecked<UMetasoundEditorGraph>(TargetNode.GetGraph());
	Graph->GetMetasoundChecked().Modify();
	TargetNode.Modify();

	TArray<UEdGraphPin*> Pins = TargetNode.GetAllPins();
	for (UEdGraphPin* Pin : Pins)
	{
		FGraphBuilder::DisconnectPinVertex(*Pin);
		Super::BreakPinLinks(*Pin, false /* bSendsNodeNotifcation */);
	}
	Super::BreakNodeLinks(TargetNode);
}

void UMetasoundEditorGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	const FScopedTransaction Transaction(LOCTEXT("BreakPinLinks", "Break Pin Links"));
	UMetasoundEditorGraph* Graph = CastChecked<UMetasoundEditorGraph>(TargetPin.GetOwningNode()->GetGraph());
	Graph->GetMetasoundChecked().Modify();
	TargetPin.Modify();

	FGraphBuilder::DisconnectPinVertex(TargetPin);
	Super::BreakPinLinks(TargetPin, bSendsNodeNotifcation);
}

void UMetasoundEditorGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const
{
	BreakSinglePinLink(SourcePin, TargetPin, true);
}

void UMetasoundEditorGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin, bool bShouldTransact) const
{
	using namespace Metasound::Editor;

	UEdGraphPin* InputPin = nullptr;
	UEdGraphPin* OutputPin = nullptr;
	if (!SourcePin || !TargetPin || !SourcePin->LinkedTo.Contains(TargetPin) || !TargetPin->LinkedTo.Contains(SourcePin))
	{
		return;
	}

	if (SourcePin->Direction == EGPD_Input)
	{
		InputPin = SourcePin;
	}
	else if (TargetPin->Direction == EGPD_Input)
	{
		InputPin = TargetPin;
	}
	else
	{
		return;
	}

	UEdGraphNode* OwningNode = InputPin->GetOwningNode();
	if (!OwningNode)
	{
		return;
	}

	const FScopedTransaction Transaction(LOCTEXT("BreakSinglePinLink", "Break Single Pin Link"), bShouldTransact);
	UMetasoundEditorGraph* Graph = CastChecked<UMetasoundEditorGraph>(OwningNode->GetGraph());
	Graph->GetMetasoundChecked().Modify();
	SourcePin->Modify();
	TargetPin->Modify();

	FGraphBuilder::DisconnectPinVertex(*InputPin);
	Super::BreakSinglePinLink(SourcePin, TargetPin);
}

void UMetasoundEditorGraphSchema::GetAssetsGraphHoverMessage(const TArray<FAssetData>& Assets, const UEdGraph* HoverGraph, FString& OutTooltipText, bool& OutOkIcon) const
{
	using namespace Metasound;
	using namespace Metasound::Editor;

	OutOkIcon = true;

	if (!HoverGraph)
	{
		OutOkIcon = false;
		return;
	}

	OutTooltipText = TEXT("Add MetaSound reference to Graph.");

	IMetasoundEditorModule& EditorModule = FModuleManager::GetModuleChecked<IMetasoundEditorModule>("MetaSoundEditor");
	for (const FAssetData& Data : Assets)
	{
		if (!EditorModule.IsMetaSoundAssetClass(Data.GetClass()->GetClassPathName()))
		{
			OutOkIcon = false;
			OutTooltipText = TEXT("Asset(s) must all be MetaSounds.");
			break;
		}

		const UMetasoundEditorGraph* MetaSoundGraph = CastChecked<UMetasoundEditorGraph>(HoverGraph);
		const UObject& MetaSound = MetaSoundGraph->GetMetasoundChecked();

		const FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&MetaSound);
		check(MetaSoundAsset);

		if (UObject* DroppedObject = Data.GetAsset())
		{
			FMetasoundAssetBase* DroppedMetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(DroppedObject);
			if (!DroppedMetaSoundAsset)
			{
				OutOkIcon = false;
				OutTooltipText = TEXT("Asset is not a valid MetaSound.");
				break;
			}

			if (MetaSoundAsset->AddingReferenceCausesLoop(Data.GetSoftObjectPath()))
			{
				OutOkIcon = false;
				OutTooltipText = TEXT("Cannot add an asset that would create a reference loop.");
				break;
			}
		}
		else
		{
			OutOkIcon = false;
			OutTooltipText = TEXT("Asset not found.");
			break;
		}
	}
}

void UMetasoundEditorGraphSchema::GetAssetsPinHoverMessage(const TArray<FAssetData>& Assets, const UEdGraphPin* HoverPin, FString& OutTooltipText, bool& OutOkIcon) const
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	if (HoverPin && HoverPin->Direction == EGPD_Input)
	{
		if (const UEdGraphNode* Node = HoverPin->GetOwningNode())
		{
			if (const UMetasoundEditorGraphNode* MetaSoundNode = Cast<UMetasoundEditorGraphNode>(HoverPin->GetOwningNode()))
			{
				if (Assets.Num() == 1)
				{
					FDataTypeRegistryInfo RegistryInfo = MetaSoundNode->GetPinDataTypeInfo(*HoverPin);
					const bool bAssetTypesMatch = SchemaPrivate::DataTypeSupportsAssetTypes(RegistryInfo, Assets);
					if (bAssetTypesMatch)
					{
						OutTooltipText = FString::Format(TEXT("Set to '{0}'"), { *Assets[0].AssetName.ToString() });
						OutOkIcon = true;
						return;
					}

					OutTooltipText = FString::Format(TEXT("'{0}': Invalid Type"), { *Assets[0].AssetName.ToString() });
					OutOkIcon = false;
					return;
				}

				OutTooltipText = TEXT("Cannot drop multiple assets on single pin.");
				OutOkIcon = false;
				return;
			}

			OutTooltipText = FString::Format(TEXT("Node '{0}' does not support drag/drop"), { *Node->GetName() });
			OutOkIcon = false;
			return;
		}
	}

	OutTooltipText = FString();
	OutOkIcon = false;
}

void UMetasoundEditorGraphSchema::DroppedAssetsOnGraph(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraph* Graph) const
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	FScopedTransaction Transaction(LOCTEXT("DropMetaSoundOnGraph", "Drop MetaSound On Graph"));

	UMetasoundEditorGraph* MetaSoundGraph = CastChecked<UMetasoundEditorGraph>(Graph);
	bool bTransactionSucceeded = false;
	bool bModifiedObjects = false;
	UObject& MetaSound = MetaSoundGraph->GetMetasoundChecked();

	FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&MetaSound);
	check(MetaSoundAsset);

	for (const FAssetData& DroppedAsset : Assets)
	{
		if (UObject* DroppedObject = DroppedAsset.GetAsset())
		{
			FMetasoundAssetBase* DroppedMetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(DroppedObject);
			if (!DroppedMetaSoundAsset)
			{
				continue;
			}

			if (MetaSoundAsset->AddingReferenceCausesLoop(DroppedAsset.GetSoftObjectPath()))
			{
				continue;
			}

			if (!bModifiedObjects)
			{
				MetaSound.Modify();
				Graph->Modify();
				bModifiedObjects = true;
			}

			// This may not be necessary as dropping an asset on the graph may load it, thus triggering the registration from the MetaSoundAssetManager.
			const FNodeClassInfo ClassInfo = DroppedMetaSoundAsset->GetAssetClassInfo();
			const FNodeRegistryKey RegistryKey = NodeRegistryKey::CreateKey(ClassInfo);
			if (ensure(NodeRegistryKey::IsValid(RegistryKey)))
			{
				FMetaSoundAssetRegistrationOptions RegOptions;
				RegOptions.bForceReregister = false;
				DroppedMetaSoundAsset->RegisterGraphWithFrontend(RegOptions);
			}

			const FMetasoundFrontendClassName ClassName = DroppedMetaSoundAsset->GetAssetClassInfo().ClassName;

			FMetasoundFrontendClass Class;
			if (ensure(ISearchEngine::Get().FindClassWithHighestVersion(ClassName.ToNodeClassName(), Class)))
			{
				Metasound::Editor::FGraphBuilder::AddExternalNode(MetaSound, Class.Metadata, GraphPosition);
				bTransactionSucceeded = true;
			}
		}
	}

	if (bTransactionSucceeded)
	{
		// Reregister this asset after adding the dropped asset to update references
		FMetaSoundAssetRegistrationOptions RegOptions;
		RegOptions.bForceReregister = true;
		MetaSoundAsset->RegisterGraphWithFrontend(RegOptions);
	}
	else
	{
		Transaction.Cancel();
	}
}

void UMetasoundEditorGraphSchema::DroppedAssetsOnNode(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraphNode* Node) const
{
	// Still needed?
}

void UMetasoundEditorGraphSchema::DroppedAssetsOnPin(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraphPin* Pin) const
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	if (!Pin)
	{
		return;
	}

	if (UMetasoundEditorGraphNode* Node = Cast<UMetasoundEditorGraphNode>(Pin->GetOwningNode()))
	{
		if (Assets.Num() == 1)
		{
			FDataTypeRegistryInfo RegistryInfo = Node->GetPinDataTypeInfo(*Pin);
			const bool bAssetTypesMatch = SchemaPrivate::DataTypeSupportsAssetTypes(RegistryInfo, Assets);
			if (bAssetTypesMatch)
			{
				UObject* Object = Assets.Last().GetAsset();
				const FText TransactionText = FText::Format(LOCTEXT("ChangeDefaultObjectTransaction", "Set {0} to '{1}'"),
					Pin->GetDisplayName(),
					FText::FromName(Object->GetFName()));
				const FScopedTransaction Transaction(TransactionText);
				Node->Modify();

				constexpr bool bMarkAsModified = true;
				TrySetDefaultObject(*Pin, Object, bMarkAsModified);
			}
		}
	}
}

void UMetasoundEditorGraphSchema::GetConversionActions(FGraphActionMenuBuilder& ActionMenuBuilder, Metasound::Editor::FActionClassFilters InFilters, bool bShowSelectedActions) const
{
	using namespace Metasound;
	using namespace Metasound::Editor;

	const bool bIncludeAllVersions = static_cast<bool>(EnableAllVersionsMetaSoundNodeClassCreationCVar);
	const TArray<FMetasoundFrontendClass> FrontendClasses = Frontend::ISearchEngine::Get().FindAllClasses(bIncludeAllVersions);
	for (const FMetasoundFrontendClass& FrontendClass : FrontendClasses)
	{
		if (InFilters.InputFilterFunction && !FrontendClass.Interface.Inputs.ContainsByPredicate(InFilters.InputFilterFunction))
		{
			continue;
		}

		if (InFilters.OutputFilterFunction && !FrontendClass.Interface.Outputs.ContainsByPredicate(InFilters.OutputFilterFunction))
		{
			continue;
		}

		FMetasoundFrontendClassMetadata Metadata = FrontendClass.Metadata;
		const FText Tooltip = Metadata.GetAuthor().IsEmpty()
			? Metadata.GetDescription()
			: FText::Format(LOCTEXT("MetasoundTooltipAuthorFormat1", "{0}\nAuthor: {1}"), Metadata.GetDescription(), FText::FromString(Metadata.GetAuthor()));
		if (!Metadata.GetCategoryHierarchy().IsEmpty() && !Metadata.GetCategoryHierarchy()[0].CompareTo(NodeCategories::Conversions))
		{
			FText KeywordsText = FText::Join(SchemaPrivate::KeywordDelim, Metadata.GetKeywords());
			const FText CategoryText = FText::Join(SchemaPrivate::CategoryDelim, Metadata.GetCategoryHierarchy());

			constexpr bool bShowNamespace = false; // TODO: Make this an option to display
			TSharedPtr<FMetasoundGraphSchemaAction_NewNode> NewNodeAction = MakeShared<FMetasoundGraphSchemaAction_NewNode>
			(
				CategoryText,
				FGraphBuilder::GetDisplayName(Metadata, FName(), bShowNamespace),
				Tooltip,
				EPrimaryContextGroup::Conversions,
				KeywordsText
			);

			Metadata.SetType(EMetasoundFrontendClassType::External);
			NewNodeAction->ClassMetadata = MoveTemp(Metadata);
			ActionMenuBuilder.AddAction(NewNodeAction);
		}
	}
}

void UMetasoundEditorGraphSchema::GetDataTypeInputNodeActions(FGraphContextMenuBuilder& ActionMenuBuilder, Metasound::Frontend::FConstGraphHandle InGraphHandle, Metasound::Editor::FInterfaceNodeFilterFunction InFilter, bool bShowSelectedActions) const
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	TArray<FConstNodeHandle> Inputs = InGraphHandle->GetConstInputNodes();
	const SchemaPrivate::FDataTypeActionQuery ActionQuery
	{
		ActionMenuBuilder,
		Inputs,
		InFilter,
		EPrimaryContextGroup::Inputs,
		SchemaPrivate::InputDisplayNameFormat,
		SchemaPrivate::InputTooltipFormat,
		bShowSelectedActions
	};
	SchemaPrivate::GetDataTypeActions<FMetasoundGraphSchemaAction_NewInput>(ActionQuery);
}

void UMetasoundEditorGraphSchema::GetDataTypeOutputNodeActions(FGraphContextMenuBuilder& ActionMenuBuilder, Metasound::Frontend::FConstGraphHandle InGraphHandle, Metasound::Editor::FInterfaceNodeFilterFunction InFilter, bool bShowSelectedActions) const
{
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	TArray<FConstNodeHandle> Outputs = InGraphHandle->GetConstOutputNodes();

	// Prune and only add actions for outputs that are not already represented in the graph
	// (as there should only be one output reference node ever to avoid confusion with which
	// is handling active input)
	if (const UMetasoundEditorGraph* Graph = Cast<UMetasoundEditorGraph>(ActionMenuBuilder.CurrentGraph))
	{
		for (int32 i = Outputs.Num() - 1; i >= 0; --i)
		{
			if (UMetasoundEditorGraphOutput* Output = Graph->FindOutput(Outputs[i]->GetID()))
			{
				if (!Output->GetNodes().IsEmpty())
				{
					Outputs.RemoveAtSwap(i, 1, false /* bAllowShrinking */);
				}
			}
		}
	}

	const SchemaPrivate::FDataTypeActionQuery ActionQuery
	{
		ActionMenuBuilder,
		Outputs,
		InFilter,
		EPrimaryContextGroup::Outputs,
		SchemaPrivate::OutputDisplayNameFormat,
		SchemaPrivate::OutputTooltipFormat,
		bShowSelectedActions
	};
	SchemaPrivate::GetDataTypeActions<FMetasoundGraphSchemaAction_NewOutput>(ActionQuery);
}

void UMetasoundEditorGraphSchema::GetFunctionActions(FGraphActionMenuBuilder& ActionMenuBuilder, Metasound::Editor::FActionClassFilters InFilters, bool bShowSelectedActions, Metasound::Frontend::FConstGraphHandle InGraphHandle) const
{
	using namespace Metasound::Editor;
	using namespace Metasound::Editor::SchemaPrivate;
	using namespace Metasound::Frontend;

	const IMetaSoundAssetManager& AssetManager = IMetaSoundAssetManager::GetChecked();
	FMetasoundAssetBase* ParentAsset = nullptr;
	if (InGraphHandle->IsValid())
	{
		FMetasoundFrontendClassMetadata AssetMetadata = InGraphHandle->GetGraphMetadata();
		AssetMetadata.SetType(EMetasoundFrontendClassType::External);
		const FNodeRegistryKey RegistryKey = NodeRegistryKey::CreateKey(AssetMetadata);
		ParentAsset = AssetManager.TryLoadAssetFromKey(RegistryKey);
	}

	const bool bIncludeAllVersions = static_cast<bool>(EnableAllVersionsMetaSoundNodeClassCreationCVar);
	const TArray<FMetasoundFrontendClass> FrontendClasses = ISearchEngine::Get().FindAllClasses(bIncludeAllVersions);
	for (const FMetasoundFrontendClass& FrontendClass : FrontendClasses)
	{
		if (InFilters.InputFilterFunction && !FrontendClass.Interface.Inputs.ContainsByPredicate(InFilters.InputFilterFunction))
		{
			continue;
		}

		if (InFilters.OutputFilterFunction && !FrontendClass.Interface.Outputs.ContainsByPredicate(InFilters.OutputFilterFunction))
		{
			continue;
		}

		if (FrontendClass.Metadata.GetType() == EMetasoundFrontendClassType::Template)
		{
			continue;
		}

		if (FrontendClass.Metadata.GetType() != EMetasoundFrontendClassType::External)
		{
			continue;
		}

		const FSoftObjectPath* Path = nullptr;
		if (ParentAsset)
		{
			const FNodeRegistryKey RegistryKey = NodeRegistryKey::CreateKey(FrontendClass.Metadata);
			Path = AssetManager.FindObjectPathFromKey(RegistryKey);
			if (Path)
			{
				if (ParentAsset->AddingReferenceCausesLoop(*Path))
				{
					continue;
				}
			}
		}

		FMetasoundFrontendClassMetadata Metadata = FrontendClass.Metadata;
		const FText Tooltip = Metadata.GetAuthor().IsEmpty()
			? Metadata.GetDescription()
			: FText::Format(LOCTEXT("MetasoundTooltipAuthorFormat2", "{0}\nAuthor: {1}"), Metadata.GetDescription(), FText::FromString(Metadata.GetAuthor()));

		if (Metadata.GetCategoryHierarchy().IsEmpty() || Metadata.GetCategoryHierarchy()[0].CompareTo(Metasound::NodeCategories::Conversions))
		{
			const EPrimaryContextGroup ContextGroup = Path ? EPrimaryContextGroup::Graphs : EPrimaryContextGroup::Functions;
			TArray<FText> CategoryHierarchy { GetContextGroupDisplayName(ContextGroup) };
			CategoryHierarchy.Append(Metadata.GetCategoryHierarchy());
			const FText KeywordsText = FText::Join(KeywordDelim, Metadata.GetKeywords());
			const FText CategoryText = FText::Join(CategoryDelim, CategoryHierarchy);

			constexpr bool bShowNamespace = false; // TODO: Make this an option to display
			TSharedPtr<FMetasoundGraphSchemaAction_NewNode> NewNodeAction = MakeShared<FMetasoundGraphSchemaAction_NewNode>
			(
				CategoryText,
				FGraphBuilder::GetDisplayName(Metadata, FName(), bShowNamespace),
				Tooltip,
				ContextGroup,
				KeywordsText
			);

			Metadata.SetType(EMetasoundFrontendClassType::External);
			NewNodeAction->ClassMetadata = Metadata;
			ActionMenuBuilder.AddAction(NewNodeAction);
		}
	}
}

void UMetasoundEditorGraphSchema::GetVariableActions(FGraphActionMenuBuilder& ActionMenuBuilder, Metasound::Editor::FActionClassFilters InFilters, bool bShowSelectedActions, Metasound::Frontend::FConstGraphHandle InGraphHandle) const
{
	using namespace Metasound::Frontend;
	using namespace Metasound::Editor;
	using namespace Metasound::Editor::SchemaPrivate;

	TArray<FConstVariableHandle> Variables = InGraphHandle->GetVariables();

	bool bGetAccessor = true;
	bool bGetDeferredAccessor = true;
	bool bGetMutator = true;
	bool bFilterByDataType = false;
	bool bCheckForLoops = false;
	FName DataType;
	FConstInputHandle ConnectingInputHandle = IInputController::GetInvalidHandle();
	FConstOutputHandle ConnectingOutputHandle = IOutputController::GetInvalidHandle();

	// Determine which variable actions to create.
	if (const UEdGraphPin* FromPin = ActionMenuBuilder.FromPin)
	{
		bFilterByDataType = true;
		bCheckForLoops = true;

		if (FromPin->Direction == EGPD_Input)
		{
			bGetMutator = false;
			ConnectingInputHandle = FGraphBuilder::GetConstInputHandleFromPin(FromPin);
			DataType = ConnectingInputHandle->GetDataType();
		}
		else if (FromPin->Direction == EGPD_Output)
		{
			bGetAccessor = false;
			bGetDeferredAccessor = false;
			ConnectingOutputHandle = FGraphBuilder::GetConstOutputHandleFromPin(FromPin);
			DataType = ConnectingOutputHandle->GetDataType();
		}
	}

	// Filter variable by data type.
	if (bFilterByDataType && DataType.IsValid() && !DataType.IsNone())
	{
		Variables.RemoveAllSwap([&DataType](const FConstVariableHandle& Var) { return Var->GetDataType() != DataType; });
	}

	// Create actions for each variable.
	const FText& GroupName = GetContextGroupDisplayName(EPrimaryContextGroup::Variables);
	for (FConstVariableHandle& Variable : Variables)
	{
		const FText VariableDisplayName = FGraphBuilder::GetDisplayName(*Variable);
		const FGuid VariableID = Variable->GetID();

		if (bGetAccessor)
		{
			// Do not add the action if adding an accessor node would cause a loop.
			if (!(bCheckForLoops && WillAddingVariableAccessorCauseLoop(*Variable, *ConnectingInputHandle)))
			{
				FText ActionDisplayName = FText::Format(SchemaPrivate::VariableAccessorDisplayNameFormat, VariableDisplayName);
				FText ActionTooltip = FText::Format(SchemaPrivate::VariableAccessorTooltipFormat, VariableDisplayName);
				ActionMenuBuilder.AddAction(MakeShared<FMetasoundGraphSchemaAction_NewVariableAccessorNode>(GroupName, ActionDisplayName, VariableID, ActionTooltip));
			}
		}

		if (bGetDeferredAccessor)
		{
			FText ActionDisplayName = FText::Format(SchemaPrivate::VariableDeferredAccessorDisplayNameFormat, VariableDisplayName);
			FText ActionTooltip = FText::Format(SchemaPrivate::VariableDeferredAccessorTooltipFormat, VariableDisplayName);
			ActionMenuBuilder.AddAction(MakeShared<FMetasoundGraphSchemaAction_NewVariableDeferredAccessorNode>(GroupName, ActionDisplayName, VariableID, ActionTooltip));
		}

		if (bGetMutator)
		{
			// There can only be one mutator node per a variable. Only add the new
			// mutator node action if no mutator nodes exist.
			bool bMutatorNodeAlreadyExists = Variable->FindMutatorNode()->IsValid();
			if (!bMutatorNodeAlreadyExists)
			{
				// Do not add the action if adding a mutator node would cause a loop.
				if (!(bCheckForLoops && WillAddingVariableMutatorCauseLoop(*Variable, *ConnectingOutputHandle)))
				{
					FText ActionDisplayName = FText::Format(SchemaPrivate::VariableMutatorDisplayNameFormat, VariableDisplayName);
					FText ActionTooltip = FText::Format(SchemaPrivate::VariableMutatorTooltipFormat, VariableDisplayName);
					ActionMenuBuilder.AddAction(MakeShared<FMetasoundGraphSchemaAction_NewVariableMutatorNode>(GroupName, ActionDisplayName, VariableID, ActionTooltip));
				}
			}
		}
	}
}

void UMetasoundEditorGraphSchema::GetCommentAction(FGraphActionMenuBuilder& ActionMenuBuilder, const UEdGraph* CurrentGraph) const
{
	using namespace Metasound::Editor;

	if (!ActionMenuBuilder.FromPin && CurrentGraph)
	{
		TSharedPtr<FEditor> MetasoundEditor = FGraphBuilder::GetEditorForGraph(*CurrentGraph);
		if (MetasoundEditor.IsValid())
		{
			const int32 NumSelected = MetasoundEditor->GetNumNodesSelected();
			const FText MenuDescription = NumSelected > 0 ? LOCTEXT("CreateCommentAction", "Create Comment from Selection") : LOCTEXT("AddCommentAction", "Add Comment...");
			const FText ToolTip = LOCTEXT("CreateCommentToolTip", "Creates a comment.");

			TSharedPtr<FMetasoundGraphSchemaAction_NewComment> NewAction = MakeShared<FMetasoundGraphSchemaAction_NewComment>(FText::GetEmpty(), MenuDescription, ToolTip, EPrimaryContextGroup::Common);
			ActionMenuBuilder.AddAction(NewAction);
		}
	}
}

int32 UMetasoundEditorGraphSchema::GetNodeSelectionCount(const UEdGraph* Graph) const
{
	using namespace Metasound::Editor;

	TSharedPtr<FEditor> MetasoundEditor = FGraphBuilder::GetEditorForGraph(*Graph);
	if (MetasoundEditor.IsValid())
	{
		return MetasoundEditor->GetNumNodesSelected();
	}

	return 0;
}

TSharedPtr<FEdGraphSchemaAction> UMetasoundEditorGraphSchema::GetCreateCommentAction() const
{
	TSharedPtr<FMetasoundGraphSchemaAction_NewComment> Comment = MakeShared<FMetasoundGraphSchemaAction_NewComment>();
	return StaticCastSharedPtr<FEdGraphSchemaAction, FMetasoundGraphSchemaAction_NewComment>(Comment);
}

void UMetasoundEditorGraphSchema::SetNodePosition(UEdGraphNode* Node, const FVector2D& Position) const
{
	if (UMetasoundEditorGraphNode* MetasoundGraphNode = Cast<UMetasoundEditorGraphNode>(Node))
	{
		MetasoundGraphNode->GetMetasoundChecked().Modify();
		UEdGraphSchema::SetNodePosition(Node, Position);
		MetasoundGraphNode->UpdateFrontendNodeLocation(Position);
	}
	else
	{
		UEdGraphSchema::SetNodePosition(Node, Position);
	}
}
#undef LOCTEXT_NAMESPACE

