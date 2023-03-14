// Copyright Epic Games, Inc. All Rights Reserved.

#include "LumenSceneRendering.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "MeshPassProcessor.inl"
#include "MeshCardRepresentation.h"
#include "GPUScene.h"
#include "Rendering/NaniteResources.h"
#include "Nanite/Nanite.h"
#include "NaniteSceneProxy.h"
#include "PixelShaderUtils.h"
#include "Lumen.h"
#include "LumenMeshCards.h"
#include "LumenSurfaceCacheFeedback.h"
#include "LumenSceneLighting.h"
#include "LumenTracingUtils.h"
#include "GlobalDistanceField.h"
#include "DistanceFieldAmbientOcclusion.h"
#include "HAL/LowLevelMemStats.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

int32 GLumenSupported = 1;
FAutoConsoleVariableRef CVarLumenSupported(
	TEXT("r.Lumen.Supported"),
	GLumenSupported,
	TEXT("Whether Lumen is supported at all for the project, regardless of platform.  This can be used to avoid compiling shaders and other load time overhead."),
	ECVF_ReadOnly
);

int32 GLumenFastCameraMode = 0;
FAutoConsoleVariableRef CVarLumenFastCameraMode(
	TEXT("r.LumenScene.FastCameraMode"),
	GLumenFastCameraMode,
	TEXT("Whether to update the Lumen Scene for fast camera movement - lower quality, faster updates so lighting can keep up with the camera."),
	ECVF_RenderThreadSafe
);

int32 GLumenSceneParallelUpdate = 1;
FAutoConsoleVariableRef CVarLumenSceneParallelUpdate(
	TEXT("r.LumenScene.ParallelUpdate"),
	GLumenSceneParallelUpdate,
	TEXT("Whether to run the Lumen Scene update in parallel."),
	ECVF_RenderThreadSafe
);

int32 GLumenScenePrimitivesPerTask = 128;
FAutoConsoleVariableRef CVarLumenScenePrimitivePerTask(
	TEXT("r.LumenScene.PrimitivesPerTask"),
	GLumenScenePrimitivesPerTask,
	TEXT("How many primitives to process per single surface cache update task."),
	ECVF_RenderThreadSafe
);

int32 GLumenSceneMeshCardsPerTask = 128;
FAutoConsoleVariableRef CVarLumenSceneMeshCardsPerTask(
	TEXT("r.LumenScene.MeshCardsPerTask"),
	GLumenSceneMeshCardsPerTask,
	TEXT("How many mesh cards to process per single surface cache update task."),
	ECVF_RenderThreadSafe
);

int32 GLumenGIMaxConeSteps = 1000;
FAutoConsoleVariableRef CVarLumenGIMaxConeSteps(
	TEXT("r.Lumen.MaxConeSteps"),
	GLumenGIMaxConeSteps,
	TEXT("Maximum steps to use for Cone Stepping of proxy cards."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenSurfaceCacheFreeze = 0;
FAutoConsoleVariableRef CVarLumenSceneSurfaceCacheFreeze(
	TEXT("r.LumenScene.SurfaceCache.Freeze"),
	GLumenSurfaceCacheFreeze,
	TEXT("Freeze surface cache updates for debugging.\n"),
	ECVF_RenderThreadSafe
);

int32 GLumenSurfaceCacheFreezeUpdateFrame = 0;
FAutoConsoleVariableRef CVarLumenSceneSurfaceCacheFreezeUpdateFrame(
	TEXT("r.LumenScene.SurfaceCache.FreezeUpdateFrame"),
	GLumenSurfaceCacheFreezeUpdateFrame,
	TEXT("Keep updating the same subset of surface cache for debugging and profiling.\n"),
	ECVF_RenderThreadSafe
);

int32 GLumenSceneSurfaceCacheReset = 0;
FAutoConsoleVariableRef CVarLumenSceneSurfaceCacheReset(
	TEXT("r.LumenScene.SurfaceCache.Reset"),
	GLumenSceneSurfaceCacheReset,
	TEXT("Reset all atlases and captured cards.\n"),	
	ECVF_RenderThreadSafe
);

int32 GLumenSceneSurfaceCacheResetEveryNthFrame = 0;
FAutoConsoleVariableRef CVarLumenSceneSurfaceCacheResetEveryNthFrame(
	TEXT("r.LumenScene.SurfaceCache.ResetEveryNthFrame"),
	GLumenSceneSurfaceCacheResetEveryNthFrame,
	TEXT("Continuously reset all atlases and captured cards every N-th frame.\n"),
	ECVF_RenderThreadSafe
);

int32 GLumenSceneCardCapturesPerFrame = 300;
FAutoConsoleVariableRef CVarLumenSceneCardCapturesPerFrame(
	TEXT("r.LumenScene.SurfaceCache.CardCapturesPerFrame"),
	GLumenSceneCardCapturesPerFrame,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenSceneCardCaptureFactor = 64;
FAutoConsoleVariableRef CVarLumenSceneCardCaptureFactor(
	TEXT("r.LumenScene.SurfaceCache.CardCaptureFactor"),
	GLumenSceneCardCaptureFactor,
	TEXT("Controls how many texels can be captured per frame. Texels = SurfaceCacheTexels / Factor."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarLumenSceneCardCaptureRefreshFraction(
	TEXT("r.LumenScene.SurfaceCache.CardCaptureRefreshFraction"),
	0.125f,
	TEXT("Fraction of card capture budget allowed to be spent on re-capturing existing pages in order to refresh surface cache materials.\n")
	TEXT("0 disables card refresh."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarLumenSceneCardCaptureEnableInvalidation(
	TEXT("r.LumenScene.SurfaceCache.CardCaptureEnableInvalidation"),
	1,
	TEXT("Whether to enable manual card recapture through InvalidateSurfaceCacheForPrimitive().\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenSceneCardCaptureMargin = 0.0f;
FAutoConsoleVariableRef CVarLumenSceneCardCaptureMargin(
	TEXT("r.LumenScene.SurfaceCache.CardCaptureMargin"),
	GLumenSceneCardCaptureMargin,
	TEXT("How far from Lumen scene range start to capture cards."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenSceneCardFixedDebugResolution = -1;
FAutoConsoleVariableRef CVarLumenSceneCardFixedDebugResolution(
	TEXT("r.LumenScene.SurfaceCache.CardFixedDebugResolution"),
	GLumenSceneCardFixedDebugResolution,
	TEXT("Lumen card resolution"),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

float GLumenSceneCardCameraDistanceTexelDensityScale = 100;
FAutoConsoleVariableRef CVarLumenSceneCardCameraDistanceTexelDensityScale(
	TEXT("r.LumenScene.SurfaceCache.CardCameraDistanceTexelDensityScale"),
	GLumenSceneCardCameraDistanceTexelDensityScale,
	TEXT("Lumen card texels per world space distance"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenSceneCardMaxTexelDensity = .2f;
FAutoConsoleVariableRef CVarLumenSceneCardMaxTexelDensity(
	TEXT("r.LumenScene.SurfaceCache.CardMaxTexelDensity"),
	GLumenSceneCardMaxTexelDensity,
	TEXT("Lumen card texels per world space distance"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenSceneCardMinResolution = 4;
FAutoConsoleVariableRef CVarLumenSceneCardMinResolution(
	TEXT("r.LumenScene.SurfaceCache.CardMinResolution"),
	GLumenSceneCardMinResolution,
	TEXT("Minimum mesh card size resolution to be visible in Lumen Scene"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenSceneCardMaxResolution = 512;
FAutoConsoleVariableRef CVarLumenSceneCardMaxResolution(
	TEXT("r.LumenScene.SurfaceCache.CardMaxResolution"),
	GLumenSceneCardMaxResolution,
	TEXT("Maximum card resolution in Lumen Scene"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GSurfaceCacheNumFramesToKeepUnusedPages = 256;
FAutoConsoleVariableRef CVarLumenSceneSurfaceCacheNumFramesToKeepUnusedPages(
	TEXT("r.LumenScene.SurfaceCache.NumFramesToKeepUnusedPages"),
	GSurfaceCacheNumFramesToKeepUnusedPages,
	TEXT("Num frames to keep unused pages in surface cache."),
	ECVF_RenderThreadSafe
);

int32 GLumenSceneForceEvictHiResPages = 0;
FAutoConsoleVariableRef CVarLumenSceneForceEvictHiResPages(
	TEXT("r.LumenScene.SurfaceCache.ForceEvictHiResPages"),
	GLumenSceneForceEvictHiResPages,
	TEXT("Evict all optional hi-res surface cache pages."),
	ECVF_RenderThreadSafe
);

int32 GLumenSceneRecaptureLumenSceneEveryFrame = 0;
FAutoConsoleVariableRef CVarLumenGIRecaptureLumenSceneEveryFrame(
	TEXT("r.LumenScene.SurfaceCache.RecaptureEveryFrame"),
	GLumenSceneRecaptureLumenSceneEveryFrame,
	TEXT(""),
	ECVF_RenderThreadSafe
);

int32 GLumenSceneGlobalDFResolution = 252;
FAutoConsoleVariableRef CVarLumenSceneGlobalDFResolution(
	TEXT("r.LumenScene.GlobalSDF.Resolution"),
	GLumenSceneGlobalDFResolution,
	TEXT("Global Distance Field resolution when Lumen is enabled."),
	ECVF_RenderThreadSafe
);

float GLumenSceneGlobalDFClipmapExtent = 2500.0f;
FAutoConsoleVariableRef CVarLumenSceneGlobalDFClipmapExtent(
	TEXT("r.LumenScene.GlobalSDF.ClipmapExtent"),
	GLumenSceneGlobalDFClipmapExtent,
	TEXT("Global Distance Field first clipmap extent when Lumen is enabled."),
	ECVF_RenderThreadSafe
);

float GLumenSceneFarFieldTexelDensity = 0.001f;
FAutoConsoleVariableRef CVarLumenSceneFarFieldTexelDensity(
	TEXT("r.LumenScene.SurfaceCache.FarField.TexelDensity"),
	GLumenSceneFarFieldTexelDensity,
	TEXT("Far Field Lumen card texels per world space unit"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenSceneFarFieldDistance = 40000.00f;
FAutoConsoleVariableRef CVarLumenSceneFarFieldDistance(
	TEXT("r.LumenScene.SurfaceCache.FarField.Distance"),
	GLumenSceneFarFieldDistance,
	TEXT("Far Field Lumen card culling distance"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenSceneSurfaceCacheLogUpdates = 0;
FAutoConsoleVariableRef CVarLumenSceneSurfaceCacheLogUpdates(
	TEXT("r.LumenScene.SurfaceCache.LogUpdates"),
	GLumenSceneSurfaceCacheLogUpdates,
	TEXT("Whether to log Lumen surface cache updates.\n")
	TEXT("2 - will log mesh names."),
	ECVF_RenderThreadSafe
);

int32 GLumenSceneSurfaceCacheResampleLighting = 1;
FAutoConsoleVariableRef CVarLumenSceneSurfaceCacheResampleLighting(
	TEXT("r.LumenScene.SurfaceCache.ResampleLighting"),
	GLumenSceneSurfaceCacheResampleLighting,
	TEXT("Whether to resample card lighting when cards are reallocated.  This is needed for Radiosity temporal accumulation but can be disabled for debugging."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> GLumenSceneSurfaceCacheCaptureMeshTargetScreenSize(
	TEXT("r.LumenScene.SurfaceCache.Capture.MeshTargetScreenSize"),
	0.15f,
	TEXT("Controls which LOD level will be used to capture static meshes into surface cache."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
		{
			Lumen::DebugResetSurfaceCache();
		}),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> GLumenSceneSurfaceCacheCaptureNaniteLODScaleFactor(
	TEXT("r.LumenScene.SurfaceCache.Capture.NaniteLODScaleFactor"),
	1.0f,
	TEXT("Controls which LOD level will be used to capture Nanite meshes into surface cache."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
		{
			Lumen::DebugResetSurfaceCache();
		}),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarLumenSceneSurfaceCacheCaptureNaniteMultiView(
	TEXT("r.LumenScene.SurfaceCache.Capture.NaniteMultiView"),
	1,
	TEXT("Toggle multi view Lumen Nanite Card capture for debugging."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
		{
			Lumen::DebugResetSurfaceCache();
		}),
	ECVF_RenderThreadSafe);

static int32 GNaniteProgrammableRasterLumen = 0; // TODO: Not working properly in all cases yet
static FAutoConsoleVariableRef CNaniteProgrammableRasterLumen(
	TEXT("r.Nanite.ProgrammableRaster.Lumen"),
	GNaniteProgrammableRasterLumen,
	TEXT("A toggle that allows Nanite programmable raster in Lumen passes.\n")
	TEXT(" 0: Programmable raster is disabled\n")
	TEXT(" 1: Programmable raster is enabled (default)"),
	ECVF_RenderThreadSafe);

#if ENABLE_LOW_LEVEL_MEM_TRACKER
DECLARE_LLM_MEMORY_STAT(TEXT("Lumen"), STAT_LumenLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("Lumen"), STAT_LumenSummaryLLM, STATGROUP_LLM);
LLM_DEFINE_TAG(Lumen, NAME_None, NAME_None, GET_STATFNAME(STAT_LumenLLM), GET_STATFNAME(STAT_LumenSummaryLLM));
#endif // ENABLE_LOW_LEVEL_MEM_TRACKER

extern int32 GAllowLumenDiffuseIndirect;
extern int32 GAllowLumenReflections;

namespace LumenSurfaceCache
{
	int32 GetMinCardResolution()
	{
		 return FMath::Clamp(GLumenSceneCardMinResolution, 1, 1024);
	}
};

namespace LumenLandscape
{
	constexpr int32 CardCaptureLOD = 0;
};

void Lumen::DebugResetSurfaceCache()
{
	GLumenSceneSurfaceCacheReset = 1;
}

bool Lumen::IsSurfaceCacheFrozen()
{
	return GLumenSurfaceCacheFreeze != 0;
}

bool Lumen::IsSurfaceCacheUpdateFrameFrozen()
{
	return GLumenSurfaceCacheFreeze != 0 || GLumenSurfaceCacheFreezeUpdateFrame != 0;
}

namespace Lumen
{
	bool AnyLumenHardwareRayTracingPassEnabled(const FScene* Scene, const FViewInfo& View)
	{
#if RHI_RAYTRACING

		const bool bLumenGI = ShouldRenderLumenDiffuseGI(Scene, View);
		const bool bLumenReflections = ShouldRenderLumenReflections(View);

		if (bLumenGI
			&& (UseHardwareRayTracedScreenProbeGather(*View.Family) || UseHardwareRayTracedRadianceCache(*View.Family) || UseHardwareRayTracedDirectLighting(*View.Family)))
		{
			return true;
		}

		if (bLumenReflections
			&& UseHardwareRayTracedReflections(*View.Family))
		{
			return true;
		}

		if ((bLumenGI || bLumenReflections) && Lumen::ShouldVisualizeHardwareRayTracing(*View.Family))
		{
			return true;
		}

		if ((bLumenGI || bLumenReflections) && Lumen::ShouldRenderRadiosityHardwareRayTracing(*View.Family))
		{
			return true;
		}
#endif
		return false;
	}

	bool AnyLumenHardwareInlineRayTracingPassEnabled(const FScene* Scene, const FViewInfo& View)
	{
		if (!AnyLumenHardwareRayTracingPassEnabled(Scene, View))
		{
			return false;
		}

		return Lumen::UseHardwareInlineRayTracing(*View.Family);
	}
}

bool Lumen::ShouldHandleSkyLight(const FScene* Scene, const FSceneViewFamily& ViewFamily)
{
	return Scene->SkyLight
		&& (Scene->SkyLight->ProcessedTexture || Scene->SkyLight->bRealTimeCaptureEnabled)
		&& ViewFamily.EngineShowFlags.SkyLighting
		&& Scene->GetFeatureLevel() >= ERHIFeatureLevel::SM5
		&& !IsForwardShadingEnabled(Scene->GetShaderPlatform())
		&& !ViewFamily.EngineShowFlags.VisualizeLightCulling;
}

bool DoesRuntimePlatformSupportLumen()
{
	return RHIIsTypedUAVLoadSupported(PF_R16_UINT);
}

bool ShouldRenderLumenForViewFamily(const FScene* Scene, const FSceneViewFamily& ViewFamily, bool bSkipProjectCheck)
{
	return Scene
		&& Scene->GetLumenSceneData(*ViewFamily.Views[0])
		&& ViewFamily.Views.Num() <= MaxLumenViews
		&& DoesPlatformSupportLumenGI(Scene->GetShaderPlatform(), bSkipProjectCheck);
}

bool Lumen::IsSoftwareRayTracingSupported()
{
	return DoesProjectSupportDistanceFields();
}

bool Lumen::IsLumenFeatureAllowedForView(const FScene* Scene, const FSceneView& View, bool bSkipTracingDataCheck, bool bSkipProjectCheck)
{
	return View.Family
		&& DoesRuntimePlatformSupportLumen()
		&& ShouldRenderLumenForViewFamily(Scene, *View.Family, bSkipProjectCheck)
		// Don't update scene lighting for secondary views
		&& !View.bIsPlanarReflection
		&& !View.bIsSceneCaptureCube
		&& !View.bIsReflectionCapture
		&& View.State
		&& (bSkipTracingDataCheck || Lumen::UseHardwareRayTracing(*View.Family) || IsSoftwareRayTracingSupported());
}

bool Lumen::UseGlobalSDFObjectGrid(const FSceneViewFamily& ViewFamily)
{
	if (!Lumen::IsSoftwareRayTracingSupported())
	{
		return false;
	}

	// All features use Hardware RayTracing, no need to update voxel lighting
	if (Lumen::UseHardwareRayTracedSceneLighting(ViewFamily)
		&& Lumen::UseHardwareRayTracedScreenProbeGather(ViewFamily)
		&& Lumen::UseHardwareRayTracedReflections(ViewFamily)
		&& Lumen::UseHardwareRayTracedRadianceCache(ViewFamily)
		&& Lumen::UseHardwareRayTracedTranslucencyVolume(ViewFamily)
		&& Lumen::UseHardwareRayTracedVisualize(ViewFamily))
	{
		return false;
	}

	return true;
}

int32 Lumen::GetGlobalDFResolution()
{
	return GLumenSceneGlobalDFResolution;
}

float Lumen::GetGlobalDFClipmapExtent(int32 ClipmapIndex)
{
	return GLumenSceneGlobalDFClipmapExtent * FMath::Pow(2.0f, ClipmapIndex);
}

int32 Lumen::GetNumGlobalDFClipmaps(const FSceneView& View)
{
	return GlobalDistanceField::GetNumGlobalDistanceFieldClipmaps(/*bLumenEnabled*/ true, View.FinalPostProcessSettings.LumenSceneViewDistance);
}

float GetCardCameraDistanceTexelDensityScale()
{
	return GLumenSceneCardCameraDistanceTexelDensityScale * (GLumenFastCameraMode ? .2f : 1.0f);
}

int32 GetCardMaxResolution()
{
	if (GLumenFastCameraMode)
	{
		return GLumenSceneCardMaxResolution / 2;
	}

	return GLumenSceneCardMaxResolution;
}

int32 GetMaxLumenSceneCardCapturesPerFrame()
{
	return FMath::Max(GLumenSceneCardCapturesPerFrame * (GLumenFastCameraMode ? 2 : 1), 0);
}

int32 GetMaxMeshCardsToAddPerFrame()
{
	return 2 * GetMaxLumenSceneCardCapturesPerFrame();
}

int32 GetMaxTileCapturesPerFrame()
{
	if (Lumen::IsSurfaceCacheFrozen())
	{
		return 0;
	}

	if (GLumenSceneRecaptureLumenSceneEveryFrame != 0)
	{
		return INT32_MAX;
	}

	return GetMaxLumenSceneCardCapturesPerFrame();
}

uint32 FLumenSceneData::GetSurfaceCacheUpdateFrameIndex() const
{
	return SurfaceCacheUpdateFrameIndex;
}

void FLumenSceneData::IncrementSurfaceCacheUpdateFrameIndex()
{
	if (!Lumen::IsSurfaceCacheUpdateFrameFrozen())
	{
		++SurfaceCacheUpdateFrameIndex;
		if (SurfaceCacheUpdateFrameIndex == 0)
		{
			++SurfaceCacheUpdateFrameIndex;
		}
	}
}

DECLARE_GPU_STAT(LumenSceneUpdate);
DECLARE_GPU_STAT(UpdateLumenSceneBuffers);

IMPLEMENT_STATIC_UNIFORM_BUFFER_STRUCT(FLumenCardPassUniformParameters, "LumenCardPass", SceneTextures);

class FLumenCardVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FLumenCardVS, MeshMaterial);

protected:

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		//@todo DynamicGI - filter
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	FLumenCardVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{}

	FLumenCardVS() = default;
};


IMPLEMENT_MATERIAL_SHADER_TYPE(, FLumenCardVS, TEXT("/Engine/Private/Lumen/LumenCardVertexShader.usf"), TEXT("Main"), SF_Vertex);

template<bool bMultiViewCapture>
class FLumenCardPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FLumenCardPS, MeshMaterial);

public:
	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		if (Parameters.VertexFactoryType->SupportsNaniteRendering() != bMultiViewCapture)
		{
			return false;
		}

		//@todo DynamicGI - filter
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	FLumenCardPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{}

	FLumenCardPS() = default;

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("LUMEN_MULTI_VIEW_CAPTURE"), bMultiViewCapture);
		OutEnvironment.SetDefine(TEXT("STRATA_INLINE_SHADING"), 1);
	}
};

void SetupCardCaptureRenderTargetsInfo(FGraphicsPipelineRenderTargetsInfo& RenderTargetsInfo)
{
	RenderTargetsInfo.NumSamples = 1;
	RenderTargetsInfo.RenderTargetsEnabled = 3;

	// Albedo
	RenderTargetsInfo.RenderTargetFormats[0] = PF_R8G8B8A8;
	RenderTargetsInfo.RenderTargetFlags[0] = TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_NoFastClear;

	// Normal
	RenderTargetsInfo.RenderTargetFormats[1] = PF_R8G8B8A8;
	RenderTargetsInfo.RenderTargetFlags[1] = TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_NoFastClear;

	// Emissive
	RenderTargetsInfo.RenderTargetFormats[2] = PF_FloatR11G11B10;
	RenderTargetsInfo.RenderTargetFlags[2] = TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_NoFastClear;

	// Setup depth stencil state
	RenderTargetsInfo.DepthStencilTargetFormat = PF_DepthStencil;
	RenderTargetsInfo.DepthStencilTargetFlag = TexCreate_ShaderResource | TexCreate_DepthStencilTargetable | TexCreate_NoFastClear;

	// See setup of FDeferredShadingSceneRenderer::UpdateLumenScene (needs to be shared)
	RenderTargetsInfo.DepthTargetLoadAction = ERenderTargetLoadAction::ELoad;
	RenderTargetsInfo.StencilTargetLoadAction = ERenderTargetLoadAction::ENoAction;
	RenderTargetsInfo.DepthStencilAccess = FExclusiveDepthStencil::DepthWrite_StencilNop;

	// Derive store actions
	const ERenderTargetStoreAction StoreAction = EnumHasAnyFlags(RenderTargetsInfo.DepthStencilTargetFlag, TexCreate_Memoryless) ? ERenderTargetStoreAction::ENoAction : ERenderTargetStoreAction::EStore;
	RenderTargetsInfo.DepthTargetStoreAction = RenderTargetsInfo.DepthStencilAccess.IsUsingDepth() ? StoreAction : ERenderTargetStoreAction::ENoAction;
	RenderTargetsInfo.StencilTargetStoreAction = RenderTargetsInfo.DepthStencilAccess.IsUsingStencil() ? StoreAction : ERenderTargetStoreAction::ENoAction;
}

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FLumenCardPS<false>, TEXT("/Engine/Private/Lumen/LumenCardPixelShader.usf"), TEXT("Main"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FLumenCardPS<true>, TEXT("/Engine/Private/Lumen/LumenCardPixelShader.usf"), TEXT("Main"), SF_Pixel);

class FLumenCardMeshProcessor : public FSceneRenderingAllocatorObject<FLumenCardMeshProcessor>, public FMeshPassProcessor
{
public:

	FLumenCardMeshProcessor(const FScene* Scene, ERHIFeatureLevel::Type FeatureLevel, const FSceneView* InViewIfDynamicMeshCommand, const FMeshPassProcessorRenderState& InPassDrawRenderState, FMeshPassDrawListContext* InDrawListContext);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final;
	virtual void CollectPSOInitializers(const FSceneTexturesConfig& SceneTexturesConfig, const FMaterial& Material, const FVertexFactoryType* VertexFactoryType, const FPSOPrecacheParams& PreCacheParams, TArray<FPSOPrecacheData>& PSOInitializers) override final;

	FMeshPassProcessorRenderState PassDrawRenderState;
};

bool GetLumenCardShaders(
	const FMaterial& Material,
	const FVertexFactoryType* VertexFactoryType,
	TShaderRef<FLumenCardVS>& VertexShader,
	TShaderRef<FLumenCardPS<false>>& PixelShader)
{
	FMaterialShaderTypes ShaderTypes;
	ShaderTypes.AddShaderType<FLumenCardVS>();
	ShaderTypes.AddShaderType<FLumenCardPS<false>>();

	FMaterialShaders Shaders;
	if (!Material.TryGetShaders(ShaderTypes, VertexFactoryType, Shaders))
	{
		return false;
	}

	Shaders.TryGetVertexShader(VertexShader);
	Shaders.TryGetPixelShader(PixelShader);
	return true;
}

void FLumenCardMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{
	LLM_SCOPE_BYTAG(Lumen);

	if (MeshBatch.bUseForMaterial
		&& DoesPlatformSupportLumenGI(GetFeatureLevelShaderPlatform(FeatureLevel))
		&& (PrimitiveSceneProxy && PrimitiveSceneProxy->ShouldRenderInMainPass() && PrimitiveSceneProxy->AffectsDynamicIndirectLighting()))
	{
		const FMaterialRenderProxy* MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
		while (MaterialRenderProxy)
		{
			const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);
			if (Material)
			{
				auto TryAddMeshBatch = [this](const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId, const FMaterialRenderProxy& MaterialRenderProxy, const FMaterial& Material) -> bool
				{
					const EBlendMode BlendMode = Material.GetBlendMode();
					const FMaterialShadingModelField ShadingModels = Material.GetShadingModels();
					const bool bIsTranslucent = IsTranslucentBlendMode(BlendMode);
					const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(MeshBatch);
					const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(Material, OverrideSettings);
					const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(Material, OverrideSettings);

					if (!bIsTranslucent
						&& ShouldIncludeDomainInMeshPass(Material.GetMaterialDomain()))
					{
						const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;
						FVertexFactoryType* VertexFactoryType = VertexFactory->GetType();
						constexpr bool bMultiViewCapture = false;

						TMeshProcessorShaders<
							FLumenCardVS,
							FLumenCardPS<bMultiViewCapture>> PassShaders;

						if (!GetLumenCardShaders(
							Material,
							VertexFactory->GetType(),
							PassShaders.VertexShader,
							PassShaders.PixelShader))
						{
							return false;
						}

						FMeshMaterialShaderElementData ShaderElementData;
						ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

						const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(PassShaders.VertexShader, PassShaders.PixelShader);

						BuildMeshDrawCommands(
							MeshBatch,
							BatchElementMask,
							PrimitiveSceneProxy,
							MaterialRenderProxy,
							Material,
							PassDrawRenderState,
							PassShaders,
							MeshFillMode,
							MeshCullMode,
							SortKey,
							EMeshPassFeatures::Default,
							ShaderElementData);
					}

					return true;
				};

				if (TryAddMeshBatch(MeshBatch, BatchElementMask, PrimitiveSceneProxy, StaticMeshId, *MaterialRenderProxy, *Material))
				{
					break;
				}
			};

			MaterialRenderProxy = MaterialRenderProxy->GetFallback(FeatureLevel);
		}
	}
}

void FLumenCardMeshProcessor::CollectPSOInitializers(const FSceneTexturesConfig& SceneTexturesConfig, const FMaterial& Material, const FVertexFactoryType* VertexFactoryType, const FPSOPrecacheParams& PreCacheParams, TArray<FPSOPrecacheData>& PSOInitializers)
{
	LLM_SCOPE_BYTAG(Lumen);

	if (!PreCacheParams.bRenderInMainPass || !PreCacheParams.bAffectDynamicIndirectLighting ||
		!DoesPlatformSupportLumenGI(GetFeatureLevelShaderPlatform(FeatureLevel)))
	{
		return;
	}

	const EBlendMode BlendMode = Material.GetBlendMode();
	const FMaterialShadingModelField ShadingModels = Material.GetShadingModels();
	const bool bIsTranslucent = IsTranslucentBlendMode(BlendMode);
	const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(PreCacheParams);
	const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(Material, OverrideSettings);
	const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(Material, OverrideSettings);

	if (!bIsTranslucent
		&& ShouldIncludeDomainInMeshPass(Material.GetMaterialDomain()))
	{
		constexpr bool bMultiViewCapture = false;

		TMeshProcessorShaders<
			FLumenCardVS,
			FLumenCardPS<bMultiViewCapture>> PassShaders;

		if (!GetLumenCardShaders(
			Material,
			VertexFactoryType,
			PassShaders.VertexShader,
			PassShaders.PixelShader))
		{
			return;
		}

		FGraphicsPipelineRenderTargetsInfo RenderTargetsInfo;
		SetupCardCaptureRenderTargetsInfo(RenderTargetsInfo);

		AddGraphicsPipelineStateInitializer(
			VertexFactoryType,
			Material,
			PassDrawRenderState,
			RenderTargetsInfo,
			PassShaders,
			MeshFillMode,
			MeshCullMode,
			(EPrimitiveType)PreCacheParams.PrimitiveType,
			EMeshPassFeatures::Default, 
			PSOInitializers);
	}
}

FLumenCardMeshProcessor::FLumenCardMeshProcessor(const FScene* Scene, ERHIFeatureLevel::Type FeatureLevel, const FSceneView* InViewIfDynamicMeshCommand, const FMeshPassProcessorRenderState& InPassDrawRenderState, FMeshPassDrawListContext* InDrawListContext)
	: FMeshPassProcessor(EMeshPass::LumenCardCapture, Scene, FeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext)
	, PassDrawRenderState(InPassDrawRenderState)
{}

FMeshPassProcessor* CreateLumenCardCapturePassProcessor(ERHIFeatureLevel::Type FeatureLevel, const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
{
	LLM_SCOPE_BYTAG(Lumen);

	FMeshPassProcessorRenderState PassState;

	// Write and test against depth
	PassState.SetDepthStencilState(TStaticDepthStencilState<true, CF_Greater>::GetRHI());

	PassState.SetBlendState(TStaticBlendState<>::GetRHI());

	return new FLumenCardMeshProcessor(Scene, FeatureLevel, InViewIfDynamicMeshCommand, PassState, InDrawListContext);
}

REGISTER_MESHPASSPROCESSOR_AND_PSOCOLLECTOR(LumenCardCapturePass, CreateLumenCardCapturePassProcessor, EShadingPath::Deferred, EMeshPass::LumenCardCapture, EMeshPassFlags::CachedMeshCommands);

class FLumenCardNaniteMeshProcessor : public FMeshPassProcessor
{
public:

	FLumenCardNaniteMeshProcessor(const FScene* Scene, ERHIFeatureLevel::Type FeatureLevel, const FSceneView* InViewIfDynamicMeshCommand, const FMeshPassProcessorRenderState& InPassDrawRenderState, FMeshPassDrawListContext* InDrawListContext);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final;
	virtual void CollectPSOInitializers(const FSceneTexturesConfig& SceneTexturesConfig, const FMaterial& Material, const FVertexFactoryType* VertexFactoryType, const FPSOPrecacheParams& PreCacheParams, TArray<FPSOPrecacheData>& PSOInitializers) override final;

	FMeshPassProcessorRenderState PassDrawRenderState;

private:
	bool TryAddMeshBatch(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		int32 StaticMeshId,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material);
};

FLumenCardNaniteMeshProcessor::FLumenCardNaniteMeshProcessor(
	const FScene* InScene,
	ERHIFeatureLevel::Type FeatureLevel,
	const FSceneView* InViewIfDynamicMeshCommand,
	const FMeshPassProcessorRenderState& InDrawRenderState,
	FMeshPassDrawListContext* InDrawListContext
) :
	FMeshPassProcessor(EMeshPass::LumenCardNanite, InScene, FeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext),
	PassDrawRenderState(InDrawRenderState)
{
}

using FLumenCardNanitePassShaders = TMeshProcessorShaders<FNaniteMultiViewMaterialVS, FLumenCardPS<true>>;

void FLumenCardNaniteMeshProcessor::AddMeshBatch(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	int32 StaticMeshId /*= -1 */
)
{
	LLM_SCOPE_BYTAG(Lumen);

	checkf(Lumen::HasPrimitiveNaniteMeshBatches(PrimitiveSceneProxy) && DoesPlatformSupportLumenGI(GetFeatureLevelShaderPlatform(FeatureLevel)),
		TEXT("Logic in BuildNaniteDrawCommands() should not have allowed an unqualifying mesh batch to be added"));

	const FMaterialRenderProxy* MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
	while (MaterialRenderProxy)
	{
		const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);
		if (Material)
		{
			if (TryAddMeshBatch(MeshBatch, BatchElementMask, PrimitiveSceneProxy, StaticMeshId, *MaterialRenderProxy, *Material))
			{
				break;
			}
		}

		MaterialRenderProxy = MaterialRenderProxy->GetFallback(FeatureLevel);
	}
}

bool FLumenCardNaniteMeshProcessor::TryAddMeshBatch(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	int32 StaticMeshId,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const FMaterial& Material)
{
	const EBlendMode BlendMode = Material.GetBlendMode();
	check(Nanite::IsSupportedBlendMode(BlendMode));
	check(Nanite::IsSupportedMaterialDomain(Material.GetMaterialDomain()));

	TShaderMapRef<FNaniteMultiViewMaterialVS> VertexShader(GetGlobalShaderMap(FeatureLevel));

	FLumenCardNanitePassShaders PassShaders;
	PassShaders.VertexShader = VertexShader;

	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;
	FVertexFactoryType* VertexFactoryType = VertexFactory->GetType();
	constexpr bool bMultiViewCapture = true;

	FMaterialShaderTypes ShaderTypes;
	ShaderTypes.AddShaderType<FLumenCardPS<bMultiViewCapture>>();

	FMaterialShaders Shaders;
	if (!Material.TryGetShaders(ShaderTypes, VertexFactoryType, Shaders))
	{
		return false;
	}

	Shaders.TryGetPixelShader(PassShaders.PixelShader);

	FMeshMaterialShaderElementData ShaderElementData;
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		Material,
		PassDrawRenderState,
		PassShaders,
		FM_Solid,
		CM_None,
		FMeshDrawCommandSortKey::Default,
		EMeshPassFeatures::Default,
		ShaderElementData
	);

	return true;
}

void FLumenCardNaniteMeshProcessor::CollectPSOInitializers(
	const FSceneTexturesConfig& SceneTexturesConfig,
	const FMaterial& Material, 
	const FVertexFactoryType* VertexFactoryType, 
	const FPSOPrecacheParams& PreCacheParams,
	TArray<FPSOPrecacheData>& PSOInitializers)
{
	// Make sure nanite rendering is supported
	if (!SupportsNaniteRendering(VertexFactoryType, Material, FeatureLevel))
	{
		return;
	}

	const EBlendMode BlendMode = Material.GetBlendMode();
	if (!Nanite::IsSupportedBlendMode(BlendMode) || Material.GetMaterialDomain())
	{
		return;
	}

	const ERasterizerFillMode MeshFillMode = FM_Solid;
	const ERasterizerCullMode MeshCullMode = CM_None;

	TShaderMapRef<FNaniteMultiViewMaterialVS> VertexShader(GetGlobalShaderMap(FeatureLevel));

	FLumenCardNanitePassShaders PassShaders;
	PassShaders.VertexShader = VertexShader;

	constexpr bool bMultiViewCapture = true;

	FMaterialShaderTypes ShaderTypes;
	ShaderTypes.AddShaderType<FLumenCardPS<bMultiViewCapture>>();

	FMaterialShaders Shaders;
	if (!Material.TryGetShaders(ShaderTypes, VertexFactoryType, Shaders))
	{
		return;
	}

	Shaders.TryGetPixelShader(PassShaders.PixelShader);

	FGraphicsPipelineRenderTargetsInfo RenderTargetsInfo;
	SetupCardCaptureRenderTargetsInfo(RenderTargetsInfo);

	AddGraphicsPipelineStateInitializer(
		VertexFactoryType,
		Material,
		PassDrawRenderState,
		RenderTargetsInfo,
		PassShaders,
		MeshFillMode,
		MeshCullMode,
		(EPrimitiveType)PreCacheParams.PrimitiveType,
		EMeshPassFeatures::Default,
		PSOInitializers);
}

FMeshPassProcessor* CreateLumenCardNaniteMeshProcessor(
	ERHIFeatureLevel::Type FeatureLevel,
	const FScene* Scene,
	const FSceneView* InViewIfDynamicMeshCommand,
	FMeshPassDrawListContext* InDrawListContext)
{
	LLM_SCOPE_BYTAG(Lumen);

	FMeshPassProcessorRenderState PassState;
	PassState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Equal, true, CF_Equal>::GetRHI());
	PassState.SetDepthStencilAccess(FExclusiveDepthStencil::DepthRead_StencilRead);
	PassState.SetStencilRef(STENCIL_SANDBOX_MASK);
	PassState.SetBlendState(TStaticBlendState<>::GetRHI());

	return new FLumenCardNaniteMeshProcessor(Scene, FeatureLevel, InViewIfDynamicMeshCommand, PassState, InDrawListContext);
}

REGISTER_MESHPASSPROCESSOR_AND_PSOCOLLECTOR(LumenCardNanitePass, CreateLumenCardNaniteMeshProcessor, EShadingPath::Deferred, EMeshPass::LumenCardNanite, EMeshPassFlags::None);

FCardPageRenderData::FCardPageRenderData(
	const FViewInfo& InMainView,
	const FLumenCard& InLumenCard,
	FVector4f InCardUVRect,
	FIntRect InCardCaptureAtlasRect,
	FIntRect InSurfaceCacheAtlasRect,
	int32 InPrimitiveGroupIndex,
	int32 InCardIndex,
	int32 InPageTableIndex,
	bool bInResampleLastLighting)
	: PrimitiveGroupIndex(InPrimitiveGroupIndex)
	, CardIndex(InCardIndex)
	, PageTableIndex(InPageTableIndex)
	, bDistantScene(InLumenCard.bDistantScene)
	, CardUVRect(InCardUVRect)
	, CardCaptureAtlasRect(InCardCaptureAtlasRect)
	, SurfaceCacheAtlasRect(InSurfaceCacheAtlasRect)
	, CardWorldOBB(InLumenCard.WorldOBB)
	, bResampleLastLighting(bInResampleLastLighting)
{
	ensure(CardIndex >= 0 && PageTableIndex >= 0);

	NaniteLODScaleFactor = GLumenSceneSurfaceCacheCaptureNaniteLODScaleFactor.GetValueOnRenderThread();

	if (InLumenCard.bDistantScene)
	{
		NaniteLODScaleFactor = Lumen::GetDistanceSceneNaniteLODScaleFactor();
	}

	UpdateViewMatrices(InMainView);
}

void FCardPageRenderData::UpdateViewMatrices(const FViewInfo& MainView)
{
	ensureMsgf(FVector3f::DotProduct(CardWorldOBB.AxisX, FVector3f::CrossProduct(CardWorldOBB.AxisY, CardWorldOBB.AxisZ)) < 0.0f, TEXT("Card has wrong handedness"));

	FMatrix ViewRotationMatrix = FMatrix::Identity;
	ViewRotationMatrix.SetColumn(0, (FVector)CardWorldOBB.AxisX);
	ViewRotationMatrix.SetColumn(1, (FVector)CardWorldOBB.AxisY);
	ViewRotationMatrix.SetColumn(2, (FVector)-CardWorldOBB.AxisZ);

	FVector ViewLocation(CardWorldOBB.Origin);
	FVector FaceLocalExtent(CardWorldOBB.Extent);
	// Pull the view location back so the entire box is in front of the near plane
	ViewLocation += FVector(FaceLocalExtent.Z * CardWorldOBB.AxisZ);

	const float NearPlane = 0.0f;
	const float FarPlane = FaceLocalExtent.Z * 2.0f;

	const float ZScale = 1.0f / (FarPlane - NearPlane);
	const float ZOffset = -NearPlane;

	const FVector4f ProjectionRect = FVector4f(2.0f, 2.0f, 2.0f, 2.0f) * CardUVRect - FVector4f(1.0f, 1.0f, 1.0f, 1.0f);

	const float ProjectionL = ProjectionRect.X * 0.5f * FaceLocalExtent.X;
	const float ProjectionR = ProjectionRect.Z * 0.5f * FaceLocalExtent.X;

	const float ProjectionB = -ProjectionRect.W * 0.5f * FaceLocalExtent.Y;
	const float ProjectionT = -ProjectionRect.Y * 0.5f * FaceLocalExtent.Y;

	const FMatrix ProjectionMatrix = FReversedZOrthoMatrix(
		ProjectionL,
		ProjectionR,
		ProjectionB,
		ProjectionT,
		ZScale,
		ZOffset);

	ProjectionMatrixUnadjustedForRHI = ProjectionMatrix;

	FViewMatrices::FMinimalInitializer Initializer;
	Initializer.ViewRotationMatrix = ViewRotationMatrix;
	Initializer.ViewOrigin = ViewLocation;
	Initializer.ProjectionMatrix = ProjectionMatrix;
	Initializer.ConstrainedViewRect = MainView.SceneViewInitOptions.GetConstrainedViewRect();
	Initializer.StereoPass = MainView.SceneViewInitOptions.StereoPass;
#if WITH_EDITOR
	Initializer.bUseFauxOrthoViewPos = MainView.SceneViewInitOptions.bUseFauxOrthoViewPos;
#endif

	ViewMatrices = FViewMatrices(Initializer);
}

void FCardPageRenderData::PatchView(const FScene* Scene, FViewInfo* View) const
{
	View->ProjectionMatrixUnadjustedForRHI = ProjectionMatrixUnadjustedForRHI;
	View->ViewMatrices = ViewMatrices;
	View->ViewRect = CardCaptureAtlasRect;

	FBox VolumeBounds[TVC_MAX];
	View->SetupUniformBufferParameters(
		VolumeBounds,
		TVC_MAX,
		*View->CachedViewUniformShaderParameters);

	View->CachedViewUniformShaderParameters->NearPlane = 0;
	View->CachedViewUniformShaderParameters->FarShadowStaticMeshLODBias = 0;
	View->CachedViewUniformShaderParameters->OverrideLandscapeLOD = LumenLandscape::CardCaptureLOD;
}

void AddCardCaptureDraws(const FScene* Scene,
	FCardPageRenderData& CardPageRenderData,
	const FLumenPrimitiveGroup& PrimitiveGroup,
	TConstArrayView<const FPrimitiveSceneInfo*> SceneInfoPrimitives,
	FMeshCommandOneFrameArray& VisibleMeshCommands,
	TArray<int32, SceneRenderingAllocator>& PrimitiveIds)
{
	LLM_SCOPE_BYTAG(Lumen);

	const EMeshPass::Type MeshPass = EMeshPass::LumenCardCapture;
	const ENaniteMeshPass::Type NaniteMeshPass = ENaniteMeshPass::LumenCardCapture;
	const FBox WorldSpaceCardBox = CardPageRenderData.CardWorldOBB.GetBox();

	uint32 MaxVisibleMeshDrawCommands = 0;
	for (const FPrimitiveSceneInfo* PrimitiveSceneInfo : SceneInfoPrimitives)
	{
		if (PrimitiveSceneInfo
			&& PrimitiveSceneInfo->Proxy->AffectsDynamicIndirectLighting()
			&& WorldSpaceCardBox.Intersect(PrimitiveSceneInfo->Proxy->GetBounds().GetBox())
			&& !PrimitiveSceneInfo->Proxy->IsNaniteMesh())
		{
			MaxVisibleMeshDrawCommands += PrimitiveSceneInfo->StaticMeshRelevances.Num();
		}
	}
	CardPageRenderData.InstanceRuns.Reserve(2 * MaxVisibleMeshDrawCommands);

	for (const FPrimitiveSceneInfo* PrimitiveSceneInfo : SceneInfoPrimitives)
	{
		if (PrimitiveSceneInfo
			&& PrimitiveSceneInfo->Proxy->AffectsDynamicIndirectLighting()
			&& WorldSpaceCardBox.Intersect(PrimitiveSceneInfo->Proxy->GetBounds().GetBox()))
		{
			if (PrimitiveSceneInfo->Proxy->IsNaniteMesh())
			{
				if (PrimitiveGroup.PrimitiveInstanceIndex >= 0)
				{
					CardPageRenderData.NaniteInstanceIds.Add(PrimitiveSceneInfo->GetInstanceSceneDataOffset() + PrimitiveGroup.PrimitiveInstanceIndex);
				}
				else
				{
					// Render all instances
					const int32 NumInstances = PrimitiveSceneInfo->GetNumInstanceSceneDataEntries();

					for (int32 InstanceIndex = 0; InstanceIndex < NumInstances; ++InstanceIndex)
					{
						CardPageRenderData.NaniteInstanceIds.Add(PrimitiveSceneInfo->GetInstanceSceneDataOffset() + InstanceIndex);
					}
				}

				for (const FNaniteCommandInfo& CommandInfo : PrimitiveSceneInfo->NaniteCommandInfos[NaniteMeshPass])
				{
					CardPageRenderData.NaniteCommandInfos.Add(CommandInfo);
				}
			}
			else
			{
				int32 LODToRender = 0;

				if (PrimitiveGroup.bHeightfield)
				{
					// Landscape can't use last LOD, as it's a single quad with only 4 distinct heightfield values
					// Also selected LOD needs to to match FLandscapeSectionLODUniformParameters uniform buffers
					LODToRender = LumenLandscape::CardCaptureLOD;
				}
				else
				{
					const float TargetScreenSize = GLumenSceneSurfaceCacheCaptureMeshTargetScreenSize.GetValueOnRenderThread();

					int32 PrevLODToRender = INT_MAX;
					int32 NextLODToRender = -1;
					for (int32 MeshIndex = 0; MeshIndex < PrimitiveSceneInfo->StaticMeshRelevances.Num(); ++MeshIndex)
					{
						const FStaticMeshBatchRelevance& Mesh = PrimitiveSceneInfo->StaticMeshRelevances[MeshIndex];
						if (Mesh.ScreenSize >= TargetScreenSize)
						{
							NextLODToRender = FMath::Max(NextLODToRender, (int32)Mesh.LODIndex);
						}
						else
						{
							PrevLODToRender = FMath::Min(PrevLODToRender, (int32)Mesh.LODIndex);
						}
					}

					LODToRender = NextLODToRender >= 0 ? NextLODToRender : PrevLODToRender;
					const int32 CurFirstLODIdx = (int32)PrimitiveSceneInfo->Proxy->GetCurrentFirstLODIdx_RenderThread();
					LODToRender = FMath::Max(LODToRender, CurFirstLODIdx);
				}

				FMeshDrawCommandPrimitiveIdInfo IdInfo(PrimitiveSceneInfo->GetIndex(), PrimitiveSceneInfo->GetInstanceSceneDataOffset());

				for (int32 MeshIndex = 0; MeshIndex < PrimitiveSceneInfo->StaticMeshRelevances.Num(); MeshIndex++)
				{
					const FStaticMeshBatchRelevance& StaticMeshRelevance = PrimitiveSceneInfo->StaticMeshRelevances[MeshIndex];
					const FStaticMeshBatch& StaticMesh = PrimitiveSceneInfo->StaticMeshes[MeshIndex];

					if (StaticMeshRelevance.bUseForMaterial && StaticMeshRelevance.LODIndex == LODToRender)
					{
						const int32 StaticMeshCommandInfoIndex = StaticMeshRelevance.GetStaticMeshCommandInfoIndex(MeshPass);
						if (StaticMeshCommandInfoIndex >= 0)
						{
							const FCachedMeshDrawCommandInfo& CachedMeshDrawCommand = PrimitiveSceneInfo->StaticMeshCommandInfos[StaticMeshCommandInfoIndex];
							const FCachedPassMeshDrawList& SceneDrawList = Scene->CachedDrawLists[MeshPass];

							const FMeshDrawCommand* MeshDrawCommand = nullptr;
							if (CachedMeshDrawCommand.StateBucketId >= 0)
							{
								MeshDrawCommand = &Scene->CachedMeshDrawCommandStateBuckets[MeshPass].GetByElementId(CachedMeshDrawCommand.StateBucketId).Key;
							}
							else
							{
								MeshDrawCommand = &SceneDrawList.MeshDrawCommands[CachedMeshDrawCommand.CommandIndex];
							}

							const uint32* InstanceRunArray = nullptr;
							uint32 NumInstanceRuns = 0;

							if (MeshDrawCommand->NumInstances > 1 && PrimitiveGroup.PrimitiveInstanceIndex >= 0)
							{
								// Render only a single specified instance, by specifying an inclusive [x;x] range

								ensure(CardPageRenderData.InstanceRuns.Num() + 2 <= CardPageRenderData.InstanceRuns.Max());
								InstanceRunArray = CardPageRenderData.InstanceRuns.GetData() + CardPageRenderData.InstanceRuns.Num();
								NumInstanceRuns = 1;

								CardPageRenderData.InstanceRuns.Add(PrimitiveGroup.PrimitiveInstanceIndex);
								CardPageRenderData.InstanceRuns.Add(PrimitiveGroup.PrimitiveInstanceIndex);
							}

							FVisibleMeshDrawCommand NewVisibleMeshDrawCommand;

							NewVisibleMeshDrawCommand.Setup(
								MeshDrawCommand,
								IdInfo,								
								CachedMeshDrawCommand.StateBucketId,
								CachedMeshDrawCommand.MeshFillMode,
								CachedMeshDrawCommand.MeshCullMode,
								CachedMeshDrawCommand.Flags,
								CachedMeshDrawCommand.SortKey,
								InstanceRunArray,
								NumInstanceRuns);

							VisibleMeshCommands.Add(NewVisibleMeshDrawCommand);
							PrimitiveIds.Add(PrimitiveSceneInfo->GetIndex());
						}
					}
				}
			}
		}
	}
}

struct FMeshCardsAdd
{
	int32 PrimitiveGroupIndex;
	float DistanceSquared;
};

struct FMeshCardsRemove
{
	int32 PrimitiveGroupIndex;
};

struct FCardAllocationOutput
{
	bool bVisible = false;
	int32 ResLevel = -1;
};

// Loop over Lumen primitives and output FMeshCards adds and removes
struct FLumenSurfaceCacheUpdatePrimitivesTask
{
public:
	FLumenSurfaceCacheUpdatePrimitivesTask(
		const TSparseSpanArray<FLumenPrimitiveGroup>& InPrimitiveGroups,
		const TArray<FVector, TInlineAllocator<2>>& InViewOrigins,
		float InLumenSceneDetail,
		float InMaxDistanceFromCamera,
		int32 InFirstPrimitiveGroupIndex,
		int32 InNumPrimitiveGroupsPerPacket)
		: PrimitiveGroups(InPrimitiveGroups)
		, ViewOrigins(InViewOrigins)
		, FirstPrimitiveGroupIndex(InFirstPrimitiveGroupIndex)
		, NumPrimitiveGroupsPerPacket(InNumPrimitiveGroupsPerPacket)
		, LumenSceneDetail(InLumenSceneDetail)
		, MaxDistanceFromCamera(InMaxDistanceFromCamera)
		, TexelDensityScale(GetCardCameraDistanceTexelDensityScale())
	{
	}

	// Output
	TArray<FMeshCardsAdd> MeshCardsAdds;
	TArray<FMeshCardsRemove> MeshCardsRemoves;
	TArray<FPrimitiveSceneInfo*> LandscapePrimitivesInRange;

	void AnyThreadTask()
	{
		const int32 MinCardResolution = FMath::Clamp(FMath::RoundToInt(LumenSurfaceCache::GetMinCardResolution() / LumenSceneDetail), 1, 1024);
		const int32 LastPrimitiveGroupIndex = FMath::Min(FirstPrimitiveGroupIndex + NumPrimitiveGroupsPerPacket, PrimitiveGroups.Num());

		for (int32 PrimitiveGroupIndex = FirstPrimitiveGroupIndex; PrimitiveGroupIndex < LastPrimitiveGroupIndex; ++PrimitiveGroupIndex)
		{
			if (PrimitiveGroups.IsAllocated(PrimitiveGroupIndex))
			{
				const FLumenPrimitiveGroup& PrimitiveGroup = PrimitiveGroups[PrimitiveGroupIndex];

				// Rough card min resolution test
				float CardMaxDistanceSq = MaxDistanceFromCamera * MaxDistanceFromCamera;
				float DistanceSquared = FLT_MAX;

				for (FVector ViewOrigin : ViewOrigins)
				{
					DistanceSquared = FMath::Min(DistanceSquared, ComputeSquaredDistanceFromBoxToPoint(FVector(PrimitiveGroup.WorldSpaceBoundingBox.Min), FVector(PrimitiveGroup.WorldSpaceBoundingBox.Max), ViewOrigin));
				}
				
				const float MaxCardExtent = PrimitiveGroup.WorldSpaceBoundingBox.GetExtent().GetMax();
				float MaxCardResolution = (TexelDensityScale * MaxCardExtent) / FMath::Sqrt(FMath::Max(DistanceSquared, 1.0f)) + 0.01f;

				// Far field cards have constant resolution over entire range
				if (PrimitiveGroup.bFarField)
				{
					CardMaxDistanceSq = GLumenSceneFarFieldDistance * GLumenSceneFarFieldDistance;
					MaxCardResolution = MaxCardExtent * GLumenSceneFarFieldTexelDensity;
				}

				if (DistanceSquared <= CardMaxDistanceSq && MaxCardResolution >= (PrimitiveGroup.bEmissiveLightSource ? 1.0f : MinCardResolution))
				{
					if (PrimitiveGroup.MeshCardsIndex == -1 && PrimitiveGroup.bValidMeshCards)
					{
						FMeshCardsAdd Add;
						Add.PrimitiveGroupIndex = PrimitiveGroupIndex;
						Add.DistanceSquared = DistanceSquared;
						MeshCardsAdds.Add(Add);
					}

					if (PrimitiveGroup.bHeightfield)
					{
						LandscapePrimitivesInRange.Append(PrimitiveGroup.Primitives);
					}
				}
				else if (PrimitiveGroup.MeshCardsIndex >= 0)
				{
					FMeshCardsRemove Remove;
					Remove.PrimitiveGroupIndex = PrimitiveGroupIndex;
					MeshCardsRemoves.Add(Remove);
				}
			}
		}
	}

	const TSparseSpanArray<FLumenPrimitiveGroup>& PrimitiveGroups;
	TArray<FVector, TInlineAllocator<2>> ViewOrigins;
	int32 FirstPrimitiveGroupIndex;
	int32 NumPrimitiveGroupsPerPacket;
	float LumenSceneDetail;
	float MaxDistanceFromCamera;
	float TexelDensityScale;
};

struct FSurfaceCacheRemove
{
public:
	int32 LumenCardIndex;
};

// Loop over Lumen mesh cards and output card updates
struct FLumenSurfaceCacheUpdateMeshCardsTask
{
public:
	FLumenSurfaceCacheUpdateMeshCardsTask(
		const TSparseSpanArray<FLumenMeshCards>& InLumenMeshCards,
		const TSparseSpanArray<FLumenCard>& InLumenCards,
		const TArray<FVector, TInlineAllocator<2>>& InViewOrigins,
		float InSurfaceCacheResolution,
		float InLumenSceneDetail,
		float InMaxDistanceFromCamera,
		int32 InFirstMeshCardsIndex,
		int32 InNumMeshCardsPerPacket)
		: LumenMeshCards(InLumenMeshCards)
		, LumenCards(InLumenCards)
		, ViewOrigins(InViewOrigins)
		, LumenSceneDetail(InLumenSceneDetail)
		, FirstMeshCardsIndex(InFirstMeshCardsIndex)
		, NumMeshCardsPerPacket(InNumMeshCardsPerPacket)
		, MaxDistanceFromCamera(InMaxDistanceFromCamera)
		, TexelDensityScale(GetCardCameraDistanceTexelDensityScale() * InSurfaceCacheResolution)
		, MaxTexelDensity(GLumenSceneCardMaxTexelDensity)
	{
	}

	// Output
	TArray<FSurfaceCacheRequest> SurfaceCacheRequests;
	TArray<int32> CardsToHide;

	void AnyThreadTask()
	{
		const int32 LastLumenMeshCardsIndex = FMath::Min(FirstMeshCardsIndex + NumMeshCardsPerPacket, LumenMeshCards.Num());
		const int32 MinCardResolution = FMath::Clamp(FMath::RoundToInt(LumenSurfaceCache::GetMinCardResolution() / LumenSceneDetail), 1, 1024);

		for (int32 MeshCardsIndex = FirstMeshCardsIndex; MeshCardsIndex < LastLumenMeshCardsIndex; ++MeshCardsIndex)
		{
			if (LumenMeshCards.IsAllocated(MeshCardsIndex))
			{
				const FLumenMeshCards& MeshCardsInstance = LumenMeshCards[MeshCardsIndex];

				for (uint32 CardIndex = MeshCardsInstance.FirstCardIndex; CardIndex < MeshCardsInstance.FirstCardIndex + MeshCardsInstance.NumCards; ++CardIndex)
				{
					const FLumenCard& LumenCard = LumenCards[CardIndex];

					float CardMaxDistance = MaxDistanceFromCamera;
					float ViewerDistance = FLT_MAX;

					for (FVector ViewOrigin : ViewOrigins)
					{
						ViewerDistance = FMath::Min(ViewerDistance, FMath::Max(FMath::Sqrt(LumenCard.WorldOBB.ComputeSquaredDistanceToPoint((FVector3f)ViewOrigin)), 100.0f));
					}

					// Compute resolution based on its largest extent
					float MaxExtent = FMath::Max(LumenCard.WorldOBB.Extent.X, LumenCard.WorldOBB.Extent.Y);
					float MaxProjectedSize = FMath::Min(TexelDensityScale * MaxExtent * LumenCard.ResolutionScale / ViewerDistance, GLumenSceneCardMaxTexelDensity * MaxExtent);

					// Far field cards have constant resolution over entire range
					if (MeshCardsInstance.bFarField)
					{
						CardMaxDistance = GLumenSceneFarFieldDistance;
						MaxProjectedSize = GLumenSceneFarFieldTexelDensity * MaxExtent * LumenCard.ResolutionScale;
					}

					if (GLumenSceneCardFixedDebugResolution > 0)
					{
						MaxProjectedSize = GLumenSceneCardFixedDebugResolution;
					}

					const int32 MinCardResolutionForMeshCards = MeshCardsInstance.bEmissiveLightSource ? 1 : MinCardResolution;
					const int32 MaxSnappedRes = FMath::RoundUpToPowerOfTwo(FMath::Min(FMath::TruncToInt(MaxProjectedSize), GetCardMaxResolution()));
					const bool bVisible = ViewerDistance < CardMaxDistance && MaxSnappedRes >= MinCardResolutionForMeshCards;
					const int32 ResLevel = FMath::FloorLog2(FMath::Max<uint32>(MaxSnappedRes, Lumen::MinCardResolution));

					if (!bVisible && LumenCard.bVisible)
					{
						CardsToHide.Add(CardIndex);
					}
					else if (bVisible && ResLevel != LumenCard.DesiredLockedResLevel)
					{
						float Distance = ViewerDistance;

						if (LumenCard.bVisible && LumenCard.DesiredLockedResLevel != ResLevel)
						{
							// Make reallocation less important than capturing new cards
							const float ResLevelDelta = FMath::Abs((int32)LumenCard.DesiredLockedResLevel - ResLevel);
							Distance += (1.0f - FMath::Clamp((ResLevelDelta + 1.0f) / 3.0f, 0.0f, 1.0f)) * 2500.0f;
						}

						FSurfaceCacheRequest Request;
						Request.ResLevel = ResLevel;
						Request.CardIndex = CardIndex;
						Request.LocalPageIndex = UINT16_MAX;
						Request.Distance = Distance;
						SurfaceCacheRequests.Add(Request);

						ensure(Request.IsLockedMip());
					}
				}
			}
		}
	}

	const TSparseSpanArray<FLumenMeshCards>& LumenMeshCards;
	const TSparseSpanArray<FLumenCard>& LumenCards;
	TArray<FVector, TInlineAllocator<2>> ViewOrigins;
	float LumenSceneDetail;
	int32 FirstMeshCardsIndex;
	int32 NumMeshCardsPerPacket;
	float MaxDistanceFromCamera;
	float TexelDensityScale;
	float MaxTexelDensity;
};

float ComputeMaxCardUpdateDistanceFromCamera(const FViewInfo& View)
{
	// Limit to global distance field range
	const float LastClipmapExtent = Lumen::GetGlobalDFClipmapExtent(Lumen::GetNumGlobalDFClipmaps(View) - 1);
	float MaxCardDistanceFromCamera = LastClipmapExtent;

#if RHI_RAYTRACING
	// Limit to ray tracing culling radius if ray tracing is used
	if (Lumen::UseHardwareRayTracing(*View.Family) && GetRayTracingCulling() != 0)
	{
		MaxCardDistanceFromCamera = GetRayTracingCullingRadius();
	}
#endif

	return MaxCardDistanceFromCamera + GLumenSceneCardCaptureMargin;
}

/**
 * Make sure that all mesh rendering data is prepared before we render this primitive group
 * @return Will return true it primitive group is ready to render or we need to wait until next frame
 */
bool UpdateStaticMeshes(FLumenPrimitiveGroup& PrimitiveGroup)
{
	bool bReadyToRender = true;

	for (FPrimitiveSceneInfo* PrimitiveSceneInfo : PrimitiveGroup.Primitives)
	{
		if (PrimitiveSceneInfo && PrimitiveSceneInfo->Proxy->AffectsDynamicIndirectLighting())
		{
			if (PrimitiveSceneInfo->NeedsUniformBufferUpdate())
			{
				PrimitiveSceneInfo->UpdateUniformBuffer(FRHICommandListExecutor::GetImmediateCommandList());
			}

			if (PrimitiveSceneInfo->NeedsUpdateStaticMeshes())
			{
				// Need to defer to next InitViews, as main view visible primitives are processed on parallel tasks and calling 
				// CacheMeshDrawCommands may resize CachedDrawLists/CachedMeshDrawCommandStateBuckets causing a crash.
				PrimitiveSceneInfo->BeginDeferredUpdateStaticMeshesWithoutVisibilityCheck();
				bReadyToRender = false;
			}

			if (PrimitiveGroup.bHeightfield && PrimitiveSceneInfo->Proxy->HeightfieldHasPendingStreaming())
			{
				bReadyToRender = false;
			}
		}
	}

	return bReadyToRender;
}

bool FLumenSceneData::RecaptureCardPage(const FViewInfo& MainView, FLumenCardRenderer& LumenCardRenderer, FLumenSurfaceCacheAllocator& CaptureAtlasAllocator, FRHIGPUMask GPUMask, int32 PageTableIndex)
{
	TArray<FCardPageRenderData, SceneRenderingAllocator>& CardPagesToRender = LumenCardRenderer.CardPagesToRender;
	FLumenPageTableEntry& PageTableEntry = GetPageTableEntry(PageTableIndex);
	const FLumenCard& Card = Cards[PageTableEntry.CardIndex];
	const FLumenMeshCards& MeshCardsElement = MeshCards[Card.MeshCardsIndex];

	// Can we fit this card into the temporary card capture allocator?
	if (CaptureAtlasAllocator.IsSpaceAvailable(Card, PageTableEntry.ResLevel, /*bSinglePage*/ true))
	{
		// Allocate space in temporary allocation atlas
		FLumenSurfaceCacheAllocator::FAllocation CardCaptureAllocation;
		CaptureAtlasAllocator.Allocate(PageTableEntry, CardCaptureAllocation);
		check(CardCaptureAllocation.PhysicalPageCoord.X >= 0);

		CardPagesToRender.Add(FCardPageRenderData(
			MainView,
			Card,
			PageTableEntry.CardUVRect,
			CardCaptureAllocation.PhysicalAtlasRect,
			PageTableEntry.PhysicalAtlasRect,
			MeshCardsElement.PrimitiveGroupIndex,
			PageTableEntry.CardIndex,
			PageTableIndex,
			/*bResampleLastLighting*/ true));

		for (uint32 GPUIndex : GPUMask)
		{
			LastCapturedPageHeap[GPUIndex].Update(GetSurfaceCacheUpdateFrameIndex(), PageTableIndex);
		}
		LumenCardRenderer.NumCardTexelsToCapture += PageTableEntry.PhysicalAtlasRect.Area();
		return true;
	}

	return false;
}

/**
 * Process a throttled number of Lumen surface cache add requests
 * It will make virtual and physical allocations, and evict old pages as required
 */
void FLumenSceneData::ProcessLumenSurfaceCacheRequests(
	const FViewInfo& MainView,
	float MaxCardUpdateDistanceFromCamera,
	int32 MaxTileCapturesPerFrame,
	FLumenCardRenderer& LumenCardRenderer,
	FRHIGPUMask GPUMask,
	const TArray<FSurfaceCacheRequest, SceneRenderingAllocator>& SurfaceCacheRequests)
{
	QUICK_SCOPE_CYCLE_COUNTER(ProcessLumenSurfaceCacheRequests);

	TArray<FCardPageRenderData, SceneRenderingAllocator>& CardPagesToRender = LumenCardRenderer.CardPagesToRender;

	TArray<FVirtualPageIndex, SceneRenderingAllocator> HiResPagesToMap;
	TSparseUniqueList<int32, SceneRenderingAllocator> DirtyCards;

	FLumenSurfaceCacheAllocator CaptureAtlasAllocator;
	CaptureAtlasAllocator.Init(GetCardCaptureAtlasSizeInPages());

	for (int32 RequestIndex = 0; RequestIndex < SurfaceCacheRequests.Num(); ++RequestIndex)
	{
		const FSurfaceCacheRequest& Request = SurfaceCacheRequests[RequestIndex];

		if (Request.IsLockedMip())
		{
			// Update low-res locked (always resident) pages
			FLumenCard& Card = Cards[Request.CardIndex];

			if (Card.DesiredLockedResLevel != Request.ResLevel)
			{
				// Check if we can make this allocation at all
				bool bCanAlloc = true;

				uint8 NewLockedAllocationResLevel = Request.ResLevel;
				while (!IsPhysicalSpaceAvailable(Card, NewLockedAllocationResLevel, /*bSinglePage*/ false))
				{
					const int32 MaxFramesSinceLastUsed = 2;

					if (!EvictOldestAllocation(/*MaxFramesSinceLastUsed*/ MaxFramesSinceLastUsed, DirtyCards))
					{
						bCanAlloc = false;
						break;
					}
				}

				// Try to decrease resolution if allocation still can't be made
				while (!bCanAlloc && NewLockedAllocationResLevel > Lumen::MinResLevel)
				{
					--NewLockedAllocationResLevel;
					bCanAlloc = IsPhysicalSpaceAvailable(Card, NewLockedAllocationResLevel, /*bSinglePage*/ false);
				}

				// Can we fit this card into the temporary card capture allocator?
				if (!CaptureAtlasAllocator.IsSpaceAvailable(Card, NewLockedAllocationResLevel, /*bSinglePage*/ false))
				{
					bCanAlloc = false;
				}

				const FLumenMeshCards& MeshCardsElement = MeshCards[Card.MeshCardsIndex];
				if (bCanAlloc && UpdateStaticMeshes(PrimitiveGroups[MeshCardsElement.PrimitiveGroupIndex]))
				{
					Card.bVisible = true;
					Card.DesiredLockedResLevel = Request.ResLevel;

					const bool bResampleLastLighting = Card.IsAllocated();

					// Free previous MinAllocatedResLevel
					FreeVirtualSurface(Card, Card.MinAllocatedResLevel, Card.MinAllocatedResLevel);

					// Free anything lower res than the new res level
					FreeVirtualSurface(Card, Card.MinAllocatedResLevel, NewLockedAllocationResLevel - 1);


					const bool bLockPages = true;
					ReallocVirtualSurface(Card, Request.CardIndex, NewLockedAllocationResLevel, bLockPages);

					// Map and update all pages
					FLumenSurfaceMipMap& MipMap = Card.GetMipMap(Card.MinAllocatedResLevel);
					for (int32 LocalPageIndex = 0; LocalPageIndex < MipMap.SizeInPagesX * MipMap.SizeInPagesY; ++LocalPageIndex)
					{
						const int32 PageIndex = MipMap.GetPageTableIndex(LocalPageIndex);
						FLumenPageTableEntry& PageTableEntry = GetPageTableEntry(PageIndex);

						if (!PageTableEntry.IsMapped())
						{
							MapSurfaceCachePage(MipMap, PageIndex, GPUMask);
							check(PageTableEntry.IsMapped());

							// Allocate space in temporary allocation atlas
							FLumenSurfaceCacheAllocator::FAllocation CardCaptureAllocation;
							CaptureAtlasAllocator.Allocate(PageTableEntry, CardCaptureAllocation);
							check(CardCaptureAllocation.PhysicalPageCoord.X >= 0);

							CardPagesToRender.Add(FCardPageRenderData(
								MainView,
								Card,
								PageTableEntry.CardUVRect,
								CardCaptureAllocation.PhysicalAtlasRect,
								PageTableEntry.PhysicalAtlasRect,
								MeshCardsElement.PrimitiveGroupIndex,
								Request.CardIndex,
								PageIndex,
								bResampleLastLighting));

							for (uint32 GPUIndex : GPUMask)
							{
								LastCapturedPageHeap[GPUIndex].Update(GetSurfaceCacheUpdateFrameIndex(), PageIndex);
							}
							LumenCardRenderer.NumCardTexelsToCapture += PageTableEntry.PhysicalAtlasRect.Area();
						}
					}

					DirtyCards.Add(Request.CardIndex);
				}
			}
		}
		else
		{
			// Hi-Res
			if (Cards.IsAllocated(Request.CardIndex))
			{
				FLumenCard& Card = Cards[Request.CardIndex];

				if (Card.bVisible && Card.MinAllocatedResLevel >= 0 && Request.ResLevel > Card.MinAllocatedResLevel)
				{
					HiResPagesToMap.Add(FVirtualPageIndex(Request.CardIndex, Request.ResLevel, Request.LocalPageIndex));
				}
			}
		}

		if (CardPagesToRender.Num() + HiResPagesToMap.Num() >= MaxTileCapturesPerFrame)
		{
			break;
		}
	}

	// Process hi-res optional pages after locked low res ones are done
	for (const FVirtualPageIndex& VirtualPageIndex : HiResPagesToMap)
	{
		FLumenCard& Card = Cards[VirtualPageIndex.CardIndex];

		if (VirtualPageIndex.ResLevel > Card.MinAllocatedResLevel)
		{
			// Make room for new physical allocations
			bool bCanAlloc = true;
			while (!IsPhysicalSpaceAvailable(Card, VirtualPageIndex.ResLevel, /*bSinglePage*/ true))
			{
				// Don't want to evict pages which may be picked up a jittering tile feedback
				const int32 MaxFramesSinceLastUsed = Lumen::GetFeedbackBufferTileSize() * Lumen::GetFeedbackBufferTileSize();

				if (!EvictOldestAllocation(MaxFramesSinceLastUsed, DirtyCards))
				{
					bCanAlloc = false;
					break;
				}
			}

			// Can we fit this card into the temporary card capture allocator?
			if (!CaptureAtlasAllocator.IsSpaceAvailable(Card, VirtualPageIndex.ResLevel, /*bSinglePage*/ true))
			{
				bCanAlloc = false;
			}

			const FLumenMeshCards& MeshCardsElement = MeshCards[Card.MeshCardsIndex];
			if (bCanAlloc && UpdateStaticMeshes(PrimitiveGroups[MeshCardsElement.PrimitiveGroupIndex]))
			{
				const bool bLockPages = false;
				const bool bResampleLastLighting = Card.IsAllocated();

				ReallocVirtualSurface(Card, VirtualPageIndex.CardIndex, VirtualPageIndex.ResLevel, bLockPages);

				FLumenSurfaceMipMap& MipMap = Card.GetMipMap(VirtualPageIndex.ResLevel);
				const int32 PageIndex = MipMap.GetPageTableIndex(VirtualPageIndex.LocalPageIndex);
				FLumenPageTableEntry& PageTableEntry = GetPageTableEntry(PageIndex);

				if (!PageTableEntry.IsMapped())
				{
					MapSurfaceCachePage(MipMap, PageIndex, GPUMask);
					check(PageTableEntry.IsMapped());

					// Allocate space in temporary allocation atlas
					FLumenSurfaceCacheAllocator::FAllocation CardCaptureAllocation;
					CaptureAtlasAllocator.Allocate(PageTableEntry, CardCaptureAllocation);
					check(CardCaptureAllocation.PhysicalPageCoord.X >= 0);

					CardPagesToRender.Add(FCardPageRenderData(
						MainView,
						Card,
						PageTableEntry.CardUVRect,
						CardCaptureAllocation.PhysicalAtlasRect,
						PageTableEntry.PhysicalAtlasRect,
						MeshCardsElement.PrimitiveGroupIndex,
						VirtualPageIndex.CardIndex,
						PageIndex,
						bResampleLastLighting));

					for (uint32 GPUIndex : GPUMask)
					{
						LastCapturedPageHeap[GPUIndex].Update(GetSurfaceCacheUpdateFrameIndex(), PageIndex);
					}
					LumenCardRenderer.NumCardTexelsToCapture += PageTableEntry.PhysicalAtlasRect.Area();
					DirtyCards.Add(VirtualPageIndex.CardIndex);
				}
			}
		}
	}

	// Process any surface cache page invalidation requests
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SceneCardCaptureInvalidation);

		if (CVarLumenSceneCardCaptureEnableInvalidation.GetValueOnRenderThread() == 0)
		{
			for (uint32 GPUIndex = 0; GPUIndex < GNumExplicitGPUsForRendering; GPUIndex++)
			{
				PagesToRecaptureHeap[GPUIndex].Clear();
			}
		}

		FBinaryHeap<uint32, uint32>& PageHeap = PagesToRecaptureHeap[GPUMask.GetFirstIndex()];
		while (PageHeap.Num() > 0)
		{
			const uint32 PageTableIndex = PageHeap.Top();
			if (RecaptureCardPage(MainView, LumenCardRenderer, CaptureAtlasAllocator, GPUMask, PageTableIndex))
			{
				PageHeap.Pop();
			}
			else
			{
				break;
			}
		}
	}

	// Finally process card refresh to capture any material updates, or render cards that need to be initialized for the first time on
	// a given GPU in multi-GPU scenarios.  Uninitialized cards on a particular GPU will have a zero captured frame index set when the
	// card was allocated.  A zero frame index otherwise can't occur on a card, because the constructor sets SurfaceCacheUpdateFrameIndex
	// to 1, and IncrementSurfaceCacheUpdateFrameIndex skips over zero if it happens to wrap around.
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SceneCardCaptureRefresh);

		int32 NumTexelsLeftToRefresh = GetCardCaptureRefreshNumTexels();
		int32 NumPagesLeftToRefesh = FMath::Min<int32>((int32)GetCardCaptureRefreshNumPages(), MaxTileCapturesPerFrame - CardPagesToRender.Num());

		FBinaryHeap<uint32,uint32>& PageHeap = LastCapturedPageHeap[GPUMask.GetFirstIndex()];

		bool bCanCapture = true;
		while (PageHeap.Num() > 0 && bCanCapture)
		{
			bCanCapture = false;

			const uint32 PageTableIndex = PageHeap.Top();
			const uint32 CapturedSurfaceCacheFrameIndex = PageHeap.GetKey(PageTableIndex);

			const int32 FramesSinceLastUpdated = GetSurfaceCacheUpdateFrameIndex() - CapturedSurfaceCacheFrameIndex;
			if (FramesSinceLastUpdated > 0)
			{
#if WITH_MGPU
				// Limit number of re-captured texels and pages per frame, except always allow captures of uninitialized
				// cards where the captured frame index is zero (don't count them against the throttled limits).
				// Uninitialized cards on a particular GPU will always be at the front of the heap, due to the zero index,
				// so even if the limits are set to zero, we'll still process them if needed (the limit comparisons below
				// are >= 0, and will pass if nothing has been decremented from the limits yet).
				if ((CapturedSurfaceCacheFrameIndex != 0) || (GNumExplicitGPUsForRendering == 1))
#endif
				{
					FLumenPageTableEntry& PageTableEntry = GetPageTableEntry(PageTableIndex);
					const FLumenCard& Card = Cards[PageTableEntry.CardIndex];
					FLumenMipMapDesc MipMapDesc;
					Card.GetMipMapDesc(PageTableEntry.ResLevel, MipMapDesc);
					NumTexelsLeftToRefresh -= MipMapDesc.PageResolution.X * MipMapDesc.PageResolution.Y;
					NumPagesLeftToRefesh -= 1;
				}

				if (NumTexelsLeftToRefresh >= 0 && NumPagesLeftToRefesh >= 0)
				{
					bCanCapture = RecaptureCardPage(MainView, LumenCardRenderer, CaptureAtlasAllocator, GPUMask, PageTableIndex);
				}
			}
		}
	}

	// Evict pages which weren't used recently
	if (!Lumen::IsSurfaceCacheFrozen())
	{
		uint32 MaxFramesSinceLastUsed = FMath::Max(GSurfaceCacheNumFramesToKeepUnusedPages, 0);
		while (EvictOldestAllocation(MaxFramesSinceLastUsed, DirtyCards))
		{
		}
	}

	for (int32 CardIndex : DirtyCards.Array)
	{
		FLumenCard& Card = Cards[CardIndex];
		UpdateCardMipMapHierarchy(Card);
		CardIndicesToUpdateInBuffer.Add(CardIndex);
	}
}

void UpdateSurfaceCachePrimitives(
	FLumenSceneData& LumenSceneData,
	const TArray<FVector, TInlineAllocator<2>>& LumenSceneCameraOrigins,
	float LumenSceneDetail,
	float MaxCardUpdateDistanceFromCamera,
	FLumenCardRenderer& LumenCardRenderer)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UpdateSurfaceCachePrimitives);

	const int32 NumPrimitivesPerTask = FMath::Max(GLumenScenePrimitivesPerTask, 1);
	const int32 NumTasks = FMath::DivideAndRoundUp(LumenSceneData.PrimitiveGroups.Num(), GLumenScenePrimitivesPerTask);

	TArray<FLumenSurfaceCacheUpdatePrimitivesTask, SceneRenderingAllocator> Tasks;
	Tasks.Reserve(NumTasks);

	for (int32 TaskIndex = 0; TaskIndex < NumTasks; ++TaskIndex)
	{
		Tasks.Emplace(
			LumenSceneData.PrimitiveGroups,
			LumenSceneCameraOrigins,
			LumenSceneDetail,
			MaxCardUpdateDistanceFromCamera,
			TaskIndex * NumPrimitivesPerTask,
			NumPrimitivesPerTask);
	}

	const bool bExecuteInParallel = FApp::ShouldUseThreadingForPerformance() && GLumenSceneParallelUpdate != 0;

	ParallelFor(Tasks.Num(),
		[&Tasks](int32 Index)
		{
			Tasks[Index].AnyThreadTask();
		},
		!bExecuteInParallel);

	TArray<FMeshCardsAdd, SceneRenderingAllocator> MeshCardsAdds;

	for (int32 TaskIndex = 0; TaskIndex < Tasks.Num(); ++TaskIndex)
	{
		const FLumenSurfaceCacheUpdatePrimitivesTask& Task = Tasks[TaskIndex];
		LumenSceneData.NumMeshCardsToAdd += Task.MeshCardsAdds.Num();

		// Append requests to the global array
		{
			MeshCardsAdds.Reserve(MeshCardsAdds.Num() + Task.MeshCardsAdds.Num());

			for (int32 RequestIndex = 0; RequestIndex < Task.MeshCardsAdds.Num(); ++RequestIndex)
			{
				MeshCardsAdds.Add(Task.MeshCardsAdds[RequestIndex]);
			}
		}

		for (const FMeshCardsRemove& MeshCardsRemove : Task.MeshCardsRemoves)
		{
			FLumenPrimitiveGroup& PrimitiveGroup = LumenSceneData.PrimitiveGroups[MeshCardsRemove.PrimitiveGroupIndex];
			LumenSceneData.RemoveMeshCards(PrimitiveGroup);
		}

		LumenCardRenderer.LandscapePrimitivesInRange.Append(Task.LandscapePrimitivesInRange);
	}

	if (MeshCardsAdds.Num() > 0)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SortAdds);

		struct FSortBySmallerDistance
		{
			FORCEINLINE bool operator()(const FMeshCardsAdd& A, const FMeshCardsAdd& B) const
			{
				return A.DistanceSquared < B.DistanceSquared;
			}
		};

		MeshCardsAdds.Sort(FSortBySmallerDistance());
	}

	const int32 MeshCardsToAddPerFrame = GetMaxMeshCardsToAddPerFrame();

	for (int32 MeshCardsIndex = 0; MeshCardsIndex < FMath::Min(MeshCardsAdds.Num(), MeshCardsToAddPerFrame); ++MeshCardsIndex)
	{
		const FMeshCardsAdd& MeshCardsAdd = MeshCardsAdds[MeshCardsIndex];
		LumenSceneData.AddMeshCards(MeshCardsAdd.PrimitiveGroupIndex);
	}
}

void UpdateSurfaceCacheMeshCards(
	FLumenSceneData& LumenSceneData,
	const TArray<FVector, TInlineAllocator<2>>& LumenSceneCameraOrigins,
	float LumenSceneDetail,
	float MaxCardUpdateDistanceFromCamera,
	TArray<FSurfaceCacheRequest, SceneRenderingAllocator>& SurfaceCacheRequests,
	const FViewFamilyInfo& ViewFamily)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UpdateMeshCards);

	const int32 NumMeshCardsPerTask = FMath::Max(GLumenSceneMeshCardsPerTask, 1);
	const int32 NumTasks = FMath::DivideAndRoundUp(LumenSceneData.MeshCards.Num(), NumMeshCardsPerTask);

	TArray<FLumenSurfaceCacheUpdateMeshCardsTask, SceneRenderingAllocator> Tasks;
	Tasks.Reserve(NumTasks);

	for (int32 TaskIndex = 0; TaskIndex < NumTasks; ++TaskIndex)
	{
		Tasks.Emplace(
			LumenSceneData.MeshCards,
			LumenSceneData.Cards,
			LumenSceneCameraOrigins,
			LumenSceneData.SurfaceCacheResolution,
			LumenSceneDetail,
			MaxCardUpdateDistanceFromCamera,
			TaskIndex * NumMeshCardsPerTask,
			NumMeshCardsPerTask);
	}

	const bool bExecuteInParallel = FApp::ShouldUseThreadingForPerformance() && GLumenSceneParallelUpdate != 0;

	ParallelFor(Tasks.Num(),
		[&Tasks](int32 Index)
		{
			Tasks[Index].AnyThreadTask();
		},
		!bExecuteInParallel);

	for (int32 TaskIndex = 0; TaskIndex < Tasks.Num(); ++TaskIndex)
	{
		const FLumenSurfaceCacheUpdateMeshCardsTask& Task = Tasks[TaskIndex];
		LumenSceneData.NumLockedCardsToUpdate += Task.SurfaceCacheRequests.Num();

		// Append requests to the global array
		{
			SurfaceCacheRequests.Reserve(SurfaceCacheRequests.Num() + Task.SurfaceCacheRequests.Num());

			for (int32 RequestIndex = 0; RequestIndex < Task.SurfaceCacheRequests.Num(); ++RequestIndex)
			{
				SurfaceCacheRequests.Add(Task.SurfaceCacheRequests[RequestIndex]);
			}
		}

		for (int32 CardIndex : Task.CardsToHide)
		{
			FLumenCard& Card = LumenSceneData.Cards[CardIndex];

			if (Card.bVisible)
			{
				LumenSceneData.RemoveCardFromAtlas(CardIndex);
				Card.bVisible = false;
			}
		}
	}

	LumenSceneData.UpdateSurfaceCacheFeedback(LumenSceneCameraOrigins, SurfaceCacheRequests, ViewFamily);

	if (SurfaceCacheRequests.Num() > 0)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SortRequests);

		struct FSortBySmallerDistance
		{
			FORCEINLINE bool operator()(const FSurfaceCacheRequest& A, const FSurfaceCacheRequest& B) const
			{
				return A.Distance < B.Distance;
			}
		};

		SurfaceCacheRequests.Sort(FSortBySmallerDistance());
	}
}

extern void UpdateLumenScenePrimitives(FRHIGPUMask GPUMask, FScene* Scene);

void AllocateResampledCardCaptureAtlas(FRDGBuilder& GraphBuilder, FIntPoint CardCaptureAtlasSize, FResampledCardCaptureAtlas& CardCaptureAtlas)
{
	CardCaptureAtlas.Size = CardCaptureAtlasSize;

	CardCaptureAtlas.DirectLighting = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(
			CardCaptureAtlasSize,
			Lumen::GetDirectLightingAtlasFormat(),
			FClearValueBinding::Green,
			TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_NoFastClear),
		TEXT("Lumen.ResampledCardCaptureDirectLighting"));

	CardCaptureAtlas.IndirectLighting = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(
			CardCaptureAtlasSize,
			Lumen::GetIndirectLightingAtlasFormat(),
			FClearValueBinding::Green,
			TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_NoFastClear),
		TEXT("Lumen.ResampledCardCaptureIndirectLighting"));

	CardCaptureAtlas.NumFramesAccumulated = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(
			CardCaptureAtlasSize,
			Lumen::GetNumFramesAccumulatedAtlasFormat(),
			FClearValueBinding::Black,
			TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_NoFastClear),
		TEXT("Lumen.ResampledCardCaptureNumFramesAccumulated"));
}

class FResampleLightingHistoryToCardCaptureAtlasPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FResampleLightingHistoryToCardCaptureAtlasPS);
	SHADER_USE_PARAMETER_STRUCT(FResampleLightingHistoryToCardCaptureAtlasPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenCardScene, LumenCardScene)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DirectLightingAtlas)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, IndirectLightingAtlas)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, RadiosityNumFramesAccumulatedAtlas)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint4>, NewCardPageResampleData)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}
};

IMPLEMENT_GLOBAL_SHADER(FResampleLightingHistoryToCardCaptureAtlasPS, "/Engine/Private/Lumen/LumenSceneLighting.usf", "ResampleLightingHistoryToCardCaptureAtlasPS", SF_Pixel);

BEGIN_SHADER_PARAMETER_STRUCT(FResampleLightingHistoryToCardCaptureParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FPixelShaderUtils::FRasterizeToRectsVS::FParameters, VS)
	SHADER_PARAMETER_STRUCT_INCLUDE(FResampleLightingHistoryToCardCaptureAtlasPS::FParameters, PS)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

// Try to resample direct lighting and indirect lighting (radiosity) from existing surface cache to new captured cards
void ResampleLightingHistory(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FScene* Scene,
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	const TArray<FCardPageRenderData, SceneRenderingAllocator>& CardPagesToRender,
	FLumenSceneData& LumenSceneData,
	FResampledCardCaptureAtlas& CardCaptureAtlas)
{
	if (GLumenSceneSurfaceCacheResampleLighting
		&& FrameTemporaries.PageTableBufferSRV
		&& FrameTemporaries.CardBufferSRV)
	{
		AllocateResampledCardCaptureAtlas(GraphBuilder, LumenSceneData.GetCardCaptureAtlasSize(), CardCaptureAtlas);

		FRDGUploadData<FUintVector4> CardCaptureRectArray(GraphBuilder, CardPagesToRender.Num());
		FRDGUploadData<FUintVector4> CardPageResampleDataArray(GraphBuilder, CardPagesToRender.Num() * 2);

		for (int32 Index = 0; Index < CardPagesToRender.Num(); Index++)
		{
			const FCardPageRenderData& CardPageRenderData = CardPagesToRender[Index];

			FUintVector4& Rect = CardCaptureRectArray[Index];
			Rect.X = FMath::Max(CardPageRenderData.CardCaptureAtlasRect.Min.X, 0);
			Rect.Y = FMath::Max(CardPageRenderData.CardCaptureAtlasRect.Min.Y, 0);
			Rect.Z = FMath::Max(CardPageRenderData.CardCaptureAtlasRect.Max.X, 0);
			Rect.W = FMath::Max(CardPageRenderData.CardCaptureAtlasRect.Max.Y, 0);

			FUintVector4& CardPageResampleData0 = CardPageResampleDataArray[Index * 2 + 0];
			FUintVector4& CardPageResampleData1 = CardPageResampleDataArray[Index * 2 + 1];

			CardPageResampleData0.X = CardPageRenderData.bResampleLastLighting ? CardPageRenderData.CardIndex : -1;
			CardPageResampleData1 = FUintVector4(
				*(const uint32*)&CardPageRenderData.CardUVRect.X,
				*(const uint32*)&CardPageRenderData.CardUVRect.Y,
				*(const uint32*)&CardPageRenderData.CardUVRect.Z,
				*(const uint32*)&CardPageRenderData.CardUVRect.W);
		}

		FRDGBufferRef CardCaptureRectBuffer = CreateUploadBuffer(GraphBuilder, TEXT("Lumen.CardCaptureRects"),
			sizeof(FUintVector4), FMath::RoundUpToPowerOfTwo(CardPagesToRender.Num()),
			CardCaptureRectArray);
		FRDGBufferSRVRef CardCaptureRectBufferSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(CardCaptureRectBuffer, PF_R32G32B32A32_UINT));

		FRDGBufferRef NewCardPageResampleDataBuffer = CreateUploadBuffer(GraphBuilder, TEXT("Lumen.CardPageResampleDataBuffer"),
			sizeof(FUintVector4), FMath::RoundUpToPowerOfTwo(CardPagesToRender.Num() * 2),
			CardPageResampleDataArray);
		FRDGBufferSRVRef NewCardPageResampleDataSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(NewCardPageResampleDataBuffer, PF_R32G32B32A32_UINT));

		{
			FResampleLightingHistoryToCardCaptureParameters* PassParameters = GraphBuilder.AllocParameters<FResampleLightingHistoryToCardCaptureParameters>();

			PassParameters->RenderTargets[0] = FRenderTargetBinding(CardCaptureAtlas.DirectLighting, ERenderTargetLoadAction::ENoAction);
			PassParameters->RenderTargets[1] = FRenderTargetBinding(CardCaptureAtlas.IndirectLighting, ERenderTargetLoadAction::ENoAction);
			PassParameters->RenderTargets[2] = FRenderTargetBinding(CardCaptureAtlas.NumFramesAccumulated, ERenderTargetLoadAction::ENoAction);

			PassParameters->PS.View = View.ViewUniformBuffer;
			PassParameters->PS.LumenCardScene = FrameTemporaries.LumenCardSceneUniformBuffer;
			PassParameters->PS.DirectLightingAtlas = FrameTemporaries.DirectLightingAtlas;
			PassParameters->PS.IndirectLightingAtlas = FrameTemporaries.IndirectLightingAtlas;
			PassParameters->PS.RadiosityNumFramesAccumulatedAtlas = FrameTemporaries.RadiosityNumFramesAccumulatedAtlas;
			PassParameters->PS.NewCardPageResampleData = NewCardPageResampleDataSRV;

			FResampleLightingHistoryToCardCaptureAtlasPS::FPermutationDomain PermutationVector;
			auto PixelShader = View.ShaderMap->GetShader<FResampleLightingHistoryToCardCaptureAtlasPS>(PermutationVector);

			FPixelShaderUtils::AddRasterizeToRectsPass<FResampleLightingHistoryToCardCaptureAtlasPS>(
				GraphBuilder,
				View.ShaderMap,
				RDG_EVENT_NAME("ResampleLightingHistoryToCardCaptureAtlas"),
				PixelShader,
				PassParameters,
				CardCaptureAtlas.Size,
				CardCaptureRectBufferSRV,
				CardPagesToRender.Num(),
				TStaticBlendState<>::GetRHI(),
				TStaticRasterizerState<>::GetRHI(),
				TStaticDepthStencilState<false, CF_Always>::GetRHI());
		}
	}
}

void FLumenSceneData::FillFrameTemporaries(FRDGBuilder& GraphBuilder, FLumenSceneFrameTemporaries& FrameTemporaries)
{
	const auto FillBuffer = [&](FRDGBufferSRV*& OutSRV, const TRefCountPtr<FRDGPooledBuffer>& InBuffer)
	{
		if (!OutSRV && InBuffer)
		{
			OutSRV = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(InBuffer));
		}
	};

	FillBuffer(FrameTemporaries.CardBufferSRV, CardBuffer);
	FillBuffer(FrameTemporaries.MeshCardsBufferSRV, MeshCardsBuffer);
	FillBuffer(FrameTemporaries.HeightfieldBufferSRV, HeightfieldBuffer);
	FillBuffer(FrameTemporaries.SceneInstanceIndexToMeshCardsIndexBufferSRV, SceneInstanceIndexToMeshCardsIndexBuffer);
	FillBuffer(FrameTemporaries.PageTableBufferSRV, PageTableBuffer);
	FillBuffer(FrameTemporaries.CardPageBufferSRV, CardPageBuffer);

	const auto FillTexture = [&](FRDGTexture*& OutTexture, const TRefCountPtr<IPooledRenderTarget>& InTexture)
	{
		if (!OutTexture && InTexture)
		{
			OutTexture = GraphBuilder.RegisterExternalTexture(InTexture);
		}
	};

	FillTexture(FrameTemporaries.AlbedoAtlas, AlbedoAtlas);
	FillTexture(FrameTemporaries.OpacityAtlas, OpacityAtlas);
	FillTexture(FrameTemporaries.NormalAtlas, NormalAtlas);
	FillTexture(FrameTemporaries.EmissiveAtlas, EmissiveAtlas);
	FillTexture(FrameTemporaries.DepthAtlas, DepthAtlas);
	FillTexture(FrameTemporaries.DirectLightingAtlas, DirectLightingAtlas);
	FillTexture(FrameTemporaries.IndirectLightingAtlas, IndirectLightingAtlas);
	FillTexture(FrameTemporaries.RadiosityNumFramesAccumulatedAtlas, RadiosityNumFramesAccumulatedAtlas);
	FillTexture(FrameTemporaries.FinalLightingAtlas, FinalLightingAtlas);
}

void FDeferredShadingSceneRenderer::BeginUpdateLumenSceneTasks(FRDGBuilder& GraphBuilder, FLumenSceneFrameTemporaries& FrameTemporaries)
{
	LLM_SCOPE_BYTAG(Lumen);

	bool bAnyLumenActive = false;

	for (const FViewInfo& View : Views)
	{
		bAnyLumenActive = bAnyLumenActive || ShouldRenderLumenDiffuseGI(Scene, View) || ShouldRenderLumenReflections(View);
	}

	LumenCardRenderer.Reset();

	if (bAnyLumenActive
		&& !ViewFamily.EngineShowFlags.HitProxies)
	{
		SCOPED_NAMED_EVENT(FDeferredShadingSceneRenderer_BeginUpdateLumenSceneTasks, FColor::Emerald);
		QUICK_SCOPE_CYCLE_COUNTER(BeginUpdateLumenSceneTasks);

		FLumenSceneData& LumenSceneData = *Scene->GetLumenSceneData(Views[0]);
		LumenSceneData.bDebugClearAllCachedState = GLumenSceneRecaptureLumenSceneEveryFrame != 0;
		const bool bReallocateAtlas = LumenSceneData.UpdateAtlasSize();

		// Surface cache reset for debugging
		if ((GLumenSceneSurfaceCacheReset != 0)
			|| (GLumenSceneSurfaceCacheResetEveryNthFrame > 0 && (ViewFamily.FrameNumber % (uint32)GLumenSceneSurfaceCacheResetEveryNthFrame == 0)))
		{
			LumenSceneData.bDebugClearAllCachedState = true;
			GLumenSceneSurfaceCacheReset = 0;
		}

		if (GLumenSceneForceEvictHiResPages != 0)
		{
			LumenSceneData.ForceEvictEntireCache();
			GLumenSceneForceEvictHiResPages = 0;
		}

		LumenSceneData.NumMeshCardsToAdd = 0;
		LumenSceneData.NumLockedCardsToUpdate = 0;
		LumenSceneData.NumHiResPagesToAdd = 0;

		UpdateLumenScenePrimitives(GraphBuilder.RHICmdList.GetGPUMask(), Scene);
		UpdateDistantScene(Scene, Views[0]);

		if (LumenSceneData.bDebugClearAllCachedState || bReallocateAtlas)
		{
			LumenSceneData.RemoveAllMeshCards();
		}

		TArray<FVector, TInlineAllocator<2>> LumenSceneCameraOrigins;
		float MaxCardUpdateDistanceFromCamera = 0.0f;
		float LumenSceneDetail = 0.0f;

		for (const FViewInfo& View : Views)
		{
			LumenSceneCameraOrigins.Add(Lumen::GetLumenSceneViewOrigin(View, Lumen::GetNumGlobalDFClipmaps(View) - 1));
			MaxCardUpdateDistanceFromCamera = FMath::Max(MaxCardUpdateDistanceFromCamera, ComputeMaxCardUpdateDistanceFromCamera(View));
			LumenSceneDetail = FMath::Max(LumenSceneDetail, FMath::Clamp<float>(View.FinalPostProcessSettings.LumenSceneDetail, .125f, 8.0f));
		}

		const int32 MaxTileCapturesPerFrame = GetMaxTileCapturesPerFrame();

		if (MaxTileCapturesPerFrame > 0)
		{
			QUICK_SCOPE_CYCLE_COUNTER(FillCardPagesToRender);

			TArray<FSurfaceCacheRequest, SceneRenderingAllocator> SurfaceCacheRequests;

			UpdateSurfaceCachePrimitives(
				LumenSceneData,
				LumenSceneCameraOrigins,
				LumenSceneDetail,
				MaxCardUpdateDistanceFromCamera,
				LumenCardRenderer);

			UpdateSurfaceCacheMeshCards(
				LumenSceneData,
				LumenSceneCameraOrigins,
				LumenSceneDetail,
				MaxCardUpdateDistanceFromCamera,
				SurfaceCacheRequests,
				ViewFamily);

			LumenSceneData.ProcessLumenSurfaceCacheRequests(
				Views[0],
				MaxCardUpdateDistanceFromCamera,
				MaxTileCapturesPerFrame,
				LumenCardRenderer,
				GraphBuilder.RHICmdList.GetGPUMask(),
				SurfaceCacheRequests);
		}

		// Atlas reallocation
		if (bReallocateAtlas || !LumenSceneData.AlbedoAtlas)
		{
			LumenSceneData.AllocateCardAtlases(GraphBuilder, FrameTemporaries);
			ClearLumenSurfaceCacheAtlas(GraphBuilder, FrameTemporaries, Views[0].ShaderMap);
		}

		LumenSceneData.FillFrameTemporaries(GraphBuilder, FrameTemporaries);

		if (LumenSceneData.bDebugClearAllCachedState)
		{
			ClearLumenSurfaceCacheAtlas(GraphBuilder, FrameTemporaries, Views[0].ShaderMap);
		}

		TArray<FCardPageRenderData, SceneRenderingAllocator>& CardPagesToRender = LumenCardRenderer.CardPagesToRender;

		if (CardPagesToRender.Num())
		{
			UpdateLumenCardSceneUniformBuffer(GraphBuilder, Scene, *Scene->GetLumenSceneData(Views[0]), FrameTemporaries);

			// Before we update the GPU page table, read from the persistent atlases for the card pages we are reallocating, and write it to the card capture atlas
			// This is a resample operation, as the original data may have been at a different mip level, or didn't exist at all
			ResampleLightingHistory(
				GraphBuilder,
				Views[0],
				Scene,
				FrameTemporaries,
				CardPagesToRender,
				LumenSceneData,
				LumenCardRenderer.ResampledCardCaptureAtlas);
		}

		LumenSceneData.UploadPageTable(GraphBuilder, FrameTemporaries);

		if (CardPagesToRender.Num() > 0)
		{
			QUICK_SCOPE_CYCLE_COUNTER(MeshPassSetup);

		#if (UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT) && STATS
			if (GLumenSceneSurfaceCacheLogUpdates != 0)
			{
				UE_LOG(LogRenderer, Log, TEXT("Surface Cache Updates: %d"), CardPagesToRender.Num());

				if (GLumenSceneSurfaceCacheLogUpdates > 1)
				{ 
					for (const FCardPageRenderData& CardPageRenderData : CardPagesToRender)
					{
						const FLumenPrimitiveGroup& LumenPrimitiveGroup = LumenSceneData.PrimitiveGroups[CardPageRenderData.PrimitiveGroupIndex];

						UE_LOG(LogRenderer, Log, TEXT("%s Instance:%d NumPrimsInGroup: %d"),
							*LumenPrimitiveGroup.Primitives[0]->Proxy->GetStatId().GetName().ToString(),
							LumenPrimitiveGroup.PrimitiveInstanceIndex,
							LumenPrimitiveGroup.Primitives.Num());
					}
				}
			}
		#endif

			for (FCardPageRenderData& CardPageRenderData : CardPagesToRender)
			{
				CardPageRenderData.StartMeshDrawCommandIndex = LumenCardRenderer.MeshDrawCommands.Num();
				CardPageRenderData.NumMeshDrawCommands = 0;
				int32 NumNanitePrimitives = 0;

				const FLumenPrimitiveGroup& PrimitiveGroup = LumenSceneData.PrimitiveGroups[CardPageRenderData.PrimitiveGroupIndex];
				const FLumenCard& Card = LumenSceneData.Cards[CardPageRenderData.CardIndex];
				ensure(Card.bVisible);

				if (PrimitiveGroup.bHeightfield)
				{
					AddCardCaptureDraws(
						Scene,
						CardPageRenderData,
						PrimitiveGroup,
						LumenCardRenderer.LandscapePrimitivesInRange,
						LumenCardRenderer.MeshDrawCommands,
						LumenCardRenderer.MeshDrawPrimitiveIds);
				}
				else
				{
					AddCardCaptureDraws(
						Scene,
						CardPageRenderData,
						PrimitiveGroup,
						PrimitiveGroup.Primitives,
						LumenCardRenderer.MeshDrawCommands,
						LumenCardRenderer.MeshDrawPrimitiveIds);
				}

				CardPageRenderData.NumMeshDrawCommands = LumenCardRenderer.MeshDrawCommands.Num() - CardPageRenderData.StartMeshDrawCommandIndex;
			}
		}
	}
}

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FLumenCardScene, "LumenCardScene");

void UpdateLumenCardSceneUniformBuffer(
	FRDGBuilder& GraphBuilder,
	FScene* Scene,
	const FLumenSceneData& LumenSceneData,
	FLumenSceneFrameTemporaries& FrameTemporaries)
{
	FLumenCardScene* UniformParameters = GraphBuilder.AllocParameters<FLumenCardScene>();
	UniformParameters->NumCards = LumenSceneData.Cards.Num();
	UniformParameters->NumMeshCards = LumenSceneData.MeshCards.Num();
	UniformParameters->NumCardPages = LumenSceneData.GetNumCardPages();
	UniformParameters->MaxConeSteps = GLumenGIMaxConeSteps;	
	UniformParameters->PhysicalAtlasSize = LumenSceneData.GetPhysicalAtlasSize();
	UniformParameters->InvPhysicalAtlasSize = FVector2f(1.0f) / UniformParameters->PhysicalAtlasSize;
	UniformParameters->IndirectLightingAtlasDownsampleFactor = Lumen::GetRadiosityAtlasDownsampleFactor();
	UniformParameters->NumDistantCards = LumenSceneData.DistantCardIndices.Num();
	extern float GLumenDistantSceneMaxTraceDistance;
	UniformParameters->DistantSceneMaxTraceDistance = GLumenDistantSceneMaxTraceDistance;
	UniformParameters->DistantSceneDirection = FVector3f::ZeroVector;

	if (Scene->DirectionalLights.Num() > 0)
	{
		UniformParameters->DistantSceneDirection = (FVector3f)-Scene->DirectionalLights[0]->Proxy->GetDirection();
	}
	
	for (int32 i = 0; i < LumenSceneData.DistantCardIndices.Num(); i++)
	{
		GET_SCALAR_ARRAY_ELEMENT(UniformParameters->DistantCardIndices, i) = LumenSceneData.DistantCardIndices[i];
	}

	UniformParameters->CardData = FrameTemporaries.CardBufferSRV;
	UniformParameters->MeshCardsData = FrameTemporaries.MeshCardsBufferSRV;
	UniformParameters->HeightfieldData = FrameTemporaries.HeightfieldBufferSRV;
	UniformParameters->SceneInstanceIndexToMeshCardsIndexBuffer = FrameTemporaries.SceneInstanceIndexToMeshCardsIndexBufferSRV;
	UniformParameters->PageTableBuffer = FrameTemporaries.PageTableBufferSRV;
	UniformParameters->CardPageData = FrameTemporaries.CardPageBufferSRV;

	UniformParameters->NumHeightfields = LumenSceneData.Heightfields.Num();

	UniformParameters->AlbedoAtlas = FrameTemporaries.AlbedoAtlas;
	UniformParameters->OpacityAtlas = FrameTemporaries.OpacityAtlas;
	UniformParameters->NormalAtlas = FrameTemporaries.NormalAtlas;
	UniformParameters->EmissiveAtlas = FrameTemporaries.EmissiveAtlas;
	UniformParameters->DepthAtlas = FrameTemporaries.DepthAtlas;

	FrameTemporaries.LumenCardSceneUniformBuffer = GraphBuilder.CreateUniformBuffer(UniformParameters);
}

DECLARE_GPU_STAT(UpdateCardSceneBuffer);

class FClearLumenCardCapturePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClearLumenCardCapturePS);
	SHADER_USE_PARAMETER_STRUCT(FClearLumenCardCapturePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}
};

IMPLEMENT_GLOBAL_SHADER(FClearLumenCardCapturePS, "/Engine/Private/Lumen/LumenSceneLighting.usf", "ClearLumenCardCapturePS", SF_Pixel);

BEGIN_SHADER_PARAMETER_STRUCT(FClearLumenCardCaptureParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FPixelShaderUtils::FRasterizeToRectsVS::FParameters, VS)
	SHADER_PARAMETER_STRUCT_INCLUDE(FClearLumenCardCapturePS::FParameters, PS)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

void ClearLumenCardCapture(
	FRDGBuilder& GraphBuilder,
	const FGlobalShaderMap* GlobalShaderMap,
	const FCardCaptureAtlas& Atlas,
	FRDGBufferSRVRef RectCoordBufferSRV,
	uint32 NumRects)
{
	FClearLumenCardCaptureParameters* PassParameters = GraphBuilder.AllocParameters<FClearLumenCardCaptureParameters>();

	PassParameters->RenderTargets[0] = FRenderTargetBinding(Atlas.Albedo, ERenderTargetLoadAction::ELoad);
	PassParameters->RenderTargets[1] = FRenderTargetBinding(Atlas.Normal, ERenderTargetLoadAction::ELoad);
	PassParameters->RenderTargets[2] = FRenderTargetBinding(Atlas.Emissive, ERenderTargetLoadAction::ELoad);
	PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(Atlas.DepthStencil, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthWrite_StencilWrite);

	auto PixelShader = GlobalShaderMap->GetShader<FClearLumenCardCapturePS>();

	FPixelShaderUtils::AddRasterizeToRectsPass<FClearLumenCardCapturePS>(
		GraphBuilder,
		GlobalShaderMap,
		RDG_EVENT_NAME("ClearCardCapture"),
		PixelShader,
		PassParameters,
		Atlas.Size,
		RectCoordBufferSRV,
		NumRects,
		TStaticBlendState<>::GetRHI(),
		TStaticRasterizerState<>::GetRHI(),
		TStaticDepthStencilState<true, CF_Always,
		true, CF_Always, SO_Replace, SO_Replace, SO_Replace,
		false, CF_Always, SO_Replace, SO_Replace, SO_Replace,
		0xff, 0xff>::GetRHI());
}

BEGIN_SHADER_PARAMETER_STRUCT(FLumenCardPassParameters, )
	// An RDG View uniform buffer is used as an optimization to move creation off the render thread.
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenCardPassUniformParameters, CardPass)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

FIntPoint FLumenSceneData::GetCardCaptureAtlasSizeInPages() const
{
	const float MultPerComponent = 1.0f / FMath::Sqrt(FMath::Clamp(GLumenSceneCardCaptureFactor, 1.0f, 1024.0f));

	FIntPoint CaptureAtlasSizeInPages;
	CaptureAtlasSizeInPages.X = FMath::DivideAndRoundUp<uint32>(PhysicalAtlasSize.X * MultPerComponent + 0.5f, Lumen::PhysicalPageSize);
	CaptureAtlasSizeInPages.Y = FMath::DivideAndRoundUp<uint32>(PhysicalAtlasSize.Y * MultPerComponent + 0.5f, Lumen::PhysicalPageSize);
	return CaptureAtlasSizeInPages;
}

FIntPoint FLumenSceneData::GetCardCaptureAtlasSize() const 
{
	return GetCardCaptureAtlasSizeInPages() * Lumen::PhysicalPageSize;
}

uint32 FLumenSceneData::GetCardCaptureRefreshNumTexels() const
{
	const float CardCaptureRefreshFraction = FMath::Clamp(CVarLumenSceneCardCaptureRefreshFraction.GetValueOnRenderThread(), 0.0f, 1.0f);
	if (CardCaptureRefreshFraction > 0.0f)
	{
		// Allow to capture at least 1 full physical page
		FIntPoint CardCaptureAtlasSize = GetCardCaptureAtlasSize();
		return FMath::Max(CardCaptureAtlasSize.X * CardCaptureAtlasSize.Y * CardCaptureRefreshFraction, Lumen::PhysicalPageSize * Lumen::PhysicalPageSize);
	}

	return 0;
}

uint32 FLumenSceneData::GetCardCaptureRefreshNumPages() const
{
	const float CardCaptureRefreshFraction = FMath::Clamp(CVarLumenSceneCardCaptureRefreshFraction.GetValueOnRenderThread(), 0.0f, 1.0f);
	if (CardCaptureRefreshFraction > 0.0f)
	{
		// Allow to capture at least 1 full physical page
		return FMath::Clamp(GetMaxTileCapturesPerFrame() * CardCaptureRefreshFraction, 1, GetMaxTileCapturesPerFrame());
	}

	return 0;
}

void AllocateCardCaptureAtlas(FRDGBuilder& GraphBuilder, FIntPoint CardCaptureAtlasSize, FCardCaptureAtlas& CardCaptureAtlas)
{
	// Collect info from SetupCardCaptureRenderTargetsInfo
	FGraphicsPipelineRenderTargetsInfo RenderTargetsInfo;
	SetupCardCaptureRenderTargetsInfo(RenderTargetsInfo);
	check(RenderTargetsInfo.RenderTargetsEnabled == 3);

	CardCaptureAtlas.Size = CardCaptureAtlasSize;

	CardCaptureAtlas.Albedo = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(
			CardCaptureAtlasSize,
			(EPixelFormat)RenderTargetsInfo.RenderTargetFormats[0],
			FClearValueBinding::Black,
			RenderTargetsInfo.RenderTargetFlags[0]),
		TEXT("Lumen.CardCaptureAlbedoAtlas"));

	CardCaptureAtlas.Normal = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(
			CardCaptureAtlasSize,
			(EPixelFormat)RenderTargetsInfo.RenderTargetFormats[1],
			FClearValueBinding::Black,
			RenderTargetsInfo.RenderTargetFlags[1]),
		TEXT("Lumen.CardCaptureNormalAtlas"));

	CardCaptureAtlas.Emissive = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(
			CardCaptureAtlasSize,
			(EPixelFormat)RenderTargetsInfo.RenderTargetFormats[2],
			FClearValueBinding::Black,
			RenderTargetsInfo.RenderTargetFlags[2]),
		TEXT("Lumen.CardCaptureEmissiveAtlas"));

	CardCaptureAtlas.DepthStencil = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(
			CardCaptureAtlasSize,
			PF_DepthStencil,
			FClearValueBinding::DepthZero,
			RenderTargetsInfo.DepthStencilTargetFlag),
		TEXT("Lumen.CardCaptureDepthStencilAtlas"));
}

bool UpdateGlobalLightingState(const FScene* Scene, const FViewInfo& View, FLumenSceneData& LumenSceneData)
{
	FLumenGlobalLightingState& GlobalLightingState = LumenSceneData.GlobalLightingState;

	bool bPropagateGlobalLightingChange = false;
	const FLightSceneInfo* DirectionalLightSceneInfo = nullptr;

	for (const FLightSceneInfo* LightSceneInfo : Scene->DirectionalLights)
	{
		if (LightSceneInfo->ShouldRenderLightViewIndependent()
			&& LightSceneInfo->ShouldRenderLight(View, true)
			&& LightSceneInfo->Proxy->GetIndirectLightingScale() > 0.0f)
		{
			DirectionalLightSceneInfo = LightSceneInfo;
			break;
		}
	}

	{
		const float OldMax = GlobalLightingState.bDirectionalLightValid ? GlobalLightingState.DirectionalLightColor.GetMax() : 0.0f;
		const float NewMax = DirectionalLightSceneInfo ? DirectionalLightSceneInfo->Proxy->GetColor().GetMax() : 0.0f;
		const float Ratio = FMath::Max(OldMax, .00001f) / FMath::Max(NewMax, .00001f);

		if (Ratio > 4.0f || Ratio < .25f)
		{
			bPropagateGlobalLightingChange = true;
		}
	}

	if (DirectionalLightSceneInfo)
	{
		GlobalLightingState.DirectionalLightColor = DirectionalLightSceneInfo->Proxy->GetColor();
		GlobalLightingState.bDirectionalLightValid = true;
	}
	else
	{
		GlobalLightingState.DirectionalLightColor = FLinearColor::Black;
		GlobalLightingState.bDirectionalLightValid = false;
	}

	const FSkyLightSceneProxy* SkyLightProxy = Scene->SkyLight;

	{
		const float OldMax = GlobalLightingState.bSkyLightValid ? GlobalLightingState.SkyLightColor.GetMax() : 0.0f;
		const float NewMax = SkyLightProxy ? SkyLightProxy->GetEffectiveLightColor().GetMax() : 0.0f;
		const float Ratio = FMath::Max(OldMax, .00001f) / FMath::Max(NewMax, .00001f);

		if (Ratio > 4.0f || Ratio < .25f)
		{
			bPropagateGlobalLightingChange = true;
		}
	}

	if (SkyLightProxy)
	{
		GlobalLightingState.SkyLightColor = SkyLightProxy->GetEffectiveLightColor();
		GlobalLightingState.bSkyLightValid = true;
	}
	else
	{
		GlobalLightingState.SkyLightColor = FLinearColor::Black;
		GlobalLightingState.bSkyLightValid = false;
	}

	return bPropagateGlobalLightingChange;
}

void FDeferredShadingSceneRenderer::UpdateLumenScene(FRDGBuilder& GraphBuilder, FLumenSceneFrameTemporaries& FrameTemporaries)
{
	LLM_SCOPE_BYTAG(Lumen);
	TRACE_CPUPROFILER_EVENT_SCOPE(FDeferredShadingSceneRenderer::UpdateLumenScene);

	bool bAnyLumenActive = false;

	for (FViewInfo& View : Views)
	{
		const FPerViewPipelineState& ViewPipelineState = GetViewPipelineState(View);
		bool bLumenActive =
			((ViewPipelineState.DiffuseIndirectMethod == EDiffuseIndirectMethod::Lumen || ViewPipelineState.ReflectionsMethod == EReflectionsMethod::Lumen)
				// Don't update scene lighting for secondary views
				&& !View.bIsPlanarReflection 
				&& !View.bIsSceneCaptureCube
				&& !View.bIsReflectionCapture
				&& View.ViewState);

		bAnyLumenActive = bAnyLumenActive || bLumenActive;

		// Cache LumenSceneData pointer per view for efficient lookup of the view specific Lumen scene (also nice for debugging)
		View.ViewLumenSceneData = Scene->FindLumenSceneData(View.ViewState ? View.ViewState->GetViewKey() : 0, View.GPUMask.GetFirstIndex());

#if WITH_MGPU
		if (bLumenActive)
		{
			if (View.ViewLumenSceneData->bViewSpecific)
			{
				// Update view specific scene data if the GPU mask changed (copies resources cross GPU so CPU and GPU data are coherent)
				View.ViewLumenSceneData->UpdateGPUMask(GraphBuilder, FrameTemporaries, View.ViewState->Lumen, View.GPUMask);
			}
			else if (View.GPUMask.GetFirstIndex() != 0)
			{
				// Otherwise, if this view is on a different GPU, we need to allocate GPU specific scene data (if not already allocated)
				if (View.ViewLumenSceneData == Scene->DefaultLumenSceneData)
				{
					View.ViewLumenSceneData = new FLumenSceneData(Scene->DefaultLumenSceneData->bTrackAllPrimitives);

					View.ViewLumenSceneData->CopyInitialData(*Scene->DefaultLumenSceneData);

					// Key shouldn't already exist in Scene, because "FindLumenSceneData" above should have found it
					FLumenSceneDataKey ByGPUIndex = { 0, View.GPUMask.GetFirstIndex() };
					check(Scene->PerViewOrGPULumenSceneData.Find(ByGPUIndex) == nullptr);

					Scene->PerViewOrGPULumenSceneData.Emplace(ByGPUIndex, View.ViewLumenSceneData);
				}
			}
		}
#endif  // WITH_MGPU
	}

	if (bAnyLumenActive)
	{
		FLumenSceneData& LumenSceneData = *Scene->GetLumenSceneData(Views[0]);
		TArray<FCardPageRenderData, SceneRenderingAllocator>& CardPagesToRender = LumenCardRenderer.CardPagesToRender;

		QUICK_SCOPE_CYCLE_COUNTER(UpdateLumenScene);
		RDG_RHI_GPU_STAT_SCOPE(GraphBuilder, UpdateLumenSceneBuffers);
		RDG_GPU_STAT_SCOPE(GraphBuilder, LumenSceneUpdate);
		RDG_EVENT_SCOPE(GraphBuilder, "LumenSceneUpdate: %u card captures %.3fM texels", CardPagesToRender.Num(), LumenCardRenderer.NumCardTexelsToCapture / (1024.0f * 1024.0f));

		LumenCardRenderer.bPropagateGlobalLightingChange = UpdateGlobalLightingState(Scene, Views[0], LumenSceneData);

		Lumen::UpdateCardSceneBuffer(GraphBuilder, FrameTemporaries, ViewFamily, Scene);

		// Init transient render targets for capturing cards
		FCardCaptureAtlas CardCaptureAtlas;
		AllocateCardCaptureAtlas(GraphBuilder, LumenSceneData.GetCardCaptureAtlasSize(), CardCaptureAtlas);

		if (CardPagesToRender.Num() > 0)
		{
			FRHIBuffer* PrimitiveIdVertexBuffer = nullptr;
			FInstanceCullingResult InstanceCullingResult;
			FInstanceCullingContext* InstanceCullingContext = nullptr;
			if (Scene->GPUScene.IsEnabled())
			{
				InstanceCullingContext = GraphBuilder.AllocObject<FInstanceCullingContext>(Views[0].GetFeatureLevel(), nullptr, TArrayView<const int32>(&Views[0].GPUSceneViewId, 1), nullptr);
				
				int32 MaxInstances = 0;
				int32 VisibleMeshDrawCommandsNum = 0;
				int32 NewPassVisibleMeshDrawCommandsNum = 0;
				
				InstanceCullingContext->SetupDrawCommands(LumenCardRenderer.MeshDrawCommands, false, MaxInstances, VisibleMeshDrawCommandsNum, NewPassVisibleMeshDrawCommandsNum);
				// Not supposed to do any compaction here.
				ensure(VisibleMeshDrawCommandsNum == LumenCardRenderer.MeshDrawCommands.Num());

				InstanceCullingContext->BuildRenderingCommands(GraphBuilder, Scene->GPUScene, Views[0].DynamicPrimitiveCollector.GetInstanceSceneDataOffset(), Views[0].DynamicPrimitiveCollector.NumInstances(), InstanceCullingResult);
			}
			else
			{
				// Prepare primitive Id VB for rendering mesh draw commands.
				if (LumenCardRenderer.MeshDrawPrimitiveIds.Num() > 0)
				{
					const uint32 PrimitiveIdBufferDataSize = LumenCardRenderer.MeshDrawPrimitiveIds.Num() * sizeof(int32);

					FPrimitiveIdVertexBufferPoolEntry Entry = GPrimitiveIdVertexBufferPool.Allocate(GraphBuilder.RHICmdList, PrimitiveIdBufferDataSize);
					PrimitiveIdVertexBuffer = Entry.BufferRHI;

					void* RESTRICT Data = RHILockBuffer(PrimitiveIdVertexBuffer, 0, PrimitiveIdBufferDataSize, RLM_WriteOnly);
					FMemory::Memcpy(Data, LumenCardRenderer.MeshDrawPrimitiveIds.GetData(), PrimitiveIdBufferDataSize);
					RHIUnlockBuffer(PrimitiveIdVertexBuffer);

					GPrimitiveIdVertexBufferPool.ReturnToFreeList(Entry);
				}
			}

			FRDGBufferRef CardCaptureRectBuffer = nullptr;
			FRDGBufferSRVRef CardCaptureRectBufferSRV = nullptr;

			{
				FRDGUploadData<FUintVector4> CardCaptureRectArray(GraphBuilder, CardPagesToRender.Num());

				for (int32 Index = 0; Index < CardPagesToRender.Num(); Index++)
				{
					const FCardPageRenderData& CardPageRenderData = CardPagesToRender[Index];

					FUintVector4& Rect = CardCaptureRectArray[Index];
					Rect.X = FMath::Max(CardPageRenderData.CardCaptureAtlasRect.Min.X, 0);
					Rect.Y = FMath::Max(CardPageRenderData.CardCaptureAtlasRect.Min.Y, 0);
					Rect.Z = FMath::Max(CardPageRenderData.CardCaptureAtlasRect.Max.X, 0);
					Rect.W = FMath::Max(CardPageRenderData.CardCaptureAtlasRect.Max.Y, 0);
				}

				CardCaptureRectBuffer =
					CreateUploadBuffer(GraphBuilder, TEXT("Lumen.CardCaptureRects"),
						sizeof(FUintVector4), FMath::RoundUpToPowerOfTwo(CardPagesToRender.Num()),
						CardCaptureRectArray);
				CardCaptureRectBufferSRV = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(CardCaptureRectBuffer, PF_R32G32B32A32_UINT));

				ClearLumenCardCapture(GraphBuilder, Views[0].ShaderMap, CardCaptureAtlas, CardCaptureRectBufferSRV, CardPagesToRender.Num());
			}

			FViewInfo* SharedView = Views[0].CreateSnapshot();
			{
				SharedView->DynamicPrimitiveCollector = FGPUScenePrimitiveCollector(&GetGPUSceneDynamicContext());
				SharedView->StereoPass = EStereoscopicPass::eSSP_FULL;
				SharedView->DrawDynamicFlags = EDrawDynamicFlags::ForceLowestLOD;

				// Don't do material texture mip biasing in proxy card rendering
				SharedView->MaterialTextureMipBias = 0;

				TRefCountPtr<IPooledRenderTarget> NullRef;
				FPlatformMemory::Memcpy(&SharedView->PrevViewInfo.HZB, &NullRef, sizeof(SharedView->PrevViewInfo.HZB));

				SharedView->CachedViewUniformShaderParameters = MakeUnique<FViewUniformShaderParameters>();
				SharedView->CachedViewUniformShaderParameters->PrimitiveSceneData = Scene->GPUScene.PrimitiveBuffer->GetSRV();
				SharedView->CachedViewUniformShaderParameters->InstanceSceneData = Scene->GPUScene.InstanceSceneDataBuffer->GetSRV();
				SharedView->CachedViewUniformShaderParameters->InstancePayloadData = Scene->GPUScene.InstancePayloadDataBuffer->GetSRV();
				SharedView->CachedViewUniformShaderParameters->LightmapSceneData = Scene->GPUScene.LightmapDataBuffer->GetSRV();
				SharedView->CachedViewUniformShaderParameters->InstanceSceneDataSOAStride = Scene->GPUScene.InstanceSceneDataSOAStride;

				SharedView->ViewUniformBuffer = TUniformBufferRef<FViewUniformShaderParameters>::CreateUniformBufferImmediate(*SharedView->CachedViewUniformShaderParameters, UniformBuffer_SingleFrame);
			}

			FLumenCardPassUniformParameters* PassUniformParameters = GraphBuilder.AllocParameters<FLumenCardPassUniformParameters>();
			SetupSceneTextureUniformParameters(GraphBuilder, &GetActiveSceneTextures(), Scene->GetFeatureLevel(), /*SceneTextureSetupMode*/ ESceneTextureSetupMode::None, PassUniformParameters->SceneTextures);
			PassUniformParameters->EyeAdaptationTexture = GetEyeAdaptationTexture(GraphBuilder, Views[0]);

			{
				uint32 NumPages = 0;
				uint32 NumDraws = 0;
				uint32 NumInstances = 0;
				uint32 NumTris = 0;

				// Compute some stats about non Nanite meshes which are captured
				#if RDG_EVENTS != RDG_EVENTS_NONE
				{
					for (FCardPageRenderData& CardPageRenderData : CardPagesToRender)
					{
						if (CardPageRenderData.NumMeshDrawCommands > 0)
						{
							NumPages += 1;
							NumDraws += CardPageRenderData.NumMeshDrawCommands;

							for (int32 DrawCommandIndex = CardPageRenderData.StartMeshDrawCommandIndex; DrawCommandIndex < CardPageRenderData.StartMeshDrawCommandIndex + CardPageRenderData.NumMeshDrawCommands; ++DrawCommandIndex)
							{
								const FVisibleMeshDrawCommand& VisibleDrawCommand = LumenCardRenderer.MeshDrawCommands[DrawCommandIndex];
								const FMeshDrawCommand* MeshDrawCommand = VisibleDrawCommand.MeshDrawCommand;

								uint32 NumInstancesPerDraw = 0;

								// Count number of instances to draw
								if (VisibleDrawCommand.NumRuns)
								{
									for (int32 InstanceRunIndex = 0; InstanceRunIndex < VisibleDrawCommand.NumRuns; ++InstanceRunIndex)
									{
										const int32 FirstInstance = VisibleDrawCommand.RunArray[InstanceRunIndex * 2 + 0];
										const int32 LastInstance = VisibleDrawCommand.RunArray[InstanceRunIndex * 2 + 1];
										NumInstancesPerDraw += LastInstance - FirstInstance + 1;
									}
								}
								else
								{
									NumInstancesPerDraw += MeshDrawCommand->NumInstances;
								}

								NumInstances += NumInstancesPerDraw;
								NumTris += MeshDrawCommand->NumPrimitives * NumInstancesPerDraw;
							}
						}
					}
				}
				#endif

				TRACE_CPUPROFILER_EVENT_SCOPE(CardPageRenderPasses);

				FLumenCardPassParameters* CommonPassParameters = GraphBuilder.AllocParameters<FLumenCardPassParameters>();
				CommonPassParameters->CardPass = GraphBuilder.CreateUniformBuffer(PassUniformParameters);
				CommonPassParameters->RenderTargets[0] = FRenderTargetBinding(CardCaptureAtlas.Albedo, ERenderTargetLoadAction::ELoad);
				CommonPassParameters->RenderTargets[1] = FRenderTargetBinding(CardCaptureAtlas.Normal, ERenderTargetLoadAction::ELoad);
				CommonPassParameters->RenderTargets[2] = FRenderTargetBinding(CardCaptureAtlas.Emissive, ERenderTargetLoadAction::ELoad);
				CommonPassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(CardCaptureAtlas.DepthStencil, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthWrite_StencilNop);

				InstanceCullingResult.GetDrawParameters(CommonPassParameters->InstanceCullingDrawParams);

				for (FCardPageRenderData& CardPageRenderData : CardPagesToRender)
				{
					RDG_EVENT_SCOPE(GraphBuilder, "MeshCardCapture Pages:%u Draws:%u Instances:%u Tris:%u", NumPages, NumDraws, NumInstances, NumTris);

					if (CardPageRenderData.NumMeshDrawCommands > 0)
					{
						CardPageRenderData.PatchView(Scene, SharedView);

						FLumenCardPassParameters* PassParameters = GraphBuilder.AllocParameters<FLumenCardPassParameters>(CommonPassParameters);
						PassParameters->View = GraphBuilder.CreateUniformBuffer(GraphBuilder.AllocParameters(SharedView->CachedViewUniformShaderParameters.Get()));

						GraphBuilder.AddPass(
							RDG_EVENT_NAME("CardPage Commands:%u", CardPageRenderData.NumMeshDrawCommands),
							PassParameters,
							ERDGPassFlags::Raster,
							[this, Scene = Scene, PrimitiveIdVertexBuffer, &CardPageRenderData, PassParameters, InstanceCullingContext](FRHICommandList& RHICmdList)
						{
							QUICK_SCOPE_CYCLE_COUNTER(MeshPass);

							const FIntRect ViewRect = CardPageRenderData.CardCaptureAtlasRect;
							RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, ViewRect.Max.X, ViewRect.Max.Y, 1.0f);

							FGraphicsMinimalPipelineStateSet GraphicsMinimalPipelineStateSet;
							if (Scene->GPUScene.IsEnabled())
							{
								FInstanceCullingDrawParams& InstanceCullingDrawParams = PassParameters->InstanceCullingDrawParams;

								InstanceCullingContext->SubmitDrawCommands(
									LumenCardRenderer.MeshDrawCommands,
									GraphicsMinimalPipelineStateSet,
									GetMeshDrawCommandOverrideArgs(PassParameters->InstanceCullingDrawParams),
									CardPageRenderData.StartMeshDrawCommandIndex,
									CardPageRenderData.NumMeshDrawCommands,
									1,
									RHICmdList);
							}
							else
							{
								SubmitMeshDrawCommandsRange(
									LumenCardRenderer.MeshDrawCommands,
									GraphicsMinimalPipelineStateSet,
									PrimitiveIdVertexBuffer,
									FInstanceCullingContext::GetInstanceIdBufferStride(Scene->GetFeatureLevel()),
									0,
									false,
									CardPageRenderData.StartMeshDrawCommandIndex,
									CardPageRenderData.NumMeshDrawCommands,
									1,
									RHICmdList);
							}
						});
					}
				}
			}

			bool bAnyNaniteMeshes = false;

			for (FCardPageRenderData& CardPageRenderData : CardPagesToRender)
			{
				if (CardPageRenderData.NaniteCommandInfos.Num() > 0 && CardPageRenderData.NaniteInstanceIds.Num() > 0)
				{
					bAnyNaniteMeshes = true;
					break;
				}
			}

			if (UseNanite(ShaderPlatform) && ViewFamily.EngineShowFlags.NaniteMeshes && bAnyNaniteMeshes)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(NaniteMeshPass);
				QUICK_SCOPE_CYCLE_COUNTER(NaniteMeshPass);

				const FIntPoint DepthStencilAtlasSize = CardCaptureAtlas.Size;
				const FIntRect DepthAtlasRect = FIntRect(0, 0, DepthStencilAtlasSize.X, DepthStencilAtlasSize.Y);

				Nanite::FSharedContext SharedContext{};
				SharedContext.FeatureLevel = Scene->GetFeatureLevel();
				SharedContext.ShaderMap = GetGlobalShaderMap(SharedContext.FeatureLevel);
				SharedContext.Pipeline = Nanite::EPipeline::Lumen;

				Nanite::FRasterContext RasterContext = Nanite::InitRasterContext(
					GraphBuilder,
					SharedContext,
					DepthStencilAtlasSize,
					false,
					Nanite::EOutputBufferMode::VisBuffer,
					true,
					CardCaptureRectBufferSRV,
					CardPagesToRender.Num());

				Nanite::FCullingContext::FConfiguration CullingConfig = { 0 };
				CullingConfig.bSupportsMultiplePasses	= true;
				CullingConfig.bForceHWRaster			= RasterContext.RasterScheduling == Nanite::ERasterScheduling::HardwareOnly;
				CullingConfig.SetViewFlags(*SharedView);
				CullingConfig.bIsLumenCapture = true;
				CullingConfig.bProgrammableRaster = GNaniteProgrammableRasterLumen != 0;

				Nanite::FCullingContext CullingContext = Nanite::InitCullingContext(
					GraphBuilder,
					SharedContext,
					*Scene,
					nullptr,
					FIntRect(),
					CullingConfig
				);

				FNaniteVisibilityResults VisibilityResults; // No material visibility culling for Lumen at this time

				const uint32 NumCardPagesToRender = CardPagesToRender.Num();

				uint32 NextCardIndex = 0;
				while(NextCardIndex < NumCardPagesToRender)
				{
					TArray<Nanite::FPackedView, SceneRenderingAllocator> NaniteViews;
					TArray<Nanite::FInstanceDraw, SceneRenderingAllocator> NaniteInstanceDraws;

					while(NextCardIndex < NumCardPagesToRender && NaniteViews.Num() < NANITE_MAX_VIEWS_PER_CULL_RASTERIZE_PASS)
					{
						const FCardPageRenderData& CardPageRenderData = CardPagesToRender[NextCardIndex];

						if(CardPageRenderData.NaniteInstanceIds.Num() > 0)
						{
							for(uint32 InstanceID : CardPageRenderData.NaniteInstanceIds)
							{
								NaniteInstanceDraws.Add(Nanite::FInstanceDraw { InstanceID, (uint32)NaniteViews.Num() });
							}

							Nanite::FPackedViewParams Params;
							Params.ViewMatrices = CardPageRenderData.ViewMatrices;
							Params.PrevViewMatrices = CardPageRenderData.ViewMatrices;
							Params.ViewRect = CardPageRenderData.CardCaptureAtlasRect;
							Params.RasterContextSize = DepthStencilAtlasSize;
							Params.LODScaleFactor = CardPageRenderData.NaniteLODScaleFactor;
							NaniteViews.Add(Nanite::CreatePackedView(Params));
						}

						NextCardIndex++;
					}

					if (NaniteInstanceDraws.Num() > 0)
					{
						RDG_EVENT_SCOPE(GraphBuilder, "Nanite::RasterizeLumenCards");

						Nanite::FRasterState RasterState;
						Nanite::CullRasterize(
							GraphBuilder,
							Scene->NaniteRasterPipelines[ENaniteMeshPass::BasePass],
							VisibilityResults,
							*Scene,
							*SharedView,
							NaniteViews,
							SharedContext,
							CullingContext,
							RasterContext,
							RasterState,
							&NaniteInstanceDraws
						);
					}
				}

				extern float GLumenDistantSceneMinInstanceBoundsRadius;

				// Render entire scene for distant cards
				for (FCardPageRenderData& CardPageRenderData : CardPagesToRender)
				{
					if (CardPageRenderData.bDistantScene)
					{
						Nanite::FRasterState RasterState;

						CardPageRenderData.PatchView(Scene, SharedView);
						Nanite::FPackedView PackedView = Nanite::CreatePackedViewFromViewInfo(
							*SharedView,
							DepthStencilAtlasSize,
							/* Flags */ 0u, // Near clip is intentionally disabled here
							/*StreamingPriorityCategory*/ 0,
							GLumenDistantSceneMinInstanceBoundsRadius,
							Lumen::GetDistanceSceneNaniteLODScaleFactor());

						Nanite::CullRasterize(
							GraphBuilder,
							Scene->NaniteRasterPipelines[ENaniteMeshPass::BasePass],
							VisibilityResults,
							*Scene,
							*SharedView,
							{ PackedView },
							SharedContext,
							CullingContext,
							RasterContext,
							RasterState);
					}
				}

				if (CVarLumenSceneSurfaceCacheCaptureNaniteMultiView.GetValueOnRenderThread() != 0)
				{
					Nanite::DrawLumenMeshCapturePass(
						GraphBuilder,
						*Scene,
						SharedView,
						TArrayView<const FCardPageRenderData>(CardPagesToRender),
						CullingContext,
						RasterContext,
						PassUniformParameters,
						CardCaptureRectBufferSRV,
						CardPagesToRender.Num(),
						CardCaptureAtlas.Size,
						CardCaptureAtlas.Albedo,
						CardCaptureAtlas.Normal,
						CardCaptureAtlas.Emissive,
						CardCaptureAtlas.DepthStencil
					);
				}
				else
				{
					// Single capture per card. Slow path, only for debugging.
					for (int32 PageIndex = 0; PageIndex < CardPagesToRender.Num(); ++PageIndex)
					{
						if (CardPagesToRender[PageIndex].NaniteCommandInfos.Num() > 0)
						{
							Nanite::DrawLumenMeshCapturePass(
								GraphBuilder,
								*Scene,
								SharedView,
								TArrayView<const FCardPageRenderData>(&CardPagesToRender[PageIndex], 1),
								CullingContext,
								RasterContext,
								PassUniformParameters,
								CardCaptureRectBufferSRV,
								CardPagesToRender.Num(),
								CardCaptureAtlas.Size,
								CardCaptureAtlas.Albedo,
								CardCaptureAtlas.Normal,
								CardCaptureAtlas.Emissive,
								CardCaptureAtlas.DepthStencil
							);
						}
					}
				}
			}

			UpdateLumenSurfaceCacheAtlas(
				GraphBuilder,
				Views[0],
				FrameTemporaries,
				CardPagesToRender,
				CardCaptureRectBufferSRV,
				CardCaptureAtlas,
				LumenCardRenderer.ResampledCardCaptureAtlas);
		}
	}

	UpdateLumenCardSceneUniformBuffer(GraphBuilder, Scene, *Scene->GetLumenSceneData(Views[0]), FrameTemporaries);

	// Reset arrays, but keep allocated memory for 1024 elements
	FLumenSceneData& LumenSceneData = *Scene->GetLumenSceneData(Views[0]);
	LumenSceneData.CardIndicesToUpdateInBuffer.Empty(1024);
	LumenSceneData.MeshCardsIndicesToUpdateInBuffer.Empty(1024);
}
