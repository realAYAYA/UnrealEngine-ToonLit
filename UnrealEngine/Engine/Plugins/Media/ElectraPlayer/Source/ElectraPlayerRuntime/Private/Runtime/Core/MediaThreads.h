// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Core/MediaTypes.h"
#include "Core/MediaMacros.h"
#include "Core/MediaNoncopyable.h"
#include "Core/MediaEventSignal.h"

#include "HAL/PlatformAffinity.h"
#include "HAL/Runnable.h"
#include "HAL/PlatformProcess.h"
#include "HAL/CriticalSection.h"
#include "Delegates/Delegate.h"

#include "Containers/UnrealString.h"
#include "Containers/StringConv.h"
#include "Templates/SharedPointer.h"

/**
 *
 *
*/
class FMediaRunnable : private TMediaNoncopyable<FMediaRunnable>, public FRunnable
{
public:
	DECLARE_DELEGATE(FStartDelegate);

	//! Common thread configuration parameters.
	struct Param
	{
		Param()
		{
			// Set some standard values as defaults.
			Priority = TPri_Normal;
			StackSize = 65536;
			CoreAffinity = -1;
		}
		EThreadPriority		Priority;
		uint32				StackSize;
		int32				CoreAffinity;
	};

	static FMediaRunnable* Create(int32 CoreAffinityMask, EThreadPriority Priority, uint32 StackSize, const FString& InThreadName);
	static void Destroy(FMediaRunnable* Thread);

	void Start(FStartDelegate Entry, bool bWaitRunning = false);

	void SetDoneSignal(FMediaEvent* DoneSignal);

	void SetName(const FString& InThreadName);

	EThreadPriority ChangePriority(EThreadPriority NewPriority);

	EThreadPriority PriorityGet() const
	{
		return ThreadPriority;
	}

	//! Returns the size of the stack.
	uint32 StackSizeGet() const
	{
		return StackSize;
	}

	//! Returns the default stack size passed to Startup()
	static uint32 StackSizeGetDefault()
	{
		return 0;
	}

	static void SleepSeconds(uint32 Seconds)
	{
		FPlatformProcess::SleepNoStats((float)Seconds);
	}

	static void SleepMilliseconds(uint32 Milliseconds)
	{
		FPlatformProcess::SleepNoStats(Milliseconds / 1000.0f);
	}

	static void SleepMicroseconds(uint32 Microseconds)
	{
		FPlatformProcess::SleepNoStats(Microseconds / 1000000.0f);
	}

private:

	FRunnableThread*		MediaThreadRunnable;
	EThreadPriority			ThreadPriority;
	uint64					InitialCoreAffinity;
	uint32					StackSize;
	FString					ThreadName;
	FCriticalSection		StateAccessMutex;
	FEvent*					SignalRunning;
	FStartDelegate			EntryFunction;
	FMediaEvent*			DoneSignal;
	bool					bIsStarted;

	FMediaRunnable();
	~FMediaRunnable();

	void StartInternal();
	uint32 Run() override;
	void Exit() override;
};


/**
 * A thread base class to either inherit from or use as a variable.
 *
 * Thread parameters are given to constructor but can be changed before starting the thread
 * with the ThreadSet..() functions.
 * This allows the constructor to use system default values so an instance of this class
 * can be used as a member variable of some class.
 *
 * To start the thread on some function of type void(*)(void) call ThreadStart() with
 * an appropriate delegate.
 *
 * IMPORTANT: If used as a member variable you have to ensure for your class to stay alive
 *            i.e. not be destroyed before the thread function has finished. Otherwise your
 *            function will most likely crash.
 *            Either wait for thread completion by calling ThreadWaitDone() *OR*
 *            have the destructor wait for the thread itself by calling ThreadWaitDoneOnDelete(true)
 *            at some point prior to destruction - preferably before starting the thread.
**/
class FMediaThread : private TMediaNoncopyable<FMediaThread>
{
public:
	virtual ~FMediaThread();

	//! Default constructor. Uses system defaults if no values given.
	FMediaThread(const char* AnsiName = nullptr);

	//! Set a thread priority other than the one given to the constructor before starting the thread.
	void ThreadSetPriority(EThreadPriority Priority);

	//! Set a core affinity before starting the thread. Defaults to -1 for no affinity (run on any core).
	void ThreadSetCoreAffinity(int32 CoreAffinity);

	//! Set a thread stack size other than the one given to the constructor before starting the thread.
	void ThreadSetStackSize(uint32 StackSize);

	//! Set a thread name other than the one given to the constructor before starting the thread.
	void ThreadSetName(const char* InAnsiThreadName);

	//! Sets a new thread name once the thread is running.
	void ThreadRename(const char* InAnsiThreadName);

	//! Sets whether or not the destructor needs to wait for the thread to have finished. Defaults to false. Useful when using this as a member variable.
	void ThreadWaitDoneOnDelete(bool bWait);

	//! Waits for the thread to have finished.
	void ThreadWaitDone();

	//! Starts the thread at the given function void(*)(void)
	void ThreadStart(FMediaRunnable::FStartDelegate EntryFunction);

	//! Resets the thread to be started again. Must have waited for thread termination using ThreadWaitDone() first!
	void ThreadReset();

private:
	FMediaEvent						SigDone;
	FMediaRunnable*					MediaRunnable;
	EThreadPriority					Priority;
	int32							CoreAffinity;
	uint32							StackSize;
	FString 						ThreadName;
	bool							bIsStarted;
	bool							bWaitDoneOnDelete;

};
