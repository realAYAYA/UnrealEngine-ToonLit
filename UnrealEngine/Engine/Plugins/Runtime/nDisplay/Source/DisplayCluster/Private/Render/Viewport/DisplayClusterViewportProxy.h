// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "EngineUtils.h"
#include "ScreenRendering.h"
#include "SceneView.h"
#include "Templates/SharedPointer.h"

#include "Render/Viewport/IDisplayClusterViewportProxy.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_OverscanSettings.h"

class FDisplayClusterViewportRenderTargetResource;
class FDisplayClusterViewportTextureResource;
class FDisplayClusterViewportProxyData;
class IDisplayClusterViewportManagerProxy;
class FDisplayClusterViewportManagerProxy;
class FDisplayClusterViewport;
class IDisplayClusterShaders;
class IDisplayClusterProjectionPolicy;
class IDisplayClusterRender_MeshComponent;
class FDisplayClusterViewportReadPixelsData;

class FDisplayClusterViewportProxy
	: public IDisplayClusterViewportProxy
	, public TSharedFromThis<FDisplayClusterViewportProxy, ESPMode::ThreadSafe>
{
public:
	FDisplayClusterViewportProxy(const FDisplayClusterViewport& RenderViewport);
	virtual ~FDisplayClusterViewportProxy();

public:
	///////////////////////////////
	// IDisplayClusterViewportProxy
	///////////////////////////////
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

	// Apply postprocess, generate mips, etc from settings in FDisplayClusterViewporDeferredUpdateSettings
	void UpdateDeferredResources(FRHICommandListImmediate& RHICmdList) const;

	//  Return viewport scene proxy resources by type
	virtual bool GetResources_RenderThread(const EDisplayClusterViewportResourceType InResourceType, TArray<FRHITexture2D*>& OutResources) const override;
	virtual bool GetResourcesWithRects_RenderThread(const EDisplayClusterViewportResourceType InResourceType, TArray<FRHITexture2D*>& OutResources, TArray<FIntRect>& OutRects) const override;

	// Resolve resource contexts
	virtual bool ResolveResources_RenderThread(FRHICommandListImmediate& RHICmdList, const EDisplayClusterViewportResourceType InputResourceType, const EDisplayClusterViewportResourceType OutputResourceType, const int32 InContextNum = INDEX_NONE) const override;

	virtual EDisplayClusterViewportResourceType GetOutputResourceType_RenderThread() const override;

	virtual const IDisplayClusterViewportManagerProxy& GetOwner_RenderThread() const override;

	virtual void OnResolvedSceneColor_RenderThread(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures, const FDisplayClusterViewportProxy_Context& InProxyContext) override;
	virtual void PostRenderViewFamily_RenderThread(FRDGBuilder& InGraphBuilder, class FSceneViewFamily& InViewFamily, const class FSceneView& InSceneView, const FDisplayClusterViewportProxy_Context& InProxyContext) override;

	///////////////////////////////
	// ~IDisplayClusterViewportProxy
	///////////////////////////////

	// Release internal resource refs
	void HandleResourceDelete_RenderThread(class FDisplayClusterViewportResource* InDeletedResourcePtr);

	void PostResolveViewport_RenderThread(FRHICommandListImmediate& RHICmdList) const;

#if WITH_EDITOR
	bool GetPreviewPixels_GameThread(TSharedPtr<FDisplayClusterViewportReadPixelsData, ESPMode::ThreadSafe>& OutPixelsData) const;
#endif

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

	FIntRect GetFinalContextRect(const EDisplayClusterViewportResourceType InputResourceType, const FIntRect& InRect) const;

private:
	bool ImplGetResourcesWithRects_RenderThread(const EDisplayClusterViewportResourceType InResourceType, TArray<FRHITexture2D*>& OutResources, TArray<FIntRect>& OutResourceRects, const int32 InRecursionDepth) const;
	bool ImplGetResources_RenderThread(const EDisplayClusterViewportResourceType InResourceType, TArray<FRHITexture2D*>& OutResources, const int32 InRecursionDepth) const;

	void ImplViewportRemap_RenderThread(FRHICommandListImmediate& RHICmdList) const;
	void ImplPreviewReadPixels_RenderThread(FRHICommandListImmediate& RHICmdList) const;

	bool ImplResolveResources_RenderThread(FRHICommandListImmediate& RHICmdList, FDisplayClusterViewportProxy const* SourceProxy, const EDisplayClusterViewportResourceType InputResourceType, const EDisplayClusterViewportResourceType OutputResourceType, const int32 InContextNum) const;
	bool IsShouldOverrideViewportResource(const EDisplayClusterViewportResourceType InResourceType) const;

protected:
	friend FDisplayClusterViewportProxyData;
	friend FDisplayClusterViewportManagerProxy;

	// Unique viewport name
	const FString ViewportId;

	const FString ClusterNodeId;

	// Viewport render params
	mutable FDisplayClusterViewport_RenderSettings       RenderSettings;
	FDisplayClusterViewport_RenderSettingsICVFX  RenderSettingsICVFX;
	FDisplayClusterViewport_PostRenderSettings   PostRenderSettings;

	// Additional parameters
	FDisplayClusterViewport_OverscanSettings     OverscanSettings;

	TSharedPtr<IDisplayClusterRender_MeshComponent, ESPMode::ThreadSafe> RemapMesh;

	// Projection policy instance that serves this viewport
	TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe> ProjectionPolicy;

	// Viewport contexts (left/center/right eyes)
	mutable TArray<FDisplayClusterViewport_Context> Contexts;

	/** The GPU nodes on which to render this view. */
	TArray<FRHIGPUMask> GPUMask;

	// View family render to this resources
	TArray<FDisplayClusterViewportRenderTargetResource*> RenderTargets;

	// Projection policy output resources
	TArray<FDisplayClusterViewportTextureResource*> OutputFrameTargetableResources;
	TArray<FDisplayClusterViewportTextureResource*> AdditionalFrameTargetableResources;

#if WITH_EDITOR
	FTextureRHIRef OutputPreviewTargetableResource;
	
	mutable bool bPreviewReadPixels = false;
	mutable FCriticalSection PreviewPixelsCSGuard;
	mutable TSharedPtr<FDisplayClusterViewportReadPixelsData, ESPMode::ThreadSafe> PreviewPixels;
#endif

	// unique viewport resources
	TArray<FDisplayClusterViewportTextureResource*> InputShaderResources;
	TArray<FDisplayClusterViewportTextureResource*> AdditionalTargetableResources;
	TArray<FDisplayClusterViewportTextureResource*> MipsShaderResources;

	const TSharedRef<FDisplayClusterViewportManagerProxy, ESPMode::ThreadSafe> Owner;
	IDisplayClusterShaders& ShadersAPI;
};

