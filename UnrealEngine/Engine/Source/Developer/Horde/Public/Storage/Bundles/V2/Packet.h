// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Utf8String.h"
#include "Storage/BlobType.h"
#include "../BundleCompression.h"
#include "../../../SharedBufferView.h"

class FChunkedBufferWriter;
struct FPacketImport;
struct FPacketExport;

/**
 * Accessor for data structures stored into a serialized bundle packet.
 */
class HORDE_API FPacket
{
public:
	static const FBlobType BlobType;

	FPacket(FSharedBufferView InBuffer);
	virtual ~FPacket();

	/** Gets the underlying buffer for this packet. */
	FSharedBufferView GetBuffer() const;

	/** Length of this packet. */
	size_t GetLength() const;

	/** Gets the number of types in this packet. */
	size_t GetTypeCount() const;

	/** Gets a type from the packet. */
	const FBlobType& GetType(size_t TypeIdx) const;

	/** Gets the number of imports in this packet. */
	size_t GetImportCount() const;

	/** Gets the locator for a particular import. */
	FPacketImport GetImport(size_t ImportIdx) const;

	/** Gets the number of exports in this packet. */
	size_t GetExportCount() const;

	/** Gets the bulk data for a particular export. */
	FPacketExport GetExport(size_t ExportIdx) const;

	/** Encodes a packet. */
	void Encode(EBundleCompressionFormat Format, FChunkedBufferWriter& Writer);

	/** Decodes a packet from the given data. */
	static FPacket Decode(const FMemoryView& View);

private:
	struct FEncodedPacketHeader;

	FSharedBufferView Buffer;
};

/**
 * Specifies the path to an imported node
 */
struct HORDE_API FPacketImport
{
	static constexpr int32 Bias = 3;
	static constexpr int32 InvalidBaseIdx = -1;
	static constexpr int32 CurrentPacketBaseIdx = -2;
	static constexpr int32 CurrentBundleBaseIdx = -3;

	FPacketImport(const FMemoryView& InView);
	FPacketImport(int32 InBaseIdx, const FUtf8StringView& InFragment);

	/** Gets the index of the base packet import. */
	int GetBaseIdx() const;

	/** Gets the fragment for this import. */
	FUtf8StringView GetFragment() const;

private:
	int32 BaseIdx;
	FUtf8StringView Fragment;
};

/*
 * Data for an exported node in a packet
 */
struct HORDE_API FPacketExport
{
	FPacketExport(FSharedBufferView InBuffer);
	~FPacketExport();

	/** Gets the index of this export's type. */
	size_t GetTypeIndex() const;

	/** Gets the import indexes for this export. */
	void GetImportIndices(TArray<size_t>& OutIndices) const;

	/** Gets the payload for this export. */
	FSharedBufferView GetPayload() const;

private:
	FSharedBufferView Buffer;
	int32 TypeIdx;
	FMemoryView Imports;
};
