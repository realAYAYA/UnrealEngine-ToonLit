// Copyright Epic Games, Inc. All Rights Reserved.

#include "Android/AndroidEventManager.h"

#if USE_ANDROID_EVENTS
#include "Android/AndroidApplication.h"
#include "AudioDevice.h"
#include "Misc/CallbackDevice.h"
#include <android/native_window.h> 
#include <android/native_window_jni.h> 
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "RenderingThread.h"
#include "UnrealEngine.h"

DEFINE_LOG_CATEGORY(LogAndroidEvents);

FAppEventManager* FAppEventManager::sInstance = NULL;


FAppEventManager* FAppEventManager::GetInstance()
{
	if(!sInstance)
	{
		sInstance = new FAppEventManager();
	}

	return sInstance;
}

static const TCHAR* GetAppEventName(EAppEventState State)
{
	const TCHAR* Names[] = {
		TEXT("APP_EVENT_STATE_WINDOW_CREATED"),
		TEXT("APP_EVENT_STATE_WINDOW_RESIZED"),
		TEXT("APP_EVENT_STATE_WINDOW_CHANGED"),
		TEXT("APP_EVENT_STATE_WINDOW_DESTROYED"),
		TEXT("APP_EVENT_STATE_WINDOW_REDRAW_NEEDED"),
		TEXT("APP_EVENT_STATE_ON_DESTROY"),
		TEXT("APP_EVENT_STATE_ON_PAUSE"),
		TEXT("APP_EVENT_STATE_ON_RESUME"),
		TEXT("APP_EVENT_STATE_ON_STOP"),
		TEXT("APP_EVENT_STATE_ON_START"),
		TEXT("APP_EVENT_STATE_WINDOW_LOST_FOCUS"),
		TEXT("APP_EVENT_STATE_WINDOW_GAINED_FOCUS"),
		TEXT("APP_EVENT_STATE_SAVE_STATE"),
		TEXT("APP_EVENT_STATE_APP_SUSPENDED"),
		TEXT("APP_EVENT_STATE_APP_ACTIVATED"),
		TEXT("APP_EVENT_RUN_CALLBACK"),
		};


	if (State == APP_EVENT_STATE_INVALID)
	{
		return TEXT("APP_EVENT_STATE_INVALID");
	}
	else if (State > APP_EVENT_RUN_CALLBACK || State < 0)
	{
		return TEXT("UnknownEAppEventStateValue");
	}
	else
	{
		return Names[State];
	}
}


void FAppEventManager::Tick()
{
	check(IsInGameThread());
	while (!Queue.IsEmpty())
	{
		FAppEventPacket Event = DequeueAppEvent();
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAppEventManager::Tick processing, %d"), int(Event.State));

		switch (Event.State)
		{
		case APP_EVENT_STATE_WINDOW_CREATED:
			FAndroidWindow::EventManagerUpdateWindowDimensions(Event.Data.WindowWidth, Event.Data.WindowHeight);
			bCreateWindow = true;
			break;
		case APP_EVENT_STATE_WINDOW_RESIZED:
			// Cache the new window's dimensions for the game thread.
			FAndroidWindow::EventManagerUpdateWindowDimensions(Event.Data.WindowWidth, Event.Data.WindowHeight);
			ExecWindowResized();
			break;
		case APP_EVENT_STATE_WINDOW_CHANGED:
			// React on device orientation/windowSize changes only when application has window
			// In case window was created this tick it should already has correct size
			// see 'Java_com_epicgames_unreal_GameActivity_nativeOnConfigurationChanged' for event thread/game thread mismatches.
			ExecWindowResized();
		break;
		case APP_EVENT_STATE_SAVE_STATE:
			bSaveState = true; //todo android: handle save state.
			break;
		case APP_EVENT_STATE_WINDOW_DESTROYED:
			bHaveWindow = false;
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("APP_EVENT_STATE_WINDOW_DESTROYED, %d, %d, %d"), int(bRunning), int(bHaveWindow), int(bHaveGame));
			break;
		case APP_EVENT_STATE_ON_START:
			//doing nothing here
			break;
		case APP_EVENT_STATE_ON_DESTROY:
			check(bHaveWindow == false);
			check(IsEngineExitRequested()); //destroy immediately. Game will shutdown.
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("APP_EVENT_STATE_ON_DESTROY"));
			break;
		case APP_EVENT_STATE_ON_STOP:
			bHaveGame = false;
			ReleaseMicrophone(true);
			break;
		case APP_EVENT_STATE_ON_PAUSE:
			FAndroidAppEntry::OnPauseEvent();
			bHaveGame = false;
			break;
		case APP_EVENT_STATE_ON_RESUME:
			bHaveGame = true;
			break;

		// window focus events that follow their own hierarchy, and might or might not respect App main events hierarchy
		case APP_EVENT_STATE_WINDOW_GAINED_FOCUS: 
			bWindowInFocus = true;
			break;
		case APP_EVENT_STATE_WINDOW_LOST_FOCUS:
			bWindowInFocus = false;
			break;
		case APP_EVENT_RUN_CALLBACK:
		{
			UE_LOG(LogAndroidEvents, Display, TEXT("Event thread callback running."));
			Event.Data.CallbackFunc();
			break;
		}
		case APP_EVENT_STATE_APP_ACTIVATED:
			bRunning = true;
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Execution will be resumed!"));
			break;
		case APP_EVENT_STATE_APP_SUSPENDED:
			bRunning = false;
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Execution will be paused..."));
			break;
		default:
			UE_LOG(LogAndroidEvents, Display, TEXT("Application Event : %u  not handled. "), Event.State);
		}

		if (bCreateWindow)
		{
			// wait until activity is in focus.
			if (bWindowInFocus) 
			{
				ExecWindowCreated();
				bCreateWindow = false;
				bHaveWindow = true;
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("ExecWindowCreated, %d, %d, %d"), int(bRunning), int(bHaveWindow), int(bHaveGame));
			}
		}
	}

	if (EmptyQueueHandlerEvent)
	{
		EmptyQueueHandlerEvent->Trigger();
	}

	if (!bRunning)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAppEventManager::Tick EventHandlerEvent Wait "));
		EventHandlerEvent->Wait();
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FAppEventManager::Tick EventHandlerEvent DONE Wait "));
	}
}

void FAppEventManager::ReleaseMicrophone(bool shuttingDown)
{
	if (FModuleManager::Get().IsModuleLoaded("Voice"))
	{
		UE_LOG(LogTemp, Log, TEXT("Android release microphone"));
		FModuleManager::Get().UnloadModule("Voice", shuttingDown);
	}
}

void FAppEventManager::TriggerEmptyQueue()
{
	if (EmptyQueueHandlerEvent)
	{
		EmptyQueueHandlerEvent->Trigger();
	}
}

FAppEventManager::FAppEventManager():
	EventHandlerEvent(nullptr)
	,EmptyQueueHandlerEvent(nullptr)
	,FirstInitialized(false)
	,bCreateWindow(false)
	,bWindowInFocus(true)
	,bSaveState(false)
	,bAudioPaused(false)
	,bHaveWindow(false)
	,bHaveGame(false)
	,bRunning(false)
{
	pthread_mutex_init(&QueueMutex, NULL);

	IConsoleVariable* CVarScale = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MobileContentScaleFactor"));
	check(CVarScale);
	CVarScale->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&FAppEventManager::OnScaleFactorChanged));

	IConsoleVariable* CVarResX = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Mobile.DesiredResX"));
	check(CVarResX);
	CVarResX->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&FAppEventManager::OnScaleFactorChanged));

	IConsoleVariable* CVarResY = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Mobile.DesiredResY"));
	check(CVarResY);
	CVarResY->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&FAppEventManager::OnScaleFactorChanged));
}

void FAppEventManager::OnScaleFactorChanged(IConsoleVariable* CVar)
{
	if ((CVar->GetFlags() & ECVF_SetByMask) == ECVF_SetByConsole)
	{
		FAppEventManager::GetInstance()->ExecWindowResized();
	}
}

void FAppEventManager::HandleWindowCreated_EventThread(void* InWindow)
{
	bool AlreadyInited = FirstInitialized;

	// Make sure window will not be deleted until event is processed
	// Window could be deleted by OS while event queue stuck at game start-up phase
	FAndroidWindow::AcquireWindowRef((ANativeWindow*)InWindow);

	check(FAndroidWindow::GetHardwareWindow_EventThread() == NULL);
	FAndroidWindow::SetHardwareWindow_EventThread(InWindow);

	if (!AlreadyInited)
	{
		//This cannot wait until first tick. 
		FirstInitialized = true;
	}
	EnqueueAppEvent(APP_EVENT_STATE_WINDOW_CREATED, FAppEventData((ANativeWindow*)InWindow));
}

void FAppEventManager::HandleWindowClosed_EventThread()
{
	check(FAndroidWindow::GetHardwareWindow_EventThread());

	FAndroidWindow::ReleaseWindowRef((ANativeWindow*)FAndroidWindow::GetHardwareWindow_EventThread());
	FAndroidWindow::SetHardwareWindow_EventThread(nullptr);

	EnqueueAppEvent(APP_EVENT_STATE_WINDOW_DESTROYED);
}


void FAppEventManager::SetEventHandlerEvent(FEvent* InEventHandlerEvent)
{
	EventHandlerEvent = InEventHandlerEvent;
}

void FAppEventManager::SetEmptyQueueHandlerEvent(FEvent* InEventHandlerEvent)
{
	EmptyQueueHandlerEvent = InEventHandlerEvent;
}

void FAppEventManager::PauseRendering()
{
	if(GUseThreadedRendering )
	{
		if (GIsThreadedRendering)
		{
			StopRenderingThread(); 
		}
	}
	else
	{
		RHIReleaseThreadOwnership();
	}
}


void FAppEventManager::ResumeRendering()
{
	if( GUseThreadedRendering )
	{
		if (!GIsThreadedRendering)
		{
			StartRenderingThread();
		}
	}
	else
	{
		RHIAcquireThreadOwnership();
	}
}


void FAppEventManager::ExecWindowCreated()
{
	UE_LOG(LogAndroidEvents, Display, TEXT("ExecWindowCreated"));
	// When application launched while device is in sleep mode SystemResolution could be set to opposite orientation values
	// Force to update SystemResolution to current values whenever we create a new window
	FPlatformRect ScreenRect = FAndroidWindow::GetScreenRect();
	FSystemResolution::RequestResolutionChange(ScreenRect.Right, ScreenRect.Bottom, EWindowMode::Fullscreen);

	// ReInit with the new window handle
	FAndroidAppEntry::ReInitWindow();
	FAndroidApplication::OnWindowSizeChanged();
}

void FAppEventManager::ExecWindowResized()
{
	if (bRunning)
	{
		FlushRenderingCommands();
	}
	FAndroidWindow::InvalidateCachedScreenRect();
	FAndroidAppEntry::ReInitWindow();
	FAndroidApplication::OnWindowSizeChanged();
}

void FAppEventManager::PauseAudio()
{
	if (!GEngine || !GEngine->IsInitialized())
	{
		UE_LOG(LogTemp, Log, TEXT("Engine not initialized, not pausing Android audio"));
		return;
	}

	bAudioPaused = true;
	UE_LOG(LogTemp, Log, TEXT("Android pause audio"));

	FAudioDeviceHandle AudioDevice = GEngine->GetMainAudioDevice();
	if (AudioDevice)
	{
		if (AudioDevice->IsAudioMixerEnabled())
		{
			FAudioCommandFence Fence;
			Fence.BeginFence();
			Fence.Wait();

			AudioDevice->SuspendContext();
		}
		else
		{
			GEngine->GetMainAudioDevice()->Suspend(false);

			// make sure the audio thread runs the pause request
			FAudioCommandFence Fence;
			Fence.BeginFence();
			Fence.Wait();
		}
	}
}


void FAppEventManager::ResumeAudio()
{
	if (!GEngine || !GEngine->IsInitialized())
	{
		UE_LOG(LogTemp, Log, TEXT("Engine not initialized, not resuming Android audio"));
		return;
	}

	bAudioPaused = false;
	UE_LOG(LogTemp, Log, TEXT("Android resume audio"));

	FAudioDeviceHandle AudioDevice = GEngine->GetMainAudioDevice();
	if (AudioDevice)
	{
		if (AudioDevice->IsAudioMixerEnabled())
		{
			AudioDevice->ResumeContext();
		}
		else
		{
			GEngine->GetMainAudioDevice()->Suspend(true);
		}
	}
}


void FAppEventManager::EnqueueAppEvent(EAppEventState InState, FAppEventData&& InData)
{
	FAppEventPacket Event;
	Event.State = InState;
	Event.Data = InData;

	int rc = pthread_mutex_lock(&QueueMutex);
	check(rc == 0);
	Queue.Enqueue(Event);

	if (EmptyQueueHandlerEvent)
	{
		EmptyQueueHandlerEvent->Reset();
	}

	rc = pthread_mutex_unlock(&QueueMutex);
	check(rc == 0);

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("LogAndroidEvents::EnqueueAppEvent : %u, [width=%d, height=%d], tid = %d, %s"), InState, InData.WindowWidth, InData.WindowHeight, gettid(), GetAppEventName(InState));
}

FAppEventPacket FAppEventManager::DequeueAppEvent()
{
	int rc = pthread_mutex_lock(&QueueMutex);
	check(rc == 0);

	FAppEventPacket OutData;
	Queue.Dequeue( OutData );

	rc = pthread_mutex_unlock(&QueueMutex);
	check(rc == 0);

	UE_LOG(LogAndroidEvents, Display, TEXT("LogAndroidEvents::DequeueAppEvent : %u, [width=%d, height=%d], %s"), OutData.State, OutData.Data.WindowWidth, OutData.Data.WindowHeight, GetAppEventName(OutData.State))

	return OutData;
}


bool FAppEventManager::IsGamePaused()
{
	return !bRunning;
}


bool FAppEventManager::IsGameInFocus()
{
	return (bWindowInFocus && bHaveWindow);
}


bool FAppEventManager::WaitForEventInQueue(EAppEventState InState, double TimeoutSeconds)
{
	bool FoundEvent = false;
	double StopTime = FPlatformTime::Seconds() + TimeoutSeconds;

	TQueue<FAppEventPacket, EQueueMode::Spsc> HoldingQueue;
	while (!FoundEvent)
	{
		int rc = pthread_mutex_lock(&QueueMutex);
		check(rc == 0);

		// Copy the existing queue (and check for our event)
		while (!Queue.IsEmpty())
		{
			FAppEventPacket OutData;
			Queue.Dequeue(OutData);

			if (OutData.State == InState)
				FoundEvent = true;

			HoldingQueue.Enqueue(OutData);
		}

		if (FoundEvent)
			break;

		// Time expired?
		if (FPlatformTime::Seconds() > StopTime)
			break;

		// Unlock for new events and wait a bit before trying again
		rc = pthread_mutex_unlock(&QueueMutex);
		check(rc == 0);
		FPlatformProcess::Sleep(0.01f);
	}

	// Add events back to queue from holding
	while (!HoldingQueue.IsEmpty())
	{
		FAppEventPacket OutData;
		HoldingQueue.Dequeue(OutData);
		Queue.Enqueue(OutData);
	}

	int rc = pthread_mutex_unlock(&QueueMutex);
	check(rc == 0);

	return FoundEvent;
}

extern volatile bool GEventHandlerInitialized;

#endif
