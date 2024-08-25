// Copyright Epic Games, Inc. All Rights Reserved.

#include "Algo/Accumulate.h"
#include "Algo/AllOf.h"
#include "Algo/Compare.h"
#include "Algo/Find.h"
#include "Algo/StableSort.h"
#include "Algo/Transform.h"
#include "Async/Async.h"
#include "Async/ManualResetEvent.h"
#include "Async/Mutex.h"
#include "Async/UniqueLock.h"
#include "Containers/StaticBitArray.h"
#include "DerivedDataBackendInterface.h"
#include "DerivedDataCacheInterface.h"
#include "DerivedDataCacheMaintainer.h"
#include "DerivedDataCachePrivate.h"
#include "DerivedDataCacheRecord.h"
#include "DerivedDataCacheUsageStats.h"
#include "DerivedDataChunk.h"
#include "DerivedDataRequestOwner.h"
#include "DerivedDataValue.h"
#include "Features/IModularFeatures.h"
#include "HAL/Event.h"
#include "HAL/FileManager.h"
#include "HAL/Thread.h"
#include "Hash/xxhash.h"
#include "HashingArchiveProxy.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreMisc.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/StringBuilder.h"
#include "ProfilingDebugging/CookStats.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinaryPackage.h"
#include "Serialization/CompactBinaryValidation.h"
#include "Serialization/CompactBinaryWriter.h"
#include "Tasks/Task.h"
#include "Templates/Greater.h"

#include <atomic>

namespace UE::DerivedData
{

TRACE_DECLARE_ATOMIC_INT_COUNTER(FileSystemDDC_Get, TEXT("FileSystemDDC Get"));
TRACE_DECLARE_ATOMIC_INT_COUNTER(FileSystemDDC_GetHit, TEXT("FileSystemDDC Get Hit"));
TRACE_DECLARE_ATOMIC_INT_COUNTER(FileSystemDDC_Put, TEXT("FileSystemDDC Put"));
TRACE_DECLARE_ATOMIC_INT_COUNTER(FileSystemDDC_PutHit, TEXT("FileSystemDDC Put Hit"));
TRACE_DECLARE_ATOMIC_INT_COUNTER(FileSystemDDC_BytesRead, TEXT("FileSystemDDC Bytes Read"));
TRACE_DECLARE_ATOMIC_INT_COUNTER(FileSystemDDC_BytesWritten, TEXT("FileSystemDDC Bytes Written"));

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if PLATFORM_LINUX
// PATH_MAX on Linux is 4096 (getconf PATH_MAX /, also see limits.h), so this value can be larger (note that it is still arbitrary).
// This should not affect sharing the cache between platforms as the absolute paths will be different anyway.
static constexpr int32 GMaxCacheRootLen = 3119;
#else
static constexpr int32 GMaxCacheRootLen = 119;
#endif // PLATFORM_LINUX

static constexpr int32 GMaxCacheKeyLen =
	FCacheBucket::MaxNameLen + // Name
	sizeof(FIoHash) * 2 +      // Hash
	4 +                        // Separators /<Name>/<Hash01>/<Hash23>/<Hash4-40>
	4;                         // Extension (.udd)

static const TCHAR* GBucketsDirectoryName = TEXT("Buckets");
static const TCHAR* GContentDirectoryName = TEXT("Content");

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void BuildPathForCachePackage(const FCacheKey& CacheKey, FStringBuilderBase& Path)
{
	const FIoHash::ByteArray& Bytes = CacheKey.Hash.GetBytes();
	Path.Appendf(TEXT("%s/%hs/%02x/%02x/"), GBucketsDirectoryName, CacheKey.Bucket.ToCString(), Bytes[0], Bytes[1]);
	UE::String::BytesToHexLower(MakeArrayView(Bytes).RightChop(2), Path);
	Path << TEXTVIEW(".udd");
}

void BuildPathForCacheContent(const FIoHash& RawHash, FStringBuilderBase& Path)
{
	const FIoHash::ByteArray& Bytes = RawHash.GetBytes();
	Path.Appendf(TEXT("%s/%02x/%02x/"), GContentDirectoryName, Bytes[0], Bytes[1]);
	UE::String::BytesToHexLower(MakeArrayView(Bytes).RightChop(2), Path);
	Path << TEXTVIEW(".udd");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static uint64 RandFromGuid()
{
	const FGuid Guid = FGuid::NewGuid();
	return FXxHash64::HashBuffer(&Guid, sizeof(FGuid)).Hash;
}

/** A LCG in which the modulus is a power of two where the exponent is the bit width of T. */
template <typename T, T Modulus = 0>
class TLinearCongruentialGenerator
{
	static_assert(!TIsSigned<T>::Value);
	static_assert((Modulus & (Modulus - 1)) == 0, "Modulus must be a power of two.");

public:
	constexpr inline TLinearCongruentialGenerator(T InMultiplier, T InIncrement)
		: Multiplier(InMultiplier)
		, Increment(InIncrement)
	{
	}

	constexpr inline T GetNext(T& Value)
	{
		Value = (Value * Multiplier + Increment) & (Modulus - 1);
		return Value;
	}

private:
	const T Multiplier;
	const T Increment;
};

class FRandomStream
{
public:
	inline explicit FRandomStream(const uint32 Seed)
		: Random(1103515245, 12345) // From ANSI C
		, Value(Seed)
	{
	}

	/** Returns a random value in [Min, Max). */
	inline uint32 GetRandRange(const uint32 Min, const uint32 Max)
	{
		return Min + uint32((uint64(Max - Min) * Random.GetNext(Value)) >> 32);
	}

private:
	TLinearCongruentialGenerator<uint32> Random;
	uint32 Value;
};

template <uint32 Modulus, uint32 Count = Modulus>
class TRandomOrder
{
	static_assert((Modulus & (Modulus - 1)) == 0 && Modulus > 16, "Modulus must be a power of two greater than 16.");
	static_assert(Count > 0 && Count <= Modulus, "Count must be in the range (0, Modulus].");

public:
	inline explicit TRandomOrder(FRandomStream& Stream)
		: Random(Stream.GetRandRange(0, Modulus / 16) * 8 + 5, 12345)
		, First(Stream.GetRandRange(0, Count))
		, Value(First)
	{
	}

	inline uint32 GetFirst() const
	{
		return First;
	}

	inline uint32 GetNext()
	{
		if constexpr (Count < Modulus)
		{
			for (;;)
			{
				if (const uint32 Next = Random.GetNext(Value); Next < Count)
				{
					return Next;
				}
			}
		}
		else
		{
			return Random.GetNext(Value);
		}
	}

private:
	TLinearCongruentialGenerator<uint32, Modulus> Random;
	uint32 First;
	uint32 Value;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct FFileSystemCacheStoreMaintainerParams
{
	/** Files older than this will be deleted. */
	FTimespan MaxFileAge = FTimespan::FromDays(15.0);
	/** Limits the number of paths scanned in one second. */
	uint32 MaxScanRate = MAX_uint32;
	/** Limits the number of directories scanned in each cache bucket or content root. */
	uint32 MaxDirectoryScanCount = MAX_uint32;
	/** Minimum duration between the start of consecutive scans. Use MaxValue to scan only once. */
	FTimespan ScanFrequency = FTimespan::FromHours(1.0);
	/** Time to wait after initialization before maintenance begins. */
	FTimespan TimeToWaitAfterInit = FTimespan::FromMinutes(1.0);
};

class FFileSystemCacheStoreMaintainer final : public ICacheStoreMaintainer
{
public:
	FFileSystemCacheStoreMaintainer(const FFileSystemCacheStoreMaintainerParams& Params, FStringView CachePath);
	~FFileSystemCacheStoreMaintainer();

	bool IsIdle() const final { return bIdle; }
	void WaitForIdle() const { IdleEvent.Wait(); }
	void BoostPriority() final;

private:
	void Tick();
	void Loop();
	void Scan();

	void CreateContentRoot();
	void CreateBucketRoots();
	void ScanHashRoot(uint32 RootIndex);
	TStaticBitArray<256> ScanHashDirectory(FStringBuilderBase& BasePath);

	TStaticBitArray<10> ScanLegacyDirectory(FStringBuilderBase& BasePath);
	void CreateLegacyRoot();
	void ScanLegacyRoot();

	void ResetRoots();

	void ProcessDirectory(const TCHAR* Path);
	void ProcessFile(const TCHAR* Path, const FFileStatData& Stat, bool& bOutDeletedFile);
	void ProcessWait();

	void DeleteDirectory(const TCHAR* Path);

private:
	struct FRoot;
	struct FLegacyRoot;

	FFileSystemCacheStoreMaintainerParams Params;
	/** Path to the root of the cache store. */
	FString CachePath;
	/** True when there is no active maintenance scan. */
	bool bIdle = false;
	/** True when maintenance is expected to exit as soon as possible. */
	bool bExit = false;
	/** True when maintenance is expected to exit at the end of the scan. */
	bool bExitAfterScan = false;
	/** Ignore the scan rate for one maintenance scan. */
	bool bIgnoreScanRate = false;

	uint32 FileCount = 0;
	uint32 FolderCount = 0;
	uint32 ProcessCount = 0;
	uint32 DeleteFileCount = 0;
	uint32 DeleteFolderCount = 0;
	uint64 DeleteSize = 0;
	uint64 ScannedSize = 0;

	double BatchStartTime = 0.0;

	IFileManager& FileManager = IFileManager::Get();

	mutable FManualResetEvent IdleEvent;
	FEventRef WaitEvent;
	FThread Thread;

	TArray<TUniquePtr<FRoot>> Roots;
	TUniquePtr<FLegacyRoot> LegacyRoot;
	FRandomStream Random{uint32(RandFromGuid())};

	static constexpr double MaxScanFrequencyDays = 365.0;
};

struct FFileSystemCacheStoreMaintainer::FRoot
{
	inline FRoot(const FStringView RootPath, FRandomStream& Stream)
		: Order(Stream)
	{
		Path.Append(RootPath);
	}

	TStringBuilder<256> Path;
	TRandomOrder<256 * 256> Order;
	TStaticBitArray<256> ScannedLevel0;
	TStaticBitArray<256> ExistsLevel0;
	TStaticBitArray<256> ExistsLevel1[256];
	uint32 DirectoryScanCount = 0;
	bool bScannedRoot = false;
};

struct FFileSystemCacheStoreMaintainer::FLegacyRoot
{
	inline explicit FLegacyRoot(FRandomStream& Stream)
		: Order(Stream)
	{
	}

	TRandomOrder<1024, 1000> Order;
	TStaticBitArray<10> ScannedLevel0;
	TStaticBitArray<10> ScannedLevel1[10];
	TStaticBitArray<10> ExistsLevel0;
	TStaticBitArray<10> ExistsLevel1[10];
	TStaticBitArray<10> ExistsLevel2[10][10];
	uint32 DirectoryScanCount = 0;
};

FFileSystemCacheStoreMaintainer::FFileSystemCacheStoreMaintainer(
	const FFileSystemCacheStoreMaintainerParams& InParams,
	const FStringView InCachePath)
	: Params(InParams)
	, CachePath(InCachePath)
	, bExitAfterScan(Params.ScanFrequency.GetTotalDays() > MaxScanFrequencyDays)
	, WaitEvent(EEventMode::AutoReset)
	, Thread(
		TEXT("FileSystemCacheStoreMaintainer"),
		[this] { Loop(); },
		[this] { Tick(); },
		/*StackSize*/ 32 * 1024,
		TPri_BelowNormal)
{
	IModularFeatures::Get().RegisterModularFeature(FeatureName, this);
}

FFileSystemCacheStoreMaintainer::~FFileSystemCacheStoreMaintainer()
{
	bExit = true;
	IModularFeatures::Get().UnregisterModularFeature(FeatureName, this);
	WaitEvent->Trigger();
	Thread.Join();
}

void FFileSystemCacheStoreMaintainer::BoostPriority()
{
	bIgnoreScanRate = true;
	WaitEvent->Trigger();
}

void FFileSystemCacheStoreMaintainer::Tick()
{
	// Scan once and exit if the priority has been boosted.
	if (bIgnoreScanRate)
	{
		bExitAfterScan = true;
		Loop();
	}
	bIdle = true;
	IdleEvent.Notify();
}

void FFileSystemCacheStoreMaintainer::Loop()
{
	WaitEvent->Wait(Params.TimeToWaitAfterInit, /*bIgnoreThreadIdleStats*/ true);

	while (!bExit)
	{
		const FDateTime ScanStart = FDateTime::Now();
		FileCount = 0;
		FolderCount = 0;
		DeleteFileCount = 0;
		DeleteFolderCount = 0;
		DeleteSize = 0;
		ScannedSize = 0;
		IdleEvent.Reset();
		bIdle = false;
		Scan();
		bIdle = true;
		IdleEvent.Notify();
		bIgnoreScanRate = false;
		const FDateTime ScanEnd = FDateTime::Now();

		UE_LOG(LogDerivedDataCache, Log,
			TEXT("%s: Maintenance finished in %s and deleted %u files with total size %" UINT64_FMT " MiB "
				 "and %u empty folders. Scanned %u files in %u folders with total size %" UINT64_FMT " MiB."),
			*CachePath, *(ScanEnd - ScanStart).ToString(), DeleteFileCount, DeleteSize / 1024 / 1024,
			DeleteFolderCount, FileCount, FolderCount, ScannedSize / 1024 / 1024);

		if (bExit || bExitAfterScan)
		{
			break;
		}

		const FDateTime ScanTime = ScanStart + Params.ScanFrequency;
		UE_CLOG(ScanEnd < ScanTime, LogDerivedDataCache, Verbose,
			TEXT("%s: Maintenance is paused until the next scan at %s."), *CachePath, *ScanTime.ToString());
		for (FDateTime Now = ScanEnd; !bExit && Now < ScanTime; Now = FDateTime::Now())
		{
			WaitEvent->Wait(ScanTime - Now, /*bIgnoreThreadIdleStats*/ true);
		}
	}

	bIdle = true;
	IdleEvent.Notify();
}

void FFileSystemCacheStoreMaintainer::Scan()
{
	CreateContentRoot();
	CreateBucketRoots();
	CreateLegacyRoot();

	while (!bExit)
	{
		const uint32 RootCount = uint32(Roots.Num());
		const uint32 TotalRootCount = uint32(RootCount + LegacyRoot.IsValid());
		if (TotalRootCount == 0)
		{
			break;
		}
		if (const uint32 RootIndex = Random.GetRandRange(0, TotalRootCount); RootIndex < RootCount)
		{
			ScanHashRoot(RootIndex);
		}
		else
		{
			ScanLegacyRoot();
		}
	}

	ResetRoots();
}

void FFileSystemCacheStoreMaintainer::CreateContentRoot()
{
	TStringBuilder<256> ContentPath;
	FPathViews::Append(ContentPath, CachePath, GContentDirectoryName);
	if (FileManager.DirectoryExists(*ContentPath))
	{
		Roots.Add(MakeUnique<FRoot>(ContentPath, Random));
	}
}

void FFileSystemCacheStoreMaintainer::CreateBucketRoots()
{
	TStringBuilder<256> BucketsPath;
	FPathViews::Append(BucketsPath, CachePath, GBucketsDirectoryName);
	if (FileManager.DirectoryExists(*BucketsPath))
	{
		++FolderCount;
		const int32 StartRootCount = Roots.Num();
		FileManager.IterateDirectoryStat(*BucketsPath, [this](const TCHAR* Path, const FFileStatData& Stat) -> bool
		{
			if (Stat.bIsDirectory)
			{
				Roots.Add(MakeUnique<FRoot>(Path, Random));
			}
			return !bExit;
		});
		if (StartRootCount == Roots.Num())
		{
			DeleteDirectory(*BucketsPath);
		}
	}
}

void FFileSystemCacheStoreMaintainer::ScanHashRoot(const uint32 RootIndex)
{
	FRoot& Root = *Roots[int32(RootIndex)];
	const uint32 DirectoryIndex = Root.Order.GetNext();
	const uint32 IndexLevel0 = DirectoryIndex / 256;
	const uint32 IndexLevel1 = DirectoryIndex % 256;

	bool bScanned = false;
	ON_SCOPE_EXIT
	{
		if ((DirectoryIndex == Root.Order.GetFirst()) ||
			(bScanned && ++Root.DirectoryScanCount >= Params.MaxDirectoryScanCount))
		{
			UE_LOG(LogDerivedDataCache, VeryVerbose,
				TEXT("%s: Maintenance finished scanning %s."), *CachePath, *Root.Path);
			Roots.RemoveAt(int32(RootIndex));
		}
	};

	if (!Root.bScannedRoot)
	{
		Root.ExistsLevel0 = ScanHashDirectory(Root.Path);
		Root.bScannedRoot = true;
	}

	if (!Root.ExistsLevel0[IndexLevel0])
	{
		return;
	}
	if (!Root.ScannedLevel0[IndexLevel0])
	{
		TStringBuilder<256> Path;
		Path.Appendf(TEXT("%s/%02x"), *Root.Path, IndexLevel0);
		Root.ExistsLevel1[IndexLevel0] = ScanHashDirectory(Path);
		Root.ScannedLevel0[IndexLevel0] = true;
	}

	if (!Root.ExistsLevel1[IndexLevel0][IndexLevel1])
	{
		return;
	}

	TStringBuilder<256> Path;
	Path.Appendf(TEXT("%s/%02x/%02x"), *Root.Path, IndexLevel0, IndexLevel1);
	ProcessDirectory(*Path);

	bScanned = true;
}

TStaticBitArray<256> FFileSystemCacheStoreMaintainer::ScanHashDirectory(FStringBuilderBase& BasePath)
{
	++FolderCount;
	TStaticBitArray<256> Exists;
	FileManager.IterateDirectoryStat(*BasePath, [this, &Exists](const TCHAR* Path, const FFileStatData& Stat) -> bool
	{
		FStringView View = FPathViews::GetCleanFilename(Path);
		if (Stat.bIsDirectory && View.Len() == 2 && Algo::AllOf(View, FChar::IsHexDigit))
		{
			uint8 Byte;
			if (String::HexToBytes(View, &Byte) == 1)
			{
				Exists[Byte] = true;
			}
		}
		return !bExit;
	});
	if (Exists.FindFirstSetBit() == INDEX_NONE)
	{
		DeleteDirectory(*BasePath);
	}
	return Exists;
}

TStaticBitArray<10> FFileSystemCacheStoreMaintainer::ScanLegacyDirectory(FStringBuilderBase& BasePath)
{
	++FolderCount;
	TStaticBitArray<10> Exists;
	FileManager.IterateDirectoryStat(*BasePath, [this, &Exists](const TCHAR* Path, const FFileStatData& Stat) -> bool
	{
		FStringView View = FPathViews::GetCleanFilename(Path);
		if (Stat.bIsDirectory && View.Len() == 1 && Algo::AllOf(View, FChar::IsDigit))
		{
			Exists[FChar::ConvertCharDigitToInt(View[0])] = true;
		}
		return !bExit;
	});
	if (Exists.FindFirstSetBit() == INDEX_NONE && BasePath.Len() > CachePath.Len())
	{
		DeleteDirectory(*BasePath);
	}
	return Exists;
}

void FFileSystemCacheStoreMaintainer::CreateLegacyRoot()
{
	TStringBuilder<256> Path;
	FPathViews::Append(Path, CachePath);
	TStaticBitArray<10> Exists = ScanLegacyDirectory(Path);
	if (Exists.FindFirstSetBit() != INDEX_NONE)
	{
		LegacyRoot = MakeUnique<FLegacyRoot>(Random);
		LegacyRoot->ExistsLevel0 = Exists;
	}
}

void FFileSystemCacheStoreMaintainer::ScanLegacyRoot()
{
	FLegacyRoot& Root = *LegacyRoot;
	const uint32 DirectoryIndex = Root.Order.GetNext();
	const int32 IndexLevel0 = int32(DirectoryIndex / 100) % 10;
	const int32 IndexLevel1 = int32(DirectoryIndex / 10) % 10;
	const int32 IndexLevel2 = int32(DirectoryIndex / 1) % 10;

	bool bScanned = false;
	ON_SCOPE_EXIT
	{
		if ((DirectoryIndex == Root.Order.GetFirst()) ||
			(bScanned && ++Root.DirectoryScanCount >= Params.MaxDirectoryScanCount))
		{
			LegacyRoot.Reset();
		}
	};

	if (!Root.ExistsLevel0[IndexLevel0])
	{
		return;
	}
	if (!Root.ScannedLevel0[IndexLevel0])
	{
		TStringBuilder<256> Path;
		FPathViews::Append(Path, CachePath, IndexLevel0);
		Root.ExistsLevel1[IndexLevel0] = ScanLegacyDirectory(Path);
		Root.ScannedLevel0[IndexLevel0] = true;
	}

	if (!Root.ExistsLevel1[IndexLevel0][IndexLevel1])
	{
		return;
	}
	if (!Root.ScannedLevel1[IndexLevel0][IndexLevel1])
	{
		TStringBuilder<256> Path;
		FPathViews::Append(Path, CachePath, IndexLevel0, IndexLevel1);
		Root.ExistsLevel2[IndexLevel0][IndexLevel1] = ScanLegacyDirectory(Path);
		Root.ScannedLevel1[IndexLevel0][IndexLevel1] = true;
	}

	if (!Root.ExistsLevel2[IndexLevel0][IndexLevel1][IndexLevel2])
	{
		return;
	}

	TStringBuilder<256> Path;
	FPathViews::Append(Path, CachePath, IndexLevel0, IndexLevel1, IndexLevel2);
	ProcessDirectory(*Path);

	bScanned = true;
}

void FFileSystemCacheStoreMaintainer::ResetRoots()
{
	Roots.Empty();
	LegacyRoot.Reset();
}

void FFileSystemCacheStoreMaintainer::ProcessDirectory(const TCHAR* const Path)
{
	++FolderCount;

	bool bTryDelete = true;

	FileManager.IterateDirectoryStat(Path, [this, &bTryDelete](const TCHAR* const Path, const FFileStatData& Stat) -> bool
	{
		bool bDeletedFile = false;
		ProcessFile(Path, Stat, bDeletedFile);
		bTryDelete &= bDeletedFile;
		return !bExit;
	});

	if (bTryDelete)
	{
		DeleteDirectory(Path);
	}

	ProcessWait();
}

void FFileSystemCacheStoreMaintainer::ProcessFile(const TCHAR* const Path, const FFileStatData& Stat, bool& bOutDeletedFile)
{
	bOutDeletedFile = false;

	if (Stat.bIsDirectory)
	{
		return;
	}

	++FileCount;
	ScannedSize += Stat.FileSize > 0 ? uint64(Stat.FileSize) : 0;

	const FDateTime Now = FDateTime::UtcNow();
	if (Stat.ModificationTime + Params.MaxFileAge < Now && Stat.AccessTime + Params.MaxFileAge < Now)
	{
		++DeleteFileCount;
		DeleteSize += Stat.FileSize > 0 ? uint64(Stat.FileSize) : 0;
		if (FileManager.Delete(Path, /*bRequireExists*/ false, /*bEvenReadOnly*/ false, /*bQuiet*/ true))
		{
			bOutDeletedFile = true;
			UE_LOG(LogDerivedDataCache, VeryVerbose,
				TEXT("%s: Maintenance deleted file %s that was last modified at %s."),
				*CachePath, Path, *Stat.ModificationTime.ToIso8601());
		}
		else
		{
			UE_LOG(LogDerivedDataCache, Verbose,
				TEXT("%s: Maintenance failed to delete file %s that was last modified at %s."),
				*CachePath, Path, *Stat.ModificationTime.ToIso8601());
		}
	}

	ProcessWait();
}

void FFileSystemCacheStoreMaintainer::ProcessWait()
{
	if (!bExit && !bIgnoreScanRate && Params.MaxScanRate && ++ProcessCount % Params.MaxScanRate == 0)
	{
		const double BatchEndTime = FPlatformTime::Seconds();
		if (const double BatchWaitTime = 1.0 - (BatchEndTime - BatchStartTime); BatchWaitTime > 0.0)
		{
			WaitEvent->Wait(FTimespan::FromSeconds(BatchWaitTime), /*bIgnoreThreadIdleStats*/ true);
			BatchStartTime = FPlatformTime::Seconds();
		}
		else
		{
			BatchStartTime = BatchEndTime;
		}
	}
}

void FFileSystemCacheStoreMaintainer::DeleteDirectory(const TCHAR* Path)
{
	if (FileManager.DeleteDirectory(Path))
	{
		++DeleteFolderCount;
		UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("%s: Maintenance deleted empty directory %s."), *CachePath, Path);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FAccessLogWriter
{
public:
	FAccessLogWriter(const TCHAR* FileName, const FString& CachePath);

	void Append(const FIoHash& RawHash, FStringView Path);
	void Append(const FCacheKey& CacheKey, FStringView Path);

private:
	void AppendPath(FStringView Path);

	TUniquePtr<FArchive> Archive;
	FString BasePath;
	FCriticalSection CriticalSection;
	TSet<FIoHash> ContentKeys;
	TSet<FCacheKey> RecordKeys;
};

FAccessLogWriter::FAccessLogWriter(const TCHAR* const FileName, const FString& CachePath)
	: Archive(IFileManager::Get().CreateFileWriter(FileName, FILEWRITE_AllowRead))
	, BasePath(CachePath / TEXT(""))
{
}

void FAccessLogWriter::Append(const FIoHash& RawHash, const FStringView Path)
{
	FScopeLock Lock(&CriticalSection);

	bool bIsAlreadyInSet = false;
	ContentKeys.FindOrAdd(RawHash, &bIsAlreadyInSet);
	if (!bIsAlreadyInSet)
	{
		AppendPath(Path);
	}
}

void FAccessLogWriter::Append(const FCacheKey& CacheKey, const FStringView Path)
{
	FScopeLock Lock(&CriticalSection);

	bool bIsAlreadyInSet = false;
	RecordKeys.FindOrAdd(CacheKey, &bIsAlreadyInSet);
	if (!bIsAlreadyInSet)
	{
		AppendPath(Path);
	}
}

void FAccessLogWriter::AppendPath(const FStringView Path)
{
	if (Path.StartsWith(BasePath))
	{
		const FTCHARToUTF8 PathUtf8(Path.RightChop(BasePath.Len()));
		Archive->Serialize(const_cast<ANSICHAR*>(PathUtf8.Get()), PathUtf8.Length());
		Archive->Serialize(const_cast<ANSICHAR*>(LINE_TERMINATOR_ANSI), sizeof(LINE_TERMINATOR_ANSI) - 1);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct FFileSystemCacheStoreParams
{
	/** Name of the cache, e.g., Local, Shared. */
	FString CacheName;

	/** Root path under which cache files are stored. */
	FString CachePath;

	/** Path to the optional access log that records every accessed file. */
	FString AccessLogPath;

	/** Optional name of a file containing redirection information so that if the file is present and valid, another cache store can be used in place of this FileSystemCacheStore. */
	FString RedirectionFileName;

	/** Optional name of a key within the RedirectionFileName containing redirection information so that if the file+key is present and valid, another cache store can be used in place of this FileSystemCacheStore. */
	FString RedirectionKeyName;

	/** Maximum total size of compressed data stored within a record package with multiple attachments. */
	uint64 MaxRecordSizeKB = 256;
	/** Maximum total size of compressed data stored within a value package, or a record package with one attachment. */
	uint64 MaxValueSizeKB = 1024;

	/** Maintenance will delete files older than this. */
	double MaxFileAgeInDays = 15.0;
	/** Maintenance will wait this long after initialization before starting. */
	double MaintenanceDelayInSeconds = 60.0;
	/** Limits the number of paths scanned by maintenance in one second. */
	uint32 MaxScanRate = MAX_uint32;
	/** Limits the number of directories scanned by maintenance in each cache bucket or content root. */
	uint32 MaxDirectoryScanCount = MAX_uint32;

	/** Latency lower than this is considered local. */
	float ConsiderFastAtMs = 10;
	/** Latency lower than this is considered ok, and anything higher is considered slow. */
	float ConsiderSlowAtMs = 50;
	/** Latency higher than this will deactivate the cache store for performance reasons. */
	float DeactivateAtMs = -1.0f;

	/** If true, skip the speed test to measure latency on startup; */
	bool bSkipSpeedTest = !WITH_EDITOR;
	/** If true, display a retry prompt in attended sessions when the cache store is missing. */
	bool bPromptIfMissing = false;
	/** If true, files older than the max file age will be deleted during maintenance. */
	bool bDeleteUnused = false;
	/** If true, do not write or read any files in this cache store, only delete expired files. */
	bool bDeleteOnly = false;
	/** If true, do not write any files in this cache store. */
	bool bReadOnly = false;
	/** If true, block on maintenance of this cache store on startup. */
	bool bClean = false;
	/** If true, delete everything in this cache store on startup. */
	bool bFlush = false;
	/** If true, always update file timestamps on access, even when read-only. */
	bool bTouch = false;

	void Parse(const TCHAR* Name, const TCHAR* Config);
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FFileSystemCacheStore final : public ILegacyCacheStore
{
public:
	static ILegacyCacheStore* TryCreate(
		const FFileSystemCacheStoreParams& Params,
		ICacheStoreOwner& Owner,
		ICacheStoreGraph* Graph,
		FString& OutPath,
		bool& bOutRedirected);

private:
	FFileSystemCacheStore(
		ICacheStoreOwner& Owner,
		const FFileSystemCacheStoreParams& Params,
		const FDerivedDataCacheSpeedStats& SpeedStats);
	~FFileSystemCacheStore();

	static bool RunSpeedTest(
		FString CachePath,
		bool bReadOnly,
		bool bSeekTimeOnly,
		double& OutSeekTimeMS,
		double& OutReadSpeedMBs,
		double& OutWriteSpeedMBs,
		std::atomic<uint32>* NumLatencyTestsCompleted,
		std::atomic<bool>* AbandonRequest);

	static ILegacyCacheStore* TryRedirection(
		const FFileSystemCacheStoreParams& Params,
		ICacheStoreOwner& Owner,
		ICacheStoreGraph* Graph);
	
	// ICacheStore Interface

	void Put(
		TConstArrayView<FCachePutRequest> Requests,
		IRequestOwner& Owner,
		FOnCachePutComplete&& OnComplete) final;
	void Get(
		TConstArrayView<FCacheGetRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetComplete&& OnComplete) final;
	void PutValue(
		TConstArrayView<FCachePutValueRequest> Requests,
		IRequestOwner& Owner,
		FOnCachePutValueComplete&& OnComplete) final;
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
	[[nodiscard]] bool PutCacheRecord(FStringView Name, const FCacheRecord& Record, const FCacheRecordPolicy& Policy, FRequestStats& Stats);

	[[nodiscard]] FOptionalCacheRecord GetCacheRecordOnly(
		FStringView Name,
		const FCacheKey& Key,
		const FCacheRecordPolicy& Policy,
		FRequestStats& Stats);
	[[nodiscard]] FOptionalCacheRecord GetCacheRecord(
		FStringView Name,
		const FCacheKey& Key,
		const FCacheRecordPolicy& Policy,
		EStatus& OutStatus,
		FRequestStats& Stats);

	[[nodiscard]] bool PutCacheValue(FStringView Name, const FCacheKey& Key, const FValue& Value, ECachePolicy Policy, FRequestStats& Stats);

	[[nodiscard]] bool GetCacheValueOnly(FStringView Name, const FCacheKey& Key, ECachePolicy Policy, FValue& OutValue, FRequestStats& Stats);
	[[nodiscard]] bool GetCacheValue(FStringView Name, const FCacheKey& Key, ECachePolicy Policy, FValue& OutValue, FRequestStats& Stats);

	[[nodiscard]] bool PutCacheContent(FStringView Name, const FCompressedBuffer& Content, FRequestStats& Stats) const;

	[[nodiscard]] bool GetCacheContentExists(const FCacheKey& Key, const FIoHash& RawHash, FRequestStats& Stats) const;
	[[nodiscard]] bool GetCacheContent(
		FStringView Name,
		const FCacheKey& Key,
		const FValueId& Id,
		const FValue& Value,
		ECachePolicy Policy,
		FValue& OutValue,
		FRequestStats& Stats) const;
	void GetCacheContent(
		FStringView Name,
		const FCacheKey& Key,
		const FValueId& Id,
		const FValue& Value,
		ECachePolicy Policy,
		FCompressedBufferReader& Reader,
		TUniquePtr<FArchive>& OutArchive,
		FRequestStats& Stats) const;

	void BuildCachePackagePath(const FCacheKey& CacheKey, FStringBuilderBase& Path) const;
	void BuildCacheContentPath(const FIoHash& RawHash, FStringBuilderBase& Path) const;

	[[nodiscard]] bool SaveFileWithHash(FStringBuilderBase& Path, FStringView DebugName, FRequestStats& Stats, TFunctionRef<void (FArchive&)> WriteFunction, bool bReplaceExisting = false) const;
	[[nodiscard]] bool LoadFileWithHash(FStringBuilderBase& Path, FStringView DebugName, FRequestStats& Stats, TFunctionRef<void (FArchive&)> ReadFunction) const;
	[[nodiscard]] bool SaveFile(FStringBuilderBase& Path, FStringView DebugName, FRequestStats& Stats, TFunctionRef<void (FArchive&)> WriteFunction, bool bReplaceExisting = false) const;
	[[nodiscard]] bool LoadFile(FStringBuilderBase& Path, FStringView DebugName, FRequestStats& Stats, TFunctionRef<void (FArchive&)> ReadFunction) const;
	[[nodiscard]] TUniquePtr<FArchive> OpenFileWrite(FStringBuilderBase& Path, FStringView DebugName, FRequestStats& Stats) const;
	[[nodiscard]] TUniquePtr<FArchive> OpenFileRead(FStringBuilderBase& Path, FStringView DebugName, FRequestStats& Stats) const;

	[[nodiscard]] bool FileExists(FStringBuilderBase& Path, FRequestStats& Stats) const;

	[[nodiscard]] bool IsDeactivatedForPerformance();
	void UpdateStatus();

	static bool RunInitialSpeedTest(const FFileSystemCacheStoreParams& Params, FDerivedDataCacheSpeedStats& OutSpeedStats);

private:
	FString CachePath;
	ICacheStoreOwner& StoreOwner;
	ICacheStoreStats* StoreStats = nullptr;

	/** Speed class of this cache. */
	EBackendSpeedClass SpeedClass = EBackendSpeedClass::Unknown;
	/** If true, do not attempt to write to this cache. */
	bool		bReadOnly;
	/** If true, always update file timestamps on access. */
	bool		bTouch;

	/** Age of file when it should be deleted from DDC cache. */
	double		MaxFileAgeInDays = 15.0;

	/** Maximum total size of compressed data stored within a record package with multiple attachments. */
	uint64		MaxRecordSizeKB = 256;
	/** Maximum total size of compressed data stored within a value package, or a record package with one attachment. */
	uint64		MaxValueSizeKB = 1024;

	/** Access log to write to */
	TUniquePtr<FAccessLogWriter> AccessLogWriter;

	/** Debug Options */
	FBackendDebugOptions DebugOptions;

	/** Speed stats */
	FDerivedDataCacheSpeedStats SpeedStats;

	TUniquePtr<FFileSystemCacheStoreMaintainer> Maintainer;

	TUniquePtr<FFileSystemCacheStoreMaintainerParams> DeactivationDeferredMaintainerParams;

	inline static FMutex ActiveStoresMutex;
	inline static TArray<FFileSystemCacheStore*> ActiveStores;

	enum class EPerformanceReEvaluationResult
	{
		Invalid = 0,
		PerformanceActivate,
		PerformanceDeactivate
	};
	FRWLock PerformanceReEvaluationTaskLock;
	Tasks::TTask<std::atomic<EPerformanceReEvaluationResult>> PerformanceReEvaluationTask;
	std::atomic<int64> LastPerformanceEvaluationTicks;
	std::atomic<bool> bDeactivatedForPerformance = false;
	bool bDeactivationDeferredClean = false;
	float DeactivateAtMs;
};

ILegacyCacheStore* FFileSystemCacheStore::TryCreate(
	const FFileSystemCacheStoreParams& Params,
	ICacheStoreOwner& Owner,
	ICacheStoreGraph* Graph,
	FString& OutPath,
	bool& bOutRedirected)
{
	// If we find a platform that has more stringent limits, this needs to be rethought.
	checkf(GMaxCacheRootLen + GMaxCacheKeyLen <= FPlatformMisc::GetMaxPathLength(),
		TEXT("Not enough room left for cache keys in max path."));

	if (Params.CachePath.IsEmpty())
	{
		UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Disabled because no path is configured."), *Params.CacheName);
		return nullptr;
	}
	else if (Params.CachePath == TEXTVIEW("None"))
	{
		UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Disabled because the path is configured to 'None'"), *Params.CacheName);
		return nullptr;
	}

	const auto HasSameCachePath = [&Params](const FFileSystemCacheStore* ActiveStore)
	{
		return FPaths::IsSamePath(Params.CachePath, ActiveStore->CachePath);
	};

	bool bRetryOnFailure;
	do
	{
		bRetryOnFailure = false;

		if (TUniqueLock Lock(ActiveStoresMutex); Algo::FindByPredicate(ActiveStores, HasSameCachePath))
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Process has an existing cache store at path %s, and the duplicate is being ignored."), *Params.CacheName, *Params.CachePath);
			return nullptr;
		}

		// Skip creation of the cache store if the shared cache directory does not exist.
		bool bShared = Params.CacheName == TEXTVIEW("Shared");
		if (!bShared || IFileManager::Get().DirectoryExists(*Params.CachePath))
		{
			if (ILegacyCacheStore* RedirectedStore = TryRedirection(Params, Owner, Graph))
			{
				bOutRedirected = true;
				return RedirectedStore;
			}

			FDerivedDataCacheSpeedStats LocalSpeedStats;
			LocalSpeedStats.ReadSpeedMBs = 999;
			LocalSpeedStats.WriteSpeedMBs = 999;
			LocalSpeedStats.LatencyMS = 0;
			if (Params.bSkipSpeedTest || RunInitialSpeedTest(Params, LocalSpeedStats))
			{
				UE_CLOG(Params.bSkipSpeedTest, LogDerivedDataCache, Log, TEXT("%s: Skipping speed test at path %s and assuming local performance."), *Params.CacheName, *Params.CachePath);

				FFileSystemCacheStore* CacheStore = new FFileSystemCacheStore(Owner, Params, LocalSpeedStats);
				{
					TUniqueLock Lock(ActiveStoresMutex);
					ActiveStores.Add(CacheStore);
				}
				OutPath = CacheStore->CachePath;
				return CacheStore;
			}

			UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: No read or write access to %s, or the speed test was abandoned due to slow progress."), *Params.CacheName, *Params.CachePath);
		}

		// Give the user a chance to retry in case they need to take steps like connecting a network drive.
		if (Params.bPromptIfMissing && !FApp::IsUnattended() && !IS_PROGRAM)
		{
			TStringBuilder<512> Message(InPlace, Params.CacheName, TEXTVIEW(" cache store path "), Params.CachePath,
				TEXTVIEW(" is not available and this cache store will be disabled.\n\nRetry connection to "), Params.CachePath, TEXTVIEW("?"));
			bRetryOnFailure = FPlatformMisc::MessageBoxExt(EAppMsgType::YesNo, *Message, TEXT("Failed to access Derived Data Cache")) == EAppReturnType::Yes;
		}
	}
	while (bRetryOnFailure); //-V654

	UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Path %s is not available and this cache store will be disabled."), *Params.CacheName, *Params.CachePath);
	return nullptr;
}

FFileSystemCacheStore::FFileSystemCacheStore(
	ICacheStoreOwner& Owner,
	const FFileSystemCacheStoreParams& Params,
	const FDerivedDataCacheSpeedStats& InSpeedStats)
	: CachePath(Params.CachePath)
	, StoreOwner(Owner)
	, bReadOnly(Params.bReadOnly)
	, bTouch(Params.bTouch)
	, MaxFileAgeInDays(Params.MaxFileAgeInDays)
	, SpeedStats(InSpeedStats)
	, DeactivateAtMs(Params.DeactivateAtMs)
{
	LastPerformanceEvaluationTicks.store(FDateTime::UtcNow().GetTicks(), std::memory_order_relaxed);

	bool bReadTestPassed = SpeedStats.ReadSpeedMBs > 0.0;
	bool bWriteTestPassed = SpeedStats.WriteSpeedMBs > 0.0;

	// if we failed writes mark this as read only
	bReadOnly = bReadOnly || !bWriteTestPassed;

	const bool bLocalDeactivatedForPerformance = (Params.DeactivateAtMs > 0.f) && (SpeedStats.LatencyMS >= Params.DeactivateAtMs);
	bDeactivatedForPerformance.store(bLocalDeactivatedForPerformance, std::memory_order_relaxed);

	// classify and report on these times
	if (SpeedStats.LatencyMS < 1)
	{
		SpeedClass = EBackendSpeedClass::Local;
	}
	else if (SpeedStats.LatencyMS <= Params.ConsiderFastAtMs)
	{
		SpeedClass = EBackendSpeedClass::Fast;
	}
	else if (SpeedStats.LatencyMS >= Params.ConsiderSlowAtMs)
	{
		SpeedClass = EBackendSpeedClass::Slow;
	}
	else
	{
		SpeedClass = EBackendSpeedClass::Ok;
	}

	UE_LOG(LogDerivedDataCache, Display,
		TEXT("%s: Performance: Latency=%.02fms. RandomReadSpeed=%.02fMBs, RandomWriteSpeed=%.02fMBs. Assigned SpeedClass '%s'"),
		*CachePath, SpeedStats.LatencyMS, SpeedStats.ReadSpeedMBs, SpeedStats.WriteSpeedMBs, LexToString(SpeedClass));

	if (bLocalDeactivatedForPerformance)
	{
		if (GIsBuildMachine)
		{
			UE_LOG(LogDerivedDataCache, Display,
				TEXT("%s: Performance does not meet minimum criteria. "
					"It will be deactivated until performance measurements improve. "
					"If this is consistent, consider disabling this cache store through "
					"environment variables or other configuration."),
				*CachePath);
		}
		else
		{
			UE_LOG(LogDerivedDataCache, Warning,
				TEXT("%s: Performance does not meet minimum criteria. "
					"It will be deactivated until performance measurements improve. "
					"If this is consistent, consider disabling this cache store through "
					"environment variables or other configuration."),
				*CachePath);
		}
	}

	if (SpeedClass <= EBackendSpeedClass::Slow && !bReadOnly)
	{
		if (GIsBuildMachine)
		{
			UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Access is slow. "
				"'Touch' will be disabled and queries/writes will be limited."), *CachePath);
		}
		else
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Access is slow. "
				"'Touch' will be disabled and queries/writes will be limited."), *CachePath);
		}
		bTouch = false;
		//bReadOnly = true;
	}

	if (FString(FCommandLine::Get()).Contains(TEXT("Run=DerivedDataCache")))
	{
		bTouch = true; // we always touch files when running the DDC commandlet
	}

	if (bTouch)
	{
		UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Files will be touched when accessed."), *CachePath);
	}

	// Flush the cache if requested.
	if (!bReadOnly && Params.bFlush)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FileSystemDDC_Flush);
		IFileManager::Get().DeleteDirectory(*(CachePath / TEXT("")), /*bRequireExists*/ false, /*bTree*/ true);
	}

	if (Params.bClean && bLocalDeactivatedForPerformance)
	{
		bDeactivationDeferredClean = true;
	}

	if (Params.bClean || Params.bDeleteUnused)
	{
		FFileSystemCacheStoreMaintainerParams* MaintainerParams;
		FFileSystemCacheStoreMaintainerParams LocalMaintainerParams;
		if (bLocalDeactivatedForPerformance)
		{
			DeactivationDeferredMaintainerParams = MakeUnique<FFileSystemCacheStoreMaintainerParams>();
			MaintainerParams = DeactivationDeferredMaintainerParams.Get();
		}
		else
		{
			MaintainerParams = &LocalMaintainerParams;
		}
		MaintainerParams->MaxFileAge = FTimespan::FromDays(MaxFileAgeInDays);
		if (Params.bDeleteUnused)
		{
			MaintainerParams->MaxScanRate = Params.MaxScanRate;
			MaintainerParams->MaxDirectoryScanCount = Params.MaxDirectoryScanCount;
		}
		else
		{
			MaintainerParams->ScanFrequency = FTimespan::MaxValue();
		}
		if (Params.bClean)
		{
			MaintainerParams->TimeToWaitAfterInit = FTimespan::Zero();
		}
		else
		{
			MaintainerParams->TimeToWaitAfterInit = FTimespan::FromSeconds(Params.MaintenanceDelayInSeconds);
		}

		if (!bLocalDeactivatedForPerformance)
		{
			Maintainer = MakeUnique<FFileSystemCacheStoreMaintainer>(*MaintainerParams, CachePath);

			if (Params.bClean)
			{
				Maintainer->BoostPriority();
				Maintainer->WaitForIdle();
			}
		}
	}

	if (!Params.AccessLogPath.IsEmpty())
	{
		AccessLogWriter.Reset(new FAccessLogWriter(*Params.AccessLogPath, CachePath));
	}

	ECacheStoreFlags Flags = ECacheStoreFlags::None;
	Flags |= Params.bDeleteOnly ? ECacheStoreFlags::None : ECacheStoreFlags::Query;
	Flags |= Params.bDeleteOnly || bReadOnly ? ECacheStoreFlags::None : ECacheStoreFlags::Store;
	Flags |= SpeedClass == EBackendSpeedClass::Local ? ECacheStoreFlags::Local : ECacheStoreFlags::Remote;

	StoreOwner.Add(this, Flags);
	if (!Params.bDeleteOnly)
	{
		StoreStats = StoreOwner.CreateStats(this, Flags, TEXTVIEW("File System"), Params.CacheName, CachePath);
	}

	UpdateStatus();

	UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Using data cache path %s: %s"), *Params.CacheName, *CachePath,
		EnumHasAnyFlags(Flags, ECacheStoreFlags::Store) ? TEXT("Writable") :
		EnumHasAnyFlags(Flags, ECacheStoreFlags::Query) ? TEXT("ReadOnly") : TEXT("DeleteOnly"));
}

FFileSystemCacheStore::~FFileSystemCacheStore()
{
	if (StoreStats)
	{
		StoreOwner.DestroyStats(StoreStats);
	}

	TUniqueLock Lock(ActiveStoresMutex);
	ActiveStores.Remove(this);
}

bool FFileSystemCacheStore::RunSpeedTest(
	FString CachePath,
	bool bReadOnly,
	bool bSeekTimeOnly,
	double& OutSeekTimeMS,
	double& OutReadSpeedMBs,
	double& OutWriteSpeedMBs,
	std::atomic<uint32>* NumLatencyTestsCompleted,
	std::atomic<bool>* AbandonRequest)
{
	SCOPED_BOOT_TIMING("RunSpeedTest");
	UE_SCOPED_ENGINE_ACTIVITY("Running IO speed test");

	//  files of increasing size. Most DDC data falls within this range so we don't want to skew by reading
	// large amounts of data. Ultimately we care most about latency anyway.
	const int FileSizes[] = { 4, 8, 16, 64, 128, 256 };
	const int NumTestFolders = 2; //(0-9)
	const int FileSizeCount = UE_ARRAY_COUNT(FileSizes);

	bool bWriteTestPassed = true;
	bool bReadTestPassed = true;
	bool bTestDataExists = true;

	double TotalSeekTime = 0;
	double TotalReadTime = 0;
	double TotalWriteTime = 0;
	int TotalDataRead = 0;
	int TotalDataWritten = 0;

	TArray<FString> Paths;
	TArray<FString> MissingFiles;

	MissingFiles.Reserve(NumTestFolders * FileSizeCount);

	const FString TestDataPath = FPaths::Combine(CachePath, TEXT("TestData"));

	// create an upfront map of paths to data size in bytes
	// create the paths we'll use. <path>/0/TestData.dat, <path>/1/TestData.dat etc. If those files don't exist we'll
	// create them which will likely give an invalid result when measuring them now but not in the future...
	TMap<FString, int> TestFileEntries;
	for (int iSize = 0; iSize < FileSizeCount; iSize++)
	{
		// make sure we dont stat/read/write to consecuting files in folders
		for (int iFolder = 0; iFolder < NumTestFolders; iFolder++)
		{
			int FileSizeKB = FileSizes[iSize];
			FString Path = FPaths::Combine(CachePath, TEXT("TestData"), *FString::FromInt(iFolder),
				*FString::Printf(TEXT("TestData_%dkb.dat"), FileSizeKB));
			TestFileEntries.Add(Path, FileSizeKB * 1024);
		}
	}

	// measure latency by checking for the presence of all these files. We'll also track which don't exist..
	const double StatStartTime = FPlatformTime::Seconds();
 	for (auto& KV : TestFileEntries)
	{
		FFileStatData StatData = IFileManager::Get().GetStatData(*KV.Key);

		if (NumLatencyTestsCompleted)
		{
			NumLatencyTestsCompleted->fetch_add(1, std::memory_order_release);
		}

		if (AbandonRequest && AbandonRequest->load(std::memory_order_relaxed))
		{
			const double TotalTestTime = FPlatformTime::Seconds() - StatStartTime;
			UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Speed tests abandoned on request after %.02f seconds."), *CachePath, TotalTestTime);
			return false;
		}

		if (!StatData.bIsValid || StatData.FileSize != KV.Value)
		{
			MissingFiles.Add(KV.Key);
		}
	}

	// save total stat time
	TotalSeekTime = (FPlatformTime::Seconds() - StatStartTime);

	// calculate seek time here
	OutSeekTimeMS = (TotalSeekTime / TestFileEntries.Num()) * 1000;

	UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Stat tests took %.02f seconds."), *CachePath, TotalSeekTime);

	if (bSeekTimeOnly)
	{
		return true;
	}

	// create any files that were missing
	if (!bReadOnly)
	{
		TArray<uint8> Data;
		for (auto& File : MissingFiles)
		{
			const int DesiredSize = TestFileEntries[File];
			Data.SetNumUninitialized(DesiredSize);

			if (!FFileHelper::SaveArrayToFile(Data, *File, &IFileManager::Get(), FILEWRITE_Silent))
			{
				// Handle the case where something else may have created the path at the same time.
				// This is less about multiple users and more about things like SCW's / UnrealPak
				// that can spin up multiple instances at once.
				if (!IFileManager::Get().FileExists(*File))
				{
					uint32 ErrorCode = FPlatformMisc::GetLastError();
					TCHAR ErrorBuffer[1024];
					FPlatformMisc::GetSystemErrorMessage(ErrorBuffer, 1024, ErrorCode);
					UE_LOG(LogDerivedDataCache, Warning,
						TEXT("%s: Failed to create %s, this cache store will be read-only. "
							"WriteError: %u (%s)"), *CachePath, *File, ErrorCode, ErrorBuffer);
					bTestDataExists = false;
					bWriteTestPassed = false;
					break;
				}
			}

			if (AbandonRequest && AbandonRequest->load(std::memory_order_relaxed))
			{
				const double TotalTestTime = FPlatformTime::Seconds() - StatStartTime;
				UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Speed tests abandoned on request after %.02f seconds."), *CachePath, TotalTestTime);
				return false;
			}
		}
	}

	// now read all sizes from random folders
	{
		const int ArraySize = UE_ARRAY_COUNT(FileSizes);
		TArray<uint8> TempData;
		TempData.Empty(FileSizes[ArraySize - 1] * 1024);

		const double ReadStartTime = FPlatformTime::Seconds();

		for (auto& KV : TestFileEntries)
		{
			const int FileSize = KV.Value;
			const FString& FilePath = KV.Key;

			if (!FFileHelper::LoadFileToArray(TempData, *FilePath, FILEREAD_Silent))
			{
				uint32 ErrorCode = FPlatformMisc::GetLastError();
				TCHAR ErrorBuffer[1024];
				FPlatformMisc::GetSystemErrorMessage(ErrorBuffer, 1024, ErrorCode);
				UE_LOG(LogDerivedDataCache, Warning,
					TEXT("%s: Failed to read from %s. ReadError: %u (%s)"),
					*CachePath, *FilePath, ErrorCode, ErrorBuffer);
				bReadTestPassed = false;
				break;
			}

			TotalDataRead += TempData.Num();
			
			if (AbandonRequest && AbandonRequest->load(std::memory_order_relaxed))
			{
				const double TotalTestTime = FPlatformTime::Seconds() - StatStartTime;
				UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Speed tests abandoned on request after %.02f seconds."), *CachePath, TotalTestTime);
				return false;
			}
		}

		TotalReadTime = FPlatformTime::Seconds() - ReadStartTime;

		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Read tests %s and took %.02f seconds."),
			*CachePath, bReadTestPassed ? TEXT("passed") : TEXT("failed"), TotalReadTime);
	}

	// do write tests if or read tests passed and our seeks were below the cut-off
	if (bReadTestPassed && !bReadOnly)
	{
		// do write tests but use a unique folder that is cleaned up afterwards
		FString CustomPath = FPaths::Combine(CachePath, TEXT("TestData"), *FGuid::NewGuid().ToString());

		const int ArraySize = UE_ARRAY_COUNT(FileSizes);
		TArray<uint8> TempData;
		TempData.Empty(FileSizes[ArraySize - 1] * 1024);

		const double WriteStartTime = FPlatformTime::Seconds();

		for (auto& KV : TestFileEntries)
		{
			const int FileSize = KV.Value;
			FString FilePath = KV.Key;

			TempData.SetNumUninitialized(FileSize);

			FilePath = FilePath.Replace(*CachePath, *CustomPath);

			if (!FFileHelper::SaveArrayToFile(TempData, *FilePath, &IFileManager::Get(), FILEWRITE_Silent))
			{
				uint32 ErrorCode = FPlatformMisc::GetLastError();
				TCHAR ErrorBuffer[1024];
				FPlatformMisc::GetSystemErrorMessage(ErrorBuffer, 1024, ErrorCode);
				UE_LOG(LogDerivedDataCache, Warning,
					TEXT("%s: Failed to write to %s. WriteError: %u (%s)"),
					*CachePath, *FilePath, ErrorCode, ErrorBuffer);
				bWriteTestPassed = false;
				break;
			}

			TotalDataWritten += TempData.Num();
			
			if (AbandonRequest && AbandonRequest->load(std::memory_order_relaxed))
			{
				const double TotalTestTime = FPlatformTime::Seconds() - StatStartTime;
				UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Speed tests abandoned on request after %.02f seconds."), *CachePath, TotalTestTime);
				return false;
			}
		}

		TotalWriteTime = FPlatformTime::Seconds() - WriteStartTime;

		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Write tests %s and took %.02f seconds."),
			*CachePath, bWriteTestPassed ? TEXT("passed") : TEXT("failed"), TotalReadTime)

			
		if (AbandonRequest && AbandonRequest->load(std::memory_order_relaxed))
		{
			const double TotalTestTime = FPlatformTime::Seconds() - StatStartTime;
			UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Speed tests abandoned on request after %.02f seconds."), *CachePath, TotalTestTime);
			return false;
		}
		
		// remove the custom path but do it async as this can be slow on remote drives
		AsyncTask(ENamedThreads::AnyThread, [CustomPath]
		{
			IFileManager::Get().DeleteDirectory(*CustomPath, false, true);
		});
	}

	const double TotalTestTime = FPlatformTime::Seconds() - StatStartTime;

	UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Speed tests took %.02f seconds."), *CachePath, TotalTestTime);

	// check latency and speed. Read values should always be valid
	OutReadSpeedMBs = (bReadTestPassed ? (TotalDataRead / TotalReadTime) : 0) / (1024 * 1024);
	OutWriteSpeedMBs = (bWriteTestPassed ? (TotalDataWritten / TotalWriteTime) : 0) / (1024 * 1024);

	return bWriteTestPassed || bReadTestPassed;
}

ILegacyCacheStore* FFileSystemCacheStore::TryRedirection(
	const FFileSystemCacheStoreParams& Params,
	ICacheStoreOwner& Owner,
	ICacheStoreGraph* Graph)
{
	if (Graph && !Params.RedirectionFileName.IsEmpty())
	{
		FString RedirectionFileName = FPaths::Combine(Params.CachePath, Params.RedirectionFileName);
		FConfigFile RedirectionFile;
		if (FConfigCacheIni::LoadLocalIniFile(RedirectionFile, *RedirectionFileName, false))
		{
			FString RedirectionKeyName = Params.RedirectionKeyName.IsEmpty() ? TEXT("Default") : Params.RedirectionKeyName;
			FString RedirectionData;
			if (RedirectionFile.GetString(TEXT("Redirect"), *RedirectionKeyName, RedirectionData))
			{
				RedirectionData.TrimStartInline();
				RedirectionData.RemoveFromStart(TEXT("("));
				RedirectionData.RemoveFromEnd(TEXT(")"));

				FString EnvName;
				FString EnvValue;
				if (FParse::Value(*RedirectionData, TEXT("SetEnvName="), EnvName) && FParse::Value(*RedirectionData, TEXT("SetEnvValue="), EnvValue))
				{
					FPlatformMisc::SetEnvironmentVar(*EnvName, *EnvValue);
				}

				FString TargetName;
				if (FParse::Value(*RedirectionData, TEXT("Target="), TargetName))
				{
					UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Found redirection to '%s'"),
						*Params.CachePath, *TargetName);
					if (ILegacyCacheStore* RedirectedStore = Graph->FindOrCreate(*TargetName))
					{
						UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Successfully redirected to '%s'"),
							*Params.CachePath, *TargetName);
						return RedirectedStore;
					}
					else
					{
						UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Failed to redirect to '%s'"),
							*Params.CachePath, *TargetName);
					}
				}
			}
		}
	}

	return nullptr;
}

void FFileSystemCacheStore::LegacyStats(FDerivedDataCacheStatsNode& OutNode)
{
	checkNoEntry();
}

bool FFileSystemCacheStore::LegacyDebugOptions(FBackendDebugOptions& InOptions)
{
	DebugOptions = InOptions;
	return true;
}

void FFileSystemCacheStore::Put(
	const TConstArrayView<FCachePutRequest> Requests,
	IRequestOwner& Owner,
	FOnCachePutComplete&& OnComplete)
{
	for (const FCachePutRequest& Request : Requests)
	{
		bool bOk;
		FRequestStats RequestStats;
		RequestStats.Name = Request.Name;
		RequestStats.Bucket = Request.Record.GetKey().Bucket;
		RequestStats.Type = ERequestType::Record;
		RequestStats.Op = ERequestOp::Put;
		{
			const FCacheRecord& Record = Request.Record;
			TRACE_CPUPROFILER_EVENT_SCOPE(FileSystemDDC_Put);
			TRACE_COUNTER_INCREMENT(FileSystemDDC_Put);
			FRequestTimer RequestTimer(RequestStats);
			uint64 WriteSize = 0;
			bOk = PutCacheRecord(Request.Name, Record, Request.Policy, RequestStats);
			if (bOk)
			{
				UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache put complete for %s from '%s'"),
					*CachePath, *WriteToString<96>(Record.GetKey()), *Request.Name);
				TRACE_COUNTER_INCREMENT(FileSystemDDC_PutHit);
				TRACE_COUNTER_ADD(FileSystemDDC_BytesRead, RequestStats.PhysicalReadSize);
				TRACE_COUNTER_ADD(FileSystemDDC_BytesWritten, RequestStats.PhysicalWriteSize);
			}
		}
		RequestStats.Status = bOk ? EStatus::Ok : EStatus::Error;
		StoreStats->AddRequest(RequestStats);
		OnComplete(Request.MakeResponse(bOk ? EStatus::Ok : EStatus::Error));
	}
}

void FFileSystemCacheStore::Get(
	const TConstArrayView<FCacheGetRequest> Requests,
	IRequestOwner& Owner,
	FOnCacheGetComplete&& OnComplete)
{
	for (const FCacheGetRequest& Request : Requests)
	{
		EStatus Status = EStatus::Error;
		FOptionalCacheRecord Record;
		FRequestStats RequestStats;
		RequestStats.Name = Request.Name;
		RequestStats.Bucket = Request.Key.Bucket;
		RequestStats.Type = ERequestType::Record;
		RequestStats.Op = ERequestOp::Get;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FileSystemDDC_Get);
			TRACE_COUNTER_INCREMENT(FileSystemDDC_Get);
			FRequestTimer RequestTimer(RequestStats);
			if ((Record = GetCacheRecord(Request.Name, Request.Key, Request.Policy, Status, RequestStats)))
			{
				UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache hit for %s from '%s'"),
					*CachePath, *WriteToString<96>(Request.Key), *Request.Name);
				TRACE_COUNTER_INCREMENT(FileSystemDDC_GetHit);
				TRACE_COUNTER_ADD(FileSystemDDC_BytesRead, RequestStats.PhysicalReadSize);
				TRACE_COUNTER_ADD(FileSystemDDC_BytesWritten, RequestStats.PhysicalWriteSize);
			}
			else
			{
				Record = FCacheRecordBuilder(Request.Key).Build();
			}
		}
		RequestStats.AddLogicalRead(Record.Get());
		RequestStats.Status = Status;
		StoreStats->AddRequest(RequestStats);
		OnComplete({Request.Name, MoveTemp(Record).Get(), Request.UserData, Status});
	}
}

void FFileSystemCacheStore::PutValue(
	const TConstArrayView<FCachePutValueRequest> Requests,
	IRequestOwner& Owner,
	FOnCachePutValueComplete&& OnComplete)
{
	for (const FCachePutValueRequest& Request : Requests)
	{
		bool bOk;
		FRequestStats RequestStats;
		RequestStats.Name = Request.Name;
		RequestStats.Bucket = Request.Key.Bucket;
		RequestStats.Type = ERequestType::Value;
		RequestStats.Op = ERequestOp::Put;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FileSystemDDC_PutValue);
			TRACE_COUNTER_INCREMENT(FileSystemDDC_Put);
			FRequestTimer RequestTimer(RequestStats);
			uint64 WriteSize = 0;
			bOk = PutCacheValue(Request.Name, Request.Key, Request.Value, Request.Policy, RequestStats);
			if (bOk)
			{
				UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache put complete for %s from '%s'"),
					*CachePath, *WriteToString<96>(Request.Key), *Request.Name);
				TRACE_COUNTER_INCREMENT(FileSystemDDC_PutHit);
				TRACE_COUNTER_ADD(FileSystemDDC_BytesRead, RequestStats.PhysicalReadSize);
				TRACE_COUNTER_ADD(FileSystemDDC_BytesWritten, RequestStats.PhysicalWriteSize);
			}
		}
		RequestStats.Status = bOk ? EStatus::Ok : EStatus::Error;
		StoreStats->AddRequest(RequestStats);
		OnComplete(Request.MakeResponse(bOk ? EStatus::Ok : EStatus::Error));
	}
}

void FFileSystemCacheStore::GetValue(
	const TConstArrayView<FCacheGetValueRequest> Requests,
	IRequestOwner& Owner,
	FOnCacheGetValueComplete&& OnComplete)
{
	for (const FCacheGetValueRequest& Request : Requests)
	{
		bool bOk;
		FValue Value;
		FRequestStats RequestStats;
		RequestStats.Name = Request.Name;
		RequestStats.Bucket = Request.Key.Bucket;
		RequestStats.Type = ERequestType::Value;
		RequestStats.Op = ERequestOp::Get;
		{
			FRequestTimer RequestTimer(RequestStats);
			TRACE_CPUPROFILER_EVENT_SCOPE(FileSystemDDC_GetValue);
			TRACE_COUNTER_INCREMENT(FileSystemDDC_Get);
			bOk = GetCacheValue(Request.Name, Request.Key, Request.Policy, Value, RequestStats);
			if (bOk)
			{
				UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache hit for %s from '%s'"),
					*CachePath, *WriteToString<96>(Request.Key), *Request.Name);
				TRACE_COUNTER_INCREMENT(FileSystemDDC_GetHit);
				TRACE_COUNTER_ADD(FileSystemDDC_BytesRead, RequestStats.PhysicalReadSize);
				TRACE_COUNTER_ADD(FileSystemDDC_BytesWritten, RequestStats.PhysicalWriteSize);
			}
		}
		RequestStats.AddLogicalRead(Value);
		RequestStats.Status = bOk ? EStatus::Ok : EStatus::Error;
		StoreStats->AddRequest(RequestStats);
		OnComplete({Request.Name, Request.Key, Value, Request.UserData, bOk ? EStatus::Ok : EStatus::Error});
	}
}

void FFileSystemCacheStore::GetChunks(
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
		FRequestStats RequestStats;
		RequestStats.Name = Request.Name;
		RequestStats.Bucket = Request.Key.Bucket;
		RequestStats.Type = Request.Id.IsNull() ? ERequestType::Value : ERequestType::Record;
		RequestStats.Op = ERequestOp::GetChunk;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FileSystemDDC_GetChunks);
			TRACE_COUNTER_INCREMENT(FileSystemDDC_Get);
			const bool bExistsOnly = EnumHasAnyFlags(Request.Policy, ECachePolicy::SkipData);
			FRequestTimer RequestTimer(RequestStats);
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
						Record = GetCacheRecordOnly(Request.Name, Request.Key, PolicyBuilder.Build(), RequestStats);
					}
					if (Record)
					{
						if (const FValueWithId& ValueWithId = Record.Get().GetValue(Request.Id))
						{
							bHasValue = true;
							Value = ValueWithId;
							ValueId = Request.Id;
							ValueKey = Request.Key;
							GetCacheContent(Request.Name, Request.Key, ValueId, Value, Request.Policy, ValueReader, ValueAr, RequestStats);
						}
					}
				}
				else
				{
					ValueKey = Request.Key;
					bHasValue = GetCacheValueOnly(Request.Name, Request.Key, Request.Policy, Value, RequestStats);
					if (bHasValue)
					{
						GetCacheContent(Request.Name, Request.Key, Request.Id, Value, Request.Policy, ValueReader, ValueAr, RequestStats);
					}
				}
			}
			if (bHasValue)
			{
				const uint64 RawOffset = FMath::Min(Value.GetRawSize(), Request.RawOffset);
				RawSize = FMath::Min(Value.GetRawSize() - RawOffset, Request.RawSize);
				TRACE_COUNTER_INCREMENT(FileSystemDDC_GetHit);
				TRACE_COUNTER_ADD(FileSystemDDC_BytesRead, RequestStats.PhysicalReadSize);
				TRACE_COUNTER_ADD(FileSystemDDC_BytesWritten, RequestStats.PhysicalWriteSize);
				if (!bExistsOnly)
				{
					Buffer = ValueReader.Decompress(RawOffset, RawSize);
					RequestStats.LogicalReadSize += Buffer.GetSize();
				}
				Status = bExistsOnly || Buffer.GetSize() == RawSize ? EStatus::Ok : EStatus::Error;
			}
		}
		RequestStats.Status = Status;
		StoreStats->AddRequest(RequestStats);
		UE_CLOG(Status == EStatus::Ok, LogDerivedDataCache, Verbose, TEXT("%s: Cache hit for %s from '%s'"),
			*CachePath, *WriteToString<96>(Request.Key, '/', Request.Id), *Request.Name);
		OnComplete({Request.Name, Request.Key, Request.Id, Request.RawOffset,
			RawSize, Value.GetRawHash(), MoveTemp(Buffer), Request.UserData, Status});
	}
}

bool FFileSystemCacheStore::PutCacheRecord(
	const FStringView Name,
	const FCacheRecord& Record,
	const FCacheRecordPolicy& Policy,
	FRequestStats& Stats)
{
	const bool bLocalDeactivatedForPerformance = IsDeactivatedForPerformance();
	if (bLocalDeactivatedForPerformance || bReadOnly)
	{
		UE_LOG(LogDerivedDataCache, VeryVerbose,
			TEXT("%s: Skipped put of %s from '%.*s' because this cache store is %s"),
			*CachePath,
			*WriteToString<96>(Record.GetKey()),
			Name.Len(),
			Name.GetData(),
			bLocalDeactivatedForPerformance ? TEXT("deactivated due to low performance") : TEXT("read-only"));
		return false;
	}

	const FCacheKey& Key = Record.GetKey();
	const ECachePolicy RecordPolicy = Policy.GetRecordPolicy();

	// Skip the request if storing to the cache is disabled.
	const ECachePolicy StoreFlag = SpeedClass == EBackendSpeedClass::Local ? ECachePolicy::StoreLocal : ECachePolicy::StoreRemote;
	if (!EnumHasAnyFlags(RecordPolicy, StoreFlag))
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
	BuildCachePackagePath(Key, Path);

	// Check if there is an existing record package.
	FCbPackage ExistingPackage;
	const ECachePolicy QueryFlag = SpeedClass == EBackendSpeedClass::Local ? ECachePolicy::QueryLocal : ECachePolicy::QueryRemote;
	bool bReplaceExisting = !EnumHasAnyFlags(RecordPolicy, QueryFlag);
	bool bSavePackage = bReplaceExisting;
	if (const bool bLoadPackage = !bReplaceExisting || !Algo::AllOf(Record.GetValues(), &FValue::HasData))
	{
		// Load the existing package to take its attachments into account.
		// Save the new package if there is no existing package or it fails to load.
		bSavePackage |= !LoadFileWithHash(Path, Name, Stats, [&ExistingPackage](FArchive& Ar) { ExistingPackage.TryLoad(Ar); });
		if (!bSavePackage)
		{
			// Save the new package if the existing package is invalid.
			const FOptionalCacheRecord ExistingRecord = FCacheRecord::Load(ExistingPackage);
			bSavePackage |= !ExistingRecord;
			const auto MakeValueTuple = [](const FValueWithId& Value) -> TTuple<FValueId, FIoHash>
			{
				return MakeTuple(Value.GetId(), Value.GetRawHash());
			};
			if (ExistingRecord && !Algo::CompareBy(ExistingRecord.Get().GetValues(), Record.GetValues(), MakeValueTuple))
			{
				// Content differs between the existing record and the new record.
				UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Cache put found non-deterministic record for %s from '%.*s'"),
					*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
				const auto HasValueContent = [this, Name, &Key, &Stats](const FValueWithId& Value) -> bool
				{
					if (!Value.HasData() && !GetCacheContentExists(Key, Value.GetRawHash(), Stats))
					{
						UE_LOG(LogDerivedDataCache, Log,
							TEXT("%s: Cache put of non-deterministic record will overwrite existing record due to "
							     "missing value %s with hash %s for %s from '%.*s'"),
							*CachePath, *WriteToString<32>(Value.GetId()), *WriteToString<48>(Value.GetRawHash()),
							*WriteToString<96>(Key), Name.Len(), Name.GetData());
						return false;
					}
					return true;
				};
				// Save the new package because the existing package differs and is missing content.
				bSavePackage |= !Algo::AllOf(ExistingRecord.Get().GetValues(), HasValueContent);
			}
			bReplaceExisting |= bSavePackage;
		}
	}

	// Serialize the record to a package and remove attachments that will be stored externally.
	FCbPackage Package = Record.Save();
	TArray<FCompressedBuffer, TInlineAllocator<8>> ExternalContent;
	if (ExistingPackage && !bSavePackage)
	{
		// Mirror the existing internal/external attachment storage.
		TArray<FCompressedBuffer, TInlineAllocator<8>> AllContent;
		Algo::Transform(Package.GetAttachments(), AllContent, &FCbAttachment::AsCompressedBinary);
		for (FCompressedBuffer& Content : AllContent)
		{
			const FIoHash RawHash = Content.GetRawHash();
			if (!ExistingPackage.FindAttachment(RawHash))
			{
				Package.RemoveAttachment(RawHash);
				ExternalContent.Add(MoveTemp(Content));
			}
		}
	}
	else
	{
		// Attempt to copy missing attachments from the existing package.
		if (ExistingPackage)
		{
			for (const FValue& Value : Record.GetValues())
			{
				if (!Value.HasData())
				{
					if (const FCbAttachment* Attachment = ExistingPackage.FindAttachment(Value.GetRawHash()))
					{
						Package.AddAttachment(*Attachment);
					}
				}
			}
		}

		// Remove the largest attachments from the package until it fits within the size limits.
		TArray<FCompressedBuffer, TInlineAllocator<8>> AllContent;
		Algo::Transform(Package.GetAttachments(), AllContent, &FCbAttachment::AsCompressedBinary);
		uint64 TotalSize = Algo::TransformAccumulate(AllContent, &FCompressedBuffer::GetCompressedSize, uint64(0));
		const uint64 MaxSize = (Record.GetValues().Num() == 1 ? MaxValueSizeKB : MaxRecordSizeKB) * 1024;
		if (TotalSize > MaxSize)
		{
			Algo::StableSortBy(AllContent, &FCompressedBuffer::GetCompressedSize, TGreater<>());
			for (FCompressedBuffer& Content : AllContent)
			{
				const uint64 CompressedSize = Content.GetCompressedSize();
				Package.RemoveAttachment(Content.GetRawHash());
				ExternalContent.Add(MoveTemp(Content));
				TotalSize -= CompressedSize;
				if (TotalSize <= MaxSize)
				{
					break;
				}
			}
		}
	}

	// Save the external content to storage.
	for (FCompressedBuffer& Content : ExternalContent)
	{
		if (!PutCacheContent(Name, Content, Stats))
		{
			return false;
		}
	}

	// Save the record package to storage.
	const auto WritePackage = [&Package](FArchive& Ar) { Package.Save(Ar); };
	if (bSavePackage)
	{
		if (!SaveFileWithHash(Path, Name, Stats, WritePackage, bReplaceExisting))
		{
			return false;
		}
		if (const FCbObject& Meta = Record.GetMeta())
		{
			Stats.LogicalWriteSize += Meta.GetSize();
		}
		Stats.LogicalWriteSize += Algo::TransformAccumulate(Package.GetAttachments(),
			[](const FCbAttachment& Attachment) { return Attachment.AsCompressedBinary().GetRawSize(); }, uint64(0));
	}

	if (AccessLogWriter)
	{
		AccessLogWriter->Append(Key, Path);
	}

	return true;
}

FOptionalCacheRecord FFileSystemCacheStore::GetCacheRecordOnly(
	const FStringView Name,
	const FCacheKey& Key,
	const FCacheRecordPolicy& Policy,
	FRequestStats& Stats)
{
	// Skip the request if querying the cache is disabled.
	const ECachePolicy QueryFlag = SpeedClass == EBackendSpeedClass::Local ? ECachePolicy::QueryLocal : ECachePolicy::QueryRemote;
	const bool bLocalDeactivatedForPerformance = IsDeactivatedForPerformance();
	if (bLocalDeactivatedForPerformance || !EnumHasAnyFlags(Policy.GetRecordPolicy(), QueryFlag))
	{
		UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("%s: Skipped get of %s from '%.*s' %s"),
			*CachePath,
			*WriteToString<96>(Key),
			Name.Len(),
			Name.GetData(),
			bLocalDeactivatedForPerformance ?
				TEXT("because this cache store is deactivated due to low performance") :
				TEXT("due to cache policy"));
		return FOptionalCacheRecord();
	}

	if (DebugOptions.ShouldSimulateGetMiss(Key))
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Simulated miss for get of %s from '%.*s'"),
			*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
		return FOptionalCacheRecord();
	}

	TStringBuilder<256> Path;
	BuildCachePackagePath(Key, Path);

	bool bDeletePackage = true;
	ON_SCOPE_EXIT
	{
		if (bDeletePackage && !bReadOnly)
		{
			IFileManager::Get().Delete(*Path, /*bRequireExists*/ false, /*bEvenReadOnly*/ false, /*bQuiet*/ true);
		}
	};

	FOptionalCacheRecord Record;
	{
		FCbPackage Package;
		if (!LoadFileWithHash(Path, Name, Stats, [&Package](FArchive& Ar) { Package.TryLoad(Ar); }))
		{
			UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache miss with missing package for %s from '%.*s'"),
				*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
			return Record;
		}

		if (ValidateCompactBinary(Package, ECbValidateMode::Default | ECbValidateMode::Package) != ECbValidateError::None)
		{
			UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Cache miss with invalid package for %s from '%.*s'"),
				*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
			return Record;
		}

		Record = FCacheRecord::Load(Package);
		if (Record.IsNull())
		{
			UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Cache miss with record load failure for %s from '%.*s'"),
				*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
			return Record;
		}
	}

	if (AccessLogWriter)
	{
		AccessLogWriter->Append(Key, Path);
	}

	bDeletePackage = false;
	return Record;
}

FOptionalCacheRecord FFileSystemCacheStore::GetCacheRecord(
	const FStringView Name,
	const FCacheKey& Key,
	const FCacheRecordPolicy& Policy,
	EStatus& OutStatus,
	FRequestStats& Stats)
{
	FOptionalCacheRecord Record = GetCacheRecordOnly(Name, Key, Policy, Stats);
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
		if (GetCacheContent(Name, Key, Id, Value, ValuePolicy, Content, Stats))
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

bool FFileSystemCacheStore::PutCacheValue(
	const FStringView Name,
	const FCacheKey& Key,
	const FValue& Value,
	const ECachePolicy Policy,
	FRequestStats& Stats)
{
	const bool bLocalDeactivatedForPerformance = IsDeactivatedForPerformance();
	if (bLocalDeactivatedForPerformance || bReadOnly)
	{
		UE_LOG(LogDerivedDataCache, VeryVerbose,
			TEXT("%s: Skipped put of %s from '%.*s' because this cache store is %s"),
			*CachePath,
			*WriteToString<96>(Key),
			Name.Len(),
			Name.GetData(),
			bLocalDeactivatedForPerformance ?
				TEXT("deactivated due to low performance") :
				TEXT("read-only"));
		return false;
	}

	// Skip the request if storing to the cache is disabled.
	const ECachePolicy StoreFlag = SpeedClass == EBackendSpeedClass::Local ? ECachePolicy::StoreLocal : ECachePolicy::StoreRemote;
	if (!EnumHasAnyFlags(Policy, StoreFlag))
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
	FCbPackage ExistingPackage;
	TStringBuilder<256> Path;
	BuildCachePackagePath(Key, Path);
	const ECachePolicy QueryFlag = SpeedClass == EBackendSpeedClass::Local ? ECachePolicy::QueryLocal : ECachePolicy::QueryRemote;
	bool bReplaceExisting = !EnumHasAnyFlags(Policy, QueryFlag);
	bool bSavePackage = bReplaceExisting;
	if (const bool bLoadPackage = !bReplaceExisting || !Value.HasData())
	{
		// Load the existing package to take its attachments into account.
		// Save the new package if there is no existing package or it fails to load.
		bSavePackage |= !LoadFileWithHash(Path, Name, Stats, [&ExistingPackage](FArchive& Ar) { ExistingPackage.TryLoad(Ar); });
		if (!bSavePackage)
		{
			const FCbObjectView Object = ExistingPackage.GetObject();
			const FIoHash RawHash = Object["RawHash"].AsHash();
			const uint64 RawSize = Object["RawSize"].AsUInt64(MAX_uint64);
			if (RawHash.IsZero() || RawSize == MAX_uint64)
			{
				// Save the new package because the existing package is invalid.
				UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Cache put found invalid existing value for %s from '%.*s'"),
					*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
				bSavePackage = true;
			}
			else if (!(RawHash == Value.GetRawHash() && RawSize == Value.GetRawSize()))
			{
				// Content differs between the existing value and the new value.
				UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Cache put found non-deterministic value "
					"with new hash %s and existing hash %s for %s from '%.*s'"),
					*CachePath, *WriteToString<48>(Value.GetRawHash()), *WriteToString<48>(RawHash),
					*WriteToString<96>(Key), Name.Len(), Name.GetData());
				if (!ExistingPackage.FindAttachment(RawHash) && !GetCacheContentExists(Key, RawHash, Stats))
				{
					// Save the new package because the existing package differs and is missing content.
					UE_LOG(LogDerivedDataCache, Log,
						TEXT("%s: Cache put of non-deterministic value will overwrite existing value due to "
						"missing value with hash %s for %s from '%.*s'"),
						*CachePath, *WriteToString<48>(RawHash), *WriteToString<96>(Key), Name.Len(), Name.GetData());
					bSavePackage = true;
				}
			}
			bReplaceExisting |= bSavePackage;
		}
	}

	// Save the value to a package and save the data to external content depending on its size.
	FCbPackage Package;
	uint64 LogicalPackageSize = 0;
	TArray<FCompressedBuffer, TInlineAllocator<1>> ExternalContent;
	if (ExistingPackage && !bSavePackage)
	{
		if (Value.HasData() && !ExistingPackage.FindAttachment(Value.GetRawHash()))
		{
			ExternalContent.Add(Value.GetData());
		}
	}
	else
	{
		FCbWriter Writer;
		Writer.BeginObject();
		Writer.AddBinaryAttachment("RawHash", Value.GetRawHash());
		Writer.AddInteger("RawSize", Value.GetRawSize());
		Writer.EndObject();

		Package.SetObject(Writer.Save().AsObject());
		if (!Value.HasData())
		{
			// Verify that the content exists in storage.
			if (!GetCacheContentExists(Key, Value.GetRawHash(), Stats))
			{
				UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Failed due to missing data for put of %s from '%.*s'"),
					*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
				return false;
			}
		}
		else if (Value.GetData().GetCompressedSize() <= MaxValueSizeKB * 1024)
		{
			// Store the content in the package.
			LogicalPackageSize += Value.GetRawSize();
			Package.AddAttachment(FCbAttachment(Value.GetData()));
		}
		else
		{
			ExternalContent.Add(Value.GetData());
		}
	}

	// Save the external content to storage.
	for (FCompressedBuffer& Content : ExternalContent)
	{
		if (!PutCacheContent(Name, Content, Stats))
		{
			return false;
		}
	}

	// Save the value package to storage.
	const auto WritePackage = [&Package](FArchive& Ar) { Package.Save(Ar); };
	if (bSavePackage)
	{
		if (!SaveFileWithHash(Path, Name, Stats, WritePackage, bReplaceExisting))
		{
			return false;
		}
		Stats.LogicalWriteSize += LogicalPackageSize;
	}

	if (AccessLogWriter)
	{
		AccessLogWriter->Append(Key, Path);
	}

	return true;
}

bool FFileSystemCacheStore::GetCacheValueOnly(
	const FStringView Name,
	const FCacheKey& Key,
	const ECachePolicy Policy,
	FValue& OutValue,
	FRequestStats& Stats)
{
	// Skip the request if querying the cache is disabled.
	const ECachePolicy QueryFlag = SpeedClass == EBackendSpeedClass::Local ? ECachePolicy::QueryLocal : ECachePolicy::QueryRemote;
	const bool bLocalDeactivatedForPerformance = IsDeactivatedForPerformance();
	if (bLocalDeactivatedForPerformance || !EnumHasAnyFlags(Policy, QueryFlag))
	{
		UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("%s: Skipped get of %s from '%.*s' %s"),
			*CachePath,
			*WriteToString<96>(Key),
			Name.Len(),
			Name.GetData(),
			bLocalDeactivatedForPerformance ?
			TEXT("because this cache store is deactivated due to low performance") :
			TEXT("due to cache policy"));
		return false;
	}

	if (DebugOptions.ShouldSimulateGetMiss(Key))
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Simulated miss for get of %s from '%.*s'"),
			*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
		return false;
	}

	TStringBuilder<256> Path;
	BuildCachePackagePath(Key, Path);

	bool bDeletePackage = true;
	ON_SCOPE_EXIT
	{
		if (bDeletePackage && !bReadOnly)
		{
			IFileManager::Get().Delete(*Path, /*bRequireExists*/ false, /*bEvenReadOnly*/ false, /*bQuiet*/ true);
		}
	};

	FCbPackage Package;
	if (!LoadFileWithHash(Path, Name, Stats, [&Package](FArchive& Ar) { Package.TryLoad(Ar); }))
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache miss with missing package for %s from '%.*s'"),
			*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
		return false;
	}

	if (ValidateCompactBinary(Package, ECbValidateMode::Default | ECbValidateMode::Package) != ECbValidateError::None)
	{
		UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Cache miss with invalid package for %s from '%.*s'"),
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

	if (const FCbAttachment* const Attachment = Package.FindAttachment(RawHash))
	{
		const FCompressedBuffer& Data = Attachment->AsCompressedBinary();
		if (Data.GetRawHash() != RawHash || Data.GetRawSize() != RawSize)
		{
			UE_LOG(LogDerivedDataCache, Display,
				TEXT("%s: Cache miss with invalid value attachment for %s from '%.*s'"),
				*CachePath, *WriteToString<96>(Key), Name.Len(), Name.GetData());
			return false;
		}
		OutValue = FValue(Data);
	}
	else
	{
		OutValue = FValue(RawHash, RawSize);
	}

	if (AccessLogWriter)
	{
		AccessLogWriter->Append(Key, Path);
	}

	bDeletePackage = false;
	return true;
}

bool FFileSystemCacheStore::GetCacheValue(
	const FStringView Name,
	const FCacheKey& Key,
	const ECachePolicy Policy,
	FValue& OutValue,
	FRequestStats& Stats)
{
	return GetCacheValueOnly(Name, Key, Policy, OutValue, Stats) && GetCacheContent(Name, Key, {}, OutValue, Policy, OutValue, Stats);
}

bool FFileSystemCacheStore::PutCacheContent(
	const FStringView Name,
	const FCompressedBuffer& Content,
	FRequestStats& Stats) const
{
	const FIoHash& RawHash = Content.GetRawHash();
	TStringBuilder<256> Path;
	BuildCacheContentPath(RawHash, Path);
	const auto WriteContent = [&Content](FArchive& Ar) { Content.Save(Ar); };
	if (!FileExists(Path, Stats))
	{
		if (!SaveFileWithHash(Path, Name, Stats, WriteContent))
		{
			return false;
		}
		Stats.LogicalWriteSize += Content.GetRawSize();
	}
	if (AccessLogWriter)
	{
		AccessLogWriter->Append(RawHash, Path);
	}
	return true;
}

bool FFileSystemCacheStore::GetCacheContentExists(const FCacheKey& Key, const FIoHash& RawHash, FRequestStats& Stats) const
{
	TStringBuilder<256> Path;
	BuildCacheContentPath(RawHash, Path);
	return FileExists(Path, Stats);
}

bool FFileSystemCacheStore::GetCacheContent(
	const FStringView Name,
	const FCacheKey& Key,
	const FValueId& Id,
	const FValue& Value,
	const ECachePolicy Policy,
	FValue& OutValue,
	FRequestStats& Stats) const
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
	BuildCacheContentPath(RawHash, Path);
	if (EnumHasAnyFlags(Policy, ECachePolicy::SkipData))
	{
		if (FileExists(Path, Stats))
		{
			if (AccessLogWriter)
			{
				AccessLogWriter->Append(RawHash, Path);
			}
			OutValue = Value;
			return true;
		}
	}
	else
	{
		FCompressedBuffer CompressedBuffer;
		if (LoadFileWithHash(Path, Name, Stats, [&CompressedBuffer](FArchive& Ar) { CompressedBuffer = FCompressedBuffer::Load(Ar); }))
		{
			if (CompressedBuffer.GetRawHash() == RawHash)
			{
				if (AccessLogWriter)
				{
					AccessLogWriter->Append(RawHash, Path);
				}
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

void FFileSystemCacheStore::GetCacheContent(
	const FStringView Name,
	const FCacheKey& Key,
	const FValueId& Id,
	const FValue& Value,
	const ECachePolicy Policy,
	FCompressedBufferReader& Reader,
	TUniquePtr<FArchive>& OutArchive,
	FRequestStats& Stats) const
{
	class FStatsArchive final : TUniquePtr<FArchive>, public FArchiveProxy
	{
	public:
		FStatsArchive(FArchive& InArchive, FRequestStats& InStats)
			: TUniquePtr<FArchive>(&InArchive)
			, FArchiveProxy(InArchive)
			, Stats(InStats)
		{
		}

		void Serialize(void* V, int64 Length) final
		{
			Stats.PhysicalReadSize += uint64(Length);
			FArchiveProxy::Serialize(V, Length);
		}

	private:
		FRequestStats& Stats;
	};

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
	BuildCacheContentPath(RawHash, Path);
	if (EnumHasAllFlags(Policy, ECachePolicy::SkipData))
	{
		if (FileExists(Path, Stats))
		{
			if (AccessLogWriter)
			{
				AccessLogWriter->Append(RawHash, Path);
			}
			return;
		}
	}
	else
	{
		OutArchive = OpenFileRead(Path, Name, Stats);
		if (OutArchive)
		{
			OutArchive.Reset(new FStatsArchive(*OutArchive.Release(), Stats));
			UE_LOG(LogDerivedDataCache, VeryVerbose,
				TEXT("%s: Opened %s from '%.*s'"),
				*CachePath, *Path, Name.Len(), Name.GetData());
			Reader.SetSource(*OutArchive);
			if (Reader.GetRawHash() == RawHash)
			{
				if (AccessLogWriter)
				{
					AccessLogWriter->Append(RawHash, Path);
				}
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

void FFileSystemCacheStore::BuildCachePackagePath(const FCacheKey& CacheKey, FStringBuilderBase& Path) const
{
	Path << CachePath << TEXT('/');
	BuildPathForCachePackage(CacheKey, Path);
}

void FFileSystemCacheStore::BuildCacheContentPath(const FIoHash& RawHash, FStringBuilderBase& Path) const
{
	Path << CachePath << TEXT('/');
	BuildPathForCacheContent(RawHash, Path);
}

bool FFileSystemCacheStore::SaveFileWithHash(
	FStringBuilderBase& Path,
	const FStringView DebugName,
	FRequestStats& Stats,
	const TFunctionRef<void (FArchive&)> WriteFunction,
	const bool bReplaceExisting) const
{
	return SaveFile(Path, DebugName, Stats, [&WriteFunction](FArchive& Ar)
	{
		THashingArchiveProxy<FBlake3> HashAr(Ar);
		WriteFunction(HashAr);
		FBlake3Hash Hash = HashAr.GetHash();
		Ar << Hash;
	}, bReplaceExisting);
}

bool FFileSystemCacheStore::LoadFileWithHash(
	FStringBuilderBase& Path,
	const FStringView DebugName,
	FRequestStats& Stats,
	const TFunctionRef<void (FArchive& Ar)> ReadFunction) const
{
	return LoadFile(Path, DebugName, Stats, [this, &Path, &DebugName, &ReadFunction](FArchive& Ar)
	{
		THashingArchiveProxy<FBlake3> HashAr(Ar);
		ReadFunction(HashAr);
		const FBlake3Hash Hash = HashAr.GetHash();
		FBlake3Hash SavedHash;
		Ar << SavedHash;

		if (Hash != SavedHash && !Ar.IsError())
		{
			Ar.SetError();
			UE_LOG(LogDerivedDataCache, Display,
				TEXT("%s: File %s from '%.*s' is corrupted and has hash %s when %s is expected."),
				*CachePath, *Path, DebugName.Len(), DebugName.GetData(),
				*WriteToString<80>(Hash), *WriteToString<80>(SavedHash));
		}
	});
}

bool FFileSystemCacheStore::SaveFile(
	FStringBuilderBase& Path,
	const FStringView DebugName,
	FRequestStats& Stats,
	const TFunctionRef<void (FArchive&)> WriteFunction,
	const bool bReplaceExisting) const
{
	const double StartTime = FPlatformTime::Seconds();

	TStringBuilder<256> TempPath;
	TempPath << FPathViews::GetPath(Path) << TEXT("/Temp.") << FGuid::NewGuid();

	TUniquePtr<FArchive> Ar = OpenFileWrite(TempPath, DebugName, Stats);
	if (!Ar)
	{
		UE_LOG(LogDerivedDataCache, Warning,
			TEXT("%s: Failed to open temp file %s for writing when saving %s from '%.*s'. Error 0x%08x."),
			*CachePath, *TempPath, *Path, DebugName.Len(), DebugName.GetData(), FPlatformMisc::GetLastError());
		return false;
	}

	WriteFunction(*Ar);
	const int64 WriteSize = Ar->Tell();

	if (!Ar->Close() || WriteSize == 0 || WriteSize != IFileManager::Get().FileSize(*TempPath))
	{
		UE_LOG(LogDerivedDataCache, Warning,
			TEXT("%s: Failed to write to temp file %s when saving %s from '%.*s'. Error 0x%08x. "
			"File is %" INT64_FMT " bytes when %" INT64_FMT " bytes are expected."),
			*CachePath, *TempPath, *Path, DebugName.Len(), DebugName.GetData(), FPlatformMisc::GetLastError(),
			IFileManager::Get().FileSize(*TempPath), WriteSize);
		IFileManager::Get().Delete(*TempPath, /*bRequireExists*/ false, /*bEvenReadOnly*/ false, /*bQuiet*/ true);
		return false;
	}

	if (!IFileManager::Get().Move(*Path, *TempPath, bReplaceExisting, /*bEvenIfReadOnly*/ false, /*bAttributes*/ false, /*bDoNotRetryOrError*/ true))
	{
		UE_LOG(LogDerivedDataCache, Log,
			TEXT("%s: Move collision when writing file %s from '%.*s'."),
			*CachePath, *Path, DebugName.Len(), DebugName.GetData());
		IFileManager::Get().Delete(*TempPath, /*bRequireExists*/ false, /*bEvenReadOnly*/ false, /*bQuiet*/ true);
	}

	const double WriteDuration = FPlatformTime::Seconds() - StartTime;
	const double WriteSpeed = WriteDuration > 0.001 ? (double(WriteSize) / WriteDuration) / (1024.0 * 1024.0) : 0.0;
	UE_LOG(LogDerivedDataCache, VeryVerbose,
		TEXT("%s: Saved %s from '%.*s' (%" INT64_FMT " bytes, %.02f secs, %.2f MiB/s)"),
		*CachePath, *Path, DebugName.Len(), DebugName.GetData(), WriteSize, WriteDuration, WriteSpeed);

	if (WriteSize > 0)
	{
		Stats.PhysicalWriteSize += uint64(WriteSize);
	}

	return true;
}

bool FFileSystemCacheStore::LoadFile(
	FStringBuilderBase& Path,
	const FStringView DebugName,
	FRequestStats& Stats,
	const TFunctionRef<void (FArchive& Ar)> ReadFunction) const
{
	const double StartTime = FPlatformTime::Seconds();

	TUniquePtr<FArchive> Ar = OpenFileRead(Path, DebugName, Stats);
	if (!Ar)
	{
		return false;
	}

	ReadFunction(*Ar);
	const int64 ReadSize = Ar->Tell();
	const bool bError = !Ar->Close();

	if (bError)
	{
		UE_LOG(LogDerivedDataCache, Display,
			TEXT("%s: Failed to load file %s from '%.*s'."),
			*CachePath, *Path, DebugName.Len(), DebugName.GetData());

		if (!bReadOnly)
		{
			IFileManager::Get().Delete(*Path, /*bRequireExists*/ false, /*bEvenReadOnly*/ false, /*bQuiet*/ true);
		}
	}
	else
	{
		const double ReadDuration = FPlatformTime::Seconds() - StartTime;
		const double ReadSpeed = ReadDuration > 0.001 ? (double(ReadSize) / ReadDuration) / (1024.0 * 1024.0) : 0.0;

		UE_LOG(LogDerivedDataCache, VeryVerbose,
			TEXT("%s: Loaded %s from '%.*s' (%" INT64_FMT " bytes, %.02f secs, %.2f MiB/s)"),
			*CachePath, *Path, DebugName.Len(), DebugName.GetData(), ReadSize, ReadDuration, ReadSpeed);

		if (!GIsBuildMachine && ReadDuration > 5.0)
		{
			// Slower than 0.5 MiB/s?
			UE_CLOG(ReadSpeed < 0.5, LogDerivedDataCache, Warning,
				TEXT("%s: Loading %s from '%.*s' is very slow (%.2f MiB/s); consider disabling this cache store."),
				*CachePath, *Path, DebugName.Len(), DebugName.GetData(), ReadSpeed);
		}
	}

	if (ReadSize > 0)
	{
		Stats.PhysicalReadSize += uint64(ReadSize);
	}

	return !bError && ReadSize > 0;
}

TUniquePtr<FArchive> FFileSystemCacheStore::OpenFileWrite(FStringBuilderBase& Path, const FStringView DebugName, FRequestStats& Stats) const
{
	const FMonotonicTimePoint StartTime = FMonotonicTimePoint::Now();
	ON_SCOPE_EXIT { Stats.AddLatency(FMonotonicTimePoint::Now() - StartTime); };

	// Retry to handle a race where the directory is deleted while the file is being created.
	constexpr int32 MaxAttemptCount = 3;
	for (int32 AttemptCount = 0; AttemptCount < MaxAttemptCount; ++AttemptCount)
	{
		if (TUniquePtr<FArchive> Ar{IFileManager::Get().CreateFileWriter(*Path, FILEWRITE_Silent)})
		{
			return Ar;
		}
	}
	return nullptr;
}

TUniquePtr<FArchive> FFileSystemCacheStore::OpenFileRead(FStringBuilderBase& Path, const FStringView DebugName, FRequestStats& Stats) const
{
	// Checking for existence may update the modification time to avoid the file being evicted from the cache.
	// Reduce Game Thread overhead by executing the update on a worker thread if the path implies higher latency.
	if (IsInGameThread() && FStringView(CachePath).StartsWith(TEXTVIEW("//"), ESearchCase::CaseSensitive))
	{
		FRequestOwner AsyncOwner(EPriority::Normal);
		Private::LaunchTaskInCacheThreadPool(AsyncOwner, [this, Path = MakeShared<TStringBuilder<256>>(InPlace, Path)]() mutable
		{
			FRequestStats UnusedStats;
			(void)FileExists(*Path, UnusedStats);
		});
		AsyncOwner.KeepAlive();
	}
	else
	{
		if (!FileExists(Path, Stats))
		{
			return nullptr;
		}
	}

	const FMonotonicTimePoint StartTime = FMonotonicTimePoint::Now();
	ON_SCOPE_EXIT { Stats.AddLatency(FMonotonicTimePoint::Now() - StartTime); };
	return TUniquePtr<FArchive>(IFileManager::Get().CreateFileReader(*Path, FILEREAD_Silent));
}

bool FFileSystemCacheStore::FileExists(FStringBuilderBase& Path, FRequestStats& Stats) const
{
	const FMonotonicTimePoint StartTime = FMonotonicTimePoint::Now();
	const FDateTime TimeStamp = IFileManager::Get().GetTimeStamp(*Path);
	Stats.AddLatency(FMonotonicTimePoint::Now() - StartTime);
	if (TimeStamp == FDateTime::MinValue())
	{
		return false;
	}
	if (bTouch || (!bReadOnly && (FDateTime::UtcNow() - TimeStamp).GetTotalDays() > (MaxFileAgeInDays / 4)))
	{
		IFileManager::Get().SetTimeStamp(*Path, FDateTime::UtcNow());
	}
	return true;
}

bool FFileSystemCacheStore::IsDeactivatedForPerformance()
{
	if ((DeactivateAtMs <= 0.f) || !bDeactivatedForPerformance.load(std::memory_order_relaxed))
	{
		return false;
	}

	// Look for an opportunity to consume the output of an existing completed performance evaluation task
	{
		FReadScopeLock ReadLock(PerformanceReEvaluationTaskLock);
		if (PerformanceReEvaluationTask.IsValid())
		{
			if (PerformanceReEvaluationTask.IsCompleted())
			{
				EPerformanceReEvaluationResult Result =
					PerformanceReEvaluationTask.GetResult().exchange(
						EPerformanceReEvaluationResult::Invalid,
						std::memory_order_relaxed);

				if (Result != EPerformanceReEvaluationResult::Invalid)
				{
					LastPerformanceEvaluationTicks.store(FDateTime::UtcNow().GetTicks(), std::memory_order_relaxed);
					bool bLocalDeactivatedForPerformance =
						Result == EPerformanceReEvaluationResult::PerformanceDeactivate;

					if (!bLocalDeactivatedForPerformance)
					{
						// We're no longer deactivated for performance.  If maintenance was deferred, do it now.
						if (FFileSystemCacheStoreMaintainerParams* MaintainerParams = DeactivationDeferredMaintainerParams.Get())
						{
							Maintainer = MakeUnique<FFileSystemCacheStoreMaintainer>(*MaintainerParams, CachePath);
							DeactivationDeferredMaintainerParams.Reset();

							if (bDeactivationDeferredClean)
							{
								Maintainer->BoostPriority();
								Maintainer->WaitForIdle();
							}
						}

						UE_LOG(LogDerivedDataCache, Display,
							TEXT("%s: Performance has improved and meets minimum performance criteria. "
								"It will be reactivated now."),
							*CachePath);
					}

					bDeactivatedForPerformance.store(bLocalDeactivatedForPerformance, std::memory_order_relaxed);
					UpdateStatus();
					return bLocalDeactivatedForPerformance;
				}
			}
			else
			{
				// Avoid attempting to get a write lock and see if you can spawn a new evaluation task
				return true;
			}
		}
	}

	// Look for an opportunity to start a new performance evaluation task
	FTimespan TimespanSinceLastPerfEval =
		FDateTime::UtcNow() - FDateTime(LastPerformanceEvaluationTicks.load(std::memory_order_relaxed));

	if (TimespanSinceLastPerfEval > FTimespan::FromMinutes(1))
	{
		FWriteScopeLock WriteLock(PerformanceReEvaluationTaskLock);
		// After acquiring the write lock, ensure that the task hasn't been re-launched
		// (and possibly completed and consumed) by someone else while we were waiting.
		// This is evaluated by checking that:
		// 1. Task is invalid or task is valid and has a consumed result
		// and
		// 2. Task consumption time is still larger than our re-evaluation interval
		if (!PerformanceReEvaluationTask.IsValid() ||
			(PerformanceReEvaluationTask.IsCompleted() &&
				(PerformanceReEvaluationTask.GetResult().load(std::memory_order_relaxed) ==
					EPerformanceReEvaluationResult::Invalid)
			))
		{
			TimespanSinceLastPerfEval =
				FDateTime::UtcNow() - FDateTime(LastPerformanceEvaluationTicks.load(std::memory_order_relaxed));

			if (TimespanSinceLastPerfEval > FTimespan::FromMinutes(1))
			{
				PerformanceReEvaluationTask = Tasks::Launch(TEXT("FFileSystemCacheStore::ReEvaluatePerformance"),
					[CachePath = this->CachePath, DeactivateAtMs = this->DeactivateAtMs]() ->
						std::atomic<EPerformanceReEvaluationResult>
					{
						check(DeactivateAtMs > 0.f);

						FDerivedDataCacheSpeedStats LocalSpeedStats;
						LocalSpeedStats.ReadSpeedMBs = 999;
						LocalSpeedStats.WriteSpeedMBs = 999;
						LocalSpeedStats.LatencyMS = 0;

						RunSpeedTest(CachePath,
							true /* bReadOnly */,
							true /* bSeekTimeOnly */,
							LocalSpeedStats.LatencyMS,
							LocalSpeedStats.ReadSpeedMBs,
							LocalSpeedStats.WriteSpeedMBs,
							nullptr,
							nullptr);

						if (LocalSpeedStats.LatencyMS >= DeactivateAtMs)
						{
							return EPerformanceReEvaluationResult::PerformanceDeactivate;
						}
						return EPerformanceReEvaluationResult::PerformanceActivate;
					});
			}
		}
	}

	return true;
}

void FFileSystemCacheStore::UpdateStatus()
{
	if (StoreStats)
	{
		if (bDeactivatedForPerformance.load(std::memory_order_relaxed))
		{
			StoreStats->SetStatus(ECacheStoreStatusCode::Warning, NSLOCTEXT("DerivedDataCache", "DeactivatedForPerformance", "Deactivated for performance"));
		}
		else
		{
			StoreStats->SetStatus(ECacheStoreStatusCode::None, {});
		}
	}
}

bool FFileSystemCacheStore::RunInitialSpeedTest(const FFileSystemCacheStoreParams& Params, FDerivedDataCacheSpeedStats& OutSpeedStats)
{
	struct FSpeedTestState : public FThreadSafeRefCountedObject
	{
		FDerivedDataCacheSpeedStats SpeedStats;
		std::atomic<uint32> NumLatencyTestsCompleted = 0;
		std::atomic<bool> AbandonRequest = false;
		bool bResult = false;
		FManualResetEvent CompletionEvent;
	};

	TRefCountPtr<FSpeedTestState> SpeedTestState = new FSpeedTestState();
	Tasks::Launch(TEXT("FFileSystemCacheStore::InitialEvaluation"),
		[CachePath = Params.CachePath, bReadOnly = Params.bReadOnly, SpeedTestState]()
		{
			SpeedTestState->bResult = RunSpeedTest(CachePath,
				bReadOnly,
				false /* bSeekTimeOnly */,
				SpeedTestState->SpeedStats.LatencyMS,
				SpeedTestState->SpeedStats.ReadSpeedMBs,
				SpeedTestState->SpeedStats.WriteSpeedMBs,
				&SpeedTestState->NumLatencyTestsCompleted,
				&SpeedTestState->AbandonRequest);
			SpeedTestState->CompletionEvent.Notify();
		});

	if (!GIsBuildMachine && FPlatformProcess::SupportsMultithreading() && (Params.DeactivateAtMs > 0.f))
	{
		if (SpeedTestState->CompletionEvent.WaitFor(FMonotonicTimeSpan::FromMilliseconds(Params.DeactivateAtMs * 2.f)))
		{
			// If the task completed in the initial wait period, return the result
			OutSpeedStats = SpeedTestState->SpeedStats;
			return SpeedTestState->bResult;
		}
		else
		{
			// If the task did not complete the initial wait period, evaluate if we're progressing fast enough to keep waiting
			// or we should abandon it and supply generic "bad" speed test results.
			if (SpeedTestState->NumLatencyTestsCompleted.load(std::memory_order_acquire) < 2)
			{
				SpeedTestState->AbandonRequest.store(true, std::memory_order_relaxed);
				OutSpeedStats.ReadSpeedMBs = 0.0;
				OutSpeedStats.WriteSpeedMBs = 0.0;
				OutSpeedStats.LatencyMS = 999.0;
				UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Skipping speed test due to slow test progress. Assuming poor performance."), *Params.CachePath);
				return true;
			}
		}
	}

	// Wait indefinitely for completion
	SpeedTestState->CompletionEvent.Wait();
	OutSpeedStats = SpeedTestState->SpeedStats;
	return SpeedTestState->bResult;
}

void FFileSystemCacheStoreParams::Parse(const TCHAR* Name, const TCHAR* Config)
{
	CacheName = Name;

	FString Key;

	// Path Params

	FParse::Value(Config, TEXT("Path="), CachePath);

	if (FParse::Value(Config, TEXT("EnvPathOverride="), Key))
	{
		if (FString Value = FPlatformMisc::GetEnvironmentVariable(*Key); !Value.IsEmpty())
		{
			CachePath = MoveTemp(Value);
			UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Found environment variable %s=%s"), Name, *Key, *CachePath);
		}
		if (FString Value; FPlatformMisc::GetStoredValue(TEXT("Epic Games"), TEXT("GlobalDataCachePath"), *Key, Value) && !Value.IsEmpty())
		{
			CachePath = MoveTemp(Value);
			UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Found registry key GlobalDataCachePath %s=%s"), Name, *Key, *CachePath);
		}
	}

	if (FParse::Value(Config, TEXT("CommandLineOverride="), Key) &&
		FParse::Value(FCommandLine::Get(), *(Key + TEXT("=")), CachePath))
	{
		UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Found command line override %s=%s"), Name, *Key, *CachePath);
	}

	if (FParse::Value(Config, TEXT("EditorOverrideSetting="), Key))
	{
		if (FString Setting; GConfig->GetString(TEXT("/Script/UnrealEd.EditorSettings"), *Key, Setting, GEditorSettingsIni) && !Setting.IsEmpty())
		{
			if (FString Value; FParse::Value(*Setting, TEXT("Path="), Value))
			{
				Value.TrimQuotesInline();
				Value.ReplaceEscapedCharWithCharInline();
				if (!Value.IsEmpty())
				{
					CachePath = MoveTemp(Value);
					UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Found editor settings override %s=%s"), Name, *Key, *CachePath);
				}
			}
		}
	}

	// Paths that start with a '?' are config keys.
	if (CachePath.StartsWith(TEXT("?")) && !GConfig->GetString(TEXT("DerivedDataCacheSettings"), *CachePath + 1, CachePath, GEngineIni))
	{
		CachePath.Empty();
	}

	FPaths::NormalizeFilename(CachePath);

	if (const FString AbsoluteCachePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*CachePath); AbsoluteCachePath.Len() >= GMaxCacheRootLen)
	{
		const FText ErrorMessage = FText::Format(NSLOCTEXT("DerivedDataCache", "PathTooLong", "Cache path {0} is longer than {1} characters. Shorten the path to leave more characters for cache keys."),
			FText::FromString(AbsoluteCachePath), FText::AsNumber(GMaxCacheRootLen));
		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
		UE_LOG(LogDerivedDataCache, Fatal, TEXT("%s"), *ErrorMessage.ToString());
	}

	// Other Params

	FParse::Value(Config, TEXT("WriteAccessLog="), AccessLogPath);
	FParse::Value(Config, TEXT("MaxRecordSizeKB="), MaxRecordSizeKB);
	FParse::Value(Config, TEXT("MaxValueSizeKB="), MaxValueSizeKB);
	FParse::Value(Config, TEXT("UnusedFileAge="), MaxFileAgeInDays);
	FParse::Value(Config, TEXT("ConsiderSlowAt="), ConsiderSlowAtMs);
	FParse::Value(Config, TEXT("DeactivateAt="), DeactivateAtMs);
	FParse::Bool(Config, TEXT("PromptIfMissing="), bPromptIfMissing);
	FParse::Bool(Config, TEXT("DeleteOnly="), bDeleteOnly);
	FParse::Bool(Config, TEXT("ReadOnly="), bReadOnly);
	FParse::Bool(Config, TEXT("Clean="), bClean);
	FParse::Bool(Config, TEXT("Flush="), bFlush);

	FParse::Bool(Config, TEXT("Touch="), bTouch);
	bTouch = bTouch || FParse::Param(FCommandLine::Get(), TEXT("DDCTOUCH"));

	bSkipSpeedTest = bSkipSpeedTest || FParse::Param(FCommandLine::Get(), TEXT("DDCSkipSpeedTest"));

	bDeleteUnused = !bReadOnly;
	FParse::Bool(Config, TEXT("DeleteUnused="), bDeleteUnused);
	bDeleteUnused = bDeleteUnused && !FParse::Param(FCommandLine::Get(), TEXT("NoDDCCleanup"));

	if (!FParse::Value(Config, TEXT("MaxFileChecksPerSec="), MaxScanRate))
	{
		int32 MaxFileScanRate;
		if (GConfig->GetInt(TEXT("DDCCleanup"), TEXT("MaxFileChecksPerSec"), MaxFileScanRate, GEngineIni))
		{
			MaxScanRate = uint32(MaxFileScanRate);
		}
	}

	FParse::Value(Config, TEXT("FoldersToClean="), MaxDirectoryScanCount);

	GConfig->GetDouble(TEXT("DDCCleanup"), TEXT("TimeToWaitAfterInit"), MaintenanceDelayInSeconds, GEngineIni);

	FParse::Value(Config, TEXT("RedirectionFileName="), RedirectionFileName);
	FParse::Value(Config, TEXT("RedirectionKeyName="), RedirectionKeyName);
}

ILegacyCacheStore* CreateFileSystemCacheStore(
	const TCHAR* Name,
	const TCHAR* Config,
	ICacheStoreOwner& Owner,
	ICacheStoreGraph* Graph,
	FString& OutPath,
	bool& bOutRedirected)
{
	FFileSystemCacheStoreParams Params;
	Params.Parse(Name, Config);
	return FFileSystemCacheStore::TryCreate(Params, Owner, Graph, OutPath, bOutRedirected);
}

} // UE::DerivedData
