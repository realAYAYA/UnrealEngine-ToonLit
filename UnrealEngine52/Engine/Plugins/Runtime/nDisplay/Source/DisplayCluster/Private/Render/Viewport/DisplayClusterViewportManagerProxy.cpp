// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/DisplayClusterViewportManagerProxy.h"

#include "ClearQuad.h"
#include "Misc/DisplayClusterLog.h"

#include "IDisplayCluster.h"
#include "IDisplayClusterCallbacks.h"
#include "Render/IDisplayClusterRenderManager.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Projection/IDisplayClusterProjectionPolicyFactory.h"
#include "Render/Projection/IDisplayClusterProjectionPolicy.h"

#include "Render/Viewport/LightCard/DisplayClusterViewportLightCardManager.h"
#include "Render/Viewport/LightCard/DisplayClusterViewportLightCardManagerProxy.h"

#include "Render/Viewport/DisplayClusterViewportProxy.h"
#include "Render/Viewport/DisplayClusterViewportManagerViewExtension.h"

#include "Render/Viewport/RenderTarget/DisplayClusterRenderTargetManager.h"
#include "Render/Viewport/RenderTarget/DisplayClusterRenderTargetResource.h"
#include "Render/Viewport/Postprocess/DisplayClusterViewportPostProcessManager.h"
#include "Render/Viewport/Containers/DisplayClusterViewportProxyData.h"

#include "Render/Viewport/DisplayClusterViewportStrings.h"
#include "RHIContext.h"

// Enable/disable warp&blend
static TAutoConsoleVariable<int32> CVarWarpBlendEnabled(
	TEXT("nDisplay.render.WarpBlendEnabled"),
	1,
	TEXT("Warp & Blend status\n")
	TEXT("0 : disabled\n")
	TEXT("1 : enabled\n")
	,
	ECVF_RenderThreadSafe
);

// Enable/Disable ClearTexture for Frame RTT
static TAutoConsoleVariable<int32> CVarClearFrameRTTEnabled(
	TEXT("nDisplay.render.ClearFrameRTTEnabled"),
	1,
	TEXT("Enables FrameRTT clearing before viewport resolving.\n")
	TEXT("0 : disabled\n")
	TEXT("1 : enabled\n")
	,
	ECVF_RenderThreadSafe
);

///////////////////////////////////////////////////////////////////////////////////////
namespace DisplayClusterViewportManagerProxyHelpers
{
	// Support warp blend logic
	static inline bool ShouldApplyWarpBlend(IDisplayClusterViewportProxy* ViewportProxy)
	{
		if (ViewportProxy->GetPostRenderSettings_RenderThread().Replace.IsEnabled())
		{
			// When used override texture, disable warp blend
			return false;
		}

		const TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& PrjPolicy = ViewportProxy->GetProjectionPolicy_RenderThread();

		// Ask current projection policy if it's warp&blend compatible
		return PrjPolicy.IsValid() && PrjPolicy->IsWarpBlendSupported();
	}
};

using namespace DisplayClusterViewportManagerProxyHelpers;

///////////////////////////////////////////////////////////////////////////////////////
//          FDisplayClusterViewportManagerProxy
///////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterViewportManagerProxy::FDisplayClusterViewportManagerProxy()
{ }

FDisplayClusterViewportManagerProxy::~FDisplayClusterViewportManagerProxy()
{ }

void FDisplayClusterViewportManagerProxy::Release_RenderThread()
{
	check(IsInRenderingThread());

	// Delete viewport proxy objects
	ViewportProxies.Empty();
	ClusterNodeViewportProxies.Empty();

	if (RenderTargetManager.IsValid())
	{
		RenderTargetManager->Release();
		RenderTargetManager.Reset();
	}

	if (PostProcessManager.IsValid())
	{
		PostProcessManager->Release();
		PostProcessManager.Reset();
	}

	LightCardManagerProxy.Reset();

	if (ViewportManagerViewExtension.IsValid())
	{
		// Force release of VE data since this TSharedPtr<> may also be held by other resources at this time.
		// So Reset() doesn't actually release it right away in some situations.
		ViewportManagerViewExtension->Release_RenderThread();
		ViewportManagerViewExtension.Reset();
	}
}

void FDisplayClusterViewportManagerProxy::Initialize(FDisplayClusterViewportManager& InViewportManager)
{
	RenderTargetManager = InViewportManager.RenderTargetManager;
	PostProcessManager = InViewportManager.PostProcessManager;
	LightCardManagerProxy = InViewportManager.LightCardManager->GetLightCardManagerProxy();
	ViewportManagerViewExtension = InViewportManager.ViewportManagerViewExtension;
}

void FDisplayClusterViewportManagerProxy::DeleteResource_RenderThread(FDisplayClusterViewportResource* InDeletedResourcePtr)
{
	if (InDeletedResourcePtr)
	{
		// Handle resource refs must be removed from all viewports
		for (TSharedPtr<FDisplayClusterViewportProxy, ESPMode::ThreadSafe>& ViewportProxyIt : ViewportProxies)
		{
			if (ViewportProxyIt.IsValid())
			{
				ViewportProxyIt->HandleResourceDelete_RenderThread(InDeletedResourcePtr);
			}
		}

		InDeletedResourcePtr->ReleaseResource();
		delete InDeletedResourcePtr;
	}
}

void FDisplayClusterViewportManagerProxy::ImplUpdateClusterNodeViewportProxies()
{
	ClusterNodeViewportProxies.Empty();
	// Collect viewport proxies for rendered cluster node
	for (TSharedPtr<FDisplayClusterViewportProxy, ESPMode::ThreadSafe>& ViewportProxyIt : ViewportProxies)
	{
		if (ViewportProxyIt.IsValid() && (RenderFrameSettings.ClusterNodeId.IsEmpty() || ViewportProxyIt->GetClusterNodeId() == RenderFrameSettings.ClusterNodeId))
		{
			ClusterNodeViewportProxies.Add(ViewportProxyIt);
		}
	}
}

void FDisplayClusterViewportManagerProxy::CreateViewport_RenderThread(const TSharedPtr<FDisplayClusterViewportProxy, ESPMode::ThreadSafe>& InViewportProxy)
{
	check(IsInRenderingThread());
	
	ViewportProxies.Add(InViewportProxy);
	ImplUpdateClusterNodeViewportProxies();
}

void FDisplayClusterViewportManagerProxy::DeleteViewport_RenderThread(const TSharedPtr<FDisplayClusterViewportProxy, ESPMode::ThreadSafe>& InViewportProxy)
{
	check(IsInRenderingThread());

	// Remove viewport obj from manager
	const int32 ViewportProxyIndex = ViewportProxies.Find(InViewportProxy);
	if (ViewportProxyIndex != INDEX_NONE)
	{
		ViewportProxies.RemoveAt(ViewportProxyIndex);
	}

	const int32 ClusterViewportProxyIndex = ClusterNodeViewportProxies.Find(InViewportProxy);
	if (ClusterViewportProxyIndex != INDEX_NONE)
	{
		ClusterNodeViewportProxies.RemoveAt(ClusterViewportProxyIndex);
	}
}

void FDisplayClusterViewportManagerProxy::ImplUpdateRenderFrameSettings(const FDisplayClusterRenderFrameSettings& InRenderFrameSettings,
	const TSharedPtr<FDisplayClusterViewportManagerViewExtension, ESPMode::ThreadSafe>& InViewportManagerViewExtension)
{
	check(IsInGameThread());

	FDisplayClusterRenderFrameSettings* Settings = new FDisplayClusterRenderFrameSettings(InRenderFrameSettings);

	// Send frame settings to renderthread
	ENQUEUE_RENDER_COMMAND(DisplayClusterUpdateRenderFrameSettings)(
		[ViewportManagerProxy = SharedThis(this), Settings, InViewportManagerViewExtension](FRHICommandListImmediate& RHICmdList)
	{
		ViewportManagerProxy->RenderFrameSettings = *Settings;
		ViewportManagerProxy->ViewportManagerViewExtension = InViewportManagerViewExtension;
		delete Settings;

		// After updated settings we need update cluster node viewports
		ViewportManagerProxy->ImplUpdateClusterNodeViewportProxies();
	});
}

void FDisplayClusterViewportManagerProxy::ImplUpdateViewports(const TArray<FDisplayClusterViewport*>& InViewports)
{
	check(IsInGameThread());

	TArray<FDisplayClusterViewportProxyData*> ViewportProxiesData;
	for (FDisplayClusterViewport* ViewportIt : InViewports)
	{
		ViewportProxiesData.Add(new FDisplayClusterViewportProxyData(ViewportIt));
	}

	// Send viewports settings to renderthread
	ENQUEUE_RENDER_COMMAND(DisplayClusterUpdateViewports)(
		[ProxiesData = std::move(ViewportProxiesData)](FRHICommandListImmediate& RHICmdList)
	{
		for (FDisplayClusterViewportProxyData* It : ProxiesData)
		{
			It->UpdateProxy_RenderThread();
			delete It;
		}
	});
}


DECLARE_GPU_STAT_NAMED(nDisplay_ViewportManager_RenderFrame, TEXT("nDisplay ViewportManager::RenderFrame"));

void FDisplayClusterViewportManagerProxy::ImplRenderFrame(FViewport* InViewport)
{
	ENQUEUE_RENDER_COMMAND(DisplayClusterRenderFrame_Setup)(
		[InViewportManagerProxy = SharedThis(this), InViewport](FRHICommandListImmediate& RHICmdList)
	{
		SCOPED_GPU_STAT(RHICmdList, nDisplay_ViewportManager_RenderFrame);
		SCOPED_DRAW_EVENT(RHICmdList, nDisplay_ViewportManager_RenderFrame);

		const FDisplayClusterViewportManagerProxy* ViewportManagerProxy = &InViewportManagerProxy.Get();

		// Handle render setup
		if (ViewportManagerProxy->PostProcessManager.IsValid())
		{
			ViewportManagerProxy->PostProcessManager->HandleRenderFrameSetup_RenderThread(RHICmdList, ViewportManagerProxy);
		}
	});

	ENQUEUE_RENDER_COMMAND(DisplayClusterRenderFrame_CrossGPUTransfer)(
		[InViewportManagerProxy = SharedThis(this), InViewport](FRHICommandListImmediate& RHICmdList)
	{
		const FDisplayClusterViewportManagerProxy* ViewportManagerProxy = &InViewportManagerProxy.Get();

		// mGPU not used for in-editor rendering
		if(ViewportManagerProxy->RenderFrameSettings.bIsRenderingInEditor == false || ViewportManagerProxy->RenderFrameSettings.bAllowMultiGPURenderingInEditor)
		{
			// Move all render target cross gpu
			ViewportManagerProxy->DoCrossGPUTransfers_RenderThread(RHICmdList);
			// Now all resources on GPU#0
		}

		// PostCrossGpuTransfer notification
		IDisplayCluster::Get().GetCallbacks().OnDisplayClusterPostCrossGpuTransfer_RenderThread().Broadcast(RHICmdList, ViewportManagerProxy, InViewport);
		// Latency processing
		IDisplayCluster::Get().GetCallbacks().OnDisplayClusterProcessLatency_RenderThread().Broadcast(RHICmdList, ViewportManagerProxy, InViewport);
	});

	ENQUEUE_RENDER_COMMAND(DisplayClusterRenderFrame_UpdateDeferredResources)(
		[ViewportManagerProxy = SharedThis(this), InViewport](FRHICommandListImmediate& RHICmdList)
	{
		// Update viewports resources: vp/texture overlay, OCIO, blur, nummips, etc
		ViewportManagerProxy->UpdateDeferredResources_RenderThread(RHICmdList);
	});

	ENQUEUE_RENDER_COMMAND(DisplayClusterRenderFrame_WarpBlend)(
		[InViewportManagerProxy = SharedThis(this), InViewport](FRHICommandListImmediate& RHICmdList)
	{
		const FDisplayClusterViewportManagerProxy* ViewportManagerProxy = &InViewportManagerProxy.Get();

		if (ViewportManagerProxy->PostProcessManager.IsValid())
		{
			ViewportManagerProxy->PostProcessManager->HandleBeginUpdateFrameResources_RenderThread(RHICmdList, ViewportManagerProxy);
		}

		const bool bWarpBlendEnabled = ViewportManagerProxy->RenderFrameSettings.bAllowWarpBlend && CVarWarpBlendEnabled.GetValueOnRenderThread() != 0;

		// Update the frame resources: post-processing, warping, and finally resolving everything to the frame resource
		ViewportManagerProxy->UpdateFrameResources_RenderThread(RHICmdList, bWarpBlendEnabled);

		if (ViewportManagerProxy->PostProcessManager.IsValid())
		{
			ViewportManagerProxy->PostProcessManager->HandleEndUpdateFrameResources_RenderThread(RHICmdList, ViewportManagerProxy);
		}

		// Postrender notification before copying final image to the backbuffer
		IDisplayCluster::Get().GetCallbacks().OnDisplayClusterPostFrameRender_RenderThread().Broadcast(RHICmdList, ViewportManagerProxy, InViewport);

		if (InViewport)
		{
			if (FRHITexture2D* FrameOutputRTT = InViewport->GetRenderTargetTexture())
			{
				// For quadbuf stereo copy only left eye, right copy from OutputFrameTarget
				//@todo Copy QuadBuf_LeftEye/(mono,sbs,tp) to separate rtt, before UI and debug rendering
				//@todo QuadBuf_LeftEye copied latter, before present
				switch (ViewportManagerProxy->RenderFrameSettings.RenderMode)
				{
				case EDisplayClusterRenderFrameMode::SideBySide:
				case EDisplayClusterRenderFrameMode::TopBottom:
					ViewportManagerProxy->ResolveFrameTargetToBackBuffer_RenderThread(RHICmdList, 1, 0, FrameOutputRTT, FrameOutputRTT->GetSizeXY());
					break;
				default:
					break;
				}

				ViewportManagerProxy->ResolveFrameTargetToBackBuffer_RenderThread(RHICmdList, 0, 0, FrameOutputRTT, FrameOutputRTT->GetSizeXY());

				// Finally, notify about backbuffer update
				IDisplayCluster::Get().GetCallbacks().OnDisplayClusterPostBackbufferUpdated_RenderThread().Broadcast(RHICmdList, InViewport);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
				IDisplayCluster::Get().GetCallbacks().OnDisplayClusterPostBackbufferUpdate_RenderThread().Broadcast(RHICmdList, ViewportManagerProxy, InViewport);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
			}
		}
	});
}

void FDisplayClusterViewportManagerProxy::UpdateDeferredResources_RenderThread(FRHICommandListImmediate& RHICmdList) const
{
	check(IsInRenderingThread());

	TArray<TSharedPtr<FDisplayClusterViewportProxy, ESPMode::ThreadSafe>> OverriddenViewports;
	OverriddenViewports.Reserve(ClusterNodeViewportProxies.Num());

	for (const TSharedPtr<FDisplayClusterViewportProxy, ESPMode::ThreadSafe>& ViewportProxy : ClusterNodeViewportProxies)
	{
		if (!ViewportProxy->RenderSettings.IsViewportOverrided())
		{
			ViewportProxy->UpdateDeferredResources(RHICmdList);
		}
		else
		{
			// Update after all
			OverriddenViewports.Add(ViewportProxy);
		}
	}

	// Update deferred viewports after all
	for (TSharedPtr<FDisplayClusterViewportProxy, ESPMode::ThreadSafe>& ViewportProxy : OverriddenViewports)
	{
		ViewportProxy->UpdateDeferredResources(RHICmdList);
	}
}

static void ImplClearRenderTargetResource_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* InRenderTargetTexture)
{
	FRHIRenderPassInfo RPInfo(InRenderTargetTexture, ERenderTargetActions::DontLoad_Store);
	RHICmdList.Transition(FRHITransitionInfo(InRenderTargetTexture, ERHIAccess::Unknown, ERHIAccess::RTV));
	RHICmdList.BeginRenderPass(RPInfo, TEXT("nDisplay_ClearRTT"));
	{
		const FIntPoint Size = InRenderTargetTexture->GetSizeXY();
		RHICmdList.SetViewport(0, 0, 0.0f, Size.X, Size.Y, 1.0f);
		DrawClearQuad(RHICmdList, FLinearColor::Black);
	}
	RHICmdList.EndRenderPass();
	RHICmdList.Transition(FRHITransitionInfo(InRenderTargetTexture, ERHIAccess::Unknown, ERHIAccess::SRVMask));
}

void FDisplayClusterViewportManagerProxy::ImplClearFrameTargets_RenderThread(FRHICommandListImmediate& RHICmdList) const
{
	TArray<FRHITexture2D*> FrameResources;
	TArray<FRHITexture2D*> AdditionalFrameResources;
	TArray<FIntPoint> TargetOffset;
	if (GetFrameTargets_RenderThread(FrameResources, TargetOffset, &AdditionalFrameResources))
	{
		for (FRHITexture2D* It : FrameResources)
		{
			ImplClearRenderTargetResource_RenderThread(RHICmdList, It);
		}
	}
}

/**
 * WarpBlend render pass. (internal)
 */
enum class EWarpPass : uint8
{
	Begin = 0,
	Render,
	End,
	COUNT
};

void FDisplayClusterViewportManagerProxy::UpdateFrameResources_RenderThread(FRHICommandListImmediate& RHICmdList, bool bWarpBlendEnabled) const
{
	check(IsInRenderingThread());

	// Do postprocess before warp&blend
	if (PostProcessManager.IsValid())
	{
		PostProcessManager->PerformPostProcessViewBeforeWarpBlend_RenderThread(RHICmdList, this);
	}

	// Support viewport overlap order sorting:
	TArray<TSharedPtr<FDisplayClusterViewportProxy, ESPMode::ThreadSafe>> SortedViewportProxy = ClusterNodeViewportProxies;
	SortedViewportProxy.Sort(
		[](const TSharedPtr<FDisplayClusterViewportProxy, ESPMode::ThreadSafe>& VP1, const TSharedPtr<FDisplayClusterViewportProxy, ESPMode::ThreadSafe>& VP2)
		{
			return  VP1->GetRenderSettings_RenderThread().OverlapOrder < VP2->GetRenderSettings_RenderThread().OverlapOrder;
		}
	);

	// Clear Frame RTT resources before viewport resolving
	const bool bClearFrameRTTEnabled = CVarClearFrameRTTEnabled.GetValueOnRenderThread() != 0;
	if (bClearFrameRTTEnabled)
	{
		ImplClearFrameTargets_RenderThread(RHICmdList);
	}

	IDisplayCluster::Get().GetCallbacks().OnDisplayClusterPreWarp_RenderThread().Broadcast(RHICmdList, this);

	// Handle warped viewport projection policy logic:
	for (uint8 WarpPass = 0; WarpPass < (uint8)EWarpPass::COUNT; WarpPass++)
	{
		// Update deferred resources for viewports
		for (TSharedPtr<FDisplayClusterViewportProxy, ESPMode::ThreadSafe>& ViewportProxyIt : SortedViewportProxy)
		{
			// Iterate over visible viewports:
			if (ViewportProxyIt.IsValid() && ViewportProxyIt->GetRenderSettings_RenderThread().bVisible)
			{
				if (bWarpBlendEnabled && ShouldApplyWarpBlend(ViewportProxyIt.Get()))
				{
					const TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& PrjPolicy = ViewportProxyIt->GetProjectionPolicy_RenderThread();
					switch ((EWarpPass)WarpPass)
					{
					case EWarpPass::Begin:
						IDisplayCluster::Get().GetCallbacks().OnDisplayClusterPreWarpViewport_RenderThread().Broadcast(RHICmdList, ViewportProxyIt.Get());
						PrjPolicy->BeginWarpBlend_RenderThread(RHICmdList, ViewportProxyIt.Get());
						break;

					case EWarpPass::Render:
						PrjPolicy->ApplyWarpBlend_RenderThread(RHICmdList, ViewportProxyIt.Get());
						break;

					case EWarpPass::End:
						PrjPolicy->EndWarpBlend_RenderThread(RHICmdList, ViewportProxyIt.Get());
						IDisplayCluster::Get().GetCallbacks().OnDisplayClusterPostWarpViewport_RenderThread().Broadcast(RHICmdList, ViewportProxyIt.Get());
						break;

					default:
						break;
					}
				}
			}
		}
	}

	if (PostProcessManager.IsValid())
	{
		// per-frame handle
		PostProcessManager->HandleUpdateFrameResourcesAfterWarpBlend_RenderThread(RHICmdList, this);

		// Per-view postprocess
		PostProcessManager->PerformPostProcessViewAfterWarpBlend_RenderThread(RHICmdList, this);
	}

	// Post resolve to Frame RTT
	// All warp&blend results are now inside AdditionalTargetableResource. Viewport images of other projection policies are still stored in the InputShaderResource.
	for (TSharedPtr<FDisplayClusterViewportProxy, ESPMode::ThreadSafe>& ViewportProxyIt : SortedViewportProxy)
	{
		// Iterate over visible viewports:
		if (ViewportProxyIt.IsValid() && ViewportProxyIt->GetRenderSettings_RenderThread().bVisible)
		{
			EDisplayClusterViewportResourceType ViewportSource = EDisplayClusterViewportResourceType::InputShaderResource;
			if (bWarpBlendEnabled && ShouldApplyWarpBlend(ViewportProxyIt.Get()))
			{
				const TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& PrjPolicy = ViewportProxyIt->GetProjectionPolicy_RenderThread();
				if (PrjPolicy->ShouldUseAdditionalTargetableResource())
				{
					ViewportSource = EDisplayClusterViewportResourceType::AdditionalTargetableResource;
				}
			}

			// resolve viewports to the frame target texture
			ViewportProxyIt->ResolveResources_RenderThread(RHICmdList, ViewportSource, ViewportProxyIt->GetOutputResourceType_RenderThread());

			// Apply post-warp (viewport remap, etc)
			ViewportProxyIt->PostResolveViewport_RenderThread(RHICmdList);
		}
	}

	IDisplayCluster::Get().GetCallbacks().OnDisplayClusterPostWarp_RenderThread().Broadcast(RHICmdList, this);

	if (PostProcessManager.IsValid())
	{
		PostProcessManager->PerformPostProcessFrameAfterWarpBlend_RenderThread(RHICmdList, this);
	}
}

void FDisplayClusterViewportManagerProxy::DoCrossGPUTransfers_RenderThread(FRHICommandListImmediate& RHICmdList) const
{
	check(IsInRenderingThread());

#if WITH_MGPU
	if (GNumExplicitGPUsForRendering < 2)
	{
		// for mGPU we need at least 2 GPUs
		return;
	}

	// Copy the view render results to all GPUs that are native to the viewport.
	TArray<FTransferResourceParams> TransferResources;

	for (const TSharedPtr<FDisplayClusterViewportProxy, ESPMode::ThreadSafe>& ViewportProxyIt : ClusterNodeViewportProxies)
	{
		if (!ViewportProxyIt.IsValid())
		{
			continue;
		}

		if (!ViewportProxyIt->RenderSettings.bEnableCrossGPUTransfer || ViewportProxyIt->RenderTargets.IsEmpty())
		{
			// Skip a frozen viewport that has already been transferred between GPUs
			// The first time freezing should do the transfer (RenderTargets must be assigned on the first pass)
			continue;
		}

		// Do cross GPU trasfers for all viewport contexts
		for (const FDisplayClusterViewport_Context& ViewportContext : ViewportProxyIt->Contexts)
		{
			if (!ViewportContext.bOverrideCrossGPUTransfer)
			{
				// Skip this context because it uses the default way
				continue;
			}

			const int32 SrcGPUIndex = ViewportContext.RenderThreadData.GPUIndex;
			if (SrcGPUIndex < 0)
			{
				// This viewport context does not use the custom GPU index for rendering
				continue;
			}

			if (!ViewportProxyIt->RenderTargets.IsValidIndex(ViewportContext.ContextNum))
			{
				// RTT does not exist for this context
				continue;
			}

			// Clamp the view rect by the rendertarget rect to prevent issues when resizing the viewport.
			const FIntRect TransferRect = ViewportContext.RenderTargetRect;
			if (TransferRect.Width() <= 0 || TransferRect.Height() <= 0)
			{
				// Broken rect, skip
				continue;
			}

			// Perform an MGPU transfer for a specific viewport context:
			if (FDisplayClusterViewportRenderTargetResource* RenderTarget = ViewportProxyIt->RenderTargets[ViewportContext.ContextNum])
			{
				if (FRHITexture2D* TextureRHI = RenderTarget->GetViewportResource2DRHI())
				{
					const FRHIGPUMask RenderTargetGPUMask = RenderTarget->GetGPUMask(RHICmdList);

					for (uint32 DestGPUIndex : RenderTargetGPUMask)
					{
						if (DestGPUIndex != SrcGPUIndex)
						{
							TransferResources.Add(FTransferResourceParams(
								TextureRHI, TransferRect,
								SrcGPUIndex, DestGPUIndex,
								RenderFrameSettings.CrossGPUTransfer.bPullData,
								RenderFrameSettings.CrossGPUTransfer.bLockSteps
							));
						}
					}
				}
			}
		}
	}

	if (TransferResources.Num() > 0)
	{
		RHICmdList.TransferResources(TransferResources);
	}

#endif // WITH_MGPU
}

bool FDisplayClusterViewportManagerProxy::GetFrameTargets_RenderThread(TArray<FRHITexture2D*>& OutFrameResources, TArray<FIntPoint>& OutTargetOffsets, TArray<FRHITexture2D*>* OutAdditionalFrameResources) const
{
	check(IsInRenderingThread());

	// Get any defined frame targets from first visible viewport
	for (const TSharedPtr<FDisplayClusterViewportProxy, ESPMode::ThreadSafe>& ViewportProxyIt : ClusterNodeViewportProxies)
	{
		// Process only valid viewports with output frame resources
		if (ViewportProxyIt.IsValid() && !ViewportProxyIt->OutputFrameTargetableResources.IsEmpty())
		{
			const TArray<FDisplayClusterViewportTextureResource*>& Frames = ViewportProxyIt->OutputFrameTargetableResources;
			const TArray<FDisplayClusterViewportTextureResource*>& AdditionalFrames = ViewportProxyIt->AdditionalFrameTargetableResources;

			for (int32 FrameIt = 0; FrameIt < Frames.Num(); FrameIt++)
			{
				if (FRHITexture2D* FrameTexture = Frames[FrameIt] ? Frames[FrameIt]->GetViewportResource2DRHI() : nullptr)
				{
					OutFrameResources.Add(FrameTexture);
					OutTargetOffsets.Add(Frames[FrameIt]->BackbufferFrameOffset);

					if (OutAdditionalFrameResources && AdditionalFrames.IsValidIndex(FrameIt))
					{
						if (FRHITexture2D* AdditionalFrameTexture = AdditionalFrames[FrameIt] ? AdditionalFrames[FrameIt]->GetViewportResource2DRHI() : nullptr)
						{
							OutAdditionalFrameResources->Add(AdditionalFrameTexture);
						}
					}
				}
			}

			// if all resources are received in the same amount
			const bool bValidAdditionalFrameResources = !OutAdditionalFrameResources || AdditionalFrames.IsEmpty() || (OutAdditionalFrameResources && OutAdditionalFrameResources->Num() == Frames.Num());
			if (OutFrameResources.Num() == Frames.Num() && bValidAdditionalFrameResources)
			{
				return true;
			}
		}

		// Resetting output values at the end of each cycle
		OutFrameResources.Reset();
		OutTargetOffsets.Reset();
		if (OutAdditionalFrameResources)
		{
			OutAdditionalFrameResources->Reset();
		}
	}

	// no visible viewports
	return false;
}

bool FDisplayClusterViewportManagerProxy::ResolveFrameTargetToBackBuffer_RenderThread(FRHICommandListImmediate& RHICmdList, const uint32 InContextNum, const int32 DestArrayIndex, FRHITexture2D* DestTexture, FVector2D WindowSize) const
{
	check(IsInRenderingThread());

	TArray<FRHITexture2D*>   FrameResources;
	TArray<FIntPoint>        TargetOffsets;
	if (GetFrameTargets_RenderThread(FrameResources, TargetOffsets))
	{
		// Use internal frame textures as source
		int32 ContextNum = InContextNum;

		FRHITexture2D* FrameTexture = FrameResources[ContextNum];
		FIntPoint DstOffset = TargetOffsets[ContextNum];

		if (FrameTexture)
		{
			const FIntPoint SrcSize = FrameTexture->GetSizeXY();
			const FIntPoint DstSize = DestTexture->GetSizeXY();;

			FIntRect DstRect(DstOffset, DstOffset + SrcSize);

			// Fit to backbuffer size
			DstRect.Max.X = FMath::Min(DstSize.X, DstRect.Max.X);
			DstRect.Max.Y = FMath::Min(DstSize.Y, DstRect.Max.Y);

			FRHICopyTextureInfo CopyInfo;

			CopyInfo.SourceSliceIndex = 0;
			CopyInfo.DestSliceIndex = DestArrayIndex;

			CopyInfo.Size.X = DstRect.Width();
			CopyInfo.Size.Y = DstRect.Height();

			CopyInfo.DestPosition.X = DstRect.Min.X;
			CopyInfo.DestPosition.Y = DstRect.Min.Y;

			TransitionAndCopyTexture(RHICmdList, FrameTexture, DestTexture, CopyInfo);

			return true;
		}
	}

	return false;
}

FDisplayClusterViewportProxy* FDisplayClusterViewportManagerProxy::ImplFindViewport_RenderThread(const FString& ViewportId) const
{
	check(IsInRenderingThread());

	// Ok, we have a request for a particular viewport. Let's find it.
	TSharedPtr<FDisplayClusterViewportProxy, ESPMode::ThreadSafe> const* DesiredViewport = ViewportProxies.FindByPredicate([ViewportId](const TSharedPtr<FDisplayClusterViewportProxy, ESPMode::ThreadSafe>& ItemViewport)
	{
		return ViewportId.Equals(ItemViewport->GetId(), ESearchCase::IgnoreCase);
	});

	return (DesiredViewport != nullptr) ? DesiredViewport->Get() : nullptr;
}

FDisplayClusterViewportProxy* FDisplayClusterViewportManagerProxy::ImplFindViewport_RenderThread(const int32 StereoViewIndex, uint32* OutContextNum) const
{
	check(IsInRenderingThread());

	for (const TSharedPtr<FDisplayClusterViewportProxy, ESPMode::ThreadSafe>&  ViewportProxyIt : ViewportProxies)
	{
		if (ViewportProxyIt.IsValid() && ViewportProxyIt->FindContext_RenderThread(StereoViewIndex, OutContextNum))
		{
			return ViewportProxyIt.Get();
		}
	}

	// Viewport proxy not found
	return nullptr;
}
