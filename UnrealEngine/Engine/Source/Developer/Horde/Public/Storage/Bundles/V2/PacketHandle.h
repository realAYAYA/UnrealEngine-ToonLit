// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "Storage/Blob.h"
#include "Storage/BlobHandle.h"
#include "Storage/BlobType.h"
#include "Storage/StorageClient.h"

class FPacketReader;
class FPacketHandleData;

// Handle to an packet within a bundle. 
typedef TBlobHandle<FPacketHandleData> FPacketHandle;

// Data for FPacketHandle
class HORDE_API FPacketHandleData final : public FBlobHandleData, public TSharedFromThis<FPacketHandleData, ESPMode::ThreadSafe>
{
public:
	static const char Type[];

	FPacketHandleData(TSharedRef<FStorageClient> InStorageClient, FBlobHandle InBundle, size_t InPacketOffset, size_t InPacketLength);
	FPacketHandleData(TSharedRef<FStorageClient> InStorageClient, FBlobHandle InBundle, const FUtf8StringView& InFragment);
	~FPacketHandleData();

	/** Reads an export from this packet. */
	FBlob ReadExport(size_t ExportIdx) const;

	/** Reads an export body from this packet. */
	FSharedBufferView ReadExportBody(size_t ExportIdx) const;

	// Implementation of FBlobHandle
	virtual bool Equals(const FBlobHandleData& Other) const override;
	virtual uint32 GetHashCode() const override;
	virtual FBlobHandle GetOuter() const override;
	virtual const char* GetType() const override;
	virtual FBlob Read() const override;
	virtual bool TryAppendIdentifier(FUtf8String& OutBuffer) const override;
	virtual FBlobHandle GetFragmentHandle(const FUtf8StringView& Fragment) const override;

private:
	static const char FragmentPrefix[];
	static const size_t FragmentPrefixLength;

	TSharedRef<FStorageClient> StorageClient;

	FBlobHandle Bundle;
	size_t PacketOffset;
	size_t PacketLength;

	mutable TSharedPtr<FPacketReader> PacketReader;

	const FPacketReader& GetPacketReader() const;
	static bool TryParse(const FUtf8StringView& Fragment, size_t& OutPacketOffset, size_t& OutPacketLength);
};
