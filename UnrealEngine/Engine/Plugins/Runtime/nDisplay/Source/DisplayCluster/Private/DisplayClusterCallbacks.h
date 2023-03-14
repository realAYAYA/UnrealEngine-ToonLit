// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDisplayClusterCallbacks.h"


/**
 * DisplayCluster callbacks API implementation
 */
class FDisplayClusterCallbacks :
	public  IDisplayClusterCallbacks
{
public:
	virtual FDisplayClusterStartSessionEvent& OnDisplayClusterStartSession() override
	{
		return DisplayClusterStartSessionEvent;
	}

	virtual FDisplayClusterEndSessionEvent& OnDisplayClusterEndSession() override
	{
		return DisplayClusterEndSessionEvent;
	}

	virtual FDisplayClusterStartFrameEvent& OnDisplayClusterStartFrame() override
	{
		return DisplayClusterStartFrameEvent;
	}

	virtual FDisplayClusterEndFrameEvent& OnDisplayClusterEndFrame() override
	{
		return DisplayClusterEndFrameEvent;
	}

	virtual FDisplayClusterPreTickEvent& OnDisplayClusterPreTick() override
	{
		return DisplayClusterPreTickEvent;
	}

	virtual FDisplayClusterTickEvent& OnDisplayClusterTick() override
	{
		return DisplayClusterTickEvent;
	}

	virtual FDisplayClusterPostTickEvent& OnDisplayClusterPostTick() override
	{
		return DisplayClusterPostTickEvent;
	}

	virtual FDisplayClusterStartSceneEvent& OnDisplayClusterStartScene() override
	{
		return DisplayClusterStartSceneEvent;
	}

	virtual FDisplayClusterEndSceneEvent& OnDisplayClusterEndScene() override
	{
		return DisplayClusterEndSceneEvent;
	}

	virtual FDisplayClusterCustomPresentSetEvent& OnDisplayClusterCustomPresentSet() override
	{
		return DisplayClusterCustomPresentSetEvent;
	}

	virtual FDisplayClusterPresentationPreSynchronization_RHIThread& OnDisplayClusterPresentationPreSynchronization_RHIThread() override
	{
		return DisplayClusterPresentationPreSynchronizationEvent;
	}

	virtual FDisplayClusterPresentationPostSynchronization_RHIThread& OnDisplayClusterPresentationPostSynchronization_RHIThread() override
	{
		return DisplayClusterPresentationPostSynchronizationEvent;
	}

	virtual FDisplayClusterFailoverNodeDown& OnDisplayClusterFailoverNodeDown() override
	{
		return DisplayClusterFailoverNodeDown;
	}

	virtual FDisplayClusterPostRenderViewFamily_RenderThread& OnDisplayClusterPostRenderViewFamily_RenderThread() override
	{
		return DisplayClusterPostRenderViewFamily;
	}

	virtual FDisplayClusterPreWarp_RenderThread& OnDisplayClusterPreWarp_RenderThread() override
	{
		return DisplayClusterPreWarpEvent;
	}

	virtual FDisplayClusterPreWarpViewport_RenderThread& OnDisplayClusterPreWarpViewport_RenderThread() override
	{
		return DisplayClusterPreWarpViewportEvent;
	}

	virtual FDisplayClusterPostWarp_RenderThread& OnDisplayClusterPostWarp_RenderThread() override
	{
		return DisplayClusterPostWarpEvent;
	}

	virtual FDisplayClusterPostWarpViewport_RenderThread& OnDisplayClusterPostWarpViewport_RenderThread() override
	{
		return DisplayClusterPostWarpViewportEvent;
	}

	virtual FDisplayClusterPostCrossGpuTransfer_RenderThread& OnDisplayClusterPostCrossGpuTransfer_RenderThread() override
	{
		return DisplayClusterPostCrossGpuTransferEvent;
	}

	virtual FDisplayClusterProcessLatency_RenderThread& OnDisplayClusterProcessLatency_RenderThread() override
	{
		return FDisplayClusterProcessLatencyEvent;
	}

	virtual FDisplayClusterPostFrameRender_RenderThread& OnDisplayClusterPostFrameRender_RenderThread() override
	{
		return DisplayClusterPostFrameRenderEvent;
	}

	virtual FDisplayClusterPostBackbufferUpdate_RenderThread& OnDisplayClusterPostBackbufferUpdate_RenderThread() override
	{
		return DisplayClusterPostBackbufferUpdateEvent;
	}

	virtual FDisplayClusterPreProcessIcvfx_RenderThread& OnDisplayClusterPreProcessIcvfx_RenderThread() override
	{
		return FDisplayClusterPreProcessIcvfxEvent;
	}

private:
	FDisplayClusterStartSessionEvent         DisplayClusterStartSessionEvent;
	FDisplayClusterEndSessionEvent           DisplayClusterEndSessionEvent;
	FDisplayClusterStartFrameEvent           DisplayClusterStartFrameEvent;
	FDisplayClusterEndFrameEvent             DisplayClusterEndFrameEvent;
	FDisplayClusterPreTickEvent              DisplayClusterPreTickEvent;
	FDisplayClusterTickEvent                 DisplayClusterTickEvent;
	FDisplayClusterPostTickEvent             DisplayClusterPostTickEvent;
	FDisplayClusterStartSceneEvent           DisplayClusterStartSceneEvent;
	FDisplayClusterEndSceneEvent             DisplayClusterEndSceneEvent;
	FDisplayClusterCustomPresentSetEvent     DisplayClusterCustomPresentSetEvent;
	FDisplayClusterFailoverNodeDown          DisplayClusterFailoverNodeDown;

	FDisplayClusterPresentationPreSynchronization_RHIThread  DisplayClusterPresentationPreSynchronizationEvent;
	FDisplayClusterPresentationPostSynchronization_RHIThread DisplayClusterPresentationPostSynchronizationEvent;

	FDisplayClusterPostRenderViewFamily_RenderThread DisplayClusterPostRenderViewFamily;
	FDisplayClusterPreWarp_RenderThread              DisplayClusterPreWarpEvent;
	FDisplayClusterPreWarpViewport_RenderThread      DisplayClusterPreWarpViewportEvent;
	FDisplayClusterPostWarp_RenderThread             DisplayClusterPostWarpEvent;
	FDisplayClusterPostWarpViewport_RenderThread     DisplayClusterPostWarpViewportEvent;
	FDisplayClusterPostCrossGpuTransfer_RenderThread DisplayClusterPostCrossGpuTransferEvent;
	FDisplayClusterProcessLatency_RenderThread       FDisplayClusterProcessLatencyEvent;
	FDisplayClusterPostFrameRender_RenderThread      DisplayClusterPostFrameRenderEvent;
	FDisplayClusterPostBackbufferUpdate_RenderThread DisplayClusterPostBackbufferUpdateEvent;
	FDisplayClusterPreProcessIcvfx_RenderThread      FDisplayClusterPreProcessIcvfxEvent;
};
