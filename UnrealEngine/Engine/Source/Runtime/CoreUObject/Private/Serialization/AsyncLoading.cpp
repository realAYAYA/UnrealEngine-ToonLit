// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnAsyncLoading.cpp: Unreal async loading code.
=============================================================================*/

#include "Serialization/AsyncLoading.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "HAL/Event.h"
#include "HAL/RunnableThread.h"
#include "Misc/PackageSegment.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"
#include "Stats/StatsMisc.h"
#include "Misc/CoreStats.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageAccessTrackingOps.h"
#include "Misc/PackageName.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/TrackedActivity.h"
#include "UObject/PackageFileSummary.h"
#include "UObject/Linker.h"
#include "UObject/CoreRedirects.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/LinkerLoad.h"
#include "UObject/LinkerLoadImportBehavior.h"
#include "UObject/PackageResourceManager.h"
#include "Serialization/DeferredMessageLog.h"
#include "UObject/UObjectThreadContext.h"
#include "UObject/ObjectSerializeAccessScope.h"
#include "UObject/LinkerManager.h"
#include "Misc/Paths.h"
#include "Misc/PlayInEditorLoadingScope.h"
#include "Serialization/AsyncLoadingThread.h"
#include "Misc/ExclusiveLoadPackageTimeTracker.h"
#include "ProfilingDebugging/CookStats.h"
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "HAL/ThreadHeartBeat.h"
#include "HAL/ExceptionHandling.h"
#include "Serialization/AsyncLoadingPrivate.h"
#include "UObject/UObjectHash.h"
#include "Templates/UniquePtr.h"
#include "Serialization/BufferReader.h"
#include "Async/TaskGraphInterfaces.h"
#include "Blueprint/BlueprintSupport.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/LowLevelMemStats.h"
#include "HAL/IPlatformFileOpenLogWrapper.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "ProfilingDebugging/AssetMetadataTrace.h"
#include "UObject/GarbageCollectionInternal.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "Serialization/LoadTimeTracePrivate.h"
#include "Misc/PathViews.h"

#define FIND_MEMORY_STOMPS (1 && (PLATFORM_WINDOWS || PLATFORM_UNIX) && !WITH_EDITORONLY_DATA)

DEFINE_LOG_CATEGORY(LogLoadingDev);

CSV_DECLARE_CATEGORY_MODULE_EXTERN(CORE_API, Basic);
CSV_DECLARE_CATEGORY_MODULE_EXTERN(CORE_API, FileIO);

//#pragma clang optimize off

/*-----------------------------------------------------------------------------
	Async loading stats.
-----------------------------------------------------------------------------*/

DECLARE_MEMORY_STAT(TEXT("Streaming Memory Used"),STAT_StreamingAllocSize,STATGROUP_Memory);

DECLARE_CYCLE_STAT(TEXT("Tick AsyncPackage"),STAT_FAsyncPackage_Tick,STATGROUP_AsyncLoad);

DECLARE_CYCLE_STAT(TEXT("CreateLinker AsyncPackage"),STAT_FAsyncPackage_CreateLinker,STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("FinishLinker AsyncPackage"),STAT_FAsyncPackage_FinishLinker,STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("LoadImports AsyncPackage"),STAT_FAsyncPackage_LoadImports,STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("CreateImports AsyncPackage"),STAT_FAsyncPackage_CreateImports,STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("CreateMetaData AsyncPackage"),STAT_FAsyncPackage_CreateMetaData,STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("CreateExports AsyncPackage"),STAT_FAsyncPackage_CreateExports,STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("FreeReferencedImports AsyncPackage"), STAT_FAsyncPackage_FreeReferencedImports, STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("Precache AsyncArchive"), STAT_FAsyncArchive_Precache, STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("PreLoadObjects AsyncPackage"),STAT_FAsyncPackage_PreLoadObjects,STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("ExternalReadDependencies AsyncPackage"),STAT_FAsyncPackage_ExternalReadDependencies,STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("PostLoadObjects AsyncPackage"),STAT_FAsyncPackage_PostLoadObjects,STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("FinishObjects AsyncPackage"),STAT_FAsyncPackage_FinishObjects,STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("CreateAsyncPackagesFromQueue"), STAT_FAsyncPackage_CreateAsyncPackagesFromQueue, STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("ProcessAsyncLoading AsyncLoadingThread"), STAT_FAsyncLoadingThread_ProcessAsyncLoading, STATGROUP_AsyncLoad);
DECLARE_CYCLE_STAT(TEXT("Async Loading Time Detailed"), STAT_AsyncLoadingTimeDetailed, STATGROUP_AsyncLoad);

DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Total PostLoadObjects time GT"), STAT_FAsyncPackage_TotalPostLoadGameThread, STATGROUP_AsyncLoadGameThread);

DECLARE_FLOAT_ACCUMULATOR_STAT( TEXT( "Async loading block time" ), STAT_AsyncIO_AsyncLoadingBlockingTime, STATGROUP_AsyncIO );
DECLARE_FLOAT_ACCUMULATOR_STAT( TEXT( "Async package precache wait time" ), STAT_AsyncIO_AsyncPackagePrecacheWaitTime, STATGROUP_AsyncIO );

struct FEDLBootObjectState
{
	ENotifyRegistrationType NotifyRegistrationType;
	ENotifyRegistrationPhase LastNotifyRegistrationPhase;
	UObject* (*Register)();
	bool bDynamic;
};

struct FEDLBootWaitingPackage
{
	void* Package;
	FPackageIndex Import;
};

struct FEDLBootNotificationManager
{
	TMap<FName, FEDLBootObjectState> PathToState;
	TMultiMap<FName, FEDLBootWaitingPackage> PathToWaitingPackageNodes;
	TArray<FName> PathsToFire;
	TArray<UClass*> CDORecursiveStack;
	TArray<UClass*> CDORecursives;
	FCriticalSection EDLBootNotificationManagerLock;
	bool bEnabled = true;

	void Disable()
	{
		PathToState.Empty();
		PathsToFire.Empty();
		bEnabled = false;
	}

	// return true if you are waiting for this compiled in object
	bool AddWaitingPackage(void* Pkg, FName PackageName, FName ObjectName, FPackageIndex Import, bool bIgnoreMissingPackage)
	{
		if (PackageName == GLongCoreUObjectPackageName)
		{
			return false; // We assume nothing in coreuobject ever loads assets in a constructor
		}
		FScopeLock Lock(&EDLBootNotificationManagerLock);
		check(GIsInitialLoad);
		check(Import.IsImport()); // compiled in exports make no sense
		FString ObjectNameString = ObjectName.ToString();
		FName LongFName(*(PackageName.ToString() / ObjectNameString));
		check(LongFName != NAME_None);
		FName WaitName = LongFName;
		FEDLBootObjectState* ExistingState = PathToState.Find(LongFName);
		if (!ExistingState)
		{
			//if (ObjectName.ToString().EndsWith(HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX))
			// there are also some arg structs and other things that are just part of the package with no registration
			{
				ExistingState = PathToState.Find(PackageName);
				WaitName = PackageName;
			}
			if (!ExistingState)
			{
				UE_CLOG(!bIgnoreMissingPackage, LogStreaming, Fatal, TEXT("Compiled in export %s not found; it was never registered."), *LongFName.ToString());
				return false;
			}
		}
		if (ExistingState->LastNotifyRegistrationPhase == ENotifyRegistrationPhase::NRP_Finished)
		{
			return false;
		}
		FEDLBootWaitingPackage WaitingPackage;
		WaitingPackage.Package = Pkg;
		WaitingPackage.Import = Import;

		PathToWaitingPackageNodes.Add(WaitName, WaitingPackage);

		return true;
	}

	void NotifyRegistrationEvent(const TCHAR* PackageName, const TCHAR* Name, ENotifyRegistrationType NotifyRegistrationType, ENotifyRegistrationPhase NotifyRegistrationPhase, UObject* (*InRegister)(), bool InbDynamic)
	{
		if (!bEnabled || !GIsInitialLoad)
		{
			return;
		}
		static FName LongCoreUObjectPackageName(TEXT("/Script/CoreUObject")); // can't use the global here because it may not be initialized yet
		FName PackageFName(PackageName);
		if (PackageFName == LongCoreUObjectPackageName)
		{
			return; // We assume nothing in coreuobject ever loads assets in a constructor
		}

		if (NotifyRegistrationType == ENotifyRegistrationType::NRT_NoExportObject)
		{
			return; // We don't care about these, they will always be found using StaticFindObject
		}

		TStringBuilder<256> LongNameBuilder;
		LongNameBuilder << PackageName;
		FPathViews::Append(LongNameBuilder, Name);
		FName LongFName(LongNameBuilder.ToView());

		FScopeLock Lock(&EDLBootNotificationManagerLock);

		//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("NotifyRegistrationEvent %s %d %d\r\n"), *LongFName.ToString(), int32(NotifyRegistrationType), int32(NotifyRegistrationPhase));

		// some things, like delegate signatures, are not registered; rather they are part of the package singleton, so we track the package state as being the max of any member of that package
		FEDLBootObjectState* ExistingPackageState = PathToState.Find(PackageFName);
		FEDLBootObjectState* ExistingState = PathToState.Find(LongFName);

		if (!ExistingState)
		{
			FEDLBootObjectState NewState;
			NewState.LastNotifyRegistrationPhase = NotifyRegistrationPhase;
			NewState.NotifyRegistrationType = NotifyRegistrationType;
			NewState.Register = InRegister;
			NewState.bDynamic = InbDynamic;
			PathToState.Add(LongFName, NewState);

			if (!ExistingPackageState)
			{
				NewState.NotifyRegistrationType = ENotifyRegistrationType::NRT_Package;
				PathToState.Add(PackageFName, NewState);
			}
		}
		else
		{
			if (int32(ExistingState->LastNotifyRegistrationPhase) >= int32(NotifyRegistrationPhase))
			{
				UE_CLOG(GEventDrivenLoaderEnabled, LogStreaming, Fatal, TEXT("Invalid state transition %d %d with %s when it has already been processed."), int32(ExistingState->LastNotifyRegistrationPhase), int32(NotifyRegistrationPhase), *LongFName.ToString());
			}
			if (ExistingState->NotifyRegistrationType != (NotifyRegistrationType))
			{
				UE_CLOG(GEventDrivenLoaderEnabled, LogStreaming, Fatal, TEXT("Multiple types %d %d with %s when it has already been processed."), int32(ExistingState->NotifyRegistrationType), int32(NotifyRegistrationType), *LongFName.ToString());
			}
			ExistingState->LastNotifyRegistrationPhase = NotifyRegistrationPhase;
			if (NotifyRegistrationPhase == ENotifyRegistrationPhase::NRP_Finished)
			{
				ExistingState->Register = nullptr; // we don't need to do this in ConstructWaitingBootObjects
				PathsToFire.Add(LongFName);
			}
			check(ExistingPackageState); // if we have an existing state for the thing, we should also have a 
			if (ExistingPackageState && int32(NotifyRegistrationPhase) > int32(ExistingPackageState->LastNotifyRegistrationPhase))
			{
				ExistingPackageState->LastNotifyRegistrationPhase = NotifyRegistrationPhase;
				if (NotifyRegistrationPhase == ENotifyRegistrationPhase::NRP_Finished)
				{
					//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Fired package %s %d %d\r\n"), *PackageFName.ToString(), int32(NotifyRegistrationType), int32(NotifyRegistrationPhase));
					PathsToFire.Add(PackageFName);
				}
			}
		}
	}

	void NotifyRegistrationComplete(FAsyncLoadingThread& AsyncLoadingThread)
	{
		if (!bEnabled)
		{
			return;
		}
#if USE_EVENT_DRIVEN_ASYNC_LOAD_AT_BOOT_TIME
		FireCompletedCompiledInImports(AsyncLoadingThread, true);
		FlushAsyncLoading();
#endif
		check(!GIsInitialLoad && IsInGameThread());
		FScopeLock Lock(&EDLBootNotificationManagerLock);
		for (auto& Pair : PathToState)
		{
			if (Pair.Value.LastNotifyRegistrationPhase != ENotifyRegistrationPhase::NRP_Finished && !Pair.Value.bDynamic)
			{
#if USE_EVENT_DRIVEN_ASYNC_LOAD_AT_BOOT_TIME
				UE_CLOG(GEventDrivenLoaderEnabled, LogStreaming, Fatal, TEXT("%s (%d) was not complete (%d) after registration was complete."), *Pair.Key.ToString(), int32(Pair.Value.NotifyRegistrationType), int32(Pair.Value.LastNotifyRegistrationPhase));
#else
				UE_LOG(LogStreaming, Warning, TEXT("%s was not complete (%d) after registration was complete."), *Pair.Key.ToString(), int32(Pair.Value.LastNotifyRegistrationPhase));
#endif
			}
		}
		if (PathToWaitingPackageNodes.Num())
		{
			UE_LOG(LogStreaming, Fatal, TEXT("Initial load is complete, but we still have %d waiting packages."), PathToWaitingPackageNodes.Num());
		}
		if (GEventDrivenLoaderEnabled && PathsToFire.Num() && USE_EVENT_DRIVEN_ASYNC_LOAD_AT_BOOT_TIME)
		{
			for (FName Path : PathsToFire)
			{
				UE_LOG(LogStreaming, Error, TEXT("%s was not fired."), *Path.ToString());
			}
			UE_LOG(LogStreaming, Fatal, TEXT("Initial load is complete, but we still have %d imports to fire (listed above)."), PathsToFire.Num());
		}
		Disable();
	}

	bool ConstructWaitingBootObjects()
	{
		static struct FFixedBootOrder
		{
			TArray<FName> Array;
			FFixedBootOrder()
			{
				// look for any packages that we want to force preload at startup
				const FConfigSection* BootObjects = GConfig->GetSection(TEXT("/Script/Engine.StreamingSettings"), false, GEngineIni);
				if (BootObjects)
				{
					// go through list and add to the array
					for (FConfigSectionMap::TConstIterator It(*BootObjects); It; ++It)
					{
						if (It.Key() == TEXT("FixedBootOrder"))
						{
							// add this package to the list to be fully loaded later
							Array.Add(FName(*It.Value().GetValue()));
						}
					}
				}
			}

		} FixedBootOrder;

		check(GIsInitialLoad && IsInGameThread());
		UObject* (*BootObjectRegister)() = nullptr;
		UObject* (*BootPackageObjectRegister)() = nullptr;
		FName WaitingPackage;
		bool bIsCDO = false;

		while (FixedBootOrder.Array.Num())
		{
			FName ThisItem = FixedBootOrder.Array.Pop();
			FScopeLock Lock(&EDLBootNotificationManagerLock);
			FEDLBootObjectState* ExistingState = PathToState.Find(ThisItem);

			if (!ExistingState)
			{
				UE_LOG(LogStreaming, Fatal, TEXT("%s was listed as a fixed load order but was not found,"), *ThisItem.ToString());
			}
			else if (!ExistingState->Register)
			{
				UE_LOG(LogStreaming, Log, TEXT("%s was listed as a fixed load order but was already processed"), *ThisItem.ToString());
			}
			else
			{
				//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Booting Fixed %s %d\r\n"), *ThisItem.ToString(), int32(ExistingState->NotifyRegistrationType));
				BootObjectRegister = ExistingState->Register;
				ExistingState->Register = nullptr; // we don't need to do this more than once
				bIsCDO = ExistingState->NotifyRegistrationType == ENotifyRegistrationType::NRT_ClassCDO;
				break;
			}
		}

		if (!BootObjectRegister)
		{
			FScopeLock Lock(&EDLBootNotificationManagerLock);
			for (auto& Pair : PathToWaitingPackageNodes)
			{
				FEDLBootObjectState* ExistingState = PathToState.Find(Pair.Key);
				if (ExistingState)
				{
					if (ExistingState->Register)
					{
						//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Booting %s %d\r\n"), *Pair.Key.ToString(), int32(ExistingState->NotifyRegistrationType));
						BootObjectRegister = ExistingState->Register;
						ExistingState->Register = nullptr; // we don't need to do this more than once
						bIsCDO = ExistingState->NotifyRegistrationType == ENotifyRegistrationType::NRT_ClassCDO;
						break;
					}
				}
			}
		}
		if (BootObjectRegister)
		{
			UObject* BootObject = BootObjectRegister();
			check(BootObject);
			UObjectForceRegistration(BootObject);
			if (bIsCDO)
			{
				UClass* Class = CastChecked<UClass>(BootObject);
				bool bAnyParentOnStack = false;
				UClass* Super = Class;
				while (Super)
				{
					if (CDORecursiveStack.Contains(Super))
					{
						bAnyParentOnStack = true;
						break;
					}
					Super = Super->GetSuperClass();
				}

				if (!bAnyParentOnStack)
				{
					CDORecursiveStack.Push(Class);
					//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Create CDO %s\r\n"), *BootObject->GetName());
					Class->GetDefaultObject();
					verify(CDORecursiveStack.Pop() == Class);
				}
				else
				{
					//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Recursive Deferred %s\r\n"), *BootObject->GetName());
					CDORecursives.Add(Class);
				}
			}
			return true;
		}
		if (CDORecursives.Num())
		{
			UClass* OkToRun = nullptr;
			for (UClass* Class : CDORecursives)
			{
				bool bAnyParentOnStack = false;
				UClass* Super = Class;
				while (Super)
				{
					if (CDORecursiveStack.Contains(Super))
					{
						bAnyParentOnStack = true;
						break;
					}
					Super = Super->GetSuperClass();
				}
				if (!bAnyParentOnStack)
				{
					OkToRun = Class;
					break;
				}
			}
			if (OkToRun)
			{
				//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("CDORecursives %s\r\n"), *OkToRun->GetName());
				CDORecursives.Remove(OkToRun);
				CDORecursiveStack.Push(OkToRun);
				OkToRun->GetDefaultObject();
				verify(CDORecursiveStack.Pop() == OkToRun);
			}
			else
			{
				FPlatformProcess::Sleep(.001f);
			}
			return true; // even if we didn't do anything we need to return true to avoid checking for cycles
		}
		return false;
	}

	bool IsWaitingForSomething()
	{
		FScopeLock Lock(&EDLBootNotificationManagerLock);
		return PathToWaitingPackageNodes.Num() > 0;
	}

	bool IsObjComplete(UObject* Obj)
	{
		static FName LongCoreUObjectPackageName(TEXT("/Script/CoreUObject")); // can't use the global here because it may not be initialized yet
		FName PackageName = Obj->GetOutermost()->GetFName();
		if (PackageName == LongCoreUObjectPackageName)
		{
			return true; // We assume nothing in coreuobject ever loads assets in a constructor, therefore it can be considered complete
		}
		FScopeLock Lock(&EDLBootNotificationManagerLock);
		FName LongFName(*(PackageName.ToString() / Obj->GetName()));

		FEDLBootObjectState* ExistingState = PathToState.Find(LongFName);

		if (!ExistingState || ExistingState->LastNotifyRegistrationPhase == ENotifyRegistrationPhase::NRP_Finished)
		{
			return true;
		}
		return false;
	}

	bool FireCompletedCompiledInImports(FAsyncLoadingThread& AsyncLoadingThread, bool bFinalRun = false)
	{
#if USE_EVENT_DRIVEN_ASYNC_LOAD_AT_BOOT_TIME
		FScopeLock Lock(&EDLBootNotificationManagerLock);
		check(bFinalRun || GIsInitialLoad);
		bool bResult = !!PathsToFire.Num();
		for (FName LongName : PathsToFire)
		{
			for (auto It = PathToWaitingPackageNodes.CreateKeyIterator(LongName); It; ++It)
			{
				FEDLBootWaitingPackage& WaitingPackage = It.Value();
				AsyncLoadingThread.FireCompletedCompiledInImport(WaitingPackage.Package, WaitingPackage.Import);
			}
			PathToWaitingPackageNodes.Remove(LongName);
		}
		PathsToFire.Empty();
		return bResult;
#else
		return false;
#endif
	}
};

static FEDLBootNotificationManager& GetGEDLBootNotificationManager()
{
	static FEDLBootNotificationManager Singleton;
	return Singleton;
}

/** Helper function for profiling load times */
static FName StaticGetNativeClassName(UClass* InClass)
{
	while(InClass && !InClass->HasAnyClassFlags(CLASS_Native))
	{
		InClass = InClass->GetSuperClass();
	}

	return InClass ? InClass->GetFName() : NAME_None;
}

/** Returns true if we're inside a FGCScopeLock */
bool IsGarbageCollectionLocked();

#if UE_ENABLE_TRACKED_IO
struct FActiveAsyncLoadContext* ActiveAsyncLoadContext;

struct FActiveAsyncLoadContext
{
	uint32 TotalCompleted = 0;

	FActiveAsyncLoadContext(const TArray<FAsyncPackageDesc*>& Queue)
	{
		TStringBuilder<256> Str;
		if (Queue.Num() > 0)
		{
			Queue[0]->Name.AppendString(Str);
			if (Str.GetData()[0] == '/')
			{
				Str.RemoveAt(0, 1);
			}
		}
		FTrackedActivity::GetIOActivity().Push(*Str);
		FTrackedActivity::GetIOActivity().Push(TEXT(""), true);
		ActiveAsyncLoadContext = this;
	}

	~FActiveAsyncLoadContext()
	{
		ActiveAsyncLoadContext = nullptr;
		FTrackedActivity::GetIOActivity().Pop();
		FTrackedActivity::GetIOActivity().Pop();
	}

	void Update(uint32 Queued)
	{
		FTrackedActivity::GetIOActivity().Update(*TStringBuilder<256>().Appendf(TEXT(" (%u/%u)"), TotalCompleted, TotalCompleted + Queued));
	}
};
#endif // UE_ENABLE_TRACKED_IO

/**
 * Updates FUObjectThreadContext with the current package when processing it.
 * FUObjectThreadContext::AsyncPackage is used by NotifyConstructedDuringAsyncLoading.
 */
struct FAsyncPackageScope
{
	/** Outer scope package */
	void* PreviousPackage;
	IAsyncPackageLoader* PreviousAsyncPackageLoader;
	/** Cached ThreadContext so we don't have to access it again */
	FUObjectThreadContext& ThreadContext;

	FAsyncPackageScope(FAsyncPackage* InPackage)
		: ThreadContext(FUObjectThreadContext::Get())
	{
		PreviousPackage = ThreadContext.AsyncPackage;
		ThreadContext.AsyncPackage = InPackage;
		PreviousAsyncPackageLoader = ThreadContext.AsyncPackageLoader;
		ThreadContext.AsyncPackageLoader = &InPackage->AsyncLoadingThread;
	}
	~FAsyncPackageScope()
	{
		ThreadContext.AsyncPackage = PreviousPackage;
		ThreadContext.AsyncPackageLoader = PreviousAsyncPackageLoader;
	}
};

static int32 GAsyncLoadingThreadEnabled = 0;
static FAutoConsoleVariableRef CVarAsyncLoadingThreadEnabled(
	TEXT("s.AsyncLoadingThreadEnabled"),
	GAsyncLoadingThreadEnabled,
	TEXT("Placeholder console variable, currently not used in runtime."),
	ECVF_Default
	);

static int32 GAsyncLoadingAlwaysProcessPackages = 0;
static FAutoConsoleVariableRef CVarAsyncLoadingAlwaysProcessPackages(
	TEXT("s.AsyncLoadingAlwaysProcessPackages"),
	GAsyncLoadingAlwaysProcessPackages,
	TEXT("When flushing, will process all packages instead of only what's needed. (Used to avoid a hard to repro potential deadlock)"),
	ECVF_Default
	);

static int32 GFlushStreamingOnExit = 1;
static FAutoConsoleVariableRef CFlushStreamingOnExit(
	TEXT("s.FlushStreamingOnExit"),
	GFlushStreamingOnExit,
	TEXT("Placeholder console variable, currently not used in runtime."),
	ECVF_Default
);

static int32 GEventDrivenLoaderEnabledInCookedBuilds = 0;
static FAutoConsoleVariableRef CVarEventDrivenLoaderEnabled(
	TEXT("s.EventDrivenLoaderEnabled"),
	GEventDrivenLoaderEnabledInCookedBuilds,
	TEXT("Placeholder console variable, currently not used in runtime."),
	ECVF_Default
);

int32 GMaxReadyRequestsToStallMB = 30;
static FAutoConsoleVariableRef CVar_MaxReadyRequestsToStallMB(
	TEXT("s.MaxReadyRequestsToStallMB"),
	GMaxReadyRequestsToStallMB,
	TEXT("Controls the maximum amount memory for unhandled IO requests before we stall the pak precacher to let the CPU catch up (in megabytes).")
);

int32 GMaxPrecacheRequestsInFlight = 2;
static FAutoConsoleVariableRef CVar_MaxPrecacheRequestsInFlight(
	TEXT("s.MaxPrecacheRequestsInFlight"),
	GMaxPrecacheRequestsInFlight,
	TEXT("Controls the maximum amount of precache requests to have in flight.")
);

int32 GMaxIncomingRequestsToStall = 100;
static FAutoConsoleVariableRef CVar_MaxIncomingRequestsToStall(
	TEXT("s.MaxIncomingRequestsToStall"),
	GMaxIncomingRequestsToStall,
	TEXT("Controls the maximum number of unhandled IO requests before we stall the pak precacher to let the CPU catch up.")
);

int32 GProcessPrestreamingRequests = 0;
static FAutoConsoleVariableRef CVar_ProcessPrestreamingRequests(
	TEXT("s.ProcessPrestreamingRequests"),
	GProcessPrestreamingRequests,
	TEXT("If non-zero, then we process prestreaming requests in cooked builds.")
	);

int32 GEditorLoadPrecacheSizeKB = 0;
static FAutoConsoleVariableRef CVar_EditorLoadPrecacheSizeKB(
	TEXT("s.EditorLoadPrecacheSizeKB"),
	GEditorLoadPrecacheSizeKB,
	TEXT("Size, in KB, to precache when loading packages in the editor.")
);

int32 GAsyncLoadingPrecachePriority = (int32)AIOP_MIN;
static FAutoConsoleVariableRef CVarAsyncLoadingPrecachePriority(
	TEXT("s.AsyncLoadingPrecachePriority"),
	GAsyncLoadingPrecachePriority,
	TEXT("Priority of asyncloading precache requests"),
	ECVF_Default
);

EAsyncIOPriorityAndFlags GetAsyncIOPriority()
{
	check(GAsyncLoadingPrecachePriority >= AIOP_MIN && GAsyncLoadingPrecachePriority <= AIOP_MAX);
	EAsyncIOPriorityAndFlags Priority = (EAsyncIOPriorityAndFlags)FMath::Clamp(GAsyncLoadingPrecachePriority, (int32)AIOP_MIN, (int32)AIOP_MAX);
	return Priority;
}

EAsyncIOPriorityAndFlags GetAsyncIOPrecachePriorityAndFlags()
{
	return GetAsyncIOPriority() | AIOP_FLAG_PRECACHE;
}



#if !UE_BUILD_SHIPPING

static void NotifyAsyncLoadingStateHasMaybeChanged()
{
	static bool bEnabled = FParse::Param(FCommandLine::Get(), TEXT("TrackBootLoading"));
	if (!bEnabled)
	{
		return;
	}
	static FCriticalSection Crit;
	FScopeLock Lock(&Crit);

	static bool bLastState = false;
	bool bState = IsAsyncLoading();
	if (bState != bLastState)
	{
		NotifyLoadingStateChanged(bState, TEXT("Async UObject"));
		bLastState = bState;
	}
}
#else

static void NotifyAsyncLoadingStateHasMaybeChanged()
{
}

#endif

uint32 FAsyncLoadingThread::AsyncLoadingThreadID = 0;
FThreadSafeCounter FAsyncLoadingThread::AsyncLoadingTickCounter;
int32 FAsyncLoadingThread::CurrentAsyncLoadingTickThreadIndex = INDEX_NONE;

static FORCEINLINE bool IsTimeLimitExceeded(double InTickStartTime, bool bUseTimeLimit, double InTimeLimit, const TCHAR* InLastTypeOfWorkPerformed = nullptr, UObject* InLastObjectWorkWasPerformedOn = nullptr)
{
	static double LastTestTime = -1.0;
	bool bTimeLimitExceeded = false;
	if (bUseTimeLimit)
	{
		double CurrentTime = FPlatformTime::Seconds();
		bTimeLimitExceeded = CurrentTime - InTickStartTime > InTimeLimit;

		if (bTimeLimitExceeded && GWarnIfTimeLimitExceeded)
		{
			IsTimeLimitExceededPrint(InTickStartTime, CurrentTime, LastTestTime, InTimeLimit, InLastTypeOfWorkPerformed, InLastObjectWorkWasPerformedOn);
		}
		LastTestTime = CurrentTime;
	}
	if (!bTimeLimitExceeded)
	{
		bTimeLimitExceeded = IsGarbageCollectionWaiting();
		UE_CLOG(bTimeLimitExceeded, LogStreaming, Verbose, TEXT("Timing out async loading due to Garbage Collection request"));
	}
	return bTimeLimitExceeded;
}
FORCEINLINE bool FAsyncPackage::IsTimeLimitExceeded()
{
	return AsyncLoadingThread.IsAsyncLoadingSuspendedInternal() || ::IsTimeLimitExceeded(TickStartTime, bUseTimeLimit, TimeLimit, LastTypeOfWorkPerformed, LastObjectWorkWasPerformedOn);
}

bool FAsyncPackage::IsFullyLoadedObj(UObject* Obj)
{
	if (!Obj)
	{
		return false;
	}
	if (Obj->HasAllFlags(RF_WasLoaded | RF_LoadCompleted)
		|| Obj->IsA(UPackage::StaticClass())) // packages are never really loaded, so if it exists, it is loaded
	{
		return true;
	}
	if (Obj->HasAnyFlags(RF_WasLoaded | RF_NeedLoad | RF_WillBeLoaded))
	{
		return false;
	}
	if (GIsInitialLoad && Obj->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn))
	{
		return EDLBootNotificationManager.IsObjComplete(Obj);
	}

	return true;
}

FORCENOINLINE static bool CheckForFilePackageOpenLogCommandLine()
{
	return FParse::Param(FCommandLine::Get(), TEXT("FilePackageOpenLog"));
}

DEFINE_LOG_CATEGORY_STATIC(LogAsyncArchive, Display, All);
DECLARE_MEMORY_STAT(TEXT("FAsyncArchive Buffers"), STAT_FAsyncArchiveMem, STATGROUP_Memory);

//#define ASYNC_WATCH_FILE "SM_Boots_Wings.uasset"
#define TRACK_SERIALIZE (0)
#define MIN_REMAIN_TIME (0.00101f)   // wait(0) is very different than wait(tiny) so we cut things off well before roundoff error could cause us to block when we didn't intend to. Also the granularity of the event API is 1ms.




FORCEINLINE void FAsyncArchive::LogItem(const TCHAR* Item, int64 Offset, int64 Size, double StartTime)
{
	if (UE_LOG_ACTIVE(LogAsyncArchive, Verbose)
#if defined(ASYNC_WATCH_FILE)
		|| PackagePath.GetLocalFullPath().Contains(TEXT(ASYNC_WATCH_FILE))
#endif
		)
	{
		FString FileName = PackagePath.GetLocalFullPath();

		static double GlobalStartTime(FPlatformTime::Seconds());
		double Now(FPlatformTime::Seconds());

		float ThisTime = (StartTime != 0.0) ? float(1000.0 * (Now - StartTime)) : 0.0f;

		if (!UE_LOG_ACTIVE(LogAsyncArchive, VeryVerbose) && ThisTime < 1.0f
#if defined(ASYNC_WATCH_FILE)
			&& !FileName.Contains(TEXT(ASYNC_WATCH_FILE))
#endif
			)
		{
			return;
		}

		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%32s%3s    %12lld %12lld    %6.2fms    (+%9.2fms)      %s\r\n"),
			Item,
			(ThisTime > 1.0f) ? TEXT("***") : TEXT(""),
			Offset,
			((Size == MAX_int64) ? TotalSize() : Offset + Size),
			ThisTime,
			float(1000.0 * (Now - GlobalStartTime)),
			*FileName);
	}
}


#if LOOKING_FOR_PERF_ISSUES
FThreadSafeCounter FAsyncLoadingThread::BlockingCycles = 0;
#endif

FAsyncLoadingThread* FAsyncLoadingThread::Instance = nullptr;

/** Just like TGuardValue for FAsyncLoadingThread::AsyncLoadingTickCounter but only works for the game thread */
struct FAsyncLoadingTickScope
{
	bool bNeedsToLeaveAsyncTick;
	FAsyncLoadingThread& AsyncLoadingThread;

	FAsyncLoadingTickScope(FAsyncLoadingThread& InAsyncLoadingThread)
		: bNeedsToLeaveAsyncTick(false)
		, AsyncLoadingThread(InAsyncLoadingThread)
	{
		if (IsInGameThread())
		{
			FAsyncLoadingThread::EnterAsyncLoadingTick(AsyncLoadingThread.GetThreadIndex());
			bNeedsToLeaveAsyncTick = true;
		}
	}
	~FAsyncLoadingTickScope()
	{
		if (bNeedsToLeaveAsyncTick)
		{
			FAsyncLoadingThread::LeaveAsyncLoadingTick(AsyncLoadingThread.GetThreadIndex());
		}
	}
};

void FAsyncLoadingThread::InitializeLoading()
{
	AsyncThreadReady.Increment();
}

void FAsyncLoadingThread::QueuePackage(FAsyncPackageDesc& Package)
{
	{
#if THREADSAFE_UOBJECTS
		FScopeLock QueueLock(&QueueCritical);
#endif
#if !UE_BUILD_SHIPPING
		if(CheckForFilePackageOpenLogCommandLine())
		{
			FPlatformFileOpenLog* PlatformFileOpenLog = (FPlatformFileOpenLog*)(FPlatformFileManager::Get().FindPlatformFile(FPlatformFileOpenLog::GetTypeName()));
			if (PlatformFileOpenLog != nullptr)
			{
				PlatformFileOpenLog->AddPackageToOpenLog(*Package.Name.ToString());
			}
		}
#endif
		QueuedPackagesCounter.Increment();
		QueuedPackages.Add(new FAsyncPackageDesc(Package, MoveTemp(Package.PackageLoadedDelegate)));
	}
	NotifyAsyncLoadingStateHasMaybeChanged();

	QueuedRequestsEvent->Trigger();
}

FUObjectSerializeContext* FAsyncPackage::GetSerializeContext()
{
	return FUObjectThreadContext::Get().GetSerializeContext();
}

FAsyncPackage* FAsyncLoadingThread::FindExistingPackageAndAddCompletionCallback(FAsyncPackageDesc* PackageRequest, TMap<FName, FAsyncPackage*>& PackageList)
{
	checkSlow(IsInAsyncLoadThread());
	FAsyncPackage* Result = PackageList.FindRef(PackageRequest->Name);
	if (Result)
	{
		if (PackageRequest->PackageLoadedDelegate.IsValid())
		{
			const bool bInternalCallback = false;
			Result->AddCompletionCallback(MoveTemp(PackageRequest->PackageLoadedDelegate), bInternalCallback);
		}
		Result->AddRequestID(PackageRequest->RequestID);
		
		const int32 QueuedPackagesCount = QueuedPackagesCounter.Decrement();
		check(QueuedPackagesCount >= 0);
		NotifyAsyncLoadingStateHasMaybeChanged();
	}
	return Result;
}

void FAsyncLoadingThread::UpdateExistingPackagePriorities(FAsyncPackage* InPackage, TAsyncLoadPriority InNewPriority)
{
	check(!IsInGameThread() || !IsMultithreaded());
	if (GEventDrivenLoaderEnabled)
	{
		InPackage->SetPriority(InNewPriority);
		return;
	}
	if (InNewPriority > InPackage->GetPriority())
	{
		AsyncPackages.Remove(InPackage);
		// always inserted anyway AsyncPackageNameLookup.Remove(InPackage->GetPackageName());
		InPackage->SetPriority(InNewPriority);

		InsertPackage(InPackage, false, EAsyncPackageInsertMode::InsertBeforeMatchingPriorities);

		// Reduce loading counters as InsertPackage incremented them again
		ExistingAsyncPackagesCounter.Decrement();
		NotifyAsyncLoadingStateHasMaybeChanged();
	}
}

void FAsyncLoadingThread::ProcessAsyncPackageRequest(FAsyncPackageDesc* InRequest, FAsyncPackage* InRootPackage)
{
	FAsyncPackage* Package = FindExistingPackageAndAddCompletionCallback(InRequest, AsyncPackageNameLookup);

	if (Package)
	{
		// The package is already sitting in the queue. Make sure the its priority, and the priority of all its
		// dependencies is at least as high as the priority of this request
		UpdateExistingPackagePriorities(Package, InRequest->Priority);
	}
	else
	{
		// [BLOCKING] LoadedPackages are accessed on the main thread too, so lock to be able to add a completion callback
#if THREADSAFE_UOBJECTS
		FScopeLock LoadedLock(&LoadedPackagesCritical);
#endif
		Package = FindExistingPackageAndAddCompletionCallback(InRequest, LoadedPackagesNameLookup);
	}

	if (!Package)
	{
		// [BLOCKING] LoadedPackagesToProcess are modified on the main thread, so lock to be able to add a completion callback
#if THREADSAFE_UOBJECTS
		FScopeLock LoadedLock(&LoadedPackagesToProcessCritical);
#endif
		Package = FindExistingPackageAndAddCompletionCallback(InRequest, LoadedPackagesToProcessNameLookup);
	}

	if (!Package)
	{
		// New package that needs to be loaded or a package has already been loaded long time ago
		{
			// GC can't run in here
			FGCScopeGuard GCGuard;
			Package = new FAsyncPackage(*this, *InRequest, EDLBootNotificationManager);
		}
		if (InRequest->PackageLoadedDelegate.IsValid())
		{
			const bool bInternalCallback = false;
			Package->AddCompletionCallback(MoveTemp(InRequest->PackageLoadedDelegate), bInternalCallback);
		}
		Package->SetDependencyRootPackage(InRootPackage);

		// Add to queue according to priority.
		InsertPackage(Package, false, EAsyncPackageInsertMode::InsertAfterMatchingPriorities);

		// For all other cases this is handled in FindExistingPackageAndAddCompletionCallback
		const int32 QueuedPackagesCount = QueuedPackagesCounter.Decrement();
		NotifyAsyncLoadingStateHasMaybeChanged();
		check(QueuedPackagesCount >= 0);
	}
}

int32 FAsyncLoadingThread::CreateAsyncPackagesFromQueue(bool bUseTimeLimit, bool bUseFullTimeLimit, double TimeLimit)
{
	SCOPED_LOADTIMER(CreateAsyncPackagesFromQueueTime);

	FAsyncLoadingTickScope InAsyncLoadingTick(*this);

	int32 NumCreated = 0;
	checkSlow(IsInAsyncLoadThread());

	int32 TimeSliceGranularity = 1; // do 4 packages at a time

	if (!bUseTimeLimit)
	{
		TimeSliceGranularity = MAX_int32; // no point in taking small steps
	}

	TArray<FAsyncPackageDesc*> QueueCopy;
	double TickStartTime = FPlatformTime::Seconds();
	do
	{
		{
#if THREADSAFE_UOBJECTS
			FScopeLock QueueLock(&QueueCritical);
#endif
			QueueCopy.Reset();
			QueueCopy.Reserve(FMath::Min(TimeSliceGranularity, QueuedPackages.Num()));

			int32 NumCopied = 0;

			for (FAsyncPackageDesc* PackageRequest : QueuedPackages)
			{
				if (NumCopied < TimeSliceGranularity)
				{
					NumCopied++;
					QueueCopy.Add(PackageRequest);
				}
				else
				{
					break;
				}
			}
			if (NumCopied)
			{
				QueuedPackages.RemoveAt(0, NumCopied, EAllowShrinking::No);
			}
			else
			{
				break;
			}
		}

		if (QueueCopy.Num() > 0)
		{
			SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_CreateAsyncPackagesFromQueue);
			double Timer = 0;
			{
				SCOPE_SECONDS_COUNTER(Timer);
				for (FAsyncPackageDesc* PackageRequest : QueueCopy)
				{
					ProcessAsyncPackageRequest(PackageRequest, nullptr);
					delete PackageRequest;
				}
			}
			UE_LOG(LogStreaming, Verbose, TEXT("Async package requests inserted in %fms"), Timer * 1000.0);
		}

		NumCreated += QueueCopy.Num();
	} while(!IsTimeLimitExceeded(TickStartTime, bUseTimeLimit, TimeLimit, TEXT("CreateAsyncPackagesFromQueue")));

	return NumCreated;
}

static FThreadSafeCounter AsyncPackageSerialNumber;

FName FUnsafeWeakAsyncPackagePtr::HumanReadableStringForDebugging() const
{
	return Package ? Package->GetPackageName() : FName();
}

FWeakAsyncPackagePtr::FWeakAsyncPackagePtr(FAsyncPackage* Package)
	: SerialNumber(0)
{
	if (Package)
	{
		PackageName = Package->GetPackageName();
		SerialNumber = Package->SerialNumber;
	}
}

FAsyncPackage& FWeakAsyncPackagePtr::GetPackage() const
{
	FAsyncPackage* Result = FAsyncLoadingThread::Get().GetPackage(*this);
	check(Result);
	return *Result;
}

FString FAsyncPackage::GetDebuggingPath(FPackageIndex Idx)
{
	if (!Linker)
	{
		return TEXT("Null linker");
	}
	FString Details;
	FString Prefix;
	if (Idx.IsExport() && Linker->LinkerRoot)
	{
		Prefix = Linker->LinkerRoot->GetName();
	}
	while (!Idx.IsNull())
	{
		FObjectResource& Res = Linker->ImpExp(Idx);
		Details = Res.ObjectName.ToString() / Details;
		Idx = Res.OuterIndex;
	}
	return Prefix / Details;
}

FString FEventLoadNodePtr::HumanReadableStringForDebugging() const
{
	const TCHAR* NodeName = TEXT("Unknown");
	FString Details;

	FAsyncPackage& Pkg = WaitingPackage.GetPackage();
	if (ImportOrExportIndex.IsNull())
	{
		switch (Phase)
		{
		case EEventLoadNode::Package_LoadSummary:
			NodeName = TEXT("Package_LoadSummary");
			break;
		case EEventLoadNode::Package_SetupImports:
			NodeName = TEXT("Package_SetupImports");
			break;
		case EEventLoadNode::Package_ExportsSerialized:
			NodeName = TEXT("Package_ExportsSerialized");
			break;
		default:
			check(0);
		}
	}
	else
	{
		switch (Phase)
		{
		case EEventLoadNode::ImportOrExport_Create:
			if (ImportOrExportIndex.IsImport())
			{
				NodeName = TEXT("Import_Create");
			}
			else
			{
				NodeName = TEXT("Export_Create");
			}
			break;
		case EEventLoadNode::Export_StartIO:
			NodeName = TEXT("Export_StartIO");
			break;
		case EEventLoadNode::ImportOrExport_Serialize:
			if (ImportOrExportIndex.IsImport())
			{
				NodeName = TEXT("Import_Serialize");
			}
			else
			{
				NodeName = TEXT("Export_Serialize");
			}
			break;
		default:
			check(0);
		}

		Details = Pkg.GetDebuggingPath(ImportOrExportIndex);
	}
	return FString::Printf(TEXT("%s %d %s   %s"), *WaitingPackage.HumanReadableStringForDebugging().ToString(), ImportOrExportIndex.ForDebugging(), NodeName, *Details);
}

void FEventLoadNodeArray::Init(int32 InNumImports, int32 InNumExports)
{
	check(InNumExports && !NumExports && TotalNumberOfNodesAdded <= int32(EEventLoadNode::Package_NumPhases) && !TotalNumberOfImportExportNodes);
	NumImports = InNumImports;
	NumExports = InNumExports;
	OffsetToImports = 0;
	OffsetToExports = OffsetToImports + NumImports * int32(EEventLoadNode::Import_NumPhases);
	TotalNumberOfImportExportNodes = OffsetToExports + NumExports * int32(EEventLoadNode::Export_NumPhases);
	check(TotalNumberOfImportExportNodes);
	Array = new FEventLoadNode[TotalNumberOfImportExportNodes];
}

void FEventLoadNodeArray::Shutdown()
{
	check(!TotalNumberOfNodesAdded);
	delete[] Array;
	Array = nullptr;
}
void FEventLoadNodeArray::GetAddedNodes(TArray<FEventLoadNodePtr>& OutAddedNodes, FAsyncPackage* Owner)
{
	if (TotalNumberOfNodesAdded)
	{
		FEventLoadNodePtr Node;
		Node.WaitingPackage = FCheckedWeakAsyncPackagePtr(Owner);
		for (int32 Index = 0; Index < int32(EEventLoadNode::Package_NumPhases); Index++)
		{
			Node.Phase = EEventLoadNode(Index);
			FEventLoadNode& NodeRef(PtrToNode(Node));
			if (NodeRef.bAddedToGraph)
			{
				OutAddedNodes.Add(Node);
			}
		}
		for (int32 ImportIndex = 0; ImportIndex < NumImports; ImportIndex++)
		{
			Node.ImportOrExportIndex = FPackageIndex::FromImport(ImportIndex);
			for (int32 Index = 0; Index < int32(EEventLoadNode::Import_NumPhases); Index++)
			{
				Node.Phase = EEventLoadNode(Index);
				FEventLoadNode& NodeRef(PtrToNode(Node));
				if (NodeRef.bAddedToGraph)
				{
					OutAddedNodes.Add(Node);
				}
			}

		}
		for (int32 ExportIndex = 0; ExportIndex < NumExports; ExportIndex++)
		{
			Node.ImportOrExportIndex = FPackageIndex::FromExport(ExportIndex);
			for (int32 Index = 0; Index < int32(EEventLoadNode::Export_NumPhases); Index++)
			{
				Node.Phase = EEventLoadNode(Index);
				FEventLoadNode& NodeRef(PtrToNode(Node));
				if (NodeRef.bAddedToGraph)
				{
					OutAddedNodes.Add(Node);
				}
			}

		}
	}
}

FORCEINLINE FEventLoadNodeArray& FEventLoadGraph::GetArray(FEventLoadNodePtr& Node)
{
	return Node.WaitingPackage.GetPackage().EventNodeArray;
}

FORCEINLINE FEventLoadNode& FEventLoadGraph::GetNode(FEventLoadNodePtr& NodeToGet)
{
	return GetArray(NodeToGet).GetNode(NodeToGet);
}



void FEventLoadGraph::AddNode(FEventLoadNodePtr& NewNode, bool bHoldForLater, int32 NumImplicitPrereqs)
{
	SCOPED_LOADTIMER_CNT(Graph_AddNode);

	FEventLoadNodeArray& Array = GetArray(NewNode);
	if (Array.AddNode(NewNode))
	{
		check(!PackagesWithNodes.Contains(NewNode.WaitingPackage));
		PackagesWithNodes.Add(NewNode.WaitingPackage);
	}
	int32 NumAddPrereq = (bHoldForLater ? 1 : 0) + NumImplicitPrereqs;
	if (NumAddPrereq)
	{
		Array.GetNode(NewNode).NumPrerequistes += NumAddPrereq;
	}
}

void FEventLoadGraph::AddArc(FEventLoadNodePtr& PrereqisitePtr, FEventLoadNodePtr& DependentPtr)
{
	SCOPED_LOADTIMER_CNT(Graph_AddArc);
	FEventLoadNode* PrereqisiteNode(&GetNode(PrereqisitePtr));
	FEventLoadNode* DependentNode(&GetNode(DependentPtr));
	check(!DependentNode->bFired);
	DependentNode->NumPrerequistes++;
	PrereqisiteNode->NodesWaitingForMe.Add(DependentPtr);
}

void FEventLoadGraph::RemoveNode(FEventLoadNodePtr& InNodeToRemove)
{
	FEventLoadNodePtr NodeToRemove(InNodeToRemove); // make a copy of this so we don't end up destroying in indirectly
	SCOPED_LOADTIMER_CNT(Graph_RemoveNode);
	check(FAsyncLoadingThread::Get().IsInAsyncLoadThread());
	check(!IndicesToFire.Num());

	FEventLoadNode::TNodesWaitingForMeArray NodesToFire;
	{
		FEventLoadNodeArray& Array = GetArray(NodeToRemove);
		FEventLoadNode* PrereqisiteNode(&Array.GetNode(NodeToRemove));
		check(PrereqisiteNode->bFired);
		check(PrereqisiteNode->NumPrerequistes == 0);
		Swap(NodesToFire, PrereqisiteNode->NodesWaitingForMe);

		for (int32 Index = 0; Index < NodesToFire.Num(); ++Index)
		{
			FEventLoadNode* DependentNode(&GetNode(NodesToFire[Index]));
			check(DependentNode->NumPrerequistes > 0);
			if (!--DependentNode->NumPrerequistes)
			{
				DependentNode->bFired = true;
				IndicesToFire.Add(Index);
			}
		}
		if (Array.RemoveNode(NodeToRemove))
		{
			PackagesWithNodes.Remove(NodeToRemove.WaitingPackage);
			Array.Shutdown();
		}
	}

#if 0

	if (IndicesToFire.Num() > 2) // sorting is not required, don't bother if there isn't much
	{
		IndicesToFire.Sort(
			[&NodesToFire](int32 A, int32 B) -> bool
			{
				return NodesToFire[A] < NodesToFire[B];
			}
		);
	}

	int32 CurrentWeakPtrSerialNumber = -1;
	FAsyncPackage* CurrentTarget = nullptr;
	for (int32 Index : IndicesToFire)
	{
		FEventLoadNodePtr& Target = NodesToFire[Index];
		if (CurrentWeakPtrSerialNumber != Target.WaitingPackage.SerialNumber)
		{
			CurrentWeakPtrSerialNumber = Target.WaitingPackage.SerialNumber;
			check(CurrentWeakPtrSerialNumber);
			CurrentTarget = FAsyncLoadingThread::Get().GetPackage(Target.WaitingPackage);
		}
		check(CurrentTarget && CurrentTarget->SerialNumber == Target.WaitingPackage.SerialNumber);
		SCOPED_LOADTIMER_CNT(Graph_RemoveNodeFire);
		CurrentTarget->FireNode(Target);
	}
#else
#if USE_IMPLICIT_ARCS
	int32 NumImplicitArcs = NodeToRemove.NumImplicitArcs();
	if (NumImplicitArcs)
	{
		check(NumImplicitArcs == 1); // would need different code otherwise
		FEventLoadNodePtr Target = NodeToRemove.GetImplicitArc();
		FEventLoadNode* DependentNode(&GetNode(Target));
		check(DependentNode->NumPrerequistes > 0);
		if (!--DependentNode->NumPrerequistes)
		{
			DependentNode->bFired = true;
			FAsyncPackage& CurrentTarget = Target.WaitingPackage.GetPackage();
			CurrentTarget.FireNode(Target);
		}
	}
#endif

	for (int32 Index : IndicesToFire)
	{
		FEventLoadNodePtr& Target = NodesToFire[Index];
		FAsyncPackage& CurrentTarget = Target.WaitingPackage.GetPackage();
#if VERIFY_WEAK_ASYNC_PACKAGE_PTRS
		check(CurrentTarget.SerialNumber == Target.WaitingPackage.SerialNumber);
#else
		check(CurrentTarget.SerialNumber);
#endif
		SCOPED_LOADTIMER_CNT(Graph_RemoveNodeFire);
		CurrentTarget.FireNode(Target);
	}
#endif
	IndicesToFire.Reset();
}


void FEventLoadGraph::NodeWillBeFiredExternally(FEventLoadNodePtr& NodeThatWasFired)
{
	SCOPED_LOADTIMER_CNT(Graph_Misc);
	FEventLoadNode* DependentNode(&GetNode(NodeThatWasFired));
	check(!DependentNode->bFired);
	DependentNode->bFired = true;
}

void FEventLoadGraph::DoneAddingPrerequistesFireIfNone(FEventLoadNodePtr& NewNode, bool bWasHeldForLater)
{
	SCOPED_LOADTIMER_CNT(Graph_DoneAddingPrerequistesFireIfNone);
	FEventLoadNode* DependentNode(&GetNode(NewNode));
	check(!DependentNode->bFired);
	if (bWasHeldForLater)
	{
		check(DependentNode->NumPrerequistes > 0);
		DependentNode->NumPrerequistes--;
	}
	if (!DependentNode->NumPrerequistes)
	{
		DependentNode->bFired = true;
		FAsyncPackage& CurrentTarget = NewNode.WaitingPackage.GetPackage();
		SCOPED_LOADTIMER_CNT(Graph_DoneAddingPrerequistesFireIfNoneFire);
		CurrentTarget.FireNode(NewNode);
	}
}

bool FEventLoadGraph::CheckForCyclesInner(const TMultiMap<FEventLoadNodePtr, FEventLoadNodePtr>& Arcs, TSet<FEventLoadNodePtr>& Visited, TSet<FEventLoadNodePtr>& Stack, const FEventLoadNodePtr& Visit)
{
	bool bResult = false;
	if (Stack.Contains(Visit))
	{
		bResult = true;
	}
	else
	{
		bool bWasAlreadyTested = false;
		Visited.Add(Visit, &bWasAlreadyTested);
		if (!bWasAlreadyTested)
		{
			Stack.Add(Visit);
			for (auto It = Arcs.CreateConstKeyIterator(Visit); !bResult && It; ++It)
			{
				bResult = CheckForCyclesInner(Arcs, Visited, Stack, It.Value());
			}
			Stack.Remove(Visit);
		}
	}
	UE_CLOG(bResult, LogStreaming, Error, TEXT("Cycle Node %s"), *Visit.HumanReadableStringForDebugging());
	return bResult;
}

void FEventLoadGraph::CheckForCycles(bool bDoSlowTests)
{
	int32 NumWaitingBoot = 0;
	if (bDoSlowTests)
	{
		TMultiMap<FEventLoadNodePtr, FEventLoadNodePtr> Arcs;
		TArray<FEventLoadNodePtr> AddedNodes;
		for (FCheckedWeakAsyncPackagePtr &Ptr : PackagesWithNodes)
		{
			FAsyncPackage* Pkg = &Ptr.GetPackage();
			Pkg->EventNodeArray.GetAddedNodes(AddedNodes, Pkg);
		}
		for (FEventLoadNodePtr &Ptr : AddedNodes)
		{
			FEventLoadNode& Node(GetNode(Ptr));

			if (!Node.NumPrerequistes)
			{
				if (GIsInitialLoad && Node.bFired)
				{
					// this is something that is compiled in, but has not been finished yet
					NumWaitingBoot++;
				}
				else if (!Node.bFired) // this will be queued later
				{
					UE_LOG(LogStreaming, Fatal, TEXT("Node %s has zero prerequisites, but has not been queued."), *Ptr.HumanReadableStringForDebugging());
				}
				else 
				{
					UE_LOG(LogStreaming, Warning, TEXT("Node %s has zero prerequisites, but has not been queued (usually waiting for an extenal queue, like the package summary)."), *Ptr.HumanReadableStringForDebugging());
				}
			}
			for (FEventLoadNodePtr Other : Node.NodesWaitingForMe)
			{
				Arcs.Add(Other, Ptr);
			}
#if USE_IMPLICIT_ARCS
			int32 NumImplicitArcs = Ptr.NumImplicitArcs();
			if (NumImplicitArcs)
			{
				check(NumImplicitArcs == 1); // would need different code otherwise
				FEventLoadNodePtr Target = Ptr.GetImplicitArc();
				Arcs.Add(Target, Ptr);
			}
#endif
		}
		TSet<FEventLoadNodePtr> Visited;
		TSet<FEventLoadNodePtr> Stack;
		for (FEventLoadNodePtr &Ptr : AddedNodes)
		{
			if (CheckForCyclesInner(Arcs, Visited, Stack, Ptr))
			{
				UE_LOG(LogStreaming, Fatal, TEXT("Async loading event graph contained a cycle, see above."));
			}
		}
		if (AddedNodes.Num() - NumWaitingBoot != 0)
		{
			for (FEventLoadNodePtr &Ptr : AddedNodes)
			{
				UE_LOG(LogStreaming, Error, TEXT("      AddedNode: %s"), *Ptr.HumanReadableStringForDebugging());
			}
			UE_LOG(LogStreaming, Fatal, TEXT("No outstanding IO, no nodes in the queue, yet we still have %d 'AddedNodes' in the graph (with %d boot nodes)."), AddedNodes.Num(), NumWaitingBoot);
		}
	}
	if (PackagesWithNodes.Num() && !NumWaitingBoot)
	{
		if (!bDoSlowTests)
		{
			UE_LOG(LogStreaming, Error, TEXT("Doing slow test"));
			CheckForCycles(true);
		}
		else
		{
			FString PackagesString;
			int32 Index = 0;
			for (FCheckedWeakAsyncPackagePtr &Ptr : PackagesWithNodes)
			{
				FAsyncPackage* Pkg = &Ptr.GetPackage();
				if (Pkg)
				{
					UE_LOG(LogStreaming, Error, TEXT("No outstanding IO, no nodes in the queue, yet we still have %s in the graph."), *Pkg->GetPackageName().ToString());

					if (Index < 5)
					{
						PackagesString += Pkg->GetPackageName().ToString();
						PackagesString += TEXT(",");
						Index++;
					}
					TArray<FEventLoadNodePtr> AddedNodes;
					Pkg->EventNodeArray.GetAddedNodes(AddedNodes, Pkg);
					for (FEventLoadNodePtr &NodePtr : AddedNodes)
					{
						UE_LOG(LogStreaming, Error, TEXT("      AddedNode: %s"), *NodePtr.HumanReadableStringForDebugging());
					}
				}
				else
				{
					UE_LOG(LogStreaming, Error, TEXT("No outstanding IO, no nodes in the queue, yet we still have [null ptr] in the graph."));
				}
			}
			UE_LOG(LogStreaming, Fatal, TEXT("No outstanding IO, no nodes in the queue, yet we still have %d 'PackagesWithNodes' in the graph: %s"), PackagesWithNodes.Num(), *PackagesString);
		}
	}
}

struct FPrecacheCallbackHandler
{
	FAsyncFileCallBack PrecacheCallBack;

	FCriticalSection IncomingLock;
	TArray<IAsyncReadRequest *> Incoming;
	TArray<FWeakAsyncPackagePtr> IncomingSummaries;
	bool bFireIncomingEvent; 
	FEvent* PermanentIncomingEvent;

	TMap<IAsyncReadRequest *, FWeakAsyncPackagePtr> WaitingPackages;
	TSet<FWeakAsyncPackagePtr> WaitingSummaries;

	int64 UnprocessedMemUsed;
	bool bPrecacheRequestsEnabled;
	bool bStalledOnMemory;

	FPrecacheCallbackHandler()
		: bFireIncomingEvent(false)
		, PermanentIncomingEvent(nullptr)
		, UnprocessedMemUsed(0)
		, bPrecacheRequestsEnabled(true)
		, bStalledOnMemory(false)

	{
		PrecacheCallBack =
			[this](bool bWasCanceled, IAsyncReadRequest* Request)
		{
			RequestComplete(bWasCanceled, Request);
		};
	}
	~FPrecacheCallbackHandler()
	{
		FScopeLock Lock(&IncomingLock);
		check(!bFireIncomingEvent);
		check(!Incoming.Num() && !IncomingSummaries.Num() && !WaitingPackages.Num() && !WaitingSummaries.Num());
		if (PermanentIncomingEvent)
		{
			FPlatformProcess::ReturnSynchEventToPool(PermanentIncomingEvent);
			PermanentIncomingEvent = nullptr;
		}
	}

	FAsyncFileCallBack* GetCompletionCallback()
	{
		return &PrecacheCallBack;
	}

	void RequestComplete(bool bWasCanceled, IAsyncReadRequest* Precache)
	{
		check(!bWasCanceled); // not handled yet
		FScopeLock Lock(&IncomingLock);
		Incoming.Add(Precache);
		if (bFireIncomingEvent)
		{
			bFireIncomingEvent = false; // only trigger once
			PermanentIncomingEvent->Trigger();
		}
		else 
		{
			if (Incoming.Num() == GMaxIncomingRequestsToStall)
			{
				UE_LOG(LogStreaming, Log, TEXT("Throttling on (incoming >= %d)"), GMaxIncomingRequestsToStall);
				UpdatePlatformFilePrecacheThrottling(false);
			}
		}
	}

	void SummaryComplete(const FWeakAsyncPackagePtr& Pkg)
	{
		FScopeLock Lock(&IncomingLock);
		IncomingSummaries.Add(Pkg);
		if (bFireIncomingEvent)
		{
			bFireIncomingEvent = false; // only trigger once
			PermanentIncomingEvent->Trigger();
		}
	}

	bool ProcessIncoming()
	{
		TArray<IAsyncReadRequest *> LocalIncoming;
		TArray<FWeakAsyncPackagePtr> LocalIncomingSummaries;
		{
			FScopeLock Lock(&IncomingLock);
			Swap(LocalIncoming, Incoming);
			Swap(LocalIncomingSummaries, IncomingSummaries);
		}
		for (IAsyncReadRequest * Req : LocalIncoming)
		{
			check(Req);
			FWeakAsyncPackagePtr Found = WaitingPackages.FindAndRemoveChecked(Req);
			FAsyncPackage* Pkg = FAsyncLoadingThread::Get().GetPackage(Found);
			check(Pkg);
			UnprocessedMemUsed += Pkg->PrecacheRequestReady(Req);
		}
		for (FWeakAsyncPackagePtr& Sum : LocalIncomingSummaries)
		{
			FAsyncLoadingThread* LocalAsyncLoadingThread = &FAsyncLoadingThread::Get();
			LocalAsyncLoadingThread->QueueEvent_FinishLinker(Sum, FAsyncLoadEvent::EventSystemPriority_MAX);
			check(WaitingSummaries.Contains(Sum));
			WaitingSummaries.Remove(Sum);
		}
		if (LocalIncoming.Num())
		{
			CheckThottleIOState(LocalIncoming.Num() >= GMaxIncomingRequestsToStall);
		}
		return LocalIncoming.Num() || LocalIncomingSummaries.Num();
	}

	bool AnyIOOutstanding()
	{
		return WaitingPackages.Num() || WaitingSummaries.Num();
	}

	bool WaitForIO(float SecondsToWait = 0.0f)
	{
		check(AnyIOOutstanding());
		check(SecondsToWait >= 0.0f);
		{
			FScopeLock Lock(&IncomingLock);
			if (Incoming.Num() || IncomingSummaries.Num())
			{
				return true;
			}
			if (!PermanentIncomingEvent)
			{
				PermanentIncomingEvent = FPlatformProcess::GetSynchEventFromPool();
			}
			bFireIncomingEvent = true;
		}
		if (SecondsToWait == 0.0f)
		{
			PermanentIncomingEvent->Wait();
			check(!bFireIncomingEvent);
			return true;
		}
		uint32 Ms = FMath::Max<uint32>(uint32(SecondsToWait * 1000.0f), 1);
		if (PermanentIncomingEvent->Wait(Ms))
		{
			check(!bFireIncomingEvent);
			return true;
		}
		FScopeLock Lock(&IncomingLock);
		if (bFireIncomingEvent)
		{
			// nobody triggered it
			bFireIncomingEvent = false;
			return false;
		}
		else
		{
			// We timed out and then it was triggered, so we have data and we need to reset the event
			PermanentIncomingEvent->Reset();
			return true;
		}
	}

	void RegisterNewPrecacheRequest(IAsyncReadRequest* Precache, FAsyncPackage* Package)
	{
		WaitingPackages.Add(Precache, FWeakAsyncPackagePtr(Package));
	}
	void RegisterNewSummaryRequest(FAsyncPackage* Package)
	{
		WaitingSummaries.Add(FWeakAsyncPackagePtr(Package));
	}

	void CheckThottleIOState(bool bMaybeWasStalledOnIncoming = false)
	{
		if (UnprocessedMemUsed <= static_cast<int64>(GMaxReadyRequestsToStallMB) * 1024 * 1024 * 9 / 10)
		{
			if (bStalledOnMemory)
			{
				if (!bPrecacheRequestsEnabled)
				{
					UE_LOG(LogStreaming, Log, TEXT("Throttling off (mem < %dMB)"), GMaxReadyRequestsToStallMB * 9 / 10);
					UpdatePlatformFilePrecacheThrottling(true);
					bPrecacheRequestsEnabled = true;
					bMaybeWasStalledOnIncoming = false; // we don't need to handle this anymore, we just turned it on
				}
			}
			bStalledOnMemory = false;
		}
		else if (UnprocessedMemUsed > static_cast<int64>(GMaxReadyRequestsToStallMB) * 1024 * 1024)
		{
			if (!bStalledOnMemory)
			{
				if (bPrecacheRequestsEnabled)
				{
					UE_LOG(LogStreaming, Log, TEXT("Throttling on (mem > %dMB)"), GMaxReadyRequestsToStallMB);
					UpdatePlatformFilePrecacheThrottling(false);
					bPrecacheRequestsEnabled = false;
				}
			}
			bStalledOnMemory = true;
		}

		if (bPrecacheRequestsEnabled && bMaybeWasStalledOnIncoming)
		{
			// we have to force a potentially redundant unstall just to make sure that the incoming stall is cleared now
			UE_LOG(LogStreaming, Log, TEXT("Throttling off (incoming grabbed)"));
			UpdatePlatformFilePrecacheThrottling(true);
		}
	}

	void FinishRequest(int64 Size)
	{
		UnprocessedMemUsed -= Size;
		check(UnprocessedMemUsed >= 0);
		CheckThottleIOState();
	}

	void UpdatePlatformFilePrecacheThrottling(bool bEnablePrecacheRequests)
	{
		CSV_EVENT(FileIO, TEXT("Precache %s"), bEnablePrecacheRequests ? TEXT("Enabled") : TEXT("Disabled"));
		// If we're not processing precache requests, set the min priority to GAsyncLoadingPrecachePriority + 1
		EAsyncIOPriorityAndFlags NewMinPriority = bEnablePrecacheRequests ? AIOP_MIN : (EAsyncIOPriorityAndFlags)FMath::Clamp(GAsyncLoadingPrecachePriority + 1, (int32)AIOP_MIN, (int32)AIOP_MAX);
		FPlatformFileManager::Get().GetPlatformFile().SetAsyncMinimumPriority(NewMinPriority);
	}
};

int32 GRandomizeLoadOrder = 0;
static FAutoConsoleVariableRef CVarRandomizeLoadOrder(
	TEXT("s.RandomizeLoadOrder"),
	GRandomizeLoadOrder,
	TEXT("If > 0, will randomize the load order of pending packages using this seed instead of using the most efficient order. This can be used to find bugs."),
	ECVF_Default
);

static int32 GetRandomSerialNumber(int32 MaxVal = MAX_int32)
{
	static FRandomStream RandomStream(GRandomizeLoadOrder);

	return RandomStream.RandHelper(MaxVal);
}

void FImportOrImportIndexArray::HeapPop(int32& OutItem, EAllowShrinking AllowShrinking)
{
	if (GRandomizeLoadOrder)
	{
		int32 Index = FMath::Clamp<int32>(GetRandomSerialNumber(Num() - 1), 0, Num() - 1);
		OutItem = (*this)[Index];
		RemoveAt(Index, 1, EAllowShrinking::No);
		return;
	}
	TArray<int32>::HeapPop(OutItem, AllowShrinking);
}


FScopedAsyncPackageEvent::FScopedAsyncPackageEvent(FAsyncPackage* InPackage)
	:Package(InPackage)
{
	check(Package);

	// Update the thread context with the current package. This is used by NotifyConstructedDuringAsyncLoading.
	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	PreviousPackage = static_cast<FAsyncPackage*>(ThreadContext.AsyncPackage);
	ThreadContext.AsyncPackage = Package;
	PreviousAsyncPackageLoader = ThreadContext.AsyncPackageLoader;
	ThreadContext.AsyncPackageLoader = &InPackage->AsyncLoadingThread;
	Package->BeginAsyncLoad();
	FExclusiveLoadPackageTimeTracker::PushLoadPackage(Package->Desc.PackagePath.GetPackageFName());
}

FScopedAsyncPackageEvent::~FScopedAsyncPackageEvent()
{
	FExclusiveLoadPackageTimeTracker::PopLoadPackage(Package->Linker ? Package->Linker->LinkerRoot : nullptr);
	Package->EndAsyncLoad();
	Package->LastObjectWorkWasPerformedOn = nullptr;
	Package->LastTypeOfWorkPerformed = nullptr;

	// Restore the package from the outer scope
	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	ThreadContext.AsyncPackage = PreviousPackage;
	ThreadContext.AsyncPackageLoader = PreviousAsyncPackageLoader;
}

FORCENOINLINE static bool CheckForFileOpenLogCommandLine()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("RandomizeLoadOrder")))
	{
		GRandomizeLoadOrder = 1;
	}

	return FParse::Param(FCommandLine::Get(), TEXT("FileOpenLog"));
}

FORCEINLINE static bool FileOpenLogActive()
{
#if 1
	static bool bDoingLoadOrder = CheckForFileOpenLogCommandLine() || CheckForFilePackageOpenLogCommandLine();
	return bDoingLoadOrder;
#else
	return true;
#endif
}

FORCEINLINE static bool CanAddWaitingPackages(FAsyncLoadingThread& AsyncLoadingThread)
{
	//for now, we're only capping off WaitingPackages with -fileopenlog, as per FORT-78563. however, problems are bound to manifest here in any case
	//marked by pathological load time performance, and this does not cover the "excessive load times when loading deployed, uncompressed data" case.
	//applying a sane cap in all circumstances would not be a terrible idea.
	const int32 MaxWaitingPackageCount = 1024;
	return !FileOpenLogActive() || AsyncLoadingThread.GetPrecacheHandler().WaitingPackages.Num() < MaxWaitingPackageCount;
}

void FAsyncLoadingThread::QueueEvent_CreateLinker(FAsyncPackage* Package, int32 EventSystemPriority)
{
	FileOpenLogActive();  // make sure GRandomizeLoadOrder is set up
	check(Package);
	Package->AddNode(EEventLoadNode::Package_LoadSummary);
	FWeakAsyncPackagePtr WeakPtr(Package);

	TAsyncLoadPriority UserPriority = Package->GetPriority();
	int32 PackageSerialNumber = GRandomizeLoadOrder ? GetRandomSerialNumber() : Package->SerialNumber;
	EventQueue.AddAsyncEvent(UserPriority, PackageSerialNumber, EventSystemPriority,
		TFunction<void(FAsyncLoadEventArgs& Args)>(
			[WeakPtr, this](FAsyncLoadEventArgs& Args)
			{
				FAsyncPackage* Pkg = GetPackage(WeakPtr);
				check(Pkg);
				if (Pkg)
				{
					Pkg->SetTimeLimit(Args, TEXT("Create Linker"));
					Pkg->Event_CreateLinker();
					Args.OutLastObjectWorkWasPerformedOn = Pkg->GetLinkerRoot();
				}
			}
	));
}

void FAsyncPackage::Event_CreateLinker()
{
	// Keep track of time when we start loading.
	if (LoadStartTime == 0.0)
	{
		double Now = FPlatformTime::Seconds();
		LoadStartTime = Now;

		// If we are a dependency of another package, we need to tell that package when its first dependent started loading,
		// otherwise because that package loads last it'll not include the entire load time of all its dependencies
		if (DependencyRootPackage)
		{
			// Only the first dependent needs to register the start time
			if (DependencyRootPackage->GetLoadStartTime() == 0.0)
			{
				DependencyRootPackage->LoadStartTime = Now;
			}
		}
	}
	FScopedAsyncPackageEvent Scope(this);
	SCOPED_LOADTIMER(Package_CreateLinker);
	check(!Linker);
	NodeWillBeFiredExternally(EEventLoadNode::Package_LoadSummary);
	CreateLinker();
	check(AsyncPackageLoadingState == EAsyncPackageLoadingState::NewPackage);
	if (Linker)
	{
		AsyncPackageLoadingState = EAsyncPackageLoadingState::WaitingForSummary;
		Linker->bLockoutLegacyOperations = true;
	}
	else
	{
		RemoveNode(EEventLoadNode::Package_LoadSummary);
		EventDrivenLoadingComplete();
		AsyncPackageLoadingState = EAsyncPackageLoadingState::PostLoad_Etc;
		check(!AsyncLoadingThread.AsyncPackagesReadyForTick.Contains(this));
		AsyncLoadingThread.AsyncPackagesReadyForTick.Add(this);
	}
}


void FAsyncLoadingThread::QueueEvent_FinishLinker(FWeakAsyncPackagePtr WeakPtr, int32 EventSystemPriority)
{
	FAsyncPackage* Pkg = GetPackage(WeakPtr);
	if (Pkg)
	{
		TAsyncLoadPriority UserPriority = Pkg->GetPriority();
		int32 PackageSerialNumber = GRandomizeLoadOrder ? GetRandomSerialNumber() : Pkg->SerialNumber;
		EventQueue.AddAsyncEvent(UserPriority, PackageSerialNumber, EventSystemPriority,
			TFunction<void(FAsyncLoadEventArgs& Args)>(
				[WeakPtr, this](FAsyncLoadEventArgs& Args)
		{
			FAsyncPackage* PkgInner = GetPackage(WeakPtr);
			check(PkgInner);
			if (PkgInner)
			{
				PkgInner->SetTimeLimit(Args, TEXT("Finish Linker"));
				PkgInner->Event_FinishLinker();
			}
		}));
	}

}


void FAsyncPackage::Event_FinishLinker()
{
	FScopedAsyncPackageEvent Scope(this);
	SCOPED_LOADTIMER(Package_FinishLinker);
	EAsyncPackageState::Type Result = EAsyncPackageState::Complete;
	if (!bLoadHasFailed)
	{
		Result = FinishLinker();
	}
	if (Result == EAsyncPackageState::TimeOut && !bLoadHasFailed)
	{
		AsyncLoadingThread.QueueEvent_FinishLinker(FWeakAsyncPackagePtr(this), FAsyncLoadEvent::EventSystemPriority_MAX);
		return;
	}

	if (!bLoadHasFailed)
	{
		check(Linker && Linker->HasFinishedInitialization());

		// Add nodes for all imports and exports.

		{
			LastTypeOfWorkPerformed = TEXT("ImportAddNode");
			int32 NumImplicitForImportExport = 0;
#if USE_IMPLICIT_ARCS
			NumImplicitForImportExport = 1;
#endif

			if (ImportAddNodeIndex == 0 && ExportAddNodeIndex == 0) // one time only
			{
				int32 NumImplicit = 0;
				check(Linker->ExportMap.Num());
#if USE_IMPLICIT_ARCS
				NumImplicit = Linker->ImportMap.Num() + Linker->ExportMap.Num();
#endif

				AddNode(EEventLoadNode::Package_ExportsSerialized, FPackageIndex(), false, NumImplicit);

				AddNode(EEventLoadNode::Package_SetupImports, FPackageIndex(), true);
				EventNodeArray.Init(Linker->ImportMap.Num(), Linker->ExportMap.Num());
			}
			if (PackagesWaitingToLinkImports.Num())
			{
				FCheckedWeakAsyncPackagePtr WeakThis(this);
				FEventLoadNodePtr MyDoneNode;
				MyDoneNode.WaitingPackage = WeakThis;
				MyDoneNode.Phase = EEventLoadNode::Package_ExportsSerialized;
				// There are things waiting to link imports. I need to not finish until those links are made
				// Package_ExportsSerialized is actually earlier than we need. We just need to make sure the linker is not destroyed before the other packages link
				for (FCheckedWeakAsyncPackagePtr& Waiter : PackagesWaitingToLinkImports)
				{
					FEventLoadNodePtr Prereq;
					Prereq.WaitingPackage = Waiter;
					Prereq.Phase = EEventLoadNode::Package_SetupImports;
					AddArc(Prereq, MyDoneNode);
				}
				PackagesWaitingToLinkImports.Empty();
			}
			FEventLoadNodePtr MyDependentExportsSerializedNode;
			MyDependentExportsSerializedNode.WaitingPackage = FCheckedWeakAsyncPackagePtr(this);
			MyDependentExportsSerializedNode.Phase = EEventLoadNode::Package_ExportsSerialized;

			for (int32 LocalImportIndex = ImportAddNodeIndex; LocalImportIndex < Linker->ImportMap.Num(); LocalImportIndex++)
			{
				//optimization: could avoid creating all of these nodes, in the common event that they are already done
				FEventLoadNodePtr MyDependentCreateNode = AddNode(EEventLoadNode::ImportOrExport_Create, FPackageIndex::FromImport(LocalImportIndex));
				FEventLoadNodePtr MyDependentSerializeNode = AddNode(EEventLoadNode::ImportOrExport_Serialize, FPackageIndex::FromImport(LocalImportIndex), false, NumImplicitForImportExport);

#if !USE_IMPLICIT_ARCS
				// Can't consider this import serialized until we hook it up after creation
				AddArc(MyDependentCreateNode, MyDependentSerializeNode);


				// Can't consider the package done with event driven loading until all imports are serialized
				AddArc(MyDependentSerializeNode, MyDependentExportsSerializedNode);
#endif
				ImportAddNodeIndex = LocalImportIndex + 1;
				if (LocalImportIndex % 50 == 0 && IsTimeLimitExceeded())
				{
					AsyncLoadingThread.QueueEvent_FinishLinker(FWeakAsyncPackagePtr(this), FAsyncLoadEvent::EventSystemPriority_MAX);
					return;
				}
			}

			LastTypeOfWorkPerformed = TEXT("ExportAddNode");
			for (int32 LocalExportIndex = ExportAddNodeIndex; LocalExportIndex < Linker->ExportMap.Num(); LocalExportIndex++)
			{
				//optimization: could avoid creating all of these nodes, in the (less) common event that they are already done

				FEventLoadNodePtr MyDependentCreateNode = AddNode(EEventLoadNode::ImportOrExport_Create, FPackageIndex::FromExport(LocalExportIndex));
				FEventLoadNodePtr MyDependentIONode = AddNode(EEventLoadNode::Export_StartIO, FPackageIndex::FromExport(LocalExportIndex), false, NumImplicitForImportExport);
				FEventLoadNodePtr MyDependentSerializeNode = AddNode(EEventLoadNode::ImportOrExport_Serialize, FPackageIndex::FromExport(LocalExportIndex), false, NumImplicitForImportExport);

#if !USE_IMPLICIT_ARCS
				// can't do the IO request until it is created
				AddArc(MyDependentCreateNode, MyDependentIONode);

				// can't serialize until the IO request is ready
				AddArc(MyDependentIONode, MyDependentSerializeNode);

				// Can't consider the package done with event driven loading until all exports are serialized
				AddArc(MyDependentSerializeNode, MyDependentExportsSerializedNode);
#endif
				ExportAddNodeIndex = LocalExportIndex + 1;

				if (LocalExportIndex % 30 == 0 && IsTimeLimitExceeded())
				{
					AsyncLoadingThread.QueueEvent_FinishLinker(FWeakAsyncPackagePtr(this), FAsyncLoadEvent::EventSystemPriority_MAX);
					return;
				}
			}
		}

		check(AsyncPackageLoadingState == EAsyncPackageLoadingState::WaitingForSummary);
		AsyncPackageLoadingState = EAsyncPackageLoadingState::StartImportPackages;
		AsyncLoadingThread.QueueEvent_StartImportPackages(this, FAsyncLoadEvent::EventSystemPriority_MAX - 1);
	}
	RemoveNode(EEventLoadNode::Package_LoadSummary);
	if (bLoadHasFailed)
	{
		EventDrivenLoadingComplete();
		AsyncPackageLoadingState = EAsyncPackageLoadingState::PostLoad_Etc;
		check(!AsyncLoadingThread.AsyncPackagesReadyForTick.Contains(this));
		AsyncLoadingThread.AsyncPackagesReadyForTick.Add(this);
	}
}

void FAsyncLoadingThread::QueueEvent_StartImportPackages(FAsyncPackage* Package, int32 EventSystemPriority)
{
	check(Package);
	FWeakAsyncPackagePtr WeakPtr(Package);

	TAsyncLoadPriority UserPriority = Package->GetPriority();
	int32 PackageSerialNumber = GRandomizeLoadOrder ? GetRandomSerialNumber() : Package->SerialNumber;
	EventQueue.AddAsyncEvent(UserPriority, PackageSerialNumber, EventSystemPriority,
		TFunction<void(FAsyncLoadEventArgs& Args)>(
			[WeakPtr, this](FAsyncLoadEventArgs& Args)
	{
		FAsyncPackage* Pkg = GetPackage(WeakPtr);
		if (Pkg)
		{
			Pkg->SetTimeLimit(Args, TEXT("Start Import Packages"));
			Pkg->Event_StartImportPackages();
		}
	}
	));
}

void FAsyncPackage::Event_StartImportPackages()
{
	LLM_SCOPE(ELLMTag::AsyncLoading);

	{
		FScopedAsyncPackageEvent Scope(this);
		if (LoadImports_Event() == EAsyncPackageState::TimeOut)
		{
			AsyncLoadingThread.QueueEvent_StartImportPackages(this, FAsyncLoadEvent::EventSystemPriority_MAX); // start here next frame
			return;
		}
	}

	check(AsyncPackageLoadingState == EAsyncPackageLoadingState::StartImportPackages);
	AsyncPackageLoadingState = EAsyncPackageLoadingState::WaitingForImportPackages;
	DoneAddingPrerequistesFireIfNone(EEventLoadNode::Package_SetupImports, FPackageIndex(), true);
}

/** Makes sure the specified object reference is added to the package reference list by the time we exit a function (early or not) */
struct FScopedAddObjectreference
{
	FAsyncPackage& Package;
	UObject*& Reference;

	FScopedAddObjectreference(FAsyncPackage& InPackage, UObject*& InReference)
		: Package(InPackage)
		, Reference(InReference)
	{
	}
	~FScopedAddObjectreference()
	{
		if (Reference)
		{
			Package.AddObjectReference(Reference);
		}
	}
};

//@todoio we should sort the imports at cook time so this recursive procedure is not needed.
FObjectImport* FAsyncPackage::FindExistingImport(int32 LocalImportIndex)
{
	FObjectImport* Import = &Linker->ImportMap[LocalImportIndex];
	if (!Import->XObject && !Import->bImportSearchedFor)
	{
		FScopedAddObjectreference OnExitAddReference(*this, Import->XObject);
		Import->bImportSearchedFor = true;
		if (Import->OuterIndex.IsNull())
		{
			Import->XObject = StaticFindObjectFast(UPackage::StaticClass(), nullptr, Linker->GetInstancingContext().RemapPackage(Import->ObjectName), true);
			check(!Import->XObject || CastChecked<UPackage>(Import->XObject));
		}
		else if (Import->OuterIndex.IsImport())
		{
			FObjectImport* ImportOuter = FindExistingImport(Import->OuterIndex.ToImport());
			if (ImportOuter->XObject)
			{
				Import->XObject = StaticFindObjectFast(UObject::StaticClass(), ImportOuter->XObject, Import->ObjectName, false);
				if (Import->XObject)
				{
					FName NameImportClass(Import->ClassName);
					FName NameActualImportClass(Import->XObject->GetClass()->GetFName());
					if (NameActualImportClass != NameImportClass)
					{
						static const FName NAME_BlueprintGeneratedClass("BlueprintGeneratedClass");
						static const FName NAME_DelegateFunction("DelegateFunction");

						bool bSafeException = (NameImportClass == NAME_Function && NameActualImportClass == NAME_DelegateFunction);

						if (!bSafeException)
						{
							FString ActualClass = *NameActualImportClass.ToString();
							FString ImportClass = *NameImportClass.ToString();
							FString PackageWithReference = *Desc.Name.ToString();

							// ^^^^ Send these to analytics or the crash report

							UE_LOG(LogStreaming, Error, TEXT("FAsyncPackage::FindExistingImport class mismatch %s != %s while reading package %s"), *ActualClass, *ImportClass, *PackageWithReference);

						}
					}
				}
			}
		}
		//else Outer is an export from the package we are currently loading, hence the import we are trying to find can't exist at this point.
	}
	return Import;
}

EAsyncPackageState::Type FAsyncPackage::LoadImports_Event()
{
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_LoadImports);
	SCOPED_LOADTIMER(LoadImports_Event);
	LastObjectWorkWasPerformedOn = LinkerRoot;
	LastTypeOfWorkPerformed = TEXT("loading imports event");

	// GC can't run in here
	FGCScopeGuard GCGuard;

	FCheckedWeakAsyncPackagePtr WeakThis(this);
	FEventLoadNodePtr MyDependentNode;
	MyDependentNode.WaitingPackage = WeakThis;
	MyDependentNode.Phase = EEventLoadNode::Package_SetupImports;

	bool bDidSomething = false;
	// Create imports.
	while (LoadImportIndex < Linker->ImportMap.Num() && !IsTimeLimitExceeded())
	{
		// Get the package for this import
		int32 LocalImportIndex = LoadImportIndex++;
		FObjectImport* Import = FindExistingImport(LocalImportIndex);
		FObjectImport* OriginalImport = Import;

		if (Import->XObject)
		{
			if (!IsFullyLoadedObj(Import->XObject))
			{
				UE_LOG(LogStreaming, Verbose, TEXT("FAsyncPackage::LoadImports for %s: import %s was found but was not fully loaded yet."), *Desc.PackagePath.GetDebugName(), *OriginalImport->ObjectName.ToString());
			}
			else
			{
				continue; // we already have this thing
			}
		}

		bool bIsPrestreamRequest = Import->ClassName == PrestreamPackageClassNameLoad;

		if (!GProcessPrestreamingRequests && bIsPrestreamRequest)
		{
			UE_LOG(LogStreaming, Display, TEXT("%s is NOT prestreaming %s"), *Desc.PackagePath.GetDebugName(), *Import->ObjectName.ToString());
			Import->bImportFailed = true;
			continue;
		}

		bool bForcePackageLoad = false;
		if (!Import->OuterIndex.IsNull() && !Import->bImportFailed)
		{
			// we didn't find an object, so we need to stream the package in because it might have been GC'd and we need to reload it (unless we have already done that according to bImportPackageHandled)
			FObjectImport* ImportOutermost = Import;

			// set the already handled flag as we go down..by the time we are done, they will all be handled
			while (!ImportOutermost->bImportPackageHandled && ImportOutermost->OuterIndex.IsImport())
			{
				ImportOutermost->bImportPackageHandled = true;
				ImportOutermost = &Linker->Imp(ImportOutermost->OuterIndex);
			}
			if (ImportOutermost->bImportPackageHandled)
			{
				continue;
			}
			check(ImportOutermost->OuterIndex.IsNull() || ImportOutermost->HasPackageName());
			ImportOutermost->bImportPackageHandled = true;
			bForcePackageLoad = true;
			Import = ImportOutermost; // just do the rest of the package code, but start the async package even if we find the upackage
		}
		// else don't set handled because bForcePackageLoad is false, meaning we might not set the thing anyway

		// @todoio: why do we need this? some UFunctions have null outer in the linker.
		if (Import->ClassName != NAME_Package && !bIsPrestreamRequest && !Import->HasPackageName())
		{
			check(0);
			continue;
		}

		// Don't try to import a package that is in an import table that we know is an invalid entry
		if (FLinkerLoad::IsKnownMissingPackage(!Import->HasPackageName() ? Import->ObjectName : Import->GetPackageName()))
		{
			continue;
		}
		UPackage* ExistingPackage = nullptr;
		FAsyncPackage* PendingPackage = nullptr;
		if (Import->XObject)
		{
			ExistingPackage = CastChecked<UPackage>(Import->XObject->GetPackage());
			PendingPackage = ExistingPackage->GetLinker() ? static_cast<FAsyncPackage*>(ExistingPackage->GetLinker()->AsyncRoot) : nullptr;
		}
		const bool bCompiledInNotDynamic = IsNativeCodePackage(ExistingPackage);
		// Our import package name is the import name
		const FName ImportPackageToLoad = !Import->HasPackageName() ? Import->ObjectName : Import->GetPackageName();
		const FName ImportPackageFName = Linker->GetInstancingContext().RemapPackage(ImportPackageToLoad);
		check(!PendingPackage || !bCompiledInNotDynamic); // we should never have a pending package for something that is compiled in
		if (!PendingPackage && !bCompiledInNotDynamic)
		{
			PendingPackage = AsyncLoadingThread.FindAsyncPackage(ImportPackageFName);
		}
		if (!PendingPackage)
		{
			if (bCompiledInNotDynamic)
			{
				// This can happen with editor only classes, not sure if this should be a warning or a silent continue
				if (!GIsInitialLoad)
				{
					UE_LOG(LogStreaming, Warning, TEXT("FAsyncPackage::LoadImports for %s: Skipping import %s, depends on missing native class"), *Desc.PackagePath.GetDebugName(), *Linker->GetImportFullName(LocalImportIndex));
				}
			}
			else if (!ExistingPackage || bForcePackageLoad)
			{
				// The package doesn't exist and this import is not in the dependency list so add it now.
				check(!FPackageName::IsShortPackageName(ImportPackageFName));
				UE_LOG(LogStreaming, Verbose, TEXT("FAsyncPackage::LoadImports for %s: Loading %s"), *Desc.PackagePath.GetDebugName(), *ImportPackageFName.ToString());
				FPackagePath ImportPackagePath = FPackagePath::FromPackageNameChecked(ImportPackageToLoad);
				const FAsyncPackageDesc Info(INDEX_NONE, ImportPackageFName, ImportPackagePath);
				PendingPackage = new FAsyncPackage(AsyncLoadingThread, Info, EDLBootNotificationManager);
				PendingPackage->Desc.Priority = Desc.Priority;
				// if we are also instancing the dependency, propagate the instancing context
				if (ImportPackageToLoad != ImportPackageFName)
				{
					PendingPackage->Desc.SetInstancingContext(Linker->GetInstancingContext());
				}
				if (bIsPrestreamRequest)
				{
					UE_LOG(LogStreaming, Display, TEXT("%s is prestreaming %s"), *Desc.PackagePath.GetDebugName(), *ImportPackagePath.GetDebugName());
				}
				TRACE_LOADTIME_ASYNC_PACKAGE_IMPORT_DEPENDENCY(this, PendingPackage);
#if !UE_BUILD_SHIPPING
				if(CheckForFilePackageOpenLogCommandLine())
				{
					FPlatformFileOpenLog* PlatformFileOpenLog = (FPlatformFileOpenLog*)(FPlatformFileManager::Get().FindPlatformFile(FPlatformFileOpenLog::GetTypeName()));
					if (PlatformFileOpenLog != nullptr)
					{
						FString PackageToOpenLogName = FString::Printf(TEXT("%s %i"), *Info.Name.ToString(), int32(GFrameCounter));
						PlatformFileOpenLog->AddPackageToOpenLog(*PackageToOpenLogName);
					}
				}
#endif
				AsyncLoadingThread.InsertPackage(PendingPackage);
				bDidSomething = true;
			}
			else
			{
				// it would be nice to make sure it is actually loaded as we expect
			}
		}
		if (PendingPackage)
		{
			AddDependentRequestsTo(PendingPackage);
			if (int32(PendingPackage->AsyncPackageLoadingState) <= int32(EAsyncPackageLoadingState::WaitingForSummary))
			{
				FEventLoadNodePtr PrereqisiteNode;
				PrereqisiteNode.WaitingPackage = FCheckedWeakAsyncPackagePtr(PendingPackage);
				PrereqisiteNode.Phase = EEventLoadNode::Package_LoadSummary;

				// we can't set up our imports until all packages we are importing have loaded their summary
				AddArc(PrereqisiteNode, MyDependentNode);

				// The other package should not leave the event driven loader until we have linked our imports, this just keeps it until we setup our imports...and at that time we will add more arcs
				// we can't do that just yet, so make a note of it to do it when the node is actually added (if it is ever added, might be a missing file or something)
				PendingPackage->PackagesWaitingToLinkImports.Add(WeakThis);
				bDidSomething = true;
			}
			else if (int32(PendingPackage->AsyncPackageLoadingState) < int32(EAsyncPackageLoadingState::WaitingForPostLoad))
			{
				FEventLoadNodePtr MyPrerequisiteNode;
				MyPrerequisiteNode.WaitingPackage = WeakThis;
				MyPrerequisiteNode.Phase = EEventLoadNode::Package_SetupImports;

				FEventLoadNodePtr DependentNode;
				DependentNode.WaitingPackage = FCheckedWeakAsyncPackagePtr(PendingPackage);
				DependentNode.Phase = EEventLoadNode::Package_ExportsSerialized;  // this could be much later...really all we care about is that the linker isn't destroyed.

				
				AddArc(MyPrerequisiteNode, DependentNode);
				bDidSomething = true;
			}
		}
		UpdateLoadPercentage();
	}

#if 0
	if (FileOpenLogActive() && !bDidSomething && LoadImportIndex == Linker->ImportMap.Num())
	{
		check(GetPriority() < MAX_int32);
		SetPriority(GetPriority() + 1);
	}
#endif
	return LoadImportIndex == Linker->ImportMap.Num() ? EAsyncPackageState::Complete : EAsyncPackageState::TimeOut;
}

void FAsyncLoadingThread::QueueEvent_SetupImports(FAsyncPackage* Package, int32 EventSystemPriority)
{
	check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState::WaitingForImportPackages);
	Package->AsyncPackageLoadingState = EAsyncPackageLoadingState::SetupImports;
	check(Package);
	FWeakAsyncPackagePtr WeakPtr(Package);
	TAsyncLoadPriority UserPriority = Package->GetPriority();
	int32 PackageSerialNumber = GRandomizeLoadOrder ? GetRandomSerialNumber() : Package->SerialNumber;
	EventQueue.AddAsyncEvent(UserPriority, PackageSerialNumber, EventSystemPriority,
		TFunction<void(FAsyncLoadEventArgs& Args)>(
			[WeakPtr, this](FAsyncLoadEventArgs& Args)
	{
		FAsyncPackage* Pkg = GetPackage(WeakPtr);
		if (Pkg)
		{
			Pkg->SetTimeLimit(Args, TEXT("Setup Imports"));
			Pkg->Event_SetupImports();
		}
	}
	));
}

void FAsyncPackage::Event_SetupImports()
{
	{
		FScopedAsyncPackageEvent Scope(this);
		//@todo we need to time slice this, it runs to completion at the moment
		verify(SetupImports_Event() == EAsyncPackageState::Complete);
	}
	check(AsyncPackageLoadingState == EAsyncPackageLoadingState::SetupImports);
	check(ImportIndex == Linker->ImportMap.Num());
	AsyncPackageLoadingState = EAsyncPackageLoadingState::SetupExports;
	RemoveNode(EEventLoadNode::Package_SetupImports);
	AsyncLoadingThread.QueueEvent_SetupExports(this);
}

static FPackageIndex FindImportFromExport(FLinkerLoad* ImportLinker, int32 ExportIndex, FLinkerLoad* ExportLinker)
{
	//@todo FH: redo ObjectNameWithOuterToExport to be a hash of object name, outer name and the class name
	FPackageIndex Result;
	const FObjectExport& Export = ExportLinker->ExportMap[ExportIndex];

	for (int ImportIndex = 0; ImportIndex < ImportLinker->ImportMap.Num(); ++ImportIndex)
	{
		const FObjectImport& Import = ImportLinker->ImportMap[ImportIndex];
		if (Import.ObjectName == Export.ObjectName &&
			Import.ClassName == ExportLinker->ImpExp(Export.ClassIndex).ObjectName &&
			ImportLinker->ImpExp(Import.OuterIndex).ObjectName == ExportLinker->ImpExp(Export.OuterIndex).ObjectName)
		{
			return FPackageIndex::FromImport(ImportIndex);
		}
	}
	return FPackageIndex();
}

static FPackageIndex FindExportFromImport(FLinkerLoad* ImportLinker, int32 ImportIndex, FLinkerLoad* ExportLinker)
{
	check(ImportLinker && ImportLinker->AsyncRoot && static_cast<FAsyncPackage*>(ImportLinker->AsyncRoot)->ObjectNameWithOuterToExport.Num());
	FPackageIndex Result;
	const FObjectImport& Import = ExportLinker->ImportMap[ImportIndex];
	
	if (!Import.OuterIndex.IsNull())
	{
		FPackageIndex OuterIndex = Import.OuterIndex.IsImport() ? FindExportFromImport(ImportLinker, Import.OuterIndex.ToImport(), ExportLinker) : FindImportFromExport(ImportLinker, Import.OuterIndex.ToExport(), ExportLinker);
		FPackageIndex* PotentialExport = static_cast<FAsyncPackage*>(ImportLinker->AsyncRoot)->ObjectNameWithOuterToExport.Find(TPair<FName, FPackageIndex>(Import.ObjectName, OuterIndex));
		if (PotentialExport)
		{
			Result = *PotentialExport;
		}
	}
	return Result;
}

EAsyncPackageState::Type FAsyncPackage::SetupImports_Event()
{
	SCOPED_LOADTIMER(CreateImportsTime);
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_CreateImports);

	// GC can't run in here
	FGCScopeGuard GCGuard;

	FCheckedWeakAsyncPackagePtr WeakThis(this);

	if (!ImportIndex)
	{
		for (int32 InnerImportIndex = 0; InnerImportIndex < Linker->ImportMap.Num(); InnerImportIndex++)
		{
			Linker->ImportMap[InnerImportIndex].bImportSearchedFor = false; // we need to clear these if we have to call FindExistingImport below
		}
	}

	// Create imports.
	bool bAnyImportArcsAdded = false;
	while (ImportIndex < Linker->ImportMap.Num())
	{
		bool bFireIfNoArcsAdded = true;
		int32 LocalImportIndex = ImportIndex++;
		FScopedCreateImportCounter ScopedCounter(Linker, LocalImportIndex);
		FObjectImport& Import = Linker->ImportMap[LocalImportIndex];

		if (Import.OuterIndex.IsNull())
		{
			if (!Import.bImportFailed)
			{
				UPackage* ImportPackage = Import.XObject ? CastChecked<UPackage>(Import.XObject) : nullptr;
				if (!ImportPackage)
				{
					ImportPackage = FindObjectFast<UPackage>(NULL, Import.ObjectName, false);
					if (!ImportPackage)
					{
						Import.bImportFailed = true;
						UE_CLOG(!FLinkerLoad::IsKnownMissingPackage(Import.ObjectName), LogStreaming, Error, TEXT("Missing native package (%s) for import of package %s"), *Import.ObjectName.ToString(), *Desc.PackagePath.GetDebugName());
					}
					else
					{
						Import.XObject = ImportPackage;
						AddObjectReference(Import.XObject);
					}
				}

				if (ImportPackage)
				{
					ImportedPackages.Add(ImportPackage);

					FLinkerLoad* ImportLinker = ImportPackage->GetLinker();
					if (ImportLinker && ImportLinker->AsyncRoot)
					{
						check(ImportLinker->AsyncRoot != this);
						// make sure we wait for this package to serialize (and all of its dependents) before we start doing postloads
						if (int32(static_cast<FAsyncPackage*>(ImportLinker->AsyncRoot)->AsyncPackageLoadingState) <= int32(EAsyncPackageLoadingState::WaitingForPostLoad)) // no need to clutter dependencies with things that are already done
						{
							PackagesIMayBeWaitingForBeforePostload.Add(FWeakAsyncPackagePtr(static_cast<FAsyncPackage*>(ImportLinker->AsyncRoot)));
						}
					}
				}
			}
		}
		else if (!Import.XObject || !IsFullyLoadedObj(Import.XObject) 
			|| GIsInitialLoad) // during the initial load, we might find the object, but it hasn't yet actually been finished
		{
			FPackageIndex OuterMostIndex = FPackageIndex::FromImport(LocalImportIndex);
			FPackageIndex OuterMostNonPackageIndex = OuterMostIndex;
			while (true)
			{
				check(!OuterMostIndex.IsNull() && OuterMostIndex.IsImport());
				FObjectImport& OuterMostImport = Linker->Imp(OuterMostIndex);
				if (OuterMostImport.OuterIndex.IsNull() || OuterMostImport.HasPackageName())
				{
					break;
				}
				OuterMostNonPackageIndex = OuterMostIndex;
				OuterMostIndex = OuterMostImport.OuterIndex;
			}
			FObjectImport& OuterMostImport = Linker->Imp(OuterMostIndex);
			check(OuterMostImport.OuterIndex.IsNull() || OuterMostImport.HasPackageName());
			FName ImportPackageName = Linker->GetInstancingContext().RemapPackage(!OuterMostImport.HasPackageName() ? OuterMostImport.ObjectName : OuterMostImport.GetPackageName());
			UPackage* ImportPackage = OuterMostImport.XObject ? OuterMostImport.XObject->GetPackage() : nullptr;
			if (!ImportPackage)
			{
				ImportPackage = FindObjectFast<UPackage>(nullptr, ImportPackageName, false);
				if (!ImportPackage)
				{
					Import.bImportFailed = true;
					UE_CLOG(!FLinkerLoad::IsKnownMissingPackage(ImportPackageName), LogStreaming, Error, TEXT("Missing native package (%s) for import of %s in %s."), *ImportPackageName.ToString(), *Import.ObjectName.ToString(), *Desc.PackagePath.GetDebugName());
				}
				else if (OuterMostImport.OuterIndex.IsNull())
				{
					OuterMostImport.XObject = ImportPackage; // this is an optimization to avoid looking up import packages multiple times, also, later we assume these are already filled in
					AddObjectReference(OuterMostImport.XObject);
				}
			}

			bool bWaitingForImport = false;
			if (ImportPackage)
			{
				FLinkerLoad* ImportLinker = ImportPackage->GetLinker();

#if USE_EVENT_DRIVEN_ASYNC_LOAD_AT_BOOT_TIME
				if (GIsInitialLoad && !ImportLinker && ImportPackage->HasAnyPackageFlags(PKG_CompiledIn))
				{
					// compiled in package shouldn't be involved in non outer package import
					check(OuterMostImport.OuterIndex.IsNull());
					// OuterMostNonPackageIndex is used here because if it is a CDO or subobject, etc, we wait for the outermost thing that is not a package
					bFireIfNoArcsAdded = !EDLBootNotificationManager.AddWaitingPackage(
						this,
						ImportPackageName,
						Linker->Imp(OuterMostNonPackageIndex).ObjectName,
						FPackageIndex::FromImport(LocalImportIndex),
						/*bIgnoreMissingPackage*/ false);
				}
#endif
				if (bFireIfNoArcsAdded && // if bFireIfNoArcsAdded is false, then we know we are waiting for compiled in thing, so don't bother to look for the import now
					(!ImportLinker || !ImportLinker->AsyncRoot))
				{
					FindExistingImport(LocalImportIndex);
					bool bFinishedLoading = IsFullyLoadedObj(Import.XObject);

					if (Import.XObject)
					{
						if (!bFinishedLoading)
						{
							UE_LOG(LogStreaming, Error, TEXT("Found package without a linker, could find %s in %s, but somehow wasn't finished loading. This can occur with client+server cooks loading on client-only executables."), *Import.ObjectName.ToString(), *ImportPackage->GetName());
							Import.XObject = nullptr;
							Import.bImportFailed = true;
						}
					}
					else
					{
						// This can happen for missing packages on disk, which already warned
						Import.bImportFailed = true;
					}
				}
				if (ImportLinker && ImportLinker->AsyncRoot)
				{
					check(ImportLinker->AsyncRoot != this);
					check(!Import.OuterIndex.IsNull());
					
					FPackageIndex LocalExportIndex = FindExportFromImport(ImportLinker, LocalImportIndex, Linker);
					FName OuterName = NAME_None;
					if (!LocalExportIndex.IsNull())
					{
						check(ImportLinker->Exp(LocalExportIndex).ObjectName == Import.ObjectName);
						FPackageIndex LocalExportOuterIndex = ImportLinker->Exp(LocalExportIndex).OuterIndex;
						if (LocalExportOuterIndex.IsExport())
						{
							OuterName = ImportLinker->Exp(LocalExportOuterIndex).ObjectName;
						}
						else if (LocalExportOuterIndex.IsImport())
						{
							OuterName = ImportLinker->Imp(LocalExportOuterIndex).ObjectName;
						}
						else if (LocalExportOuterIndex.IsNull())
						{
							OuterName = ImportLinker->LinkerRoot->GetFName();
						}
						check(OuterName != NAME_None);
						check(OuterName == Linker->GetInstancingContext().RemapPackage(Linker->ImpExp(Import.OuterIndex).ObjectName));
					}

					Import.bImportFailed = LocalExportIndex.IsNull();
					UE_CLOG(Import.bImportFailed, LogStreaming, Warning, TEXT("Could not find import %s.%s in package %s."), *OuterName.ToString(), *Import.ObjectName.ToString(), *ImportPackage->GetName());
					if (Import.bImportFailed)
					{
						UE_LOG(LogStreaming, Warning, TEXT("    Full import name %s"), *Linker->GetPathName(FPackageIndex::FromImport(LocalImportIndex)));
						UE_LOG(LogStreaming, Warning, TEXT("    AsyncRoot = %s"), *static_cast<FAsyncPackage*>(ImportLinker->AsyncRoot)->Desc.Name.ToString());
						for (int32 i = 0; i < ImportLinker->ExportMap.Num(); i++)
						{
							FObjectExport& PrintExport = ImportLinker->Exp(FPackageIndex::FromExport(i));
							UE_LOG(LogStreaming, Warning, TEXT("        Export %5d (outer %5d)   %s     (class %s)"), i,
								!PrintExport.OuterIndex.IsExport() ? -1 : PrintExport.OuterIndex.ToExport(),
								*ImportLinker->GetPathName(FPackageIndex::FromExport(i)),
								PrintExport.ClassIndex.IsNull() ? TEXT("null") : *ImportLinker->ImpExp(PrintExport.ClassIndex).ObjectName.ToString()
							);
						}
					}
					if (!Import.bImportFailed)
					{
						FObjectExport& Export = ImportLinker->Exp(LocalExportIndex);
						Import.bImportFailed = Export.bExportLoadFailed;
						if (!Import.bImportFailed)
						{
							Import.SourceIndex = LocalExportIndex.ToExport();
							Import.SourceLinker = ImportLinker;
							if (!Export.Object)
							{
								bAnyImportArcsAdded = true;
								FEventLoadNodePtr MyDependentNode;
								MyDependentNode.WaitingPackage = WeakThis;
								MyDependentNode.ImportOrExportIndex = FPackageIndex::FromImport(LocalImportIndex);
								MyDependentNode.Phase = EEventLoadNode::ImportOrExport_Create;

								{
									FEventLoadNodePtr PrereqisiteNode;
									PrereqisiteNode.WaitingPackage = FCheckedWeakAsyncPackagePtr(static_cast<FAsyncPackage*>(ImportLinker->AsyncRoot));
									PrereqisiteNode.ImportOrExportIndex = LocalExportIndex;
									PrereqisiteNode.Phase = EEventLoadNode::ImportOrExport_Create;

									// can't create an import until the corresponding export is created
									AddArc(PrereqisiteNode, MyDependentNode);
								}


								{
									FEventLoadNodePtr DependentNode;
									DependentNode.WaitingPackage = FCheckedWeakAsyncPackagePtr(static_cast<FAsyncPackage*>(ImportLinker->AsyncRoot));
									DependentNode.Phase = EEventLoadNode::Package_ExportsSerialized; // this could be much later...really all we care about is that the linker isn't destroyed.

									// The other package should not leave the event driven loader until we have linked this import
									AddArc(MyDependentNode, DependentNode);
								}
							}
							else
							{
								check(!Import.XObject || Import.XObject == Export.Object);
								Import.XObject = Export.Object;
								AddObjectReference(Import.XObject);
							}
							if (!IsFullyLoadedObj(Export.Object))
							{
								bAnyImportArcsAdded = true;
								FEventLoadNodePtr MyDependentNode;
								MyDependentNode.WaitingPackage = WeakThis;
								MyDependentNode.ImportOrExportIndex = FPackageIndex::FromImport(LocalImportIndex);
								MyDependentNode.Phase = EEventLoadNode::ImportOrExport_Serialize;

								FEventLoadNodePtr PrereqisiteNode;
								PrereqisiteNode.WaitingPackage = FCheckedWeakAsyncPackagePtr(static_cast<FAsyncPackage*>(ImportLinker->AsyncRoot));
								PrereqisiteNode.ImportOrExportIndex = LocalExportIndex;
								PrereqisiteNode.Phase = EEventLoadNode::ImportOrExport_Serialize;

								// can't consider an import serialized until the corresponding export is serialized
								AddArc(PrereqisiteNode, MyDependentNode);
							}
						}
					}
				}
			}
		}
		if (bFireIfNoArcsAdded)
		{
			DoneAddingPrerequistesFireIfNone(EEventLoadNode::ImportOrExport_Create, FPackageIndex::FromImport(LocalImportIndex));
		}
		else
		{
			NodeWillBeFiredExternally(EEventLoadNode::ImportOrExport_Create, FPackageIndex::FromImport(LocalImportIndex));
		}
	}

#if 0
	if (bAnyImportArcsAdded && Linker->GetAsyncLoader())
	{
		// we are waiting for imports, so drop our precache requests
		Linker->GetAsyncLoader()->FlushCache();
	}
#endif
	return ImportIndex == Linker->ImportMap.Num() ? EAsyncPackageState::Complete : EAsyncPackageState::TimeOut;
}


void FAsyncLoadingThread::QueueEvent_SetupExports(FAsyncPackage* Package, int32 EventSystemPriority)
{
	check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState::SetupExports);
	check(Package);
	FWeakAsyncPackagePtr WeakPtr(Package);
	TAsyncLoadPriority UserPriority = Package->GetPriority();
	int32 PackageSerialNumber = GRandomizeLoadOrder ? GetRandomSerialNumber() : Package->SerialNumber;
	EventQueue.AddAsyncEvent(UserPriority, PackageSerialNumber, EventSystemPriority,
		TFunction<void(FAsyncLoadEventArgs& Args)>(
			[WeakPtr, this](FAsyncLoadEventArgs& Args)
	{
		FAsyncPackage* Pkg = GetPackage(WeakPtr);
		if (Pkg)
		{
			Pkg->SetTimeLimit(Args, TEXT("Setup Exports"));
			Pkg->Event_SetupExports();
		}
	}
	));
}

void FAsyncPackage::Event_SetupExports()
{
	{
		FScopedAsyncPackageEvent Scope(this);
		if (SetupExports_Event() == EAsyncPackageState::TimeOut)
		{
			AsyncLoadingThread.QueueEvent_SetupExports(this); // start here next frame
			return;
		}
	}
	check(AsyncPackageLoadingState == EAsyncPackageLoadingState::SetupExports);
	AsyncPackageLoadingState = EAsyncPackageLoadingState::ProcessNewImportsAndExports;
	ConditionalQueueProcessImportsAndExports();
}

void FAsyncLoadingThread::QueueEvent_ProcessImportsAndExports(FAsyncPackage* Package, int32 EventSystemPriority)
{
	check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState::ProcessNewImportsAndExports);
	check(Package);
	FWeakAsyncPackagePtr WeakPtr(Package);
	TAsyncLoadPriority UserPriority = Package->GetPriority();
	int32 PackageSerialNumber = GRandomizeLoadOrder ? GetRandomSerialNumber() : Package->SerialNumber;
	EventQueue.AddAsyncEvent(Package->GetPriority(), GRandomizeLoadOrder ? GetRandomSerialNumber() : Package->SerialNumber, EventSystemPriority,
		TFunction<void(FAsyncLoadEventArgs& Args)>(
			[WeakPtr, this](FAsyncLoadEventArgs& Args)
	{
		FAsyncPackage* Pkg = GetPackage(WeakPtr);
		if (Pkg)
		{
			Pkg->SetTimeLimit(Args, TEXT("ProcessImportsAndExports"));
			Pkg->Event_ProcessImportsAndExports();
		}
	}
	));
}

void FAsyncLoadingThread::QueueEvent_ProcessPostloadWait(FAsyncPackage* Package, int32 EventSystemPriority)
{
	check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState::WaitingForPostLoad);
	check(Package);
	FWeakAsyncPackagePtr WeakPtr(Package);
	TAsyncLoadPriority UserPriority = Package->GetPriority();
	int32 PackageSerialNumber = GRandomizeLoadOrder ? GetRandomSerialNumber() : Package->SerialNumber;
	EventQueue.AddAsyncEvent(UserPriority, PackageSerialNumber, EventSystemPriority,
		TFunction<void(FAsyncLoadEventArgs& Args)>(
			[WeakPtr, this](FAsyncLoadEventArgs& Args)
	{
		FAsyncPackage* Pkg = GetPackage(WeakPtr);
		if (Pkg)
		{
			Pkg->SetTimeLimit(Args, TEXT("Process Process Postload Wait"));
			Pkg->Event_ProcessPostloadWait();
		}
	}
	));
}

void FAsyncLoadingThread::QueueEvent_ExportsDone(FAsyncPackage* Package, int32 EventSystemPriority)
{
	check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState::ProcessNewImportsAndExports);
	check(Package);
	FWeakAsyncPackagePtr WeakPtr(Package);
	TAsyncLoadPriority UserPriority = Package->GetPriority();
	int32 PackageSerialNumber = GRandomizeLoadOrder ? GetRandomSerialNumber() : Package->SerialNumber;
	EventQueue.AddAsyncEvent(UserPriority, PackageSerialNumber, EventSystemPriority,
		TFunction<void(FAsyncLoadEventArgs& Args)>(
			[WeakPtr, this](FAsyncLoadEventArgs& Args)
	{
		FAsyncPackage* Pkg = GetPackage(WeakPtr);
		if (Pkg)
		{
			Pkg->SetTimeLimit(Args, TEXT("Exports Done"));
			Pkg->Event_ExportsDone();
		}
	}
	));
}

void FAsyncLoadingThread::QueueEvent_StartPostLoad(FAsyncPackage* Package, int32 EventSystemPriority)
{
	check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState::ReadyForPostLoad);
	check(Package);
	FWeakAsyncPackagePtr WeakPtr(Package);
	TAsyncLoadPriority UserPriority = Package->GetPriority();
	int32 PackageSerialNumber = GRandomizeLoadOrder ? GetRandomSerialNumber() : Package->SerialNumber;
	EventQueue.AddAsyncEvent(Package->GetPriority(), GRandomizeLoadOrder ? GetRandomSerialNumber() : Package->SerialNumber, EventSystemPriority,
		TFunction<void(FAsyncLoadEventArgs& Args)>(
			[WeakPtr, this](FAsyncLoadEventArgs& Args)
	{
		FAsyncPackage* Pkg = GetPackage(WeakPtr);
		if (Pkg)
		{
			Pkg->SetTimeLimit(Args, TEXT("Start Post Load"));
			Pkg->Event_StartPostload();
		}
	}
	));
}
bool FAsyncPackage::AnyImportsAndExportWorkOutstanding()
{
	return ImportsThatAreNowCreated.Num() ||
		ImportsThatAreNowSerialized.Num() ||
		ExportsThatCanBeCreated.Num() ||
		ExportsThatCanHaveIOStarted.Num() ||
		ExportsThatCanBeSerialized.Num() ||
		ReadyPrecacheRequests.Num();
}

void FAsyncPackage::ConditionalQueueProcessImportsAndExports(bool bRequeueForTimeout)
{
	if (AsyncPackageLoadingState != EAsyncPackageLoadingState::ProcessNewImportsAndExports)
	{
		return;
	}
	if (!bProcessImportsAndExportsInFlight && AnyImportsAndExportWorkOutstanding())
	{
		bProcessImportsAndExportsInFlight = true;
		int32 Pri = -1;
		if (ReadyPrecacheRequests.Num())
		{
			Pri = -2;
		}
		else if (ExportsThatCanHaveIOStarted.Num() && PrecacheRequests.Num() < 2)
		{
			Pri = -3;
		}
		AsyncLoadingThread.QueueEvent_ProcessImportsAndExports(this , Pri);
	}
}

void FAsyncPackage::ConditionalQueueProcessPostloadWait()
{
	check(AsyncPackageLoadingState == EAsyncPackageLoadingState::WaitingForPostLoad);
	if (!bProcessPostloadWaitInFlight
		&& !PackagesIAmWaitingForBeforePostload.Num()) // if there other things we are waiting for, no need to the do the processing now
	{
		bProcessPostloadWaitInFlight = true;
		AsyncLoadingThread.QueueEvent_ProcessPostloadWait(this);
	}
}

EAsyncPackageState::Type FAsyncPackage::SetupExports_Event()
{
	SCOPED_LOADTIMER(CreateExportsTime);
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_CreateExports);

	// GC can't run in here
	FGCScopeGuard GCGuard;

	FCheckedWeakAsyncPackagePtr WeakThis(this);
	Linker->GetAsyncLoader()->LogItem(TEXT("SetupExports_Event"));

	LastTypeOfWorkPerformed = TEXT("SetupExports_Event");
	LastObjectWorkWasPerformedOn = LinkerRoot;
	// Create exports.
	while (ExportIndex < Linker->ExportMap.Num())
	{
		if (ExportIndex % 10 == 0 && IsTimeLimitExceeded())
		{
			break;
		}
		int32 LocalExportIndex = ExportIndex++;
		FObjectExport& Export = Linker->ExportMap[LocalExportIndex];
		// Check whether we already loaded the object and if not whether the context flags allow loading it.
		check(!Export.Object); // we should not have this yet
		if (!Export.Object)
		{ 
			if (!Linker->FilterExport(Export) && !Export.ClassIndex.IsNull())
			{
				check(Export.ObjectName != NAME_None || !(Export.ObjectFlags&RF_Public));

				int32 RunningIndex = Export.FirstExportDependency;
				if (RunningIndex >= 0)
				{
					FEventLoadNodePtr MyDependentNode;
					MyDependentNode.WaitingPackage = WeakThis;
					MyDependentNode.ImportOrExportIndex = FPackageIndex::FromExport(LocalExportIndex);

					FEventLoadNodePtr PrereqisiteNode;
					PrereqisiteNode.WaitingPackage = WeakThis;

					MyDependentNode.Phase = EEventLoadNode::Export_StartIO;
					PrereqisiteNode.Phase = EEventLoadNode::ImportOrExport_Serialize;
					for (int32 Index = Export.SerializationBeforeSerializationDependencies; Index > 0; Index--)
					{
						FPackageIndex Dep = Linker->PreloadDependencies[RunningIndex++];
						check(!Dep.IsNull());
						PrereqisiteNode.ImportOrExportIndex = Dep;
						// don't request IO for this export until these are serialized
						AddArc(PrereqisiteNode, MyDependentNode);
					}

					MyDependentNode.Phase = EEventLoadNode::Export_StartIO;
					PrereqisiteNode.Phase = EEventLoadNode::ImportOrExport_Create;
					for (int32 Index = Export.CreateBeforeSerializationDependencies; Index > 0; Index--)
					{
						FPackageIndex Dep = Linker->PreloadDependencies[RunningIndex++];
						check(!Dep.IsNull());
						PrereqisiteNode.ImportOrExportIndex = Dep;
						// don't request IO for this export until these are done
						AddArc(PrereqisiteNode, MyDependentNode);
					}

					MyDependentNode.Phase = EEventLoadNode::ImportOrExport_Create;
					PrereqisiteNode.Phase = EEventLoadNode::ImportOrExport_Serialize;
					for (int32 Index = Export.SerializationBeforeCreateDependencies; Index > 0; Index--)
					{
						FPackageIndex Dep = Linker->PreloadDependencies[RunningIndex++];
						check(!Dep.IsNull());
						PrereqisiteNode.ImportOrExportIndex = Dep;
						// can't create this export until these things are serialized
						AddArc(PrereqisiteNode, MyDependentNode);
					}

					MyDependentNode.Phase = EEventLoadNode::ImportOrExport_Create;
					PrereqisiteNode.Phase = EEventLoadNode::ImportOrExport_Create;
					for (int32 Index = Export.CreateBeforeCreateDependencies; Index > 0; Index--)
					{
						FPackageIndex Dep = Linker->PreloadDependencies[RunningIndex++];
						check(!Dep.IsNull());
						PrereqisiteNode.ImportOrExportIndex = Dep;
						// can't create this export until these things are created
						AddArc(PrereqisiteNode, MyDependentNode);
					}

				}
			}
			else
			{
				Export.bExportLoadFailed = true;
			}
		}
		DoneAddingPrerequistesFireIfNone(EEventLoadNode::ImportOrExport_Create, FPackageIndex::FromExport(LocalExportIndex));
	}

	return ExportIndex == Linker->ExportMap.Num() ? EAsyncPackageState::Complete : EAsyncPackageState::TimeOut;
}

void FAsyncPackage::Event_ProcessImportsAndExports()
{
	LLM_SCOPE(ELLMTag::AsyncLoading);

	if (bAllExportsSerialized)
	{
		// we can sometimes get a stray event here caused by the completion of an import that no export was waiting for
		check(!AnyImportsAndExportWorkOutstanding());
		return;
	}
	check(AsyncPackageLoadingState == EAsyncPackageLoadingState::ProcessNewImportsAndExports);
	{
		FScopedAsyncPackageEvent Scope(this);
		ProcessImportsAndExports_Event();
		bProcessImportsAndExportsInFlight = false;
		ConditionalQueueProcessImportsAndExports(true);
	}
	check(AsyncPackageLoadingState == EAsyncPackageLoadingState::ProcessNewImportsAndExports);
}

void FAsyncPackage::LinkImport(int32 LocalImportIndex)
{
	check(LocalImportIndex >= 0 && LocalImportIndex < Linker->ImportMap.Num());
	FObjectImport& Import = Linker->ImportMap[LocalImportIndex];
	if (!Import.XObject && !Import.bImportFailed)
	{
		FScopedAddObjectreference OnExitAddReference(*this, Import.XObject);
		if (Linker->GetAsyncLoader())
		{
			Linker->GetAsyncLoader()->LogItem(TEXT("LinkImport"));
		}
		if (Import.SourceLinker)
		{
			Import.XObject = Import.SourceLinker->ExportMap[Import.SourceIndex].Object;
		}
		else
		{
			// this block becomes active when a package completely finishes before we setup our import arcs.

			FPackageIndex OuterMostIndex = FPackageIndex::FromImport(LocalImportIndex);
			while (true)
			{
				check(!OuterMostIndex.IsNull() && OuterMostIndex.IsImport());
				FObjectImport& OuterMostImport = Linker->Imp(OuterMostIndex);
				if (OuterMostImport.bImportFailed)
				{
					Import.bImportFailed = true;
					return;
				}

				if (OuterMostImport.OuterIndex.IsNull() || OuterMostImport.HasPackageName())
				{
					break;
				}
				OuterMostIndex = OuterMostImport.OuterIndex;
			}
			FObjectImport& OuterMostImport = Linker->Imp(OuterMostIndex);
			UPackage* ImportPackage = OuterMostImport.XObject ? OuterMostImport.XObject->GetOutermost() : nullptr; // these were filled in a previous step

			if (!ImportPackage)
			{
				Import.bImportFailed = true;
				UE_CLOG(!FLinkerLoad::IsKnownMissingPackage(OuterMostImport.ObjectName), LogStreaming, Error, TEXT("Missing native package (%s) for import of %s in %s."), *OuterMostImport.ObjectName.ToString(), *Import.ObjectName.ToString(), *Desc.PackagePath.GetDebugName());
			}
			else
			{
				if (&OuterMostImport == &Import)
				{
					check(0); // we should not be here because package imports are already filled in
				}
				else
				{
					UPackage* ClassPackage = FindObjectFast<UPackage>(NULL, Import.ClassPackage, false);
					if (ClassPackage)
					{
						UClass*	FindClass = FindObjectFast<UClass>(ClassPackage, Import.ClassName, false);
						if (FindClass)
						{
							UObject* Outer = ImportPackage;
							if (OuterMostIndex != Import.OuterIndex)
							{
								if (Import.OuterIndex.IsImport())
								{
									FObjectImport& OuterImport = Linker->Imp(Import.OuterIndex);
									LinkImport(Import.OuterIndex.ToImport());
									if (OuterImport.bImportFailed)
									{
										Import.bImportFailed = true;
										return;
									}
									Outer = OuterImport.XObject;
									UE_CLOG(!Outer, LogStreaming, Fatal, TEXT("Missing outer for import of (%s): %s in %s was not found, but the package exists."), *Desc.PackagePath.GetDebugName(), *OuterImport.ObjectName.ToString(), *ImportPackage->GetFullName());
								}
							}
							//@todo FH: if we change how StaticFindObjectFast works with external package we will need to change this
							Import.XObject = FLinkerLoad::FindImportFast(FindClass, Outer, Import.ObjectName);
							UE_CLOG(!Import.XObject, LogStreaming, Fatal, TEXT("Missing import of (%s): %s in %s was not found, but the package exists."), *Desc.PackagePath.GetDebugName(), *Import.ObjectName.ToString(), *ImportPackage->GetFullName());
						}
					}
				}
			}
		}
	}
}

void FAsyncPackage::DumpDependencies(const TCHAR* Label, UObject* Obj)
{
	UE_LOG(LogStreaming, Error, TEXT("****DumpDependencies [%s]:"), Label);
	if (!Obj)
	{
		UE_LOG(LogStreaming, Error, TEXT("    Obj is nullptr"));
		return;
	}
	UE_LOG(LogStreaming, Error, TEXT("    Obj is %s"), *Obj->GetFullName());
	UPackage* Package = Obj->GetOutermost();
	if (!Package->GetLinker())
	{
		UE_LOG(LogStreaming, Error, TEXT("    %s has no linker"), *Package->GetFullName());
	}
	else
	{
		for (int32 LocalExportIndex = 0; LocalExportIndex < Package->GetLinker()->ExportMap.Num(); LocalExportIndex++)
		{
			FObjectExport& Export = Package->GetLinker()->ExportMap[LocalExportIndex];
			if (Export.Object == Obj || Export.Object == nullptr)
			{
				if (Export.ObjectName == Obj->GetFName())
				{
					DumpDependencies(TEXT(""), Package->GetLinker(), FPackageIndex::FromExport(LocalExportIndex));
				}
			}
		}
	}
}

void FAsyncPackage::DumpDependencies(const TCHAR* Label, FLinkerLoad* DumpLinker, FPackageIndex DumpExportIndex)
{
	FObjectExport& Export = DumpLinker->Exp(DumpExportIndex);
	if (Label[0])
	{
		UE_LOG(LogStreaming, Error, TEXT("****DumpDependencies [%s]:"), Label);
	}
	UE_LOG(LogStreaming, Error, TEXT("    Export %d %s"), DumpExportIndex.ForDebugging(), *DumpLinker->GetPathName(DumpExportIndex));
	UE_LOG(LogStreaming, Error, TEXT("    Linker is %s"), *DumpLinker->GetArchiveName());


	auto PrintDep = [DumpLinker](const TCHAR* DepLabel, FPackageIndex Dep)
	{
		if (Dep.IsNull())
		{
			UE_LOG(LogStreaming, Error, TEXT("        Dep %s null"), DepLabel);
		}
		else if (Dep.IsImport())
		{
			UE_LOG(LogStreaming, Error, TEXT("        Dep %s Import %5d   %s"), DepLabel, Dep.ToImport(), *DumpLinker->GetPathName(Dep));
		}
		else
		{
			UE_LOG(LogStreaming, Error, TEXT("        Dep %s Export %5d    %s     (class %s)"), DepLabel, Dep.ToExport(),
				*DumpLinker->GetPathName(Dep),
				DumpLinker->Exp(Dep).ClassIndex.IsNull() ? TEXT("null") : *DumpLinker->ImpExp(DumpLinker->Exp(Dep).ClassIndex).ObjectName.ToString()
				);
		}
	};

	int32 RunningIndex = Export.FirstExportDependency;
	if (RunningIndex >= 0)
	{
		for (int32 Index = Export.SerializationBeforeSerializationDependencies; Index > 0; Index--)
		{
			FPackageIndex Dep = DumpLinker->PreloadDependencies[RunningIndex++];
			PrintDep(TEXT("S_BEFORE_S"), Dep);
		}

		for (int32 Index = Export.CreateBeforeSerializationDependencies; Index > 0; Index--)
		{
			FPackageIndex Dep = DumpLinker->PreloadDependencies[RunningIndex++];
			PrintDep(TEXT("C_BEFORE_S"), Dep);
		}

		for (int32 Index = Export.SerializationBeforeCreateDependencies; Index > 0; Index--)
		{
			FPackageIndex Dep = DumpLinker->PreloadDependencies[RunningIndex++];
			PrintDep(TEXT("S_BEFORE_C"), Dep);
		}

		for (int32 Index = Export.CreateBeforeCreateDependencies; Index > 0; Index--)
		{
			FPackageIndex Dep = DumpLinker->PreloadDependencies[RunningIndex++];
			PrintDep(TEXT("C_BEFORE_C"), Dep);
		}
	}

}


UObject* FAsyncPackage::EventDrivenIndexToObject(FPackageIndex Index, bool bCheckSerialized, FPackageIndex DumpIndex)
{
	UObject* Result = nullptr;
	if (Index.IsNull())
	{
		return Result;
	}
	if (Index.IsExport())
	{
		Result = Linker->Exp(Index).Object;
	}
	else if (Index.IsImport())
	{
		Result = Linker->Imp(Index).XObject;
	}
	if (!Result)
	{
		FEventLoadNodePtr MyDependentNode;
		MyDependentNode.WaitingPackage = FCheckedWeakAsyncPackagePtr(this); //unnecessary
		MyDependentNode.ImportOrExportIndex = Index;
		MyDependentNode.Phase = EEventLoadNode::ImportOrExport_Create;
		if (EventNodeArray.GetNode(MyDependentNode, false).bAddedToGraph || !EventNodeArray.GetNode(MyDependentNode, false).bFired)
		{
			FUObjectSerializeContext* LoadContext = GetSerializeContext();
			UClass* SerClass = Cast<UClass>(LoadContext->SerializedObject);
			if (!SerClass || Linker->ImpExp(Index).ObjectName != SerClass->GetDefaultObjectName())
			{
				DumpDependencies(TEXT("Dependencies"), LoadContext->SerializedObject);
				UE_LOG(LogStreaming, Fatal, TEXT("Missing Dependency, request for %s but it was still waiting for creation."), *Linker->GetPathName(Index));
			}
		}
	}
	if (bCheckSerialized && !IsFullyLoadedObj(Result))
	{
		FEventLoadNodePtr MyDependentNode;
		MyDependentNode.WaitingPackage = FCheckedWeakAsyncPackagePtr(this); //unnecessary
		MyDependentNode.ImportOrExportIndex = Index;
		MyDependentNode.Phase = EEventLoadNode::ImportOrExport_Serialize;

		if (DumpIndex.IsNull())
		{
			FUObjectSerializeContext* LoadContext = GetSerializeContext();
			DumpDependencies(TEXT("Dependencies"), LoadContext->SerializedObject);
		}
		else
		{
			DumpDependencies(TEXT("Dependencies"), Linker, DumpIndex);
		}

		if (!Result)
		{
			UE_LOG(LogStreaming, Error, TEXT("Missing Dependency, request for %s but it hasn't been created yet."), *Linker->GetPathName(Index));
		}
		else if (EventNodeArray.GetNode(MyDependentNode, false).bAddedToGraph || !EventNodeArray.GetNode(MyDependentNode, false).bFired)
		{
			UE_LOG(LogStreaming, Fatal, TEXT("Missing Dependency, request for %s but it was still waiting for serialization."), *Linker->GetPathName(Index));
		}
		else
		{
			UE_LOG(LogStreaming, Fatal, TEXT("Missing Dependency, request for %s but it was still has RF_NeedLoad."), *Linker->GetPathName(Index));
		}
	}
	if (Result)
	{
		UE_CLOG(Result->IsUnreachable(), LogStreaming, Fatal, TEXT("Returning an object  (%s) from EventDrivenIndexToObject that is unreachable."), *Result->GetFullName());
		checkSlow(ReferencedObjects.Contains(Result));
	}
	return Result;
}


void FAsyncPackage::EventDrivenCreateExport(int32 LocalExportIndex)
{
	LLM_SCOPE(ELLMTag::AsyncLoading);
	SCOPED_LOADTIMER(Package_CreateExports);

	FObjectExport& Export = Linker->ExportMap[LocalExportIndex];

	TRACE_LOADTIME_CREATE_EXPORT_SCOPE(this, &Export.Object);

	// Check whether we already loaded the object and if not whether the context flags allow loading it.
	//check(!Export.Object || Export.Object->HasAnyFlags(RF_ClassDefaultObject)); // we should not have this yet, unless it is a CDO
	check(!Export.Object); // we should not have this yet
	if (!Export.Object && !Export.bExportLoadFailed)
	{
		const UClass* Class = CastEventDrivenIndexToObject<UClass>(Export.ClassIndex, false);
		LLM_SCOPE_DYNAMIC_STAT_OBJECTPATH(GetLinkerRoot(), ELLMTagSet::Assets);
		LLM_SCOPE_DYNAMIC_STAT_OBJECTPATH(Class, ELLMTagSet::AssetClasses);
		UE_TRACE_METADATA_SCOPE_ASSET_FNAME(Export.ObjectName, Class->GetFName(), GetLinkerRoot()->GetFName());

		FUObjectSerializeContext* LoadContext = GetSerializeContext();
		FScopedAddObjectreference OnExitAddReference(*this, Export.Object);

		if (!Linker->FilterExport(Export)) // for some acceptable position, it was not "not for" 
		{
			SCOPED_ACCUM_LOADTIME(Construction, StaticGetNativeClassName(CastEventDrivenIndexToObject<UClass>(Export.ClassIndex, false)));

			if (Linker->GetAsyncLoader())
			{
				Linker->GetAsyncLoader()->LogItem(TEXT("EventDrivenCreateExport"), Export.SerialOffset, Export.SerialSize);
			}
			LastTypeOfWorkPerformed = TEXT("EventDrivenCreateExport");
			LastObjectWorkWasPerformedOn = nullptr;
			check(Export.ObjectName != NAME_None || !(Export.ObjectFlags&RF_Public));
			check(LoadContext->HasStartedLoading());
			{
				UClass* LoadClass = nullptr;
				if (Export.ClassIndex.IsNull())
				{
					LoadClass = UClass::StaticClass();
				}
				else
				{
					LoadClass = CastEventDrivenIndexToObject<UClass>(Export.ClassIndex, true, FPackageIndex::FromExport(LocalExportIndex));
				}
				if (!LoadClass)
				{
					UE_LOG(LogStreaming, Error, TEXT("Could not find class %s to create %s"), *Linker->ImpExp(Export.ClassIndex).ObjectName.ToString(), *Export.ObjectName.ToString());
					Export.bExportLoadFailed = true;
					return;
				}
				UObject* ThisParent = nullptr;
				if (!Export.OuterIndex.IsNull())
				{
					ThisParent = EventDrivenIndexToObject(Export.OuterIndex, false, FPackageIndex::FromExport(LocalExportIndex));
				}
				else if (Export.bForcedExport)
				{
					// see FLinkerLoad::CreateExport, there may be some more we can do here
					check(!Export.bForcedExport); // this is leftover from seekfree loading I think
				}
				else
				{
					check(LinkerRoot);
					ThisParent = LinkerRoot;
				}
				check(!dynamic_cast<UObjectRedirector*>(ThisParent));
				if (!ThisParent)
				{
					UE_LOG(LogStreaming, Error, TEXT("Could not find outer %s to create %s"), *Linker->ImpExp(Export.OuterIndex).ObjectName.ToString(), *Export.ObjectName.ToString());
					Export.bExportLoadFailed = true;
					return;
				}

				// Try to find existing object first in case we're a forced export to be able to reconcile. Also do it for the
				// case of async loading as we cannot in-place replace objects.

				UObject* ActualObjectWithTheName = StaticFindObjectFastInternal(NULL, ThisParent, Export.ObjectName, true);

				// Always attempt to find object in memory first
				if (ActualObjectWithTheName && (ActualObjectWithTheName->GetClass() == LoadClass))
				{
					Export.Object = ActualObjectWithTheName;
				}

				// Object is found in memory.
				if (Export.Object)
				{
					// Mark that we need to dissociate forced exports later on if we are a forced export.
					if (Export.bForcedExport)
					{
						// see FLinkerLoad::CreateExport, there may be some more we can do here
						check(!Export.bForcedExport); // this is leftover from seekfree loading I think
					}
					// Associate linker with object to avoid detachment mismatches.
					else
					{
						Export.Object->SetLinker(Linker, LocalExportIndex);

						// If this object was allocated but never loaded (components created by a constructor, CDOs, etc) make sure it gets loaded
						// Do this for all subobjects created in the native constructor.
						if (!Export.Object->HasAnyFlags(RF_LoadCompleted))
						{
							UE_LOG(LogStreaming, VeryVerbose, TEXT("Note2: %s was constructed during load and is an export and so needs loading."), *Export.Object->GetFullName());
							UE_CLOG(!Export.Object->HasAllFlags(RF_WillBeLoaded), LogStreaming, Fatal, TEXT("%s was found in memory and is an export but does not have all load flags."), *Export.Object->GetFullName());
							if(Export.Object->HasAnyFlags(RF_ClassDefaultObject))
							{
								// never call PostLoadSubobjects on class default objects, this matches the behavior of the old linker where
								// StaticAllocateObject prevents setting of RF_NeedPostLoad and RF_NeedPostLoadSubobjects, but FLinkerLoad::Preload
										// assigns RF_NeedPostLoad for blueprint CDOs:
								Export.Object->SetFlags(RF_NeedLoad | RF_NeedPostLoad | RF_WasLoaded);
							}
							else
							{
								Export.Object->SetFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_WasLoaded);
							}
							Export.Object->ClearFlags(RF_WillBeLoaded);
						}
					}
				}
				else
				{
					if (ActualObjectWithTheName && !ActualObjectWithTheName->GetClass()->IsChildOf(LoadClass))
					{
						UE_LOG(LogLinker, Error, TEXT("Failed import: class '%s' name '%s' outer '%s'. There is another object (of '%s' class) at the path."),
							*LoadClass->GetName(), *Export.ObjectName.ToString(), *ThisParent->GetName(), *ActualObjectWithTheName->GetClass()->GetName());
						Export.bExportLoadFailed = true; // I am not sure if this is an actual fail or not...looked like it in the original code
						return;
					}

					// Find the Archetype object for the one we are loading.
					check(!Export.TemplateIndex.IsNull());
					UObject* Template = EventDrivenIndexToObject(Export.TemplateIndex, true, FPackageIndex::FromExport(LocalExportIndex));
					if (!Template)
					{
						UE_LOG(LogStreaming, Error, TEXT("Cannot construct %s in %s because we could not find its template %s"), *Export.ObjectName.ToString(), *Linker->GetArchiveName(), *Linker->GetImportPathName(Export.TemplateIndex));
						Export.bExportLoadFailed = true;
						return;
					}
					// we also need to ensure that the template has set up any instances
					Template->ConditionalPostLoadSubobjects();


					check(!GVerifyObjectReferencesOnly); // not supported with the event driven loader
					// Create the export object, marking it with the appropriate flags to
					// indicate that the object's data still needs to be loaded.
					EObjectFlags ObjectLoadFlags = Export.ObjectFlags;
					ObjectLoadFlags = EObjectFlags(ObjectLoadFlags | RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_WasLoaded);

					FName NewName = Export.ObjectName;

					// If we are about to create a CDO, we need to ensure that all parent sub-objects are loaded
					// to get default value initialization to work.
		#if DO_CHECK
					if ((ObjectLoadFlags & RF_ClassDefaultObject) != 0)
					{
						UClass* SuperClass = LoadClass->GetSuperClass();
						UObject* SuperCDO = SuperClass ? SuperClass->GetDefaultObject() : nullptr;
						check(!SuperCDO || Template == SuperCDO); // the template for a CDO is the CDO of the super
						if (SuperClass && !SuperClass->IsNative())
						{
							check(SuperCDO);
							if (SuperClass->HasAnyFlags(RF_NeedLoad))
							{
								UE_LOG(LogStreaming, Fatal, TEXT("Super %s had RF_NeedLoad while creating %s"), *SuperClass->GetFullName(), *Export.ObjectName.ToString());
								Export.bExportLoadFailed = true;
								return;
							}
							if (SuperCDO->HasAnyFlags(RF_NeedLoad))
							{
								UE_LOG(LogStreaming, Fatal, TEXT("Super CDO %s had RF_NeedLoad while creating %s"), *SuperCDO->GetFullName(), *Export.ObjectName.ToString());
								Export.bExportLoadFailed = true;
								return;
							}
							TArray<UObject*> SuperSubObjects;
							GetObjectsWithOuter(SuperCDO, SuperSubObjects, /*bIncludeNestedObjects=*/ false, /*ExclusionFlags=*/ RF_NoFlags, /*InternalExclusionFlags=*/ EInternalObjectFlags::Native);

							for (UObject* SubObject : SuperSubObjects)
							{
								if (SubObject->HasAnyFlags(RF_NeedLoad))
								{
									UE_LOG(LogStreaming, Fatal, TEXT("Super CDO subobject %s had RF_NeedLoad while creating %s"), *SubObject->GetFullName(), *Export.ObjectName.ToString());
									Export.bExportLoadFailed = true;
									return;
								}
							}
						}
						else
						{
							check(Template->IsA(LoadClass));
						}
					}
		#endif
					if (LoadClass->HasAnyFlags(RF_NeedLoad))
					{
						UE_LOG(LogStreaming, Fatal, TEXT("LoadClass %s had RF_NeedLoad while creating %s"), *LoadClass->GetFullName(), *Export.ObjectName.ToString());
						Export.bExportLoadFailed = true;
						return;
					}
					{
						UObject* LoadCDO = LoadClass->GetDefaultObject();
						if (LoadCDO->HasAnyFlags(RF_NeedLoad))
						{
							UE_LOG(LogStreaming, Fatal, TEXT("Class CDO %s had RF_NeedLoad while creating %s"), *LoadCDO->GetFullName(), *Export.ObjectName.ToString());
							Export.bExportLoadFailed = true;
							return;
						}
					}
					if (Template->HasAnyFlags(RF_NeedLoad))
					{
						UE_LOG(LogStreaming, Fatal, TEXT("Template %s had RF_NeedLoad while creating %s"), *Template->GetFullName(), *Export.ObjectName.ToString());
						Export.bExportLoadFailed = true;
						return;
					}

					FStaticConstructObjectParameters Params(LoadClass);
					Params.Outer = ThisParent;
					Params.Name = NewName;
					Params.SetFlags = ObjectLoadFlags;
					Params.Template = Template;
					Params.bAssumeTemplateIsArchetype = true;
					// if our outer is actually an import, then the package we are an export of is not in our outer chain, set our package in that case
					Params.ExternalPackage = Export.OuterIndex.IsImport() ? LinkerRoot : nullptr;
					Export.Object = StaticConstructObject_Internal(Params);

					if (GIsInitialLoad || GUObjectArray.IsOpenForDisregardForGC())
					{
						Export.Object->AddToRoot();
					}
					Export.Object->SetLinker(Linker, LocalExportIndex);
					check(Export.Object->GetClass() == LoadClass);
					check(NewName == Export.ObjectName);
				}
			}
		}
	}
	if (!Export.Object)
	{
		Export.bExportLoadFailed = true;
	}
	LastObjectWorkWasPerformedOn = Export.Object;
	check(Export.Object || Export.bExportLoadFailed);
}

static FPackageIndex FindExportFromObject(FLinkerLoad* Linker, UObject *Object)
{
	check(Linker && Linker->AsyncRoot && static_cast<FAsyncPackage*>(Linker->AsyncRoot)->ObjectNameWithOuterToExport.Num());
	FPackageIndex Result;
	UObject* Outer = Object->GetOuter();
	if (Outer)
	{
		FPackageIndex OuterIndex = FindExportFromObject(Linker, Outer);
		FPackageIndex* PotentialExport = static_cast<FAsyncPackage*>(Linker->AsyncRoot)->ObjectNameWithOuterToExport.Find(TPair<FName, FPackageIndex>(Object->GetFName(), OuterIndex));
		if (PotentialExport)
		{
			Result = *PotentialExport;
		}
		// The object might be found in the linker import table instead
		else
		{
			for (int i = 0; i < Linker->ImportMap.Num(); ++i)
			{
				const FObjectImport& Import = Linker->ImportMap[i];
				if (Import.XObject == Object ||
					(Import.ObjectName == Object->GetFName() &&
					 Import.ClassName == Object->GetClass()->GetFName() &&
					 Linker->ImpExp(Import.OuterIndex).ObjectName == Object->GetOuter()->GetFName()))
				{
					Result = FPackageIndex::FromImport(i);
					break;
				}
			}
		}
	}
	return Result;
}

void FAsyncPackage::MarkNewObjectForLoadIfItIsAnExport(UObject *Object)
{
	if (!Object->HasAnyFlags(RF_WillBeLoaded | RF_LoadCompleted | RF_NeedLoad))
	{
		FPackageIndex MaybeExportIndex = FindExportFromObject(Linker, Object);
		if (MaybeExportIndex.IsExport())
		{
			UE_LOG(LogStreaming, VeryVerbose, TEXT("Note: %s was constructed during load and is an export and so needs loading."), *Object->GetFullName());
			Object->SetFlags(RF_WillBeLoaded);
		}
	}
}

void FAsyncPackage::EventDrivenSerializeExport(int32 LocalExportIndex)
{
	LLM_SCOPE(ELLMTag::UObject);
	SCOPED_LOADTIMER(Package_PreLoadObjects);

	FObjectExport& Export = Linker->ExportMap[LocalExportIndex];

	UObject* Object = Export.Object;
	if (Object && Object->HasAnyFlags(RF_NeedLoad))
	{
		const UClass* Class = CastEventDrivenIndexToObject<UClass>(Export.ClassIndex, false);
		LLM_SCOPE_DYNAMIC_STAT_OBJECTPATH(GetLinkerRoot(), ELLMTagSet::Assets);
		LLM_SCOPE_DYNAMIC_STAT_OBJECTPATH(Class, ELLMTagSet::AssetClasses);
		UE_TRACE_METADATA_SCOPE_ASSET_FNAME(Export.ObjectName, Class->GetFName(), GetLinkerRoot()->GetFName());

		Linker->GetAsyncLoader()->LogItem(TEXT("EventDrivenSerializeExport"), Export.SerialOffset, Export.SerialSize);

		LastTypeOfWorkPerformed = TEXT("EventDrivenSerializeExport");
		LastObjectWorkWasPerformedOn = Object;
		check(Object->GetLinker() == Linker);
		check(Object->GetLinkerIndex() == LocalExportIndex);

		// If this is a struct, make sure that its parent struct is completely loaded
		if (UStruct* Struct = dynamic_cast<UStruct*>(Object))
		{
			UStruct* SuperStruct = nullptr;
			if (!Export.SuperIndex.IsNull())
			{
				SuperStruct = CastEventDrivenIndexToObject<UStruct>(Export.SuperIndex, true, FPackageIndex::FromExport(LocalExportIndex));
				if (!SuperStruct)
				{
					// see FLinkerLoad::CreateExport, there may be some more we can do here
					UE_LOG(LogStreaming, Fatal, TEXT("Could not find SuperStruct %s to create %s"), *Linker->ImpExp(Export.SuperIndex).ObjectName.ToString(), *Export.ObjectName.ToString());
					Export.bExportLoadFailed = true;
					return;
				}
			}
			if (SuperStruct)
			{
				Struct->SetSuperStruct(SuperStruct);
				if (UClass* ClassObject = dynamic_cast<UClass*>(Object))
				{
					ClassObject->Bind();
				}
			}
		}
		check(Export.SerialOffset >= CurrentBlockOffset && Export.SerialOffset + Export.SerialSize <= CurrentBlockOffset + CurrentBlockBytes);

		FAsyncArchive* AsyncLoader = Linker->GetAsyncLoader();
		check(AsyncLoader);

		const int64 SavedPos = AsyncLoader->Tell();
		AsyncLoader->Seek(Export.SerialOffset);

		Object->ClearFlags(RF_NeedLoad);

		TRACE_LOADTIME_SERIALIZE_EXPORT_SCOPE(Object, Export.SerialSize);

		FUObjectSerializeContext* LoadContext = GetSerializeContext();
		UObject* PrevSerializedObject = LoadContext->SerializedObject;
		LoadContext->SerializedObject = Object;
		Linker->bForceSimpleIndexToObject = true;

		// Find the Archetype object for the one we are loading. This is piped to GetArchetypeFromLoader
		check(!Export.TemplateIndex.IsNull());
		UObject* Template = EventDrivenIndexToObject(Export.TemplateIndex, true, FPackageIndex::FromExport(LocalExportIndex));
		check(Template);

		check(!Linker->TemplateForGetArchetypeFromLoader);
		Linker->TemplateForGetArchetypeFromLoader = Template;

		{
			ACCUM_LOADTIMECOUNT_STAT(StaticGetNativeClassName(Object->GetClass()).ToString());
			SCOPED_ACCUM_LOADTIME_STAT(StaticGetNativeClassName(Object->GetClass()).ToString());
			SCOPED_ACCUM_LOADTIME(Serialize, StaticGetNativeClassName(Object->GetClass()));
			
			FLinkerExportArchive ExportArchive(*Linker, Export.SerialOffset, Export.SerialSize);
			ExportArchive.SetFilterEditorOnly(Linker->IsFilterEditorOnly());
		
			if (Object->HasAnyFlags(RF_ClassDefaultObject))
			{
				Object->GetClass()->SerializeDefaultObject(Object, ExportArchive);
			}
			else
			{
				UE_SCOPED_IO_ACTIVITY(*Object->GetName());
				UE_SERIALIZE_ACCCESS_SCOPE(Object);
				Object->Serialize(ExportArchive);
			}
		}
		check(Linker->TemplateForGetArchetypeFromLoader == Template);
		Linker->TemplateForGetArchetypeFromLoader = nullptr;

		Object->SetFlags(RF_LoadCompleted);
		LoadContext->SerializedObject = PrevSerializedObject;
		Linker->bForceSimpleIndexToObject = false;

		if (AsyncLoader->Tell() - Export.SerialOffset != Export.SerialSize)
		{
			if (Object->GetClass()->HasAnyClassFlags(CLASS_Deprecated))
			{
				UE_LOG(LogStreaming, Warning, TEXT("%s"), *FString::Printf(TEXT("%s: Serial size mismatch: Got %d, Expected %d"), *Object->GetFullName(), (int32)(AsyncLoader->Tell() - Export.SerialOffset), Export.SerialSize));
			}
			else
			{
				UE_LOG(LogStreaming, Fatal, TEXT("%s"), *FString::Printf(TEXT("%s: Serial size mismatch: Got %d, Expected %d"), *Object->GetFullName(), (int32)(AsyncLoader->Tell() - Export.SerialOffset), Export.SerialSize));
			}
		}

		AsyncLoader->Seek(SavedPos);
#if DO_CHECK
		if (Object->HasAnyFlags(RF_ClassDefaultObject) && Object->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
		{
			check(Object->HasAllFlags(RF_NeedPostLoad | RF_WasLoaded));
			//Object->SetFlags(RF_NeedPostLoad | RF_WasLoaded);
		}
#endif
	}

	// push stats so that we don't overflow number of tags per thread during blocking loading
	LLM_PUSH_STATS_FOR_ASSET_TAGS();
}

#define MAX_EXPORT_PRECACHE_BLOCK (1024*1024)
#define MAX_EXPORT_COUNT_PRECACHE (20)
#define MAX_EXPORT_ALLOWED_SKIP (48*1024)

void FAsyncPackage::StartPrecacheRequest()
{
	SCOPED_LOADTIMER(StartPrecacheRequests);
	int32 LocalExportIndex = -1;
	while (true)
	{
		ExportsThatCanHaveIOStarted.HeapPop(LocalExportIndex, EAllowShrinking::No);
		FObjectExport& Export = Linker->ExportMap[LocalExportIndex];
		bool bReady = false;
		if (Export.Object && Export.Object->HasAnyFlags(RF_NeedLoad))
		{
			// look for an existing request that will cover it
			if (Export.SerialOffset >= CurrentBlockOffset && Export.SerialOffset + Export.SerialSize <= CurrentBlockOffset + CurrentBlockBytes)
			{
				// ready right now
				bReady = true;
			}
			else
			{
				IAsyncReadRequest* Precache = ExportIndexToPrecacheRequest.FindRef(LocalExportIndex);
				if (Precache)
				{
					// it is in an outstanding request
					FExportIORequest* Req = PrecacheRequests.Find(Precache);
					check(Req);
					check(Export.SerialOffset >= Req->Offset && Export.SerialOffset + Export.SerialSize <= Req->Offset + Req->BytesToRead);
					Req->ExportsToRead.Add(LocalExportIndex);
				}
				else
				{
					break;
				}
			}
		}
		else
		{
			bReady = true;
		}
		if (bReady)
		{
			RemoveNode(EEventLoadNode::Export_StartIO, FPackageIndex::FromExport(LocalExportIndex));
		}
		if (!ExportsThatCanHaveIOStarted.Num())
		{
			return;
		}
	}
	// LocalExportIndex will start a new precache request
	FObjectExport& Export = Linker->ExportMap[LocalExportIndex];

	FExportIORequest NewReq;
	NewReq.Offset = Export.SerialOffset;
	NewReq.BytesToRead = Export.SerialSize;
	check(NewReq.BytesToRead > 0 && NewReq.Offset > 0);
	NewReq.ExportsToRead.Add(LocalExportIndex);

	int32 LastExportIndex = LocalExportIndex;
	if (!GRandomizeLoadOrder) // the blow code relies on sorting, which doesn't happen when we use a random load order; we will load export-by-export with no export fusion.
	{
		while (ExportsThatCanHaveIOStarted.Num() && NewReq.BytesToRead < MAX_EXPORT_PRECACHE_BLOCK && LastExportIndex - LocalExportIndex <= MAX_EXPORT_COUNT_PRECACHE)
		{
			int32 MaybeLastExportIndex = ExportsThatCanHaveIOStarted[0];
			check(MaybeLastExportIndex > LastExportIndex); 

				FObjectExport& LaterExport = Linker->ExportMap[MaybeLastExportIndex];
			if (LaterExport.SerialOffset >= CurrentBlockOffset && LaterExport.SerialOffset + LaterExport.SerialSize <= CurrentBlockOffset + CurrentBlockBytes)
			{
				// ready right now, release it and remove it from the queue
				int32 TempExportIndex = -1;
				ExportsThatCanHaveIOStarted.HeapPop(TempExportIndex, EAllowShrinking::No);
				check(TempExportIndex == MaybeLastExportIndex);
				RemoveNode(EEventLoadNode::Export_StartIO, FPackageIndex::FromExport(MaybeLastExportIndex));
				break;
			}

			int64 Gap = LaterExport.SerialOffset - (NewReq.Offset + NewReq.BytesToRead);
			check(Gap >= 0);

			if (Gap > MAX_EXPORT_ALLOWED_SKIP || NewReq.BytesToRead + LaterExport.SerialSize > MAX_EXPORT_PRECACHE_BLOCK)
			{
				break; // this is too big of a gap or we already have a big enough read request
			}
			bool bAlreadyCovered = false;
			for (int32 Index = LastExportIndex + 1; Index <= MaybeLastExportIndex; Index++)
			{
				if (ExportIndexToPrecacheRequest.Contains(Index))
				{
					bAlreadyCovered = true;
					break;
				}
			}
			if (bAlreadyCovered)
			{
				break;
			}
			// this export is good to merge into the request
			ExportsThatCanHaveIOStarted.HeapPop(LastExportIndex, EAllowShrinking::No);
			check(LastExportIndex == MaybeLastExportIndex);
			NewReq.BytesToRead = LaterExport.SerialOffset + LaterExport.SerialSize - NewReq.Offset;
			check(NewReq.BytesToRead > 0);
			NewReq.ExportsToRead.Add(LastExportIndex);
		}
	}
	check(NewReq.ExportsToRead.Num());
	FAsyncArchive* AsyncLoader = Linker->GetAsyncLoader();
	check(AsyncLoader);

	IAsyncReadRequest* Precache = AsyncLoader->MakeEventDrivenPrecacheRequest(NewReq.Offset, NewReq.BytesToRead, AsyncLoadingThread.GetPrecacheHandler().GetCompletionCallback());

	NewReq.FirstExportCovered = LocalExportIndex;
	NewReq.LastExportCovered = LastExportIndex;
	for (int32 Index = NewReq.FirstExportCovered; Index <= NewReq.LastExportCovered; Index++)
	{
		check(!ExportIndexToPrecacheRequest.Contains(Index));
		ExportIndexToPrecacheRequest.Add(Index, Precache);
	}
	check(!PrecacheRequests.Contains(Precache));
	FExportIORequest& RequestInPlace = PrecacheRequests.Add(Precache);
	Swap(RequestInPlace, NewReq);
	AsyncLoadingThread.GetPrecacheHandler().RegisterNewPrecacheRequest(Precache, this);
}

int64 FAsyncPackage::PrecacheRequestReady(IAsyncReadRequest * Read)
{
	ReadyPrecacheRequests.Add(Read);
	int64 Size = PrecacheRequests.FindChecked(Read).BytesToRead;
	ConditionalQueueProcessImportsAndExports();
	return Size;
}

void FAsyncPackage::MakeNextPrecacheRequestCurrent()
{
	SCOPED_LOADTIMER(MakeNextPrecacheRequestCurrent);
	LLM_SCOPE(ELLMTag::AsyncLoading);

	check(ReadyPrecacheRequests.Num());
	IAsyncReadRequest* Read = ReadyPrecacheRequests.Pop(EAllowShrinking::No);
	FExportIORequest& Req = PrecacheRequests.FindChecked(Read);
	CurrentBlockOffset = Req.Offset;
	CurrentBlockBytes = Req.BytesToRead;
	ExportsInThisBlock.Reset();

	AsyncLoadingThread.GetPrecacheHandler().FinishRequest(Req.BytesToRead);

	FAsyncArchive* AsyncLoader = Linker->GetAsyncLoader();
	check(AsyncLoader);
	Read->WaitCompletion();

	bool bReady = AsyncLoader->PrecacheForEvent(Read, CurrentBlockOffset, CurrentBlockBytes);
	UE_CLOG(!bReady, LogStreaming, Warning, TEXT("Precache request should have been hot %s."), *Linker->GetPackagePath().GetDebugName());
	for (int32 Index = Req.FirstExportCovered; Index <= Req.LastExportCovered; Index++)
	{
		verify(ExportIndexToPrecacheRequest.Remove(Index) == 1);
		ExportsInThisBlock.Add(Index);
	}
	for (int32 LocalExportIndex : Req.ExportsToRead)
	{
		RemoveNode(EEventLoadNode::Export_StartIO, FPackageIndex::FromExport(LocalExportIndex));
	}
	PrecacheRequests.Remove(Read);
}

void FAsyncPackage::FlushPrecacheBuffer()
{
	SCOPED_LOADTIMER(FlushPrecacheBuffer);
	CurrentBlockOffset = -1;
	CurrentBlockBytes = -1;
	FAsyncArchive* AsyncLoader = Linker->GetAsyncLoader();
	check(AsyncLoader);
	AsyncLoader->FlushPrecacheBlock();
}

int32 GCurrentExportIndex = -1;

EAsyncPackageState::Type FAsyncPackage::ProcessImportsAndExports_Event()
{
	SCOPED_LOADTIMER(ProcessImportsAndExports_Event);
	check(Linker);
	bool bDidSomething = true;
	int32 LoopIterations = 0;
	while (!IsTimeLimitExceeded() && bDidSomething)
	{
		if ((LoopIterations && GRandomizeLoadOrder) || ++LoopIterations == 20)
		{
			break; // requeue this to give other packages a chance to start IO
		}
		bDidSomething = false;
		if (PrecacheRequests.Num() < GMaxPrecacheRequestsInFlight && ExportsThatCanHaveIOStarted.Num() && CanAddWaitingPackages(AsyncLoadingThread))
		{
			bDidSomething = true;
			StartPrecacheRequest();
			LastTypeOfWorkPerformed = TEXT("ProcessImportsAndExports Start IO");
			LastObjectWorkWasPerformedOn = nullptr;
		}
		if (bDidSomething)
		{
			continue; // check time limit, and lets do the creates and new IO requests before the serialize checks
		}
		if (ImportsThatAreNowCreated.Num())
		{
			bDidSomething = true;
			int32 LocalImportIndex = -1;
			ImportsThatAreNowCreated.HeapPop(LocalImportIndex, EAllowShrinking::No);
			{
				// GC can't run in here
				FGCScopeGuard GCGuard;
				LinkImport(LocalImportIndex);
			}
			RemoveNode(EEventLoadNode::ImportOrExport_Create, FPackageIndex::FromImport(LocalImportIndex));
			LastTypeOfWorkPerformed = TEXT("ProcessImportsAndExports LinkImport");
			LastObjectWorkWasPerformedOn = nullptr;
		}
		if (bDidSomething)
		{
			continue; // check time limitE
		}
		if (ImportsThatAreNowSerialized.Num())
		{
			bDidSomething = true;
			int32 LocalImportIndex = -1;
			ImportsThatAreNowSerialized.HeapPop(LocalImportIndex, EAllowShrinking::No);
			FObjectImport& Import = Linker->ImportMap[LocalImportIndex];
			if (Import.XObject)
			{
				checkf(!Import.XObject->HasAnyFlags(RF_NeedLoad), TEXT("%s had RF_NeedLoad yet it was marked as serialized."), *Import.XObject->GetFullName()); // otherwise is isn't done serializing, is it?
			}
			RemoveNode(EEventLoadNode::ImportOrExport_Serialize, FPackageIndex::FromImport(LocalImportIndex));
			LastTypeOfWorkPerformed = TEXT("ProcessImportsAndExports ImportsThatAreNowSerialized");
			LastObjectWorkWasPerformedOn = nullptr;
		}
		if (bDidSomething)
		{
			continue; // check time limit, and lets do the creates before the serialize checks
		}
		if (ExportsThatCanBeCreated.Num())
		{
			bDidSomething = true;
			int32 LocalExportIndex = -1;
			ExportsThatCanBeCreated.HeapPop(LocalExportIndex, EAllowShrinking::No);
			{
				FGCScopeGuard GCGuard;
				EventDrivenCreateExport(LocalExportIndex);
			}
			RemoveNode(EEventLoadNode::ImportOrExport_Create, FPackageIndex::FromExport(LocalExportIndex));
		}
		if (bDidSomething)
		{
			continue; // check time limit, and lets do the creates before the serialize checks
		}
		if (ExportsThatCanHaveIOStarted.Num() && CanAddWaitingPackages(AsyncLoadingThread))
		{
			bDidSomething = true;
			StartPrecacheRequest();
			LastTypeOfWorkPerformed = TEXT("ProcessImportsAndExports Start IO");
			LastObjectWorkWasPerformedOn = nullptr;
		}
		if (bDidSomething)
		{
			continue; // check time limit, and lets do the creates and new IO requests before the serialize checks
		}
		if (ExportsThatCanBeSerialized.Num())
		{
			bDidSomething = true;
			int32 LocalExportIndex = -1;
			ExportsThatCanBeSerialized.HeapPop(LocalExportIndex, EAllowShrinking::No);

			if (ExportsInThisBlock.Remove(LocalExportIndex) == 1)
			{
				FGCScopeGuard GCGuard;
				GCurrentExportIndex = LocalExportIndex;
				EventDrivenSerializeExport(LocalExportIndex);
				GCurrentExportIndex = -1;
				{
					FObjectExport& Export = Linker->ExportMap[LocalExportIndex];
					UObject* Object = Export.Object;
					check(!Object || !Object->HasAnyFlags(RF_NeedLoad));
				}
			}
			else
			{
				check(!Linker->ExportMap[LocalExportIndex].Object || !Linker->ExportMap[LocalExportIndex].Object->HasAnyFlags(RF_NeedLoad));
			}

			RemoveNode(EEventLoadNode::ImportOrExport_Serialize, FPackageIndex::FromExport(LocalExportIndex));
		}
		if (bDidSomething)
		{
			continue; // This is really important, we want to avoid discarding the current read block at all costs
		}
		check(!ExportsThatCanBeSerialized.Num());
		if (CurrentBlockBytes > 0 && !ExportsInThisBlock.Num())
		{
			// we are completely done with this block, so we should explicitly discard it to save memory
			// this is pretty mediocre because maybe the things left in the list don't need to be load anyway, but it covers the common case of precaching a single thing and precaching a block of things that are all loaded
			FlushPrecacheBuffer();
			LastTypeOfWorkPerformed = TEXT("ProcessImportsAndExports FlushPrecacheBuffer");
			LastObjectWorkWasPerformedOn = nullptr;
		}
		// else we might get a new export in this block, so we might as well hang onto it...though it might be discarded anyway for a new request below

		if (ReadyPrecacheRequests.Num())
		{
			// this generally takes no time, so we don't consider it doing something
			MakeNextPrecacheRequestCurrent();
			LastTypeOfWorkPerformed = TEXT("ProcessImportsAndExports MakeNextPrecacheRequestCurrent");
			LastObjectWorkWasPerformedOn = nullptr;
		}
	}
	return (!bDidSomething) ? EAsyncPackageState::Complete : EAsyncPackageState::TimeOut;
}

void FAsyncPackage::Event_ExportsDone()
{
	Linker->GetAsyncLoader()->LogItem(TEXT("Event_ExportsDone"));
	check(AsyncPackageLoadingState == EAsyncPackageLoadingState::ProcessNewImportsAndExports);
	bAllExportsSerialized = true;
	RemoveNode(EEventLoadNode::Package_ExportsSerialized);
	check(AsyncPackageLoadingState == EAsyncPackageLoadingState::ProcessNewImportsAndExports);
	AsyncPackageLoadingState = EAsyncPackageLoadingState::WaitingForPostLoad;
	check(!AnyImportsAndExportWorkOutstanding());
	FlushPrecacheBuffer();

	ConditionalQueueProcessPostloadWait();

	FWeakAsyncPackagePtr WeakThis(this);
	for (FWeakAsyncPackagePtr NotifyPtr : OtherPackagesWaitingForMeBeforePostload)
	{
		FAsyncPackage* TestPkg(AsyncLoadingThread.GetPackage(NotifyPtr));
		if (TestPkg)
		{
			check(TestPkg != this);
			int32 NumRem = TestPkg->PackagesIAmWaitingForBeforePostload.Remove(WeakThis);
			check(NumRem);
			TestPkg->PackagesIMayBeWaitingForBeforePostload.Add(WeakThis);
			TestPkg->ConditionalQueueProcessPostloadWait();
		}
	}
	OtherPackagesWaitingForMeBeforePostload.Empty();
}


void FAsyncPackage::Event_ProcessPostloadWait()
{
	Linker->GetAsyncLoader()->LogItem(TEXT("Event_ProcessPostloadWait"));
	check(AsyncPackageLoadingState == EAsyncPackageLoadingState::WaitingForPostLoad);
	check(bAllExportsSerialized && !OtherPackagesWaitingForMeBeforePostload.Num());
	bProcessPostloadWaitInFlight = false;

	FWeakAsyncPackagePtr WeakThis(this);

	check(!PackagesIAmWaitingForBeforePostload.Num());
	TSet<FWeakAsyncPackagePtr> AlreadyHandled;
	AlreadyHandled.Add(WeakThis); // we never consider ourself a dependent

	// pretty dang complicated incremental algorithm to determine when all dependent packages are loaded....so we can postload our objects

	// remove junk from the wait list and look for anything that isn't ready
	for (auto It = PackagesIMayBeWaitingForBeforePostload.CreateIterator(); It; ++It)
	{
		FWeakAsyncPackagePtr TestPtr = *It;
		check(TestPtr == WeakThis || !AlreadyHandled.Contains(TestPtr));
		FAsyncPackage* TestPkg(AsyncLoadingThread.GetPackage(TestPtr));
		if (!TestPkg || TestPkg == this || int32(TestPkg->AsyncPackageLoadingState) > int32(EAsyncPackageLoadingState::WaitingForPostLoad))
		{
			AlreadyHandled.Add(TestPtr);
			It.RemoveCurrent();
			continue;
		}
		if (!TestPkg->bAllExportsSerialized)
		{
			AlreadyHandled.Add(TestPtr);
			// We need to wait for this package, link it so that we are notified, we will stop exploring on the next iteration because we are definitely waiting for something
			check(!PackagesIAmWaitingForBeforePostload.Contains(TestPtr));
			PackagesIAmWaitingForBeforePostload.Add(TestPtr);
			check(!TestPkg->OtherPackagesWaitingForMeBeforePostload.Contains(WeakThis));
			TestPkg->OtherPackagesWaitingForMeBeforePostload.Add(WeakThis);
			It.RemoveCurrent();
		}
	}

	while (PackagesIMayBeWaitingForBeforePostload.Num() && !PackagesIAmWaitingForBeforePostload.Num())
	{
		// flatten the dependency tree looking for something that isn't finished
		FWeakAsyncPackagePtr PoppedPtr;
		{
			auto First = PackagesIMayBeWaitingForBeforePostload.CreateIterator();
			PoppedPtr = *First;
			First.RemoveCurrent();
		}
		if (AlreadyHandled.Contains(PoppedPtr))
		{
			continue;
		}
		AlreadyHandled.Add(PoppedPtr);
		FAsyncPackage* TestPkg(AsyncLoadingThread.GetPackage(PoppedPtr));
		if (!TestPkg)
		{
			continue;
		}
		check(TestPkg != this);
		if (int32(TestPkg->AsyncPackageLoadingState) > int32(EAsyncPackageLoadingState::WaitingForPostLoad))
		{
			continue;
		}
		check(TestPkg->bAllExportsSerialized); // we should have already handled these
		// this package and all _direct_ dependents are ready, but lets collapse the tree here and deal with indirect dependents
		for (FWeakAsyncPackagePtr MaybeRecursePtr : TestPkg->PackagesIAmWaitingForBeforePostload)
		{
			check(!(MaybeRecursePtr == WeakThis));
			FAsyncPackage* MaybeRecursePkg(AsyncLoadingThread.GetPackage(MaybeRecursePtr));
			check(MaybeRecursePkg && !MaybeRecursePkg->bAllExportsSerialized);

			check(!PackagesIAmWaitingForBeforePostload.Contains(MaybeRecursePtr));
			PackagesIAmWaitingForBeforePostload.Add(MaybeRecursePtr);
			check(!MaybeRecursePkg->OtherPackagesWaitingForMeBeforePostload.Contains(WeakThis));
			MaybeRecursePkg->OtherPackagesWaitingForMeBeforePostload.Add(WeakThis);
		}
		for (FWeakAsyncPackagePtr MaybeRecursePtr : TestPkg->PackagesIMayBeWaitingForBeforePostload)
		{
			if (!AlreadyHandled.Contains(MaybeRecursePtr))
			{
				FAsyncPackage* MaybeRecursePkg(AsyncLoadingThread.GetPackage(MaybeRecursePtr));
				if (!MaybeRecursePkg)
				{
					continue;
				}
				check(MaybeRecursePkg != this);
				if (int32(MaybeRecursePkg->AsyncPackageLoadingState) > int32(EAsyncPackageLoadingState::WaitingForPostLoad))
				{
					continue;
				}
				if (MaybeRecursePkg->bAllExportsSerialized)
				{
					PackagesIMayBeWaitingForBeforePostload.Add(MaybeRecursePtr);
				}
				else
				{
					check(!PackagesIAmWaitingForBeforePostload.Contains(MaybeRecursePtr));
					PackagesIAmWaitingForBeforePostload.Add(MaybeRecursePtr);
					check(!MaybeRecursePkg->OtherPackagesWaitingForMeBeforePostload.Contains(WeakThis));
					MaybeRecursePkg->OtherPackagesWaitingForMeBeforePostload.Add(WeakThis);
				}
			}
		}
	}
	if (!PackagesIAmWaitingForBeforePostload.Num())
	{
		check(!PackagesIMayBeWaitingForBeforePostload.Num());
		// all done
		check(AsyncPackageLoadingState == EAsyncPackageLoadingState::WaitingForPostLoad);
		AsyncPackageLoadingState = EAsyncPackageLoadingState::ReadyForPostLoad;
		AsyncLoadingThread.QueueEvent_StartPostLoad(this);
		check(bAllExportsSerialized && !OtherPackagesWaitingForMeBeforePostload.Num());
	}
}

void FAsyncPackage::Event_StartPostload()
{
	LLM_SCOPE(ELLMTag::AsyncLoading);

	Linker->GetAsyncLoader()->LogItem(TEXT("Event_StartPostload"));
	check(AsyncPackageLoadingState == EAsyncPackageLoadingState::ReadyForPostLoad);
	check(!PackagesIMayBeWaitingForBeforePostload.Num());
	check(!PackagesIAmWaitingForBeforePostload.Num());
	check(!OtherPackagesWaitingForMeBeforePostload.Num());
	AsyncPackageLoadingState = EAsyncPackageLoadingState::PostLoad_Etc;
	EventDrivenLoadingComplete();
	{
		FUObjectSerializeContext* LoadContext = GetSerializeContext();
		LoadContext->ReserveObjectsLoaded(LoadContext->GetNumObjectsLoaded() + Linker->ExportMap.Num());
		for (int32 LocalExportIndex = 0; LocalExportIndex < Linker->ExportMap.Num(); LocalExportIndex++)
		{
			FObjectExport& Export = Linker->ExportMap[LocalExportIndex];
			UObject* Object = Export.Object;
			checkSlow(!(Object && !ReferencedObjects.Contains(Object)));
			if (Object && (Object->HasAnyFlags(RF_NeedPostLoad) || Object->HasAnyInternalFlags(EInternalObjectFlags::AsyncLoading)))
			{
				check(Object->IsValidLowLevelFast());
				LoadContext->AddLoadedObject(Object);
			}
		}
	}
	check(!AsyncLoadingThread.AsyncPackagesReadyForTick.Contains(this));
	AsyncLoadingThread.AsyncPackagesReadyForTick.Add(this);
}
void FAsyncPackage::EventDrivenLoadingComplete()
{
	check(!AnyImportsAndExportWorkOutstanding());
	bool bAny = false;
	TArray<FEventLoadNodePtr> AddedNodes;
	EventNodeArray.GetAddedNodes(AddedNodes, this);

	for (FEventLoadNodePtr& Ptr : AddedNodes)
	{
		bAny = true;
		UE_LOG(LogStreaming, Error, TEXT("Leaked Event Driven Node %s"), *Ptr.HumanReadableStringForDebugging());
	}

	if (bAny)
	{
		check(!bAny);
		RemoveAllNodes();
	}
	check(!AnyImportsAndExportWorkOutstanding());

	PackagesWaitingToLinkImports.Empty(); // usually redundant

}

FEventLoadNodePtr FAsyncPackage::AddNode(EEventLoadNode Phase, FPackageIndex ImportOrExportIndex, bool bHoldForLater, int32 NumImplicitPrereqs)
{
	FEventLoadNodePtr MyNode;
	MyNode.WaitingPackage = FCheckedWeakAsyncPackagePtr(this);
	MyNode.ImportOrExportIndex = ImportOrExportIndex;
	MyNode.Phase = Phase;

	AsyncLoadingThread.GetEventGraph().AddNode(MyNode, bHoldForLater, NumImplicitPrereqs);
	return MyNode;
}

void FAsyncPackage::DoneAddingPrerequistesFireIfNone(EEventLoadNode Phase, FPackageIndex ImportOrExportIndex, bool bWasHeldForLater)
{
	FEventLoadNodePtr MyNode;
	MyNode.WaitingPackage = FCheckedWeakAsyncPackagePtr(this);
	MyNode.ImportOrExportIndex = ImportOrExportIndex;
	MyNode.Phase = Phase;

	AsyncLoadingThread.GetEventGraph().DoneAddingPrerequistesFireIfNone(MyNode, bWasHeldForLater);
}


void FAsyncPackage::RemoveNode(EEventLoadNode Phase, FPackageIndex ImportOrExportIndex)
{
	FEventLoadNodePtr MyNode;
	MyNode.WaitingPackage = FCheckedWeakAsyncPackagePtr(this);
	MyNode.ImportOrExportIndex = ImportOrExportIndex;
	MyNode.Phase = Phase;

	return AsyncLoadingThread.GetEventGraph().RemoveNode(MyNode);
}

void FAsyncPackage::NodeWillBeFiredExternally(EEventLoadNode Phase, FPackageIndex ImportOrExportIndex)
{
	FEventLoadNodePtr MyNode;
	MyNode.WaitingPackage = FCheckedWeakAsyncPackagePtr(this);
	MyNode.ImportOrExportIndex = ImportOrExportIndex;
	MyNode.Phase = Phase;

	return AsyncLoadingThread.GetEventGraph().NodeWillBeFiredExternally(MyNode);
}

void FAsyncPackage::AddArc(FEventLoadNodePtr& PrereqisiteNode, FEventLoadNodePtr& DependentNode)
{
	AsyncLoadingThread.GetEventGraph().AddArc(PrereqisiteNode, DependentNode);
}

void FAsyncPackage::RemoveAllNodes()
{
	FEventLoadGraph& Graph = AsyncLoadingThread.GetEventGraph();
	TArray<FEventLoadNodePtr> AddedNodes;
	EventNodeArray.GetAddedNodes(AddedNodes, this);
	for (FEventLoadNodePtr& Ptr : AddedNodes)
	{
		Graph.RemoveNode(Ptr);
	}
}

void FAsyncPackage::FireNode(FEventLoadNodePtr& NodeToFire)
{
	check(int32(AsyncPackageLoadingState) < int32(EAsyncPackageLoadingState::PostLoad_Etc));
	if (NodeToFire.ImportOrExportIndex.IsNull())
	{
		switch (NodeToFire.Phase)
		{
		case EEventLoadNode::Package_LoadSummary:
			break;
		case EEventLoadNode::Package_SetupImports:
			AsyncLoadingThread.QueueEvent_SetupImports(this);
			break;
		case EEventLoadNode::Package_ExportsSerialized:
			AsyncLoadingThread.QueueEvent_ExportsDone(this);
			break;
		default:
			check(0);
		}
	}
	else
	{
		switch (NodeToFire.Phase)
		{
		case EEventLoadNode::ImportOrExport_Create:
			if (NodeToFire.ImportOrExportIndex.IsImport())
			{
				ImportsThatAreNowCreated.HeapPush(NodeToFire.ImportOrExportIndex.ToImport());
			}
			else
			{
				ExportsThatCanBeCreated.HeapPush(NodeToFire.ImportOrExportIndex.ToExport());
			}
			break;
		case EEventLoadNode::Export_StartIO:
			ExportsThatCanHaveIOStarted.HeapPush(NodeToFire.ImportOrExportIndex.ToExport());
			break;
		case EEventLoadNode::ImportOrExport_Serialize:
			if (NodeToFire.ImportOrExportIndex.IsImport())
			{
				ImportsThatAreNowSerialized.HeapPush(NodeToFire.ImportOrExportIndex.ToImport());
			}
			else
			{
				ExportsThatCanBeSerialized.HeapPush(NodeToFire.ImportOrExportIndex.ToExport());
			}
			break;
		default:
			check(0);
		}

		if (AsyncPackageLoadingState == EAsyncPackageLoadingState::ProcessNewImportsAndExports) // this is redundant, but we want to save the function call
		{
			ConditionalQueueProcessImportsAndExports();
		}
	}
}


void FAsyncLoadingThread::InsertPackage(FAsyncPackage* Package, bool bReinsert, EAsyncPackageInsertMode InsertMode)
{
	checkSlow(IsInAsyncLoadThread());
	check(!IsInGameThread() || !IsMultithreaded());

#if DO_CHECK || USING_CODE_ANALYSIS
	FWeakAsyncPackagePtr WeakPtr;
	if (GEventDrivenLoaderEnabled)
	{
		WeakPtr = Package;
		check(Package);
	}
#endif

	if (!bReinsert)
	{
		// Incremented on the Async Thread, decremented on the game thread
		ExistingAsyncPackagesCounter.Increment();
		NotifyAsyncLoadingStateHasMaybeChanged();

	}

	{
#if THREADSAFE_UOBJECTS
		FScopeLock LockAsyncPackages(&AsyncPackagesCritical);
#endif
		if (bReinsert)
		{
			AsyncPackages.RemoveSingle(Package);
		}

		if (GEventDrivenLoaderEnabled)
		{
			AsyncPackages.Add(Package);
		}
		else
		{
			int32 InsertIndex = -1;

			switch (InsertMode)
			{
				case EAsyncPackageInsertMode::InsertAfterMatchingPriorities:
				{
					InsertIndex = AsyncPackages.IndexOfByPredicate([Package](const FAsyncPackage* Element)
					{
						return Element->GetPriority() < Package->GetPriority();
					});

					break;
				}

				case EAsyncPackageInsertMode::InsertBeforeMatchingPriorities:
				{
					// Insert new package keeping descending priority order in AsyncPackages
					InsertIndex = AsyncPackages.IndexOfByPredicate([Package](const FAsyncPackage* Element)
					{
						return Element->GetPriority() <= Package->GetPriority();
					});

					break;
				}
			};

			InsertIndex = InsertIndex == INDEX_NONE ? AsyncPackages.Num() : InsertIndex;

			AsyncPackages.InsertUninitialized(InsertIndex);
			AsyncPackages[InsertIndex] = Package;
		}

		if (!bReinsert)
		{
			AsyncPackageNameLookup.Add(Package->GetPackageName(), Package);
			if (GEventDrivenLoaderEnabled)
			{
				// @todo If this is a reinsert for some priority thing, well we don't go back and retract the stuff in flight to adjust the priority of events
				QueueEvent_CreateLinker(Package, FAsyncLoadEvent::EventSystemPriority_MAX);
			}
		}
	}
	check(!GEventDrivenLoaderEnabled || GetPackage(WeakPtr) == Package);
}

void FAsyncLoadingThread::AddToLoadedPackages(FAsyncPackage* Package)
{
#if THREADSAFE_UOBJECTS
	FScopeLock LoadedLock(&LoadedPackagesCritical);
#endif
	if (!LoadedPackages.Contains(Package))
	{
		#if UE_ENABLE_TRACKED_IO
		if (ActiveAsyncLoadContext)
			++ActiveAsyncLoadContext->TotalCompleted;
		#endif
		LoadedPackages.Add(Package);
		LoadedPackagesNameLookup.Add(Package->GetPackageName(), Package);
	}
}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
struct FScopedRecursionNotAllowed
{
	FAsyncLoadingThread& AsyncLoadingThread;
	FScopedRecursionNotAllowed(FAsyncLoadingThread& InThread)
		: AsyncLoadingThread(InThread)
	{
		verify(AsyncLoadingThread.RecursionNotAllowed.Increment() == 1);
	}
	~FScopedRecursionNotAllowed()
	{
		verify(AsyncLoadingThread.RecursionNotAllowed.Decrement() == 0);
	}
};
#endif

EAsyncPackageState::Type FAsyncLoadingThread::ProcessAsyncLoading(int32& OutPackagesProcessed, bool bUseTimeLimit /*= false*/, bool bUseFullTimeLimit /*= false*/, float TimeLimit /*= 0.0f*/, FFlushRequest& FlushRequest)
{
	SCOPED_LOADTIMER(AsyncLoadingTime);
	check(!IsInGameThread() || !IsMultithreaded());

	// If we're not multithreaded and flushing async loading, update the thread heartbeat
	const bool bNeedsHeartbeatTick = !bUseTimeLimit && !FAsyncLoadingThread::IsMultithreaded();
	EAsyncPackageState::Type LoadingState = EAsyncPackageState::Complete;
	OutPackagesProcessed = 0;

	double TickStartTime = FPlatformTime::Seconds();

	if (GEventDrivenLoaderEnabled)
	{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
		FScopedRecursionNotAllowed RecursionGuard(*this);
#endif

		FAsyncLoadingTickScope InAsyncLoadingTick(*this);
		uint32 LoopIterations = 0;

		while (true)
		{
			if (bNeedsHeartbeatTick && (++LoopIterations) % 32 == 31)
			{
				// Update heartbeat after 32 events
				FThreadHeartBeat::Get().HeartBeat();
				FCoreDelegates::OnAsyncLoadingFlushUpdate.Broadcast();
			}

			#if UE_ENABLE_TRACKED_IO
			if (ActiveAsyncLoadContext)
				ActiveAsyncLoadContext->Update(AsyncPackages.Num());
			#endif

			bool bDidSomething = false;
			{
				bDidSomething = GetPrecacheHandler().ProcessIncoming();
				OutPackagesProcessed += (bDidSomething ? 1 : 0);

				if (IsTimeLimitExceeded(TickStartTime, bUseTimeLimit, TimeLimit, TEXT("ProcessIncoming"), nullptr))
				{
					return EAsyncPackageState::TimeOut;
				}
			}

			if (IsAsyncLoadingSuspendedInternal())
			{
				return EAsyncPackageState::TimeOut;
			}

			{
				const float RemainingTimeLimit = FMath::Max(0.0f, TimeLimit - (float)(FPlatformTime::Seconds() - TickStartTime));
				int32 NumCreated = CreateAsyncPackagesFromQueue(bUseTimeLimit, bUseFullTimeLimit, RemainingTimeLimit);
				OutPackagesProcessed += NumCreated;
				bDidSomething = NumCreated > 0 || bDidSomething;
				if (IsTimeLimitExceeded(TickStartTime, bUseTimeLimit, TimeLimit, TEXT("CreateAsyncPackagesFromQueue")))
				{
					return EAsyncPackageState::TimeOut;
				}
			}
			if (bDidSomething)
			{
				continue;
			}

			{
				FAsyncLoadEventArgs Args;
				Args.bUseTimeLimit = bUseTimeLimit;
				Args.TickStartTime = TickStartTime;
				Args.TimeLimit = TimeLimit;

				Args.OutLastTypeOfWorkPerformed = nullptr;
				Args.OutLastObjectWorkWasPerformedOn = nullptr;
				if (EventQueue.PopAndExecute(Args))
				{
					OutPackagesProcessed++;
					if (IsTimeLimitExceeded(Args.TickStartTime, Args.bUseTimeLimit, Args.TimeLimit, Args.OutLastTypeOfWorkPerformed, Args.OutLastObjectWorkWasPerformedOn))
					{
						return EAsyncPackageState::TimeOut;
					}
					bDidSomething = true;
				}
			}
			if (bDidSomething)
			{
				continue;
			}
			if (AsyncPackagesReadyForTick.Num())
			{
				SCOPE_CYCLE_COUNTER(STAT_FAsyncLoadingThread_ProcessAsyncLoading);

				OutPackagesProcessed++;
				bDidSomething = true;
				FAsyncPackage* Package = AsyncPackagesReadyForTick[0];
				check(Package->AsyncPackageLoadingState == EAsyncPackageLoadingState::PostLoad_Etc);
				SCOPED_LOADTIMER(ProcessAsyncLoadingTime);

				EAsyncPackageState::Type LocalLoadingState = EAsyncPackageState::Complete;
				if (Package->HasFinishedLoading() == false)
				{
					float RemainingTimeLimit = FMath::Max(0.0f, TimeLimit - (float)(FPlatformTime::Seconds() - TickStartTime));
					LocalLoadingState = Package->TickAsyncPackage(bUseTimeLimit, bUseFullTimeLimit, RemainingTimeLimit);
					if (LocalLoadingState == EAsyncPackageState::TimeOut)
					{
						if (IsTimeLimitExceeded(TickStartTime, bUseTimeLimit, TimeLimit, TEXT("TickAsyncPackage")))
						{
							return EAsyncPackageState::TimeOut;
						}
						UE_LOG(LogStreaming, Error, TEXT("Should not have a timeout when the time limit is not exceeded."));
						continue;
					}
				}
				else
				{
					check(0); // if it has finished loading, it should not be in AsyncPackagesReadyForTick
				}
				if (LocalLoadingState == EAsyncPackageState::Complete)
				{
					{
#if THREADSAFE_UOBJECTS
						FScopeLock LockAsyncPackages(&AsyncPackagesCritical);
#endif
						AsyncPackageNameLookup.Remove(Package->GetPackageName());
						int32 PackageIndex = AsyncPackages.Find(Package);
						AsyncPackages.RemoveAt(PackageIndex);
						AsyncPackagesReadyForTick.RemoveAt(0, 1, EAllowShrinking::No); //@todoio this should maybe be a heap or something to avoid the removal cost
					}

					// We're done, at least on this thread, so we can remove the package now.
					AddToLoadedPackages(Package);
				}
				if (IsTimeLimitExceeded(TickStartTime, bUseTimeLimit, TimeLimit, TEXT("TickAsyncPackage")))
				{
					return EAsyncPackageState::TimeOut;
				}
			}
			if (bDidSomething)
			{
				continue;
			}
			bool bAnyIOOutstanding = GetPrecacheHandler().AnyIOOutstanding();
			if (bAnyIOOutstanding)
			{
				SCOPED_LOADTIMER(Package_EventIOWait);
				double StartTime = FPlatformTime::Seconds();
				if (bUseTimeLimit)
				{
					if (bUseFullTimeLimit)
					{
						const float RemainingTimeLimit = FMath::Max(0.0f, TimeLimit - (float)(FPlatformTime::Seconds() - TickStartTime));
						if (RemainingTimeLimit > 0.0f)
						{
							bool bGotIO = GetPrecacheHandler().WaitForIO(RemainingTimeLimit);
							if (bGotIO)
							{
								OutPackagesProcessed++;
								continue; // we got some IO, so start processing at the top
							}
							{
								float ThisTime = float(FPlatformTime::Seconds() - StartTime);
							}
						}
					}
					//UE_LOG(LogStreaming, Error, TEXT("Not using full time limit, waiting for IO..."));
					return EAsyncPackageState::TimeOut;
				}
				else
				{
					bool bGotIO = GetPrecacheHandler().WaitForIO(10.0f); // wait "forever"
					if (!bGotIO)
					{
						//UE_LOG(LogStreaming, Error, TEXT("Waited for 10 seconds on IO...."));
						FPlatformMisc::LowLevelOutputDebugString(TEXT("Waited for 10 seconds on IO...."));
					}
					{
						OutPackagesProcessed++;
					}
				}
			}
			else
			{
				LoadingState = EAsyncPackageState::Complete;
				break;
			}
		}
	} // GEventDrivenLoaderEnabled
	else if (AsyncPackages.Num())
	{
		SCOPE_CYCLE_COUNTER(STAT_FAsyncLoadingThread_ProcessAsyncLoading);
		// We need to loop as the function has to handle finish loading everything given no time limit
		// like e.g. when called from FlushAsyncLoading.
		for (int32 PackageIndex = 0; LoadingState != EAsyncPackageState::TimeOut && PackageIndex < AsyncPackages.Num(); ++PackageIndex)
		{
			SCOPED_LOADTIMER(ProcessAsyncLoadingTime);
			OutPackagesProcessed++;

			// Package to be loaded.
			FAsyncPackage* Package = AsyncPackages[PackageIndex];
			if (!ShouldProcessPackage(Package, FlushRequest))
			{
				LoadingState = EAsyncPackageState::PendingImports;
			}
			else if (Package->HasFinishedLoading() == false)
			{
				// Package tick returns EAsyncPackageState::Complete on completion.
				// We only tick packages that have not yet been loaded.
				LoadingState = Package->TickAsyncPackage(bUseTimeLimit, bUseFullTimeLimit, TimeLimit);
			}
			else
			{
				// This package has finished loading but some other package is still holding
				// a reference to it because it has this package in its dependency list.
				LoadingState = EAsyncPackageState::Complete;
			}
			if (LoadingState == EAsyncPackageState::Complete)
			{
				// We're done, at least on this thread, so we can remove the package now.
				if (!Package->HasThreadedLoadingFinished())
				{
					Package->ThreadedLoadingHasFinished();
					AddToLoadedPackages(Package);
#if THREADSAFE_UOBJECTS
					FScopeLock LockAsyncPackages(&AsyncPackagesCritical);
#endif
					AsyncPackageNameLookup.Remove(Package->GetPackageName());
					AsyncPackages.Remove(Package);

					// Need to process this index again as we just removed an item
					PackageIndex--;
				}

				check(!AsyncPackages.Contains(Package));

			}

			//@note FH: if we are flushing should we be doing this? are those exclusive external request or could be dependencies being pulled in?
			{ // maybe we shouldn't do this if we are already out of time?
				// Check if there's any new packages in the queue.
				const float RemainingTimeLimit = FMath::Max(0.0f, TimeLimit - (float)(FPlatformTime::Seconds() - TickStartTime));
				CreateAsyncPackagesFromQueue(bUseTimeLimit, bUseFullTimeLimit, RemainingTimeLimit);
			}

			#if UE_ENABLE_TRACKED_IO
			if (ActiveAsyncLoadContext)
				ActiveAsyncLoadContext->Update(AsyncPackages.Num());
			#endif

			if (bNeedsHeartbeatTick)
			{
				// Update heartbeat after each package has been processed
				FThreadHeartBeat::Get().HeartBeat();
			}
		}
	} // !GEventDrivenLoaderEnabled
	return LoadingState;
}

bool FAsyncPackage::AreAllDependenciesFullyLoadedInternal(FAsyncPackage* Package, TSet<UPackage*>& VisitedPackages, FString& OutError)
{
	// @note FH: this isn't properly thread-safe at all...

	for (UPackage* ImportPackage : Package->ImportedPackages)
	{
		if (!ImportPackage || VisitedPackages.Contains(ImportPackage))
		{
			continue;
		}
		VisitedPackages.Add(ImportPackage);

		if (FLinkerLoad* ImportPackageLinker = FLinkerLoad::FindExistingLinkerForPackage(ImportPackage))
		{
			if (ImportPackageLinker->AsyncRoot)
			{
				if (!static_cast<FAsyncPackage*>(ImportPackageLinker->AsyncRoot)->bAllExportsSerialized)
				{
					OutError = FString::Printf(TEXT("%s Doesn't have all exports Serialized"), *Package->GetPackageName().ToString());
					return false;
				}
				if (static_cast<FAsyncPackage*>(ImportPackageLinker->AsyncRoot)->DeferredPostLoadIndex < static_cast<FAsyncPackage*>(ImportPackageLinker->AsyncRoot)->DeferredPostLoadObjects.Num())
				{
					OutError = FString::Printf(TEXT("%s Doesn't have all objects processed by DeferredPostLoad"), *Package->GetPackageName().ToString());
					return false;
				}
				for (FObjectExport& Export : ImportPackageLinker->ExportMap)
				{
					if (Export.Object && Export.Object->HasAnyFlags(RF_NeedPostLoad|RF_NeedLoad))
					{
						OutError = FString::Printf(TEXT("%s has not been %s"), *Export.Object->GetFullName(), 
							Export.Object->HasAnyFlags(RF_NeedLoad) ? TEXT("Serialized") : TEXT("PostLoaded"));
						return false;
					}
				}

				if (!AreAllDependenciesFullyLoadedInternal(static_cast<FAsyncPackage*>(ImportPackageLinker->AsyncRoot), VisitedPackages, OutError))
				{
					OutError = Package->GetPackageName().ToString() + TEXT("->") + OutError;
					return false;
				}
			}
		}
	}
	return true;
}


bool FAsyncPackage::IsDependencyOf(int32 RequestID)
{
	FReadScopeLock Lock(DependentRequestsLock);
	return DependentRequests.Contains(RequestID);
}

void FAsyncPackage::AddDependentRequestsTo(FAsyncPackage* InDependency)
{
	TArray<int32> Requests;
	{
		FReadScopeLock Lock(DependentRequestsLock);
		Requests = DependentRequests.Array();
	}
	InDependency->AddDependentRequests(Requests);
}

void FAsyncPackage::AddDependentRequests(TArrayView<int32 const> Requests)
{
	FWriteScopeLock Lock(DependentRequestsLock);
	DependentRequests.Reserve(DependentRequests.Num() + Requests.Num());
	for (const int32 Request : Requests)
	{
		DependentRequests.Add(Request);
	}
}

bool FAsyncPackage::AreAllDependenciesFullyLoaded(TSet<UPackage*>& VisitedPackages)
{
	VisitedPackages.Reset();
	FString Error;
	bool bLoaded = AreAllDependenciesFullyLoadedInternal(this, VisitedPackages, Error);
	if (!bLoaded)
	{
		UE_LOG(LogStreaming, Verbose, TEXT("AreAllDependenciesFullyLoaded: %s"), *Error);
	}
	return bLoaded;
}

void FAsyncLoadingThread::CompleteFlushRequests(TArray<int32>& RequestIDs)
{
	FScopeLock Lock(&FlushRequestCritical);
	for (int32 RequestID : RequestIDs)
	{
		for (FFlushRequest& Flush : FlushRequests)
		{
			Flush.OnRequestComplete(RequestID);
		}
		FlushRequests.RemoveAll([RequestID](const FFlushRequest& Request)
			{
				return Request.IsComplete();
			});
		DuplicateRequestMap.Remove(RequestID);
	}
}

void FAsyncLoadingThread::MapDuplicateRequestID(int32 DuplicateRequestId, int32 MainRequestId)
{
	DuplicateRequestMap.Add(DuplicateRequestId, MainRequestId);
}

FFlushRequest FAsyncLoadingThread::AddFlushRequest(TConstArrayView<int32> RequestIDs)
{
	FFlushRequest FlushRequest = FFlushRequest(RequestIDs);
	{
		FScopeLock Lock(&FlushRequestCritical);
		FlushRequests.Push(FlushRequest);
	}
	return FlushRequest;
}


FFlushRequest FAsyncLoadingThread::PeekFlushRequest()
{
	FFlushRequest FlushRequest;
	{
		FScopeLock Lock(&FlushRequestCritical);
		if (FlushRequests.Num())
		{
			FlushRequest = FlushRequests.Top();
		}
	}

	// Adjust request id
	AdjustFlushRequest(FlushRequest);
	return FlushRequest;
}

void FAsyncLoadingThread::AdjustFlushRequest(FFlushRequest& FlushRequest)
{
	if (FlushRequest)
	{
		FlushRequest.AdjustRequestIDs(DuplicateRequestMap);
	}
}

bool FAsyncLoadingThread::ShouldProcessPackage(FAsyncPackage* InAsyncPackage, const FFlushRequest& FlushRequest)
{
	if (!FlushRequest || GAsyncLoadingAlwaysProcessPackages)
	{
		return true;
	}

	return Algo::AnyOf(FlushRequest.GetRequestIDs(), 
		[InAsyncPackage](const std::atomic<int32>& RequestID) { return InAsyncPackage->IsDependencyOf(RequestID.load(std::memory_order_relaxed)); });
}

EAsyncPackageState::Type FAsyncLoadingThread::ProcessLoadedPackages(bool bUseTimeLimit, bool bUseFullTimeLimit, double TimeLimit, bool& bDidSomething, const FFlushRequest& FlushRequest)
{
	EAsyncPackageState::Type Result = EAsyncPackageState::Complete;

	const double TickStartTime = FPlatformTime::Seconds();

	{
#if THREADSAFE_UOBJECTS
		FScopeLock LoadedPackagesLock(&LoadedPackagesCritical);
		FScopeLock LoadedPackagesToProcessLock(&LoadedPackagesToProcessCritical);
#endif
		if (LoadedPackages.Num() != 0)
		{
			LoadedPackagesToProcess.Append(LoadedPackages);
			LoadedPackages.Reset();
		}
		if (LoadedPackagesNameLookup.Num() != 0)
		{
			LoadedPackagesToProcessNameLookup.Append(LoadedPackagesNameLookup);
			LoadedPackagesNameLookup.Reset();
		}
	}
#if USE_EVENT_DRIVEN_ASYNC_LOAD_AT_BOOT_TIME
	if (IsMultithreaded() && GEventDrivenLoaderEnabled &&
		ENamedThreads::GetRenderThread() == ENamedThreads::GameThread &&
		!FTaskGraphInterface::Get().IsThreadProcessingTasks(ENamedThreads::GameThread)) // render thread tasks are actually being sent to the game thread.
	{
		// The async loading thread might have queued some render thread tasks (we don't have a render thread yet, so these are actually sent to the game thread)
		// We need to process them now before we do any postloads.
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		if (IsTimeLimitExceeded(TickStartTime, bUseTimeLimit, TimeLimit, TEXT("ProcessRenderThreadCommandsOnGameThread")))
		{
			return EAsyncPackageState::TimeOut;
		}
	}
#endif


	bDidSomething = bDidSomething || LoadedPackagesToProcess.Num() > 0;

	TArray<FAsyncPackage*, TInlineAllocator<4>> CompletedPackages;
	for (int32 PackageIndex = 0; PackageIndex < LoadedPackagesToProcess.Num() && !IsAsyncLoadingSuspendedInternal(); ++PackageIndex)
	{
		FPlatformMisc::PumpEssentialAppMessages();

		FAsyncPackage* Package = LoadedPackagesToProcess[PackageIndex];
		// if we are currently flushing a specific load request, only post load related package objects
		if (ShouldProcessPackage(Package, FlushRequest) 
			// Always process loaded packages when using the EDL 
			// This is because objects created/loaded are passed to package for processing through the serialize context without consideration to which their belong
			|| GEventDrivenLoaderEnabled)
		{
			SCOPED_LOADTIMER(ProcessLoadedPackagesTime);
			UE_SCOPED_COOK_STAT(Package->GetPackageName(), EPackageEventStatType::LoadPackage);
			Result = Package->PostLoadDeferredObjects(TickStartTime, bUseTimeLimit, TimeLimit);
			// if we didn't complete the post-load, we ran out of time for this slice so break immediately
			if (Result != EAsyncPackageState::Complete)
			{
				break;
			}
			// if we are done with post load and there isn't any package currently depending on this package we can start to clean it up
			else if (Package->GetDependencyRefCount() == 0)
			{
				check(Result == EAsyncPackageState::Complete);
				// Remove the package from the list before we trigger the callbacks, 
				// this is to ensure we can re-enter FlushAsyncLoading from any of the callbacks
				{
					FScopeLock LoadedLock(&LoadedPackagesToProcessCritical);
					LoadedPackagesToProcess.RemoveAt(PackageIndex--);
					LoadedPackagesToProcessNameLookup.Remove(Package->GetPackageName());

					if (FPlatformProperties::RequiresCookedData())
					{
						// Emulates ResetLoaders on the package linker's linkerroot.
						if (!Package->IsBeingProcessedRecursively())
						{
							Package->ResetLoader();
						}
					}
					else
					{
						if (GIsEditor)
						{
							// Flush linker cache for all objects loaded with this package.
							// This may be slow, hence we only do it in the editor
							Package->FlushObjectLinkerCache();
						}
						// Detach linker in mutex scope to make sure that if something requests the packagename again
						// before this old async package has been deleted we do not try to associate the new async package
						// with the linker while this async package is still bound to it.
						Package->DetachLinker();
					}

					// Close linkers opened by synchronous loads during async loading
					Package->CloseDelayedLinkers();
				}

				// Incremented on the Async Thread, now decrement as we're done with this package				
				const int32 NewExistingAsyncPackagesCounterValue = ExistingAsyncPackagesCounter.Decrement();
				NotifyAsyncLoadingStateHasMaybeChanged();

				UE_CLOG(NewExistingAsyncPackagesCounterValue < 0, LogStreaming, Fatal, TEXT("ExistingAsyncPackagesCounter is negative, this means we loaded more packages then requested so there must be a bug in async loading code."));

				CompletedPackages.Add(Package);

				if (IsTimeLimitExceeded(TickStartTime, bUseTimeLimit, TimeLimit, TEXT("ProcessLoadedPackages Misc")) 
				|| (FlushRequest && !ContainsAnyRequestInternal(FlushRequest.GetRequestIDs())))
				{
					// The only package we care about has finished loading, so we're good to exit
					break;
				}
			}
		}
	}

	// Call callbacks in a batch in a stack-local array. This is to ensure that callbacks that recursively enter this function
	// via FlushAsyncLoading do not stack overflow by doing one recursive call per LoadedPackageToProcess
	TSet<UPackage*> CompletedUPackages;
	for (FAsyncPackage* Package : CompletedPackages)
	{
		UE_SCOPED_COOK_STAT(Package->GetPackageName(), EPackageEventStatType::LoadPackage);

		// Call external callbacks
		const bool bInternalCallbacks = false;
		const EAsyncLoadingResult::Type LoadingResult = Package->HasLoadFailed() ? EAsyncLoadingResult::Failed : EAsyncLoadingResult::Succeeded;
		Package->CallCompletionCallbacks(bInternalCallbacks, LoadingResult);
		
#if WITH_EDITOR
		// In the editor we need to find any assets and packages and add them to list for later callback
		Package->GetLoadedAssetsAndPackages(LoadedAssets, CompletedUPackages);
#endif
		// We don't need the package anymore
		PackagesToDelete.AddUnique(Package);
		Package->MarkRequestIDsAsComplete();
	}
#if WITH_EDITOR
	// Call the global delegate for package endloads and set the bHasBeenLoaded flag that is used
	// to check which packages have reached this state
	for (UPackage* CompletedUPackage : CompletedUPackages)
	{
		CompletedUPackage->SetHasBeenEndLoaded(true);
	}

	if (CompletedUPackages.Num())
	{
		FCoreUObjectDelegates::OnEndLoadPackage.Broadcast(
		FEndLoadPackageContext{ CompletedUPackages.Array(), 0, false /* bSynchronous */ });
	}
#endif

	bDidSomething = bDidSomething || PackagesToDelete.Num() > 0;

	// Delete packages we're done processing and are no longer dependencies of anything else
	if (Result != EAsyncPackageState::TimeOut)
	{
		SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_CreateClustersGameThread);

		// For performance reasons this set is created here and reset inside of AreAllDependenciesFullyLoaded
		TSet<UPackage*> VisitedPackages;

		for (int32 PackageIndex = 0; PackageIndex < PackagesToDelete.Num(); ++PackageIndex)
		{
			FAsyncPackage* Package = PackagesToDelete[PackageIndex];
			UE_SCOPED_COOK_STAT(Package->GetPackageName(), EPackageEventStatType::LoadPackage);

			if (Package->GetDependencyRefCount() == 0 && !Package->IsBeingProcessedRecursively())
			{
				bool bSafeToDelete = false;
				if (Package->HasClusterObjects())
				{
					// This package will create GC clusters but first check if all dependencies of this package have been fully loaded
					if (Package->AreAllDependenciesFullyLoaded(VisitedPackages))
					{
						if (Package->CreateClusters(TickStartTime, bUseTimeLimit, TimeLimit) == EAsyncPackageState::Complete)
						{
							// All clusters created, it's safe to delete the package
							bSafeToDelete = true;
						}
						else
						{
							// Cluster creation timed out
							Result = EAsyncPackageState::TimeOut;
							break;
						}
					}
				}
				else
				{
					// No clusters to create so it's safe to delete
					bSafeToDelete = true;
				}

				if (bSafeToDelete)
				{
					PackagesToDelete.RemoveAtSwap(PackageIndex--);
					delete Package;
				}
			}

			// push stats so that we don't overflow number of tags per thread during blocking loading
			LLM_PUSH_STATS_FOR_ASSET_TAGS();
		}
	}

	if (Result == EAsyncPackageState::Complete)
	{
#if WITH_EDITORONLY_DATA
		// This needs to happen after loading new blueprints in the editor, and this is handled in EndLoad for synchronous loads
		FBlueprintSupport::FlushReinstancingQueue();
#endif

#if WITH_EDITOR
		// In editor builds, call the asset load callback. This happens in both editor and standalone to match EndLoad
		TSet<FWeakObjectPtr> TempLoadedAssets = LoadedAssets;
		LoadedAssets.Reset();

		// Make a copy because LoadedAssets could be modified by one of the OnAssetLoaded callbacks
		for (const FWeakObjectPtr& WeakAsset : TempLoadedAssets)
		{
			// It may have been unloaded/marked pending kill since being added, ignore those cases
			if (UObject* LoadedAsset = WeakAsset.Get())
			{
				// Current contract for UE_TRACK_REFERENCING_PACKAGE_SCOPED and AssetLoaded is that the caller of
				// OnAssetLoaded sets the scope to the LoadedAsset's package.
				UE_TRACK_REFERENCING_PACKAGE_SCOPED(LoadedAsset->GetPackage(), PackageAccessTrackingOps::NAME_Load);
				FCoreUObjectDelegates::OnAssetLoaded.Broadcast(LoadedAsset);
			}
		}
#endif

		// We're not done until all packages have been deleted
		Result = PackagesToDelete.Num() ? EAsyncPackageState::PendingImports : EAsyncPackageState::Complete;
	}

	return Result;
}



EAsyncPackageState::Type FAsyncLoadingThread::TickAsyncLoading(bool bUseTimeLimit, bool bUseFullTimeLimit, double TimeLimit, FFlushRequest FlushRequest)
{
	LLM_SCOPE(ELLMTag::AsyncLoading);

	check(IsInGameThread());
	check(!IsGarbageCollecting());

#if WITH_EDITOR
	// In the editor loading cannot be part of a transaction as it cannot be undone, and may result in recording half-loaded objects. So we suppress any active transaction while in this stack, and set the editor loading flag
	TGuardValue<ITransaction*> SuppressTransaction(GUndo, nullptr);
	TGuardValue<bool> IsEditorLoadingPackage(GIsEditorLoadingPackage, GIsEditor || GIsEditorLoadingPackage);
#endif

	const bool bLoadingSuspended = IsAsyncLoadingSuspendedInternal();
	EAsyncPackageState::Type Result = bLoadingSuspended ? EAsyncPackageState::PendingImports : EAsyncPackageState::Complete;

	if (!bLoadingSuspended)
	{
		const double TickStartTime = FPlatformTime::Seconds();
		double TimeLimitUsedForProcessLoaded = 0.0;

		// First make sure there's no objects pending to be unhashed. This is important in uncooked builds since we don't 
		// detach linkers immediately and we may otherwise end up getting unreachable objects from Linkers in CreateImports
		if (FPlatformProperties::RequiresCookedData() == false && IsIncrementalUnhashPending() && IsAsyncLoadingPackages())
		{
			// Call ConditionalBeginDestroy on all pending objects. CBD is where linkers get detached from objects.
			UnhashUnreachableObjects(bUseTimeLimit, TimeLimit);

			TimeLimitUsedForProcessLoaded = FPlatformTime::Seconds() - TickStartTime;

			if(IsIncrementalUnhashPending() || TimeLimitUsedForProcessLoaded > TimeLimit)
			{
				return EAsyncPackageState::TimeOut;
			}
		}

		const bool bIsMultithreaded = FAsyncLoadingThread::IsMultithreaded();

		bool bDidSomething = false;
		{
			Result = ProcessLoadedPackages(bUseTimeLimit, bUseFullTimeLimit, TimeLimit, bDidSomething, FlushRequest);
			TimeLimitUsedForProcessLoaded = FPlatformTime::Seconds() - TickStartTime;
			UE_CLOG(!GIsEditor && bUseTimeLimit && TimeLimitUsedForProcessLoaded > .1f, LogStreaming, Warning, TEXT("Took %6.2fms to ProcessLoadedPackages"), float(TimeLimitUsedForProcessLoaded) * 1000.0f);
		}

		if (!bIsMultithreaded && Result != EAsyncPackageState::TimeOut && !IsTimeLimitExceeded(TickStartTime, bUseTimeLimit, TimeLimit, TEXT("ProcessLoadedPackages")))
		{
			const double RemainingTimeLimit = FMath::Max(0.0, TimeLimit - TimeLimitUsedForProcessLoaded);
			AdjustFlushRequest(FlushRequest);
			Result = TickAsyncThread(bUseTimeLimit, bUseFullTimeLimit, RemainingTimeLimit, bDidSomething, FlushRequest);
		}

		if (Result != EAsyncPackageState::TimeOut && !IsTimeLimitExceeded(TickStartTime, bUseTimeLimit, TimeLimit, TEXT("TickAsyncThread")))
		{
			{
#if THREADSAFE_UOBJECTS
				FScopeLock QueueLock(&QueueCritical);
				FScopeLock LoadedLock(&LoadedPackagesCritical);
#endif
				// Flush deferred messages
				if (ExistingAsyncPackagesCounter.GetValue() == 0)
				{
					bDidSomething = true; // we are all done, no need to check for cycles
					FDeferredMessageLog::Flush();
					IsTimeLimitExceeded(TickStartTime, bUseTimeLimit, TimeLimit, TEXT("FDeferredMessageLog::Flush()"));
				}
			}
			if (!bDidSomething && GEventDrivenLoaderEnabled)
			{
				if (bIsMultithreaded)
				{
#if USE_EVENT_DRIVEN_ASYNC_LOAD_AT_BOOT_TIME
					if (GIsInitialLoad)
					{
						bDidSomething = EDLBootNotificationManager.ConstructWaitingBootObjects(); // with the ASL, we always create new boot objects when we have nothing else to do
						IsTimeLimitExceeded(TickStartTime, bUseTimeLimit, TimeLimit, TEXT("ConstructWaitingBootObjects"));
					}
#endif
				}
				else
				{
#if USE_EVENT_DRIVEN_ASYNC_LOAD_AT_BOOT_TIME
					if (GIsInitialLoad)
					{
						bDidSomething = EDLBootNotificationManager.FireCompletedCompiledInImports(*this); // no ASL, first try to fire any completed boot objects, and if there are none, then create some boot objects
						IsTimeLimitExceeded(TickStartTime, bUseTimeLimit, TimeLimit, TEXT("FireCompletedCompiledInImports"));
						if (!bDidSomething)
						{
							bDidSomething = EDLBootNotificationManager.ConstructWaitingBootObjects();
							IsTimeLimitExceeded(TickStartTime, bUseTimeLimit, TimeLimit, TEXT("ConstructWaitingBootObjects"));
						}
					}
#endif
					if (!bDidSomething)
					{
						CheckForCycles();
					}

					IsTimeLimitExceeded(TickStartTime, bUseTimeLimit, TimeLimit, TEXT("CheckForCycles (non-shipping)"));
				}
			}
		}

		// Call update callback once per tick on the game thread
		FCoreDelegates::OnAsyncLoadingFlushUpdate.Broadcast();
	}

	return Result;
}


int32 FMaxPackageSummarySize::Value = 8192;

void FMaxPackageSummarySize::Init()
{
	// this is used for the initial precache and should be large enough to find the actual Sum.TotalHeaderSize
	// the editor packages may not have the AdditionalPackagesToCook array stripped so we need to allocate more memory
#if WITH_EDITORONLY_DATA
	const int32 MinimumPackageSummarySize = 1024;
	check(GConfig || IsEngineExitRequested());
	Value = 16384;
	if (GConfig)
	{
		GConfig->GetInt(TEXT("/Script/Engine.StreamingSettings"), TEXT("s.MaxPackageSummarySize"), Value, GEngineIni);
		if (Value <= MinimumPackageSummarySize)
		{
			UE_LOG(LogStreaming, Warning, TEXT("Invalid minimum package file summary size (s.MaxPackageSummarySize=%d), %d is min."), Value, MinimumPackageSummarySize);
			Value = MinimumPackageSummarySize;
		}
	}
#endif
}

bool FAsyncLoadingThread::bThreadStarted = false;

FAsyncLoadingThread::FAsyncLoadingThread(int32 InThreadIndex)
	: EDLBootNotificationManager(GetGEDLBootNotificationManager())
	, Thread(nullptr)
{
	if (!Instance)
	{
		Instance = this;
	}

	AsyncLoadingThreadIndex = InThreadIndex;

	check(!bThreadStarted);
	// Current these two vars are always on or off together but can be made separate
	GEventDrivenLoaderEnabled = IsEventDrivenLoaderEnabled();

	PrecacheHandler = new FPrecacheCallbackHandler();
	QueuedRequestsEvent = FPlatformProcess::GetSynchEventFromPool();
	CancelLoadingEvent = FPlatformProcess::GetSynchEventFromPool();
	ThreadSuspendedEvent = FPlatformProcess::GetSynchEventFromPool();
	ThreadResumedEvent = FPlatformProcess::GetSynchEventFromPool();
	if ((!GEventDrivenLoaderEnabled || !USE_EVENT_DRIVEN_ASYNC_LOAD_AT_BOOT_TIME) && FAsyncLoadingThreadSettings::Get().bAsyncLoadingThreadEnabled)
	{
		StartThread();
	}

#if !IS_PROGRAM && !WITH_EDITORONLY_DATA
	UE_LOG(LogStreaming, Display, TEXT("Async Loading initialized: Event Driven Loader: %s, Async Loading Thread: %s, Async Post Load: %s"),
		GEventDrivenLoaderEnabled ? TEXT("true") : TEXT("false"),
		FAsyncLoadingThreadSettings::Get().bAsyncLoadingThreadEnabled ? TEXT("true") : TEXT("false"),
		FAsyncLoadingThreadSettings::Get().bAsyncPostLoadEnabled ? TEXT("true") : TEXT("false"));

#endif
}

FAsyncLoadingThread::~FAsyncLoadingThread()
{
	if (Thread)
	{
		ShutdownLoading();
	}
}

void FAsyncLoadingThread::ShutdownLoading()
{
	if (IsEventDrivenLoaderEnabled())
	{
		// check that event queue is empty
		const TCHAR* LastTypeOfWorkPerformed = nullptr;
		UObject* LastObjectWorkWasPerformedOn = nullptr;

		FAsyncLoadEventArgs Args;
		check(!EventQueue.PopAndExecute(Args));
	}

	delete Thread;
	Thread = nullptr;
	FPlatformProcess::ReturnSynchEventToPool(QueuedRequestsEvent);
	QueuedRequestsEvent = nullptr;
	FPlatformProcess::ReturnSynchEventToPool(CancelLoadingEvent);
	CancelLoadingEvent = nullptr;
	FPlatformProcess::ReturnSynchEventToPool(ThreadSuspendedEvent);
	ThreadSuspendedEvent = nullptr;
	FPlatformProcess::ReturnSynchEventToPool(ThreadResumedEvent);
	ThreadResumedEvent = nullptr;	
}

void FAsyncLoadingThread::StartThread()
{
	// Make sure the GC sync object is created before we start the thread (apparently this can happen before we call InitUObject())
	FGCCSyncObject::Create();

	if (!Thread && FAsyncLoadingThreadSettings::Get().bAsyncLoadingThreadEnabled)
	{
		UE::Trace::ThreadGroupBegin(TEXT("AsyncLoading"));

		UE_LOG(LogStreaming, Log, TEXT("Starting Async Loading Thread."));
		bThreadStarted = true;
		FPlatformMisc::MemoryBarrier();
		Thread = FRunnableThread::Create(this, TEXT("FAsyncLoadingThread"), 0, TPri_Normal);

		UE::Trace::ThreadGroupEnd();
	}
}

bool FAsyncLoadingThread::Init()
{
	return true;
}

uint32 FAsyncLoadingThread::Run()
{
	LLM_SCOPE(ELLMTag::AsyncLoading);

	AsyncLoadingThreadID = FPlatformTLS::GetCurrentThreadId();

	TRACE_LOADTIME_START_ASYNC_LOADING();

	if (!IsInGameThread())
	{
		FPlatformProcess::SetThreadAffinityMask(FPlatformAffinity::GetAsyncLoadingThreadMask());
		FMemory::SetupTLSCachesOnCurrentThread();
	}

	bool bWasSuspendedLastFrame = false;
	while (StopTaskCounter.GetValue() == 0)
	{
		if (IsLoadingSuspended.GetValue() == 0)
		{
			if (bWasSuspendedLastFrame)
			{
				bWasSuspendedLastFrame = false;
				ThreadResumedEvent->Trigger();
			}
			if (!IsGarbageCollectionWaiting())
			{
				bool bDidSomething = false;

				// Peek if we currently have a flush request going on.
				FFlushRequest FlushRequest = PeekFlushRequest();
				TickAsyncThread(true, false, 0.033, bDidSomething, FlushRequest);
			}
		}
		else if (!bWasSuspendedLastFrame)
		{
			bWasSuspendedLastFrame = true;
			ThreadSuspendedEvent->Trigger();			
		}
		else
		{
			FPlatformProcess::SleepNoStats(0.001f);
		}		
	}
	return 0;
}


void FAsyncLoadingThread::CheckForCycles()
{
	if (GetPrecacheHandler().AnyIOOutstanding() || EventQueue.EventQueue.Num())
	{
		// we can't check for cycles if there is stuff in flight.
		return;
	}
	// no outstanding IO, nothing was done in this iteration, we are done
	GetEventGraph().CheckForCycles();

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	// lets look over the postload wait stuff and see if that is bugged
	for (int32 PackageIdx = 0; PackageIdx < AsyncPackages.Num(); ++PackageIdx)
	{
		FAsyncPackage* Package = AsyncPackages[PackageIdx];
		if (Package->AsyncPackageLoadingState == EAsyncPackageLoadingState::WaitingForPostLoad)
		{
			UE_CLOG(!Package->PackagesIAmWaitingForBeforePostload.Num(), LogStreaming, Fatal, TEXT("We have nothing to do and there is no IO outstanding, yet %s is waiting for NO other packages to serialize:"), *Package->GetPackageName().ToString());
			UE_LOG(LogStreaming, Error, TEXT("We have nothing to do and there is no IO outstanding, yet %s is waiting for other packages to serialize:"), *Package->GetPackageName().ToString());

			for (FWeakAsyncPackagePtr Test : Package->PackagesIAmWaitingForBeforePostload)
			{
				UE_LOG(LogStreaming, Error, TEXT("    Waiting for %s"), *Test.HumanReadableStringForDebugging().ToString());
			}
		}
	}
#endif
}


EAsyncPackageState::Type FAsyncLoadingThread::TickAsyncThread(bool bUseTimeLimit, bool bUseFullTimeLimit, double TimeLimit, bool& bDidSomething, FFlushRequest& FlushRequest)
{
	check(!IsInGameThread() || !IsMultithreaded());
	EAsyncPackageState::Type Result = EAsyncPackageState::Complete;
	if (!bShouldCancelLoading)
	{
		int32 ProcessedRequests = 0;
		double TickStartTime = FPlatformTime::Seconds();
		if (AsyncThreadReady.GetValue())
		{
			if (GIsInitialLoad && GEventDrivenLoaderEnabled)
			{
				EDLBootNotificationManager.FireCompletedCompiledInImports(*this);
			}
			{
				//@note FH: pretty sure the GCScopeGuard isn't needed here, nor is it useful, it is also retaken inside CreateAsyncPackagesFromQueue which isn't necessary
				FGCScopeGuard GCGuard;
				CreateAsyncPackagesFromQueue(bUseTimeLimit, bUseFullTimeLimit, TimeLimit); 
			}
			float TimeUsed = (float)(FPlatformTime::Seconds() - TickStartTime);
			const float RemainingTimeLimit = FMath::Max(0.0f, (float)TimeLimit - TimeUsed);
			if (IsGarbageCollectionWaiting() || (RemainingTimeLimit <= 0.0f && bUseTimeLimit && !IsMultithreaded()))
			{
				Result = EAsyncPackageState::TimeOut;
			}
			else
			{
				FGCScopeGuard GCGuard;
				Result = ProcessAsyncLoading(ProcessedRequests, bUseTimeLimit, bUseFullTimeLimit, RemainingTimeLimit, FlushRequest);
				bDidSomething = bDidSomething || ProcessedRequests > 0;
			}
		}

		if (ProcessedRequests == 0 && IsMultithreaded() && Result == EAsyncPackageState::Complete)
		{
			uint32 WaitTime = 30;
			if (IsEventDrivenLoaderEnabled())
			{
				if (!EDLBootNotificationManager.IsWaitingForSomething() && !(IsGarbageCollectionWaiting() || IsGarbageCollecting()))
				{
					CheckForCycles();
					IsTimeLimitExceeded(TickStartTime, bUseTimeLimit, TimeLimit, TEXT("CheckForCycles (non-shipping)"));
				}
				else
				{
					WaitTime = 1; // we are waiting for the game thread to deal with the boot constructors, so lets spin tighter
				}
			}
			const bool bIgnoreThreadIdleStats = true;
			SCOPED_LOADTIMER(Package_Temp3);
			QueuedRequestsEvent->Wait(WaitTime, bIgnoreThreadIdleStats);
		}

	}
	else
	{
		// Blocks main thread
		double TickStartTime = FPlatformTime::Seconds();
		CancelAsyncLoadingInternal();
		IsTimeLimitExceeded(TickStartTime, bUseTimeLimit, TimeLimit, TEXT("CancelAsyncLoadingInternal"));
		bShouldCancelLoading = false;
	}

#if LOOKING_FOR_PERF_ISSUES
	// Update stats
	SET_FLOAT_STAT( STAT_AsyncIO_AsyncLoadingBlockingTime, FPlatformTime::ToSeconds( BlockingCycles.GetValue() ) );
	BlockingCycles.Set( 0 );
#endif

	return Result;
}

void FAsyncLoadingThread::Stop()
{
	StopTaskCounter.Increment();
}

void FAsyncLoadingThread::CancelLoading()
{
	check(IsInGameThread());

	bShouldCancelLoading = true;
	if (IsMultithreaded())
	{
		CancelLoadingEvent->Wait();
	}
	else
	{
		// This will immediately cancel async loading without waiting for packages to finish loading
		FlushAsyncLoading();
		// It's possible we haven't been async loading at all in which case the above call would not reset bShouldCancelLoading
		bShouldCancelLoading = false;
	}

	// Actually delete all packages and execute delegates
	FinalizeCancelAsyncLoadingInternal();
}

void FAsyncLoadingThread::CancelAsyncLoadingInternal()
{
	FAsyncLoadingTickScope AsyncTickScope(*this);

	if (GEventDrivenLoaderEnabled)
	{
		while (GetPrecacheHandler().AnyIOOutstanding())
		{
			GetPrecacheHandler().WaitForIO(10.0f);
			GetPrecacheHandler().ProcessIncoming();
		}
	}

	{
		// Packages we haven't yet started processing.
#if THREADSAFE_UOBJECTS
		FScopeLock QueueLock(&QueueCritical);
#endif
		QueuedPackagesToCancel = MoveTemp(QueuedPackages);
		QueuedPackages.Reset();
	}

	{
		// Packages we started processing, need to be canceled.
		// Accessed only in async thread, no need to protect region.
		// Move first so that we remove the package from these lists BEFORE we delete it otherwise we will assert in the package dtor.
		PackagesToCancel.Append(PackagesToDelete);

		// This is accessed on the game thread but it should be blocked at this point
		PackagesToCancel.Append(AsyncPackagesReadyForTick);
		PackagesToCancel.Append(AsyncPackages);
		AsyncPackagesReadyForTick.Reset();
		AsyncPackages.Reset();
		AsyncPackageNameLookup.Reset();
	}

	{
		// Packages that are already loaded. May be halfway through PostLoad
#if THREADSAFE_UOBJECTS
		FScopeLock LoadedLock(&LoadedPackagesCritical);
#endif
		PackagesToCancel.Append(LoadedPackages);
		LoadedPackages.Reset();
		LoadedPackagesNameLookup.Reset();
	}
	{
#if THREADSAFE_UOBJECTS
		FScopeLock LoadedLock(&LoadedPackagesToProcessCritical);
#endif
		PackagesToCancel.Append(LoadedPackagesToProcess);
		LoadedPackagesToProcess.Reset();
		LoadedPackagesToProcessNameLookup.Reset();
	}

	ExistingAsyncPackagesCounter.Reset();
	QueuedPackagesCounter.Reset();

	EventQueue.EventQueue.Empty();
	GetEventGraph().PackagesWithNodes.Empty();

	{
#if THREADSAFE_UOBJECTS
		FScopeLock Lock(&PendingRequestsCritical);
#endif
		PendingRequests.Empty();
	}

	NotifyAsyncLoadingStateHasMaybeChanged();

	// Notify everyone streaming is canceled.
	CancelLoadingEvent->Trigger();
}

void FAsyncLoadingThread::FinalizeCancelAsyncLoadingInternal()
{
	check(IsInGameThread());	

#if THREADSAFE_UOBJECTS
	FScopeLock QueueLock(&QueueCritical);
	FScopeLock LoadedLock(&LoadedPackagesCritical);
	FScopeLock LoadedToProcessLock(&LoadedPackagesToProcessCritical);
#endif

	check(QueuedPackages.Num() == 0);
	const EAsyncLoadingResult::Type Result = EAsyncLoadingResult::Canceled;
	for (FAsyncPackageDesc* PackageDescToCancel : QueuedPackagesToCancel)
	{
		if (PackageDescToCancel->PackageLoadedDelegate.IsValid())
		{
			PackageDescToCancel->PackageLoadedDelegate->ExecuteIfBound(PackageDescToCancel->Name, nullptr, Result);
		}
		delete PackageDescToCancel;
	}
	QueuedPackagesToCancel.Empty();

	check(PackagesToDelete.Num() == 0);
	check(AsyncPackages.Num() == 0);
	check(LoadedPackages.Num() == 0);
	check(LoadedPackagesToProcess.Num() == 0);
	for (FAsyncPackage* PackageToCancel : PackagesToCancel)
	{
		PackageToCancel->Cancel();
	}
	for (FAsyncPackage* PackageToCancel : PackagesToCancel)
	{
		delete PackageToCancel;
	}
	PackagesToCancel.Empty();
}

void FAsyncLoadingThread::SuspendLoading()
{
	UE_CLOG(!IsInGameThread() || IsInSlateThread(), LogStreaming, Fatal, TEXT("Async loading can only be suspended from the main thread"));
	const int32 SuspendCount = IsLoadingSuspended.Increment();
#if !WITH_EDITORONLY_DATA
	UE_LOG(LogStreaming, Display, TEXT("Suspending async loading (%d)"), SuspendCount);
#endif
	if (IsMultithreaded() && SuspendCount == 1)
	{
		TRACE_LOADTIME_SUSPEND_ASYNC_LOADING();
		ThreadSuspendedEvent->Wait();
	}
}

void FAsyncLoadingThread::ResumeLoading()
{
	check(IsInGameThread() && !IsInSlateThread());
	const int32 SuspendCount = IsLoadingSuspended.Decrement();
#if !WITH_EDITORONLY_DATA
	UE_LOG(LogStreaming, Display, TEXT("Resuming async loading (%d)"), SuspendCount);
#endif
	UE_CLOG(SuspendCount < 0, LogStreaming, Fatal, TEXT("ResumeAsyncLoadingThread: Async loading was resumed more times than it was suspended."));
	if (IsMultithreaded() && SuspendCount == 0)
	{
		ThreadResumedEvent->Wait();
		TRACE_LOADTIME_RESUME_ASYNC_LOADING();
	}
}

float FAsyncLoadingThread::GetAsyncLoadPercentage(const FName& PackageName)
{
	float LoadPercentage = -1.0f;
	{
#if THREADSAFE_UOBJECTS
		FScopeLock LockAsyncPackages(&AsyncPackagesCritical);
#endif
		FAsyncPackage* Package = AsyncPackageNameLookup.FindRef(PackageName);
		if (Package)
		{
			LoadPercentage = Package->GetLoadPercentage();
		}
	}
	if (LoadPercentage < 0.0f)
	{
#if THREADSAFE_UOBJECTS
		FScopeLock LockLoadedPackages(&LoadedPackagesCritical);
#endif
		FAsyncPackage* Package = LoadedPackagesNameLookup.FindRef(PackageName);
		if (Package)
		{
			LoadPercentage = Package->GetLoadPercentage();
		}
	}
	if (LoadPercentage < 0.0f)
	{
		checkSlow(IsInGameThread());
		FAsyncPackage* Package = LoadedPackagesToProcessNameLookup.FindRef(PackageName);
		if (Package)
		{
			LoadPercentage = Package->GetLoadPercentage();
		}
	}

	return LoadPercentage;
}

/**
 * Call back into the async loading code to inform of the creation of a new object
 * @param Object		Object created
 * @param bSubObject	Object created as a sub-object of a loaded object
 */
void FAsyncLoadingThread::NotifyConstructedDuringAsyncLoading(UObject* Object, bool bSubObject)
{
	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	if (!ThreadContext.AsyncPackage)
	{
		// Something is creating objects on the async loading thread outside of the actual async loading code
		// e.g. ShaderCodeLibrary::OnExternalReadCallback doing FTaskGraphInterface::Get().WaitUntilTaskCompletes(Event);
		return;
	}

	// Mark objects created during async loading process (e.g. from within PostLoad or CreateExport) as async loaded so they 
	// cannot be found. This requires also keeping track of them so we can remove the async loading flag later one when we 
	// finished routing PostLoad to all objects.
	if (!bSubObject)
	{
		Object->SetInternalFlags(EInternalObjectFlags::AsyncLoading);
	}

	FAsyncPackage* AsyncPackage = static_cast<FAsyncPackage*>(ThreadContext.AsyncPackage);
	AsyncPackage->AddObjectReference(Object);
	if (GEventDrivenLoaderEnabled)
	{
		// if this is in the package and is an export, then let mark it as needing load now
		if (Object->GetOutermost() == AsyncPackage->GetLinkerRoot() && 
			int32(AsyncPackage->AsyncPackageLoadingState) <= int32(EAsyncPackageLoadingState::ProcessNewImportsAndExports) &&
			int32(AsyncPackage->AsyncPackageLoadingState) > int32(EAsyncPackageLoadingState::WaitingForSummary))
		{
			AsyncPackage->MarkNewObjectForLoadIfItIsAnExport(Object);
		}
	}
}

void FAsyncLoadingThread::NotifyRegistrationEvent(const TCHAR* PackageName, const TCHAR* Name, ENotifyRegistrationType NotifyRegistrationType, ENotifyRegistrationPhase NotifyRegistrationPhase, UObject* (*InRegister)(), bool InbDynamic, UObject* FinishedObject)
{
	EDLBootNotificationManager.NotifyRegistrationEvent(PackageName, Name, NotifyRegistrationType, NotifyRegistrationPhase, InRegister, InbDynamic);
}

void FAsyncLoadingThread::NotifyRegistrationComplete()
{
	EDLBootNotificationManager.NotifyRegistrationComplete(*this);
}

void FAsyncLoadingThread::FireCompletedCompiledInImport(void* AsyncPackage, FPackageIndex Import)
{
	FEventLoadNodePtr NodeToFire;
	NodeToFire.WaitingPackage = FCheckedWeakAsyncPackagePtr(static_cast<FAsyncPackage*>(AsyncPackage));
	NodeToFire.ImportOrExportIndex = Import;
	NodeToFire.Phase = EEventLoadNode::ImportOrExport_Create;
	static_cast<FAsyncPackage*>(AsyncPackage)->FireNode(NodeToFire);
}

/*-----------------------------------------------------------------------------
	FAsyncPackage implementation.
-----------------------------------------------------------------------------*/

/**
* Constructor
*/
FAsyncPackage::FAsyncPackage(FAsyncLoadingThread& InThread, const FAsyncPackageDesc& InDesc, FEDLBootNotificationManager& InEDLBootNotificationManager)
: Desc(InDesc)
, Linker(nullptr)
, LinkerRoot(nullptr)
, DependencyRootPackage(nullptr)
, DependencyRefCount(0)
, LoadImportIndex(0)
, ImportIndex(0)
, ExportIndex(0)
, PreLoadIndex(0)
, PreLoadSortIndex(0)
, FinishExternalReadDependenciesIndex(0)
, PostLoadIndex(0)
, DeferredPostLoadIndex(0)
, PostLoadInstanceIndex(0)
, DeferredFinalizeIndex(0)
, DeferredClusterIndex(0)
, TimeLimit(FLT_MAX)
, bUseTimeLimit(false)
, bUseFullTimeLimit(false)
, bTimeLimitExceeded(false)
, bLoadHasFailed(false)
, bLoadHasFinished(false)
, bThreadedLoadingFinished(false)
, bCreatedLinkerRoot(false)
, TickStartTime(0)
, LastObjectWorkWasPerformedOn(nullptr)
, LastTypeOfWorkPerformed(nullptr)
, LoadStartTime(0.0)
, LoadPercentage(0)
, ReentryCount(0)
, AsyncLoadingThread(InThread)
, EDLBootNotificationManager(InEDLBootNotificationManager)
// Begin EDL specific properties
, AsyncPackageLoadingState(EAsyncPackageLoadingState::NewPackage)
, SerialNumber(AsyncPackageSerialNumber.Increment())
, CurrentBlockOffset(-1)
, CurrentBlockBytes(-1)
, ImportAddNodeIndex(0)
, ExportAddNodeIndex(0)
, bProcessImportsAndExportsInFlight(false)
, bProcessPostloadWaitInFlight(false)
, bAllExportsSerialized(false)
// End EDL specific properties

#if PERF_TRACK_DETAILED_ASYNC_STATS
, TickCount(0)
, TickLoopCount(0)
, CreateLinkerCount(0)
, FinishLinkerCount(0)
, CreateImportsCount(0)
, CreateExportsCount(0)
, PreLoadObjectsCount(0)
, PostLoadObjectsCount(0)
, FinishObjectsCount(0)
, TickTime(0.0)
, CreateLinkerTime(0.0)
, FinishLinkerTime(0.0)
, CreateImportsTime(0.0)
, CreateExportsTime(0.0)
, PreLoadObjectsTime(0.0)
, PostLoadObjectsTime(0.0)
, FinishObjectsTime(0.0)
#endif // PERF_TRACK_DETAILED_ASYNC_STATS
{
	TRACE_LOADTIME_NEW_ASYNC_PACKAGE(this);
	AddRequestID(InDesc.RequestID);
	AddDependentRequests(MakeArrayView<const int32>(&InDesc.RequestID, 1));
}

FAsyncPackage::~FAsyncPackage()
{
#if DO_CHECK
	if (GEventDrivenLoaderEnabled)
	{
		for (FCompletionCallback& CompletionCallback : CompletionCallbacks)
		{
			checkSlow(CompletionCallback.bIsInternal || IsInGameThread());

			if (!CompletionCallback.bCalled)
			{
				check(0);
			}
		}
	}
	check(bLoadHasFailed || DeferredClusterObjects.Num() == 0);
#endif

	MarkRequestIDsAsComplete();
	DetachLinker();
	if (GEventDrivenLoaderEnabled)
	{
		SerialNumber = 0; // the weak pointer will always fail now
		check(!EventNodeArray.Array && !EventNodeArray.TotalNumberOfNodesAdded);
		RemoveAllNodes();
	}

	EmptyReferencedObjects();

	TRACE_LOADTIME_DESTROY_ASYNC_PACKAGE(this);
}


void FAsyncPackage::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AllowEliminatingReferences(false);
	Collector.AddReferencedObjects(ReferencedObjects);
	Collector.AddReferencedObjects(DeferredFinalizeObjects);
	Collector.AddReferencedObjects(PackageObjLoaded);
	Collector.AddReferencedObjects(ImportedPackages);
	Collector.AllowEliminatingReferences(true);
}

void FAsyncPackage::AddObjectReference(UObject* InObject)
{
	if (InObject)
	{
		UE_CLOG(!IsInGameThread() && !IsGarbageCollectionLocked(), LogStreaming, Fatal, TEXT("Trying to add an object %s to FAsyncPackage referenced objects list outside of a FGCScopeGuard."), *InObject->GetFullName());
		{
			FScopeLock ReferencedObjectsLock(&ReferencedObjectsCritical);
			if (!ReferencedObjects.Contains(InObject))
			{
				ReferencedObjects.Add(InObject);
			}
		}
		UE_CLOG(InObject->IsUnreachable(), LogStreaming, Fatal, TEXT("Trying to add an unreachable object %s to FAsyncPackage %s referenced objects list."), *InObject->GetFullName(), *GetPackageName().ToString());
	}
}

void FAsyncPackage::EmptyReferencedObjects()
{
	const EInternalObjectFlags AsyncFlags = EInternalObjectFlags::Async | EInternalObjectFlags::AsyncLoading;
	FScopeLock ReferencedObjectsLock(&ReferencedObjectsCritical);
	for (UObject* Obj : ReferencedObjects)
	{
		if (Obj)
		{
			// Temporary fatal messages instead of checks to find the cause for a one-time crash in shipping config
			UE_CLOG(!Obj->IsValidLowLevelFast(), LogStreaming, Fatal, TEXT("Invalid object in Async Objects Referencer"));
			Obj->AtomicallyClearInternalFlags(AsyncFlags);
			check(!Obj->HasAnyInternalFlags(AsyncFlags))
		}
	}
	ReferencedObjects.Reset();
}

void FAsyncPackage::AddRequestID(int32 Id)
{
	if (Id > 0)
	{
		if (Desc.RequestID == INDEX_NONE)
		{
			// For debug readability
			Desc.RequestID = Id;
		}
		else if (Desc.RequestID != Id)
		{
			// If this is a duplicate id, map it to the original request.
			// this is to properly handle flush request which may not have propagated request dependencies to a duplicate request made later
			AsyncLoadingThread.MapDuplicateRequestID(Id, Desc.RequestID);
		}
		RequestIDs.Add(Id);
		AsyncLoadingThread.AddPendingRequest(Id);
		TRACE_LOADTIME_ASYNC_PACKAGE_REQUEST_ASSOCIATION(this, Id);
	}
}

void FAsyncPackage::MarkRequestIDsAsComplete()
{
	AsyncLoadingThread.RemovePendingRequests(RequestIDs);
	AsyncLoadingThread.CompleteFlushRequests(RequestIDs);
	RequestIDs.Reset();
}

/**
 * @return Time load begun. This is NOT the time the load was requested in the case of other pending requests.
 */
double FAsyncPackage::GetLoadStartTime() const
{
	return LoadStartTime;
}

/**
 * Emulates ResetLoaders for the package's Linker objects, hence deleting it. 
 */
void FAsyncPackage::ResetLoader()
{
	LLM_SCOPE(ELLMTag::AsyncLoading);

	// Reset loader.
	if (Linker)
	{
		check(Linker->AsyncRoot == this || Linker->AsyncRoot == nullptr);
		Linker->AsyncRoot = nullptr;
		// Flush cache and queue for delete
		Linker->FlushCache();
		Linker->Detach();
		FLinkerManager::Get().RemoveLinker(Linker);
		Linker = nullptr;
	}
}

void FAsyncPackage::DetachLinker()
{	
	if (Linker)
	{
		Linker->FlushCache();
		checkf(bLoadHasFinished || bLoadHasFailed, TEXT("FAsyncPackage::DetachLinker called before load finished on package \"%s\""), *this->GetPackageName().ToString());
		check(Linker->AsyncRoot == this || Linker->AsyncRoot == nullptr);
		Linker->AsyncRoot = nullptr;
		Linker = nullptr;
	}
}

void FAsyncPackage::FlushObjectLinkerCache()
{
	TSet<FLinkerLoad*> Linkers;
	for (UObject* Obj : PackageObjLoaded)
	{
		if (Obj)
		{
			FLinkerLoad* ObjLinker = Obj->GetLinker();
			if (ObjLinker)
			{
				Linkers.Add(ObjLinker);
			}
		}
	}
	for (FLinkerLoad* LocalLinker : Linkers)
	{
		LocalLinker->FlushCache();
	}
}

#if WITH_EDITOR 
void FAsyncPackage::GetLoadedAssetsAndPackages(TSet<FWeakObjectPtr>& AssetList, TSet<UPackage*>& PackageList)
{
	for (UObject* Obj : PackageObjLoaded)
	{
		if (!IsValid(Obj))
		{
			continue;
		}
		if (Obj->IsAsset())
		{
			AssetList.Add(Obj);
		}
		UPackage* Package = Obj->GetPackage();
		if (Package && !Package->HasAnyFlags(RF_Transient) && !Package->HasAnyPackageFlags(PKG_InMemoryOnly))
		{
			PackageList.Add(Package);
		}
	}
}
#endif

/**
 * Gives up time slice if time limit is enabled.
 *
 * @return true if time slice can be given up, false otherwise
 */
bool FAsyncPackage::GiveUpTimeSlice()
{
	if (bUseTimeLimit && !bUseFullTimeLimit)
	{
		bTimeLimitExceeded = true;
	}
	return bTimeLimitExceeded;
}

/**
 * Begin async loading process. Simulates parts of BeginLoad.
 *
 * Objects created during BeginAsyncLoad and EndAsyncLoad will have EInternalObjectFlags::AsyncLoading set
 */
void FAsyncPackage::BeginAsyncLoad()
{
	if (IsInGameThread())
	{
		FAsyncLoadingThread::EnterAsyncLoadingTick(AsyncLoadingThread.GetThreadIndex());
	}

	// this won't do much during async loading except increase the load count which causes IsLoading to return true
	FUObjectSerializeContext* LoadContext = GetSerializeContext();
	BeginLoad(LoadContext);
}

/**
 * End async loading process. Simulates parts of EndLoad(). FinishObjects 
 * simulates some further parts once we're fully done loading the package.
 */
void FAsyncPackage::EndAsyncLoad()
{
	check(IsAsyncLoading());

	// this won't do much during async loading except decrease the load count which causes IsLoading to return false
	FUObjectSerializeContext* LoadContext = GetSerializeContext();
	EndLoad(LoadContext);

	if (IsInGameThread())
	{
		FAsyncLoadingThread::LeaveAsyncLoadingTick(AsyncLoadingThread.GetThreadIndex());
	}

	if (!bLoadHasFailed)
	{
		// @note FH: This is actually unsafer if multiple thread plays with the object flags...
		// Mark the package as loaded, if we succeeded
		LinkerRoot->SetFlags(RF_WasLoaded);
	}
}

/**
 * Ticks the async loading code.
 *
 * @param	InbUseTimeLimit		Whether to use a time limit
 * @param	InbUseFullTimeLimit	If true use the entire time limit, even if you have to block on IO
 * @param	InOutTimeLimit			Soft limit to time this function may take
 *
 * @return	true if package has finished loading, false otherwise
 */

EAsyncPackageState::Type FAsyncPackage::TickAsyncPackage(bool InbUseTimeLimit, bool InbUseFullTimeLimit, float& InOutTimeLimit)
{

	// We want this check only with EDL enabled
	check(!GEventDrivenLoaderEnabled || int32(AsyncPackageLoadingState) > int32(EAsyncPackageLoadingState::ProcessNewImportsAndExports));
	// @note FH: We shouldn't be ticking this package from the async thread once we are have finished loading it
	check(!HasFinishedLoading());

	ReentryCount++;

	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_Tick);
	SCOPED_LOADTIMER(Package_Tick);
	UE_SCOPED_COOK_STAT(Desc.Name, EPackageEventStatType::LoadPackage);

	// Whether we should execute the next step.
	EAsyncPackageState::Type LoadingState = EAsyncPackageState::Complete;

	// Set up tick relevant variables.
	bUseTimeLimit = InbUseTimeLimit;
	bUseFullTimeLimit = InbUseFullTimeLimit;
	bTimeLimitExceeded = false;
	TimeLimit = InOutTimeLimit;
	TickStartTime = FPlatformTime::Seconds();

	// Keep track of time when we start loading.
	if (LoadStartTime == 0.0)
	{
		LoadStartTime = TickStartTime;

		// If we are a dependency of another package, we need to tell that package when its first dependent started loading,
		// otherwise because that package loads last it'll not include the entire load time of all its dependencies
		if (DependencyRootPackage)
		{
			// Only the first dependent needs to register the start time
			if (DependencyRootPackage->GetLoadStartTime() == 0.0)
			{
				DependencyRootPackage->LoadStartTime = TickStartTime;
			}
		}
	}

	FAsyncPackageScope PackageScope(this);
	// Make sure we finish our work if there's no time limit. The loop is required as PostLoad
	// might cause more objects to be loaded in which case we need to Preload them again.
	do
	{
		// Reset value to true at beginning of loop.
		LoadingState = EAsyncPackageState::Complete;

		// Begin async loading, simulates BeginLoad
		BeginAsyncLoad();
		{
#if WITH_EDITOR
			UE::Core::Private::FPlayInEditorLoadingScope PlayInEditorIDScope(Desc.PIEInstanceID);
#endif
			// We have begun loading a package that we know the name of. Let the package time tracker know.
			FExclusiveLoadPackageTimeTracker::PushLoadPackage(Desc.PackagePath.GetPackageFName());

			if (!GEventDrivenLoaderEnabled)
			{
				// Create raw linker. Needs to be async created via ticking before it can be used.
				if (LoadingState == EAsyncPackageState::Complete) //-V547
				{
					SCOPED_LOADTIMER(Package_CreateLinker);
					LoadingState = CreateLinker();
				}

				// Async create linker.
				if (LoadingState == EAsyncPackageState::Complete)
				{
					SCOPED_LOADTIMER(Package_FinishLinker);
					LoadingState = FinishLinker();
				}

				// Load imports from linker import table asynchronously.
				if (LoadingState == EAsyncPackageState::Complete && !bLoadHasFinished)
				{
					SCOPED_LOADTIMER(Package_LoadImports);
					LoadingState = LoadImports();
				}

				// Create imports from linker import table.
				if (LoadingState == EAsyncPackageState::Complete && !bLoadHasFinished)
				{
					SCOPED_LOADTIMER(Package_CreateImports);
					LoadingState = CreateImports();
				}

#if WITH_EDITORONLY_DATA
				// Create and preload the package meta-data
				if (LoadingState == EAsyncPackageState::Complete && !bLoadHasFinished)
				{
					SCOPED_LOADTIMER(Package_CreateMetaData);
					LoadingState = CreateMetaData();
				}
#endif // WITH_EDITORONLY_DATA

				// Create exports from linker export table and also preload them.
				if (LoadingState == EAsyncPackageState::Complete && !bLoadHasFinished)
				{
					SCOPED_LOADTIMER(Package_CreateExports);
					LoadingState = CreateExports();
				}

				// Call Preload on the linker for all loaded objects which causes actual serialization.
				if (LoadingState == EAsyncPackageState::Complete && !bLoadHasFinished)
				{
					SCOPED_LOADTIMER(Package_PreLoadObjects);
					LoadingState = PreLoadObjects();
				}

				if ((LoadingState == EAsyncPackageState::Complete || bLoadHasFailed) && !bLoadHasFinished)
				{
					const bool bInternalCallbacks = true;
					CallCompletionCallbacks(bInternalCallbacks, bLoadHasFailed ? EAsyncLoadingResult::Failed : EAsyncLoadingResult::Succeeded);
				}

				if (LoadingState == EAsyncPackageState::Complete)
				{
					// We can only continue to PostLoad if all imported packages finished serializing their exports
					for (UPackage* ImportedPackage : ImportedPackages)
					{
						if (ImportedPackage && ImportedPackage->GetLinker() && ImportedPackage->GetLinker()->AsyncRoot && !static_cast<FAsyncPackage*>(ImportedPackage->GetLinker()->AsyncRoot)->bAllExportsSerialized)
						{
							LoadingState = EAsyncPackageState::PendingImports;
							break;
						}
					}
				}
			} // !GEventDrivenLoaderEnabled

			if (LoadingState == EAsyncPackageState::Complete && !bLoadHasFailed && !bLoadHasFinished)
			{
				SCOPED_LOADTIMER(Package_ExternalReadDependencies);
				LoadingState = FinishExternalReadDependencies();
			}

			// Call PostLoad on objects, this could cause new objects to be loaded that require
			// another iteration of the PreLoad loop.
			if (LoadingState == EAsyncPackageState::Complete && !bLoadHasFailed && !bLoadHasFinished)
			{
				SCOPED_LOADTIMER(Package_PostLoadObjects);
				LoadingState = PostLoadObjects();
			}

			// We are done loading the package for now. Whether it is done or not, let the package time tracker know.
			FExclusiveLoadPackageTimeTracker::PopLoadPackage(Linker ? Linker->LinkerRoot : nullptr);
		}

		// End async loading, simulates EndLoad
		EndAsyncLoad();

		// Finish objects (removing EInternalObjectFlags::AsyncLoading, dissociate imports and forced exports, 
		// call completion callback, ...
		// If the load has failed, perform completion callbacks and then quit
		if (LoadingState == EAsyncPackageState::Complete || bLoadHasFailed)
		{
			LoadingState = FinishObjects();
		}
	} while (!IsTimeLimitExceeded() && LoadingState == EAsyncPackageState::TimeOut);

	check(bUseTimeLimit || LoadingState != EAsyncPackageState::TimeOut || AsyncLoadingThread.IsAsyncLoadingSuspendedInternal() || IsGarbageCollectionWaiting());

	// @note FH: Should the package be marked as fully loaded if there are still pending post-load?
	if (LinkerRoot && LoadingState == EAsyncPackageState::Complete)
	{
		LinkerRoot->MarkAsFullyLoaded();
	}

	// Mark this package as loaded if everything completed. leave it true if it was already marked as such through recursivity
	bLoadHasFinished = bLoadHasFinished || (LoadingState == EAsyncPackageState::Complete);
	// We can't have a reference to a UObject.
	LastObjectWorkWasPerformedOn = nullptr;
	// Reset type of work performed.
	LastTypeOfWorkPerformed = nullptr;

	if (bLoadHasFinished && GEventDrivenLoaderEnabled)
	{
		check(AsyncPackageLoadingState == EAsyncPackageLoadingState::PostLoad_Etc);
		AsyncPackageLoadingState = EAsyncPackageLoadingState::PackageComplete;
	}

	// Subtract the time it took to load this package from the global limit.
	InOutTimeLimit = (float)FMath::Max(0.0, InOutTimeLimit - (FPlatformTime::Seconds() - TickStartTime));

	ReentryCount--;
	check(ReentryCount >= 0);

	// true means that we're done loading this package.
	return LoadingState;
}

/**
 * Create linker async. Linker is not finalized at this point.
 *
 * @return true
 */
EAsyncPackageState::Type FAsyncPackage::CreateLinker()
{
	SCOPED_LOADTIMER(CreateLinkerTime);
	if (Linker == nullptr)
	{
		SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_CreateLinker);

		LastObjectWorkWasPerformedOn = nullptr;
		LastTypeOfWorkPerformed = TEXT("creating Linker");

		// Try to find existing package or create it if not already present.
		UPackage* Package = nullptr;
		{
			SCOPED_LOADTIMER(CreateLinker_CreatePackage);
			FGCScopeGuard GCGuard;
			{
#if UE_WITH_PACKAGE_ACCESS_TRACKING
				UE_TRACK_REFERENCING_PACKAGE_SCOPED(Desc.ReferencerPackageName, Desc.ReferencerPackageOp);
#endif
#if WITH_EDITOR
				FCookLoadScope CookLoadScope(Desc.CookLoadType);
#endif
				Package = CreatePackage(*Desc.Name.ToString());
			}
			if (!Package)
			{
				UE_LOG(LogStreaming, Error, TEXT("Failed to create package %s requested by async loading code. NameToLoad: %s"), *Desc.Name.ToString(), *Desc.PackagePath.GetDebugName());
				bLoadHasFailed = true;
				return EAsyncPackageState::TimeOut;
			}

			if (IsNativeCodePackage(Package))
			{
				// Client requested load of a compiled in package, silently fail early instead of trying and failing to load it off disk
				bLoadHasFailed = true;
				return EAsyncPackageState::TimeOut;
			}

			AddObjectReference(Package);
			LinkerRoot = Package;
		}
		FScopeCycleCounterUObject ConstructorScope(Package, GET_STATID(STAT_FAsyncPackage_CreateLinker));

		if (Package->GetLoadedPath().IsEmpty() && !Package->bHasBeenFullyLoaded)
		{
			SCOPED_LOADTIMER(CreateLinker_SetFlags);
			// We just created the package, so set ownership flag and set up package info
			bCreatedLinkerRoot = true;

			// Set package specific data 
			Package->SetPackageFlags(Desc.PackageFlags);
			Package->SetPIEInstanceID(Desc.PIEInstanceID);

			// Always store package filename we loading from
			Package->SetLoadedPath(Desc.PackagePath);
#if WITH_EDITORONLY_DATA
			// Assume all packages loaded through async loading are required by runtime
			Package->SetLoadedByEditorPropertiesOnly(false);
#endif
		}

		LastObjectWorkWasPerformedOn = Package;
		// if the linker already exists, we don't need to lookup the file (it may have been pre-created with
		// a different filename)
		{
			SCOPED_LOADTIMER(CreateLinker_FindLinker);
			Linker = FLinkerLoad::FindExistingLinkerForPackage(Package);
		}
		if (Linker)
		{
			if (GEventDrivenLoaderEnabled)
			{
				// this almost works, but the EDL does not tolerate redoing steps it already did
				UE_LOG(LogStreaming, Fatal, TEXT("Package %s was reloaded before it even closed the linker from a previous load. Seems like a waste of time eh?"), *Desc.Name.ToString());
				check(Package);
				FWeakAsyncPackagePtr WeakPtr(this);
				AsyncLoadingThread.GetPrecacheHandler().RegisterNewSummaryRequest(this);
				AsyncLoadingThread.GetPrecacheHandler().SummaryComplete(WeakPtr);
			}
		}

		if (!Linker)
		{
			// Process any package redirects
			FPackagePath PackagePath(Desc.PackagePath);
			FString NameToLoad = PackagePath.GetPackageName();
			{
				SCOPED_LOADTIMER(CreateLinker_GetRedirectedName);
				FName FNameToLoad(*NameToLoad);
				const FCoreRedirectObjectName NewPackageName = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Package, FCoreRedirectObjectName(NAME_None, NAME_None, FNameToLoad));
				if (NewPackageName.PackageName != FNameToLoad)
				{
					NameToLoad = NewPackageName.PackageName.ToString();
					if (!FPackagePath::TryFromMountedName(NameToLoad, PackagePath))
					{
						UE_LOG(LogStreaming, Error, TEXT("PackagePath %s requested by async loading code (PackageName: %s) has invalid FCoreRedirects redirector to unmounted contentroot %s."), *Desc.PackagePath.GetPackageName(), *Desc.Name.ToString(), *NameToLoad);
						bLoadHasFailed = true;
						return EAsyncPackageState::TimeOut;
					}
				}
			}

			// The editor must not redirect packages for localization.
			if (!GIsEditor)
			{
				SCOPED_LOADTIMER(CreateLinker_MassagePath);
				// Allow delegates to resolve this path
				FString PackageNameToLoad = FPackageName::GetDelegateResolvedPackagePath(NameToLoad);
				PackageNameToLoad = FPackageName::GetLocalizedPackagePath(PackageNameToLoad);
				if (PackageNameToLoad != NameToLoad)
				{
					NameToLoad = MoveTemp(PackageNameToLoad);
					if (!FPackagePath::TryFromMountedName(NameToLoad, PackagePath))
					{
						UE_LOG(LogStreaming, Error, TEXT("PackagePath %s requested by async loading code (PackageName: %s) has invalid Localization redirector to unmounted contentroot %s."), *Desc.PackagePath.GetPackageName(), *Desc.Name.ToString(), *NameToLoad);
						bLoadHasFailed = true;
						return EAsyncPackageState::TimeOut;
					}
				}
			}

			FName NameToLoadFName = PackagePath.GetPackageFName();
			{
				bool bDoesPackageExist = false;
				{
					SCOPED_LOADTIMER(CreateLinker_DoesExist);
					// Only look for packages on disk
					bDoesPackageExist = FPackageName::DoesPackageExistEx(PackagePath, FPackageName::EPackageLocationFilter::FileSystem, false /* bMatchCaseOnDisk */, &PackagePath) != FPackageName::EPackageLocationFilter::None;
				}

				if (!bDoesPackageExist)
				{
					SCOPED_LOADTIMER(CreateLinker_MissingPackage);

					if (!FLinkerLoad::IsKnownMissingPackage(NameToLoadFName))
					{
						UE_LOG(LogStreaming, Error, TEXT("Couldn't find file for package %s requested by async loading code. NameToLoad: %s"), *Desc.Name.ToString(), *Desc.PackagePath.GetDebugName());

#if !WITH_EDITORONLY_DATA
						UE_CLOG(bUseTimeLimit, LogStreaming, Error, TEXT("This will hitch streaming because it ends up searching the disk instead of finding the file in the pak file."));
#endif

						if (GEventDrivenLoaderEnabled)
						{
							TSet<FName> DependentPackages;
							FWeakAsyncPackagePtr WeakPtr(this);
							TArray<FEventLoadNodePtr> AddedNodes;
							EventNodeArray.GetAddedNodes(AddedNodes, this);
							for (const FEventLoadNodePtr& NodePtr : AddedNodes)
							{
								const FEventLoadNode& Node = EventNodeArray.GetNode(NodePtr);
								for (const FEventLoadNodePtr& Other : Node.NodesWaitingForMe)
								{
									FName DependentPackageName = Other.WaitingPackage.HumanReadableStringForDebugging();
									if (DependentPackageName != NAME_None)
									{
										DependentPackages.Add(DependentPackageName);
									}
								}
							}

							UE_LOG(LogStreaming, Error, TEXT("Found %d dependent packages..."), DependentPackages.Num());
							for (const FName& DependentPackageName : DependentPackages)
							{
								UE_LOG(LogStreaming, Error, TEXT("  %s"), *DependentPackageName.ToString());
							}
						}

						// Add to known missing list so it won't error again
						FLinkerLoad::AddKnownMissingPackage(NameToLoadFName);
					}

					bLoadHasFailed = true;
					return EAsyncPackageState::TimeOut;
				}
			}

			// Create raw async linker, requiring to be ticked till finished creating.
			uint32 LinkerFlags = (LOAD_Async | LOAD_NoVerify);
#if WITH_EDITOR
			if ((!FApp::IsGame() || GIsEditor) && (Desc.PackageFlags & PKG_PlayInEditor) != 0)
			{
				LinkerFlags |= LOAD_PackageForPIE;
			}
#endif
			SCOPED_LOADTIMER(CreateLinker_CreateLinkerAsync);
			FUObjectSerializeContext* LoadContext = GetSerializeContext();
			if (GEventDrivenLoaderEnabled)
			{
				FWeakAsyncPackagePtr WeakPtr(this);
				FPrecacheCallbackHandler* PrecacheHandler = &AsyncLoadingThread.GetPrecacheHandler();
				check(Package);
				Linker = FLinkerLoad::CreateLinkerAsync(LoadContext, Package, PackagePath, LinkerFlags, Desc.GetInstancingContext()
					, TFunction<void()>(
						[WeakPtr, PrecacheHandler]()
						{
							PrecacheHandler->SummaryComplete(WeakPtr);
						}
					));
				if (Linker)
				{
					AsyncLoadingThread.GetPrecacheHandler().RegisterNewSummaryRequest(this);
					if (!Linker->Loader)
					{
						AsyncLoadingThread.GetPrecacheHandler().SummaryComplete(WeakPtr);
						bLoadHasFailed = true;
					}
				}
			}
			else
			{
				Linker = FLinkerLoad::CreateLinkerAsync(LoadContext, Package, PackagePath, LinkerFlags, Desc.GetInstancingContext(), TFunction<void()>([]() {}));
			}
		}

		// Associate this async package with the linker
		check(Linker);
		check(Linker->AsyncRoot == nullptr || Linker->AsyncRoot == this);
		Linker->AsyncRoot = this;

		UE_LOG(LogStreaming, Verbose, TEXT("FAsyncPackage::CreateLinker for %s finished."), *Desc.PackagePath.GetDebugName());
		TRACE_LOADTIME_ASYNC_PACKAGE_LINKER_ASSOCIATION(this, Linker);
	}
	return EAsyncPackageState::Complete;
}

/**
 * Finalizes linker creation till time limit is exceeded.
 *
 * @return true if linker is finished being created, false otherwise
 */
EAsyncPackageState::Type FAsyncPackage::FinishLinker()
{
	LLM_SCOPE(ELLMTag::AsyncLoading);

	SCOPED_LOADTIMER(FinishLinkerTime);
	EAsyncPackageState::Type Result = EAsyncPackageState::Complete;
	if (Linker && !Linker->HasFinishedInitialization())
	{
		SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_FinishLinker);
		LastObjectWorkWasPerformedOn	= LinkerRoot;
		LastTypeOfWorkPerformed			= TEXT("ticking linker");
	
		const float RemainingTimeLimit = TimeLimit - (float)(FPlatformTime::Seconds() - TickStartTime);

		// Operation still pending if Tick returns false
		FLinkerLoad::ELinkerStatus LinkerResult = Linker->Tick(RemainingTimeLimit, bUseTimeLimit, bUseFullTimeLimit, &ObjectNameWithOuterToExport);
		if (LinkerResult != FLinkerLoad::LINKER_Loaded)
		{ 
			// Give up remainder of timeslice if there is one to give up.
			GiveUpTimeSlice();
			Result = EAsyncPackageState::TimeOut;
			if (LinkerResult == FLinkerLoad::LINKER_Failed)
			{
				// If linker failed we exit with EAsyncPackageState::TimeOut to skip all the remaining steps.
				// The error will be handled as bLoadHasFailed will be true.
				bLoadHasFailed = true;
			}
		}
	}

	return Result;
}

/**
 * Adds a package to the list of pending import packages.
 *
 * @param PendingImport Name of the package imported either directly or by one of the imported packages
 * @param PackageToLoad Name of the package to load into PendingImport
 */
void FAsyncPackage::AddImportDependency(const FName& PendingImport, const FName& PackageToLoad, FLinkerInstancingContext InstancingContext)
{
	FAsyncPackage* PackageToStream = AsyncLoadingThread.FindAsyncPackage(PendingImport);
	const bool bReinsert = PackageToStream != nullptr;

	if (!PackageToStream)
	{
		FPackagePath ImportPackagePath = FPackagePath::FromPackageNameChecked(!PackageToLoad.IsNone() ? PackageToLoad : PendingImport);
		FAsyncPackageDesc Info(INDEX_NONE, PendingImport, ImportPackagePath);
#if UE_WITH_PACKAGE_ACCESS_TRACKING
		Info.ReferencerPackageName = GetPackageName();
		Info.ReferencerPackageOp = PackageAccessTrackingOps::NAME_Load;
#endif
#if WITH_EDITOR
		Info.CookLoadType = ECookLoadType::Unexpected;
#endif
		Info.SetInstancingContext(MoveTemp(InstancingContext));
		PackageToStream = new FAsyncPackage(AsyncLoadingThread, Info, EDLBootNotificationManager);

		// If priority of the dependency is not set, inherit from parent.
		if (PackageToStream->Desc.Priority == 0)
		{
			PackageToStream->Desc.Priority = Desc.Priority;
		}
	}

	if (!bReinsert)
	{
		TRACE_LOADTIME_ASYNC_PACKAGE_IMPORT_DEPENDENCY(this, PackageToStream);
		AsyncLoadingThread.InsertPackage(PackageToStream, bReinsert);
	}

	// Add dependent requests (any request that depends on this package will also depends on this package dependencies)
	AddDependentRequestsTo(PackageToStream);

	if (!PackageToStream->HasFinishedLoading() && 
		!PackageToStream->bLoadHasFailed)
	{
		const bool bInternalCallback = true;
		TUniquePtr<FLoadPackageAsyncDelegate> InternalDelegate = MakeUnique<FLoadPackageAsyncDelegate>(FLoadPackageAsyncDelegate::CreateRaw(this, &FAsyncPackage::ImportPreLoadedAsyncCallback));
		PackageToStream->AddCompletionCallback(MoveTemp(InternalDelegate), bInternalCallback);
		PackageToStream->DependencyRefCount.Increment();
		PendingImportedPackages.Emplace(PackageToStream->GetPackageName(), PackageToStream);
	}
	else
	{
		PackageToStream->DependencyRefCount.Increment();
		ReferencedImports.Add(PackageToStream);
	}
}


/**
 * Adds a unique package to the list of packages to wait for until their linkers have been created.
 *
 * @param PendingImport Package imported either directly or by one of the imported packages
 */
bool FAsyncPackage::AddUniqueLinkerDependencyPackage(FAsyncPackage& PendingImport)
{
	if (!PendingImportedPackages.Contains(PendingImport.GetPackageName()))
	{
		FLinkerLoad* PendingImportLinker = PendingImport.Linker;
		if (PendingImportLinker == nullptr || !PendingImportLinker->HasFinishedInitialization())
		{
			AddImportDependency(PendingImport.GetPackageName(), NAME_None, FLinkerInstancingContext());
			UE_LOG(LogStreaming, Verbose, TEXT("  Adding linker dependency %s"), *PendingImport.GetPackageName().ToString());
		}
		else if (this != &PendingImport)
		{
			return false;
		}
	}
	return true;
}

/**
 * Adds dependency tree to the list if packages to wait for until their linkers have been created.
 *
 * @param ImportedPackage Package imported either directly or by one of the imported packages
 */
void FAsyncPackage::AddDependencyTree(FAsyncPackage& ImportedPackage, TSet<FAsyncPackage*>& SearchedPackages)
{
	if (SearchedPackages.Contains(&ImportedPackage))
	{
		// we've already searched this package
		return;
	}
	for (TTuple<FName, FAsyncPackage*>& Kvp : ImportedPackage.PendingImportedPackages)
	{
		FAsyncPackage& PendingImport = *Kvp.Value;
		if (!AddUniqueLinkerDependencyPackage(PendingImport))
		{
			AddDependencyTree(PendingImport, SearchedPackages);
		}
	}
	// Mark this package as searched
	SearchedPackages.Add(&ImportedPackage);
}

/** 
 * Load imports till time limit is exceeded.
 *
 * @return true if we finished load all imports, false otherwise
 */
EAsyncPackageState::Type FAsyncPackage::LoadImports()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FAsyncPackage::LoadImports);
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_LoadImports);
	LastObjectWorkWasPerformedOn	= LinkerRoot;
	LastTypeOfWorkPerformed			= TEXT("loading imports");

	// GC can't run in here
	FGCScopeGuard GCGuard;

	// Create imports.
	while (LoadImportIndex < Linker->ImportMap.Num() && !IsTimeLimitExceeded())
	{
		// Get the package for this import
		FObjectImport* Import = &Linker->ImportMap[LoadImportIndex++];

		while (Import->OuterIndex.IsImport())
		{
			Import = &Linker->Imp(Import->OuterIndex);
		}
	
		// @todo: why do we need this? some UFunctions have null outer in the linker.
		if (Import->ClassName != NAME_Package && !Import->HasPackageName())
		{
			continue;
		}

		// This may be an import left behind from a core redirects fixup
		if (Import->ObjectName.IsNone())
		{
			continue;
		}

#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
		if (FLinkerLoad::IsImportLazyLoadEnabled())
		{
			using namespace UE::LinkerLoad;
			if (GetPropertyImportLoadBehavior(*Import, *Linker) == EImportBehavior::LazyOnDemand)
			{
				continue;
			}
		}
#endif

		// Our import package name is the import name itself if import is a package, else the specified package name
		const FLinkerInstancingContext& InstancingContext = Linker->GetInstancingContext();
		FName ImportToLoad = !Import->HasPackageName() ? Import->ObjectName : Import->GetPackageName();
		FName ImportPackageFName = InstancingContext.RemapPackage(ImportToLoad);

		// Don't try to import a package that is in an import table that we know is an invalid entry or if it was filtered out
		// by the instancing context.
		if ((ImportPackageFName.IsNone() && !ImportToLoad.IsNone()) || FLinkerLoad::IsKnownMissingPackage(ImportPackageFName))
		{
			continue;
		}

		// Handle circular dependencies - try to find existing packages.
		UPackage* ExistingPackage = dynamic_cast<UPackage*>(StaticFindObjectFast(UPackage::StaticClass(), nullptr, ImportPackageFName, true));
		if (ExistingPackage && !ExistingPackage->bHasBeenFullyLoaded && !IsNativeCodePackage(ExistingPackage))
		{
			// The import package already exists. Check if it's currently being streamed as well. If so, make sure
			// we add all dependencies that don't yet have linkers created otherwise we risk that if the current package
			// doesn't depend on any other packages that have not yet started streaming, creating imports is going
			// to load packages blocking the main thread.
			FAsyncPackage* PendingPackage = AsyncLoadingThread.FindAsyncPackage(ImportPackageFName);
			if (PendingPackage)
			{
				FLinkerLoad* PendingPackageLinker = PendingPackage->Linker;
				if (PendingPackageLinker == nullptr || !PendingPackageLinker->HasFinishedInitialization())
				{
					// Add this import to the dependency list.
					AddUniqueLinkerDependencyPackage(*PendingPackage);
				}
				else
				{
					UE_LOG(LogStreaming, Verbose, TEXT("FAsyncPackage::LoadImports for %s: Linker exists for %s"), *Desc.PackagePath.GetDebugName(), *ImportPackageFName.ToString());
					// Only keep a reference to this package so that its linker doesn't go away too soon
					PendingPackage->DependencyRefCount.Increment();
					ReferencedImports.Add(PendingPackage);
					// Check if we need to add its dependencies too.
					TSet<FAsyncPackage*> SearchedPackages;
					AddDependencyTree(*PendingPackage, SearchedPackages);
				}
			}
		}

		if (!ExistingPackage && !PendingImportedPackages.Contains(ImportPackageFName))
		{
			const FString ImportPackageName(ImportPackageFName.ToString());
			// The package doesn't exist and this import is not in the dependency list so add it now.
			if (!FPackageName::IsShortPackageName(ImportPackageName))
			{
				bool bLoadedFromIoStore = false;
				if (IAsyncPackageLoader* IoStorePackageLoader = AsyncLoadingThread.GetIoStorePackageLoader())
				{
					FPackagePath ImportedPackagePath = FPackagePath::FromPackageNameUnchecked(ImportPackageFName);
					FPackageName::EPackageLocationFilter PackageLocation = FPackageName::EPackageLocationFilter(uint8(FPackageName::EPackageLocationFilter::IoDispatcher) | uint8(FPackageName::EPackageLocationFilter::FileSystem));
					PackageLocation = FPackageName::DoesPackageExistEx(ImportedPackagePath, PackageLocation, false /* bMatchCaseOnDisk */, nullptr);
					if (PackageLocation == FPackageName::EPackageLocationFilter::IoDispatcher)
					{
						UE_LOG(LogStreaming, Verbose, TEXT("FAsyncPackage::LoadImports for %s: Loading %s (0x%llX) using IoStore package loader"), *Desc.PackagePath.GetDebugName(), *ImportPackageName, FPackageId::FromName(ImportPackageFName).ValueForDebugging());
						check(IsInGameThread());
						FAsyncLoadingThread::LeaveAsyncLoadingTick(AsyncLoadingThread.GetThreadIndex()); // Fix thread check that triggers if we recurse back into the uncooked package loader from here
						int32 ImportRequestId = IoStorePackageLoader->LoadPackage(ImportedPackagePath, NAME_None, FLoadPackageAsyncDelegate(), PKG_None, INDEX_NONE, 0, nullptr, LOAD_None);
						IoStorePackageLoader->FlushLoading({ImportRequestId});
						FAsyncLoadingThread::EnterAsyncLoadingTick(AsyncLoadingThread.GetThreadIndex());
						bLoadedFromIoStore = true;
					}
				}
				if (!bLoadedFromIoStore)
				{
					UE_LOG(LogStreaming, Verbose, TEXT("FAsyncPackage::LoadImports for %s: Loading %s"), *Desc.PackagePath.GetDebugName(), *ImportPackageName);
					AddImportDependency(ImportPackageFName, ImportToLoad, ImportToLoad != ImportPackageFName ? InstancingContext : FLinkerInstancingContext());
				}
			}
			else
			{
				// This usually means there's a reference to a script package from another project
				UE_LOG(LogStreaming, Warning, TEXT("FAsyncPackage::LoadImports for %s: Short package name in imports list: %s"), *Desc.PackagePath.GetDebugName(), *ImportPackageName);
			}
		}
		UpdateLoadPercentage();
	}
			
	
	if (PendingImportedPackages.Num())
	{
		GiveUpTimeSlice();
		return EAsyncPackageState::PendingImports;
	}
	return LoadImportIndex == Linker->ImportMap.Num() ? EAsyncPackageState::Complete : EAsyncPackageState::TimeOut;
}

/**
 * this callback is triggered once the package exports are created and de-serialized but not post loaded
 */
void FAsyncPackage::ImportPreLoadedAsyncCallback(const FName& InPackageName, UPackage* LoadedPackage, EAsyncLoadingResult::Type Result)
{
	// @note FH: the array here should only be accessed from the async thread
	check(IsInAsyncLoadingThread());
	if (Result != EAsyncLoadingResult::Canceled)
	{
		UE_LOG(LogStreaming, Verbose, TEXT("FAsyncPackage::LoadImports for %s: Loaded %s"), *Desc.PackagePath.GetDebugName(), *InPackageName.ToString());
		FAsyncPackage** AsyncPackage = PendingImportedPackages.Find(InPackageName);
		if (AsyncPackage != nullptr)
		{
			// Keep a reference to this package so that its linker doesn't go away too soon
			ReferencedImports.Add(*AsyncPackage);
			PendingImportedPackages.Remove(InPackageName);
		}
	}
}

/** 
 * Create imports till time limit is exceeded.
 *
 * @return true if we finished creating all imports, false otherwise
 */
EAsyncPackageState::Type FAsyncPackage::CreateImports()
{
	SCOPED_LOADTIMER(CreateImportsTime);
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_CreateImports);
	// GC can't run in here
	FGCScopeGuard GCGuard;

	// Create imports.
	while( ImportIndex < Linker->ImportMap.Num() && !IsTimeLimitExceeded() )
	{

#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
		if (FLinkerLoad::IsImportLazyLoadEnabled())
		{
			FObjectImport& Import = Linker->ImportMap[ImportIndex];
			using namespace UE::LinkerLoad;
			if (GetPropertyImportLoadBehavior(Import, *Linker) == EImportBehavior::LazyOnDemand)
			{
				ImportIndex++;
				continue;
			}
		}
#endif

 		UObject* Object	= Linker->CreateImport( ImportIndex++ );
		LastObjectWorkWasPerformedOn	= Object;
		LastTypeOfWorkPerformed			= TEXT("creating imports for");

		// Make sure this object is not claimed by GC if it's triggered while streaming.
		AddObjectReference(Object);

		// Keep track of all imported packages that are also being loaded so that we can wait until they also finished serializing their exports
		if (UPackage* ImportedPackage = Cast<UPackage>(Object))
		{
			if (ImportedPackage->GetLinker() && ImportedPackage->GetLinker()->AsyncRoot)
			{
				ImportedPackages.Add(ImportedPackage);
			}
		}
	}

	return ImportIndex == Linker->ImportMap.Num() ? EAsyncPackageState::Complete : EAsyncPackageState::TimeOut;
}

#if WITH_EDITORONLY_DATA
/**
* Creates and loads meta-data for the package.
*
* @return true if we finished creating meta-data, false otherwise.
*/
EAsyncPackageState::Type FAsyncPackage::CreateMetaData()
{
	SCOPED_LOADTIMER(CreateMetaDataTime);
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_CreateMetaData);

	if (!MetaDataIndex.IsSet())
	{
		checkSlow(!FPlatformProperties::RequiresCookedData());
		MetaDataIndex = Linker->LoadMetaDataFromExportMap(false);
	}

	return EAsyncPackageState::Complete;
}
#endif // WITH_EDITORONLY_DATA

/**
 * Create exports till time limit is exceeded.
 *
 * @return true if we finished creating and preloading all exports, false otherwise.
 */
EAsyncPackageState::Type FAsyncPackage::CreateExports()
{
	SCOPED_LOADTIMER(CreateExportsTime);
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_CreateExports);

	// GC can't run in here
	FGCScopeGuard GCGuard;

	// Create exports.
	while( ExportIndex < Linker->ExportMap.Num() && !IsTimeLimitExceeded() )
	{
#if WITH_EDITORONLY_DATA
		checkf(MetaDataIndex.IsSet(), TEXT("FAsyncPackage::CreateExports called before FAsyncPackage::CreateMetaData!"));
		if (ExportIndex == MetaDataIndex.GetValue())
		{
			++ExportIndex;
			continue;
		}
#endif // WITH_EDITORONLY_DATA

		const FObjectExport& Export = Linker->ExportMap[ExportIndex];
		// Precache data and see whether it's already finished.
		bool bReady;

		FAsyncArchive* AsyncLoader = Linker->GetAsyncLoader();
		if (AsyncLoader)
		{
			bReady = AsyncLoader->PrecacheWithTimeLimit(Export.SerialOffset, Export.SerialSize, bUseTimeLimit, bUseFullTimeLimit, TickStartTime, TimeLimit);
		}
		else
		{
			bReady = Linker->Precache(Export.SerialOffset, Export.SerialSize);
		}
		if (bReady)
		{
			// Create the object...
			UObject* Object	= Linker->CreateExport( ExportIndex++ );
			// ... and preload it.
			if( Object )
			{				
				// This will cause the object to be serialized. We do this here for all objects and
				// not just UClass and template objects, for which this is required in order to ensure
				// seek free loading, to be able introduce async file I/O.
				Linker->Preload(Object);
				PackageObjLoaded.Add(Object);
			}
			LastObjectWorkWasPerformedOn	= Object;
			LastTypeOfWorkPerformed = TEXT("creating exports for");
				
			UpdateLoadPercentage();
		}
		// Data isn't ready yet. Give up remainder of time slice if we're not using a time limit.
		else if (GiveUpTimeSlice())
		{
			INC_FLOAT_STAT_BY(STAT_AsyncIO_AsyncPackagePrecacheWaitTime, (float)FApp::GetDeltaTime());
			return EAsyncPackageState::TimeOut;
		}
	}
	
	// We no longer need the referenced packages.
	FreeReferencedImports();

	EAsyncPackageState::Type Result = (ExportIndex == Linker->ExportMap.Num() ? EAsyncPackageState::Complete : EAsyncPackageState::TimeOut);
	if (Result == EAsyncPackageState::Complete)
	{
		bAllExportsSerialized = true;
	}

	return Result;
}

/**
 * Removes references to any imported packages.
 */
void FAsyncPackage::FreeReferencedImports()
{	
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_FreeReferencedImports);	

	for (int32 ReferenceIndex = 0; ReferenceIndex < ReferencedImports.Num(); ++ReferenceIndex)
	{
		FAsyncPackage& Ref = *ReferencedImports[ReferenceIndex];
		UE_LOG(LogStreaming, Verbose, TEXT("FAsyncPackage::FreeReferencedImports for %s: Releasing %s (%d)"), *Desc.PackagePath.GetDebugName(), *Ref.GetPackageName().ToString(), Ref.GetDependencyRefCount());
		int32 RefPackageDependencyRefCount = Ref.DependencyRefCount.Decrement();
		check(RefPackageDependencyRefCount >= 0);
	}
	ReferencedImports.Empty();
}

EAsyncPackageState::Type FAsyncPackage::PreLoadObjects()
{
	SCOPED_LOADTIMER(PreLoadObjectsTime);
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_PreLoadObjects);

	// GC can't run in here
	FGCScopeGuard GCGuard;

	FUObjectSerializeContext* LoadContext = GetSerializeContext();
	TArray<UObject*>& ThreadObjLoaded = LoadContext->PRIVATE_GetObjectsLoadedInternalUseOnly();
	PackageObjLoaded.Append(ThreadObjLoaded);
	ThreadObjLoaded.Reset();

	// Preload (aka serialize) the objects.
	while (PreLoadIndex < PackageObjLoaded.Num() && !IsTimeLimitExceeded())
	{
		//@todo async: make this part async as well.
		UObject* Object = PackageObjLoaded[PreLoadIndex++];
		if (Object && Object->GetLinker())
		{
			Object->GetLinker()->Preload(Object);
				LastObjectWorkWasPerformedOn = Object;
				LastTypeOfWorkPerformed = TEXT("preloading");
		}
	}

	PackageObjLoaded.Append(ThreadObjLoaded);
	ThreadObjLoaded.Reset();

	return PreLoadIndex == PackageObjLoaded.Num() ? EAsyncPackageState::Complete : EAsyncPackageState::TimeOut;
}

EAsyncPackageState::Type FAsyncPackage::FinishExternalReadDependencies()
{
	if (IsTimeLimitExceeded())
	{
		return EAsyncPackageState::TimeOut;
	}

	LastTypeOfWorkPerformed = TEXT("ExternalReadDependencies");
	
	double RemainingTime = FMath::Max<double>(MIN_REMAIN_TIME, TimeLimit - (FPlatformTime::Seconds() - TickStartTime));
		
	FLinkerLoad* VisitedLinkerLoad = nullptr;
	while (FinishExternalReadDependenciesIndex < PackageObjLoaded.Num())
	{
		UObject* Obj = PackageObjLoaded[FinishExternalReadDependenciesIndex];
		FLinkerLoad* LinkerLoad = Obj ? Obj->GetLinker() : nullptr;
		if (LinkerLoad && LinkerLoad != VisitedLinkerLoad)
		{
			if (!LinkerLoad->FinishExternalReadDependencies(bUseTimeLimit ? RemainingTime : 0.0))
			{
				return EAsyncPackageState::TimeOut;
			}
					
			VisitedLinkerLoad = LinkerLoad;
					
			// Update remaining time 
			if (bUseTimeLimit)
			{
				RemainingTime = TimeLimit - (FPlatformTime::Seconds() - TickStartTime);
				if (RemainingTime <= 0.0)
				{
					return EAsyncPackageState::TimeOut;
				}
			}
		}
		FinishExternalReadDependenciesIndex++;
	}
		
	return EAsyncPackageState::Complete;
}

/**
 * Route PostLoad to all loaded objects. This might load further objects!
 *
 * @return true if we finished calling PostLoad on all loaded objects and no new ones were created, false otherwise
 */
EAsyncPackageState::Type FAsyncPackage::PostLoadObjects()
{
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_PostLoadObjects);

	SCOPED_LOADTIMER(PostLoadObjectsTime);

	// GC can't run in here
	FGCScopeGuard GCGuard;

	FUObjectThreadContext& ThreadContext = FUObjectThreadContext::Get();
	TGuardValue<bool> GuardIsRoutingPostLoad(ThreadContext.IsRoutingPostLoad, true);

	FUObjectSerializeContext* LoadContext = GetSerializeContext();
	TArray<UObject*>& ThreadObjLoaded = LoadContext->PRIVATE_GetObjectsLoadedInternalUseOnly();
	if (ThreadObjLoaded.Num())
	{
		// New objects have been loaded. They need to go through PreLoad first so exit now and come back after they've been preloaded.
		PackageObjLoaded.Append(ThreadObjLoaded);
		ThreadObjLoaded.Reset();
		return EAsyncPackageState::TimeOut;
	}

	if (GEventDrivenLoaderEnabled)
	{
		PreLoadIndex = PackageObjLoaded.Num(); // we did preloading in a different way and never incremented this
	}

	const bool bAsyncPostLoadEnabled = FAsyncLoadingThreadSettings::Get().bAsyncPostLoadEnabled;

	TOptional<FScopedSlowTask> SlowTask;
	if (IsInGameThread())
	{
		SlowTask.Emplace(1.0f, NSLOCTEXT("AsyncLoading", "PostloadingObjects", "Postloading objects..."));
	}

	// PostLoad objects.
	TRACE_LOADTIME_POSTLOAD_SCOPE;
	while (PostLoadIndex < PackageObjLoaded.Num() && PostLoadIndex < PreLoadIndex && !IsTimeLimitExceeded())
	{
		UObject* Object = PackageObjLoaded[PostLoadIndex++];
		if (Object)
		{
			if (!Object->IsReadyForAsyncPostLoad())
			{
				--PostLoadIndex;
				break;
			}
			else if (bAsyncPostLoadEnabled && CanPostLoadOnAsyncLoadingThread(Object))
			{
				SCOPED_ACCUM_LOADTIME(PostLoad, StaticGetNativeClassName(Object->GetClass()));

				// We want this check only with EDL enabled
				check(!GEventDrivenLoaderEnabled || !Object->HasAnyFlags(RF_NeedLoad));

				ThreadContext.CurrentlyPostLoadedObjectByALT = Object;
				Object->ConditionalPostLoad();
				ThreadContext.CurrentlyPostLoadedObjectByALT = nullptr;

				LastObjectWorkWasPerformedOn = Object;
				LastTypeOfWorkPerformed = TEXT("postloading_async");

				if (ThreadObjLoaded.Num())
				{
					// New objects have been loaded. They need to go through PreLoad first so exit now and come back after they've been preloaded.
					PackageObjLoaded.Append(ThreadObjLoaded);
					ThreadObjLoaded.Reset();
					return EAsyncPackageState::TimeOut;
				}
			}
			else
			{
				DeferredPostLoadObjects.Add(Object);
			}
			// All object must be finalized on the game thread
			DeferredFinalizeObjects.Add(Object);
			check(Object->IsValidLowLevelFast());
			// Make sure all objects in DeferredFinalizeObjects are referenced too
			AddObjectReference(Object);

			if (SlowTask)
			{
				// Don't report progress but gives a chance to tick slate to improve the responsiveness of the 
				// progress bar being shown. We expect slate to be ticked at regular intervals throughout the loading.
				SlowTask->TickProgress();
			}
		}
	}

	PackageObjLoaded.Append(ThreadObjLoaded);
	ThreadObjLoaded.Reset();

	// New objects might have been loaded during PostLoad.
	EAsyncPackageState::Type Result = (PreLoadIndex == PackageObjLoaded.Num()) && (PostLoadIndex == PackageObjLoaded.Num()) ? EAsyncPackageState::Complete : EAsyncPackageState::TimeOut;
	return Result;
}

void CreateClustersFromPackage(FLinkerLoad* PackageLinker, TArray<UObject*>& OutClusterObjects);

EAsyncPackageState::Type FAsyncPackage::PostLoadDeferredObjects(double InTickStartTime, bool bInUseTimeLimit, double InOutTimeLimit)
{
	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_PostLoadObjectsGameThread);
	SCOPED_LOADTIMER(PostLoadDeferredObjectsTime);

	FAsyncPackageScope PackageScope(this);
	++ReentryCount;

	EAsyncPackageState::Type Result = EAsyncPackageState::Complete;
	FAsyncLoadingTickScope InAsyncLoadingTick(AsyncLoadingThread);

#if WITH_EDITOR
	UE::Core::Private::FPlayInEditorLoadingScope PlayInEditorIDScope(Desc.PIEInstanceID);
#endif

	FUObjectSerializeContext* LoadContext = GetSerializeContext();
	TArray<UObject*>& ObjLoadedInPostLoad = LoadContext->PRIVATE_GetObjectsLoadedInternalUseOnly();
	TArray<UObject*> ObjLoadedInPostLoadLocal;

	// It may happen that the last tick timed out before having time to preload some objects
	// and these will continue on the next tick. But we don't want to mistakenly think
	// they were loaded as part of the postload we're going to execute in this function.
	// So clear the list now and restore it for the next tick when we exit this function.
	TArray<UObject*> LastTickLeftOver = MoveTemp(ObjLoadedInPostLoad);
	ON_SCOPE_EXIT{ ObjLoadedInPostLoad.Append(LastTickLeftOver); };

	STAT(double PostLoadStartTime = FPlatformTime::Seconds());

	{
		TGuardValue<bool> GuardIsRoutingPostLoad(PackageScope.ThreadContext.IsRoutingPostLoad, true);

		TRACE_LOADTIME_POSTLOAD_SCOPE;
		while (DeferredPostLoadIndex < DeferredPostLoadObjects.Num() &&
			!AsyncLoadingThread.IsAsyncLoadingSuspendedInternal() &&
			!::IsTimeLimitExceeded(InTickStartTime, bInUseTimeLimit, InOutTimeLimit, LastTypeOfWorkPerformed, LastObjectWorkWasPerformedOn))
		{
			UObject* Object = DeferredPostLoadObjects[DeferredPostLoadIndex++];
			check(Object);

			if (!Object->IsReadyForAsyncPostLoad())
			{
				--DeferredPostLoadIndex;
				break;
			}

			LastObjectWorkWasPerformedOn = Object;
			LastTypeOfWorkPerformed = TEXT("postloading_gamethread");

			FScopeCycleCounterUObject ConstructorScope(Object, GET_STATID(STAT_FAsyncPackage_PostLoadObjectsGameThread));

			PackageScope.ThreadContext.CurrentlyPostLoadedObjectByALT = Object;
			Object->ConditionalPostLoad();
			PackageScope.ThreadContext.CurrentlyPostLoadedObjectByALT = nullptr;

			if (ObjLoadedInPostLoad.Num())
			{
				// If there were any LoadObject calls inside of PostLoad, we need to pre-load those objects here. 
				// There's no going back to the async tick loop from here.
				UE_LOG(LogStreaming, Log, TEXT("Detected %d objects loaded in PostLoad while streaming, this may cause hitches as we're blocking async loading to pre-load them."), ObjLoadedInPostLoad.Num());

				// Copy to local array because ObjLoadedInPostLoad can change while we're iterating over it
				ObjLoadedInPostLoadLocal.Append(ObjLoadedInPostLoad);
				ObjLoadedInPostLoad.Reset();

				while (ObjLoadedInPostLoadLocal.Num())
				{
					// Make sure all objects loaded in PostLoad get post-loaded too
					DeferredPostLoadObjects.Append(ObjLoadedInPostLoadLocal);

					// Preload (aka serialize) the objects loaded in PostLoad.
					for (UObject* PreLoadObject : ObjLoadedInPostLoadLocal)
					{
						if (PreLoadObject && PreLoadObject->GetLinker())
						{
							PreLoadObject->GetLinker()->Preload(PreLoadObject);
							PackageObjLoaded.Add(PreLoadObject);
						}
					}

					// Other objects could've been loaded while we were preloading, continue until we've processed all of them.
					ObjLoadedInPostLoadLocal.Reset();
					ObjLoadedInPostLoadLocal.Append(ObjLoadedInPostLoad);
					ObjLoadedInPostLoad.Reset();
				}
			}

			LastObjectWorkWasPerformedOn = Object;

			UpdateLoadPercentage();
		}
	}
	// New objects might have been loaded during PostLoad.
	Result = (DeferredPostLoadIndex == DeferredPostLoadObjects.Num()) ? EAsyncPackageState::Complete : EAsyncPackageState::TimeOut;

	if (Result == EAsyncPackageState::Complete)
	{
		// Class specific object non-native initialization
		while (PostLoadInstanceIndex < PackageObjLoaded.Num() &&
			!AsyncLoadingThread.IsAsyncLoadingSuspendedInternal() &&
			!::IsTimeLimitExceeded(InTickStartTime, bInUseTimeLimit, InOutTimeLimit, LastTypeOfWorkPerformed, LastObjectWorkWasPerformedOn))
		{
			UObject* Object = PackageObjLoaded[PostLoadInstanceIndex++];
			check(Object);

			LastObjectWorkWasPerformedOn = Object;
			LastTypeOfWorkPerformed = TEXT("postloadinginstance_gamethread");

			UClass* ObjectClass = Object->GetClass();
			ObjectClass->PostLoadInstance(Object);
		}
	}

	INC_FLOAT_STAT_BY(STAT_FAsyncPackage_TotalPostLoadGameThread, (float)(FPlatformTime::Seconds() - PostLoadStartTime));
	Result = (PostLoadInstanceIndex == PackageObjLoaded.Num()) ? EAsyncPackageState::Complete : EAsyncPackageState::TimeOut;
	if (Result == EAsyncPackageState::Complete)
	{
		LastObjectWorkWasPerformedOn = nullptr;
		LastTypeOfWorkPerformed = TEXT("DeferredFinalizeObjects");
		TArray<UObject*> CDODefaultSubobjects;
		// Clear async loading flags (we still want RF_Async, but EInternalObjectFlags::AsyncLoading can be cleared)
		while (DeferredFinalizeIndex < DeferredFinalizeObjects.Num() &&
			((!AsyncLoadingThread.IsAsyncLoadingSuspendedInternal() && !::IsTimeLimitExceeded(InTickStartTime, bInUseTimeLimit, InOutTimeLimit, LastTypeOfWorkPerformed, LastObjectWorkWasPerformedOn))))
		{
			UObject* Object = DeferredFinalizeObjects[DeferredFinalizeIndex++];
			if (Object)
			{
				Object->AtomicallyClearInternalFlags(EInternalObjectFlags::AsyncLoading);
			}

			// CDO need special handling, no matter if it's listed in DeferredFinalizeObjects
			UObject* CDOToHandle = ((Object != nullptr) && Object->HasAnyFlags(RF_ClassDefaultObject)) ? Object : nullptr;

			// Clear AsyncLoading in CDO's subobjects.
			if(CDOToHandle != nullptr)
			{
				CDOToHandle->GetDefaultSubobjects(CDODefaultSubobjects);
				for (UObject* SubObject : CDODefaultSubobjects)
				{
					if (SubObject && SubObject->HasAnyInternalFlags(EInternalObjectFlags::AsyncLoading))
					{
						SubObject->AtomicallyClearInternalFlags(EInternalObjectFlags::AsyncLoading);
					}
				}
				CDODefaultSubobjects.Reset();
			}
		}
		::IsTimeLimitExceeded(InTickStartTime, bInUseTimeLimit, InOutTimeLimit, LastTypeOfWorkPerformed, LastObjectWorkWasPerformedOn);

		//@ note FH: this might currently get called multiple time, would need to had a check for being already finalized 
		if (DeferredFinalizeIndex == DeferredFinalizeObjects.Num())
		{
			DeferredFinalizeIndex = 0;
			DeferredFinalizeObjects.Reset();
			Result = EAsyncPackageState::Complete;
		}
		else
		{
			Result = EAsyncPackageState::TimeOut;
		}

		// Mark package as having been fully loaded and update load time.
		if (Result == EAsyncPackageState::Complete && LinkerRoot && !bLoadHasFailed)
		{
			LastObjectWorkWasPerformedOn = LinkerRoot;
			LastTypeOfWorkPerformed = TEXT("CreateClustersFromPackage");
			LinkerRoot->AtomicallyClearInternalFlags(EInternalObjectFlags::AsyncLoading);
			LinkerRoot->MarkAsFullyLoaded();			
			LinkerRoot->SetLoadTime((float)(FPlatformTime::Seconds() - LoadStartTime));

			if (Linker)
			{
				CreateClustersFromPackage(Linker, DeferredClusterObjects);
			}
			::IsTimeLimitExceeded(InTickStartTime, bInUseTimeLimit, InOutTimeLimit, LastTypeOfWorkPerformed, LastObjectWorkWasPerformedOn);
		}
	}

	--ReentryCount;
	return Result;
}

EAsyncPackageState::Type FAsyncPackage::CreateClusters(double InTickStartTime, bool bInUseTimeLimit, double InOutTimeLimit)
{
	LastObjectWorkWasPerformedOn = nullptr;
	LastTypeOfWorkPerformed = TEXT("CreateClusters");

	while (DeferredClusterIndex < DeferredClusterObjects.Num() &&
		(!AsyncLoadingThread.IsAsyncLoadingSuspendedInternal() && !::IsTimeLimitExceeded(InTickStartTime, bInUseTimeLimit, InOutTimeLimit, LastTypeOfWorkPerformed, LastObjectWorkWasPerformedOn)))
	{
		UObject* ClusterRootObject = DeferredClusterObjects[DeferredClusterIndex++];
		LastObjectWorkWasPerformedOn = ClusterRootObject;
		ClusterRootObject->CreateCluster();
	}

	EAsyncPackageState::Type Result;
	if (DeferredClusterIndex == DeferredClusterObjects.Num())
	{
		DeferredClusterIndex = 0;
		DeferredClusterObjects.Reset();
		Result = EAsyncPackageState::Complete;
	}
	else
	{
		Result = EAsyncPackageState::TimeOut;
	}

	LastObjectWorkWasPerformedOn = nullptr;

	return Result;
}

EAsyncPackageState::Type FAsyncPackage::FinishObjects()
{
	SCOPED_LOADTIMER(FinishObjectsTime);

	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_FinishObjects);
	LastObjectWorkWasPerformedOn	= nullptr;
	LastTypeOfWorkPerformed			= TEXT("finishing all objects");

	FUObjectSerializeContext* LoadContext = GetSerializeContext();
	check(!Linker || LoadContext == Linker->GetSerializeContext());		
	TArray<UObject*>& ThreadObjLoaded = LoadContext->PRIVATE_GetObjectsLoadedInternalUseOnly();

	EAsyncLoadingResult::Type LoadingResult;
	if (!bLoadHasFailed)
	{
		ThreadObjLoaded.Reset();
		LoadingResult = EAsyncLoadingResult::Succeeded;
	}
	else
	{		
		PackageObjLoaded.Append(ThreadObjLoaded);

		// Cleanup objects from this package only
		for (int32 ObjectIndex = PackageObjLoaded.Num() - 1; ObjectIndex >= 0; --ObjectIndex)
		{
			UObject* Object = PackageObjLoaded[ObjectIndex];
			if (Object && Object->GetOutermost()->GetFName() == Desc.Name)
			{
				Object->ClearFlags(RF_NeedPostLoad | RF_NeedLoad | RF_NeedPostLoadSubobjects);
				Object->MarkAsGarbage();
				PackageObjLoaded[ObjectIndex] = nullptr;
			}
		}

		// Clean up UPackage so it can't be found later
		if (LinkerRoot && !LinkerRoot->IsRooted())
		{
			if (bCreatedLinkerRoot)
			{
				LinkerRoot->ClearFlags(RF_NeedPostLoad | RF_NeedLoad | RF_NeedPostLoadSubobjects);
				LinkerRoot->MarkAsGarbage();
				LinkerRoot->Rename(*MakeUniqueObjectName(GetTransientPackage(), UPackage::StaticClass()).ToString(), nullptr, REN_DontCreateRedirectors | REN_DoNotDirty | REN_ForceNoResetLoaders | REN_NonTransactional);
			}
			DetachLinker();
		}

		LoadingResult = EAsyncLoadingResult::Failed;
	}

	// Simulate what EndLoad does.
	FLinkerManager::Get().DissociateImportsAndForcedExports(); //@todo: this should be avoidable

	// Keep the linkers to close until we finish loading and it's safe to close them too
	LoadContext->MoveDelayedLinkerClosePackages(DelayedLinkerClosePackages);

	if (Linker)
	{
		// Flush linker cache now to reduce peak memory usage (5.5-10x)
		// We shouldn't need it anyway at this point and even if something attempts to read in PostLoad, 
		// we're just going to re-cache then.
		Linker->FlushCache();
	}

	if (GEventDrivenLoaderEnabled)
	{
		const bool bInternalCallbacks = true;
		CallCompletionCallbacks(bInternalCallbacks, LoadingResult);
	}
	else
	{
		LoadContext->DetachFromLinkers();
	}

	return EAsyncPackageState::Complete;
}

void FAsyncPackage::CloseDelayedLinkers()
{
	// Close any linkers that have been opened as a result of blocking loads while async loading
	for (FLinkerLoad* LinkerToClose : DelayedLinkerClosePackages)
	{
		if (LinkerToClose->LinkerRoot != nullptr)
		{
			check(LinkerToClose);
			//check(LinkerToClose->LinkerRoot);
			if (GEventDrivenLoaderEnabled)
			{
				FLinkerLoad* LinkerToReset = FLinkerLoad::FindExistingLinkerForPackage(LinkerToClose->LinkerRoot);
				check(LinkerToReset == LinkerToClose);
				if (LinkerToReset && LinkerToReset->AsyncRoot)
				{
					UE_LOG(LogStreaming, Error, TEXT("Linker cannot be reset right now...leaking %s"), *LinkerToReset->GetArchiveName());
					continue;
				}

			}
			else
			{
				if (!LinkerToClose->HasAnyObjectsPendingLoad())
				{
					FLinkerManager::Get().ResetLoaders(LinkerToClose->LinkerRoot);
				}
				else
				{
					UE_LOG(LogStreaming, Warning, TEXT("Linker cannot be reset right now because it still has objects pending load...leaking %s"), *LinkerToClose->GetArchiveName());
					continue;
				}
			}
		}
		check(LinkerToClose->LinkerRoot == nullptr);
		check(LinkerToClose->AsyncRoot == nullptr);
	}
}

void FAsyncPackage::CallCompletionCallbacks(bool bInternal, EAsyncLoadingResult::Type LoadingResult)
{
	checkSlow(bInternal || !GetOwnerThread().IsInAsyncLoadThread());

#if WITH_EDITOR
	UE::Core::Private::FPlayInEditorLoadingScope PlayInEditorIDScope(Desc.PIEInstanceID);
#endif

	UPackage* LoadedPackage = (!bLoadHasFailed) ? LinkerRoot : nullptr;
	for (FCompletionCallback& CompletionCallback : CompletionCallbacks)
	{
		if (CompletionCallback.bIsInternal == bInternal && !CompletionCallback.bCalled)
		{
			CompletionCallback.bCalled = true;
			CompletionCallback.Callback->ExecuteIfBound(Desc.Name, LoadedPackage, LoadingResult);
		}
	}
}

UPackage* FAsyncPackage::GetLoadedPackage()
{
	UPackage* LoadedPackage = (!bLoadHasFailed) ? LinkerRoot : nullptr;
	return LoadedPackage;
}

void FAsyncPackage::Cancel()
{
	// Call any completion callbacks specified.
	bLoadHasFailed = true;
	const EAsyncLoadingResult::Type Result = EAsyncLoadingResult::Canceled;
	CallCompletionCallbacks(true, Result);
	CallCompletionCallbacks(false, Result);

	for (TPair<IAsyncReadRequest*, FExportIORequest> Request : PrecacheRequests)
	{
		delete Request.Key;
	}
	PrecacheRequests.Empty();
	ExportIndexToPrecacheRequest.Empty();

	PackagesIMayBeWaitingForBeforePostload.Empty();
	PackagesIAmWaitingForBeforePostload.Empty();
	OtherPackagesWaitingForMeBeforePostload.Empty();
	PackagesWaitingToLinkImports.Empty();

	EventNodeArray.TotalNumberOfNodesAdded = 0;
	EventNodeArray.TotalNumberOfImportExportNodes = 0;
	EventNodeArray.Shutdown();

	FUObjectSerializeContext* LoadContext = GetSerializeContext();
	if (LoadContext)
	{
		TArray<UObject*>& ThreadObjLoaded = LoadContext->PRIVATE_GetObjectsLoadedInternalUseOnly();
		if (ThreadObjLoaded.Num())
		{
			PackageObjLoaded.Append(ThreadObjLoaded);
			ThreadObjLoaded.Reset();
		}
	}

	{
		// Clear load flags from any referenced objects
		FScopeLock RereferncedObjectsLock(&ReferencedObjectsCritical);
		ClearFlagsAndDissolveClustersFromLoadedObjects(ReferencedObjects);
		ClearFlagsAndDissolveClustersFromLoadedObjects(PackageObjLoaded);
		ClearFlagsAndDissolveClustersFromLoadedObjects(DeferredFinalizeObjects);
	
		// Release references
		EmptyReferencedObjects();
		PackageObjLoaded.Empty();
		DeferredFinalizeObjects.Empty();
	}

	if (LinkerRoot)
	{
		if (Linker)
		{
			Linker->FlushCache();
		}
		if (bCreatedLinkerRoot)
		{
			LinkerRoot->ClearFlags(RF_WasLoaded);
			LinkerRoot->bHasBeenFullyLoaded = false;
			LinkerRoot->Rename(*MakeUniqueObjectName(GetTransientPackage(), UPackage::StaticClass()).ToString(), nullptr, REN_DontCreateRedirectors | REN_DoNotDirty | REN_ForceNoResetLoaders | REN_NonTransactional);
		}
		ResetLoader();
	}
	PreLoadIndex = 0;
	PreLoadSortIndex = 0;
	FinishExternalReadDependenciesIndex = 0;
}

void FAsyncPackage::AddCompletionCallback(TUniquePtr<FLoadPackageAsyncDelegate>&& Callback, bool bInternal)
{
	// This is to ensure that there is no one trying to subscribe to a already loaded package
	//check(!bLoadHasFinished && !bLoadHasFailed);
	CompletionCallbacks.Emplace(bInternal, MoveTemp(Callback));
}

void FAsyncPackage::UpdateLoadPercentage()
{
	// PostLoadCount is just an estimate to prevent packages to go to 100% too quickly
	// We may never reach 100% this way, but it's better than spending most of the load package time at 100%
	float NewLoadPercentage = 0.0f;
	if (Linker)
	{
		const int32 PostLoadCount = FMath::Max(DeferredPostLoadObjects.Num(), Linker->ImportMap.Num());
		NewLoadPercentage = 100.f * static_cast<float>(LoadImportIndex + ExportIndex + DeferredPostLoadIndex) / static_cast<float>(Linker->ExportMap.Num() + Linker->ImportMap.Num() + PostLoadCount);
	}
	else if (DeferredPostLoadObjects.Num() > 0)
	{
		NewLoadPercentage = static_cast<float>(DeferredPostLoadIndex) / static_cast<float>(DeferredPostLoadObjects.Num());
	}
	// It's also possible that we got so many objects to PostLoad that LoadPercantage will actually drop
	LoadPercentage = FMath::Max(NewLoadPercentage, LoadPercentage);
}

bool FAsyncLoadingThread::ShouldAlwaysLoadPackageAsync(const FPackagePath& InPackagePath)
{
	return FPlatformProperties::RequiresCookedData() && GEventDrivenLoaderEnabled && EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME;
}

int32 FAsyncLoadingThread::LoadPackage(const FPackagePath& PackagePath, FLoadPackageAsyncOptionalParams OptionalParams)
{
	if (OptionalParams.ProgressDelegate.IsValid())
	{
		UE_LOG(LogStreaming, Warning, TEXT("Progress delegate is only supported for zenloader. A CompletionDelegate should be used instead for this loader."));
	}

	FLoadPackageAsyncDelegate CompletionDelegate;
	if (OptionalParams.CompletionDelegate.IsValid())
	{
		CompletionDelegate = MoveTemp(*OptionalParams.CompletionDelegate.Get());
	}
	return LoadPackage(PackagePath, OptionalParams.CustomPackageName, MoveTemp(CompletionDelegate), OptionalParams.PackageFlags, OptionalParams.PIEInstanceID, OptionalParams.PackagePriority, OptionalParams.InstancingContext, OptionalParams.LoadFlags);
}

int32 FAsyncLoadingThread::LoadPackage(const FPackagePath& InPackagePath, FName InCustomName, FLoadPackageAsyncDelegate InCompletionDelegate, EPackageFlags InPackageFlags, int32 InPIEInstanceID, int32 InPackagePriority, const FLinkerInstancingContext* InInstancingContext, uint32 InLoadFlags)
{
	checkf(IsInGameThread(), TEXT("LoadPackageAsync is only thread-safe when using the zenloader (i.e. AsyncLoading2)."));

	static bool bOnce = false;
	if (!bOnce && GEventDrivenLoaderEnabled)
	{
		bOnce = true;
		FGCObject::StaticInit(); // otherwise this thing is created during async loading, but not associated with a package
	}

	if (!InPackagePath.IsMountedPath())
	{
		UE_LOG(LogStreaming, Warning, TEXT("Async loading code requires MountedPath PackagePaths (%s)."), *InPackagePath.GetDebugName());
		InCompletionDelegate.ExecuteIfBound(InPackagePath.GetPackageFName(), nullptr, EAsyncLoadingResult::Failed);
		return INDEX_NONE;
	}

	FName PackageName = InCustomName.IsNone() ? InPackagePath.GetPackageFName() : InCustomName;
	UE_SCOPED_COOK_STAT(PackageName, EPackageEventStatType::LoadPackage);

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if ( FCoreDelegates::OnAsyncLoadPackage.IsBound() )
	{
		FCoreDelegates::OnAsyncLoadPackage.Broadcast(PackageName.ToString());
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	if (FCoreDelegates::GetOnAsyncLoadPackage().IsBound())
	{
		FCoreDelegates::GetOnAsyncLoadPackage().Broadcast(PackageName.ToString());
	}

	// Generate new request ID and add it immediately to the global request list (it needs to be there before we exit
	// this function, otherwise it would be added when the packages are being processed on the async thread).
	int32 RequestID = IAsyncPackageLoader::GetNextRequestId();
	TRACE_LOADTIME_BEGIN_REQUEST(RequestID);
	AddPendingRequest(RequestID);

	// Allocate delegate on Game Thread, it is not safe to copy delegates by value on other threads
	TUniquePtr<FLoadPackageAsyncDelegate> CompletionDelegatePtr;
	if (InCompletionDelegate.IsBound())
	{
		CompletionDelegatePtr = MakeUnique<FLoadPackageAsyncDelegate>(MoveTemp(InCompletionDelegate));
	}

	// Add new package request
	FAsyncPackageDesc PackageDesc(RequestID, PackageName, InPackagePath, MoveTemp(CompletionDelegatePtr), InPackageFlags, InPIEInstanceID, InPackagePriority);
	if (InInstancingContext)
	{
		PackageDesc.SetInstancingContext(*InInstancingContext);
	}
#if UE_WITH_PACKAGE_ACCESS_TRACKING
	PackageAccessTracking_Private::FTrackedData* AccumulatedScopeData = PackageAccessTracking_Private::FPackageAccessRefScope::GetCurrentThreadAccumulatedData();
	if (AccumulatedScopeData)
	{
		PackageDesc.ReferencerPackageName = AccumulatedScopeData->PackageName;
		PackageDesc.ReferencerPackageOp = AccumulatedScopeData->OpName;
	}
#endif
#if WITH_EDITOR
	PackageDesc.CookLoadType = FCookLoadScope::GetCurrentValue();
#endif

	QueuePackage(PackageDesc);
	return RequestID;
}

void FAsyncLoadingThread::FlushLoading(TConstArrayView<int32> RequestIDs)
{
 	if (IsAsyncLoadingPackages())
	{
		checkf(IsInGameThread(), TEXT("The current loader %s is unable to FlushAsyncLoading from any thread other than the game thread."), LexToString(GetLoaderType()));

		// Flushing async loading while loading is suspend will result in infinite stall
		UE_CLOG(IsAsyncLoadingSuspendedInternal(), LogStreaming, Fatal, TEXT("Cannot Flush Async Loading while async loading is suspended (%d)"), GetAsyncLoadingSuspendedCount());

		SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_FlushAsyncLoadingGameThread);

		if (RequestIDs.Num() != 0 && !ContainsAnyRequestInternal(RequestIDs))
		{
			return;
		}

		FCoreDelegates::OnAsyncLoadingFlush.Broadcast();

		double StartTime = FPlatformTime::Seconds();

		// Flush async loaders without using a time limit. Needed for e.g. garbage collection.
		{
			FFlushRequest FlushRequest;
			if (RequestIDs.Num() != 0)
			{
				// This create and add a flush request to use on the async thread
				FlushRequest = AddFlushRequest(RequestIDs);
			}
			SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_TickAsyncLoadingGameThread);
			while (IsAsyncLoadingPackages())
			{
				EAsyncPackageState::Type Result = TickAsyncLoading(false, false, 0, FlushRequest);
				if (RequestIDs.Num() != 0 && !ContainsAnyRequestInternal(RequestIDs))
				{
					break;
				}

				if (IsMultithreaded())
				{
					// Update the heartbeat and sleep. If we're not multithreading, the heartbeat is updated after each package has been processed
					FThreadHeartBeat::Get().HeartBeat();
					FPlatformProcess::SleepNoStats(0.0001f);
				}

				// push stats so that we don't overflow number of tags per thread during blocking loading
				LLM_PUSH_STATS_FOR_ASSET_TAGS();
			}
		}

		double EndTime = FPlatformTime::Seconds();
		double ElapsedTime = EndTime - StartTime;

		GFlushAsyncLoadingTime += ElapsedTime;
		GFlushAsyncLoadingCount++;

		check(RequestIDs.Num() != 0 || !IsAsyncLoadingPackages());

	}
}

EAsyncPackageState::Type FAsyncLoadingThread::ProcessLoadingUntilComplete(TFunctionRef<bool()> CompletionPredicate, double TimeLimit)
{
	if (!IsAsyncLoadingPackages())
	{
		return EAsyncPackageState::Complete;
	}

	SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_FlushAsyncLoadingGameThread);

	// Flushing async loading while loading is suspend will result in infinite stall
	UE_CLOG(IsAsyncLoadingSuspendedInternal(), LogStreaming, Fatal, TEXT("Cannot Flush Async Loading while async loading is suspended (%d)"), GetAsyncLoadingSuspendedCount());

	if (TimeLimit <= 0.0)
	{
		// Set to one hour if no time limit
		TimeLimit = 60.0 * 60.0;
	}

	#if UE_ENABLE_TRACKED_IO
	FActiveAsyncLoadContext Context(QueuedPackages);
	#endif

	while (IsAsyncLoadingPackages() && TimeLimit > 0 && !CompletionPredicate())
	{
		double TickStartTime = FPlatformTime::Seconds();
		if (ProcessLoading(true, true, TimeLimit) == EAsyncPackageState::Complete)
		{
			return EAsyncPackageState::Complete;
		}

		if (IsMultithreaded())
		{
			// Update the heartbeat and sleep. If we're not multithreading, the heartbeat is updated after each package has been processed
			FThreadHeartBeat::Get().HeartBeat();
			FPlatformProcess::SleepNoStats(0.0001f);
		}

		TimeLimit -= (FPlatformTime::Seconds() - TickStartTime);
	}

	return TimeLimit <= 0.0 ? EAsyncPackageState::TimeOut : EAsyncPackageState::Complete;
}

EAsyncPackageState::Type FAsyncLoadingThread::ProcessLoading(bool bUseTimeLimit, bool bUseFullTimeLimit, double TimeLimit)
{
	SCOPE_CYCLE_COUNTER(STAT_AsyncLoadingTime);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(AsyncLoading);

	CSV_CUSTOM_STAT(FileIO, EDLEventQueueDepth, EventQueue.EventQueue.Num(), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(FileIO, QueuedPackagesQueueDepth, GetQueuedPackagesCount(), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(FileIO, ExistingQueuedPackagesQueueDepth, GetExistingAsyncPackagesCount(), ECsvCustomStatOp::Set);

	{
		SCOPE_CYCLE_COUNTER(STAT_FAsyncPackage_TickAsyncLoadingGameThread); 
		TickAsyncLoading(bUseTimeLimit, bUseFullTimeLimit, TimeLimit);
	}

	return IsAsyncLoadingPackages() ? EAsyncPackageState::TimeOut : EAsyncPackageState::Complete;
}

#define USE_DETAILED_FASYNCARCHIVE_MEMORY_TRACKING 0

#if USE_DETAILED_FASYNCARCHIVE_MEMORY_TRACKING
class FAsyncArchiveMemTracker
{
	TMap<FString, int64> AllocatedMem;
	FCriticalSection AllocatedMemCritical;

public:

	void Allocate(const FString& Filename, int64 Mem)
	{
		FScopeLock AllocatedMemLock(&AllocatedMemCritical);
		int64& AllocatedMemAmount = AllocatedMem.FindOrAdd(Filename);
		AllocatedMemAmount += Mem;
	}

	void Deallocate(const FString& Filename, int64 Mem)
	{
		FScopeLock AllocatedMemLock(&AllocatedMemCritical);
		int64& AllocatedMemAmount = AllocatedMem.FindOrAdd(Filename);
		AllocatedMemAmount -= Mem;
		check(AllocatedMemAmount >= 0);
		if (AllocatedMemAmount == 0)
		{
			AllocatedMem.Remove(Filename);
		}
	}

	void Dump()
	{
		FScopeLock AllocatedMemLock(&AllocatedMemCritical);

		UE_LOG(LogStreaming, Display, TEXT("Dumping FAsyncArchie allocated memory (%d)"), AllocatedMem.Num());
		for (TPair<FString, int64>& ArchiveMem : AllocatedMem)
		{
			UE_LOG(LogStreaming, Display, TEXT("  %s %lldb"), *ArchiveMem.Key, ArchiveMem.Value);
		}
	}
} GAsyncArchiveMemTracker;

void DumpAsyncArchiveMem(const TArray<FString>& Args)
{
	GAsyncArchiveMemTracker.Dump();
}

static FAutoConsoleCommand GDumpSerializeCmd(
	TEXT("DumpAsyncArchiveMem"),
	TEXT("Debug command to dump the memory allocated by existing FAsyncArchive."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&DumpAsyncArchiveMem)
);
#endif // USE_DETAILED_FASYNCARCHIVE_MEMORY_TRACKING

static FCriticalSection SummaryRacePreventer;

FAsyncArchive::FAsyncArchive(const FPackagePath& InPackagePath, FLinkerLoad* InOwner, TFunction<void()>&& InSummaryReadyCallback)
	: Handle(nullptr)
	, SizeRequestPtr(nullptr)
	, EditorPrecacheRequestPtr(nullptr)
	, SummaryRequestPtr(nullptr)
	, SummaryPrecacheRequestPtr(nullptr)
	, ReadRequestPtr(nullptr)
	, CanceledReadRequestPtr(nullptr)
	, PrecacheBuffer(nullptr)
	, FileSize(-1)
	, CurrentPos(0)
	, PrecacheStartPos(0)
	, PrecacheEndPos(0)
	, ReadRequestOffset(0)
	, ReadRequestSize(0)
	, HeaderSize(0)
	, HeaderSizeWhenReadingExportsFromSplitFile(0)
	, LoadPhase(ELoadPhase::WaitingForSize)
	, LoadError(ELoadError::Unknown)
	, bCookedForEDLInEditor(false)
	, bNeedsEngineVersionChecks(true)
	, PackagePath(InPackagePath)
	, OpenTime(FPlatformTime::Seconds())
	, SummaryReadTime(0.0)
	, ExportReadTime(0.0)
	, SummaryReadyCallback(Forward<TFunction<void()>>(InSummaryReadyCallback))
	, OwnerLinker(InOwner)
{
	// Set the Archive flags for code that uses this as an FArchive
	SetIsLoading(true);

	LogItem(TEXT("Open"));
	FOpenAsyncPackageResult OpenResult = IPackageResourceManager::Get().OpenAsyncReadPackage(PackagePath, EPackageSegment::Header);
	Handle = OpenResult.Handle.Release();
	check(Handle); // OpenAsyncReadPackage guarantees a non-null return value; the handle will fail to read later if the path does not exist
	if (OpenResult.Format != EPackageFormat::Binary)
	{
		SetError();
		LoadError = ELoadError::UnsupportedFormat;
	}
	bNeedsEngineVersionChecks = OpenResult.bNeedsEngineVersionChecks;

	ReadCallbackFunction = [this](bool bWasCancelled, IAsyncReadRequest* Request)
	{
		ReadCallback(bWasCancelled, Request);
	};

	if (GEventDrivenLoaderEnabled)
	{
		check(SummaryReadyCallback);
		ReadCallbackFunctionForLinkerLoad = [this](bool bWasCancelled, IAsyncReadRequest* Request)
		{
			SummaryReadyCallback();
		};
	}

	SizeRequestPtr = Handle->SizeRequest(&ReadCallbackFunction);

}

FAsyncArchive::~FAsyncArchive()
{
	UE_CLOG(OwnerLinker && !(OwnerLinker->GetLoader_Unsafe() == this && OwnerLinker->IsDestroyingLoader()), LogStreaming, Fatal,
		TEXT("Destroying FAsyncArchive %s that belongs to linker %s outside of the linker's DestroyLoader code!"), *GetArchiveName(), *OwnerLinker->GetArchiveName());

	// Invalidate any precached data and free memory.
	FlushCache();
	if (Handle)
	{
		delete Handle;
		Handle = nullptr;
	}
	LogItem(TEXT("~FAsyncArchive"), 0, 0);
}

void FAsyncArchive::ReadCallback(bool bWasCancelled, IAsyncReadRequest* Request)
{
	if (bWasCancelled || IsError())
	{
		if (!IsError())
		{
			LoadError = ELoadError::Cancelled;
		}
		SetError();
		return; // we don't do much with this, the code on the other thread knows how to deal with my request
	}
	if (LoadPhase == ELoadPhase::WaitingForSize)
	{
		LoadPhase = ELoadPhase::WaitingForSummary;
		FileSize = Request->GetSizeResults();
		if (FileSize < 32)
		{
			SetError();
			LoadError = FileSize > 0 ? ELoadError::CorruptData : ELoadError::FileDoesNotExist;
		}
		else
		{
			if (GEventDrivenLoaderEnabled)
			{
				// in this case we don't need to serialize the summary because we know the header is the whole file
				FScopeLock Lock(&SummaryRacePreventer);
				HeaderSize = FileSize;
				LogItem(TEXT("Starting Split Header"), 0, FileSize);
				PrecacheInternal(0, HeaderSize);
				FPlatformMisc::MemoryBarrier();
				LoadPhase = ELoadPhase::WaitingForHeader;
			}
			else
			{
				int64 Size = FMath::Min<int64>(FMaxPackageSummarySize::Value, FileSize);
				LogItem(TEXT("Starting Summary"), 0, Size);
				SummaryRequestPtr = Handle->ReadRequest(0, Size, GetAsyncIOPriority(), &ReadCallbackFunction);
				// I need a precache request here to keep the memory alive until I submit the header request
				SummaryPrecacheRequestPtr = Handle->ReadRequest(0, Size, GetAsyncIOPrecachePriorityAndFlags() );
#if WITH_EDITOR
				if (FileSize > Size && GEditorLoadPrecacheSizeKB > 0)
				{
					const int64 MaxEditorPrecacheSize = int64(GEditorLoadPrecacheSizeKB) * 1024;
					EditorPrecacheRequestPtr = Handle->ReadRequest(Size, FMath::Min<int64>(FileSize - Size, MaxEditorPrecacheSize), GetAsyncIOPrecachePriorityAndFlags());
				}
#endif
			}
		}
	}
	else if (LoadPhase == ELoadPhase::WaitingForSummary)
	{
		check(!GEventDrivenLoaderEnabled || !EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME);
		uint8* Mem = Request->GetReadResults();
		if (!Mem)
		{
			SetError();
			LoadError = ELoadError::CorruptData;
			FPlatformMisc::MemoryBarrier();
			LoadPhase = ELoadPhase::WaitingForHeader;
		}
		else
		{
			FBufferReader Ar(Mem, FMath::Min<int64>(FMaxPackageSummarySize::Value, FileSize),/*bInFreeOnClose=*/ false, /*bIsPersistent=*/ true);
			FPackageFileSummary Sum;
			Ar << Sum;
			if (Ar.IsError() || Sum.TotalHeaderSize > FileSize || Sum.IsFileVersionTooOld())
			{
				SetError();
				LoadError = ELoadError::CorruptData;
			}
			else
			{
				FScopeLock Lock(&SummaryRacePreventer);
				//@todoio change header format to put the TotalHeaderSize at the start of the file
				// we need to be sure that we can at least get the size from the initial request. This is an early warning that custom versions are starting to get too big, relocate the total size to be at offset 4!
				checkf(Ar.Tell() < FMaxPackageSummarySize::Value / 2,
					TEXT("The initial read request was too small (%d) compared to package %s header size (%lld). Try increasing s.MaxPackageSummarySize value in DefaultEngine.ini."),
					FMaxPackageSummarySize::Value, *PackagePath.GetDebugName(), Ar.Tell());
				
				// Support for cooked EDL packages in the editor
				bCookedForEDLInEditor = !FPlatformProperties::RequiresCookedData() && (Sum.GetPackageFlags() & PKG_FilterEditorOnly) && Sum.PreloadDependencyCount > 0 && Sum.PreloadDependencyOffset > 0;

				HeaderSize = Sum.TotalHeaderSize;
				LogItem(TEXT("Starting Header"), 0, HeaderSize);
				PrecacheInternal(0, HeaderSize);
				FPlatformMisc::MemoryBarrier();
				LoadPhase = ELoadPhase::WaitingForHeader;
			}
			FMemory::Free(Mem);
		}
	}
	else
	{
		check(0); // we don't use callbacks for other phases
	}
}

void FAsyncArchive::FlushPrecacheBlock()
{
#if DEVIRTUALIZE_FLinkerLoad_Serialize
	DiscardInlineBufferAndUpdateCurrentPos();
#endif
	if (PrecacheBuffer)
	{
		DEC_MEMORY_STAT_BY(STAT_FAsyncArchiveMem, PrecacheEndPos - PrecacheStartPos);
		FMemory::Free(PrecacheBuffer);
#if USE_DETAILED_FASYNCARCHIVE_MEMORY_TRACKING
		GAsyncArchieMemTracker.Deallocate(FileName, PrecacheEndPos - PrecacheStartPos);
#endif
		check(!GEventDrivenLoaderEnabled || LoadPhase > ELoadPhase::WaitingForHeader);
	}
	PrecacheBuffer = nullptr;
	PrecacheStartPos = 0;
	PrecacheEndPos = 0;
}

void FAsyncArchive::FlushCache()
{
	bool nNonRedundantFlush = PrecacheEndPos || PrecacheBuffer || ReadRequestPtr;
	LogItem(TEXT("Flush"));
	WaitForIntialPhases();
	WaitRead(); // this deals with the read request
	CompleteCancel(); // this deals with the cancel request, important this is last because completing other things leaves cancels to process
	FlushPrecacheBlock();

	if (EditorPrecacheRequestPtr)
	{
		EditorPrecacheRequestPtr->WaitCompletion();
		delete EditorPrecacheRequestPtr;
		EditorPrecacheRequestPtr = nullptr;
	}

	if (Handle)
	{
		Handle->ShrinkHandleBuffers();
	}

	if ((UE_LOG_ACTIVE(LogAsyncArchive, Verbose) 
#if defined(ASYNC_WATCH_FILE)
		|| FileName.Contains(TEXT(ASYNC_WATCH_FILE))
#endif
		) && nNonRedundantFlush)
	{
		double Now(FPlatformTime::Seconds());
		float TotalLifetime = float(1000.0 * (Now - OpenTime));

		if (!UE_LOG_ACTIVE(LogAsyncArchive, VeryVerbose) && TotalLifetime < 100.0f
#if defined(ASYNC_WATCH_FILE)
			&& !FileName.Contains(TEXT(ASYNC_WATCH_FILE))
#endif
			)
		{
			return;
		}

		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Flush     Lifeitme %6.2fms   Open->Summary %6.2fms    Summary->Export1 %6.2fms    Export1->Now %6.2fms       %s\r\n"),
			TotalLifetime,
			float(1000.0 * (SummaryReadTime - OpenTime)),
			float(1000.0 * (ExportReadTime - SummaryReadTime)),
			float(1000.0 * (Now - ExportReadTime)),
			*PackagePath.GetDebugName());
#if defined(ASYNC_WATCH_FILE)
		if (FileName.Contains(TEXT(ASYNC_WATCH_FILE)))
		{
			UE_LOG(LogAsyncArchive, Warning, TEXT("Handy Breakpoint after flush."));
		}
#endif
	}

}

bool FAsyncArchive::Close()
{
	// Invalidate any precached data and free memory.
	FlushCache();
	// Return true if there were NO errors, false otherwise.
	return !IsError();
}

bool FAsyncArchive::SetCompressionMap(TArray<FCompressedChunk>* InCompressedChunks, ECompressionFlags InCompressionFlags)
{
	check(0); // no support for compression
	return false;
}

int64 FAsyncArchive::TotalSize()
{
	if (SizeRequestPtr)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FArchiveAsync2_TotalSize);
		SizeRequestPtr->WaitCompletion();
		if ((GEventDrivenLoaderEnabled || bCookedForEDLInEditor) && HeaderSizeWhenReadingExportsFromSplitFile)
		{
			FileSize = SizeRequestPtr->GetSizeResults();
		}
		delete SizeRequestPtr;
		SizeRequestPtr = nullptr;
	}
	return FileSize + HeaderSizeWhenReadingExportsFromSplitFile;
}

#if DEVIRTUALIZE_FLinkerLoad_Serialize
FORCEINLINE void FAsyncArchive::SetPosAndUpdatePrecacheBuffer(int64 Pos)
{
	check(Pos >= 0 && Pos <= TotalSizeOrMaxInt64IfNotReady());
	if (Pos < PrecacheStartPos || Pos >= PrecacheEndPos)
	{
		ActiveFPLB->Reset();
		CurrentPos = Pos;
	}
	else
	{
		check(PrecacheBuffer);
		ActiveFPLB->OriginalFastPathLoadBuffer = PrecacheBuffer;
		ActiveFPLB->StartFastPathLoadBuffer = PrecacheBuffer + (Pos - PrecacheStartPos);
		ActiveFPLB->EndFastPathLoadBuffer = PrecacheBuffer + (PrecacheEndPos - PrecacheStartPos);
		CurrentPos = PrecacheStartPos;
	}
	check(Tell() == Pos);
}
#endif

void FAsyncArchive::Seek(int64 InPos)
{
	if ((GEventDrivenLoaderEnabled || bCookedForEDLInEditor) && LoadPhase < ELoadPhase::ProcessingExports)
	{
		check(!HeaderSizeWhenReadingExportsFromSplitFile && HeaderSize && TotalSize() == HeaderSize);
		if (InPos >= HeaderSize)
		{
			FirstExportStarting();
		}
	}
	checkf(InPos >= 0 && InPos <= TotalSizeOrMaxInt64IfNotReady(), TEXT("Bad position in FAsyncArchive::Seek. Filename:%s InPos:%lu, Size:%lu"),
		*PackagePath.GetDebugName(), InPos, TotalSizeOrMaxInt64IfNotReady());
#if DEVIRTUALIZE_FLinkerLoad_Serialize
	SetPosAndUpdatePrecacheBuffer(InPos);
#else
	CurrentPos = InPos;
#endif
}

bool FAsyncArchive::WaitRead(double TimeLimit)
{
	if (ReadRequestPtr)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FArchiveAsync2_WaitRead);
		const int64 Offset = ReadRequestOffset;
		const int64 Size = ReadRequestSize;
		check(Size > 0);
		const double StartTime = FPlatformTime::Seconds();
		const bool bResult = ReadRequestPtr->WaitCompletion((float)TimeLimit);
		LogItem(TEXT("Wait Read"), Offset, Size, StartTime);
		if (!bResult)
		{
			return false;
		}
		CompleteRead();
	}
	return true;
}

void FAsyncArchive::CompleteRead()
{
	double StartTime = FPlatformTime::Seconds();
	check(LoadPhase != ELoadPhase::WaitingForSize && LoadPhase != ELoadPhase::WaitingForSummary);
	check(ReadRequestPtr && ReadRequestPtr->PollCompletion());
	if (PrecacheBuffer)
	{
		FlushPrecacheBlock();
	}
	if (!IsError())
	{
		uint8* Mem = ReadRequestPtr->GetReadResults();
		if (!Mem)
		{
			SetError();
			LoadError = ELoadError::CorruptData;
		}
		else
		{
			PrecacheBuffer = Mem;
			PrecacheStartPos = ReadRequestOffset;
			PrecacheEndPos = ReadRequestOffset + ReadRequestSize;
			check(ReadRequestSize > 0 && PrecacheStartPos >= 0);
			INC_MEMORY_STAT_BY(STAT_FAsyncArchiveMem, PrecacheEndPos - PrecacheStartPos);
#if USE_DETAILED_FASYNCARCHIVE_MEMORY_TRACKING
			GAsyncArchiveMemTracker.Allocate(FileName, PrecacheEndPos - PrecacheStartPos);
#endif
			// keeps the last cache block of the header around until we process the first export
			if (LoadPhase != ELoadPhase::ProcessingExports && Handle->UsesCache())
			{
				CompleteCancel();
				CanceledReadRequestPtr = Handle->ReadRequest(PrecacheEndPos - HeaderSizeWhenReadingExportsFromSplitFile - 1, 1, GetAsyncIOPrecachePriorityAndFlags());
			}
		}
	}

	delete ReadRequestPtr;
	ReadRequestPtr = nullptr;
	LogItem(TEXT("CompleteRead"), ReadRequestOffset, ReadRequestSize);
	ReadRequestOffset = 0;
	ReadRequestSize = 0;
}

void FAsyncArchive::CompleteCancel()
{
	if (CanceledReadRequestPtr)
	{
		double StartTime = FPlatformTime::Seconds();
		CanceledReadRequestPtr->WaitCompletion();
		//check(!CanceledReadRequestPtr->GetReadResults()); // this should have been canceled
		delete CanceledReadRequestPtr;
		CanceledReadRequestPtr = nullptr;
		LogItem(TEXT("Complete Cancel"), 0, 0, StartTime);
	}
}


void FAsyncArchive::CancelRead()
{
	if (ReadRequestPtr)
	{
		ReadRequestPtr->Cancel();
		CompleteCancel();
		CanceledReadRequestPtr = ReadRequestPtr;
		ReadRequestPtr = nullptr;
	}
	ReadRequestOffset = 0;
	ReadRequestSize = 0;
}

bool FAsyncArchive::WaitForIntialPhases(double InTimeLimit)
{
	if (SizeRequestPtr 
		|| GEventDrivenLoaderEnabled || SummaryRequestPtr || SummaryPrecacheRequestPtr
		)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FArchiveAsync2_WaitForIntialPhases);
		double StartTime = FPlatformTime::Seconds();
		if (SizeRequestPtr)
		{
			if (SizeRequestPtr->WaitCompletion((float)InTimeLimit))
			{
				delete SizeRequestPtr;
				SizeRequestPtr = nullptr;
			}
			else
			{
				check(InTimeLimit > 0.0);
				return false;
			}
		}
		if (!GEventDrivenLoaderEnabled)
		{
			if (SummaryRequestPtr)
			{
				double TimeLimit = 0.0;
				if (InTimeLimit > 0.0)
				{
					TimeLimit = InTimeLimit - (FPlatformTime::Seconds() - StartTime);
					if (TimeLimit < MIN_REMAIN_TIME)
					{
						return false;
					}
				}
				if (SummaryRequestPtr->WaitCompletion((float)TimeLimit))
				{
					delete SummaryRequestPtr;
					SummaryRequestPtr = nullptr;
				}
				else
				{
					check(InTimeLimit > 0.0);
					return false;
				}
			}
			if (SummaryPrecacheRequestPtr)
			{
				double TimeLimit = 0.0;
				if (InTimeLimit > 0.0)
				{
					TimeLimit = InTimeLimit - (FPlatformTime::Seconds() - StartTime);
					if (TimeLimit < MIN_REMAIN_TIME)
					{
						return false;
					}
				}
				if (SummaryPrecacheRequestPtr->WaitCompletion((float)TimeLimit))
				{
					delete SummaryPrecacheRequestPtr;
					SummaryPrecacheRequestPtr = nullptr;
				}
				else
				{
					check(InTimeLimit > 0.0);
					return false;
				}
			}
		}
		LogItem(TEXT("Wait Summary"), 0, HeaderSize, StartTime);
	}
	return true;
}

bool FAsyncArchive::PrecacheInternal(int64 RequestOffset, int64 RequestSize, bool bApplyMinReadSize, IAsyncReadRequest* Read)
{
	// CAUTION! This is possibly called the first time from a random IO thread.

	bool bIsWaitingForSummary =( LoadPhase == ELoadPhase::WaitingForSummary);

	bool bReadIsActualRequest = !Handle->UsesCache();

	if (!bIsWaitingForSummary)
	{
		if (RequestSize == 0 || (RequestOffset >= PrecacheStartPos && RequestOffset + RequestSize <= PrecacheEndPos))
		{
			// ready
			delete Read;
			return true;
		}
		if (ReadRequestPtr && RequestOffset >= ReadRequestOffset && RequestOffset + RequestSize <= ReadRequestOffset + ReadRequestSize)
		{
			// current request contains request
			bool bResult = false;
			if (ReadRequestPtr->PollCompletion())
			{
				CompleteRead();
				if (GetLoadError() == ELoadError::Unknown)
				{
					check(RequestOffset >= PrecacheStartPos && RequestOffset + RequestSize <= PrecacheEndPos);
					bResult = true;
				}
			}
			delete Read;
			return bResult;
		}
		if (ReadRequestPtr)
		{
			// this one does not have what we need
			UE_LOG(LogStreaming, Warning, TEXT("FAsyncArchive::PrecacheInternal Canceled read for %s  Offset = %lld   Size = %lld"), *PackagePath.GetDebugName(), RequestOffset, ReadRequestSize);
			CancelRead();
		}
	}
	check(!ReadRequestPtr);
	ReadRequestOffset = RequestOffset;
	ReadRequestSize = RequestSize;


	if (bApplyMinReadSize && !bIsWaitingForSummary && !bReadIsActualRequest)
	{
#if WITH_EDITOR
		static int64 MinimumReadSize = 1024 * 1024;
#else
		static int64 MinimumReadSize = 65536;
#endif
		checkSlow(MinimumReadSize >= 2048 && MinimumReadSize <= 1024 * 1024); // not a hard limit, but we should be loading at least a reasonable amount of data
		if (ReadRequestSize < MinimumReadSize)
		{ 
			ReadRequestSize = MinimumReadSize;
			int64 LocalFileSize = TotalSize();
			ReadRequestSize = FMath::Min(ReadRequestOffset + ReadRequestSize, LocalFileSize) - ReadRequestOffset;
		}
	}
	if (ReadRequestSize <= 0)
	{
		SetError();
		LoadError = ELoadError::CorruptData;
		return true;
	}
	double StartTime = FPlatformTime::Seconds();
#if defined(ASYNC_WATCH_FILE)
	if (FileName.Contains(TEXT(ASYNC_WATCH_FILE)) && ReadRequestOffset == 80203)
	{
		UE_LOG(LogAsyncArchive, Warning, TEXT("Handy Breakpoint Read"));
	}
#endif
	check(ReadRequestOffset - HeaderSizeWhenReadingExportsFromSplitFile >= 0 && ReadRequestSize > 0);

	if (Read && bReadIsActualRequest)
	{
		ReadRequestPtr = Read;
		Read = nullptr;
	}
	else
	{
	// caution, this callback can fire before this even returns....and so bIsWaitingForSummary must be a local variable or we could get all confused by concurrency!
		ReadRequestPtr = Handle->ReadRequest(ReadRequestOffset - HeaderSizeWhenReadingExportsFromSplitFile, ReadRequestSize, GetAsyncIOPriority()
		, (GEventDrivenLoaderEnabled && bIsWaitingForSummary) ? &ReadCallbackFunctionForLinkerLoad : nullptr
		);
	}
	delete Read;
	if (!bIsWaitingForSummary && ReadRequestPtr->PollCompletion())
	{
		LogItem(TEXT("Read Start Hot"), ReadRequestOffset - HeaderSizeWhenReadingExportsFromSplitFile, ReadRequestSize, StartTime);
		CompleteRead();
		check(RequestOffset >= PrecacheStartPos && RequestOffset + RequestSize <= PrecacheEndPos);
		return true;
	}
	else if (bIsWaitingForSummary)
	{
		LogItem(TEXT("Read Start Summary"), ReadRequestOffset - HeaderSizeWhenReadingExportsFromSplitFile, ReadRequestSize, StartTime);
	}
	else 
	{
		LogItem(TEXT("Read Start Cold"), ReadRequestOffset - HeaderSizeWhenReadingExportsFromSplitFile, ReadRequestSize, StartTime);
	}
	return false;
}

void FAsyncArchive::FirstExportStarting()
{
	ExportReadTime = FPlatformTime::Seconds();
	LogItem(TEXT("Exports"));
	LoadPhase = ELoadPhase::ProcessingExports;

	if ((GEventDrivenLoaderEnabled && !EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME) || bCookedForEDLInEditor)
	{
		FlushCache();
		if (Handle)
		{
			delete Handle;
			Handle = nullptr;
		}

		HeaderSizeWhenReadingExportsFromSplitFile = HeaderSize;

		FOpenAsyncPackageResult OpenResult = IPackageResourceManager::Get().OpenAsyncReadPackage(PackagePath, EPackageSegment::Exports);
		Handle = OpenResult.Handle.Release();
		check(Handle); // OpenAsyncReadPackage guarantees a non-null return value; the handle will fail to read later if the path does not exist

		check(!SizeRequestPtr);
		SizeRequestPtr = Handle->SizeRequest();
		if (SizeRequestPtr->PollCompletion())
		{
			TotalSize(); // complete the request
		}
	}
}

IAsyncReadRequest* FAsyncArchive::MakeEventDrivenPrecacheRequest(int64 Offset, int64 BytesToRead, FAsyncFileCallBack* CompleteCallback)
{
	check(GEventDrivenLoaderEnabled);
	if (LoadPhase == ELoadPhase::WaitingForFirstExport)
	{
		// we need to avoid tearing down the old file and requests until we have the one in flight
		HeaderSizeWhenReadingExportsFromSplitFile = HeaderSize;
		IAsyncReadFileHandle* NewHandle;
		{
			double StartTime = FPlatformTime::Seconds();
			FOpenAsyncPackageResult OpenResult = IPackageResourceManager::Get().OpenAsyncReadPackage(PackagePath, EPackageSegment::Exports);
			NewHandle = OpenResult.Handle.Release();
			check(NewHandle); // OpenAsyncReadPackage guarantees a non-null return value; the handle will fail to read later if the path does not exist
			LogItem(TEXT("Open UExp"), Offset - HeaderSizeWhenReadingExportsFromSplitFile, BytesToRead, StartTime);
		}
		{
			double StartTime = FPlatformTime::Seconds();

			check(Offset - HeaderSizeWhenReadingExportsFromSplitFile >= 0);

			EAsyncIOPriorityAndFlags Prio = NewHandle->UsesCache() ? GetAsyncIOPrecachePriorityAndFlags() : GetAsyncIOPriority();

			IAsyncReadRequest* Precache = NewHandle->ReadRequest(Offset - HeaderSizeWhenReadingExportsFromSplitFile, BytesToRead, Prio, CompleteCallback);
			FlushCache();
			if (Handle)
			{
				delete Handle;
				Handle = nullptr;
			}
			Handle = NewHandle;

			FirstExportStarting();

			check(!SizeRequestPtr);
			SizeRequestPtr = Handle->SizeRequest();
			if (SizeRequestPtr->PollCompletion())
			{
				TotalSize(); // complete the request
			}
			LogItem(TEXT("First Precache"), Offset - HeaderSizeWhenReadingExportsFromSplitFile, BytesToRead, StartTime);
			return Precache;
		}
	}
	double StartTime = FPlatformTime::Seconds();
	check(Offset - HeaderSizeWhenReadingExportsFromSplitFile >= 0);
	check(Offset + BytesToRead <= TotalSizeOrMaxInt64IfNotReady());
	EAsyncIOPriorityAndFlags Prio = Handle->UsesCache() ? GetAsyncIOPrecachePriorityAndFlags() : GetAsyncIOPriority();
	IAsyncReadRequest* Precache = Handle->ReadRequest(Offset - HeaderSizeWhenReadingExportsFromSplitFile, BytesToRead, Prio, CompleteCallback);
	LogItem(TEXT("Event Precache"), Offset - HeaderSizeWhenReadingExportsFromSplitFile, BytesToRead, StartTime);
	return Precache;
}


bool FAsyncArchive::PrecacheWithTimeLimit(int64 RequestOffset, int64 RequestSize, bool bUseTimeLimit, bool bUseFullTimeLimit, double TickStartTime, double TimeLimit)
{
#if defined(ASYNC_WATCH_FILE)
	if (FileName.Contains(TEXT(ASYNC_WATCH_FILE)) && RequestOffset == 878129)
	{
		UE_LOG(LogAsyncArchive, Warning, TEXT("Handy Breakpoint Raw Precache %lld"), RequestOffset);
	}
#endif
	if (LoadPhase == ELoadPhase::WaitingForSize || LoadPhase == ELoadPhase::WaitingForSummary || LoadPhase == ELoadPhase::WaitingForHeader)
	{
		check(0); // this is a precache for an export, why is the summary not read yet?
		return false;
	}
	if (LoadPhase == ELoadPhase::WaitingForFirstExport)
	{
		FirstExportStarting();
	}
	if (!bUseTimeLimit)
	{
		return true; // we will stream and do the blocking on the serialize calls
	}
	bool bResult = PrecacheInternal(RequestOffset, RequestSize);
	if (!bResult && bUseFullTimeLimit)
	{
		double RemainingTime = TimeLimit - (FPlatformTime::Seconds() - TickStartTime);
		if (RemainingTime > MIN_REMAIN_TIME && WaitRead(RemainingTime))
		{
			bResult = true;
		}
	}
	return bResult;
}

bool FAsyncArchive::Precache(int64 RequestOffset, int64 RequestSize)
{
	if (LoadPhase == ELoadPhase::WaitingForSize || LoadPhase == ELoadPhase::WaitingForSummary)
	{
		return false;
	}
	if (LoadPhase == ELoadPhase::WaitingForHeader)
	{
		//@todoio, it would be nice to check that when we read the header, we don't read any more than we really need...i.e. no "minimum read size"
		check(RequestOffset == 0 && RequestOffset + RequestSize <= HeaderSize);
	}
	return PrecacheInternal(RequestOffset, RequestSize);
}

bool FAsyncArchive::PrecacheForEvent(IAsyncReadRequest* Read, int64 RequestOffset, int64 RequestSize)
{
	check(int32(LoadPhase) > int32(ELoadPhase::WaitingForHeader));
	return PrecacheInternal(RequestOffset, RequestSize, false, Read);
}


void FAsyncArchive::StartReadingHeader()
{
	//LogItem(TEXT("Start Header"));
	WaitForIntialPhases();
	if (!IsError())
	{
		if (int32(LoadPhase) < int32(ELoadPhase::WaitingForHeader))
		{
			FScopeLock Lock(&SummaryRacePreventer);
		}
		check(LoadPhase == ELoadPhase::WaitingForHeader && ReadRequestPtr);
		WaitRead();
	}
}

void FAsyncArchive::EndReadingHeader()
{
	LogItem(TEXT("End Header"));
	
	if (!IsError())
	{
		check(LoadPhase == ELoadPhase::WaitingForHeader);
		LoadPhase = ELoadPhase::WaitingForFirstExport;
		FlushPrecacheBlock();
	}
}

bool FAsyncArchive::ReadyToStartReadingHeader(bool bUseTimeLimit, bool bUseFullTimeLimit, double TickStartTime, double TimeLimit)
{
	if (SummaryReadTime == 0.0)
	{
		SummaryReadTime = FPlatformTime::Seconds();
	}
	if (!bUseTimeLimit)
	{
		return true; // we will stream and do the blocking on the serialize calls
	}
	if (LoadPhase == ELoadPhase::WaitingForSize || LoadPhase == ELoadPhase::WaitingForSummary)
	{
		if (bUseFullTimeLimit)
		{
			double RemainingTime = TimeLimit - (FPlatformTime::Seconds() - TickStartTime);
			if (RemainingTime < MIN_REMAIN_TIME || !WaitForIntialPhases(RemainingTime))
			{
				return false; // not ready
			}
		}
		else
		{
			return false; // not ready, not going to wait
		}
	}
	check(LoadPhase == ELoadPhase::WaitingForHeader);
	LogItem(TEXT("Ready For Header"));
	return true;
}

#if TRACK_SERIALIZE
void CallSerializeHook();
#endif

void FAsyncArchive::Serialize(void* Data, int64 Count)
{
	if (!Count || IsError())
	{
		return;
	}
	check(Count > 0);
#if DEVIRTUALIZE_FLinkerLoad_Serialize
	if (ActiveFPLB->StartFastPathLoadBuffer + Count <= ActiveFPLB->EndFastPathLoadBuffer)
	{
		// this wasn't one of the cases we devirtualized; we can short circut here to avoid resettting the buffer when we don't need to
		FMemory::ParallelMemcpy(Data, ActiveFPLB->StartFastPathLoadBuffer, Count);
		ActiveFPLB->StartFastPathLoadBuffer += Count;
		return;
	}

	DiscardInlineBufferAndUpdateCurrentPos();
#endif

#if TRACK_SERIALIZE
	CallSerializeHook();
#endif

#if PLATFORM_DESKTOP
	// Show a message box indicating, possible, corrupt data (desktop platforms only)
	if (CurrentPos + Count > TotalSize())
	{
		FText ErrorMessage, ErrorCaption;
		GConfig->GetText(TEXT("/Script/Engine.Engine"),
			TEXT("SerializationOutOfBoundsErrorMessage"),
			ErrorMessage,
			GEngineIni);
		GConfig->GetText(TEXT("/Script/Engine.Engine"),
			TEXT("SerializationOutOfBoundsErrorMessageCaption"),
			ErrorCaption,
			GEngineIni);

		FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage, ErrorCaption);
	}
#endif
	// Ensure we aren't reading beyond the end of the file
	checkf(CurrentPos + Count <= TotalSizeOrMaxInt64IfNotReady(), TEXT("Seeked past end of file %s (%lld / %lld)"), *PackagePath.GetDebugName(), CurrentPos + Count, TotalSize());

	int64 BeforeBlockOffset = 0;
	int64 BeforeBlockSize = 0;
	int64 AfterBlockOffset = 0;
	int64 AfterBlockSize = 0;

	if (CurrentPos + Count <= PrecacheStartPos || CurrentPos >= PrecacheEndPos)
	{
		// no overlap with current buffer
		AfterBlockOffset = CurrentPos;
		AfterBlockSize = Count;
	}
	else
	{
		if (CurrentPos >= PrecacheStartPos)
		{
			// no before block and head of desired block is in the cache
			int64 CopyLen = FMath::Min(PrecacheEndPos - CurrentPos, Count);
			check(CopyLen > 0);
			check(PrecacheBuffer);
			FMemory::Memcpy(Data, PrecacheBuffer + CurrentPos - PrecacheStartPos, CopyLen);
			AfterBlockSize = Count - CopyLen;
			check(AfterBlockSize >= 0);
			AfterBlockOffset = PrecacheEndPos;
		}
		else
		{
			// first part of the block is not in the cache
			BeforeBlockSize = PrecacheStartPos - CurrentPos;
			check(BeforeBlockSize > 0);
			BeforeBlockOffset = CurrentPos;
			if (CurrentPos + Count > PrecacheStartPos)
			{
				// tail of desired block is in the cache
				int64 CopyLen = FMath::Min(PrecacheEndPos - CurrentPos - BeforeBlockSize, Count - BeforeBlockSize);
				check(CopyLen > 0);
				check(PrecacheBuffer);
				FMemory::Memcpy(((uint8*)Data) + BeforeBlockSize, PrecacheBuffer, CopyLen);
				AfterBlockSize = Count - CopyLen - BeforeBlockSize;
				check(AfterBlockSize >= 0);
				AfterBlockOffset = PrecacheEndPos;
			}
		}
	}
	if (BeforeBlockSize)
	{
		UE_CLOG(GEventDrivenLoaderEnabled, LogAsyncArchive, Warning, TEXT("FAsyncArchive::Serialize Backwards streaming in %s  CurrentPos = %lld   BeforeBlockOffset = %lld"),
			*PackagePath.GetDebugName(), CurrentPos, BeforeBlockOffset);
		LogItem(TEXT("Sync Before Block"), BeforeBlockOffset, BeforeBlockSize);
		if (!PrecacheInternal(BeforeBlockOffset, BeforeBlockSize))
		{
		WaitRead();
		}
		if (IsError())
		{
			return;
		}
		check(BeforeBlockOffset >= PrecacheStartPos && BeforeBlockOffset + BeforeBlockSize <= PrecacheEndPos);
		check(PrecacheBuffer);
		FMemory::Memcpy(Data, PrecacheBuffer + BeforeBlockOffset - PrecacheStartPos, BeforeBlockSize);
	}
	if (AfterBlockSize)
	{
#if defined(ASYNC_WATCH_FILE)
		if (FileName.Contains(TEXT(ASYNC_WATCH_FILE)))
		{
			UE_LOG(LogAsyncArchive, Warning, TEXT("Handy Breakpoint AfterBlockSize"));
		}
#endif
		LogItem(TEXT("Sync After Block"), AfterBlockOffset, AfterBlockSize);
		check(int32(LoadPhase) > int32(ELoadPhase::WaitingForSummary));

		int64_t OldPrecacheStartPos = PrecacheStartPos;
		int64_t OldPrecacheEndPos = PrecacheEndPos;
		void* OldRead = ReadRequestPtr;
		int64_t OldReadRequestOffset = ReadRequestOffset;
		int64_t OldReadRequestSize = ReadRequestSize;

		int64_t OldFileSize = FileSize;
		int64_t OldHeaderSizeWhenReadingExportsFromSplitFile = HeaderSizeWhenReadingExportsFromSplitFile;



		if (!PrecacheInternal(AfterBlockOffset, AfterBlockSize))
		{
			verify(WaitRead());
			void* OldRead2 = ReadRequestPtr;
			if (!IsError())
			{
				checkf(AfterBlockOffset >= PrecacheStartPos && AfterBlockOffset + AfterBlockSize <= PrecacheEndPos, 
					TEXT("Sync After Block Wait ????  %lld %lld     %lld %lld <-  %lld %lld     %lld %lld <-  %lld %lld    %p <- %p <- %p    %lld %lld <-  %lld %lld"), 
					AfterBlockOffset, AfterBlockSize, 
					PrecacheStartPos, PrecacheEndPos, OldPrecacheStartPos, OldPrecacheEndPos,
					ReadRequestOffset, ReadRequestSize, OldReadRequestOffset, OldReadRequestSize,
					ReadRequestPtr, OldRead2, OldRead,
					HeaderSizeWhenReadingExportsFromSplitFile, FileSize, OldHeaderSizeWhenReadingExportsFromSplitFile, OldFileSize
				);
			}
		}
		if (IsError())
		{
			return;
		}
		checkf(AfterBlockOffset >= PrecacheStartPos && AfterBlockOffset + AfterBlockSize <= PrecacheEndPos, TEXT("Sync After Block ????   %lld %lld %lld %lld"), AfterBlockOffset, AfterBlockSize, PrecacheStartPos, PrecacheEndPos);
		check(PrecacheBuffer);
		FMemory::Memcpy(((uint8*)Data) + Count - AfterBlockSize, PrecacheBuffer + AfterBlockOffset - PrecacheStartPos, AfterBlockSize);
	}
#if DEVIRTUALIZE_FLinkerLoad_Serialize
	SetPosAndUpdatePrecacheBuffer(CurrentPos + Count);
#else
	CurrentPos += Count;
#endif
}

#if DEVIRTUALIZE_FLinkerLoad_Serialize
void FAsyncArchive::DiscardInlineBufferAndUpdateCurrentPos()
{
	CurrentPos += (ActiveFPLB->StartFastPathLoadBuffer - ActiveFPLB->OriginalFastPathLoadBuffer);
	ActiveFPLB->Reset();
}
#endif

FLinkerExportArchive::FLinkerExportArchive(FLinkerLoad& InLinker, int64 InExportSerialOffset, int64 InExportSerialSize)
	: FArchiveProxy(InLinker)
	, ExportSerialOffset(InExportSerialOffset)
	, ExportSerialSize(InExportSerialSize)
{
	bExportsCookedToSeparateArchive = InLinker.IsLoadingFromCookedPackage() && InLinker.Summary.GetFileVersionUE() >= EUnrealEngineObjectUE5Version::DATA_RESOURCES;
}

int64 FLinkerExportArchive::Tell() 
{
	const int64 Position = FArchiveProxy::Tell();

	if (bExportsCookedToSeparateArchive)
	{
		return Position - ExportSerialOffset;
	}

	return Position; 
}

void FLinkerExportArchive::Seek(int64 Position)
{
	if (bExportsCookedToSeparateArchive && Position <= ExportSerialSize)
	{
		FArchiveProxy::Seek(ExportSerialOffset + Position);
	}
	else
	{
		FArchiveProxy::Seek(Position);
	}
}

#if TRACK_SERIALIZE

#include "Containers/StackTracker.h"
static TAutoConsoleVariable<int32> CVarLogAsyncArchiveSerializeChurn(
	TEXT("LogAsyncArchiveSerializeChurn.Enable"),
	0,
	TEXT("If > 0, then sample game thread FAsyncArchive::Serialize calls, periodically print a report of the worst offenders."));

static TAutoConsoleVariable<int32> CVarLogAsyncArchiveSerializeChurn_Threshhold(
	TEXT("LogAsyncArchiveSerializeChurn.Threshhold"),
	1000,
	TEXT("Minimum average number of FAsyncArchive::Serialize calls to include in the report."));

static TAutoConsoleVariable<int32> CVarLogAsyncArchiveSerializeChurn_SampleFrequency(
	TEXT("LogAsyncArchiveSerializeChurn.SampleFrequency"),
	1000,
	TEXT("Number of FAsyncArchive::Serialize calls per sample. This is used to prevent sampling from slowing the game down too much."));

static TAutoConsoleVariable<int32> CVarLogAsyncArchiveSerializeChurn_StackIgnore(
	TEXT("LogAsyncArchiveSerializeChurn.StackIgnore"),
	2,
	TEXT("Number of items to discard from the top of a stack frame."));

static TAutoConsoleVariable<int32> CVarLogAsyncArchiveSerializeChurn_RemoveAliases(
	TEXT("LogAsyncArchiveSerializeChurn.RemoveAliases"),
	1,
	TEXT("If > 0 then remove aliases from the counting process. This essentialy merges addresses that have the same human readable string. It is slower."));

static TAutoConsoleVariable<int32> CVarLogAsyncArchiveSerializeChurn_StackLen(
	TEXT("LogAsyncArchiveSerializeChurn.StackLen"),
	4,
	TEXT("Maximum number of stack frame items to keep. This improves aggregation because calls that originate from multiple places but end up in the same place will be accounted together."));


struct FSampleSerializeChurn
{
	FStackTracker GGameThreadFNameChurnTracker;
	bool bEnabled;
	int32 CountDown;

	FSampleSerializeChurn()
		: bEnabled(false)
		, CountDown(MAX_int32)
	{
	}

	void SerializeHook()
	{
		bool bNewEnabled = CVarLogAsyncArchiveSerializeChurn.GetValueOnGameThread() > 0;
		if (bNewEnabled != bEnabled)
		{
			check(IsInGameThread());
			bEnabled = bNewEnabled;
			if (bEnabled)
			{
				CountDown = CVarLogAsyncArchiveSerializeChurn_SampleFrequency.GetValueOnGameThread();
				GGameThreadFNameChurnTracker.ResetTracking();
				GGameThreadFNameChurnTracker.ToggleTracking(true, true);
			}
			else
			{
				GGameThreadFNameChurnTracker.ToggleTracking(false, true);
				GGameThreadFNameChurnTracker.ResetTracking();
			}
		}
		else if (bEnabled)
		{
			check(IsInGameThread());
			if (--CountDown <= 0)
	{
				CountDown = CVarLogAsyncArchiveSerializeChurn_SampleFrequency.GetValueOnGameThread();
				CollectSample();
			}
		}
	}

	void CollectSample()
	{
		check(IsInGameThread());
		GGameThreadFNameChurnTracker.CaptureStackTrace(CVarLogAsyncArchiveSerializeChurn_StackIgnore.GetValueOnGameThread(), nullptr, CVarLogAsyncArchiveSerializeChurn_StackLen.GetValueOnGameThread(), CVarLogAsyncArchiveSerializeChurn_RemoveAliases.GetValueOnGameThread() > 0);
	}
	void PrintResultsAndReset()
	{
		FOutputDeviceRedirector* Log = FOutputDeviceRedirector::Get();
		float SampleAndFrameCorrection = float(CVarLogAsyncArchiveSerializeChurn_SampleFrequency.GetValueOnGameThread());
		GGameThreadFNameChurnTracker.DumpStackTraces(CVarLogAsyncArchiveSerializeChurn_Threshhold.GetValueOnGameThread(), *Log, SampleAndFrameCorrection);
		GGameThreadFNameChurnTracker.ResetTracking();
	}
};

FSampleSerializeChurn GGameThreadSerializeTracker;

void CallSerializeHook()
{
	if (GIsRunning && IsInGameThread())
	{
		GGameThreadSerializeTracker.SerializeHook();
	}
}

static void DumpSerialize(const TArray<FString>& Args)
{
	if (IsInGameThread())
	{
		GGameThreadSerializeTracker.PrintResultsAndReset();
	}
}

static FAutoConsoleCommand DumpSerializeCmd(
	TEXT("LogAsyncArchiveSerializeChurn.Dump"),
	TEXT("debug command to dump the results of tracking the serialization calls."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&DumpSerialize)
	);
#endif

bool IsEventDrivenLoaderEnabledInCookedBuilds()
{
	return true;
}

bool IsEventDrivenLoaderEnabled()
{
	return FPlatformProperties::RequiresCookedData();
}

//#pragma clang optimize on
