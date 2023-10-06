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


void UCustomizableObjectNodeMeshMorphStackApplication::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	// Input pins
	CustomCreatePin(EGPD_Input, Schema->PC_Mesh, TEXT("InMesh"));
	CustomCreatePin(EGPD_Input, Schema->PC_Stack, TEXT("Stack"));

	// Output pins
	CustomCreatePin(EGPD_Output, Schema->PC_Mesh, TEXT("Result Mesh"));
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


UEdGraphPin* UCustomizableObjectNodeMeshMorphStackApplication::GetMeshPin() const
{
	return FindPin(TEXT("InMesh"), EGPD_Input);
}


UEdGraphPin* UCustomizableObjectNodeMeshMorphStackApplication::GetStackPin() const
{
	return FindPin(TEXT("Stack"), EGPD_Input);
}

TArray<FString> UCustomizableObjectNodeMeshMorphStackApplication::GetMorphList() const
{
	const UEdGraphPin* MeshPin = GetMeshPin();
	if (!MeshPin)
	{
		return {};
	}

	const UEdGraphPin* OutputMeshPin = FollowInputPin(*MeshPin);
	if (!OutputMeshPin)
	{
		return {};
	}

	TArray<FString> MorphNames;

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

	return MorphNames;
}


#undef LOCTEXT_NAMESPACE
