// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/DisplayClusterMediaInputViewport.h"
#include "Render/Viewport/IDisplayClusterViewportManagerProxy.h"
#include "Render/Viewport/IDisplayClusterViewport.h"

#include "IDisplayCluster.h"
#include "IDisplayClusterCallbacks.h"

#include "MediaTexture.h"

#include "RHICommandList.h"
#include "RHIResources.h"


FDisplayClusterMediaInputViewport::FDisplayClusterMediaInputViewport(const FString& InMediaId, const FString& InClusterNodeId, const FString& InViewportId, UMediaSource* InMediaSource)
	: FDisplayClusterMediaInputBase(InMediaId, InClusterNodeId, InMediaSource)
	, ViewportId(InViewportId)
{
}


bool FDisplayClusterMediaInputViewport::Play()
{
	// If playback has started successfully, subscribe for rendering callbacks
	if (FDisplayClusterMediaInputBase::Play())
	{
		IDisplayCluster::Get().GetCallbacks().OnDisplayClusterPostCrossGpuTransfer_RenderThread().AddRaw(this, &FDisplayClusterMediaInputViewport::PostCrossGpuTransfer_RenderThread);
		IDisplayCluster::Get().GetCallbacks().OnDisplayClusterUpdateViewportMediaState().AddRaw(this, &FDisplayClusterMediaInputViewport::OnUpdateViewportMediaState);

		return true;
	}

	return false;
}

void FDisplayClusterMediaInputViewport::Stop()
{
	// Stop receiving notifications
	IDisplayCluster::Get().GetCallbacks().OnDisplayClusterPostCrossGpuTransfer_RenderThread().RemoveAll(this);

	// Stop raising media flags for the viewport.
	IDisplayCluster::Get().GetCallbacks().OnDisplayClusterUpdateViewportMediaState().RemoveAll(this);

	// Stop playing
	FDisplayClusterMediaInputBase::Stop();
}

void FDisplayClusterMediaInputViewport::OnUpdateViewportMediaState(IDisplayClusterViewport* InViewport, EDisplayClusterViewportMediaState& InOutMediaState)
{
	// Note: Media currently supports only one DCRA.
	// In the future, after the media redesign, the DCRA name will also need to be checked here.
	if (InViewport && InViewport->GetId().Equals(GetViewportId(), ESearchCase::IgnoreCase))
	{
		// Raise flags that this viewport texture will be overridden by media.
		InOutMediaState |= EDisplayClusterViewportMediaState::Input;

		if (bForceLateOCIOPass)
		{
			// Raise flags that this viewport requires ForceLateOCIOPass.
			InOutMediaState |= EDisplayClusterViewportMediaState::Input_ForceLateOCIOPass;
		}
	}
}

void FDisplayClusterMediaInputViewport::PostCrossGpuTransfer_RenderThread(FRHICommandListImmediate& RHICmdList, const IDisplayClusterViewportManagerProxy* ViewportManagerProxy, FViewport* Viewport)
{
	checkSlow(ViewportManagerProxy);

	if (const IDisplayClusterViewportProxy* const PlaybackViewport = ViewportManagerProxy->FindViewport_RenderThread(GetViewportId()))
	{
		const bool bShouldImportMedia = !PlaybackViewport->GetPostRenderSettings_RenderThread().Replace.IsEnabled();

		if (bShouldImportMedia)
		{
			TArray<FRHITexture*> Textures;
			TArray<FIntRect>     Regions;

			if (PlaybackViewport->GetResourcesWithRects_RenderThread(EDisplayClusterViewportResourceType::InternalRenderTargetResource, Textures, Regions))
			{
				if (Textures.Num() > 0 && Regions.Num() > 0)
				{
					FMediaTextureInfo TextureInfo{ Textures[0], Regions[0] };
					ImportMediaData(RHICmdList, TextureInfo);
				}
			}
		}
	}
}
