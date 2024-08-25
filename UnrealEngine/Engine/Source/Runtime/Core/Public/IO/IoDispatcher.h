// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Future.h"
#include "Async/TaskGraphInterfaces.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/StringFwd.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Delegates/Delegate.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/CriticalSection.h"
#include "HAL/PlatformAtomics.h"
#include "HAL/PlatformFile.h"
#include "HAL/UnrealMemory.h"
#include "IO/IoBuffer.h"
#include "IO/IoChunkId.h"
#include "IO/IoContainerId.h"
#include "IO/IoDispatcherPriority.h"
#include "IO/IoHash.h"
#include "IO/IoStatus.h"
#include "Logging/LogMacros.h"
#include "Math/NumericLimits.h"
#include "Memory/MemoryFwd.h"
#include "Memory/MemoryView.h"
#include "Misc/AES.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Build.h"
#include "Misc/ByteSwap.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/Guid.h"
#include "Misc/IEngineCrypto.h"
#include "Misc/SecureHash.h"
#include "Serialization/Archive.h"
#include "Serialization/FileRegions.h"
#include "String/BytesToHex.h"
#include "Tasks/Task.h"
#include "Templates/Function.h"
#include "Templates/RefCounting.h"
#include "Templates/SharedPointer.h"
#include "Templates/TypeCompatibleBytes.h"
#include "Templates/UniquePtr.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"

class FEvent;
class FIoBatchImpl;
class FIoDirectoryIndexReaderImpl;
class FIoDispatcher;
class FIoDispatcherImpl;
class FIoRequest;
class FIoRequestImpl;
class FIoStoreEnvironment;
class FIoStoreReader;
class FIoStoreReaderImpl;
class FIoStoreWriterContextImpl;
class FPackageId;
class IMappedFileHandle;
class IMappedFileRegion;
struct FFileRegion;
struct IIoDispatcherBackend;
struct FIoOffsetAndLength;
template <typename CharType> class TStringBuilderBase;
template <typename OptionalType> struct TOptional;

CORE_API DECLARE_LOG_CATEGORY_EXTERN(LogIoDispatcher, Log, All);

//////////////////////////////////////////////////////////////////////////

/** Helper used to manage creation of I/O store file handles etc
  */
class FIoStoreEnvironment
{
public:
	CORE_API FIoStoreEnvironment();
	CORE_API ~FIoStoreEnvironment();

	CORE_API void InitializeFileEnvironment(FStringView InPath, int32 InOrder = 0);

	const FString& GetPath() const { return Path; }
	int32 GetOrder() const { return Order; }

private:
	FString			Path;
	int32			Order = 0;
};

class FIoChunkHash
{
public:
	friend uint32 GetTypeHash(const FIoChunkHash& InChunkHash)
	{
		uint32 Result = 5381;
		for (int i = 0; i < sizeof Hash; ++i)
		{
			Result = Result * 33 + InChunkHash.Hash[i];
		}
		return Result;
	}

	friend FArchive& operator<<(FArchive& Ar, FIoChunkHash& ChunkHash)
	{
		Ar.Serialize(&ChunkHash.Hash, sizeof Hash);
		return Ar;
	}

	inline bool operator ==(const FIoChunkHash& Rhs) const
	{
		return 0 == FMemory::Memcmp(Hash, Rhs.Hash, sizeof Hash);
	}

	inline bool operator !=(const FIoChunkHash& Rhs) const
	{
		return !(*this == Rhs);
	}

	inline FString ToString() const
	{
		return BytesToHex(Hash, 20);
	}

	FIoHash ToIoHash() const
	{
		FIoHash IoHash;
		FMemory::Memcpy(IoHash.GetBytes(), Hash, sizeof(FIoHash));
		return IoHash;
	}

	static FIoChunkHash CreateFromIoHash(const FIoHash& IoHash)
	{
		FIoChunkHash Result;
		FMemory::Memcpy(Result.Hash, &IoHash, sizeof IoHash);
		FMemory::Memset(Result.Hash + 20, 0, 12);
		return Result;
	}

	static FIoChunkHash HashBuffer(const void* Data, uint64 DataSize)
	{
		return CreateFromIoHash(FIoHash::HashBuffer(Data, DataSize));
	}

private:
	uint8	Hash[32];
};

//////////////////////////////////////////////////////////////////////////

class FIoReadOptions
{
public:
	FIoReadOptions() = default;

	FIoReadOptions(uint64 InOffset, uint64 InSize)
		: RequestedOffset(InOffset)
		, RequestedSize(InSize)
	{ }
	
	FIoReadOptions(uint64 InOffset, uint64 InSize, void* InTargetVa)
		: RequestedOffset(InOffset)
		, RequestedSize(InSize)
		, TargetVa(InTargetVa)
	{ }

	~FIoReadOptions() = default;

	void SetRange(uint64 Offset, uint64 Size)
	{
		RequestedOffset = Offset;
		RequestedSize	= Size;
	}

	void SetTargetVa(void* InTargetVa)
	{
		TargetVa = InTargetVa;
	}

	uint64 GetOffset() const
	{
		return RequestedOffset;
	}

	uint64 GetSize() const
	{
		return RequestedSize;
	}

	void* GetTargetVa() const
	{
		return TargetVa;
	}

private:
	uint64	RequestedOffset = 0;
	uint64	RequestedSize = ~uint64(0);
	void* TargetVa = nullptr;
};

//////////////////////////////////////////////////////////////////////////

/**
  */
class FIoRequest final
{
public:
	FIoRequest() = default;
	CORE_API ~FIoRequest();

	CORE_API FIoRequest(const FIoRequest& Other);
	CORE_API FIoRequest(FIoRequest&& Other);
	CORE_API FIoRequest& operator=(const FIoRequest& Other);
	CORE_API FIoRequest& operator=(FIoRequest&& Other);
	CORE_API FIoStatus						Status() const;
	CORE_API const FIoBuffer*				GetResult() const;
	CORE_API const FIoBuffer&				GetResultOrDie() const;
	CORE_API void							Cancel();
	CORE_API void							UpdatePriority(uint32 NewPriority);
	CORE_API void							Release();

private:
	FIoRequestImpl* Impl = nullptr;

	explicit FIoRequest(FIoRequestImpl* InImpl);

	friend class FIoDispatcher;
	friend class FIoDispatcherImpl;
	friend class FIoBatch;
};

using FIoReadCallback = TFunction<void(TIoStatusOr<FIoBuffer>)>;

inline int32 ConvertToIoDispatcherPriority(EAsyncIOPriorityAndFlags AIOP)
{
	int32 AIOPriorityToIoDispatcherPriorityMap[] = {
		IoDispatcherPriority_Min,
		IoDispatcherPriority_Low,
		IoDispatcherPriority_Medium - 1,
		IoDispatcherPriority_Medium,
		IoDispatcherPriority_High,
		IoDispatcherPriority_Max
	};
	static_assert(AIOP_NUM == UE_ARRAY_COUNT(AIOPriorityToIoDispatcherPriorityMap), "IoDispatcher and AIO priorities mismatch");
	return AIOPriorityToIoDispatcherPriorityMap[AIOP & AIOP_PRIORITY_MASK];
}

/** I/O batch

	This is a primitive used to group I/O requests for synchronization
	purposes
  */
class FIoBatch final
{
	friend class FIoDispatcher;
	friend class FIoDispatcherImpl;
	friend class FIoRequestStats;

public:
	CORE_API FIoBatch();
	CORE_API FIoBatch(FIoBatch&& Other);
	CORE_API ~FIoBatch();
	CORE_API FIoBatch& operator=(FIoBatch&& Other);
	CORE_API FIoRequest Read(const FIoChunkId& Chunk, FIoReadOptions Options, int32 Priority);
	CORE_API FIoRequest ReadWithCallback(const FIoChunkId& ChunkId, const FIoReadOptions& Options, int32 Priority, FIoReadCallback&& Callback);

	CORE_API void Issue();
	CORE_API void IssueWithCallback(TFunction<void()>&& Callback);
	CORE_API void IssueAndTriggerEvent(FEvent* Event);
	CORE_API void IssueAndDispatchSubsequents(FGraphEventRef Event);

private:
	FIoBatch(FIoDispatcherImpl& InDispatcher);
	FIoRequestImpl* ReadInternal(const FIoChunkId& ChunkId, const FIoReadOptions& Options, int32 Priority);

	FIoDispatcherImpl*	Dispatcher;
	FIoRequestImpl*		HeadRequest = nullptr;
	FIoRequestImpl*		TailRequest = nullptr;
};

/**
 * Mapped region.
 */
struct FIoMappedRegion
{
	IMappedFileHandle* MappedFileHandle = nullptr;
	IMappedFileRegion* MappedFileRegion = nullptr;
};

struct FIoDispatcherMountedContainer
{
	FIoStoreEnvironment Environment;
	FIoContainerId ContainerId;
};

struct FIoSignatureError
{
	FString ContainerName;
	int32 BlockIndex = INDEX_NONE;
	FSHAHash ExpectedHash;
	FSHAHash ActualHash;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FIoSignatureErrorDelegate, const FIoSignatureError&);

struct FIoSignatureErrorEvent
{
	FCriticalSection CriticalSection;
	FIoSignatureErrorDelegate SignatureErrorDelegate;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FIoSignatureErrorDelegate, const FIoSignatureError&);

DECLARE_MULTICAST_DELEGATE_OneParam(FIoContainerMountedDelegate, const FIoContainerId&);

/** I/O dispatcher
  */
class FIoDispatcher final
{
public:
	DECLARE_EVENT_OneParam(FIoDispatcher, FIoContainerMountedEvent, const FIoDispatcherMountedContainer&);
	DECLARE_EVENT_OneParam(FIoDispatcher, FIoContainerUnmountedEvent, const FIoDispatcherMountedContainer&);

	CORE_API						FIoDispatcher();
	CORE_API						~FIoDispatcher();

	CORE_API void					Mount(TSharedRef<IIoDispatcherBackend> Backend, int32 Priority = 0);

	CORE_API FIoBatch				NewBatch();

	CORE_API TIoStatusOr<FIoMappedRegion> OpenMapped(const FIoChunkId& ChunkId, const FIoReadOptions& Options);

	// Polling methods
	CORE_API bool					DoesChunkExist(const FIoChunkId& ChunkId) const;
	CORE_API TIoStatusOr<uint64>	GetSizeForChunk(const FIoChunkId& ChunkId) const;
	CORE_API int64					GetTotalLoaded() const;


	// Events
	CORE_API FIoSignatureErrorDelegate& OnSignatureError();

	FIoDispatcher(const FIoDispatcher&) = default;
	FIoDispatcher& operator=(const FIoDispatcher&) = delete;

	static CORE_API bool IsInitialized();
	static CORE_API FIoStatus Initialize();
	static CORE_API void InitializePostSettings();
	static CORE_API void Shutdown();
	static CORE_API FIoDispatcher& Get();

private:
	CORE_API bool					DoesChunkExist(const FIoChunkId& ChunkId, const FIoOffsetAndLength& ChunkRange) const;
	CORE_API TIoStatusOr<uint64>	GetSizeForChunk(const FIoChunkId& ChunkId, const FIoOffsetAndLength& ChunkRange, uint64& OutAvailable) const;

	FIoDispatcherImpl* Impl = nullptr;

	friend class FIoRequest;
	friend class FIoBatch;
	friend class FIoQueue;
	friend class FBulkData;
};

//////////////////////////////////////////////////////////////////////////

class FIoDirectoryIndexHandle
{
	static constexpr uint32 InvalidHandle = ~uint32(0);
	static constexpr uint32 RootHandle = 0;

public:
	FIoDirectoryIndexHandle() = default;

	inline bool IsValid() const
	{
		return Handle != InvalidHandle;
	}

	inline bool operator<(FIoDirectoryIndexHandle Other) const
	{
		return Handle < Other.Handle;
	}

	inline bool operator==(FIoDirectoryIndexHandle Other) const
	{
		return Handle == Other.Handle;
	}

	inline friend uint32 GetTypeHash(FIoDirectoryIndexHandle InHandle)
	{
		return InHandle.Handle;
	}

	inline uint32 ToIndex() const
	{
		return Handle;
	}

	static inline FIoDirectoryIndexHandle FromIndex(uint32 Index)
	{
		return FIoDirectoryIndexHandle(Index);
	}

	static inline FIoDirectoryIndexHandle RootDirectory()
	{
		return FIoDirectoryIndexHandle(RootHandle);
	}

	static inline FIoDirectoryIndexHandle Invalid()
	{
		return FIoDirectoryIndexHandle(InvalidHandle);
	}

private:
	FIoDirectoryIndexHandle(uint32 InHandle)
		: Handle(InHandle) { }

	uint32 Handle = InvalidHandle;
};

using FDirectoryIndexVisitorFunction = TFunctionRef<bool(FStringView, const uint32)>;

class FIoDirectoryIndexReader
{
public:
	CORE_API FIoDirectoryIndexReader();
	CORE_API ~FIoDirectoryIndexReader();
	CORE_API FIoStatus Initialize(TArray<uint8>& InBuffer, FAES::FAESKey InDecryptionKey);

	CORE_API const FString& GetMountPoint() const;
	CORE_API FIoDirectoryIndexHandle GetChildDirectory(FIoDirectoryIndexHandle Directory) const;
	CORE_API FIoDirectoryIndexHandle GetNextDirectory(FIoDirectoryIndexHandle Directory) const;
	CORE_API FIoDirectoryIndexHandle GetFile(FIoDirectoryIndexHandle Directory) const;
	CORE_API FIoDirectoryIndexHandle GetNextFile(FIoDirectoryIndexHandle File) const;
	CORE_API FStringView GetDirectoryName(FIoDirectoryIndexHandle Directory) const;
	CORE_API FStringView GetFileName(FIoDirectoryIndexHandle File) const;
	CORE_API uint32 GetFileData(FIoDirectoryIndexHandle File) const;

	CORE_API bool IterateDirectoryIndex(FIoDirectoryIndexHandle Directory, FStringView Path, FDirectoryIndexVisitorFunction Visit) const;

private:
	UE_NONCOPYABLE(FIoDirectoryIndexReader);

	FIoDirectoryIndexReaderImpl* Impl;
};

//////////////////////////////////////////////////////////////////////////

struct FIoStoreWriterSettings
{
	FName CompressionMethod = NAME_None;
	uint64 CompressionBlockSize = 64 << 10;

	// This does not align every entry - it tries to prevent excess crossings of this boundary by inserting padding.
	// and happens whether or not the entry is compressed.
	uint64 CompressionBlockAlignment = 0;
	int32 CompressionMinBytesSaved = 0;
	int32 CompressionMinPercentSaved = 0;
	int32 CompressionMinSizeToConsiderDDC = 0;
	uint64 MemoryMappingAlignment = 0;
	uint64 MaxPartitionSize = 0;
	bool bEnableFileRegions = false;
	bool bCompressionEnableDDC = false;
};

enum class EIoContainerFlags : uint8
{
	None,
	Compressed	= (1 << 0),
	Encrypted	= (1 << 1),
	Signed		= (1 << 2),
	Indexed		= (1 << 3),
	OnDemand	= (1 << 4),
};
ENUM_CLASS_FLAGS(EIoContainerFlags);

struct FIoContainerSettings
{
	FIoContainerId ContainerId;
	EIoContainerFlags ContainerFlags = EIoContainerFlags::None;
	FGuid EncryptionKeyGuid;
	FAES::FAESKey EncryptionKey;
	FRSAKeyHandle SigningKey;
	bool bGenerateDiffPatch = false;

	bool IsCompressed() const
	{
		return !!(ContainerFlags & EIoContainerFlags::Compressed);
	}

	bool IsEncrypted() const
	{
		return !!(ContainerFlags & EIoContainerFlags::Encrypted);
	}

	bool IsSigned() const
	{
		return !!(ContainerFlags & EIoContainerFlags::Signed);
	}

	bool IsIndexed() const
	{
		return !!(ContainerFlags & EIoContainerFlags::Indexed);
	}
	
	bool IsOnDemand() const
	{
		return !!(ContainerFlags & EIoContainerFlags::OnDemand);
	}
};

struct FIoStoreWriterResult
{
	FIoContainerId ContainerId;
	FString ContainerName; // This is the base filename of the utoc used for output.
	int64 TocSize = 0;
	int64 TocEntryCount = 0;
	int64 PaddingSize = 0;
	int64 UncompressedContainerSize = 0; // this is the size the container would be if it were uncompressed.
	int64 CompressedContainerSize = 0; // this is the size of the container with the given compression (which may be none). Should be the sum of all partition file sizes.
	int64 DirectoryIndexSize = 0;
	uint64 TotalEntryCompressedSize = 0; // sum of the compressed size of entries excluding encryption alignment.
	uint64 ReferenceCacheMissBytes = 0; // number of compressed bytes excluding alignment that could have been from refcache but weren't.
	uint64 AddedChunksCount = 0;
	uint64 AddedChunksSize = 0;
	uint64 ModifiedChunksCount = 0;
	uint64 ModifiedChunksSize = 0;
	FName CompressionMethod = NAME_None;
	EIoContainerFlags ContainerFlags = EIoContainerFlags::None;
};

struct FIoWriteOptions
{
	FString FileName;
	const TCHAR* DebugName = nullptr;
	bool bForceUncompressed = false;
	bool bIsMemoryMapped = false;
};

class FIoStoreWriterContext
{
public:
	struct FProgress
	{
		uint64 TotalChunksCount = 0;
		uint64 HashedChunksCount = 0;
		// Number of chunks where we avoided reading and hashing, and instead used the result from the hashdb, and their types
		uint64 HashDbChunksCount = 0;
		uint64 HashDbChunksByType[(int8)EIoChunkType::MAX] = { 0 };
		// Number of chunks that were passed to the compressor (i.e. passed the various opt-outs), and their types
		uint64 CompressedChunksCount = 0;
		uint64 CompressedChunksByType[(int8)EIoChunkType::MAX] = { 0 };
		uint64 SerializedChunksCount = 0;
		uint64 ScheduledCompressionTasksCount = 0;
		uint64 CompressionDDCHitCount = 0;
		uint64 CompressionDDCMissCount = 0;

		// The number of chunk retrieved from the reference cache database, and their types.
		uint64 RefDbChunksCount{ 0 };
		uint64 RefDbChunksByType[(int8)EIoChunkType::MAX] = { 0 };
		
		// The type of chunk that landed in BeginCompress before any opt-outs.
		uint64 BeginCompressChunksByType[(int8)EIoChunkType::MAX] = { 0 };
	};

	CORE_API FIoStoreWriterContext();
	CORE_API ~FIoStoreWriterContext();

	[[nodiscard]] CORE_API FIoStatus Initialize(const FIoStoreWriterSettings& InWriterSettings);
	CORE_API TSharedPtr<class IIoStoreWriter> CreateContainer(const TCHAR* InContainerPath, const FIoContainerSettings& InContainerSettings);
	CORE_API void Flush();
	CORE_API FProgress GetProgress() const;

private:
	FIoStoreWriterContextImpl* Impl;
};

class IIoStoreWriteRequest
{
public:
	virtual ~IIoStoreWriteRequest() = default;

	// Launches any async operations necessary in order to access the buffer. CompletionEvent is set once it's ready, which may be immediate.
	virtual void PrepareSourceBufferAsync(FGraphEventRef CompletionEvent) = 0;
	virtual uint64 GetOrderHint() = 0;
	virtual TArrayView<const FFileRegion> GetRegions() = 0;

	// Only valid after the completion event passed to PrepareSourceBufferAsync has fired.
	virtual const FIoBuffer* GetSourceBuffer() = 0;

	// Can't be called between PrepareSourceBufferAsync and its completion!
	virtual void FreeSourceBuffer() = 0;
};


struct FIoStoreTocChunkInfo
{
	FIoChunkId Id;
	FString FileName;
	FIoChunkHash Hash;
	uint64 Offset;
	uint64 OffsetOnDisk;
	uint64 Size;
	uint64 CompressedSize;
	uint32 NumCompressedBlocks;
	int32 PartitionIndex;
	EIoChunkType ChunkType;
	bool bHasValidFileName;
	bool bForceUncompressed;
	bool bIsMemoryMapped;
	bool bIsCompressed;
};

struct FIoStoreTocCompressedBlockInfo
{
	uint64 Offset;
	uint32 CompressedSize;
	uint32 UncompressedSize;
	uint8 CompressionMethodIndex;
};

struct FIoStoreCompressedBlockInfo
{
	/**
	* Hash of the block on disk. Note that this can be all zero if the hash info was not computed when
	* the utoc was created.
	*/
	FIoHash DiskHash;

	/** Name of the method used to compress the block. */
	FName CompressionMethod;
	/** The size of relevant data in the block (i.e. what you pass to decompress). */
	uint32 CompressedSize;
	/** The size of the _block_ after decompression. This is not adjusted for any FIoReadOptions used. */
	uint32 UncompressedSize;
	/** The size of the data this block takes in IoBuffer (i.e. after padding for decryption). */
	uint32 AlignedSize;
	/** Where in IoBuffer this block starts. */
	uint64 OffsetInBuffer;
};

struct FIoStoreCompressedChunkInfo
{
	/** Info about the blocks that the chunk is split up into. */
	TArray<FIoStoreCompressedBlockInfo> Blocks;

	/**
	* Hash of the compressed chunk on disk. Note that this can be all zero if the hash info was
	* not computed when the utoc was created.
	*/
	FIoHash DiskHash;

	/** There is where the data starts in IoBuffer(for when you pass in a data range via FIoReadOptions). */
	uint64 UncompressedOffset = 0;
	/**
	 * This is the total size requested via FIoReadOptions. Notably, if you requested a narrow range, you could
	 * add up all the block uncompressed sizes and it would be larger than this.
	 */
	uint64 UncompressedSize = 0;
	/** This is the total size of compressed data, which is less than IoBuffer size due to padding for decryption. */
	uint64 TotalCompressedSize = 0;
};

struct FIoStoreCompressedReadResult
{
	/** The buffer containing the chunk. */
	FIoBuffer IoBuffer;

	/** Info about the blocks that the chunk is split up into. */
	TArray<FIoStoreCompressedBlockInfo> Blocks;
	// There is where the data starts in IoBuffer (for when you pass in a data range via FIoReadOptions)
	uint64 UncompressedOffset = 0;
	// This is the total size requested via FIoReadOptions. Notably, if you requested a narrow range, you could
	// add up all the block uncompressed sizes and it would be larger than this.
	uint64 UncompressedSize = 0;
	// This is the total size of compressed data, which is less than IoBuffer size due to padding for decryption.
	uint64 TotalCompressedSize = 0;
};

class IIoStoreWriterReferenceChunkDatabase
{
public:
	virtual ~IIoStoreWriterReferenceChunkDatabase() = default;

	/*
	* Used by IIoStoreWriter to check and see if there's a reference chunk that matches the data that
	* IoStoreWriter wants to compress and write. Validity checks must be synchronous - if a chunk can't be
	* used for some reason (no matching chunk exists or otherwise), this function must return false and not 
	* call InCompletionCallback.
	* 
	* Once a matching chunk is found, it is read from the source iostore container asynchronously, and upon
	* completion InCompletionCallback is called with the raw output from FIoStoreReader::ReadCompressed (i.e.
	* FIoStoreCompressedReadResult). Failures once the async read process has started are currently fatal due to
	* difficulties in rekicking a read.
	* 
	* For the moment, changes in compression method are allowed.
	* 
	* RetrieveChunk is not currently thread safe and must be called from a single thread.
	* 
	* Chunks provided *MUST* decompress to bits that hash to the exact value provided in InChunkKey (i.e. be exactly the same bits),
	* and also be the same number of blocks (i.e. same CompressionBlockSize)
	*/
	virtual bool RetrieveChunk(const TPair<FIoContainerId, FIoChunkHash>& InChunkKey, TUniqueFunction<void(TIoStatusOr<FIoStoreCompressedReadResult>)> InCompletionCallback) = 0;

	/* 
	* Quick synchronous existence check that returns the number of blocks for the chunk. This is used to set up
	* the necessary structures without needing to read the source data for the chunk.
	*/
	virtual bool ChunkExists(const TPair<FIoContainerId, FIoChunkHash>& InChunkKey, const FIoChunkId& InChunkId, uint32& OutNumChunkBlocks) = 0;

	/*
	* Returns the compression block size that was used to break up the IoChunks in the source containers. If this is different than what we want, 
	* then none of the chunks will ever match. Knowing this up front allows us to only match on hash
	*/
	virtual uint32 GetCompressionBlockSize() const = 0;

	/*
	* Called by an iostore writer implementation to notify the ref cache it's been added
	*/
	virtual void NotifyAddedToWriter(const FIoContainerId& InContainerId) = 0;
};

/**
*	Allows the IIoStoreWriter to avoid loading and hashing chunks, saving pak/stage time, as the normal
*	process involved loading the chunks, hashing them, freeing them, making some decisions, then loading
*	them _again_ for compression/writting. It's completely fine for this to not have all available hashes,
*	but they have to match when provided!
*/
class IIoStoreWriterHashDatabase
{
public:
	virtual ~IIoStoreWriterHashDatabase() = default;
	virtual bool FindHashForChunkId(const FIoChunkId& ChunkId, FIoChunkHash& OutHash) const = 0;
};


class IIoStoreWriter
{
public:
	virtual ~IIoStoreWriter() = default;

	/**
	*	If a reference database is provided, the IoStoreWriter implementation may elect to reuse compressed blocks
	*	from previous containers instead of recompressing input data. This must be set before any writes are appended.
	*/
	virtual void SetReferenceChunkDatabase(TSharedPtr<IIoStoreWriterReferenceChunkDatabase> ReferenceChunkDatabase) = 0;
	virtual void SetHashDatabase(TSharedPtr<IIoStoreWriterHashDatabase> HashDatabase, bool bVerifyHashDatabase) = 0;
	virtual void EnableDiskLayoutOrdering(const TArray<TUniquePtr<FIoStoreReader>>& PatchSourceReaders = TArray<TUniquePtr<FIoStoreReader>>()) = 0;
	virtual void Append(const FIoChunkId& ChunkId, FIoBuffer Chunk, const FIoWriteOptions& WriteOptions, uint64 OrderHint = MAX_uint64) = 0;
	virtual void Append(const FIoChunkId& ChunkId, IIoStoreWriteRequest* Request, const FIoWriteOptions& WriteOptions) = 0;
	virtual TIoStatusOr<FIoStoreWriterResult> GetResult() = 0;
	virtual void EnumerateChunks(TFunction<bool(FIoStoreTocChunkInfo&&)>&& Callback) const = 0;
};

class FIoStoreReader
{
public:
	CORE_API FIoStoreReader();
	CORE_API ~FIoStoreReader();

	[[nodiscard]] CORE_API FIoStatus Initialize(FStringView ContainerPath, const TMap<FGuid, FAES::FAESKey>& InDecryptionKeys);
	CORE_API FIoContainerId GetContainerId() const;
	CORE_API uint32 GetVersion() const;
	CORE_API EIoContainerFlags GetContainerFlags() const;
	CORE_API FGuid GetEncryptionKeyGuid() const;

	CORE_API void EnumerateChunks(TFunction<bool(FIoStoreTocChunkInfo&&)>&& Callback) const;
	CORE_API TIoStatusOr<FIoStoreTocChunkInfo> GetChunkInfo(const FIoChunkId& Chunk) const;
	CORE_API TIoStatusOr<FIoStoreTocChunkInfo> GetChunkInfo(const uint32 TocEntryIndex) const;
	CORE_API TIoStatusOr<FIoStoreCompressedChunkInfo> GetChunkCompressedInfo(const FIoChunkId& Chunk) const;

	// Reads the chunk off the disk, decrypting/decompressing as necessary.
	CORE_API TIoStatusOr<FIoBuffer> Read(const FIoChunkId& Chunk, const FIoReadOptions& Options) const;
	
	// As Read(), except returns a task that will contain the result after a .Wait/.BusyWait.
	CORE_API UE::Tasks::TTask<TIoStatusOr<FIoBuffer>> ReadAsync(const FIoChunkId& Chunk, const FIoReadOptions& Options) const;

	// Reads and decrypts if necessary the compressed blocks, but does _not_ decompress them. The totality of the data is stored
	// in FIoStoreCompressedReadResult::FIoBuffer as a contiguous buffer, however each block is padded during encryption, so
	// either use FIoStoreCompressedBlockInfo::AlignedSize to advance through the buffer, or use FIoStoreCompressedBlockInfo::OffsetInBuffer
	// directly.
	CORE_API TIoStatusOr<FIoStoreCompressedReadResult> ReadCompressed(const FIoChunkId& Chunk, const FIoReadOptions& Options, bool bDecrypt = true) const;

	CORE_API const FIoDirectoryIndexReader& GetDirectoryIndexReader() const;

	CORE_API void GetFilenamesByBlockIndex(const TArray<int32>& InBlockIndexList, TArray<FString>& OutFileList) const;
	CORE_API void GetFilenames(TArray<FString>& OutFileList) const;

	CORE_API uint32 GetCompressionBlockSize() const;
	CORE_API const TArray<FName>& GetCompressionMethods() const;
	CORE_API void EnumerateCompressedBlocks(TFunction<bool(const FIoStoreTocCompressedBlockInfo&)>&& Callback) const;
	CORE_API void EnumerateCompressedBlocksForChunk(const FIoChunkId& Chunk, TFunction<bool(const FIoStoreTocCompressedBlockInfo&)>&& Callback) const;

	// Returns the .ucas file path and all partition(s) ({containername}_s1.ucas, {containername}_s2.ucas)
	CORE_API void GetContainerFilePaths(TArray<FString>& OutPaths);

private:
	FIoStoreReaderImpl* Impl;
};
//////////////////////////////////////////////////////////////////////////
