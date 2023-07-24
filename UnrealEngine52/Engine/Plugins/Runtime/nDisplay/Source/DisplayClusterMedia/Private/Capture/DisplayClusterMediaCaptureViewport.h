// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Capture/DisplayClusterMediaCaptureBase.h"

class FRDGBuilder;
class FSceneViewFamily;
class IDisplayClusterViewportProxy;


/**
 * Viewport media capture
 */
class FDisplayClusterMediaCaptureViewport
	: public FDisplayClusterMediaCaptureBase
{
public:
	FDisplayClusterMediaCaptureViewport(const FString& MediaId, const FString& ClusterNodeId, const FString& ViewportId, UMediaOutput* MediaOutput);

public:
	virtual bool StartCapture() override;
	virtual void StopCapture() override;

	const FString& GetViewportId() const
	{
		return ViewportId;
	}

protected:
	
	virtual FIntPoint GetCaptureSize() const override;

private:
	void OnPostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily, const IDisplayClusterViewportProxy* ViewportProxy);

private:
	const FString ViewportId;
};
