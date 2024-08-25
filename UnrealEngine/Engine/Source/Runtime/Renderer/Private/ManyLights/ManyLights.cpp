// Copyright Epic Games, Inc. All Rights Reserved.

#include "ManyLights.h"
#include "ManyLightsInternal.h"
#include "RendererPrivate.h"
#include "PixelShaderUtils.h"
#include "BasePassRendering.h"

static TAutoConsoleVariable<int32> CVarManyLights(
	TEXT("r.ManyLights"),
	0,
	TEXT("Whether to enable Many Lights. Experimental feature leveraging ray tracing to stochastically importance sample lights.\n")
	TEXT("1 - all lights using ray tracing shadows will be stochastically sampled\n")
	TEXT("2 - all lights will be stochastically sampled"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarManyLightsNumSamplesPerPixel(
	TEXT("r.ManyLights.NumSamplesPerPixel"),
	4,
	TEXT("Number of samples (shadow rays) per half-res pixel.\n")
	TEXT("1 - 0.25 trace per pixel\n")
	TEXT("2 - 0.5 trace per pixel\n")
	TEXT("4 - 1 trace per pixel"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarManyLightsMaxShadingTilesPerGridCell(
	TEXT("r.ManyLights.MaxShadingTilesPerGridCell"),
	32,
	TEXT("Maximum number of shading tiles per grid cell."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarManyLightsSamplingMinWeight(
	TEXT("r.ManyLights.Sampling.MinWeight"),
	0.001f,
	TEXT("Determines minimal sample influence on final pixels. Used to skip samples which would have minimal impact to the final image even if light is fully visible."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarManyLightsTemporal(
	TEXT("r.ManyLights.Temporal"),
	1,
	TEXT("Whether to use temporal accumulation for shadow mask."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarManyLightsTemporalMaxFramesAccumulated(
	TEXT("r.ManyLights.Temporal.MaxFramesAccumulated"),
	8,
	TEXT("Max history length when accumulating frames. Lower values have less ghosting, but more noise."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarManyLightsTemporalNeighborhoodClampScale(
	TEXT("r.ManyLights.Temporal.NeighborhoodClampScale"),
	2.0f,
	TEXT("Scales how permissive is neighborhood clamp. Higher values cause more ghosting, but allow smoother temporal accumulation."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarManyLightsSpatial(
	TEXT("r.ManyLights.Spatial"),
	1,
	TEXT("Whether denoiser should run spatial filter."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarManyLightsSpatialDepthWeightScale(
	TEXT("r.ManyLights.Spatial.DepthWeightScale"),
	10000.0f,
	TEXT("Scales the depth weight of the spatial filter. Smaller values allow for more sample reuse, but also introduce more bluriness between unrelated surfaces."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarManyLightsWaveOps(
	TEXT("r.ManyLights.WaveOps"),
	1,
	TEXT("Whether to use wave ops. Useful for debugging."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarManyLightsDebug(
	TEXT("r.ManyLights.Debug"),
	0,
	TEXT("Whether to enabled debug mode, which prints various extra debug information from shaders.")
	TEXT("0 - Disable\n")
	TEXT("1 - Visualize sampling\n")
	TEXT("2 - Visualize tracing\n"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarManyLightsDebugLightId(
	TEXT("r.ManyLights.Debug.LightId"),
	-1,
	TEXT("Which light to show debug info for. When set to -1, uses the currently selected light in editor."),
	ECVF_RenderThreadSafe
);

int32 GManyLightsReset = 0;
FAutoConsoleVariableRef CVarManyLightsReset(
	TEXT("r.ManyLights.Reset"),
	GManyLightsReset,
	TEXT("Reset history for debugging."),
	ECVF_RenderThreadSafe
);

int32 GManyLightsResetEveryNthFrame = 0;
	FAutoConsoleVariableRef CVarManyLightsResetEveryNthFrame(
	TEXT("r.ManyLights.ResetEveryNthFrame"),
		GManyLightsResetEveryNthFrame,
	TEXT("Reset history every Nth frame for debugging."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int> CVarManyLightsFixedStateFrameIndex(
	TEXT("r.ManyLights.FixedStateFrameIndex"),
	-1,
	TEXT("Whether to override View.StateFrameIndex for debugging."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int> CVarManyLightsTexturedRectLights(
	TEXT("r.ManyLights.TexturedRectLights"),
	0,
	TEXT("Whether to support textured rect lights."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int> CVarManyLightsLightFunctions(
	TEXT("r.ManyLights.LightFunctions"),
	0,
	TEXT("Whether to support light functions."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int> CVarManyLightsIESProfiles(
	TEXT("r.ManyLights.IESProfiles"),
	1,
	TEXT("Whether to support IES profiles on lights."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

namespace ManyLights
{
	// must match values in ManyLights.ush
	constexpr int32 TileSize = 8;
	constexpr int32 MaxLocalLightIndexXY = 16; // 16 * 16 = 256
	constexpr uint32 ShadingTileIndexUnshadowed = 0xFFFFF; // limited by PackShadingTile()
	constexpr uint32 ShadingAtlasSizeInTiles = 512;

	bool IsEnabled()
	{
		return CVarManyLights.GetValueOnRenderThread() != 0;
	}

	bool IsUsingLightFunctions()
	{
		return IsEnabled() && CVarManyLightsLightFunctions.GetValueOnRenderThread() != 0;
	}

	bool IsLightSupported(uint8 LightType, ECastRayTracedShadow::Type CastRayTracedShadow)
	{
		if (ManyLights::IsEnabled() && LightType != LightType_Directional)
		{
			const bool bRayTracedShadows = (CastRayTracedShadow == ECastRayTracedShadow::Enabled || (ShouldRenderRayTracingShadows() && CastRayTracedShadow == ECastRayTracedShadow::UseProjectSetting));
			return CVarManyLights.GetValueOnRenderThread() == 2 || bRayTracedShadows;
		}

		return false;
	}

	bool ShouldCompileShaders(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (IsMobilePlatform(Parameters.Platform))
		{
			return false;
		}

		// SM6 because it uses typed loads to accumulate lights
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6) && RHISupportsWaveOperations(Parameters.Platform);
	}

	uint32 GetStateFrameIndex(FSceneViewState* ViewState)
	{
		uint32 StateFrameIndex = ViewState ? ViewState->GetFrameIndex() : 0;

		if (CVarManyLightsFixedStateFrameIndex.GetValueOnRenderThread() >= 0)
		{
			StateFrameIndex = CVarManyLightsFixedStateFrameIndex.GetValueOnRenderThread();
		}

		return StateFrameIndex;
	}

	FIntPoint GetNumSamplesPerPixel2d()
	{
		const uint32 NumSamplesPerPixel1d = FMath::RoundUpToPowerOfTwo(FMath::Clamp(CVarManyLightsNumSamplesPerPixel.GetValueOnRenderThread(), 1, 4));
		return NumSamplesPerPixel1d == 4 ? FIntPoint(2, 2) : (NumSamplesPerPixel1d == 2 ? FIntPoint(2, 1) : FIntPoint(1, 1));
	}

	int32 GetDebugMode()
	{
		return CVarManyLightsDebug.GetValueOnRenderThread();
	}

	bool UseWaveOps(EShaderPlatform ShaderPlatform)
	{
		return CVarManyLightsWaveOps.GetValueOnRenderThread() != 0
			&& GRHISupportsWaveOperations
			&& RHISupportsWaveOperations(ShaderPlatform);
	}

	void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FForwardLightingParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
		ShaderPrint::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	// Keep in sync with TILE_TYPE_* in shaders
	enum class ETileType : uint8
	{
		SimpleShading = 0,
		ComplexShading = 1,
		SHADING_MAX = 2,

		Empty = 2,
		MAX = 3
	};
};

class FTileClassificationCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTileClassificationCS)
	SHADER_USE_PARAMETER_STRUCT(FTileClassificationCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FManyLightsParameters, ManyLightsParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWTileData)
	END_SHADER_PARAMETER_STRUCT()

	class FDownsampledClassification : SHADER_PERMUTATION_BOOL("DOWNSAMPLED_CLASSIFICATION");
	using FPermutationDomain = TShaderPermutationDomain<FDownsampledClassification>;

	static int32 GetGroupSize()
	{	
		return 8;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ManyLights::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FTileClassificationCS, "/Engine/Private/ManyLights/ManyLights.usf", "TileClassificationCS", SF_Compute);

class FInitTileIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInitTileIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FInitTileIndirectArgsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FManyLightsParameters, ManyLightsParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWTileIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWDownsampledTileIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, DownsampledTileAllocator)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ManyLights::ShouldCompileShaders(Parameters);
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

IMPLEMENT_GLOBAL_SHADER(FInitTileIndirectArgsCS, "/Engine/Private/ManyLights/ManyLights.usf", "InitTileIndirectArgsCS", SF_Compute);

class FInitCompositeIndirectArgsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInitCompositeIndirectArgsCS)
	SHADER_USE_PARAMETER_STRUCT(FInitCompositeIndirectArgsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FManyLightsParameters, ManyLightsParameters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CompositeTileAllocator)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ManyLights::ShouldCompileShaders(Parameters);
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

IMPLEMENT_GLOBAL_SHADER(FInitCompositeIndirectArgsCS, "/Engine/Private/ManyLights/ManyLights.usf", "InitCompositeIndirectArgsCS", SF_Compute);

class FGenerateLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGenerateLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FGenerateLightSamplesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_STRUCT_INCLUDE(FManyLightsParameters, ManyLightsParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float2>, RWSampleLuminanceSum)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWDownsampledSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<UNORM float3>, RWDownsampledSceneWorldNormal)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWCompositeTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint2>, RWCompositeTileData)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSamples)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, DownsampledTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, DownsampledTileData)
		SHADER_PARAMETER(FVector4f, HistoryUVMinMax)
		SHADER_PARAMETER(FVector4f, HistoryScreenPositionScaleBias)
	END_SHADER_PARAMETER_STRUCT()

	class FTileType : SHADER_PERMUTATION_INT("TILE_TYPE", (int32)ManyLights::ETileType::SHADING_MAX);
	class FIESProfile : SHADER_PERMUTATION_BOOL("USE_IES_PROFILE");
	class FLightFunctionAtlas : SHADER_PERMUTATION_BOOL("USE_LIGHT_FUNCTION_ATLAS");
	class FTexturedRectLights : SHADER_PERMUTATION_BOOL("USE_SOURCE_TEXTURE");
	class FNumSamplesPerPixel1d : SHADER_PERMUTATION_SPARSE_INT("NUM_SAMPLES_PER_PIXEL_1D", 1, 2, 4);

	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FTileType, FIESProfile, FLightFunctionAtlas, FTexturedRectLights, FNumSamplesPerPixel1d, FDebugMode>;

	static int32 GetGroupSize()
	{	
		return 8;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ManyLights::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		ManyLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
		OutEnvironment.CompilerFlags.Add(CFLAG_WaveOperations);
	}
};

IMPLEMENT_GLOBAL_SHADER(FGenerateLightSamplesCS, "/Engine/Private/ManyLights/ManyLightsSampling.usf", "GenerateLightSamplesCS", SF_Compute);

class FClearLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClearLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FClearLightSamplesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_STRUCT_INCLUDE(FManyLightsParameters, ManyLightsParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWDownsampledSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<UNORM float3>, RWDownsampledSceneWorldNormal)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWLightSamples)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, DownsampledTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, DownsampledTileData)
	END_SHADER_PARAMETER_STRUCT()

	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FDebugMode>;

	static int32 GetGroupSize()
	{
		return 8;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ManyLights::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		ManyLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FClearLightSamplesCS, "/Engine/Private/ManyLights/ManyLightsSampling.usf", "ClearLightSamplesCS", SF_Compute);

class FInitCompositeUpsampleWeightsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInitCompositeUpsampleWeightsCS)
	SHADER_USE_PARAMETER_STRUCT(FInitCompositeUpsampleWeightsCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FManyLightsParameters, ManyLightsParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWCompositeUpsampleWeights)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 8;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ManyLights::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FInitCompositeUpsampleWeightsCS, "/Engine/Private/ManyLights/ManyLights.usf", "InitCompositeUpsampleWeightsCS", SF_Compute);

class FResolveLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FResolveLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FResolveLightSamplesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FManyLightsParameters, ManyLightsParameters)
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWShadingTileAllocator)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWShadingTileGridAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, RWShadingTileGrid)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWShadingTileAtlas)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint2>, CompositeTileData)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<flaot4>, CompositeUpsampleWeights)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, LightSamples)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 8;
	}

	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ManyLights::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		ManyLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FResolveLightSamplesCS, "/Engine/Private/ManyLights/ManyLightsResolve.usf", "ResolveLightSamplesCS", SF_Compute);

class FShadeLightSamplesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FShadeLightSamplesCS)
	SHADER_USE_PARAMETER_STRUCT(FShadeLightSamplesCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_STRUCT_INCLUDE(FManyLightsParameters, ManyLightsParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWResolvedDiffuseLighting)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWResolvedSpecularLighting)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, TileData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ShadingTileAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, CompositeTileAllocator)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadingTileGridAllocator)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ShadingTileGrid)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, ShadingTiles)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, ShadingTileAtlas)
	END_SHADER_PARAMETER_STRUCT()

	static int32 GetGroupSize()
	{
		return 8;
	}

	class FTileType : SHADER_PERMUTATION_INT("TILE_TYPE", (int32)ManyLights::ETileType::SHADING_MAX);
	class FIESProfile : SHADER_PERMUTATION_BOOL("USE_IES_PROFILE");
	class FLightFunctionAtlas : SHADER_PERMUTATION_BOOL("USE_LIGHT_FUNCTION_ATLAS");
	class FTexturedRectLights : SHADER_PERMUTATION_BOOL("USE_SOURCE_TEXTURE");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FTileType, FIESProfile, FLightFunctionAtlas, FTexturedRectLights, FDebugMode>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ManyLights::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		ManyLights::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FShadeLightSamplesCS, "/Engine/Private/ManyLights/ManyLightsShading.usf", "ShadeLightSamplesCS", SF_Compute);

class FDenoiserTemporalCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDenoiserTemporalCS)
	SHADER_USE_PARAMETER_STRUCT(FDenoiserTemporalCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FManyLightsParameters, ManyLightsParameters)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, CompositeUpsampleWeights)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float2>, SampleLuminanceSumTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, ResolvedDiffuseLighting)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, ResolvedSpecularLighting)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, DiffuseLightingAndSecondMomentHistoryTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, SpecularLightingAndSecondMomentHistoryTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UNORM float>, NumFramesAccumulatedHistoryTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, ManyLightsDepthHistory)
		SHADER_PARAMETER(FVector4f, HistoryUVMinMax)
		SHADER_PARAMETER(FVector4f, HistoryScreenPositionScaleBias)
		SHADER_PARAMETER(float, PrevSceneColorPreExposureCorrection)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWDiffuseLightingAndSecondMoment)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWSpecularLightingAndSecondMoment)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<UNORM float>, RWNumFramesAccumulated)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, RWSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWSceneColor)
	END_SHADER_PARAMETER_STRUCT()

	class FValidHistory : SHADER_PERMUTATION_BOOL("VALID_HISTORY");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FValidHistory, FDebugMode>;

	static int32 GetGroupSize()
	{
		return 8;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ManyLights::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FDenoiserTemporalCS, "/Engine/Private/ManyLights/ManyLightsDenoiserTemporal.usf", "DenoiserTemporalCS", SF_Compute);

class FDenoiserSpatialCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDenoiserSpatialCS)
	SHADER_USE_PARAMETER_STRUCT(FDenoiserSpatialCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FManyLightsParameters, ManyLightsParameters)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWSceneColor)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float3>, DiffuseLightingAndSecondMomentTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float3>, SpecularLightingAndSecondMomentTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UNORM float>, NumFramesAccumulatedTexture)
		SHADER_PARAMETER(float, SpatialFilterDepthWeightScale)
	END_SHADER_PARAMETER_STRUCT()

	class FSpatialFilter : SHADER_PERMUTATION_BOOL("SPATIAL_FILTER");
	class FDebugMode : SHADER_PERMUTATION_BOOL("DEBUG_MODE");
	using FPermutationDomain = TShaderPermutationDomain<FSpatialFilter, FDebugMode>;

	static int32 GetGroupSize()
	{
		return 8;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ManyLights::ShouldCompileShaders(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FDenoiserSpatialCS, "/Engine/Private/ManyLights/ManyLightsDenoiserSpatial.usf", "DenoiserSpatialCS", SF_Compute);

/**
 * Single pass batched light rendering using ray tracing (distance field or triangle) for shadowing.
 */
void FDeferredShadingSceneRenderer::RenderManyLights(FRDGBuilder& GraphBuilder, const FSceneTextures& SceneTextures)
{
	if (!ManyLights::IsEnabled())
	{
		return;
	}

	check(AreLightsInLightGrid());

	RDG_EVENT_SCOPE(GraphBuilder, "ManyLights");

	const uint32 ViewIndex = 0;
	const FViewInfo& View = Views[ViewIndex];
	FBlueNoise BlueNoise = GetBlueNoiseGlobalParameters();
	TUniformBufferRef<FBlueNoise> BlueNoiseUniformBuffer = CreateUniformBufferImmediate(BlueNoise, EUniformBufferUsage::UniformBuffer_SingleDraw);

	const bool bDebug = ManyLights::GetDebugMode() != 0;
	const bool bWaveOps = ManyLights::UseWaveOps(View.GetShaderPlatform())
		&& GRHIMinimumWaveSize <= 32
		&& GRHIMaximumWaveSize >= 32;

	// History reset for debugging purposes
	bool bResetHistory = false;

	if (GManyLightsResetEveryNthFrame > 0 && (ViewFamily.FrameNumber % (uint32)GManyLightsResetEveryNthFrame) == 0)
	{
		bResetHistory = true;
	}

	if (GManyLightsReset != 0)
	{
		GManyLightsReset = 0;
		bResetHistory = true;
	}

	const FIntPoint NumSamplesPerPixel2d = ManyLights::GetNumSamplesPerPixel2d();

	const uint32 DownsampleFactor = 2;
	const FIntPoint DownsampledViewSize = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), DownsampleFactor);
	const FIntPoint SampleViewSize = DownsampledViewSize * NumSamplesPerPixel2d;
	const FIntPoint DownsampledBufferSize = FIntPoint::DivideAndRoundUp(SceneTextures.Config.Extent, DownsampleFactor);
	const FIntPoint SampleBufferSize = DownsampledBufferSize * NumSamplesPerPixel2d;
	const FIntPoint DonwnsampledSampleBufferSize = DownsampledBufferSize * NumSamplesPerPixel2d;

	// #ml_todo: make atlas size based on the resolution
	const FIntPoint ShadingTileGridSize = FIntPoint::DivideAndRoundUp(SceneTextures.Config.Extent, ManyLights::TileSize);
	const FIntPoint ShadingTileAtlasSize = ManyLights::TileSize * ManyLights::ShadingAtlasSizeInTiles;
	const int32 MaxShadingTiles = ManyLights::ShadingAtlasSizeInTiles * ManyLights::ShadingAtlasSizeInTiles;
	check(MaxShadingTiles == (ShadingTileAtlasSize.X * ShadingTileAtlasSize.Y) / (ManyLights::TileSize * ManyLights::TileSize));
	check(MaxShadingTiles < ManyLights::ShadingTileIndexUnshadowed);
	const int32 MaxCompositeTiles = MaxShadingTiles;

	const int32 MaxShadingTilesPerGridCell = FMath::Max(CVarManyLightsMaxShadingTilesPerGridCell.GetValueOnRenderThread(), 0);

	FRDGTextureRef DownsampledSceneDepth = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(DownsampledBufferSize, PF_R32_FLOAT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("ManyLights.DownsampledSceneDepth"));

	FRDGTextureRef DownsampledSceneWorldNormal = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(DownsampledBufferSize, PF_A2B10G10R10, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("ManyLights.DownsampledSceneWorldNormal"));

	FRDGTextureRef LightSamples = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(DonwnsampledSampleBufferSize, PF_R32_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("ManyLights.LightSamples"));

	FRDGTextureRef LightSampleRayDistance = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(DonwnsampledSampleBufferSize, PF_R16F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("ManyLights.LightSampleRayDistance"));
	
	bool bTemporal = CVarManyLightsTemporal.GetValueOnRenderThread() != 0;
	FVector4f HistoryScreenPositionScaleBias = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
	FVector4f HistoryUVMinMax = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
	FRDGTextureRef DiffuseLightingAndSecondMomentHistory = nullptr;
	FRDGTextureRef SpecularLightingAndSecondMomentHistory = nullptr;
	FRDGTextureRef SceneDepthHistory = nullptr;
	FRDGTextureRef NumFramesAccumulatedHistory = nullptr;

	if (View.ViewState)
	{
		const FManyLightsViewState& LightingViewState = View.ViewState->ManyLights;

		if (!View.bCameraCut && !bResetHistory && bTemporal)
		{
			HistoryScreenPositionScaleBias = LightingViewState.HistoryScreenPositionScaleBias;
			HistoryUVMinMax = LightingViewState.HistoryUVMinMax;

			if (LightingViewState.DiffuseLightingAndSecondMomentHistory
				&& LightingViewState.SpecularLightingAndSecondMomentHistory
				&& LightingViewState.SceneDepthHistory
				&& LightingViewState.NumFramesAccumulatedHistory
				&& LightingViewState.DiffuseLightingAndSecondMomentHistory->GetDesc().Extent == View.GetSceneTexturesConfig().Extent
				&& LightingViewState.SpecularLightingAndSecondMomentHistory->GetDesc().Extent == View.GetSceneTexturesConfig().Extent
				&& LightingViewState.SceneDepthHistory->GetDesc().Extent == SceneTextures.Depth.Resolve->Desc.Extent)
			{
				DiffuseLightingAndSecondMomentHistory = GraphBuilder.RegisterExternalTexture(LightingViewState.DiffuseLightingAndSecondMomentHistory);
				SpecularLightingAndSecondMomentHistory = GraphBuilder.RegisterExternalTexture(LightingViewState.SpecularLightingAndSecondMomentHistory);
				SceneDepthHistory = GraphBuilder.RegisterExternalTexture(LightingViewState.SceneDepthHistory);
				NumFramesAccumulatedHistory = GraphBuilder.RegisterExternalTexture(LightingViewState.NumFramesAccumulatedHistory);
			}
		}
	}

	// Setup the light function atlas
	const bool bUseLightFunctionAtlas = LightFunctionAtlas::IsEnabled(View, LightFunctionAtlas::ELightFunctionAtlasSystem::ManyLights);

	const FIntPoint ViewSizeInTiles = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), ManyLights::TileSize);
	const int32 TileDataStride = ViewSizeInTiles.X * ViewSizeInTiles.Y;

	const FIntPoint DownsampledViewSizeInTiles = FIntPoint::DivideAndRoundUp(DownsampledViewSize, ManyLights::TileSize);
	const int32 DownsampledTileDataStride = DownsampledViewSizeInTiles.X * DownsampledViewSizeInTiles.Y;

	FRDGTextureRef DownsampledTileMask = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(FMath::DivideAndRoundUp<FIntPoint>(DownsampledBufferSize, ManyLights::TileSize), PF_R8_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("ManyLights.DownsampledTileMask"));

	FManyLightsParameters ManyLightsParameters;
	{
		ManyLightsParameters.ViewUniformBuffer = View.ViewUniformBuffer;
		ManyLightsParameters.Scene = View.GetSceneUniforms().GetBuffer(GraphBuilder);
		ManyLightsParameters.SceneTextures = GetSceneTextureParameters(GraphBuilder, SceneTextures.UniformBuffer);
		ManyLightsParameters.SceneTexturesStruct = SceneTextures.UniformBuffer;
		ManyLightsParameters.Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
		ManyLightsParameters.ForwardLightData = View.ForwardLightingResources.ForwardLightUniformBuffer;
		ManyLightsParameters.LightFunctionAtlas = LightFunctionAtlas::BindGlobalParameters(GraphBuilder, View);
		ManyLightsParameters.BlueNoise = BlueNoiseUniformBuffer;
		ManyLightsParameters.PreIntegratedGF = GSystemTextures.PreintegratedGF->GetRHI();
		ManyLightsParameters.PreIntegratedGFSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		ManyLightsParameters.DownsampledViewSize = DownsampledViewSize;
		ManyLightsParameters.SampleViewSize = SampleViewSize;
		ManyLightsParameters.NumSamplesPerPixel = NumSamplesPerPixel2d;
		ManyLightsParameters.NumSamplesPerPixelDivideShift.X = FMath::FloorLog2(NumSamplesPerPixel2d.X);
		ManyLightsParameters.NumSamplesPerPixelDivideShift.Y = FMath::FloorLog2(NumSamplesPerPixel2d.Y);
		ManyLightsParameters.ManyLightsStateFrameIndex = ManyLights::GetStateFrameIndex(View.ViewState);
		ManyLightsParameters.DownsampledTileMask = DownsampledTileMask;
		ManyLightsParameters.DownsampledSceneDepth = DownsampledSceneDepth;
		ManyLightsParameters.DownsampledSceneWorldNormal = DownsampledSceneWorldNormal;
		ManyLightsParameters.MaxCompositeTiles = MaxCompositeTiles;
		ManyLightsParameters.MaxShadingTiles = MaxShadingTiles;
		ManyLightsParameters.MaxShadingTilesPerGridCell = MaxShadingTilesPerGridCell;
		ManyLightsParameters.ShadingTileGridSize = ShadingTileGridSize;
		ManyLightsParameters.DownsampledBufferInvSize = FVector2f(1.0f) / DownsampledBufferSize;
		ManyLightsParameters.SamplingMinWeight = FMath::Max(CVarManyLightsSamplingMinWeight.GetValueOnRenderThread(), 0.0f);
		ManyLightsParameters.TileDataStride = TileDataStride;
		ManyLightsParameters.DownsampledTileDataStride = DownsampledTileDataStride;
		ManyLightsParameters.TemporalMaxFramesAccumulated = FMath::Max(CVarManyLightsTemporalMaxFramesAccumulated.GetValueOnRenderThread(), 0.0f);
		ManyLightsParameters.TemporalNeighborhoodClampScale = CVarManyLightsTemporalNeighborhoodClampScale.GetValueOnRenderThread();
		ManyLightsParameters.TemporalAdvanceFrame = View.ViewState && !View.bStatePrevViewInfoIsReadOnly ? 1 : 0;
		ManyLightsParameters.DebugMode = ManyLights::GetDebugMode();
		ManyLightsParameters.DebugLightId = INDEX_NONE;

		if (bDebug)
		{
			ShaderPrint::SetEnabled(true);
			ShaderPrint::RequestSpaceForLines(1024);
			ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, ManyLightsParameters.ShaderPrintUniformBuffer);

			ManyLightsParameters.DebugLightId = CVarManyLightsDebugLightId.GetValueOnRenderThread();

			if (ManyLightsParameters.DebugLightId < 0)
			{
				for (auto LightIt = Scene->Lights.CreateConstIterator(); LightIt; ++LightIt)
				{
					const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
					const FLightSceneInfo* const LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

					if (LightSceneInfo->Proxy->IsSelected())
					{
						ManyLightsParameters.DebugLightId = LightSceneInfo->Id;
						break;
					}
				}
			}
		}
	}

	FRDGBufferRef TileAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), (int32)ManyLights::ETileType::MAX), TEXT("ManyLights.TileAllocator"));
	FRDGBufferRef TileData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), TileDataStride * (int32)ManyLights::ETileType::MAX), TEXT("ManyLights.TileData"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(TileAllocator), 0);

	FRDGBufferRef DownsampledTileAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), (int32)ManyLights::ETileType::MAX), TEXT("ManyLights.DownsampledTileAllocator"));
	FRDGBufferRef DownsampledTileData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), DownsampledTileDataStride * (int32)ManyLights::ETileType::MAX), TEXT("ManyLights.DownsampledTileData"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(DownsampledTileAllocator), 0);

	// #ml_todo: merge classification passes or reuse downsampled one to create full res tiles
	// Run tile classification to generate tiles for the subsequent passes
	{
		{
			FTileClassificationCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTileClassificationCS::FParameters>();
			PassParameters->ManyLightsParameters = ManyLightsParameters;
			PassParameters->RWTileAllocator = GraphBuilder.CreateUAV(TileAllocator);
			PassParameters->RWTileData = GraphBuilder.CreateUAV(TileData);

			FTileClassificationCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FTileClassificationCS::FDownsampledClassification>(false);
			auto ComputeShader = View.ShaderMap->GetShader<FTileClassificationCS>(PermutationVector);

			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FTileClassificationCS::GetGroupSize());

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("TileClassification %dx%d", View.ViewRect.Size().X, View.ViewRect.Size().Y),
				ComputeShader,
				PassParameters,
				GroupCount);
		}

		{
			FTileClassificationCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTileClassificationCS::FParameters>();
			PassParameters->ManyLightsParameters = ManyLightsParameters;
			PassParameters->RWTileAllocator = GraphBuilder.CreateUAV(DownsampledTileAllocator);
			PassParameters->RWTileData = GraphBuilder.CreateUAV(DownsampledTileData);

			FTileClassificationCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FTileClassificationCS::FDownsampledClassification>(true);
			auto ComputeShader = View.ShaderMap->GetShader<FTileClassificationCS>(PermutationVector);

			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FTileClassificationCS::GetGroupSize());

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("DownsampledTileClassification %dx%d", DownsampledViewSize.X, DownsampledViewSize.Y),
				ComputeShader,
				PassParameters,
				GroupCount);
		}
	}

	FRDGBufferRef TileIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>((int32)ManyLights::ETileType::MAX), TEXT("ManyLights.TileIndirectArgs"));
	FRDGBufferRef DownsampledTileIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>((int32)ManyLights::ETileType::MAX), TEXT("ManyLights.DownsampledTileIndirectArgs"));

	// Setup indirect args for classified tiles
	{
		FInitTileIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitTileIndirectArgsCS::FParameters>();
		PassParameters->ManyLightsParameters = ManyLightsParameters;
		PassParameters->RWTileIndirectArgs = GraphBuilder.CreateUAV(TileIndirectArgs);
		PassParameters->RWDownsampledTileIndirectArgs = GraphBuilder.CreateUAV(DownsampledTileIndirectArgs);
		PassParameters->TileAllocator = GraphBuilder.CreateSRV(TileAllocator);
		PassParameters->DownsampledTileAllocator = GraphBuilder.CreateSRV(DownsampledTileAllocator);

		auto ComputeShader = View.ShaderMap->GetShader<FInitTileIndirectArgsCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("InitTileIndirectArgs"),
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	FRDGBufferRef CompositeTileAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("ManyLights.CompositeTileAllocator"));
	FRDGBufferRef CompositeTileData = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(2 * sizeof(uint32), MaxCompositeTiles), TEXT("ManyLights.CompositeTileData"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(CompositeTileAllocator), 0);


	FRDGTextureRef SampleLuminanceSum = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(DownsampledBufferSize, PF_G16R16F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("ManyLights.SampleLuminanceSum"));
	
	// Generate new candidate light samples
	{
		FRDGTextureUAVRef SampleLuminanceSumUAV = GraphBuilder.CreateUAV(SampleLuminanceSum, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGTextureUAVRef DownsampledSceneDepthUAV = GraphBuilder.CreateUAV(DownsampledSceneDepth, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGTextureUAVRef DownsampledSceneWorldNormalUAV = GraphBuilder.CreateUAV(DownsampledSceneWorldNormal, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGBufferUAVRef CompositeTileAllocatorUAV = GraphBuilder.CreateUAV(CompositeTileAllocator, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGBufferUAVRef CompositeTileDataUAV = GraphBuilder.CreateUAV(CompositeTileData, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGTextureUAVRef LightSamplesUAV = GraphBuilder.CreateUAV(LightSamples, ERDGUnorderedAccessViewFlags::SkipBarrier);

		// Clear tiles which don't contain any lights or geometry
		{
			FClearLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FClearLightSamplesCS::FParameters>();
			PassParameters->IndirectArgs = DownsampledTileIndirectArgs;
			PassParameters->ManyLightsParameters = ManyLightsParameters;
			PassParameters->RWDownsampledSceneDepth = DownsampledSceneDepthUAV;
			PassParameters->RWDownsampledSceneWorldNormal = DownsampledSceneWorldNormalUAV;
			PassParameters->RWLightSamples = LightSamplesUAV;
			PassParameters->DownsampledTileAllocator = GraphBuilder.CreateSRV(DownsampledTileAllocator);
			PassParameters->DownsampledTileData = GraphBuilder.CreateSRV(DownsampledTileData);

			FClearLightSamplesCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FClearLightSamplesCS::FDebugMode>(bDebug);
			auto ComputeShader = View.ShaderMap->GetShader<FClearLightSamplesCS>(PermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ClearLightSamples"),
				ComputeShader,
				PassParameters,
				DownsampledTileIndirectArgs,
				(int32)ManyLights::ETileType::Empty * sizeof(FRHIDispatchIndirectParameters));
		}

		for (int32 TileType = 0; TileType < (int32)ManyLights::ETileType::SHADING_MAX; ++TileType)
		{
			FGenerateLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGenerateLightSamplesCS::FParameters>();
			PassParameters->IndirectArgs = DownsampledTileIndirectArgs;
			PassParameters->ManyLightsParameters = ManyLightsParameters;
			PassParameters->RWSampleLuminanceSum = SampleLuminanceSumUAV;
			PassParameters->RWDownsampledSceneDepth = DownsampledSceneDepthUAV;
			PassParameters->RWDownsampledSceneWorldNormal = DownsampledSceneWorldNormalUAV;
			PassParameters->RWLightSamples = LightSamplesUAV;
			PassParameters->RWCompositeTileAllocator = CompositeTileAllocatorUAV;
			PassParameters->RWCompositeTileData = CompositeTileDataUAV;
			PassParameters->DownsampledTileAllocator = GraphBuilder.CreateSRV(DownsampledTileAllocator);
			PassParameters->DownsampledTileData = GraphBuilder.CreateSRV(DownsampledTileData);
			PassParameters->HistoryScreenPositionScaleBias = HistoryScreenPositionScaleBias;
			PassParameters->HistoryUVMinMax = HistoryUVMinMax;

			FGenerateLightSamplesCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FGenerateLightSamplesCS::FTileType>(TileType);
			PermutationVector.Set<FGenerateLightSamplesCS::FIESProfile>(CVarManyLightsIESProfiles.GetValueOnRenderThread() != 0);
			PermutationVector.Set<FGenerateLightSamplesCS::FLightFunctionAtlas>(bUseLightFunctionAtlas);
			PermutationVector.Set<FGenerateLightSamplesCS::FTexturedRectLights>(CVarManyLightsTexturedRectLights.GetValueOnRenderThread() != 0);
			PermutationVector.Set<FGenerateLightSamplesCS::FNumSamplesPerPixel1d>(NumSamplesPerPixel2d.X * NumSamplesPerPixel2d.Y);
			PermutationVector.Set<FGenerateLightSamplesCS::FDebugMode>(bDebug);
			auto ComputeShader = View.ShaderMap->GetShader<FGenerateLightSamplesCS>(PermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("GenerateSamples SamplesPerPixel:%dx%d TileType:%d", NumSamplesPerPixel2d.X, NumSamplesPerPixel2d.Y, TileType),
				ComputeShader,
				PassParameters,
				DownsampledTileIndirectArgs,
				TileType * sizeof(FRHIDispatchIndirectParameters));
		}
	}

	FRDGBufferRef CompositeTileIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("ManyLights.CompositeTileIndirectArgs"));

	// Setup indirect args for shadow mask tile updates
	{
		FInitCompositeIndirectArgsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitCompositeIndirectArgsCS::FParameters>();
		PassParameters->ManyLightsParameters = ManyLightsParameters;
		PassParameters->RWIndirectArgs = GraphBuilder.CreateUAV(CompositeTileIndirectArgs);
		PassParameters->CompositeTileAllocator = GraphBuilder.CreateSRV(CompositeTileAllocator);

		auto ComputeShader = View.ShaderMap->GetShader<FInitCompositeIndirectArgsCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("InitCompositeIndirectArgs"),
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	ManyLights::RayTraceLightSamples(
		View,
		GraphBuilder, 
		SceneTextures,
		SampleBufferSize,
		LightSamples,
		LightSampleRayDistance,
		ManyLightsParameters
	);

	FRDGTextureRef CompositeUpsampleWeights = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(SceneTextures.Config.Extent, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("ManyLights.CompositeUpsampleWeights"));

	// Init composite upsample weights
	{
		FInitCompositeUpsampleWeightsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitCompositeUpsampleWeightsCS::FParameters>();
		PassParameters->ManyLightsParameters = ManyLightsParameters;
		PassParameters->RWCompositeUpsampleWeights = GraphBuilder.CreateUAV(CompositeUpsampleWeights);

		auto ComputeShader = View.ShaderMap->GetShader<FInitCompositeUpsampleWeightsCS>();

		const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FInitCompositeUpsampleWeightsCS::GetGroupSize());

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("InitCompositeUpsampleWeights"),
			ComputeShader,
			PassParameters,
			GroupCount);
	}

	FRDGBufferRef ShadingTileAllocator = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1),
		TEXT("ManyLightsParameters.ShadingTileAllocator"));

	FRDGTextureRef ShadingTileGridAllocator = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(ShadingTileGridSize, PF_R32_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("ManyLights.ShadingTileGridAllocator"));

	FRDGBufferRef ShadingTileGrid = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), ShadingTileGridSize.X * ShadingTileGridSize.Y * MaxShadingTilesPerGridCell),
		TEXT("ManyLightsParameters.ShadingTileGrid"));

	FRDGTextureRef ShadingTileAtlas = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(ShadingTileAtlasSize, PF_R16F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("ManyLights	.ShadingTileAtlas"));

	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(ShadingTileAllocator), 0u);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(ShadingTileGridAllocator), 0u);

	// Composite shadow masks traces
	{
		FResolveLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FResolveLightSamplesCS::FParameters>();
		PassParameters->ManyLightsParameters = ManyLightsParameters;
		PassParameters->IndirectArgs = CompositeTileIndirectArgs;
		PassParameters->RWShadingTileAllocator = GraphBuilder.CreateUAV(ShadingTileAllocator);
		PassParameters->RWShadingTileGridAllocator = GraphBuilder.CreateUAV(ShadingTileGridAllocator);
		PassParameters->RWShadingTileGrid = GraphBuilder.CreateUAV(ShadingTileGrid);
		PassParameters->RWShadingTileAtlas = GraphBuilder.CreateUAV(ShadingTileAtlas);
		PassParameters->CompositeTileData = GraphBuilder.CreateSRV(CompositeTileData);
		PassParameters->CompositeUpsampleWeights = CompositeUpsampleWeights;
		PassParameters->LightSamples = LightSamples;

		FResolveLightSamplesCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FResolveLightSamplesCS::FDebugMode>(bDebug);
		auto ComputeShader = View.ShaderMap->GetShader<FResolveLightSamplesCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CompositeLightSamples"),
			ComputeShader,
			PassParameters,
			CompositeTileIndirectArgs,
			0);
	}

	FRDGTextureRef ResolvedDiffuseLighting = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(View.GetSceneTexturesConfig().Extent, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("ManyLights.ResolvedDiffuseLighting"));

	FRDGTextureRef ResolvedSpecularLighting = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(View.GetSceneTexturesConfig().Extent, PF_FloatRGB, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("ManyLights.ResolvedSpecularLighting"));

	// Shade light samples
	{
		FRDGTextureUAVRef ResolvedDiffuseLightingUAV = GraphBuilder.CreateUAV(ResolvedDiffuseLighting, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGTextureUAVRef ResolvedSpecularLightingUAV = GraphBuilder.CreateUAV(ResolvedSpecularLighting, ERDGUnorderedAccessViewFlags::SkipBarrier);

		for (int32 TileType = 0; TileType < (int32)ManyLights::ETileType::SHADING_MAX; ++TileType)
		{
			FShadeLightSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FShadeLightSamplesCS::FParameters>();
			PassParameters->RWResolvedDiffuseLighting = ResolvedDiffuseLightingUAV;
			PassParameters->RWResolvedSpecularLighting = ResolvedSpecularLightingUAV;
			PassParameters->IndirectArgs = TileIndirectArgs;
			PassParameters->ManyLightsParameters = ManyLightsParameters;
			PassParameters->CompositeTileAllocator = GraphBuilder.CreateSRV(CompositeTileAllocator);
			PassParameters->TileAllocator = GraphBuilder.CreateSRV(TileAllocator);
			PassParameters->TileData = GraphBuilder.CreateSRV(TileData);
			PassParameters->ShadingTileAllocator = GraphBuilder.CreateSRV(ShadingTileAllocator);
			PassParameters->ShadingTileGridAllocator = ShadingTileGridAllocator;
			PassParameters->ShadingTileGrid = GraphBuilder.CreateSRV(ShadingTileGrid);
			PassParameters->ShadingTileAtlas = ShadingTileAtlas;

			FShadeLightSamplesCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FShadeLightSamplesCS::FTileType>(TileType);
			PermutationVector.Set<FShadeLightSamplesCS::FIESProfile>(CVarManyLightsIESProfiles.GetValueOnRenderThread() != 0);
			PermutationVector.Set<FShadeLightSamplesCS::FLightFunctionAtlas>(bUseLightFunctionAtlas);
			PermutationVector.Set<FShadeLightSamplesCS::FTexturedRectLights>(CVarManyLightsTexturedRectLights.GetValueOnRenderThread() != 0);
			PermutationVector.Set<FShadeLightSamplesCS::FDebugMode>(bDebug);
			auto ComputeShader = View.ShaderMap->GetShader<FShadeLightSamplesCS>(PermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("ShadeLightSamples TileType:%d", TileType),
				ComputeShader,
				PassParameters,
				TileIndirectArgs,
				TileType * sizeof(FRHIDispatchIndirectParameters));
		}
	}

	// Demodulated lighting components with second luminance moments stored in alpha channel for temporal variance tracking
	// This will be passed to the next frame
	FRDGTextureRef DiffuseLightingAndSecondMoment = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(View.GetSceneTexturesConfig().Extent, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("ManyLights.DiffuseLightingAndSecondMoment"));

	FRDGTextureRef SpecularLightingAndSecondMoment = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(View.GetSceneTexturesConfig().Extent, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("ManyLights.SpecularLightingAndSecondMoment"));

	FRDGTextureRef SceneDepthCopy = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(SceneTextures.Depth.Resolve->Desc.Extent, PF_R32_FLOAT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("ManyLights.DepthHistory"));

	FRDGTextureRef NumFramesAccumulated = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(View.GetSceneTexturesConfig().Extent, PF_G8, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV),
		TEXT("ManyLights.NumFramesAccumulated"));

	// Temporal accumulation
	{
		FDenoiserTemporalCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDenoiserTemporalCS::FParameters>();
		PassParameters->ManyLightsParameters = ManyLightsParameters;
		PassParameters->CompositeUpsampleWeights = CompositeUpsampleWeights;
		PassParameters->SampleLuminanceSumTexture = SampleLuminanceSum;
		PassParameters->ResolvedDiffuseLighting = ResolvedDiffuseLighting;
		PassParameters->ResolvedSpecularLighting = ResolvedSpecularLighting;
		PassParameters->DiffuseLightingAndSecondMomentHistoryTexture = DiffuseLightingAndSecondMomentHistory;
		PassParameters->SpecularLightingAndSecondMomentHistoryTexture = SpecularLightingAndSecondMomentHistory;
		PassParameters->NumFramesAccumulatedHistoryTexture = NumFramesAccumulatedHistory;
		PassParameters->ManyLightsDepthHistory = SceneDepthHistory;
		PassParameters->PrevSceneColorPreExposureCorrection = View.PreExposure / View.PrevViewInfo.SceneColorPreExposure;
		PassParameters->HistoryScreenPositionScaleBias = HistoryScreenPositionScaleBias;
		PassParameters->HistoryUVMinMax = HistoryUVMinMax;
		PassParameters->RWDiffuseLightingAndSecondMoment = GraphBuilder.CreateUAV(DiffuseLightingAndSecondMoment);
		PassParameters->RWSpecularLightingAndSecondMoment = GraphBuilder.CreateUAV(SpecularLightingAndSecondMoment);
		PassParameters->RWNumFramesAccumulated = GraphBuilder.CreateUAV(NumFramesAccumulated);
		PassParameters->RWSceneDepth = GraphBuilder.CreateUAV(SceneDepthCopy);
		PassParameters->RWSceneColor = GraphBuilder.CreateUAV(SceneTextures.Color.Target);

		FDenoiserTemporalCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FDenoiserTemporalCS::FValidHistory>(DiffuseLightingAndSecondMomentHistory != nullptr && bTemporal);
		PermutationVector.Set<FDenoiserTemporalCS::FDebugMode>(bDebug);
		auto ComputeShader = View.ShaderMap->GetShader<FDenoiserTemporalCS>(PermutationVector);

		const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FDenoiserTemporalCS::GetGroupSize());

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("TemporalAccumulation"),
			ComputeShader,
			PassParameters,
			GroupCount);
	}

	// Spatial filter
	{
		FDenoiserSpatialCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDenoiserSpatialCS::FParameters>();
		PassParameters->ManyLightsParameters = ManyLightsParameters;
		PassParameters->RWSceneColor = GraphBuilder.CreateUAV(SceneTextures.Color.Target);
		PassParameters->DiffuseLightingAndSecondMomentTexture = DiffuseLightingAndSecondMoment;
		PassParameters->SpecularLightingAndSecondMomentTexture = SpecularLightingAndSecondMoment;
		PassParameters->NumFramesAccumulatedTexture = NumFramesAccumulated;
		PassParameters->SpatialFilterDepthWeightScale = CVarManyLightsSpatialDepthWeightScale.GetValueOnRenderThread();

		FDenoiserSpatialCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FDenoiserSpatialCS::FSpatialFilter>(CVarManyLightsSpatial.GetValueOnRenderThread() != 0);
		PermutationVector.Set<FDenoiserSpatialCS::FDebugMode>(bDebug);
		auto ComputeShader = View.ShaderMap->GetShader<FDenoiserSpatialCS>(PermutationVector);

		const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FDenoiserSpatialCS::GetGroupSize());

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Spatial"),
			ComputeShader,
			PassParameters,
			GroupCount);
	}

	if (View.ViewState && !View.bStatePrevViewInfoIsReadOnly)
	{
		FManyLightsViewState& LightingViewState = View.ViewState->ManyLights;

		LightingViewState.HistoryScreenPositionScaleBias = View.GetScreenPositionScaleBias(View.GetSceneTexturesConfig().Extent, View.ViewRect);

		// Pull in the max UV to exclude the region which will read outside the viewport due to bilinear filtering
		const FVector2D InvBufferSize(1.0f / SceneTextures.Config.Extent.X, 1.0f / SceneTextures.Config.Extent.Y);
		LightingViewState.HistoryUVMinMax = FVector4f(
			(View.ViewRect.Min.X + 0.5f) * InvBufferSize.X,
			(View.ViewRect.Min.Y + 0.5f) * InvBufferSize.Y,
			(View.ViewRect.Max.X - 1.0f) * InvBufferSize.X,
			(View.ViewRect.Max.Y - 1.0f) * InvBufferSize.Y);

		if (DiffuseLightingAndSecondMoment && SpecularLightingAndSecondMoment && SceneDepthCopy && NumFramesAccumulated && bTemporal)
		{
			GraphBuilder.QueueTextureExtraction(DiffuseLightingAndSecondMoment, &LightingViewState.DiffuseLightingAndSecondMomentHistory);
			GraphBuilder.QueueTextureExtraction(SpecularLightingAndSecondMoment, &LightingViewState.SpecularLightingAndSecondMomentHistory);
			GraphBuilder.QueueTextureExtraction(SceneDepthCopy, &LightingViewState.SceneDepthHistory);
			GraphBuilder.QueueTextureExtraction(NumFramesAccumulated, &LightingViewState.NumFramesAccumulatedHistory);
		}
		else
		{
			LightingViewState.DiffuseLightingAndSecondMomentHistory = nullptr;
			LightingViewState.SpecularLightingAndSecondMomentHistory = nullptr;
			LightingViewState.SceneDepthHistory = nullptr;
			LightingViewState.NumFramesAccumulatedHistory = nullptr;
		}
	}
}