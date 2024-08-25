// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FileEntry.h"
#include "DirectoryEntry.h"
#include "../BlobType.h"
#include "../StorageClient.h"

/**
 * Flags for a directory node
 */
enum class EDirectoryFlags
{
	/** No flags specified. */
	None = 0,
};

/**
 * Stores the contents of a directory in a blob
 */
class HORDE_API FDirectoryNode
{
public:
	static const FBlobType BlobType;

	EDirectoryFlags Flags;
	TMap<FUtf8String, FFileEntry> NameToFile;
	TMap<FUtf8String, FDirectoryEntry> NameToDirectory;

	FDirectoryNode(EDirectoryFlags InFlags = EDirectoryFlags::None);
	~FDirectoryNode();

	static FDirectoryNode Read(const FBlob& Blob);
	FBlobHandle Write(FBlobWriter& Writer) const;
};
