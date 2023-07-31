// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/SCustomizableObjectNodeMaterial.h"

#include "Containers/EnumAsByte.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/Nodes/SCustomizableObjectNodeMaterialPinImage.h"
#include "UObject/NameTypes.h"

class SGraphPin;


void SCustomizableObjectNodeMaterial::Construct(const FArguments& InArgs, UEdGraphNode* InGraphNode)
{
	GraphNode = InGraphNode;
	UpdateGraphNode();
}


TSharedPtr<SGraphPin> SCustomizableObjectNodeMaterial::CreatePinWidget(UEdGraphPin* Pin) const
{
	if (Pin->PinType.PinCategory == UEdGraphSchema_CustomizableObject::PC_Image &&
		Pin->Direction == EGPD_Input)
	{
		return SNew(SCustomizableObjectNodeMaterialPinImage, Pin);
	}

	return SGraphNode::CreatePinWidget(Pin);
}
