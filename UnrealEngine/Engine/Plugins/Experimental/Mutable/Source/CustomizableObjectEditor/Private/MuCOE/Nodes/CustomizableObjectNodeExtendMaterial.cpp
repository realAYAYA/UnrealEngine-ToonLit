// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeExtendMaterial.h"

#include "EdGraph/EdGraphPin.h"
#include "HAL/Platform.h"
#include "Internationalization/Internationalization.h"
#include "MaterialTypes.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/CustomizableObjectEditor_Deprecated.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/Nodes/CustomizableObjectNodeCopyMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeParentedNode.h"
#include "Serialization/Archive.h"
#include "Templates/Casts.h"
#include "UObject/NameTypes.h"

class UCustomizableObjectNode;
class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeExtendMaterial::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	
	Ar.UsingCustomVersion(FCustomizableObjectCustomVersion::GUID);
}


void UCustomizableObjectNodeExtendMaterial::BackwardsCompatibleFixup()
{
	Super::PostBackwardsCompatibleFixup();

	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);
	
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::ExtendMaterialRemoveImages)
	{
		if (const UCustomizableObjectNodeMaterial* ParentMaterial = GetParentMaterialNode())
		{
			for (const FCustomizableObjectNodeExtendMaterialImage& Image : Images_DEPRECATED)
			{
				const UEdGraphPin* ImagePin = FindPin(Image.Name);
				if (!ImagePin)
				{
					continue;
				}

				FGuid ImageId = FGuid::NewGuid();

				// Search for the Image Id the Extend pin was referring to.
				const int32 NumImages = ParentMaterial->GetNumParameters(EMaterialParameterType::Texture);
				for (int32 ImageIndex = 0; ImageIndex < NumImages; ++ImageIndex)
				{
					if (ParentMaterial->GetParameterName(EMaterialParameterType::Texture, ImageIndex).ToString() == Image.Name)
					{
						ImageId = ParentMaterial->GetParameterId(EMaterialParameterType::Texture, ImageIndex);
						break;
					}
				}
                
				PinsParameter.Add(ImageId, FEdGraphPinReference(ImagePin));                
			}
		}
		
		Images_DEPRECATED.Empty();
		ReconstructNode();
	}
}

void UCustomizableObjectNodeExtendMaterial::PostBackwardsCompatibleFixup()
{
	Super::PostBackwardsCompatibleFixup();
	
	PostBackwardsCompatibleFixupWork();
}


void UCustomizableObjectNodeExtendMaterial::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UEdGraphPin* AddMeshPin = CustomCreatePin(EGPD_Input, Schema->PC_Mesh, FName("Add Mesh") );
	AddMeshPin->bDefaultValueIsIgnored = true;

	// Begin texture pins
	UCustomizableObjectNodeMaterial* ParentMaterialNode = GetParentMaterialNodeIfPath();
	
	// In case of a NodeCopyMaterial get the real NodeMaterial
	if (const UCustomizableObjectNodeCopyMaterial* ParentMaterialCopyNode = Cast<UCustomizableObjectNodeCopyMaterial>(ParentMaterialNode))
	{
		ParentMaterialNode = ParentMaterialCopyNode->GetMaterialNode();
	}

	if (ParentMaterialNode)
	{
		const int32 NumImages = ParentMaterialNode->GetNumParameters(EMaterialParameterType::Texture);
		for (int32 ImageIndex = 0; ImageIndex < NumImages; ++ImageIndex)
		{
			if (ParentMaterialNode->IsImageMutableMode(ImageIndex))
			{
				const FName ImageName = ParentMaterialNode->GetParameterName(EMaterialParameterType::Texture, ImageIndex);
				UEdGraphPin* PinImage = CustomCreatePin(EGPD_Input, Schema->PC_Image, ImageName);
				PinImage->bDefaultValueIsIgnored = true;

				const FGuid ImageId = ParentMaterialNode->GetParameterId(EMaterialParameterType::Texture, ImageIndex);
				
				PinsParameter.Add(ImageId, FEdGraphPinReference(PinImage));
			}
		}
	}
	// End texture pins

	CustomCreatePin(EGPD_Output, Schema->PC_Material, FName("Material"));
}


FText UCustomizableObjectNodeExtendMaterial::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Extend_Material", "Extend Material");
}


FLinearColor UCustomizableObjectNodeExtendMaterial::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Material);
}


void UCustomizableObjectNodeExtendMaterial::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);

	PinConnectionListChangedWork(Pin);
}


bool UCustomizableObjectNodeExtendMaterial::IsNodeOutDatedAndNeedsRefresh()
{
	return IsNodeOutDatedAndNeedsRefreshWork();
}


FString UCustomizableObjectNodeExtendMaterial::GetRefreshMessage() const
{
	return "Source material has changed, texture channels might have been added, removed or renamed. Please refresh the parent material node to reflect those changes.";
}


void UCustomizableObjectNodeExtendMaterial::SaveParentNode(UCustomizableObject* Object, FGuid NodeId)
{
	ParentMaterialObject = Object;
	ParentMaterialNodeId = NodeId;
}


UEdGraphPin* UCustomizableObjectNodeExtendMaterial::AddMeshPin() const
{
	return FindPin(TEXT("Add Mesh"));
}


UEdGraphPin* UCustomizableObjectNodeExtendMaterial::OutputPin() const
{
	return FindPin(TEXT("Material"));
}

void UCustomizableObjectNodeExtendMaterial::SetParentNode(UCustomizableObject* Object, FGuid NodeId)
{
	PreSetParentNodeWork(Object, NodeId);

	ICustomizableObjectNodeParentedNode::SetParentNode(Object, NodeId);
	
	PostSetParentNodeWork(Object, NodeId);
}


FText UCustomizableObjectNodeExtendMaterial::GetTooltipText() const
{
	return LOCTEXT("Extend_Material_Tooltip", "Extend an ancestor's material: add a new mesh section, and add its corresponding texture to the ancestor's material texture parameters.");
}


bool UCustomizableObjectNodeExtendMaterial::IsSingleOutputNode() const
{
	return true;
}


bool UCustomizableObjectNodeExtendMaterial::CustomRemovePin(UEdGraphPin& Pin)
{
	CustomRemovePinWork(Pin);
	
	return Super::CustomRemovePin(Pin);
}


UCustomizableObjectNode& UCustomizableObjectNodeExtendMaterial::GetNode()
{
	return *this;
}


TMap<FGuid, FEdGraphPinReference>& UCustomizableObjectNodeExtendMaterial::GetPinsParameter()
{
	return PinsParameter;
}


FGuid UCustomizableObjectNodeExtendMaterial::GetParentNodeId() const
{
	return ParentMaterialNodeId;
}


UCustomizableObject* UCustomizableObjectNodeExtendMaterial::GetParentObject() const
{
	return ParentMaterialObject;
}


FCustomizableObjectNodeParentedMaterial& UCustomizableObjectNodeExtendMaterial::GetNodeParentedMaterial()
{
	return *this;
}


#undef LOCTEXT_NAMESPACE
