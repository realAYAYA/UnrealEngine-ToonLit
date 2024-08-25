// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/DisplayDevice/IDisplayClusterDisplayDeviceProxy.h"

#include "OpenColorIORendering.h"
#include "OpenColorIOColorSpace.h"

/**
 * Display Device Proxy object (OCIO render pass)
 * [rendering thread]
 */
class FDisplayClusterDisplayDeviceProxy_OpenColorIO
	: public IDisplayClusterDisplayDeviceProxy
{
public:
	FDisplayClusterDisplayDeviceProxy_OpenColorIO(FOpenColorIOColorConversionSettings& InColorConversionSettings);
	virtual ~FDisplayClusterDisplayDeviceProxy_OpenColorIO() = default;

public:
	virtual bool HasFinalPass_RenderThread() const override;
	virtual bool AddFinalPass_RenderThread(FRDGBuilder& GraphBuilder, const FDisplayClusterViewport_Context& InViewportContext,
		FRHITexture2D* InputTextureRHI, const FIntRect& InputRect, FRHITexture2D* OutputTextureRHI, const FIntRect& OutputRect) const override;

public:
	const FString OCIOPassId;

private:
	FOpenColorIORenderPassResources OCIOPassResources;
};
