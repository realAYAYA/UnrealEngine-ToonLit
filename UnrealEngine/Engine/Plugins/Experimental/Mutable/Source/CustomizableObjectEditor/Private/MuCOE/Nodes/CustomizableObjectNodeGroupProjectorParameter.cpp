// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeGroupProjectorParameter.h"

#include "Containers/Map.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "Serialization/Archive.h"
#include "Trace/Detail/Channel.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UnrealType.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeGroupProjectorParameter::UCustomizableObjectNodeGroupProjectorParameter()
	: Super()
{

}


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

	for (auto RowIt = Helper_GetRowMap(OptionImagesDataTable).CreateConstIterator(); RowIt; ++RowIt)
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


void UCustomizableObjectNodeGroupProjectorParameter::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FCustomizableObjectCustomVersion::GUID);

	UEdGraphPin* projectorPin = ProjectorPin();
	if (Ar.CustomVer(FCustomizableObjectCustomVersion::GUID) < FCustomizableObjectCustomVersion::GroupProjectorPinTypeAdded
		&& projectorPin
		&& projectorPin->PinType.PinCategory == UEdGraphSchema_CustomizableObject::PC_Projector)
	{
		projectorPin->PinType.PinCategory = UEdGraphSchema_CustomizableObject::PC_GroupProjector;
	}
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


void UCustomizableObjectNodeGroupProjectorParameter::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	FString PinName = TEXT("Value");
	UEdGraphPin* ValuePin = CustomCreatePin(EGPD_Output, Schema->PC_GroupProjector, FName(*PinName));
	ValuePin->bDefaultValueIsIgnored = true;
}


#undef LOCTEXT_NAMESPACE
