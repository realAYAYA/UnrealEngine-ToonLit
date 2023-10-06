// Copyright Epic Games, Inc. All Rights Reserved.

#include "LumenSceneCardCapture.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "NaniteSceneProxy.h"
#include "StaticMeshBatch.h"
#include "MeshPassProcessor.inl"
#include "MeshCardRepresentation.h"

static TAutoConsoleVariable<float> GLumenSceneSurfaceCacheMeshTargetScreenSize(
	TEXT("r.LumenScene.SurfaceCache.MeshTargetScreenSize"),
	0.15f,
	TEXT("Controls which LOD level will be used to capture static meshes into surface cache."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		Lumen::DebugResetSurfaceCache();
	}),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> GLumenSceneSurfaceCacheNaniteLODScaleFactor(
	TEXT("r.LumenScene.SurfaceCache.NaniteLODScaleFactor"),
	1.0f,
	TEXT("Controls which LOD level will be used to capture Nanite meshes into surface cache."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		Lumen::DebugResetSurfaceCache();
	}),
	ECVF_RenderThreadSafe);

namespace LumenCardCapture
{
	constexpr int32 LandscapeLOD = 0;
};

bool ShouldCompileLumenMeshCardShaders(const FMeshMaterialShaderPermutationParameters& Parameters)
{
	return Parameters.MaterialParameters.MaterialDomain == MD_Surface
		&& Parameters.VertexFactoryType->SupportsLumenMeshCards()
		&& IsOpaqueOrMaskedBlendMode(Parameters.MaterialParameters.BlendMode)
		&& DoesPlatformSupportLumenGI(Parameters.Platform);
}

class FLumenCardVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FLumenCardVS, MeshMaterial);

protected:

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return ShouldCompileLumenMeshCardShaders(Parameters);
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

		return ShouldCompileLumenMeshCardShaders(Parameters);
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
		// Use fully simplified material for less complex shaders when multiple slabs are used.
		OutEnvironment.SetDefine(TEXT("STRATA_USE_FULLYSIMPLIFIED_MATERIAL"), 1);
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FLumenCardPS<false>, TEXT("/Engine/Private/Lumen/LumenCardPixelShader.usf"), TEXT("Main"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>, FLumenCardPS<true>, TEXT("/Engine/Private/Lumen/LumenCardPixelShader.usf"), TEXT("Main"), SF_Pixel);

class FLumenCardMeshProcessor : public FSceneRenderingAllocatorObject<FLumenCardMeshProcessor>, public FMeshPassProcessor
{
public:

	FLumenCardMeshProcessor(const FScene* Scene, ERHIFeatureLevel::Type FeatureLevel, const FSceneView* InViewIfDynamicMeshCommand, const FMeshPassProcessorRenderState& InPassDrawRenderState, FMeshPassDrawListContext* InDrawListContext);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final;
	virtual void CollectPSOInitializers(const FSceneTexturesConfig& SceneTexturesConfig, const FMaterial& Material, const FPSOPrecacheVertexFactoryData& VertexFactoryData, const FPSOPrecacheParams& PreCacheParams, TArray<FPSOPrecacheData>& PSOInitializers) override final;

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
					const FMaterialShadingModelField ShadingModels = Material.GetShadingModels();
					const bool bIsTranslucent = IsTranslucentBlendMode(Material);
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

void LumenScene::AllocateCardCaptureAtlas(FRDGBuilder& GraphBuilder, FIntPoint CardCaptureAtlasSize, FCardCaptureAtlas& CardCaptureAtlas)
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

void FLumenCardMeshProcessor::CollectPSOInitializers(const FSceneTexturesConfig& SceneTexturesConfig, const FMaterial& Material, const FPSOPrecacheVertexFactoryData& VertexFactoryData, const FPSOPrecacheParams& PreCacheParams, TArray<FPSOPrecacheData>& PSOInitializers)
{
	LLM_SCOPE_BYTAG(Lumen);

	if (!PreCacheParams.bRenderInMainPass || !PreCacheParams.bAffectDynamicIndirectLighting ||
		!Lumen::ShouldPrecachePSOs(GetFeatureLevelShaderPlatform(FeatureLevel)))
	{
		return;
	}

	const FMaterialShadingModelField ShadingModels = Material.GetShadingModels();
	const bool bIsTranslucent = IsTranslucentBlendMode(Material);
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
			VertexFactoryData.VertexFactoryType,
			PassShaders.VertexShader,
			PassShaders.PixelShader))
		{
			return;
		}

		FGraphicsPipelineRenderTargetsInfo RenderTargetsInfo;
		SetupCardCaptureRenderTargetsInfo(RenderTargetsInfo);

		AddGraphicsPipelineStateInitializer(
			VertexFactoryData,
			Material,
			PassDrawRenderState,
			RenderTargetsInfo,
			PassShaders,
			MeshFillMode,
			MeshCullMode,
			(EPrimitiveType)PreCacheParams.PrimitiveType,
			EMeshPassFeatures::Default, 
			true /*bRequired*/,
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
	virtual void CollectPSOInitializers(const FSceneTexturesConfig& SceneTexturesConfig, const FMaterial& Material, const FPSOPrecacheVertexFactoryData& VertexFactoryData, const FPSOPrecacheParams& PreCacheParams, TArray<FPSOPrecacheData>& PSOInitializers) override final;

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

	checkf(LumenScene::HasPrimitiveNaniteMeshBatches(PrimitiveSceneProxy) && DoesPlatformSupportLumenGI(GetFeatureLevelShaderPlatform(FeatureLevel)),
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
	check(Nanite::IsSupportedBlendMode(Material));
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
	const FPSOPrecacheVertexFactoryData& VertexFactoryData,
	const FPSOPrecacheParams& PreCacheParams,
	TArray<FPSOPrecacheData>& PSOInitializers)
{
	EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform(FeatureLevel);

	// Make sure Nanite rendering is supported.
	if (!UseNanite(ShaderPlatform) || !SupportsNaniteRendering(VertexFactoryData.VertexFactoryType, Material, FeatureLevel))
	{
		return;
	}

	if (!Nanite::IsSupportedBlendMode(Material) || Material.GetMaterialDomain() ||
		!Lumen::ShouldPrecachePSOs(ShaderPlatform))
	{
		return;
	}

	// Nanite passes always use the forced fixed vertex element and not custom default vertex declaration even if it's provided
	FPSOPrecacheVertexFactoryData NaniteVertexFactoryData = VertexFactoryData;
	NaniteVertexFactoryData.CustomDefaultVertexDeclaration = nullptr;

	const ERasterizerFillMode MeshFillMode = FM_Solid;
	const ERasterizerCullMode MeshCullMode = CM_None;

	TShaderMapRef<FNaniteMultiViewMaterialVS> VertexShader(GetGlobalShaderMap(FeatureLevel));

	FLumenCardNanitePassShaders PassShaders;
	PassShaders.VertexShader = VertexShader;

	constexpr bool bMultiViewCapture = true;

	FMaterialShaderTypes ShaderTypes;
	ShaderTypes.AddShaderType<FLumenCardPS<bMultiViewCapture>>();

	FMaterialShaders Shaders;
	if (!Material.TryGetShaders(ShaderTypes, NaniteVertexFactoryData.VertexFactoryType, Shaders))
	{
		return;
	}

	Shaders.TryGetPixelShader(PassShaders.PixelShader);

	FGraphicsPipelineRenderTargetsInfo RenderTargetsInfo;
	SetupCardCaptureRenderTargetsInfo(RenderTargetsInfo);

	AddGraphicsPipelineStateInitializer(
		NaniteVertexFactoryData,
		Material,
		PassDrawRenderState,
		RenderTargetsInfo,
		PassShaders,
		MeshFillMode,
		MeshCullMode,
		(EPrimitiveType)PreCacheParams.PrimitiveType,
		EMeshPassFeatures::Default,
		true /*bRequired*/,
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
	, CardUVRect(InCardUVRect)
	, CardCaptureAtlasRect(InCardCaptureAtlasRect)
	, SurfaceCacheAtlasRect(InSurfaceCacheAtlasRect)
	, CardWorldOBB(InLumenCard.WorldOBB)
	, bResampleLastLighting(bInResampleLastLighting)
{
	ensure(CardIndex >= 0 && PageTableIndex >= 0);

	NaniteLODScaleFactor = GLumenSceneSurfaceCacheNaniteLODScaleFactor.GetValueOnRenderThread();

	UpdateViewMatrices(InMainView);
}

FCardPageRenderData::~FCardPageRenderData() = default;

void FCardPageRenderData::UpdateViewMatrices(const FViewInfo& MainView)
{
	ensureMsgf(FVector::DotProduct(CardWorldOBB.AxisX, FVector::CrossProduct(CardWorldOBB.AxisY, CardWorldOBB.AxisZ)) < 0.0f, TEXT("Card has wrong handedness"));

	FMatrix ViewRotationMatrix = FMatrix::Identity;
	ViewRotationMatrix.SetColumn(0, CardWorldOBB.AxisX);
	ViewRotationMatrix.SetColumn(1, CardWorldOBB.AxisY);
	ViewRotationMatrix.SetColumn(2, -CardWorldOBB.AxisZ);

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

	// We do not want FauxOrtho projection moving the camera origin far away from the card since we have just setup the correct projection.
	// That can result in low accuracy when using world position even with LWC.
	Initializer.bUseFauxOrthoViewPos = false;

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
	View->CachedViewUniformShaderParameters->OverrideLandscapeLOD = LumenCardCapture::LandscapeLOD;
}

void LumenScene::AddCardCaptureDraws(
	const FScene* Scene,
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
					LODToRender = LumenCardCapture::LandscapeLOD;
				}
				else
				{
					const float TargetScreenSize = GLumenSceneSurfaceCacheMeshTargetScreenSize.GetValueOnRenderThread();

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