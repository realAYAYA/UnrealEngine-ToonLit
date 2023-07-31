// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Capture/DisplayClusterMediaCaptureViewport.h"

class FRDGBuilder;
class FSceneViewFamily;
class IDisplayClusterViewportProxy;


/**
 * Viewport media capture
 */
class FDisplayClusterMediaCaptureCamera
	: public FDisplayClusterMediaCaptureViewport
{
public:
	FDisplayClusterMediaCaptureCamera(const FString& MediaId, const FString& ClusterNodeId, const FString& CameraId, const FString& ViewportId, UMediaOutput* MediaOutput);

protected:
	
	virtual FIntPoint GetCaptureSize() const override;

private:
	const FString CameraId;
	FIntPoint CameraResolution = FIntPoint::ZeroValue;
};
