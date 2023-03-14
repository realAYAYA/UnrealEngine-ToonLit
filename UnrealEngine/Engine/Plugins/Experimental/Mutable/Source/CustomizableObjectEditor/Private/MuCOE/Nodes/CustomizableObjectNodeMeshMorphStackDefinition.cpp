// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMeshMorphStackDefinition.h"

#include "Animation/MorphTarget.h"
#include "Containers/EnumAsByte.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/ICustomizableObjectEditor.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "Templates/Casts.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectPtr.h"

class UCustomizableObjectNodeRemapPins;


#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"



UCustomizableObjectNodeMeshMorphStackDefinition::UCustomizableObjectNodeMeshMorphStackDefinition() : Super()
{

}


void UCustomizableObjectNodeMeshMorphStackDefinition::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

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
		UpdateMorphList();

		TSharedPtr<ICustomizableObjectEditor> Editor = GetGraphEditor();

		if (Editor.IsValid())
		{
			Editor->UpdateGraphNodeProperties();
			Super::ReconstructNode();
		}
	}
}


void UCustomizableObjectNodeMeshMorphStackDefinition::ReconstructNode(UCustomizableObjectNodeRemapPins* RemapPins)
{
	UpdateMorphList();

	Super::ReconstructNode(RemapPins);
}


void UCustomizableObjectNodeMeshMorphStackDefinition::UpdateMorphList()
{
	MorphNames.Empty();

	if (UEdGraphPin* MeshPin = GetMeshPin())
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*MeshPin))
		{
			USkeletalMesh* SkeletalMesh = nullptr;

			if (const UCustomizableObjectNodeSkeletalMesh* SkeletalMeshNode = Cast< UCustomizableObjectNodeSkeletalMesh >(ConnectedPin->GetOuter()))
			{
				SkeletalMesh = SkeletalMeshNode->SkeletalMesh;
			}
			else if (const UCustomizableObjectNodeTable* TableNode = Cast< UCustomizableObjectNodeTable >(ConnectedPin->GetOuter()))
			{
				SkeletalMesh = TableNode->GetColumnDefaultAssetByType<USkeletalMesh>(MeshPin->LinkedTo[0]);
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
	if (UEdGraphPin* MeshPin = GetMeshPin())
	{
		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*MeshPin))
		{
			if (const UCustomizableObjectNodeSkeletalMesh* SkeletalMeshNode = Cast< UCustomizableObjectNodeSkeletalMesh >(ConnectedPin->GetOuter()))
			{
				if (USkeletalMesh* SkeletalMesh = SkeletalMeshNode->SkeletalMesh)
				{
					if(SkeletalMesh->GetMorphTargets().Num() != MorphNames.Num())
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
		}
	}

	return false;
}


#undef LOCTEXT_NAMESPACE
