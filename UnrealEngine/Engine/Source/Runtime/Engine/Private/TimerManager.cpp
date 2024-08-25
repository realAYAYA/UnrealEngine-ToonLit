// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TickTaskManager.cpp: Manager for ticking tasks
=============================================================================*/

#include "TimerManager.h"
#include "Containers/StringConv.h"
#include "Engine/GameInstance.h"
#include "Logging/LogScopedCategoryAndVerbosityOverride.h"
#include "Misc/CoreDelegates.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Stats/StatsTrace.h"
#include "UnrealEngine.h"
#include "Misc/TimeGuard.h"
#include "HAL/PlatformStackWalk.h"

DECLARE_CYCLE_STAT(TEXT("SetTimer"), STAT_SetTimer, STATGROUP_Engine);
DECLARE_CYCLE_STAT(TEXT("SetTimeForNextTick"), STAT_SetTimerForNextTick, STATGROUP_Engine);
DECLARE_CYCLE_STAT(TEXT("ClearTimer"), STAT_ClearTimer, STATGROUP_Engine);
DECLARE_CYCLE_STAT(TEXT("ClearAllTimers"), STAT_ClearAllTimers, STATGROUP_Engine);

CSV_DECLARE_CATEGORY_MODULE_EXTERN(CORE_API, Basic);

/** Track the last assigned handle globally */
uint64 FTimerManager::LastAssignedSerialNumber = 0;

static float DumpTimerLogsThreshold = 0.f;
static FAutoConsoleVariableRef CVarDumpTimerLogsThreshold(
	TEXT("TimerManager.DumpTimerLogsThreshold"), DumpTimerLogsThreshold,
	TEXT("Threshold (in milliseconds) after which we log timer info to try and help track down spikes in the timer code. Disabled when set to 0"),
	ECVF_Default);

static int32 DumpTimerLogResolveVirtualFunctions = 1;
static FAutoConsoleVariableRef CVarDumpTimerLogResolveVirtualFunctions(
	TEXT("TimerManager.DumpTimerLogResolveVirtualFunctions"), DumpTimerLogResolveVirtualFunctions,
	TEXT("When logging timer info virtual functions will be resolved, if possible."),
	ECVF_Default);

static int32 DumpTimerLogSymbolNames = 1;
static FAutoConsoleVariableRef CVarDumpTimerLogSymbolNames(
	TEXT("TimerManager.DumpTimerLogSymbolNames"), DumpTimerLogSymbolNames,
	TEXT("When logging timer info, symbol names will be included if set to 1."),
	ECVF_Default);

static int32 MaxExpiredTimersToLog = 30;
static FAutoConsoleVariableRef CVarMaxExpiredTimersToLog(
	TEXT("TimerManager.MaxExpiredTimersToLog"), 
	MaxExpiredTimersToLog,
	TEXT("Maximum number of TimerData exceeding the threshold to log in a single frame."));

#ifndef UE_ENABLE_DUMPALLTIMERLOGSTHRESHOLD
#define UE_ENABLE_DUMPALLTIMERLOGSTHRESHOLD !UE_BUILD_SHIPPING
#endif

#if UE_ENABLE_DUMPALLTIMERLOGSTHRESHOLD
static int32 DumpAllTimerLogsThreshold = -1;
static FAutoConsoleVariableRef CVarDumpAllTimerLogsThreshold(
	TEXT("TimerManager.DumpAllTimerLogsThreshold"),
	DumpAllTimerLogsThreshold,
	TEXT("Threshold (in count of active timers) at which to dump info about all active timers to logs. -1 means this is disabled. NOTE: This will only be dumped once per process launch."));
#endif // #if UE_ENABLE_DUMPALLTIMERLOGSTHRESHOLD


#if UE_ENABLE_TRACKING_TIMER_SOURCES
static int32 GBuildTimerSourceList = 0;
static FAutoConsoleVariableRef CVarGBuildTimerSourceList(
	TEXT("TimerManager.BuildTimerSourceList"), GBuildTimerSourceList,
	TEXT("When non-zero, tracks which timers expire each frame, dumping them during shutdown or when the flag is changed back to 0.")
	TEXT("\n  0: Off")
	TEXT("\n  1: On - Group timers by class (useful to focus on entire systems of things, especially bad spikey frames where we care about aggregates)")
	TEXT("\n  2: On - Do not group timers by class (useful if individual instances are problematic)"),
	ECVF_Default);

// Information about a single timer or timer source being tracked
struct FTimerSourceEntry
{
	uint64 TotalCount = 0;
	double SumApproxRate = 0.0;
	uint32 LastFrameID = 0;
	uint32 NumThisFrame = 0;
	uint32 MaxPerFrame = 0;

	void UpdateEntry(float Rate, uint32 FrameID, int32 CallCount)
	{
		TotalCount += CallCount;
		SumApproxRate += Rate;

		if (LastFrameID != FrameID)
		{
			LastFrameID = FrameID;
			NumThisFrame = 0;
		}
		NumThisFrame += CallCount;

		MaxPerFrame = FMath::Max(NumThisFrame, MaxPerFrame);
	}
};

// List of information about all timers that have expired during a tracking window
struct FTimerSourceList
{
	TMap<FString, FTimerSourceEntry> Entries;
	UGameInstance* OwningGameInstance = nullptr;

	// This is similar to FTimerUnifiedDelegate::ToString() but it tries to find the base class / exclude vptr printout info so that timers are collapsed/aggregated better
	static FString GetPartialDeduplicateDelegateToString(const FTimerUnifiedDelegate& Delegate)
	{
		FString FunctionNameStr;
		FString ObjectNameStr;

		if (Delegate.FuncDelegate.IsBound())
		{
			ObjectNameStr = TEXT("NonDynamicDelegate");
			FName FunctionName;
#if USE_DELEGATE_TRYGETBOUNDFUNCTIONNAME
			FunctionName = Delegate.FuncDelegate.TryGetBoundFunctionName();
#endif
			if (FunctionName.IsNone())
			{
				uint64 ProgramCounter = Delegate.FuncDelegate.GetBoundProgramCounterForTimerManager();
				if (ProgramCounter != 0)
				{
					// Add the function address
					if (DumpTimerLogSymbolNames)
					{
						// Try to resolve the function address to a symbol
						FProgramCounterSymbolInfo SymbolInfo;
						FPlatformStackWalk::ProgramCounterToSymbolInfo(ProgramCounter, /*out*/ SymbolInfo);
						FunctionNameStr = FString::Printf(TEXT("%s [%s:%d]"), ANSI_TO_TCHAR(SymbolInfo.FunctionName), ANSI_TO_TCHAR(SymbolInfo.Filename), SymbolInfo.LineNumber);
					}
					else
					{
						FunctionNameStr = FString::Printf(TEXT("func: 0x%llx"), ProgramCounter);
					}
				}
				else
				{
					FunctionNameStr = TEXT("0x0");
				}
			}
			else
			{
				FunctionNameStr = FunctionName.ToString();
			}
		}
		else if (Delegate.FuncDynDelegate.IsBound())
		{
			const FName FuncFName = Delegate.FuncDynDelegate.GetFunctionName();
			FunctionNameStr = FuncFName.ToString();

			UClass* SourceClass = nullptr;
			if (const UObject* Object = Delegate.FuncDynDelegate.GetUObject())
			{
				SourceClass = Object->GetClass();
				if (UFunction* Func = SourceClass->FindFunctionByName(FuncFName))
				{
					SourceClass = Func->GetOwnerClass();
				}
			}


			ObjectNameStr = GetPathNameSafe(SourceClass);
		}
		else
		{
			ObjectNameStr = TEXT("NotBound!");
			FunctionNameStr = TEXT("NotBound!");
		}

		return FString::Printf(TEXT("%s,\"%s\""), *ObjectNameStr, *FunctionNameStr);
	}

	void AddEntry(const FTimerData& Data, int32 CallCount)
	{
		Entries.FindOrAdd((GBuildTimerSourceList == 2) ? Data.TimerDelegate.ToString() : GetPartialDeduplicateDelegateToString(Data.TimerDelegate)).UpdateEntry(Data.Rate, GFrameCounter, CallCount);
	}

	void DumpEntries()
	{
		FString AdditionalInfoAboutGameInstance;
		if (OwningGameInstance != nullptr)
		{
			if (FWorldContext* WorldContext = OwningGameInstance->GetWorldContext())
			{
				if (WorldContext->RunAsDedicated)
				{
					AdditionalInfoAboutGameInstance = TEXT(" (dedicated server)");
				}
			}
		}

		UE_LOG(LogEngine, Log, TEXT("-----------------------"));
		UE_LOG(LogEngine, Log, TEXT("Listing Expired Timer Stats for OwningGameInstance=%s%s"), *GetPathNameSafe(OwningGameInstance), *AdditionalInfoAboutGameInstance);
		
		UE_LOG(LogEngine, Log, TEXT("TotalExpires,RateAvg,PeakInFrame,ObjectOrClass,FunctionName"));
		Entries.ValueSort([](const FTimerSourceEntry& A, const FTimerSourceEntry& B) { return A.TotalCount > B.TotalCount; });
		for (const auto& KVP : Entries)
		{
			UE_LOG(LogEngine, Log, TEXT("%llu,%.2f,%u,%s"), KVP.Value.TotalCount, KVP.Value.SumApproxRate / KVP.Value.TotalCount, KVP.Value.MaxPerFrame, *KVP.Key);
		}

		UE_LOG(LogEngine, Log, TEXT("-----------------------"));
	}

	~FTimerSourceList()
	{
		DumpEntries();
	}
};
#endif // UE_ENABLE_TRACKING_TIMER_SOURCES

namespace
{
	void DescribeFTimerDataSafely(FOutputDevice& Ar, const FTimerData& Data)
	{
		Ar.Logf(
			TEXT("TimerData %p : bLoop=%s, bRequiresDelegate=%s, Status=%d, Rate=%f, ExpireTime=%f, Delegate=%s"),
			&Data,
			Data.bLoop ? TEXT("true") : TEXT("false"),
			Data.bRequiresDelegate ? TEXT("true") : TEXT("false"),
			static_cast<int32>(Data.Status),
			Data.Rate,
			Data.ExpireTime,
			*Data.TimerDelegate.ToString()
		);
	}

	FString GetFTimerDataSafely(const FTimerData& Data)
	{
		FStringOutputDevice Output;
		DescribeFTimerDataSafely(Output, Data);
		return MoveTemp(Output);
	}
}

struct FTimerHeapOrder
{
	explicit FTimerHeapOrder(const TSparseArray<FTimerData>& InTimers)
		: Timers(InTimers)
		, NumTimers(InTimers.Num())
	{
	}

	bool operator()(FTimerHandle LhsHandle, FTimerHandle RhsHandle) const
	{
		int32 LhsIndex = LhsHandle.GetIndex();
		int32 RhsIndex = RhsHandle.GetIndex();

		const FTimerData& LhsData = Timers[LhsIndex];
		const FTimerData& RhsData = Timers[RhsIndex];

		return LhsData.ExpireTime < RhsData.ExpireTime;
	}

	const TSparseArray<FTimerData>& Timers;
	int32 NumTimers;
};

FTimerManager::FTimerManager(UGameInstance* GameInstance)
	: InternalTime(0.0)
	, LastTickedFrame(static_cast<uint64>(-1))
	, OwningGameInstance(nullptr)
{
	if (IsRunningDedicatedServer())
	{
		// Off by default, reenable if needed
		//FCoreDelegates::OnHandleSystemError.AddRaw(this, &FTimerManager::OnCrash);
	}

	if (GameInstance)
	{
		SetGameInstance(GameInstance);
	}
}

FTimerManager::~FTimerManager()
{
	if (IsRunningDedicatedServer())
	{
		FCoreDelegates::OnHandleSystemError.RemoveAll(this);
	}
}

void FTimerManager::OnCrash()
{
	UE_LOG(LogEngine, Warning, TEXT("TimerManager %p on crashing delegate called, dumping extra information"), this);

	UE_LOG(LogEngine, Log, TEXT("------- %d Active Timers (including expired) -------"), ActiveTimerHeap.Num());
	int32 ExpiredActiveTimerCount = 0;
	for (FTimerHandle Handle : ActiveTimerHeap)
	{
		const FTimerData& Timer = GetTimer(Handle);
		if (Timer.Status == ETimerStatus::ActivePendingRemoval)
		{
			++ExpiredActiveTimerCount;
		}
		else
		{
			DescribeFTimerDataSafely(*GLog, Timer);
		}
	}
	UE_LOG(LogEngine, Log, TEXT("------- %d Expired Active Timers -------"), ExpiredActiveTimerCount);

	UE_LOG(LogEngine, Log, TEXT("------- %d Paused Timers -------"), PausedTimerSet.Num());
	for (FTimerHandle Handle : PausedTimerSet)
	{
		const FTimerData& Timer = GetTimer(Handle);
		DescribeFTimerDataSafely(*GLog, Timer);
	}

	UE_LOG(LogEngine, Log, TEXT("------- %d Pending Timers -------"), PendingTimerSet.Num());
	for (FTimerHandle Handle : PendingTimerSet)
	{
		const FTimerData& Timer = GetTimer(Handle);
		DescribeFTimerDataSafely(*GLog, Timer);
	}

	UE_LOG(LogEngine, Log, TEXT("------- %d Total Timers -------"), PendingTimerSet.Num() + PausedTimerSet.Num() + ActiveTimerHeap.Num() - ExpiredActiveTimerCount);

	UE_LOG(LogEngine, Warning, TEXT("TimerManager %p dump ended"), this);
}


FString FTimerUnifiedDelegate::ToString() const
{
	const UObject* Object = nullptr;
	FString FunctionNameStr;
	bool bDynDelegate = false;

	if (FuncDelegate.IsBound())
	{
		FName FunctionName;
#if USE_DELEGATE_TRYGETBOUNDFUNCTIONNAME
		FunctionName = FuncDelegate.TryGetBoundFunctionName();
#endif
		if (FunctionName.IsNone())
		{
			void** VtableAddr = nullptr;
#if PLATFORM_COMPILER_CLANG || defined(_MSC_VER)
			// Add the vtable address
			const void* UserObject = FuncDelegate.GetObjectForTimerManager();
			if (UserObject)
			{
				VtableAddr = *(void***)UserObject;
				FunctionNameStr = FString::Printf(TEXT("vtbl: %p"), VtableAddr);
			}
#endif // PLATFORM_COMPILER_CLANG

			uint64 ProgramCounter = FuncDelegate.GetBoundProgramCounterForTimerManager();
			if (ProgramCounter != 0)
			{
				// Add the function address

#if PLATFORM_COMPILER_CLANG
				// See if this is a virtual function. Heuristic is that real function addresses are higher than some value, and vtable offsets are lower
				const uint64 MaxVTableAddressOffset = 32768;
				if (DumpTimerLogResolveVirtualFunctions && VtableAddr && ProgramCounter > 0 && ProgramCounter < MaxVTableAddressOffset)
				{
					// If the ProgramCounter is just an offset to the vtable (virtual member function) then resolve the actual ProgramCounter here.
					ProgramCounter = (uint64)VtableAddr[ProgramCounter / sizeof(void*)];
				}
#endif // PLATFORM_COMPILER_CLANG

				FunctionNameStr += FString::Printf(TEXT(" func: 0x%llx"), ProgramCounter);

				if (DumpTimerLogSymbolNames)
				{
					// Try to resolve the function address to a symbol
					FProgramCounterSymbolInfo SymbolInfo;
					SymbolInfo.FunctionName[0] = 0;
					SymbolInfo.Filename[0] = 0;
					SymbolInfo.LineNumber = 0;
					FPlatformStackWalk::ProgramCounterToSymbolInfo(ProgramCounter, SymbolInfo);
					FunctionNameStr += FString::Printf(TEXT(" %s [%s:%d]"), ANSI_TO_TCHAR(SymbolInfo.FunctionName), ANSI_TO_TCHAR(SymbolInfo.Filename), SymbolInfo.LineNumber);
				}
			}
			else
			{
				FunctionNameStr = TEXT(" 0x0");
			}
		}
		else
		{
			FunctionNameStr = FunctionName.ToString();
		}
	}
	else if (FuncDynDelegate.IsBound())
	{
		Object = FuncDynDelegate.GetUObject();
		FunctionNameStr = FuncDynDelegate.GetFunctionName().ToString();
		bDynDelegate = true;
	}
	else
	{
		FunctionNameStr = TEXT("NotBound!");
	}

	return FString::Printf(TEXT("%s,%s,%s"), bDynDelegate ? TEXT("DYN DELEGATE") : TEXT("DELEGATE"), Object == nullptr ? TEXT("NO OBJ") : *Object->GetPathName(), *FunctionNameStr);
}

// ---------------------------------
// Private members
// ---------------------------------

FTimerData& FTimerManager::GetTimer(FTimerHandle const& InHandle)
{
	int32 Index = InHandle.GetIndex();
	checkSlow(Index >= 0 && Index < Timers.GetMaxIndex() && Timers.IsAllocated(Index) && Timers[Index].Handle == InHandle);
	FTimerData& Timer = Timers[Index];
	return Timer;
}

FTimerData* FTimerManager::FindTimer(FTimerHandle const& InHandle)
{
	// not currently threadsafe
	check(IsInGameThread());

	if (!InHandle.IsValid())
	{
		return nullptr;
	}

	int32 Index = InHandle.GetIndex();
	if (Index < 0 || Index >= Timers.GetMaxIndex() || !Timers.IsAllocated(Index))
	{
		return nullptr;
	}

	FTimerData& Timer = Timers[Index];

	if (Timer.Handle != InHandle || Timer.Status == ETimerStatus::ActivePendingRemoval)
	{
		return nullptr;
	}

	return &Timer;
}

/** Finds a handle to a dynamic timer bound to a particular pointer and function name. */
FTimerHandle FTimerManager::K2_FindDynamicTimerHandle(FTimerDynamicDelegate InDynamicDelegate) const
{
	FTimerHandle Result;

	if (const void* Obj = InDynamicDelegate.GetUObject())
	{
		if (const TSet<FTimerHandle>* TimersForObject = ObjectToTimers.Find(Obj))
		{
			for (FTimerHandle Handle : *TimersForObject)
			{
				const FTimerData& Data = GetTimer(Handle);
				if (Data.Status != ETimerStatus::ActivePendingRemoval && Data.TimerDelegate.FuncDynDelegate == InDynamicDelegate)
				{
					Result = Handle;
					break;
				}
			}
		}
	}

	return Result;
}

void FTimerManager::InternalSetTimer(FTimerHandle& InOutHandle, FTimerUnifiedDelegate&& InDelegate, float InRate, bool bInLoop, float InFirstDelay)
{
	InternalSetTimer(InOutHandle, MoveTemp(InDelegate), InRate, FTimerManagerTimerParameters{ .bLoop = bInLoop, .FirstDelay = InFirstDelay });
}

void FTimerManager::InternalSetTimer(FTimerHandle& InOutHandle, FTimerUnifiedDelegate&& InDelegate, float InRate, const FTimerManagerTimerParameters& InTimerParameters)
{
	SCOPE_CYCLE_COUNTER(STAT_SetTimer);

	// not currently threadsafe
	check(IsInGameThread());

	if (FindTimer(InOutHandle))
	{
		// if the timer is already set, just clear it and we'll re-add it, since 
		// there's no data to maintain.
		InternalClearTimer(InOutHandle);
	}

	if (InRate > 0.f)
	{
		// set up the new timer
		FTimerData NewTimerData;
		NewTimerData.TimerDelegate = MoveTemp(InDelegate);

		NewTimerData.Rate = InRate;
		NewTimerData.bLoop = InTimerParameters.bLoop;
		NewTimerData.bMaxOncePerFrame = InTimerParameters.bMaxOncePerFrame;
		NewTimerData.bRequiresDelegate = NewTimerData.TimerDelegate.IsBound();

		// Set level collection
		const UWorld* const OwningWorld = OwningGameInstance ? OwningGameInstance->GetWorld() : nullptr;
		if (OwningWorld && OwningWorld->GetActiveLevelCollection())
		{
			NewTimerData.LevelCollection = OwningWorld->GetActiveLevelCollection()->GetType();
		}

		const float FirstDelay = (InTimerParameters.FirstDelay >= 0.f) ? InTimerParameters.FirstDelay : InRate;

		FTimerHandle NewTimerHandle;
		if (HasBeenTickedThisFrame())
		{
			NewTimerData.ExpireTime = InternalTime + FirstDelay;
			NewTimerData.Status = ETimerStatus::Active;
			NewTimerHandle = AddTimer(MoveTemp(NewTimerData));
			ActiveTimerHeap.HeapPush(NewTimerHandle, FTimerHeapOrder(Timers));
		}
		else
		{
			// Store time remaining in ExpireTime while pending
			NewTimerData.ExpireTime = FirstDelay;
			NewTimerData.Status = ETimerStatus::Pending;
			NewTimerHandle = AddTimer(MoveTemp(NewTimerData));
			PendingTimerSet.Add(NewTimerHandle);
		}

		InOutHandle = NewTimerHandle;
	}
	else
	{
		InOutHandle.Invalidate();
	}
}

FTimerHandle FTimerManager::InternalSetTimerForNextTick(FTimerUnifiedDelegate&& InDelegate)
{
	SCOPE_CYCLE_COUNTER(STAT_SetTimerForNextTick);

	// not currently threadsafe
	check(IsInGameThread());

	FTimerData NewTimerData;
	NewTimerData.Rate = 0.f;
	NewTimerData.bLoop = false;
	NewTimerData.bRequiresDelegate = true;
	NewTimerData.TimerDelegate = MoveTemp(InDelegate);
	NewTimerData.ExpireTime = InternalTime;
	NewTimerData.Status = ETimerStatus::Active;

	// Set level collection
	const UWorld* const OwningWorld = OwningGameInstance ? OwningGameInstance->GetWorld() : nullptr;
	if (OwningWorld && OwningWorld->GetActiveLevelCollection())
	{
		NewTimerData.LevelCollection = OwningWorld->GetActiveLevelCollection()->GetType();
	}

	FTimerHandle NewTimerHandle = AddTimer(MoveTemp(NewTimerData));
	ActiveTimerHeap.HeapPush(NewTimerHandle, FTimerHeapOrder(Timers));

	return NewTimerHandle;
}

void FTimerManager::InternalClearTimer(FTimerHandle InHandle)
{
	SCOPE_CYCLE_COUNTER(STAT_ClearTimer);

	// not currently threadsafe
	check(IsInGameThread());

	FTimerData& Data = GetTimer(InHandle);
	switch (Data.Status)
	{
		case ETimerStatus::Pending:
			{
				int32 NumRemoved = PendingTimerSet.Remove(InHandle);
				check(NumRemoved == 1);
				RemoveTimer(InHandle);
			}
			break;

		case ETimerStatus::Active:
			Data.Status = ETimerStatus::ActivePendingRemoval;
			break;

		case ETimerStatus::ActivePendingRemoval:
			// Already removed
			break;

		case ETimerStatus::Paused:
			{
				int32 NumRemoved = PausedTimerSet.Remove(InHandle);
				check(NumRemoved == 1);
				RemoveTimer(InHandle);
			}
			break;

		case ETimerStatus::Executing:
			check(CurrentlyExecutingTimer == InHandle);

			// Edge case. We're currently handling this timer when it got cleared.  Clear it to prevent it firing again
			// in case it was scheduled to fire multiple times.
			CurrentlyExecutingTimer.Invalidate();
			RemoveTimer(InHandle);
			break;

		default:
			check(false);
	}
}

void FTimerManager::InternalClearAllTimers(void const* Object)
{
	SCOPE_CYCLE_COUNTER(STAT_ClearAllTimers);

	if (!Object)
	{
		return;
	}

	TSet<FTimerHandle>* TimersToRemove = ObjectToTimers.Find(Object);
	if (!TimersToRemove)
	{
		return;
	}

	TSet<FTimerHandle> LocalTimersToRemove = *TimersToRemove;
	for (FTimerHandle TimerToRemove : LocalTimersToRemove)
	{
		InternalClearTimer(TimerToRemove);
	}
}

float FTimerManager::InternalGetTimerRemaining(FTimerData const* const TimerData) const
{
	if (TimerData)
	{
		switch (TimerData->Status)
		{
			case ETimerStatus::Active:
				return TimerData->ExpireTime - InternalTime;

			case ETimerStatus::Executing:
				return 0.0f;

			default:
				// ExpireTime is time remaining for paused timers
				return TimerData->ExpireTime;
		}
	}

	return -1.f;
}

float FTimerManager::InternalGetTimerElapsed(FTimerData const* const TimerData) const
{
	if (TimerData)
	{
		switch (TimerData->Status)
		{
			case ETimerStatus::Active:
			case ETimerStatus::Executing:
				return (TimerData->Rate - (TimerData->ExpireTime - InternalTime));

			default:
				// ExpireTime is time remaining for paused timers
				return (TimerData->Rate - TimerData->ExpireTime);
		}
	}

	return -1.f;
}

float FTimerManager::InternalGetTimerRate(FTimerData const* const TimerData) const
{
	if (TimerData)
	{
		return TimerData->Rate;
	}
	return -1.f;
}

void FTimerManager::PauseTimer(FTimerHandle InHandle)
{
	// not currently threadsafe
	check(IsInGameThread());

	FTimerData* TimerToPause = FindTimer(InHandle);
	if (!TimerToPause || TimerToPause->Status == ETimerStatus::Paused)
	{
		return;
	}

	ETimerStatus PreviousStatus = TimerToPause->Status;

	// Remove from previous TArray
	switch( PreviousStatus )
	{
		case ETimerStatus::ActivePendingRemoval:
			break;

		case ETimerStatus::Active:
			{
				int32 IndexIndex = ActiveTimerHeap.Find(InHandle);
				check(IndexIndex != INDEX_NONE);
				ActiveTimerHeap.HeapRemoveAt(IndexIndex, FTimerHeapOrder(Timers), EAllowShrinking::No);
			}
			break;

		case ETimerStatus::Pending:
			{
				int32 NumRemoved = PendingTimerSet.Remove(InHandle);
				check(NumRemoved == 1);
			}
			break;

		case ETimerStatus::Executing:
			check(CurrentlyExecutingTimer == InHandle);

			CurrentlyExecutingTimer.Invalidate();
			break;

		default:
			check(false);
	}

	// Don't pause the timer if it's currently executing and isn't going to loop
	if( PreviousStatus == ETimerStatus::Executing && !TimerToPause->bLoop )
	{
		RemoveTimer(InHandle);
	}
	else
	{
		// Add to Paused list
		PausedTimerSet.Add(InHandle);

		// Set new status
		TimerToPause->Status = ETimerStatus::Paused;

		// Store time remaining in ExpireTime while paused. Don't do this if the timer is in the pending list.
		if (PreviousStatus != ETimerStatus::Pending)
		{
			TimerToPause->ExpireTime -= InternalTime;
		}
	}
}

void FTimerManager::UnPauseTimer(FTimerHandle InHandle)
{
	// not currently threadsafe
	check(IsInGameThread());

	FTimerData* TimerToUnPause = FindTimer(InHandle);
	if (!TimerToUnPause || TimerToUnPause->Status != ETimerStatus::Paused)
	{
		return;
	}

	// Move it out of paused list and into proper TArray
	if( HasBeenTickedThisFrame() )
	{
		// Convert from time remaining back to a valid ExpireTime
		TimerToUnPause->ExpireTime += InternalTime;
		TimerToUnPause->Status = ETimerStatus::Active;
		ActiveTimerHeap.HeapPush(InHandle, FTimerHeapOrder(Timers));
	}
	else
	{
		TimerToUnPause->Status = ETimerStatus::Pending;
		PendingTimerSet.Add(InHandle);
	}

	// remove from paused list
	PausedTimerSet.Remove(InHandle);
}

FTimerData::FTimerData()
	: bLoop(false)
	, bMaxOncePerFrame(false)
	, bRequiresDelegate(false)
	, Status(ETimerStatus::Active)
	, Rate(0)
	, ExpireTime(0)
	, LevelCollection(ELevelCollectionType::DynamicSourceLevels)
{}

// ---------------------------------
// Public members
// ---------------------------------

DECLARE_DWORD_COUNTER_STAT(TEXT("TimerManager Heap Size"),STAT_NumHeapEntries,STATGROUP_Game);

void FTimerManager::Tick(float DeltaTime)
{
	SCOPED_NAMED_EVENT(FTimerManager_Tick, FColor::Orange);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(TimerManager);

#if DO_TIMEGUARD && 0
	TArray<FTimerUnifiedDelegate> RunTimerDelegates;
	FTimerNameDelegate NameFunction = FTimerNameDelegate::CreateLambda([&] {
			FString ActiveDelegates;
			for ( const FTimerUnifiedDelegate& Descriptor : RunTimerDelegates )
			{
				ActiveDelegates += FString::Printf(TEXT("Delegate %s, "), *Descriptor.ToString() );
			}
			return FString::Printf(TEXT("UWorld::Tick - TimerManager, %s"), *ActiveDelegates);
		});


	// no delegate should take longer then 5ms to run 
	SCOPE_TIME_GUARD_DELEGATE_MS(NameFunction, 5);
#endif	

	// @todo, might need to handle long-running case
	// (e.g. every X seconds, renormalize to InternalTime = 0)

	INC_DWORD_STAT_BY(STAT_NumHeapEntries, ActiveTimerHeap.Num());

	if (HasBeenTickedThisFrame())
	{
		return;
	}

#if UE_ENABLE_TRACKING_TIMER_SOURCES
	if ((TimerSourceList == nullptr) != (GBuildTimerSourceList == 0))
	{
		if (TimerSourceList == nullptr)
		{
			TimerSourceList.Reset(new FTimerSourceList);
			TimerSourceList->OwningGameInstance = OwningGameInstance;
		}
		else
		{
			TimerSourceList.Reset();
		}
	}
#endif

	const double StartTime = FPlatformTime::Seconds();
	bool bDumpTimerLogsThresholdExceeded = false;
	int32 NbExpiredTimers = 0;

	InternalTime += DeltaTime;

	UWorld* const OwningWorld = OwningGameInstance ? OwningGameInstance->GetWorld() : nullptr;
	UWorld* const LevelCollectionWorld = OwningWorld;

#if UE_ENABLE_DUMPALLTIMERLOGSTHRESHOLD
	// Dump timer info to logs if we have way too many timers active.
	UE_SUPPRESS(LogEngine, Warning,
	{
		if (DumpAllTimerLogsThreshold > 0 && ActiveTimerHeap.Num() > DumpAllTimerLogsThreshold)
		{
			static bool bAlreadyLogged = false;
			if(!bAlreadyLogged)
			{
				bAlreadyLogged = true;
			
				UE_LOG(LogEngine, Warning, TEXT("Number of active Timers (%d) has exceeded DumpAllTimerLogsThreshold (%d)!  Dumping all timer info to log:"), ActiveTimerHeap.Num(), DumpAllTimerLogsThreshold);

				TArray<const FTimerData*> ValidActiveTimers;
				ValidActiveTimers.Reserve(ActiveTimerHeap.Num());
				for (FTimerHandle Handle : ActiveTimerHeap)
				{
					if (const FTimerData* Data = FindTimer(Handle))
					{
						ValidActiveTimers.Add(Data);
					}
				}

				for (const FTimerData* Data : ValidActiveTimers)
				{
					DescribeFTimerDataSafely(*GLog, *Data);
				}
			}
		}
	});
#endif // #if UE_ENABLE_DUMPALLTIMERLOGSTHRESHOLD

	while (ActiveTimerHeap.Num() > 0)
	{
		FTimerHandle TopHandle = ActiveTimerHeap.HeapTop();

		// Test for expired timers
		int32 TopIndex = TopHandle.GetIndex();
		FTimerData* Top = &Timers[TopIndex];

		if (Top->Status == ETimerStatus::ActivePendingRemoval)
		{
			ActiveTimerHeap.HeapPop(TopHandle, FTimerHeapOrder(Timers), EAllowShrinking::No);
			RemoveTimer(TopHandle);
			continue;
		}

		if (InternalTime > Top->ExpireTime)
		{
			// Timer has expired! Fire the delegate, then handle potential looping.

			if (bDumpTimerLogsThresholdExceeded)
			{
				++NbExpiredTimers;
				if (NbExpiredTimers <= MaxExpiredTimersToLog)
				{
					DescribeFTimerDataSafely(*GLog, *Top);
				}
			}

			// Set the relevant level context for this timer
			const int32 LevelCollectionIndex = OwningWorld ? OwningWorld->FindCollectionIndexByType(Top->LevelCollection) : INDEX_NONE;
			
			FScopedLevelCollectionContextSwitch LevelContext(LevelCollectionIndex, LevelCollectionWorld);

			// Remove it from the heap and store it while we're executing
			ActiveTimerHeap.HeapPop(CurrentlyExecutingTimer, FTimerHeapOrder(Timers), EAllowShrinking::No);
			Top->Status = ETimerStatus::Executing;

			// Determine how many times the timer may have elapsed (e.g. for large DeltaTime on a short looping timer)
			int32 const CallCount = Top->bLoop ? 
				FMath::TruncToInt( (InternalTime - Top->ExpireTime) / Top->Rate ) + 1
				: 1;

#if UE_ENABLE_TRACKING_TIMER_SOURCES
			if (TimerSourceList.IsValid())
			{
				//@TODO: The actual call count may be less, e.g., if the delegate clears itself during the loop below
				TimerSourceList->AddEntry(*Top, CallCount);
			}
#endif

			// Now call the function
			for (int32 CallIdx=0; CallIdx<CallCount; ++CallIdx)
			{ 
#if DO_TIMEGUARD && 0
				FTimerNameDelegate NameFunction = FTimerNameDelegate::CreateLambda([&] { 
						return FString::Printf(TEXT("FTimerManager slowtick from delegate %s "), *Top->TimerDelegate.ToString());
					});
				// no delegate should take longer then 2ms to run 
				SCOPE_TIME_GUARD_DELEGATE_MS(NameFunction, 2);
#endif
#if DO_TIMEGUARD && 0
				RunTimerDelegates.Add(Top->TimerDelegate);
#endif

				checkf(!WillRemoveTimerAssert(CurrentlyExecutingTimer), TEXT("RemoveTimer(CurrentlyExecutingTimer) - due to fail before Execute()"));
				Top->TimerDelegate.Execute();

				// Update Top pointer, in case it has been invalidated by the Execute call
				Top = FindTimer(CurrentlyExecutingTimer);
				checkf(!Top || !WillRemoveTimerAssert(CurrentlyExecutingTimer), TEXT("RemoveTimer(CurrentlyExecutingTimer) - due to fail after Execute()"));
				if (!Top || Top->Status != ETimerStatus::Executing || Top->bMaxOncePerFrame)
				{
					break;
				}
			}

			if (DumpTimerLogsThreshold > 0.f && !bDumpTimerLogsThresholdExceeded)
			{
				// help us hunt down outliers that cause our timer manager times to spike.  Recommended that users set meaningful DumpTimerLogsThresholds in appropriate ini files if they are seeing spikes in the timer manager.
				const double DeltaT = (FPlatformTime::Seconds() - StartTime) * 1000.f;
				if (DeltaT >= DumpTimerLogsThreshold)
				{
					bDumpTimerLogsThresholdExceeded = true;
                    ++NbExpiredTimers;
					UE_LOG(LogEngine, Log, TEXT("TimerManager's time threshold of %.2fms exceeded with a deltaT of %.4f, dumping current timer data."), DumpTimerLogsThreshold, DeltaT);

					if (Top)
					{
						DescribeFTimerDataSafely(*GLog, *Top);
					}
					else
					{
						UE_LOG(LogEngine, Log, TEXT("There was no timer data for the first timer after exceeding the time threshold!"));
					}
				}
			}

			// test to ensure it didn't get cleared during execution
			if (Top)
			{
				// if timer requires a delegate, make sure it's still validly bound (i.e. the delegate's object didn't get deleted or something)
				if (Top->bLoop && (!Top->bRequiresDelegate || Top->TimerDelegate.IsBound()))
				{
					// Put this timer back on the heap
					Top->ExpireTime += CallCount * Top->Rate;
					Top->Status = ETimerStatus::Active;
					ActiveTimerHeap.HeapPush(CurrentlyExecutingTimer, FTimerHeapOrder(Timers));
				}
				else
				{
					RemoveTimer(CurrentlyExecutingTimer);
				}

				CurrentlyExecutingTimer.Invalidate();
			}
		}
		else
		{
			// no need to go further down the heap, we can be finished
			break;
		}
	}

	if (NbExpiredTimers > MaxExpiredTimersToLog)
	{
		UE_LOG(LogEngine, Log, TEXT("TimerManager's caught %d Timers exceeding the time threshold. Only the first %d were logged."), NbExpiredTimers, MaxExpiredTimersToLog);
	}

	// Timer has been ticked.
	LastTickedFrame = GFrameCounter;

	// If we have any Pending Timers, add them to the Active Queue.
	if( PendingTimerSet.Num() > 0 )
	{
		for (FTimerHandle Handle : PendingTimerSet)
		{
			FTimerData& TimerToActivate = GetTimer(Handle);

			// Convert from time remaining back to a valid ExpireTime
			TimerToActivate.ExpireTime += InternalTime;
			TimerToActivate.Status = ETimerStatus::Active;
			ActiveTimerHeap.HeapPush( Handle, FTimerHeapOrder(Timers) );
		}
		PendingTimerSet.Reset();
	}
}

TStatId FTimerManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FTimerManager, STATGROUP_Tickables);
}

void FTimerManager::SetGameInstance(UGameInstance* InGameInstance)
{
	OwningGameInstance = InGameInstance;

#if UE_ENABLE_TRACKING_TIMER_SOURCES
	if (TimerSourceList.IsValid())
	{
		TimerSourceList->OwningGameInstance = OwningGameInstance;
	}
#endif
}

void FTimerManager::ListTimers() const
{
	// not currently threadsafe
	check(IsInGameThread());

	TArray<const FTimerData*> ValidActiveTimers;
	ValidActiveTimers.Reserve(ActiveTimerHeap.Num());
	for (FTimerHandle Handle : ActiveTimerHeap)
	{
		if (const FTimerData* Data = FindTimer(Handle))
		{
			ValidActiveTimers.Add(Data);
		}
	}

	UE_LOG(LogEngine, Log, TEXT("------- %d Active Timers -------"), ValidActiveTimers.Num());
	for (const FTimerData* Data : ValidActiveTimers)
	{
		check(Data);
		DescribeFTimerDataSafely(*GLog, *Data);
	}

	UE_LOG(LogEngine, Log, TEXT("------- %d Paused Timers -------"), PausedTimerSet.Num());
	for (FTimerHandle Handle : PausedTimerSet)
	{
		const FTimerData& Data = GetTimer(Handle);
		DescribeFTimerDataSafely(*GLog, Data);
	}

	UE_LOG(LogEngine, Log, TEXT("------- %d Pending Timers -------"), PendingTimerSet.Num());
	for (FTimerHandle Handle : PendingTimerSet)
	{
		const FTimerData& Data = GetTimer(Handle);
		DescribeFTimerDataSafely(*GLog, Data);
	}

	UE_LOG(LogEngine, Log, TEXT("------- %d Total Timers -------"), PendingTimerSet.Num() + PausedTimerSet.Num() + ValidActiveTimers.Num());
}

FTimerHandle FTimerManager::AddTimer(FTimerData&& TimerData)
{
	const void* TimerIndicesByObjectKey = TimerData.TimerDelegate.GetBoundObject();
	TimerData.TimerIndicesByObjectKey = TimerIndicesByObjectKey;

	int32 NewIndex = Timers.Add(MoveTemp(TimerData));

	FTimerHandle Result = GenerateHandle(NewIndex);
	Timers[NewIndex].Handle = Result;

	if (TimerIndicesByObjectKey)
	{
		TSet<FTimerHandle>& HandleSet = ObjectToTimers.FindOrAdd(TimerIndicesByObjectKey);

		bool bAlreadyExists = false;
		HandleSet.Add(Result, &bAlreadyExists);
		checkf(!bAlreadyExists, TEXT("A timer with this handle and object has already been added! (%s)"), *GetFTimerDataSafely(TimerData));
	}

	return Result;
}

void FTimerManager::RemoveTimer(FTimerHandle Handle)
{
	const FTimerData& Data = GetTimer(Handle);

	// Remove TimerIndicesByObject entry if necessary
	if (const void* TimerIndicesByObjectKey = Data.TimerIndicesByObjectKey)
	{
		TSet<FTimerHandle>* TimersForObject = ObjectToTimers.Find(TimerIndicesByObjectKey);
		checkf(TimersForObject, TEXT("Removed timer was bound to an object which is not tracked by ObjectToTimers! (%s)"), *GetFTimerDataSafely(Data));

		int32 NumRemoved = TimersForObject->Remove(Handle);
		checkf(NumRemoved == 1, TEXT("Removed timer was bound to an object which is not tracked by ObjectToTimers! (%s)"), *GetFTimerDataSafely(Data));

		if (TimersForObject->Num() == 0)
		{
			ObjectToTimers.Remove(TimerIndicesByObjectKey);
		}
	}

	Timers.RemoveAt(Handle.GetIndex());
}

bool FTimerManager::WillRemoveTimerAssert(FTimerHandle Handle) const
{
	const FTimerData& Data = GetTimer(Handle);

	// Remove TimerIndicesByObject entry if necessary
	if (const void* TimerIndicesByObjectKey = Data.TimerIndicesByObjectKey)
	{
		const TSet<FTimerHandle>* TimersForObject = ObjectToTimers.Find(TimerIndicesByObjectKey);
		if (!TimersForObject)
		{
			return true;
		}

		const FTimerHandle* Found = TimersForObject->Find(Handle);
		if (!Found)
		{
			return true;
		}
	}

	return false;
}

FTimerHandle FTimerManager::GenerateHandle(int32 Index)
{
	uint64 NewSerialNumber = ++LastAssignedSerialNumber;
	if (!ensureMsgf(NewSerialNumber != FTimerHandle::MaxSerialNumber, TEXT("Timer serial number has wrapped around!")))
	{
		NewSerialNumber = (uint64)1;
	}

	FTimerHandle Result;
	Result.SetIndexAndSerialNumber(Index, NewSerialNumber);
	return Result;
}


// Handler for ListTimers console command
static void OnListTimers(UWorld* World)
{
	if(World != nullptr)
	{
		World->GetTimerManager().ListTimers();
	}
}

// Register ListTimers console command, needs a World context
FAutoConsoleCommandWithWorld ListTimersConsoleCommand(
	TEXT("ListTimers"),
	TEXT(""),
	FConsoleCommandWithWorldDelegate::CreateStatic(OnListTimers)
	);
