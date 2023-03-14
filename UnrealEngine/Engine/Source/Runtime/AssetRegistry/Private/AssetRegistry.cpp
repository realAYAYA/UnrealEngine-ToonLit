// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetRegistry.h"

#include "Algo/Unique.h"
#include "AssetDataGatherer.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetDependencyGatherer.h"
#include "AssetRegistryConsoleCommands.h"
#include "AssetRegistryPrivate.h"
#include "Async/Async.h"
#include "Blueprint/BlueprintSupport.h"
#include "DependsNode.h"
#include "GenericPlatform/GenericPlatformChunkInstall.h"
#include "HAL/PlatformMisc.h"
#include "HAL/ThreadHeartBeat.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Optional.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "Misc/RedirectCollector.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/TrackedActivity.h"
#include "PackageReader.h"
#include "Serialization/ArrayReader.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/CoreRedirects.h"
#include "UObject/MetaData.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"

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
}

namespace UE::AssetRegistry::Impl
{
	/** The max time to spend in UAssetRegistryImpl::Tick */
	const float MaxSecondsPerFrame = 0.04f;
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

protected:
	/* This function is a workaround for platforms that don't support disable of deprecation warnings on override functions*/
	virtual void GetDependenciesDeprecated(FName InPackageName, TArray<FName>& OutDependencies, EAssetRegistryDependencyType::Type InDependencyType) override
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		IAssetRegistry::GetChecked().GetDependencies(InPackageName, OutDependencies, InDependencyType);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
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

static bool CanConsumeAsync()
{
	// Async Consumption (copying the preloaded Premade FAssetRegistryState into the global AR) is only
	// available when async is available, and is not currently used in the cooked game because we have
	// to block on it anyway.
	return CanLoadAsync() && !FPlatformProperties::RequiresCookedData();
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
		Shutdown();
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
			PakMountedDelegate = FCoreDelegates::OnPakFileMounted2.AddLambda([this](const IPakFile& Pak)
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
						FCoreDelegates::OnPakFileMounted2.Remove(LocalPakMountedDelegate);
					}
				});
		}
	}

	void KickPreload()
	{
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
		FCoreDelegates::OnPakFileMounted2.Remove(PakMountedDelegate);
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
	void Shutdown()
	{
		OnTaskGraphReady.Reset();
		if (PreloadReady)
		{
			FPlatformProcess::ReturnSynchEventToPool(PreloadReady);
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

	/** Delegate handle for the callback added to OnPakFileMounted2.After threading starts, Read / Write only within the lock. */
	FDelegateHandle PakMountedDelegate;

	/** Callback from ConsumeOrDefer that is set so TryLoadAsync can trigger the Consume when it completes.Read / Write only within the lock. */
	FConsumeFunction ConsumeCallback;

	/** State machine state. Read/Write only within the lock (or before threading starts). */
	EState LoadState = EState::WillNeverPreload;

	/** Result of TryLoad.Thread ownership rules are the same as the rules for Payload. */
	ELoadResult LoadResult = ELoadResult::UninitializedMemberLoadResult;
}
GPreloader;

#if ASSETREGISTRY_ENABLE_PREMADE_REGISTRY_IN_EDITOR
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
	UARI.GuardedData.LoadPremadeAssetRegistry(EventContext, LoadResult, MoveTemp(ARState), FAssetRegistryState::EInitializationMode::OnlyUpdateNew);
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

#endif

}

namespace UE::AssetRegistry
{
void FAssetRegistryImpl::ConditionalLoadPremadeAssetRegistry(UAssetRegistryImpl& UARI, Impl::FEventContext& EventContext, FWriteScopeLock& ScopeLock)
{
#if ASSETREGISTRY_ENABLE_PREMADE_REGISTRY_IN_EDITOR
	AsyncConsumer.Wait(UARI, ScopeLock);
#endif
}

void FAssetRegistryImpl::ConsumeOrDeferPreloadedPremade(UAssetRegistryImpl& UARI, Impl::FEventContext& EventContext)
{
	// Called from inside WriteLock on InterfaceLock
	using namespace UE::AssetRegistry::Premade;
	if (!UE::AssetRegistry::Premade::IsEnabled())
	{
		// if we aren't doing any preloading, then we can set the initial search is done right away.
		// Otherwise, it is set from LoadPremadeAssetRegistry
		bCanFinishInitialSearch = true;
		return;
	}
#if ASSETREGISTRY_ENABLE_PREMADE_REGISTRY_IN_EDITOR
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
	if (CanConsumeAsync())
	{
		AsyncConsumer.PrepareForConsume();
		GPreloader.ConsumeOrDefer(MoveTemp(ConsumeOnCurrentThread), MoveTemp(ConsumeFromAsyncThread));
	}
	else
#endif
	{
		GPreloader.Consume([this, &EventContext](Premade::ELoadResult LoadResult, FAssetRegistryState&& ARState)
			{
				LoadPremadeAssetRegistry(EventContext, LoadResult, MoveTemp(ARState), FAssetRegistryState::EInitializationMode::Rebuild);
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
	Premade::ELoadResult LoadResult, FAssetRegistryState&& ARState, FAssetRegistryState::EInitializationMode Mode)
{
	SCOPED_BOOT_TIMING("LoadPremadeAssetRegistry");
	UE_SCOPED_ENGINE_ACTIVITY("Loading premade asset registry");

	if (SerializationOptions.bSerializeAssetRegistry)
	{
		if (LoadResult == Premade::ELoadResult::Succeeded)
		{
			if (Mode == FAssetRegistryState::EInitializationMode::Rebuild)
			{
				State = MoveTemp(ARState);
				CachePathsFromState(EventContext, State);
			}
			else
			{
				State.InitializeFromExisting(ARState, SerializationOptions, Mode);
				CachePathsFromState(EventContext, ARState);
			}
		}
		else
		{
			UE_CLOG(FPlatformProperties::RequiresCookedData() && (IsRunningGame() || IsRunningDedicatedServer()),
				LogAssetRegistry, Error, TEXT("Failed to load premade asset registry. LoadResult == %d."), static_cast<int32>(LoadResult));
		}
	}

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

				State.InitializeFromExisting(PluginState, SerializationOptions, FAssetRegistryState::EInitializationMode::Append);
				CachePathsFromState(EventContext, PluginState);
			}
		}
	}

	// let Tick know that it can finalize the initial search
	bCanFinishInitialSearch = true;
}

void FAssetRegistryImpl::Initialize(Impl::FInitializeContext& Context)
{
	LLM_SCOPE(ELLMTag::AssetRegistry);

	const double StartupStartTime = FPlatformTime::Seconds();

	bInitialSearchStarted = false;
	bInitialSearchCompleted = true;
	GatherStatus = Impl::EGatherStatus::Active;
	bSearchAllAssets = false;
	AmortizeStartTime = 0;
	TotalAmortizeTime = 0;

	// By default update the disk cache once on asset load, to incorporate changes made in PostLoad. This only happens in editor builds
	bUpdateDiskCacheAfterLoad = true;

	bIsTempCachingAlwaysEnabled = ASSETREGISTRY_CACHE_ALWAYS_ENABLED;
	bIsTempCachingEnabled = bIsTempCachingAlwaysEnabled;
	TempCachedInheritanceBuffer.bDirty = true;

	ClassGeneratorNamesRegisteredClassesVersionNumber = MAX_uint64;

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

	// If in the editor or cookcommandlet, we start the GlobalGatherer now
	// In the game or other commandlets, we do not construct it until project or commandlet code calls SearchAllAssets or ScanPathsSynchronous
	bool bSearchAllAssetsAtStart = GIsEditor && (!IsRunningCommandlet() || IsRunningCookCommandlet());
#if !UE_BUILD_SHIPPING
	bool bCommandlineAllAssetsAtStart;
	if (FParse::Bool(FCommandLine::Get(), TEXT("AssetGatherAll="), bCommandlineAllAssetsAtStart))
	{
		bSearchAllAssetsAtStart = bCommandlineAllAssetsAtStart;
	}
#endif
	if (bSearchAllAssetsAtStart)
	{
		ConstructGatherer();
		if (!GlobalGatherer->IsSynchronous())
		{
			SearchAllAssetsInitialAsync(Context.Events, Context.InheritanceContext);
		}
		else
		{
			// For the Editor we need to take responsibility for the synchronous search; Commandlets will handle that themselves
			Context.bNeedsSearchAllAssetsAtStartSynchronous = GIsEditor && !IsRunningCommandlet();
		}
	}

	ConsumeOrDeferPreloadedPremade(Context.UARI, Context.Events);

	// Report startup time. This does not include DirectoryWatcher startup time.
	UE_LOG(LogAssetRegistry, Log, TEXT("FAssetRegistry took %0.4f seconds to start up"), FPlatformTime::Seconds() - StartupStartTime);

#if WITH_EDITOR
	if (GConfig)
	{
		GConfig->GetBool(TEXT("AssetRegistry"), TEXT("bUpdateDiskCacheAfterLoad"), bUpdateDiskCacheAfterLoad, GEngineIni);
	}
#endif
	Context.bUpdateDiskCacheAfterLoad = bUpdateDiskCacheAfterLoad;

	// Content roots always exist; add them as paths
	FPackageName::QueryRootContentPaths(Context.RootContentPaths);
	for (const FString& AssetPath : Context.RootContentPaths)
	{
		AddPath(Context.Events, AssetPath);
	}

	InitRedirectors(Context.Events, Context.InheritanceContext, Context.bRedirectorsNeedSubscribe);

#if WITH_EDITOR
	if (!UE::AssetDependencyGatherer::Private::FRegisteredAssetDependencyGatherer::IsEmpty())
	{
		TArray<UObject*> Classes;
		GetObjectsOfClass(UClass::StaticClass(), Classes);

		/** Per Class dependency gatherers */
		UE::AssetDependencyGatherer::Private::FRegisteredAssetDependencyGatherer::ForEach([&](UE::AssetDependencyGatherer::Private::FRegisteredAssetDependencyGatherer* RegisteredAssetDependencyGatherer)
		{
			UClass* AssetClass = RegisteredAssetDependencyGatherer->GetAssetClass();
			for(UObject* ClassObject : Classes)
			{
				if (UClass* Class = Cast<UClass>(ClassObject); Class && Class->IsChildOf(AssetClass) && !Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
				{
					RegisteredDependencyGathererClasses.Add(Class, RegisteredAssetDependencyGatherer);
				}
			}
		});
	}
#endif
}

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
		FCoreDelegates::FResolvePackageNameDelegate PackageResolveDelegate;
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
			for (TArray<FString>::TConstIterator RootPathIt(Context.RootContentPaths); RootPathIt; ++RootPathIt)
			{
				const FString& RootPath = *RootPathIt;
				const FString& ContentFolder = FPackageName::LongPackageNameToFilename(RootPath);

				// A missing directory here could be due to a plugin that specifies it contains content, yet has no content yet. PluginManager
				// Mounts these folders anyway which results in them being returned from QueryRootContentPaths
				if (IFileManager::Get().DirectoryExists(*ContentFolder))
				{
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
	}

	if (Context.bUpdateDiskCacheAfterLoad)
	{
		FCoreUObjectDelegates::OnAssetLoaded.AddUObject(this, &UAssetRegistryImpl::OnAssetLoaded);
	}

	if (bAddMetaDataTagsToOnGetExtraObjectTags)
	{
		UObject::FAssetRegistryTag::OnGetExtraObjectTags.AddUObject(this, &UAssetRegistryImpl::OnGetExtraObjectTags);
	}
	if (Context.bNeedsSearchAllAssetsAtStartSynchronous)
	{
		FCoreDelegates::OnFEngineLoopInitComplete.AddUObject(this, &UAssetRegistryImpl::OnFEngineLoopInitCompleteSearchAllAssets);
	}
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
					// Get the guid off the script package, it is updated when script is changed so we need to refresh it every run
					PRAGMA_DISABLE_DEPRECATION_WARNINGS
					ScriptPackageData->PackageGuid = Package->GetGuid();
					PRAGMA_ENABLE_DEPRECATION_WARNINGS
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
	FConfigFile* EngineIni = nullptr;
#if WITH_EDITOR
	// Use passed in platform, or current platform if empty
	FConfigFile PlatformEngineIni;
	FConfigCacheIni::LoadLocalIniFile(PlatformEngineIni, TEXT("Engine"), true, (!PlatformIniName.IsEmpty() ? *PlatformIniName : ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName())));
	EngineIni = &PlatformEngineIni;
#else
	// In cooked builds, always use the normal engine INI
	EngineIni = GConfig->FindConfigFile(GEngineIni);
#endif

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
			TrimmedEntry.RightChopInline(1, false);
		}
		if (TrimmedEntry.Right(1) == TEXT(")"))
		{
			TrimmedEntry.LeftChopInline(1, false);
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
				if (KeyString == TEXT("Class"))
				{
					ClassName = ValueString;
				}
				else if (KeyString == TEXT("Tag"))
				{
					TagName = ValueString;
				}
				else if (KeyString == TEXT("KeepInDevOnly"))
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

void FAssetRegistryImpl::CollectCodeGeneratorClasses()
{
	// Only refresh the list if our registered classes have changed
	if (ClassGeneratorNamesRegisteredClassesVersionNumber == GetRegisteredClassesVersionNumber())
	{
		return;
	}
	ClassGeneratorNamesRegisteredClassesVersionNumber = GetRegisteredClassesVersionNumber();

	// Work around the fact we don't reference Engine module directly
	FTopLevelAssetPath BlueprintCorePathName(TEXT("/Script/Engine"), TEXT("BlueprintCore"));
	UClass* BlueprintCoreClass = FindObject<UClass>(BlueprintCorePathName);
	if (!BlueprintCoreClass)
	{
		return;
	}

	ClassGeneratorNames.Add(BlueprintCoreClass->GetClassPathName());

	TArray<UClass*> BlueprintCoreDerivedClasses;
	GetDerivedClasses(BlueprintCoreClass, BlueprintCoreDerivedClasses);
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
}

}

#if WITH_EDITOR
void UAssetRegistryImpl::OnFEngineLoopInitCompleteSearchAllAssets()
{
	SearchAllAssets(true);
}
#endif

void UAssetRegistryImpl::OnEnginePreExit()
{
	FWriteScopeLock InterfaceScopeLock(InterfaceLock);
	GuardedData.OnEnginePreExit();
}

void UAssetRegistryImpl::FinishDestroy()
{
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
						const FString& ContentFolder = FPackageName::LongPackageNameToFilename(RootPath);
						DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(ContentFolder, OnDirectoryChangedDelegateHandles.FindRef(RootPath));
					}
				}
			}
		}

		if (GuardedData.IsUpdateDiskCacheAfterLoad())
		{
			FCoreUObjectDelegates::OnAssetLoaded.RemoveAll(this);
		}

		if (bAddMetaDataTagsToOnGetExtraObjectTags)
		{
			UObject::FAssetRegistryTag::OnGetExtraObjectTags.RemoveAll(this);
		}
		FCoreDelegates::OnFEngineLoopInitComplete.RemoveAll(this);

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

	bool bIsSynchronous = IsRunningGame();
	GlobalGatherer = MakeUnique<FAssetDataGatherer>(PathsDenyList, ContentSubPathsDenyList, bIsSynchronous);

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
	bInitialSearchStarted = true;
	bInitialSearchCompleted = false;
	FullSearchStartTime = FPlatformTime::Seconds();
	SearchAllAssets(EventContext, InheritanceContext, false /* bSynchronousSearch */);
}

}

void UAssetRegistryImpl::SearchAllAssets(bool bSynchronousSearch)
{
	using namespace UE::AssetRegistry::Impl;

	FEventContext EventContext;
	{
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
	LLM_SCOPE(ELLMTag::AssetRegistry);

	ConstructGatherer();
	FAssetDataGatherer& Gatherer = *GlobalGatherer;
	if (Gatherer.IsSynchronous())
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
	using namespace UE::AssetRegistry::Impl;
	LLM_SCOPE(ELLMTag::AssetRegistry);

	for (;;)
	{
		FEventContext EventContext;
		EGatherStatus Status;
		{
			FWriteScopeLock InterfaceScopeLock(InterfaceLock);
			FClassInheritanceContext InheritanceContext;
			FClassInheritanceBuffer InheritanceBuffer;
			GetInheritanceContextWithRequiredLock(InterfaceScopeLock, InheritanceContext, InheritanceBuffer);

			bool bUnusedInterrupted;
			Status = GuardedData.TickGatherer(EventContext, InheritanceContext, -1., bUnusedInterrupted);
		}
#if WITH_EDITOR
		ProcessLoadedAssetsToUpdateCache(EventContext, -1., Status);
#endif
		Broadcast(EventContext);
		if (Status != EGatherStatus::Active)
		{
			break;
		}

		FThreadHeartBeat::Get().HeartBeat();
		FPlatformProcess::SleepNoStats(0.0001f);
	}
}

void UAssetRegistryImpl::WaitForPackage(const FString& PackageName)
{
	UE::AssetRegistry::Impl::FEventContext EventContext;
	{
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

bool UAssetRegistryImpl::GetAssetsByClass(FName ClassName, TArray<FAssetData>& OutAssetData, bool bSearchSubClasses) const
{
	FTopLevelAssetPath ClassPathName = TryConvertShortTypeNameToPathName(ClassName);
	return GetAssetsByClass(ClassPathName, OutAssetData, bSearchSubClasses);
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

void EnumerateMemoryAssetsHelper(const FARCompiledFilter& InFilter, TSet<FName>& OutPackageNamesWithAssets,
	bool& bOutStopIteration, TFunctionRef<bool(const UObject* Object, FAssetData&& PartialAssetData)> Callback,
	bool bSkipARFilteredAssets)
{
	checkf(IsInGameThread(), TEXT("Enumerating in-memory assets can only be done on the game thread; it uses non-threadsafe UE::AssetRegistry::Filtering globals."));
	bOutStopIteration = false;

	// Skip assets that were loaded for diffing
	const uint32 FilterWithoutPackageFlags = InFilter.WithoutPackageFlags | PKG_ForDiffing;
	const uint32 FilterWithPackageFlags = InFilter.WithPackageFlags;

	auto FilterInMemoryObjectLambda = [&](const UObject* Obj, bool& OutContinue)
	{
		if (Obj->IsAsset())
		{
			// Skip assets that are currently loading
			if (Obj->HasAnyFlags(RF_NeedLoad))
			{
				return;
			}

			UPackage* InMemoryPackage = Obj->GetOutermost();

			// Skip assets with any of the specified 'without' package flags 
			if (InMemoryPackage->HasAnyPackageFlags(FilterWithoutPackageFlags))
			{
				return;
			}

			// Skip assets without any the specified 'with' packages flags
			if (!InMemoryPackage->HasAllPackagesFlags(FilterWithPackageFlags))
			{
				return;
			}

			// Skip classes that report themselves as assets but that the editor AssetRegistry is currently not counting as assets
			if (bSkipARFilteredAssets && UE::AssetRegistry::FFiltering::ShouldSkipAsset(Obj))
			{
				return;
			}

			// Package name
			const FName PackageName = InMemoryPackage->GetFName();

			OutPackageNamesWithAssets.Add(PackageName);

			if (InFilter.PackageNames.Num() && !InFilter.PackageNames.Contains(PackageName))
			{
				return;
			}

			// Asset Path
			const FSoftObjectPath ObjectPath = FSoftObjectPath(Obj);
			if (InFilter.SoftObjectPaths.Num() > 0)
			{
				if (!InFilter.SoftObjectPaths.Contains(ObjectPath))
				{
					return;
				}
			}

			// Package path
			const FString PackageNameStr = InMemoryPackage->GetName();
			if (InFilter.PackagePaths.Num() > 0)
			{
				const FName PackagePath = FName(*FPackageName::GetLongPackagePath(PackageNameStr));
				if (!InFilter.PackagePaths.Contains(PackagePath))
				{
					return;
				}
			}

			// Could perhaps save some FName -> String conversions by creating this a bit earlier using the UObject constructor
			// to get package name and path.
			FAssetData PartialAssetData(PackageNameStr, ObjectPath.ToString(), Obj->GetClass()->GetClassPathName(), FAssetDataTagMap(),
				InMemoryPackage->GetChunkIDs(), InMemoryPackage->GetPackageFlags());

			// All filters passed, except for AssetRegistry filter; caller must check that one
			OutContinue = Callback(Obj, MoveTemp(PartialAssetData));
		}
	};

	// Iterate over all in-memory assets to find the ones that pass the filter components
	if (InFilter.ClassPaths.Num() > 0)
	{
		TArray<UObject*> InMemoryObjects;
		for (FTopLevelAssetPath ClassName : InFilter.ClassPaths)
		{
			UClass* Class = FindObject<UClass>(ClassName);
			if (Class != nullptr)
			{
				GetObjectsOfClass(Class, InMemoryObjects, false, RF_NoFlags);
			}
		}

		for (UObject* Object : InMemoryObjects)
		{
			bool bContinue = true;
			FilterInMemoryObjectLambda(Object, bContinue);
			if (!bContinue)
			{
				bOutStopIteration = true;
				return;
			}
		}
	}
	else
	{
		for (FThreadSafeObjectIterator ObjIt; ObjIt; ++ObjIt)
		{
			bool bContinue = true;
			FilterInMemoryObjectLambda(*ObjIt, bContinue);
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
	bool& bOutStopIteration, TFunctionRef<bool(FAssetData&&)> Callback, bool bSkipARFilteredAssets)
{
	check(!InFilter.IsEmpty() && Utils::IsFilterValid(InFilter));
	EnumerateMemoryAssetsHelper(InFilter, OutPackageNamesWithAssets, bOutStopIteration,
		[&InFilter, &Callback](const UObject* Object, FAssetData&& PartialAssetData)
		{
			Object->GetAssetRegistryTags(PartialAssetData);
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

FAssetData UAssetRegistryImpl::GetAssetByObjectPath(const FSoftObjectPath& ObjectPath, bool bIncludeOnlyOnDiskAssets) const
{
	if (!bIncludeOnlyOnDiskAssets)
	{
		TStringBuilder<FName::StringBufferSize> Builder;
		ObjectPath.ToString(Builder);
		UObject* Asset = FindObject<UObject>(nullptr, *Builder);

		if (Asset)
		{
			if (!UE::AssetRegistry::FFiltering::ShouldSkipAsset(Asset))
			{
				return FAssetData(Asset, false /* bAllowBlueprintClass */);
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
		return (FoundData && !GuardedData.ShouldSkipAsset(FoundData->AssetClassPath, FoundData->PackageFlags)) ? *FoundData : FAssetData();
	}
}

FAssetData UAssetRegistryImpl::GetAssetByObjectPath(const FName ObjectPath, bool bIncludeOnlyOnDiskAssets) const
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	return GetAssetByObjectPath(FSoftObjectPath(ObjectPath.ToString()));
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

bool UAssetRegistryImpl::GetDependencies(const FAssetIdentifier& AssetIdentifier, TArray<FAssetIdentifier>& OutDependencies, EAssetRegistryDependencyType::Type InDependencyType) const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return GuardedData.GetState().GetDependencies(AssetIdentifier, OutDependencies, InDependencyType);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
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
	OutPackageNames.SetNum(UniqueNum,false);
}

bool UAssetRegistryImpl::GetDependencies(FName PackageName, TArray<FName>& OutDependencies, EAssetRegistryDependencyType::Type InDependencyType) const
{
	TArray<FAssetIdentifier> TempDependencies;
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (!GetDependencies(PackageName, TempDependencies, InDependencyType))
	{
		return false;
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	ConvertAssetIdentifiersToPackageNames(TempDependencies, OutDependencies);
	return true;
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

bool UAssetRegistryImpl::GetReferencers(const FAssetIdentifier& AssetIdentifier, TArray<FAssetIdentifier>& OutReferencers, EAssetRegistryDependencyType::Type InReferenceType) const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return GuardedData.GetState().GetReferencers(AssetIdentifier, OutReferencers, InReferenceType);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
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

bool UAssetRegistryImpl::GetReferencers(FName PackageName, TArray<FName>& OutReferencers, EAssetRegistryDependencyType::Type InReferenceType) const
{
	TArray<FAssetIdentifier> TempReferencers;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (!GetReferencers(FAssetIdentifier(PackageName), TempReferencers, InReferenceType))
	{
		return false;
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	ConvertAssetIdentifiersToPackageNames(TempReferencers, OutReferencers);
	return true;
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

const FAssetPackageData* UAssetRegistryImpl::GetAssetPackageData(FName PackageName) const
{
	FWriteScopeLock InterfaceScopeLock(InterfaceLock);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return const_cast<UE::AssetRegistry::FAssetRegistryImpl&>(GuardedData).GetAssetPackageData(PackageName);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

namespace UE::AssetRegistry
{

const FAssetPackageData* FAssetRegistryImpl::GetAssetPackageData(FName PackageName)
{
	const FAssetPackageData* AssetPackageData = State.GetAssetPackageData(PackageName);
	if (!AssetPackageData)
	{
		return nullptr;
	}
	FAssetPackageData* Result = new FAssetPackageData(*AssetPackageData);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	DeleteActions.Add([Result]() { delete Result; });
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	return Result;
}

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
			// TODO: Save the extension on the AssetPackageData rather than deriving it here
			// Guessing the extension based on map vs non-map also does not support text assets and maps which have a different extension
			TArray<FAssetData> Assets;
			GetAssetsByPackageName(PackageName, Assets, /*bIncludeOnlyDiskAssets*/ true);
			*OutExtension = CalculateExtension(PackageNameStr, Assets);
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

FSoftObjectPath UAssetRegistryImpl::GetRedirectedObjectPath(const FSoftObjectPath& ObjectPath) const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	return GuardedData.GetRedirectedObjectPath(ObjectPath);
}

FName UAssetRegistryImpl::GetRedirectedObjectPath(const FName ObjectPath) const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return GuardedData.GetRedirectedObjectPath(ObjectPath);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

namespace UE::AssetRegistry
{

FSoftObjectPath FAssetRegistryImpl::GetRedirectedObjectPath(const FSoftObjectPath& ObjectPath) const
{
	FSoftObjectPath RedirectedPath = ObjectPath;

	// For legacy behavior, for the first object pointed to, we look up the object in memory
	// before checking the on-disk assets
	UObject* Asset = ObjectPath.ResolveObject();
	const FAssetData* AssetData = nullptr;
	if (!Asset)
	{
		AssetData = State.GetAssetByObjectPath(ObjectPath);
	}

	TSet<FSoftObjectPath> SeenPaths;
	SeenPaths.Add(RedirectedPath);

	auto TryGetRedirectedPath = [](UObject* InAsset, const FAssetData* InAssetData, FSoftObjectPath& OutRedirectedPath)
	{
		if (InAsset)
		{
			UObjectRedirector* Redirector = Cast<UObjectRedirector>(InAsset);
			if (Redirector && Redirector->DestinationObject)
			{
				OutRedirectedPath = FSoftObjectPath(Redirector->DestinationObject);
				return true;
			}
		}
		else if (InAssetData && InAssetData->IsRedirector())
		{
			FString Dest;
			if (InAssetData->GetTagValue("DestinationObject", Dest))
			{
				ConstructorHelpers::StripObjectClass(Dest);
				OutRedirectedPath = Dest;
				return true;
			}
		}
		return false;
	};

	// Need to follow chain of redirectors
	while (TryGetRedirectedPath(Asset, AssetData, RedirectedPath))
	{
		if (SeenPaths.Contains(RedirectedPath))
		{
			// Recursive, bail
			break;
		}
		else
		{
			SeenPaths.Add(RedirectedPath);
			// For legacy behavior, for all redirects after the initial request, we only check on-disk assets
			Asset = nullptr;
			AssetData = State.GetAssetByObjectPath(RedirectedPath);
		}
	}

	return RedirectedPath;
}

UE_DEPRECATED(5.1, "Asset path FNames have been deprecated, use FSoftObjectPath instead.")
FName FAssetRegistryImpl::GetRedirectedObjectPath(const FName ObjectPath) const
{
	return FName(*GetRedirectedObjectPath(FSoftObjectPath(ObjectPath.ToString())).ToString());
}

}

bool UAssetRegistryImpl::GetAncestorClassNames(FName ClassName, TArray<FName>& OutAncestorClassNames) const
{
	FTopLevelAssetPath ClassPathName = TryConvertShortTypeNameToPathName(ClassName);
	TArray<FTopLevelAssetPath> OutAncestorClassPathNames;
	bool bResult = GetAncestorClassNames(ClassPathName, OutAncestorClassPathNames);
	for (FTopLevelAssetPath AncestorPathName : OutAncestorClassPathNames)
	{
		OutAncestorClassNames.Add(AncestorPathName.GetAssetName());
	}
	return bResult;
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

void UAssetRegistryImpl::GetDerivedClassNames(const TArray<FName>& ClassNames, const TSet<FName>& ExcludedClassNames,
	TSet<FName>& OutDerivedClassNames) const
{
	TArray<FTopLevelAssetPath> ClassPaths;
	for (FName ClassName : ClassNames)
	{
		ClassPaths.Add(TryConvertShortTypeNameToPathName(ClassName));
	}
	TSet<FTopLevelAssetPath> ExcludedClassPathNames;
	for (FName ExcludedClassName : ExcludedClassNames)
	{
		ExcludedClassPathNames.Add(TryConvertShortTypeNameToPathName(ExcludedClassName));
	}
	TSet<FTopLevelAssetPath> OutDerivedClassPathNames;
	GetDerivedClassNames(ClassPaths, ExcludedClassPathNames, OutDerivedClassPathNames);
	for (FTopLevelAssetPath DerivedClassPathName : OutDerivedClassPathNames)
	{
		OutDerivedClassNames.Add(DerivedClassPathName.GetAssetName());
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

	// Spin the array backwards to minimize the number of elements that are repeatedly moved down
	for (int32 AssetDataIndex = AssetDataList.Num() - 1; AssetDataIndex >= 0; --AssetDataIndex)
	{
		const bool bFilterResult = RunAssetThroughFilter_Unchecked(AssetDataList[AssetDataIndex], CompiledFilter, bPassFilterValue);
		if (bFilterResult != bPassFilterValue)
		{
			AssetDataList.RemoveAt(AssetDataIndex, 1, /*bAllowShrinking*/false);
			continue;
		}
	}
	if (OriginalArrayCount > AssetDataList.Num())
	{
		AssetDataList.Shrink();
	}
}

}

void UAssetRegistryImpl::ExpandRecursiveFilter(const FARFilter& InFilter, FARFilter& ExpandedFilter) const
{
	FARCompiledFilter CompiledFilter;
	CompileFilter(InFilter, CompiledFilter);

	ExpandedFilter.Clear();
	ExpandedFilter.PackageNames = CompiledFilter.PackageNames.Array();
	ExpandedFilter.PackagePaths = CompiledFilter.PackagePaths.Array();
	ExpandedFilter.SoftObjectPaths = CompiledFilter.SoftObjectPaths.Array();
	ExpandedFilter.ClassPaths = CompiledFilter.ClassPaths.Array();
	ExpandedFilter.TagsAndValues = CompiledFilter.TagsAndValues;
	ExpandedFilter.bIncludeOnlyOnDiskAssets = CompiledFilter.bIncludeOnlyOnDiskAssets;
	ExpandedFilter.WithoutPackageFlags = CompiledFilter.WithoutPackageFlags;
	ExpandedFilter.WithPackageFlags = CompiledFilter.WithPackageFlags;
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
					UE_LOG(LogAssetRegistry, Error, TEXT("Failed to resolve class path for short clas name \"%s\" when compiling asset registry filter"), *ClassName.ToString());
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
	IPlatformChunkInstall* ChunkInstall = FPlatformMisc::GetPlatformChunkInstall();
	EChunkProgressReportingType::Type ChunkReportType = GetChunkAvailabilityProgressType(ReportType);

	bool IsPercentageComplete = (ChunkReportType == EChunkProgressReportingType::PercentageComplete) ? true : false;
	check(ReportType == EAssetAvailabilityProgressReportingType::PercentageComplete || ReportType == EAssetAvailabilityProgressReportingType::ETA);

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
	IPlatformChunkInstall* ChunkInstall = FPlatformMisc::GetPlatformChunkInstall();
	return ChunkInstall->GetProgressReportingTypeSupported(GetChunkAvailabilityProgressType(ReportType));
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
	IPlatformChunkInstall* ChunkInstall = FPlatformMisc::GetPlatformChunkInstall();

	const TConstArrayView<int32> ChunkIDs = AssetData.GetChunkIDs();
	if (ChunkIDs.Num() == 0)
	{
		return;
	}

	ChunkInstall->PrioritizePakchunk(ChunkIDs[0], EChunkPriority::Immediate);
}

}

bool UAssetRegistryImpl::GetVerseFilesByPath(FName PackagePath, TArray<FName>& OutFilePaths, bool bRecursive /*= false*/) const
{
	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	return GuardedData.GetVerseFilesByPath(PackagePath, OutFilePaths, bRecursive);
}

namespace UE::AssetRegistry
{

bool FAssetRegistryImpl::GetVerseFilesByPath(FName PackagePath, TArray<FName>& OutFilePaths, bool bRecursive /*= false*/) const
{
	bool bFoundAnything = false;
	TSet<FName> PathList;
	PathList.Reserve(32);
	if (bRecursive)
	{
		CachedPathTree.GetSubPaths(PackagePath, PathList, true);
	}
	PathList.Add(PackagePath);
	for (const FName& PathName : PathList)
	{
		const TArray<FName>* FilePaths = CachedVerseFilesByPath.Find(PathName);
		if (FilePaths)
		{
			OutFilePaths.Append(*FilePaths);
			bFoundAnything = true;
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
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		bResult = GuardedData.AddPath(EventContext, PathToAdd);
	}
	Broadcast(EventContext);
	return bResult;
}

namespace UE::AssetRegistry
{

bool FAssetRegistryImpl::AddPath(Impl::FEventContext& EventContext, const FString& PathToAdd)
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
	return AddAssetPath(EventContext, FName(*PathToAdd));
}

}

bool UAssetRegistryImpl::RemovePath(const FString& PathToRemove)
{
	UE::AssetRegistry::Impl::FEventContext EventContext;
	bool bResult;
	{
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
	ScanPathsSynchronousInternal(InPaths, TArray<FString>(), bForceRescan, bIgnoreDenyListScanFilters);
}

void UAssetRegistryImpl::ScanFilesSynchronous(const TArray<FString>& InFilePaths, bool bForceRescan)
{
	ScanPathsSynchronousInternal(TArray<FString>(), InFilePaths, bForceRescan,
		false /* bIgnoreDenyListScanFilters */);
}

void UAssetRegistryImpl::ScanPathsSynchronousInternal(const TArray<FString>& InDirs, const TArray<FString>& InFiles,
	bool bInForceRescan, bool bInIgnoreDenyListScanFilters)
{
	UE_SCOPED_IO_ACTIVITY(*WriteToString<256>("Scan Paths"));

	TRACE_CPUPROFILER_EVENT_SCOPE(UAssetRegistryImpl::ScanPathsSynchronousInternal);
	UE_TRACK_REFERENCING_OPNAME_SCOPED(PackageAccessTrackingOps::NAME_ResetContext);
	const double SearchStartTime = FPlatformTime::Seconds();

	UE::AssetRegistry::Impl::FEventContext EventContext;
	UE::AssetRegistry::Impl::FClassInheritanceContext InheritanceContext;
	UE::AssetRegistry::Impl::FClassInheritanceBuffer InheritanceBuffer;
	UE::AssetRegistry::Impl::FScanPathContext Context(EventContext, InheritanceContext, InDirs, InFiles,
		bInForceRescan, bInIgnoreDenyListScanFilters, nullptr /* OutFindAssets */);

	{
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		GetInheritanceContextWithRequiredLock(InterfaceScopeLock, InheritanceContext, InheritanceBuffer);

		// make sure any outstanding async preload is complete
		GuardedData.ConditionalLoadPremadeAssetRegistry(*this, EventContext, InterfaceScopeLock);
		GuardedData.ScanPathsSynchronous(Context);
	}
	if (Context.LocalPaths.IsEmpty())
	{
		return;
	}

#if WITH_EDITOR
	ProcessLoadedAssetsToUpdateCache(EventContext, -1., Context.Status);
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

	UE_LOG(LogAssetRegistry, Verbose, TEXT("ScanPathsSynchronous completed scanning %s to find %d assets in %0.4f seconds"), *PathsString,
		Context.NumFoundAssets, FPlatformTime::Seconds() - SearchStartTime);
}

void UAssetRegistryImpl::PrioritizeSearchPath(const FString& PathToPrioritize)
{
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

	// Also prioritize the queue of background search results
	int32 FirstNonPriorityIndex = 0;
	for (int Index = 0; Index < BackgroundResults.Assets.Num(); ++Index)
	{
		FAssetData* PriorityElement = BackgroundResults.Assets[Index];
		if (PriorityElement && PriorityElement->PackagePath.ToString().StartsWith(PathToPrioritize))
		{
			Swap(BackgroundResults.Assets[FirstNonPriorityIndex++], BackgroundResults.Assets[Index]);
		}
	}
	FirstNonPriorityIndex = 0;
	for (int Index = 0; Index < BackgroundResults.Paths.Num(); ++Index)
	{
		FString& PriorityElement = BackgroundResults.Paths[Index];
		if (PriorityElement.StartsWith(PathToPrioritize))
		{
			Swap(BackgroundResults.Paths[FirstNonPriorityIndex++], BackgroundResults.Paths[Index]);
		}
	}
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
			AssetAddedEvent.Broadcast(FAssetData(NewAsset, true /* bAllowBlueprintClass */));

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
			FAssetData AssetDataDeleted = FAssetData(DeletedAsset, true /* bAllowBlueprintClass */);

			checkf(IsInGameThread(), TEXT("AssetDeleted is not yet implemented as callable from other threads"));
			// Let subscribers know that the asset was removed from the registry
			AssetRemovedEvent.Broadcast(AssetDataDeleted);

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

		bool bShouldSkipAsset;
		UE::AssetRegistry::Impl::FEventContext EventContext;
		{
			FWriteScopeLock InterfaceScopeLock(InterfaceLock);
			GuardedData.RemoveEmptyPackage(NewPackage->GetFName());

			if (OldPackage && UPackage::IsEmptyPackage(OldPackage))
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
			AssetRenamedEvent.Broadcast(FAssetData(RenamedAsset, true /* bAllowBlueprintClass */), OldObjectPath);
		}
	}
}

void UAssetRegistryImpl::AssetSaved(const UObject& SavedAsset)
{
#if WITH_EDITOR
	if (!ensure(SavedAsset.IsAsset()))
	{
		return;
	}

	FWriteScopeLock InterfaceScopeLock(InterfaceLock);
	GuardedData.AddLoadedAssetToProcess(SavedAsset);
#endif
}

void UAssetRegistryImpl::AssetTagsFinalized(const UObject& FinalizedAsset)
{
#if WITH_EDITOR
	if (!FinalizedAsset.IsAsset())
	{
		return;
	}

	FWriteScopeLock InterfaceScopeLock(InterfaceLock);
	GuardedData.AddLoadedAssetToProcess(FinalizedAsset);
#endif
}

void UAssetRegistryImpl::PackageDeleted(UPackage* DeletedPackage)
{
	checkf(GIsEditor, TEXT("Updating the AssetRegistry is only available in editor"));
	UE::AssetRegistry::Impl::FEventContext EventContext;
	if (ensure(DeletedPackage))
	{
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
	LLM_SCOPE(ELLMTag::AssetRegistry);

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
			FWriteScopeLock InterfaceScopeLock(InterfaceLock);
			UE::AssetRegistry::Impl::FClassInheritanceContext InheritanceContext;
			UE::AssetRegistry::Impl::FClassInheritanceBuffer InheritanceBuffer;
			GetInheritanceContextWithRequiredLock(InterfaceScopeLock, InheritanceContext, InheritanceBuffer);

			// Process any deferred events and deletes
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			GuardedData.TickDeletes();
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
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
	} while (bInterrupted && (TickStartTime < 0 || (FPlatformTime::Seconds() - TickStartTime) <= UE::AssetRegistry::Impl::MaxSecondsPerFrame));
}

namespace UE::AssetRegistry
{

void FAssetRegistryImpl::TickDeletes()
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	for (TUniqueFunction<void()>& Action : DeleteActions)
	{
		Action();
	}
	DeleteActions.Empty();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
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
	auto UpdateStatus = [bHadAssetsToProcess, &ResultContext, &EventContext, &OutStatus, this](int32 NumGatherPending, bool bInterrupted)
	{
		// Compute total pending, plus highest pending for this run so we can show a good progress bar
		int32 NumPending = NumGatherPending
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
		PathDataGathered(EventContext, TickStartTime, BackgroundResults.Paths);
	}

	// Process the asset results
	if (BackgroundResults.Assets.Num())
	{
		// Mark the first amortize time
		if (AmortizeStartTime == 0)
		{
			AmortizeStartTime = FPlatformTime::Seconds();
		}
		if (AssetsFoundCallback.IsSet())
		{
			AssetsFoundCallback.GetValue()(BackgroundResults.Assets);
		}

		AssetSearchDataGathered(EventContext, TickStartTime, BackgroundResults.Assets);

		if (BackgroundResults.Assets.Num() == 0)
		{
			TotalAmortizeTime += FPlatformTime::Seconds() - AmortizeStartTime;
			AmortizeStartTime = 0;
		}
	}

	// Add dependencies
	if (BackgroundResults.Dependencies.Num())
	{
		DependencyDataGathered(TickStartTime, BackgroundResults.Dependencies);
	}

	// Load cooked packages that do not have asset data
	if (BackgroundResults.CookedPackageNamesWithoutAssetData.Num())
	{
		CookedPackageNamesWithoutAssetDataGathered(EventContext, TickStartTime, BackgroundResults.CookedPackageNamesWithoutAssetData, bOutInterrupted);
		if (bOutInterrupted)
		{
			UpdateStatus(GetNumGatherFromDiskPending(), true /* bInterrupted */);
			return OutStatus;
		}
	}

	// Add Verse files
	if (BackgroundResults.VerseFiles.Num())
	{
		VerseFilesGathered(TickStartTime, BackgroundResults.VerseFiles);
	}

	// Load Calculated Dependencies when the gather from disk is complete; the full gather is not complete until after this is done
	int32 NumGatherFromDiskPending = GetNumGatherFromDiskPending();
#if WITH_EDITOR
	bool bDiskGatherComplete = !ResultContext.bIsSearching && NumGatherFromDiskPending == 0;
	if (bDiskGatherComplete && PackagesNeedingDependencyCalculation.Num())
	{
		LoadCalculatedDependencies(nullptr, TickStartTime, InheritanceContext, bOutInterrupted);
		if (bOutInterrupted)
		{
			UpdateStatus(NumGatherFromDiskPending, true /* bInterrupted */);
			return OutStatus;
		}
	}
#endif

	// If completing an initial search, refresh the content browser
	UpdateStatus(NumGatherFromDiskPending, false /* bInterrupted */);

	if (OutStatus == EGatherStatus::Complete)
	{
		HighestPending = 0;

		if (!bInitialSearchCompleted && bCanFinishInitialSearch)
		{
#if WITH_EDITOR
			// update redirectors
			UpdateRedirectCollector();
#endif
			UE_LOG(LogAssetRegistry, Verbose, TEXT("### Time spent amortizing search results: %0.4f seconds"), TotalAmortizeTime);
			UE_LOG(LogAssetRegistry, Log, TEXT("Asset discovery search completed in %0.4f seconds"), FPlatformTime::Seconds() - FullSearchStartTime);

			bInitialSearchCompleted = true;

			EventContext.bFileLoadedEventBroadcast = true;
		}
	}

	return OutStatus;
}

void FAssetRegistryImpl::TickGatherPackage(Impl::FEventContext& EventContext, const FString& PackageName, const FString& LocalPath)
{
	if (!GlobalGatherer.IsValid())
	{
		return;
	}
	GlobalGatherer->WaitOnPath(LocalPath);

	FName PackageFName(PackageName);

	// Gather results from the background search
	GlobalGatherer->GetPackageResults(BackgroundResults.Assets, BackgroundResults.Dependencies);

	TRingBuffer<FAssetData*> PackageAssets;
	TRingBuffer<FPackageDependencyData> PackageDependencyDatas;
	for (int n = 0; n < BackgroundResults.Assets.Num(); ++n)
	{
		FAssetData* Asset = BackgroundResults.Assets[n];
		if (Asset->PackageName == PackageFName)
		{
			Swap(BackgroundResults.Assets[n], BackgroundResults.Assets.Last());
			PackageAssets.Add(BackgroundResults.Assets.PopValue());
			--n;
		}
	}
	for (int n = 0; n < BackgroundResults.Dependencies.Num(); ++n)
	{
		FPackageDependencyData& DependencyData = BackgroundResults.Dependencies[n];
		if (DependencyData.PackageName == PackageFName)
		{
			Swap(BackgroundResults.Dependencies[n], BackgroundResults.Dependencies.Last());
			PackageDependencyDatas.Add(BackgroundResults.Dependencies.PopValue());
			--n;
		}
	}
	if (PackageAssets.Num() > 0)
	{
		AssetSearchDataGathered(EventContext, -1., PackageAssets);
	}
	if (PackageDependencyDatas.Num() > 0)
	{
		DependencyDataGathered(-1., PackageDependencyDatas);
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

	for (const FAssetData* AssetData : State.GetAssetsByPackageName(PackageName))
	{
		if (UE::AssetDependencyGatherer::Private::FRegisteredAssetDependencyGatherer** AssetDependencyGatherer = RegisteredDependencyGathererClasses.Find(AssetData->GetClass()))
		{
			if (!bOutHadActivity)
			{
				RemoveDirectoryReferencer(PackageName);
			}

			bOutHadActivity = true;
			TArray<IAssetDependencyGatherer::FGathereredDependency> GatheredDependencies;
			TArray<FString> DirectoryReferences;

			(*AssetDependencyGatherer)->GatherDependencies(*AssetData, State, GetCompiledFilter, GatheredDependencies, DirectoryReferences);
			
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
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		GuardedData.AppendState(EventContext, InState);
	}

	Broadcast(EventContext);
	checkf(IsInGameThread(), TEXT("AppendState is not yet implemented as callable from other threads"));
	InState.EnumerateAllAssets(TSet<FName>(), [this](const FAssetData& AssetData)
	{
		// Let subscribers know that the new asset was added to the registry
		AssetAddedEvent.Broadcast(AssetData);
		return true;
	}, true /*bARFiltering*/);
}

namespace UE::AssetRegistry
{

void FAssetRegistryImpl::AppendState(Impl::FEventContext& EventContext, const FAssetRegistryState& InState)
{
	State.InitializeFromExisting(InState, SerializationOptions, FAssetRegistryState::EInitializationMode::Append);
	CachePathsFromState(EventContext, InState);
}

void FAssetRegistryImpl::CachePathsFromState(Impl::FEventContext& EventContext, const FAssetRegistryState& InState)
{
	SCOPED_BOOT_TIMING("FAssetRegistryImpl::CachePathsFromState");

	LLM_SCOPE(ELLMTag::AssetRegistry);

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

void UAssetRegistryImpl::InitializeTemporaryAssetRegistryState(FAssetRegistryState& OutState, const FAssetRegistrySerializationOptions& Options, bool bRefreshExisting) const
{
	using FAssetDataMap = UE::AssetRegistry::Private::FAssetDataMap;

	FReadScopeLock InterfaceScopeLock(InterfaceLock);
	const FAssetRegistryState& State = GuardedData.GetState();
	OutState.InitializeFromExisting(State.CachedAssets, State.CachedDependsNodes, State.CachedPackageData, Options, bRefreshExisting ? FAssetRegistryState::EInitializationMode::OnlyUpdateExisting : FAssetRegistryState::EInitializationMode::Rebuild);
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
		if (!FPackageName::TryConvertToMountedPath(InFile, &LocalPath, &PackageName, nullptr, nullptr, &Extension, &FlexNameType))
		{
			UE_LOG(LogAssetRegistry, Warning, TEXT("ScanPathsSynchronous: %s is not in a mounted path, will not scan."), *InFile);
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
		if (!FPackageName::TryConvertToMountedPath(InDir, &LocalPath, &PackageName, nullptr, nullptr, &Extension, &FlexNameType))
		{
			UE_LOG(LogAssetRegistry, Warning, TEXT("ScanPathsSynchronous: %s is not in a mounted path, will not scan."), *InDir);
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
	if (!Context.bForceRescan && Gatherer.IsCacheEnabled())
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
		Gatherer.LoadCacheFile(CacheFilename);
	}

	Gatherer.ScanPathsSynchronous(Context.LocalPaths, Context.bForceRescan, Context.bIgnoreDenyListScanFilters, CacheFilename, Context.PackageDirs);
	TArray<FName> FoundAssetPackageNames;

	auto AssetsFoundCallback = [&Context, &FoundAssetPackageNames, this](const TRingBuffer<FAssetData*>& InFoundAssets)
	{
		Context.NumFoundAssets = InFoundAssets.Num();

		FoundAssetPackageNames.Reset();
		FoundAssetPackageNames.Reserve(Context.NumFoundAssets);

		// The gatherer may have added other assets that were scanned as part of the ongoing background scan; remove any assets that were not in the requested paths
		for (FAssetData* AssetData : InFoundAssets)
		{
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
		}
	};

	bool bUnusedInterrupted;
	Context.Status = TickGatherer(Context.EventContext, Context.InheritanceContext, -1., bUnusedInterrupted,
		FAssetsFoundCallback(AssetsFoundCallback));
#if WITH_EDITOR
	LoadCalculatedDependencies(&FoundAssetPackageNames, -1., Context.InheritanceContext, bUnusedInterrupted);
#endif
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
						TagsAndValues.Remove(Tag.Name);
						TagsAndValues.Add(Tag.Name, Tag.Value);
					}
					AssetData->TagsAndValues = FAssetDataTagMapSharedView(MoveTemp(TagsAndValues));
				}
			}
		}
	}
}
#endif

void FAssetRegistryImpl::AssetSearchDataGathered(Impl::FEventContext& EventContext, const double TickStartTime, TRingBuffer<FAssetData*>& AssetResults)
{
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
	while (AssetResults.Num() > 0)
	{
		// Delete or take ownership of the BackgroundResult; it was originally new'd by an FPackageReader
		TUniquePtr<FAssetData> BackgroundResult(AssetResults.PopFrontValue());
		CA_ASSUME(BackgroundResult.Get() != nullptr);

		// Try to update any asset data that may already exist
		FCachedAssetKey Key(*BackgroundResult);
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
				// If the current AssetData came from a loaded asset, don't overwrite it with the new one from disk; loaded asset is more authoritative because it has run the postload steps
#if WITH_EDITOR
				if (!AssetDataObjectPathsUpdatedOnLoad.Contains(BackgroundResult->GetSoftObjectPath()))
#endif
				{
#if WITH_EDITOR
					PostLoadAssetRegistryTags(BackgroundResult.Get());
#endif
					// The asset exists in the cache from disk and has not yet been loaded into memory, update it with the new background data
					UpdateAssetData(EventContext, ExistingAssetData, MoveTemp(*BackgroundResult));
				}
			}
		}
		else
		{
			// The asset isn't in the cache yet, add it and notify subscribers
			if (bPathIsMounted)
			{
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
	const bool bFlushFullBuffer = TickStartTime < 0;

	TSet<FString> MountPoints;
	FString PackageRoot;
	if (PathResults.Num() > 0 && bVerifyMountPointAfterGather)
	{
		TArray<FString> MountPointsArray;
		FPackageName::QueryRootContentPaths(MountPointsArray, /*bIncludeReadOnlyRoots=*/ true, /*bWithoutLeadingSlashes*/ false, /*WithoutTrailingSlashes=*/ true);
		MountPoints.Append(MoveTemp(MountPointsArray));
	}

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

void FAssetRegistryImpl::DependencyDataGathered(const double TickStartTime, TRingBuffer<FPackageDependencyData>& DependsResults)
{
	using namespace UE::AssetRegistry;
	const bool bFlushFullBuffer = TickStartTime < 0;

	while (DependsResults.Num() > 0)
	{
		FPackageDependencyData Result = DependsResults.PopFrontValue();

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
				FName Redirected = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Package,
					FCoreRedirectObjectName(NAME_None, NAME_None, DependencyPackageName)).PackageName;
				DependencyPackageName = Redirected;

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
						// This was newly created, see if we need to read the script package Guid
						const FNameBuilder DependencyPackageNameStr(DependencyPackageName);

						if (FPackageName::IsScriptPackage(DependencyPackageNameStr))
						{
							// Get the guid off the script package, it is updated when script is changed so we need to refresh it every run
							UPackage* Package = FindPackage(nullptr, *DependencyPackageNameStr);

							if (Package)
							{
								FAssetPackageData* ScriptPackageData = State.CreateOrGetAssetPackageData(DependencyPackageName);
								PRAGMA_DISABLE_DEPRECATION_WARNINGS
								ScriptPackageData->PackageGuid = Package->GetGuid();
								PRAGMA_ENABLE_DEPRECATION_WARNINGS
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

void FAssetRegistryImpl::VerseFilesGathered(const double TickStartTime, TRingBuffer<FName>& VerseResults)
{
	while (VerseResults.Num() > 0)
	{
		FName VerseFilePath = VerseResults.PopFrontValue();

		bool bAlreadyExists = false;
		CachedVerseFiles.Add(VerseFilePath, &bAlreadyExists);
		if (!bAlreadyExists)
		{
			WriteToString<256> VerseFilePathString(VerseFilePath);
			FName VerseDirectoryPath(FPathViews::GetPath(VerseFilePathString.ToView()));
			TArray<FName>& FilePathsArray = CachedVerseFilesByPath.FindOrAdd(VerseDirectoryPath);
			FilePathsArray.Add(VerseFilePath);
		}

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
			const FTopLevelAssetPath GeneratedClassPathName(GeneratedClass);
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

void FAssetRegistryImpl::UpdateAssetData(Impl::FEventContext& EventContext, FAssetData* AssetData, FAssetData&& NewAssetData)
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

	bool bModified;
	State.UpdateAssetData(AssetData, MoveTemp(NewAssetData), &bModified);
	
	if (bModified && !ShouldSkipAsset(AssetData->AssetClassPath, AssetData->PackageFlags))
	{
		EventContext.AssetEvents.Emplace(*AssetData, Impl::FEventContext::EEvent::Updated);
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

void FAssetRegistryImpl::RemoveVerseFile(FName VerseFilePathToRemove)
{
	if (CachedVerseFiles.Remove(VerseFilePathToRemove))
	{
		WriteToString<256> VerseFilePathString(VerseFilePathToRemove);
		FName VerseDirectoryPath(FPathViews::GetPath(VerseFilePathString.ToView()));
		TArray<FName>* FilePathsArray = CachedVerseFilesByPath.Find(VerseDirectoryPath);
		if (ensure(FilePathsArray)) // We found it in CachedVerseFiles, so we must also find it here
		{
			FilePathsArray->Remove(VerseFilePathToRemove);
			if (FilePathsArray->IsEmpty())
			{
				CachedVerseFilesByPath.Remove(VerseDirectoryPath);
			}
		}
	}
}

}

#if WITH_EDITOR

void UAssetRegistryImpl::OnDirectoryChanged(const TArray<FFileChangeData>& FileChanges)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UAssetRegistryImpl::OnDirectoryChanged);

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

	UE::AssetRegistry::Impl::FEventContext EventContext;
	{
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		UE::AssetRegistry::Impl::FClassInheritanceContext InheritanceContext;
		UE::AssetRegistry::Impl::FClassInheritanceBuffer InheritanceBuffer;
		GetInheritanceContextWithRequiredLock(InterfaceScopeLock, InheritanceContext, InheritanceBuffer);
		GuardedData.OnDirectoryChanged(EventContext, InheritanceContext, FileChangesProcessed);
	}
	Broadcast(EventContext);
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
					WriteToString<256> VerseFilePathString(LongPackageName, FPathViews::GetExtension(File, true));
					RemoveVerseFile(*VerseFilePathString);
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
					RemoveAssetPath(EventContext, *LongPackageName);
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

}

void UAssetRegistryImpl::OnAssetLoaded(UObject *AssetLoaded)
{
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

	LLM_SCOPE(ELLMTag::AssetRegistry);


	constexpr int32 BatchSize = 16;
	TArray<const UObject*> BatchObjects;
	TArray<FAssetData, TInlineAllocator<BatchSize>> BatchAssetDatas;

	{
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		GuardedData.GetProcessLoadedAssetsBatch(BatchObjects, BatchSize);
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
			BatchAssetDatas.Add(FAssetData(LoadedObject, true /* bAllowBlueprintClass */));

			// Check to see if we have run out of time in this tick
			if (!bFlushFullBuffer &&
				(FPlatformTime::Seconds() - TickStartTime) > UE::AssetRegistry::Impl::MaxSecondsPerFrame)
			{
				bTimedOut = true;
				break;
			}
		}

		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		GuardedData.PushProcessLoadedAssetsBatch(EventContext, BatchAssetDatas,
			TArrayView<const UObject*>(BatchObjects).Slice(Index, CurrentBatchSize-Index));
		if (bTimedOut)
		{
			break;
		}
		GuardedData.GetProcessLoadedAssetsBatch(BatchObjects, BatchSize);
	}
}

namespace UE::AssetRegistry
{

void FAssetRegistryImpl::AddLoadedAssetToProcess(const UObject& AssetLoaded)
{
	LoadedAssetsToProcess.Add(&AssetLoaded);
}

void FAssetRegistryImpl::GetProcessLoadedAssetsBatch(TArray<const UObject*>& OutLoadedAssets, uint32 BatchSize)
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

		//@todo_ow: this will skip actors because after postload some actors might not have proper transform
		if (LoadedAsset->HasAnyFlags(RF_HasExternalPackage))
		{
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
			UpdateAssetData(EventContext, *DataFromGather, MoveTemp(NewAssetData));
		}
	}

	// Push back any objects from the batch that were not processed due to timing out
	for (int32 Index = UnprocessedFromBatch.Num() - 1; Index >= 0; --Index)
	{
		LoadedAssetsToProcess.EmplaceFront(UnprocessedFromBatch[Index]);
	}
}


void FAssetRegistryImpl::UpdateRedirectCollector()
{
	// Look for all redirectors in list
	const TArray<const FAssetData*>& RedirectorAssets = State.GetAssetsByClassPathName(UObjectRedirector::StaticClass()->GetClassPathName());

	for (const FAssetData* AssetData : RedirectorAssets)
	{
		FSoftObjectPath Destination = GetRedirectedObjectPath(AssetData->GetSoftObjectPath());

		if (Destination != AssetData->GetSoftObjectPath())
		{
			GRedirectCollector.AddAssetPathRedirection(AssetData->GetSoftObjectPath(), Destination);
		}
	}
}

}

#endif // WITH_EDITOR

void UAssetRegistryImpl::ScanModifiedAssetFiles(const TArray<FString>& InFilePaths)
{
	UE::AssetRegistry::Impl::FEventContext EventContext;
	{
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
		TArray<TArray<FAssetData*, TInlineAllocator<1>>> ExistingFilesAssetData;
		ExistingFilesAssetData.Reserve(InFilePaths.Num());
		for (const FString& PackageName : ModifiedPackageNames)
		{
			TArray<FAssetData*, TInlineAllocator<1>>* PackageAssetsPtr = State.CachedAssetsByPackageName.Find(*PackageName);
			if (PackageAssetsPtr && PackageAssetsPtr->Num() > 0)
			{
				ExistingFilesAssetData.Add(*PackageAssetsPtr);
			}
			else
			{
				ExistingFilesAssetData.AddDefaulted();
			}
		}

		// Re-scan and update the asset registry with the new asset data
		TArray<FSoftObjectPath> FoundAssets;
		Impl::FScanPathContext Context(EventContext, InheritanceContext, TArray<FString>(), InFilePaths,
			true /* bForceRescan */, false /* bIgnoreDenyListScanFilters */, &FoundAssets);
		ScanPathsSynchronous(Context);

		// Remove any assets that are no longer present in the package
		for (const TArray<FAssetData*, TInlineAllocator<1>>& OldPackageAssets : ExistingFilesAssetData)
		{
			for (FAssetData* OldPackageAsset : OldPackageAssets)
			{
				if (!FoundAssets.Contains(OldPackageAsset->GetSoftObjectPath()))
				{
					RemoveAssetData(EventContext, OldPackageAsset);
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
		DirectoryWatcher = DirectoryWatcherModule.Get();
		if (DirectoryWatcher)
		{
			// If the path doesn't exist on disk, make it so the watcher will work.
			IFileManager::Get().MakeDirectory(*FileSystemPath, /*Tree=*/true);
		}
	}
		
#endif

	UE::AssetRegistry::Impl::FEventContext EventContext;
	{
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		UE::AssetRegistry::Impl::FClassInheritanceContext InheritanceContext;
		UE::AssetRegistry::Impl::FClassInheritanceBuffer InheritanceBuffer;
		GetInheritanceContextWithRequiredLock(InterfaceScopeLock, InheritanceContext, InheritanceBuffer);
		GuardedData.OnContentPathMounted(EventContext, InheritanceContext, InAssetPath, AssetPathWithTrailingSlash,
			FileSystemPath);

		// Listen for directory changes in this content path
#if WITH_EDITOR
		// In-game doesn't listen for directory changes
		if (DirectoryWatcher)
		{
			if (!OnDirectoryChangedDelegateHandles.Contains(AssetPathWithTrailingSlash))
			{
				FDelegateHandle NewHandle;
				DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(
					FileSystemPath, 
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
	AddPath(EventContext, AssetPathWithTrailingSlash);

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
		AssetPathNoTrailingSlash.LeftChopInline(1, false);
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
		FWriteScopeLock InterfaceScopeLock(InterfaceLock);
		GuardedData.OnContentPathDismounted(EventContext, InAssetPath, AssetPathNoTrailingSlash, FileSystemPath);

		// Stop listening for directory changes in this content path
#if WITH_EDITOR
		if (DirectoryWatcher)
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
				DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(FileSystemPath, DirectoryChangedHandle);
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

	// Remove all cached assets found at this location
	{
		FName AssetPathNoTrailingSlashFName(*AssetPathNoTrailingSlash);
		TArray<FAssetData*> AllAssetDataToRemove;
		TSet<FName> PathList;
		const bool bRecurse = true;
		CachedPathTree.GetSubPaths(AssetPathNoTrailingSlashFName, PathList, bRecurse);
		PathList.Add(AssetPathNoTrailingSlashFName);
		for (FName PathName : PathList)
		{
			TArray<FAssetData*>* AssetsInPath = State.CachedAssetsByPath.Find(PathName);
			if (AssetsInPath)
			{
				AllAssetDataToRemove.Append(*AssetsInPath);
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

}

void UAssetRegistryImpl::SetTemporaryCachingMode(bool bEnable)
{
	checkf(IsInGameThread(), TEXT("Changing Caching mode is only available on the game thread because it affects behavior on all threads"));
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

	OutBuffer.RegisteredClassesVersionNumber = GetRegisteredClassesVersionNumber();
	OutBuffer.bDirty = false;
}

}

void UAssetRegistryImpl::GetInheritanceContextWithRequiredLock(FRWScopeLock& InOutScopeLock,
	UE::AssetRegistry::Impl::FClassInheritanceContext& InheritanceContext,
	UE::AssetRegistry::Impl::FClassInheritanceBuffer& StackBuffer)
{
	uint64 CurrentClassesVersionNumber = GetRegisteredClassesVersionNumber();
	bool bNeedsWriteLock = false;
	if (GuardedData.GetClassGeneratorNamesRegisteredClassesVersionNumber() != CurrentClassesVersionNumber)
	{
		// ConditionalUpdate writes to protected data in CollectCodeGeneratorClasses, so we cannot proceed under a read lock
		bNeedsWriteLock = true;
	}
	if (GuardedData.IsTempCachingEnabled() && !GuardedData.GetTempCachedInheritanceBuffer().IsUpToDate(CurrentClassesVersionNumber))
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
	GetInheritanceContextAfterVerifyingLock(CurrentClassesVersionNumber, InheritanceContext, StackBuffer);
}

void UAssetRegistryImpl::GetInheritanceContextWithRequiredLock(FWriteScopeLock& InOutScopeLock,
	UE::AssetRegistry::Impl::FClassInheritanceContext& InheritanceContext,
	UE::AssetRegistry::Impl::FClassInheritanceBuffer& StackBuffer)
{
	uint64 CurrentClassesVersionNumber = GetRegisteredClassesVersionNumber();
	GetInheritanceContextAfterVerifyingLock(CurrentClassesVersionNumber, InheritanceContext, StackBuffer);
}

void UAssetRegistryImpl::GetInheritanceContextAfterVerifyingLock(uint64 CurrentClassesVersionNumber,
	UE::AssetRegistry::Impl::FClassInheritanceContext& InheritanceContext,
	UE::AssetRegistry::Impl::FClassInheritanceBuffer& StackBuffer)
{
	// If bIsTempCachingAlwaysEnabled, then we are guaranteed that bIsTempCachingEnabled=true.
	// We rely on this to simplify logic and only check bIsTempCachingEnabled
	check(!GuardedData.IsTempCachingAlwaysEnabled() || GuardedData.IsTempCachingEnabled());

	bool bCodeGeneratorClassesUpToDate = GuardedData.GetClassGeneratorNamesRegisteredClassesVersionNumber() == CurrentClassesVersionNumber;
	if (GuardedData.IsTempCachingEnabled())
	{
		// Use the persistent buffer
		UE::AssetRegistry::Impl::FClassInheritanceBuffer& TempCachedInheritanceBuffer = GuardedData.GetTempCachedInheritanceBuffer();
		bool bInheritanceMapUpToDate = TempCachedInheritanceBuffer.IsUpToDate(CurrentClassesVersionNumber);
		InheritanceContext.BindToBuffer(TempCachedInheritanceBuffer, GuardedData, bInheritanceMapUpToDate, bCodeGeneratorClassesUpToDate);
	}
	else
	{
		// Use the StackBuffer for the duration of the caller
		InheritanceContext.BindToBuffer(StackBuffer, GuardedData, false /* bInInheritanceMapUpToDate */, bCodeGeneratorClassesUpToDate);
	}
}

#if WITH_EDITOR
void UAssetRegistryImpl::OnGetExtraObjectTags(const UObject* Object, TArray<UObject::FAssetRegistryTag>& OutTags)
{
	if (bAddMetaDataTagsToOnGetExtraObjectTags)
	{
		TSet<FName>& MetaDataTags = UObject::GetMetaDataTagsForAssetRegistry();
		// It is critical that bIncludeOnlyOnDiskAssets=true otherwise this will cause an infinite loop
		const FAssetData AssetData = GetAssetByObjectPath(FSoftObjectPath(Object), /*bIncludeOnlyOnDiskAssets=*/true);
		for (const FName MetaDataTag : MetaDataTags)
		{
			auto OutTagsContainsTagPredicate = [MetaDataTag](const UObject::FAssetRegistryTag& Tag) { return Tag.Name == MetaDataTag; };
			if (!OutTags.ContainsByPredicate(OutTagsContainsTagPredicate))
			{
				FAssetTagValueRef TagValue = AssetData.TagsAndValues.FindTag(MetaDataTag);
				if (TagValue.IsSet())
				{
					OutTags.Add(UObject::FAssetRegistryTag(MetaDataTag, TagValue.AsString(), UObject::FAssetRegistryTag::TT_Alphabetical));
				}
			}
		}
	}
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

bool FClassInheritanceBuffer::IsUpToDate(uint64 CurrentClassesVersionNumber) const
{
	return !bDirty && RegisteredClassesVersionNumber == CurrentClassesVersionNumber;
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
			UE_LOG(LogAssetRegistry, Error, TEXT("Cannot set %s to manage asset %s because %s does not exist!"), *Pair.Key.ToString(), *Pair.Value.ToString(), *Pair.Value.ToString());
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
	for (const TPair<FAssetIdentifier, FDependsNode*>& Pair : State.CachedDependsNodes)
	{
		FDependsNode* DependsNode = Pair.Value;
		DependsNode->SetIsDependencyListSorted(UE::AssetRegistry::EDependencyCategory::Manage, true);
		DependsNode->SetIsReferencersSorted(true);
	}

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
	UpdateAssetData(EventContext, AssetData, MoveTemp(NewAssetData));

	return true;
}

}

void FAssetRegistryDependencyOptions::SetFromFlags(const EAssetRegistryDependencyType::Type InFlags)
{
	bIncludeSoftPackageReferences = (InFlags & EAssetRegistryDependencyType::Soft);
	bIncludeHardPackageReferences = (InFlags & EAssetRegistryDependencyType::Hard);
	bIncludeSearchableNames = (InFlags & EAssetRegistryDependencyType::SearchableName);
	bIncludeSoftManagementReferences = (InFlags & EAssetRegistryDependencyType::SoftManage);
	bIncludeHardManagementReferences = (InFlags & EAssetRegistryDependencyType::HardManage);
}

EAssetRegistryDependencyType::Type FAssetRegistryDependencyOptions::GetAsFlags() const
{
	uint32 Flags = EAssetRegistryDependencyType::None;
	Flags |= (bIncludeSoftPackageReferences ? EAssetRegistryDependencyType::Soft : EAssetRegistryDependencyType::None);
	Flags |= (bIncludeHardPackageReferences ? EAssetRegistryDependencyType::Hard : EAssetRegistryDependencyType::None);
	Flags |= (bIncludeSearchableNames ? EAssetRegistryDependencyType::SearchableName : EAssetRegistryDependencyType::None);
	Flags |= (bIncludeSoftManagementReferences ? EAssetRegistryDependencyType::SoftManage : EAssetRegistryDependencyType::None);
	Flags |= (bIncludeHardManagementReferences ? EAssetRegistryDependencyType::HardManage : EAssetRegistryDependencyType::None);
	return (EAssetRegistryDependencyType::Type)Flags;
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
}

bool FEventContext::IsEmpty() const
{
	return !bFileLoadedEventBroadcast &&
		!ProgressUpdateData.IsSet() &&
		PathEvents.Num() == 0 &&
		AssetEvents.Num() == 0 &&
		RequiredLoads.Num() == 0;
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
	if (!IsInGameThread())
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

	if (EventContext.bFileLoadedEventBroadcast)
	{
		FileLoadedEvent.Broadcast();
		EventContext.bFileLoadedEventBroadcast = false;
	}

	if (EventContext.ProgressUpdateData.IsSet())
	{
		FileLoadProgressUpdatedEvent.Broadcast(*EventContext.ProgressUpdateData);
		EventContext.ProgressUpdateData.Reset();
	}

	if (EventContext.PathEvents.Num())
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
		EventContext.PathEvents.Empty();
	}

	if (EventContext.AssetEvents.Num())
	{
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
	if (EventContext.RequiredLoads.Num())
	{
		for (const FString& RequiredLoad : EventContext.RequiredLoads)
		{
			LoadPackage(nullptr, *RequiredLoad, 0);
		}
		EventContext.RequiredLoads.Empty();
	}
}


UAssetRegistryImpl::FPathAddedEvent& UAssetRegistryImpl::OnPathAdded()
{
	checkf(IsInGameThread(), TEXT("Registering to AssetRegistry events is not supported from multiple threads."));
	return PathAddedEvent;
}

UAssetRegistryImpl::FPathRemovedEvent& UAssetRegistryImpl::OnPathRemoved()
{
	checkf(IsInGameThread(), TEXT("Registering to AssetRegistry events is not supported from multiple threads."));
	return PathRemovedEvent;
}

UAssetRegistryImpl::FAssetAddedEvent& UAssetRegistryImpl::OnAssetAdded()
{
	checkf(IsInGameThread(), TEXT("Registering to AssetRegistry events is not supported from multiple threads."));
	return AssetAddedEvent;
}

UAssetRegistryImpl::FAssetRemovedEvent& UAssetRegistryImpl::OnAssetRemoved()
{
	checkf(IsInGameThread(), TEXT("Registering to AssetRegistry events is not supported from multiple threads."));
	return AssetRemovedEvent;
}

UAssetRegistryImpl::FAssetRenamedEvent& UAssetRegistryImpl::OnAssetRenamed()
{
	checkf(IsInGameThread(), TEXT("Registering to AssetRegistry events is not supported from multiple threads."));
	return AssetRenamedEvent;
}

UAssetRegistryImpl::FAssetUpdatedEvent& UAssetRegistryImpl::OnAssetUpdated()
{
	checkf(IsInGameThread(), TEXT("Registering to AssetRegistry events is not supported from multiple threads."));
	return AssetUpdatedEvent;
}

UAssetRegistryImpl::FAssetUpdatedEvent& UAssetRegistryImpl::OnAssetUpdatedOnDisk()
{
	checkf(IsInGameThread(), TEXT("Registering to AssetRegistry events is not supported from multiple threads."));
	return AssetUpdatedOnDiskEvent;
}

UAssetRegistryImpl::FInMemoryAssetCreatedEvent& UAssetRegistryImpl::OnInMemoryAssetCreated()
{
	checkf(IsInGameThread(), TEXT("Registering to AssetRegistry events is not supported from multiple threads."));
	return InMemoryAssetCreatedEvent;
}

UAssetRegistryImpl::FInMemoryAssetDeletedEvent& UAssetRegistryImpl::OnInMemoryAssetDeleted()
{
	checkf(IsInGameThread(), TEXT("Registering to AssetRegistry events is not supported from multiple threads."));
	return InMemoryAssetDeletedEvent;
}

UAssetRegistryImpl::FFilesLoadedEvent& UAssetRegistryImpl::OnFilesLoaded()
{
	checkf(IsInGameThread(), TEXT("Registering to AssetRegistry events is not supported from multiple threads."));
	return FileLoadedEvent;
}

UAssetRegistryImpl::FFileLoadProgressUpdatedEvent& UAssetRegistryImpl::OnFileLoadProgressUpdated()
{
	checkf(IsInGameThread(), TEXT("Registering to AssetRegistry events is not supported from multiple threads."));
	return FileLoadProgressUpdatedEvent;
}


namespace UE::AssetRegistry
{
const FAssetData* GetMostImportantAsset(TConstArrayView<const FAssetData*> PackageAssetDatas, bool bRequireOneTopLevelAsset)
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
		if (FFiltering::ShouldSkipAsset(Asset->AssetClassPath, Asset->PackageFlags))
		{
			continue;
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

	if (bRequireOneTopLevelAsset)
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
			OutPackageToAssetData.FindOrAdd(CurrentPackageName) = *GetMostImportantAsset(PackageAssetDatas, false);
			PackageAssetDatas.Reset();
			CurrentPackageName = AssetData.PackageName;
		}

		PackageAssetDatas.Push(&AssetData);
	}

	OutPackageToAssetData.FindOrAdd(CurrentPackageName) = *GetMostImportantAsset(PackageAssetDatas, false);

}

} // namespace AssetRegistry
