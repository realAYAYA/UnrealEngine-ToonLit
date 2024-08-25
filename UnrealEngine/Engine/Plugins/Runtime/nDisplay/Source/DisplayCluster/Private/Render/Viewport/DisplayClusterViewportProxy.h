// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Render/Viewport/IDisplayClusterViewportProxy.h"
#include "Render/Viewport/DisplayClusterViewportResources.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_OverscanRuntimeSettings.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationProxy.h"

#include "Render/DisplayDevice/IDisplayClusterDisplayDeviceProxy.h"

#include "PostProcess/PostProcessAA.h"

#include "EngineUtils.h"
#include "ScreenRendering.h"
#include "SceneView.h"
#include "Templates/SharedPointer.h"

class FDisplayClusterViewport;
class IDisplayClusterProjectionPolicy;
class IDisplayClusterRender_MeshComponent;
class FDisplayClusterViewport_OpenColorIO;

class FRDGBuilder;
class FSceneView;
struct FPostProcessMaterialInputs;
struct FScreenPassTexture;
struct FSceneTextures;

/**
 * Copy mode for texture channels.
 */
enum class EDisplayClusterTextureCopyMode : uint8
{
	RGBA = 0,
	RGB,
	Alpha,
};

/**
 * OCIO is applied in different ways. It depends on the rendering workflow
 */
enum class EDisplayClusterViewportOpenColorIOMode : uint8
{
	None = 0,

	// When the viewport renders with a postprocess, OCIO must be done in between
	PostProcess,

	// When the viewport is rendered without postprocessing, OCIO is applied last, to the RTT texture of the viewport
	Resolved
};

/**
 * nDisplay viewport proxy implementation.
 */
class FDisplayClusterViewportProxy
	: public IDisplayClusterViewportProxy
	, public TSharedFromThis<FDisplayClusterViewportProxy, ESPMode::ThreadSafe>
{
public:
	FDisplayClusterViewportProxy(const TSharedRef<FDisplayClusterViewportConfiguration, ESPMode::ThreadSafe>& InConfiguration, const FString& InViewportId, const TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& InProjectionPolicy);
	virtual ~FDisplayClusterViewportProxy();

public:
	//~~ BEGIN IDisplayClusterViewportProxy
	virtual TSharedPtr<IDisplayClusterViewportProxy, ESPMode::ThreadSafe> ToSharedPtr() override
	{
		return AsShared();
	}

	virtual TSharedPtr<const IDisplayClusterViewportProxy, ESPMode::ThreadSafe> ToSharedPtr() const override
	{
		return AsShared();
	}

	virtual const IDisplayClusterViewportConfigurationProxy& GetConfigurationProxy() const override
	{
		return *ConfigurationProxy;
	}

	virtual FString GetId() const override
	{
		check(IsInRenderingThread());
		return ViewportId;
	}

	virtual FString GetClusterNodeId() const override
	{
		check(IsInRenderingThread());
		return ClusterNodeId;
	}

	virtual const FDisplayClusterViewport_RenderSettings& GetRenderSettings_RenderThread() const override
	{
		check(IsInRenderingThread());
		return RenderSettings;
	}

	virtual const FDisplayClusterViewport_RenderSettingsICVFX& GetRenderSettingsICVFX_RenderThread() const override
	{
		check(IsInRenderingThread());
		return RenderSettingsICVFX;
	}

	virtual const FDisplayClusterViewport_PostRenderSettings& GetPostRenderSettings_RenderThread() const override
	{
		check(IsInRenderingThread());
		return PostRenderSettings;
	}

	virtual const TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& GetProjectionPolicy_RenderThread() const override
	{
		check(IsInRenderingThread());
		return ProjectionPolicy;
	}
	
	virtual const TArray<FDisplayClusterViewport_Context>& GetContexts_RenderThread() const override
	{
		check(IsInRenderingThread());
		return Contexts;
	}

	virtual void SetRenderSettings_RenderThread(const FDisplayClusterViewport_RenderSettings& InRenderSettings) const override
	{
		check(IsInRenderingThread());
		RenderSettings = InRenderSettings;
	}

	virtual void SetContexts_RenderThread(const TArray<FDisplayClusterViewport_Context>& InContexts) const override
	{
		check(IsInRenderingThread());
		Contexts.Empty();
		Contexts.Append(InContexts);
	}

	//  Return viewport scene proxy resources by type
	virtual bool GetResources_RenderThread(const EDisplayClusterViewportResourceType InResourceType, TArray<FRHITexture2D*>& OutResources) const override;
	virtual bool GetResourcesWithRects_RenderThread(const EDisplayClusterViewportResourceType InResourceType, TArray<FRHITexture2D*>& OutResources, TArray<FIntRect>& OutRects) const override;

	// Resolve resource contexts
	virtual bool ResolveResources_RenderThread(FRHICommandListImmediate& RHICmdList, const EDisplayClusterViewportResourceType InputResourceType, const EDisplayClusterViewportResourceType OutputResourceType, const int32 InContextNum = INDEX_NONE) const override;
	virtual bool ResolveResources_RenderThread(FRHICommandListImmediate& RHICmdList, IDisplayClusterViewportProxy* InputResourceViewportProxy, const EDisplayClusterViewportResourceType InputResourceType, const EDisplayClusterViewportResourceType OutputResourceType, const int32 InContextNum = INDEX_NONE) const override;

	//~~ END IDisplayClusterViewportProxy

public:
	/**
	* Fill the entire texture with the specified color.
	* 
	* @param InRenderTargetTexture - texture,  must be RTT.
	* @param InColor - the texture will be filled with this color.
	*/
	static void FillTextureWithColor_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* InRenderTargetTexture, const FLinearColor& InColor);

	/** Get valid resource type
	 * 
	 * @param InResourceType - the requested resource type from the entire namespace
	 * 
	 * @return - the type of resource actually used, depending on the current configuration of the rendering pipeline
	 */
	EDisplayClusterViewportResourceType GetResourceType_RenderThread(const EDisplayClusterViewportResourceType& InResourceType) const;

	/** Get actual region for viewport context
	 * 
	 * @param InResourceType - the requested resource type from the entire namespace
	 * @param InRect - viewport context region
	 * 
	 * @return - InRect with applied overscan for RTT
	 */
	FIntRect GetFinalContextRect(const EDisplayClusterViewportResourceType InResourceType, const FIntRect& InRect) const;

	/** Resolve viewport RTT: render OCIO, PP, generate MIPS, etc.
	 *
	 * @param RHICmdList - RHI interface
	 *
	 * @return - none
	 */
	void UpdateDeferredResources(FRHICommandListImmediate& RHICmdList) const;

	/** Called at the end of the frame, after all callbacks.
	* At the end, some resources may be filled with black, etc.
	* This is useful because the resources are reused and the image from the previous frame goes into the new one.
	*/
	void CleanupResources_RenderThread(FRHICommandListImmediate& RHICmdList) const;

	/** nDisplay VE Callback [subscribed to Renderer:ResolvedSceneColorCallbacks].
	 *
	 * @param GraphBuilder   - RDG interface
	 * @param SceneTextures  - Scene textures (SceneColor, Depth, etc)
	 * @param InProxyContext - Saved from game thread context for this proxy
	 *
	 * @return - none
	 */
	void OnResolvedSceneColor_RenderThread(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures, const FDisplayClusterViewportProxy_Context& InProxyContext);

	/** nDisplay VE Callback [PostRenderViewFamily_RenderThread()].
	 *
	 * @param GraphBuilder   - RDG interface
	 * @param InViewFamily   - Scene View Family
	 * @param InSceneView    - Scene View
	 * @param InProxyContext - Saved from game thread context for this proxy
	 *
	 * @return - none
	 */
	void OnPostRenderViewFamily_RenderThread(FRDGBuilder& InGraphBuilder, FSceneViewFamily& InViewFamily, const FSceneView& InSceneView, const FDisplayClusterViewportProxy_Context& InProxyContext);

	/** Allow callback OnPostProcessPassAfterFXAA.  */
	bool ShouldUsePostProcessPassAfterFXAA() const;

	/** Callback OnPostProcessPassAfterFXAA.
	 *
	 * @param GraphBuilder - RDG interface
	 * @param View         - Scene View
	 * @param Inputs       - PP Input resources
	 * @param ContextNum   - viewport context index
	 *
	 * @return - Screen pass texture
	 */
	FScreenPassTexture OnPostProcessPassAfterFXAA_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs, const uint32 ContextNum);

	/** Allow callback OnPostProcessPassAfterSSRInput. */
	bool ShouldUsePostProcessPassAfterSSRInput() const;

	/** Callback OnPostProcessPassAfterSSRInput.
	 *
	 * @param GraphBuilder - RDG interface
	 * @param View         - Scene View
	 * @param Inputs       - PP Input resources
	 * @param ContextNum   - viewport context index
	 *
	 * @return - Screen pass texture
	 */
	FScreenPassTexture OnPostProcessPassAfterSSRInput_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs, const uint32 ContextNum);

	/** Allow callback OnPostProcessPassAfterTonemap. */
	bool ShouldUsePostProcessPassTonemap() const;

	/** Callback OnPostProcessPassAfterTonemap.
	 *
	 * @param GraphBuilder - RDG interface
	 * @param View         - Scene View
	 * @param Inputs       - PP Input resources
	 * @param ContextNum   - viewport context index
	 *
	 * @return - Screen pass texture
	 */
	FScreenPassTexture OnPostProcessPassAfterTonemap_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs, const uint32 ContextNum);

	/** Enable alpha channel for this viewport (useful for overlays with alpha channel: ChromaKey, LightCard). */
	bool ShouldUseAlphaChannel_RenderThread() const;

	/** Finally, resolve the viewport to the output RTT and apply the last PPs (ViewportRemap, etc.)
	 *
	 * @param RHICmdList - RHI interface
	 *
	 * @return - none
	 */
	void PostResolveViewport_RenderThread(FRHICommandListImmediate& RHICmdList) const;

	/** Release all textures. */
	void ReleaseTextures_RenderThread();

	inline bool FindContext_RenderThread(const int32 ViewIndex, uint32* OutContextNum)
	{
		check(IsInRenderingThread());

		for (int32 ContextNum = 0; ContextNum < Contexts.Num(); ContextNum++)
		{
			if (ViewIndex == Contexts[ContextNum].StereoViewIndex)
			{
				if (OutContextNum != nullptr)
				{
					*OutContextNum = ContextNum;
				}

				return true;
			}
		}

		return false;
	}

public:
	/* Returns true if the warp can be applied to this viewport. */
	bool ShouldApplyWarpBlend_RenderThread() const;

	const FDisplayClusterViewportResources& GetResources_RenderThread() const
	{
		return Resources;
	}

	void UpdateViewportProxyData_RenderThread(const class FDisplayClusterViewportProxyData& InViewportProxyData);

	/** Viewports should be processed in the appropriate order.
	* Viewports with lower priority values will be processed earlier.
	*/
	uint8 GetPriority_RenderThread() const;

private:
	bool ImplGetResourcesWithRects_RenderThread(const EDisplayClusterViewportResourceType InResourceType, TArray<FRHITexture2D*>& OutResources, TArray<FIntRect>& OutResourceRects, const int32 InRecursionDepth) const;
	bool ImplGetResources_RenderThread(const EDisplayClusterViewportResourceType InResourceType, TArray<FRHITexture2D*>& OutResources, const int32 InRecursionDepth) const;

	void ImplViewportRemap_RenderThread(FRHICommandListImmediate& RHICmdList) const;

	bool ImplResolveResources_RenderThread(FRHICommandListImmediate& RHICmdList, FDisplayClusterViewportProxy const* SourceProxy, const EDisplayClusterViewportResourceType InputResourceType, const EDisplayClusterViewportResourceType OutputResourceType, const int32 InContextNum = INDEX_NONE) const;

	/** Copy the tile to the target viewport. */
	void ImplResolveTileResource_RenderThread(FRHICommandListImmediate& RHICmdList, FDisplayClusterViewportProxy const* InDestViewportProxy) const;

	/** Returns true if this viewport requires FXAA to be applied.
	* 
	* @param OutFXAAQuality - (out) FXAA shader parameter that define quality level.
	*/
	bool ShouldApplyFXAA_RenderThread(EFXAAQuality& OutFXAAQuality) const;

	/** When a resource by type can be overridden from another viewport, true is returned. */
	bool ShouldOverrideViewportResource(const EDisplayClusterViewportResourceType InResourceType) const;

	/** Copy pixel channels between resources.
	 *
	 * @param GraphBuilder       - RDG interface
	 * @param InCopyMode         - channels copy mode
	 * @param InContextNum       - viewport context
	 * @param InSrcTextureRef    - Input resource
	 * @param InSrcRect          - Input resource rect
	 * @param InDestResourceType - Dest viewport resource type
	 *
	 * @return - true, if success
	 */
	bool CopyResource_RenderThread(FRDGBuilder& GraphBuilder, const EDisplayClusterTextureCopyMode InCopyMode, const int32 InContextNum, FRDGTextureRef InSrcTextureRef, const FIntRect& InSrcRect, const EDisplayClusterViewportResourceType InDestResourceType);

	/** Copy pixel channels between resources.
	 *
	 * @param GraphBuilder      - RDG interface
	 * @param InCopyMode        - channels copy mode
	 * @param InContextNum      - viewport context
	 * @param InDestTextureRef  - Dest resource
	 * @param InDestRect        - Dest resource rect
	 *
	 * @return - true, if success
	 */
	bool CopyResource_RenderThread(FRDGBuilder& GraphBuilder, const EDisplayClusterTextureCopyMode InCopyMode, const int32 InContextNum, const EDisplayClusterViewportResourceType InSrcResourceType, FRDGTextureRef InDestTextureRef, const FIntRect& InDestRect);

	/** Copy pixel channels between resources.
	 *
	 * @param GraphBuilder       - RDG interface
	 * @param InCopyMode         - channels copy mode
	 * @param InContextNum       - viewport context
	 * @param InSrcResourceType  - Input viewport resource type
	 * @param InDestResourceType - Dest viewport resource type
	 *
	 * @return - true, if success
	 */
	bool CopyResource_RenderThread(FRDGBuilder& GraphBuilder, const EDisplayClusterTextureCopyMode InCopyMode, const int32 InContextNum, const EDisplayClusterViewportResourceType InSrcResourceType, const EDisplayClusterViewportResourceType InDestResourceType);

	/** Return viewport used to render RTT (support ViewportOverride)
	 *
	 * @return - viewport proxy with valid render resources
	 */
	const FDisplayClusterViewportProxy& GetRenderingViewportProxy() const;

	/** Apply OCIO to the image from the 'InSrcResourceType' and put the result image to the EDisplayClusterViewportResourceType::InputShaderResource. */
	bool ApplyOCIO_RenderThread(FRHICommandListImmediate& RHICmdList, const FDisplayClusterViewportProxy& InSrcViewportProxy, const EDisplayClusterViewportResourceType InSrcResourceType) const;

	/** Check if there is an RTT source (internal or external) in this viewport proxy. */
	bool IsInputRenderTargetResourceExists() const;

	/** Returns the OCIO rendering type for the given viewport. */
	EDisplayClusterViewportOpenColorIOMode GetOpenColorIOMode() const;

public:
	// Configuration for proxy
	const TSharedRef<FDisplayClusterViewportConfigurationProxy, ESPMode::ThreadSafe> ConfigurationProxy;

	/** Unique viewport name. */
	const FString ViewportId;

	/** Cluster node name. */
	const FString ClusterNodeId;

protected:
	/** OpenColorIO nDisplay interface ref. */
	TSharedPtr<FDisplayClusterViewport_OpenColorIO, ESPMode::ThreadSafe> OpenColorIO;

	/** Display Device Proxy. */
	TSharedPtr<IDisplayClusterDisplayDeviceProxy, ESPMode::ThreadSafe> DisplayDeviceProxy;

	// Viewport render params
	mutable FDisplayClusterViewport_RenderSettings RenderSettings;

	FDisplayClusterViewport_RenderSettingsICVFX  RenderSettingsICVFX;
	FDisplayClusterViewport_PostRenderSettings   PostRenderSettings;

	// Additional parameters
	FDisplayClusterViewport_OverscanRuntimeSettings OverscanRuntimeSettings;

	TSharedPtr<IDisplayClusterRender_MeshComponent, ESPMode::ThreadSafe> RemapMesh;

	// Projection policy instance that serves this viewport
	TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> ProjectionPolicy;

	// Viewport contexts (left/center/right eyes)
	mutable TArray<FDisplayClusterViewport_Context> Contexts;

	// Unified repository of viewport resources
	FDisplayClusterViewportResources Resources;

	// Used ViewStates
	TArray<TSharedPtr<FSceneViewStateReference, ESPMode::ThreadSafe>> ViewStates;
};
