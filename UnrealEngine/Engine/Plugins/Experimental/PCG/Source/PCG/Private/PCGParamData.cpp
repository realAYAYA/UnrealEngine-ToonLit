// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGParamData.h"

UPCGParamData::UPCGParamData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Metadata = ObjectInitializer.CreateDefaultSubobject<UPCGMetadata>(this, TEXT("Metadata"));
}

int64 UPCGParamData::FindMetadataKey(const FName& InName) const
{
	if (const PCGMetadataEntryKey* FoundKey = NameMap.Find(InName))
	{
		return *FoundKey;
	}
	else
	{
		return PCGInvalidEntryKey;
	}
}

int64 UPCGParamData::FindOrAddMetadataKey(const FName& InName)
{
	if (const PCGMetadataEntryKey* FoundKey = NameMap.Find(InName))
	{
		return *FoundKey;
	}
	else
	{
		check(Metadata);
		PCGMetadataEntryKey NewKey = Metadata->AddEntry();
		NameMap.Add(InName, NewKey);
		return NewKey;
	}
}

UPCGParamData* UPCGParamData::FilterParamsByName(const FName& InName) const
{
	PCGMetadataEntryKey EntryKey = FindMetadataKey(InName);
	UPCGParamData* NewParams = FilterParamsByKey(EntryKey);

	if (EntryKey != PCGInvalidEntryKey)
	{
		// NOTE: this relies on the fact that there will be only one entry
		NewParams->NameMap.Add(InName, 0);
	}

	return NewParams;
}

UPCGParamData* UPCGParamData::FilterParamsByKey(int64 InKey) const
{
	UPCGParamData* NewParams = NewObject<UPCGParamData>();

	// Here instead of parenting the metadata, we will create a copy
	// so that the only entry in the metadata (if any) will have the 0 key.
	check(NewParams && NewParams->Metadata);

	NewParams->Metadata->AddAttributes(Metadata);

	if (InKey != PCGInvalidEntryKey)
	{
		PCGMetadataEntryKey OutKey = PCGInvalidEntryKey;
		NewParams->Metadata->SetAttributes(InKey, Metadata, OutKey);
	}

	return NewParams;
}