// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeCopyMaterial.h"

#include "Containers/EnumAsByte.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphPin.h"
#include "HAL/Platform.h"
#include "Internationalization/Internationalization.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeExposePin.h"
#include "MuCOE/Nodes/CustomizableObjectNodeExternalPin.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"
#include "MuCOE/RemapPins/CustomizableObjectNodeRemapPins.h"
#include "Templates/Casts.h"
#include "UObject/NameTypes.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


/** Mesh input pin key */
const static FString MeshPinName = TEXT("Mesh_Input_Pin");

/** Material input pin key */
const static FString MaterialPinName = TEXT("Material_Input_Pin");

FText UCustomizableObjectNodeCopyMaterial::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Copy_Material", "Copy Material");
}

void UCustomizableObjectNodeCopyMaterial::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	if (UCustomizableObjectNodeMaterialRemapPinsByName* RemapPinsByNameCustom = Cast<UCustomizableObjectNodeMaterialRemapPinsByName>(RemapPins))
	{
		RemapPinsByNameCustom->Node = this;
	}
	
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	// Input pins
	FString PinFriendlyName = TEXT("Mesh");
	UEdGraphPin* Pin = CustomCreatePin(EGPD_Input, Schema->PC_Mesh, FName(MeshPinName));
	Pin->PinFriendlyName = FText::FromString(PinFriendlyName);
	Pin->bDefaultValueIsIgnored = true;

	PinFriendlyName = TEXT("Base Material");
	Pin = CustomCreatePin(EGPD_Input, Schema->PC_Material, FName(MaterialPinName));
	Pin->PinFriendlyName = FText::FromString(PinFriendlyName);
	Pin->bDefaultValueIsIgnored = true;

	// Output pins
	PinFriendlyName = TEXT("Material");
	FString PinName = PinFriendlyName + FString(TEXT("_Output_Pin"));
	Pin = CustomCreatePin(EGPD_Output, Schema->PC_Material, FName(*PinName));
	Pin->PinFriendlyName = FText::FromString(PinFriendlyName);
}

UEdGraphPin* UCustomizableObjectNodeCopyMaterial::GetMeshPin() const
{
	return FindPin(MeshPinName);
}

UEdGraphPin* UCustomizableObjectNodeCopyMaterial::GetMaterialPin() const
{
	return FindPin(MaterialPinName);
}

UCustomizableObjectNodeSkeletalMesh* UCustomizableObjectNodeCopyMaterial::GetMeshNode() const
{
	UCustomizableObjectNodeSkeletalMesh* Result = nullptr;

	UEdGraphPin* MaterialPin = GetMeshPin();
	if (const UEdGraphPin* ConnectedPin = FollowInputPin(*MaterialPin))
	{
		const UEdGraphPin* SourceMeshPin = FindMeshBaseSource(*ConnectedPin, false);
		if (SourceMeshPin)
		{
			Result = Cast<UCustomizableObjectNodeSkeletalMesh>(SourceMeshPin->GetOwningNode());
		}
	}

	return Result;
}


UCustomizableObjectNodeMaterial* UCustomizableObjectNodeCopyMaterial::GetMaterialNode() const
{
	UCustomizableObjectNodeMaterial* Result = nullptr;

	UEdGraphPin* MaterialPin = GetMaterialPin();
	if (const UEdGraphPin* ConnectedPin = FollowInputPin(*MaterialPin))
	{
		return Cast<UCustomizableObjectNodeMaterial>(ConnectedPin->GetOwningNode());
	}

	return Result;
}

bool UCustomizableObjectNodeCopyMaterial::CanConnect(const UEdGraphPin* InOwnedInputPin, const UEdGraphPin* InOutputPin, bool& bOutIsOtherNodeBlocklisted, bool& bOutArePinsCompatible) const
{
	if (!Super::CanConnect(InOwnedInputPin, InOutputPin, bOutIsOtherNodeBlocklisted, bOutIsOtherNodeBlocklisted))
	{
		return false;
	}

	if (InOwnedInputPin == GetMaterialPin())
	{
		const UEdGraphNode* OuputPinOwningNode = InOutputPin->GetOwningNode();
		return (OuputPinOwningNode->IsA(UCustomizableObjectNodeMaterial::StaticClass()) && !OuputPinOwningNode->IsA(UCustomizableObjectNodeCopyMaterial::StaticClass()))
			|| OuputPinOwningNode->IsA(UCustomizableObjectNodeExternalPin::StaticClass());
	}

	return true;
}

bool UCustomizableObjectNodeCopyMaterial::ShouldBreakExistingConnections(const UEdGraphPin* InputPin, const UEdGraphPin* OutputPin) const
{
	return true;
}

bool UCustomizableObjectNodeCopyMaterial::IsNodeOutDatedAndNeedsRefresh()
{
	return false;
}

bool UCustomizableObjectNodeCopyMaterial::ProvidesCustomPinRelevancyTest() const
{
	return true;
}

bool UCustomizableObjectNodeCopyMaterial::IsPinRelevant(const UEdGraphPin* Pin) const
{
	const UEdGraphNode* Node = Pin->GetOwningNode();

	if (Pin->Direction == EGPD_Output)
	{
		return (Node->IsA(UCustomizableObjectNodeMaterial::StaticClass()) && !Node->IsA(UCustomizableObjectNodeCopyMaterial::StaticClass())) ||
			(Node->IsA(UCustomizableObjectNodeExternalPin::StaticClass()) && Pin->PinType.PinCategory == UEdGraphSchema_CustomizableObject::PC_Material) ||
			Pin->PinType.PinCategory == UEdGraphSchema_CustomizableObject::PC_Mesh;
	}
	else
	{
		return Node->IsA(UCustomizableObjectNodeObject::StaticClass()) ||
			(Node->IsA(UCustomizableObjectNodeExposePin::StaticClass()) && Pin->PinType.PinCategory == UEdGraphSchema_CustomizableObject::PC_Material);
			
	}
}

FText UCustomizableObjectNodeCopyMaterial::GetTooltipText() const
{
	return LOCTEXT("CopyMaterial_Tooltip", "Copies a Customizable Object material.\nDuplicates all Material node input pins and properties except for the Mesh input pin.");
}

#undef LOCTEXT_NAMESPACE
