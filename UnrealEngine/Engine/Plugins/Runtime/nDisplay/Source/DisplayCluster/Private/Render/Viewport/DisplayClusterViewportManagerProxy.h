// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Render/Viewport/IDisplayClusterViewportManagerProxy.h"
#include "Render/Viewport/DisplayClusterViewportProxy.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_Enums.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"
#include "Templates/SharedPointer.h"

class FDisplayClusterRenderTargetManager;
class FDisplayClusterViewportPostProcessManager;
class FDisplayClusterViewportManager;
class FDisplayClusterViewportManagerViewExtension;
class FDisplayClusterViewportResource;
class FDisplayClusterViewportLightCardManagerProxy;
class IDisplayClusterProjectionPolicy;
class FViewport;

/**
 * ViewportManagerProxy implementation.
 * This is a proxy object for the rendering thread. The data is copied every frame from the ViewportManager on the game thread.
 */
class FDisplayClusterViewportManagerProxy
	: public IDisplayClusterViewportManagerProxy
	, public TSharedFromThis<FDisplayClusterViewportManagerProxy, ESPMode::ThreadSafe>
{
public:
	FDisplayClusterViewportManagerProxy();
	virtual ~FDisplayClusterViewportManagerProxy();

public:
	//~IDisplayClusterViewportManagerProxy
	virtual TSharedPtr<IDisplayClusterViewportManagerProxy, ESPMode::ThreadSafe> ToSharedPtr() override
	{
		return AsShared();
	}

	virtual TSharedPtr<const IDisplayClusterViewportManagerProxy, ESPMode::ThreadSafe> ToSharedPtr() const override
	{
		return AsShared();
	}

	virtual EDisplayClusterRenderFrameMode GetRenderMode() const override;

	virtual IDisplayClusterViewportProxy* FindViewport_RenderThread(const FString& InViewportId) const override
	{
		return ImplFindViewportProxy_RenderThread(InViewportId);
	}

	virtual IDisplayClusterViewportProxy* FindViewport_RenderThread(const int32 StereoViewIndex, uint32* OutContextNum = nullptr) const override
	{
		return ImplFindViewportProxy_RenderThread(StereoViewIndex, OutContextNum);
	}

	virtual const TArrayView<TSharedPtr<IDisplayClusterViewportProxy, ESPMode::ThreadSafe>> GetViewports_RenderThread() const override
	{
		return TArrayView<TSharedPtr<IDisplayClusterViewportProxy, ESPMode::ThreadSafe>>((TSharedPtr<IDisplayClusterViewportProxy, ESPMode::ThreadSafe>*)(CurrentRenderFrameViewportProxies.GetData()), CurrentRenderFrameViewportProxies.Num());
	}

	virtual bool GetFrameTargets_RenderThread(TArray<FRHITexture2D*>& OutFrameResources, TArray<FIntPoint>& OutTargetOffsets, TArray<FRHITexture2D*>* OutAdditionalFrameResources = nullptr) const override;
	virtual bool ResolveFrameTargetToBackBuffer_RenderThread(FRHICommandListImmediate& RHICmdList, const uint32 InContextNum, const int32 DestArrayIndex, FRHITexture2D* DestTexture, FVector2D WindowSize) const override;
	//~~IDisplayClusterViewportManagerProxy

	/** Custom cross-GPU implementation for mGPU. Applyed for viewports with bOverrideCrossGPUTransfer=true. */
	void DoCrossGPUTransfers_RenderThread(FRHICommandListImmediate& RHICmdList) const;

	/** UpdateDeferredResources for all viewports (OCIO, PP, mips, etc). */
	void UpdateDeferredResources_RenderThread(FRHICommandListImmediate& RHICmdList) const;

	/** Apply WarpBlend and resolve to frame resources. */
	void UpdateFrameResources_RenderThread(FRHICommandListImmediate& RHICmdList, bool bWarpBlendEnabled) const;

	/** Release all referenced objects and resources. */
	void Release_RenderThread();

	/** Register new viewport proxy. */
	void CreateViewport_RenderThread(const TSharedPtr<FDisplayClusterViewportProxy, ESPMode::ThreadSafe>& InViewportProxy);

	/** Unregister exist viewport proxy. */
	void DeleteViewport_RenderThread(const TSharedPtr<FDisplayClusterViewportProxy, ESPMode::ThreadSafe>& InViewportProxy);

	/** Reset all references to InDeletedResourcePtr for all viewport proxies and delete the resource. */
	void DeleteResource_RenderThread(FDisplayClusterViewportResource* InDeletedResourcePtr);

	/** Initialize ViewportManagerProxy from ViewportManager. */
	void Initialize(FDisplayClusterViewportManager& InViewportManager);

	/** Get LightCardManager proxy object. */
	inline TSharedPtr<FDisplayClusterViewportLightCardManagerProxy, ESPMode::ThreadSafe> GetLightCardManagerProxy_RenderThread() const
	{ return LightCardManagerProxy; }

	/** Get RenderFrameSettings on render thread. */
	inline  const FDisplayClusterRenderFrameSettings& GetRenderFrameSettings_RenderThread() const
	{ return RenderFrameSettings; }

	/** Geting the viewport proxies of the current rendering frame. (viewports from the current node or from a special named list) */
	inline const TArray<TSharedPtr<FDisplayClusterViewportProxy, ESPMode::ThreadSafe>>& ImplGetCurrentRenderFrameViewportProxies_RenderThread() const
	{ return CurrentRenderFrameViewportProxies; }

	/** Get the viewport proxies  of the whole cluster on the render thread. */
	inline const TArray<TSharedPtr<FDisplayClusterViewportProxy, ESPMode::ThreadSafe>>& ImplGetEntireClusterViewportProxies_RenderThread() const
	{ return EntireClusterViewportProxies; }

	/** Find viewport by name. */
	FDisplayClusterViewportProxy* ImplFindViewportProxy_RenderThread(const FString& InViewportId) const;

	/** Find viewport and context index by StereoViewIndex. */
	FDisplayClusterViewportProxy* ImplFindViewportProxy_RenderThread(const int32 StereoViewIndex, uint32* OutContextNum = nullptr) const;

	/** Copy RenderFrameSettings from game to render thread. */
	void ImplUpdateRenderFrameSettings(const FDisplayClusterRenderFrameSettings& InRenderFrameSettings,
		const TSharedPtr<FDisplayClusterViewportManagerViewExtension, ESPMode::ThreadSafe>& InViewportManagerViewExtension);

	/** Sending viewport data from the game stream to the viewport proxy that is in the rendering stream. */
	void ImplUpdateViewportProxies_GameThread(const TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>>& InCurrentRenderFrameViewports);

	/** Rendering the final frame of nDisplay. Called after all viewports have been rendered in RTT. */
	void ImplRenderFrame_GameThread(FViewport* InViewport);

	/** Rendering black color in all frame textures. */
	void ImplClearFrameTargets_RenderThread(FRHICommandListImmediate& RHICmdList) const;

private:
	/** Update ClusterNodeViewportProxies from ViewportProxies for the current cluster node. */
	void ImplUpdateClusterNodeViewportProxies_RenderThread();

private:
	// RenderTarget manager object. Used to remove related objects in order.
	TSharedPtr<FDisplayClusterRenderTargetManager, ESPMode::ThreadSafe>        RenderTargetManager;

	// PostProcess manager object. Used to remove related objects in order.
	TSharedPtr<FDisplayClusterViewportPostProcessManager, ESPMode::ThreadSafe> PostProcessManager;

	// LightCard manager proxy object.
	TSharedPtr<FDisplayClusterViewportLightCardManagerProxy, ESPMode::ThreadSafe> LightCardManagerProxy;

	// Copy of RenderFrameSettings for the render thread.
	FDisplayClusterRenderFrameSettings RenderFrameSettings;

	// Viewport proxies of the entire cluster.
	TArray<TSharedPtr<FDisplayClusterViewportProxy, ESPMode::ThreadSafe>> EntireClusterViewportProxies;

	// Viewport proxies of the current rendering frame (viewports from the current node or from a special named list).
	TArray<TSharedPtr<FDisplayClusterViewportProxy, ESPMode::ThreadSafe>> CurrentRenderFrameViewportProxies;

	// nDisplay VE object. Used to remove related objects in order.
	TSharedPtr<class FDisplayClusterViewportManagerViewExtension, ESPMode::ThreadSafe> ViewportManagerViewExtension;
};
