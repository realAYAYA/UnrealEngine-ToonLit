// Copyright Epic Games, Inc. All Rights Reserved.

#include "CookMetadata.h"

#include "Hash/xxhash.h"
#include "Memory/MemoryView.h"

namespace UE::Cook
{

const FString& GetCookMetadataFilename()
{
	static const FString CookMetadataFilename(TEXT("CookMetadata.ucookmeta"));
	return CookMetadataFilename;
}

constexpr uint32 COOK_METADATA_HEADER_MAGIC = 'UCMT';

bool FCookMetadataState::Serialize(FArchive& Ar)
{
	uint32 MagicHeader = 0;
	if (Ar.IsLoading())
	{
		Ar << MagicHeader;
		if (MagicHeader != COOK_METADATA_HEADER_MAGIC)
		{
			return false; // not a metadata file.
		}
	}
	else
	{
		MagicHeader = COOK_METADATA_HEADER_MAGIC;
		Ar << MagicHeader;
	}
	Ar << Version;
	Ar << PluginHierarchy;
	Ar << AssociatedDevelopmentAssetRegistryHash;

	return true;
}

/* static */ 
uint64 FCookMetadataState::ComputeHashOfDevelopmentAssetRegistry(FMemoryView InSerializedDevelopmentAssetRegistry)
{
	return FXxHash64::HashBufferChunked(InSerializedDevelopmentAssetRegistry.GetData(), InSerializedDevelopmentAssetRegistry.GetSize(), 1ULL << 19).Hash;
}


} // end namespace