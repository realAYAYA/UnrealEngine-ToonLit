// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Capture/DisplayClusterMediaCaptureViewport.h"


/**
 * Tile media capture
 */
class FDisplayClusterMediaCaptureTile
	: public FDisplayClusterMediaCaptureViewport
{
public:
	FDisplayClusterMediaCaptureTile(const FString& MediaId, const FString& ClusterNodeId, const FString& ViewportId, UMediaOutput* MediaOutput, UDisplayClusterMediaOutputSynchronizationPolicy* SyncPolicy = nullptr);

protected:

	//~ Begin FDisplayClusterMediaCaptureViewport
	virtual bool GetCaptureSizeFromConfig(FIntPoint& OutSize) const override;
	//~ End FDisplayClusterMediaCaptureViewport
};
