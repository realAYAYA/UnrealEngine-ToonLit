// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/OutputDeviceRedirector.h"

#include "Async/EventCount.h"
#include "Containers/BitArray.h"
#include "Containers/ConsumeAllMpmcQueue.h"
#include "Experimental/ConcurrentLinearAllocator.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformTLS.h"
#include "HAL/Thread.h"
#include "Logging/StructuredLog.h"
#include "Misc/App.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/StringBuilder.h"
#include "Misc/TVariant.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include <atomic>

/*-----------------------------------------------------------------------------
	FOutputDeviceRedirector.
-----------------------------------------------------------------------------*/

FBufferedLine::FBufferedLine(const TCHAR* InData, const FName& InCategory, ELogVerbosity::Type InVerbosity, double InTime)
	: Category(InCategory)
	, Time(InTime)
	, Verbosity(InVerbosity)
{
	int32 NumChars = FCString::Strlen(InData) + 1;
	Data = MakeUniqueForOverwrite<TCHAR[]>(sizeof(TCHAR) * NumChars);
	FMemory::Memcpy(Data.Get(), InData, sizeof(TCHAR) * NumChars);
}

namespace UE::Private
{

struct FOutputDeviceBlockAllocationTag : FDefaultBlockAllocationTag
{
	static constexpr const char* TagName = "OutputDeviceLinear";

	using Allocator = FOsAllocator;
};

struct FOutputDeviceLinearAllocator
{
	FORCEINLINE static void* Malloc(SIZE_T Size, uint32 Alignment)
	{
		return TConcurrentLinearAllocator<FOutputDeviceBlockAllocationTag>::Malloc(Size, Alignment);
	}

	FORCEINLINE static void Free(void* Pointer)
	{
		TConcurrentLinearAllocator<FOutputDeviceBlockAllocationTag>::Free(Pointer);
	}
};

struct FOutputDeviceLine
{
	const double Time;
	const TCHAR* Data;
	const FName Category;
	const ELogVerbosity::Type Verbosity;

	FOutputDeviceLine(const FBufferedLine&) = delete;
	FOutputDeviceLine& operator=(const FBufferedLine&) = delete;

	FORCEINLINE FOutputDeviceLine(const TCHAR* const InData, const FName& InCategory, const ELogVerbosity::Type InVerbosity, const double InTime)
		: Time(InTime)
		, Data(CopyData(InData))
		, Category(InCategory)
		, Verbosity(InVerbosity)
	{
	}

	FORCEINLINE ~FOutputDeviceLine()
	{
		FOutputDeviceLinearAllocator::Free(const_cast<TCHAR*>(Data));
	}

private:
	FORCEINLINE static const TCHAR* CopyData(const TCHAR* const InData)
	{
		const int32 Len = FCString::Strlen(InData) + 1;
		void* const Dest = FOutputDeviceLinearAllocator::Malloc(sizeof(TCHAR) * Len, alignof(TCHAR));
		return static_cast<TCHAR*>(FMemory::Memcpy(Dest, InData, sizeof(TCHAR) * Len));
	}
};

struct FOutputDeviceItem
{
	TVariant<FOutputDeviceLine, FLogRecord> Value;

	FORCEINLINE FOutputDeviceItem(const TCHAR* const Data, const FName& Category, const ELogVerbosity::Type Verbosity, const double Time)
		: Value(TInPlaceType<FOutputDeviceLine>(), Data, Category, Verbosity, Time)
	{
	}

	FORCEINLINE explicit FOutputDeviceItem(const FLogRecord& Record)
		: Value(TInPlaceType<FLogRecord>(), Record)
	{
	}
};

static constexpr uint64 CalculateRedirectorCacheLinePadding(const uint64 Size)
{
	return PLATFORM_CACHE_LINE_SIZE * FMath::DivideAndRoundUp<uint64>(Size, PLATFORM_CACHE_LINE_SIZE) - Size;
}

struct FOutputDeviceRedirectorState
{
	/** A custom lock to guard access to both buffered and unbuffered output devices. */
	FRWLock OutputDevicesLock;
	std::atomic<uint32> OutputDevicesLockState = 0;
	uint8 OutputDevicesLockPadding[CalculateRedirectorCacheLinePadding(sizeof(OutputDevicesLock) + sizeof(OutputDevicesLockState))]{};

	/** A queue of items logged by non-primary threads. */
	TConsumeAllMpmcQueue<FOutputDeviceItem, FOutputDeviceLinearAllocator> BufferedItems;

	/** Array of output devices to redirect to from the primary thread. */
	TArray<FOutputDevice*> BufferedOutputDevices;

	/** Array of output devices to redirect to from the calling thread. */
	TArray<FOutputDevice*> UnbufferedOutputDevices;

	/** A queue of lines logged before the editor added its output device. */
	TArray<FBufferedLine> BacklogLines;
	FRWLock BacklogLock;

	/** An optional dedicated primary thread for logging to buffered output devices. */
	FThread Thread;

	/** A lock to synchronize access to the thread. */
	FRWLock ThreadLock;

	/** An event that is notified when the dedicated primary thread is idle. */
	FEventCount ThreadIdleEvent;

	/** An event to wake the dedicated primary thread to process buffered items. */
	FEventCount ThreadWakeEvent;

	/** The ID of the thread holding the primary lock. */
	std::atomic<uint32> LockedThreadId = MAX_uint32;

	/** The ID of the primary logging thread. Logging from other threads will be buffered for processing by the primary thread. */
	std::atomic<uint32> PrimaryThreadId = FPlatformTLS::GetCurrentThreadId();

	/** The ID of the panic thread, which is only set by Panic(). */
	std::atomic<uint32> PanicThreadId = MAX_uint32;

	/** Whether a dedicated primary thread has been started. */
	std::atomic<bool> bThreadStarted = false;

	/** Whether the backlog is enabled. */
	bool bEnableBacklog = false;

	/** Whether the output device at the corresponding index can be used on the panic thread. */
	TBitArray<TInlineAllocator<1>> BufferedOutputDevicesCanBeUsedOnPanicThread;
	TBitArray<TInlineAllocator<1>> UnbufferedOutputDevicesCanBeUsedOnPanicThread;

	bool HasPanicThread() const
	{
		return PanicThreadId.load(std::memory_order_relaxed) != MAX_uint32;
	}

	bool IsPrimaryThread(const uint32 ThreadId) const
	{
		return ThreadId == PrimaryThreadId.load(std::memory_order_relaxed);
	}

	bool IsPanicThread(const uint32 ThreadId) const
	{
		return ThreadId == PanicThreadId.load(std::memory_order_relaxed);
	}

	bool CanLockFromThread(const uint32 ThreadId) const
	{
		if (UNLIKELY(ThreadId == LockedThreadId.load(std::memory_order_relaxed)))
		{
			return false;
		}
		const uint32 LocalPanicThreadId = PanicThreadId.load(std::memory_order_relaxed);
		return LocalPanicThreadId == MAX_uint32 || LocalPanicThreadId == ThreadId;
	}

	void AddOutputDevice(FOutputDevice* OutputDevice);
	void RemoveOutputDevice(FOutputDevice* OutputDevice);

	bool TryStartThread();
	bool TryStopThread();

	void ThreadLoop();

	void FlushBufferedItems();

	template <typename OutputDevicesType, typename FunctionType, typename... ArgTypes>
	FORCEINLINE void BroadcastTo(
		const uint32 ThreadId,
		const OutputDevicesType& OutputDevices,
		const TBitArray<TInlineAllocator<1>>& CanBeUsedOnPanicThread,
		FunctionType&& Function,
		ArgTypes&&... Args)
	{
		int32 Index = 0;
		const bool bIsPanicThread = IsPanicThread(ThreadId);
		for (FOutputDevice* OutputDevice : OutputDevices)
		{
			if (!bIsPanicThread || CanBeUsedOnPanicThread[Index++])
			{
				Invoke(Function, OutputDevice, Forward<ArgTypes>(Args)...);
			}
		}
	}

	FORCENOINLINE void AddToBacklog(const FLogRecord& Record);
	FORCENOINLINE void AddToBacklog(const TCHAR* Data, ELogVerbosity::Type Verbosity, const FName& Category, double Time);
};

/**
 * A scoped lock for readers of the OutputDevices arrays.
 *
 * The read lock:
 * - Must be locked to read the OutputDevices arrays.
 * - Must be locked to write to unbuffered output devices.
 * - Must not be entered when the thread holds a write or primary lock.
 */
class FOutputDevicesReadScopeLock
{
public:
	FORCEINLINE explicit FOutputDevicesReadScopeLock(FOutputDeviceRedirectorState& InState)
		: State(InState)
	{
		// Read locks add/sub by 2 to keep the LSB free for write locks to use.
		if (State.OutputDevicesLockState.fetch_add(2, std::memory_order_acquire) & 1)
		{
			WaitForWriteLock();
		}
	}

	FORCENOINLINE void WaitForWriteLock()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FOutputDevicesReadScopeLock);
		// A write lock has set the LSB. Cancel this read lock and wait for the write.
		State.OutputDevicesLockState.fetch_sub(2, std::memory_order_relaxed);
		// This read lock will wait until the write lock exits.
		FReadScopeLock ScopeLock(State.OutputDevicesLock);
		// This is relaxed because locking OutputDevicesLock has acquire semantics.
		uint32 LockState = State.OutputDevicesLockState.fetch_add(2, std::memory_order_relaxed);
		check((LockState & 1) == 0);
	}

	FORCEINLINE ~FOutputDevicesReadScopeLock()
	{
		State.OutputDevicesLockState.fetch_sub(2, std::memory_order_release);
	}

private:
	FOutputDeviceRedirectorState& State;
};

/**
 * A scoped lock for writers of the OutputDevices arrays.
 *
 * The write lock has the same access as the primary lock, and:
 * - Must be locked to add or remove output devices.
 * - Must not be entered when the thread holds a read, write, or primary lock.
 */
class FOutputDevicesWriteScopeLock
{
public:
	FORCEINLINE explicit FOutputDevicesWriteScopeLock(FOutputDeviceRedirectorState& InState)
		: State(InState)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FOutputDevicesWriteScopeLock);
		// Take the lock before modifying the state, to avoid contention on the LSB.
		State.OutputDevicesLock.WriteLock();
		State.LockedThreadId.store(FPlatformTLS::GetCurrentThreadId(), std::memory_order_relaxed);
		// Set the LSB to flag to read locks that a write lock is waiting.
		uint32 LockState = State.OutputDevicesLockState.fetch_or(uint32(1), std::memory_order_acquire);
		check((LockState & 1) == 0);
		if (LockState > 1)
		{
			// Wait for read locks to be cleared.
			do
			{
				FPlatformProcess::Sleep(0);
				LockState = State.OutputDevicesLockState.load(std::memory_order_acquire);
			}
			while (LockState > 1);
		}
	}

	FORCEINLINE ~FOutputDevicesWriteScopeLock()
	{
		// Clear the LSB to allow read locks after the unlock below.
		uint32 LockState = State.OutputDevicesLockState.fetch_and(~uint32(1), std::memory_order_release);
		check((LockState & 1) == 1);
		State.LockedThreadId.store(MAX_uint32, std::memory_order_relaxed);
		State.OutputDevicesLock.WriteUnlock();
	}

private:
	FOutputDeviceRedirectorState& State;
};

/**
 * A scoped lock for exclusive access to the state of the primary log thread.
 *
 * The primary lock has the same access as the read lock, and:
 * - Must not be entered when the thread holds a write lock or primary lock.
 * - Must check IsLocked() before performing restricted operations.
 * - Must be locked to write to buffered output devices.
 * - Must be locked while calling FlushBufferedItems().
 * - May be locked when the thread holds a read lock.
 * - When a panic thread is active, locking will only succeed from the panic thread.
 */
class FOutputDevicesPrimaryScopeLock
{
public:
	explicit FOutputDevicesPrimaryScopeLock(FOutputDeviceRedirectorState& InState)
		: State(InState)
	{
		const uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();
		if (State.CanLockFromThread(ThreadId))
		{
			if (State.IsPanicThread(ThreadId))
			{
				bLocked = true;
			}
			else
			{
				State.OutputDevicesLock.WriteLock();
				if (!State.CanLockFromThread(ThreadId))
				{
					State.OutputDevicesLock.WriteUnlock();
				}
				else
				{
					bNeedsUnlock = true;
					bLocked = true;
				}
			}
			if (bLocked)
			{
				State.LockedThreadId.store(ThreadId, std::memory_order_relaxed);
			}
		}
	}

	FORCEINLINE ~FOutputDevicesPrimaryScopeLock()
	{
		if (bLocked)
		{
			State.LockedThreadId.store(MAX_uint32, std::memory_order_relaxed);
		}
		if (bNeedsUnlock)
		{
			State.OutputDevicesLock.WriteUnlock();
		}
	}

	FORCEINLINE bool IsLocked() const { return bLocked; }

private:
	FOutputDeviceRedirectorState& State;
	bool bNeedsUnlock = false;
	bool bLocked = false;
};

void FOutputDeviceRedirectorState::AddOutputDevice(FOutputDevice* OutputDevice)
{
	const auto AddTo = [OutputDevice](TArray<FOutputDevice*>& OutputDevices, TBitArray<TInlineAllocator<1>>& Flags)
	{
		const int32 Count = OutputDevices.Num();
		if (OutputDevices.AddUnique(OutputDevice) == Count)
		{
			Flags.Add(OutputDevice->CanBeUsedOnPanicThread());
		}
	};
	UE::Private::FOutputDevicesWriteScopeLock ScopeLock(*this);
	if (OutputDevice->CanBeUsedOnMultipleThreads())
	{
		AddTo(UnbufferedOutputDevices, UnbufferedOutputDevicesCanBeUsedOnPanicThread);
	}
	else
	{
		AddTo(BufferedOutputDevices, BufferedOutputDevicesCanBeUsedOnPanicThread);
	}
}

void FOutputDeviceRedirectorState::RemoveOutputDevice(FOutputDevice* OutputDevice)
{
	const auto RemoveFrom = [OutputDevice](TArray<FOutputDevice*>& OutputDevices, TBitArray<TInlineAllocator<1>>& Flags)
	{
		if (const int32 Index = OutputDevices.FindLast(OutputDevice); Index != INDEX_NONE)
		{
			OutputDevices.RemoveAt(Index);
			Flags.RemoveAt(Index);
		}
	};
	UE::Private::FOutputDevicesWriteScopeLock ScopeLock(*this);
	RemoveFrom(BufferedOutputDevices, BufferedOutputDevicesCanBeUsedOnPanicThread);
	RemoveFrom(UnbufferedOutputDevices, UnbufferedOutputDevicesCanBeUsedOnPanicThread);
}

bool FOutputDeviceRedirectorState::TryStartThread()
{
	if (FWriteScopeLock ThreadScopeLock(ThreadLock); !bThreadStarted.exchange(true, std::memory_order_relaxed))
	{
		Thread = FThread(TEXT("OutputDeviceRedirector"), [this] { ThreadLoop(); });
	}
	return true;
}

bool FOutputDeviceRedirectorState::TryStopThread()
{
	if (FWriteScopeLock ThreadScopeLock(ThreadLock); bThreadStarted.exchange(false, std::memory_order_relaxed))
	{
		ThreadWakeEvent.Notify();
		Thread.Join();
	}
	return true;
}

void FOutputDeviceRedirectorState::ThreadLoop()
{
	const uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();

	if (FOutputDevicesPrimaryScopeLock Lock(*this); Lock.IsLocked())
	{
		PrimaryThreadId.store(ThreadId, std::memory_order_relaxed);
	}

	for (;;)
	{
		FEventCountToken Token = ThreadWakeEvent.PrepareWait();
		if (!bThreadStarted.load(std::memory_order_relaxed))
		{
			break;
		}
		while (!BufferedItems.IsEmpty() && IsPrimaryThread(ThreadId))
		{
			if (FOutputDevicesPrimaryScopeLock Lock(*this); Lock.IsLocked())
			{
				FlushBufferedItems();
			}
		}
		ThreadIdleEvent.Notify();
		ThreadWakeEvent.Wait(Token);
	}
}

void FOutputDeviceRedirectorState::FlushBufferedItems()
{
	using namespace UE;
	using namespace UE::Private;

	if (BufferedItems.IsEmpty())
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FOutputDeviceRedirector::FlushBufferedItems);

	const uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();
	bool bIsPanicThread = this->IsPanicThread(ThreadId);
	BufferedItems.ConsumeAllFifo([this, ThreadId, bIsPanicThread](FOutputDeviceItem&& Item)
	{
		if (!bIsPanicThread && HasPanicThread())
		{
			// Enqueuing during Deplete is okay because the items passed to the Deplete consumer
			// are a moved list that is no longer associated with the queue; Deplete will return
			// even though new items are in the queue.
			BufferedItems.ProduceItem(MoveTemp(Item));
		}
		else
		{
			Visit([this, ThreadId](auto&& Value)
			{
				using ValueType = std::decay_t<decltype(Value)>;
				if constexpr (std::is_same_v<ValueType, FOutputDeviceLine>)
				{
					const FOutputDeviceLine& Line = Value;
					BroadcastTo(ThreadId, BufferedOutputDevices, BufferedOutputDevicesCanBeUsedOnPanicThread,
						UE_PROJECTION_MEMBER(FOutputDevice, Serialize),
						Line.Data, Line.Verbosity, Line.Category, Line.Time);
				}
				else if constexpr (std::is_same_v<ValueType, FLogRecord>)
				{
					const FLogRecord& Record = Value;
					BroadcastTo(ThreadId, BufferedOutputDevices, BufferedOutputDevicesCanBeUsedOnPanicThread,
						UE_PROJECTION_MEMBER(FOutputDevice, SerializeRecord), Record);
				}
			}, Item.Value);
		}
	});
}

void FOutputDeviceRedirectorState::AddToBacklog(const FLogRecord& Record)
{
	TStringBuilder<512> Text;
	Record.FormatMessageTo(Text);
	AddToBacklog(*Text, Record.GetVerbosity(), Record.GetCategory(), FPlatformTime::Seconds() - GStartTime);
}

void FOutputDeviceRedirectorState::AddToBacklog(
	const TCHAR* const Data,
	const ELogVerbosity::Type Verbosity,
	const FName& Category,
	const double Time)
{
	FWriteScopeLock ScopeLock(BacklogLock);
	BacklogLines.Emplace(Data, Category, Verbosity, Time);
}

} // UE::Private

FOutputDeviceRedirector::FOutputDeviceRedirector()
	: State(MakePimpl<UE::Private::FOutputDeviceRedirectorState>())
{
}

FOutputDeviceRedirector* FOutputDeviceRedirector::Get()
{
	static FOutputDeviceRedirector Singleton;
	return &Singleton;
}

void FOutputDeviceRedirector::AddOutputDevice(FOutputDevice* OutputDevice)
{
	if (OutputDevice)
	{
		State->AddOutputDevice(OutputDevice);
	}
}

void FOutputDeviceRedirector::RemoveOutputDevice(FOutputDevice* OutputDevice)
{
	if (OutputDevice)
	{
		State->RemoveOutputDevice(OutputDevice);
	}
}

bool FOutputDeviceRedirector::IsRedirectingTo(FOutputDevice* OutputDevice)
{
	UE::Private::FOutputDevicesReadScopeLock Lock(*State);
	return State->BufferedOutputDevices.Contains(OutputDevice) || State->UnbufferedOutputDevices.Contains(OutputDevice);
}

void FOutputDeviceRedirector::FlushThreadedLogs(EOutputDeviceRedirectorFlushOptions Options)
{
	if (FReadScopeLock ThreadLock(State->ThreadLock); State->bThreadStarted.load(std::memory_order_relaxed))
	{
		if (!EnumHasAnyFlags(Options, EOutputDeviceRedirectorFlushOptions::Async))
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FOutputDeviceRedirector::FlushThreadedLogs);
			UE::FEventCountToken Token = State->ThreadIdleEvent.PrepareWait();
			State->ThreadWakeEvent.Notify();
			State->ThreadIdleEvent.Wait(Token);
		}
		return;
	}

	if (UE::Private::FOutputDevicesPrimaryScopeLock Lock(*State); Lock.IsLocked())
	{
		State->FlushBufferedItems();
	}
}

void FOutputDeviceRedirector::SerializeBacklog(FOutputDevice* OutputDevice)
{
	FReadScopeLock ScopeLock(State->BacklogLock);
	for (const FBufferedLine& BacklogLine : State->BacklogLines)
	{
		OutputDevice->Serialize(BacklogLine.Data.Get(), BacklogLine.Verbosity, BacklogLine.Category, BacklogLine.Time);
	}
}

void FOutputDeviceRedirector::EnableBacklog(bool bEnable)
{
	FWriteScopeLock ScopeLock(State->BacklogLock);
	State->bEnableBacklog = bEnable;
	if (!bEnable)
	{
		State->BacklogLines.Empty();
	}
}

void FOutputDeviceRedirector::SetCurrentThreadAsPrimaryThread()
{
	const uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();

	if (UE::Private::FOutputDevicesPrimaryScopeLock Lock(*State); !Lock.IsLocked() || State->PrimaryThreadId.load(std::memory_order_relaxed) == ThreadId)
	{
		return;
	}
	else
	{
		State->PrimaryThreadId.store(ThreadId, std::memory_order_relaxed);
		State->FlushBufferedItems();
	}

	State->TryStopThread();
}

bool FOutputDeviceRedirector::TryStartDedicatedPrimaryThread()
{
	return FApp::ShouldUseThreadingForPerformance() && State->TryStartThread();
}

void FOutputDeviceRedirector::SerializeRecord(const UE::FLogRecord& Record)
{
	using namespace UE::Private;

	FOutputDevicesReadScopeLock Lock(*State);

	const uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();

	// Serialize directly to any output devices which don't require buffering
	State->BroadcastTo(ThreadId, State->UnbufferedOutputDevices, State->UnbufferedOutputDevicesCanBeUsedOnPanicThread,
		UE_PROJECTION_MEMBER(FOutputDevice, SerializeRecord), Record);

	// Serialize to the backlog when not in panic mode. This will deadlock in panic mode when the
	// FPlatformMallocCrash allocator has been enabled and logging occurs on a non-panic thread.
	if (UNLIKELY(State->bEnableBacklog && !State->HasPanicThread()))
	{
		State->AddToBacklog(Record);
	}

	// Serialize to buffered output devices from the primary logging thread.
	// Records are queued until buffered output devices are added to avoid missing early log records.
	if (State->IsPrimaryThread(ThreadId) && !State->BufferedOutputDevices.IsEmpty())
	{
		// Verify that this is the primary thread again because another thread may have become
		// the primary thread between the previous check and the lock.
		if (FOutputDevicesPrimaryScopeLock PrimaryLock(*State); PrimaryLock.IsLocked() && State->IsPrimaryThread(ThreadId))
		{
			State->FlushBufferedItems();
			State->BroadcastTo(ThreadId, State->BufferedOutputDevices, State->BufferedOutputDevicesCanBeUsedOnPanicThread,
				UE_PROJECTION_MEMBER(FOutputDevice, SerializeRecord), Record);
			if (UNLIKELY(State->IsPanicThread(ThreadId)))
			{
				Flush();
			}
			return;
		}
	}

	// Queue the record to serialize to buffered output devices from the primary thread.
	if (State->BufferedItems.ProduceItem(Record) == UE::EConsumeAllMpmcQueueResult::WasEmpty)
	{
		State->ThreadWakeEvent.Notify();
	}
}

void FOutputDeviceRedirector::Serialize(const TCHAR* const Data, const ELogVerbosity::Type Verbosity, const FName& Category, const double Time)
{
	using namespace UE::Private;

	const double RealTime = Time == -1.0 ? FPlatformTime::Seconds() - GStartTime : Time;

	FOutputDevicesReadScopeLock Lock(*State);

#if PLATFORM_DESKTOP
	// Print anything that arrives after logging has shut down to at least have it in stdout.
	if (UNLIKELY(State->BufferedOutputDevices.IsEmpty() && IsEngineExitRequested()))
	{
	#if PLATFORM_WINDOWS
		_tprintf(_T("%s\n"), Data);
	#endif
		FGenericPlatformMisc::LocalPrint(Data);
		return;
	}
#endif

	const uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();

	// Serialize directly to any output devices which don't require buffering
	State->BroadcastTo(ThreadId, State->UnbufferedOutputDevices, State->UnbufferedOutputDevicesCanBeUsedOnPanicThread,
		UE_PROJECTION_MEMBER(FOutputDevice, Serialize),
		Data, Verbosity, Category, RealTime);

	// Serialize to the backlog when not in panic mode. This will deadlock in panic mode when the
	// FPlatformMallocCrash allocator has been enabled and logging occurs on a non-panic thread.
	if (UNLIKELY(State->bEnableBacklog && !State->HasPanicThread()))
	{
		State->AddToBacklog(Data, Verbosity, Category, RealTime);
	}

	// Serialize to buffered output devices from the primary logging thread.
	// Lines are queued until buffered output devices are added to avoid missing early log lines.
	if (State->IsPrimaryThread(ThreadId) && !State->BufferedOutputDevices.IsEmpty())
	{
		// Verify that this is the primary thread again because another thread may have become
		// the primary thread between the previous check and the lock.
		if (FOutputDevicesPrimaryScopeLock PrimaryLock(*State); PrimaryLock.IsLocked() && State->IsPrimaryThread(ThreadId))
		{
			State->FlushBufferedItems();
			State->BroadcastTo(ThreadId, State->BufferedOutputDevices, State->BufferedOutputDevicesCanBeUsedOnPanicThread,
				UE_PROJECTION_MEMBER(FOutputDevice, Serialize),
				Data, Verbosity, Category, RealTime);
			if (UNLIKELY(State->IsPanicThread(ThreadId)))
			{
				Flush();
			}
			return;
		}
	}

	// Queue the line to serialize to buffered output devices from the primary thread.
	if (State->BufferedItems.ProduceItem(Data, Category, Verbosity, RealTime) == UE::EConsumeAllMpmcQueueResult::WasEmpty)
	{
		State->ThreadWakeEvent.Notify();
	}
}

void FOutputDeviceRedirector::Serialize(const TCHAR* Data, ELogVerbosity::Type Verbosity, const FName& Category)
{
	Serialize(Data, Verbosity, Category, -1.0);
}

void FOutputDeviceRedirector::RedirectLog(const FName& Category, ELogVerbosity::Type Verbosity, const TCHAR* Data)
{
	Serialize(Data, Verbosity, Category, -1.0);
}

void FOutputDeviceRedirector::RedirectLog(const FLazyName& Category, ELogVerbosity::Type Verbosity, const TCHAR* Data)
{
	Serialize(Data, Verbosity, Category, -1.0);
}

void FOutputDeviceRedirector::Flush()
{
	if (UE::Private::FOutputDevicesPrimaryScopeLock Lock(*State); Lock.IsLocked())
	{
		State->FlushBufferedItems();
		const uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();
		State->BroadcastTo(ThreadId, State->BufferedOutputDevices, State->BufferedOutputDevicesCanBeUsedOnPanicThread, &FOutputDevice::Flush);
		State->BroadcastTo(ThreadId, State->UnbufferedOutputDevices, State->UnbufferedOutputDevicesCanBeUsedOnPanicThread, &FOutputDevice::Flush);
	}
}

void FOutputDeviceRedirector::Panic()
{
	uint32 PreviousThreadId = MAX_uint32;
	const uint32 ThreadId = FPlatformTLS::GetCurrentThreadId();
	if (State->PanicThreadId.compare_exchange_strong(PreviousThreadId, ThreadId, std::memory_order_relaxed))
	{
		// Another thread may be holding the lock. Wait a while for it, but avoid waiting forever
		// because the thread holding the lock may be unable to progress. After the timeout is
		// reached, assume that it is safe enough to continue on the panic thread. There is a
		// chance that the thread holding the lock has left an output device in an unusable state
		// or will resume and crash due to a race with the panic thread. Executing on this thread
		// and having logging for most panic situations with a chance of a crash is preferable to
		// the alternative of missing logging in a panic situation.
		constexpr double WaitTime = 1.0;
		for (const double EndTime = FPlatformTime::Seconds() + WaitTime; FPlatformTime::Seconds() < EndTime;)
		{
			if (State->OutputDevicesLock.TryWriteLock())
			{
				State->OutputDevicesLock.WriteUnlock();
				break;
			}
			FPlatformProcess::Yield();
		}

		// Make the panic thread the primary thread. Neither thread can be changed after this point.
		State->PrimaryThreadId.exchange(ThreadId, std::memory_order_relaxed);

		// Flush. Every log from the panic thread after this point will also flush.
		Flush();
	}
	else if (PreviousThreadId == ThreadId)
	{
		// Calling Panic() multiple times from the panic thread is equivalent to calling Flush().
		Flush();
	}
}

void FOutputDeviceRedirector::TearDown()
{
	SetCurrentThreadAsPrimaryThread();

	Flush();

	State->TryStopThread();

	TArray<FOutputDevice*> LocalBufferedDevices;
	TArray<FOutputDevice*> LocalUnbufferedDevices;

	{
		UE::Private::FOutputDevicesWriteScopeLock Lock(*State);
		LocalBufferedDevices = MoveTemp(State->BufferedOutputDevices);
		LocalUnbufferedDevices = MoveTemp(State->UnbufferedOutputDevices);
		State->BufferedOutputDevices.Empty();
		State->UnbufferedOutputDevices.Empty();
	}

	for (FOutputDevice* OutputDevice : LocalBufferedDevices)
	{
		OutputDevice->TearDown();
	}

	for (FOutputDevice* OutputDevice : LocalUnbufferedDevices)
	{
		OutputDevice->TearDown();
	}
}

bool FOutputDeviceRedirector::IsBacklogEnabled() const
{
	FReadScopeLock Lock(State->BacklogLock);
	return State->bEnableBacklog;
}

CORE_API FOutputDeviceRedirector* GetGlobalLogSingleton()
{
	return FOutputDeviceRedirector::Get();
}
