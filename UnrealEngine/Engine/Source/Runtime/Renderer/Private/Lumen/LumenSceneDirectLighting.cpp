// Copyright Epic Games, Inc. All Rights Reserved.

#include "LumenSceneLighting.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "VolumeLighting.h"
#include "DistanceFieldLightingShared.h"
#include "VirtualShadowMaps/VirtualShadowMapClipmap.h"
#include "VolumetricCloudRendering.h"
#include "LumenTracingUtils.h"

int32 GLumenDirectLighting = 1;
FAutoConsoleVariableRef CVarLumenDirectLighting(
	TEXT("r.LumenScene.DirectLighting"),
	GLumenDirectLighting,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GLumenDirectLightingForceForceShadowMaps = 0;
FAutoConsoleVariableRef CVarLumenDirectLightingForceShadowMaps(
	TEXT("r.LumenScene.DirectLighting.ForceShadowMaps"),
	GLumenDirectLightingForceForceShadowMaps,
	TEXT("Use shadow maps for all lights casting shadows."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenDirectLightingReuseShadowMaps = 1;
FAutoConsoleVariableRef CVarLumenDirectLightingReuseShadowMaps(
	TEXT("r.LumenScene.DirectLighting.ReuseShadowMaps"),
	GLumenDirectLightingReuseShadowMaps,
	TEXT("Whether to use shadow maps for shadowing Lumen Scene, where they are available (onscreen).  Offscreen areas will still use ray tracing."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

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

int32 GLumenDirectLightingVirtualShadowMap = 1;
FAutoConsoleVariableRef CVarLumenDirectLightingVirtualShadowMap(
	TEXT("r.LumenScene.DirectLighting.VirtualShadowMap"),
	GLumenDirectLightingVirtualShadowMap,
	TEXT("Whether to sample virtual shadow when avaible."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenDirectLightingShadowMapSamplingBias(
	TEXT("r.LumenScene.DirectLighting.ShadowMap.SamplingBias"),
	2.0f,
	TEXT("Bias for sampling shadow maps."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarLumenDirectLightingVirtualShadowMapSamplingBias(
	TEXT("r.LumenScene.DirectLighting.VirtualShadowMap.SamplingBias"),
	7.0f,
	TEXT("Bias for sampling virtual shadow maps."),
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

float LumenSceneDirectLighting::GetShadowMapSamplingBias()
{
	return FMath::Max(CVarLumenDirectLightingShadowMapSamplingBias.GetValueOnRenderThread(), 0.0f);
}

float LumenSceneDirectLighting::GetVirtualShadowMapSamplingBias()
{
	return FMath::Max(CVarLumenDirectLightingVirtualShadowMapSamplingBias.GetValueOnRenderThread(), 0.0f);
}

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

bool LumenSceneDirectLighting::UseVirtualShadowMaps()
{
	return GLumenDirectLightingVirtualShadowMap != 0;
}

bool LumenSceneDirectLighting::AllowShadowMaps(const FEngineShowFlags& EngineShowFlags)
{
	return GLumenDirectLightingReuseShadowMaps != 0 && EngineShowFlags.LumenReuseShadowMaps;
}

class FLumenGatheredLight
{
public:
	FLumenGatheredLight(const FScene* Scene, const FViewInfo& View, const FLightSceneInfo* InLightSceneInfo, uint32 InLightIndex)
	{
		LightIndex = InLightIndex;
		LightSceneInfo = InLightSceneInfo;
		bHasShadows = InLightSceneInfo->Proxy->CastsDynamicShadow();

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
			bHasCloudTransmittance = LightHasCloudShadow(Scene, View, LightSceneInfo);
		}

		LightFunctionMaterialProxy = Proxy->GetLightFunctionMaterial();
		if (LightFunctionMaterialProxy
			&& (!View.Family->EngineShowFlags.LightFunctions || !LightFunctionMaterialProxy->GetIncompleteMaterialWithFallback(Scene->GetFeatureLevel()).IsLightFunction()))
		{
			LightFunctionMaterialProxy = nullptr;
		}

		FTexture* IESTextureResource = Proxy->GetIESTextureResource();
		if (IESTextureResource && View.Family->EngineShowFlags.TexturedLightProfiles)
		{
			IESTexture = IESTextureResource->TextureRHI;
		}

		FSceneRenderer::GetLightNameForDrawEvent(Proxy, Name);
	}

	bool NeedsShadowMask() const
	{
		return bHasShadows
			|| bHasCloudTransmittance
			|| LightFunctionMaterialProxy
			|| IESTexture;
	}

	const FLightSceneInfo* LightSceneInfo = nullptr;
	const FMaterialRenderProxy* LightFunctionMaterialProxy = nullptr;
	FRHITexture* IESTexture = nullptr;
	uint32 LightIndex = 0;
	ELumenLightType Type = ELumenLightType::MAX;
	bool bHasShadows = false;
	bool bHasCloudTransmittance = false;
	FString Name;
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
		SHADER_PARAMETER_ARRAY(FMatrix44f, WorldToClip, [MaxLumenViews])
		SHADER_PARAMETER_ARRAY(FVector4f, PreViewTranslation, [MaxLumenViews])
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
	const FLumenCardTracingInputs& TracingInputs,
	FLumenCardUpdateContext CardUpdateContext)
{
	FClearLumenCardsParameters* PassParameters = GraphBuilder.AllocParameters<FClearLumenCardsParameters>();

	PassParameters->RenderTargets[0] = FRenderTargetBinding(TracingInputs.DirectLightingAtlas, ERenderTargetLoadAction::ELoad);
	PassParameters->VS.LumenCardScene = TracingInputs.LumenCardSceneUniformBuffer;
	PassParameters->VS.DrawIndirectArgs = CardUpdateContext.DrawCardPageIndicesIndirectArgs;
	PassParameters->VS.CardPageIndexAllocator = GraphBuilder.CreateSRV(CardUpdateContext.CardPageIndexAllocator);
	PassParameters->VS.CardPageIndexData = GraphBuilder.CreateSRV(CardUpdateContext.CardPageIndexData);
	PassParameters->VS.IndirectLightingAtlasSize = LumenSceneData.GetRadiosityAtlasSize();
	PassParameters->PS.View = View.ViewUniformBuffer;
	PassParameters->PS.LumenCardScene = TracingInputs.LumenCardSceneUniformBuffer;

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

void Lumen::SetDirectLightingDeferredLightUniformBuffer(
	const FViewInfo& View,
	const FLightSceneInfo* LightSceneInfo,
	TUniformBufferBinding<FDeferredLightUniformStruct>& UniformBuffer)
{
	FDeferredLightUniformStruct DeferredLightUniforms = GetDeferredLightParameters(View, *LightSceneInfo);
	if (LightSceneInfo->Proxy->IsInverseSquared())
	{
		DeferredLightUniforms.LightParameters.FalloffExponent = 0;
	}
	DeferredLightUniforms.LightParameters.Color *= LightSceneInfo->Proxy->GetIndirectLightingScale();

	UniformBuffer = CreateUniformBufferImmediate(DeferredLightUniforms, UniformBuffer_SingleDraw);
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
		SHADER_PARAMETER_ARRAY(FVector4f, PreViewTranslation, [MaxLumenViews])
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

BEGIN_SHADER_PARAMETER_STRUCT(FLumenDirectLightingNonRayTracedShadowsParameters, )
	RDG_BUFFER_ACCESS(IndirectArgBuffer, ERHIAccess::IndirectArgs)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWShadowMaskTiles)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWShadowTraceAllocator)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWShadowTraces)
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenCardScene, LumenCardScene)
	SHADER_PARAMETER_STRUCT_INCLUDE(FLumenLightTileScatterParameters, LightTileScatterParameters)
	SHADER_PARAMETER(uint32, CardScatterInstanceIndex)
	SHADER_PARAMETER(uint32, LightIndex)
	SHADER_PARAMETER(uint32, ViewIndex)
	SHADER_PARAMETER(uint32, NumViews)
	SHADER_PARAMETER(uint32, DummyZeroForFixingShaderCompilerBug)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightData, ForwardLightData)
	SHADER_PARAMETER_STRUCT_REF(FDeferredLightUniformStruct, DeferredLightUniforms)
	SHADER_PARAMETER_STRUCT_INCLUDE(FVirtualShadowMapSamplingParameters, VirtualShadowMapSamplingParameters)
	SHADER_PARAMETER_STRUCT_INCLUDE(FVolumeShadowingShaderParameters, VolumeShadowingShaderParameters)
	SHADER_PARAMETER_STRUCT_INCLUDE(FLightCloudTransmittanceParameters, LightCloudTransmittanceParameters)
	SHADER_PARAMETER(float, ShadowMapSamplingBias)
	SHADER_PARAMETER(float, VirtualShadowMapSamplingBias)
	SHADER_PARAMETER(float, HeightfieldShadowReceiverBias)
	SHADER_PARAMETER(float, StepFactor)
	SHADER_PARAMETER(float, TanLightSourceAngle)
	SHADER_PARAMETER(float, MaxTraceDistance)
	SHADER_PARAMETER(int32, VirtualShadowMapId)
	SHADER_PARAMETER(uint32, SampleDenseShadowMap)
	SHADER_PARAMETER(uint32, ForceShadowMaps)
	SHADER_PARAMETER(uint32, UseIESProfile)
	SHADER_PARAMETER_TEXTURE(Texture2D, IESTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, IESTextureSampler)
END_SHADER_PARAMETER_STRUCT()

class FLumenDirectLightingNonRayTracedShadowsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLumenDirectLightingNonRayTracedShadowsCS)
	SHADER_USE_PARAMETER_STRUCT(FLumenDirectLightingNonRayTracedShadowsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenDirectLightingNonRayTracedShadowsParameters, Common)
	END_SHADER_PARAMETER_STRUCT()

	class FThreadGroupSize32 : SHADER_PERMUTATION_BOOL("THREADGROUP_SIZE_32");
	class FCompactShadowTraces : SHADER_PERMUTATION_BOOL("COMPACT_SHADOW_TRACES");
	class FLightType : SHADER_PERMUTATION_ENUM_CLASS("LIGHT_TYPE", ELumenLightType);
	class FDynamicallyShadowed : SHADER_PERMUTATION_BOOL("DYNAMICALLY_SHADOWED");
	class FVirtualShadowMap : SHADER_PERMUTATION_BOOL("VIRTUAL_SHADOW_MAP");
	class FDenseShadowMap : SHADER_PERMUTATION_BOOL("DENSE_SHADOW_MAP");
	class FCloudTransmittance : SHADER_PERMUTATION_BOOL("USE_CLOUD_TRANSMITTANCE");
	using FPermutationDomain = TShaderPermutationDomain<FThreadGroupSize32, FCompactShadowTraces, FLightType, FDynamicallyShadowed, FVirtualShadowMap, FDenseShadowMap, FCloudTransmittance>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (PermutationVector.Get<FCloudTransmittance>() && PermutationVector.Get<FLightType>() != ELumenLightType::Directional)
		{
			return false;
		}

		if (PermutationVector.Get<FDynamicallyShadowed>() && !PermutationVector.Get<FDenseShadowMap>())
		{
			return false;
		}

		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
		OutEnvironment.SetDefine(TEXT("LIGHT_FUNCTION"), 0);
		OutEnvironment.SetDefine(TEXT("USE_IES_PROFILE"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLumenDirectLightingNonRayTracedShadowsCS, "/Engine/Private/Lumen/LumenSceneDirectLightingShadowMask.usf", "LumenSceneDirectLightingNonRayTracedShadowsCS", SF_Compute);

BEGIN_SHADER_PARAMETER_STRUCT(FLightFunctionParameters, )
	SHADER_PARAMETER(FVector4f, LightFunctionParameters)
	SHADER_PARAMETER(FMatrix44f, LightFunctionTranslatedWorldToLight)
	SHADER_PARAMETER(FVector3f, LightFunctionParameters2)
END_SHADER_PARAMETER_STRUCT()

class FLumenDirectLightingNonRayTracedShadowsWithLightFunctionCS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FLumenDirectLightingNonRayTracedShadowsWithLightFunctionCS, Material);

	FLumenDirectLightingNonRayTracedShadowsWithLightFunctionCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
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

	FLumenDirectLightingNonRayTracedShadowsWithLightFunctionCS() {}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenDirectLightingNonRayTracedShadowsParameters, Common)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLightFunctionParameters, LightFunctionParameters)
	END_SHADER_PARAMETER_STRUCT()

	class FThreadGroupSize32 : SHADER_PERMUTATION_BOOL("THREADGROUP_SIZE_32");
	class FCompactShadowTraces : SHADER_PERMUTATION_BOOL("COMPACT_SHADOW_TRACES");
	class FLightType : SHADER_PERMUTATION_ENUM_CLASS("LIGHT_TYPE", ELumenLightType);
	class FDynamicallyShadowed : SHADER_PERMUTATION_BOOL("DYNAMICALLY_SHADOWED");
	class FVirtualShadowMap : SHADER_PERMUTATION_BOOL("VIRTUAL_SHADOW_MAP");
	class FDenseShadowMap : SHADER_PERMUTATION_BOOL("DENSE_SHADOW_MAP");
	class FCloudTransmittance : SHADER_PERMUTATION_BOOL("USE_CLOUD_TRANSMITTANCE");
	using FPermutationDomain = TShaderPermutationDomain<FThreadGroupSize32, FCompactShadowTraces, FLightType, FDynamicallyShadowed, FVirtualShadowMap, FDenseShadowMap, FCloudTransmittance>;

	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (PermutationVector.Get<FCloudTransmittance>() && PermutationVector.Get<FLightType>() != ELumenLightType::Directional)
		{
			return false;
		}

		if (PermutationVector.Get<FDynamicallyShadowed>() && !PermutationVector.Get<FDenseShadowMap>())
		{
			return false;
		}

		return Parameters.MaterialParameters.MaterialDomain == EMaterialDomain::MD_LightFunction && DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
		OutEnvironment.SetDefine(TEXT("LIGHT_FUNCTION"), 1);
		OutEnvironment.SetDefine(TEXT("STRATA_INLINE_SHADING"), 1);
		OutEnvironment.SetDefine(TEXT("USE_IES_PROFILE"), 1);
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FLumenDirectLightingNonRayTracedShadowsWithLightFunctionCS, TEXT("/Engine/Private/Lumen/LumenSceneDirectLightingShadowMask.usf"), TEXT("LumenSceneDirectLightingNonRayTracedShadowsCS"), SF_Compute);

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
		SHADER_PARAMETER_STRUCT_INCLUDE(FLumenLightTileScatterParameters, LightTileScatterParameters)
		SHADER_PARAMETER(uint32, LightIndex)
		SHADER_PARAMETER(uint32, ViewIndex)
		SHADER_PARAMETER(uint32, NumViews)
		SHADER_PARAMETER(uint32, DummyZeroForFixingShaderCompilerBug)
		SHADER_PARAMETER_STRUCT_REF(FDeferredLightUniformStruct, DeferredLightUniforms)
		SHADER_PARAMETER_STRUCT_INCLUDE(FDistanceFieldObjectBufferParameters, ObjectBufferParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLightTileIntersectionParameters, LightTileIntersectionParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FDistanceFieldAtlasParameters, DistanceFieldAtlasParameters)
		SHADER_PARAMETER(FMatrix44f, TranslatedWorldToShadow)
		SHADER_PARAMETER(float, TwoSidedMeshDistanceBiasScale)
		SHADER_PARAMETER(float, StepFactor)
		SHADER_PARAMETER(float, TanLightSourceAngle)
		SHADER_PARAMETER(float, MaxTraceDistance)
		SHADER_PARAMETER(float, MeshSDFShadowRayBias)
		SHADER_PARAMETER(float, HeightfieldShadowRayBias)
		SHADER_PARAMETER(float, GlobalSDFShadowRayBias)
		SHADER_PARAMETER(int32, HeightfieldMaxTracingSteps)
	END_SHADER_PARAMETER_STRUCT()

	class FThreadGroupSize32 : SHADER_PERMUTATION_BOOL("THREADGROUP_SIZE_32");
	class FLightType : SHADER_PERMUTATION_ENUM_CLASS("LIGHT_TYPE", ELumenLightType);
	class FTraceGlobalSDF : SHADER_PERMUTATION_BOOL("OFFSCREEN_SHADOWING_TRACE_GLOBAL_SDF");
	class FTraceMeshSDFs : SHADER_PERMUTATION_BOOL("OFFSCREEN_SHADOWING_TRACE_MESH_SDF");
	class FTraceHeightfields : SHADER_PERMUTATION_BOOL("OFFSCREEN_SHADOWING_TRACE_HEIGHTFIELDS");
	class FOffsetDataStructure : SHADER_PERMUTATION_INT("OFFSET_DATA_STRUCT", 3);
	using FPermutationDomain = TShaderPermutationDomain<FThreadGroupSize32, FLightType, FTraceGlobalSDF, FTraceMeshSDFs, FTraceHeightfields, FOffsetDataStructure>;

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
	const FVector LumenSceneExtent = FVector(ComputeMaxCardUpdateDistanceFromCamera(View));
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
		false,
		ObjectBufferParameters,
		CulledObjectBufferParameters,
		LightTileIntersectionParameters);
}

FLumenShadowSetup GetShadowForLumenDirectLighting(const FViewInfo& View, FVisibleLightInfo& VisibleLightInfo)
{
	FLumenShadowSetup ShadowSetup;
	ShadowSetup.VirtualShadowMapId = LumenSceneDirectLighting::UseVirtualShadowMaps() ? VisibleLightInfo.GetVirtualShadowMapId(&View) : INDEX_NONE;
	ShadowSetup.DenseShadowMap = nullptr;

	for (int32 ShadowIndex = 0; ShadowIndex < VisibleLightInfo.ShadowsToProject.Num(); ShadowIndex++)
	{
		FProjectedShadowInfo* ProjectedShadowInfo = VisibleLightInfo.ShadowsToProject[ShadowIndex];
		if (ProjectedShadowInfo->bIncludeInScreenSpaceShadowMask 
			&& ProjectedShadowInfo->bWholeSceneShadow 
			&& !ProjectedShadowInfo->bRayTracedDistanceField)
		{
			if (ProjectedShadowInfo->bAllocated)
			{
				ShadowSetup.DenseShadowMap = ProjectedShadowInfo;
			}
		}
	}

	return ShadowSetup;
}

const FProjectedShadowInfo* GetShadowForInjectionIntoVolumetricFog(const FVisibleLightInfo & VisibleLightInfo);

void RenderDirectLightIntoLumenCardsBatched(
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

void ComputeNonRayTracedShadows(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View,
	TRDGUniformBufferRef<FLumenCardScene> LumenCardSceneUniformBuffer,
	TArray<FVisibleLightInfo, SceneRenderingAllocator>& VisibleLightInfos,
	const FVirtualShadowMapArray& VirtualShadowMapArray,
	const FLumenGatheredLight& Light,
	const FLumenLightTileScatterParameters& LightTileScatterParameters,
	int32 ViewIndex,
	int32 NumViews,
	FRDGBufferUAVRef ShadowMaskTilesUAV,
	FRDGBufferUAVRef ShadowTraceAllocatorUAV,
	FRDGBufferUAVRef ShadowTracesUAV,
	ERDGPassFlags ComputePassFlags)
{
	FLumenSceneData& LumenSceneData = *Scene->GetLumenSceneData(View);
	check(Light.NeedsShadowMask());

	FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[Light.LightSceneInfo->Id];
	FLumenShadowSetup ShadowSetup = GetShadowForLumenDirectLighting(View, VisibleLightInfo);

	bool bUseVirtualShadowMap = false;
	bool bUseDenseShadowMap = false;

	if (Light.bHasShadows && LumenSceneDirectLighting::AllowShadowMaps(View.Family->EngineShowFlags))
	{
		bUseVirtualShadowMap = ShadowSetup.VirtualShadowMapId != INDEX_NONE;
		if (!bUseVirtualShadowMap)
		{
			// Fallback to a complete shadow map
			ShadowSetup.DenseShadowMap = GetShadowForInjectionIntoVolumetricFog(VisibleLightInfo);
		}
		bUseDenseShadowMap = ShadowSetup.DenseShadowMap != nullptr;
	}

	bool bUseCloudTransmittance = false;
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
		CommonParameters.LightIndex = Light.LightIndex;
		CommonParameters.ViewIndex = ViewIndex;
		CommonParameters.NumViews = NumViews;
		CommonParameters.DummyZeroForFixingShaderCompilerBug = 0;
		Lumen::SetDirectLightingDeferredLightUniformBuffer(View, Light.LightSceneInfo, CommonParameters.DeferredLightUniforms);
		CommonParameters.ForwardLightData = View.ForwardLightingResources.ForwardLightUniformBuffer;

		GetVolumeShadowingShaderParameters(
			GraphBuilder,
			View,
			Light.LightSceneInfo,
			ShadowSetup.DenseShadowMap,
			CommonParameters.VolumeShadowingShaderParameters);
		
		CommonParameters.VirtualShadowMapId = ShadowSetup.VirtualShadowMapId;
		if (bUseVirtualShadowMap)
		{
			CommonParameters.VirtualShadowMapSamplingParameters = VirtualShadowMapArray.GetSamplingParameters(GraphBuilder);
		}

		CommonParameters.TanLightSourceAngle = FMath::Tan(Light.LightSceneInfo->Proxy->GetLightSourceAngle());
		CommonParameters.MaxTraceDistance = Lumen::GetMaxTraceDistance(View);
		CommonParameters.StepFactor = FMath::Clamp(GOffscreenShadowingTraceStepFactor, .1f, 10.0f);
		CommonParameters.ShadowMapSamplingBias = LumenSceneDirectLighting::GetShadowMapSamplingBias();
		CommonParameters.VirtualShadowMapSamplingBias = LumenSceneDirectLighting::GetVirtualShadowMapSamplingBias();
		CommonParameters.HeightfieldShadowReceiverBias = Lumen::GetHeightfieldReceiverBias();
		CommonParameters.ForceShadowMaps = GLumenDirectLightingForceForceShadowMaps;

		bUseCloudTransmittance = SetupLightCloudTransmittanceParameters(
			GraphBuilder,
			Scene,
			View,
			GLumenDirectLightingCloudTransmittance != 0 && Light.bHasCloudTransmittance ? Light.LightSceneInfo : nullptr,
			CommonParameters.LightCloudTransmittanceParameters);

		CommonParameters.UseIESProfile = Light.IESTexture ? 1 : 0;
		CommonParameters.IESTexture = Light.IESTexture ? Light.IESTexture : GWhiteTexture->TextureRHI.GetReference();
		CommonParameters.IESTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	};

	const FMaterialRenderProxy* LightFunctionMaterialProxy = Light.LightSceneInfo->Proxy->GetLightFunctionMaterial();
	bool bUseLightFunction = true;

	if (!LightFunctionMaterialProxy
		|| !LightFunctionMaterialProxy->GetIncompleteMaterialWithFallback(Scene->GetFeatureLevel()).IsLightFunction()
		|| !View.Family->EngineShowFlags.LightFunctions)
	{
		bUseLightFunction = false;
		LightFunctionMaterialProxy = UMaterial::GetDefaultMaterial(MD_LightFunction)->GetRenderProxy();
	}

	const uint32 DispatchIndirectArgOffset = (Light.LightIndex * NumViews + ViewIndex) * sizeof(FRHIDispatchIndirectParameters);

	if (bUseLightFunction)
	{
		FLumenDirectLightingNonRayTracedShadowsWithLightFunctionCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenDirectLightingNonRayTracedShadowsWithLightFunctionCS::FParameters>();
		SetCommonParameters(PassParameters->Common);
		SetupLightFunctionParameters(View, Light.LightSceneInfo, 1.0f, PassParameters->LightFunctionParameters);

		FLumenDirectLightingNonRayTracedShadowsWithLightFunctionCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FLumenDirectLightingNonRayTracedShadowsWithLightFunctionCS::FThreadGroupSize32>(Lumen::UseThreadGroupSize32());
		PermutationVector.Set<FLumenDirectLightingNonRayTracedShadowsWithLightFunctionCS::FCompactShadowTraces>(ShadowTraceAllocatorUAV != nullptr);
		PermutationVector.Set<FLumenDirectLightingNonRayTracedShadowsWithLightFunctionCS::FLightType>(Light.Type);
		PermutationVector.Set<FLumenDirectLightingNonRayTracedShadowsWithLightFunctionCS::FVirtualShadowMap>(bUseVirtualShadowMap);
		PermutationVector.Set<FLumenDirectLightingNonRayTracedShadowsWithLightFunctionCS::FDynamicallyShadowed>(bUseDenseShadowMap);
		PermutationVector.Set<FLumenDirectLightingNonRayTracedShadowsWithLightFunctionCS::FDenseShadowMap>(bUseDenseShadowMap);
		PermutationVector.Set<FLumenDirectLightingNonRayTracedShadowsWithLightFunctionCS::FCloudTransmittance>(bUseCloudTransmittance);
		
		const FMaterial& Material = LightFunctionMaterialProxy->GetMaterialWithFallback(Scene->GetFeatureLevel(), LightFunctionMaterialProxy);
		const FMaterialShaderMap* MaterialShaderMap = Material.GetRenderingThreadShaderMap();
		TShaderRef<FLumenDirectLightingNonRayTracedShadowsWithLightFunctionCS> ComputeShader = MaterialShaderMap->GetShader<FLumenDirectLightingNonRayTracedShadowsWithLightFunctionCS>(PermutationVector);

		FRDGBufferRef IndirectArgsBuffer = LightTileScatterParameters.DispatchIndirectArgs;
		ClearUnusedGraphResources(ComputeShader, PassParameters, { IndirectArgsBuffer });

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("NonRayTracedShadowPass LF %s", *Light.Name),
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
		FLumenDirectLightingNonRayTracedShadowsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FLumenDirectLightingNonRayTracedShadowsCS::FParameters>();
		SetCommonParameters(PassParameters->Common);

		FLumenDirectLightingNonRayTracedShadowsCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FLumenDirectLightingNonRayTracedShadowsCS::FThreadGroupSize32>(Lumen::UseThreadGroupSize32());
		PermutationVector.Set<FLumenDirectLightingNonRayTracedShadowsCS::FCompactShadowTraces>(ShadowTraceAllocatorUAV != nullptr);
		PermutationVector.Set<FLumenDirectLightingNonRayTracedShadowsCS::FLightType>(Light.Type);
		PermutationVector.Set<FLumenDirectLightingNonRayTracedShadowsCS::FVirtualShadowMap>(bUseVirtualShadowMap);
		PermutationVector.Set<FLumenDirectLightingNonRayTracedShadowsCS::FDynamicallyShadowed>(bUseDenseShadowMap);
		PermutationVector.Set<FLumenDirectLightingNonRayTracedShadowsCS::FDenseShadowMap>(bUseDenseShadowMap);
		PermutationVector.Set<FLumenDirectLightingNonRayTracedShadowsCS::FCloudTransmittance>(bUseCloudTransmittance);
		TShaderRef<FLumenDirectLightingNonRayTracedShadowsCS> ComputeShader = View.ShaderMap->GetShader<FLumenDirectLightingNonRayTracedShadowsCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("NonRayTracedShadowPass %s", *Light.Name),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			LightTileScatterParameters.DispatchIndirectArgs,
			DispatchIndirectArgOffset);
	}
}

void TraceDistanceFieldShadows(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View,
	TRDGUniformBufferRef<FLumenCardScene> LumenCardSceneUniformBuffer,
	const FLumenGatheredLight& Light,
	const FLumenLightTileScatterParameters& LightTileScatterParameters,
	int32 ViewIndex,
	int32 NumViews,
	FRDGBufferUAVRef ShadowMaskTilesUAV,
	ERDGPassFlags ComputePassFlags)
{
	const FLumenSceneData& LumenSceneData = *Scene->GetLumenSceneData(View);
	check(Light.bHasShadows);

	FDistanceFieldObjectBufferParameters ObjectBufferParameters = DistanceField::SetupObjectBufferParameters(GraphBuilder, Scene->DistanceFieldSceneData);

	// Patch DF heightfields with Lumen heightfields
	ObjectBufferParameters.SceneHeightfieldObjectBounds = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(LumenSceneData.HeightfieldBuffer));
	ObjectBufferParameters.SceneHeightfieldObjectData = nullptr;
	ObjectBufferParameters.NumSceneHeightfieldObjects = LumenSceneData.Heightfields.Num();

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
	{
		PassParameters->IndirectArgBuffer = LightTileScatterParameters.DispatchIndirectArgs;
		PassParameters->RWShadowMaskTiles = ShadowMaskTilesUAV;

		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->LumenCardScene = LumenCardSceneUniformBuffer;
		PassParameters->LightTileScatterParameters = LightTileScatterParameters;
		PassParameters->LightIndex = Light.LightIndex;
		PassParameters->ViewIndex = ViewIndex;
		PassParameters->NumViews = NumViews;
		PassParameters->DummyZeroForFixingShaderCompilerBug = 0;
		Lumen::SetDirectLightingDeferredLightUniformBuffer(View, Light.LightSceneInfo, PassParameters->DeferredLightUniforms);

		PassParameters->ObjectBufferParameters = ObjectBufferParameters;
		PassParameters->LightTileIntersectionParameters = LightTileIntersectionParameters;

		FDistanceFieldAtlasParameters DistanceFieldAtlasParameters = DistanceField::SetupAtlasParameters(GraphBuilder, Scene->DistanceFieldSceneData);

		PassParameters->DistanceFieldAtlasParameters = DistanceFieldAtlasParameters;
		PassParameters->TranslatedWorldToShadow = FMatrix44f(FTranslationMatrix(-View.ViewMatrices.GetPreViewTranslation()) * WorldToMeshSDFShadowValue);
		extern float GDFShadowTwoSidedMeshDistanceBiasScale;
		PassParameters->TwoSidedMeshDistanceBiasScale = GDFShadowTwoSidedMeshDistanceBiasScale;

		PassParameters->TanLightSourceAngle = FMath::Tan(Light.LightSceneInfo->Proxy->GetLightSourceAngle());
		PassParameters->MaxTraceDistance = Lumen::GetMaxTraceDistance(View);
		PassParameters->StepFactor = FMath::Clamp(GOffscreenShadowingTraceStepFactor, .1f, 10.0f);
		PassParameters->MeshSDFShadowRayBias = LumenSceneDirectLighting::GetMeshSDFShadowRayBias();
		PassParameters->HeightfieldShadowRayBias = LumenSceneDirectLighting::GetHeightfieldShadowRayBias();
		PassParameters->GlobalSDFShadowRayBias = LumenSceneDirectLighting::GetGlobalSDFShadowRayBias();
		PassParameters->HeightfieldMaxTracingSteps = Lumen::GetHeightfieldMaxTracingSteps();
	}

	FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FThreadGroupSize32>(Lumen::UseThreadGroupSize32());
	PermutationVector.Set<FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FLightType>(Light.Type);
	PermutationVector.Set<FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FTraceGlobalSDF>(Lumen::UseGlobalSDFTracing(*View.Family));
	PermutationVector.Set<FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FTraceMeshSDFs>(bTraceMeshSDFs);
	PermutationVector.Set<FLumenSceneDirectLightingTraceDistanceFieldShadowsCS::FTraceHeightfields>(bTraceHeighfieldObjects);
	extern int32 GDistanceFieldOffsetDataStructure;
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
	uint32 bHasShadowMask;
	uint32 Padding[2];
};

FRDGBufferRef CreateLumenLightDataBuffer(FRDGBuilder& GraphBuilder, const TArray<FLumenGatheredLight, TInlineAllocator<64>>& GatheredLights, float Exposure)
{
	TArray<FLumenPackedLight, TInlineAllocator<16>> PackedLightData;
	PackedLightData.SetNum(FMath::RoundUpToPowerOfTwo(FMath::Max(GatheredLights.Num(), 16)));

	for (int32 LightIndex = 0; LightIndex < GatheredLights.Num(); ++LightIndex)
	{
		const FLumenGatheredLight& LumenLight = GatheredLights[LightIndex];
		const FLightSceneInfo* LightSceneInfo = LumenLight.LightSceneInfo;
		const FSphere LightBounds = LightSceneInfo->Proxy->GetBoundingSphere();

		FLightRenderParameters ShaderParameters;
		LightSceneInfo->Proxy->GetLightShaderParameters(ShaderParameters);

		if (LightSceneInfo->Proxy->IsInverseSquared())
		{
			ShaderParameters.FalloffExponent = 0;
		}
		ShaderParameters.Color *= LightSceneInfo->Proxy->GetIndirectLightingScale();

		ShaderParameters.Color *= ShaderParameters.GetLightExposureScale(Exposure);

		FLumenPackedLight& LightData = PackedLightData[LightIndex];
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

		LightData.LightingChannelMask = LightSceneInfo->Proxy->GetLightingChannelMask();
		LightData.bHasShadowMask = LumenLight.NeedsShadowMask() ? 1 : 0;
	}

	FRDGBufferRef LightDataBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("Lumen.DirectLighting.Lights"), PackedLightData);
	return LightDataBuffer;
}

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
void CullDirectLightingTiles(
	const TArray<FViewInfo>& Views,
	FRDGBuilder& GraphBuilder,
	const FLumenCardUpdateContext& CardUpdateContext,
	TRDGUniformBufferRef<FLumenCardScene> LumenCardSceneUniformBuffer,
	const TArray<FLumenGatheredLight, TInlineAllocator<64>>& GatheredLights,
	FRDGBufferRef LumenPackedLights,
	FLightTileCullContext& CullContext,
	FLumenCardTileUpdateContext& CardTileUpdateCotnext,
	ERDGPassFlags ComputePassFlags)
{
	RDG_EVENT_SCOPE(GraphBuilder, "CullTiles %d lights", GatheredLights.Num());
	const FGlobalShaderMap* GlobalShaderMap = Views[0].ShaderMap;

	const uint32 MaxLightTiles = CardUpdateContext.MaxUpdateTiles;;
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

void FDeferredShadingSceneRenderer::RenderDirectLightingForLumenScene(
	FRDGBuilder& GraphBuilder,
	const FLumenCardTracingInputs& TracingInputs,
	const FLumenCardUpdateContext& CardUpdateContext,
	ERDGPassFlags ComputePassFlags)
{
	LLM_SCOPE_BYTAG(Lumen);

	if (GLumenDirectLighting)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "DirectLighting");
		QUICK_SCOPE_CYCLE_COUNTER(RenderDirectLightingForLumenScene);

		const FViewInfo& MainView = Views[0];
		FLumenSceneData& LumenSceneData = *Scene->GetLumenSceneData(Views[0]);

		TRDGUniformBufferRef<FLumenCardScene> LumenCardSceneUniformBuffer = TracingInputs.LumenCardSceneUniformBuffer;

		TArray<FLumenGatheredLight, TInlineAllocator<64>> GatheredLights;
		bool bHasRectLights = false;

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
						const FLumenGatheredLight GatheredLight(Scene, View, LightSceneInfo, /*LightIndex*/ GatheredLights.Num());
						bHasRectLights = bHasRectLights || GatheredLight.Type == ELumenLightType::Rect;
						GatheredLights.Add(GatheredLight);
						break;
					}
				}
			}
		}

		FRDGBufferRef LumenPackedLights = CreateLumenLightDataBuffer(GraphBuilder, GatheredLights, MainView.GetLastEyeAdaptationExposure()); // TODO View

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

		// Apply shadow map
		{
			RDG_EVENT_SCOPE(GraphBuilder, "Non raytraced shadows");

			FRDGBufferUAVRef ShadowMaskTilesUAV = GraphBuilder.CreateUAV(ShadowMaskTiles, ERDGUnorderedAccessViewFlags::SkipBarrier);
			FRDGBufferUAVRef ShadowTraceAllocatorUAV = ShadowTraceAllocator ? GraphBuilder.CreateUAV(ShadowTraceAllocator, ERDGUnorderedAccessViewFlags::SkipBarrier) : nullptr;
			FRDGBufferUAVRef ShadowTracesUAV = ShadowTraces ? GraphBuilder.CreateUAV(ShadowTraces, ERDGUnorderedAccessViewFlags::SkipBarrier) : nullptr;

			int32 NumShadowedLights = 0;
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				const FViewInfo& View = Views[ViewIndex];

				for (int32 LightIndex = 0; LightIndex < GatheredLights.Num(); ++LightIndex)
				{
					const FLumenGatheredLight& GatheredLight = GatheredLights[LightIndex];
					if (GatheredLight.NeedsShadowMask())
					{
						ComputeNonRayTracedShadows(
							GraphBuilder,
							Scene,
							View,
							LumenCardSceneUniformBuffer,
							VisibleLightInfos,
							VirtualShadowMapArray,
							GatheredLight,
							CullContext.LightTileScatterParameters,
							ViewIndex,
							Views.Num(),
							ShadowMaskTilesUAV,
							ShadowTraceAllocatorUAV,
							ShadowTracesUAV,
							ComputePassFlags);

						++NumShadowedLights;
					}
				}
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
			RDG_EVENT_SCOPE(GraphBuilder, "Offscreen shadows");

			FRDGBufferUAVRef ShadowMaskTilesUAV = GraphBuilder.CreateUAV(ShadowMaskTiles, ERDGUnorderedAccessViewFlags::SkipBarrier);

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				const FViewInfo& View = Views[ViewIndex];

				if (Lumen::UseHardwareRayTracedDirectLighting(ViewFamily))
				{
					TraceLumenHardwareRayTracedDirectLightingShadows(
						GraphBuilder,
						Scene,
						View,
						ViewIndex,
						TracingInputs,
						ShadowTraceIndirectArgs,
						ShadowTraceAllocator,
						ShadowTraces,
						CullContext.LightTileAllocator,
						CullContext.LightTiles,
						LumenPackedLights,
						ShadowMaskTilesUAV);
				}
				else
				{
					for (int32 LightIndex = 0; LightIndex < GatheredLights.Num(); ++LightIndex)
					{
						const FLumenGatheredLight& GatheredLight = GatheredLights[LightIndex];
						if (GatheredLight.bHasShadows)
						{
							TraceDistanceFieldShadows(
								GraphBuilder,
								Scene,
								View,
								LumenCardSceneUniformBuffer,
								GatheredLight,
								CullContext.LightTileScatterParameters,
								ViewIndex,
								Views.Num(),
								ShadowMaskTilesUAV,
								ComputePassFlags);
						}
					}
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
			FRDGTextureUAVRef DirectLightingAtlasUAV = GraphBuilder.CreateUAV(TracingInputs.DirectLightingAtlas);

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
			TracingInputs,
			CardUpdateContext,
			CardTileUpdateContext,
			ComputePassFlags);
	}
}
