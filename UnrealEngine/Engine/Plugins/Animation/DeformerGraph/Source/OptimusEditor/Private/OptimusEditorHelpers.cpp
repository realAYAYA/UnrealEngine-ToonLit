// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusEditorHelpers.h"
#include "EdGraph/EdGraphPin.h"
#include "OptimusEditorGraphNode.h"

#include "OptimusNode.h"
#include "OptimusNodePin.h"

UOptimusNode* OptimusEditor::GetModelNodeFromGraphPin(const UEdGraphPin* InGraphPin)
{
	UOptimusEditorGraphNode* GraphNode = Cast<UOptimusEditorGraphNode>(InGraphPin->GetOwningNode());

	if (ensure(GraphNode != nullptr) && ensure(GraphNode->ModelNode != nullptr))
	{
		return GraphNode->ModelNode;
	}

	return nullptr;
}

UOptimusNodePin* OptimusEditor::GetModelPinFromGraphPin(const UEdGraphPin* InGraphPin)
{
	const UOptimusNode* ModelNode = GetModelNodeFromGraphPin(InGraphPin);
	
	if (ensure(ModelNode))
	{
		return ModelNode->FindPin(InGraphPin->GetName());
	}

	return nullptr;
}

FName OptimusEditor::GetAdderPinName(EEdGraphPinDirection InDirection)
{
	return InDirection == EGPD_Input ? TEXT("_AdderPinInput") : TEXT("_AdderPinOutput");
};
	
FText OptimusEditor::GetAdderPinFriendlyName(EEdGraphPinDirection InDirection)
{
	return InDirection == EGPD_Input ? FText::FromString(TEXT("New Input")) : FText::FromString(TEXT("New Output"));
};

FName OptimusEditor::GetAdderPinCategoryName()
{
	return TEXT("OptimusAdderPin");
}

bool OptimusEditor::IsAdderPin(const UEdGraphPin* InGraphPin)
{
	if (InGraphPin->PinType.PinCategory == GetAdderPinCategoryName())
	{
		return true;
	}
		
	return false;
};

bool OptimusEditor::IsAdderPinType(const FEdGraphPinType& InPinType)
{
	if (InPinType.PinCategory == GetAdderPinCategoryName())
	{
		return true;
	}
	return false;
}


