// Copyright Epic Games, Inc. All Rights Reserved.

#include "LumenSceneLighting.h"
#include "Materials/Material.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "VolumeLighting.h"
#include "DistanceFieldLightingShared.h"
#include "VolumetricCloudRendering.h"
#include "LumenTracingUtils.h"

using namespace LightFunctionAtlas;

static TAutoConsoleVariable<int32> CVarLumenLumenSceneDirectLighting(
	TEXT("r.LumenScene.DirectLighting"),
	1,
	TEXT("Whether to compute direct ligshting for surface cache."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

int32 GLumenDirectLightingOffscreenShadowingTraceMeshSDFs = 1;
FAutoConsoleVariableRef CVarLumenDirectLightingOffscreenShadowingTraceMeshSDFs(
	TEXT("r.LumenScene.DirectLighting.OffscreenShadowing.TraceMeshSDFs"),
	GLumenDirectLightingOffscreenShadowingTraceMeshSDFs,
	TEXT("Whether to trace against Mesh Signed Distance Fields for offscreen shadowing, or to trace against the lower resolution Global SDF."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenDirectLightingMaxLightsPerTile(
	TEXT("r.LumenScene.DirectLighting.MaxLightsPerTile"),
	8,
	TEXT("Max number of lights to pick per tile based on their intenstiy and attenuation. Valid values are 4/8/16/32. Increasing this value will cause more memory usage and will slow down Lumen surface cache direct lighting pass."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GOffscreenShadowingTraceStepFactor = 5;
FAutoConsoleVariableRef CVarOffscreenShadowingTraceStepFactor(
	TEXT("r.LumenScene.DirectLighting.OffscreenShadowingTraceStepFactor"),
	GOffscreenShadowingTraceStepFactor,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenDirectLightingCloudTransmittance = 1;
FAutoConsoleVariableRef CVarLumenDirectLightingCloudTransmittance(
	TEXT("r.LumenScene.DirectLighting.CloudTransmittance"),
	GLumenDirectLightingCloudTransmittance,
	TEXT("Whether to sample cloud shadows when avaible."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenDirectLightingMeshSDFShadowRayBias(
	TEXT("r.LumenScene.DirectLighting.MeshSDF.ShadowRayBias"),
	2.0f,
	TEXT("Bias for tracing mesh SDF shadow rays."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenDirectLightingHeightfieldShadowRayBias(
	TEXT("r.LumenScene.DirectLighting.Heightfield.ShadowRayBias"),
	2.0f,
	TEXT("Bias for tracing heightfield shadow rays."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenDirectLightingGlobalSDFShadowRayBias(
	TEXT("r.LumenScene.DirectLighting.GlobalSDF.ShadowRayBias"),
	1.0f,
	TEXT("Bias for tracing global SDF shadow rays."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenDirectLightingHardwareRayTracingShadowRayBias(
	TEXT("r.LumenScene.DirectLighting.HardwareRayTracing.ShadowRayBias"),
	1.0f,
	TEXT("Bias for hardware ray tracing shadow rays."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenDirectLightingBatchShadows(
	TEXT("r.LumenScene.DirectLighting.BatchShadows"),
	1,
	TEXT("Whether to enable batching lumen light shadow passes. This cvar mainly exists for debugging."),
	ECVF_RenderThreadSafe);

float LumenSceneDirectLighting::GetMeshSDFShadowRayBias()
{
	return FMath::Max(CVarLumenDirectLightingMeshSDFShadowRayBias.GetValueOnRenderThread(), 0.0f);
}

float LumenSceneDirectLighting::GetHeightfieldShadowRayBias()
{
	return FMath::Max(CVarLumenDirectLightingHeightfieldShadowRayBias.GetValueOnRenderThread(), 0.0f);
}

float LumenSceneDirectLighting::GetGlobalSDFShadowRayBias()
{
	return FMath::Max(CVarLumenDirectLightingGlobalSDFShadowRayBias.GetValueOnRenderThread(), 0.0f);
}

float LumenSceneDirectLighting::GetHardwareRayTracingShadowRayBias()
{
	return FMath::Max(CVarLumenDirectLightingHardwareRayTracingShadowRayBias.GetValueOnRenderThread(), 0.0f);
}

class FLumenGatheredLight
{
public:
	FLumenGatheredLight(const FScene* Scene, TConstArrayView<FViewInfo> Views, const FLightSceneInfo* InLightSceneInfo, uint32 InLightIndex, bool bUseHardwareRayTracing)
	{
		LightIndex = InLightIndex;
		LightSceneInfo = InLightSceneInfo;
		bHasShadows = InLightSceneInfo->Proxy->CastsDynamicShadow();

		const FViewInfo& View = Views[0];
		const FLightSceneProxy* Proxy = LightSceneInfo->Proxy;

		Type = ELumenLightType::MAX;
		const ELightComponentType LightType = (ELightComponentType)Proxy->GetLightType();
		switch (LightType)
		{
			case LightType_Directional: Type = ELumenLightType::Directional;	break;
			case LightType_Point:		Type = ELumenLightType::Point;			break;
			case LightType_Spot:		Type = ELumenLightType::Spot;			break;
			case LightType_Rect:		Type = ELumenLightType::Rect;			break;
		}

		if (Type == ELumenLightType::Directional)
		{
			bMayCastCloudTransmittance = LightMayCastCloudShadow(Scene, View, LightSceneInfo);
		}

		LightFunctionMaterialProxy = Proxy->GetLightFunctionMaterial();
		if (LightFunctionMaterialProxy && (!View.Family->EngineShowFlags.LightFunctions || !LightFunctionMaterialProxy->GetIncompleteMaterialWithFallback(Scene->GetFeatureLevel()).IsLightFunction()))
		{
			LightFunctionMaterialProxy = nullptr;
		}
		const bool bBatchableLightFunction = LightFunctionMaterialProxy == nullptr || (LightFunctionAtlas::IsEnabled(View, ELightFunctionAtlasSystem::Lumen) && LightSceneInfo->Proxy->HasValidLightFunctionAtlasSlot());
		
		FSceneRenderer::GetLightNameForDrawEvent(Proxy, Name);

		bNeedsShadowMask = bHasShadows || bMayCastCloudTransmittance || LightFunctionMaterialProxy;

		// If evaluates to false, the light may still be eligible for batching during a raytraced shadow pass.
		// The assumption is that such lights are not common so we are not optimizing for them.
		bBatchedShadowsEligible = !bMayCastCloudTransmittance && bBatchableLightFunction && Type != ELumenLightType::Directional;

		// Non-raytraced and distance field shadows require the light uniform buffer struct for each view.
		if ((!bUseHardwareRayTracing && bHasShadows) || NeedsShadowMask())
		{
			DeferredLightUniformBuffers.SetNum(Views.Num());

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
			{
				FDeferredLightUniformStruct DeferredLightUniforms = GetDeferredLightParameters(Views[ViewIndex], *LightSceneInfo);
				if (LightSceneInfo->Proxy->IsInverseSquared())
				{
					DeferredLightUniforms.LightParameters.FalloffExponent = 0;
				}
				DeferredLightUniforms.LightParameters.Color *= LightSceneInfo->Proxy->GetIndirectLightingScale();
				DeferredLightUniformBuffers[ViewIndex] = CreateUniformBufferImmediate(DeferredLightUniforms, UniformBuffer_SingleFrame);
			}
		}
	}

	bool NeedsShadowMask() const
	{
		return bNeedsShadowMask;
	}

	bool CanUseBatchedShadows() const
	{
		return bBatchedShadowsEligible;
	}

	const FLightSceneInfo* LightSceneInfo = nullptr;
	const FMaterialRenderProxy* LightFunctionMaterialProxy = nullptr;
	uint32 LightIndex = 0;
	ELumenLightType Type = ELumenLightType::MAX;
	bool bHasShadows = false;
	bool bMayCastCloudTransmittance = false;
	bool bNeedsShadowMask = false;
	bool bBatchedShadowsEligible = false;
	FString Name;
	TArray<TUniformBufferRef<FDeferredLightUniformStruct>, TInlineAllocator<4>> DeferredLightUniformBuffers;
};

BEGIN_SHADER_PARAMETER_STRUCT(FLumenLightTileScatterParameters, )
	RDG_BUFFER_ACCESS(DrawIndirectArgs, ERHIAccess::IndirectArgs)
	RDG_BUFFER_ACCESS(DispatchIndirectArgs, ERHIAccess::IndirectArgs)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, LightTileAllocator)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint2>, LightTiles)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, LightTileOffsetsPerLight)
END_SHADER_PARAMETER_STRUCT()

class FRasterizeToLightTilesVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRasterizeToLightTilesVS);
	SHADER_USE_PARAMETER_STRUCT(FRasterizeToLightTilesVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenCardScene, LumenCardScene)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenLightTileScatterParameters, LightTileScatterParameters)
		SHADER_PARAMETER(uint32, LightIndex)
		SHADER_PARAMETER(uint32, ViewIndex)
		SHADER_PARAMETER(uint32, NumViews)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}
};

IMPLEMENT_GLOBAL_SHADER(FRasterizeToLightTilesVS,"/Engine/Private/Lumen/LumenSceneDirectLightingCulling.usf","RasterizeToLightTilesVS",SF_Vertex);

class FSpliceCardPagesIntoTilesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSpliceCardPagesIntoTilesCS);
	SHADER_USE_PARAMETER_STRUCT(FSpliceCardPagesIntoTilesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectArgBuffer, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenCardScene, LumenCardScene)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FLumenPackedLight>, LumenPackedLights)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWCardTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWCardTiles)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWLightTileAllocatorPerLight)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CardPageIndexAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CardPageIndexData)
		SHADER_PARAMETER(uint32, MaxLightsPerTile)
		SHADER_PARAMETER(uint32, NumLights)
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
		return 8;
	}
};

IMPLEMENT_GLOBAL_SHADER(FSpliceCardPagesIntoTilesCS, "/Engine/Private/Lumen/LumenSceneDirectLightingCulling.usf", "SpliceCardPagesIntoTilesCS", SF_Compute);

class FInitializeCardTileIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInitializeCardTileIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FInitializeCardTileIndirectArgsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWDispatchCardTilesIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CardTileAllocator)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static uint32 GetGroupSize()
	{
		return 64;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FInitializeCardTileIndirectArgsCS, "/Engine/Private/Lumen/LumenSceneDirectLightingCulling.usf", "InitializeCardTileIndirectArgsCS", SF_Compute)

void Lumen::SpliceCardPagesIntoTiles(
	FRDGBuilder& GraphBuilder,
	const FGlobalShaderMap* GlobalShaderMap,
	const FLumenCardUpdateContext& CardUpdateContext,
	const TRDGUniformBufferRef<FLumenCardScene>& LumenCardSceneUniformBuffer,
	FLumenCardTileUpdateContext& OutCardTileUpdateContext,
	ERDGPassFlags ComputePassFlags)
{
	const uint32 MaxLightTilesTilesX = FMath::DivideAndRoundUp<uint32>(CardUpdateContext.UpdateAtlasSize.X, Lumen::CardTileSize);
	const uint32 MaxLightTilesTilesY = FMath::DivideAndRoundUp<uint32>(CardUpdateContext.UpdateAtlasSize.Y, Lumen::CardTileSize);
	const uint32 MaxLightTiles = MaxLightTilesTilesX * MaxLightTilesTilesY;

	FRDGBufferRef CardTileAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("Lumen.CardTileAllocator"));
	FRDGBufferRef CardTiles = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), MaxLightTiles), TEXT("Lumen.CardTiles"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(CardTileAllocator), 0, ComputePassFlags);

	// Splice card pages into card tiles
	{
		FSpliceCardPagesIntoTilesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSpliceCardPagesIntoTilesCS::FParameters>();
		PassParameters->IndirectArgBuffer = CardUpdateContext.DispatchCardPageIndicesIndirectArgs;
		PassParameters->LumenCardScene = LumenCardSceneUniformBuffer;
		PassParameters->RWCardTileAllocator = GraphBuilder.CreateUAV(CardTileAllocator);
		PassParameters->RWCardTiles = GraphBuilder.CreateUAV(CardTiles);
		PassParameters->CardPageIndexAllocator = GraphBuilder.CreateSRV(CardUpdateContext.CardPageIndexAllocator);
		PassParameters->CardPageIndexData = GraphBuilder.CreateSRV(CardUpdateContext.CardPageIndexData);
		auto ComputeShader = GlobalShaderMap->GetShader<FSpliceCardPagesIntoTilesCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SpliceCardPagesIntoTiles"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			CardUpdateContext.DispatchCardPageIndicesIndirectArgs,
			FLumenCardUpdateContext::EIndirectArgOffset::ThreadPerTile);
	}

	// Setup indirect args for card tile processing
	FRDGBufferRef DispatchCardTilesIndirectArgs = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>((uint32)ELumenDispatchCardTilesIndirectArgsOffset::Num),
		TEXT("Lumen.DispatchCardTilesIndirectArgs"));
	{
		FInitializeCardTileIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitializeCardTileIndirectArgsCS::FParameters>();
		PassParameters->RWDispatchCardTilesIndirectArgs = GraphBuilder.CreateUAV(DispatchCardTilesIndirectArgs);
		PassParameters->CardTileAllocator = GraphBuilder.CreateSRV(CardTileAllocator);

		auto ComputeShader = GlobalShaderMap->GetShader<FInitializeCardTileIndirectArgsCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("InitializeCardTileIndirectArgs"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	OutCardTileUpdateContext.CardTileAllocator = CardTileAllocator;
	OutCardTileUpdateContext.CardTiles = CardTiles;
	OutCardTileUpdateContext.DispatchCardTilesIndirectArgs = DispatchCardTilesIndirectArgs;
}

class FBuildLightTilesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FBuildLightTilesCS);
	SHADER_USE_PARAMETER_STRUCT(FBuildLightTilesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectArgBuffer, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenCardScene, LumenCardScene)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FLumenPackedLight>, LumenPackedLights)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWLightTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWLightTileAllocatorForPerCardTileDispatch)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint2>, RWLightTiles)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWLightTileAllocatorPerLight)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWLightTileOffsetNumPerCardTile)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CardTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CardTiles)
		SHADER_PARAMETER(uint32, MaxLightsPerTile)
		SHADER_PARAMETER(uint32, NumLights)
		SHADER_PARAMETER(uint32, NumViews)
		SHADER_PARAMETER_ARRAY(FMatrix44f, WorldToClip, [LUMEN_MAX_VIEWS])
		SHADER_PARAMETER_ARRAY(FVector4f, PreViewTranslation, [LUMEN_MAX_VIEWS])
		SHADER_PARAMETER(FVector2f, ViewExposure)
	END_SHADER_PARAMETER_STRUCT()

	class FMaxLightSamples : SHADER_PERMUTATION_SPARSE_INT("MAX_LIGHT_SAMPLES", 1, 2, 4, 8, 16, 32);
	using FPermutationDomain = TShaderPermutationDomain<FMaxLightSamples>;

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

IMPLEMENT_GLOBAL_SHADER(FBuildLightTilesCS, "/Engine/Private/Lumen/LumenSceneDirectLightingCulling.usf", "BuildLightTilesCS", SF_Compute);

class FComputeLightTileOffsetsPerLightCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FComputeLightTileOffsetsPerLightCS)
	SHADER_USE_PARAMETER_STRUCT(FComputeLightTileOffsetsPerLightCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWLightTileOffsetsPerLight)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, LightTileAllocatorPerLight)
		SHADER_PARAMETER(uint32, NumLights)
		SHADER_PARAMETER(uint32, NumViews)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static uint32 GetGroupSize()
	{
		return 64;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FComputeLightTileOffsetsPerLightCS, "/Engine/Private/Lumen/LumenSceneDirectLightingCulling.usf", "ComputeLightTileOffsetsPerLightCS", SF_Compute);

class FCompactLightTilesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCompactLightTilesCS);
	SHADER_USE_PARAMETER_STRUCT(FCompactLightTilesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectArgBuffer, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint2>, RWCompactedLightTiles)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint2>, RWLightTilesPerCardTile)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWCompactedLightTileAllocatorPerLight)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, LightTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint2>, LightTiles)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, LightTileOffsetsPerLight)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CardTiles)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, LightTileOffsetNumPerCardTile)
		SHADER_PARAMETER(uint32, NumLights)
		SHADER_PARAMETER(uint32, NumViews)
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

IMPLEMENT_GLOBAL_SHADER(FCompactLightTilesCS, "/Engine/Private/Lumen/LumenSceneDirectLightingCulling.usf", "CompactLightTilesCS", SF_Compute);

class FInitializeLightTileIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInitializeLightTileIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FInitializeLightTileIndirectArgsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWDispatchLightTilesIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWDrawTilesPerLightIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWDispatchTilesPerLightIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, LightTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, LightTileAllocatorPerLight)
		SHADER_PARAMETER(uint32, VertexCountPerInstanceIndirect)
		SHADER_PARAMETER(uint32, PerLightDispatchFactor)
		SHADER_PARAMETER(uint32, NumLights)
		SHADER_PARAMETER(uint32, NumViews)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static uint32 GetGroupSize()
	{
		return 64;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FInitializeLightTileIndirectArgsCS, "/Engine/Private/Lumen/LumenSceneDirectLightingCulling.usf", "InitializeLightTileIndirectArgsCS", SF_Compute)

BEGIN_SHADER_PARAMETER_STRUCT(FClearLumenCardsParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FRasterizeToCardsVS::FParameters, VS)
	SHADER_PARAMETER_STRUCT_INCLUDE(FClearLumenCardsPS::FParameters, PS)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

void ClearLumenSceneDirectLighting(
	const FViewInfo& View,
	FRDGBuilder& GraphBuilder,
	const FLumenSceneData& LumenSceneData,
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	FLumenCardUpdateContext CardUpdateContext)
{
	FClearLumenCardsParameters* PassParameters = GraphBuilder.AllocParameters<FClearLumenCardsParameters>();

	PassParameters->RenderTargets[0] = FRenderTargetBinding(FrameTemporaries.DirectLightingAtlas, ERenderTargetLoadAction::ELoad);
	PassParameters->VS.LumenCardScene = FrameTemporaries.LumenCardSceneUniformBuffer;
	PassParameters->VS.DrawIndirectArgs = CardUpdateContext.DrawCardPageIndicesIndirectArgs;
	PassParameters->VS.CardPageIndexAllocator = GraphBuilder.CreateSRV(CardUpdateContext.CardPageIndexAllocator);
	PassParameters->VS.CardPageIndexData = GraphBuilder.CreateSRV(CardUpdateContext.CardPageIndexData);
	PassParameters->VS.IndirectLightingAtlasSize = LumenSceneData.GetRadiosityAtlasSize();
	PassParameters->PS.View = View.ViewUniformBuffer;
	PassParameters->PS.LumenCardScene = FrameTemporaries.LumenCardSceneUniformBuffer;

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("ClearDirectLighting"),
		PassParameters,
		ERDGPassFlags::Raster,
		[ViewportSize = LumenSceneData.GetPhysicalAtlasSize(), PassParameters, GlobalShaderMap = View.ShaderMap](FRHICommandList& RHICmdList)
	{
		FClearLumenCardsPS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FClearLumenCardsPS::FNumTargets>(1);
		auto PixelShader = GlobalShaderMap->GetShader<FClearLumenCardsPS>(PermutationVector);

		auto VertexShader = GlobalShaderMap->GetShader<FRasterizeToCardsVS>();

		DrawQuadsToAtlas(
			ViewportSize,
			VertexShader,
			PixelShader,
			PassParameters,
			GlobalShaderMap,
			TStaticBlendState<>::GetRHI(),
			RHICmdList,
			[](FRHICommandList& RHICmdList, TShaderRefBase<FClearLumenCardsPS, FShaderMapPointerTable> Shader, FRHIPixelShader* ShaderRHI, const typename FClearLumenCardsPS::FParameters& Parameters) {},
			PassParameters->VS.DrawIndirectArgs,
			0);
	});
}

class FLumenCardBatchDirectLightingCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenCardBatchDirectLightingCS)
	SHADER_USE_PARAMETER_STRUCT(FLumenCardBatchDirectLightingCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectArgBuffer, ERHIAccess::IndirectArgs)
		// This shader isn't view specific but the RectLightAtlasTexture, though doesn't vary per view, is accessed through the view uniform buffer
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenCardScene, LumenCardScene)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FLumenPackedLight>, LumenPackedLights)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ShadowMaskTiles)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CardTiles)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, LightTileOffsetNumPerCardTile)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint2>, LightTilesPerCardTile)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWDirectLightingAtlas)
		SHADER_PARAMETER_ARRAY(FVector4f, PreViewTranslation, [LUMEN_MAX_VIEWS])
		SHADER_PARAMETER(FVector2f, ViewExposure)
	END_SHADER_PARAMETER_STRUCT()

	class FMultiView : SHADER_PERMUTATION_BOOL("HAS_MULTIPLE_VIEWS");
	class FHasRectLights : SHADER_PERMUTATION_BOOL("HAS_RECT_LIGHTS");
	using FPermutationDomain = TShaderPermutationDomain<FMultiView, FHasRectLights>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("USE_LIGHT_UNIFORM_BUFFER"), 0);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLumenCardBatchDirectLightingCS, "/Engine/Private/Lumen/LumenSceneDirectLighting.usf", "LumenCardBatchDirectLightingCS", SF_Compute);

BEGIN_SHADER_PARAMETER_STRUCT(FPerLightParameters, )
	SHADER_PARAMETER(uint32, LightIndex)
	SHADER_PARAMETER(float, TanLightSourceAngle)
	SHADER_PARAMETER_STRUCT_REF(FDeferredLightUniformStruct, DeferredLightUniforms)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FLumenDirectLightingNonRayTracedShadowsParameters, )
	RDG_BUFFER_ACCESS(IndirectArgBuffer, ERHIAccess::IndirectArgs)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWShadowMaskTiles)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWShadowTraceAllocator)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWShadowTraces)
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenCardScene, LumenCardScene)
	SHADER_PARAMETER_STRUCT_INCLUDE(FPerLightParameters, LightParameters)
	SHADER_PARAMETER_STRUCT_INCLUDE(FLumenLightTileScatterParameters, LightTileScatterParameters)
	SHADER_PARAMETER(uint32, CardScatterInstanceIndex)
	SHADER_PARAMETER(uint32, ViewIndex)
	SHADER_PARAMETER(uint32, NumViews)
	SHADER_PARAMETER(uint32, DummyZeroForFixingShaderCompilerBug)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightData, ForwardLightData)
	SHADER_PARAMETER_STRUCT_INCLUDE(FLightCloudTransmittanceParameters, LightCloudTransmittanceParameters)
	SHADER_PARAMETER(float, HeightfieldShadowReceiverBias)
	SHADER_PARAMETER(float, StepFactor)
	SHADER_PARAMETER(float, MaxTraceDistance)
END_SHADER_PARAMETER_STRUCT()

class FLumenDirectLightingShadowMaskFromLightAttenuationCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenDirectLightingShadowMaskFromLightAttenuationCS)
	SHADER_USE_PARAMETER_STRUCT(FLumenDirectLightingShadowMaskFromLightAttenuationCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenDirectLightingNonRayTracedShadowsParameters, Common)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLightFunctionAtlasGlobalParameters, LightFunctionAtlas)
	END_SHADER_PARAMETER_STRUCT()

	class FThreadGroupSize32 : SHADER_PERMUTATION_BOOL("THREADGROUP_SIZE_32");
	class FCompactShadowTraces : SHADER_PERMUTATION_BOOL("COMPACT_SHADOW_TRACES");
	class FLightType : SHADER_PERMUTATION_ENUM_CLASS("LIGHT_TYPE", ELumenLightType);
	class FCloudTransmittance : SHADER_PERMUTATION_BOOL("USE_CLOUD_TRANSMITTANCE");
	class FLightFunctionAtlas : SHADER_PERMUTATION_BOOL("USE_LIGHT_FUNCTION_ATLAS");
	using FPermutationDomain = TShaderPermutationDomain<FThreadGroupSize32, FCompactShadowTraces, FLightType, FCloudTransmittance, FLightFunctionAtlas>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (PermutationVector.Get<FCloudTransmittance>() && PermutationVector.Get<FLightType>() != ELumenLightType::Directional)
		{
			return false;
		}

		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
		OutEnvironment.SetDefine(TEXT("LIGHT_FUNCTION"), 0);
		OutEnvironment.SetDefine(TEXT("USE_IES_PROFILE"), 1);
		OutEnvironment.SetDefine(TEXT("SUBSTRATE_INLINE_SHADING"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLumenDirectLightingShadowMaskFromLightAttenuationCS, "/Engine/Private/Lumen/LumenSceneDirectLightingShadowMask.usf", "LumenSceneDirectLightingShadowMaskFromLightAttenuationCS", SF_Compute);

BEGIN_SHADER_PARAMETER_STRUCT(FLightFunctionParameters, )
	SHADER_PARAMETER_STRUCT_REF(FPrimitiveUniformShaderParameters, PrimitiveUniformBuffer)
	SHADER_PARAMETER(FVector4f, LightFunctionParameters)
	SHADER_PARAMETER(FMatrix44f, LightFunctionTranslatedWorldToLight)
	SHADER_PARAMETER(FVector3f, LightFunctionParameters2)
END_SHADER_PARAMETER_STRUCT()

class FLumenDirectLightingShadowMaskFromLightAttenuationWithLightFunctionCS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FLumenDirectLightingShadowMaskFromLightAttenuationWithLightFunctionCS, Material);

	FLumenDirectLightingShadowMaskFromLightAttenuationWithLightFunctionCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMaterialShader(Initializer)
	{
		Bindings.BindForLegacyShaderParameters(
			this,
			Initializer.PermutationId,
			Initializer.ParameterMap,
			*FParameters::FTypeInfo::GetStructMetadata(),
			// Don't require full bindings, we use FMaterialShader::SetParameters
			false);
	}

	FLumenDirectLightingShadowMaskFromLightAttenuationWithLightFunctionCS() {}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenDirectLightingNonRayTracedShadowsParameters, Common)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLightFunctionParameters, LightFunctionParameters)
	END_SHADER_PARAMETER_STRUCT()

	class FThreadGroupSize32 : SHADER_PERMUTATION_BOOL("THREADGROUP_SIZE_32");
	class FCompactShadowTraces : SHADER_PERMUTATION_BOOL("COMPACT_SHADOW_TRACES");
	class FLightType : SHADER_PERMUTATION_ENUM_CLASS("LIGHT_TYPE", ELumenLightType);
	class FCloudTransmittance : SHADER_PERMUTATION_BOOL("USE_CLOUD_TRANSMITTANCE");
	using FPermutationDomain = TShaderPermutationDomain<FThreadGroupSize32, FCompactShadowTraces, FLightType, FCloudTransmittance>;

	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (PermutationVector.Get<FCloudTransmittance>() && PermutationVector.Get<FLightType>() != ELumenLightType::Directional)
		{
			return false;
		}

		return Parameters.MaterialParameters.MaterialDomain == EMaterialDomain::MD_LightFunction && DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
		OutEnvironment.SetDefine(TEXT("LIGHT_FUNCTION"), 1);
		OutEnvironment.SetDefine(TEXT("USE_IES_PROFILE"), 1);
		OutEnvironment.SetDefine(TEXT("SUBSTRATE_INLINE_SHADING"), 1);
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FLumenDirectLightingShadowMaskFromLightAttenuationWithLightFunctionCS, TEXT("/Engine/Private/Lumen/LumenSceneDirectLightingShadowMask.usf"), TEXT("LumenSceneDirectLightingShadowMaskFromLightAttenuationCS"), SF_Compute);

class FInitShadowTraceIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInitShadowTraceIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FInitShadowTraceIndirectArgsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWShadowTraceIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ShadowTraceAllocator)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static uint32 GetGroupSize()
	{
		return 64;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FInitShadowTraceIndirectArgsCS, "/Engine/Private/Lumen/LumenSceneDirectLightingSoftwareRayTracing.usf", "InitShadowTraceIndirectArgsCS", SF_Compute)

class FLumenSceneDirectLightingTraceDistanceFieldShadowsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenSceneDirectLightingTraceDistanceFieldShadowsCS)
	SHADER_USE_PARAMETER_STRUCT(FLumenSceneDirectLightingTraceDistanceFieldShadowsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectArgBuffer, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWShadowMaskTiles)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenCardScene, LumenCardScene)
		SHADER_PARAMETER_STRUCT_INCLUDE(FPerLightParameters, LightParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenLightTileScatterParameters, LightTileScatterParameters)
		SHADER_PARAMETER(uint32, ViewIndex)
		SHADER_PARAMETER(uint32, NumViews)
		SHADER_PARAMETER(uint32, DummyZeroForFixingShaderCompilerBug)
		SHADER_PARAMETER_STRUCT_INCLUDE(FDistanceFieldObjectBufferParameters, ObjectBufferParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLightTileIntersectionParameters, LightTileIntersectionParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FDistanceFieldAtlasParameters, DistanceFieldAtlasParameters)
		SHADER_PARAMETER(FMatrix44f, TranslatedWorldToShadow)
		SHADER_PARAMETER(float, TwoSidedMeshDistanceBiasScale)
		SHADER_PARAMETER(float, StepFactor)
		SHADER_PARAMETER(float, MaxTraceDistance)
		SHADER_PARAMETER(float, MeshSDFShadowRayBias)
		SHADER_PARAMETER(float, HeightfieldShadowRayBias)
		SHADER_PARAMETER(float, GlobalSDFShadowRayBias)
		SHADER_PARAMETER(int32, HeightfieldMaxTracingSteps)
	END_SHADER_PARAMETER_STRUCT()

	class FThreadGroupSize32 : SHADER_PERMUTATION_BOOL("THREADGROUP_SIZE_32");
	class FLightType : SHADER_PERMUTATION_ENUM_CLASS("LIGHT_TYPE", ELumenLightType);
	class FTraceGlobalSDF : SHADER_PERMUTATION_BOOL("OFFSCREEN_SHADOWING_TRACE_GLOBAL_SDF");
	class FSimpleCoverageBasedExpand : SHADER_PERMUTATION_BOOL("GLOBALSDF_SIMPLE_COVERAGE_BASED_EXPAND");
	class FTraceMeshSDFs : SHADER_PERMUTATION_BOOL("OFFSCREEN_SHADOWING_TRACE_MESH_SDF");
	class FTraceHeightfields : SHADER_PERMUTATION_BOOL("OFFSCREEN_SHADOWING_TRACE_HEIGHTFIELDS");
	class FOffsetDataStructure : SHADER_PERMUTATION_INT("OFFSET_DATA_STRUCT", 3);
	using FPermutationDomain = TShaderPermutationDomain<FThreadGroupSize32, FLightType, FTraceGlobalSDF, FSimpleCoverageBasedExpand, FTraceMeshSDFs, FTraceHeightfields, FOffsetDataStructure>;

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		// Only directional lights support mesh SDF offscreen shadowing
		if (PermutationVector.Get<FLightType>() != ELumenLightType::Directional)
		{
			PermutationVector.Set<FTraceMeshSDFs>(false);
			PermutationVector.Set<FTraceHeightfields>(false);
		}

		// Don't trace global SDF if per mesh object traces are enabled
		if (PermutationVector.Get<FTraceMeshSDFs>() || PermutationVector.Get<FTraceHeightfields>())
		{
			PermutationVector.Set<FTraceGlobalSDF>(false);
		}

		// FOffsetDataStructure is only used for mesh SDFs
		if (!PermutationVector.Get<FTraceMeshSDFs>())
		{
			PermutationVector.Set<FOffsetDataStructure>(0);
		}

		if (!PermutationVector.Get<FTraceGlobalSDF>())
		{
			PermutationVector.Set<FSimpleCoverageBasedExpand>(false);
		}

		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (RemapPermutation(PermutationVector) != PermutationVector)
		{
			return false;
		}

		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	FORCENOINLINE static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment) 
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLumenSceneDirectLightingTraceDistanceFieldShadowsCS, "/Engine/Private/Lumen/LumenSceneDirectLightingSoftwareRayTracing.usf", "LumenSceneDirectLightingTraceDistanceFieldShadowsCS", SF_Compute);

void SetupLightFunctionParameters(const FViewInfo& View, const FLightSceneInfo* LightSceneInfo, float ShadowFadeFraction, FLightFunctionParameters& OutParameters)
{
	const bool bIsSpotLight = LightSceneInfo->Proxy->GetLightType() == LightType_Spot;
	const bool bIsPointLight = LightSceneInfo->Proxy->GetLightType() == LightType_Point;
	const float TanOuterAngle = bIsSpotLight ? FMath::Tan(LightSceneInfo->Proxy->GetOuterConeAngle()) : 1.0f;

	OutParameters.LightFunctionParameters = FVector4f(TanOuterAngle, ShadowFadeFraction, bIsSpotLight ? 1.0f : 0.0f, bIsPointLight ? 1.0f : 0.0f);

	const FVector Scale = LightSceneInfo->Proxy->GetLightFunctionScale();
	// Switch x and z so that z of the user specified scale affects the distance along the light direction
	const FVector InverseScale = FVector( 1.f / Scale.Z, 1.f / Scale.Y, 1.f / Scale.X );
	const FMatrix WorldToLight = LightSceneInfo->Proxy->GetWorldToLight() * FScaleMatrix(FVector(InverseScale));	

	OutParameters.LightFunctionTranslatedWorldToLight = FMatrix44f(FTranslationMatrix(-View.ViewMatrices.GetPreViewTranslation()) * WorldToLight);

	const float PreviewShadowsMask = 0.0f;
	OutParameters.LightFunctionParameters2 = FVector3f(
		LightSceneInfo->Proxy->GetLightFunctionFadeDistance(),
		LightSceneInfo->Proxy->GetLightFunctionDisabledBrightness(),
		PreviewShadowsMask);

	OutParameters.PrimitiveUniformBuffer = GIdentityPrimitiveUniformBuffer.GetUniformBufferRef();
}

void SetupMeshSDFShadowInitializer(
	const FLightSceneInfo* LightSceneInfo,
	const FBox& LumenSceneBounds, 
	FSphere& OutShadowBounds,
	FWholeSceneProjectedShadowInitializer& OutInitializer)
{	
	FSphere Bounds;

	{
		// Get the 8 corners of the cascade's camera frustum, in world space
		FVector CascadeFrustumVerts[8];
		const FVector LumenSceneCenter = LumenSceneBounds.GetCenter();
		const FVector LumenSceneExtent = LumenSceneBounds.GetExtent();
		CascadeFrustumVerts[0] = LumenSceneCenter + FVector(LumenSceneExtent.X, LumenSceneExtent.Y, LumenSceneExtent.Z);  
		CascadeFrustumVerts[1] = LumenSceneCenter + FVector(LumenSceneExtent.X, LumenSceneExtent.Y, -LumenSceneExtent.Z); 
		CascadeFrustumVerts[2] = LumenSceneCenter + FVector(LumenSceneExtent.X, -LumenSceneExtent.Y, LumenSceneExtent.Z);    
		CascadeFrustumVerts[3] = LumenSceneCenter + FVector(LumenSceneExtent.X, -LumenSceneExtent.Y, -LumenSceneExtent.Z);  
		CascadeFrustumVerts[4] = LumenSceneCenter + FVector(-LumenSceneExtent.X, LumenSceneExtent.Y, LumenSceneExtent.Z);     
		CascadeFrustumVerts[5] = LumenSceneCenter + FVector(-LumenSceneExtent.X, LumenSceneExtent.Y, -LumenSceneExtent.Z);   
		CascadeFrustumVerts[6] = LumenSceneCenter + FVector(-LumenSceneExtent.X, -LumenSceneExtent.Y, LumenSceneExtent.Z);      
		CascadeFrustumVerts[7] = LumenSceneCenter + FVector(-LumenSceneExtent.X, -LumenSceneExtent.Y, -LumenSceneExtent.Z);   

		Bounds = FSphere(LumenSceneCenter, 0);
		for (int32 Index = 0; Index < 8; Index++)
		{
			Bounds.W = FMath::Max(Bounds.W, FVector::DistSquared(CascadeFrustumVerts[Index], Bounds.Center));
		}

		Bounds.W = FMath::Max(FMath::Sqrt(Bounds.W), 1.0f);

		ComputeShadowCullingVolume(true, CascadeFrustumVerts, -LightSceneInfo->Proxy->GetDirection(), OutInitializer.CascadeSettings.ShadowBoundsAccurate, OutInitializer.CascadeSettings.NearFrustumPlane, OutInitializer.CascadeSettings.FarFrustumPlane);
	}

	OutInitializer.CascadeSettings.ShadowSplitIndex = 0;

	const float ShadowExtent = Bounds.W / FMath::Sqrt(3.0f);
	const FBoxSphereBounds SubjectBounds(Bounds.Center, FVector(ShadowExtent, ShadowExtent, ShadowExtent), Bounds.W);
	OutInitializer.PreShadowTranslation = -Bounds.Center;
	OutInitializer.WorldToLight = FInverseRotationMatrix(LightSceneInfo->Proxy->GetDirection().GetSafeNormal().Rotation());
	OutInitializer.Scales = FVector2D(1.0f / Bounds.W, 1.0f / Bounds.W);
	OutInitializer.SubjectBounds = FBoxSphereBounds(FVector::ZeroVector, SubjectBounds.BoxExtent, SubjectBounds.SphereRadius);
	OutInitializer.WAxis = FVector4(0, 0, 0, 1);
	OutInitializer.MinLightW = FMath::Min<float>(-0.5f * UE_OLD_WORLD_MAX, -SubjectBounds.SphereRadius);
	const float MaxLightW = SubjectBounds.SphereRadius;
	OutInitializer.MaxDistanceToCastInLightW = MaxLightW - OutInitializer.MinLightW;
	OutInitializer.bRayTracedDistanceField = true;
	OutInitializer.CascadeSettings.bFarShadowCascade = false;

	const float SplitNear = -Bounds.W;
	const float SplitFar = Bounds.W;

	OutInitializer.CascadeSettings.SplitFarFadeRegion = 0.0f;
	OutInitializer.CascadeSettings.SplitNearFadeRegion = 0.0f;
	OutInitializer.CascadeSettings.SplitFar = SplitFar;
	OutInitializer.CascadeSettings.SplitNear = SplitNear;
	OutInitializer.CascadeSettings.FadePlaneOffset = SplitFar;
	OutInitializer.CascadeSettings.FadePlaneLength = 0;
	OutInitializer.CascadeSettings.CascadeBiasDistribution = 0;
	OutInitializer.CascadeSettings.ShadowSplitIndex = 0;

	OutShadowBounds = Bounds;
}

void CullMeshObjectsForLightCards(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View,
	const FLightSceneInfo* LightSceneInfo,
	EDistanceFieldPrimitiveType PrimitiveType,
	const FDistanceFieldObjectBufferParameters& ObjectBufferParameters,
	FMatrix& WorldToMeshSDFShadowValue,
	FLightTileIntersectionParameters& LightTileIntersectionParameters)
{
	const FVector LumenSceneViewOrigin = Lumen::GetLumenSceneViewOrigin(View, Lumen::GetNumGlobalDFClipmaps(View) - 1);
	const FVector LumenSceneExtent = FVector(LumenScene::GetCardMaxDistance(View));
	const FBox LumenSceneBounds(LumenSceneViewOrigin - LumenSceneExtent, LumenSceneViewOrigin + LumenSceneExtent);

	FSphere MeshSDFShadowBounds;
	FWholeSceneProjectedShadowInitializer MeshSDFShadowInitializer;
	SetupMeshSDFShadowInitializer(LightSceneInfo, LumenSceneBounds, MeshSDFShadowBounds, MeshSDFShadowInitializer);

	const FMatrix FaceMatrix(
		FPlane(0, 0, 1, 0),
		FPlane(0, 1, 0, 0),
		FPlane(-1, 0, 0, 0),
		FPlane(0, 0, 0, 1));

	const FMatrix TranslatedWorldToView = MeshSDFShadowInitializer.WorldToLight * FaceMatrix;

	double MaxSubjectZ = TranslatedWorldToView.TransformPosition(MeshSDFShadowInitializer.SubjectBounds.Origin).Z + MeshSDFShadowInitializer.SubjectBounds.SphereRadius;
	MaxSubjectZ = FMath::Min(MaxSubjectZ, MeshSDFShadowInitializer.MaxDistanceToCastInLightW);
	const double MinSubjectZ = FMath::Max(MaxSubjectZ - MeshSDFShadowInitializer.SubjectBounds.SphereRadius * 2, MeshSDFShadowInitializer.MinLightW);

	const FMatrix ScaleMatrix = FScaleMatrix( FVector( MeshSDFShadowInitializer.Scales.X, MeshSDFShadowInitializer.Scales.Y, 1.0f ) );
	const FMatrix ViewToClip = ScaleMatrix * FShadowProjectionMatrix(MinSubjectZ, MaxSubjectZ, MeshSDFShadowInitializer.WAxis);
	const FMatrix SubjectAndReceiverMatrix = TranslatedWorldToView * ViewToClip;

	int32 NumPlanes = MeshSDFShadowInitializer.CascadeSettings.ShadowBoundsAccurate.Planes.Num();
	const FPlane* PlaneData = MeshSDFShadowInitializer.CascadeSettings.ShadowBoundsAccurate.Planes.GetData();
	FVector PrePlaneTranslation = FVector::ZeroVector;
	FVector4f LocalLightShadowBoundingSphere = FVector4f::Zero();

	WorldToMeshSDFShadowValue = FTranslationMatrix(MeshSDFShadowInitializer.PreShadowTranslation) * SubjectAndReceiverMatrix;

	FDistanceFieldCulledObjectBufferParameters CulledObjectBufferParameters;

	const bool bCullingForDirectShadowing = false;
	const bool bCullHeighfieldsNotInAtlas = false;

	CullDistanceFieldObjectsForLight(
		GraphBuilder,
		View,
		LightSceneInfo->Proxy,
		PrimitiveType,
		WorldToMeshSDFShadowValue,
		NumPlanes,
		PlaneData,
		PrePlaneTranslation,
		LocalLightShadowBoundingSphere,
		MeshSDFShadowBounds.W,
		bCullingForDirectShadowing,
		bCullHeighfieldsNotInAtlas,
		ObjectBufferParameters,
		CulledObjectBufferParameters,
		LightTileIntersectionParameters);
}

static void RenderDirectLightIntoLumenCardsBatched(
	FRDGBuilder& GraphBuilder,
	const TArray<FViewInfo>& Views,
	TRDGUniformBufferRef<FLumenCardScene> LumenCardSceneUniformBuffer,
	FRDGBufferSRVRef LumenPackedLightsSRV,
	FRDGBufferSRVRef ShadowMaskTilesSRV,
	FRDGBufferSRVRef CardTilesSRV,
	FRDGBufferSRVRef LightTileOffsetNumPerCardTileSRV,
	FRDGBufferSRVRef LightTilesPerCardTileSRV,
	FRDGTextureUAVRef DirectLightingAtlasUAV,
	FRDGBufferRef IndirectArgBuffer,
	bool bHasRectLights,
	ERDGPassFlags ComputePassFlags)
{
	FLumenCardBatchDirectLightingCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenCardBatchDirectLightingCS::FParameters>();
	PassParameters->IndirectArgBuffer = IndirectArgBuffer;
	PassParameters->View = Views[0].ViewUniformBuffer;
	PassParameters->LumenCardScene = LumenCardSceneUniformBuffer;
	PassParameters->LumenPackedLights = LumenPackedLightsSRV;
	PassParameters->ShadowMaskTiles = ShadowMaskTilesSRV;
	PassParameters->CardTiles = CardTilesSRV;
	PassParameters->LightTileOffsetNumPerCardTile = LightTileOffsetNumPerCardTileSRV;
	PassParameters->LightTilesPerCardTile = LightTilesPerCardTileSRV;
	PassParameters->RWDirectLightingAtlas = DirectLightingAtlasUAV;
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		PassParameters->PreViewTranslation[ViewIndex] = FVector4f((FVector3f)Views[ViewIndex].ViewMatrices.GetPreViewTranslation(), 0.0f);
		PassParameters->ViewExposure[ViewIndex] = Views[ViewIndex].GetLastEyeAdaptationExposure();
	}

	FLumenCardBatchDirectLightingCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FLumenCardBatchDirectLightingCS::FMultiView>(Views.Num() > 1);
	PermutationVector.Set<FLumenCardBatchDirectLightingCS::FHasRectLights>(bHasRectLights);
	auto ComputeShader = Views[0].ShaderMap->GetShader<FLumenCardBatchDirectLightingCS>(PermutationVector);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("Batched lights"),
		ComputePassFlags,
		ComputeShader,
		PassParameters,
		IndirectArgBuffer,
		(uint32)ELumenDispatchCardTilesIndirectArgsOffset::OneGroupPerCardTile);
}

struct FViewBatchedLightParameters
{
	TArray<FPerLightParameters> PerLightTypeParameters[(int32)ELumenLightType::MAX];
};

static void SetPerLightParameters(FPerLightParameters& DstParameters, const FLumenGatheredLight& Light, int32 ViewIndex)
{
	DstParameters.LightIndex = Light.LightIndex;
	DstParameters.TanLightSourceAngle = FMath::Tan(Light.LightSceneInfo->Proxy->GetLightSourceAngle());
	DstParameters.DeferredLightUniforms = Light.DeferredLightUniformBuffers[ViewIndex];
}

// Compute for each light the shadow mask based on light attenuation properties (distance falloff, light functions, IES, volumetric cloud)
// This pass allows to pre-cull needs for tracing shadow rays
static int32 ComputeShadowMaskFromLightAttenuation(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View,
	TRDGUniformBufferRef<FLumenCardScene> LumenCardSceneUniformBuffer,
	TConstArrayView<FLumenGatheredLight> GatheredLights,
	TConstArrayView<int32> StandaloneLightIndices,
	const FViewBatchedLightParameters& ViewBatchedLightParameters,
	const FLumenLightTileScatterParameters& LightTileScatterParameters,
	int32 ViewIndex,
	int32 NumViews,
	const bool bHasLightFunctions, 
	FRDGBufferUAVRef ShadowMaskTilesUAV,
	FRDGBufferUAVRef ShadowTraceAllocatorUAV,
	FRDGBufferUAVRef ShadowTracesUAV,
	ERDGPassFlags ComputePassFlags)
{
	auto SetCommonParameters = [&](FLumenDirectLightingNonRayTracedShadowsParameters& CommonParameters)
	{
		CommonParameters.IndirectArgBuffer = LightTileScatterParameters.DispatchIndirectArgs;
		CommonParameters.RWShadowMaskTiles = ShadowMaskTilesUAV;
		CommonParameters.RWShadowTraceAllocator = ShadowTraceAllocatorUAV;
		CommonParameters.RWShadowTraces = ShadowTracesUAV;

		CommonParameters.View = View.ViewUniformBuffer;
		CommonParameters.LumenCardScene = LumenCardSceneUniformBuffer;
		CommonParameters.LightTileScatterParameters = LightTileScatterParameters;
		CommonParameters.CardScatterInstanceIndex = 0;
		CommonParameters.ViewIndex = ViewIndex;
		CommonParameters.NumViews = NumViews;
		CommonParameters.DummyZeroForFixingShaderCompilerBug = 0;
		CommonParameters.ForwardLightData = View.ForwardLightingResources.ForwardLightUniformBuffer;
		CommonParameters.MaxTraceDistance = Lumen::GetMaxTraceDistance(View);
		CommonParameters.StepFactor = FMath::Clamp(GOffscreenShadowingTraceStepFactor, .1f, 10.0f);
		CommonParameters.HeightfieldShadowReceiverBias = Lumen::GetHeightfieldReceiverBias();
	};

	int32 NumLightsNeedShadowMasks = StandaloneLightIndices.Num();

	for (const int32 StandaloneLightIndex : StandaloneLightIndices)
	{
		const FLumenGatheredLight& Light = GatheredLights[StandaloneLightIndex];
		check(Light.NeedsShadowMask());

		const FMaterialRenderProxy* LightFunctionMaterialProxy = Light.LightFunctionMaterialProxy;
		const bool bMayUseCloudTransmittance = GLumenDirectLightingCloudTransmittance != 0 && Light.bMayCastCloudTransmittance;
		const uint32 DispatchIndirectArgOffset = (Light.LightIndex * NumViews + ViewIndex) * sizeof(FRHIDispatchIndirectParameters);

		if (LightFunctionMaterialProxy)
		{
			FLumenDirectLightingShadowMaskFromLightAttenuationWithLightFunctionCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenDirectLightingShadowMaskFromLightAttenuationWithLightFunctionCS::FParameters>();
			SetCommonParameters(PassParameters->Common);
			SetPerLightParameters(PassParameters->Common.LightParameters, Light, ViewIndex);
			const bool bUseCloudTransmittance = SetupLightCloudTransmittanceParameters(
				GraphBuilder,
				Scene,
				View,
				bMayUseCloudTransmittance ? Light.LightSceneInfo : nullptr,
				PassParameters->Common.LightCloudTransmittanceParameters);
			SetupLightFunctionParameters(View, Light.LightSceneInfo, 1.0f, PassParameters->LightFunctionParameters);

			FLumenDirectLightingShadowMaskFromLightAttenuationWithLightFunctionCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FLumenDirectLightingShadowMaskFromLightAttenuationWithLightFunctionCS::FThreadGroupSize32>(Lumen::UseThreadGroupSize32());
			PermutationVector.Set<FLumenDirectLightingShadowMaskFromLightAttenuationWithLightFunctionCS::FCompactShadowTraces>(ShadowTraceAllocatorUAV != nullptr);
			PermutationVector.Set<FLumenDirectLightingShadowMaskFromLightAttenuationWithLightFunctionCS::FLightType>(Light.Type);
			PermutationVector.Set<FLumenDirectLightingShadowMaskFromLightAttenuationWithLightFunctionCS::FCloudTransmittance>(bUseCloudTransmittance);

			const FMaterial& Material = LightFunctionMaterialProxy->GetMaterialWithFallback(Scene->GetFeatureLevel(), LightFunctionMaterialProxy);
			const FMaterialShaderMap* MaterialShaderMap = Material.GetRenderingThreadShaderMap();
			TShaderRef<FLumenDirectLightingShadowMaskFromLightAttenuationWithLightFunctionCS> ComputeShader = MaterialShaderMap->GetShader<FLumenDirectLightingShadowMaskFromLightAttenuationWithLightFunctionCS>(PermutationVector);

			FRDGBufferRef IndirectArgsBuffer = LightTileScatterParameters.DispatchIndirectArgs;
			ClearUnusedGraphResources(ComputeShader, PassParameters, { IndirectArgsBuffer });

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ShadowMaskFromLightAttenuationPass(LF,%s)", *Light.Name),
				PassParameters,
				ComputePassFlags,
				[PassParameters, ComputeShader, IndirectArgsBuffer, DispatchIndirectArgOffset, LightFunctionMaterialProxy, &Material, &View](FRHIComputeCommandList& RHICmdList)
				{
					IndirectArgsBuffer->MarkResourceAsUsed();
					FComputeShaderUtils::ValidateIndirectArgsBuffer(IndirectArgsBuffer, DispatchIndirectArgOffset);
					FRHIComputeShader* ShaderRHI = ComputeShader.GetComputeShader();
					SetComputePipelineState(RHICmdList, ShaderRHI);
					SetShaderParameters(RHICmdList, ComputeShader, ShaderRHI, *PassParameters);
					ComputeShader->SetParameters(RHICmdList, ShaderRHI, LightFunctionMaterialProxy, Material, View);
					RHICmdList.DispatchIndirectComputeShader(IndirectArgsBuffer->GetIndirectRHICallBuffer(), DispatchIndirectArgOffset);
					UnsetShaderUAVs(RHICmdList, ComputeShader, ShaderRHI);
				});
		}
		else
		{
			FLumenDirectLightingShadowMaskFromLightAttenuationCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenDirectLightingShadowMaskFromLightAttenuationCS::FParameters>();
			SetCommonParameters(PassParameters->Common);
			SetPerLightParameters(PassParameters->Common.LightParameters, Light, ViewIndex);
			const bool bUseCloudTransmittance = SetupLightCloudTransmittanceParameters(
				GraphBuilder,
				Scene,
				View,
				bMayUseCloudTransmittance ? Light.LightSceneInfo : nullptr,
				PassParameters->Common.LightCloudTransmittanceParameters);

			FLumenDirectLightingShadowMaskFromLightAttenuationCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FLumenDirectLightingShadowMaskFromLightAttenuationCS::FThreadGroupSize32>(Lumen::UseThreadGroupSize32());
			PermutationVector.Set<FLumenDirectLightingShadowMaskFromLightAttenuationCS::FCompactShadowTraces>(ShadowTraceAllocatorUAV != nullptr);
			PermutationVector.Set<FLumenDirectLightingShadowMaskFromLightAttenuationCS::FLightType>(Light.Type);
			PermutationVector.Set<FLumenDirectLightingShadowMaskFromLightAttenuationCS::FCloudTransmittance>(bUseCloudTransmittance);
			PermutationVector.Set<FLumenDirectLightingShadowMaskFromLightAttenuationCS::FLightFunctionAtlas>(false);
			TShaderRef<FLumenDirectLightingShadowMaskFromLightAttenuationCS> ComputeShader = View.ShaderMap->GetShader<FLumenDirectLightingShadowMaskFromLightAttenuationCS>(PermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ShadowMaskFromLightAttenuationPass(%s)", *Light.Name),
				ComputePassFlags,
				ComputeShader,
				PassParameters,
				LightTileScatterParameters.DispatchIndirectArgs,
				DispatchIndirectArgOffset);
		}
	}

	const bool bUseLightFunctionAtlas = bHasLightFunctions && LightFunctionAtlas::IsEnabled(View, ELightFunctionAtlasSystem::Lumen);
	for (int32 LightTypeIndex = 0; LightTypeIndex < (int32)ELumenLightType::MAX; ++LightTypeIndex)
	{
		TConstArrayView<FPerLightParameters> BatchedLightParameters = ViewBatchedLightParameters.PerLightTypeParameters[LightTypeIndex];
		NumLightsNeedShadowMasks += BatchedLightParameters.Num();

		if (BatchedLightParameters.Num() > 0)
		{
			FLumenDirectLightingShadowMaskFromLightAttenuationCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenDirectLightingShadowMaskFromLightAttenuationCS::FParameters>();
			SetCommonParameters(PassParameters->Common);
			SetupLightCloudTransmittanceParameters(GraphBuilder, Scene, View, nullptr, PassParameters->Common.LightCloudTransmittanceParameters);
			if (bUseLightFunctionAtlas)
			{
				PassParameters->LightFunctionAtlas = LightFunctionAtlas::BindGlobalParameters(GraphBuilder, View);
			}

			FLumenDirectLightingShadowMaskFromLightAttenuationCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FLumenDirectLightingShadowMaskFromLightAttenuationCS::FThreadGroupSize32>(Lumen::UseThreadGroupSize32());
			PermutationVector.Set<FLumenDirectLightingShadowMaskFromLightAttenuationCS::FCompactShadowTraces>(ShadowTraceAllocatorUAV != nullptr);
			PermutationVector.Set<FLumenDirectLightingShadowMaskFromLightAttenuationCS::FLightType>((ELumenLightType)LightTypeIndex);
			PermutationVector.Set<FLumenDirectLightingShadowMaskFromLightAttenuationCS::FCloudTransmittance>(false);
			PermutationVector.Set<FLumenDirectLightingShadowMaskFromLightAttenuationCS::FLightFunctionAtlas>(bUseLightFunctionAtlas);
			TShaderRef<FLumenDirectLightingShadowMaskFromLightAttenuationCS> ComputeShader = View.ShaderMap->GetShader<FLumenDirectLightingShadowMaskFromLightAttenuationCS>(PermutationVector);

			const FShaderParametersMetadata* ParametersMetaData = FLumenDirectLightingShadowMaskFromLightAttenuationCS::FParameters::FTypeInfo::GetStructMetadata();
			FRDGBufferRef IndirectArgsBuffer = LightTileScatterParameters.DispatchIndirectArgs;
			ClearUnusedGraphResourcesImpl(ComputeShader->Bindings, ParametersMetaData, PassParameters, { IndirectArgsBuffer });

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ShadowMaskFromLightAttenuationPass(LightType=%d,BatchedNum=%d)", LightTypeIndex, BatchedLightParameters.Num()),
				PassParameters,
				ComputePassFlags,
				[PassParameters, ComputeShader, IndirectArgsBuffer, NumViews, ViewIndex, BatchedLightParameters](FRHIComputeCommandList& RHICmdList)
				{
					// Marks the indirect draw parameter as used by the pass manually, given it can't be bound directly by any of the shader,
					// meaning SetShaderParameters() won't be able to do it.
					IndirectArgsBuffer->MarkResourceAsUsed();

					for (const FPerLightParameters& LightParameterValues : BatchedLightParameters)
					{
						const uint32 IndirectArgsOffset = (LightParameterValues.LightIndex * NumViews + ViewIndex) * sizeof(FRHIDispatchIndirectParameters);

						// TODO: Only set changed paramters
						PassParameters->Common.LightParameters = LightParameterValues;
						FComputeShaderUtils::DispatchIndirect(RHICmdList, ComputeShader, *PassParameters, IndirectArgsBuffer->GetIndirectRHICallBuffer(), IndirectArgsOffset);
					}
				});
		}
	}

	return NumLightsNeedShadowMasks;
}

void TraceDistanceFieldShadows(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View,
	TRDGUniformBufferRef<FLumenCardScene> LumenCardSceneUniformBuffer,
	TConstArrayView<FLumenGatheredLight> GatheredLights,
	TConstArrayView<int32> StandaloneLightIndices,
	FViewBatchedLightParameters& ViewBatchedLightParameters,
	const FLumenLightTileScatterParameters& LightTileScatterParameters,
	const FDistanceFieldObjectBufferParameters& ObjectBufferParameters,
	int32 ViewIndex,
	int32 NumViews,
	FRDGBufferUAVRef ShadowMaskTilesUAV,
	ERDGPassFlags ComputePassFlags)
{
	extern int32 GDistanceFieldOffsetDataStructure;
	extern float GDFShadowTwoSidedMeshDistanceBiasScale;

	auto SetCommonParameters = [&](
		FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FParameters* PassParameters,
		const FLightTileIntersectionParameters& LightTileIntersectionParameters,
		const FMatrix& WorldToMeshSDFShadowValue)
	{
		PassParameters->IndirectArgBuffer = LightTileScatterParameters.DispatchIndirectArgs;
		PassParameters->RWShadowMaskTiles = ShadowMaskTilesUAV;

		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->LumenCardScene = LumenCardSceneUniformBuffer;
		PassParameters->LightTileScatterParameters = LightTileScatterParameters;
		PassParameters->ViewIndex = ViewIndex;
		PassParameters->NumViews = NumViews;
		PassParameters->DummyZeroForFixingShaderCompilerBug = 0;

		PassParameters->ObjectBufferParameters = ObjectBufferParameters;
		PassParameters->LightTileIntersectionParameters = LightTileIntersectionParameters;

		FDistanceFieldAtlasParameters DistanceFieldAtlasParameters = DistanceField::SetupAtlasParameters(GraphBuilder, Scene->DistanceFieldSceneData);

		PassParameters->DistanceFieldAtlasParameters = DistanceFieldAtlasParameters;
		PassParameters->TranslatedWorldToShadow = FMatrix44f(FTranslationMatrix(-View.ViewMatrices.GetPreViewTranslation()) * WorldToMeshSDFShadowValue);
		PassParameters->TwoSidedMeshDistanceBiasScale = GDFShadowTwoSidedMeshDistanceBiasScale;

		PassParameters->MaxTraceDistance = Lumen::GetMaxTraceDistance(View);
		PassParameters->StepFactor = FMath::Clamp(GOffscreenShadowingTraceStepFactor, .1f, 10.0f);
		PassParameters->MeshSDFShadowRayBias = LumenSceneDirectLighting::GetMeshSDFShadowRayBias();
		PassParameters->HeightfieldShadowRayBias = LumenSceneDirectLighting::GetHeightfieldShadowRayBias();
		PassParameters->GlobalSDFShadowRayBias = LumenSceneDirectLighting::GetGlobalSDFShadowRayBias();
		PassParameters->HeightfieldMaxTracingSteps = Lumen::GetHeightfieldMaxTracingSteps();
	};

	const bool bThreadGroupSize32 = Lumen::UseThreadGroupSize32();
	const bool bTraceGlobalSDF = Lumen::UseGlobalSDFTracing(*View.Family);
	const bool bSimpleCoverageBasedExpand = bTraceGlobalSDF && Lumen::UseGlobalSDFSimpleCoverageBasedExpand();

	for (const int32 StandaloneLightIndex : StandaloneLightIndices)
	{
		const FLumenGatheredLight& Light = GatheredLights[StandaloneLightIndex];

		if (!Light.bHasShadows)
		{
			continue;
		}

		const FLumenSceneData& LumenSceneData = *Scene->GetLumenSceneData(View);

		FLightTileIntersectionParameters LightTileIntersectionParameters;
		FMatrix WorldToMeshSDFShadowValue = FMatrix::Identity;

		// Whether to trace individual mesh SDFs or heightfield objects for higher quality offscreen shadowing
		const bool bTraceMeshObjects = Light.bHasShadows
			&& Light.Type == ELumenLightType::Directional
			&& DoesPlatformSupportDistanceFieldShadowing(View.GetShaderPlatform())
			&& GLumenDirectLightingOffscreenShadowingTraceMeshSDFs != 0;

		const bool bTraceMeshSDFs = bTraceMeshObjects
			&& Lumen::UseMeshSDFTracing(*View.Family)
			&& ObjectBufferParameters.NumSceneObjects > 0;

		const bool bTraceHeighfieldObjects = bTraceMeshObjects 
			&& Lumen::UseHeightfieldTracing(*View.Family, LumenSceneData);

		if (bTraceMeshSDFs)
		{
			CullMeshObjectsForLightCards(
				GraphBuilder,
				Scene,
				//@todo - this breaks second view if far away
				View,
				Light.LightSceneInfo,
				DFPT_SignedDistanceField,
				ObjectBufferParameters,
				WorldToMeshSDFShadowValue,
				LightTileIntersectionParameters);
		}

		if (bTraceHeighfieldObjects)
		{
			FLightTileIntersectionParameters LightTileHeightfieldIntersectionParameters;

			CullMeshObjectsForLightCards(
				GraphBuilder, 
				Scene, 
				View, 
				Light.LightSceneInfo,
				DFPT_HeightField,
				ObjectBufferParameters,
				WorldToMeshSDFShadowValue,
				LightTileHeightfieldIntersectionParameters);

			if (!bTraceMeshSDFs)
			{
				LightTileIntersectionParameters = LightTileHeightfieldIntersectionParameters;
			}

			LightTileIntersectionParameters.HeightfieldShadowTileNumCulledObjects = LightTileHeightfieldIntersectionParameters.ShadowTileNumCulledObjects;
			LightTileIntersectionParameters.HeightfieldShadowTileStartOffsets = LightTileHeightfieldIntersectionParameters.ShadowTileStartOffsets;
			LightTileIntersectionParameters.HeightfieldShadowTileArrayData = LightTileHeightfieldIntersectionParameters.ShadowTileArrayData;
		}

		FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FParameters>();
		SetCommonParameters(PassParameters, LightTileIntersectionParameters, WorldToMeshSDFShadowValue);
		SetPerLightParameters(PassParameters->LightParameters, Light, ViewIndex);

		FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FThreadGroupSize32>(bThreadGroupSize32);
		PermutationVector.Set<FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FLightType>(Light.Type);
		PermutationVector.Set<FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FTraceGlobalSDF>(bTraceGlobalSDF);
		PermutationVector.Set<FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FSimpleCoverageBasedExpand>(bSimpleCoverageBasedExpand);
		PermutationVector.Set<FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FTraceMeshSDFs>(bTraceMeshSDFs);
		PermutationVector.Set<FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FTraceHeightfields>(bTraceHeighfieldObjects);
		PermutationVector.Set<FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FOffsetDataStructure>(GDistanceFieldOffsetDataStructure);
		PermutationVector = FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::RemapPermutation(PermutationVector);

		TShaderRef<FLumenSceneDirectLightingTraceDistanceFieldShadowsCS> ComputeShader = View.ShaderMap->GetShader<FLumenSceneDirectLightingTraceDistanceFieldShadowsCS>(PermutationVector);

		const uint32 DispatchIndirectArgOffset = (Light.LightIndex * NumViews + ViewIndex) * sizeof(FRHIDispatchIndirectParameters);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("DistanceFieldShadowPass %s", *Light.Name),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			LightTileScatterParameters.DispatchIndirectArgs,
			DispatchIndirectArgOffset);
	}

	for (int32 LightTypeIndex = 0; LightTypeIndex < (int32)ELumenLightType::MAX; ++LightTypeIndex)
	{
		TArray<FPerLightParameters>& BatchedLightParameters = ViewBatchedLightParameters.PerLightTypeParameters[LightTypeIndex];

		if (BatchedLightParameters.Num() > 0)
		{
			FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FParameters>();
			SetCommonParameters(PassParameters, FLightTileIntersectionParameters(), FMatrix::Identity);

			FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FThreadGroupSize32>(bThreadGroupSize32);
			PermutationVector.Set<FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FLightType>((ELumenLightType)LightTypeIndex);
			PermutationVector.Set<FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FTraceGlobalSDF>(bTraceGlobalSDF);
			PermutationVector.Set<FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FSimpleCoverageBasedExpand>(bSimpleCoverageBasedExpand);
			PermutationVector.Set<FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FTraceMeshSDFs>(false);
			PermutationVector.Set<FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FTraceHeightfields>(false);
			PermutationVector.Set<FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FOffsetDataStructure>(GDistanceFieldOffsetDataStructure);
			PermutationVector = FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::RemapPermutation(PermutationVector);

			TShaderRef<FLumenSceneDirectLightingTraceDistanceFieldShadowsCS> ComputeShader = View.ShaderMap->GetShader<FLumenSceneDirectLightingTraceDistanceFieldShadowsCS>(PermutationVector);

			const FShaderParametersMetadata* ParametersMetaData = FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FParameters::FTypeInfo::GetStructMetadata();
			FRDGBufferRef IndirectArgsBuffer = LightTileScatterParameters.DispatchIndirectArgs;
			ClearUnusedGraphResourcesImpl(ComputeShader->Bindings, ParametersMetaData, PassParameters, { IndirectArgsBuffer });

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("DistanceFieldShadowPass LightType=%d BatchedNum=%d", LightTypeIndex, BatchedLightParameters.Num()),
				PassParameters,
				ComputePassFlags,
				[PassParameters, ComputeShader, IndirectArgsBuffer, NumViews, ViewIndex, LocalBatchedLightParameters = MoveTemp(BatchedLightParameters)](FRHIComputeCommandList& RHICmdList) mutable
				{
					// Marks the indirect draw parameter as used by the pass manually, given it can't be bound directly by any of the shader,
					// meaning SetShaderParameters() won't be able to do it.
					IndirectArgsBuffer->MarkResourceAsUsed();

					for (FPerLightParameters& LightParameterValues : LocalBatchedLightParameters)
					{
						const uint32 IndirectArgsOffset = (LightParameterValues.LightIndex * NumViews + ViewIndex) * sizeof(FRHIDispatchIndirectParameters);

						// TODO: Only set changed paramters
						PassParameters->LightParameters = MoveTemp(LightParameterValues);
						FComputeShaderUtils::DispatchIndirect(RHICmdList, ComputeShader, *PassParameters, IndirectArgsBuffer->GetIndirectRHICallBuffer(), IndirectArgsOffset);
					}
				});
		}
	}
}

// Must match FLumenPackedLight in LumenSceneDirectLighting.ush
struct FLumenPackedLight
{
	FVector3f WorldPosition;
	float InvRadius;

	FVector3f Color;
	float FalloffExponent;

	FVector3f Direction;
	float SpecularScale;

	FVector3f Tangent;
	float SourceRadius;

	FVector2f SpotAngles;
	float SoftSourceRadius;
	float SourceLength;

	float RectLightBarnCosAngle;
	float RectLightBarnLength;
	uint32 LightType;
	uint32 VirtualShadowMapId;

	FVector4f InfluenceSphere;

	FVector3f ProxyPosition;
	float ProxyRadius;

	FVector3f ProxyDirection;
	float RectLightAtlasMaxLevel;
	
	FVector2f SinCosConeAngleOrRectLightAtlasUVScale;
	FVector2f RectLightAtlasUVOffset;

	uint32 LightingChannelMask;
	uint32 LightFunctionAtlasIndex_bHasShadowMask;
	float IESAtlasIndex;
	float InverseExposureBlend;
};

struct FLightTileCullContext
{
	FLumenLightTileScatterParameters LightTileScatterParameters;
	FRDGBufferRef LightTileAllocator;
	FRDGBufferRef LightTiles;
	FRDGBufferRef DispatchLightTilesIndirectArgs;

	FRDGBufferRef LightTileOffsetNumPerCardTile;
	FRDGBufferRef LightTilesPerCardTile;
	uint32 MaxCulledCardTiles;
};

// Build list of surface cache tiles per light for future processing
static void CullDirectLightingTiles(
	const TArray<FViewInfo>& Views,
	FRDGBuilder& GraphBuilder,
	const FLumenCardUpdateContext& CardUpdateContext,
	TRDGUniformBufferRef<FLumenCardScene> LumenCardSceneUniformBuffer,
	TConstArrayView<FLumenGatheredLight> GatheredLights,
	FRDGBufferRef LumenPackedLights,
	FLightTileCullContext& CullContext,
	FLumenCardTileUpdateContext& CardTileUpdateCotnext,
	ERDGPassFlags ComputePassFlags)
{
	RDG_EVENT_SCOPE(GraphBuilder, "CullTiles %d lights", GatheredLights.Num());
	const FGlobalShaderMap* GlobalShaderMap = Views[0].ShaderMap;

	const uint32 MaxLightTiles = CardUpdateContext.MaxUpdateTiles;
	const uint32 NumLightsRoundedUp = FMath::RoundUpToPowerOfTwo(FMath::Max(GatheredLights.Num(), 1)) * Views.Num();
	const uint32 MaxLightsPerTile = FMath::RoundUpToPowerOfTwo(FMath::Clamp(CVarLumenDirectLightingMaxLightsPerTile.GetValueOnRenderThread(), 1, 32));
	const uint32 MaxCulledCardTiles = MaxLightsPerTile * MaxLightTiles;

	Lumen::SpliceCardPagesIntoTiles(GraphBuilder, GlobalShaderMap, CardUpdateContext, LumenCardSceneUniformBuffer, CardTileUpdateCotnext, ComputePassFlags);

	FRDGBufferRef CardTileAllocator = CardTileUpdateCotnext.CardTileAllocator;
	FRDGBufferRef CardTiles = CardTileUpdateCotnext.CardTiles;
	FRDGBufferRef DispatchCardTilesIndirectArgs = CardTileUpdateCotnext.DispatchCardTilesIndirectArgs;

	FRDGBufferRef LightTileAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("Lumen.DirectLighting.LightTileAllocator"));
	FRDGBufferRef LightTiles = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(2 * sizeof(uint32), MaxCulledCardTiles), TEXT("Lumen.DirectLighting.LightTiles"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(LightTileAllocator), 0, ComputePassFlags);

	FRDGBufferRef LightTileAllocatorPerLight = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), NumLightsRoundedUp), TEXT("Lumen.DirectLighting.LightTileAllocatorPerLight"));
	FRDGBufferRef LightTileOffsetsPerLight = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), NumLightsRoundedUp), TEXT("Lumen.DirectLighting.LightTileOffsetsPerLight"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(LightTileAllocatorPerLight), 0, ComputePassFlags);

	// Used to figure out the offset to store light tiles for each card tile
	FRDGBufferRef LightTileAllocatorForPerCardTileDispatch = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("Lumen.DirectLighting.LightTileAllocatorForPerCardTileDispatch"));
	FRDGBufferRef LightTileOffsetNumPerCardTile = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), MaxLightTiles), TEXT("Lumen.DirectLighting.LightTileOffsetNumPerCardTile"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(LightTileAllocatorForPerCardTileDispatch), 0, ComputePassFlags);

	// Build a list of light tiles for future processing
	{
		FBuildLightTilesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FBuildLightTilesCS::FParameters>();
		PassParameters->IndirectArgBuffer = DispatchCardTilesIndirectArgs;
		PassParameters->View = Views[0].ViewUniformBuffer;
		PassParameters->LumenCardScene = LumenCardSceneUniformBuffer;
		PassParameters->LumenPackedLights = GraphBuilder.CreateSRV(LumenPackedLights);
		PassParameters->RWLightTileAllocator = GraphBuilder.CreateUAV(LightTileAllocator);
		PassParameters->RWLightTileAllocatorForPerCardTileDispatch = GraphBuilder.CreateUAV(LightTileAllocatorForPerCardTileDispatch);
		PassParameters->RWLightTiles = GraphBuilder.CreateUAV(LightTiles);
		PassParameters->RWLightTileAllocatorPerLight = GraphBuilder.CreateUAV(LightTileAllocatorPerLight);
		PassParameters->RWLightTileOffsetNumPerCardTile = GraphBuilder.CreateUAV(LightTileOffsetNumPerCardTile);
		PassParameters->CardTileAllocator = GraphBuilder.CreateSRV(CardTileAllocator);
		PassParameters->CardTiles = GraphBuilder.CreateSRV(CardTiles);
		PassParameters->MaxLightsPerTile = MaxLightsPerTile;
		PassParameters->NumLights = GatheredLights.Num();
		PassParameters->NumViews = Views.Num();
		check(Views.Num() <= PassParameters->WorldToClip.Num());

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			PassParameters->WorldToClip[ViewIndex] = FMatrix44f(Views[ViewIndex].ViewMatrices.GetViewProjectionMatrix());
			PassParameters->PreViewTranslation[ViewIndex] = FVector4f((FVector3f)Views[ViewIndex].ViewMatrices.GetPreViewTranslation(), 0.0f);
			PassParameters->ViewExposure[ViewIndex] = Views[ViewIndex].GetLastEyeAdaptationExposure();
		}

		FBuildLightTilesCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FBuildLightTilesCS::FMaxLightSamples>(MaxLightsPerTile);

		auto ComputeShader = GlobalShaderMap->GetShader<FBuildLightTilesCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("BuildLightTiles"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			DispatchCardTilesIndirectArgs,
			(uint32)ELumenDispatchCardTilesIndirectArgsOffset::OneThreadPerCardTile);
	}

	// Compute prefix sum for card tile array
	{
		FComputeLightTileOffsetsPerLightCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FComputeLightTileOffsetsPerLightCS::FParameters>();
		PassParameters->RWLightTileOffsetsPerLight = GraphBuilder.CreateUAV(LightTileOffsetsPerLight);
		PassParameters->LightTileAllocatorPerLight = GraphBuilder.CreateSRV(LightTileAllocatorPerLight);
		PassParameters->NumLights = GatheredLights.Num();
		PassParameters->NumViews = Views.Num();

		auto ComputeShader = GlobalShaderMap->GetShader<FComputeLightTileOffsetsPerLightCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ComputeLightTileOffsetsPerLight"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	enum class EDispatchTilesIndirectArgOffset
	{
		NumTilesDiv1 = 0 * sizeof(FRHIDispatchIndirectParameters),
		NumTilesDiv64 = 1 * sizeof(FRHIDispatchIndirectParameters),
		MAX = 2,
	};

	// Initialize indirect args for culled tiles
	FRDGBufferRef DispatchLightTilesIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>((int32)EDispatchTilesIndirectArgOffset::MAX), TEXT("Lumen.DirectLighting.DispatchLightTilesIndirectArgs"));
	FRDGBufferRef DrawTilesPerLightIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDrawIndirectParameters>(NumLightsRoundedUp), TEXT("Lumen.DirectLighting.DrawTilesPerLightIndirectArgs"));
	FRDGBufferRef DispatchTilesPerLightIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(NumLightsRoundedUp), TEXT("Lumen.DirectLighting.DispatchTilesPerLightIndirectArgs"));
	{
		FInitializeLightTileIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitializeLightTileIndirectArgsCS::FParameters>();
		PassParameters->RWDispatchLightTilesIndirectArgs = GraphBuilder.CreateUAV(DispatchLightTilesIndirectArgs);
		PassParameters->RWDrawTilesPerLightIndirectArgs = GraphBuilder.CreateUAV(DrawTilesPerLightIndirectArgs);
		PassParameters->RWDispatchTilesPerLightIndirectArgs = GraphBuilder.CreateUAV(DispatchTilesPerLightIndirectArgs);
		PassParameters->LightTileAllocator = GraphBuilder.CreateSRV(LightTileAllocator);
		PassParameters->LightTileAllocatorPerLight = GraphBuilder.CreateSRV(LightTileAllocatorPerLight);
		PassParameters->VertexCountPerInstanceIndirect = GRHISupportsRectTopology ? 3 : 6;
		PassParameters->PerLightDispatchFactor = Lumen::UseThreadGroupSize32() ? 2 : 1;
		PassParameters->NumLights = GatheredLights.Num();
		PassParameters->NumViews = Views.Num();

		auto ComputeShader = GlobalShaderMap->GetShader<FInitializeLightTileIndirectArgsCS>();

		const FIntVector GroupSize = FComputeShaderUtils::GetGroupCount(
			FMath::Max(GatheredLights.Num() * Views.Num(), 1), // Dispatch at least one group in order to init global tile indirect arguments
			FInitializeLightTileIndirectArgsCS::GetGroupSize());

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("InitializeLightTileIndirectArgs"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			GroupSize);
	}

	FRDGBufferRef LightTilesPerCardTile = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(2 * sizeof(uint32), MaxCulledCardTiles), TEXT("Lumen.DirectLighting.LightTilesPerCardTile"));

	// Compact card tile array
	{
		FRDGBufferRef CompactedLightTiles = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(2 * sizeof(uint32), MaxCulledCardTiles), TEXT("Lumen.DirectLighting.CompactedLightTiles"));
		FRDGBufferRef CompactedLightTileAllocatorPerLight = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), NumLightsRoundedUp), TEXT("Lumen.DirectLighting.CompactedLightTileAllocatorPerLight"));
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(CompactedLightTileAllocatorPerLight), 0, ComputePassFlags);

		FCompactLightTilesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCompactLightTilesCS::FParameters>();
		PassParameters->IndirectArgBuffer = DispatchLightTilesIndirectArgs;
		PassParameters->RWCompactedLightTiles = GraphBuilder.CreateUAV(CompactedLightTiles);
		PassParameters->RWCompactedLightTileAllocatorPerLight = GraphBuilder.CreateUAV(CompactedLightTileAllocatorPerLight);
		PassParameters->RWLightTilesPerCardTile = GraphBuilder.CreateUAV(LightTilesPerCardTile);
		PassParameters->LightTileAllocator = GraphBuilder.CreateSRV(LightTileAllocator);
		PassParameters->LightTiles = GraphBuilder.CreateSRV(LightTiles);
		PassParameters->LightTileOffsetsPerLight = GraphBuilder.CreateSRV(LightTileOffsetsPerLight);
		PassParameters->CardTiles = GraphBuilder.CreateSRV(CardTiles);
		PassParameters->LightTileOffsetNumPerCardTile = GraphBuilder.CreateSRV(LightTileOffsetNumPerCardTile);
		PassParameters->NumLights = GatheredLights.Num();
		PassParameters->NumViews = Views.Num();

		auto ComputeShader = GlobalShaderMap->GetShader<FCompactLightTilesCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CompactLightTiles"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			DispatchLightTilesIndirectArgs,
			(int32)EDispatchTilesIndirectArgOffset::NumTilesDiv64);

		LightTiles = CompactedLightTiles;
	}

	CullContext.LightTileScatterParameters.DrawIndirectArgs = DrawTilesPerLightIndirectArgs;
	CullContext.LightTileScatterParameters.DispatchIndirectArgs = DispatchTilesPerLightIndirectArgs;
	CullContext.LightTileScatterParameters.LightTileAllocator = GraphBuilder.CreateSRV(LightTileAllocator);
	CullContext.LightTileScatterParameters.LightTiles = GraphBuilder.CreateSRV(LightTiles);
	CullContext.LightTileScatterParameters.LightTileOffsetsPerLight = GraphBuilder.CreateSRV(LightTileOffsetsPerLight);

	CullContext.LightTiles = LightTiles;
	CullContext.LightTileAllocator = LightTileAllocator;
	CullContext.DispatchLightTilesIndirectArgs = DispatchLightTilesIndirectArgs;

	CullContext.LightTileOffsetNumPerCardTile = LightTileOffsetNumPerCardTile;
	CullContext.LightTilesPerCardTile = LightTilesPerCardTile;
	CullContext.MaxCulledCardTiles = MaxCulledCardTiles;
}

struct FLumenDirectLightingTaskData
{
	mutable UE::Tasks::FTask Task;
	TArray<FLumenGatheredLight, TInlineAllocator<64>> GatheredLights;
	TArray<FLumenPackedLight, TInlineAllocator<16>> PackedLightData;
	// Note: All batched lights cast ray traced shadows
	mutable TArray<FViewBatchedLightParameters, TInlineAllocator<1>> ViewBatchedLightParameters;
	// Note: All standalone (non-batched) lights need shadow masks but may not cast ray traced shadows
	TArray<int32, TInlineAllocator<4>> StandaloneLightIndices;
	bool bHasRectLights = false;
	bool bHasLightFunctions = false;
};

void FDeferredShadingSceneRenderer::BeginGatherLumenLights(FLumenDirectLightingTaskData*& TaskData, IVisibilityTaskData* VisibilityTaskData)
{
	bool bAnyLumenActive = false;

	for (const FViewInfo& View : Views)
	{
		const FPerViewPipelineState& ViewPipelineState = GetViewPipelineState(View);
		bAnyLumenActive |= ViewPipelineState.DiffuseIndirectMethod == EDiffuseIndirectMethod::Lumen;
	}

	if (!bAnyLumenActive || CVarLumenLumenSceneDirectLighting.GetValueOnRenderThread() == 0)
	{
		return;
	}

	TaskData = Allocator.Create<FLumenDirectLightingTaskData>();

	TaskData->Task = LaunchSceneRenderTask(TEXT("GatherLumenLights"), [TaskData, Scene = this->Scene, &Views = this->Views, &ViewFamily = this->ViewFamily]
	{
		SCOPED_NAMED_EVENT_TEXT("GatherLumenLights", FColor::Green);

		const bool bUseHardwareRayTracing = Lumen::UseHardwareRayTracedDirectLighting(ViewFamily);
		const bool bUseBatchedShadows = CVarLumenDirectLightingBatchShadows.GetValueOnAnyThread() != 0;
		constexpr int32 NumLightTypes = (int32)ELumenLightType::MAX;
		int32 BatchedLightCounts[NumLightTypes] = {};

		for (auto LightIt = Scene->Lights.CreateConstIterator(); LightIt; ++LightIt)
		{
			const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
			const FLightSceneInfo* LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

			if (LightSceneInfo->ShouldRenderLightViewIndependent()
				&& LightSceneInfo->Proxy->GetIndirectLightingScale() > 0.0f)
			{
				for (const FViewInfo& View : Views)
				{
					if (LightSceneInfo->ShouldRenderLight(View, true))
					{
						const FLumenGatheredLight GatheredLight(Scene, Views, LightSceneInfo, /*LightIndex*/ TaskData->GatheredLights.Num(), bUseHardwareRayTracing);

						if (GatheredLight.NeedsShadowMask())
						{
							if (bUseBatchedShadows && GatheredLight.CanUseBatchedShadows())
							{
								++BatchedLightCounts[(int32)GatheredLight.Type];
							}
							else
							{
								TaskData->StandaloneLightIndices.Add(GatheredLight.LightIndex);
							}
						}

						TaskData->bHasRectLights |= GatheredLight.Type == ELumenLightType::Rect;
						TaskData->bHasLightFunctions |= GatheredLight.LightFunctionMaterialProxy != nullptr;
						TaskData->GatheredLights.Add(GatheredLight);
						break;
					}
				}
			}
		}

		TaskData->PackedLightData.SetNum(FMath::RoundUpToPowerOfTwo(FMath::Max(TaskData->GatheredLights.Num(), 16)));

		TaskData->ViewBatchedLightParameters.SetNum(Views.Num());
		for (FViewBatchedLightParameters& ViewLightParameters : TaskData->ViewBatchedLightParameters)
		{
			for (int32 LightTypeIndex = 0; LightTypeIndex < NumLightTypes; ++LightTypeIndex)
			{
				ViewLightParameters.PerLightTypeParameters[LightTypeIndex].Empty(BatchedLightCounts[LightTypeIndex]);
			}
		}

		for (int32 LightIndex = 0; LightIndex < TaskData->GatheredLights.Num(); ++LightIndex)
		{
			const FLumenGatheredLight& LumenLight = TaskData->GatheredLights[LightIndex];
			const FLightSceneInfo* LightSceneInfo = LumenLight.LightSceneInfo;
			const FSphere LightBounds = LightSceneInfo->Proxy->GetBoundingSphere();

			FLightRenderParameters ShaderParameters;
			LightSceneInfo->Proxy->GetLightShaderParameters(ShaderParameters);

			if (LightSceneInfo->Proxy->IsInverseSquared())
			{
				ShaderParameters.FalloffExponent = 0;
			}
			ShaderParameters.Color *= LightSceneInfo->Proxy->GetIndirectLightingScale();
			// InverseExposureBlend applied in shader since it's view dependent

			FLumenPackedLight& LightData = TaskData->PackedLightData[LightIndex];
			LightData.WorldPosition = FVector3f(ShaderParameters.WorldPosition);
			LightData.InvRadius = ShaderParameters.InvRadius;

			LightData.Color = FVector3f(ShaderParameters.Color);
			LightData.FalloffExponent = ShaderParameters.FalloffExponent;

			LightData.Direction = ShaderParameters.Direction;
			LightData.SpecularScale = ShaderParameters.SpecularScale;

			LightData.Tangent = ShaderParameters.Tangent;
			LightData.SourceRadius = ShaderParameters.SourceRadius;

			LightData.SpotAngles = ShaderParameters.SpotAngles;
			LightData.SoftSourceRadius = ShaderParameters.SoftSourceRadius;
			LightData.SourceLength = ShaderParameters.SourceLength;

			LightData.RectLightBarnCosAngle = ShaderParameters.RectLightBarnCosAngle;
			LightData.RectLightBarnLength = ShaderParameters.RectLightBarnLength;
			LightData.LightType = LightSceneInfo->Proxy->GetLightType();
			LightData.VirtualShadowMapId = 0;

			LightData.InfluenceSphere = FVector4f((FVector3f)LightBounds.Center, LightBounds.W);

			LightData.ProxyPosition = FVector4f(LightSceneInfo->Proxy->GetPosition()); // LUMEN_LWC_TODO
			LightData.ProxyRadius = LightSceneInfo->Proxy->GetRadius();

			LightData.ProxyDirection = (FVector3f)LightSceneInfo->Proxy->GetDirection();
			LightData.RectLightAtlasMaxLevel = ShaderParameters.RectLightAtlasMaxLevel > 0.0f ? ShaderParameters.RectLightAtlasMaxLevel : 0.0f;

			if (LightData.LightType == LightType_Rect)
			{
				LightData.SinCosConeAngleOrRectLightAtlasUVScale = ShaderParameters.RectLightAtlasUVScale;
			}
			else
			{
				LightData.SinCosConeAngleOrRectLightAtlasUVScale = FVector2f(FMath::Sin(LightSceneInfo->Proxy->GetOuterConeAngle()), FMath::Cos(LightSceneInfo->Proxy->GetOuterConeAngle()));
			}
			LightData.RectLightAtlasUVOffset = ShaderParameters.RectLightAtlasUVOffset;
			LightData.IESAtlasIndex = ShaderParameters.IESAtlasIndex;
			LightData.LightFunctionAtlasIndex_bHasShadowMask = ShaderParameters.LightFunctionAtlasLightIndex | (LumenLight.NeedsShadowMask() ? (1 << 31) : 0);
			LightData.LightingChannelMask = LightSceneInfo->Proxy->GetLightingChannelMask();
			LightData.InverseExposureBlend = ShaderParameters.InverseExposureBlend;

			if (bUseBatchedShadows && LumenLight.NeedsShadowMask() && LumenLight.CanUseBatchedShadows())
			{
				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
				{
					FPerLightParameters& LightParameters = TaskData->ViewBatchedLightParameters[ViewIndex].PerLightTypeParameters[(int32)LumenLight.Type].AddDefaulted_GetRef();
					SetPerLightParameters(LightParameters, LumenLight, ViewIndex);
				}
			}
		}

#if DO_CHECK
		for (FViewBatchedLightParameters& ViewLightParameters : TaskData->ViewBatchedLightParameters)
		{
			for (int32 LightTypeIndex = 0; LightTypeIndex < NumLightTypes; ++LightTypeIndex)
			{
				check(ViewLightParameters.PerLightTypeParameters[LightTypeIndex].Num() == BatchedLightCounts[LightTypeIndex]);
			}
		}
#endif
	}, VisibilityTaskData->GetLightVisibilityTask());
}

void FDeferredShadingSceneRenderer::RenderDirectLightingForLumenScene(
	FRDGBuilder& GraphBuilder,
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	const FLumenDirectLightingTaskData* LightingTaskData,
	const FLumenCardUpdateContext& CardUpdateContext,
	ERDGPassFlags ComputePassFlags)
{
	LLM_SCOPE_BYTAG(Lumen);

	if (CVarLumenLumenSceneDirectLighting.GetValueOnRenderThread() != 0 && CardUpdateContext.MaxUpdateTiles > 0)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "DirectLighting");
		QUICK_SCOPE_CYCLE_COUNTER(RenderDirectLightingForLumenScene);

		check(LightingTaskData);
		LightingTaskData->Task.Wait();

		const FViewInfo& MainView = Views[0];
		FLumenSceneData& LumenSceneData = *Scene->GetLumenSceneData(Views[0]);

		TRDGUniformBufferRef<FLumenCardScene> LumenCardSceneUniformBuffer = FrameTemporaries.LumenCardSceneUniformBuffer;

		TConstArrayView<FLumenGatheredLight> GatheredLights = LightingTaskData->GatheredLights;
		const bool bHasRectLights = LightingTaskData->bHasRectLights;

		FRDGBufferRef LumenPackedLights = CreateStructuredBuffer(GraphBuilder, TEXT("Lumen.DirectLighting.Lights"), LightingTaskData->PackedLightData, ERDGInitialDataFlags::NoCopy);

		FLightTileCullContext CullContext;
		FLumenCardTileUpdateContext CardTileUpdateContext;
		CullDirectLightingTiles(Views, GraphBuilder, CardUpdateContext, LumenCardSceneUniformBuffer, GatheredLights, LumenPackedLights, CullContext, CardTileUpdateContext, ComputePassFlags);

		// 8 bits per shadow mask texel
		const uint32 ShadowMaskTilesSize = FMath::Max(16 * CullContext.MaxCulledCardTiles, 1024u);
		FRDGBufferRef ShadowMaskTiles = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), ShadowMaskTilesSize), TEXT("Lumen.DirectLighting.ShadowMaskTiles"));

		// 1 uint per packed shadow trace
		FRDGBufferRef ShadowTraceAllocator = nullptr;
		FRDGBufferRef ShadowTraces = nullptr;
		if (Lumen::UseHardwareRayTracedDirectLighting(ViewFamily))
		{
			const uint32 MaxShadowTraces = FMath::Max(Lumen::CardTileSize * Lumen::CardTileSize * CullContext.MaxCulledCardTiles, 1024u);

			ShadowTraceAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("Lumen.DirectLighting.ShadowTraceAllocator"));
			ShadowTraces = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), MaxShadowTraces), TEXT("Lumen.DirectLighting.ShadowTraces"));
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(ShadowTraceAllocator), 0);
		}

		// Compute shadow mask basd on light attenuation (IES/LightFunction/Distance fall) to reduce need for shadow tracing done after.
		{
			SCOPED_NAMED_EVENT_TEXT("Light Attenuation ShadowMask ", FColor::Green);
			RDG_EVENT_SCOPE_FINAL(GraphBuilder, "Light Attenuation ShadowMask");

			FRDGBufferUAVRef ShadowMaskTilesUAV = GraphBuilder.CreateUAV(ShadowMaskTiles, ERDGUnorderedAccessViewFlags::SkipBarrier);
			FRDGBufferUAVRef ShadowTraceAllocatorUAV = ShadowTraceAllocator ? GraphBuilder.CreateUAV(ShadowTraceAllocator, ERDGUnorderedAccessViewFlags::SkipBarrier) : nullptr;
			FRDGBufferUAVRef ShadowTracesUAV = ShadowTraces ? GraphBuilder.CreateUAV(ShadowTraces, ERDGUnorderedAccessViewFlags::SkipBarrier) : nullptr;

			int32 NumShadowedLights = 0;
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				const FViewInfo& View = Views[ViewIndex];

				NumShadowedLights = ComputeShadowMaskFromLightAttenuation(
					GraphBuilder,
					Scene,
					View,
					LumenCardSceneUniformBuffer,
					GatheredLights,
					LightingTaskData->StandaloneLightIndices,
					LightingTaskData->ViewBatchedLightParameters[ViewIndex],
					CullContext.LightTileScatterParameters,
					ViewIndex,
					Views.Num(),
					LightingTaskData->bHasLightFunctions,
					ShadowMaskTilesUAV,
					ShadowTraceAllocatorUAV,
					ShadowTracesUAV,
					ComputePassFlags);
			}

			// Clear to mark resource as used if it wasn't ever written to
			if (ShadowTracesUAV && NumShadowedLights == 0)
			{
				AddClearUAVPass(GraphBuilder, ShadowTracesUAV, 0);
			}
		}

		FRDGBufferRef ShadowTraceIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.DirectLighting.CompactedShadowTraceIndirectArgs"));
		if (ShadowTraceAllocator)
		{
			FInitShadowTraceIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitShadowTraceIndirectArgsCS::FParameters>();
			PassParameters->RWShadowTraceIndirectArgs = GraphBuilder.CreateUAV(ShadowTraceIndirectArgs);
			PassParameters->ShadowTraceAllocator = GraphBuilder.CreateSRV(ShadowTraceAllocator);

			auto ComputeShader = Views[0].ShaderMap->GetShader<FInitShadowTraceIndirectArgsCS>();

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("InitShadowTraceIndirectArgs"),
				ComputePassFlags,
				ComputeShader,
				PassParameters,
				FIntVector(1, 1, 1));
		}

		// Offscreen shadowing
		{
			SCOPED_NAMED_EVENT_TEXT("Offscreen shadows", FColor::Green);
			RDG_EVENT_SCOPE_FINAL(GraphBuilder, "Offscreen shadows");

			FRDGBufferUAVRef ShadowMaskTilesUAV = GraphBuilder.CreateUAV(ShadowMaskTiles, ERDGUnorderedAccessViewFlags::SkipBarrier);

			FDistanceFieldObjectBufferParameters ObjectBufferParameters;

			const bool bUseHardwareRayTracedDirectLighting = Lumen::UseHardwareRayTracedDirectLighting(ViewFamily);

			if (!bUseHardwareRayTracedDirectLighting)
			{
				ObjectBufferParameters = DistanceField::SetupObjectBufferParameters(GraphBuilder, Scene->DistanceFieldSceneData);

				// Patch DF heightfields with Lumen heightfields
				ObjectBufferParameters.SceneHeightfieldObjectBounds = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(LumenSceneData.HeightfieldBuffer));
				ObjectBufferParameters.SceneHeightfieldObjectData = nullptr;
				ObjectBufferParameters.NumSceneHeightfieldObjects = LumenSceneData.Heightfields.Num();
			}

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				const FViewInfo& View = Views[ViewIndex];

				if (bUseHardwareRayTracedDirectLighting)
				{
					TraceLumenHardwareRayTracedDirectLightingShadows(
						GraphBuilder,
						Scene,
						View,
						ViewIndex,
						FrameTemporaries,
						ShadowTraceIndirectArgs,
						ShadowTraceAllocator,
						ShadowTraces,
						CullContext.LightTileAllocator,
						CullContext.LightTiles,
						LumenPackedLights,
						ShadowMaskTilesUAV,
						ComputePassFlags);
				}
				else
				{
					TraceDistanceFieldShadows(
						GraphBuilder,
						Scene,
						View,
						LumenCardSceneUniformBuffer,
						GatheredLights,
						LightingTaskData->StandaloneLightIndices,
						LightingTaskData->ViewBatchedLightParameters[ViewIndex],
						CullContext.LightTileScatterParameters,
						ObjectBufferParameters,
						ViewIndex,
						Views.Num(),
						ShadowMaskTilesUAV,
						ComputePassFlags);
				}
			}
		}

		// Apply lights
		{
			RDG_EVENT_SCOPE(GraphBuilder, "Lights");

			FRDGBufferSRVRef ShadowMaskTilesSRV = GraphBuilder.CreateSRV(ShadowMaskTiles->HasBeenProduced() ? ShadowMaskTiles : GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, sizeof(uint32)));
			FRDGBufferSRVRef LumenPackedLightsSRV = GraphBuilder.CreateSRV(LumenPackedLights);
			FRDGBufferSRVRef CardTilesSRV = GraphBuilder.CreateSRV(CardTileUpdateContext.CardTiles);
			FRDGBufferSRVRef LightTileOffsetNumPerCardTileSRV = GraphBuilder.CreateSRV(CullContext.LightTileOffsetNumPerCardTile);
			FRDGBufferSRVRef LightTilesPerCardTileSRV = GraphBuilder.CreateSRV(CullContext.LightTilesPerCardTile);
			FRDGTextureUAVRef DirectLightingAtlasUAV = GraphBuilder.CreateUAV(FrameTemporaries.DirectLightingAtlas);

			RenderDirectLightIntoLumenCardsBatched(
				GraphBuilder,
				Views,
				LumenCardSceneUniformBuffer,
				LumenPackedLightsSRV,
				ShadowMaskTilesSRV,
				CardTilesSRV,
				LightTileOffsetNumPerCardTileSRV,
				LightTilesPerCardTileSRV,
				DirectLightingAtlasUAV,
				CardTileUpdateContext.DispatchCardTilesIndirectArgs,
				bHasRectLights,
				ComputePassFlags);
		}

		// Update Final Lighting
		Lumen::CombineLumenSceneLighting(
			Scene,
			MainView,
			GraphBuilder,
			FrameTemporaries,
			CardUpdateContext,
			CardTileUpdateContext,
			ComputePassFlags);
	}
	else if (CVarLumenLumenSceneDirectLighting.GetValueOnRenderThread() == 0)
	{
		AddClearRenderTargetPass(GraphBuilder, FrameTemporaries.DirectLightingAtlas);
	}
}
