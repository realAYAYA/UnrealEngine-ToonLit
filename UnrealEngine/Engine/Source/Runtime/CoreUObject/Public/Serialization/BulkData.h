// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/AsyncFileHandle.h"
#include "BulkDataBuffer.h"
#include "Containers/Array.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Map.h"
#include "Containers/SortedMap.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/MemoryBase.h"
#include "IO/IoDispatcher.h"
#include "IO/PackageId.h"
#include "Math/NumericLimits.h"
#include "Misc/AssertionMacros.h"
#include "Misc/CompressionFlags.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/PackagePath.h"
#include "Misc/PackageSegment.h"
#include "Serialization/Archive.h"
#include "Serialization/CustomVersion.h"
#include "Serialization/FileRegions.h"
#include "Templates/Function.h"
#include "Templates/IsPODType.h"
#include "Templates/UniquePtr.h"
#include "Templates/PimplPtr.h"
#include "UObject/NameTypes.h"

class FLinkerLoad;
class FOutputDevice;
class IAsyncReadFileHandle;
class IAsyncReadRequest;
class UObject;
struct FTimespan;
namespace UE { namespace Serialization { class FEditorBulkData; } }

#if WITH_EDITOR == 0 && WITH_EDITORONLY_DATA == 0
	#define USE_NEW_BULKDATA UE_DEPRECATED_MACRO(5.1, "The USE_NEW_BULKDATA macro has been deprecated in favor of USE_RUNTIME_BULKDATA.") 1
	#define USE_RUNTIME_BULKDATA 1
#else
	#define USE_NEW_BULKDATA UE_DEPRECATED_MACRO(5.1, "The USE_NEW_BULKDATA macro has been deprecated in favor of USE_RUNTIME_BULKDATA.") 0
	#define USE_RUNTIME_BULKDATA 0
#endif

// Enable the following to use the more compact FBulkDataStreamingToken in places where it is implemented
#define USE_BULKDATA_STREAMING_TOKEN UE_DEPRECATED_MACRO(5.0, "USE_BULKDATA_STREAMING_TOKEN now always evaluates to 0 and will be removed") 0
#define STREAMINGTOKEN_PARAM(param) UE_DEPRECATED_MACRO(5.0, "STREAMINGTOKEN_PARAM now always evaluates to a NOP")

// When set to 1 attempting to reload inline data will fail with the old loader in the same way that it fails in
// the new loader to keep the results consistent.
#define UE_KEEP_INLINE_RELOADING_CONSISTENT 0

class IMappedFileHandle;
class IMappedFileRegion;

/*-----------------------------------------------------------------------------
	I/O filename helper(s)
-----------------------------------------------------------------------------*/

/** A loose hash value that can be created from either a filenames or a FIoChunkId */
using FIoFilenameHash = uint32;
const FIoFilenameHash INVALID_IO_FILENAME_HASH = 0;
/** Helpers to create the hash from a filename. Returns IOFILENAMEHASH_NONE if and only if the filename is empty. */
COREUOBJECT_API FIoFilenameHash MakeIoFilenameHash(const FString& Filename);
/** Helpers to create the hash from a FPackagePath. Returns IOFILENAMEHASH_NONE if and only if the PackagePath is empty. */
COREUOBJECT_API FIoFilenameHash MakeIoFilenameHash(const FPackagePath& Filename);
/** Helpers to create the hash from a chunk id. Returns IOFILENAMEHASH_NONE if and only if the chunk id is invalid. */
COREUOBJECT_API FIoFilenameHash MakeIoFilenameHash(const FIoChunkId& ChunkID);

/**
 * Represents an IO request from the BulkData streaming API.
 *
 * It functions pretty much the same as IAsyncReadRequest expect that it also holds
 * the file handle as well.
 */
class COREUOBJECT_API IBulkDataIORequest
{
public:
	virtual ~IBulkDataIORequest() {}

	virtual bool PollCompletion() const = 0;
	virtual bool WaitCompletion(float TimeLimitSeconds = 0.0f) = 0;

	virtual uint8* GetReadResults() = 0;
	virtual int64 GetSize() const = 0;

	virtual void Cancel() = 0;
};

/**
 * Flags serialized with the bulk data.
 */
enum EBulkDataFlags : uint32
{
	/** Empty flag set. */
	BULKDATA_None = 0,
	/**
	 * INTERNAL SET ONLY - callers of bulkdata should not set this flag on the bulk data
	 * It is overwritten according to global configuration by Serialize.
	 * If set, payload is stored not inline; it is stored either at the end of the file
	 * or in a separate file.
	 */
	BULKDATA_PayloadAtEndOfFile = 1 << 0,
	/** If set, payload should be [un]compressed using ZLIB during serialization. */
	BULKDATA_SerializeCompressedZLIB = 1 << 1,
	/** Force usage of SerializeElement over bulk serialization. */
	BULKDATA_ForceSingleElementSerialization = 1 << 2,
	/** Bulk data is only used once at runtime in the game. */
	BULKDATA_SingleUse = 1 << 3,
	/** Bulk data won't be used and doesn't need to be loaded. */
	BULKDATA_Unused = 1 << 5,
	/** Forces the payload to be saved inline, regardless of its size. */
	BULKDATA_ForceInlinePayload = 1 << 6,
	/** Flag to check if either compression mode is specified. */
	BULKDATA_SerializeCompressed = (BULKDATA_SerializeCompressedZLIB),
	/** Forces the payload to be always streamed, regardless of its size. */
	BULKDATA_ForceStreamPayload = 1 << 7,
	/**
	 * INTERNAL SET ONLY - callers of bulkdata should not set this flag on the bulk data
	 * It is overwritten according to global configuration by Serialize.
	 * If set, payload is stored in a separate file such as .ubulk.
	 * */
	BULKDATA_PayloadInSeperateFile = 1 << 8,
	/** DEPRECATED: If set, payload is compressed using platform specific bit window. */
	BULKDATA_SerializeCompressedBitWindow = 1 << 9,
	/** There is a new default to inline unless you opt out. */
	BULKDATA_Force_NOT_InlinePayload = 1 << 10,
	/** This payload is optional and may not be on device. */
	BULKDATA_OptionalPayload = 1 << 11,
	/** This payload will be memory mapped, this requires alignment, no compression etc. */
	BULKDATA_MemoryMappedPayload = 1 << 12,
	/** Bulk data size is 64 bits long. */
	BULKDATA_Size64Bit = 1 << 13,
	/** Duplicate non-optional payload in optional bulk data. */
	BULKDATA_DuplicateNonOptionalPayload = 1 << 14,
	/** Indicates that an old ID is present in the data, at some point when the DDCs are flushed we can remove this. */
	BULKDATA_BadDataVersion = 1 << 15,
	/** BulkData did not have it's offset changed during the cook and does not need the fix up at load time */
	BULKDATA_NoOffsetFixUp = 1 << 16,
	/**
	 * INTERNAL SET ONLY - callers of bulkdata should not set this flag on the bulk data
	 * If set, payload is stored in the workspace domain version of the file.
	 */
	BULKDATA_WorkspaceDomainPayload = 1 << 17,
	/**
	 * INTERNAL SET ONLY - callers of bulkdata should not set this flag on the bulk data
	 * If true, the BulkData can be loaded from its file at any time
	 */
	BULKDATA_LazyLoadable = 1 << 18,

	/* Runtime only flags below this point! Note that they take the high bits in reverse order! */

	/** Assigned at runtime to indicate that the BulkData should be using the IoDispatcher when loading, not filepaths. */
	BULKDATA_UsesIoDispatcher = 1u << 31u,
	/** Assigned at runtime to indicate that the BulkData allocation is a memory mapped region of a file and not raw data. */
	BULKDATA_DataIsMemoryMapped = 1 << 30,
	/** Assigned at runtime to indicate that the BulkData object has an async loading request in flight and will need to wait on it. */
	BULKDATA_HasAsyncReadPending = 1 << 29,
	/** Assigned at runtime to indicate that the BulkData object should be considered for discard even if it cannot load from disk. */
	BULKDATA_AlwaysAllowDiscard = 1 << 28,
};

/**
 * Allows FArchive to serialize EBulkDataFlags, this will not be required once EBulkDataFlags is promoted
 * to be a enum class.
 */
inline FArchive& operator<<(FArchive& Ar, EBulkDataFlags& Flags)
{
	Ar << (uint32&)Flags;
	return Ar;
}

/** Serialize the given Value as an int32 or int64 depending on InBulkDataFlags&BULKDATA_Size64Bit. */
inline void SerializeBulkDataSizeInt(FArchive& Ar, int64& Value, EBulkDataFlags InBulkDataFlags)
{
	if (InBulkDataFlags & BULKDATA_Size64Bit)
	{
		Ar << Value;
	}
	else
	{
		check(!Ar.IsSaving() || (MIN_int32 <= Value && Value <= MAX_int32));
		int32 ValueAsInt32 = static_cast<int32>(Value);
		Ar << ValueAsInt32;
		Value = ValueAsInt32;
	}
}

/**
 * Enumeration for bulk data lock status.
 */
enum EBulkDataLockStatus
{
	/** Unlocked array				*/
	LOCKSTATUS_Unlocked = 0,
	/** Locked read-only			*/
	LOCKSTATUS_ReadOnlyLock = 1,
	/** Locked read-write-realloc	*/
	LOCKSTATUS_ReadWriteLock = 2,
};

/**
 * Enumeration for bulk data lock behavior
 */
enum EBulkDataLockFlags
{
	LOCK_READ_ONLY = 1,
	LOCK_READ_WRITE = 2,
};

/**
 * Callback to use when making streaming requests
 */
typedef TFunction<void(bool bWasCancelled, IBulkDataIORequest*)> FBulkDataIORequestCallBack;

UE_DEPRECATED(5.1, "Use CreateStreamingRequest instead")
TUniquePtr<IBulkDataIORequest> CreateBulkDataIoDispatcherRequest(
	const FIoChunkId& InChunkID,
	int64 InOffsetInBulkData = 0,
	int64 InBytesToRead = INDEX_NONE,
	FBulkDataIORequestCallBack* InCompleteCallback = nullptr,
	uint8* InUserSuppliedMemory = nullptr,
	int32 InPriority = IoDispatcherPriority_Low);

/**
 * @documentation @todo documentation
 */
struct COREUOBJECT_API FOwnedBulkDataPtr
{
	explicit FOwnedBulkDataPtr(void* InAllocatedData)
		: AllocatedData(InAllocatedData)
		, MappedHandle(nullptr)
		, MappedRegion(nullptr)
	{
		
	}

	FOwnedBulkDataPtr(IMappedFileHandle* Handle, IMappedFileRegion* Region)
		: AllocatedData(nullptr)
		, MappedHandle(Handle)
		, MappedRegion(Region)
	{
		
	}

	~FOwnedBulkDataPtr();
	const void* GetPointer();

	IMappedFileHandle* GetMappedHandle()
	{
		return MappedHandle;
	}
	IMappedFileRegion* GetMappedRegion()
	{
		return MappedRegion;
	}

	void RelinquishOwnership()
	{
		AllocatedData = nullptr;
		MappedHandle = nullptr;
		MappedRegion = nullptr;
	}

private:
	// hidden
	FOwnedBulkDataPtr() {}


	// if allocated memory was used, this will be non-null
	void* AllocatedData;
	
	// if memory mapped IO was used, these will be non-null
	IMappedFileHandle* MappedHandle;
	IMappedFileRegion* MappedRegion;
};

UE_DEPRECATED(5.1, "Use CreateStreamingRequest instead");
class FBulkDataIORequest : public IBulkDataIORequest
{
public:
	FBulkDataIORequest(IAsyncReadFileHandle* InFileHandle);
	FBulkDataIORequest(IAsyncReadFileHandle* InFileHandle, IAsyncReadRequest* InReadRequest, int64 BytesToRead);

	virtual ~FBulkDataIORequest();

	bool MakeReadRequest(int64 Offset, int64 BytesToRead, EAsyncIOPriorityAndFlags PriorityAndFlags, FBulkDataIORequestCallBack* CompleteCallback, uint8* UserSuppliedMemory);

	virtual bool PollCompletion() const override;
	virtual bool WaitCompletion( float TimeLimitSeconds = 0.0f ) override;

	virtual uint8* GetReadResults() override;
	virtual int64 GetSize() const override;

	virtual void Cancel() override;

private:
	IAsyncReadFileHandle* FileHandle;
	IAsyncReadRequest* ReadRequest;

	int64 Size;
};

namespace UE::BulkData::Private
{

/**
 * Serialized bulk meta data.
 */
struct FBulkMetaResource
{
	/** Bulk data flags. */
	EBulkDataFlags Flags = BULKDATA_None;

	/** Number of bulk data elements. */
	int64 ElementCount = -1;

	/** Size on disk, can differ from size when compressed. */
	int64 SizeOnDisk = -1;
	
	/** Offset within the bulk data chunk. */
	int64 Offset = -1;
	
	/** Non-optional duplicate bulk data flags. */
	EBulkDataFlags DuplicateFlags = BULKDATA_None;
	
	/** Duplicate bulk data size on disk. */
	int64 DuplicateSizeOnDisk = -1;

	/** Offset within the optional bulk data chunk. */
	int64 DuplicateOffset = -1;

	COREUOBJECT_API friend FArchive& operator<<(FArchive& Ar, FBulkMetaResource& BulkMeta);
};

/**
 * Bulk meta data, size, offset, flags and lock status packed in 16 bytes.
 *
 * Uses 5 bytes for size and offset.
 * [0 - 4][5 - 9][  10	][    11	][12    -   15]([16   -  23])
 * [Size][Offset][MetaFlags][LockFlags][BulkDataFlags]([SizeOnDisk])
 */
class FBulkMetaData
{
public:
	enum class EMetaFlags : uint8
	{
		/** No additional bulk data flags. */
		None				= 0,
		/** Loading from a cooked package. */
		CookedPackage		= (1 << 0),
		/** Loading optional package data. */
		OptionalPackage		= (1 << 1)
	};
	FRIEND_ENUM_CLASS_FLAGS(EMetaFlags);

	/** 40 bits for max bulk data size. */
	static constexpr int64 MaxSize = 0xFFffFFffFF;
	/** 39 bits for max bulk data offset and 1 bit to indicate INDEX_NONE. */
	static constexpr int64 MaxOffset = 0xFFffFFffFE;

	FBulkMetaData()
	{
		SetOffset(-1);
#if !USE_RUNTIME_BULKDATA
		SetSizeOnDisk(-1);
#endif
	}

	explicit FBulkMetaData(EBulkDataFlags Flags)
		: FBulkMetaData()
	{
		SetFlags(Flags);
	}

	inline int64 GetSize() const
	{
		return static_cast<int64>(ReadUInt40(Data));
	}

	inline void SetSize(int64 Size)
	{
		check(Size <= MaxSize);
		WriteUInt40(Data, uint64(Size));
	}

	int64 GetSizeOnDisk() const
	{
#if USE_RUNTIME_BULKDATA
		return GetSize();
#else
		return static_cast<int64>(*reinterpret_cast<const int64*>(&Data[16]));
#endif
	}

	void SetSizeOnDisk(int64 SizeOnDisk)
	{
#if !USE_RUNTIME_BULKDATA
		*reinterpret_cast<int64*>(&Data[16]) = SizeOnDisk;
#endif // !USE_RUNTIME_BULKDATA
	}

	inline int64 GetOffset() const
	{
		const int64 Offset = static_cast<int64>(ReadUInt40(Data + 5));
		return Offset > MaxOffset ? INDEX_NONE : Offset;
	}

	inline void SetOffset(int64 Offset)
	{
		check(Offset <= MaxOffset);

		if (Offset < 0)
		{
			Offset = MAX_int64;
		}

		WriteUInt40(Data + 5, uint64(Offset));
	}

	EBulkDataLockStatus GetLockStatus() const
	{
		return static_cast<EBulkDataLockStatus>(Data[11]);
	}

	void SetLockStatus(EBulkDataLockStatus Status)
	{
		Data[11] = uint8(Status);
	}
	
	EBulkDataFlags GetFlags() const
	{
		return static_cast<EBulkDataFlags>(*reinterpret_cast<const uint32*>(&Data[12]));
	}

	void SetFlags(EBulkDataFlags Flags)
	{
		*reinterpret_cast<uint32*>(&Data[12]) = static_cast<uint32>(Flags);
	}

	inline bool HasAnyFlags(EBulkDataFlags Flags) const
	{
		return (GetFlags() & Flags) != 0;
	}

	inline bool HasAllFlags(EBulkDataFlags Flags) const
	{
		return (GetFlags() & Flags) == Flags;
	}

	void SetMetaFlags(EMetaFlags MetaFlags)
	{
		Data[10] = static_cast<uint8>(MetaFlags);
	}
	
	EMetaFlags GetMetaFlags() const
	{
		return static_cast<EMetaFlags>(Data[10]);
	}
	
	static FBulkMetaData FromSerialized(const FBulkMetaResource& MetaResource, int64 ElementSize)
	{
		FBulkMetaData Meta;
		
		if (MetaResource.ElementCount > 0)
		{
			Meta.SetSize(MetaResource.ElementCount * ElementSize);
		}

		Meta.SetSizeOnDisk(MetaResource.SizeOnDisk);
		Meta.SetOffset(MetaResource.Offset);
		Meta.SetFlags(MetaResource.Flags);

		return Meta;
	}

private:
	static uint64 ReadUInt40(const uint8* Memory)
	{
		return static_cast<int64>(uint64(Memory[0]) | uint64(Memory[1]) << 8 | uint64(Memory[2]) << 16 | uint64(Memory[3]) << 24 | uint64(Memory[4]) << 32);
	}

	static void WriteUInt40(uint8* Memory, uint64 Value)
	{
		Memory[0] = uint8(Value); Memory[1] = uint8(Value >> 8); Memory[2] = uint8(Value >> 16); Memory[3] = uint8(Value >> 24); Memory[4] = uint8(Value >> 32);
	}
#if USE_RUNTIME_BULKDATA
	uint8 Data[16] = {0};
#else
	uint8 Data[24] = {0};
#endif // USE_RUNTIME_BULKDATA
};

/**
 * Chunk identifier when reading non-inlined bulk data. Either
 * a package path or a package ID depending on the package loader.
 */
class FBulkDataChunkId
{
public:
	COREUOBJECT_API FBulkDataChunkId();

	COREUOBJECT_API FBulkDataChunkId(const FBulkDataChunkId& Other);

	COREUOBJECT_API FBulkDataChunkId(FBulkDataChunkId&&) = default;

	COREUOBJECT_API ~FBulkDataChunkId();

	COREUOBJECT_API FBulkDataChunkId& operator=(const FBulkDataChunkId& Other);

	COREUOBJECT_API FBulkDataChunkId& operator=(FBulkDataChunkId&&) = default;
	
	COREUOBJECT_API bool operator==(const FBulkDataChunkId& Other) const;
	
	bool IsValid() const { return Impl.IsValid(); }

	FPackageId GetPackageId() const;
	
	COREUOBJECT_API const FPackagePath& GetPackagePath() const;

	COREUOBJECT_API FIoFilenameHash GetIoFilenameHash(EBulkDataFlags BulkDataFlags) const;

	FString ToDebugString() const;

	static FBulkDataChunkId FromPackagePath(const FPackagePath& PackagePath);

	static FBulkDataChunkId FromPackageId(const FPackageId& PackageId);

private:
	struct FImpl;

	FBulkDataChunkId(TPimplPtr<FImpl>&& InImpl);

	TPimplPtr<FImpl> Impl;
};

/** Returns an I/O chunk ID to be used when loading from I/O store. */
FIoChunkId CreateBulkDataIoChunkId(const FBulkMetaData& BulkMeta, const FPackageId& PackageId);

/** Returns the package segment when loading from the package resource manager. */
COREUOBJECT_API EPackageSegment GetPackageSegmentFromFlags(const FBulkMetaData& BulkMeta);

} // namespace UE::BulkData::Private

/**
 * @documentation @todo documentation
 */
class COREUOBJECT_API FBulkData
{
	// This struct represents an optional allocation.
	struct FAllocatedPtr
	{
		FAllocatedPtr() = default;
		~FAllocatedPtr() = default;

		inline bool IsLoaded() const
		{
			return Allocation.RawData != nullptr; // Doesn't matter which allocation we test
		}

		void Free(FBulkData* Owner);

		void* ReallocateData(FBulkData* Owner, SIZE_T SizeInBytes);

		void SetData(FBulkData* Owner, void* Buffer);
		void SetMemoryMappedData(FBulkData* Owner, IMappedFileHandle* MappedHandle, IMappedFileRegion* MappedRegion);

		void* GetAllocationForWrite(const FBulkData* Owner) const;
		const void* GetAllocationReadOnly(const FBulkData* Owner) const;

		FOwnedBulkDataPtr* StealFileMapping(FBulkData* Owner);
		void Swap(FBulkData* Owner, void** DstBuffer);

	private:

		union FAllocation
		{
			/** Raw memory allocations, allocated via FMemory::Malloc/Realloc */
			void* RawData;
			/** Wrapper around memory mapped allocations allocated via new */
			FOwnedBulkDataPtr* MemoryMappedData;
		};

		FAllocation Allocation{ nullptr };
	};

public:
	friend class FLinkerLoad;
	friend class FExportArchive;
	friend class UE::Serialization::FEditorBulkData; // To allow access to AttachedAr
	friend class FBulkDataBatchRequest;

	using BulkDataRangeArray = TArray<FBulkData*, TInlineAllocator<8>>;

	static constexpr SIZE_T MaxBulkDataSize = (1ull << 40) - 1;

	/*-----------------------------------------------------------------------------
		Constructors and operators
	-----------------------------------------------------------------------------*/

	/**
	 * Constructor, initializing all member variables.
	 */
	FBulkData() = default;

	/**
	 * Copy constructor. Use the common routine to perform the copy.
	 *
	 * @param Other the source array to copy
	 */
	FBulkData( const FBulkData& Other );

	/**
	 * Virtual destructor, free'ing allocated memory.
	 */
	~FBulkData();

	/**
	 * Copies the source array into this one after detaching from archive.
	 *
	 * @param Other the source array to copy
	 */
	FBulkData& operator=( const FBulkData& Other );

	/*-----------------------------------------------------------------------------
		Static functions.
	-----------------------------------------------------------------------------*/

	/**
	 * Dumps detailed information of bulk data usage.
	 *
	 * @param Log FOutputDevice to use for logging
	 */
	static void DumpBulkDataUsage( FOutputDevice& Log );

	/*-----------------------------------------------------------------------------
		Accessors
	-----------------------------------------------------------------------------*/

	/**
	 * Returns the size of the bulk data in bytes.
	 *
	 * @return Size of the bulk data in bytes
	 */
	int64 GetBulkDataSize() const;
	/**
	 * Returns the size of the bulk data on disk. This can differ from GetBulkDataSize if
	 * BULKDATA_SerializeCompressed is set.
	 *
	 * @return Size of the bulk data on disk or INDEX_NONE in case there's no association
	 */
	int64 GetBulkDataSizeOnDisk() const;
	/**
	 * Returns the offset into the file the bulk data is located at.
	 *
	 * @return Offset into the file or INDEX_NONE in case there is no association
	 */
	int64 GetBulkDataOffsetInFile() const;
	/**
	 * Returns whether the bulk data is stored compressed on disk.
	 *
	 * @return true if data is compressed on disk, false otherwise
	 */
	bool IsStoredCompressedOnDisk() const;

	/**
	 * Returns true if the data can be loaded from disk.
	 */
	bool CanLoadFromDisk() const;
	
	/**
	 * Returns true if the data references a file that currently exists and can be referenced by the file system.
	 */
	bool DoesExist() const;

	/**
	 * Returns flags usable to decompress the bulk data
	 * 
	 * @return NAME_None if the data was not compressed on disk, or valid format to pass to FCompression::UncompressMemory for this data
	 */
	FName GetDecompressionFormat() const;

	/**
	 * Returns whether the bulk data is currently loaded and resident in memory.
	 *
	 * @return true if bulk data is loaded, false otherwise
	 */
	bool IsBulkDataLoaded() const;

	/**
	* Returns whether the bulk data asynchronous load has completed.
	*
	* @return true if bulk data has been loaded or async loading was not used to load this data, false otherwise
	*/
	bool IsAsyncLoadingComplete() const;

	/**
	* Returns whether this bulk data is used
	* @return true if BULKDATA_Unused is not set
	*/
	bool IsAvailableForUse() const;

	/**
	* Returns whether this bulk data represents optional data or not
	* @return true if BULKDATA_OptionalPayload is set
	*/
	bool IsOptional() const
	{
		return (GetBulkDataFlags() & BULKDATA_OptionalPayload) != 0;
	}

	/**
	* Returns whether this bulk data is currently stored inline or not
	* @return true if BULKDATA_PayloadAtEndOfFile is not set
	*/
	bool IsInlined() const
	{
		return   (GetBulkDataFlags() & BULKDATA_PayloadAtEndOfFile) == 0;
	}

	UE_DEPRECATED(4.25, "Use ::IsInSeparateFile() instead")
	inline bool InSeperateFile() const { return IsInSeparateFile(); }

	/**
	* Returns whether this bulk data is currently stored in it's own file or not
	* @return true if BULKDATA_PayloadInSeperateFile is not set
	*/
	bool IsInSeparateFile() const
	{
		return	(GetBulkDataFlags() & BULKDATA_PayloadInSeperateFile) != 0;
	}

	/**
	 * Returns whether this bulk data is stored in a PackageExternalResource rather than in
	 * a neighboring segment of its owner's PackagePath. 
	 */
	bool IsInExternalResource() const
	{
		return IsInSeparateFile() && (GetBulkDataFlags() & BULKDATA_WorkspaceDomainPayload);
	}

	/**
	* Returns whether this bulk data is accessed via the IoDispatcher or not.
	* @return false as the old BulkData API does not support it
	*/
	bool IsUsingIODispatcher() const
	{
		return (GetBulkDataFlags() & BULKDATA_UsesIoDispatcher) != 0;
	}

	/**
	 * Returns whether this bulk data is memory mapped or not.
	 */
	bool IsDataMemoryMapped() const
	{
		return (GetBulkDataFlags() & BULKDATA_DataIsMemoryMapped) != 0;
	}

	/**
	 * Returns whether to deallocate the bulk data after lock
	 */
	bool IsSingleUse() const
	{
		return (GetBulkDataFlags() & BULKDATA_SingleUse) != 0;
	}
	
	/**
	* Returns whether this bulk data represents duplicate non-optional data or not
	* @return true if BULKDATA_DuplicateNonOptionalPayload is set
	*/
	bool IsDuplicateNonOptional() const
	{
		return (GetBulkDataFlags() & BULKDATA_DuplicateNonOptionalPayload) != 0;
	}

	/**
	 * Enables the given flags without affecting any previously set flags.
	 *
	 * @param BulkDataFlagsToSet	Bulk data flags to set
	 */
	void SetBulkDataFlags( uint32 BulkDataFlagsToSet );

	/**
	 * Enable the given flags and disable all other flags.
	 * This can be used with ::GetBulkDataFlags() to effectively
	 * reset the flags to a previous state.
	 *
	 * @param BulkDataFlagsToSet	Bulk data flags to set
	 */
	void ResetBulkDataFlags(uint32 BulkDataFlagsToSet);

	/**
	* Gets the current bulk data flags.
	*
	* @return Bulk data flags currently set
	*/
	uint32 GetBulkDataFlags() const;

	UE_DEPRECATED(5.1, "Bulk Data will always use default alignment")
	void SetBulkDataAlignment(uint16 BulkDataAlignmentToSet);

	/**
	* Gets the current bulk data alignment.
	*
	* @return Bulk data alignment currently set
	*/
	uint32 GetBulkDataAlignment() const;

	/**
	 * Clears the passed in bulk data flags.
	 *
	 * @param BulkDataFlagsToClear	Bulk data flags to clear
	 */
	void ClearBulkDataFlags( uint32 BulkDataFlagsToClear );

	UE_DEPRECATED(5.0, "Use GetPackagePath instead")
	FString GetFilename() const
	{ 
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		return GetPackagePath().GetLocalFullPath(GetPackageSegment());
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	/** Returns the PackagePath this bulkdata resides in */
	UE_DEPRECATED(5.1, "Deprecated")
	const FPackagePath& GetPackagePath() const { return BulkChunkId.GetPackagePath(); }

	/** Returns which segment of its PackagePath this bulkdata resides in */
	UE_DEPRECATED(5.1, "Deprecated")
	EPackageSegment GetPackageSegment() const { return UE::BulkData::Private::GetPackageSegmentFromFlags(BulkMeta); }

	/** 
	 * Returns the io filename hash associated with this bulk data.
	 *
	 * @return Hash or INVALID_IO_FILENAME_HASH if invalid.
	 **/
	FIoFilenameHash GetIoFilenameHash() const { return BulkChunkId.GetIoFilenameHash(BulkMeta.GetFlags()); }
	
	/** Returns a FIoChunkId for the bulkdata payload, this will be invalid if the bulkdata is not stored in the IoStore */
	FIoChunkId CreateChunkId() const;

	/** Returns a string representing the bulk data for debugging purposes. */
	FString GetDebugName() const;

	/*-----------------------------------------------------------------------------
		Data retrieval and manipulation.
	-----------------------------------------------------------------------------*/

	/**
	 * Retrieves a copy of the bulk data.
	 *
	 * @param Dest [in/out] Pointer to pointer going to hold copy, can point to NULL pointer in which case memory is allocated
	 * @param bDiscardInternalCopy Whether to discard/ free the potentially internally allocated copy of the data
	 */
	void GetCopy( void** Dest, bool bDiscardInternalCopy = true );

	/**
	 * Locks the bulk data and returns a pointer to it.
	 *
	 * @param	LockFlags	Flags determining lock behavior
	 */
	void* Lock( uint32 LockFlags );

	/**
	 * Locks the bulk data and returns a read-only pointer to it.
	 * This variant can be called on a const bulkdata
	 */
	const void* LockReadOnly() const;

	/**
	 * Change size of locked bulk data. Only valid if locked via read-write lock.
	 *
	 * @param ElementCount	Number of elements to allocate.
	 * @param ElementSize	Size in bytes of the individual element.
	 */
	void* Realloc(int64 ElementCount, int64 ElementSize);

	/** 
	 * Unlocks bulk data after which point the pointer returned by Lock no longer is valid.
	 */
	void Unlock() const;

	/** 
	 * Checks if this bulk is locked
	 */
	bool IsLocked() const { return BulkMeta.GetLockStatus() != LOCKSTATUS_Unlocked; }
	
	/** 
	 * Checks if this bulk is unlocked
	 */
	bool IsUnlocked() const { return BulkMeta.GetLockStatus() == LOCKSTATUS_Unlocked; }

	/**
	 * Clears/removes any currently allocated data payload and resets element count to 0.
	 * 
	 * Note that once this has been called, the bulkdata object will no longer be able to reload
	 * it's payload from disk!
	 */
	void RemoveBulkData();

#if !USE_RUNTIME_BULKDATA
	/**
	 * Load the bulk data using a file reader. Works even when no archive is attached to the bulk data..
  	 * @return Whether the operation succeeded.
	 */
	bool LoadBulkDataWithFileReader();

	/**
	 * Test if it is possible to load the bulk data using a file reader, even when no archive is attached to the bulk data.
	 * @return Whether the operation is allowed.
	 */
	bool CanLoadBulkDataWithFileReader() const;
#endif // USE_RUNTIME_BULKDATA

	/**
	 * Forces the bulk data to be resident in memory and detaches the archive.
	 */
	void ForceBulkDataResident();

	/** 
	* Initiates a new asynchronous operation to load the dulkdata from disk assuming that it is not already
	* loaded.
	* Note that a new asynchronous loading operation will not be created if one is already in progress.
	*
	* @return True if an asynchronous loading operation is in progress by the time that the method returns
	* and false if the data is already loaded or cannot be loaded from disk.
	*/
	UE_DEPRECATED(5.1, "Use FBulkDataRequest or CreateStreamingRequest instead")
	bool StartAsyncLoading();
	
	/**
	 * Sets whether we should store the data compressed on disk.
	 *
	 * @param CompressionFlags	Flags to use for compressing the data. Use COMPRESS_NONE for no compression, or something like COMPRESS_ZLIB to compress the data
	 */
	UE_DEPRECATED(4.21, "Use the FName version of StoreCompressedOnDisk")
	void StoreCompressedOnDisk( ECompressionFlags CompressionFlags );
	void StoreCompressedOnDisk( FName CompressionFormat );

	/**
	 * Deallocates bulk data without detaching the archive, so that further bulk data accesses require a reload.
	 * Only supported in editor builds.
	 *
	 * @return Whether the operation succeeded.
	 */
	bool UnloadBulkData();

	/*-----------------------------------------------------------------------------
		Serialization.
	-----------------------------------------------------------------------------*/

	using FSerializeBulkDataElements = TFunction<void(FArchive&, void*, int64, EBulkDataFlags)>;
	
	/**
	 * Serialize function used to serialize this bulk data structure.
	 *
	 * @param Ar	Archive to serialize with
	 * @param Owner	Object owning the bulk data
	 * @param Idx	Index of bulk data item being serialized
	 * @param bAttemptFileMapping	If true, attempt to map this instead of loading it into malloc'ed memory
	 * @param FileRegionType	When cooking, a hint describing the type of data, used by some platforms to improve compression ratios
	 */
	void Serialize(FArchive& Ar, UObject* Owner, bool bAttemptFileMapping, int32 ElementSize, EFileRegionType FileRegionType);
	
	/**
	 * Serialize just the bulk data portion to/ from the passed in memory.
	 *
	 * @param	Ar					Archive to serialize with
	 * @param	Data				Memory to serialize either to or from
	 * @param	InBulkDataFlags		Flags describing how the data was/shouldbe serialized
	 */
	void SerializeBulkData(FArchive& Ar, void* Data, EBulkDataFlags InBulkDataFlags)
	{
		SerializeBulkData(Ar, Data, GetBulkDataSize(), InBulkDataFlags);
	}

	UE_DEPRECATED(5.0, "Use the version that takes InBulkDataFlags")
	void SerializeBulkData(FArchive& Ar, void* Data)
	{
		SerializeBulkData(Ar, Data, static_cast<EBulkDataFlags>(GetBulkDataFlags()));
	}

	FOwnedBulkDataPtr* StealFileMapping()
	{
		// @todo if non-mapped bulk data, do we need to detach this, or mimic GetCopy more than we do?
		return DataAllocation.StealFileMapping(this); 
	}

	UE_DEPRECATED(5.1, "Call GetBulkDataVersions instead.")
	FCustomVersionContainer GetCustomVersions(FArchive& InlineArchive) const;

	/**
	 * Get the CustomVersions used in the file containing the BulkData payload. If !IsInSeparateFile, this will be
	 * the custom versions from the archive used to serialize the FBulkData, which the caller must provide.
	 * Otherwise, the CustomVersions come from the separate file and this function will look them up.
	 *
	 * @param InlineArchive The archive that was used to load this object
	 *
	 */
	void GetBulkDataVersions(FArchive& InlineArchive, FPackageFileVersion& OutUEVersion, int32& OutLicenseeUEVersion,
		FCustomVersionContainer& OutCustomVersions) const;

#if WITH_EDITOR
	/**
	 * When saving BulkData, if we are overwriting the file we need to update the BulkData's (flags,offset,size) to be
	 * able to load from the new file. But SerializeBulkData modifies those values when loading, so the in-memory values after
	 * loading from disk are not the same as the values on disk.
	 * This function handles running the same steps that SerializeBulkData does, but skips the deserialization of the BulkData.
	 * */
	void SetFlagsFromDiskWrittenValues(EBulkDataFlags InBulkDataFlags, int64 InBulkDataOffsetInFile, int64 InBulkDataSizeOnDisk, int64 LinkerSummaryBulkDataStartOffset);
#endif

	/*-----------------------------------------------------------------------------
		Async Streaming Interface.
	-----------------------------------------------------------------------------*/

	/**
	 * Opens a new IAsyncReadFileHandle that references the file that the BulkData object represents.
	 *
	 * @return A valid handle if the file can be accessed, if it cannot then nullptr.
	 */
	IAsyncReadFileHandle* OpenAsyncReadHandle() const;

	/**
	 * Create an async read request for the bulk data.
	 * This version will load the entire data range that the FBulkData represents.
	 *
	 * @param Priority				Priority and flags of the request. If this includes AIOP_FLAG_PRECACHE, then memory will never be returned. The request should always be canceled and waited for, even for a precache request.
	 * @param CompleteCallback		Called from an arbitrary thread when the request is complete. Can be nullptr, if non-null, must remain valid until it is called. It will always be called.
	 * @param UserSuppliedMemory	A pointer to memory for the IO request to be written to, it is up to the caller to make sure that it is large enough. If the pointer is null then the system will allocate memory instead.
	 * @return						A request for the read. This is owned by the caller and must be deleted by the caller.
	 */
	IBulkDataIORequest* CreateStreamingRequest(EAsyncIOPriorityAndFlags Priority, FBulkDataIORequestCallBack* CompleteCallback, uint8* UserSuppliedMemory) const;

	/**
	 * Create an async read request for the bulk data.
	 * This version allows the user to request a subset of the data that the FBulkData represents.
	 *
	 * @param OffsetInBulkData		Offset into the bulk data to start reading from.
	 * @param BytesToRead			The number of bytes to read. If this request is AIOP_Preache, the size can be anything, even MAX_int64, otherwise the size and offset must be fully contained in the file.
	 * @param Priority				Priority and flags of the request. If this includes AIOP_FLAG_PRECACHE, then memory will never be returned. The request should always be canceled and waited for, even for a precache request.
	 * @param CompleteCallback		Called from an arbitrary thread when the request is complete. Can be nullptr, if non-null, must remain valid until it is called. It will always be called.
	 * @param UserSuppliedMemory	A pointer to memory for the IO request to be written to, it is up to the caller to make sure that it is large enough. If the pointer is null then the system will allocate memory instead.
	 * @return						A request for the read. This is owned by the caller and must be deleted by the caller.
	 */
	IBulkDataIORequest* CreateStreamingRequest(int64 OffsetInBulkData, int64 BytesToRead, EAsyncIOPriorityAndFlags Priority, FBulkDataIORequestCallBack* CompleteCallback, uint8* UserSuppliedMemory) const;

	/**
	 * Create an async read request for a range of bulk data streaming tokens
	 * The request will read all data between the two given streaming tokens objects. They must both represent areas of data in the file!
	 * There is no way to validate this and it is up to the caller to make sure that it is correct.
	 * The memory to be read into will be automatically allocated the size of which can be retrieved by calling IBulkDataIORequest::GetSize()
	 *
	 * @param Start				The bulk data to start reading from.
	 * @param End				The bulk data to finish reading from.
	 * @param Priority			Priority and flags of the request. If this includes AIOP_FLAG_PRECACHE, then memory will never be returned. The request should always be canceled and waited for, even for a precache request.
	 * @param CompleteCallback	Called from an arbitrary thread when the request is complete. Can be nullptr, if non-null, must remain valid until it is called. It will always be called.
	 * @return					A request for the read. This is owned by the caller and must be deleted by the caller.
	**/
	UE_DEPRECATED(5.1, "Use FBulkDataRequest instead")
	static IBulkDataIORequest* CreateStreamingRequestForRange(const BulkDataRangeArray& RangeArray, EAsyncIOPriorityAndFlags Priority, FBulkDataIORequestCallBack* CompleteCallback);

	/** Enable the given flags in the given accumulator variable. */
	static void SetBulkDataFlagsOn(EBulkDataFlags& InOutAccumulator, EBulkDataFlags FlagsToSet);
	/** Disable the given flags in the given accumulator variable. */
	static void ClearBulkDataFlagsOn(EBulkDataFlags& InOutAccumulator, EBulkDataFlags FlagsToClear);
	/** Returns decompress method flags specified by the given bulk data flags. */
	static FName GetDecompressionFormat(EBulkDataFlags InFlags);

	/*-----------------------------------------------------------------------------
		Class specific virtuals.
	-----------------------------------------------------------------------------*/

protected:

	void SerializeBulkData(FArchive& Ar, void* Data, int64 DataSize, EBulkDataFlags InBulkDataFlags);

private:
#if WITH_EDITOR
	/**
	 * Detaches the bulk data from the passed in archive. Needs to match the archive we are currently
	 * attached to.
	 *
	 * @param Ar						Archive to detach from
	 * @param bEnsureBulkDataIsLoaded	whether to ensure that bulk data is load before detaching from archive
	 */
	void DetachFromArchive( FArchive* Ar, bool bEnsureBulkDataIsLoaded );
#endif // WITH_EDITOR

	/*-----------------------------------------------------------------------------
		Internal helpers
	-----------------------------------------------------------------------------*/

	/**
	 * Loads the bulk data if it is not already loaded.
	 */
	void MakeSureBulkDataIsLoaded();

	/**
	 * Loads the data from disk into the specified memory block. This requires us still being attached to an
	 * archive we can use for serialization.
	 *
	 * @param Dest Memory to serialize data into
	 *
	 * @return Whether the load succeeded
	 */
	bool TryLoadDataIntoMemory(FIoBuffer Dest);

	/** Flushes any pending async load of bulk data  and copies the data to Dest buffer*/
	void FlushAsyncLoading();

	/** Returns if the offset needs fixing when serialized */
	bool NeedsOffsetFixup() const;
	
	bool CanDiscardInternalData() const;
	
	/** Sets whether inline bulk data is allowed to be unloaded or not */
	void ConditionalSetInlineAlwaysAllowDiscard(bool bPackageUsesIoStore);

	/** Reallocate bulk data */
	inline void* ReallocateData(SIZE_T SizeInBytes) { return DataAllocation.ReallocateData(this, SizeInBytes); }

	/** Free bulk data */
	inline void  FreeData() { DataAllocation.Free(this); }

	/** Retrieve the internal allocation for writing */
	inline void* GetDataBufferForWrite() const { return DataAllocation.GetAllocationForWrite(this); }

	/** Retrieve the internal allocation for reading */
	inline const void* GetDataBufferReadOnly() const { return DataAllocation.GetAllocationReadOnly(this); }

	/*-----------------------------------------------------------------------------
		Member variables.
	-----------------------------------------------------------------------------*/
	
	FAllocatedPtr								DataAllocation;
	UE::BulkData::Private::FBulkMetaData		BulkMeta;
	UE::BulkData::Private::FBulkDataChunkId		BulkChunkId;
	
protected:
#if WITH_EDITOR
	/** Archive associated with bulk data for serialization																*/
	FArchive*									AttachedAr = nullptr;
	/** Used to make sure the linker doesn't get garbage collected at runtime for things with attached archives			*/
	FLinkerLoad*								Linker = nullptr;
#endif // WITH_EDITOR
	   //
#if !USE_RUNTIME_BULKDATA
	/** Custom bulk data serialization hook when using FUntypedBulkData. */
	FSerializeBulkDataElements* SerializeBulkDataElements = nullptr;
#endif // !USE_RUNTIME_BULKDATA
};

/**
 * Templated bulk data.
 *
 * A bulk data array of typed elements.
 */
template<typename ElementType>
class TBulkData : public FBulkData
{
public:

	static_assert(TIsPODType<ElementType>::Value, "Bulk data is limited to POD types");

	/**
	 * Element size in bytes.
	 *
	 * @return Returns the element size in bytes.
	 */
	int32 GetElementSize() const
	{ 
		return sizeof(ElementType);
	}

	/**
	 * Returns the number of elements in this bulk data array.
	 *
	 * @return Number of elements in this bulk data array
	 */
	int64 GetElementCount() const
	{
		return GetBulkDataSize() / sizeof(ElementType);
	}

	/**
	 * Change size of locked bulk data. Only valid if locked via read-write lock.
	 *
	 * @param ElementCount	Number of elements array should be resized to
	 */
	ElementType* Realloc(int64 ElementCount)
	{
		return (ElementType*)FBulkData::Realloc(ElementCount, sizeof(ElementType));
	}

	/**
	 * Returns a copy encapsulated by a FBulkDataBuffer.
	 *
	 * @param RequestedElementCount If set to greater than 0, the returned FBulkDataBuffer will be limited to
	 * this number of elements. This will give an error if larger than the actual number of elements in the BulkData object.
	 * @param bDiscardInternalCopy If true then the BulkData object will free it's internal buffer once called.
	 *
	 * @return A FBulkDataBuffer that owns a copy of the BulkData, this might be a subset of the data depending on the value of RequestedSize.
	 */
	FBulkDataBuffer<ElementType> GetCopyAsBuffer(int64 RequestedElementCount, bool bDiscardInternalCopy)
	{ 
		const int64 MaxElementCount = GetElementCount();

		check(RequestedElementCount <= MaxElementCount);

		ElementType* Ptr = nullptr;
		GetCopy((void**)& Ptr, bDiscardInternalCopy);

		const int64 BufferSize = (RequestedElementCount > 0 ? RequestedElementCount : MaxElementCount);

		return FBulkDataBuffer<ElementType>(Ptr, BufferSize);
	}

	/**
	 * Serialize function used to serialize this bulk data structure.
	 *
	 * @param Ar	Archive to serialize with
	 * @param Owner	Object owning the bulk data
	 * @param Idx	Index of bulk data item being serialized
	 * @param bAttemptFileMapping	If true, attempt to map this instead of loading it into malloc'ed memory
	 * @param FileRegionType	When cooking, a hint describing the type of data, used by some platforms to improve compression ratios
	 */
	void Serialize(FArchive& Ar, UObject* Owner, int32 Idx=INDEX_NONE, bool bAttemptFileMapping = false, EFileRegionType FileRegionType = EFileRegionType::None)
	{
		FBulkData::Serialize(Ar, Owner, bAttemptFileMapping, GetElementSize(), FileRegionType);
	}
};

UE_DEPRECATED(5.1, "Use FBulkData/TBulkData");
struct COREUOBJECT_API FUntypedBulkData : public FBulkData
{
	virtual ~FUntypedBulkData() = default;

	/**
	 * Returns the number of elements allocated.
	 *
	 * @return Number of elements
	 */
	int64 GetElementCount() const
	{
		return GetBulkDataSize() / GetElementSize();
	}

	/**
	 * Returns size in bytes of single element.
	 *
	 * Pure virtual that needs to be overloaded in derived classes.
	 *
	 * @return Size in bytes of single element
	 */
	virtual int32 GetElementSize() const = 0;
	
	/**
	 * Change size of locked bulk data. Only valid if locked via read-write lock.
	 *
	 * @param ElementCount	Number of elements to allocate.
	 */
	void* Realloc(int64 ElementCount)
	{
		return FBulkData::Realloc(ElementCount, GetElementSize());
	}

	void Serialize(FArchive& Ar, UObject* Owner, int32 Idx=INDEX_NONE, bool bAttemptFileMapping = false, EFileRegionType FileRegionType = EFileRegionType::None)
	{
		FBulkData::Serialize(Ar, Owner, bAttemptFileMapping, GetElementSize(), FileRegionType);
	}

protected:

	FUntypedBulkData()
	{
		SerializeElementsCallback = [this](FArchive& Ar, void* Data, int64 Size, EBulkDataFlags InBulkDataFlags)
		{
			SerializeBulkData(Ar, Data, Size, InBulkDataFlags);
		};
#if !USE_RUNTIME_BULKDATA
		SerializeBulkDataElements = &SerializeElementsCallback;
#endif // !USE_RUNTIME_BULKDATA
	}

	virtual void SerializeElements(FArchive& Ar, void* Data);
	
	virtual void SerializeElement(FArchive& Ar, void* Data, int64 ElementIndex) = 0;

	virtual bool RequiresSingleElementSerialization(FArchive& Ar)
	{
		return false;
	}
	
private:

	void SerializeBulkData(FArchive& Ar, void* Data, int64 BulkDataSize, EBulkDataFlags InBulkDataFlags);

	FSerializeBulkDataElements SerializeElementsCallback;
};

using FBulkDataInterface UE_DEPRECATED(5.1, "FBulkDataInterface is deprecated. Use FBulkData") = FBulkData;
using FByteBulkData = TBulkData<uint8>;
using FWordBulkData = TBulkData<uint16>;
using FIntBulkData = TBulkData<int32>;
using FFloatBulkData = TBulkData<float>;

class FFormatContainer
{
	friend class UBodySetup;

	TSortedMap<FName, FByteBulkData*, FDefaultAllocator, FNameFastLess> Formats;
public:
	~FFormatContainer()
	{
		FlushData();
	}
	bool Contains(FName Format) const
	{
		return Formats.Contains(Format);
	}
	void GetContainedFormats(TArray<FName>& OutFormats) const
	{
		Formats.GenerateKeyArray(OutFormats);
	}
	FByteBulkData& GetFormat(FName Format)
	{
		FByteBulkData* Result = Formats.FindRef(Format);
		if (!Result)
		{
			Result = new FByteBulkData;
			Formats.Add(Format, Result);
		}
		return *Result;
	}
	void FlushData()
	{
		for (const TPair<FName, FByteBulkData*>& Format : Formats)
		{
			FByteBulkData* BulkData = Format.Value;
			delete BulkData;
		}
		Formats.Empty();
	}
	COREUOBJECT_API void Serialize(FArchive& Ar, UObject* Owner, const TArray<FName>* FormatsToSave = nullptr, bool bSingleUse = true, uint16 InAlignment = DEFAULT_ALIGNMENT, bool bInline = true, bool bMapped = false);
	COREUOBJECT_API void SerializeAttemptMappedLoad(FArchive& Ar, UObject* Owner);
};

/** Handle to a bulk data I/O request. */
class COREUOBJECT_API FBulkDataRequest
{
public:
	class IHandle;

	/** Bulk data request status. */
	enum class EStatus : uint32
	{
		/** The request hasn't been issued. */
		None,
		/** The request is pending. */
		Pending,
		/** The request has been completed successfully. */
		Ok,
		/** The request was cancelled. */
		Cancelled,
		/** An error occured while issuing the request. */
		Error
	};

	/** Completion callback. */
	using FCompletionCallback = TFunction<void(EStatus)>;

	/** Default bulk data I/O request priority. */
	static constexpr EAsyncIOPriorityAndFlags DefaultPriority = AIOP_BelowNormal;

	/** Constructs a new handle to bulk data request. */
	FBulkDataRequest();

	/** Destructor, cancels and waits for any pending requests. */
	~FBulkDataRequest();

	/** Moves ownership from an invalid or pending request. */
	FBulkDataRequest(FBulkDataRequest&&);

	/** Moves ownership from an invalid or pending request. */
	FBulkDataRequest& operator=(FBulkDataRequest&&);
	
	/** Not copy constructable. */
	FBulkDataRequest(const FBulkDataRequest&) = delete;

	/** Not copy assignable. */
	FBulkDataRequest& operator=(const FBulkDataRequest&) = delete;

	/** Returns current status of the request. */
	EStatus GetStatus() const;

	/** Returns whether the request is associated with a pending or completed request. */
	bool IsNone() const;

	/** Returns whether the request is pending. */
	bool IsPending() const;

	/** Returns whether the request completed successfully. */
	bool IsOk() const;

	/** Returns whether the request has been completed. */
	bool IsCompleted() const;

	/** Cancel the pending request. */
	void Cancel();

	/** Reset the request handle to an invalid state. Will cancel and wait if the request is not completed. */
	void Reset();

protected:
	friend class FChunkBatchRequest;
	friend class FFileSystemBatchRequest;
	
	FBulkDataRequest(IHandle* InHandle);
	int32 GetRefCount() const;

	TRefCountPtr<IHandle> Handle;
};

/** Handle to a bulk data I/O batch read request. */
using FBulkDataBatchReadRequest = FBulkDataRequest;

/**
 * A batch request is a handle to one or more I/O requests.
 *
 * The batch request is kept alive by passing in handles when appending
 * read operations or by passing in a handle when issuing the batch. At least
 * one handle needs to be passed in. The last handle will block until the
 * entire batch is complete before being released.
 */
class COREUOBJECT_API FBulkDataBatchRequest : public FBulkDataRequest
{
public:
	class IHandle;

private:
	class COREUOBJECT_API FBuilder
	{
	public:
		~FBuilder();
		FBuilder(const FBuilder&) = delete;
		FBuilder& operator=(const FBuilder&) = delete;

	protected:
		FBuilder();
		explicit FBuilder(int32 MaxCount);
		FBulkDataBatchRequest::IHandle& GetBatch(const FBulkData& BulkData);
		EStatus IssueBatch(FBulkDataBatchRequest* OutRequest, FCompletionCallback&& Callback);

		int32 BatchMax = -1;
		int32 BatchCount = 0;
		int32 NumLoaded = 0;
		TRefCountPtr<FBulkDataBatchRequest::IHandle> Batch;
	};

public:	
	using FBulkDataRequest::FBulkDataRequest;

	/** Blocks the calling thread until the request is completed. */
	void Wait();

	/** Waits the specified amount of time in milliseconds for the request to be completed. */
	bool WaitFor(uint32 Milliseconds);

	/** Waits the specified amount of time for the request to be completed. */
	bool WaitFor(const FTimespan& WaitTime);

	/** Issue one or more I/O request in a single batch. */
	class COREUOBJECT_API FBatchBuilder : public FBuilder
	{
	public:
		FBatchBuilder(int32 MaxCount);
		/** Read the entire bulk data and copy the result to the specified instance. */
		FBatchBuilder& Read(FBulkData& BulkData, EAsyncIOPriorityAndFlags Priority = DefaultPriority);
		/**
		 * Read the bulk data from the specified offset and size and copy the result into the destination buffer.
		 * @param BulkData		The bulk data instance.
		 * @param Offset		Offset relative to the bulk data offset.
		 * @param Size			Number of bytes to read. Use MAX_uint64 to read the entire bulk data.
		 * @param Priority		The I/O priority.
		 * @param Dst			An empty or preallocated I/O buffer.	
		 */
		FBatchBuilder& Read(const FBulkData& BulkData, uint64 Offset, uint64 Size, EAsyncIOPriorityAndFlags Priority, FIoBuffer& Dst)
		{
			return Read(BulkData, Offset, Size, Priority, Dst, nullptr);
		}
		/**
		 * Read the bulk data from the specified offset and size and copy the result into the destination buffer.
		 * @param BulkData		The bulk data instance.
		 * @param Offset		Offset relative to the bulk data.
		 * @param Size			Number of bytes to read. Use MAX_uint64 to read the entire bulk data.
		 * @param Priority		The I/O priority.
		 * @param Dst			An empty or preallocated I/O buffer.	
		 * @param OutRequest	A handle to the read request.	
		 */
		FBatchBuilder& Read(const FBulkData& BulkData, uint64 Offset, uint64 Size, EAsyncIOPriorityAndFlags Priority, FIoBuffer& Dst, FBulkDataBatchReadRequest& OutRequest)
		{
			return Read(BulkData, Offset, Size, Priority, Dst, &OutRequest);
		}

		/**
		 * Issue the batch.
		 * @param OutRequest	A handle to the batch request.
		 * @return				Status of the issue operation.
		 */
		EStatus Issue(FBulkDataBatchRequest& OutRequest);
		/**
		 * Issue the batch. 
		 * @return				Status of the issue operation.
		 *
		 * @note Assumes one or more handle(s) has been passed into any of the read operations.
		 */
		[[nodiscard]] EStatus Issue();

	private:
		FBatchBuilder& Read(const FBulkData& BulkData, uint64 Offset, uint64 Size, EAsyncIOPriorityAndFlags Priority, FIoBuffer& Dst, FBulkDataBatchReadRequest* OutRequest);
	};

	/** Reads one or more bulk data and copies the result into a single I/O buffer. */
	class COREUOBJECT_API FScatterGatherBuilder : public FBuilder
	{
	public:
		FScatterGatherBuilder(int32 MaxCount);
		/** Read the bulk data from the specified offset and size. */
		FScatterGatherBuilder& Read(const FBulkData& BulkData, uint64 Offset = 0, uint64 Size = MAX_uint64);
		/**
		 * Issue the batch.
		 * @param Dst			An empty or preallocated I/O buffer.	
		 * @param Priority		The I/O priority.
		 * @param Callback		A callback triggered when the operation is completed.
		 * @param OutRequest	A handle to the batch request.
		 * @return				Status of the issue operation.
		 *
		 * @note Assumes one or more handle(s) has been passed into any of the read operations.
		 */
		EStatus Issue(FIoBuffer& Dst, EAsyncIOPriorityAndFlags Priority, FCompletionCallback&& Callback, FBulkDataBatchRequest& OutRequest);

	private:
		struct FRequest
		{
			const FBulkData* BulkData	= nullptr;
			uint64 Offset				= 0;
			uint64 Size					= MAX_uint64;
		};

		TArray<FRequest, TInlineAllocator<8>> Requests;
	};

	/** Returns a request builder that dispatches one or more I/O requests in a single batch. */
	static FBatchBuilder NewBatch(int32 MaxCount = -1) { return FBatchBuilder(MaxCount); }

	/** Returns a request builder that reads one or more bulk data into a single I/O buffer. */
	static FScatterGatherBuilder ScatterGather(int32 MaxCount = -1) { return FScatterGatherBuilder(MaxCount); }
};
