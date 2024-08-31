// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetRegistry.h"

#include "Algo/Unique.h"
#include "AssetDataGatherer.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetDependencyGatherer.h"
#include "AssetRegistry/AssetRegistryTelemetry.h"
#include "AssetRegistryConsoleCommands.h"
#include "AssetRegistryPrivate.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "Blueprint/BlueprintSupport.h"
#include "DependsNode.h"
#include "GenericPlatform/GenericPlatformChunkInstall.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformMisc.h"
#include "HAL/ThreadHeartBeat.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Optional.h"
#include "Misc/PackageAccessTracking.h"
#include "Misc/PackageAccessTrackingOps.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "Misc/RedirectCollector.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/TrackedActivity.h"
#include "AssetRegistry/PackageReader.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "Serialization/ArrayReader.h"
#include "Serialization/CompactBinarySerialization.h"
#include "Serialization/CompactBinaryWriter.h"
#include "String/RemoveFrom.h"
#include "Templates/UnrealTemplate.h"
#include "TelemetryRouter.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/CoreRedirects.h"
#include "UObject/MetaData.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectThreadContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IAssetRegistry)
#include UE_INLINE_GENERATED_CPP_BY_NAME(AssetRegistry)

#if WITH_EDITOR
#include "DirectoryWatcherModule.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "IDirectoryWatcher.h"
#endif // WITH_EDITOR

/**
 * ********** AssetRegistry threading model **********
 * *** Functions and InterfaceLock ***
 * All data(except events and RWLock) in the AssetRegistry is stored on the FAssetRegistryImpl GuardedData object.
 * No data can be read on GuardedData unless the caller has entered the InterfaceLock.
 * All data on FAssetRegistryImpl is private; this allows us to mark threading model with function prototypes.
 * All functions on FAssetRegistryImpl are intended to be called only within a critical section.
 * const functions require a ReadLock critical section; non-const require a WriteLock.
 * The requirement that functions must be called only from within a critical section(and non-const only within a
 * WriteLock) is not enforced technically; change authors need to carefully follow the synchronization model.

 * *** Events, Callbacks, and Object Virtuals ***
 * The AssetRegistry provides several Events(e.g.AssetAddedEvent) that can be subscribed to from arbitrary engine or
 * licensee code, and some functions(e.g.EnumerateAssets) take a callback, and some functions call arbitrary
 * UObject virtuals(e.g. new FAssetData(UObject*)). Some of this arbitrary code can call AssetRegistry functions of
 * their own, and if they were called from within the lock that reentrancy would cause a deadlock when we tried
 * to acquire the RWLock (RWLocks are not reenterable on the same thread). With some exceptions AssetRegistryImpl code
 * is therefore not allowed to call callbacks, send events, or call UObject virtuals from inside a lock.

 * FEventContext allows deferring events to a point in the top-level interface function outside the lock. The top-level
 * function passes the EventContext in to the GuardedData functions, which add events on to it, and then it broadcasts
 * the events outside the lock. FEventContext also handles deferring events to the Tick function executed from
 * the GameThread, as we have a contract that events are only called from the game thread.

 * Callbacks are handled on a case-by-case basis; each interface function handles queuing up the data for the callback
 * functions and calling it outside the lock. The one exception is the ShouldSetManager function, which we call
 * from inside the lock, since it is relatively well-behaved code as it is only used by UAssetManager and licensee
 * subclasses of UAssetManager.

 * UObject virtuals are handled on a case-by-case basis; the primary example is `new FAssetData(UObject*)`, which
 * ProcessLoadedAssetsToUpdateCache takes care to call outside the lock and only on the game thread.
 *
 * *** Updating Caches - InheritanceContext ***
 * The AssetRegistry has a cache for CodeGeneratorClasses and for an InheritanceMap of classes - native and blueprint.
 * Updating these caches needs to be done within a writelock; for CodeGeneratorClasses we do this normally by marking
 * all functions that need to update it as non-const. For InheritanceMap that would be overly pessimistic as several
 * otherwise-const functions need to occasionally update the caches. For InheritanceMap we therefore have
 * FClassInheritanceContext and FClassInheritanceBuffer. The top-level interface functions check whether the
 * inheritance map will need to be updated during their execution, and if so they enter a write lock with the ability
 * to update the members in the InheritanceContext. Otherwise they enter a readlock and the InheritanceBuffer will not
 * be modified. All functions that use the cached data require the InheritanceContext to give them access, to ensure
 * they are only using correctly updated cache data.
 *
 * *** Returning Internal Data ***
 * All interface functions that return internal data return it by copy, or provide a ReadLockEnumerate function that
 * calls a callback under the readlock, where the author of the callback has to ensure other AssetRegistry functions
 * are not called.
 */

static FAssetRegistryConsoleCommands ConsoleCommands; // Registers its various console commands in the constructor

namespace UE::AssetRegistry
{
	const FName WildcardFName(TEXT("*"));
	const FTopLevelAssetPath WildcardPathName(TEXT("/*"), TEXT("*"));

	const FName Stage_ChunkCountFName(TEXT("Stage_ChunkCount"));
	const FName Stage_ChunkSizeFName(TEXT("Stage_ChunkSize"));
	const FName Stage_ChunkCompressedSizeFName(TEXT("Stage_ChunkCompressedSize"));
	const FName Stage_ChunkInstalledSizeFName(TEXT("Stage_ChunkInstalledSize"));
	const FName Stage_ChunkStreamingSizeFName(TEXT("Stage_ChunkStreamingSize"));
	const FName Stage_ChunkOptionalSizeFName(TEXT("Stage_ChunkOptionalSize"));
	
	FString LexToString(EScanFlags Flags)
	{
		const TCHAR* Names[] = {
			TEXT("ForceRescan"),
			TEXT("IgnoreDenyListScanFilters"),
			TEXT("WaitForInMemoryObjects")
		};
		
		if (Flags == EScanFlags::None)
		{
			return TEXT("None");
		}
		
		uint32 AllKnownFlags = (1 << (UE_ARRAY_COUNT(Names)+1)) - 1;
		ensureMsgf(EnumHasAllFlags((EScanFlags)AllKnownFlags, Flags), TEXT("LexToString(UE::AssetRegistry::EScanFlags) is missing some cases"));

		TStringBuilder<256> Builder;
		for (uint32 i=0; i < UE_ARRAY_COUNT(Names); ++i)
		{
			if (EnumHasAllFlags(Flags, (EScanFlags)(1 << i)))
			{
				if (Builder.Len() != 0)
				{
					Builder << TEXT("|");
				}
				Builder << Names[i];	
			}
		}
		
		return Builder.ToString();
	}
}

namespace UE::AssetRegistry::Impl
{
	/** The max time to spend in UAssetRegistryImpl::Tick */
	float MaxSecondsPerFrame = 0.04f;
	static FAutoConsoleVariableRef CVarAssetRegistryMaxSecondsPerFrame(
		TEXT("AssetRegistry.MaxSecondsPerFrame"),
		UE::AssetRegistry::Impl::MaxSecondsPerFrame,
		TEXT("Maximum amount of time allowed for Asset Registry processing, in seconds"));

	/** If true, defer sorting of dependencies until loading is complete */
	bool bDeferDependencySort = false;
	static FAutoConsoleVariableRef CVarAssetRegistryDeferDependencySort(
		TEXT("AssetRegistry.DeferDependencySort"),
		UE::AssetRegistry::Impl::bDeferDependencySort,
		TEXT("If true, the dependency lists on dependency nodes will not be sorted until after the initial load is complete"));

	/** If true, defer sorting of referencer data until loading is complete, this is enabled by default because of native packages with many referencers */
	bool bDeferReferencerSort = true;
	static FAutoConsoleVariableRef CVarAssetRegistryDeferReferencerSort(
		TEXT("AssetRegistry.DeferReferencerSort"),
		UE::AssetRegistry::Impl::bDeferReferencerSort,
		TEXT("If true, the referencer list on dependency nodes will not be sorted until after the initial load is complete"));

	/** Name of UObjectRedirector property */
	const FName DestinationObjectFName(TEXT("DestinationObject"));
}

/**
 * Implementation of IAssetRegistryInterface; forwards calls from the CoreUObject-accessible IAssetRegistryInterface into the AssetRegistry-accessible IAssetRegistry
 */
class FAssetRegistryInterface : public IAssetRegistryInterface
{
public:
	virtual void GetDependencies(FName InPackageName, TArray<FName>& OutDependencies,
		UE::AssetRegistry::EDependencyCategory Category = UE::AssetRegistry::EDependencyCategory::Package,
		const UE::AssetRegistry::FDependencyQuery& Flags = UE::AssetRegistry::FDependencyQuery()) override
	{
		IAssetRegistry::GetChecked().GetDependencies(InPackageName, OutDependencies, Category, Flags);
	}

	virtual UE::AssetRegistry::EExists TryGetAssetByObjectPath(const FSoftObjectPath& ObjectPath, FAssetData& OutAssetData) const override
	{
		auto AssetRegistry = IAssetRegistry::Get();
		if (!AssetRegistry)
		{
			return UE::AssetRegistry::EExists::Unknown;
		}
		return AssetRegistry->TryGetAssetByObjectPath(ObjectPath, OutAssetData);
	}

	virtual UE::AssetRegistry::EExists TryGetAssetPackageData(FName PackageName, class FAssetPackageData& OutPackageData) const override
	{
		auto AssetRegistry = IAssetRegistry::Get();
		if (!AssetRegistry)
		{
			return UE::AssetRegistry::EExists::Unknown;
		}
		return AssetRegistry->TryGetAssetPackageData(PackageName, OutPackageData);
	}
};
FAssetRegistryInterface GAssetRegistryInterface;

// Caching is permanently enabled in editor because memory is not that constrained, disabled by default otherwise
#define ASSETREGISTRY_CACHE_ALWAYS_ENABLED (WITH_EDITOR)

DEFINE_LOG_CATEGORY(LogAssetRegistry);


namespace UE::AssetRegistry::Premade
{

/** Returns whether the given executable configuration supports AssetRegistry Preloading. Called before Main. */
static bool IsEnabled()
{
	return (FPlatformProperties::RequiresCookedData() && (IsRunningGame() || IsRunningDedicatedServer()))
		|| ASSETREGISTRY_ENABLE_PREMADE_REGISTRY_IN_EDITOR;
}

static bool CanLoadAsync()
{
	// TaskGraphSystemReady callback doesn't really mean it's running
	return FPlatformProcess::SupportsMultithreading() && FTaskGraphInterface::IsRunning();
}

/** Returns the paths to possible Premade AssetRegistry files, ordered from highest priority to lowest. */
TArray<FString, TInlineAllocator<2>> GetPriorityPaths()
{
	TArray<FString, TInlineAllocator<2>> Paths;
#if ASSETREGISTRY_ENABLE_PREMADE_REGISTRY_IN_EDITOR
	Paths.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("EditorClientAssetRegistry.bin")));
#endif
	Paths.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("AssetRegistry.bin")));
	return Paths;
}

enum class ELoadResult : uint8
{
	Succeeded = 0,
	NotFound = 1,
	FailedToLoad = 2,
	Inactive = 3,
	AlreadyConsumed = 4,
	UninitializedMemberLoadResult = 5,
};

// Loads cooked AssetRegistry.bin using an async preload task if available and sync otherwise
class FPreloader
{
public:
	enum class EConsumeResult
	{
		Succeeded,
		Failed,
		Deferred
	};
	using FConsumeFunction = TFunction<void(ELoadResult LoadResult, FAssetRegistryState&& ARState)>;

	FPreloader()
	{
		if (UE::AssetRegistry::Premade::IsEnabled())
		{
			LoadState = EState::NotFound;

			// run DelayedInitialize when TaskGraph system is ready
			OnTaskGraphReady.Emplace(STATS ? EDelayedRegisterRunPhase::StatSystemReady :
											 EDelayedRegisterRunPhase::TaskGraphSystemReady,
				[this]()
			{
				DelayedInitialize();
			}); 
		}
	}
	~FPreloader()
	{
		// We are destructed after Main exits, which means that our AsyncThread was either never called
		// or it was waited on to complete by TaskGraph. Therefore we do not need to handle waiting for it ourselves.
		Shutdown(true /* bFromGlobalDestructor */);
	}

	/**
	 * Block on any pending async load, load if synchronous, and call ConsumeFunction with the results before returning.
	 * If Consume has been called previously, the current ConsumeFunction is ignored and this call returns false.
	 * 
	 * @return Whether the load succeeded (this information is also passed to the ConsumeFunction).
	 */
	bool Consume(FConsumeFunction&& ConsumeFunction)
	{
		EConsumeResult Result = ConsumeInternal(MoveTemp(ConsumeFunction), FConsumeFunction());
		check(Result != EConsumeResult::Deferred);
		return Result == EConsumeResult::Succeeded;
	}

	/**
	 * If a load is pending, store ConsumeAsynchronous for later calling and return EConsumeResult::Deferred.
	 * If load is complete, or failed, or needs to run synchronously, load if necessary and call ConsumeSynchronous with results before returning.
	 * Note if this function returns EConsumeResult::Deferred, the ConsumeAsynchronous will be called from another thread,
	 * possibly before this call returns.
	 * If Consume has been called previously, this call is ignored and returns EConsumeResult::Failed.
	 *
	 * @return Whether the load succeeded (this information is also passed to the ConsumeFunction).
	 */
	EConsumeResult ConsumeOrDefer(FConsumeFunction&& ConsumeSynchronous, FConsumeFunction&& ConsumeAsynchronous)
	{
		return ConsumeInternal(MoveTemp(ConsumeSynchronous), MoveTemp(ConsumeAsynchronous));
	}

private:
	enum class EState : uint8
	{
		WillNeverPreload,
		LoadSynchronous,
		NotFound,
		Loading,
		Loaded,
		Consumed,
	};


	bool TrySetPath()
	{
		for (FString& LocalPath : GetPriorityPaths())
		{
			if (IFileManager::Get().FileExists(*LocalPath))
			{
				ARPath = MoveTemp(LocalPath);
				return true;
			}
		}
		return false;
	}

	bool TrySetPath(const IPakFile& Pak)
	{
		for (FString& LocalPath : GetPriorityPaths())
		{
			if (Pak.PakContains(LocalPath))
			{
				ARPath = MoveTemp(LocalPath);
				return true;
			}
		}
		return false;
	}

	ELoadResult TryLoad()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCookedAssetRegistryPreloader::TryLoad);
		LLM_SCOPE(ELLMTag::AssetRegistry);
		checkf(!ARPath.IsEmpty(), TEXT("TryLoad must not be called until after TrySetPath has succeeded."));

		FAssetRegistryLoadOptions Options;
		const int32 ThreadReduction = 2; // This thread + main thread already has work to do 
		int32 MaxWorkers = CanLoadAsync() ? FPlatformMisc::NumberOfCoresIncludingHyperthreads() - ThreadReduction : 0;
		Options.ParallelWorkers = FMath::Clamp(MaxWorkers, 0, 16);
		bool bLoadSucceeded = FAssetRegistryState::LoadFromDisk(*ARPath, Options, Payload);
		UE_CLOG(!bLoadSucceeded, LogAssetRegistry, Warning, TEXT("Premade AssetRegistry path %s existed but failed to load."), *ARPath);
		UE_CLOG(bLoadSucceeded, LogAssetRegistry, Log, TEXT("Premade AssetRegistry loaded from '%s'"), *ARPath);
		LoadResult = bLoadSucceeded ? ELoadResult::Succeeded : ELoadResult::FailedToLoad;
		return LoadResult;
	}

	void DelayedInitialize()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCookedAssetRegistryPreloader::DelayedInitialize);
		// This function will run before any UObject (ie UAssetRegistryImpl) code can run, so we don't need to do any thread safety
		// CanLoadAsync - we have to check this after the task graph is ready
		if (!CanLoadAsync())
		{
			LoadState = EState::LoadSynchronous;
			return;
		}

		// PreloadReady is in Triggered state until the Async thread is created. It is Reset in KickPreload.
		PreloadReady = FPlatformProcess::GetSynchEventFromPool(true /* bIsManualReset */);
		PreloadReady->Trigger();

		if (TrySetPath())
		{
			KickPreload();
		}
		else
		{
			// set to NotFound, although PakMounted may set it to found later
			LoadState = EState::NotFound;

			// The PAK with the main registry isn't mounted yet
			PakMountedDelegate = FCoreDelegates::GetOnPakFileMounted2().AddLambda([this](const IPakFile& Pak)
				{
					FScopeLock Lock(&StateLock);
					if (LoadState == EState::NotFound && TrySetPath(Pak))
					{
						KickPreload();
						// Remove the callback from OnPakFileMounted2 to avoid wasting time in all future PakFile mounts
						// Do not access any of the lambda captures after the call to Remove, because deallocating the 
						// DelegateHandle also deallocates our lambda captures
						FDelegateHandle LocalPakMountedDelegate = PakMountedDelegate;
						PakMountedDelegate.Reset();
						FCoreDelegates::GetOnPakFileMounted2().Remove(LocalPakMountedDelegate);
					}
				});
		}
	}

	void KickPreload()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FCookedAssetRegistryPreloader::KickPreload);
		// Called from Within the Lock
		check(LoadState == EState::NotFound && !ARPath.IsEmpty());
		LoadState = EState::Loading;
		PreloadReady->Reset();
		Async(EAsyncExecution::TaskGraph, [this]() { TryLoadAsync(); });
	}

	void TryLoadAsync()
	{
		// This function is active only after State has been set to Loading and PreloadReady has been Reset
		// Until this function triggers PreloadReady, it has exclusive ownership of bLoadSucceeded and Payload
		// Load outside the lock so that ConsumeOrDefer does not have to wait for the Load before it can defer and exit
		ELoadResult LocalResult = TryLoad();
		// Trigger outside the lock so that a locked Consume function that is waiting on PreloadReady can wait inside the lock.
		PreloadReady->Trigger();

		FConsumeFunction LocalConsumeCallback;
		{
			FScopeLock Lock(&StateLock);
			// The consume function may have woken up after the trigger and already consumed and changed LoadState to Consumed
			if (LoadState == EState::Loading)
			{
				LoadState = EState::Loaded;
				if (ConsumeCallback)
				{
					LocalConsumeCallback = MoveTemp(ConsumeCallback);
					ConsumeCallback.Reset();
					LoadState = EState::Consumed;
				}
			}
		}

		if (LocalConsumeCallback)
		{
			// No further threads will read/write payload at this point (until destructor, which is called after all async threads are complete
			// so we can use it outside the lock
			LocalConsumeCallback(LocalResult, MoveTemp(Payload));
			Shutdown();
		}
	}

	EConsumeResult ConsumeInternal(FConsumeFunction&& ConsumeSynchronous, FConsumeFunction&& ConsumeAsynchronous)
	{
		SCOPED_BOOT_TIMING("FCookedAssetRegistryPreloader::Consume");

		FScopeLock Lock(&StateLock);
		// Report failure if constructor decided not to preload or this has already been Consumed
		if (LoadState == EState::WillNeverPreload || LoadState == EState::Consumed || ConsumeCallback)
		{
			Lock.Unlock(); // Unlock before calling external code in Consume callback
			ELoadResult LocalResult = (LoadState == EState::Consumed || ConsumeCallback) ? ELoadResult::AlreadyConsumed : ELoadResult::Inactive;
			ConsumeSynchronous(LocalResult, FAssetRegistryState());
			return EConsumeResult::Failed;
		}

		if (LoadState == EState::LoadSynchronous)
		{
			ELoadResult LocalResult = TrySetPath() ? TryLoad() : ELoadResult::NotFound;
			LoadState = EState::Consumed;
			Lock.Unlock(); // Unlock before calling external code in Consume callback
			ConsumeSynchronous(LocalResult, MoveTemp(Payload));
			Shutdown(); // Shutdown can be called outside the lock since AsyncThread doesn't exist
			return LocalResult == ELoadResult::Succeeded ? EConsumeResult::Succeeded : EConsumeResult::Failed;
		}

		// Cancel any further searching in Paks since we will no longer accept preloads starting after this point
		FCoreDelegates::GetOnPakFileMounted2().Remove(PakMountedDelegate);
		PakMountedDelegate.Reset();

		if (ConsumeAsynchronous && LoadState == EState::Loading)
		{
			// The load might have completed and the TryAsyncLoad thread is waiting to enter the lock, but we will still defer since Consume won the race
			ConsumeCallback = MoveTemp(ConsumeAsynchronous);
			return EConsumeResult::Deferred;
		}

		{
			SCOPED_BOOT_TIMING("BlockingConsume");
			// If the load is in progress, wait for it to finish (which it does outside the lock)
			PreloadReady->Wait();
		}

		// TryAsyncLoad might not yet have set state to Loaded
		check(LoadState == EState::Loaded || LoadState == EState::Loading || LoadState == EState::NotFound);
		ELoadResult LocalResult = LoadState == EState::NotFound ? ELoadResult::NotFound : LoadResult;
		LoadState = EState::Consumed;

		// No further async threads exist that will read/write payload at this point so we can use it outside the lock
		Lock.Unlock(); // Unlock before calling external code in Consume callback
		ConsumeSynchronous(LocalResult, MoveTemp(Payload));
		Shutdown(); // Shutdown can be called outside the lock since we have set state to Consumed and the Async thread will notice and exit
		return LocalResult == ELoadResult::Succeeded ? EConsumeResult::Succeeded : EConsumeResult::Failed;
	}

	/** Called when the Preloader has no further work to do, to free resources early since destruction occurs at end of process. */
	void Shutdown(bool bFromGlobalDestructor = false)
	{
		OnTaskGraphReady.Reset();
		if (PreloadReady)
		{
			// If we are exiting the process early while PreloadReady is still allocated, the event
			// system has already been torn down and there is nothing for us to free for PreloadReady.
			if (!bFromGlobalDestructor)
			{
				FPlatformProcess::ReturnSynchEventToPool(PreloadReady);
			}
			PreloadReady = nullptr;
		}
		ARPath.Reset();
		Payload.Reset();
	}

	/** simple way to trigger a callback at a specific time that TaskGraph is usable. */
	TOptional<FDelayedAutoRegisterHelper> OnTaskGraphReady;

	/** Lock that guards members on this (see notes on each member). */
	FCriticalSection StateLock;
	/** Trigger for blocking Consume to wait upon TryLoadAsync. This Trigger is only allocated when in the states NotFound, Loaded, Loading. */
	FEvent* PreloadReady = nullptr;

	/** Path discovered for the AssetRegistry; Read/Write only within the Lock. */
	FString ARPath;

	/**
	 * The ARState loaded from disk. Owned exclusively by either the first Consume or by TryAsyncLoad.
	 * If LoadState is never set to Loading, this state is read/written only by the first thread to call Consume.
	 * If LoadState is set to Loading (which happens before threading starts), the thread running TryAsyncLoad
	 * owns this payload until it triggers PayloadReady, after which ownership returns to the first thread to call Consume.
	 */
	FAssetRegistryState Payload;

	FDelegateHandle PakMountedDelegate;

	/** Callback from ConsumeOrDefer that is set so TryLoadAsync can trigger the Consume when it completes.Read / Write only within the lock. */
	FConsumeFunction ConsumeCallback;

	/** State machine state. Read/Write only within the lock (or before threading starts). */
	EState LoadState = EState::WillNeverPreload;

	/** Result of TryLoad.Thread ownership rules are the same as the rules for Payload. */
	ELoadResult LoadResult = ELoadResult::UninitializedMemberLoadResult;
}
GPreloader;

FAsyncConsumer::~FAsyncConsumer()
{
	if (Consumed)
	{
		FPlatformProcess::ReturnSynchEventToPool(Consumed);
		Consumed = nullptr;
	}
}

void FAsyncConsumer::PrepareForConsume()
{
	// Called within the lock
	check(!Consumed);
	Consumed = FPlatformProcess::GetSynchEventFromPool(true /* bIsManualReset */);
	++ReferenceCount;
};

void FAsyncConsumer::Wait(UAssetRegistryImpl& UARI, FWriteScopeLock& ScopeLock)
{
	// Called within the lock
	if (ReferenceCount == 0)
	{
		return;
	}
	++ReferenceCount;

	// Wait outside of the lock so that the AsyncThread can enter the lock to call Consume
	{
		UARI.InterfaceLock.WriteUnlock();
		ON_SCOPE_EXIT{ UARI.InterfaceLock.WriteLock(); };
		check(Consumed != nullptr);
		Consumed->Wait();
	}

	--ReferenceCount;
	if (ReferenceCount == 0)
	{
		// We're the last one to drop the refcount, so delete Consumed
		check(Consumed != nullptr);
		FPlatformProcess::ReturnSynchEventToPool(Consumed);
		Consumed = nullptr;
	}
}

void FAsyncConsumer::Consume(UAssetRegistryImpl& UARI, UE::AssetRegistry::Impl::FEventContext& EventContext, ELoadResult LoadResult, FAssetRegistryState&& ARState)
{
	// Called within the lock
	UARI.GuardedData.LoadPremadeAssetRegistry(EventContext, LoadResult, MoveTemp(ARState));
	check(ReferenceCount >= 1);
	check(Consumed != nullptr);
	Consumed->Trigger();
	--ReferenceCount;
	if (ReferenceCount == 0)
	{
		// We're the last one to drop the refcount, so delete Consumed
		FPlatformProcess::ReturnSynchEventToPool(Consumed);
		Consumed = nullptr;
	}
}

}

namespace UE::AssetRegistry
{
void FAssetRegistryImpl::ConditionalLoadPremadeAssetRegistry(UAssetRegistryImpl& UARI, Impl::FEventContext& EventContext, FWriteScopeLock& ScopeLock)
{
	AsyncConsumer.Wait(UARI, ScopeLock);
}

void FAssetRegistryImpl::ConsumeOrDeferPreloadedPremade(UAssetRegistryImpl& UARI, Impl::FEventContext& EventContext)
{
	// Called from inside WriteLock on InterfaceLock
	using namespace UE::AssetRegistry::Premade;
	if (!Premade::IsEnabled())
	{
		// if we aren't doing any preloading, then we can set the initial search is done right away.
		// Otherwise, it is set from LoadPremadeAssetRegistry
		bPreloadingComplete = true;
		return;
	}

	if (Premade::CanLoadAsync())
	{
		FPreloader::FConsumeFunction ConsumeFromAsyncThread = [this, &UARI](Premade::ELoadResult LoadResult, FAssetRegistryState&& ARState)
		{
			Impl::FEventContext EventContext;
			{
				FWriteScopeLock InterfaceScopeLock(UARI.InterfaceLock);
				AsyncConsumer.Consume(UARI, EventContext, LoadResult, MoveTemp(ARState));
			}
			UARI.Broadcast(EventContext);
		};
		auto ConsumeOnCurrentThread = [ConsumeFromAsyncThread](Premade::ELoadResult LoadResult, FAssetRegistryState&& ARState) mutable
		{
			Async(EAsyncExecution::TaskGraph, [LoadResult, ARState=MoveTemp(ARState), ConsumeFromAsyncThread=MoveTemp(ConsumeFromAsyncThread)]() mutable
			{
				ConsumeFromAsyncThread(LoadResult, MoveTemp(ARState));
			});
		};

		AsyncConsumer.PrepareForConsume();
		GPreloader.ConsumeOrDefer(MoveTemp(ConsumeOnCurrentThread), MoveTemp(ConsumeFromAsyncThread));
	}
	else
	{
		GPreloader.Consume([this, &EventContext](Premade::ELoadResult LoadResult, FAssetRegistryState&& ARState)
			{
				LoadPremadeAssetRegistry(EventContext, LoadResult, MoveTemp(ARState));
			});
	}
}
}

/** Returns the appropriate ChunkProgressReportingType for the given Asset enum */
EChunkProgressReportingType::Type GetChunkAvailabilityProgressType(EAssetAvailabilityProgressReportingType::Type ReportType)
{
	EChunkProgressReportingType::Type ChunkReportType;
	switch (ReportType)
	{
	case EAssetAvailabilityProgressReportingType::ETA:
		ChunkReportType = EChunkProgressReportingType::ETA;
		break;
	case EAssetAvailabilityProgressReportingType::PercentageComplete:
		ChunkReportType = EChunkProgressReportingType::PercentageComplete;
		break;
	default:
		ChunkReportType = EChunkProgressReportingType::PercentageComplete;
		UE_LOG(LogAssetRegistry, Error, TEXT("Unsupported assetregistry report type: %i"), (int)ReportType);
		break;
	}
	return ChunkReportType;
}

const TCHAR* GetDevelopmentAssetRegistryFilename()
{
	return TEXT("DevelopmentAssetRegistry.bin");
}

FAssetData IAssetRegistry::K2_GetAssetByObjectPath(const FSoftObjectPath& ObjectPath, bool bIncludeOnlyOnDiskAssets, bool bSkipARFilteredAssets) const
{
	return GetAssetByObjectPath(ObjectPath, bIncludeOnlyOnDiskAssets, bSkipARFilteredAssets);
}

IAssetRegistry::FLoadPackageRegistryData::FLoadPackageRegistryData(bool bInGetDependencies)
	: bGetDependencies(bInGetDependencies)
{
}

IAssetRegistry::FLoadPackageRegistryData::~FLoadPackageRegistryData() = default;


UAssetRegistry::UAssetRegistry(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

namespace UE::AssetRegistry::Impl
{

struct FInitializeContext
{
	UAssetRegistryImpl& UARI;
	FEventContext Events;
	FClassInheritanceContext InheritanceContext;
	FClassInheritanceBuffer InheritanceBuffer;
	TArray<FString> RootContentPaths;
	bool bRedirectorsNeedSubscribe = false;
	bool bUpdateDiskCacheAfterLoad = false;
	bool bNeedsSearchAllAssetsAtStartSynchronous = false;
};

}

UAssetRegistryImpl::UAssetRegistryImpl(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SCOPED_BOOT_TIMING("UAssetRegistryImpl::UAssetRegistryImpl");

	UE::AssetRegistry::Impl::FInitializeContext Context{ *this };

	{
		LLM_SCOPE(ELLMTag::AssetRegistry);
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		GetInheritanceContextWithRequiredLock(InterfaceScopeLock, Context.InheritanceContext,
			Context.InheritanceBuffer);

		GuardedData.Initialize(Context);
		InitializeEvents(Context);
	}
	Broadcast(Context.Events);
}

bool UAssetRegistryImpl::IsPathBeautificationNeeded(const FString& InAssetPath) const
{
	return InAssetPath.Contains(FPackagePath::GetExternalActorsFolderName()) || InAssetPath.Contains(FPackagePath::GetExternalObjectsFolderName());
}

namespace UE::AssetRegistry
{

FAssetRegistryImpl::FAssetRegistryImpl()
{
}

void FAssetRegistryImpl::LoadPremadeAssetRegistry(Impl::FEventContext& EventContext,
	Premade::ELoadResult LoadResult, FAssetRegistryState&& ARState)
{
	SCOPED_BOOT_TIMING("LoadPremadeAssetRegistry");
	UE_SCOPED_ENGINE_ACTIVITY("Loading premade asset registry");

	if (SerializationOptions.bSerializeAssetRegistry)
	{
		SCOPED_BOOT_TIMING("LoadPremadeAssetRegistry_Main");
		if (LoadResult == Premade::ELoadResult::Succeeded)
		{
			if (State.GetNumAssets() == 0)
			{
				State = MoveTemp(ARState);
				CachePathsFromState(EventContext, State);
			}
			else if (State.GetNumAssets() < ARState.GetNumAssets())
			{
				FAssetRegistryState ExistingState = MoveTemp(State);
				State = MoveTemp(ARState);
				CachePathsFromState(EventContext, State);
				AppendState(EventContext, ExistingState);
			}
			else
			{
				AppendState(EventContext, ARState, FAssetRegistryState::EInitializationMode::OnlyUpdateNew);
			}
			UpdatePersistentMountPoints();
			State.bCookedGlobalAssetRegistryState = true;
		}
		else
		{
			UE_CLOG(FPlatformProperties::RequiresCookedData() && (IsRunningGame() || IsRunningDedicatedServer()),
				LogAssetRegistry, Error, TEXT("Failed to load premade asset registry. LoadResult == %d."), static_cast<int32>(LoadResult));
		}
	}

	{
		SCOPED_BOOT_TIMING("LoadPremadeAssetRegistry_Plugins");
		TArray<TSharedRef<IPlugin>> ContentPlugins = IPluginManager::Get().GetEnabledPluginsWithContent();
		for (const TSharedRef<IPlugin>& ContentPlugin : ContentPlugins)
		{
			if (ContentPlugin->CanContainContent())
			{
				FArrayReader SerializedAssetData;
				FString PluginAssetRegistry = ContentPlugin->GetBaseDir() / TEXT("AssetRegistry.bin");
				if (IFileManager::Get().FileExists(*PluginAssetRegistry) && FFileHelper::LoadFileToArray(SerializedAssetData, *PluginAssetRegistry))
				{
					SerializedAssetData.Seek(0);
					FAssetRegistryState PluginState;
					PluginState.Load(SerializedAssetData);

#if ASSETREGISTRY_ENABLE_PREMADE_REGISTRY_IN_EDITOR
					/*
					 * Only update the new assets when using a premade asset registry in editor.
					 * The main state will often already include the DLC/plugin assets and is often in a development mode where the plugin state will not be.
					 * If we update the existing assets in those cases it will be causing a lost of tags and values that are needed for the editor systems.
					 */
					AppendState(EventContext, PluginState,  FAssetRegistryState::EInitializationMode::OnlyUpdateNew);
#else
					AppendState(EventContext, PluginState);
#endif
				}
			}
		}
	}

	// let Tick know that it can finalize the initial search
	bPreloadingComplete = true;
}

void FAssetRegistryImpl::Initialize(Impl::FInitializeContext& Context)
{
	const double StartupStartTime = FPlatformTime::Seconds();

	bInitialSearchStarted = false;
	bInitialSearchCompleted = true;
	GatherStatus = Impl::EGatherStatus::Active;
	PerformanceMode = Impl::EPerformanceMode::MostlyStatic;

	bSearchAllAssets = false;
#if NO_LOGGING
	bVerboseLogging = false;
#else
	bVerboseLogging = LogAssetRegistry.GetVerbosity() >= ELogVerbosity::Verbose;
#endif
	StoreGatherResultsTimeSeconds = 0.f;

	// By default update the disk cache once on asset load, to incorporate changes made in PostLoad. This only happens in editor builds
#if !WITH_EDITOR
	Context.bUpdateDiskCacheAfterLoad = false;
#else
	if (IsRunningCookCommandlet())
	{
		Context.bUpdateDiskCacheAfterLoad = false;
	}
	else
	{
		Context.bUpdateDiskCacheAfterLoad = true;
		if (GConfig)
		{
			GConfig->GetBool(TEXT("AssetRegistry"), TEXT("bUpdateDiskCacheAfterLoad"), Context.bUpdateDiskCacheAfterLoad, GEngineIni);
		}
	}
#endif

	bIsTempCachingAlwaysEnabled = ASSETREGISTRY_CACHE_ALWAYS_ENABLED;
	bIsTempCachingEnabled = bIsTempCachingAlwaysEnabled;
	TempCachedInheritanceBuffer.bDirty = true;

	SavedGeneratorClassesVersionNumber = MAX_uint64;
	SavedAllClassesVersionNumber = MAX_uint64;

	// By default do not double check mount points are still valid when gathering new assets
	bVerifyMountPointAfterGather = false;

#if WITH_EDITOR
	if (GIsEditor)
	{
		// Double check mount point is still valid because it could have been umounted
		bVerifyMountPointAfterGather = true;
	}
#endif // WITH_EDITOR

	// Collect all code generator classes (currently BlueprintCore-derived ones)
	CollectCodeGeneratorClasses();
#if WITH_ENGINE && WITH_EDITOR
	Utils::PopulateSkipClasses(SkipUncookedClasses, SkipCookedClasses);
#endif

	// Read default serialization options
	Utils::InitializeSerializationOptionsFromIni(SerializationOptions, FString());
	Utils::InitializeSerializationOptionsFromIni(DevelopmentSerializationOptions, FString(), UE::AssetRegistry::ESerializationTarget::ForDevelopment);

	bool bStartedAsyncGather = false;
	if (ShouldSearchAllAssetsAtStart())
	{
		ConstructGatherer();
		if (GlobalGatherer->IsAsyncEnabled())
		{
			SearchAllAssetsInitialAsync(Context.Events, Context.InheritanceContext);
			bStartedAsyncGather = true;
		}
		else
		{
			// For the Editor we need to take responsibility for the synchronous search; Commandlets will handle that themselves
			Context.bNeedsSearchAllAssetsAtStartSynchronous = GIsEditor && !IsRunningCommandlet();
		}
	}

	ConsumeOrDeferPreloadedPremade(Context.UARI, Context.Events);

	// Report startup time. This does not include DirectoryWatcher startup time.
	double StartupDuration = FPlatformTime::Seconds() - StartupStartTime;
	UE_LOG(LogAssetRegistry, Log, TEXT("FAssetRegistry took %0.4f seconds to start up"), StartupDuration);
	
	FTelemetryRouter::Get().ProvideTelemetry<UE::Telemetry::AssetRegistry::FStartupTelemetry>({
		StartupDuration,
		bStartedAsyncGather
	});

	// Content roots always exist; add them as paths
	FPackageName::QueryRootContentPaths(Context.RootContentPaths, false, false, true);
	for (const FString& AssetPath : Context.RootContentPaths)
	{
		AddPath(Context.Events, AssetPath);
	}

	InitRedirectors(Context.Events, Context.InheritanceContext, Context.bRedirectorsNeedSubscribe);

#if WITH_EDITOR
	// Makae sure first call to LoadCalculatedDependencies builds the Gatherer list. At that point Classes should be loaded.
	bRegisteredDependencyGathererClassesDirty = true;
#endif
}

#if WITH_EDITOR

void FAssetRegistryImpl::RebuildAssetDependencyGathererMapIfNeeded()
{
	if (!bRegisteredDependencyGathererClassesDirty)
	{
		return;
	}

	RegisteredDependencyGathererClasses.Reset();

	TArray<UObject*> Classes;
	GetObjectsOfClass(UClass::StaticClass(), Classes);

	/** Per Class dependency gatherers */
	UE::AssetDependencyGatherer::Private::FRegisteredAssetDependencyGatherer::ForEach([&](UE::AssetDependencyGatherer::Private::FRegisteredAssetDependencyGatherer* RegisteredAssetDependencyGatherer)
	{
		UClass* AssetClass = RegisteredAssetDependencyGatherer->GetAssetClass();
		for (UObject* ClassObject : Classes)
		{
			if (UClass* Class = Cast<UClass>(ClassObject); Class && Class->IsChildOf(AssetClass) && !Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
			{
				RegisteredDependencyGathererClasses.Add(FTopLevelAssetPath(Class), RegisteredAssetDependencyGatherer);
			}
		}
	});

	bRegisteredDependencyGathererClassesDirty = false;
}

#endif

}

void UAssetRegistryImpl::InitializeEvents(UE::AssetRegistry::Impl::FInitializeContext& Context)
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		check(UE::AssetRegistry::Private::IAssetRegistrySingleton::Singleton == nullptr && IAssetRegistryInterface::Default == nullptr);
		UE::AssetRegistry::Private::IAssetRegistrySingleton::Singleton = this;
		IAssetRegistryInterface::Default = &GAssetRegistryInterface;
	}

	if (Context.bRedirectorsNeedSubscribe)
	{
		TDelegate<bool(const FString&, FString&)> PackageResolveDelegate;
		PackageResolveDelegate.BindUObject(this, &UAssetRegistryImpl::OnResolveRedirect);
		FCoreDelegates::PackageNameResolvers.Add(PackageResolveDelegate);
	}

#if WITH_EDITOR
	// In-game doesn't listen for directory changes
	if (GIsEditor)
	{
		FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
		IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get();

		if (DirectoryWatcher)
		{
			// The vast majority of directories we are watching are below the Plugin directories. The memory cost per watch
			// is sufficiently high to want to avoid setting up many granular watches when we can also setup two coarse ones.

			const FString ProjectPluginDir = FPaths::CreateStandardFilename(FPaths::ProjectPluginsDir());
			if (IPlatformFile::GetPlatformPhysical().DirectoryExists(*ProjectPluginDir))
			{
				DirectoryWatchRoots.Add(ProjectPluginDir);
			}
			const FString EnginePluginDir = FPaths::CreateStandardFilename(FPaths::EnginePluginsDir());
			if (IPlatformFile::GetPlatformPhysical().DirectoryExists(*EnginePluginDir))
			{
				DirectoryWatchRoots.Add(EnginePluginDir);
			}

			for (FString& WatchRoot : DirectoryWatchRoots)
			{
				FDelegateHandle NewHandle;
				DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(
					WatchRoot,
					IDirectoryWatcher::FDirectoryChanged::CreateUObject(this, &UAssetRegistryImpl::OnDirectoryChanged),
					NewHandle,
					IDirectoryWatcher::WatchOptions::IncludeDirectoryChanges);

				OnDirectoryChangedDelegateHandles.Add(WatchRoot, NewHandle);
			}

			FString ContentFolder;
			for (TArray<FString>::TConstIterator RootPathIt(Context.RootContentPaths); RootPathIt; ++RootPathIt)
			{
				const FString& RootPath = *RootPathIt;
				ContentFolder = FPaths::CreateStandardFilename(FPackageName::LongPackageNameToFilename(RootPath));
				if (IsDirAlreadyWatchedByRootWatchers(ContentFolder))
				{
					continue;
				}

				// A missing directory here could be due to a plugin that specifies it contains content, yet has no content yet.
				// PluginManager mounts these folders anyway which results in them being returned from QueryRootContentPaths.
				// Make sure the directory exits on disk so that the OS level DirectoryWatcher can be used to monitor it.
				IPlatformFile::GetPlatformPhysical().CreateDirectoryTree(*ContentFolder);
				FDelegateHandle NewHandle;
				DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(
						ContentFolder,
						IDirectoryWatcher::FDirectoryChanged::CreateUObject(this, &UAssetRegistryImpl::OnDirectoryChanged),
						NewHandle,
						IDirectoryWatcher::WatchOptions::IncludeDirectoryChanges);

				OnDirectoryChangedDelegateHandles.Add(RootPath, NewHandle);
			}
		}
	}

	bUpdateDiskCacheAfterLoad = Context.bUpdateDiskCacheAfterLoad;
	if (bUpdateDiskCacheAfterLoad)
	{
		FCoreUObjectDelegates::OnAssetLoaded.AddUObject(this, &UAssetRegistryImpl::OnAssetLoaded);
	}

	if (bAddMetaDataTagsToOnGetExtraObjectTags)
	{
		UObject::FAssetRegistryTag::OnGetExtraObjectTagsWithContext.AddUObject(this, &UAssetRegistryImpl::OnGetExtraObjectTags);
	}
	if (Context.bNeedsSearchAllAssetsAtStartSynchronous)
	{
		FCoreDelegates::OnFEngineLoopInitComplete.AddUObject(this, &UAssetRegistryImpl::OnFEngineLoopInitCompleteSearchAllAssets);
	}

	UE::AssetDependencyGatherer::Private::FRegisteredAssetDependencyGatherer::OnAssetDependencyGathererRegistered.AddUObject(this, &UAssetRegistryImpl::OnAssetDependencyGathererRegistered);
#endif // WITH_EDITOR

	FCoreDelegates::OnEnginePreExit.AddUObject(this, &UAssetRegistryImpl::OnEnginePreExit);

	// Listen for new content paths being added or removed at runtime.  These are usually plugin-specific asset paths that
	// will be loaded a bit later on.
	FPackageName::OnContentPathMounted().AddUObject(this, &UAssetRegistryImpl::OnContentPathMounted);
	FPackageName::OnContentPathDismounted().AddUObject(this, &UAssetRegistryImpl::OnContentPathDismounted);

	// If we were called before engine has fully initialized, refresh classes on initialize. If not this won't do anything as it already happened
	FCoreDelegates::OnPostEngineInit.AddUObject(this, &UAssetRegistryImpl::OnRefreshNativeClasses);

	IPluginManager& PluginManager = IPluginManager::Get();
	ELoadingPhase::Type LoadingPhase = PluginManager.GetLastCompletedLoadingPhase();
	if (LoadingPhase == ELoadingPhase::None || LoadingPhase < ELoadingPhase::PostEngineInit)
	{
		PluginManager.OnLoadingPhaseComplete().AddUObject(this, &UAssetRegistryImpl::OnPluginLoadingPhaseComplete);
	}
}

UAssetRegistryImpl::UAssetRegistryImpl(FVTableHelper& Helper)
	: Super(Helper)
{
}

bool UAssetRegistryImpl::OnResolveRedirect(const FString& InPackageName, FString& OutPackageName)
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	return GuardedData.ResolveRedirect(InPackageName, OutPackageName);
}

namespace UE::AssetRegistry
{

bool FAssetRegistryImpl::ResolveRedirect(const FString& InPackageName, FString& OutPackageName) const
{
	int32 DotIndex = InPackageName.Find(TEXT("."), ESearchCase::CaseSensitive);

	FString ContainerPackageName; 
	const FString* PackageNamePtr = &InPackageName; // don't return this
	if (DotIndex != INDEX_NONE)
	{
		ContainerPackageName = InPackageName.Left(DotIndex);
		PackageNamePtr = &ContainerPackageName;
	}
	const FString& PackageName = *PackageNamePtr;

	for (const FAssetRegistryPackageRedirect& PackageRedirect : PackageRedirects)
	{
		if (PackageName.Compare(PackageRedirect.SourcePackageName) == 0)
		{
			OutPackageName = InPackageName.Replace(*PackageRedirect.SourcePackageName, *PackageRedirect.DestPackageName);
			return true;
		}
	}
	return false;
}

void FAssetRegistryImpl::InitRedirectors(Impl::FEventContext& EventContext,
	Impl::FClassInheritanceContext& InheritanceContext, bool& bOutRedirectorsNeedSubscribe)
{
	bOutRedirectorsNeedSubscribe = false;

	// plugins can't initialize redirectors in the editor, it will mess up the saving of content.
	if ( GIsEditor )
	{
		return;
	}

	TArray<TSharedRef<IPlugin>> EnabledPlugins = IPluginManager::Get().GetEnabledPlugins();
	for (const TSharedRef<IPlugin>& Plugin : EnabledPlugins)
	{
		FString PluginConfigFilename = FString::Printf(TEXT("%s%s/%s.ini"), *FPaths::GeneratedConfigDir(), ANSI_TO_TCHAR(FPlatformProperties::PlatformName()), *Plugin->GetName() );
		
		bool bShouldRemap = false;
		
		if ( !GConfig->GetBool(TEXT("PluginSettings"), TEXT("RemapPluginContentToGame"), bShouldRemap, PluginConfigFilename) )
		{
			continue;
		}

		if (!bShouldRemap)
		{
			continue;
		}

		// if we are -game in editor build we might need to initialize the asset registry manually for this plugin
		if (!FPlatformProperties::RequiresCookedData() && IsRunningGame())
		{
			TArray<FString> PathsToSearch;
			
			FString RootPackageName = FString::Printf(TEXT("/%s/"), *Plugin->GetName());
			PathsToSearch.Add(RootPackageName);

			Impl::FScanPathContext Context(EventContext, InheritanceContext, PathsToSearch, TArray<FString>());
			ScanPathsSynchronous(Context);
		}

		FName PluginPackageName = FName(*FString::Printf(TEXT("/%s/"), *Plugin->GetName()));
		EnumerateAssetsByPathNoTags(PluginPackageName,
			[&Plugin, this](const FAssetData& PartialAssetData)
			{
				FString NewPackageNameString = PartialAssetData.PackageName.ToString();
				FString RootPackageName = FString::Printf(TEXT("/%s/"), *Plugin->GetName());
				FString OriginalPackageNameString = NewPackageNameString.Replace(*RootPackageName, TEXT("/Game/"));

				PackageRedirects.Add(FAssetRegistryPackageRedirect(OriginalPackageNameString, NewPackageNameString));
				return true;
			}, true, false);

		bOutRedirectorsNeedSubscribe = true;
	}
}

}

void UAssetRegistryImpl::OnPluginLoadingPhaseComplete(ELoadingPhase::Type LoadingPhase, bool bPhaseSuccessful)
{
	if (LoadingPhase != ELoadingPhase::PostEngineInit)
	{
		return;
	}
	{
		LLM_SCOPE(ELLMTag::AssetRegistry);
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		GuardedData.OnPostEngineInit(bPhaseSuccessful);
	}

	IPluginManager::Get().OnLoadingPhaseComplete().RemoveAll(this);
}

namespace UE::AssetRegistry
{

void FAssetRegistryImpl::OnPostEngineInit(bool bPhaseSuccessful)
{
	// If we have constructed the GlobalGatherer then we need to readscriptpackages,
	// otherwise we will read them when constructing the gatherer.
	if (GlobalGatherer.IsValid())
	{
		ReadScriptPackages();
	}

	// Reparse the skip classes the next time ShouldSkipAsset is called, since available classes
	// for the search over all classes may have changed
#if WITH_ENGINE && WITH_EDITOR
	// If we ever need to update the Filtering list outside of the game thread, we will need to defer the update 
	// of the Filtering namespace to the tick function; UE::AssetRegistry::Filtering can only be used in game thread
	check(IsInGameThread());
	Utils::PopulateSkipClasses(SkipUncookedClasses, SkipCookedClasses);
	UE::AssetRegistry::FFiltering::SetSkipClasses(SkipUncookedClasses, SkipCookedClasses);
#endif
}

void FAssetRegistryImpl::ReadScriptPackages()
{
	GlobalGatherer->SetInitialPluginsLoaded();
	if (GlobalGatherer->IsGatheringDependencies())
	{
		// Now that all scripts have been loaded, we need to create AssetPackageDatas for every script
		// This is also done whenever scripts are referenced in our gather of existing packages,
		// but we need to complete it for all scripts that were referenced but not yet loaded for packages
		// that we already gathered
		for (TObjectIterator<UPackage> It; It; ++It)
		{
			UPackage* Package = *It;
			if (Package)
			{
				if (Package && FPackageName::IsScriptPackage(Package->GetName()))
				{
					FAssetPackageData* ScriptPackageData = State.CreateOrGetAssetPackageData(Package->GetFName());
#if WITH_EDITORONLY_DATA
					// Get the hash off the script package, it is updated when script is changed so we need to refresh it every run
					ScriptPackageData->SetPackageSavedHash(Package->GetSavedHash());
#endif
				}
			}
		}
	}
}

}

void UAssetRegistryImpl::InitializeSerializationOptions(FAssetRegistrySerializationOptions& Options, const FString& PlatformIniName, UE::AssetRegistry::ESerializationTarget Target) const
{
	if (PlatformIniName.IsEmpty())
	{
		FReadScopeLock InterfaceScopeLock(InterfaceLock);
		// Use options we already loaded, the first pass for this happens at object creation time so this is always valid when queried externally
		GuardedData.CopySerializationOptions(Options, Target);
	}
	else
	{
		UE::AssetRegistry::Utils::InitializeSerializationOptionsFromIni(Options, PlatformIniName, Target);
	}
}

namespace UE::AssetRegistry
{

void FAssetRegistryImpl::CopySerializationOptions(FAssetRegistrySerializationOptions& OutOptions, ESerializationTarget Target) const
{
	if (Target == UE::AssetRegistry::ESerializationTarget::ForGame)
	{
		OutOptions = SerializationOptions;
	}
	else
	{
		OutOptions = DevelopmentSerializationOptions;
	}
}

namespace Utils
{

static TSet<FName> MakeNameSet(const TArray<FString>& Strings)
{
	TSet<FName> Out;
	Out.Reserve(Strings.Num());
	for (const FString& String : Strings)
	{
		Out.Add(FName(*String));
	}

	return Out;
}

void InitializeSerializationOptionsFromIni(FAssetRegistrySerializationOptions& Options, const FString& PlatformIniName, UE::AssetRegistry::ESerializationTarget Target)
{
	// Use passed in platform, or current platform if empty
	FConfigFile LocalEngineIni;
	FConfigFile* EngineIni = FConfigCacheIni::FindOrLoadPlatformConfig(LocalEngineIni, TEXT("Engine"), (!PlatformIniName.IsEmpty() ? *PlatformIniName : ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName())));

	Options = FAssetRegistrySerializationOptions(Target);
	// For DevelopmentAssetRegistry, all non-tag options are overridden in the constructor
	const bool bForDevelopment = Target == UE::AssetRegistry::ESerializationTarget::ForDevelopment;
	if (!bForDevelopment)
	{
		EngineIni->GetBool(TEXT("AssetRegistry"), TEXT("bSerializeAssetRegistry"), Options.bSerializeAssetRegistry);
		EngineIni->GetBool(TEXT("AssetRegistry"), TEXT("bSerializeDependencies"), Options.bSerializeDependencies);
		EngineIni->GetBool(TEXT("AssetRegistry"), TEXT("bSerializeNameDependencies"), Options.bSerializeSearchableNameDependencies);
		EngineIni->GetBool(TEXT("AssetRegistry"), TEXT("bSerializeManageDependencies"), Options.bSerializeManageDependencies);
		EngineIni->GetBool(TEXT("AssetRegistry"), TEXT("bSerializePackageData"), Options.bSerializePackageData);
		EngineIni->GetBool(TEXT("AssetRegistry"), TEXT("bFilterAssetDataWithNoTags"), Options.bFilterAssetDataWithNoTags);
		EngineIni->GetBool(TEXT("AssetRegistry"), TEXT("bFilterDependenciesWithNoTags"), Options.bFilterDependenciesWithNoTags);
		EngineIni->GetBool(TEXT("AssetRegistry"), TEXT("bFilterSearchableNames"), Options.bFilterSearchableNames);
	}

	EngineIni->GetBool(TEXT("AssetRegistry"), TEXT("bUseAssetRegistryTagsWhitelistInsteadOfBlacklist"), Options.bUseAssetRegistryTagsAllowListInsteadOfDenyList);
	TArray<FString> FilterListItems;
	if (Options.bUseAssetRegistryTagsAllowListInsteadOfDenyList)
	{
		EngineIni->GetArray(TEXT("AssetRegistry"), TEXT("CookedTagsWhitelist"), FilterListItems);
	}
	else
	{
		EngineIni->GetArray(TEXT("AssetRegistry"), TEXT("CookedTagsBlacklist"), FilterListItems);
	}

	{
		// this only needs to be done once, and only on builds using USE_COMPACT_ASSET_REGISTRY
		TArray<FString> AsFName;
		EngineIni->GetArray(TEXT("AssetRegistry"), TEXT("CookedTagsAsFName"), AsFName);
		Options.CookTagsAsName = MakeNameSet(AsFName);

		TArray<FString> AsPathName;
		EngineIni->GetArray(TEXT("AssetRegistry"), TEXT("CookedTagsAsPathName"), AsPathName);
		Options.CookTagsAsPath = MakeNameSet(AsPathName);
	}

	// Takes on the pattern "(Class=SomeClass,Tag=SomeTag)"
	// Optional key KeepInDevOnly for tweaking a DevelopmentAssetRegistry (additive if allow list, subtractive if deny list)
	for (const FString& FilterEntry : FilterListItems)
	{
		FString TrimmedEntry = FilterEntry;
		TrimmedEntry.TrimStartAndEndInline();
		if (TrimmedEntry.Left(1) == TEXT("("))
		{
			TrimmedEntry.RightChopInline(1, EAllowShrinking::No);
		}
		if (TrimmedEntry.Right(1) == TEXT(")"))
		{
			TrimmedEntry.LeftChopInline(1, EAllowShrinking::No);
		}

		TArray<FString> Tokens;
		TrimmedEntry.ParseIntoArray(Tokens, TEXT(","));
		FString ClassName;
		FString TagName;
		bool bKeepInDevOnly = false;

		for (const FString& Token : Tokens)
		{
			FString KeyString;
			FString ValueString;
			if (Token.Split(TEXT("="), &KeyString, &ValueString))
			{
				KeyString.TrimStartAndEndInline();
				ValueString.TrimStartAndEndInline();
				if (KeyString.Equals(TEXT("Class"), ESearchCase::IgnoreCase))
				{
					ClassName = ValueString;
				}
				else if (KeyString.Equals(TEXT("Tag"), ESearchCase::IgnoreCase))
				{
					TagName = ValueString;
				}
			}
			else
			{
				KeyString = Token.TrimStartAndEnd();
				if (KeyString.Equals(TEXT("KeepInDevOnly"), ESearchCase::IgnoreCase))
				{
					bKeepInDevOnly = true;
				}
			}
		}

		const bool bPassesDevOnlyRule = !bKeepInDevOnly || Options.bUseAssetRegistryTagsAllowListInsteadOfDenyList == bForDevelopment;
		if (!ClassName.IsEmpty() && !TagName.IsEmpty() && bPassesDevOnlyRule)
		{
			FName TagFName = FName(*TagName);

			// Include subclasses if the class is in memory at this time (native classes only)
			UClass* FilterlistClass = Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, *ClassName));
			if (FilterlistClass)
			{
				Options.CookFilterlistTagsByClass.FindOrAdd(FilterlistClass->GetClassPathName()).Add(TagFName);

				TArray<UClass*> DerivedClasses;
				GetDerivedClasses(FilterlistClass, DerivedClasses);
				for (UClass* DerivedClass : DerivedClasses)
				{
					Options.CookFilterlistTagsByClass.FindOrAdd(DerivedClass->GetClassPathName()).Add(TagFName);
				}
			}
			else
			{
				FTopLevelAssetPath ClassPathName;
				if (ClassName == TEXTVIEW("*"))
				{
					ClassPathName = UE::AssetRegistry::WildcardPathName;
				}
				else if (FPackageName::IsShortPackageName(ClassName))
				{
					ClassPathName = UClass::TryConvertShortTypeNameToPathName<UClass>(ClassName, ELogVerbosity::Warning, TEXT("Parsing [AssetRegistry] CookedTagsWhitelist or CookedTagsBlacklist"));
					UE_CLOG(ClassPathName.IsNull(), LogAssetRegistry, Warning, TEXT("Failed to convert short class name \"%s\" when parsing ini [AssetRegistry] CookedTagsWhitelist or CookedTagsBlacklist"), *ClassName);
				}
				else
				{
					ClassPathName = FTopLevelAssetPath(ClassName);
				}
				// Class is not in memory yet. Just add an explicit filter.
				// Automatically adding subclasses of non-native classes is not supported.
				// In these cases, using Class=* is usually sufficient				
				Options.CookFilterlistTagsByClass.FindOrAdd(ClassPathName).Add(TagFName);
			}
		}
	}
}

}

uint64 FAssetRegistryImpl::GetCurrentGeneratorClassesVersionNumber()
{
	// Generator classes can only be native, so we can use the less-frequently-updated
	// RegisteredNativeClassesVersionNumber. In monolithic configurations, this will only be
	// updated at program start and when enabling DLC modules.
	return GetRegisteredNativeClassesVersionNumber();
}

uint64 FAssetRegistryImpl::GetCurrentAllClassesVersionNumber()
{
	return GetRegisteredClassesVersionNumber();
}

void FAssetRegistryImpl::CollectCodeGeneratorClasses()
{
	LLM_SCOPE(ELLMTag::AssetRegistry); // Tagged here instead of a higher level because it can occur even when reading
	// Only refresh the list if our registered classes have changed
	uint64 CurrentGeneratorClassesVersionNumber = GetCurrentGeneratorClassesVersionNumber();
	if (SavedGeneratorClassesVersionNumber == CurrentGeneratorClassesVersionNumber)
	{
		return;
	}
	SavedGeneratorClassesVersionNumber = CurrentGeneratorClassesVersionNumber;

	TArray<UClass*> BlueprintCoreDerivedClasses;
	FTopLevelAssetPath BlueprintCorePathName(TEXT("/Script/Engine"), TEXT("BlueprintCore"));
	UClass* BlueprintCoreClass = nullptr;

	{
		// FindObject and GetDerivedClasses are not legal during GarbageCollection. Note that we might be called from
		// an async thread, in which case we might lock this thread until GC completes. This could cause a deadlock if
		// there aren't enough async threads. But CollectCodeGeneratorClasses is not called on runtime or cooked
		// editor because they are monolithic, and so this lock should only occur on uncooked editor platforms, which
		// should have a high enough number of threads to not block garbage collection.
		FGCScopeGuard NoGCScopeGuard;

		// Work around the fact we don't reference Engine module directly
		BlueprintCoreClass = FindObject<UClass>(BlueprintCorePathName);
		if (!BlueprintCoreClass)
		{
			return;
		}
		GetDerivedClasses(BlueprintCoreClass, BlueprintCoreDerivedClasses);
	}

	ClassGeneratorNames.Add(BlueprintCoreClass->GetClassPathName());
	for (UClass* BPCoreClass : BlueprintCoreDerivedClasses)
	{
		bool bAlreadyRecorded;
		FTopLevelAssetPath BPCoreClassName = BPCoreClass->GetClassPathName();
		ClassGeneratorNames.Add(BPCoreClassName, &bAlreadyRecorded);
		if (bAlreadyRecorded)
		{
			continue;
		}

		// For new generator classes, add all instances of them to CachedBPInheritanceMap. This is usually done
		// when AddAssetData is called for those instances, but when we add a new generator class we have to recheck all
		// instances of the class since they would have failed to detect they were Blueprint classes before.
		// This can happen if blueprints in plugin B are scanned before their blueprint class from plugin A is scanned.
		for (const FAssetData* AssetData : State.GetAssetsByClassPathName(BPCoreClassName))
		{
			const FString GeneratedClass = AssetData->GetTagValueRef<FString>(FBlueprintTags::GeneratedClassPath);
			const FString ParentClass = AssetData->GetTagValueRef<FString>(FBlueprintTags::ParentClassPath);
			if (!GeneratedClass.IsEmpty() && !ParentClass.IsEmpty())
			{
				const FTopLevelAssetPath GeneratedClassPathName(FPackageName::ExportTextPathToObjectPath(GeneratedClass));
				const FTopLevelAssetPath ParentClassPathName(FPackageName::ExportTextPathToObjectPath(ParentClass));

				if (!CachedBPInheritanceMap.Contains(GeneratedClassPathName))
				{
					AddCachedBPClassParent(GeneratedClassPathName, ParentClassPathName);

					// Invalidate caching because CachedBPInheritanceMap got modified
					TempCachedInheritanceBuffer.bDirty = true;
				}
			}
		}
	}
}

}

void UAssetRegistryImpl::OnRefreshNativeClasses()
{
	LLM_SCOPE(ELLMTag::AssetRegistry);
	FWriteScopeLock InterfaceScopeLock(InterfaceLock);
	GuardedData.RefreshNativeClasses();
}

namespace UE::AssetRegistry
{

void FAssetRegistryImpl::RefreshNativeClasses()
{
	// Native classes have changed so reinitialize code generator, class inheritance maps,
	// and serialization options
	CollectCodeGeneratorClasses();
	TempCachedInheritanceBuffer.bDirty = true;

	// Read default serialization options
	Utils::InitializeSerializationOptionsFromIni(SerializationOptions, FString());
	Utils::InitializeSerializationOptionsFromIni(DevelopmentSerializationOptions, FString(), UE::AssetRegistry::ESerializationTarget::ForDevelopment);
}

}

#if WITH_EDITOR
void UAssetRegistryImpl::OnFEngineLoopInitCompleteSearchAllAssets()
{
	SearchAllAssets(true);
}

void UAssetRegistryImpl::OnAssetDependencyGathererRegistered()
{
	LLM_SCOPE(ELLMTag::AssetRegistry);
	FWriteScopeLock InterfaceScopeLock(InterfaceLock);
	GuardedData.OnAssetDependencyGathererRegistered();
}
#endif

void UAssetRegistryImpl::OnEnginePreExit()
{
	LLM_SCOPE(ELLMTag::AssetRegistry);
	FWriteScopeLock InterfaceScopeLock(InterfaceLock);
	GuardedData.OnEnginePreExit();
}

void UAssetRegistryImpl::FinishDestroy()
{
	LLM_SCOPE(ELLMTag::AssetRegistry);
	{
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);

		// Stop listening for content mount point events
		FPackageName::OnContentPathMounted().RemoveAll(this);
		FPackageName::OnContentPathDismounted().RemoveAll(this);
		FCoreDelegates::OnPostEngineInit.RemoveAll(this);
		FCoreDelegates::OnEnginePreExit.RemoveAll(this);
		IPluginManager::Get().OnLoadingPhaseComplete().RemoveAll(this);

#if WITH_EDITOR
		if (GIsEditor)
		{
			// If the directory module is still loaded, unregister any delegates
			if (FModuleManager::Get().IsModuleLoaded("DirectoryWatcher"))
			{
				FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::GetModuleChecked<FDirectoryWatcherModule>("DirectoryWatcher");
				IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get();

				if (DirectoryWatcher)
				{
					TArray<FString> RootContentPaths;
					FPackageName::QueryRootContentPaths(RootContentPaths);
					for (TArray<FString>::TConstIterator RootPathIt(RootContentPaths); RootPathIt; ++RootPathIt)
					{
						const FString& RootPath = *RootPathIt;
						const FString ContentFolder = FPaths::CreateStandardFilename(FPackageName::LongPackageNameToFilename(RootPath));
						if (!IsDirAlreadyWatchedByRootWatchers(ContentFolder))
						{
							DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(ContentFolder, OnDirectoryChangedDelegateHandles.FindRef(RootPath));
						}
					}

					for (TArray<FString>::TConstIterator RootPathIt(DirectoryWatchRoots); RootPathIt; ++RootPathIt)
					{
						const FString& RootPath = *RootPathIt;
						DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(RootPath, OnDirectoryChangedDelegateHandles.FindRef(RootPath));
					}
					DirectoryWatchRoots.Empty();
				}
			}
		}

		if (bUpdateDiskCacheAfterLoad)
		{
			FCoreUObjectDelegates::OnAssetLoaded.RemoveAll(this);
		}

		if (bAddMetaDataTagsToOnGetExtraObjectTags)
		{
			UObject::FAssetRegistryTag::OnGetExtraObjectTagsWithContext.RemoveAll(this);
		}
		FCoreDelegates::OnFEngineLoopInitComplete.RemoveAll(this);

		UE::AssetDependencyGatherer::Private::FRegisteredAssetDependencyGatherer::OnAssetDependencyGathererRegistered.RemoveAll(this);
#endif // WITH_EDITOR

		if (HasAnyFlags(RF_ClassDefaultObject))
		{
			check(UE::AssetRegistry::Private::IAssetRegistrySingleton::Singleton == this && IAssetRegistryInterface::Default == &GAssetRegistryInterface);
			UE::AssetRegistry::Private::IAssetRegistrySingleton::Singleton = nullptr;
			IAssetRegistryInterface::Default = nullptr;
		}

		// Clear all listeners
		PathAddedEvent.Clear();
		PathRemovedEvent.Clear();
		AssetAddedEvent.Clear();
		AssetRemovedEvent.Clear();
		AssetRenamedEvent.Clear();
		AssetUpdatedEvent.Clear();
		AssetUpdatedOnDiskEvent.Clear();
		for (FAssetsEvent& Event : BatchedAssetEvents)
		{
			Event.Clear();
		}
		InMemoryAssetCreatedEvent.Clear();
		InMemoryAssetDeletedEvent.Clear();
		FileLoadedEvent.Clear();
		FileLoadProgressUpdatedEvent.Clear();
	}

	Super::FinishDestroy();
}

UAssetRegistryImpl::~UAssetRegistryImpl()
{
}

UAssetRegistryImpl& UAssetRegistryImpl::Get()
{
	check(UE::AssetRegistry::Private::IAssetRegistrySingleton::Singleton);
	return static_cast<UAssetRegistryImpl&>(*UE::AssetRegistry::Private::IAssetRegistrySingleton::Singleton);
}

namespace UE::AssetRegistry
{

void FAssetRegistryImpl::OnEnginePreExit()
{
	// Shut down the GlobalGatherer's gather threads, before we start tearing down the engine
	GlobalGatherer.Reset();
}

void FAssetRegistryImpl::ConstructGatherer()
{
	if (GlobalGatherer.IsValid())
	{
		return;
	}

	TArray<FString> PathsDenyList;
	TArray<FString> ContentSubPathsDenyList;
	if (FConfigFile* EngineIni = GConfig->FindConfigFile(GEngineIni))
	{
		EngineIni->GetArray(TEXT("AssetRegistry"), TEXT("BlacklistPackagePathScanFilters"), PathsDenyList);
		EngineIni->GetArray(TEXT("AssetRegistry"), TEXT("BlacklistContentSubPathScanFilters"), ContentSubPathsDenyList);
	}

	bool bAsyncGatherEnabled = !IsRunningGame();
	GlobalGatherer = MakeUnique<FAssetDataGatherer>(PathsDenyList, ContentSubPathsDenyList, bAsyncGatherEnabled);

	// Read script packages if all initial plugins have been loaded, otherwise do nothing; we wait for the callback.
	ELoadingPhase::Type LoadingPhase = IPluginManager::Get().GetLastCompletedLoadingPhase();
	if (LoadingPhase != ELoadingPhase::None && LoadingPhase >= ELoadingPhase::PostEngineInit)
	{
		ReadScriptPackages();
	}
}

void FAssetRegistryImpl::SearchAllAssetsInitialAsync(Impl::FEventContext& EventContext,
	Impl::FClassInheritanceContext& InheritanceContext)
{
	InitialSearchStartTime = FPlatformTime::Seconds();
	bInitialSearchStarted = true;
	bInitialSearchCompleted = false;
	SetPerformanceMode(Impl::EPerformanceMode::BulkLoading);
	SearchAllAssets(EventContext, InheritanceContext, false /* bSynchronousSearch */);
}

void FAssetRegistryImpl::SetPerformanceMode(Impl::EPerformanceMode NewMode)
{
	if (PerformanceMode != NewMode)
	{
		const bool bWereDependenciesSorted = ShouldSortDependencies();
		const bool bWereReferencersSorted = ShouldSortReferencers();

		PerformanceMode = NewMode;

		const bool bShouldSortDependencies = ShouldSortDependencies();
		const bool bShouldSortReferencers = ShouldSortReferencers();

		if ((bWereDependenciesSorted != bShouldSortDependencies) || (bWereReferencersSorted != bShouldSortReferencers))
		{
			State.SetDependencyNodeSorting(bShouldSortDependencies, bShouldSortReferencers);
		}		
	}
}

bool FAssetRegistryImpl::ShouldSortDependencies() const
{
	// Always sort in static, sometimes sort during loading
	return (PerformanceMode == Impl::MostlyStatic || (PerformanceMode == Impl::BulkLoading && !Impl::bDeferDependencySort));
}

bool FAssetRegistryImpl::ShouldSortReferencers() const
{
	// Always sort in static, sometimes sort during loading
	return (PerformanceMode == Impl::MostlyStatic || (PerformanceMode == Impl::BulkLoading && !Impl::bDeferReferencerSort));
}

}

void UAssetRegistryImpl::SearchAllAssets(bool bSynchronousSearch)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UAssetRegistryImpl::SearchAllAssets");
	using namespace UE::AssetRegistry::Impl;

	if (bSynchronousSearch)
	{
		// Ensure any ongoing async scan finishes fully first
		WaitForCompletion();
	}

	FEventContext EventContext;
	{
		LLM_SCOPE(ELLMTag::AssetRegistry);
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		FClassInheritanceContext InheritanceContext;
		FClassInheritanceBuffer InheritanceBuffer;
		GetInheritanceContextWithRequiredLock(InterfaceScopeLock, InheritanceContext, InheritanceBuffer);
		if (bSynchronousSearch)
		{
			// make sure any outstanding async preload is complete
			GuardedData.ConditionalLoadPremadeAssetRegistry(*this, EventContext, InterfaceScopeLock);
		}
		GuardedData.SearchAllAssets(EventContext, InheritanceContext, bSynchronousSearch);
	}
#if WITH_EDITOR
	if (bSynchronousSearch)
	{
		ProcessLoadedAssetsToUpdateCache(EventContext, -1., EGatherStatus::Complete);
	}
#endif
	Broadcast(EventContext);
}

bool UAssetRegistryImpl::IsSearchAllAssets() const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	return GuardedData.IsSearchAllAssets();
}

bool UAssetRegistryImpl::IsSearchAsync() const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	return GuardedData.IsInitialSearchStarted();
}

namespace UE::AssetRegistry
{

void FAssetRegistryImpl::SearchAllAssets(Impl::FEventContext& EventContext,
	Impl::FClassInheritanceContext& InheritanceContext, bool bSynchronousSearch)
{
	TRACE_BEGIN_REGION(TEXT("Asset Registry Scan"));
	ConstructGatherer();
	FAssetDataGatherer& Gatherer = *GlobalGatherer;
	if (!Gatherer.IsAsyncEnabled())
	{
		UE_CLOG(!bSynchronousSearch, LogAssetRegistry, Warning, TEXT("SearchAllAssets: Gatherer is in synchronous mode; forcing bSynchronousSearch=true."));
		bSynchronousSearch = true;
	}

	Gatherer.ActivateMonolithicCache();

	// Add all existing mountpoints to the GlobalGatherer
	// This will include Engine content, Game content, but also may include mounted content directories for one or more plugins.
	TArray<FString> PackagePathsToSearch;
	FPackageName::QueryRootContentPaths(PackagePathsToSearch);
	for (const FString& PackagePath : PackagePathsToSearch)
	{
		const FString& MountLocalPath = FPackageName::LongPackageNameToFilename(PackagePath);
		Gatherer.AddMountPoint(MountLocalPath, PackagePath);
		Gatherer.SetIsOnAllowList(MountLocalPath, true);
	}
	bSearchAllAssets = true; // Mark that future mounts and directories should be scanned

	if (bSynchronousSearch)
	{
		Gatherer.WaitForIdle();
		bool bUnusedInterrupted;
		Impl::EGatherStatus UnusedStatus = TickGatherer(EventContext, InheritanceContext, -1., bUnusedInterrupted);
#if WITH_EDITOR
		if (!bInitialSearchStarted)
		{
			// We have a contract that we call UpdateRedirectCollector after the call to SearchAllAssets completes.
			// If we ran the initial async call asynchronously it is done in TickGatherer; for later synchronous calls it is done here
			UpdateRedirectCollector();
		}
#endif
	}
	else
	{
		Gatherer.StartAsync();
	}
}

}

void UAssetRegistryImpl::WaitForCompletion()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UAssetRegistryImpl::WaitForCompletion);

	using namespace UE::AssetRegistry::Impl;

	bool bInitialSearchStarted = false;
	bool bInitialSearchCompleted = false;
	// Try taking over the gather thread for a short time in case it is mostly done.
	// But if it has more than a small amount of work to do, let the gather thread do that work
	// while we consume the results in parallel.
	{
		LLM_SCOPE(ELLMTag::AssetRegistry);
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		FClassInheritanceContext InheritanceContext;
		FClassInheritanceBuffer InheritanceBuffer;
		GetInheritanceContextWithRequiredLock(InterfaceScopeLock, InheritanceContext, InheritanceBuffer);
		constexpr float TimeToJoinSeconds = 0.100f;
		GuardedData.WaitForGathererIdle(TimeToJoinSeconds);
		bInitialSearchStarted = GuardedData.IsInitialSearchStarted();
		bInitialSearchCompleted = GuardedData.IsInitialSearchCompleted();
	}

#if WITH_EDITOR
	if (bInitialSearchStarted && !bInitialSearchCompleted)
	{
		// If we do need to wait, then tick the DirectoryWatcher so we have the most up to date information.
		// This is also important because we ignore rescan events from the directory watcher if they are sent
		// during startup, so if there is a rescan event pending we want to trigger it now and ignore it
		if (GIsEditor) 	// In-game doesn't listen for directory changes
		{
			FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
			DirectoryWatcherModule.Get()->Tick(-1.f);
		}
	}
#endif

	for (;;)
	{
		FEventContext EventContext;
		EGatherStatus Status;
		{
			// Keep the LLM scope limited so it does not surround the broadcast which calls external code
			LLM_SCOPE(ELLMTag::AssetRegistry);
			FWriteScopeLock InterfaceScopeLock(InterfaceLock);
			FClassInheritanceContext InheritanceContext;
			FClassInheritanceBuffer InheritanceBuffer;
			GetInheritanceContextWithRequiredLock(InterfaceScopeLock, InheritanceContext, InheritanceBuffer);
			if (IsInGameThread())
			{
				// Process any deferred events. Required since deferred events would block sending the FileLoadedEvent
				EventContext = MoveTemp(DeferredEvents);
				DeferredEvents.Clear();
			}

			GuardedData.WaitForGathererIdleIfSynchronous();

			bool bUnusedInterrupted;
			Status = GuardedData.TickGatherer(EventContext, InheritanceContext, -1., bUnusedInterrupted);
		}
#if WITH_EDITOR
		ProcessLoadedAssetsToUpdateCache(EventContext, -1., Status);
#endif
		Broadcast(EventContext);
		if (Status != EGatherStatus::Active && Status != EGatherStatus::WaitingForEvents)
		{
			break;
		}

		FThreadHeartBeat::Get().HeartBeat();
		// Sleep long enough to avoid causing contention on the CriticalSection in GetAndTrimSearchResults
		constexpr float SleepTimeSeconds = 0.010f;
		FPlatformProcess::SleepNoStats(SleepTimeSeconds);
	}
}

void UAssetRegistryImpl::WaitForPremadeAssetRegistry()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UAssetRegistryImpl::WaitForPremadeAssetRegistry);
	using namespace UE::AssetRegistry::Impl;

	FEventContext EventContext;
	{
		LLM_SCOPE(ELLMTag::AssetRegistry);
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		FClassInheritanceContext InheritanceContext;
		FClassInheritanceBuffer InheritanceBuffer;
		GetInheritanceContextWithRequiredLock(InterfaceScopeLock, InheritanceContext, InheritanceBuffer);
		GuardedData.ConditionalLoadPremadeAssetRegistry(*this, EventContext, InterfaceScopeLock);
	}
	Broadcast(EventContext);
}

void UAssetRegistryImpl::ClearGathererCache()
{
	LLM_SCOPE(ELLMTag::AssetRegistry);
	FWriteScopeLock InterfaceScopeLock(InterfaceLock);
	GuardedData.ClearGathererCache();
}

namespace UE::AssetRegistry
{

void FAssetRegistryImpl::ClearGathererCache()
{
	if (GlobalGatherer)
	{
		GlobalGatherer->ClearCache();
	}
}

}

void UAssetRegistryImpl::WaitForPackage(const FString& PackageName)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UAssetRegistryImpl::WaitForPackage);

	UE::AssetRegistry::Impl::FEventContext EventContext;
	{
		LLM_SCOPE(ELLMTag::AssetRegistry);
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		if (GuardedData.IsLoadingAssets())
		{
			FString LocalPath;
			if (FPackageName::TryConvertLongPackageNameToFilename(PackageName, LocalPath))
			{
				GuardedData.TickGatherPackage(EventContext, PackageName, LocalPath);
			}
		}
	}
	Broadcast(EventContext);
}

bool UAssetRegistryImpl::HasAssets(const FName PackagePath, const bool bRecursive) const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	return GuardedData.HasAssets(PackagePath, bRecursive);
}

namespace UE::AssetRegistry
{

bool FAssetRegistryImpl::HasAssets(const FName PackagePath, const bool bRecursive) const
{
	bool bHasAssets = State.HasAssets(PackagePath, true /*bARFiltering*/);

	if (!bHasAssets && bRecursive)
	{
		CachedPathTree.EnumerateSubPaths(PackagePath, [this, &bHasAssets](FName SubPath)
		{
			bHasAssets = State.HasAssets(SubPath, true /*bARFiltering*/);
			return !bHasAssets;
		});
	}

	return bHasAssets;
}

}

bool UAssetRegistryImpl::GetAssetsByPackageName(FName PackageName, TArray<FAssetData>& OutAssetData, bool bIncludeOnlyOnDiskAssets, bool bSkipARFilteredAssets) const
{
	FARFilter Filter;
	Filter.PackageNames.Add(PackageName);
	Filter.bIncludeOnlyOnDiskAssets = bIncludeOnlyOnDiskAssets;
	return GetAssets(Filter, OutAssetData, bSkipARFilteredAssets);
}

bool UAssetRegistryImpl::GetAssetsByPath(FName PackagePath, TArray<FAssetData>& OutAssetData, bool bRecursive, bool bIncludeOnlyOnDiskAssets) const
{
	FARFilter Filter;
	Filter.bRecursivePaths = bRecursive;
	Filter.PackagePaths.Add(PackagePath);
	Filter.bIncludeOnlyOnDiskAssets = bIncludeOnlyOnDiskAssets;
	return GetAssets(Filter, OutAssetData);
}

bool UAssetRegistryImpl::GetAssetsByPaths(TArray<FName> PackagePaths, TArray<FAssetData>& OutAssetData, bool bRecursive, bool bIncludeOnlyOnDiskAssets) const
{
	FARFilter Filter;
	Filter.bRecursivePaths = bRecursive;
	Filter.PackagePaths = MoveTemp(PackagePaths);
	Filter.bIncludeOnlyOnDiskAssets = bIncludeOnlyOnDiskAssets;
	return GetAssets(Filter, OutAssetData);
}

namespace UE::AssetRegistry
{

void FAssetRegistryImpl::EnumerateAssetsByPathNoTags(FName PackagePath,
	TFunctionRef<bool(const FAssetData&)> Callback, bool bRecursive, bool bIncludeOnlyOnDiskAssets) const
{
	if (PackagePath.IsNone())
	{
		return;
	}
	FARFilter Filter;
	Filter.bRecursivePaths = bRecursive;
	Filter.PackagePaths.Add(PackagePath);
	Filter.bIncludeOnlyOnDiskAssets = bIncludeOnlyOnDiskAssets;

	// CompileFilter takes an inheritance context, but only to handle filters with recursive classes, which we are not using here
	UE::AssetRegistry::Impl::FClassInheritanceContext EmptyInheritanceContext;
	FARCompiledFilter CompiledFilter;
	CompileFilter(EmptyInheritanceContext, Filter, CompiledFilter);

	TSet<FName> PackagesToSkip;
	if (!bIncludeOnlyOnDiskAssets)
	{
		bool bStopIteration;
		Utils::EnumerateMemoryAssetsHelper(CompiledFilter, PackagesToSkip, bStopIteration,
			[&Callback](const UObject* Object, FAssetData&& PartialAssetData)
			{
				return Callback(PartialAssetData);
			}, true /* bSkipARFilteredAssets */);
		if (bStopIteration)
		{
			return;
		}
	}
	EnumerateDiskAssets(CompiledFilter, PackagesToSkip, Callback, true /* bSkipARFilteredAssets */);
}

}

static FTopLevelAssetPath TryConvertShortTypeNameToPathName(FName ClassName)
{
	FTopLevelAssetPath ClassPathName;
	if (ClassName != NAME_None)
	{
		FString ShortClassName = ClassName.ToString();
		ClassPathName = UClass::TryConvertShortTypeNameToPathName<UStruct>(*ShortClassName, ELogVerbosity::Warning, TEXT("AssetRegistry using deprecated function"));
		UE_CLOG(ClassPathName.IsNull(), LogClass, Error, TEXT("Failed to convert short class name %s to class path name."), *ShortClassName);
	}
	return ClassPathName;
}

bool UAssetRegistryImpl::GetAssetsByClass(FTopLevelAssetPath ClassPathName, TArray<FAssetData>& OutAssetData, bool bSearchSubClasses) const
{
	FARFilter Filter;
	Filter.ClassPaths.Add(ClassPathName);
	Filter.bRecursiveClasses = bSearchSubClasses;
	return GetAssets(Filter, OutAssetData);
}

bool UAssetRegistryImpl::GetAssetsByTags(const TArray<FName>& AssetTags, TArray<FAssetData>& OutAssetData) const
{
	FARFilter Filter;
	for (const FName& AssetTag : AssetTags)
	{
		Filter.TagsAndValues.Add(AssetTag);
	}
	return GetAssets(Filter, OutAssetData);
}

bool UAssetRegistryImpl::GetAssetsByTagValues(const TMultiMap<FName, FString>& AssetTagsAndValues, TArray<FAssetData>& OutAssetData) const
{
	FARFilter Filter;
	for (const auto& AssetTagsAndValue : AssetTagsAndValues)
	{
		Filter.TagsAndValues.Add(AssetTagsAndValue.Key, AssetTagsAndValue.Value);
	}
	return GetAssets(Filter, OutAssetData);
}

bool UAssetRegistryImpl::GetAssets(const FARFilter& InFilter, TArray<FAssetData>& OutAssetData,
	bool bSkipARFilteredAssets) const
{
	using namespace UE::AssetRegistry::Utils;

	FARCompiledFilter CompiledFilter;
	CompileFilter(InFilter, CompiledFilter);
	if (CompiledFilter.IsEmpty() || !IsFilterValid(CompiledFilter))
	{
		return false;
	}

	TSet<FName> PackagesToSkip;
	if (!InFilter.bIncludeOnlyOnDiskAssets)
	{
		bool bStopIterationUnused;
		EnumerateMemoryAssets(CompiledFilter, PackagesToSkip, bStopIterationUnused,
			InterfaceLock, GuardedData.GetState(),
			[&OutAssetData](FAssetData&& AssetData)
			{
				OutAssetData.Add(MoveTemp(AssetData));
				return true;
			}, bSkipARFilteredAssets);
	}

	{
		FReadScopeLock InterfaceScopeLock(InterfaceLock);
		GuardedData.EnumerateDiskAssets(CompiledFilter, PackagesToSkip, [&OutAssetData](const FAssetData& AssetData)
			{
				OutAssetData.Emplace(AssetData);
				return true;
			}, bSkipARFilteredAssets);
	}
	return true;
}

bool UAssetRegistryImpl::EnumerateAssets(const FARFilter& InFilter, TFunctionRef<bool(const FAssetData&)> Callback,
	bool bSkipARFilteredAssets) const
{
	FARCompiledFilter CompiledFilter;
	CompileFilter(InFilter, CompiledFilter);
	return EnumerateAssets(CompiledFilter, Callback, bSkipARFilteredAssets);
}

bool UAssetRegistryImpl::EnumerateAssets(const FARCompiledFilter& InFilter, TFunctionRef<bool(const FAssetData&)> Callback,
	bool bSkipARFilteredAssets) const
{
	using namespace UE::AssetRegistry::Utils;

	// Verify filter input. If all assets are needed, use EnumerateAllAssets() instead.
	if (InFilter.IsEmpty() || !IsFilterValid(InFilter))
	{
		return false;
	}

	TSet<FName> PackagesToSkip;
	if (!InFilter.bIncludeOnlyOnDiskAssets)
	{
		bool bStopIteration;
		EnumerateMemoryAssets(InFilter, PackagesToSkip, bStopIteration,
			InterfaceLock, GuardedData.GetState(),
			[&Callback](FAssetData&& AssetData)
			{
				return Callback(AssetData);
			}, bSkipARFilteredAssets);
		if (bStopIteration)
		{
			return true;
		}
	}

	TArray<FAssetData, TInlineAllocator<128>> FoundAssets;
	{
		FReadScopeLock InterfaceScopeLock(InterfaceLock);
		GuardedData.EnumerateDiskAssets(InFilter, PackagesToSkip, [&FoundAssets](const FAssetData& AssetData)
			{
				FoundAssets.Emplace(AssetData);
				return true;
			}, bSkipARFilteredAssets);
	}
	for (const FAssetData& AssetData : FoundAssets)
	{
		if (!Callback(AssetData))
		{
			break;
		}
	}
	return true;
}

namespace UE::AssetRegistry
{

namespace Utils
{

TOptional<FAssetDataTagMap> AddNonOverlappingTags(FAssetData& ExistingAssetData, const FAssetData& NewAssetData)
{
	TOptional<FAssetDataTagMap> ModifiedTags;
	NewAssetData.TagsAndValues.ForEach([&ExistingAssetData, &ModifiedTags](const TPair<FName, FAssetTagValueRef>& TagPair)
		{
			if (ModifiedTags)
			{
				if (!ModifiedTags->Contains(TagPair.Key))
				{
					ModifiedTags->Add(TagPair.Key, TagPair.Value.GetStorageString());
				}
			}
			else
			{
				if (!ExistingAssetData.TagsAndValues.Contains(TagPair.Key))
				{
					ModifiedTags.Emplace(ExistingAssetData.TagsAndValues.CopyMap());
					ModifiedTags->Add(TagPair.Key, TagPair.Value.GetStorageString());
				}
			}
		});
	return ModifiedTags;
}

void EnumerateMemoryAssetsHelper(const FARCompiledFilter& InFilter, TSet<FName>& OutPackageNamesWithAssets,
	bool& bOutStopIteration, TFunctionRef<bool(const UObject* Object, FAssetData&& PartialAssetData)> Callback,
	bool bSkipARFilteredAssets)
{
	checkf(IsInGameThread(), TEXT("Enumerating in-memory assets can only be done on the game thread; it uses non-threadsafe UE::AssetRegistry::Filtering globals."));
	bOutStopIteration = false;

	// Skip assets that were loaded for diffing
	const uint32 FilterWithoutPackageFlags = InFilter.WithoutPackageFlags | PKG_ForDiffing;
	const uint32 FilterWithPackageFlags = InFilter.WithPackageFlags;

	struct FFilterData
	{
		const UObject* Object;
		const UPackage* Package;
		FString PackageNameStr;
		FSoftObjectPath ObjectPath;
	};

	/**
	 * The portions of the filter that are safe to execute even in the UObject global hash lock in FThreadSafeObjectIterator
	 * Returns true if the object passes the filter and should be copied into an array for calling the rest of the filter
	 * outside the lock.
	 */
	auto PassesLockSafeFilter =
		[&InFilter, bSkipARFilteredAssets, FilterWithoutPackageFlags, FilterWithPackageFlags]
	(const UObject* Obj, FFilterData& FilterData)
	{
		if (!Obj->IsAsset())
		{
			return false;
		}

		// Skip assets that are currently loading
		if (Obj->HasAnyFlags(RF_NeedLoad))
		{
			return false;
		}

		check(!Obj->GetPackage()->HasAnyPackageFlags(PKG_PlayInEditor));
		check(!Obj->GetOutermostObject()->GetPackage()->HasAnyPackageFlags(PKG_PlayInEditor));

		FilterData.Package = Obj->GetOutermost();

		// Skip assets with any of the specified 'without' package flags 
		if (FilterData.Package->HasAnyPackageFlags(FilterWithoutPackageFlags))
		{
			return false;
		}

		// Skip assets without any the specified 'with' packages flags
		if (!FilterData.Package->HasAllPackagesFlags(FilterWithPackageFlags))
		{
			return false;
		}

		// Skip classes that report themselves as assets but that the editor AssetRegistry is currently not counting as assets
		if (bSkipARFilteredAssets && UE::AssetRegistry::FFiltering::ShouldSkipAsset(Obj))
		{
			return false;
		}

		// Package name
		const FName PackageName = FilterData.Package->GetFName();

		if (InFilter.PackageNames.Num() && !InFilter.PackageNames.Contains(PackageName))
		{
			return false;
		}

		// Asset Path
		FilterData.ObjectPath = FSoftObjectPath(Obj);
		if (InFilter.SoftObjectPaths.Num() > 0)
		{
			if (!InFilter.SoftObjectPaths.Contains(FilterData.ObjectPath))
			{
				return false;
			}
		}

		// Package path
		PackageName.ToString(FilterData.PackageNameStr);
		if (InFilter.PackagePaths.Num() > 0)
		{
			const FName PackagePath = FName(*FPackageName::GetLongPackagePath(FilterData.PackageNameStr));
			if (!InFilter.PackagePaths.Contains(PackagePath))
			{
				return false;
			}
		}

		FilterData.Object = Obj;
		return true;
	};

	auto RunUnsafeFilterAndCallback =
		[&Callback, &OutPackageNamesWithAssets]
	(FFilterData& FilterData, bool& bOutContinue)
	{
		// We mark the package found for this passing asset, so that any followup search for assets on disk will not
		// add a duplicate of this Asset. We do this here for convenience; it would be more correct to call it only for assets that
		// pass the callers remaining filters inside of Callback
		OutPackageNamesWithAssets.Add(FilterData.Package->GetFName());

		// Could perhaps save some FName -> String conversions by creating this a bit earlier using the UObject constructor
		// to get package name and path.
		FAssetData PartialAssetData(MoveTemp(FilterData.PackageNameStr), FilterData.ObjectPath.ToString(),
			FilterData.Object->GetClass()->GetClassPathName(), FAssetDataTagMap(),
			FilterData.Package->GetChunkIDs(), FilterData.Package->GetPackageFlags());

		// All filters passed, except for AssetRegistry filter; caller must check that one
		bOutContinue = Callback(FilterData.Object, MoveTemp(PartialAssetData));
	};

	// Iterate over all in-memory assets to find the ones that pass the filter components
	if (InFilter.ClassPaths.Num() > 0 || InFilter.PackageNames.Num() > 0)
	{
		TArray<UObject*, TInlineAllocator<10>> InMemoryObjects;
		if (InFilter.ClassPaths.Num())
		{
			for (FTopLevelAssetPath ClassName : InFilter.ClassPaths)
			{
				UClass* Class = FindObject<UClass>(ClassName);
				if (Class != nullptr)
				{
					ForEachObjectOfClass(Class, [&InMemoryObjects](UObject* Object)
						{
							InMemoryObjects.Add(Object);
						}, false /* bIncludeDerivedClasses */, RF_NoFlags);
				}
			}
		}
		else
		{
			for (FName PackageName : InFilter.PackageNames)
			{
				UPackage* Package = FindObjectFast<UPackage>(nullptr, PackageName);
				if (Package != nullptr)
				{
					// Store objects in an intermediate rather than calling FilterInMemoryObjectLambda on them directly
					// because the callback is arbitrary code and might create UObjects, which is disallowed in
					// ForEachObjectWithPackage
					ForEachObjectWithPackage(Package, [&InMemoryObjects](UObject* Object)
						{
							// Avoid adding an element to InMemoryObjects for every UObject
							// There could be many UObjects (thousands) but only a single Asset
							if (Object->IsAsset())
							{
								InMemoryObjects.Add(Object);
							}
							return true;
						});
				}
			}
		}

		FFilterData ScratchFilterData;
		for (const UObject* Object : InMemoryObjects)
		{
			if (PassesLockSafeFilter(Object, ScratchFilterData))
			{
				bool bContinue = true;
				RunUnsafeFilterAndCallback(ScratchFilterData, bContinue);
				if (!bContinue)
				{
					bOutStopIteration = true;
					return;
				}
			}
		}
	}
	else
	{
		TArray<FFilterData> FirstPassFilterResults;
		FFilterData ScratchFilterData;
		for (FThreadSafeObjectIterator ObjIt; ObjIt; ++ObjIt)
		{
			if (PassesLockSafeFilter(*ObjIt, ScratchFilterData))
			{
				FirstPassFilterResults.Add(MoveTemp(ScratchFilterData));
			}
		}

		for (FFilterData& FilterData : FirstPassFilterResults)
		{
			bool bContinue = true;
			RunUnsafeFilterAndCallback(FilterData, bContinue);
			if (!bContinue)
			{
				bOutStopIteration = true;
				return;
			}

			FPlatformMisc::PumpEssentialAppMessages();
		}
	}
}

void EnumerateMemoryAssets(const FARCompiledFilter& InFilter, TSet<FName>& OutPackageNamesWithAssets,
	bool& bOutStopIteration, FRWLock& InterfaceLock, const FAssetRegistryState& GuardedDataState,
	TFunctionRef<bool(FAssetData&&)> Callback, bool bSkipARFilteredAssets)
{
	check(!InFilter.IsEmpty() && Utils::IsFilterValid(InFilter));
	EnumerateMemoryAssetsHelper(InFilter, OutPackageNamesWithAssets, bOutStopIteration,
		[&InFilter, &Callback, &InterfaceLock, &GuardedDataState](const UObject* Object, FAssetData&& PartialAssetData)
		{
			FAssetRegistryTagsContextData Context(Object, EAssetRegistryTagsCaller::AssetRegistryQuery);
			Object->GetAssetRegistryTags(Context, PartialAssetData);
			{
				// GetAssetRegistryTags with EAssetRegistryTagsCaller::AssetRegistryQuery does not add some tags that
				// are too expensive to regularly compute but that exist in the on-disk Asset from SavePackage.
				// Our contract for on-disk versus in-memory tags is that in-memory tags override on-disk tags, but we
				// keep any on-disk tags that do not exist in the in-memory tags because they may be extended tags.
				FReadScopeLock InterfaceScopeLock(InterfaceLock);
				const FAssetData* OnDiskAssetData = GuardedDataState.GetAssetByObjectPath(FSoftObjectPath(Object));
				if (OnDiskAssetData)
				{
					TOptional<FAssetDataTagMap> ModifiedTags = Utils::AddNonOverlappingTags(PartialAssetData, *OnDiskAssetData);
					if (ModifiedTags)
					{
						PartialAssetData.TagsAndValues = FAssetDataTagMapSharedView(MoveTemp(*ModifiedTags));
					}
#if !WITH_EDITORONLY_DATA
					// In non-editor builds, UObject::GetChunkIds returns an empty set.
					// Like our contract for tags, when the information is missing from the UObject, our contract
					// for that information in AssetRegistry queries is that we return the on-disk version of the data.
					// The on-disk version of the data for GetChunkIds is the data that was stored in the generated
					// AssetRegistry by calling AddChunkId for each chunkID that the cooker found the Asset to be in.
					PartialAssetData.SetChunkIDs(OnDiskAssetData->GetChunkIDs());
#endif
				}
			}
			// After adding tags, PartialAssetData is now a full AssetData

			// Tags and values
			if (InFilter.TagsAndValues.Num() > 0)
			{
				bool bMatch = false;
				for (const TPair<FName, TOptional<FString>>& FilterPair : InFilter.TagsAndValues)
				{
					FAssetTagValueRef RegistryValue = PartialAssetData.TagsAndValues.FindTag(FilterPair.Key);

					if (RegistryValue.IsSet() && (!FilterPair.Value.IsSet() || RegistryValue == FilterPair.Value.GetValue()))
					{
						bMatch = true;
						break;
					}
				}

				if (!bMatch)
				{
					return true;
				}
			}

			// All filters passed
			return Callback(MoveTemp(PartialAssetData));
		}, bSkipARFilteredAssets);
}

}

void FAssetRegistryImpl::EnumerateDiskAssets(const FARCompiledFilter& InFilter, TSet<FName>& PackagesToSkip,
	TFunctionRef<bool(const FAssetData&)> Callback, bool bSkipARFilteredAssets) const
{
	check(!InFilter.IsEmpty() && Utils::IsFilterValid(InFilter));
	PackagesToSkip.Append(CachedEmptyPackages);
	State.EnumerateAssets(InFilter, PackagesToSkip, Callback, bSkipARFilteredAssets);
}

}

FAssetData UAssetRegistryImpl::GetAssetByObjectPath(const FSoftObjectPath& ObjectPath, bool bIncludeOnlyOnDiskAssets, bool bSkipARFilteredAssets) const
{
	if (!bIncludeOnlyOnDiskAssets)
	{
		TStringBuilder<FName::StringBufferSize> Builder;
		ObjectPath.ToString(Builder);
		UObject* Asset = FindObject<UObject>(nullptr, *Builder);

		if (Asset)
		{
			if (!bSkipARFilteredAssets || !UE::AssetRegistry::FFiltering::ShouldSkipAsset(Asset))
			{
				return FAssetData(Asset, FAssetData::ECreationFlags::None /** Do not allow blueprint classes */,
					EAssetRegistryTagsCaller::AssetRegistryQuery);
			}
			else
			{
				return FAssetData();
			}
		}
	}

	{
		FReadScopeLock InterfaceScopeLock(InterfaceLock);
		const FAssetRegistryState& State = GuardedData.GetState();
		const FAssetData* FoundData = State.GetAssetByObjectPath(ObjectPath);
		return (FoundData && !State.IsPackageUnmountedAndFiltered(FoundData->PackageName)
			&& (!bSkipARFilteredAssets || !GuardedData.ShouldSkipAsset(FoundData->AssetClassPath, FoundData->PackageFlags))) ? *FoundData : FAssetData();
	}
}

FAssetData UAssetRegistryImpl::GetAssetByObjectPath(const FName ObjectPath, bool bIncludeOnlyOnDiskAssets) const
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	return GetAssetByObjectPath(FSoftObjectPath(ObjectPath.ToString()), bIncludeOnlyOnDiskAssets);
PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

UE::AssetRegistry::EExists UAssetRegistryImpl::TryGetAssetByObjectPath(const FSoftObjectPath& ObjectPath, FAssetData& OutAssetData) const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	bool bAssetRegistryReady = GuardedData.IsInitialSearchStarted() && GuardedData.IsInitialSearchCompleted();
	const FAssetRegistryState& State = GuardedData.GetState();
	const FAssetData* FoundData = State.GetAssetByObjectPath(ObjectPath);
	if (!FoundData)
	{
		if (!bAssetRegistryReady)
		{
			return UE::AssetRegistry::EExists::Unknown;
		}
		return UE::AssetRegistry::EExists::DoesNotExist;
	}
	OutAssetData = *FoundData;
	return UE::AssetRegistry::EExists::Exists;
}

UE::AssetRegistry::EExists UAssetRegistryImpl::TryGetAssetPackageData(const FName PackageName, FAssetPackageData& OutAssetPackageData) const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	bool bAssetRegistryReady = GuardedData.IsInitialSearchStarted() && GuardedData.IsInitialSearchCompleted();
	const FAssetRegistryState& State = GuardedData.GetState();
	const FAssetPackageData* FoundData = State.GetAssetPackageData(PackageName);
	if (!FoundData)
	{
		if (!bAssetRegistryReady)
		{
			return UE::AssetRegistry::EExists::Unknown;
		}
		return UE::AssetRegistry::EExists::DoesNotExist;
	}
	OutAssetPackageData = *FoundData;
	return UE::AssetRegistry::EExists::Exists;
}

bool UAssetRegistryImpl::GetAllAssets(TArray<FAssetData>& OutAssetData, bool bIncludeOnlyOnDiskAssets) const
{
	const double GetAllAssetsStartTime = FPlatformTime::Seconds();
	TSet<FName> PackageNamesToSkip;

	// All in memory assets
	if (!bIncludeOnlyOnDiskAssets)
	{
		bool bStopIterationUnused;
		UE::AssetRegistry::Utils::EnumerateAllMemoryAssets(PackageNamesToSkip, bStopIterationUnused,
			[&OutAssetData](FAssetData&& AssetData)
			{
				OutAssetData.Add(MoveTemp(AssetData));
				return true;
			});
	}

	{
		FReadScopeLock InterfaceScopeLock(InterfaceLock);
		GuardedData.EnumerateAllDiskAssets(PackageNamesToSkip,
			[&OutAssetData](const FAssetData& AssetData)
			{
				OutAssetData.Add(AssetData);
				return true;
			});
	}

	UE_LOG(LogAssetRegistry, VeryVerbose, TEXT("GetAllAssets completed in %0.4f seconds"), FPlatformTime::Seconds() - GetAllAssetsStartTime);
	return true;
}

bool UAssetRegistryImpl::EnumerateAllAssets(TFunctionRef<bool(const FAssetData&)> Callback, bool bIncludeOnlyOnDiskAssets) const
{
	const double GetAllAssetsStartTime = FPlatformTime::Seconds();
	TSet<FName> PackageNamesToSkip;

	// All in memory assets
	if (!bIncludeOnlyOnDiskAssets)
	{
		bool bStopIteration;
		UE::AssetRegistry::Utils::EnumerateAllMemoryAssets(PackageNamesToSkip, bStopIteration,
			[&Callback](FAssetData&& AssetData)
			{
				return Callback(AssetData);
			});
		if (bStopIteration)
		{
			return true;
		}
	}

	// We have to call the callback on a copy rather than a reference since the callback may reenter the lock
	TArray<FAssetData, TInlineAllocator<128>> OnDiskAssetDatas;
	{
		FReadScopeLock InterfaceScopeLock(InterfaceLock);
		GuardedData.EnumerateAllDiskAssets(PackageNamesToSkip,
			[&OnDiskAssetDatas](const FAssetData& AssetData)
			{
				OnDiskAssetDatas.Add(AssetData);
				return true;
			});
	}

	for (const FAssetData& AssetData : OnDiskAssetDatas)
	{
		if (!Callback(AssetData))
		{
			return true;
		}
	}
	return true;
}

namespace UE::AssetRegistry
{

namespace Utils
{

void EnumerateAllMemoryAssets(TSet<FName>& OutPackageNamesWithAssets, bool& bOutStopIteration,
	TFunctionRef<bool(FAssetData&&)> Callback)
{
	checkf(IsInGameThread(), TEXT("Enumerating memory assets can only be done on the game thread; it uses non-threadsafe UE::AssetRegistry::Filtering globals."));
	bOutStopIteration = false;
	for (FThreadSafeObjectIterator ObjIt; ObjIt; ++ObjIt)
	{
		if (ObjIt->IsAsset() && !UE::AssetRegistry::FFiltering::ShouldSkipAsset(*ObjIt))
		{
			FAssetData AssetData(*ObjIt, true /* bAllowBlueprintClass */);
			OutPackageNamesWithAssets.Add(AssetData.PackageName);
			if (!Callback(MoveTemp(AssetData)))
			{
				bOutStopIteration = true;
				return;
			}
		}
	}
}

}

void FAssetRegistryImpl::EnumerateAllDiskAssets(TSet<FName>& PackageNamesToSkip, TFunctionRef<bool(const FAssetData&)> Callback) const
{
	PackageNamesToSkip.Append(CachedEmptyPackages);
	State.EnumerateAllAssets(PackageNamesToSkip, Callback, true /*bARFiltering*/);
}

}

void UAssetRegistryImpl::GetPackagesByName(FStringView PackageName, TArray<FName>& OutPackageNames) const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	const FAssetRegistryState& State = GuardedData.GetState();
	UE_CLOG(GuardedData.IsInitialSearchStarted() && !GuardedData.IsInitialSearchCompleted(), LogAssetRegistry, Warning,
		TEXT("GetPackagesByName has been called before AssetRegistry gather is complete and it does not wait. ")
		TEXT("The search may return incomplete results."));
	State.GetPackagesByName(PackageName, OutPackageNames);

}

FName UAssetRegistryImpl::GetFirstPackageByName(FStringView PackageName) const
{
	FName LongPackageName;
	bool bSearchAllAssets;
	{
		FReadScopeLock InterfaceScopeLock(InterfaceLock);
		const FAssetRegistryState& State = GuardedData.GetState();
		UE_CLOG(GuardedData.IsInitialSearchStarted() && !GuardedData.IsInitialSearchCompleted(), LogAssetRegistry, Warning,
			TEXT("GetFirstPackageByName has been called before AssetRegistry gather is complete and it does not wait. ")
			TEXT("The search may fail to find the package."));
		LongPackageName = State.GetFirstPackageByName(PackageName);
		bSearchAllAssets = GuardedData.IsSearchAllAssets();
	}
#if WITH_EDITOR
	if (!GIsEditor && !bSearchAllAssets)
	{
		// Temporary support for -game:
		// When running editor.exe with -game, we do not have a cooked AssetRegistry and we do not scan either
		// In that case, fall back to searching on disk if the search in the AssetRegistry (as expected) fails
		// In the future we plan to avoid this situation by having -game run the scan as well
		if (LongPackageName.IsNone())
		{
			UE_LOG(LogAssetRegistry, Warning,
				TEXT("GetFirstPackageByName is being called in `-game` to resolve partial package name. ")
				TEXT("This may cause a slow scan on disk. ")
				TEXT("Consider using the fully qualified package name for better performance. "));
			FString LongPackageNameString;
			if (FPackageName::SearchForPackageOnDisk(FString(PackageName), &LongPackageNameString))
			{
				LongPackageName = FName(*LongPackageNameString);
			}
		}
	}
#endif
	return LongPackageName;
}

bool UAssetRegistryImpl::GetDependencies(const FAssetIdentifier& AssetIdentifier, TArray<FAssetIdentifier>& OutDependencies, UE::AssetRegistry::EDependencyCategory Category, const UE::AssetRegistry::FDependencyQuery& Flags) const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	return GuardedData.GetState().GetDependencies(AssetIdentifier, OutDependencies, Category, Flags);
}

bool UAssetRegistryImpl::GetDependencies(const FAssetIdentifier& AssetIdentifier, TArray<FAssetDependency>& OutDependencies, UE::AssetRegistry::EDependencyCategory Category, const UE::AssetRegistry::FDependencyQuery& Flags) const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	return GuardedData.GetState().GetDependencies(AssetIdentifier, OutDependencies, Category, Flags);
}

static void ConvertAssetIdentifiersToPackageNames(const TArray<FAssetIdentifier>& AssetIdentifiers, TArray<FName>& OutPackageNames)
{
	// add all PackageNames :
	OutPackageNames.Reserve(OutPackageNames.Num() + AssetIdentifiers.Num());
	for (const FAssetIdentifier& AssetId : AssetIdentifiers)
	{
		if (AssetId.PackageName != NAME_None)
		{
			OutPackageNames.Add(AssetId.PackageName);
		}
	}

	// make unique ; sort in previous contents of OutPackageNames to unique against them too
	OutPackageNames.Sort( FNameFastLess() );

	int UniqueNum = Algo::Unique( OutPackageNames );
	OutPackageNames.SetNum(UniqueNum, EAllowShrinking::No);
}

bool UAssetRegistryImpl::GetDependencies(FName PackageName, TArray<FName>& OutDependencies, UE::AssetRegistry::EDependencyCategory Category, const UE::AssetRegistry::FDependencyQuery& Flags) const
{
	TArray<FAssetIdentifier> TempDependencies;
	if (!GetDependencies(FAssetIdentifier(PackageName), TempDependencies, Category, Flags))
	{
		return false;
	}
	ConvertAssetIdentifiersToPackageNames(TempDependencies, OutDependencies);
	return true;
}

bool IAssetRegistry::K2_GetDependencies(FName PackageName, const FAssetRegistryDependencyOptions& DependencyOptions, TArray<FName>& OutDependencies) const
{
	UE::AssetRegistry::FDependencyQuery Flags;
	bool bResult = false;
	if (DependencyOptions.GetPackageQuery(Flags))
	{
		bResult = GetDependencies(PackageName, OutDependencies, UE::AssetRegistry::EDependencyCategory::Package, Flags) || bResult;
	}
	if (DependencyOptions.GetSearchableNameQuery(Flags))
	{
		bResult = GetDependencies(PackageName, OutDependencies, UE::AssetRegistry::EDependencyCategory::SearchableName, Flags) || bResult;
	}
	if (DependencyOptions.GetManageQuery(Flags))
	{
		bResult = GetDependencies(PackageName, OutDependencies, UE::AssetRegistry::EDependencyCategory::Manage, Flags) || bResult;
	}
	return bResult;
}

bool UAssetRegistryImpl::GetReferencers(const FAssetIdentifier& AssetIdentifier, TArray<FAssetIdentifier>& OutReferencers, UE::AssetRegistry::EDependencyCategory Category, const UE::AssetRegistry::FDependencyQuery& Flags) const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	return GuardedData.GetState().GetReferencers(AssetIdentifier, OutReferencers, Category, Flags);
}

bool UAssetRegistryImpl::GetReferencers(const FAssetIdentifier& AssetIdentifier, TArray<FAssetDependency>& OutReferencers, UE::AssetRegistry::EDependencyCategory Category, const UE::AssetRegistry::FDependencyQuery& Flags) const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	return GuardedData.GetState().GetReferencers(AssetIdentifier, OutReferencers, Category, Flags);
}

bool UAssetRegistryImpl::GetReferencers(FName PackageName, TArray<FName>& OutReferencers, UE::AssetRegistry::EDependencyCategory Category, const UE::AssetRegistry::FDependencyQuery& Flags) const
{
	TArray<FAssetIdentifier> TempReferencers;

	if (!GetReferencers(FAssetIdentifier(PackageName), TempReferencers, Category, Flags))
	{
		return false;
	}
	ConvertAssetIdentifiersToPackageNames(TempReferencers, OutReferencers);
	return true;
}

bool IAssetRegistry::K2_GetReferencers(FName PackageName, const FAssetRegistryDependencyOptions& ReferenceOptions, TArray<FName>& OutReferencers) const
{
	UE::AssetRegistry::FDependencyQuery Flags;
	bool bResult = false;
	if (ReferenceOptions.GetPackageQuery(Flags))
	{
		bResult = GetReferencers(PackageName, OutReferencers, UE::AssetRegistry::EDependencyCategory::Package, Flags) || bResult;
	}
	if (ReferenceOptions.GetSearchableNameQuery(Flags))
	{
		bResult = GetReferencers(PackageName, OutReferencers, UE::AssetRegistry::EDependencyCategory::SearchableName, Flags) || bResult;
	}
	if (ReferenceOptions.GetManageQuery(Flags))
	{
		bResult = GetReferencers(PackageName, OutReferencers, UE::AssetRegistry::EDependencyCategory::Manage, Flags) || bResult;
	}

	return bResult;
}

TOptional<FAssetPackageData> UAssetRegistryImpl::GetAssetPackageDataCopy(FName PackageName) const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	const FAssetPackageData* AssetPackageData = GuardedData.GetState().GetAssetPackageData(PackageName);
	return AssetPackageData ? *AssetPackageData : TOptional<FAssetPackageData>();
}

void UAssetRegistryImpl::EnumerateAllPackages(TFunctionRef<void(FName PackageName, const FAssetPackageData& PackageData)> Callback) const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	for (const TPair<FName, const FAssetPackageData*>& Pair : GuardedData.GetState().GetAssetPackageDataMap())
	{
		Callback(Pair.Key, *Pair.Value);
	}
}


bool UAssetRegistryImpl::DoesPackageExistOnDisk(FName PackageName, FString* OutCorrectCasePackageName, FString* OutExtension) const
{
	auto CalculateExtension = [](const FString& PackageNameStr, TConstArrayView<FAssetData> Assets) -> FString
	{
		FTopLevelAssetPath ClassRedirector = UObjectRedirector::StaticClass()->GetClassPathName();
		bool bContainsMap = false;
		bool bContainsRedirector = false;
		for (const FAssetData& Asset : Assets)
		{
			bContainsMap |= ((Asset.PackageFlags & PKG_ContainsMap) != 0);
			bContainsRedirector |= (Asset.AssetClassPath == ClassRedirector);
		}
		if (!bContainsMap && bContainsRedirector)
		{
			// presence of map -> .umap
			// But we can only assume lack of map -> .uasset if we know the type of every object in the package.
			// If we don't, because there was a redirector, we have to check the package on disk
			FString FileName;
			if (FPackageName::DoesPackageExist(PackageNameStr, &FileName, false /* InAllowTextFormats */))
			{
				check(!FileName.IsEmpty());
				return FPaths::GetExtension(FileName, true /* bIncludeDot */);
			}
		}
		return bContainsMap ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension();
	};

#if WITH_EDITOR
	if (GIsEditor)
	{
		// The editor always gathers PackageAssetDatas and uses those because they exactly match files on disk, whereas AssetsByPackageName
		// includes memory-only assets that have added themselves to the AssetRegistry's State.
		FString PackageNameStr = PackageName.ToString();
		if (FPackageName::IsScriptPackage(PackageNameStr))
		{
			// Script packages are an exception; the AssetRegistry creates AssetPackageData for them but they exist only in memory
			return false;
		}

		const FAssetPackageData* AssetPackageData;
		{
			FReadScopeLock InterfaceScopeLock(InterfaceLock);
			AssetPackageData = GuardedData.GetState().GetAssetPackageData(PackageName);
		}
		const static bool bVerifyNegativeResults = FParse::Param(FCommandLine::Get(), TEXT("AssetRegistryValidatePackageExists"));
		if (bVerifyNegativeResults && !AssetPackageData)
		{
			FString FileName;
			if (FPackageName::DoesPackageExist(PackageNameStr, &FileName, false /* InAllowTextFormats */))
			{
				UE_LOG(LogAssetRegistry, Warning, TEXT("Package %s exists on disk but does not exist in the AssetRegistry"), *PackageNameStr);
				if (OutCorrectCasePackageName)
				{
					FPackageName::TryConvertLongPackageNameToFilename(FileName, *OutCorrectCasePackageName);
				}
				if (OutExtension)
				{
					*OutExtension = FPaths::GetExtension(FileName, true /* bIncludeDot */);
				}
				return true;
			}
		}

		if (!AssetPackageData)
		{
			return false;
		}

		if (OutCorrectCasePackageName)
		{
			// TODO: Implement this correctly by saving the information in AssetPackageData if the capitalization does not match the FName capitalization
			*OutCorrectCasePackageName = PackageNameStr;
		}
		if (OutExtension)
		{
			if (AssetPackageData->Extension == EPackageExtension::Unspecified || AssetPackageData->Extension == EPackageExtension::Custom)
			{
				FString FileName;
				if (FPackageName::DoesPackageExist(PackageNameStr, &FileName, false /* InAllowTextFormats */))
				{
					check(!FileName.IsEmpty());
					*OutExtension = FPaths::GetExtension(FileName, true /* bIncludeDot */);
				}
				else
				{
					UE_LOG(LogAssetRegistry, Error,
						TEXT("UAssetRegistryImpl::DoesPackageExistOnDisk failed to find the extension for %s. The package exists in the AssetRegistry but does not exist on disk."),
						*PackageNameStr);
					TArray<FAssetData> Assets;
					GetAssetsByPackageName(PackageName, Assets, /*bIncludeOnlyDiskAssets*/ true);
					*OutExtension = CalculateExtension(PackageNameStr, Assets);
				}
			}
			else
			{
				*OutExtension = LexToString(AssetPackageData->Extension);
			}
		}
		return true;
	}
	else
#endif
	{
		// Runtime Game and Programs use GetAssetsByPackageName, which will match the files on disk since these configurations do not
		// add loaded assets to the AssetRegistryState
		TArray<FAssetData> Assets;
		GetAssetsByPackageName(PackageName, Assets, /*bIncludeOnlyDiskAssets*/ true);
		if (Assets.Num() == 0)
		{
			return false;
		}
		FString PackageNameStr = PackageName.ToString();
		if (OutCorrectCasePackageName)
		{
			// In Game does not handle matching case, but it still needs to return a value for the CorrectCase field if asked
			*OutCorrectCasePackageName = PackageNameStr;
		}
		if (OutExtension)
		{
			*OutExtension = CalculateExtension(PackageNameStr, Assets);
		}
		return true;
	}
}

FSoftObjectPath UAssetRegistryImpl::GetRedirectedObjectPath(const FSoftObjectPath& ObjectPath)
{
	if (IsSearchAllAssets())
	{
		FReadScopeLock InterfaceScopeLock(InterfaceLock);
		return GuardedData.GetRedirectedObjectPath(ObjectPath, nullptr, nullptr, /*bNeedsScanning*/ false);
	}

	FSoftObjectPath RedirectedObjectPath;
	UE::AssetRegistry::Impl::FEventContext EventContext;
	UE::AssetRegistry::Impl::FClassInheritanceContext InheritanceContext;
	UE::AssetRegistry::Impl::FClassInheritanceBuffer InheritanceBuffer;
	{
		LLM_SCOPE(ELLMTag::AssetRegistry);
		FWriteScopeLock WriteScopeLock(InterfaceLock);
		GetInheritanceContextWithRequiredLock(WriteScopeLock, InheritanceContext, InheritanceBuffer);
		RedirectedObjectPath = GuardedData.GetRedirectedObjectPath(ObjectPath, &EventContext, &InheritanceContext, /*bNeedsScanning*/ true);
	}	
	Broadcast(EventContext);

	return RedirectedObjectPath;
}

namespace UE::AssetRegistry
{

FSoftObjectPath FAssetRegistryImpl::GetRedirectedObjectPath(const FSoftObjectPath& ObjectPath, UE::AssetRegistry::Impl::FEventContext* EventContext, UE::AssetRegistry::Impl::FClassInheritanceContext* InheritanceContext, bool bNeedsScanning)
{
	check(!bNeedsScanning || (EventContext && InheritanceContext));

	FSoftObjectPath RedirectedPath = ObjectPath;

	// For legacy behavior, for the first object pointed to, we look up the object in memory
	// before checking the on-disk assets
	UObject* Asset = ObjectPath.ResolveObject();
	if (Asset)
	{
		RedirectedPath = FSoftObjectPath(Asset);
		UObjectRedirector* Redirector = Cast<UObjectRedirector>(Asset);
		if (!Redirector || !Redirector->DestinationObject)
		{
			return RedirectedPath;
		}
		// For legacy behavior, for all redirects after the initial request, we only check on-disk assets
		RedirectedPath = FSoftObjectPath(Redirector->DestinationObject);
	}

	if (bNeedsScanning)
	{
		UE::AssetRegistry::Impl::FScanPathContext Context(*EventContext, *InheritanceContext, {}, { RedirectedPath.ToString() });
		ScanPathsSynchronous(Context);
	}

	FString SubPathString;
	const FAssetData* AssetData = State.GetAssetByObjectPath(RedirectedPath);
	if (!AssetData && RedirectedPath.IsSubobject())
	{
		// If we found no Asset because it is a subobject, then look for its toplevelobject's Asset
		SubPathString = RedirectedPath.GetSubPathString();
		RedirectedPath = FSoftObjectPath(RedirectedPath.GetAssetPath(), FString());
		AssetData = State.GetAssetByObjectPath(RedirectedPath);
	}

	// Most of the time this will either not be a redirector or only have one redirect, so optimize for that case
	TArray<FSoftObjectPath, TInlineAllocator<2>> SeenPaths = { RedirectedPath };

	// Need to follow chain of redirectors
	while (AssetData && AssetData->IsRedirector())
	{
		FString Dest;

		if (!AssetData->GetTagValue(UE::AssetRegistry::Impl::DestinationObjectFName, Dest))
		{
			break;
		}
		
		// The FSoftObjectPath functions handle stripping class name if necessary
		RedirectedPath = Dest;

		if (SeenPaths.Contains(RedirectedPath))
		{
			// Recursive, bail
			break;
		}

		if (bNeedsScanning)
		{
			UE::AssetRegistry::Impl::FScanPathContext Context(*EventContext, *InheritanceContext, {}, { RedirectedPath.ToString() });
			ScanPathsSynchronous(Context);
		}

		SeenPaths.Add(RedirectedPath);
		AssetData = State.GetAssetByObjectPath(RedirectedPath);
	}

	if (!SubPathString.IsEmpty())
	{
		if (!RedirectedPath.IsSubobject())
		{
			RedirectedPath.SetSubPathString(SubPathString);
		}
		else
		{
			// A complicated case; the redirector pointed to a subobject. Append old subobject path onto the new one
			// Appending old to new will always use '.' because only the first subobject uses ':'
			RedirectedPath.SetSubPathString(RedirectedPath.GetSubPathString() + TEXT(".") + SubPathString);
		}
	}
	return RedirectedPath;
}
}

bool UAssetRegistryImpl::GetAncestorClassNames(FTopLevelAssetPath ClassName, TArray<FTopLevelAssetPath>& OutAncestorClassNames) const
{
	UE::AssetRegistry::Impl::FClassInheritanceContext InheritanceContext;
	UE::AssetRegistry::Impl::FClassInheritanceBuffer InheritanceBuffer;
	FRWScopeLock InterfaceScopeLock(InterfaceLock, SLT_ReadOnly);
	const_cast<UAssetRegistryImpl*>(this)->GetInheritanceContextWithRequiredLock(
		InterfaceScopeLock, InheritanceContext, InheritanceBuffer);
	return GuardedData.GetAncestorClassNames(InheritanceContext, ClassName, OutAncestorClassNames);
}

namespace UE::AssetRegistry
{

bool FAssetRegistryImpl::GetAncestorClassNames(Impl::FClassInheritanceContext& InheritanceContext, FTopLevelAssetPath ClassName,
	TArray<FTopLevelAssetPath>& OutAncestorClassNames) const
{
	// Assume we found the class unless there is an error
	bool bFoundClass = true;

	InheritanceContext.ConditionalUpdate();
	const TMap<FTopLevelAssetPath, FTopLevelAssetPath>& InheritanceMap = InheritanceContext.Buffer->InheritanceMap;

	// Make sure the requested class is in the inheritance map
	if (!InheritanceMap.Contains(ClassName))
	{
		bFoundClass = false;
	}
	else
	{
		// Now follow the map pairs until we cant find any more parents
		const FTopLevelAssetPath* CurrentClassName = &ClassName;
		const uint32 MaxInheritanceDepth = 65536;
		uint32 CurrentInheritanceDepth = 0;
		while (CurrentInheritanceDepth < MaxInheritanceDepth && CurrentClassName != nullptr)
		{
			CurrentClassName = InheritanceMap.Find(*CurrentClassName);

			if (CurrentClassName)
			{
				if (CurrentClassName->IsNull())
				{
					// No parent, we are at the root
					CurrentClassName = nullptr;
				}
				else
				{
					OutAncestorClassNames.Add(*CurrentClassName);
				}
			}
			CurrentInheritanceDepth++;
		}

		if (CurrentInheritanceDepth == MaxInheritanceDepth)
		{
			UE_LOG(LogAssetRegistry, Error, TEXT("IsChildClass exceeded max inheritance depth. There is probably an infinite loop of parent classes."));
			bFoundClass = false;
		}
	}

	return bFoundClass;
}
}

void UAssetRegistryImpl::GetDerivedClassNames(const TArray<FTopLevelAssetPath>& ClassNames, const TSet<FTopLevelAssetPath>& ExcludedClassNames,
	TSet<FTopLevelAssetPath>& OutDerivedClassNames) const
{
	UE::AssetRegistry::Impl::FClassInheritanceContext InheritanceContext;
	UE::AssetRegistry::Impl::FClassInheritanceBuffer InheritanceBuffer;
	FRWScopeLock InterfaceScopeLock(InterfaceLock, SLT_ReadOnly);
	const_cast<UAssetRegistryImpl*>(this)->GetInheritanceContextWithRequiredLock(
		InterfaceScopeLock, InheritanceContext, InheritanceBuffer);
	GuardedData.GetSubClasses(InheritanceContext, ClassNames, ExcludedClassNames, OutDerivedClassNames);
}

void UAssetRegistryImpl::GetAllCachedPaths(TArray<FString>& OutPathList) const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	const FPathTree& CachedPathTree = GuardedData.GetCachedPathTree();
	OutPathList.Reserve(OutPathList.Num() + CachedPathTree.NumPaths());
	CachedPathTree.EnumerateAllPaths([&OutPathList](FName Path)
	{
		OutPathList.Emplace(Path.ToString());
		return true;
	});
}

void UAssetRegistryImpl::EnumerateAllCachedPaths(TFunctionRef<bool(FString)> Callback) const
{
	EnumerateAllCachedPaths([&Callback](FName Path)
	{
		return Callback(Path.ToString());
	});
}

void UAssetRegistryImpl::EnumerateAllCachedPaths(TFunctionRef<bool(FName)> Callback) const
{
	TArray<FName> FoundPaths;
	{
		FReadScopeLock InterfaceScopeLock(InterfaceLock);
		const FPathTree& CachedPathTree = GuardedData.GetCachedPathTree();
		FoundPaths.Reserve(CachedPathTree.NumPaths());
		CachedPathTree.EnumerateAllPaths([&FoundPaths](FName Path)
		{
			FoundPaths.Add(Path);
			return true;
		});
	}
	for (FName Path : FoundPaths)
	{
		if (!Callback(Path))
		{
			return;
		}
	}
}

void UAssetRegistryImpl::GetSubPaths(const FString& InBasePath, TArray<FString>& OutPathList, bool bInRecurse) const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	const FPathTree& CachedPathTree = GuardedData.GetCachedPathTree();
	CachedPathTree.EnumerateSubPaths(*InBasePath, [&OutPathList](FName Path)
	{
		OutPathList.Emplace(Path.ToString());
		return true;
	}, bInRecurse);
}

void UAssetRegistryImpl::GetSubPaths(const FName& InBasePath, TArray<FName>& OutPathList, bool bInRecurse) const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	const FPathTree& CachedPathTree = GuardedData.GetCachedPathTree();
	CachedPathTree.EnumerateSubPaths(InBasePath, [&OutPathList](FName Path)
	{
		OutPathList.Emplace(Path);
		return true;
	}, bInRecurse);
}

void UAssetRegistryImpl::EnumerateSubPaths(const FString& InBasePath, TFunctionRef<bool(FString)> Callback, bool bInRecurse) const
{
	TArray<FName, TInlineAllocator<64>> SubPaths;
	{
		FReadScopeLock InterfaceScopeLock(InterfaceLock);
		const FPathTree& CachedPathTree = GuardedData.GetCachedPathTree();
		CachedPathTree.EnumerateSubPaths(FName(*InBasePath), [&SubPaths](FName PathName)
		{
			SubPaths.Add(PathName);
			return true;
		}, bInRecurse);
	}
	for (FName PathName : SubPaths)
	{
		if (!Callback(PathName.ToString()))
		{
			break;
		}
	}
}

void UAssetRegistryImpl::EnumerateSubPaths(const FName InBasePath, TFunctionRef<bool(FName)> Callback, bool bInRecurse) const
{
	TArray<FName, TInlineAllocator<64>> SubPaths;
	{
		FReadScopeLock InterfaceScopeLock(InterfaceLock);
		const FPathTree& CachedPathTree = GuardedData.GetCachedPathTree();
		CachedPathTree.EnumerateSubPaths(InBasePath, [&SubPaths](FName PathName)
		{
			SubPaths.Add(PathName);
			return true;
		}, bInRecurse);
	}
	for (FName PathName : SubPaths)
	{
		if (!Callback(PathName))
		{
			break;
		}
	}
}

void UAssetRegistryImpl::RunAssetsThroughFilter(TArray<FAssetData>& AssetDataList, const FARFilter& Filter) const
{
	if (Filter.IsEmpty())
	{
		return;
	}
	FARCompiledFilter CompiledFilter;
	CompileFilter(Filter, CompiledFilter);
	UE::AssetRegistry::Utils::RunAssetsThroughFilter(AssetDataList, CompiledFilter, UE::AssetRegistry::Utils::EFilterMode::Inclusive);
}

void UAssetRegistryImpl::UseFilterToExcludeAssets(TArray<FAssetData>& AssetDataList, const FARFilter& Filter) const
{
	if (Filter.IsEmpty())
	{
		return;
	}
	FARCompiledFilter CompiledFilter;
	CompileFilter(Filter, CompiledFilter);
	UseFilterToExcludeAssets(AssetDataList, CompiledFilter);
}

void UAssetRegistryImpl::UseFilterToExcludeAssets(TArray<FAssetData>& AssetDataList, const FARCompiledFilter& CompiledFilter) const
{
	UE::AssetRegistry::Utils::RunAssetsThroughFilter(AssetDataList, CompiledFilter, UE::AssetRegistry::Utils::EFilterMode::Exclusive);
}

bool UAssetRegistryImpl::IsAssetIncludedByFilter(const FAssetData& AssetData, const FARCompiledFilter& Filter) const
{
	return UE::AssetRegistry::Utils::RunAssetThroughFilter(AssetData, Filter, UE::AssetRegistry::Utils::EFilterMode::Inclusive);
}

bool UAssetRegistryImpl::IsAssetExcludedByFilter(const FAssetData& AssetData, const FARCompiledFilter& Filter) const
{
	return UE::AssetRegistry::Utils::RunAssetThroughFilter(AssetData, Filter, UE::AssetRegistry::Utils::EFilterMode::Exclusive);
}

namespace UE::AssetRegistry::Utils
{

bool RunAssetThroughFilter(const FAssetData& AssetData, const FARCompiledFilter& Filter, const EFilterMode FilterMode)
{
	const bool bPassFilterValue = FilterMode == EFilterMode::Inclusive;
	if (Filter.IsEmpty())
	{
		return bPassFilterValue;
	}

	const bool bFilterResult = RunAssetThroughFilter_Unchecked(AssetData, Filter, bPassFilterValue);
	return bFilterResult == bPassFilterValue;
}

bool RunAssetThroughFilter_Unchecked(const FAssetData& AssetData, const FARCompiledFilter& Filter, const bool bPassFilterValue)
{
	// Package Names
	if (Filter.PackageNames.Num() > 0)
	{
		const bool bPassesPackageNames = Filter.PackageNames.Contains(AssetData.PackageName);
		if (bPassesPackageNames != bPassFilterValue)
		{
			return !bPassFilterValue;
		}
	}

	// Package Paths
	if (Filter.PackagePaths.Num() > 0)
	{
		const bool bPassesPackagePaths = Filter.PackagePaths.Contains(AssetData.PackagePath);
		if (bPassesPackagePaths != bPassFilterValue)
		{
			return !bPassFilterValue;
		}
	}

	// ObjectPaths
	if (Filter.SoftObjectPaths.Num() > 0)
	{
		const bool bPassesObjectPaths = Filter.SoftObjectPaths.Contains(AssetData.GetSoftObjectPath());
		if (bPassesObjectPaths != bPassFilterValue)
		{
			return !bPassFilterValue;
		}
	}

	// Classes
	if (Filter.ClassPaths.Num() > 0)
	{
		const bool bPassesClasses = Filter.ClassPaths.Contains(AssetData.AssetClassPath);
		if (bPassesClasses != bPassFilterValue)
		{
			return !bPassFilterValue;
		}
	}

	// Tags and values
	if (Filter.TagsAndValues.Num() > 0)
	{
		bool bPassesTags = false;
		for (const auto& TagsAndValuePair : Filter.TagsAndValues)
		{
			bPassesTags |= TagsAndValuePair.Value.IsSet()
				? AssetData.TagsAndValues.ContainsKeyValue(TagsAndValuePair.Key, TagsAndValuePair.Value.GetValue())
				: AssetData.TagsAndValues.Contains(TagsAndValuePair.Key);
			if (bPassesTags)
			{
				break;
			}
		}
		if (bPassesTags != bPassFilterValue)
		{
			return !bPassFilterValue;
		}
	}

	return bPassFilterValue;
}

void RunAssetsThroughFilter(TArray<FAssetData>& AssetDataList, const FARCompiledFilter& CompiledFilter, const EFilterMode FilterMode)
{
	if (!IsFilterValid(CompiledFilter))
	{
		return;
	}

	const int32 OriginalArrayCount = AssetDataList.Num();
	const bool bPassFilterValue = FilterMode == EFilterMode::Inclusive;

	AssetDataList.RemoveAll([&CompiledFilter, bPassFilterValue](const FAssetData& AssetData)
		{
			const bool bFilterResult = RunAssetThroughFilter_Unchecked(AssetData, CompiledFilter, bPassFilterValue);
			return bFilterResult != bPassFilterValue;
		});
	if (OriginalArrayCount > AssetDataList.Num())
	{
		AssetDataList.Shrink();
	}
}

}

void UAssetRegistryImpl::CompileFilter(const FARFilter& InFilter, FARCompiledFilter& OutCompiledFilter) const
{
	UE::AssetRegistry::Impl::FClassInheritanceContext InheritanceContext;
	UE::AssetRegistry::Impl::FClassInheritanceBuffer InheritanceBuffer;
	FRWScopeLock InterfaceScopeLock(InterfaceLock, SLT_ReadOnly);
	if (InFilter.bRecursiveClasses)
	{
		const_cast<UAssetRegistryImpl*>(this)->GetInheritanceContextWithRequiredLock(
			InterfaceScopeLock, InheritanceContext, InheritanceBuffer);
	}
	else
	{
		// CompileFilter takes an inheritance context, but only to handle filters with recursive classes
		// which we are not using here, so leave the InheritanceContext empty
	}
	GuardedData.CompileFilter(InheritanceContext, InFilter, OutCompiledFilter);
}

namespace UE::AssetRegistry
{

void FAssetRegistryImpl::CompileFilter(Impl::FClassInheritanceContext& InheritanceContext, const FARFilter& InFilter,
	FARCompiledFilter& OutCompiledFilter) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FAssetRegistryImpl::CompileFilter);

	OutCompiledFilter.Clear();
	OutCompiledFilter.PackageNames.Append(InFilter.PackageNames);
	OutCompiledFilter.PackagePaths.Reserve(InFilter.PackagePaths.Num());
	for (FName PackagePath : InFilter.PackagePaths)
	{
		OutCompiledFilter.PackagePaths.Add(FPathTree::NormalizePackagePath(PackagePath));
	}
	OutCompiledFilter.SoftObjectPaths.Append(InFilter.SoftObjectPaths);

#if WITH_EDITORONLY_DATA
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	OutCompiledFilter.SoftObjectPaths.Append(UE::SoftObjectPath::Private::ConvertObjectPathNames(InFilter.ObjectPaths));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (!ensureAlwaysMsgf(InFilter.ClassNames.Num() == 0, TEXT("Asset Registry Filter using ClassNames instead of ClassPaths. First class name: \"%s\""), *InFilter.ClassNames[0].ToString()))
	{
		OutCompiledFilter.ClassPaths.Reserve(InFilter.ClassNames.Num());
		for (FName ClassName : InFilter.ClassNames)
		{
			if (!ClassName.IsNone())
			{
				FTopLevelAssetPath ClassPathName = UClass::TryConvertShortTypeNameToPathName<UStruct>(ClassName.ToString(), ELogVerbosity::Warning, TEXT("Compiling Asset Registry Filter"));
				if (!ClassPathName.IsNull())
				{
					OutCompiledFilter.ClassPaths.Add(ClassPathName);
				}
				else
				{
					UE_LOG(LogAssetRegistry, Error, TEXT("Failed to resolve class path for short class name \"%s\" when compiling asset registry filter"), *ClassName.ToString());
				}
			}
		}
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	OutCompiledFilter.ClassPaths.Append(InFilter.ClassPaths);
	OutCompiledFilter.TagsAndValues = InFilter.TagsAndValues;
	OutCompiledFilter.bIncludeOnlyOnDiskAssets = InFilter.bIncludeOnlyOnDiskAssets;
	OutCompiledFilter.WithoutPackageFlags = InFilter.WithoutPackageFlags;
	OutCompiledFilter.WithPackageFlags = InFilter.WithPackageFlags;

	if (InFilter.bRecursivePaths)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FAssetRegistryImpl::CompileFilter::AddPaths);
		
		// Add the sub-paths of all the input paths to the expanded list
		for (const FName& PackagePath : InFilter.PackagePaths)
		{
			CachedPathTree.GetSubPaths(PackagePath, OutCompiledFilter.PackagePaths);
		}
	}

	if (InFilter.bRecursiveClasses)
	{
		// Add the sub-classes of all the input classes to the expanded list, excluding any that were requested
		if (InFilter.RecursiveClassPathsExclusionSet.Num() > 0 && InFilter.ClassPaths.Num() == 0)
		{
			TArray<FTopLevelAssetPath> ClassNamesObject;
			ClassNamesObject.Add(UObject::StaticClass()->GetClassPathName());

			GetSubClasses(InheritanceContext, ClassNamesObject, InFilter.RecursiveClassPathsExclusionSet, OutCompiledFilter.ClassPaths);
		}
		else
		{
			GetSubClasses(InheritanceContext, InFilter.ClassPaths, InFilter.RecursiveClassPathsExclusionSet, OutCompiledFilter.ClassPaths);
		}
	}
}

}

EAssetAvailability::Type UAssetRegistryImpl::GetAssetAvailability(const FAssetData& AssetData) const
{
	return UE::AssetRegistry::Utils::GetAssetAvailability(AssetData);
}

namespace UE::AssetRegistry::Utils
{

EAssetAvailability::Type GetAssetAvailability(const FAssetData& AssetData)
{
#if ENABLE_PLATFORM_CHUNK_INSTALL
	IPlatformChunkInstall* ChunkInstall = FPlatformMisc::GetPlatformChunkInstall();

	EChunkLocation::Type BestLocation = EChunkLocation::DoesNotExist;

	// check all chunks to see which has the best locality
	for (int32 PakchunkId : AssetData.GetChunkIDs())
	{
		EChunkLocation::Type ChunkLocation = ChunkInstall->GetPakchunkLocation(PakchunkId);

		// if we find one in the best location, early out
		if (ChunkLocation == EChunkLocation::BestLocation)
		{
			BestLocation = ChunkLocation;
			break;
		}

		if (ChunkLocation > BestLocation)
		{
			BestLocation = ChunkLocation;
		}
	}

	switch (BestLocation)
	{
	case EChunkLocation::LocalFast:
		return EAssetAvailability::LocalFast;
	case EChunkLocation::LocalSlow:
		return EAssetAvailability::LocalSlow;
	case EChunkLocation::NotAvailable:
		return EAssetAvailability::NotAvailable;
	case EChunkLocation::DoesNotExist:
		return EAssetAvailability::DoesNotExist;
	default:
		check(0);
		return EAssetAvailability::LocalFast;
	}
#else
	return EAssetAvailability::LocalFast;
#endif
}

}

float UAssetRegistryImpl::GetAssetAvailabilityProgress(const FAssetData& AssetData, EAssetAvailabilityProgressReportingType::Type ReportType) const
{
	return UE::AssetRegistry::Utils::GetAssetAvailabilityProgress(AssetData, ReportType);
}

namespace UE::AssetRegistry::Utils
{

float GetAssetAvailabilityProgress(const FAssetData& AssetData, EAssetAvailabilityProgressReportingType::Type ReportType)
{
	check(ReportType == EAssetAvailabilityProgressReportingType::PercentageComplete || ReportType == EAssetAvailabilityProgressReportingType::ETA);

#if ENABLE_PLATFORM_CHUNK_INSTALL
	IPlatformChunkInstall* ChunkInstall = FPlatformMisc::GetPlatformChunkInstall();
	EChunkProgressReportingType::Type ChunkReportType = GetChunkAvailabilityProgressType(ReportType);

	bool IsPercentageComplete = (ChunkReportType == EChunkProgressReportingType::PercentageComplete) ? true : false;

	float BestProgress = MAX_FLT;

	// check all chunks to see which has the best time remaining
	for (int32 PakchunkID : AssetData.GetChunkIDs())
	{
		float Progress = ChunkInstall->GetChunkProgress(PakchunkID, ChunkReportType);

		// need to flip percentage completes for the comparison
		if (IsPercentageComplete)
		{
			Progress = 100.0f - Progress;
		}

		if (Progress <= 0.0f)
		{
			BestProgress = 0.0f;
			break;
		}

		if (Progress < BestProgress)
		{
			BestProgress = Progress;
		}
	}

	// unflip percentage completes
	if (IsPercentageComplete)
	{
		BestProgress = 100.0f - BestProgress;
	}
	return BestProgress;

#else
	if (ReportType == EAssetAvailabilityProgressReportingType::PercentageComplete)
	{
		return 100.0f;
	}
	else
	{
		return 0.0f;
	}
#endif

}

}

bool UAssetRegistryImpl::GetAssetAvailabilityProgressTypeSupported(EAssetAvailabilityProgressReportingType::Type ReportType) const
{
	return UE::AssetRegistry::Utils::GetAssetAvailabilityProgressTypeSupported(ReportType);
}

namespace UE::AssetRegistry::Utils
{

bool GetAssetAvailabilityProgressTypeSupported(EAssetAvailabilityProgressReportingType::Type ReportType)
{
#if ENABLE_PLATFORM_CHUNK_INSTALL
	IPlatformChunkInstall* ChunkInstall = FPlatformMisc::GetPlatformChunkInstall();
	return ChunkInstall->GetProgressReportingTypeSupported(GetChunkAvailabilityProgressType(ReportType));
#else
	return true;
#endif
}

}

void UAssetRegistryImpl::PrioritizeAssetInstall(const FAssetData& AssetData) const
{
	UE::AssetRegistry::Utils::PrioritizeAssetInstall(AssetData);
}

namespace UE::AssetRegistry::Utils
{

void PrioritizeAssetInstall(const FAssetData& AssetData)
{
#if ENABLE_PLATFORM_CHUNK_INSTALL
	IPlatformChunkInstall* ChunkInstall = FPlatformMisc::GetPlatformChunkInstall();

	const TConstArrayView<int32> ChunkIDs = AssetData.GetChunkIDs();
	if (ChunkIDs.Num() == 0)
	{
		return;
	}

	ChunkInstall->PrioritizePakchunk(ChunkIDs[0], EChunkPriority::Immediate);
#endif
}

}

bool UAssetRegistryImpl::HasVerseFiles(FName PackagePath, bool bRecursive /*= false*/) const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	return GuardedData.GetVerseFilesByPath(PackagePath, /*OutFilePaths=*/nullptr, bRecursive);
}

bool UAssetRegistryImpl::GetVerseFilesByPath(FName PackagePath, TArray<FName>& OutFilePaths, bool bRecursive /*= false*/) const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	return GuardedData.GetVerseFilesByPath(PackagePath, &OutFilePaths, bRecursive);
}

namespace UE::AssetRegistry
{

bool FAssetRegistryImpl::GetVerseFilesByPath(FName PackagePath, TArray<FName>* OutFilePaths, bool bRecursive) const
{
	TSet<FName> PathList;
	PathList.Reserve(32);
	PathList.Add(PackagePath);
	if (bRecursive)
	{
		CachedPathTree.GetSubPaths(PackagePath, PathList, true);
	}

	bool bFoundAnything = false;
	for (const FName& PathName : PathList)
	{
		const TArray<FName>* FilePaths = CachedVerseFilesByPath.Find(PathName);
		if (FilePaths)
		{
			bFoundAnything = true;
			if (OutFilePaths)
			{
				OutFilePaths->Append(*FilePaths);
			}
			else
			{
				break;
			}
		}
	}
	return bFoundAnything;
}

}

bool UAssetRegistryImpl::AddPath(const FString& PathToAdd)
{
	UE::AssetRegistry::Impl::FEventContext EventContext;
	bool bResult;
	{
		LLM_SCOPE(ELLMTag::AssetRegistry);
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		bResult = GuardedData.AddPath(EventContext, PathToAdd);
	}
	Broadcast(EventContext);
	return bResult;
}

namespace UE::AssetRegistry
{

bool FAssetRegistryImpl::AddPath(Impl::FEventContext& EventContext, FStringView PathToAdd)
{
	bool bIsDenied = false;
	// If no GlobalGatherer, then we are in the game or non-cook commandlet and we do not implement deny listing
	if (GlobalGatherer.IsValid())
	{
		FString LocalPathToAdd;
		if (FPackageName::TryConvertLongPackageNameToFilename(PathToAdd, LocalPathToAdd))
		{
			bIsDenied = GlobalGatherer->IsOnDenyList(LocalPathToAdd);
		}
	}
	if (bIsDenied)
	{
		return false;
	}
	return AddAssetPath(EventContext, FName(PathToAdd));
}

}

bool UAssetRegistryImpl::RemovePath(const FString& PathToRemove)
{
	UE::AssetRegistry::Impl::FEventContext EventContext;
	bool bResult;
	{
		LLM_SCOPE(ELLMTag::AssetRegistry);
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		bResult = GuardedData.RemoveAssetPath(EventContext, FName(*PathToRemove));
	}
	Broadcast(EventContext);
	return bResult;
}

bool UAssetRegistryImpl::PathExists(const FString& PathToTest) const
{
	return PathExists(FName(*PathToTest));
}

bool UAssetRegistryImpl::PathExists(const FName PathToTest) const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	return GuardedData.GetCachedPathTree().PathExists(PathToTest);
}

void UAssetRegistryImpl::ScanPathsSynchronous(const TArray<FString>& InPaths, bool bForceRescan,
	bool bIgnoreDenyListScanFilters)
{
	// The contract of this older version of ScanSynchronous always set the WaitForInMemoryObjects flag.
	UE::AssetRegistry::EScanFlags ScanFlags = UE::AssetRegistry::EScanFlags::WaitForInMemoryObjects;

	if (bForceRescan)
	{
		ScanFlags |= UE::AssetRegistry::EScanFlags::ForceRescan;
	}

	if (bIgnoreDenyListScanFilters)
	{
		ScanFlags |= UE::AssetRegistry::EScanFlags::IgnoreDenyListScanFilters;
	}

	ScanPathsSynchronousInternal(InPaths, TArray<FString>(), ScanFlags);
}

void UAssetRegistryImpl::ScanFilesSynchronous(const TArray<FString>& InFilePaths, bool bForceRescan)
{
	// The contract of this older version of ScanSynchronous always set the WaitForInMemoryObjects flag.
	UE::AssetRegistry::EScanFlags ScanFlags = UE::AssetRegistry::EScanFlags::WaitForInMemoryObjects;

	if (bForceRescan)
	{
		ScanFlags |= UE::AssetRegistry::EScanFlags::ForceRescan;
	}

	ScanPathsSynchronousInternal(TArray<FString>(), InFilePaths, ScanFlags);
}

void UAssetRegistryImpl::ScanSynchronous(const TArray<FString>& InPaths, const TArray<FString>& InFilePaths, UE::AssetRegistry::EScanFlags InScanFlags)
{
	ScanPathsSynchronousInternal(InPaths, InFilePaths, InScanFlags);
}

void UAssetRegistryImpl::ScanPathsSynchronousInternal(const TArray<FString>& InDirs, const TArray<FString>& InFiles,
	UE::AssetRegistry::EScanFlags InScanFlags)
{
	UE_SCOPED_IO_ACTIVITY(*WriteToString<256>("Scan Paths"));

	TRACE_CPUPROFILER_EVENT_SCOPE(UAssetRegistryImpl::ScanPathsSynchronousInternal);
	UE_TRACK_REFERENCING_OPNAME_SCOPED(PackageAccessTrackingOps::NAME_ResetContext);
	const double SearchStartTime = FPlatformTime::Seconds();

	const bool bForceRescan = !!(InScanFlags & UE::AssetRegistry::EScanFlags::ForceRescan);
	const bool bIgnoreDenyListScanFilters = !!(InScanFlags & UE::AssetRegistry::EScanFlags::IgnoreDenyListScanFilters);
	const bool bWaitForInMemoryObjects = !!(InScanFlags & UE::AssetRegistry::EScanFlags::WaitForInMemoryObjects);

	UE::AssetRegistry::Impl::FEventContext EventContext;
	UE::AssetRegistry::Impl::FClassInheritanceContext InheritanceContext;
	UE::AssetRegistry::Impl::FClassInheritanceBuffer InheritanceBuffer;
	UE::AssetRegistry::Impl::FScanPathContext Context(EventContext, InheritanceContext, InDirs, InFiles,
		bForceRescan, bIgnoreDenyListScanFilters, nullptr /* OutFindAssets */);

	bool bInitialSearchStarted;
	bool bInitialSearchCompleted;
	{
		LLM_SCOPE(ELLMTag::AssetRegistry);
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		GetInheritanceContextWithRequiredLock(InterfaceScopeLock, InheritanceContext, InheritanceBuffer);

		bInitialSearchStarted = GuardedData.IsInitialSearchStarted();
		bInitialSearchCompleted = GuardedData.IsInitialSearchCompleted();
		// make sure any outstanding async preload is complete
		GuardedData.ConditionalLoadPremadeAssetRegistry(*this, EventContext, InterfaceScopeLock);
		GuardedData.ScanPathsSynchronous(Context);
	}
	if (Context.LocalPaths.IsEmpty())
	{
		return;
	}

#if WITH_EDITOR
	if (bWaitForInMemoryObjects)
	{
		ProcessLoadedAssetsToUpdateCache(EventContext, -1., Context.Status);
	}
#endif
	Broadcast(EventContext);

	// Log stats
	FString PathsString;
	if (Context.LocalPaths.Num() > 1)
	{
		PathsString = FString::Printf(TEXT("'%s' and %d other paths"), *Context.LocalPaths[0], Context.LocalPaths.Num() - 1);
	}
	else
	{
		PathsString = FString::Printf(TEXT("'%s'"), *Context.LocalPaths[0]);
	}


	double Duration = FPlatformTime::Seconds() - SearchStartTime;
	UE::Telemetry::AssetRegistry::FSynchronousScanTelemetry Telemetry; 
	Telemetry.Directories = MakeArrayView(InDirs);
	Telemetry.Files = MakeArrayView(InFiles);
	Telemetry.Flags = InScanFlags;
	Telemetry.NumFoundAssets = Context.NumFoundAssets;
	Telemetry.Duration = Duration;
	Telemetry.bInitialSearchStarted = bInitialSearchStarted;
	Telemetry.bInitialSearchCompleted = bInitialSearchCompleted;
	FTelemetryRouter::Get().ProvideTelemetry(Telemetry);
	UE_LOG(LogAssetRegistry, Verbose, TEXT("ScanPathsSynchronous completed scanning %s to find %d assets in %0.4f seconds"), *PathsString,
		Context.NumFoundAssets, Duration);
}

void UAssetRegistryImpl::PrioritizeSearchPath(const FString& PathToPrioritize)
{
	LLM_SCOPE(ELLMTag::AssetRegistry);
	FWriteScopeLock InterfaceScopeLock(InterfaceLock);
	GuardedData.PrioritizeSearchPath(PathToPrioritize);
}

namespace UE::AssetRegistry
{

void FAssetRegistryImpl::PrioritizeSearchPath(const FString& PathToPrioritize)
{
	if (!GlobalGatherer.IsValid())
	{
		return;
	}
	GlobalGatherer->PrioritizeSearchPath(PathToPrioritize);
}

}

void UAssetRegistryImpl::AssetCreated(UObject* NewAsset)
{
	if (ensure(NewAsset) && NewAsset->IsAsset())
	{
		// Add the newly created object to the package file cache because its filename can already be
		// determined by its long package name.
		// @todo AssetRegistry We are assuming it will be saved in a single asset package.
		UPackage* NewPackage = NewAsset->GetOutermost();

		// Mark this package as newly created.
		NewPackage->SetPackageFlags(PKG_NewlyCreated);

		const FString NewPackageName = NewPackage->GetName();

		bool bShouldSkipAssset;
		UE::AssetRegistry::Impl::FEventContext EventContext;
		{
			LLM_SCOPE(ELLMTag::AssetRegistry);
			FWriteScopeLock InterfaceScopeLock(InterfaceLock);
			// If this package was marked as an empty package before, it is no longer empty, so remove it from the list
			GuardedData.RemoveEmptyPackage(NewPackage->GetFName());

			// Add the path to the Path Tree, in case it wasn't already there
			GuardedData.AddAssetPath(EventContext, *FPackageName::GetLongPackagePath(NewPackageName));
			bShouldSkipAssset = GuardedData.ShouldSkipAsset(NewAsset);
		}

		Broadcast(EventContext);
		if (!bShouldSkipAssset)
		{
			checkf(IsInGameThread(), TEXT("AssetCreated is not yet implemented as callable from other threads"));
			// Let subscribers know that the new asset was added to the registry
			FAssetData AssetData = FAssetData(NewAsset, FAssetData::ECreationFlags::AllowBlueprintClass,
				EAssetRegistryTagsCaller::AssetRegistryQuery);
			AssetAddedEvent.Broadcast(AssetData);
			OnAssetsAdded().Broadcast({ AssetData });

			// Notify listeners that an asset was just created
			InMemoryAssetCreatedEvent.Broadcast(NewAsset);
		}
	}
}

void UAssetRegistryImpl::AssetDeleted(UObject* DeletedAsset)
{
	checkf(GIsEditor, TEXT("Updating the AssetRegistry is only available in editor"));
	if (ensure(DeletedAsset) && DeletedAsset->IsAsset())
	{
		UPackage* DeletedObjectPackage = DeletedAsset->GetOutermost();
		bool bIsEmptyPackage = DeletedObjectPackage != nullptr && UPackage::IsEmptyPackage(DeletedObjectPackage, DeletedAsset);
		bool bInitialSearchCompleted = false;

		bool bShouldSkipAsset;
		{
			LLM_SCOPE(ELLMTag::AssetRegistry);
			FWriteScopeLock InterfaceScopeLock(InterfaceLock);

			// Deleting the last asset in a package causes the package to be garbage collected.
			// If the UPackage object is GCed, it will be considered 'Unloaded' which will cause it to
			// be fully loaded from disk when save is invoked.
			// We want to keep the package around so we can save it empty or delete the file.
			if (bIsEmptyPackage)
			{
				GuardedData.AddEmptyPackage(DeletedObjectPackage->GetFName());

				// If there is a package metadata object, clear the standalone flag so the package can be truly emptied upon GC
				if (UMetaData* MetaData = DeletedObjectPackage->GetMetaData())
				{
					MetaData->ClearFlags(RF_Standalone);
				}
			}
			bInitialSearchCompleted = GuardedData.IsInitialSearchCompleted();
			bShouldSkipAsset = GuardedData.ShouldSkipAsset(DeletedAsset);
		}

#if WITH_EDITOR
		if (bInitialSearchCompleted && FAssetData::IsRedirector(DeletedAsset))

		{
			// Need to remove from GRedirectCollector
			GRedirectCollector.RemoveAssetPathRedirection(FSoftObjectPath(DeletedAsset));
		}
#endif

		if (!bShouldSkipAsset)
		{
			FAssetData AssetDataDeleted = FAssetData(DeletedAsset, FAssetData::ECreationFlags::AllowBlueprintClass,
				EAssetRegistryTagsCaller::AssetRegistryQuery);

			checkf(IsInGameThread(), TEXT("AssetDeleted is not yet implemented as callable from other threads"));
			// Let subscribers know that the asset was removed from the registry
			AssetRemovedEvent.Broadcast(AssetDataDeleted);
			OnAssetsRemoved().Broadcast({AssetDataDeleted});

			// Notify listeners that an in-memory asset was just deleted
			InMemoryAssetDeletedEvent.Broadcast(DeletedAsset);
		}
	}
}

void UAssetRegistryImpl::AssetRenamed(const UObject* RenamedAsset, const FString& OldObjectPath)
{
	checkf(GIsEditor, TEXT("Updating the AssetRegistry is only available in editor"));
	if (ensure(RenamedAsset) && RenamedAsset->IsAsset())
	{
		// Add the renamed object to the package file cache because its filename can already be
		// determined by its long package name.
		// @todo AssetRegistry We are assuming it will be saved in a single asset package.
		UPackage* NewPackage = RenamedAsset->GetOutermost();
		const FString NewPackageName = NewPackage->GetName();
		const FString Filename = FPackageName::LongPackageNameToFilename(NewPackageName, FPackageName::GetAssetPackageExtension());

		// We want to keep track of empty packages so we can properly merge cached assets with in-memory assets
		UPackage* OldPackage = nullptr;
		FString OldPackageName;
		FString OldAssetName;
		if (OldObjectPath.Split(TEXT("."), &OldPackageName, &OldAssetName))
		{
			OldPackage = FindPackage(nullptr, *OldPackageName);
		}

		// Call IsEmptyPackage outside of the lock; it can call LoadPackage internally.
		bool bOldPackageIsEmpty = OldPackage && UPackage::IsEmptyPackage(OldPackage);

		bool bShouldSkipAsset;
		UE::AssetRegistry::Impl::FEventContext EventContext;
		{
			LLM_SCOPE(ELLMTag::AssetRegistry);
			FWriteScopeLock InterfaceScopeLock(InterfaceLock);
			GuardedData.RemoveEmptyPackage(NewPackage->GetFName());

			if (bOldPackageIsEmpty)
			{
				GuardedData.AddEmptyPackage(OldPackage->GetFName());
			}

			// Add the path to the Path Tree, in case it wasn't already there
			GuardedData.AddAssetPath(EventContext, *FPackageName::GetLongPackagePath(NewPackageName));
			bShouldSkipAsset = GuardedData.ShouldSkipAsset(RenamedAsset);
		}

		Broadcast(EventContext);
		if (!bShouldSkipAsset)
		{
			checkf(IsInGameThread(), TEXT("AssetRenamed is not yet implemented as callable from other threads"));
			AssetRenamedEvent.Broadcast(
				FAssetData(RenamedAsset, FAssetData::ECreationFlags::AllowBlueprintClass,
					EAssetRegistryTagsCaller::AssetRegistryQuery),
				OldObjectPath
			);
		}
	}
}

void UAssetRegistryImpl::AssetSaved(const UObject& SavedAsset)
{
	AssetUpdateTags(const_cast<UObject*>(&SavedAsset), EAssetRegistryTagsCaller::Fast);
}

void UAssetRegistryImpl::AssetsSaved(TArray<FAssetData>&& Assets)
{
#if WITH_EDITOR
	UE::AssetRegistry::Impl::FEventContext EventContext;
	{
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		GuardedData.AssetsSaved(EventContext, MoveTemp(Assets));
	}
	Broadcast(EventContext);
#endif
}

void UAssetRegistryImpl::AssetFullyUpdateTags(UObject* Object)
{
	AssetUpdateTags(Object, EAssetRegistryTagsCaller::Fast);
}

void UAssetRegistryImpl::AssetUpdateTags(UObject* Object, EAssetRegistryTagsCaller Caller)
{
#if WITH_EDITOR
	FAssetData AssetData(Object, FAssetData::ECreationFlags::None, Caller);
	TArray<FAssetData> Assets;
	Assets.Add(MoveTemp(AssetData));

	UE::AssetRegistry::Impl::FEventContext EventContext;
	{
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		GuardedData.AssetsSaved(EventContext, MoveTemp(Assets));
	}
	Broadcast(EventContext);
#endif
}

namespace UE::AssetRegistry
{

#if WITH_EDITOR
void FAssetRegistryImpl::AssetsSaved(UE::AssetRegistry::Impl::FEventContext& EventContext, TArray<FAssetData>&& Assets)
{
	LLM_SCOPE(ELLMTag::AssetRegistry);
	for (FAssetData& NewAssetData : Assets)
	{
		FCachedAssetKey Key(NewAssetData);
		FAssetData** DataFromGather = State.CachedAssets.Find(Key);

		AssetDataObjectPathsUpdatedOnLoad.Add(NewAssetData.GetSoftObjectPath());

		if (!DataFromGather)
		{
			FAssetData* ClonedAssetData = new FAssetData(MoveTemp(NewAssetData));
			AddAssetData(EventContext, ClonedAssetData);
		}
		else
		{
			UpdateAssetData(EventContext, *DataFromGather, MoveTemp(NewAssetData), false /* bKeepDeletedTags */);
		}
	}
}
#endif

}

void UAssetRegistryImpl::AssetTagsFinalized(const UObject& FinalizedAsset)
{
#if WITH_EDITOR
	if (!FinalizedAsset.IsAsset())
	{
		return;
	}
	LLM_SCOPE(ELLMTag::AssetRegistry);

	FWriteScopeLock InterfaceScopeLock(InterfaceLock);
	GuardedData.AddLoadedAssetToProcess(FinalizedAsset);
#endif
}

bool UAssetRegistryImpl::VerseCreated(const FString& FilePathOnDisk)
{
	checkf(GIsEditor, TEXT("Updating the AssetRegistry is only available in editor"));
	if (!FAssetDataGatherer::IsVerseFile(FilePathOnDisk))
	{
		return false;
	}

	FString PackageName;
	if (!FPackageName::TryConvertFilenameToLongPackageName(FilePathOnDisk, PackageName))
	{
		return false;
	}

	FNameBuilder VersePackagePathName;
	VersePackagePathName.Append(PackageName);
	VersePackagePathName.Append(FPathViews::GetExtension(FilePathOnDisk, /*bIncludeDot=*/true));

	UE::AssetRegistry::Impl::FEventContext EventContext;
	{
		LLM_SCOPE(ELLMTag::AssetRegistry);
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		GuardedData.AddVerseFile(EventContext, *VersePackagePathName);
	}
	Broadcast(EventContext);

	return true;
}

bool UAssetRegistryImpl::VerseDeleted(const FString& FilePathOnDisk)
{
	checkf(GIsEditor, TEXT("Updating the AssetRegistry is only available in editor"));
	if (!FAssetDataGatherer::IsVerseFile(FilePathOnDisk))
	{
		return false;
	}

	FString PackageName;
	if (!FPackageName::TryConvertFilenameToLongPackageName(FilePathOnDisk, PackageName))
	{
		return false;
	}

	FNameBuilder VersePackagePathName;
	VersePackagePathName.Append(PackageName);
	VersePackagePathName.Append(FPathViews::GetExtension(FilePathOnDisk, /*bIncludeDot=*/true));

	UE::AssetRegistry::Impl::FEventContext EventContext;
	{
		LLM_SCOPE(ELLMTag::AssetRegistry);
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		GuardedData.RemoveVerseFile(EventContext, *VersePackagePathName);
	}
	Broadcast(EventContext);

	return true;
}

void UAssetRegistryImpl::PackageDeleted(UPackage* DeletedPackage)
{
	checkf(GIsEditor, TEXT("Updating the AssetRegistry is only available in editor"));
	UE::AssetRegistry::Impl::FEventContext EventContext;
	if (ensure(DeletedPackage))
	{
		LLM_SCOPE(ELLMTag::AssetRegistry);
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		GuardedData.RemovePackageData(EventContext, DeletedPackage->GetFName());
	}
	Broadcast(EventContext);
}

bool UAssetRegistryImpl::IsLoadingAssets() const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	return GuardedData.IsLoadingAssets();
}

namespace UE::AssetRegistry
{

bool FAssetRegistryImpl::IsLoadingAssets() const
{
	return !bInitialSearchCompleted;
}

}

void UAssetRegistryImpl::Tick(float DeltaTime)
{
	checkf(IsInGameThread(), TEXT("The tick function executes deferred loads and events and must be on the game thread to do so."));
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("Asset Registry Tick");

	UE::AssetRegistry::Impl::EGatherStatus Status = UE::AssetRegistry::Impl::EGatherStatus::Active;
	double TickStartTime = -1; // Force a full flush if DeltaTime < 0
	if (DeltaTime >= 0)
	{
		TickStartTime = FPlatformTime::Seconds();
	}


	bool bInterrupted;
	do
	{
		bInterrupted = false;
		UE::AssetRegistry::Impl::FEventContext EventContext;
		{
			LLM_SCOPE(ELLMTag::AssetRegistry);
			FWriteScopeLock InterfaceScopeLock(InterfaceLock);
			UE::AssetRegistry::Impl::FClassInheritanceContext InheritanceContext;
			UE::AssetRegistry::Impl::FClassInheritanceBuffer InheritanceBuffer;
			GetInheritanceContextWithRequiredLock(InterfaceScopeLock, InheritanceContext, InheritanceBuffer);

			// Process any deferred events
			EventContext = MoveTemp(DeferredEvents);
			DeferredEvents.Clear();
			if (EventContext.IsEmpty())
			{
				// Tick the Gatherer
				Status = GuardedData.TickGatherer(EventContext, InheritanceContext, TickStartTime, bInterrupted);
			}
			else
			{
				// Skip the TickGather to deal with the DeferredEvents first
				bInterrupted = true;
			}
		}

#if WITH_EDITOR
		if (!bInterrupted)
		{
			ProcessLoadedAssetsToUpdateCache(EventContext, TickStartTime, Status);
		}
#endif
		Broadcast(EventContext);
	} while ((bInterrupted || Status == UE::AssetRegistry::Impl::EGatherStatus::WaitingForEvents) &&
		(TickStartTime < 0 || (FPlatformTime::Seconds() - TickStartTime) <= UE::AssetRegistry::Impl::MaxSecondsPerFrame));
}

namespace UE::AssetRegistry
{

void FAssetRegistryImpl::WaitForGathererIdleIfSynchronous()
{
	if (GlobalGatherer && GlobalGatherer->IsSynchronous())
	{
		GlobalGatherer->WaitForIdle();
	}
}

void FAssetRegistryImpl::WaitForGathererIdle(float TimeoutSeconds)
{
	if (GlobalGatherer)
	{
		GlobalGatherer->WaitForIdle(TimeoutSeconds);
	}
}

Impl::EGatherStatus FAssetRegistryImpl::TickGatherer(Impl::FEventContext& EventContext,
	Impl::FClassInheritanceContext& InheritanceContext, const double TickStartTime, bool& bOutInterrupted,
	TOptional<FAssetsFoundCallback> AssetsFoundCallback)
{
	using namespace UE::AssetRegistry::Impl;

	EGatherStatus OutStatus = EGatherStatus::Complete;
	bOutInterrupted = false;
	if (!GlobalGatherer.IsValid())
	{
		return OutStatus;
	}
	double TimingStartTime = -1.;
	auto LazyStartTimer = [&TimingStartTime]()
	{
		if (TimingStartTime <= 0.)
		{
			TimingStartTime = FPlatformTime::Seconds();
		}
	};
	auto RecordTimer = [this, &TimingStartTime]()
	{
		if (TimingStartTime > 0.)
		{
			StoreGatherResultsTimeSeconds += static_cast<float>(FPlatformTime::Seconds() - TimingStartTime);
			TimingStartTime = -1.;
		}
	};
	ON_SCOPE_EXIT
	{
		RecordTimer();
	};

	// Gather results from the background search
	FAssetDataGatherer::FResultContext ResultContext;
	GlobalGatherer->GetAndTrimSearchResults(BackgroundResults, ResultContext);	
	// Report the search times
	for (double SearchTime : ResultContext.SearchTimes)
	{
		UE_LOG(LogAssetRegistry, Verbose, TEXT("### Background search completed in %0.4f seconds"), SearchTime);
	}
	const bool bHadAssetsToProcess = BackgroundResults.Assets.Num() > 0 || BackgroundResults.Dependencies.Num() > 0;
	auto GetNumGatherFromDiskPending = [&ResultContext, this]()
	{
		return ResultContext.NumFilesToSearch + ResultContext.NumPathsToSearch + BackgroundResults.Paths.Num() + BackgroundResults.Assets.Num()
			+ BackgroundResults.Dependencies.Num() + BackgroundResults.CookedPackageNamesWithoutAssetData.Num();
	};
	int32 NumPending = 0;
	auto CalculateStatus =
		[bHadAssetsToProcess, &NumPending, &ResultContext, &EventContext, &OutStatus, this]
		(int32 NumGatherPending, bool bInterrupted)
	{
		// Compute total pending, plus highest pending for this run so we can show a good progress bar
		NumPending = NumGatherPending
#if WITH_EDITOR
			+ (PackagesNeedingDependencyCalculation.Num() ? 1 : 0)
#endif
			;
		HighestPending = FMath::Max(this->HighestPending, NumPending);

		if (!bInterrupted && !ResultContext.bIsSearching && NumPending == 0)
		{
			OutStatus = EGatherStatus::Complete;
		}
		else if (!bInterrupted && !ResultContext.bAbleToProgress)
		{
			OutStatus = EGatherStatus::UnableToProgress;
		}
		else
		{
			OutStatus = EGatherStatus::Active;
		}
	};
	auto UpdateStatus = [bHadAssetsToProcess, &NumPending, &ResultContext, &EventContext, &OutStatus, this]()
	{
		// Notify the status change, only when something changed, or when sending the final result before going idle
		if (ResultContext.bIsSearching || bHadAssetsToProcess ||
			(OutStatus == EGatherStatus::Complete && this->GatherStatus != EGatherStatus::Complete))
		{
			EventContext.ProgressUpdateData.Emplace(
				HighestPending,					// NumTotalAssets
				HighestPending - NumPending,	// NumAssetsProcessedByAssetRegistry
				NumPending / 2,					// NumAssetsPendingDataLoad, divided by 2 because assets are double counted due to dependencies
				ResultContext.bIsDiscoveringFiles // bIsDiscoveringAssetFiles
			);
		}
		this->GatherStatus = OutStatus;
	};

	// Add discovered paths
	if (BackgroundResults.Paths.Num())
	{
		LazyStartTimer();
		PathDataGathered(EventContext, TickStartTime, BackgroundResults.Paths);
	}

	// Process the asset results
	if (BackgroundResults.Assets.Num())
	{
		LazyStartTimer();
		// Mark the first amortize time
		if (AssetsFoundCallback.IsSet())
		{
			AssetsFoundCallback.GetValue()(BackgroundResults.Assets);
		}

		AssetSearchDataGathered(EventContext, TickStartTime, BackgroundResults.Assets);
	}

	// Add dependencies
	if (BackgroundResults.Dependencies.Num())
	{
		LazyStartTimer();
		DependencyDataGathered(TickStartTime, BackgroundResults.Dependencies);
	}

	// Load cooked packages that do not have asset data
	if (BackgroundResults.CookedPackageNamesWithoutAssetData.Num())
	{
		LazyStartTimer();
		CookedPackageNamesWithoutAssetDataGathered(EventContext, TickStartTime, BackgroundResults.CookedPackageNamesWithoutAssetData, bOutInterrupted);
		if (bOutInterrupted)
		{
			CalculateStatus(GetNumGatherFromDiskPending(), true /* bInterrupted */);
			UpdateStatus();
			return OutStatus;
		}
	}

	// Add Verse files
	if (BackgroundResults.VerseFiles.Num())
	{
		LazyStartTimer();
		VerseFilesGathered(EventContext, TickStartTime, BackgroundResults.VerseFiles);
	}

	// Store blocked files to be reported
	if (BackgroundResults.BlockedFiles.Num())
	{
		EventContext.BlockedFiles.Append(MoveTemp(BackgroundResults.BlockedFiles));
		BackgroundResults.BlockedFiles.Reset();
	}

	// Load Calculated Dependencies when the gather from disk is complete; the full gather is not complete until after this is done
	int32 NumGatherFromDiskPending = GetNumGatherFromDiskPending();
#if WITH_EDITOR
	bool bDiskGatherComplete = !ResultContext.bIsSearching && NumGatherFromDiskPending == 0;
	if (bDiskGatherComplete && PackagesNeedingDependencyCalculation.Num())
	{
		LazyStartTimer();
		LoadCalculatedDependencies(nullptr, TickStartTime, InheritanceContext, bOutInterrupted);
		if (bOutInterrupted)
		{
			CalculateStatus(NumGatherFromDiskPending, true /* bInterrupted */);
			UpdateStatus();
			return OutStatus;
		}
	}
#endif

	CalculateStatus(NumGatherFromDiskPending, false /* bInterrupted */);

	if (OutStatus == EGatherStatus::Complete)
	{
		if (!bInitialSearchCompleted)
		{
			// Finishing the background search is blocked until preloading complete because plugins can be mounted during
			// startup up until that point, and we need to wait for all the plugins to load before declaring completion.
			bool bCanCompleteInitialSearch = bPreloadingComplete && IsEngineStartupModuleLoadingComplete();

			if (bCanCompleteInitialSearch)
			{
				if (!EventContext.AssetEvents.IsEmpty())
				{
					// Don't mark the InitialSearch completed until we have sent all the AssetDataAdded events
					// that arose from the final tick of the gatherer. Some callers might do more expensive
					// work for assets added after the initial search completed, and we don't want them to do
					// that more expensive work on the last batch of assets before completion.
					OutStatus = EGatherStatus::WaitingForEvents;
					bCanCompleteInitialSearch = false;
				}
			}
			if (bCanCompleteInitialSearch)
			{
				RecordTimer(); // OnInitialSearchComplete reads data set by RecordTimer
				OnInitialSearchCompleted(EventContext);
			}
		}
	}

	UpdateStatus();
	if (OutStatus == EGatherStatus::Complete)
	{
		HighestPending = 0;
		BackgroundResults.Shrink();
	}

	return OutStatus;
}

void FAssetRegistryImpl::OnInitialSearchCompleted(Impl::FEventContext& EventContext)
{
	bInitialSearchCompleted = true;

#if WITH_EDITOR
	// update redirectors
	UpdateRedirectCollector();
#endif

	// Handle any deferred loading operations
	SetPerformanceMode(Impl::EPerformanceMode::MostlyStatic);

	LogSearchDiagnostics(InitialSearchStartTime);
	TRACE_END_REGION(TEXT("Asset Registry Scan"));

	GlobalGatherer->OnInitialSearchCompleted();

	EventContext.bFileLoadedEventBroadcast = true;
}

void FAssetRegistryImpl::LogSearchDiagnostics(double StartTime)
{
	FAssetGatherDiagnostics Diagnostics = GlobalGatherer->GetDiagnostics();
	float Total = Diagnostics.DiscoveryTimeSeconds + Diagnostics.GatherTimeSeconds + StoreGatherResultsTimeSeconds;
	UE::Telemetry::AssetRegistry::FGatherTelemetry Telemetry;
	Telemetry.TotalSearchDurationSeconds = FPlatformTime::Seconds() - StartTime;
	Telemetry.TotalWorkTimeSeconds = Total;
	Telemetry.DiscoveryTimeSeconds = Diagnostics.DiscoveryTimeSeconds;
	Telemetry.GatherTimeSeconds = Diagnostics.GatherTimeSeconds;
	Telemetry.StoreTimeSeconds = StoreGatherResultsTimeSeconds;
	Telemetry.NumCachedDirectories = Diagnostics.NumCachedDirectories;
	Telemetry.NumUncachedDirectories = Diagnostics.NumUncachedDirectories;
	Telemetry.NumCachedAssetFiles = Diagnostics.NumCachedAssetFiles;
	Telemetry.NumUncachedAssetFiles = Diagnostics.NumUncachedAssetFiles;
	FTelemetryRouter::Get().ProvideTelemetry(Telemetry);
	UE_LOG(LogAssetRegistry, Log, TEXT("AssetRegistryGather time %.4fs: AssetDataDiscovery %0.4fs, AssetDataGather %0.4fs, StoreResults %0.4fs.")
		TEXT("\n\tNumCachedDirectories %d. NumUncachedDirectories %d. NumCachedFiles %d. NumUncachedFiles %d."),
		Total, Diagnostics.DiscoveryTimeSeconds, Diagnostics.GatherTimeSeconds, StoreGatherResultsTimeSeconds,
		Diagnostics.NumCachedDirectories, Diagnostics.NumUncachedDirectories, Diagnostics.NumCachedAssetFiles,
		Diagnostics.NumUncachedAssetFiles);

#if !NO_LOGGING
	if (bVerboseLogging)
	{
		UE_LOG(LogAssetRegistry, Verbose, TEXT("TagMemoryUse:"));
		TagSizeByClass.ValueSort([](int64 A, int64 B) { return A > B; });
		constexpr int64 MinSizeToLog = 1000 * 1000;
		for (const TPair<FTopLevelAssetPath, int64>& Pair : TagSizeByClass)
		{
			if (Pair.Value < MinSizeToLog)
			{
				break;
			}
			UE_LOG(LogAssetRegistry, Verbose, TEXT("%s: %.1f MB"),
				*Pair.Key.ToString(), (float)Pair.Value / (1000.f * 1000.f));
		}
	}
#endif
}

void FAssetRegistryImpl::TickGatherPackage(Impl::FEventContext& EventContext, const FString& PackageName, const FString& LocalPath)
{
	if (!GlobalGatherer.IsValid())
	{
		return;
	}
	GlobalGatherer->WaitOnPath(LocalPath);
	double TimingStartTime = -1.;
	auto LazyStartTimer = [&TimingStartTime]()
	{
		if (TimingStartTime <= 0.)
		{
			TimingStartTime = FPlatformTime::Seconds();
		}
	};
	ON_SCOPE_EXIT
	{
		if (TimingStartTime > 0.)
		{
			StoreGatherResultsTimeSeconds += static_cast<float>(FPlatformTime::Seconds() - TimingStartTime);
			TimingStartTime = -1.;
		}
	};

	FName PackageFName(PackageName);

	// Gather results from the background search
	GlobalGatherer->GetPackageResults(BackgroundResults.Assets, BackgroundResults.Dependencies);

	TArray<FAssetData*> PackageAssets;
	TArray<FPackageDependencyData> PackageDependencyDatas;
	BackgroundResults.Assets.MultiFind(PackageFName, PackageAssets);
	BackgroundResults.Assets.Remove(PackageFName);
	BackgroundResults.Dependencies.MultiFind(PackageFName, PackageDependencyDatas);
	BackgroundResults.Dependencies.Remove(PackageFName);
	if (PackageAssets.Num() > 0)
	{
		LazyStartTimer();
		TMultiMap<FName, FAssetData*> PackageAssetsMap;
		PackageAssetsMap.Reserve(PackageAssets.Num());
		for (FAssetData* PackageAsset : PackageAssets)
		{
			PackageAssetsMap.Add(PackageFName, PackageAsset);
		}
		AssetSearchDataGathered(EventContext, -1., PackageAssetsMap);
	}
	if (PackageDependencyDatas.Num() > 0)
	{
		LazyStartTimer();
		TMultiMap<FName, FPackageDependencyData> PackageDependencyDatasMap;
		PackageDependencyDatasMap.Reserve(PackageDependencyDatas.Num());
		for (FPackageDependencyData& DependencyData : PackageDependencyDatas)
		{
			PackageDependencyDatasMap.Add(PackageFName, MoveTemp(DependencyData));
		}
		DependencyDataGathered(-1., PackageDependencyDatasMap);
	}
}

#if WITH_EDITOR
void FAssetRegistryImpl::LoadCalculatedDependencies(TArray<FName>* AssetPackageNamesToCalculate, double TickStartTime,
	Impl::FClassInheritanceContext& InheritanceContext, bool& bOutInterrupted)
{
	auto CheckForTimeUp = [&TickStartTime, &bOutInterrupted](bool bHadActivity)
	{
		// Only Check TimeUp when we found something to do, otherwise we waste time calling FPlatformTime::Seconds
		if (!bHadActivity)
		{
			return false;
		}
		if (TickStartTime >= 0 && (FPlatformTime::Seconds() - TickStartTime) >= UE::AssetRegistry::Impl::MaxSecondsPerFrame)
		{
			bOutInterrupted = true;
			return true;
		}
		return false;
	};

	RebuildAssetDependencyGathererMapIfNeeded();

	if (AssetPackageNamesToCalculate)
	{
		for (FName PackageName : *AssetPackageNamesToCalculate)
		{
			// We do not remove the package from PackagesNeedingDependencyCalculation, because
			// we are only calculating an interim result when AssetsToCalculate is non-null
			// We will run again on each of these PackageNames when TickGatherer finishes gathering all dependencies
			if (PackagesNeedingDependencyCalculation.Contains(PackageName))
			{
				bool bHadActivity;
				LoadCalculatedDependencies(PackageName, InheritanceContext, bHadActivity);
				if (CheckForTimeUp(bHadActivity))
				{
					return;
				}
			}
		}
	}
	else
	{
		for (TSet<FName>::TIterator It = PackagesNeedingDependencyCalculation.CreateIterator(); It; ++It)
		{
			bool bHadActivity;
			LoadCalculatedDependencies(*It, InheritanceContext, bHadActivity);
			It.RemoveCurrent();
			if (CheckForTimeUp(bHadActivity))
			{
				return;
			}
		}
		check(PackagesNeedingDependencyCalculation.IsEmpty());
	}
}

void FAssetRegistryImpl::LoadCalculatedDependencies(FName PackageName,
	Impl::FClassInheritanceContext& InheritanceContext, bool& bOutHadActivity)
{
	bOutHadActivity = false;
	
	auto GetCompiledFilter = [this, &InheritanceContext](const FARFilter& InFilter) -> FARCompiledFilter
	{
		FARCompiledFilter CompiledFilter;
		CompileFilter(InheritanceContext, InFilter, CompiledFilter);
		return CompiledFilter;
	};

	TArray<UE::AssetDependencyGatherer::Private::FRegisteredAssetDependencyGatherer*, TInlineAllocator<2>> Gatherers;
	for (const FAssetData* AssetData : State.GetAssetsByPackageName(PackageName))
	{
		Gatherers.Reset();

		// Check the class name instead of trying to load the actual class as that is slow
		// This code could be moved somewhere where it doesn't need to re-query the asset data, but it needs to happen after both dependencies and data are handled
		RegisteredDependencyGathererClasses.MultiFind(AssetData->AssetClassPath, Gatherers);
		for (UE::AssetDependencyGatherer::Private::FRegisteredAssetDependencyGatherer* Gatherer : Gatherers)
		{
			if (!bOutHadActivity)
			{
				RemoveDirectoryReferencer(PackageName);
			}

			bOutHadActivity = true;
			TArray<IAssetDependencyGatherer::FGathereredDependency> GatheredDependencies;
			TArray<FString> DirectoryReferences;

			Gatherer->GatherDependencies(*AssetData, State, GetCompiledFilter, GatheredDependencies, DirectoryReferences);
			
			if (GatheredDependencies.Num())
			{
				FDependsNode* SourceNode = State.CreateOrFindDependsNode(FAssetIdentifier(PackageName));

				for (const IAssetDependencyGatherer::FGathereredDependency& GatheredDepencency : GatheredDependencies)
				{
					FDependsNode* TargetNode = State.CreateOrFindDependsNode(FAssetIdentifier(GatheredDepencency.PackageName));
					EDependencyProperty DependencyProperties = GatheredDepencency.Property;
					SourceNode->AddDependency(TargetNode, EDependencyCategory::Package, DependencyProperties);
					TargetNode->AddReferencer(SourceNode);
				}
			}

			for (const FString& Directory : DirectoryReferences)
			{
				AddDirectoryReferencer(PackageName, Directory);
			}
		}
	}
}

void FAssetRegistryImpl::AddDirectoryReferencer(FName PackageName, const FString& DirectoryLocalPathOrLongPackageName)
{
	FString DirectoryLocalPath;
	if (!FPackageName::TryConvertLongPackageNameToFilename(DirectoryLocalPathOrLongPackageName, DirectoryLocalPath))
	{
		UE_LOG(LogAssetRegistry, Warning,
			TEXT("AddDirectoryReferencer called with LongPackageName %s that cannot be mapped to a local path. Ignoring it."),
			*DirectoryLocalPathOrLongPackageName);
		return;
	}
	FPaths::MakeStandardFilename(DirectoryLocalPath);
	DirectoryReferencers.AddUnique(DirectoryLocalPath, PackageName);
}

void FAssetRegistryImpl::RemoveDirectoryReferencer(FName PackageName)
{
	TArray<FString> FoundKeys;
	for (const TPair<FString, FName>& Pair : DirectoryReferencers)
	{
		if (Pair.Value == PackageName)
		{
			FoundKeys.Add(Pair.Key);
		}
	}
	for (FString& Key : FoundKeys)
	{
		DirectoryReferencers.Remove(Key, PackageName);
	}
}

#endif


}

void UAssetRegistryImpl::Serialize(FArchive& Ar)
{
	UE::AssetRegistry::Impl::FEventContext EventContext;
	{
		LLM_SCOPE(ELLMTag::AssetRegistry);
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		GuardedData.Serialize(Ar, EventContext);
	}
	Broadcast(EventContext);
}

namespace UE::AssetRegistry
{

void FAssetRegistryImpl::Serialize(FArchive& Ar, Impl::FEventContext& EventContext)
{
	if (Ar.IsObjectReferenceCollector())
	{
		// The Asset Registry does not have any object references, and its serialization function is expensive
		return;
	}
	else if (Ar.IsLoading())
	{
		State.Load(Ar);
		CachePathsFromState(EventContext, State);
		UpdatePersistentMountPoints();
	}
	else if (Ar.IsSaving())
	{
		State.Save(Ar, SerializationOptions);
	}
}

}

/** Append the assets from the incoming state into our own */
void UAssetRegistryImpl::AppendState(const FAssetRegistryState& InState)
{
	UE::AssetRegistry::Impl::FEventContext EventContext;
	{
		LLM_SCOPE(ELLMTag::AssetRegistry);
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		GuardedData.AppendState(EventContext, InState);
	}

	// AppendState does not create the AssetAdded events; create them here and then broadcast
	TArray<FAssetData> TempAssets;
	InState.GetAllAssets(TSet<FName>(), TempAssets, true /* bARFiltering */);
	for (FAssetData& AssetData : TempAssets)
	{
		EventContext.AssetEvents.Emplace(MoveTemp(AssetData), UE::AssetRegistry::Impl::FEventContext::EEvent::Added);
	}
	Broadcast(EventContext);
}

namespace UE::AssetRegistry
{

void FAssetRegistryImpl::AppendState(Impl::FEventContext& EventContext, const FAssetRegistryState& InState, FAssetRegistryState::EInitializationMode Mode)
{
	// @note Using this define to identify cooked editor for now. We might want to change the name later if we find more differences.
	State.InitializeFromExisting(
		InState,
#if ASSETREGISTRY_ENABLE_PREMADE_REGISTRY_IN_EDITOR
		DevelopmentSerializationOptions,
#else
		SerializationOptions,
#endif
		Mode);

	CachePathsFromState(EventContext, InState);
}

void FAssetRegistryImpl::CachePathsFromState(Impl::FEventContext& EventContext, const FAssetRegistryState& InState)
{
	SCOPED_BOOT_TIMING("FAssetRegistryImpl::CachePathsFromState");

	// Refreshes ClassGeneratorNames if out of date due to module load
	CollectCodeGeneratorClasses();

	// Add paths to cache
	for (const FAssetData* AssetData : InState.CachedAssets)
	{
		if (AssetData != nullptr)
		{
			AddAssetPath(EventContext, AssetData->PackagePath);

			// Populate the class map if adding blueprint
			if (ClassGeneratorNames.Contains(AssetData->AssetClassPath))
			{
				FAssetRegistryExportPath GeneratedClass = AssetData->GetTagValueRef<FAssetRegistryExportPath>(FBlueprintTags::GeneratedClassPath);
				FAssetRegistryExportPath ParentClass = AssetData->GetTagValueRef<FAssetRegistryExportPath>(FBlueprintTags::ParentClassPath);

				if (GeneratedClass && ParentClass)
				{
					AddCachedBPClassParent(GeneratedClass.ToTopLevelAssetPath(), ParentClass.ToTopLevelAssetPath());

					// Invalidate caching because CachedBPInheritanceMap got modified
					TempCachedInheritanceBuffer.bDirty = true;
				}
			}
		}
	}
}

}

SIZE_T UAssetRegistryImpl::GetAllocatedSize(bool bLogDetailed) const
{
	SIZE_T StateSize = 0;
	SIZE_T StaticSize = 0;
	SIZE_T SearchSize = 0;
	{
		FReadScopeLock InterfaceScopeLock(InterfaceLock);
		GuardedData.GetAllocatedSize(bLogDetailed, StateSize, StaticSize, SearchSize);
		StaticSize += sizeof(UAssetRegistryImpl);
#if WITH_EDITOR
		StaticSize += OnDirectoryChangedDelegateHandles.GetAllocatedSize();
#endif
	}

	if (bLogDetailed)
	{
		UE_LOG(LogAssetRegistry, Log, TEXT("AssetRegistry Static Size: %" SIZE_T_FMT "k"), StaticSize / 1024);
		UE_LOG(LogAssetRegistry, Log, TEXT("AssetRegistry Search Size: %" SIZE_T_FMT "k"), SearchSize / 1024);
	}

	return StateSize + StaticSize + SearchSize;
}

namespace UE::AssetRegistry
{

void FAssetRegistryImpl::GetAllocatedSize(bool bLogDetailed, SIZE_T& StateSize, SIZE_T& StaticSize, SIZE_T& SearchSize) const
{
	StateSize = State.GetAllocatedSize(bLogDetailed);

	StaticSize = CachedEmptyPackages.GetAllocatedSize() + CachedBPInheritanceMap.GetAllocatedSize() + ClassGeneratorNames.GetAllocatedSize();
	SearchSize = BackgroundResults.GetAllocatedSize() + CachedPathTree.GetAllocatedSize();

	if (bIsTempCachingEnabled && !bIsTempCachingAlwaysEnabled)
	{
		SIZE_T TempCacheMem = TempCachedInheritanceBuffer.GetAllocatedSize();
		StaticSize += TempCacheMem;
		UE_LOG(LogAssetRegistry, Warning, TEXT("Asset Registry Temp caching enabled, wasting memory: %lldk"), TempCacheMem / 1024);
	}

	if (GlobalGatherer.IsValid())
	{
		SearchSize += sizeof(*GlobalGatherer);
		SearchSize += GlobalGatherer->GetAllocatedSize();
	}

	StaticSize += SerializationOptions.CookFilterlistTagsByClass.GetAllocatedSize();
	for (const TPair<FTopLevelAssetPath, TSet<FName>>& Pair : SerializationOptions.CookFilterlistTagsByClass)
	{
		StaticSize += Pair.Value.GetAllocatedSize();
	}
}

}

void UAssetRegistryImpl::LoadPackageRegistryData(FArchive& Ar, FLoadPackageRegistryData& InOutData) const
{
	FPackageReader Reader;
	if (Reader.OpenPackageFile(&Ar))
	{
		UE::AssetRegistry::Utils::ReadAssetFile(Reader, InOutData);
	}
}

void UAssetRegistryImpl::LoadPackageRegistryData(const FString& PackageFilename, FLoadPackageRegistryData& InOutData) const
{
	FPackageReader Reader;
	if (Reader.OpenPackageFile(PackageFilename))
	{
		UE::AssetRegistry::Utils::ReadAssetFile(Reader, InOutData);
	}
}

namespace UE::AssetRegistry::Utils
{

bool ReadAssetFile(FPackageReader& PackageReader, IAssetRegistry::FLoadPackageRegistryData& InOutData)
{
	TArray<FAssetData*> AssetDataList;
	TArray<FString> CookedPackageNamesWithoutAssetDataGathered;

	const bool bGetDependencies = (InOutData.bGetDependencies);
	FPackageDependencyData DependencyData;

	bool bReadOk = FAssetDataGatherer::ReadAssetFile(PackageReader, AssetDataList, DependencyData,
		CookedPackageNamesWithoutAssetDataGathered,
		InOutData.bGetDependencies ? FPackageReader::EReadOptions::Dependencies : FPackageReader::EReadOptions::None);

	if (bReadOk)
	{
		// Copy & free asset data to the InOutData
		InOutData.Data.Reset(AssetDataList.Num());
		for (FAssetData* AssetData : AssetDataList)
		{
			InOutData.Data.Emplace(*AssetData);
		}

		AssetDataList.Reset();

		if (InOutData.bGetDependencies)
		{
			InOutData.DataDependencies.Reset(DependencyData.PackageDependencies.Num());
			for (FPackageDependencyData::FPackageDependency& Dependency : DependencyData.PackageDependencies)
			{
				InOutData.DataDependencies.Emplace(Dependency.PackageName);
			}
		}
	}

	// Cleanup the allocated asset data
	for (FAssetData* AssetData : AssetDataList)
	{
		delete AssetData;
	}

	return bReadOk;
}

}

void UAssetRegistryImpl::InitializeTemporaryAssetRegistryState(FAssetRegistryState& OutState,
	const FAssetRegistrySerializationOptions& Options, bool bRefreshExisting,
	const TSet<FName>& RequiredPackages, const TSet<FName>& RemovePackages) const
{
	using FAssetDataMap = UE::AssetRegistry::Private::FAssetDataMap;

	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	const FAssetRegistryState& State = GuardedData.GetState();
	if (!RequiredPackages.IsEmpty() || !RemovePackages.IsEmpty())
	{
		if (bRefreshExisting)
		{
			// InitializeFromExistingAndPrune does not support EInitializationMode so we have to Initialize and then Prune
			OutState.InitializeFromExisting(State.CachedAssets, State.CachedDependsNodes, State.CachedPackageData, Options,
				FAssetRegistryState::EInitializationMode::OnlyUpdateExisting);
			OutState.PruneAssetData(RequiredPackages, RemovePackages, Options);
		}
		else
		{
			TSet<int32> UnusedChunksToKeep;
			OutState.InitializeFromExistingAndPrune(State, RequiredPackages, RemovePackages, UnusedChunksToKeep, Options);
		}
	}
	else
	{
		OutState.InitializeFromExisting(State.CachedAssets, State.CachedDependsNodes, State.CachedPackageData, Options,
			bRefreshExisting ? FAssetRegistryState::EInitializationMode::OnlyUpdateExisting : FAssetRegistryState::EInitializationMode::Rebuild);
	}
}

#if ASSET_REGISTRY_STATE_DUMPING_ENABLED
void UAssetRegistryImpl::DumpState(const TArray<FString>& Arguments, TArray<FString>& OutPages, int32 LinesPerPage) const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	GuardedData.GetState().Dump(Arguments, OutPages, LinesPerPage);
}
#endif

const FAssetRegistryState* UAssetRegistryImpl::GetAssetRegistryState() const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return &GuardedData.GetState();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

TSet<FName> UAssetRegistryImpl::GetCachedEmptyPackagesCopy() const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	return GuardedData.GetCachedEmptyPackages();
}

const TSet<FName>& UAssetRegistryImpl::GetCachedEmptyPackages() const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return GuardedData.GetCachedEmptyPackages();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

bool UAssetRegistryImpl::ContainsTag(FName TagName) const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	return GuardedData.GetState().GetTagToAssetDatasMap().Contains(TagName);
}

namespace UE::AssetRegistry
{

namespace Impl
{

FScanPathContext::FScanPathContext(FEventContext& InEventContext, FClassInheritanceContext& InInheritanceContext, 
	const TArray<FString>& InDirs, const TArray<FString>& InFiles, bool bInForceRescan,
	bool bInIgnoreDenyListScanFilters, TArray<FSoftObjectPath>* FoundAssets)
	: EventContext(InEventContext)
	, InheritanceContext(InInheritanceContext)
	, OutFoundAssets(FoundAssets)
	, bForceRescan(bInForceRescan)
	, bIgnoreDenyListScanFilters(bInIgnoreDenyListScanFilters)
{
	if (OutFoundAssets)
	{
		OutFoundAssets->Empty();
	}

	if (bIgnoreDenyListScanFilters && !bForceRescan)
	{
		// This restriction is necessary because we have not yet implemented some of the required behavior to handle bIgnoreDenyListScanFilters without bForceRescan;
		// For skipping of directories that we have already scanned, we would have to check whether the directory has been set to be monitored with the proper flag (ignore deny list or not)
		// rather than just checking whether it has been set to be monitored at all
		UE_LOG(LogAssetRegistry, Warning, TEXT("ScanPathsSynchronous: bIgnoreDenyListScanFilters==true is only valid when bForceRescan==true. Setting bForceRescan=true."));
		bForceRescan = true;
	}

	FString LocalPath;
	FString PackageName;
	FString Extension;
	FPackageName::EFlexNameType FlexNameType;
	LocalFiles.Reserve(InFiles.Num());
	PackageFiles.Reserve(InFiles.Num());
	for (const FString& InFile : InFiles)
	{
		if (InFile.IsEmpty())
		{
			continue;
		}
		else if (!FPackageName::TryConvertToMountedPath(InFile, &LocalPath, &PackageName, nullptr, nullptr, &Extension, &FlexNameType))
		{
			UE_LOG(LogAssetRegistry, Warning, TEXT("ScanPathsSynchronous: %s is not in a mounted path, will not scan."), *InFile);
			continue;
		}
		if (FPackageName::IsTempPackage(PackageName))
		{
			UE_LOG(LogAssetRegistry, Warning, TEXT("ScanPathsSynchronous: %s is in the /Temp path, will not scan."), *InFile);
			continue;
		}
		if (Extension.IsEmpty())
		{
			// The empty extension is not a valid Package extension; it might exist, but we will pay the price to check it
			if (!IFileManager::Get().FileExists(*LocalPath))
			{
				// Find the extension
				FPackagePath PackagePath = FPackagePath::FromLocalPath(LocalPath);
				if (!FPackageName::DoesPackageExist(PackagePath, &PackagePath))
				{
					UE_LOG(LogAssetRegistry, Warning, TEXT("ScanPathsSynchronous: Package %s does not exist, will not scan."), *InFile);
					continue;
				}
				Extension = LexToString(PackagePath.GetHeaderExtension());
			}
		}
		LocalFiles.Add(LocalPath + Extension);
		PackageFiles.Add(PackageName);
	}
	LocalDirs.Reserve(InDirs.Num());
	PackageDirs.Reserve(InDirs.Num());
	for (const FString& InDir : InDirs)
	{
		if (InDir.IsEmpty())
		{
			continue;
		}
		else if (!FPackageName::TryConvertToMountedPath(InDir, &LocalPath, &PackageName, nullptr, nullptr, &Extension, &FlexNameType))
		{
			UE_LOG(LogAssetRegistry, Warning, TEXT("ScanPathsSynchronous: %s is not in a mounted path, will not scan."), *InDir);
			continue;
		}
		if (FPackageName::IsTempPackage(PackageName))
		{
			UE_LOG(LogAssetRegistry, Warning, TEXT("ScanPathsSynchronous: %s is in the /Temp path, will not scan."), *InDir);
			continue;
		}
		LocalDirs.Add(LocalPath + Extension);
		PackageDirs.Add(PackageName + Extension);
	}
}

}

void FAssetRegistryImpl::ScanPathsSynchronous(Impl::FScanPathContext& Context)
{
	LLM_SCOPE(ELLMTag::AssetRegistry);

	ConstructGatherer();
	FAssetDataGatherer& Gatherer = *GlobalGatherer;

	// Add a cache file for any not-yet-scanned dirs
	TArray<FString> CacheFilePackagePaths;
	if (!Context.bForceRescan && (Gatherer.IsCacheReadEnabled() || Gatherer.IsCacheWriteEnabled()))
	{
		for (int n = 0; n < Context.LocalDirs.Num(); ++n)
		{
			if (!Gatherer.IsOnAllowList(Context.LocalDirs[n]))
			{
				CacheFilePackagePaths.Add(Context.PackageDirs[n]);
			}
		}
	}
	Context.LocalPaths.Reserve(Context.LocalFiles.Num() + Context.LocalDirs.Num());
	Context.LocalPaths.Append(MoveTemp(Context.LocalDirs));
	Context.LocalPaths.Append(MoveTemp(Context.LocalFiles));
	if (Context.LocalPaths.IsEmpty())
	{
		return;
	}
	Gatherer.AddRequiredMountPoints(Context.LocalPaths);

	FString CacheFilename;
	if (!CacheFilePackagePaths.IsEmpty())
	{
		CacheFilename = Gatherer.GetCacheFilename(CacheFilePackagePaths);
		Gatherer.LoadCacheFiles({CacheFilename});
	}

	// If we are forcing a rescan, then delete any old assets that no longer exist. If we are not forcing a rescan,
	// then there should not be any old assets that no longer exist, so we skip the cost of searching for them.
	TSet<FSoftObjectPath> OldAssetsToRemove;
	if (Context.bForceRescan)
	{
		// Initialize OldAssetsToRemove to the list of all assets in the given paths.
		if (!Context.PackageDirs.IsEmpty())
		{
			FARFilter Filter;
			Filter.bIncludeOnlyOnDiskAssets = true;
			Filter.bRecursivePaths = true;
			for (const FString& PackageDir : Context.PackageDirs)
			{
				Filter.PackagePaths.Add(FName(*PackageDir));
			}
			FARCompiledFilter CompiledFilter;
			CompileFilter(Context.InheritanceContext, Filter, CompiledFilter);
			TArray<FAssetData> AssetsInPaths;
			State.EnumerateAssets(CompiledFilter, TSet<FName>() /* PackageNamesToSkip */,
				[&OldAssetsToRemove](const FAssetData& AssetData)
				{
					OldAssetsToRemove.Add(AssetData.ToSoftObjectPath());
					return true;
				});
		}
		for (const FString& PackageName : Context.PackageFiles)
		{
			for (const FAssetData* AssetData : State.GetAssetsByPackageName(FName(*PackageName)))
			{
				OldAssetsToRemove.Add(AssetData->ToSoftObjectPath());
			}
		}
	}

	Gatherer.ScanPathsSynchronous(Context.LocalPaths, Context.bForceRescan, Context.bIgnoreDenyListScanFilters, CacheFilename, Context.PackageDirs);
	TArray<FName> FoundAssetPackageNames;

	auto AssetsFoundCallback =
		[&Context, &FoundAssetPackageNames, &OldAssetsToRemove, this]
		(const TMultiMap<FName, FAssetData*>& InFoundAssets)
	{
		Context.NumFoundAssets = InFoundAssets.Num();

		FoundAssetPackageNames.Reset();
		FoundAssetPackageNames.Reserve(Context.NumFoundAssets);

		// The gatherer may have added other assets that were scanned as part of the ongoing background scan,
		// so remove any assets that were not in the requested paths
		for (const TPair<FName,FAssetData*>& Pair : InFoundAssets)
		{
			FAssetData* AssetData = Pair.Value;
			bool bIsInRequestedPaths = false;

			TStringBuilder<128> PackageNameStr;
			AssetData->PackageName.ToString(PackageNameStr);
			FStringView PackageName(PackageNameStr.ToString(), PackageNameStr.Len());

			for (const FString& RequestedPackageDir : Context.PackageDirs)
			{
				if (FPathViews::IsParentPathOf(RequestedPackageDir, PackageName))
				{
					bIsInRequestedPaths = true;
					break;
				}
			}

			if (!bIsInRequestedPaths)
			{
				for (const FString& RequestedPackageFile : Context.PackageFiles)
				{
					if (PackageName.Equals(RequestedPackageFile, ESearchCase::IgnoreCase))
					{
						bIsInRequestedPaths = true;
						break;
					}
				}
			}

			if (bIsInRequestedPaths)
			{
				UE_LOG(LogAssetRegistry, VeryVerbose, TEXT("FAssetRegistryImpl::ScanPathsSynchronous: Found Asset: %s"),
					*AssetData->GetObjectPathString());
				if (Context.OutFoundAssets)
				{
					Context.OutFoundAssets->Add(AssetData->GetSoftObjectPath());
				}
				FoundAssetPackageNames.Add(AssetData->PackageName);
			}

			if (!OldAssetsToRemove.IsEmpty())
			{
				OldAssetsToRemove.Remove(AssetData->ToSoftObjectPath());
			}
		}
	};

	bool bUnusedInterrupted;
	Context.Status = TickGatherer(Context.EventContext, Context.InheritanceContext, -1., bUnusedInterrupted,
		FAssetsFoundCallback(AssetsFoundCallback));
#if WITH_EDITOR
	LoadCalculatedDependencies(&FoundAssetPackageNames, -1., Context.InheritanceContext, bUnusedInterrupted);
#endif
	for (FSoftObjectPath& OldAssetToRemove : OldAssetsToRemove)
	{
		bool bOutRemovedAssetData;
		bool bOutRemovedPackageData;
		State.RemoveAssetData(OldAssetToRemove, true /* bRemoveDependencyData */,
			bOutRemovedAssetData, bOutRemovedPackageData);
	}
}

namespace Utils
{

bool IsPathMounted(const FString& Path, const TSet<FString>& MountPointsNoTrailingSlashes, FString& StringBuffer)
{
	const int32 SecondSlash = Path.Len() > 1 ? Path.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 1) : INDEX_NONE;
	if (SecondSlash != INDEX_NONE)
	{
		StringBuffer.Reset(SecondSlash);
		StringBuffer.Append(*Path, SecondSlash);
		if (MountPointsNoTrailingSlashes.Contains(StringBuffer))
		{
			return true;
		}
	}
	else
	{
		if (MountPointsNoTrailingSlashes.Contains(Path))
		{
			return true;
		}
	}

	return false;
}

}

#if WITH_EDITOR
void FAssetRegistryImpl::PostLoadAssetRegistryTags(FAssetData* AssetData)
{
	check(AssetData);
	if (AssetData->TagsAndValues.Num())
	{
		FTopLevelAssetPath AssetClassPath = AssetData->AssetClassPath;
		UClass* AssetClass = FindObject<UClass>(AssetClassPath, true);
		while (!AssetClass)
		{
			// this is probably a blueprint that has not yet been loaded, try to find its native base class
			const FTopLevelAssetPath* ParentClassPath = CachedBPInheritanceMap.Find(AssetClassPath);
			if (ParentClassPath && !ParentClassPath->IsNull())
			{
				AssetClassPath = *ParentClassPath;
				AssetClass = FindObject<UClass>(AssetClassPath, true);
			}
			else
			{
				break;
			}
		}

		if (AssetClass)
		{
			if (UObject* ClassCDO = AssetClass->GetDefaultObject(false))
			{
				TArray<UObject::FAssetRegistryTag> TagsToModify;
				ClassCDO->PostLoadAssetRegistryTags(*AssetData, TagsToModify);
				if (TagsToModify.Num())
				{
					FAssetDataTagMap TagsAndValues = AssetData->TagsAndValues.CopyMap();
					for (const UObject::FAssetRegistryTag& Tag : TagsToModify)
					{
						if (!Tag.Value.IsEmpty())
						{
							FString& Value = TagsAndValues.FindOrAdd(Tag.Name);
							Value = Tag.Value;
						}
						else
						{
							TagsAndValues.Remove(Tag.Name);
						}
					}
					AssetData->TagsAndValues = FAssetDataTagMapSharedView(MoveTemp(TagsAndValues));
				}
			}
		}
	}
}
#endif

bool FAssetRegistryImpl::ShouldSkipGatheredAsset(FAssetData& AssetData)
{
	// TODO: This pruning of invalid ExternalActors is temporary, to handle the fallout from a bug in SaveAs
	// that is keeping the old ExternalActors as duplicates of the new ones. Remove it after the data has been
	// cleaned up for all affected licensees. If we need such validation permanently, it should be decoupled
	// from the AssetRegistry by adding a delegate.
	// Extra validation for ExternalActors. If duplicate ExternalActors with the same object path exist
	// then we intermittently will fail to find the correct one and WorldPartition will break.
	// Validate that the PackageName matches what is expected from the ObjectPath.

#if WITH_EDITORONLY_DATA
	if (AssetData.GetOptionalOuterPathName().IsNone())
	{
		// If no outer path, this can't be an external asset
		return false;
	}
#endif

	FStringView ExternalActorsFolderName(FPackagePath::GetExternalActorsFolderName());
	TStringBuilder<256> PackageNameStr;
	AssetData.PackageName.ToString(PackageNameStr);
	if (UE::String::FindFirst(PackageNameStr, ExternalActorsFolderName) != INDEX_NONE)
	{
		TStringBuilder<256> ObjectPathString;
		AssetData.AppendObjectPath(ObjectPathString);
		FStringView ObjectPathPackageName = FPackageName::ObjectPathToPackageName(ObjectPathString);
		FStringView PackageNamePackageRoot;
		FStringView PackageNameRelPath;
		FStringView ObjectPathPackageRoot;
		FStringView ObjectPathRelPath;

		// /PackageRoot/__ExternalActors__/RelPathFromPackageRootToMap/#/##/#######
		// OR
		// /PackageRoot/__ExternalActors__/ContentBundle/######/RelPathFromPackageRootToMap/#/##/#######
		// OR
		// /PackageRoot/__ExternalActors__/EDL/######/ObjectPathPackageRoot/RelPathFromPackageRootToMap/#/##/#######
		// Package roots do not need to be the same; ContentBundles can be injected into /Game maps from plugins
		PackageNamePackageRoot = FPackageName::SplitPackageNameRoot(PackageNameStr, &PackageNameRelPath);
		ObjectPathPackageRoot = FPackageName::SplitPackageNameRoot(ObjectPathPackageName, &ObjectPathRelPath);

		if (!PackageNameRelPath.StartsWith(ExternalActorsFolderName) || !PackageNameRelPath.RightChop(ExternalActorsFolderName.Len()).StartsWith(TEXT("/")))
		{
			UE_LOG(LogAssetRegistry, Verbose,
				TEXT("Invalid ExternalActor: Package %s is an ExternalActor package but is not in the expected root path for ExternalActors /%.*s/%.*s. Ignoring this actor."),
				*PackageNameStr, PackageNamePackageRoot.Len(), PackageNamePackageRoot.GetData(),
				ExternalActorsFolderName.Len(), ExternalActorsFolderName.GetData());
			return true;
		}

		bool bIsEDLActor = false;
		bool bIsPluginActor = false;
		FStringView PackageNameRelPathAfterExternalActorRoot = PackageNameRelPath.RightChop(ExternalActorsFolderName.Len() + 1);
		FStringView ContentBundleDirName(TEXTVIEW("ContentBundle"));
		FStringView ExternalDataLayerDirName(TEXTVIEW("EDL"));
		if (PackageNameRelPathAfterExternalActorRoot.StartsWith(ContentBundleDirName))
		{
			PackageNameRelPathAfterExternalActorRoot.RightChopInline(ContentBundleDirName.Len());
			bIsPluginActor = true;
		}
		else if (PackageNameRelPathAfterExternalActorRoot.StartsWith(ExternalDataLayerDirName))
		{
			PackageNameRelPathAfterExternalActorRoot.RightChopInline(ExternalDataLayerDirName.Len());
			bIsEDLActor = true;
			bIsPluginActor = true;
		}

		bool bAllowValidation = true;
		if (bIsPluginActor)
		{
			bAllowValidation = false; // Don't allow validation unless we succeed in finding the new relpath
			if (PackageNameRelPathAfterExternalActorRoot.StartsWith(TEXT("/")))
			{
				PackageNameRelPathAfterExternalActorRoot.RightChopInline(1);
				int32 NextSlash;
				PackageNameRelPathAfterExternalActorRoot.FindChar('/', NextSlash);
				if (NextSlash != INDEX_NONE)
				{
					PackageNameRelPathAfterExternalActorRoot.RightChopInline(NextSlash + 1);
					// EDL path keeps ObjectPathPackageRoot
					if (bIsEDLActor)
					{
						if (PackageNameRelPathAfterExternalActorRoot.StartsWith(ObjectPathPackageRoot))
						{
							PackageNameRelPathAfterExternalActorRoot.RightChopInline(ObjectPathPackageRoot.Len());
							if (PackageNameRelPathAfterExternalActorRoot.StartsWith(TEXT("/")))
							{
								PackageNameRelPathAfterExternalActorRoot.RightChopInline(1);
								bAllowValidation = true;
							}
						}
					}
					else
					{
						bAllowValidation = true;
					}
				}
			}
		}

		if (bAllowValidation && !PackageNameRelPathAfterExternalActorRoot.StartsWith(ObjectPathRelPath))
		{
			TStringBuilder<256> ExpectedPath;
			ExpectedPath << "/" << ObjectPathPackageRoot << "/" << ExternalActorsFolderName << "/" << ObjectPathRelPath;
			UE_LOG(LogAssetRegistry, Verbose,
				TEXT("Invalid ExternalActor: Package %s is an ExternalActor package but its path does not match the expected path %s created from its objectpath %s. Ignoring this actor."),
				*PackageNameStr, *ExpectedPath, *ObjectPathString);
			return true;
		}
	}
	return false;
}

void FAssetRegistryImpl::AssetSearchDataGathered(Impl::FEventContext& EventContext, const double TickStartTime, TMultiMap<FName, FAssetData*>& AssetResults)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AssetSearchDataGathered);
	const bool bFlushFullBuffer = TickStartTime < 0;

	// Refreshes ClassGeneratorNames if out of date due to module load
	CollectCodeGeneratorClasses();

	TSet<FString> MountPoints;
	FString PackagePathString;
	FString PackageRoot;
	if (AssetResults.Num() > 0 && bVerifyMountPointAfterGather)
	{
		TArray<FString> MountPointsArray;
		FPackageName::QueryRootContentPaths(MountPointsArray, /*bIncludeReadOnlyRoots=*/ true, /*bWithoutLeadingSlashes*/ false, /*WithoutTrailingSlashes=*/ true);
		MountPoints.Append(MoveTemp(MountPointsArray));
	}

	// Add the found assets
	for (TMultiMap<FName, FAssetData*>::TIterator Iter(AssetResults); Iter; ++Iter)
	{
		// Delete or take ownership of the BackgroundResult; it was originally new'd by an FPackageReader
		TUniquePtr<FAssetData> BackgroundResult(Iter.Value());
		CA_ASSUME(BackgroundResult.Get() != nullptr);
		Iter.RemoveCurrent();

		// Try to update any asset data that may already exist
		FCachedAssetKey Key(*BackgroundResult);

		if (ShouldSkipGatheredAsset(*BackgroundResult))
		{
			continue;
		}

		FAssetData* const* FoundData = State.CachedAssets.Find(Key);
		FAssetData* ExistingAssetData = FoundData ? *FoundData : nullptr;

		const FName PackagePath = BackgroundResult->PackagePath;

		// Skip stale results caused by mount then unmount of a path within short period.
		bool bPathIsMounted = true;
		if (bVerifyMountPointAfterGather)
		{
			PackagePath.ToString(PackagePathString);
			if (!Utils::IsPathMounted(PackagePathString, MountPoints, PackageRoot))
			{
				bPathIsMounted = false;
			}
		}

		if (ExistingAssetData)
		{
			// If this ensure fires then we've somehow processed the same result more than once, and that should never happen
			if (ensure(ExistingAssetData != BackgroundResult.Get()))
			{
#if WITH_EDITOR
				PostLoadAssetRegistryTags(BackgroundResult.Get());
				if (AssetDataObjectPathsUpdatedOnLoad.Contains(BackgroundResult->GetSoftObjectPath()))
				{
					// If the current AssetData came from a loaded asset, don't overwrite it with the new one from disk
					// The loaded asset is more authoritative because it has run the postload steps.
					// However, the loaded asset is missing the extended tags. Our contract for extended tags is to keep any 
					// that do not exist in the non-extended tags. So add on any tags from the BackgroundResult that
					// are not already on the existing asset.
					AddNonOverlappingTags(EventContext, *ExistingAssetData, *BackgroundResult);
				}
				else
#endif
				{
					// The asset exists in the cache from disk and has not yet been loaded into memory, update it with the new background data
					UpdateAssetData(EventContext, ExistingAssetData, MoveTemp(*BackgroundResult), false /* bKeepDeletedTags */);
				}
			}
		}
		else
		{
			// The asset isn't in the cache yet, add it and notify subscribers
			if (bPathIsMounted)
			{
#if !NO_LOGGING
				if (bVerboseLogging)
				{
					int64& ClassTagSizes = TagSizeByClass.FindOrAdd(BackgroundResult->AssetClassPath);
					BackgroundResult->TagsAndValues.ForEach([&ClassTagSizes](const TPair<FName, FAssetTagValueRef>& Pair)
						{
							ClassTagSizes += Pair.Value.GetResourceSize();
						});
				}
#endif

#if WITH_EDITOR
				PostLoadAssetRegistryTags(BackgroundResult.Get());
#endif
				AddAssetData(EventContext, BackgroundResult.Release());
			}
		}

		if (bPathIsMounted)
		{
			// Populate the path tree
			AddAssetPath(EventContext, PackagePath);
		}
		else
		{
			UE_LOG(LogAssetRegistry, Warning, TEXT("AssetRegistry: An asset has been loaded with an invalid mount point: '%s', Mount Point: '%s'"), *BackgroundResult->GetObjectPathString(), *PackagePathString)
		}

		// Check to see if we have run out of time in this tick
		if (!bFlushFullBuffer && (FPlatformTime::Seconds() - TickStartTime) > Impl::MaxSecondsPerFrame)
		{
			return;
		}
	}
}

void FAssetRegistryImpl::PathDataGathered(Impl::FEventContext& EventContext, const double TickStartTime, TRingBuffer<FString>& PathResults)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(PathDataGathered);
	const bool bFlushFullBuffer = TickStartTime < 0;

	TSet<FString> MountPoints;
	FString PackageRoot;
	if (PathResults.Num() > 0 && bVerifyMountPointAfterGather)
	{
		TArray<FString> MountPointsArray;
		FPackageName::QueryRootContentPaths(MountPointsArray, /*bIncludeReadOnlyRoots=*/ true, /*bWithoutLeadingSlashes*/ false, /*WithoutTrailingSlashes=*/ true);
		MountPoints.Append(MoveTemp(MountPointsArray));
	}
	
	CachedPathTree.EnsureAdditionalCapacity(PathResults.Num());

	while (PathResults.Num() > 0)
	{
		FString Path = PathResults.PopFrontValue();

		// Skip stale results caused by mount then unmount of a path within short period.
		if (!bVerifyMountPointAfterGather || Utils::IsPathMounted(Path, MountPoints, PackageRoot))
		{
			AddAssetPath(EventContext, FName(*Path));
		}
		else
		{
			UE_LOG(LogAssetRegistry, Warning, TEXT("AssetRegistry: A path has been loaded with an invalid mount point: '%s'"), *Path)
		}

		// Check to see if we have run out of time in this tick
		if (!bFlushFullBuffer && (FPlatformTime::Seconds() - TickStartTime) > Impl::MaxSecondsPerFrame)
		{
			return;
		}
	}
}

void FAssetRegistryImpl::DependencyDataGathered(const double TickStartTime, TMultiMap<FName, FPackageDependencyData>& DependsResults)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DependencyDataGathered);
	using namespace UE::AssetRegistry;
	const bool bFlushFullBuffer = TickStartTime < 0;

	TMap<FName, FName> CachedDepToRedirect;
	for (TMultiMap<FName, FPackageDependencyData>::TIterator Iter(DependsResults); Iter; ++Iter)
	{
		FPackageDependencyData Result = MoveTemp(Iter.Value());
		Iter.RemoveCurrent();

		checkf(!GIsEditor || Result.bHasPackageData, TEXT("We rely on PackageData being read for every gathered Asset in the editor."));
		if (Result.bHasPackageData)
		{
			// Update package data
			FAssetPackageData* PackageData = State.CreateOrGetAssetPackageData(Result.PackageName);
			*PackageData = Result.PackageData;
		}

		if (Result.bHasDependencyData)
		{
			FDependsNode* Node = State.CreateOrFindDependsNode(Result.PackageName);
#if WITH_EDITOR
			PackagesNeedingDependencyCalculation.Add(Result.PackageName);
#endif

			// We will populate the node dependencies below. Empty the set here in case this file was already read
			// Also remove references to all existing dependencies, those will be also repopulated below
			Node->IterateOverDependencies([Node](FDependsNode* InDependency, UE::AssetRegistry::EDependencyCategory Category, UE::AssetRegistry::EDependencyProperty Properties, bool bDuplicate)
				{
					if (!bDuplicate)
					{
						InDependency->RemoveReferencer(Node);
					}
				});

			Node->ClearDependencies();
			Node->SetIsDependencyListSorted(EDependencyCategory::All, ShouldSortDependencies());
			Node->SetIsReferencersSorted(ShouldSortReferencers());

			// Don't bother registering dependencies on these packages, every package in the game will depend on them
			static TArray<FName> ScriptPackagesToSkip = TArray<FName>{ TEXT("/Script/CoreUObject"), TEXT("/Script/Engine"), TEXT("/Script/BlueprintGraph"), TEXT("/Script/UnrealEd") };

			// Conditionally add package dependencies
			TMap<FName, FDependsNode::FPackageFlagSet> PackageDependencies;
			for (FPackageDependencyData::FPackageDependency& DependencyData : Result.PackageDependencies)
			{
				// Skip hard dependencies to the common script packages
				FName DependencyPackageName = DependencyData.PackageName;
				if (EnumHasAnyFlags(DependencyData.Property, EDependencyProperty::Hard) && ScriptPackagesToSkip.Contains(DependencyPackageName))
				{
					continue;
				}
				
				FName& RedirectedName = CachedDepToRedirect.FindOrAdd(DependencyPackageName, NAME_None);
				if (RedirectedName.IsNone())
				{
					RedirectedName = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Package,
						FCoreRedirectObjectName(NAME_None, NAME_None, DependencyPackageName)).PackageName;
				}
				DependencyPackageName = RedirectedName;

				FDependsNode::FPackageFlagSet& PackageFlagSet = PackageDependencies.FindOrAdd(DependencyPackageName);
				PackageFlagSet.Add(FDependsNode::PackagePropertiesToByte(DependencyData.Property));
			}

			// Doubly-link all of the PackageDependencies
			for (TPair<FName, FDependsNode::FPackageFlagSet>& NewDependsIt : PackageDependencies)
			{
				FName DependencyPackageName = NewDependsIt.Key;
				FAssetIdentifier Identifier(DependencyPackageName);
				FDependsNode* DependsNode = State.CreateOrFindDependsNode(Identifier);

				// Handle failure of CreateOrFindDependsNode
				// And Skip dependencies to self 
				if (DependsNode != nullptr && DependsNode != Node)
				{
					if (DependsNode->GetConnectionCount() == 0)
					{
						DependsNode->SetIsDependencyListSorted(EDependencyCategory::All, ShouldSortDependencies());
						DependsNode->SetIsReferencersSorted(ShouldSortReferencers());

						// This was newly created, see if we need to read the script package Guid
						const FNameBuilder DependencyPackageNameStr(DependencyPackageName);

						if (FPackageName::IsScriptPackage(DependencyPackageNameStr))
						{
							// Get the guid off the script package, it is updated when script is changed so we need to refresh it every run
							UPackage* Package = FindPackage(nullptr, *DependencyPackageNameStr);

							if (Package)
							{
								FAssetPackageData* ScriptPackageData = State.CreateOrGetAssetPackageData(DependencyPackageName);
#if WITH_EDITORONLY_DATA
								ScriptPackageData->SetPackageSavedHash(Package->GetSavedHash());
#endif
							}
						}
					}

					Node->AddPackageDependencySet(DependsNode, NewDependsIt.Value);
					DependsNode->AddReferencer(Node);
				}
			}

			// Add node for all name references
			for (FPackageDependencyData::FSearchableNamesDependency& NamesDependency : Result.SearchableNameDependencies)
			{
				for (FName& ValueName : NamesDependency.ValueNames)
				{
					FAssetIdentifier AssetId(NamesDependency.PackageName, NamesDependency.ObjectName, ValueName);
					FDependsNode* DependsNode = State.CreateOrFindDependsNode(AssetId);
					if (DependsNode != nullptr)
					{
						Node->AddDependency(DependsNode, EDependencyCategory::SearchableName, EDependencyProperty::None);
						DependsNode->AddReferencer(Node);
					}
				}
			}
			Node->SetIsDependenciesInitialized(true);
		}

		// Check to see if we have run out of time in this tick
		if (!bFlushFullBuffer && (FPlatformTime::Seconds() - TickStartTime) > Impl::MaxSecondsPerFrame)
		{
			return;
		}
	}
}

void FAssetRegistryImpl::CookedPackageNamesWithoutAssetDataGathered(Impl::FEventContext& EventContext,
	const double TickStartTime, TRingBuffer<FString>& CookedPackageNamesWithoutAssetDataResults, bool& bOutInterrupted)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CookedPackageNamesWithoutAssetDataGathered);
	bOutInterrupted = false;

	struct FConfigValue
	{
		FConfigValue()
		{
			if (GConfig)
			{
				GConfig->GetBool(TEXT("AssetRegistry"), TEXT("LoadCookedPackagesWithoutAssetData"), bShouldProcess, GEngineIni);
			}
		}

		bool bShouldProcess = true;
	};
	static FConfigValue ShouldProcessCookedPackages;

	// Add the found assets
	if (ShouldProcessCookedPackages.bShouldProcess)
	{
		while (CookedPackageNamesWithoutAssetDataResults.Num() > 0)
		{
			// If this data is cooked and it we couldn't find any asset in its export table then try to load the entire package 
			// Loading the entire package will make all of its assets searchable through the in-memory scanning performed by GetAssets
			EventContext.RequiredLoads.Add(CookedPackageNamesWithoutAssetDataResults.PopFrontValue());
		}
		if (TickStartTime >= 0)
		{
			// If the tick is time-limited, signal an interruption now to process the loads before proceeding with any other tick steps
			bOutInterrupted = true;
			return;
		}
	}
	else
	{
		// Do nothing will these packages. For projects which could run entirely from cooked data, this
		// process will involve opening every single package synchronously on the game thread which will
		// kill performance. We need a better way.
		CookedPackageNamesWithoutAssetDataResults.Empty();
	}
}

void FAssetRegistryImpl::VerseFilesGathered(Impl::FEventContext& EventContext, const double TickStartTime, TRingBuffer<FName>& VerseResults)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(VerseFilesGathered);
	while (VerseResults.Num() > 0)
	{
		FName VerseFilePath = VerseResults.PopFrontValue();

		AddVerseFile(EventContext, VerseFilePath);

		// Check to see if we have run out of time in this tick
		if (TickStartTime >= 0 && (FPlatformTime::Seconds() - TickStartTime) > Impl::MaxSecondsPerFrame)
		{
			return;
		}
	}
}

void FAssetRegistryImpl::AddEmptyPackage(FName PackageName)
{
	CachedEmptyPackages.Add(PackageName);
}

bool FAssetRegistryImpl::RemoveEmptyPackage(FName PackageName)
{
	return CachedEmptyPackages.Remove(PackageName) > 0;
}

bool FAssetRegistryImpl::AddAssetPath(Impl::FEventContext& EventContext, FName PathToAdd)
{
	return CachedPathTree.CachePath(PathToAdd, [this, &EventContext](FName AddedPath)
	{
		EventContext.PathEvents.Emplace(AddedPath.ToString(), Impl::FEventContext::EEvent::Added);
	});
}

bool FAssetRegistryImpl::RemoveAssetPath(Impl::FEventContext& EventContext, FName PathToRemove, bool bEvenIfAssetsStillExist)
{
	if (!bEvenIfAssetsStillExist)
	{
		// Check if there were assets in the specified folder. You can not remove paths that still contain assets
		bool bHasAsset = false;
		EnumerateAssetsByPathNoTags(PathToRemove, [&bHasAsset](const FAssetData&)
			{
				bHasAsset = true;
				return false;
			}, true /* bRecursive */, false /* bIncludeOnlyOnDiskAssets */);
		if (bHasAsset)
		{
			// At least one asset still exists in the path. Fail the remove.
			return false;
		}
	}

	CachedPathTree.RemovePath(PathToRemove, [this, &EventContext](FName RemovedPath)
	{
		EventContext.PathEvents.Emplace(RemovedPath.ToString(), Impl::FEventContext::EEvent::Removed);
	});
	return true;
}

void FAssetRegistryImpl::AddAssetData(Impl::FEventContext& EventContext, FAssetData* AssetData)
{
	State.AddAssetData(AssetData);

	if (!ShouldSkipAsset(AssetData->AssetClassPath, AssetData->PackageFlags))
	{
		EventContext.AssetEvents.Emplace(*AssetData, Impl::FEventContext::EEvent::Added);
	}

	// Populate the class map if adding blueprint
	if (ClassGeneratorNames.Contains(AssetData->AssetClassPath))
	{
		const FString GeneratedClass = AssetData->GetTagValueRef<FString>(FBlueprintTags::GeneratedClassPath);
		const FString ParentClass = AssetData->GetTagValueRef<FString>(FBlueprintTags::ParentClassPath);
		if (!GeneratedClass.IsEmpty() && !ParentClass.IsEmpty() && GeneratedClass != TEXTVIEW("None") && ParentClass != TEXTVIEW("None"))
		{
			const FTopLevelAssetPath SavedGeneratedClassPathName(GeneratedClass);
			const FTopLevelAssetPath GeneratedClassPathName(AssetData->PackageName, SavedGeneratedClassPathName.GetAssetName());
			const FTopLevelAssetPath ParentClassPathName(ParentClass);
			if (ensureAlwaysMsgf(!GeneratedClassPathName.IsNull() && !ParentClassPathName.IsNull(),
				TEXT("Short class names used in AddAssetData: GeneratedClass=%s, ParentClass=%s. Short class names in these tags on the Blueprint class should have been converted to path names."),
				*GeneratedClass, *ParentClass))
			{
				AddCachedBPClassParent(GeneratedClassPathName, ParentClassPathName);

				// Invalidate caching because CachedBPInheritanceMap got modified
				TempCachedInheritanceBuffer.bDirty = true;
			}
		}
	}
}

void FAssetRegistryImpl::UpdateAssetData(Impl::FEventContext& EventContext, FAssetData* AssetData,
	FAssetData&& NewAssetData, bool bKeepDeletedTags)
{
	// Update the class map if updating a blueprint
	if (ClassGeneratorNames.Contains(AssetData->AssetClassPath))
	{
		const FString OldGeneratedClass = AssetData->GetTagValueRef<FString>(FBlueprintTags::GeneratedClassPath);
		const FString OldParentClass = AssetData->GetTagValueRef<FString>(FBlueprintTags::ParentClassPath);
		const FString NewGeneratedClass = NewAssetData.GetTagValueRef<FString>(FBlueprintTags::GeneratedClassPath);
		const FString NewParentClass = NewAssetData.GetTagValueRef<FString>(FBlueprintTags::ParentClassPath);
		if (OldGeneratedClass != NewGeneratedClass || OldParentClass != NewParentClass)
		{
			if (!OldGeneratedClass.IsEmpty() && OldGeneratedClass != TEXTVIEW("None"))
			{
				const FTopLevelAssetPath OldGeneratedClassName(OldGeneratedClass);
				if (ensureAlwaysMsgf(!OldGeneratedClassName.IsNull(),
					TEXT("Short class name used: OldGeneratedClass=%s. Short class names in tags on the Blueprint class should have been converted to path names."),
					*OldGeneratedClass))
				{
					CachedBPInheritanceMap.Remove(OldGeneratedClassName);

					// Invalidate caching because CachedBPInheritanceMap got modified
					TempCachedInheritanceBuffer.bDirty = true;
				}
			}

			if (!NewGeneratedClass.IsEmpty() && !NewParentClass.IsEmpty() && NewGeneratedClass != TEXTVIEW("None") && NewParentClass != TEXTVIEW("None"))
			{
				const FTopLevelAssetPath NewGeneratedClassName(NewGeneratedClass);
				const FTopLevelAssetPath NewParentClassName(NewParentClass);
				if (ensureAlwaysMsgf(!NewGeneratedClassName.IsNull() && !NewParentClassName.IsNull(),
					TEXT("Short class names used in AddAssetData: GeneratedClass=%s, ParentClass=%s. Short class names in these tags on the Blueprint class should have been converted to path names."),
					*NewGeneratedClass, *NewParentClass))
				{
					AddCachedBPClassParent(NewGeneratedClassName, NewParentClassName);
				}

				// Invalidate caching because CachedBPInheritanceMap got modified
				TempCachedInheritanceBuffer.bDirty = true;
			}
		}
	}

	if (bKeepDeletedTags)
	{
		TOptional<FAssetDataTagMap> UpdatedTags;
		AssetData->TagsAndValues.ForEach([&NewAssetData, &UpdatedTags](const TPair<FName, FAssetTagValueRef>& TagPair)
			{
				if (UpdatedTags)
				{
					if (!UpdatedTags->Contains(TagPair.Key))
					{
						UpdatedTags->Add(TagPair.Key, TagPair.Value.GetStorageString());
					}
				}
				else
				{
					if (!NewAssetData.TagsAndValues.Contains(TagPair.Key))
					{
						UpdatedTags.Emplace(NewAssetData.TagsAndValues.CopyMap());
						UpdatedTags->Add(TagPair.Key, TagPair.Value.GetStorageString());
					}
				}
			});
		if (UpdatedTags)
		{
			NewAssetData.TagsAndValues = FAssetDataTagMapSharedView(MoveTemp(*UpdatedTags));
		}
	}

	bool bModified;
	State.UpdateAssetData(AssetData, MoveTemp(NewAssetData), &bModified);
	
	if (bModified && !ShouldSkipAsset(AssetData->AssetClassPath, AssetData->PackageFlags))
	{
		EventContext.AssetEvents.Emplace(*AssetData, Impl::FEventContext::EEvent::Updated);
	}
}

void FAssetRegistryImpl::AddNonOverlappingTags(Impl::FEventContext& EventContext, FAssetData& ExistingAssetData,
	const FAssetData& NewAssetData)
{
	TOptional<FAssetDataTagMap> ModifiedTags = Utils::AddNonOverlappingTags(ExistingAssetData, NewAssetData);
	if (ModifiedTags)
	{
		State.SetTagsOnExistingAsset(&ExistingAssetData, MoveTemp(*ModifiedTags));
		if (!ShouldSkipAsset(ExistingAssetData.AssetClassPath, ExistingAssetData.PackageFlags))
		{
			EventContext.AssetEvents.Emplace(ExistingAssetData, Impl::FEventContext::EEvent::Updated);
		}
	}
}

bool FAssetRegistryImpl::RemoveAssetData(Impl::FEventContext& EventContext, FAssetData* AssetData)
{
	bool bRemoved = false;

	if (ensure(AssetData))
	{
		if (!ShouldSkipAsset(AssetData->AssetClassPath, AssetData->PackageFlags))
		{
			EventContext.AssetEvents.Emplace(*AssetData, Impl::FEventContext::EEvent::Removed);
		}

		// Remove from the class map if removing a blueprint
		if (ClassGeneratorNames.Contains(AssetData->AssetClassPath))
		{
			const FString OldGeneratedClass = AssetData->GetTagValueRef<FString>(FBlueprintTags::GeneratedClassPath);
			if (!OldGeneratedClass.IsEmpty() && OldGeneratedClass != TEXTVIEW("None"))
			{
				const FTopLevelAssetPath OldGeneratedClassPathName(FPackageName::ExportTextPathToObjectPath(OldGeneratedClass));
				if (ensureAlwaysMsgf(!OldGeneratedClassPathName.IsNull(),
					TEXT("Short class name used: OldGeneratedClass=%s"), *OldGeneratedClass))
				{
					CachedBPInheritanceMap.Remove(OldGeneratedClassPathName);

					// Invalidate caching because CachedBPInheritanceMap got modified
					TempCachedInheritanceBuffer.bDirty = true;
				}
			}
		}

		bool bRemovedDependencyData;
		State.RemoveAssetData(AssetData, true /* bRemoveDependencyData */, bRemoved, bRemovedDependencyData);
	}

	return bRemoved;
}

void FAssetRegistryImpl::RemovePackageData(Impl::FEventContext& EventContext, const FName PackageName)
{
	TArray<FAssetData*, TInlineAllocator<1>>* PackageAssetsPtr = State.CachedAssetsByPackageName.Find(PackageName);
	if (PackageAssetsPtr && PackageAssetsPtr->Num() > 0)
	{
		FAssetIdentifier PackageAssetIdentifier(PackageName);
		// If there were any EDependencyCategory::Package referencers, re-add them to a new empty dependency node, as it would be when the referencers are loaded from disk
		// We do not have to handle SearchableName or Manage referencers, because those categories of dependencies are not created for non-existent AssetIdentifiers
		TArray<TPair<FAssetIdentifier, FDependsNode::FPackageFlagSet>> PackageReferencers;
		{
			FDependsNode** FoundPtr = State.CachedDependsNodes.Find(PackageAssetIdentifier);
			FDependsNode* DependsNode = FoundPtr ? *FoundPtr : nullptr;
			if (DependsNode)
			{
				DependsNode->GetPackageReferencers(PackageReferencers);
			}
		}

		// Copy the array since RemoveAssetData may re-allocate it!
		TArray<FAssetData*, TInlineAllocator<1>> PackageAssets = *PackageAssetsPtr;
		for (FAssetData* PackageAsset : PackageAssets)
		{
			RemoveAssetData(EventContext, PackageAsset);
		}

		// Readd any referencers, creating an empty DependsNode to hold them
		if (PackageReferencers.Num())
		{
			FDependsNode* NewNode = State.CreateOrFindDependsNode(PackageAssetIdentifier);
			for (TPair<FAssetIdentifier, FDependsNode::FPackageFlagSet>& Pair : PackageReferencers)
			{
				FDependsNode* ReferencerNode = State.CreateOrFindDependsNode(Pair.Key);
				if (ReferencerNode != nullptr)
				{
					ReferencerNode->AddPackageDependencySet(NewNode, Pair.Value);
					NewNode->AddReferencer(ReferencerNode);
				}
			}
		}
	}
}

void FAssetRegistryImpl::AddVerseFile(Impl::FEventContext& EventContext, FName VerseFilePathToAdd)
{
	bool bAlreadyExists = false;
	CachedVerseFiles.Add(VerseFilePathToAdd, &bAlreadyExists);
	if (!bAlreadyExists)
	{
		FName VerseDirectoryPath(FPathViews::GetPath(WriteToString<256>(VerseFilePathToAdd)));
		TArray<FName>& FilePathsArray = CachedVerseFilesByPath.FindOrAdd(VerseDirectoryPath);
		FilePathsArray.Add(VerseFilePathToAdd);
		EventContext.VerseEvents.Emplace(VerseFilePathToAdd, Impl::FEventContext::EEvent::Added);
	}
}

void FAssetRegistryImpl::RemoveVerseFile(Impl::FEventContext& EventContext, FName VerseFilePathToRemove)
{
	if (CachedVerseFiles.Remove(VerseFilePathToRemove))
	{
		FName VerseDirectoryPath(FPathViews::GetPath(WriteToString<256>(VerseFilePathToRemove)));
		TArray<FName>* FilePathsArray = CachedVerseFilesByPath.Find(VerseDirectoryPath);
		if (ensure(FilePathsArray)) // We found it in CachedVerseFiles, so we must also find it here
		{
			FilePathsArray->Remove(VerseFilePathToRemove);
			if (FilePathsArray->IsEmpty())
			{
				CachedVerseFilesByPath.Remove(VerseDirectoryPath);
			}
		}
		EventContext.VerseEvents.Emplace(VerseFilePathToRemove, Impl::FEventContext::EEvent::Removed);
	}
}

}

#if WITH_EDITOR

void UAssetRegistryImpl::OnDirectoryChanged(const TArray<FFileChangeData>& FileChanges)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UAssetRegistryImpl::OnDirectoryChanged);

	double StartTime = FPlatformTime::Seconds();
	
	// Take local copy of FileChanges array as we wish to collapse pairs of 'Removed then Added' FileChangeData
	// entries into a single 'Modified' entry.
	TArray<FFileChangeData> FileChangesProcessed(FileChanges);

	for (int32 FileEntryIndex = 0; FileEntryIndex < FileChangesProcessed.Num(); FileEntryIndex++)
	{
		if (FileChangesProcessed[FileEntryIndex].Action == FFileChangeData::FCA_Added)
		{
			// Search back through previous entries to see if this Added can be paired with a previous Removed
			const FString& FilenameToCompare = FileChangesProcessed[FileEntryIndex].Filename;
			for (int32 SearchIndex = FileEntryIndex - 1; SearchIndex >= 0; SearchIndex--)
			{
				if (FileChangesProcessed[SearchIndex].Action == FFileChangeData::FCA_Removed &&
					FileChangesProcessed[SearchIndex].Filename == FilenameToCompare)
				{
					// Found a Removed which matches the Added - change the Added file entry to be a Modified...
					FileChangesProcessed[FileEntryIndex].Action = FFileChangeData::FCA_Modified;

					// ...and remove the Removed entry
					FileChangesProcessed.RemoveAt(SearchIndex);
					FileEntryIndex--;
					break;
				}
			}
		}
	}

	{
		// Check that the change is related to a directory that has actually been mounted.
		FStringBuilderBase MountPointPackageName;
		FStringBuilderBase MountPointFilePath;
		FStringBuilderBase RelativePath;
		for (int32 FileEntryIndex = FileChangesProcessed.Num() - 1; FileEntryIndex >= 0; FileEntryIndex--)
		{
			FFileChangeData& Data = FileChangesProcessed[FileEntryIndex];
			if (Data.Action != FFileChangeData::FCA_RescanRequired && !FPackageName::TryGetMountPointForPath(
				Data.Filename, MountPointPackageName, MountPointFilePath, RelativePath))
			{
				FileChangesProcessed.RemoveAt(FileEntryIndex);
			}
		}
	}

	UE::AssetRegistry::Impl::FEventContext EventContext;
	bool bInitialSearchStarted;
	bool bInitialSearchCompleted;
	{
		LLM_SCOPE(ELLMTag::AssetRegistry);
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		bInitialSearchStarted = GuardedData.IsInitialSearchStarted();
		bInitialSearchCompleted = GuardedData.IsInitialSearchCompleted();
		UE::AssetRegistry::Impl::FClassInheritanceContext InheritanceContext;
		UE::AssetRegistry::Impl::FClassInheritanceBuffer InheritanceBuffer;
		GetInheritanceContextWithRequiredLock(InterfaceScopeLock, InheritanceContext, InheritanceBuffer);
		GuardedData.OnDirectoryChanged(EventContext, InheritanceContext, FileChangesProcessed);
	}
	Broadcast(EventContext);
	
	FTelemetryRouter::Get().ProvideTelemetry<UE::Telemetry::AssetRegistry::FDirectoryWatcherUpdateTelemetry>({
		FileChanges,
		FPlatformTime::Seconds() - StartTime,
		bInitialSearchStarted,
		bInitialSearchCompleted,
	});
}

namespace UE::AssetRegistry
{

void FAssetRegistryImpl::OnDirectoryChanged(Impl::FEventContext& EventContext,
	Impl::FClassInheritanceContext& InheritanceContext, TArray<FFileChangeData>& FileChangesProcessed)
{
	TArray<FString> NewDirs;
	TArray<FString> NewFiles;
	TArray<FString> ModifiedFiles;
	for (int32 FileIdx = 0; FileIdx < FileChangesProcessed.Num(); ++FileIdx)
	{
		if (FileChangesProcessed[FileIdx].Action == FFileChangeData::FCA_RescanRequired)
		{
			if (bInitialSearchStarted && !bInitialSearchCompleted)
			{
				// Ignore rescan request during initial scan as it is probably caused by the scan itself
				UE_LOG(LogAssetRegistry, Log, TEXT("FAssetRegistry ignoring rescan request for %s during startup"), *FileChangesProcessed[FileIdx].Filename);
			}
			else
			{
				OnDirectoryRescanRequired(EventContext, InheritanceContext, FileChangesProcessed[FileIdx].Filename,
					FileChangesProcessed[FileIdx].TimeStamp);
			}

			continue;
		}
		FString LongPackageName;
		const FString File = FString(FileChangesProcessed[FileIdx].Filename);
		const bool bIsPackageFile = FPackageName::IsPackageExtension(*FPaths::GetExtension(File, true));
		const bool bIsValidPackageName = FPackageName::TryConvertFilenameToLongPackageName(File, LongPackageName);
		const bool bIsValidPackage = bIsPackageFile && bIsValidPackageName;

		if (bIsValidPackage)
		{
			FName LongPackageFName(*LongPackageName);

			bool bAddedOrCreated = false;
			switch (FileChangesProcessed[FileIdx].Action)
			{
			case FFileChangeData::FCA_Added:
				// This is a package file that was created on disk. Mark it to be scanned for asset data.
				NewFiles.AddUnique(File);
				bAddedOrCreated = true;
				UE_LOG(LogAssetRegistry, Verbose, TEXT("File was added to content directory: %s"), *File);
				break;

			case FFileChangeData::FCA_Modified:
				// This is a package file that changed on disk. Mark it to be scanned immediately for new or removed asset data.
				ModifiedFiles.AddUnique(File);
				bAddedOrCreated = true;
				UE_LOG(LogAssetRegistry, Verbose, TEXT("File changed in content directory: %s"), *File);
				break;

			case FFileChangeData::FCA_Removed:
				// This file was deleted. Remove all assets in the package from the registry.
				RemovePackageData(EventContext, LongPackageFName);
				// If the package was a package we were tracking as empty (due to e.g. a rename in editor), remove it.
				// Disk now matches editor
				RemoveEmptyPackage(LongPackageFName);
				UE_LOG(LogAssetRegistry, Verbose, TEXT("File was removed from content directory: %s"), *File);
				break;
			}
			if (bAddedOrCreated && CachedEmptyPackages.Contains(LongPackageFName))
			{
				UE_LOG(LogAssetRegistry, Warning, TEXT("%s: package was marked as deleted in editor, but has been modified on disk. It will once again be returned from AssetRegistry queries."),
					*File);
				RemoveEmptyPackage(LongPackageFName);
			}
		}
		else if (bIsValidPackageName)
		{
			// Is this a Verse file?
			if (FAssetDataGatherer::IsVerseFile(File))
			{
				switch (FileChangesProcessed[FileIdx].Action)
				{
				case FFileChangeData::FCA_Added:
					// This is a Verse file that was created on disk.
					NewFiles.AddUnique(File);
					UE_LOG(LogAssetRegistry, Verbose, TEXT("Verse file was added to content directory: %s"), *File);
					break;

				case FFileChangeData::FCA_Modified:
					// Note: Since content of Verse files is not scanned, no need to handle FCA_Modified
					break;

				case FFileChangeData::FCA_Removed:
					RemoveVerseFile(EventContext, FName(WriteToString<256>(LongPackageName, FPathViews::GetExtension(File, /*bIncludeDot*/ true))));
					UE_LOG(LogAssetRegistry, Verbose, TEXT("Verse file was removed from content directory: %s"), *File);
					break;
				}
			}
			else
			{
				// This could be a directory or possibly a file with no extension or a wrong extension.
				// No guaranteed way to know at this point since it may have been deleted.
				switch (FileChangesProcessed[FileIdx].Action)
				{
				case FFileChangeData::FCA_Added:
				{
					if (FPaths::DirectoryExists(File))
					{
						NewDirs.Add(File);
						UE_LOG(LogAssetRegistry, Verbose, TEXT("Directory was added to content directory: %s"), *File);
					}
					break;
				}
				case FFileChangeData::FCA_Removed:
				{
					FName Path(UE::String::RemoveFromEnd(FStringView(LongPackageName), TEXTVIEW("/")));
					RemoveAssetPath(EventContext, Path);
					UE_LOG(LogAssetRegistry, Verbose, TEXT("Directory was removed from content directory: %s"), *File);
					break;
				}
				default:
					break;
				}
			}
		}

#if WITH_EDITOR
		if (bIsValidPackageName)
		{
			// If a package changes in a referenced directory, modify the Assets that monitor that directory
			FString DirectoryPath = FPaths::CreateStandardFilename(FPaths::GetPath(File));
			// TODO: Change DirectoryReferencers to a FileNameMap so we can do this FindParentDirectory check more quickly
			TArray<FName, TInlineAllocator<1>> WatcherPackageNames;
			for (const TPair<FString, FName>& Pair : DirectoryReferencers)
			{
				if (FPathViews::IsParentPathOf(Pair.Key, DirectoryPath))
				{
					WatcherPackageNames.Add(Pair.Value);
				}
			}
			for (FName WatcherPackageName : WatcherPackageNames)
			{
				// ScanModifiedAssetFiles accepts LongPackageNames as well as LocalPaths
				ModifiedFiles.AddUnique(WatcherPackageName.ToString());
			}
		}
#endif
			
	}

	if (NewFiles.Num() || NewDirs.Num())
	{
		if (GlobalGatherer.IsValid())
		{
			for (FString& NewDir : NewDirs)
			{
				GlobalGatherer->OnDirectoryCreated(NewDir);
			}
			GlobalGatherer->OnFilesCreated(NewFiles);
			if (GlobalGatherer->IsSynchronous())
			{
				Impl::FScanPathContext Context(EventContext, InheritanceContext, NewDirs, NewFiles,
					false /* bForceRescan */, false /* bIgnoreDenyListScanFilters */, nullptr /* OutFoundAssets */);
				ScanPathsSynchronous(Context);
			}
		}
	}
	ScanModifiedAssetFiles(EventContext, InheritanceContext, ModifiedFiles);
}

void FAssetRegistryImpl::OnDirectoryRescanRequired(Impl::FEventContext& EventContext,
	Impl::FClassInheritanceContext& InheritanceContext, FString& DirPath, int64 BeforeTimeStamp)
{
	TArray<TPair<FString,FString>> DirPathsAndPackageNames;
	FString DirPathAsPackageName;
	FString NormalizedDirPath = FPaths::CreateStandardFilename(DirPath);
	if (FPackageName::TryConvertFilenameToLongPackageName(NormalizedDirPath, DirPathAsPackageName))
	{
		DirPathsAndPackageNames.Emplace(DirPath, MoveTemp(DirPathAsPackageName));
	}
	else
	{
		TArray<FString> ContentRoots;
		FPackageName::QueryRootContentPaths(ContentRoots);
		TStringBuilder<64> UnusedPackageName;
		TStringBuilder<256> MountedFilePath;
		TStringBuilder<16> UnusedRelPath;
		for (FString& MountedLongPackageName : ContentRoots)
		{
			if (FPackageName::TryGetMountPointForPath(MountedLongPackageName, UnusedPackageName, MountedFilePath, UnusedRelPath))
			{
				FString NormalizeMountedFilePath = FPaths::CreateStandardFilename(FString(MountedFilePath));
				if (FPaths::IsUnderDirectory(NormalizeMountedFilePath, NormalizedDirPath))
				{
					DirPathsAndPackageNames.Emplace(MoveTemp(NormalizeMountedFilePath), MoveTemp(MountedLongPackageName));
				}
			}
		}
	}
	if (DirPathsAndPackageNames.IsEmpty())
	{
		return;
	}

	struct FDirectoryResults
	{
		TArray<FString> NewFiles;
		TArray<FString> ModifiedFiles;
		TSet<FName> RemovedLongPackageNames;
	};
	int32 NumDirs = DirPathsAndPackageNames.Num();
	TArray<FDirectoryResults> Results;
	Results.SetNum(NumDirs);
	FDateTime BeforeDateTime = FDateTime::FromUnixTimestamp(BeforeTimeStamp);

	for (int32 DirIndex = 0; DirIndex < NumDirs; ++DirIndex)
	{
		FString& PackageNamePath = DirPathsAndPackageNames[DirIndex].Value;
		FDirectoryResults& Result = Results[DirIndex];
		EnumerateAssetsByPathNoTags(*PackageNamePath, [&Result](const FAssetData& AssetData)
			{
				Result.RemovedLongPackageNames.Add(AssetData.PackageName);
				return true;
			}, true /* bRecursive */, true /* bIncludeOnlyOnDiskAssets */);
	}

	ParallelFor(NumDirs, [this, &DirPathsAndPackageNames, &Results, &BeforeDateTime](int32 DirIndex)
		{
			FDirectoryResults& Result = Results[DirIndex];
			TPair<FString, FString>& Pair = DirPathsAndPackageNames[DirIndex];
			FString& LocalPath = Pair.Key;
			FString& PackageNamePath = Pair.Value;

			FPackageName::IteratePackagesInDirectory(LocalPath,
				[&LocalPath, &PackageNamePath, &BeforeDateTime, &Result]
				(const TCHAR* Filename, const FFileStatData& StatData)
				{
					// Convert Filename to a PackagePath. We know the base dir so its faster to use that than FPackageName
					// which has to scan all mount dirs
					FStringView RelPath;
					FString NormalizedFilename = FPaths::CreateStandardFilename(Filename);
					if (!FPathViews::TryMakeChildPathRelativeTo(NormalizedFilename, LocalPath, RelPath))
					{
						return true;
					}
					const bool bIsPackageFile = FPackageName::IsPackageExtension(
						*FString(FPathViews::GetExtension(RelPath, true /* bIncludeDot */)));
					RelPath = FPathViews::GetBaseFilenameWithPath(RelPath);
					TStringBuilder<256> FilePackagePath;
					FilePackagePath << PackageNamePath;
					FPathViews::AppendPath(FilePackagePath, RelPath);
					for (int32 Index = 0; Index < FilePackagePath.Len(); ++Index)
					{
						TCHAR& Char = FilePackagePath.GetData()[Index];
						if (Char == '\\')
						{
							Char = '/';
						}
					}
					const bool bIsValidPackageName = FPackageName::IsValidTextForLongPackageName(FilePackagePath);
					if (!bIsPackageFile || !bIsValidPackageName)
					{
						return true;
					}

					if (StatData.CreationTime > BeforeDateTime)
					{
						Result.NewFiles.Add(NormalizedFilename);
					}
					else if (StatData.ModificationTime > BeforeDateTime)
					{
						Result.ModifiedFiles.Add(NormalizedFilename);
					}
					Result.RemovedLongPackageNames.Remove(FName(FilePackagePath.ToView()));

					return true;
				});
		});

	TArray<FName> FinalRemovedLongPackageNames;
	FDirectoryResults& FinalResult = Results[0];
	FinalRemovedLongPackageNames.Append(FinalResult.RemovedLongPackageNames.Array());
	for (int32 DirIndex = 1; DirIndex < NumDirs; ++DirIndex)
	{
		FDirectoryResults& ResultToMerge = Results[DirIndex];
		FinalResult.NewFiles.Append(MoveTemp(ResultToMerge.NewFiles));
		FinalResult.ModifiedFiles.Append(MoveTemp(ResultToMerge.ModifiedFiles));
		FinalRemovedLongPackageNames.Append(ResultToMerge.RemovedLongPackageNames.Array());
	}

	for (FName LongPackageName : FinalRemovedLongPackageNames)
	{
		// This file was deleted. Remove all assets in the package from the registry.
		RemovePackageData(EventContext, LongPackageName);
		// If the package was a package we were tracking as empty (due to e.g. a rename in editor), remove it.
		// Disk now matches editor
		RemoveEmptyPackage(LongPackageName);
	}
	if (FinalResult.NewFiles.Num())
	{
		if (GlobalGatherer.IsValid())
		{
			GlobalGatherer->OnFilesCreated(FinalResult.NewFiles);
			if (GlobalGatherer->IsSynchronous())
			{
				TArray<FString> UnusedNewDirs;
				Impl::FScanPathContext Context(EventContext, InheritanceContext, UnusedNewDirs, FinalResult.NewFiles,
					false /* bForceRescan */, false /* bIgnoreDenyListScanFilters */, nullptr /* OutFoundAssets */);
				ScanPathsSynchronous(Context);
			}
		}
	}
	ScanModifiedAssetFiles(EventContext, InheritanceContext, FinalResult.ModifiedFiles);
}

}

void UAssetRegistryImpl::OnAssetLoaded(UObject *AssetLoaded)
{
	LLM_SCOPE(ELLMTag::AssetRegistry);
	FWriteScopeLock InterfaceScopeLock(InterfaceLock);
	GuardedData.AddLoadedAssetToProcess(*AssetLoaded);
}

void UAssetRegistryImpl::ProcessLoadedAssetsToUpdateCache(UE::AssetRegistry::Impl::FEventContext& EventContext,
	const double TickStartTime, UE::AssetRegistry::Impl::EGatherStatus Status)
{
	// Note this function can be reentered due to arbitrary code execution in construction of FAssetData
	if (!IsInGameThread())
	{
		// Calls to GetAssetRegistryTags are only allowed on the GameThread
		return;
	}

	// Early exit to save cputime if we're still processing cache data
	const bool bFlushFullBuffer = TickStartTime < 0;
	if (Status == UE::AssetRegistry::Impl::EGatherStatus::Active && !bFlushFullBuffer)
	{
		return;
	}

	constexpr int32 BatchSize = 16;
	TArray<const UObject*> BatchObjects;
	TArray<FAssetData, TInlineAllocator<BatchSize>> BatchAssetDatas;

	{
		LLM_SCOPE(ELLMTag::AssetRegistry);
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		GuardedData.GetProcessLoadedAssetsBatch(BatchObjects, BatchSize, bUpdateDiskCacheAfterLoad);
		if (BatchObjects.Num() == 0)
		{
			return;
		}

		// Refreshes ClassGeneratorNames if out of date due to module load
		GuardedData.CollectCodeGeneratorClasses();
	}

	while (BatchObjects.Num() > 0)
	{
		bool bTimedOut = false;
		int32 CurrentBatchSize = BatchObjects.Num();
		BatchAssetDatas.Reset(CurrentBatchSize);
		int32 Index = 0;
		while (Index < CurrentBatchSize)
		{
			const UObject* LoadedObject = BatchObjects[Index++];
			if (!LoadedObject->IsAsset())
			{
				// If the object has changed and is no longer an asset, ignore it. This can happen when an Actor is modified during cooking to no longer have an external package
				continue;
			}
			BatchAssetDatas.Add(FAssetData(LoadedObject, FAssetData::ECreationFlags::AllowBlueprintClass,
				EAssetRegistryTagsCaller::AssetRegistryLoad));

			// Check to see if we have run out of time in this tick
			if (!bFlushFullBuffer &&
				(FPlatformTime::Seconds() - TickStartTime) > UE::AssetRegistry::Impl::MaxSecondsPerFrame)
			{
				bTimedOut = true;
				break;
			}
		}

		LLM_SCOPE(ELLMTag::AssetRegistry);
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		GuardedData.PushProcessLoadedAssetsBatch(EventContext, BatchAssetDatas,
			TArrayView<const UObject*>(BatchObjects).Slice(Index, CurrentBatchSize-Index));
		if (bTimedOut)
		{
			break;
		}
		GuardedData.GetProcessLoadedAssetsBatch(BatchObjects, BatchSize, bUpdateDiskCacheAfterLoad);
	}
}

namespace UE::AssetRegistry
{

void FAssetRegistryImpl::AddLoadedAssetToProcess(const UObject& AssetLoaded)
{
	LoadedAssetsToProcess.Add(&AssetLoaded);
}

void FAssetRegistryImpl::GetProcessLoadedAssetsBatch(TArray<const UObject*>& OutLoadedAssets, uint32 BatchSize,
	bool bUpdateDiskCacheAfterLoad)
{
	if (!GlobalGatherer.IsValid() || !bUpdateDiskCacheAfterLoad)
	{
		OutLoadedAssets.Reset();
		return;
	}

	OutLoadedAssets.Reset(BatchSize);
	while (!LoadedAssetsToProcess.IsEmpty() && OutLoadedAssets.Num() < static_cast<int32>(BatchSize))
	{
		const UObject* LoadedAsset = LoadedAssetsToProcess.PopFrontValue().Get();
		if (!LoadedAsset)
		{
			// This could be null, in which case it already got freed, ignore
			continue;
		}

		// Take a new snapshot of the asset's data every time it loads or saves

		UPackage* InMemoryPackage = LoadedAsset->GetOutermost();
		if (InMemoryPackage->IsDirty())
		{
			// Package is dirty, which means it has changes other than just a PostLoad
			// In editor, ignore the update of the asset; it will be updated when saved
			// In the cook commandlet, in which editoruser-created changes are impossible, do the update anyway.
			// Occurrences of IsDirty in the cook commandlet are spurious and a code bug.
			if (!IsRunningCookCommandlet())
			{
				continue;
			}
		}

		OutLoadedAssets.Add(LoadedAsset);
	}
}

void FAssetRegistryImpl::PushProcessLoadedAssetsBatch(Impl::FEventContext& EventContext,
	TArrayView<FAssetData> LoadedAssetDatas, TArrayView<const UObject*> UnprocessedFromBatch)
{
	// Add or update existing for all of the AssetDatas created by the batch
	for (FAssetData& NewAssetData : LoadedAssetDatas)
	{
		if (ShouldSkipGatheredAsset(NewAssetData))
		{
			continue;
		}
		FCachedAssetKey Key(NewAssetData);
		FAssetData** DataFromGather = State.CachedAssets.Find(Key);

		AssetDataObjectPathsUpdatedOnLoad.Add(NewAssetData.GetSoftObjectPath());

		if (!DataFromGather)
		{
			FAssetData* ClonedAssetData = new FAssetData(MoveTemp(NewAssetData));
			AddAssetData(EventContext, ClonedAssetData);
		}
		else
		{
			// When updating disk-based AssetData with the AssetData from a loaded UObject, we keep
			// existing tags from disk even if they are not returned from the
			// GetAssetRegistryTags(EAssetRegistryTagsCaller::AssetRegistryLoad) function on the loaded UObject.
			// We do this because the tags might be tags that are only calculated during
			// GetAssetRegistryTags(EAssetRegistryTagsCaller::SavePackage).
			// Modified tag values on the other hand do overwrite the old values from disk.
			// This means that the only way to delete no-longer present tags from an AssetData
			// is to resave the package, or to manually call AssetUpdateTags(EAssetRegistryTagsCaller::FullUpdate) from c++.
			UpdateAssetData(EventContext, *DataFromGather, MoveTemp(NewAssetData), true /* bKeepDeletedTags */);
		}
	}

	// Push back any objects from the batch that were not processed due to timing out
	for (const UObject* Obj : ReverseIterate(UnprocessedFromBatch))
	{
		LoadedAssetsToProcess.EmplaceFront(Obj);
	}
}


void FAssetRegistryImpl::UpdateRedirectCollector()
{
	// Look for all redirectors in list
	const TArray<const FAssetData*>& RedirectorAssets = State.GetAssetsByClassPathName(UObjectRedirector::StaticClass()->GetClassPathName());

	for (const FAssetData* AssetData : RedirectorAssets)
	{
		FSoftObjectPath Source = AssetData->GetSoftObjectPath();
		FSoftObjectPath Destination = GetRedirectedObjectPath(Source, nullptr, nullptr, /*bNeedsScanning*/ false);

		if (Destination != Source)
		{
			GRedirectCollector.AddAssetPathRedirection(Source, Destination);
		}
	}
}

}

#endif // WITH_EDITOR

void UAssetRegistryImpl::ScanModifiedAssetFiles(const TArray<FString>& InFilePaths)
{
	UE::AssetRegistry::Impl::FEventContext EventContext;
	{
		LLM_SCOPE(ELLMTag::AssetRegistry);
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		UE::AssetRegistry::Impl::FClassInheritanceContext InheritanceContext;
		UE::AssetRegistry::Impl::FClassInheritanceBuffer InheritanceBuffer;
		GetInheritanceContextWithRequiredLock(InterfaceScopeLock, InheritanceContext, InheritanceBuffer);
		GuardedData.ScanModifiedAssetFiles(EventContext, InheritanceContext, InFilePaths);
	}

#if WITH_EDITOR
	// Our caller expects up to date results after calling this function,
	// but in-memory results will override the on-disk results we just scanned,
	// and our in-memory results might be out of date due to being queued but not yet processed.
	// So ProcessLoadedAssetsToUpdateCache before returning to make sure results are up to date.
	ProcessLoadedAssetsToUpdateCache(EventContext, -1., UE::AssetRegistry::Impl::EGatherStatus::Complete);
#endif

	Broadcast(EventContext);
}

namespace UE::AssetRegistry
{

void FAssetRegistryImpl::ScanModifiedAssetFiles(Impl::FEventContext& EventContext,
	Impl::FClassInheritanceContext& InheritanceContext, const TArray<FString>& InFilePaths)
{
	if (InFilePaths.Num() > 0)
	{
		// Convert all the filenames to package names
		TArray<FString> ModifiedPackageNames;
		ModifiedPackageNames.Reserve(InFilePaths.Num());
		for (const FString& File : InFilePaths)
		{
			ModifiedPackageNames.Add(FPackageName::FilenameToLongPackageName(File));
		}

		// Get the assets that are currently inside the package
		TArray<FSoftObjectPath> ExistingAssetDatas;
		ExistingAssetDatas.Reserve(InFilePaths.Num());
		for (const FString& PackageName : ModifiedPackageNames)
		{
			TArray<FAssetData*, TInlineAllocator<1>>* PackageAssetsPtr = State.CachedAssetsByPackageName.Find(*PackageName);
			if (PackageAssetsPtr && PackageAssetsPtr->Num() > 0)
			{
				ExistingAssetDatas.Reserve(ExistingAssetDatas.Num() + PackageAssetsPtr->Num());
				for (FAssetData* AssetData : *PackageAssetsPtr)
				{
					ExistingAssetDatas.Add(AssetData->ToSoftObjectPath());
				}
			}
		}

		// Re-scan and update the asset registry with the new asset data
		TArray<FSoftObjectPath> FoundAssets;
		Impl::FScanPathContext Context(EventContext, InheritanceContext, TArray<FString>(), InFilePaths,
			true /* bForceRescan */, false /* bIgnoreDenyListScanFilters */, &FoundAssets);
		ScanPathsSynchronous(Context);

		// Remove any assets that are no longer present in the package
		for (FSoftObjectPath& OldAssetPath : ExistingAssetDatas)
		{
			if (!FoundAssets.Contains(OldAssetPath))
			{
				FAssetData* OldAssetData = const_cast<FAssetData*>(State.GetAssetByObjectPath(OldAssetPath));
				if (OldAssetData)
				{
					RemoveAssetData(EventContext, OldAssetData);
				}
			}
		}

		// Send ModifiedOnDisk event for every Asset that was modified
		for (const FSoftObjectPath& FoundAsset : FoundAssets)
		{
			FAssetData** AssetData = State.CachedAssets.Find(FCachedAssetKey(FoundAsset));
			if (AssetData)
			{
				EventContext.AssetEvents.Emplace(**AssetData, Impl::FEventContext::EEvent::UpdatedOnDisk);
			}
		}
	}
}

}

void UAssetRegistryImpl::OnContentPathMounted(const FString& InAssetPath, const FString& FileSystemPath)
{
	// Sanitize
	FString AssetPathWithTrailingSlash;
	if (!InAssetPath.EndsWith(TEXT("/"), ESearchCase::CaseSensitive))
	{
		// We actually want a trailing slash here so the path can be properly converted while searching for assets
		AssetPathWithTrailingSlash = InAssetPath + TEXT("/");
	}
	else
	{
		AssetPathWithTrailingSlash = InAssetPath;
	}

#if WITH_EDITOR
	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	IDirectoryWatcher* DirectoryWatcher = nullptr;
	if (GIsEditor) 	// In-game doesn't listen for directory changes
	{
		LLM_SCOPE(ELLMTag::AssetRegistry);
		DirectoryWatcher = DirectoryWatcherModule.Get();
		if (DirectoryWatcher)
		{
			// Make sure the directory exits on disk so that the OS level DirectoryWatcher can be used to monitor it.
			IPlatformFile::GetPlatformPhysical().CreateDirectoryTree(*FileSystemPath);
		}
	}
		
#endif

	UE::AssetRegistry::Impl::FEventContext EventContext;
	{
		LLM_SCOPE(ELLMTag::AssetRegistry);
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		UE::AssetRegistry::Impl::FClassInheritanceContext InheritanceContext;
		UE::AssetRegistry::Impl::FClassInheritanceBuffer InheritanceBuffer;
		GetInheritanceContextWithRequiredLock(InterfaceScopeLock, InheritanceContext, InheritanceBuffer);
		GuardedData.OnContentPathMounted(EventContext, InheritanceContext, InAssetPath, AssetPathWithTrailingSlash,
			FileSystemPath);

		// Listen for directory changes in this content path
#if WITH_EDITOR
		const FString StandardFileSystemPath = FPaths::CreateStandardFilename(FileSystemPath);
		// In-game doesn't listen for directory changes
		if (DirectoryWatcher && !IsDirAlreadyWatchedByRootWatchers(StandardFileSystemPath))
		{
			if (!OnDirectoryChangedDelegateHandles.Contains(AssetPathWithTrailingSlash))
			{
				FDelegateHandle NewHandle;
				DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(
					StandardFileSystemPath,
					IDirectoryWatcher::FDirectoryChanged::CreateUObject(this, &UAssetRegistryImpl::OnDirectoryChanged),
					NewHandle, 
					IDirectoryWatcher::WatchOptions::IncludeDirectoryChanges);

				OnDirectoryChangedDelegateHandles.Add(AssetPathWithTrailingSlash, NewHandle);
			}
		}
#endif // WITH_EDITOR
	}

	Broadcast(EventContext);
}

namespace UE::AssetRegistry
{

void FAssetRegistryImpl::OnContentPathMounted(Impl::FEventContext& EventContext,
	Impl::FClassInheritanceContext& InheritanceContext, const FString& InAssetPath,
	const FString& AssetPathWithTrailingSlash, const FString& FileSystemPath)
{
	// Content roots always exist
	AddPath(EventContext, UE::String::RemoveFromEnd(FStringView(AssetPathWithTrailingSlash), TEXTVIEW("/")));

	if (GlobalGatherer.IsValid() && bSearchAllAssets)
	{
		if (GlobalGatherer->IsSynchronous())
		{
			Impl::FScanPathContext Context(EventContext, InheritanceContext, { FileSystemPath }, TArray<FString>());
			ScanPathsSynchronous(Context);
		}
		else
		{
			GlobalGatherer->AddMountPoint(FileSystemPath, InAssetPath);
			GlobalGatherer->SetIsOnAllowList(FileSystemPath, true);
		}
	}
}

}

void UAssetRegistryImpl::OnContentPathDismounted(const FString& InAssetPath, const FString& FileSystemPath)
{
	// Sanitize
	FString AssetPathNoTrailingSlash = InAssetPath;
	if (AssetPathNoTrailingSlash.EndsWith(TEXT("/"), ESearchCase::CaseSensitive))
	{
		// We don't want a trailing slash here as it could interfere with RemoveAssetPath
		AssetPathNoTrailingSlash.LeftChopInline(1, EAllowShrinking::No);
	}

#if WITH_EDITOR
	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	IDirectoryWatcher* DirectoryWatcher = nullptr;
	if (GIsEditor) 	// In-game doesn't listen for directory changes
	{
		DirectoryWatcher = DirectoryWatcherModule.Get();
	}
#endif

	UE::AssetRegistry::Impl::FEventContext EventContext;
	{
		LLM_SCOPE(ELLMTag::AssetRegistry);
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		GuardedData.OnContentPathDismounted(EventContext, InAssetPath, AssetPathNoTrailingSlash, FileSystemPath);

		// Stop listening for directory changes in this content path
#if WITH_EDITOR
		const FString StandardFileSystemPath = FPaths::CreateStandardFilename(FileSystemPath);
		if (DirectoryWatcher && !IsDirAlreadyWatchedByRootWatchers(StandardFileSystemPath))
		{
			// Make sure OnDirectoryChangedDelegateHandles key is symmetrical with the one used in OnContentPathMounted
			FString AssetPathWithTrailingSlash;
			if (!InAssetPath.EndsWith(TEXT("/"), ESearchCase::CaseSensitive))
			{
				AssetPathWithTrailingSlash = InAssetPath + TEXT("/");
			}
			else
			{
				AssetPathWithTrailingSlash = InAssetPath;
			}

			FDelegateHandle DirectoryChangedHandle;
			if (ensure(OnDirectoryChangedDelegateHandles.RemoveAndCopyValue(AssetPathWithTrailingSlash, DirectoryChangedHandle)))
			{
				DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(StandardFileSystemPath, DirectoryChangedHandle);
			}
		}
#endif // WITH_EDITOR
	}
	Broadcast(EventContext);
}

namespace UE::AssetRegistry
{

void FAssetRegistryImpl::OnContentPathDismounted(Impl::FEventContext& EventContext, const FString& InAssetPath, const FString& AssetPathNoTrailingSlash, const FString& FileSystemPath)
{
	if (GlobalGatherer.IsValid())
	{
		GlobalGatherer->RemoveMountPoint(FileSystemPath);
	}

	FName MountPoint = FName(FStringView(AssetPathNoTrailingSlash));
	if (PersistentMountPoints.Contains(MountPoint))
	{
		// This path is marked to never remove its AssetDatas. Skip the code below to remove it.
		return;
	}

	// Remove all cached assets and Verse files found at this location
	{
		FName AssetPathNoTrailingSlashFName(*AssetPathNoTrailingSlash);
		TArray<FAssetData*> AllAssetDataToRemove;
		TSet<FName> PathList;
		const bool bRecurse = true;
		CachedPathTree.GetSubPaths(AssetPathNoTrailingSlashFName, PathList, bRecurse);
		PathList.Add(AssetPathNoTrailingSlashFName);
		for (FName PathName : PathList)
		{
			// Gather assets
			TArray<FAssetData*>* AssetsInPath = State.CachedAssetsByPath.Find(PathName);
			if (AssetsInPath)
			{
				AllAssetDataToRemove.Append(*AssetsInPath);
			}

			// Forget Verse files
			const TArray<FName>* VerseFilesInPath = CachedVerseFilesByPath.Find(PathName);
			if (VerseFilesInPath)
			{
				for (FName FilePath : *VerseFilesInPath)
				{
					CachedVerseFiles.Remove(FilePath);
				}
				CachedVerseFilesByPath.Remove(PathName);
			}
		}

		for (FAssetData* AssetData : AllAssetDataToRemove)
		{
			RemoveAssetData(EventContext, AssetData);
		}
	}

	// Remove the root path
	{
		const bool bEvenIfAssetsStillExist = true;
		RemoveAssetPath(EventContext, FName(*AssetPathNoTrailingSlash), bEvenIfAssetsStillExist);
	}
}

void FAssetRegistryImpl::UpdatePersistentMountPoints()
{
	State.EnumerateAllPaths([this](FName Path)
		{
			TStringBuilder<256> PathString(InPlace, Path);
			bool bHadClassesPrefix;
			FStringView MountPoint = FPathViews::GetMountPointNameFromPath(PathString, &bHadClassesPrefix, false /* bInWithoutSlashes*/);
			if (!MountPoint.IsEmpty() && !bHadClassesPrefix)
			{
				// Format returned by GetMountPointNameFromPath is e.g. /Engine, which is the format we need:
				// LongPackageName with no trailing slash
				PersistentMountPoints.Add(FName(MountPoint));
			}
		});
}

}

void UAssetRegistryImpl::SetTemporaryCachingMode(bool bEnable)
{
	checkf(IsInGameThread(), TEXT("Changing Caching mode is only available on the game thread because it affects behavior on all threads"));
	LLM_SCOPE(ELLMTag::AssetRegistry);
	FWriteScopeLock InterfaceScopeLock(InterfaceLock);
	GuardedData.SetTemporaryCachingMode(bEnable);
}

namespace UE::AssetRegistry
{

void FAssetRegistryImpl::SetTemporaryCachingMode(bool bEnable)
{
	if (bIsTempCachingAlwaysEnabled || bEnable == bIsTempCachingEnabled)
	{
		return;
	}

	bIsTempCachingEnabled = bEnable;
	TempCachedInheritanceBuffer.bDirty = true;
	if (!bEnable)
	{
		TempCachedInheritanceBuffer.Clear();
	}
}

}

void UAssetRegistryImpl::SetTemporaryCachingModeInvalidated()
{
	checkf(IsInGameThread(), TEXT("Invalidating temporary cache is only available on the game thread because it affects behavior on all threads"));
	LLM_SCOPE(ELLMTag::AssetRegistry);
	FWriteScopeLock InterfaceScopeLock(InterfaceLock);
	GuardedData.SetTemporaryCachingModeInvalidated();
}

namespace UE::AssetRegistry
{

void FAssetRegistryImpl::SetTemporaryCachingModeInvalidated()
{
	TempCachedInheritanceBuffer.bDirty = true;
}

}

bool UAssetRegistryImpl::GetTemporaryCachingMode() const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	return GuardedData.IsTempCachingEnabled();
}

namespace UE::AssetRegistry
{

void FAssetRegistryImpl::AddCachedBPClassParent(const FTopLevelAssetPath& ClassPath, const FTopLevelAssetPath& NotYetRedirectedParentPath)
{
	// We do not check for CoreRedirects for ClassPath, because this function is only called on behalf of ClassPath being loaded,
	// and the code author would have changed the package containing ClassPath to match the redirect they added.
	// But we do need to check for CoreRedirects in the ParentPath, because when a parent class is renamed, we do not resave
	// all packages containing subclasses to update their FBlueprintTags::ParentClassPath AssetData tags.
	FTopLevelAssetPath ParentPath = NotYetRedirectedParentPath;
#if WITH_EDITOR
	FCoreRedirectObjectName RedirectedParentObjectName = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Class,
		FCoreRedirectObjectName(NotYetRedirectedParentPath.GetAssetName(), NAME_None, NotYetRedirectedParentPath.GetPackageName()));
	if (!RedirectedParentObjectName.OuterName.IsNone())
	{
		UE_LOG(LogAssetRegistry, Error,
			TEXT("Class redirect exists from %s -> %s, which is invalid because ClassNames must be TopLevelAssetPaths. ")
			TEXT("Redirect will be ignored in AssetRegistry queries."),
			*NotYetRedirectedParentPath.ToString(), *RedirectedParentObjectName.ToString());
	}
	else
	{
		ParentPath = FTopLevelAssetPath(RedirectedParentObjectName.PackageName, RedirectedParentObjectName.ObjectName);
	}
#endif
	CachedBPInheritanceMap.Add(ClassPath, ParentPath);
}

void FAssetRegistryImpl::UpdateInheritanceBuffer(Impl::FClassInheritanceBuffer& OutBuffer) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UAssetRegistryImpl::UpdateTemporaryCaches)
	LLM_SCOPE(ELLMTag::AssetRegistry);
	UE_TRACK_REFERENCING_OPNAME_SCOPED(PackageAccessTrackingOps::NAME_ResetContext);

	TMap<UClass*, TSet<UClass*>> NativeSubclasses = GetAllDerivedClasses();

	uint32 NumNativeClasses = 1; // UObject has no superclass
	for (const TPair<UClass*, TSet<UClass*>>& Pair : NativeSubclasses)
	{
		NumNativeClasses += Pair.Value.Num();
	}
	OutBuffer.InheritanceMap.Reserve(NumNativeClasses + CachedBPInheritanceMap.Num());
	OutBuffer.InheritanceMap = CachedBPInheritanceMap;
	OutBuffer.InheritanceMap.Add(FTopLevelAssetPath(TEXT("/Script.CoreUObject"), TEXT("Object")), FTopLevelAssetPath());

	for (TPair<FTopLevelAssetPath, TArray<FTopLevelAssetPath>>& Pair : OutBuffer.ReverseInheritanceMap)
	{
		Pair.Value.Reset();
	}
	OutBuffer.ReverseInheritanceMap.Reserve(NativeSubclasses.Num());

	for (const TPair<UClass*, TSet<UClass*>>& Pair : NativeSubclasses)
	{
		FTopLevelAssetPath SuperclassName = Pair.Key->GetClassPathName();

		TArray<FTopLevelAssetPath>* OutputSubclasses = &OutBuffer.ReverseInheritanceMap.FindOrAdd(SuperclassName);
		OutputSubclasses->Reserve(Pair.Value.Num());
		for (UClass* Subclass : Pair.Value)
		{
			if (!Subclass->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists))
			{
				FTopLevelAssetPath SubclassName = Subclass->GetClassPathName();
				OutputSubclasses->Add(SubclassName);
				OutBuffer.InheritanceMap.Add(SubclassName, SuperclassName);

				if (Subclass->Interfaces.Num())
				{
					// Add any implemented interfaces to the reverse inheritance map, but not to the forward map
					for (const FImplementedInterface& Interface : Subclass->Interfaces)
					{
						if (UClass* InterfaceClass = Interface.Class) // could be nulled out by ForceDelete of a blueprint interface
						{
							TArray<FTopLevelAssetPath>& Implementations = OutBuffer.ReverseInheritanceMap.FindOrAdd(InterfaceClass->GetClassPathName());
							Implementations.Add(SubclassName);
						}
					}

					// Refetch OutputSubClasses from ReverseInheritanceMap because we just modified the ReverseInheritanceMap and may have resized
					OutputSubclasses = OutBuffer.ReverseInheritanceMap.Find(SuperclassName);
					check(OutputSubclasses); // It was added above
				}
			}
		}
	}

	// Add non-native classes to reverse map
	for (const TPair<FTopLevelAssetPath, FTopLevelAssetPath>& Kvp : CachedBPInheritanceMap)
	{
		const FTopLevelAssetPath& ParentClassName = Kvp.Value;
		if (!ParentClassName.IsNull())
		{
			TArray<FTopLevelAssetPath>& ChildClasses = OutBuffer.ReverseInheritanceMap.FindOrAdd(ParentClassName);
			ChildClasses.Add(Kvp.Key);
		}

	}

	OutBuffer.SavedAllClassesVersionNumber = GetCurrentAllClassesVersionNumber();
	OutBuffer.bDirty = false;
}

}

void UAssetRegistryImpl::GetInheritanceContextWithRequiredLock(FRWScopeLock& InOutScopeLock,
	UE::AssetRegistry::Impl::FClassInheritanceContext& InheritanceContext,
	UE::AssetRegistry::Impl::FClassInheritanceBuffer& StackBuffer)
{
	using namespace UE::AssetRegistry;

	uint64 CurrentGeneratorClassesVersionNumber = FAssetRegistryImpl::GetCurrentGeneratorClassesVersionNumber();
	uint64 CurrentAllClassesVersionNumber = FAssetRegistryImpl::GetCurrentAllClassesVersionNumber();
	bool bNeedsWriteLock = false;
	if (GuardedData.GetSavedGeneratorClassesVersionNumber() != CurrentGeneratorClassesVersionNumber)
	{
		// ConditionalUpdate writes to protected data in CollectCodeGeneratorClasses, so we cannot proceed under a read lock
		bNeedsWriteLock = true;
	}
	if (GuardedData.IsTempCachingEnabled() &&
		!GuardedData.GetTempCachedInheritanceBuffer().IsUpToDate(CurrentAllClassesVersionNumber))
	{
		// Temp caching is enabled, so we will be reading the protected data in TempCachedInheritanceBuffer
		// It's out of date, so we need to write to it first, so we cannot proceed under a read lock
		bNeedsWriteLock = true;
	}
	if (bNeedsWriteLock)
	{
		InOutScopeLock.ReleaseReadOnlyLockAndAcquireWriteLock_USE_WITH_CAUTION();
	}

	// Note that we have to reread all data since we may have dropped the lock
	GetInheritanceContextAfterVerifyingLock(CurrentGeneratorClassesVersionNumber, CurrentAllClassesVersionNumber,
		InheritanceContext, StackBuffer);
}

void UAssetRegistryImpl::GetInheritanceContextWithRequiredLock(FWriteScopeLock& InOutScopeLock,
	UE::AssetRegistry::Impl::FClassInheritanceContext& InheritanceContext,
	UE::AssetRegistry::Impl::FClassInheritanceBuffer& StackBuffer)
{
	using namespace UE::AssetRegistry;

	uint64 CurrentGeneratorClassesVersionNumber = FAssetRegistryImpl::GetCurrentGeneratorClassesVersionNumber();
	uint64 CurrentAllClassesVersionNumber = FAssetRegistryImpl::GetCurrentAllClassesVersionNumber();
	GetInheritanceContextAfterVerifyingLock(CurrentGeneratorClassesVersionNumber, CurrentAllClassesVersionNumber,
		InheritanceContext, StackBuffer);
}

void UAssetRegistryImpl::GetInheritanceContextAfterVerifyingLock(uint64 CurrentGeneratorClassesVersionNumber,
	uint64 CurrentAllClassesVersionNumber,
	UE::AssetRegistry::Impl::FClassInheritanceContext& InheritanceContext,
	UE::AssetRegistry::Impl::FClassInheritanceBuffer& StackBuffer)
{
	// If bIsTempCachingAlwaysEnabled, then we are guaranteed that bIsTempCachingEnabled=true.
	// We rely on this to simplify logic and only check bIsTempCachingEnabled
	check(!GuardedData.IsTempCachingAlwaysEnabled() || GuardedData.IsTempCachingEnabled());

	bool bCodeGeneratorClassesUpToDate = GuardedData.GetSavedGeneratorClassesVersionNumber() == CurrentGeneratorClassesVersionNumber;
	if (GuardedData.IsTempCachingEnabled())
	{
		// Use the persistent buffer
		UE::AssetRegistry::Impl::FClassInheritanceBuffer& TempCachedInheritanceBuffer = GuardedData.GetTempCachedInheritanceBuffer();
		bool bInheritanceMapUpToDate = TempCachedInheritanceBuffer.IsUpToDate(CurrentAllClassesVersionNumber);
		InheritanceContext.BindToBuffer(TempCachedInheritanceBuffer, GuardedData, bInheritanceMapUpToDate, bCodeGeneratorClassesUpToDate);
	}
	else
	{
		// Use the StackBuffer for the duration of the caller
		InheritanceContext.BindToBuffer(StackBuffer, GuardedData, false /* bInInheritanceMapUpToDate */, bCodeGeneratorClassesUpToDate);
	}
}

#if WITH_EDITOR
void UAssetRegistryImpl::OnGetExtraObjectTags(FAssetRegistryTagsContext Context)
{
	if (bAddMetaDataTagsToOnGetExtraObjectTags)
	{
		// It is critical that bIncludeOnlyOnDiskAssets=true otherwise this will cause an infinite loop
		const FAssetData AssetData = GetAssetByObjectPath(FSoftObjectPath(Context.GetObject()), /*bIncludeOnlyOnDiskAssets=*/true);
		// Adding metadata tags from disk is only necessary for cooked assets; uncooked assets still have the metadata and add them elsewhere
		// in UObject::GetAssetRegistryTags. Adding the tags from disk into uncooked assets would make the tags impossible to remove when
		// the uncooked assets are resaved.
		if ((AssetData.PackageFlags & PKG_Cooked) != 0)
		{
			TSet<FName>& MetaDataTags = UObject::GetMetaDataTagsForAssetRegistry();
			for (const FName MetaDataTag : MetaDataTags)
			{
				auto OutTagsContainsTagPredicate = [MetaDataTag](const UObject::FAssetRegistryTag& Tag) { return Tag.Name == MetaDataTag; };
				if (!Context.ContainsTag(MetaDataTag))
				{
					FAssetTagValueRef TagValue = AssetData.TagsAndValues.FindTag(MetaDataTag);
					if (TagValue.IsSet())
					{
						Context.AddTag(UObject::FAssetRegistryTag(MetaDataTag, TagValue.AsString(), UObject::FAssetRegistryTag::TT_Alphabetical));
					}
				}
			}
		}
	}
}

bool UAssetRegistryImpl::IsDirAlreadyWatchedByRootWatchers(const FString& Directory) const
{
	return DirectoryWatchRoots.ContainsByPredicate([&Directory](const FString& WatchRoot) -> bool {
		return FPaths::IsUnderDirectory(Directory, WatchRoot);
	});
}

#endif

namespace UE::AssetRegistry
{

namespace Impl
{

void FClassInheritanceBuffer::Clear()
{
	InheritanceMap.Empty();
	ReverseInheritanceMap.Empty();
}

bool FClassInheritanceBuffer::IsUpToDate(uint64 CurrentAllClassesVersionNumber) const
{
	return !bDirty && SavedAllClassesVersionNumber == CurrentAllClassesVersionNumber;
}

SIZE_T FClassInheritanceBuffer::GetAllocatedSize() const
{
	return InheritanceMap.GetAllocatedSize() + ReverseInheritanceMap.GetAllocatedSize();
}

void FClassInheritanceContext::BindToBuffer(FClassInheritanceBuffer& InBuffer, FAssetRegistryImpl& InAssetRegistryImpl, 
	bool bInInheritanceMapUpToDate, bool bInCodeGeneratorClassesUpToDate)
{
	AssetRegistryImpl = &InAssetRegistryImpl;
	Buffer = &InBuffer;
	bInheritanceMapUpToDate = bInInheritanceMapUpToDate;
	bCodeGeneratorClassesUpToDate = bInCodeGeneratorClassesUpToDate;
}

void FClassInheritanceContext::ConditionalUpdate()
{
	check(Buffer != nullptr); // It is not valid to call ConditionalUpdate with an empty FClassInheritanceContext
	if (bInheritanceMapUpToDate)
	{
		return;
	}

	if (!bCodeGeneratorClassesUpToDate)
	{
		AssetRegistryImpl->CollectCodeGeneratorClasses();
		bCodeGeneratorClassesUpToDate = true;
	}
	AssetRegistryImpl->UpdateInheritanceBuffer(*Buffer);
	bInheritanceMapUpToDate = true;
}

}

void FAssetRegistryImpl::GetSubClasses(Impl::FClassInheritanceContext& InheritanceContext,
	const TArray<FTopLevelAssetPath>& InClassNames, const TSet<FTopLevelAssetPath>& ExcludedClassNames, TSet<FTopLevelAssetPath>& SubClassNames) const
{
	InheritanceContext.ConditionalUpdate();

	TSet<FTopLevelAssetPath> ProcessedClassNames;
	for (const FTopLevelAssetPath& ClassName : InClassNames)
	{
		// Now find all subclass names
		GetSubClasses_Recursive(InheritanceContext, ClassName, SubClassNames, ProcessedClassNames, ExcludedClassNames);
	}
}

void FAssetRegistryImpl::GetSubClasses_Recursive(Impl::FClassInheritanceContext& InheritanceContext, FTopLevelAssetPath InClassName,
	TSet<FTopLevelAssetPath>& SubClassNames, TSet<FTopLevelAssetPath>& ProcessedClassNames, const TSet<FTopLevelAssetPath>& ExcludedClassNames) const
{
	if (ExcludedClassNames.Contains(InClassName))
	{
		// This class is in the exclusion list. Exclude it.
	}
	else if (ProcessedClassNames.Contains(InClassName))
	{
		// This class has already been processed. Ignore it.
	}
	else
	{
		SubClassNames.Add(InClassName);
		ProcessedClassNames.Add(InClassName);

		auto AddSubClasses = [this, &InheritanceContext, &SubClassNames, &ProcessedClassNames, &ExcludedClassNames]
		(FTopLevelAssetPath ParentClassName)
		{
			const TArray<FTopLevelAssetPath>* FoundSubClassNames = InheritanceContext.Buffer->ReverseInheritanceMap.Find(ParentClassName);
			if (FoundSubClassNames)
			{
				for (FTopLevelAssetPath ClassName : (*FoundSubClassNames))
				{
					GetSubClasses_Recursive(InheritanceContext, ClassName, SubClassNames, ProcessedClassNames,
						ExcludedClassNames);
				}
			}
		};

		// Add Subclasses of the given classname
		AddSubClasses(InClassName);
	}
}

}

#if WITH_EDITOR
static FString GAssetRegistryManagementPathsPackageDebugName;
static FAutoConsoleVariableRef CVarAssetRegistryManagementPathsPackageDebugName(
	TEXT("AssetRegistry.ManagementPathsPackageDebugName"),
	GAssetRegistryManagementPathsPackageDebugName,
	TEXT("If set, when manage references are set, the chain of references that caused this package to become managed will be printed to the log"));

void PrintAssetRegistryManagementPathsPackageDebugInfo(FDependsNode* Node, const TMap<FDependsNode*, FDependsNode*>& EditorOnlyManagementPaths)
{
	if (Node)
	{
		UE_LOG(LogAssetRegistry, Display, TEXT("SetManageReferences is printing out the reference chain that caused '%s' to be managed"), *GAssetRegistryManagementPathsPackageDebugName);
		TSet<FDependsNode*> AllVisitedNodes;
		while (FDependsNode* ReferencingNode = EditorOnlyManagementPaths.FindRef(Node))
		{
			UE_LOG(LogAssetRegistry, Display, TEXT("  %s"), *ReferencingNode->GetIdentifier().ToString());
			if (AllVisitedNodes.Contains(ReferencingNode))
			{
				UE_LOG(LogAssetRegistry, Display, TEXT("  ... (Circular reference back to %s)"), *ReferencingNode->GetPackageName().ToString());
				break;
			}

			AllVisitedNodes.Add(ReferencingNode);
			Node = ReferencingNode;
		}
	}
	else
	{
		UE_LOG(LogAssetRegistry, Warning, TEXT("Node with AssetRegistryManagementPathsPackageDebugName '%s' was not found"), *GAssetRegistryManagementPathsPackageDebugName);
	}
}
#endif // WITH_EDITOR

void UAssetRegistryImpl::SetManageReferences(const TMultiMap<FAssetIdentifier, FAssetIdentifier>& ManagerMap,
	bool bClearExisting, UE::AssetRegistry::EDependencyCategory RecurseType, TSet<FDependsNode*>& ExistingManagedNodes,
	ShouldSetManagerPredicate ShouldSetManager)
{
	// For performance reasons we call the ShouldSetManager callback when inside the lock. Licensee UAssetManagers
	// are responsible for not calling AssetRegistry functions from ShouldSetManager as that would create a deadlock
	LLM_SCOPE(ELLMTag::AssetRegistry);
	FWriteScopeLock InterfaceScopeLock(InterfaceLock);
	GuardedData.SetManageReferences(ManagerMap, bClearExisting, RecurseType, ExistingManagedNodes, ShouldSetManager);
}

namespace UE::AssetRegistry
{

void FAssetRegistryImpl::SetManageReferences(const TMultiMap<FAssetIdentifier, FAssetIdentifier>& ManagerMap,
	bool bClearExisting, UE::AssetRegistry::EDependencyCategory RecurseType, TSet<FDependsNode*>& ExistingManagedNodes,
	IAssetRegistry::ShouldSetManagerPredicate ShouldSetManager)
{
	// Set default predicate if needed
	if (!ShouldSetManager)
	{
		ShouldSetManager = [](const FAssetIdentifier& Manager, const FAssetIdentifier& Source, const FAssetIdentifier& Target, UE::AssetRegistry::EDependencyCategory Category, UE::AssetRegistry::EDependencyProperty Properties, EAssetSetManagerFlags::Type Flags)
		{
			return EAssetSetManagerResult::SetButDoNotRecurse;
		};
	}

	if (bClearExisting)
	{
		// Find all nodes with incoming manage dependencies
		for (const TPair<FAssetIdentifier, FDependsNode*>& Pair : State.CachedDependsNodes)
		{
			Pair.Value->IterateOverDependencies([&ExistingManagedNodes](FDependsNode* TestNode, EDependencyCategory Category, EDependencyProperty Property, bool bUnique)
				{
					ExistingManagedNodes.Add(TestNode);
				}, EDependencyCategory::Manage);
		}

		// Clear them
		for (const TPair<FAssetIdentifier, FDependsNode*>& Pair : State.CachedDependsNodes)
		{
			Pair.Value->ClearDependencies(EDependencyCategory::Manage);
		}
		for (FDependsNode* NodeToClear : ExistingManagedNodes)
		{
			NodeToClear->SetIsReferencersSorted(false);
			NodeToClear->RefreshReferencers();
		}
		ExistingManagedNodes.Empty();
	}

	TMap<FDependsNode*, TArray<FDependsNode *>> ExplicitMap; // Reverse of ManagerMap, specifies what relationships to add to each node

	for (const TPair<FAssetIdentifier, FAssetIdentifier>& Pair : ManagerMap)
	{
		FDependsNode* ManagedNode = State.FindDependsNode(Pair.Value);

		if (!ManagedNode)
		{
			UE_LOG(LogAssetRegistry, Warning, TEXT("Cannot set %s to manage asset %s because %s does not exist!"), 
				*Pair.Key.ToString(),
				*Pair.Value.ToString(),
				*Pair.Value.ToString());
			continue;
		}

		TArray<FDependsNode*>& ManagerList = ExplicitMap.FindOrAdd(ManagedNode);

		FDependsNode* ManagerNode = State.CreateOrFindDependsNode(Pair.Key);

		ManagerList.Add(ManagerNode);
	}

	TSet<FDependsNode*> Visited;
	TMap<FDependsNode*, EDependencyProperty> NodesToManage;
	TArray<FDependsNode*> NodesToHardReference;
	TArray<FDependsNode*> NodesToRecurse;

#if WITH_EDITOR
	// Map of every depends node to the node whose reference caused it to become managed by an asset. Used to look up why an asset was chosen to be the manager.
	TMap<FDependsNode*, FDependsNode*> EditorOnlyManagementPaths;
#endif

	TSet<FDependsNode*> NewManageNodes;
	// For each explicitly set asset
	for (const TPair<FDependsNode*, TArray<FDependsNode *>>& ExplicitPair : ExplicitMap)
	{
		FDependsNode* BaseManagedNode = ExplicitPair.Key;
		const TArray<FDependsNode*>& ManagerNodes = ExplicitPair.Value;

		for (FDependsNode* ManagerNode : ManagerNodes)
		{
			Visited.Reset();
			NodesToManage.Reset();
			NodesToRecurse.Reset();

			FDependsNode* SourceNode = ManagerNode;

			auto IterateFunction = [&ManagerNode, &SourceNode, &ShouldSetManager, &NodesToManage, &NodesToHardReference, &NodesToRecurse, &Visited, &ExplicitMap, &ExistingManagedNodes
#if WITH_EDITOR
										, &EditorOnlyManagementPaths
#endif
										](FDependsNode* ReferencingNode, FDependsNode* TargetNode, EDependencyCategory DependencyType, EDependencyProperty DependencyProperties)
			{
				// Only recurse if we haven't already visited, and this node passes recursion test
				if (!Visited.Contains(TargetNode))
				{
					EAssetSetManagerFlags::Type Flags = (EAssetSetManagerFlags::Type)((SourceNode == ManagerNode ? EAssetSetManagerFlags::IsDirectSet : 0)
						| (ExistingManagedNodes.Contains(TargetNode) ? EAssetSetManagerFlags::TargetHasExistingManager : 0)
						| (ExplicitMap.Find(TargetNode) && SourceNode != ManagerNode ? EAssetSetManagerFlags::TargetHasDirectManager : 0));

					EAssetSetManagerResult::Type Result = ShouldSetManager(ManagerNode->GetIdentifier(), SourceNode->GetIdentifier(), TargetNode->GetIdentifier(), DependencyType, DependencyProperties, Flags);

					if (Result == EAssetSetManagerResult::DoNotSet)
					{
						return;
					}

					EDependencyProperty ManageProperties = (Flags & EAssetSetManagerFlags::IsDirectSet) ? EDependencyProperty::Direct : EDependencyProperty::None;
					NodesToManage.Add(TargetNode, ManageProperties);

#if WITH_EDITOR
					if (!GAssetRegistryManagementPathsPackageDebugName.IsEmpty())
					{
						EditorOnlyManagementPaths.Add(TargetNode, ReferencingNode ? ReferencingNode : ManagerNode);
					}
#endif

					if (Result == EAssetSetManagerResult::SetAndRecurse)
					{
						NodesToRecurse.Push(TargetNode);
					}
				}
			};

			// Check initial node
			IterateFunction(nullptr, BaseManagedNode, EDependencyCategory::Manage, EDependencyProperty::Direct);

			// Do all recursion first, but only if we have a recurse type
			if (RecurseType != EDependencyCategory::None)
			{
				while (NodesToRecurse.Num())
				{
					// Pull off end of array, order doesn't matter
					SourceNode = NodesToRecurse.Pop();
										
					Visited.Add(SourceNode);

					SourceNode->IterateOverDependencies([&IterateFunction, SourceNode](FDependsNode* TargetNode, EDependencyCategory DependencyCategory, EDependencyProperty DependencyProperties, bool bDuplicate)
						{
							// Skip editor-only properties
							if (!!(DependencyProperties & EDependencyProperty::Game))
							{
								IterateFunction(SourceNode, TargetNode, DependencyCategory, DependencyProperties);
							}
						}, RecurseType);
				}
			}

			ManagerNode->SetIsDependencyListSorted(UE::AssetRegistry::EDependencyCategory::Manage, false);
			for (TPair<FDependsNode*, EDependencyProperty>& ManagePair : NodesToManage)
			{
				ManagePair.Key->SetIsReferencersSorted(false);
				ManagePair.Key->AddReferencer(ManagerNode);
				ManagerNode->AddDependency(ManagePair.Key, EDependencyCategory::Manage, ManagePair.Value);
				NewManageNodes.Add(ManagePair.Key);
			}
		}
	}

	for (FDependsNode* ManageNode : NewManageNodes)
	{
		ExistingManagedNodes.Add(ManageNode);
	}
	// Restore all nodes to manage dependencies sorted and references sorted, so we can efficiently read them in future operations
	State.SetDependencyNodeSorting(ShouldSortDependencies(), ShouldSortReferencers());

#if WITH_EDITOR
	if (!GAssetRegistryManagementPathsPackageDebugName.IsEmpty())
	{
		FDependsNode* PackageDebugInfoNode = State.FindDependsNode(FName(*GAssetRegistryManagementPathsPackageDebugName));
		PrintAssetRegistryManagementPathsPackageDebugInfo(PackageDebugInfoNode, EditorOnlyManagementPaths);
	}
#endif
}

}

bool UAssetRegistryImpl::SetPrimaryAssetIdForObjectPath(const FSoftObjectPath& ObjectPath, FPrimaryAssetId PrimaryAssetId)
{
	UE::AssetRegistry::Impl::FEventContext EventContext;
	bool bResult;
	{
		LLM_SCOPE(ELLMTag::AssetRegistry);
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		bResult = GuardedData.SetPrimaryAssetIdForObjectPath(EventContext, ObjectPath, PrimaryAssetId);
	}
	Broadcast(EventContext);
	return bResult;
}

namespace UE::AssetRegistry
{

bool FAssetRegistryImpl::SetPrimaryAssetIdForObjectPath(Impl::FEventContext& EventContext, const FSoftObjectPath& ObjectPath, FPrimaryAssetId PrimaryAssetId)
{
	FAssetData** FoundAssetData = State.CachedAssets.Find(FCachedAssetKey(ObjectPath));

	if (!FoundAssetData)
	{
		return false;
	}

	FAssetData* AssetData = *FoundAssetData;

	FAssetDataTagMap TagsAndValues = AssetData->TagsAndValues.CopyMap();
	TagsAndValues.Add(FPrimaryAssetId::PrimaryAssetTypeTag, PrimaryAssetId.PrimaryAssetType.ToString());
	TagsAndValues.Add(FPrimaryAssetId::PrimaryAssetNameTag, PrimaryAssetId.PrimaryAssetName.ToString());

	FAssetData NewAssetData(*AssetData);
	NewAssetData.TagsAndValues = FAssetDataTagMapSharedView(MoveTemp(TagsAndValues));
	UpdateAssetData(EventContext, AssetData, MoveTemp(NewAssetData), false /* bKeepDeletedTags */);

	return true;
}

}

bool FAssetRegistryDependencyOptions::GetPackageQuery(UE::AssetRegistry::FDependencyQuery& Flags) const
{
	Flags = UE::AssetRegistry::FDependencyQuery();
	if (bIncludeSoftPackageReferences || bIncludeHardPackageReferences)
	{
		if (!bIncludeSoftPackageReferences) Flags.Required |= UE::AssetRegistry::EDependencyProperty::Hard;
		if (!bIncludeHardPackageReferences) Flags.Excluded |= UE::AssetRegistry::EDependencyProperty::Hard;
		return true;
	}
	return false;
}

bool FAssetRegistryDependencyOptions::GetSearchableNameQuery(UE::AssetRegistry::FDependencyQuery& Flags) const
{
	Flags = UE::AssetRegistry::FDependencyQuery();
	return bIncludeSearchableNames;
}

bool FAssetRegistryDependencyOptions::GetManageQuery(UE::AssetRegistry::FDependencyQuery& Flags) const
{
	Flags = UE::AssetRegistry::FDependencyQuery();
	if (bIncludeSoftManagementReferences || bIncludeHardManagementReferences)
	{
		if (!bIncludeSoftManagementReferences) Flags.Required |= UE::AssetRegistry::EDependencyProperty::Direct;
		if (!bIncludeHardPackageReferences) Flags.Excluded|= UE::AssetRegistry::EDependencyProperty::Direct;
		return true;
	}
	return false;
}

void FAssetDependency::WriteCompactBinary(FCbWriter& Writer) const
{
	Writer.BeginArray();
	Writer << AssetId;
	static_assert(sizeof(uint8) >= sizeof(Category));
	Writer.AddInteger((uint8)Category);
	static_assert(sizeof(uint8) >= sizeof(Properties));
	Writer.AddInteger((uint8)Properties);
	Writer.EndArray();
}

bool LoadFromCompactBinary(FCbFieldView Field, FAssetDependency& Dependency)
{
	FCbArrayView ArrayField = Field.AsArrayView();
	if (ArrayField.Num() < 3)
	{
		Dependency = FAssetDependency();
		return false;
	}
	FCbFieldViewIterator Iter = ArrayField.CreateViewIterator();
	if (!LoadFromCompactBinary(Iter++, Dependency.AssetId))
	{
		Dependency = FAssetDependency();
		return false;
	}
	uint8 Value;
	if (LoadFromCompactBinary(Iter++, Value))
	{
		Dependency.Category = (UE::AssetRegistry::EDependencyCategory)Value;
	}
	else
	{
		Dependency = FAssetDependency();
		return false;
	}
	if (LoadFromCompactBinary(Iter++, Value))
	{
		Dependency.Properties = (UE::AssetRegistry::EDependencyProperty)Value;
	}
	else
	{
		Dependency = FAssetDependency();
		return false;
	}
	return true;
}

namespace UE::AssetRegistry
{

const FAssetRegistryState& FAssetRegistryImpl::GetState() const
{
	return State;
}

const FPathTree& FAssetRegistryImpl::GetCachedPathTree() const
{
	return CachedPathTree;
}

const TSet<FName>& FAssetRegistryImpl::GetCachedEmptyPackages() const
{
	return CachedEmptyPackages;
}

bool FAssetRegistryImpl::ShouldSkipAsset(FTopLevelAssetPath AssetClass, uint32 PackageFlags) const
{
#if WITH_ENGINE && WITH_EDITOR
	return Utils::ShouldSkipAsset(AssetClass, PackageFlags, SkipUncookedClasses, SkipCookedClasses);
#else
	return false;
#endif
}

bool FAssetRegistryImpl::ShouldSkipAsset(const UObject* InAsset) const
{
#if WITH_ENGINE && WITH_EDITOR
	return Utils::ShouldSkipAsset(InAsset, SkipUncookedClasses, SkipCookedClasses);
#else
	return false;
#endif
}

namespace Impl
{

void FEventContext::Clear()
{
	bFileLoadedEventBroadcast = false;
	ProgressUpdateData.Reset();
	PathEvents.Empty();
	AssetEvents.Empty();
	RequiredLoads.Empty();
	BlockedFiles.Empty();
}

bool FEventContext::IsEmpty() const
{
	return !bFileLoadedEventBroadcast &&
		!ProgressUpdateData.IsSet() &&
		PathEvents.Num() == 0 &&
		AssetEvents.Num() == 0 &&
		RequiredLoads.Num() == 0 &&
		BlockedFiles.Num() == 0;
}

void FEventContext::Append(FEventContext&& Other)
{
	if (&Other == this)
	{
		return;
	}
	bFileLoadedEventBroadcast |= Other.bFileLoadedEventBroadcast;
	if (Other.ProgressUpdateData.IsSet())
	{
		ProgressUpdateData = MoveTemp(Other.ProgressUpdateData);
	}
	PathEvents.Append(MoveTemp(Other.PathEvents));
	AssetEvents.Append(MoveTemp(Other.AssetEvents));
	RequiredLoads.Append(MoveTemp(Other.RequiredLoads));
	BlockedFiles.Append(MoveTemp(Other.BlockedFiles));
}

}

}

void UAssetRegistryImpl::ReadLockEnumerateTagToAssetDatas(TFunctionRef<void(FName TagName, const TArray<const FAssetData*>& Assets)> Callback) const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	for (const TPair<FName, const TArray<const FAssetData*>>& Pair : GuardedData.GetState().GetTagToAssetDatasMap())
	{
		Callback(Pair.Key, Pair.Value);
	}
}

void UAssetRegistryImpl::Broadcast(UE::AssetRegistry::Impl::FEventContext& EventContext)
{
	using namespace UE::AssetRegistry::Impl;
	if (!IsInGameThread() || FUObjectThreadContext::Get().IsRoutingPostLoad)
	{
		// By contract events (and PackageLoads) can only be sent on the game thread; some legacy systems depend on 
		// this and are not threadsafe. If we're not in the game thread, defer all events in the EventContext
		// instead of broadcasting them on this thread
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		check(&EventContext != &DeferredEvents); // Only the GameThread should be calling Broadcast on DeferredEvents
		DeferredEvents.Append(MoveTemp(EventContext));
		EventContext.Clear();
		return;
	}

	if (EventContext.PathEvents.Num())
	{
		// Batch add/remove events 
		TArray<FStringView> Params;
		// Ensure loop batch condition is always false first iteration
		bool bCurrentBatchIsAdd = EventContext.PathEvents[0].Get<1>() == FEventContext::EEvent::Added;
		for (const TPair<FString, FEventContext::EEvent>& PathEvent : EventContext.PathEvents)
		{
			const FString& Path = PathEvent.Get<0>();
			bool bEventIsAdd = PathEvent.Get<1>() == FEventContext::EEvent::Added;
			if (bEventIsAdd != bCurrentBatchIsAdd)
			{
				(bCurrentBatchIsAdd ? PathsAddedEvent : PathsRemovedEvent).Broadcast(MakeArrayView(Params));
				Params.Reset();
				bCurrentBatchIsAdd = bEventIsAdd;
			}
			Params.Add(FStringView(Path));
		}
		if (Params.Num() != 0)
		{
			(bCurrentBatchIsAdd ? PathsAddedEvent : PathsRemovedEvent).Broadcast(MakeArrayView(Params));
		}

		// Legacy single events 
		if (PathAddedEvent.IsBound() || PathRemovedEvent.IsBound())
		{
			for (const TPair<FString, FEventContext::EEvent>& PathEvent : EventContext.PathEvents)
			{
				const FString& Path = PathEvent.Get<0>();
				switch (PathEvent.Get<1>())
				{
				case FEventContext::EEvent::Added:
					PathAddedEvent.Broadcast(Path);
					break;
				case FEventContext::EEvent::Removed:
					PathRemovedEvent.Broadcast(Path);
					break;
				}
			}
		}
		EventContext.PathEvents.Empty();
	}

	if (EventContext.AssetEvents.Num())
	{
		// Batch events so that if adds/updates are interspersed with removes, relative ordering of the add/remove is maintained 
		constexpr uint32 EventTypeCount = static_cast<uint32>(FEventContext::EEvent::MAX);
		static_assert(EventTypeCount == 4, "Loop needs to be rewritten to correctly order new event types");
		TArray<FAssetData> EventBatches[EventTypeCount];
		FEventContext::EEvent LastEvent = EventContext.AssetEvents[0].Get<1>();
		auto FlushBatchedEvents = [&EventBatches, &Events=BatchedAssetEvents]() 
		{
			for (int32 i=0; i < UE_ARRAY_COUNT(EventBatches); ++i)
			{
				if (EventBatches[i].Num())
				{
					Events[i].Broadcast(EventBatches[i]);
					EventBatches[i].Reset();
				}
			}
		};

		for (const TPair<FAssetData, FEventContext::EEvent>& AssetEvent : EventContext.AssetEvents)
		{
			const FAssetData& AssetData = AssetEvent.Get<0>();
			FEventContext::EEvent Event = AssetEvent.Get<1>();

			// Flush events when switching between removed and non-removed events 
			if ((Event == FEventContext::EEvent::Removed) != (LastEvent == FEventContext::EEvent::Removed))
			{
				FlushBatchedEvents();
			}
			EventBatches[static_cast<int32>(Event)].Add(AssetData);
			LastEvent = Event;
		}
		// Flush last batch of events 
		FlushBatchedEvents();
		
		// Single events
		for (const TPair<FAssetData, FEventContext::EEvent>& AssetEvent : EventContext.AssetEvents)
		{
			const FAssetData& AssetData = AssetEvent.Get<0>();
			switch (AssetEvent.Get<1>())
			{
			case FEventContext::EEvent::Added:
				AssetAddedEvent.Broadcast(AssetData);
				break;
			case FEventContext::EEvent::Removed:
				AssetRemovedEvent.Broadcast(AssetData);
				break;
			case FEventContext::EEvent::Updated:
				AssetUpdatedEvent.Broadcast(AssetData);
				break;
			case FEventContext::EEvent::UpdatedOnDisk:
				AssetUpdatedOnDiskEvent.Broadcast(AssetData);
				break;
			default:
				checkNoEntry();
				break;
			}
		}
		EventContext.AssetEvents.Empty();
	}
	if (EventContext.VerseEvents.Num())
	{
		for (const TPair<FName, FEventContext::EEvent>& VerseEvent : EventContext.VerseEvents)
		{
			const FName& VerseFilepath = VerseEvent.Get<0>();
			switch (VerseEvent.Get<1>())
			{
			case FEventContext::EEvent::Added:
				VerseAddedEvent.Broadcast(VerseFilepath);
				break;
			case FEventContext::EEvent::Removed:
				VerseRemovedEvent.Broadcast(VerseFilepath);
				break;
			// (jcotton) We are not yet broadcasting Verse updating events as the only use case for VerseEvent broadcasts currently is to trigger a Verse-build
			// and triggering a build on every change would be far too expensive.
			case FEventContext::EEvent::Updated:
				[[fallthrough]];
			case FEventContext::EEvent::UpdatedOnDisk:
				[[fallthrough]];
			default:
				break;
			}
		}
		EventContext.VerseEvents.Empty();
	}
	if (EventContext.RequiredLoads.Num())
	{
		for (const FString& RequiredLoad : EventContext.RequiredLoads)
		{
			LoadPackage(nullptr, *RequiredLoad, 0);
		}
		EventContext.RequiredLoads.Empty();
	}
	if (EventContext.BlockedFiles.Num())
	{
		FilesBlockedEvent.Broadcast(EventContext.BlockedFiles);
		EventContext.BlockedFiles.Empty();
	}

	if (EventContext.ProgressUpdateData.IsSet())
	{
		FileLoadProgressUpdatedEvent.Broadcast(*EventContext.ProgressUpdateData);
		EventContext.ProgressUpdateData.Reset();
	}

	// FileLoadedEvent needs to come after all of the AssetEvents. Some systems do more expensive work for
	// AssetEvents after receiving FileLoadedEvent, because they batched up that work for all assets in the initial load in
	// their FileLoadedEvent handler. The AssetEvents precede the FileLoadedEvent in the broadcast that is sent from
	// TickGatherer, so it is correct to make them precede it in the order in which we broadcast the events.

	if (EventContext.bFileLoadedEventBroadcast)
	{
		if (&EventContext != &DeferredEvents)
		{
			FWriteScopeLock InterfaceScopeLock(InterfaceLock);
			// Do not send the file loaded event yet if there are still deferred events and pass the flag on instead
			if (!DeferredEvents.IsEmpty())
			{
				EventContext.bFileLoadedEventBroadcast = false;
				DeferredEvents.bFileLoadedEventBroadcast = true;
				return;
			}
		}

		FileLoadedEvent.Broadcast();
		EventContext.bFileLoadedEventBroadcast = false;
	}
}


UAssetRegistryImpl::FFilesBlockedEvent& UAssetRegistryImpl::OnFilesBlocked()
{
	return FilesBlockedEvent;
}

UAssetRegistryImpl::FPathsEvent& UAssetRegistryImpl::OnPathsAdded()
{
	return PathsAddedEvent;
}

UAssetRegistryImpl::FPathsEvent& UAssetRegistryImpl::OnPathsRemoved()
{
	return PathsRemovedEvent;
}

UAssetRegistryImpl::FPathAddedEvent& UAssetRegistryImpl::OnPathAdded()
{
	return PathAddedEvent;
}

UAssetRegistryImpl::FPathRemovedEvent& UAssetRegistryImpl::OnPathRemoved()
{
	return PathRemovedEvent;
}

UAssetRegistryImpl::FAssetAddedEvent& UAssetRegistryImpl::OnAssetAdded()
{
	return AssetAddedEvent;
}

UAssetRegistryImpl::FAssetRemovedEvent& UAssetRegistryImpl::OnAssetRemoved()
{
	return AssetRemovedEvent;
}

UAssetRegistryImpl::FAssetRenamedEvent& UAssetRegistryImpl::OnAssetRenamed()
{
	return AssetRenamedEvent;
}

UAssetRegistryImpl::FAssetUpdatedEvent& UAssetRegistryImpl::OnAssetUpdated()
{
	return AssetUpdatedEvent;
}

UAssetRegistryImpl::FAssetUpdatedEvent& UAssetRegistryImpl::OnAssetUpdatedOnDisk()
{
	return AssetUpdatedOnDiskEvent;
}

UAssetRegistryImpl::FAssetsEvent& UAssetRegistryImpl::OnAssetsAdded()
{
	return BatchedAssetEvents[static_cast<SIZE_T>(UE::AssetRegistry::Impl::FEventContext::EEvent::Added)];
}

UAssetRegistryImpl::FAssetsEvent& UAssetRegistryImpl::OnAssetsUpdated()
{
	return BatchedAssetEvents[static_cast<SIZE_T>(UE::AssetRegistry::Impl::FEventContext::EEvent::Updated)];
}

UAssetRegistryImpl::FAssetsEvent& UAssetRegistryImpl::OnAssetsUpdatedOnDisk()
{
	return BatchedAssetEvents[static_cast<SIZE_T>(UE::AssetRegistry::Impl::FEventContext::EEvent::UpdatedOnDisk)];
}

UAssetRegistryImpl::FAssetsEvent& UAssetRegistryImpl::OnAssetsRemoved()
{
	return BatchedAssetEvents[static_cast<SIZE_T>(UE::AssetRegistry::Impl::FEventContext::EEvent::Removed)];
}

UAssetRegistryImpl::FInMemoryAssetCreatedEvent& UAssetRegistryImpl::OnInMemoryAssetCreated()
{
	return InMemoryAssetCreatedEvent;
}

UAssetRegistryImpl::FInMemoryAssetDeletedEvent& UAssetRegistryImpl::OnInMemoryAssetDeleted()
{
	return InMemoryAssetDeletedEvent;
}

UAssetRegistryImpl::FVerseAddedEvent& UAssetRegistryImpl::OnVerseAdded()
{
	return VerseAddedEvent;
}

UAssetRegistryImpl::FVerseRemovedEvent& UAssetRegistryImpl::OnVerseRemoved()
{
	return VerseRemovedEvent;
}

UAssetRegistryImpl::FFilesLoadedEvent& UAssetRegistryImpl::OnFilesLoaded()
{
	return FileLoadedEvent;
}

UAssetRegistryImpl::FFileLoadProgressUpdatedEvent& UAssetRegistryImpl::OnFileLoadProgressUpdated()
{
	return FileLoadProgressUpdatedEvent;
}


namespace UE::AssetRegistry
{
const FAssetData* GetMostImportantAsset(TConstArrayView<const FAssetData*> PackageAssetDatas, EGetMostImportantAssetFlags InFlags)
{
	if (PackageAssetDatas.Num() == 1) // common case
	{
		return PackageAssetDatas[0];
	}

	// Find a candidate asset.
	// If there's a "UAsset", then we use that as the asset.
	// If not, then we look for a "TopLevelAsset", i.e. one that shows up in the content browser.
	int32 TopLevelAssetCount = 0;

	// If we have multiple TLAs, then we pick the "least" TLA.
	// If we have NO TLAs, then we pick the "least" asset,
	// both determined by class then name:
	auto AssetDataLessThan = [](const FAssetData* LHS, const FAssetData* RHS)
	{
		int32 ClassCompare = LHS->AssetClassPath.Compare(RHS->AssetClassPath);
		if (ClassCompare == 0)
		{
			return LHS->AssetName.LexicalLess(RHS->AssetName);
		}
		return ClassCompare < 0;
	};

	const FAssetData* LeastTopLevelAsset = nullptr;
	const FAssetData* LeastAsset = nullptr;
	for (const FAssetData* Asset : PackageAssetDatas)
	{
		if (Asset->AssetName.IsNone())
		{
			continue;
		}
		if (Asset->IsUAsset())
		{
			return Asset;
		}
		// This is after IsUAsset because Blueprints can be the UAsset but also be considered skipable.
		if (!EnumHasAnyFlags(InFlags, EGetMostImportantAssetFlags::IgnoreSkipClasses))
		{
			if (FFiltering::ShouldSkipAsset(Asset->AssetClassPath, Asset->PackageFlags))
			{
				continue;
			}
		}

		if (Asset->IsTopLevelAsset())
		{
			TopLevelAssetCount++;
			if (LeastTopLevelAsset == nullptr ||
				AssetDataLessThan(Asset, LeastTopLevelAsset))
			{
				LeastTopLevelAsset = Asset;
			}
		}
		if (LeastAsset == nullptr ||
			AssetDataLessThan(Asset, LeastAsset))
		{
			LeastAsset = Asset;
		}
	}

	if (EnumHasAnyFlags(InFlags, EGetMostImportantAssetFlags::RequireOneTopLevelAsset))
	{
		if (TopLevelAssetCount == 1)
		{
			return LeastTopLevelAsset;
		}
		return nullptr;
	}

	if (TopLevelAssetCount)
	{
		return LeastTopLevelAsset;
	}
	return LeastAsset;
}


void GetAssetForPackages(TConstArrayView<FName> PackageNames, TMap<FName, FAssetData>& OutPackageToAssetData)
{
	FARFilter Filter;
	for (FName PackageName : PackageNames)
	{
		Filter.PackageNames.Add(PackageName);
	}
	
	TArray<FAssetData> AssetDataList;
	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (!AssetRegistry)
	{
		return;
	}
	AssetRegistry->GetAssets(Filter, AssetDataList);

	if (AssetDataList.Num() == 0)
	{
		return;
	}

	Algo::SortBy(AssetDataList, &FAssetData::PackageName, FNameFastLess());

	TArray<const FAssetData*, TInlineAllocator<1>> PackageAssetDatas;
	FName CurrentPackageName = AssetDataList[0].PackageName;
	for (const FAssetData& AssetData : AssetDataList)
	{
		if (CurrentPackageName != AssetData.PackageName)
		{
			OutPackageToAssetData.FindOrAdd(CurrentPackageName) = *GetMostImportantAsset(PackageAssetDatas);
			PackageAssetDatas.Reset();
			CurrentPackageName = AssetData.PackageName;
		}

		PackageAssetDatas.Push(&AssetData);
	}

	OutPackageToAssetData.FindOrAdd(CurrentPackageName) = *GetMostImportantAsset(PackageAssetDatas);

}

bool ShouldSearchAllAssetsAtStart()
{
	// If in the editor or cookcommandlet, or an allowlist commandlet, we start the GlobalGatherer now
	// In the game or other commandlets, we do not construct it until project or commandlet code calls SearchAllAssets or ScanPathsSynchronous
	bool bSearchAllAssetsAtStart = false;
	if (GIsEditor)
	{
		if (!IsRunningCommandlet() || IsRunningCookCommandlet())
		{
			bSearchAllAssetsAtStart = true;
		}
		else
		{
			TArray<FString> CommandletsUsingAR;
			GConfig->GetArray(TEXT("AssetRegistry"), TEXT("CommandletsUsingAR"), CommandletsUsingAR, GEngineIni);
			FString CommandlineCommandlet;
			FString CommandletToken(TEXT("commandlet"));
			if (!CommandletsUsingAR.IsEmpty() &&
				FParse::Value(FCommandLine::Get(), TEXT("-run="), CommandlineCommandlet))
			{
				if (CommandlineCommandlet.EndsWith(CommandletToken))
				{
					CommandlineCommandlet.LeftChopInline(CommandletToken.Len(), EAllowShrinking::No);
				}
				for (FString& CommandletUsingAR : CommandletsUsingAR)
				{
					if (CommandletUsingAR.EndsWith(CommandletToken))
					{
						CommandletUsingAR.LeftChopInline(CommandletToken.Len(), EAllowShrinking::No);
					}
					if (CommandletUsingAR == CommandlineCommandlet)
					{
						bSearchAllAssetsAtStart = true;
						break;
					}
				}
			}
		}
	}
#if WITH_EDITOR || !UE_BUILD_SHIPPING
	bool bCommandlineAllAssetsAtStart;
	if (FParse::Bool(FCommandLine::Get(), TEXT("AssetGatherAll="), bCommandlineAllAssetsAtStart))
	{
		bSearchAllAssetsAtStart = bCommandlineAllAssetsAtStart;
	}
#endif // WITH_EDITOR || !UE_BUILD_SHIPPING
	return bSearchAllAssetsAtStart;
}

} // namespace AssetRegistry
