// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "KeyValueStorageClient.h"
#include <filesystem>

/**
 * Implementation of FStorageClient which writes data to files on disk.
 */
class HORDE_API FFileStorageClient : public FKeyValueStorageClient
{
public:
	FFileStorageClient(std::filesystem::path InRootDir);
	~FFileStorageClient();

	static FBlobLocator ReadRefFromFile(const std::filesystem::path &File);
	static void WriteRefToFile(const std::filesystem::path& File, const FBlobLocator& Locator);

	std::filesystem::path GetBlobFile(const FBlobLocator& Locator) const;
	std::filesystem::path GetRefFile(const FRefName& Name) const;

	virtual void AddAlias(const char* Name, FBlobAlias Alias) override;
	virtual void RemoveAlias(const char* Name, FBlobHandle Handle) override;
	virtual void FindAliases(const char* Name, int MaxResults, TArray<FBlobAlias>& OutAliases) override;

	virtual bool DeleteRef(const FRefName& Name) override;
	virtual FBlobHandle ReadRef(const FRefName& Name, const FRefCacheTime& CacheTime) const override;
	virtual void WriteRef(const FRefName& Name, const FBlobHandle& Handle, const FRefOptions& Options) override;

protected:
	virtual FBlob ReadBlob(const FBlobLocator& Locator) const override;
	virtual FBlobHandle WriteBlob(const FUtf8StringView& BasePath, const FBlobType& Type, const TArrayView<FMemoryView>& Data, const TArrayView<FBlobHandle>& Imports) override;

private:
	std::filesystem::path RootDir;
};
