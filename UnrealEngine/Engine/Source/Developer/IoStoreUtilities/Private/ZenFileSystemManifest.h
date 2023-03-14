// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IO/IoDispatcher.h"

class ITargetPlatform;
class FArchive;

class FZenFileSystemManifest
{
public:

	struct FManifestEntry
	{
		FString ServerPath;
		FString ClientPath;
		FIoChunkId FileChunkId;
	};
	
	FZenFileSystemManifest(const ITargetPlatform& InTargetPlatform, FString InCookDirectory);
	
	int32 Generate();

	const FManifestEntry& CreateManifestEntry(const FString& Filename);
	
	const FManifestEntry& AddManifestEntry(const FIoChunkId& FileChunkId, FString ServerPath, FString ClientPath);

	TArrayView<const FManifestEntry> ManifestEntries() const
	{
		return Entries;
	}

	bool Save(const TCHAR* Filename);

	int32 NumEntries() const
	{
		return Entries.Num();
	}

	const FString& ServerRootPath() const
	{
		return ServerRoot;
	}

private:
	static void GetExtensionDirs(TArray<FString>& OutExtensionDirs, const TCHAR* BaseDir, const TCHAR* SubDir, const TArray<FString>& PlatformDirectoryNames);

	const ITargetPlatform& TargetPlatform;
	FString CookDirectory;
	FString ServerRoot;
	TMap<FString, int32> ServerPathToEntry;
	TArray<FManifestEntry> Entries;
	
	static const FManifestEntry InvalidEntry;
};
