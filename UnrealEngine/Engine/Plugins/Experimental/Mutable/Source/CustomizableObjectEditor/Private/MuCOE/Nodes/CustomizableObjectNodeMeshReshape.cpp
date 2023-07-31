// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMeshReshape.h"

#include "EdGraph/EdGraphPin.h"
#include "Internationalization/Internationalization.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshReshapeCommon.h"
#include "UObject/NameTypes.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeMeshReshape::UCustomizableObjectNodeMeshReshape()
	: Super()
{
}


void UCustomizableObjectNodeMeshReshape::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Mesh, FName("Mesh"));
	OutputPin->bDefaultValueIsIgnored = true;

	UEdGraphPin* MeshPin = CustomCreatePin(EGPD_Input, Schema->PC_Mesh, FName("Base Mesh"));
	MeshPin->bDefaultValueIsIgnored = true;

	MeshPin = CustomCreatePin(EGPD_Input, Schema->PC_Mesh, FName("Base Shape"));
	MeshPin->bDefaultValueIsIgnored = true;

	MeshPin = CustomCreatePin(EGPD_Input, Schema->PC_Mesh, FName("Target Shape"));
	MeshPin->bDefaultValueIsIgnored = true;
}


FText UCustomizableObjectNodeMeshReshape::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Mesh_Reshape", "Mesh Reshape");
}


FLinearColor UCustomizableObjectNodeMeshReshape::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Mesh);
}


FText UCustomizableObjectNodeMeshReshape::GetTooltipText() const
{
	return LOCTEXT("Mesh_Reshape_Tooltip", "Apply a mesh reshape on a mesh.");
}


FString UCustomizableObjectNodeMeshReshape::GetRefreshMessage() const
{
	return "One or more bones selected to deform do not exist in the current reference mesh skeleton!";
}


void UCustomizableObjectNodeMeshReshape::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (Ar.CustomVer(FCustomizableObjectCustomVersion::GUID) < FCustomizableObjectCustomVersion::DeformSkeletonOptionsAdded)
	{
		if (bDeformAllBones_DEPRECATED)
		{
			SelectionMethod = EBoneDeformSelectionMethod::ALL_BUT_SELECTED;
			BonesToDeform.Empty();
		}
	}
}

#undef LOCTEXT_NAMESPACE
