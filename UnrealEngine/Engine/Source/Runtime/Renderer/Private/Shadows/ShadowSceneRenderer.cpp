// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShadowSceneRenderer.h"
#include "ShadowScene.h"
#include "DeferredShadingRenderer.h"
#include "ScenePrivate.h"
#include "ShadowRendering.h"
#include "VirtualShadowMaps/VirtualShadowMapCacheManager.h"
#include "VirtualShadowMaps/VirtualShadowMapProjection.h"
#include "SceneCulling/SceneCulling.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#include "DynamicPrimitiveDrawing.h"
#endif

CSV_DECLARE_CATEGORY_EXTERN(VSM);

extern int32 GForceInvalidateDirectionalVSM;

TAutoConsoleVariable<int32> CVarVSMMaterialVisibility(
	TEXT("r.Shadow.Virtual.Nanite.MaterialVisibility"),
	0,
	TEXT("Enable Nanite CPU-side visibility filtering of draw commands, depends on r.Nanite.MaterialVisibility being enabled."),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarMaxDistantLightsPerFrame(
	TEXT("r.Shadow.Virtual.MaxDistantUpdatePerFrame"),
	1,
	TEXT("Maximum number of distant lights to update each frame."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarDistantLightMode(
	TEXT("r.Shadow.Virtual.DistantLightMode"),
	1,
	TEXT("Control whether distant light mode is enabled for local lights.\n0 == Off, \n1 == On (default), \n2 == Force All."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarDistantLightForceCacheFootprintFraction(
	TEXT("r.Shadow.Virtual.DistantLightForceCacheFootprintFraction"),
	0.0f,
	TEXT("Fraction of footprint size below which start force-caching lights that are invalidated (i.e., are moving or re-added)\n")
	TEXT("  The base footprint is based on the page size.\n")
	TEXT("  0.0 == Never force-cache (default), 1.0 == Always force-cache."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarNaniteShadowsLODBias(
	TEXT("r.Shadow.NaniteLODBias"),
	1.0f,
	TEXT("LOD bias for nanite geometry in shadows. 0 = full detail. >0 = reduced detail."),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarVirtualShadowOnePassProjection(
	TEXT("r.Shadow.Virtual.OnePassProjection"),
	1,
	TEXT("Projects all local light virtual shadow maps in a single pass for better performance."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarResolutionLodBiasLocal(
	TEXT("r.Shadow.Virtual.ResolutionLodBiasLocal"),
	0.0f,
	TEXT("Bias applied to LOD calculations for local lights. -1.0 doubles resolution, 1.0 halves it and so on."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarResolutionLodBiasLocalMoving(
	TEXT("r.Shadow.Virtual.ResolutionLodBiasLocalMoving"),
	1.0f,
	TEXT("Bias applied to LOD calculations for moving local lights. -1.0 doubles resolution, 1.0 halves it and so on."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

DECLARE_DWORD_COUNTER_STAT(TEXT("VSM Total Raster Bins"), STAT_VSMNaniteBasePassTotalRasterBins, STATGROUP_ShadowRendering);
DECLARE_DWORD_COUNTER_STAT(TEXT("VSM Total Shading Draws"), STAT_VSMNaniteBasePassTotalShadingDraws, STATGROUP_ShadowRendering);

DECLARE_DWORD_COUNTER_STAT(TEXT("VSM Visible Raster Bins"), STAT_VSMNaniteBasePassVisibleRasterBins, STATGROUP_ShadowRendering);
DECLARE_DWORD_COUNTER_STAT(TEXT("VSM Visible Shading Draws"), STAT_VSMNaniteBassPassVisibleShadingDraws, STATGROUP_ShadowRendering);

DECLARE_DWORD_COUNTER_STAT(TEXT("Distant Light Count"), STAT_DistantLightCount, STATGROUP_ShadowRendering);
DECLARE_DWORD_COUNTER_STAT(TEXT("Distant Cached Count"), STAT_DistantCachedCount, STATGROUP_ShadowRendering);

DECLARE_DWORD_COUNTER_STAT(TEXT("VSM Light Projections (Directional)"), STAT_VSMDirectionalProjectionFull, STATGROUP_ShadowRendering);
DECLARE_DWORD_COUNTER_STAT(TEXT("VSM Light Projections (Local Full)"), STAT_VSMLocalProjectionFull, STATGROUP_ShadowRendering);
DECLARE_DWORD_COUNTER_STAT(TEXT("VSM Light Projections (Local One Pass Copy)"), STAT_VSMLocalProjectionOnePassCopy, STATGROUP_ShadowRendering);

FShadowSceneRenderer::FShadowSceneRenderer(FDeferredShadingSceneRenderer& InSceneRenderer)
	: SceneRenderer(InSceneRenderer)
	, Scene(*InSceneRenderer.Scene)
	, ShadowScene(*Scene.ShadowScene)
	, VirtualShadowMapArray(InSceneRenderer.VirtualShadowMapArray)
{
}

float FShadowSceneRenderer::ComputeNaniteShadowsLODScaleFactor()
{
	return FMath::Pow(2.0f, -CVarNaniteShadowsLODBias.GetValueOnRenderThread());
}

static float GetResolutionLODBiasLocal(float LightMobilityFactor)
{
	return FVirtualShadowMapArray::InterpolateResolutionBias(
		CVarResolutionLodBiasLocal.GetValueOnRenderThread(),
		CVarResolutionLodBiasLocalMoving.GetValueOnRenderThread(),
		LightMobilityFactor);
}

FVirtualShadowMapProjectionShaderData FShadowSceneRenderer::GetLocalLightProjectionShaderData(
	float ResolutionLODBiasLocal,
	const FProjectedShadowInfo* ProjectedShadowInfo,
	int32 MapIndex) const
{
	TSharedPtr<FVirtualShadowMapPerLightCacheEntry> CacheEntry = ProjectedShadowInfo->VirtualShadowMapPerLightCacheEntry;
	check(CacheEntry.IsValid());

	int32 VirtualShadowMapId = ProjectedShadowInfo->VirtualShadowMapId + MapIndex;
	bool bIsSinglePageSM = FVirtualShadowMapArray::IsSinglePage(VirtualShadowMapId);
	check(VirtualShadowMapId != INDEX_NONE && CacheEntry->Current.bIsDistantLight == bIsSinglePageSM);

	uint32 Flags = bIsSinglePageSM ? VSM_PROJ_FLAG_CURRENT_DISTANT_LIGHT : 0U;
	Flags |= CacheEntry->IsUncached() ? VSM_PROJ_FLAG_UNCACHED : 0U;

	const FViewMatrices ViewMatrices = ProjectedShadowInfo->GetShadowDepthRenderingViewMatrices(MapIndex, true);
	const FLargeWorldRenderPosition PreViewTranslation(ProjectedShadowInfo->PreShadowTranslation);

	FVirtualShadowMapProjectionShaderData Data; 
	Data.TranslatedWorldToShadowViewMatrix		= FMatrix44f(ViewMatrices.GetTranslatedViewMatrix());	// LWC_TODO: Precision loss?
	Data.ShadowViewToClipMatrix					= FMatrix44f(ViewMatrices.GetProjectionMatrix());
	Data.TranslatedWorldToShadowUVMatrix		= FMatrix44f(CalcTranslatedWorldToShadowUVMatrix( ViewMatrices.GetTranslatedViewMatrix(), ViewMatrices.GetProjectionMatrix() ));
	Data.TranslatedWorldToShadowUVNormalMatrix	= FMatrix44f(CalcTranslatedWorldToShadowUVNormalMatrix( ViewMatrices.GetTranslatedViewMatrix(), ViewMatrices.GetProjectionMatrix() ));
	Data.PreViewTranslationLWCTile				= PreViewTranslation.GetTile();
	Data.PreViewTranslationLWCOffset			= PreViewTranslation.GetOffset();
	Data.LightType								= ProjectedShadowInfo->GetLightSceneInfo().Proxy->GetLightType();
	Data.LightSourceRadius						= ProjectedShadowInfo->GetLightSceneInfo().Proxy->GetSourceRadius();
	Data.ResolutionLodBias						= ResolutionLODBiasLocal;
	Data.LightRadius							= ProjectedShadowInfo->GetLightSceneInfo().Proxy->GetRadius();
	Data.Flags									= Flags;

	return Data;
}

TSharedPtr<FVirtualShadowMapPerLightCacheEntry> FShadowSceneRenderer::AddLocalLightShadow(const FWholeSceneProjectedShadowInitializer& ProjectedShadowInitializer, FProjectedShadowInfo* ProjectedShadowInfo, FLightSceneInfo* LightSceneInfo, float MaxScreenRadius)
{
	FVirtualShadowMapArrayCacheManager* CacheManager = VirtualShadowMapArray.CacheManager;

	const int32 LocalLightShadowIndex = LocalLights.Num();
	FLocalLightShadowFrameSetup& LocalLightShadowFrameSetup = LocalLights.AddDefaulted_GetRef();
	LocalLightShadowFrameSetup.ProjectedShadowInfo = ProjectedShadowInfo;
	LocalLightShadowFrameSetup.LightSceneInfo = LightSceneInfo;

	const float ResolutionLODBiasLocal = GetResolutionLODBiasLocal(ShadowScene.GetLightMobilityFactor(LightSceneInfo->Id));

	// Single page res, at this point we force the VSM to be single page
	// TODO: this computation does not match up with page marking logic super-well, particularly for long spot lights,
	//       we can absolutely mirror the page marking calc better, just unclear how much it helps. 
	//       Also possible to feed back from gpu - which would be more accurate wrt partially visible lights (e.g., a spot going through the ground).
	//       Of course this creates jumps if visibility changes, which may or may not create unsolvable artifacts.	
	const float BiasedFootprintThreshold = float(FVirtualShadowMap::PageSize) * FMath::Exp2(ResolutionLODBiasLocal);
	const bool bIsDistantLight = CVarDistantLightMode.GetValueOnRenderThread() != 0
		&& (MaxScreenRadius <= BiasedFootprintThreshold || CVarDistantLightMode.GetValueOnRenderThread() == 2);

	const int32 NumMaps = ProjectedShadowInitializer.bOnePassPointLightShadow ? 6 : 1;
	TSharedPtr<FVirtualShadowMapPerLightCacheEntry> PerLightCacheEntry = CacheManager->FindCreateLightCacheEntry(LightSceneInfo->Id, 0, NumMaps);
			
	const float DistantLightForceCacheFootprintFraction = FMath::Clamp(CVarDistantLightForceCacheFootprintFraction.GetValueOnRenderThread(), 0.0f, 1.0f);
	bool bShouldForceTimeSliceDistantUpdate = (bIsDistantLight && MaxScreenRadius <= BiasedFootprintThreshold * DistantLightForceCacheFootprintFraction);
	LocalLightShadowFrameSetup.PerLightCacheEntry = PerLightCacheEntry;
	bool bIsCached = PerLightCacheEntry->UpdateLocal(ProjectedShadowInitializer, bIsDistantLight, CacheManager->IsCacheEnabled(), !bShouldForceTimeSliceDistantUpdate);

	// Update info on the ProjectionShadowInfo; eventually this should all move into local data structures here
	const int32 VirtualShadowMapId = VirtualShadowMapArray.Allocate(bIsDistantLight, NumMaps);
	ProjectedShadowInfo->VirtualShadowMapId = VirtualShadowMapId;
	ProjectedShadowInfo->VirtualShadowMapPerLightCacheEntry = PerLightCacheEntry;
	ProjectedShadowInfo->bShouldRenderVSM = !PerLightCacheEntry->IsFullyCached();

	for (int32 Index = 0; Index < NumMaps; ++Index)
	{
		const int32 FaceVirtualShadowMapId = VirtualShadowMapId + Index;
		FVirtualShadowMapCacheEntry& VirtualSmCacheEntry = PerLightCacheEntry->ShadowMapEntries[Index];
		VirtualSmCacheEntry.Update(VirtualShadowMapArray, *PerLightCacheEntry, FaceVirtualShadowMapId);
		// Update projection data
		VirtualSmCacheEntry.ProjectionData = GetLocalLightProjectionShaderData(ResolutionLODBiasLocal, ProjectedShadowInfo, Index);
	}

	// Only round-robin those that were not invalidated.
	if (bIsDistantLight && bIsCached)
	{
		// This priority could be calculated based also on whether the light has actually been invalidated or not (currently not tracked on host).
		// E.g., all things being equal update those with an animated mesh in, for example. Plus don't update those the don't need it at all.
		int32 FramesSinceLastRender = int32(Scene.GetFrameNumber()) - int32(PerLightCacheEntry->GetLastScheduledFrameNumber());
		DistantLightUpdateQueue.Add(-FramesSinceLastRender, LocalLightShadowIndex);
	}

	return PerLightCacheEntry;
}

void FShadowSceneRenderer::AddDirectionalLightShadow(FProjectedShadowInfo* ProjectedShadowInfo)
{
	FDirectionalLightShadowFrameSetup& DirectionalLightShadowFrameSetup = DirectionalLights.AddDefaulted_GetRef();
	DirectionalLightShadowFrameSetup.ProjectedShadowInfo = ProjectedShadowInfo;
}

void FShadowSceneRenderer::PostInitDynamicShadowsSetup()
{
	UpdateDistantLightPriorityRender();

	// Dispatch async Nanite culling job if appropriate
	if (CVarVSMMaterialVisibility.GetValueOnRenderThread() != 0)
	{
		TArray<FConvexVolume, SceneRenderingAllocator> NaniteCullingViewsVolumes;
		// If we have a clipmap that can't be culled, it'd be a complete waste of time to cull the local lights.
		bool bUnboundedClipmap = false;
		
		for (const FDirectionalLightShadowFrameSetup& DirectionalLightShadowFrameSetup : DirectionalLights)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = DirectionalLightShadowFrameSetup.ProjectedShadowInfo;
			if (!bUnboundedClipmap && ProjectedShadowInfo->bShouldRenderVSM)
			{
				const bool bIsCached = VirtualShadowMapArray.CacheManager->IsCacheEnabled() && GForceInvalidateDirectionalVSM == 0;

				// We can only do this culling if the light is both uncached & it is using the accurate bounds (i.e., r.Shadow.Virtual.Clipmap.UseConservativeCulling is turned off).
				if (!bIsCached && !ProjectedShadowInfo->CascadeSettings.ShadowBoundsAccurate.Planes.IsEmpty())
				{
					NaniteCullingViewsVolumes.Add(ProjectedShadowInfo->CascadeSettings.ShadowBoundsAccurate);
				}
				else
				{
					bUnboundedClipmap = true;
				}
			}
		}
		if (!bUnboundedClipmap)
		{
			for (const FLocalLightShadowFrameSetup& LocalLightShadowFrameSetup : LocalLights)
			{
				FProjectedShadowInfo* ProjectedShadowInfo = LocalLightShadowFrameSetup.ProjectedShadowInfo;
				if (ProjectedShadowInfo->bShouldRenderVSM)
				{
					FConvexVolume WorldSpaceCasterOuterFrustum = ProjectedShadowInfo->CasterOuterFrustum;
					for (FPlane& Plane : WorldSpaceCasterOuterFrustum.Planes)
					{
						Plane = Plane.TranslateBy(-ProjectedShadowInfo->PreShadowTranslation);
					}
					WorldSpaceCasterOuterFrustum.Init();
					NaniteCullingViewsVolumes.Add(WorldSpaceCasterOuterFrustum);
				}
			}

			if (!NaniteCullingViewsVolumes.IsEmpty())
			{
				NaniteVisibilityQuery = Scene.NaniteVisibility[ENaniteMeshPass::BasePass].BeginVisibilityQuery(
					Scene,
					NaniteCullingViewsVolumes,
					&Scene.NaniteRasterPipelines[ENaniteMeshPass::BasePass],
					&Scene.NaniteMaterials[ENaniteMeshPass::BasePass]
				);
			}
		}
	}
}

void FShadowSceneRenderer::RenderVirtualShadowMaps(FRDGBuilder& GraphBuilder, bool bNaniteEnabled, bool bUpdateNaniteStreaming)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FShadowSceneRenderer::RenderVirtualShadowMaps);

	// Always process an existing query if it exists
	FNaniteVisibilityResults VisibilityResults;
	if (NaniteVisibilityQuery != nullptr)
	{
		Scene.NaniteVisibility[ENaniteMeshPass::BasePass].FinishVisibilityQuery(NaniteVisibilityQuery, VisibilityResults);

		uint32 TotalRasterBins = 0;
		uint32 VisibleRasterBins = 0;
		VisibilityResults.GetRasterBinStats(VisibleRasterBins, TotalRasterBins);

		uint32 TotalShadingDraws = 0;
		uint32 VisibleShadingDraws = 0;
		VisibilityResults.GetShadingDrawStats(VisibleShadingDraws, TotalShadingDraws);

		SET_DWORD_STAT(STAT_VSMNaniteBasePassTotalRasterBins, TotalRasterBins);
		SET_DWORD_STAT(STAT_VSMNaniteBasePassTotalShadingDraws, TotalShadingDraws);

		SET_DWORD_STAT(STAT_VSMNaniteBasePassVisibleRasterBins, VisibleRasterBins);
		SET_DWORD_STAT(STAT_VSMNaniteBassPassVisibleShadingDraws, VisibleShadingDraws);
	}


	const TArray<FProjectedShadowInfo*, SceneRenderingAllocator>& VirtualShadowMapShadows = SceneRenderer.SortedShadowsForShadowDepthPass.VirtualShadowMapShadows;
	TArray<TSharedPtr<FVirtualShadowMapClipmap>, SceneRenderingAllocator>& VirtualShadowMapClipmaps = SceneRenderer.SortedShadowsForShadowDepthPass.VirtualShadowMapClipmaps;

	if (VirtualShadowMapShadows.Num() == 0 && VirtualShadowMapClipmaps.Num() == 0)
	{
		return;
	}

	if (bNaniteEnabled)
	{
		VirtualShadowMapArray.RenderVirtualShadowMapsNanite(GraphBuilder, SceneRenderer, bUpdateNaniteStreaming, VisibilityResults, VirtualShadowMapViews, SceneInstanceCullingQuery);
	}

	if (UseNonNaniteVirtualShadowMaps(SceneRenderer.ShaderPlatform, SceneRenderer.FeatureLevel))
	{
		VirtualShadowMapArray.RenderVirtualShadowMapsNonNanite(GraphBuilder, SceneRenderer.GetSceneUniforms(), VirtualShadowMapShadows, SceneRenderer.Views);
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
		LocalLightShadowFrameSetup.PerLightCacheEntry->Current.ScheduledFrameNumber = Scene.GetFrameNumber();
		// Should trigger invalidations also.
		LocalLightShadowFrameSetup.PerLightCacheEntry->Invalidate();
	}
}


void FShadowSceneRenderer::DispatchVirtualShadowMapViewAndCullingSetup(FRDGBuilder& GraphBuilder, TConstArrayView<FProjectedShadowInfo*> VirtualShadowMapShadows)
{
	// Don't want to run this more than once in a given frame.
	check(SceneInstanceCullingQuery == nullptr);

	// Set up view array and collect culling work at the same time.
	SceneInstanceCullingQuery = SceneRenderer.SceneCullingRenderer.CreateInstanceQuery(GraphBuilder);
	VirtualShadowMapViews = VirtualShadowMapArray.CreateVirtualShadowMapNaniteViews(GraphBuilder, SceneRenderer.Views, VirtualShadowMapShadows, ComputeNaniteShadowsLODScaleFactor(), SceneInstanceCullingQuery);

	// Dispatch collected query 
	if (SceneInstanceCullingQuery)
	{
		SceneInstanceCullingQuery->Dispatch(GraphBuilder);
	}
}

void FShadowSceneRenderer::PostSetupDebugRender()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if ((SceneRenderer.ViewFamily.EngineShowFlags.DebugDrawDistantVirtualSMLights) && VirtualShadowMapArray.IsEnabled())
	{
		int32 NumFullyCached = 0;
		int32 NumDistant = 0;
		for (FViewInfo& View : SceneRenderer.Views)
		{
			FViewElementPDI DebugPDI(&View, nullptr, nullptr);

			for (const FLocalLightShadowFrameSetup& LightSetup : LocalLights)
			{			
				FLinearColor Color = FLinearColor(FColor::Blue);
				if (LightSetup.PerLightCacheEntry && LightSetup.PerLightCacheEntry->Current.bIsDistantLight)
				{
					++NumDistant;
					int32 FramesSinceLastRender = int32(Scene.GetFrameNumber()) - int32(LightSetup.PerLightCacheEntry->GetLastScheduledFrameNumber());
					float Fade = FMath::Min(0.8f, float(FramesSinceLastRender) / float(LocalLights.Num()));
					if (LightSetup.PerLightCacheEntry->IsFullyCached())
					{
						++NumFullyCached;
						Color = FMath::Lerp(FLinearColor(FColor::Green), FLinearColor(FColor::Red), Fade);
					}
					else
					{
						Color = FLinearColor(FColor::Purple);
					}
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
		SET_DWORD_STAT(STAT_DistantLightCount, NumDistant);
		SET_DWORD_STAT(STAT_DistantCachedCount, NumFullyCached);
	}
#endif
}

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
					INC_DWORD_STAT(STAT_VSMDirectionalProjectionFull);

					// Project directional light virtual shadow map
					RenderVirtualShadowMapProjection(
						GraphBuilder,
						SceneTextures,
						View, ViewIndex,
						VirtualShadowMapArray,
						ScissorRect,
						EVirtualShadowMapProjectionInputType::GBuffer,
						VisibleLightInfo.FindShadowClipmapForView(&View),
						false, // bModulateRGB
						nullptr, // TiledVSMProjection
						OutputScreenShadowMaskTexture);
				}
				else if (bShouldUseVirtualShadowMapOnePassProjection)
				{
					INC_DWORD_STAT(STAT_VSMLocalProjectionOnePassCopy);

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
					INC_DWORD_STAT(STAT_VSMLocalProjectionFull);

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
						false, // bModulateRGB
						nullptr, // TiledVSMProjection
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
