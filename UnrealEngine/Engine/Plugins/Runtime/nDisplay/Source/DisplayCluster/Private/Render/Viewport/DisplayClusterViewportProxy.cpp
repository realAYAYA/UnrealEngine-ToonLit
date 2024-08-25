// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/DisplayClusterViewportProxy.h"
#include "Render/Viewport/DisplayClusterViewportManagerProxy.h"
#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewportManagerViewExtension.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_PostRenderSettings.h"
#include "Render/Viewport/Containers/DisplayClusterViewportProxyData.h"

#include "Render/Viewport/RenderTarget/DisplayClusterRenderTargetResource.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"

#include "Render/Viewport/LightCard/DisplayClusterViewportLightCardManagerProxy.h"

#include "Render/Projection/IDisplayClusterProjectionPolicy.h"

#include "Render/Containers/IDisplayClusterRender_MeshComponent.h"
#include "Render/Containers/IDisplayClusterRender_MeshComponentProxy.h"

#include "IDisplayCluster.h"
#include "IDisplayClusterCallbacks.h"
#include "IDisplayClusterShaders.h"
#include "TextureResource.h"

#include "RHIStaticStates.h"

#include "RenderResource.h"
#include "RenderingThread.h"
#include "CommonRenderResources.h"
#include "PixelShaderUtils.h"

#include "GlobalShader.h"
#include "ShaderParameters.h"
#include "ShaderParameterUtils.h"

#include "ScreenRendering.h"
#include "RenderGraphUtils.h"
#include "RenderGraphResources.h"
#include "PostProcess/DrawRectangle.h"

#include "ScreenPass.h"
#include "SceneTextures.h"
#include "PostProcess/PostProcessMaterialInputs.h"

///////////////////////////////////////////////////////////////////////////////////////
namespace UE::DisplayCluster::ViewportProxy
{
	static IDisplayClusterShaders& GetShadersAPI()
	{
		static IDisplayClusterShaders& ShadersAPISingleton = IDisplayClusterShaders::Get();

		return ShadersAPISingleton;
	}
};

///////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterViewportProxy::FDisplayClusterViewportProxy(const TSharedRef<FDisplayClusterViewportConfiguration, ESPMode::ThreadSafe>& InConfiguration, const FString& InViewportId, const TSharedPtr<IDisplayClusterProjectionPolicy, ESPMode::ThreadSafe>& InProjectionPolicy)
	: ConfigurationProxy(InConfiguration->Proxy)
	, ViewportId(InViewportId)
	, ClusterNodeId(InConfiguration->GetClusterNodeId())
	, ProjectionPolicy(InProjectionPolicy)
{
	check(ProjectionPolicy.IsValid());
}

FDisplayClusterViewportProxy::~FDisplayClusterViewportProxy()
{
}

void FDisplayClusterViewportProxy::UpdateViewportProxyData_RenderThread(const FDisplayClusterViewportProxyData& InViewportProxyData)
{
	OpenColorIO = InViewportProxyData.OpenColorIO;

	DisplayDeviceProxy = InViewportProxyData.DisplayDeviceProxy;

	OverscanRuntimeSettings = InViewportProxyData.OverscanRuntimeSettings;

	RemapMesh = InViewportProxyData.RemapMesh;

	RenderSettings = InViewportProxyData.RenderSettings;

	RenderSettingsICVFX.SetParameters(InViewportProxyData.RenderSettingsICVFX);
	PostRenderSettings.SetParameters(InViewportProxyData.PostRenderSettings);

	ProjectionPolicy = InViewportProxyData.ProjectionPolicy;

	// The RenderThreadData for DstViewportProxy has been updated in DisplayClusterViewportManagerViewExtension on the rendering thread.
	// Therefore, the RenderThreadData values from the game thread must be overridden by current data from the render thread.
	{
		const TArray<FDisplayClusterViewport_Context> CurrentContexts = Contexts;
		Contexts = InViewportProxyData.Contexts;

		int32 ContextAmmount = FMath::Min(CurrentContexts.Num(), Contexts.Num());
		for (int32 ContextIndex = 0; ContextIndex < ContextAmmount; ContextIndex++)
		{
			Contexts[ContextIndex].RenderThreadData = CurrentContexts[ContextIndex].RenderThreadData;
		}
	}

	// Update viewport proxy resources from container
	Resources = InViewportProxyData.Resources;
	ViewStates = InViewportProxyData.ViewStates;
}


//  Return viewport scene proxy resources by type
bool FDisplayClusterViewportProxy::GetResources_RenderThread(const EDisplayClusterViewportResourceType InExtResourceType, TArray<FRHITexture2D*>& OutResources) const
{
	return ImplGetResources_RenderThread(InExtResourceType, OutResources, false);
}

const FDisplayClusterViewportProxy& FDisplayClusterViewportProxy::GetRenderingViewportProxy() const
{
	switch (RenderSettings.GetViewportOverrideMode())
	{
	case EDisplayClusterViewportOverrideMode::All:
	case EDisplayClusterViewportOverrideMode::InernalRTT:
		if (FDisplayClusterViewportManagerProxy* ViewportManagerProxy = ConfigurationProxy->GetViewportManagerProxyImpl())
		{
			if (FDisplayClusterViewportProxy const* OverrideViewportProxy = ViewportManagerProxy->ImplFindViewportProxy_RenderThread(RenderSettings.GetViewportOverrideId()))
			{
				return *OverrideViewportProxy;
			}
		}

		break;
	}

	return *this;
}

EDisplayClusterViewportOpenColorIOMode FDisplayClusterViewportProxy::GetOpenColorIOMode() const
{
	if (OpenColorIO.IsValid() && OpenColorIO->IsValid_RenderThread())
	{
		if (EnumHasAnyFlags(RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard | EDisplayClusterViewportRuntimeICVFXFlags::Lightcard | EDisplayClusterViewportRuntimeICVFXFlags::Chromakey)
			|| ConfigurationProxy->GetRenderFrameSettings().IsPostProcessDisabled())
		{
			// Rendering without post-processing, OCIO is applied last, to the RTT texture of the viewport
			return EDisplayClusterViewportOpenColorIOMode::Resolved;
		}
		else if (RenderSettings.HasAnyMediaStates(EDisplayClusterViewportMediaState::Capture_ForceLateOCIOPass))
		{
			// When capturing a viewport, it's possible that it's going to be shared within a cluster via the media pipeline.
			// In this case we should postpone the OCIO step so every node can apply its own OCIO settings.
			return EDisplayClusterViewportOpenColorIOMode::Resolved;
		}

		// By default, viewports render with a postprocess, OCIO must be done in between.
		return EDisplayClusterViewportOpenColorIOMode::PostProcess;
	}

	return EDisplayClusterViewportOpenColorIOMode::None;
}

void FDisplayClusterViewportProxy::PostResolveViewport_RenderThread(FRHICommandListImmediate& RHICmdList) const
{
	// resolve warped viewport resource to the output texture
	ResolveResources_RenderThread(RHICmdList, EDisplayClusterViewportResourceType::AfterWarpBlendTargetableResource, EDisplayClusterViewportResourceType::OutputTargetableResource);

	// Implement ViewportRemap feature
	ImplViewportRemap_RenderThread(RHICmdList);
}

void FDisplayClusterViewportProxy::ImplViewportRemap_RenderThread(FRHICommandListImmediate& RHICmdList) const
{
	using namespace UE::DisplayCluster::ViewportProxy;

	// Preview in editor not support this feature
	if (ConfigurationProxy->IsPreviewRendering_RenderThread())
	{
		return;
	}

	if (RemapMesh.IsValid())
	{
		const IDisplayClusterRender_MeshComponentProxy* MeshProxy = RemapMesh->GetMeshComponentProxy_RenderThread();
		if (MeshProxy!=nullptr && MeshProxy->IsEnabled_RenderThread())
		{
			if (Resources[EDisplayClusterViewportResource::AdditionalFrameTargetableResources].Num() != Resources[EDisplayClusterViewportResource::OutputFrameTargetableResources].Num())
			{
				// error
				return;
			}

			for (int32 ContextIt = 0; ContextIt < Resources[EDisplayClusterViewportResource::AdditionalFrameTargetableResources].Num(); ContextIt++)
			{
				const TSharedPtr<FDisplayClusterViewportResource, ESPMode::ThreadSafe>& Src = Resources[EDisplayClusterViewportResource::AdditionalFrameTargetableResources][ContextIt];
				const TSharedPtr<FDisplayClusterViewportResource, ESPMode::ThreadSafe>& Dst = Resources[EDisplayClusterViewportResource::OutputFrameTargetableResources][ContextIt];

				FRHITexture2D* Input = Src.IsValid() ? Src->GetViewportResourceRHI_RenderThread() : nullptr;
				FRHITexture2D* Output = Dst.IsValid() ? Dst->GetViewportResourceRHI_RenderThread() : nullptr;

				if (Input && Output)
				{
					GetShadersAPI().RenderPostprocess_OutputRemap(RHICmdList, Input, Output, *MeshProxy);
				}
			}
		}
	}
}

bool FDisplayClusterViewportProxy::GetResourcesWithRects_RenderThread(const EDisplayClusterViewportResourceType InExtResourceType, TArray<FRHITexture2D*>& OutResources, TArray<FIntRect>& OutResourceRects) const
{
	return ImplGetResourcesWithRects_RenderThread(InExtResourceType, OutResources, OutResourceRects, false);
}

bool FDisplayClusterViewportProxy::ApplyOCIO_RenderThread(FRHICommandListImmediate& RHICmdList, const FDisplayClusterViewportProxy& InSrcViewportProxy, const EDisplayClusterViewportResourceType InSrcResourceType) const
{
	if (GetOpenColorIOMode() != EDisplayClusterViewportOpenColorIOMode::Resolved)
	{
		return false;
	}

	const EDisplayClusterViewportResourceType DestResourceType = (InSrcResourceType == EDisplayClusterViewportResourceType::InternalRenderTargetResource)
		? EDisplayClusterViewportResourceType::InputShaderResource
		: EDisplayClusterViewportResourceType::AdditionalTargetableResource;

	TArray<FRHITexture2D*> Input, Output;
	TArray<FIntRect> InputRects, OutputRects;
	if (!InSrcViewportProxy.GetResourcesWithRects_RenderThread(InSrcResourceType, Input, InputRects)
		|| !GetResourcesWithRects_RenderThread(DestResourceType, Output, OutputRects)
		|| Input.Num() != Output.Num())
	{
		return false;
	};

	FRDGBuilder GraphBuilder(RHICmdList);

	bool bResult = false;
	for (int32 ContextNum = 0; ContextNum < Input.Num(); ContextNum++)
	{
		const bool bUnpremultiply = EnumHasAnyFlags(RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard | EDisplayClusterViewportRuntimeICVFXFlags::Lightcard);
		const bool bInvertAlpha = !EnumHasAnyFlags(RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard);

		if (OpenColorIO->AddPass_RenderThread(
			GraphBuilder,
			InSrcViewportProxy.GetContexts_RenderThread()[ContextNum],
			Input[ContextNum], InputRects[ContextNum],
			Output[ContextNum], OutputRects[ContextNum],
			bUnpremultiply,
			bInvertAlpha))
		{
			bResult = true;
		}
	}

	if (bResult)
	{
		GraphBuilder.Execute();

		// copy OCIO results back
		if (DestResourceType != EDisplayClusterViewportResourceType::InputShaderResource)
		{
			ResolveResources_RenderThread(RHICmdList, DestResourceType, EDisplayClusterViewportResourceType::InputShaderResource);
		}
	}

	return bResult;
}

void FDisplayClusterViewportProxy::UpdateDeferredResources(FRHICommandListImmediate& RHICmdList) const
{
	using namespace UE::DisplayCluster::ViewportProxy;
	check(IsInRenderingThread());

	if (RenderSettings.bFreezeRendering || RenderSettings.bSkipRendering)
	{
		// Disable deferred update
		return;
	}

	// Tiled viewports simply copy their RTT to the RTT of the source viewport.
	if (RenderSettings.TileSettings.GetType() == EDisplayClusterViewportTileType::Tile)
	{
		if (FDisplayClusterViewportManagerProxy* ViewportManagerProxy = ConfigurationProxy->GetViewportManagerProxyImpl())
		{
			if (FDisplayClusterViewportProxy const* SourceViewportProxy = ViewportManagerProxy->ImplFindViewportProxy_RenderThread(RenderSettings.TileSettings.GetSourceViewportId()))
			{
				// Copy tile to the source
				ImplResolveTileResource_RenderThread(RHICmdList, SourceViewportProxy);
			}
		}

		// The tile has been copied. This viewport is no longer needed.
		// All of the following logic is applied later, in the tile source viewport.
		return;
	}

	if (RenderSettings.GetViewportOverrideMode() == EDisplayClusterViewportOverrideMode::All)
	{
		// Disable deferred update for clone viewports
		return;
	}

	const FDisplayClusterViewportProxy& SourceViewportProxy = GetRenderingViewportProxy();
	if(!SourceViewportProxy.IsInputRenderTargetResourceExists())
	{
		// No input RTT resource for deferred update
		return;
	}

	EDisplayClusterViewportResourceType SrcResourceType = EDisplayClusterViewportResourceType::InternalRenderTargetResource;

	// pre-Pass 0 (Projection policy):The projection policy can use its own method to resolve 'InternalRenderTargetResource' to 'InputShaderResource'
	if (ProjectionPolicy.IsValid() && ProjectionPolicy->ResolveInternalRenderTargetResource_RenderThread(RHICmdList, this, &SourceViewportProxy))
	{
		SrcResourceType = EDisplayClusterViewportResourceType::InputShaderResource;
	}

	// Pass 0 (OCIO): OCIO support on the first pass for an resolved RTT
	if(!ApplyOCIO_RenderThread(RHICmdList, SourceViewportProxy, SrcResourceType))
	{
		// Pass 0 (default): Resolve from RTT region to separated viewport context resource:
		if (SrcResourceType == EDisplayClusterViewportResourceType::InternalRenderTargetResource)
		{
			ImplResolveResources_RenderThread(RHICmdList, &SourceViewportProxy, SrcResourceType, EDisplayClusterViewportResourceType::InputShaderResource);
		}
	}

	// Pass 1: Generate blur postprocess effect for render target texture rect for all contexts
	if (PostRenderSettings.PostprocessBlur.IsEnabled())
	{
		TArray<FRHITexture2D*> InShaderResources;
		TArray<FRHITexture2D*> OutTargetableResources;
		if (GetResources_RenderThread(EDisplayClusterViewportResourceType::InputShaderResource, InShaderResources) && GetResources_RenderThread(EDisplayClusterViewportResourceType::AdditionalTargetableResource, OutTargetableResources))
		{
			// Render postprocess blur:
			for (int32 ContextNum = 0; ContextNum < InShaderResources.Num(); ContextNum++)
			{
				GetShadersAPI().RenderPostprocess_Blur(RHICmdList, InShaderResources[ContextNum], OutTargetableResources[ContextNum], PostRenderSettings.PostprocessBlur);
			}

			// Copy result back to input
			ResolveResources_RenderThread(RHICmdList, EDisplayClusterViewportResourceType::AdditionalTargetableResource, EDisplayClusterViewportResourceType::InputShaderResource);
		}
	}

	// Pass 2: Create mips texture and generate mips from render target rect for all contexts
	if (PostRenderSettings.GenerateMips.IsEnabled())
	{
		TArray<FRHITexture2D*> InOutMipsResources;
		if (GetResources_RenderThread(EDisplayClusterViewportResourceType::MipsShaderResource, InOutMipsResources))
		{
			// Copy input image to layer0 on mips texture
			ResolveResources_RenderThread(RHICmdList, EDisplayClusterViewportResourceType::InputShaderResource, EDisplayClusterViewportResourceType::MipsShaderResource);

			// Generate mips
			for (FRHITexture2D*& ResourceIt : InOutMipsResources)
			{
				GetShadersAPI().GenerateMips(RHICmdList, ResourceIt, PostRenderSettings.GenerateMips);
			}
		}
	}
}

// Resolve resource contexts
bool FDisplayClusterViewportProxy::ResolveResources_RenderThread(FRHICommandListImmediate& RHICmdList, const EDisplayClusterViewportResourceType InExtResourceType, const EDisplayClusterViewportResourceType OutExtResourceType, const int32 InContextNum) const
{
	return ImplResolveResources_RenderThread(RHICmdList, this, InExtResourceType, OutExtResourceType, InContextNum);
}

bool FDisplayClusterViewportProxy::ResolveResources_RenderThread(FRHICommandListImmediate& RHICmdList, IDisplayClusterViewportProxy* InputResourceViewportProxy, const EDisplayClusterViewportResourceType InExtResourceType, const EDisplayClusterViewportResourceType OutExtResourceType, const int32 InContextNum) const
{
	const FDisplayClusterViewportProxy* SourceProxy = static_cast<FDisplayClusterViewportProxy*>(InputResourceViewportProxy);

	return ImplResolveResources_RenderThread(RHICmdList, SourceProxy , InExtResourceType, OutExtResourceType, InContextNum);
}

void FDisplayClusterViewportProxy::OnResolvedSceneColor_RenderThread(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures, const FDisplayClusterViewportProxy_Context& InProxyContext)
{
	const uint32 InContextNum = InProxyContext.ContextNum;
	if (ShouldUseAlphaChannel_RenderThread())
	{
		switch (ConfigurationProxy->GetRenderFrameSettings().AlphaChannelCaptureMode)
		{
		case EDisplayClusterRenderFrameAlphaChannelCaptureMode::FXAA:
		case EDisplayClusterRenderFrameAlphaChannelCaptureMode::Copy:
		case EDisplayClusterRenderFrameAlphaChannelCaptureMode::CopyAA:
		{
			const FIntRect SrcRect = GetFinalContextRect(EDisplayClusterViewportResourceType::InternalRenderTargetResource, Contexts[InContextNum].RenderTargetRect);
			// Copy alpha channel from 'SceneTextures.Color.Resolve' to 'InputShaderResource'
			CopyResource_RenderThread(GraphBuilder, EDisplayClusterTextureCopyMode::Alpha, InContextNum, SceneTextures.Color.Resolve, SrcRect, EDisplayClusterViewportResourceType::InputShaderResource);
		}
		break;

		default:
			break;
		}
	}
}

FScreenPassTexture FDisplayClusterViewportProxy::OnPostProcessPassAfterSSRInput_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs, const uint32 ContextNum)
{
	FScreenPassTexture OutScreenPassTexture = Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
	if (OutScreenPassTexture.IsValid())
	{
		// Copy alpha channel to 'InputShaderResource'
		const FIntRect SrcRect = GetFinalContextRect(EDisplayClusterViewportResourceType::InternalRenderTargetResource, Contexts[ContextNum].RenderTargetRect);
		CopyResource_RenderThread(GraphBuilder, EDisplayClusterTextureCopyMode::Alpha, ContextNum, OutScreenPassTexture.Texture, SrcRect, EDisplayClusterViewportResourceType::InputShaderResource);
	}

	return OutScreenPassTexture;
}

FScreenPassTexture FDisplayClusterViewportProxy::OnPostProcessPassAfterFXAA_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs, const uint32 ContextNum)
{
	FScreenPassTexture OutScreenPassTexture = Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
	if (OutScreenPassTexture.IsValid())
	{
		// Restore alpha channel after OCIO
		// Copy alpha channel from 'InputShaderResource'
		const FIntRect DestRect = GetFinalContextRect(EDisplayClusterViewportResourceType::InternalRenderTargetResource, Contexts[ContextNum].RenderTargetRect);
		CopyResource_RenderThread(GraphBuilder, EDisplayClusterTextureCopyMode::Alpha, ContextNum, EDisplayClusterViewportResourceType::InputShaderResource, OutScreenPassTexture.Texture, DestRect);
	}

	return OutScreenPassTexture;
}

FScreenPassTexture FDisplayClusterViewportProxy::OnPostProcessPassAfterTonemap_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs, const uint32 ContextNum)
{
	// Perform OCIO rendering after the tonemapper
	if (GetOpenColorIOMode() == EDisplayClusterViewportOpenColorIOMode::PostProcess)
	{
		return OpenColorIO->PostProcessPassAfterTonemap_RenderThread(GraphBuilder, GetContexts_RenderThread()[ContextNum], View, Inputs);
	}

	return Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
}

void FDisplayClusterViewportProxy::OnPostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily, const FSceneView& InSceneView, const FDisplayClusterViewportProxy_Context& InProxyContext)
{
	using namespace UE::DisplayCluster::ViewportProxy;

	const uint32 InContextNum = InProxyContext.ContextNum;

#if WITH_MGPU
	// Get the GPUIndex used to render this viewport
	if (Contexts.IsValidIndex(InContextNum))
	{

		const uint32 GPUIndex = InSceneView.GPUMask.GetFirstIndex();
		Contexts[InContextNum].RenderThreadData.GPUIndex = (GPUIndex < GNumExplicitGPUsForRendering) ? GPUIndex : -1;
	}
#endif

	if (Contexts.IsValidIndex(InContextNum))
	{
		Contexts[InContextNum].RenderThreadData.EngineDisplayGamma = InSceneView.Family->RenderTarget->GetDisplayGamma();
		Contexts[InContextNum].RenderThreadData.EngineShowFlags = InSceneView.Family->EngineShowFlags;
	}

	if (!InProxyContext.ViewFamilyProfileDescription.IsEmpty())
	{
		static IDisplayClusterCallbacks& DCCallbacksAPI = IDisplayCluster::Get().GetCallbacks();
		if (DCCallbacksAPI.OnDisplayClusterPostRenderViewFamily_RenderThread().IsBound())
		{
			// Now we can perform viewport notification
			DCCallbacksAPI.OnDisplayClusterPostRenderViewFamily_RenderThread().Broadcast(GraphBuilder, InViewFamily, this);
		}
	}

	if (ShouldUseAlphaChannel_RenderThread())
	{
		switch (ConfigurationProxy->GetRenderFrameSettings().AlphaChannelCaptureMode)
		{
		case EDisplayClusterRenderFrameAlphaChannelCaptureMode::FXAA:
		{
			// Restore alpha channed
			EFXAAQuality FXAAQuality = EFXAAQuality::Q0;
			if (ShouldApplyFXAA_RenderThread(FXAAQuality) && Resources[EDisplayClusterViewportResource::InputShaderResources].IsValidIndex(InContextNum))
			{
				if (FRHITexture2D* InputTextureRHI = Resources[EDisplayClusterViewportResource::InputShaderResources][InContextNum] ? Resources[EDisplayClusterViewportResource::InputShaderResources][InContextNum]->GetViewportResourceRHI_RenderThread() : nullptr)
				{
					// Apply FXAA for RGB only
					// Note: Add AA for alpha channel

					// Copy Alpha channels back from'InputShaderResource' to 'InternalRenderTargetResource'
					CopyResource_RenderThread(GraphBuilder, EDisplayClusterTextureCopyMode::Alpha, InContextNum, EDisplayClusterViewportResourceType::InputShaderResource, EDisplayClusterViewportResourceType::InternalRenderTargetResource);

					// 1. Copy RGB channels from 'InternalRenderTargetResource' to 'InputShaderResource'
					CopyResource_RenderThread(GraphBuilder, EDisplayClusterTextureCopyMode::RGB, InContextNum, EDisplayClusterViewportResourceType::InternalRenderTargetResource, EDisplayClusterViewportResourceType::InputShaderResource);

					// 2. FXAA render pass to 'InputShaderResource'
					// Input is 'InputShaderResource'
					FRDGTextureRef InputTexture = RegisterExternalTexture(GraphBuilder, InputTextureRHI, TEXT("DCViewportProxyFXAAResource"));
					GraphBuilder.SetTextureAccessFinal(InputTexture, ERHIAccess::RTV);

					FFXAAInputs PassInputs;
					PassInputs.SceneColor = FScreenPassTexture(InputTexture);
					PassInputs.Quality = FXAAQuality;

					// 2.1. Do FXAA
					FScreenPassTexture OutputColorTexture = AddFXAAPass(GraphBuilder, InSceneView, PassInputs);

					// 2.2. Copy FXAA result from 'OutputTexture' to the 'InternalRenderTargetResource'
					if (OutputColorTexture.Texture)
					{
						const FIntVector OutputTextureSize = OutputColorTexture.Texture->Desc.GetSize();
						const FIntRect SrcRect(FIntPoint(0, 0), FIntPoint(OutputTextureSize.X, OutputTextureSize.Y));

						CopyResource_RenderThread(GraphBuilder, EDisplayClusterTextureCopyMode::RGB, InContextNum, OutputColorTexture.Texture, SrcRect, EDisplayClusterViewportResourceType::InternalRenderTargetResource);
					}
				}
				break;
			}
			// don't break (FXAA not used, just copy alpha)
		}

		case EDisplayClusterRenderFrameAlphaChannelCaptureMode::Copy:
		case EDisplayClusterRenderFrameAlphaChannelCaptureMode::CopyAA:
			// Copy Alpha channels back from'InputShaderResource' to 'InternalRenderTargetResource'
			CopyResource_RenderThread(GraphBuilder, EDisplayClusterTextureCopyMode::Alpha, InContextNum, EDisplayClusterViewportResourceType::InputShaderResource, EDisplayClusterViewportResourceType::InternalRenderTargetResource);
			break;

		default:
			break;
		}
	}
}

void FDisplayClusterViewportProxy::ReleaseTextures_RenderThread()
{
	Resources.ReleaseAllResources();
}
