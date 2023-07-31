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
	FDisplayClusterMediaCaptureNode(const FString& MediaId, const FString& ClusterNodeId, UMediaOutput* MediaOutput);

public:
	virtual bool StartCapture() override;
	virtual void StopCapture() override;

protected:
	virtual FIntPoint GetCaptureSize() const override;

private:
	void OnPostFrameRender_RenderThread(FRHICommandListImmediate& RHICmdList, const IDisplayClusterViewportManagerProxy* ViewportManagerProxy, FViewport* Viewport);

	// Auxiliary method to create an intermediate texture
	FTextureRHIRef CreateIntermediateTexture_RenderThread(EPixelFormat Format, ETextureCreateFlags Flags, const FIntPoint& Size);

private:
	// We need an intermediate texture with the same size as backbuffer has. This allows to avoid
	// any texture size related problems caused by optimizations in the nD rendering pipeline.
	FTextureRHIRef InterimTexture = nullptr;
};
