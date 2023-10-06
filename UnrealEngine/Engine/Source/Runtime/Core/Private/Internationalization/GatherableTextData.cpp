// Copyright Epic Games, Inc. All Rights Reserved.

#include "Internationalization/GatherableTextData.h"

#include "CoreTypes.h"
#include "Serialization/StructuredArchive.h"
#include "Serialization/StructuredArchiveAdapters.h"
#include "Serialization/StructuredArchiveNameHelpers.h"
#include "Serialization/StructuredArchiveSlots.h"

class FArchive;

FArchive& operator<<(FArchive& Archive, FTextSourceSiteContext& This)
{
	FStructuredArchiveFromArchive(Archive).GetSlot() << This;
	return Archive;
}

void operator<<(FStructuredArchive::FSlot Slot, FTextSourceSiteContext& This)
{
	FStructuredArchive::FRecord Record = Slot.EnterRecord();
	Record << SA_VALUE(TEXT("KeyName"), This.KeyName);
	Record << SA_VALUE(TEXT("SiteDescription"), This.SiteDescription);
	Record << SA_VALUE(TEXT("IsEditorOnly"), This.IsEditorOnly);
	Record << SA_VALUE(TEXT("IsOptional"), This.IsOptional);
	Record << SA_VALUE(TEXT("InfoMetaData"), This.InfoMetaData);
	Record << SA_VALUE(TEXT("KeyMetaData"), This.KeyMetaData);
}

FArchive& operator<<(FArchive& Archive, FTextSourceData& This)
{
	FStructuredArchiveFromArchive(Archive).GetSlot() << This;
	return Archive;
}

void operator<<(FStructuredArchive::FSlot Slot, FTextSourceData& This)
{
	FStructuredArchive::FRecord Record = Slot.EnterRecord();
	Record << SA_VALUE(TEXT("SourceString"), This.SourceString);
	Record << SA_VALUE(TEXT("SourceStringMetaData"), This.SourceStringMetaData);
}

FArchive& operator<<(FArchive& Archive, FGatherableTextData& This)
{
	FStructuredArchiveFromArchive(Archive).GetSlot() << This;
	return Archive;
}

void operator<<(FStructuredArchive::FSlot Slot, FGatherableTextData& This)
{
	FStructuredArchive::FRecord Record = Slot.EnterRecord();
	Record << SA_VALUE(TEXT("NamespaceName"), This.NamespaceName);
	Record << SA_VALUE(TEXT("SourceData"), This.SourceData);
	Record << SA_VALUE(TEXT("SourceSiteContexts"), This.SourceSiteContexts);
}