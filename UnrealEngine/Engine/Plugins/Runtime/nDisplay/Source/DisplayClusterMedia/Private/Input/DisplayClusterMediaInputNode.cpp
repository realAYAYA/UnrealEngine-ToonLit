// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/DisplayClusterMediaInputNode.h"

#include "DisplayClusterMediaHelpers.h"
#include "Render/Viewport/IDisplayClusterViewportManagerProxy.h"

#include "IDisplayCluster.h"
#include "IDisplayClusterCallbacks.h"

#include "RHICommandList.h"
#include "RHIResources.h"


FDisplayClusterMediaInputNode::FDisplayClusterMediaInputNode(const FString& InMediaId, const FString& InClusterNodeId, UMediaSource* InMediaSource)
	: FDisplayClusterMediaInputBase(InMediaId, InClusterNodeId, InMediaSource)
{
}


bool FDisplayClusterMediaInputNode::Play()
{
	if (FDisplayClusterMediaInputBase::Play())
	{
		IDisplayCluster::Get().GetCallbacks().OnDisplayClusterPostFrameRender_RenderThread().AddRaw(this, &FDisplayClusterMediaInputNode::OnPostFrameRender_RenderThread);
		return true;
	}

	return false;
}

void FDisplayClusterMediaInputNode::Stop()
{
	// Stop receiving notifications
	IDisplayCluster::Get().GetCallbacks().OnDisplayClusterPostFrameRender_RenderThread().RemoveAll(this);
	// Stop playing
	FDisplayClusterMediaInputBase::Stop();
}

void FDisplayClusterMediaInputNode::OnPostFrameRender_RenderThread(FRHICommandListImmediate& RHICmdList, const IDisplayClusterViewportManagerProxy* ViewportManagerProxy, FViewport* Viewport)
{
	ensure(ViewportManagerProxy);

	TArray<FRHITexture*> Textures;
	TArray<FIntPoint>    Regions;

	if (ViewportManagerProxy && ViewportManagerProxy->GetFrameTargets_RenderThread(Textures, Regions))
	{
		if (Textures.Num() > 0 && Regions.Num() > 0 && Textures[0])
		{
			FMediaTextureInfo TextureInfo{ Textures[0], FIntRect(Regions[0], Regions[0] + Textures[0]->GetDesc().Extent) };
			ImportMediaData(RHICmdList, TextureInfo);
		}
	}
}
