// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Capture/DisplayClusterMediaCaptureBase.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_Enums.h"

class FRDGBuilder;
class FSceneViewFamily;
class IDisplayClusterViewport;
class IDisplayClusterViewportProxy;


/**
 * Viewport media capture
 */
class FDisplayClusterMediaCaptureViewport
	: public FDisplayClusterMediaCaptureBase
{
public:
	FDisplayClusterMediaCaptureViewport(const FString& MediaId, const FString& ClusterNodeId, const FString& ViewportId, UMediaOutput* MediaOutput, UDisplayClusterMediaOutputSynchronizationPolicy* SyncPolicy = nullptr);

public:
	/** Start capturing */
	virtual bool StartCapture() override;

	/** Stop capturing */
	virtual void StopCapture() override;

	/** Returns viewport ID that is configured for capture */
	const FString& GetViewportId() const
	{
		return ViewportId;
	}

protected:
	/** Returns texture size of a viewport assigned to capture */
	virtual FIntPoint GetCaptureSize() const override;

	/** Provides default texture size from config */
	virtual bool GetCaptureSizeFromConfig(FIntPoint& OutSize) const;

	/** Provides texture size from a game proxy (if available) */
	bool GetCaptureSizeFromGameProxy(FIntPoint& OutSize) const;

private:
	/** PostRenderViewFamily callback handler where data is captured */
	void OnPostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily, const IDisplayClusterViewportProxy* ViewportProxy);

	/** UpdateViewportMediaState callback to configure media state for a viewoprt */
	void OnUpdateViewportMediaState(IDisplayClusterViewport* InViewport, EDisplayClusterViewportMediaState& InOutMediaState);

public:
	/** Force late OCIO pass */
	bool bForceLateOCIOPass = false;

private:
	/** Viewport ID assigned to capture */
	const FString ViewportId;
};
