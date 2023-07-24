// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlayerCore.h"
#include "PlayerRuntimeGlobal.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"

namespace Electra
{
	namespace Global
	{
		TMap<FString, bool>									EnabledAnalyticsEvents;
		FDelegateHandle										ApplicationSuspendedDelegate;
		FDelegateHandle										ApplicationResumeDelegate;
		FDelegateHandle										ApplicationTerminatingDelegate;
		FCriticalSection									ApplicationHandlerLock;
		TArray<TWeakPtrTS<FApplicationTerminationHandler>>	ApplicationTerminationHandlers;
		TArray<TWeakPtrTS<FFGBGNotificationHandlers>>		ApplicationBGFGHandlers;
		volatile bool										bIsInBackground = false;
		volatile bool										bAppIsTerminating = false;
	}
	using namespace Global;


	static void HandleApplicationWillTerminate()
	{
		ApplicationHandlerLock.Lock();
		TArray<TWeakPtrTS<FApplicationTerminationHandler>>	CurrentHandlers(ApplicationTerminationHandlers);
		bAppIsTerminating = true;
		ApplicationHandlerLock.Unlock();
		for(auto &Handler : CurrentHandlers)
		{
			TSharedPtrTS<FApplicationTerminationHandler> Hdlr = Handler.Pin();
			if (Hdlr.IsValid())
			{
				Hdlr->Terminate();
			}
		}
	}

	static void HandleApplicationWillEnterBackground()
	{
		ApplicationHandlerLock.Lock();
		TArray<TWeakPtrTS<FFGBGNotificationHandlers>>	CurrentHandlers(ApplicationBGFGHandlers);
		bIsInBackground = true;
		ApplicationHandlerLock.Unlock();
		for(auto &Handler : CurrentHandlers)
		{
			TSharedPtrTS<FFGBGNotificationHandlers> Hdlr = Handler.Pin();
			if (Hdlr.IsValid())
			{
				Hdlr->WillEnterBackground();
			}
		}
	}

	static void HandleApplicationHasEnteredForeground()
	{
		ApplicationHandlerLock.Lock();
		TArray<TWeakPtrTS<FFGBGNotificationHandlers>>	CurrentHandlers(ApplicationBGFGHandlers);
		bIsInBackground = false;
		ApplicationHandlerLock.Unlock();
		for(auto &Handler : CurrentHandlers)
		{
			TSharedPtrTS<FFGBGNotificationHandlers> Hdlr = Handler.Pin();
			if (Hdlr.IsValid())
			{
				Hdlr->HasEnteredForeground();
			}
		}
	}


	//-----------------------------------------------------------------------------
	/**
	 * Initializes core service functionality. Memory hooks must have been registered before calling this function.
	 *
	 * @param configuration
	 *
	 * @return
	 */
	bool Startup(const Configuration& InConfiguration)
	{
		if (!ApplicationTerminatingDelegate.IsValid())
		{
			ApplicationTerminatingDelegate = FCoreDelegates::ApplicationWillTerminateDelegate.AddStatic(&HandleApplicationWillTerminate);
		}
		if (!ApplicationSuspendedDelegate.IsValid())
		{
			ApplicationSuspendedDelegate = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddStatic(&HandleApplicationWillEnterBackground);
		}
		if (!ApplicationResumeDelegate.IsValid())
		{
			ApplicationResumeDelegate = FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddStatic(&HandleApplicationHasEnteredForeground);
		}

		// Load the modules we depend on. They may have been loaded already, but we do it explicitly here to ensure that
		// they will not be unloaded on shutdown before this module here, otherwise there could be crashes.
		FModuleManager::Get().LoadModule(TEXT("ElectraBase"));
		FModuleManager::Get().LoadModule(TEXT("ElectraSamples"));
		FModuleManager::Get().LoadModule(TEXT("ElectraHTTPStream"));
		FModuleManager::Get().LoadModule(TEXT("ElectraSubtitles"));
		FModuleManager::Get().LoadModule(TEXT("ElectraCDM"));

		EnabledAnalyticsEvents = InConfiguration.EnabledAnalyticsEvents;
		return(true);
	}


	//-----------------------------------------------------------------------------
	/**
	 * Shuts down core services.
	 */
	void Shutdown(void)
	{
		if (ApplicationSuspendedDelegate.IsValid())
		{
			FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(ApplicationSuspendedDelegate);
			ApplicationSuspendedDelegate.Reset();
		}
		if (ApplicationResumeDelegate.IsValid())
		{
			FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(ApplicationResumeDelegate);
			ApplicationResumeDelegate.Reset();
		}
		if (ApplicationTerminatingDelegate.IsValid())
		{
			FCoreDelegates::ApplicationWillTerminateDelegate.Remove(ApplicationTerminatingDelegate);
		}
	}


	void AddTerminationNotificationHandler(TSharedPtrTS<FApplicationTerminationHandler> InHandler)
	{
		FScopeLock lock(&ApplicationHandlerLock);
		ApplicationTerminationHandlers.Add(InHandler);
	}

	void RemoveTerminationNotificationHandler(TSharedPtrTS<FApplicationTerminationHandler> InHandler)
	{
		FScopeLock lock(&ApplicationHandlerLock);
		ApplicationTerminationHandlers.Remove(InHandler);
	}

	bool AddBGFGNotificationHandler(TSharedPtrTS<FFGBGNotificationHandlers> InHandlers)
	{
		FScopeLock lock(&ApplicationHandlerLock);
		ApplicationBGFGHandlers.Add(InHandlers);
		return bIsInBackground;
	}

	void RemoveBGFGNotificationHandler(TSharedPtrTS<FFGBGNotificationHandlers> InHandlers)
	{
		FScopeLock lock(&ApplicationHandlerLock);
		ApplicationBGFGHandlers.Remove(InHandlers);
	}


	/**
	 * Check if the provided analytics event is enabled
	 *
	 * @param AnalyticsEventName of event to check
	 * @return true if event is found and is set to true
	 */
	bool IsAnalyticsEventEnabled(const FString& AnalyticsEventName)
	{
		const bool* bEventEnabled = EnabledAnalyticsEvents.Find(AnalyticsEventName);
		return bEventEnabled && *bEventEnabled;
	}
	
	class PendingTaskCounter
	{
	public:
		PendingTaskCounter() : AllDoneSignal(nullptr), NumPendingTasks(0)
		{
			// Note: we only initialize the done signal on first adding a task etc.
			// to avoid a signal to be used during the global constructor phase
			// (too early for UE)
		}

		~PendingTaskCounter()
		{
		}

		//! Adds a pending task.
		void AddTask()
		{
			Init();
			if (FMediaInterlockedIncrement(NumPendingTasks) == 0)
			{
				AllDoneSignal->Reset();
			}
		}

		//! Removes a pending task when it's done. Returns true when this was the last task, false otherwise.
		bool RemoveTask()
		{
			Init();
			if (FMediaInterlockedDecrement(NumPendingTasks) == 1)
			{
				AllDoneSignal->Signal();
				return true;
			}
			else
			{
				return false;
			}
		}

		//! Waits for all pending tasks to have finished. Once all are done new tasks cannot be added.
		bool WaitAllFinished(int32 TimeoutMs)
		{
			Init();
			if (TimeoutMs <= 0)
			{
				AllDoneSignal->Wait();
				return true;
			}
			return AllDoneSignal->WaitTimeout(TimeoutMs * 1000);
		}

		void Reset()
		{
			delete AllDoneSignal;
			AllDoneSignal = nullptr;
		}

	private:
		void Init()
		{
			// Initialize our signal event if we don't have it already...
			if (!AllDoneSignal)
			{
				FMediaEvent* NewSignal = new FMediaEvent();
				if (TMediaInterlockedExchangePointer(AllDoneSignal, NewSignal) != nullptr)
				{
					delete NewSignal;
				}
				// The new signal must be set initially to allow for WaitAllFinished() to leave
				// without any task having been added. It gets cleared on the first AddTask().
				AllDoneSignal->Signal();
			}
		}

		FMediaEvent* AllDoneSignal;
		int32		NumPendingTasks;
	};




	static PendingTaskCounter NumActivePlayers;


	bool WaitForAllPlayersToHaveTerminated()
	{
		bool bOk = NumActivePlayers.WaitAllFinished(1000);	// bAppIsTerminating ? 1000 : 0);
		if (bOk)
		{
			// Explicitly shutdown anything in the counter class that may use the engine (as it might shutdown after this)
			NumActivePlayers.Reset();
		}
		return bOk;
	}

	void AddActivePlayerInstance()
	{
		NumActivePlayers.AddTask();
	}

	void RemoveActivePlayerInstance()
	{
		NumActivePlayers.RemoveTask();
	}


};


