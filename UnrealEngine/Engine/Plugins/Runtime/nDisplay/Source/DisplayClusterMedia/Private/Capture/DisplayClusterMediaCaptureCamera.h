// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Capture/DisplayClusterMediaCaptureViewport.h"


/**
 * Viewport media capture
 */
class FDisplayClusterMediaCaptureCamera
	: public FDisplayClusterMediaCaptureViewport
{
public:
	FDisplayClusterMediaCaptureCamera(const FString& MediaId, const FString& ClusterNodeId, const FString& CameraId, const FString& ViewportId, UMediaOutput* MediaOutput, UDisplayClusterMediaOutputSynchronizationPolicy* SyncPolicy = nullptr);

protected:
	//~ Begin FDisplayClusterMediaCaptureViewport
	virtual bool GetCaptureSizeFromConfig(FIntPoint& OutSize) const override;
	//~ End FDisplayClusterMediaCaptureViewport

private:
	/** ICVFX camera name */
	const FString CameraId;
};
