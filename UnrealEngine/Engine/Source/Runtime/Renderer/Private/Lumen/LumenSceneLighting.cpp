// Copyright Epic Games, Inc. All Rights Reserved.

#include "LumenSceneLighting.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "SceneTextureParameters.h"
#include "LumenMeshCards.h"
#include "LumenRadianceCache.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "LumenTracingUtils.h"
#include "ShaderPrintParameters.h"
#include "LumenRadiosity.h"

static TAutoConsoleVariable<int32> CVarLumenSceneLightingForceFullUpdate(
	TEXT("r.LumenScene.Lighting.ForceLightingUpdate"),
	0,
	TEXT("Force full Lumen Scene Lighting update every frame. Useful for debugging"),
	ECVF_RenderThreadSafe
);

int32 GLumenSceneLightingFeedback = 1;
FAutoConsoleVariableRef CVarLumenSceneLightingFeedback(
	TEXT("r.LumenScene.Lighting.Feedback"),
	GLumenSceneLightingFeedback,
	TEXT("Whether to prioritize surface cache lighting updates based on the feedback."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenDirectLightingUpdateFactor = 32;
FAutoConsoleVariableRef CVarLumenSceneDirectLightingUpdateFactor(
	TEXT("r.LumenScene.DirectLighting.UpdateFactor"),
	GLumenDirectLightingUpdateFactor,
	TEXT("Controls for how many texels direct lighting will be updated every frame. Texels = SurfaceCacheTexels / Factor."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenRadiosityUpdateFactor = 64;
FAutoConsoleVariableRef CVarLumenSceneRadiosityUpdateFactor(
	TEXT("r.LumenScene.Radiosity.UpdateFactor"),
	GLumenRadiosityUpdateFactor,
	TEXT("Controls for how many texels radiosity will be updated every frame. Texels = SurfaceCacheTexels / Factor."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenLightingStats = 0;
FAutoConsoleVariableRef CVarLumenSceneLightingStats(
	TEXT("r.LumenScene.Lighting.Stats"),
	GLumenLightingStats,
	TEXT("GPU print out Lumen lighting update stats."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenSceneLightingAsyncCompute(
	TEXT("r.LumenScene.Lighting.AsyncCompute"),
	1,
	TEXT("Whether to run LumenSceneLighting on the compute pipe if possible."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

namespace LumenSceneLighting
{
	bool UseFeedback(const FSceneViewFamily& ViewFamily)
	{
		return Lumen::UseHardwareRayTracing(ViewFamily) && GLumenSceneLightingFeedback != 0;
	}
}

bool Lumen::UseHardwareRayTracedSceneLighting(const FSceneViewFamily& ViewFamily)
{
	return Lumen::UseHardwareRayTracedDirectLighting(ViewFamily) || Lumen::UseHardwareRayTracedRadiosity(ViewFamily);
}

namespace LumenCardUpdateContext
{
	// Must match LumenSceneLighting.usf
	constexpr uint32 CARD_UPDATE_CONTEXT_MAX = 2;
	constexpr uint32 PRIORITY_HISTOGRAM_SIZE = 16;
	constexpr uint32 MAX_UPDATE_BUCKET_STRIDE = 2;
	constexpr uint32 CARD_PAGE_TILE_ALLOCATOR_STRIDE = 2;
};

void SetLightingUpdateAtlasSize(FIntPoint PhysicalAtlasSize, int32 UpdateFactor, FLumenCardUpdateContext& Context)
{
	Context.UpdateAtlasSize = FIntPoint(0, 0);
	Context.MaxUpdateTiles = 0;
	Context.UpdateFactor = FMath::Clamp(UpdateFactor, 1, 1024);

	if (!Lumen::IsSurfaceCacheFrozen())
	{
		if (CVarLumenSceneLightingForceFullUpdate.GetValueOnRenderThread() != 0)
		{
			Context.UpdateFactor = 1;
		}

		const float MultPerComponent = 1.0f / FMath::Sqrt((float)Context.UpdateFactor);

		FIntPoint UpdateAtlasSize;
		UpdateAtlasSize.X = FMath::DivideAndRoundUp<uint32>(PhysicalAtlasSize.X * MultPerComponent + 0.5f, Lumen::CardTileSize) * Lumen::CardTileSize;
		UpdateAtlasSize.Y = FMath::DivideAndRoundUp<uint32>(PhysicalAtlasSize.Y * MultPerComponent + 0.5f, Lumen::CardTileSize) * Lumen::CardTileSize;

		// Update at least one full res card page so that we don't get stuck
		UpdateAtlasSize.X = FMath::Max<int32>(UpdateAtlasSize.X, Lumen::PhysicalPageSize);
		UpdateAtlasSize.Y = FMath::Max<int32>(UpdateAtlasSize.Y, Lumen::PhysicalPageSize);

		const FIntPoint UpdateAtlasSizeInTiles = UpdateAtlasSize / Lumen::CardTileSize;

		Context.UpdateAtlasSize = UpdateAtlasSize;
		Context.MaxUpdateTiles = UpdateAtlasSizeInTiles.X * UpdateAtlasSizeInTiles.Y;
	}
}

IMPLEMENT_GLOBAL_SHADER(FClearLumenCardsPS, "/Engine/Private/Lumen/LumenSceneLighting.usf", "ClearLumenCardsPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FCopyCardCaptureLightingToAtlasPS, "/Engine/Private/Lumen/LumenSceneLighting.usf", "CopyCardCaptureLightingToAtlasPS", SF_Pixel);

bool FRasterizeToCardsVS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return DoesPlatformSupportLumenGI(Parameters.Platform);
}

IMPLEMENT_GLOBAL_SHADER(FRasterizeToCardsVS,"/Engine/Private/Lumen/LumenSceneLighting.usf","RasterizeToCardsVS",SF_Vertex);

class FLumenCardCombineLightingCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenCardCombineLightingCS);
	SHADER_USE_PARAMETER_STRUCT(FLumenCardCombineLightingCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectArgsBuffer, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenCardScene, LumenCardScene)
		SHADER_PARAMETER(float, DiffuseColorBoost)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, AlbedoAtlas)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, OpacityAtlas)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, EmissiveAtlas)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DirectLightingAtlas)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, IndirectLightingAtlas)
		SHADER_PARAMETER_SAMPLER(SamplerState, BilinearClampedSampler)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CardTiles)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWFinalLightingAtlas)
		SHADER_PARAMETER(FVector2f, IndirectLightingAtlasHalfTexelSize)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLumenCardCombineLightingCS, "/Engine/Private/Lumen/LumenSceneLighting.usf", "CombineLumenSceneLightingCS", SF_Compute);

void Lumen::CombineLumenSceneLighting(
	FScene* Scene,
	const FViewInfo& View,
	FRDGBuilder& GraphBuilder,
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	const FLumenCardUpdateContext& CardUpdateContext,
	const FLumenCardTileUpdateContext& CardTileUpdateContext,
	ERDGPassFlags ComputePassFlags)
{
	LLM_SCOPE_BYTAG(Lumen);
	FLumenSceneData& LumenSceneData = *Scene->GetLumenSceneData(View);

	FLumenCardCombineLightingCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenCardCombineLightingCS::FParameters>();
	PassParameters->IndirectArgsBuffer = CardTileUpdateContext.DispatchCardTilesIndirectArgs;
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->LumenCardScene = FrameTemporaries.LumenCardSceneUniformBuffer;
	PassParameters->DiffuseColorBoost = 1.0f / FMath::Max(View.FinalPostProcessSettings.LumenDiffuseColorBoost, 1.0f);
	PassParameters->AlbedoAtlas = FrameTemporaries.AlbedoAtlas;
	PassParameters->OpacityAtlas = FrameTemporaries.OpacityAtlas;
	PassParameters->EmissiveAtlas = FrameTemporaries.EmissiveAtlas;
	PassParameters->DirectLightingAtlas = FrameTemporaries.DirectLightingAtlas;
	PassParameters->IndirectLightingAtlas = FrameTemporaries.IndirectLightingAtlas;
	PassParameters->BilinearClampedSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->CardTiles = GraphBuilder.CreateSRV(CardTileUpdateContext.CardTiles);
	PassParameters->RWFinalLightingAtlas = GraphBuilder.CreateUAV(FrameTemporaries.FinalLightingAtlas);
	const FIntPoint IndirectLightingAtlasSize = LumenSceneData.GetRadiosityAtlasSize();
	PassParameters->IndirectLightingAtlasHalfTexelSize = FVector2f(0.5f / IndirectLightingAtlasSize.X, 0.5f / IndirectLightingAtlasSize.Y);

	auto ComputeShader = View.ShaderMap->GetShader<FLumenCardCombineLightingCS>();

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("CombineLighting CS"),
		ComputePassFlags,
		ComputeShader,
		PassParameters,
		CardTileUpdateContext.DispatchCardTilesIndirectArgs,
		(uint32)ELumenDispatchCardTilesIndirectArgsOffset::OneGroupPerCardTile);
}

bool LumenSceneLighting::UseAsyncCompute(const FViewFamilyInfo& ViewFamily)
{
	return Lumen::UseAsyncCompute(ViewFamily) && CVarLumenSceneLightingAsyncCompute.GetValueOnRenderThread() != 0;
}

DECLARE_GPU_STAT(LumenSceneLighting);

void FDeferredShadingSceneRenderer::RenderLumenSceneLighting(
	FRDGBuilder& GraphBuilder,
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	const FLumenDirectLightingTaskData* DirectLightingTaskData)
{
	LLM_SCOPE_BYTAG(Lumen);
	TRACE_CPUPROFILER_EVENT_SCOPE(FDeferredShadingSceneRenderer::RenderLumenSceneLighting);

	FLumenSceneData& LumenSceneData = *Scene->GetLumenSceneData(Views[0]);

	bool bAnyLumenActive = false;

	for (const FViewInfo& View : Views)
	{
		const FPerViewPipelineState& ViewPipelineState = GetViewPipelineState(View);
		bAnyLumenActive = bAnyLumenActive || ViewPipelineState.DiffuseIndirectMethod == EDiffuseIndirectMethod::Lumen;
	}

	if (bAnyLumenActive)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RenderLumenSceneLighting);
		QUICK_SCOPE_CYCLE_COUNTER(RenderLumenSceneLighting);
		RDG_EVENT_SCOPE(GraphBuilder, "LumenSceneLighting%s", LumenCardRenderer.bPropagateGlobalLightingChange ? TEXT(" PROPAGATE GLOBAL CHANGE!") : TEXT(""));
		RDG_GPU_STAT_SCOPE(GraphBuilder, LumenSceneLighting);

		const ERDGPassFlags ComputePassFlags = LumenSceneLighting::UseAsyncCompute(ViewFamily) ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute;

		LumenSceneData.IncrementSurfaceCacheUpdateFrameIndex();

		if (LumenSceneData.GetNumCardPages() > 0)
		{
			if (LumenSceneData.bDebugClearAllCachedState)
			{
				AddClearRenderTargetPass(GraphBuilder, FrameTemporaries.DirectLightingAtlas);
				AddClearRenderTargetPass(GraphBuilder, FrameTemporaries.IndirectLightingAtlas);
				AddClearRenderTargetPass(GraphBuilder, FrameTemporaries.RadiosityNumFramesAccumulatedAtlas);
				AddClearRenderTargetPass(GraphBuilder, FrameTemporaries.FinalLightingAtlas);
			}

			LumenRadiosity::FFrameTemporaries RadiosityFrameTemporaries;
			LumenRadiosity::InitFrameTemporaries(GraphBuilder, LumenSceneData, ViewFamily, Views, RadiosityFrameTemporaries);

			FLumenCardUpdateContext DirectLightingCardUpdateContext;
			FLumenCardUpdateContext IndirectLightingCardUpdateContext;
			Lumen::BuildCardUpdateContext(
				GraphBuilder,
				LumenSceneData,
				Views,
				FrameTemporaries,
				RadiosityFrameTemporaries.bIndirectLightingHistoryValid,
				DirectLightingCardUpdateContext,
				IndirectLightingCardUpdateContext,
				ComputePassFlags);

			RenderDirectLightingForLumenScene(
				GraphBuilder,
				FrameTemporaries,
				DirectLightingTaskData,
				DirectLightingCardUpdateContext,
				ComputePassFlags);

			RenderRadiosityForLumenScene(
				GraphBuilder,
				FrameTemporaries,
				RadiosityFrameTemporaries,
				IndirectLightingCardUpdateContext,
				ComputePassFlags);

			LumenSceneData.bFinalLightingAtlasContentsValid = true;
		}
	}
}

class FClearCardUpdateContextCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClearCardUpdateContextCS);
	SHADER_USE_PARAMETER_STRUCT(FClearCardUpdateContextCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWDirectLightingCardPageIndexAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWIndirectLightingCardPageIndexAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWMaxUpdateBucket)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWCardPageTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWPriorityHistogram)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

	static int32 GetGroupSize()
	{
		return 64;
	}
};

IMPLEMENT_GLOBAL_SHADER(FClearCardUpdateContextCS, "/Engine/Private/Lumen/LumenSceneLighting.usf", "ClearCardUpdateContextCS", SF_Compute);

class FBuildPageUpdatePriorityHistogramCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FBuildPageUpdatePriorityHistogramCS);
	SHADER_USE_PARAMETER_STRUCT(FBuildPageUpdatePriorityHistogramCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWPriorityHistogram)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenCardScene, LumenCardScene)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CardPageLastUsedBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CardPageHighResLastUsedBuffer)
		SHADER_PARAMETER(uint32, SurfaceCacheUpdateFrameIndex)
		SHADER_PARAMETER(uint32, FreezeUpdateFrame)
		SHADER_PARAMETER(uint32, CardPageNum)
		SHADER_PARAMETER(float, FirstClipmapWorldExtentRcp)
		SHADER_PARAMETER(uint32, NumCameraOrigins)
		SHADER_PARAMETER_ARRAY(FVector4f, WorldCameraOrigins, [LUMEN_MAX_VIEWS])
		SHADER_PARAMETER(float, DirectLightingUpdateFactor)
		SHADER_PARAMETER(float, IndirectLightingUpdateFactor)
	END_SHADER_PARAMETER_STRUCT()

	class FSurfaceCacheFeedback : SHADER_PERMUTATION_BOOL("SURFACE_CACHE_FEEDBACK");
	using FPermutationDomain = TShaderPermutationDomain<FSurfaceCacheFeedback>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

	static int32 GetGroupSize()
	{
		return 64;
	}
};

IMPLEMENT_GLOBAL_SHADER(FBuildPageUpdatePriorityHistogramCS, "/Engine/Private/Lumen/LumenSceneLighting.usf", "BuildPageUpdatePriorityHistogramCS", SF_Compute);

class FSelectMaxUpdateBucketCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSelectMaxUpdateBucketCS);
	SHADER_USE_PARAMETER_STRUCT(FSelectMaxUpdateBucketCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWMaxUpdateBucket)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, PriorityHistogram)
		SHADER_PARAMETER(uint32, MaxDirectLightingTilesToUpdate)
		SHADER_PARAMETER(uint32, MaxIndirectLightingTilesToUpdate)
		SHADER_PARAMETER(uint32, SurfaceCacheUpdateFrameIndex)
		SHADER_PARAMETER(uint32, FreezeUpdateFrame)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

	static int32 GetGroupSize()
	{
		return 64;
	}
};

IMPLEMENT_GLOBAL_SHADER(FSelectMaxUpdateBucketCS, "/Engine/Private/Lumen/LumenSceneLighting.usf", "SelectMaxUpdateBucketCS", SF_Compute);

class FBuildCardsUpdateListCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FBuildCardsUpdateListCS);
	SHADER_USE_PARAMETER_STRUCT(FBuildCardsUpdateListCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWDirectLightingCardPageIndexAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWDirectLightingCardPageIndexData)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWIndirectLightingCardPageIndexAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWIndirectLightingCardPageIndexData)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWCardPageTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, MaxUpdateBucket)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, LumenCardDataBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float4>, RWLumenCardPageDataBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CardPageLastUsedBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CardPageHighResLastUsedBuffer)
		SHADER_PARAMETER(uint32, SurfaceCacheUpdateFrameIndex)
		SHADER_PARAMETER(uint32, FreezeUpdateFrame)
		SHADER_PARAMETER(uint32, CardPageNum)
		SHADER_PARAMETER(float, FirstClipmapWorldExtentRcp)
		SHADER_PARAMETER(uint32, NumCameraOrigins)
		SHADER_PARAMETER_ARRAY(FVector4f, WorldCameraOrigins, [LUMEN_MAX_VIEWS])
		SHADER_PARAMETER(uint32, MaxDirectLightingTilesToUpdate)
		SHADER_PARAMETER(uint32, MaxIndirectLightingTilesToUpdate)
		SHADER_PARAMETER(float, DirectLightingUpdateFactor)
		SHADER_PARAMETER(float, IndirectLightingUpdateFactor)
		SHADER_PARAMETER(int32, IndirectLightingHistoryValid)
	END_SHADER_PARAMETER_STRUCT()

	class FSurfaceCacheFeedback : SHADER_PERMUTATION_BOOL("SURFACE_CACHE_FEEDBACK");
	using FPermutationDomain = TShaderPermutationDomain<FSurfaceCacheFeedback>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
		OutEnvironment.SetDefine(TEXT("USE_LUMEN_CARD_DATA_BUFFER"), 1);
		OutEnvironment.SetDefine(TEXT("USE_RW_LUMEN_CARD_PAGE_DATA_BUFFER"), 1);
	}

	static int32 GetGroupSize()
	{
		return 64;
	}
};

IMPLEMENT_GLOBAL_SHADER(FBuildCardsUpdateListCS, "/Engine/Private/Lumen/LumenSceneLighting.usf", "BuildCardsUpdateListCS", SF_Compute);

class FSetCardPageIndexIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSetCardPageIndexIndirectArgsCS);
	SHADER_USE_PARAMETER_STRUCT(FSetCardPageIndexIndirectArgsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWDirectLightingDrawCardPageIndicesIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWDirectLightingDispatchCardPageIndicesIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWIndirectLightingDrawCardPageIndicesIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWIndirectLightingDispatchCardPageIndicesIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, DirectLightingCardPageIndexAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, IndirectLightingCardPageIndexAllocator)
		SHADER_PARAMETER(uint32, VertexCountPerInstanceIndirect)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

	static int32 GetGroupSize()
	{
		return 64;
	}
};

IMPLEMENT_GLOBAL_SHADER(FSetCardPageIndexIndirectArgsCS, "/Engine/Private/Lumen/LumenSceneLighting.usf", "SetCardPageIndexIndirectArgsCS", SF_Compute);

class FLumenSceneLightingStatsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenSceneLightingStatsCS)
	SHADER_USE_PARAMETER_STRUCT(FLumenSceneLightingStatsCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenCardScene, LumenCardScene)
		SHADER_PARAMETER(uint32, CardPageNum)
		SHADER_PARAMETER(uint32, LightingStatMode)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, DirectLightingCardPageIndexAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, IndirectLightingCardPageIndexAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, PriorityHistogram)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, MaxUpdateBucket)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CardPageTileAllocator)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLumenSceneLightingStatsCS, "/Engine/Private/Lumen/LumenSceneLightingDebug.usf", "LumenSceneLightingStatsCS", SF_Compute);

void Lumen::BuildCardUpdateContext(
	FRDGBuilder& GraphBuilder,
	const FLumenSceneData& LumenSceneData,
	const TArray<FViewInfo>& Views,
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	bool bIndirectLightingHistoryValid,
	FLumenCardUpdateContext& DirectLightingCardUpdateContext,
	FLumenCardUpdateContext& IndirectLightingCardUpdateContext,
	ERDGPassFlags ComputePassFlags)
{
	RDG_EVENT_SCOPE(GraphBuilder, "BuildCardUpdateContext");

	float LumenSceneLightingUpdateSpeed = 0.0f;

	for (const FViewInfo& View : Views)
	{
		LumenSceneLightingUpdateSpeed = FMath::Max(LumenSceneLightingUpdateSpeed, FMath::Clamp<float>(View.FinalPostProcessSettings.LumenSceneLightingUpdateSpeed, .5f, 16.0f));
	}

	FRDGBufferSRV* CardPageLastUsedBufferSRV = nullptr;
	FRDGBufferSRV* CardPageHighResLastUsedBufferSRV = nullptr;

	const bool bUseFeedback = FrameTemporaries.CardPageLastUsedBufferSRV && FrameTemporaries.CardPageHighResLastUsedBufferSRV && LumenSceneLighting::UseFeedback(*Views[0].Family);
	if (bUseFeedback)
	{
		CardPageLastUsedBufferSRV = FrameTemporaries.CardPageLastUsedBufferSRV;
		CardPageHighResLastUsedBufferSRV = FrameTemporaries.CardPageHighResLastUsedBufferSRV;
	}

	const int32 NumCardPages = LumenSceneData.GetNumCardPages();
	const uint32 UpdateFrameIndex = LumenSceneData.GetSurfaceCacheUpdateFrameIndex();
	const uint32 FreezeUpdateFrame = Lumen::IsSurfaceCacheUpdateFrameFrozen() ? 1 : 0;
	const float FirstClipmapWorldExtentRcp = 1.0f / Lumen::GetGlobalDFClipmapExtent(0);

	SetLightingUpdateAtlasSize(LumenSceneData.GetPhysicalAtlasSize(), FMath::RoundToInt(GLumenDirectLightingUpdateFactor / LumenSceneLightingUpdateSpeed), DirectLightingCardUpdateContext);
	SetLightingUpdateAtlasSize(LumenSceneData.GetPhysicalAtlasSize(), FMath::RoundToInt(GLumenRadiosityUpdateFactor / LumenSceneLightingUpdateSpeed), IndirectLightingCardUpdateContext);

	DirectLightingCardUpdateContext.CardPageIndexAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("Lumen.DirectLightingCardPageIndexAllocator"));
	DirectLightingCardUpdateContext.CardPageIndexData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), FMath::RoundUpToPowerOfTwo(NumCardPages)), TEXT("Lumen.DirectLightingCardPageIndexData"));
	DirectLightingCardUpdateContext.DrawCardPageIndicesIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDrawIndirectParameters>(1), TEXT("Lumen.DirectLighting.DrawCardPageIndicesIndirectArgs"));
	DirectLightingCardUpdateContext.DispatchCardPageIndicesIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(FLumenCardUpdateContext::MAX), TEXT("Lumen.DirectLighting.DispatchCardPageIndicesIndirectArgs"));

	IndirectLightingCardUpdateContext.CardPageIndexAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("Lumen.IndirectLightingCardPageIndexAllocator"));
	IndirectLightingCardUpdateContext.CardPageIndexData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), FMath::RoundUpToPowerOfTwo(NumCardPages)), TEXT("Lumen.IndirectLightingCardPageIndexData"));
	IndirectLightingCardUpdateContext.DrawCardPageIndicesIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDrawIndirectParameters>(1), TEXT("Lumen.IndirectLighting.DrawCardPageIndicesIndirectArgs"));
	IndirectLightingCardUpdateContext.DispatchCardPageIndicesIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(FLumenCardUpdateContext::MAX), TEXT("Lumen.IndirectLighting.DispatchCardPageIndicesIndirectArgs"));

	FRDGBufferRef PriorityHistogram = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), LumenCardUpdateContext::CARD_UPDATE_CONTEXT_MAX * LumenCardUpdateContext::PRIORITY_HISTOGRAM_SIZE), TEXT("Lumen.PriorityHistogram"));
	FRDGBufferRef MaxUpdateBucket = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), LumenCardUpdateContext::CARD_UPDATE_CONTEXT_MAX * LumenCardUpdateContext::MAX_UPDATE_BUCKET_STRIDE), TEXT("Lumen.MaxUpdateBucket"));
	FRDGBufferRef CardPageTileAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), LumenCardUpdateContext::CARD_UPDATE_CONTEXT_MAX * LumenCardUpdateContext::CARD_PAGE_TILE_ALLOCATOR_STRIDE), TEXT("Lumen.CardPageTileAllocator"));

	FRDGBufferUAV* MaxUpdateBucketUAV = GraphBuilder.CreateUAV(MaxUpdateBucket);
	FRDGBufferSRV* MaxUpdateBucketSRV = GraphBuilder.CreateSRV(MaxUpdateBucket);

	FRDGBufferUAV* DirectCardPageIndexAllocatorUAV = GraphBuilder.CreateUAV(DirectLightingCardUpdateContext.CardPageIndexAllocator);
	FRDGBufferSRV* DirectCardPageIndexAllocatorSRV = GraphBuilder.CreateSRV(DirectLightingCardUpdateContext.CardPageIndexAllocator);

	FRDGBufferUAV* CardPageTileAllocatorUAV = GraphBuilder.CreateUAV(CardPageTileAllocator);
	FRDGBufferSRV* CardPageTileAllocatorSRV = GraphBuilder.CreateSRV(CardPageTileAllocator);

	FRDGBufferUAV* IndirectCardPageIndexAllocatorUAV = GraphBuilder.CreateUAV(IndirectLightingCardUpdateContext.CardPageIndexAllocator);
	FRDGBufferSRV* IndirectCardPageIndexAllocatorSRV = GraphBuilder.CreateSRV(IndirectLightingCardUpdateContext.CardPageIndexAllocator);

	FRDGBufferUAV* PriorityHistogramUAV = GraphBuilder.CreateUAV(PriorityHistogram);
	FRDGBufferSRV* PriorityHistogramSRV = GraphBuilder.CreateSRV(PriorityHistogram);

	// Batch clear all resources required for the subsequent card context update pass
	{
		FClearCardUpdateContextCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FClearCardUpdateContextCS::FParameters>();
		PassParameters->RWDirectLightingCardPageIndexAllocator = DirectCardPageIndexAllocatorUAV;
		PassParameters->RWIndirectLightingCardPageIndexAllocator = IndirectCardPageIndexAllocatorUAV;
		PassParameters->RWMaxUpdateBucket = MaxUpdateBucketUAV;
		PassParameters->RWCardPageTileAllocator = CardPageTileAllocatorUAV;
		PassParameters->RWPriorityHistogram = PriorityHistogramUAV;

		auto ComputeShader = Views[0].ShaderMap->GetShader<FClearCardUpdateContextCS>();

		const FIntVector GroupSize(FMath::DivideAndRoundUp<int32>(LumenCardUpdateContext::CARD_UPDATE_CONTEXT_MAX * LumenCardUpdateContext::PRIORITY_HISTOGRAM_SIZE, FClearCardUpdateContextCS::GetGroupSize()), 1, 1);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ClearCardUpdateContext"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			GroupSize);
	}

	// Prepare update priority histogram
	{
		FBuildPageUpdatePriorityHistogramCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FBuildPageUpdatePriorityHistogramCS::FParameters>();
		PassParameters->RWPriorityHistogram = PriorityHistogramUAV;
		PassParameters->LumenCardScene = FrameTemporaries.LumenCardSceneUniformBuffer;
		PassParameters->CardPageLastUsedBuffer = CardPageLastUsedBufferSRV;
		PassParameters->CardPageHighResLastUsedBuffer = CardPageHighResLastUsedBufferSRV;
		PassParameters->CardPageNum = NumCardPages;
		PassParameters->SurfaceCacheUpdateFrameIndex = UpdateFrameIndex;
		PassParameters->FreezeUpdateFrame = FreezeUpdateFrame;
		PassParameters->FirstClipmapWorldExtentRcp = FirstClipmapWorldExtentRcp;
		PassParameters->NumCameraOrigins = Views.Num();
		check(Views.Num() <= PassParameters->WorldCameraOrigins.Num());

		for (int32 i = 0; i < Views.Num(); i++)
		{
			PassParameters->WorldCameraOrigins[i] = FVector4f((FVector3f)Views[i].ViewMatrices.GetViewOrigin(), 0.0f);
		}

		PassParameters->DirectLightingUpdateFactor = DirectLightingCardUpdateContext.UpdateFactor;
		PassParameters->IndirectLightingUpdateFactor = IndirectLightingCardUpdateContext.UpdateFactor;

		FBuildPageUpdatePriorityHistogramCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FBuildPageUpdatePriorityHistogramCS::FSurfaceCacheFeedback>(bUseFeedback);
		auto ComputeShader = Views[0].ShaderMap->GetShader<FBuildPageUpdatePriorityHistogramCS>(PermutationVector);

		const FIntVector GroupSize(FMath::DivideAndRoundUp<int32>(LumenSceneData.GetNumCardPages(), FBuildPageUpdatePriorityHistogramCS::GetGroupSize()), 1, 1);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("BuildPageUpdatePriorityHistogram"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			GroupSize);
	}

	// Compute prefix sum and pick max bucket
	{
		FSelectMaxUpdateBucketCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSelectMaxUpdateBucketCS::FParameters>();
		PassParameters->RWMaxUpdateBucket = MaxUpdateBucketUAV;
		PassParameters->PriorityHistogram = PriorityHistogramSRV;
		PassParameters->MaxDirectLightingTilesToUpdate = DirectLightingCardUpdateContext.MaxUpdateTiles;
		PassParameters->MaxIndirectLightingTilesToUpdate = IndirectLightingCardUpdateContext.MaxUpdateTiles;
		PassParameters->SurfaceCacheUpdateFrameIndex = UpdateFrameIndex;
		PassParameters->FreezeUpdateFrame = FreezeUpdateFrame;

		auto ComputeShader = Views[0].ShaderMap->GetShader<FSelectMaxUpdateBucketCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Select max update bucket"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			FIntVector(2, 1, 1));
	}

	// Build list of tiles to update in this frame
	{
		FBuildCardsUpdateListCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FBuildCardsUpdateListCS::FParameters>();
		PassParameters->RWDirectLightingCardPageIndexAllocator = DirectCardPageIndexAllocatorUAV;
		PassParameters->RWDirectLightingCardPageIndexData = GraphBuilder.CreateUAV(DirectLightingCardUpdateContext.CardPageIndexData);
		PassParameters->RWIndirectLightingCardPageIndexAllocator = IndirectCardPageIndexAllocatorUAV;
		PassParameters->RWIndirectLightingCardPageIndexData = GraphBuilder.CreateUAV(IndirectLightingCardUpdateContext.CardPageIndexData);
		PassParameters->RWCardPageTileAllocator = CardPageTileAllocatorUAV;
		PassParameters->MaxUpdateBucket = MaxUpdateBucketSRV;
		PassParameters->LumenCardDataBuffer = FrameTemporaries.CardBufferSRV;
		PassParameters->RWLumenCardPageDataBuffer = FrameTemporaries.CardPageBufferUAV;
		PassParameters->CardPageLastUsedBuffer = CardPageLastUsedBufferSRV;
		PassParameters->CardPageHighResLastUsedBuffer = CardPageHighResLastUsedBufferSRV;
		PassParameters->CardPageNum = NumCardPages;
		PassParameters->SurfaceCacheUpdateFrameIndex = UpdateFrameIndex;
		PassParameters->FreezeUpdateFrame = FreezeUpdateFrame;
		PassParameters->FirstClipmapWorldExtentRcp = FirstClipmapWorldExtentRcp;
		PassParameters->NumCameraOrigins = Views.Num();
		PassParameters->IndirectLightingHistoryValid = bIndirectLightingHistoryValid ? 1 : 0;
		check(Views.Num() <= PassParameters->WorldCameraOrigins.Num());

		for (int32 i = 0; i < Views.Num(); i++)
		{
			PassParameters->WorldCameraOrigins[i] = FVector4f((FVector3f)Views[i].ViewMatrices.GetViewOrigin(), 0.0f);
		}

		PassParameters->MaxDirectLightingTilesToUpdate = DirectLightingCardUpdateContext.MaxUpdateTiles;
		PassParameters->MaxIndirectLightingTilesToUpdate = IndirectLightingCardUpdateContext.MaxUpdateTiles;
		PassParameters->DirectLightingUpdateFactor = DirectLightingCardUpdateContext.UpdateFactor;
		PassParameters->IndirectLightingUpdateFactor = IndirectLightingCardUpdateContext.UpdateFactor;

		FBuildCardsUpdateListCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FBuildCardsUpdateListCS::FSurfaceCacheFeedback>(bUseFeedback);
		auto ComputeShader = Views[0].ShaderMap->GetShader<FBuildCardsUpdateListCS>(PermutationVector);

		const FIntVector GroupSize(FMath::DivideAndRoundUp<int32>(LumenSceneData.GetNumCardPages(), FBuildCardsUpdateListCS::GetGroupSize()), 1, 1);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Build cards update list"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			GroupSize);
	}

	// Setup indirect args
	{
		FSetCardPageIndexIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSetCardPageIndexIndirectArgsCS::FParameters>();
		PassParameters->RWDirectLightingDrawCardPageIndicesIndirectArgs = GraphBuilder.CreateUAV(DirectLightingCardUpdateContext.DrawCardPageIndicesIndirectArgs);
		PassParameters->RWDirectLightingDispatchCardPageIndicesIndirectArgs = GraphBuilder.CreateUAV(DirectLightingCardUpdateContext.DispatchCardPageIndicesIndirectArgs);
		PassParameters->RWIndirectLightingDrawCardPageIndicesIndirectArgs = GraphBuilder.CreateUAV(IndirectLightingCardUpdateContext.DrawCardPageIndicesIndirectArgs);
		PassParameters->RWIndirectLightingDispatchCardPageIndicesIndirectArgs = GraphBuilder.CreateUAV(IndirectLightingCardUpdateContext.DispatchCardPageIndicesIndirectArgs);
		PassParameters->DirectLightingCardPageIndexAllocator = DirectCardPageIndexAllocatorSRV;
		PassParameters->IndirectLightingCardPageIndexAllocator = IndirectCardPageIndexAllocatorSRV;
		PassParameters->VertexCountPerInstanceIndirect = GRHISupportsRectTopology ? 3 : 6;

		auto ComputeShader = Views[0].ShaderMap->GetShader<FSetCardPageIndexIndirectArgsCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SetCardPageIndexIndirectArgs"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	if (GLumenLightingStats != 0)
	{
		ShaderPrint::SetEnabled(true);

		FLumenSceneLightingStatsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenSceneLightingStatsCS::FParameters>();
		ShaderPrint::SetParameters(GraphBuilder, Views[0].ShaderPrintData, PassParameters->ShaderPrintUniformBuffer);
		PassParameters->LumenCardScene = FrameTemporaries.LumenCardSceneUniformBuffer;
		PassParameters->DirectLightingCardPageIndexAllocator = DirectCardPageIndexAllocatorSRV;
		PassParameters->IndirectLightingCardPageIndexAllocator = IndirectCardPageIndexAllocatorSRV;
		PassParameters->PriorityHistogram = PriorityHistogramSRV;
		PassParameters->MaxUpdateBucket = MaxUpdateBucketSRV;
		PassParameters->CardPageTileAllocator = GraphBuilder.CreateSRV(CardPageTileAllocator);
		PassParameters->CardPageNum = LumenSceneData.GetNumCardPages();
		PassParameters->LightingStatMode = GLumenLightingStats;

		auto ComputeShader = Views[0].ShaderMap->GetShader<FLumenSceneLightingStatsCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SceneLightingStats"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}
}