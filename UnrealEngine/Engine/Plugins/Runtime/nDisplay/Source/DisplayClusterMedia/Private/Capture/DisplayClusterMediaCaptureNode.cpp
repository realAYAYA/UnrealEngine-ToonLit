// Copyright Epic Games, Inc. All Rights Reserved.

#include "Capture/DisplayClusterMediaCaptureNode.h"

#include "Config/IDisplayClusterConfigManager.h"
#include "DisplayClusterConfigurationTypes.h"
#include "DisplayClusterMediaLog.h"

#include "IDisplayCluster.h"
#include "IDisplayClusterCallbacks.h"

#include "RenderGraphBuilder.h"
#include "Render/Viewport/IDisplayClusterViewportManagerProxy.h"
#include "RHICommandList.h"
#include "RHIResources.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"


FDisplayClusterMediaCaptureNode::FDisplayClusterMediaCaptureNode(const FString& InMediaId, const FString& InClusterNodeId, UMediaOutput* InMediaOutput, UDisplayClusterMediaOutputSynchronizationPolicy* SyncPolicy)
	: FDisplayClusterMediaCaptureBase(InMediaId, InClusterNodeId, InMediaOutput, SyncPolicy)
{
}


bool FDisplayClusterMediaCaptureNode::StartCapture()
{
	// If capturing initialized and started successfully, subscribe for rendering callbacks
	if (FDisplayClusterMediaCaptureBase::StartCapture())
	{
		IDisplayCluster::Get().GetCallbacks().OnDisplayClusterPostBackbufferUpdated_RenderThread().AddRaw(this, &FDisplayClusterMediaCaptureNode::OnPostBackbufferUpdated_RenderThread);
		return true;
	}

	return false;
}

void FDisplayClusterMediaCaptureNode::StopCapture()
{
	// Stop rendering notifications
	IDisplayCluster::Get().GetCallbacks().OnDisplayClusterPostBackbufferUpdated_RenderThread().RemoveAll(this);
	// Stop capturing
	FDisplayClusterMediaCaptureBase::StopCapture();
}

FIntPoint FDisplayClusterMediaCaptureNode::GetCaptureSize() const
{
	// Return backbuffer runtime size
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		const FIntPoint Size = GEngine->GameViewport->Viewport->GetSizeXY();

		UE_LOG(LogDisplayClusterMedia, Log, TEXT("'%s' capture size is [%d, %d]"), *GetMediaId(), Size.X, Size.Y);

		return Size;
	}

	UE_LOG(LogDisplayClusterMedia, Warning, TEXT("'%s' couldn't get viewport size"), *GetMediaId());

	return FIntPoint();
}

void FDisplayClusterMediaCaptureNode::OnPostBackbufferUpdated_RenderThread(FRHICommandListImmediate& RHICmdList, FViewport* Viewport)
{
	if (Viewport)
	{
		if (FRHITexture* const BackbufferTexture = Viewport->GetRenderTargetTexture())
		{
			// Capture backbuffer
			FMediaTextureInfo TextureInfo{ BackbufferTexture, FIntRect(FIntPoint::ZeroValue, BackbufferTexture->GetDesc().Extent) };
			FRDGBuilder Builder(RHICmdList);
			ExportMediaData(Builder, TextureInfo);
			Builder.Execute();
		}
	}
}
