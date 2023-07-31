// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/DisplayClusterMediaInputBase.h"

class FRHICommandListImmediate;
class FViewport;
class IDisplayClusterViewportManagerProxy;


/**
 * Viewport media input
 */
class FDisplayClusterMediaInputViewport
	: public FDisplayClusterMediaInputBase
{
public:
	FDisplayClusterMediaInputViewport(const FString& MediaId, const FString& ClusterNodeId, const FString& ViewportId, UMediaSource* MediaSource);

public:
	virtual bool Play() override;
	virtual void Stop() override;

	const FString& GetViewportId() const
	{
		return ViewportId;
	}

private:
	void PostCrossGpuTransfer_RenderThread(FRHICommandListImmediate& RHICmdList, const IDisplayClusterViewportManagerProxy* ViewportManagerProxy, FViewport* Viewport);

private:
	const FString ViewportId;
};
