// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CancellationToken.h"
#include "IO/IoStatus.h"
#include "Memory/MemoryFwd.h"
#include "Tasks/Task.h"
#include "Templates/UniquePtr.h"

class FIoBuffer;
class FIoReadOptions;
struct FIoHash;

DECLARE_LOG_CATEGORY_EXTERN(LogIasCache, Log, All);

/** Cache for binary blobs with a 20 byte cache key. */
class IIoCache
{
public:
	virtual ~IIoCache() = default;

	/** Returns whether the specified cache key is present in the cache. */
	virtual bool ContainsChunk(const FIoHash& Key) const = 0;

	/** Get the chunk associated with the specified cache key. */
	virtual UE::Tasks::TTask<TIoStatusOr<FIoBuffer>> Get(
		const FIoHash& Key,
		const FIoReadOptions& Options,
		const FIoCancellationToken* CancellationToken) = 0;

	/** Insert a new chunk into the cache. */
	virtual FIoStatus Put(const FIoHash& Key, FIoBuffer& Data) = 0;
};

struct FFileIoCacheConfig
{
	struct FRate
	{
		uint32 Allowance = ~0u;
		uint32 Ops = 256;
		uint32 Seconds = 1;
	};

	uint64 DiskQuota = 1ull << 30;
	uint32 MemoryQuota = 3 << 20;
	FRate WriteRate;
	bool DropCache = false;
};

TUniquePtr<IIoCache> MakeFileIoCache(const FFileIoCacheConfig& Config);
