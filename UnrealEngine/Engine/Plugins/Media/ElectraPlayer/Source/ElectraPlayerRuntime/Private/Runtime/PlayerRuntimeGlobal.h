// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"
#include "Templates/Function.h"

namespace Electra
{
	struct Configuration
	{	
		TMap<FString, bool>	EnabledAnalyticsEvents;
	};

	//! Initializes core service functionality. Memory hooks must have been registered before calling this function.
	bool Startup(const Configuration& Configuration);

	//! Shuts down core services.
	void Shutdown();

	//! Waits until all player instances have terminated, which may happen asynchronously.
	bool WaitForAllPlayersToHaveTerminated();


	void AddActivePlayerInstance();
	void RemoveActivePlayerInstance();

	//! Check if an analytics event is enabled
	bool IsAnalyticsEventEnabled(const FString& AnalyticsEventName);


	//! Application termination
	struct FApplicationTerminationHandler
	{
		TFunction<void()> Terminate;
	};
	void AddTerminationNotificationHandler(TSharedPtrTS<FApplicationTerminationHandler> InHandler);
	void RemoveTerminationNotificationHandler(TSharedPtrTS<FApplicationTerminationHandler> InHandler);

	//! Background / foreground handling
	struct FFGBGNotificationHandlers
	{
		TFunction<void()> WillEnterBackground;
		TFunction<void()> HasEnteredForeground;
	};
	bool AddBGFGNotificationHandler(TSharedPtrTS<FFGBGNotificationHandlers> InHandlers);
	void RemoveBGFGNotificationHandler(TSharedPtrTS<FFGBGNotificationHandlers> InHandlers);
};

