// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MobileDecalRendering.cpp: Decals for mobile renderer
=============================================================================*/

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "RHIDefinitions.h"
#include "RHI.h"
#include "SceneUtils.h"
#include "RHIStaticStates.h"
#include "PostProcess/SceneRenderTargets.h"
#include "SceneRendering.h"
#include "ScenePrivate.h"
#include "DecalRenderingCommon.h"
#include "DecalRenderingShared.h"
#include "RenderCore.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "CompositionLighting/PostProcessDeferredDecals.h"
#include "DBufferTextures.h"

void RenderMeshDecalsMobile(FRHICommandList& RHICmdList, const FViewInfo& View, EDecalRenderStage DecalRenderStage, EDecalRenderTargetMode RenderTargetMode);
extern void RenderDeferredDecalsMobile(FRHICommandList& RHICmdList, const FScene& Scene, const FViewInfo& View, EDecalRenderStage DecalRenderStage, EDecalRenderTargetMode RenderTargetMode);

static bool DoesPlatformSupportDecals(EShaderPlatform ShaderPlatform)
{
	if (!IsMobileHDR())
	{
		// Vulkan uses sub-pass to fetch SceneDepth
		if (IsVulkanPlatform(ShaderPlatform) ||
			IsSimulatedPlatform(ShaderPlatform) ||
			// Some Androids support SceneDepth fetch
			(IsAndroidOpenGLESPlatform(ShaderPlatform) && GSupportsShaderDepthStencilFetch))
		{
			return true;
		}

		// Metal needs DepthAux to fetch depth, and its not availle in LDR mode
		return false;
	}

	// HDR always supports decals
	return true;
}

void FMobileSceneRenderer::RenderDecals(FRHICommandList& RHICmdList, const FViewInfo& View, const FInstanceCullingDrawParams* InstanceCullingDrawParams)
{
	if (!DoesPlatformSupportDecals(View.GetShaderPlatform()) || !ViewFamily.EngineShowFlags.Decals || View.bIsPlanarReflection)
	{
		return;
	}

	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(RenderDecals);
	SCOPE_CYCLE_COUNTER(STAT_DecalsDrawTime);

	const bool bIsMobileDeferred = IsMobileDeferredShadingEnabled(View.GetShaderPlatform());
	const EDecalRenderStage DecalRenderStage = bIsMobileDeferred ? EDecalRenderStage::MobileBeforeLighting : bRequiresDBufferDecals ? EDecalRenderStage::Emissive : EDecalRenderStage::Mobile;
	const EDecalRenderTargetMode RenderTargetMode = bIsMobileDeferred ? EDecalRenderTargetMode::SceneColorAndGBuffer : EDecalRenderTargetMode::SceneColor;

	// Deferred decals
	if (Scene->Decals.Num() > 0)
	{
		SCOPED_DRAW_EVENT(RHICmdList, Decals);
		RenderDeferredDecalsMobile(RHICmdList, *Scene, View, DecalRenderStage, RenderTargetMode);
	}

	// Mesh decals
	if (View.MeshDecalBatches.Num() > 0)
	{
		SCOPED_DRAW_EVENT(RHICmdList, MeshDecals);
		RenderMeshDecalsMobile(RHICmdList, View, DecalRenderStage, RenderTargetMode);

		// MeshDecals use DrawDynamicMeshPass which may change BatchedPrimitive binding, so we need to restore it
		FUniformBufferStaticSlot BatchedPrimitiveSlot = FInstanceCullingContext::GetUniformBufferViewStaticSlot(View.GetShaderPlatform());
		if (IsUniformBufferStaticSlotValid(BatchedPrimitiveSlot) && InstanceCullingDrawParams->BatchedPrimitive)
		{
			FRHIUniformBuffer* BatchedPrimitiveBufferRHI = InstanceCullingDrawParams->BatchedPrimitive.GetUniformBuffer()->GetRHI();
			check(BatchedPrimitiveBufferRHI);
			RHICmdList.SetStaticUniformBuffer(BatchedPrimitiveSlot, BatchedPrimitiveBufferRHI);
		}
	}
}

void RenderDeferredDecalsMobile(FRHICommandList& RHICmdList, const FScene& Scene, const FViewInfo& View, EDecalRenderStage DecalRenderStage, EDecalRenderTargetMode RenderTargetMode)
{
	int32 SortedDecalCount = 0;
	FTransientDecalRenderDataList SortedDecals;

	if (!Scene.Decals.IsEmpty())
	{
		FTransientDecalRenderDataList VisibleDecals = DecalRendering::BuildVisibleDecalList(Scene.Decals, View);

		// Build a list of decals that need to be rendered for this view
		DecalRendering::BuildRelevantDecalList(VisibleDecals, DecalRenderStage, &SortedDecals);
		SortedDecalCount = SortedDecals.Num();
		INC_DWORD_STAT_BY(STAT_Decals, SortedDecalCount);
	}

	if (SortedDecalCount > 0)
	{
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1);
		RHICmdList.SetStreamSource(0, GetUnitCubeVertexBuffer(), 0);

		for (int32 DecalIndex = 0; DecalIndex < SortedDecalCount; DecalIndex++)
		{
			const FTransientDecalRenderData& DecalData = SortedDecals[DecalIndex];
			const FDeferredDecalProxy& DecalProxy = *DecalData.Proxy;
			const FMatrix ComponentToWorldMatrix = DecalProxy.ComponentTrans.ToMatrixWithScale();
			const FMatrix FrustumComponentToClip = DecalRendering::ComputeComponentToClipMatrix(View, ComponentToWorldMatrix);

			const float ConservativeRadius = DecalData.ConservativeRadius;
			const bool bInsideDecal = ((FVector)View.ViewMatrices.GetViewOrigin() - ComponentToWorldMatrix.GetOrigin()).SizeSquared() < FMath::Square(ConservativeRadius * 1.05f + View.NearClippingDistance * 2.0f);
			bool bReverseHanded = false;
			{
				// Account for the reversal of handedness caused by negative scale on the decal
				const auto& Scale3d = DecalProxy.ComponentTrans.GetScale3D();
				bReverseHanded = Scale3d[0] * Scale3d[1] * Scale3d[2] < 0.f;
			}
			EDecalRasterizerState DecalRasterizerState = DecalRendering::GetDecalRasterizerState(bInsideDecal, bReverseHanded, View.bReverseCulling);
			GraphicsPSOInit.RasterizerState = DecalRendering::GetDecalRasterizerState(DecalRasterizerState);

			if (bInsideDecal)
			{
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
					false, CF_Always,
					true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
					false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
					GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1), 0x00>::GetRHI();
			}
			else
			{
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
					false, CF_DepthNearOrEqual,
					true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
					false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
					GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1), 0x00>::GetRHI();
			}

			GraphicsPSOInit.BlendState = DecalRendering::GetDecalBlendState(DecalData.BlendDesc, DecalRenderStage, RenderTargetMode);

			// Set shader params
			DecalRendering::SetShader(RHICmdList, GraphicsPSOInit, 0, View, DecalData, DecalRenderStage, FrustumComponentToClip);

			RHICmdList.DrawIndexedPrimitive(GetUnitCubeIndexBuffer(), 0, 0, 8, 0, UE_ARRAY_COUNT(GCubeIndices) / 3, 1);
		}
	}
}

void FMobileSceneRenderer::RenderDBuffer(FRDGBuilder& GraphBuilder, FSceneTextures& SceneTextures, FDBufferTextures& DBufferTextures, FInstanceCullingManager& InstanceCullingManager)
{
	RDG_EVENT_SCOPE(GraphBuilder, "RenderDBuffer");
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RenderDBuffer);

	const EShaderPlatform Platform = GetViewFamilyInfo(Views).GetShaderPlatform();

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		FViewInfo& View = Views[ViewIndex];

		if (!View.ShouldRenderView())
		{
			continue;
		}

		FTransientDecalRenderDataList VisibleDecals = DecalRendering::BuildVisibleDecalList(Scene->Decals, View);

		FDeferredDecalPassTextures DecalPassTextures = GetDeferredDecalPassTextures(GraphBuilder, View, SceneTextures, &DBufferTextures);
		AddDeferredDecalPass(GraphBuilder, View, VisibleDecals, DecalPassTextures, InstanceCullingManager, EDecalRenderStage::BeforeBasePass);
	}
}
