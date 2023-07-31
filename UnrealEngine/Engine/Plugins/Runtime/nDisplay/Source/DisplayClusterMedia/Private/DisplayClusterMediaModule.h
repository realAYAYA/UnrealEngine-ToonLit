// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

#include "Synchronization/DisplayClusterFrameQueue.h"

class FDisplayClusterMediaCaptureBase;
class FDisplayClusterMediaCaptureICVFX;
class FDisplayClusterMediaCaptureNode;
class FDisplayClusterMediaCaptureViewport;
class FDisplayClusterMediaInputBase;
class FDisplayClusterMediaInputICVFX;
class FDisplayClusterMediaInputNode;
class FDisplayClusterMediaInputViewport;


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
	TMap<FString, TUniquePtr<FDisplayClusterMediaCaptureViewport>> CaptureViewports;
	TUniquePtr<FDisplayClusterMediaCaptureNode>                    CaptureNode;
	TArray<FDisplayClusterMediaCaptureBase*>                       AllCaptures;

	TMap<FString, TUniquePtr<FDisplayClusterMediaInputViewport>>   InputViewports;
	TUniquePtr<FDisplayClusterMediaInputNode>                      InputNode;
	TArray<FDisplayClusterMediaInputBase*>                         AllInputs;

	// Latency queue
	FDisplayClusterFrameQueue FrameQueue;
};
