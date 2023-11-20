// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteMaterials.h"
#include "Async/ParallelFor.h"
#include "Materials/Material.h"
#include "Materials/MaterialRenderProxy.h"
#include "NaniteDrawList.h"
#include "NaniteSceneProxy.h"
#include "NaniteVertexFactory.h"
#include "NaniteVisualizationData.h"
#include "NaniteRayTracing.h"
#include "Rendering/NaniteResources.h"
#include "Rendering/NaniteStreamingManager.h"
#include "RHI.h"
#include "SceneUtils.h"
#include "ScenePrivate.h"
#include "ScreenPass.h"
#include "GPUScene.h"
#include "ClearQuad.h"
#include "RendererModule.h"
#include "PixelShaderUtils.h"
#include "Lumen/LumenSceneCardCapture.h"
#include "Strata/Strata.h"
#include "SystemTextures.h"
#include "BasePassRendering.h"
#include "VariableRateShadingImageManager.h"
#include "Lumen/Lumen.h"
#include "ComponentRecreateRenderStateContext.h"

DECLARE_CYCLE_STAT(TEXT("NaniteBasePass"), STAT_CLP_NaniteBasePass, STATGROUP_ParallelCommandListMarkers);

BEGIN_SHADER_PARAMETER_STRUCT(FDummyDepthDecompressParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, SceneDepth)
END_SHADER_PARAMETER_STRUCT()

// TODO: Heavily work in progress / experimental - do not use!
static int32 GNaniteFastTileClear = 1;
static FAutoConsoleVariableRef CVarNaniteFastTileClear(
	TEXT("r.Nanite.FastTileClear"),
	GNaniteFastTileClear,
	TEXT("Whether to enable Nanite fast tile clearing"),
	ECVF_RenderThreadSafe
);

// TODO: Heavily work in progress / experimental - do not use!
static int32 GNaniteComputeMaterials = 0;
static FAutoConsoleVariableRef CVarNaniteComputeMaterials(
	TEXT("r.Nanite.ComputeMaterials"),
	GNaniteComputeMaterials,
	TEXT("Whether to enable Nanite material compute shading"),
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

// TODO: Heavily work in progress / experimental - do not use!
static int32 GBinningTechnique = 2;
static FAutoConsoleVariableRef CVarNaniteBinningTechnique(
	TEXT("r.Nanite.BinningTechnique"),
	GBinningTechnique,
	TEXT(""),
	ECVF_RenderThreadSafe
);

// TODO: Heavily work in progress / experimental - do not use!
static int32 GNaniteSoftwareVRS = 1;
static FAutoConsoleVariableRef CVarNaniteSoftwareVRS(
	TEXT("r.Nanite.SoftwareVRS"),
	GNaniteSoftwareVRS,
	TEXT("Whether to enable Nanite software variable rate shading in compute."),
	ECVF_RenderThreadSafe
);

static int32 GNaniteBarrierTest = 1;
static FAutoConsoleVariableRef CVarNaniteBarrierTest(
	TEXT("r.Nanite.BarrierTest"),
	GNaniteBarrierTest,
	TEXT(""),
	ECVF_RenderThreadSafe
);

// TODO: Heavily work in progress / experimental - do not use!
static int32 GNaniteQuadBinning = 1;
static FAutoConsoleVariableRef CVarNaniteQuadBinning(
	TEXT("r.Nanite.QuadBinning"),
	GNaniteQuadBinning,
	TEXT(""),
	ECVF_RenderThreadSafe
);

static int32 GNaniteMaterialVisibility = 0;
static FAutoConsoleVariableRef CVarNaniteMaterialVisibility(
	TEXT("r.Nanite.MaterialVisibility"),
	GNaniteMaterialVisibility,
	TEXT("Whether to enable Nanite material visibility tests"),
	ECVF_ReadOnly
);

static int32 GNaniteMaterialVisibilityAsync = 1;
static FAutoConsoleVariableRef CVarNaniteMaterialVisibilityAsync(
	TEXT("r.Nanite.MaterialVisibility.Async"),
	GNaniteMaterialVisibilityAsync,
	TEXT("Whether to enable parallelization of Nanite material visibility tests"),
	ECVF_RenderThreadSafe
);

int32 GNaniteMaterialVisibilityPrimitives = 1;
static FAutoConsoleVariableRef CVarNaniteMaterialVisibilityPrimitives(
	TEXT("r.Nanite.MaterialVisibility.Primitives"),
	GNaniteMaterialVisibilityPrimitives,
	TEXT("")
);

int32 GNaniteMaterialVisibilityInstances = 0;
static FAutoConsoleVariableRef CVarNaniteMaterialVisibilityInstances(
	TEXT("r.Nanite.MaterialVisibility.Instances"),
	GNaniteMaterialVisibilityInstances,
	TEXT("")
);

int32 GNaniteMaterialVisibilityRasterBins = 1;
static FAutoConsoleVariableRef CVarNaniteMaterialVisibilityRasterBins(
	TEXT("r.Nanite.MaterialVisibility.RasterBins"),
	GNaniteMaterialVisibilityRasterBins,
	TEXT("")
);

int32 GNaniteMaterialVisibilityShadingDraws = 1;
static FAutoConsoleVariableRef CVarNaniteMaterialVisibilityShadingDraws(
	TEXT("r.Nanite.MaterialVisibility.ShadingDraws"),
	GNaniteMaterialVisibilityShadingDraws,
	TEXT("")
);

int32 GNaniteResummarizeHTile = 1;
static FAutoConsoleVariableRef CVarNaniteResummarizeHTile(
	TEXT("r.Nanite.ResummarizeHTile"),
	GNaniteResummarizeHTile,
	TEXT("")
);

static TAutoConsoleVariable<int32> CVarParallelBasePassBuild(
	TEXT("r.Nanite.ParallelBasePassBuild"),
	1,
	TEXT(""),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarNaniteMultipleSceneViewsInOnePass(
	TEXT("r.Nanite.MultipleSceneViewsInOnePass"),
	1,
	TEXT("Supports rendering multiple views (FSceneView) whenever possible. Currently only ISR stereo rendering is supported."),
	ECVF_RenderThreadSafe
	);

int32 GNaniteDecompressDepth = 0;
static FAutoConsoleVariableRef CVarNaniteDecompressDepth(
	TEXT("r.Nanite.DecompressDepth"),
	GNaniteDecompressDepth,
	TEXT("")
);

// TODO: This should be defaulted to 1, but there are currently outstanding bugs with combining HTILEs containing both
// Nanite and non-Nanite depth and stencil
int32 GNaniteCustomDepthExportMethod = 1;
static FAutoConsoleVariableRef CVarNaniteCustomDepthExportMethod(
	TEXT("r.Nanite.CustomDepth.ExportMethod"),
	GNaniteCustomDepthExportMethod,
	TEXT("0 - Export depth/stencil into separate targets via PS\n")
	TEXT("1 - Export depth/stencil direct to target via CS (requires HTILE support)\n")
);

#if WITH_EDITORONLY_DATA
extern int32 GNaniteIsolateInvalidCoarseMesh;
#endif

extern TAutoConsoleVariable<int32> CVarNaniteShowDrawEvents;

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

static bool UseComputeMaterials()
{
	// TODO: Heavily work in progress / experimental - do not use!
	static const bool bAllowComputeMaterials = NaniteComputeMaterialsSupported();
	return (bAllowComputeMaterials && GNaniteComputeMaterials != 0);
}

static bool UseLegacyCulling()
{
	return !UseComputeMaterials();
}

static uint32 GetShadingRateTileSize()
{
	uint32 TileSize = 0;

	if (GNaniteSoftwareVRS != 0 && GRHISupportsAttachmentVariableRateShading)
	{
		// Technically these could be different, but currently never in practice
		// 8x8, 16x16, or 32x32 for DX12 Tier2 HW VRS
		ensure
		(
			GRHIVariableRateShadingImageTileMinWidth == GRHIVariableRateShadingImageTileMinHeight &&
			GRHIVariableRateShadingImageTileMinWidth == GRHIVariableRateShadingImageTileMaxWidth &&
			GRHIVariableRateShadingImageTileMinWidth == GRHIVariableRateShadingImageTileMaxHeight
		);

		TileSize = GRHIVariableRateShadingImageTileMinWidth;
	}

	return TileSize;
}

static FRDGTextureRef GetShadingRateImage(FRDGBuilder& GraphBuilder, const FViewInfo& ViewInfo)
{
	FRDGTextureRef ShadingRateImage = nullptr;
	
	if (GetShadingRateTileSize() != 0)
	{
		ShadingRateImage = GVRSImageManager.GetVariableRateShadingImage(GraphBuilder, ViewInfo, FVariableRateShadingImageManager::EVRSPassType::NaniteEmitGBufferPass);
	}

	if (ShadingRateImage == nullptr)
	{
		const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);
		ShadingRateImage = SystemTextures.Black;
	}

	return ShadingRateImage;
}

class FNaniteMarkStencilPS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FNaniteMarkStencilPS);
	SHADER_USE_PARAMETER_STRUCT(FNaniteMarkStencilPS, FNaniteGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UlongType>, VisBuffer64)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FNaniteMarkStencilPS, "/Engine/Private/Nanite/NaniteExportGBuffer.usf", "MarkStencilPS", SF_Pixel);

class FEmitMaterialDepthPS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FEmitMaterialDepthPS);
	SHADER_USE_PARAMETER_STRUCT(FEmitMaterialDepthPS, FNaniteGlobalShader);

	class FLegacyCullingDim : SHADER_PERMUTATION_BOOL("LEGACY_CULLING");
	class FShadingMaskLoadDim : SHADER_PERMUTATION_BOOL("SHADING_MASK_LOAD");
	using FPermutationDomain = TShaderPermutationDomain<FLegacyCullingDim, FShadingMaskLoadDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
		SHADER_PARAMETER(uint32, DummyZero)

		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, VisibleClustersSWHW)
		SHADER_PARAMETER(FIntVector4, PageConstants)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ClusterPageData)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadingMask)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UlongType>, VisBuffer64)

		SHADER_PARAMETER_SRV(ByteAddressBuffer, MaterialSlotTable)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, MaterialDepthTable)

		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FEmitMaterialDepthPS, "/Engine/Private/Nanite/NaniteExportGBuffer.usf", "EmitMaterialDepthPS", SF_Pixel);

IMPLEMENT_GLOBAL_SHADER(FNaniteIndirectMaterialVS, "/Engine/Private/Nanite/NaniteExportGBuffer.usf", "FullScreenVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FNaniteMultiViewMaterialVS, "/Engine/Private/Nanite/NaniteExportGBuffer.usf", "FullScreenVS", SF_Vertex);

class FEmitSceneDepthPS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FEmitSceneDepthPS);
	SHADER_USE_PARAMETER_STRUCT(FEmitSceneDepthPS, FNaniteGlobalShader);

	class FLegacyCullingDim : SHADER_PERMUTATION_BOOL("LEGACY_CULLING");
	class FVelocityExportDim : SHADER_PERMUTATION_BOOL("VELOCITY_EXPORT");
	class FShadingMaskExportDim : SHADER_PERMUTATION_BOOL("SHADING_MASK_EXPORT");
	using FPermutationDomain = TShaderPermutationDomain<FLegacyCullingDim, FVelocityExportDim, FShadingMaskExportDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetRenderTargetOutputFormat(0, PF_R32_UINT);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FPackedView>, InViews)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, VisibleClustersSWHW)
		SHADER_PARAMETER(FIntVector4, PageConstants)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ClusterPageData)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UlongType>, VisBuffer64)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, MaterialSlotTable)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FEmitSceneDepthPS, "/Engine/Private/Nanite/NaniteExportGBuffer.usf", "EmitSceneDepthPS", SF_Pixel);

class FEmitSceneStencilPS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FEmitSceneStencilPS);
	SHADER_USE_PARAMETER_STRUCT(FEmitSceneStencilPS, FNaniteGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADING_MASK_LOAD"), 1);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)

		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, VisibleClustersSWHW)
		SHADER_PARAMETER(FIntVector4, PageConstants)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ClusterPageData)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadingMask)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UlongType>, VisBuffer64)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FEmitSceneStencilPS, "/Engine/Private/Nanite/NaniteExportGBuffer.usf", "EmitSceneStencilPS", SF_Pixel);

class FEmitCustomDepthStencilPS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FEmitCustomDepthStencilPS);
	SHADER_USE_PARAMETER_STRUCT(FEmitCustomDepthStencilPS, FNaniteGlobalShader);

	class FWriteCustomStencilDim : SHADER_PERMUTATION_BOOL("WRITE_CUSTOM_STENCIL");
	using FPermutationDomain = TShaderPermutationDomain<FWriteCustomStencilDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		const FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FWriteCustomStencilDim>())
		{
			OutEnvironment.SetRenderTargetOutputFormat(0, PF_R16G16_UINT);
		}
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FPackedView>, InViews)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, VisibleClustersSWHW)
		SHADER_PARAMETER(FIntVector4, PageConstants)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ClusterPageData)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UlongType>, VisBuffer64)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, CustomDepth)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<uint2>, CustomStencil)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FEmitCustomDepthStencilPS, "/Engine/Private/Nanite/NaniteExportGBuffer.usf", "EmitCustomDepthStencilPS", SF_Pixel);

class FDepthExportCS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDepthExportCS);
	SHADER_USE_PARAMETER_STRUCT(FDepthExportCS, FNaniteGlobalShader);

	class FVelocityExportDim : SHADER_PERMUTATION_BOOL("VELOCITY_EXPORT");
	class FMaterialDepthExportDim : SHADER_PERMUTATION_BOOL("MATERIAL_DEPTH_EXPORT");
	class FShadingMaskExportDim : SHADER_PERMUTATION_BOOL("SHADING_MASK_EXPORT");
	class FLegacyCullingDim : SHADER_PERMUTATION_BOOL("LEGACY_CULLING");
	using FPermutationDomain = TShaderPermutationDomain<FVelocityExportDim, FMaterialDepthExportDim, FShadingMaskExportDim, FLegacyCullingDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FPackedView>, InViews)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, VisibleClustersSWHW)
		SHADER_PARAMETER(FIntVector4, PageConstants)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ClusterPageData)
		SHADER_PARAMETER(FIntVector4, DepthExportConfig)
		SHADER_PARAMETER(FUint32Vector4, ViewRect)
		SHADER_PARAMETER(uint32, bWriteCustomStencil)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UlongType>, VisBuffer64)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, Velocity)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, ShadingMask)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTextureMetadata, SceneHTile)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, SceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, SceneStencil)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTextureMetadata, MaterialHTile)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, MaterialDepth)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, MaterialSlotTable)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, MaterialDepthTable)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FDepthExportCS, "/Engine/Private/Nanite/NaniteDepthExport.usf", "DepthExport", SF_Compute);

class FClearTilesCS : public FNaniteGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FClearTilesCS);

	class FNumExports : SHADER_PERMUTATION_RANGE_INT("NUM_EXPORTS", 1, MaxSimultaneousRenderTargets);
	using FPermutationDomain = TShaderPermutationDomain<FNumExports>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FUint32Vector4, ViewRect)
		SHADER_PARAMETER(uint32, ValidWriteMask)
		SHADER_PARAMETER_RDG_TEXTURE_UAV_ARRAY(RWTextureMetadata, OutCMaskBuffer, [MaxSimultaneousRenderTargets])
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<uint>, ShadingMask)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FNaniteShadingBinMeta>, ShadingBinMeta)
	END_SHADER_PARAMETER_STRUCT()

	FClearTilesCS() = default;
	FClearTilesCS(const ShaderMetaType::CompiledShaderInitializerType & Initializer)
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
IMPLEMENT_GLOBAL_SHADER(FClearTilesCS, "/Engine/Private/Nanite/NaniteFastClear.usf", "ClearTiles", SF_Compute);

class FInitializeMaterialsCS : public FNaniteGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FInitializeMaterialsCS);
	SHADER_USE_PARAMETER_STRUCT(FInitializeMaterialsCS, FNaniteGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, MaterialTileCount)
		SHADER_PARAMETER(uint32, MaterialRemapCount)
		SHADER_PARAMETER(uint32, MaterialSlotCount)
		SHADER_PARAMETER(uint32, MaterialBinCount)
		SHADER_PARAMETER(uint32, TopologyIndexCount)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, MaterialIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, MaterialTileRemap)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FInitializeMaterialsCS, "/Engine/Private/Nanite/NaniteMaterialCulling.usf", "InitializeMaterials", SF_Compute);

class FFinalizeMaterialsCS : public FNaniteGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFinalizeMaterialsCS);
	SHADER_USE_PARAMETER_STRUCT(FFinalizeMaterialsCS, FNaniteGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, MaterialTileCount)
		SHADER_PARAMETER(uint32, MaterialSlotCount)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, MaterialIndirectArgs)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FFinalizeMaterialsCS, "/Engine/Private/Nanite/NaniteMaterialCulling.usf", "FinalizeMaterials", SF_Compute);

class FClassifyMaterialsCS : public FNaniteGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FClassifyMaterialsCS);
	SHADER_USE_PARAMETER_STRUCT(FClassifyMaterialsCS, FNaniteGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, VisibleClustersSWHW)
		SHADER_PARAMETER(FIntVector4, PageConstants)
		SHADER_PARAMETER(FIntVector4, ViewRect)
		SHADER_PARAMETER(FIntPoint, FetchClamp)
		SHADER_PARAMETER(uint32, MaterialTileCount)
		SHADER_PARAMETER(uint32, MaterialRemapCount)
		SHADER_PARAMETER(uint32, MaterialSlotCount)
		SHADER_PARAMETER(uint32, MaterialBinCount)
		SHADER_PARAMETER(uint32, RowTileCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ClusterPageData)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UlongType>, VisBuffer64)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, MaterialIndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, MaterialTileRemap)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, MaterialSlotTable)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, MaterialDepthTable)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadingMask)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};
IMPLEMENT_GLOBAL_SHADER(FClassifyMaterialsCS, "/Engine/Private/Nanite/NaniteMaterialCulling.usf", "ClassifyMaterials", SF_Compute);

class FShadingBinBuildCS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FShadingBinBuildCS);
	SHADER_USE_PARAMETER_STRUCT(FShadingBinBuildCS, FNaniteGlobalShader);

	class FBuildPassDim : SHADER_PERMUTATION_SPARSE_INT("SHADING_BIN_PASS", NANITE_SHADING_BIN_COUNT, NANITE_SHADING_BIN_SCATTER);
	class FTechniqueDim : SHADER_PERMUTATION_INT("BINNING_TECHNIQUE", 3);
	class FGatherStatsDim : SHADER_PERMUTATION_BOOL("GATHER_STATS");
	class FQuadBinningDim : SHADER_PERMUTATION_BOOL("QUAD_BINNING");
	class FVariableRateDim : SHADER_PERMUTATION_BOOL("VARIABLE_SHADING_RATE");
	using FPermutationDomain = TShaderPermutationDomain<FBuildPassDim, FTechniqueDim, FGatherStatsDim, FQuadBinningDim, FVariableRateDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (!DoesPlatformSupportNanite(Parameters.Platform))
		{
			return false;
		}

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FGatherStatsDim>() && PermutationVector.Get<FBuildPassDim>() != NANITE_SHADING_BIN_COUNT)
		{
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
		SHADER_PARAMETER(FUint32Vector2, QuadDispatchDim)
		SHADER_PARAMETER(uint32, ShadingBinCount)
		SHADER_PARAMETER(uint32, ShadingRateTileSize)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadingRateImage)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadingMask)
		SHADER_PARAMETER_SAMPLER(SamplerState, ShadingMaskSampler)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FNaniteShadingBinMeta>, OutShadingBinMeta)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutShadingBinData)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutShadingBinArgs)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FShadingBinBuildCS, "/Engine/Private/Nanite/NaniteShadeBinning.usf", "ShadingBinBuildCS", SF_Compute);

class FShadingBinReserveCS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FShadingBinReserveCS);
	SHADER_USE_PARAMETER_STRUCT(FShadingBinReserveCS, FNaniteGlobalShader);

	class FGatherStatsDim : SHADER_PERMUTATION_BOOL("GATHER_STATS");
	class FQuadBinningDim : SHADER_PERMUTATION_BOOL("QUAD_BINNING");
	using FPermutationDomain = TShaderPermutationDomain<FGatherStatsDim, FQuadBinningDim>;

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
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FNaniteShadingBinStats>, OutShadingBinStats)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FNaniteShadingBinMeta>, OutShadingBinMeta)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutShadingBinAllocator)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutShadingBinArgs)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FShadingBinReserveCS, "/Engine/Private/Nanite/NaniteShadeBinning.usf", "ShadingBinReserveCS", SF_Compute);

BEGIN_SHADER_PARAMETER_STRUCT(FNaniteMarkStencilRectsParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FPixelShaderUtils::FRasterizeToRectsVS::FParameters, VS)
	SHADER_PARAMETER_STRUCT_INCLUDE(FNaniteMarkStencilPS::FParameters, PS)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FNaniteEmitMaterialIdRectsParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FPixelShaderUtils::FRasterizeToRectsVS::FParameters, VS)
	SHADER_PARAMETER_STRUCT_INCLUDE(FEmitMaterialDepthPS::FParameters, PS)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FNaniteEmitDepthRectsParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FPixelShaderUtils::FRasterizeToRectsVS::FParameters, VS)
	SHADER_PARAMETER_STRUCT_INCLUDE(FEmitSceneDepthPS::FParameters, PS)
END_SHADER_PARAMETER_STRUCT()

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
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FNaniteEmitGBufferParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FNaniteShadingPassParameters, Shading)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

TRDGUniformBufferRef<FNaniteUniformParameters> CreateDebugNaniteUniformBuffer(FRDGBuilder& GraphBuilder, uint32 InstanceSceneDataSOAStride)
{
	FNaniteUniformParameters* UniformParameters = GraphBuilder.AllocParameters<FNaniteUniformParameters>();
	UniformParameters->PageConstants.X           = InstanceSceneDataSOAStride;
	UniformParameters->PageConstants.Y           = Nanite::GStreamingManager.GetMaxStreamingPages();
	UniformParameters->MaxNodes                  = Nanite::FGlobalResources::GetMaxNodes();
	UniformParameters->MaxVisibleClusters        = Nanite::FGlobalResources::GetMaxVisibleClusters();

	UniformParameters->ClusterPageData           = Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
	UniformParameters->HierarchyBuffer           = Nanite::GStreamingManager.GetHierarchySRV(GraphBuilder);
	UniformParameters->VisibleClustersSWHW       = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<uint32>(GraphBuilder));
	UniformParameters->MaterialTileRemap         = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<uint32>(GraphBuilder), PF_R32_UINT);

#if RHI_RAYTRACING
	UniformParameters->RayTracingCutError		= Nanite::GRayTracingManager.GetCutError();
	UniformParameters->RayTracingDataBuffer		= Nanite::GRayTracingManager.GetAuxiliaryDataSRV(GraphBuilder);
#else
	UniformParameters->RayTracingCutError		= 0.0f;
	UniformParameters->RayTracingDataBuffer		= GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<uint32>(GraphBuilder));
#endif

	UniformParameters->ShadingBinMeta			= GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<FNaniteShadingBinMeta>(GraphBuilder));
	UniformParameters->ShadingBinData			= GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<uint32>(GraphBuilder), PF_R32_UINT);

	const FRDGSystemTextures& SystemTextures		= FRDGSystemTextures::Get(GraphBuilder);
	UniformParameters->VisBuffer64					= SystemTextures.Black;
	UniformParameters->DbgBuffer64					= SystemTextures.Black;
	UniformParameters->DbgBuffer32					= SystemTextures.Black;
	UniformParameters->ShadingMask					= SystemTextures.Black;
	UniformParameters->MaterialDepthTable			= GraphBuilder.GetPooledBuffer(GSystemTextures.GetDefaultBuffer(GraphBuilder, 4, 0u))->GetSRV(GraphBuilder.RHICmdList, FRHIBufferSRVCreateInfo(PF_R32_UINT));
	UniformParameters->MultiViewIndices				= GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<uint32>(GraphBuilder));
	UniformParameters->MultiViewRectScaleOffsets	= GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<FVector4>(GraphBuilder));
	UniformParameters->InViews						= GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<FPackedNaniteView>(GraphBuilder));

	return GraphBuilder.CreateUniformBuffer(UniformParameters);
}

// TODO: Heavily work in progress / experimental - do not use!
struct FNaniteShadingCommand
{
	const FMaterialRenderProxy* MaterialProxy = nullptr;
	const FMaterial* Material = nullptr;
	TShaderRef<TBasePassComputeShaderPolicyParamType<FUniformLightMapPolicy>> ComputeShader;
	FMeshDrawShaderBindings ShaderBindings;
	uint32 BoundTargetMask = 0u;
	uint16 ShadingBin = 0xFFFFu;
};

namespace Nanite
{

// TODO: Heavily work in progress / experimental - do not use!
void BuildShadingCommands(
	const FScene& Scene,
	const FNaniteShadingPipelines& ShadingPipelines,
	TArray<TPimplPtr<FNaniteShadingCommand>>& ShadingCommands
)
{
	const ERHIFeatureLevel::Type FeatureLevel = Scene.GetFeatureLevel();

	FNaniteVertexFactory* NaniteVertexFactory = GVertexFactoryResource.GetVertexFactory2();
	FVertexFactoryType* NaniteVertexFactoryType = NaniteVertexFactory->GetType();
	check(NaniteVertexFactory && NaniteVertexFactory->IsInitialized() && !NaniteVertexFactory->NeedsDeclaration());

	const auto& Pipelines = ShadingPipelines.GetShadingPipelineMap();

	FMeshPassProcessorRenderState DrawRenderState;
	{
		SetupBasePassState(FExclusiveDepthStencil::DepthWrite_StencilNop, false, DrawRenderState);
		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Equal>::GetRHI());
		DrawRenderState.SetDepthStencilAccess(FExclusiveDepthStencil::DepthWrite_StencilNop);
		check(DrawRenderState.GetDepthStencilState());
		check(DrawRenderState.GetBlendState());
	}

	ShadingCommands.Reset();
	ShadingCommands.Reserve(Pipelines.Num());

	for (const auto& ShadingBin : Pipelines)
	{
		const FNaniteShadingEntry& ShadingEntry = ShadingBin.Value;

		const FMaterialRenderProxy* ShadingMaterialRenderProxyPtr = ShadingEntry.ShadingPipeline.ShadingMaterial;
		while (ShadingMaterialRenderProxyPtr)
		{
			const FMaterial* Material = ShadingMaterialRenderProxyPtr->GetMaterialNoFallback(FeatureLevel);
			if (Material)
			{
				break;
			}
			ShadingMaterialRenderProxyPtr = ShadingMaterialRenderProxyPtr->GetFallback(FeatureLevel);
		}

		if (!ShadingMaterialRenderProxyPtr)
		{
			continue;
		}

		const FMaterial& ShadingMaterial = ShadingEntry.ShadingPipeline.ShadingMaterial->GetIncompleteMaterialWithFallback(FeatureLevel);
		check(Nanite::IsSupportedMaterialDomain(ShadingMaterial.GetMaterialDomain()));
		check(Nanite::IsSupportedBlendMode(ShadingMaterial));

		const FMaterialShadingModelField ShadingModels = ShadingMaterial.GetShadingModels();
		const bool bRenderSkylight = Scene.ShouldRenderSkylightInBasePass(IsTranslucentBlendMode(ShadingMaterial.GetBlendMode())) && ShadingModels != MSM_Unlit;
		FUniformLightMapPolicy LightMapPolicy = FUniformLightMapPolicy(LMP_NO_LIGHTMAP);

		const EGBufferLayout GBufferLayout = Nanite::GetGBufferLayoutForMaterial(ShadingMaterial.MaterialUsesWorldPositionOffset_RenderThread());

		TShaderRef<TBasePassComputeShaderPolicyParamType<FUniformLightMapPolicy>> BasePassComputeShader;
		bool bShadersValid = GetBasePassShader<FUniformLightMapPolicy>(
			ShadingMaterial,
			NaniteVertexFactoryType,
			LightMapPolicy,
			FeatureLevel,
			bRenderSkylight,
			GBufferLayout,
			&BasePassComputeShader
		);

		if (!bShadersValid)
		{
			continue;
		}

		TPimplPtr<FNaniteShadingCommand>& ShadingCommand = ShadingCommands.AddDefaulted_GetRef();
		ShadingCommand = MakePimpl<FNaniteShadingCommand>();
		ShadingCommand->ComputeShader = BasePassComputeShader;
		ShadingCommand->MaterialProxy = ShadingMaterialRenderProxyPtr;
		ShadingCommand->Material = ShadingCommand->MaterialProxy->GetMaterialNoFallback(FeatureLevel);
		ShadingCommand->ShadingBin = ShadingEntry.BinIndex;
		check(ShadingCommand->Material);

		TMeshProcessorShaders
		<
			FMeshMaterialShader, // Vertex
			FMeshMaterialShader, // Pixel
			FMeshMaterialShader, // Geometry
			FMeshMaterialShader, // RayTracing
			TBasePassComputeShaderPolicyParamType<FUniformLightMapPolicy>
		>
		PassShaders;
		PassShaders.ComputeShader = BasePassComputeShader;

		ShadingCommand->BoundTargetMask = PassShaders.ComputeShader->GetBoundTargetMask();

		const FShaderParameterBindings& Bindings = BasePassComputeShader->Bindings;

		//const FLightMapInteraction LightMapInteraction = (bAllowStaticLighting && MeshBatch.LCI && bIsLitMaterial)
		//	? MeshBatch.LCI->GetLightMapInteraction(FeatureLevel)
		//	: FLightMapInteraction();
		FLightCacheInterface* LCI = nullptr; // TODO: Lightmaps?

		TBasePassShaderElementData<FUniformLightMapPolicy> ShaderElementData(LCI);
		ShaderElementData.InitializeMeshMaterialData(
			/* SceneView = */ nullptr,
			/* PrimitiveSceneProxy = */ nullptr,
			/* StaticMeshId = */ INDEX_NONE,
			/* bDitheredLODTransition = */ false,
			/* bAllowStencilDither = */ false
		);

		ShadingCommand->ShaderBindings.Initialize(PassShaders.GetUntypedShaders());

		{
			int32 DataOffset = 0;
			if (PassShaders.ComputeShader.IsValid())
			{
				FMeshDrawSingleShaderBindings ShaderBindings = ShadingCommand->ShaderBindings.GetSingleShaderBindings(SF_Compute, DataOffset);
				PassShaders.ComputeShader->GetShaderBindings(&Scene, FeatureLevel, /* PrimitiveSceneProxy = */ nullptr, *ShadingCommand->MaterialProxy, ShadingMaterial, DrawRenderState, ShaderElementData, ShaderBindings);
			}
		}

		FMeshProcessorShaders ShadersForDebugging = PassShaders.GetUntypedShaders();
		ShadingCommand->ShaderBindings.Finalize(&ShadersForDebugging);
	}

	if (GNaniteComputeMaterialsSort != 0)
	{
		ShadingCommands.Sort([&ShadingCommands](auto& A, auto& B)
		{
			if (A->ComputeShader.GetComputeShader() != B->ComputeShader.GetComputeShader())
			{
				return A->ComputeShader.GetComputeShader() < B->ComputeShader.GetComputeShader();
			}

			if (A->BoundTargetMask != B->BoundTargetMask)
			{
				return A->BoundTargetMask < B->BoundTargetMask;
			}

			if (A->Material != B->Material)
			{
				return A->Material < B->Material;
			}

			return A.Get() < B.Get();
		});
	}
}

void RecordShadingCommand(
	FRHIComputeCommandList& RHICmdList,
	FUint32Vector4& PassData,
	FRHIBuffer* IndirectArgsBuffer,
	const uint32 IndirectArgStride,
	const FUint32Vector4& ViewRect,
	const TArray<FRHIUnorderedAccessView*, TInlineAllocator<8>>& OutputTargets,
	FRHIUnorderedAccessView* OutputTargetsArray,
	const FNaniteShadingCommand& ShadingCommand
)
{
#if WANTS_DRAW_MESH_EVENTS
	SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, SWShading, CVarNaniteShowDrawEvents.GetValueOnRenderThread() != 0, TEXT("%s"), GetShadingMaterialName(ShadingCommand.MaterialProxy));
#endif

	PassData.X = ShadingCommand.ShadingBin;

	const uint32 IndirectOffset = (ShadingCommand.ShadingBin * IndirectArgStride);

	FRHIComputeShader* ComputeShaderRHI = ShadingCommand.ComputeShader.GetComputeShader();
	SetComputePipelineState(RHICmdList, ComputeShaderRHI);

	FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();
	ShadingCommand.ShaderBindings.SetParameters(BatchedParameters, ComputeShaderRHI);

	if (ComputeShaderRHI)
	{
		ShadingCommand.ComputeShader->SetPassParameters(
			BatchedParameters,
			ViewRect,
			PassData,
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

	RHICmdList.SetBatchedShaderParameters(ComputeShaderRHI, BatchedParameters);

	RHICmdList.DispatchIndirectComputeShader(IndirectArgsBuffer, IndirectOffset);
}

class FRecordShadingCommandsAnyThreadTask : public FRenderTask
{
	FRHICommandList& RHICmdList;
	FRHIBuffer* IndirectArgs = nullptr;
	const TConstArrayView<TPimplPtr<FNaniteShadingCommand>> ShadingCommands;
	TArray<FRHIUnorderedAccessView*, TInlineAllocator<8>> OutputTargets;
	FRHIUnorderedAccessView* OutputTargetsArray = nullptr;
	FUint32Vector4 ViewRect;
	FUint32Vector4 PassData;
	uint32 IndirectArgsStride;
	int32 TaskIndex;
	int32 TaskNum;

public:

	FRecordShadingCommandsAnyThreadTask(
		FRHICommandList& InRHICmdList,
		FRHIBuffer* InIndirectArgs,
		uint32 InIndirectArgsStride,
		const TConstArrayView<TPimplPtr<FNaniteShadingCommand>> InShadingCommands,
		const TConstArrayView<FRHIUnorderedAccessView*> InOutputTargets,
		FRHIUnorderedAccessView* InOutputTargetsArray,
		const FUint32Vector4& InViewRect,
		const FUint32Vector4& InPassData,
		int32 InTaskIndex,
		int32 InTaskNum
	)
		: RHICmdList(InRHICmdList)
		, IndirectArgs(InIndirectArgs)
		, ShadingCommands(InShadingCommands)
		, OutputTargets(InOutputTargets)
		, OutputTargetsArray(InOutputTargetsArray)
		, ViewRect(InViewRect)
		, PassData(InPassData)
		, IndirectArgsStride(InIndirectArgsStride)
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
		const int32 CommandNum = ShadingCommands.Num();
		const int32 NumCommandsPerTask = TaskIndex < CommandNum ? FMath::DivideAndRoundUp(CommandNum, TaskNum) : 0;
		const int32 StartIndex = TaskIndex * NumCommandsPerTask;
		const int32 NumCommands = FMath::Min(NumCommandsPerTask, CommandNum - StartIndex);

		for (int32 CommandIndex = 0; CommandIndex < NumCommands; ++CommandIndex)
		{
			RecordShadingCommand(
				RHICmdList,
				PassData,
				IndirectArgs,
				IndirectArgsStride,
				ViewRect,
				OutputTargets,
				OutputTargetsArray,
				*ShadingCommands[StartIndex + CommandIndex]
			);
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
	const FIntPoint& TileGridSize,
	const uint32 TileRemaps,
	const FNaniteMaterialCommands& MaterialCommands,
	FRDGTextureRef ShadingMask,
	FRDGTextureRef VisBuffer64,
	FRDGTextureRef DbgBuffer64,
	FRDGTextureRef DbgBuffer32,
	FRDGBufferRef VisibleClustersSWHW,
	FRDGBufferRef MaterialTileRemap,
	FRDGBufferRef MaterialIndirectArgs,
	FRDGBufferRef MultiViewIndices,
	FRDGBufferRef MultiViewRectScaleOffsets,
	FRDGBufferRef ViewsBuffer,
	const FRenderTargetBindingSlots& BasePassRenderTargets,
	const FShadeBinning& ShadeBinning
)
{
	FNaniteShadingPassParameters Result;

	const bool bShadeBinning = ShadeBinning.ShadingBinArgs != nullptr;
	Result.MaterialIndirectArgs = bShadeBinning ? ShadeBinning.ShadingBinArgs : MaterialIndirectArgs;

	{
		const FIntPoint ScaledSize = TileGridSize * 64;
		const FVector4f RectScaleOffset(
			float(ScaledSize.X) / float(ViewRect.Max.X - ViewRect.Min.X),
			float(ScaledSize.Y) / float(ViewRect.Max.Y - ViewRect.Min.Y),
			0.0f,
			0.0f
		);

		const FIntVector4 MaterialConfig(1 /* Indirect */, TileGridSize.X, TileGridSize.Y, TileRemaps);

		FNaniteUniformParameters* UniformParameters = GraphBuilder.AllocParameters<FNaniteUniformParameters>();
		UniformParameters->PageConstants = RasterResults.PageConstants;
		UniformParameters->MaxNodes = RasterResults.MaxNodes;
		UniformParameters->MaxVisibleClusters = RasterResults.MaxVisibleClusters;
		UniformParameters->RenderFlags = RasterResults.RenderFlags;

		UniformParameters->MaterialConfig = MaterialConfig;
		UniformParameters->RectScaleOffset = RectScaleOffset;

		UniformParameters->ClusterPageData = Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
		UniformParameters->HierarchyBuffer = Nanite::GStreamingManager.GetHierarchySRV(GraphBuilder);
		UniformParameters->VisibleClustersSWHW = GraphBuilder.CreateSRV(VisibleClustersSWHW);

		if (bShadeBinning)
		{
			UniformParameters->MaterialTileRemap = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<uint32>(GraphBuilder), PF_R32_UINT);
		}
		else
		{
			UniformParameters->MaterialTileRemap = GraphBuilder.CreateSRV(MaterialTileRemap, PF_R32_UINT);
		}

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
		UniformParameters->MaterialDepthTable = MaterialCommands.GetMaterialDepthSRV();

		UniformParameters->MultiViewEnabled = 0;
		UniformParameters->MultiViewIndices = GraphBuilder.CreateSRV(MultiViewIndices);
		UniformParameters->MultiViewRectScaleOffsets = GraphBuilder.CreateSRV(MultiViewRectScaleOffsets);
		UniformParameters->InViews = GraphBuilder.CreateSRV(ViewsBuffer);

		if (bShadeBinning)
		{
			UniformParameters->ShadingBinData = GraphBuilder.CreateSRV(ShadeBinning.ShadingBinData);
			UniformParameters->ShadingBinMeta = GraphBuilder.CreateSRV(ShadeBinning.ShadingBinMeta);
		}
		else
		{
			UniformParameters->ShadingBinData = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<uint32>(GraphBuilder), PF_R32_UINT);
			UniformParameters->ShadingBinMeta = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<FNaniteShadingBinMeta>(GraphBuilder), PF_R32G32B32A32_UINT);
		}

		Result.Nanite = GraphBuilder.CreateUniformBuffer(UniformParameters);
	}

	Result.View = View.GetShaderParameters(); // To get VTFeedbackBuffer
	Result.Scene = View.GetSceneUniforms().GetBuffer(GraphBuilder);
	const bool bLumenGIEnabled = SceneRenderer.IsLumenGIEnabled(View);
	Result.BasePass = CreateOpaqueBasePassUniformBuffer(GraphBuilder, View, 0, {}, DBufferTextures, bLumenGIEnabled);
	Result.ActiveShadingBin = ~uint32(0);

	if (UseComputeMaterials())
	{
		// No possibility of read/write hazard due to fully resolved vbuffer/materials
		const ERDGUnorderedAccessViewFlags OutTargetFlags = GNaniteBarrierTest != 0 ? ERDGUnorderedAccessViewFlags::SkipBarrier : ERDGUnorderedAccessViewFlags::None;

		FRDGTextureUAVRef MaterialTextureArrayUAV = nullptr;
		if (Strata::IsStrataEnabled() && SceneRenderer.Scene)
		{
			MaterialTextureArrayUAV = GraphBuilder.CreateUAV(SceneRenderer.Scene->StrataSceneData.MaterialTextureArray, OutTargetFlags);
			//MaterialTextureArrayUAV = SceneRenderer.Scene->StrataSceneData.MaterialTextureArrayUAVWithoutRTs;
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
				if (bMaintainCompression)
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
	}

	return Result;
}

// TODO: Heavily work in progress / experimental - do not use!
void DispatchBasePass(
	FRDGBuilder& GraphBuilder,
	TArray<FNaniteMaterialPassCommand, SceneRenderingAllocator>& MaterialPassCommands,
	const FSceneRenderer& SceneRenderer,
	const FSceneTextures& SceneTextures,
	const FRenderTargetBindingSlots& BasePassRenderTargets,
	const FDBufferTextures& DBufferTextures,
	const FScene& Scene,
	const FViewInfo& View,
	const FIntRect InViewRect,
	const FRasterResults& RasterResults
)
{
	checkSlow(DoesPlatformSupportNanite(GMaxRHIShaderPlatform));

	LLM_SCOPE_BYTAG(Nanite);
	RDG_EVENT_SCOPE(GraphBuilder, "Nanite::BasePass");
	SCOPED_NAMED_EVENT(DispatchBasePass, FColor::Emerald);

	const TArray<TPimplPtr<FNaniteShadingCommand>>& ShadingCommands = Scene.NaniteShadingCommands[ENaniteMeshPass::BasePass];
	const uint32 ShadingBinCount = uint32(ShadingCommands.Num());

	if (ShadingBinCount == 0u)
	{
		return;
	}

	FShadeBinning Binning = ShadeBinning(GraphBuilder, Scene, View, InViewRect, RasterResults);

	const int32 ViewWidth = InViewRect.Max.X - InViewRect.Min.X;
	const int32 ViewHeight = InViewRect.Max.Y - InViewRect.Min.Y;
	const FIntPoint ViewSize = FIntPoint(ViewWidth, ViewHeight);

	const FUint32Vector4 ViewRect(
		(uint32)InViewRect.Min.X,
		(uint32)InViewRect.Min.Y,
		(uint32)InViewRect.Max.X,
		(uint32)InViewRect.Max.Y
	);

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

	const FNaniteVisibilityResults& VisibilityResults = RasterResults.VisibilityResults;
	const bool bWPOInSecondPass = !IsUsingBasePassVelocity(View.GetShaderPlatform());

	TStaticArray<FTextureRenderTargetBinding, MaxSimultaneousRenderTargets> BasePassTextures;
	uint32 BasePassTextureCount = SceneTextures.GetGBufferRenderTargets(BasePassTextures, GBL_Default);

	// We don't want to have Substrate MRTs appended to the list, except for the top layer data
	if (Strata::IsStrataEnabled() && SceneRenderer.Scene)
	{
		// Add another MRT for Strata top layer information. We want to follow the usual clear process which can leverage fast clear.
		{
			BasePassTextures[BasePassTextureCount] = FTextureRenderTargetBinding(SceneRenderer.Scene->StrataSceneData.TopLayerTexture);
			BasePassTextureCount++;
		};
	}

	TArrayView<FTextureRenderTargetBinding> BasePassTexturesView = MakeArrayView(BasePassTextures.GetData(), BasePassTextureCount);

	// Render targets bindings should remain constant at this point.
	FRenderTargetBindingSlots BasePassBindings = GetRenderTargetBindings(ERenderTargetLoadAction::ELoad, BasePassTexturesView);
	BasePassBindings.DepthStencil = BasePassRenderTargets.DepthStencil;

	// Fast tile clear prior to fast clear eliminate
	const bool bFastTileClear = UseComputeMaterials() && GNaniteFastTileClear != 0 && RHISupportsRenderTargetWriteMask(GMaxRHIShaderPlatform);
	if (bFastTileClear)
	{
		uint32 ValidWriteMask = 0x0u;

		TArray<FRDGTextureRef, TInlineAllocator<MaxSimultaneousRenderTargets>> TargetList;
		for (uint32 TargetIndex = 0; TargetIndex < MaxSimultaneousRenderTargets; ++TargetIndex)
		{
			if (FRDGTexture* TargetTexture = BasePassRenderTargets.Output[TargetIndex].GetTexture())
			{
				if (!EnumHasAnyFlags(TargetTexture->Desc.Flags, TexCreate_DisableDCC))
				{
					// Skip any targets that do not explicitly disable DCC, as this clear would not work correctly for DCC
					continue;
				}

				if (EnumHasAnyFlags(TargetTexture->Desc.Flags, TexCreate_NoFastClear))
				{
					// Skip any targets that explicitly disable fast clear optimization
					continue;
				}

				TargetList.Add(TargetTexture);

				// Compute a mask containing only set bits for MRT targets that are suitable for meta data optimization.
				ValidWriteMask |= (1u << TargetIndex);
			}
		}

		//FRDGTextureUAVRef MaterialTextureArrayUAV = nullptr;
		//if (Strata::IsStrataEnabled() && SceneRenderer.Scene)
		//{
			//MaterialTextureArrayUAV = GraphBuilder.CreateUAV(SceneRenderer.Scene->StrataSceneData.MaterialTextureArray, OutTargetFlags);
		//}

		if (TargetList.Num() > 0)
		{
			FClearTilesCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FClearTilesCS::FNumExports>(TargetList.Num());
			auto ClearTilesComputeShader = View.ShaderMap->GetShader<FClearTilesCS>(PermutationVector);

			FClearTilesCS::FParameters* ClearTilesPassParameters = GraphBuilder.AllocParameters<FClearTilesCS::FParameters>();
			ClearTilesPassParameters->ValidWriteMask = ValidWriteMask;
			ClearTilesPassParameters->ViewRect = ViewRect;
			ClearTilesPassParameters->ShadingMask = GraphBuilder.CreateSRV(RasterResults.ShadingMask);
			ClearTilesPassParameters->ShadingBinMeta = GraphBuilder.CreateSRV(Binning.ShadingBinMeta);

			for (int32 TargetIndex = 0; TargetIndex < TargetList.Num(); ++TargetIndex)
			{
				ClearTilesPassParameters->OutCMaskBuffer[TargetIndex] = GraphBuilder.CreateUAV(FRDGTextureUAVDesc::CreateForMetaData(TargetList[TargetIndex], ERDGTextureMetaDataAccess::CMask));
			}

			const FIntVector DispatchDim = FComputeShaderUtils::GetGroupCount(InViewRect.Size(), 8u);

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("NaniteTileClear"),
				ClearTilesPassParameters,
				ERDGPassFlags::Compute,
				[DispatchDim, ClearTilesComputeShader, ClearTilesPassParameters](FRHIComputeCommandList& RHICmdList)
				{
					// Note: Assumes all targets match in resolution (which they should)
					FRHITexture* TargetTextureRHI = ClearTilesPassParameters->OutCMaskBuffer[0]->GetParentRHI();

					// Retrieve the platform specific data that the decode shader needs.
					void* PlatformDataPtr = nullptr;
					uint32 PlatformDataSize = 0;
					TargetTextureRHI->GetWriteMaskProperties(PlatformDataPtr, PlatformDataSize);
					check(PlatformDataSize > 0);

					if (PlatformDataPtr == nullptr)
					{
						// If the returned pointer was null, the platform RHI wants us to allocate the memory instead.
						PlatformDataPtr = alloca(PlatformDataSize);
						TargetTextureRHI->GetWriteMaskProperties(PlatformDataPtr, PlatformDataSize);
					}

					SetComputePipelineState(RHICmdList, ClearTilesComputeShader.GetComputeShader());
					SetShaderParametersMixedCS(RHICmdList, ClearTilesComputeShader, *ClearTilesPassParameters, PlatformDataPtr, PlatformDataSize);

					RHICmdList.DispatchComputeShader(DispatchDim.X, DispatchDim.Y, DispatchDim.Z);
				}
			);
		}
	}

	FNaniteShadingPassParameters* ShadingPassParameters = GraphBuilder.AllocParameters<FNaniteShadingPassParameters>();
	*ShadingPassParameters = CreateNaniteShadingPassParams(
		GraphBuilder,
		SceneRenderer,
		SceneTextures,
		DBufferTextures,
		View,
		InViewRect,
		RasterResults,
		FIntPoint::ZeroValue,//TileGridSize,
		0,//TileRemaps,
		Scene.NaniteMaterials[ENaniteMeshPass::BasePass],
		RasterResults.ShadingMask,
		VisBuffer64,
		DbgBuffer64,
		DbgBuffer32,
		VisibleClustersSWHW,
		nullptr,//MaterialTileRemap,
		nullptr,//MaterialIndirectArgs,
		MultiViewIndices,
		MultiViewRectScaleOffsets,
		ViewsBuffer,
		BasePassBindings,
		Binning
	);

	const bool bSkipBarriers = GNaniteBarrierTest != 0;

	auto ShadePassWork = []
	(
		FRDGParallelCommandListSet* ParallelCommandListSet,
		const FUint32Vector4& ViewRect,
		const TConstArrayView<const TPimplPtr<FNaniteShadingCommand>> ShadingCommands,
		FNaniteShadingPassParameters* ShadingPassParameters,
		FRHIComputeCommandList& RHICmdList,
		const uint32 IndirectArgStride,
		bool bSkipBarriers
	)
	{
		ShadingPassParameters->MaterialIndirectArgs->MarkResourceAsUsed();

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

		OutputTargets.Add(GetOutputTargetRHI(ShadingPassParameters->OutTarget0));
		OutputTargets.Add(GetOutputTargetRHI(ShadingPassParameters->OutTarget1));
		OutputTargets.Add(GetOutputTargetRHI(ShadingPassParameters->OutTarget2));
		OutputTargets.Add(GetOutputTargetRHI(ShadingPassParameters->OutTarget3));
		OutputTargets.Add(GetOutputTargetRHI(ShadingPassParameters->OutTarget4));
		OutputTargets.Add(GetOutputTargetRHI(ShadingPassParameters->OutTarget5));
		OutputTargets.Add(GetOutputTargetRHI(ShadingPassParameters->OutTarget6));
		OutputTargets.Add(GetOutputTargetRHI(ShadingPassParameters->OutTarget7));

		FRHIUnorderedAccessView* OutputTargetsArray = GetOutputTargetRHI(ShadingPassParameters->OutTargets);

		// .X = Active Shading Bin
		// .Y = VRS Tile Size
		// .Z = Quad Binning Flag
		// .W = Unused
		FUint32Vector4 PassData(
			0, // Set per shading command
			GetShadingRateTileSize(),
			GNaniteQuadBinning != 0,
			0
		);

		FRHIBuffer* IndirectArgsBuffer = ShadingPassParameters->MaterialIndirectArgs->GetIndirectRHICallBuffer();

		if (ParallelCommandListSet)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ParallelRecordShadingCommands);

			// Distribute work evenly to the available task graph workers based on NumPassCommands.
			const int32 NumPassCommands = ShadingCommands.Num();
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
					ConstructAndDispatchWhenReady(*CmdList, IndirectArgsBuffer, IndirectArgStride, ShadingCommands, OutputTargets, OutputTargetsArray, ViewRect, PassData, TaskIndex, NumTasks);

				ParallelCommandListSet->AddParallelCommandList(CmdList, AnyThreadCompletionEvent, NumCommands);
			}
		}
		else
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(RecordShadingCommands);

			for (const TPimplPtr<FNaniteShadingCommand>& ShadingCommand : ShadingCommands)
			{
				RecordShadingCommand(RHICmdList, PassData, IndirectArgsBuffer, IndirectArgStride, ViewRect, OutputTargets, OutputTargetsArray, *ShadingCommand);
			}
		}
	};

	const bool bParallelDispatch = GRHICommandList.UseParallelAlgorithms() && CVarParallelBasePassBuild.GetValueOnRenderThread() != 0 && FParallelMeshDrawCommandPass::IsOnDemandShaderCreationEnabled();
	if (bParallelDispatch)
	{
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("ShadeGBufferCS"),
			ShadingPassParameters,
			ERDGPassFlags::Compute,
			[&ShadePassWork, ShadingPassParameters, &ShadingCommands, IndirectArgStride, &View, ViewRect, bSkipBarriers](const FRDGPass* RDGPass, FRHICommandListImmediate& RHICmdList)
			{
				FParallelCommandListBindings CmdListBindings(ShadingPassParameters);
				FRDGParallelCommandListSet ParallelCommandListSet(RDGPass, RHICmdList, GET_STATID(STAT_CLP_NaniteBasePass), View, CmdListBindings);
				ParallelCommandListSet.SetHighPriority();

				ShadePassWork(&ParallelCommandListSet, ViewRect, ShadingCommands, ShadingPassParameters, RHICmdList, IndirectArgStride, bSkipBarriers);
			}
		);
	}
	else
	{
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("ShadeGBufferCS"),
			ShadingPassParameters,
			ERDGPassFlags::Compute,
			[&ShadePassWork, ShadingPassParameters, &ShadingCommands, IndirectArgStride, &View, ViewRect, bSkipBarriers](const FRDGPass* RDGPass, FRHIComputeCommandList& RHICmdList)
			{
				ShadePassWork(nullptr, ViewRect, ShadingCommands, ShadingPassParameters, RHICmdList, IndirectArgStride, bSkipBarriers);
			}
		);
	}

	ExtractShadingDebug(GraphBuilder, View, nullptr, Binning, ShadingBinCount);
}

void DrawBasePass(
	FRDGBuilder& GraphBuilder,
	TArray<FNaniteMaterialPassCommand, SceneRenderingAllocator>& MaterialPassCommands,
	FSceneRenderer& SceneRenderer,
	const FSceneTextures& SceneTextures,
	const FRenderTargetBindingSlots& BasePassRenderTargets,
	const FDBufferTextures& DBufferTextures,
	const FScene& Scene,
	const FViewInfo& View,
	const FRasterResults& RasterResults
)
{
	const bool bDrawSceneViewsInOneNanitePass = ShouldDrawSceneViewsInOneNanitePass(View);
	FIntRect ViewRect = bDrawSceneViewsInOneNanitePass ? View.GetFamilyViewRect() : View.ViewRect;

	if (UseComputeMaterials())
	{
		DispatchBasePass(
			GraphBuilder,
			MaterialPassCommands,
			SceneRenderer,
			SceneTextures,
			BasePassRenderTargets,
			DBufferTextures,
			Scene,
			View,
			ViewRect,
			RasterResults
		);

		return;
	}

	checkSlow(DoesPlatformSupportNanite(GMaxRHIShaderPlatform));

	LLM_SCOPE_BYTAG(Nanite);
	RDG_EVENT_SCOPE(GraphBuilder, "Nanite::BasePass");

	FShadeBinning Binning{};

	const int32 ViewWidth		= ViewRect.Max.X - ViewRect.Min.X;
	const int32 ViewHeight		= ViewRect.Max.Y - ViewRect.Min.Y;
	const FIntPoint ViewSize	= FIntPoint(ViewWidth, ViewHeight);

	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);

	FRDGTextureRef MaterialDepth	= RasterResults.MaterialDepth ? RasterResults.MaterialDepth : SystemTextures.Black;
	FRDGTextureRef VisBuffer64		= RasterResults.VisBuffer64   ? RasterResults.VisBuffer64   : SystemTextures.Black;
	FRDGTextureRef DbgBuffer64		= RasterResults.DbgBuffer64   ? RasterResults.DbgBuffer64   : SystemTextures.Black;
	FRDGTextureRef DbgBuffer32		= RasterResults.DbgBuffer32   ? RasterResults.DbgBuffer32   : SystemTextures.Black;

	FRDGBufferRef VisibleClustersSWHW	= RasterResults.VisibleClustersSWHW;

	const uint32 MaxMaterialSlots = NANITE_MAX_STATE_BUCKET_ID + 1;

	const uint32 IndirectArgStride = (sizeof(FRHIDrawIndexedIndirectParameters) + sizeof(FRHIDispatchIndirectParametersNoPadding)) >> 2u;
	FRDGBufferRef MaterialIndirectArgs = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(IndirectArgStride * MaxMaterialSlots), TEXT("Nanite.MaterialIndirectArgs"));

	FRDGBufferRef MultiViewIndices = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("Nanite.DummyMultiViewIndices"));
	FRDGBufferRef MultiViewRectScaleOffsets = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector4f), 1), TEXT("Nanite.DummyMultiViewRectScaleOffsets"));
	FRDGBufferRef ViewsBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FVector4f), 1), TEXT("Nanite.PackedViews"));

	const uint32 HighestMaterialSlot = Scene.NaniteMaterials[ENaniteMeshPass::BasePass].GetHighestMaterialSlot();
	const uint32 HighestMaterialBin  = FMath::DivideAndRoundUp(HighestMaterialSlot, 32u);

	const FIntPoint	TileGridSize	= FMath::DivideAndRoundUp(ViewRect.Max - ViewRect.Min, { 64, 64 });
	const uint32	TileCount		= TileGridSize.X * TileGridSize.Y;
	const uint32	TileRemaps		= FMath::DivideAndRoundUp(TileCount, 32u);

	FRDGBufferRef MaterialTileRemap = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), TileRemaps * MaxMaterialSlots), TEXT("Nanite.MaterialTileRemap"));

	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(MultiViewIndices), 0);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(MultiViewRectScaleOffsets), 0);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(ViewsBuffer), 0);

	// Classify materials for tile culling
	// TODO: Run velocity export in here instead of depth pre-pass?
	{
		// Initialize acceleration/indexing structures for tile classification
		{
			auto ComputeShader = View.ShaderMap->GetShader<FInitializeMaterialsCS>();
			FInitializeMaterialsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInitializeMaterialsCS::FParameters>();
			PassParameters->MaterialSlotCount		= HighestMaterialSlot;
			PassParameters->MaterialTileCount		= TileGridSize.X * TileGridSize.Y;
			PassParameters->MaterialRemapCount		= TileRemaps;
			PassParameters->TopologyIndexCount		= GRHISupportsRectTopology ? 3 : 6;
			PassParameters->MaterialIndirectArgs	= GraphBuilder.CreateUAV(FRDGBufferUAVDesc(MaterialIndirectArgs, PF_R32_UINT));
			PassParameters->MaterialTileRemap		= GraphBuilder.CreateUAV(MaterialTileRemap);
			PassParameters->MaterialBinCount		= HighestMaterialBin;

			const FIntVector DispatchDim = FComputeShaderUtils::GetGroupCount(PassParameters->MaterialSlotCount, 64);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Initialize Materials"),
				ComputeShader,
				PassParameters,
				DispatchDim
			);
		}

		// Material tile classification
		{
			FClassifyMaterialsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FClassifyMaterialsCS::FParameters>();
			PassParameters->View					= View.ViewUniformBuffer;
			PassParameters->Scene					= SceneRenderer.GetSceneUniforms().GetBuffer(GraphBuilder);
			PassParameters->VisibleClustersSWHW		= GraphBuilder.CreateSRV(VisibleClustersSWHW);
			PassParameters->PageConstants			= RasterResults.PageConstants;
			PassParameters->ClusterPageData			= Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
			PassParameters->VisBuffer64				= VisBuffer64;
			PassParameters->MaterialSlotTable		= Scene.NaniteMaterials[ENaniteMeshPass::BasePass].GetMaterialSlotSRV();
			PassParameters->MaterialDepthTable		= Scene.NaniteMaterials[ENaniteMeshPass::BasePass].GetMaterialDepthSRV();
			PassParameters->ShadingMask				= RasterResults.ShadingMask;
			PassParameters->MaterialIndirectArgs	= GraphBuilder.CreateUAV(FRDGBufferUAVDesc(MaterialIndirectArgs, PF_R32_UINT));
			PassParameters->MaterialTileRemap		= GraphBuilder.CreateUAV(MaterialTileRemap);
			PassParameters->MaterialSlotCount		= HighestMaterialSlot;
			PassParameters->MaterialTileCount		= TileGridSize.X * TileGridSize.Y;
			PassParameters->MaterialRemapCount		= TileRemaps;
			PassParameters->MaterialBinCount		= HighestMaterialBin;

			uint32 DispatchGroupSize = 0;

			PassParameters->ViewRect = FIntVector4(ViewRect.Min.X, ViewRect.Min.Y, ViewRect.Max.X, ViewRect.Max.Y);
			DispatchGroupSize = 64;
			PassParameters->FetchClamp = ViewRect.Max - 1;

			const FIntVector DispatchDim = FComputeShaderUtils::GetGroupCount(ViewRect.Max - ViewRect.Min, DispatchGroupSize);

			PassParameters->RowTileCount = DispatchDim.X;

			FClassifyMaterialsCS::FPermutationDomain PermutationShadingMaskCS;
			auto ComputeShader = View.ShaderMap->GetShader<FClassifyMaterialsCS>(PermutationShadingMaskCS);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Classify Materials"),
				ComputeShader,
				PassParameters,
				DispatchDim
			);
		}

		// Finalize acceleration/indexing structures for tile classification
		{
			auto ComputeShader = View.ShaderMap->GetShader<FFinalizeMaterialsCS>();
			FFinalizeMaterialsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFinalizeMaterialsCS::FParameters>();
			PassParameters->MaterialSlotCount		= HighestMaterialSlot;
			PassParameters->MaterialTileCount		= TileGridSize.X * TileGridSize.Y;
			PassParameters->MaterialIndirectArgs	= GraphBuilder.CreateUAV(FRDGBufferUAVDesc(MaterialIndirectArgs, PF_R32_UINT));

			const FIntVector DispatchDim = FComputeShaderUtils::GetGroupCount(PassParameters->MaterialSlotCount, 64);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Finalize Materials"),
				ComputeShader,
				PassParameters,
				DispatchDim
			);
		}
	}

	const FNaniteMaterialCommands& MaterialCommands = Scene.NaniteMaterials[ENaniteMeshPass::BasePass];
	const int32 NumMaterialCommands = MaterialCommands.GetCommands().Num();
	MaterialPassCommands.Reset(NumMaterialCommands);

	if (NumMaterialCommands > 0)
	{
		const FNaniteVisibilityResults& VisibilityResults = RasterResults.VisibilityResults;
		const bool bWPOInSecondPass = !IsUsingBasePassVelocity(View.GetShaderPlatform());

		FNaniteShadingPassParameters TempParams = CreateNaniteShadingPassParams(
			GraphBuilder,
			SceneRenderer,
			SceneTextures,
			DBufferTextures,
			View,
			ViewRect,
			RasterResults,
			TileGridSize,
			TileRemaps,
			MaterialCommands,
			RasterResults.ShadingMask,
			VisBuffer64,
			DbgBuffer64,
			DbgBuffer32,
			VisibleClustersSWHW,
			MaterialTileRemap,
			MaterialIndirectArgs,
			MultiViewIndices,
			MultiViewRectScaleOffsets,
			ViewsBuffer,
			BasePassRenderTargets,
			Binning
		);

		const FExclusiveDepthStencil MaterialDepthStencil = UseComputeDepthExport()
			? FExclusiveDepthStencil::DepthWrite_StencilNop
			: FExclusiveDepthStencil::DepthWrite_StencilWrite;

		struct FPassParamsAndInfo
		{
			FNaniteEmitGBufferParameters Params[(uint32)ENaniteMaterialPass::Max];
			FNaniteMaterialPassInfo PassInfo[(uint32)ENaniteMaterialPass::Max];
			uint32 NumPasses = 0;
		};
		FPassParamsAndInfo* ParamsAndInfo = GraphBuilder.AllocParameters<FPassParamsAndInfo>();

		// TODO: Perhaps use visibility results to cull the secondary pass when possible?
		ParamsAndInfo->NumPasses = bWPOInSecondPass ? 2 : 1;
		for (uint32 PassIndex = 0; PassIndex < ParamsAndInfo->NumPasses; ++PassIndex)
		{
			static const EGBufferLayout PassGBufferLayouts[] =
			{
				GBL_Default,		// EmitGBuffer
				GBL_ForceVelocity	// EmitGBufferWithVelocity
			};
			static_assert(UE_ARRAY_COUNT(PassGBufferLayouts) == (uint32)ENaniteMaterialPass::Max,
						  "Unhandled Nanite material pass");

			TStaticArray<FTextureRenderTargetBinding, MaxSimultaneousRenderTargets> BasePassTextures;
			uint32 BasePassTextureCount = SceneTextures.GetGBufferRenderTargets(BasePassTextures, PassGBufferLayouts[PassIndex]);
			Strata::AppendStrataMRTs(SceneRenderer, BasePassTextureCount, BasePassTextures);
			TArrayView<FTextureRenderTargetBinding> BasePassTexturesView = MakeArrayView(BasePassTextures.GetData(), BasePassTextureCount);

			FNaniteEmitGBufferParameters& PassParams = ParamsAndInfo->Params[PassIndex];
			PassParams.Shading = TempParams;
			PassParams.RenderTargets = GetRenderTargetBindings(ERenderTargetLoadAction::ELoad, BasePassTexturesView);
			PassParams.RenderTargets.DepthStencil = FDepthStencilBinding(
				MaterialDepth,
				ERenderTargetLoadAction::ELoad,
				ERenderTargetLoadAction::ELoad,
				MaterialDepthStencil
			);
			PassParams.RenderTargets.ShadingRateTexture = GVRSImageManager.GetVariableRateShadingImage(GraphBuilder, View, FVariableRateShadingImageManager::EVRSPassType::NaniteEmitGBufferPass);
		}

		GraphBuilder.AddSetupTask([ParamsAndInfo, &MaterialCommands, &MaterialPassCommands, VisibilityResults = RasterResults.VisibilityResults /* Intentional copy */]
		{
			TArray<FGraphicsPipelineRenderTargetsInfo, TFixedAllocator<(uint32)ENaniteMaterialPass::Max>> RTInfo;
			for (uint32 PassIndex = 0; PassIndex < ParamsAndInfo->NumPasses; ++PassIndex)
			{
				RTInfo.Emplace(ExtractRenderTargetsInfo(ParamsAndInfo->Params[PassIndex].RenderTargets));
			}
			TArrayView<FNaniteMaterialPassInfo> PassInfo = MakeArrayView(ParamsAndInfo->PassInfo, ParamsAndInfo->NumPasses);
			BuildNaniteMaterialPassCommands(RTInfo, MaterialCommands, VisibilityResults, MaterialPassCommands, PassInfo);
		});

		TShaderMapRef<FNaniteIndirectMaterialVS> NaniteVertexShader(View.ShaderMap);
		const bool bParallelDispatch = GRHICommandList.UseParallelAlgorithms() && CVarParallelBasePassBuild.GetValueOnRenderThread() != 0 && FParallelMeshDrawCommandPass::IsOnDemandShaderCreationEnabled();

		if (bParallelDispatch)
		{
			static const TCHAR* const PassNames[] =
			{
				TEXT("EmitGBufferParallel"),
				TEXT("EmitGBufferWithVelocityParallel"),
			};
			static_assert(UE_ARRAY_COUNT(PassNames) == (uint32)ENaniteMaterialPass::Max);

			for (uint32 PassIndex = 0; PassIndex < ParamsAndInfo->NumPasses; ++PassIndex)
			{
				GraphBuilder.AddPass(
					FRDGEventName(PassNames[PassIndex]),
					&ParamsAndInfo->Params[PassIndex],
					ERDGPassFlags::Raster | ERDGPassFlags::SkipRenderPass,
					[ParamsAndInfo, PassIndex, &SceneRenderer, &View, ViewRect, TileCount, NaniteVertexShader, &MaterialPassCommands, MaterialIndirectArgs](const FRDGPass* Pass, FRHICommandListImmediate& RHICmdList)
				{
					if (ParamsAndInfo->PassInfo[PassIndex].NumCommands == 0)
					{
						return;
					}

					FParallelCommandListBindings CmdListBindings(&ParamsAndInfo->Params[PassIndex]);
					TConstArrayView<FNaniteMaterialPassCommand> PassCommands = MakeArrayView(MaterialPassCommands.GetData() + ParamsAndInfo->PassInfo[PassIndex].CommandOffset, ParamsAndInfo->PassInfo[PassIndex].NumCommands);
					FRDGParallelCommandListSet ParallelCommandListSet(Pass, RHICmdList, GET_STATID(STAT_CLP_NaniteBasePass), View, CmdListBindings);
					ParallelCommandListSet.SetHighPriority();
					DrawNaniteMaterialPass(&ParallelCommandListSet, RHICmdList, ViewRect, TileCount, NaniteVertexShader, MaterialIndirectArgs, PassCommands);
				});
			}
		}
		else
		{
			static const TCHAR* const PassNames[] =
			{
				TEXT("EmitGBuffer"),
				TEXT("EmitGBufferWithVelocity"),
			};
			static_assert(UE_ARRAY_COUNT(PassNames) == (uint32)ENaniteMaterialPass::Max);

			for (uint32 PassIndex = 0; PassIndex < ParamsAndInfo->NumPasses; ++PassIndex)
			{
				GraphBuilder.AddPass(
					FRDGEventName(PassNames[PassIndex]),
					&ParamsAndInfo->Params[PassIndex],
					ERDGPassFlags::Raster,
					[ParamsAndInfo, PassIndex, ViewRect = ViewRect, TileCount, NaniteVertexShader, &MaterialPassCommands, MaterialIndirectArgs](const FRDGPass* Pass, FRHICommandList& RHICmdList)
				{
					if (ParamsAndInfo->PassInfo[PassIndex].NumCommands == 0)
					{
						return;
					}

					TConstArrayView<FNaniteMaterialPassCommand> PassCommands = MakeArrayView(MaterialPassCommands.GetData() + ParamsAndInfo->PassInfo[PassIndex].CommandOffset, ParamsAndInfo->PassInfo[PassIndex].NumCommands);
					DrawNaniteMaterialPass(nullptr, RHICmdList, ViewRect, TileCount, NaniteVertexShader, MaterialIndirectArgs, PassCommands);
				});
			}
		}
	}

	ExtractShadingDebug(GraphBuilder, View, MaterialIndirectArgs, Binning, HighestMaterialSlot);
}

void EmitDepthTargets(
	FRDGBuilder& GraphBuilder,
	const FScene& Scene,
	const FViewInfo& View,
	bool bDrawSceneViewsInOneNanitePass,
	FRasterResults& RasterResults,
	FRDGTextureRef SceneDepth,
	FRDGTextureRef VelocityBuffer
)
{
	LLM_SCOPE_BYTAG(Nanite);
	RDG_EVENT_SCOPE(GraphBuilder, "Nanite::EmitDepthTargets");

	FRDGTextureRef VisBuffer64 = RasterResults.VisBuffer64;
#if WITH_EDITORONLY_DATA
	// Hide all Nanite meshes when the isolate invalid coarse mesh batch debug mode is active.
	if (GNaniteIsolateInvalidCoarseMesh != 0)
	{
		const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);
		VisBuffer64 = SystemTextures.Black;
	}
#endif

	const FSceneTexturesConfig& Config = View.GetSceneTexturesConfig();
	const EShaderPlatform ShaderPlatform = View.GetShaderPlatform();
	const FIntPoint SceneTexturesExtent = Config.Extent;
	const FClearValueBinding DefaultDepthStencil = Config.DepthClearValue;

	float DefaultDepth = 0.0f;
	uint32 DefaultStencil = 0;
	DefaultDepthStencil.GetDepthStencil(DefaultDepth, DefaultStencil);

	const uint32 StencilDecalMask = GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1);

	const bool bEmitVelocity = VelocityBuffer != nullptr;
	const bool bClearVelocity = bEmitVelocity && !HasBeenProduced(VelocityBuffer);

	FRDGTextureDesc ShadingMaskDesc = FRDGTextureDesc::Create2D(
		SceneTexturesExtent,
		PF_R32_UINT,
		FClearValueBinding::Transparent,
		TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV);

	FRDGTextureDesc MaterialDepthDesc = FRDGTextureDesc::Create2D(
		SceneTexturesExtent,
		PF_DepthStencil,
		DefaultDepthStencil,
		TexCreate_DepthStencilTargetable | TexCreate_ShaderResource | TexCreate_InputAttachmentRead | (UseComputeDepthExport() ? TexCreate_UAV : TexCreate_NoFastClear));

	RasterResults.ShadingMask = GraphBuilder.CreateTexture(ShadingMaskDesc, TEXT("Nanite.ShadingMask"));
	RasterResults.MaterialDepth = UseLegacyCulling() ? GraphBuilder.CreateTexture(MaterialDepthDesc, TEXT("Nanite.MaterialDepth")) : nullptr;

	RasterResults.ClearTileArgs = nullptr;
	RasterResults.ClearTileBuffer = nullptr;

	if (UseComputeDepthExport())
	{
		// Emit depth, stencil, mask and velocity

		if (GNaniteDecompressDepth != 0)
		{
			// Force depth decompression so the depth shader only processes decompressed surfaces
			FDummyDepthDecompressParameters* DecompressParams = GraphBuilder.AllocParameters<FDummyDepthDecompressParameters>();
			DecompressParams->SceneDepth = SceneDepth;

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("NaniteDepthDecompress"),
				DecompressParams,
				ERDGPassFlags::Copy | ERDGPassFlags::NeverCull,
				[](FRHICommandList&) {}
			);
		}

		const FIntRect ViewRect = bDrawSceneViewsInOneNanitePass ? View.GetFamilyViewRect() : View.ViewRect;
		const int32 kHTileSize = 8;
		checkf((ViewRect.Min.X % kHTileSize) == 0 && (ViewRect.Min.Y % kHTileSize) == 0, TEXT("Viewport rect must be %d-pixel aligned."), kHTileSize);

		const FIntVector DispatchDim = FComputeShaderUtils::GetGroupCount(ViewRect.Size(), kHTileSize);
		const uint32 PlatformConfig = RHIGetHTilePlatformConfig(SceneTexturesExtent.X, SceneTexturesExtent.Y);

		FRDGTextureUAVRef SceneDepthUAV			= GraphBuilder.CreateUAV(FRDGTextureUAVDesc::CreateForMetaData(SceneDepth, ERDGTextureMetaDataAccess::CompressedSurface));
		FRDGTextureUAVRef SceneStencilUAV		= GraphBuilder.CreateUAV(FRDGTextureUAVDesc::CreateForMetaData(SceneDepth, ERDGTextureMetaDataAccess::Stencil));
		FRDGTextureUAVRef SceneHTileUAV			= GraphBuilder.CreateUAV(FRDGTextureUAVDesc::CreateForMetaData(SceneDepth, ERDGTextureMetaDataAccess::HTile));
		FRDGTextureUAVRef MaterialDepthUAV		= UseLegacyCulling() ? GraphBuilder.CreateUAV(FRDGTextureUAVDesc::CreateForMetaData(RasterResults.MaterialDepth, ERDGTextureMetaDataAccess::CompressedSurface)) : nullptr;
		FRDGTextureUAVRef MaterialHTileUAV		= UseLegacyCulling() ? GraphBuilder.CreateUAV(FRDGTextureUAVDesc::CreateForMetaData(RasterResults.MaterialDepth, ERDGTextureMetaDataAccess::HTile)) : nullptr;
		FRDGTextureUAVRef VelocityUAV			= bEmitVelocity ? GraphBuilder.CreateUAV(VelocityBuffer) : nullptr;
		FRDGTextureUAVRef ShadingMaskUAV		= GraphBuilder.CreateUAV(RasterResults.ShadingMask);

		FDepthExportCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDepthExportCS::FParameters>();

		PassParameters->View					= View.GetShaderParameters();
		PassParameters->Scene					= View.GetSceneUniforms().GetBuffer(GraphBuilder);
		PassParameters->InViews					= GraphBuilder.CreateSRV(RasterResults.ViewsBuffer);
		PassParameters->VisibleClustersSWHW		= GraphBuilder.CreateSRV(RasterResults.VisibleClustersSWHW);
		PassParameters->PageConstants			= RasterResults.PageConstants;
		PassParameters->ClusterPageData			= Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
		PassParameters->DepthExportConfig		= FIntVector4(PlatformConfig, SceneTexturesExtent.X, StencilDecalMask, Nanite::FGlobalResources::GetMaxVisibleClusters());
		PassParameters->ViewRect				= FUint32Vector4((uint32)ViewRect.Min.X, (uint32)ViewRect.Min.Y, (uint32)ViewRect.Max.X, (uint32)ViewRect.Max.Y);
		PassParameters->bWriteCustomStencil		= false;
		PassParameters->VisBuffer64				= VisBuffer64;
		PassParameters->Velocity				= VelocityUAV;
		PassParameters->ShadingMask				= ShadingMaskUAV;
		PassParameters->SceneHTile				= SceneHTileUAV;
		PassParameters->SceneDepth				= SceneDepthUAV;
		PassParameters->SceneStencil			= SceneStencilUAV;
		PassParameters->MaterialHTile			= MaterialHTileUAV;
		PassParameters->MaterialDepth			= MaterialDepthUAV;
		PassParameters->MaterialSlotTable		= Scene.NaniteMaterials[ENaniteMeshPass::BasePass].GetMaterialSlotSRV();
		PassParameters->MaterialDepthTable		= UseLegacyCulling() ? Scene.NaniteMaterials[ENaniteMeshPass::BasePass].GetMaterialDepthSRV() : nullptr;

		FDepthExportCS::FPermutationDomain PermutationVectorCS;
		PermutationVectorCS.Set<FDepthExportCS::FLegacyCullingDim>(UseLegacyCulling());
		PermutationVectorCS.Set<FDepthExportCS::FVelocityExportDim>(bEmitVelocity);
		PermutationVectorCS.Set<FDepthExportCS::FMaterialDepthExportDim>(UseLegacyCulling());
		PermutationVectorCS.Set<FDepthExportCS::FShadingMaskExportDim>(true);
		auto ComputeShader = View.ShaderMap->GetShader<FDepthExportCS>(PermutationVectorCS);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("DepthExport"),
			ComputeShader,
			PassParameters,
			DispatchDim
		);
	}
	else
	{
		// Can't use ERenderTargetLoadAction::EClear to clear here because it needs to be the same for all render targets.
		AddClearRenderTargetPass(GraphBuilder, RasterResults.ShadingMask);
		if (bClearVelocity)
		{
			AddClearRenderTargetPass(GraphBuilder, VelocityBuffer);
		}

		// Emit scene depth buffer, mask and velocity
		{
			FEmitSceneDepthPS::FPermutationDomain PermutationVectorPS;
			PermutationVectorPS.Set<FEmitSceneDepthPS::FLegacyCullingDim>(UseLegacyCulling());
			PermutationVectorPS.Set<FEmitSceneDepthPS::FVelocityExportDim>(bEmitVelocity);
			PermutationVectorPS.Set<FEmitSceneDepthPS::FShadingMaskExportDim>(true);
			auto  PixelShader = View.ShaderMap->GetShader<FEmitSceneDepthPS>(PermutationVectorPS);
			
			auto* PassParameters = GraphBuilder.AllocParameters<FEmitSceneDepthPS::FParameters>();

			PassParameters->View						= View.GetShaderParameters();
			PassParameters->Scene						= View.GetSceneUniforms().GetBuffer(GraphBuilder);
			PassParameters->InViews						= GraphBuilder.CreateSRV(RasterResults.ViewsBuffer);
			PassParameters->VisibleClustersSWHW			= GraphBuilder.CreateSRV(RasterResults.VisibleClustersSWHW);
			PassParameters->PageConstants				= RasterResults.PageConstants;
			PassParameters->VisBuffer64					= VisBuffer64;
			PassParameters->ClusterPageData				= Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
			PassParameters->MaterialSlotTable			= Scene.NaniteMaterials[ENaniteMeshPass::BasePass].GetMaterialSlotSRV();
			PassParameters->RenderTargets[0]			= FRenderTargetBinding(RasterResults.ShadingMask, ERenderTargetLoadAction::ELoad);
			PassParameters->RenderTargets[1]			= bEmitVelocity ? FRenderTargetBinding(VelocityBuffer, ERenderTargetLoadAction::ELoad) : FRenderTargetBinding();
			PassParameters->RenderTargets.DepthStencil	= FDepthStencilBinding(SceneDepth, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthWrite_StencilWrite);

			FPixelShaderUtils::AddFullscreenPass(
				GraphBuilder,
				View.ShaderMap,
				RDG_EVENT_NAME("Emit Scene Depth/Resolve/Velocity"),
				PixelShader,
				PassParameters,
				bDrawSceneViewsInOneNanitePass ? View.GetFamilyViewRect() : View.ViewRect,
				TStaticBlendState<>::GetRHI(),
				TStaticRasterizerState<>::GetRHI(),
				TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI()
			);
		}

		// Emit scene stencil
		{
			auto  PixelShader		= View.ShaderMap->GetShader<FEmitSceneStencilPS>();
			auto* PassParameters	= GraphBuilder.AllocParameters<FEmitSceneStencilPS::FParameters>();

			PassParameters->View						= View.ViewUniformBuffer;
			PassParameters->Scene						= View.GetSceneUniforms().GetBuffer(GraphBuilder);
			PassParameters->VisibleClustersSWHW			= GraphBuilder.CreateSRV(RasterResults.VisibleClustersSWHW);
			PassParameters->PageConstants				= RasterResults.PageConstants;
			PassParameters->ClusterPageData				= Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
			PassParameters->ShadingMask					= RasterResults.ShadingMask;
			PassParameters->VisBuffer64					= VisBuffer64;
			PassParameters->RenderTargets.DepthStencil	= FDepthStencilBinding
			(
				SceneDepth,
				ERenderTargetLoadAction::ELoad,
				FExclusiveDepthStencil::DepthWrite_StencilWrite
			);

			FPixelShaderUtils::AddFullscreenPass(
				GraphBuilder,
				View.ShaderMap,
				RDG_EVENT_NAME("Emit Scene Stencil"),
				PixelShader,
				PassParameters,
				View.ViewRect,
				TStaticBlendState<>::GetRHI(),
				TStaticRasterizerState<>::GetRHI(),
				TStaticDepthStencilState<false, CF_DepthNearOrEqual, true, CF_Always, SO_Keep, SO_Keep, SO_Replace>::GetRHI(),
				StencilDecalMask | GET_STENCIL_BIT_MASK(DISTANCE_FIELD_REPRESENTATION, 1)
			);
		}

		// Emit material depth for pixels produced from Nanite rasterization.
		if (UseLegacyCulling())
		{
			auto* PassParameters = GraphBuilder.AllocParameters<FEmitMaterialDepthPS::FParameters>();

			PassParameters->DummyZero = 0u;
			PassParameters->ClusterPageData				= Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
			PassParameters->MaterialSlotTable			= Scene.NaniteMaterials[ENaniteMeshPass::BasePass].GetMaterialSlotSRV();
			PassParameters->MaterialDepthTable			= Scene.NaniteMaterials[ENaniteMeshPass::BasePass].GetMaterialDepthSRV();
			PassParameters->PageConstants				= RasterResults.PageConstants;
			PassParameters->View						= View.ViewUniformBuffer;
			PassParameters->ShadingMask					= RasterResults.ShadingMask;
			PassParameters->VisBuffer64					= VisBuffer64;
			PassParameters->VisibleClustersSWHW			= GraphBuilder.CreateSRV(RasterResults.VisibleClustersSWHW);
			PassParameters->RenderTargets.DepthStencil	= FDepthStencilBinding(
				RasterResults.MaterialDepth,
				ERenderTargetLoadAction::EClear,
				FExclusiveDepthStencil::DepthWrite_StencilWrite
			);

			FEmitMaterialDepthPS::FPermutationDomain PermutationVectorPS;
			PermutationVectorPS.Set<FEmitMaterialDepthPS::FLegacyCullingDim>(UseLegacyCulling());
			PermutationVectorPS.Set<FEmitMaterialDepthPS::FShadingMaskLoadDim>(true /* using shading mask */);
			auto PixelShader = View.ShaderMap->GetShader<FEmitMaterialDepthPS>(PermutationVectorPS);

			FPixelShaderUtils::AddFullscreenPass(
				GraphBuilder,
				View.ShaderMap,
				RDG_EVENT_NAME("Emit Material Depth"),
				PixelShader,
				PassParameters,
				bDrawSceneViewsInOneNanitePass ? View.GetFamilyViewRect() : View.ViewRect,
				TStaticBlendState<>::GetRHI(),
				TStaticRasterizerState<>::GetRHI(),
				TStaticDepthStencilState<true, CF_Always>::GetRHI(),
				0u
			);
		}

		if (GRHISupportsResummarizeHTile && GNaniteResummarizeHTile != 0)
		{
			// Resummarize HTile meta data if the RHI supports it
			AddResummarizeHTilePass(GraphBuilder, SceneDepth);
		}
	}
}

FCustomDepthContext InitCustomDepthStencilContext(
	FRDGBuilder& GraphBuilder,
	const FCustomDepthTextures& CustomDepthTextures,
	bool bWriteCustomStencil
)
{
	enum ECustomDepthExportMethod
	{
		DepthExportSeparatePS,	// Emit depth & stencil from PS (Stencil separated and written to RT0)
		DepthExportCS			// Emit depth & stencil from CS with HTILE (requires RHI support)
	};

	check(CustomDepthTextures.IsValid());

	const FIntPoint CustomDepthExtent = CustomDepthTextures.Depth->Desc.Extent;

	FCustomDepthContext Output;
	Output.InputDepth = CustomDepthTextures.Depth;
	Output.InputStencilSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateWithPixelFormat(CustomDepthTextures.Depth, PF_X24_G8));
	Output.bComputeExport = UseComputeDepthExport() && GNaniteCustomDepthExportMethod == DepthExportCS;

	if (Output.bComputeExport)
	{
		// We can output directly to the depth target using compute
		Output.DepthTarget = CustomDepthTextures.Depth;
		Output.StencilTarget = bWriteCustomStencil ? CustomDepthTextures.Depth : nullptr;
	}
	else
	{
		// Since we cannot output the stencil ref from the pixel shader, we'll combine Nanite and non-Nanite custom depth/stencil
		// into new, separate targets. Note that stencil test using custom stencil from this point will require tests to be
		// performed manually in the pixel shader (see PostProcess materials, for example).
		FRDGTextureDesc OutCustomDepthDesc = FRDGTextureDesc::Create2D(
			CustomDepthExtent,
			PF_DepthStencil,
			FClearValueBinding::DepthFar,
			TexCreate_DepthStencilTargetable | TexCreate_ShaderResource);
		Output.DepthTarget = GraphBuilder.CreateTexture(OutCustomDepthDesc, TEXT("CombinedCustomDepth"));

		if (bWriteCustomStencil)
		{
			FRDGTextureDesc OutCustomStencilDesc = FRDGTextureDesc::Create2D(
				CustomDepthExtent,
				PF_R16G16_UINT, //PF_R8G8_UINT,
				FClearValueBinding::Transparent,
				TexCreate_RenderTargetable | TexCreate_ShaderResource);

			Output.StencilTarget = GraphBuilder.CreateTexture(OutCustomStencilDesc, TEXT("CombinedCustomStencil"));
		}
	}

	return Output;
}

void EmitCustomDepthStencilTargets(
	FRDGBuilder& GraphBuilder,
	const FScene& Scene,
	const FViewInfo& View,
	const FIntVector4& PageConstants,
	FRDGBufferRef VisibleClustersSWHW,
	FRDGBufferRef ViewsBuffer,
	FRDGTextureRef VisBuffer64,
	const FCustomDepthContext& CustomDepthContext
)
{
	LLM_SCOPE_BYTAG(Nanite);
	RDG_EVENT_SCOPE(GraphBuilder, "Nanite::EmitCustomDepthStencilTargets");

	FRDGTextureRef CustomDepth = CustomDepthContext.InputDepth;
	FRDGTextureSRVRef CustomStencilSRV = CustomDepthContext.InputStencilSRV;
	const FIntPoint CustomDepthExtent = CustomDepth->Desc.Extent;
	const bool bWriteCustomStencil = CustomDepthContext.StencilTarget != nullptr;

	if (CustomDepthContext.bComputeExport)
	{
		// Emit custom depth and stencil from a CS that can handle HTILE
		if (GNaniteDecompressDepth != 0)
		{
			// Force depth decompression so the depth shader only processes decompressed surfaces
			FDummyDepthDecompressParameters* DecompressParams = GraphBuilder.AllocParameters<FDummyDepthDecompressParameters>();
			DecompressParams->SceneDepth = CustomDepth;

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("NaniteCustomDepthDecompress"),
				DecompressParams,
				ERDGPassFlags::Copy | ERDGPassFlags::NeverCull,
				[](FRHICommandList&) {}
			);
		}

		// TODO: Don't currently support offset views.
		checkf(View.ViewRect.Min.X == 0 && View.ViewRect.Min.Y == 0, TEXT("Viewport offset support is not implemented."));

		// Export depth
		{
			const FIntVector DispatchDim = FComputeShaderUtils::GetGroupCount(View.ViewRect.Max, 8); // Only run DepthExport shader on viewport. We have already asserted that ViewRect.Min=0.
			const uint32 PlatformConfig = RHIGetHTilePlatformConfig(CustomDepthExtent.X, CustomDepthExtent.Y);

			FRDGTextureUAVRef CustomDepthUAV		= GraphBuilder.CreateUAV(FRDGTextureUAVDesc::CreateForMetaData(CustomDepth, ERDGTextureMetaDataAccess::CompressedSurface));
			FRDGTextureUAVRef CustomStencilUAV		= GraphBuilder.CreateUAV(FRDGTextureUAVDesc::CreateForMetaData(CustomDepth, ERDGTextureMetaDataAccess::Stencil));
			FRDGTextureUAVRef CustomHTileUAV		= GraphBuilder.CreateUAV(FRDGTextureUAVDesc::CreateForMetaData(CustomDepth, ERDGTextureMetaDataAccess::HTile));

			FDepthExportCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDepthExportCS::FParameters>();

			PassParameters->View					= View.GetShaderParameters();
			PassParameters->Scene					= View.GetSceneUniforms().GetBuffer(GraphBuilder);
			PassParameters->InViews					= GraphBuilder.CreateSRV(ViewsBuffer);
			PassParameters->VisibleClustersSWHW		= GraphBuilder.CreateSRV(VisibleClustersSWHW);
			PassParameters->PageConstants			= PageConstants;
			PassParameters->ClusterPageData			= Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
			PassParameters->DepthExportConfig		= FIntVector4(PlatformConfig, CustomDepthExtent.X, 0, Nanite::FGlobalResources::GetMaxVisibleClusters());
			PassParameters->ViewRect				= FUint32Vector4((uint32)View.ViewRect.Min.X, (uint32)View.ViewRect.Min.Y, (uint32)View.ViewRect.Max.X, (uint32)View.ViewRect.Max.Y);
			PassParameters->bWriteCustomStencil		= bWriteCustomStencil;
			PassParameters->VisBuffer64				= VisBuffer64;
			PassParameters->Velocity				= nullptr;
			PassParameters->ShadingMask				= nullptr;
			PassParameters->SceneHTile				= CustomHTileUAV;
			PassParameters->SceneDepth				= CustomDepthUAV;
			PassParameters->SceneStencil			= CustomStencilUAV;
			PassParameters->MaterialHTile			= nullptr;
			PassParameters->MaterialDepth			= nullptr;
			PassParameters->MaterialSlotTable		= Scene.NaniteMaterials[ENaniteMeshPass::BasePass].GetMaterialSlotSRV();
			PassParameters->MaterialDepthTable		= Scene.NaniteMaterials[ENaniteMeshPass::BasePass].GetMaterialDepthSRV();

			FDepthExportCS::FPermutationDomain PermutationVectorCS;
			PermutationVectorCS.Set<FDepthExportCS::FLegacyCullingDim>(UseLegacyCulling());
			PermutationVectorCS.Set<FDepthExportCS::FVelocityExportDim>(false);
			PermutationVectorCS.Set<FDepthExportCS::FMaterialDepthExportDim>(false);
			PermutationVectorCS.Set<FDepthExportCS::FShadingMaskExportDim>(false);
			auto ComputeShader = View.ShaderMap->GetShader<FDepthExportCS>(PermutationVectorCS);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("DepthExport"),
				ComputeShader,
				PassParameters,
				DispatchDim
			);
		}
	}
	else // DepthExportSeparatePS
	{
		FRDGTextureRef OutCustomDepth = CustomDepthContext.DepthTarget;
		FRDGTextureRef OutCustomStencil = CustomDepthContext.StencilTarget;

		FEmitCustomDepthStencilPS::FPermutationDomain PermutationVectorPS;
		PermutationVectorPS.Set<FEmitCustomDepthStencilPS::FWriteCustomStencilDim>(bWriteCustomStencil);
		auto PixelShader = View.ShaderMap->GetShader<FEmitCustomDepthStencilPS>(PermutationVectorPS);

		auto* PassParameters = GraphBuilder.AllocParameters<FEmitCustomDepthStencilPS::FParameters>();

		// If we aren't emitting stencil, clear it so it's not garbage
		ERenderTargetLoadAction StencilLoadAction = OutCustomStencil ? ERenderTargetLoadAction::ENoAction : ERenderTargetLoadAction::EClear;

		PassParameters->View						= View.ViewUniformBuffer;
		PassParameters->Scene						= View.GetSceneUniforms().GetBuffer(GraphBuilder);
		PassParameters->InViews						= GraphBuilder.CreateSRV(ViewsBuffer);
		PassParameters->VisibleClustersSWHW			= GraphBuilder.CreateSRV(VisibleClustersSWHW);
		PassParameters->PageConstants				= PageConstants;
		PassParameters->VisBuffer64					= VisBuffer64;
		PassParameters->ClusterPageData				= Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
		PassParameters->CustomDepth					= CustomDepth;
		PassParameters->CustomStencil				= CustomStencilSRV;
		PassParameters->RenderTargets[0]			= OutCustomStencil ? FRenderTargetBinding(OutCustomStencil, ERenderTargetLoadAction::ENoAction) : FRenderTargetBinding();
		PassParameters->RenderTargets.DepthStencil	= FDepthStencilBinding(OutCustomDepth, ERenderTargetLoadAction::ENoAction, StencilLoadAction, FExclusiveDepthStencil::DepthWrite_StencilNop);

		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder,
			View.ShaderMap,
			OutCustomStencil ? RDG_EVENT_NAME("Emit Custom Depth/Stencil") : RDG_EVENT_NAME("Emit Custom Depth"),
			PixelShader,
			PassParameters,
			View.ViewRect,
			TStaticBlendState<>::GetRHI(),
			TStaticRasterizerState<>::GetRHI(),
			TStaticDepthStencilState<true, CF_Always>::GetRHI()
		);
	}
}

void FinalizeCustomDepthStencil(
	FRDGBuilder& GraphBuilder,
	const FCustomDepthContext& CustomDepthContext,
	FCustomDepthTextures& OutTextures
)
{
	OutTextures.Depth = CustomDepthContext.DepthTarget;
	if (CustomDepthContext.StencilTarget)
	{
		if (CustomDepthContext.bComputeExport)
		{
			// we wrote straight to the depth/stencil buffer
			OutTextures.Stencil = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateWithPixelFormat(CustomDepthContext.StencilTarget, PF_X24_G8));
		}
		else
		{
			// separate stencil texture
			OutTextures.Stencil = GraphBuilder.CreateSRV(CustomDepthContext.StencilTarget);
		}
	}
	else
	{
		OutTextures.Stencil = CustomDepthContext.InputStencilSRV;
	}
	OutTextures.bSeparateStencilBuffer = !CustomDepthContext.bComputeExport;
}

struct FLumenMeshCaptureMaterialPassIndex
{
	FLumenMeshCaptureMaterialPassIndex(int32 InIndex, int32 InCommandStateBucketId)
		: Index(InIndex)
		, CommandStateBucketId(InCommandStateBucketId)
	{
	}

	inline friend uint32 GetTypeHash(const FLumenMeshCaptureMaterialPassIndex& PassIndex)
	{
		return CityHash32((const char*)&PassIndex.CommandStateBucketId, sizeof(PassIndex.CommandStateBucketId));
	}

	inline bool operator==(const FLumenMeshCaptureMaterialPassIndex& PassIndex) const
	{
		return CommandStateBucketId == PassIndex.CommandStateBucketId;
	}

	int32 Index = -1;
	int32 CommandStateBucketId = -1;
};

struct FLumenMeshCaptureMaterialPass
{
	uint64 SortKey = 0;
	int32 CommandStateBucketId = INDEX_NONE;
	uint32 ViewIndexBufferOffset = 0;
	TArray<uint16, TInlineAllocator<64>> ViewIndices;

	inline float GetMaterialDepth() const
	{
		return FNaniteCommandInfo::GetDepthId(CommandStateBucketId);
	}

	bool operator<(const FLumenMeshCaptureMaterialPass& Other) const
	{
		return SortKey < Other.SortKey;
	}
};

void DrawLumenMeshCapturePass(
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
	RDG_EVENT_SCOPE(GraphBuilder, "Nanite::DrawLumenMeshCapturePass");
	TRACE_CPUPROFILER_EVENT_SCOPE(NaniteDraw_LumenMeshCapturePass);

	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);

	// Material tile remap buffer (currently not used by Lumen, but still must be bound)
	FRDGBufferRef MaterialTileRemap = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(4, 1), TEXT("Nanite.MaterialTileRemap"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(MaterialTileRemap), 0);

	// Mark stencil for all pixels that pass depth test
	{
		FNaniteMarkStencilRectsParameters* PassParameters = GraphBuilder.AllocParameters<FNaniteMarkStencilRectsParameters>();

		PassParameters->PS.View = SharedView->ViewUniformBuffer;
		PassParameters->PS.VisBuffer64 = RasterContext.VisBuffer64;

		PassParameters->PS.RenderTargets.DepthStencil = FDepthStencilBinding(
			DepthAtlasTexture,
			ERenderTargetLoadAction::ELoad,
			ERenderTargetLoadAction::ELoad,
			FExclusiveDepthStencil::DepthRead_StencilWrite
		);
		
		auto PixelShader = SharedView->ShaderMap->GetShader<FNaniteMarkStencilPS>();

		FPixelShaderUtils::AddRasterizeToRectsPass(GraphBuilder,
			SharedView->ShaderMap,
			RDG_EVENT_NAME("Mark Stencil"),
			PixelShader,
			PassParameters,
			ViewportSize,
			RectMinMaxBufferSRV,
			NumRects,
			TStaticBlendState<>::GetRHI(),
			TStaticRasterizerState<>::GetRHI(),
			TStaticDepthStencilState<false, CF_DepthNearOrEqual, true, CF_Always, SO_Keep, SO_Keep, SO_Replace>::GetRHI(),
			STENCIL_SANDBOX_MASK
		);
	}

	// Emit material IDs as depth values
	{
		FNaniteEmitMaterialIdRectsParameters* PassParameters = GraphBuilder.AllocParameters<FNaniteEmitMaterialIdRectsParameters>();

		PassParameters->PS.View = SharedView->ViewUniformBuffer;
		PassParameters->PS.Scene = SharedView->GetSceneUniforms().GetBuffer(GraphBuilder);
		PassParameters->PS.DummyZero = 0u;

		PassParameters->PS.VisibleClustersSWHW = GraphBuilder.CreateSRV(RasterResults.VisibleClustersSWHW);
		PassParameters->PS.PageConstants = RasterResults.PageConstants;
		PassParameters->PS.ClusterPageData = GStreamingManager.GetClusterPageDataSRV(GraphBuilder);

		PassParameters->PS.VisBuffer64 = RasterContext.VisBuffer64;

		PassParameters->PS.MaterialSlotTable	= Scene.NaniteMaterials[ENaniteMeshPass::LumenCardCapture].GetMaterialSlotSRV();
		PassParameters->PS.MaterialDepthTable	= Scene.NaniteMaterials[ENaniteMeshPass::LumenCardCapture].GetMaterialDepthSRV();

		PassParameters->PS.RenderTargets.DepthStencil = FDepthStencilBinding(
			DepthAtlasTexture,
			ERenderTargetLoadAction::ELoad,
			ERenderTargetLoadAction::ELoad,
			FExclusiveDepthStencil::DepthWrite_StencilRead
		);

		FEmitMaterialDepthPS::FPermutationDomain PermutationVectorPS;
		PermutationVectorPS.Set<FEmitMaterialDepthPS::FLegacyCullingDim>(UseLegacyCulling());
		PermutationVectorPS.Set<FEmitMaterialDepthPS::FShadingMaskLoadDim>(false /* not using shading mask */);
		auto PixelShader = SharedView->ShaderMap->GetShader<FEmitMaterialDepthPS>(PermutationVectorPS);

		FPixelShaderUtils::AddRasterizeToRectsPass(GraphBuilder,
			SharedView->ShaderMap,
			RDG_EVENT_NAME("Emit Material Depth"),
			PixelShader,
			PassParameters,
			ViewportSize,
			RectMinMaxBufferSRV,
			NumRects,
			TStaticBlendState<>::GetRHI(),
			TStaticRasterizerState<>::GetRHI(),
			TStaticDepthStencilState<true, CF_Always, true, CF_Equal>::GetRHI(),
			STENCIL_SANDBOX_MASK
		);
	}

	// Emit GBuffer Values
	{
		FNaniteEmitGBufferParameters* PassParameters = GraphBuilder.AllocParameters<FNaniteEmitGBufferParameters>();

		PassParameters->RenderTargets[0] = FRenderTargetBinding(AlbedoAtlasTexture, ERenderTargetLoadAction::ELoad);
		PassParameters->RenderTargets[1] = FRenderTargetBinding(NormalAtlasTexture, ERenderTargetLoadAction::ELoad);
		PassParameters->RenderTargets[2] = FRenderTargetBinding(EmissiveAtlasTexture, ERenderTargetLoadAction::ELoad);

		PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(
			DepthAtlasTexture,
			ERenderTargetLoadAction::ELoad,
			ERenderTargetLoadAction::ELoad,
			FExclusiveDepthStencil::DepthWrite_StencilRead
		);

		struct FMaterialPassBuildContext
		{
			int32 NumMaterialQuads = 0;
			TArray<FLumenMeshCaptureMaterialPass, SceneRenderingAllocator> MaterialPasses;
			TArray<uint32, SceneRenderingAllocator> ViewIndices;
			TArray<FVector4f, SceneRenderingAllocator> ViewRectScaleOffsets;
			TArray<Nanite::FPackedView, SceneRenderingAllocator> PackedViews;
		};

		FMaterialPassBuildContext& BuildContext = *GraphBuilder.AllocObject<FMaterialPassBuildContext>();

		GraphBuilder.AddSetupTask([&BuildContext, CardPagesToRender, PassParameters, &Scene, ViewportSize]
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(BuildLumenMeshCaptureMaterialPasses);

			BuildContext.MaterialPasses.Reserve(CardPagesToRender.Num());
			BuildContext.ViewRectScaleOffsets.Reserve(CardPagesToRender.Num());
			BuildContext.PackedViews.Reserve(CardPagesToRender.Num());

			// Build list of unique materials
			{
				FGraphicsPipelineRenderTargetsInfo RenderTargetsInfo = ExtractRenderTargetsInfo(PassParameters->RenderTargets);

				Experimental::TRobinHoodHashSet<FLumenMeshCaptureMaterialPassIndex> MaterialPassSet;

				for (int32 CardPageIndex = 0; CardPageIndex < CardPagesToRender.Num(); ++CardPageIndex)
				{
					const FCardPageRenderData& CardPageRenderData = CardPagesToRender[CardPageIndex];

					for (const FNaniteCommandInfo& CommandInfo : CardPageRenderData.NaniteCommandInfos)
					{
						const FLumenMeshCaptureMaterialPassIndex& PassIndex = *MaterialPassSet.FindOrAdd(FLumenMeshCaptureMaterialPassIndex(BuildContext.MaterialPasses.Num(), CommandInfo.GetStateBucketId()));

						if (PassIndex.Index >= BuildContext.MaterialPasses.Num())
						{
							const FNaniteMaterialCommands& LumenMaterialCommands = Scene.NaniteMaterials[ENaniteMeshPass::LumenCardCapture];
							FNaniteMaterialCommands::FCommandId CommandId(CommandInfo.GetStateBucketId());
							const FMeshDrawCommand& MeshDrawCommand = LumenMaterialCommands.GetCommand(CommandId);

							FLumenMeshCaptureMaterialPass MaterialPass;
							MaterialPass.SortKey = MeshDrawCommand.GetPipelineStateSortingKey(RenderTargetsInfo);
							MaterialPass.CommandStateBucketId = CommandInfo.GetStateBucketId();
							MaterialPass.ViewIndexBufferOffset = 0;
							BuildContext.MaterialPasses.Add(MaterialPass);
						}

						BuildContext.MaterialPasses[PassIndex.Index].ViewIndices.Add(CardPageIndex);
						++BuildContext.NumMaterialQuads;
					}
				}
				ensure(BuildContext.MaterialPasses.Num() > 0);
			}

			if (BuildContext.MaterialPasses.Num() > 0)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(Sort);
				BuildContext.MaterialPasses.Sort();
			}

			BuildContext.ViewIndices.Reserve(BuildContext.NumMaterialQuads);

			for (FLumenMeshCaptureMaterialPass& MaterialPass : BuildContext.MaterialPasses)
			{
				MaterialPass.ViewIndexBufferOffset = BuildContext.ViewIndices.Num();

				for (int32 ViewIndex : MaterialPass.ViewIndices)
				{
					BuildContext.ViewIndices.Add(ViewIndex);
				}
			}
			ensure(BuildContext.ViewIndices.Num() > 0);

			const FVector2f ViewportSizeF = FVector2f(float(ViewportSize.X), float(ViewportSize.Y));

			for (const FCardPageRenderData& CardPageRenderData : CardPagesToRender)
			{
				const FVector2f CardViewportSize = FVector2f(float(CardPageRenderData.CardCaptureAtlasRect.Width()), float(CardPageRenderData.CardCaptureAtlasRect.Height()));
				const FVector2f RectOffset = FVector2f(float(CardPageRenderData.CardCaptureAtlasRect.Min.X), float(CardPageRenderData.CardCaptureAtlasRect.Min.Y)) / ViewportSizeF;
				const FVector2f RectScale = CardViewportSize / ViewportSizeF;

				BuildContext.ViewRectScaleOffsets.Add(FVector4f(RectScale, RectOffset));

				Nanite::FPackedViewParams Params;
				Params.ViewMatrices = CardPageRenderData.ViewMatrices;
				Params.PrevViewMatrices = CardPageRenderData.ViewMatrices;
				Params.ViewRect = CardPageRenderData.CardCaptureAtlasRect;
				Params.RasterContextSize = ViewportSize;
				Params.LODScaleFactor = 0.0f;
				Params.MaxPixelsPerEdgeMultipler = 1.0f;

				BuildContext.PackedViews.Add(Nanite::CreatePackedView(Params));
			}
		});

		FRDGBuffer* ViewIndexBuffer = CreateStructuredBuffer(
			GraphBuilder,
			TEXT("Nanite.ViewIndices"),
			BuildContext.ViewIndices.GetTypeSize(),
			[&ViewIndices = BuildContext.ViewIndices] { return FMath::RoundUpToPowerOfTwo(ViewIndices.Num()); },
			[&ViewIndices = BuildContext.ViewIndices] { return ViewIndices.GetData(); },
			[&ViewIndices = BuildContext.ViewIndices] { return ViewIndices.Num() * ViewIndices.GetTypeSize(); }
		);

		FRDGBuffer* ViewRectScaleOffsetBuffer = CreateStructuredBuffer(
			GraphBuilder,
			TEXT("Nanite.ViewRectScaleOffset"),
			BuildContext.ViewRectScaleOffsets.GetTypeSize(),
			[&ViewRectScaleOffsets = BuildContext.ViewRectScaleOffsets] { return FMath::RoundUpToPowerOfTwo(ViewRectScaleOffsets.Num()); },
			[&ViewRectScaleOffsets = BuildContext.ViewRectScaleOffsets] { return ViewRectScaleOffsets.GetData(); },
			[&ViewRectScaleOffsets = BuildContext.ViewRectScaleOffsets] { return ViewRectScaleOffsets.Num() * ViewRectScaleOffsets.GetTypeSize(); }
		);

		FRDGBuffer* PackedViewBuffer = CreateStructuredBuffer(
			GraphBuilder,
			TEXT("Nanite.PackedViews"),
			BuildContext.PackedViews.GetTypeSize(),
			[&PackedViews = BuildContext.PackedViews] { return FMath::RoundUpToPowerOfTwo(PackedViews.Num()); },
			[&PackedViews = BuildContext.PackedViews] { return PackedViews.GetData(); },
			[&PackedViews = BuildContext.PackedViews] { return PackedViews.Num() * PackedViews.GetTypeSize(); }
		);

		{
			FNaniteUniformParameters* UniformParameters		= GraphBuilder.AllocParameters<FNaniteUniformParameters>();
			UniformParameters->PageConstants				= RasterResults.PageConstants;
			UniformParameters->MaxNodes						= Nanite::FGlobalResources::GetMaxNodes();
			UniformParameters->MaxVisibleClusters			= Nanite::FGlobalResources::GetMaxVisibleClusters();
			UniformParameters->RenderFlags					= RasterResults.RenderFlags;
			UniformParameters->MaterialConfig				= FIntVector4(0, 1, 1, 0); // Tile based material culling is not required for Lumen, as each card is rendered as a small rect
			UniformParameters->RectScaleOffset				= FVector4f(1.0f, 1.0f, 0.0f, 0.0f); // This will be overridden in vertex shader

			UniformParameters->ClusterPageData				= Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
			UniformParameters->HierarchyBuffer				= Nanite::GStreamingManager.GetHierarchySRV(GraphBuilder);
			UniformParameters->VisibleClustersSWHW			= GraphBuilder.CreateSRV(RasterResults.VisibleClustersSWHW);
			UniformParameters->MaterialTileRemap			= GraphBuilder.CreateSRV(MaterialTileRemap, PF_R32_UINT);

#if RHI_RAYTRACING
			UniformParameters->RayTracingCutError			= Nanite::GRayTracingManager.GetCutError();
			UniformParameters->RayTracingDataBuffer			= Nanite::GRayTracingManager.GetAuxiliaryDataSRV(GraphBuilder);
#else
			UniformParameters->RayTracingCutError			= 0.0f;
			UniformParameters->RayTracingDataBuffer			= GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<uint32>(GraphBuilder));
#endif

			UniformParameters->VisBuffer64					= RasterContext.VisBuffer64;
			UniformParameters->DbgBuffer64					= SystemTextures.Black;
			UniformParameters->DbgBuffer32					= SystemTextures.Black;
			UniformParameters->ShadingMask					= SystemTextures.Black;
			UniformParameters->MaterialDepthTable			= GraphBuilder.GetPooledBuffer(GSystemTextures.GetDefaultBuffer(GraphBuilder, 4, 0u))->GetSRV(GraphBuilder.RHICmdList, FRHIBufferSRVCreateInfo(PF_R32_UINT));

			UniformParameters->ShadingBinMeta				= GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<FUint32Vector4>(GraphBuilder), PF_R32G32B32A32_UINT);
			UniformParameters->ShadingBinData				= GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<uint32>(GraphBuilder), PF_R32_UINT);

			UniformParameters->MaterialDepthTable			= GraphBuilder.GetPooledBuffer(GSystemTextures.GetDefaultStructuredBuffer<uint32>(GraphBuilder))->GetSRV(GraphBuilder.RHICmdList, FRHIBufferSRVCreateInfo(PF_R32_UINT));

			UniformParameters->MultiViewEnabled				= 1;
			UniformParameters->MultiViewIndices				= GraphBuilder.CreateSRV(ViewIndexBuffer);
			UniformParameters->MultiViewRectScaleOffsets	= GraphBuilder.CreateSRV(ViewRectScaleOffsetBuffer);
			UniformParameters->InViews						= GraphBuilder.CreateSRV(PackedViewBuffer);

			PassParameters->Shading.Nanite					= GraphBuilder.CreateUniformBuffer(UniformParameters);
		}

		CardPagesToRender[0].PatchView(&Scene, SharedView);
		PassParameters->Shading.View = SharedView->GetShaderParameters();
		PassParameters->Shading.Scene = SharedView->GetSceneUniforms().GetBuffer(GraphBuilder);
		PassParameters->Shading.CardPass = GraphBuilder.CreateUniformBuffer(PassUniformParameters);

		TShaderMapRef<FNaniteMultiViewMaterialVS> NaniteVertexShader(SharedView->ShaderMap);

		GraphBuilder.AddPass
		(
			RDG_EVENT_NAME("Lumen Emit GBuffer"),
			PassParameters,
			ERDGPassFlags::Raster,
			[PassParameters, &Scene, &MaterialPasses = BuildContext.MaterialPasses, &NumMaterialQuads = BuildContext.NumMaterialQuads, NaniteVertexShader](FRHICommandList& RHICmdList)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(LumenEmitGBuffer);
				SCOPED_DRAW_EVENTF(RHICmdList, LumenEmitGBuffer, TEXT("%d materials %d quads"), MaterialPasses.Num(), NumMaterialQuads);

				FGraphicsMinimalPipelineStateSet GraphicsMinimalPipelineStateSet;
				FMeshDrawCommandStateCache StateCache;

				const FNaniteMaterialCommands& LumenMaterialCommands = Scene.NaniteMaterials[ENaniteMeshPass::LumenCardCapture];
				for (const FLumenMeshCaptureMaterialPass& MaterialPass : MaterialPasses)
				{
					// One instance per card page
					const uint32 InstanceFactor = MaterialPass.ViewIndices.Num();
					const uint32 InstanceBaseOffset = MaterialPass.ViewIndexBufferOffset;

					FNaniteMaterialCommands::FCommandId CommandId(MaterialPass.CommandStateBucketId);
					const FMeshDrawCommand& MeshDrawCommand = LumenMaterialCommands.GetCommand(CommandId);
					const float MaterialDepth = MaterialPass.GetMaterialDepth();

					SubmitNaniteMultiViewMaterial(
						MeshDrawCommand,
						MaterialDepth,
						NaniteVertexShader,
						GraphicsMinimalPipelineStateSet,
						InstanceFactor,
						RHICmdList,
						StateCache,
						InstanceBaseOffset
					);
				}
			}
		);
	}

	// Emit depth values
	{
		FNaniteEmitDepthRectsParameters* PassParameters = GraphBuilder.AllocParameters<FNaniteEmitDepthRectsParameters>();

		PassParameters->PS.VisBuffer64 = RasterContext.VisBuffer64;
		PassParameters->PS.RenderTargets.DepthStencil = FDepthStencilBinding(
			DepthAtlasTexture,
			ERenderTargetLoadAction::ELoad,
			ERenderTargetLoadAction::ELoad,
			FExclusiveDepthStencil::DepthWrite_StencilRead
		);

		FEmitSceneDepthPS::FPermutationDomain PermutationVectorPS;
		PermutationVectorPS.Set<FEmitSceneDepthPS::FLegacyCullingDim>(UseLegacyCulling());
		PermutationVectorPS.Set<FEmitSceneDepthPS::FVelocityExportDim>(false);
		PermutationVectorPS.Set<FEmitSceneDepthPS::FShadingMaskExportDim>(false);
		auto PixelShader = SharedView->ShaderMap->GetShader<FEmitSceneDepthPS>(PermutationVectorPS);

		FPixelShaderUtils::AddRasterizeToRectsPass(GraphBuilder,
			SharedView->ShaderMap,
			RDG_EVENT_NAME("Emit Depth"),
			PixelShader,
			PassParameters,
			ViewportSize,
			RectMinMaxBufferSRV,
			NumRects,
			TStaticBlendState<>::GetRHI(),
			TStaticRasterizerState<>::GetRHI(),
			TStaticDepthStencilState<true, CF_Always, true, CF_Equal>::GetRHI(),
			STENCIL_SANDBOX_MASK
		);
	}
}

EGBufferLayout GetGBufferLayoutForMaterial(bool bMaterialUsesWorldPositionOffset)
{
	// If WPO is enabled and BasePass velocity is disabled, force velocity output in the base pass
	// (We split the shading pass into two passes for this reason - see Nanite::DrawBasePass above)
	if (!IsUsingBasePassVelocity(GMaxRHIShaderPlatform) && bMaterialUsesWorldPositionOffset)
	{
		return GBL_ForceVelocity;
	}

	return GBL_Default;
}

} // namespace Nanite

FNaniteMaterialCommands::FNaniteMaterialCommands(uint32 InMaxMaterials)
: MaxMaterials(InMaxMaterials)
{
	check(MaxMaterials > 0);
}

FNaniteMaterialCommands::~FNaniteMaterialCommands()
{
	Release();
}

void FNaniteMaterialCommands::Release()
{
	HitProxyTableUploadBuffer.Release();
	HitProxyTableDataBuffer = nullptr;

	MaterialSlotUploadBuffer.Release();
	MaterialSlotDataBuffer = nullptr;

	MaterialDepthUploadBuffer.Release();
	MaterialDepthDataBuffer = nullptr;

#if WITH_DEBUG_VIEW_MODES
	MaterialEditorUploadBuffer.Release();
	MaterialEditorDataBuffer = nullptr;
#endif
}

FNaniteCommandInfo FNaniteMaterialCommands::Register(FMeshDrawCommand& Command, FCommandHash CommandHash, uint32 InstructionCount, bool bWPOEnabled)
{
	FNaniteCommandInfo CommandInfo;

	FCommandId CommandId = FindOrAddIdByHash(CommandHash, Command);
	
	CommandInfo.SetStateBucketId(CommandId.GetIndex());

	FNaniteMaterialEntry& MaterialEntry = GetPayload(CommandId);
	if (MaterialEntry.ReferenceCount == 0)
	{
		check(MaterialEntry.MaterialSlot == INDEX_NONE);
		MaterialEntry.MaterialSlot = MaterialSlotAllocator.Allocate(1);
		MaterialEntry.MaterialId = CommandInfo.GetMaterialId();
	#if WITH_DEBUG_VIEW_MODES
		MaterialEntry.InstructionCount = InstructionCount;
	#endif
		MaterialEntry.bNeedUpload = true;
		MaterialEntry.bWPOEnabled = bWPOEnabled;

		++NumMaterialDepthUpdates;
	}

	CommandInfo.SetMaterialSlot(MaterialEntry.MaterialSlot);

	++MaterialEntry.ReferenceCount;

	check(CommandInfo.GetMaterialSlot() != INDEX_NONE);
	return CommandInfo;
}

void FNaniteMaterialCommands::Unregister(const FNaniteCommandInfo& CommandInfo)
{
	if (CommandInfo.GetStateBucketId() == INDEX_NONE)
	{
		return;
	}

	const FMeshDrawCommand& MeshDrawCommand = GetCommand(CommandInfo.GetStateBucketId());
	FGraphicsMinimalPipelineStateId CachedPipelineId = MeshDrawCommand.CachedPipelineId;

	FNaniteMaterialEntry& MaterialEntry = GetPayload(CommandInfo.GetStateBucketId());
	check(MaterialEntry.ReferenceCount > 0);
	check(MaterialEntry.MaterialSlot != INDEX_NONE);

	--MaterialEntry.ReferenceCount;
	if (MaterialEntry.ReferenceCount == 0)
	{
		check(MaterialEntry.MaterialSlot != INDEX_NONE);
		MaterialSlotAllocator.Free(MaterialEntry.MaterialSlot, 1);

		MaterialEntry.MaterialSlot = INDEX_NONE;
	#if WITH_DEBUG_VIEW_MODES
		MaterialEntry.InstructionCount = 0;
	#endif

		if (MaterialEntry.bNeedUpload)
		{
			check(NumMaterialDepthUpdates > 0);
			--NumMaterialDepthUpdates;
			MaterialEntry.bNeedUpload = false;
		}
		
		RemoveById(CommandInfo.GetStateBucketId());
	}

	FGraphicsMinimalPipelineStateId::RemovePersistentId(CachedPipelineId);
}

void FNaniteMaterialCommands::UpdateBufferState(FRDGBuilder& GraphBuilder, uint32 NumPrimitives)
{
	checkSlow(DoesPlatformSupportNanite(GMaxRHIShaderPlatform));

	LLM_SCOPE_BYTAG(Nanite);

	check(NumPrimitiveUpdates == 0);

	TArray<FRHITransitionInfo, TInlineAllocator<2>> UAVs;

	const uint32 NumMaterialSlots = MaterialSlotAllocator.GetMaxSize();

	const uint32 PrimitiveUpdateReserve = FMath::RoundUpToPowerOfTwo(FMath::Max(NumPrimitives * MaxMaterials, 256u));
	const uint32 MaterialSlotReserve = FMath::RoundUpToPowerOfTwo(FMath::Max(NumMaterialSlots, 256u));

#if WITH_EDITOR
	ResizeByteAddressBufferIfNeeded(GraphBuilder, HitProxyTableDataBuffer, PrimitiveUpdateReserve * sizeof(uint32), TEXT("Nanite.HitProxyTableDataBuffer"));
#endif

	ResizeByteAddressBufferIfNeeded(GraphBuilder, MaterialSlotDataBuffer, PrimitiveUpdateReserve * MaterialSlotSize, TEXT("Nanite.MaterialSlotDataBuffer"));

	ResizeByteAddressBufferIfNeeded(GraphBuilder, MaterialDepthDataBuffer, MaterialSlotReserve * sizeof(uint32), TEXT("Nanite.MaterialDepthDataBuffer"));

#if WITH_DEBUG_VIEW_MODES
	ResizeByteAddressBufferIfNeeded(GraphBuilder, MaterialEditorDataBuffer, MaterialSlotReserve * sizeof(uint32), TEXT("Nanite.MaterialEditorDataBuffer"));
#endif
}

FNaniteMaterialCommands::FUploader* FNaniteMaterialCommands::Begin(FRDGBuilder& GraphBuilder, uint32 NumPrimitives, uint32 InNumPrimitiveUpdates)
{
	checkSlow(DoesPlatformSupportNanite(GMaxRHIShaderPlatform));

	LLM_SCOPE_BYTAG(Nanite);

	check(NumPrimitiveUpdates == 0);

	const uint32 NumMaterialSlots = MaterialSlotAllocator.GetMaxSize();

	const uint32 PrimitiveUpdateReserve = FMath::RoundUpToPowerOfTwo(FMath::Max(NumPrimitives * MaxMaterials, 256u));
	const uint32 MaterialSlotReserve = FMath::RoundUpToPowerOfTwo(FMath::Max(NumMaterialSlots, 256u));

	const auto Register = [&](const TRefCountPtr<FRDGPooledBuffer>& PooledBuffer)
	{
		return GraphBuilder.RegisterExternalBuffer(PooledBuffer);
	};

#if WITH_EDITOR
	check(NumHitProxyTableUpdates == 0);
	check(HitProxyTableDataBuffer);
	check(HitProxyTableDataBuffer->GetSize() == PrimitiveUpdateReserve * sizeof(uint32));
#endif
#if WITH_DEBUG_VIEW_MODES
	check(MaterialEditorDataBuffer)
	check(MaterialEditorDataBuffer->GetSize() == MaterialSlotReserve * sizeof(uint32));
#endif
	check(MaterialSlotDataBuffer);
	check(MaterialSlotDataBuffer->GetSize() == PrimitiveUpdateReserve * MaterialSlotSize);
	check(MaterialDepthDataBuffer);
	check(MaterialDepthDataBuffer->GetSize() == MaterialSlotReserve * sizeof(uint32));

	FNaniteMaterialCommands::FUploader* Uploader = GraphBuilder.AllocObject<FNaniteMaterialCommands::FUploader>();
	Uploader->MaxMaterials = MaxMaterials;

	NumPrimitiveUpdates = InNumPrimitiveUpdates;
	if (NumPrimitiveUpdates > 0)
	{
		Uploader->MaterialSlotUploader = MaterialSlotUploadBuffer.Begin(GraphBuilder, Register(MaterialSlotDataBuffer), NumPrimitiveUpdates * MaxMaterials, MaterialSlotSize, TEXT("Nanite.MaterialSlotUploadBuffer"));
	#if WITH_EDITOR
		Uploader->HitProxyTableUploader = HitProxyTableUploadBuffer.Begin(GraphBuilder, Register(HitProxyTableDataBuffer), NumPrimitiveUpdates * MaxMaterials, sizeof(uint32), TEXT("Nanite.HitProxyTableUploadBuffer"));
	#endif
	}

	if (NumMaterialDepthUpdates > 0)
	{
		Uploader->MaterialDepthUploader = MaterialDepthUploadBuffer.Begin(GraphBuilder, Register(MaterialDepthDataBuffer), NumMaterialDepthUpdates, sizeof(uint32), TEXT("Nanite.MaterialDepthUploadBuffer"));
	#if WITH_DEBUG_VIEW_MODES
		Uploader->MaterialEditorUploader = MaterialEditorUploadBuffer.Begin(GraphBuilder, Register(MaterialEditorDataBuffer), NumMaterialDepthUpdates, sizeof(uint32), TEXT("Nanite.MaterialEditorUploadBuffer"));
	#endif

		for (auto& Command : EntryMap)
		{
			FNaniteMaterialEntry& MaterialEntry = Command.Value;
			if (MaterialEntry.bNeedUpload)
			{
				check(MaterialEntry.MaterialSlot != INDEX_NONE);
				Uploader->DirtyMaterialEntries.Emplace(MaterialEntry);
				MaterialEntry.bNeedUpload = false;
			}
		}
	}

	return Uploader;
}

void FNaniteMaterialCommands::FUploader::Lock(FRHICommandListBase& RHICmdList)
{
	const auto LockIfValid = [&RHICmdList](FRDGScatterUploader* Uploader)
	{
		if (Uploader)
		{
			Uploader->Lock(RHICmdList);
		}
	};

	LockIfValid(MaterialSlotUploader);
#if WITH_EDITOR
	LockIfValid(HitProxyTableUploader);
#endif
	LockIfValid(MaterialDepthUploader);
#if WITH_DEBUG_VIEW_MODES
	LockIfValid(MaterialEditorUploader);
#endif

	for (const FMaterialUploadEntry& MaterialEntry : DirtyMaterialEntries)
	{
		*static_cast<uint32*>(MaterialDepthUploader->Add_GetRef(MaterialEntry.MaterialSlot)) = MaterialEntry.MaterialId;
#if WITH_DEBUG_VIEW_MODES
		* static_cast<uint32*>(MaterialEditorUploader->Add_GetRef(MaterialEntry.MaterialSlot)) = MaterialEntry.InstructionCount;
#endif
	}
	DirtyMaterialEntries.Empty();
}

void FNaniteMaterialCommands::FUploader::Unlock(FRHICommandListBase& RHICmdList)
{
	const auto UnlockIfValid = [&RHICmdList](FRDGScatterUploader* Uploader)
	{
		if (Uploader)
		{
			Uploader->Unlock(RHICmdList);
		}
	};

	UnlockIfValid(MaterialSlotUploader);
#if WITH_EDITOR
	UnlockIfValid(HitProxyTableUploader);
#endif
	UnlockIfValid(MaterialDepthUploader);
#if WITH_DEBUG_VIEW_MODES
	UnlockIfValid(MaterialEditorUploader);
#endif
}

void* FNaniteMaterialCommands::FUploader::GetMaterialSlotPtr(uint32 PrimitiveIndex, uint32 EntryCount)
{
	const uint32 BaseIndex = PrimitiveIndex * MaxMaterials;
	return MaterialSlotUploader->Add_GetRef(BaseIndex, EntryCount);
}

#if WITH_EDITOR
void* FNaniteMaterialCommands::FUploader::GetHitProxyTablePtr(uint32 PrimitiveIndex, uint32 EntryCount)
{
	const uint32 BaseIndex = PrimitiveIndex * MaxMaterials;
	return HitProxyTableUploader->Add_GetRef(BaseIndex, EntryCount);
}
#endif

void FNaniteMaterialCommands::Finish(FRDGBuilder& GraphBuilder, FRDGExternalAccessQueue& ExternalAccessQueue, FUploader* Uploader)
{
	checkSlow(DoesPlatformSupportNanite(GMaxRHIShaderPlatform));

	LLM_SCOPE_BYTAG(Nanite);

	if (NumPrimitiveUpdates == 0 && NumMaterialDepthUpdates == 0)
	{
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "UpdateNaniteMaterials PrimitiveUpdate = %u, MaterialUpdate = %u", NumPrimitiveUpdates, NumMaterialDepthUpdates);

	const auto UploadEnd = [&](FRDGAsyncScatterUploadBuffer& UploadBuffer, FRDGScatterUploader* Uploader)
	{
		check(Uploader);
		UploadBuffer.End(GraphBuilder, Uploader);
		ExternalAccessQueue.Add(Uploader->GetDstResource());
	};

	if (NumPrimitiveUpdates > 0)
	{
		UploadEnd(MaterialSlotUploadBuffer, Uploader->MaterialSlotUploader);
#if WITH_EDITOR
		UploadEnd(HitProxyTableUploadBuffer, Uploader->HitProxyTableUploader);
	#endif
	}

	if (NumMaterialDepthUpdates > 0)
	{
		UploadEnd(MaterialDepthUploadBuffer, Uploader->MaterialDepthUploader);
#if WITH_DEBUG_VIEW_MODES
		UploadEnd(MaterialEditorUploadBuffer, Uploader->MaterialEditorUploader);
	#endif
	}

	NumPrimitiveUpdates = 0;
	NumMaterialDepthUpdates = 0;
}

FNaniteRasterPipeline FNaniteRasterPipeline::GetFixedFunctionPipeline(bool bIsTwoSided, bool bSplineMesh)
{
	FNaniteRasterPipeline Ret;
	Ret.RasterMaterial = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
	Ret.bIsTwoSided = bIsTwoSided;
	Ret.bSplineMesh = bSplineMesh;
	Ret.bPerPixelEval = false;
	Ret.bWPODisableDistance = false;

	return Ret;
}

FNaniteRasterPipelines::FNaniteRasterPipelines()
{
	PipelineBins.Reserve(256);
	PerPixelEvalPipelineBins.Reserve(256);
	PipelineMap.Reserve(256);
}

FNaniteRasterPipelines::~FNaniteRasterPipelines()
{
	PipelineBins.Reset();
	PerPixelEvalPipelineBins.Reset();
	PipelineMap.Empty();
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

/// TODO: Work in progress / experimental

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
		ShadingEntry.ShadingPipeline = InShadingPipeline;
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

namespace Nanite
{

using FMetaBufferArray = TArray<FUintVector4, SceneRenderingAllocator>;

static bool TessellationEnabled()
{
	static const auto TessellationVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Nanite.Tessellation"));
	const bool bTessellation = (TessellationVar && TessellationVar->GetValueOnRenderThread() != 0);
	return bTessellation != 0 && NaniteTessellationSupported();
}

inline uint32 PackMaterialBitFlags(const FMaterial& Material, uint32 BoundTargetMask)
{
	FNaniteMaterialFlags Flags = {0};
	Flags.bPixelDiscard = Material.IsMasked();
	Flags.bPixelDepthOffset = Material.MaterialUsesPixelDepthOffset_RenderThread();
	Flags.bWorldPositionOffset = Material.MaterialUsesWorldPositionOffset_RenderThread();
	Flags.bDisplacement = TessellationEnabled() && Material.MaterialUsesDisplacement_RenderThread();
	const uint32 PackedFlags = PackNaniteMaterialBitFlags(Flags);
	return ((BoundTargetMask & 0xFFu) << 24u) | (PackedFlags & 0x00FFFFFFu);
}

FShadeBinning ShadeBinning(
	FRDGBuilder& GraphBuilder,
	const FScene& Scene,
	const FViewInfo& View,
	const FIntRect InViewRect,
	const FRasterResults& RasterResults
)
{
	FShadeBinning Binning = {};
	if (UseLegacyCulling())
	{
		return Binning;
	}

	LLM_SCOPE_BYTAG(Nanite);
	RDG_EVENT_SCOPE(GraphBuilder, "Nanite::ShadeBinning");

	const FSceneTexturesConfig& Config = View.GetSceneTexturesConfig();
	const EShaderPlatform ShaderPlatform = View.GetShaderPlatform();

	const TArray<TPimplPtr<FNaniteShadingCommand>>& ShadingCommands = Scene.NaniteShadingCommands[ENaniteMeshPass::BasePass];
	const uint32 ShadingCommandCount = uint32(ShadingCommands.Num());

	if (ShadingCommandCount == 0u)
	{
		return Binning;
	}

	// TODO: Optimize this (either tightly compact shading bins / defrag, or cache the max during add to scene)
	uint32 MaxShadingBin = 0;
	for (const TPimplPtr<FNaniteShadingCommand>& ShadingCommand : ShadingCommands)
	{
		MaxShadingBin = FMath::Max<uint32>(MaxShadingBin, uint32(ShadingCommand->ShadingBin));
	}

	const uint32 ShadingBinCount = MaxShadingBin + 1u;
	const uint32 ShadingBinCountPow2 = FMath::RoundUpToPowerOfTwo(ShadingBinCount);

	const bool bGatherStats = GNaniteShowStats != 0;
	const bool bQuadBinning = GNaniteQuadBinning != 0;

	const FUintVector4 ViewRect = FUintVector4(uint32(InViewRect.Min.X), uint32(InViewRect.Min.Y), uint32(InViewRect.Max.X), uint32(InViewRect.Max.Y));

	const uint32 PixelCount = InViewRect.Width() * InViewRect.Height();

	const int32 MacroTileLoops = GBinningTechnique == 2 ? 2 : 1; // 4x 32x32
	const int32 QuadWidth = FMath::DivideAndRoundUp(InViewRect.Width(), 2 * MacroTileLoops);
	const int32 QuadHeight = FMath::DivideAndRoundUp(InViewRect.Height(), 2 * MacroTileLoops);

	const FIntPoint GroupDim = GBinningTechnique == 0 ? FIntPoint(8u, 8u) : FIntPoint(32u, 32u);
	const FIntVector  QuadDispatchDim = FComputeShaderUtils::GetGroupCount(FIntPoint(QuadWidth, QuadHeight), GroupDim);
	const FIntVector   BinDispatchDim = FComputeShaderUtils::GetGroupCount(ShadingBinCount, 64u);

	FMetaBufferArray MetaBufferData;
	MetaBufferData.SetNumZeroed(ShadingBinCount);

	for (const TPimplPtr<FNaniteShadingCommand>& ShadingCommand : ShadingCommands)
	{
		if (const FMaterial* Material = ShadingCommand->Material)
		{
			FUintVector4& MetaEntry = MetaBufferData[ShadingCommand->ShadingBin];
			MetaEntry.W = PackMaterialBitFlags(*Material, ShadingCommand->BoundTargetMask);
		}
	}

	Binning.ShadingBinMeta = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("Nanite.ShadingBinMeta"),
		sizeof(FNaniteShadingBinMeta),
		ShadingBinCountPow2,
		MetaBufferData.GetData(),
		sizeof(FNaniteShadingBinMeta) * MetaBufferData.Num()
	);

	Binning.ShadingBinArgs   = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(ShadingBinCountPow2 * 4u /* XYZ and Padding */), TEXT("Nanite.ShadingBinArgs"));
	Binning.ShadingBinData   = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), PixelCount), TEXT("Nanite.ShadingBinData"));
	Binning.ShadingBinStats  = bGatherStats ? GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FNaniteShadingBinStats), 1u), TEXT("Nanite.ShadingBinStats")) : nullptr;

	FRDGBufferUAVRef ShadingBinMetaUAV  = GraphBuilder.CreateUAV(Binning.ShadingBinMeta);
	FRDGBufferUAVRef ShadingBinArgsUAV  = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Binning.ShadingBinArgs, PF_R32_UINT));
	FRDGBufferUAVRef ShadingBinDataUAV = GraphBuilder.CreateUAV(Binning.ShadingBinData);
	FRDGBufferUAVRef ShadingBinStatsUAV = bGatherStats ? GraphBuilder.CreateUAV(Binning.ShadingBinStats) : nullptr;

	if (bGatherStats)
	{
		AddClearUAVPass(GraphBuilder, ShadingBinStatsUAV, 0);
	}

	// Shading Bin Count
	{
		FShadingBinBuildCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FShadingBinBuildCS::FParameters>();
		PassParameters->ViewRect = ViewRect;
		PassParameters->QuadDispatchDim = FUint32Vector2(QuadDispatchDim.X, QuadDispatchDim.Y);
		PassParameters->ShadingBinCount = ShadingBinCount;
		PassParameters->ShadingRateTileSize = GetShadingRateTileSize();
		PassParameters->ShadingRateImage = GetShadingRateImage(GraphBuilder, View);
		PassParameters->ShadingMaskSampler = TStaticSamplerState<SF_Point>::GetRHI();
		PassParameters->ShadingMask = RasterResults.ShadingMask;
		PassParameters->OutShadingBinMeta = ShadingBinMetaUAV;
		PassParameters->OutShadingBinData = nullptr;
		PassParameters->OutShadingBinArgs = ShadingBinArgsUAV;

		FShadingBinBuildCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FShadingBinBuildCS::FBuildPassDim>(NANITE_SHADING_BIN_COUNT);
		PermutationVector.Set<FShadingBinBuildCS::FTechniqueDim>(FMath::Clamp<int32>(GBinningTechnique, 0, 2));
		PermutationVector.Set<FShadingBinBuildCS::FGatherStatsDim>(bGatherStats);
		PermutationVector.Set<FShadingBinBuildCS::FQuadBinningDim>(bQuadBinning);
		PermutationVector.Set<FShadingBinBuildCS::FVariableRateDim>(PassParameters->ShadingRateTileSize != 0u);
		auto ComputeShader = View.ShaderMap->GetShader<FShadingBinBuildCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ShadingCount"),
			ComputeShader,
			PassParameters,
			QuadDispatchDim
		);
	}

	// Shading Bin Reserve
	{
		FRDGBufferRef ShadingBinAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), 1), TEXT("Nanite.ShadingBinAllocator"));
		FRDGBufferUAVRef ShadingBinAllocatorUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(ShadingBinAllocator, PF_R32_UINT));
		AddClearUAVPass(GraphBuilder, ShadingBinAllocatorUAV, 0);

		FShadingBinReserveCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FShadingBinReserveCS::FParameters>();
		PassParameters->ShadingBinCount = ShadingBinCount;
		PassParameters->OutShadingBinMeta = ShadingBinMetaUAV;
		PassParameters->OutShadingBinAllocator = ShadingBinAllocatorUAV;
		PassParameters->OutShadingBinArgs = ShadingBinArgsUAV;
		PassParameters->OutShadingBinStats = ShadingBinStatsUAV;

		FShadingBinReserveCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FShadingBinReserveCS::FGatherStatsDim>(bGatherStats);
		PermutationVector.Set<FShadingBinReserveCS::FQuadBinningDim>(bQuadBinning);
		auto ComputeShader = View.ShaderMap->GetShader<FShadingBinReserveCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ShadingReserve"),
			ComputeShader,
			PassParameters,
			BinDispatchDim
		);
	}

	// Shading Bin Scatter
	{
		FShadingBinBuildCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FShadingBinBuildCS::FParameters>();
		PassParameters->ViewRect = ViewRect;
		PassParameters->QuadDispatchDim = FUint32Vector2(QuadDispatchDim.X, QuadDispatchDim.Y);
		PassParameters->ShadingBinCount = ShadingBinCount;
		PassParameters->ShadingRateTileSize = GetShadingRateTileSize();
		PassParameters->ShadingRateImage = GetShadingRateImage(GraphBuilder, View);
		PassParameters->ShadingMaskSampler = TStaticSamplerState<SF_Point>::GetRHI();
		PassParameters->ShadingMask = RasterResults.ShadingMask;
		PassParameters->OutShadingBinMeta = ShadingBinMetaUAV;
		PassParameters->OutShadingBinData = ShadingBinDataUAV;
		PassParameters->OutShadingBinArgs = nullptr;

		const bool bVariableRateShading = PassParameters->ShadingRateTileSize != 0u;

		FShadingBinBuildCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FShadingBinBuildCS::FBuildPassDim>(NANITE_SHADING_BIN_SCATTER);
		PermutationVector.Set<FShadingBinBuildCS::FTechniqueDim>(FMath::Clamp<int32>(GBinningTechnique, 0, 2));
		PermutationVector.Set<FShadingBinBuildCS::FGatherStatsDim>(false);
		PermutationVector.Set<FShadingBinBuildCS::FQuadBinningDim>(bQuadBinning);
		PermutationVector.Set<FShadingBinBuildCS::FVariableRateDim>(PassParameters->ShadingRateTileSize != 0u);
		auto ComputeShader = View.ShaderMap->GetShader<FShadingBinBuildCS>(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ShadingScatter"),
			ComputeShader,
			PassParameters,
			QuadDispatchDim
		);
	}

	return Binning;
}

}

/// END-TODO: Work in progress / experimental

struct FNaniteVisibilityQuery
{
	void Init(
		const FNaniteRasterPipelines* RasterPipelines,
		const FNaniteMaterialCommands* MaterialCommands)
	{
		RasterBinCount = RasterPipelines->GetBinCount();
		ShadingDrawCount = MaterialCommands->GetCommands().Num();
		BinIndexTranslator = RasterPipelines->GetBinIndexTranslator();
		ShadingDrawVisibility.Reserve(ShadingDrawCount);

		RasterBinVisibility.SetNum(RasterBinCount);
		for (uint32 RasterBinIndex = 0; RasterBinIndex < RasterBinCount; ++RasterBinIndex)
		{
			RasterBinVisibility[int32(RasterBinIndex)] = false;
		}
	}

	UE::Tasks::FTask		CompletedEvent;
	TArray<FConvexVolume>	Views;
	TArray<TAtomic<bool>>	RasterBinVisibility;
	TSet<uint32>			ShadingDrawVisibility;
	TSet<uint32>			VisibleCustomDepthPrimitives;
	FNaniteRasterBinIndexTranslator BinIndexTranslator;
	uint32 RasterBinCount;
	uint32 ShadingDrawCount;
	uint8 bFinished			: 1; 
	uint8 bCullRasterBins	: 1;
	uint8 bCullShadingBins	: 1;
};

bool FNaniteVisibilityResults::IsRasterBinVisible(uint16 BinIndex) const
{
	return IsRasterTestValid() ? RasterBinVisibility[int32(BinIndexTranslator.Translate(BinIndex))] : true;
}

bool FNaniteVisibilityResults::IsShadingDrawVisible(uint32 DrawId) const
{
	return IsShadingTestValid() ? ShadingDrawVisibility.Contains(DrawId) : true;
}

void FNaniteVisibilityResults::Invalidate()
{
	bRasterTestValid	= false;
	bShadingTestValid	= false;
	VisibleRasterBins	= 0;
	VisibleShadingDraws	= 0;
	RasterBinVisibility.Reset();
	ShadingDrawVisibility.Reset();
}

static FORCEINLINE bool IsVisibilityTestNeeded(
	const FNaniteVisibilityQuery* Query,
	const FNaniteVisibility::FPrimitiveReferences& References,
	const FNaniteRasterBinIndexTranslator BinIndexTranslator,
	bool bAsync)
{
	bool bShouldTest = false;

	for (const FNaniteVisibility::FPrimitiveBins& RasterBins : References.RasterBins)
	{
		const bool bPrimaryVisible = Query->RasterBinVisibility[int32(BinIndexTranslator.Translate(RasterBins.Primary))];
		const bool bSecondaryVisible = RasterBins.Secondary != 0xFFFFu ? (bool)(Query->RasterBinVisibility[int32(BinIndexTranslator.Translate(RasterBins.Secondary))]) : true;

		if (!bPrimaryVisible || !bSecondaryVisible) // Raster bin reference is not marked visible
		{
			bShouldTest = true;
			break;
		}
	}

	if (!bShouldTest)
	{
		for (const uint32& ShadingDrawId : References.ShadingDraws)
		{
			if (!Query->ShadingDrawVisibility.Contains(ShadingDrawId)) // Shading draw reference is not present
			{
				bShouldTest = true;
				break;
			}
		}
	}

	return bShouldTest;
}

static FORCEINLINE bool IsNanitePrimitiveVisible(const FNaniteVisibilityQuery* Query, const FPrimitiveSceneInfo* SceneInfo)
{
	FPrimitiveSceneProxy* SceneProxy = SceneInfo->Proxy;
	if (!SceneProxy || SceneInfo->Scene == nullptr || !SceneInfo->IsIndexValid())
	{
		return false;
	}

	bool bPrimitiveVisible = true;
	
	if (GNaniteMaterialVisibilityPrimitives != 0)
	{
		bPrimitiveVisible = false;

		const FBoxSphereBounds& ProxyBounds = SceneInfo->Scene->PrimitiveBounds[SceneInfo->GetIndex()].BoxSphereBounds; // World space bounds

		for (const FConvexVolume& View : Query->Views)
		{
			bPrimitiveVisible = View.IntersectBox(ProxyBounds.Origin, ProxyBounds.BoxExtent);
			if (bPrimitiveVisible)
			{
				break;
			}
		}
	}

	if (bPrimitiveVisible && GNaniteMaterialVisibilityInstances != 0)
	{
		bPrimitiveVisible = false;

		const FMatrix& PrimitiveToWorld = SceneInfo->Scene->PrimitiveTransforms[SceneInfo->GetIndex()];
		const TConstArrayView<FInstanceSceneData> InstanceSceneData = SceneProxy->GetInstanceSceneData();

		for (int32 InstanceIndex = 0; InstanceIndex < InstanceSceneData.Num(); ++InstanceIndex)
		{
			const FInstanceSceneData& PrimitiveInstance = InstanceSceneData[InstanceIndex];
			const FMatrix InstanceToWorld = PrimitiveInstance.LocalToPrimitive.ToMatrix() * PrimitiveToWorld;
			const FBox InstanceBounds = SceneProxy->GetInstanceLocalBounds(InstanceIndex).ToBox();
			const FBoxSphereBounds InstanceWorldBounds = FBoxSphereBounds(InstanceBounds.TransformBy(InstanceToWorld));

			for (const FConvexVolume& View : Query->Views)
			{
				bPrimitiveVisible = View.IntersectBox(InstanceWorldBounds.Origin, InstanceWorldBounds.BoxExtent);
				if (bPrimitiveVisible)
				{
					break;
				}
			}

			if (bPrimitiveVisible)
			{
				break;
			}
		}
	}

	return bPrimitiveVisible;
}

static void PerformNaniteVisibility(const FNaniteVisibility::PrimitiveMapType& PrimitiveReferences, FNaniteVisibilityQuery* Query)
{
	SCOPED_NAMED_EVENT(PerformNaniteVisibility, FColor::Magenta);

	if (PrimitiveReferences.Num() == 0)
	{
		return;
	}

	for (const auto& KeyValue : PrimitiveReferences)
	{
		const FNaniteVisibility::FPrimitiveReferences& References = KeyValue.Value;

		bool bPrimitiveVisible = true;
		const bool bShouldTest = IsVisibilityTestNeeded(Query, References, Query->BinIndexTranslator, false /* Async */);
		if (bShouldTest)
		{
			bPrimitiveVisible = IsNanitePrimitiveVisible(Query, References.SceneInfo);
			if (bPrimitiveVisible)
			{
				if (Query->bCullRasterBins)
				{
					for (const FNaniteVisibility::FPrimitiveBins RasterBins : References.RasterBins)
					{
						Query->RasterBinVisibility[int32(Query->BinIndexTranslator.Translate(RasterBins.Primary))] = true;
						if (RasterBins.Secondary != 0xFFFFu)
						{
							Query->RasterBinVisibility[int32(Query->BinIndexTranslator.Translate(RasterBins.Secondary))] = true;
						}
					}
				}

				if (Query->bCullShadingBins)
				{
					Query->ShadingDrawVisibility.Append(References.ShadingDraws);
				}
			}
		}

		// NOTE: This makes the assumption that the visibility test doesn't occlusion cull
		if (Nanite::GetSupportsCustomDepthRendering() && References.bWritesCustomDepthStencil && bPrimitiveVisible)
		{
			Query->VisibleCustomDepthPrimitives.Add(References.SceneInfo->GetIndex());
		}
	}
}

FNaniteVisibility::FNaniteVisibility()
: bCalledBegin(false)
{
}

void FNaniteVisibility::BeginVisibilityFrame()
{
	check(VisibilityQueries.Num() == 0);
	check(!bCalledBegin);
	bCalledBegin = true;
}

void FNaniteVisibility::FinishVisibilityFrame()
{
	check(bCalledBegin);

	WaitForTasks();

	for (FNaniteVisibilityQuery* Query : VisibilityQueries)
	{
		check(Query->bFinished);
		delete Query;
	}

	VisibilityQueries.Reset();
	bCalledBegin = false;
}

FNaniteVisibilityQuery* FNaniteVisibility::BeginVisibilityQuery(
	FScene& Scene,
	const TConstArrayView<FConvexVolume>& ViewList,
	const class FNaniteRasterPipelines* RasterPipelines,
	const class FNaniteMaterialCommands* MaterialCommands
)
{
	check(RasterPipelines);
	check(MaterialCommands);

	if (!bCalledBegin || ViewList.IsEmpty() || GNaniteMaterialVisibility == 0)
	{
		// Nothing to do
		return nullptr;
	}

	const bool bRunAsync = GNaniteMaterialVisibilityAsync != 0;

	FNaniteVisibilityQuery* VisibilityQuery = new FNaniteVisibilityQuery;
	VisibilityQuery->Views = ViewList;
	VisibilityQuery->bCullRasterBins  = GNaniteMaterialVisibilityRasterBins != 0;
	VisibilityQuery->bCullShadingBins = GNaniteMaterialVisibilityShadingDraws != 0;

	VisibilityQueries.Emplace(VisibilityQuery);

	VisibilityQuery->bFinished = false;
	if (bRunAsync)
	{
		VisibilityQuery->CompletedEvent = UE::Tasks::Launch(UE_SOURCE_LOCATION, [this, VisibilityQuery, RasterPipelines, MaterialCommands]
		{
			VisibilityQuery->Init(RasterPipelines, MaterialCommands);
			PerformNaniteVisibility(PrimitiveReferences, VisibilityQuery);

		}, Scene.GetCacheNaniteDrawCommandsTask(), UE::Tasks::ETaskPriority::High);

		ActiveEvents.Emplace(VisibilityQuery->CompletedEvent);
	}
	else
	{
		Scene.WaitForCacheNaniteDrawCommandsTask();
		VisibilityQuery->Init(RasterPipelines, MaterialCommands);
	}
	return VisibilityQuery;
}

void FNaniteVisibility::FinishVisibilityQuery(FNaniteVisibilityQuery* Query, FNaniteVisibilityResults& OutResults)
{
	OutResults.Invalidate();

	if (Query != nullptr)
	{
		check(!Query->bFinished);

		if (Query->CompletedEvent.IsValid())
		{
			SCOPED_NAMED_EVENT_TEXT("EndPerformNaniteVisibility", FColor::Magenta);
			Query->CompletedEvent.Wait();
			ActiveEvents.RemoveSingleSwap(Query->CompletedEvent, false);
		}
		else
		{
			PerformNaniteVisibility(PrimitiveReferences, Query);
		}

		OutResults.SetRasterBinIndexTranslator(Query->BinIndexTranslator);
		OutResults.bRasterTestValid  = Query->bCullRasterBins;
		OutResults.bShadingTestValid = Query->bCullShadingBins;

		if (OutResults.bRasterTestValid)
		{
			OutResults.RasterBinVisibility.Init(false, Query->RasterBinVisibility.Num());
			for (int32 RasterBinIndex = 0; RasterBinIndex < Query->RasterBinVisibility.Num(); ++RasterBinIndex)
			{
				if (Query->RasterBinVisibility[RasterBinIndex])
				{
					OutResults.RasterBinVisibility[RasterBinIndex] = true;
					++OutResults.VisibleRasterBins;
				}
			}
		}

		if (OutResults.bShadingTestValid)
		{
			OutResults.ShadingDrawVisibility = Query->ShadingDrawVisibility.Array();
			OutResults.VisibleShadingDraws = OutResults.ShadingDrawVisibility.Num();
		}

		OutResults.TotalRasterBins = Query->RasterBinCount;
		OutResults.TotalShadingDraws = Query->ShadingDrawCount;
		OutResults.VisibleCustomDepthPrimitives = Query->VisibleCustomDepthPrimitives;
		Query->bFinished = true;
	}
}

FNaniteVisibility::FPrimitiveReferences* FNaniteVisibility::FindOrAddPrimitiveReferences(const FPrimitiveSceneInfo* SceneInfo)
{
	if (!GNaniteMaterialVisibility)
	{
		return nullptr;
	}

	FNaniteVisibility::FPrimitiveReferences* References = PrimitiveReferences.FindOrAdd(SceneInfo, FNaniteVisibility::FPrimitiveReferences{});

	// If we perform visibility query for either raster bins or shading, we can piggy back the testing to further cull Nanite
	// custom depth instances on the view
	if (SceneInfo->Proxy && SceneInfo->Proxy->ShouldRenderCustomDepth())
	{
		References->bWritesCustomDepthStencil = true;
	}

	return References;
}

void FNaniteVisibility::WaitForTasks()
{
	// This wait is mainly to catch any unexpected places that are trying to mutate the PrimitiveReferences array.
	// We should be waiting on each query manually and all reference changes should be done by now.
	ensureMsgf(ActiveEvents.IsEmpty(), TEXT("Nanite Visibility task is being waited on in an unexpected place. This is safe but should be investigated for performance issues."));

	if (!ActiveEvents.IsEmpty())
	{
		UE::Tasks::Wait(ActiveEvents);
		ActiveEvents.Reset();
	}
}

FNaniteVisibility::PrimitiveBinsType* FNaniteVisibility::GetRasterBinReferences(const FPrimitiveSceneInfo* SceneInfo)
{
	if (!GNaniteMaterialVisibility)
	{
		return nullptr;
	}

	FNaniteVisibility::FPrimitiveReferences* References = FindOrAddPrimitiveReferences(SceneInfo);
	References->SceneInfo = SceneInfo;
	return &References->RasterBins;
}

FNaniteVisibility::PrimitiveDrawType* FNaniteVisibility::GetShadingDrawReferences(const FPrimitiveSceneInfo* SceneInfo)
{
	if (!GNaniteMaterialVisibility)
	{
		return nullptr;
	}

	FNaniteVisibility::FPrimitiveReferences* References = FindOrAddPrimitiveReferences(SceneInfo);
	References->SceneInfo = SceneInfo;
	return &References->ShadingDraws;
}

void FNaniteVisibility::RemoveReferences(const FPrimitiveSceneInfo* SceneInfo)
{
	if (!GNaniteMaterialVisibility)
	{
		return;
	}

	PrimitiveReferences.Remove(SceneInfo);
}
