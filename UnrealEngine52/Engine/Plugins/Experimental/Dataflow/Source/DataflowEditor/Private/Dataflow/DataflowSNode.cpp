// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowSNode.h"

#include "Dataflow/DataflowEdNode.h"
#include "Dataflow/DataflowEditorStyle.h"
#include "Dataflow/DataflowEngineUtil.h"
#include "Dataflow/DataflowGraphEditor.h"
#include "Dataflow/DataflowNodeFactory.h"
#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowCore.h"
#include "Logging/LogMacros.h"
#include "SourceCodeNavigation.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SCheckBox.h"

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
			const FText Catagory = FText::FromString(Param.Category.ToString().IsEmpty() ? FString("Dataflow") : Param.Category.ToString());
			const FText Tags = FText::FromString(Param.Tags);
			TSharedPtr<FAssetSchemaAction_Dataflow_CreateNode_DataflowEdNode> NewNodeAction(
				new FAssetSchemaAction_Dataflow_CreateNode_DataflowEdNode(InNodeTypeName, Catagory, NodeName, ToolTip, Tags));
			return NewNodeAction;
		}
	}
	return TSharedPtr<FAssetSchemaAction_Dataflow_CreateNode_DataflowEdNode>(nullptr);
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

		if (Dataflow::FNodeFactory* Factory = Dataflow::FNodeFactory::GetInstance())
		{
			if (TSharedPtr<FDataflowNode> DataflowNode = Factory->NewNodeFromRegisteredType(*Dataflow->GetDataflow(), { FGuid::NewGuid(), NodeTypeName, NodeUniqueName }))
			{
				if (UDataflowEdNode* EdNode = NewObject<UDataflowEdNode>(Dataflow, UDataflowEdNode::StaticClass(), NodeUniqueName))
				{
					Dataflow->Modify();
					if (FromPin != nullptr)
						FromPin->Modify();
		
					Dataflow->AddNode(EdNode, true, bSelectNewNode);
		
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
	}
	return nullptr;
}

//void FAssetSchemaAction_Dataflow_CreateNode_DataflowEdNode::AddReferencedObjects(FReferenceCollector& Collector)
//{
//	FEdGraphSchemaAction::AddReferencedObjects(Collector);
//	Collector.AddReferencedObject(NodeTemplate);
//}




#undef LOCTEXT_NAMESPACE

