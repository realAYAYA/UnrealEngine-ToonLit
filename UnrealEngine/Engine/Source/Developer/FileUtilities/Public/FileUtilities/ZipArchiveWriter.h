// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ContainersFwd.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/Platform.h"
#include "HAL/UnrealMemory.h"
#include "Misc/DateTime.h"

#if WITH_ENGINE

class IFileHandle;

/** Helper class for generating an uncompressed zip archive file. */
class FILEUTILITIES_API FZipArchiveWriter
{
	struct FFileEntry
	{
		FString Filename;
		uint32 Crc32;
		uint64 Length;
		uint64 Offset;
		uint32 Time;

		FFileEntry(const FString& InFilename, uint32 InCrc32, uint64 InLength, uint64 InOffset, uint32 InTime)
			: Filename(InFilename)
			, Crc32(InCrc32)
			, Length(InLength)
			, Offset(InOffset)
			, Time(InTime)
		{}
	};

	TArray<FFileEntry> Files;

	TArray<uint8> Buffer;
	IFileHandle* File;

	inline void Write(uint16 V) { Write((void*)&V, sizeof(V)); }
	inline void Write(uint32 V) { Write((void*)&V, sizeof(V)); }
	inline void Write(uint64 V) { Write((void*)&V, sizeof(V)); }
	inline void Write(void* Src, uint64 Size)
	{
		if (Size)
		{
			void* Dst = &Buffer[Buffer.AddUninitialized(Size)];
			FMemory::Memcpy(Dst, Src, Size);
		}
	}
	inline uint64 Tell() { return (File ? File->Tell() : 0) + Buffer.Num(); }
	void Flush();

public:
	FZipArchiveWriter(IFileHandle* InFile);
	~FZipArchiveWriter();

	void AddFile(const FString& Filename, TConstArrayView<uint8> Data, const FDateTime& Timestamp);
	void AddFile(const FString& Filename, const TArray<uint8>& Data, const FDateTime& Timestamp);
};

#endif