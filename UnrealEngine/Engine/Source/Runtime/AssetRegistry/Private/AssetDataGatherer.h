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
#include "PackageReader.h"
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

/**
 * Async task for gathering asset data from from the file list in FAssetRegistry
 */
class FAssetDataGatherer : public FRunnable
{
public:
	FAssetDataGatherer(const TArray<FString>& InLongPackageNamesDenyList, const TArray<FString>& InMountRelativePathsDenyList, bool bInIsSynchronous);
	virtual ~FAssetDataGatherer();


	// Extra at-construction configuration 

	/** Configure the gatherer to use a single monolithic cache for all files, and read/write this cache during ticks. */
	void ActivateMonolithicCache();


	// Controlling Async behavior

	/** Start the async thread, if this Gatherer was created async. Does nothing if not async or already started. */
	void StartAsync();

	// FRunnable implementation
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

	bool IsSynchronous() const;
	/** Signals to end the thread and waits for it to close before returning */
	void EnsureCompletion();


	// Receiving Results (possibly while tick is running)
	struct FResults
	{
		TRingBuffer<FAssetData*> Assets;
		TRingBuffer<FString> Paths;
		TRingBuffer<FPackageDependencyData> Dependencies;
		TRingBuffer<FString> CookedPackageNamesWithoutAssetData;
		TRingBuffer<FName> VerseFiles;

		SIZE_T GetAllocatedSize() const { return Assets.GetAllocatedSize() + Paths.GetAllocatedSize() + Dependencies.GetAllocatedSize() + CookedPackageNamesWithoutAssetData.GetAllocatedSize() + VerseFiles.GetAllocatedSize(); }
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
	/** Gets just the AssetResults and DependencyResults from the data gatherer. */
	void GetPackageResults(TRingBuffer<FAssetData*>& OutAssetResults, TRingBuffer<FPackageDependencyData>& OutDependencyResults);
	/** Wait for all monitored assets under the given path to be added to search results. Returns immediately if the given path is not monitored. */
	void WaitOnPath(FStringView LocalPath);
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
	void WaitForIdle();
	/**
	 * Report whether all monitored assets have been added to search results, AND these results have been gathered through a GetAndTrimSearchResults call.
	 * This function can be used to check whether there is any work to be done on the gatherer.
	 */
	bool IsComplete() const;


	// Reading/writing/triggering depot-wide properties and events (possibly while tick is running)

	/** Inform the gatherer that initial plugins have finished loading and it should not retry failed loads in hope of a missing custom version */
	void SetInitialPluginsLoaded();
	/** Report whether the gatherer is configured to load depends data in addition to asset data. */
	bool IsGatheringDependencies() const;
	/** Return whether the current process enables reading and writing AssetDataGatherer cache files. */
	bool IsCacheEnabled() const;
	/** Calculate the cache filename that should be used for the given list of package paths. */
	FString GetCacheFilename(TConstArrayView<FString> CacheFilePackagePaths);
	/** Attempt to read the cache file at the given LocalPath, and store all of its results in the in-memory cache. */
	void LoadCacheFile(FStringView CacheFilename);
	/** Return the total amount of heap memory used by the gatherer (including not-yet-claimed search results). Used for performance metrics. */
	SIZE_T GetAllocatedSize() const;


	// Configuring mount points (possibly while tick is running)

	/** Add a mountpoint to the gatherer. This should be the localpath to the packagepath/localpath pair that has been registered with FPackageName .*/
	void AddMountPoint(FStringView LocalPath, FStringView LongPackageName);
	/** Remove a previously added mountpoint. */
	void RemoveMountPoint(FStringView LocalPath);
	/** For each LocalPath in LocalPaths, query FPackageName for its mount point, and add this mountpoint to the gatherer. */
	void AddRequiredMountPoints(TArrayView<FString> LocalPaths);


	// Reading/Writing properties of files and directories (possibly while tick is running)

	/** Called from DirectoryWatcher. Update the directory in internal structures and report it as necessary in a future search result. */
	void OnDirectoryCreated(FStringView LocalPath);
	/** Called from DirectoryWatcher. Update the files in internal structures and report them as necessary in a future search result. */
	void OnFilesCreated(TConstArrayView<FString> LocalPaths);
	/** Mark a file or directory to be scanned before unprioritized assets. */
	void PrioritizeSearchPath(const FString& PathToPrioritize);
	/**
	 * Mark whether a given path is in the scanning allow list.
	 *
	 * By default no paths are scanned; adding a path to the allow list causes it and its subdirectories to be scanned.
	 * Note that the deny list (InLongPackageNameDenyList) overrides the allow list.
	 * Allow list settings are recursive. Attempting to mark a path as allowed if a parent path is on the allow list
	 * will have no effect. This means the scenario (1) add allow list A (2) add allow list A/Child (3) remove allow list A will
	 * therefore not result in A/Child being allowed.
	 */
	void SetIsOnAllowList(FStringView LocalPath, bool bIsAllowed);
	/** Report whether the path is in the allow list. If not it will not be scanned. If so it will be scanned unless it is in the deny list. */
	bool IsOnAllowList(FStringView LocalPath) const;
	/** Report whether the path is in the deny list. If so it will not be scanned regardless of whether it is in the allow list. */
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

	/**
	 * Helper function to run the tick in a loop-within-a-loop to minimize critical section entry, and to move expensive
	 * operations out of the critical section
	 */
	void InnerTickLoop(bool bInIsSynchronousTick, bool bContributeToCacheSave);
	/**
	 * Tick function to pump scanning and push results into the search results structure. May be called from devoted thread
	 * or inline from synchronous functions on other threads.
	 */
	void TickInternal(bool& bOutIsIdle);
	/** Add any new package files from the background directory scan to our work list **/
	void IngestDiscoveryResults();

	/** Helper for OnFilesCreated. Update the file in internal structures and report it as necessary in a future search result. */
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
	void ConsumeCacheFile(UE::AssetDataGather::Private::FCachePayload&& Payload);
	/**
	 * If a save of the monolithic cache has been triggered, get the cache filename and pointers to all elements that should be saved,
	 * for later saving outside of the critical section.
	 */
	void TryReserveSaveMonolithicCache(bool& bOutShouldSave, TArray<TPair<FName,FDiskCachedAssetData*>>& AssetsToSave);
	/**
	 * If the CacheFilename/AssetsToSave are non empty, save the cache file. 
	 * This function reads the read-only-after-creation data from each FDiskCachedAssetData*, but otherwise does not use
	 * data from this Gatherer and so can be run outside any critical section.
	 */
	void SaveCacheFileInternal(const FString& CacheFilename, const TArray<TPair<FName,FDiskCachedAssetData*>>& AssetsToSave, bool bIsAsyncCacheSave);
	/**
	 * Get the list of FDiskCachedAssetData* that have been loaded in the gatherer, for saving into a cachefile.
	 * Filters the list of assets by child paths of the elements in SaveCacheLongPackageNameDirs, if it is non-empty.
	 */
	void GetAssetsToSave(TArrayView<const FString> SaveCacheLongPackageNameDirs, TArray<TPair<FName,FDiskCachedAssetData*>>& OutAssetsToSave);

	/* Adds the given PackageName,DiskCachedAssetData pair into NewCachedAssetDataMap, and detects collisions for multiple files with the same PackageName */
	void AddToCache(FName PackageName, FDiskCachedAssetData* DiskCachedAssetData);

	/**
	 * Mark that the gatherer has become idle or has become active. Called from tick function and configuration functions when they note a possible state change.
	 * Caller is responsible for holding the ResultsLock.
	 */
	void SetIsIdle(bool IsIdle);

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

	/** Thread to run async Ticks on. Constant during threading. */
	FRunnableThread* Thread;
	/**
	 * True if this Gatherer is synchronous, false if it has a worker thread. If synchronous, results will only be
	 * added when Wait functions are called. Constant during threading.
	 */
	bool bIsSynchronous;
	/** True if AssetPackageData should be gathered. Constant during threading. */
	bool bGatherAssetPackageData;
	/** True if dependency data should be gathered. Constant during threading. */
	bool bGatherDependsData;
	/** True if the current process allows reading/writing of AssetDataGatherer cache files. Constant during threading. */
	bool bCacheEnabled;


	// Variable section for variables that are atomics read/writable from outside critical sections.

	/** > 0 if we've been asked to abort work in progress at the next opportunity. Read/writable anywhere. */
	std::atomic<uint32> IsStopped;
	/** > 0 if we've been asked to pause the worker thread so a synchronous function can take over the tick. Read/writable anywhere */
	mutable std::atomic<uint32> IsPaused;
	/**
	 * Discovery subsystem; this subsystem handles deciding which paths to search and running the FileManager calls to search directories.
	 * Pointer itself is constant during threading. Object pointed to is read/writable anywhere; internally provides threadsafety.
	 */
	TUniquePtr<UE::AssetDataGather::Private::FAssetDataDiscovery> Discovery;
	/** Async only. Set to true once initial plugins have been loaded. Read/writable anywhere. */
	std::atomic<bool> bInitialPluginsLoaded;
	/**
	 * Set to true when periodic or final save of the async cache was detected during TickInternal and should be processed outside of the Tick.
	 * Read/writable anywhere.
	 */
	std::atomic<bool> bSaveAsyncCacheTriggered;

	// Variable section for variables that are read/writable only within ResultsLock.

	/** List of files that need to be processed by the search. Read/writable only within ResultsLock. */
	TUniquePtr<UE::AssetDataGather::Private::FFilesToSearch> FilesToSearch;

	/** The asset data gathered from the searched files. Read/writable only within ResultsLock. */
	TArray<FAssetData*> AssetResults;
	/** Dependency data gathered from the searched files packages. Read/writable only within ResultsLock. */
	TArray<FPackageDependencyData> DependencyResults;
	/**
	 * A list of cooked packages that did not have asset data in them.
	 * These assets may still contain assets (if they were older for example). Read/writable only within ResultsLock.
	 */
	TArray<FString> CookedPackageNamesWithoutAssetDataResults;
	/** File paths (in UE LongPackagePath notation) of the Verse source code gathered from the searched files. */
	TArray<FName> VerseResults;

	/** All the search times since the last call to GetAndTrimSearchResults. Read/writable only within ResultsLock. */
	TArray<double> SearchTimes;
	/**
	 * The directories found during the search, unless they are hidden by DirLongPackageNamesToNotReport.
	 * Read/writable only within ResultsLock.
	 */
	TArray<FString> DiscoveredPaths;

	/** The current search start time, set in the first tick after idle and used for performance metrics when reporting results. Read/writable only within ResultsLock. */
	double SearchStartTime;
	/** The last time at which the cache file was written, used to periodically update the cache with the results of ticking. Read/writable only within ResultsLock. */
	double LastCacheWriteTime;
	/**
	 * The cached value of the NumPathsToSearch returned by the Discovery subsystem the last time we synchronized with it.
	 * Read/writable only within ResultsLock.
	 */
	int32 NumPathsToSearchAtLastSyncPoint;
	/**
	 * Track whether we are using a monolithic cache that should be loaded/saved during tick.
	 * Wether we are or not, the AssetRegistry can also call LoadCacheFile/ScanPathsSynchronous to load/save smaller files.
	 * Read/writable only within ResultsLock.
	 */
	bool bUseMonolithicCache;
	/** If bHasLoadedMonolithicCache is true, track whether the cache has been loaded. Read/writable only within ResultsLock. */
	bool bHasLoadedMonolithicCache;
	/** Track whether the Discovery subsystem has gone idle and we have read all filenames from it. Read/writable only within ResultsLock. */
	bool bDiscoveryIsComplete;
	/** Track whether this Gather has gone idle and a caller has read all search data from it. Read/writable only within ResultsLock. */
	bool bIsComplete;
	/** Track whether this Gatherer has gone idle, either because of no more work or because its blocked on external events. Read/writable only within ResultsLock. */
	bool bIsIdle;
	/** Track the first tick after idle to set up e.g. timing data. Read/writable only within ResultsLock. */
	bool bFirstTickAfterIdle;
	/** True if we have finished discovering our first wave of files, to report metrics for that most-important wave. Read/writable only within ResultsLock. */
	bool bFinishedInitialDiscovery;


	// Variable section for variables that are read/writable only within TickLock.

	/**
	 * An array of all cached data that was newly discovered this run. This array is just used to make sure they are all deleted at shutdown.
	 * Read/writable only within TickLock.
	 */
	TArray<FDiskCachedAssetData*> NewCachedAssetData;
	TArray<TPair<int32, FDiskCachedAssetData*>> DiskCachedAssetBlocks;
	/**
	 * Map of PackageName to cached discovered assets that were loaded from disk.
	 * This should only be modified by ConsumeCacheFile.
	 * Read/Writable only within TickLock.
	 */
	TMap<FName, FDiskCachedAssetData*> DiskCachedAssetDataMap;
	/** Map of PackageName to cached discovered assets that will be written to disk at shutdown. Read/writable only within TickLock. */
	TMap<FName, FDiskCachedAssetData*> NewCachedAssetDataMap;
	/**
	 * Used when we are blocking on gather results. If non-zero, we should end the tick when WaitBatchCount files have been processed.
	 * Read/writable only within TickLock.
	 */
	int32 WaitBatchCount;
	/** How many files in the search results were read from the cache. Read/writable only within TickLock. */
	int32 NumCachedAssetFiles;
	/** How many files in the search results were not in the cache and were read by parsing the file. Read/writable only within TickLock. */
	int32 NumUncachedAssetFiles;
	/**
	 * True if the current TickInternal is synchronous, which may be because bIsSynchronous or because the game thread has taken over the tick for a synchronous function.
	 * Read/writable only within TickLock.
	 */
	bool bIsSynchronousTick;
	/**
	 * Set to true when a thread is in the middle of saving an async cache and so another save of the cache should not be triggered.
	 * Read/writable only within TickLock.
	 */
	bool bIsSavingAsyncCache;

};
