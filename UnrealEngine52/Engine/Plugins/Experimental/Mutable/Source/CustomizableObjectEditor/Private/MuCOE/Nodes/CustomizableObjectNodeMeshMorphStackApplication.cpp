// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMeshMorphStackApplication.h"

#include "Engine/SkeletalMesh.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/ICustomizableObjectEditor.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"

class UCustomizableObjectNodeRemapPins;


#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"



UCustomizableObjectNodeMeshMorphStackApplication::UCustomizableObjectNodeMeshMorphStackApplication() : Super()
{

}


void UCustomizableObjectNodeMeshMorphStackApplication::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	// Input pins
	FString PinName = TEXT("InMesh");
	UEdGraphPin* InputMeshPin = CustomCreatePin(EGPD_Input, Schema->PC_Mesh, FName(*PinName));

	PinName = TEXT("Stack");
	UEdGraphPin* StackPin = CustomCreatePin(EGPD_Input, Schema->PC_Stack, FName(*PinName));

	// Output pins
	PinName = TEXT("Result Mesh");
	UEdGraphPin* OutputMeshPin = CustomCreatePin(EGPD_Output, Schema->PC_Mesh, FName(*PinName));

}


FText UCustomizableObjectNodeMeshMorphStackApplication::GetNodeTitle(ENodeTitleType::Type TittleType)const
{
	return LOCTEXT("Mesh_Morph_Stack_Application", "Mesh Morph Stack Application");
}


FLinearColor UCustomizableObjectNodeMeshMorphStackApplication::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Mesh);
}


FText UCustomizableObjectNodeMeshMorphStackApplication::GetTooltipText() const
{
	return LOCTEXT("Morph_Stack_Application_Tooltip","Applies a morph stack to a mesh");
}


void UCustomizableObjectNodeMeshMorphStackApplication::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);

	if (Pin->Direction == EGPD_Input && Pin->PinName == "InMesh")
	{
		UpdateMorphList();

		TSharedPtr<ICustomizableObjectEditor> Editor = GetGraphEditor();

		if (Editor.IsValid())
		{
			Editor->UpdateGraphNodeProperties();
			MarkForReconstruct();
		}
	}
}


void UCustomizableObjectNodeMeshMorphStackApplication::ReconstructNode(UCustomizableObjectNodeRemapPins* RemapPins)
{
	UpdateMorphList();

	Super::ReconstructNode(RemapPins);
}


void UCustomizableObjectNodeMeshMorphStackApplication::UpdateMorphList()
{
	MorphNames.Empty();

	UEdGraphPin* MeshPin = GetMeshPin();

	if (!MeshPin)
	{
		return;
	}

	UEdGraphPin* OutputMeshPin = FollowInputPin(*MeshPin);

	if (!OutputMeshPin)
	{
		return;
	}

	const UEdGraphPin* MeshNodePin = FindMeshBaseSource(*OutputMeshPin, false);

	if (MeshNodePin && MeshNodePin->GetOwningNode())
	{
		USkeletalMesh* SkeletalMesh = nullptr;

		if (const UCustomizableObjectNodeSkeletalMesh* SkeletalMeshNode = Cast< UCustomizableObjectNodeSkeletalMesh >(MeshNodePin->GetOwningNode()))
		{
			SkeletalMesh = SkeletalMeshNode->SkeletalMesh;
		}
		else if (const UCustomizableObjectNodeTable* TableNode = Cast< UCustomizableObjectNodeTable >(MeshNodePin->GetOwningNode()))
		{
			SkeletalMesh = TableNode->GetColumnDefaultAssetByType<USkeletalMesh>(MeshNodePin);
		}

		if (SkeletalMesh)
		{
			for (int32 i = 0; i < SkeletalMesh->GetMorphTargets().Num(); ++i)
			{
				MorphNames.Add(SkeletalMesh->GetMorphTargets()[i]->GetName());
			}
		}
	}
}


bool UCustomizableObjectNodeMeshMorphStackApplication::IsNodeOutDatedAndNeedsRefresh()
{
	UEdGraphPin* MeshPin = GetMeshPin();

	if (!MeshPin)
	{
		return false;
	}

	UEdGraphPin* OutputMeshPin = FollowInputPin(*MeshPin);

	if (!OutputMeshPin)
	{
		if (MorphNames.Num())
		{
			return true;
		}

		return false;
	}

	const UEdGraphPin* MeshNodePin = FindMeshBaseSource(*OutputMeshPin, false);

	if (MeshNodePin && MeshNodePin->GetOwningNode())
	{
		USkeletalMesh* SkeletalMesh = nullptr;

		if (const UCustomizableObjectNodeSkeletalMesh* SkeletalMeshNode = Cast< UCustomizableObjectNodeSkeletalMesh >(MeshNodePin->GetOwningNode()))
		{
			SkeletalMesh = SkeletalMeshNode->SkeletalMesh;
		}
		else if (const UCustomizableObjectNodeTable* TableNode = Cast< UCustomizableObjectNodeTable >(MeshNodePin->GetOwningNode()))
		{
			SkeletalMesh = TableNode->GetColumnDefaultAssetByType<USkeletalMesh>(MeshNodePin);
		}

		if (SkeletalMesh)
		{
			if (SkeletalMesh->GetMorphTargets().Num() != MorphNames.Num())
			{
				return true;
			}

			for (int32 i = 0; i < SkeletalMesh->GetMorphTargets().Num(); ++i)
			{
				if (!MorphNames.Contains(SkeletalMesh->GetMorphTargets()[i]->GetName()))
				{
					return true;
				}
			}
		}
	}

	return false;
}


#undef LOCTEXT_NAMESPACE
