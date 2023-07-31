// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowSNode.h"

#include "Dataflow/DataflowEdNode.h"
#include "Dataflow/DataflowNodeFactory.h"
#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowCore.h"
#include "Logging/LogMacros.h"
#include "Widgets/Layout/SBorder.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowSNode)

#define LOCTEXT_NAMESPACE "SDataflowEdNode"
//
// SDataflowEdNode
//

void SDataflowEdNode::Construct(const FArguments& InArgs, UDataflowEdNode* InNode)
{
	GraphNode = InNode;
	UpdateGraphNode();
}

FReply SDataflowEdNode::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	return Super::OnMouseButtonDoubleClick(InMyGeometry, InMouseEvent);
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
		const FName NodeName = MakeUniqueObjectName(Dataflow, UDataflowEdNode::StaticClass(), FName(GetMenuDescription().ToString()));
		if (Dataflow::FNodeFactory* Factory = Dataflow::FNodeFactory::GetInstance())
		{
			if (TSharedPtr<FDataflowNode> DataflowNode = Factory->NewNodeFromRegisteredType(*Dataflow->GetDataflow(), { FGuid::NewGuid(),NodeTypeName,NodeName}))
			{
				if (UDataflowEdNode* EdNode = NewObject<UDataflowEdNode>(Dataflow, UDataflowEdNode::StaticClass(), NodeName))
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

