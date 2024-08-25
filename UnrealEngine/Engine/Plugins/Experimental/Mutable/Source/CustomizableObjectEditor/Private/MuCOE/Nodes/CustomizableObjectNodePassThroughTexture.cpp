// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodePassThroughTexture.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodePassThroughTexture::BackwardsCompatibleFixup()
{
	Super::BackwardsCompatibleFixup();

	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::AddedAnyTextureTypeToPassThroughTextures)
	{
		if (Texture)
		{
			if (!PassThroughTexture)
			{
				PassThroughTexture = Texture;
			}

			Texture = nullptr;
		}
	}
}


void UCustomizableObjectNodePassThroughTexture::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	FString PinName = TEXT("Texture");
	UEdGraphPin* PinImagePin = CustomCreatePin(EGPD_Output, Schema->PC_PassThroughImage, FName(*PinName));
	PinImagePin->bDefaultValueIsIgnored = true;
}


FText UCustomizableObjectNodePassThroughTexture::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (Texture)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("TextureName"), FText::FromString(Texture->GetName()));

		return FText::Format(LOCTEXT("PassThrough Texture_Title", "{TextureName}\nPassThrough Texture"), Args);
	}
	else
	{
		return LOCTEXT("PassThrough Texture", "PassThrough Texture");
	}
}


FLinearColor UCustomizableObjectNodePassThroughTexture::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_PassThroughImage);
}


FText UCustomizableObjectNodePassThroughTexture::GetTooltipText() const
{
	return LOCTEXT("PassThrough_Texture_Tooltip", "Defines a pass-through texture. It will not be modified by Mutable in any way, just referenced as a UE asset. It's much cheaper than a Mutable texture, but you cannot make any operations on it, just switch it.");
}

#undef LOCTEXT_NAMESPACE
