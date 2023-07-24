// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "Containers/Array.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "Templates/UniquePtr.h"

class FOutputDevice;
class IFileHandle;

/** Helper class for reading an uncompressed zip archive file. */
class FILEUTILITIES_API FZipArchiveReader
{
	class FImpl;
	TUniquePtr<FImpl> Impl;

public:
	/**
	 * Constructs the ZipArchive to read from a FileHandle. Takes ownership of the FileHandle
	 * (even if it is corrupt); it will be closed by the destructor.
	 */
	FZipArchiveReader(IFileHandle* InFileHandle, FOutputDevice* ErrorHandler = nullptr);
	~FZipArchiveReader();

	bool IsValid() const;

	TArray<FString> GetFileNames() const;
	bool TryReadFile(FStringView FileName, TArray<uint8>& OutData, FOutputDevice* ErrorHandler = nullptr) const;
};

#endif