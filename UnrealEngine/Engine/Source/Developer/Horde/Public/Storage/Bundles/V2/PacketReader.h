// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Storage/StorageClient.h"
#include "Packet.h"
#include "PacketHandle.h"

/**
 * Utility class for constructing BlobData objects from a packet, caching any computed handles to other blobs.
 */
class HORDE_API FPacketReader
{
public:
	FPacketReader(TSharedRef<FStorageClient> InStorageClient, FBlobHandle InBundleHandle, FPacketHandle InPacketHandle, FPacket InPacket);
	~FPacketReader();

	/** Read a blob object for the entire packet. */
	FBlob ReadPacket() const;

	/** Reads an export from this packet. */
	FBlob ReadExport(size_t ExportIdx) const;

	/** Reads an export body from this packet. */
	FSharedBufferView ReadExportBody(size_t ExportIdx) const;

private:
	TSharedRef<FStorageClient> StorageClient;
	FBlobHandle BundleHandle;
	FPacketHandle PacketHandle;
	FPacket Packet;
	mutable TArray<FBlobHandle> CachedImportHandles;

	FBlobHandle GetImportHandle(size_t Index) const;
};
