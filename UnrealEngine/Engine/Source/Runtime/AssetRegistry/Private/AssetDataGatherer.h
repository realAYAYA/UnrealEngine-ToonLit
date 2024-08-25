// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/RingBuffer.h"
#include "Containers/StringFwd.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "CriticalSectionQueryable.h"
#include "DiskCachedAssetData.h"
#include "HAL/CriticalSection.h"
#include "HAL/FileManager.h"
#include "HAL/Platform.h"
#include "HAL/Runnable.h"
#include "Misc/DateTime.h"
#include "Misc/Optional.h"
#include "PackageDependencyData.h"
#include "AssetRegistry/PackageReader.h"
#include "Templates/UniquePtr.h"
#include "UObject/NameTypes.h"

#include <atomic>

class FArchive;
struct FAssetData;
class FDiskCachedAssetData;
class FAssetRegistryReader;
class FAssetRegistryWriter; // Not defined if !ALLOW_NAME_BATCH_SAVING
namespace UE::AssetDataGather::Private
{
class FAssetDataDiscovery;
class FFilesToSearch;
struct FCachePayload;
struct FGatheredPathData;
struct FPathExistence;
struct FSetPathProperties;
enum class EPriority : uint8;
}

#if DO_CHECK
typedef FCriticalSectionQueryable FGathererCriticalSection;
typedef FScopeLockQueryable FGathererScopeLock;
#define CHECK_IS_LOCKED_CURRENT_THREAD(CritSec) check(CritSec.IsLockedOnCurrentThread())
#define CHECK_IS_NOT_LOCKED_CURRENT_THREAD(CritSec) check(!CritSec.IsLockedOnCurrentThread())
#else
typedef FCriticalSection FGathererCriticalSection;
typedef FScopeLock FGathererScopeLock;
#define CHECK_IS_LOCKED_CURRENT_THREAD(CritSec) do {} while (false)
#define CHECK_IS_NOT_LOCKED_CURRENT_THREAD(CritSec) do {} while (false)
#endif

struct FAssetGatherDiagnostics
{
	/** Time spent identifying asset files on disk */
	float DiscoveryTimeSeconds;
	/** Time spent reading asset files on disk / from cache */
	float GatherTimeSeconds;
	/** How many directories in the search results were read from the cache. */
	int32 NumCachedDirectories;
	/** How many directories in the search results were not in the cache and were read by scanning the disk. */
	int32 NumUncachedDirectories;
	/** How many files in the search results were read from the cache. */
	int32 NumCachedAssetFiles;
	/** How many files in the search results were not in the cache and were read by parsing the file. */
	int32 NumUncachedAssetFiles;
};

/**
 * Async task for gathering asset data from from the file list in FAssetRegistry
 */
class FAssetDataGatherer : public FRunnable
{
public:
	FAssetDataGatherer(const TArray<FString>& InLongPackageNamesDenyList,
		const TArray<FString>& InMountRelativePathsDenyList, bool bInAsyncEnabled);
	virtual ~FAssetDataGatherer();

	void OnInitialSearchCompleted();

	// Extra at-construction configuration 

	/** Configure the gatherer to use a single monolithic cache, and read/write this cache during ticks. */
	void ActivateMonolithicCache();


	// Controlling Async behavior

	/** Start the async thread, if this Gatherer was created async. Does nothing if not async or already started. */
	void StartAsync();

	// FRunnable implementation
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

	bool IsAsyncEnabled() const;
	bool IsSynchronous() const;
	/** Signals to end the thread and waits for it to close before returning */
	void EnsureCompletion();


	// Receiving Results (possibly while tick is running)
	struct FResults
	{
		TMultiMap<FName, FAssetData*> Assets;
		TRingBuffer<FString> Paths;
		TMultiMap<FName, FPackageDependencyData> Dependencies;
		TRingBuffer<FString> CookedPackageNamesWithoutAssetData;
		TRingBuffer<FName> VerseFiles;
		TArray<FString> BlockedFiles;

		SIZE_T GetAllocatedSize() const
		{
			return Assets.GetAllocatedSize() + Paths.GetAllocatedSize() + Dependencies.GetAllocatedSize() +
				CookedPackageNamesWithoutAssetData.GetAllocatedSize() + VerseFiles.GetAllocatedSize() +
				BlockedFiles.GetAllocatedSize();
		}
		void Shrink()
		{
			Assets.Shrink();
			Paths.Trim();
			Dependencies.Shrink();
			CookedPackageNamesWithoutAssetData.Trim();
			VerseFiles.Trim();
			BlockedFiles.Shrink();
		}
	};
	struct FResultContext
	{
		bool bIsSearching = false;
		bool bAbleToProgress = false;
		TArray<double> SearchTimes;
		int32 NumFilesToSearch = 0;
		int32 NumPathsToSearch = 0;
		bool bIsDiscoveringFiles = false;
	};
	/** Gets search results from the data gatherer. */
	void GetAndTrimSearchResults(FResults& InOutResults, FResultContext& OutContext);
	/** Get diagnostics for telemetry or logging. */
	FAssetGatherDiagnostics GetDiagnostics();
	/** Gets just the AssetResults and DependencyResults from the data gatherer. */
	void GetPackageResults(TMultiMap<FName, FAssetData*>& OutAssetResults,
		TMultiMap<FName, FPackageDependencyData>& OutDependencyResults);
	/**
	 * Wait for all monitored assets under the given path to be added to search results.
	 * Returns immediately if the given path is not monitored.
	 */
	void WaitOnPath(FStringView LocalPath);
	/**
	 * Empty the cache read from disk and the cache used to write to disk. Disable further caching.
	 * Used to save memory when cooking after the scan is complete.
	*/
	void ClearCache();

	/**
	 * Add a set of paths to the allow list, optionally force rescanning and ignore deny list on them,
	 * and wait for all assets in the paths to be added to search results.
	 * Wait time is minimized by prioritizing the paths and transferring async scanning to the current thread.
	 * If SaveCacheFilename is non-empty, save a cachefile to it with all discovered paths that are in one of
	 * the SaveCacheLongPackageNameDirs paths.
	 */
	void ScanPathsSynchronous(const TArray<FString>& InPaths, bool bForceRescan, bool bIgnoreDenyListScanFilters,
		const FString& SaveCacheFilename, const TArray<FString>& SaveCacheLongPackageNameDirs);
	/** Wait for all monitored assets to be added to search results. */
	void WaitForIdle(float TimeoutSeconds = -1.0f);
	/**
	 * Report whether all monitored assets have been added to search results, AND these results have been gathered
	 * through a GetAndTrimSearchResults call.
	 * This function can be used to check whether there is any work to be done on the gatherer.
	 */
	bool IsComplete() const;


	// Reading/writing/triggering depot-wide properties and events (possibly while tick is running)

	/** Set after initial plugins have loaded and we should not retry failed loads with missing custom versions. */
	void SetInitialPluginsLoaded();
	/** Report whether the gatherer is configured to load depends data in addition to asset data. */
	bool IsGatheringDependencies() const;
	/** Return whether the current process enables reading AssetDataGatherer cache files. */
	bool IsCacheReadEnabled() const;
	/** Return whether the current process enables writing AssetDataGatherer cache files. */
	bool IsCacheWriteEnabled() const;
	/** Calculate the cache filename that should be used for the given list of package paths. */
	FString GetCacheFilename(TConstArrayView<FString> CacheFilePackagePaths);
	/** Attempt to read the cache file at the given LocalPath, and store all of its results in the in-memory cache. */
	void LoadCacheFiles(TConstArrayView<FString> CacheFilename);
	/** Return the memory used by the gatherer. Used for performance metrics. */
	SIZE_T GetAllocatedSize() const;


	// Configuring mount points (possibly while tick is running)

	/** Add a mountpoint to the gatherer after it has been registered with FPackageName .*/
	void AddMountPoint(FStringView LocalPath, FStringView LongPackageName);
	/** Remove a previously added mountpoint. */
	void RemoveMountPoint(FStringView LocalPath);
	/** Add MountPoints in LocalPaths to the gatherer. */
	void AddRequiredMountPoints(TArrayView<FString> LocalPaths);


	// Reading/Writing properties of files and directories (possibly while tick is running)

	/** Called from DirectoryWatcher. Update the directory for reporting in future search results. */
	void OnDirectoryCreated(FStringView LocalPath);
	/** Called from DirectoryWatcher. Update the files for reporting in future search results. */
	void OnFilesCreated(TConstArrayView<FString> LocalPaths);
	/** Mark a file or directory to be scanned before unprioritized assets. */
	void PrioritizeSearchPath(const FString& PathToPrioritize);
	/**
	 * Mark whether a given path is in the scanning allow list.
	 *
	 * By default no paths are scanned; adding a path to the allow list causes it and its subdirectories to be scanned.
	 * Note that the deny list (InLongPackageNameDenyList) overrides the allow list.
	 * Allow list settings are recursive. Attempting to mark a path as allowed if a parent path is on the allow list
	 * will have no effect. This means the scenario ((1) add allow list A (2) add allow list A/Child (3) remove allow
	 * list A) will therefore not result in A/Child being allowed.
	 */
	void SetIsOnAllowList(FStringView LocalPath, bool bIsAllowed);
	/** Report whether the path is in the allow list. Only paths in AllowList AND not in DenyList will be scanned. */
	bool IsOnAllowList(FStringView LocalPath) const;
	/** Report whether the path is in the deny list. Paths in DenyList are not scanned. */
	bool IsOnDenyList(FStringView LocalPath) const;
	/** Report whether the path is both in the allow list and not in the deny list. */
	bool IsMonitored(FStringView LocalPath) const;

	/** Determine, based on the file extension, if the given file path is a Verse file */
	static bool IsVerseFile(FStringView FilePath);

	/**
	 * Reads FAssetData information out of a previously initialized package reader
	 *
	 * @param PackageReader the previously opened package reader
	 * @param AssetDataList the FAssetData for every asset found in the file
	 * @param DependencyData the FPackageDependencyData for every asset found in the file
	 * @param CookedPackagesToLoadUponDiscovery the list of cooked packages to be loaded if any
	 * @param Options Which bits of data to read
	 */
	static bool ReadAssetFile(FPackageReader& PackageReader, TArray<FAssetData*>& AssetDataList,
		FPackageDependencyData& DependencyData, TArray<FString>& CookedPackagesToLoadUponDiscovery,
		FPackageReader::EReadOptions Options);

private:
	enum class ETickResult
	{
		KeepTicking,
		PollDiscovery,
		Idle,
		Interrupt,
	};
	/**
	 * Helper function to run the tick in a loop-within-a-loop to minimize critical section entry, and to move expensive
	 * operations out of the critical section
	 */
	void InnerTickLoop(bool bInSynchronousTick, bool bContributeToCacheSave, double EndTimeSeconds);
	/**
	 * Tick function to pump scanning and push results into the search results structure. May be called from devoted
	 * thread or inline from synchronous functions on other threads.
	 */
	ETickResult TickInternal(double& TickStartTime, bool bPollDiscovery);
	/** Add any new package files from the background directory scan to our work list **/
	void IngestDiscoveryResults();

	/** Helper for OnFilesCreated. Update the file for reporting in future search results. */
	void OnFileCreated(FStringView LocalPath);

	/**
	 * Set a selection of directory-scanning properties on a given LocalPath.
	 * This function is used when multiple properties need to be set on the path and we want to avoid redundant
	 * tree-traversal costs.
	 */
	void SetDirectoryProperties(FStringView LocalPath, const UE::AssetDataGather::Private::FSetPathProperties& Properties);

	/**
	 * Wait for all monitored assets under the given path to be added to search results.
	 * Returns immediately if the given path are not monitored.
	 */
	void WaitOnPathsInternal(TArrayView<UE::AssetDataGather::Private::FPathExistence> QueryPaths,
		const FString& SaveCacheFilename, const TArray<FString>& SaveCacheFilterDirs);

	/** Sort the pending list of filepaths so that assets under the given directory/filename are processed first. */
	void SortPathsByPriority(TArrayView<UE::AssetDataGather::Private::FPathExistence> QueryPaths,
		UE::AssetDataGather::Private::EPriority Priority, int32& OutNumPaths);
	/**
	 * Reads FAssetData information out of a file
	 *
	 * @param AssetLongPackageName the package name of the file to read
	 * @param AssetFilename the local path of the file to read
	 * @param AssetDataList the FAssetData for every asset found in the file
	 * @param DependencyData the FPackageDependencyData for every asset found in the file
	 * @param CookedPackagesToLoadUponDiscovery the list of cooked packages to be loaded if any
	 * @param OutCanRetry Set to true if this file failed to load, but might be loadable later (due to missing modules)
	 *
	 * @return true if the file was successfully read
	 */
	bool ReadAssetFile(const FString& AssetLongPackageName, const FString& AssetFilename,
		TArray<FAssetData*>& AssetDataList, FPackageDependencyData& DependencyData,
		TArray<FString>& CookedPackagesToLoadUponDiscovery, bool& OutCanRetry) const;

	/** Add the given AssetDatas into DiskCachedAssetDataMap and DiskCachedAssetBlocks. */
	void ConsumeCacheFiles(TArray<UE::AssetDataGather::Private::FCachePayload> Payloads);
	/**
	 * If a save of the monolithic cache has been triggered, get the cache filename and pointers to all elements that
	 * should be saved, for later saving outside of the critical section.
	 */
	void TryReserveSaveMonolithicCache(bool& bOutShouldSave, TArray<TPair<FName,FDiskCachedAssetData*>>& AssetsToSave);
	/** 
	 * Save a monolithic cache for the main asset discovery process, possibly sharded into multiple files.
	*/
	void SaveMonolithicCacheFile(const TArray<TPair<FName,FDiskCachedAssetData*>>& AssetsToSave);
	/**
	 * If the CacheFilename/AssetsToSave are non empty, save the cache file. 
	 * This function reads the read-only-after-creation data from each FDiskCachedAssetData*, but otherwise does not use
	 * data from this Gatherer and so can be run outside any critical section.
	 * Returns the size of the saved file, or 0 if nothing was saved for any reason.
	 */
	int64 SaveCacheFileInternal(const FString& CacheFilename,
		const TArray<TPair<FName,FDiskCachedAssetData*>>& AssetsToSave);
	/**
	 * Get the list of FDiskCachedAssetData* that have been loaded in the gatherer, for saving into a cachefile.
	 * Filters the list of assets by child paths of the elements in SaveCacheLongPackageNameDirs, if it is non-empty.
	 */
	void GetAssetsToSave(TArrayView<const FString> SaveCacheLongPackageNameDirs,
		TArray<TPair<FName,FDiskCachedAssetData*>>& OutAssetsToSave);
	/**
	 * Get the list of FDiskCachedAssetData* for saving into the monolithic cache.
	 * Includes both assets that were loaded in the gatherer and assets which were loaded from the monolithic cache and have not been pruned.
	 */
	void GetMonolithicCacheAssetsToSave(TArray<TPair<FName,FDiskCachedAssetData*>>& OutAssetsToSave);

	/* Adds the given pair into NewCachedAssetDataMap. Detects collisions for multiple files with the same PackageName */
	void AddToCache(FName PackageName, FDiskCachedAssetData* DiskCachedAssetData);

	/**
	 * Mark that the gatherer has become idle or has become active. Called from tick function and configuration functions
	 * when they note a possible state change. Caller is responsible for holding the ResultsLock.
	 */
	void SetIsIdle(bool IsIdle);
	void SetIsIdle(bool IsIdle, double& TickStartTime);

	/** Minimize memory usage in the buffers used during gathering. */
	void Shrink();

	/** Scoped guard for pausing the asynchronous tick. */
	struct FScopedPause
	{
		FScopedPause(const FAssetDataGatherer& InOwner);
		~FScopedPause();
		const FAssetDataGatherer& Owner;
	};

	/** Convert the LocalPath into our normalized version. */
	static FString NormalizeLocalPath(FStringView LocalPath);
	/** Convert the LongPackageName into our normalized version. */
	static FStringView NormalizeLongPackageName(FStringView LongPackageName);

	void OnAllModuleLoadingPhasesComplete();
private:

	/**
	 * Critical section to allow Tick to be called from worker thread or from synchronous functions on other threads.
	 * To prevent DeadLocks, TickLock can not be entered from within any of the other locks on this class.
	 */
	mutable FGathererCriticalSection TickLock;
	/**
	 * A critical section to protect data transfer to GetAndTrimSearchResults.
	 * ResultsLock can be entered while holding TickLock.
	 */
	mutable FGathererCriticalSection ResultsLock;


	// Variable section for variables that are constant during threading.

	/**
	 * Thread to run async Ticks on. Constant during threading.
	 * Activated when StartAsync is called and bAsyncEnabled is true.
	 * If null, results will only be added when Wait functions are called. Constant during threading.
	 */
	FRunnableThread* Thread;
	int32 TickInternalBatchSize = 1;

	/**
	 * True if async gathering is enabled, false if e.g. singlethreaded or disabled by commandline.
 	 * Even when enabled, gathering is still synchronous until StartAsync is called.
	 */
	bool bAsyncEnabled;
	/** True if AssetPackageData should be gathered. Constant during threading. */
	bool bGatherAssetPackageData;
	/** True if dependency data should be gathered. Constant during threading. */
	bool bGatherDependsData;

	/** Timestamp of the start of the gather for consistent marking of 'last discovered' time in caching */
	FDateTime GatherStartTime;

	// Variable section for variables that are atomics read/writable from outside critical sections.

	/** > 0 if we've been asked to abort work in progress at the next opportunity. */
	std::atomic<uint32> IsStopped;
	/** > 0 if we've been asked to pause the worker thread so a synchronous function can take over the tick. */
	mutable std::atomic<uint32> IsPaused;
	/**
	 * Discovery subsystem; decides which paths to search and queries the FileManager to search directories.
	 * Pointer is constant during threading. Object pointed to internally provides threadsafety.
	 */
	TUniquePtr<UE::AssetDataGather::Private::FAssetDataDiscovery> Discovery;
	/** Async only. Set to true once initial plugins have been loaded. */
	std::atomic<bool> bInitialPluginsLoaded;
	/** True when TickInternal requests periodic or final save of the async cache. */
	std::atomic<bool> bSaveAsyncCacheTriggered;
	/** True if the current process allows reading of AssetDataGatherer cache files. */
	std::atomic<bool> bCacheReadEnabled;
	/** True if the current process allows writing of AssetDataGatherer cache files. */
	std::atomic<bool> bCacheWriteEnabled;

	// Variable section for variables that are read/writable only within ResultsLock.

	/** List of files that need to be processed by the search. */
	TUniquePtr<UE::AssetDataGather::Private::FFilesToSearch> FilesToSearch;

	/** The asset data gathered from the searched files. */
	TArray<FAssetData*> AssetResults;
	/** Dependency data gathered from the searched files packages. */
	TArray<FPackageDependencyData> DependencyResults;
	/**
	 * A list of cooked packages that did not have asset data in them.
	 * These assets may still contain assets (if they were older for example). 
	 */
	TArray<FString> CookedPackageNamesWithoutAssetDataResults;
	/** File paths (in UE LongPackagePath notation) of the Verse source code gathered from the searched files. */
	TArray<FName> VerseResults;
	/** File paths (in regular filesystem notation) of blocked packages from the searched files. */
	TArray<FString> BlockedResults;

	/** All the search times since the last call to GetAndTrimSearchResults. */
	TArray<double> SearchTimes;
	/** Sum of all SearchTimes. */
	float CumulativeGatherTime = 0.f;

	/** The directories found during the search, unless they are hidden by DirLongPackageNamesToNotReport. */
	TArray<FString> DiscoveredPaths;

	/** The time spent in TickInternal since the last idle time. Used for performance metrics when reporting results. */
	double CurrentSearchTime = 0.;
	/** The last time at which the cache file was written, used to periodically update the cache. */
	double LastCacheWriteTime;
	/** The cached value of the NumPathsToSearch returned by Discovery the last time we synchronized with it. */
	int32 NumPathsToSearchAtLastSyncPoint;
	/** The total number of files in the search results that were read from the cache. */
	int32 NumCachedAssetFiles = 0;
	/** The total number of files in the search results that were not in the cache and were read by parsing the file. */
	int32 NumUncachedAssetFiles = 0;
	/**
	 * Track whether we are allowed to read from a monolithic cache that should be loaded during tick.
	 * Even if we are or not, if bCacheReadEnabled the AssetRegistry can also call LoadCacheFile/ScanPathsSynchronous to
	 * load/save smaller files.
	 */
	bool bReadMonolithicCache;
	/** Track whether we are allowed to write to the monolithic cache. */
	bool bWriteMonolithicCache;
	/** If bHasLoadedMonolithicCache is true, track whether the cache has been loaded. */
	bool bHasLoadedMonolithicCache;
	/** Track whether the Discovery subsystem has gone idle and we have read all filenames from it. */
	bool bDiscoveryIsComplete;
	/** Track whether this Gather has gone idle and a caller has read all search data from it. */
	bool bIsComplete;
	/** Track whether this Gatherer has gone idle, either it has no more work or it's blocked on external events. */
	bool bIsIdle;
	/** Track the first tick after idle to set up e.g. timing data. */
	bool bFirstTickAfterIdle;
	/** True if we have finished discovering our first wave of files, to report metrics for that most-important wave. */
	bool bFinishedInitialDiscovery;

	// Variable section for variables that are read/writable only within TickLock.

	/**
	 * An array of all cached data that was newly discovered this run. This array is just used to make sure they are all
	 * deleted at shutdown.
	 */
	TArray<FDiskCachedAssetData*> NewCachedAssetData;
	TArray<TPair<int32, FDiskCachedAssetData*>> DiskCachedAssetBlocks;
	/**
	 * Map of PackageName to cached discovered assets that were loaded from disk.
	 * This should only be modified by ConsumeCacheFiles.
	 */
	TMap<FName, FDiskCachedAssetData*> DiskCachedAssetDataMap;
	/** Map of PackageName to cached discovered assets that will be written to disk at shutdown. */
	TMap<FName, FDiskCachedAssetData*> NewCachedAssetDataMap;
	/** Used to block on gather results. If non-negative, tick should end when WaitBatchCount files have been processed. */
	int32 WaitBatchCount;
	/** How many uncached asset files had been discovered at the last async cache save */
	int32 LastMonolithicCacheSaveUncachedAssetFiles;
	/**
	 * Incremented when a thread is in the middle of saving any cache and therefore the cache cannot be deleted,
	 * decremented when the thread is done. Only incremented when bCacheEnabled has been recently confirmed to be true.
	 */
	int32 CacheInUseCount;
	/**
	 * True if the current TickInternal is synchronous, which may be because !IsSynchronous or because the game thread has
	 * taken over the tick for a synchronous function.
	 */
	bool bSynchronousTick;
	/** True when a thread is saving an async cache and so another save of the cache should not be triggered. */
	bool bIsSavingAsyncCache;
	/** Packages can be marked for retry up until bInitialPluginsLoaded is set. After it is set, we retry them once. */
	bool bFlushedRetryFiles;
};
