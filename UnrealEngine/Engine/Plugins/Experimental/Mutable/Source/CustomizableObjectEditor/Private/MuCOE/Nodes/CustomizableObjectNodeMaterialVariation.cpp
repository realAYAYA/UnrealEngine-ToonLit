// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeMaterialVariation.h"

#include "MuCOE/CustomizableObjectEditor_Deprecated.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"


void UCustomizableObjectNodeMaterialVariation::BackwardsCompatibleFixup()
{
	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::NodeVariationSerializationIssue)
	{
		for (const FCustomizableObjectMaterialVariation& OldVariation : Variations_DEPRECATED)
		{
			FCustomizableObjectVariation Variation;
			Variation.Tag = OldVariation.Tag;
			
			VariationsData.Add(Variation);
		}
	}

	Super::BackwardsCompatibleFixup();
}


FName UCustomizableObjectNodeMaterialVariation::GetCategory() const
{
	return UEdGraphSchema_CustomizableObject::PC_Material;
}


bool UCustomizableObjectNodeMaterialVariation::IsInputPinArray() const
{
	return true;
}


bool UCustomizableObjectNodeMaterialVariation::IsSingleOutputNode() const
{
	return true;
}
