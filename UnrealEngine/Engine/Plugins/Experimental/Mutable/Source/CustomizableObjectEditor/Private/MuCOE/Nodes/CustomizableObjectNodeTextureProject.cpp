// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeTextureProject.h"

#include "Containers/EnumAsByte.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphPin.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "Serialization/Archive.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealType.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeTextureProject::UCustomizableObjectNodeTextureProject()
	: Super()
{
	Textures = 1;
}


void UCustomizableObjectNodeTextureProject::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if ( PropertyThatChanged && PropertyThatChanged->GetName() == TEXT("Textures") )
	{
		ReconstructNode();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UCustomizableObjectNodeTextureProject::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (Ar.CustomVer(FCustomizableObjectCustomVersion::GUID) < FCustomizableObjectCustomVersion::PinsNamesImageToTexture)
	{
		{
			uint32 TexturePinsCount = 0;
			uint32 OutputPinsCount = 0;

			TArray<UEdGraphPin*> NonOrphanPins = GetAllNonOrphanPins();
			
			for (const UEdGraphPin* Pin : NonOrphanPins)
			{
				if (Pin->GetName().StartsWith(TEXT("Image ")) && Pin->Direction == EGPD_Input)
				{
					TexturePinsCount++;
				}
				else if (Pin->GetName().StartsWith(TEXT("Image ")) && Pin->Direction == EGPD_Output)
				{
					OutputPinsCount++;
				}
			}

			TexturePinsReferences.SetNum(OutputPinsCount);
			for (uint32 Index = 0; Index < TexturePinsCount; ++Index)
			{
				TexturePinsReferences.Add(FEdGraphPinReference(FindPin(FString::Printf(TEXT("Image %d "), Index), EGPD_Input)));
			}
			
			OutputPinsReferences.SetNum(TexturePinsCount);
			for (uint32 Index = 0; Index < TexturePinsCount; ++Index)
			{
				OutputPinsReferences.Add(FEdGraphPinReference(FindPin(FString::Printf(TEXT("Image %d "), Index), EGPD_Output)));
			}
		}
	}
}


void UCustomizableObjectNodeTextureProject::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	TexturePinsReferences.Reset(0);
	OutputPinsReferences.Reset(0);
	
	for ( int LayerIndex = 0; LayerIndex < Textures; ++LayerIndex )
	{
		FString PinName = FString::Printf(TEXT("Texture %d"), LayerIndex);
		UEdGraphPin* TexturePin = CustomCreatePin(EGPD_Input, Schema->PC_Image, FName(*PinName));
		TexturePin->bDefaultValueIsIgnored = true;

		TexturePinsReferences.Add(FEdGraphPinReference(TexturePin));
		
		PinName = FString::Printf(TEXT("Texture %d"), LayerIndex);
		UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Image, FName(*PinName));
		OutputPin->bDefaultValueIsIgnored = true;

		OutputPinsReferences.Add(FEdGraphPinReference(OutputPin));
	}

	FString PinName = TEXT("Mesh");
	UEdGraphPin* MeshPin = CustomCreatePin(EGPD_Input, Schema->PC_Mesh, FName(*PinName));
	MeshPin->bDefaultValueIsIgnored = true;

	PinName = TEXT("Mesh Mask");
	UEdGraphPin* MeshMaskPin = CustomCreatePin(EGPD_Input, Schema->PC_Image, FName(*PinName));
	MeshMaskPin->bDefaultValueIsIgnored = true;

	PinName = TEXT("Fade Start Angle");
	UEdGraphPin* AngleFadeStartPin = CustomCreatePin(EGPD_Input, Schema->PC_Float, FName(*PinName));
	AngleFadeStartPin->bDefaultValueIsIgnored = false;
	AngleFadeStartPin->DefaultValue = "90";

	PinName = TEXT("Fade End Angle");
	UEdGraphPin* AngleFadeEndPin = CustomCreatePin(EGPD_Input, Schema->PC_Float, FName(*PinName));
	AngleFadeEndPin->bDefaultValueIsIgnored = false;
	AngleFadeEndPin->DefaultValue = "95";

	PinName = TEXT("Projector");
	UEdGraphPin* ProjectorPin = CustomCreatePin(EGPD_Input, Schema->PC_Projector, FName(*PinName));
	ProjectorPin->bDefaultValueIsIgnored = true;
}


UEdGraphPin* UCustomizableObjectNodeTextureProject::TexturePins(int32 Index) const
{
	return TexturePinsReferences[Index].Get();	
}


UEdGraphPin* UCustomizableObjectNodeTextureProject::OutputPins(int32 Index) const
{
	return OutputPinsReferences[Index].Get();
}


int32 UCustomizableObjectNodeTextureProject::GetNumTextures() const
{
	return TexturePinsReferences.Num();
}

int32 UCustomizableObjectNodeTextureProject::GetNumOutputs() const
{
	return OutputPinsReferences.Num();
}


FText UCustomizableObjectNodeTextureProject::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Texture_Project", "Texture Project");
}


FLinearColor UCustomizableObjectNodeTextureProject::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Image);
}


FText UCustomizableObjectNodeTextureProject::GetTooltipText() const
{
	return LOCTEXT("Texture_Project_Tooltip",
	"Projects one or more textures on a mesh. Transforms a flat image to a layed out version usable as texture for the input mesh. This node does not modify the mesh section or material.");
}


#undef LOCTEXT_NAMESPACE

