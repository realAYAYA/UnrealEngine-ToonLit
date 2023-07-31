// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "HAL/UnrealMemory.h"
#include "IO/IoContainerId.h"
#include "IO/IoDispatcher.h"
#include "Logging/LogMacros.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/Guid.h"
#include "Misc/SecureHash.h"
#include "UObject/NameTypes.h"

CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogIoStore, Log, All);

/**
 * I/O store container format version
 */
enum class EIoStoreTocVersion : uint8
{
	Invalid = 0,
	Initial,
	DirectoryIndex,
	PartitionSize,
	PerfectHash,
	PerfectHashWithOverflow,
	LatestPlusOne,
	Latest = LatestPlusOne - 1
};

/**
 * I/O Store TOC header.
 */
struct FIoStoreTocHeader
{
	static constexpr char TocMagicImg[] = "-==--==--==--==-";

	uint8	TocMagic[16];
	uint8	Version;
	uint8	Reserved0 = 0;
	uint16	Reserved1 = 0;
	uint32	TocHeaderSize;
	uint32	TocEntryCount;
	uint32	TocCompressedBlockEntryCount;
	uint32	TocCompressedBlockEntrySize;	// For sanity checking
	uint32	CompressionMethodNameCount;
	uint32	CompressionMethodNameLength;
	uint32	CompressionBlockSize;
	uint32	DirectoryIndexSize;
	uint32	PartitionCount = 0;
	FIoContainerId ContainerId;
	FGuid	EncryptionKeyGuid;
	EIoContainerFlags ContainerFlags;
	uint8	Reserved3 = 0;
	uint16	Reserved4 = 0;
	uint32	TocChunkPerfectHashSeedsCount = 0;
	uint64	PartitionSize = 0;
	uint32	TocChunksWithoutPerfectHashCount = 0;
	uint32	Reserved7 = 0;
	uint64	Reserved8[5] = { 0 };

	void MakeMagic()
	{
		FMemory::Memcpy(TocMagic, TocMagicImg, sizeof TocMagic);
	}

	bool CheckMagic() const
	{
		return FMemory::Memcmp(TocMagic, TocMagicImg, sizeof TocMagic) == 0;
	}
};

/**
 * Combined offset and length.
 */
struct FIoOffsetAndLength
{
public:
	inline uint64 GetOffset() const
	{
		return OffsetAndLength[4]
			| (uint64(OffsetAndLength[3]) << 8)
			| (uint64(OffsetAndLength[2]) << 16)
			| (uint64(OffsetAndLength[1]) << 24)
			| (uint64(OffsetAndLength[0]) << 32)
			;
	}

	inline uint64 GetLength() const
	{
		return OffsetAndLength[9]
			| (uint64(OffsetAndLength[8]) << 8)
			| (uint64(OffsetAndLength[7]) << 16)
			| (uint64(OffsetAndLength[6]) << 24)
			| (uint64(OffsetAndLength[5]) << 32)
			;
	}

	inline void SetOffset(uint64 Offset)
	{
		OffsetAndLength[0] = uint8(Offset >> 32);
		OffsetAndLength[1] = uint8(Offset >> 24);
		OffsetAndLength[2] = uint8(Offset >> 16);
		OffsetAndLength[3] = uint8(Offset >>  8);
		OffsetAndLength[4] = uint8(Offset >>  0);
	}

	inline void SetLength(uint64 Length)
	{
		OffsetAndLength[5] = uint8(Length >> 32);
		OffsetAndLength[6] = uint8(Length >> 24);
		OffsetAndLength[7] = uint8(Length >> 16);
		OffsetAndLength[8] = uint8(Length >> 8);
		OffsetAndLength[9] = uint8(Length >> 0);
	}

private:
	// We use 5 bytes for offset and size, this is enough to represent
	// an offset and size of 1PB
	uint8 OffsetAndLength[5 + 5];
};

enum class FIoStoreTocEntryMetaFlags : uint8
{
	None,
	Compressed		= (1 << 0),
	MemoryMapped	= (1 << 1)
};

ENUM_CLASS_FLAGS(FIoStoreTocEntryMetaFlags);

/**
 * TOC entry meta data
 */
struct FIoStoreTocEntryMeta
{
	// Source data hash (i.e. not the on disk data)
	FIoChunkHash ChunkHash;
	FIoStoreTocEntryMetaFlags Flags;
};

/**
 * Compression block entry.
 */
struct FIoStoreTocCompressedBlockEntry
{
	static constexpr uint32 OffsetBits = 40;
	static constexpr uint64 OffsetMask = (1ull << OffsetBits) - 1ull;
	static constexpr uint32 SizeBits = 24;
	static constexpr uint32 SizeMask = (1 << SizeBits) - 1;
	static constexpr uint32 SizeShift = 8;

	inline uint64 GetOffset() const
	{
		const uint64* Offset = reinterpret_cast<const uint64*>(Data);
		return *Offset & OffsetMask;
	}

	inline void SetOffset(uint64 InOffset)
	{
		uint64* Offset = reinterpret_cast<uint64*>(Data);
		*Offset = InOffset & OffsetMask;
	}

	inline uint32 GetCompressedSize() const
	{
		const uint32* Size = reinterpret_cast<const uint32*>(Data) + 1;
		return (*Size >> SizeShift) & SizeMask;
	}

	inline void SetCompressedSize(uint32 InSize)
	{
		uint32* Size = reinterpret_cast<uint32*>(Data) + 1;
		*Size |= (uint32(InSize) << SizeShift);
	}

	inline uint32 GetUncompressedSize() const
	{
		const uint32* UncompressedSize = reinterpret_cast<const uint32*>(Data) + 2;
		return *UncompressedSize & SizeMask;
	}

	inline void SetUncompressedSize(uint32 InSize)
	{
		uint32* UncompressedSize = reinterpret_cast<uint32*>(Data) + 2;
		*UncompressedSize = InSize & SizeMask;
	}

	inline uint8 GetCompressionMethodIndex() const
	{
		const uint32* Index = reinterpret_cast<const uint32*>(Data) + 2;
		return static_cast<uint8>(*Index >> SizeBits);
	}

	inline void SetCompressionMethodIndex(uint8 InIndex)
	{
		uint32* Index = reinterpret_cast<uint32*>(Data) + 2;
		*Index |= uint32(InIndex) << SizeBits;
	}

private:
	/* 5 bytes offset, 3 bytes for size / uncompressed size and 1 byte for compresseion method. */
	uint8 Data[5 + 3 + 3 + 1];
};

/**
 * TOC resource read options.
 */
enum class EIoStoreTocReadOptions
{
	Default,
	ReadDirectoryIndex	= (1 << 0),
	ReadTocMeta			= (1 << 1),
	ReadAll				= ReadDirectoryIndex | ReadTocMeta
};
ENUM_CLASS_FLAGS(EIoStoreTocReadOptions);

/**
 * Container TOC data.
 */
struct FIoStoreTocResource
{
	enum { CompressionMethodNameLen = 32 };

	FIoStoreTocHeader Header;

	TArray<FIoChunkId> ChunkIds;

	TArray<FIoOffsetAndLength> ChunkOffsetLengths;

	TArray<int32> ChunkPerfectHashSeeds;

	TArray<int32> ChunkIndicesWithoutPerfectHash;

	TArray<FIoStoreTocCompressedBlockEntry> CompressionBlocks;

	TArray<FName> CompressionMethods;

	FSHAHash SignatureHash;
	
	TArray<FSHAHash> ChunkBlockSignatures;

	TArray<FIoStoreTocEntryMeta> ChunkMetas;

	TArray<uint8> DirectoryIndexBuffer;

	UE_NODISCARD CORE_API static FIoStatus Read(const TCHAR* TocFilePath, EIoStoreTocReadOptions ReadOptions, FIoStoreTocResource& OutTocResource);

	UE_NODISCARD static TIoStatusOr<uint64> Write(const TCHAR* TocFilePath, FIoStoreTocResource& TocResource, const FIoContainerSettings& ContainerSettings, const FIoStoreWriterSettings& WriterSettings);

	CORE_API static uint64 HashChunkIdWithSeed(int32 Seed, const FIoChunkId& ChunkId);
};
