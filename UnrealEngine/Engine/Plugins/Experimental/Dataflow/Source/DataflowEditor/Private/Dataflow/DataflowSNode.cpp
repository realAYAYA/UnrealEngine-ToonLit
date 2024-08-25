// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowSNode.h"

#include "Dataflow/DataflowEdNode.h"
#include "Dataflow/DataflowEditorStyle.h"
#include "Dataflow/DataflowEngineUtil.h"
#include "Dataflow/DataflowGraphEditor.h"
#include "Dataflow/DataflowNodeFactory.h"
#include "Dataflow/DataflowObject.h"
#include "Logging/LogMacros.h"
#include "SourceCodeNavigation.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SCheckBox.h"
#include "Editor/Transactor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowSNode)

#define LOCTEXT_NAMESPACE "SDataflowEdNode"
//
// SDataflowEdNode
//

void SDataflowEdNode::Construct(const FArguments& InArgs, UDataflowEdNode* InNode)
{
	GraphNode = InNode;
	DataflowGraphNode = Cast<UDataflowEdNode>(InNode);
	UpdateGraphNode();

	
	const FSlateBrush* DisabledSwitchBrush = FDataflowEditorStyle::Get().GetBrush(TEXT("Dataflow.Render.Disabled"));
	const FSlateBrush* EnabledSwitchBrush = FDataflowEditorStyle::Get().GetBrush(TEXT("Dataflow.Render.Enabled"));

	//
	// Render
	//
	CheckBoxStyle = FCheckBoxStyle()
		.SetCheckBoxType(ESlateCheckBoxType::CheckBox)
		.SetUncheckedImage(*DisabledSwitchBrush)
		.SetUncheckedHoveredImage(*DisabledSwitchBrush)
		.SetUncheckedPressedImage(*DisabledSwitchBrush)
		.SetCheckedImage(*EnabledSwitchBrush)
		.SetCheckedHoveredImage(*EnabledSwitchBrush)
		.SetCheckedPressedImage(*EnabledSwitchBrush)
		.SetPadding(FMargin(0, 0, 0, 1));
	
	RenderCheckBoxWidget = SNew(SCheckBox)
		.Style(&CheckBoxStyle)
		.IsChecked_Lambda([this]()-> ECheckBoxState
			{
				if (DataflowGraphNode && DataflowGraphNode->DoAssetRender())
				{
					return ECheckBoxState::Checked;
				}
				return ECheckBoxState::Unchecked;
			})
		.OnCheckStateChanged_Lambda([&](const ECheckBoxState NewState)
			{
				if (DataflowGraphNode)
				{
					if (NewState == ECheckBoxState::Checked)
						DataflowGraphNode->SetAssetRender(true);
					else
						DataflowGraphNode->SetAssetRender(false);
				}
			});


	//
	//  Cached
	//
	/*
	const FSlateBrush* CachedFalseBrush = FDataflowEditorStyle::Get().GetBrush(TEXT("Dataflow.Cached.False"));
	const FSlateBrush* CachedTrueBrush = FDataflowEditorStyle::Get().GetBrush(TEXT("Dataflow.Cached.True"));

	CacheStatusStyle = FCheckBoxStyle()
		.SetCheckBoxType(ESlateCheckBoxType::CheckBox)
		.SetUncheckedImage(*CachedFalseBrush)
		.SetUncheckedHoveredImage(*CachedFalseBrush)
		.SetUncheckedPressedImage(*CachedFalseBrush)
		.SetCheckedImage(*CachedTrueBrush)
		.SetCheckedHoveredImage(*CachedTrueBrush)
		.SetCheckedPressedImage(*CachedTrueBrush)
		.SetPadding(FMargin(0, 0, 0, 1));

		CacheStatus = SNew(SCheckBox)
		.Style(&CacheStatusStyle)
		.IsChecked_Lambda([this]()-> ECheckBoxState
		{
			if (DataflowGraphNode)
			{
				return ECheckBoxState::Checked;
				// @todo(dataflow) : Missing connection to FToolkit
			}
			return ECheckBoxState::Unchecked;
		});
	*/
}

TArray<FOverlayWidgetInfo> SDataflowEdNode::GetOverlayWidgets(bool bSelected, const FVector2D& WidgetSize) const
{
	TArray<FOverlayWidgetInfo> Widgets = SGraphNode::GetOverlayWidgets(bSelected, WidgetSize);

	if (DataflowGraphNode && DataflowGraphNode->GetDataflowNode() && DataflowGraphNode->GetDataflowNode()->GetRenderParameters().Num())
	{
		const FVector2D ImageSize = RenderCheckBoxWidget->GetDesiredSize();

		FOverlayWidgetInfo Info;
		Info.OverlayOffset = FVector2D(WidgetSize.X - ImageSize.X - 6.f, 6.f);
		Info.Widget = RenderCheckBoxWidget;

		Widgets.Add(Info);
	}

	/*
	if (DataflowGraphNode )
	{
		// @todo(dataflow) : Need to bump the title box over 6px
		const FVector2D ImageSize = CacheStatus->GetDesiredSize();

		FOverlayWidgetInfo Info;
		Info.OverlayOffset = FVector2D(6.f, 6.f);
		Info.Widget = CacheStatus;

		Widgets.Add(Info);
	}
	*/
	return Widgets;
}


void SDataflowEdNode::UpdateErrorInfo()
{
	if (DataflowGraphNode)
	{
		if (const TSharedPtr<FDataflowNode> DataflowNode = DataflowGraphNode->GetDataflowNode())
		{
			if (DataflowNode->IsExperimental())
			{
				ErrorMsg = FString(TEXT("Experimental"));
				ErrorColor = FAppStyle::GetColor("ErrorReporting.WarningBackgroundColor");
			} 
			else if (DataflowNode->IsDeprecated())
			{
				ErrorMsg = FString(TEXT("Deprecated"));
				ErrorColor = FAppStyle::GetColor("ErrorReporting.WarningBackgroundColor");
			}
		}
	}
	
}


FReply SDataflowEdNode::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (GraphNode)
	{
		if (UDataflowEdNode* DataflowNode = Cast<UDataflowEdNode>(GraphNode))
		{
			if (TSharedPtr<Dataflow::FGraph> Graph = DataflowNode->GetDataflowGraph())
			{
				if (TSharedPtr<FDataflowNode> Node = Graph->FindBaseNode(DataflowNode->GetDataflowNodeGuid()))
				{
					if (FSourceCodeNavigation::CanNavigateToStruct(Node->TypedScriptStruct()))
					{
						FSourceCodeNavigation::NavigateToStruct(Node->TypedScriptStruct());
					}
				}
			}
		}
	}
	return Super::OnMouseButtonDoubleClick(InMyGeometry, InMouseEvent);
}

void SDataflowEdNode::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (DataflowGraphNode)
	{
		Collector.AddReferencedObject(DataflowGraphNode);
		if (TSharedPtr<FDataflowNode> DataflowNode = DataflowGraphNode->GetDataflowNode())
		{
			Collector.AddPropertyReferences(DataflowNode->TypedScriptStruct(), DataflowNode.Get());
		}
	}
}


//
// Add a menu option to create a graph node.
//
TSharedPtr<FAssetSchemaAction_Dataflow_CreateNode_DataflowEdNode> FAssetSchemaAction_Dataflow_CreateNode_DataflowEdNode::CreateAction(UEdGraph* ParentGraph, const FName & InNodeTypeName)
{
	if (Dataflow::FNodeFactory* Factory = Dataflow::FNodeFactory::GetInstance())
	{
		const Dataflow::FFactoryParameters& Param = Factory->GetParameters(InNodeTypeName);
		if (Param.IsValid())
		{
			const FText ToolTip = FText::FromString(Param.ToolTip.IsEmpty() ? FString("Add a Dataflow node.") : Param.ToolTip);
			const FText NodeName = FText::FromString(Param.DisplayName.ToString());
			const FText Category = FText::FromString(Param.Category.ToString().IsEmpty() ? FString("Dataflow") : Param.Category.ToString());
			const FText Tags = FText::FromString(Param.Tags);
			TSharedPtr<FAssetSchemaAction_Dataflow_CreateNode_DataflowEdNode> NewNodeAction(
				new FAssetSchemaAction_Dataflow_CreateNode_DataflowEdNode(InNodeTypeName, Category, NodeName, ToolTip, Tags));
			return NewNodeAction;
		}
	}
	return TSharedPtr<FAssetSchemaAction_Dataflow_CreateNode_DataflowEdNode>(nullptr);
}

static FName GetNodeUniqueName(UDataflow* Dataflow, FString NodeBaseName)
{
	FString Left, Right;
	int32 NameIndex = 1;

	// Check if NodeBaseName already ends with "_dd"
	if (NodeBaseName.Split(TEXT("_"), &Left, &Right, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
	{
		if (Right.IsNumeric())
		{
			NameIndex = FCString::Atoi(*Right);

			NodeBaseName = Left;
		}
	}

	FName NodeUniqueName{ NodeBaseName };
	while (Dataflow->GetDataflow()->FindBaseNode(FName(NodeUniqueName)) != nullptr)
	{
		NodeUniqueName = FName(NodeBaseName + FString::Printf(TEXT("_%02d"), NameIndex));
		NameIndex++;
	}

	return NodeUniqueName;
}

void SDataflowEdNode::CopyDataflowNodeSettings(TSharedPtr<FDataflowNode> SourceDataflowNode, TSharedPtr<FDataflowNode> TargetDataflowNode)
{
	using namespace UE::Transaction;
	FSerializedObject SerializationObject;

	FSerializedObjectDataWriter ArWriter(SerializationObject);
	SourceDataflowNode->SerializeInternal(ArWriter);

	FSerializedObjectDataReader ArReader(SerializationObject);
	TargetDataflowNode->SerializeInternal(ArReader);
}

static UDataflowEdNode* CreateNode(UDataflow* Dataflow, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode, const FName NodeUniqueName, const FName NodeTypeName, TSharedPtr<FDataflowNode> DataflowNodeToDuplicate, bool bCopySettings = false)
{
	if (Dataflow::FNodeFactory* Factory = Dataflow::FNodeFactory::GetInstance())
	{
		if (TSharedPtr<FDataflowNode> DataflowNode =
			Factory->NewNodeFromRegisteredType(
				*Dataflow->GetDataflow(),
				{ FGuid::NewGuid(), NodeTypeName, NodeUniqueName, Dataflow }))
		{
			if (UDataflowEdNode* EdNode = NewObject<UDataflowEdNode>(Dataflow, UDataflowEdNode::StaticClass(), NodeUniqueName))
			{
				Dataflow->Modify();
				if (FromPin != nullptr)
				{
					FromPin->Modify();
				}

				Dataflow->AddNode(EdNode, true, bSelectNewNode);

				// Copy properties from DataflowNodeToDuplicate to DataflowNode
				if (bCopySettings)
				{
					SDataflowEdNode::CopyDataflowNodeSettings(DataflowNodeToDuplicate, DataflowNode);
				}
				
				EdNode->CreateNewGuid();
				EdNode->PostPlacedNewNode();

				EdNode->SetDataflowGraph(Dataflow->GetDataflow());
				EdNode->SetDataflowNodeGuid(DataflowNode->GetGuid());
				EdNode->AllocateDefaultPins();

				EdNode->AutowireNewNode(FromPin);

				EdNode->NodePosX = Location.X;
				EdNode->NodePosY = Location.Y;

				EdNode->SetFlags(RF_Transactional);

				return EdNode;
			}
		}
	}

	return nullptr;
}

//
//  Created the EdGraph node and bind the guids to the Dataflow's node. 
//
UEdGraphNode* FAssetSchemaAction_Dataflow_CreateNode_DataflowEdNode::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	if (UDataflow* Dataflow = Cast<UDataflow>(ParentGraph))
	{
		// by default use the type name and check if it is unique in the context of the graph
		// if not, then generate a unique name 
		const FString NodeBaseName = GetMenuDescription().ToString();
		FName NodeUniqueName{ NodeBaseName };
		int32 NameIndex= 0;
		while (Dataflow->GetDataflow()->FindBaseNode(FName(NodeUniqueName)) != nullptr)
		{ 
			NodeUniqueName = FName(NodeBaseName + FString::Printf(TEXT("_%d"), NameIndex));
			NameIndex++;
		}

		return CreateNode(Dataflow, FromPin, Location, bSelectNewNode, NodeUniqueName, NodeTypeName, nullptr);
	}

	return nullptr;
}

//void FAssetSchemaAction_Dataflow_CreateNode_DataflowEdNode::AddReferencedObjects(FReferenceCollector& Collector)
//{
//	FEdGraphSchemaAction::AddReferencedObjects(Collector);
//	Collector.AddReferencedObject(NodeTemplate);
//}

//
// 
//
TSharedPtr<FAssetSchemaAction_Dataflow_DuplicateNode_DataflowEdNode> FAssetSchemaAction_Dataflow_DuplicateNode_DataflowEdNode::CreateAction(UEdGraph* ParentGraph, const FName& InNodeTypeName)
{
	if (Dataflow::FNodeFactory* Factory = Dataflow::FNodeFactory::GetInstance())
	{
		const Dataflow::FFactoryParameters& Param = Factory->GetParameters(InNodeTypeName);
		if (Param.IsValid())
		{
			const FText ToolTip = FText::FromString(Param.ToolTip.IsEmpty() ? FString("Add a Dataflow node.") : Param.ToolTip);
			const FText NodeName = FText::FromString(Param.DisplayName.ToString());
			const FText Category = FText::FromString(Param.Category.ToString().IsEmpty() ? FString("Dataflow") : Param.Category.ToString());
			const FText Tags = FText::FromString(Param.Tags);
			TSharedPtr<FAssetSchemaAction_Dataflow_DuplicateNode_DataflowEdNode> NewNodeAction(
				new FAssetSchemaAction_Dataflow_DuplicateNode_DataflowEdNode(InNodeTypeName, Category, NodeName, ToolTip, Tags));
			return NewNodeAction;
		}
	}
	return TSharedPtr<FAssetSchemaAction_Dataflow_DuplicateNode_DataflowEdNode>(nullptr);
}

//
//   
//
UEdGraphNode* FAssetSchemaAction_Dataflow_DuplicateNode_DataflowEdNode::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	if (UDataflow* Dataflow = Cast<UDataflow>(ParentGraph))
	{
		FString NodeToDuplicateName = DataflowNodeToDuplicate->GetName().ToString();

		// Check if that is unique, if not then make it unique with an index postfix
		FName NodeUniqueName = GetNodeUniqueName(Dataflow, NodeToDuplicateName);

		return CreateNode(Dataflow, FromPin, Location, bSelectNewNode, NodeUniqueName, NodeTypeName, DataflowNodeToDuplicate, /*bCopySettings=*/true);
	}

	return nullptr;
}

//
// 
//
TSharedPtr<FAssetSchemaAction_Dataflow_PasteNode_DataflowEdNode> FAssetSchemaAction_Dataflow_PasteNode_DataflowEdNode::CreateAction(UEdGraph* ParentGraph, const FName& InNodeTypeName)
{
	if (Dataflow::FNodeFactory* Factory = Dataflow::FNodeFactory::GetInstance())
	{
		const Dataflow::FFactoryParameters& Param = Factory->GetParameters(InNodeTypeName);
		if (Param.IsValid())
		{
			const FText ToolTip = FText::FromString(Param.ToolTip.IsEmpty() ? FString("Add a Dataflow node.") : Param.ToolTip);
			const FText NodeName = FText::FromString(Param.DisplayName.ToString());
			const FText Category = FText::FromString(Param.Category.ToString().IsEmpty() ? FString("Dataflow") : Param.Category.ToString());
			const FText Tags = FText::FromString(Param.Tags);
			TSharedPtr<FAssetSchemaAction_Dataflow_PasteNode_DataflowEdNode> NewNodeAction(
				new FAssetSchemaAction_Dataflow_PasteNode_DataflowEdNode(InNodeTypeName, Category, NodeName, ToolTip, Tags));
			return NewNodeAction;
		}
	}
	return TSharedPtr<FAssetSchemaAction_Dataflow_PasteNode_DataflowEdNode>(nullptr);
}

static UDataflowEdNode* CreateNodeFromPaste(UDataflow* Dataflow, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode, const FName NodeUniqueName, const FName NodeTypeName, FString NodeProperties)
{
	if (Dataflow::FNodeFactory* Factory = Dataflow::FNodeFactory::GetInstance())
	{
		if (TSharedPtr<FDataflowNode> DataflowNode =
			Factory->NewNodeFromRegisteredType(
				*Dataflow->GetDataflow(),
				{ FGuid::NewGuid(), NodeTypeName, NodeUniqueName, Dataflow }))
		{
			if (UDataflowEdNode* EdNode = NewObject<UDataflowEdNode>(Dataflow, UDataflowEdNode::StaticClass(), NodeUniqueName))
			{
				Dataflow->Modify();

				Dataflow->AddNode(EdNode, true, bSelectNewNode);

				// Copy properties to DataflowNode
				if (!NodeProperties.IsEmpty())
				{
					DataflowNode->TypedScriptStruct()->ImportText(*NodeProperties, DataflowNode.Get(), nullptr, EPropertyPortFlags::PPF_None, nullptr, DataflowNode->TypedScriptStruct()->GetName(), true);
				}

				EdNode->CreateNewGuid();
				EdNode->PostPlacedNewNode();

				EdNode->SetDataflowGraph(Dataflow->GetDataflow());
				EdNode->SetDataflowNodeGuid(DataflowNode->GetGuid());
				EdNode->AllocateDefaultPins();

				EdNode->NodePosX = Location.X;
				EdNode->NodePosY = Location.Y;

				EdNode->SetFlags(RF_Transactional);

				return EdNode;
			}
		}
	}

	return nullptr;
}

//
//   
//
UEdGraphNode* FAssetSchemaAction_Dataflow_PasteNode_DataflowEdNode::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	if (UDataflow* Dataflow = Cast<UDataflow>(ParentGraph))
	{
		FString NodeToDuplicateName = NodeName.ToString();

		// Check if that is unique, if not then make it unique with an index postfix
		FName NodeUniqueName = GetNodeUniqueName(Dataflow, NodeToDuplicateName);

		return CreateNodeFromPaste(Dataflow, FromPin, Location, bSelectNewNode, NodeUniqueName, NodeTypeName, NodeProperties);
	}

	return nullptr;
}
#undef LOCTEXT_NAMESPACE

