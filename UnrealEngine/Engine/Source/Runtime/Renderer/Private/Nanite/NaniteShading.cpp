// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteShading.h"
#include "NaniteVertexFactory.h"
#include "NaniteRayTracing.h"
#include "NaniteVisualizationData.h"
#include "Rendering/NaniteResources.h"
#include "Rendering/NaniteStreamingManager.h"
#include "ComponentRecreateRenderStateContext.h"
#include "VariableRateShadingImageManager.h"
#include "SystemTextures.h"
#include "SceneUtils.h"
#include "ScenePrivate.h"
#include "RHI.h"
#include "BasePassRendering.h"
#include "Async/ParallelFor.h"
#include "Materials/Material.h"
#include "Materials/MaterialRenderProxy.h"
#include "MeshPassUtils.h"
#include "PSOPrecacheMaterial.h"
#include "PSOPrecacheValidation.h"

DEFINE_STAT(STAT_CLP_NaniteBasePass);

extern TAutoConsoleVariable<int32> CVarNaniteShowDrawEvents;

extern int32 GSkipDrawOnPSOPrecaching;
extern int32 GNaniteShowStats;

#if WANTS_DRAW_MESH_EVENTS
static FORCEINLINE const TCHAR* GetShadingMaterialName(const FMaterialRenderProxy* InShadingMaterial)
{
	if (InShadingMaterial == nullptr)
	{
		return TEXT("<Invalid>");
	}

	return *InShadingMaterial->GetMaterialName();
}
#endif

TAutoConsoleVariable<int32> CVarParallelBasePassBuild(
	TEXT("r.Nanite.ParallelBasePassBuild"),
	1,
	TEXT(""),
	ECVF_RenderThreadSafe
);

static int32 GNaniteBarrierTest = 1;
static FAutoConsoleVariableRef CVarNaniteBarrierTest(
	TEXT("r.Nanite.BarrierTest"),
	GNaniteBarrierTest,
	TEXT(""),
	ECVF_RenderThreadSafe
);

static int32 GNaniteFastTileClear = 1;
static FAutoConsoleVariableRef CVarNaniteFastTileClear(
	TEXT("r.Nanite.FastTileClear"),
	GNaniteFastTileClear,
	TEXT("Whether to enable Nanite fast tile clearing"),
	ECVF_RenderThreadSafe
);

static int32 GNaniteFastTileClearSubTiles = 1;
static FAutoConsoleVariableRef CVarNaniteFastTileClearSubTiles(
	TEXT("r.Nanite.FastTileClear.SubTiles"),
	GNaniteFastTileClearSubTiles,
	TEXT("Whether to enable Nanite fast tile clearing (for 4x4 sub tiles)"),
	ECVF_RenderThreadSafe
);

static int32 GNaniteFastTileVis = INDEX_NONE;
static FAutoConsoleVariableRef CVarNaniteFastTileVis(
	TEXT("r.Nanite.FastTileVis"),
	GNaniteFastTileVis,
	TEXT("Allows for just showing a single target in the visualization, or -1 to show all accumulated"),
	ECVF_RenderThreadSafe
);

static int32 GNaniteBundleEmulation = 0;
static FAutoConsoleVariableRef CVarNaniteBundleEmulation(
	TEXT("r.Nanite.Bundle.Emulation"),
	GNaniteBundleEmulation,
	TEXT("Whether to force shader bundle dispatch emulation"),
	ECVF_RenderThreadSafe
);

static int32 GNaniteBundleShading = 0;
static FAutoConsoleVariableRef CVarNaniteBundleShading(
	TEXT("r.Nanite.Bundle.Shading"),
	GNaniteBundleShading,
	TEXT("Whether to enable Nanite shader bundle dispatch for shading"),
	ECVF_RenderThreadSafe
);

static int32 GNaniteComputeMaterialsSort = 1;
static FAutoConsoleVariableRef CVarNaniteComputeMaterialsSort(
	TEXT("r.Nanite.ComputeMaterials.Sort"),
	GNaniteComputeMaterialsSort,
	TEXT(""),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		// We need to recreate scene proxies so that BuildShadingCommands can be re-evaluated.
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_RenderThreadSafe
);

static int32 GBinningTechnique = 0;
static FAutoConsoleVariableRef CVarNaniteBinningTechnique(
	TEXT("r.Nanite.BinningTechnique"),
	GBinningTechnique,
	TEXT(""),
	ECVF_RenderThreadSafe
);

static int32 GNaniteShadeBinningMode = 0;
static FAutoConsoleVariableRef CVarNaniteShadeBinningMode(
	TEXT("r.Nanite.ShadeBinningMode"),
	GNaniteShadeBinningMode,
	TEXT("0: Auto\n")
	TEXT("1: Force to Pixel Mode\n")
	TEXT("2: Force to Quad Mode\n"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		// We need to recreate scene proxies so that BuildShadingCommands can be re-evaluated.
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_RenderThreadSafe
);

static int32 GNaniteSoftwareVRS = 1;
static FAutoConsoleVariableRef CVarNaniteSoftwareVRS(
	TEXT("r.Nanite.SoftwareVRS"),
	GNaniteSoftwareVRS,
	TEXT("Whether to enable Nanite software variable rate shading in compute."),
	ECVF_RenderThreadSafe
);

int32 GNaniteValidateShadeBinning = 0;
static FAutoConsoleVariableRef CVarNaniteValidateShadeBinning(
	TEXT("r.Nanite.Debug.ValidateShadeBinning"),
	GNaniteValidateShadeBinning,
	TEXT(""),
	ECVF_RenderThreadSafe
);

static uint32 GetShadingRateTileSizeBits()
{
	uint32 TileSizeBits = 0;

	// Temporarily disable this on Intel until the shader is fixed to
	// correctly handle a wave size of 16.
	if (GNaniteSoftwareVRS != 0 && !IsRHIDeviceIntel() && GVRSImageManager.IsVRSEnabledForFrame() /* HW or SW VRS enabled? */)
	{
		bool bUseSoftwareImage = GVRSImageManager.IsSoftwareVRSEnabledForFrame();
		if (!bUseSoftwareImage)
		{
			// Technically these could be different, but currently never in practice
			// 8x8, 16x16, or 32x32 for DX12 Tier2 HW VRS
			ensure
			(
				GRHIVariableRateShadingImageTileMinWidth == GRHIVariableRateShadingImageTileMinHeight &&
				GRHIVariableRateShadingImageTileMinWidth == GRHIVariableRateShadingImageTileMaxWidth &&
				GRHIVariableRateShadingImageTileMinWidth == GRHIVariableRateShadingImageTileMaxHeight &&
				FMath::IsPowerOfTwo(GRHIVariableRateShadingImageTileMinWidth)
			);
		}

		uint32 TileSize = GVRSImageManager.GetSRITileSize(bUseSoftwareImage).X;
		TileSizeBits = FMath::FloorLog2(TileSize);
	}

	return TileSizeBits;
}

static FRDGTextureRef GetShadingRateImage(FRDGBuilder& GraphBuilder, const FViewInfo& ViewInfo)
{
	FRDGTextureRef ShadingRateImage = nullptr;

	if (GetShadingRateTileSizeBits() != 0)
	{
		bool bUseSoftwareImage = GVRSImageManager.IsSoftwareVRSEnabledForFrame();
		ShadingRateImage = GVRSImageManager.GetVariableRateShadingImage(GraphBuilder, ViewInfo, FVariableRateShadingImageManager::EVRSPassType::NaniteEmitGBufferPass, bUseSoftwareImage);
	}

	if (ShadingRateImage == nullptr)
	{
		const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);
		ShadingRateImage = SystemTextures.Black;
	}

	return ShadingRateImage;
}

class FVisualizeClearTilesCS : public FNaniteGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FVisualizeClearTilesCS);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FUint32Vector4, ViewRect)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTextureMetadata, OutCMaskBuffer)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, OutVisualized)
	END_SHADER_PARAMETER_STRUCT()

	FVisualizeClearTilesCS() = default;
	FVisualizeClearTilesCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FNaniteGlobalShader(Initializer)
	{
		PlatformDataParam.Bind(Initializer.ParameterMap, TEXT("PlatformData"), SPF_Mandatory);
		BindForLegacyShaderParameters<FParameters>(this, Initializer.PermutationId, Initializer.ParameterMap);
	}

	// Shader parameter structs don't have a way to push variable sized data yet. So the we use the old shader parameter API.
	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters, const void* PlatformDataPtr, uint32 PlatformDataSize)
	{
		BatchedParameters.SetShaderParameter(PlatformDataParam.GetBufferIndex(), PlatformDataParam.GetBaseIndex(), PlatformDataSize, PlatformDataPtr);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return RHISupportsRenderTargetWriteMask(Parameters.Platform) && DoesPlatformSupportNanite(Parameters.Platform);
	}

private:
	LAYOUT_FIELD(FShaderParameter, PlatformDataParam);
};
IMPLEMENT_GLOBAL_SHADER(FVisualizeClearTilesCS, "/Engine/Private/Nanite/NaniteFastClear.usf", "VisualizeClearTilesCS", SF_Compute);

class FShadingBinBuildCS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FShadingBinBuildCS);

	class FBuildPassDim : SHADER_PERMUTATION_SPARSE_INT("SHADING_BIN_PASS", NANITE_SHADING_BIN_COUNT, NANITE_SHADING_BIN_SCATTER);
	class FTechniqueDim : SHADER_PERMUTATION_INT("BINNING_TECHNIQUE", 2);
	class FGatherStatsDim : SHADER_PERMUTATION_BOOL("GATHER_STATS");
	class FVariableRateDim : SHADER_PERMUTATION_BOOL("VARIABLE_SHADING_RATE");
	class FOptimizeWriteMaskDim : SHADER_PERMUTATION_BOOL("OPTIMIZE_WRITE_MASK");
	class FNumExports : SHADER_PERMUTATION_RANGE_INT("NUM_EXPORTS", 1, MaxSimultaneousRenderTargets);
	using FPermutationDomain = TShaderPermutationDomain<FBuildPassDim, FTechniqueDim, FGatherStatsDim, FVariableRateDim, FOptimizeWriteMaskDim, FNumExports>;

	FShadingBinBuildCS() = default;
	FShadingBinBuildCS(const ShaderMetaType::CompiledShaderInitializerType & Initializer)
	: FNaniteGlobalShader(Initializer)
	{
		PlatformDataParam.Bind(Initializer.ParameterMap, TEXT("PlatformData"), SPF_Optional);
		SubTileMatchParam.Bind(Initializer.ParameterMap, TEXT("SubTileMatch"), SPF_Optional);
		BindForLegacyShaderParameters<FParameters>(this, Initializer.PermutationId, Initializer.ParameterMap);
	}

	// Shader parameter structs don't have a way to push variable sized data yet. So the we use the old shader parameter API.
	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters, const void* PlatformDataPtr, uint32 PlatformDataSize, bool bSubTileMatch)
	{
		BatchedParameters.SetShaderParameter(PlatformDataParam.GetBufferIndex(), PlatformDataParam.GetBaseIndex(), PlatformDataSize, PlatformDataPtr);

		uint32 SubTileMatch = bSubTileMatch ? 1u : 0u;
		BatchedParameters.SetShaderParameter(SubTileMatchParam.GetBufferIndex(), SubTileMatchParam.GetBaseIndex(), sizeof(SubTileMatch), &SubTileMatch);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (!DoesPlatformSupportNanite(Parameters.Platform))
		{
			return false;
		}

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FOptimizeWriteMaskDim>() && !RHISupportsRenderTargetWriteMask(Parameters.Platform))
		{
			return false;
		}

		if (PermutationVector.Get<FOptimizeWriteMaskDim>() && PermutationVector.Get<FBuildPassDim>() != NANITE_SHADING_BIN_COUNT)
		{
			// We only want one of the build passes to export out cmask, so we choose the 
			// counting pass because it touches less memory already than scatter.
			return false;
		}

		if (!PermutationVector.Get<FOptimizeWriteMaskDim>() && PermutationVector.Get<FNumExports>() > 1)
		{
			// The NUM_EXPORTS perm is only valid when optimizing the write mask.
			return false;
		}

		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FUint32Vector4, ViewRect)
		SHADER_PARAMETER(uint32, ValidWriteMask)
		SHADER_PARAMETER(FUint32Vector2, DispatchOffsetTL)
		SHADER_PARAMETER(uint32, ShadingBinCount)
		SHADER_PARAMETER(uint32, ShadingBinDataByteOffset)
		SHADER_PARAMETER(uint32, ShadingRateTileSizeBits)
		SHADER_PARAMETER(uint32, DummyZero)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadingRateImage)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadingMask)
		SHADER_PARAMETER_SAMPLER(SamplerState, ShadingMaskSampler)
		SHADER_PARAMETER_RDG_TEXTURE_UAV_ARRAY(RWTextureMetadata, OutCMaskBuffer, [MaxSimultaneousRenderTargets])
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FNaniteShadingBinStats>, OutShadingBinStats)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, OutShadingBinData)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, OutShadingBinArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FNaniteShadingBinScatterMeta>, OutShadingBinScatterMeta)
	END_SHADER_PARAMETER_STRUCT()

private:
	LAYOUT_FIELD(FShaderParameter, PlatformDataParam);
	LAYOUT_FIELD(FShaderParameter, SubTileMatchParam);
};
IMPLEMENT_GLOBAL_SHADER(FShadingBinBuildCS, "/Engine/Private/Nanite/NaniteShadeBinning.usf", "ShadingBinBuildCS", SF_Compute);

class FShadingBinReserveCS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FShadingBinReserveCS);
	SHADER_USE_PARAMETER_STRUCT(FShadingBinReserveCS, FNaniteGlobalShader);

	class FGatherStatsDim : SHADER_PERMUTATION_BOOL("GATHER_STATS");
	using FPermutationDomain = TShaderPermutationDomain<FGatherStatsDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADING_BIN_PASS"), NANITE_SHADING_BIN_RESERVE);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, ShadingBinCount)
		SHADER_PARAMETER(uint32, ShadingBinDataByteOffset)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FNaniteShadingBinStats>, OutShadingBinStats)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, OutShadingBinData)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutShadingBinAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, OutShadingBinArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FNaniteShadingBinScatterMeta>, OutShadingBinScatterMeta)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FShadingBinReserveCS, "/Engine/Private/Nanite/NaniteShadeBinning.usf", "ShadingBinReserveCS", SF_Compute);

class FShadingBinValidateCS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FShadingBinValidateCS);
	SHADER_USE_PARAMETER_STRUCT(FShadingBinValidateCS, FNaniteGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADING_BIN_PASS"), NANITE_SHADING_BIN_VALIDATE);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, ShadingBinCount)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, OutShadingBinData)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FShadingBinValidateCS, "/Engine/Private/Nanite/NaniteShadeBinning.usf", "ShadingBinValidateCS", SF_Compute);


BEGIN_SHADER_PARAMETER_STRUCT(FNaniteShadingPassParameters, )
	RDG_BUFFER_ACCESS(MaterialIndirectArgs, ERHIAccess::IndirectArgs)
	SHADER_PARAMETER(uint32, ActiveShadingBin)

	SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)	// To access VTFeedbackBuffer
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FNaniteUniformParameters, Nanite)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FOpaqueBasePassUniformParameters, BasePass)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenCardPassUniformParameters, CardPass)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutTarget0)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutTarget1)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutTarget2)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutTarget3)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutTarget4)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutTarget5)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutTarget6)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutTarget7)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray, OutTargets)

	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, RecordArgBuffer)
END_SHADER_PARAMETER_STRUCT()

namespace Nanite
{

bool HasNoDerivativeOps(FRHIComputeShader* ComputeShaderRHI)
{
	if (GNaniteShadeBinningMode == 1)
	{
		return true;
	}
	else if (GNaniteShadeBinningMode == 2)
	{
		return false;
	}
	else
	{
		return ComputeShaderRHI ? ComputeShaderRHI->HasNoDerivativeOps() : false;
	}
}

void BuildShadingCommands(FRDGBuilder& GraphBuilder, FScene& Scene, ENaniteMeshPass::Type MeshPass, FNaniteShadingCommands& ShadingCommands, bool bForceBuildCommands)
{
	FNaniteShadingPipelines& ShadingPipelines = Scene.NaniteShadingPipelines[MeshPass];
	if (ShadingPipelines.bBuildCommands || bForceBuildCommands)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Nanite::BuildShadingCommands);
		const auto& Pipelines = ShadingPipelines.GetShadingPipelineMap();

		ShadingCommands.SetupTask = GraphBuilder.AddSetupTask([&ShadingCommands, &Pipelines]
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Nanite::BuildShadingCommandsMetadata);
			ShadingCommands.MaxShadingBin = 0u;
			ShadingCommands.BoundTargetMask = 0x0u;
			ShadingCommands.NumCommands = Pipelines.Num();

			for (const auto& Iter : Pipelines)
			{
				const FNaniteShadingEntry& Entry = Iter.Value;
				ShadingCommands.MaxShadingBin = FMath::Max<uint32>(ShadingCommands.MaxShadingBin, uint32(Entry.BinIndex));
				ShadingCommands.BoundTargetMask |= Entry.ShadingPipeline->BoundTargetMask;
			}

			ShadingCommands.MetaBufferData.SetNumZeroed(ShadingCommands.MaxShadingBin + 1u);

			for (const auto& Iter : Pipelines)
			{
				const FNaniteShadingEntry& Entry = Iter.Value;
				FUintVector4& MetaEntry = ShadingCommands.MetaBufferData[Entry.BinIndex];
				// Note: .XYZ are populated by the GPU during shade binning
				MetaEntry.W = Entry.ShadingPipeline->MaterialBitFlags;
			}

			// Create Shader Bundle
			if (!!GRHISupportsShaderBundleDispatch && ShadingCommands.NumCommands > 0)
			{
				const uint32 NumRecords = ShadingCommands.MaxShadingBin + 1u;
				ShadingCommands.ShaderBundle = RHICreateShaderBundle(NumRecords);
				check(ShadingCommands.ShaderBundle != nullptr);
			}
			else
			{
				ShadingCommands.ShaderBundle = nullptr;
			}
		});

		ShadingCommands.BuildCommandsTask = GraphBuilder.AddSetupTask([&Pipelines, &Commands = ShadingCommands.Commands]
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(Nanite::BuildShadingCommandsTask);
			Commands.Reset();
			Commands.Reserve(Pipelines.Num());

			for (const auto& Iter : Pipelines)
			{
				FNaniteShadingCommand& ShadingCommand = Commands.AddDefaulted_GetRef();
				const FNaniteShadingEntry& Entry = Iter.Value;
				ShadingCommand.Pipeline = Entry.ShadingPipeline;
				ShadingCommand.ShadingBin = Entry.BinIndex;
			}

			if (GNaniteComputeMaterialsSort != 0)
			{
				Commands.Sort([](auto& A, auto& B)
				{
					const FNaniteShadingPipeline& PipelineA = *A.Pipeline.Get();
					const FNaniteShadingPipeline& PipelineB = *B.Pipeline.Get();

					// First group all shaders with the same bound target mask (UAV exports)
					if (PipelineA.BoundTargetMask != PipelineB.BoundTargetMask)
					{
						return PipelineA.BoundTargetMask < PipelineB.BoundTargetMask;
					}

					// Then group up all shading bins using same shader but different bindings
					if (PipelineA.ComputeShader != PipelineB.ComputeShader)
					{
						return PipelineA.ComputeShader < PipelineB.ComputeShader;
					}

					// Sort indirect arg memory location in ascending order to help minimize cache misses on the indirect args
					return A.ShadingBin < B.ShadingBin;
				});
			}

		}, ShadingCommands.SetupTask);

		if (!bForceBuildCommands)
		{
			ShadingPipelines.bBuildCommands = false;
		}
	}
}

uint32 PackMaterialBitFlags(const FMaterial& Material, uint32 BoundTargetMask, bool bNoDerivativeOps)
{
	FNaniteMaterialFlags Flags = { 0 };
	Flags.bPixelDiscard = Material.IsMasked();
	Flags.bPixelDepthOffset = Material.MaterialUsesPixelDepthOffset_RenderThread();
	Flags.bWorldPositionOffset = Material.MaterialUsesWorldPositionOffset_RenderThread();
	Flags.bDisplacement = UseNaniteTessellation() && Material.MaterialUsesDisplacement_RenderThread();
	Flags.bNoDerivativeOps = bNoDerivativeOps;
	Flags.bTwoSided = Material.IsTwoSided();
	const uint32 PackedFlags = PackNaniteMaterialBitFlags(Flags);
	return ((BoundTargetMask & 0xFFu) << 24u) | (PackedFlags & 0x00FFFFFFu);
}

bool LoadBasePassPipeline(
	const FScene& Scene,
	FSceneProxyBase* SceneProxy,
	FSceneProxyBase::FMaterialSection& Section,
	FNaniteShadingPipeline& ShadingPipeline
)
{
	static const bool bAllowStaticLighting = IsStaticLightingAllowed();

	const ERHIFeatureLevel::Type FeatureLevel = Scene.GetFeatureLevel();

	FNaniteVertexFactory* NaniteVertexFactory = Nanite::GVertexFactoryResource.GetVertexFactory2();
	FVertexFactoryType* NaniteVertexFactoryType = NaniteVertexFactory->GetType();

	const FMaterialRenderProxy* MaterialProxy = Section.ShadingMaterialProxy;
	while (MaterialProxy)
	{
		const FMaterial* Material = MaterialProxy->GetMaterialNoFallback(FeatureLevel);
		if (Material)
		{
			break;
		}
		MaterialProxy = MaterialProxy->GetFallback(FeatureLevel);
	}

	check(MaterialProxy);

	ELightMapPolicyType LightMapPolicyType = ELightMapPolicyType::LMP_NO_LIGHTMAP;

	FLightCacheInterface* LightCacheInterface = nullptr;
	if (bAllowStaticLighting && SceneProxy->HasStaticLighting())
	{
		FPrimitiveSceneProxy::FLCIArray LCIs;
		SceneProxy->GetLCIs(LCIs);

		// We expect a Nanite scene proxy can only ever have a single LCI
		check(LCIs.Num() == 1u);
		LightCacheInterface = LCIs[0];
	}

	bool bRenderSkylight = false;

	TShaderRef<TBasePassComputeShaderPolicyParamType<FUniformLightMapPolicy>> BasePassComputeShader;

	auto LoadShadingMaterial = [&](const FMaterialRenderProxy* MaterialProxyPtr)
	{
		const FMaterial& ShadingMaterial = MaterialProxy->GetIncompleteMaterialWithFallback(FeatureLevel);
		check(Nanite::IsSupportedMaterialDomain(ShadingMaterial.GetMaterialDomain()));
		check(Nanite::IsSupportedBlendMode(ShadingMaterial));

		const FMaterialShadingModelField ShadingModels = ShadingMaterial.GetShadingModels();
		bRenderSkylight = Scene.ShouldRenderSkylightInBasePass(IsTranslucentBlendMode(ShadingMaterial.GetBlendMode())) && ShadingModels != MSM_Unlit;

		if (LightCacheInterface)
		{
			LightMapPolicyType = FBasePassMeshProcessor::GetUniformLightMapPolicyType(FeatureLevel, &Scene, LightCacheInterface, SceneProxy, ShadingMaterial);
		}

		bool bShadersValid = GetBasePassShader<FUniformLightMapPolicy>(
			ShadingMaterial,
			NaniteVertexFactoryType,
			FUniformLightMapPolicy(LightMapPolicyType),
			FeatureLevel,
			bRenderSkylight,
			&BasePassComputeShader
		);

		return bShadersValid;
	};

	bool bLoaded = LoadShadingMaterial(MaterialProxy);
	if (!bLoaded)
	{
		MaterialProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
		bLoaded = LoadShadingMaterial(MaterialProxy);
	}

	if (bLoaded)
	{
		ShadingPipeline.MaterialProxy		= MaterialProxy;
		ShadingPipeline.Material			= MaterialProxy->GetMaterialNoFallback(FeatureLevel);
		ShadingPipeline.BoundTargetMask		= BasePassComputeShader->GetBoundTargetMask();
		ShadingPipeline.ComputeShader		= BasePassComputeShader.GetComputeShader();
		ShadingPipeline.bIsTwoSided			= !!Section.MaterialRelevance.bTwoSided;
		ShadingPipeline.bIsMasked			= !!Section.MaterialRelevance.bMasked;
		ShadingPipeline.bNoDerivativeOps	= HasNoDerivativeOps(ShadingPipeline.ComputeShader);
		ShadingPipeline.MaterialBitFlags	= PackMaterialBitFlags(*ShadingPipeline.Material, ShadingPipeline.BoundTargetMask, ShadingPipeline.bNoDerivativeOps);

		ShadingPipeline.BasePassData = MakePimpl<FNaniteBasePassData, EPimplPtrMode::DeepCopy>();
		ShadingPipeline.BasePassData->TypedShader = BasePassComputeShader;

		check(ShadingPipeline.ComputeShader);

		TBasePassShaderElementData<FUniformLightMapPolicy> ShaderElementData(LightCacheInterface);
		ShaderElementData.InitializeMeshMaterialData();

		ShadingPipeline.ShaderBindings = MakePimpl<FMeshDrawShaderBindings, EPimplPtrMode::DeepCopy>();

		UE::MeshPassUtils::SetupComputeBindings(BasePassComputeShader, &Scene, FeatureLevel, SceneProxy, *MaterialProxy, *ShadingPipeline.Material, ShaderElementData, *ShadingPipeline.ShaderBindings);

		ShadingPipeline.ShaderBindingsHash = ShadingPipeline.ShaderBindings->GetDynamicInstancingHash();
	}

	return bLoaded;
}

inline void RecordShadingParameters(
	FRHIBatchedShaderParameters& BatchedParameters,
	FNaniteShadingCommand& ShadingCommand,
	const uint32 DataByteOffset,
	const FUint32Vector4& ViewRect,
	const TArray<FRHIUnorderedAccessView*, TInlineAllocator<8>>& OutputTargets,
	FRHIUnorderedAccessView* OutputTargetsArray
)
{
	FRHIComputeShader* ComputeShaderRHI = ShadingCommand.Pipeline->ComputeShader;
	const bool bNoDerivativeOps = !!ShadingCommand.Pipeline->bNoDerivativeOps;

	ShadingCommand.PassData.X = ShadingCommand.ShadingBin; // Active Shading Bin
	ShadingCommand.PassData.Y = bNoDerivativeOps ? 0 /* Pixel Binning */ : 1 /* Quad Binning */;
	ShadingCommand.PassData.Z = DataByteOffset;
	ShadingCommand.PassData.W = 0; // Unused

	ShadingCommand.Pipeline->ShaderBindings->SetParameters(BatchedParameters, ComputeShaderRHI);

	if (ComputeShaderRHI)
	{
		ShadingCommand.Pipeline->BasePassData->TypedShader->SetPassParameters(
			BatchedParameters,
			ViewRect,
			ShadingCommand.PassData,
			OutputTargets[0],
			OutputTargets[1],
			OutputTargets[2],
			OutputTargets[3],
			OutputTargets[4],
			OutputTargets[5],
			OutputTargets[6],
			OutputTargets[7],
			OutputTargetsArray
		);
	}
}

inline void RecordShadingCommand(
	FRHIComputeCommandList& RHICmdList,
	FRHIBuffer* IndirectArgsBuffer,
	const uint32 IndirectArgStride,
	FRHIBatchedShaderParameters& ShadingParameters,
	FNaniteShadingCommand& ShadingCommand
)
{
#if WANTS_DRAW_MESH_EVENTS
	SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, SWShading, CVarNaniteShowDrawEvents.GetValueOnRenderThread() != 0, TEXT("%s"), GetShadingMaterialName(ShadingCommand.Pipeline->MaterialProxy));
#endif

	const uint32 IndirectOffset = (ShadingCommand.ShadingBin * IndirectArgStride);

	FRHIComputeShader* ComputeShaderRHI = ShadingCommand.Pipeline->ComputeShader;
	SetComputePipelineState(RHICmdList, ComputeShaderRHI);

	if (GRHISupportsShaderRootConstants)
	{
		RHICmdList.SetShaderRootConstants(ShadingCommand.PassData);
	}

	RHICmdList.SetBatchedShaderParameters(ComputeShaderRHI, ShadingParameters);
	RHICmdList.DispatchIndirectComputeShader(IndirectArgsBuffer, IndirectOffset);
}

inline bool PrepareShadingCommand(FNaniteShadingCommand& ShadingCommand)
{
	if (!PipelineStateCache::IsPSOPrecachingEnabled())
	{
		ShadingCommand.PSOPrecacheState = EPSOPrecacheResult::Unknown;
		return true;
	}

	EPSOPrecacheResult PSOPrecacheResult = ShadingCommand.PSOPrecacheState;
	bool bShouldCheckPrecacheResult = false;

	// If PSO precache validation is on, we need to check the state for stats tracking purposes.
#if PSO_PRECACHING_VALIDATE
	if (PSOCollectorStats::IsPrecachingValidationEnabled() && PSOPrecacheResult == EPSOPrecacheResult::Unknown)
	{
		bShouldCheckPrecacheResult = true;
	}
#endif

	// If we are skipping commands when the PSO is being precached but is not ready, we
	// need to keep checking the state until it's not marked active anymore.
	const bool bAllowSkip = true;
	if (bAllowSkip && GSkipDrawOnPSOPrecaching)
	{
		if (PSOPrecacheResult == EPSOPrecacheResult::Unknown ||
			PSOPrecacheResult == EPSOPrecacheResult::Active)
		{
			bShouldCheckPrecacheResult = true;
		}
	}

	if (bShouldCheckPrecacheResult)
	{
		// Cache the state so that it's only checked again if necessary.
		PSOPrecacheResult = PipelineStateCache::CheckPipelineStateInCache(ShadingCommand.Pipeline->ComputeShader);
		ShadingCommand.PSOPrecacheState = PSOPrecacheResult;
	}

#if PSO_PRECACHING_VALIDATE
	static int32 PSOCollectorIndex = FPSOCollectorCreateManager::GetIndex(EShadingPath::Deferred, TEXT("NaniteMesh"));
	PSOCollectorStats::CheckComputePipelineStateInCache(*ShadingCommand.Pipeline->ComputeShader, PSOPrecacheResult, ShadingCommand.Pipeline->MaterialProxy, PSOCollectorIndex);
#endif

	// Try and skip draw if the PSO is not precached yet.
	const bool bSkipped = (bAllowSkip && GSkipDrawOnPSOPrecaching && PSOPrecacheResult == EPSOPrecacheResult::Active);
	return !bSkipped;
}

class FRecordShadingCommandsAnyThreadTask : public FRenderTask
{
	FRHICommandList& RHICmdList;
	TSharedPtr<TBitArray<SceneRenderingBitArrayAllocator>> VisibilityData;
	FNaniteShadingCommands& ShadingCommands;
	TArray<FRHIUnorderedAccessView*, TInlineAllocator<8>> OutputTargets;
	FRHIUnorderedAccessView* OutputTargetsArray = nullptr;
	FUint32Vector4 ViewRect;
	FRHIBuffer* IndirectArgs = nullptr;
	uint32 IndirectArgsStride;
	uint32 DataByteOffset;
	uint32 ViewIndex;
	int32 TaskIndex;
	int32 TaskNum;

public:
	FRecordShadingCommandsAnyThreadTask(
		FRHICommandList& InRHICmdList,
		FRHIBuffer* InIndirectArgs,
		uint32 InIndirectArgsStride,
		uint32 InDataByteOffset,
		TSharedPtr<TBitArray<SceneRenderingBitArrayAllocator>> InVisibilityData,
		FNaniteShadingCommands& InShadingCommands,
		const TConstArrayView<FRHIUnorderedAccessView*> InOutputTargets,
		FRHIUnorderedAccessView* InOutputTargetsArray,
		const FUint32Vector4& InViewRect,
		uint32 InViewIndex,
		int32 InTaskIndex,
		int32 InTaskNum
	)
		: RHICmdList(InRHICmdList)
		, VisibilityData(InVisibilityData)
		, ShadingCommands(InShadingCommands)
		, OutputTargets(InOutputTargets)
		, OutputTargetsArray(InOutputTargetsArray)
		, ViewRect(InViewRect)
		, IndirectArgs(InIndirectArgs)
		, IndirectArgsStride(InIndirectArgsStride)
		, DataByteOffset(InDataByteOffset)
		, ViewIndex(InViewIndex)
		, TaskIndex(InTaskIndex)
		, TaskNum(InTaskNum)
	{}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FRecordShadingCommandsAnyThreadTask, STATGROUP_TaskGraphTasks);
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		FOptionalTaskTagScope Scope(ETaskTag::EParallelRenderingThread);
		TRACE_CPUPROFILER_EVENT_SCOPE(RecordShadingCommandsAnyThreadTask);

		// Recompute shading command range.
		const int32 CommandNum = ShadingCommands.Commands.Num();
		const int32 NumCommandsPerTask = TaskIndex < CommandNum ? FMath::DivideAndRoundUp(CommandNum, TaskNum) : 0;
		const int32 StartIndex = TaskIndex * NumCommandsPerTask;
		const int32 NumCommands = FMath::Min(NumCommandsPerTask, CommandNum - StartIndex);

		for (int32 CommandIndex = 0; CommandIndex < NumCommands; ++CommandIndex)
		{
			FNaniteShadingCommand& ShadingCommand = ShadingCommands.Commands[StartIndex + CommandIndex];
			ShadingCommand.bVisible = !VisibilityData.IsValid() || VisibilityData->AccessCorrespondingBit(FRelativeBitReference(ShadingCommand.ShadingBin));
			if (ShadingCommand.bVisible && PrepareShadingCommand(ShadingCommand))
			{
				FRHIBatchedShaderParameters ShadingParameters;

				RecordShadingParameters(
					ShadingParameters,
					ShadingCommand,
					DataByteOffset,
					ViewRect,
					OutputTargets,
					OutputTargetsArray
				);

				RecordShadingCommand(
					RHICmdList,
					IndirectArgs,
					IndirectArgsStride,
					ShadingParameters,
					ShadingCommand
				);
			}
		}

		RHICmdList.FinishRecording();
	}
};

FNaniteShadingPassParameters CreateNaniteShadingPassParams(
	FRDGBuilder& GraphBuilder,
	const FSceneRenderer& SceneRenderer,
	const FSceneTextures& SceneTextures,
	const FDBufferTextures& DBufferTextures,
	const FViewInfo& View,
	const FIntRect ViewRect,
	const FRasterResults& RasterResults,
	FRDGTextureRef ShadingMask,
	FRDGTextureRef VisBuffer64,
	FRDGTextureRef DbgBuffer64,
	FRDGTextureRef DbgBuffer32,
	FRDGBufferRef VisibleClustersSWHW,
	FRDGBufferRef MultiViewIndices,
	FRDGBufferRef MultiViewRectScaleOffsets,
	FRDGBufferRef ViewsBuffer,
	const FRenderTargetBindingSlots& BasePassRenderTargets,
	const uint32 BoundTargetMask,
	const FShadeBinning& ShadeBinning
)
{
	FNaniteShadingPassParameters Result;

	Result.MaterialIndirectArgs = ShadeBinning.ShadingBinArgs;

	Result.RecordArgBuffer = nullptr;

	{
		FNaniteUniformParameters* UniformParameters = GraphBuilder.AllocParameters<FNaniteUniformParameters>();
		UniformParameters->PageConstants = RasterResults.PageConstants;
		UniformParameters->MaxNodes = RasterResults.MaxNodes;
		UniformParameters->MaxVisibleClusters = RasterResults.MaxVisibleClusters;
		UniformParameters->RenderFlags = RasterResults.RenderFlags;

		UniformParameters->MaterialConfig = FIntVector4(1 /* Indirect */, 0, 0, 0); // TODO: Remove

		UniformParameters->ClusterPageData = Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
		UniformParameters->HierarchyBuffer = Nanite::GStreamingManager.GetHierarchySRV(GraphBuilder);
		UniformParameters->VisibleClustersSWHW = GraphBuilder.CreateSRV(VisibleClustersSWHW);

		UniformParameters->MaterialTileRemap = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<uint32>(GraphBuilder), PF_R32_UINT); // TODO: Remove

	#if RHI_RAYTRACING
		UniformParameters->RayTracingCutError = Nanite::GRayTracingManager.GetCutError();
		UniformParameters->RayTracingDataBuffer = Nanite::GRayTracingManager.GetAuxiliaryDataSRV(GraphBuilder);
	#else
		UniformParameters->RayTracingCutError = 0.0f;
		UniformParameters->RayTracingDataBuffer = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<uint32>(GraphBuilder));
	#endif

		UniformParameters->VisBuffer64 = VisBuffer64;
		UniformParameters->DbgBuffer64 = DbgBuffer64;
		UniformParameters->DbgBuffer32 = DbgBuffer32;

		UniformParameters->ShadingMask = ShadingMask;

		UniformParameters->MaterialDepthTable = SceneRenderer.Scene->NaniteMaterials[ENaniteMeshPass::BasePass].GetMaterialDepthSRV(); // TODO: Remove

		UniformParameters->MultiViewEnabled = 0;
		UniformParameters->MultiViewIndices = GraphBuilder.CreateSRV(MultiViewIndices);
		UniformParameters->MultiViewRectScaleOffsets = GraphBuilder.CreateSRV(MultiViewRectScaleOffsets);
		UniformParameters->InViews = GraphBuilder.CreateSRV(ViewsBuffer);

		UniformParameters->ShadingBinData = GraphBuilder.CreateSRV(ShadeBinning.ShadingBinData);

		Result.Nanite = GraphBuilder.CreateUniformBuffer(UniformParameters);
	}

	Result.View = View.GetShaderParameters(); // To get VTFeedbackBuffer
	Result.Scene = View.GetSceneUniforms().GetBuffer(GraphBuilder);
	const bool bLumenGIEnabled = SceneRenderer.IsLumenGIEnabled(View);
	Result.BasePass = CreateOpaqueBasePassUniformBuffer(GraphBuilder, View, 0, {}, DBufferTextures, bLumenGIEnabled);
	Result.ActiveShadingBin = ~uint32(0);

	// No possibility of read/write hazard due to fully resolved vbuffer/materials
	const ERDGUnorderedAccessViewFlags OutTargetFlags = GNaniteBarrierTest != 0 ? ERDGUnorderedAccessViewFlags::SkipBarrier : ERDGUnorderedAccessViewFlags::None;

	FRDGTextureUAVRef MaterialTextureArrayUAV = nullptr;
	if (Substrate::IsSubstrateEnabled())
	{
		MaterialTextureArrayUAV = GraphBuilder.CreateUAV(SceneRenderer.Scene->SubstrateSceneData.MaterialTextureArray, OutTargetFlags);
	}

	const bool bMaintainCompression = (GNaniteFastTileClear == 2) && RHISupportsRenderTargetWriteMask(GMaxRHIShaderPlatform);

	FRDGTextureUAVRef* OutTargets[MaxSimultaneousRenderTargets] =
	{
		&Result.OutTarget0,
		&Result.OutTarget1,
		&Result.OutTarget2,
		&Result.OutTarget3,
		&Result.OutTarget4,
		&Result.OutTarget5,
		&Result.OutTarget6,
		&Result.OutTarget7
	};

	for (uint32 TargetIndex = 0; TargetIndex < MaxSimultaneousRenderTargets; ++TargetIndex)
	{
		if (FRDGTexture* TargetTexture = BasePassRenderTargets.Output[TargetIndex].GetTexture())
		{
			if ((BoundTargetMask & (1u << TargetIndex)) == 0u)
			{
				// Change any target over to a dummy if not written by at least one shading command
				FRDGTextureDesc DummyDesc = FRDGTextureDesc::Create2D(
					FIntPoint(1u, 1u),
					PF_R32_UINT,
					FClearValueBinding::Transparent,
					TexCreate_ShaderResource | TexCreate_UAV
				);

				*OutTargets[TargetIndex] = GraphBuilder.CreateUAV(GraphBuilder.CreateTexture(DummyDesc, TEXT("Nanite.TargetDummy")), OutTargetFlags);
			}
			else if (bMaintainCompression)
			{
				*OutTargets[TargetIndex] = GraphBuilder.CreateUAV(FRDGTextureUAVDesc::CreateForMetaData(TargetTexture, ERDGTextureMetaDataAccess::PrimaryCompressed), OutTargetFlags);
			}
			else
			{
				*OutTargets[TargetIndex] = GraphBuilder.CreateUAV(TargetTexture, OutTargetFlags);
			}
		}
	}

	Result.OutTargets = MaterialTextureArrayUAV;

	return Result;
}

void DispatchBasePass(
	FRDGBuilder& GraphBuilder,
	FNaniteShadingCommands& ShadingCommands,
	const FSceneRenderer& SceneRenderer,
	const FSceneTextures& SceneTextures,
	const FRenderTargetBindingSlots& BasePassRenderTargets,
	const FDBufferTextures& DBufferTextures,
	const FScene& Scene,
	const FViewInfo& View,
	const uint32 ViewIndex,
	const FRasterResults& RasterResults
)
{
	checkSlow(DoesPlatformSupportNanite(GMaxRHIShaderPlatform));

	LLM_SCOPE_BYTAG(Nanite);
	RDG_EVENT_SCOPE(GraphBuilder, "Nanite::BasePass");
	SCOPED_NAMED_EVENT(DispatchBasePass, FColor::Emerald);

	ShadingCommands.SetupTask.Wait();

	const uint32 ShadingBinCount = ShadingCommands.NumCommands;
	if (ShadingBinCount == 0u)
	{
		return;
	}

	FShaderBundleRHIRef ShaderBundle = ShadingCommands.ShaderBundle;

	const bool bDrawSceneViewsInOneNanitePass = ShouldDrawSceneViewsInOneNanitePass(View);
	FIntRect ViewRect = bDrawSceneViewsInOneNanitePass ? View.GetFamilyViewRect() : View.ViewRect;

	const int32 ViewWidth = ViewRect.Max.X - ViewRect.Min.X;
	const int32 ViewHeight = ViewRect.Max.Y - ViewRect.Min.Y;
	const FIntPoint ViewSize = FIntPoint(ViewWidth, ViewHeight);

	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);

	FRDGTextureRef MaterialDepth = RasterResults.MaterialDepth ? RasterResults.MaterialDepth : SystemTextures.Black;
	FRDGTextureRef VisBuffer64 = RasterResults.VisBuffer64 ? RasterResults.VisBuffer64 : SystemTextures.Black;
	FRDGTextureRef DbgBuffer64 = RasterResults.DbgBuffer64 ? RasterResults.DbgBuffer64 : SystemTextures.Black;
	FRDGTextureRef DbgBuffer32 = RasterResults.DbgBuffer32 ? RasterResults.DbgBuffer32 : SystemTextures.Black;

	FRDGBufferRef VisibleClustersSWHW = RasterResults.VisibleClustersSWHW;

	const uint32 IndirectArgStride = sizeof(FUint32Vector4);

	FRDGBufferRef MultiViewIndices = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("Nanite.DummyMultiViewIndices"));
	FRDGBufferRef MultiViewRectScaleOffsets = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector4f), 1), TEXT("Nanite.DummyMultiViewRectScaleOffsets"));
	FRDGBufferRef ViewsBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector4f), 1), TEXT("Nanite.PackedViews"));

	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(MultiViewIndices), 0);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(MultiViewRectScaleOffsets), 0);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(ViewsBuffer), 0);

	const FNaniteVisibilityQuery* VisibilityQuery = RasterResults.VisibilityQuery;

	TStaticArray<FTextureRenderTargetBinding, MaxSimultaneousRenderTargets> BasePassTextures;

	// NOTE: Always use a GBuffer layout with velocity output (It won't be written to unless the material has WPO or IsUsingBasePassVelocity())
	uint32 BasePassTextureCount = SceneTextures.GetGBufferRenderTargets(BasePassTextures, GBL_ForceVelocity);

	// We don't want to have Substrate MRTs appended to the list, except for the top layer data
	if (Substrate::IsSubstrateEnabled() && SceneRenderer.Scene)
	{
		// Add another MRT for Substrate top layer information. We want to follow the usual clear process which can leverage fast clear.
		{
			BasePassTextures[BasePassTextureCount] = FTextureRenderTargetBinding(SceneRenderer.Scene->SubstrateSceneData.TopLayerTexture);
			BasePassTextureCount++;
		};
	}

	TArrayView<FTextureRenderTargetBinding> BasePassTexturesView = MakeArrayView(BasePassTextures.GetData(), BasePassTextureCount);

	// Render targets bindings should remain constant at this point.
	FRenderTargetBindingSlots BasePassBindings = GetRenderTargetBindings(ERenderTargetLoadAction::ELoad, BasePassTexturesView);
	BasePassBindings.DepthStencil = BasePassRenderTargets.DepthStencil;

	TArray<FRDGTextureRef, TInlineAllocator<MaxSimultaneousRenderTargets>> ClearTargetList;

	// Fast tile clear prior to fast clear eliminate
	const bool bFastTileClear = UseNaniteComputeMaterials() && GNaniteFastTileClear != 0 && RHISupportsRenderTargetWriteMask(GMaxRHIShaderPlatform);
	if (bFastTileClear)
	{
		for (uint32 TargetIndex = 0; TargetIndex < MaxSimultaneousRenderTargets; ++TargetIndex)
		{
			if (FRDGTexture* TargetTexture = BasePassRenderTargets.Output[TargetIndex].GetTexture())
			{
				if (!EnumHasAnyFlags(TargetTexture->Desc.Flags, TexCreate_DisableDCC))
				{
					// Skip any targets that do not explicitly disable DCC, as this clear would not work correctly for DCC
					ClearTargetList.Add(nullptr);
					continue;
				}

				if (EnumHasAnyFlags(TargetTexture->Desc.Flags, TexCreate_NoFastClear))
				{
					// Skip any targets that explicitly disable fast clear optimization
					ClearTargetList.Add(nullptr);
					continue;
				}

				if ((ShadingCommands.BoundTargetMask & (1u << TargetIndex)) == 0u)
				{
					// Skip any targets that are not written by at least one shading command
					ClearTargetList.Add(nullptr);
					continue;
				}

				ClearTargetList.Add(TargetTexture);
			}
		}
	}

	FShadeBinning Binning = ShadeBinning(GraphBuilder, Scene, View, ViewRect, ShadingCommands, RasterResults, ClearTargetList);

	FNaniteShadingPassParameters* ShadingPassParameters = GraphBuilder.AllocParameters<FNaniteShadingPassParameters>();
	*ShadingPassParameters = CreateNaniteShadingPassParams(
		GraphBuilder,
		SceneRenderer,
		SceneTextures,
		DBufferTextures,
		View,
		ViewRect,
		RasterResults,
		RasterResults.ShadingMask,
		VisBuffer64,
		DbgBuffer64,
		DbgBuffer32,
		VisibleClustersSWHW,
		MultiViewIndices,
		MultiViewRectScaleOffsets,
		ViewsBuffer,
		BasePassBindings,
		ShadingCommands.BoundTargetMask,
		Binning
	);

	const bool bSkipBarriers = GNaniteBarrierTest != 0;
	const bool bBundleShading = !!GRHISupportsShaderBundleDispatch && GNaniteBundleShading != 0 && ShaderBundle != nullptr;
	const bool bBundleEmulation = bBundleShading && GNaniteBundleEmulation != 0;

	auto ShadePassWork = []
	(
		FRDGParallelCommandListSet* ParallelCommandListSet,
		const FUint32Vector4& ViewRect,
		const uint32 ViewIndex,
		const FNaniteVisibilityQuery* VisibilityQuery,
		FNaniteShadingCommands& ShadingCommands,
		FShaderBundleRHIRef ShaderBundle,
		FNaniteShadingPassParameters* ShadingPassParameters,
		FRHIComputeCommandList& RHICmdList,
		const uint32 IndirectArgStride,
		const uint32 DataByteOffset,
		bool bSkipBarriers,
		bool bBundleShading,
		bool bBundleEmulation
	)
	{
		// This is processed within the RDG pass lambda, so the setup task should be complete by now.
		check(ShadingCommands.BuildCommandsTask.IsCompleted());

		if (!bBundleShading || bBundleEmulation)
		{
			ShadingPassParameters->MaterialIndirectArgs->MarkResourceAsUsed();
		}

		TArray<FRHIUnorderedAccessView*, TInlineAllocator<8>> OutputTargets;
		auto GetOutputTargetRHI = [](const FRDGTextureUAVRef OutputTarget)
		{
			FRHIUnorderedAccessView* OutputTargetRHI = nullptr;
			if (OutputTarget != nullptr)
			{
				OutputTarget->MarkResourceAsUsed();
				OutputTargetRHI = OutputTarget->GetRHI();
			}
			return OutputTargetRHI;
		};

		const FNaniteVisibilityResults* VisibilityResults = Nanite::GetVisibilityResults(VisibilityQuery);

		TSharedPtr<TBitArray<SceneRenderingBitArrayAllocator>> VisibilityData;
		if (VisibilityResults && VisibilityResults->IsShadingTestValid())
		{
			VisibilityData = MakeShared<TBitArray<SceneRenderingBitArrayAllocator>>(VisibilityResults->GetShadingBinVisibility());
		}

		OutputTargets.Add(GetOutputTargetRHI(ShadingPassParameters->OutTarget0));
		OutputTargets.Add(GetOutputTargetRHI(ShadingPassParameters->OutTarget1));
		OutputTargets.Add(GetOutputTargetRHI(ShadingPassParameters->OutTarget2));
		OutputTargets.Add(GetOutputTargetRHI(ShadingPassParameters->OutTarget3));
		OutputTargets.Add(GetOutputTargetRHI(ShadingPassParameters->OutTarget4));
		OutputTargets.Add(GetOutputTargetRHI(ShadingPassParameters->OutTarget5));
		OutputTargets.Add(GetOutputTargetRHI(ShadingPassParameters->OutTarget6));
		OutputTargets.Add(GetOutputTargetRHI(ShadingPassParameters->OutTarget7));

		FRHIUnorderedAccessView* OutputTargetsArray = GetOutputTargetRHI(ShadingPassParameters->OutTargets);
		FRHIBuffer* IndirectArgsBuffer = (!bBundleShading || bBundleEmulation) ? ShadingPassParameters->MaterialIndirectArgs->GetIndirectRHICallBuffer() : nullptr;

		if (ParallelCommandListSet)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ParallelRecordShadingCommands);

			// Distribute work evenly to the available task graph workers based on NumPassCommands.
			const int32 NumPassCommands = ShadingCommands.Commands.Num();
			const int32 NumThreads = FMath::Min<int32>(FTaskGraphInterface::Get().GetNumWorkerThreads(), ParallelCommandListSet->Width);
			const int32 NumTasks = FMath::Min<int32>(NumThreads, FMath::DivideAndRoundUp(NumPassCommands, ParallelCommandListSet->MinDrawsPerCommandList));
			const int32 NumCommandsPerTask = FMath::DivideAndRoundUp(NumPassCommands, NumTasks);

			const ENamedThreads::Type RenderThread = ENamedThreads::GetRenderThread();

			// Assume on demand shader creation is enabled for platforms supporting Nanite
			// otherwise there might be issues with PSO creation on a task which is not running on the RenderThread
			// So task prerequisites can be empty (MeshDrawCommands task has prereq on FMeshDrawCommandInitResourcesTask which calls LazilyInitShaders on all shader)
			ensure(FParallelMeshDrawCommandPass::IsOnDemandShaderCreationEnabled());
			FGraphEventArray EmptyPrereqs;

			for (int32 TaskIndex = 0; TaskIndex < NumTasks; TaskIndex++)
			{
				const int32 StartIndex = TaskIndex * NumCommandsPerTask;
				const int32 NumCommands = FMath::Min(NumCommandsPerTask, NumPassCommands - StartIndex);
				checkSlow(NumCommands > 0);

				FRHICommandList* CmdList = ParallelCommandListSet->NewParallelCommandList();

				FGraphEventRef AnyThreadCompletionEvent = TGraphTask<FRecordShadingCommandsAnyThreadTask>::CreateTask(&EmptyPrereqs, RenderThread).
					ConstructAndDispatchWhenReady(
						*CmdList,
						IndirectArgsBuffer,
						IndirectArgStride,
						DataByteOffset,
						VisibilityData,
						ShadingCommands,
						OutputTargets,
						OutputTargetsArray,
						ViewRect,
						ViewIndex,
						TaskIndex,
						NumTasks
					);

				ParallelCommandListSet->AddParallelCommandList(CmdList, AnyThreadCompletionEvent, NumCommands);
			}
		}
		else
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(RecordShadingCommands);

			FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();
			check(!BatchedParameters.HasParameters());

			if (bBundleShading)
			{
				auto RecordDispatches = [&](FRHICommandDispatchShaderBundle& Command)
				{
					Command.ShaderBundle		= ShaderBundle;
					Command.bEmulated			= bBundleEmulation;
					Command.RecordArgBufferSRV	= ShadingPassParameters->RecordArgBuffer->GetRHI();

					check(!bBundleEmulation || Command.RecordArgBufferSRV->GetBuffer() == IndirectArgsBuffer);

					Command.Dispatches.SetNum(ShaderBundle->NumRecords);

					const bool bParallel = true;
					if (bParallel)
					{
						std::atomic<uint32> PendingPSOs{ 0u };

						ParallelFor(ShadingCommands.Commands.Num(), [&](int32 CommandIndex)
						{
							FNaniteShadingCommand& ShadingCommand = ShadingCommands.Commands[CommandIndex];
							ShadingCommand.bVisible = !VisibilityData.IsValid() || VisibilityData->AccessCorrespondingBit(FRelativeBitReference(ShadingCommand.ShadingBin));

							if (ShadingCommand.bVisible && PrepareShadingCommand(ShadingCommand))
							{
								FRHIShaderBundleDispatch& Dispatch = Command.Dispatches[ShadingCommand.ShadingBin];

								Dispatch.RecordIndex = ShadingCommand.ShadingBin;
								RecordShadingParameters(Dispatch.Parameters, ShadingCommand, DataByteOffset, ViewRect, OutputTargets, OutputTargetsArray);
								Dispatch.Shader = ShadingCommand.Pipeline->ComputeShader;
								Dispatch.Constants = ShadingCommand.PassData;
								Dispatch.PipelineState = FindComputePipelineState(Dispatch.Shader);
								if (Dispatch.PipelineState != nullptr)
								{
									if (RHICmdList.Bypass())
									{
										Dispatch.RHIPipeline = ExecuteSetComputePipelineState(Dispatch.PipelineState);
									}
								}
								else
								{
									PendingPSOs.fetch_add(1u, std::memory_order_relaxed);
								}
							}
							else
							{
								// TODO: Optimization: Send partial dispatch lists, but for now we'll leave the record index invalid so bundle dispatch skips it
								Command.Dispatches[ShadingCommand.ShadingBin].RecordIndex = ~uint32(0u);
							}
						});

						// Resolve invalid pipeline states
						if (PendingPSOs.load(std::memory_order_relaxed) > 0)
						{
							for (FRHIShaderBundleDispatch& Dispatch : Command.Dispatches)
							{
								if (!Dispatch.IsValid() || Dispatch.PipelineState != nullptr)
								{
									continue;
								}

								// This cache lookup cannot be parallelized due to the possibility of a fence insertion into the command list during a miss.
								Dispatch.PipelineState = GetComputePipelineState(RHICmdList, Dispatch.Shader);
								if (RHICmdList.Bypass())
								{
									Dispatch.RHIPipeline = ExecuteSetComputePipelineState(Dispatch.PipelineState);
								}
							}
						}
					}
					else
					{
						for (int32 CommandIndex = 0; CommandIndex < ShadingCommands.Commands.Num(); ++CommandIndex)
						{
							FNaniteShadingCommand& ShadingCommand = ShadingCommands.Commands[CommandIndex];
							ShadingCommand.bVisible = !VisibilityData.IsValid() || VisibilityData->AccessCorrespondingBit(FRelativeBitReference(ShadingCommand.ShadingBin));

							if (ShadingCommand.bVisible && PrepareShadingCommand(ShadingCommand))
							{
								FRHIShaderBundleDispatch& Dispatch = Command.Dispatches[ShadingCommand.ShadingBin];

								Dispatch.RecordIndex = ShadingCommand.ShadingBin;
								RecordShadingParameters(Dispatch.Parameters, ShadingCommand, DataByteOffset, ViewRect, OutputTargets, OutputTargetsArray);

								Dispatch.Shader = ShadingCommand.Pipeline->ComputeShader;
								check(Dispatch.Shader);

								Dispatch.PipelineState = GetComputePipelineState(RHICmdList, Dispatch.Shader);
								check(Dispatch.PipelineState);
								if (RHICmdList.Bypass())
								{
									Dispatch.RHIPipeline = ExecuteSetComputePipelineState(Dispatch.PipelineState);
								}
							}
							else
							{
								// TODO: Allow for sending partial dispatch lists, but for now we'll leave the record index invalid so bundle dispatch skips it
								Command.Dispatches[ShadingCommand.ShadingBin].RecordIndex = ~uint32(0u);
							}
						}
					}
				};

				// Need to explicitly enqueue the RHI command so we can avoid an unnecessary copy of the dispatches array.
				// Because of this, we need to special case RHI bypass vs. threaded instead of calling RHICmdList.DispatchShaderBundle().
				if (RHICmdList.Bypass())
				{
					FRHICommandDispatchShaderBundle DispatchBundleCommand;
					RecordDispatches(DispatchBundleCommand);
					DispatchBundleCommand.Execute(RHICmdList);
				}
				else
				{
					FRHICommandDispatchShaderBundle& DispatchBundleCommand = *ALLOC_COMMAND_CL(RHICmdList, FRHICommandDispatchShaderBundle);
					RecordDispatches(DispatchBundleCommand);
				}
			}
			else // !bDispatchBundle
			{
				for (FNaniteShadingCommand& ShadingCommand : ShadingCommands.Commands)
				{
					ShadingCommand.bVisible = !VisibilityData.IsValid() || VisibilityData->AccessCorrespondingBit(FRelativeBitReference(ShadingCommand.ShadingBin));
					if (ShadingCommand.bVisible && PrepareShadingCommand(ShadingCommand))
					{
						FRHIBatchedShaderParameters ShadingParameters;
						RecordShadingParameters(ShadingParameters, ShadingCommand, DataByteOffset, ViewRect, OutputTargets, OutputTargetsArray);
						RecordShadingCommand(RHICmdList, IndirectArgsBuffer, IndirectArgStride, ShadingParameters, ShadingCommand);
					}
				}
			}
		}
	};

	const bool bParallelDispatch = !bBundleShading && GRHICommandList.UseParallelAlgorithms() && CVarParallelBasePassBuild.GetValueOnRenderThread() != 0 &&
								   FParallelMeshDrawCommandPass::IsOnDemandShaderCreationEnabled();
	if (bParallelDispatch)
	{
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("ShadeGBufferCS"),
			ShadingPassParameters,
			ERDGPassFlags::Compute,
			[&ShadePassWork, ShadingPassParameters, &ShadingCommands, IndirectArgStride, DataByteOffset = Binning.DataByteOffset, VisibilityQuery, &View, ViewRect, ViewIndex, bSkipBarriers]
			(const FRDGPass* RDGPass, FRHICommandListImmediate& RHICmdList)
			{
				FParallelCommandListBindings CmdListBindings(ShadingPassParameters);
				FRDGParallelCommandListSet ParallelCommandListSet(RDGPass, RHICmdList, GET_STATID(STAT_CLP_NaniteBasePass), View, CmdListBindings);
				ParallelCommandListSet.SetHighPriority();

				ShadePassWork(
					&ParallelCommandListSet,
					FUint32Vector4(
						(uint32)ViewRect.Min.X,
						(uint32)ViewRect.Min.Y,
						(uint32)ViewRect.Max.X,
						(uint32)ViewRect.Max.Y
					),
					ViewIndex,
					VisibilityQuery,
					ShadingCommands,
					FShaderBundleRHIRef(),
					ShadingPassParameters,
					RHICmdList,
					IndirectArgStride,
					DataByteOffset,
					bSkipBarriers,
					false /* bBundleShading   */,
					false /* bBundleEmulation */
				);
			});
	}
	else
	{
		if (bBundleShading)
		{
			ShadingPassParameters->RecordArgBuffer = GraphBuilder.CreateSRV(Binning.ShadingBinArgs);
			check(ShadingPassParameters->RecordArgBuffer != nullptr);
		}

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("ShadeGBufferCS"),
			ShadingPassParameters,
			ERDGPassFlags::Compute,
			[&ShadePassWork, ShadingPassParameters, &ShadingCommands, ShaderBundle, IndirectArgStride, DataByteOffset = Binning.DataByteOffset, VisibilityQuery, &View, ViewRect, ViewIndex, bSkipBarriers, bBundleShading, bBundleEmulation]
			(const FRDGPass* RDGPass, FRHIComputeCommandList& RHICmdList)
			{
				if (bBundleShading)
				{
					ShadingPassParameters->RecordArgBuffer->MarkResourceAsUsed();
				}

				ShadePassWork(
					nullptr,
					FUint32Vector4(
						(uint32)ViewRect.Min.X,
						(uint32)ViewRect.Min.Y,
						(uint32)ViewRect.Max.X,
						(uint32)ViewRect.Max.Y
					),
					ViewIndex,
					VisibilityQuery,
					ShadingCommands,
					ShaderBundle,
					ShadingPassParameters,
					RHICmdList,
					IndirectArgStride,
					DataByteOffset,
					bSkipBarriers,
					bBundleShading,
					bBundleEmulation
				);
			}
		);
	}

	ExtractShadingDebug(GraphBuilder, View, nullptr, Binning, ShadingBinCount);
}

FShadeBinning ShadeBinning(
	FRDGBuilder& GraphBuilder,
	const FScene& Scene,
	const FViewInfo& View,
	const FIntRect InViewRect,
	const FNaniteShadingCommands& ShadingCommands,
	const FRasterResults& RasterResults,
	const TConstArrayView<FRDGTextureRef> ClearTargets
)
{
	FShadeBinning Binning = {};

	LLM_SCOPE_BYTAG(Nanite);
	RDG_EVENT_SCOPE(GraphBuilder, "Nanite::ShadeBinning");

	const FSceneTexturesConfig& Config = View.GetSceneTexturesConfig();
	const EShaderPlatform ShaderPlatform = View.GetShaderPlatform();

	if (!ShadingCommands.NumCommands)
	{
		return Binning;
	}

	const FNaniteShadingCommands::FMetaBufferArray& MetaBufferData = ShadingCommands.MetaBufferData;

	TArray<FRDGTextureRef, TInlineAllocator<MaxSimultaneousRenderTargets>> ValidClearTargets;

	uint32 ValidWriteMask = 0x0u;
	if (ClearTargets.Num() > 0)
	{
		for (int32 TargetIndex = 0; TargetIndex < ClearTargets.Num(); ++TargetIndex)
		{
			if (ClearTargets[TargetIndex] != nullptr)
			{
				// Compute a mask containing only set bits for MRT targets that are suitable for meta data optimization.
				ValidWriteMask |= (1u << uint32(TargetIndex));
				ValidClearTargets.Add(ClearTargets[TargetIndex]);
			}
		}
	}

	const uint32 ShadingBinCount = ShadingCommands.MaxShadingBin + 1u;
	const uint32 ShadingBinCountPow2 = FMath::RoundUpToPowerOfTwo(ShadingBinCount);

	const bool bGatherStats = GNaniteShowStats != 0;

	const FUintVector4 ViewRect = FUintVector4(uint32(InViewRect.Min.X), uint32(InViewRect.Min.Y), uint32(InViewRect.Max.X), uint32(InViewRect.Max.Y));

	const uint32 PixelCount = InViewRect.Width() * InViewRect.Height();

	const int32 QuadWidth = FMath::DivideAndRoundUp(InViewRect.Width(), 2);
	const int32 QuadHeight = FMath::DivideAndRoundUp(InViewRect.Height(), 2);

	const FIntPoint GroupDim = GBinningTechnique == 0 ? FIntPoint(8u, 8u) : FIntPoint(32u, 32u);
	const FIntVector  QuadDispatchDim = FComputeShaderUtils::GetGroupCount(FIntPoint(QuadWidth, QuadHeight), GroupDim);
	const FIntVector   BinDispatchDim = FComputeShaderUtils::GetGroupCount(ShadingBinCount, 64u);

	const FUint32Vector2 DispatchOffsetTL = FUint32Vector2(InViewRect.Min.X, InViewRect.Min.Y);

	const uint32 NumBytes_Meta = sizeof(FNaniteShadingBinMeta) * ShadingBinCountPow2;
	const uint32 NumBytes_Data = PixelCount * 8;

	FRDGBufferRef ShadingBinMeta = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("Nanite.ShadingBinMeta"),
		sizeof(FNaniteShadingBinMeta),
		ShadingBinCountPow2,
		MetaBufferData.GetData(),
		sizeof(FNaniteShadingBinMeta) * MetaBufferData.Num()
	);

	Binning.DataByteOffset = NumBytes_Meta;
	Binning.ShadingBinData	= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateByteAddressDesc(NumBytes_Meta + NumBytes_Data), TEXT("Nanite.ShadingBinData"));

	AddCopyBufferPass(GraphBuilder, Binning.ShadingBinData, 0, ShadingBinMeta, 0, NumBytes_Meta);

	Binning.ShadingBinArgs   = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateRawIndirectDesc(sizeof(FUint32Vector4) * ShadingBinCountPow2), TEXT("Nanite.ShadingBinArgs"));
	Binning.ShadingBinStats  = bGatherStats ? GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FNaniteShadingBinStats), 1u), TEXT("Nanite.ShadingBinStats")) : nullptr;

	FRDGBufferUAVRef ShadingBinArgsUAV  = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Binning.ShadingBinArgs, PF_R32_UINT));
	FRDGBufferUAVRef ShadingBinDataUAV  = GraphBuilder.CreateUAV(Binning.ShadingBinData);
	FRDGBufferUAVRef ShadingBinStatsUAV = bGatherStats ? GraphBuilder.CreateUAV(Binning.ShadingBinStats) : nullptr;

	FRDGBufferRef ShadingBinScatterMetaBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FNaniteShadingBinScatterMeta), ShadingBinCountPow2), TEXT("Nanite.ShadingBinScatterMeta"));
	FRDGBufferUAVRef ShadingBinScatterMetaUAV = GraphBuilder.CreateUAV(ShadingBinScatterMetaBuffer);

	if (bGatherStats)
	{
		AddClearUAVPass(GraphBuilder, ShadingBinStatsUAV, 0);
	}

	const bool bOptimizeWriteMask = (ValidClearTargets.Num() > 0);

	const uint32 ShadingRateTileSizeBits = GetShadingRateTileSizeBits();
	const bool bVariableRateShading = (ShadingRateTileSizeBits != 0);

	const uint32 TargetAlignment =	bOptimizeWriteMask ? 8 :	// 8x8 for optimized write mask
									bVariableRateShading ? 4 :	// 4x4 for VRS
									2;							// 2x2 for just quad processing
	const uint32 TargetAlignmentMask = ~(TargetAlignment - 1u);

	const FUint32Vector2 AlignedDispatchOffsetTL = FUint32Vector2(InViewRect.Min.X & TargetAlignmentMask, InViewRect.Min.Y & TargetAlignmentMask);
	const FIntVector AlignedDispatchDim = FComputeShaderUtils::GetGroupCount(FIntPoint(InViewRect.Max.X - AlignedDispatchOffsetTL.X, InViewRect.Max.Y - AlignedDispatchOffsetTL.Y), GroupDim * 2);

	check(QuadDispatchDim.X == AlignedDispatchDim.X);
	check(QuadDispatchDim.Y == AlignedDispatchDim.Y);

	// Shading Bin Count
	{
		FShadingBinBuildCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FShadingBinBuildCS::FParameters>();
		PassParameters->ViewRect = ViewRect;
		PassParameters->ValidWriteMask = ValidWriteMask;
		PassParameters->DispatchOffsetTL = bOptimizeWriteMask ? AlignedDispatchOffsetTL : DispatchOffsetTL;
		PassParameters->ShadingBinCount = ShadingBinCount;
		PassParameters->ShadingBinDataByteOffset = Binning.DataByteOffset;
		PassParameters->ShadingRateTileSizeBits = GetShadingRateTileSizeBits();
		PassParameters->DummyZero = 0;
		PassParameters->ShadingRateImage = GetShadingRateImage(GraphBuilder, View);
		PassParameters->ShadingMaskSampler = TStaticSamplerState<SF_Point>::GetRHI();
		PassParameters->ShadingMask = RasterResults.ShadingMask;
		PassParameters->OutShadingBinData = ShadingBinDataUAV;
		PassParameters->OutShadingBinArgs = ShadingBinArgsUAV;

		FShadingBinBuildCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FShadingBinBuildCS::FBuildPassDim>(NANITE_SHADING_BIN_COUNT);
		PermutationVector.Set<FShadingBinBuildCS::FTechniqueDim>(FMath::Clamp<int32>(GBinningTechnique, 0, 1));
		PermutationVector.Set<FShadingBinBuildCS::FGatherStatsDim>(bGatherStats);
		PermutationVector.Set<FShadingBinBuildCS::FVariableRateDim>(bVariableRateShading);
		PermutationVector.Set<FShadingBinBuildCS::FOptimizeWriteMaskDim>(bOptimizeWriteMask);
		PermutationVector.Set<FShadingBinBuildCS::FNumExports>(FMath::Max(1, ValidClearTargets.Num()));
		auto ComputeShader = View.ShaderMap->GetShader<FShadingBinBuildCS>(PermutationVector);

		if (bOptimizeWriteMask)
		{
			for (int32 TargetIndex = 0; TargetIndex < ValidClearTargets.Num(); ++TargetIndex)
			{
				PassParameters->OutCMaskBuffer[TargetIndex] = GraphBuilder.CreateUAV(FRDGTextureUAVDesc::CreateForMetaData(ValidClearTargets[TargetIndex], ERDGTextureMetaDataAccess::CMask));
			}

			const bool bWriteSubTiles = GNaniteFastTileClearSubTiles != 0u;
	
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ShadingCount"),
				PassParameters,
				ERDGPassFlags::Compute,
				[AlignedDispatchDim, ComputeShader, PassParameters, TargetCount = ValidClearTargets.Num(), bWriteSubTiles](FRHIComputeCommandList& RHICmdList)
				{
					void* PlatformDataPtr = nullptr;
					uint32 PlatformDataSize = 0;

					// Note: Assumes all targets match in resolution (which they should)
					if (PassParameters->OutCMaskBuffer[0] != nullptr)
					{
						FRHITexture* TargetTextureRHI = PassParameters->OutCMaskBuffer[0]->GetParentRHI();

						// Retrieve the platform specific data that the decode shader needs.
						TargetTextureRHI->GetWriteMaskProperties(PlatformDataPtr, PlatformDataSize);
						check(PlatformDataSize > 0);

						if (PlatformDataPtr == nullptr)
						{
							// If the returned pointer was null, the platform RHI wants us to allocate the memory instead.
							PlatformDataPtr = alloca(PlatformDataSize);
							TargetTextureRHI->GetWriteMaskProperties(PlatformDataPtr, PlatformDataSize);
						}
					}

					check(PlatformDataPtr != nullptr && PlatformDataSize > 0);

					bool bSubTileMatch = bWriteSubTiles;

					// If we want to write 4x4 subtiles, ensure platform specific data matches across all MRTs (tile modes, etc..)
					if (bWriteSubTiles)
					{
						TArray<uint8, TInlineAllocator<8>> Scratch;

						for (int32 TargetIndex = 1; TargetIndex < TargetCount; ++TargetIndex)
						{
							void* TestPlatformDataPtr = nullptr;
							uint32 TestPlatformDataSize = 0;

							// We want to enforce that the platform metadata is bit exact across all MRTs
							if (PassParameters->OutCMaskBuffer[TargetIndex] != nullptr)
							{
								FRHITexture* TargetTextureRHI = PassParameters->OutCMaskBuffer[TargetIndex]->GetParentRHI();

								TargetTextureRHI->GetWriteMaskProperties(TestPlatformDataPtr, TestPlatformDataSize);
								check(TestPlatformDataSize > 0);

								if (TestPlatformDataPtr == nullptr)
								{
									// If the returned pointer was null, the platform RHI wants us to allocate the memory instead.
									Scratch.SetNumZeroed(TestPlatformDataSize);
									TestPlatformDataPtr = Scratch.GetData();
									TargetTextureRHI->GetWriteMaskProperties(TestPlatformDataPtr, TestPlatformDataSize);
								}

								check(TestPlatformDataPtr != nullptr && TestPlatformDataSize == PlatformDataSize);

								if (FMemory::Memcmp(PlatformDataPtr, TestPlatformDataPtr, PlatformDataSize) != 0)
								{
									bSubTileMatch = false;
									break;
								}
							}
						}
					}

					SetComputePipelineState(RHICmdList, ComputeShader.GetComputeShader());
					SetShaderParametersMixedCS(RHICmdList, ComputeShader, *PassParameters, PlatformDataPtr, PlatformDataSize, bSubTileMatch);

					RHICmdList.DispatchComputeShader(AlignedDispatchDim.X, AlignedDispatchDim.Y, AlignedDispatchDim.Z);
				}
			);
		}
		else
		{
			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ShadingCount"), ComputeShader, PassParameters, AlignedDispatchDim);
		}
	}

	// Shading Bin Reserve
	{
		FRDGBufferRef ShadingBinAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("Nanite.ShadingBinAllocator"));
		FRDGBufferUAVRef ShadingBinAllocatorUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ShadingBinAllocator, PF_R32_UINT));
		AddClearUAVPass(GraphBuilder, ShadingBinAllocatorUAV, 0);

		FShadingBinReserveCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FShadingBinReserveCS::FParameters>();
		PassParameters->ShadingBinCount = ShadingBinCount;
		PassParameters->ShadingBinDataByteOffset = Binning.DataByteOffset;
		PassParameters->OutShadingBinStats = ShadingBinStatsUAV;
		PassParameters->OutShadingBinData = ShadingBinDataUAV;
		PassParameters->OutShadingBinAllocator = ShadingBinAllocatorUAV;
		PassParameters->OutShadingBinArgs = ShadingBinArgsUAV;
		PassParameters->OutShadingBinStats = ShadingBinStatsUAV;
		PassParameters->OutShadingBinScatterMeta = ShadingBinScatterMetaUAV;

		FShadingBinReserveCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FShadingBinReserveCS::FGatherStatsDim>(bGatherStats);
		auto ComputeShader = View.ShaderMap->GetShader<FShadingBinReserveCS>(PermutationVector);

		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ShadingReserve"), ComputeShader, PassParameters, BinDispatchDim);
	}

	// Shading Bin Scatter
	{
		FShadingBinBuildCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FShadingBinBuildCS::FParameters>();
		PassParameters->ViewRect = ViewRect;
		PassParameters->DispatchOffsetTL = AlignedDispatchOffsetTL;
		PassParameters->ShadingBinCount = ShadingBinCount;
		PassParameters->ShadingBinDataByteOffset = Binning.DataByteOffset;
		PassParameters->ShadingRateTileSizeBits = GetShadingRateTileSizeBits();
		PassParameters->DummyZero = 0;
		PassParameters->ShadingRateImage = GetShadingRateImage(GraphBuilder, View);
		PassParameters->ShadingMaskSampler = TStaticSamplerState<SF_Point>::GetRHI();
		PassParameters->ShadingMask = RasterResults.ShadingMask;
		PassParameters->OutShadingBinStats = ShadingBinStatsUAV;
		PassParameters->OutShadingBinData = ShadingBinDataUAV;
		PassParameters->OutShadingBinArgs = nullptr;
		PassParameters->OutShadingBinScatterMeta = ShadingBinScatterMetaUAV;

		FShadingBinBuildCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FShadingBinBuildCS::FBuildPassDim>(NANITE_SHADING_BIN_SCATTER);
		PermutationVector.Set<FShadingBinBuildCS::FTechniqueDim>(FMath::Clamp<int32>(GBinningTechnique, 0, 1));
		PermutationVector.Set<FShadingBinBuildCS::FGatherStatsDim>(bGatherStats);
		PermutationVector.Set<FShadingBinBuildCS::FVariableRateDim>(bVariableRateShading);
		PermutationVector.Set<FShadingBinBuildCS::FOptimizeWriteMaskDim>(false);
		PermutationVector.Set<FShadingBinBuildCS::FNumExports>(1);
		auto ComputeShader = View.ShaderMap->GetShader<FShadingBinBuildCS>(PermutationVector);

		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ShadingScatter"), ComputeShader, PassParameters, AlignedDispatchDim);
	}

	// Shading Bin Validate
	if (GNaniteValidateShadeBinning)
	{
		FShadingBinValidateCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FShadingBinValidateCS::FParameters>();
		PassParameters->ShadingBinCount = ShadingBinCount;
		PassParameters->OutShadingBinData = ShadingBinDataUAV;

		auto ComputeShader = View.ShaderMap->GetShader<FShadingBinValidateCS>();
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ShadingValidate"), ERDGPassFlags::Compute | ERDGPassFlags::NeverCull, ComputeShader, PassParameters, BinDispatchDim);
	}

	const FNaniteVisualizationData& VisualizationData = GetNaniteVisualizationData();
	if (bOptimizeWriteMask && VisualizationData.IsActive())
	{
		auto ComputeShader = View.ShaderMap->GetShader<FVisualizeClearTilesCS>();

		FRDGTextureDesc VisClearMaskDesc = FRDGTextureDesc::Create2D(
			FIntPoint(InViewRect.Width(), InViewRect.Height()),
			PF_R32_UINT,
			FClearValueBinding::Transparent,
			TexCreate_ShaderResource | TexCreate_UAV
		);

		Binning.FastClearVisualize = GraphBuilder.CreateTexture(VisClearMaskDesc, TEXT("Nanite.VisClearMask"));
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(Binning.FastClearVisualize), FUintVector4(ForceInitToZero));

		for (int32 TargetIndex = 0; TargetIndex < ValidClearTargets.Num(); ++TargetIndex)
		{
			if (TargetIndex != GNaniteFastTileVis && GNaniteFastTileVis != INDEX_NONE)
			{
				continue;
			}

			FVisualizeClearTilesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVisualizeClearTilesCS::FParameters>();
			PassParameters->ViewRect		= ViewRect;
			PassParameters->OutCMaskBuffer	= GraphBuilder.CreateUAV(FRDGTextureUAVDesc::CreateForMetaData(ValidClearTargets[TargetIndex], ERDGTextureMetaDataAccess::CMask));
			PassParameters->OutVisualized	= GraphBuilder.CreateUAV(Binning.FastClearVisualize);

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("VisualizeFastClear"),
				PassParameters,
				ERDGPassFlags::Compute,
				[InViewRect, ComputeShader, PassParameters](FRHIComputeCommandList& RHICmdList)
				{
					void* PlatformDataPtr = nullptr;
					uint32 PlatformDataSize = 0;

					if (PassParameters->OutCMaskBuffer != nullptr)
					{
						FRHITexture* TargetTextureRHI = PassParameters->OutCMaskBuffer->GetParentRHI();

						// Retrieve the platform specific data that the decode shader needs.
						TargetTextureRHI->GetWriteMaskProperties(PlatformDataPtr, PlatformDataSize);
						check(PlatformDataSize > 0);

						if (PlatformDataPtr == nullptr)
						{
							// If the returned pointer was null, the platform RHI wants us to allocate the memory instead.
							PlatformDataPtr = alloca(PlatformDataSize);
							TargetTextureRHI->GetWriteMaskProperties(PlatformDataPtr, PlatformDataSize);
						}
					}

					SetComputePipelineState(RHICmdList, ComputeShader.GetComputeShader());
					SetShaderParametersMixedCS(RHICmdList, ComputeShader, *PassParameters, PlatformDataPtr, PlatformDataSize);

					const FIntVector DispatchDim = FComputeShaderUtils::GetGroupCount(FIntPoint(InViewRect.Width(), InViewRect.Height()), FIntPoint(8u, 8u));
					RHICmdList.DispatchComputeShader(DispatchDim.X, DispatchDim.Y, DispatchDim.Z);
				}
			);
		}
	}

	return Binning;
}

void CollectShadingPSOInitializers(
	const FSceneTexturesConfig& SceneTexturesConfig,
	const FPSOPrecacheVertexFactoryData& VertexFactoryData,
	const FMaterial& Material,
	const FPSOPrecacheParams& PreCacheParams,
	ERHIFeatureLevel::Type FeatureLevel,
	EShaderPlatform ShaderPlatform,
	int32 PSOCollectorIndex,
	TArray<FPSOPrecacheData>& PSOInitializers)
{
	// Base Pass
	{
		TArray<ELightMapPolicyType, TInlineAllocator<2>> UniformLightMapPolicyTypes = FBasePassMeshProcessor::GetUniformLightMapPolicyTypeForPSOCollection(FeatureLevel, Material);

		auto CollectBasePass = [&](bool bRenderSkyLight)
		{
			for (ELightMapPolicyType UniformLightMapPolicyType : UniformLightMapPolicyTypes)
			{
				TShaderRef<TBasePassComputeShaderPolicyParamType<FUniformLightMapPolicy>> BasePassComputeShader;

				bool bShadersValid = GetBasePassShader<FUniformLightMapPolicy>(
					Material,
					VertexFactoryData.VertexFactoryType,
					FUniformLightMapPolicy(UniformLightMapPolicyType),
					FeatureLevel,
					bRenderSkyLight,
					&BasePassComputeShader
				);

				if (!bShadersValid)
				{
					continue;
				}

				FPSOPrecacheData ComputePSOPrecacheData;
				ComputePSOPrecacheData.Type = FPSOPrecacheData::EType::Compute;
				ComputePSOPrecacheData.ComputeShader = BasePassComputeShader.GetComputeShader();
			#if PSO_PRECACHING_VALIDATE
				ComputePSOPrecacheData.PSOCollectorIndex = PSOCollectorIndex;
				ComputePSOPrecacheData.VertexFactoryType = VertexFactoryData.VertexFactoryType;
			#endif
				PSOInitializers.Add(ComputePSOPrecacheData);

			#if PSO_PRECACHING_VALIDATE
				if (PSOCollectorStats::IsFullPrecachingValidationEnabled())
				{
					PSOCollectorStats::GetShadersOnlyPSOPrecacheStatsCollector().AddStateToCache(*ComputePSOPrecacheData.ComputeShader, PSOCollectorStats::GetPSOPrecacheHash, &Material, (uint32)EMeshPass::BasePass, VertexFactoryData.VertexFactoryType);
					PSOCollectorStats::GetMinimalPSOPrecacheStatsCollector().AddStateToCache(*ComputePSOPrecacheData.ComputeShader, PSOCollectorStats::GetPSOPrecacheHash, &Material, (uint32)EMeshPass::BasePass, VertexFactoryData.VertexFactoryType);
				}
			#endif
			}
		};

		CollectBasePass(true);
		CollectBasePass(false);
	}
}

} // Nanite

FNaniteRasterPipeline FNaniteRasterPipeline::GetFixedFunctionPipeline(bool bIsTwoSided, bool bSplineMesh)
{
	FNaniteRasterPipeline Pipeline;
	Pipeline.RasterMaterial = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
	Pipeline.bIsTwoSided = bIsTwoSided;
	Pipeline.bSplineMesh = bSplineMesh;
	Pipeline.bPerPixelEval = false;
	Pipeline.bWPODisableDistance = false;
	return Pipeline;
}

FNaniteRasterPipelines::FNaniteRasterPipelines()
{
	PipelineBins.Reserve(256);
	PerPixelEvalPipelineBins.Reserve(256);
	PipelineMap.Reserve(256);

	AllocateFixedFunctionBins();
}

FNaniteRasterPipelines::~FNaniteRasterPipelines()
{
	ReleaseFixedFunctionBins();

	PipelineBins.Reset();
	PerPixelEvalPipelineBins.Reset();
	PipelineMap.Empty();
}

void FNaniteRasterPipelines::AllocateFixedFunctionBins()
{
	check(FixedFunctionBins.Num() == 0);

	FFixedFunctionBin Bin00;
	{
		Bin00.TwoSided = false;
		Bin00.Spline   = false;

		FNaniteRasterPipeline Pipeline00 = FNaniteRasterPipeline::GetFixedFunctionPipeline(Bin00.TwoSided, Bin00.Spline);
		Bin00.RasterBin = Register(Pipeline00);
		check(Bin00.RasterBin.BinIndex == NANITE_FIXED_FUNCTION_BIN);
	}

	FFixedFunctionBin Bin01;
	{
		Bin01.TwoSided = true;
		Bin01.Spline   = false;

		FNaniteRasterPipeline Pipeline01 = FNaniteRasterPipeline::GetFixedFunctionPipeline(Bin01.TwoSided, Bin01.Spline);
		Bin01.RasterBin = Register(Pipeline01);
		check(Bin01.RasterBin.BinIndex == NANITE_FIXED_FUNCTION_BIN_TWOSIDED);
	}

	FFixedFunctionBin Bin10;
	{
		Bin10.TwoSided = false;
		Bin10.Spline   = true;

		FNaniteRasterPipeline Pipeline10 = FNaniteRasterPipeline::GetFixedFunctionPipeline(Bin10.TwoSided, Bin10.Spline);
		Bin10.RasterBin = Register(Pipeline10);
		check(Bin10.RasterBin.BinIndex == NANITE_FIXED_FUNCTION_BIN_SPLINE);
	}

	FFixedFunctionBin Bin11;
	{
		Bin11.TwoSided = true;
		Bin11.Spline   = true;

		FNaniteRasterPipeline Pipeline11 = FNaniteRasterPipeline::GetFixedFunctionPipeline(Bin11.TwoSided, Bin11.Spline);
		Bin11.RasterBin = Register(Pipeline11);
		check(Bin11.RasterBin.BinIndex == (NANITE_FIXED_FUNCTION_BIN_SPLINE | NANITE_FIXED_FUNCTION_BIN_TWOSIDED));
	}

	FixedFunctionBins.Emplace(Bin00);
	FixedFunctionBins.Emplace(Bin01);
	FixedFunctionBins.Emplace(Bin10);
	FixedFunctionBins.Emplace(Bin11);
}

void FNaniteRasterPipelines::ReleaseFixedFunctionBins()
{
	for (const FFixedFunctionBin& FixedFunctionBin : FixedFunctionBins)
	{
		Unregister(FixedFunctionBin.RasterBin);
	}

	FixedFunctionBins.Reset();
}

void FNaniteRasterPipelines::ReloadFixedFunctionBins()
{
	for (const FFixedFunctionBin& FixedFunctionBin : FixedFunctionBins)
	{
		FNaniteRasterPipeline Pipeline = FNaniteRasterPipeline::GetFixedFunctionPipeline(FixedFunctionBin.TwoSided, FixedFunctionBin.Spline);
		FNaniteRasterEntry* RasterEntry = PipelineMap.Find(Pipeline);
		check(RasterEntry != nullptr);
		RasterEntry->RasterPipeline = Pipeline;
	}

	// Reset the entire raster setup cache
	for (const auto& Pair : PipelineMap)
	{
		Pair.Value.CacheMap.Reset();
	}
}

uint16 FNaniteRasterPipelines::AllocateBin(bool bPerPixelEval)
{
	TBitArray<>& BinUsageMask = bPerPixelEval ? PerPixelEvalPipelineBins : PipelineBins;
	int32 BinIndex = BinUsageMask.FindAndSetFirstZeroBit();
	if (BinIndex == INDEX_NONE)
	{
		BinIndex = BinUsageMask.Add(true);
	}

	check(int32(uint16(BinIndex)) == BinIndex && PipelineBins.Num() + PerPixelEvalPipelineBins.Num() <= int32(MAX_uint16));
	return bPerPixelEval ? FNaniteRasterBinIndexTranslator::RevertBinIndex(BinIndex) : uint16(BinIndex);
}

void FNaniteRasterPipelines::ReleaseBin(uint16 BinIndex)
{
	check(IsBinAllocated(BinIndex));
	if (BinIndex < PipelineBins.Num())
	{
		PipelineBins[BinIndex] = false;
	}
	else
	{
		PerPixelEvalPipelineBins[FNaniteRasterBinIndexTranslator::RevertBinIndex(BinIndex)] = false;
	}
}

bool FNaniteRasterPipelines::IsBinAllocated(uint16 BinIndex) const
{
	return BinIndex < PipelineBins.Num() ? PipelineBins[BinIndex] : PerPixelEvalPipelineBins[FNaniteRasterBinIndexTranslator::RevertBinIndex(BinIndex)];
}

uint32 FNaniteRasterPipelines::GetRegularBinCount() const
{
	return PipelineBins.FindLast(true) + 1;
}

uint32 FNaniteRasterPipelines::GetBinCount() const
{
	return GetRegularBinCount() + PerPixelEvalPipelineBins.FindLast(true) + 1;
}

FNaniteRasterBin FNaniteRasterPipelines::Register(const FNaniteRasterPipeline& InRasterPipeline)
{
	FNaniteRasterBin RasterBin;

	const FRasterHash RasterPipelineHash = PipelineMap.ComputeHash(InRasterPipeline);
	FRasterId RasterBinId = PipelineMap.FindOrAddIdByHash(RasterPipelineHash, InRasterPipeline, FNaniteRasterEntry());
	RasterBin.BinId = RasterBinId.GetIndex();

	FNaniteRasterEntry& RasterEntry = PipelineMap.GetByElementId(RasterBinId).Value;
	if (RasterEntry.ReferenceCount == 0)
	{
		// First reference
		RasterEntry.RasterPipeline = InRasterPipeline;
		RasterEntry.BinIndex = AllocateBin(InRasterPipeline.bPerPixelEval);
		RasterEntry.bForceDisableWPO = InRasterPipeline.bForceDisableWPO;
	}

	++RasterEntry.ReferenceCount;

	RasterBin.BinIndex = RasterEntry.BinIndex;
	return RasterBin;
}

void FNaniteRasterPipelines::Unregister(const FNaniteRasterBin& InRasterBin)
{
	FRasterId RasterBinId(InRasterBin.BinId);
	check(RasterBinId.IsValid());

	FNaniteRasterEntry& RasterEntry = PipelineMap.GetByElementId(RasterBinId).Value;

	check(RasterEntry.ReferenceCount > 0);
	--RasterEntry.ReferenceCount;
	if (RasterEntry.ReferenceCount == 0)
	{
		checkf(!ShouldBinRenderInCustomPass(InRasterBin.BinIndex), TEXT("A raster bin has dangling references to Custom Pass on final release."));
		ReleaseBin(RasterEntry.BinIndex);
		PipelineMap.RemoveByElementId(RasterBinId);
	}
}

void FNaniteRasterPipelines::RegisterBinForCustomPass(uint16 BinIndex)
{
	check(IsBinAllocated(BinIndex));

	const bool bPerPixelEval = BinIndex >= PipelineBins.Num();
	TArray<uint32>& RefCounts = bPerPixelEval ? PerPixelEvalCustomPassRefCounts : CustomPassRefCounts;
	const uint16 ArrayIndex = bPerPixelEval ? FNaniteRasterBinIndexTranslator::RevertBinIndex(BinIndex) : BinIndex;

	if (RefCounts.Num() <= ArrayIndex)
	{
		RefCounts.AddZeroed(ArrayIndex - RefCounts.Num() + 1);
	}
	RefCounts[ArrayIndex]++;
}

void FNaniteRasterPipelines::UnregisterBinForCustomPass(uint16 BinIndex)
{
	check(IsBinAllocated(BinIndex));

	const bool bPerPixelEval = BinIndex >= PipelineBins.Num();
	TArray<uint32>& RefCounts = bPerPixelEval ? PerPixelEvalCustomPassRefCounts : CustomPassRefCounts;
	const uint16 ArrayIndex = bPerPixelEval ? FNaniteRasterBinIndexTranslator::RevertBinIndex(BinIndex) : BinIndex;

	checkf(RefCounts.IsValidIndex(ArrayIndex), TEXT("Attempting to unregister a bin that was never registered for Custom Pass"));
	checkf(RefCounts[ArrayIndex] > 0, TEXT("Mismatched calls to RegisterBinForCustomPass/UnregisterBinForCustomPass"));

	RefCounts[ArrayIndex]--;
}

bool FNaniteRasterPipelines::ShouldBinRenderInCustomPass(uint16 BinIndex) const
{
	check(IsBinAllocated(BinIndex));

	const bool bPerPixelEval = BinIndex >= PipelineBins.Num();
	const TArray<uint32>& RefCounts = bPerPixelEval ? PerPixelEvalCustomPassRefCounts : CustomPassRefCounts;
	const uint16 ArrayIndex = bPerPixelEval ? FNaniteRasterBinIndexTranslator::RevertBinIndex(BinIndex) : BinIndex;

	return RefCounts.IsValidIndex(ArrayIndex) ? RefCounts[ArrayIndex] > 0 : false;
}

FNaniteShadingPipelines::FNaniteShadingPipelines()
{
	PipelineBins.Reserve(256);
	PipelineMap.Reserve(256);
}

FNaniteShadingPipelines::~FNaniteShadingPipelines()
{
	PipelineBins.Reset();
	PipelineMap.Empty();
}

uint16 FNaniteShadingPipelines::AllocateBin()
{
	TBitArray<>& BinUsageMask = PipelineBins;
	int32 BinIndex = BinUsageMask.FindAndSetFirstZeroBit();
	if (BinIndex == INDEX_NONE)
	{
		BinIndex = BinUsageMask.Add(true);
	}

	check(int32(uint16(BinIndex)) == BinIndex && PipelineBins.Num() <= int32(MAX_uint16));
	return uint16(BinIndex);
}

void FNaniteShadingPipelines::ReleaseBin(uint16 BinIndex)
{
	check(IsBinAllocated(BinIndex));
	if (BinIndex < PipelineBins.Num())
	{
		PipelineBins[BinIndex] = false;
	}
}

bool FNaniteShadingPipelines::IsBinAllocated(uint16 BinIndex) const
{
	return BinIndex < PipelineBins.Num() ? PipelineBins[BinIndex] : false;
}

uint32 FNaniteShadingPipelines::GetBinCount() const
{
	return PipelineBins.FindLast(true) + 1;
}

FNaniteShadingBin FNaniteShadingPipelines::Register(const FNaniteShadingPipeline& InShadingPipeline)
{
	FNaniteShadingBin ShadingBin;

	const FShadingHash ShadingPipelineHash = PipelineMap.ComputeHash(InShadingPipeline);
	FShadingId ShadingBinId = PipelineMap.FindOrAddIdByHash(ShadingPipelineHash, InShadingPipeline, FNaniteShadingEntry());
	ShadingBin.BinId = ShadingBinId.GetIndex();

	FNaniteShadingEntry& ShadingEntry = PipelineMap.GetByElementId(ShadingBinId).Value;
	if (ShadingEntry.ReferenceCount == 0)
	{
		// First reference
		ShadingEntry.ShadingPipeline = MakeShared<FNaniteShadingPipeline>(InShadingPipeline);
		ShadingEntry.BinIndex = AllocateBin();
	}

	++ShadingEntry.ReferenceCount;

	ShadingBin.BinIndex = ShadingEntry.BinIndex;
	return ShadingBin;
}

void FNaniteShadingPipelines::Unregister(const FNaniteShadingBin& InShadingBin)
{
	FShadingId ShadingBinId(InShadingBin.BinId);
	check(ShadingBinId.IsValid());

	FNaniteShadingEntry& ShadingEntry = PipelineMap.GetByElementId(ShadingBinId).Value;

	check(ShadingEntry.ReferenceCount > 0);
	--ShadingEntry.ReferenceCount;
	if (ShadingEntry.ReferenceCount == 0)
	{
		ReleaseBin(ShadingEntry.BinIndex);
		PipelineMap.RemoveByElementId(ShadingBinId);
	}
}

#if 0
void DispatchLumenMeshCapturePass(
	FRDGBuilder& GraphBuilder,
	FScene& Scene,
	FViewInfo* SharedView,
	TArrayView<const FCardPageRenderData> CardPagesToRender,
	const FRasterResults& RasterResults,
	const FRasterContext& RasterContext,
	FLumenCardPassUniformParameters* PassUniformParameters,
	FRDGBufferSRVRef RectMinMaxBufferSRV,
	uint32 NumRects,
	FIntPoint ViewportSize,
	FRDGTextureRef AlbedoAtlasTexture,
	FRDGTextureRef NormalAtlasTexture,
	FRDGTextureRef EmissiveAtlasTexture,
	FRDGTextureRef DepthAtlasTexture
)
{
	checkSlow(DoesPlatformSupportNanite(GMaxRHIShaderPlatform));
	checkSlow(DoesPlatformSupportLumenGI(GMaxRHIShaderPlatform));

	LLM_SCOPE_BYTAG(Nanite);
	RDG_EVENT_SCOPE(GraphBuilder, "Nanite::LumenMeshCapturePass");
	TRACE_CPUPROFILER_EVENT_SCOPE(Nanite_LumenMeshCapturePass);

	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);


}
#endif
