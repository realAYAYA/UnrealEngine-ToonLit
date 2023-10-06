// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/GenericPlatformChunkInstall.h"
#include "HAL/PlatformMisc.h"

DEFINE_LOG_CATEGORY(LogChunkInstaller)



/*
 *  Functionality to to implement the deprecated 'FCustomChunk' API using the new 'named chunk' API
 */

PRAGMA_DISABLE_DEPRECATION_WARNINGS


bool FGenericPlatformChunkInstall_WithEmulatedCustomChunks::IsChunkInstallationPending(const TArray<FCustomChunk>& ChunkTagsID)
{
	for (const FCustomChunk& CustomChunk : ChunkTagsID)
	{
		if (IsNamedChunkInProgress(FName(CustomChunk.ChunkTag)) || IsNamedChunkInProgress(FName(CustomChunk.ChunkTag2)) )
		{
			return true;
		}
	}

	return false;
}

bool FGenericPlatformChunkInstall_WithEmulatedCustomChunks::InstallChunks(const TArray<FCustomChunk>& ChunkTagsID)
{
	for (const FCustomChunk& CustomChunk : ChunkTagsID)
	{
		if (!InstallNamedChunk(FName(CustomChunk.ChunkTag)) && !InstallNamedChunk(FName(CustomChunk.ChunkTag2)))
		{
			return false;
		}
	}

	return true;
}

bool FGenericPlatformChunkInstall_WithEmulatedCustomChunks::UninstallChunks(const TArray<FCustomChunk>& ChunkTagsID)
{
	for (const FCustomChunk& CustomChunk : ChunkTagsID)
	{
		if (!UninstallNamedChunk(FName(CustomChunk.ChunkTag)) && !UninstallNamedChunk(FName(CustomChunk.ChunkTag2)))
		{
			return false;
		}
	}

	return true;
}



PRAGMA_ENABLE_DEPRECATION_WARNINGS
