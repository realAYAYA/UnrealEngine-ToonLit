// Copyright Epic Games, Inc. All Rights Reserved.

// libc.so/scudo memory tracing.
// Implemented overriding malloc, calloc, realloc, memalign, free, posix_memalign, aligned_alloc, reallocarray via LD_PRELOAD.
// When modifying this code beware that any allocation from with-in malloc handler will cause recursion,
// for example thread local will call malloc on the first access, snprintf calls malloc, Android log calls malloc, etc.

#include "ScudoMemoryTrace.h"

// Trace events before libUnreal.so is loaded and sets the trace hook.
// This requires creating a temporary file as a backing storage to store the events.
constexpr bool bTraceEventsBeforeLibUnrealIsLoaded = true;

// If bTraceEventsBeforeLibUnrealIsLoaded is true, we record everything as soon as the application starts,
// but if the hook is never set we need to stop recording to not run over the storage.
// Also there are other processes besides main one during the app launch, and tracer will be preloaded into them as well.
constexpr uint32_t MaxEventsBeforeHookIsSet = 1'000'000;

// If bTraceEventsBeforeLibUnrealIsLoaded is true, enable dumping events into a debug file, will use a tmp file instead if not defined (tmp file is not linked to fs).
constexpr const char* DebugFilePath = nullptr; // "/data/user/0/%PACKAGE_NAME%/files/llmemorytrace_";

// If bTraceEventsBeforeLibUnrealIsLoaded is true, size of in-memory lockfree double buffer used to temporary store events.
static const size_t EventBufferSize = 16 * 1024;

// Experimental generation of time events for pre-libUnreal.so memory tracing.
constexpr bool bGenerateTimeEvents = false;

// If bGenerateTimeEvents is true, how often to set timestamps before the marker is set.
static const uint32_t BeforeHookMarkerSamplePeriod = (4 << 10) - 1;

// On some OS scudo has special hooks available, but they were disabled in Android 15.
#define USE_SCUDO_HOOKS 0

#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <sys/time.h>

#pragma pack(push, 1)

struct FEvent
{
	uint64_t Ptr;
	uint64_t Size;
	uint32_t Alignment;
	EScudoEventType Type;
	uint8_t FrameCount;

	static const uint32_t MaxCallstackFrameCount = 20;
	uint64_t Frames[MaxCallstackFrameCount];

	constexpr static inline uint32_t GetEmptyEventSize()
	{
		return sizeof(FEvent) - MaxCallstackFrameCount * sizeof(uint64_t);
	}

	inline uint32_t GetEventSize() const
	{
		return GetEmptyEventSize() + FrameCount * sizeof(uint64_t);
	}
};

#pragma pack(pop)

struct Spinlock
{
	std::atomic_flag Flag = {false};

	inline void lock()
	{
		while (Flag.test_and_set(std::memory_order_acquire))
		{
			while (Flag.test(std::memory_order_relaxed))
			{
				std::this_thread::yield();
			}
		}
	}

	inline void unlock()
	{
		Flag.clear(std::memory_order_release);
	}
};

struct TLSFlag
{
	pthread_key_t Key;

	inline void Setup()
	{
		pthread_key_create(&Key, nullptr);
	}

	inline void Set(bool bValue)
	{
		pthread_setspecific(Key, bValue ? (const void*)1 : nullptr);
	}

	inline bool Get()
	{
		return pthread_getspecific(Key) != nullptr;
	}
};

struct EventBuffer
{
	uint8_t Data[EventBufferSize];
	std::atomic<uint32_t> Head = {0};
	std::atomic<uint32_t> Tail = {0};

	inline bool Write(const FEvent* Event)
	{
		const void* Buffer = (const void*)Event;
		const uint32_t EventSize = Event->GetEventSize();

		// Acquire space in the buffer
		uint32_t WritePos = Head.load();
		do
		{
			if (WritePos + EventSize > EventBufferSize)
			{
				return false;
			}
		}
		while(Head.compare_exchange_strong(WritePos, WritePos + EventSize) == false);

		memcpy(Data + WritePos, Buffer, EventSize);

		Tail.fetch_add(EventSize);
		return true;
	}

	inline bool Read(FEvent* Event, uint32_t* Offset, const uint32_t DataSize)
	{
		const uint32_t EmptySize = FEvent::GetEmptyEventSize();
		if (*Offset + EmptySize > DataSize)
		{
			return false;
		}

		memcpy(Event, Data + *Offset, EmptySize);

		const uint32_t EventSize = Event->GetEventSize();
		if (*Offset + EventSize > DataSize) // don't bother with partial events
		{
			return false;
		}

		const uint32_t ExtraBytes = EventSize - EmptySize;
		if (ExtraBytes > 0)
		{
			memcpy(((uint8_t*)Event) + EmptySize, Data + *Offset + EmptySize, ExtraBytes);
		}

		return true;
	}

	inline void Reset()
	{
		Head.store(0);
		Tail.store(0);
	}
};

static struct
{
	std::atomic_flag bSetup = {false};

	std::atomic_flag bDisableEventsGlobal = {false};
	// can't use thread_local because it will malloc during creation causing recursion
	TLSFlag DisableEventsPerThread = {};

	std::atomic<ScudoMemoryTraceHook> TraceHook = {nullptr};

	std::atomic<uint32_t> EventCount = {0};

	Spinlock WriteLock = {};
	Spinlock SetupLock = {};

	std::atomic<int> Fd = {0};
	std::atomic<off64_t> FdOffset = {0};

	EventBuffer Front, Back = {};
	std::atomic<EventBuffer*> FrontBuffer = {nullptr};

	std::atomic<uint64_t> StackTopMainThread = {0};

#if !USE_SCUDO_HOOKS
	void* (*DefaultMalloc)(size_t);
	void* (*DefaultCalloc)(size_t, size_t);
	void* (*DefaultRealloc)(void*, size_t);
	void* (*DefaultMemAlign)(size_t, size_t);
	void  (*DefaultFree)(void*);
	int   (*DefaultPosixMemAlign)(void**, size_t, size_t);
	void* (*DefaultAlignedAlloc)(size_t, size_t);
	void* (*DefaultReallocArray)(void*, size_t, size_t);
#endif
} Ctx = {};

struct GlobalReentranceGuard
{
	GlobalReentranceGuard()
	{
		Ctx.bDisableEventsGlobal.test_and_set();
	}

	~GlobalReentranceGuard()
	{
		Ctx.bDisableEventsGlobal.clear();
	}

private:
	GlobalReentranceGuard(GlobalReentranceGuard const&) = delete;
	GlobalReentranceGuard& operator=(GlobalReentranceGuard const&) = delete;
};

struct PerThreadTraceReentranceGuard
{
	PerThreadTraceReentranceGuard()
	{
		Ctx.DisableEventsPerThread.Set(true);
	}

	~PerThreadTraceReentranceGuard()
	{
		Ctx.DisableEventsPerThread.Set(false);
	}

private:
	PerThreadTraceReentranceGuard(PerThreadTraceReentranceGuard const&) = delete;
	PerThreadTraceReentranceGuard& operator=(PerThreadTraceReentranceGuard const&) = delete;
};

static inline void* LookupSymbol(const char* Name)
{
	void* Symbol = dlsym(RTLD_NEXT, Name);
	if (!Symbol)
	{
		abort();
	}
	return Symbol;
}

static inline void PathWithPid(char* Buffer, size_t BufferSize, const char* Prefix)
{
	const size_t PrefixLen = strlen(Prefix); // bionic strlen() doesn't allocate
	if (BufferSize < PrefixLen + 8)
	{
		return;
	}

	memcpy(Buffer, Prefix, PrefixLen);

	int Pid = getpid(); // bionic getpid() doesn't allocate

	// can't use snprintf due to internal allocations
	Buffer[PrefixLen + 7] = '\0';
	for (int i = 6; i >= 0; i--, Pid /= 10)
	{
		Buffer[PrefixLen + i] = '0' + (Pid % 10);
	}
}

static inline void Setup()
{
	if (Ctx.bSetup.test(std::memory_order::relaxed))
	{
		return;
	}

	std::lock_guard Lock(Ctx.SetupLock);

	if (Ctx.bSetup.test() == true)
	{
		return;
	}

	{
		// Disable any tracing for duration of TLS creation
		GlobalReentranceGuard Guard;
		Ctx.DisableEventsPerThread.Setup();
	}

	if constexpr (bTraceEventsBeforeLibUnrealIsLoaded)
	{
		if constexpr (DebugFilePath == nullptr)
		{
			char* InternalFolderPath = getenv("UE_INTERNAL_FOLDER");
			if (InternalFolderPath == nullptr)
			{
				abort();
			}
			// O_TMPFILE will be removed after it's closed
			Ctx.Fd = open(InternalFolderPath, O_TMPFILE | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
		}
		else
		{
			char Path[128];
			PathWithPid(Path, sizeof(Path), DebugFilePath);
			Ctx.Fd = open(Path, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
		}

		// Disable events if backing storage is not created.
		if (Ctx.Fd == 0)
		{
			Ctx.bDisableEventsGlobal.test_and_set();
		}
	}

#if !USE_SCUDO_HOOKS
	Ctx.DefaultMalloc        = (decltype(Ctx.DefaultMalloc))        LookupSymbol("malloc");
	Ctx.DefaultCalloc        = (decltype(Ctx.DefaultCalloc))        LookupSymbol("calloc");
	Ctx.DefaultRealloc       = (decltype(Ctx.DefaultRealloc))       LookupSymbol("realloc");
	Ctx.DefaultMemAlign      = (decltype(Ctx.DefaultMemAlign))      LookupSymbol("memalign");
	Ctx.DefaultFree          = (decltype(Ctx.DefaultFree))          LookupSymbol("free");
	Ctx.DefaultPosixMemAlign = (decltype(Ctx.DefaultPosixMemAlign)) LookupSymbol("posix_memalign");
	Ctx.DefaultAlignedAlloc  = (decltype(Ctx.DefaultAlignedAlloc))  LookupSymbol("aligned_alloc");
	Ctx.DefaultReallocArray  = (decltype(Ctx.DefaultReallocArray))  LookupSymbol("reallocarray");
#endif

	Ctx.FrontBuffer = &Ctx.Front;

	Ctx.bSetup.test_and_set();
}

static inline void CloseTempFileAndDisableTracing()
{
	Ctx.bDisableEventsGlobal.test_and_set();
	const int Fd = Ctx.Fd.exchange(0);
	if (Fd != 0)
	{
		close(Fd);
	}
}

static inline void ReportEventToHook(const ScudoMemoryTraceHook TraceHook, const FEvent* Event)
{
	TraceHook(Event->Type, Event->Ptr, Event->Size, Event->Alignment, Event->Frames, Event->FrameCount);
}

static inline void ReportEvent(const FEvent* Event)
{
	ScudoMemoryTraceHook TraceHook = Ctx.TraceHook.load(std::memory_order_relaxed);

	if (TraceHook != nullptr)
	{
		PerThreadTraceReentranceGuard Guard;
		ReportEventToHook(TraceHook, Event);
	}
	else if constexpr(bTraceEventsBeforeLibUnrealIsLoaded)
	{
		const uint32_t EventCount = Ctx.EventCount.fetch_add(1);

		// Disable tracer if UE never set the hook
		if (Ctx.Fd.load(std::memory_order_relaxed) != 0 &&
			EventCount > MaxEventsBeforeHookIsSet)
		{
			CloseTempFileAndDisableTracing();
			return;
		}

		while (Ctx.FrontBuffer.load()->Write(Event) == false)
		{
			std::lock_guard Lock(Ctx.WriteLock);

			// Report again if trace hook was installed right when we were waiting for the lock.
			if (Ctx.TraceHook.load() != nullptr)
			{
				ReportEvent(Event);
				return;
			}

			EventBuffer* FrontBuffer = Ctx.FrontBuffer.load();

			// Try to write to front buffer again, maybe it was swapped while we were waiting for the lock
			if (FrontBuffer->Write(Event))
			{
				return;
			}

			// If writing fails again, check if we got lock out of events while waiting for the lock, maybe Fd was closed.
			if (Ctx.bDisableEventsGlobal.test())
			{
				return;
			}

			// Current buffer is full, swap buffers to unlock new writes on other threads
			EventBuffer* BackBuffer = FrontBuffer;
			FrontBuffer = BackBuffer == &Ctx.Front ? &Ctx.Back : &Ctx.Front;
			Ctx.FrontBuffer = FrontBuffer;

			// Try to write to new buffer, so our event will be closer to the front of the buffer
			bool bReturnAfterFlush = false;
			if (FrontBuffer->Write(Event))
			{
				bReturnAfterFlush = true;
			}
			else
			{
				// New front buffer is already full? Will try again in the next loop.
			}

			const uint32_t BackBufferSize = BackBuffer->Head.load();
			const int Fd = Ctx.Fd.load();
			if (Fd == 0)
			{
				Ctx.bDisableEventsGlobal.test_and_set();
				return;
			}
			const off64_t Cursor = Ctx.FdOffset.fetch_add(BackBufferSize);
			ftruncate64(Fd, Cursor + BackBufferSize);

			uint32_t BytesWritten = 0;
			while (BytesWritten < BackBufferSize)
			{
				const ssize_t Result = pwrite64(Fd, BackBuffer->Data + BytesWritten, BackBufferSize - BytesWritten, Cursor + BytesWritten);
				if (Result > 0)
				{
					BytesWritten += Result;
				}
				else if (Result == -1) // Failed to write to backing storage, disable tracing
				{
					CloseTempFileAndDisableTracing();
					return;
				}
			}

			BackBuffer->Reset();

			if (bReturnAfterFlush)
			{
				return;
			}
		}

		// Periodically report time events, only needed before the hook is set, configuration is the same to MemoryTrace.cpp
		if constexpr(bGenerateTimeEvents)
		{
			if ((EventCount & BeforeHookMarkerSamplePeriod) == 0)
			{
				timespec ts;
				clock_gettime(CLOCK_MONOTONIC, &ts);
				const uint64_t Cycles = ((((uint64_t)ts.tv_sec) * 1000000ULL) + (((uint64_t)ts.tv_nsec) / 1000ULL));

				FEvent TimeEvent = {
					.Ptr = Cycles,
					.Type = EScudoEventType::Time
				};

				// Beware, recursion
				ReportEvent(&TimeEvent);
			}
		}
	}
}

static inline bool ReadEventFromStorage(const int Fd, FEvent* Event, off64_t* ReadPos)
{
	uint8_t* Buffer = (uint8_t*)Event;
	uint32_t BytesRead = 0;
	for (uint32_t i = 0; i < 2; ++i)
	{
		uint32_t BytesToRead = i == 0 ? FEvent::GetEmptyEventSize() : Event->GetEventSize();

		while (BytesRead < BytesToRead)
		{
			const ssize_t Result = pread64(Fd, Buffer + BytesRead, BytesToRead - BytesRead, *ReadPos);
			if (Result < 0)
			{
				return false;
			}
			BytesRead += Result;
			*ReadPos += Result;
		}
	}

	return true;
}

// ---------------------------------------------------------------------------------------------------------------------

static inline uint64_t RemovePtrTags(const void* RawPtr)
{
	uint64_t Raw = (uint64_t)RawPtr;
	// Remove bionic pointer tag. For more info see https://cs.android.com/android/platform/superproject/main/+/main:bionic/libc/bionic/malloc_tagged_pointers.h
	Raw &= 0x00FF'FFFF'FFFF'FFFFull;
#if defined(__aarch64__)
	// Remove PAC
	asm("xpaclri" : "+r" (Raw));
	return Raw;
#else
	return Raw;
#endif
}

static inline void CollectCallstack(const void* FrameStartPtr, const void* ReturnAddrPtr, FEvent* OutEvent)
{
	struct StackFrame
	{
		StackFrame* NextFrame;
		void* ReturnPtr;
	};

	// There is no easy way to get current thread stack top without invoking malloc somewhere in the chain of calls, hence assume some sensible thread size.
	StackFrame* FrameEnd = (StackFrame*)((uint8_t*)FrameStartPtr + 16 * PAGE_SIZE); // 64kb
	StackFrame* FrameStart = (StackFrame*)FrameStartPtr;

	for (StackFrame* CurrentFrame = FrameStart;
		OutEvent->FrameCount < FEvent::MaxCallstackFrameCount &&
		CurrentFrame->NextFrame > FrameStart &&
		CurrentFrame->NextFrame <= FrameEnd &&
		((uintptr_t)CurrentFrame->NextFrame & (sizeof(StackFrame) - 1)) == 0; // stop at unaligned frame
		CurrentFrame = CurrentFrame->NextFrame)
	{
		const uint64_t Ptr = RemovePtrTags(CurrentFrame->ReturnPtr);
		if (Ptr != 0)
		{
			OutEvent->Frames[OutEvent->FrameCount++] = Ptr;
		}
		else
		{
			break;
		}
	}

	// If any unwinding fails, record return pointer to get at least some info.
	if (OutEvent->FrameCount == 0)
	{
		OutEvent->Frames[OutEvent->FrameCount++] = RemovePtrTags(ReturnAddrPtr);
	}
}
// ---------------------------------------------------------------------------------------------------------------------

static inline bool ShouldIgnoreEvent()
{
	return Ctx.bDisableEventsGlobal.test() || Ctx.DisableEventsPerThread.Get();
}

// ---------------------------------------------------------------------------------------------------------------------

static inline void ReportAllocEventImpl(const void* FrameStartPtr, const void* ReturnAddrPtr, const void* RawPtr, const size_t Size, const size_t Alignment, const uint8_t Hint)
{
	if (ShouldIgnoreEvent())
	{
		return;
	}

	const uint64_t Ptr = RemovePtrTags(RawPtr);
	if (Ptr == 0)
	{
		return;
	}

	FEvent Event = {
		.Ptr = Ptr,
		.Size = Size,
		.Alignment = (uint32_t)Alignment,
		.Type = EScudoEventType::Alloc,
	};

	CollectCallstack(FrameStartPtr, ReturnAddrPtr, &Event);
	ReportEvent(&Event);
}

#define ReportAllocEvent(Ptr, Size, Alignment, Hint) ReportAllocEventImpl(__builtin_frame_address(0), __builtin_return_address(0), Ptr, Size, Alignment, Hint)

static inline void ReportFreeEventImpl(const void* FrameStartPtr, const void* ReturnAddrPtr, const void* RawPtr, const uint8_t Hint)
{
	if (ShouldIgnoreEvent())
	{
		return;
	}

	const uint64_t Ptr = RemovePtrTags(RawPtr);
	if (Ptr == 0)
	{
		return;
	}

	FEvent Event = {
		.Ptr = Ptr,
		.Type = EScudoEventType::Free,
	};

	CollectCallstack(FrameStartPtr, ReturnAddrPtr, &Event);
	ReportEvent(&Event);
}

#define ReportFreeEvent(Ptr, Hint) ReportFreeEventImpl(__builtin_frame_address(0), __builtin_return_address(0), Ptr, Hint)

// ---------------------------------------------------------------------------------------------------------------------

void ScudoMemoryTrace_SetHook(ScudoMemoryTraceHook TraceHook)
{
	Ctx.WriteLock.lock();

	Ctx.TraceHook.store(TraceHook);

	// Start reporting again even if we disabled reporting events previously.
	Ctx.bDisableEventsGlobal.clear();

	const int Fd = Ctx.Fd.exchange(0);
	const off64_t FileSize = Ctx.FdOffset.exchange(0);

	Ctx.WriteLock.unlock();

	if (Ctx.Fd == 0)
	{
		return;
	}

	if constexpr(bTraceEventsBeforeLibUnrealIsLoaded)
	{
		// At this point all new events will be flushed to our hook. Proceed with draining backing storage and event buffers.
		lseek(Ctx.Fd, 0, SEEK_SET);

		PerThreadTraceReentranceGuard Guard;

		FEvent Event;

		off64_t ReadPos = 0;
		while(ReadPos < FileSize && ReadEventFromStorage(Fd, &Event, &ReadPos))
		{
			ReportEventToHook(TraceHook, &Event);
		}

		close(Fd);

		uint32_t EventBufferOffset = 0;
		uint32_t EventBufferDataSize = Ctx.Back.Head.load();
		while(Ctx.Back.Read(&Event, &EventBufferOffset, EventBufferDataSize))
		{
			ReportEventToHook(TraceHook, &Event);
		}

		Ctx.Back.Reset();

		EventBufferOffset = 0;
		EventBufferDataSize = Ctx.Front.Head.load();
		while(Ctx.Front.Read(&Event, &EventBufferOffset, EventBufferDataSize))
		{
			ReportEventToHook(TraceHook, &Event);
		}

		Ctx.Front.Reset();
	}
}

// ---------------------------------------------------------------------------------------------------------------------

#if USE_SCUDO_HOOKS

extern "C" void __scudo_allocate_hook(void* Ptr, size_t Size)
{
	Setup();

	ReportAllocEvent(Ptr, Size, 0);
}

extern "C" void __scudo_deallocate_hook(void* Ptr)
{
	Setup();

	ReportFreeEvent(Ptr, 1);
}

#else

extern "C" void* malloc(size_t Size)
{
	Setup();

	void* Ptr = Ctx.DefaultMalloc(Size);
	ReportAllocEvent(Ptr, Size, 0, 0);
	return Ptr;
}

extern "C" void* calloc(size_t Num, size_t Size)
{
	Setup();

	void* Ptr = Ctx.DefaultCalloc(Num, Size);
	ReportAllocEvent(Ptr, Size, 0, 1);
	return Ptr;
}

extern "C" void* realloc(void* OldPtr, size_t NewSize)
{
	Setup();

	ReportFreeEvent(OldPtr, 2);
	void* NewPtr = Ctx.DefaultRealloc(OldPtr, NewSize);
	ReportAllocEvent(NewPtr, NewSize, 0, 2);
	return NewPtr;
}

extern "C" void* memalign(size_t Alignment, size_t Size)
{
	Setup();

	void* Ptr = Ctx.DefaultMemAlign(Alignment, Size);
	ReportAllocEvent(Ptr, Size, Alignment, 3);
	return Ptr;
}

extern "C" void free(void* Ptr)
{
	Setup();

	ReportFreeEvent(Ptr, 4);
	Ctx.DefaultFree(Ptr);
}

extern "C" int posix_memalign(void** Ptr, size_t Alignment, size_t Size)
{
	Setup();

	int Result = Ctx.DefaultPosixMemAlign(Ptr, Alignment, Size);
	ReportAllocEvent((Ptr ? *Ptr : nullptr), Size, Alignment, 5);
	return Result;
}

extern "C" void* aligned_alloc(size_t Alignment, size_t Size)
{
	Setup();

	void* Ptr = Ctx.DefaultAlignedAlloc(Alignment, Size);
	ReportAllocEvent(Ptr, Size, Alignment, 6);
	return Ptr;
}

extern "C" void* reallocarray(void* OldPtr, size_t ItemCount, size_t ItemSize)
{
	Setup();

	ReportFreeEvent(OldPtr, 7);
	void* NewPtr = Ctx.DefaultReallocArray(OldPtr, ItemCount, ItemSize);
	ReportAllocEvent(NewPtr, ItemCount * ItemSize, 0, 7);
	return NewPtr;
}

#endif
