// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Templates/SharedPointer.h"

class BACKGROUNDHTTP_API FBackgroundHttpNotificationObject
	: public TSharedFromThis<FBackgroundHttpNotificationObject, ESPMode::ThreadSafe>
{
public:
	FBackgroundHttpNotificationObject(FText InNotificationTitle, FText InNotificationBody, FText InNotificationAction, const FString& InNotificationActivationString, bool InNotifyOnlyOnFullSuccess);
	FBackgroundHttpNotificationObject(FText InNotificationTitle, FText InNotificationBody, FText InNotificationAction, const FString& InNotificationActivationString, bool InNotifyOnlyOnFullSuccess, bool bOnlySendNotificationInBackground);
	~FBackgroundHttpNotificationObject();

	void NotifyOfDownloadResult(bool bWasSuccess);

private:
	void OnApp_EnteringBackground();
	void OnApp_EnteringForeground();

private:
	FText NotificationTitle;
	FText NotificationAction;
	FText NotificationBody;
	FString NotificationActivationString;

	bool bOnlySendNotificationInBackground;
	bool bNotifyOnlyOnFullSuccess;
	
	volatile bool bIsInBackground;
	volatile int32 NumFailedDownloads;

	class ILocalNotificationService* PlatformNotificationService;

	FDelegateHandle OnApp_EnteringForegroundHandle;
	FDelegateHandle OnApp_EnteringBackgroundHandle;

	//No default constructor
	FBackgroundHttpNotificationObject() {}
};

typedef TSharedPtr<FBackgroundHttpNotificationObject, ESPMode::ThreadSafe> FBackgroundHttpNotificationObjectPtr;
