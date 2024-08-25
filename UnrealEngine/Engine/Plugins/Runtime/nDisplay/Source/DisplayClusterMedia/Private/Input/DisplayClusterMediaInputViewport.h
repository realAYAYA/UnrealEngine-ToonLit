// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/DisplayClusterMediaInputBase.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_Enums.h"

class FRHICommandListImmediate;
class FViewport;
class IDisplayClusterViewport;
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
	/** Start playback */
	virtual bool Play() override;

	/** Stop playback */
	virtual void Stop() override;

	/** Returns viewport ID bound for playback */
	const FString& GetViewportId() const
	{
		return ViewportId;
	}

private:
	/** PostCrossGpuTransfer callback handler where media data is pushed into nDisplay internal buffers */
	void PostCrossGpuTransfer_RenderThread(FRHICommandListImmediate& RHICmdList, const IDisplayClusterViewportManagerProxy* ViewportManagerProxy, FViewport* Viewport);

	/** UpdateViewportMediaState callback to configure media state for a viewoprt */
	void OnUpdateViewportMediaState(IDisplayClusterViewport* InViewport, EDisplayClusterViewportMediaState& InOutMediaState);

public:
	/** Force late OCIO pass */
	bool bForceLateOCIOPass = false;

private:
	/** Viewport ID assigned for this media input */
	const FString ViewportId;
};
