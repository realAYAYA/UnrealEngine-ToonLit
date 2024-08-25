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
#include "NaniteComposition.h"
#include "NaniteShading.h"
#include "Rendering/NaniteResources.h"
#include "Rendering/NaniteStreamingManager.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "SceneUtils.h"
#include "ScenePrivate.h"
#include "ScreenPass.h"
#include "GPUScene.h"
#include "ClearQuad.h"
#include "RendererModule.h"
#include "PixelShaderUtils.h"
#include "Lumen/LumenSceneCardCapture.h"
#include "Substrate/Substrate.h"
#include "SystemTextures.h"
#include "BasePassRendering.h"
#include "VariableRateShadingImageManager.h"
#include "Lumen/Lumen.h"
#include "ComponentRecreateRenderStateContext.h"
#include "InstanceDataSceneProxy.h"

extern TAutoConsoleVariable<int32> CVarParallelBasePassBuild;

static TAutoConsoleVariable<int32> CVarNaniteMultipleSceneViewsInOnePass(
	TEXT("r.Nanite.MultipleSceneViewsInOnePass"),
	1,
	TEXT("Supports rendering multiple views (FSceneView) whenever possible. Currently only ISR stereo rendering is supported."),
	ECVF_RenderThreadSafe
	);

extern int32 GNaniteShowStats;

static bool UseLegacyCulling()
{
	return !UseNaniteComputeMaterials();
}

BEGIN_SHADER_PARAMETER_STRUCT(FNaniteMaterialPassParameters, )
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

BEGIN_SHADER_PARAMETER_STRUCT(FNaniteEmitGBufferParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FNaniteMaterialPassParameters, Shading)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER(FNaniteIndirectMaterialVS, "/Engine/Private/Nanite/NaniteExportGBuffer.usf", "FullScreenVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FNaniteMultiViewMaterialVS, "/Engine/Private/Nanite/NaniteExportGBuffer.usf", "FullScreenVS", SF_Vertex);

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

namespace Nanite
{

FNaniteMaterialPassParameters CreateNaniteMaterialPassParams(
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
	FNaniteMaterialPassParameters Result;

	Result.MaterialIndirectArgs = MaterialIndirectArgs;

	Result.RecordArgBuffer = nullptr;

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

		UniformParameters->MaterialTileRemap = GraphBuilder.CreateSRV(MaterialTileRemap, PF_R32_UINT);

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

		UniformParameters->ShadingBinData = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<uint32>(GraphBuilder), PF_R32_UINT);
	
		Result.Nanite = GraphBuilder.CreateUniformBuffer(UniformParameters);
	}

	Result.View = View.GetShaderParameters(); // To get VTFeedbackBuffer
	Result.Scene = View.GetSceneUniforms().GetBuffer(GraphBuilder);
	const bool bLumenGIEnabled = SceneRenderer.IsLumenGIEnabled(View);
	Result.BasePass = CreateOpaqueBasePassUniformBuffer(GraphBuilder, View, 0, {}, DBufferTextures, bLumenGIEnabled);
	Result.ActiveShadingBin = ~uint32(0);

	return Result;
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

	const FNaniteMaterialCommands& MaterialCommands = Scene.NaniteMaterials[ENaniteMeshPass::BasePass];
	const int32 NumMaterialCommands = MaterialCommands.GetCommands().Num();

	// Classify materials for tile culling
	// TODO: Run velocity export in here instead of depth pre-pass?
	if(NumMaterialCommands > 0)
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

			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("Initialize Materials"), ComputeShader, PassParameters, DispatchDim);
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

			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("Classify Materials"), ComputeShader, PassParameters, DispatchDim);
		}

		// Finalize acceleration/indexing structures for tile classification
		{
			auto ComputeShader = View.ShaderMap->GetShader<FFinalizeMaterialsCS>();
			FFinalizeMaterialsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FFinalizeMaterialsCS::FParameters>();
			PassParameters->MaterialSlotCount		= HighestMaterialSlot;
			PassParameters->MaterialTileCount		= TileGridSize.X * TileGridSize.Y;
			PassParameters->MaterialIndirectArgs	= GraphBuilder.CreateUAV(FRDGBufferUAVDesc(MaterialIndirectArgs, PF_R32_UINT));

			const FIntVector DispatchDim = FComputeShaderUtils::GetGroupCount(PassParameters->MaterialSlotCount, 64);

			FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("Finalize Materials"), ComputeShader, PassParameters, DispatchDim);
		}
	}

	MaterialPassCommands.Reset(NumMaterialCommands);

	if (NumMaterialCommands > 0)
	{
		const bool bWPOInSecondPass = !IsUsingBasePassVelocity(View.GetShaderPlatform());

		FNaniteMaterialPassParameters TempParams = CreateNaniteMaterialPassParams(
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
			static_assert(UE_ARRAY_COUNT(PassGBufferLayouts) == (uint32)ENaniteMaterialPass::Max, "Unhandled Nanite material pass");

			TStaticArray<FTextureRenderTargetBinding, MaxSimultaneousRenderTargets> BasePassTextures;
			uint32 BasePassTextureCount = SceneTextures.GetGBufferRenderTargets(BasePassTextures, PassGBufferLayouts[PassIndex]);
			Substrate::AppendSubstrateMRTs(SceneRenderer, BasePassTextureCount, BasePassTextures);
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

		GraphBuilder.AddSetupTask([ParamsAndInfo, &MaterialCommands, &MaterialPassCommands, VisibilityQuery = RasterResults.VisibilityQuery]
		{
			TArray<FGraphicsPipelineRenderTargetsInfo, TFixedAllocator<(uint32)ENaniteMaterialPass::Max>> RTInfo;
			for (uint32 PassIndex = 0; PassIndex < ParamsAndInfo->NumPasses; ++PassIndex)
			{
				RTInfo.Emplace(ExtractRenderTargetsInfo(ParamsAndInfo->Params[PassIndex].RenderTargets));
			}
			TArrayView<FNaniteMaterialPassInfo> PassInfo = MakeArrayView(ParamsAndInfo->PassInfo, ParamsAndInfo->NumPasses);
			BuildNaniteMaterialPassCommands(RTInfo, MaterialCommands, Nanite::GetVisibilityResults(VisibilityQuery), MaterialPassCommands, PassInfo);

		}, Nanite::GetVisibilityTask(RasterResults.VisibilityQuery));

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
// TODO: WIP
#if 0
	if (UseNaniteComputeMaterials())
	{
		DispatchLumenMeshCapturePass(
			FRDGBuilder & GraphBuilder,
			FScene & Scene,
			FViewInfo * SharedView,
			TArrayView<const FCardPageRenderData> CardPagesToRender,
			const FRasterResults & RasterResults,
			const FRasterContext & RasterContext,
			FLumenCardPassUniformParameters * PassUniformParameters,
			FRDGBufferSRVRef RectMinMaxBufferSRV,
			uint32 NumRects,
			FIntPoint ViewportSize,
			FRDGTextureRef AlbedoAtlasTexture,
			FRDGTextureRef NormalAtlasTexture,
			FRDGTextureRef EmissiveAtlasTexture,
			FRDGTextureRef DepthAtlasTexture
		);
		return;
	}
#endif

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
		MarkStencilRects(
			GraphBuilder,
			RasterContext,
			Scene,
			SharedView,
			ViewportSize,
			NumRects,
			RectMinMaxBufferSRV,
			DepthAtlasTexture
		);
	}

	// Emit material IDs as depth values
	{
		EmitMaterialIdRects(
			GraphBuilder,
			RasterResults,
			RasterContext,
			Scene,
			SharedView,
			ViewportSize,
			NumRects,
			RectMinMaxBufferSRV,
			DepthAtlasTexture
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

			UniformParameters->ShadingBinData				= GraphBuilder.CreateSRV(GSystemTextures.GetDefaultByteAddressBuffer(GraphBuilder, 4u));

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
		EmitMaterialDepthRects(
			GraphBuilder,
			RasterContext,
			Scene,
			SharedView,
			ViewportSize,
			NumRects,
			RectMinMaxBufferSRV,
			DepthAtlasTexture
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
	const uint32 MaterialSlotReserve = FMath::RoundUpToPowerOfTwo(FMath::Max(NumMaterialSlots, 256u));

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
#endif
#if WITH_DEBUG_VIEW_MODES
	check(MaterialEditorDataBuffer);
	check(MaterialEditorDataBuffer->GetSize() == MaterialSlotReserve * sizeof(uint32));
#endif
	check(MaterialDepthDataBuffer);
	check(MaterialDepthDataBuffer->GetSize() == MaterialSlotReserve * sizeof(uint32));

	FNaniteMaterialCommands::FUploader* Uploader = GraphBuilder.AllocObject<FNaniteMaterialCommands::FUploader>();
	Uploader->MaxMaterials = MaxMaterials;

	NumPrimitiveUpdates = InNumPrimitiveUpdates;

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
	LockIfValid(MaterialDepthUploader);
#if WITH_DEBUG_VIEW_MODES
	LockIfValid(MaterialEditorUploader);
#endif

	for (const FMaterialUploadEntry& MaterialEntry : DirtyMaterialEntries)
	{
		*static_cast<uint32*>(MaterialDepthUploader->Add_GetRef(MaterialEntry.MaterialSlot)) = MaterialEntry.MaterialId;
	#if WITH_DEBUG_VIEW_MODES
		*static_cast<uint32*>(MaterialEditorUploader->Add_GetRef(MaterialEntry.MaterialSlot)) = MaterialEntry.InstructionCount;
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
	UnlockIfValid(MaterialDepthUploader);
#if WITH_DEBUG_VIEW_MODES
	UnlockIfValid(MaterialEditorUploader);
#endif
}

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
