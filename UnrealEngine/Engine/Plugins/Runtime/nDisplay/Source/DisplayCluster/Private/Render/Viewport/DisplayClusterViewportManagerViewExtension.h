// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SceneViewExtension.h"
#include "Render/Viewport/Containers/DisplayClusterViewportProxy_Context.h"

class IDisplayClusterViewportManager;
class IDisplayClusterViewportManagerProxy;

/**
 * View extension applying an DC Viewport features
 */
class FDisplayClusterViewportManagerViewExtension : public FSceneViewExtensionBase
{
public:
	FDisplayClusterViewportManagerViewExtension(const FAutoRegister& AutoRegister, const IDisplayClusterViewportManager* InViewportManager);
	virtual ~FDisplayClusterViewportManagerViewExtension();

public:
	//~ Begin ISceneViewExtension interface
	virtual int32 GetPriority() const override
	{
		return -1;
	}

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override
	{ }

	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override
	{ }

	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override
	{ }

	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;

	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	virtual void PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	//~End ISceneVIewExtension interface

private:
	/** Rendered callback (get scene textures to share) */
	void OnResolvedSceneColor_RenderThread(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures);

	void RegisterCallbacks();
	void UnregisterCallbacks();

private:
	FDelegateHandle ResolvedSceneColorCallbackHandle;

private:
	const IDisplayClusterViewportManager* ViewportManager;
	const IDisplayClusterViewportManagerProxy* ViewportManagerProxy;

	struct FViewportProxy
	{
		inline bool IsEnabled() const
		{
			return ViewportProxy != nullptr;
		}

		class IDisplayClusterViewportProxy* ViewportProxy = nullptr;
		FDisplayClusterViewportProxy_Context ViewportProxyContext;

		int32 ViewIndex = 0;
	};

	TArray<FViewportProxy> Viewports;
	
};
