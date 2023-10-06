// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "CoreGlobals.h"
#include "Containers/Map.h"
#include "HAL/CriticalSection.h"

#ifndef PLATFORM_SUPPORTS_ALL_THREAD_BACKTRACES
	#define PLATFORM_SUPPORTS_ALL_THREAD_BACKTRACES (PLATFORM_WINDOWS || PLATFORM_MAC)
#endif

class FRunnableThread;

/**
 * Manages runnables and runnable threads.
 */
class FThreadManager
{
	/** Critical section for ThreadList */
	FCriticalSection ThreadsCritical;

	using FThreads = TMap<uint32, FRunnableThread*, TInlineSetAllocator<256>>;
	/** List of thread objects to be ticked. */
	FThreads Threads;

public:

	/**
	* Used internally to add a new thread object.
	*
	* @param Thread thread object.
	* @see RemoveThread
	*/
	CORE_API void AddThread(uint32 ThreadId, FRunnableThread* Thread);

	/**
	* Used internally to remove thread object.
	*
	* @param Thread thread object to be removed.
	* @see AddThread
	*/
	CORE_API void RemoveThread(FRunnableThread* Thread);

	/** Get the number of registered threads */
	int32 NumThreads() const { return Threads.Num(); }

	/** Ticks all fake threads and their runnable objects. */
	CORE_API void Tick();

	/** Returns the name of a thread given its TLS id */
	inline static const FString& GetThreadName(uint32 ThreadId)
	{
		static FString GameThreadName(TEXT("GameThread"));
		static FString RenderThreadName(TEXT("RenderThread"));
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		if (ThreadId == GGameThreadId)
		{
			return GameThreadName;
		}
		else if (ThreadId == GRenderThreadId)
		{
			return RenderThreadName;
		}
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		return Get().GetThreadNameInternal(ThreadId);
	}

#if PLATFORM_SUPPORTS_ALL_THREAD_BACKTRACES
	struct FThreadStackBackTrace
	{
		uint32 ThreadId;
		FString ThreadName;
		TArray<uint64, TInlineAllocator<100>> ProgramCounters;
	};

	CORE_API void GetAllThreadStackBackTraces(TArray<FThreadStackBackTrace>& StackTraces);
#endif

	/**
	 * Enumerate each thread.
	 *
	 */
	CORE_API void ForEachThread(TFunction<void(uint32 ThreadId, FRunnableThread* Thread)> Func);

	/**
	 * Access to the singleton object.
	 *
	 * @return Thread manager object.
	 */
	static CORE_API FThreadManager& Get();

private:

	friend class FForkProcessHelper;

	/** Returns a list of registered forkable threads  */
	CORE_API TArray<FRunnableThread*> GetForkableThreads();

	/** Returns internal name of a the thread given its TLS id */
	CORE_API const FString& GetThreadNameInternal(uint32 ThreadId);
};
