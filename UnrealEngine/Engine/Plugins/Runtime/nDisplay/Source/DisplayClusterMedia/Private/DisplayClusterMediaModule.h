// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

#include "Synchronization/LatencyQueue/DisplayClusterFrameQueue.h"

class FDisplayClusterMediaCaptureNode;
class FDisplayClusterMediaCaptureViewport;
class FDisplayClusterMediaInputNode;
class FDisplayClusterMediaInputViewport;
class IMediaPlayerFactory;


/**
 * Media module
 */
class FDisplayClusterMediaModule :
	public IModuleInterface
{
public:
	//~ Begin IModuleInterface Implementation
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface Implementation

protected:
	/** Initialize all media internals */
	void InitializeMedia();

	/** Release all media internals */
	void ReleaseMedia();

	/** Start media capture (all sources) */
	void StartCapture();

	/** Stop media capture (all sources) */
	void StopCapture();

	/** Start playing media (all inputs) */
	void PlayMedia();

	/** Stop playing media (all inputs) */
	void StopMedia();

private:
	void OnCustomPresentSet();
	void OnEnginePreExit();

private:
	TMap<FString, TSharedPtr<FDisplayClusterMediaCaptureViewport>> CaptureViewports;
	TMap<FString, TSharedPtr<FDisplayClusterMediaCaptureNode>>     CaptureNode;

	TMap<FString, TSharedPtr<FDisplayClusterMediaInputViewport>>   InputViewports;
	TSharedPtr<FDisplayClusterMediaInputNode>                      InputNode;

	// Latency queue
	FDisplayClusterFrameQueue FrameQueue;
};
