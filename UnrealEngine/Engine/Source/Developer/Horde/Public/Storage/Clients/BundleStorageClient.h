// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Storage/StorageClient.h"
#include "Storage/BlobHandle.h"
#include "Storage/Bundles/BundleOptions.h"
#include "Storage/Clients/KeyValueStorageClient.h"

/**
 * Base class for an implementation of <see cref="IStorageClient"/>, providing implementations for some common functionality using bundles.
 */
class HORDE_API FBundleStorageClient final : public FKeyValueStorageClient
{
public:
	FBundleStorageClient(TSharedRef<FKeyValueStorageClient> InInner);
	virtual ~FBundleStorageClient() override;

	// FKeyValueStorageClient implementation
	virtual FBlob ReadBlob(const FBlobLocator& Locator) const override;
	virtual FBlobHandle WriteBlob(const FUtf8StringView& BasePath, const FBlobType& Type, const TArrayView<FMemoryView>& Data, const TArrayView<FBlobHandle>& Imports) override;

	// FBundleStorageClient implementation
	virtual FBlobHandle CreateHandle(const FBlobLocator& Locator) const override;
	virtual TUniquePtr<FBlobWriter> CreateWriter(FUtf8String BasePath) override;
	TUniquePtr<FBlobWriter> CreateWriter(FUtf8String BasePath, const FBundleOptions& Options);

	virtual void AddAlias(const char* Name, FBlobAlias Alias) override;
	virtual void RemoveAlias(const char* Name, FBlobHandle Handle) override;
	virtual void FindAliases(const char* Name, int MaxResults, TArray<FBlobAlias>& OutAliases) override;

	virtual bool DeleteRef(const FRefName& Name) override;
	virtual FBlobHandle ReadRef(const FRefName& Name, const FRefCacheTime& CacheTime) const override;
	virtual void WriteRef(const FRefName& Name, const FBlobHandle& Handle, const FRefOptions& Options) override;

private:
	class FBundleHandleData;
	friend FBundleHandleData;

	TSharedRef<FKeyValueStorageClient> Inner;

	TBlobHandle<FBundleHandleData> CreateBundleHandle(const FBlobLocator& Locator) const;
	FBlobHandle GetFragmentHandle(const FBlobHandle& BundleHandle, const FUtf8StringView& Fragment) const;
};
