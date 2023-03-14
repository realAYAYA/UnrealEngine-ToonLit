// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/RunnableThread.h"
#include "HAL/Runnable.h"
#include "HAL/Event.h"
#include "HAL/ThreadManager.h"
#include "Containers/StringConv.h"
#include "Microsoft/WindowsHWrapper.h"
#include "HoloLensPlatformProcess.h"
#include "Microsoft/AllowMicrosoftPlatformTypes.h"

/**
* This class has a lot of code that is very similar to FRunnableThreadWin.  It would make a lot of sense to refactor into an FRunnableThreadMicrosoft 
* from which both this and FRunnableThreadWin could derive.  Unfortunately another platform is using the FRunnableThreadWin .h and providing its own
* function definitions in a different .cpp and there are dire perforance, debugging, crash reporting, and exception handling concerns about the use of virtual
* functions in this thread class.  So this refactor would have to be done carefully on all platforms.
*/
class FRunnableThreadHoloLens
	: public FRunnableThread
{
	/** The thread handle for the thread. */
	HANDLE Thread = 0;

	/**
	* Helper function to set thread names, visible by the debugger.
	* @param ThreadID		Thread ID for the thread who's name is going to be set
	* @param ThreadName	Name to set
	*/
	static void SetThreadName(uint32 ThreadID, LPCSTR ThreadName)
	{
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
		/**
		* Code setting the thread name for use in the debugger.
		*
		* http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
		*/
		const uint32 MS_VC_EXCEPTION = 0x406D1388;

		struct THREADNAME_INFO
		{
			uint32 dwType;		// Must be 0x1000.
			LPCSTR szName;		// Pointer to name (in user addr space).
			uint32 dwThreadID;	// Thread ID (-1=caller thread).
			uint32 dwFlags;		// Reserved for future use, must be zero.
		};

		// on the xbox setting thread names messes up the XDK COM API that UnrealConsole uses so check to see if they have been
		// explicitly enabled
		Sleep(10);
		THREADNAME_INFO ThreadNameInfo;
		ThreadNameInfo.dwType = 0x1000;
		ThreadNameInfo.szName = ThreadName;
		ThreadNameInfo.dwThreadID = ThreadID;
		ThreadNameInfo.dwFlags = 0;

		__try
		{
			RaiseException(MS_VC_EXCEPTION, 0, sizeof(ThreadNameInfo) / sizeof(ULONG_PTR), (ULONG_PTR*)&ThreadNameInfo);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
			CA_SUPPRESS(6322)
		{
		}
#endif
	}

	/**
	* The thread entry point. Simply forwards the call on to the right
	* thread main function
	*/
	static DWORD STDCALL _ThreadProc(LPVOID pThis)
	{
		check(pThis);
		auto* ThisThread = (FRunnableThreadHoloLens*)pThis;
		FThreadManager::Get().AddThread(ThisThread->GetThreadID(), ThisThread);
		return ThisThread->GuardedRun();
	}

	/** Guarding works only if debugger is not attached or GAlwaysReportCrash is true. */
	uint32 GuardedRun();

	/**
	* The real thread entry point. It calls the Init/Run/Exit methods on
	* the runnable object
	*/
	uint32 Run();

public:
	~FRunnableThreadHoloLens()
	{
		// Clean up our thread if it is still active
		if (Thread != NULL)
		{
			Kill(true);
		}
	}

	virtual void SetThreadPriority(EThreadPriority NewPriority) override
	{
		// Don't bother calling the OS if there is no need
		if (NewPriority != ThreadPriority)
		{
			ThreadPriority = NewPriority;
			// Change the priority on the thread
			::SetThreadPriority(Thread, TranslateThreadPriority(ThreadPriority));
		}
	}

	virtual void Suspend(bool bShouldPause = 1) override
	{
		check(Thread);
		if (bShouldPause == true)
		{
			SuspendThread(Thread);
		}
		else
		{
			ResumeThread(Thread);
		}
	}

	virtual bool Kill(bool bShouldWait = false) override
	{
		check(Thread && "Did you forget to call Create()?");
		bool bDidExitOK = true;
		// Let the runnable have a chance to stop without brute force killing
		if (Runnable)
		{
			Runnable->Stop();
		}
		// If waiting was specified, wait the amount of time. If that fails,
		// brute force kill that thread. Very bad as that might leak.
		if (bShouldWait == true)
		{
			// Wait indefinitely for the thread to finish.  IMPORTANT:  It's not safe to just go and
			// kill the thread with TerminateThread() as it could have a mutex lock that's shared
			// with a thread that's continuing to run, which would cause that other thread to
			// dead-lock.  (This can manifest itself in code as simple as the synchronization
			// object that is used by our logging output classes.  Trust us, we've seen it!)
			WaitForSingleObjectEx(Thread, INFINITE, FALSE);
		}
		// Now clean up the thread handle so we don't leak
		CloseHandle(Thread);
		Thread = NULL;

		return bDidExitOK;
	}

	virtual void WaitForCompletion() override
	{
		// Block until this thread exits
		WaitForSingleObjectEx(Thread, INFINITE, FALSE);
	}

	static int TranslateThreadPriority(EThreadPriority Priority);
protected:

	virtual bool CreateInternal(FRunnable* InRunnable, const TCHAR* InThreadName,
		uint32 InStackSize = 0,
		EThreadPriority InThreadPri = TPri_Normal, uint64 InThreadAffinityMask = 0,
		EThreadCreateFlags InCreateFlags = EThreadCreateFlags::None) override
	{
		check(InRunnable);
		Runnable = InRunnable;
		ThreadAffinityMask = InThreadAffinityMask;

		// Create a sync event to guarantee the Init() function is called first
		ThreadInitSyncEvent = FPlatformProcess::GetSynchEventFromPool(true);

		ThreadName = InThreadName ? InThreadName : TEXT("Unnamed UE");
		ThreadPriority = InThreadPri;

		// Create the thread as suspended, so we can ensure ThreadId is initialized and the thread manager knows about the thread before it runs.
		Thread = CreateThread(NULL, InStackSize, _ThreadProc, this, STACK_SIZE_PARAM_IS_A_RESERVATION | CREATE_SUSPENDED, (DWORD *)&ThreadID);

		// If it fails, clear all the vars
		if (Thread == NULL)
		{
			Runnable = NULL;
		}
		else
		{
			ResumeThread(Thread);

			// Let the thread start up, then set the name for debug purposes.
			ThreadInitSyncEvent->Wait(INFINITE);
			SetThreadName(ThreadID, TCHAR_TO_ANSI(*ThreadName));

			ThreadPriority = TPri_Normal;
			SetThreadPriority(InThreadPri);
		}

		// Cleanup the sync event
		FPlatformProcess::ReturnSynchEventToPool(ThreadInitSyncEvent);
		ThreadInitSyncEvent = nullptr;
		return Thread != NULL;
	}
};
#include "Microsoft/HideMicrosoftPlatformTypes.h"
