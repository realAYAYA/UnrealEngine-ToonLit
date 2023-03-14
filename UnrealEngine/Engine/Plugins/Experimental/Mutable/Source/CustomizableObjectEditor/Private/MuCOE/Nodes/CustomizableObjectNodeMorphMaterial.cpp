// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMorphMaterial.h"

#include "Animation/MorphTarget.h"
#include "Containers/Array.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/SkeletalMesh.h"
#include "Internationalization/Internationalization.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/ICustomizableObjectEditor.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"
#include "Templates/Casts.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectPtr.h"

class UCustomizableObjectNodeRemapPins;
struct FPropertyChangedEvent;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeMorphMaterial::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	//if (!MorphPin)
	//{
	//	MorphPin = CreatePin(EGPD_Input, Schema->PC_Mesh, FString(), NULL, false, false, TEXT("Morph") );
	//	MorphPin->bDefaultValueIsIgnored = true;
	//}
	//else
	//{
	//	Pins.Add( MorphPin );
	//}

	FString PinName = TEXT("Material");
	UEdGraphPin* MaterialPin = CustomCreatePin(EGPD_Output, Schema->PC_Material, FName(PinName));
	MaterialPin->bDefaultValueIsIgnored = true;

	PinName = TEXT("Factor");
	UEdGraphPin* FactorPin = CustomCreatePin(EGPD_Input, Schema->PC_Float, FName(*PinName));
	FactorPin->bDefaultValueIsIgnored = true;
}


FText UCustomizableObjectNodeMorphMaterial::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Morph_Material", "Morph Material");
}


FLinearColor UCustomizableObjectNodeMorphMaterial::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Material);
}


bool UCustomizableObjectNodeMorphMaterial::IsNodeOutDatedAndNeedsRefresh()
{
	bool Result = false;

	if (const UCustomizableObjectNodeMaterial* CustomizableObjectNodeMaterial = Cast<UCustomizableObjectNodeMaterial>(GetParentMaterialNode()))
	{
		if (const UEdGraphPin* BaseSourcePin = FollowInputPin(*CustomizableObjectNodeMaterial->GetMeshPin()))
		{
			if (const UCustomizableObjectNodeSkeletalMesh* TypedSourceNode = Cast<UCustomizableObjectNodeSkeletalMesh>(BaseSourcePin->GetOwningNode()))
			{
				if (const USkeletalMesh* SkeletalMesh = TypedSourceNode->SkeletalMesh)
				{
					bool bOutdated = true;
					for (int m = 0; m < SkeletalMesh->GetMorphTargets().Num(); ++m)
					{
						if (MorphTargetName == *SkeletalMesh->GetMorphTargets()[m]->GetName())
						{
							bOutdated = false;
							break;
						}
					}
					Result = bOutdated;
				}
			}
		}
	}

	// Remove previous compilation warnings
	if (!Result && bHasCompilerMessage)
	{
		RemoveWarnings();
		GetGraph()->NotifyGraphChanged();
	}

    return Result;
}


FString UCustomizableObjectNodeMorphMaterial::GetRefreshMessage() const
{
	return "Morph Target not found in the SkeletalMesh. Please Refresh Node and select a valid morph option.";
}


FText UCustomizableObjectNodeMorphMaterial::GetTooltipText() const
{
	return LOCTEXT("Morph_Material_Tooltip", "Fully activate one morph of a parent's material.");
}


void UCustomizableObjectNodeMorphMaterial::BackwardsCompatibleFixup()
{
	Super::BackwardsCompatibleFixup();

	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::MorphMaterialAddFactorPin)
	{
		ReconstructNode();
	}
}


void UCustomizableObjectNodeMorphMaterial::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);

	if (Pin->PinName == "Material")
	{
		TSharedPtr<ICustomizableObjectEditor> Editor = GetGraphEditor();

		if (Editor.IsValid())
		{
			MorphTargetName = "";
			Editor->UpdateGraphNodeProperties();
		}
	}
}


void UCustomizableObjectNodeMorphMaterial::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	TSharedPtr<ICustomizableObjectEditor> Editor = GetGraphEditor();
	
	if (Editor.IsValid())
	{
		Editor->UpdateGraphNodeProperties();
	}
}


bool UCustomizableObjectNodeMorphMaterial::IsSingleOutputNode() const
{
	return true;
}


#undef LOCTEXT_NAMESPACE
