// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstancePackers/PCGInstancePackerByRegex.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Internationalization/Regex.h"

void UPCGInstancePackerByRegex::PackInstances_Implementation(FPCGContext& Context, const UPCGSpatialData* InSpatialData, const FPCGMeshInstanceList& InstanceList, FPCGPackedCustomData& OutPackedCustomData) const
{
	if (!InSpatialData || !InSpatialData->Metadata)
	{
		PCGE_LOG_C(Error, &Context, "Invalid input data");
		return;
	}
	
	TArray<FName> AttributeNames;
	TArray<EPCGMetadataTypes> AttributeTypes;
	InSpatialData->Metadata->GetAttributes(AttributeNames, AttributeTypes);

	TArray<const FPCGMetadataAttributeBase*> SelectedAttributes;
	TSet<PCGMetadataAttributeKey> FoundAttributes;

	for (const FString& Regex : RegexPatterns)
	{
		const FRegexPattern Pattern(Regex);
	
		// Match our Regex Pattern against every Attribute
		for (int Index = 0; Index < AttributeNames.Num(); ++Index)
		{
			const FName& AttributeName = AttributeNames[Index];
			FRegexMatcher RegexMatcher(Pattern, AttributeName.ToString());
	
			if (!RegexMatcher.FindNext()) 
			{
				continue;
			}
	
			const FPCGMetadataAttributeBase* AttributeBase = InSpatialData->Metadata->GetConstAttribute(AttributeName);
			check(AttributeBase);

			// Avoid adding the same attribute multiple times if it is captured by different regex patterns
			if (FoundAttributes.Contains(AttributeBase->AttributeId))
			{
				continue;
			}

			FoundAttributes.Add(AttributeBase->AttributeId);
	
			if (!AddTypeToPacking(AttributeBase->GetTypeId(), OutPackedCustomData))
			{
				PCGE_LOG_C(Warning, &Context, "Attribute name %s is not a valid type", *AttributeName.ToString());
				continue;
			}
	
			SelectedAttributes.Add(AttributeBase);
		}
	}
	
	PackCustomDataFromAttributes(InstanceList, SelectedAttributes, OutPackedCustomData);
}
