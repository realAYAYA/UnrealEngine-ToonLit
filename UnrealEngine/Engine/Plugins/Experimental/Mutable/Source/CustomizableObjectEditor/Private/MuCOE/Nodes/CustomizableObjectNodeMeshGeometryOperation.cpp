// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMeshGeometryOperation.h"

#include "EdGraph/EdGraphPin.h"
#include "Internationalization/Internationalization.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "UObject/NameTypes.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeMeshGeometryOperation::UCustomizableObjectNodeMeshGeometryOperation()
	: Super()
{
}


void UCustomizableObjectNodeMeshGeometryOperation::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Mesh, FName("Mesh"));
	OutputPin->bDefaultValueIsIgnored = true;

	UEdGraphPin* MeshPin = CustomCreatePin(EGPD_Input, Schema->PC_Mesh, FName("Mesh A"));
	MeshPin->bDefaultValueIsIgnored = true;

	MeshPin = CustomCreatePin(EGPD_Input, Schema->PC_Mesh, FName("Mesh B"));
	MeshPin->bDefaultValueIsIgnored = true;

	UEdGraphPin* FactorPin = CustomCreatePin(EGPD_Input, Schema->PC_Float, FName("Scalar A"));
	FactorPin->bDefaultValueIsIgnored = true;

	FactorPin = CustomCreatePin(EGPD_Input, Schema->PC_Float, FName("Scalar B"));
	FactorPin->bDefaultValueIsIgnored = true;
}


FText UCustomizableObjectNodeMeshGeometryOperation::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Mesh_GeometryOperation", "Geometry Operation");
}


FLinearColor UCustomizableObjectNodeMeshGeometryOperation::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Mesh);
}


FText UCustomizableObjectNodeMeshGeometryOperation::GetTooltipText() const
{
	return LOCTEXT("Mesh_GeometryOperation_Tooltip", "Apply a Geometry operation on a mesh.");
}



#undef LOCTEXT_NAMESPACE
