// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DeferredShadingRenderer.cpp: Top level rendering loop for deferred shading
=============================================================================*/

#include "DeferredShadingRenderer.h"
#include "BasePassRendering.h"
#include "VelocityRendering.h"
#include "SingleLayerWaterRendering.h"
#include "SkyAtmosphereRendering.h"
#include "VolumetricCloudRendering.h"
#include "SparseVolumeTexture/SparseVolumeTextureViewerRendering.h"
#include "VolumetricRenderTarget.h"
#include "ScenePrivate.h"
#include "SceneOcclusion.h"
#include "ScreenRendering.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessSubsurface.h"
#include "PostProcess/PostProcessVisualizeCalibrationMaterial.h"
#include "PostProcess/TemporalAA.h"
#include "CompositionLighting/CompositionLighting.h"
#include "FXSystem.h"
#include "OneColorShader.h"
#include "CompositionLighting/PostProcessDeferredDecals.h"
#include "CompositionLighting/PostProcessAmbientOcclusion.h"
#include "DistanceFieldAmbientOcclusion.h"
#include "GlobalDistanceField.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessEyeAdaptation.h"
#include "DistanceFieldAtlas.h"
#include "EngineModule.h"
#include "SceneViewExtension.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"
#include "RendererModule.h"
#include "VT/VirtualTextureFeedback.h"
#include "VT/VirtualTextureSystem.h"
#include "GPUScene.h"
#include "PathTracing.h"
#include "RayTracing/RayTracingMaterialHitShaders.h"
#include "RayTracing/RayTracingLighting.h"
#include "RayTracing/RayTracingDecals.h"
#include "RayTracing/RayTracingScene.h"
#include "RayTracing/RayTracingInstanceMask.h"
#include "RayTracingDynamicGeometryCollection.h"
#include "RayTracingSkinnedGeometry.h"
#include "SceneTextureParameters.h"
#include "ScreenSpaceDenoise.h"
#include "ScreenSpaceRayTracing.h"
#include "RayTracing/RaytracingOptions.h"
#include "RayTracingDefinitions.h"
#include "RayTracingInstance.h"
#include "ShaderPrint.h"
#include "GPUSortManager.h"
#include "HairStrands/HairStrandsRendering.h"
#include "HairStrands/HairStrandsData.h"
#include "PhysicsField/PhysicsFieldComponent.h"
#include "PhysicsFieldRendering.h"
#include "NaniteVisualizationData.h"
#include "Rendering/NaniteResources.h"
#include "Rendering/NaniteStreamingManager.h"
#include "Rendering/NaniteCoarseMeshStreamingManager.h"
#include "SceneTextureReductions.h"
#include "VirtualShadowMaps/VirtualShadowMapCacheManager.h"
#include "Strata/Strata.h"
#include "Lumen/Lumen.h"
#include "Experimental/Containers/SherwoodHashTable.h"
#include "RayTracingGeometryManager.h"
#include "InstanceCulling/InstanceCullingManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Engine/SubsurfaceProfile.h"
#include "SceneCaptureRendering.h"
#include "NaniteSceneProxy.h"
#include "Nanite/NaniteRayTracing.h"
#include "RayTracing/RayTracingInstanceCulling.h"
#include "GPUMessaging.h"
#include "RectLightTextureManager.h"
#include "IESTextureManager.h"
#include "Lumen/LumenFrontLayerTranslucency.h"
#include "Lumen/LumenSceneLighting.h"
#include "Containers/ChunkedArray.h"
#include "Async/ParallelFor.h"
#include "Shadows/ShadowSceneRenderer.h"
#include "HeterogeneousVolumes/HeterogeneousVolumes.h"
#include "ComponentRecreateRenderStateContext.h"
#include "RenderCore.h"
#include "VariableRateShadingImageManager.h"

extern int32 GNaniteShowStats;
extern int32 GNanitePickingDomain;

extern DynamicRenderScaling::FBudget GDynamicNaniteScalingPrimary;

static TAutoConsoleVariable<int32> CVarClearCoatNormal(
	TEXT("r.ClearCoatNormal"),
	0,
	TEXT("0 to disable clear coat normal.\n")
	TEXT(" 0: off\n")
	TEXT(" 1: on"),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarIrisNormal(
	TEXT("r.IrisNormal"),
	0,
	TEXT("0 to disable iris normal.\n")
	TEXT(" 0: off\n")
	TEXT(" 1: on"),
	ECVF_ReadOnly);

int32 GbEnableAsyncComputeTranslucencyLightingVolumeClear = 0; // @todo: disabled due to GPU crashes
static FAutoConsoleVariableRef CVarEnableAsyncComputeTranslucencyLightingVolumeClear(
	TEXT("r.EnableAsyncComputeTranslucencyLightingVolumeClear"),
	GbEnableAsyncComputeTranslucencyLightingVolumeClear,
	TEXT("Whether to clear the translucency lighting volume using async compute.\n"),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

static int32 GRayTracing = 0;
static TAutoConsoleVariable<int32> CVarRayTracing(
	TEXT("r.RayTracing"),
	GRayTracing,
	TEXT("0 to disable ray tracing.\n")
	TEXT(" 0: off\n")
	TEXT(" 1: on"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

static bool bHasRayTracingEnableChanged = false;
static TAutoConsoleVariable<int32> CVarRayTracingEnable(
	TEXT("r.RayTracing.Enable"),
	1,
	TEXT("Runtime toggle for switching raytracing on/off (experimental)."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
		{
			FGlobalComponentRecreateRenderStateContext Context;		 
			ENQUEUE_RENDER_COMMAND(RefreshRayTracingMeshCommandsCmd)(
				[](FRHICommandListImmediate&)
				{
					bHasRayTracingEnableChanged = true;
				}
			);
		}),
	ECVF_RenderThreadSafe
);

int32 GRayTracingUseTextureLod = 0;
static TAutoConsoleVariable<int32> CVarRayTracingTextureLod(
	TEXT("r.RayTracing.UseTextureLod"),
	GRayTracingUseTextureLod,
	TEXT("Enable automatic texture mip level selection in ray tracing material shaders.\n")
	TEXT(" 0: highest resolution mip level is used for all texture (default).\n")
	TEXT(" 1: texture LOD is approximated based on total ray length, output resolution and texel density at hit point (ray cone method)."),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

static int32 GForceAllRayTracingEffects = -1;
static TAutoConsoleVariable<int32> CVarForceAllRayTracingEffects(
	TEXT("r.RayTracing.ForceAllRayTracingEffects"),
	GForceAllRayTracingEffects,
	TEXT("Force all ray tracing effects ON/OFF.\n")
	TEXT(" -1: Do not force (default) \n")
	TEXT(" 0: All ray tracing effects disabled\n")
	TEXT(" 1: All ray tracing effects enabled"),
	ECVF_RenderThreadSafe);

static int32 GRayTracingAllowInline = 1;
static TAutoConsoleVariable<int32> CVarRayTracingAllowInline(
	TEXT("r.RayTracing.AllowInline"),
	GRayTracingAllowInline,
	TEXT("Allow use of Inline Ray Tracing if supported (default=1)."),	
	ECVF_RenderThreadSafe);

static int32 GRayTracingAllowPipeline = 1;
static TAutoConsoleVariable<int32> CVarRayTracingAllowPipeline(
	TEXT("r.RayTracing.AllowPipeline"),
	GRayTracingAllowPipeline,
	TEXT("Allow use of Ray Tracing pipelines if supported (default=1)."),
	ECVF_RenderThreadSafe);

static int32 GRayTracingSceneCaptures = -1;
static FAutoConsoleVariableRef CVarRayTracingSceneCaptures(
	TEXT("r.RayTracing.SceneCaptures"),
	GRayTracingSceneCaptures,
	TEXT("Enable ray tracing in scene captures.\n")
	TEXT(" -1: Use scene capture settings (default) \n")
	TEXT(" 0: off \n")
	TEXT(" 1: on"),
	ECVF_RenderThreadSafe);

static int32 GRayTracingExcludeDecals = 0;
static FAutoConsoleVariableRef CRayTracingExcludeDecals(
	TEXT("r.RayTracing.ExcludeDecals"),
	GRayTracingExcludeDecals,
	TEXT("A toggle that modifies the inclusion of decals in the ray tracing BVH.\n")
	TEXT(" 0: Decals included in the ray tracing BVH (default)\n")
	TEXT(" 1: Decals excluded from the ray tracing BVH"),
	ECVF_RenderThreadSafe);

static int32 GRayTracingExcludeTranslucent = 0;
static FAutoConsoleVariableRef CRayTracingExcludeTranslucent(
	TEXT("r.RayTracing.ExcludeTranslucent"),
	GRayTracingExcludeTranslucent,
	TEXT("A toggle that modifies the inclusion of translucent objects in the ray tracing scene.\n")
	TEXT(" 0: Translucent objects included in the ray tracing scene (default)\n")
	TEXT(" 1: Translucent objects excluded from the ray tracing scene"),
	ECVF_RenderThreadSafe);

static int32 GRayTracingExcludeSky = 1;
static FAutoConsoleVariableRef CRayTracingExcludeSky(
	TEXT("r.RayTracing.ExcludeSky"),
	GRayTracingExcludeSky,
	TEXT("A toggle that controls inclusion of sky geometry in the ray tracing scene (excluding sky can make ray tracing faster).\n")
	TEXT(" 0: Sky objects included in the ray tracing scene\n")
	TEXT(" 1: Sky objects excluded from the ray tracing scene (default)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarRayTracingAsyncBuild(
	TEXT("r.RayTracing.AsyncBuild"),
	0,
	TEXT("Whether to build ray tracing acceleration structures on async compute queue.\n"),
	ECVF_RenderThreadSafe
);

static int32 GRayTracingParallelMeshBatchSetup = 1;
static FAutoConsoleVariableRef CRayTracingParallelMeshBatchSetup(
	TEXT("r.RayTracing.ParallelMeshBatchSetup"),
	GRayTracingParallelMeshBatchSetup,
	TEXT("Whether to setup ray tracing materials via parallel jobs."),
	ECVF_RenderThreadSafe);

static int32 GRayTracingParallelMeshBatchSize = 1024;
static FAutoConsoleVariableRef CRayTracingParallelMeshBatchSize(
	TEXT("r.RayTracing.ParallelMeshBatchSize"),
	GRayTracingParallelMeshBatchSize,
	TEXT("Batch size for ray tracing materials parallel jobs."),
	ECVF_RenderThreadSafe);

static int32 GAsyncCreateLightPrimitiveInteractions = 1;
static FAutoConsoleVariableRef CVarAsyncCreateLightPrimitiveInteractions(
	TEXT("r.AsyncCreateLightPrimitiveInteractions"),
	GAsyncCreateLightPrimitiveInteractions,
	TEXT("Light primitive interactions are created off the render thread in an async task."),
	ECVF_RenderThreadSafe);

static int32 GAsyncCacheMeshDrawCommands = 1;
static FAutoConsoleVariableRef CVarAsyncMeshDrawCommands(
	TEXT("r.AsyncCacheMeshDrawCommands"),
	GAsyncCacheMeshDrawCommands,
	TEXT("Mesh draw command caching is offloaded to an async task."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarRayTracingDynamicGeometryLastRenderTimeUpdateDistance(
	TEXT("r.RayTracing.DynamicGeometryLastRenderTimeUpdateDistance"),
	5000.0f,
	TEXT("Dynamic geometries within this distance will have their LastRenderTime updated, so that visibility based ticking (like skeletal mesh) can work when the component is not directly visible in the view (but reflected)."));

static TAutoConsoleVariable<int32> CVarRayTracingAutoInstance(
	TEXT("r.RayTracing.AutoInstance"),
	1,
	TEXT("Whether to auto instance static meshes\n"),
	ECVF_RenderThreadSafe
);

static int32 GRayTracingDebugDisableTriangleCull = 0;
static FAutoConsoleVariableRef CVarRayTracingDebugDisableTriangleCull(
	TEXT("r.RayTracing.DebugDisableTriangleCull"),
	GRayTracingDebugDisableTriangleCull,
	TEXT("Forces all ray tracing geometry instances to be double-sided by disabling back-face culling. This is useful for debugging and profiling. (default = 0)")
);


static int32 GRayTracingDebugForceOpaque = 0;
static FAutoConsoleVariableRef CVarRayTracingDebugForceOpaque(
	TEXT("r.RayTracing.DebugForceOpaque"),
	GRayTracingDebugForceOpaque,
	TEXT("Forces all ray tracing geometry instances to be opaque, effectively disabling any-hit shaders. This is useful for debugging and profiling. (default = 0)")
);

static int32 GRayTracingMultiGpuTLASMask = 1;
static FAutoConsoleVariableRef CVarRayTracingMultiGpuTLASMask(
	TEXT("r.RayTracing.MultiGpuMaskTLAS"),
	GRayTracingMultiGpuTLASMask,
	TEXT("For Multi-GPU, controls which GPUs TLAS and material pipeline updates run on.  (default = 1)\n")
	TEXT(" 0: Run TLAS and material pipeline updates on all GPUs.  Original behavior, which may be useful for debugging.\n")
	TEXT(" 1: Run TLAS and material pipeline updates masked to the active view's GPUs to improve performance.  BLAS updates still run on all GPUs.")
);

static TAutoConsoleVariable<int32> CVarSceneDepthHZBAsyncCompute(
	TEXT("r.SceneDepthHZBAsyncCompute"), 0,
	TEXT("Selects whether HZB for scene depth buffer should be built with async compute.\n")
	TEXT(" 0: Don't use async compute (default)\n")
	TEXT(" 1: Use async compute, start as soon as possible\n")
	TEXT(" 2: Use async compute, start after ComputeLightGrid.CompactLinks pass"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarShadowsUseSharedExternalAccessQueue(
	TEXT("r.ShadowsUseSharedExternalAccessQueue"), 1,
	TEXT("If enabled, shadows will use the shared external access queue, minimizing unnecessary transitions"),
	ECVF_RenderThreadSafe);


#if RHI_RAYTRACING

static bool bRefreshRayTracingInstances = false;

static void RefreshRayTracingInstancesSinkFunction()
{
	static const auto RayTracingStaticMeshesCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RayTracing.Geometry.StaticMeshes"));
	static const auto RayTracingHISMCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RayTracing.Geometry.HierarchicalInstancedStaticMesh"));
	static const auto RayTracingNaniteProxiesCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RayTracing.Geometry.NaniteProxies"));
	static const auto RayTracingLandscapeGrassCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RayTracing.Geometry.LandscapeGrass"));

	static int32 CachedRayTracingStaticMeshes = RayTracingStaticMeshesCVar->GetValueOnGameThread();
	static int32 CachedRayTracingHISM = RayTracingHISMCVar->GetValueOnGameThread();
	static int32 CachedRayTracingNaniteProxies = RayTracingNaniteProxiesCVar->GetValueOnGameThread();
	static int32 CachedRayTracingLandscapeGrass = RayTracingLandscapeGrassCVar->GetValueOnGameThread();

	const int32 RayTracingStaticMeshes = RayTracingStaticMeshesCVar->GetValueOnGameThread();
	const int32 RayTracingHISM = RayTracingHISMCVar->GetValueOnGameThread();
	const int32 RayTracingNaniteProxies = RayTracingNaniteProxiesCVar->GetValueOnGameThread();
	const int32 RayTracingLandscapeGrass = RayTracingLandscapeGrassCVar->GetValueOnGameThread();

	if (RayTracingStaticMeshes != CachedRayTracingStaticMeshes
		|| RayTracingHISM != CachedRayTracingHISM
		|| RayTracingNaniteProxies != CachedRayTracingNaniteProxies
		|| RayTracingLandscapeGrass != CachedRayTracingLandscapeGrass)
	{
		ENQUEUE_RENDER_COMMAND(RefreshRayTracingInstancesCmd)(
			[](FRHICommandListImmediate&)
			{
				bRefreshRayTracingInstances = true;
			}
		);

		CachedRayTracingStaticMeshes = RayTracingStaticMeshes;
		CachedRayTracingHISM = RayTracingHISM;
		CachedRayTracingNaniteProxies = RayTracingNaniteProxies;
		CachedRayTracingLandscapeGrass = RayTracingLandscapeGrass;
	}
}

static FAutoConsoleVariableSink CVarRefreshRayTracingInstancesSink(FConsoleCommandDelegate::CreateStatic(&RefreshRayTracingInstancesSinkFunction));

#endif // RHI_RAYTRACING

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarForceBlackVelocityBuffer(
	TEXT("r.Test.ForceBlackVelocityBuffer"), 0,
	TEXT("Force the velocity buffer to have no motion vector for debugging purpose."),
	ECVF_RenderThreadSafe);
#endif

static TAutoConsoleVariable<int32> CVarNaniteViewMeshLODBiasEnable(
	TEXT("r.Nanite.ViewMeshLODBias.Enable"), 1,
	TEXT("Whether LOD offset to apply for rasterized Nanite meshes for the main viewport should be based off TSR's ScreenPercentage (Enabled by default)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarNaniteViewMeshLODBiasOffset(
	TEXT("r.Nanite.ViewMeshLODBias.Offset"), 0.0f,
	TEXT("LOD offset to apply for rasterized Nanite meshes for the main viewport when using TSR (Default = 0)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarNaniteViewMeshLODBiasMin(
	TEXT("r.Nanite.ViewMeshLODBias.Min"), -2.0f,
	TEXT("Minimum LOD offset for rasterizing Nanite meshes for the main viewport (Default = -2)."),
	ECVF_RenderThreadSafe);

static int32 GNaniteProgrammableRasterPrimary = 1;
static FAutoConsoleVariableRef CNaniteProgrammableRasterPrimary(
	TEXT("r.Nanite.ProgrammableRaster.Primary"),
	GNaniteProgrammableRasterPrimary,
	TEXT("A toggle that allows Nanite programmable raster in the primary pass.\n")
	TEXT(" 0: Programmable raster is disabled\n")
	TEXT(" 1: Programmable raster is enabled (default)"),
	ECVF_RenderThreadSafe);

namespace Lumen
{
	extern bool AnyLumenHardwareRayTracingPassEnabled();
}
namespace Nanite
{
	extern bool IsStatFilterActive(const FString& FilterName);
	extern void ListStatFilters(FSceneRenderer* SceneRenderer);
}
extern bool ShouldVisualizeLightGrid();

DECLARE_CYCLE_STAT(TEXT("InitViews Intentional Stall"), STAT_InitViews_Intentional_Stall, STATGROUP_InitViews);

DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer UpdateDownsampledDepthSurface"), STAT_FDeferredShadingSceneRenderer_UpdateDownsampledDepthSurface, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer Render Init"), STAT_FDeferredShadingSceneRenderer_Render_Init, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer FGlobalDynamicVertexBuffer Commit"), STAT_FDeferredShadingSceneRenderer_FGlobalDynamicVertexBuffer_Commit, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer FXSystem PreRender"), STAT_FDeferredShadingSceneRenderer_FXSystem_PreRender, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer AllocGBufferTargets"), STAT_FDeferredShadingSceneRenderer_AllocGBufferTargets, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer DBuffer"), STAT_FDeferredShadingSceneRenderer_DBuffer, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer ResolveDepth After Basepass"), STAT_FDeferredShadingSceneRenderer_ResolveDepth_After_Basepass, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer Resolve After Basepass"), STAT_FDeferredShadingSceneRenderer_Resolve_After_Basepass, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer FXSystem PostRenderOpaque"), STAT_FDeferredShadingSceneRenderer_FXSystem_PostRenderOpaque, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer AfterBasePass"), STAT_FDeferredShadingSceneRenderer_AfterBasePass, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer Lighting"), STAT_FDeferredShadingSceneRenderer_Lighting, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer RenderLightShaftOcclusion"), STAT_FDeferredShadingSceneRenderer_RenderLightShaftOcclusion, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer RenderAtmosphere"), STAT_FDeferredShadingSceneRenderer_RenderAtmosphere, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer RenderSkyAtmosphere"), STAT_FDeferredShadingSceneRenderer_RenderSkyAtmosphere, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer RenderFog"), STAT_FDeferredShadingSceneRenderer_RenderFog, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer RenderLightShaftBloom"), STAT_FDeferredShadingSceneRenderer_RenderLightShaftBloom, STATGROUP_SceneRendering);
DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer RenderFinish"), STAT_FDeferredShadingSceneRenderer_RenderFinish, STATGROUP_SceneRendering);

DECLARE_GPU_STAT(RayTracingUpdate);
DECLARE_GPU_STAT(RayTracingScene);
DECLARE_GPU_STAT(RayTracingGeometry);

DECLARE_GPU_DRAWCALL_STAT(Postprocessing);
DECLARE_GPU_STAT(VisibilityCommands);
DECLARE_GPU_STAT(RenderDeferredLighting);
DECLARE_GPU_STAT(AllocateRendertargets);
DECLARE_GPU_STAT(FrameRenderFinish);
DECLARE_GPU_STAT(SortLights);
DECLARE_GPU_STAT(PostRenderOpsFX);
DECLARE_GPU_STAT(GPUSceneUpdate);
DECLARE_GPU_STAT_NAMED(Unaccounted, TEXT("[unaccounted]"));
DECLARE_GPU_STAT(WaterRendering);
DECLARE_GPU_STAT(HairRendering);
DEFINE_GPU_DRAWCALL_STAT(VirtualTextureUpdate);
DECLARE_GPU_STAT(UploadDynamicBuffers);
DECLARE_GPU_STAT(PostOpaqueExtensions);

DECLARE_GPU_STAT_NAMED(NaniteVisBuffer, TEXT("Nanite VisBuffer"));

DECLARE_DWORD_COUNTER_STAT(TEXT("BasePass Total Raster Bins"), STAT_NaniteBasePassTotalRasterBins, STATGROUP_Nanite);
DECLARE_DWORD_COUNTER_STAT(TEXT("BasePass Total Shading Draws"), STAT_NaniteBasePassTotalShadingDraws, STATGROUP_Nanite);

DECLARE_DWORD_COUNTER_STAT(TEXT("BasePass Visible Raster Bins"), STAT_NaniteBasePassVisibleRasterBins, STATGROUP_Nanite);
DECLARE_DWORD_COUNTER_STAT(TEXT("BasePass Visible Shading Draws"), STAT_NaniteBasePassVisibleShadingDraws, STATGROUP_Nanite);

CSV_DEFINE_CATEGORY(LightCount, true);

/*-----------------------------------------------------------------------------
	Global Illumination Plugin Function Delegates
-----------------------------------------------------------------------------*/

static FGlobalIlluminationPluginDelegates::FAnyRayTracingPassEnabled GIPluginAnyRaytracingPassEnabledDelegate;
FGlobalIlluminationPluginDelegates::FAnyRayTracingPassEnabled& FGlobalIlluminationPluginDelegates::AnyRayTracingPassEnabled()
{
	return GIPluginAnyRaytracingPassEnabledDelegate;
}

static FGlobalIlluminationPluginDelegates::FPrepareRayTracing GIPluginPrepareRayTracingDelegate;
FGlobalIlluminationPluginDelegates::FPrepareRayTracing& FGlobalIlluminationPluginDelegates::PrepareRayTracing()
{
	return GIPluginPrepareRayTracingDelegate;
}

static FGlobalIlluminationPluginDelegates::FRenderDiffuseIndirectLight GIPluginRenderDiffuseIndirectLightDelegate;
FGlobalIlluminationPluginDelegates::FRenderDiffuseIndirectLight& FGlobalIlluminationPluginDelegates::RenderDiffuseIndirectLight()
{
	return GIPluginRenderDiffuseIndirectLightDelegate;
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static FGlobalIlluminationPluginDelegates::FRenderDiffuseIndirectVisualizations GIPluginRenderDiffuseIndirectVisualizationsDelegate;
FGlobalIlluminationPluginDelegates::FRenderDiffuseIndirectVisualizations& FGlobalIlluminationPluginDelegates::RenderDiffuseIndirectVisualizations()
{
	return GIPluginRenderDiffuseIndirectVisualizationsDelegate;
}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

const TCHAR* GetDepthPassReason(bool bDitheredLODTransitionsUseStencil, EShaderPlatform ShaderPlatform)
{
	if (IsForwardShadingEnabled(ShaderPlatform))
	{
		return TEXT("(Forced by ForwardShading)");
	}

	if (UseNanite(ShaderPlatform))
	{
		return TEXT("(Forced by Nanite)");
	}

	if (IsUsingDBuffers(ShaderPlatform))
	{
		return TEXT("(Forced by DBuffer)");
	}

	if (UseVirtualTexturing(ShaderPlatform))
	{
		return TEXT("(Forced by VirtualTexture)");
	}

	if (bDitheredLODTransitionsUseStencil)
	{
		return TEXT("(Forced by StencilLODDither)");
	}

	return TEXT("");
}

/*-----------------------------------------------------------------------------
	FDeferredShadingSceneRenderer
-----------------------------------------------------------------------------*/

FDeferredShadingSceneRenderer::FDeferredShadingSceneRenderer(const FSceneViewFamily* InViewFamily, FHitProxyConsumer* HitProxyConsumer)
	: FSceneRenderer(InViewFamily, HitProxyConsumer)
	, DepthPass(GetDepthPassInfo(Scene))
	, bAreLightsInLightGrid(false)
{
	ViewPipelineStates.SetNum(Views.Num());

	ShadowSceneRenderer = MakeUnique<FShadowSceneRenderer>(*this);
}

/** 
* Renders the view family. 
*/

DEFINE_STAT(STAT_CLM_PrePass);
DECLARE_CYCLE_STAT(TEXT("FXPreRender"), STAT_CLM_FXPreRender, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AfterPrePass"), STAT_CLM_AfterPrePass, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("Lighting"), STAT_CLM_Lighting, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AfterLighting"), STAT_CLM_AfterLighting, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("WaterPass"), STAT_CLM_WaterPass, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("Translucency"), STAT_CLM_Translucency, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("Distortion"), STAT_CLM_Distortion, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AfterTranslucency"), STAT_CLM_AfterTranslucency, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("RenderDistanceFieldLighting"), STAT_CLM_RenderDistanceFieldLighting, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("LightShaftBloom"), STAT_CLM_LightShaftBloom, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("PostProcessing"), STAT_CLM_PostProcessing, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("Velocity"), STAT_CLM_Velocity, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AfterVelocity"), STAT_CLM_AfterVelocity, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("TranslucentVelocity"), STAT_CLM_TranslucentVelocity, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("RenderFinish"), STAT_CLM_RenderFinish, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("AfterFrame"), STAT_CLM_AfterFrame, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("Wait RayTracing Add Mesh Batch"), STAT_WaitRayTracingAddMesh, STATGROUP_SceneRendering);

FGlobalDynamicIndexBuffer FDeferredShadingSceneRenderer::DynamicIndexBufferForInitViews;
FGlobalDynamicIndexBuffer FDeferredShadingSceneRenderer::DynamicIndexBufferForInitShadows;
FGlobalDynamicVertexBuffer FDeferredShadingSceneRenderer::DynamicVertexBufferForInitViews;
FGlobalDynamicVertexBuffer FDeferredShadingSceneRenderer::DynamicVertexBufferForInitShadows;
TGlobalResource<FGlobalDynamicReadBuffer> FDeferredShadingSceneRenderer::DynamicReadBufferForInitShadows;
TGlobalResource<FGlobalDynamicReadBuffer> FDeferredShadingSceneRenderer::DynamicReadBufferForInitViews;

/**
 * Returns true if the depth Prepass needs to run
 */
bool FDeferredShadingSceneRenderer::ShouldRenderPrePass() const
{
	return (DepthPass.EarlyZPassMode != DDM_None || DepthPass.bEarlyZPassMovable != 0);
}

bool FDeferredShadingSceneRenderer::RenderHzb(FRDGBuilder& GraphBuilder, FRDGTextureRef SceneDepthTexture, const FBuildHZBAsyncComputeParams* AsyncComputeParams)
{
	RDG_GPU_STAT_SCOPE(GraphBuilder, HZB);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);

		FSceneViewState* ViewState = View.ViewState;
		const FPerViewPipelineState& ViewPipelineState = GetViewPipelineState(View);


		if (ViewPipelineState.bClosestHZB || ViewPipelineState.bFurthestHZB)
		{
			RDG_EVENT_SCOPE(GraphBuilder, "BuildHZB(ViewId=%d)", ViewIndex);

			FRDGTextureRef ClosestHZBTexture = nullptr;
			FRDGTextureRef FurthestHZBTexture = nullptr;

			BuildHZB(
				GraphBuilder,
				SceneDepthTexture,
				/* VisBufferTexture = */ nullptr,
				View.ViewRect,
				View.GetFeatureLevel(),
				View.GetShaderPlatform(),
				TEXT("HZBClosest"),
				/* OutClosestHZBTexture = */ ViewPipelineState.bClosestHZB ? &ClosestHZBTexture : nullptr,
				TEXT("HZBFurthest"),
				/* OutFurthestHZBTexture = */ &FurthestHZBTexture,
				BuildHZBDefaultPixelFormat,
				AsyncComputeParams);

			// Update the view.
			{
				View.HZBMipmap0Size = FurthestHZBTexture->Desc.Extent;
				View.HZB = FurthestHZBTexture;

				// Extract furthest HZB texture.
				if (View.ViewState)
				{
					if (IsNaniteEnabled() || FInstanceCullingContext::IsOcclusionCullingEnabled())
					{
						GraphBuilder.QueueTextureExtraction(FurthestHZBTexture, &View.ViewState->PrevFrameViewInfo.HZB);
					}
					else
					{
						View.ViewState->PrevFrameViewInfo.HZB = nullptr;
					}
				}

				// Extract closest HZB texture.
				if (ViewPipelineState.bClosestHZB)
				{
					View.ClosestHZB = ClosestHZBTexture;
				}
			}
		}

		if (FamilyPipelineState->bHZBOcclusion && ViewState && ViewState->HZBOcclusionTests.GetNum() != 0)
		{
			check(ViewState->HZBOcclusionTests.IsValidFrame(ViewState->OcclusionFrameCounter));
			ViewState->HZBOcclusionTests.Submit(GraphBuilder, View);
		}
	}

	return FamilyPipelineState->bHZBOcclusion;
}

BEGIN_SHADER_PARAMETER_STRUCT(FRenderOpaqueFXPassParameters, )
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTextures)
END_SHADER_PARAMETER_STRUCT()

static void RenderOpaqueFX(
	FRDGBuilder& GraphBuilder,
	TArrayView<const FViewInfo> Views,
	FFXSystemInterface* FXSystem,
	TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTexturesUniformBuffer)
{
	// Notify the FX system that opaque primitives have been rendered and we now have a valid depth buffer.
	if (FXSystem && Views.Num() > 0)
	{
		RDG_GPU_STAT_SCOPE(GraphBuilder, PostRenderOpsFX);
		RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, RenderOpaqueFX);

		const ERDGPassFlags UBPassFlags = ERDGPassFlags::Compute | ERDGPassFlags::Raster | ERDGPassFlags::SkipRenderPass | ERDGPassFlags::NeverCull;

		// Add a pass which extracts the RHI handle from the scene textures UB and sends it to the FX system.
		FRenderOpaqueFXPassParameters* ExtractUBPassParameters = GraphBuilder.AllocParameters<FRenderOpaqueFXPassParameters>();
		ExtractUBPassParameters->SceneTextures = SceneTexturesUniformBuffer;
		GraphBuilder.AddPass(RDG_EVENT_NAME("SetSceneTexturesUniformBuffer"), ExtractUBPassParameters, UBPassFlags, [ExtractUBPassParameters, FXSystem](FRHICommandListImmediate&)
		{
			FXSystem->SetSceneTexturesUniformBuffer(ExtractUBPassParameters->SceneTextures->GetRHIRef());
		});

		FXSystem->PostRenderOpaque(GraphBuilder, Views, true /*bAllowGPUParticleUpdate*/);

		// Clear the scene textures UB pointer on the FX system. Use the same pass parameters to extend resource lifetimes.
		GraphBuilder.AddPass(RDG_EVENT_NAME("UnsetSceneTexturesUniformBuffer"), ExtractUBPassParameters, UBPassFlags, [FXSystem](FRHICommandListImmediate&)
		{
			FXSystem->SetSceneTexturesUniformBuffer({});
		});

		if (FGPUSortManager* GPUSortManager = FXSystem->GetGPUSortManager())
		{
			GPUSortManager->OnPostRenderOpaque(GraphBuilder);
		}

		GraphBuilder.AddDispatchHint();
	}
}

#if RHI_RAYTRACING

static bool ShouldPrepareRayTracingDecals(const FScene& Scene, const FSceneViewFamily& ViewFamily)
{
	if (!IsRayTracingEnabled() || !RHISupportsRayTracingCallableShaders(ViewFamily.GetShaderPlatform()))
	{
		return false;
	}

	if (Scene.Decals.Num() == 0 || GRayTracingExcludeDecals)
	{
		return false;
	}

	return ViewFamily.EngineShowFlags.PathTracing && PathTracing::UsesDecals(ViewFamily);
}

static void AddDebugRayTracingInstanceFlags(ERayTracingInstanceFlags& InOutFlags)
{
	if (GRayTracingDebugForceOpaque)
	{
		InOutFlags |= ERayTracingInstanceFlags::ForceOpaque;
	}
	if (GRayTracingDebugDisableTriangleCull)
	{
		InOutFlags |= ERayTracingInstanceFlags::TriangleCullDisable;
	}
}

struct FRayTracingRelevantPrimitive
{
	FRHIRayTracingGeometry* RayTracingGeometryRHI = nullptr;
	TArrayView<const int32> CachedRayTracingMeshCommandIndices; // Pointer to FPrimitiveSceneInfo::CachedRayTracingMeshCommandIndicesPerLOD data
	uint64 StateHash = 0;
	int32 PrimitiveIndex = -1;
	int8 LODIndex = -1;
	uint8 InstanceMask = 0;
	bool bStatic = false;
	bool bAllSegmentsOpaque = true;
	bool bAllSegmentsCastShadow = true;
	bool bAnySegmentsCastShadow = false;
	bool bAnySegmentsDecal = false;
	bool bAllSegmentsDecal = true;
	bool bTwoSided = false;
	bool bIsSky = false;
	bool bAllSegmentsTranslucent = true;

	uint64 InstancingKey() const
	{
		uint64 Key = StateHash;
		Key ^= uint64(InstanceMask) << 32;
		Key ^= bAllSegmentsOpaque ? 0x1ull << 40 : 0x0;
		Key ^= bAllSegmentsCastShadow ? 0x1ull << 41 : 0x0;
		Key ^= bAnySegmentsCastShadow ? 0x1ull << 42 : 0x0;
		Key ^= bAnySegmentsDecal ? 0x1ull << 43 : 0x0;
		Key ^= bAllSegmentsDecal ? 0x1ull << 44 : 0x0;
		Key ^= bTwoSided ? 0x1ull << 45 : 0x0;
		Key ^= bIsSky ? 0x1ull << 46 : 0x0;
		Key ^= bAllSegmentsTranslucent ? 0x1ull << 47 : 0x0;
		return Key ^ reinterpret_cast<uint64>(RayTracingGeometryRHI);
	}

	void UpdateMasks(const ERayTracingPrimitiveFlags Flags, ERayTracingViewMaskMode MaskMode)
	{
		FRayTracingMeshCommand Command;
		Command.InstanceMask = InstanceMask;
		Command.bOpaque = bAllSegmentsOpaque;
		Command.bCastRayTracedShadows = bAnySegmentsCastShadow;
		Command.bDecal = bAnySegmentsDecal;
		Command.bTwoSided = bTwoSided;
		Command.bIsSky = bIsSky;
		Command.bIsTranslucent = bAllSegmentsTranslucent;

		UpdateRayTracingMeshCommandMasks(Command, Flags, MaskMode);

		InstanceMask = Command.InstanceMask;
	}
};

struct FRayTracingRelevantPrimitiveList
{
	// Filtered lists of relevant primitives
	TChunkedArray<FRayTracingRelevantPrimitive> StaticPrimitives;
	TChunkedArray<FRayTracingRelevantPrimitive> DynamicPrimitives;

	// Relevant static primitive LODs are computed asynchronously.
	// This task must complete before accessing StaticPrimitives in FRayTracingSceneAddInstancesTask.
	FGraphEventRef StaticPrimitiveLODTask;

	// Array of primitives that should update their cached ray tracing instances via FPrimitiveSceneInfo::UpdateCachedRaytracingData()
	TArray<FPrimitiveSceneInfo*> DirtyCachedRayTracingPrimitives;

	// Used coarse mesh streaming handles during the last TLAS build
	TArray<Nanite::CoarseMeshStreamingHandle> UsedCoarseMeshStreamingHandles;

	// Indicates that this object has been fully produced (for validation)
	bool bValid = false;
};

// Iterates over Scene's PrimitiveSceneProxies and extracts ones that are relevant for ray tracing.
// This function can run on any thread.
static void GatherRayTracingRelevantPrimitives(const FScene& Scene, const FViewInfo& View, FRayTracingRelevantPrimitiveList& Result)
{
	Result.DirtyCachedRayTracingPrimitives.Reserve(Scene.PrimitiveSceneProxies.Num());

	const bool bGameView = View.bIsGameView || View.Family->EngineShowFlags.Game;

	bool bPerformRayTracing = View.State != nullptr && !View.bIsReflectionCapture && View.bAllowRayTracing;
	if (bPerformRayTracing)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GatherRayTracingRelevantPrimitives);

		// Index into the TypeOffsetTable, which contains a prefix sum of primitive indices by proxy type
		int32 BroadIndex = 0;

		for (int PrimitiveIndex = 0; PrimitiveIndex < Scene.PrimitiveSceneProxies.Num(); PrimitiveIndex++)
		{
			// Find the next TypeOffsetTable entry that's relevant to this primitive index.
			while (PrimitiveIndex >= int(Scene.TypeOffsetTable[BroadIndex].Offset))
			{
				BroadIndex++;
			}

			const ERayTracingPrimitiveFlags Flags = Scene.PrimitiveRayTracingFlags[PrimitiveIndex];

			// Skip before dereferencing SceneInfo
			if (Flags == ERayTracingPrimitiveFlags::UnsupportedProxyType)
			{
				// Find the index of a proxy of the next type, skipping over a batch of proxies that are the same type as current.
				// This assumes that FPrimitiveSceneProxy::IsRayTracingRelevant() is consistent for all proxies of the same type.
				// I.e. does not depend on members of the particular FPrimitiveSceneProxy implementation.
				PrimitiveIndex = Scene.TypeOffsetTable[BroadIndex].Offset - 1;
				continue;
			}

			// Get primitive visibility state from culling
			if (!View.PrimitiveRayTracingVisibilityMap[PrimitiveIndex])
			{
				continue;
			}

			check(!EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::Excluded));

			const FPrimitiveSceneInfo* SceneInfo = Scene.Primitives[PrimitiveIndex];

			// #dxr_todo: ray tracing in scene captures should re-use the persistent RT scene. (UE-112448)
			bool bShouldRayTraceSceneCapture = GRayTracingSceneCaptures > 0
				|| (GRayTracingSceneCaptures == -1 && View.bSceneCaptureUsesRayTracing);

			if (View.bIsSceneCapture && (!bShouldRayTraceSceneCapture || !SceneInfo->bIsVisibleInSceneCaptures))
			{
				continue;
			}

			if (!View.bIsSceneCapture && SceneInfo->bIsVisibleInSceneCapturesOnly)
			{
				continue;
			}

			// Some primitives should only be visible editor mode, however far field geometry 
			// and hidden shadow casters must still always be added to the RT scene.
			if (bGameView && !SceneInfo->bDrawInGame && !SceneInfo->bRayTracingFarField)
			{
				// Make sure this isn't an object that wants to be hidden to camera but still wants to cast shadows or be visible to indirect
				checkf(SceneInfo->Proxy != nullptr, TEXT("SceneInfo does not have a valid Proxy object. If this occurs, this object should probably have been filtered out before being added to Scene.Primitives"));
				if (!SceneInfo->Proxy->CastsHiddenShadow() && !SceneInfo->Proxy->AffectsIndirectLightingWhileHidden())
				{
					continue;
				}
			}

			// Marked visible and used after point, check if streaming then mark as used in the TLAS (so it can be streamed in)
			if (EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::Streaming))
			{
				check(SceneInfo->CoarseMeshStreamingHandle != INDEX_NONE);
				Result.UsedCoarseMeshStreamingHandles.Add(SceneInfo->CoarseMeshStreamingHandle);
			}

			// Is the cached data dirty?
			// eg: mesh was streamed in/out
			if (SceneInfo->bCachedRaytracingDataDirty)
			{
				Result.DirtyCachedRayTracingPrimitives.Add(Scene.Primitives[PrimitiveIndex]);
			}

			FRayTracingRelevantPrimitive Item;
			Item.PrimitiveIndex = PrimitiveIndex;

			if (EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::StaticMesh))
			{
				if (View.Family->EngineShowFlags.StaticMeshes)
				{
					Item.bStatic = true;
					Result.StaticPrimitives.AddElement(Item);
				}
			}
			else if (View.Family->EngineShowFlags.SkeletalMeshes)
			{
				checkf(!EnumHasAllFlags(Flags, ERayTracingPrimitiveFlags::CacheInstances),
					TEXT("Only static primitives are expected to use CacheInstances flag."));

				Item.bStatic = false;
				Result.DynamicPrimitives.AddElement(Item);
			}
		}
	}

	static const auto ICVarStaticMeshLODDistanceScale = IConsoleManager::Get().FindConsoleVariable(TEXT("r.StaticMeshLODDistanceScale"));
	const float LODScaleCVarValue = ICVarStaticMeshLODDistanceScale->GetFloat();
	const int32 ForcedLODLevel = GetCVarForceLOD();

	Result.StaticPrimitiveLODTask = FFunctionGraphTask::CreateAndDispatchWhenReady(
		[&Result, &Scene, &View, LODScaleCVarValue, ForcedLODLevel]()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GatherRayTracingWorldInstances_ComputeLOD);

		ParallelFor(TEXT("GatherRayTracingRelevantPrimitives_ComputeLOD"), Result.StaticPrimitives.Num(), 128,
			[&Result, &Scene, &View, LODScaleCVarValue, ForcedLODLevel](int32 ItemIndex) 
		{
			FRayTracingRelevantPrimitive& RelevantPrimitive = Result.StaticPrimitives[ItemIndex];

			const int32 PrimitiveIndex = RelevantPrimitive.PrimitiveIndex;
			const FPrimitiveSceneInfo* SceneInfo = Scene.Primitives[PrimitiveIndex];
			const ERayTracingPrimitiveFlags Flags = Scene.PrimitiveRayTracingFlags[PrimitiveIndex];

			int8 LODIndex = 0;

			if (EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::ComputeLOD))
			{
				const FPrimitiveBounds& Bounds = Scene.PrimitiveBounds[PrimitiveIndex];
				const FPrimitiveSceneInfo* RESTRICT PrimitiveSceneInfo = Scene.Primitives[PrimitiveIndex];

				FLODMask LODToRender;

				const int8 CurFirstLODIdx = PrimitiveSceneInfo->Proxy->GetCurrentFirstLODIdx_RenderThread();
				check(CurFirstLODIdx >= 0);

				float MeshScreenSizeSquared = 0;
				float LODScale = LODScaleCVarValue * View.LODDistanceFactor;
				LODToRender = ComputeLODForMeshes(SceneInfo->StaticMeshRelevances, View, Bounds.BoxSphereBounds.Origin, Bounds.BoxSphereBounds.SphereRadius, ForcedLODLevel, MeshScreenSizeSquared, CurFirstLODIdx, LODScale, true);

				LODIndex = LODToRender.GetRayTracedLOD();
			}

			if (!EnumHasAllFlags(Flags, ERayTracingPrimitiveFlags::CacheInstances))
			{
				FRHIRayTracingGeometry* RayTracingGeometryInstance = SceneInfo->GetStaticRayTracingGeometryInstance(LODIndex);
				if (RayTracingGeometryInstance == nullptr)
				{
					return;
				}

				// Sometimes LODIndex is out of range because it is clamped by ClampToFirstLOD, like the requested LOD is being streamed in and hasn't been available
				// According to InitViews, we should hide the static mesh instance
				check(EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::CacheMeshCommands));
				if (SceneInfo->CachedRayTracingMeshCommandIndicesPerLOD.IsValidIndex(LODIndex))
				{
					RelevantPrimitive.LODIndex = LODIndex;
					RelevantPrimitive.RayTracingGeometryRHI = RayTracingGeometryInstance;

					RelevantPrimitive.CachedRayTracingMeshCommandIndices = SceneInfo->CachedRayTracingMeshCommandIndicesPerLOD[LODIndex];
					RelevantPrimitive.StateHash = SceneInfo->CachedRayTracingMeshCommandsHashPerLOD[LODIndex];
					
					// TODO: Cache these flags to avoid having to loop over the RayTracingMeshCommands
					for (int32 CommandIndex : RelevantPrimitive.CachedRayTracingMeshCommandIndices)
					{
						if (CommandIndex >= 0)
						{
							const FRayTracingMeshCommand& RayTracingMeshCommand = Scene.CachedRayTracingMeshCommands[CommandIndex];

							RelevantPrimitive.InstanceMask |= RayTracingMeshCommand.InstanceMask;
							RelevantPrimitive.bAllSegmentsOpaque &= RayTracingMeshCommand.bOpaque;
							RelevantPrimitive.bAllSegmentsCastShadow &= RayTracingMeshCommand.bCastRayTracedShadows;
							RelevantPrimitive.bAnySegmentsCastShadow |= RayTracingMeshCommand.bCastRayTracedShadows;
							RelevantPrimitive.bAnySegmentsDecal |= RayTracingMeshCommand.bDecal;
							RelevantPrimitive.bAllSegmentsDecal &= RayTracingMeshCommand.bDecal;
							RelevantPrimitive.bTwoSided |= RayTracingMeshCommand.bTwoSided;
							RelevantPrimitive.bIsSky |= RayTracingMeshCommand.bIsSky;
							RelevantPrimitive.bAllSegmentsTranslucent &= RayTracingMeshCommand.bIsTranslucent;
						}
						else
						{
							// CommandIndex == -1 indicates that the mesh batch has been filtered by FRayTracingMeshProcessor (like the shadow depth pass batch)
							// Do nothing in this case
						}
					}

					ERayTracingViewMaskMode MaskMode = static_cast<ERayTracingViewMaskMode>(Scene.CachedRayTracingMeshCommandsMode);

					RelevantPrimitive.UpdateMasks(Flags, MaskMode);
				}
			}
		});
	}, TStatId(), nullptr, ENamedThreads::AnyThread);

	Result.bValid = true;
}

// Class to implement build instance mask and flags so that rendering related mask build is maintained in any renderer module.
// BuildInstanceMaskAndFlags() will be called in the Engine module where it does not know specifics of the ray tracing instance
// masks used by the renderer (e.g., path tracer mask might be different from raytracing mask).
struct FDeferredShadingRayTracingMaterialGatheringContext : public FRayTracingMaterialGatheringContext
{
	FDeferredShadingRayTracingMaterialGatheringContext(
		const FScene* InScene,
		const FSceneView* InReferenceView,
		const FSceneViewFamily& InReferenceViewFamily,
		FRDGBuilder& InGraphBuilder,
		FRayTracingMeshResourceCollector& InRayTracingMeshResourceCollector)
		:FRayTracingMaterialGatheringContext(InScene, InReferenceView, InReferenceViewFamily, InGraphBuilder, InRayTracingMeshResourceCollector){}

	virtual FRayTracingMaskAndFlags BuildInstanceMaskAndFlags(const FRayTracingInstance& Instance, const FPrimitiveSceneProxy& ScenePrimitive) override
	{
		return BuildRayTracingInstanceMaskAndFlags(Instance, ScenePrimitive, &ReferenceViewFamily);
	}
};

bool FDeferredShadingSceneRenderer::GatherRayTracingWorldInstancesForView(FRDGBuilder& GraphBuilder, FViewInfo& View, FRayTracingScene& RayTracingScene, FRayTracingRelevantPrimitiveList& RelevantPrimitiveList)
{
	checkf(IsRayTracingEnabled() && bAnyRayTracingPassEnabled, TEXT("GatherRayTracingWorldInstancesForView should only be called if ray tracing is used"))

	TRACE_CPUPROFILER_EVENT_SCOPE(FDeferredShadingSceneRenderer::GatherRayTracingWorldInstances);
	SCOPE_CYCLE_COUNTER(STAT_GatherRayTracingWorldInstances);

	// Ensure that any invalidated cached uniform expressions have been updated on the rendering thread.
	// Normally this work is done through FMaterialRenderProxy::UpdateUniformExpressionCacheIfNeeded,
	// however ray tracing material processing (FMaterialShader::GetShaderBindings, which accesses UniformExpressionCache)
	// is done on task threads, therefore all work must be done here up-front as UpdateUniformExpressionCacheIfNeeded is not free-threaded.
	FMaterialRenderProxy::UpdateDeferredCachedUniformExpressions();

	RayTracingCollector.ClearViewMeshArrays();

	FGPUScenePrimitiveCollector DummyDynamicPrimitiveCollector;

	RayTracingCollector.AddViewMeshArrays(
		&View,
		&View.RayTracedDynamicMeshElements,
		&View.SimpleElementCollector,
		&DummyDynamicPrimitiveCollector,
		ViewFamily.GetFeatureLevel(),
		&DynamicIndexBufferForInitViews,
		&DynamicVertexBufferForInitViews,
		&DynamicReadBufferForInitViews
	);

	View.DynamicRayTracingMeshCommandStorage.Reserve(Scene->Primitives.Num());
	View.VisibleRayTracingMeshCommands.Reserve(Scene->Primitives.Num());

	extern TSet<IPersistentViewUniformBufferExtension*> PersistentViewUniformBufferExtensions;

	for (IPersistentViewUniformBufferExtension* Extension : PersistentViewUniformBufferExtensions)
	{
		Extension->BeginRenderView(&View);
	}

	View.RayTracingMeshResourceCollector = MakeUnique<FRayTracingMeshResourceCollector>(
		Scene->GetFeatureLevel(),
		Allocator,
		&DynamicIndexBufferForInitViews,
		&DynamicVertexBufferForInitViews,
		&DynamicReadBufferForInitViews);

	View.RayTracingCullingParameters.Init(View);

	FDeferredShadingRayTracingMaterialGatheringContext MaterialGatheringContext
	(
		Scene,
		&View,
		ViewFamily,
		GraphBuilder,
		*View.RayTracingMeshResourceCollector
	);

	const float CurrentWorldTime = View.Family->Time.GetWorldTimeSeconds();

	// Consume output of the relevant primitive gathering task
	RayTracingScene.UsedCoarseMeshStreamingHandles = MoveTemp(RelevantPrimitiveList.UsedCoarseMeshStreamingHandles);

	// Inform the coarse mesh streaming manager about all the used streamable render assets in the scene
	Nanite::FCoarseMeshStreamingManager* CoarseMeshSM = IStreamingManager::Get().GetNaniteCoarseMeshStreamingManager();
	if (CoarseMeshSM)
	{
		CoarseMeshSM->AddUsedStreamingHandles(RayTracingScene.UsedCoarseMeshStreamingHandles);
	}

	INC_DWORD_STAT_BY(STAT_VisibleRayTracingPrimitives, RelevantPrimitiveList.DynamicPrimitives.Num() + RelevantPrimitiveList.StaticPrimitives.Num());

	FPrimitiveSceneInfo::UpdateCachedRaytracingData(Scene, RelevantPrimitiveList.DirtyCachedRayTracingPrimitives);

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GatherRayTracingWorldInstances_DynamicElements);

		const bool bParallelMeshBatchSetup = GRayTracingParallelMeshBatchSetup && FApp::ShouldUseThreadingForPerformance();

		const int64 SharedBufferGenerationID = Scene->GetRayTracingDynamicGeometryCollection()->BeginUpdate();

		struct FRayTracingMeshBatchWorkItem
		{
			const FPrimitiveSceneProxy* SceneProxy = nullptr;
			TArray<FMeshBatch> MeshBatchesOwned;
			TArrayView<const FMeshBatch> MeshBatchesView;
			uint32 InstanceIndex;
			uint32 DecalInstanceIndex;

			TArrayView<const FMeshBatch> GetMeshBatches() const
			{
				if (MeshBatchesOwned.Num())
				{
					check(MeshBatchesView.Num() == 0);
					return TArrayView<const FMeshBatch>(MeshBatchesOwned);
				}
				else
				{
					check(MeshBatchesOwned.Num() == 0);
					return MeshBatchesView;
				}
			}
		};

		static constexpr uint32 MaxWorkItemsPerPage = 128; // Try to keep individual pages small to avoid slow-path memory allocations
		struct FRayTracingMeshBatchTaskPage
		{
			FRayTracingMeshBatchWorkItem WorkItems[MaxWorkItemsPerPage];
			uint32 NumWorkItems = 0;
			FRayTracingMeshBatchTaskPage* Next = nullptr;
		};

		FRayTracingMeshBatchTaskPage* MeshBatchTaskHead = nullptr;
		FRayTracingMeshBatchTaskPage* MeshBatchTaskPage = nullptr;
		uint32 NumPendingMeshBatches = 0;
		const uint32 RayTracingParallelMeshBatchSize = GRayTracingParallelMeshBatchSize;

		auto KickRayTracingMeshBatchTask = [&Allocator = Allocator, &View, &MeshBatchTaskHead, &MeshBatchTaskPage, &NumPendingMeshBatches, Scene = this->Scene]()
		{
			if (MeshBatchTaskHead)
			{
				FDynamicRayTracingMeshCommandStorage* TaskDynamicCommandStorage = Allocator.Create<FDynamicRayTracingMeshCommandStorage>();
				View.DynamicRayTracingMeshCommandStoragePerTask.Add(TaskDynamicCommandStorage);

				FRayTracingMeshCommandOneFrameArray* TaskVisibleCommands = Allocator.Create<FRayTracingMeshCommandOneFrameArray>();
				TaskVisibleCommands->Reserve(NumPendingMeshBatches);
				View.VisibleRayTracingMeshCommandsPerTask.Add(TaskVisibleCommands);

				View.AddRayTracingMeshBatchTaskList.Add(FFunctionGraphTask::CreateAndDispatchWhenReady(
					[TaskDataHead = MeshBatchTaskHead, &View, Scene, TaskDynamicCommandStorage, TaskVisibleCommands]()
				{
					FTaskTagScope TaskTagScope(ETaskTag::EParallelRenderingThread);
					TRACE_CPUPROFILER_EVENT_SCOPE(RayTracingMeshBatchTask);
					FRayTracingMeshBatchTaskPage* Page = TaskDataHead;
					const int32 ExpectedMaxVisibieCommands = TaskVisibleCommands->Max();
					while (Page)
					{
						for (uint32 ItemIndex = 0; ItemIndex < Page->NumWorkItems; ++ItemIndex)
						{
							const FRayTracingMeshBatchWorkItem& WorkItem = Page->WorkItems[ItemIndex];
							TArrayView<const FMeshBatch> MeshBatches = WorkItem.GetMeshBatches();
							for (int32 SegmentIndex = 0; SegmentIndex < MeshBatches.Num(); SegmentIndex++)
							{
								const FMeshBatch& MeshBatch = MeshBatches[SegmentIndex];
								FDynamicRayTracingMeshCommandContext CommandContext(
									*TaskDynamicCommandStorage, *TaskVisibleCommands,
									SegmentIndex, WorkItem.InstanceIndex, WorkItem.DecalInstanceIndex);
								FMeshPassProcessorRenderState PassDrawRenderState;
								FRayTracingMeshProcessor RayTracingMeshProcessor(&CommandContext, Scene, &View, PassDrawRenderState, Scene->CachedRayTracingMeshCommandsMode);
								RayTracingMeshProcessor.AddMeshBatch(MeshBatch, 1, WorkItem.SceneProxy);
							}
						}
						FRayTracingMeshBatchTaskPage* NextPage = Page->Next;
						Page = NextPage;
					}
					check(ExpectedMaxVisibieCommands <= TaskVisibleCommands->Max());
				}, TStatId(), nullptr, ENamedThreads::AnyThread));
			}

			MeshBatchTaskHead = nullptr;
			MeshBatchTaskPage = nullptr;
			NumPendingMeshBatches = 0;
		};

		// Local temporary array of instances used for GetDynamicRayTracingInstances()
		TArray<FRayTracingInstance> TempRayTracingInstances;

		for (const FRayTracingRelevantPrimitive& RelevantPrimitive : RelevantPrimitiveList.DynamicPrimitives)
		{
			const int32 PrimitiveIndex = RelevantPrimitive.PrimitiveIndex;
			FPrimitiveSceneInfo* SceneInfo = Scene->Primitives[PrimitiveIndex];

			FPrimitiveSceneProxy* SceneProxy = Scene->PrimitiveSceneProxies[PrimitiveIndex];
			TempRayTracingInstances.Reset();
			MaterialGatheringContext.DynamicRayTracingGeometriesToUpdate.Reset();

			SceneProxy->GetDynamicRayTracingInstances(MaterialGatheringContext, TempRayTracingInstances);

			for (const FRayTracingDynamicGeometryUpdateParams& DynamicRayTracingGeometryUpdate : MaterialGatheringContext.DynamicRayTracingGeometriesToUpdate)
			{
				Scene->GetRayTracingDynamicGeometryCollection()->AddDynamicMeshBatchForGeometryUpdate(
					Scene,
					&View,
					SceneProxy,
					DynamicRayTracingGeometryUpdate,
					PrimitiveIndex
				);
			}

			if (TempRayTracingInstances.Num() > 0)
			{
				for (FRayTracingInstance& Instance : TempRayTracingInstances)
				{
					const FRayTracingGeometry* Geometry = Instance.Geometry;

					if (!ensureMsgf(Geometry->DynamicGeometrySharedBufferGenerationID == FRayTracingGeometry::NonSharedVertexBuffers
						|| Geometry->DynamicGeometrySharedBufferGenerationID == SharedBufferGenerationID,
						TEXT("GenerationID %lld, but expected to be %lld or %lld. Geometry debug name: '%s'. ")
						TEXT("When shared vertex buffers are used, the contents is expected to be written every frame. ")
						TEXT("Possibly AddDynamicMeshBatchForGeometryUpdate() was not called for this geometry."),
						Geometry->DynamicGeometrySharedBufferGenerationID, SharedBufferGenerationID, FRayTracingGeometry::NonSharedVertexBuffers,
						*Geometry->Initializer.DebugName.ToString()))
					{
						continue;
					}

					// If geometry still has pending build request then add to list which requires a force build
					if (Geometry->HasPendingBuildRequest())
					{
						RayTracingScene.GeometriesToBuild.Add(Geometry);
					}

					// Validate the material/segment counts
					if (!ensureMsgf(Instance.GetMaterials().Num() == Geometry->Initializer.Segments.Num() ||
						(Geometry->Initializer.Segments.Num() == 0 && Instance.GetMaterials().Num() == 1),
						TEXT("Ray tracing material assignment validation failed for geometry '%s'. "
							"Instance.GetMaterials().Num() = %d, Geometry->Initializer.Segments.Num() = %d, Instance.Mask = 0x%X."),
						*Geometry->Initializer.DebugName.ToString(), Instance.GetMaterials().Num(),
						Geometry->Initializer.Segments.Num(), Instance.MaskAndFlags.Mask))
					{
						continue;
					}

					// Autobuild of InstanceMaskAndFlags if the mask and flags are not built
					UpdateRayTracingInstanceMaskAndFlagsIfNeeded(Instance, *SceneProxy, &ViewFamily);

					// if primitive has mixed decal and non-decal segments we need to have two ray tracing instances
					// one containing non-decal segments and the other with decal segments
					// masking of segments is done using "hidden" hitgroups
					// TODO: Debug Visualization to highlight primitives using this?
					const bool bNeedSeparateDecalInstance = Instance.MaskAndFlags.bAnySegmentsDecal && !Instance.MaskAndFlags.bAllSegmentsDecal;

					if (GRayTracingExcludeDecals && Instance.MaskAndFlags.bAnySegmentsDecal && !bNeedSeparateDecalInstance)
					{
						continue;
					}

					FRayTracingGeometryInstance RayTracingInstance;
					RayTracingInstance.GeometryRHI = Geometry->RayTracingGeometryRHI;
					checkf(RayTracingInstance.GeometryRHI, TEXT("Ray tracing instance must have a valid geometry."));
					RayTracingInstance.DefaultUserData = PrimitiveIndex;
					RayTracingInstance.bApplyLocalBoundsTransform = Instance.bApplyLocalBoundsTransform;
					RayTracingInstance.LayerIndex = (uint8)(Instance.MaskAndFlags.bAnySegmentsDecal && !bNeedSeparateDecalInstance ? ERayTracingSceneLayer::Decals : ERayTracingSceneLayer::Base);
					RayTracingInstance.Mask = Instance.MaskAndFlags.Mask;

					if (Instance.MaskAndFlags.bForceOpaque)
					{
						RayTracingInstance.Flags |= ERayTracingInstanceFlags::ForceOpaque;
					}
					if (Instance.MaskAndFlags.bDoubleSided)
					{
						RayTracingInstance.Flags |= ERayTracingInstanceFlags::TriangleCullDisable;
					}
					AddDebugRayTracingInstanceFlags(RayTracingInstance.Flags);

					if (Instance.InstanceGPUTransformsSRV.IsValid())
					{
						RayTracingInstance.NumTransforms = Instance.NumTransforms;
						RayTracingInstance.GPUTransformsSRV = Instance.InstanceGPUTransformsSRV;
					}
					else 
					{
						if (Instance.OwnsTransforms())
						{
							// Slow path: copy transforms to the owned storage
							checkf(Instance.InstanceTransformsView.Num() == 0, TEXT("InstanceTransformsView is expected to be empty if using InstanceTransforms"));
							TArrayView<FMatrix> SceneOwnedTransforms = RayTracingScene.Allocate<FMatrix>(Instance.InstanceTransforms.Num());
							FMemory::Memcpy(SceneOwnedTransforms.GetData(), Instance.InstanceTransforms.GetData(), Instance.InstanceTransforms.Num() * sizeof(RayTracingInstance.Transforms[0]));
							static_assert(std::is_same_v<decltype(SceneOwnedTransforms[0]), decltype(Instance.InstanceTransforms[0])>, "Unexpected transform type");

							RayTracingInstance.NumTransforms = SceneOwnedTransforms.Num();
							RayTracingInstance.Transforms = SceneOwnedTransforms;
						}
						else
						{
							// Fast path: just reference persistently-allocated transforms and avoid a copy
							checkf(Instance.InstanceTransforms.Num() == 0, TEXT("InstanceTransforms is expected to be empty if using InstanceTransformsView"));
							RayTracingInstance.NumTransforms = Instance.InstanceTransformsView.Num();
							RayTracingInstance.Transforms = Instance.InstanceTransformsView;
						}
					}

					const uint32 InstanceIndex = RayTracingScene.AddInstance(RayTracingInstance, SceneProxy, true);

					uint32 DecalInstanceIndex = INDEX_NONE;
					if (bNeedSeparateDecalInstance && !GRayTracingExcludeDecals)
					{
						FRayTracingGeometryInstance DecalRayTracingInstance = RayTracingInstance;
						DecalRayTracingInstance.LayerIndex = (uint8)ERayTracingSceneLayer::Decals;

						DecalInstanceIndex = RayTracingScene.AddInstance(MoveTemp(DecalRayTracingInstance), SceneProxy, true);
					}

					if (bParallelMeshBatchSetup)
					{
						if (NumPendingMeshBatches >= RayTracingParallelMeshBatchSize)
						{
							KickRayTracingMeshBatchTask();
						}

						if (MeshBatchTaskPage == nullptr || MeshBatchTaskPage->NumWorkItems == MaxWorkItemsPerPage)
						{
							FRayTracingMeshBatchTaskPage* NextPage = Allocator.Create<FRayTracingMeshBatchTaskPage>();
							if (MeshBatchTaskHead == nullptr)
							{
								MeshBatchTaskHead = NextPage;
							}
							if (MeshBatchTaskPage)
							{
								MeshBatchTaskPage->Next = NextPage;
							}
							MeshBatchTaskPage = NextPage;
						}

						FRayTracingMeshBatchWorkItem& WorkItem = MeshBatchTaskPage->WorkItems[MeshBatchTaskPage->NumWorkItems];
						MeshBatchTaskPage->NumWorkItems++;

						NumPendingMeshBatches += Instance.GetMaterials().Num();

						if (Instance.OwnsMaterials())
						{
							Swap(WorkItem.MeshBatchesOwned, Instance.Materials);
						}
						else
						{
							WorkItem.MeshBatchesView = Instance.MaterialsView;
						}

						WorkItem.SceneProxy = SceneProxy;
						WorkItem.InstanceIndex = InstanceIndex;
						WorkItem.DecalInstanceIndex = DecalInstanceIndex;
					}
					else
					{
						TArrayView<const FMeshBatch> InstanceMaterials = Instance.GetMaterials();
						for (int32 SegmentIndex = 0; SegmentIndex < InstanceMaterials.Num(); SegmentIndex++)
						{
							const FMeshBatch& MeshBatch = InstanceMaterials[SegmentIndex];
							FDynamicRayTracingMeshCommandContext CommandContext(View.DynamicRayTracingMeshCommandStorage, View.VisibleRayTracingMeshCommands, SegmentIndex, InstanceIndex, DecalInstanceIndex);
							FMeshPassProcessorRenderState PassDrawRenderState;
							FRayTracingMeshProcessor RayTracingMeshProcessor(&CommandContext, Scene, &View, PassDrawRenderState, Scene->CachedRayTracingMeshCommandsMode);
							RayTracingMeshProcessor.AddMeshBatch(MeshBatch, 1, SceneProxy);
						}
					}
				}

				if (CVarRayTracingDynamicGeometryLastRenderTimeUpdateDistance.GetValueOnRenderThread() > 0.0f)
				{
					if (FVector::Distance(SceneProxy->GetActorPosition(), View.ViewMatrices.GetViewOrigin()) < CVarRayTracingDynamicGeometryLastRenderTimeUpdateDistance.GetValueOnRenderThread())
					{
						// Update LastRenderTime for components so that visibility based ticking (like skeletal meshes) can get updated
						// We are only doing this for dynamic geometries now
						SceneInfo->LastRenderTime = CurrentWorldTime;
						SceneInfo->UpdateComponentLastRenderTime(CurrentWorldTime, /*bUpdateLastRenderTimeOnScreen=*/true);
						SceneInfo->ConditionalUpdateUniformBuffer(GraphBuilder.RHICmdList);
					}
				}
			}
		}

		KickRayTracingMeshBatchTask();
	}
	
	// Task to iterate over static ray tracing instances, perform auto-instancing and culling.
	// This adds final instances to the ray tracing scene and must be done before FRayTracingScene::BuildInitializationData().
	struct FRayTracingSceneAddInstancesTask
	{
		UE_NONCOPYABLE(FRayTracingSceneAddInstancesTask)

		static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
		TStatId                       GetStatId() const    { return TStatId(); }
		ENamedThreads::Type           GetDesiredThread()   { return ENamedThreads::AnyThread; }

		// Inputs

		const FScene& Scene;
		TChunkedArray<FRayTracingRelevantPrimitive>& RelevantStaticPrimitives;
		const FRayTracingCullingParameters& CullingParameters;

		// Outputs

		FRayTracingScene& RayTracingScene; // New instances are added into FRayTracingScene::Instances and FRayTracingScene::Allocator is used for temporary data
		TArray<FVisibleRayTracingMeshCommand>& VisibleRayTracingMeshCommands; // New elements are added here by this task
		TArray<FPrimitiveSceneProxy*>& ProxiesWithDirtyCachedInstance;

		FRayTracingSceneAddInstancesTask(const FScene& InScene,
											TChunkedArray<FRayTracingRelevantPrimitive>& InRelevantStaticPrimitives,
											const FRayTracingCullingParameters& InCullingParameters,
											FRayTracingScene& InRayTracingScene, TArray<FVisibleRayTracingMeshCommand>& InVisibleRayTracingMeshCommands,
											TArray<FPrimitiveSceneProxy*>& InProxiesWithDirtyCachedInstance)
			: Scene(InScene)
			, RelevantStaticPrimitives(InRelevantStaticPrimitives)
			, CullingParameters(InCullingParameters)
			, RayTracingScene(InRayTracingScene)
			, VisibleRayTracingMeshCommands(InVisibleRayTracingMeshCommands)
			, ProxiesWithDirtyCachedInstance(InProxiesWithDirtyCachedInstance)
		{
			VisibleRayTracingMeshCommands.Reserve(RelevantStaticPrimitives.Num());
		}

		// TODO: Consider moving auto instance batching logic into FRayTracingScene

		struct FAutoInstanceBatch
		{
			int32 Index = INDEX_NONE;
			int32 DecalIndex = INDEX_NONE;

			// Copies the next InstanceSceneDataOffset and user data into the current batch, returns true if arrays were re-allocated.
			bool Add(FRayTracingScene& InRayTracingScene, uint32 InInstanceSceneDataOffset, uint32 InUserData)
			{
				// Adhoc TArray-like resize behavior, in lieu of support for using a custom FMemStackBase in TArray.
				// Idea for future: if batch becomes large enough, we could actually split it into multiple instances to avoid memory waste.

				const bool bNeedReallocation = Cursor == InstanceSceneDataOffsets.Num();

				if (bNeedReallocation)
				{
					int32 PrevCount = InstanceSceneDataOffsets.Num();
					int32 NextCount = FMath::Max(PrevCount * 2, 1);

					TArrayView<uint32> NewInstanceSceneDataOffsets = InRayTracingScene.Allocate<uint32>(NextCount);
					if (PrevCount)
					{
						FMemory::Memcpy(NewInstanceSceneDataOffsets.GetData(), InstanceSceneDataOffsets.GetData(), InstanceSceneDataOffsets.GetTypeSize() * InstanceSceneDataOffsets.Num());
					}
					InstanceSceneDataOffsets = NewInstanceSceneDataOffsets;

					TArrayView<uint32> NewUserData = InRayTracingScene.Allocate<uint32>(NextCount);
					if (PrevCount)
					{
						FMemory::Memcpy(NewUserData.GetData(), UserData.GetData(), UserData.GetTypeSize() * UserData.Num());
					}
					UserData = NewUserData;
				}

				InstanceSceneDataOffsets[Cursor] = InInstanceSceneDataOffset;
				UserData[Cursor] = InUserData;

				++Cursor;

				return bNeedReallocation;
			}

			bool IsValid() const
			{
				return InstanceSceneDataOffsets.Num() != 0;
			}

			TArrayView<uint32> InstanceSceneDataOffsets;
			TArrayView<uint32> UserData;
			uint32 Cursor = 0;
		};

		void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
		{
			FTaskTagScope TaskTagScope(ETaskTag::EParallelRenderingThread);

			TRACE_CPUPROFILER_EVENT_SCOPE(RayTracingSceneStaticInstanceTask);

			FGraphEventArray CullingTasks;

			const bool bAutoInstance = CVarRayTracingAutoInstance.GetValueOnRenderThread() != 0;

			// Instance batches by FRayTracingRelevantPrimitive::InstancingKey()
			Experimental::TSherwoodMap<uint64, FAutoInstanceBatch> InstanceBatches;

			TArray<FRayTracingCullPrimitiveInstancesClosure> CullInstancesClosures;
			if (CullingParameters.CullingMode != RayTracing::ECullingMode::Disabled && GetRayTracingCullingPerInstance())
			{
				CullInstancesClosures.Reserve(RelevantStaticPrimitives.Num());
				CullingTasks.Reserve(RelevantStaticPrimitives.Num() / 256 + 1);
			}

			// scan relevant primitives computing hash data to look for duplicate instances
			for (const FRayTracingRelevantPrimitive& RelevantPrimitive : RelevantStaticPrimitives)
			{
				const int32 PrimitiveIndex = RelevantPrimitive.PrimitiveIndex;
				FPrimitiveSceneInfo* SceneInfo = Scene.Primitives[PrimitiveIndex];
				FPrimitiveSceneProxy* SceneProxy = Scene.PrimitiveSceneProxies[PrimitiveIndex];
				ERayTracingPrimitiveFlags Flags = Scene.PrimitiveRayTracingFlags[PrimitiveIndex];

				if (EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::CacheInstances))
				{
					// For primitives with ERayTracingPrimitiveFlags::CacheInstances flag we only cache the instance/mesh commands of the current LOD
					// (see FPrimitiveSceneInfo::UpdateCachedRayTracingInstance(...) and CacheRayTracingPrimitive(...))
					const int32 LODIndex = 0;

					const bool bUsingNaniteRayTracing = (Nanite::GetRayTracingMode() != Nanite::ERayTracingMode::Fallback) && SceneProxy->IsNaniteMesh();

					if (bUsingNaniteRayTracing)
					{
						Nanite::GRayTracingManager.AddVisiblePrimitive(SceneInfo);

						if (SceneInfo->CachedRayTracingInstance.GeometryRHI == nullptr)
						{
							// Nanite ray tracing geometry not ready yet, doesn't include primitive in ray tracing scene
							continue;
						}
					}
					else if (!SceneInfo->IsCachedRayTracingGeometryValid())
					{
						// cached instance is not valid (eg: was streamed out) need to invalidate for next frame
						ProxiesWithDirtyCachedInstance.Add(Scene.PrimitiveSceneProxies[PrimitiveIndex]);
						continue;
					}
					
					// TODO: Consider requesting a recache of all ray tracing commands during which decals are excluded
					
					// if primitive has mixed decal and non-decal segments we need to have two ray tracing instances
					// one containing non-decal segments and the other with decal segments
					// masking of segments is done using "hidden" hitgroups
					// TODO: Debug Visualization to highlight primitives using this?
					const bool bNeedSeparateDecalInstance = SceneInfo->bCachedRayTracingInstanceAnySegmentsDecal && !SceneInfo->bCachedRayTracingInstanceAllSegmentsDecal;

					if (GRayTracingExcludeDecals && SceneInfo->bCachedRayTracingInstanceAnySegmentsDecal && !bNeedSeparateDecalInstance)
					{
						continue;
					}

					checkf(SceneInfo->CachedRayTracingInstance.GeometryRHI, TEXT("Ray tracing instance must have a valid geometry."));

					FRayTracingGeometryInstance NewInstance = SceneInfo->CachedRayTracingInstance;
					NewInstance.LayerIndex = (uint8)(SceneInfo->bCachedRayTracingInstanceAnySegmentsDecal && !bNeedSeparateDecalInstance ? ERayTracingSceneLayer::Decals : ERayTracingSceneLayer::Base);

					const Experimental::FHashElementId GroupId = Scene.PrimitiveRayTracingGroupIds[PrimitiveIndex];
					const bool bUseGroupBounds = CullingParameters.bCullUsingGroupIds && GroupId.IsValid();

					if (CullingParameters.CullingMode != RayTracing::ECullingMode::Disabled && GetRayTracingCullingPerInstance() && SceneInfo->CachedRayTracingInstance.NumTransforms > 1 && !bUseGroupBounds)
					{
						const bool bIsFarFieldPrimitive = EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::FarField);

						TArrayView<uint32> InstanceActivationMask = RayTracingScene.Allocate<uint32>(FMath::DivideAndRoundUp(NewInstance.NumTransforms, 32u));

						NewInstance.ActivationMask = InstanceActivationMask;

						FRayTracingCullPrimitiveInstancesClosure Closure;
						Closure.Scene = &Scene;
						Closure.SceneInfo = SceneInfo;
						Closure.PrimitiveIndex = PrimitiveIndex;
						Closure.bIsFarFieldPrimitive = bIsFarFieldPrimitive;
						Closure.CullingParameters = &CullingParameters;
						Closure.OutInstanceActivationMask = InstanceActivationMask;

						CullInstancesClosures.Add(MoveTemp(Closure));

						if (CullInstancesClosures.Num() >= 256)
						{
							CullingTasks.Add(FFunctionGraphTask::CreateAndDispatchWhenReady([CullInstancesClosures = MoveTemp(CullInstancesClosures)]()
							{
								for (auto& Closure : CullInstancesClosures)
								{
									Closure();
								}
							}, TStatId(), nullptr, ENamedThreads::AnyThread));
						}
					}

					AddDebugRayTracingInstanceFlags(NewInstance.Flags);

					const int32 NewInstanceIndex = RayTracingScene.AddInstance(NewInstance, SceneInfo->Proxy, false);

					uint32 DecalInstanceIndex = INDEX_NONE;
					if (bNeedSeparateDecalInstance && !GRayTracingExcludeDecals)
					{
						FRayTracingGeometryInstance DecalRayTracingInstance = NewInstance;
						DecalRayTracingInstance.LayerIndex = (uint8)ERayTracingSceneLayer::Decals;

						DecalInstanceIndex = RayTracingScene.AddInstance(MoveTemp(DecalRayTracingInstance), SceneInfo->Proxy, false);
					}

					// At the moment we only support SM & ISMs on this path
					check(EnumHasAnyFlags(Flags, ERayTracingPrimitiveFlags::CacheMeshCommands));
					if (SceneInfo->CachedRayTracingMeshCommandIndicesPerLOD.Num() > 0 && SceneInfo->CachedRayTracingMeshCommandIndicesPerLOD[LODIndex].Num() > 0)
					{
						const bool bHasDecalInstanceIndex = DecalInstanceIndex != INDEX_NONE;

						for (int32 CommandIndex : SceneInfo->CachedRayTracingMeshCommandIndicesPerLOD[LODIndex])
						{
							const FRayTracingMeshCommand& MeshCommand = Scene.CachedRayTracingMeshCommands[CommandIndex];

							{
								const bool bHidden = bHasDecalInstanceIndex && MeshCommand.bDecal;
								FVisibleRayTracingMeshCommand NewVisibleMeshCommand(&MeshCommand, NewInstanceIndex, bHidden);
								VisibleRayTracingMeshCommands.Add(NewVisibleMeshCommand);
							}

							if(bHasDecalInstanceIndex)
							{
								const bool bHidden = !MeshCommand.bDecal;
								FVisibleRayTracingMeshCommand NewVisibleMeshCommand(&MeshCommand, DecalInstanceIndex, bHidden);
								VisibleRayTracingMeshCommands.Add(NewVisibleMeshCommand);
							}
						}
					}
				}
				else
				{
					const int8 LODIndex = RelevantPrimitive.LODIndex;

					if (LODIndex < 0 || !RelevantPrimitive.bStatic)
					{
						continue; // skip dynamic primitives and other 
					}

					// if primitive has mixed decal and non-decal segments we need to have two ray tracing instances
					// one containing non-decal segments and the other with decal segments
					// masking of segments is done using "hidden" hitgroups
					// TODO: Debug Visualization to highlight primitives using this?
					const bool bNeedSeparateDecalInstance = RelevantPrimitive.bAnySegmentsDecal && !RelevantPrimitive.bAllSegmentsDecal;

					if (GRayTracingExcludeDecals && RelevantPrimitive.bAnySegmentsDecal && !bNeedSeparateDecalInstance)
					{
						continue;
					}

					if ((GRayTracingExcludeDecals && RelevantPrimitive.bAnySegmentsDecal)
						|| (GRayTracingExcludeTranslucent && RelevantPrimitive.bAllSegmentsTranslucent)
						|| (GRayTracingExcludeSky && RelevantPrimitive.bIsSky))
					{
						continue;
					}

					// location if this is a new entry
					const uint64 InstanceKey = RelevantPrimitive.InstancingKey();

					FAutoInstanceBatch DummyInstanceBatch = { };
					FAutoInstanceBatch& InstanceBatch = bAutoInstance ? InstanceBatches.FindOrAdd(InstanceKey, DummyInstanceBatch) : DummyInstanceBatch;

					if (InstanceBatch.IsValid())
					{
						// Reusing a previous entry, just append to the instance list.

						bool bReallocated = InstanceBatch.Add(RayTracingScene, SceneInfo->GetInstanceSceneDataOffset(), (uint32)PrimitiveIndex);

						check(InstanceBatch.Index != INDEX_NONE);
						{
							FRayTracingGeometryInstance& RayTracingInstance = RayTracingScene.GetInstance(InstanceBatch.Index);
							++RayTracingInstance.NumTransforms;
							check(RayTracingInstance.NumTransforms == InstanceBatch.Cursor); // sanity check

							if (bReallocated)
							{
								RayTracingInstance.InstanceSceneDataOffsets = InstanceBatch.InstanceSceneDataOffsets;
								RayTracingInstance.UserData = InstanceBatch.UserData;
							}
						}

						if(InstanceBatch.DecalIndex != INDEX_NONE)
						{
							FRayTracingGeometryInstance& RayTracingInstance = RayTracingScene.GetInstance(InstanceBatch.DecalIndex);
							++RayTracingInstance.NumTransforms;
							check(RayTracingInstance.NumTransforms == InstanceBatch.Cursor); // sanity check

							if (bReallocated)
							{
								RayTracingInstance.InstanceSceneDataOffsets = InstanceBatch.InstanceSceneDataOffsets;
								RayTracingInstance.UserData = InstanceBatch.UserData;
							}
						}
					}
					else
					{
						// Starting new instance batch

						InstanceBatch.Add(RayTracingScene, SceneInfo->GetInstanceSceneDataOffset(), (uint32)PrimitiveIndex);

						FRayTracingGeometryInstance RayTracingInstance;
						RayTracingInstance.GeometryRHI = RelevantPrimitive.RayTracingGeometryRHI;
						checkf(RayTracingInstance.GeometryRHI, TEXT("Ray tracing instance must have a valid geometry."));
						RayTracingInstance.InstanceSceneDataOffsets = InstanceBatch.InstanceSceneDataOffsets;
						RayTracingInstance.UserData = InstanceBatch.UserData;
						RayTracingInstance.NumTransforms = 1;

						RayTracingInstance.Mask = RelevantPrimitive.InstanceMask; // When no cached command is found, InstanceMask == 0 and the instance is effectively filtered out

						if (RelevantPrimitive.bAllSegmentsOpaque && RelevantPrimitive.bAllSegmentsCastShadow)
						{
							RayTracingInstance.Flags |= ERayTracingInstanceFlags::ForceOpaque;
						}
						if (RelevantPrimitive.bTwoSided)
						{
							RayTracingInstance.Flags |= ERayTracingInstanceFlags::TriangleCullDisable;
						}
						AddDebugRayTracingInstanceFlags(RayTracingInstance.Flags);

						RayTracingInstance.LayerIndex = (uint8)(RelevantPrimitive.bAnySegmentsDecal && !bNeedSeparateDecalInstance ? ERayTracingSceneLayer::Decals : ERayTracingSceneLayer::Base);

						InstanceBatch.Index = RayTracingScene.AddInstance(RayTracingInstance, SceneInfo->Proxy, false);

						if (bNeedSeparateDecalInstance && !GRayTracingExcludeDecals)
						{
							FRayTracingGeometryInstance DecalRayTracingInstance = RayTracingInstance;
							DecalRayTracingInstance.LayerIndex = (uint8)ERayTracingSceneLayer::Decals;

							InstanceBatch.DecalIndex = RayTracingScene.AddInstance(MoveTemp(DecalRayTracingInstance), SceneInfo->Proxy, false);
						}

						const bool bHasDecalInstanceIndex = InstanceBatch.DecalIndex != INDEX_NONE;

						for (int32 CommandIndex : RelevantPrimitive.CachedRayTracingMeshCommandIndices)
						{
							if (CommandIndex >= 0)
							{
								const FRayTracingMeshCommand& MeshCommand = Scene.CachedRayTracingMeshCommands[CommandIndex];

								{
									const bool bHidden = bHasDecalInstanceIndex && MeshCommand.bDecal;
									FVisibleRayTracingMeshCommand NewVisibleMeshCommand(&MeshCommand, InstanceBatch.Index, bHidden);
									VisibleRayTracingMeshCommands.Add(NewVisibleMeshCommand);
								}

								if (bHasDecalInstanceIndex)
								{
									const bool bHidden = !MeshCommand.bDecal;
									FVisibleRayTracingMeshCommand NewVisibleMeshCommand(&MeshCommand, InstanceBatch.DecalIndex, bHidden);
									VisibleRayTracingMeshCommands.Add(NewVisibleMeshCommand);
								}
							}
							else
							{
								// CommandIndex == -1 indicates that the mesh batch has been filtered by FRayTracingMeshProcessor (like the shadow depth pass batch)
								// Do nothing in this case
							}
						}
					}
				}
			}

			CullingTasks.Add(FFunctionGraphTask::CreateAndDispatchWhenReady([CullInstancesClosures = MoveTemp(CullInstancesClosures)]()
			{
				for (auto& Closure : CullInstancesClosures)
				{
					Closure();
				}
			}, TStatId(), nullptr, ENamedThreads::AnyThread));

			for (FGraphEventRef& CullingTask : CullingTasks)
			{
				MyCompletionGraphEvent->DontCompleteUntil(CullingTask);
			}
		}
	};

	FGraphEventArray AddInstancesTaskPrerequisites;
	AddInstancesTaskPrerequisites.Add(RelevantPrimitiveList.StaticPrimitiveLODTask);

	FGraphEventRef AddInstancesTask = TGraphTask<FRayTracingSceneAddInstancesTask>::CreateTask(&AddInstancesTaskPrerequisites).ConstructAndDispatchWhenReady(
		*Scene, RelevantPrimitiveList.StaticPrimitives, View.RayTracingCullingParameters, // inputs 
		RayTracingScene, View.VisibleRayTracingMeshCommands, View.ProxiesWithDirtyCachedInstance // outputs
	);

	// Scene init task can run only when all pre-init tasks are complete (including culling tasks that are spawned while adding instances)
	View.RayTracingSceneInitTask = FFunctionGraphTask::CreateAndDispatchWhenReady(
		[&View, &RayTracingScene]()
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(RayTracingSceneInitTask);
			View.RayTracingSceneInitData = RayTracingScene.BuildInitializationData();
		},
		TStatId(), AddInstancesTask, ENamedThreads::AnyThread);

	return true;
}

static void DeduplicateRayGenerationShaders(TArray< FRHIRayTracingShader*>& RayGenShaders)
{
	TSet<FRHIRayTracingShader*> UniqueRayGenShaders;
	for (FRHIRayTracingShader* Shader : RayGenShaders)
	{
		UniqueRayGenShaders.Add(Shader);
	}
	RayGenShaders = UniqueRayGenShaders.Array();
}

BEGIN_SHADER_PARAMETER_STRUCT(FBuildAccelerationStructurePassParams, )
	RDG_BUFFER_ACCESS(RayTracingSceneScratchBuffer, ERHIAccess::UAVCompute)
	RDG_BUFFER_ACCESS(DynamicGeometryScratchBuffer, ERHIAccess::UAVCompute)
	RDG_BUFFER_ACCESS(RayTracingSceneInstanceBuffer, ERHIAccess::SRVCompute)
	RDG_BUFFER_ACCESS(LumenHitDataBuffer, ERHIAccess::CopyDest)

	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FRaytracingLightDataPacked, LightDataPacked)

	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ClusterPageData)
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, HierarchyBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint2>, RayTracingDataBuffer)
END_SHADER_PARAMETER_STRUCT()

bool FDeferredShadingSceneRenderer::SetupRayTracingPipelineStates(FRDGBuilder& GraphBuilder)
{
	if (!IsRayTracingEnabled() || Views.Num() == 0)
	{
		return false;
	}

	if (!bAnyRayTracingPassEnabled)
	{
		return false;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FDeferredShadingSceneRenderer::SetupRayTracingPipelineStates);

	const int32 ReferenceViewIndex = 0;
	FViewInfo& ReferenceView = Views[ReferenceViewIndex];

	if (ReferenceView.AddRayTracingMeshBatchTaskList.Num() > 0)
	{
		SCOPE_CYCLE_COUNTER(STAT_WaitRayTracingAddMesh);

		FTaskGraphInterface::Get().WaitUntilTasksComplete(ReferenceView.AddRayTracingMeshBatchTaskList, ENamedThreads::GetRenderThread_Local());

		for (int32 TaskIndex = 0; TaskIndex < ReferenceView.AddRayTracingMeshBatchTaskList.Num(); TaskIndex++)
		{
			ReferenceView.VisibleRayTracingMeshCommands.Append(*ReferenceView.VisibleRayTracingMeshCommandsPerTask[TaskIndex]);
		}

		ReferenceView.AddRayTracingMeshBatchTaskList.Empty();
	}

	const bool bIsPathTracing = ViewFamily.EngineShowFlags.PathTracing;

	if (GRHISupportsRayTracingShaders)
	{
		// #dxr_todo: UE-72565: refactor ray tracing effects to not be member functions of DeferredShadingRenderer. 
		// Should register each effect at startup and just loop over them automatically to gather all required shaders.

		TArray<FRHIRayTracingShader*> RayGenShaders;

		// We typically see ~120 raygen shaders, but allow some headroom to avoid reallocation if our estimate is wrong.
		RayGenShaders.Reserve(256);

		if (bIsPathTracing)
		{
			// This view only needs the path tracing raygen shaders as all other
			// passes should be disabled.
			PreparePathTracing(ViewFamily, RayGenShaders);
		}
		else
		{
			// Path tracing is disabled, get all other possible raygen shaders
			PrepareRayTracingDebug(ViewFamily, RayGenShaders);

			// These other cases do potentially depend on the camera position since they are
			// driven by FinalPostProcessSettings, which is why we need to merge them across views
			if (!IsForwardShadingEnabled(ShaderPlatform))
			{
				for (const FViewInfo& View : Views)
				{
					PrepareRayTracingReflections(View, *Scene, RayGenShaders);
					PrepareSingleLayerWaterRayTracingReflections(View, *Scene, RayGenShaders);
					PrepareRayTracingShadows(View, *Scene, RayGenShaders);
					PrepareRayTracingAmbientOcclusion(View, RayGenShaders);
					PrepareRayTracingSkyLight(View, *Scene, RayGenShaders);
					PrepareRayTracingGlobalIllumination(View, RayGenShaders);
					PrepareRayTracingGlobalIlluminationPlugin(View, RayGenShaders);
					PrepareRayTracingTranslucency(View, RayGenShaders);

					if (DoesPlatformSupportLumenGI(ShaderPlatform) && Lumen::UseHardwareRayTracing(ViewFamily))
					{
						PrepareLumenHardwareRayTracingScreenProbeGather(View, RayGenShaders);
						PrepareLumenHardwareRayTracingShortRangeAO(View, RayGenShaders);
						PrepareLumenHardwareRayTracingRadianceCache(View, RayGenShaders);
						PrepareLumenHardwareRayTracingReflections(View, RayGenShaders);
						PrepareLumenHardwareRayTracingVisualize(View, RayGenShaders);
					}
				}
			}
			DeduplicateRayGenerationShaders(RayGenShaders);
		}

		if (RayGenShaders.Num())
		{
			// Create RTPSO and kick off high-level material parameter binding tasks which will be consumed during RDG execution in BindRayTracingMaterialPipeline()
			ReferenceView.RayTracingMaterialPipeline = CreateRayTracingMaterialPipeline(GraphBuilder.RHICmdList, ReferenceView, RayGenShaders);
		}
	}

	// Add deferred material gather shaders
	if (GRHISupportsRayTracingShaders)
	{
		TArray<FRHIRayTracingShader*> DeferredMaterialRayGenShaders;
		if (!IsForwardShadingEnabled(ShaderPlatform))
		{
			for (const FViewInfo& View : Views)
			{
				PrepareRayTracingReflectionsDeferredMaterial(View, *Scene, DeferredMaterialRayGenShaders);
				PrepareRayTracingDeferredReflectionsDeferredMaterial(View, *Scene, DeferredMaterialRayGenShaders);
				PrepareRayTracingGlobalIlluminationDeferredMaterial(View, DeferredMaterialRayGenShaders);
				if (DoesPlatformSupportLumenGI(ShaderPlatform))
				{
					PrepareLumenHardwareRayTracingReflectionsDeferredMaterial(View, DeferredMaterialRayGenShaders);
					PrepareLumenHardwareRayTracingRadianceCacheDeferredMaterial(View, DeferredMaterialRayGenShaders);
					PrepareLumenHardwareRayTracingScreenProbeGatherDeferredMaterial(View, DeferredMaterialRayGenShaders);
					PrepareLumenHardwareRayTracingVisualizeDeferredMaterial(View, DeferredMaterialRayGenShaders);
				}
			}
		}

		DeduplicateRayGenerationShaders(DeferredMaterialRayGenShaders);

		if (DeferredMaterialRayGenShaders.Num())
		{
			ReferenceView.RayTracingMaterialGatherPipeline = CreateRayTracingDeferredMaterialGatherPipeline(GraphBuilder.RHICmdList, ReferenceView, DeferredMaterialRayGenShaders);
		}
	}

	// Add Lumen hardware ray tracing materials
	if (GRHISupportsRayTracingShaders)
	{
		TArray<FRHIRayTracingShader*> LumenHardwareRayTracingRayGenShaders;
		if (DoesPlatformSupportLumenGI(ShaderPlatform))
		{
			for (const FViewInfo& View : Views)
			{
				PrepareLumenHardwareRayTracingVisualizeLumenMaterial(View, LumenHardwareRayTracingRayGenShaders);
				PrepareLumenHardwareRayTracingRadianceCacheLumenMaterial(View, LumenHardwareRayTracingRayGenShaders);
				PrepareLumenHardwareRayTracingTranslucencyVolumeLumenMaterial(View, LumenHardwareRayTracingRayGenShaders);
				PrepareLumenHardwareRayTracingRadiosityLumenMaterial(View, LumenHardwareRayTracingRayGenShaders);
				PrepareLumenHardwareRayTracingReflectionsLumenMaterial(View, LumenHardwareRayTracingRayGenShaders);
				PrepareLumenHardwareRayTracingScreenProbeGatherLumenMaterial(View, LumenHardwareRayTracingRayGenShaders);
				PrepareLumenHardwareRayTracingDirectLightingLumenMaterial(View, LumenHardwareRayTracingRayGenShaders);
			}
		}
		DeduplicateRayGenerationShaders(LumenHardwareRayTracingRayGenShaders);

		if (LumenHardwareRayTracingRayGenShaders.Num())
		{
			ReferenceView.LumenHardwareRayTracingMaterialPipeline = CreateLumenHardwareRayTracingMaterialPipeline(GraphBuilder.RHICmdList, ReferenceView, LumenHardwareRayTracingRayGenShaders);
		}
	}

	// Initialize common resources used for lighting in ray tracing effects

	ReferenceView.RayTracingSubSurfaceProfileTexture = GetSubsurfaceProfileTextureWithFallback();

	ReferenceView.RayTracingSubSurfaceProfileSRV = RHICreateShaderResourceView(ReferenceView.RayTracingSubSurfaceProfileTexture, 0);

	for (int32 ViewIndex = 0; ViewIndex < AllFamilyViews.Num(); ++ViewIndex)
	{
		// TODO:  It would make more sense for common ray tracing resources to be in a shared structure, rather than copied into each FViewInfo.
		//        A goal is to have the FViewInfo structure only be visible to the scene renderer that owns it, to avoid dependencies being created
		//        that could lead to maintenance issues or interfere with paralellism goals.  For now, this works though...
		FViewInfo* View = const_cast<FViewInfo*>(static_cast<const FViewInfo*>(AllFamilyViews[ViewIndex]));

		// Send common ray tracing resources from reference view to all others.
		if (View->bHasAnyRayTracingPass && View != &ReferenceView)
		{
			View->RayTracingSubSurfaceProfileTexture = ReferenceView.RayTracingSubSurfaceProfileTexture;
			View->RayTracingSubSurfaceProfileSRV = ReferenceView.RayTracingSubSurfaceProfileSRV;
			View->RayTracingMaterialPipeline = ReferenceView.RayTracingMaterialPipeline;
		}
	}

	return true;
}

void FDeferredShadingSceneRenderer::SetupRayTracingLightDataForViews(FRDGBuilder& GraphBuilder)
{
	if (!bAnyRayTracingPassEnabled)
	{
		return;
	}

	const bool bIsPathTracing = ViewFamily.EngineShowFlags.PathTracing;

	uint32 NumOfSkippedRayTracingLights = 0;

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		FViewInfo& View = Views[ViewIndex];

		if (bIsPathTracing)
		{
			// Path Tracing currently uses its own code to manage lights, so doesn't need to run this.
			// TODO: merge the lighting representations between ray traced and path traced cases?
		}
		else
		{
			// This light data is a function of the camera position, so must be computed per view.
			View.RayTracingLightDataUniformBuffer = CreateRayTracingLightData(GraphBuilder, Scene, View, View.ShaderMap, NumOfSkippedRayTracingLights);
		}
	}

#if !UE_BUILD_SHIPPING
	if (!bIsPathTracing && NumOfSkippedRayTracingLights > 0)
	{
		OnGetOnScreenMessages.AddLambda([NumOfSkippedRayTracingLights](FScreenMessageWriter& ScreenMessageWriter)->void
			{
				FString String = FString::Printf(
					TEXT("%d light(s) skipped. Active Ray Tracing light count > RAY_TRACING_LIGHT_COUNT_MAXIMUM (%d)."),
					NumOfSkippedRayTracingLights,
					RAY_TRACING_LIGHT_COUNT_MAXIMUM);
				ScreenMessageWriter.DrawLine(FText::FromString(String), 10, FColor::Yellow);
			});
	}
#endif
}

bool FDeferredShadingSceneRenderer::DispatchRayTracingWorldUpdates(FRDGBuilder& GraphBuilder, FRDGBufferRef& OutDynamicGeometryScratchBuffer)
{
	OutDynamicGeometryScratchBuffer = nullptr;

	// We only need to update ray tracing scene for the first view family, if multiple are rendered in a single scene render call.
	if (!bShouldUpdateRayTracingScene)
	{
		// This needs to happen even when ray tracing is not enabled
		// - importers might batch BVH creation requests that need to be resolved in any case
		GRayTracingGeometryManager.ProcessBuildRequests(GraphBuilder.RHICmdList);
		// - Nanite ray tracing instances are already pointing at the new BLASes and RayTracingDataOffsets in GPUScene have been updated
		Nanite::GRayTracingManager.ProcessBuildRequests(GraphBuilder);
		return false;
	}

	check(IsRayTracingEnabled() && bAnyRayTracingPassEnabled && !Views.IsEmpty());

	TRACE_CPUPROFILER_EVENT_SCOPE(FDeferredShadingSceneRenderer::DispatchRayTracingWorldUpdates);

	// Make sure there are no pending skin cache builds and updates anymore:
	// FSkeletalMeshObjectGPUSkin::UpdateDynamicData_RenderThread could have enqueued build operations which might not have
	// been processed by CommitRayTracingGeometryUpdates. 
	// All pending builds should be done before adding them to the top level BVH.
	if (FRayTracingSkinnedGeometryUpdateQueue* RayTracingSkinnedGeometryUpdateQueue = Scene->GetRayTracingSkinnedGeometryUpdateQueue())
	{
		RayTracingSkinnedGeometryUpdateQueue->Commit(GraphBuilder);
	}

	GRayTracingGeometryManager.ProcessBuildRequests(GraphBuilder.RHICmdList);

	const int32 ReferenceViewIndex = 0;
	FViewInfo& ReferenceView = Views[ReferenceViewIndex];
	FRayTracingScene& RayTracingScene = Scene->RayTracingScene;

	if (RayTracingScene.GeometriesToBuild.Num() > 0)
	{
		// Force update all the collected geometries (use stack allocator?)
		GRayTracingGeometryManager.ForceBuildIfPending(GraphBuilder.RHICmdList, RayTracingScene.GeometriesToBuild);
	}

	FTaskGraphInterface::Get().WaitUntilTaskCompletes(ReferenceView.RayTracingSceneInitTask, ENamedThreads::GetRenderThread_Local());

	ReferenceView.RayTracingSceneInitTask = {};

	for (FPrimitiveSceneProxy* SceneProxy : ReferenceView.ProxiesWithDirtyCachedInstance)
	{
		SceneProxy->GetScene().UpdateCachedRayTracingState(SceneProxy);
	}

	{
		Nanite::GRayTracingManager.ProcessUpdateRequests(GraphBuilder, Scene->GPUScene.PrimitiveBuffer->GetSRV());
		const bool bAnyBlasRebuilt = Nanite::GRayTracingManager.ProcessBuildRequests(GraphBuilder);
		if (bAnyBlasRebuilt)
		{
			for (FViewInfo& View : Views)
			{
				if (View.ViewState != nullptr && !View.bIsOfflineRender)
				{
					// don't invalidate in the offline case because we only get one attempt at rendering each sample
					View.ViewState->PathTracingInvalidate();
				}
			}
		}
	}

	// Keep mask the same as what's already set (which will be the view mask) if TLAS updates should be masked to the view
	RDG_GPU_MASK_SCOPE(GraphBuilder, GRayTracingMultiGpuTLASMask ? GraphBuilder.RHICmdList.GetGPUMask() : FRHIGPUMask::All());

	RayTracingScene.CreateWithInitializationData(GraphBuilder, &Scene->GPUScene, ReferenceView.ViewMatrices, MoveTemp(ReferenceView.RayTracingSceneInitData));

	// Transition internal resources before building
	RayTracingScene.Transition(GraphBuilder, ERayTracingSceneState::Writable);

	const uint32 BLASScratchSize = Scene->GetRayTracingDynamicGeometryCollection()->ComputeScratchBufferSize();
	if (BLASScratchSize > 0)
	{
		const uint32 ScratchAlignment = GRHIRayTracingScratchBufferAlignment;
		FRDGBufferDesc ScratchBufferDesc;
		ScratchBufferDesc.Usage = EBufferUsageFlags::RayTracingScratch | EBufferUsageFlags::StructuredBuffer;
		ScratchBufferDesc.BytesPerElement = ScratchAlignment;
		ScratchBufferDesc.NumElements = FMath::DivideAndRoundUp(BLASScratchSize, ScratchAlignment);

		OutDynamicGeometryScratchBuffer = GraphBuilder.CreateBuffer(ScratchBufferDesc, TEXT("DynamicGeometry.BLASSharedScratchBuffer"));
	}

	const bool bRayTracingAsyncBuild = CVarRayTracingAsyncBuild.GetValueOnRenderThread() != 0 && GRHISupportsRayTracingAsyncBuildAccelerationStructure;
	const ERDGPassFlags ComputePassFlags = bRayTracingAsyncBuild ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute;

	{
		RDG_GPU_STAT_SCOPE(GraphBuilder, RayTracingUpdate);

		{
			// Dynamic geometry (BLAS) updates must always run on all GPUs.  Other passes may either run on all GPUs or be scoped to the view's GPUs.
			// See GRayTracingMultiGpuTLASMask.
			RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());

			FBuildAccelerationStructurePassParams* PassParams = GraphBuilder.AllocParameters<FBuildAccelerationStructurePassParams>();
			PassParams->RayTracingSceneScratchBuffer = nullptr;
			PassParams->RayTracingSceneInstanceBuffer = nullptr;
			PassParams->View = ReferenceView.ViewUniformBuffer;
			PassParams->DynamicGeometryScratchBuffer = OutDynamicGeometryScratchBuffer;
			PassParams->LightDataPacked = nullptr;
			PassParams->ClusterPageData = nullptr;
			PassParams->HierarchyBuffer = nullptr;
			PassParams->RayTracingDataBuffer = nullptr;

			// Use ERDGPassFlags::NeverParallel so the pass never runs off the render thread and we always get the following order of execution on the CPU:
			// BuildTLASInstanceBuffer, RayTracingDynamicUpdate, RayTracingUpdate, RayTracingEndUpdate, ..., ReleaseRayTracingResources
			GraphBuilder.AddPass(RDG_EVENT_NAME("RayTracingDynamicUpdate"), PassParams, ComputePassFlags | ERDGPassFlags::NeverCull | ERDGPassFlags::NeverParallel,
				[this, PassParams, bRayTracingAsyncBuild](FRHICommandListImmediate& RHICmdList)
			{
				SCOPED_GPU_STAT(RHICmdList, RayTracingGeometry);
				FRHIBuffer* DynamicGeometryScratchBuffer = PassParams->DynamicGeometryScratchBuffer ? PassParams->DynamicGeometryScratchBuffer->GetRHI() : nullptr;
				Scene->GetRayTracingDynamicGeometryCollection()->DispatchUpdates(RHICmdList, DynamicGeometryScratchBuffer);
			});
		}

		{
			FBuildAccelerationStructurePassParams* PassParams = GraphBuilder.AllocParameters<FBuildAccelerationStructurePassParams>();
			PassParams->RayTracingSceneScratchBuffer = Scene->RayTracingScene.BuildScratchBuffer;
			PassParams->RayTracingSceneInstanceBuffer = Scene->RayTracingScene.InstanceBuffer;
			PassParams->View = ReferenceView.ViewUniformBuffer;
			PassParams->DynamicGeometryScratchBuffer = OutDynamicGeometryScratchBuffer;
			PassParams->LightDataPacked = nullptr;
			PassParams->ClusterPageData = nullptr;
			PassParams->HierarchyBuffer = nullptr;
			PassParams->RayTracingDataBuffer = nullptr;

			// Use ERDGPassFlags::NeverParallel here too -- see comment above on the previous pass
			GraphBuilder.AddPass(RDG_EVENT_NAME("RayTracingUpdate"), PassParams, ComputePassFlags | ERDGPassFlags::NeverCull | ERDGPassFlags::NeverParallel,
				[this, PassParams, bRayTracingAsyncBuild](FRHICommandListImmediate& RHICmdList)
			{
				SCOPED_GPU_STAT(RHICmdList, RayTracingScene);

				FRHIRayTracingScene* RayTracingSceneRHI = Scene->RayTracingScene.GetRHIRayTracingSceneChecked();
				FRHIBuffer* AccelerationStructureBuffer = Scene->RayTracingScene.GetBufferChecked();
				FRHIBuffer* ScratchBuffer = PassParams->RayTracingSceneScratchBuffer->GetRHI();
				FRHIBuffer* InstanceBuffer = PassParams->RayTracingSceneInstanceBuffer->GetRHI();

				FRayTracingSceneBuildParams BuildParams;
				BuildParams.Scene = RayTracingSceneRHI;
				BuildParams.ScratchBuffer = ScratchBuffer;
				BuildParams.ScratchBufferOffset = 0;
				BuildParams.InstanceBuffer = InstanceBuffer;
				BuildParams.InstanceBufferOffset = 0;

				RHICmdList.BindAccelerationStructureMemory(RayTracingSceneRHI, AccelerationStructureBuffer, 0);
				RHICmdList.BuildAccelerationStructure(BuildParams);
			});
		}
	}

	AddPass(GraphBuilder, RDG_EVENT_NAME("RayTracingEndUpdate"), [this, bRayTracingAsyncBuild](FRHICommandListImmediate& RHICmdList)
	{
		if (!bRayTracingAsyncBuild)
		{
			// Submit potentially expensive BVH build commands to the GPU as soon as possible.
			// Avoids a GPU bubble in some CPU-limited cases.
			RHICmdList.SubmitCommandsHint();
		}

		Scene->GetRayTracingDynamicGeometryCollection()->EndUpdate(RHICmdList);
	});

	return true;
}

static void ReleaseRaytracingResources(FRDGBuilder& GraphBuilder, TArrayView<FViewInfo> Views, FRayTracingScene &RayTracingScene, bool bIsLastRenderer)
{
	// Keep mask the same as what's already set (which will be the view mask) if TLAS updates should be masked to the view
	RDG_GPU_MASK_SCOPE(GraphBuilder, GRayTracingMultiGpuTLASMask ? GraphBuilder.RHICmdList.GetGPUMask() : FRHIGPUMask::All());
	AddPass(GraphBuilder, RDG_EVENT_NAME("ReleaseRayTracingResources"), [Views, &RayTracingScene, bIsLastRenderer](FRHICommandListImmediate& RHICmdList)
	{
		if (RayTracingScene.IsCreated())
		{
			// Clear ray tracing bindings only on the last renderer, where multiple view families are rendered
			if (bIsLastRenderer)
			{
				RHICmdList.ClearRayTracingBindings(RayTracingScene.GetRHIRayTracingScene());
			}

			// Track if we ended up rendering anything this frame.  After rendering all view families, we'll release the
			// ray tracing scene resources if nothing used ray tracing.
			if (RayTracingScene.GetInstances().Num() > 0)
			{
				RayTracingScene.bUsedThisFrame = true;
			}
		}

		// Release resources that were bound to the ray tracing scene to allow them to be immediately recycled.
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			FViewInfo& View = Views[ViewIndex];

			// Release common lighting resources -- these are ref counted, so they won't be released until after the last view
			// is finished using them (where multiple view families are rendered).
			View.RayTracingSubSurfaceProfileSRV.SafeRelease();
			View.RayTracingSubSurfaceProfileTexture.SafeRelease();
		}
	});
}

void FDeferredShadingSceneRenderer::WaitForRayTracingScene(FRDGBuilder& GraphBuilder, FRDGBufferRef DynamicGeometryScratchBuffer)
{
	check(bAnyRayTracingPassEnabled);

	TRACE_CPUPROFILER_EVENT_SCOPE(FDeferredShadingSceneRenderer::WaitForRayTracingScene);

	// Keep mask the same as what's already set (which will be the view mask) if TLAS updates should be masked to the view
	RDG_GPU_MASK_SCOPE(GraphBuilder, GRayTracingMultiGpuTLASMask ? GraphBuilder.RHICmdList.GetGPUMask() : FRHIGPUMask::All());

	SetupRayTracingPipelineStates(GraphBuilder);

	const int32 ReferenceViewIndex = 0;
	FViewInfo& ReferenceView = Views[ReferenceViewIndex];

	bool bAnyLumenHardwareInlineRayTracingPassEnabled = false;
	for (const FViewInfo& View : Views)
	{
		bAnyLumenHardwareInlineRayTracingPassEnabled |= Lumen::AnyLumenHardwareInlineRayTracingPassEnabled(Scene, View);
	}

	if (bAnyLumenHardwareInlineRayTracingPassEnabled)
	{
		SetupLumenHardwareRayTracingHitGroupBuffer(GraphBuilder, ReferenceView);
	}

	const bool bIsPathTracing = ViewFamily.EngineShowFlags.PathTracing;

	// Scratch buffer must be referenced in this pass, as it must live until the BVH build is complete.
	FBuildAccelerationStructurePassParams* PassParams = GraphBuilder.AllocParameters<FBuildAccelerationStructurePassParams>();
	PassParams->RayTracingSceneScratchBuffer = Scene->RayTracingScene.BuildScratchBuffer;
	PassParams->DynamicGeometryScratchBuffer = DynamicGeometryScratchBuffer;
	PassParams->LightDataPacked = bIsPathTracing ? nullptr : ReferenceView.RayTracingLightDataUniformBuffer; // accessed by FRayTracingLightingMS
	PassParams->LumenHitDataBuffer = ReferenceView.LumenHardwareRayTracingHitDataBuffer;

	if (IsNaniteEnabled())
	{
		PassParams->ClusterPageData = Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
		PassParams->HierarchyBuffer = Nanite::GStreamingManager.GetHierarchySRV(GraphBuilder);
		PassParams->RayTracingDataBuffer = Nanite::GRayTracingManager.GetAuxiliaryDataSRV(GraphBuilder);
	}
	else
	{
		PassParams->ClusterPageData = nullptr;
		PassParams->HierarchyBuffer = nullptr;
		PassParams->RayTracingDataBuffer = nullptr;
	}

	const FRayTracingLightFunctionMap* RayTracingLightFunctionMap = GraphBuilder.Blackboard.Get<FRayTracingLightFunctionMap>();
	GraphBuilder.AddPass(RDG_EVENT_NAME("WaitForRayTracingScene"), PassParams, ERDGPassFlags::Copy | ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		[this, PassParams, bIsPathTracing, &ReferenceView, bAnyLumenHardwareInlineRayTracingPassEnabled, RayTracingLightFunctionMap](FRHICommandListImmediate& RHICmdList)
	{
		check(ReferenceView.RayTracingMaterialPipeline || ReferenceView.RayTracingMaterialBindings.Num() == 0);

		if (ReferenceView.RayTracingMaterialPipeline && (ReferenceView.RayTracingMaterialBindings.Num() || ReferenceView.RayTracingCallableBindings.Num()))
		{
			if (IsNaniteEnabled())
			{
				FNaniteRayTracingUniformParameters NaniteRayTracingUniformParams;
				NaniteRayTracingUniformParams.PageConstants.X = Scene->GPUScene.InstanceSceneDataSOAStride;
				NaniteRayTracingUniformParams.PageConstants.Y = Nanite::GStreamingManager.GetMaxStreamingPages();
				NaniteRayTracingUniformParams.MaxNodes = Nanite::FGlobalResources::GetMaxNodes();
				NaniteRayTracingUniformParams.MaxVisibleClusters = Nanite::FGlobalResources::GetMaxVisibleClusters();
				NaniteRayTracingUniformParams.RenderFlags = 0;
				NaniteRayTracingUniformParams.RayTracingCutError = Nanite::GRayTracingManager.GetCutError();
				NaniteRayTracingUniformParams.ClusterPageData = PassParams->ClusterPageData->GetRHI();
				NaniteRayTracingUniformParams.HierarchyBuffer = PassParams->HierarchyBuffer->GetRHI();
				NaniteRayTracingUniformParams.RayTracingDataBuffer = PassParams->RayTracingDataBuffer->GetRHI();

				Nanite::GRayTracingManager.GetUniformBuffer().UpdateUniformBufferImmediate(NaniteRayTracingUniformParams);
			}

			BindRayTracingMaterialPipeline(RHICmdList, ReferenceView, ReferenceView.RayTracingMaterialPipeline);

			if (bIsPathTracing)
			{
				SetupPathTracingDefaultMissShader(RHICmdList, ReferenceView);

				BindLightFunctionShadersPathTracing(RHICmdList, Scene, RayTracingLightFunctionMap, ReferenceView);
			}
			else
			{
				SetupRayTracingDefaultMissShader(RHICmdList, ReferenceView);
				SetupRayTracingLightingMissShader(RHICmdList, ReferenceView);
				
				BindLightFunctionShaders(RHICmdList, Scene, RayTracingLightFunctionMap, ReferenceView);
			}
		}

		if (!bIsPathTracing)
		{
			FRayTracingLocalShaderBindings* LumenHardwareRayTracingMaterialBindings = nullptr;

			// When Lumen passes are running in inline-only mode we need to build bindings for HitGroupData here instead of when building the pipeline.
			if (bAnyLumenHardwareInlineRayTracingPassEnabled && !GRHISupportsRayTracingShaders)
			{
				LumenHardwareRayTracingMaterialBindings = BuildLumenHardwareRayTracingMaterialBindings(RHICmdList, ReferenceView, ReferenceView.LumenHardwareRayTracingHitDataBuffer, true);
			}

			if (GRHISupportsRayTracingShaders)
			{
				if (ReferenceView.RayTracingMaterialGatherPipeline)
				{
					RHICmdList.SetRayTracingMissShader(ReferenceView.GetRayTracingSceneChecked(), RAY_TRACING_MISS_SHADER_SLOT_DEFAULT, ReferenceView.RayTracingMaterialGatherPipeline, 0 /* MissShaderPipelineIndex */, 0, nullptr, 0);
					BindRayTracingDeferredMaterialGatherPipeline(RHICmdList, ReferenceView, ReferenceView.RayTracingMaterialGatherPipeline);
				}

				if (ReferenceView.LumenHardwareRayTracingMaterialPipeline)
				{
					RHICmdList.SetRayTracingMissShader(ReferenceView.GetRayTracingSceneChecked(), RAY_TRACING_MISS_SHADER_SLOT_DEFAULT, ReferenceView.LumenHardwareRayTracingMaterialPipeline, 0 /* MissShaderPipelineIndex */, 0, nullptr, 0);
					BindLumenHardwareRayTracingMaterialPipeline(RHICmdList, LumenHardwareRayTracingMaterialBindings, ReferenceView, ReferenceView.LumenHardwareRayTracingMaterialPipeline, ReferenceView.LumenHardwareRayTracingHitDataBuffer);
				}
			}
		}

		// Send ray tracing resources from reference view to all others.
		for (int32 ViewIndex = 0; ViewIndex < AllFamilyViews.Num(); ++ViewIndex)
		{
			// See comment above where we copy "RayTracingSubSurfaceProfileTexture" to each view...
			FViewInfo* View = const_cast<FViewInfo*>(static_cast<const FViewInfo*>(AllFamilyViews[ViewIndex]));
			if (View->bHasAnyRayTracingPass && View != &ReferenceView)
			{
				View->RayTracingMaterialGatherPipeline = ReferenceView.RayTracingMaterialGatherPipeline;
				View->LumenHardwareRayTracingMaterialPipeline = ReferenceView.LumenHardwareRayTracingMaterialPipeline;
			}
		}

		if (RayTracingDynamicGeometryUpdateEndTransition)
		{
			RHICmdList.EndTransition(RayTracingDynamicGeometryUpdateEndTransition);
			RayTracingDynamicGeometryUpdateEndTransition = nullptr;
		}
	});

    // Transition to readable state, synchronizing with previous build operation
	Scene->RayTracingScene.Transition(GraphBuilder, ERayTracingSceneState::Readable);
}

struct FRayTracingRelevantPrimitiveTaskData
{
	FRayTracingRelevantPrimitiveList List;
	FGraphEventRef Task;
};
#endif // RHI_RAYTRACING

void FDeferredShadingSceneRenderer::PreGatherDynamicMeshElements()
{
#if RHI_RAYTRACING
	if (bAnyRayTracingPassEnabled)
	{
		const int32 ReferenceViewIndex = 0;
		FViewInfo& ReferenceView = Views[ReferenceViewIndex];

		RayTracingRelevantPrimitiveTaskData = Allocator.Create<FRayTracingRelevantPrimitiveTaskData>();
		RayTracingRelevantPrimitiveTaskData->Task = FFunctionGraphTask::CreateAndDispatchWhenReady(
			[Scene = this->Scene, &ReferenceView, &RayTracingRelevantPrimitiveList = RayTracingRelevantPrimitiveTaskData->List]()
			{
				FTaskTagScope TaskTagScope(ETaskTag::EParallelRenderingThread);
				GatherRayTracingRelevantPrimitives(*Scene, ReferenceView, RayTracingRelevantPrimitiveList);
			}, TStatId(), nullptr, ENamedThreads::AnyNormalThreadHiPriTask);
	}
#endif // RHI_RAYTRACING

	const bool bHasRayTracedOverlay = HasRayTracedOverlay(ViewFamily);

	extern int32 GEarlyInitDynamicShadows;

	if (GEarlyInitDynamicShadows &&
		CurrentDynamicShadowsTaskData == nullptr &&
		ViewFamily.EngineShowFlags.DynamicShadows
		&& !ViewFamily.EngineShowFlags.HitProxies
		&& !bHasRayTracedOverlay)
	{
		CurrentDynamicShadowsTaskData = BeginInitDynamicShadows(true);
	}
}

static TAutoConsoleVariable<float> CVarStallInitViews(
	TEXT("CriticalPathStall.AfterInitViews"),
	0.0f,
	TEXT("Sleep for the given time after InitViews. Time is given in ms. This is a debug option used for critical path analysis and forcing a change in the critical path."));

void FDeferredShadingSceneRenderer::CommitFinalPipelineState()
{
	// Family pipeline state
	{
		FamilyPipelineState.Set(&FFamilyPipelineState::bNanite, UseNanite(ShaderPlatform)); // TODO: Should this respect ViewFamily.EngineShowFlags.NaniteMeshes?

		static const auto ICVarHZBOcc = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HZBOcclusion"));
		FamilyPipelineState.Set(&FFamilyPipelineState::bHZBOcclusion, ICVarHZBOcc->GetInt() != 0);	
	}

	CommitIndirectLightingState();

	// Views pipeline states
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FViewInfo& View = Views[ViewIndex];
		TPipelineState<FPerViewPipelineState>& ViewPipelineState = GetViewPipelineStateWritable(View);

		// Commit HZB state
		{
			const bool bHasSSGI = ViewPipelineState[&FPerViewPipelineState::DiffuseIndirectMethod] == EDiffuseIndirectMethod::SSGI;
			const bool bUseLumen = ViewPipelineState[&FPerViewPipelineState::DiffuseIndirectMethod] == EDiffuseIndirectMethod::Lumen 
				|| ViewPipelineState[&FPerViewPipelineState::ReflectionsMethod] == EReflectionsMethod::Lumen;

			// Requires FurthestHZB
			ViewPipelineState.Set(&FPerViewPipelineState::bFurthestHZB,
				FamilyPipelineState[&FFamilyPipelineState::bHZBOcclusion] ||
				FamilyPipelineState[&FFamilyPipelineState::bNanite] ||
				ViewPipelineState[&FPerViewPipelineState::AmbientOcclusionMethod] == EAmbientOcclusionMethod::SSAO ||
				ViewPipelineState[&FPerViewPipelineState::ReflectionsMethod] == EReflectionsMethod::SSR ||
				bHasSSGI || bUseLumen);

			ViewPipelineState.Set(&FPerViewPipelineState::bClosestHZB, 
				bHasSSGI || bUseLumen);
		}
	}

	// Commit all the pipeline states.
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			const FViewInfo& View = Views[ViewIndex];

			GetViewPipelineStateWritable(View).Commit();
		}
		FamilyPipelineState.Commit();
	} 
}

bool FDeferredShadingSceneRenderer::IsNaniteEnabled() const
{
	return UseNanite(ShaderPlatform) && ViewFamily.EngineShowFlags.NaniteMeshes && Nanite::GStreamingManager.HasResourceEntries();
}

void FDeferredShadingSceneRenderer::Render(FRDGBuilder& GraphBuilder)
{
	const bool bNaniteEnabled = IsNaniteEnabled();

	GPU_MESSAGE_SCOPE(GraphBuilder);

	ShaderPrint::BeginViews(GraphBuilder, Views);

	ON_SCOPE_EXIT
	{
		ShaderPrint::EndViews(Views);
	};

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];
		RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);

		ShadingEnergyConservation::Init(GraphBuilder, View);
	}

	{
		EUpdateAllPrimitiveSceneInfosAsyncOps AsyncOps = EUpdateAllPrimitiveSceneInfosAsyncOps::None;

		if (GAsyncCreateLightPrimitiveInteractions > 0)
		{
			AsyncOps |= EUpdateAllPrimitiveSceneInfosAsyncOps::CreateLightPrimitiveInteractions;
		}

		if (GAsyncCacheMeshDrawCommands > 0)
		{
			AsyncOps |= EUpdateAllPrimitiveSceneInfosAsyncOps::CacheMeshDrawCommands;
		}

		Scene->UpdateAllPrimitiveSceneInfos(GraphBuilder, AsyncOps);
	}

#if RHI_RAYTRACING
	// Initialize ray tracing flags, in case they weren't initialized in the CreateSceneRenderers code path
	InitializeRayTracingFlags_RenderThread();

	GRayTracingGeometryManager.Tick();

	if ((GetRayTracingMode() == ERayTracingMode::Dynamic) && bHasRayTracingEnableChanged)
	{
		Scene->GetRayTracingDynamicGeometryCollection()->Clear();
	}

	// Now that we have updated all the PrimitiveSceneInfos, update the RayTracing mesh commands cache if needed
	{
		const ERayTracingMeshCommandsMode CurrentMode = ViewFamily.EngineShowFlags.PathTracing ? ERayTracingMeshCommandsMode::PATH_TRACING : ERayTracingMeshCommandsMode::RAY_TRACING;
		bool bNaniteCoarseMeshStreamingModeChanged = false;
#if WITH_EDITOR
		bNaniteCoarseMeshStreamingModeChanged = Nanite::FCoarseMeshStreamingManager::CheckStreamingMode();
#endif // WITH_EDITOR
		const bool bNaniteRayTracingModeChanged = Nanite::GRayTracingManager.CheckModeChanged();

		if (CurrentMode != Scene->CachedRayTracingMeshCommandsMode || bNaniteCoarseMeshStreamingModeChanged || bNaniteRayTracingModeChanged || bHasRayTracingEnableChanged)
		{
			Scene->WaitForCacheMeshDrawCommandsTask();

			// In some situations, we need to refresh the cached ray tracing mesh commands because they contain data about the currently bound shader. 
			// This operation is a bit expensive but only happens once as we transition between modes which should be rare.
			Scene->CachedRayTracingMeshCommandsMode = CurrentMode;
			Scene->RefreshRayTracingMeshCommandCache();
			bHasRayTracingEnableChanged = false;
		}

		if (bRefreshRayTracingInstances)
		{
			Scene->WaitForCacheMeshDrawCommandsTask();

			// In some situations, we need to refresh the cached ray tracing instance.
			// eg: Need to update PrimitiveRayTracingFlags
			// This operation is a bit expensive but only happens once as we transition between modes which should be rare.
			Scene->RefreshRayTracingInstances();
			bRefreshRayTracingInstances = false;
		}

		if (bNaniteRayTracingModeChanged)
		{
			for (FViewInfo& View : Views)
			{
				if (View.ViewState != nullptr && !View.bIsOfflineRender)
				{
					// don't invalidate in the offline case because we only get one attempt at rendering each sample
					View.ViewState->PathTracingInvalidate();
				}
			}
		}
	}
#endif

	FGPUSceneScopeBeginEndHelper GPUSceneScopeBeginEndHelper(Scene->GPUScene, GPUSceneDynamicContext, Scene);

	bool bUpdateNaniteStreaming = false;
	bool bVisualizeNanite = false;
	if (bNaniteEnabled)
	{
		Nanite::GGlobalResources.Update(GraphBuilder);

		// Only update Nanite streaming residency for the first view when multiple view rendering (nDisplay) is enabled.
		// Streaming requests are still accumulated from the remaining views.
		bUpdateNaniteStreaming =  !ViewFamily.bIsMultipleViewFamily || ViewFamily.bIsFirstViewInMultipleViewFamily;
		if (bUpdateNaniteStreaming)
		{
			Nanite::GStreamingManager.BeginAsyncUpdate(GraphBuilder);
		}

		FNaniteVisualizationData& NaniteVisualization = GetNaniteVisualizationData();
		if (Views.Num() > 0)
		{
			const FName& NaniteViewMode = Views[0].CurrentNaniteVisualizationMode;
			if (NaniteVisualization.Update(NaniteViewMode))
			{
				// When activating the view modes from the command line, automatically enable the VisualizeNanite show flag for convenience.
				ViewFamily.EngineShowFlags.SetVisualizeNanite(true);
			}

			bVisualizeNanite = NaniteVisualization.IsActive() && ViewFamily.EngineShowFlags.VisualizeNanite;
		}
	}

	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(RenderOther);

	// Setups the final FViewInfo::ViewRect.
	PrepareViewRectsForRendering(GraphBuilder.RHICmdList);

	const bool bPathTracedAtmosphere = ViewFamily.EngineShowFlags.PathTracing && Views.Num() > 0 && Views[0].FinalPostProcessSettings.PathTracingEnableReferenceAtmosphere;
	if (ShouldRenderSkyAtmosphere(Scene, ViewFamily.EngineShowFlags) && !bPathTracedAtmosphere)
	{
		for (int32 LightIndex = 0; LightIndex < NUM_ATMOSPHERE_LIGHTS; ++LightIndex)
		{
			if (Scene->AtmosphereLights[LightIndex])
			{
				PrepareSunLightProxy(*Scene->GetSkyAtmosphereSceneInfo(),LightIndex, *Scene->AtmosphereLights[LightIndex]);
			}
		}
	}
	else
	{
		Scene->ResetAtmosphereLightsProperties();
	}

	SCOPED_NAMED_EVENT(FDeferredShadingSceneRenderer_Render, FColor::Emerald);

#if WITH_MGPU
	ComputeGPUMasks(&GraphBuilder.RHICmdList);
#endif // WITH_MGPU

	// By default, limit our GPU usage to only GPUs specified in the view masks.
	RDG_GPU_MASK_SCOPE(GraphBuilder, ViewFamily.EngineShowFlags.PathTracing ? FRHIGPUMask::All() : AllViewsGPUMask);

	WaitOcclusionTests(GraphBuilder.RHICmdList);

	if (!ViewFamily.EngineShowFlags.Rendering)
	{
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "Scene");
	RDG_GPU_STAT_SCOPE_VERBOSE(GraphBuilder, Unaccounted, *ViewFamily.ProfileDescription);
	
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_Render_Init);
		RDG_RHI_GPU_STAT_SCOPE(GraphBuilder, AllocateRendertargets);

		// Initialize global system textures (pass-through if already initialized).
		GSystemTextures.InitializeTextures(GraphBuilder.RHICmdList, FeatureLevel);

		// Force the subsurface profile texture to be updated.
		UpdateSubsurfaceProfileTexture(GraphBuilder, ShaderPlatform);

		// Force the rect light texture & IES texture to be updated.
		RectLightAtlas::UpdateAtlasTexture(GraphBuilder, FeatureLevel);
		IESAtlas::UpdateAtlasTexture(GraphBuilder, FeatureLevel);
	}

	InitializeSceneTexturesConfig(ViewFamily.SceneTexturesConfig, ViewFamily);
	FSceneTexturesConfig& SceneTexturesConfig = GetActiveSceneTexturesConfig();
	FSceneTexturesConfig::Set(SceneTexturesConfig);

	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Create(GraphBuilder);

	const bool bHasRayTracedOverlay = HasRayTracedOverlay(ViewFamily);
	const bool bAllowStaticLighting = !bHasRayTracedOverlay && IsStaticLightingAllowed();

	const bool bUseVirtualTexturing = UseVirtualTexturing(FeatureLevel);
	if (bUseVirtualTexturing)
	{
		VirtualTextureFeedbackBegin(GraphBuilder, Views, SceneTexturesConfig.Extent);
	}

	// Important that this uses consistent logic throughout the frame, so evaluate once and pass in the flag from here
	// NOTE: Must be done after  system texture initialization
	// TODO: This doesn't take into account the potential for split screen views with separate shadow caches
	VirtualShadowMapArray.Initialize(GraphBuilder, Scene->GetVirtualShadowMapCache(Views[0]), UseVirtualShadowMaps(ShaderPlatform, FeatureLevel), Views[0].bIsSceneCapture);

	// if DDM_AllOpaqueNoVelocity was used, then velocity should have already been rendered as well
	const bool bIsEarlyDepthComplete = (DepthPass.EarlyZPassMode == DDM_AllOpaque || DepthPass.EarlyZPassMode == DDM_AllOpaqueNoVelocity);

	// Use read-only depth in the base pass if we have a full depth prepass.
	const bool bAllowReadOnlyDepthBasePass = bIsEarlyDepthComplete
		&& !ViewFamily.EngineShowFlags.ShaderComplexity
		&& !ViewFamily.UseDebugViewPS()
		&& !ViewFamily.EngineShowFlags.Wireframe
		&& !ViewFamily.EngineShowFlags.LightMapDensity;

	const FExclusiveDepthStencil::Type BasePassDepthStencilAccess =
		bAllowReadOnlyDepthBasePass
		? FExclusiveDepthStencil::DepthRead_StencilWrite
		: FExclusiveDepthStencil::DepthWrite_StencilWrite;

	FILCUpdatePrimTaskData ILCTaskData;

	// Find the visible primitives.
	if (GDynamicRHI->RHIIncludeOptionalFlushes())
	{
		GraphBuilder.RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
	}

	FInstanceCullingManager& InstanceCullingManager = *GraphBuilder.AllocObject<FInstanceCullingManager>(Scene->GPUScene.IsEnabled(), GraphBuilder);

	::Strata::PreInitViews(*Scene);

	{
		RDG_GPU_STAT_SCOPE(GraphBuilder, VisibilityCommands);
		BeginInitViews(GraphBuilder, SceneTexturesConfig, BasePassDepthStencilAccess, ILCTaskData, InstanceCullingManager);
	}

	// GetBinIndexTranslator cannot be called before UpdateAllPrimitiveSceneInfos which can change the number of raster bins
	FNaniteScopedVisibilityFrame NaniteVisibility(
		bNaniteEnabled,
		Scene->NaniteVisibility[ENaniteMeshPass::BasePass],
		Scene->NaniteRasterPipelines[ENaniteMeshPass::BasePass].GetBinIndexTranslator());

	FNaniteVisibilityQuery* NaniteVisibilityQuery = nullptr;
	if (bNaniteEnabled)
	{
		if (Views.Num() > 0)
		{
			TArray<FConvexVolume, TInlineAllocator<2>> NaniteCullingViews;

			// For now we'll share the same visibility results across all views
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				FViewInfo& View = Views[ViewIndex];
				NaniteCullingViews.Add(View.ViewFrustum);
			}

			NaniteVisibilityQuery = NaniteVisibility.Get().BeginVisibilityQuery(
				NaniteCullingViews,
				&Scene->NaniteRasterPipelines[ENaniteMeshPass::BasePass],
				&Scene->NaniteMaterials[ENaniteMeshPass::BasePass]
			);
		}
	}

	// Compute & commit the final state of the entire dependency topology of the renderer.
	CommitFinalPipelineState();

#if !UE_BUILD_SHIPPING
	if (CVarStallInitViews.GetValueOnRenderThread() > 0.0f)
	{
		SCOPE_CYCLE_COUNTER(STAT_InitViews_Intentional_Stall);
		FPlatformProcess::Sleep(CVarStallInitViews.GetValueOnRenderThread() / 1000.0f);
	}
#endif

	extern TSet<IPersistentViewUniformBufferExtension*> PersistentViewUniformBufferExtensions;

	for (IPersistentViewUniformBufferExtension* Extension : PersistentViewUniformBufferExtensions)
	{
		Extension->BeginFrame();

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			// Must happen before RHI thread flush so any tasks we dispatch here can land in the idle gap during the flush
			Extension->PrepareView(&Views[ViewIndex]);
		}
	}

#if RHI_RAYTRACING

	const int32 ReferenceViewIndex = 0;
	FViewInfo& ReferenceView = Views[ReferenceViewIndex];

	// Prepare the scene for rendering this frame.
	FRayTracingScene& RayTracingScene = Scene->RayTracingScene;
	RayTracingScene.Reset(IsRayTracingInstanceDebugDataEnabled(ReferenceView)); // Resets the internal arrays, but does not release any resources.

	if (ShouldPrepareRayTracingDecals(*Scene, ViewFamily))
	{
		// Calculate decal grid for ray tracing per view since decal fade is view dependent
		// TODO: investigate reusing the same grid for all views (ie: different callable shader SBT entries for each view so fade alpha is still correct for each view)

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			FViewInfo& View = Views[ViewIndex];
			View.RayTracingDecalUniformBuffer = CreateRayTracingDecalData(GraphBuilder, *Scene, View, RayTracingScene.NumCallableShaderSlots);
			View.bHasRayTracingDecals = true;
			RayTracingScene.NumCallableShaderSlots += Scene->Decals.Num();
		}
	}
	else
	{
		TRDGUniformBufferRef<FRayTracingDecals> NullRayTracingDecalUniformBuffer = CreateNullRayTracingDecalsUniformBuffer(GraphBuilder);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			FViewInfo& View = Views[ViewIndex];
			View.RayTracingDecalUniformBuffer = NullRayTracingDecalUniformBuffer;
			View.bHasRayTracingDecals = false;
		}
	}

	if (IsRayTracingEnabled() && RHISupportsRayTracingShaders(ViewFamily.GetShaderPlatform()))
	{
		// Nanite raytracing manager update must run before GPUScene update since it can modify primitive data
		Nanite::GRayTracingManager.Update();

		if (!ViewFamily.EngineShowFlags.PathTracing)
		{
			// get the default lighting miss shader (to implicitly fill in the MissShader library before the RT pipeline is created)
			GetRayTracingLightingMissShader(ReferenceView.ShaderMap);
			RayTracingScene.NumMissShaderSlots++;
		}

		if (ViewFamily.EngineShowFlags.LightFunctions)
		{
			// gather all the light functions that may be used (and also count how many miss shaders we will need)
			FRayTracingLightFunctionMap RayTracingLightFunctionMap;
			if (ViewFamily.EngineShowFlags.PathTracing)
			{
				RayTracingLightFunctionMap = GatherLightFunctionLightsPathTracing(Scene, ViewFamily.EngineShowFlags, ReferenceView.GetFeatureLevel());
			}
			else
			{
				RayTracingLightFunctionMap = GatherLightFunctionLights(Scene, ViewFamily.EngineShowFlags, ReferenceView.GetFeatureLevel());
			}
			if (!RayTracingLightFunctionMap.IsEmpty())
			{
				// If we got some light functions in our map, store them in the RDG blackboard so downstream functions can use them.
				// The map itself will be strictly read-only from this point on.
				GraphBuilder.Blackboard.Create<FRayTracingLightFunctionMap>(MoveTemp(RayTracingLightFunctionMap));
			}
		}
	}
#endif // RHI_RAYTRACING

	// Notify the FX system that the scene is about to be rendered.
	if (FXSystem && Views.IsValidIndex(0))
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_FXSystem_PreRender);
		GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLM_FXPreRender));
		FXSystem->PreRender(GraphBuilder, Views, true /*bAllowGPUParticleUpdate*/);
		if (FGPUSortManager* GPUSortManager = FXSystem->GetGPUSortManager())
		{
			GPUSortManager->OnPreRender(GraphBuilder);
		}
	}

	FRDGExternalAccessQueue ExternalAccessQueue;

	{
		RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, UpdateGPUScene);
		RDG_GPU_STAT_SCOPE(GraphBuilder, GPUSceneUpdate);

		if (bIsFirstSceneRenderer)
		{
			GraphBuilder.SetFlushResourcesRHI();
		}

		Scene->GPUScene.Update(GraphBuilder, *Scene, ExternalAccessQueue);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			FViewInfo& View = Views[ViewIndex];
			RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);

			Scene->GPUScene.UploadDynamicPrimitiveShaderDataForView(GraphBuilder, *Scene, View, ExternalAccessQueue);

			Scene->GPUScene.DebugRender(GraphBuilder, *Scene, View);
		}

		InstanceCullingManager.BeginDeferredCulling(GraphBuilder, Scene->GPUScene);

		if (Views.Num() > 0)
		{
			FViewInfo& View = Views[0];
			Scene->UpdatePhysicsField(GraphBuilder, View);
		}
	}

	FSceneTextures::InitializeViewFamily(GraphBuilder, ViewFamily);
	FSceneTextures& SceneTextures = GetActiveSceneTextures();

	if (bUseVirtualTexturing)
	{
		FVirtualTextureUpdateSettings Settings;
		Settings.EnableThrottling(!ViewFamily.bOverrideVirtualTextureThrottle);

		// We can move this call earlier and enable r.VT.AsyncPageRequestTask after fixing race conditions with InitViews
		TUniquePtr<FVirtualTextureUpdater> VirtualTextureUpdater;
		VirtualTextureUpdater = FVirtualTextureSystem::Get().BeginUpdate(GraphBuilder, FeatureLevel, Scene, Settings);

		// Note, should happen after the GPU-Scene update to ensure rendering to runtime virtual textures is using the correctly updated scene
		FVirtualTextureSystem::Get().EndUpdate(GraphBuilder, MoveTemp(VirtualTextureUpdater), FeatureLevel);
	}

#if RHI_RAYTRACING
	if (bAnyRayTracingPassEnabled)
	{
		// Wait until RayTracingRelevantPrimitiveList is ready
		if (RayTracingRelevantPrimitiveTaskData->Task.IsValid())
		{
			RayTracingRelevantPrimitiveTaskData->Task->Wait();
			RayTracingRelevantPrimitiveTaskData->Task.SafeRelease();
		}

		// Prepare ray tracing scene instance list
		checkf(RayTracingRelevantPrimitiveTaskData->List.bValid, TEXT("Ray tracing relevant primitive list is expected to have been created before GatherRayTracingWorldInstancesForView() is called."));

		GatherRayTracingWorldInstancesForView(GraphBuilder, ReferenceView, RayTracingScene, RayTracingRelevantPrimitiveTaskData->List);
	}
#endif // RHI_RAYTRACING

	const bool bUseGBuffer = IsUsingGBuffers(ShaderPlatform);
	
	const bool bRenderDeferredLighting = ViewFamily.EngineShowFlags.Lighting
		&& FeatureLevel >= ERHIFeatureLevel::SM5
		&& ViewFamily.EngineShowFlags.DeferredLighting
		&& bUseGBuffer
		&& !bHasRayTracedOverlay;

	bool bComputeLightGrid = false;
	bool bAnyLumenEnabled = false;

	{
		if (bUseGBuffer)
		{
			bComputeLightGrid = bRenderDeferredLighting;
		}
		else
		{
			bComputeLightGrid = ViewFamily.EngineShowFlags.Lighting;
		}

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			FViewInfo& View = Views[ViewIndex];
			bAnyLumenEnabled = bAnyLumenEnabled 
				|| GetViewPipelineState(View).DiffuseIndirectMethod == EDiffuseIndirectMethod::Lumen
				|| GetViewPipelineState(View).ReflectionsMethod == EReflectionsMethod::Lumen;
		}

		bComputeLightGrid |= (
			ShouldRenderVolumetricFog() ||
			VolumetricCloudWantsToSampleLocalLights(Scene, ViewFamily.EngineShowFlags) ||
			ViewFamily.ViewMode != VMI_Lit ||
			bAnyLumenEnabled ||
			VirtualShadowMapArray.IsEnabled() ||
			ShouldVisualizeLightGrid());
	}

	// force using occ queries for wireframe if rendering is parented or frozen in the first view
	check(Views.Num());
	#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
		const bool bIsViewFrozen = false;
		const bool bHasViewParent = false;
	#else
		const bool bIsViewFrozen = Views[0].State && ((FSceneViewState*)Views[0].State)->bIsFrozen;
		const bool bHasViewParent = Views[0].State && ((FSceneViewState*)Views[0].State)->HasViewParent();
	#endif

	
	const bool bIsOcclusionTesting = DoOcclusionQueries() && !ViewFamily.EngineShowFlags.DisableOcclusionQueries
		&& (!ViewFamily.EngineShowFlags.Wireframe || bIsViewFrozen || bHasViewParent);
	const bool bNeedsPrePass = ShouldRenderPrePass();

	GEngine->GetPreRenderDelegateEx().Broadcast(GraphBuilder);

	// Strata initialisation is always run even when not enabled.
	const bool bStrataEnabled = Strata::IsStrataEnabled();
	Strata::InitialiseStrataFrameSceneData(GraphBuilder, *this);

	if (DepthPass.IsComputeStencilDitherEnabled())
	{
		AddDitheredStencilFillPass(GraphBuilder, Views, SceneTextures.Depth.Target, DepthPass);
	}

	FHairStrandsBookmarkParameters& HairStrandsBookmarkParameters = *GraphBuilder.AllocObject<FHairStrandsBookmarkParameters>();
	if (IsHairStrandsEnabled(EHairStrandsShaderType::All, Scene->GetShaderPlatform()))
	{
		HairStrandsBookmarkParameters = CreateHairStrandsBookmarkParameters(Scene, Views[0]);
		RunHairStrandsBookmark(GraphBuilder, EHairStrandsBookmark::ProcessTasks, HairStrandsBookmarkParameters);

		// Interpolation needs to happen after the skin cache run as there is a dependency 
		// on the skin cache output.
		const bool bRunHairStrands = HairStrandsBookmarkParameters.HasInstances() && (Views.Num() > 0);
		if (bRunHairStrands)
		{
			if (IsHairStrandsEnabled(EHairStrandsShaderType::Strands, Scene->GetShaderPlatform()))
			{
				RunHairStrandsBookmark(GraphBuilder, EHairStrandsBookmark::ProcessGatherCluster, HairStrandsBookmarkParameters);

				FHairCullingParams CullingParams;
				CullingParams.bCullingProcessSkipped = false;
				ComputeHairStrandsClustersCulling(GraphBuilder, *HairStrandsBookmarkParameters.ShaderMap, Views, CullingParams, HairStrandsBookmarkParameters.HairClusterData);
			}

			RunHairStrandsBookmark(GraphBuilder, EHairStrandsBookmark::ProcessStrandsInterpolation, HairStrandsBookmarkParameters);
		}
		else
		{
			for (FViewInfo& View : Views)
			{
				View.HairStrandsViewData.UniformBuffer = HairStrands::CreateDefaultHairStrandsViewUniformBuffer(GraphBuilder, View);
			}
		}
	}

	if (bNaniteEnabled)
	{
		// Must happen before any Nanite rendering in the frame
		if (bUpdateNaniteStreaming)
		{
			Nanite::GStreamingManager.EndAsyncUpdate(GraphBuilder);

			const TSet<uint32> ModifiedResources = Nanite::GStreamingManager.GetAndClearModifiedResources();
#if RHI_RAYTRACING
			Nanite::GRayTracingManager.RequestUpdates(ModifiedResources);
#endif
		}
	}

	PrepareDistanceFieldScene(GraphBuilder, ExternalAccessQueue, false);

	FLumenSceneFrameTemporaries LumenFrameTemporaries;
	{
		{
			RDG_RHI_GPU_STAT_SCOPE(GraphBuilder, VisibilityCommands);
			EndInitViews(GraphBuilder, LumenFrameTemporaries, ILCTaskData, InstanceCullingManager, ExternalAccessQueue);
		}

		// Dynamic vertex and index buffers need to be committed before rendering.
		{
			SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_FGlobalDynamicVertexBuffer_Commit);
			DynamicIndexBufferForInitViews.Commit();
			DynamicVertexBufferForInitViews.Commit();
			DynamicReadBufferForInitViews.Commit();
		}

		{
			SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_FGlobalDynamicVertexBuffer_Commit);
			DynamicVertexBufferForInitShadows.Commit();
			DynamicIndexBufferForInitShadows.Commit();
			DynamicReadBufferForInitShadows.Commit();
		}
	}

	ExternalAccessQueue.Submit(GraphBuilder);

	const bool bShouldRenderVelocities = ShouldRenderVelocities();
	const EShaderPlatform Platform = GetViewFamilyInfo(Views).GetShaderPlatform();
	const bool bBasePassCanOutputVelocity = FVelocityRendering::BasePassCanOutputVelocity(Platform);
	const bool bHairStrandsEnable = HairStrandsBookmarkParameters.HasInstances() && Views.Num() > 0 && IsHairStrandsEnabled(EHairStrandsShaderType::Strands, Platform);

	{
		GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLM_PrePass));

		// Both compute approaches run earlier, so skip clearing stencil here, just load existing.
		const ERenderTargetLoadAction StencilLoadAction = DepthPass.IsComputeStencilDitherEnabled()
			? ERenderTargetLoadAction::ELoad
			: ERenderTargetLoadAction::EClear;

		const ERenderTargetLoadAction DepthLoadAction = ERenderTargetLoadAction::EClear;
		AddClearDepthStencilPass(GraphBuilder, SceneTextures.Depth.Target, DepthLoadAction, StencilLoadAction);

		// Draw the scene pre-pass / early z pass, populating the scene depth buffer and HiZ
		if (bNeedsPrePass)
		{
			RenderPrePass(GraphBuilder, SceneTextures.Depth.Target, InstanceCullingManager);
		}
		else
		{
			// We didn't do the prepass, but we still want the HMD mask if there is one
			RenderPrePassHMD(GraphBuilder, SceneTextures.Depth.Target);
		}

		GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLM_AfterPrePass));

		// special pass for DDM_AllOpaqueNoVelocity, which uses the velocity pass to finish the early depth pass write
		if (bShouldRenderVelocities && Scene->EarlyZPassMode == DDM_AllOpaqueNoVelocity)
		{
			// Render the velocities of movable objects
			GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLM_Velocity));
			RenderVelocities(GraphBuilder, SceneTextures, EVelocityPass::Opaque, bHairStrandsEnable);
			GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLM_AfterVelocity));
		}
	}

	TArray<Nanite::FRasterResults, TInlineAllocator<2>> NaniteRasterResults;
	TArray<Nanite::FPackedView, TInlineAllocator<2>> NaniteViews;
	{
		if (bNaniteEnabled && Views.Num() > 0)
		{
			LLM_SCOPE_BYTAG(Nanite);
			TRACE_CPUPROFILER_EVENT_SCOPE(InitNaniteRaster);

			NaniteRasterResults.AddDefaulted(Views.Num());
			if (NaniteVisibilityQuery != nullptr)
			{
				NaniteVisibility.Get().FinishVisibilityQuery(NaniteVisibilityQuery, NaniteRasterResults[0].VisibilityResults);

				// For now we'll share the same visibility results across all views
				for (int32 ViewIndex = 1; ViewIndex < NaniteRasterResults.Num(); ++ViewIndex)
				{
					NaniteRasterResults[ViewIndex].VisibilityResults = NaniteRasterResults[0].VisibilityResults;
				}

				uint32 TotalRasterBins = 0;
				uint32 VisibleRasterBins = 0;
				NaniteRasterResults[0].VisibilityResults.GetRasterBinStats(VisibleRasterBins, TotalRasterBins);

				uint32 TotalShadingDraws = 0;
				uint32 VisibleShadingDraws = 0;
				NaniteRasterResults[0].VisibilityResults.GetShadingDrawStats(VisibleShadingDraws, TotalShadingDraws);

				SET_DWORD_STAT(STAT_NaniteBasePassTotalRasterBins, TotalRasterBins);
				SET_DWORD_STAT(STAT_NaniteBasePassTotalShadingDraws, TotalShadingDraws);

				SET_DWORD_STAT(STAT_NaniteBasePassVisibleRasterBins, VisibleRasterBins);
				SET_DWORD_STAT(STAT_NaniteBasePassVisibleShadingDraws, VisibleShadingDraws);
			}

			const FIntPoint RasterTextureSize = SceneTextures.Depth.Target->Desc.Extent;

			// Primary raster view
			{
				Nanite::FSharedContext SharedContext{};
				SharedContext.FeatureLevel = Scene->GetFeatureLevel();
				SharedContext.ShaderMap = GetGlobalShaderMap(SharedContext.FeatureLevel);
				SharedContext.Pipeline = Nanite::EPipeline::Primary;

				FIntRect RasterTextureRect(0, 0, RasterTextureSize.X, RasterTextureSize.Y);
				if (Views.Num() == 1)
				{
					const FViewInfo& View = Views[0];
					if (View.ViewRect.Min.X == 0 && View.ViewRect.Min.Y == 0)
					{
						RasterTextureRect = View.ViewRect;
					}
				}

				Nanite::FRasterContext RasterContext;

				// Nanite::VisBuffer (Visibility Buffer Clear)
				{
					RDG_GPU_STAT_SCOPE(GraphBuilder, NaniteVisBuffer);
					RasterContext = Nanite::InitRasterContext(
						GraphBuilder,
						SharedContext,
						ViewFamily,
						RasterTextureSize,
						RasterTextureRect,
						ViewFamily.EngineShowFlags.VisualizeNanite
					);
				}

				Nanite::FCullingContext::FConfiguration CullingConfig = { 0 };
				CullingConfig.bTwoPassOcclusion = true;
				CullingConfig.bUpdateStreaming = true;
				CullingConfig.bPrimaryContext = true;
				CullingConfig.bForceHWRaster = RasterContext.RasterScheduling == Nanite::ERasterScheduling::HardwareOnly;
				CullingConfig.bProgrammableRaster = GNaniteProgrammableRasterPrimary != 0;

				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View%d", ViewIndex);

					Nanite::FRasterResults& RasterResults = NaniteRasterResults[ViewIndex];
					const FViewInfo& View = Views[ViewIndex];
					CullingConfig.SetViewFlags(View);

					static FString EmptyFilterName = TEXT(""); // Empty filter represents primary view.
					const bool bExtractStats = Nanite::IsStatFilterActive(EmptyFilterName);

					float LODScaleFactor = 1.0f;
					if (View.PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::TemporalUpscale &&
						CVarNaniteViewMeshLODBiasEnable.GetValueOnRenderThread() != 0)
					{
						float TemporalUpscaleFactor = float(View.GetSecondaryViewRectSize().X) / float(View.ViewRect.Width());

						LODScaleFactor = TemporalUpscaleFactor * FMath::Exp2(-CVarNaniteViewMeshLODBiasOffset.GetValueOnRenderThread());
						LODScaleFactor = FMath::Min(LODScaleFactor, FMath::Exp2(-CVarNaniteViewMeshLODBiasMin.GetValueOnRenderThread()));
					}

					float MaxPixelsPerEdgeMultipler = 1.0f;
					if (GDynamicNaniteScalingPrimary.GetSettings().IsEnabled())
					{
						MaxPixelsPerEdgeMultipler = 1.0f / DynamicResolutionFractions[GDynamicNaniteScalingPrimary];
					}

					FIntRect HZBTestRect(0, 0, View.PrevViewInfo.ViewRect.Width(), View.PrevViewInfo.ViewRect.Height());
					Nanite::FPackedView PackedView = Nanite::CreatePackedViewFromViewInfo(
						View,
						RasterTextureSize,
						NANITE_VIEW_FLAG_HZBTEST | NANITE_VIEW_FLAG_NEAR_CLIP,
						/* StreamingPriorityCategory = */ 3,
						/* MinBoundsRadius = */ 0.0f,
						LODScaleFactor,
						MaxPixelsPerEdgeMultipler,
						/* viewport rect in HZB space. HZB is built per view and is always 0,0-based */
						&HZBTestRect
					);

					NaniteViews.Add(PackedView);

					Nanite::FCullingContext CullingContext{};

					// Nanite::VisBuffer (Culling and Rasterization)
					{
						DynamicRenderScaling::FRDGScope DynamicScalingScope(GraphBuilder, GDynamicNaniteScalingPrimary);

						RDG_GPU_STAT_SCOPE(GraphBuilder, NaniteVisBuffer);
						RDG_EVENT_SCOPE(GraphBuilder, "Nanite::VisBuffer");

						CullingContext = Nanite::InitCullingContext(
							GraphBuilder,
							SharedContext,
							*Scene,
							!bIsEarlyDepthComplete ? View.PrevViewInfo.NaniteHZB : View.PrevViewInfo.HZB,
							View.ViewRect,
							CullingConfig
						);

						Nanite::CullRasterize(
							GraphBuilder,
							Scene->NaniteRasterPipelines[ENaniteMeshPass::BasePass],
							RasterResults.VisibilityResults,
							*Scene,
							View,
							{ PackedView },
							SharedContext,
							CullingContext,
							RasterContext,
							/*OptionalInstanceDraws*/ nullptr,
							bExtractStats
						);
					}

					// Nanite::BasePass (Depth Pre-Pass and HZB Build)
					{
						RDG_GPU_STAT_SCOPE(GraphBuilder, NaniteBasePass);

						// Emit velocity with depth if not writing it in base pass.
						FRDGTexture* VelocityBuffer = !IsUsingBasePassVelocity(ShaderPlatform) ? SceneTextures.Velocity : nullptr;

						const bool bEmitStencilMask = NANITE_MATERIAL_STENCIL != 0;

						if (bNeedsPrePass)
						{
							Nanite::EmitDepthTargets(
								GraphBuilder,
								*Scene,
								Views[ViewIndex],
								CullingContext.PageConstants,
								CullingContext.VisibleClustersSWHW,
								CullingContext.ViewsBuffer,
								SceneTextures.Depth.Target,
								RasterContext.VisBuffer64,
								VelocityBuffer,
								RasterResults.MaterialDepth,
								RasterResults.MaterialResolve,
								bEmitStencilMask
							);
						}

						if (!bIsEarlyDepthComplete && CullingConfig.bTwoPassOcclusion && View.ViewState)
						{
							// Won't have a complete SceneDepth for post pass so can't use complete HZB for main pass or it will poke holes in the post pass HZB killing occlusion culling.
							RDG_EVENT_SCOPE(GraphBuilder, "Nanite::BuildHZB");

							FRDGTextureRef SceneDepth = SystemTextures.Black;
							FRDGTextureRef GraphHZB = nullptr;

							const FIntRect PrimaryViewRect = View.GetPrimaryView()->ViewRect;

							BuildHZBFurthest(
								GraphBuilder,
								SceneDepth,
								RasterContext.VisBuffer64,
								PrimaryViewRect,
								FeatureLevel,
								ShaderPlatform,
								TEXT("Nanite.HZB"),
								/* OutFurthestHZBTexture = */ &GraphHZB);

							GraphBuilder.QueueTextureExtraction(GraphHZB, &View.ViewState->PrevFrameViewInfo.NaniteHZB);
						}

						Nanite::ExtractResults(GraphBuilder, CullingContext, RasterContext, RasterResults);
					}
				}
			}
		}
	}

	SceneTextures.SetupMode = ESceneTextureSetupMode::SceneDepth;
	SceneTextures.UniformBuffer = CreateSceneTextureUniformBuffer(GraphBuilder, &SceneTextures, FeatureLevel, SceneTextures.SetupMode);

	AddResolveSceneDepthPass(GraphBuilder, Views, SceneTextures.Depth);

	GVRSImageManager.PrepareImageBasedVRS(GraphBuilder, ViewFamily, SceneTextures);

	FComputeLightGridOutput ComputeLightGridOutput = {};

	// NOTE: The ordering of the lights is used to select sub-sets for different purposes, e.g., those that support clustered deferred.
	FSortedLightSetSceneInfo& SortedLightSet = *GraphBuilder.AllocObject<FSortedLightSetSceneInfo>();
	{
		RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, SortLights);
		RDG_GPU_STAT_SCOPE(GraphBuilder, SortLights);
		ComputeLightGridOutput = GatherLightsAndComputeLightGrid(GraphBuilder, bComputeLightGrid, SortedLightSet);
	}

	CSV_CUSTOM_STAT(LightCount, All,  float(SortedLightSet.SortedLights.Num()), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(LightCount, Batched, float(SortedLightSet.UnbatchedLightStart), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(LightCount, Unbatched, float(SortedLightSet.SortedLights.Num()) - float(SortedLightSet.UnbatchedLightStart), ECsvCustomStatOp::Set);

	FCompositionLighting CompositionLighting(Views, SceneTextures, [this] (int32 ViewIndex)
	{
		return GetViewPipelineState(Views[ViewIndex]).AmbientOcclusionMethod == EAmbientOcclusionMethod::SSAO;
	});

	const bool bShouldRenderSkyAtmosphere = ShouldRenderSkyAtmosphere(Scene, ViewFamily.EngineShowFlags);
	const ESkyAtmospherePassLocation SkyAtmospherePassLocation = GetSkyAtmospherePassLocation();
	const bool bShouldRenderVolumetricCloudBase = ShouldRenderVolumetricCloud(Scene, ViewFamily.EngineShowFlags);
	const bool bShouldRenderVolumetricCloud = bShouldRenderVolumetricCloudBase && (!ViewFamily.EngineShowFlags.VisualizeVolumetricCloudConservativeDensity && !ViewFamily.EngineShowFlags.VisualizeVolumetricCloudEmptySpaceSkipping);
	const bool bShouldVisualizeVolumetricCloud = bShouldRenderVolumetricCloudBase && (!!ViewFamily.EngineShowFlags.VisualizeVolumetricCloudConservativeDensity || !!ViewFamily.EngineShowFlags.VisualizeVolumetricCloudEmptySpaceSkipping);
	bool bAsyncComputeVolumetricCloud = IsVolumetricRenderTargetEnabled() && IsVolumetricRenderTargetAsyncCompute();
	bool bVolumetricRenderTargetRequired = bShouldRenderVolumetricCloud && !bHasRayTracedOverlay;

	if (SkyAtmospherePassLocation == ESkyAtmospherePassLocation::BeforeOcclusion && bShouldRenderSkyAtmosphere)
	{
		// Generate the Sky/Atmosphere look up tables
		RenderSkyAtmosphereLookUpTables(GraphBuilder, ExternalAccessQueue);
	}

	const auto RenderOcclusionLambda = [&]()
	{
		const int32 AsyncComputeMode = CVarSceneDepthHZBAsyncCompute.GetValueOnRenderThread();
		bool bAsyncCompute = AsyncComputeMode != 0;

		FBuildHZBAsyncComputeParams AsyncComputeParams = {};
		if (AsyncComputeMode == 2)
		{
			AsyncComputeParams.Prerequisite = ComputeLightGridOutput.CompactLinksPass;
		}

		RenderOcclusion(GraphBuilder, SceneTextures, bIsOcclusionTesting, 
			bAsyncCompute ? &AsyncComputeParams : nullptr);

		CompositionLighting.ProcessAfterOcclusion(GraphBuilder);
	};

	// Early occlusion queries
	const bool bOcclusionBeforeBasePass = ((DepthPass.EarlyZPassMode == EDepthDrawingMode::DDM_AllOccluders) || bIsEarlyDepthComplete);

	if (bOcclusionBeforeBasePass)
	{
		RenderOcclusionLambda();
	}

	// End early occlusion queries

	BeginAsyncDistanceFieldShadowProjections(GraphBuilder, SceneTextures);

	if (bShouldRenderVolumetricCloudBase)
	{
		InitVolumetricRenderTargetForViews(GraphBuilder, Views);
	}
	else
	{
		ResetVolumetricRenderTargetForViews(GraphBuilder, Views);
	}

	InitVolumetricCloudsForViews(GraphBuilder, bShouldRenderVolumetricCloudBase, InstanceCullingManager);

	// Generate sky LUTs
	// TODO: Valid shadow maps (for volumetric light shafts) have not yet been generated at this point in the frame. Need to resolve dependency ordering!
	// This also must happen before the BasePass for Sky material to be able to sample valid LUTs.
	if (SkyAtmospherePassLocation == ESkyAtmospherePassLocation::BeforeBasePass && bShouldRenderSkyAtmosphere)
	{
		// Generate the Sky/Atmosphere look up tables
		RenderSkyAtmosphereLookUpTables(GraphBuilder, ExternalAccessQueue);

		// Sky env map capture uses the view UB, which contains the LUTs computed above. We need to transition them to readable now.
		ExternalAccessQueue.Submit(GraphBuilder);
	}

	// Capture the SkyLight using the SkyAtmosphere and VolumetricCloud component if available.
	const bool bRealTimeSkyCaptureEnabled = Scene->SkyLight && Scene->SkyLight->bRealTimeCaptureEnabled && Views.Num() > 0 && ViewFamily.EngineShowFlags.SkyLighting;
	if (bRealTimeSkyCaptureEnabled)
	{
		FViewInfo& MainView = Views[0];
		Scene->AllocateAndCaptureFrameSkyEnvMap(GraphBuilder, *this, MainView, bShouldRenderSkyAtmosphere, bShouldRenderVolumetricCloud, InstanceCullingManager, ExternalAccessQueue);
	}

	const ECustomDepthPassLocation CustomDepthPassLocation = GetCustomDepthPassLocation(ShaderPlatform);
	if (CustomDepthPassLocation == ECustomDepthPassLocation::BeforeBasePass)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_CustomDepthPass_BeforeBasePass);
		if (RenderCustomDepthPass(GraphBuilder, SceneTextures.CustomDepth, SceneTextures.GetSceneTextureShaderParameters(FeatureLevel), NaniteRasterResults, NaniteViews, GNaniteProgrammableRasterPrimary != 0))
		{
			SceneTextures.SetupMode |= ESceneTextureSetupMode::CustomDepth;
			SceneTextures.UniformBuffer = CreateSceneTextureUniformBuffer(GraphBuilder, &SceneTextures, FeatureLevel, SceneTextures.SetupMode);
		}
	}

	// Lumen updates need access to sky atmosphere LUT.
	ExternalAccessQueue.Submit(GraphBuilder);

	UpdateLumenScene(GraphBuilder, LumenFrameTemporaries);

	FRDGTextureRef HalfResolutionDepthCheckerboardMinMaxTexture = nullptr;
	FRDGTextureRef QuarterResolutionDepthMinMaxTexture = nullptr;
	bool bQuarterResMinMaxDepthRequired = bShouldRenderVolumetricCloud && ShouldVolumetricCloudTraceWithMinMaxDepth(Views);

	auto GenerateQuarterResDepthMinMaxTexture = [&](auto& GraphBuilder, auto& Views, auto& InputTexture)
	{
		if (bQuarterResMinMaxDepthRequired)
		{
			check(InputTexture != nullptr);	// Must receive a valid texture
			check(QuarterResolutionDepthMinMaxTexture == nullptr);	// Only generate it once
			return CreateQuarterResolutionDepthMinAndMax(GraphBuilder, Views, InputTexture);
		}
		return FRDGTextureRef(nullptr);
	};

	// Kick off async compute cloud early if all depth has been written in the prepass
	if (bShouldRenderVolumetricCloud && bAsyncComputeVolumetricCloud && DepthPass.EarlyZPassMode == DDM_AllOpaque && !bHasRayTracedOverlay)
	{
		HalfResolutionDepthCheckerboardMinMaxTexture = CreateHalfResolutionDepthCheckerboardMinMax(GraphBuilder, Views, SceneTextures.Depth.Resolve);
		QuarterResolutionDepthMinMaxTexture = GenerateQuarterResDepthMinMaxTexture(GraphBuilder, Views, HalfResolutionDepthCheckerboardMinMaxTexture);

		bool bSkipVolumetricRenderTarget = false;
		bool bSkipPerPixelTracing = true;
		bAsyncComputeVolumetricCloud = RenderVolumetricCloud(GraphBuilder, SceneTextures, bSkipVolumetricRenderTarget, bSkipPerPixelTracing, 
			HalfResolutionDepthCheckerboardMinMaxTexture, QuarterResolutionDepthMinMaxTexture, true, InstanceCullingManager);
	}
	
	FRDGTextureRef ForwardScreenSpaceShadowMaskTexture = nullptr;
	FRDGTextureRef ForwardScreenSpaceShadowMaskHairTexture = nullptr;
	if (IsForwardShadingEnabled(ShaderPlatform))
	{
		// With forward shading we need to render shadow maps early
		ensureMsgf(!VirtualShadowMapArray.IsEnabled(), TEXT("Virtual shadow maps are not supported in the forward shading path"));
		RenderShadowDepthMaps(GraphBuilder, InstanceCullingManager, ExternalAccessQueue);

		if (bHairStrandsEnable && !bHasRayTracedOverlay)
		{
			RenderHairPrePass(GraphBuilder, Scene, Views, InstanceCullingManager);
			RenderHairBasePass(GraphBuilder, Scene, SceneTextures, Views, InstanceCullingManager);
		}

		RenderForwardShadowProjections(GraphBuilder, SceneTextures, ForwardScreenSpaceShadowMaskTexture, ForwardScreenSpaceShadowMaskHairTexture);

		// With forward shading we need to render volumetric fog before the base pass
		ComputeVolumetricFog(GraphBuilder, SceneTextures);
	}

	ExternalAccessQueue.Submit(GraphBuilder);

	FDBufferTextures DBufferTextures = CreateDBufferTextures(GraphBuilder, SceneTextures.Config.Extent, ShaderPlatform);

	{
		RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, DeferredShadingSceneRenderer_DBuffer);
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_DBuffer);
		CompositionLighting.ProcessBeforeBasePass(GraphBuilder, DBufferTextures);
	}
	
	if (IsForwardShadingEnabled(ShaderPlatform) && bAllowStaticLighting)
	{
		RenderIndirectCapsuleShadows(GraphBuilder, SceneTextures);
	}

	FTranslucencyLightingVolumeTextures TranslucencyLightingVolumeTextures;

	if (bRenderDeferredLighting && GbEnableAsyncComputeTranslucencyLightingVolumeClear && GSupportsEfficientAsyncCompute)
	{
		TranslucencyLightingVolumeTextures.Init(GraphBuilder, Views, ERDGPassFlags::AsyncCompute);
	}

	FRDGBufferRef DynamicGeometryScratchBuffer = nullptr;
#if RHI_RAYTRACING
	// Async AS builds can potentially overlap with BasePass.
	bool bNeedToWaitForRayTracingScene = DispatchRayTracingWorldUpdates(GraphBuilder, DynamicGeometryScratchBuffer);

	/** Should be called somewhere before "WaitForRayTracingScene" */
	SetupRayTracingLightDataForViews(GraphBuilder);
#endif

	if (!bHasRayTracedOverlay)
	{
#if RHI_RAYTRACING
		// Lumen scene lighting requires ray tracing scene to be ready if HWRT shadows are desired
		if (bNeedToWaitForRayTracingScene && Lumen::UseHardwareRayTracedSceneLighting(ViewFamily))
		{
			WaitForRayTracingScene(GraphBuilder, DynamicGeometryScratchBuffer);
			bNeedToWaitForRayTracingScene = false;
		}
#endif

		LLM_SCOPE_BYTAG(Lumen);
		BeginGatheringLumenSurfaceCacheFeedback(GraphBuilder, Views[0], LumenFrameTemporaries);
		RenderLumenSceneLighting(GraphBuilder, LumenFrameTemporaries);
	}

	{
		RenderBasePass(GraphBuilder, SceneTextures, DBufferTextures, BasePassDepthStencilAccess, ForwardScreenSpaceShadowMaskTexture, InstanceCullingManager, bNaniteEnabled, NaniteRasterResults);
		GraphBuilder.AddDispatchHint();

		if (!bAllowReadOnlyDepthBasePass)
		{
			AddResolveSceneDepthPass(GraphBuilder, Views, SceneTextures.Depth);
		}

		if (bNaniteEnabled)
		{
			if (GNaniteShowStats != 0)
			{
				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					const FViewInfo& View = Views[ViewIndex];
					if (IStereoRendering::IsAPrimaryView(View))
					{
						Nanite::PrintStats(GraphBuilder, View);
					}
				}
			}

			if (bVisualizeNanite)
			{
				FNanitePickingFeedback PickingFeedback = { 0 };

				Nanite::AddVisualizationPasses(
					GraphBuilder,
					Scene,
					SceneTextures,
					ViewFamily.EngineShowFlags,
					Views,
					NaniteRasterResults,
					PickingFeedback
				);

				OnGetOnScreenMessages.AddLambda([this, PickingFeedback, ScenePtr = Scene](FScreenMessageWriter& ScreenMessageWriter)->void
				{
					Nanite::DisplayPicking(ScenePtr, PickingFeedback, ScreenMessageWriter);
				});
			}
		}

		// VisualizeVirtualShadowMap TODO
	}

	// Extract emissive from SceneColor (before lighting is applied) + material diffuse and subsurface colors
	FRDGTextureRef ExposureIlluminanceSetup = AddSetupExposureIlluminancePass(GraphBuilder, Views, SceneTextures);

	if (ViewFamily.EngineShowFlags.VisualizeLightCulling)
	{
		FRDGTextureRef VisualizeLightCullingTexture = GraphBuilder.CreateTexture(SceneTextures.Color.Target->Desc, TEXT("SceneColorVisualizeLightCulling"));
		AddClearRenderTargetPass(GraphBuilder, VisualizeLightCullingTexture, FLinearColor::Transparent);
		SceneTextures.Color.Target = VisualizeLightCullingTexture;

		// When not in MSAA, assign to both targets.
		if (SceneTexturesConfig.NumSamples == 1)
		{
			SceneTextures.Color.Resolve = SceneTextures.Color.Target;
		}
	}

	if (bUseGBuffer)
	{
		// mark GBufferA for saving for next frame if it's needed
		ExtractNormalsForNextFrameReprojection(GraphBuilder, SceneTextures, Views);
	}

	// Rebuild scene textures to include GBuffers.
	SceneTextures.SetupMode |= ESceneTextureSetupMode::GBuffers;
	if (bShouldRenderVelocities && (bBasePassCanOutputVelocity || Scene->EarlyZPassMode == DDM_AllOpaqueNoVelocity))
	{
		SceneTextures.SetupMode |= ESceneTextureSetupMode::SceneVelocity;
	}
	SceneTextures.UniformBuffer = CreateSceneTextureUniformBuffer(GraphBuilder, &SceneTextures, FeatureLevel, SceneTextures.SetupMode);

	if (bRealTimeSkyCaptureEnabled)
	{
		Scene->ValidateSkyLightRealTimeCapture(GraphBuilder, Views[0], SceneTextures.Color.Target);
	}

	VisualizeVolumetricLightmap(GraphBuilder, SceneTextures);

	// Occlusion after base pass
	if (!bOcclusionBeforeBasePass)
	{
		RenderOcclusionLambda();
	}

	// End occlusion after base

	if (!bUseGBuffer)
	{
		AddResolveSceneColorPass(GraphBuilder, Views, SceneTextures.Color);
	}

	// Render hair
	if (bHairStrandsEnable && !IsForwardShadingEnabled(ShaderPlatform) && !bHasRayTracedOverlay)
	{
		RenderHairPrePass(GraphBuilder, Scene, Views, InstanceCullingManager);
		RenderHairBasePass(GraphBuilder, Scene, SceneTextures, Views, InstanceCullingManager);
	}

	// Post base pass for material classification
	// This needs to run before virtual shadow map, in order to have ready&cleared classified SSS data
	if (Strata::IsStrataEnabled())
	{
		Strata::AddStrataMaterialClassificationPass(GraphBuilder, SceneTextures, DBufferTextures, Views);
		Strata::AddStrataDBufferPass(GraphBuilder, SceneTextures, DBufferTextures, Views);
	}

	// Copy lighting channels out of stencil before deferred decals which overwrite those values
	TArray<FRDGTextureRef, TInlineAllocator<2>> NaniteMaterialResolve;
	if (bNaniteEnabled && Views.Num() > 0)
	{
		check(Views.Num() == NaniteRasterResults.Num());
		for (const Nanite::FRasterResults& Results : NaniteRasterResults)
		{
			NaniteMaterialResolve.Add(Results.MaterialResolve);
		}
	}
	FRDGTextureRef LightingChannelsTexture = CopyStencilToLightingChannelTexture(GraphBuilder, SceneTextures.Stencil, NaniteMaterialResolve);

	// Single layer water depth prepass. Needs to run before VSM page allocation.
	const FSingleLayerWaterPrePassResult* SingleLayerWaterPrePassResult = nullptr;

	const bool bShouldRenderSingleLayerWaterDepthPrepass = !bHasRayTracedOverlay && ShouldRenderSingleLayerWaterDepthPrepass(Views);
	if (bShouldRenderSingleLayerWaterDepthPrepass)
	{
		SingleLayerWaterPrePassResult = RenderSingleLayerWaterDepthPrepass(GraphBuilder, SceneTextures);
	}

	FAsyncLumenIndirectLightingOutputs AsyncLumenIndirectLightingOutputs;
	const bool bHasLumenLights = SortedLightSet.LumenLightStart < SortedLightSet.SortedLights.Num();

	GraphBuilder.FlushSetupQueue();

	// Shadows, lumen and fog after base pass
	if (!bHasRayTracedOverlay)
	{
#if RHI_RAYTRACING
		// When Lumen HWRT is running async we need to wait for ray tracing scene before dispatching the work
		if (bNeedToWaitForRayTracingScene && Lumen::UseAsyncCompute(ViewFamily) && Lumen::UseHardwareInlineRayTracing(ViewFamily))
		{
			WaitForRayTracingScene(GraphBuilder, DynamicGeometryScratchBuffer);
			bNeedToWaitForRayTracingScene = false;
		}
#endif // RHI_RAYTRACING

		DispatchAsyncLumenIndirectLightingWork(
			GraphBuilder,
			CompositionLighting,
			SceneTextures,
			LumenFrameTemporaries,
			LightingChannelsTexture,
			bHasLumenLights,
			AsyncLumenIndirectLightingOutputs);

		// If forward shading is enabled, we rendered shadow maps earlier already
		if (!IsForwardShadingEnabled(ShaderPlatform))
		{
			if (VirtualShadowMapArray.IsEnabled())
			{
				// TODO: actually move this inside RenderShadowDepthMaps instead of this extra scope to make it 1:1 with profiling captures/traces
				RDG_GPU_STAT_SCOPE(GraphBuilder, ShadowDepths);

				ensureMsgf(AreLightsInLightGrid(), TEXT("Virtual shadow map setup requires local lights to be injected into the light grid (this may be caused by 'r.LightCulling.Quality=0')."));
				FFrontLayerTranslucencyData FrontLayerTranslucencyData = RenderFrontLayerTranslucency(GraphBuilder, Views, SceneTextures, true /*VSM page marking*/);
				VirtualShadowMapArray.BuildPageAllocations(GraphBuilder, SceneTextures, Views, ViewFamily.EngineShowFlags, SortedLightSet, VisibleLightInfos, NaniteRasterResults, SingleLayerWaterPrePassResult, FrontLayerTranslucencyData);
			}

			RenderShadowDepthMaps(GraphBuilder, InstanceCullingManager, ExternalAccessQueue);
		}
		CheckShadowDepthRenderCompleted();

#if RHI_RAYTRACING
		// Lumen scene lighting requires ray tracing scene to be ready if HWRT shadows are desired
		if (bNeedToWaitForRayTracingScene && Lumen::UseHardwareRayTracedSceneLighting(ViewFamily))
		{
			WaitForRayTracingScene(GraphBuilder, DynamicGeometryScratchBuffer);
			bNeedToWaitForRayTracingScene = false;
		}
#endif // RHI_RAYTRACING
	}

	ExternalAccessQueue.Submit(GraphBuilder);

	// End shadow and fog after base pass

	// Trigger a command submit here, to avoid GPU bubbles
	AddDispatchToRHIThreadPass(GraphBuilder);
	
	if (bNaniteEnabled)
	{
		// Needs doing after shadows such that the checks for shadow atlases etc work.
		Nanite::ListStatFilters(this);
	}

	if (bUpdateNaniteStreaming)
	{
		Nanite::GStreamingManager.SubmitFrameStreamingRequests(GraphBuilder);
	}

	{
		FVirtualShadowMapArrayCacheManager* CacheManager = VirtualShadowMapArray.CacheManager;
		if (CacheManager)
		{
			// Do this even if VSMs are disabled this frame to clean up any previously extracted data
			CacheManager->ExtractFrameData(
				GraphBuilder,
				VirtualShadowMapArray,
				*this,
				ViewFamily.EngineShowFlags.VirtualShadowMapCaching);
		}
	}

	// If not all depth is written during the prepass, kick off async compute cloud after basepass
	if (bShouldRenderVolumetricCloud && bAsyncComputeVolumetricCloud && DepthPass.EarlyZPassMode != DDM_AllOpaque && !bHasRayTracedOverlay)
	{
		HalfResolutionDepthCheckerboardMinMaxTexture = CreateHalfResolutionDepthCheckerboardMinMax(GraphBuilder, Views, SceneTextures.Depth.Resolve);
		QuarterResolutionDepthMinMaxTexture = GenerateQuarterResDepthMinMaxTexture(GraphBuilder, Views, HalfResolutionDepthCheckerboardMinMaxTexture);

		bool bSkipVolumetricRenderTarget = false;
		bool bSkipPerPixelTracing = true;
		bAsyncComputeVolumetricCloud = RenderVolumetricCloud(GraphBuilder, SceneTextures, bSkipVolumetricRenderTarget, bSkipPerPixelTracing, 
			HalfResolutionDepthCheckerboardMinMaxTexture, QuarterResolutionDepthMinMaxTexture, true, InstanceCullingManager);
	}

	if (CustomDepthPassLocation == ECustomDepthPassLocation::AfterBasePass)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_CustomDepthPass_AfterBasePass);
		if (RenderCustomDepthPass(GraphBuilder, SceneTextures.CustomDepth, SceneTextures.GetSceneTextureShaderParameters(FeatureLevel), NaniteRasterResults, NaniteViews, GNaniteProgrammableRasterPrimary != 0))
		{
			SceneTextures.SetupMode |= ESceneTextureSetupMode::CustomDepth;
			SceneTextures.UniformBuffer = CreateSceneTextureUniformBuffer(GraphBuilder, &SceneTextures, FeatureLevel, SceneTextures.SetupMode);
		}
	}

	// If we are not rendering velocities in depth or base pass then do that here.
	if (bShouldRenderVelocities && !bBasePassCanOutputVelocity && (Scene->EarlyZPassMode != DDM_AllOpaqueNoVelocity))
	{
		GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLM_Velocity));
		RenderVelocities(GraphBuilder, SceneTextures, EVelocityPass::Opaque, bHairStrandsEnable);
		GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLM_AfterVelocity));
	}

	// Pre-lighting composition lighting stage
	// e.g. deferred decals, SSAO
	{
		RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, AfterBasePass);
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_AfterBasePass);

		if (!IsForwardShadingEnabled(ShaderPlatform))
		{
			AddResolveSceneDepthPass(GraphBuilder, Views, SceneTextures.Depth);
		}

		const FCompositionLighting::EProcessAfterBasePassMode Mode = AsyncLumenIndirectLightingOutputs.bHasDrawnBeforeLightingDecals ?
			FCompositionLighting::EProcessAfterBasePassMode::SkipBeforeLightingDecals : FCompositionLighting::EProcessAfterBasePassMode::All;

		CompositionLighting.ProcessAfterBasePass(GraphBuilder, Mode);
	}

	// Rebuild scene textures to include velocity, custom depth, and SSAO.
	SceneTextures.SetupMode |= ESceneTextureSetupMode::All;
	SceneTextures.UniformBuffer = CreateSceneTextureUniformBuffer(GraphBuilder, &SceneTextures, FeatureLevel, SceneTextures.SetupMode);

	if (!IsForwardShadingEnabled(ShaderPlatform))
	{
		// Clear stencil to 0 now that deferred decals are done using what was setup in the base pass.
		AddClearStencilPass(GraphBuilder, SceneTextures.Depth.Target);
	}

#if RHI_RAYTRACING
	// If Lumen did not force an earlier ray tracing scene sync, we must wait for it here.
	if (bNeedToWaitForRayTracingScene)
	{
		WaitForRayTracingScene(GraphBuilder, DynamicGeometryScratchBuffer);
		bNeedToWaitForRayTracingScene = false;
	}
#endif // RHI_RAYTRACING

	GraphBuilder.FlushSetupQueue();

	if (bRenderDeferredLighting)
	{
		RDG_GPU_STAT_SCOPE(GraphBuilder, RenderDeferredLighting);
		RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, RenderLighting);
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_Lighting);
		SCOPED_NAMED_EVENT(RenderLighting, FColor::Emerald);

		FRDGTextureRef DynamicBentNormalAOTexture = nullptr;

		RenderDiffuseIndirectAndAmbientOcclusion(
			GraphBuilder,
			SceneTextures,
			LumenFrameTemporaries,
			LightingChannelsTexture,
			bHasLumenLights,
			/* bCompositeRegularLumenOnly = */ false,
			/* bIsVisualizePass = */ false,
			AsyncLumenIndirectLightingOutputs);

		// These modulate the scenecolor output from the basepass, which is assumed to be indirect lighting
		if (bAllowStaticLighting)
		{
			RenderIndirectCapsuleShadows(GraphBuilder, SceneTextures);
		}

		// These modulate the scene color output from the base pass, which is assumed to be indirect lighting
		RenderDFAOAsIndirectShadowing(GraphBuilder, SceneTextures, DynamicBentNormalAOTexture);

		// Clear the translucent lighting volumes before we accumulate
		if ((GbEnableAsyncComputeTranslucencyLightingVolumeClear && GSupportsEfficientAsyncCompute) == false)
		{
			TranslucencyLightingVolumeTextures.Init(GraphBuilder, Views, ERDGPassFlags::Compute);
		}

#if RHI_RAYTRACING
		if (IsRayTracingEnabled())
		{
			RenderDitheredLODFadingOutMask(GraphBuilder, Views[0], SceneTextures.Depth.Target);
		}
#endif

		GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLM_Lighting));
		RenderLights(GraphBuilder, SceneTextures, TranslucencyLightingVolumeTextures, LightingChannelsTexture, SortedLightSet);
		GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLM_AfterLighting));

		InjectTranslucencyLightingVolumeAmbientCubemap(GraphBuilder, Views, TranslucencyLightingVolumeTextures);
		FilterTranslucencyLightingVolume(GraphBuilder, Views, TranslucencyLightingVolumeTextures);

		// Do DiffuseIndirectComposite after Lights so that async Lumen work can overlap
		RenderDiffuseIndirectAndAmbientOcclusion(
			GraphBuilder,
			SceneTextures,
			LumenFrameTemporaries,
			LightingChannelsTexture,
			bHasLumenLights,
			/* bCompositeRegularLumenOnly = */ true,
			/* bIsVisualizePass = */ false,
			AsyncLumenIndirectLightingOutputs);

		// Render diffuse sky lighting and reflections that only operate on opaque pixels
		RenderDeferredReflectionsAndSkyLighting(GraphBuilder, SceneTextures, DynamicBentNormalAOTexture);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Renders debug visualizations for global illumination plugins
		RenderGlobalIlluminationPluginVisualizations(GraphBuilder, LightingChannelsTexture);
#endif

		AddSubsurfacePass(GraphBuilder, SceneTextures, Views);

		Strata::AddStrataOpaqueRoughRefractionPasses(GraphBuilder, SceneTextures, Views);

		{
			RenderHairStrandsSceneColorScattering(GraphBuilder, SceneTextures.Color.Target, Scene, Views);
		}

	#if RHI_RAYTRACING
		if (ShouldRenderRayTracingSkyLight(Scene->SkyLight) 
			//@todo - integrate RenderRayTracingSkyLight into RenderDiffuseIndirectAndAmbientOcclusion
			&& GetViewPipelineState(Views[0]).DiffuseIndirectMethod != EDiffuseIndirectMethod::Lumen
			&& ViewFamily.EngineShowFlags.GlobalIllumination)
		{
			FRDGTextureRef SkyLightTexture = nullptr;
			FRDGTextureRef SkyLightHitDistanceTexture = nullptr;
			RenderRayTracingSkyLight(GraphBuilder, SceneTextures.Color.Target, SkyLightTexture, SkyLightHitDistanceTexture);
			CompositeRayTracingSkyLight(GraphBuilder, SceneTextures, SkyLightTexture, SkyLightHitDistanceTexture);
		}
	#endif
	}
	else if (HairStrands::HasViewHairStrandsData(Views) && ViewFamily.EngineShowFlags.Lighting)
	{
		RenderLightsForHair(GraphBuilder, SceneTextures, SortedLightSet, ForwardScreenSpaceShadowMaskHairTexture, LightingChannelsTexture);
		RenderDeferredReflectionsAndSkyLightingHair(GraphBuilder);
	}

	// Volumetric fog after Lumen GI and shadow depths
	if (!IsForwardShadingEnabled(ShaderPlatform) && !bHasRayTracedOverlay)
	{
		ComputeVolumetricFog(GraphBuilder, SceneTextures);
	}

	if (ShouldRenderHeterogeneousVolumes(Scene) && !bHasRayTracedOverlay)
	{
		RenderHeterogeneousVolumes(GraphBuilder, SceneTextures);
	}

	GraphBuilder.FlushSetupQueue();

	if (bShouldRenderVolumetricCloud && IsVolumetricRenderTargetEnabled() && HalfResolutionDepthCheckerboardMinMaxTexture==nullptr && !bHasRayTracedOverlay)
	{
		HalfResolutionDepthCheckerboardMinMaxTexture = CreateHalfResolutionDepthCheckerboardMinMax(GraphBuilder, Views, SceneTextures.Depth.Resolve);
		QuarterResolutionDepthMinMaxTexture = GenerateQuarterResDepthMinMaxTexture(GraphBuilder, Views, HalfResolutionDepthCheckerboardMinMaxTexture);
	}

	if (bShouldRenderVolumetricCloud && !bHasRayTracedOverlay)
	{
		if (!bAsyncComputeVolumetricCloud)
		{
			// Generate the volumetric cloud render target
			bool bSkipVolumetricRenderTarget = false;
			bool bSkipPerPixelTracing = true;
			RenderVolumetricCloud(GraphBuilder, SceneTextures, bSkipVolumetricRenderTarget, bSkipPerPixelTracing, 
				HalfResolutionDepthCheckerboardMinMaxTexture, QuarterResolutionDepthMinMaxTexture, false, InstanceCullingManager);
		}
		// Reconstruct the volumetric cloud render target to be ready to compose it over the scene
		ReconstructVolumetricRenderTarget(GraphBuilder, Views, SceneTextures.Depth.Resolve, HalfResolutionDepthCheckerboardMinMaxTexture, bAsyncComputeVolumetricCloud);
	}

	const bool bShouldRenderTranslucency = !bHasRayTracedOverlay && ShouldRenderTranslucency();

	// Union of all translucency view render flags.
	ETranslucencyView TranslucencyViewsToRender = bShouldRenderTranslucency ? GetTranslucencyViews(Views) : ETranslucencyView::None;

	FTranslucencyPassResourcesMap TranslucencyResourceMap(Views.Num());

	const bool bShouldRenderSingleLayerWater = !bHasRayTracedOverlay && ShouldRenderSingleLayerWater(Views);
	FSceneWithoutWaterTextures SceneWithoutWaterTextures;
	if (bShouldRenderSingleLayerWater)
	{
		if (EnumHasAnyFlags(TranslucencyViewsToRender, ETranslucencyView::UnderWater))
		{
			RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, RenderTranslucency);
			SCOPED_NAMED_EVENT(RenderTranslucency, FColor::Emerald);
			SCOPE_CYCLE_COUNTER(STAT_TranslucencyDrawTime);
			GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLM_Translucency));
			const bool bStandardTranslucentCanRenderSeparate = false;
			RenderTranslucency(GraphBuilder, SceneTextures, TranslucencyLightingVolumeTextures, &TranslucencyResourceMap, ETranslucencyView::UnderWater, InstanceCullingManager, bStandardTranslucentCanRenderSeparate);
			EnumRemoveFlags(TranslucencyViewsToRender, ETranslucencyView::UnderWater);
		}

		GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLM_WaterPass));
		RenderSingleLayerWater(GraphBuilder, SceneTextures, SingleLayerWaterPrePassResult, bShouldRenderVolumetricCloud, SceneWithoutWaterTextures, LumenFrameTemporaries);

		// Replace main depth texture with the output of the SLW depth prepass which contains the scene + water.
		// Note: Stencil now has all water bits marked with 1. As long as no other passes after this point want to read the depth buffer,
		// a stencil clear should not be necessary here.
		if (SingleLayerWaterPrePassResult)
		{
			SceneTextures.Depth = SingleLayerWaterPrePassResult->DepthPrepassTexture;
		}
	}

	// Rebuild scene textures to include scene color.
	SceneTextures.UniformBuffer = CreateSceneTextureUniformBuffer(GraphBuilder, &SceneTextures, FeatureLevel, SceneTextures.SetupMode);

	FRDGTextureRef LightShaftOcclusionTexture = nullptr;

	// Draw Lightshafts
	if (!bHasRayTracedOverlay && ViewFamily.EngineShowFlags.LightShafts)
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_RenderLightShaftOcclusion);
		LightShaftOcclusionTexture = RenderLightShaftOcclusion(GraphBuilder, SceneTextures);
	}

	// Draw the sky atmosphere
	if (!bHasRayTracedOverlay && bShouldRenderSkyAtmosphere && !IsForwardShadingEnabled(ShaderPlatform))
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_RenderSkyAtmosphere);
		RenderSkyAtmosphere(GraphBuilder, SceneTextures);
	}

	// Draw fog.
	if (!bHasRayTracedOverlay && ShouldRenderFog(ViewFamily))
	{
		RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, RenderFog);
		SCOPED_NAMED_EVENT(RenderFog, FColor::Emerald);
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_RenderFog);
		RenderFog(GraphBuilder, SceneTextures, LightShaftOcclusionTexture);
	}

	// After the height fog, Draw volumetric clouds (having fog applied on them already) when using per pixel tracing,
	if (!bHasRayTracedOverlay && bShouldRenderVolumetricCloud)
	{
		bool bSkipVolumetricRenderTarget = true;
		bool bSkipPerPixelTracing = false;
		RenderVolumetricCloud(GraphBuilder, SceneTextures, bSkipVolumetricRenderTarget, bSkipPerPixelTracing, 
			HalfResolutionDepthCheckerboardMinMaxTexture, QuarterResolutionDepthMinMaxTexture, false, InstanceCullingManager);
	}

	// or composite the off screen buffer over the scene.
	if (bVolumetricRenderTargetRequired)
	{
		ComposeVolumetricRenderTargetOverScene(GraphBuilder, Views, SceneTextures.Color.Target, SceneTextures.Depth.Target, bShouldRenderSingleLayerWater, SceneWithoutWaterTextures, SceneTextures);
	}

	FRDGTextureRef ExposureIlluminance = AddCalculateExposureIlluminancePass(GraphBuilder, Views, SceneTextures, TranslucencyLightingVolumeTextures, ExposureIlluminanceSetup);

	RenderOpaqueFX(GraphBuilder, Views, FXSystem, SceneTextures.UniformBuffer);

	FRendererModule& RendererModule = static_cast<FRendererModule&>(GetRendererModule());
	RendererModule.RenderPostOpaqueExtensions(GraphBuilder, Views, SceneTextures);

	if (Scene->GPUScene.ExecuteDeferredGPUWritePass(GraphBuilder, Views, EGPUSceneGPUWritePass::PostOpaqueRendering))
	{
		InstanceCullingManager.BeginDeferredCulling(GraphBuilder, Scene->GPUScene);
	}

	if (GetHairStrandsComposition() == EHairStrandsCompositionType::BeforeTranslucent)
	{
		RDG_GPU_STAT_SCOPE(GraphBuilder, HairRendering);
		RenderHairComposition(GraphBuilder, Views, SceneTextures.Color.Target, SceneTextures.Depth.Target, SceneTextures.Velocity, TranslucencyResourceMap);
	}

	// Draw translucency.
	TArray<FScreenPassTexture> TSRMoireInputTextures;
	if (!bHasRayTracedOverlay && TranslucencyViewsToRender != ETranslucencyView::None)
	{
		RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, RenderTranslucency);
		SCOPED_NAMED_EVENT(RenderTranslucency, FColor::Emerald);
		SCOPE_CYCLE_COUNTER(STAT_TranslucencyDrawTime);

		RDG_EVENT_SCOPE(GraphBuilder, "Translucency");

		// Raytracing doesn't need the distortion effect.
		const bool bShouldRenderDistortion = TranslucencyViewsToRender != ETranslucencyView::RayTracing && ShouldRenderDistortion();

#if RHI_RAYTRACING
		if (EnumHasAnyFlags(TranslucencyViewsToRender, ETranslucencyView::RayTracing))
		{
			RenderRayTracingTranslucency(GraphBuilder, SceneTextures.Color);
			EnumRemoveFlags(TranslucencyViewsToRender, ETranslucencyView::RayTracing);
		}
#endif

		// Lumen/VSM translucent front layer
		FFrontLayerTranslucencyData FrontLayerTranslucencyData = RenderFrontLayerTranslucency(GraphBuilder, Views, SceneTextures, false /*VSM page marking*/);
		for (FViewInfo& View : Views)
		{
			if (GetViewPipelineState(View).ReflectionsMethod == EReflectionsMethod::Lumen)
			{
				RenderLumenFrontLayerTranslucencyReflections(GraphBuilder, View, SceneTextures, LumenFrameTemporaries, FrontLayerTranslucencyData);
			}
		}

		// Extract TSR's moire heuristic luminance before renderering translucency into the scene color.
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			FViewInfo& View = Views[ViewIndex];
			if (ITemporalUpscaler::GetMainTAAPassConfig(View) == EMainTAAPassConfig::TSR)
			{
				if (TSRMoireInputTextures.Num() == 0)
				{
					TSRMoireInputTextures.SetNum(Views.Num());
				}

				TSRMoireInputTextures[ViewIndex] = AddTSRComputeMoireLuma(GraphBuilder, View.ShaderMap, FScreenPassTexture(SceneTextures.Color.Target, View.ViewRect));
			}
		}

		// Sort objects' triangles
		for (FViewInfo& View : Views)
		{
			if (OIT::IsEnabled(EOITSortingType::SortedTriangles, View))
			{
				OIT::AddSortTrianglesPass(GraphBuilder, View, Scene->OITSceneData, FTriangleSortingOrder::BackToFront);
			}
		}

		{
			// Render all remaining translucency views.
			GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLM_Translucency));
			const bool bStandardTranslucentCanRenderSeparate = bShouldRenderDistortion; // It is only needed to render standard translucent as separate when there is distortion (non self distortion of transmittance/specular/etc.)
			RenderTranslucency(GraphBuilder, SceneTextures, TranslucencyLightingVolumeTextures, &TranslucencyResourceMap, TranslucencyViewsToRender, InstanceCullingManager, bStandardTranslucentCanRenderSeparate);
			TranslucencyViewsToRender = ETranslucencyView::None;
		}

		// Compose hair before velocity/distortion pass since these pass write depth value, 
		// and this would make the hair composition fails in this cases.
		if (GetHairStrandsComposition() == EHairStrandsCompositionType::AfterTranslucent)
		{
			RDG_GPU_STAT_SCOPE(GraphBuilder, HairRendering);
			RenderHairComposition(GraphBuilder, Views, SceneTextures.Color.Target, SceneTextures.Depth.Target, SceneTextures.Velocity, TranslucencyResourceMap);
		}

		if (bShouldRenderDistortion)
		{
			GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLM_Distortion));
			RenderDistortion(GraphBuilder, SceneTextures.Color.Target, SceneTextures.Depth.Target, SceneTextures.Velocity, TranslucencyResourceMap);
		}

		if (bShouldRenderVelocities)
		{
			const bool bRecreateSceneTextures = !HasBeenProduced(SceneTextures.Velocity);

			GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLM_TranslucentVelocity));
			RenderVelocities(GraphBuilder, SceneTextures, EVelocityPass::Translucent, false);

			if (bRecreateSceneTextures)
			{
				// Rebuild scene textures to include newly allocated velocity.
				SceneTextures.UniformBuffer = CreateSceneTextureUniformBuffer(GraphBuilder, &SceneTextures, FeatureLevel, SceneTextures.SetupMode);
			}
		}

		GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLM_AfterTranslucency));
	}
	else if (GetHairStrandsComposition() == EHairStrandsCompositionType::AfterTranslucent)
	{
		RDG_GPU_STAT_SCOPE(GraphBuilder, HairRendering);
		RenderHairComposition(GraphBuilder, Views, SceneTextures.Color.Target, SceneTextures.Depth.Target, SceneTextures.Velocity, TranslucencyResourceMap);
	}

#if !UE_BUILD_SHIPPING
	if (CVarForceBlackVelocityBuffer.GetValueOnRenderThread())
	{
		SceneTextures.Velocity = SystemTextures.Black;

		// Rebuild the scene texture uniform buffer to include black.
		SceneTextures.UniformBuffer = CreateSceneTextureUniformBuffer(GraphBuilder, &SceneTextures, FeatureLevel, SceneTextures.SetupMode);
	}
#endif

	{
		if (HairStrandsBookmarkParameters.HasInstances())
		{
			RenderHairStrandsDebugInfo(GraphBuilder, Scene, Views, HairStrandsBookmarkParameters.HairClusterData, SceneTextures.Color.Target, SceneTextures.Depth.Target);
		}
	}

	if (VirtualShadowMapArray.IsEnabled())
	{
		VirtualShadowMapArray.RenderDebugInfo(GraphBuilder, Views);
	}

	for (FViewInfo& View : Views)
	{
		ShadingEnergyConservation::Debug(GraphBuilder, View, SceneTextures);
	}

	Scene->ProcessAndRenderIlluminanceMeter(GraphBuilder, Views, SceneTextures.Color.Target);

	if (!bHasRayTracedOverlay && ViewFamily.EngineShowFlags.LightShafts)
	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_RenderLightShaftBloom);
		GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLM_LightShaftBloom));
		RenderLightShaftBloom(GraphBuilder, SceneTextures, /* inout */ TranslucencyResourceMap);
	}

	if (bUseVirtualTexturing)
	{
		RDG_GPU_STAT_SCOPE(GraphBuilder, VirtualTextureUpdate);
		VirtualTextureFeedbackEnd(GraphBuilder);
	}

#if RHI_RAYTRACING
	if (IsRayTracingEnabled())
	{
		// Path tracer requires the full ray tracing pipeline support, as well as specialized extra shaders.
		// Most of the ray tracing debug visualizations also require the full pipeline, but some support inline mode.
		
		if (ViewFamily.EngineShowFlags.PathTracing 
			&& FDataDrivenShaderPlatformInfo::GetSupportsPathTracing(Scene->GetShaderPlatform()))
		{
			for (const FViewInfo& View : Views)
			{
				RenderPathTracing(GraphBuilder, View, SceneTextures.UniformBuffer, SceneTextures.Color.Target);
			}
		}
		else if (ViewFamily.EngineShowFlags.RayTracingDebug)
		{
			for (const FViewInfo& View : Views)
			{
				FRayTracingPickingFeedback PickingFeedback = {};
				RenderRayTracingDebug(GraphBuilder, View, SceneTextures.Color.Target, PickingFeedback);

				OnGetOnScreenMessages.AddLambda([this, PickingFeedback](FScreenMessageWriter& ScreenMessageWriter)->void
					{
						RayTracingDisplayPicking(PickingFeedback, ScreenMessageWriter);
					});
			}
		}
	}
#endif

	RendererModule.RenderOverlayExtensions(GraphBuilder, Views, SceneTextures);

	if (ViewFamily.EngineShowFlags.PhysicsField && Scene->PhysicsField)
	{
		RenderPhysicsField(GraphBuilder, Views, Scene->PhysicsField, SceneTextures.Color.Target);
	}

	if (ViewFamily.EngineShowFlags.VisualizeDistanceFieldAO && ShouldRenderDistanceFieldLighting())
	{
		GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLM_RenderDistanceFieldLighting));

		// Use the skylight's max distance if there is one, to be consistent with DFAO shadowing on the skylight
		const float OcclusionMaxDistance = Scene->SkyLight && !Scene->SkyLight->bWantsStaticShadowing ? Scene->SkyLight->OcclusionMaxDistance : Scene->DefaultMaxDistanceFieldOcclusionDistance;
		FRDGTextureRef DummyOutput = nullptr;
		RenderDistanceFieldLighting(GraphBuilder, SceneTextures, FDistanceFieldAOParameters(OcclusionMaxDistance), DummyOutput, false, ViewFamily.EngineShowFlags.VisualizeDistanceFieldAO);
	}

	// Draw visualizations just before use to avoid target contamination
	if (ViewFamily.EngineShowFlags.VisualizeMeshDistanceFields || ViewFamily.EngineShowFlags.VisualizeGlobalDistanceField)
	{
		RenderMeshDistanceFieldVisualization(GraphBuilder, SceneTextures, FDistanceFieldAOParameters(Scene->DefaultMaxDistanceFieldOcclusionDistance));
	}

	if (bRenderDeferredLighting)
	{
		RenderLumenMiscVisualizations(GraphBuilder, SceneTextures, LumenFrameTemporaries);
		RenderDiffuseIndirectAndAmbientOcclusion(
			GraphBuilder,
			SceneTextures,
			LumenFrameTemporaries,
			LightingChannelsTexture,
			/* bHasLumenLights = */ false,
			/* bCompositeRegularLumenOnly = */ false,
			/* bIsVisualizePass = */ true,
			AsyncLumenIndirectLightingOutputs);
	}

	if (ViewFamily.EngineShowFlags.StationaryLightOverlap)
	{
		RenderStationaryLightOverlap(GraphBuilder, SceneTextures, LightingChannelsTexture);
	}

	if (ShouldRenderHeterogeneousVolumes(Scene) && !bHasRayTracedOverlay)
	{
		CompositeHeterogeneousVolumes(GraphBuilder, SceneTextures);
	}

	if (bShouldVisualizeVolumetricCloud && !bHasRayTracedOverlay)
	{
		RenderVolumetricCloud(GraphBuilder, SceneTextures, false, true, HalfResolutionDepthCheckerboardMinMaxTexture, QuarterResolutionDepthMinMaxTexture, false, InstanceCullingManager);
		ReconstructVolumetricRenderTarget(GraphBuilder, Views, SceneTextures.Depth.Resolve, HalfResolutionDepthCheckerboardMinMaxTexture, false);
		ComposeVolumetricRenderTargetOverSceneForVisualization(GraphBuilder, Views, SceneTextures.Color.Target, SceneTextures);
		RenderVolumetricCloud(GraphBuilder, SceneTextures, true, false, HalfResolutionDepthCheckerboardMinMaxTexture, QuarterResolutionDepthMinMaxTexture, false, InstanceCullingManager);
	}

	if (!bHasRayTracedOverlay)
	{
		AddSparseVolumeTextureViewerRenderPass(GraphBuilder, *this, SceneTextures);
	}

	// Resolve the scene color for post processing.
	AddResolveSceneColorPass(GraphBuilder, Views, SceneTextures.Color);

	RendererModule.RenderPostResolvedSceneColorExtension(GraphBuilder, SceneTextures);

	FRDGTextureRef ViewFamilyTexture = TryCreateViewFamilyTexture(GraphBuilder, ViewFamily);

	CopySceneCaptureComponentToTarget(GraphBuilder, SceneTextures, ViewFamilyTexture, ViewFamily, Views);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		const FViewInfo& View = Views[ViewIndex];

		if (((View.FinalPostProcessSettings.DynamicGlobalIlluminationMethod == EDynamicGlobalIlluminationMethod::ScreenSpace && ScreenSpaceRayTracing::ShouldKeepBleedFreeSceneColor(View))
			|| GetViewPipelineState(View).DiffuseIndirectMethod == EDiffuseIndirectMethod::Lumen
			|| GetViewPipelineState(View).ReflectionsMethod == EReflectionsMethod::Lumen)
			&& !View.bStatePrevViewInfoIsReadOnly)
		{
			// Keep scene color and depth for next frame screen space ray tracing.
			FSceneViewState* ViewState = View.ViewState;
			GraphBuilder.QueueTextureExtraction(SceneTextures.Depth.Resolve, &ViewState->PrevFrameViewInfo.DepthBuffer);
			GraphBuilder.QueueTextureExtraction(SceneTextures.Color.Resolve, &ViewState->PrevFrameViewInfo.ScreenSpaceRayTracingInput);
		}
	}

	// Finish rendering for each view.
	if (ViewFamily.bResolveScene && ViewFamilyTexture)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "PostProcessing");
		RDG_GPU_STAT_SCOPE(GraphBuilder, Postprocessing);
		SCOPED_NAMED_EVENT(PostProcessing, FColor::Emerald);

		GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLM_PostProcessing));

		FPostProcessingInputs PostProcessingInputs;
		PostProcessingInputs.ViewFamilyTexture = ViewFamilyTexture;
		PostProcessingInputs.CustomDepthTexture = SceneTextures.CustomDepth.Depth;
		PostProcessingInputs.ExposureIlluminance = ExposureIlluminance;
		PostProcessingInputs.SceneTextures = SceneTextures.UniformBuffer;
		PostProcessingInputs.bSeparateCustomStencil = SceneTextures.CustomDepth.bSeparateStencilBuffer;

		GraphBuilder.FlushSetupQueue();

		if (ViewFamily.UseDebugViewPS())
		{
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				const FViewInfo& View = Views[ViewIndex];
   				const Nanite::FRasterResults* NaniteResults = bNaniteEnabled ? &NaniteRasterResults[ViewIndex] : nullptr;
				RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
				RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View%d", ViewIndex);
				PostProcessingInputs.TranslucencyViewResourcesMap = FTranslucencyViewResourcesMap(TranslucencyResourceMap, ViewIndex);
				AddDebugViewPostProcessingPasses(GraphBuilder, View, PostProcessingInputs, NaniteResults);
			}
		}
		else
		{
			for (int32 ViewExt = 0; ViewExt < ViewFamily.ViewExtensions.Num(); ++ViewExt)
			{
				for (int32 ViewIndex = 0; ViewIndex < ViewFamily.Views.Num(); ++ViewIndex)
				{
					FViewInfo& View = Views[ViewIndex];
					RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
					PostProcessingInputs.TranslucencyViewResourcesMap = FTranslucencyViewResourcesMap(TranslucencyResourceMap, ViewIndex);
					ViewFamily.ViewExtensions[ViewExt]->PrePostProcessPass_RenderThread(GraphBuilder, View, PostProcessingInputs);
				}
			}
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				const FViewInfo& View = Views[ViewIndex];
				const Nanite::FRasterResults* NaniteResults = bNaniteEnabled ? &NaniteRasterResults[ViewIndex] : nullptr;
				RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
				RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View%d", ViewIndex);

				PostProcessingInputs.TranslucencyViewResourcesMap = FTranslucencyViewResourcesMap(TranslucencyResourceMap, ViewIndex);

				if (IsPostProcessVisualizeCalibrationMaterialEnabled(View))
				{
					const UMaterialInterface* DebugMaterialInterface = GetPostProcessVisualizeCalibrationMaterialInterface(View);
					check(DebugMaterialInterface);

					AddVisualizeCalibrationMaterialPostProcessingPasses(GraphBuilder, View, PostProcessingInputs, DebugMaterialInterface);
				}
				else
				{
					const FPerViewPipelineState& ViewPipelineState = GetViewPipelineState(View);
					const bool bAnyLumenActive = ViewPipelineState.DiffuseIndirectMethod == EDiffuseIndirectMethod::Lumen || ViewPipelineState.ReflectionsMethod == EReflectionsMethod::Lumen;

					FScreenPassTexture TSRMoireInput;
					if (ViewIndex < TSRMoireInputTextures.Num())
					{
						TSRMoireInput = TSRMoireInputTextures[ViewIndex];
					}

					AddPostProcessingPasses(
						GraphBuilder,
						View, ViewIndex,
						bAnyLumenActive,
						ViewPipelineState.ReflectionsMethod,
						PostProcessingInputs,
						NaniteResults,
						InstanceCullingManager,
						&VirtualShadowMapArray,
						LumenFrameTemporaries,
						SceneWithoutWaterTextures,
						TSRMoireInput);
				}
			}
		}
	}

	// After AddPostProcessingPasses in case of Lumen Visualizations writing to feedback
	FinishGatheringLumenSurfaceCacheFeedback(GraphBuilder, Views[0], LumenFrameTemporaries);

	if (ViewFamily.bResolveScene && ViewFamilyTexture)
	{
		GVRSImageManager.DrawDebugPreview(GraphBuilder, ViewFamily, ViewFamilyTexture);
	}

	GEngine->GetPostRenderDelegateEx().Broadcast(GraphBuilder);

#if RHI_RAYTRACING
	ReleaseRaytracingResources(GraphBuilder, Views, Scene->RayTracingScene, bIsLastSceneRenderer);
#endif //  RHI_RAYTRACING

#if WITH_MGPU
	DoCrossGPUTransfers(GraphBuilder, ViewFamilyTexture);
#endif

	{
		SCOPE_CYCLE_COUNTER(STAT_FDeferredShadingSceneRenderer_RenderFinish);
		RDG_GPU_STAT_SCOPE(GraphBuilder, FrameRenderFinish);
		GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLM_RenderFinish));
		RenderFinish(GraphBuilder, ViewFamilyTexture);
		GraphBuilder.SetCommandListStat(GET_STATID(STAT_CLM_AfterFrame));
		GraphBuilder.AddDispatchHint();
		GraphBuilder.FlushSetupQueue();
	}

	QueueSceneTextureExtractions(GraphBuilder, SceneTextures);

	::Strata::PostRender(*Scene);

	// Release the view's previous frame histories so that their memory can be reused at the graph's execution.
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		Views[ViewIndex].PrevViewInfo = FPreviousViewInfo();
	}
}

#if RHI_RAYTRACING

bool AnyRayTracingPassEnabled(const FScene* Scene, const FViewInfo& View)
{
	if (!IsRayTracingEnabled() || Scene == nullptr)
	{
		return false;
	}

	return ShouldRenderRayTracingAmbientOcclusion(View)
		|| ShouldRenderRayTracingReflections(View)
		|| ShouldRenderRayTracingGlobalIllumination(View)
		|| ShouldRenderRayTracingTranslucency(View)
		|| ShouldRenderRayTracingSkyLight(Scene->SkyLight)
		|| ShouldRenderRayTracingShadows()
		|| Scene->bHasRayTracedLights
		|| ShouldRenderPluginRayTracingGlobalIllumination(View)
        || Lumen::AnyLumenHardwareRayTracingPassEnabled(Scene, View)
		|| ShouldRenderRayTracingReflectionsWater(View)
		|| HasRayTracedOverlay(*View.Family);
}

bool ShouldRenderRayTracingEffect(bool bEffectEnabled, ERayTracingPipelineCompatibilityFlags CompatibilityFlags, const FSceneView* View)
{
	if (!IsRayTracingEnabled() || (View && !View->bAllowRayTracing))
	{
		return false;
	}

	const bool bAllowPipeline = GRHISupportsRayTracingShaders && 
								CVarRayTracingAllowPipeline.GetValueOnRenderThread() &&
								EnumHasAnyFlags(CompatibilityFlags, ERayTracingPipelineCompatibilityFlags::FullPipeline);

	const bool bAllowInline = GRHISupportsInlineRayTracing && 
							  CVarRayTracingAllowInline.GetValueOnRenderThread() &&
							  EnumHasAnyFlags(CompatibilityFlags, ERayTracingPipelineCompatibilityFlags::Inline);

	// Disable the effect if current machine does not support the full ray tracing pipeline and the effect can't fall back to inline mode or vice versa.
	if (!bAllowPipeline && !bAllowInline)
	{
		return false;
	}

	const int32 OverrideMode = CVarForceAllRayTracingEffects.GetValueOnRenderThread();

	if (OverrideMode >= 0)
	{
		return OverrideMode > 0;
	}
	else
	{
		return bEffectEnabled;
	}
}

bool HasRayTracedOverlay(const FSceneViewFamily& ViewFamily)
{
	// Return true if a full screen ray tracing pass will be displayed on top of the raster pass
	// This can be used to skip certain calculations
	return
		ViewFamily.EngineShowFlags.PathTracing ||
		ViewFamily.EngineShowFlags.RayTracingDebug;
}

void FDeferredShadingSceneRenderer::InitializeRayTracingFlags_RenderThread()
{
	// The result of this call is used by AnyRayTracingPassEnabled to decide if we have any RT shadows enabled
	Scene->UpdateRayTracedLights();

	// This function may be called twice -- once in CreateSceneRenderers and again in Render.  We deliberately skip the logic
	// if the flag is already set, because CreateSceneRenderers fills in the correct value for "bShouldUpdateRayTracingScene"
	// in that case, and we don't want to overwrite it.
	if (!bAnyRayTracingPassEnabled && !ViewFamily.EngineShowFlags.HitProxies)
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			bool bHasRayTracing = AnyRayTracingPassEnabled(Scene, Views[ViewIndex]);

			Views[ViewIndex].bHasAnyRayTracingPass = bHasRayTracing;

			bAnyRayTracingPassEnabled |= bHasRayTracing;
		}

		bShouldUpdateRayTracingScene = bAnyRayTracingPassEnabled;
	}
}
#endif // RHI_RAYTRACING
