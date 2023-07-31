// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Metadata/PCGMetadataElementCommon.h"

#include "PCGParamData.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGSettingsHelpers.h"

namespace PCGMetadataElementCommon
{
	void DuplicateTaggedData(const FPCGTaggedData& InTaggedData, FPCGTaggedData& OutTaggedData, UPCGMetadata*& OutMetadata)
	{
		if (const UPCGSpatialData* SpatialInput = Cast<const UPCGSpatialData>(InTaggedData.Data))
		{
			UPCGSpatialData* NewSpatialData = Cast<UPCGSpatialData>(StaticDuplicateObject(SpatialInput, const_cast<UPCGSpatialData*>(SpatialInput), FName()));
			NewSpatialData->InitializeFromData(SpatialInput);
			OutTaggedData.Data = NewSpatialData;

			OutMetadata = NewSpatialData->Metadata;
		}
		else if (const UPCGParamData* ParamsInput = Cast<const UPCGParamData>(InTaggedData.Data))
		{
			UPCGParamData* NewParamData = Cast<UPCGParamData>(StaticDuplicateObject(ParamsInput, const_cast<UPCGParamData*>(ParamsInput), FName()));
			OutTaggedData.Data = NewParamData;

			NewParamData->Metadata->Initialize(ParamsInput->Metadata);
			OutMetadata = NewParamData->Metadata;
		}
	}

	void CopyEntryToValueKeyMap(const UPCGMetadata* MetadataToCopy, const FPCGMetadataAttributeBase* AttributeToCopy, FPCGMetadataAttributeBase* OutAttribute)
	{
		if (!OutAttribute)
		{
			UE_LOG(LogPCG, Error, TEXT("Failed to create output attribute"));
			return;
		}

		const PCGMetadataEntryKey EntryKeyCount = MetadataToCopy->GetItemCountForChild();
		for (PCGMetadataEntryKey EntryKey = 0; EntryKey < EntryKeyCount; ++EntryKey)
		{
			const PCGMetadataValueKey ValueKey = AttributeToCopy->GetValueKey(EntryKey);
			OutAttribute->SetValueFromValueKey(EntryKey, ValueKey);
		}
	}
}