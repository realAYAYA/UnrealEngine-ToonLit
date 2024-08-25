// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "KeyValueStorageClient.h"

/**
 * Implementation of FStorageClient which stores data in memory.
 */
class HORDE_API FMemoryStorageClient final : public FKeyValueStorageClient
{
public:
	FMemoryStorageClient();
	~FMemoryStorageClient();

	virtual void AddAlias(const char* Name, FBlobAlias Alias) override;
	virtual void RemoveAlias(const char* Name, FBlobHandle Handle) override;
	virtual void FindAliases(const char* Name, int MaxResults, TArray<FBlobAlias>& OutAliases) override;

	virtual bool DeleteRef(const FRefName& Name) override;
	virtual FBlobHandle ReadRef(const FRefName& Name, const FRefCacheTime& CacheTime) const override;
	virtual void WriteRef(const FRefName& Name, const FBlobHandle& Target, const FRefOptions& Options) override;

protected:
	virtual FBlob ReadBlob(const FBlobLocator& Locator) const override;
	virtual FBlobHandle WriteBlob(const FUtf8StringView& BasePath, const FBlobType& Type, const TArrayView<FMemoryView>& Data, const TArrayView<FBlobHandle>& Imports) override;

private:
	mutable FCriticalSection CriticalSection;

	TMap<FBlobLocator, FBlob> Blobs;
	TMap<FRefName, FBlobLocator> Refs;
	TMap<FUtf8String, TArray<FBlobAlias>> Aliases;
};
