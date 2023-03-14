// Copyright Epic Games, Inc. All Rights Reserved.

#include "BackgroundHttpNotificationObject.h"

#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#include "LocalNotification.h"

FBackgroundHttpNotificationObject::FBackgroundHttpNotificationObject(FText InNotificationTitle, FText InNotificationBody, FText InNotificationAction, const FString& InNotificationActivationString, bool InNotifyOnlyOnFullSuccess)
	: FBackgroundHttpNotificationObject(InNotificationTitle, InNotificationBody, InNotificationAction, InNotificationActivationString, InNotifyOnlyOnFullSuccess, true)
{
}

FBackgroundHttpNotificationObject::FBackgroundHttpNotificationObject(FText InNotificationTitle, FText InNotificationBody, FText InNotificationAction, const FString& InNotificationActivationString, bool InNotifyOnlyOnFullSuccess, bool InbOnlySendNotificationInBackground)
	: NotificationTitle(InNotificationTitle)
    , NotificationAction(InNotificationAction)
    , NotificationBody(InNotificationBody)
	, NotificationActivationString(InNotificationActivationString)
    , bOnlySendNotificationInBackground(InbOnlySendNotificationInBackground)
    , bNotifyOnlyOnFullSuccess(InNotifyOnlyOnFullSuccess)
	, bIsInBackground(false)
	, NumFailedDownloads(0)
    , PlatformNotificationService(nullptr)
    , OnApp_EnteringForegroundHandle()
	, OnApp_EnteringBackgroundHandle()
{
	if (GConfig)
	{
		FString ModuleName;
		GConfig->GetString(TEXT("LocalNotification"), TEXT("DefaultPlatformService"), ModuleName, GEngineIni);

		if (ModuleName.Len() > 0)
		{
			// load the module by name from the .ini
			if (ILocalNotificationModule* Module = FModuleManager::LoadModulePtr<ILocalNotificationModule>(*ModuleName))
			{
				PlatformNotificationService = Module->GetLocalNotificationService();
			}
		}
	}

	//Only register with this delegate if we are actually going to monitor background notifications
	if (bOnlySendNotificationInBackground)
	{
		OnApp_EnteringBackgroundHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddRaw(this, &FBackgroundHttpNotificationObject::OnApp_EnteringBackground);
		OnApp_EnteringForegroundHandle = FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddRaw(this, &FBackgroundHttpNotificationObject::OnApp_EnteringForeground);
	}
}

void FBackgroundHttpNotificationObject::OnApp_EnteringForeground()
{
	bIsInBackground = false;
}

void FBackgroundHttpNotificationObject::OnApp_EnteringBackground()
{
	bIsInBackground = true;
}

FBackgroundHttpNotificationObject::~FBackgroundHttpNotificationObject()
{
	if (bOnlySendNotificationInBackground)
	{
		//These should only be registered if bOnlySendNotificationInBackground is set
		FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Remove(OnApp_EnteringBackgroundHandle);
		FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(OnApp_EnteringForegroundHandle);

		//If we have flagged as only sending notifications when we are in the BG, and we are not in the BG, just early out
		//so we don't send a notification
		if (!bIsInBackground)
		{
			return;
		}
	}

	if (nullptr != PlatformNotificationService)
	{
		if (!bNotifyOnlyOnFullSuccess || (NumFailedDownloads == 0))
		{
			//make a notification 1 second from now
			FDateTime TargetTime = FDateTime::Now();
			TargetTime += FTimespan::FromSeconds(15);

			if (nullptr != PlatformNotificationService)
			{
				PlatformNotificationService->ScheduleLocalNotificationAtTime(TargetTime, true, NotificationTitle, NotificationBody, NotificationAction, NotificationActivationString);
			}
		}
	}
}

void FBackgroundHttpNotificationObject::NotifyOfDownloadResult(bool bWasSuccess)
{
	if (!bWasSuccess)
	{
		FPlatformAtomics::InterlockedIncrement(&NumFailedDownloads);
	}
}
