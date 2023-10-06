// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstanceDataPackers/PCGInstanceDataPackerByAttribute.h"

#include "PCGContext.h"
#include "PCGElement.h"
#include "Data/PCGSpatialData.h"
#include "InstanceDataPackers/PCGInstanceDataPackerBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGInstanceDataPackerByAttribute)

#define LOCTEXT_NAMESPACE "PCGInstanceDataPackerByAttribute"

void UPCGInstanceDataPackerByAttribute::PackInstances_Implementation(FPCGContext& Context, const UPCGSpatialData* InSpatialData, const FPCGMeshInstanceList& InstanceList, FPCGPackedCustomData& OutPackedCustomData) const
{
	if (!InSpatialData || !InSpatialData->Metadata)
	{
		PCGE_LOG_C(Error, GraphAndLog, &Context, LOCTEXT("InvalidInputData", "Invalid input data"));
		return;
	}

	TArray<const FPCGMetadataAttributeBase*> SelectedAttributes;

	// Find Attributes by name and calculate NumCustomDataFloats
	for (const FName& AttributeName : AttributeNames)
	{
		if (!InSpatialData->Metadata->HasAttribute(AttributeName)) 
		{
			PCGE_LOG_C(Warning, GraphAndLog, &Context, FText::Format(LOCTEXT("AttributeNotInMetadata", "Attribute '{0}' is not in the metadata"), FText::FromName(AttributeName)));
			continue;
		}

		const FPCGMetadataAttributeBase* AttributeBase = InSpatialData->Metadata->GetConstAttribute(AttributeName);
		check(AttributeBase);

		if (!AddTypeToPacking(AttributeBase->GetTypeId(), OutPackedCustomData))
		{
			PCGE_LOG_C(Warning, GraphAndLog, &Context, FText::Format(LOCTEXT("AttributeInvalidType", "Attribute name '{0}' is not a valid type"), FText::FromName(AttributeName)));
			continue;
		}

		SelectedAttributes.Add(AttributeBase);
	}

	PackCustomDataFromAttributes(InstanceList, SelectedAttributes, OutPackedCustomData);
 }

#undef LOCTEXT_NAMESPACE
