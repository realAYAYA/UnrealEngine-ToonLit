// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SceneViewExtension.h"
#include "Render/Viewport/Containers/DisplayClusterViewportProxy_Context.h"

class FDisplayClusterViewportManager;
class FDisplayClusterViewportManagerProxy;
class FDisplayClusterViewportProxy;

#define DISPLAYCLUSTER_SCENE_VIEW_EXTENSION_PRIORITY -1

/**
 * View extension applying an DC Viewport features
 */
class FDisplayClusterViewportManagerViewExtension : public FSceneViewExtensionBase
{
public:
	FDisplayClusterViewportManagerViewExtension(const FAutoRegister& AutoRegister, const FDisplayClusterViewportManager* InViewportManager);
	virtual ~FDisplayClusterViewportManagerViewExtension();

public:
	//~ Begin ISceneViewExtension interface
	virtual int32 GetPriority() const override
	{
		return DISPLAYCLUSTER_SCENE_VIEW_EXTENSION_PRIORITY;
	}

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override
	{ }

	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override
	{ }

	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override
	{ }

	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;

	virtual void SubscribeToPostProcessingPass(EPostProcessingPass PassId, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;

	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	virtual void PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	//~End ISceneVIewExtension interface

	/** PP Helper: Get output PP texture from inputs (OverrideOutput or SceneColor).
	 *
	 * @param InOutInputs - PP material inputs
	 *
	 * @return - Screen pass texture
	 */
	static FScreenPassTexture ReturnUntouchedSceneColorForPostProcessing(const FPostProcessMaterialInputs& InOutInputs);

	/** Release from game thread.*/
	void Release();

	/** Release from render thread.*/
	void Release_RenderThread();

private:
	/** nDisplay VE Callback [subscribed to Renderer:ResolvedSceneColorCallbacks].
	 *
	 * @param GraphBuilder   - RDG interface
	 * @param SceneTextures  - Scene textures (SceneColor, Depth, etc)
	 *
	 * @return - none
	 */
	void OnResolvedSceneColor_RenderThread(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures);

	/** Callback OnPostProcessPassAfterFXAA.
	 *
	 * @param GraphBuilder - RDG interface
	 * @param View         - Scene View
	 * @param Inputs       - PP Input resources
	 *
	 * @return - Screen pass texture
	 */
	FScreenPassTexture PostProcessPassAfterFXAA_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs);

	/** Callback OnPostProcessPassAfterSSRInput.
	 *
	 * @param GraphBuilder - RDG interface
	 * @param View         - Scene View
	 * @param Inputs       - PP Input resources
	 *
	 * @return - Screen pass texture
	 */
	FScreenPassTexture PostProcessPassAfterSSRInput_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs);

	/** Callback OnPostProcessPassAfterTonemap.
	 *
	 * @param GraphBuilder - RDG interface
	 * @param View         - Scene View
	 * @param Inputs       - PP Input resources
	 *
	 * @return - Screen pass texture
	 */
	FScreenPassTexture PostProcessPassAfterTonemap_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs);

	/** Register callbacks to external modules (Renderer:ResolvedSceneColor). */
	void RegisterCallbacks();

	/** UnRegister callbacks to external modules (Renderer:ResolvedSceneColor). */
	void UnregisterCallbacks();

	/** Return true if the VE is safe to use. */
	bool IsActive() const;

private:
	FDelegateHandle ResolvedSceneColorCallbackHandle;

private:
	const FDisplayClusterViewportManager* ViewportManager;
	TWeakPtr<FDisplayClusterViewportManagerProxy, ESPMode::ThreadSafe> ViewportManagerProxy;

	struct FViewportProxy
	{
		// The ViewportProxy must exist until this object is removed.
		TWeakPtr<FDisplayClusterViewportProxy, ESPMode::ThreadSafe> ViewportProxy;

		// The index of view in viewfamily collection
		int32 ViewIndex = 0;

		// Context for viewport proxy object
		FDisplayClusterViewportProxy_Context ViewportProxyContext;
	};

	/** RenderThreadData:. Viewport proxies from rendered view family. */
	TArray<FViewportProxy> ViewportProxies;
};
