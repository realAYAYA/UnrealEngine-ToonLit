// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteVisualize.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "NaniteVisualizationData.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessVisualizeComplexity.h"
#include "PostProcess/SceneRenderTargets.h"
#include "PixelShaderUtils.h"
#include "ScenePrivate.h"
#include "SceneTextureReductions.h"
#include "SceneManagement.h"
#include "Rendering/NaniteStreamingManager.h"
#include "DebugViewModeHelpers.h"
#include "Materials/Material.h"
#include "Materials/MaterialRenderProxy.h"
#include "NaniteSceneProxy.h"
#include "ShaderPrint.h"
#include "InstanceDataSceneProxy.h"
#include "Nanite/NaniteMaterialsSceneExtension.h"

// Specifies if visualization only shows Nanite information that passes full scene depth test
// -1: Use default composition specified the each mode
//  0: Force composition with scene depth off
//  1: Force composition with scene depth on
int32 GNaniteVisualizeComposite = -1;
FAutoConsoleVariableRef CVarNaniteVisualizeComposite(
	TEXT("r.Nanite.Visualize.Composite"),
	GNaniteVisualizeComposite,
	TEXT("")
);

int32 GNaniteVisualizeEdgeDetect = 1;
static FAutoConsoleVariableRef CVarNaniteVisualizeEdgeDetect(
	TEXT("r.Nanite.Visualize.EdgeDetect"),
	GNaniteVisualizeEdgeDetect,
	TEXT("")
);

int32 GNaniteVisualizeOverdrawScale = 15; // % of contribution per pixel evaluation (up to 100%)
FAutoConsoleVariableRef CVarNaniteVisualizeOverdrawScale(
	TEXT("r.Nanite.Visualize.OverdrawScale"),
	GNaniteVisualizeOverdrawScale,
	TEXT("")
);

int32 GNaniteVisualizeComplexityScale = 80; // % of contribution per material evaluation (up to 100%)
FAutoConsoleVariableRef CVarNaniteVisualizeComplexityScale(
	TEXT("r.Nanite.Visualize.ComplexityScale"),
	GNaniteVisualizeComplexityScale,
	TEXT("")
);

// Fudge factor chosen by visually comparing Nanite vs non-Nanite cube shader complexity using default material, and choosing value where colors match.
int32 GNaniteVisualizeComplexityOverhead = 7400; // Baseline overhead of Nanite ALU (added to global shader budget)
FAutoConsoleVariableRef CVarNaniteVisualizeComplexityOverhead(
	TEXT("r.Nanite.Visualize.ComplexityOverhead"),
	GNaniteVisualizeComplexityOverhead,
	TEXT("")
);

int32 GNanitePickingDomain = NANITE_PICKING_DOMAIN_TRIANGLE;
FAutoConsoleVariableRef CVarNanitePickingDomain(
	TEXT("r.Nanite.Picking.Domain"),
	GNanitePickingDomain,
	TEXT("")
);

int32 GNanitePixelProgrammableVisMode = NANITE_PIXEL_PROG_VIS_MODE_DEFAULT;
FAutoConsoleVariableRef CVarNanitePixelProgrammableVisMode(
	TEXT("r.Nanite.Visualize.PixelProgrammableVisMode"),
	GNanitePixelProgrammableVisMode,
	TEXT("0: Show masked, pixel depth offset, and dynamic displacement materials.\n")
	TEXT("1: Show masked materials only.\n")
	TEXT("2: Show pixel depth offset only.\n")
	TEXT("3: Show dynamic displacement only.")
);


static FRDGBufferSRVRef GetEditorSelectedHitProxyIdsSRV(FRDGBuilder& GraphBuilder, const FViewInfo& View)
{
	FRDGBufferRef HitProxyIdsBuffer = nullptr;

#if WITH_EDITOR
	uint32 IdCount = View.EditorSelectedNaniteHitProxyIds.Num();
	uint32 BufferCount = FMath::Max(FMath::RoundUpToPowerOfTwo(IdCount), 1u);

	TConstArrayView<uint32> HitProxyIds = View.EditorSelectedNaniteHitProxyIds;
	TArray<uint32, SceneRenderingAllocator> HitProxyIdsCopy;
	if (BufferCount > IdCount)
	{
		const uint32 FillValue = IdCount == 0 ? FHitProxyId().GetColor().ToPackedARGB() : HitProxyIds.Last();
		HitProxyIdsCopy.Reserve(BufferCount);
		HitProxyIdsCopy.Append(View.EditorSelectedNaniteHitProxyIds);
		for (uint32 i = IdCount; i < BufferCount; ++i)
		{
			HitProxyIdsCopy.Add(FillValue);
		}
		HitProxyIds = HitProxyIdsCopy;
	}

	HitProxyIdsBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateUploadDesc(sizeof(uint32), BufferCount), TEXT("EditorSelectedNaniteHitProxyIds"));
	GraphBuilder.QueueBufferUpload(HitProxyIdsBuffer, HitProxyIds);
#else
	HitProxyIdsBuffer = GSystemTextures.GetDefaultBuffer<uint32>(GraphBuilder);
#endif

	return GraphBuilder.CreateSRV(HitProxyIdsBuffer, PF_R32_UINT);
}

static FIntVector4 GetVisualizeConfig(int32 ModeID, bool bCompositeScene, bool bEdgeDetect)
{
	if (ModeID != INDEX_NONE)
	{
		int32 ModeArg = 0;
		switch (ModeID)
		{
		case NANITE_VISUALIZE_PICKING:
			ModeArg = GNanitePickingDomain;
			break;
		case NANITE_VISUALIZE_PIXEL_PROGRAMMABLE_RASTER:
			ModeArg = GNanitePixelProgrammableVisMode;
		default:
			break;
		}
		return FIntVector4(ModeID, ModeArg, bCompositeScene ? 1 : 0, bEdgeDetect ? 1 : 0);
	}

	return FIntVector4(INDEX_NONE, 0, 0, 0);
}

static FIntVector4 GetVisualizeScales(int32 ModeID, uint32 ShadingExportCount)
{
	if (ModeID != INDEX_NONE)
	{
		return FIntVector4(GNaniteVisualizeOverdrawScale, GNaniteVisualizeComplexityScale, int32(ShadingExportCount), 0 /* Unused */);
	}

	return FIntVector4(INDEX_NONE, 0, 0, 0);
}

static bool VisualizationRequiresHiZDecode(int32 ModeID)
{
	switch (ModeID)
	{
	case NANITE_VISUALIZE_SCENE_Z_MIN:
	case NANITE_VISUALIZE_SCENE_Z_MAX:
	case NANITE_VISUALIZE_SCENE_Z_DELTA:
	case NANITE_VISUALIZE_SCENE_Z_DECODED:
	case NANITE_VISUALIZE_MATERIAL_Z_MIN:
	case NANITE_VISUALIZE_MATERIAL_Z_MAX:
	case NANITE_VISUALIZE_MATERIAL_Z_DELTA:
	case NANITE_VISUALIZE_MATERIAL_Z_DECODED:
		return true;

	default:
		return false;
	}
}

class FNaniteVisualizeCS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FNaniteVisualizeCS);
	SHADER_USE_PARAMETER_STRUCT(FNaniteVisualizeCS, FNaniteGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NANITE_USE_VIEW_UNIFORM_BUFFER"), 1);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, DebugOutput)
		SHADER_PARAMETER(FIntVector4, VisualizeConfig)
		SHADER_PARAMETER(FIntVector4, VisualizeScales)
		SHADER_PARAMETER(FIntVector4, PageConstants)
		SHADER_PARAMETER(uint32, MaxVisibleClusters)
		SHADER_PARAMETER(uint32, RenderFlags)
		SHADER_PARAMETER(uint32, RegularMaterialRasterBinCount)
		SHADER_PARAMETER(uint32, FixedFunctionBin)
		SHADER_PARAMETER(FIntPoint, PickingPixelPos)
		SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualShadowMapUniformParameters, VirtualShadowMap)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ClusterPageData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, VisibleClustersSWHW)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ShadingBinData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FNaniteRasterBinMeta>, RasterBinMeta)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UlongType>, VisBuffer64)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UlongType>, DbgBuffer64)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, DbgBuffer32)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadingMask)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, SceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, SceneZDecoded)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<FUint32Vector4>, SceneZLayout)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, MaterialZDecoded)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<FUint32Vector4>, MaterialZLayout)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, FastClearTileVis)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, MaterialDepthTable)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, MaterialHitProxyTable)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FNaniteVisualizeCS, "/Engine/Private/Nanite/NaniteVisualize.usf", "VisualizeCS", SF_Compute);

class FNanitePickingCS : public FNaniteGlobalShader
{
	DECLARE_GLOBAL_SHADER(FNanitePickingCS);
	SHADER_USE_PARAMETER_STRUCT(FNanitePickingCS, FNaniteGlobalShader);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NANITE_USE_VIEW_UNIFORM_BUFFER"), 1);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FNanitePickingFeedback>, FeedbackBuffer)
		SHADER_PARAMETER(FIntVector4, VisualizeConfig)
		SHADER_PARAMETER(FIntVector4, PageConstants)
		SHADER_PARAMETER(uint32, MaxVisibleClusters)
		SHADER_PARAMETER(uint32, RenderFlags)
		SHADER_PARAMETER(uint32, RegularMaterialRasterBinCount)
		SHADER_PARAMETER(FIntPoint, PickingPixelPos)
		SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ShadingBinData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ClusterPageData)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, VisibleClustersSWHW)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UlongType>, VisBuffer64)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UlongType>, DbgBuffer64)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, DbgBuffer32)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadingMask)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, SceneDepth)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, MaterialDepthTable)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, MaterialHitProxyTable)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FNanitePickingCS, "/Engine/Private/Nanite/NaniteVisualize.usf", "PickingCS", SF_Compute);

class FDepthDecodeCS : public FNaniteGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FDepthDecodeCS);
	SHADER_USE_PARAMETER_STRUCT(FDepthDecodeCS, FNaniteGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FPackedView>, InViews)
		SHADER_PARAMETER(FUint32Vector4, ViewRect)
		SHADER_PARAMETER(FUint32Vector4, HTileConfig)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float>, SceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadingMask)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, MaterialDepthTable)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(TextureMetadata, SceneHTileBuffer)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(TextureMetadata, MaterialHTileBuffer)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, SceneZDecoded)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<FUint32Vector4>, SceneZLayout)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, MaterialZDecoded)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<FUint32Vector4>, MaterialZLayout)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}
};
IMPLEMENT_GLOBAL_SHADER(FDepthDecodeCS, "/Engine/Private/Nanite/NaniteDepthDecode.usf", "DepthDecode", SF_Compute);

class FExportDebugViewPS : public FNaniteGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FExportDebugViewPS);
	SHADER_USE_PARAMETER_STRUCT(FExportDebugViewPS, FNaniteGlobalShader);

	class FSearchBufferCountDim : SHADER_PERMUTATION_INT("EDITOR_SELECTED_BUFFER_COUNT_LOG_2", 25);
	using FPermutationDomain = TShaderPermutationDomain<FSearchBufferCountDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, VisibleClustersSWHW)
		SHADER_PARAMETER(FIntVector4, PageConstants)
		SHADER_PARAMETER(FIntVector4, ViewRect)
		SHADER_PARAMETER(float, InvShaderBudget)
		SHADER_PARAMETER(FVector3f, SelectionColor)
		SHADER_PARAMETER(uint32, DebugViewMode)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ClusterPageData)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<UlongType>, VisBuffer64)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, SceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ShadingMask)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, MaterialDepthTable)
		SHADER_PARAMETER_SRV(ByteAddressBuffer, MaterialEditorTable)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, MaterialHitProxyTable)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, EditorSelectedHitProxyIds)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, ShadingBinData)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool IsPlatformSupported(EShaderPlatform ShaderPlatform)
	{
		return DoesPlatformSupportNanite(ShaderPlatform) && FDataDrivenShaderPlatformInfo::GetSupportsDebugViewShaders(ShaderPlatform);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsPlatformSupported(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FNaniteGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("NANITE_USE_VIEW_UNIFORM_BUFFER"), 1);

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		uint32 SelectedBufferCount = 1u << (uint32)PermutationVector.Get<FSearchBufferCountDim>();
		OutEnvironment.SetDefine(TEXT("EDITOR_SELECTED_BUFFER_COUNT"), SelectedBufferCount);
	}
};
IMPLEMENT_GLOBAL_SHADER(FExportDebugViewPS, "/Engine/Private/Nanite/NaniteDebugViews.usf", "ExportDebugViewPS", SF_Pixel);

namespace Nanite
{

static FRDGBufferSRVRef GetShadingBinDataSRV(FRDGBuilder& GraphBuilder)
{
	FRDGBufferRef ShadingBinData = nullptr;
	if (Nanite::GGlobalResources.GetShadingBinDataBufferRef().IsValid())
	{
		ShadingBinData = GraphBuilder.RegisterExternalBuffer(Nanite::GGlobalResources.GetShadingBinDataBufferRef());
	}
	else
	{
		ShadingBinData = GSystemTextures.GetDefaultByteAddressBuffer(GraphBuilder, 4u);
	}

	return GraphBuilder.CreateSRV(ShadingBinData);
}

static FRDGTextureRef GetFastClearTileVis(FRDGBuilder& GraphBuilder)
{
	FRDGTextureRef FastClearTileVis = nullptr;
	if (Nanite::GGlobalResources.GetFastClearTileVisRef().IsValid())
	{
		FastClearTileVis = GraphBuilder.RegisterExternalTexture(Nanite::GGlobalResources.GetFastClearTileVisRef());
	}
	else
	{
		FastClearTileVis = GSystemTextures.GetBlackAlphaOneDummy(GraphBuilder);
	}

	return FastClearTileVis;
}

static FRDGBufferRef PerformPicking(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FSceneTextures& SceneTextures,
	Nanite::FRasterResults& Data,
	const FViewInfo& View
)
{
	// Force shader print on
	ShaderPrint::SetEnabled(true);

	// Make sure there's space for all debug lines the picking CS could possibly draw
	const uint32 NumDebugLines =
		8 * 2			// 2 OBBs - Instance + Cluster
		+ 3				// Instance origin axis
		+ 32 * 3		// (Cluster domain) Cluster LOD bounds sphere
		+ 8 * 16 * 3	// (Cluster domain, Spline mesh) Slice spheres used to generate deformed cluster AABB
	;
	ShaderPrint::RequestSpaceForLines(NumDebugLines);

	const FNaniteVisualizationData& VisualizationData = GetNaniteVisualizationData();
	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);

	const FNaniteMaterialCommands& MaterialCommands = Scene->NaniteMaterials[ENaniteMeshPass::BasePass];
	const FNaniteRasterPipelines& RasterPipelines = Scene->NaniteRasterPipelines[ENaniteMeshPass::BasePass];

	FRDGBufferDesc PickingFeedbackBufferDesc(FRDGBufferDesc::CreateStructuredDesc(sizeof(FNanitePickingFeedback), 1));
	PickingFeedbackBufferDesc.Usage |= BUF_SourceCopy;
	FRDGBufferRef PickingFeedback = GraphBuilder.CreateBuffer(PickingFeedbackBufferDesc, TEXT("Nanite.PickingFeedback"));
	FRDGBufferRef HitProxyIDBuffer = GSystemTextures.GetDefaultByteAddressBuffer(GraphBuilder, 4u); // NOTE: unused in this mode

	{
		FNanitePickingCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FNanitePickingCS::FParameters>();
		ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, PassParameters->ShaderPrintUniformBuffer);
		PassParameters->View = View.GetShaderParameters();
		PassParameters->Scene = View.GetSceneUniforms().GetBuffer(GraphBuilder);
		PassParameters->ShadingBinData = GetShadingBinDataSRV(GraphBuilder);
		PassParameters->ClusterPageData = Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
		PassParameters->VisualizeConfig = GetVisualizeConfig(NANITE_VISUALIZE_PICKING, /* bCompositeScene = */ false, GNaniteVisualizeEdgeDetect != 0);
		PassParameters->PageConstants = Data.PageConstants;
		PassParameters->MaxVisibleClusters = Data.MaxVisibleClusters;
		PassParameters->RenderFlags = Data.RenderFlags;
		PassParameters->RegularMaterialRasterBinCount = RasterPipelines.GetRegularBinCount();
		PassParameters->PickingPixelPos = FIntPoint((int32)VisualizationData.GetPickingMousePos().X, (int32)VisualizationData.GetPickingMousePos().Y);
		PassParameters->VisibleClustersSWHW = GraphBuilder.CreateSRV(Data.VisibleClustersSWHW);
		PassParameters->VisBuffer64 = Data.VisBuffer64;
		PassParameters->DbgBuffer64 = Data.DbgBuffer64;
		PassParameters->DbgBuffer32 = Data.DbgBuffer32;
		PassParameters->ShadingMask = Data.ShadingMask;
		PassParameters->SceneDepth = SceneTextures.Depth.Target;
		PassParameters->MaterialDepthTable = MaterialCommands.GetMaterialDepthSRV();
		PassParameters->MaterialHitProxyTable = GraphBuilder.CreateSRV(HitProxyIDBuffer);
		PassParameters->FeedbackBuffer = GraphBuilder.CreateUAV(PickingFeedback);

		auto PickingShader = View.ShaderMap->GetShader<FNanitePickingCS>();
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Nanite::Picking"),
			PickingShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	return PickingFeedback;
}

void DisplayPicking(const FScene* Scene, const FNanitePickingFeedback& PickingFeedback, uint32 RenderFlags, FScreenMessageWriter& Writer)
{
	const FNaniteVisualizationData& VisualizationData = GetNaniteVisualizationData();
	if (VisualizationData.GetActiveModeID() != NANITE_VISUALIZE_PICKING)
	{
		return;
	}

	switch (GNanitePickingDomain)
	{
	case NANITE_PICKING_DOMAIN_TRIANGLE:
		Writer.DrawLine(FText::FromString(TEXT("Domain [Triangle]")), 10, FColor::Yellow);
		break;

	case NANITE_PICKING_DOMAIN_CLUSTER:
		Writer.DrawLine(FText::FromString(TEXT("Domain [Cluster]")), 10, FColor::Yellow);
		break;

	case NANITE_PICKING_DOMAIN_INSTANCE:
		Writer.DrawLine(FText::FromString(TEXT("Domain [Instance]")), 10, FColor::Yellow);
		break;

	case NANITE_PICKING_DOMAIN_PRIMITIVE:
		Writer.DrawLine(FText::FromString(TEXT("Domain [Primitive]")), 10, FColor::Yellow);
		break;

	default:
		break; // Invalid picking domain
	}

	Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Pixel [%d:%d]"), PickingFeedback.PixelX, PickingFeedback.PixelY)), 10, FColor::Yellow);

	if (PickingFeedback.PrimitiveId == ~uint32(0))
	{
		return;
	}

	const int32 PickedPrimitiveIndex = Scene->GetPrimitiveIndex(FPersistentPrimitiveIndex{int32(PickingFeedback.PrimitiveId)});
	if (!Scene->PrimitiveSceneProxies.IsValidIndex(PickedPrimitiveIndex))
	{
		return;
	}

	const FPrimitiveSceneProxy* PickedSceneProxy = Scene->PrimitiveSceneProxies[PickedPrimitiveIndex];
	if (!PickedSceneProxy->IsNaniteMesh())
	{
		return;
	}

	const Nanite::FSceneProxyBase* PickedNaniteProxy = (const Nanite::FSceneProxyBase*)PickedSceneProxy;
	const FPrimitiveSceneInfo* PickedSceneInfo = Scene->Primitives[PickedPrimitiveIndex];

	Writer.EmptyLine();

	Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Persistent Index: %d"), PickingFeedback.PersistentIndex)), 10, FColor::Yellow);
	Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Primitive Id: %d"),     PickedPrimitiveIndex)),     10, FColor::Yellow);
	Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Instance Id: %d"),      PickingFeedback.InstanceId)),      10, FColor::Yellow);
	const FInstanceSceneDataBuffers *InstanceSceneDataBuffers = PickedNaniteProxy->GetInstanceSceneDataBuffers();
	int32 NumInstances = InstanceSceneDataBuffers ? InstanceSceneDataBuffers->GetNumInstances() : 0;
	Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Instance Count: %d"),   NumInstances)), 10, FColor::Yellow);

	Writer.EmptyLine();

	Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Page Index: %d"),          PickingFeedback.PageIndex)),         10, FColor::Yellow);
	Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Group Index: %d"),         PickingFeedback.GroupIndex)),        10, FColor::Yellow);
	Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Cluster Index: %d"),       PickingFeedback.ClusterIndex)),      10, FColor::Yellow);
	Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Triangle Index: %d"),      PickingFeedback.TriangleIndex)),     10, FColor::Yellow);
	Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Hierarchy Offset: %d"),    PickingFeedback.HierarchyOffset)),   10, FColor::Yellow);
	Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Runtime Resource Id: %d"), PickingFeedback.RuntimeResourceID)), 10, FColor::Yellow);

	Writer.EmptyLine();

	Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Raster Depth: %.6f"), *reinterpret_cast<const float*>(&PickingFeedback.DepthInt))), 10, FColor::Yellow);

	if (PickingFeedback.RasterMode == 1)
	{
		Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Raster Mode: Hardware"))), 10, FColor::Yellow);
	}
	else if (PickingFeedback.RasterMode == 2)
	{
		Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Raster Mode: Software"))), 10, FColor::Yellow);
	}

	Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Raster Bin: %d"), PickingFeedback.RasterBin)), 10, FColor::Yellow);

	Writer.EmptyLine();

	Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Shading Bin: %d"), PickingFeedback.ShadingBin)), 10, FColor::Yellow);

	if (PickingFeedback.MaterialMode == 0)
	{
		Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Material Mode: Fast"))), 10, FColor::Yellow);
	}
	else if (PickingFeedback.MaterialMode == 1)
	{
		Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Material Mode: Slow"))), 10, FColor::Yellow);
	}

	Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Material Index: %d"), PickingFeedback.MaterialIndex)), 10, FColor::Yellow);
	Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Material Count: %d"), PickingFeedback.MaterialCount)), 10, FColor::Yellow);
	Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Material Depth: %.6f"), *reinterpret_cast<const float*>(&PickingFeedback.MaterialDepthId))), 10, FColor::Yellow);
	Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Legacy Shading Id: %d"), PickingFeedback.LegacyShadingId)), 10, FColor::Yellow);

	Writer.EmptyLine();

	const TArray<Nanite::FSceneProxyBase::FMaterialSection>& PickedMaterialSections = PickedNaniteProxy->GetMaterialSections();
	if (int32(PickingFeedback.MaterialIndex) < PickedMaterialSections.Num())
	{
		const Nanite::FSceneProxyBase::FMaterialSection& PickedMaterialSection = PickedMaterialSections[PickingFeedback.MaterialIndex];

		if (PickedMaterialSection.ShadingMaterialProxy)
		{
			Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Shading Material [%s]"), *PickedMaterialSection.ShadingMaterialProxy->GetMaterialName())), 10, FColor::Yellow);
		}

		Writer.EmptyLine();

		FMaterialRenderProxy* FixedFunctionProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();

		const bool bDisableProgrammable = (RenderFlags & NANITE_RENDER_FLAG_DISABLE_PROGRAMMABLE) != 0;
		if (!bDisableProgrammable && PickedMaterialSection.RasterMaterialProxy && PickedMaterialSection.RasterMaterialProxy != FixedFunctionProxy)
		{
			Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Raster Material [%s]"), *PickedMaterialSection.RasterMaterialProxy->GetMaterialName())), 10, FColor::Yellow);
			const FMaterial& PickedRasterMaterial = PickedMaterialSection.RasterMaterialProxy->GetIncompleteMaterialWithFallback(Scene->GetFeatureLevel());

			Writer.DrawLine(FText::FromString(FString::Printf(TEXT("  Programmable:"))), 10, FColor::Yellow);

			if (PickedRasterMaterial.MaterialUsesDisplacement_RenderThread())
			{
				Writer.DrawLine(FText::FromString(FString::Printf(TEXT("  - Displacement Mapping"))), 10, FColor::Yellow);
			}

			if (PickedRasterMaterial.MaterialUsesWorldPositionOffset_RenderThread())
			{
				if (PickedNaniteProxy->EvaluateWorldPositionOffset())
				{
					Writer.DrawLine(FText::FromString(FString::Printf(TEXT("  - World Position Offset"))), 10, FColor::Yellow);
				}
				else
				{
					Writer.DrawLine(FText::FromString(FString::Printf(TEXT("  - World Position Offset [Disabled]"))), 10, FColor::Yellow);
				}
			}

			if (PickedRasterMaterial.MaterialUsesPixelDepthOffset_RenderThread())
			{
				Writer.DrawLine(FText::FromString(FString::Printf(TEXT("   - Pixel Depth Offset"))), 10, FColor::Yellow);
			}

			if (PickedRasterMaterial.IsMasked())
			{
				Writer.DrawLine(FText::FromString(FString::Printf(TEXT("   - Alpha Masking"))), 10, FColor::Yellow);
			}
		}
		else
		{
			Writer.DrawLine(FText::FromString(FString::Printf(TEXT("Raster Material [Fixed Function]"))), 10, FColor::Yellow);
		}
	}
}

void AddVisualizationPasses(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FSceneTextures& SceneTextures,
	const FEngineShowFlags& EngineShowFlags,
	TArrayView<const FViewInfo> Views,
	TArrayView<Nanite::FRasterResults> Results,
	FNanitePickingFeedback& PickingFeedback,
	FVirtualShadowMapArray&	VirtualShadowMapArray
)
{
	checkSlow(DoesPlatformSupportNanite(GMaxRHIShaderPlatform));

	const FNaniteVisualizationData& VisualizationData = GetNaniteVisualizationData();

	FRDGBufferRef PickingBuffer = nullptr;

	if (Scene && Views.Num() > 0 && VisualizationData.IsActive() && EngineShowFlags.VisualizeNanite)
	{
		// Don't create the hit proxy ID buffer until it's needed
		// TODO: Permutation with hit proxy support to keep this clean when !WITH_EDITOR?
		FRDGBufferRef HitProxyIDBuffer = GSystemTextures.GetDefaultByteAddressBuffer(GraphBuilder, 4u);
		bool bHitProxyIDBufferCreated = false;

		// These should always match 1:1
		if (ensure(Views.Num() == Results.Num()))
		{
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				const FViewInfo& View = Views[ViewIndex];
				Nanite::FRasterResults& Data = Results[ViewIndex];

				// Skip over secondary instanced stereo views, which use the primary view's data instead
				if (View.ShouldRenderView())
				{
					const int32 ViewWidth = View.ViewRectWithSecondaryViews.Max.X - View.ViewRectWithSecondaryViews.Min.X;
					const int32 ViewHeight = View.ViewRectWithSecondaryViews.Max.Y - View.ViewRectWithSecondaryViews.Min.Y;
					const FIntPoint ViewSize = FIntPoint(ViewWidth, ViewHeight);

					const FNaniteMaterialCommands& MaterialCommands = Scene->NaniteMaterials[ENaniteMeshPass::BasePass];
					const FNaniteRasterPipelines& RasterPipelines = Scene->NaniteRasterPipelines[ENaniteMeshPass::BasePass];

					LLM_SCOPE_BYTAG(Nanite);
					RDG_EVENT_SCOPE(GraphBuilder, "Nanite::Visualization");
					RDG_GPU_STAT_SCOPE(GraphBuilder, NaniteDebug);

					const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);
					const FIntPoint TileGridDim = FMath::DivideAndRoundUp(ViewSize, { 8, 8 });

					FRDGTextureRef VisBuffer64 = Data.VisBuffer64 ? Data.VisBuffer64 : SystemTextures.Black;
					FRDGTextureRef DbgBuffer64 = Data.DbgBuffer64 ? Data.DbgBuffer64 : SystemTextures.Black;
					FRDGTextureRef DbgBuffer32 = Data.DbgBuffer32 ? Data.DbgBuffer32 : SystemTextures.Black;
					FRDGTextureRef ShadingMask = Data.ShadingMask ? Data.ShadingMask : SystemTextures.Black;

					FRDGBufferRef RasterBinMeta = Data.RasterBinMeta ? Data.RasterBinMeta : GSystemTextures.GetDefaultStructuredBuffer<FNaniteRasterBinMeta>(GraphBuilder);

					FRDGBufferRef VisibleClustersSWHW = Data.VisibleClustersSWHW;

					// Debug picking feedback (mouse dependent, does not support stereo)
					if (VisualizationData.GetActiveModeID() == NANITE_VISUALIZE_PICKING && Views.Num() == 1)
					{
						PickingBuffer = PerformPicking(GraphBuilder, Scene, SceneTextures, Data, View);
					}

					Data.Visualizations.Reset();

					const bool bSingleVisualization = VisualizationData.GetActiveModeID() > 0;
					const bool bOverviewVisualization = VisualizationData.GetActiveModeID() == 0;

					if (bSingleVisualization)
					{
						// Single visualization
						FVisualizeResult Visualization = {};
						Visualization.ModeName = VisualizationData.GetActiveModeName();
						Visualization.ModeID = VisualizationData.GetActiveModeID();
						Visualization.bCompositeScene = VisualizationData.GetActiveModeDefaultComposited();
						Visualization.bSkippedTile = false;
						Data.Visualizations.Emplace(Visualization);
					}
					else if (bOverviewVisualization)
					{
						// Overview mode
						const auto& OverviewModeNames = VisualizationData.GetOverviewModeNames();
						for (const FName& ModeName : OverviewModeNames)
						{
							FVisualizeResult Visualization = {};
							Visualization.ModeName = ModeName;
							Visualization.ModeID = VisualizationData.GetModeID(Visualization.ModeName);
							Visualization.bCompositeScene = VisualizationData.GetModeDefaultComposited(Visualization.ModeName);
							Visualization.bSkippedTile = Visualization.ModeName == NAME_None;
							Data.Visualizations.Emplace(Visualization);
						}
					}

					bool bRequiresHitProxyIDs = false;
					bool bRequiresHiZDecode = false;
					for (FVisualizeResult& Visualization : Data.Visualizations)
					{
						if (Visualization.bSkippedTile)
						{
							continue;
						}

						bRequiresHitProxyIDs |= Visualization.ModeID == NANITE_VISUALIZE_HIT_PROXY_DEPTH;
						bRequiresHiZDecode |= VisualizationRequiresHiZDecode(Visualization.ModeID);
					}

				#if WITH_EDITOR
					if (bRequiresHitProxyIDs && !bHitProxyIDBufferCreated)
					{
						auto& MaterialsExtension = Scene->GetExtension<Nanite::FMaterialsSceneExtension>();
						HitProxyIDBuffer = MaterialsExtension.CreateHitProxyIDBuffer(GraphBuilder);
						bHitProxyIDBufferCreated = true;
					}
				#endif

					FRDGTextureRef DefaultUintVec4 = GSystemTextures.GetDefaultTexture(GraphBuilder, ETextureDimension::Texture2D, PF_R32G32B32A32_UINT, FUintVector4(0.0, 0.0, 0.0, 0.0));

					FRDGTextureRef SceneZDecoded = SystemTextures.Black;
					FRDGTextureRef SceneZLayout = DefaultUintVec4;
					FRDGTextureRef MaterialZDecoded = SystemTextures.Black;
					FRDGTextureRef MaterialZLayout = DefaultUintVec4;
					if (bRequiresHiZDecode && UseComputeDepthExport())
					{
						const uint32 PixelsWide = uint32(ViewSize.X);
						const uint32 PixelsTall = uint32(ViewSize.Y);
						const uint32 PlatformConfig = RHIGetHTilePlatformConfig(PixelsWide, PixelsTall);

						FRDGTextureDesc SceneZDecodedDesc = FRDGTextureDesc::Create2D(ViewSize, PF_R32_FLOAT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV);
						SceneZDecoded = GraphBuilder.CreateTexture(SceneZDecodedDesc, TEXT("Nanite.SceneZDecoded"));

						FRDGTextureDesc SceneZLayoutDesc = FRDGTextureDesc::Create2D(ViewSize, PF_R32G32B32A32_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV);
						SceneZLayout = GraphBuilder.CreateTexture(SceneZLayoutDesc, TEXT("Nanite.SceneZLayout"));

						FRDGTextureDesc MaterialZDecodedDesc = FRDGTextureDesc::Create2D(ViewSize, PF_R32_FLOAT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV);
						MaterialZDecoded = GraphBuilder.CreateTexture(MaterialZDecodedDesc, TEXT("Nanite.MaterialZDecoded"));

						FRDGTextureDesc MaterialZLayoutDesc = FRDGTextureDesc::Create2D(ViewSize, PF_R32G32B32A32_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV);
						MaterialZLayout = GraphBuilder.CreateTexture(MaterialZLayoutDesc, TEXT("Nanite.MaterialZLayout"));

						FDepthDecodeCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FDepthDecodeCS::FParameters>();
						PassParameters->View = View.ViewUniformBuffer;
						PassParameters->InViews = GraphBuilder.CreateSRV(Data.ViewsBuffer);
						PassParameters->ViewRect = FUint32Vector4((uint32)View.ViewRectWithSecondaryViews.Min.X, (uint32)View.ViewRectWithSecondaryViews.Min.Y, (uint32)View.ViewRectWithSecondaryViews.Max.X, (uint32)View.ViewRectWithSecondaryViews.Max.Y);
						PassParameters->HTileConfig = FUint32Vector4(PlatformConfig, PixelsWide, 0, 0);
						PassParameters->SceneDepth = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMetaData(SceneTextures.Depth.Target, ERDGTextureMetaDataAccess::CompressedSurface));
						PassParameters->ShadingMask = ShadingMask;
						PassParameters->MaterialDepthTable = MaterialCommands.GetMaterialDepthSRV();
						PassParameters->SceneHTileBuffer = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMetaData(SceneTextures.Depth.Target, ERDGTextureMetaDataAccess::HTile));
						PassParameters->MaterialHTileBuffer = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMetaData(Data.MaterialDepth, ERDGTextureMetaDataAccess::HTile));
						PassParameters->SceneZDecoded = GraphBuilder.CreateUAV(SceneZDecoded);
						PassParameters->SceneZLayout = GraphBuilder.CreateUAV(SceneZLayout);
						PassParameters->MaterialZDecoded = GraphBuilder.CreateUAV(MaterialZDecoded);
						PassParameters->MaterialZLayout = GraphBuilder.CreateUAV(MaterialZLayout);

						auto ComputeShader = View.ShaderMap->GetShader<FDepthDecodeCS>();
						FComputeShaderUtils::AddPass(
							GraphBuilder,
							RDG_EVENT_NAME("DepthDecode"),
							ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
							ComputeShader,
							PassParameters,
							FComputeShaderUtils::GetGroupCount(ViewSize, 8)
						);
					}

					for (FVisualizeResult& Visualization : Data.Visualizations)
					{
						if (Visualization.bSkippedTile)
						{
							continue;
						}

						// Apply force off/on scene composition
						if (GNaniteVisualizeComposite == 0)
						{
							// Force off
							Visualization.bCompositeScene = false;
						}
						else if (GNaniteVisualizeComposite == 1)
						{
							// Force on
							Visualization.bCompositeScene = true;
						}

						FRDGTextureDesc VisualizationOutputDesc = FRDGTextureDesc::Create2D(
							View.ViewRectWithSecondaryViews.Max,
							PF_A32B32G32R32F,
							FClearValueBinding::None,
							TexCreate_ShaderResource | TexCreate_UAV);

						Visualization.ModeOutput = GraphBuilder.CreateTexture(VisualizationOutputDesc, TEXT("Nanite.Visualization"));

						FNaniteVisualizeCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FNaniteVisualizeCS::FParameters>();

						FGraphicsPipelineRenderTargetsInfo RenderTargetsInfo;
						const uint32 ShadingExportCount = SceneTextures.Config.GetGBufferRenderTargetsInfo(RenderTargetsInfo);

						PassParameters->View = View.GetShaderParameters();
						PassParameters->Scene = View.GetSceneUniforms().GetBuffer(GraphBuilder);
						PassParameters->VirtualShadowMap = VirtualShadowMapArray.GetUniformBuffer();
						PassParameters->ClusterPageData = Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
						PassParameters->VisualizeConfig = GetVisualizeConfig(Visualization.ModeID, Visualization.bCompositeScene, GNaniteVisualizeEdgeDetect != 0);
						PassParameters->VisualizeScales = GetVisualizeScales(Visualization.ModeID, ShadingExportCount);
						PassParameters->PageConstants = Data.PageConstants;
						PassParameters->MaxVisibleClusters = Data.MaxVisibleClusters;
						PassParameters->RenderFlags = Data.RenderFlags;
						PassParameters->RegularMaterialRasterBinCount = RasterPipelines.GetRegularBinCount();
						PassParameters->PickingPixelPos = FIntPoint((int32)VisualizationData.GetPickingMousePos().X, (int32)VisualizationData.GetPickingMousePos().Y);
						PassParameters->VisibleClustersSWHW = GraphBuilder.CreateSRV(VisibleClustersSWHW);
						PassParameters->VisBuffer64 = VisBuffer64;
						PassParameters->DbgBuffer64 = DbgBuffer64;
						PassParameters->DbgBuffer32 = DbgBuffer32;
						PassParameters->ShadingMask = ShadingMask;
						PassParameters->SceneDepth = SceneTextures.Depth.Target;
						PassParameters->SceneZDecoded = SceneZDecoded;
						PassParameters->SceneZLayout = SceneZLayout;
						PassParameters->MaterialZDecoded = MaterialZDecoded;
						PassParameters->MaterialZLayout = MaterialZLayout;
						PassParameters->FastClearTileVis = GetFastClearTileVis(GraphBuilder);
						PassParameters->MaterialDepthTable = MaterialCommands.GetMaterialDepthSRV();
						PassParameters->MaterialHitProxyTable = GraphBuilder.CreateSRV(HitProxyIDBuffer);
						PassParameters->ShadingBinData = GetShadingBinDataSRV(GraphBuilder);
						PassParameters->RasterBinMeta = GraphBuilder.CreateSRV(RasterBinMeta);
						PassParameters->DebugOutput = GraphBuilder.CreateUAV(Visualization.ModeOutput);

						auto ComputeShader = View.ShaderMap->GetShader<FNaniteVisualizeCS>();
						FComputeShaderUtils::AddPass(
							GraphBuilder,
							RDG_EVENT_NAME("Nanite::Visualize"),
							ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
							ComputeShader,
							PassParameters,
							FComputeShaderUtils::GetGroupCount(ViewSize, 8)
						);
					}
				}
			}
		}
	}

	if (PickingBuffer != nullptr)
	{
		const int32 MaxPickingBuffers = Nanite::GGlobalResources.MaxPickingBuffers;

		int32& PickingBufferWriteIndex = Nanite::GGlobalResources.PickingBufferWriteIndex;
		int32& PickingBufferNumPending = Nanite::GGlobalResources.PickingBufferNumPending;

		TArray<TUniquePtr<FRHIGPUBufferReadback>>& PickingBuffers = Nanite::GGlobalResources.PickingBuffers;

		// Skip when queue is full. It is NOT safe to EnqueueCopy on a buffer that already has a pending copy
		if (PickingBufferNumPending < MaxPickingBuffers)
		{
			TUniquePtr<FRHIGPUBufferReadback>* GPUBufferReadback = &PickingBuffers[PickingBufferWriteIndex];
			if (!GPUBufferReadback->IsValid())
			{
				static const FName PickingFeedbackName(TEXT("Nanite.PickingFeedback"));
				PickingBuffers[PickingBufferWriteIndex] = MakeUnique<FRHIGPUBufferReadback>(PickingFeedbackName);
				GPUBufferReadback = &PickingBuffers[PickingBufferWriteIndex];
			}

			AddEnqueueCopyPass(GraphBuilder, GPUBufferReadback->Get(), PickingBuffer, 0);

			PickingBufferWriteIndex = (PickingBufferWriteIndex + 1) % MaxPickingBuffers;
			PickingBufferNumPending = FMath::Min(PickingBufferNumPending + 1, MaxPickingBuffers);
		}

		{
			TUniquePtr<FRHIGPUBufferReadback>* LatestPickingBuffer = nullptr;

			// Find latest buffer that is ready
			while (PickingBufferNumPending > 0)
			{
				uint32 Index = (PickingBufferWriteIndex + MaxPickingBuffers - PickingBufferNumPending) % MaxPickingBuffers;
				if (PickingBuffers[Index]->IsReady())
				{
					--PickingBufferNumPending;
					LatestPickingBuffer = &PickingBuffers[Index];
				}
				else
				{
					break;
				}
			}

			if (LatestPickingBuffer && LatestPickingBuffer->IsValid())
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(LockBuffer);
				const FNanitePickingFeedback* DataPtr = (const FNanitePickingFeedback*)(*LatestPickingBuffer)->Lock(sizeof(FNanitePickingFeedback));
				if (DataPtr)
				{
					PickingFeedback = *DataPtr;
					(*LatestPickingBuffer)->Unlock();
				}
			}
		}
	}
}

#if WITH_DEBUG_VIEW_MODES

void RenderDebugViewMode(
	FRDGBuilder& GraphBuilder,
	EDebugViewMode DebugViewMode,
	const FScene& Scene,
	const FViewInfo& View,
	const FSceneViewFamily& ViewFamily,
	const FRasterResults& RasterResults,
	FRDGTextureRef OutputColorTexture,
	FRDGTextureRef InputDepthTexture,
	FRDGTextureRef QuadOverdrawTexture
)
{
	LLM_SCOPE_BYTAG(Nanite);
	RDG_EVENT_SCOPE(GraphBuilder, "Nanite::DebugView");
	RDG_GPU_STAT_SCOPE(GraphBuilder, NaniteDebug);

	if (!FExportDebugViewPS::IsPlatformSupported(View.GetShaderPlatform()))
	{
		UE_CALL_ONCE([](){ UE_LOG(LogNanite, Error, TEXT("Platform does not support Nanite debug view shaders")); });
		return;
	}

	const uint32 GlobalShaderBudget = GetMaxShaderComplexityCount(View.GetFeatureLevel());

	// Increase the shader budget for Nanite meshes to account for baseline ALU overhead.
	const uint32 NaniteShaderBudget = GlobalShaderBudget + uint32(GNaniteVisualizeComplexityOverhead);

	const FLinearColor SelectionColor = GetSelectionColor(FLinearColor::White, true /* selected */, false /* hovered */, false /* use overlay intensity */);

	// TODO: Need to apply hover intensity to per-primitive wireframe color, not white
	//const FLinearColor HoveredColor = GetSelectionColor(FLinearColor::White, false /* selected */, true /* hovered */);

	const FNaniteMaterialCommands& MaterialCommands = Scene.NaniteMaterials[ENaniteMeshPass::BasePass];

	FExportDebugViewPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FExportDebugViewPS::FParameters>();
	PassParameters->View = View.GetShaderParameters();
	PassParameters->Scene = View.GetSceneUniforms().GetBuffer(GraphBuilder);
	PassParameters->VisibleClustersSWHW = GraphBuilder.CreateSRV(RasterResults.VisibleClustersSWHW);
	PassParameters->PageConstants = RasterResults.PageConstants;
	PassParameters->ViewRect = FIntVector4(View.ViewRect.Min.X, View.ViewRect.Min.Y, View.ViewRect.Max.X, View.ViewRect.Max.Y);
	PassParameters->InvShaderBudget = 1.0f / float(NaniteShaderBudget);
	PassParameters->SelectionColor = FVector3f(SelectionColor.R, SelectionColor.G, SelectionColor.B);
	PassParameters->DebugViewMode = uint32(DebugViewMode);
	PassParameters->ClusterPageData = Nanite::GStreamingManager.GetClusterPageDataSRV(GraphBuilder);
	PassParameters->VisBuffer64 = RasterResults.VisBuffer64;
	PassParameters->SceneDepth = InputDepthTexture;
	PassParameters->ShadingMask = RasterResults.ShadingMask;
	PassParameters->MaterialDepthTable = MaterialCommands.GetMaterialDepthSRV();
	PassParameters->MaterialEditorTable = MaterialCommands.GetMaterialEditorSRV();
	PassParameters->EditorSelectedHitProxyIds = GetEditorSelectedHitProxyIdsSRV(GraphBuilder, View);
	PassParameters->ShadingBinData = GetShadingBinDataSRV(GraphBuilder);

	PassParameters->MaterialHitProxyTable = GraphBuilder.CreateSRV(
	#if WITH_EDITOR
		Scene.GetExtension<Nanite::FMaterialsSceneExtension>().CreateHitProxyIDBuffer(GraphBuilder)
	#else
		// TODO: Permutation with hit proxy support to keep this clean?
		// For now, bind a valid SRV
		GSystemTextures.GetDefaultByteAddressBuffer(GraphBuilder, 4u)
	#endif
	);

	PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputColorTexture, ERenderTargetLoadAction::ELoad, 0);

#if WITH_EDITOR
	const uint32 HitProxyIdCount = View.EditorSelectedNaniteHitProxyIds.Num();
#else
	const uint32 HitProxyIdCount = 0;
#endif

	const uint32 SelectionCount = FMath::RoundUpToPowerOfTwo(HitProxyIdCount);
	const uint32 SearchBufferCountDim = FMath::Min(uint32(FExportDebugViewPS::FSearchBufferCountDim::MaxValue), FMath::FloorLog2(SelectionCount));

	FExportDebugViewPS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FExportDebugViewPS::FSearchBufferCountDim>(SearchBufferCountDim);

	auto PixelShader = View.ShaderMap->GetShader<FExportDebugViewPS>(PermutationVector.ToDimensionValueId());

	FPixelShaderUtils::AddFullscreenPass(
		GraphBuilder,
		View.ShaderMap,
		RDG_EVENT_NAME("Export Debug View"),
		PixelShader,
		PassParameters,
		View.ViewRect
	);
}

#endif // WITH_DEBUG_VIEW_MODES

} // namespace Nanite
