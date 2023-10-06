// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMeshMorphStackDefinition.h"

#include "Engine/SkeletalMesh.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/ICustomizableObjectEditor.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"

class UCustomizableObjectNodeRemapPins;


#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeMeshMorphStackDefinition::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UpdateMorphList();
	
	// Input pins
	UEdGraphPin* MeshPin = CustomCreatePin(EGPD_Input, Schema->PC_Mesh, FName(TEXT("Mesh")));
	MeshPin->bDefaultValueIsIgnored = true;

	for (int32 i = 0; i < MorphNames.Num(); ++i)
	{
		FString PinName = MorphNames[i] + FString("_Morph");
		UEdGraphPin* Pin = CustomCreatePin(EGPD_Input, Schema->PC_Float, FName(*PinName));
		Pin->PinFriendlyName = FText::FromString(MorphNames[i]);
		Pin->bDefaultValueIsIgnored = true;
	}

	// Output pins
	UEdGraphPin* StackPin = CustomCreatePin(EGPD_Output, Schema->PC_Stack, FName(TEXT("Stack")));
	StackPin->bDefaultValueIsIgnored = true;
}


FText UCustomizableObjectNodeMeshMorphStackDefinition::GetNodeTitle(ENodeTitleType::Type TittleType)const
{
	return LOCTEXT("Mesh_Morph_Stack_Definition", "Mesh Morph Stack Definition");
}


FLinearColor UCustomizableObjectNodeMeshMorphStackDefinition::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Mesh);
}


FText UCustomizableObjectNodeMeshMorphStackDefinition::GetTooltipText() const
{
	return LOCTEXT("Morph_Stack_Definition_Tooltip","Allows to stack morphs");
}


void UCustomizableObjectNodeMeshMorphStackDefinition::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);

	if (Pin->Direction == EGPD_Input && Pin->PinName == "Mesh")
	{
		Super::ReconstructNode();
	}
}


void UCustomizableObjectNodeMeshMorphStackDefinition::ReconstructNode(UCustomizableObjectNodeRemapPins* RemapPins)
{
	Super::ReconstructNode(RemapPins);
}


void UCustomizableObjectNodeMeshMorphStackDefinition::UpdateMorphList()
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


UEdGraphPin* UCustomizableObjectNodeMeshMorphStackDefinition::GetMeshPin() const
{
	return FindPin(TEXT("Mesh"), EGPD_Input);
}


UEdGraphPin* UCustomizableObjectNodeMeshMorphStackDefinition::GetStackPin() const
{
	return FindPin(TEXT("Stack"), EGPD_Output);
}


UEdGraphPin* UCustomizableObjectNodeMeshMorphStackDefinition::GetMorphPin(int32 Index) const
{
	FString PinName = MorphNames[Index] + FString("_Morph");

	return FindPin(PinName);
}


int32 UCustomizableObjectNodeMeshMorphStackDefinition::NextConnectedPin(int32 Index, TArray<FString> AvailableMorphs) const
{
	if (AvailableMorphs.Num())
	{
		for (int32 i = Index + 1; i < MorphNames.Num(); ++i)
		{
			FString PinName = MorphNames[i] + FString("_Morph");

			UEdGraphPin* NextPin = FindPin(PinName);

			// Checking if the node contains another connected pin and if the morph of this pin existis in the skeletal mesh
			if (NextPin && FollowInputPin(*NextPin) && AvailableMorphs.Contains(MorphNames[i]))
			{
				return i;
			}
		}
	}

	return -1;
}


bool UCustomizableObjectNodeMeshMorphStackDefinition::IsNodeOutDatedAndNeedsRefresh()
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
