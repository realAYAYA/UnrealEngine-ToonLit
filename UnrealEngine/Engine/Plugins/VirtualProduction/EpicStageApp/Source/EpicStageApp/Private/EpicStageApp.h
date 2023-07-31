// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "StageAppBeaconReceiver.h"
#include "StageAppRouteHandler.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class FEpicStageAppModule : public IModuleInterface
{
	//~ Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface interface
	
	/** Handler for WebSocket routes. */
	FStageAppRouteHandler RouteHandler;

	/** Receiver that responds to beacon messages from the stage app. */
	FStageAppBeaconReceiver StageAppBeaconReceiver;

private:
	/** Start the beacon receiver so the engine can be seen by the app. */
	void StartupBeaconReceiver();

	/** Shut down the beacon receiver so the engine can no longer be seen by the app. */
	void ShutdownBeaconReceiver();

private:
	/** Whether the beacon receiver has been started and has not been stopped. */
	bool bIsBeaconReceiverRunning = false;

	/** Delegate for when the websocket server starts. */
	FDelegateHandle WebSocketServerStartedDelegate;
};

IMPLEMENT_MODULE(FEpicStageAppModule, EpicStageApp);