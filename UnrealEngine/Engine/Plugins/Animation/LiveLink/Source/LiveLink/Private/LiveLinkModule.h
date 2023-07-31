// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ILiveLinkModule.h"

#include "Features/IModularFeatures.h"

#include "LiveLinkClient.h"
#include "LiveLinkDebugCommand.h"
#include "LiveLinkHeartbeatEmitter.h"
#include "LiveLinkMessageBusDiscoveryManager.h"
#include "LiveLinkMotionController.h"

#include "Styling/SlateStyle.h"

/**
 * Implements the Messaging module.
 */

 struct FLiveLinkClientReference;

class FLiveLinkModule : public ILiveLinkModule
{
public:
	FLiveLinkModule();

	//~ Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual bool SupportsDynamicReloading() override { return false; }
	//~ End IModuleInterface interface

	//~ Begin ILiveLinkModule interface
	virtual FLiveLinkHeartbeatEmitter& GetHeartbeatEmitter() override { return *HeartbeatEmitter; }
#if WITH_LIVELINK_DISCOVERY_MANAGER_THREAD
	virtual FLiveLinkMessageBusDiscoveryManager& GetMessageBusDiscoveryManager() override { return *DiscoveryManager; }
#endif
	virtual TSharedPtr<FSlateStyleSet> GetStyle() override { return StyleSet; }
	//~ End ILiveLinkModule interface

private:
	void CreateStyle();
	void OnEngineLoopInitComplete();

private:
	friend FLiveLinkClientReference;
	static FLiveLinkClient* LiveLinkClient_AnyThread;

	FLiveLinkClient LiveLinkClient;
	FLiveLinkMotionController LiveLinkMotionController;

	TSharedPtr<FSlateStyleSet> StyleSet;

	TUniquePtr<FLiveLinkHeartbeatEmitter> HeartbeatEmitter;
	TUniquePtr<FLiveLinkMessageBusDiscoveryManager> DiscoveryManager;
	TUniquePtr<FLiveLinkDebugCommand> LiveLinkDebugCommand;
};
