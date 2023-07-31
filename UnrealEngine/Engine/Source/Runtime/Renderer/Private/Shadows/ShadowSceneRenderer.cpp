// Copyright Epic Games, Inc. All Rights Reserved.
/*=============================================================================
	ShadowSceneRenderer.cpp:
=============================================================================*/
#include "ShadowSceneRenderer.h"
#include "DeferredShadingRenderer.h"
#include "ScenePrivate.h"
#include "VirtualShadowMaps/VirtualShadowMapCacheManager.h"
#include "VirtualShadowMaps/VirtualShadowMapProjection.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#include "DynamicPrimitiveDrawing.h"
#endif

TAutoConsoleVariable<int32> CVarMaxDistantLightsPerFrame(
	TEXT("r.Shadow.Virtual.MaxDistantUpdatePerFrame"),
	1,
	TEXT("Maximum number of distant lights to update each frame."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);


static TAutoConsoleVariable<int32> CVarDistantLightMode(
	TEXT("r.Shadow.Virtual.DistantLightMode"),
	0,
	TEXT("Control whether distant light mode is enabled for local lights.\n0 == Off (default), \n1 == On, \n2 == Force All."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarNaniteShadowsLODBias(
	TEXT("r.Shadow.NaniteLODBias"),
	1.0f,
	TEXT("LOD bias for nanite geometry in shadows. 0 = full detail. >0 = reduced detail."),
	ECVF_RenderThreadSafe);

namespace Nanite
{
	extern bool IsStatFilterActive(const FString& FilterName);
}

FShadowSceneRenderer::FShadowSceneRenderer(FDeferredShadingSceneRenderer& InSceneRenderer)
	: SceneRenderer(InSceneRenderer)
	, Scene(*InSceneRenderer.Scene)
	, VirtualShadowMapArray(InSceneRenderer.VirtualShadowMapArray)
{
}

float FShadowSceneRenderer::ComputeNaniteShadowsLODScaleFactor()
{
	return FMath::Pow(2.0f, -CVarNaniteShadowsLODBias.GetValueOnRenderThread());
}

TSharedPtr<FVirtualShadowMapPerLightCacheEntry> FShadowSceneRenderer::AddLocalLightShadow(const FWholeSceneProjectedShadowInitializer& ProjectedShadowInitializer, FProjectedShadowInfo* ProjectedShadowInfo, FLightSceneInfo* LightSceneInfo, float MaxScreenRadius)
{
	FVirtualShadowMapArrayCacheManager* CacheManager = VirtualShadowMapArray.CacheManager;

	const int32 LocalLightShadowIndex = LocalLights.Num();
	FLocalLightShadowFrameSetup& LocalLightShadowFrameSetup = LocalLights.AddDefaulted_GetRef();
	LocalLightShadowFrameSetup.ProjectedShadowInfo = ProjectedShadowInfo;
	LocalLightShadowFrameSetup.LightSceneInfo = LightSceneInfo;

	// Single page res, at this point we force the VSM to be single page
	// TODO: this computation does not match up with page marking logic super-well, particularly for long spot lights,
	//       we can absolutely mirror the page marking calc better, just unclear how much it helps. 
	//       Also possible to feed back from gpu - which would be more accurate wrt partially visible lights (e.g., a spot going through the ground).
	//       Of course this creates jumps if visibility changes, which may or may not create unsolvable artifacts.
	const bool bIsDistantLight = CVarDistantLightMode.GetValueOnRenderThread() != 0
		&& (MaxScreenRadius <= float(FVirtualShadowMap::PageSize) * FMath::Exp2(VirtualShadowMapArray.GetResolutionLODBiasLocal()) || CVarDistantLightMode.GetValueOnRenderThread() == 2);


	const int32 NumMaps = ProjectedShadowInitializer.bOnePassPointLightShadow ? 6 : 1;
	for (int32 Index = 0; Index < NumMaps; ++Index)
	{
		FVirtualShadowMap* VirtualShadowMap = VirtualShadowMapArray.Allocate(bIsDistantLight);
		// TODO: redundant
		ProjectedShadowInfo->VirtualShadowMaps.Add(VirtualShadowMap);
		LocalLightShadowFrameSetup.VirtualShadowMaps.Add(VirtualShadowMap);
	}

	TSharedPtr<FVirtualShadowMapPerLightCacheEntry> PerLightCacheEntry = CacheManager->FindCreateLightCacheEntry(LightSceneInfo->Id);
	if (PerLightCacheEntry.IsValid())
	{
		LocalLightShadowFrameSetup.PerLightCacheEntry = PerLightCacheEntry;
		PerLightCacheEntry->UpdateLocal(ProjectedShadowInitializer, bIsDistantLight);

		for (int32 Index = 0; Index < NumMaps; ++Index)
		{
			FVirtualShadowMap* VirtualShadowMap = LocalLightShadowFrameSetup.VirtualShadowMaps[Index];

			TSharedPtr<FVirtualShadowMapCacheEntry> VirtualSmCacheEntry = PerLightCacheEntry->FindCreateShadowMapEntry(Index);
			VirtualSmCacheEntry->UpdateLocal(VirtualShadowMap->ID, *PerLightCacheEntry);
			VirtualShadowMap->VirtualShadowMapCacheEntry = VirtualSmCacheEntry;
		}

		if (bIsDistantLight)
		{
			// This priority could be calculated based also on whether the light has actually been invalidated or not (currently not tracked on host).
			// E.g., all things being equal update those with an animated mesh in, for example. Plus don't update those the don't need it at all.
			int32 FramesSinceLastRender = int32(Scene.GetFrameNumber()) - int32(PerLightCacheEntry->GetLastScheduledFrameNumber());
			DistantLightUpdateQueue.Add(-FramesSinceLastRender, LocalLightShadowIndex);
		}
	}
	return PerLightCacheEntry;
}

void FShadowSceneRenderer::PostInitDynamicShadowsSetup()
{
	UpdateDistantLightPriorityRender();

	PostSetupDebugRender();
}

void FShadowSceneRenderer::RenderVirtualShadowMaps(FRDGBuilder& GraphBuilder, bool bNaniteEnabled, bool bUpdateNaniteStreaming, bool bNaniteProgrammableRaster)
{
	const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& VirtualShadowMapShadows = SceneRenderer.SortedShadowsForShadowDepthPass.VirtualShadowMapShadows;
	TArray<TSharedPtr<FVirtualShadowMapClipmap>, SceneRenderingAllocator>& VirtualShadowMapClipmaps = SceneRenderer.SortedShadowsForShadowDepthPass.VirtualShadowMapClipmaps;

	if (VirtualShadowMapShadows.Num() == 0 && VirtualShadowMapClipmaps.Num() == 0)
	{
		return;
	}

	FVirtualShadowMapArrayCacheManager* CacheManager = VirtualShadowMapArray.CacheManager;

	// TODO: Separate out the decision about nanite using HZB and stuff like HZB culling invalidations?
	const bool bVSMUseHZB = VirtualShadowMapArray.UseHzbOcclusion();

	const FIntPoint VirtualShadowSize = VirtualShadowMapArray.GetPhysicalPoolSize();
	const FIntRect VirtualShadowViewRect = FIntRect(0, 0, VirtualShadowSize.X, VirtualShadowSize.Y);

	Nanite::FSharedContext SharedContext{};
	SharedContext.FeatureLevel = SceneRenderer.FeatureLevel;
	SharedContext.ShaderMap = GetGlobalShaderMap(SharedContext.FeatureLevel);
	SharedContext.Pipeline = Nanite::EPipeline::Shadows;

	if (bNaniteEnabled)
	{
		const TRefCountPtr<IPooledRenderTarget> PrevHZBPhysical = bVSMUseHZB ? CacheManager->PrevBuffers.HZBPhysical : nullptr;

		RDG_EVENT_SCOPE(GraphBuilder, "RenderVirtualShadowMaps(Nanite)");

		check(VirtualShadowMapArray.PhysicalPagePoolRDG != nullptr);

		Nanite::FRasterContext RasterContext = Nanite::InitRasterContext(
			GraphBuilder,
			SharedContext,
			VirtualShadowSize,
			false,
			Nanite::EOutputBufferMode::DepthOnly,
			false,	// Clear entire texture
			nullptr, 0,
			VirtualShadowMapArray.PhysicalPagePoolRDG);

		const FViewInfo& SceneView = SceneRenderer.Views[0];

		static FString VirtualFilterName = TEXT("VirtualShadowMaps");

		TArray<Nanite::FPackedView, SceneRenderingAllocator> VirtualShadowViews;

		for (FProjectedShadowInfo* ProjectedShadowInfo : VirtualShadowMapShadows)
		{
			if (ProjectedShadowInfo->bShouldRenderVSM)
			{
				VirtualShadowMapArray.AddRenderViews(
					ProjectedShadowInfo,
					ComputeNaniteShadowsLODScaleFactor(),
					PrevHZBPhysical.IsValid(),
					bVSMUseHZB,
					ProjectedShadowInfo->ShouldClampToNearPlane(),
					VirtualShadowViews);
			}
		}

		if (VirtualShadowViews.Num() > 0)
		{
			int32 NumPrimaryViews = VirtualShadowViews.Num();
			VirtualShadowMapArray.CreateMipViews(VirtualShadowViews);

			Nanite::FRasterState RasterState;

			FNaniteVisibilityResults VisibilityResults; // TODO: Hook up culling for shadows

			Nanite::FCullingContext::FConfiguration CullingConfig = { 0 };
			CullingConfig.bUpdateStreaming = bUpdateNaniteStreaming;
			CullingConfig.bTwoPassOcclusion = VirtualShadowMapArray.UseTwoPassHzbOcclusion();
			CullingConfig.bProgrammableRaster = bNaniteProgrammableRaster;
			CullingConfig.SetViewFlags(SceneView);

			Nanite::FCullingContext CullingContext = Nanite::InitCullingContext(
				GraphBuilder,
				SharedContext,
				Scene,
				PrevHZBPhysical,
				VirtualShadowViewRect,
				CullingConfig
			);

			const bool bExtractStats = Nanite::IsStatFilterActive(VirtualFilterName);

			Nanite::CullRasterize(
				GraphBuilder,
				Scene.NaniteRasterPipelines[ENaniteMeshPass::BasePass],
				VisibilityResults,
				Scene,
				SceneView,
				VirtualShadowViews,
				NumPrimaryViews,
				SharedContext,
				CullingContext,
				RasterContext,
				RasterState,
				nullptr,
				&VirtualShadowMapArray,
				bExtractStats
			);
		}

		if (bVSMUseHZB)
		{
			VirtualShadowMapArray.UpdateHZB(GraphBuilder);
		}
	}

	if (UseNonNaniteVirtualShadowMaps(SceneRenderer.ShaderPlatform, SceneRenderer.FeatureLevel))
	{
		VirtualShadowMapArray.RenderVirtualShadowMapsNonNanite(GraphBuilder, VirtualShadowMapShadows, SceneRenderer.Views);
	}

	// If separate static/dynamic caching is enabled, we may need to merge some pages after rendering
	VirtualShadowMapArray.MergeStaticPhysicalPages(GraphBuilder);
}

void FShadowSceneRenderer::UpdateDistantLightPriorityRender()
{
	int32 UpdateBudgetNumLights = CVarMaxDistantLightsPerFrame.GetValueOnRenderThread() < 0 ? int32(DistantLightUpdateQueue.Num()) : FMath::Min(int32(DistantLightUpdateQueue.Num()), CVarMaxDistantLightsPerFrame.GetValueOnRenderThread());
	for (int32 Index = 0; Index < UpdateBudgetNumLights; ++Index)
	{
		const int32 LocalLightShadowIndex = DistantLightUpdateQueue.Top();
		const int32 Age = DistantLightUpdateQueue.GetKey(LocalLightShadowIndex);
		// UE_LOG(LogTemp, Log, TEXT("Index: %d Age: %d"), LocalLightShadowIndex, Age);
		DistantLightUpdateQueue.Pop();

		FLocalLightShadowFrameSetup& LocalLightShadowFrameSetup = LocalLights[LocalLightShadowIndex];
		
		// Force fully cached to be off.
		LocalLightShadowFrameSetup.ProjectedShadowInfo->bShouldRenderVSM = true;
		LocalLightShadowFrameSetup.PerLightCacheEntry->CurrenScheduledFrameNumber = Scene.GetFrameNumber();
		// Should trigger invalidations also.
		LocalLightShadowFrameSetup.PerLightCacheEntry->Invalidate();
	}
}


void FShadowSceneRenderer::PostSetupDebugRender()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// TODO: Move to debug rendering function in FShadowSceneRenderer
	if ((SceneRenderer.ViewFamily.EngineShowFlags.DebugDrawDistantVirtualSMLights))
	{
		int32 NumDistant = 0;
		for (FViewInfo& View : SceneRenderer.Views)
		{
			FViewElementPDI DebugPDI(&View, nullptr, &View.DynamicPrimitiveCollector);

			for (const FLocalLightShadowFrameSetup& LightSetup : LocalLights)
			{			
				FLinearColor Color = FLinearColor(FColor::Blue);
				if (LightSetup.PerLightCacheEntry && LightSetup.PerLightCacheEntry->bCurrentIsDistantLight)
				{
					++NumDistant;
					int32 FramesSinceLastRender = int32(Scene.GetFrameNumber()) - int32(LightSetup.PerLightCacheEntry->GetLastScheduledFrameNumber());
					float Fade = FMath::Min(0.8f, float(FramesSinceLastRender) / float(LocalLights.Num()));
					Color = LightSetup.PerLightCacheEntry->IsFullyCached() ? FMath::Lerp(FLinearColor(FColor::Green), FLinearColor(FColor::Red), Fade) : FLinearColor(FColor::Red);
				}

				Color.A = 1.0f;
				if (LightSetup.LightSceneInfo->Proxy->GetLightType() == LightType_Spot)
				{
					FTransform TransformNoScale = FTransform(LightSetup.LightSceneInfo->Proxy->GetLightToWorld());
					TransformNoScale.RemoveScaling();

					DrawWireSphereCappedCone(&DebugPDI, TransformNoScale, LightSetup.LightSceneInfo->Proxy->GetRadius(), FMath::RadiansToDegrees(LightSetup.LightSceneInfo->Proxy->GetOuterConeAngle()), 16, 4, 8, Color, SDPG_World);
				}
				else
				{
					DrawWireSphereAutoSides(&DebugPDI, -LightSetup.ProjectedShadowInfo->PreShadowTranslation, Color, LightSetup.LightSceneInfo->Proxy->GetRadius(), SDPG_World);
				}
			}
		}
		SceneRenderer.OnGetOnScreenMessages.AddLambda([this, NumDistant](FScreenMessageWriter& ScreenMessageWriter)->void
		{
			ScreenMessageWriter.DrawLine(FText::FromString(FString::Printf(TEXT("Distant Light Count: %d"), NumDistant)), 10, FColor::Yellow);
			ScreenMessageWriter.DrawLine(FText::FromString(FString::Printf(TEXT("Active Local Light Count: %d"), LocalLights.Num())), 10, FColor::Yellow);
			ScreenMessageWriter.DrawLine(FText::FromString(FString::Printf(TEXT("Scene Light Count: %d"), Scene.Lights.Num())), 10, FColor::Yellow);
		});
	}
#endif
}

extern TAutoConsoleVariable<int32> CVarVirtualShadowOnePassProjection;

void FShadowSceneRenderer::RenderVirtualShadowMapProjectionMaskBits(
	FRDGBuilder& GraphBuilder,
	FMinimalSceneTextures& SceneTextures)
{
	// VSM one pass projection (done first as it may be needed by clustered shading)
	bShouldUseVirtualShadowMapOnePassProjection =
		VirtualShadowMapArray.IsAllocated() &&
		CVarVirtualShadowOnePassProjection.GetValueOnRenderThread();

	if (!VirtualShadowMapArray.HasAnyShadowData())
	{
		return;
	}

	if (bShouldUseVirtualShadowMapOnePassProjection)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "VirtualShadowMapProjectionMaskBits");

		VirtualShadowMapMaskBits = CreateVirtualShadowMapMaskBits(GraphBuilder, SceneTextures, VirtualShadowMapArray, TEXT("Shadow.Virtual.MaskBits"));
		VirtualShadowMapMaskBitsHairStrands = CreateVirtualShadowMapMaskBits(GraphBuilder, SceneTextures, VirtualShadowMapArray, TEXT("Shadow.Virtual.MaskBits(HairStrands)"));

		for (int32 ViewIndex = 0; ViewIndex < SceneRenderer.Views.Num(); ++ViewIndex)
		{
			RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, SceneRenderer.Views.Num() > 1, "View%d", ViewIndex);

			const FViewInfo& View = SceneRenderer.Views[ViewIndex];

			RenderVirtualShadowMapProjectionOnePass(
				GraphBuilder,
				SceneTextures,
				View, ViewIndex,
				VirtualShadowMapArray,
				EVirtualShadowMapProjectionInputType::GBuffer,
				VirtualShadowMapMaskBits);

			if (HairStrands::HasViewHairStrandsData(View))
			{
				RenderVirtualShadowMapProjectionOnePass(
					GraphBuilder,
					SceneTextures,
					View, ViewIndex,
					VirtualShadowMapArray,
					EVirtualShadowMapProjectionInputType::HairStrands,
					VirtualShadowMapMaskBitsHairStrands);
			}
		}
	}
	else
	{
		//FRDGTextureRef Dummy = GraphBuilder.RegisterExternalTexture(GSystemTextures.ZeroUIntDummy);
		VirtualShadowMapMaskBits = nullptr;//Dummy;
		VirtualShadowMapMaskBitsHairStrands = nullptr;//Dummy;
	}
}

void FShadowSceneRenderer::ApplyVirtualShadowMapProjectionForLight(
	FRDGBuilder& GraphBuilder,
	const FMinimalSceneTextures& SceneTextures,
	const FLightSceneInfo* LightSceneInfo,
	FRDGTextureRef OutputScreenShadowMaskTexture,
	FRDGTextureRef OutputScreenShadowMaskSubPixelTexture)
{
	if (!VirtualShadowMapArray.HasAnyShadowData())
	{
		return;
	}

	const FVisibleLightInfo& VisibleLightInfo = SceneRenderer.VisibleLightInfos[LightSceneInfo->Id];
	FSceneTextureParameters SceneTextureParameters = GetSceneTextureParameters(GraphBuilder, SceneTextures.UniformBuffer);

	for (int32 ViewIndex = 0; ViewIndex < SceneRenderer.Views.Num(); ViewIndex++)
	{
		RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, SceneRenderer.Views.Num() > 1, "View%d", ViewIndex);

		FViewInfo& View = SceneRenderer.Views[ViewIndex];
		FIntRect ScissorRect;
		if (!LightSceneInfo->Proxy->GetScissorRect(ScissorRect, View, View.ViewRect))
		{
			ScissorRect = View.ViewRect;
		}

		if (ScissorRect.Area() > 0)
		{
			int32 VirtualShadowMapId = VisibleLightInfo.GetVirtualShadowMapId(&View);
			
			// Some lights can elide the screen shadow mask entirely, in which case they will be sampled directly in the lighting shader
			if (OutputScreenShadowMaskTexture)
			{
				if (VisibleLightInfo.VirtualShadowMapClipmaps.Num() > 0)
				{
					// Project directional light virtual shadow map
					RenderVirtualShadowMapProjection(
						GraphBuilder,
						SceneTextures,
						View, ViewIndex,
						VirtualShadowMapArray,
						ScissorRect,
						EVirtualShadowMapProjectionInputType::GBuffer,
						VisibleLightInfo.FindShadowClipmapForView(&View),
						OutputScreenShadowMaskTexture);
				}
				else if (bShouldUseVirtualShadowMapOnePassProjection)
				{
					// Copy local light from one pass projection output
					CompositeVirtualShadowMapFromMaskBits(
						GraphBuilder,
						SceneTextures,
						View,
						ScissorRect,
						VirtualShadowMapArray,
						EVirtualShadowMapProjectionInputType::GBuffer,
						VirtualShadowMapId,
						VirtualShadowMapMaskBits,
						OutputScreenShadowMaskTexture);
				}
				else
				{
					// Project local light virtual shadow map
					RenderVirtualShadowMapProjection(
						GraphBuilder,
						SceneTextures,
						View, ViewIndex,
						VirtualShadowMapArray,
						ScissorRect,
						EVirtualShadowMapProjectionInputType::GBuffer,
						*LightSceneInfo,
						VirtualShadowMapId,
						OutputScreenShadowMaskTexture);
				}
			}

			// Sub-pixel shadow (no denoising for hair)
			if (HairStrands::HasViewHairStrandsData(View) && OutputScreenShadowMaskSubPixelTexture)
			{
				if (VisibleLightInfo.VirtualShadowMapClipmaps.Num() > 0)
				{
					RenderVirtualShadowMapProjection(
						GraphBuilder,
						SceneTextures,
						View, ViewIndex,
						VirtualShadowMapArray,
						ScissorRect,
						EVirtualShadowMapProjectionInputType::HairStrands,
						VisibleLightInfo.FindShadowClipmapForView(&View),
						OutputScreenShadowMaskSubPixelTexture);
				}
				else if (bShouldUseVirtualShadowMapOnePassProjection)
				{
					// Copy local light from one pass projection output
					CompositeVirtualShadowMapFromMaskBits(
						GraphBuilder,
						SceneTextures,
						View,
						ScissorRect,
						VirtualShadowMapArray,
						EVirtualShadowMapProjectionInputType::HairStrands,
						VirtualShadowMapId,
						VirtualShadowMapMaskBitsHairStrands,
						OutputScreenShadowMaskSubPixelTexture);
				}
				else
				{
					RenderVirtualShadowMapProjection(
						GraphBuilder,
						SceneTextures,
						View, ViewIndex,
						VirtualShadowMapArray,
						ScissorRect,
						EVirtualShadowMapProjectionInputType::HairStrands,
						*LightSceneInfo,
						VirtualShadowMapId,
						OutputScreenShadowMaskSubPixelTexture);
				}
			}
		}
	}
}
