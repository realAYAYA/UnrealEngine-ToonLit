// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMeshMorph.h"

#include "Animation/MorphTarget.h"
#include "Containers/EnumAsByte.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/SkeletalMesh.h"
#include "Internationalization/Internationalization.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/ICustomizableObjectEditor.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshReshapeCommon.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"
#include "Templates/Casts.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectPtr.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeMeshMorph::UCustomizableObjectNodeMeshMorph()
	: Super()
{

}


void UCustomizableObjectNodeMeshMorph::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	FString PinName = TEXT("Mesh");
	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Mesh, FName(*PinName));
	OutputPin->bDefaultValueIsIgnored = true;

	PinName = TEXT("Mesh");
	UEdGraphPin* MeshPin = CustomCreatePin(EGPD_Input, Schema->PC_Mesh, FName(*PinName));
	MeshPin->bDefaultValueIsIgnored = true;

	PinName = TEXT("Factor");
	UEdGraphPin* FactorPin = CustomCreatePin(EGPD_Input, Schema->PC_Float, FName(*PinName));
	FactorPin->bDefaultValueIsIgnored = true;
}


FText UCustomizableObjectNodeMeshMorph::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Mesh_Morph", "Mesh Morph");
}


FLinearColor UCustomizableObjectNodeMeshMorph::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Mesh);
}


UCustomizableObjectNodeSkeletalMesh* UCustomizableObjectNodeMeshMorph::GetSourceSkeletalMesh() const
{
	if (const UEdGraphPin* Pin = MeshPin())
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*Pin))
		{
			UEdGraphNode* InMeshNode = ConnectedPin->GetOwningNode();
			if (UCustomizableObjectNodeSkeletalMesh* SkeletalMeshNode = Cast<UCustomizableObjectNodeSkeletalMesh>(InMeshNode))
			{
				return SkeletalMeshNode;
			}
			else if (UCustomizableObjectNodeMeshMorph* MorphNode = Cast<UCustomizableObjectNodeMeshMorph>(InMeshNode))
			{
				return MorphNode->GetSourceSkeletalMesh();
			}
		}
	}

	return nullptr;
}


bool UCustomizableObjectNodeMeshMorph::IsNodeOutDatedAndNeedsRefresh()
{
	bool Result = false;

	const UCustomizableObjectNodeSkeletalMesh* SkeletalMeshNode = GetSourceSkeletalMesh();

	if (SkeletalMeshNode && SkeletalMeshNode->SkeletalMesh)
	{
		bool bOutdated = true;
		for (int m = 0; m < SkeletalMeshNode->SkeletalMesh->GetMorphTargets().Num(); ++m)
		{
			FString MorphName = SkeletalMeshNode->SkeletalMesh->GetMorphTargets()[m]->GetName();
			if (MorphTargetName == MorphName)
			{
				bOutdated = false;
				break;
			}
		}
		Result = bOutdated;
	}

	// Remove previous compilation warnings
	if (!Result && bHasCompilerMessage)
	{
		RemoveWarnings();
		GetGraph()->NotifyGraphChanged();
	}

	return Result;
}


FString UCustomizableObjectNodeMeshMorph::GetRefreshMessage() const
{
    return "Morph Target not found in the SkeletalMesh. Please Refresh Node and select a valid morph option.";
}


void UCustomizableObjectNodeMeshMorph::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);

	if (Pin->Direction == EGPD_Input && Pin->PinName == "Mesh")
	{
		TSharedPtr<ICustomizableObjectEditor> Editor = GetGraphEditor();

		if (Editor.IsValid())
		{
			Editor->UpdateGraphNodeProperties();
		}
	}
}


FText UCustomizableObjectNodeMeshMorph::GetTooltipText() const
{
	return LOCTEXT("Mesh_Morph_Tooltip", "Changes the weight of a mesh morph target.");
}


void UCustomizableObjectNodeMeshMorph::Serialize(FArchive& Ar)
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
