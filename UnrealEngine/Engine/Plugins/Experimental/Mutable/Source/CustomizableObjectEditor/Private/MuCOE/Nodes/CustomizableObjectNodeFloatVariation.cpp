// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeFloatVariation.h"

#include "MuCOE/CustomizableObjectEditor_Deprecated.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"


void UCustomizableObjectNodeFloatVariation::BackwardsCompatibleFixup()
{
	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::NodeVariationSerializationIssue)
	{
		for (const FCustomizableObjectFloatVariation& OldVariation : Variations_DEPRECATED)
		{
			FCustomizableObjectVariation Variation;
			Variation.Tag = OldVariation.Tag;
			
			VariationsData.Add(Variation);
		}
	}

	Super::BackwardsCompatibleFixup();
}


FName UCustomizableObjectNodeFloatVariation::GetCategory() const
{
	return UEdGraphSchema_CustomizableObject::PC_Float;
}
