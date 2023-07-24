// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusEditorGraphSchema.h"

#include "OptimusEditorHelpers.h"
#include "OptimusEditorGraph.h"
#include "OptimusEditorGraphSchemaActions.h"
#include "OptimusEditorGraphNode.h"

#include "OptimusDataTypeRegistry.h"
#include "OptimusNode.h"
#include "OptimusNodeGraph.h"
#include "OptimusNodePin.h"
#include "IOptimusNodeAdderPinProvider.h"

#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "OptimusComputeDataInterface.h"
#include "OptimusEditorGraphConnectionDrawingPolicy.h"
#include "Styling/SlateIconFinder.h"

#define LOCTEXT_NAMESPACE "OptimusEditor"

static bool IsValidAdderPinConnection(const UEdGraphPin* InPinA, const UEdGraphPin* InPinB, 
	FString* OutReason = nullptr)
{
	const bool bIsPinAAdderPin = OptimusEditor::IsAdderPin(InPinA);
	const bool bIsPinBAdderPin = OptimusEditor::IsAdderPin(InPinB);

	if (bIsPinAAdderPin && bIsPinBAdderPin)
	{
		if (OutReason)
		{
			*OutReason = TEXT("Can't connect adder pin to adder pin");
		}
		return false;
	}

	// The pins should be in the correct order now.
	UOptimusNodePin* ModelPinA = OptimusEditor::GetModelPinFromGraphPin(InPinA);
	UOptimusNodePin* ModelPinB = OptimusEditor::GetModelPinFromGraphPin(InPinB);
	UOptimusNode* ModelNodeA = OptimusEditor::GetModelNodeFromGraphPin(InPinA);
	UOptimusNode* ModelNodeB = OptimusEditor::GetModelNodeFromGraphPin(InPinB);

	// Default to Adder Pin on the PinB side
	EOptimusNodePinDirection NewPinDirection =
		InPinB->Direction == EGPD_Input?
			EOptimusNodePinDirection::Input : EOptimusNodePinDirection::Output;
	UOptimusNode* TargetNode = ModelNodeB;
	UOptimusNodePin* SourcePin = ModelPinA;

	// Flip Source/Target if the Adder Pin is on the PinA side
	if (bIsPinAAdderPin)
	{
		NewPinDirection =
			InPinA->Direction == EGPD_Input?
				EOptimusNodePinDirection::Input : EOptimusNodePinDirection::Output;
		TargetNode = ModelNodeA;
		SourcePin = ModelPinB;
	}

	const IOptimusNodeAdderPinProvider *AdderPinProvider = Cast<IOptimusNodeAdderPinProvider>(TargetNode);
	check(AdderPinProvider);

	const bool bCanConnect = AdderPinProvider->CanAddPinFromPin(SourcePin, NewPinDirection, OutReason);
	
	return bCanConnect;
}

UOptimusEditorGraphSchema::UOptimusEditorGraphSchema()
{

}


void UOptimusEditorGraphSchema::GetGraphActions(
	FGraphActionListBuilderBase& IoActionBuilder, 
	const UEdGraphPin* InFromPin, 
	const UEdGraph* InGraph
	) const
{
	// Basic Nodes
	for (UClass* Class : UOptimusNode::GetAllNodeClasses())
	{
		UOptimusNode* Node = Cast<UOptimusNode>(Class->GetDefaultObject());
		if (Node == nullptr)
		{
			continue;
		}

		const FText NodeName = Node->GetDisplayName();
		const FText NodeCategory = FText::FromName(Node->GetNodeCategory());

		TSharedPtr< FOptimusGraphSchemaAction_NewNode> Action(
			new FOptimusGraphSchemaAction_NewNode(
				NodeCategory,
				NodeName,
				/* Tooltip */{}, 0, /* Keywords */{}
		));

		Action->NodeClass = Class;

		IoActionBuilder.AddAction(Action);
	}

	// Constant Value Nodes
	for (FOptimusDataTypeHandle DataTypeHandle: FOptimusDataTypeRegistry::Get().GetAllTypes())
	{
		// For now only allow variable/resource compatible types to spawn constant nodes
		if (DataTypeHandle->CanCreateProperty() &&
			EnumHasAnyFlags(DataTypeHandle->UsageFlags, EOptimusDataTypeUsageFlags::Resource | EOptimusDataTypeUsageFlags::Variable))
		{
			const FText NodeName = FText::Format(LOCTEXT("ConstantValueNode", "{0} Constant"), DataTypeHandle->DisplayName);
			const FText NodeCategory = FText::FromName(UOptimusNode::CategoryName::Values);
			
			TSharedPtr< FOptimusGraphSchemaAction_NewConstantValueNode> Action(
				new FOptimusGraphSchemaAction_NewConstantValueNode(
					NodeCategory,
					NodeName,
					/* Tooltip */{}, 0, /* Keywords */{}
			));

			Action->DataType = DataTypeHandle;

			IoActionBuilder.AddAction(Action);
		}
	}

	// Data Interface Nodes
	for (UClass* Class : UOptimusComputeDataInterface::GetAllComputeDataInterfaceClasses())
	{
		UOptimusComputeDataInterface* DataInterface = Cast<UOptimusComputeDataInterface>(Class->GetDefaultObject());
		if (!ensure(DataInterface != nullptr))
		{
			continue;
		}

		const FText NodeName = FText::FromString(DataInterface->GetDisplayName());

		const FText NodeCategory = FText::FromName(DataInterface->GetCategory());

		TSharedPtr< FOptimusGraphSchemaAction_NewDataInterfaceNode> Action(
			new FOptimusGraphSchemaAction_NewDataInterfaceNode(
				NodeCategory,
				NodeName,
				/* Tooltip */{}, 0, /* Keywords */{}
		));

		Action->DataInterfaceClass = Class;

		IoActionBuilder.AddAction(Action);
	}
	
}


bool UOptimusEditorGraphSchema::TryCreateConnection(
	UEdGraphPin* InPinA, 
	UEdGraphPin* InPinB) const
{
#if WITH_EDITOR
	if (GEditor)
	{
		GEditor->CancelTransaction(0);
	}
#endif
	
	if (InPinA->Direction == EGPD_Input)
	{
		Swap(InPinA, InPinB);
	}

	// The pins should be in the correct order now.
	UOptimusNodePin *OutputModelPin = OptimusEditor::GetModelPinFromGraphPin(InPinA);
	UOptimusNodePin* InputModelPin = OptimusEditor::GetModelPinFromGraphPin(InPinB);

	if (OutputModelPin && InputModelPin)
	{
		if (!OutputModelPin->CanCannect(InputModelPin))
		{
			return false;
		}

		UOptimusNodeGraph *Graph = OutputModelPin->GetOwningNode()->GetOwningGraph();
		return Graph->AddLink(OutputModelPin, InputModelPin);
	}

	// Pins might be connectable if one of the two pins is an adder pin;
	if (OptimusEditor::IsAdderPin(InPinA) || OptimusEditor::IsAdderPin(InPinB))
	{
		const bool bCanConnect = IsValidAdderPinConnection(InPinA, InPinB);

		if (!bCanConnect)
		{
			return false;
		}

		UOptimusNode* TargetNode = OptimusEditor::GetModelNodeFromGraphPin(InPinB);
		UOptimusNodePin* SourcePin = OutputModelPin;

		// Flip Source/Target if the Adder Pin is on the Output side 
		if (OptimusEditor::IsAdderPin(InPinA))
		{
			// Add new pin on the output pin's node
			TargetNode = OptimusEditor::GetModelNodeFromGraphPin(InPinA);
			SourcePin = InputModelPin;
		}
		UOptimusNodeGraph *Graph = TargetNode->GetOwningGraph();
		return Graph->AddPinAndLink(TargetNode, SourcePin);
	}

	return false;
}


const FPinConnectionResponse UOptimusEditorGraphSchema::CanCreateConnection(
	const UEdGraphPin* InPinA, 
	const UEdGraphPin* InPinB
	) const
{
	if (InPinA->Direction == EGPD_Input)
	{
		Swap(InPinA, InPinB);
	}

	// The pins should be in the correct order now.
	UOptimusNodePin* OutputModelPin = OptimusEditor::GetModelPinFromGraphPin(InPinA);
	UOptimusNodePin* InputModelPin = OptimusEditor::GetModelPinFromGraphPin(InPinB);

	if (InputModelPin && OutputModelPin)
	{
		FString FailureReason;
		bool bCanConnect = OutputModelPin->CanCannect(InputModelPin, &FailureReason);

		return FPinConnectionResponse(
			bCanConnect ? CONNECT_RESPONSE_MAKE :  CONNECT_RESPONSE_DISALLOW,
			FText::FromString(FailureReason));
	}
	
	// Pins might be connectable if one of the two pins is an adder pin;
	if (OptimusEditor::IsAdderPin(InPinA) || OptimusEditor::IsAdderPin(InPinB))
	{
		FString FailureReason;
		const bool bCanConnect = IsValidAdderPinConnection(InPinA, InPinB, &FailureReason);

		return FPinConnectionResponse(
			bCanConnect ? CONNECT_RESPONSE_MAKE :  CONNECT_RESPONSE_DISALLOW,
			FText::FromString(FailureReason));
	}
	
	return FPinConnectionResponse( CONNECT_RESPONSE_DISALLOW,FText::GetEmpty());
}


void UOptimusEditorGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const
{
	UOptimusEditorGraphNode* GraphNode = Cast<UOptimusEditorGraphNode>(TargetPin.GetOwningNode());
	UOptimusEditorGraph* EditorGraph = Cast<UOptimusEditorGraph>(GraphNode->GetGraph());

	if (ensure(EditorGraph))
	{
		UOptimusNodeGraph* ModelGraph = EditorGraph->GetModelGraph();

		UOptimusNodePin *TargetModelPin = GraphNode->FindModelPinFromGraphPin(&TargetPin);

		if (ensure(TargetModelPin))
		{
			ModelGraph->RemoveAllLinks(TargetModelPin);
		}
	}
}


void UOptimusEditorGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const
{

}


FConnectionDrawingPolicy* UOptimusEditorGraphSchema::CreateConnectionDrawingPolicy(
	int32 InBackLayerID,
	int32 InFrontLayerID,
	float InZoomFactor,
	const FSlateRect& InClippingRect,
	FSlateWindowElementList& InDrawElements,
	UEdGraph* InGraphObj
	) const
{
	return new FOptimusEditorGraphConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
}


void UOptimusEditorGraphSchema::GetGraphContextActions(
	FGraphContextMenuBuilder& IoContextMenuBuilder
	) const
{
	GetGraphActions(IoContextMenuBuilder, IoContextMenuBuilder.FromPin, IoContextMenuBuilder.CurrentGraph);
}


bool UOptimusEditorGraphSchema::SafeDeleteNodeFromGraph(UEdGraph* InGraph, UEdGraphNode* InNode) const
{
	UOptimusEditorGraphNode* GraphNode = Cast<UOptimusEditorGraphNode>(InNode);

	if (GraphNode)
	{
		UOptimusEditorGraph* Graph = Cast<UOptimusEditorGraph>(GraphNode->GetGraph());

		return Graph->GetModelGraph()->RemoveNode(GraphNode->ModelNode);
	}

	return false;
}

void UOptimusEditorGraphSchema::GetGraphDisplayInformation(const UEdGraph& Graph, FGraphDisplayInfo& DisplayInfo) const
{
	const UOptimusEditorGraph& EditorGraph = static_cast<const UOptimusEditorGraph&>(Graph);
	DisplayInfo.PlainName = FText::FromString(EditorGraph.GetModelGraph()->GetName());
	DisplayInfo.DisplayName = DisplayInfo.PlainName;
}


FEdGraphPinType UOptimusEditorGraphSchema::GetPinTypeFromDataType(
	FOptimusDataTypeHandle InDataType
	)
{
	FEdGraphPinType PinType;

	if (InDataType.IsValid())
	{
		// Set the categories as defined by the registered data type. We hijack the PinSubCategory
		// so that we can query back to the registry for whether the pin color should come out of the
		// K2 schema or the registered custom color.
		PinType.PinCategory = InDataType->TypeCategory;
		PinType.PinSubCategory = InDataType->TypeName;
		PinType.PinSubCategoryObject = InDataType->TypeObject;
	}

	return PinType;
}



FLinearColor UOptimusEditorGraphSchema::GetPinTypeColor(
	const FEdGraphPinType& InPinType
	) const
{
	return GetColorFromPinType(InPinType);
}


const FSlateBrush* UOptimusEditorGraphSchema::GetIconFromPinType(
	const FEdGraphPinType& InPinType
	)
{
	const FSlateBrush* IconBrush = FAppStyle::GetBrush(TEXT("Kismet.VariableList.TypeIcon"));
	const UClass *PinObjectClass = Cast<UClass>(InPinType.PinSubCategoryObject.Get());

	if (PinObjectClass)
	{
		IconBrush = FSlateIconFinder::FindIconBrushForClass(PinObjectClass);
	}

	return IconBrush;
}


FLinearColor UOptimusEditorGraphSchema::GetColorFromPinType(const FEdGraphPinType& InPinType)
{
	if (OptimusEditor::IsAdderPinType(InPinType))
	{
		const UGraphEditorSettings* Settings = GetDefault<UGraphEditorSettings>();
		return Settings->WildcardPinTypeColor;
	}
	
	// Use the PinSubCategory value to resolve the type. It's set in
	// UOptimusEditorGraphSchema::GetPinTypeFromDataType.
	FOptimusDataTypeHandle DataType = FOptimusDataTypeRegistry::Get().FindType(InPinType.PinSubCategory);

	// Unset data types get black pins.
	if (!DataType.IsValid())
	{
		return FLinearColor::Black;
	}

	// If the data type has custom color, use that. Otherwise fall back on the K2 schema
	// since we want to be compatible with known types (which also have preferences for them).
	if (DataType->bHasCustomPinColor)
	{
		return DataType->CustomPinColor;
	}

	return GetDefault<UEdGraphSchema_K2>()->GetPinTypeColor(InPinType);
}


void UOptimusEditorGraphSchema::TrySetDefaultValue(
	UEdGraphPin& Pin, 
	const FString& NewDefaultValue, 
	bool bMarkAsModified
	) const
{
	UOptimusNodePin* ModelPin = OptimusEditor::GetModelPinFromGraphPin(&Pin);
	if (ensure(ModelPin))
	{
		// Kill the existing transaction, since it copies the wrong node.
		GEditor->CancelTransaction(0);

		ModelPin->SetValueFromString(NewDefaultValue);
	}
}

#undef LOCTEXT_NAMESPACE