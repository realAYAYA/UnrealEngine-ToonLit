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

#include "ClearQuad.h"

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

// Tile rect border width
int32 GDisplayClusterRenderTileBorder = 0;
static FAutoConsoleVariableRef CVarDisplayClusterRenderTileBorder(
	TEXT("nDisplay.render.TileBorder"),
	GDisplayClusterRenderTileBorder,
	TEXT("Tile border width in pixels (default 0).\n"),
	ECVF_RenderThreadSafe
);

///////////////////////////////////////////////////////////////////////////////////////
namespace UE::DisplayCluster::ViewportProxy
{
	// The viewport override has the maximum depth. This protects against a link cycle
	static const int32 DisplayClusterViewportProxyResourcesOverrideRecursionDepthMax = 4;

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
			if (!VertexShader.IsValid() || !PixelShader.IsValid())
			{
				// Always check if shaders are available on the current platform and hardware
				return;
			}

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			const bool bSameSize = (SrcRect.Size() == DstRect.Size());
			FRHISamplerState* PixelSampler = bSameSize ? TStaticSamplerState<SF_Point>::GetRHI() : TStaticSamplerState<SF_Bilinear>::GetRHI();

			SetShaderParametersLegacyPS(RHICmdList, PixelShader, PixelSampler, SrcTexture);

			// Set up vertex uniform parameters for scaling and biasing the rectangle.
			// Note: Use DrawRectangle in the vertex shader to calculate the correct vertex position and uv.

			UE::Renderer::PostProcess::DrawRectangle(
				RHICmdList, VertexShader,
				DstRect.Min.X, DstRect.Min.Y,
				DstRect.Size().X, DstRect.Size().Y,
				SrcRect.Min.X, SrcRect.Min.Y,
				SrcRect.Size().X, SrcRect.Size().Y,
				DstSize, SrcSize
			);
		}
		RHICmdList.EndRenderPass();
		RHICmdList.Transition(FRHITransitionInfo(DstTexture, ERHIAccess::Unknown, ERHIAccess::SRVMask));
	}

	static void ImplResolveResource(FRHICommandListImmediate& RHICmdList, FRHITexture2D* InputResource, const FIntRect& InputRect, FRHITexture2D* OutputResource, const FIntRect& OutputRect, const bool bOutputIsMipsResource, const bool bOutputIsPreviewResource)
	{
		check(InputResource);
		check(OutputResource);

		if (bOutputIsPreviewResource)
		{
			// The preview texture should use only RGB colors and ignore the alpha channel. The alpha channel may or may not be inverted in third-party libraries.
			ResampleCopyTextureImpl_RenderThread<FScreenPS>(RHICmdList, InputResource, OutputResource, InputRect, OutputRect, EDisplayClusterTextureCopyMode::RGB);
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

	struct FViewportResourceResolverData
	{
		FViewportResourceResolverData(const FDisplayClusterViewport_Context& InViewportContext, FRHITexture2D* InInputResource, const FIntRect& InInputRect, FRHITexture2D* InOutputResource, const FIntRect& InOutputRect, const bool InbOutputIsMipsResource, const bool InbOutputIsPreviewResource)
			: InputResource(InInputResource)
			, OutputResource(InOutputResource)
			, InputRect(InInputRect)
			, OutputRect(InOutputRect)
			, bOutputIsMipsResource(InbOutputIsMipsResource)
			, bOutputIsPreviewResource(InbOutputIsPreviewResource)
			, ViewportContext(InViewportContext)
		{ }

		void Resolve_RenderThread(FRHICommandListImmediate& RHICmdList)
		{
			ImplResolveResource(RHICmdList, InputResource, InputRect, OutputResource, OutputRect, bOutputIsMipsResource, bOutputIsPreviewResource);
		}

		void AddFinalPass_RenderThread(FRDGBuilder& GraphBuilder, IDisplayClusterDisplayDeviceProxy& DisplayDevice)
		{
			DisplayDevice.AddFinalPass_RenderThread(GraphBuilder, ViewportContext, InputResource, InputRect, OutputResource, OutputRect);
		}

	private:
		FRHITexture2D* InputResource;
		FRHITexture2D* OutputResource;

		const FIntRect InputRect;
		const FIntRect OutputRect;

		const bool bOutputIsMipsResource;
		const bool bOutputIsPreviewResource;

		const FDisplayClusterViewport_Context ViewportContext;
	};
};

void FDisplayClusterViewportProxy::FillTextureWithColor_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture2D* InRenderTargetTexture, const FLinearColor& InColor)
{
	if (InRenderTargetTexture)
	{
		FRHIRenderPassInfo RPInfo(InRenderTargetTexture, ERenderTargetActions::DontLoad_Store);
		RHICmdList.Transition(FRHITransitionInfo(InRenderTargetTexture, ERHIAccess::Unknown, ERHIAccess::RTV));
		RHICmdList.BeginRenderPass(RPInfo, TEXT("nDisplay_FillTextureWithColor"));
		{
			const FIntPoint Size = InRenderTargetTexture->GetSizeXY();
			RHICmdList.SetViewport(0, 0, 0.0f, Size.X, Size.Y, 1.0f);
			DrawClearQuad(RHICmdList, FLinearColor::Black);
		}
		RHICmdList.EndRenderPass();
		RHICmdList.Transition(FRHITransitionInfo(InRenderTargetTexture, ERHIAccess::Unknown, ERHIAccess::SRVMask));
	}
}

bool FDisplayClusterViewportProxy::ImplGetResources_RenderThread(const EDisplayClusterViewportResourceType InExtResourceType, TArray<FRHITexture2D*>& OutResources, const int32 InRecursionDepth) const
{
	using namespace UE::DisplayCluster::ViewportProxy;
	check(IsInRenderingThread());

	const EDisplayClusterViewportResourceType InResourceType = GetResourceType_RenderThread(InExtResourceType);

	// Override resources from other viewport
	if (ShouldOverrideViewportResource(InResourceType))
	{
		if (InRecursionDepth < DisplayClusterViewportProxyResourcesOverrideRecursionDepthMax)
		{
			return GetRenderingViewportProxy().ImplGetResources_RenderThread(InExtResourceType, OutResources, InRecursionDepth + 1);
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
				if (FDisplayClusterViewportManagerProxy* ViewportManagerProxy = ConfigurationProxy->GetViewportManagerProxyImpl())
				{
					TSharedPtr<FDisplayClusterViewportLightCardManagerProxy, ESPMode::ThreadSafe> LightCardManager = ViewportManagerProxy->GetLightCardManagerProxy_RenderThread();
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
			}

			// 3. Finally Use InternalRTT
			if (!bResult)
			{
				bResult = Resources.GetRHIResources_RenderThread(EDisplayClusterViewportResource::RenderTargets, OutResources);
			}
		}

		if (!bResult || Contexts.Num() != OutResources.Num())
		{
			OutResources.Empty();
		}

		return OutResources.Num() > 0;
	}

	case EDisplayClusterViewportResourceType::InputShaderResource:
		return Resources.GetRHIResources_RenderThread(EDisplayClusterViewportResource::InputShaderResources, OutResources);

	case EDisplayClusterViewportResourceType::AdditionalTargetableResource:
		return Resources.GetRHIResources_RenderThread(EDisplayClusterViewportResource::AdditionalTargetableResources, OutResources);

	case EDisplayClusterViewportResourceType::MipsShaderResource:
		return Resources.GetRHIResources_RenderThread(EDisplayClusterViewportResource::MipsShaderResources, OutResources);

	case EDisplayClusterViewportResourceType::OutputFrameTargetableResource:
		return Resources.GetRHIResources_RenderThread(EDisplayClusterViewportResource::OutputFrameTargetableResources, OutResources);

	case EDisplayClusterViewportResourceType::AdditionalFrameTargetableResource:
		return Resources.GetRHIResources_RenderThread(EDisplayClusterViewportResource::AdditionalFrameTargetableResources, OutResources);

	case EDisplayClusterViewportResourceType::OutputPreviewTargetableResource:
		return Resources.GetRHIResources_RenderThread(EDisplayClusterViewportResource::OutputPreviewTargetableResources, OutResources);

	default:
		break;
	}

	return false;
}

bool FDisplayClusterViewportProxy::ImplGetResourcesWithRects_RenderThread(const EDisplayClusterViewportResourceType InExtResourceType, TArray<FRHITexture2D*>& OutResources, TArray<FIntRect>& OutResourceRects, const int32 InRecursionDepth) const
{
	using namespace UE::DisplayCluster::ViewportProxy;
	check(IsInRenderingThread());

	// Override resources from other viewport
	if (ShouldOverrideViewportResource(InExtResourceType))
	{
		if (InRecursionDepth < DisplayClusterViewportProxyResourcesOverrideRecursionDepthMax)
		{
			return GetRenderingViewportProxy().ImplGetResourcesWithRects_RenderThread(InExtResourceType, OutResources, OutResourceRects, InRecursionDepth + 1);
		}

		return false;
	}

	const EDisplayClusterViewportResourceType InResourceType = GetResourceType_RenderThread(InExtResourceType);

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

bool FDisplayClusterViewportProxy::ImplResolveResources_RenderThread(FRHICommandListImmediate& RHICmdList, FDisplayClusterViewportProxy const* SourceProxy, const EDisplayClusterViewportResourceType InExtResourceType, const EDisplayClusterViewportResourceType OutExtResourceType, const int32 InContextNum) const
{
	using namespace UE::DisplayCluster::ViewportProxy;

	check(IsInRenderingThread());
	check(SourceProxy);

	const EDisplayClusterViewportResourceType InResourceType = SourceProxy->GetResourceType_RenderThread(InExtResourceType);
	const EDisplayClusterViewportResourceType OutResourceType = GetResourceType_RenderThread(OutExtResourceType);

	if (InResourceType == EDisplayClusterViewportResourceType::MipsShaderResource)
	{
		// RenderTargetMips not allowved for resolve op
		return false;
	}

	const bool bOutputIsMipsResource = OutResourceType == EDisplayClusterViewportResourceType::MipsShaderResource;
	const bool bOutputIsPreviewResource = OutResourceType == EDisplayClusterViewportResourceType::OutputPreviewTargetableResource;

	TArray<FViewportResourceResolverData> ResourceResolverData;

	TArray<FRHITexture2D*> SrcResources, DestResources;
	TArray<FIntRect> SrcResourcesRect, DestResourcesRect;
	if (SourceProxy->GetResourcesWithRects_RenderThread(InExtResourceType, SrcResources, SrcResourcesRect) && GetResourcesWithRects_RenderThread(OutExtResourceType, DestResources, DestResourcesRect))
	{
		const int32 SrcResourcesAmount = FMath::Min(SrcResources.Num(), DestResources.Num());
		for (int32 SrcResourceContextIndex = 0; SrcResourceContextIndex < SrcResourcesAmount; SrcResourceContextIndex++)
		{
			// Get context num to copy
			const int32 SrcContextNum = (InContextNum == INDEX_NONE) ? SrcResourceContextIndex : InContextNum;
			const FIntRect SrcRect = SourceProxy->GetFinalContextRect(InExtResourceType, SrcResourcesRect[SrcContextNum]);

			if ((SrcContextNum + 1) == SrcResourcesAmount)
			{
				// last input mono -> stereo outputs
				for (int32 DestResourceContextIndex = SrcContextNum; DestResourceContextIndex < DestResources.Num(); DestResourceContextIndex++)
				{
					const FIntRect DestRect = GetFinalContextRect(OutResourceType, DestResourcesRect[DestResourceContextIndex]);
					ResourceResolverData.Add(FViewportResourceResolverData(Contexts[SrcContextNum], SrcResources[SrcContextNum], SrcRect, DestResources[DestResourceContextIndex], DestRect, bOutputIsMipsResource, bOutputIsPreviewResource));
				}
				break;
			}
			else
			{
				const FIntRect DestRect = GetFinalContextRect(OutResourceType, DestResourcesRect[SrcContextNum]);
				ResourceResolverData.Add(FViewportResourceResolverData(Contexts[SrcContextNum], SrcResources[SrcContextNum], SrcRect, DestResources[SrcContextNum], DestRect, bOutputIsMipsResource, bOutputIsPreviewResource));
			}

			if (InContextNum != INDEX_NONE)
			{
				// Copy only one texture
				break;
			}
		}

		if (InExtResourceType == EDisplayClusterViewportResourceType::AfterWarpBlendTargetableResource
			&& OutExtResourceType == EDisplayClusterViewportResourceType::OutputTargetableResource
			&& DisplayDeviceProxy.IsValid()
			&& DisplayDeviceProxy->HasFinalPass_RenderThread())
		{
			// Custom resolve at external Display Device
			FRDGBuilder GraphBuilder(RHICmdList);
			for (FViewportResourceResolverData& ResourceResolverIt : ResourceResolverData)
			{
				ResourceResolverIt.AddFinalPass_RenderThread(GraphBuilder, *DisplayDeviceProxy.Get());
			}
			GraphBuilder.Execute();
		}
		else
		{
			// Standard resolve:
			for (FViewportResourceResolverData& ResourceResolverIt : ResourceResolverData)
			{
				ResourceResolverIt.Resolve_RenderThread(RHICmdList);
			}
		}

		return true;
	}

	return false;
}

bool FDisplayClusterViewportProxy::CopyResource_RenderThread(FRDGBuilder& GraphBuilder, const EDisplayClusterTextureCopyMode InCopyMode, const int32 InContextNum, const EDisplayClusterViewportResourceType InExtSrcResourceType, const EDisplayClusterViewportResourceType InExtDestResourceType)
{
	TArray<FRHITexture2D*> SrcResources;
	TArray<FIntRect> SrcResourceRects;
	if (GetResourcesWithRects_RenderThread(InExtSrcResourceType, SrcResources, SrcResourceRects))
	{
		check(SrcResources.IsValidIndex(InContextNum));
		check(SrcResourceRects.IsValidIndex(InContextNum));

		const FIntRect SrcRect = SrcResourceRects[InContextNum];

		FRDGTextureRef SrcTextureRef = RegisterExternalTexture(GraphBuilder, SrcResources[InContextNum]->GetTexture2D(), TEXT("DCViewportProxyCopySrcResource"));
		GraphBuilder.SetTextureAccessFinal(SrcTextureRef, ERHIAccess::SRVGraphics);

		return CopyResource_RenderThread(GraphBuilder, InCopyMode, InContextNum, SrcTextureRef, SrcRect, InExtDestResourceType);
	}

	return false;
}

BEGIN_SHADER_PARAMETER_STRUCT(FDisplayClusterCopyTextureParameters, )
RDG_TEXTURE_ACCESS(Input, ERHIAccess::CopySrc)
RDG_TEXTURE_ACCESS(Output, ERHIAccess::CopyDest)
END_SHADER_PARAMETER_STRUCT()

bool FDisplayClusterViewportProxy::CopyResource_RenderThread(FRDGBuilder& GraphBuilder, const EDisplayClusterTextureCopyMode InCopyMode, const int32 InContextNum, FRDGTextureRef InSrcTextureRef, const FIntRect& InSrcRect, const EDisplayClusterViewportResourceType InExtDestResourceType)
{
	using namespace UE::DisplayCluster::ViewportProxy;

	TArray<FRHITexture2D*> DestResources;
	TArray<FIntRect> DestResourceRects;
	if (HasBeenProduced(InSrcTextureRef) && GetResourcesWithRects_RenderThread(InExtDestResourceType, DestResources, DestResourceRects))
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

bool FDisplayClusterViewportProxy::CopyResource_RenderThread(FRDGBuilder& GraphBuilder, const EDisplayClusterTextureCopyMode InCopyMode, const int32 InContextNum, const EDisplayClusterViewportResourceType InExtSrcResourceType, FRDGTextureRef InDestTextureRef, const FIntRect& InDestRect)
{
	using namespace UE::DisplayCluster::ViewportProxy;

	TArray<FRHITexture2D*> SrcResources;
	TArray<FIntRect> SrcResourceRects;
	if (HasBeenProduced(InDestTextureRef) && GetResourcesWithRects_RenderThread(InExtSrcResourceType, SrcResources, SrcResourceRects))
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

void FDisplayClusterViewportProxy::ImplResolveTileResource_RenderThread(FRHICommandListImmediate& RHICmdList, FDisplayClusterViewportProxy const* InDestViewportProxy) const
{
	check(InDestViewportProxy);

	using namespace UE::DisplayCluster::ViewportProxy;

	const TArray<FDisplayClusterViewport_Context>& DestContexts = InDestViewportProxy->GetContexts_RenderThread();
	const TArray<FDisplayClusterViewport_Context>& SrcContexts = Contexts;

	TArray<FRHITexture2D*> SrcResources, DestResources;
	if (GetResources_RenderThread(EDisplayClusterViewportResourceType::InternalRenderTargetResource, SrcResources)
	&& InDestViewportProxy->GetResources_RenderThread(EDisplayClusterViewportResourceType::InternalRenderTargetResource, DestResources)
	&& SrcResources.Num() == DestResources.Num()
	&& SrcContexts.Num() == DestContexts.Num()
	&& SrcResources.Num() == SrcContexts.Num()
	&& !SrcResources.IsEmpty())
	{
		// Copy all contexts:
		for (int32 ContextNum = 0; ContextNum < SrcContexts.Num(); ContextNum++)
		{
			// Get src rect inside overscan
			FIntRect SrcRect = GetFinalContextRect(EDisplayClusterViewportResourceType::InternalRenderTargetResource, SrcContexts[ContextNum].RenderTargetRect);
			FIntRect DestRect = SrcContexts[ContextNum].TileDestRect;

			if (GDisplayClusterRenderTileBorder > 0)
			{
				// The maximum border is 1/4 of the minimum side of the rectangle.
				const int32 MinBorder = FMath::Min(SrcRect.Size().GetMin(), DestRect.Size().GetMin()) / 4;
				const int32 TileBorder = FMath::Min(GDisplayClusterRenderTileBorder, MinBorder);

				// Set rect smaller to show gaps between tiles:
				SrcRect.Min.X += TileBorder;
				SrcRect.Min.Y += TileBorder;
				SrcRect.Max.X -= TileBorder * 2;
				SrcRect.Max.Y -= TileBorder * 2;

				DestRect.Min.X += TileBorder;
				DestRect.Min.Y += TileBorder;
				DestRect.Max.X -= TileBorder * 2;
				DestRect.Max.Y -= TileBorder * 2;
			}

			// Copy tile to dest
			ImplResolveResource(RHICmdList, SrcResources[ContextNum], SrcRect, DestResources[ContextNum], DestRect, false, false);
		}
	}
}

void FDisplayClusterViewportProxy::CleanupResources_RenderThread(FRHICommandListImmediate& RHICmdList) const
{
	// Since the RTT is reused through frames, in case we need to show a black border between viewport tiles, we must fill the original viewport with this colour.
	if (GDisplayClusterRenderTileBorder > 0 && RenderSettings.TileSettings.GetType() == EDisplayClusterViewportTileType::Source)
	{
		TArray<FRHITexture2D*> RenderTargets;
		if (GetResources_RenderThread(EDisplayClusterViewportResourceType::InternalRenderTargetResource, RenderTargets))
		{
			for (FRHITexture2D* TextureIt : RenderTargets)
			{
				// Note: It may make sense to move the CVar and border color to the StageSettings.
				FDisplayClusterViewportProxy::FillTextureWithColor_RenderThread(RHICmdList, TextureIt, FLinearColor::Black);
			}
		}
	}
}
