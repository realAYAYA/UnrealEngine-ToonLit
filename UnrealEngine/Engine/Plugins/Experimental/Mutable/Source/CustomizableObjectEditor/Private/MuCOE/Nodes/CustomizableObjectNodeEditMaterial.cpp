// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeEditMaterial.h"

#include "MaterialTypes.h"
#include "MuCOE/CustomizableObjectEditor_Deprecated.h"
#include "MuCOE/CustomizableObjectLayout.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"

class FCustomizableObjectNodeParentedMaterial;
class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeEditMaterial::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	
	if (const UCustomizableObjectNodeMaterial* ParentMaterialNode = GetParentMaterialNodeIfPath())
	{
		const int32 NumImages = ParentMaterialNode->GetNumParameters(EMaterialParameterType::Texture);
		for (int32 ImageIndex = 0; ImageIndex < NumImages; ++ImageIndex)
		{
			if (ParentMaterialNode->IsImageMutableMode(ImageIndex))
			{
				UCustomizableObjectNodeEditMaterialPinEditImageData* PinEditImageData = NewObject<UCustomizableObjectNodeEditMaterialPinEditImageData>(this);
				PinEditImageData->ImageId = ParentMaterialNode->GetParameterId(EMaterialParameterType::Texture, ImageIndex);
			
				const FName ImageName = ParentMaterialNode->GetParameterName(EMaterialParameterType::Texture, ImageIndex);
				UEdGraphPin* PinImage = CustomCreatePin(EGPD_Input, Schema->PC_Image, ImageName, PinEditImageData);
				PinImage->bHidden = true;
				PinImage->bDefaultValueIsIgnored = true;

				PinsParameter.Add(PinEditImageData->ImageId, FEdGraphPinReference(PinImage));

				FString PinMaskName = ImageName.ToString() + " Mask";
				UEdGraphPin* PinMask = CustomCreatePin(EGPD_Input, Schema->PC_Image, *PinMaskName);
				PinMask->bHidden = true;
				PinMask->bDefaultValueIsIgnored = true;
			
				PinEditImageData->PinMask = FEdGraphPinReference(PinMask);
			}
		}
	}

	CustomCreatePin(EGPD_Output, Schema->PC_Material, TEXT("Material"));
}


const UEdGraphPin* UCustomizableObjectNodeEditMaterial::GetUsedImageMaskPin(const FGuid& ImageId) const
{
	if (const UEdGraphPin* Pin = GetUsedImagePin(ImageId))
	{
		UCustomizableObjectNodeEditMaterialPinEditImageData& PinData = GetPinData<UCustomizableObjectNodeEditMaterialPinEditImageData>(*Pin);
		return PinData.PinMask.Get();
	}

	return nullptr;
}


bool UCustomizableObjectNodeEditMaterial::IsSingleOutputNode() const
{
	return true;
}


bool UCustomizableObjectNodeEditMaterial::CustomRemovePin(UEdGraphPin& Pin)
{
	CustomRemovePinWork(Pin);
	
	return Super::CustomRemovePin(Pin);
}


UEdGraphPin* UCustomizableObjectNodeEditMaterial::OutputPin() const
{
	return Super::OutputPin();
}


void UCustomizableObjectNodeEditMaterial::SetParentNode(UCustomizableObject* Object, FGuid NodeId)
{
	PreSetParentNodeWork(Object, NodeId);

	Super::SetParentNode(Object, NodeId);
	
	PostSetParentNodeWork(Object, NodeId);

	SelectAllLayoutBlocks();
}


void UCustomizableObjectNodeEditMaterial::BackwardsCompatibleFixup()
{
	Super::BackwardsCompatibleFixup();

	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);

	// Convert deprecated node index list to the node id list.
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::PostLoadToCustomVersion
		&& BlockIds.Num() < Blocks_DEPRECATED.Num())
	{
		UCustomizableObjectNodeMaterialBase* ParentMaterialNode = GetParentMaterialNode();

		TArray<UCustomizableObjectLayout*> Layouts = ParentMaterialNode->GetLayouts();

		if (!Layouts.IsValidIndex(ParentLayoutIndex))
		{
			UE_LOG(LogMutable, Warning, TEXT("[%s] UCustomizableObjectNodeEditMaterial refers to an invalid texture layout index %d. Parent node has %d layouts."), 
				*GetOutermost()->GetName(), ParentLayoutIndex, Layouts.Num());
		}
		else
		{
			UCustomizableObjectLayout* Layout = Layouts[ParentLayoutIndex];

			if (Cast<UCustomizableObjectNodeMaterial>(ParentMaterialNode))
			{
				for (int IndexIndex = BlockIds.Num(); IndexIndex < Blocks_DEPRECATED.Num(); ++IndexIndex)
				{
					int BlockIndex = Blocks_DEPRECATED[IndexIndex];
					if (Layout->Blocks.IsValidIndex(BlockIndex) )
					{
						const FGuid Id = Layout->Blocks[BlockIndex].Id;
						if (Id.IsValid())
						{
							BlockIds.Add(Id);
						}
						else
						{
							UE_LOG(LogMutable, Warning, TEXT("[%s] UCustomizableObjectNodeEditMaterial refers to an valid layout block %d but that block doesn't have an id."),
								*GetOutermost()->GetName(), BlockIndex );
						}
					}
					else
					{
						UE_LOG(LogMutable, Warning, TEXT("[%s] UCustomizableObjectNodeEditMaterial refers to an invalid layout block index %d. Parent node has %d blocks."),
							*GetOutermost()->GetName(), BlockIndex, Layout->Blocks.Num());
					}
				}
			}
		}
	}
	
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::AutomaticNodeMaterial)
	{
		if (const UCustomizableObjectNodeMaterial* ParentMaterial = GetParentMaterialNode())
		{
			for (const FCustomizableObjectNodeEditMaterialImage& Image : Images_DEPRECATED)
			{
				const UEdGraphPin* ImagePin = FindPin(Image.Name);
				const UEdGraphPin* PinMask = FindPin(Image.Name + " Mask");
				if (!ImagePin || !PinMask)
				{
					continue;
				}
				
				UCustomizableObjectNodeEditMaterialPinEditImageData*  PinEditImageData = NewObject<UCustomizableObjectNodeEditMaterialPinEditImageData>(this);
				PinEditImageData->ImageId = FGuid::NewGuid();
				PinEditImageData->PinMask = PinMask;
				
				// Search for the Image Id the Edit pin was referring to.
				const int32 NumImages = ParentMaterial->GetNumParameters(EMaterialParameterType::Texture);
				for (int32 ImageIndex = 0; ImageIndex < NumImages; ++ImageIndex)
				{
					if (ParentMaterial->GetParameterName(EMaterialParameterType::Texture, ImageIndex).ToString() == Image.Name)
					{
						PinEditImageData->ImageId = ParentMaterial->GetParameterId(EMaterialParameterType::Texture, ImageIndex);
						break;
					}
				}
				
				AddPinData(*ImagePin, *PinEditImageData);
			}
		}
		
		Images_DEPRECATED.Empty();
		ReconstructNode();
	}

	// Fill PinsParameter.
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::AutomaticNodeMaterialPerformanceBug) // || CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::AutomaticNodeMaterialPerformance
	{
		for (const UEdGraphPin* Pin : GetAllNonOrphanPins())
		{
			if (const UCustomizableObjectNodeEditMaterialPinEditImageData* PinData = Cast<UCustomizableObjectNodeEditMaterialPinEditImageData>(GetPinData(*Pin)))
			{
				PinsParameter.Add(PinData->ImageId, FEdGraphPinReference(Pin));
			}
		}
	}
	
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::EditMaterialOnlyMutableModeParameters)
	{
		ReconstructNode();
	}

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::EditMaterialMaskPinDesync)
	{
		ReconstructNode();
	}
}


void UCustomizableObjectNodeEditMaterial::PostBackwardsCompatibleFixup()
{
	Super::PostBackwardsCompatibleFixup();
	
	PostBackwardsCompatibleFixupWork();
}


void UCustomizableObjectNodeEditMaterial::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FCustomizableObjectCustomVersion::GUID);
}


FText UCustomizableObjectNodeEditMaterial::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Edit_Material", "Edit Material");
}


FLinearColor UCustomizableObjectNodeEditMaterial::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Material);
}


bool UCustomizableObjectNodeEditMaterial::IsNodeOutDatedAndNeedsRefresh()
{
	return IsNodeOutDatedAndNeedsRefreshWork();
}


FString UCustomizableObjectNodeEditMaterial::GetRefreshMessage() const
{
	return "Source material has changed, texture channels might have been added, removed or renamed. Please refresh the parent material node to reflect those changes.";
}


FText UCustomizableObjectNodeEditMaterial::GetTooltipText() const
{
	return LOCTEXT("Edit_Material_Tooltip", "Modify the texture parameters of an ancestor's material partially or completely.");
}


void UCustomizableObjectNodeEditMaterial::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);
	
	PinConnectionListChangedWork(Pin);
}


UCustomizableObjectNode& UCustomizableObjectNodeEditMaterial::GetNode()
{
	return *this;
}


TMap<FGuid, FEdGraphPinReference>& UCustomizableObjectNodeEditMaterial::GetPinsParameter()
{
	return PinsParameter;	
}


FCustomizableObjectNodeParentedMaterial& UCustomizableObjectNodeEditMaterial::GetNodeParentedMaterial()
{
	return *this;	
}


void UCustomizableObjectNodeEditMaterial::SetLayoutIndex(const int32 LayoutIndex)
{
	ParentLayoutIndex = LayoutIndex;
	SelectAllLayoutBlocks();
}


void UCustomizableObjectNodeEditMaterial::SelectAllLayoutBlocks()
{
	BlockIds.Empty();

	// On reset parent material we want to select all nodes 
	if (UCustomizableObjectNodeMaterialBase* ParentMaterialNode = GetParentMaterialNode())
	{
		TArray<UCustomizableObjectLayout*> Layouts = ParentMaterialNode->GetLayouts();

		if (Layouts.IsValidIndex(ParentLayoutIndex))
		{
			UCustomizableObjectLayout* Layout = Layouts[ParentLayoutIndex];

			for (const FCustomizableObjectLayoutBlock& Block : Layout->Blocks)
			{
				BlockIds.AddUnique(Block.Id);
			}
		}
	}
}



#undef LOCTEXT_NAMESPACE

