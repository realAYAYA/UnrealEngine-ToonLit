// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Capture/DisplayClusterMediaCaptureBase.h"

#include "RHIResources.h"

class FRHICommandListImmediate;
class FViewport;
class IDisplayClusterViewportManagerProxy;


/**
 * Node backbuffer media capture
 */
class FDisplayClusterMediaCaptureNode
	: public FDisplayClusterMediaCaptureBase
{
public:
	FDisplayClusterMediaCaptureNode(const FString& MediaId, const FString& ClusterNodeId, UMediaOutput* MediaOutput, UDisplayClusterMediaOutputSynchronizationPolicy* SyncPolicy = nullptr);

public:
	virtual bool StartCapture() override;
	virtual void StopCapture() override;

protected:
	virtual FIntPoint GetCaptureSize() const override;

private:
	void OnPostBackbufferUpdated_RenderThread(FRHICommandListImmediate& RHICmdList, FViewport* Viewport);
};
