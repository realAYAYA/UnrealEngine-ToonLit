// Copyright Epic Games, Inc. All Rights Reserved.
#include "Serialization/BulkData.h"
#include "Async/MappedFileHandle.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/TVariant.h"
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/LargeMemoryReader.h"
#include "Templates/Casts.h"
#include "UObject/Package.h"
#include "UObject/LinkerLoad.h"
#include "UObject/LinkerSave.h"
#include "UObject/PackageResourceManager.h"

/** Whether to track information of how bulk data is being used */
#define TRACK_BULKDATA_USE 0

#if TRACK_BULKDATA_USE
#include "Misc/ScopeLock.h"
#endif // TRACK_BULKDATA_USE

// If set to 0 then we will pretend that optional data does not exist, useful for testing.
#define ALLOW_OPTIONAL_DATA 1

//////////////////////////////////////////////////////////////////////////////

const FIoFilenameHash FALLBACK_IO_FILENAME_HASH = INVALID_IO_FILENAME_HASH - 1;

FIoFilenameHash MakeIoFilenameHash(const FPackagePath& PackagePath)
{
	if (!PackagePath.IsEmpty())
	{
		FString BaseFileName = PackagePath.GetLocalBaseFilenameWithPath().ToLower();
		const FIoFilenameHash Hash = FCrc::StrCrc32<TCHAR>(*BaseFileName);
		return Hash != INVALID_IO_FILENAME_HASH ? Hash : FALLBACK_IO_FILENAME_HASH;
	}
	else
	{
		return INVALID_IO_FILENAME_HASH;
	}
}

FIoFilenameHash MakeIoFilenameHash(const FString& Filename)
{
	if (!Filename.IsEmpty())
	{
		FString BaseFileName = FPaths::GetBaseFilename(Filename).ToLower();
		const FIoFilenameHash Hash = FCrc::StrCrc32<TCHAR>(*BaseFileName);
		return Hash != INVALID_IO_FILENAME_HASH ? Hash : FALLBACK_IO_FILENAME_HASH;
	}
	else
	{
		return INVALID_IO_FILENAME_HASH;
	}
}

FIoFilenameHash MakeIoFilenameHash(const FIoChunkId& ChunkID)
{
	if (ChunkID.IsValid())
	{
		const FIoFilenameHash Hash = GetTypeHash(ChunkID);
		return Hash != INVALID_IO_FILENAME_HASH ? Hash : FALLBACK_IO_FILENAME_HASH;
	}
	else
	{
		return INVALID_IO_FILENAME_HASH;
	}
}

//////////////////////////////////////////////////////////////////////////////

namespace UE::BulkData::Private
{

/** Open the bulk data chunk fo reading. */
bool OpenReadBulkData(
	const FBulkMetaData& BulkMeta,
	const FBulkDataChunkId& BulkChunkId,
	int64 Offset,
	int64 Size,
	EAsyncIOPriorityAndFlags Priority,
	TFunction<void(FArchive& Ar)>&& Read);

/** Open async read file handle for the specified bulk data chunk ID. */
TUniquePtr<IAsyncReadFileHandle> OpenAsyncReadBulkData(const FBulkMetaData& BulkMeta, const FBulkDataChunkId& BulkChunkId);

/** Create bulk data streaming request. */
TUniquePtr<IBulkDataIORequest> CreateStreamingRequest(
	const FBulkMetaData& BulkMeta,
	const FBulkDataChunkId& BulkChunkId,
	int64 Offset,
	int64 Size,
	EAsyncIOPriorityAndFlags Priority,
	FBulkDataIORequestCallBack* CompleteCallback,
	uint8* UserSuppliedMemory);

/** Returns whether the bulk data chunk exist or not. */
bool DoesBulkDataExist(const FBulkMetaData& BulkMeta, const FBulkDataChunkId& BulkChunkId);

/** Try memory map the chunk specified by the bulk data ID. */
bool TryMemoryMapBulkData(
	const FBulkMetaData& BulkMeta,
	const FBulkDataChunkId& BulkChunkId,
	int64 Offset,
	int64 Size,
	FIoMappedRegion& OutRegion);

/** Start load the internal bulk data payload. */ 
bool StartAsyncLoad(
	FBulkData* Owner,
	const FBulkMetaData& BulkMeta,
	const FBulkDataChunkId& BulkChunkId,
	int64 Offset,
	int64 Size,
	EAsyncIOPriorityAndFlags Priority,
	TFunction<void(TIoStatusOr<FIoBuffer>)>&& Callback);

/** Flush pending async load. */
void FlushAsyncLoad(FBulkData* BulkData);

//////////////////////////////////////////////////////////////////////////////

static FArchive& SerializeAsInt32(FArchive& Ar, int64& Value)
{
	check(!Ar.IsSaving() || (MIN_int32 <= Value && Value <= MAX_int32));
	int32 ValueAsInt32 = static_cast<int32>(Value);
	Ar << ValueAsInt32;
	Value = ValueAsInt32;

	return Ar;
}

//////////////////////////////////////////////////////////////////////////////

FArchive& operator<<(FArchive& Ar, FBulkMetaResource& BulkMeta)
{
	Ar << BulkMeta.Flags;

	if (UNLIKELY(BulkMeta.Flags & BULKDATA_Size64Bit))
	{
		Ar << BulkMeta.ElementCount;
		Ar << BulkMeta.SizeOnDisk;
		Ar << BulkMeta.Offset;

		if (UNLIKELY(BulkMeta.Flags & BULKDATA_BadDataVersion))
		{
			if (Ar.IsLoading())
			{
				uint16 DummyValue;
				Ar << DummyValue;
				BulkMeta.Flags = static_cast<EBulkDataFlags>(BulkMeta.Flags & ~BULKDATA_BadDataVersion);
			}
		}

		if (BulkMeta.Flags & BULKDATA_DuplicateNonOptionalPayload)
		{
			Ar << BulkMeta.DuplicateFlags;
			Ar << BulkMeta.DuplicateSizeOnDisk;
			Ar << BulkMeta.DuplicateOffset;
		}
	}
	else
	{
		SerializeAsInt32(Ar, BulkMeta.ElementCount);
		SerializeAsInt32(Ar, BulkMeta.SizeOnDisk);
		Ar << BulkMeta.Offset;
		
		if (UNLIKELY(BulkMeta.Flags & BULKDATA_BadDataVersion))
		{
			if (Ar.IsLoading())
			{
				uint16 DummyValue;
				Ar << DummyValue;
				BulkMeta.Flags = static_cast<EBulkDataFlags>(BulkMeta.Flags & ~BULKDATA_BadDataVersion);
			}
		}
		
		if (BulkMeta.Flags & BULKDATA_DuplicateNonOptionalPayload)
		{
			Ar << BulkMeta.DuplicateFlags;
			SerializeAsInt32(Ar, BulkMeta.DuplicateSizeOnDisk);
			Ar << BulkMeta.DuplicateOffset;
		}
	}

	return Ar;
}

//////////////////////////////////////////////////////////////////////////////

struct FBulkDataChunkId::FImpl
{
	using FPathOrId = TVariant<FPackagePath, FPackageId>;

	FImpl(const FPackageId& PackageId)
		: PathOrId(TInPlaceType<FPackageId>(), PackageId)
	{ }

	FImpl(const FPackagePath& PackagePath)
		: PathOrId(TInPlaceType<FPackagePath>(), PackagePath)
	{ }

	FImpl(const FPathOrId& InPathOrId)
		: PathOrId(InPathOrId)
	{ }

	FPathOrId PathOrId;
};

FBulkDataChunkId::FBulkDataChunkId()
{
}

FBulkDataChunkId::FBulkDataChunkId(TPimplPtr<FImpl>&& InImpl)
	: Impl(MoveTemp(InImpl))
{
}

FBulkDataChunkId::~FBulkDataChunkId()
{
}

FBulkDataChunkId::FBulkDataChunkId(const FBulkDataChunkId& Other)
{
	*this = Other;
}

FBulkDataChunkId& FBulkDataChunkId::operator=(const FBulkDataChunkId& Other)
{
	Impl.Reset();

	if (Other.Impl)
	{
		Impl = MakePimpl<FImpl>(Other.Impl->PathOrId);
	}

	return *this;
}

bool FBulkDataChunkId::operator==(const FBulkDataChunkId& Other) const
{
	if (Impl.IsValid() && Other.Impl.IsValid())
	{
		if (const FPackageId* Id = Impl->PathOrId.TryGet<FPackageId>())
		{
			if (const FPackageId* OtherId = Other.Impl->PathOrId.TryGet<FPackageId>())
			{
				return *Id == *OtherId;
			}
		}

		if (const FPackagePath* PackagePath = Impl->PathOrId.TryGet<FPackagePath>())
		{
			if (const FPackagePath* OtherPackagePath = Other.Impl->PathOrId.TryGet<FPackagePath>())
			{
				return *PackagePath == *OtherPackagePath;
			}
		}
	}

	return Impl.IsValid() == Other.Impl.IsValid();
}

FPackageId FBulkDataChunkId::GetPackageId() const
{
	if (Impl)
	{
		if (const FPackageId* Id = Impl->PathOrId.TryGet<FPackageId>())
		{
			return *Id;
		}
	}

	return FPackageId();
}

const FPackagePath& FBulkDataChunkId::GetPackagePath() const
{
	if (Impl)
	{
		if (const FPackagePath* Path = Impl->PathOrId.TryGet<FPackagePath>())
		{
			return *Path;
		}
	}

	static FPackagePath Empty;
	return Empty;
}

FIoFilenameHash FBulkDataChunkId::GetIoFilenameHash(EBulkDataFlags BulkDataFlags) const
{
	if (Impl)
	{
		if (const FPackageId* Id = Impl->PathOrId.TryGet<FPackageId>())
		{
			return MakeIoFilenameHash(CreateBulkDataIoChunkId(UE::BulkData::Private::FBulkMetaData(BulkDataFlags), *Id));
		}

		if (const FPackagePath* PackagePath = Impl->PathOrId.TryGet<FPackagePath>())
		{
			return MakeIoFilenameHash(*PackagePath);
		}
	}

	return INVALID_IO_FILENAME_HASH;
}

FString FBulkDataChunkId::ToDebugString() const
{
	if (Impl)
	{
		if (const FPackageId* Id = Impl->PathOrId.TryGet<FPackageId>())
		{
			return FString::Printf(TEXT("0x%llX"), Id->Value());
		}

		if (const FPackagePath* PackagePath = Impl->PathOrId.TryGet<FPackagePath>())
		{
			return PackagePath->GetDebugName();
		}
	}

	return FString(TEXT("None"));
}

FBulkDataChunkId FBulkDataChunkId::FromPackagePath(const FPackagePath& PackagePath)
{
	return FBulkDataChunkId(MakePimpl<FImpl>(PackagePath));
}

FBulkDataChunkId FBulkDataChunkId::FromPackageId(const FPackageId& PackageId)
{
	return FBulkDataChunkId(MakePimpl<FImpl>(PackageId));
}

} // namespace UE::BulkData

/*-----------------------------------------------------------------------------
	Memory managament
-----------------------------------------------------------------------------*/

void FBulkData::FAllocatedPtr::Free(FBulkData* Owner)
{
	if (!Owner->IsDataMemoryMapped())
	{
		FMemory::Free(Allocation.RawData); //-V611 We know this was allocated via FMemory::Malloc if ::IsDataMemoryMapped returned false
		Allocation.RawData = nullptr;
	}
	else
	{
		delete Allocation.MemoryMappedData;
		Allocation.MemoryMappedData = nullptr;
	}
}

void* FBulkData::FAllocatedPtr::ReallocateData(FBulkData* Owner, SIZE_T SizeInBytes)
{
	checkf(!Owner->IsDataMemoryMapped(),  TEXT("Trying to reallocate a memory mapped BulkData object without freeing it first!"));

	Allocation.RawData = FMemory::Realloc(Allocation.RawData, SizeInBytes, DEFAULT_ALIGNMENT);

	return Allocation.RawData;
}

void FBulkData::FAllocatedPtr::SetData(FBulkData* Owner, void* Buffer)
{
	checkf(Allocation.RawData == nullptr, TEXT("Trying to assign a BulkData object without freeing it first!"));

	Allocation.RawData = Buffer;
}

void FBulkData::FAllocatedPtr::SetMemoryMappedData(FBulkData* Owner, IMappedFileHandle* MappedHandle, IMappedFileRegion* MappedRegion)
{
	checkf(Allocation.MemoryMappedData == nullptr, TEXT("Trying to assign a BulkData object without freeing it first!"));
	Allocation.MemoryMappedData = new FOwnedBulkDataPtr(MappedHandle, MappedRegion);

	Owner->SetBulkDataFlags(BULKDATA_DataIsMemoryMapped);
}

void* FBulkData::FAllocatedPtr::GetAllocationForWrite(const FBulkData* Owner) const
{
	if (!Owner->IsDataMemoryMapped())
	{
		return Allocation.RawData;
	}
	else
	{
		return nullptr;
	}
}

const void* FBulkData::FAllocatedPtr::GetAllocationReadOnly(const FBulkData* Owner) const
{
	if (!Owner->IsDataMemoryMapped())
	{
		return Allocation.RawData;
	}
	else if (Allocation.MemoryMappedData != nullptr)
	{
		return Allocation.MemoryMappedData->GetPointer();
	}
	else
	{
		return nullptr;
	}
}

FOwnedBulkDataPtr* FBulkData::FAllocatedPtr::StealFileMapping(FBulkData* Owner)
{
	FOwnedBulkDataPtr* Ptr;
	if (!Owner->IsDataMemoryMapped())
	{
		Ptr = new FOwnedBulkDataPtr(Allocation.RawData);
	}
	else
	{
		Ptr = Allocation.MemoryMappedData;
		Owner->ClearBulkDataFlags(BULKDATA_DataIsMemoryMapped);
	}	

	Allocation.RawData = nullptr;

	return Ptr;
}

void FBulkData::FAllocatedPtr::Swap(FBulkData* Owner, void** DstBuffer)
{
	if (!Owner->IsDataMemoryMapped())
	{
		::Swap(*DstBuffer, Allocation.RawData);
	}
	else
	{
		const int64 DataSize = Owner->GetBulkDataSize();

		*DstBuffer = FMemory::Malloc(DataSize, DEFAULT_ALIGNMENT);
		FMemory::Memcpy(*DstBuffer, Allocation.MemoryMappedData->GetPointer(), DataSize);

		delete Allocation.MemoryMappedData;
		Allocation.MemoryMappedData = nullptr;

		Owner->ClearBulkDataFlags(BULKDATA_DataIsMemoryMapped);
	}	
}

/*-----------------------------------------------------------------------------
	Constructors and operators
-----------------------------------------------------------------------------*/

DECLARE_STATS_GROUP(TEXT("Bulk Data"), STATGROUP_BulkData, STATCAT_Advanced);

#if TRACK_BULKDATA_USE

/** Simple wrapper for tracking the bulk data usage in the thread-safe way. */
struct FThreadSafeBulkDataToObjectMap
{
	static FThreadSafeBulkDataToObjectMap& Get()
	{
		static FThreadSafeBulkDataToObjectMap Instance;
		return Instance;
	}

	void Add( FBulkData* Key, UObject* Value )
	{
		FScopeLock ScopeLock(&CriticalSection);
		BulkDataToObjectMap.Add(Key,Value);
	}

	void Remove( FBulkData* Key )
	{
		FScopeLock ScopeLock(&CriticalSection);
		BulkDataToObjectMap.Remove(Key);
	}

	FCriticalSection& GetLock() 
	{
		return CriticalSection;
	}

	const TMap<FBulkData*,UObject*>::TConstIterator GetIterator() const
	{
		return BulkDataToObjectMap.CreateConstIterator();
	}

protected:
	/** Map from bulk data pointer to object it is contained by */
	TMap<FBulkData*,UObject*> BulkDataToObjectMap;

	/** CriticalSection. */
	FCriticalSection CriticalSection;
};

/**
 * Helper structure associating an object and a size for sorting purposes.
 */
struct FObjectAndSize
{
	FObjectAndSize( const UObject* InObject, int64 InSize )
	:	Object( InObject )
	,	Size( InSize )
	{}

	/** Object associated with size. */
	const UObject*	Object;
	/** Size associated with object. */
	int64			Size;
};

/** Hash function required for TMap support */
uint32 GetTypeHash( const FBulkData* BulkData )
{
	return PointerHash(BulkData);
}

#endif

FOwnedBulkDataPtr::~FOwnedBulkDataPtr()
{
	if (AllocatedData)
	{
		FMemory::Free(AllocatedData);
	}
	else
	{
		if (MappedRegion || MappedHandle)
		{
			delete MappedRegion;
			delete MappedHandle;
		}
	}
}

const void* FOwnedBulkDataPtr::GetPointer()
{
	// return the pointer that the caller can use
	return AllocatedData ? AllocatedData : (MappedRegion ? MappedRegion->GetMappedPtr() : nullptr);
}

/**
 * Copy constructor. Use the common routine to perform the copy.
 *
 * @param Other the source array to copy
 */
FBulkData::FBulkData( const FBulkData& Other )
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FBulkData::FBulkData"), STAT_UBD_Constructor, STATGROUP_Memory);

	*this = Other;

#if TRACK_BULKDATA_USE
	FThreadSafeBulkDataToObjectMap::Get().Add( this, NULL );
#endif
}

/**
 * Virtual destructor, free'ing allocated memory.
 */
FBulkData::~FBulkData()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FBulkData::~FBulkData"), STAT_UBD_Destructor, STATGROUP_Memory);

	check(IsUnlocked());
	FlushAsyncLoading();

	// Free memory.
	FreeData();

#if WITH_EDITOR
	// Detach from archive.
	if( AttachedAr )
	{
		AttachedAr->DetachBulkData( this, false );
		check( AttachedAr == nullptr );
	}
#endif // WITH_EDITOR

#if TRACK_BULKDATA_USE
	FThreadSafeBulkDataToObjectMap::Get().Remove( this );
#endif
}

/**
 * Copies the source array into this one after detaching from archive.
 *
 * @param Other the source array to copy
 */
FBulkData& FBulkData::operator=( const FBulkData& Other )
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FBulkData::operator="), STAT_UBD_Constructor, STATGROUP_Memory);

	checkf(IsUnlocked(), TEXT("Attempting to modify a BulkData object that is locked"));
	checkf(Other.BulkMeta.GetLockStatus() != LOCKSTATUS_ReadWriteLock, TEXT("Attempting to read from a BulkData object that is locked for write"));

	RemoveBulkData();
	
	BulkMeta = Other.BulkMeta;
	BulkChunkId = Other.BulkChunkId;

	if (!Other.IsDataMemoryMapped() || !Other.IsInSeparateFile())
	{
		if (const void* Src = Other.GetDataBufferReadOnly())
		{
			void* Dst = ReallocateData(GetBulkDataSize());
			FMemory::Memcpy(Dst, Src, GetBulkDataSize());
		}
	}
	else
	{
		FIoMappedRegion MappedRegion;
		if (UE::BulkData::Private::TryMemoryMapBulkData(BulkMeta, BulkChunkId, BulkMeta.GetOffset(), BulkMeta.GetSize(), MappedRegion))
		{
			DataAllocation.SetMemoryMappedData(this, MappedRegion.MappedFileHandle, MappedRegion.MappedFileRegion);
		}
	}

	return *this;
}

/*-----------------------------------------------------------------------------
	Static functions.
-----------------------------------------------------------------------------*/

/**
 * Dumps detailed information of bulk data usage.
 *
 * @param Log FOutputDevice to use for logging
 */
void FBulkData::DumpBulkDataUsage( FOutputDevice& Log )
{
#if TRACK_BULKDATA_USE
	// Arrays about to hold per object and per class size information.
	TArray<FObjectAndSize> PerObjectSizeArray;
	TArray<FObjectAndSize> PerClassSizeArray;

	{
		FScopeLock Lock(&FThreadSafeBulkDataToObjectMap::Get().GetLock());

		// Iterate over all "live" bulk data and add size to arrays if it is loaded.
		for( auto It(FThreadSafeBulkDataToObjectMap::Get().GetIterator()); It; ++It )
		{
			const FBulkData*		BulkData	= It.Key();
			const UObject*			Owner		= It.Value();
			// Only add bulk data that is consuming memory to array.
			if( Owner && BulkData->IsBulkDataLoaded() && BulkData->GetBulkDataSize() > 0 )
			{
				// Per object stats.
				PerObjectSizeArray.Add( FObjectAndSize( Owner, BulkData->GetBulkDataSize() ) );

				// Per class stats.
				bool bFoundExistingPerClassSize = false;
				// Iterate over array, trying to find existing entry.
				for( int32 PerClassIndex=0; PerClassIndex<PerClassSizeArray.Num(); PerClassIndex++ )
				{
					FObjectAndSize& PerClassSize = PerClassSizeArray[ PerClassIndex ];
					// Add to existing entry if found.
					if( PerClassSize.Object == Owner->GetClass() )
					{
						PerClassSize.Size += BulkData->GetBulkDataSize();
						bFoundExistingPerClassSize = true;
						break;
					}
				}
				// Add new entry if we didn't find an existing one.
				if( !bFoundExistingPerClassSize )
				{
					PerClassSizeArray.Add( FObjectAndSize( Owner->GetClass(), BulkData->GetBulkDataSize() ) );
				}
			}
		}
	}

	/** Compare operator, sorting by size in descending order */
	struct FCompareFObjectAndSize
	{
		FORCEINLINE bool operator()( const FObjectAndSize& A, const FObjectAndSize& B ) const
		{
			return B.Size < A.Size;
		}
	};

	// Sort by size.
	PerObjectSizeArray.Sort( FCompareFObjectAndSize() );
	PerClassSizeArray.Sort( FCompareFObjectAndSize() );

	// Log information.
	UE_LOG(LogSerialization, Log, TEXT(""));
	UE_LOG(LogSerialization, Log, TEXT("Per class summary of bulk data use:"));
	for( int32 PerClassIndex=0; PerClassIndex<PerClassSizeArray.Num(); PerClassIndex++ )
	{
		const FObjectAndSize& PerClassSize = PerClassSizeArray[ PerClassIndex ];
		Log.Logf( TEXT("  %5lld KByte of bulk data for Class %s"), PerClassSize.Size / 1024, *PerClassSize.Object->GetPathName() );
	}
	UE_LOG(LogSerialization, Log, TEXT(""));
	UE_LOG(LogSerialization, Log, TEXT("Detailed per object stats of bulk data use:"));
	for( int32 PerObjectIndex=0; PerObjectIndex<PerObjectSizeArray.Num(); PerObjectIndex++ )
	{
		const FObjectAndSize& PerObjectSize = PerObjectSizeArray[ PerObjectIndex ];
		Log.Logf( TEXT("  %5lld KByte of bulk data for %s"), PerObjectSize.Size / 1024, *PerObjectSize.Object->GetFullName() );
	}
	UE_LOG(LogSerialization, Log, TEXT(""));
#else
	UE_LOG(LogSerialization, Log, TEXT("Please recompiled with TRACK_BULKDATA_USE set to 1 in UnBulkData.cpp."));
#endif
}


/*-----------------------------------------------------------------------------
	Accessors.
-----------------------------------------------------------------------------*/

/**
 * Returns the size of the bulk data in bytes.
 *
 * @return Size of the bulk data in bytes
 */
int64 FBulkData::GetBulkDataSize() const
{
	return BulkMeta.GetSize();
}

/**
 * Returns the size of the bulk data on disk. This can differ from GetBulkDataSize if
 * BULKDATA_SerializeCompressed is set.
 *
 * @return Size of the bulk data on disk or INDEX_NONE in case there's no association
 */
int64 FBulkData::GetBulkDataSizeOnDisk() const
{
	return BulkMeta.GetSizeOnDisk();
}
/**
 * Returns the offset into the file the bulk data is located at.
 *
 * @return Offset into the file or INDEX_NONE in case there is no association
 */
int64 FBulkData::GetBulkDataOffsetInFile() const
{
	return BulkMeta.GetOffset();
}
/**
 * Returns whether the bulk data is stored compressed on disk.
 *
 * @return true if data is compressed on disk, false otherwise
 */
bool FBulkData::IsStoredCompressedOnDisk() const
{
	return BulkMeta.HasAnyFlags(BULKDATA_SerializeCompressed);
}

bool FBulkData::CanLoadFromDisk() const
{
#if WITH_EDITOR
	return AttachedAr != nullptr || BulkChunkId.IsValid();
#else
#if UE_KEEP_INLINE_RELOADING_CONSISTENT
	if (IsInlined())
	{
		return false;
	}
#endif //UE_KEEP_INLINE_RELOADING_CONSISTENT
	return BulkChunkId.IsValid();
#endif // WITH_EDITOR
}

bool FBulkData::DoesExist() const
{
#if !ALLOW_OPTIONAL_DATA
	if (IsOptional())
	{
		return false;
	}
#endif

	return UE::BulkData::Private::DoesBulkDataExist(BulkMeta, BulkChunkId);
}

/**
 * Returns flags usable to decompress the bulk data
 * 
 * @return COMPRESS_NONE if the data was not compressed on disk, or valid flags to pass to FCompression::UncompressMemory for this data
 */
FName FBulkData::GetDecompressionFormat() const
{
	return GetDecompressionFormat(BulkMeta.GetFlags());
}

FName FBulkData::GetDecompressionFormat(EBulkDataFlags InFlags)
{
	return (InFlags & BULKDATA_SerializeCompressedZLIB) ? NAME_Zlib : NAME_None;
}

/**
 * Returns whether the bulk data is currently loaded and resident in memory.
 *
 * @return true if bulk data is loaded, false otherwise
 */
bool FBulkData::IsBulkDataLoaded() const
{
	return DataAllocation.IsLoaded();
}

bool FBulkData::IsAsyncLoadingComplete() const
{
	return (GetBulkDataFlags() & BULKDATA_HasAsyncReadPending) == 0;
}

/**
* Returns whether this bulk data is used
* @return true if BULKDATA_Unused is not set
*/
bool FBulkData::IsAvailableForUse() const
{
	return BulkMeta.HasAnyFlags(BULKDATA_Unused) == false;
}

/*-----------------------------------------------------------------------------
	Data retrieval and manipulation.
-----------------------------------------------------------------------------*/

/**
 * Retrieves a copy of the bulk data.
 *
 * @param Dest [in/out] Pointer to pointer going to hold copy, can point to NULL pointer in which case memory is allocated
 * @param bDiscardInternalCopy Whether to discard/ free the potentially internally allocated copy of the data
 */
void FBulkData::GetCopy( void** Dest, bool bDiscardInternalCopy )
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FBulkData::GetCopy"), STAT_UBD_GetCopy, STATGROUP_Memory);

	check( IsUnlocked() );
	check( Dest );

	// Make sure any async loads have completed and moved the data into BulkData
	FlushAsyncLoading();

	const int64 BulkDataSize = GetBulkDataSize();

	// Passed in memory is going to be used.
	if( *Dest )
	{
		// The data is already loaded so we can simply use a mempcy.
		if( IsBulkDataLoaded() )
		{
			// Copy data into destination memory.
			FMemory::Memcpy( *Dest, GetDataBufferReadOnly(), BulkDataSize );
			// Discard internal copy if wanted and we're still attached to an archive or if we're
			// single use bulk data.
			if( bDiscardInternalCopy && CanDiscardInternalData() )
			{
#if USE_RUNTIME_BULKDATA
				UE_LOG(LogSerialization, Warning, TEXT("FBulkData::GetCopy both copied and discarded it's data, passing in an empty pointer would avoid an extra allocate and memcpy!"));
#endif
				FreeData();
			}
		}
		// Data isn't currently loaded so we need to load it from disk.
		else
		{
			FIoBuffer Buffer(FIoBuffer::Wrap, *Dest, BulkDataSize);
			TryLoadDataIntoMemory(Buffer);
		}
	}
	// Passed in memory is NULL so we need to allocate some.
	else
	{
		// The data is already loaded so we can simply use a mempcy.
		if( IsBulkDataLoaded() )
		{
			// If the internal copy should be discarded and we are still attached to an archive we can
			// simply "return" the already existing copy and NULL out the internal reference. We can
			// also do this if the data is single use like e.g. when uploading texture data.
			if( bDiscardInternalCopy && CanDiscardInternalData() )
			{
				DataAllocation.Swap(this, Dest);
			}
			// Can't/ Don't discard so we need to allocate and copy.
			else
			{
				if (BulkDataSize != 0)
				{
					// Allocate enough memory for data...
					*Dest = FMemory::Malloc( BulkDataSize, GetBulkDataAlignment() );

					// ... and copy it into memory now pointed to by out parameter.
					FMemory::Memcpy( *Dest, GetDataBufferReadOnly(), BulkDataSize );
				}
				else
				{
					*Dest = nullptr;
				}
			}
		}
		// Data isn't currently loaded so we need to load it from disk.
		else
		{
			if (BulkDataSize != 0)
			{
				FIoBuffer Buffer(BulkDataSize);
				if (TryLoadDataIntoMemory(Buffer))
				{
					*Dest = Buffer.Release().ConsumeValueOrDie();
				}
			}
			else
			{
				*Dest = nullptr;
			}
		}
	}
}

/**
 * Locks the bulk data and returns a pointer to it.
 *
 * @param	LockFlags	Flags determining lock behavior
 */
void* FBulkData::Lock( uint32 LockFlags )
{
	check(IsUnlocked());
	
	// Make sure bulk data is loaded.
	MakeSureBulkDataIsLoaded();
		
	// Read-write operations are allowed on returned memory.
	if( LockFlags & LOCK_READ_WRITE )
	{
#if WITH_EDITOR
		// We need to detach from the archive to not be able to clobber changes by serializing
		// over them.
		if( AttachedAr )
		{
			// Detach bulk data. This will call DetachFromArchive which in turn will clear AttachedAr.
			AttachedAr->DetachBulkData( this, false );
			check( AttachedAr == nullptr );
		}
#endif // WITH_EDITOR
		// This has to be set after the DetachBulkData because we can't detach a locked bulkdata
		BulkMeta.SetLockStatus(LOCKSTATUS_ReadWriteLock);
		ClearBulkDataFlags(BULKDATA_LazyLoadable);
		return GetDataBufferForWrite();
	}
	else if( LockFlags & LOCK_READ_ONLY )
	{
		BulkMeta.SetLockStatus(LOCKSTATUS_ReadOnlyLock);
		return (void*)GetDataBufferReadOnly(); // Cast the const away, icky but our hands are tied by the original API at this time
	}
	else
	{
		UE_LOG(LogSerialization, Fatal, TEXT("Unknown lock flag '%u'"), LockFlags);
		return nullptr;
	}
}

const void* FBulkData::LockReadOnly() const
{
	check(IsUnlocked());
	
	FBulkData* mutable_this = const_cast<FBulkData*>(this);

	// Make sure bulk data is loaded.
	mutable_this->MakeSureBulkDataIsLoaded();

	// Only read operations are allowed on returned memory.
	mutable_this->BulkMeta.SetLockStatus(LOCKSTATUS_ReadOnlyLock);

	return GetDataBufferReadOnly();
}

void* FBulkData::Realloc(int64 ElementCount, int64 ElementSize)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FBulkData::Realloc"), STAT_UBD_Realloc, STATGROUP_Memory);

	checkf(IsLocked(), TEXT("BulkData must be locked for 'write' before reallocating!"));
#if !WITH_EDITOR
	checkf(!CanLoadFromDisk(), TEXT("Cannot re-allocate a FBulkDataBase object that represents a file on disk!"));
#endif //!WITH_EDITOR

	// We might want to consider this a valid use case if anyone can come up with one?
	checkf(!IsUsingIODispatcher(), TEXT("Attempting to re-allocate data loaded from the IoDispatcher"));

	const int64 BulkDataSize = ElementCount * ElementSize;
	check(BulkDataSize <= MaxBulkDataSize);
	BulkMeta.SetSize(BulkDataSize);
	ReallocateData(BulkDataSize);

	return GetDataBufferForWrite();
}

/** 
 * Unlocks bulk data after which point the pointer returned by Lock no longer is valid.
 */
void FBulkData::Unlock() const
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FBulkData::Unlock"), STAT_UBD_Unlock, STATGROUP_Memory);

	check(IsUnlocked() == false);

	FBulkData* mutable_this = const_cast<FBulkData*>(this);

	mutable_this->BulkMeta.SetLockStatus(LOCKSTATUS_Unlocked);

	// Free pointer if we're guaranteed to only to access the data once.
	if (BulkMeta.HasAnyFlags(BULKDATA_SingleUse))
	{
		mutable_this->FreeData();
	}
}

/**
 * Clears/ removes the bulk data and resets element count to 0.
 */
void FBulkData::RemoveBulkData()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FBulkData::RemoveBulkData"), STAT_UBD_RemoveBulkData, STATGROUP_Memory);

	check(IsUnlocked());

#if WITH_EDITOR
	// Detach from archive without loading first.
	if( AttachedAr )
	{
		AttachedAr->DetachBulkData( this, false );
		check( AttachedAr == nullptr );
	}
#endif // WITH_EDITOR
	
	// Resize to 0 elements.
	BulkMeta.SetSize(0);
	FreeData();
	ClearBulkDataFlags(BULKDATA_LazyLoadable);
}

/**
 * Deallocates bulk data without detaching the archive.
 */
bool FBulkData::UnloadBulkData()
{
#if WITH_EDITOR
	if (IsUnlocked())
	{
		FlushAsyncLoading();
		FreeData();
		return true;
	}
#endif
	return false;
}

#if !USE_RUNTIME_BULKDATA
/**
* Load the bulk data using a file reader. Works when no archive is attached to the bulk data.
* @return Whether the operation succeeded.
*/
bool FBulkData::LoadBulkDataWithFileReader()
{
#if WITH_EDITOR
	if (IsBulkDataLoaded() == false && CanLoadBulkDataWithFileReader())
	{
		const int64 BulkDataOffset = GetBulkDataOffsetInFile();
		const int64 BulkDataSizeOnDisk = GetBulkDataSizeOnDisk();

		const bool bOk = UE::BulkData::Private::OpenReadBulkData(
			BulkMeta,
			BulkChunkId,
			BulkDataOffset,
			BulkDataSizeOnDisk,
			AIOP_High,
			[this](FArchive& Ar)
			{
				const int64 BulkDataSize = GetBulkDataSize();
				void* DataBuffer = ReallocateData(BulkDataSize);
				SerializeBulkData(Ar, DataBuffer, BulkDataSize, BulkMeta.GetFlags());
			});

		return bOk;
	}
#endif
	return false;
}

bool FBulkData::CanLoadBulkDataWithFileReader() const
{
#if WITH_EDITOR
	return BulkChunkId.GetPackagePath().IsEmpty() == false;
#else
	return false;
#endif
}
#endif // !USE_RUNTIME_BULKDATA

/**
 * Forces the bulk data to be resident in memory and detaches the archive.
 */
void FBulkData::ForceBulkDataResident()
{
	// Make sure bulk data is loaded.
	MakeSureBulkDataIsLoaded();

#if WITH_EDITOR
	// Detach from the archive 
	if( AttachedAr )
	{
		// Detach bulk data. This will call DetachFromArchive which in turn will clear AttachedAr.
		AttachedAr->DetachBulkData( this, false );
		check( AttachedAr == nullptr );
	}
#endif // WITH_EDITOR
}

bool FBulkData::StartAsyncLoading()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FBulkData::StartAsyncLoading"), STAT_UBD_StartSerializingBulkData, STATGROUP_Memory);

	if (!IsAsyncLoadingComplete())
	{
		return true; // Early out if an asynchronous load is already in progress.
	}

	if (IsBulkDataLoaded())
	{
		return false; // Early out if we do not need to actually load any data
	}

	if (!CanLoadFromDisk())
	{
		return false; // Early out if we cannot load from disk
	}

	checkf(IsUnlocked(), TEXT("Attempting to modify a BulkData object that is locked"));
	check(GetBulkDataSize() > 0);
	check(BulkChunkId.IsValid());
	
	BulkMeta.SetLockStatus(LOCKSTATUS_ReadWriteLock); // Bulkdata is effectively locked while streaming!
	SetBulkDataFlags(BULKDATA_HasAsyncReadPending);
	FPlatformMisc::MemoryBarrier();

	return UE::BulkData::Private::StartAsyncLoad(
		this,
		BulkMeta,
		BulkChunkId,
		BulkMeta.GetOffset(), 
		BulkMeta.GetSize(),
		AIOP_Low,
		[this](TIoStatusOr<FIoBuffer> Result)
	{
		if (Result.IsOk())
		{
			FIoBuffer Buffer = Result.ConsumeValueOrDie();
			Buffer.EnsureOwned();
			DataAllocation.SetData(this, Buffer.Release().ConsumeValueOrDie());
		}
		else
		{
			UE_LOG(LogSerialization, Error, TEXT("Async load bulk data '%s' FAILED, reason '%s'"), *BulkChunkId.ToDebugString(), GetIoErrorText(Result.Status().GetErrorCode()));
			RemoveBulkData();
		}

		FPlatformMisc::MemoryBarrier();
		BulkMeta.SetLockStatus(LOCKSTATUS_Unlocked);
		ClearBulkDataFlags(BULKDATA_HasAsyncReadPending);
	});
}

/**
 * Sets the passed in bulk data flags.
 *
 * @param BulkDataFlagsToSet	Bulk data flags to set
 */
void FBulkData::SetBulkDataFlags( uint32 BulkDataFlagsToSet )
{
	BulkMeta.SetFlags(static_cast<EBulkDataFlags>(BulkMeta.GetFlags() | BulkDataFlagsToSet));
}

void FBulkData::ResetBulkDataFlags(uint32 BulkDataFlagsToSet)
{
	BulkMeta.SetFlags(static_cast<EBulkDataFlags>(BulkDataFlagsToSet));
}

/**
* Gets the current bulk data flags.
*
* @return Bulk data flags currently set
*/
uint32 FBulkData::GetBulkDataFlags() const
{
	return BulkMeta.GetFlags();
}

/**
 * Sets the passed in bulk data alignment.
 *
 * @param BulkDataAlignmentToSet	Bulk data alignment to set
 */
void FBulkData::SetBulkDataAlignment(uint16 BulkDataAlignmentToSet)
{
}

/**
* Gets the current bulk data alignment.
*
* @return Bulk data alignment currently set
*/
uint32 FBulkData::GetBulkDataAlignment() const
{
	return DEFAULT_ALIGNMENT;
}

/**
 * Clears the passed in bulk data flags.
 *
 * @param BulkDataFlagsToClear	Bulk data flags to clear
 */
void FBulkData::ClearBulkDataFlags( uint32 BulkDataFlagsToClear )
{
	BulkMeta.SetFlags(static_cast<EBulkDataFlags>(BulkMeta.GetFlags() & ~BulkDataFlagsToClear));
}

FIoChunkId FBulkData::CreateChunkId() const
{
	return UE::BulkData::Private::CreateBulkDataIoChunkId(BulkMeta, BulkChunkId.GetPackageId());
}

FString FBulkData::GetDebugName() const
{
	return BulkChunkId.ToDebugString();
}

/*-----------------------------------------------------------------------------
	Serialization.
-----------------------------------------------------------------------------*/

int32 GMinimumBulkDataSizeForAsyncLoading = 131072;
static FAutoConsoleVariableRef CVarMinimumBulkDataSizeForAsyncLoading(
	TEXT("s.MinBulkDataSizeForAsyncLoading"),
	GMinimumBulkDataSizeForAsyncLoading,
	TEXT("Minimum time the time limit exceeded warning will be triggered by."),
	ECVF_Default
	);

bool FBulkData::NeedsOffsetFixup() const
{
	return BulkMeta.HasAnyFlags(BULKDATA_NoOffsetFixUp) == false;
}

void FBulkData::SetBulkDataFlagsOn(EBulkDataFlags& InOutAccumulator, EBulkDataFlags FlagsToSet)
{
	InOutAccumulator = static_cast<EBulkDataFlags>(InOutAccumulator | FlagsToSet);
}

void FBulkData::ClearBulkDataFlagsOn(EBulkDataFlags& InOutAccumulator, EBulkDataFlags FlagsToClear)
{
	InOutAccumulator = static_cast<EBulkDataFlags>(InOutAccumulator & ~FlagsToClear);
}

void FBulkData::Serialize(FArchive& Ar, UObject* Owner, bool bAttemptFileMapping, int32 ElementSize, EFileRegionType FileRegionType)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FBulkData::Serialize"), STAT_UBD_Serialize, STATGROUP_Memory);

	SCOPED_LOADTIMER(BulkData_Serialize);

	check(IsUnlocked());
	
	check(!bAttemptFileMapping || Ar.IsLoading()); // makes no sense to map unless we are loading

#if !USE_RUNTIME_BULKDATA
	if (Ar.IsTransacting())
	{
		// Special case for transacting bulk data arrays.

		// Constructing the object during load will save it to the transaction buffer.
		// We need to cancel that save because trying to load the bulk data now would break.
		bool bActuallySave = Ar.IsSaving() && (!Owner || !Owner->HasAnyFlags(RF_NeedLoad));

		Ar << bActuallySave;

		if (bActuallySave)
		{
			if (Ar.IsLoading())
			{
				// Flags for bulk data.
				EBulkDataFlags BulkDataFlags;
				Ar << BulkDataFlags;
				BulkMeta.SetFlags(BulkDataFlags);
				
				// Number of elements in array.
				int64 ElementCount;
				Ar << ElementCount;

				BulkMeta.SetSize(ElementCount * ElementSize);

				// Allocate bulk data.
				void* DataBuffer = ReallocateData(GetBulkDataSize());

				// Deserialize bulk data.
				SerializeBulkData(Ar, DataBuffer, GetBulkDataSize(), BulkMeta.GetFlags());
			}
			else if (Ar.IsSaving())
			{
				// Size of bulk data
				const int64 BulkDataSize = BulkMeta.GetSize();
				// Flags for bulk data.
				EBulkDataFlags BulkDataFlags = BulkMeta.GetFlags();
				Ar << BulkDataFlags;
				// Number of elements in array.
				int64 ElementCount = BulkDataSize / ElementSize;
				Ar << ElementCount;

				// Don't attempt to load or serialize BulkData if the current size is 0.
				// This could be a newly constructed BulkData that has not yet been loaded, 
				// and allocating 0 bytes now will cause a crash when we load.
				if (BulkDataSize > 0)
				{
					// Make sure bulk data is loaded.
					MakeSureBulkDataIsLoaded();

					// Serialize bulk data.
					SerializeBulkData(Ar, GetDataBufferForWrite(), BulkDataSize, BulkDataFlags);
				}
			}
		}
	}
	else
#endif // !USE_RUNTIME_BULKDATA
	if (Ar.IsPersistent() && !Ar.IsObjectReferenceCollector() && !Ar.ShouldSkipBulkData())
	{
#if TRACK_BULKDATA_USE
		FThreadSafeBulkDataToObjectMap::Get().Add( this, Owner );
#endif
		if (Ar.IsLoading())
		{
			using namespace UE::BulkData::Private;
			checkf(IsUnlocked(), TEXT("Serialize bulk data FAILED, bulk data is locked"));

			UPackage* Package					= Owner ? Owner->GetPackage() : nullptr;
			FArchive* CacheableAr				= Ar.GetCacheableArchive();
			const bool bCanLazyLoad				= CacheableAr != nullptr && Ar.IsAllowingLazyLoading();
			FLinkerLoad* LinkerLoad				= nullptr;

			FBulkMetaResource MetaResource;
			Ar << MetaResource;
#if WITH_EDITOR
			if (Owner == nullptr)
			{
				// Temp fix for accidentially uploading bulk data with wrong flags to DDC
				ClearBulkDataFlagsOn(MetaResource.Flags, static_cast<EBulkDataFlags>(BULKDATA_PayloadAtEndOfFile | BULKDATA_PayloadInSeperateFile | BULKDATA_WorkspaceDomainPayload));
			}
#endif
			BulkMeta = FBulkMetaData::FromSerialized(MetaResource, ElementSize);

			bool bIsUsingIoDispatcher = false;
			if (Ar.IsLoadingFromCookedPackage())
			{
				BulkMeta.SetMetaFlags(FBulkMetaData::EMetaFlags::CookedPackage);
				bIsUsingIoDispatcher = Package && Package->GetPackageId().IsValid();
			}

			check(MetaResource.ElementCount <= 0 || BulkMeta.GetSize() == MetaResource.ElementCount * ElementSize);
#if !USE_RUNTIME_BULKDATA
			check(MetaResource.ElementCount <= 0 || BulkMeta.GetSizeOnDisk() == MetaResource.SizeOnDisk);
#endif
			check(BulkMeta.GetOffset() == MetaResource.Offset);
			check(BulkMeta.GetFlags() == MetaResource.Flags);

			if (GIsEditor)
			{
				ClearBulkDataFlags(BULKDATA_SingleUse);
			}

			if (bIsUsingIoDispatcher)
			{
				if (IsInlined() == false)
				{
					check(IsInSeparateFile());
					check(Package != nullptr);
					BulkMeta.SetFlags(EBulkDataFlags(BulkMeta.GetFlags() | BULKDATA_UsesIoDispatcher));
					BulkChunkId = FBulkDataChunkId::FromPackageId(Package->GetPackageIdToLoad());
				}
			}
			else
			{
				if (Owner)
				{
					if (LinkerLoad = Owner->GetLinker(); LinkerLoad == nullptr)
					{
						LinkerLoad = FLinkerLoad::FindExistingLinkerForPackage(Package);
					}
					
					if (LinkerLoad != nullptr)
					{
						BulkChunkId = FBulkDataChunkId::FromPackagePath(LinkerLoad->GetPackagePath());
					}
#if WITH_EDITOR
					Linker = LinkerLoad;
					if (bCanLazyLoad)
					{
						check(CacheableAr->IsTextFormat() == false);
						AttachedAr = CacheableAr;
						AttachedAr->AttachBulkData(Owner, this);
					}
#endif // WITH_EDITOR
				}
			}

			if (IsInlined())
			{
				checkf(bAttemptFileMapping == false || bIsUsingIoDispatcher == false, TEXT("Trying to memory map inline bulk data '%s' which is not supported when loading from I/O store"), *Owner->GetPackage()->GetFName().ToString());

				if (LinkerLoad && Ar.IsLoadingFromCookedPackage())
				{
					// Cooked packages are split into .uasset/.exp files and the offset needs to be adjusted accordingly.
					const int64 PackageHeaderSize = IPackageResourceManager::Get().FileSize(LinkerLoad->GetPackagePath(), EPackageSegment::Header);
					BulkMeta.SetOffset(BulkMeta.GetOffset() - PackageHeaderSize);
				}

				const int64 BulkDataSize = GetBulkDataSize();
				void* DataBuffer = ReallocateData(BulkDataSize);
				SerializeBulkData(Ar, DataBuffer, BulkDataSize, BulkMeta.GetFlags());

				ConditionalSetInlineAlwaysAllowDiscard(bIsUsingIoDispatcher);
			}
			else
			{
				if (NeedsOffsetFixup())
				{
					check(LinkerLoad);
					check(bIsUsingIoDispatcher == false);
					BulkMeta.SetOffset(BulkMeta.GetOffset() + LinkerLoad->Summary.BulkDataStartOffset);
				}

				if (IsInSeparateFile())
				{
					SetBulkDataFlags(BULKDATA_LazyLoadable);

					if (IsDuplicateNonOptional())
					{
						const bool bOptionalDataExist = DoesBulkDataExist(FBulkMetaData(BULKDATA_OptionalPayload), BulkChunkId);

						if (bOptionalDataExist)
						{
							BulkMeta.SetFlags(static_cast<EBulkDataFlags>((MetaResource.DuplicateFlags & ~BULKDATA_DuplicateNonOptionalPayload) | BULKDATA_OptionalPayload | BULKDATA_PayloadInSeperateFile | BULKDATA_PayloadAtEndOfFile));
							BulkMeta.SetOffset(MetaResource.DuplicateOffset);
							BulkMeta.SetSizeOnDisk(MetaResource.DuplicateSizeOnDisk);

							if (bIsUsingIoDispatcher)
							{
								BulkMeta.SetFlags(EBulkDataFlags(BulkMeta.GetFlags() | BULKDATA_UsesIoDispatcher));
							}

							if (NeedsOffsetFixup())
							{
								checkf(bIsUsingIoDispatcher == false, TEXT("Loading bulk data from I/O store doesn't require offset fixup, use flag BULKDATA_NoOffsetFixUp when cooking"));
								checkf(LinkerLoad != nullptr, TEXT("BulkData needs its offset fixed on load but no linker found"));
								BulkMeta.SetOffset(BulkMeta.GetOffset() + LinkerLoad->Summary.BulkDataStartOffset);
							}
						}
					}

					bool bFileMappingFailed = false;
					if (bAttemptFileMapping && (bCanLazyLoad || bIsUsingIoDispatcher))
					{
						FIoMappedRegion MappedRegion;
						if (TryMemoryMapBulkData(BulkMeta, BulkChunkId, BulkMeta.GetOffset(), BulkMeta.GetSize(), MappedRegion))
						{
							DataAllocation.SetMemoryMappedData(this, MappedRegion.MappedFileHandle, MappedRegion.MappedFileRegion);
						}
						else
						{
							bFileMappingFailed = true;
						}
					}

					if (bFileMappingFailed || (bCanLazyLoad == false && bIsUsingIoDispatcher == false))
					{
						UE_CLOG(bFileMappingFailed, LogSerialization, Warning, TEXT("Memory map bulk data '%s' FAILED"), *BulkChunkId.ToDebugString());
						ForceBulkDataResident();
					}
				}
				else
				{
					check(bIsUsingIoDispatcher == false);
					check(Ar.IsLoadingFromCookedPackage() == false);

					if (bCanLazyLoad)
					{
						SetBulkDataFlags(BULKDATA_LazyLoadable);
					}
					else
					{
						ClearBulkDataFlags(BULKDATA_LazyLoadable);

						const int64 BulkDataSize = GetBulkDataSize();
						const int64 BulkDataOffset = BulkMeta.GetOffset();
						const int64 SavedPos = Ar.Tell();

						Ar.Seek(BulkDataOffset);

						void* DataBuffer = ReallocateData(BulkDataSize);
						SerializeBulkData(Ar, DataBuffer, BulkDataSize, BulkMeta.GetFlags());

						Ar.Seek(SavedPos);
					}
				}
			}
		}
#if !USE_RUNTIME_BULKDATA
		else if (Ar.IsSaving())
		{
			// Make sure bulk data is loaded.
			MakeSureBulkDataIsLoaded();

			// Make mutable copies of the bulkdata location variables
			EBulkDataFlags LocalBulkDataFlags = BulkMeta.GetFlags();
			int64 LocalBulkDataSize = GetBulkDataSize(); 
			int64 LocalBulkDataSizeOnDisk = GetBulkDataSizeOnDisk(); 
			int64 LocalBulkDataOffsetInFile = BulkMeta.GetOffset();

			// If the bulk data size is greater than can be held in an int32, then potentially the ElementCount
			// and BulkDataSizeOnDisk need to be held as int64s, so set a flag indicating the new format.
			if (GetBulkDataSize() >= (1LL << 31))
			{
				SetBulkDataFlagsOn(LocalBulkDataFlags, BULKDATA_Size64Bit);
			}
			// Remove single element serialization requirement before saving out bulk data flags.
			ClearBulkDataFlagsOn(LocalBulkDataFlags, BULKDATA_ForceSingleElementSerialization);

			// Save offset where we are serializing BulkDataFlags and store a placeholder
			int64 SavedBulkDataFlagsPos = Ar.Tell();
			{
				Ar << LocalBulkDataFlags;
			}

			// Number of elements in array.
			int64 ElementCount = LocalBulkDataSize / ElementSize;
			SerializeBulkDataSizeInt(Ar, ElementCount, LocalBulkDataFlags);

			// Only serialize status information if wanted.
			int64 SavedBulkDataSizeOnDiskPos	= INDEX_NONE;
			int64 SavedBulkDataOffsetInFilePos	= INDEX_NONE;
			
			{
				// Save offset where we are serializing BulkDataSizeOnDisk and store a placeholder
				SavedBulkDataSizeOnDiskPos = Ar.Tell();
				LocalBulkDataSizeOnDisk = INDEX_NONE;
				SerializeBulkDataSizeInt(Ar, LocalBulkDataSizeOnDisk, LocalBulkDataFlags);

				// Save offset where we are serializing BulkDataOffsetInFile and store a placeholder
				SavedBulkDataOffsetInFilePos = Ar.Tell();
				LocalBulkDataOffsetInFile = INDEX_NONE;
				Ar << LocalBulkDataOffsetInFile;
			}

			// try to get the linkersave object
			FLinkerSave* LinkerSave = Cast<FLinkerSave>(Ar.GetLinker());

			// determine whether we are going to store the payload inline or not.
			bool bStoreInline = !!(LocalBulkDataFlags & BULKDATA_ForceInlinePayload) || !LinkerSave || Ar.IsTextFormat();
			if (Ar.IsCooking() && !(LocalBulkDataFlags & BULKDATA_Force_NOT_InlinePayload))
			{
				bStoreInline = true;
			}

			if (!bStoreInline)
			{
				// set the flag indicating where the payload is stored
				SetBulkDataFlagsOn(LocalBulkDataFlags, BULKDATA_PayloadAtEndOfFile);
				ClearBulkDataFlagsOn(LocalBulkDataFlags,
					static_cast<EBulkDataFlags>(BULKDATA_PayloadInSeperateFile | BULKDATA_WorkspaceDomainPayload)); // SavePackageUtilities::SaveBulkData will add these back if required

				// with no LinkerSave we have to store the data inline
				check(LinkerSave != NULL);

				// add the bulkdata storage info object to the linkersave
				FLinkerSave::FBulkDataStorageInfo& BulkStore = LinkerSave->BulkDataToAppend.AddZeroed_GetRef();

				BulkStore.BulkDataOffsetInFilePos = SavedBulkDataOffsetInFilePos;
				BulkStore.BulkDataSizeOnDiskPos = SavedBulkDataSizeOnDiskPos;
				BulkStore.BulkDataFlagsPos = SavedBulkDataFlagsPos;
				BulkStore.BulkDataFlags = LocalBulkDataFlags;
				BulkStore.BulkDataFileRegionType = FileRegionType;
				BulkStore.BulkData = this;

				// If having flag BULKDATA_DuplicateNonOptionalPayload, duplicate bulk data in optional storage (.uptnl)
				if (LocalBulkDataFlags & BULKDATA_DuplicateNonOptionalPayload)
				{
					int64 SavedDupeBulkDataFlagsPos = INDEX_NONE;
					int64 SavedDupeBulkDataSizeOnDiskPos = INDEX_NONE;
					int64 SavedDupeBulkDataOffsetInFilePos = INDEX_NONE;

					EBulkDataFlags SavedDupeBulkDataFlags = static_cast<EBulkDataFlags>(
						(LocalBulkDataFlags & ~BULKDATA_DuplicateNonOptionalPayload) | BULKDATA_OptionalPayload);
					{
						// Save offset where we are serializing SavedDupeBulkDataFlags and store a placeholder
						SavedDupeBulkDataFlagsPos = Ar.Tell();
						Ar << SavedDupeBulkDataFlags;

						// Save offset where we are serializing SavedDupeBulkDataSizeOnDisk and store a placeholder
						SavedDupeBulkDataSizeOnDiskPos = Ar.Tell();
						int64 DupeBulkDataSizeOnDisk = INDEX_NONE;
						SerializeBulkDataSizeInt(Ar, DupeBulkDataSizeOnDisk, SavedDupeBulkDataFlags);

						// Save offset where we are serializing SavedDupeBulkDataOffsetInFile and store a placeholder
						SavedDupeBulkDataOffsetInFilePos = Ar.Tell();
						int64 DupeBulkDataOffsetInFile = INDEX_NONE;
						Ar << DupeBulkDataOffsetInFile;
					}

					// add duplicate bulkdata with different flag
					FLinkerSave::FBulkDataStorageInfo& DupeBulkStore = LinkerSave->BulkDataToAppend.AddZeroed_GetRef();

					DupeBulkStore.BulkDataOffsetInFilePos = SavedDupeBulkDataOffsetInFilePos;
					DupeBulkStore.BulkDataSizeOnDiskPos = SavedDupeBulkDataSizeOnDiskPos;
					DupeBulkStore.BulkDataFlagsPos = SavedDupeBulkDataFlagsPos;
					DupeBulkStore.BulkDataFlags = SavedDupeBulkDataFlags;
					DupeBulkStore.BulkDataFileRegionType = FileRegionType;
					DupeBulkStore.BulkData = this;
				}
			}
			else
			{
				// set the flag indicating where the payload is stored
				ClearBulkDataFlagsOn(LocalBulkDataFlags,
					static_cast<EBulkDataFlags>(BULKDATA_PayloadAtEndOfFile | BULKDATA_PayloadInSeperateFile | BULKDATA_WorkspaceDomainPayload));

				int64 SavedBulkDataStartPos = Ar.Tell();

				// Serialize bulk data.
				if (FileRegionType != EFileRegionType::None)
				{
					Ar.PushFileRegionType(FileRegionType);
				}
				SerializeBulkData(Ar, GetDataBufferForWrite(), LocalBulkDataFlags);
				if (FileRegionType != EFileRegionType::None)
				{
					Ar.PopFileRegionType();
				}

				// store the payload endpos
				int64 SavedBulkDataEndPos = Ar.Tell();

				checkf(SavedBulkDataStartPos >= 0 && SavedBulkDataEndPos >= 0,
					TEXT("Bad archive positions for bulkdata. StartPos=%d EndPos=%d"),
					SavedBulkDataStartPos, SavedBulkDataEndPos);

				LocalBulkDataSizeOnDisk = SavedBulkDataEndPos - SavedBulkDataStartPos;
				LocalBulkDataOffsetInFile = SavedBulkDataStartPos;

				// Since we are storing inline we are not relying on SavePackageUtilities::SaveBulkData to update the placeholder
				// location data, so we need to do it here.

				// store current file offset before seeking back
				int64 CurrentFileOffset = Ar.Tell();
				{
					// Seek back and overwrite the flags 
					Ar.Seek(SavedBulkDataFlagsPos);
					Ar << LocalBulkDataFlags;

					// Seek back and overwrite placeholder for BulkDataSizeOnDisk
					Ar.Seek(SavedBulkDataSizeOnDiskPos);
					SerializeBulkDataSizeInt(Ar, LocalBulkDataSizeOnDisk, LocalBulkDataFlags);

					// Seek back and overwrite placeholder for BulkDataOffsetInFile
					Ar.Seek(SavedBulkDataOffsetInFilePos);
					Ar << LocalBulkDataOffsetInFile;
				}
				// Seek to the end of written data so we don't clobber any data in subsequent writes
				Ar.Seek(CurrentFileOffset);

#if WITH_EDITOR
				// If we are overwriting the LoadedPath for the current package, set the location variables on *this equal to the new values
				// that we are writing into the package on disk
				if (LinkerSave && LinkerSave->bUpdatingLoadedPath)
				{
					SetFlagsFromDiskWrittenValues(LocalBulkDataFlags, LocalBulkDataOffsetInFile, LocalBulkDataSizeOnDisk,
						INDEX_NONE /* LinkerSummaryBulkDataStartOffset, not applicable */);
				}
#endif
			}
		}
#endif // !USE_RUNTIME_BULKDATA
	}
}

#if WITH_EDITOR

void FBulkData::SetFlagsFromDiskWrittenValues(
	EBulkDataFlags InBulkDataFlags,
	int64 InBulkDataOffsetInFile,
	int64 InBulkDataSizeOnDisk,
	int64 LinkerSummaryBulkDataStartOffset)
{
	check(!(InBulkDataFlags & BULKDATA_BadDataVersion)); // This is a legacy flag that should no longer be set when saving
	if (GIsEditor)
	{
		InBulkDataFlags = static_cast<EBulkDataFlags>(InBulkDataFlags & ~BULKDATA_SingleUse);
	}

	// We are no longer loading from iostore, even if we were before; our data is now stored in the loose file we saved
	InBulkDataFlags = static_cast<EBulkDataFlags>(InBulkDataFlags & ~BULKDATA_UsesIoDispatcher);

	const bool bPayloadInline = !(InBulkDataFlags & BULKDATA_PayloadAtEndOfFile);
	const bool bPayloadInSeparateFile = !bPayloadInline && (InBulkDataFlags & BULKDATA_PayloadInSeperateFile);
	// fix up the file offset if the offset is relative to the file's bulkdata section
	if (!bPayloadInline && NeedsOffsetFixup())
	{
		check(LinkerSummaryBulkDataStartOffset >= 0);
		InBulkDataOffsetInFile += LinkerSummaryBulkDataStartOffset;
	}

	BulkMeta.SetFlags(InBulkDataFlags);
	BulkMeta.SetOffset(InBulkDataOffsetInFile);
	BulkMeta.SetSizeOnDisk(InBulkDataSizeOnDisk);
}
#endif

FCustomVersionContainer FBulkData::GetCustomVersions(FArchive& InlineArchive) const
{
	FPackageFileVersion OutUEVersion;
	int32 OutLicenseeUEVersion;
	FCustomVersionContainer OutCustomVersions;
	GetBulkDataVersions(InlineArchive, OutUEVersion, OutLicenseeUEVersion, OutCustomVersions);
	return OutCustomVersions;
}

void FBulkData::GetBulkDataVersions(FArchive& InlineArchive, FPackageFileVersion& OutUEVersion,
	int32& OutLicenseeUEVersion, FCustomVersionContainer& OutCustomVersions) const
{
	if (!IsInSeparateFile())
	{
		OutUEVersion = InlineArchive.UEVer();
		OutLicenseeUEVersion = InlineArchive.LicenseeUEVer();
		OutCustomVersions = InlineArchive.GetCustomVersions();
	}
	else if (!IsInExternalResource())
	{
		// The BulkData is in a sidecar file. These files were created with the same custom versions that
		// the package file containing the BulkData used
		OutUEVersion = InlineArchive.UEVer();
		OutLicenseeUEVersion = InlineArchive.LicenseeUEVer();
		OutCustomVersions = InlineArchive.GetCustomVersions();
	}
	else
	{
		// Read the CustomVersions out of the separate package file
		const FPackagePath& PackagePath = BulkChunkId.GetPackagePath();
		TUniquePtr<FArchive> ExternalArchive = IPackageResourceManager::Get().OpenReadExternalResource(
			EPackageExternalResource::WorkspaceDomainFile, PackagePath.GetPackageName());
		if (ExternalArchive.IsValid())
		{
			FPackageFileSummary PackageFileSummary;
			*ExternalArchive << PackageFileSummary;
			if (PackageFileSummary.Tag == PACKAGE_FILE_TAG && !ExternalArchive->IsError())
			{
				OutUEVersion = PackageFileSummary.GetFileVersionUE();
				OutLicenseeUEVersion = PackageFileSummary.GetFileVersionLicenseeUE();
				OutCustomVersions = PackageFileSummary.GetCustomVersionContainer();
				return;
			}
		}
		OutUEVersion = InlineArchive.UEVer();
		OutLicenseeUEVersion = InlineArchive.LicenseeUEVer();
		OutCustomVersions = InlineArchive.GetCustomVersions();
	}
}

/*-----------------------------------------------------------------------------
	Accessors for friend classes FLinkerLoad and content cookers.
-----------------------------------------------------------------------------*/

#if WITH_EDITOR
/**
 * Detaches the bulk data from the passed in archive. Needs to match the archive we are currently
 * attached to.
 *
 * @param Ar						Archive to detach from
 * @param bEnsureBulkDataIsLoaded	whether to ensure that bulk data is load before detaching from archive
 */
void FBulkData::DetachFromArchive( FArchive* Ar, bool bEnsureBulkDataIsLoaded )
{
	check( Ar );
	check( Ar == AttachedAr || AttachedAr == nullptr || AttachedAr->IsProxyOf(Ar) );
	check( IsUnlocked() );

	// Make sure bulk data is loaded.
	if( bEnsureBulkDataIsLoaded )
	{
		MakeSureBulkDataIsLoaded();
	}

	// Detach from archive.
	AttachedAr = nullptr;
	Linker = nullptr;
	BulkChunkId = UE::BulkData::Private::FBulkDataChunkId();
}
#endif // WITH_EDITOR

void FBulkData::StoreCompressedOnDisk(ECompressionFlags CompressionFlags)
{
	StoreCompressedOnDisk(FCompression::GetCompressionFormatFromDeprecatedFlags(CompressionFlags));
}

void FBulkData::StoreCompressedOnDisk( FName CompressionFormat )
{
	if( CompressionFormat != GetDecompressionFormat() )
	{
		//Need to force this to be resident so we don't try to load data as though it were compressed when it isn't.
		ForceBulkDataResident();

		if( CompressionFormat == NAME_None )
		{
			// clear all compression settings
			ClearBulkDataFlags(BULKDATA_SerializeCompressed);
		}
		else
		{
			// right BulkData only knows zlib
			check(CompressionFormat == NAME_Zlib);
			const uint32 FlagToSet = CompressionFormat == NAME_Zlib ? BULKDATA_SerializeCompressedZLIB : BULKDATA_None;
			SetBulkDataFlags(FlagToSet);

			// make sure we are not forcing the bulkdata to be stored inline if we use compression
			ClearBulkDataFlags(BULKDATA_ForceInlinePayload);
		}
	}
}

void FBulkData::SerializeBulkData(FArchive& Ar, void* Data, int64 DataSize, EBulkDataFlags InBulkDataFlags)
{
	SCOPED_LOADTIMER(BulkData_SerializeBulkData);

#if !USE_RUNTIME_BULKDATA
	if (SerializeBulkDataElements != nullptr)
	{
		(*SerializeBulkDataElements)(Ar, Data, DataSize, InBulkDataFlags);
		return;
	}
#endif

	// skip serializing of unused data
	if (InBulkDataFlags & BULKDATA_Unused)
	{
		return;
	}

	if (DataSize == 0)
	{
		return;
	}

	if (InBulkDataFlags & BULKDATA_SerializeCompressed)
	{
		Ar.SerializeCompressed(Data, DataSize, GetDecompressionFormat(InBulkDataFlags), COMPRESS_NoFlags, false);
	}
	else
	{
		Ar.Serialize(Data, DataSize);
	}
}

IAsyncReadFileHandle* FBulkData::OpenAsyncReadHandle() const
{
	return UE::BulkData::Private::OpenAsyncReadBulkData(BulkMeta, BulkChunkId).Release();
}

IBulkDataIORequest* FBulkData::CreateStreamingRequest(EAsyncIOPriorityAndFlags Priority, FBulkDataIORequestCallBack* CompleteCallback, uint8* UserSuppliedMemory) const
{
	const int64 DataSize = GetBulkDataSize();

	return CreateStreamingRequest(0, DataSize, Priority, CompleteCallback, UserSuppliedMemory);
}

IBulkDataIORequest* FBulkData::CreateStreamingRequest(int64 OffsetInBulkData, int64 BytesToRead, EAsyncIOPriorityAndFlags Priority, FBulkDataIORequestCallBack* CompleteCallback, uint8* UserSuppliedMemory) const
{
	return UE::BulkData::Private::CreateStreamingRequest(
		BulkMeta,
		BulkChunkId,
		BulkMeta.GetOffset() + OffsetInBulkData,
		BytesToRead,
		Priority,
		CompleteCallback,
		UserSuppliedMemory).Release();
}

IBulkDataIORequest* FBulkData::CreateStreamingRequestForRange(const BulkDataRangeArray& RangeArray, EAsyncIOPriorityAndFlags Priority, FBulkDataIORequestCallBack* CompleteCallback)
{	
	check(RangeArray.Num() > 0);

	const FBulkData& Start = *(RangeArray[0]);
	const FBulkData& End = *(RangeArray[RangeArray.Num()-1]);
	const int64 ReadOffset = Start.GetBulkDataOffsetInFile();
	const int64 ReadSize = (End.GetBulkDataOffsetInFile() + End.GetBulkDataSize()) - ReadOffset;
	
	check(ReadSize > 0);

	checkf(
		Start.IsUsingIODispatcher() == false || Start.IsInSeparateFile(),
		TEXT("Create bulkdata stream request from package '%s' FAILED, inline bulk data cannot be streamed from I/O store"),
		*Start.BulkChunkId.ToDebugString());

	checkf(
		Start.IsUsingIODispatcher() == false || (End.IsInSeparateFile() && Start.BulkChunkId.GetPackageId() == End.BulkChunkId.GetPackageId()),
		TEXT("Create bulk data stream request FAILED, range spans from package '%s' to package '%s'"),
		*Start.BulkChunkId.ToDebugString(), *End.BulkChunkId.ToDebugString());

	return UE::BulkData::Private::CreateStreamingRequest(
		Start.BulkMeta,
		Start.BulkChunkId,
		ReadOffset,
		ReadSize,
		Priority,
		CompleteCallback,
		nullptr).Release();
}

/**
 * Loads the bulk data if it is not already loaded.
 */
void FBulkData::MakeSureBulkDataIsLoaded()
{
	FlushAsyncLoading();

	if (IsBulkDataLoaded())
	{
		return;
	}

	const int64 BulkDataSize = GetBulkDataSize();
	void* Dest = ReallocateData(BulkDataSize);

	if (TryLoadDataIntoMemory(FIoBuffer(FIoBuffer::Wrap, Dest, BulkDataSize)) == false)
	{
		DataAllocation.Free(this);
	}
}

void FBulkData::FlushAsyncLoading()
{
	if (IsAsyncLoadingComplete() == false)
	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FBulkData::FlushAsyncLoading"), STAT_UBD_WaitForAsyncLoading, STATGROUP_Memory);
		UE::BulkData::Private::FlushAsyncLoad(this);
	}
}

/**
 * Loads the data from disk into the specified memory block. This requires us still being attached to an
 * archive we can use for serialization.
 *
 * @param Dest Memory to serialize data into
 */
bool FBulkData::TryLoadDataIntoMemory(FIoBuffer Dest)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FBulkData::TryLoadDataIntoMemory"), STAT_UBD_LoadDataDirectly, STATGROUP_Memory);

	const int64 BulkDataOffset = GetBulkDataOffsetInFile();
	const int64 BulkDataSize = GetBulkDataSize();
	const int64 BulkDataSizeOnDisk = GetBulkDataSizeOnDisk();

	// Early out if we have no data to load
	if (BulkDataSize == 0)
	{
		return false;
	}
	
	check(Dest.GetSize() == BulkDataSize);

#if WITH_EDITOR
	// BulkDatas in the same package share AttachedAr that they get from the LinkerLoad of the package.
	// To make calls to those BulkDatas threadsafe, we need to not use AttachedAr when called multithreaded.
	// Also don't use the attached archive when loading from the editor domain and the bulk data is referenced
	// from the original .uasset file
	if (IsInGameThread() && !IsInSeparateFile() && !IsInExternalResource())
	{
		if (FArchive* Ar = AttachedAr)
		{
			const int64 SavedPos = Ar->Tell();
			Ar->Seek(BulkDataOffset);
			SerializeBulkData(*Ar, Dest.GetData(), Dest.GetSize(), BulkMeta.GetFlags());

			Ar->Seek(SavedPos);
			Ar->FlushCache();
		
			return true;
		}
	}
#endif // WITH_EDITOR
	   
	if (CanLoadFromDisk() == false)
	{
		UE_LOG(LogSerialization, Error, TEXT("Attempting to load a BulkData object that cannot be loaded from disk"));
		return false;
	}

	const bool bOk = UE::BulkData::Private::OpenReadBulkData(
		BulkMeta,
		BulkChunkId,
		BulkDataOffset,
		BulkDataSizeOnDisk,
		AIOP_High,
		[this, BulkDataSize, &Dest](FArchive& Ar)
		{
			SerializeBulkData(Ar, Dest.GetData(), BulkDataSize, BulkMeta.GetFlags());
		});

	return bOk;
}

bool FBulkData::CanDiscardInternalData() const
{	
	// Data marked as single use should always be discarded
	if (IsSingleUse())
	{
		return true;
	}

	// If we can load from disk then we can discard it as it can be reloaded later
	if (CanLoadFromDisk())
	{
		return true;
	}

	// If BULKDATA_AlwaysAllowDiscard has been set then we should always allow the data to 
	// be discarded even if it cannot be reloaded again.
	if (BulkMeta.HasAnyFlags(BULKDATA_AlwaysAllowDiscard))
	{
		return true;
	}

	return false;
}

void FBulkData::ConditionalSetInlineAlwaysAllowDiscard(bool bPackageUsesIoStore)
{
	// If PackagePath is null and we do not have BULKDATA_UsesIoDispatcher, we will not be able to reload the bulkdata.
	// We will not have a PackagePath if the engine is using IoStore.
	// So in IoStore with BulkData stored inline, we can not reload the bulkdata.
	// We do not need to consider the end-of-file bulkdata section because IoStore  guarantees during cooking
	// (SaveBulkData) that only inlined data is in the package file; there is no end-of-file bulkdata section.

	// In the Inlined IoStore case therefore we need to not discard the bulk data if !BULKDATA_SingleUse;
	// we have to keep it in case it gets requested twice.
	// However, some systems (Audio,Animation) have large inline data they for legacy reasons have not marked as
	// BULKDATA_SingleUse.
	// In the old loader this data was discarded for them since CanLoadFromDisk was true.
	// Since CanLoadFromDisk is now false, we are keeping that data around, and this causes memory bloat.
	// Licensees have asked us to fix this memory bloat.
	//
	// To hack-fix the memory bloat in these systems when using IoStore we will mark all inline BulkData
	// as discardable when using IoStore.
	// This will cause a bug when using IoStore in any systems that do actually need to reload inline data.
	// Each project that uses IoStore needs to guarantee that they do not have any systems that need to
	// reload inline data.
	// Note that when the define UE_KEEP_INLINE_RELOADING_CONSISTENT is enabled the old loading path should also
	// not allow the reloading of inline data so that it behaves the same way as the new loading path. So when it
	// is enabled we do not need to check if the loader is or isn't enabled, we can just set the flag and assume
	// all inline data should be allowed to be discarded.

#if !UE_KEEP_INLINE_RELOADING_CONSISTENT
	if (bPackageUsesIoStore)
#endif // !UE_KEEP_INLINE_RELOADING_CONSISTENT
	{
		SetBulkDataFlags(BULKDATA_AlwaysAllowDiscard);
	}
}

void FUntypedBulkData::SerializeElements(FArchive& Ar, void* Data)
{
	for (int64 ElementIndex = 0, Count = GetElementCount(); ElementIndex < Count; ++ElementIndex)
	{
		SerializeElement(Ar, Data, ElementIndex);
	}
}

void FUntypedBulkData::SerializeBulkData(FArchive& Ar, void* Data, int64 DataSize, EBulkDataFlags InBulkDataFlags)
{
	SCOPED_LOADTIMER(BulkData_SerializeBulkData);

	// skip serializing of unused data
	if (InBulkDataFlags & BULKDATA_Unused)
	{
		return;
	}

	// Skip serialization for bulk data of zero length
	if (DataSize == 0)
	{
		return;
	}

	// Allow backward compatible serialization by forcing bulk serialization off if required. Saving also always uses single
	// element serialization so errors or oversight when changing serialization code is recoverable.
	bool bSerializeInBulk = true;
	if (RequiresSingleElementSerialization(Ar) 
	// Set when serialized like a lazy array.
	|| (InBulkDataFlags & BULKDATA_ForceSingleElementSerialization)
	// We use bulk serialization even when saving 1 byte types (texture & sound bulk data) as an optimization for those.
	|| (Ar.IsSaving() && (GetElementSize() > 1)))
	{
		bSerializeInBulk = false;
	}

	// Raw serialize the bulk data without any possibility for potential endian conversion.
	if (bSerializeInBulk)
	{
		// Serialize data compressed.
		if (InBulkDataFlags & BULKDATA_SerializeCompressed)
		{
			Ar.SerializeCompressed(Data, DataSize,
				GetDecompressionFormat(InBulkDataFlags), COMPRESS_NoFlags, false);
		}
		// Uncompressed/ regular serialization.
		else
		{
			Ar.Serialize(Data, DataSize);
		}
	}
	// Serialize an element at a time via the virtual SerializeElement function potentially allowing and dealing with 
	// endian conversion. Dealing with compression makes this a bit more complex as SerializeCompressed expects the 
	// full data to be compressed en block and not piecewise.
	else
	{
		// Serialize data compressed.
		if (InBulkDataFlags & BULKDATA_SerializeCompressed)
		{
			// Loading, data is compressed in archive and needs to be decompressed.
			if (Ar.IsLoading())
			{
				TUniquePtr<uint8[]> SerializedData = MakeUnique<uint8[]>(DataSize);

				// Serialize data with passed in archive and compress.
				Ar.SerializeCompressed(SerializedData.Get(), DataSize,
					GetDecompressionFormat(InBulkDataFlags), COMPRESS_NoFlags, false);

				// Initialize memory reader with uncompressed data array and propagate forced byte swapping
				FLargeMemoryReader MemoryReader(SerializedData.Get(), DataSize, ELargeMemoryReaderFlags::Persistent);
				MemoryReader.SetByteSwapping(Ar.ForceByteSwapping());

				// Serialize each element individually via memory reader.
				SerializeElements(MemoryReader, Data);
			}
			// Saving, data is uncompressed in memory and needs to be compressed.
			else if (Ar.IsSaving())
			{
				// Initialize memory writer with blank data array and propagate forced byte swapping
				FLargeMemoryWriter MemoryWriter(GetBulkDataSize(), true);
				MemoryWriter.SetByteSwapping(Ar.ForceByteSwapping());

				// Serialize each element individually via memory writer.
				SerializeElements(MemoryWriter, Data);

				// Serialize data with passed in archive and compress.
				Ar.SerializeCompressed(MemoryWriter.GetData(), DataSize,
					GetDecompressionFormat(InBulkDataFlags), COMPRESS_NoFlags, false);
			}
		}
		// Uncompressed/ regular serialization.
		else
		{
			// We can use the passed in archive if we're not compressing the data.
			SerializeElements(Ar, Data);
		}
	}
}

void FFormatContainer::Serialize(FArchive& Ar, UObject* Owner, const TArray<FName>* FormatsToSave, bool bSingleUse, uint16 InAlignment, bool bInline, bool bMapped)
{
	if (Ar.IsLoading())
	{
		int32 NumFormats = 0;
		Ar << NumFormats;
		for (int32 Index = 0; Index < NumFormats; Index++)
		{
			FName Name;
			Ar << Name;
			FByteBulkData& Bulk = GetFormat(Name);
			Bulk.Serialize(Ar, Owner, INDEX_NONE, false);
		}
	}
	else
	{
		check(Ar.IsCooking() && FormatsToSave); // this thing is for cooking only, and you need to provide a list of formats

		int32 NumFormats = 0;
		for (const TPair<FName, FByteBulkData*>& Format : Formats)
		{
			const FName Name = Format.Key;
			FByteBulkData* Bulk = Format.Value;
			check(Bulk);
			if (FormatsToSave->Contains(Name) && Bulk->GetBulkDataSize() > 0)
			{
				NumFormats++;
			}
		}
		Ar << NumFormats;
		for (const TPair<FName, FByteBulkData*>& Format : Formats)
		{
			FName Name = Format.Key;
			FByteBulkData* Bulk = Format.Value;
			if (FormatsToSave->Contains(Name) && Bulk->GetBulkDataSize() > 0)
			{
				NumFormats--;
				Ar << Name;
				// Force this kind of bulk data (physics, etc) to be stored inline for streaming
				const uint32 OldBulkDataFlags = Bulk->GetBulkDataFlags();
				if (bInline)
				{
					Bulk->SetBulkDataFlags(BULKDATA_ForceInlinePayload);
					Bulk->ClearBulkDataFlags(BULKDATA_Force_NOT_InlinePayload | BULKDATA_MemoryMappedPayload);
				}
				else
				{
					Bulk->SetBulkDataFlags(BULKDATA_Force_NOT_InlinePayload);
					if (bMapped)
					{
						Bulk->SetBulkDataFlags(BULKDATA_MemoryMappedPayload);
					}
					Bulk->ClearBulkDataFlags(BULKDATA_ForceInlinePayload);

				}
				if (bSingleUse)
				{
					Bulk->SetBulkDataFlags(BULKDATA_SingleUse);
				}
				Bulk->Serialize(Ar, Owner, INDEX_NONE, false);
				Bulk->ClearBulkDataFlags(0xFFFFFFFF);
				Bulk->SetBulkDataFlags(OldBulkDataFlags);
			}
		}
		check(NumFormats == 0);
	}
}

void FFormatContainer::SerializeAttemptMappedLoad(FArchive& Ar, UObject* Owner)
{
	check(Ar.IsLoading());
	int32 NumFormats = 0;
	Ar << NumFormats;
	for (int32 Index = 0; Index < NumFormats; Index++)
	{
		FName Name;
		Ar << Name;
		FByteBulkData& Bulk = GetFormat(Name);
		Bulk.Serialize(Ar, Owner, -1, true);
	}
}
