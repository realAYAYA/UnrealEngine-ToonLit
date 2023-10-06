// Copyright Epic Games, Inc. All Rights Reserved.

#include "PostProcess/TextureSharePostprocess.h"
#include "Module/TextureShareDisplayClusterLog.h"
#include "Misc/TextureShareDisplayClusterStrings.h"

#include "Containers/TextureShareCoreEnums.h"
#include "Core/TextureShareCoreHelpers.h"

#include "ITextureShareObjectProxy.h"

#include "Render/Viewport/IDisplayClusterViewportManagerProxy.h"
#include "Render/Viewport/IDisplayClusterViewportProxy.h"
#include "Render/Projection/IDisplayClusterProjectionPolicy.h"

namespace UE::TextureShare::PostProcess_FrameProxy
{
	// Support warp blend logic
	static bool ShouldApplyWarpBlend(IDisplayClusterViewportProxy* ViewportProxy)
	{
		if (ViewportProxy->GetPostRenderSettings_RenderThread().Replace.IsEnabled())
		{
			// When used override texture, disable warp blend
			return false;
		}

		const TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& PrjPolicy = ViewportProxy->GetProjectionPolicy_RenderThread();

		// Projection policy must support warp blend op
		return PrjPolicy.IsValid() && PrjPolicy->IsWarpBlendSupported();
	}
};
using namespace UE::TextureShare::PostProcess_FrameProxy;
using namespace UE::TextureShareCore;

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureSharePostprocess
//////////////////////////////////////////////////////////////////////////////////////////////// Copyright Epic Games, Inc. All Rights Reserved.
void FTextureSharePostprocess::ShareViewport_RenderThread(FRHICommandListImmediate& RHICmdList, const IDisplayClusterViewportManagerProxy* InViewportManagerProxy, const ETextureShareSyncStep InReceiveSyncStep, const EDisplayClusterViewportResourceType InResourceType, const FString& InTextureId, bool bAfterWarpBlend) const
{
	if (InViewportManagerProxy)
	{
		for (TSharedPtr<IDisplayClusterViewportProxy, ESPMode::ThreadSafe>& ViewportProxyIt : InViewportManagerProxy->GetViewports_RenderThread())
		{
			if (ViewportProxyIt.IsValid())
			{
				// Get viewport resource type
				EDisplayClusterViewportResourceType ResourceType = InResourceType;
				if (bAfterWarpBlend && ShouldApplyWarpBlend(ViewportProxyIt.Get()))
				{
					const TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& PrjPolicy = ViewportProxyIt->GetProjectionPolicy_RenderThread();
					if (PrjPolicy->ShouldUseAdditionalTargetableResource())
					{
						TArray<FRHITexture2D*> Resources;
						if (ViewportProxyIt->GetResources_RenderThread(EDisplayClusterViewportResourceType::AdditionalTargetableResource, Resources) && Resources.Num())
						{
							ResourceType = EDisplayClusterViewportResourceType::AdditionalTargetableResource;
						}
					}
				}

				const bool bMonoscopic = ViewportProxyIt->GetContexts_RenderThread().Num() == 1;

				TArray<FRHITexture2D*> ViewportResources;
				TArray<FIntRect> ViewportResourceRects;
				if (ViewportProxyIt->GetResourcesWithRects_RenderThread(ResourceType, ViewportResources, ViewportResourceRects))
				{
					for (int32 ContextIndex = 0; ContextIndex < ViewportResources.Num(); ContextIndex++)
					{
						const int32 InGPUIndex = ViewportProxyIt->GetContexts_RenderThread()[ContextIndex].RenderThreadData.GPUIndex;

						const ETextureShareEyeType EyeType = bMonoscopic
							? ETextureShareEyeType::Default
							: ((ContextIndex == 0) ? ETextureShareEyeType::StereoLeft : ETextureShareEyeType::StereoRight);

						const FTextureShareCoreViewDesc InViewDesc(ViewportProxyIt->GetId(), EyeType);

						// Execute Send request from the remote process
						ObjectProxy->ShareResource_RenderThread(RHICmdList, FTextureShareCoreResourceDesc(InTextureId, InViewDesc, ETextureShareTextureOp::Read), ViewportResources[ContextIndex], InGPUIndex, &ViewportResourceRects[ContextIndex]);
						
						// Execute Receive request from the remote process (Delayed)
						ObjectProxy->ShareResource_RenderThread(RHICmdList, FTextureShareCoreResourceDesc(InTextureId, InViewDesc, ETextureShareTextureOp::Write, InReceiveSyncStep), ViewportResources[ContextIndex], InGPUIndex, &ViewportResourceRects[ContextIndex]);
					}
				}
			}
		}
	}
}

void FTextureSharePostprocess::ShareFrame_RenderThread(FRHICommandListImmediate& RHICmdList, const IDisplayClusterViewportManagerProxy* InViewportManagerProxy, const ETextureShareSyncStep InReceiveSyncStep, const EDisplayClusterViewportResourceType InResourceType, const FString& InTextureId) const
{
	if (InViewportManagerProxy)
	{
		TArray<FRHITexture2D*> FrameResources;
		TArray<FRHITexture2D*> AdditionalFrameResources;
		TArray<FIntPoint> TargetOffsets;
		if (InViewportManagerProxy->GetFrameTargets_RenderThread(FrameResources, TargetOffsets, &AdditionalFrameResources))
		{
			TArray<FRHITexture2D*>& SharedResources = (InResourceType == EDisplayClusterViewportResourceType::AdditionalFrameTargetableResource)
				? AdditionalFrameResources : FrameResources;

			const bool bMonoscopic = SharedResources.Num() == 1;

			for (int32 ContextIndex = 0; ContextIndex < SharedResources.Num(); ContextIndex++)
			{
				const int32 InGPUIndex = -1;

				const ETextureShareEyeType EyeType = bMonoscopic
					? ETextureShareEyeType::Default
					: ((ContextIndex == 0) ? ETextureShareEyeType::StereoLeft : ETextureShareEyeType::StereoRight);

				const FTextureShareCoreViewDesc InViewDesc(EyeType);

				// Send
				ObjectProxy->ShareResource_RenderThread(RHICmdList, FTextureShareCoreResourceDesc(InTextureId, InViewDesc, ETextureShareTextureOp::Read), SharedResources[ContextIndex], InGPUIndex);

				// Receive
				ObjectProxy->ShareResource_RenderThread(RHICmdList, FTextureShareCoreResourceDesc(InTextureId, InViewDesc, ETextureShareTextureOp::Write, InReceiveSyncStep), SharedResources[ContextIndex], InGPUIndex);
			}
		}
	}
}
