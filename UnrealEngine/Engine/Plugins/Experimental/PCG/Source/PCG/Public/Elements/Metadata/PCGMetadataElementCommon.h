// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGModule.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

struct FPCGTaggedData;

namespace PCGMetadataElementCommon
{
	void DuplicateTaggedData(const FPCGTaggedData& InTaggedData, FPCGTaggedData& OutTaggedData, UPCGMetadata*& OutMetadata);

	/** Copies the entry to value key relationship stored in the given Metadata, including its parents */
	void CopyEntryToValueKeyMap(const UPCGMetadata* MetadataToCopy, const FPCGMetadataAttributeBase* AttributeToCopy, FPCGMetadataAttributeBase* OutAttribute);

	/** Creates a new attribute, or clears the attribute if it already exists and is a 'T' type */
	template<typename T>
	FPCGMetadataAttribute<T>* ClearOrCreateAttribute(UPCGMetadata* Metadata, const FName& DestinationAttribute, T DefaultValue)
	{
		if (!Metadata)
		{
			UE_LOG(LogPCG, Error, TEXT("Failed to create metadata"));
			return nullptr;
		}

		if (Metadata->HasAttribute(DestinationAttribute))
		{
			UE_LOG(LogPCG, Warning, TEXT("Attribute %s already exists and has been overwritten"), *DestinationAttribute.ToString());
			Metadata->DeleteAttribute(DestinationAttribute);
		}

		Metadata->CreateAttribute<T>(DestinationAttribute, DefaultValue, /*bAllowsInterpolation=*/true, /*bOverrideParent=*/false);

		return static_cast<FPCGMetadataAttribute<T>*>(Metadata->GetMutableAttribute(DestinationAttribute));
	}
}