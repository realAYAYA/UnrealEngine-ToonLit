// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDataGatherer.h"
#include "AssetDataGathererPrivate.h"

#include "Algo/AnyOf.h"
#include "Algo/Find.h"
#include "Algo/Sort.h"
#include "Algo/Unique.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistryArchive.h"
#include "AssetRegistryPrivate.h"
#include "Async/MappedFileHandle.h"
#include "Async/ParallelFor.h"
#include "Containers/BinaryHeap.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "Hash/xxhash.h"
#include "Math/NumericLimits.h"
#include "Memory/MemoryView.h"
#include "Misc/AsciiSet.h"
#include "Misc/Char.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeExit.h"
#include "Misc/TrackedActivity.h"
#include "Serialization/Archive.h"
#include "String/LexFromString.h"
#include "Tasks/Task.h"
#include "Templates/UniquePtr.h"
#include "Templates/UnrealTemplate.h"
#include "Trace/Trace.h"
#include "Trace/Trace.inl"

namespace AssetDataGathererConstants
{
	constexpr int32 SingleThreadFilesPerBatch = 3;
	constexpr int32 ExpectedMaxBatchSize = 100;
	constexpr int32 MinSecondsToElapseBeforeCacheWrite = 30;
	constexpr int32 MinAssetReadsBeforeCacheWrite = 100000;
	constexpr int32 CacheShardAssetCount = 250000;
	static constexpr uint32 CacheSerializationMagic = 0xC3A9D89B; // Versioning and integrity checking
	static constexpr uint64 CurrentVersion = FAssetRegistryVersion::LatestVersion | (uint64(CacheSerializationMagic) << 32);
	static const FTimespan CachePruneAge = FTimespan(7, 0, 0, 0);
	/**
	 * The maximum number of threads used in the ParallelFor in FAssetDataDiscovery::TickInternal.
	 * Higher values cause more time spent waking up workers. Lower values cause more time spent running the DirectoryEnumeration on each thread.
	 * Current value was chosen based on experiments on an SSD drive with a warm disk cache.
	 */
	static int32 GARDiscoverThreads = 15;
	/**
	 * Minimum batch sized used used in the ParallelFor in FAssetDataDiscovery::TickInternal.
	 * In theory higher minimum values could reduce the cost of waking up workers, but in practice it does not have a significant effect.
	 */
	static int32 GARDiscoverMinBatchSize = 1;
}

namespace UE::AssetDataGather::Private
{

bool bBlockPackagesWithMarkOfTheWeb = false;
static FAutoConsoleVariableRef CVarBlockPackagesWithMarkOfTheWeb(
	TEXT("AssetRegistry.BlockPackagesWithMarkOfTheWeb"),
	bBlockPackagesWithMarkOfTheWeb,
	TEXT("Whether package files with mark of the web are blocked from the asset registry"));

bool bIgnoreEmptyDirectories = false;
static FAutoConsoleVariableRef CVarIgnoreEmptyDirectories(
	TEXT("AssetRegistry.IgnoreEmptyDirectories"),
	bIgnoreEmptyDirectories,
	TEXT("If true, completely empty leaf directories are ignored by the asset registry while scanning"));

void LexFromString(EFeatureEnabledReadWrite& OutValue, FStringView Text)
{
	Text.TrimStartAndEndInline();

	if (Text.Equals(TEXTVIEW("NeverWriteNeverRead"), ESearchCase::IgnoreCase) ||
		Text.Equals(TEXTVIEW("Never"), ESearchCase::IgnoreCase))
	{
		OutValue = EFeatureEnabledReadWrite::NeverWriteNeverRead;
		return;
	}
	if (Text.Equals(TEXTVIEW("NeverWriteDefaultRead"), ESearchCase::IgnoreCase))
	{
		OutValue = EFeatureEnabledReadWrite::NeverWriteDefaultRead;
		return;
	}
	if (Text.Equals(TEXTVIEW("NeverWriteAlwaysRead"), ESearchCase::IgnoreCase))
	{
		OutValue = EFeatureEnabledReadWrite::NeverWriteAlwaysRead;
		return;
	}

	if (Text.Equals(TEXTVIEW("DefaultWriteNeverRead"), ESearchCase::IgnoreCase))
	{
		OutValue = EFeatureEnabledReadWrite::DefaultWriteNeverRead;
		return;
	}
	if (Text.Equals(TEXTVIEW("DefaultWriteDefaultRead"), ESearchCase::IgnoreCase) ||
		Text.Equals(TEXTVIEW("Default"), ESearchCase::IgnoreCase))
	{
		OutValue = EFeatureEnabledReadWrite::DefaultWriteDefaultRead;
		return;
	}
	if (Text.Equals(TEXTVIEW("DefaultWriteAlwaysRead"), ESearchCase::IgnoreCase))
	{
		OutValue = EFeatureEnabledReadWrite::DefaultWriteAlwaysRead;
		return;
	}

	if (Text.Equals(TEXTVIEW("AlwaysWriteNeverRead"), ESearchCase::IgnoreCase))
	{
		OutValue = EFeatureEnabledReadWrite::AlwaysWriteNeverRead;
		return;
	}
	if (Text.Equals(TEXTVIEW("AlwaysWriteDefaultRead"), ESearchCase::IgnoreCase) ||
		Text.Equals(TEXTVIEW("AlwaysWrite"), ESearchCase::IgnoreCase))
	{
		OutValue = EFeatureEnabledReadWrite::AlwaysWriteDefaultRead;
		return;
	}
	if (Text.Equals(TEXTVIEW("AlwaysWriteAlwaysRead"), ESearchCase::IgnoreCase))
	{
		OutValue = EFeatureEnabledReadWrite::AlwaysWriteAlwaysRead;
		return;
	}
	if (Text.Equals(TEXTVIEW("false"), ESearchCase::IgnoreCase) ||
		Text.Equals(TEXTVIEW("f"), ESearchCase::IgnoreCase) ||
		Text.Equals(TEXTVIEW("off"), ESearchCase::IgnoreCase) ||
		Text.Equals(TEXTVIEW("0"), ESearchCase::IgnoreCase))
	{
		OutValue = EFeatureEnabledReadWrite::NeverWriteNeverRead;
		return;
	}
	if (Text.Equals(TEXTVIEW("true"), ESearchCase::IgnoreCase) ||
		Text.Equals(TEXTVIEW("t"), ESearchCase::IgnoreCase) ||
		Text.Equals(TEXTVIEW("on"), ESearchCase::IgnoreCase))
	{
		OutValue = EFeatureEnabledReadWrite::DefaultWriteDefaultRead;
		return;
	}
	uint32 IntValue = 0;
	LexFromString(IntValue, Text);
	if (IntValue != 0)
	{
		OutValue = EFeatureEnabledReadWrite::DefaultWriteDefaultRead;
		return;
	}
	OutValue = EFeatureEnabledReadWrite::Invalid;
	return;
}

void FPreloadSettings::Initialize()
{
	if (bInitialized)
	{
		return;
	}
	bInitialized = true;
	if (!FParse::Value(FCommandLine::Get(), TEXT("AssetRegistryCacheRootFolder="), AssetRegistryCacheRootFolder))
	{
		AssetRegistryCacheRootFolder = FPaths::ProjectIntermediateDir();
	}
	bForceDependsGathering = FParse::Param(FCommandLine::Get(), TEXT("ForceDependsGathering"));
	bGatherDependsData = (GIsEditor && !FParse::Param(FCommandLine::Get(), TEXT("NoDependsGathering"))) || bForceDependsGathering;
	bool bNoAssetRegistryCache = FParse::Param(FCommandLine::Get(), TEXT("NoAssetRegistryCache"));
	bool bNoAssetRegistryDiscoveryCache = bNoAssetRegistryCache || FParse::Param(FCommandLine::Get(), TEXT("NoAssetRegistryDiscoveryCache"));
	bool bNoAssetRegistryCacheRead = FParse::Param(FCommandLine::Get(), TEXT("NoAssetRegistryCacheRead"));
	uint32 MultiprocessId = UE::GetMultiprocessId();
	bool bMultiprocess = MultiprocessId > 0 || FParse::Param(FCommandLine::Get(), TEXT("multiprocess"));
	bool bNoAssetRegistryCacheWrite = FParse::Param(FCommandLine::Get(), TEXT("NoAssetRegistryCacheWrite")) || bMultiprocess;
	bGatherCacheReadEnabled = !bNoAssetRegistryCache && !bNoAssetRegistryCacheRead;
	bGatherCacheWriteEnabled = !bNoAssetRegistryCache && !bNoAssetRegistryCacheWrite;
	bool bPlatformSupportsDiscoveryCacheInvalidation = FPlatformFileManager::Get().GetPlatformFile().FileJournalIsAvailable();

	bool bSkipInvalidate = FParse::Param(FCommandLine::Get(), TEXT("AssetRegistryCacheSkipInvalidate"));
	EFeatureEnabledReadWrite DiscoverySetting = EFeatureEnabledReadWrite::Invalid;
	FString AssetRegistryDiscoveryCacheStr = TEXT("Default");
	GConfig->GetString(TEXT("AssetRegistry"), TEXT("AssetRegistryDiscoveryCache"),
		AssetRegistryDiscoveryCacheStr, GEngineIni);
	FParse::Value(FCommandLine::Get(), TEXT("AssetRegistryDiscoveryCache="), AssetRegistryDiscoveryCacheStr);
	LexFromString(DiscoverySetting, AssetRegistryDiscoveryCacheStr);
	if (DiscoverySetting == EFeatureEnabledReadWrite::Invalid)
	{
		UE_LOG(LogAssetRegistry, Error,
			TEXT("Invalid text \"%s\" for Engine.ini:[AssetRegistry]:AssetRegistryDiscoveryCache. Expected \"Never\", \"Default\", or \"AlwaysWrite\"."),
			*AssetRegistryDiscoveryCacheStr);
		DiscoverySetting = EFeatureEnabledReadWrite::DefaultWriteDefaultRead;
	}
	if (bNoAssetRegistryDiscoveryCache)
	{
		DiscoverySetting = EFeatureEnabledReadWrite::NeverWriteNeverRead;
	}
	if (bNoAssetRegistryCacheRead)
	{
		DiscoverySetting = (DiscoverySetting & ~EFeatureEnabledReadWrite::ReadMask) | EFeatureEnabledReadWrite::NeverRead;
	}
	if (bNoAssetRegistryCacheWrite)
	{
		DiscoverySetting = (DiscoverySetting & ~EFeatureEnabledReadWrite::WriteMask) | EFeatureEnabledReadWrite::NeverWrite;
	}
	if (bSkipInvalidate || MultiprocessId != 0)
	{
		if ((DiscoverySetting & EFeatureEnabledReadWrite::WriteMask) == EFeatureEnabledReadWrite::DefaultWrite)
		{
			DiscoverySetting = (DiscoverySetting & ~EFeatureEnabledReadWrite::WriteMask) | EFeatureEnabledReadWrite::AlwaysWrite;
		}
		if ((DiscoverySetting & EFeatureEnabledReadWrite::ReadMask) == EFeatureEnabledReadWrite::DefaultRead)
		{
			DiscoverySetting = (DiscoverySetting & ~EFeatureEnabledReadWrite::ReadMask) | EFeatureEnabledReadWrite::AlwaysRead;
		}
	}
	else if (!bPlatformSupportsDiscoveryCacheInvalidation)
	{
		// Precalculate Default -> Never if we already know the platform doesn't support it
		if ((DiscoverySetting & EFeatureEnabledReadWrite::WriteMask) == EFeatureEnabledReadWrite::DefaultWrite)
		{
			DiscoverySetting = (DiscoverySetting& ~EFeatureEnabledReadWrite::WriteMask) | EFeatureEnabledReadWrite::NeverWrite;
		}
		if ((DiscoverySetting & EFeatureEnabledReadWrite::ReadMask) == EFeatureEnabledReadWrite::DefaultRead)
		{
			DiscoverySetting = (DiscoverySetting& ~EFeatureEnabledReadWrite::ReadMask) | EFeatureEnabledReadWrite::NeverRead;
		}
	}
	bDiscoveryCacheReadEnabled = (DiscoverySetting & EFeatureEnabledReadWrite::ReadMask) != EFeatureEnabledReadWrite::NeverRead;
	bDiscoveryCacheInvalidateEnabled = (DiscoverySetting & EFeatureEnabledReadWrite::ReadMask) != EFeatureEnabledReadWrite::AlwaysRead;
	switch (DiscoverySetting & EFeatureEnabledReadWrite::WriteMask)
	{
	case EFeatureEnabledReadWrite::NeverWrite:
		DiscoveryCacheWriteEnabled = EFeatureEnabled::Never;
		break;
	case EFeatureEnabledReadWrite::DefaultWrite:
		DiscoveryCacheWriteEnabled = EFeatureEnabled::IfPlatformSupported;
		break;
	case EFeatureEnabledReadWrite::AlwaysWrite:
		DiscoveryCacheWriteEnabled = EFeatureEnabled::Always;
		break;
	default:
		checkNoEntry();
		DiscoveryCacheWriteEnabled = EFeatureEnabled::Never;
		break;
	}

	bool bAsyncEnabled = FPlatformProcess::SupportsMultithreading() && FTaskGraphInterface::IsRunning();

	MonolithicCacheBaseFilename = AssetRegistryCacheRootFolder / (bGatherDependsData ? TEXT("CachedAssetRegistry") : TEXT("CachedAssetRegistryNoDeps"));
#if UE_EDITOR // See note on FPreloader for why we only allow preloading if UE_EDITOR
	bMonolithicCacheActivatedDuringPreload = bAsyncEnabled && UE::AssetRegistry::ShouldSearchAllAssetsAtStart();
#else
	bMonolithicCacheActivatedDuringPreload = false;
#endif
}
bool FPreloadSettings::IsGatherCacheReadEnabled() const
{
	return bGatherCacheReadEnabled;
}
bool FPreloadSettings::IsGatherCacheWriteEnabled() const
{
	return bGatherCacheWriteEnabled;
}
bool FPreloadSettings::IsDiscoveryCacheReadEnabled() const
{
	return bDiscoveryCacheReadEnabled;
}
EFeatureEnabled FPreloadSettings::IsDiscoveryCacheWriteEnabled() const
{
	return DiscoveryCacheWriteEnabled;
}
bool FPreloadSettings::IsDiscoveryCacheInvalidateEnabled() const
{
	return bDiscoveryCacheInvalidateEnabled;
}
bool FPreloadSettings::IsMonolithicCacheActivatedDuringPreload() const
{
	return bMonolithicCacheActivatedDuringPreload;
}
bool FPreloadSettings::IsPreloadMonolithicCache() const
{
	return bGatherCacheReadEnabled && bMonolithicCacheActivatedDuringPreload;
}
bool FPreloadSettings::IsGatherDependsData() const
{
	return bGatherDependsData;
}
bool FPreloadSettings::IsForceDependsGathering() const
{
	return bForceDependsGathering;
}
FString FPreloadSettings::GetLegacyMonolithicCacheFilename() const
{
	return MonolithicCacheBaseFilename + TEXT(".bin");
}
const FString& FPreloadSettings::GetMonolithicCacheBaseFilename() const
{
	return MonolithicCacheBaseFilename;
}
const FString& FPreloadSettings::GetAssetRegistryCacheRootFolder() const
{
	return AssetRegistryCacheRootFolder;
}

TArray<FString> FPreloadSettings::FindShardedMonolithicCacheFiles() const
{
	TArray<FString> CachePaths;
	IFileManager::Get().FindFiles(CachePaths, *(GetMonolithicCacheBaseFilename() + TEXT("_*.bin")), /* Files */ true, /* Directories */ false);
	if (CachePaths.Num())
	{
		FString Directory = FPaths::GetPath(GetMonolithicCacheBaseFilename());
		for (FString& Path : CachePaths)
		{
			Path = Directory / Path;
		}
	}
	return CachePaths;
}

FPreloadSettings GPreloadSettings;

bool IsPackageBlocked(const FStringView& FilePath)
{
	return bBlockPackagesWithMarkOfTheWeb && IPlatformFile::GetPlatformPhysical().HasMarkOfTheWeb(*FString(FilePath));
}

/** A structure to hold serialized cache data from async loads before adding it to the Gatherer's main cache. */
struct FCachePayload
{
	TUniquePtr<FName[]> PackageNames;
	TUniquePtr<FDiskCachedAssetData[]> AssetDatas;
	int32 NumAssets = 0;
	bool bSucceeded = false;
	void Reset()
	{
		PackageNames.Reset();
		AssetDatas.Reset();
		NumAssets = 0;
		bSucceeded = false;
	}
};

void SerializeCacheSave(FAssetRegistryWriter& Ar, const TArray<TPair<FName, FDiskCachedAssetData*>>& AssetsToSave);
FCachePayload SerializeCacheLoad(FAssetRegistryReader& Ar);
TArray<FCachePayload> LoadCacheFiles(TConstArrayView<FString> CacheFilenames, bool bIsMonolithicCache);


/** InOutResult = Value, but without shrinking the string to fit. */
void AssignStringWithoutShrinking(FString& InOutResult, FStringView Value)
{
	TArray<TCHAR, FString::AllocatorType>& Result = InOutResult.GetCharArray();
	if (Value.IsEmpty())
	{
		Result.Reset();
	}
	else
	{
		Result.SetNumUninitialized(Value.Len() + 1, EAllowShrinking::No);
		FMemory::Memcpy(Result.GetData(), Value.GetData(), Value.Len() * sizeof(Value[0]));
		Result[Value.Len()] = '\0';
	}
}

FDiscoveredPathData::FDiscoveredPathData(FStringView InLocalAbsPath, FStringView InLongPackageName, FStringView InRelPath, const FDateTime& InPackageTimestamp, EGatherableFileType InType, bool bInBlocked)
	: LocalAbsPath(InLocalAbsPath)
	, LongPackageName(InLongPackageName)
	, RelPath(InRelPath)
	, PackageTimestamp(InPackageTimestamp)
	, Type(InType)
	, bBlocked(bInBlocked)
{
}

FDiscoveredPathData::FDiscoveredPathData(FStringView InLocalAbsPath, FStringView InLongPackageName, FStringView InRelPath, EGatherableFileType InType, bool bInBlocked)
	: LocalAbsPath(InLocalAbsPath)
	, LongPackageName(InLongPackageName)
	, RelPath(InRelPath)
	, Type(InType)
	, bBlocked(bInBlocked)
{
}

void FDiscoveredPathData::Assign(FStringView InLocalAbsPath, FStringView InLongPackageName, FStringView InRelPath, EGatherableFileType InType, bool bInBlocked)
{
	AssignStringWithoutShrinking(LocalAbsPath, InLocalAbsPath);
	AssignStringWithoutShrinking(LongPackageName, InLongPackageName);
	AssignStringWithoutShrinking(RelPath, InRelPath);
	Type = InType;
	bBlocked = bInBlocked;
}

void FDiscoveredPathData::Assign(FStringView InLocalAbsPath, FStringView InLongPackageName, FStringView InRelPath, const FDateTime& InPackageTimestamp, EGatherableFileType InType, bool bInBlocked)
{
	Assign(InLocalAbsPath, InLongPackageName, InRelPath, InType, bInBlocked);
	PackageTimestamp = InPackageTimestamp;
}

SIZE_T FDiscoveredPathData::GetAllocatedSize() const
{
	return LocalAbsPath.GetAllocatedSize() + LongPackageName.GetAllocatedSize() + RelPath.GetAllocatedSize();
}

FGatheredPathData::FGatheredPathData(FStringView InLocalAbsPath, FStringView InLongPackageName, const FDateTime& InPackageTimestamp, EGatherableFileType InType, bool bInBlocked)
	: LocalAbsPath(InLocalAbsPath)
	, LongPackageName(InLongPackageName)
	, PackageTimestamp(InPackageTimestamp)
	, Type(InType)
	, bBlocked(bInBlocked)
{
}

FGatheredPathData::FGatheredPathData(const FDiscoveredPathData& DiscoveredData)
	: FGatheredPathData(DiscoveredData.LocalAbsPath, DiscoveredData.LongPackageName, DiscoveredData.PackageTimestamp, DiscoveredData.Type, DiscoveredData.bBlocked)
{
}

FGatheredPathData::FGatheredPathData(FDiscoveredPathData&& DiscoveredData)
	: LocalAbsPath(MoveTemp(DiscoveredData.LocalAbsPath))
	, LongPackageName(MoveTemp(DiscoveredData.LongPackageName))
	, PackageTimestamp(MoveTemp(DiscoveredData.PackageTimestamp))
	, Type(DiscoveredData.Type)
	, bBlocked(DiscoveredData.bBlocked)
{
}

void FGatheredPathData::Assign(FStringView InLocalAbsPath, FStringView InLongPackageName, const FDateTime& InPackageTimestamp, EGatherableFileType InType, bool bInBlocked)
{
	AssignStringWithoutShrinking(LocalAbsPath, InLocalAbsPath);
	AssignStringWithoutShrinking(LongPackageName, InLongPackageName);
	PackageTimestamp = InPackageTimestamp;
	Type = InType;
	bBlocked = bInBlocked;
}

void FGatheredPathData::Assign(const FDiscoveredPathData& DiscoveredData)
{
	Assign(DiscoveredData.LocalAbsPath, DiscoveredData.LongPackageName, DiscoveredData.PackageTimestamp, DiscoveredData.Type, DiscoveredData.bBlocked);
}

SIZE_T FGatheredPathData::GetAllocatedSize() const
{
	return LocalAbsPath.GetAllocatedSize() + LongPackageName.GetAllocatedSize();
}

FScanDir::FScanDir(FMountDir& InMountDir, FScanDir* InParent, FStringView InRelPath)
	: MountDir(&InMountDir)
	, Parent(InParent)
	, RelPath(InRelPath)
{
	FAssetDataDiscovery& Discovery = InMountDir.GetDiscovery();
	Discovery.NumDirectoriesToScan.Increment();
	UE_CLOG(Discovery.bIsIdle, LogAssetRegistry, Warning, TEXT("AssetDataGatherer: FScanDir is constructed while bIsIdle=true."));
}

FScanDir::~FScanDir()
{
	// Assert that Shutdown has been called to confirm that the parent no longer has a reference we need to clear.
	check(!MountDir);
}

void FScanDir::Shutdown()
{
	if (!MountDir)
	{
		// Already shutdown
		return;
	}

	// Shutdown all children
	for (TRefCountPtr<FScanDir>& ScanDir : SubDirs)
	{
		// Destruction contract for FScanDir requires that the parent calls Shutdown before dropping the reference
		ScanDir->Shutdown();
		ScanDir.SafeRelease();
	}
	SubDirs.Empty();

	// Update MountDir data that we influence
	if (!bIsComplete)
	{
		MountDir->GetDiscovery().NumDirectoriesToScan.Decrement();
	}

	// Clear backpointers (which also marks us as shutdown)
	MountDir = nullptr;
	Parent = nullptr;
}

bool FScanDir::IsValid() const
{
	return MountDir != nullptr;
}

FMountDir* FScanDir::GetMountDir() const
{
	return MountDir;
}

FStringView FScanDir::GetRelPath() const
{
	return RelPath;
}

void FScanDir::AppendLocalAbsPath(FStringBuilderBase& OutFullPath) const
{
	if (!MountDir)
	{
		return;
	}

	if (Parent)
	{
		Parent->AppendLocalAbsPath(OutFullPath);
		FPathViews::AppendPath(OutFullPath, RelPath);
	}
	else
	{
		// The root ScanDir should have an empty RelPath from the MountDir
		check(RelPath.IsEmpty());
		OutFullPath << MountDir->GetLocalAbsPath();
	}
}

FString FScanDir::GetLocalAbsPath() const
{
	TStringBuilder<128> Result;
	AppendLocalAbsPath(Result);
	return FString(Result);
}

void FScanDir::AppendMountRelPath(FStringBuilderBase& OutRelPath) const
{
	if (!MountDir)
	{
		return;
	}

	if (Parent)
	{
		Parent->AppendMountRelPath(OutRelPath);
		FPathViews::AppendPath(OutRelPath, RelPath);
	}
	else
	{
		// The root ScanDir should have an empty RelPath from the MountDir
		check(RelPath.IsEmpty());
	}
}

FString FScanDir::GetMountRelPath() const
{
	TStringBuilder<128> Result;
	AppendMountRelPath(Result);
	return FString(Result);
}

bool FScanDir::FInherited::IsMonitored() const
{
	return IsOnAllowList() && !IsOnDenyList();
}

bool FScanDir::FInherited::IsOnDenyList() const
{
	return bMatchesDenyList && !bIgnoreDenyList;
}

bool FScanDir::FInherited::IsOnAllowList() const
{
	return bIsOnAllowList;
}

bool FScanDir::FInherited::HasSetting() const
{
	return bIsOnAllowList || bMatchesDenyList || bIgnoreDenyList;
}

FScanDir::FInherited::FInherited(const FInherited& Parent, const FInherited& Child)
	: bIsOnAllowList(Parent.bIsOnAllowList || Child.bIsOnAllowList)
	, bMatchesDenyList(Parent.bMatchesDenyList || Child.bMatchesDenyList)
	, bIgnoreDenyList(Parent.bIgnoreDenyList || Child.bIgnoreDenyList)
{
}

void FScanDir::GetMonitorData(FStringView InRelPath, const FInherited& ParentData, FInherited& OutData) const
{
	if (!MountDir)
	{
		OutData = FInherited();
		return;
	}

	FInherited Accumulated(ParentData, DirectData);

	const FScanDir* SubDir = nullptr;
	FStringView FirstComponent;
	FStringView RemainingPath;
	if (!InRelPath.IsEmpty())
	{
		FPathViews::SplitFirstComponent(InRelPath, FirstComponent, RemainingPath);
		SubDir = FindSubDir(FirstComponent);
	}
	if (!SubDir)
	{
		OutData = Accumulated;
	}
	else
	{
		SubDir->GetMonitorData(RemainingPath, Accumulated, OutData);
	}
}

bool FScanDir::IsMonitored(const FInherited& ParentData) const
{
	if (!MountDir)
	{
		return false;
	}
	FInherited Accumulated(ParentData, DirectData);
	return Accumulated.IsMonitored();
}

bool FScanDir::ShouldScan(const FInherited& ParentData) const
{
	return !bHasScanned && IsMonitored(ParentData);
}

bool FScanDir::HasScanned() const
{
	return bHasScanned;
}

bool FScanDir::IsComplete() const
{
	return bIsComplete;
}

SIZE_T FScanDir::GetAllocatedSize() const
{
	SIZE_T Result = 0;
	Result += SubDirs.GetAllocatedSize();
	for (const TRefCountPtr<FScanDir>& Value : SubDirs)
	{
		Result += sizeof(*Value);
		Result += Value->GetAllocatedSize();
	}
	Result += AlreadyScannedFiles.GetAllocatedSize();
	for (const FString& Value : AlreadyScannedFiles)
	{
		Result += Value.GetAllocatedSize();
	}
	Result += RelPath.GetAllocatedSize();
	return Result;
}

FScanDir* FScanDir::GetControllingDir(FStringView InRelPath, bool bIsDirectory, const FInherited& ParentData, FInherited& OutData, FString& OutRelPath)
{
	// GetControllingDir can only be called on valid ScanDirs, which we rely on since we need to call FindOrAddSubDir which relies on that
	check(IsValid());

	FInherited Accumulated(ParentData, DirectData);
	if (InRelPath.IsEmpty())
	{
		if (!bIsDirectory)
		{
			UE_LOG(LogAssetRegistry, Warning, TEXT("GetControllingDir called on %s with !bIsDirectory, but we have it recorded as a directory. Returning null."), *GetLocalAbsPath());
			OutData = FInherited();
			OutRelPath.Reset();
			return nullptr;
		}
		else
		{
			OutData = Accumulated;
			OutRelPath = InRelPath;
			return this;
		}
	}

	FStringView FirstComponent;
	FStringView RemainingPath;
	FPathViews::SplitFirstComponent(InRelPath, FirstComponent, RemainingPath);
	if (RemainingPath.IsEmpty() && !bIsDirectory)
	{
		OutData = Accumulated;
		OutRelPath = InRelPath;
		return this;
	}
	else
	{
		FScanDir* SubDir = nullptr;
		if (ShouldScan(ParentData))
		{
			SubDir = &FindOrAddSubDir(FirstComponent);
		}
		else
		{
			SubDir = FindSubDir(FirstComponent);
			if (!SubDir)
			{
				OutData = Accumulated;
				OutRelPath = InRelPath;
				return this;
			}
		}
		return SubDir->GetControllingDir(RemainingPath, bIsDirectory, Accumulated, OutData, OutRelPath);
	}
}

bool FScanDir::TrySetDirectoryProperties(FStringView InRelPath, FInherited& ParentData,
	const FSetPathProperties& InProperties, bool bConfirmedExists, FScanDirAndParentData& OutControllingDir)
{
	// TrySetDirectoryProperties can only be called on valid ScanDirs, which we rely on so we can call FindOrAddSubDir which requires that
	check(IsValid()); 

	if (InRelPath.IsEmpty())
	{
		// The properties apply to this entire directory
		if (InProperties.IsOnAllowList.IsSet() && DirectData.bIsOnAllowList != *InProperties.IsOnAllowList)
		{
			SetComplete(false);
			if (bScanInFlight)
			{
				bScanInFlightInvalidated = true;
			}
			DirectData.bIsOnAllowList = *InProperties.IsOnAllowList;

			if (DirectData.bIsOnAllowList)
			{
				// Since we are setting this directory to be monitored, we need to implement the guarantee that all Monitored flags of its children are set to false
				// We also need to SetComplete false on all directories in between this and a previously allow listed directory, since those non-allow listed parent directories
				// marked themselves complete once their allow listed children finished
				ForEachDescendent([](FScanDir& ScanDir)
					{
						ScanDir.DirectData.bIsOnAllowList = false;
						ScanDir.SetComplete(false);
					});
			}
			else
			{
				// Cancel any scans since they are no longer allow listed
				ForEachDescendent([](FScanDir& ScanDir)
					{
						if (ScanDir.bScanInFlight)
						{
							ScanDir.bScanInFlightInvalidated = true;
						}
					});
			}
		}
		if ((InProperties.MatchesDenyList.IsSet() && DirectData.bMatchesDenyList != *InProperties.MatchesDenyList) ||
			(InProperties.IgnoreDenyList.IsSet() && DirectData.bIgnoreDenyList != *InProperties.IgnoreDenyList))
		{
			SetComplete(false);
			if (InProperties.MatchesDenyList.IsSet())
			{
				DirectData.bMatchesDenyList = *InProperties.MatchesDenyList;
			}
			if (InProperties.IgnoreDenyList.IsSet())
			{
				DirectData.bIgnoreDenyList = *InProperties.IgnoreDenyList;
			}
			bool bIgnoreDenyList = false;
			bool bMatchesDenyList = false;
			for (FScanDir* Current = this; Current; Current = Current->Parent)
			{
				bIgnoreDenyList = bIgnoreDenyList || Current->DirectData.bIgnoreDenyList;
				bMatchesDenyList = bMatchesDenyList || Current->DirectData.bMatchesDenyList;
			}
			bool bIsOnDenyList = bMatchesDenyList && !bIgnoreDenyList;

			// Mark all children as incomplete
			// Also cancel any scans since they are now potentially on the deny list
			if (bIsOnDenyList && bScanInFlight)
			{
				bScanInFlightInvalidated = true;
			}
			ForEachDescendent([&bIsOnDenyList](FScanDir& ScanDir)
				{
					if (bIsOnDenyList && ScanDir.bScanInFlight)
					{
						ScanDir.bScanInFlightInvalidated = true;
					}
					ScanDir.SetComplete(false);
				});
		}
		if (InProperties.HasScanned.IsSet())
		{
			SetComplete(false);
			bool bNewValue = *InProperties.HasScanned;
			auto SetProperties = [bNewValue](FScanDir& ScanDir)
			{
				if (ScanDir.bScanInFlight)
				{
					ScanDir.bScanInFlightInvalidated = true;
				}
				ScanDir.bHasScanned = bNewValue;
				ScanDir.AlreadyScannedFiles.Reset();
			};
			SetProperties(*this);
			ForEachDescendent(SetProperties);
		}

		OutControllingDir.ScanDir = this;
		OutControllingDir.ParentData = ParentData;
		return true;
	}
	else
	{
		TOptional<FSetPathProperties> ModifiedProperties;
		const FSetPathProperties* Properties = &InProperties;
		if (Properties->IsOnAllowList.IsSet() && DirectData.bIsOnAllowList)
		{
			// If this directory is set to be monitored, all Monitored flags of its children are unused, are guaranteed set to false, and should not be changed
			ModifiedProperties = *Properties;
			ModifiedProperties->IsOnAllowList.Reset();
			if (!ModifiedProperties->IsSet())
			{
				OutControllingDir.ScanDir = this;
				OutControllingDir.ParentData = ParentData;
				return false;
			}
			Properties = &ModifiedProperties.GetValue();
		}

		FStringView FirstComponent;
		FStringView Remainder;
		FPathViews::SplitFirstComponent(InRelPath, FirstComponent, Remainder);

		FScanDir* SubDir = nullptr;
		if (bHasScanned &&
			(!Properties->HasScanned.IsSet() || *Properties->HasScanned == true) &&
			(!Properties->IsOnAllowList.IsSet()) &&
			(!Properties->IgnoreDenyList.IsSet()) &&
			(!Properties->MatchesDenyList.IsSet()))
		{
			// If this parent directory has already been scanned and we are not changing the target directory's has-been-scanned value,
			// and the next child subdirectory does not exist, then the child directory has already been scanned and we do not need to set the properties on it.
			// Therefore call FindSubDir instead of FindOrAddSubDir, and abort if we do not find it
			SubDir = FindSubDir(FirstComponent);
			if (!SubDir)
			{
				OutControllingDir.ScanDir = this;
				OutControllingDir.ParentData = ParentData;
				return false;
			}
		}
		else
		{
			SubDir = FindSubDir(FirstComponent);
			if (!SubDir)
			{
				if (!bConfirmedExists)
				{
					TStringBuilder<256> LocalAbsPath;
					AppendLocalAbsPath(LocalAbsPath);
					FPathViews::AppendPath(LocalAbsPath, InRelPath);
					FFileStatData StatData = IFileManager::Get().GetStatData(LocalAbsPath.ToString());
					if (!StatData.bIsValid || !StatData.bIsDirectory)
					{
						UE_LOG(LogAssetRegistry, Warning, TEXT("SetDirectoryProperties called on %s path %.*s. Ignoring the call."),
							StatData.bIsValid ? TEXT("file") : TEXT("non-existent"), LocalAbsPath.Len(), LocalAbsPath.GetData());
						OutControllingDir.ScanDir = this;
						OutControllingDir.ParentData = ParentData;
						return false;
					}
					bConfirmedExists = true;
				}
				SubDir = &FindOrAddSubDir(FirstComponent);
				// If the current directory has already been scanned then the SubDir we just created must have been
				// previously discovered, or it was created on disk after the last time we scanned the current
				// directory. If it was created on disk, and it is not being force rescanned
				// (aka Properties->HasScanned=true) then we are allowed to ignore it. If it was previously discovered,
				// then we completed and deleted it: either it was not IsMonitored, or we scanned it. To avoid an
				// unnecessary rescan, we should therefore set bHasScanned=true if the current directory has
				// bHasScanned=true and the SubDir has IsMonitored=true. If force rescan is requested on the SubDir,
				// then we will set it back to bHasScanned=false in TrySetDirectoryProperties below.
				if (this->bHasScanned)
				{
					FInherited ParentDataForSubDir(ParentData, DirectData);
					if (SubDir->IsMonitored(ParentDataForSubDir))
					{
						SubDir->bHasScanned = true;
					}
				}
				SetComplete(false);
			}
		}

		FInherited ParentDataForSubDir(ParentData, DirectData);
		bool bResult = SubDir->TrySetDirectoryProperties(Remainder, ParentDataForSubDir, *Properties, bConfirmedExists,
			OutControllingDir);
		if (OutControllingDir.ScanDir && !OutControllingDir.ScanDir->IsComplete())
		{
			SetComplete(false);
		}
		return bResult;
	}
}

void FScanDir::MarkFileAlreadyScanned(FStringView BaseName)
{
	if (bHasScanned)
	{
		return;
	}
	check(FPathViews::IsPathLeaf(BaseName));
	for (const FString& AlreadyScannedFile : AlreadyScannedFiles)
	{
		if (FStringView(AlreadyScannedFile).Equals(BaseName, ESearchCase::IgnoreCase))
		{
			return;
		}
	}
	AlreadyScannedFiles.Emplace(BaseName);
}

void FScanDir::SetScanResults(FStringView LocalAbsPath, const FInherited& ParentData, TArrayView<FDiscoveredPathData>& InOutSubDirs, TArrayView<FDiscoveredPathData>& InOutFiles)
{
	SetComplete(false);
	check(!bScanInFlightInvalidated);
	check(MountDir);

	if (!ensure(!bHasScanned))
	{
		return;
	}
	FInherited Accumulated(ParentData, DirectData);

	// Add SubDirectories in the tree for the directories found by the scan, and report the directories as discovered directory paths as well
	for (int32 Index = 0; Index < InOutSubDirs.Num(); )
	{
		FDiscoveredPathData& SubDirPath = InOutSubDirs[Index];
		FScanDir& SubScanDir = FindOrAddSubDir(SubDirPath.RelPath);
		bool bReportResult = SubScanDir.IsMonitored(Accumulated) && MountDir->GetDiscovery().ShouldDirBeReported(SubDirPath.LongPackageName);
		if (!bReportResult)
		{
			Swap(SubDirPath, InOutSubDirs.Last());
			InOutSubDirs = InOutSubDirs.Slice(0, InOutSubDirs.Num() - 1);
		}
		else
		{
			++Index;
		}
	}

	// Add the files that were found in the scan, skipping any files that have already been scanned
	if (InOutFiles.Num())
	{
		auto IsAlreadyScanned = [this, &LocalAbsPath](const FDiscoveredPathData& InFile)
		{
			return Algo::AnyOf(AlreadyScannedFiles, [&InFile](const FString& AlreadyScannedFileRelPath) { return FPathViews::Equals(AlreadyScannedFileRelPath, InFile.RelPath); });
		};
		bool bScanAll = AlreadyScannedFiles.Num() == 0;
		for (int32 Index = 0; Index < InOutFiles.Num(); )
		{
			FDiscoveredPathData& InFile = InOutFiles[Index];
			if (!bScanAll && IsAlreadyScanned(InFile))
			{
				// Remove this file from InOutFiles
				Swap(InFile, InOutFiles.Last());
				InOutFiles = InOutFiles.Slice(0, InOutFiles.Num() - 1);
			}
			else
			{
				++Index;
			}
		}
	}
	AlreadyScannedFiles.Empty();

	MountDir->SetHasStartedScanning();
	bHasScanned = true;
}

void FScanDir::Update(TArray<FScanDirAndParentData>& OutScanRequests, const FScanDir::FInherited& ParentData)
{
	check(MountDir);
	if (bIsComplete)
	{
		return;
	}

	bool bScanThis = ShouldScan(ParentData);
	if (bScanThis)
	{
		OutScanRequests.Add(FScanDirAndParentData{ this, ParentData });
	}

	bool bAllSubDirsComplete = true;
	if (SubDirs.Num())
	{
		FInherited ParentDataForSubDirs(ParentData, DirectData);
		TArray<TRefCountPtr<FScanDir>> CopySubDirs = SubDirs;
		for (const TRefCountPtr<FScanDir>& SubDir : CopySubDirs)
		{
			if (SubDir->bIsComplete)
			{
				continue;
			}
			int32 PreviousCount = OutScanRequests.Num();
			SubDir->Update(OutScanRequests, ParentDataForSubDirs);
			bool bSubDirComplete = SubDir->IsComplete();
			check(OutScanRequests.Num() > PreviousCount || bSubDirComplete);
			bAllSubDirsComplete &= bSubDirComplete;
		}
	}

	if (bScanThis || !bAllSubDirsComplete)
	{
		return;
	}

	SetComplete(true);
	// After calling SetComplete, this may have been removed from tree and should no longer run calculations
}

FScanDir* FScanDir::GetFirstIncompleteScanDir()
{
	for (const TRefCountPtr<FScanDir>& SubDir : SubDirs)
	{
		FScanDir* Result = SubDir->GetFirstIncompleteScanDir();
		if (Result)
		{
			return Result;
		}
	}
	if (!bIsComplete)
	{
		return this;
	}
	return nullptr;
}

bool FScanDir::IsScanInFlight() const
{
	return bScanInFlight;
}

void FScanDir::SetScanInFlight(bool bInScanInFlight)
{
	bScanInFlight = bInScanInFlight;
}

bool FScanDir::IsScanInFlightInvalidated() const
{
	return bScanInFlightInvalidated;
}

void FScanDir::SetScanInFlightInvalidated(bool bInvalidated)
{
	bScanInFlightInvalidated = bInvalidated;
}

void FScanDir::MarkDirty(bool bMarkDescendents)
{
	if (bMarkDescendents)
	{
		ForEachDescendent([](FScanDir& Descendent) { Descendent.SetComplete(false); });
	}
	FScanDir* Current = this;
	while (Current)
	{
		Current->SetComplete(false);
		Current = Current->Parent;
	}
}

void FScanDir::Shrink()
{
	ForEachSubDir([](FScanDir& SubDir) {SubDir.Shrink(); });
	SubDirs.Shrink();
	AlreadyScannedFiles.Shrink();
}


void FScanDir::SetComplete(bool bInIsComplete)
{
	if (!MountDir || bIsComplete == bInIsComplete)
	{
		return;
	}

	bIsComplete = bInIsComplete;
	if (bIsComplete)
	{
		MountDir->GetDiscovery().NumDirectoriesToScan.Decrement();
		// Upon completion, subdirs that do not need to be maintained are deleted, which is done by removing them from the parent.
		// ScanDirs need to be maintained if they are the root, or have persistent settings, or have child ScanDirs that need to be maintained,
		// or the parent scan has not been done yet.
		if ((Parent != nullptr && Parent->bHasScanned) && !HasPersistentSettings() && SubDirs.IsEmpty())
		{
			Parent->RemoveSubDir(GetRelPath());
			// *this is Shutdown (e.g. Parent is now null) and it may also have been deallocated
			return;
		}
	}
	else
	{
		FAssetDataDiscovery& Discovery = MountDir->GetDiscovery();
		Discovery.NumDirectoriesToScan.Increment();
		UE_CLOG(Discovery.bIsIdle, LogAssetRegistry, Warning, TEXT("AssetDataGatherer: SetComplete(false) is called while bIsIdle=true."));
	}
}

bool FScanDir::HasPersistentSettings() const
{
	return DirectData.HasSetting();
}

FScanDir* FScanDir::FindSubDir(FStringView SubDirBaseName)
{
	int32 Index = FindLowerBoundSubDir(SubDirBaseName);
	if (Index == SubDirs.Num() || !FPathViews::Equals(SubDirs[Index]->GetRelPath(), SubDirBaseName))
	{
		return nullptr;
	}
	else
	{
		return SubDirs[Index].GetReference();
	}
}

const FScanDir* FScanDir::FindSubDir(FStringView SubDirBaseName) const
{
	return const_cast<FScanDir*>(this)->FindSubDir(SubDirBaseName);
}

FScanDir& FScanDir::FindOrAddSubDir(FStringView SubDirBaseName)
{
	// FindOrAddSubDir is only allowed to be called on valid FScanDirs, which we rely on since we need a non-null MountDir which valid ScanDirs have
	check(MountDir != nullptr);

	int32 Index = FindLowerBoundSubDir(SubDirBaseName);
	if (Index == SubDirs.Num() || !FPathViews::Equals(SubDirs[Index]->GetRelPath(), SubDirBaseName))
	{
		return *SubDirs.EmplaceAt_GetRef(Index, new FScanDir(*MountDir, this, SubDirBaseName));
	}
	else
	{
		return *SubDirs[Index];
	}
}

void FScanDir::RemoveSubDir(FStringView SubDirBaseName)
{
	int32 Index = FindLowerBoundSubDir(SubDirBaseName);
	if (Index < SubDirs.Num() && FPathViews::Equals(SubDirs[Index]->GetRelPath(), SubDirBaseName))
	{
		// Destruction contract for FScanDir requires that the parent calls Shutdown before dropping the reference
		SubDirs[Index]->Shutdown();
		SubDirs.RemoveAt(Index);
	}
}

int32 FScanDir::FindLowerBoundSubDir(FStringView SubDirBaseName)
{
	return Algo::LowerBound(SubDirs, SubDirBaseName,
		[](const TRefCountPtr<FScanDir>& SubDir, FStringView BaseName)
		{
			return FPathViews::Less(SubDir->GetRelPath(), BaseName);
		}
	);
}

template <typename CallbackType>
void FScanDir::ForEachSubDir(const CallbackType& Callback)
{
	for (TRefCountPtr<FScanDir>& Ptr : SubDirs)
	{
		Callback(*Ptr);
	}
}

/** Depth-first-search traversal of all descedent subdirs under this (not including this). Callback is called on parents before children. */
template <typename CallbackType>
void FScanDir::ForEachDescendent(const CallbackType& Callback)
{
	TArray<TPair<FScanDir*, int32>, TInlineAllocator<10>> Stack; // 10 chosen arbitrarily as a depth that is greater than most of our content root directory tree depths
	Stack.Add(TPair<FScanDir*, int32>(this, 0));
	while (Stack.Num())
	{
		TPair<FScanDir*, int32>& Top = Stack.Last();
		FScanDir* ParentOnStack = Top.Get<0>();
		int32& NextIndex = Top.Get<1>();
		if (NextIndex == ParentOnStack->SubDirs.Num())
		{
			Stack.SetNum(Stack.Num() - 1, EAllowShrinking::No);
			continue;
		}
		FScanDir* Child = ParentOnStack->SubDirs[NextIndex++];
		Callback(*Child);
		Stack.Add(TPair<FScanDir*, int32>(Child, 0));
	}
}

FMountDir::FMountDir(FAssetDataDiscovery& InDiscovery, FStringView InLocalAbsPath, FStringView InLongPackageName)
	: LocalAbsPath(InLocalAbsPath)
	, LongPackageName(InLongPackageName)
	, Discovery(InDiscovery)
{
	Root = new FScanDir(*this, nullptr, FStringView());
	UpdateDenyList();
}

FMountDir::~FMountDir()
{
	// ScanDir's destruction contract requires that the parent calls Shutdown on it before dropping the reference
	Root->Shutdown();
	Root.SafeRelease();
}

FStringView FMountDir::GetLocalAbsPath() const
{
	return LocalAbsPath;
}

FStringView FMountDir::GetLongPackageName() const
{
	return LongPackageName;
}

FAssetDataDiscovery& FMountDir::GetDiscovery() const
{
	return Discovery;
}

FScanDir* FMountDir::GetControllingDir(FStringView InLocalAbsPath, bool bIsDirectory, FScanDir::FInherited& OutData,
	FString& OutRelPath)
{
	FStringView RemainingPath;
	if (!FPathViews::TryMakeChildPathRelativeTo(InLocalAbsPath, GetLocalAbsPath(), RemainingPath))
	{
		return nullptr;
	}
	return Root->GetControllingDir(RemainingPath, bIsDirectory, FScanDir::FInherited() /* ParentData */,
		OutData, OutRelPath);
}

SIZE_T FMountDir::GetAllocatedSize() const
{
	SIZE_T Result = sizeof(*Root);
	Result += Root->GetAllocatedSize();
	Result += ChildMountPaths.GetAllocatedSize();
	for (const FString& Value : ChildMountPaths)
	{
		Result += Value.GetAllocatedSize();
	}
	Result += LongPackageName.GetAllocatedSize();
	Result += RelPathsDenyList.GetAllocatedSize();
	for (const FString& Value : RelPathsDenyList)
	{
		Result += Value.GetAllocatedSize();
	}
	return Result;
}

void FMountDir::Shrink()
{
	Root->Shrink();
	ChildMountPaths.Shrink();
	RelPathsDenyList.Shrink();
}

bool FMountDir::IsComplete() const
{
	return Root->IsComplete();
}

void FMountDir::GetMonitorData(FStringView InLocalAbsPath, FScanDir::FInherited& OutData) const
{
	FStringView QueryRelPath;
	if (!ensure(FPathViews::TryMakeChildPathRelativeTo(InLocalAbsPath, GetLocalAbsPath(), QueryRelPath)))
	{
		OutData = FScanDir::FInherited();
		return;
	}

	return Root->GetMonitorData(QueryRelPath, FScanDir::FInherited() /* ParentData */, OutData);
}

bool FMountDir::IsMonitored(FStringView InLocalAbsPath) const
{
	FScanDir::FInherited MonitorData;
	GetMonitorData(InLocalAbsPath, MonitorData);
	return MonitorData.IsMonitored();
}

bool FMountDir::TrySetDirectoryProperties(FStringView InLocalAbsPath, const FSetPathProperties& InProperties, bool bConfirmedExists,
	FScanDirAndParentData* OutControllingDir)
{
	FStringView RelPath;
	if (!ensure(FPathViews::TryMakeChildPathRelativeTo(InLocalAbsPath, GetLocalAbsPath(), RelPath)))
	{
		if (OutControllingDir)
		{
			OutControllingDir->ScanDir.SafeRelease();
		}
		return false;
	}
	if (InProperties.IgnoreDenyList.IsSet())
	{
		if (!ensure(!IsChildMountPath(RelPath)))
		{
			// Setting IgnoreDenyList on a child path would break behavior because we use MatchesDenyList to indicate that 
			// the scandir is a child path, and setting it to IgnoreDenyLists will defeat that setting.
			// This should never be called, because setting IgnoreDenyList is only called external to FAssetDataDiscovery, and 
			// FAssetDataDiscovery would call it on the child mount dir instead of this parent mountdir
			FSetPathProperties NewProperties(InProperties);
			NewProperties.IgnoreDenyList.Reset();
			if (!NewProperties.IsSet())
			{
				if (OutControllingDir)
				{
					OutControllingDir->ScanDir.SafeRelease();
				}
				return false;
			}
			return TrySetDirectoryProperties(InLocalAbsPath, NewProperties, bConfirmedExists, OutControllingDir);
		}
	}
	FScanDirAndParentData PlaceHolderOutControllingDir;
	if (!OutControllingDir)
	{
		OutControllingDir = &PlaceHolderOutControllingDir;
	}
	FScanDir::FInherited ParentData;
	return Root->TrySetDirectoryProperties(RelPath, ParentData, InProperties, bConfirmedExists, *OutControllingDir);
}

void FMountDir::UpdateDenyList()
{
	TSet<FString> RemovedDenyLists;
	for (const FString& Old : RelPathsDenyList)
	{
		RemovedDenyLists.Add(Old);
	}

	RelPathsDenyList.Empty(Discovery.MountRelativePathsDenyList.Num());
	for (const FString& DenyListEntry : Discovery.LongPackageNamesDenyList)
	{
		FStringView MountRelPath;
		if (FPathViews::TryMakeChildPathRelativeTo(DenyListEntry, LongPackageName, MountRelPath))
		{
			// Note that an empty RelPath means we deny the entire mountpoint
			RelPathsDenyList.Emplace(MountRelPath);
		}
	}
	for (const FString& MountRelPath : Discovery.MountRelativePathsDenyList)
	{
		RelPathsDenyList.Emplace(MountRelPath);
	}
	for (const FString& ChildPath : ChildMountPaths)
	{
		RelPathsDenyList.Emplace(ChildPath);
	}

	TSet<FString> AddedDenyListPaths;
	for (const FString& New : RelPathsDenyList)
	{
		if (!RemovedDenyLists.Remove(New))
		{
			AddedDenyListPaths.Add(New);
		}
	}

	TStringBuilder<256> AbsPathDenyList;
	IFileManager& FileManager = IFileManager::Get();
	FSetPathProperties ChangeDenyList;
	FScanDir::FInherited ParentData;
	ChangeDenyList.MatchesDenyList = true;
	for (const FString& RelPath : AddedDenyListPaths)
	{
		AbsPathDenyList.Reset();
		AbsPathDenyList << LocalAbsPath;
		FPathViews::AppendPath(AbsPathDenyList, RelPath);
		if (FileManager.DirectoryExists(AbsPathDenyList.ToString()))
		{
			FScanDirAndParentData UnusedControllingDir;
			Root->TrySetDirectoryProperties(RelPath, ParentData, ChangeDenyList, true /* bConfirmedExists */, UnusedControllingDir);
		}
	}
	ChangeDenyList.MatchesDenyList = false;
	for (const FString& RelPath : RemovedDenyLists)
	{
		// We don't need to check for existence when setting the removal property, because the scandir already exists
		FScanDirAndParentData UnusedControllingDir;
		Root->TrySetDirectoryProperties(RelPath, ParentData, ChangeDenyList, false /* bConfirmedExists */, UnusedControllingDir);
	}
}

void FMountDir::Update(TArray<FScanDirAndParentData>& OutScanRequests)
{
	FScanDir::FInherited ParentData = FScanDir::FInherited();
	Root->Update(OutScanRequests, ParentData);
}

FScanDir* FMountDir::GetFirstIncompleteScanDir()
{
	return Root->GetFirstIncompleteScanDir();
}

void FMountDir::SetHasStartedScanning()
{
	bHasStartedScanning = true;
}

void FMountDir::AddChildMount(FMountDir* ChildMount)
{
	if (!ChildMount)
	{
		return;
	}
	FStringView RelPath;
	if (!FPathViews::TryMakeChildPathRelativeTo(ChildMount->GetLocalAbsPath(), LocalAbsPath, RelPath))
	{
		return;
	}
	AddChildMountPath(RelPath);
	if (bHasStartedScanning)
	{
		UE_LOG(LogAssetRegistry, Warning, TEXT("AssetDataGatherer directory %.*s has already started scanning when a new mountpoint was added under it at %.*s. ")
			TEXT("Assets in the new mount point may exist twice in the AssetRegistry under two different package names."),
			LocalAbsPath.Len(), *LocalAbsPath, ChildMount->LocalAbsPath.Len(), *ChildMount->LocalAbsPath);
	}
	UpdateDenyList();
	MarkDirty(RelPath);
}

void FMountDir::RemoveChildMount(FMountDir* ChildMount)
{
	if (!ChildMount)
	{
		return;
	}
	FStringView RelPath;
	if (!FPathViews::TryMakeChildPathRelativeTo(ChildMount->GetLocalAbsPath(), LocalAbsPath, RelPath))
	{
		return;
	}
	if (!RemoveChildMountPath(RelPath))
	{
		return;
	}
	if (ChildMount->bHasStartedScanning)
	{
		UE_LOG(LogAssetRegistry, Warning, TEXT("AssetDataGatherer directory %.*s has already started scanning when it was removed and merged into its parent mount at %.*s. ")
			TEXT("Assets in the new mount point may exist twice in the AssetRegistry under two different package names."),
			ChildMount->LocalAbsPath.Len(), *ChildMount->LocalAbsPath, LocalAbsPath.Len(), *LocalAbsPath);
	}
	UpdateDenyList();
	MarkDirty(RelPath);
}

void FMountDir::OnDestroyClearChildMounts()
{
	ChildMountPaths.Empty();
}

void FMountDir::SetParentMount(FMountDir* Parent)
{
	ParentMount = Parent;
}

FMountDir* FMountDir::GetParentMount() const
{
	return ParentMount;
}

TArray<FMountDir*> FMountDir::GetChildMounts() const
{
	// Called within Discovery's TreeLock
	TArray<FMountDir*> Result;
	for (const FString& ChildPath : ChildMountPaths)
	{
		TStringBuilder<256> ChildAbsPath;
		ChildAbsPath << LocalAbsPath;
		FPathViews::AppendPath(ChildAbsPath, ChildPath);
		FMountDir* ChildMount = Discovery.FindMountPoint(ChildAbsPath);
		if (ensure(ChildMount)) // This PathData information should have been removed with RemoveChildMount when the child MountDir was removed from the Discovery
		{
			Result.Add(ChildMount);
		}
	}
	return Result;
}

void FMountDir::MarkDirty(FStringView MountRelPath)
{
	FScanDir::FInherited UnusedMonitorData;
	FString ControlRelPath;
	FScanDir* ScanDir = Root->GetControllingDir(MountRelPath, true /* bIsDirectory */, FScanDir::FInherited() /* ParentData */,
		UnusedMonitorData, ControlRelPath);
	if (ScanDir)
	{
		// If a ScanDir exists for the directory that is being marked dirty, mark all of its descendants dirty as well.
		// If the control dir is a parent directory of the requested path, just mark it and its parents dirty
		// Mark all parents dirty in either case
		bool bDirtyAllDescendents = ControlRelPath.IsEmpty();
		ScanDir->MarkDirty(bDirtyAllDescendents);
	}
}

void FMountDir::AddChildMountPath(FStringView MountRelPath)
{
	FString* ExistingPath = ChildMountPaths.FindByPredicate([&MountRelPath](const FString& ChildPath) { return FPathViews::Equals(ChildPath, MountRelPath); });
	if (!ExistingPath)
	{
		ChildMountPaths.Emplace(MountRelPath);
	}
}

bool FMountDir::RemoveChildMountPath(FStringView MountRelPath)
{
	return ChildMountPaths.RemoveAllSwap([MountRelPath](const FString& ChildPath) { return FPathViews::Equals(ChildPath, MountRelPath);  }) != 0;
}

bool FMountDir::IsChildMountPath(FStringView MountRelPath) const
{
	for (const FString& ChildPath : ChildMountPaths)
	{
		if (FPathViews::IsParentPathOf(ChildPath, MountRelPath))
		{
			return true;
		}
	}
	return false;
}


FAssetDataDiscovery::FAssetDataDiscovery(const TArray<FString>& InLongPackageNamesDenyList,
	const TArray<FString>& InMountRelativePathsDenyList, bool bInAsyncEnabled)
	: LongPackageNamesDenyList(InLongPackageNamesDenyList)
	, MountRelativePathsDenyList(InMountRelativePathsDenyList)
	, Thread(nullptr)
	, bAsyncEnabled(bInAsyncEnabled)
	, bIsIdle(false)
	, IsStopped(0)
	, IsPaused(0)
	, NumDirectoriesToScan(0)
{
	DirLongPackageNamesToNotReport.Add(TEXT("/Game/Collections"));

	if (bAsyncEnabled && !FPlatformProcess::SupportsMultithreading())
	{
		bAsyncEnabled = false;
		UE_LOG(LogAssetRegistry, Warning, TEXT("Requested asyncronous asset data discovery, but threading support is disabled. Performing a synchronous discovery instead!"));
	}

	PriorityDataUpdated->Trigger();

	FParse::Value(FCommandLine::Get(), TEXT("-ARDiscoverThreads="), AssetDataGathererConstants::GARDiscoverThreads);
	FParse::Value(FCommandLine::Get(), TEXT("-ARDiscoverMinBatchSize="), AssetDataGathererConstants::GARDiscoverMinBatchSize);
	AssetDataGathererConstants::GARDiscoverThreads = FMath::Max(0, AssetDataGathererConstants::GARDiscoverThreads);
	AssetDataGathererConstants::GARDiscoverMinBatchSize = FMath::Max(1, AssetDataGathererConstants::GARDiscoverMinBatchSize);
}

FAssetDataDiscovery::~FAssetDataDiscovery()
{
	EnsureCompletion();
	// Remove pointers to other MountDirs before we delete any of them
	for (TUniquePtr<FMountDir>& MountDir : MountDirs)
	{
		MountDir->SetParentMount(nullptr);
		MountDir->OnDestroyClearChildMounts();
	}
	MountDirs.Empty();
}

void FAssetDataDiscovery::StartAsync()
{
	if (bAsyncEnabled && !Thread)
	{
		Thread = FRunnableThread::Create(this, TEXT("FAssetDataDiscovery"), 0, TPri_BelowNormal);
		checkf(Thread, TEXT("Failed to create asset data discovery thread"));
	}
}

bool FAssetDataDiscovery::IsSynchronous() const
{
	return Thread == nullptr;
}

bool FAssetDataDiscovery::Init()
{
	return true;
}

uint32 FAssetDataDiscovery::Run()
{
	LLM_SCOPE(ELLMTag::AssetRegistry);
	constexpr float IdleSleepTime = 0.1f;

	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(TreeLock);
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	while (!IsStopped)
	{
		bool bTickOwner = false;
		while (!IsStopped && !bIsIdle && !IsPaused)
		{
			if (!bTickOwner)
			{
				if (TickOwner.TryTakeOwnership(TreeLock))
				{
					bTickOwner = true;
				}
			}
			if (bTickOwner)
			{
				TickInternal(true /* bTickAll */);
			}
			else
			{
				FPlatformProcess::Sleep(IdleSleepTime);
			}
		}
		if (bTickOwner)
		{
			TickOwner.ReleaseOwnershipChecked(TreeLock);
			bTickOwner = false;
		}

		while (!IsStopped && (IsPaused || bIsIdle))
		{
			// No work to do. Sleep for a little and try again later.
			// TODO: Need IsPaused to be a condition variable so we avoid sleeping while waiting for it and then taking a long time to wake after it is unset.
			FPlatformProcess::Sleep(IdleSleepTime);
		} 
	}
	return 0;
}

FAssetDataDiscovery::FScopedPause::FScopedPause(const FAssetDataDiscovery& InOwner)
	:Owner(InOwner)
{
	if (!Owner.IsSynchronous())
	{
		Owner.IsPaused++;
	}
	while (!Owner.TickOwner.TryTakeOwnership(Owner.TreeLock))
	{
		check(!Owner.TickOwner.IsOwnedByCurrentThread());
		constexpr float BlockingSleepTime = 0.001f;
		FPlatformProcess::Sleep(BlockingSleepTime);
	}
}

FAssetDataDiscovery::FScopedPause::~FScopedPause()
{
	Owner.TickOwner.ReleaseOwnershipChecked(Owner.TreeLock);
	if (!Owner.IsSynchronous())
	{
		check(Owner.IsPaused > 0);
		Owner.IsPaused--;
	}
}

void FAssetDataDiscovery::Stop()
{
	IsStopped++;
}

void FAssetDataDiscovery::Exit()
{
}

void FAssetDataDiscovery::EnsureCompletion()
{
	Stop();

	if (Thread != nullptr)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

void FAssetDataDiscovery::OnInitialSearchCompleted()
{
	Cache.SaveCache();
	Cache.Shutdown();
}

void FAssetDataDiscovery::TickInternal(bool bTickAll)
{
	LLM_SCOPE(ELLMTag::AssetRegistry);
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	check(TickOwner.IsOwnedByCurrentThread());

	if (!Cache.IsInitialized())
	{
		Cache.LoadAndUpdateCache();
	}

	TArray<FScanDirAndParentData> ScanRequests;
	TStringBuilder<128> DirMountRelPath;

	int32 LocalNumCachedDirectories = 0;
	int32 DirToScanDatasNum = 0;
	bool bUpdatedPriorityData = false;
	double TickStartTime = FPlatformTime::Seconds();
	ON_SCOPE_EXIT
	{
		if (TickStartTime >= 0.)
		{
			CurrentDiscoveryTime += FPlatformTime::Seconds() - TickStartTime;
			TickStartTime = -1.;
		}
	};
	for (;;)
	{
		{
			FGathererScopeLock TreeScopeLock(&TreeLock);

			// Process scanned directories from the previous iteration of the for(;;) loop.
			int32 NumScanned = 0;
			if (DirToScanDatasNum > 0)
			{
				for (FDirToScanData& Data : TArrayView<FDirToScanData>(DirToScanDatas.GetData(), DirToScanDatasNum))
				{
					if (Data.bScanned)
					{
						++NumScanned;
						TArrayView<FDiscoveredPathData> LocalSubDirs(Data.IteratedSubDirs.GetData(), Data.NumIteratedDirs);
						TArrayView<FDiscoveredPathData> LocalDiscoveredFiles(Data.IteratedFiles.GetData(), Data.NumIteratedFiles);
						if (!Data.ScanDir->IsValid())
						{
							// The ScanDir has been shutdown, and it is only still allocated to prevent us from crashing. Drop our reference and allow it to delete.
						}
						else if (Data.ScanDir->IsScanInFlightInvalidated())
						{
							// Some setting has been applied to the ScanDir that requires a new scan
							// Consume the invalidated flag and ignore the results of our scan
							Data.ScanDir->SetScanInFlightInvalidated(false);
						}
						else
						{
							Data.ScanDir->SetScanResults(Data.DirLocalAbsPath, Data.ParentData, LocalSubDirs, LocalDiscoveredFiles);
							if (!LocalSubDirs.IsEmpty() || !LocalDiscoveredFiles.IsEmpty())
							{
								AddDiscovered(Data.DirLocalAbsPath, Data.DirLongPackageName, LocalSubDirs, LocalDiscoveredFiles);
							}
						}
						Data.ScanDir->SetScanInFlight(false);
						Data.ScanDir.SafeRelease();
					}
				}
				// Rather than collecting LocalNumUncachedDirectories from inside the parallel for that scans them,
				// just calculate it from the number of Data.bScanned and the number of LocalNumCachedDirectories
				int32 LocalNumUncachedDirectories = NumScanned - LocalNumCachedDirectories;
				DirToScanDatasNum = 0;
				{
					FGathererScopeLock ResultsScopeLock(&ResultsLock);
					NumCachedDirectories += LocalNumCachedDirectories;
					NumUncachedDirectories += LocalNumUncachedDirectories;
				}
				LocalNumCachedDirectories = 0;
				LocalNumUncachedDirectories = 0;
			}

			// Look for new dirs to scan, break out of the for (;;) loop if we don't find any
			auto AddScanRequest = [this, &DirToScanDatasNum, &DirMountRelPath](FScanDirAndParentData&& ScanRequest)
			{
				if (DirToScanDatas.Num() <= DirToScanDatasNum)
				{
					// We increment DirToScanDatasNum one at a time so DirToScanDatas.Num() should always be >= to it.
					check(DirToScanDatas.Num() == DirToScanDatasNum);
					DirToScanDatas.Emplace();
				}
				FScanDir* ScanDir = ScanRequest.ScanDir.GetReference();
				FDirToScanData& ScanData = DirToScanDatas[DirToScanDatasNum++];
				ScanData.Reset();
				DirMountRelPath.Reset();
				ScanDir->SetScanInFlight(true);
				FMountDir* MountDir = ScanDir->GetMountDir();
				check(MountDir);
				ScanDir->AppendMountRelPath(DirMountRelPath);
				ScanData.DirLocalAbsPath << MountDir->GetLocalAbsPath();
				FPathViews::AppendPath(ScanData.DirLocalAbsPath, DirMountRelPath);
				ScanData.DirLongPackageName << MountDir->GetLongPackageName();
				FPathViews::AppendPath(ScanData.DirLongPackageName, DirMountRelPath);
				// The DirLocalAbsPath and DirLongPackageName need to be normalized. They are already mostly
				// normalized, but might have a redundant terminating separator
				while (FPathViews::HasRedundantTerminatingSeparator(ScanData.DirLocalAbsPath))
				{
					ScanData.DirLocalAbsPath.RemoveSuffix(1);
				}
				while (FPathViews::HasRedundantTerminatingSeparator(ScanData.DirLongPackageName))
				{
					ScanData.DirLongPackageName.RemoveSuffix(1);
				}
				ScanData.ScanDir = MoveTemp(ScanRequest.ScanDir);
				ScanData.ParentData = MoveTemp(ScanRequest.ParentData);
			};

			bool bExitAfterPriorityUpdate = !bTickAll && bUpdatedPriorityData;
			if (!PriorityScanDirs.IsEmpty())
			{
				ScanRequests.Reset();
				for (int32 PriorityIndex = 0; PriorityIndex < PriorityScanDirs.Num(); ++PriorityIndex)
				{
					FPriorityScanDirData& PriorityData = PriorityScanDirs[PriorityIndex];
					int32 OriginalScanRequestsNum = ScanRequests.Num();
					if (PriorityData.ScanDir->IsValid() && !PriorityData.ScanDir->IsComplete())
					{
						PriorityData.ScanDir->Update(ScanRequests, PriorityData.ParentData);
					}
					if (PriorityData.ScanDir->IsComplete())
					{
						// Update should not add ScanRequests if it was already or transitioned to complete
						check(ScanRequests.Num() == OriginalScanRequestsNum); 
						if (PriorityData.bReleaseWhenComplete)
						{
							PriorityData.bReleaseWhenComplete = false;
							check(PriorityData.RequestCount > 0);
							PriorityData.RequestCount--;
							if (PriorityData.RequestCount == 0)
							{
								PriorityScanDirs.RemoveAtSwap(PriorityIndex);
								--PriorityIndex; // Counteract the ++PriorityIndex in the for loop iteration
							}
							continue;
						}
					}
				}

				if (!bExitAfterPriorityUpdate)
				{
					// A ScanDir and its parent can both be in PriorityScanDirs, and in that case we can get duplicates
					// in the list of ScanRequests. Ensure uniqueness now.
					Algo::Sort(ScanRequests,
						[](const FScanDirAndParentData & A, const FScanDirAndParentData& B)
						{
							return A.ScanDir.GetReference() < B.ScanDir.GetReference();
						});
					ScanRequests.SetNum(Algo::Unique(ScanRequests,
						[](const FScanDirAndParentData& A, const FScanDirAndParentData& B)
						{
							return A.ScanDir.GetReference() == B.ScanDir.GetReference();
						}));

					for (FScanDirAndParentData& ScanRequest : ScanRequests)
					{
						AddScanRequest(MoveTemp(ScanRequest));
					}
				}
				ScanRequests.Reset();
			}
			if (bUpdatedPriorityData)
			{
				PriorityDataUpdated->Trigger();
			}
			if (bExitAfterPriorityUpdate)
			{
				return; // exit to check the done condition
			}
			bPriorityDirty = false;
			bUpdatedPriorityData = DirToScanDatasNum > 0;

			if (bTickAll && DirToScanDatasNum == 0)
			{
				ScanRequests.Reset();
				UpdateAll(ScanRequests);
				for (FScanDirAndParentData& ScanRequest : ScanRequests)
				{
					AddScanRequest(MoveTemp(ScanRequest));
				}
				ScanRequests.Reset();
			}

			if (DirToScanDatasNum == 0)
			{
				if (!bTickAll)
				{
					return;
				}

				int32 LocalNumDirectoriesToScan = NumDirectoriesToScan.GetValue();
				if (LocalNumDirectoriesToScan != 0)
				{
					// We have some directories left to scan, but we were unable to find any of them. Print diagnostics.
					FScanDir* Incomplete = nullptr;
					for (TUniquePtr<FMountDir>& MountDir : MountDirs)
					{
						Incomplete = MountDir->GetFirstIncompleteScanDir();
						if (Incomplete)
						{
							break;
						}
					}
					UE_LOG(LogAssetRegistry, Warning, TEXT("FAssetDataDiscovery::SetIsIdle(true) called when NumDirectoriesToScan == %d.\n")
						TEXT("First incomplete scandir: %s"), LocalNumDirectoriesToScan, Incomplete ? *Incomplete->GetLocalAbsPath() : TEXT("<NoneFound>"));
				}
				SetIsIdle(true, TickStartTime);
				return;
			}
			SetIsIdle(false);
		}

		// Outside of the TreeLock critical section, scan the directories

		// Process on a single thread any of the DirToScans that we find in the cache.
		for (int32 DirToScanDatasIndex = 0; DirToScanDatasIndex < DirToScanDatasNum; ++DirToScanDatasIndex)
		{
			FDirToScanData& Data = DirToScanDatas[DirToScanDatasIndex];
			FCachedDirScanDir* PathData = Cache.FindDir(Data.DirLocalAbsPath);
			if (!PathData || !PathData->bCacheValid)
			{
				continue;
			}
			Data.bScanned = true;
			++LocalNumCachedDirectories;

			for (const FString& RelPath : PathData->SubDirRelPaths)
			{
				// Don't enter directories that contain invalid packagepath characters (including '.';
				// extensions are not valid in content directories because '.' is not valid in a packagepath)
				if (!FPackageName::DoesPackageNameContainInvalidCharacters(RelPath))
				{
					int32 DirLongPackageRootNameLen = Data.DirLongPackageName.Len();
					int32 DirLocalAbsPathLen = Data.DirLocalAbsPath.Len();
					ON_SCOPE_EXIT
					{
						Data.DirLongPackageName.RemoveSuffix(Data.DirLongPackageName.Len() - DirLongPackageRootNameLen);
						Data.DirLocalAbsPath.RemoveSuffix(Data.DirLocalAbsPath.Len() - DirLocalAbsPathLen);
					};

					FPathViews::AppendPath(Data.DirLongPackageName, RelPath);
					FPathViews::AppendPath(Data.DirLocalAbsPath, RelPath);
					if (Data.IteratedSubDirs.Num() < Data.NumIteratedDirs + 1)
					{
						check(Data.IteratedSubDirs.Num() == Data.NumIteratedDirs);
						Data.IteratedSubDirs.Emplace();
					}
					Data.IteratedSubDirs[Data.NumIteratedDirs++].Assign(Data.DirLocalAbsPath, Data.DirLongPackageName, RelPath,
						EGatherableFileType::Directory, false /* bBlocked */);
				}
			}
			for (const FCachedDirScanFile& FileData : PathData->Files)
			{
				FStringView RelPath(FileData.RelPath);

				EGatherableFileType FileType = GetFileType(RelPath);
				// Don't record files that contain invalid packagepath characters (not counting their extension)
				// or that do not end with a recognized extension
				if (FileType != EGatherableFileType::Invalid)
				{
					FStringView BaseName = FPathViews::GetBaseFilename(RelPath);
					if (!FPackageName::DoesPackageNameContainInvalidCharacters(BaseName))
					{
						int32 DirLongPackageRootNameLen = Data.DirLongPackageName.Len();
						int32 DirLocalAbsPathLen = Data.DirLocalAbsPath.Len();
						ON_SCOPE_EXIT
						{
							Data.DirLongPackageName.RemoveSuffix(Data.DirLongPackageName.Len() - DirLongPackageRootNameLen);
							Data.DirLocalAbsPath.RemoveSuffix(Data.DirLocalAbsPath.Len() - DirLocalAbsPathLen);
						};

						if (Data.IteratedFiles.Num() < Data.NumIteratedFiles + 1)
						{
							check(Data.IteratedFiles.Num() == Data.NumIteratedFiles);
							Data.IteratedFiles.Emplace();
						}
						FPathViews::AppendPath(Data.DirLongPackageName, BaseName);
						FPathViews::AppendPath(Data.DirLocalAbsPath, RelPath);
						const bool bBlocked = FileType == EGatherableFileType::PackageFile && IsPackageBlocked(Data.DirLocalAbsPath);
						Data.IteratedFiles[Data.NumIteratedFiles++].Assign(Data.DirLocalAbsPath, Data.DirLongPackageName, RelPath,
							FileData.ModificationTime, FileType, bBlocked);
					}
				}
			}
		}

		// If we found any cached directories, keep looking in their children before we start querying
		// the disk for uncached
		if (LocalNumCachedDirectories > 0 && !bUpdatedPriorityData)
		{
			continue;
		}

		// Otherwise look on disk in parallel for all of the DirToScans
		int32 NumThreads = FMath::Max(FTaskGraphInterface::Get().GetNumWorkerThreads(), 1);
		if (AssetDataGathererConstants::GARDiscoverThreads > 0)
		{
			NumThreads = FMath::Min(NumThreads, AssetDataGathererConstants::GARDiscoverThreads);
		}
		int32 DirToScanBuffersNum = FMath::Min(NumThreads, DirToScanDatasNum);
		if (DirToScanBuffers.Num() < DirToScanBuffersNum)
		{
			DirToScanBuffers.SetNum(DirToScanBuffersNum);
		}
		TArrayView<FDirToScanBuffer> LocalScanBuffers(DirToScanBuffers.GetData(), DirToScanBuffersNum);
		for (FDirToScanBuffer& ScanBuffer : LocalScanBuffers)
		{
			ScanBuffer.Reset();
		}

		ParallelForWithExistingTaskContext(LocalScanBuffers, DirToScanDatasNum, AssetDataGathererConstants::GARDiscoverMinBatchSize,
		[this](FDirToScanBuffer& ScanBuffer, int32 DirToScanDatasIndex)
		{
			FDirToScanData& Data = DirToScanDatas[DirToScanDatasIndex];
			if (Data.bScanned)
			{
				return;
			}
			if (ScanBuffer.bAbort)
			{
				return;
			}
			if (bPriorityDirty.load(std::memory_order_relaxed))
			{
				ScanBuffer.bAbort = true;
				return;
			}

			FCachedDirScanDir CacheDataToAdd;
			auto ProcessIterData = [this, &Data, &CacheDataToAdd]
			(const TCHAR* IterFilename, bool bIsDirectory, const FDateTime& ModificationTime, FFileJournalFileHandle JournalHandle)
			{
				FStringView LocalAbsPath(IterFilename);
				FStringView RelPath;
				FString Buffer;
				if (!FPathViews::TryMakeChildPathRelativeTo(IterFilename, Data.DirLocalAbsPath, RelPath))
				{
					// Try again with the path converted to the absolute path format that we passed in; some
					// IFileManagers can send relative paths to the visitor even though the search path is absolute
					Buffer = FPaths::ConvertRelativePathToFull(FString(IterFilename));
					LocalAbsPath = Buffer;
					if (!FPathViews::TryMakeChildPathRelativeTo(Buffer, Data.DirLocalAbsPath, RelPath))
					{
						UE_LOG(LogAssetRegistry, Warning,
							TEXT("IterateDirectory returned unexpected result %s which is not a child of the requested path %s."),
							IterFilename, Data.DirLocalAbsPath.ToString());
						return true;
					}
				}
				if (FPathViews::GetPathLeaf(RelPath).Len() != RelPath.Len())
				{
					UE_LOG(LogAssetRegistry, Warning,
						TEXT("IterateDirectory returned unexpected result %s which is not a direct child of the requested path %s."),
						IterFilename, Data.DirLocalAbsPath.ToString());
					return true;
				}
				int32 DirLongPackageRootNameLen = Data.DirLongPackageName.Len();
				ON_SCOPE_EXIT
				{
					Data.DirLongPackageName.RemoveSuffix(Data.DirLongPackageName.Len() - DirLongPackageRootNameLen);
				};

				if (bIsDirectory)
				{
					if (Cache.IsWriteEnabled() != EFeatureEnabled::Never)
					{
						CacheDataToAdd.SubDirRelPaths.Add(FString(RelPath));
						Cache.QueueAdd(FString(LocalAbsPath), JournalHandle);
					}

					FPathViews::AppendPath(Data.DirLongPackageName, RelPath);
					// Don't enter directories that contain invalid packagepath characters (including '.';
					// extensions are not valid in content directories because '.' is not valid in a packagepath)
					if (!FPackageName::DoesPackageNameContainInvalidCharacters(RelPath))
					{
						if (Data.IteratedSubDirs.Num() < Data.NumIteratedDirs + 1)
						{
							check(Data.IteratedSubDirs.Num() == Data.NumIteratedDirs);
							Data.IteratedSubDirs.Emplace();
						}
						Data.IteratedSubDirs[Data.NumIteratedDirs++].Assign(LocalAbsPath, Data.DirLongPackageName, RelPath,
							EGatherableFileType::Directory, false /* bBlocked */);
					}
				}
				else
				{
					if (Cache.IsWriteEnabled() != EFeatureEnabled::Never)
					{
						FCachedDirScanFile& FileData = CacheDataToAdd.Files.Emplace_GetRef();
						FileData.RelPath = FString(RelPath);
						FileData.ModificationTime = ModificationTime;
					}
					EGatherableFileType FileType = GetFileType(RelPath);
					// Don't record files that contain invalid packagepath characters (not counting their extension)
					// or that do not end with a recognized extension
					if (FileType != EGatherableFileType::Invalid)
					{
						FStringView BaseName = FPathViews::GetBaseFilename(RelPath);
						if (!FPackageName::DoesPackageNameContainInvalidCharacters(BaseName))
						{
							if (Data.IteratedFiles.Num() < Data.NumIteratedFiles + 1)
							{
								check(Data.IteratedFiles.Num() == Data.NumIteratedFiles);
								Data.IteratedFiles.Emplace();
							}
							FPathViews::AppendPath(Data.DirLongPackageName, BaseName);
							const bool bBlocked = FileType == EGatherableFileType::PackageFile && IsPackageBlocked(LocalAbsPath);
							Data.IteratedFiles[Data.NumIteratedFiles++].Assign(LocalAbsPath, Data.DirLongPackageName, RelPath,
								ModificationTime, FileType, bBlocked);
						}
					}
				}
				return true;
			};

			if (Cache.IsWriteEnabled() != EFeatureEnabled::Never)
			{
				FPlatformFileManager::Get().GetPlatformFile().FileJournalIterateDirectory(Data.DirLocalAbsPath.ToString(),
					[this, &Data, &CacheDataToAdd, &ProcessIterData]
					(const TCHAR* IterFilename, const FFileJournalData& IterData)
					{
						return ProcessIterData(IterFilename, IterData.bIsDirectory, IterData.ModificationTime, IterData.JournalHandle);
					});

				Cache.QueueAdd(FString(Data.DirLocalAbsPath), MoveTemp(CacheDataToAdd));
			}
			else
			{
				IFileManager::Get().IterateDirectoryStat(Data.DirLocalAbsPath.ToString(),
					[this, &Data, &ProcessIterData]
					(const TCHAR* IterFilename, const FFileStatData& IterData)
					{
						return ProcessIterData(IterFilename, IterData.bIsDirectory, IterData.ModificationTime, FFileJournalFileHandle());
					});
			}

			Data.bScanned = true;
		}, EParallelForFlags::BackgroundPriority);

		if (Cache.IsWriteEnabled() != EFeatureEnabled::Never)
		{
			Cache.QueueConsume();
		}
	}
}

void FAssetDataDiscovery::UpdateAll(TArray<FScanDirAndParentData>& OutScanRequests)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TreeLock);
	for (TUniquePtr<FMountDir>& MountDirOwner : MountDirs)
	{
		FMountDir* MountDir = MountDirOwner.Get();
		if (MountDir->IsComplete())
		{
			continue;
		}

		MountDir->Update(OutScanRequests);
	}
}

void FAssetDataDiscovery::SetIsIdle(bool bInIsIdle)
{
	double TickStartTime = -1.;
	SetIsIdle(bInIsIdle, TickStartTime);
}

void FAssetDataDiscovery::SetIsIdle(bool bInIsIdle, double& TickStartTime)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TreeLock);

	// Caller is responsible for holding TreeLock around this function; writes of SetIsIdle are done inside the TreeLock
	// If bIsIdle is true, caller holds TickOwner and TreeLock
	if (bIsIdle == bInIsIdle)
	{
		return;
	}
	FGathererScopeLock ResultsScopeLock(&ResultsLock);
	bIsIdle = bInIsIdle;
	if (!IsSynchronous())
	{
		if (bIsIdle)
		{
			if (TickStartTime >= 0.)
			{
				CurrentDiscoveryTime += FPlatformTime::Seconds() - TickStartTime;
				TickStartTime = -1.;
			}

			CumulativeDiscoveryTime += static_cast<float>(CurrentDiscoveryTime);
			CumulativeDiscoveredFiles += NumDiscoveredFiles;
			UE_LOG(LogAssetRegistry, Verbose,
				TEXT("Discovery took %0.4f seconds to add %d files, Cumulative=%0.4f seconds to add %d."),
				CurrentDiscoveryTime, NumDiscoveredFiles, CumulativeDiscoveryTime, CumulativeDiscoveredFiles);
			CurrentDiscoveryTime = 0.;
		}
		else
		{
			NumDiscoveredFiles = 0;
		}
	}

	if (bIsIdle)
	{
		check(TickOwner.IsOwnedByCurrentThread());
		Shrink();
	}
}


void FAssetDataDiscovery::GetAndTrimSearchResults(bool& bOutIsComplete, TArray<FString>& OutDiscoveredPaths, FFilesToSearch& OutFilesToSearch, int32& OutNumPathsToSearch)
{
	FGathererScopeLock ResultsScopeLock(&ResultsLock);

	OutDiscoveredPaths.Append(MoveTemp(DiscoveredDirectories));
	DiscoveredDirectories.Reset();

	for (FDirectoryResult& DirectoryResult : DiscoveredFiles)
	{
		OutFilesToSearch.AddDirectory(MoveTemp(DirectoryResult.DirAbsPath), MoveTemp(DirectoryResult.Files));
	}
	DiscoveredFiles.Reset();
	for (FGatheredPathData& FileResult : DiscoveredSingleFiles)
	{
		// Single files are currently only added from the blocking function FAssetDataDiscovery::SetPropertiesAndWait,
		// so we add them at blocking priority.
		OutFilesToSearch.AddPriorityFile(MoveTemp(FileResult));
	}
	DiscoveredSingleFiles.Reset();

	OutNumPathsToSearch = NumDirectoriesToScan.GetValue();
	bOutIsComplete = bIsIdle;
	if (bIsIdle && OutNumPathsToSearch != 0)
	{
		UE_LOG(LogAssetRegistry, Warning, TEXT("FAssetDataDiscovery::GetAndTrimSearchResults is returning bIsIdle=true while OutNumPathsToSearch=%d."),
			OutNumPathsToSearch);
	}
}

void FAssetDataDiscovery::GetDiagnostics(float& OutCumulativeDiscoveryTime, int32& OutNumCachedDirectories,
	int32& OutNumUncachedDirectories)
{
	FGathererScopeLock ResultsScopeLock(&ResultsLock);
	OutCumulativeDiscoveryTime = CumulativeDiscoveryTime;
	OutNumCachedDirectories = NumCachedDirectories;
	OutNumUncachedDirectories = NumUncachedDirectories;
}


void FAssetDataDiscovery::WaitForIdle(double EndTimeSeconds)
{
	if (bIsIdle)
	{
		return;
	}
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(TreeLock);
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);

	constexpr float IdleSleepTime = 0.1f;
	bool bTickOwner = false;
	while (!bIsIdle)
	{
		if (!bTickOwner)
		{
			bTickOwner = TickOwner.TryTakeOwnership(TreeLock);
		}
		if (bTickOwner)
		{
			TickInternal(true /* bTickAll */);
		}
		else
		{
			float SleepTime = IdleSleepTime;
			if (EndTimeSeconds > 0)
			{
				SleepTime = FMath::Min(SleepTime, static_cast<float>(EndTimeSeconds - FPlatformTime::Seconds()));
			}
			if (SleepTime > 0.f)
			{
				FPlatformProcess::Sleep(SleepTime);
			}
		}
		if (EndTimeSeconds > 0 && FPlatformTime::Seconds() > EndTimeSeconds)
		{
			break;
		}
	}
	if (bTickOwner)
	{
		TickOwner.ReleaseOwnershipChecked(TreeLock);
	}
}

bool FAssetDataDiscovery::IsIdle() const
{
	return bIsIdle;
}

FPathExistence::FPathExistence(FStringView InLocalAbsPath)
	:LocalAbsPath(InLocalAbsPath)
{
}

const FString& FPathExistence::GetLocalAbsPath() const
{
	return LocalAbsPath;
}

FStringView FPathExistence::GetLowestExistingPath()
{
	LoadExistenceData();
	switch (PathType)
	{
	case EType::MissingButDirExists:
		return FPathViews::GetPath(LocalAbsPath);
	case EType::MissingParentDir:
		return FStringView();
	default:
		return LocalAbsPath;
	}
}

FPathExistence::EType FPathExistence::GetType()
{
	LoadExistenceData();
	return PathType;
}

FDateTime FPathExistence::GetModificationTime()
{
	LoadExistenceData();
	return ModificationTime;
}

void FPathExistence::LoadExistenceData()
{
	if (bHasExistenceData)
	{
		return;
	}
	FFileStatData StatData = IFileManager::Get().GetStatData(*LocalAbsPath);
	if (StatData.bIsValid)
	{
		FString CorrectedCapitalization = IFileManager::Get().GetFilenameOnDisk(*LocalAbsPath);
		if (LocalAbsPath == CorrectedCapitalization)
		{
			LocalAbsPath = MoveTemp(CorrectedCapitalization);
		}
		else
		{
			UE_LOG(LogAssetRegistry, Verbose,
				TEXT("FPathExistence failed to gather correct capitalization from disk for %s, because GetFilenameOnDisk returned a non-matching filename"),
				*LocalAbsPath);
		}

		ModificationTime = StatData.ModificationTime;
		PathType = StatData.bIsDirectory ? EType::Directory : EType::File;
	}
	else
	{
		FString ParentPath, BaseName, Extension;
		FPaths::Split(LocalAbsPath, ParentPath, BaseName, Extension);
		StatData = IFileManager::Get().GetStatData(*ParentPath);
		if (StatData.bIsValid && StatData.bIsDirectory)
		{
			FString CorrectedCapitalization = IFileManager::Get().GetFilenameOnDisk(*ParentPath);
			CorrectedCapitalization = FPaths::Combine(CorrectedCapitalization, BaseName) +
				(!Extension.IsEmpty() ? TEXT(".") : TEXT("")) + Extension;
			if (LocalAbsPath == CorrectedCapitalization)
			{
				LocalAbsPath = MoveTemp(CorrectedCapitalization);
			}
			else
			{
				UE_LOG(LogAssetRegistry, Verbose,
					TEXT("FPathExistence failed to gather correct capitalization from disk for %s, because GetFilenameOnDisk returned a non-matching filename"),
					*LocalAbsPath);
			}
			PathType = EType::MissingButDirExists;
		}
		else
		{
			PathType = EType::MissingParentDir;
		}
	}

	bHasExistenceData = true;
}

void FAssetDataDiscovery::SetPropertiesAndWait(TArrayView<FPathExistence> QueryPaths, bool bAddToAllowList,
	bool bForceRescan, bool bIgnoreDenyListScanFilters)
{
	for (FPathExistence& QueryPath : QueryPaths)
	{
		QueryPath.LoadExistenceData();
	}

	struct FScanDirAndQueryPath
	{
		TRefCountPtr<FScanDir> ScanDir;
		bool bScanEntireTree = false;
	};
	TArray<FScanDirAndQueryPath> DirsToScan;
	bool bTickOwner = false;
	{
		CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
		FGathererScopeLock TreeScopeLock(&TreeLock);
		for (FPathExistence& QueryPath : QueryPaths)
		{
			// We might have been asked to wait on a filename missing the extension, in which case QueryPath.GetType() == MissingButDirExists
			// We need to handle Directory, File, and MissingButDirExists in unique ways
			FPathExistence::EType PathType = QueryPath.GetType();
			if (PathType == FPathExistence::EType::MissingParentDir)
			{
				// SetPropertiesAndWait is called for every ScanPathsSynchronous, and this is the first spot that checks for existence. 
				// Some systems call ScanPathsSynchronous speculatively to scan whatever is present, so this log is verbose-only.
				UE_LOG(LogAssetRegistry, Verbose, TEXT("SetPropertiesAndWait called on non-existent path %s. Call will be ignored."),
					*QueryPath.GetLocalAbsPath());
				continue;
			}
			FStringView SearchPath = QueryPath.GetLowestExistingPath();

			FMountDir* MountDir = FindContainingMountPoint(SearchPath);
			if (!MountDir)
			{
				UE_LOG(LogAssetRegistry, Log, TEXT("SetPropertiesAndWait called on %s which is not in a mounted directory. Call will be ignored."),
					*QueryPath.GetLocalAbsPath());
				continue;
			}

			if (PathType == FPathExistence::EType::Directory)
			{
				FSetPathProperties Properties;
				if (bAddToAllowList)
				{
					Properties.IsOnAllowList = bAddToAllowList;
				}
				if (bForceRescan)
				{
					Properties.HasScanned = false;
				}
				if (bIgnoreDenyListScanFilters)
				{
					Properties.IgnoreDenyList = true;
				}
				if (Properties.IsSet())
				{
					SetIsIdle(false);
					MountDir->TrySetDirectoryProperties(SearchPath, Properties, true /* bConfirmedExists */, nullptr);
				}
			}

			FString RelPath;
			FScanDir::FInherited MonitorData;
			bool bSearchPathIsDirectory = PathType == FPathExistence::EType::Directory || PathType == FPathExistence::EType::MissingButDirExists;
			TRefCountPtr<FScanDir> ScanDir = MountDir->GetControllingDir(SearchPath, bSearchPathIsDirectory, MonitorData, RelPath);
			bool bIsAllowedInThisCall = MonitorData.IsOnAllowList() || bAddToAllowList;
			bool bIsDeniedInThisCall = MonitorData.IsOnDenyList() && !bIgnoreDenyListScanFilters;
			bool bIsMonitoredInThisCall = bIsAllowedInThisCall && !bIsDeniedInThisCall;
			if (!ScanDir || !bIsMonitoredInThisCall)
			{
				UE_LOG(LogAssetRegistry, Log, TEXT("SetPropertiesAndWait called on %s which is not monitored. Call will be ignored."),
					*QueryPath.GetLocalAbsPath());
				continue;
			}

			if (bSearchPathIsDirectory)
			{
				// If Relpath from the controlling dir to the requested dir is not empty then we have found a parent directory rather than the requested directory.
				// This can only occur for a monitored directory when the requested directory is already complete and we do not need to wait on it.
				if (!RelPath.IsEmpty() || ScanDir->IsComplete())
				{
					continue;
				}

				FScanDirAndQueryPath& ScanQuery = DirsToScan.Emplace_GetRef();
				ScanQuery.ScanDir = ScanDir;
				ScanQuery.bScanEntireTree = PathType == FPathExistence::EType::Directory;
				FPriorityScanDirData* PriorityData = PriorityScanDirs.FindByPredicate(
					[&ScanDir](const FPriorityScanDirData& ScanDirData) { return ScanDirData.ScanDir == ScanDir; });
				if (!PriorityData)
				{
					PriorityData = &PriorityScanDirs.Add_GetRef(FPriorityScanDirData{ ScanDir });
				}
				++PriorityData->RequestCount;
				PriorityData->ParentData = MonitorData;
			}
			else
			{
				check(PathType == FPathExistence::EType::File);
				bool bAlreadyScanned = ScanDir->HasScanned() && MonitorData.IsMonitored();
				if (!bAlreadyScanned || bForceRescan)
				{
					FStringView RelPathFromParentDir = FPathViews::GetCleanFilename(RelPath);
					EGatherableFileType FileType = GetFileType(RelPathFromParentDir);
					if (FileType != EGatherableFileType::Invalid)
					{
						FStringView FileRelPathNoExt = FPathViews::GetBaseFilenameWithPath(RelPath);
						if (!FPackageName::DoesPackageNameContainInvalidCharacters(FileRelPathNoExt))
						{
							TStringBuilder<256> LongPackageName;
							LongPackageName << MountDir->GetLongPackageName();
							FPathViews::AppendPath(LongPackageName, ScanDir->GetMountRelPath());
							FPathViews::AppendPath(LongPackageName, FileRelPathNoExt);
							const bool bBlocked = FileType == EGatherableFileType::PackageFile && IsPackageBlocked(SearchPath);
							AddDiscoveredFile(FDiscoveredPathData(SearchPath, LongPackageName, RelPathFromParentDir, QueryPath.GetModificationTime(), FileType, bBlocked));
							if (FPathViews::IsPathLeaf(RelPath) && !ScanDir->HasScanned())
							{
								SetIsIdle(false);
								ScanDir->MarkFileAlreadyScanned(RelPath);
							}
						}
					}
				}
			}
		}

		if (!DirsToScan.IsEmpty())
		{
			bPriorityDirty = true;
			PriorityDataUpdated->Reset();
			bTickOwner = TickOwner.TryTakeOwnership(TreeScopeLock);
		}
	}

	while (!DirsToScan.IsEmpty())
	{
		if (bTickOwner)
		{
			TickInternal(false /* bTickAll */);
		}
		else
		{
			constexpr uint32 WaitTimeMilliseconds = 100;
			PriorityDataUpdated->Wait(WaitTimeMilliseconds);
		}

		{
			FGathererScopeLock LoopTreeScopeLock(&TreeLock);
			for (int32 Index = 0; Index < DirsToScan.Num(); ++Index)
			{
				FScanDirAndQueryPath& ScanQuery = DirsToScan[Index];
				FScanDir* ScanDir = ScanQuery.ScanDir.GetReference();
				auto RemoveCurrent = [&DirsToScan, &Index, ScanDir, this]()
				{
					DirsToScan.RemoveAtSwap(Index);
					int32 PriorityDataIndex = PriorityScanDirs.IndexOfByPredicate(
						[ScanDir](const FPriorityScanDirData& ScanDirData) { return ScanDirData.ScanDir.GetReference() == ScanDir; });

					check(PriorityDataIndex != INDEX_NONE); // Nothing should be able to remove it until we remove our RequestCount
					FPriorityScanDirData& PriorityData = PriorityScanDirs[PriorityDataIndex];
					check(PriorityData.RequestCount > 0);
					--PriorityData.RequestCount;
					if (PriorityData.RequestCount == 0)
					{
						PriorityScanDirs.RemoveAtSwap(PriorityDataIndex);
					}
				};

				if (!ScanDir->IsValid())
				{
					RemoveCurrent();
					continue;
				}
				if (ScanDir->IsComplete() || (!ScanQuery.bScanEntireTree && ScanDir->HasScanned()))
				{
					RemoveCurrent();
					continue;
				}
				else if (!ensureMsgf(!bIsIdle, TEXT("It should not be possible for the Discovery to go idle while there is an incomplete ScanDir.")))
				{
					RemoveCurrent();
					continue;
				}
			}

			if (DirsToScan.IsEmpty())
			{
				if (bTickOwner)
				{
					TickOwner.ReleaseOwnershipChecked(LoopTreeScopeLock);
					bTickOwner = false;
				}
			}
			else
			{
				PriorityDataUpdated->Reset();
				if (!bTickOwner)
				{
					bTickOwner = TickOwner.TryTakeOwnership(LoopTreeScopeLock);
				}
			}
		}
	}
}

void FAssetDataDiscovery::PrioritizeSearchPath(const FString& LocalAbsPath, EPriority Priority)
{
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	FGathererScopeLock TreeScopeLock(&TreeLock);
	SetIsIdle(false);
	FMountDir* MountDir = FindContainingMountPoint(LocalAbsPath);
	if (!MountDir)
	{
		UE_LOG(LogAssetRegistry, Warning, TEXT("FAssetDataGatherer::PrioritizeSearchPath called on unmounted path %.*s. Call will be ignored."),
			LocalAbsPath.Len(), *LocalAbsPath);
		return;
	}

	FSetPathProperties EmptyProperties;
	FScanDirAndParentData ScanDirAndParent;
	MountDir->TrySetDirectoryProperties(LocalAbsPath, EmptyProperties, false, &ScanDirAndParent);
	FScanDir* ScanDir = ScanDirAndParent.ScanDir.GetReference();
	if (ScanDir && ScanDir->IsValid() && !ScanDir->IsComplete())
	{
		FPriorityScanDirData* PriorityData = PriorityScanDirs.FindByPredicate(
			[&ScanDir](const FPriorityScanDirData& ScanDirData) { return ScanDirData.ScanDir.GetReference() == ScanDir; });
		if (!PriorityData)
		{
			PriorityData = &PriorityScanDirs.Add_GetRef(FPriorityScanDirData{ ScanDir });
		}
		if (!PriorityData->bReleaseWhenComplete)
		{
			PriorityData->bReleaseWhenComplete = true;
			++PriorityData->RequestCount;
		}
		PriorityData->ParentData = ScanDirAndParent.ParentData;
	}
}

bool FAssetDataDiscovery::TrySetDirectoryProperties(const FString& LocalAbsPath, const FSetPathProperties& InProperties, bool bConfirmedExists)
{
	if (!InProperties.IsSet())
	{
		return false;
	}
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	FGathererScopeLock TreeScopeLock(&TreeLock);
	SetIsIdle(false);
	if (!TrySetDirectoryPropertiesInternal(LocalAbsPath, InProperties, bConfirmedExists))
	{
		return false;
	}
	return true;
}

bool FAssetDataDiscovery::TrySetDirectoryPropertiesInternal(const FString& LocalAbsPath, const FSetPathProperties& InProperties, bool bConfirmedExists)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TreeLock);
	FMountDir* MountDir = FindContainingMountPoint(LocalAbsPath);
	if (!MountDir)
	{
		UE_LOG(LogAssetRegistry, Warning, TEXT("FAssetDataGatherer::SetDirectoryProperties called on unmounted path %.*s. Call will be ignored."), LocalAbsPath.Len(), *LocalAbsPath);
		return false;
	}

	return MountDir->TrySetDirectoryProperties(LocalAbsPath, InProperties, bConfirmedExists, nullptr);
}

bool FAssetDataDiscovery::IsOnAllowList(FStringView LocalAbsPath) const
{
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	FGathererScopeLock TreeScopeLock(&TreeLock);
	const FMountDir* MountDir = FindContainingMountPoint(LocalAbsPath);
	if (!MountDir)
	{
		return false;
	}
	FScanDir::FInherited MonitorData;
	MountDir->GetMonitorData(LocalAbsPath, MonitorData);
	return MonitorData.IsOnAllowList();
}

bool FAssetDataDiscovery::IsOnDenyList(FStringView LocalAbsPath) const
{
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	FGathererScopeLock TreeScopeLock(&TreeLock);
	const FMountDir* MountDir = FindContainingMountPoint(LocalAbsPath);
	if (!MountDir)
	{
		return false;
	}
	FScanDir::FInherited MonitorData;
	MountDir->GetMonitorData(LocalAbsPath, MonitorData);
	return MonitorData.IsOnDenyList();
}

bool FAssetDataDiscovery::IsMonitored(FStringView LocalAbsPath) const
{
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	FGathererScopeLock TreeScopeLock(&TreeLock);
	const FMountDir* MountDir = FindContainingMountPoint(LocalAbsPath);
	return MountDir && MountDir->IsMonitored(LocalAbsPath);
}

template <typename ContainerType>
SIZE_T GetArrayRecursiveAllocatedSize(const ContainerType& Container)
{
	SIZE_T Result = Container.GetAllocatedSize();
	for (const auto& Value : Container)
	{
		Result += Value.GetAllocatedSize();
	}
	return Result;
};

SIZE_T FAssetDataDiscovery::GetAllocatedSize() const
{
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(TreeLock);
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	check(!TickOwner.IsOwnedByCurrentThread());
	FScopedPause ScopedPause(*this);
	FGathererScopeLock TreeScopeLock(&TreeLock);
	FGathererScopeLock ResultsScopeLock(&ResultsLock);

	SIZE_T Result = 0;
	Result += GetArrayRecursiveAllocatedSize(LongPackageNamesDenyList);
	Result += GetArrayRecursiveAllocatedSize(MountRelativePathsDenyList);
	Result += GetArrayRecursiveAllocatedSize(DirLongPackageNamesToNotReport);
	if (Thread)
	{
		// TODO: Thread->GetAllocatedSize()
		Result += sizeof(*Thread);
	}

	Result += GetArrayRecursiveAllocatedSize(DiscoveredDirectories);
	Result += GetArrayRecursiveAllocatedSize(DiscoveredFiles);
	Result += GetArrayRecursiveAllocatedSize(DiscoveredSingleFiles);

	Result += MountDirs.GetAllocatedSize();
	for (const TUniquePtr<FMountDir>& Value : MountDirs)
	{
		Result += sizeof(*Value);
		Result += Value->GetAllocatedSize();
	}
	Result += GetArrayRecursiveAllocatedSize(DirToScanDatas);
	Result += DirToScanBuffers.GetAllocatedSize();
	return Result;
}

void FAssetDataDiscovery::FDirToScanData::Reset()
{
	DirLocalAbsPath.Reset();
	DirLongPackageName.Reset();
	NumIteratedDirs = 0;
	NumIteratedFiles = 0;
	bScanned = false;
}

SIZE_T FAssetDataDiscovery::FDirToScanData::GetAllocatedSize() const
{
	SIZE_T Result = 0;
	Result += DirLocalAbsPath.GetAllocatedSize();
	Result += DirLongPackageName.GetAllocatedSize();
	Result += GetArrayRecursiveAllocatedSize(IteratedSubDirs);
	Result += GetArrayRecursiveAllocatedSize(IteratedFiles);
	return Result;
}

void FAssetDataDiscovery::FDirToScanBuffer::Reset()
{
	bAbort = false;
}

SIZE_T FAssetDataDiscovery::FDirectoryResult::GetAllocatedSize() const
{
	return DirAbsPath.GetAllocatedSize() + Files.GetAllocatedSize();
}

void FAssetDataDiscovery::Shrink()
{
	check(TickOwner.IsOwnedByCurrentThread());
	CHECK_IS_LOCKED_CURRENT_THREAD(TreeLock);
	CHECK_IS_LOCKED_CURRENT_THREAD(ResultsLock);
	DirLongPackageNamesToNotReport.Shrink();
	DiscoveredDirectories.Shrink();
	DiscoveredFiles.Shrink();
	DiscoveredSingleFiles.Shrink();
	MountDirs.Shrink();
	for (TUniquePtr<FMountDir>& MountDir : MountDirs)
	{
		MountDir->Shrink();
	}
	DirToScanDatas.Empty();
	DirToScanBuffers.Empty();
}

void FAssetDataDiscovery::AddMountPoint(const FString& LocalAbsPath, FStringView LongPackageName, bool& bOutAlreadyExisted)
{
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	FGathererScopeLock TreeScopeLock(&TreeLock);
	bool bAlreadyExisted = false;
	AddMountPointInternal(LocalAbsPath, LongPackageName, bAlreadyExisted);
	if (!bAlreadyExisted)
	{
		SetIsIdle(false);
	}
}

void FAssetDataDiscovery::AddMountPointInternal(const FString& LocalAbsPath, FStringView LongPackageName, bool& bOutAlreadyExisted)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TreeLock);
	TArray<FMountDir*> ChildMounts;
	FMountDir* ParentMount = nullptr;
	bOutAlreadyExisted = false;
	for (TUniquePtr<FMountDir>& ExistingMount : MountDirs)
	{
		if (FPathViews::Equals(ExistingMount->GetLocalAbsPath(), LocalAbsPath))
		{
			bOutAlreadyExisted = true;
			break;
		}
		else if (FPathViews::IsParentPathOf(ExistingMount->GetLocalAbsPath(), LocalAbsPath))
		{
			// Overwrite any earlier ParentMount; later mounts are more direct parents than earlier mounts
			ParentMount = ExistingMount.Get();
		}
		else if (FPathViews::IsParentPathOf(LocalAbsPath, ExistingMount->GetLocalAbsPath()))
		{
			// A mount under the new directory might be a grandchild mount.
			// Don't add it as a child mount unless there is no other mount in between the new mount and the mount.
			FMountDir* ExistingParentMount = ExistingMount->GetParentMount();
			if (!ExistingParentMount || ExistingParentMount == ParentMount)
			{
				ChildMounts.Add(ExistingMount.Get());
			}
		}
	}
	if (bOutAlreadyExisted)
	{
		return;
	}
	SetIsIdle(false);

	FMountDir& Mount = FindOrAddMountPoint(LocalAbsPath, LongPackageName);
	if (ParentMount)
	{
		FStringView RelPath;
		verify(FPathViews::TryMakeChildPathRelativeTo(LocalAbsPath, ParentMount->GetLocalAbsPath(), RelPath));
		ParentMount->AddChildMount(&Mount);
		Mount.SetParentMount(ParentMount);
		for (FMountDir* ChildMount : ChildMounts)
		{
			ParentMount->RemoveChildMount(ChildMount);
		}
	}
	for (FMountDir* ChildMount : ChildMounts)
	{
		Mount.AddChildMount(ChildMount);
		ChildMount->SetParentMount(&Mount);
	}
}

void FAssetDataDiscovery::RemoveMountPoint(const FString& LocalAbsPath)
{
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	FGathererScopeLock TreeScopeLock(&TreeLock);
	SetIsIdle(false);
	RemoveMountPointInternal(LocalAbsPath);
}

void FAssetDataDiscovery::RemoveMountPointInternal(const FString& LocalAbsPath)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TreeLock);
	int32 ExistingIndex = FindLowerBoundMountPoint(LocalAbsPath);
	if (ExistingIndex == MountDirs.Num() || !FPathViews::Equals(MountDirs[ExistingIndex]->GetLocalAbsPath(), LocalAbsPath))
	{
		return;
	}
	TUniquePtr<FMountDir> Mount = MoveTemp(MountDirs[ExistingIndex]);
	MountDirs.RemoveAt(ExistingIndex);
	FMountDir* ParentMount = Mount->GetParentMount();

	if (ParentMount)
	{
		for (FMountDir* ChildMount : Mount->GetChildMounts())
		{
			ParentMount->AddChildMount(ChildMount);
			ChildMount->SetParentMount(ParentMount);
		}
		ParentMount->RemoveChildMount(Mount.Get());
	}
	else
	{
		for (FMountDir* ChildMount : Mount->GetChildMounts())
		{
			ChildMount->SetParentMount(nullptr);
		}
	}
}

void FAssetDataDiscovery::OnDirectoryCreated(FStringView LocalAbsPath)
{
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	FGathererScopeLock TreeScopeLock(&TreeLock);
	FMountDir* MountDir = FindContainingMountPoint(LocalAbsPath);
	if (!MountDir || !MountDir->IsMonitored(LocalAbsPath))
	{
		return;
	}

	FStringView MountRelPath;
	verify(FPathViews::TryMakeChildPathRelativeTo(LocalAbsPath, MountDir->GetLocalAbsPath(), MountRelPath));
	TStringBuilder<128> LongPackageName;
	LongPackageName << MountDir->GetLongPackageName();
	FPathViews::AppendPath(LongPackageName, MountRelPath);
	if (FPackageName::DoesPackageNameContainInvalidCharacters(LongPackageName))
	{
		return;
	}

	// Skip reporting the directory if it is in the deny list of directories to not report
	if (!ShouldDirBeReported(LongPackageName))
	{
		return;
	}

	FDiscoveredPathData DirData;
	DirData.LocalAbsPath = LocalAbsPath;
	DirData.LongPackageName = LongPackageName;
	DirData.RelPath = FPathViews::GetCleanFilename(MountRelPath);

	// Note that we AddDiscovered but do not scan the directory
	// Any files and paths under it will be added by their own event from the directory watcher, so a scan is unnecessary.
	// The directory may also be scanned in the future because a parent directory is still yet pending to scan,
	// we do not try to prevent that wasteful rescan because this is a rare event and it does not cause a behavior problem
	SetIsIdle(false);
	AddDiscovered(DirData.LocalAbsPath, DirData.LongPackageName, TConstArrayView<FDiscoveredPathData>(&DirData, 1), TConstArrayView<FDiscoveredPathData>());
}

void FAssetDataDiscovery::OnFilesCreated(TConstArrayView<FString> LocalAbsPaths)
{
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	FGathererScopeLock TreeScopeLock(&TreeLock);
	SetIsIdle(false);
	for (const FString& LocalAbsPath : LocalAbsPaths)
	{
		OnFileCreated(LocalAbsPath);
	}
}

void FAssetDataDiscovery::OnFileCreated(const FString& LocalAbsPath)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TreeLock);
	// Detect whether the file should be scanned and if so pass it through to the gatherer
	FMountDir* MountDir = FindContainingMountPoint(LocalAbsPath);
	if (!MountDir)
	{
		// The content root of the file is not registered; ignore it
		return;
	}
	FFileStatData StatData = IFileManager::Get().GetStatData(*LocalAbsPath);
	if (!StatData.bIsValid || StatData.bIsDirectory)
	{
		// The caller has erroneously told us a file exists that doesn't exist (perhaps due to create/delete hysteresis); ignore it
		return;
	}

	FString FileRelPath;
	FScanDir::FInherited MonitorData;
	FScanDir* ScanDir = MountDir->GetControllingDir(LocalAbsPath, false /* bIsDirectory */, MonitorData, FileRelPath);
	if (!ScanDir || !MonitorData.IsMonitored())
	{
		// The new file is in an unmonitored directory; ignore it
		return;
	}

	FStringView RelPathFromParentDir = FPathViews::GetCleanFilename(FileRelPath);
	EGatherableFileType FileType = GetFileType(RelPathFromParentDir);
	if (FileType != EGatherableFileType::Invalid)
	{
		FStringView FileRelPathNoExt = FPathViews::GetBaseFilenameWithPath(FileRelPath);
		if (!FPackageName::DoesPackageNameContainInvalidCharacters(FileRelPathNoExt))
		{
			TStringBuilder<256> LongPackageName;
			LongPackageName << MountDir->GetLongPackageName();
			FPathViews::AppendPath(LongPackageName, ScanDir->GetMountRelPath());
			FPathViews::AppendPath(LongPackageName, FileRelPathNoExt);
			const bool bBlocked = FileType == EGatherableFileType::PackageFile && IsPackageBlocked(LocalAbsPath);
			AddDiscoveredFile(FDiscoveredPathData(LocalAbsPath, LongPackageName, RelPathFromParentDir, StatData.ModificationTime, FileType, bBlocked));
			if (FPathViews::IsPathLeaf(FileRelPath))
			{
				ScanDir->MarkFileAlreadyScanned(FileRelPath);
			}
		}
	}
}

FMountDir* FAssetDataDiscovery::FindContainingMountPoint(FStringView LocalAbsPath)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TreeLock);
	int32 Index = FindLowerBoundMountPoint(LocalAbsPath);
	// The LowerBound is >= LocalAbsPath, so it is a parentpath of LocalAbsPath only if it is equal to LocalAbsPath
	if (Index < MountDirs.Num() && FPathViews::Equals(MountDirs[Index]->GetLocalAbsPath(), LocalAbsPath))
	{
		return MountDirs[Index].Get();
	}

	// The last element before the lower bound is either (1) an unrelated path and LocalAbsPath does not have a parent
	// (2) a parent path of LocalAbsPath, (3) A sibling path that is a child of an earlier path that is a parent path of LocalAbsPath
	// (4) An unrelated path that is a child of an earlier path, but none of its parents are a parent path of LocalAbsPath
	// Distinguishing between cases (3) and (4) doesn't have a fast algorithm based on sorted paths alone, but we have recorded the parent
	// so we can figure it out that way
	if (Index > 0)
	{
		FMountDir* Previous = MountDirs[Index - 1].Get();
		while (Previous)
		{
			if (FPathViews::IsParentPathOf(Previous->GetLocalAbsPath(), LocalAbsPath))
			{
				return Previous;
			}
			Previous = Previous->GetParentMount();
		}
	}
	return nullptr;
}

const FMountDir* FAssetDataDiscovery::FindContainingMountPoint(FStringView LocalAbsPath) const
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TreeLock);
	return const_cast<FAssetDataDiscovery*>(this)->FindContainingMountPoint(LocalAbsPath);
}

FMountDir* FAssetDataDiscovery::FindMountPoint(FStringView LocalAbsPath)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TreeLock);
	int32 Index = FindLowerBoundMountPoint(LocalAbsPath);
	if (Index != MountDirs.Num() && FPathViews::Equals(MountDirs[Index]->GetLocalAbsPath(), LocalAbsPath))
	{
		return MountDirs[Index].Get();
	}
	return nullptr;
}

FMountDir& FAssetDataDiscovery::FindOrAddMountPoint(FStringView LocalAbsPath, FStringView LongPackageName)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TreeLock);
	int32 Index = FindLowerBoundMountPoint(LocalAbsPath);
	if (Index != MountDirs.Num() && FPathViews::Equals(MountDirs[Index]->GetLocalAbsPath(), LocalAbsPath))
	{
		// Already exists
		return *MountDirs[Index];
	}
	return *MountDirs.EmplaceAt_GetRef(Index, new FMountDir(*this, LocalAbsPath, LongPackageName));
}

int32 FAssetDataDiscovery::FindLowerBoundMountPoint(FStringView LocalAbsPath) const
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TreeLock);
	return Algo::LowerBound(MountDirs, LocalAbsPath, [](const TUniquePtr<FMountDir>& MountDir, FStringView LocalAbsPath)
		{
			return FPathViews::Less(MountDir->GetLocalAbsPath(), LocalAbsPath);
		}
	);
}

void FAssetDataDiscovery::AddDiscovered(FStringView DirAbsPath, FStringView DirPackagePath, TConstArrayView<FDiscoveredPathData> SubDirs, TConstArrayView<FDiscoveredPathData> Files)
{
	// This function is inside the critical section so we have moved filtering results outside of it
	// Caller is responsible for filtering SubDirs and Files by ShouldScan and packagename validity
	FGathererScopeLock ResultsScopeLock(&ResultsLock);

	if (UE::AssetDataGather::Private::bIgnoreEmptyDirectories)
	{
		// Only register this directory, this will get called for anything that has files or subdirectories
		DiscoveredDirectories.Add(FString(DirPackagePath));
	}
	else 
	{
		// Register all of the subdirectories even if they are empty
		for (const FDiscoveredPathData& SubDir : SubDirs)
		{
			DiscoveredDirectories.Add(FString(SubDir.LongPackageName));
		}
	}

	if (Files.Num())
	{
		DiscoveredFiles.Emplace(DirAbsPath, Files);
		NumDiscoveredFiles += Files.Num();
	}
}

void FAssetDataDiscovery::AddDiscoveredFile(FDiscoveredPathData&& File)
{
	FGathererScopeLock ResultsScopeLock(&ResultsLock);
	DiscoveredSingleFiles.Emplace(MoveTemp(File));
	NumDiscoveredFiles++;
}

EGatherableFileType FAssetDataDiscovery::GetFileType(FStringView FilePath)
{
	return FPackageName::IsPackageFilename(FilePath)
		? EGatherableFileType::PackageFile
		: (FAssetDataGatherer::IsVerseFile(FilePath) ? EGatherableFileType::VerseFile : EGatherableFileType::Invalid);
}

FAssetDataDiscovery::FDirectoryResult::FDirectoryResult(FStringView InDirAbsPath, TConstArrayView<FDiscoveredPathData> InFiles)
	: DirAbsPath(InDirAbsPath)
{
	Files.Reserve(InFiles.Num());
	for (const FDiscoveredPathData& DiscoveredFile : InFiles)
	{
		Files.Emplace(DiscoveredFile); // Emplace passes the FDiscoveredPathData to the FGatheredPathData explicit constructor for it
	}

}
bool FAssetDataDiscovery::ShouldDirBeReported(FStringView LongPackageName) const
{
	return !DirLongPackageNamesToNotReport.ContainsByHash(GetTypeHash(LongPackageName), LongPackageName);
}

// Reads an FMemoryView once
class FMemoryViewReader
{
public:
	FMemoryViewReader() = default;
	FMemoryViewReader(FMemoryView Data) : Remaining(Data), TotalSize(Data.GetSize()) {}

	uint64 GetRemainingSize() const { return Remaining.GetSize(); }
	uint64 GetTotalSize() const { return TotalSize; }
	uint64 Tell() const { return TotalSize - Remaining.GetSize(); }

	FMemoryView Load(uint64 Size)
	{
		check(Size <= Remaining.GetSize());
		FMemoryView Out(Remaining.GetData(), Size);
		Remaining += Size;
		return Out;
	}

	void Load(FMutableMemoryView Out)
	{
		FMemoryView In = Load(Out.GetSize());
		if (In.GetSize())
		{
			FMemory::Memcpy(Out.GetData(), In.GetData(), In.GetSize());
		}
	}

	template<typename T>
	T Load()
	{
		static_assert(std::is_integral_v<T>, "Only integer loading supported");
		static_assert(PLATFORM_LITTLE_ENDIAN, "Byte-swapping not implemented");
		return FPlatformMemory::ReadUnaligned<T>(Load(sizeof(T)).GetData());
	}

	template<typename T>
	TOptional<T> TryLoad()
	{
		return sizeof(T) <= Remaining.GetSize() ? Load<T>() : TOptional<T>();
	}

private:
	FMemoryView Remaining;
	uint64 TotalSize = 0;
};


// Enables both versioning and distinguishing out-of-sync reads from data corruption
static constexpr uint32 BlockMagic = 0xb1a3;

struct FBlockHeader
{
	uint32 Magic = 0;
	uint32 Size = 0;
	uint64 Checksum = 0;
};

TOptional<FBlockHeader> LoadBlockHeader(FMemoryView Data)
{
	check(Data.GetSize() == sizeof(FBlockHeader));

	FMemoryViewReader Reader(Data);
	FBlockHeader Header;
	Header.Magic = Reader.Load<uint32>();
	Header.Size = Reader.Load<uint32>();
	Header.Checksum = Reader.Load<uint64>();

	if (Header.Magic != BlockMagic)
	{
		UE_LOG(LogAssetRegistry, Warning, TEXT("Wrong block magic (0x%x)"), Header.Magic);
		return {};
	}

	return Header;
}

static uint64 CalculateBlockChecksum(FMemoryView Data)
{
	return INTEL_ORDER64(FXxHash64::HashBuffer(Data).Hash);
}

class FChecksumArchiveBase : public FArchiveProxy
{
	static constexpr uint32 SaveBlockSize = 4 << 20;

	struct FBlock
	{
		uint8* Begin = nullptr;
		uint8* Cursor = nullptr;
		uint8* End = nullptr;

		explicit FBlock(uint32 Size)				{ Reset(Size); }
		~FBlock()									{ FMemory::Free(Begin); }
		uint64 GetCapacity() const					{ return End - Begin; }
		uint64 GetUsedSize() const					{ return Cursor - Begin; }
		uint64 GetRemainingSize() const				{ return End - Cursor; }
		FMutableMemoryView GetUsed() const			{ return {Begin, GetUsedSize()}; }
		FMutableMemoryView GetRemaining() const		{ return {Cursor, GetRemainingSize()}; };

		void Reset(uint32 Size)
		{
			if (GetCapacity() < Size)
			{
				FMemory::Free(Begin);
				Begin = (uint8*)FMemory::Malloc(Size);
			}
			
			// All blocks have the same size except the last one, which may be smaller.
			// It doesn't matter that we lose some capacity when loading the last block.
			End = Begin + Size;
			Cursor = Begin;
		}

		void Write(FMemoryView In)
		{
			check(GetRemainingSize() >= In.GetSize());
			if (In.GetSize())
			{
				FMemory::Memcpy(Cursor, In.GetData(), In.GetSize());
			}
			Cursor += In.GetSize();	
		}

		void Read(FMutableMemoryView Out)
		{
			check(GetRemainingSize() >= Out.GetSize());
			if (Out.GetSize())
			{
				FMemory::Memcpy(Out.GetData(), Cursor, Out.GetSize());
			}
			Cursor += Out.GetSize();	
		}
	};

	FBlock Block;

	void SaveBlock()
	{
		FBlockHeader Header;
		Header.Magic = BlockMagic;
		Header.Size = IntCastChecked<uint32>(Block.GetUsedSize());
		Header.Checksum = CalculateBlockChecksum(Block.GetUsed());
		InnerArchive << Header.Magic << Header.Size << Header.Checksum;

		InnerArchive.Serialize(Block.Begin, Header.Size);

		Block.Cursor = Block.Begin;
	}

	bool LoadBlock()
	{
		check(Block.GetRemainingSize() == 0);

		uint8 HeaderData[sizeof(FBlockHeader)];
		InnerArchive.Serialize(HeaderData, sizeof(HeaderData));
		if (InnerArchive.IsError())
		{
			UE_LOG(LogAssetRegistry, Warning, TEXT("Couldn't read block header"));
			return false;
		}

		if (TOptional<FBlockHeader> Header = LoadBlockHeader(MakeMemoryView(HeaderData)))
		{
			Block.Reset(Header->Size);

			InnerArchive.Serialize(Block.Begin, Header->Size);

			if (InnerArchive.IsError())
			{
				UE_LOG(LogAssetRegistry, Warning, TEXT("Couldn't read block data"));
				return false;
			}
			else if (CalculateBlockChecksum(Block.GetRemaining()) != Header->Checksum)
			{
				UE_LOG(LogAssetRegistry, Warning, TEXT("Wrong block checksum"));
				return false;
			}

			return true;
		}

		return false;
	}

public:
	explicit FChecksumArchiveBase(FArchive& Inner)
	: FArchiveProxy(Inner)
	, Block(IsLoading() ? 0 : SaveBlockSize)
	{}

protected:
	~FChecksumArchiveBase()
	{
		if (!IsLoading() && Block.GetUsedSize() > 0)
		{
			SaveBlock();
		}
	}
	const FBlock& GetCurrentBlock() const
	{
		return Block;
	}

	void Save(FMemoryView Data)
	{
		for (uint64 Size = Block.GetRemainingSize(); Size < Data.GetSize(); Size = Block.GetRemainingSize())
		{
			Block.Write(Data.Left(Size));
			Data += Size;
			SaveBlock();
		}

		Block.Write(Data);
	}

	void Load(FMutableMemoryView Data)
	{
		if (IsError())
		{
			return;
		}

		for (uint64 Size = Block.GetRemainingSize(); Size < Data.GetSize(); Size = Block.GetRemainingSize())
		{
			Block.Read(Data.Left(Size));
			Data += Size;

			if (!LoadBlock())
			{
				UE_LOG(LogAssetRegistry, Warning, TEXT("Integrity check failed, '%s' cache will be discarded"), *InnerArchive.GetArchiveName());
				SetError();
				return;
			}
		}

		Block.Read(Data);
	}

public:
	// Use FArchive implementations that map back to Serialize() instead of FArchiveProxy overloads
	// that forward to the inner archive and bypass integrity checking
	virtual FArchive& operator<<(FText& Value) override					{ return FArchive::operator<<(Value); }
	virtual void SerializeBits(void* Bits, int64 LengthBits) override	{ FArchive::SerializeBits(Bits, LengthBits); }
	virtual void SerializeInt(uint32& Value, uint32 Max) override		{ FArchive::SerializeInt(Value, Max); }
	virtual void SerializeIntPacked(uint32& Value) override				{ FArchive::SerializeIntPacked(Value); }

private:
	// Wrapping an inner FArchiveUObject is not supported. The inner archive should be low-level
	// and an outer archive should have intercepted these calls.
	virtual FArchive& operator<<(FName& Value) override					{ unimplemented(); return *this; }
	virtual FArchive& operator<<(UObject*& Value) override				{ unimplemented(); return *this; }
	virtual FArchive& operator<<(FObjectPtr& Value) override			{ unimplemented(); return *this; }
	virtual FArchive& operator<<(FLazyObjectPtr& Value) override		{ unimplemented(); return *this; }
	virtual FArchive& operator<<(FSoftObjectPath& Value) override		{ unimplemented(); return *this; }
	virtual FArchive& operator<<(FSoftObjectPtr& Value) override		{ unimplemented(); return *this; }
	virtual FArchive& operator<<(FWeakObjectPtr& Value) override		{ unimplemented(); return *this; }
	virtual FArchive& operator<<(FField*& Value) override				{ unimplemented(); return *this; }

	virtual void Seek(int64 InPos) override								{ unimplemented(); }
};

class FChecksumArchiveWriter : public FChecksumArchiveBase
{
public:
	using FChecksumArchiveBase::FChecksumArchiveBase;
	virtual void Serialize(void* V, int64 Len) override { Save(FMemoryView(V, Len)); }
	virtual int64 Tell() override { return InnerArchive.Tell() + GetCurrentBlock().GetUsedSize(); }
};

class FChecksumArchiveReader : public FChecksumArchiveBase
{
public:
	using FChecksumArchiveBase::FChecksumArchiveBase;
	virtual void Serialize(void* V, int64 Len) override { Load(FMutableMemoryView(V, Len)); }
	virtual int64 Tell() override { return InnerArchive.Tell() - GetCurrentBlock().GetRemainingSize(); }
};

// Memory-mapped equivalent of FChecksumArchiveReader
class FChecksumViewReader : public FArchive
{
public:
	explicit FChecksumViewReader(FMemoryViewReader&& Reader, FStringView InFileName)
	: RemainingBlocks(MoveTemp(Reader))
	, FileName(InFileName)
	{
		SetIsLoading(true);
	}

private:
	FMemoryViewReader RemainingBlocks;
	FMemoryViewReader CurrentBlock;
	FString FileName;

	virtual void Seek(int64) override { unimplemented(); }
	virtual int64 Tell() override { return RemainingBlocks.Tell() - CurrentBlock.GetRemainingSize(); }
	virtual int64 TotalSize() override { return RemainingBlocks.GetTotalSize(); }

	virtual void Serialize(void* V, int64 Len) override
	{
		FMutableMemoryView Out(V, Len);

		while (CurrentBlock.GetRemainingSize() < Out.GetSize())
		{
			if (IsError())
			{
				return;
			}

			FMutableMemoryView OutSlice(Out.GetData(), CurrentBlock.GetRemainingSize());
			Out += OutSlice.GetSize();
			CurrentBlock.Load(OutSlice);
			check(CurrentBlock.GetRemainingSize() == 0);

			TOptional<FMemoryView> NextBlock = LoadNextBlock(RemainingBlocks);
			if (!NextBlock)
			{
				UE_LOG(LogAssetRegistry, Warning, TEXT("Integrity check failed, '%s' cache will be discarded"), *FileName);
				SetError();
				return;
			}
			CurrentBlock = *NextBlock;
		}

		CurrentBlock.Load(Out);
	}

	FORCENOINLINE static TOptional<FMemoryView> LoadNextBlock(FMemoryViewReader& In)
	{
		if (In.GetRemainingSize() < sizeof(FBlockHeader))
		{
			UE_LOG(LogAssetRegistry, Warning, TEXT("Couldn't read block header"));
			return {};
		}

		if (TOptional<FBlockHeader> Header = LoadBlockHeader(In.Load(sizeof(FBlockHeader))))
		{
			if (Header->Size > In.GetRemainingSize())
			{
				UE_LOG(LogAssetRegistry, Warning, TEXT("Incomplete block"));
				return {};
			}

			FMemoryView Block = In.Load(Header->Size);
			if (CalculateBlockChecksum(Block) != Header->Checksum)
			{
				UE_LOG(LogAssetRegistry, Warning, TEXT("Wrong block checksum"));
				return {};
			}

			return Block;
		}

		return {};
	}
};

// Util that maps an entire file
class FMemoryMappedFile
{
public:
	FMemoryMappedFile(const TCHAR* Path)
		: Handle(FPlatformFileManager::Get().GetPlatformFile().OpenMapped(Path))
		, Region(Handle ? Handle->MapRegion() : nullptr)
	{}

	void Preload(int64 Size = MAX_int64) const
	{
		if (Region)
		{
			Region->PreloadHint(0, Size);
		}
	}
	
	FMemoryView View() const
	{
		return Region ? FMemoryView(Region->GetMappedPtr(), Region->GetMappedSize()) : FMemoryView();
	}

private:
	TUniquePtr<IMappedFileHandle> Handle;
	TUniquePtr<IMappedFileRegion> Region;
};

#if UE_EDITOR
/** A class to preload the monolithic cache used by FAssetDataGatherer. Preloading the cache allows us to
 * start very early in editor startup, so that we have time to finish the cache load before the engine starts
 * making package load requests that need to use the gathered data.
 *
 * In UE_EDITOR, we know the values we need to decide whether we can preload early enough that it is useful to preload
 * In other configurations we do not know those parameters for sure until EDelayedRegisterRunPhase::ShaderTypesReady,
 *  which occurs around the same time as UAssetRegistryImpl is created, so it is not useful to preload.
 */
class FPreloader : public FDelayedAutoRegisterHelper
{
public:
	FPreloader()
		// The callback needs to occur after GIsEditor, ProjectIntermediateDir, IsRunningCommandlet, and
		// IsRunningCookCommandlet have been set
		:FDelayedAutoRegisterHelper(EDelayedRegisterRunPhase::IniSystemReady, [this]() { DelayedInitialize(); }) //-V1099
	{
	}

	~FPreloader()
	{
		// This destructor is called during c++ global shutdown after main, and it is not valid to call TFuture::Wait
		// after Engine shutdown. TaskGraph has already shutdown at this point, so we do not need to worry about the 
		// Async thread still running and accessing *this.
		// PreloadReady.Wait();
	}

	TArray<FCachePayload> Consume()
	{
		check(bInitialized);
		if (bConsumed)
		{
			return {};
		}
		bConsumed = true;
		PreloadReady.Wait();
		PreloadReady.Reset();
		return MoveTemp(Payloads);
	}

private:

	void DelayedInitialize()
	{
		GPreloadSettings.Initialize();

		if (GPreloadSettings.IsPreloadMonolithicCache())
		{
			TArray<FString> CachePaths = GPreloadSettings.FindShardedMonolithicCacheFiles();
			if (CachePaths.Num())
			{
				PreloadReady = Async(EAsyncExecution::TaskGraph, [this, CachePaths=MoveTemp(CachePaths)]() { LoadAsync(CachePaths); });
			}
		}
		bInitialized = true;
	}

	void LoadAsync(const TArray<FString>& Paths)
	{
		LLM_SCOPE(ELLMTag::AssetRegistry);
		Payloads = LoadCacheFiles(Paths, true /* bIsMonolithicCache */);
	}

	TFuture<void> PreloadReady;
	TArray<FCachePayload> Payloads;
	bool bInitialized = false;
	bool bConsumed = false;
};
FPreloader GPreloader;
#endif

} // namespace UE::AssetDataGather::Private

FAssetDataGatherer::FAssetDataGatherer(const TArray<FString>& InLongPackageNamesDenyList,
	const TArray<FString>& InMountRelativePathsDenyList, bool bInAsyncEnabled)
	: Thread(nullptr)
	, bAsyncEnabled(bInAsyncEnabled)
	, GatherStartTime(FDateTime::Now())
	, IsStopped(0)
	, IsPaused(0)
	, bInitialPluginsLoaded(false)
	, bSaveAsyncCacheTriggered(false)
	, CurrentSearchTime(0.)
	, LastCacheWriteTime(0.0)
	, bHasLoadedMonolithicCache(false)
	, bDiscoveryIsComplete(false)
	, bIsComplete(false)
	, bIsIdle(false)
	, bFirstTickAfterIdle(true)
	, bFinishedInitialDiscovery(false)
	, WaitBatchCount(-1)
	, LastMonolithicCacheSaveUncachedAssetFiles(0)
	, CacheInUseCount(0)
	, bIsSavingAsyncCache(false)
	, bFlushedRetryFiles(false)
{
	using namespace UE::AssetDataGather::Private;

	TickInternalBatchSize = FMath::Max(1, FTaskGraphInterface::Get().GetNumWorkerThreads()) * AssetDataGathererConstants::SingleThreadFilesPerBatch;

	GPreloadSettings.Initialize();
	bGatherAssetPackageData = GIsEditor || GPreloadSettings.IsForceDependsGathering();
	bGatherDependsData = GPreloadSettings.IsGatherDependsData();
	bCacheReadEnabled = GPreloadSettings.IsGatherCacheReadEnabled();
	bCacheWriteEnabled = GPreloadSettings.IsGatherCacheWriteEnabled();
	// If IsMonolithicCacheActivatedDuringPreload is true, we are already instructed to use the MonolithicCache.
	// Otherwise it may be set to true later if game/commandlet calls SearchAllAssets.
	bReadMonolithicCache = bCacheReadEnabled && GPreloadSettings.IsMonolithicCacheActivatedDuringPreload();
	bWriteMonolithicCache = bCacheWriteEnabled && GPreloadSettings.IsMonolithicCacheActivatedDuringPreload();
	LastCacheWriteTime = FPlatformTime::Seconds();

#if WITH_EDITOR || !UE_BUILD_SHIPPING
	bool bCommandlineSynchronous;
	if (FParse::Bool(FCommandLine::Get(), TEXT("AssetGatherSync="), bCommandlineSynchronous))
	{
		bAsyncEnabled = !bCommandlineSynchronous;
	}
#endif // WITH_EDITOR || !UE_BUILD_SHIPPING
	if (bAsyncEnabled && !FPlatformProcess::SupportsMultithreading())
	{
		bAsyncEnabled = false;
		UE_LOG(LogAssetRegistry, Warning, TEXT("Requested asynchronous asset data gather, but threading support is disabled. Performing a synchronous gather instead!"));
	}
	// Tick is Synchronous until StartAsync is called, even if bAsyncEnabled
	bSynchronousTick = true; 

	Discovery = MakeUnique<UE::AssetDataGather::Private::FAssetDataDiscovery>(InLongPackageNamesDenyList,
		InMountRelativePathsDenyList, bAsyncEnabled);
	FilesToSearch = MakeUnique<UE::AssetDataGather::Private::FFilesToSearch>();
	FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddRaw(this, &FAssetDataGatherer::OnAllModuleLoadingPhasesComplete);
}

FAssetDataGatherer::~FAssetDataGatherer()
{
	FCoreDelegates::OnAllModuleLoadingPhasesComplete.RemoveAll(this);
	EnsureCompletion();
	NewCachedAssetDataMap.Empty();
	DiskCachedAssetDataMap.Empty();

	for (FDiskCachedAssetData* AssetData : NewCachedAssetData)
	{
		delete AssetData;
	}
	NewCachedAssetData.Empty();
	for (TPair<int32, FDiskCachedAssetData*> BlockData : DiskCachedAssetBlocks)
	{
		delete[] BlockData.Get<1>();
	}
	DiskCachedAssetBlocks.Empty();
}

void FAssetDataGatherer::OnInitialSearchCompleted()
{
	if (Discovery)
	{
		Discovery->OnInitialSearchCompleted();
	}
}

void FAssetDataGatherer::ActivateMonolithicCache()
{
	FGathererScopeLock ResultsScopeLock(&ResultsLock);
	bool bNewWriteMonolithicCache = bCacheWriteEnabled;
	bool bNewReadMonolithicCache = bCacheReadEnabled;
	if ((!bNewWriteMonolithicCache || bWriteMonolithicCache) &&
		(!bNewReadMonolithicCache || bReadMonolithicCache))
	{
		return;
	}

	bWriteMonolithicCache = bNewWriteMonolithicCache;
	bReadMonolithicCache = bNewReadMonolithicCache;
	LastCacheWriteTime = FPlatformTime::Seconds();
}

void FAssetDataGatherer::StartAsync()
{
	if (bAsyncEnabled && !Thread)
	{
		Thread = FRunnableThread::Create(this, TEXT("FAssetDataGatherer"), 0, TPri_BelowNormal);
		checkf(Thread, TEXT("Failed to create asset data gatherer thread"));
		Discovery->StartAsync();
		bSynchronousTick = false;
	}
}

bool FAssetDataGatherer::Init()
{
	return true;
}

uint32 FAssetDataGatherer::Run()
{
	constexpr float IdleSleepTime = 0.1f;
	constexpr float PausedSleepTime = 0.005f;
	LLM_SCOPE(ELLMTag::AssetRegistry);

	while (!IsStopped)
	{
		InnerTickLoop(false /* bInSynchronousTick */, true /* bContributeToCacheSave */, -1. /* EndTimeSeconds */);

		for (;;)
		{
			bool bLocalIdle = false;
			{
				FGathererScopeLock ResultsScopeLock(&ResultsLock); // bIsIdle requires the lock
				if (IsStopped || bSaveAsyncCacheTriggered || (!IsPaused && !bIsIdle))
				{
					break;
				}
				bLocalIdle = bIsIdle;
			}
			// No work to do. Sleep for a little and try again later.
			// TODO: Need IsPaused to be a condition variable so we avoid sleeping while waiting for it and then taking a long time to wake after it is unset.
			FPlatformProcess::Sleep(bLocalIdle ? IdleSleepTime : PausedSleepTime);
		}
	}
	return 0;
}

void FAssetDataGatherer::InnerTickLoop(bool bInSynchronousTick, bool bContributeToCacheSave, double EndTimeSeconds)
{
	using namespace UE::AssetDataGather::Private;

	// Synchronous ticks during Wait contribute to saving of the async cache only if there is no dedicated async thread to do it (Thread == nullptr)
	// The dedicated async thread always contributes
	bContributeToCacheSave = !bInSynchronousTick || (Thread == nullptr && bContributeToCacheSave);

	bool bShouldSaveMonolithicCache = false;
	TArray<TPair<FName, FDiskCachedAssetData*>> AssetsToSave;
	{
		CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
		FGathererScopeLock RunScopeLock(&TickLock);
		TGuardValue<bool> ScopeSynchronousTick(bSynchronousTick, bInSynchronousTick);
		TRACE_CPUPROFILER_EVENT_SCOPE(FAssetDataGatherer::Tick);
		double TickStartTime = FPlatformTime::Seconds();
		bool bPollDiscovery = true;
		constexpr float PollDiscoveryPeriodSeconds = .005f;
		double LastPollTimeSeconds = 0;

		for (;;)
		{
			ETickResult TickResult = TickInternal(TickStartTime, bPollDiscovery);
			if (EndTimeSeconds > 0. && FPlatformTime::Seconds() > EndTimeSeconds)
			{
				break;
			}
			if (IsStopped || (!bInSynchronousTick && IsPaused))
			{
				break;
			}
			if (TickResult != ETickResult::KeepTicking && TickResult != ETickResult::PollDiscovery)
			{
				break;
			}
			double CurrentTimeSeconds = FPlatformTime::Seconds();
			if (bPollDiscovery)
			{
				LastPollTimeSeconds = CurrentTimeSeconds;
			}
			if (TickResult == ETickResult::KeepTicking)
			{
				// Poll discovery every so often to reduce super-linear costs
				bPollDiscovery =
					static_cast<float>(CurrentTimeSeconds - LastPollTimeSeconds) > PollDiscoveryPeriodSeconds;
			}
			else if (!bPollDiscovery)
			{
				bPollDiscovery = true;
			}
			else
			{
				// We just polled discovery; go to sleep to wait for the discovery thread rather than busy-spinning and
				// causing contention on its critical section
				float SleepTimeSeconds = PollDiscoveryPeriodSeconds;
				if (EndTimeSeconds > 0.)
				{
					SleepTimeSeconds = FMath::Min(SleepTimeSeconds,
						static_cast<float>(EndTimeSeconds - CurrentTimeSeconds));
				}
				if (SleepTimeSeconds > 0.f)
				{
					if (TickStartTime >= 0.)
					{
						CurrentSearchTime += CurrentTimeSeconds - TickStartTime;
					}
					FPlatformProcess::Sleep(SleepTimeSeconds);
					TickStartTime = FPlatformTime::Seconds();
				}
			}
		}
		if (TickStartTime >= 0.)
		{
			CurrentSearchTime += FPlatformTime::Seconds() - TickStartTime;
			TickStartTime = 0.;
		}

		if (bContributeToCacheSave)
		{
			TryReserveSaveMonolithicCache(bShouldSaveMonolithicCache, AssetsToSave);
			if (bShouldSaveMonolithicCache)
			{
				CacheInUseCount++;
			}
		}
	}
	if (bShouldSaveMonolithicCache)
	{
		SaveMonolithicCacheFile(AssetsToSave);
	}
}

void FAssetDataGatherer::SaveMonolithicCacheFile(const TArray<TPair<FName,FDiskCachedAssetData*>>& AssetsToSave)
{
	using namespace UE::AssetDataGather::Private;
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("Save Monolithic Cache")
	
	// If we recently saved or loaded the file then pause for 0.5 seconds before trying to save on
	// top of it, to avoid failure to be able to delete the file we just saved/loaded.
	double LocalLastCacheWriteTime;
	{
		FGathererScopeLock ResultsScopeLock(&ResultsLock);
		LocalLastCacheWriteTime = LastCacheWriteTime;
	}
	double CurrentTime = FPlatformTime::Seconds();
	constexpr float WaitTimeBeforeReopen = 0.5f;
	if (CurrentTime < LastCacheWriteTime + WaitTimeBeforeReopen)
	{
		FPlatformProcess::Sleep(static_cast<float>(LastCacheWriteTime + WaitTimeBeforeReopen - CurrentTime));
	}

	const int32 MaxShards = FPlatformMisc::NumberOfCores();
	constexpr int32 CacheShardAssetCount = AssetDataGathererConstants::CacheShardAssetCount;
	const int32 CacheShards = FMath::RoundUpToPowerOfTwo(FMath::Min((AssetsToSave.Num() + CacheShardAssetCount-1) / CacheShardAssetCount, MaxShards));
	const int32 ShardMask = CacheShards - 1;
	std::atomic<int64> TotalCacheSize{0};

	TArray<TArray<TPair<FName, FDiskCachedAssetData*>>> DataPerShard;
	{
		// Hash package names by string rather than by name id so that cache shard is maintained across runs in case of interruption
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("Build Shards")
		DataPerShard.AddDefaulted(CacheShards);
		for (TArray<TPair<FName, FDiskCachedAssetData*>>& ShardData : DataPerShard)
		{
			ShardData.Reserve(AssetsToSave.Num() / CacheShards);	
		}
		TStringBuilder<256> Buffer;
		for (const TPair<FName, FDiskCachedAssetData*>& Entry : AssetsToSave)
		{
			Buffer.Reset();
			Buffer << Entry.Key;
			// Force to lower case to maintain ordering across runs where fname casing changes because of load order
			// Hash doesn't need to be persistent across utf-8 transition so no need to re-encode strings
			for (int32 i=0; i < Buffer.Len(); ++i)
			{
				Buffer.GetData()[i] = FChar::ToLower(Buffer.GetData()[i]);
			}
			uint64 Hash = CityHash64((const char*)Buffer.GetData(), Buffer.Len() * sizeof(TCHAR));
			int32 Index = (Hash & ShardMask);
			DataPerShard[Index].Add(Entry);
		}
	}

	ParallelFor(CacheShards, [this, &AssetsToSave, &TotalCacheSize, &DataPerShard, ShardMask](int32 Shard) 
	{
		TArray<TPair<FName, FDiskCachedAssetData*>>& ShardData = DataPerShard[Shard];
		FString Filename = FString::Printf(TEXT("%s_%d.bin"), *GPreloadSettings.GetMonolithicCacheBaseFilename(), Shard);
		int64 CacheSize = SaveCacheFileInternal(Filename, ShardData);
		TotalCacheSize += CacheSize;
	}, EParallelForFlags::BackgroundPriority);

	UE_LOG(LogAssetRegistry, Display, TEXT("Asset registry cache written as %.1f MiB to %s_*.bin"), 
		static_cast<float>(TotalCacheSize)/1024.f/1024.f, *GPreloadSettings.GetMonolithicCacheBaseFilename());
	// Delete old monolithic cache file if it exists
	FString OldMonolithicName = GPreloadSettings.GetLegacyMonolithicCacheFilename();
	IFileManager::Get().Delete(*OldMonolithicName, /* bRequireExists */ false, /* EvenReadOnly */ true, /* Quiet */ true);
	
	// Delete any other shards if number of shards was reduced
	TArray<FString> CacheFiles = GPreloadSettings.FindShardedMonolithicCacheFiles();
	for (const FString& CacheFile : CacheFiles)
	{
		FStringView BaseName = FPathViews::GetBaseFilename(CacheFile);
		if (int32 Index; BaseName.FindLastChar('_', Index))
		{
			BaseName.MidInline(Index+1);
			int32 Suffix = 0;
			LexFromString(Suffix, BaseName);
			if (Suffix >= CacheShards)
			{
				IFileManager::Get().Delete(*CacheFile);
			}
		}
	}

	FScopedPause ScopedPause(*this);
	FGathererScopeLock TickScopeLock(&TickLock);
	bIsSavingAsyncCache = false;
	check(CacheInUseCount > 0);
	CacheInUseCount--;
	FGathererScopeLock ResultsScopeLock(&ResultsLock);
	LastCacheWriteTime = FPlatformTime::Seconds();
}

FAssetDataGatherer::FScopedPause::FScopedPause(const FAssetDataGatherer& InOwner)
	:Owner(InOwner)
{
	if (!Owner.IsSynchronous())
	{
		Owner.IsPaused++;
	}
}

FAssetDataGatherer::FScopedPause::~FScopedPause()
{
	if (!Owner.IsSynchronous())
	{
		check(Owner.IsPaused > 0)
		Owner.IsPaused--;
	}
}

void FAssetDataGatherer::Stop()
{
	Discovery->Stop();
	IsStopped++;
}

void FAssetDataGatherer::Exit()
{
}

bool FAssetDataGatherer::IsAsyncEnabled() const
{
	return bAsyncEnabled;
}

bool FAssetDataGatherer::IsSynchronous() const
{
	return Thread == nullptr;
}

void FAssetDataGatherer::EnsureCompletion()
{
	Discovery->EnsureCompletion();

	Stop();

	if (Thread != nullptr)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

FAssetDataGatherer::ETickResult FAssetDataGatherer::TickInternal(double& TickStartTime, bool bPollDiscovery)
{
	using namespace UE::AssetDataGather::Private;

	typedef TInlineAllocator<AssetDataGathererConstants::ExpectedMaxBatchSize> FBatchInlineAllocator;

	TArray<FGatheredPathData, FBatchInlineAllocator> LocalFilesToSearch;
	TArray<FAssetData*, FBatchInlineAllocator> LocalAssetResults;
	TArray<FPackageDependencyData, FBatchInlineAllocator> LocalDependencyResults;
	TArray<FString, FBatchInlineAllocator> LocalCookedPackageNamesWithoutAssetDataResults;
	TArray<FName, FBatchInlineAllocator> LocalVerseResults;
	TArray<FString, FBatchInlineAllocator> LocalBlockedResults;
	bool bLoadMonolithicCache = false;
	bool bLocalIsCacheWriteEnabled = false;
	double LocalLastCacheWriteTime = 0.0;
	bool bWaitBatchCountDecremented = false;
	ETickResult TickResult = ETickResult::KeepTicking;

	{
		FGathererScopeLock ResultsScopeLock(&ResultsLock);

		if (bFirstTickAfterIdle)
		{
			bFirstTickAfterIdle = false;
			LastCacheWriteTime = FPlatformTime::Seconds();
		}

		if (bPollDiscovery)
		{
			IngestDiscoveryResults();
		}
		if (bInitialPluginsLoaded && !bFlushedRetryFiles)
		{
			bFlushedRetryFiles = true;
			FilesToSearch->RetryLaterRetryFiles();
		}

		// Take a batch off of the work list. If we're waiting only on the first WaitBatchCount results don't take more than that
		int32 NumToProcess = FMath::Min<int32>(TickInternalBatchSize - LocalFilesToSearch.Num(), FilesToSearch->GetNumAvailable());
		// If no work is available mark idle and exit
		if (NumToProcess == 0)
		{
			if (WaitBatchCount != -1)
			{
				WaitBatchCount = -1; // WaitBatchCount was set higher than FilesToSearch->GetNumAvailable(), mark it completed
				TickResult = ETickResult::Interrupt;
			}

			if (bDiscoveryIsComplete)
			{
				if (TickResult == ETickResult::KeepTicking)
				{
					TickResult = ETickResult::Idle;
				}
				const bool bWasInitialDiscoveryFinished = bFinishedInitialDiscovery;
				SetIsIdle(true, TickStartTime);
				if (!bWasInitialDiscoveryFinished && bFinishedInitialDiscovery)
				{
					UE_LOG(LogAssetRegistry, Display, TEXT("Triggering cache save on discovery complete"));
					bSaveAsyncCacheTriggered = true;
				}
			}
			else if (TickResult == ETickResult::KeepTicking)
			{
				TickResult = ETickResult::PollDiscovery;
			}

			return TickResult;
		}

		if (WaitBatchCount >= 0)
		{
			if (WaitBatchCount == 0)
			{
				// We've finished executing the caller's requested batchcount (and we have restored idle if we
				// are idle), so exit now without doing any further work.
				WaitBatchCount = -1;
				return ETickResult::Interrupt;
			}

			// Otherwise we still have some work to do for the caller's requested batchcount, so do work up to
			// that batchcount.
			bWaitBatchCountDecremented = true;
			NumToProcess = FMath::Min(NumToProcess, WaitBatchCount);
			WaitBatchCount -= NumToProcess;
			if (WaitBatchCount == 0)
			{
				// Mark that WaitBatchCount has been consumed and is no longer active
				WaitBatchCount = -1;
				TickResult = ETickResult::Interrupt;
			}
		}
		DependencyResults.Reserve(FilesToSearch->GetNumAvailable() + DependencyResults.Num());
		check(NumToProcess > 0);
		FilesToSearch->PopFront(LocalFilesToSearch, NumToProcess);
		check(LocalFilesToSearch.Num() > 0);

		if (bReadMonolithicCache && !bHasLoadedMonolithicCache)
		{
			bLoadMonolithicCache = true;
		}
		LocalLastCacheWriteTime = LastCacheWriteTime;
	}
	bLocalIsCacheWriteEnabled = bCacheWriteEnabled;

	// Load the async cache if not yet loaded
	if (bLoadMonolithicCache)
	{
		double CacheLoadStartTime = FPlatformTime::Seconds();
		TArray<FCachePayload> Payloads;
#if UE_EDITOR
		if (GPreloadSettings.IsPreloadMonolithicCache())
		{
			Payloads = GPreloader.Consume();
		}
		else
#endif
		{
			TArray<FString> CachePaths = GPreloadSettings.FindShardedMonolithicCacheFiles();
			Payloads = UE::AssetDataGather::Private::LoadCacheFiles(CachePaths, true /* bIsMonolithicCache */);
		}
		ConsumeCacheFiles(MoveTemp(Payloads));
		UE_LOG(LogAssetRegistry, Display, TEXT("AssetDataGatherer spent %.3fs loading caches %s_*.bin."),
			FPlatformTime::Seconds() - CacheLoadStartTime, *GPreloadSettings.GetMonolithicCacheBaseFilename());

		FGathererScopeLock ResultsScopeLock(&ResultsLock);
		bHasLoadedMonolithicCache = true;

		// After we load the monolithic cache, restart the write timer for it. We don't need to save to if we just
		// finished loading it (which we do before gathering anything) and we want to avoid failure to save due to
		// writing a file that we just closed a readhandle for.
		LastCacheWriteTime = FPlatformTime::Seconds();
		LocalLastCacheWriteTime = LastCacheWriteTime;
	}

	struct FReadContext
	{
		FName PackageName;
		FName Extension;
		FGatheredPathData& AssetFileData;
		TArray<FAssetData*> AssetDataFromFile;
		FPackageDependencyData DependencyData;
		TArray<FString> CookedPackageNamesWithoutAssetData;
		bool bCanAttemptAssetRetry = false;
		bool bResult = false;
		bool bCanceled = false;

		FReadContext(FName InPackageName, FName InExtension, FGatheredPathData& InAssetFileData)
			: PackageName(InPackageName)
			, Extension(InExtension)
			, AssetFileData(InAssetFileData)
		{
		}
	};

	int32 NewCachedAssetFiles = 0;
	int32 NewUncachedAssetFiles = 0;
	// Try to read each file in the batch out of the cache, and accumulate a list for more expensive reading of all of the files that are not in the cache 
	TArray<FReadContext> ReadContexts;
	for (FGatheredPathData& AssetFileData : LocalFilesToSearch)
	{
		// If this a Verse source file, just directly add its file name to the Verse results
		if (AssetFileData.Type == EGatherableFileType::VerseFile)
		{
			// Store Verse results in a hybrid format using the LongPackageName but keeping the extension
			LocalVerseResults.Emplace(WriteToString<256>(AssetFileData.LongPackageName, FPathViews::GetExtension(AssetFileData.LocalAbsPath, true)));
			continue;
		}

		if (AssetFileData.Type != EGatherableFileType::PackageFile)
		{
			ensureMsgf(false, TEXT("Encountered unrecognized gathered asset %s!"), *AssetFileData.LongPackageName);
			continue;
		}

		// If this is a blocked package, just directly add its file path to the Blocked results
		if (AssetFileData.bBlocked)
		{
			LocalBlockedResults.Add(AssetFileData.LocalAbsPath);
			continue;
		}

		const FName PackageName = *AssetFileData.LongPackageName;
		const FName Extension = FName(*FPaths::GetExtension(AssetFileData.LocalAbsPath));

		FDiskCachedAssetData** DiskCachedAssetDataPtr = DiskCachedAssetDataMap.Find(PackageName);
		FDiskCachedAssetData* DiskCachedAssetData = DiskCachedAssetDataPtr ? *DiskCachedAssetDataPtr : nullptr;
		if (DiskCachedAssetData)
		{
			// Check whether we need to invalidate the cached data
			const FDateTime& CachedTimestamp = DiskCachedAssetData->ModificationTime;
			if (AssetFileData.PackageTimestamp != CachedTimestamp)
			{
				DiskCachedAssetData = nullptr;
			}
			else if ((!DiskCachedAssetData->DependencyData.PackageName.IsEqual(PackageName, ENameCase::CaseSensitive) && DiskCachedAssetData->DependencyData.PackageName != NAME_None) ||
				DiskCachedAssetData->Extension != Extension)
			{
				UE_LOG(LogAssetRegistry, Display, TEXT("Cached dependency data for package '%s' is invalid. Discarding cached data."), *PackageName.ToString());
				DiskCachedAssetData = nullptr;
			}
		}

		if (DiskCachedAssetData)
		{
			// Add the valid cached data to our results, and to the map of data we keep to write out the new version of the cache file
			++NewCachedAssetFiles;

			// Set the transient flags based on whether our current cache has dependency data.
			// Note that in editor, bGatherAssetPackageData is always true, no way to turn it off,
			// and in game it is always equal to bGatherDependsData, so it can share the cache with dependency data.
			DiskCachedAssetData->DependencyData.bHasPackageData = bGatherAssetPackageData;
			DiskCachedAssetData->DependencyData.bHasDependencyData = bGatherDependsData;

			LocalAssetResults.Reserve(LocalAssetResults.Num() + DiskCachedAssetData->AssetDataList.Num());
			for (const FAssetData& AssetData : DiskCachedAssetData->AssetDataList)
			{
				LocalAssetResults.Add(new FAssetData(AssetData));
			}

			LocalDependencyResults.Add(DiskCachedAssetData->DependencyData);

			AddToCache(PackageName, DiskCachedAssetData);
		}
		else
		{
			// Not found in cache (or stale) - schedule to be read from disk
			ReadContexts.Emplace(PackageName, Extension, AssetFileData);
		}
	}

	// For all the files not found in the cache, read them from their package files on disk; the file reads are done in parallel
	static const EParallelForFlags ParallelFlags = []()
		{
			EParallelForFlags ReturnFlags = EParallelForFlags::Unbalanced;
			if (!IsRunningCommandlet())
			{
				ReturnFlags |= EParallelForFlags::BackgroundPriority;
			}
			return ReturnFlags;
		}();
	ParallelFor(ReadContexts.Num(),
		[this, &ReadContexts](int32 Index)
		{
			FReadContext& ReadContext = ReadContexts[Index];
			if (!bSynchronousTick && IsPaused)
			{
				ReadContext.bCanceled = true;
				return;
			}
			UE_SCOPED_IO_ACTIVITY(*WriteToString<512>(TEXT("Loading Asset"), ReadContext.PackageName.ToString()));
			ReadContext.bResult = ReadAssetFile(ReadContext.AssetFileData.LongPackageName, ReadContext.AssetFileData.LocalAbsPath, ReadContext.AssetDataFromFile, ReadContext.DependencyData, ReadContext.CookedPackageNamesWithoutAssetData, ReadContext.bCanAttemptAssetRetry);
		},
		ParallelFlags
	);

	// Accumulate the results
	bool bHasCancelation = false;
	for (FReadContext& ReadContext : ReadContexts)
	{
		if (ReadContext.bCanceled)
		{
			bHasCancelation = true;
		}
		else if (ReadContext.bResult)
		{
			++NewUncachedAssetFiles;

			// Add the results from a cooked package into our results on cooked package
			LocalCookedPackageNamesWithoutAssetDataResults.Append(MoveTemp(ReadContext.CookedPackageNamesWithoutAssetData));
			// Do not add the results from a cooked package into the map of data we keep to write out the new version of the cache file
			bool bCachePackage = bLocalIsCacheWriteEnabled
				&& LocalCookedPackageNamesWithoutAssetDataResults.Num() == 0
				&& ensure(ReadContext.AssetFileData.Type == EGatherableFileType::PackageFile);
			if (bCachePackage)
			{
				for (const FAssetData* AssetData : ReadContext.AssetDataFromFile)
				{
					if (!!(AssetData->PackageFlags & PKG_FilterEditorOnly))
					{
						bCachePackage = false;
						break;
					}
				}
			}

			// Add the results from non-cooked packages into the map of data we keep to write out the new version of the cache file 
			if (bCachePackage)
			{
				// Update the cache
				FDiskCachedAssetData* NewData = new FDiskCachedAssetData(ReadContext.AssetFileData.PackageTimestamp, GatherStartTime, ReadContext.Extension);
				NewData->AssetDataList.Reserve(ReadContext.AssetDataFromFile.Num());
				for (const FAssetData* BackgroundAssetData : ReadContext.AssetDataFromFile)
				{
					NewData->AssetDataList.Add(*BackgroundAssetData);
				}

				// MoveTemp only used if we don't need DependencyData anymore
				NewData->DependencyData = ReadContext.DependencyData;

				NewCachedAssetData.Add(NewData);
				AddToCache(ReadContext.PackageName, NewData);
			}

			// Add the results from the package into our output results
			LocalAssetResults.Append(MoveTemp(ReadContext.AssetDataFromFile));
			LocalDependencyResults.Add(MoveTemp(ReadContext.DependencyData));
		}
		else if (ReadContext.bCanAttemptAssetRetry)
		{
			// If the read temporarily failed, return it to the worklist, pushed to the end
			FGathererScopeLock ResultsScopeLock(&ResultsLock);
			FilesToSearch->AddFileForLaterRetry(MoveTemp(ReadContext.AssetFileData));
		}
	}

	{
		FGathererScopeLock ResultsScopeLock(&ResultsLock);

		// Submit the results into the thread-shared lists
		AssetResults.Append(MoveTemp(LocalAssetResults));
		DependencyResults.Append(MoveTemp(LocalDependencyResults));
		CookedPackageNamesWithoutAssetDataResults.Append(MoveTemp(LocalCookedPackageNamesWithoutAssetDataResults));
		VerseResults.Append(MoveTemp(LocalVerseResults));
		BlockedResults.Append(MoveTemp(LocalBlockedResults));

		NumUncachedAssetFiles += NewUncachedAssetFiles;
		NumCachedAssetFiles += NewCachedAssetFiles;

		if (bHasCancelation)
		{
			// If we skipped reading files due to a pause request, push the canceled files back onto the FilesToSearch
			for (int Index = ReadContexts.Num() - 1; Index >= 0; --Index) // AddToFront in reverse order so that the elements are readded in the same order they were popped
			{
				FReadContext& ReadContext = ReadContexts[Index];
				if (ReadContext.bCanceled)
				{
					FilesToSearch->AddFileAgainAfterTimeout(MoveTemp(ReadContext.AssetFileData));
					if (bWaitBatchCountDecremented)
					{
						// Restore WaitBatchCount to 0 if we marked it done by setting it to -1
						WaitBatchCount = FMath::Max(0, WaitBatchCount);
						// Then add back on the work that we thought we were going to do but didn't get to
						++WaitBatchCount;
					}
				}
			}
		}

		const int32 NumAssetsReadSinceLastCacheWrite = NumUncachedAssetFiles - LastMonolithicCacheSaveUncachedAssetFiles;
		if (bWriteMonolithicCache && !bIsSavingAsyncCache 
			&& FPlatformTime::Seconds() - LocalLastCacheWriteTime >= AssetDataGathererConstants::MinSecondsToElapseBeforeCacheWrite
			&& NumAssetsReadSinceLastCacheWrite >= AssetDataGathererConstants::MinAssetReadsBeforeCacheWrite)
		{
			bSaveAsyncCacheTriggered = true;
			TickResult = ETickResult::Interrupt;
		}
	}
	return TickResult;
}

void FAssetDataGatherer::IngestDiscoveryResults()
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TickLock);
	CHECK_IS_LOCKED_CURRENT_THREAD(ResultsLock);
	Discovery->GetAndTrimSearchResults(bDiscoveryIsComplete, DiscoveredPaths, *FilesToSearch, NumPathsToSearchAtLastSyncPoint);
}

bool FAssetDataGatherer::ReadAssetFile(const FString& AssetLongPackageName, const FString& AssetFilename, TArray<FAssetData*>& AssetDataList, FPackageDependencyData& DependencyData, TArray<FString>& CookedPackageNamesWithoutAssetData, bool& OutCanRetry) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FAssetDataGatherer::ReadAssetFile);
	OutCanRetry = false;
	AssetDataList.Reset();

	FPackageReader PackageReader;

	FPackageReader::EOpenPackageResult OpenPackageResult;
	if (!PackageReader.OpenPackageFile(AssetLongPackageName, AssetFilename, &OpenPackageResult))
	{
		// If we're missing a custom version, we might be able to load this package later once the module containing that version is loaded...
		//   -	We can only attempt a retry in editors (not commandlets) that haven't yet finished initializing (!GIsRunning), as we 
		//		have no guarantee that a commandlet or an initialized editor is going to load any more modules/plugins
		const bool bAllowRetry = GIsEditor && !bInitialPluginsLoaded;
		if (OpenPackageResult == FPackageReader::EOpenPackageResult::CustomVersionMissing)
		{
			OutCanRetry = bAllowRetry;
			if (!bAllowRetry)
			{
				UE_LOG(LogAssetRegistry, Display, TEXT("Package %s uses an unknown custom version and cannot be loaded for the AssetRegistry"), *AssetFilename);
			}
		}
		else
		{
			OutCanRetry = false;
		}
		return false;
	}
	else
	{
		return ReadAssetFile(PackageReader, AssetDataList, DependencyData, CookedPackageNamesWithoutAssetData,
			(bGatherAssetPackageData ? FPackageReader::EReadOptions::PackageData : FPackageReader::EReadOptions::None) |
			(bGatherDependsData ? FPackageReader::EReadOptions::Dependencies : FPackageReader::EReadOptions::None));
	}
}

bool FAssetDataGatherer::ReadAssetFile(FPackageReader& PackageReader, TArray<FAssetData*>& AssetDataList,
	FPackageDependencyData& DependencyData, TArray<FString>& CookedPackageNamesWithoutAssetData, FPackageReader::EReadOptions Options)
{
	bool bOutIsCookedWithoutAssetData;
	if (!PackageReader.ReadAssetRegistryData(AssetDataList, bOutIsCookedWithoutAssetData))
	{
		return false;
	}
	if (bOutIsCookedWithoutAssetData)
	{
		CookedPackageNamesWithoutAssetData.Add(PackageReader.GetLongPackageName());
	}

	if (!PackageReader.ReadDependencyData(DependencyData, Options))
	{
		return false;
	}

	if (PackageReader.UEVer() >= VER_UE4_ASSETREGISTRY_DEPENDENCYFLAGS && PackageReader.UEVer() < VER_UE4_CORRECT_LICENSEE_FLAG)
	{
		if (EnumHasAnyFlags(Options, FPackageReader::EReadOptions::Dependencies))
		{
			// In version VER_UE4_ASSETREGISTRY_DEPENDENCYFLAGS, UObjectRedirectors were incorrectly saved as having
			// editor-only imports, since UObjectRedirector is an editor-only class. But UObjectRedirectors are
			// followed during cooking and so their imports should be considered used-in-game. SavePackage was fixed
			// to save them as in-game imports by adding HasNonEditorOnlyReferences; the next version bump after that
			// fix was VER_UE4_CORRECT_LICENSEE_FLAG. Mark all dependencies in the affected version as used in game
			// if the package has a UObjectRedirector object.
			FTopLevelAssetPath RedirectorClassPathName = UObjectRedirector::StaticClass()->GetClassPathName();
			if (Algo::AnyOf(AssetDataList, [RedirectorClassPathName](FAssetData* AssetData) { return AssetData->AssetClassPath == RedirectorClassPathName; }))
			{
				for (FPackageDependencyData::FPackageDependency& Dependency : DependencyData.PackageDependencies)
				{
					Dependency.Property |= UE::AssetRegistry::EDependencyProperty::Game;
				}
			}
		}
	}

	return true;
}

void FAssetDataGatherer::AddToCache(FName PackageName, FDiskCachedAssetData* DiskCachedAssetData)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TickLock);
	FDiskCachedAssetData*& ValueInMap = NewCachedAssetDataMap.FindOrAdd(PackageName, DiskCachedAssetData);
	if (ValueInMap != DiskCachedAssetData)
	{
		// An updated DiskCachedAssetData for the same package; replace the existing DiskCachedAssetData with the new one.
		// Note that memory management of the DiskCachedAssetData is handled in a separate structure; we do not need to delete the old value here.
		if (DiskCachedAssetData->Extension != ValueInMap->Extension)
		{
			// Two files with the same package name but different extensions, e.g. basename.umap and basename.uasset
			// This is invalid - some systems in the engine (Cooker's FPackageNameCache) assume that package : filename is 1 : 1 - so issue a warning
			// Because it is invalid, we don't fully support it here (our map is keyed only by packagename), and will remove from cache all but the last filename we find with the same packagename
			// TODO: Turn this into a warning once all sample projects have fixed it
			UE_LOG(LogAssetRegistry, Display, TEXT("Multiple files exist with the same package name %s but different extensions (%s and %s). ")
				TEXT("This is invalid and will cause errors; merge or rename or delete one of the files."),
				*PackageName.ToString(), *ValueInMap->Extension.ToString(), *DiskCachedAssetData->Extension.ToString());
		}
		ValueInMap = DiskCachedAssetData;
	}
}

void FAssetDataGatherer::GetAndTrimSearchResults(FResults& InOutResults, FResultContext& OutContext)
{
	FGathererScopeLock ResultsScopeLock(&ResultsLock);

	auto MoveAppendRangeToRingBuffer = [](auto& InOutRingBuffer, auto& InArray)
	{
		InOutRingBuffer.MoveAppendRange(InArray.GetData(), InArray.Num());
		InArray.Reset();
	};

	for (FAssetData* AssetData : AssetResults)
	{
		InOutResults.Assets.Add(AssetData->PackageName, AssetData);
	}
	AssetResults.Reset();
	MoveAppendRangeToRingBuffer(InOutResults.Paths, DiscoveredPaths);
	for (FPackageDependencyData& DependencyData : DependencyResults)
	{
		FName PackageName = DependencyData.PackageName;
		InOutResults.Dependencies.Add(PackageName, MoveTemp(DependencyData));
	}
	DependencyResults.Reset();
	MoveAppendRangeToRingBuffer(InOutResults.CookedPackageNamesWithoutAssetData, CookedPackageNamesWithoutAssetDataResults);
	MoveAppendRangeToRingBuffer(InOutResults.VerseFiles, VerseResults);

	InOutResults.BlockedFiles.Append(MoveTemp(BlockedResults));
	BlockedResults.Reset();

	OutContext.SearchTimes.Append(MoveTemp(SearchTimes));
	SearchTimes.Reset();

	OutContext.NumFilesToSearch = FilesToSearch->Num();
	OutContext.NumPathsToSearch = NumPathsToSearchAtLastSyncPoint;
	OutContext.bIsDiscoveringFiles = !bDiscoveryIsComplete;

	// Idle means no more work OR we are blocked on external events, but complete means no more work period.
	bool bLocalIsComplete = bIsIdle && FilesToSearch->Num() == 0;
	if (bLocalIsComplete && !bIsComplete)
	{
		bIsComplete = true;
		Shrink();
	}
	OutContext.bIsSearching = !bLocalIsComplete;
	OutContext.bAbleToProgress = !bIsIdle;
}

FAssetGatherDiagnostics FAssetDataGatherer::GetDiagnostics()
{
	FAssetGatherDiagnostics Diag;
	Discovery->GetDiagnostics(Diag.DiscoveryTimeSeconds, Diag.NumCachedDirectories, Diag.NumUncachedDirectories);
	FGathererScopeLock ResultsScopeLock(&ResultsLock);
	Diag.GatherTimeSeconds = CumulativeGatherTime;
	Diag.NumCachedAssetFiles = NumCachedAssetFiles;
	Diag.NumUncachedAssetFiles = NumUncachedAssetFiles;
	return Diag;
}

void FAssetDataGatherer::GetPackageResults(TMultiMap<FName, FAssetData*>& OutAssetResults, TMultiMap<FName, FPackageDependencyData>& OutDependencyResults)
{
	FGathererScopeLock ResultsScopeLock(&ResultsLock);

	for (FAssetData* AssetData : AssetResults)
	{
		OutAssetResults.Add(AssetData->PackageName, AssetData);
	}
	AssetResults.Reset();
	for (FPackageDependencyData& DependencyData : DependencyResults)
	{
		FName PackageName = DependencyData.PackageName;
		OutDependencyResults.Add(PackageName, MoveTemp(DependencyData));
	}
	DependencyResults.Reset();
}

void FAssetDataGatherer::WaitOnPath(FStringView InPath)
{
	{
		FGathererScopeLock ResultsScopeLock(&ResultsLock);
		if (bIsIdle)
		{
			return;
		}
	}
	FString LocalAbsPath = NormalizeLocalPath(InPath);
	UE::AssetDataGather::Private::FPathExistence QueryPath(LocalAbsPath);
	Discovery->SetPropertiesAndWait(TArrayView<UE::AssetDataGather::Private::FPathExistence>(&QueryPath, 1),
		false /* bAddToAllowList */, false /* bForceRescan */, false /* bIgnoreDenyListScanFilters */);
	WaitOnPathsInternal(TArrayView<UE::AssetDataGather::Private::FPathExistence>(&QueryPath, 1), FString(), TArray<FString>());
}

void FAssetDataGatherer::ClearCache()
{
	bool bCacheIsInUseOnOtherThread = false;
	bool bWasCacheEnabled = false;
	{
		FGathererScopeLock TickScopeLock(&TickLock);
		bWasCacheEnabled = bCacheWriteEnabled || bCacheReadEnabled;
		bCacheWriteEnabled = false;
		bCacheReadEnabled = false;
		{
			FGathererScopeLock ResultsScopeLock(&ResultsLock);
			bReadMonolithicCache = false;
			bWriteMonolithicCache = false;
		}
		bCacheIsInUseOnOtherThread = CacheInUseCount > 0;
	}

	if (!bWasCacheEnabled)
	{
		return;
	}

	// Wait for any cache saves to complete because saves read the cache data we are about to delete.
	// Saves are executed outside of the lock, but they indicate they are in progress by incrementing CacheInUseCount.
	// CacheInUseCount is no longer incremented after bCacheEnabled=false which we set above, so starvation should not be possible.
	while (bCacheIsInUseOnOtherThread)
	{
		constexpr float WaitForSaveCompleteTime = .001f;
		FPlatformProcess::Sleep(WaitForSaveCompleteTime);
		FGathererScopeLock TickScopeLock(&TickLock);
		bCacheIsInUseOnOtherThread = CacheInUseCount > 0;
	}

	{
		FGathererScopeLock TickScopeLock(&TickLock);
		NewCachedAssetDataMap.Empty();
		DiskCachedAssetDataMap.Empty();

		for (FDiskCachedAssetData* AssetData : NewCachedAssetData)
		{
			delete AssetData;
		}
		NewCachedAssetData.Empty();
		for (TPair<int32, FDiskCachedAssetData*> BlockData : DiskCachedAssetBlocks)
		{
			delete[] BlockData.Get<1>();
		}
		DiskCachedAssetBlocks.Empty();
	}
}


void FAssetDataGatherer::ScanPathsSynchronous(const TArray<FString>& InLocalPaths, bool bForceRescan,
	bool bIgnoreDenyListScanFilters, const FString& SaveCacheFilename, const TArray<FString>& SaveCacheLongPackageNameDirs)
{
	TArray<UE::AssetDataGather::Private::FPathExistence> QueryPaths;
	QueryPaths.Reserve(InLocalPaths.Num());
	for (const FString& LocalPath : InLocalPaths)
	{
		QueryPaths.Add(UE::AssetDataGather::Private::FPathExistence(NormalizeLocalPath(LocalPath)));
	}

	Discovery->SetPropertiesAndWait(QueryPaths, true /* bAddToAllowList */, bForceRescan, bIgnoreDenyListScanFilters);

	{
		FGathererScopeLock ResultsScopeLock(&ResultsLock);
		SetIsIdle(false);
	}

	WaitOnPathsInternal(QueryPaths, SaveCacheFilename, SaveCacheLongPackageNameDirs);
}

void FAssetDataGatherer::WaitOnPathsInternal(TArrayView<UE::AssetDataGather::Private::FPathExistence> QueryPaths,
	const FString& SaveCacheFilename, const TArray<FString>& SaveCacheLongPackageNameDirs)
{
	LLM_SCOPE(ELLMTag::AssetRegistry);

	// Request a halt to the async tick
	FScopedPause ScopedPause(*this);
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	{
		FGathererScopeLock TickScopeLock(&TickLock);

		// Read all results from Discovery into our worklist and then sort our worklist
		{
			FGathererScopeLock ResultsScopeLock(&ResultsLock);
			IngestDiscoveryResults();

			int32 NumDiscoveredPaths;
			SortPathsByPriority(QueryPaths, UE::AssetDataGather::Private::EPriority::Blocking, NumDiscoveredPaths);
			// Set WaitBatchCount to valid (non-negative) but possibly to 0. If it is 0 we still want to call TickInternal
			// to SetIdle(false) if necessary.
			WaitBatchCount = NumDiscoveredPaths;
		}
	}

	// We do not contribute to the async cache save if we have been given a modular cache to save 
	bool bContributeToCacheSave = SaveCacheFilename.IsEmpty();

	// Tick until NumDiscoveredPaths have been read
	TRACE_CPUPROFILER_EVENT_SCOPE(FAssetDataGatherer::Tick);
	for (;;)
	{
		InnerTickLoop(true /* bInSynchronousTick */, bContributeToCacheSave, -1. /* EndTimeSeconds */);
		FGathererScopeLock ResultsScopeLock(&ResultsLock); // WaitBatchCount requires the lock
		if (WaitBatchCount < 0)
		{
			break;
		}
	}

	if (!SaveCacheFilename.IsEmpty() && bCacheWriteEnabled)
	{
		TArray<TPair<FName, FDiskCachedAssetData*>> AssetsToSave;
		{
			FGathererScopeLock TickScopeLock(&TickLock);
			GetAssetsToSave(SaveCacheLongPackageNameDirs, AssetsToSave);
			CacheInUseCount++;
		}
		SaveCacheFileInternal(SaveCacheFilename, AssetsToSave);
		{
			FGathererScopeLock TickScopeLock(&TickLock);
			check(CacheInUseCount > 0);
			CacheInUseCount--;
		}
	}
}

void FAssetDataGatherer::WaitForIdle(float TimeoutSeconds)
{
	{
		FGathererScopeLock ResultsScopeLock(&ResultsLock);
		if (bIsIdle)
		{
			return;
		}
	}
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("FAssetDataGatherer::WaitForIdle");
	LLM_SCOPE(ELLMTag::AssetRegistry);

	double EndTimeSeconds = -1.;
	if (TimeoutSeconds >= 0.0f)
	{
		EndTimeSeconds = FPlatformTime::Seconds() + TimeoutSeconds;
	}
	if (Discovery->IsSynchronous())
	{
		Discovery->WaitForIdle(EndTimeSeconds);
		if (EndTimeSeconds > 0. && FPlatformTime::Seconds() > EndTimeSeconds)
		{
			return;
		}
	}

	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);

	// Request a halt to the async tick
	FScopedPause ScopedPause(*this);
	// Tick until idle
	for (;;)
	{
		InnerTickLoop(true /* bInSynchronousTick */, true /* bContributeToCacheSave */, EndTimeSeconds);
		if (EndTimeSeconds > 0 && FPlatformTime::Seconds() > EndTimeSeconds)
		{
			break;
		}
		FGathererScopeLock ResultsScopeLock(&ResultsLock); // bIsIdle requires the lock
		if (bIsIdle)
		{
			// We need to break out of WaitForIdle whenever it requires main thread action to proceed,
			// so we check bIsIdle rather than whether we're complete
			break;
		}
	}
}

bool FAssetDataGatherer::IsComplete() const
{
	FGathererScopeLock ResultsScopeLock(&ResultsLock);
	return bIsComplete;
}

void FAssetDataGatherer::SetInitialPluginsLoaded()
{
	bInitialPluginsLoaded = true;
	FGathererScopeLock ResultsScopeLock(&ResultsLock);
	SetIsIdle(false);
}

bool FAssetDataGatherer::IsGatheringDependencies() const
{
	return bGatherDependsData;
}

bool FAssetDataGatherer::IsCacheReadEnabled() const
{
	return bCacheReadEnabled;
}

bool FAssetDataGatherer::IsCacheWriteEnabled() const
{
	return bCacheWriteEnabled;
}

FString FAssetDataGatherer::GetCacheFilename(TConstArrayView<FString> CacheFilePackagePaths)
{
	// Try and build a consistent hash for this input
	// Normalize the paths; removing any trailing /
	TArray<FString> SortedPaths(CacheFilePackagePaths);
	for (FString& PackagePath : SortedPaths)
	{
		while (PackagePath.Len() > 1 && PackagePath.EndsWith(TEXT("/")))
		{
			PackagePath.LeftChopInline(1);
		}
	}

	// Sort the paths
	SortedPaths.StableSort();

	// todo: handle hash collisions?
	uint32 CacheHash = SortedPaths.Num() > 0 ? GetTypeHash(SortedPaths[0]) : 0;
	for (int32 PathIndex = 1; PathIndex < SortedPaths.Num(); ++PathIndex)
	{
		CacheHash = HashCombine(CacheHash, GetTypeHash(SortedPaths[PathIndex]));
	}

	return UE::AssetDataGather::Private::GPreloadSettings.GetAssetRegistryCacheRootFolder() / TEXT("AssetRegistryCache") / FString::Printf(TEXT("%08x%s.bin"), CacheHash, bGatherDependsData ? TEXT("") : TEXT("NoDeps"));
}

void FAssetDataGatherer::LoadCacheFiles(TConstArrayView<FString> CacheFilenames)
{
	using namespace UE::AssetDataGather::Private;
	if (!bCacheReadEnabled)
	{
		return;
	}

	TArray<FCachePayload> Payloads = UE::AssetDataGather::Private::LoadCacheFiles(CacheFilenames, false /* bIsMonolithicCache */);
	FScopedPause ScopedPause(*this);
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	FGathererScopeLock TickScopeLock(&TickLock);
	ConsumeCacheFiles(MoveTemp(Payloads));
}

void FAssetDataGatherer::ConsumeCacheFiles(TArray<UE::AssetDataGather::Private::FCachePayload> Payloads)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TickLock);
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	
	int32 Count = 0;
	for (UE::AssetDataGather::Private::FCachePayload& Payload : Payloads)
	{
		Count += Payload.NumAssets;
	}

	if (Count == 0)
	{
		return;
	}

	DiskCachedAssetDataMap.Reserve(DiskCachedAssetDataMap.Num() + Count);
	for (UE::AssetDataGather::Private::FCachePayload& Payload : Payloads)
	{
		for (int32 AssetIndex = 0; AssetIndex < Payload.NumAssets; ++AssetIndex)
		{
			DiskCachedAssetDataMap.Add(*(Payload.PackageNames.Get() + AssetIndex),
				(Payload.AssetDatas.Get() + AssetIndex)); // -C6385
		}
		DiskCachedAssetBlocks.Emplace(Payload.NumAssets, Payload.AssetDatas.Release());
		Payload.Reset();
	}

	FGathererScopeLock ResultsScopeLock(&ResultsLock);
	DependencyResults.Reserve(DiskCachedAssetDataMap.Num());
	AssetResults.Reserve(DiskCachedAssetDataMap.Num());
}

void FAssetDataGatherer::TryReserveSaveMonolithicCache(bool& bOutShouldSave, TArray<TPair<FName,FDiskCachedAssetData*>>& AssetsToSave)
{
	bOutShouldSave = false;
	if (IsStopped)
	{
		return;
	}
	if (!bSaveAsyncCacheTriggered || bIsSavingAsyncCache)
	{
		return;
	}
	CHECK_IS_LOCKED_CURRENT_THREAD(TickLock);
	int32 LocalNumUncachedAssetFiles;
	{
		FGathererScopeLock ResultsScopeLock(&ResultsLock);
		bOutShouldSave = bWriteMonolithicCache;
		LocalNumUncachedAssetFiles = NumUncachedAssetFiles;
	}
	if (bOutShouldSave)
	{
		GetMonolithicCacheAssetsToSave(AssetsToSave);
		bIsSavingAsyncCache = true;
		LastMonolithicCacheSaveUncachedAssetFiles = LocalNumUncachedAssetFiles ;
	}
	bSaveAsyncCacheTriggered = false;
}

void FAssetDataGatherer::GetAssetsToSave(TArrayView<const FString> SaveCacheLongPackageNameDirs, TArray<TPair<FName,FDiskCachedAssetData*>>& OutAssetsToSave)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TickLock);

	OutAssetsToSave.Reset();
	if (SaveCacheLongPackageNameDirs.IsEmpty())
	{
		OutAssetsToSave.Reserve(NewCachedAssetDataMap.Num());
		for (const TPair<FName, FDiskCachedAssetData*>& Pair : NewCachedAssetDataMap)
		{
			OutAssetsToSave.Add(Pair);
		}
	}
	else
	{
		for (const TPair<FName, FDiskCachedAssetData*>& Pair : NewCachedAssetDataMap)
		{
			TStringBuilder<128> PackageNameStr;
			Pair.Key.ToString(PackageNameStr);
			if (Algo::AnyOf(SaveCacheLongPackageNameDirs, [PackageNameSV = FStringView(PackageNameStr)](const FString& SaveCacheLongPackageNameDir)
			{
				return FPathViews::IsParentPathOf(SaveCacheLongPackageNameDir, PackageNameSV);
			}))
			{
				OutAssetsToSave.Add(Pair);
			}
		}
	}
}

void FAssetDataGatherer::GetMonolithicCacheAssetsToSave(TArray<TPair<FName,FDiskCachedAssetData*>>& OutAssetsToSave)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(TickLock);

	OutAssetsToSave.Reset();
	OutAssetsToSave.Reserve(FMath::Max(NewCachedAssetDataMap.Num(), DiskCachedAssetDataMap.Num()));
	for (const TPair<FName, FDiskCachedAssetData*>& Pair : NewCachedAssetDataMap)
	{
		OutAssetsToSave.Add(Pair);
	}

	for (const TPair<FName, FDiskCachedAssetData*>& Pair : DiskCachedAssetDataMap)
	{
		if(NewCachedAssetDataMap.Contains(Pair.Key))
		{
			continue; // Data was replaced when populating NewCachedAssetDataMap
		}	
		FTimespan Age = GatherStartTime - Pair.Value->LastGatheredTime;
		// Conservatively persist cached data until final save to avoid pruning the cache too much if discovery is interrupted 
		// when revisiting a workspace after some delay 
		if (!bFinishedInitialDiscovery || Age < AssetDataGathererConstants::CachePruneAge)
		{
			OutAssetsToSave.Add(Pair);
		}
	}
}

UE_TRACE_EVENT_BEGIN(Cpu, SaveCacheFile, NoSync)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, Path)
	UE_TRACE_EVENT_FIELD(int32, Count)
UE_TRACE_EVENT_END()

int64 FAssetDataGatherer::SaveCacheFileInternal(const FString& CacheFilename, const TArray<TPair<FName,FDiskCachedAssetData*>>& AssetsToSave)
{
	if (CacheFilename.IsEmpty() || !bCacheWriteEnabled)
	{
		return 0;
	}
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(TickLock);
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
#if CPUPROFILERTRACE_ENABLED
	UE_TRACE_LOG_SCOPED_T(Cpu, SaveCacheFile, CpuChannel)
		<< SaveCacheFile.Path(*CacheFilename)
		<< SaveCacheFile.Count(AssetsToSave.Num());
#endif // CPUPROFILERTRACE_ENABLED
	
	// Save to a temp file first, then move to the destination to avoid corruption
	FString CacheFilenameStr(CacheFilename);
	FString TempFilename = CacheFilenameStr + TEXT(".tmp");
	TUniquePtr<FArchive> FileAr(IFileManager::Get().CreateFileWriter(*TempFilename, 0));
	if (FileAr)
	{
		uint64 CurrentVersion = AssetDataGathererConstants::CurrentVersion;
		*FileAr << CurrentVersion;

#if ALLOW_NAME_BATCH_SAVING
		{
			// We might be able to reduce load time by using AssetRegistry::SerializationOptions
			// to save certain common tags as FName.
			UE::AssetDataGather::Private::FChecksumArchiveWriter ChecksummingWriter(*FileAr);
			FAssetRegistryWriter Ar(FAssetRegistryWriterOptions(), ChecksummingWriter);
			UE::AssetDataGather::Private::SerializeCacheSave(Ar, AssetsToSave);
		}
#else		
		checkf(false, TEXT("Cannot save asset registry cache in this configuration"));
#endif
		int64 CacheSize = FileAr->TotalSize();
		// Close file handle before moving temp file to target 
		FileAr.Reset();
		IFileManager::Get().Move(*CacheFilenameStr, *TempFilename);
		return CacheSize;
	}
	else
	{
		UE_LOG(LogAssetRegistry, Error, TEXT("Failed to open file for write %s"), *TempFilename);
		return 0;
	}
}

namespace UE::AssetDataGather::Private
{

void SerializeCacheSave(FAssetRegistryWriter& Ar, const TArray<TPair<FName,FDiskCachedAssetData*>>& AssetsToSave)
{
#if ALLOW_NAME_BATCH_SAVING
	double SerializeStartTime = FPlatformTime::Seconds();

	// serialize number of objects
	int32 LocalNumAssets = AssetsToSave.Num();
	Ar << LocalNumAssets;

	for (const TPair<FName, FDiskCachedAssetData*>& Pair : AssetsToSave)
	{
		FName AssetName = Pair.Key;
		Ar << AssetName;
		Pair.Value->SerializeForCache(Ar);
	}

	UE_LOG(LogAssetRegistry, Verbose, TEXT("Asset data gatherer serialized in %0.6f seconds"), FPlatformTime::Seconds() - SerializeStartTime);
#endif
}

FCachePayload SerializeCacheLoad(FAssetRegistryReader& Ar)
{
	double SerializeStartTime = FPlatformTime::Seconds();
	ON_SCOPE_EXIT
	{
		UE_LOG(LogAssetRegistry, Verbose, TEXT("Asset data gatherer serialized in %0.6f seconds"), FPlatformTime::Seconds() - SerializeStartTime);
	};

	// serialize number of objects
	int32 LocalNumAssets = 0;
	Ar << LocalNumAssets;

	const int32 MinAssetEntrySize = sizeof(int32);
	const int64 MaxPossibleNumAssets = (Ar.TotalSize() - Ar.Tell()) / MinAssetEntrySize;
	if (Ar.IsError() || LocalNumAssets < 0 || MaxPossibleNumAssets < LocalNumAssets)
	{
		Ar.SetError();
		return FCachePayload();
	}

	if (LocalNumAssets == 0)
	{
		FCachePayload Payload = FCachePayload();
		Payload.bSucceeded = true;
		return Payload;
	}

	FSoftObjectPathSerializationScope SerializationScope(NAME_None, NAME_None, ESoftObjectPathCollectType::NonPackage, ESoftObjectPathSerializeType::AlwaysSerialize);

	// allocate one single block for all asset data structs (to reduce tens of thousands of heap allocations)
	TUniquePtr<FName[]> PackageNameBlock(new FName[LocalNumAssets]);
	TUniquePtr<FDiskCachedAssetData[]> AssetDataBlock(new FDiskCachedAssetData[LocalNumAssets]);
	for (int32 AssetIndex = 0; AssetIndex < LocalNumAssets; ++AssetIndex)
	{
		// Load the name first to add the entry to the tmap below
		// Visual Studio Static Analyzer issues C6385 if we call Ar << PackageNameBlock[AssetIndex] or AssetDataBlock[AssetIndex].SerializeForCache
		Ar << *(PackageNameBlock.Get() + AssetIndex); // -C6385
		(AssetDataBlock.Get() + AssetIndex)->SerializeForCache(Ar); // -C6385
		if (Ar.IsError())
		{
			// There was an error reading the cache. Bail out.
			break;
		}
	}

	if (Ar.IsError())
	{
		return FCachePayload();
	}
	FCachePayload Result;
	Result.PackageNames = MoveTemp(PackageNameBlock);
	Result.AssetDatas = MoveTemp(AssetDataBlock);
	Result.NumAssets = LocalNumAssets;
	Result.bSucceeded = true;
	return Result;
}

TArray<FCachePayload> LoadCacheFiles(TConstArrayView<FString> InCacheFilenames, bool bIsMonolithicCache)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(LoadCacheFiles);
	TArray<FCachePayload> Results;
	Results.AddDefaulted(InCacheFilenames.Num());
	ParallelFor(InCacheFilenames.Num(), [&InCacheFilenames, &Results, bIsMonolithicCache](int32 Index)
	{
		const FString& CacheFilename = InCacheFilenames[Index];
		auto DoLoad = [&](FArchive& ChecksummingReader)
		{
			// We are already using AsyncTasks for our own ParallelFor, passing non-zero NumAsyncWorkers into
			// FAssetRegistryReader will cause our current task to block waiting for another task to run
			// the name batch loading. That can cause a deadlock due to running out of task threads
			// if we don't have a large number of threads, so disable the parallelism.
			constexpr int32 NumAsyncWorkers = 0;
			
			// The discovery cache is always serialized with a fixed format.
			// We discard it before this point if it's not the latest version, and it always includes editor-only data.
			FAssetRegistryHeader Header;
			Header.Version = FAssetRegistryVersion::LatestVersion;
			Header.bFilterEditorOnlyData = false;
			FAssetRegistryReader RegistryReader(ChecksummingReader, NumAsyncWorkers, Header);
			return RegistryReader.IsError() ? FCachePayload() : SerializeCacheLoad(RegistryReader);
		};

		FCachePayload Payload;
		if (FPlatformProperties::SupportsMemoryMappedFiles())
		{
			FMemoryMappedFile File(*CacheFilename);
			UE::Tasks::FTask Preload = UE::Tasks::Launch(UE_SOURCE_LOCATION, [&]() { File.Preload(); });

			FMemoryViewReader FileReader(File.View());
			TOptional<uint64> Version = FileReader.TryLoad<uint64>();
			if (Version == AssetDataGathererConstants::CurrentVersion)
			{
				FChecksumViewReader ChecksummingReader(MoveTemp(FileReader), CacheFilename);
				Payload = DoLoad(ChecksummingReader);
				UE_CLOG(bIsMonolithicCache && Payload.bSucceeded, LogAssetRegistry, Display, TEXT("Asset registry cache read as %.1f MiB from %s"), static_cast<float>(FileReader.GetTotalSize())/1024.f/1024.f, *CacheFilename);
				UE_CLOG(!Payload.bSucceeded, LogAssetRegistry, Warning, TEXT("There was an error loading the asset registry cache using memory mapping"));
			}

			Preload.Wait();
			UE::Tasks::Launch(UE_SOURCE_LOCATION, [KillAsync = MoveTemp(File)](){});
		}
		else
		{
			TUniquePtr<FArchive> FileAr(IFileManager::Get().CreateFileReader(*CacheFilename, FILEREAD_Silent));
			if (FileAr && !FileAr->IsError() && FileAr->TotalSize() > sizeof(uint64))
			{
				uint64 Version = 0;
				*FileAr << Version;
				if (Version == AssetDataGathererConstants::CurrentVersion)
				{
					FChecksumArchiveReader ChecksummingReader(*FileAr);
					Payload = DoLoad(ChecksummingReader);
					UE_CLOG(bIsMonolithicCache && Payload.bSucceeded, LogAssetRegistry, Display, TEXT("Asset registry cache read as %.1f MiB from %s"), static_cast<float>(FileAr->TotalSize())/1024.f/1024.f, *CacheFilename);
					UE_CLOG(!Payload.bSucceeded, LogAssetRegistry, Warning, TEXT("There was an error loading the asset registry cache"));
				}
			}
		}
	
		Results[Index] = MoveTemp(Payload);
	}, EParallelForFlags::BackgroundPriority);

	return MoveTemp(Results);
}
}

SIZE_T FAssetDataGatherer::GetAllocatedSize() const
{
	using namespace UE::AssetDataGather::Private;
	auto GetArrayRecursiveAllocatedSize = [](auto Container)
	{
		SIZE_T Result = Container.GetAllocatedSize();
		for (const auto& Value : Container)
		{
			Result += Value.GetAllocatedSize();
		}
		return Result;
	};

	SIZE_T Result = 0;
	if (Thread)
	{
		// TODO: Add size of Thread->GetAllocatedSize()
		Result += sizeof(*Thread);
	}

	Result += sizeof(*Discovery) + Discovery->GetAllocatedSize();

	FScopedPause ScopedPause(*this);
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	FGathererScopeLock TickScopeLock(&TickLock);
	FGathererScopeLock ResultsScopeLock(&ResultsLock);

	Result += sizeof(*FilesToSearch) + FilesToSearch->GetAllocatedSize();

	Result += AssetResults.GetAllocatedSize();
	FAssetDataTagMapSharedView::FMemoryCounter TagMemoryUsage;
	for (FAssetData* Value : AssetResults)
	{
		Result += sizeof(*Value);
		TagMemoryUsage.Include(Value->TagsAndValues);
	}
	Result += FAssetData::GetChunkArrayRegistryAllocatedSize();
	Result += TagMemoryUsage.GetFixedSize() + TagMemoryUsage.GetLooseSize();

	Result += GetArrayRecursiveAllocatedSize(DependencyResults);
	Result += GetArrayRecursiveAllocatedSize(CookedPackageNamesWithoutAssetDataResults);
	Result += VerseResults.GetAllocatedSize();
	Result += BlockedResults.GetAllocatedSize();
	Result += SearchTimes.GetAllocatedSize();
	Result += GetArrayRecursiveAllocatedSize(DiscoveredPaths);
	Result += GPreloadSettings.GetMonolithicCacheBaseFilename().GetAllocatedSize();

	Result += NewCachedAssetData.GetAllocatedSize();
	for (const FDiskCachedAssetData* Value : NewCachedAssetData)
	{
		Result += sizeof(*Value);
		Result += Value->GetAllocatedSize();
	}
	Result += DiskCachedAssetBlocks.GetAllocatedSize();
	for (const TPair<int32, FDiskCachedAssetData*>& ArrayData : DiskCachedAssetBlocks)
	{
		Result += ArrayData.Get<0>() * sizeof(FDiskCachedAssetData);
	}
	Result += DiskCachedAssetDataMap.GetAllocatedSize();
	Result += NewCachedAssetDataMap.GetAllocatedSize();

	return Result;
}

void FAssetDataGatherer::Shrink()
{
	CHECK_IS_LOCKED_CURRENT_THREAD(ResultsLock);
	FilesToSearch->Shrink();
	AssetResults.Shrink();
	DependencyResults.Shrink();
	CookedPackageNamesWithoutAssetDataResults.Shrink();
	VerseResults.Shrink();
	BlockedResults.Shrink();
	SearchTimes.Shrink();
	DiscoveredPaths.Shrink();
}

void FAssetDataGatherer::AddMountPoint(FStringView LocalPath, FStringView LongPackageName)
{
	bool bAlreadyExisted = false;
	Discovery->AddMountPoint(NormalizeLocalPath(LocalPath), NormalizeLongPackageName(LongPackageName), bAlreadyExisted);

	if (!bAlreadyExisted)
	{
		FGathererScopeLock ResultsScopeLock(&ResultsLock);
		SetIsIdle(false);
	}
}

void FAssetDataGatherer::RemoveMountPoint(FStringView LocalPath)
{
	Discovery->RemoveMountPoint(NormalizeLocalPath(LocalPath));
}

void FAssetDataGatherer::AddRequiredMountPoints(TArrayView<FString> LocalPaths)
{
	TStringBuilder<128> MountPackageName;
	TStringBuilder<128> MountFilePath;
	TStringBuilder<128> RelPath;
	bool bAllExisted = true;
	for (const FString& LocalPath : LocalPaths)
	{
		if (FPackageName::TryGetMountPointForPath(LocalPath, MountPackageName, MountFilePath, RelPath))
		{
			bool bAlreadyExisted = false;
			Discovery->AddMountPoint(NormalizeLocalPath(MountFilePath), NormalizeLongPackageName(MountPackageName), bAlreadyExisted);
			bAllExisted = bAllExisted && bAlreadyExisted;
		}
	}

	if (!bAllExisted)
	{
		FGathererScopeLock ResultsScopeLock(&ResultsLock);
		SetIsIdle(false);
	}
}

void FAssetDataGatherer::OnDirectoryCreated(FStringView LocalPath)
{
	Discovery->OnDirectoryCreated(NormalizeLocalPath(LocalPath));
	{
		FGathererScopeLock ResultsScopeLock(&ResultsLock);
		SetIsIdle(false);
	}
}

void FAssetDataGatherer::OnFilesCreated(TConstArrayView<FString> LocalPaths)
{
	TArray<FString> LocalAbsPaths;
	LocalAbsPaths.Reserve(LocalPaths.Num());
	for (const FString& LocalPath : LocalPaths)
	{
		LocalAbsPaths.Add(NormalizeLocalPath(LocalPath));
	}
	Discovery->OnFilesCreated(LocalAbsPaths);
	{
		FGathererScopeLock ResultsScopeLock(&ResultsLock);
		SetIsIdle(false);
	}
}

void FAssetDataGatherer::PrioritizeSearchPath(const FString& PathToPrioritize)
{
	using namespace UE::AssetDataGather::Private;

	FString LocalFilenamePathToPrioritize;
	if (FPackageName::TryConvertLongPackageNameToFilename(PathToPrioritize, LocalFilenamePathToPrioritize))
	{
		LocalFilenamePathToPrioritize = NormalizeLocalPath(LocalFilenamePathToPrioritize);
		if (LocalFilenamePathToPrioritize.Len() == 0)
		{
			return;
		}
		EPriority Priority = EPriority::High;
		Discovery->PrioritizeSearchPath(LocalFilenamePathToPrioritize, Priority);

		{
			FGathererScopeLock ResultsScopeLock(&ResultsLock);
			SetIsIdle(false);
			int32 NumPrioritizedPaths;
			UE::AssetDataGather::Private::FPathExistence QueryPath(LocalFilenamePathToPrioritize);
			SortPathsByPriority(TArrayView<UE::AssetDataGather::Private::FPathExistence>(&QueryPath, 1),
				Priority, NumPrioritizedPaths);
		}
	}
}

void FAssetDataGatherer::SetDirectoryProperties(FStringView LocalPath, const UE::AssetDataGather::Private::FSetPathProperties& InProperties)
{
	FString LocalAbsPath = NormalizeLocalPath(LocalPath);
	if (LocalAbsPath.Len() == 0)
	{
		return;
	}

	Discovery->TrySetDirectoryProperties(LocalAbsPath, InProperties, false);

	{
		FGathererScopeLock ResultsScopeLock(&ResultsLock);
		SetIsIdle(false);
	}
}

void FAssetDataGatherer::SortPathsByPriority(
	TArrayView<UE::AssetDataGather::Private::FPathExistence> LocalAbsPathsToPrioritize,
	UE::AssetDataGather::Private::EPriority Priority, int32& OutNumPaths)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(ResultsLock);
	using namespace UE::AssetDataGather::Private;

	for (UE::AssetDataGather::Private::FPathExistence& QueryPath : LocalAbsPathsToPrioritize)
	{
		switch (QueryPath.GetType())
		{
		case FPathExistence::EType::Directory:
			FilesToSearch->PrioritizeDirectory(QueryPath.GetLocalAbsPath(), Priority);
			break;
		case FPathExistence::EType::File:
			// Intentional fall-through
		case FPathExistence::EType::MissingButDirExists:
			// We assume MissingButDirExists is a file missing the extension;
			// PrioritizeFile handles searching for BaseNameWithPath.* and does nothing if no matches are found
			FilesToSearch->PrioritizeFile(QueryPath.GetLocalAbsPath(), Priority);
			break;
		default:
			break;
		}
	}
	OutNumPaths = FilesToSearch->NumBlockingFiles();
}

void FAssetDataGatherer::SetIsOnAllowList(FStringView LocalPath, bool bIsAllowed)
{
	using namespace UE::AssetDataGather::Private;

	FSetPathProperties Properties;
	Properties.IsOnAllowList = bIsAllowed;
	SetDirectoryProperties(LocalPath, Properties);
}

bool FAssetDataGatherer::IsOnAllowList(FStringView LocalPath) const
{
	return Discovery->IsOnAllowList(NormalizeLocalPath(LocalPath));
}

bool FAssetDataGatherer::IsOnDenyList(FStringView LocalPath) const
{
	return Discovery->IsOnDenyList(NormalizeLocalPath(LocalPath));
}

bool FAssetDataGatherer::IsMonitored(FStringView LocalPath) const
{
	return Discovery->IsMonitored(NormalizeLocalPath(LocalPath));
}

bool FAssetDataGatherer::IsVerseFile(FStringView FilePath)
{
	return FilePath.EndsWith(TEXT(".verse")) || FilePath.EndsWith(TEXT(".vmodule"));
}

void FAssetDataGatherer::SetIsIdle(bool bInIsIdle)
{
	double TickStartTime = -1.;
	SetIsIdle(bInIsIdle, TickStartTime);
}

void FAssetDataGatherer::SetIsIdle(bool bInIsIdle, double& TickStartTime)
{
	CHECK_IS_LOCKED_CURRENT_THREAD(ResultsLock);
	if (bInIsIdle == bIsIdle)
	{
		return;
	}

	bIsIdle = bInIsIdle;
	if (bIsIdle)
	{
		// bIsComplete will be set in GetAndTrimSearchResults
		if (TickStartTime >= 0.)
		{
			CurrentSearchTime += FPlatformTime::Seconds() - TickStartTime;
			TickStartTime = -1.;
		}
		// Finishing the initial discovery is blocked until IsEngineStartupModuleLoadingComplete because plugins can
		// be mounted during startup up until that point, and we need to wait for all the plugins that will load
		// before declaring completion.
		if (!bFinishedInitialDiscovery && IsEngineStartupModuleLoadingComplete())
		{
			bFinishedInitialDiscovery = true;

			UE_LOG(LogAssetRegistry, Verbose, TEXT("Initial scan took %0.6f seconds (found %d cached assets, and loaded %d)"),
				(float)CurrentSearchTime, NumCachedAssetFiles, NumUncachedAssetFiles);
		}
		SearchTimes.Add(CurrentSearchTime);
		CumulativeGatherTime += static_cast<float>(CurrentSearchTime);
		CurrentSearchTime = 0.;
	}
	else
	{
		bIsComplete = false;
		bDiscoveryIsComplete = false;
		bFirstTickAfterIdle = true;
	}
}

FString FAssetDataGatherer::NormalizeLocalPath(FStringView LocalPath)
{
	FString LocalAbsPath(LocalPath);
	LocalAbsPath = FPaths::ConvertRelativePathToFull(MoveTemp(LocalAbsPath));
	while (FPathViews::HasRedundantTerminatingSeparator(LocalAbsPath))
	{
		LocalAbsPath.LeftChopInline(1);
	}
	return LocalAbsPath;
}

FStringView FAssetDataGatherer::NormalizeLongPackageName(FStringView LongPackageName)
{
	// Conform LongPackageName to our internal format, which does not have a terminating redundant /
	if (LongPackageName.EndsWith(TEXT('/')))
	{
		LongPackageName = LongPackageName.LeftChop(1);
	}
	return LongPackageName;
}

void FAssetDataGatherer::OnAllModuleLoadingPhasesComplete()
{
	CHECK_IS_NOT_LOCKED_CURRENT_THREAD(ResultsLock);
	FGathererScopeLock ResultsScopeLock(&ResultsLock);
	SetIsIdle(false);
}

namespace UE::AssetDataGather::Private
{

void FFilesToSearch::AddPriorityFile(FGatheredPathData&& FilePath)
{
	++AvailableFilesNum;
	BlockingFiles.Add(MoveTemp(FilePath));
}

void FFilesToSearch::AddDirectory(FString&& DirAbsPath, TArray<FGatheredPathData>&& FilePaths)
{
	if (FilePaths.Num() == 0)
	{
		return;
	}
	check(!DirAbsPath.IsEmpty());

	FTreeNode& Node = Root.FindOrAddNode(DirAbsPath);
	AvailableFilesNum += FilePaths.Num();
	Node.AddFiles(MoveTemp(FilePaths));
}

void FFilesToSearch::AddFileAgainAfterTimeout(FGatheredPathData&& FilePath)
{
	++AvailableFilesNum;
	BlockingFiles.AddFront(MoveTemp(FilePath));
}

void FFilesToSearch::AddFileForLaterRetry(FGatheredPathData&& FilePath)
{
	LaterRetryFiles.Add(FilePath);
}

void FFilesToSearch::RetryLaterRetryFiles()
{
	while (!LaterRetryFiles.IsEmpty())
	{
		FGatheredPathData FilePath = LaterRetryFiles.PopFrontValue();
		FTreeNode& Node = Root.FindOrAddNode(FPathViews::GetPath(FilePath.LocalAbsPath));
		++AvailableFilesNum;
		Node.AddFile(MoveTemp(FilePath));
	}
}

template <typename AllocatorType>
void FFilesToSearch::PopFront(TArray<FGatheredPathData, AllocatorType>& Out, int32 NumToPop)
{
	int32 InitialNumToPop = NumToPop;
	while (NumToPop > 0 && !BlockingFiles.IsEmpty())
	{
		Out.Add(BlockingFiles.PopFrontValue());
		--NumToPop;
	}
	Root.PopFiles(Out, NumToPop);
	AvailableFilesNum += NumToPop - InitialNumToPop;
	check(AvailableFilesNum >= 0);
}

void FFilesToSearch::PrioritizeDirectory(FStringView DirAbsPath, EPriority Priority)
{
	// We may need to prioritize a LaterRetryFile that is now loadable, so add them all into the Root
	RetryLaterRetryFiles();

	if (Priority > EPriority::Blocking)
	{
		// TODO: Implement another tree that is searched first for the High Priority 
		// We cannot add the High Priority files to the BlockingFiles array, because
		// then blocking on BlockingFiles to be empty could be slow. We cannot add them
		// as a separate simple array, because we would have to search that (sometimes large)
		// array linearly when looking for files to accomodate a blocking priority request
		return;
	}
	FTreeNode* TreeNode = Root.FindNode(DirAbsPath);
	if (TreeNode)
	{
		TreeNode->PopAllFiles(BlockingFiles);
	}
}

void FFilesToSearch::PrioritizeFile(FStringView FileAbsPathExtOptional, EPriority Priority)
{
	// We may need to prioritize a LaterRetryFile that is now loadable, so add them all into the Root
	RetryLaterRetryFiles();

	if (Priority > EPriority::Blocking)
	{
		// TODO: Implement High Priority; see note in PrioritizeDirectory
		return;
	}
	FTreeNode* TreeNode = Root.FindNode(FPathViews::GetPath(FileAbsPathExtOptional));
	if (TreeNode)
	{
		int32 BeforeSize = BlockingFiles.Num();
		TreeNode->PopMatchingDirectFiles(BlockingFiles, FileAbsPathExtOptional);
	}
}

int32 FFilesToSearch::NumBlockingFiles() const
{
	return BlockingFiles.Num();
}

void FFilesToSearch::Shrink()
{
	// TODO: Make TRingBuffer::Shrink
	TRingBuffer<UE::AssetDataGather::Private::FGatheredPathData> Buffer;
	Buffer.Reserve(BlockingFiles.Num());
	for (UE::AssetDataGather::Private::FGatheredPathData& File : BlockingFiles)
	{
		Buffer.Add(MoveTemp(File));
	}
	Swap(Buffer, BlockingFiles);

	Buffer.Empty(LaterRetryFiles.Num());
	for (UE::AssetDataGather::Private::FGatheredPathData& File : LaterRetryFiles)
	{
		Buffer.Add(MoveTemp(File));
	}
	Swap(Buffer, LaterRetryFiles);

	Root.Shrink();
}

int32 FFilesToSearch::Num() const
{
	return AvailableFilesNum + LaterRetryFiles.Num();
}

int32 FFilesToSearch::GetNumAvailable() const
{
	return AvailableFilesNum;
}

SIZE_T FFilesToSearch::GetAllocatedSize() const
{
	SIZE_T Size = 0;
	Size += BlockingFiles.GetAllocatedSize();
	for (const FGatheredPathData& PathData : BlockingFiles)
	{
		Size += PathData.GetAllocatedSize();
	}
	Size += Root.GetAllocatedSize();
	Size += LaterRetryFiles.GetAllocatedSize();
	for (const FGatheredPathData& PathData : LaterRetryFiles)
	{
		Size += PathData.GetAllocatedSize();
	}
	return Size;
}

FFilesToSearch::FTreeNode::FTreeNode(FStringView InRelPath)
	:RelPath(InRelPath)
{
}

FStringView FFilesToSearch::FTreeNode::GetRelPath() const
{
	return RelPath;
}

FFilesToSearch::FTreeNode& FFilesToSearch::FTreeNode::FindOrAddNode(FStringView InRelPath)
{
	if (InRelPath.IsEmpty())
	{
		return *this;
	}
	FStringView FirstComponent;
	FStringView RemainingPath;
	FPathViews::SplitFirstComponent(InRelPath, FirstComponent, RemainingPath);
	FTreeNode& SubDir = FindOrAddSubDir(FirstComponent);
	return SubDir.FindOrAddNode(RemainingPath);
}

FFilesToSearch::FTreeNode* FFilesToSearch::FTreeNode::FindNode(FStringView InRelPath)
{
	if (InRelPath.IsEmpty())
	{
		return this;
	}
	FStringView FirstComponent;
	FStringView RemainingPath;
	FPathViews::SplitFirstComponent(InRelPath, FirstComponent, RemainingPath);
	FTreeNode* SubDir = FindSubDir(FirstComponent);
	if (!SubDir)
	{
		return nullptr;
	}

	return SubDir->FindNode(RemainingPath);
}

FFilesToSearch::FTreeNode& FFilesToSearch::FTreeNode::FindOrAddSubDir(FStringView SubDirBaseName)
{
	int32 Index = FindLowerBoundSubDir(SubDirBaseName);
	if (Index == SubDirs.Num() || !FPathViews::Equals(SubDirs[Index]->GetRelPath(), SubDirBaseName))
	{
		return *SubDirs.EmplaceAt_GetRef(Index, MakeUnique<FTreeNode>(SubDirBaseName));
	}
	else
	{
		return *SubDirs[Index];
	}
}

FFilesToSearch::FTreeNode* FFilesToSearch::FTreeNode::FindSubDir(FStringView SubDirBaseName)
{
	int32 Index = FindLowerBoundSubDir(SubDirBaseName);
	if (Index == SubDirs.Num() || !FPathViews::Equals(SubDirs[Index]->GetRelPath(), SubDirBaseName))
	{
		return nullptr;
	}
	else
	{
		return SubDirs[Index].Get();
	}
}

int32 FFilesToSearch::FTreeNode::FindLowerBoundSubDir(FStringView SubDirBaseName)
{
	return Algo::LowerBound(SubDirs, SubDirBaseName,
		[](const TUniquePtr<FTreeNode>& SubDir, FStringView BaseName)
		{
			return FPathViews::Less(SubDir->RelPath, BaseName);
		}
	);
}

void FFilesToSearch::FTreeNode::AddFiles(TArray<FGatheredPathData>&& FilePaths)
{
	if (Files.Num() == 0)
	{
		Files = MoveTemp(FilePaths);
	}
	else
	{
		Files.Append(MoveTemp(FilePaths));
	}
}

void FFilesToSearch::FTreeNode::AddFile(FGatheredPathData&& FilePath)
{
	Files.Add(MoveTemp(FilePath));
}

template <typename RangeType>
void FFilesToSearch::FTreeNode::PopFiles(RangeType& Out, int32& NumToPop)
{
	while (NumToPop > 0 && !Files.IsEmpty())
	{
		Out.Add(Files.Pop(EAllowShrinking::No));
		--NumToPop;
	}
	while (NumToPop > 0 && SubDirs.Num() != 0)
	{
		TUniquePtr<FTreeNode>& SubDir = SubDirs[SubDirs.Num() - 1];
		SubDir->PopFiles(Out, NumToPop);
		if (SubDir->IsEmpty())
		{
			SubDirs.RemoveAt(SubDirs.Num() - 1);
		}
	}
}

template <typename RangeType>
void FFilesToSearch::FTreeNode::PopAllFiles(RangeType& Out)
{
	while (!Files.IsEmpty())
	{
		Out.Add(Files.Pop(EAllowShrinking::No));
	}
	for (int32 Index = SubDirs.Num() - 1; Index >= 0; --Index) // Match the order of PopFiles
	{
		TUniquePtr<FTreeNode>& SubDir = SubDirs[Index];
		SubDir->PopAllFiles(Out);
		SubDir.Reset();
	}
	SubDirs.Empty();
}

template <typename RangeType>
void FFilesToSearch::FTreeNode::PopMatchingDirectFiles(RangeType& Out, FStringView FileAbsPathExtOptional)
{
	// TODO: Make this more performant by sorting the list of Files.
	// To prevent shifting costs, when popping the file we will leave a placeholder behind with an ignore flag set.
	FStringView FileAbsPathNoExt = FPathViews::GetBaseFilenameWithPath(FileAbsPathExtOptional);
	int32 NumFiles = Files.Num();
	for (int32 Index = 0; Index < NumFiles; )
	{
		FGatheredPathData& PathData = Files[Index];
		if (FPathViews::Equals(FPathViews::GetBaseFilenameWithPath(PathData.LocalAbsPath), FileAbsPathNoExt))
		{
			Out.Add(MoveTemp(PathData));
			Files.RemoveAt(Index);
			--NumFiles;
		}
		else
		{
			++Index;
		}
	}
}

void FFilesToSearch::FTreeNode::PruneEmptyChild(FStringView SubDirBaseName)
{
	int32 Index = FindLowerBoundSubDir(SubDirBaseName);
	if (!(Index == SubDirs.Num() || !FPathViews::Equals(SubDirs[Index]->GetRelPath(), SubDirBaseName)))
	{
		if (SubDirs[Index]->IsEmpty())
		{
			SubDirs.RemoveAt(Index);
		}
	}
}

bool FFilesToSearch::FTreeNode::IsEmpty() const
{
	return Files.Num() == 0 && SubDirs.Num() == 0;
}

void FFilesToSearch::FTreeNode::Shrink()
{
	Files.Shrink();
	SubDirs.Shrink();
	for (TUniquePtr<FTreeNode>& SubDir : SubDirs)
	{
		SubDir->Shrink();
	}
}

SIZE_T FFilesToSearch::FTreeNode::GetAllocatedSize() const
{
	SIZE_T Size = 0;
	Size = Files.GetAllocatedSize();
	for (const FGatheredPathData& File : Files)
	{
		Size += File.GetAllocatedSize();
	}
	Size += SubDirs.GetAllocatedSize() + SubDirs.Num()*sizeof(FTreeNode);
	for (const TUniquePtr<FTreeNode>& SubDir : SubDirs)
	{
		Size += SubDir->GetAllocatedSize();
	}
	return Size;
}

int32 FFilesToSearch::FTreeNode::NumFiles() const
{
	int32 Num = Files.Num();
	for (const TUniquePtr<FTreeNode>& SubDir : SubDirs)
	{
		Num += SubDir->NumFiles();
	}
	return Num;
}

}
