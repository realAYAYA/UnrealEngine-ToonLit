// Copyright Epic Games, Inc. All Rights Reserved.

#include "Capture/DisplayClusterMediaCaptureNode.h"

#include "Config/IDisplayClusterConfigManager.h"
#include "DisplayClusterConfigurationTypes.h"
#include "IDisplayCluster.h"
#include "IDisplayClusterCallbacks.h"

#include "RenderGraphBuilder.h"
#include "Render/Viewport/IDisplayClusterViewportManagerProxy.h"
#include "RHICommandList.h"
#include "RHIResources.h"

#include "UnrealClient.h"


FDisplayClusterMediaCaptureNode::FDisplayClusterMediaCaptureNode(const FString& InMediaId, const FString& InClusterNodeId, UMediaOutput* InMediaOutput)
	: FDisplayClusterMediaCaptureBase(InMediaId, InClusterNodeId, InMediaOutput)
{
}


bool FDisplayClusterMediaCaptureNode::StartCapture()
{
	// If capturing initialized and started successfully, subscribe for rendering callbacks
	if (FDisplayClusterMediaCaptureBase::StartCapture())
	{
		IDisplayCluster::Get().GetCallbacks().OnDisplayClusterPostFrameRender_RenderThread().AddRaw(this, &FDisplayClusterMediaCaptureNode::OnPostFrameRender_RenderThread);
		return true;
	}

	return false;
}

void FDisplayClusterMediaCaptureNode::StopCapture()
{
	// Stop rendering notifications
	IDisplayCluster::Get().GetCallbacks().OnDisplayClusterPostFrameRender_RenderThread().RemoveAll(this);
	// Stop capturing
	FDisplayClusterMediaCaptureBase::StopCapture();
}

FIntPoint FDisplayClusterMediaCaptureNode::GetCaptureSize() const
{
	if (IDisplayCluster::Get().GetConfigMgr() && IDisplayCluster::Get().GetConfigMgr()->GetLocalNode())
	{
		const FDisplayClusterConfigurationRectangle& Window = IDisplayCluster::Get().GetConfigMgr()->GetLocalNode()->WindowRect;
		return { Window.W, Window.H };
	}

	return FIntPoint();
}

FTextureRHIRef FDisplayClusterMediaCaptureNode::CreateIntermediateTexture_RenderThread(EPixelFormat Format, ETextureCreateFlags Flags, const FIntPoint& Size)
{
	// Prepare description
	FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create2D(TEXT("DisplayClusterFrameQueueCacheTexture"), Size.X, Size.Y, Format)
		.SetClearValue(FClearValueBinding::Black)
		.SetNumMips(1)
		.SetFlags(ETextureCreateFlags::Dynamic)
		.AddFlags(ETextureCreateFlags::MultiGPUGraphIgnore)
		.SetInitialState(ERHIAccess::SRVMask);

	// Leave original flags, but make sure it's ResolveTargetable but not RenderTargetable
	Flags &= ~ETextureCreateFlags::RenderTargetable;
	Flags |= ETextureCreateFlags::ResolveTargetable;
	Desc.SetFlags(Flags);

	// Create texture
	return RHICreateTexture(Desc);
}

void FDisplayClusterMediaCaptureNode::OnPostFrameRender_RenderThread(FRHICommandListImmediate& RHICmdList, const IDisplayClusterViewportManagerProxy* ViewportManagerProxy, FViewport* Viewport)
{
	TArray<FRHITexture*> Textures;
	TArray<FIntPoint>    Regions;

	if (ViewportManagerProxy && ViewportManagerProxy->GetFrameTargets_RenderThread(Textures, Regions))
	{
		if (Textures.Num() > 0 && Regions.Num() > 0 && Textures[0])
		{
			// Create interim texture if not already available
			if (!InterimTexture.IsValid() && Viewport)
			{
				if (FRHITexture* const BackbufferTexture = Viewport->GetRenderTargetTexture())
				{
					InterimTexture = CreateIntermediateTexture_RenderThread(Textures[0]->GetDesc().Format, Textures[0]->GetDesc().Flags, BackbufferTexture->GetDesc().Extent);
				}
			}

			if (InterimTexture.IsValid())
			{
				// Copy texture to the intermediate buffer
				FRHICopyTextureInfo CopyInfo;
				CopyInfo.Size = Textures[0]->GetDesc().GetSize();
				RHICmdList.CopyTexture(Textures[0], InterimTexture, CopyInfo);

				// Capture intermediate texture
				FMediaTextureInfo TextureInfo{ InterimTexture, FIntRect(FIntPoint::ZeroValue, InterimTexture->GetDesc().Extent) };
				FRDGBuilder Builder(RHICmdList);
				ExportMediaData(Builder, TextureInfo);
				Builder.Execute();
			}
		}
	}
}
