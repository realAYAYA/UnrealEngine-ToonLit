// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
 
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Internationalization/InternationalizationMetadata.h"
#include "Serialization/StructuredArchive.h"

class FArchive;

struct FTextSourceSiteContext
{
	FTextSourceSiteContext()
		: IsEditorOnly(false)
		, IsOptional(false)
	{
	}

	FString KeyName;
	FString SiteDescription;
	bool IsEditorOnly;
	bool IsOptional;
	FLocMetadataObject InfoMetaData;
	FLocMetadataObject KeyMetaData;

	CORE_API friend FArchive& operator<<(FArchive& Archive, FTextSourceSiteContext& This);
	CORE_API friend void operator<<(FStructuredArchive::FSlot Slot, FTextSourceSiteContext& This);
};

struct FTextSourceData
{
	FString SourceString;
	FLocMetadataObject SourceStringMetaData;
};

CORE_API FArchive& operator<<(FArchive& Archive, FTextSourceData& This);
CORE_API void operator<<(FStructuredArchive::FSlot Slot, FTextSourceData& This);

struct FGatherableTextData
{
	FString NamespaceName;
	FTextSourceData SourceData;

	TArray<FTextSourceSiteContext> SourceSiteContexts;

	CORE_API friend FArchive& operator<<(FArchive& Archive, FGatherableTextData& This);
	CORE_API friend void operator<<(FStructuredArchive::FSlot Slot, FGatherableTextData& This);
};
