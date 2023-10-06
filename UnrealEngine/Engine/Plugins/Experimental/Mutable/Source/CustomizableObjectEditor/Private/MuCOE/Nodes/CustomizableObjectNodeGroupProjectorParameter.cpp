// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeGroupProjectorParameter.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


TArray<FGroupProjectorParameterImage> UCustomizableObjectNodeGroupProjectorParameter::GetOptionImagesFromTable() const
{
	TArray<FGroupProjectorParameterImage> ArrayResult;

	if (OptionImagesDataTable == nullptr)
	{
		return ArrayResult;
	}

	TArray<FName> ArrayRowName = OptionImagesDataTable->GetRowNames();

	FProperty* PropertyTexturePath = OptionImagesDataTable->FindTableProperty(DataTableTextureColumnName);

	if (PropertyTexturePath == nullptr)
	{
		UE_LOG(LogMutable, Warning, TEXT("WARNING: No column found with texture path information to load projection textures"));
		return ArrayResult;
	}

	int32 NameIndex = 0;

	for (TMap<FName, uint8*>::TConstIterator RowIt = OptionImagesDataTable->GetRowMap().CreateConstIterator(); RowIt; ++RowIt)
	{
		uint8* RowData = RowIt.Value();
		FString PropertyValue(TEXT(""));
		PropertyTexturePath->ExportText_InContainer(0, PropertyValue, RowData, RowData, nullptr, PPF_None);
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *PropertyValue, nullptr);

		if (Texture == nullptr)
		{
			UE_LOG(LogMutable, Warning, TEXT("WARNING: Unable to load texture %s"), *PropertyValue);
		}
		else
		{
			FGroupProjectorParameterImage GroupProjectorParameterImage;
			GroupProjectorParameterImage.OptionName = ArrayRowName[NameIndex].ToString();
			GroupProjectorParameterImage.OptionImage = Texture;
			ArrayResult.Add(GroupProjectorParameterImage);
		}

		NameIndex++;
	}

	return ArrayResult;
}


TArray<FGroupProjectorParameterImage> UCustomizableObjectNodeGroupProjectorParameter::GetFinalOptionImagesNoRepeat() const
{
	TArray<FGroupProjectorParameterImage> ArrayDataTable = GetOptionImagesFromTable();

	for (int32 i = 0; i < OptionImages.Num(); ++i)
	{
		bool AlreadyAdded = false;
		for (int32 j = 0; j < ArrayDataTable.Num(); ++j)
		{
			if (OptionImages[i].OptionName == ArrayDataTable[j].OptionName)
			{
				AlreadyAdded = true;
				break;
			}
		}

		if (!AlreadyAdded)
		{
			ArrayDataTable.Add(OptionImages[i]);
		}
	}

	return ArrayDataTable;
}


FText UCustomizableObjectNodeGroupProjectorParameter::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Group_Projector_Parameter", "Group Projector Parameter");
}


FLinearColor UCustomizableObjectNodeGroupProjectorParameter::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_GroupProjector);
}


FText UCustomizableObjectNodeGroupProjectorParameter::GetTooltipText() const
{
	return LOCTEXT("Group_Projector_Parameter_Tooltip", "Projects one or many textures to all children in the group it's connected to. It modifies only the materials that define a specific material asset texture parameter.");
}


void UCustomizableObjectNodeGroupProjectorParameter::BackwardsCompatibleFixup()
{
	Super::BackwardsCompatibleFixup();

	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);
	
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::GroupProjectorPinTypeAdded)
	{
		if (UEdGraphPin* Pin = ProjectorPin())
		{
			Pin->PinType.PinCategory = UEdGraphSchema_CustomizableObject::PC_GroupProjector;
		}
	}

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::GroupProjectorImagePinRemoved)
	{
		ReconstructNode();		
	}
}


void UCustomizableObjectNodeGroupProjectorParameter::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	CustomCreatePin(EGPD_Output, Schema->PC_GroupProjector, TEXT("Value"));	
}


#undef LOCTEXT_NAMESPACE
