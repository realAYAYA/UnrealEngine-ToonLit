// Copyright Epic Games, Inc. All Rights Reserved.

#include "Capture/DisplayClusterMediaCaptureViewport.h"

#include "Config/IDisplayClusterConfigManager.h"
#include "DisplayClusterConfigurationTypes_Viewport.h"
#include "IDisplayCluster.h"
#include "IDisplayClusterCallbacks.h"

#include "Render/Viewport/IDisplayClusterViewportManagerProxy.h"
#include "RHICommandList.h"
#include "RHIResources.h"


FDisplayClusterMediaCaptureViewport::FDisplayClusterMediaCaptureViewport(const FString& InMediaId, const FString& InClusterNodeId, const FString& InViewportId, UMediaOutput* InMediaOutput)
	: FDisplayClusterMediaCaptureBase(InMediaId, InClusterNodeId, InMediaOutput)
	, ViewportId(InViewportId)
{
}


bool FDisplayClusterMediaCaptureViewport::StartCapture()
{
	// If capturing has started successfully, subscribe for rendering callbacks
	if (FDisplayClusterMediaCaptureBase::StartCapture())
	{
		IDisplayCluster::Get().GetCallbacks().OnDisplayClusterPostRenderViewFamily_RenderThread().AddRaw(this, &FDisplayClusterMediaCaptureViewport::OnPostRenderViewFamily_RenderThread);
		return true;
	}

	return false;
}

void FDisplayClusterMediaCaptureViewport::StopCapture()
{
	// Stop rendering notifications
	IDisplayCluster::Get().GetCallbacks().OnDisplayClusterPostRenderViewFamily_RenderThread().RemoveAll(this);
	// Stop capturing
	FDisplayClusterMediaCaptureBase::StopCapture();
}

FIntPoint FDisplayClusterMediaCaptureViewport::GetCaptureSize() const
{
	if (IDisplayCluster::Get().GetConfigMgr())
	{
		const UDisplayClusterConfigurationViewport* Viewport = IDisplayCluster::Get().GetConfigMgr()->GetLocalViewport(ViewportId);
		if (ensure(Viewport))
		{
			return { Viewport->Region.W, Viewport->Region.H };
		}
	}

	return FIntPoint();
}

void FDisplayClusterMediaCaptureViewport::OnPostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, const FSceneViewFamily& ViewFamily, const IDisplayClusterViewportProxy* ViewportProxy)
{
	ensure(ViewportProxy);

	if (ViewportProxy && ViewportProxy->GetId().Equals(GetViewportId(), ESearchCase::IgnoreCase))
	{
		TArray<FRHITexture*> Textures;
		TArray<FIntRect>     Regions;

		// Get RHI texture
		if (ViewportProxy->GetResourcesWithRects_RenderThread(EDisplayClusterViewportResourceType::InternalRenderTargetResource, Textures, Regions))
		{
			if (Textures.Num() > 0 && Regions.Num() > 0)
			{
				FMediaTextureInfo TextureInfo{ Textures[0], Regions[0] };
				ExportMediaData(GraphBuilder, TextureInfo);
			}
		}
	}
}
