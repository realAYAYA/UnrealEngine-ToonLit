// Copyright Epic Games, Inc. All Rights Reserved.

#include "PakFileCacheStore.h"

#include "Algo/Accumulate.h"
#include "Algo/StableSort.h"
#include "Algo/Transform.h"
#include "Compression/OodleDataCompression.h"
#include "DerivedDataBackendInterface.h"
#include "DerivedDataCachePrivate.h"
#include "DerivedDataCacheRecord.h"
#include "DerivedDataCacheUsageStats.h"
#include "DerivedDataChunk.h"
#include "DerivedDataRequestOwner.h"
#include "DerivedDataValue.h"
#include "HAL/CriticalSection.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "HashingArchiveProxy.h"
#include "Misc/Compression.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "Misc/ScopeRWLock.h"
#include "ProfilingDebugging/CookStats.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinaryPackage.h"
#include "Serialization/CompactBinaryValidation.h"
#include "Serialization/CompactBinaryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Templates/Greater.h"
#include "Templates/UniquePtr.h"

namespace UE::DerivedData
{

/**
 * A simple thread safe, pak file based backend.
 */
class FPakFileCacheStore : public IPakFileCacheStore
{
public:
	FPakFileCacheStore(const TCHAR* InFilename, bool bInWriting);
	~FPakFileCacheStore();

	void Close() final;

	bool IsWritable() const final { return bWriting && !bClosed; }

	bool CachedDataProbablyExists(const TCHAR* CacheKey);
	bool GetCachedData(const TCHAR* CacheKey, TArray<uint8>& OutData);
	void PutCachedData(const TCHAR* CacheKey, TArrayView<const uint8> InData, bool bPutEvenIfExists);

	/**
	 * Save the cache to disk
	 * @return	true if file was saved successfully
	 */
	bool SaveCache() final;

	/**
	 * Load the cache to disk	 * @param	Filename	Filename to load
	 * @return	true if file was loaded successfully
	 */
	bool LoadCache(const TCHAR* InFilename) final;

	/**
	 * Merges another cache file into this one.
	 * @return true on success
	 */
	void MergeCache(IPakFileCacheStore* OtherPak) final;
	
	const FString& GetFilename() const final
	{
		return CachePath;
	}

	// ICacheStore

	void Put(
		TConstArrayView<FCachePutRequest> Requests,
		IRequestOwner& Owner,
		FOnCachePutComplete&& OnComplete) override;

	void Get(
		TConstArrayView<FCacheGetRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetComplete&& OnComplete) final;

	void PutValue(
		TConstArrayView<FCachePutValueRequest> Requests,
		IRequestOwner& Owner,
		FOnCachePutValueComplete&& OnComplete) override;

	void GetValue(
		TConstArrayView<FCacheGetValueRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetValueComplete&& OnComplete) final;

	void GetChunks(
		TConstArrayView<FCacheGetChunkRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetChunkComplete&& OnComplete) final;

	// ILegacyCacheStore

	void LegacyStats(FDerivedDataCacheStatsNode& OutNode) final;
	bool LegacyDebugOptions(FBackendDebugOptions& Options) final;

private:
	[[nodiscard]] bool PutCacheRecord(FStringView Name, const FCacheRecord& Record, const FCacheRecordPolicy& Policy, uint64& OutWriteSize);

	[[nodiscard]] FOptionalCacheRecord GetCacheRecordOnly(
		FStringView Name,
		const FCacheKey& Key,
		const FCacheRecordPolicy& Policy);
	[[nodiscard]] FOptionalCacheRecord GetCacheRecord(
		FStringView Name,
		const FCacheKey& Key,
		const FCacheRecordPolicy& Policy,
		EStatus& OutStatus);

	[[nodiscard]] bool PutCacheValue(FStringView Name, const FCacheKey& Key, const FValue& Value, ECachePolicy Policy, uint64& OutWriteSize);

	[[nodiscard]] bool GetCacheValueOnly(FStringView Name, const FCacheKey& Key, ECachePolicy Policy, FValue& OutValue);
	[[nodiscard]] bool GetCacheValue(FStringView Name, const FCacheKey& Key, ECachePolicy Policy, FValue& OutValue);

	[[nodiscard]] bool PutCacheContent(FStringView Name, const FCompressedBuffer& Content, uint64& OutWriteSize);

	[[nodiscard]] bool GetCacheContentExists(const FCacheKey& Key, const FIoHash& RawHash);
	[[nodiscard]] bool GetCacheContent(
		FStringView Name,
		const FCacheKey& Key,
		const FValueId& Id,
		const FValue& Value,
		ECachePolicy Policy,
		FValue& OutValue);
	void GetCacheContent(
		const FStringView Name,
		const FCacheKey& Key,
		const FValueId& Id,
		const FValue& Value,
		const ECachePolicy Policy,
		FCompressedBufferReader& Reader,
		TUniquePtr<FArchive>& OutArchive);

	[[nodiscard]] bool SaveFile(FStringView Path, FStringView DebugName, TFunctionRef<void (FArchive&)> WriteFunction);
	[[nodiscard]] FSharedBuffer LoadFile(FStringView Path, FStringView DebugName);
	[[nodiscard]] TUniquePtr<FArchive> OpenFile(FStringBuilderBase& Path, const FStringView DebugName);
	[[nodiscard]] bool FileExists(FStringView Path);

private:
	FDerivedDataCacheUsageStats UsageStats;
	FBackendDebugOptions DebugOptions;

	struct FCacheValue
	{
		int64 Offset;
		int64 Size;
		uint32 Crc;
		FCacheValue(int64 InOffset, int64 InSize, uint32 InCrc)
			: Offset(InOffset)
			, Size(InSize)
			, Crc(InCrc)
		{
		}
	};

	/** When set to true, we are a pak writer (we don't do reads). */
	bool bWriting;
	/** When set to true, we are a pak writer and we saved, so we shouldn't be used anymore. Also, a read cache that failed to open. */
	bool bClosed;
	/** Object used for synchronization via scoped read or write locks. */
	FRWLock SynchronizationObject;
	/** Set of files that are being written to disk asynchronously. */
	TMap<FString, FCacheValue> CacheItems;
	/** File handle of pak. */
	TUniquePtr<IFileHandle> FileHandle;
	/** File name of pak. */
	FString CachePath;

	/** Maximum total size of compressed data stored within a record package with multiple attachments. */
	uint64 MaxRecordSizeKB = 256;
	/** Maximum total size of compressed data stored within a value package, or a record package with one attachment. */
	uint64 MaxValueSizeKB = 1024;

	enum 
	{
		/** Magic number to use in header */
		PakCache_Magic = 0x0c7c0ddc,
	};

	friend class IPakFileCacheStore;
};

FPakFileCacheStore::FPakFileCacheStore(const TCHAR* const InCachePath, const bool bInWriting)
	: bWriting(bInWriting)
	, bClosed(false)
	, CachePath(InCachePath)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (bWriting)
	{
		PlatformFile.CreateDirectoryTree(*FPaths::GetPath(CachePath));
		FileHandle.Reset(PlatformFile.OpenWrite(*CachePath, /*bAppend*/ false, /*bAllowRead*/ true));
		if (!FileHandle)
		{
			UE_LOG(LogDerivedDataCache, Fatal, TEXT("%s: Failed to open pak cache for writing."), *CachePath);
			bClosed = true;
		}
		else
		{
			UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Opened pak cache for writing."), *CachePath);
		}
	}
	else
	{
		FileHandle.Reset(PlatformFile.OpenRead(*CachePath));
		if (!FileHandle)
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Failed to open pak cache for reading."), *CachePath);
		}
		else if (!LoadCache(*CachePath))
		{
			FileHandle.Reset();
			CacheItems.Empty();
			bClosed = true;
		}
		else
		{
			UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Opened pak cache for reading. (%" INT64_FMT " MiB)"),
				*CachePath, FileHandle->Size() / 1024 / 1024);
		}
	}
}

FPakFileCacheStore::~FPakFileCacheStore()
{
	Close();
}

void FPakFileCacheStore::Close()
{
	FDerivedDataBackend::Get().WaitForQuiescence();
	if (!bClosed)
	{
		if (bWriting)
		{
			SaveCache();
		}
		FWriteScopeLock ScopeLock(SynchronizationObject);
		FileHandle.Reset();
		CacheItems.Empty();
		bClosed = true;
	}
}

bool FPakFileCacheStore::CachedDataProbablyExists(const TCHAR* CacheKey)
{
	COOK_STAT(auto Timer = UsageStats.TimeProbablyExists());
	if (bClosed)
	{
		return false;
	}
	FReadScopeLock ScopeLock(SynchronizationObject);
	bool Result = CacheItems.Contains(FString(CacheKey));
	if (Result)
	{
		COOK_STAT(Timer.AddHit(0));
	}
	return Result;
}

bool FPakFileCacheStore::GetCachedData(const TCHAR* CacheKey, TArray<uint8>& OutData)
{
	COOK_STAT(auto Timer = UsageStats.TimeGet());
	if (bClosed)
	{
		return false;
	}
	FWriteScopeLock ScopeLock(SynchronizationObject);
	if (FCacheValue* Item = CacheItems.Find(FString(CacheKey)))
	{
		check(FileHandle);
		ON_SCOPE_EXIT
		{
			if (bWriting)
			{
				FileHandle->SeekFromEnd();
			}
		};
		if (Item->Size >= MAX_int32)
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Pak file, %s exceeds 2 GiB limit."), *CachePath, CacheKey);
		}
		else if (!FileHandle->Seek(Item->Offset))
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Pak file, bad seek."), *CachePath);
		}
		else
		{
			check(Item->Size);
			check(!OutData.Num());
			OutData.AddUninitialized(int32(Item->Size));
			if (!FileHandle->Read(OutData.GetData(), int64(Item->Size)))
			{
				UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Pak file, bad read."), *CachePath);
			}
			else if (uint32 TestCrc = FCrc::MemCrc_DEPRECATED(OutData.GetData(), int32(Item->Size)); TestCrc != Item->Crc)
			{
				UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Pak file, bad crc."), *CachePath);
			}
			else
			{
				UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache hit on %s"), *CachePath, CacheKey);
				check(OutData.Num());
				COOK_STAT(Timer.AddHit(OutData.Num()));
				return true;
			}
		}
	}
	else
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache miss on %s"), *CachePath, CacheKey);
	}
	OutData.Empty();
	return false;
}

void FPakFileCacheStore::PutCachedData(const TCHAR* CacheKey, TArrayView<const uint8> InData, bool bPutEvenIfExists)
{
	COOK_STAT(auto Timer = UsageStats.TimePut());
	if (bWriting && !bClosed)
	{
		FWriteScopeLock ScopeLock(SynchronizationObject);
		FString Key(CacheKey);
		TOptional<uint32> Crc;
		check(InData.Num());
		check(Key.Len());
		check(FileHandle);

		if (bPutEvenIfExists)
		{
			if (FCacheValue* Item = CacheItems.Find(FString(CacheKey)))
			{
				// If there was an existing entry for this key, if it had the same contents, do nothing as the desired value is already stored.
				// If the contents differ, replace it if the size hasn't changed, but if the size has changed, 
				// remove the existing entry from the index but leave they actual data payload in place as it is too
				// costly to go back and attempt to rewrite all offsets and shift all bytes that follow it in the file.
				if (Item->Size == InData.Num())
				{
					COOK_STAT(Timer.AddHit(InData.Num()));
					Crc = FCrc::MemCrc_DEPRECATED(InData.GetData(), InData.Num());
					if (Crc.GetValue() != Item->Crc)
					{
						int64 Offset = FileHandle->Tell();
						FileHandle->Seek(Item->Offset);
						FileHandle->Write(InData.GetData(), InData.Num());
						Item->Crc = Crc.GetValue();
						FileHandle->Seek(Offset);
					}
					return;
				}

				UE_LOG(LogDerivedDataCache, Warning,
					TEXT("%s: Repeated put of %s with different sized contents. Multiple contents will be in the file, "
					     "but only the last will be in the index. This has wasted %" INT64_FMT " bytes in the file."),
					*CachePath, CacheKey, Item->Size);
				CacheItems.Remove(Key);
			}
		}

		int64 Offset = FileHandle->Tell();
		if (Offset < 0)
		{
			CacheItems.Empty();
			FileHandle.Reset();
			UE_LOG(LogDerivedDataCache, Fatal, TEXT("%s: Could not write pak file... out of disk space?"), *CachePath);
		}
		else
		{
			COOK_STAT(Timer.AddHit(InData.Num()));
			if (!Crc.IsSet())
			{
				Crc = FCrc::MemCrc_DEPRECATED(InData.GetData(), InData.Num());
			}
			FileHandle->Write(InData.GetData(), InData.Num());
			UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Put %s"), *CachePath, CacheKey);
			CacheItems.Add(Key, FCacheValue(Offset, InData.Num(), Crc.GetValue()));
		}
	}
}

bool FPakFileCacheStore::SaveCache()
{
	FWriteScopeLock ScopeLock(SynchronizationObject);
	check(FileHandle);
	int64 IndexOffset = FileHandle->Tell();
	check(IndexOffset >= 0);
	uint32 NumItems = uint32(CacheItems.Num());
	check(IndexOffset > 0 || !NumItems);
	TArray<uint8> IndexBuffer;
	{
		FMemoryWriter Saver(IndexBuffer);
		uint32 NumProcessed = 0;
		for (TMap<FString, FCacheValue>::TIterator It(CacheItems); It; ++It )
		{
			FCacheValue& Value = It.Value();
			check(It.Key().Len());
			check(Value.Size);
			check(Value.Offset >= 0 && Value.Offset < IndexOffset);
			Saver << It.Key();
			Saver << Value.Offset;
			Saver << Value.Size;
			Saver << Value.Crc;
			NumProcessed++;
		}
		check(NumProcessed == NumItems);
	}
	uint32 IndexCrc = FCrc::MemCrc_DEPRECATED(IndexBuffer.GetData(), IndexBuffer.Num());
	uint32 SizeIndex = uint32(IndexBuffer.Num());

	uint32 Magic = PakCache_Magic;
	TArray<uint8> Buffer;
	FMemoryWriter Saver(Buffer);
	Saver << Magic;
	Saver << IndexCrc;
	Saver << NumItems;
	Saver << SizeIndex;
	Saver.Serialize(IndexBuffer.GetData(), IndexBuffer.Num());
	Saver << Magic;
	Saver << IndexOffset;
	FileHandle->Write(Buffer.GetData(), Buffer.Num());
	CacheItems.Empty();
	FileHandle.Reset();
	bClosed = true;
	return true;
}

bool FPakFileCacheStore::LoadCache(const TCHAR* InFilename)
{
	check(FileHandle);
	int64 FileSize = FileHandle->Size();
	check(FileSize >= 0);
	if (FileSize < sizeof(int64) + sizeof(uint32) * 5)
	{
		UE_LOG(LogDerivedDataCache, Error, TEXT("%s: Pak cache was corrupted (short)."), InFilename);
		return false;
	}
	int64 IndexOffset = -1;
	int64 Trailer = -1;
	{
		TArray<uint8> Buffer;
		const int64 SeekPos = FileSize - int64(sizeof(int64) + sizeof(uint32));
		FileHandle->Seek(SeekPos);
		Trailer = FileHandle->Tell();
		if (Trailer != SeekPos)
		{
			UE_LOG(LogDerivedDataCache, Error, TEXT("%s: Pak cache was corrupted (bad seek)."), InFilename);
			return false;
		}
		check(Trailer >= 0 && Trailer < FileSize);
		Buffer.AddUninitialized(sizeof(int64) + sizeof(uint32));
		FileHandle->Read(Buffer.GetData(), int64(sizeof(int64)+sizeof(uint32)));
		FMemoryReader Loader(Buffer);
		uint32 Magic = 0;
		Loader << Magic;
		Loader << IndexOffset;
		if (Magic != PakCache_Magic || IndexOffset < 0 || IndexOffset + int64(sizeof(uint32) * 4) > Trailer)
		{
			UE_LOG(LogDerivedDataCache, Error, TEXT("%s: Pak cache was corrupted (bad footer)."), InFilename);
			return false;
		}
	}
	uint32 IndexCrc = 0;
	uint32 NumIndex = 0;
	uint32 SizeIndex = 0;
	{
		TArray<uint8> Buffer;
		FileHandle->Seek(IndexOffset);
		if (FileHandle->Tell() != IndexOffset)
		{
			UE_LOG(LogDerivedDataCache, Error, TEXT("%s: Pak cache was corrupted (bad seek index)."), InFilename);
			return false;
		}
		Buffer.AddUninitialized(sizeof(uint32) * 4);
		FileHandle->Read(Buffer.GetData(), sizeof(uint32) * 4);
		FMemoryReader Loader(Buffer);
		uint32 Magic = 0;
		Loader << Magic;
		Loader << IndexCrc;
		Loader << NumIndex;
		Loader << SizeIndex;
		if (Magic != PakCache_Magic || (SizeIndex != 0 && NumIndex == 0) || (SizeIndex == 0 && NumIndex != 0)) 
		{
			UE_LOG(LogDerivedDataCache, Error, TEXT("%s: Pak cache was corrupted (bad index header)."), InFilename);
			return false;
		}
		if (IndexOffset + sizeof(uint32) * 4 + SizeIndex != Trailer) 
		{
			UE_LOG(LogDerivedDataCache, Error, TEXT("%s: Pak cache was corrupted (bad index size)."), InFilename);
			return false;
		}
	}
	{
		TArray<uint8> Buffer;
		Buffer.AddUninitialized(SizeIndex);
		FileHandle->Read(Buffer.GetData(), SizeIndex);
		FMemoryReader Loader(Buffer);
		while (Loader.Tell() < SizeIndex)
		{
			FString Key;
			int64 Offset;
			int64 Size;
			uint32 Crc;
			Loader << Key;
			Loader << Offset;
			Loader << Size;
			Loader << Crc;
			if (!Key.Len() || Offset < 0 || Offset >= IndexOffset || !Size)
			{
				UE_LOG(LogDerivedDataCache, Error, TEXT("%s: Pak cache was corrupted (bad index entry)."), InFilename);
				return false;
			}
			CacheItems.Add(Key, FCacheValue(Offset, Size, Crc));
		}
		if (CacheItems.Num() != NumIndex)
		{
			UE_LOG(LogDerivedDataCache, Error, TEXT("%s: Pak cache was corrupted (bad index count)."), InFilename);
			return false;
		}
	}
	return true;
}

void FPakFileCacheStore::MergeCache(IPakFileCacheStore* OtherPakInterface)
{
	FPakFileCacheStore* OtherPak = static_cast<FPakFileCacheStore*>(OtherPakInterface);

	// Get all the existing keys
	TArray<FString> KeyNames;
	OtherPak->CacheItems.GenerateKeyArray(KeyNames);

	// Find all the keys to copy
	TArray<FString> CopyKeyNames;
	for(const FString& KeyName : KeyNames)
	{
		if(!CachedDataProbablyExists(*KeyName))
		{
			CopyKeyNames.Add(KeyName);
		}
	}
	UE_LOG(LogDerivedDataCache, Display, TEXT("Merging %d entries (%d skipped)."), CopyKeyNames.Num(), KeyNames.Num() - CopyKeyNames.Num());

	// Copy them all to the new cache. Don't use the overloaded get/put methods (which may compress/decompress); copy the raw data directly.
	TArray<uint8> Buffer;
	for(const FString& CopyKeyName : CopyKeyNames)
	{
		Buffer.Reset();
		if(OtherPak->FPakFileCacheStore::GetCachedData(*CopyKeyName, Buffer))
		{
			FPakFileCacheStore::PutCachedData(*CopyKeyName, Buffer, false);
		}
	}
}

bool IPakFileCacheStore::SortAndCopy(const FString &InputFilename, const FString &OutputFilename)
{
	// Open the input and output files
	FPakFileCacheStore InputPak(*InputFilename, false);
	if (InputPak.bClosed) return false;

	FPakFileCacheStore OutputPak(*OutputFilename, true);
	if (OutputPak.bClosed) return false;

	// Sort the key names
	TArray<FString> KeyNames;
	InputPak.CacheItems.GenerateKeyArray(KeyNames);
	KeyNames.Sort();

	// Copy all the DDC to the new cache
	TArray<uint8> Buffer;
	TArray<uint32> KeySizes;
	for (int KeyIndex = 0; KeyIndex < KeyNames.Num(); KeyIndex++)
	{
		Buffer.Reset();
		// Data over 2 GiB is not copied.
		if (InputPak.GetCachedData(*KeyNames[KeyIndex], Buffer))
		{
			OutputPak.PutCachedData(*KeyNames[KeyIndex], Buffer, false);
		}
		KeySizes.Add(Buffer.Num());
	}

	// Write out a TOC listing for debugging
	FStringOutputDevice Output;
	Output.Logf(TEXT("Asset,Size" LINE_TERMINATOR_ANSI));
	for(int KeyIndex = 0; KeyIndex < KeyNames.Num(); KeyIndex++)
	{
		Output.Logf(TEXT("%s,%d" LINE_TERMINATOR_ANSI), *KeyNames[KeyIndex], KeySizes[KeyIndex]);
	}
	FFileHelper::SaveStringToFile(Output, *FPaths::Combine(*FPaths::GetPath(OutputFilename), *(FPaths::GetBaseFilename(OutputFilename) + TEXT(".csv"))));
	return true;
}

void FPakFileCacheStore::LegacyStats(FDerivedDataCacheStatsNode& OutNode)
{
	OutNode = {TEXT("PakFile"), CachePath, /*bIsLocal*/ true};
	OutNode.UsageStats.Add(TEXT(""), UsageStats);
}

bool FPakFileCacheStore::LegacyDebugOptions(FBackendDebugOptions& InOptions)
{
	DebugOptions = InOptions;
	return true;
}

void FPakFileCacheStore::Put(
	const TConstArrayView<FCachePutRequest> Requests,
	IRequestOwner& Owner,
	FOnCachePutComplete&& OnComplete)
{
	for (const FCachePutRequest& Request : Requests)
	{
		bool bOk;
		{
			const FCacheRecord& Record = Request.Record;
			TRACE_CPUPROFILER_EVENT_SCOPE(PakFileDDC_Put);
			COOK_STAT(auto Timer = UsageStats.TimePut());
			uint64 WriteSize = 0;
			bOk = PutCacheRecord(Request.Name, Record, Request.Policy, WriteSize);
			if (bOk)
			{
				UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache put complete for %s from '%s'"),
					*CachePath, *WriteToString<96>(Record.GetKey()), *Request.Name);
				if (WriteSize)
				{
					COOK_STAT(Timer.AddHit(WriteSize));
				}
			}
		}
		OnComplete(Request.MakeResponse(bOk ? EStatus::Ok : EStatus::Error));
	}
}

void FPakFileCacheStore::Get(
	const TConstArrayView<FCacheGetRequest> Requests,
	IRequestOwner& Owner,
	FOnCacheGetComplete&& OnComplete)
{
	for (const FCacheGetRequest& Request : Requests)
	{
		EStatus Status = EStatus::Error;
		FOptionalCacheRecord Record;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(PakFileDDC_Get);
			COOK_STAT(auto Timer = UsageStats.TimeGet());
			if ((Record = GetCacheRecord(Request.Name, Request.Key, Request.Policy, Status)))
			{
				UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache hit for %s from '%s'"),
					*CachePath, *WriteToString<96>(Request.Key), *Request.Name);
				COOK_STAT(Timer.AddHit(Private::GetCacheRecordCompressedSize(Record.Get())));
			}
			else
			{
				Record = FCacheRecordBuilder(Request.Key).Build();
			}
		}
		OnComplete({Request.Name, MoveTemp(Record).Get(), Request.UserData, Status});
	}
}

void FPakFileCacheStore::PutValue(
	const TConstArrayView<FCachePutValueRequest> Requests,
	IRequestOwner& Owner,
	FOnCachePutValueComplete&& OnComplete)
{
	for (const FCachePutValueRequest& Request : Requests)
	{
		bool bOk;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(PakFileDDC_PutValue);
			COOK_STAT(auto Timer = UsageStats.TimePut());
			uint64 WriteSize = 0;
			bOk = PutCacheValue(Request.Name, Request.Key, Request.Value, Request.Policy, WriteSize);
			if (bOk)
			{
				UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache put complete for %s from '%s'"),
					*CachePath, *WriteToString<96>(Request.Key), *Request.Name);
				if (WriteSize)
				{
					COOK_STAT(Timer.AddHit(WriteSize));
				}
			}
		}
		OnComplete(Request.MakeResponse(bOk ? EStatus::Ok : EStatus::Error));
	}
}

void FPakFileCacheStore::GetValue(
	const TConstArrayView<FCacheGetValueRequest> Requests,
	IRequestOwner& Owner,
	FOnCacheGetValueComplete&& OnComplete)
{
	for (const FCacheGetValueRequest& Request : Requests)
	{
		bool bOk;
		FValue Value;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(PakFileDDC_GetValue);
			COOK_STAT(auto Timer = UsageStats.TimeGet());
			bOk = GetCacheValue(Request.Name, Request.Key, Request.Policy, Value);
			if (bOk)
			{
				UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache hit for %s from '%s'"),
					*CachePath, *WriteToString<96>(Request.Key), *Request.Name);
				COOK_STAT(Timer.AddHit(Value.GetData().GetCompressedSize()));
			}
		}
		OnComplete({Request.Name, Request.Key, Value, Request.UserData, bOk ? EStatus::Ok : EStatus::Error});
	}
}

void FPakFileCacheStore::GetChunks(
	const TConstArrayView<FCacheGetChunkRequest> Requests,
	IRequestOwner& Owner,
	FOnCacheGetChunkComplete&& OnComplete)
{
	TArray<FCacheGetChunkRequest, TInlineAllocator<16>> SortedRequests(Requests);
	SortedRequests.StableSort(TChunkLess());

	bool bHasValue = false;
	FValue Value;
	FValueId ValueId;
	FCacheKey ValueKey;
	TUniquePtr<FArchive> ValueAr;
	FCompressedBufferReader ValueReader;
	FOptionalCacheRecord Record;
	for (const FCacheGetChunkRequest& Request : SortedRequests)
	{
		EStatus Status = EStatus::Error;
		FSharedBuffer Buffer;
		uint64 RawSize = 0;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(PakFileDDC_Get);
			const bool bExistsOnly = EnumHasAnyFlags(Request.Policy, ECachePolicy::SkipData);
			COOK_STAT(auto Timer = bExistsOnly ? UsageStats.TimeProbablyExists() : UsageStats.TimeGet());
			if (!(bHasValue && ValueKey == Request.Key && ValueId == Request.Id) || ValueReader.HasSource() < !bExistsOnly)
			{
				ValueReader.ResetSource();
				ValueAr.Reset();
				ValueKey = {};
				ValueId.Reset();
				Value.Reset();
				bHasValue = false;
				if (Request.Id.IsValid())
				{
					if (!(Record && Record.Get().GetKey() == Request.Key))
					{
						FCacheRecordPolicyBuilder PolicyBuilder(ECachePolicy::None);
						PolicyBuilder.AddValuePolicy(Request.Id, Request.Policy);
						Record.Reset();
						Record = GetCacheRecordOnly(Request.Name, Request.Key, PolicyBuilder.Build());
					}
					if (Record)
					{
						if (const FValueWithId& ValueWithId = Record.Get().GetValue(Request.Id))
						{
							bHasValue = true;
							Value = ValueWithId;
							ValueId = Request.Id;
							ValueKey = Request.Key;
							GetCacheContent(Request.Name, Request.Key, ValueId, Value, Request.Policy, ValueReader, ValueAr);
						}
					}
				}
				else
				{
					ValueKey = Request.Key;
					bHasValue = GetCacheValueOnly(Request.Name, Request.Key, Request.Policy, Value);
					if (bHasValue)
					{
						GetCacheContent(Request.Name, Request.Key, Request.Id, Value, Request.Policy, ValueReader, ValueAr);
					}
				}
			}
			if (bHasValue)
			{
				const uint64 RawOffset = FMath::Min(Value.GetRawSize(), Request.RawOffset);
				RawSize = FMath::Min(Value.GetRawSize() - RawOffset, Request.RawSize);
				UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache hit for %s from '%s'"),
					*CachePath, *WriteToString<96>(Request.Key, '/', Request.Id), *Request.Name);
				COOK_STAT(Timer.AddHit(!bExistsOnly ? RawSize : 0));
				if (!bExistsOnly)
				{
					Buffer = ValueReader.Decompress(RawOffset, RawSize);
				}
				Status = bExistsOnly || Buffer.GetSize() == RawSize ? EStatus::Ok : EStatus::Error;
			}
		}
		OnComplete({Request.Name, Request.Key, Request.Id, Request.RawOffset,
			RawSize, Value.GetRawHash(), MoveTemp(Buffer), Request.UserData, Status});
	}
}

bool FPakFileCacheStore::PutCacheRecord(
	const FStringView Name,
	const FCacheRecord& Record,
	const FCacheRecordPolicy& Policy,
	uint64& OutWriteSize)
{
	if (!bWriting || bClosed)
	{
		return false;
	}

	const FCacheKey& Key = Record.GetKey();
	const ECachePolicy RecordPolicy = Policy.GetRecordPolicy();

	// Skip the request if storing to the cache is disabled.
	if (!EnumHasAnyFlags(Policy.GetRecordPolicy(), ECachePolicy::StoreLocal))
	{
		UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("%s: Skipped put of %s from '%.*s' due to cache policy"),
			*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
		return false;
	}

	if (DebugOptions.ShouldSimulatePutMiss(Key))
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Simulated miss for put of %s from '%.*s'"),
			*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
		return false;
	}

	TStringBuilder<256> Path;
	FPathViews::Append(Path, TEXT("Buckets"), Key);

	// Check if there is an existing record package.
	bool bReplaceExisting = !EnumHasAnyFlags(RecordPolicy, ECachePolicy::QueryLocal);
	bool bSaveRecord = bReplaceExisting;
	if (!bReplaceExisting)
	{
		bSaveRecord |= !FileExists(Path);
	}

	// Serialize the record to a package and remove attachments that will be stored externally.
	FCbPackage Package = Record.Save();
	TArray<FCompressedBuffer, TInlineAllocator<8>> ExternalContent;
	Algo::Transform(Package.GetAttachments(), ExternalContent, &FCbAttachment::AsCompressedBinary);
	Package = FCbPackage(Package.GetObject());

	// Save the external content to storage.
	for (FCompressedBuffer& Content : ExternalContent)
	{
		uint64 WriteSize = 0;
		if (!PutCacheContent(Name, Content, WriteSize))
		{
			return false;
		}
		OutWriteSize += WriteSize;
	}

	// Save the record package to storage.
	const auto WriteRecord = [&](FArchive& Ar) { Package.Save(Ar); OutWriteSize += uint64(Ar.TotalSize()); };
	if (bSaveRecord && !SaveFile(Path, Name, WriteRecord))
	{
		return false;
	}

	return true;
}

FOptionalCacheRecord FPakFileCacheStore::GetCacheRecordOnly(
	const FStringView Name,
	const FCacheKey& Key,
	const FCacheRecordPolicy& Policy)
{
	if (bClosed)
	{
		UE_LOG(LogDerivedDataCache, VeryVerbose,
			TEXT("%s: Skipped get of %s from '%.*s' because this cache store is not available"),
			*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
		return FOptionalCacheRecord();
	}

	// Skip the request if querying the cache is disabled.
	if (!EnumHasAnyFlags(Policy.GetRecordPolicy(), ECachePolicy::QueryLocal))
	{
		UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("%s: Skipped get of %s from '%.*s' due to cache policy"),
			*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
		return FOptionalCacheRecord();
	}

	if (DebugOptions.ShouldSimulateGetMiss(Key))
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Simulated miss for get of %s from '%.*s'"),
			*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
		return FOptionalCacheRecord();
	}

	TStringBuilder<256> Path;
	FPathViews::Append(Path, TEXT("Buckets"), Key);

	// Request the record from storage.
	FSharedBuffer Buffer = LoadFile(Path, Name);
	if (Buffer.IsNull())
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache miss with missing record for %s from '%.*s'"),
			*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
		return FOptionalCacheRecord();
	}

	// Validate that the record can be read as a compact binary package without crashing.
	if (ValidateCompactBinaryPackage(Buffer, ECbValidateMode::Default | ECbValidateMode::Package) != ECbValidateError::None)
	{
		UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Cache miss with invalid package for %s from '%.*s'"),
			*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
		return FOptionalCacheRecord();
	}

	// Load the record from the package.
	FOptionalCacheRecord Record;
	{
		FCbPackage Package;
		if (FCbFieldIterator It = FCbFieldIterator::MakeRange(Buffer); !Package.TryLoad(It))
		{
			UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Cache miss with package load failure for %s from '%.*s'"),
				*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
			return FOptionalCacheRecord();
		}
		Record = FCacheRecord::Load(Package);
		if (Record.IsNull())
		{
			UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Cache miss with record load failure for %s from '%.*s'"),
				*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
			return FOptionalCacheRecord();
		}
	}

	return Record.Get();
}

FOptionalCacheRecord FPakFileCacheStore::GetCacheRecord(
	const FStringView Name,
	const FCacheKey& Key,
	const FCacheRecordPolicy& Policy,
	EStatus& OutStatus)
{
	FOptionalCacheRecord Record = GetCacheRecordOnly(Name, Key, Policy);
	if (Record.IsNull())
	{
		OutStatus = EStatus::Error;
		return Record;
	}

	OutStatus = EStatus::Ok;

	FCacheRecordBuilder RecordBuilder(Key);

	const ECachePolicy RecordPolicy = Policy.GetRecordPolicy();
	if (!EnumHasAnyFlags(RecordPolicy, ECachePolicy::SkipMeta))
	{
		RecordBuilder.SetMeta(FCbObject(Record.Get().GetMeta()));
	}

	for (const FValueWithId& Value : Record.Get().GetValues())
	{
		const FValueId& Id = Value.GetId();
		const ECachePolicy ValuePolicy = Policy.GetValuePolicy(Id);
		FValue Content;
		if (GetCacheContent(Name, Key, Id, Value, ValuePolicy, Content))
		{
			RecordBuilder.AddValue(Id, MoveTemp(Content));
		}
		else if (EnumHasAnyFlags(RecordPolicy, ECachePolicy::PartialRecord))
		{
			OutStatus = EStatus::Error;
			RecordBuilder.AddValue(Value);
		}
		else
		{
			OutStatus = EStatus::Error;
			return FOptionalCacheRecord();
		}
	}

	return RecordBuilder.Build();
}

bool FPakFileCacheStore::PutCacheValue(
	const FStringView Name,
	const FCacheKey& Key,
	const FValue& Value,
	const ECachePolicy Policy,
	uint64& OutWriteSize)
{
	if (!bWriting || bClosed)
	{
		return false;
	}

	// Skip the request if storing to the cache is disabled.
	if (!EnumHasAnyFlags(Policy, ECachePolicy::StoreLocal))
	{
		UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("%s: Skipped put of %s from '%.*s' due to cache policy"),
			*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
		return false;
	}

	if (DebugOptions.ShouldSimulatePutMiss(Key))
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Simulated miss for put of %s from '%.*s'"),
			*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
		return false;
	}

	// Check if there is an existing value package.
	bool bValueExists = false;
	TStringBuilder<256> Path;
	FPathViews::Append(Path, TEXT("Buckets"), Key);
	const bool bReplaceExisting = !EnumHasAnyFlags(Policy, ECachePolicy::QueryLocal);
	if (!bReplaceExisting)
	{
		bValueExists = FileExists(Path);
	}

	// Save the value to a package and save the data to external content.
	if (!bValueExists)
	{
		FCbWriter Writer;
		Writer.BeginObject();
		Writer.AddBinaryAttachment("RawHash", Value.GetRawHash());
		Writer.AddInteger("RawSize", Value.GetRawSize());
		Writer.EndObject();

		FCbPackage Package(Writer.Save().AsObject());
		if (!Value.HasData())
		{
			// Verify that the content exists in storage.
			if (!GetCacheContentExists(Key, Value.GetRawHash()))
			{
				UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Failed due to missing data for put of %s from '%.*s'"),
					*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
				return false;
			}
		}
		else
		{
			// Save the external content to storage.
			uint64 WriteSize = 0;
			if (!PutCacheContent(Name, Value.GetData(), WriteSize))
			{
				return false;
			}
			OutWriteSize += WriteSize;
		}

		// Save the value package to storage.
		const auto WritePackage = [&](FArchive& Ar) { Package.Save(Ar); OutWriteSize += uint64(Ar.TotalSize()); };
		if (!SaveFile(Path, Name, WritePackage))
		{
			return false;
		}
	}

	return true;
}

bool FPakFileCacheStore::GetCacheValueOnly(
	const FStringView Name,
	const FCacheKey& Key,
	const ECachePolicy Policy,
	FValue& OutValue)
{
	if (bClosed)
	{
		UE_LOG(LogDerivedDataCache, VeryVerbose,
			TEXT("%s: Skipped get of %s from '%.*s' because this cache store is not available"),
			*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
		return false;
	}

	// Skip the request if querying the cache is disabled.
	if (!EnumHasAnyFlags(Policy, ECachePolicy::QueryLocal))
	{
		UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("%s: Skipped get of %s from '%.*s' due to cache policy"),
			*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
		return false;
	}

	if (DebugOptions.ShouldSimulateGetMiss(Key))
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Simulated miss for get of %s from '%.*s'"),
			*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
		return false;
	}

	TStringBuilder<256> Path;
	FPathViews::Append(Path, TEXT("Buckets"), Key);

	// Request the value package from storage.
	FSharedBuffer Buffer = LoadFile(Path, Name);
	if (Buffer.IsNull())
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache miss with missing value for %s from '%.*s'"),
			*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
		return false;
	}

	if (ValidateCompactBinary(Buffer, ECbValidateMode::Default | ECbValidateMode::Package) != ECbValidateError::None)
	{
		UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Cache miss with invalid package for %s from '%.*s'"),
			*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
		return false;
	}

	FCbPackage Package;
	if (FCbFieldIterator It = FCbFieldIterator::MakeRange(Buffer); !Package.TryLoad(It))
	{
		UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Cache miss with package load failure for %s from '%.*s'"),
			*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
		return false;
	}

	const FCbObjectView Object = Package.GetObject();
	const FIoHash RawHash = Object["RawHash"].AsHash();
	const uint64 RawSize = Object["RawSize"].AsUInt64(MAX_uint64);
	if (RawHash.IsZero() || RawSize == MAX_uint64)
	{
		UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Cache miss with invalid value for %s from '%.*s'"),
			*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
		return false;
	}

	OutValue = FValue(RawHash, RawSize);

	return true;
}

bool FPakFileCacheStore::GetCacheValue(
	const FStringView Name,
	const FCacheKey& Key,
	const ECachePolicy Policy,
	FValue& OutValue)
{
	return GetCacheValueOnly(Name, Key, Policy, OutValue) && GetCacheContent(Name, Key, {}, OutValue, Policy, OutValue);
}

bool FPakFileCacheStore::PutCacheContent(const FStringView Name, const FCompressedBuffer& Content, uint64& OutWriteSize)
{
	const FIoHash& RawHash = Content.GetRawHash();
	TStringBuilder<256> Path;
	FPathViews::Append(Path, TEXT("Content"), RawHash);
	if (!FileExists(Path))
	{
		if (!SaveFile(Path, Name, [&Content, &OutWriteSize](FArchive& Ar) { Content.Save(Ar); OutWriteSize += uint64(Ar.TotalSize()); }))
		{
			return false;
		}
	}
	return true;
}

bool FPakFileCacheStore::GetCacheContentExists(const FCacheKey& Key, const FIoHash& RawHash)
{
	TStringBuilder<256> Path;
	FPathViews::Append(Path, TEXT("Buckets"), Key);
	return FileExists(Path);
}

bool FPakFileCacheStore::GetCacheContent(
	const FStringView Name,
	const FCacheKey& Key,
	const FValueId& Id,
	const FValue& Value,
	const ECachePolicy Policy,
	FValue& OutValue)
{
	if (!EnumHasAnyFlags(Policy, ECachePolicy::Query))
	{
		OutValue = Value.RemoveData();
		return true;
	}

	if (Value.HasData())
	{
		OutValue = EnumHasAnyFlags(Policy, ECachePolicy::SkipData) ? Value.RemoveData() : Value;
		return true;
	}

	const FIoHash& RawHash = Value.GetRawHash();

	TStringBuilder<256> Path;
	FPathViews::Append(Path, TEXT("Content"), RawHash);
	if (EnumHasAnyFlags(Policy, ECachePolicy::SkipData))
	{
		if (FileExists(Path))
		{
			OutValue = Value;
			return true;
		}
	}
	else
	{
		if (FSharedBuffer CompressedData = LoadFile(Path, Name))
		{
			if (FCompressedBuffer CompressedBuffer = FCompressedBuffer::FromCompressed(MoveTemp(CompressedData));
				CompressedBuffer && CompressedBuffer.GetRawHash() == RawHash)
			{
				OutValue = FValue(MoveTemp(CompressedBuffer));
				return true;
			}
			UE_LOG(LogDerivedDataCache, Display,
				TEXT("%s: Cache miss with corrupted value %s with hash %s for %s from '%.*s'"),
				*CachePath, *WriteToString<16>(Id), *WriteToString<48>(RawHash),
				*WriteToString<96>(Key), Name.Len(), Name.GetData());
			return false;
		}
	}

	UE_LOG(LogDerivedDataCache, Verbose,
		TEXT("%s: Cache miss with missing value %s with hash %s for %s from '%.*s'"),
		*CachePath, *WriteToString<16>(Id), *WriteToString<48>(RawHash), *WriteToString<96>(Key),
		Name.Len(), Name.GetData());
	return false;
}

void FPakFileCacheStore::GetCacheContent(
	const FStringView Name,
	const FCacheKey& Key,
	const FValueId& Id,
	const FValue& Value,
	const ECachePolicy Policy,
	FCompressedBufferReader& Reader,
	TUniquePtr<FArchive>& OutArchive)
{
	if (!EnumHasAnyFlags(Policy, ECachePolicy::Query))
	{
		return;
	}

	if (Value.HasData())
	{
		if (!EnumHasAnyFlags(Policy, ECachePolicy::SkipData))
		{
			Reader.SetSource(Value.GetData());
		}
		OutArchive.Reset();
		return;
	}

	const FIoHash& RawHash = Value.GetRawHash();

	TStringBuilder<256> Path;
	FPathViews::Append(Path, TEXT("Content"), RawHash);
	if (EnumHasAllFlags(Policy, ECachePolicy::SkipData))
	{
		if (FileExists(Path))
		{
			return;
		}
	}
	else
	{
		OutArchive = OpenFile(Path, Name);
		if (OutArchive)
		{
			Reader.SetSource(*OutArchive);
			if (Reader.GetRawHash() == RawHash)
			{
				return;
			}
			UE_LOG(LogDerivedDataCache, Display,
				TEXT("%s: Cache miss with corrupted value %s with hash %s for %s from '%.*s'"),
				*CachePath, *WriteToString<16>(Id), *WriteToString<48>(RawHash),
				*WriteToString<96>(Key), Name.Len(), Name.GetData());
			Reader.ResetSource();
			OutArchive.Reset();
			return;
		}
	}

	UE_LOG(LogDerivedDataCache, Verbose,
		TEXT("%s: Cache miss with missing value %s with hash %s for %s from '%.*s'"),
		*CachePath, *WriteToString<16>(Id), *WriteToString<48>(RawHash), *WriteToString<96>(Key),
		Name.Len(), Name.GetData());
}

class FCrcBuilder
{
public:
	inline void Update(const void* Data, uint64 Size)
	{
		while (Size > 0)
		{
			const int32 CrcSize = int32(FMath::Min<uint64>(Size, MAX_int32));
			Crc = FCrc::MemCrc_DEPRECATED(Data, CrcSize, Crc);
			Size -= CrcSize;
		}
	}

	inline uint32 Finalize()
	{
		return Crc;
	}

private:
	uint32 Crc = 0;
};

class FPakWriterArchive final : public FArchive
{
public:
	inline FPakWriterArchive(IFileHandle& InHandle, FStringView InPath)
		: Handle(InHandle)
		, Path(InPath)
	{
		SetIsSaving(true);
		SetIsPersistent(true);
	}

	inline FString GetArchiveName() const final { return FString(Path); }
	inline int64 TotalSize() final { return Handle.Size(); }
	inline int64 Tell() final { unimplemented(); return 0; }
	inline void Seek(int64 InPos) final { unimplemented(); }
	inline void Flush() final { unimplemented(); }
	inline bool Close() final { unimplemented(); return false; }

	inline void Serialize(void* V, int64 Length) final
	{
		if (!Handle.Write(static_cast<uint8*>(V), Length))
		{
			SetError();
		}
	}

private:
	IFileHandle& Handle;
	FStringView Path;
};

class FPakReaderArchive final : public FArchive
{
public:
	inline FPakReaderArchive(IFileHandle& InHandle, FStringView InPath)
		: Handle(InHandle)
		, Path(InPath)
	{
		SetIsLoading(true);
		SetIsPersistent(true);
	}

	inline FString GetArchiveName() const final { return FString(Path); }
	inline int64 TotalSize() final { return Handle.Size(); }
	inline int64 Tell() final { unimplemented(); return 0; }
	inline void Seek(int64 InPos) final { unimplemented(); }
	inline void Flush() final { unimplemented(); }
	inline bool Close() final { unimplemented(); return false; }

	inline void Serialize(void* V, int64 Length) final
	{
		if (!Handle.Read(static_cast<uint8*>(V), Length))
		{
			SetError();
		}
	}

private:
	IFileHandle& Handle;
	FStringView Path;
};

bool FPakFileCacheStore::SaveFile(
	const FStringView Path,
	const FStringView DebugName,
	TFunctionRef<void (FArchive&)> WriteFunction)
{
	FWriteScopeLock ScopeLock(SynchronizationObject);
	check(FileHandle);
	if (const int64 Offset = FileHandle->Tell(); Offset >= 0)
	{
		FPakWriterArchive Ar(*FileHandle, CachePath);
		THashingArchiveProxy<FCrcBuilder> HashAr(Ar);
		WriteFunction(HashAr);
		if (const int64 EndOffset = FileHandle->Tell(); EndOffset >= Offset && !Ar.IsError())
		{
			FCacheValue& Item = CacheItems.Emplace(Path, FCacheValue(Offset, EndOffset - Offset, HashAr.GetHash()));
			UE_LOG(LogDerivedDataCache, VeryVerbose,
				TEXT("%s: File %.*s from '%.*s' written with offset %" INT64_FMT ", size %" INT64_FMT", CRC 0x%08x."),
				*CachePath, Path.Len(), Path.GetData(), DebugName.Len(), DebugName.GetData(), Item.Offset, Item.Size, Item.Crc);
			return true;
		}
	}
	return false;
}

FSharedBuffer FPakFileCacheStore::LoadFile(const FStringView Path, const FStringView DebugName)
{
	FWriteScopeLock ScopeLock(SynchronizationObject);
	if (const FCacheValue* Item = CacheItems.FindByHash(GetTypeHash(Path), Path))
	{
		check(FileHandle);
		ON_SCOPE_EXIT
		{
			if (bWriting)
			{
				FileHandle->SeekFromEnd();
			}
		};
		check(Item->Size);
		if (FileHandle->Seek(Item->Offset))
		{
			FPakReaderArchive Ar(*FileHandle, CachePath);
			THashingArchiveProxy<FCrcBuilder> HashAr(Ar);
			FUniqueBuffer MutableBuffer = FUniqueBuffer::Alloc(uint64(Item->Size));
			HashAr.Serialize(MutableBuffer.GetData(), Item->Size);
			if (Ar.IsError())
			{
				UE_LOG(LogDerivedDataCache, Display,
					TEXT("%s: File %.*s from '%.*s' failed to read %" INT64_FMT " bytes."),
					*CachePath, Path.Len(), Path.GetData(), DebugName.Len(), DebugName.GetData(), Item->Size);
			}
			else if (const uint32 TestCrc = HashAr.GetHash(); TestCrc != Item->Crc)
			{
				UE_LOG(LogDerivedDataCache, Display,
					TEXT("%s: File %.*s from '%.*s' is corrupted and has CRC 0x%08x when 0x%08x is expected."),
					*CachePath, Path.Len(), Path.GetData(), DebugName.Len(), DebugName.GetData(), TestCrc, Item->Crc);
			}
			else
			{
				return MutableBuffer.MoveToShared();
			}
		}
	}
	return FSharedBuffer();
}

TUniquePtr<FArchive> FPakFileCacheStore::OpenFile(FStringBuilderBase& Path, const FStringView DebugName)
{
	FReadScopeLock ScopeLock(SynchronizationObject);
	const FStringView PathView(Path);
	if (const FCacheValue* Item = CacheItems.FindByHash(GetTypeHash(PathView), PathView))
	{
		check(Item->Size);
		if (TUniquePtr<FArchive> Ar{IFileManager::Get().CreateFileReader(*CachePath, FILEREAD_Silent | FILEREAD_AllowWrite)})
		{
			Ar->Seek(Item->Offset);
			return Ar;
		}
	}
	return nullptr;
}

bool FPakFileCacheStore::FileExists(const FStringView Path)
{
	FReadScopeLock ScopeLock(SynchronizationObject);
	const uint32 PathHash = GetTypeHash(Path);
	return CacheItems.ContainsByHash(PathHash, Path);
}

class FCompressedPakFileCacheStore final : public FPakFileCacheStore
{
public:
	FCompressedPakFileCacheStore(const TCHAR* InFilename, bool bInWriting);

	void Put(
		TConstArrayView<FCachePutRequest> Requests,
		IRequestOwner& Owner,
		FOnCachePutComplete&& OnComplete) final;

	void PutValue(
		TConstArrayView<FCachePutValueRequest> Requests,
		IRequestOwner& Owner,
		FOnCachePutValueComplete&& OnComplete) final;

private:
	static const ECompressedBufferCompressor RequiredCompressor = ECompressedBufferCompressor::Kraken;
	static const ECompressedBufferCompressionLevel MinRequiredCompressionLevel = ECompressedBufferCompressionLevel::Optimal2;

	static FValue Compress(const FValue& Value);
};

FCompressedPakFileCacheStore::FCompressedPakFileCacheStore(const TCHAR* InFilename, bool bInWriting)
	: FPakFileCacheStore(InFilename, bInWriting)
{
}

void FCompressedPakFileCacheStore::Put(
	const TConstArrayView<FCachePutRequest> Requests,
	IRequestOwner& Owner,
	FOnCachePutComplete&& OnComplete)
{
	Owner.LaunchTask(TEXT("PakFileDDC_Put"),
		[this, &Owner, Requests = TArray<FCachePutRequest, TInlineAllocator<1>>(Requests), OnComplete = MoveTemp(OnComplete)]() mutable
		{
			for (FCachePutRequest& Request : Requests)
			{
				FCacheRecordBuilder Builder(Request.Record.GetKey());
				Builder.SetMeta(CopyTemp(Request.Record.GetMeta()));
				for (const FValueWithId& Value : Request.Record.GetValues())
				{
					Builder.AddValue(Value.GetId(), Compress(Value));
				}
				Request.Record = Builder.Build();
			}
			Private::LaunchTaskInCacheThreadPool(Owner,
				[this, &Owner, Requests = MoveTemp(Requests), OnComplete = MoveTemp(OnComplete)]() mutable
				{
					FPakFileCacheStore::Put(Requests, Owner, MoveTemp(OnComplete));
				});
		});
}

void FCompressedPakFileCacheStore::PutValue(
	const TConstArrayView<FCachePutValueRequest> Requests,
	IRequestOwner& Owner,
	FOnCachePutValueComplete&& OnComplete)
{
	Owner.LaunchTask(TEXT("PakFileDDC_PutValue"),
		[this, &Owner, Requests = TArray<FCachePutValueRequest, TInlineAllocator<1>>(Requests), OnComplete = MoveTemp(OnComplete)]() mutable
		{
			for (FCachePutValueRequest& Request : Requests)
			{
				Request.Value = Compress(Request.Value);
			}
			Private::LaunchTaskInCacheThreadPool(Owner,
				[this, &Owner, Requests = MoveTemp(Requests), OnComplete = MoveTemp(OnComplete)]() mutable
				{
					FPakFileCacheStore::PutValue(Requests, Owner, MoveTemp(OnComplete));
				});
		});
}

FValue FCompressedPakFileCacheStore::Compress(const FValue& Value)
{
	uint64 BlockSize = 0;
	ECompressedBufferCompressor Compressor;
	ECompressedBufferCompressionLevel CompressionLevel;
	if (!Value.HasData() ||
		(Value.GetData().TryGetCompressParameters(Compressor, CompressionLevel, BlockSize) &&
			Compressor == RequiredCompressor &&
			CompressionLevel >= MinRequiredCompressionLevel))
	{
		return Value;
	}
	TRACE_CPUPROFILER_EVENT_SCOPE(PakFileDDC_Compress);
	const FCompositeBuffer Data = Value.GetData().DecompressToComposite();
	return FValue(FCompressedBuffer::Compress(Data, RequiredCompressor, MinRequiredCompressionLevel, BlockSize));
}

IPakFileCacheStore* CreatePakFileCacheStore(const TCHAR* Filename, bool bWriting, bool bCompressed)
{
	return bCompressed ? new FCompressedPakFileCacheStore(Filename, bWriting) : new FPakFileCacheStore(Filename, bWriting);
}

} // UE::DerivedData
