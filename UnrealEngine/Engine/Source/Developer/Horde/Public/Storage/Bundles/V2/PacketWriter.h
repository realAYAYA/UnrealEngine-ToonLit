// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Storage/BlobHandle.h"
#include "Packet.h"
#include "PacketHandle.h"
#include "Storage/ChunkedBufferWriter.h"

/**
 * Writes exports into a new bundle packet
 */
class HORDE_API FPacketWriter
{
public:
	FPacketWriter(FBlobHandle InBundleHandle, FBlobHandle InPacketHandle);
	FPacketWriter(const FPacketWriter&) = delete;
	~FPacketWriter();

	/** Current length of the packet */
	size_t GetLength() const;

	/** Adds an import to the current blob */
	void AddImport(FBlobHandle Import);

	/** Gets the number of unique imports current added to this packet */
	int GetImportCount() const;

	/** Gets a packet import by index */
	FBlobHandle GetImport(int ImportIdx) const;

	/** Gets data to write new export */
	FMutableMemoryView GetOutputBuffer(size_t UsedSize, size_t DesiredSize);

	/** Increase the length of the current blob */
	void Advance(size_t Size);

	/** Writes a new blob to this packet */
	int CompleteBlob(const FBlobType& Type);

	/** Gets the number of exports currently in this packet */
	int GetExportCount() const;

	/** Reads data for a blob written to storage */
	FBlob GetExport(int ExportIdx) const;

	/** Mark the current packet as complete */
	FPacket CompletePacket();

	FPacketWriter& operator=(const FPacketWriter&) = delete;

private:
	FBlobHandle BundleHandle;
	FBlobHandle PacketHandle;

	FSharedBuffer Buffer;
	uint8* BufferBasePtr;
	size_t Length;

	size_t NextBlobLength;
	TArray<int32> NextBlobImports;

	TArray<FBlobType> Types;
	TArray<uint32> ImportOffsets;
	FChunkedBufferWriter ImportWriter;
	TArray<FBlobHandle> ImportHandles;
	TMap<FBlobHandle, int> ImportMap;
	TArray<uint32> ExportOffsets;

	FMutableMemoryView GetOutputBufferInternal(size_t UsedSize, size_t DesiredSize);
	uint8* GetOutputSpanAndAdvance(size_t Length);

	int32 FindOrAddType(FBlobType Type);
	int32 FindOrAddImport(const FBlobHandle& Handle);

	void WriteTypeTable();
	void WriteImportTable();
	void WriteExportTable();

	static size_t Align(size_t Offset);
};
