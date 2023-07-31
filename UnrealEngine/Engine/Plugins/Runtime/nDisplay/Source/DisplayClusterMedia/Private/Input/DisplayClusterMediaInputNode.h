// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/DisplayClusterMediaInputBase.h"

class FRHICommandListImmediate;
class FViewport;
class IDisplayClusterViewportManagerProxy;


/**
 * Node backbuffer media input
 */
class FDisplayClusterMediaInputNode
	: public FDisplayClusterMediaInputBase
{
public:
	FDisplayClusterMediaInputNode(const FString& MediaId, const FString& ClusterNodeId, UMediaSource* MediaSource);

public:
	virtual bool Play() override;
	virtual void Stop() override;

protected:
	void OnPostFrameRender_RenderThread(FRHICommandListImmediate& RHICmdList, const IDisplayClusterViewportManagerProxy* ViewportManagerProxy, FViewport* Viewport);
};
