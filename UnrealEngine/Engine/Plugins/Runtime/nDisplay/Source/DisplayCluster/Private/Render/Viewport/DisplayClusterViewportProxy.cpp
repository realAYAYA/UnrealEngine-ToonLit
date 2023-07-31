// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/DisplayClusterViewportProxy.h"

#include "Render/Viewport/DisplayClusterViewportManagerProxy.h"
#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_PostRenderSettings.h"
#include "Render/Viewport/RenderTarget/DisplayClusterRenderTargetResource.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfiguration.h"

#include "Render/Projection/IDisplayClusterProjectionPolicy.h"

#include "Render/Containers/IDisplayClusterRender_MeshComponent.h"
#include "Render/Containers/IDisplayClusterRender_MeshComponentProxy.h"

#include "IDisplayCluster.h"
#include "IDisplayClusterCallbacks.h"
#include "IDisplayClusterShaders.h"

#if WITH_EDITOR
#include "DisplayClusterRootActor.h"
#include "Render/Viewport/Containers/DisplayClusterViewportReadPixels.h"
#endif

#include "RHIStaticStates.h"

#include "RenderResource.h"
#include "RenderingThread.h"
#include "CommonRenderResources.h"
#include "PixelShaderUtils.h"

#include "GlobalShader.h"
#include "ShaderParameters.h"
#include "ShaderParameterUtils.h"

#include "ScreenRendering.h"
#include "PostProcess/SceneFilterRendering.h"
#include "SceneRendering.h"
#include "RenderGraphUtils.h"
#include "RenderGraphResources.h"

static TAutoConsoleVariable<int32> CVarDisplayClusterRenderOverscanResolve(
	TEXT("nDisplay.render.overscan.resolve"),
	1,
	TEXT("Allow resolve overscan internal rect to output backbuffer.\n")
	TEXT(" 0 - to disable.\n"),
	ECVF_RenderThreadSafe
);

///////////////////////////////////////////////////////////////////////////////////////
namespace DisplayClusterViewportProxyHelpers
{
	// The viewport override has the maximum depth. This protects against a link cycle
	static const int32 DisplayClusterViewportProxyResourcesOverrideRecursionDepthMax = 4;

	/**
	* Get viewport RHI resources
	*
	* @param InResources - Array with DC resources (for mono num=1, for stereo num=2)
	* @param OutResources - Array with RHI resources
	*
	* @return true, if all resources are valid
	*/
	static bool GetViewportRHIResourcesImpl_RenderThread(const TArray<FDisplayClusterViewportTextureResource*>& InResources, TArray<FRHITexture2D*>& OutResources)
	{
		check(OutResources.Num() == 0);

		if (InResources.Num() > 0)
		{
			for (int32 ResourceIndex = 0; ResourceIndex < InResources.Num(); ResourceIndex++)
			{
				if (InResources[ResourceIndex] != nullptr)
				{
					OutResources.Add(InResources[ResourceIndex]->GetViewportResourceRHI());
				}

			}

			if (OutResources.Num() == InResources.Num())
			{
				return true;
			}

			// Some resources lost
			OutResources.Empty();
		}

		return false;
	}

	/**
	* Get viewport RHI resources
	*
	* @param InResources - Array with DC resources (for mono num=1, for stereo num=2)
	* @param OutResources - Array with RHI resources
	*
	* @return true, if all resources are valid
	*/
	static bool GetViewportRHIResourcesImpl_RenderThread(const TArray<FTextureRenderTargetResource*>& InResources, TArray<FRHITexture2D*>& OutResources)
	{
		check(OutResources.Num() == 0);

		if (InResources.Num() > 0)
		{
			for (int32 ResourceIndex = 0; ResourceIndex < InResources.Num(); ResourceIndex++)
			{
				if (InResources[ResourceIndex] != nullptr)
				{
					OutResources.Add(InResources[ResourceIndex]->GetTexture2DRHI());
				}
			}

			if (OutResources.Num() == InResources.Num())
			{
				return true;
			}

			// Some resources lost
			OutResources.Empty();
		}

		return false;
	}

	// Reset reference to deleted resource
	static void HandleResourceDeleteImpl_RenderThread(TArray<FDisplayClusterViewportTextureResource*>& InResources, const FDisplayClusterViewportResource* InDeletedResourcePtr)
	{
		for (int32 Index = 0; Index < InResources.Num(); Index++)
		{
			if (InResources[Index] == InDeletedResourcePtr)
			{
				// remove references to a deleted resource
				InResources[Index] = nullptr;
			}
		}
	}

	// Reset reference to deleted resource
	static void HandleResourceDeleteImpl_RenderThread(TArray<FDisplayClusterViewportRenderTargetResource*>& InResources, const FDisplayClusterViewportResource* InDeletedResourcePtr)
	{
		for (int32 Index = 0; Index < InResources.Num(); Index++)
		{
			if (InResources[Index] == InDeletedResourcePtr)
			{
				// remove references to a deleted resource
				InResources[Index] = nullptr;
			}
		}
	}
};
using namespace DisplayClusterViewportProxyHelpers;

///////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterViewportProxy::FDisplayClusterViewportProxy(const FDisplayClusterViewport& RenderViewport)
	: ViewportId(RenderViewport.ViewportId)
	, ClusterNodeId(RenderViewport.ClusterNodeId)
	, RenderSettings(RenderViewport.RenderSettings)
	, ProjectionPolicy(RenderViewport.UninitializedProjectionPolicy)
	, Owner(RenderViewport.ImplGetOwner().GetViewportManagerProxy().ToSharedRef())
	, ShadersAPI(IDisplayClusterShaders::Get())
{
	check(ProjectionPolicy.IsValid());
}

FDisplayClusterViewportProxy::~FDisplayClusterViewportProxy()
{
}

const IDisplayClusterViewportManagerProxy& FDisplayClusterViewportProxy::GetOwner_RenderThread() const
{
	check(IsInRenderingThread());

	return Owner.Get();
}

void FDisplayClusterViewportProxy::HandleResourceDelete_RenderThread(FDisplayClusterViewportResource* InDeletedResourcePtr)
{
	// Reset all references to deleted resource
	HandleResourceDeleteImpl_RenderThread(RenderTargets, InDeletedResourcePtr);
	HandleResourceDeleteImpl_RenderThread(OutputFrameTargetableResources, InDeletedResourcePtr);
	HandleResourceDeleteImpl_RenderThread(AdditionalFrameTargetableResources, InDeletedResourcePtr);

	HandleResourceDeleteImpl_RenderThread(InputShaderResources, InDeletedResourcePtr);
	HandleResourceDeleteImpl_RenderThread(AdditionalTargetableResources, InDeletedResourcePtr);
	HandleResourceDeleteImpl_RenderThread(MipsShaderResources, InDeletedResourcePtr);
}


bool FDisplayClusterViewportProxy::IsShouldOverrideViewportResource(const EDisplayClusterViewportResourceType InResourceType) const
{
	// Override resources from other viewport
	if (RenderSettings.OverrideViewportId.IsEmpty() == false)
	{
		switch (InResourceType)
		{
		case EDisplayClusterViewportResourceType::InternalRenderTargetResource:
		case EDisplayClusterViewportResourceType::InputShaderResource:
		case EDisplayClusterViewportResourceType::MipsShaderResource:
		case EDisplayClusterViewportResourceType::AdditionalTargetableResource:
			return true;

		default:
			// By default use all output resources from this viewport
			break;
		}
	}

	return false;
}

//  Return viewport scene proxy resources by type
bool FDisplayClusterViewportProxy::GetResources_RenderThread(const EDisplayClusterViewportResourceType InResourceType, TArray<FRHITexture2D*>& OutResources) const
{
	return ImplGetResources_RenderThread(InResourceType, OutResources, false);
}

bool FDisplayClusterViewportProxy::ImplGetResources_RenderThread(const EDisplayClusterViewportResourceType InResourceType, TArray<FRHITexture2D*>& OutResources, const int32 InRecursionDepth) const
{
	check(IsInRenderingThread());

	// Override resources from other viewport
	if (IsShouldOverrideViewportResource(InResourceType))
	{
		if (InRecursionDepth < DisplayClusterViewportProxyResourcesOverrideRecursionDepthMax)
		{
			if (FDisplayClusterViewportProxy const* OverrideViewportProxy = Owner->ImplFindViewport_RenderThread(RenderSettings.OverrideViewportId))
			{
				return OverrideViewportProxy->ImplGetResources_RenderThread(InResourceType, OutResources, InRecursionDepth + 1);
			}
		}

		return false;
	}

	OutResources.Empty();

	switch (InResourceType)
	{
	case EDisplayClusterViewportResourceType::InternalRenderTargetResource:
	{
		if (Contexts.Num() > 0)
		{
			for (int32 ContextIt = 0; ContextIt < Contexts.Num(); ContextIt++)
			{
				// Support texture replace:
				if (PostRenderSettings.Replace.IsEnabled())
				{
					OutResources.Add(PostRenderSettings.Replace.TextureRHI->GetTexture2D());
				}
				else
				{
					if (ContextIt < RenderTargets.Num())
					{
						if (FDisplayClusterViewportRenderTargetResource* Input = RenderTargets[ContextIt])
						{
							OutResources.Add(Input->GetViewportRenderTargetResourceRHI());
						}
					}
				}
			}
		}

		if (Contexts.Num() != OutResources.Num())
		{
			OutResources.Empty();
		}

		return OutResources.Num() > 0;
	}

	case EDisplayClusterViewportResourceType::InputShaderResource:
		return GetViewportRHIResourcesImpl_RenderThread(InputShaderResources, OutResources);

	case EDisplayClusterViewportResourceType::AdditionalTargetableResource:
		return GetViewportRHIResourcesImpl_RenderThread(AdditionalTargetableResources, OutResources);

	case EDisplayClusterViewportResourceType::MipsShaderResource:
		return GetViewportRHIResourcesImpl_RenderThread(MipsShaderResources, OutResources);

	case EDisplayClusterViewportResourceType::OutputFrameTargetableResource:
		return GetViewportRHIResourcesImpl_RenderThread(OutputFrameTargetableResources, OutResources);

	case EDisplayClusterViewportResourceType::AdditionalFrameTargetableResource:
		return GetViewportRHIResourcesImpl_RenderThread(AdditionalFrameTargetableResources, OutResources);

#if WITH_EDITOR
		// Support preview:
	case EDisplayClusterViewportResourceType::OutputPreviewTargetableResource:
	{
		// Get external resource:
		if (OutputPreviewTargetableResource.IsValid())
		{
			FRHITexture* Texture = OutputPreviewTargetableResource;
			FRHITexture2D* Texture2D = static_cast<FRHITexture2D*>(Texture);
			if (Texture2D != nullptr)
			{
				OutResources.Add(Texture2D);
				return true;
			}
		}
		break;
	}
#endif

	default:
		break;
	}

	return false;
}

void FDisplayClusterViewportProxy::PostResolveViewport_RenderThread(FRHICommandListImmediate& RHICmdList) const
{
	check(IsInRenderingThread());

	// Implement ViewportRemap feature
	ImplViewportRemap_RenderThread(RHICmdList);

	// Implement read pixels for preview DCRA
	ImplPreviewReadPixels_RenderThread(RHICmdList);
}

void FDisplayClusterViewportProxy::ImplViewportRemap_RenderThread(FRHICommandListImmediate& RHICmdList) const
{
#if WITH_EDITOR
	switch(Owner->GetRenderFrameSettings_RenderThread().RenderMode)
	{
	case EDisplayClusterRenderFrameMode::PreviewInScene:
		// Preview in editor not support this feature
		return;
	default:
		break;
	}
#endif

	if (RemapMesh.IsValid())
	{
		const IDisplayClusterRender_MeshComponentProxy* MeshProxy = RemapMesh->GetMeshComponentProxy_RenderThread();
		if (MeshProxy!=nullptr && MeshProxy->IsEnabled_RenderThread())
		{
			if (AdditionalFrameTargetableResources.Num() != OutputFrameTargetableResources.Num())
			{
				// error
				return;
			}

			for (int32 ContextIt = 0; ContextIt < AdditionalFrameTargetableResources.Num(); ContextIt++)
			{
				FDisplayClusterViewportTextureResource* Src = AdditionalFrameTargetableResources[ContextIt];
				FDisplayClusterViewportTextureResource* Dst = OutputFrameTargetableResources[ContextIt];

				FRHITexture2D* Input = Src ? Src->GetViewportResourceRHI() : nullptr;
				FRHITexture2D* Output = Dst ? Dst->GetViewportResourceRHI() : nullptr;

				if (Input && Output)
				{
					ShadersAPI.RenderPostprocess_OutputRemap(RHICmdList, Input, Output, *MeshProxy);
				}
			}
		}
	}
}

void FDisplayClusterViewportProxy::ImplPreviewReadPixels_RenderThread(FRHICommandListImmediate& RHICmdList) const
{
#if WITH_EDITOR
	if (RenderSettings.bPreviewReadPixels)
	{
		bPreviewReadPixels = true;
	}

	// Now try to read until success:
	if (bPreviewReadPixels && Contexts.Num() > 0 && OutputPreviewTargetableResource.IsValid())
	{
		check(Owner->GetRenderFrameSettings_RenderThread().RenderMode == EDisplayClusterRenderFrameMode::PreviewInScene);

		// We should synchronize thread for preview read
		FScopeLock Lock(&PreviewPixelsCSGuard);

		if (PreviewPixels.IsValid() == false)
		{
			// Read pixels from this texture
			FRHITexture* Texture = OutputPreviewTargetableResource;
			if (FRHITexture2D* Texture2D = static_cast<FRHITexture2D*>(Texture))
			{
				
				// Clear deferred read flag
				bPreviewReadPixels = false;

				TSharedPtr<FDisplayClusterViewportReadPixelsData, ESPMode::ThreadSafe> ReadData = MakeShared<FDisplayClusterViewportReadPixelsData, ESPMode::ThreadSafe>();

				RHICmdList.ReadSurfaceData(
					Texture,
					FIntRect(FIntPoint(0, 0), Texture2D->GetSizeXY()),
					ReadData->Pixels,
					FReadSurfaceDataFlags(RCM_UNorm, CubeFace_MAX)
				);

				ReadData->Size = Texture2D->GetSizeXY();

				// Expose result:
				PreviewPixels = ReadData;
			}
		}
	}
#endif
}

#if WITH_EDITOR
bool FDisplayClusterViewportProxy::GetPreviewPixels_GameThread(TSharedPtr<FDisplayClusterViewportReadPixelsData, ESPMode::ThreadSafe>& OutPixelsData) const
{
	check(IsInGameThread());

	// We should synchronize thread for preview read
	FScopeLock Lock(&PreviewPixelsCSGuard);

	if (PreviewPixels.IsValid())
	{
		OutPixelsData = PreviewPixels;
		PreviewPixels.Reset();
		return true;
	}

	return false;
}
#endif

EDisplayClusterViewportResourceType FDisplayClusterViewportProxy::GetOutputResourceType_RenderThread() const
{
	check(IsInRenderingThread());

#if WITH_EDITOR
	switch(Owner->GetRenderFrameSettings_RenderThread().RenderMode)
	{
	case EDisplayClusterRenderFrameMode::PreviewInScene:
		return EDisplayClusterViewportResourceType::OutputPreviewTargetableResource;

	default:
		break;
	}
#endif

	if (RemapMesh.IsValid())
	{
		const IDisplayClusterRender_MeshComponentProxy* MeshProxy = RemapMesh->GetMeshComponentProxy_RenderThread();
		if (MeshProxy!=nullptr && MeshProxy->IsEnabled_RenderThread())
		{
			// In this case render to additional frame targetable
			return EDisplayClusterViewportResourceType::AdditionalFrameTargetableResource;
		}
	}

	return EDisplayClusterViewportResourceType::OutputFrameTargetableResource;
}

bool FDisplayClusterViewportProxy::GetResourcesWithRects_RenderThread(const EDisplayClusterViewportResourceType InResourceType, TArray<FRHITexture2D*>& OutResources, TArray<FIntRect>& OutResourceRects) const
{
	return ImplGetResourcesWithRects_RenderThread(InResourceType, OutResources, OutResourceRects, false);
}

bool FDisplayClusterViewportProxy::ImplGetResourcesWithRects_RenderThread(const EDisplayClusterViewportResourceType InResourceType, TArray<FRHITexture2D*>& OutResources, TArray<FIntRect>& OutResourceRects, const int32 InRecursionDepth) const
{
	check(IsInRenderingThread());

	// Override resources from other viewport
	if(IsShouldOverrideViewportResource(InResourceType))
	{
		if (InRecursionDepth < DisplayClusterViewportProxyResourcesOverrideRecursionDepthMax)
		{
			if (FDisplayClusterViewportProxy const* OverrideViewportProxy = Owner->ImplFindViewport_RenderThread(RenderSettings.OverrideViewportId))
			{
				return OverrideViewportProxy->ImplGetResourcesWithRects_RenderThread(InResourceType, OutResources, OutResourceRects, InRecursionDepth + 1);
			}
		}

		return false;
	}

	if (!GetResources_RenderThread(InResourceType, OutResources))
	{
		return false;
	}

	// Collect all resource rects:
	OutResourceRects.AddDefaulted(OutResources.Num());

	switch (InResourceType)
	{
	case EDisplayClusterViewportResourceType::InternalRenderTargetResource:
		for (int32 ContextIt = 0; ContextIt < OutResourceRects.Num(); ContextIt++)
		{
			if (PostRenderSettings.Replace.IsEnabled())
			{
				// Get image from Override
				OutResourceRects[ContextIt] = PostRenderSettings.Replace.Rect;
			}
			else
			{
				OutResourceRects[ContextIt] = Contexts[ContextIt].RenderTargetRect;
			}
		}
		break;
	case EDisplayClusterViewportResourceType::OutputFrameTargetableResource:
	case EDisplayClusterViewportResourceType::AdditionalFrameTargetableResource:
		for (int32 ContextIt = 0; ContextIt < OutResourceRects.Num(); ContextIt++)
		{
			OutResourceRects[ContextIt] = Contexts[ContextIt].FrameTargetRect;
		}
		break;
	default:
		for (int32 ContextIt = 0; ContextIt < OutResourceRects.Num(); ContextIt++)
		{
			OutResourceRects[ContextIt] = FIntRect(FIntPoint(0, 0), OutResources[ContextIt]->GetSizeXY());
		}
	}

	return true;
}

// Apply postprocess, generate mips, etc from settings in FDisplayClusterViewporDeferredUpdateSettings
void FDisplayClusterViewportProxy::UpdateDeferredResources(FRHICommandListImmediate& RHICmdList) const
{
	check(IsInRenderingThread());

	if (RenderTargets.Num() == 0 && PostRenderSettings.Replace.IsEnabled() == false)
	{
		// Internal RTT required for deferred update. Except when use 'Replace' as source
		return;
	}

	if (RenderSettings.bFreezeRendering || RenderSettings.bSkipRendering || RenderSettings.OverrideViewportId.IsEmpty() == false)
	{
		// Disable deferred update
		return;
	}

	// Pass 0: Resolve from RTT region to separated viewport context resource:
	ResolveResources_RenderThread(RHICmdList, EDisplayClusterViewportResourceType::InternalRenderTargetResource, EDisplayClusterViewportResourceType::InputShaderResource);

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
				ShadersAPI.RenderPostprocess_Blur(RHICmdList, InShaderResources[ContextNum], OutTargetableResources[ContextNum], PostRenderSettings.PostprocessBlur);
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
				ShadersAPI.GenerateMips(RHICmdList, ResourceIt, PostRenderSettings.GenerateMips);
			}
		}
	}
}

template<class TScreenPixelShader>
void ResampleCopyTextureImpl_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* SrcTexture, FRHITexture* DstTexture, const FIntRect& SrcRect, const FIntRect& DstRect, const bool bCopyOnlyAlphaChannel)
{
	// Texture format mismatch, use a shader to do the copy.
	// #todo-renderpasses there's no explicit resolve here? Do we need one?
	FRHIRenderPassInfo RPInfo(DstTexture, ERenderTargetActions::Load_Store);
	RHICmdList.Transition(FRHITransitionInfo(DstTexture, ERHIAccess::Unknown, ERHIAccess::RTV));

	RHICmdList.BeginRenderPass(RPInfo, TEXT("DisplayCluster_ResampleCopyTexture"));
	{
		FIntVector SrcSizeXYZ = SrcTexture->GetSizeXYZ();
		FIntVector DstSizeXYZ = DstTexture->GetSizeXYZ();

		FIntPoint SrcSize(SrcSizeXYZ.X, SrcSizeXYZ.Y);
		FIntPoint DstSize(DstSizeXYZ.X, DstSizeXYZ.Y);

		RHICmdList.SetViewport(0.f, 0.f, 0.0f, DstSize.X, DstSize.Y, 1.0f);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		if(bCopyOnlyAlphaChannel)
		{
			// Copy alpha channel from source to dest
			// RT0ColorWriteMask,RT0ColorBlendOp,RT0ColorSrcBlend,RT0ColorDestBlend,RT0AlphaBlendOp,RT0AlphaSrcBlend,RT0AlphaDestBlend,
			GraphicsPSOInit.BlendState = TStaticBlendState <CW_ALPHA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();
		}
		else
		{
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		}

		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();




		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
		TShaderMapRef<TScreenPixelShader> PixelShader(ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

		if (SrcRect.Size() != DstRect.Size())
		{
			PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), SrcTexture);
		}
		else
		{
			PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(), SrcTexture);
		}

		// Set up vertex uniform parameters for scaling and biasing the rectangle.
		// Note: Use DrawRectangle in the vertex shader to calculate the correct vertex position and uv.
		FDrawRectangleParameters Parameters;
		{
			Parameters.PosScaleBias = FVector4f(DstRect.Size().X, DstRect.Size().Y, DstRect.Min.X, DstRect.Min.Y);
			Parameters.UVScaleBias = FVector4f(SrcRect.Size().X, SrcRect.Size().Y, SrcRect.Min.X, SrcRect.Min.Y);
			Parameters.InvTargetSizeAndTextureSize = FVector4f(1.0f / DstSize.X, 1.0f / DstSize.Y, 1.0f / SrcSize.X, 1.0f / SrcSize.Y);

			SetUniformBufferParameterImmediate(RHICmdList, VertexShader.GetVertexShader(), VertexShader->GetUniformBufferParameter<FDrawRectangleParameters>(), Parameters);
		}

		FPixelShaderUtils::DrawFullscreenQuad(RHICmdList, 1);
	}
	RHICmdList.EndRenderPass();
	RHICmdList.Transition(FRHITransitionInfo(DstTexture, ERHIAccess::Unknown, ERHIAccess::SRVMask));
}

void ImplResolveResource(FRHICommandListImmediate& RHICmdList, FRHITexture2D* InputResource, const FIntRect& InputRect, FRHITexture2D* OutputResource, const FIntRect& OutputRect, const bool bOutputIsMipsResource, const bool bOutputIsPreviewResource)
{
	check(InputResource);
	check(OutputResource);

	if (bOutputIsPreviewResource)
	{
		// Preview require a normal alpha (re-invert)
		ResampleCopyTextureImpl_RenderThread<FScreenPSInvertAlpha>(RHICmdList, InputResource, OutputResource, InputRect, OutputRect, false);
	}
	else if (InputRect.Size() == OutputRect.Size() && InputResource->GetFormat() == OutputResource->GetFormat())
	{
		FRHICopyTextureInfo CopyInfo;
		CopyInfo.Size = FIntVector(InputRect.Width(), InputRect.Height(), 0);
		CopyInfo.SourcePosition.X = InputRect.Min.X;
		CopyInfo.SourcePosition.Y = InputRect.Min.Y;
		CopyInfo.DestPosition.X = OutputRect.Min.X;
		CopyInfo.DestPosition.Y = OutputRect.Min.Y;

		TransitionAndCopyTexture(RHICmdList, InputResource, OutputResource, CopyInfo);
	}
	else
	{
		ResampleCopyTextureImpl_RenderThread<FScreenPS>(RHICmdList, InputResource, OutputResource, InputRect, OutputRect, false);
	}
}

FIntRect FDisplayClusterViewportProxy::GetFinalContextRect(const EDisplayClusterViewportResourceType InputResourceType, const FIntRect& InRect) const
{
	// When resolving without warp, apply overscan
	switch (InputResourceType)
	{
	case EDisplayClusterViewportResourceType::InternalRenderTargetResource:
		if (OverscanSettings.bIsEnabled && CVarDisplayClusterRenderOverscanResolve.GetValueOnRenderThread() != 0)
		{
			// Support overscan crop
			return OverscanSettings.OverscanPixels.GetInnerRect(InRect);
		}
		break;
	default:
		break;
	}

	return InRect;
}

// Resolve resource contexts
bool FDisplayClusterViewportProxy::ResolveResources_RenderThread(FRHICommandListImmediate& RHICmdList, const EDisplayClusterViewportResourceType InputResourceType, const EDisplayClusterViewportResourceType OutputResourceType, const int32 InContextNum) const
{
	return ImplResolveResources_RenderThread(RHICmdList, this, InputResourceType, OutputResourceType, InContextNum);
}

bool FDisplayClusterViewportProxy::ImplResolveResources_RenderThread(FRHICommandListImmediate& RHICmdList, FDisplayClusterViewportProxy const* SourceProxy, const EDisplayClusterViewportResourceType InputResourceType, const EDisplayClusterViewportResourceType OutputResourceType, const int32 InContextNum) const
{
	check(IsInRenderingThread());

	if (InputResourceType == EDisplayClusterViewportResourceType::MipsShaderResource) {
		// RenderTargetMips not allowved for resolve op
		return false;
	}

	bool bOutputIsMipsResource = OutputResourceType == EDisplayClusterViewportResourceType::MipsShaderResource;

#if WITH_EDITOR
	// This resolve pattern always called once for preview. This flag force to invert alpha (at this point alpha from engine is inverted)
	const bool bOutputIsPreviewResource = InputResourceType == EDisplayClusterViewportResourceType::InputShaderResource && OutputResourceType == EDisplayClusterViewportResourceType::OutputPreviewTargetableResource;
#else
	const bool bOutputIsPreviewResource = false;
#endif

	TArray<FRHITexture2D*> SrcResources, DestResources;
	TArray<FIntRect> SrcResourcesRect, DestResourcesRect;
	if (SourceProxy->GetResourcesWithRects_RenderThread(InputResourceType, SrcResources, SrcResourcesRect) && GetResourcesWithRects_RenderThread(OutputResourceType, DestResources, DestResourcesRect))
	{
		const int32 SrcResourcesAmount = FMath::Min(SrcResources.Num(), DestResources.Num());
		for (int32 SrcResourceContextIndex = 0; SrcResourceContextIndex < SrcResourcesAmount; SrcResourceContextIndex++)
		{
			// Get context num to copy
			const int32 SrcContextNum = (InContextNum == INDEX_NONE) ? SrcResourceContextIndex : InContextNum;
			const FIntRect SrcRect = GetFinalContextRect(InputResourceType, SrcResourcesRect[SrcContextNum]);

			if ((SrcContextNum + 1) == SrcResourcesAmount)
			{
				// last input mono -> stereo outputs
				for (int32 DestResourceContextIndex = SrcContextNum; DestResourceContextIndex < DestResources.Num(); DestResourceContextIndex++)
				{
					const FIntRect DestRect = GetFinalContextRect(OutputResourceType, DestResourcesRect[DestResourceContextIndex]);
					ImplResolveResource(RHICmdList, SrcResources[SrcContextNum], SrcRect, DestResources[DestResourceContextIndex], DestRect, bOutputIsMipsResource, bOutputIsPreviewResource);
				}
				break;
			}
			else
			{
				const FIntRect DestRect = GetFinalContextRect(OutputResourceType, DestResourcesRect[SrcContextNum]);
				ImplResolveResource(RHICmdList, SrcResources[SrcContextNum], SrcRect, DestResources[SrcContextNum], DestRect, bOutputIsMipsResource, bOutputIsPreviewResource);
			}

			if (InContextNum != INDEX_NONE)
			{
				// Copy only one texture
				break;
			}
		}

		return true;
	}

	return false;
}

void FDisplayClusterViewportProxy::OnResolvedSceneColor_RenderThread(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures, const FDisplayClusterViewportProxy_Context& InProxyContext)
{ }

void FDisplayClusterViewportProxy::PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily, const FSceneView& InSceneView, const FDisplayClusterViewportProxy_Context& InProxyContext)
{
	const uint32 InContextNum = InProxyContext.ContextNum;

	// Save GPUMask for each viewport context
	if (Contexts.IsValidIndex(InContextNum))
	{
		if (GPUMask.Num() < Contexts.Num())
		{
			GPUMask.AddDefaulted(Contexts.Num() - GPUMask.Num());
		}

		checkSlow(InSceneView.bIsViewInfo);
		const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(InSceneView);

		GPUMask[InContextNum] = ViewInfo.GPUMask;
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
}
