// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "VirtualFileCache.h"

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "HAL/Runnable.h"
#include "Logging/LogMacros.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeRWLock.h"
#include "Async/MappedFileHandle.h"
#include "Async/Future.h"
#include "Containers/IntrusiveDoubleLinkedList.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVFC, Log, All);

DECLARE_STATS_GROUP(TEXT("VFC"), STATGROUP_VFC, STATCAT_Advanced);
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Files Added"), STAT_FilesAdded, STATGROUP_VFC, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Bytes Added"), STAT_BytesAdded, STATGROUP_VFC, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Files Removed"), STAT_FilesRemoved, STATGROUP_VFC, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Bytes Removed"), STAT_BytesRemoved, STATGROUP_VFC, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Files Evicted"), STAT_FilesEvicted, STATGROUP_VFC, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Bytes Evicted"), STAT_BytesEvicted, STATGROUP_VFC, );

// Fragmentation is defined as the amount of separation per file in the cache.
// A value of 1 means that there is no fragmentation
// A value of 2 means that the mean number of ranges per file is 2
constexpr double VFC_ALLOWED_FRAGMENTATION = 4.0f;

constexpr uint64 VFC_BLOCK_SIZE_MIN = 1;
constexpr uint64 VFC_BLOCK_SIZE_MAX = 1 << 24;

using FMappedRange = TUniquePtr<IMappedFileRegion>;

enum class EVFCFileVersion
{
	Invalid,
	Initial,

	Count,
	Current = Count - 1,
};

struct FBlockRange
{
	int32 StartIndex;
	int32 NumBlocks;
};

struct FRangeId
{
	int32 FileId;
	FBlockRange Range;
};

struct FDataReference
{
	TArray<FRangeId> Ranges;
	uint32 TotalSize = 0;

	mutable int64 LastReferencedUnixTime = 0; // Mutable since it needs to update when the data is read
	void Touch();
};

struct FBlockFile
{
	// Serialized:
	EVFCFileVersion FileVersion = EVFCFileVersion::Current;
	int32 FileId = 0;
	int32 BlockSize = 0;
	int32 NumBlocks = 0;

	TArray<FBlockRange> FreeRanges;
	TArray<FBlockRange> UsedRanges;

	// Not Serialized:
	TUniquePtr<IFileHandle> WriteHandle;
	TUniquePtr<IMappedFileHandle> MapHandle;
	bool bWriteLocked = false;
	TUniquePtr<FRWLock> FileHandleLock = MakeUnique<FRWLock>();

	int64 TotalSize()
	{
		return static_cast<int64>(NumBlocks) * BlockSize;
	}

	void Reset()
	{
		FileId = 0;
		BlockSize = 0;
		NumBlocks = 0;
		FreeRanges.Empty();
		UsedRanges.Empty();
		WriteHandle = nullptr;
		MapHandle = nullptr;
		bWriteLocked = false;
		FileHandleLock = MakeUnique<FRWLock>();
	}
};

using FFileMap = TMap<VFCKey, FDataReference>;
struct FVirtualFileCache;

struct IFileTableReader
{
	virtual ~IFileTableReader() = default;

	virtual bool ValidateRanges() = 0;
	virtual const FDataReference* FindDataReference(const VFCKey& Id) const = 0;
	virtual TIoStatusOr<TArray<uint8>> ReadData(VFCKey Id, int64 ReadOffset = 0, int64 ReadSizeOrZero = 0) = 0;
	virtual TIoStatusOr<uint64> GetSizeForChunk(const VFCKey& Id) const = 0;
	virtual bool DoesChunkExist(const VFCKey& Id) const = 0;
	virtual double CurrentFragmentation() const = 0;
	virtual int64 GetUsedSize() const = 0;
	virtual int64 GetTotalSize() const = 0;
};

struct FFileTable : IFileTableReader
{
	FFileTable(FVirtualFileCache* InParent)
		: Parent(InParent)
	{ }

private:
	FVirtualFileCache* Parent;
	EVFCFileVersion FileVersion = EVFCFileVersion::Current;

	/* Files on disk */
	TArray<FBlockFile> BlockFiles;

	/* Data stored in the chunks */
	FFileMap FileMap;

	int64 TotalSize = -1;
	int64 UsedSize = -1;
	int32 LastBlockFileId = 0;

	int32 WriteLockCount = 0;

public:
	void Initialize(const FVirtualFileCacheSettings& Settings);
	bool ReadTableFile();
	int32 CreateBlockFile(int64 FileSize, int32 BlockSize);
	bool DeleteBlockFile(int32 BlockFileId);
	void WriteTableFile();
	void Defragment();

	void Empty();
	bool CoalesceRanges();
	void CalculateSizes();

	FString GetBlockFilename(const FBlockFile& File) const;
	FBlockFile* GetFileForRange(FRangeId Id);
	FBlockFile* GetFileForId(int32 BlockFileId);
	bool BlockFileExistsOnDisk(const FBlockFile& BlockFile) const;
	IFileHandle* OpenBlockFileForWrite(FBlockFile& BlockFile);
	bool DeleteBlockFile(FFileTable* FileTable, int32 BlockFileId);
	bool RangeIsValid(FRangeId Block);
	bool EraseData(VFCKey Id);

	TIoStatusOr<FRangeId> AllocateSingleRange(FBlockFile& File, int64 MaximumSize);
	TArray<FRangeId> AllocateBlocksForSize(uint64 Size);
	int64 AllocationSize(FDataReference* DataRef);

	void FreeBlock(FRangeId RangeId);
	bool EnsureSizeFor(int64 RequiredBytes);
	int64 EvictOne();
	bool EvictAmount(int64 NumBytesToEvict);

	IMappedFileHandle* MapBlockFile(FBlockFile& BlockFile);
	FMappedRange MapFileRange(FRangeId Range);
	bool ReadRange(FRangeId RangeId, void* Dest, int64 ReadSize);

	FIoStatus WriteData(VFCKey Id, const uint8* Data, uint64 DataSize);

	// IFileTableReader
	virtual bool ValidateRanges() override;
	virtual const FDataReference* FindDataReference(const VFCKey& Id) const override;
	virtual TIoStatusOr<TArray<uint8>> ReadData(VFCKey Id, int64 ReadOffset = 0, int64 ReadSizeOrZero = 0) override;
	virtual TIoStatusOr<uint64> GetSizeForChunk(const VFCKey& Id) const override;
	virtual bool DoesChunkExist(const VFCKey& Id) const override;
	virtual double CurrentFragmentation() const override;
	virtual int64 GetUsedSize() const override;
	virtual int64 GetTotalSize() const override;
	// \IFileTableReader

	friend FArchive& operator<<(FArchive& Ar, FFileTable& FileTable);
};

struct FBlockRangeSortStartIndex
{
	bool operator()(const FBlockRange& L, const FBlockRange& R) const
	{
		return L.StartIndex < R.StartIndex;
	}
};
struct FBlockRangeSortSize
{
	bool operator()(const FBlockRange& L, const FBlockRange& R) const
	{
		return L.NumBlocks > R.NumBlocks;
	}
};

inline FString GetVFCDirectory()
{
	return FPaths::ProjectPersistentDownloadDir() / "VFC";
}
FArchive& operator<<(FArchive& Ar, FFileTable& FileTable);

enum class ERWOp : int8
{
	Invalid = -1,
	Read,
	Write,
	Erase,
};

struct FRWOp
{
	ERWOp Op = ERWOp::Invalid;
	VFCKey Target;

	// Write
	TSharedPtr<TArray<uint8>> DataToWrite;

	// Read
	TOptional<TPromise<TArray<uint8>>> ReadResult;
	int64 ReadOffset = 0;
	int64 ReadSize = 0;
};


struct FLruCacheNode : public TIntrusiveDoubleLinkedListNode<FLruCacheNode>
{
	// For reverse lookups
	VFCKey Key;
	TSharedPtr<TArray<uint8>> Data;
	// Store the size here to ensure that the CurrentSize of the LruCache can rely on the Data size value changing.
	uint64 RecordedSize;
};


class FLruCache
{
	void EvictToBelowMaxSize();
	void EvictOne();
	bool FreeSpaceFor(int64 SizeToAdd);

public:
	~FLruCache()
	{
		LruList.Reset();
	}
	FLruCacheNode* Find(VFCKey Key);
	const FLruCacheNode* Find(VFCKey Key) const;
	TSharedPtr<TArray<uint8>> ReadLockAndFindData(VFCKey Key) const;
	void Insert(VFCKey Key, TSharedPtr<TArray<uint8>> Data);
	void Remove(VFCKey Key);
	bool IsEnabled() const;
	void SetMaxSize(int64 NewMaxSize);

public:
	mutable FRWLock Lock;

private:
	TMap<VFCKey, TUniquePtr<FLruCacheNode>> NodeMap;
	TIntrusiveDoubleLinkedList<FLruCacheNode> LruList;
	int64 CurrentSize = 0;
	int64 MaxSize = 0;
};

struct FVirtualFileCache;

template <typename TTableType, FRWScopeLockType LOCKTYPE>
struct FFileTableLockingReference
{
	static_assert(LOCKTYPE == SLT_ReadOnly || LOCKTYPE == SLT_Write);

	FFileTableLockingReference(TTableType& InTable, FRWLock& InLock)
		: FileTable(&InTable)
		, Lock(&InLock)
	{
		if constexpr (LOCKTYPE == SLT_ReadOnly)
		{
			Lock->ReadLock();
		}
		else if constexpr (LOCKTYPE == SLT_Write)
		{
			Lock->WriteLock();
		}
	}

	~FFileTableLockingReference()
	{
		if (Lock)
		{
			if constexpr (LOCKTYPE == SLT_ReadOnly)
			{
				Lock->ReadUnlock();
			}
			else if constexpr (LOCKTYPE == SLT_Write)
			{
				Lock->WriteUnlock();
			}
		}
	}

	FFileTableLockingReference(FFileTableLockingReference&& ToMove)
		: FileTable(ToMove.FileTable)
		, Lock(ToMove.Lock)
	{
		ToMove.Lock = nullptr;
	}

	FFileTableLockingReference(FFileTableLockingReference& ToMove) = delete;
	FFileTableLockingReference operator=(FFileTableLockingReference& ToMove) = delete;
	FFileTableLockingReference& operator=(FFileTableLockingReference&& ToMove) = delete;

	TTableType* operator->()
	{
		return FileTable;
	}

	TTableType* Get()
	{
		return FileTable;
	}

private:
	TTableType* FileTable;
	FRWLock* Lock;
};

using FFileTableReader = FFileTableLockingReference<const IFileTableReader, FRWScopeLockType::SLT_ReadOnly>;
using FFileTableMutator = FFileTableLockingReference<FFileTable, FRWScopeLockType::SLT_ReadOnly>;
using FFileTableWriter = FFileTableLockingReference<FFileTable, FRWScopeLockType::SLT_Write>;

struct FVirtualFileCacheThread : public FRunnable
{
	FVirtualFileCacheThread(FVirtualFileCache* InParent);
	~FVirtualFileCacheThread();

	/* FRunnable */
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;
	/* FRunnable */

	// Public Interface
	TFuture<TArray<uint8>> RequestRead(VFCKey Target, int64 ReadOffset = 0, int64 ReadSizeOrZero = 0);
	void RequestWrite(VFCKey Target, TArrayView<const uint8> Data);
	void RequestErase(VFCKey Target);
	void Shutdown();

	[[nodiscard]] FFileTableReader ReadFileTable() const;
	[[nodiscard]] FFileTableMutator MutateFileTable();
	[[nodiscard]] FFileTableWriter ModifyFileTable();

public:
	void DeleteUnexpectedCacheFiles(TSet<FString>& ExpectedFiles);
	void FreeBlock(FRangeId RangeId);
	FString GetTableFilename() const;

	void EnqueueOrRunOp(TSharedPtr<FRWOp> Op);
	TSharedPtr<FRWOp> GetNextOp();
	void DoOneOp(FRWOp* Op);

	void Touch(FDataReference& Id);
	void EraseTableFile();

	void SetInMemoryCacheSize(int64 MaxSize);

	uint64 GetTotalMemCacheHits() const;
	uint64 GetTotalMemCacheMisses() const;

public:
	FVirtualFileCache* Parent;
	FRunnableThread* Thread;
	FEvent* Event;

	FLruCache MemCache;

	FRWLock OperationQueueLock;
	TArray<TSharedPtr<FRWOp>> OperationQueue;

	FString BasePath;

	std::atomic_bool bStopRequested;

private:
	mutable FRWLock FileTableLock;
	FFileTable FileTableStorage;
	uint64 TotalMemCacheHits = 0;
	uint64 TotalMemCacheMisses = 0;
};

struct FVirtualFileCache final : IVirtualFileCache
{
	FVirtualFileCache()
		: Thread(this)
	{}
	virtual ~FVirtualFileCache()
	{
		Shutdown();
	}
	FVirtualFileCache(const FVirtualFileCache&) = delete;
	FVirtualFileCache& operator=(const FVirtualFileCache&) = delete;

public:
	void Shutdown();

	virtual void Initialize(const FVirtualFileCacheSettings& Settings) override;
	virtual FIoStatus WriteData(VFCKey Id, const uint8* Data, uint64 DataSize) override;
	virtual TFuture<TArray<uint8>> ReadData(VFCKey Id, int64 ReadOffset = 0, int64 ReadSizeOrZero = 0) override;
	virtual bool DoesChunkExist(const VFCKey& Id) const override;
	virtual void EraseData(VFCKey Id) override;
	virtual TIoStatusOr<uint64> GetSizeForChunk(const VFCKey& Id) const override;
	virtual double CurrentFragmentation() const override;
	virtual void Defragment() override;
	virtual int64 GetTotalSize() const override;
	virtual int64 GetUsedSize() const override;
	virtual uint64 GetTotalMemCacheHits() const override;
	virtual uint64 GetTotalMemCacheMisses() const override;

	FString GetTableFilename() const { return Thread.GetTableFilename(); }

public:
	FChunkEvictedDelegate OnDataEvicted;
	FVirtualFileCacheThread Thread;
	FString BasePath;

private:
	FVirtualFileCacheSettings Settings;

	friend struct FVirtualFileCacheThread;
};

