// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/DisplayClusterViewportProxy.h"

#include "Render/Viewport/DisplayClusterViewportManagerProxy.h"
#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewportManagerViewExtension.h"

#include "Render/Viewport/Containers/DisplayClusterViewport_PostRenderSettings.h"
#include "Render/Viewport/Configuration/DisplayClusterViewportConfiguration.h"

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
#include "PostProcess/PostProcessAA.h"
#include "SceneRendering.h"
#include "RenderGraphUtils.h"
#include "RenderGraphResources.h"

#include "ScreenPass.h"
#include "PostProcess/PostProcessing.h"
// for FPostProcessMaterialInputs
#include "PostProcess/PostProcessMaterial.h"

static TAutoConsoleVariable<int32> CVarDisplayClusterRenderOverscanResolve(
	TEXT("nDisplay.render.overscan.resolve"),
	1,
	TEXT("Allow resolve overscan internal rect to output backbuffer.\n")
	TEXT(" 0 - to disable.\n"),
	ECVF_RenderThreadSafe
);

int32 GDisplayClusterShadersICVFXFXAALightCard = 2;
static FAutoConsoleVariableRef CVarDisplayClusterShadersICVFXFXAALightCard(
	TEXT("nDisplay.render.icvfx.fxaa.lightcard"),
	GDisplayClusterShadersICVFXFXAALightCard,
	TEXT("FXAA quality for lightcard (0 - disable).\n")
	TEXT("1..6 - FXAA quality from Lowest Quality(Fastest) to Highest Quality(Slowest).\n"),
	ECVF_RenderThreadSafe
);

int32 GDisplayClusterShadersICVFXFXAAChromakey = 2;
static FAutoConsoleVariableRef CVarDisplayClusterShadersICVFXFXAAChromakey(
	TEXT("nDisplay.render.icvfx.fxaa.chromakey"),
	GDisplayClusterShadersICVFXFXAAChromakey,
	TEXT("FXAA quality for chromakey (0 - disable).\n")
	TEXT("1..6 - FXAA quality from Lowest Quality(Fastest) to Highest Quality(Slowest).\n"),
	ECVF_RenderThreadSafe
);

///////////////////////////////////////////////////////////////////////////////////////
namespace DisplayClusterViewportProxyHelpers
{
	// The viewport override has the maximum depth. This protects against a link cycle
	static const int32 DisplayClusterViewportProxyResourcesOverrideRecursionDepthMax = 4;

	static bool GetFXAAQuality(const EDisplayClusterViewportCaptureMode InCaptureMode, EFXAAQuality& OutFXAAQuality)
	{
		// Get FXAA quality for current viewport
		EFXAAQuality FXAAQuality = EFXAAQuality::MAX;

		switch (InCaptureMode)
		{
		case EDisplayClusterViewportCaptureMode::Chromakey:
			if (GDisplayClusterShadersICVFXFXAAChromakey > 0)
			{
				OutFXAAQuality = (EFXAAQuality)FMath::Clamp(GDisplayClusterShadersICVFXFXAAChromakey - 1, 0, (int32)EFXAAQuality::MAX - 1);
				return true;
			}
			break;

		case EDisplayClusterViewportCaptureMode::Lightcard:
			if (GDisplayClusterShadersICVFXFXAALightCard > 0)
			{
				OutFXAAQuality = (EFXAAQuality)FMath::Clamp(GDisplayClusterShadersICVFXFXAALightCard - 1, 0, (int32)EFXAAQuality::MAX - 1);
				return true;
			}
			break;

		default:
			break;
		}

		// No FXAA
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
	static bool GetViewportRHIResourcesImpl_RenderThread(const TArrayView<FDisplayClusterViewportResource*>& InResources, TArray<FRHITexture2D*>& OutResources)
	{
		OutResources.Reset();

		for (FDisplayClusterViewportResource* ViewportResourceIt : InResources)
		{
			if (FRHITexture2D* RHITexture2D = ViewportResourceIt ? ViewportResourceIt->GetViewportResource2DRHI() : nullptr)
			{
				// Collects only valid resources.
				OutResources.Add(RHITexture2D);
			}
		}

		if (OutResources.Num() != InResources.Num())
		{
			// Some resources lost
			OutResources.Reset();
		}

		// returns success if the number of output resources is equal to the input and is not empty
		return !OutResources.IsEmpty();
	}

	static bool GetViewportRHIResourcesImpl_RenderThread(const TArray<FDisplayClusterViewportTextureResource*>& InResources, TArray<FRHITexture2D*>& OutResources)
	{
		return GetViewportRHIResourcesImpl_RenderThread(TArrayView<FDisplayClusterViewportResource*>((FDisplayClusterViewportResource**)(InResources.GetData()), InResources.Num()), OutResources);
	}

	static bool GetViewportRHIResourcesImpl_RenderThread(const TArray<FDisplayClusterViewportRenderTargetResource*>& InResources, TArray<FRHITexture2D*>& OutResources)
	{
		return GetViewportRHIResourcesImpl_RenderThread(TArrayView<FDisplayClusterViewportResource*>((FDisplayClusterViewportResource**)(InResources.GetData()), InResources.Num()), OutResources);
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
	if (RenderSettings.IsViewportOverrided())
	{
		switch (RenderSettings.ViewportOverrideMode)
		{
		case EDisplayClusterViewportOverrideMode::All:
		{
			switch (InResourceType)
			{
			case EDisplayClusterViewportResourceType::InternalRenderTargetResource:
			case EDisplayClusterViewportResourceType::InputShaderResource:
			case EDisplayClusterViewportResourceType::MipsShaderResource:
			case EDisplayClusterViewportResourceType::AdditionalTargetableResource:
				return true;

			default:
				break;
			}
		}
		break;

		case EDisplayClusterViewportOverrideMode::InernalRTT:
		{
			switch (InResourceType)
			{
			case EDisplayClusterViewportResourceType::InternalRenderTargetResource:
				return true;

			default:
				break;
			}
		}
		break;

		default:
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

const FDisplayClusterViewportProxy& FDisplayClusterViewportProxy::GetRenderingViewportProxy() const
{
	switch (RenderSettings.ViewportOverrideMode)
	{
	case EDisplayClusterViewportOverrideMode::All:
	case EDisplayClusterViewportOverrideMode::InernalRTT:
		if (FDisplayClusterViewportProxy const* OverrideViewportProxy = Owner->ImplFindViewport_RenderThread(RenderSettings.ViewportOverrideId))
		{
			return *OverrideViewportProxy;
		}

		break;
	}

	return *this;
}

bool FDisplayClusterViewportProxy::IsInputRenderTargetResourceExists() const
{
	if (PostRenderSettings.Replace.IsEnabled())
	{
		// Use external texture
		return true;
	}

	if (EnumHasAnyFlags(RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard))
	{
		// Use external UVLightCard Resource
		return true;
	}

	return !RenderTargets.IsEmpty();
}

bool FDisplayClusterViewportProxy::ImplGetResources_RenderThread(const EDisplayClusterViewportResourceType InResourceType, TArray<FRHITexture2D*>& OutResources, const int32 InRecursionDepth) const
{
	check(IsInRenderingThread());

	// Override resources from other viewport
	if (IsShouldOverrideViewportResource(InResourceType))
	{
		if (InRecursionDepth < DisplayClusterViewportProxyResourcesOverrideRecursionDepthMax)
		{
			return GetRenderingViewportProxy().ImplGetResources_RenderThread(InResourceType, OutResources, InRecursionDepth + 1);
		}

		return false;
	}

	OutResources.Empty();

	switch (InResourceType)
	{
	case EDisplayClusterViewportResourceType::InternalRenderTargetResource:
	{
		bool bResult = false;

		if (Contexts.Num() > 0)
		{

			// 1. Replace RTT from configuration
			if (!bResult && PostRenderSettings.Replace.IsEnabled())
			{
				bResult = true;

				// Support texture replace:
				if (FRHITexture2D* ReplaceTextureRHI = PostRenderSettings.Replace.TextureRHI->GetTexture2D())
				{
					for (int32 ContextIndex = 0; ContextIndex < Contexts.Num(); ContextIndex++)
					{
						OutResources.Add(ReplaceTextureRHI);
					}
				}
			}

			// 2. Replace RTT from UVLightCard:
			if (!bResult && EnumHasAnyFlags(RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard))
			{
				bResult = true;
				
				// Get resources from external UV LightCard manager
				TSharedPtr<FDisplayClusterViewportLightCardManagerProxy, ESPMode::ThreadSafe> LightCardManager = Owner->GetLightCardManagerProxy_RenderThread();
				if (LightCardManager.IsValid())
				{
					if (FRHITexture* UVLightCardRHIResource = LightCardManager->GetUVLightCardRHIResource_RenderThread())
					{
						for (int32 ContextIndex = 0; ContextIndex < Contexts.Num(); ContextIndex++)
						{
							OutResources.Add(UVLightCardRHIResource);
						}
					}
				}
			}

			// 3. Finally Use InternalRTT
			if (!bResult)
			{
				bResult = GetViewportRHIResourcesImpl_RenderThread(RenderTargets, OutResources);
			}
		}

		if (!bResult || Contexts.Num() != OutResources.Num())
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

EDisplayClusterViewportOpenColorIOMode FDisplayClusterViewportProxy::GetOpenColorIOMode() const
{
	if (OpenColorIO.IsValid() && OpenColorIO->IsEnabled_RenderThread())
	{
		if (EnumHasAnyFlags(RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard | EDisplayClusterViewportRuntimeICVFXFlags::Lightcard | EDisplayClusterViewportRuntimeICVFXFlags::Chromakey))
		{
			// Rendering without post-processing, OCIO is applied last, to the RTT texture of the viewport
			return EDisplayClusterViewportOpenColorIOMode::Resolved;
		}

		// By default, viewports render with a postprocess, OCIO must be done in between.
		return EDisplayClusterViewportOpenColorIOMode::PostProcess;
	}

	return EDisplayClusterViewportOpenColorIOMode::None;
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

				FRHITexture2D* Input = Src ? Src->GetViewportResource2DRHI() : nullptr;
				FRHITexture2D* Output = Dst ? Dst->GetViewportResource2DRHI() : nullptr;

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
			return GetRenderingViewportProxy().ImplGetResourcesWithRects_RenderThread(InResourceType, OutResources, OutResourceRects, InRecursionDepth + 1);
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

void FDisplayClusterViewportProxy::UpdateDeferredResources(FRHICommandListImmediate& RHICmdList) const
{
	check(IsInRenderingThread());

	if (RenderSettings.bFreezeRendering || RenderSettings.bSkipRendering)
	{
		// Disable deferred update
		return;
	}

	if (RenderSettings.IsViewportOverrided() && RenderSettings.ViewportOverrideMode == EDisplayClusterViewportOverrideMode::All)
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

	bool bPass0Applied = false;

	// OCIO support on the first pass for an resolved RTT
	if (GetOpenColorIOMode() == EDisplayClusterViewportOpenColorIOMode::Resolved)
	{
		TArray<FRHITexture2D*> Input, Output;
		TArray<FIntRect> InputRects, OutputRects;
		if (SourceViewportProxy.GetResourcesWithRects_RenderThread(EDisplayClusterViewportResourceType::InternalRenderTargetResource, Input, InputRects)
			&& GetResourcesWithRects_RenderThread(EDisplayClusterViewportResourceType::InputShaderResource, Output, OutputRects)
			&& Input.Num() == Output.Num())
		{
			FRDGBuilder GraphBuilder(RHICmdList);

			for (int32 ContextNum = 0; ContextNum < Input.Num(); ContextNum++)
			{
				const bool bUnpremultiply = EnumHasAnyFlags(RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard | EDisplayClusterViewportRuntimeICVFXFlags::Lightcard);
				const bool bInvertAlpha = !EnumHasAnyFlags(RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::UVLightcard);

				if (OpenColorIO->AddPass_RenderThread(
					GraphBuilder,
					SourceViewportProxy.GetContexts_RenderThread()[ContextNum],
					Input[ContextNum], InputRects[ContextNum],
					Output[ContextNum], OutputRects[ContextNum],
					bUnpremultiply,
					bInvertAlpha))
				{
					bPass0Applied = true;
				}
			}

			if (bPass0Applied)
			{
				GraphBuilder.Execute();
			}
		}
	}
	
	if(!bPass0Applied)
	{
		// Pass 0: Resolve from RTT region to separated viewport context resource:
		ResolveResources_RenderThread(RHICmdList, EDisplayClusterViewportResourceType::InternalRenderTargetResource, EDisplayClusterViewportResourceType::InputShaderResource);
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
void ResampleCopyTextureImpl_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* SrcTexture, FRHITexture* DstTexture, const FIntRect& SrcRect, const FIntRect& DstRect, const EDisplayClusterTextureCopyMode InCopyMode = EDisplayClusterTextureCopyMode::RGBA)
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

		switch (InCopyMode)
		{
		case EDisplayClusterTextureCopyMode::Alpha:
			// Copy alpha channel from source to dest
			GraphicsPSOInit.BlendState = TStaticBlendState <CW_ALPHA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();
			break;

		case EDisplayClusterTextureCopyMode::RGB:
			// Copy only RGB channels from source to dest
			GraphicsPSOInit.BlendState = TStaticBlendState <CW_RGB, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();
			break;

		case EDisplayClusterTextureCopyMode::RGBA:
		default:
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			break;
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
		ResampleCopyTextureImpl_RenderThread<FScreenPSInvertAlpha>(RHICmdList, InputResource, OutputResource, InputRect, OutputRect);
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
		ResampleCopyTextureImpl_RenderThread<FScreenPS>(RHICmdList, InputResource, OutputResource, InputRect, OutputRect);
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

bool FDisplayClusterViewportProxy::CopyResource_RenderThread(FRDGBuilder& GraphBuilder, const EDisplayClusterTextureCopyMode InCopyMode, const int32 InContextNum, const EDisplayClusterViewportResourceType InSrcResourceType, const EDisplayClusterViewportResourceType InDestResourceType)
{
	TArray<FRHITexture2D*> SrcResources;
	TArray<FIntRect> SrcResourceRects;
	if (GetResourcesWithRects_RenderThread(InSrcResourceType, SrcResources, SrcResourceRects))
	{
		check(SrcResources.IsValidIndex(InContextNum));
		check(SrcResourceRects.IsValidIndex(InContextNum));

		const FIntRect SrcRect = SrcResourceRects[InContextNum];

		FRDGTextureRef SrcTextureRef = RegisterExternalTexture(GraphBuilder, SrcResources[InContextNum]->GetTexture2D(), TEXT("DCViewportProxyCopySrcResource"));
		GraphBuilder.SetTextureAccessFinal(SrcTextureRef, ERHIAccess::SRVGraphics);

		return CopyResource_RenderThread(GraphBuilder, InCopyMode, InContextNum, SrcTextureRef, SrcRect, InDestResourceType);
	}

	return false;
}

BEGIN_SHADER_PARAMETER_STRUCT(FDisplayClusterCopyTextureParameters, )
RDG_TEXTURE_ACCESS(Input, ERHIAccess::CopySrc)
RDG_TEXTURE_ACCESS(Output, ERHIAccess::CopyDest)
END_SHADER_PARAMETER_STRUCT()

bool FDisplayClusterViewportProxy::CopyResource_RenderThread(FRDGBuilder& GraphBuilder, const EDisplayClusterTextureCopyMode InCopyMode, const int32 InContextNum, FRDGTextureRef InSrcTextureRef, const FIntRect& InSrcRect, const EDisplayClusterViewportResourceType InDestResourceType)
{
	TArray<FRHITexture2D*> DestResources;
	TArray<FIntRect> DestResourceRects;
	if (HasBeenProduced(InSrcTextureRef) && GetResourcesWithRects_RenderThread(InDestResourceType, DestResources, DestResourceRects))
	{
		check(DestResources.IsValidIndex(InContextNum));
		check(DestResourceRects.IsValidIndex(InContextNum));

		FRDGTextureRef DestTextureRef = RegisterExternalTexture(GraphBuilder, DestResources[InContextNum]->GetTexture2D(), TEXT("DCViewportProxyCopyDestResource"));
		const FIntRect DestRect = DestResourceRects[InContextNum];

		FDisplayClusterCopyTextureParameters* PassParameters = GraphBuilder.AllocParameters<FDisplayClusterCopyTextureParameters>();
		PassParameters->Input = InSrcTextureRef;
		PassParameters->Output = DestTextureRef;
		GraphBuilder.SetTextureAccessFinal(DestTextureRef, ERHIAccess::RTV);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("DisplayClusterViewportProxy_CopyResource(%s)", *GetId()),
			PassParameters,
			ERDGPassFlags::Copy | ERDGPassFlags::NeverCull,
			[InCopyMode, InContextNum, InSrcTextureRef, InSrcRect, DestTextureRef, DestRect](FRHICommandListImmediate& RHICmdList)
			{
				ResampleCopyTextureImpl_RenderThread<FScreenPS>(RHICmdList, InSrcTextureRef->GetRHI(), DestTextureRef->GetRHI(), InSrcRect, DestRect, InCopyMode);
			});

		return true;
	}

	return false;
}

bool FDisplayClusterViewportProxy::CopyResource_RenderThread(FRDGBuilder& GraphBuilder, const EDisplayClusterTextureCopyMode InCopyMode, const int32 InContextNum, const EDisplayClusterViewportResourceType InSrcResourceType, FRDGTextureRef InDestTextureRef, const FIntRect& InDestRect)
{
	TArray<FRHITexture2D*> SrcResources;
	TArray<FIntRect> SrcResourceRects;
	if (HasBeenProduced(InDestTextureRef) && GetResourcesWithRects_RenderThread(InSrcResourceType, SrcResources, SrcResourceRects))
	{
		check(SrcResources.IsValidIndex(InContextNum));
		check(SrcResourceRects.IsValidIndex(InContextNum));

		FRDGTextureRef SrcTextureRef = RegisterExternalTexture(GraphBuilder, SrcResources[InContextNum]->GetTexture2D(), TEXT("DCViewportProxyCopySrcResource"));
		const FIntRect SrcRect = SrcResourceRects[InContextNum];

		FDisplayClusterCopyTextureParameters* PassParameters = GraphBuilder.AllocParameters<FDisplayClusterCopyTextureParameters>();
		PassParameters->Input = SrcTextureRef;
		PassParameters->Output = InDestTextureRef;
		GraphBuilder.SetTextureAccessFinal(SrcTextureRef, ERHIAccess::RTV);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("DisplayClusterViewportProxy_CopyResource(%s)", *GetId()),
			PassParameters,
			ERDGPassFlags::Copy | ERDGPassFlags::NeverCull,
			[InCopyMode, InContextNum, SrcTextureRef, SrcRect, InDestTextureRef, InDestRect](FRHICommandListImmediate& RHICmdList)
			{
				ResampleCopyTextureImpl_RenderThread<FScreenPS>(RHICmdList, SrcTextureRef->GetRHI(), InDestTextureRef->GetRHI(), SrcRect, InDestRect, InCopyMode);
			});

		return true;
	}

	return false;
}

bool FDisplayClusterViewportProxy::ShouldUseAlphaChannel_RenderThread() const
{
	// Chromakey and Light Cards use alpha channel
	if (EnumHasAnyFlags(RenderSettingsICVFX.RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Lightcard | EDisplayClusterViewportRuntimeICVFXFlags::Chromakey))
	{
		return true;
	}

	return false;
}

bool FDisplayClusterViewportProxy::ShouldUsePostProcessPassAfterSSRInput() const
{	
	if (ShouldUseAlphaChannel_RenderThread())
	{
		return Owner->GetRenderFrameSettings_RenderThread().AlphaChannelCaptureMode == EDisplayClusterRenderFrameAlphaChannelCaptureMode::ThroughTonemapper;
	}

	return false;
}

bool FDisplayClusterViewportProxy::ShouldUsePostProcessPassAfterFXAA() const
{
	if (ShouldUseAlphaChannel_RenderThread())
	{
		return Owner->GetRenderFrameSettings_RenderThread().AlphaChannelCaptureMode == EDisplayClusterRenderFrameAlphaChannelCaptureMode::ThroughTonemapper;
	}

	return false;
}

bool FDisplayClusterViewportProxy::ShouldUsePostProcessPassTonemap() const
{
	if(GetOpenColorIOMode() == EDisplayClusterViewportOpenColorIOMode::PostProcess)
	{
		return true;
	}

	return false;
}

void FDisplayClusterViewportProxy::OnResolvedSceneColor_RenderThread(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures, const FDisplayClusterViewportProxy_Context& InProxyContext)
{
	const uint32 InContextNum = InProxyContext.ContextNum;
	if (ShouldUseAlphaChannel_RenderThread())
	{
		switch (Owner->GetRenderFrameSettings_RenderThread().AlphaChannelCaptureMode)
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
	FScreenPassTexture OutScreenPassTexture = FDisplayClusterViewportManagerViewExtension::ReturnUntouchedSceneColorForPostProcessing(Inputs);
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
	FScreenPassTexture OutScreenPassTexture = FDisplayClusterViewportManagerViewExtension::ReturnUntouchedSceneColorForPostProcessing(Inputs);
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

	return FDisplayClusterViewportManagerViewExtension::ReturnUntouchedSceneColorForPostProcessing(Inputs);
}

void FDisplayClusterViewportProxy::OnPostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily, const FSceneView& InSceneView, const FDisplayClusterViewportProxy_Context& InProxyContext)
{
	const uint32 InContextNum = InProxyContext.ContextNum;

#if WITH_MGPU
	// Get the GPUIndex used to render this viewport
	if (Contexts.IsValidIndex(InContextNum))
	{
		checkSlow(InSceneView.bIsViewInfo);
		const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(InSceneView);

		const uint32 GPUIndex = ViewInfo.GPUMask.GetFirstIndex();
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
		switch (Owner->GetRenderFrameSettings_RenderThread().AlphaChannelCaptureMode)
		{
		case EDisplayClusterRenderFrameAlphaChannelCaptureMode::FXAA:
		{
			// Restore alpha channed
			EFXAAQuality FXAAQuality = EFXAAQuality::Q0;
			if (GetFXAAQuality(RenderSettings.CaptureMode, FXAAQuality) && InputShaderResources.IsValidIndex(InContextNum))
			{
				if (FRHITexture2D* InputTextureRHI = InputShaderResources[InContextNum] ? InputShaderResources[InContextNum]->GetViewportResource2DRHI() : nullptr)
				{
					// Apply FXAA for RGB only
					// Note: Add AA for alpha channel

					// Copy Alpha channels back from'InputShaderResource' to 'InternalRenderTargetResource'
					CopyResource_RenderThread(GraphBuilder, EDisplayClusterTextureCopyMode::Alpha, InContextNum, EDisplayClusterViewportResourceType::InputShaderResource, EDisplayClusterViewportResourceType::InternalRenderTargetResource);

					checkSlow(InSceneView.bIsViewInfo);
					const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(InSceneView);

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
					FScreenPassTexture OutputColorTexture = AddFXAAPass(GraphBuilder, ViewInfo, PassInputs);

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
