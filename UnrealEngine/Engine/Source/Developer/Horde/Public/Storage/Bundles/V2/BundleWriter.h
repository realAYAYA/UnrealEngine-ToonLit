// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Utf8String.h"
#include "Storage/StorageClient.h"
#include "Storage/BlobWriter.h"
#include "Storage/ChunkedBufferWriter.h"
#include "Storage/Clients/BundleStorageClient.h"
#include "Storage/Clients/KeyValueStorageClient.h"

class FPacketWriter;

/**
 * Writes blobs into bundles
 */
class HORDE_API FBundleWriter final : public FBlobWriter
{
public:
	FBundleWriter(TSharedRef<FKeyValueStorageClient> InStorageClient, FUtf8String InBasePath, const FBundleOptions& InOptions);
	~FBundleWriter();

	// Inherited from FBlobWriter
	void Flush() override;
	TUniquePtr<FBlobWriter> Fork() override;
	virtual void AddAlias(const FAliasInfo& AliasInfo) override;
	virtual void AddImport(FBlobHandle Target) override;
	virtual void AddRef(const FRefName& RefName, const FRefOptions& Options) override;
	virtual FBlobHandle CompleteBlob(const FBlobType& InType) override;
	virtual FMutableMemoryView GetOutputBufferAsSpan(size_t UsedSize, size_t DesiredSize) override;
	virtual void Advance(size_t Size) override;

private:
	class FPendingExportHandleData;
	class FPendingPacketHandleData;
	class FPendingBundleHandleData;

	TSharedRef<FKeyValueStorageClient> StorageClient;
	FUtf8String BasePath;
	FBundleOptions Options;

	TBlobHandle<FPendingPacketHandleData> CurrentPacketHandle;
	TUniquePtr<FPacketWriter> PacketWriter;

	TBlobHandle<FPendingBundleHandleData> CurrentBundleHandle;
	FChunkedBufferWriter CompressedPacketWriter;
	TArray<FBlobHandle> CurrentBundleImports;

	TArray<FBlobHandle> BundleReferences;
	//	std::vector<std::pair<FBlobHandle, FAliasInfo>> PendingExportAliases;
	//	FChunkedBlobWriter EncodedPacketWriter;

	void StartPacket();
	void FinishPacket();
};