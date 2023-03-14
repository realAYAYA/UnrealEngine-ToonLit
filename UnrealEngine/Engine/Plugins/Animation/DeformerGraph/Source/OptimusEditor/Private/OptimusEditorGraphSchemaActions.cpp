// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusEditorGraphSchemaActions.h"

#include "OptimusComponentSource.h"
#include "OptimusEditorGraph.h"
#include "OptimusEditorGraphNode.h"

#include "OptimusNode.h"
#include "OptimusResourceDescription.h"
#include "OptimusVariableDescription.h"
#include "OptimusNodeGraph.h"

#include "EdGraph/EdGraphNode.h"


UEdGraphNode* FOptimusGraphSchemaAction_NewNode::PerformAction(
	UEdGraph* InParentGraph, 
	UEdGraphPin* InFromPin, 
	const FVector2D InLocation, 
	bool bInSelectNewNode /*= true*/
	)
{
	UOptimusEditorGraph* Graph = Cast<UOptimusEditorGraph>(InParentGraph);
	
	if (ensure(Graph != nullptr) && ensure(NodeClass != nullptr))
	{
		UOptimusNode* ModelNode = Graph->GetModelGraph()->AddNode(NodeClass, InLocation);

 		// FIXME: Automatic connection from the given pin.

		UOptimusEditorGraphNode* GraphNode = Graph->FindGraphNodeFromModelNode(ModelNode);
		if (GraphNode && bInSelectNewNode)
		{
			Graph->SelectNodeSet({GraphNode});
		}
		return GraphNode;
	}

	return nullptr;
}


UEdGraphNode* FOptimusGraphSchemaAction_NewConstantValueNode::PerformAction(
	UEdGraph* InParentGraph,
	UEdGraphPin* InFromPin,
	const FVector2D InLocation,
	bool bInSelectNewNode
	)
{
	UOptimusEditorGraph* Graph = Cast<UOptimusEditorGraph>(InParentGraph);
	
	if (ensure(Graph != nullptr) && ensure(DataType.IsValid()))
	{
		UOptimusNode* ModelNode = Graph->GetModelGraph()->AddValueNode(DataType, InLocation);

		// FIXME: Automatic connection from the given pin.

		UOptimusEditorGraphNode* GraphNode = Graph->FindGraphNodeFromModelNode(ModelNode);
		if (GraphNode && bInSelectNewNode)
		{
			Graph->SelectNodeSet({GraphNode});
		}
		return GraphNode;
	}

	return nullptr;
}


UEdGraphNode* FOptimusGraphSchemaAction_NewDataInterfaceNode::PerformAction(
	UEdGraph* InParentGraph,
	UEdGraphPin* InFromPin,
	const FVector2D InLocation,
	bool bInSelectNewNode
	)
{
	UOptimusEditorGraph* Graph = Cast<UOptimusEditorGraph>(InParentGraph);
	
	if (ensure(Graph != nullptr) && ensure(DataInterfaceClass != nullptr))
	{
		UOptimusNode* ModelNode = Graph->GetModelGraph()->AddDataInterfaceNode(DataInterfaceClass, InLocation);

		// FIXME: Automatic connection from the given pin.

		UOptimusEditorGraphNode* GraphNode = Graph->FindGraphNodeFromModelNode(ModelNode);
		if (GraphNode && bInSelectNewNode)
		{
			Graph->SelectNodeSet({GraphNode});
		}
		return GraphNode;
	}

	return nullptr;
}


static FText GetGraphTooltip(UOptimusNodeGraph* InGraph)
{
	return FText::GetEmpty();
}


FOptimusSchemaAction_Graph::FOptimusSchemaAction_Graph(
	UOptimusNodeGraph* InGraph,
	int32 InGrouping,
	const FText& InCategory) : 
		FEdGraphSchemaAction(
			InCategory, 
			FText::FromString(InGraph->GetName()), 
			GetGraphTooltip(InGraph), 
			InGrouping, 
			FText(), 
			int32(EOptimusSchemaItemGroup::Graphs) 
		), 
		GraphType(InGraph->GetGraphType())
{
	GraphPath = InGraph->GetGraphPath();
}


FOptimusSchemaAction_Binding::FOptimusSchemaAction_Binding(
	UOptimusComponentSourceBinding* InBinding,
	int32 InGrouping
	) :
	FEdGraphSchemaAction(
			FText::GetEmpty(),
			FText::FromString(InBinding->GetName()),
			FText::GetEmpty(),
			InGrouping,
			FText(),
			int32(EOptimusSchemaItemGroup::Bindings)
		)

{
	BindingName = InBinding->GetFName();
}


FOptimusSchemaAction_Resource::FOptimusSchemaAction_Resource(
	UOptimusResourceDescription* InResource, 
	int32 InGrouping ) :
	FEdGraphSchemaAction(
			FText::GetEmpty(),
			FText::FromString(InResource->GetName()),
			FText::GetEmpty(),
			InGrouping,
			FText(),
			int32(EOptimusSchemaItemGroup::Resources)
		)
{
	ResourceName = InResource->GetFName();
}


FOptimusSchemaAction_Variable::FOptimusSchemaAction_Variable(
	UOptimusVariableDescription* InVariable, 
	int32 InGrouping ) : 
	FEdGraphSchemaAction(
          FText::GetEmpty(),
          FText::FromString(InVariable->GetName()),
          FText::GetEmpty(),
          InGrouping,
          FText(),
          int32(EOptimusSchemaItemGroup::Variables))
{
	VariableName = InVariable->GetFName();
}
