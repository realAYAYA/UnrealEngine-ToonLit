// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Utf8String.h"
#include "../BlobHandle.h"
#include "IO/IoHash.h"

class FBlobReader;
class FBlobWriter;

/**
 * Entry for a directory within a directory node
 */
class HORDE_API FDirectoryEntry
{
public:
	FBlobHandle Target;
	FIoHash TargetHash;

	/** Total size of this directory's contents. */
	int64 Length;

	/** Name of this directory. */
	const FUtf8String Name;

	FDirectoryEntry(FBlobHandle InTarget, const FIoHash& InTargetHash, FUtf8String InName, int64 InLength);
	~FDirectoryEntry();

	static FDirectoryEntry Read(FBlobReader& Reader);
	void Write(FBlobWriter& Writer) const;
};
