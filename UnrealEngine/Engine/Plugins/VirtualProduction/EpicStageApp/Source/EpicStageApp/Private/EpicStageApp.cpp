// Copyright Epic Games, Inc. All Rights Reserved.

#include "EpicStageApp.h"
#include "IWebRemoteControlModule.h"

void FEpicStageAppModule::StartupModule()
{
	IWebRemoteControlModule& WebRemoteControl = FModuleManager::LoadModuleChecked<IWebRemoteControlModule>("WebRemoteControl");
	RouteHandler.RegisterRoutes(WebRemoteControl);

	if (!IsRunningCommandlet())
	{
		if (WebRemoteControl.IsWebSocketServerRunning())
		{
			StartupBeaconReceiver();
		}
		else
		{
			// Wait for the WebSocket server to start
			WebSocketServerStartedDelegate = WebRemoteControl.OnWebSocketServerStarted().AddLambda([this](uint32)
			{
				StartupBeaconReceiver();
			});
		}
	}
}

void FEpicStageAppModule::ShutdownModule()
{
	if (IWebRemoteControlModule* WebRemoteControl = FModuleManager::GetModulePtr<IWebRemoteControlModule>("WebRemoteControl"))
	{
		RouteHandler.UnregisterRoutes(*WebRemoteControl);

		if (WebSocketServerStartedDelegate.IsValid())
		{
			WebRemoteControl->OnWebSocketServerStarted().Remove(WebSocketServerStartedDelegate);
		}
	}

	ShutdownBeaconReceiver();
}

void FEpicStageAppModule::StartupBeaconReceiver()
{
	// Make sure it's not already running
	ShutdownBeaconReceiver();

	StageAppBeaconReceiver.Startup();
	bIsBeaconReceiverRunning = true;
}

void FEpicStageAppModule::ShutdownBeaconReceiver()
{
	if (bIsBeaconReceiverRunning)
	{
		StageAppBeaconReceiver.Shutdown();
		bIsBeaconReceiverRunning = false;
	}
}
