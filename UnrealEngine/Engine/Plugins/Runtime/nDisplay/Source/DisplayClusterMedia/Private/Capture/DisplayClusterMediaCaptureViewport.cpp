// Copyright Epic Games, Inc. All Rights Reserved.

#include "Capture/DisplayClusterMediaCaptureViewport.h"

#include "IDisplayCluster.h"
#include "IDisplayClusterCallbacks.h"

#include "DisplayClusterConfigurationTypes_Viewport.h"
#include "DisplayClusterMediaLog.h"
#include "DisplayClusterRootActor.h"

#include "Config/IDisplayClusterConfigManager.h"
#include "Game/IDisplayClusterGameManager.h"

#include "Render/IDisplayClusterRenderManager.h"
#include "Render/Viewport/IDisplayClusterViewport.h"
#include "Render/Viewport/IDisplayClusterViewportManager.h"
#include "Render/Viewport/IDisplayClusterViewportManagerProxy.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_Context.h"
#include "Render/Viewport/Containers/DisplayClusterViewport_RenderSettings.h"
#include "RHICommandList.h"
#include "RHIResources.h"


FDisplayClusterMediaCaptureViewport::FDisplayClusterMediaCaptureViewport(const FString& InMediaId, const FString& InClusterNodeId, const FString& InViewportId, UMediaOutput* InMediaOutput, UDisplayClusterMediaOutputSynchronizationPolicy* SyncPolicy)
	: FDisplayClusterMediaCaptureBase(InMediaId, InClusterNodeId, InMediaOutput, SyncPolicy)
	, ViewportId(InViewportId)
{
}


bool FDisplayClusterMediaCaptureViewport::StartCapture()
{
	// Subscribe for events
	IDisplayCluster::Get().GetCallbacks().OnDisplayClusterPostRenderViewFamily_RenderThread().AddRaw(this, &FDisplayClusterMediaCaptureViewport::OnPostRenderViewFamily_RenderThread);
	IDisplayCluster::Get().GetCallbacks().OnDisplayClusterUpdateViewportMediaState().AddRaw(this, &FDisplayClusterMediaCaptureViewport::OnUpdateViewportMediaState);

	// Start capture
	const bool bStarted = FDisplayClusterMediaCaptureBase::StartCapture();

	return bStarted;
}

void FDisplayClusterMediaCaptureViewport::StopCapture()
{
	// Unsubscribe from events
	IDisplayCluster::Get().GetCallbacks().OnDisplayClusterPostRenderViewFamily_RenderThread().RemoveAll(this);
	IDisplayCluster::Get().GetCallbacks().OnDisplayClusterUpdateViewportMediaState().RemoveAll(this);

	// Stop capturing
	FDisplayClusterMediaCaptureBase::StopCapture();
}

void FDisplayClusterMediaCaptureViewport::OnUpdateViewportMediaState(IDisplayClusterViewport* InViewport, EDisplayClusterViewportMediaState& InOutMediaState)
{
	// Note: Media currently supports only one DCRA.
	// In the future, after the media redesign, the DCRA name will also need to be checked here.
	if (InViewport && InViewport->GetId().Equals(GetViewportId(), ESearchCase::IgnoreCase))
	{
		// Raise flags that this viewport will be captured by media.
		InOutMediaState |= EDisplayClusterViewportMediaState::Capture;

		if (bForceLateOCIOPass)
		{
			// Raise flags that this capture requires ForceLateOCIOPass.
			InOutMediaState |= EDisplayClusterViewportMediaState::Capture_ForceLateOCIOPass;
		}
	}
}

FIntPoint FDisplayClusterMediaCaptureViewport::GetCaptureSize() const
{
	FIntPoint CaptureSize{ FIntPoint::ZeroValue };

	if (GetCaptureSizeFromGameProxy(CaptureSize))
	{
		UE_LOG(LogDisplayClusterMedia, Verbose, TEXT("'%s' acquired capture size from game proxy [%d, %d]"), *GetMediaId(), CaptureSize.X, CaptureSize.Y);
	}
	else if (GetCaptureSizeFromConfig(CaptureSize))
	{
		UE_LOG(LogDisplayClusterMedia, Verbose, TEXT("'%s' acquired capture size from config [%d, %d]"), *GetMediaId(), CaptureSize.X, CaptureSize.Y);
	}
	else
	{
		UE_LOG(LogDisplayClusterMedia, Verbose, TEXT("'%s' couldn't acquire capture"), *GetMediaId());
	}

	return CaptureSize;
}

bool FDisplayClusterMediaCaptureViewport::GetCaptureSizeFromConfig(FIntPoint& OutSize) const
{
	if (const ADisplayClusterRootActor* const ActiveRootActor = IDisplayCluster::Get().GetGameMgr()->GetRootActor())
	{
		if (const UDisplayClusterConfigurationData* const ConfigData = ActiveRootActor->GetConfigData())
		{
			const FString& NodeId = GetClusterNodeId();
			if (const UDisplayClusterConfigurationViewport* const ViewportCfg = ConfigData->GetViewport(NodeId, ViewportId))
			{
				const FIntRect ViewportRect = ViewportCfg->Region.ToRect();
				OutSize = FIntPoint(ViewportRect.Width(), ViewportRect.Height());

				return true;
			}
		}
	}

	return false;
}

bool FDisplayClusterMediaCaptureViewport::GetCaptureSizeFromGameProxy(FIntPoint& OutSize) const
{
	// We need to get actual texture size for the viewport
	if (const IDisplayClusterRenderManager* const RenderMgr = IDisplayCluster::Get().GetRenderMgr())
	{
		if (const IDisplayClusterViewportManager* const ViewportMgr = RenderMgr->GetViewportManager())
		{
			if (const IDisplayClusterViewport* const Viewport = ViewportMgr->FindViewport(ViewportId))
			{
				const TArray<FDisplayClusterViewport_Context>& Contexts = Viewport->GetContexts();
				if (Contexts.Num() > 0)
				{
					OutSize = Contexts[0].RenderTargetRect.Size();
					return true;
				}
			}
		}
	}

	return false;
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
