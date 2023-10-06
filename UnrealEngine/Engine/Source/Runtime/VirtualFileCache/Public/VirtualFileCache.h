// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IO/IoDispatcher.h"

constexpr uint64 VFS_DEFAULT_BLOCK_SIZE = 8192ull;
constexpr uint64 VFS_BLOCK_FILE_SIZE = 2 * 1024ull * 1024ull * 1024ull;

using VFCKey = FSHAHash;

DECLARE_MULTICAST_DELEGATE_OneParam(FChunkEvictedDelegate, VFCKey);

struct FVirtualFileCacheSettings
{
	uint64 BlockSize = VFS_DEFAULT_BLOCK_SIZE;
	uint64 BlockFileSize = VFS_BLOCK_FILE_SIZE;
	uint64 NumBlockFiles = 1;
	FString OverrideDefaultDirectory;
	bool bMultiThreaded = true;

	// Memory Cache behavior
	uint64 RecentWriteLRUSize = 0;
};

struct IVirtualFileCache
{
	virtual ~IVirtualFileCache() = default;

	virtual void Initialize(const FVirtualFileCacheSettings& Context = {}) = 0;
	virtual FIoStatus WriteData(VFCKey Id, const uint8* Data, uint64 DataSize) = 0;
	virtual TFuture<TArray<uint8>> ReadData(VFCKey Id, int64 ReadOffset = 0, int64 ReadSizeOrZero = 0) = 0;
	virtual bool DoesChunkExist(const VFCKey& Id) const = 0;
	virtual void EraseData(VFCKey Id) = 0;
	virtual TIoStatusOr<uint64> GetSizeForChunk(const VFCKey& Id) const = 0;
	virtual double CurrentFragmentation() const = 0;
	virtual void Defragment() = 0;
	virtual int64 GetTotalSize() const = 0;
	virtual int64 GetUsedSize() const = 0;
	virtual uint64 GetTotalMemCacheHits() const = 0;
	virtual uint64 GetTotalMemCacheMisses() const = 0;

	static VIRTUALFILECACHE_API TSharedRef<IVirtualFileCache, ESPMode::ThreadSafe> CreateVirtualFileCache();
};

