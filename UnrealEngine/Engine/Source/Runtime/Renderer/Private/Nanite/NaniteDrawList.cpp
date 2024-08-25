// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteDrawList.h"
#include "BasePassRendering.h"
#include "NaniteSceneProxy.h"
#include "NaniteShading.h"
#include "NaniteVertexFactory.h"
#include "SceneUtils.h"
#include "ScenePrivate.h"
#include "MeshPassProcessor.inl"

int32 GNaniteMaterialSortMode = 4;
static FAutoConsoleVariableRef CVarNaniteMaterialSortMode(
	TEXT("r.Nanite.MaterialSortMode"),
	GNaniteMaterialSortMode,
	TEXT("Method of sorting Nanite material draws. 0=disabled, 1=shader, 2=sortkey, 3=refcount"),
	ECVF_RenderThreadSafe
);

int32 GNaniteAllowWPODistanceDisable = 1;
static FAutoConsoleVariableRef CVarNaniteAllowWPODistanceDisable(
	TEXT("r.Nanite.AllowWPODistanceDisable"),
	GNaniteAllowWPODistanceDisable,
	TEXT("Whether or not to allow disabling World Position Offset for Nanite instances at a distance from the camera."),
	ECVF_ReadOnly
);

FMeshDrawCommand& FNaniteDrawListContext::AddCommand(FMeshDrawCommand& Initializer, uint32 NumElements)
{
	checkf(CurrentPrimitiveSceneInfo != nullptr, TEXT("BeginPrimitiveSceneInfo() must be called on the context before adding commands"));
	checkf(CurrentMeshPass < ENaniteMeshPass::Num, TEXT("BeginMeshPass() must be called on the context before adding commands"));

	{
		MeshDrawCommandForStateBucketing.~FMeshDrawCommand();
		new(&MeshDrawCommandForStateBucketing) FMeshDrawCommand();
	}

	MeshDrawCommandForStateBucketing = Initializer;
	return MeshDrawCommandForStateBucketing;
}

void FNaniteDrawListContext::BeginPrimitiveSceneInfo(FPrimitiveSceneInfo& PrimitiveSceneInfo)
{
	checkf(CurrentPrimitiveSceneInfo == nullptr, TEXT("BeginPrimitiveSceneInfo() was called without a matching EndPrimitiveSceneInfo()"));
	check(PrimitiveSceneInfo.Proxy->IsNaniteMesh());

	Nanite::FSceneProxyBase* NaniteSceneProxy = static_cast<Nanite::FSceneProxyBase*>(PrimitiveSceneInfo.Proxy);
	const int32 NumMaterialSections = NaniteSceneProxy->GetMaterialSections().Num();

	// Pre-allocate the max possible material slots for the slot arrays here, before contexts are applied serially.
	for (auto& MaterialSlots : PrimitiveSceneInfo.NaniteMaterialSlots)
	{
		MaterialSlots.Empty(NumMaterialSections);
	}

	CurrentPrimitiveSceneInfo = &PrimitiveSceneInfo;
}

void FNaniteDrawListContext::EndPrimitiveSceneInfo()
{
	checkf(CurrentPrimitiveSceneInfo != nullptr, TEXT("EndPrimitiveSceneInfo() was called without matching BeginPrimitiveSceneInfo()"));
	CurrentPrimitiveSceneInfo = nullptr;
}

void FNaniteDrawListContext::BeginMeshPass(ENaniteMeshPass::Type MeshPass)
{
	checkf(CurrentMeshPass == ENaniteMeshPass::Num, TEXT("BeginMeshPass() was called without a matching EndMeshPass()"));
	check(MeshPass < ENaniteMeshPass::Num);
	CurrentMeshPass = MeshPass;
}

void FNaniteDrawListContext::EndMeshPass()
{
	checkf(CurrentMeshPass < ENaniteMeshPass::Num, TEXT("EndMeshPass() was called without matching BeginMeshPass()"));
	CurrentMeshPass = ENaniteMeshPass::Num;
}

FNaniteMaterialSlot& FNaniteDrawListContext::GetMaterialSlotForWrite(FPrimitiveSceneInfo& PrimitiveSceneInfo, ENaniteMeshPass::Type MeshPass, uint8 SectionIndex)
{	
	TArray<FNaniteMaterialSlot>& MaterialSlots = PrimitiveSceneInfo.NaniteMaterialSlots[MeshPass];

	// Initialize material slots if they haven't been already
	// NOTE: Lazily initializing them like this prevents adding material slots for primitives that have no bins in the pass
	if (MaterialSlots.Num() == 0)
	{
		check(PrimitiveSceneInfo.Proxy->IsNaniteMesh());
		check(PrimitiveSceneInfo.NaniteCommandInfos[MeshPass].Num() == 0);
		check(PrimitiveSceneInfo.NaniteRasterBins[MeshPass].Num() == 0);
		check(PrimitiveSceneInfo.NaniteShadingBins[MeshPass].Num() == 0);

		auto* NaniteSceneProxy = static_cast<const Nanite::FSceneProxyBase*>(PrimitiveSceneInfo.Proxy);
		const int32 NumMaterialSections = NaniteSceneProxy->GetMaterialSections().Num();

		MaterialSlots.SetNumUninitialized(NumMaterialSections);
		FMemory::Memset(MaterialSlots.GetData(), 0xFF, NumMaterialSections * MaterialSlots.GetTypeSize());
	}

	check(MaterialSlots.IsValidIndex(SectionIndex));
	return MaterialSlots[SectionIndex];
}

void FNaniteDrawListContext::AddShadingCommand(FPrimitiveSceneInfo& PrimitiveSceneInfo, const FNaniteCommandInfo& ShadingCommand, ENaniteMeshPass::Type MeshPass, uint8 SectionIndex)
{
	FNaniteMaterialSlot& MaterialSlot = GetMaterialSlotForWrite(PrimitiveSceneInfo, MeshPass, SectionIndex);
	check(MaterialSlot.LegacyShadingId == 0xFFFFu);
	MaterialSlot.LegacyShadingId = uint16(ShadingCommand.GetMaterialSlot());

	PrimitiveSceneInfo.NaniteCommandInfos[MeshPass].Add(ShadingCommand);
}

void FNaniteDrawListContext::AddShadingBin(FPrimitiveSceneInfo& PrimitiveSceneInfo, const FNaniteShadingBin& ShadingBin, ENaniteMeshPass::Type MeshPass, uint8 SectionIndex)
{
	FNaniteMaterialSlot& MaterialSlot = GetMaterialSlotForWrite(PrimitiveSceneInfo, MeshPass, SectionIndex);
	check(MaterialSlot.ShadingBin == 0xFFFFu);
	MaterialSlot.ShadingBin = ShadingBin.BinIndex;

	PrimitiveSceneInfo.NaniteShadingBins[MeshPass].Add(ShadingBin);
}

void FNaniteDrawListContext::AddRasterBin(
	FPrimitiveSceneInfo& PrimitiveSceneInfo,
	const FNaniteRasterBin& PrimaryRasterBin,
	const FNaniteRasterBin& SecondaryRasterBin,
	ENaniteMeshPass::Type MeshPass,
	uint8 SectionIndex)
{
	check(PrimaryRasterBin.IsValid());
	
	FNaniteMaterialSlot& MaterialSlot = GetMaterialSlotForWrite(PrimitiveSceneInfo, MeshPass, SectionIndex);
	check(MaterialSlot.RasterBin == 0xFFFFu);
	MaterialSlot.RasterBin = PrimaryRasterBin.BinIndex;
	MaterialSlot.SecondaryRasterBin = SecondaryRasterBin.BinIndex;
	
	PrimitiveSceneInfo.NaniteRasterBins[MeshPass].Add(PrimaryRasterBin);
	if (SecondaryRasterBin.IsValid())
	{
		PrimitiveSceneInfo.NaniteRasterBins[MeshPass].Add(SecondaryRasterBin);
	}
}

void FNaniteDrawListContext::FinalizeCommand(
	const FMeshBatch& MeshBatch,
	int32 BatchElementIndex,
	const FMeshDrawCommandPrimitiveIdInfo& IdInfo,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode,
	FMeshDrawCommandSortKey SortKey,
	EFVisibleMeshDrawCommandFlags Flags,
	const FGraphicsMinimalPipelineStateInitializer& PipelineState,
	const FMeshProcessorShaders* ShadersForDebugging,
	FMeshDrawCommand& MeshDrawCommand
)
{
	checkf(CurrentPrimitiveSceneInfo != nullptr, TEXT("BeginPrimitiveSceneInfo() must be called on the context before finalizing commands"));
	checkf(CurrentMeshPass < ENaniteMeshPass::Num, TEXT("BeginMeshPass() must be called on the context before finalizing commands"));

	FGraphicsMinimalPipelineStateId PipelineId;
	PipelineId = FGraphicsMinimalPipelineStateId::GetPersistentId(PipelineState);
	MeshDrawCommand.SetDrawParametersAndFinalize(MeshBatch, BatchElementIndex, PipelineId, ShadersForDebugging);
#if UE_BUILD_DEBUG
	FMeshDrawCommand MeshDrawCommandDebug = FMeshDrawCommand(MeshDrawCommand);
	check(MeshDrawCommandDebug.ShaderBindings.GetDynamicInstancingHash() == MeshDrawCommand.ShaderBindings.GetDynamicInstancingHash());
	check(MeshDrawCommandDebug.GetDynamicInstancingHash() == MeshDrawCommand.GetDynamicInstancingHash());
#endif

#if MESH_DRAW_COMMAND_DEBUG_DATA
	// When using state buckets, multiple PrimitiveSceneProxies can use the same 
	// MeshDrawCommand, so The PrimitiveSceneProxy pointer can't be stored.
	MeshDrawCommand.ClearDebugPrimitiveSceneProxy();
#endif

#if WITH_DEBUG_VIEW_MODES
	uint32 NumPSInstructions = 0;
	uint32 NumVSInstructions = 0;
	if (ShadersForDebugging != nullptr)
	{
		NumPSInstructions = ShadersForDebugging->PixelShader->GetNumInstructions();
		NumVSInstructions = ShadersForDebugging->VertexShader->GetNumInstructions();
	}

	const uint32 InstructionCount = static_cast<uint32>(NumPSInstructions << 16u | NumVSInstructions);
#endif

	const ERHIFeatureLevel::Type FeatureLevel = CurrentPrimitiveSceneInfo->Scene->GetFeatureLevel();
	const bool bWPOEnabled = MeshBatch.MaterialRenderProxy && MeshBatch.MaterialRenderProxy->GetIncompleteMaterialWithFallback(FeatureLevel).MaterialUsesWorldPositionOffset_RenderThread();

	// Defer the command
	DeferredCommands[CurrentMeshPass].Add(
		FDeferredCommand {
			CurrentPrimitiveSceneInfo,
			MeshDrawCommand,
			FNaniteMaterialEntryMap::ComputeHash(MeshDrawCommand),
		#if WITH_DEBUG_VIEW_MODES
			InstructionCount,
		#endif
			MeshBatch.SegmentIndex,
			bWPOEnabled
		}
	);
}

void FNaniteDrawListContext::Apply(FScene& Scene)
{
	check(IsInParallelRenderingThread());

	const bool bUseComputeMaterials = UseNaniteComputeMaterials();

	for (int32 MeshPass = 0; MeshPass < ENaniteMeshPass::Num; ++MeshPass)
	{
		FNaniteMaterialCommands& ShadingCommands = Scene.NaniteMaterials[MeshPass];
		FNaniteRasterPipelines& RasterPipelines  = Scene.NaniteRasterPipelines[MeshPass];
		FNaniteShadingPipelines& ShadingPipelines = Scene.NaniteShadingPipelines[MeshPass];
		FNaniteVisibility& Visibility = Scene.NaniteVisibility[MeshPass];

		for (auto& Command : DeferredCommands[MeshPass])
		{
			uint32 InstructionCount = 0;
		#if WITH_DEBUG_VIEW_MODES
			InstructionCount = Command.InstructionCount;
		#endif
			FPrimitiveSceneInfo* PrimitiveSceneInfo = Command.PrimitiveSceneInfo;
			FNaniteCommandInfo CommandInfo = ShadingCommands.Register(Command.MeshDrawCommand, Command.CommandHash, InstructionCount, Command.bWPOEnabled);
			AddShadingCommand(*PrimitiveSceneInfo, CommandInfo, ENaniteMeshPass::Type(MeshPass), Command.SectionIndex);

			FNaniteVisibility::PrimitiveShadingDrawType* ShadingDraws = !bUseComputeMaterials ? Visibility.GetShadingDrawReferences(PrimitiveSceneInfo) : nullptr;
			if (ShadingDraws)
			{
				ShadingDraws->Add(Command.CommandHash.AsUInt());
			}
		}

		for (const FDeferredPipelines& PipelinesCommand : DeferredPipelines[MeshPass])
		{
			FPrimitiveSceneInfo* PrimitiveSceneInfo = PipelinesCommand.PrimitiveSceneInfo;
			FNaniteVisibility::PrimitiveRasterBinType*  RasterBins  = Visibility.GetRasterBinReferences(PrimitiveSceneInfo);
			FNaniteVisibility::PrimitiveShadingBinType* ShadingBins = bUseComputeMaterials ? Visibility.GetShadingBinReferences(PrimitiveSceneInfo) : nullptr;

			check(!bUseComputeMaterials || (PipelinesCommand.RasterPipelines.Num() == PipelinesCommand.ShadingPipelines.Num()));
			const int32 MaterialSectionCount = PipelinesCommand.RasterPipelines.Num();
			for (int32 MaterialSectionIndex = 0; MaterialSectionIndex < MaterialSectionCount; ++MaterialSectionIndex)
			{
				// Register raster bin
				{
					const FNaniteRasterPipeline& RasterPipeline = PipelinesCommand.RasterPipelines[MaterialSectionIndex];
					FNaniteRasterBin PrimaryRasterBin = RasterPipelines.Register(RasterPipeline);

					// Check to register a secondary bin (used to disable WPO at a distance)
					FNaniteRasterBin SecondaryRasterBin;
					FNaniteRasterPipeline SecondaryRasterPipeline;
					if (GNaniteAllowWPODistanceDisable && RasterPipeline.GetSecondaryPipeline(SecondaryRasterPipeline))
					{
						SecondaryRasterBin = RasterPipelines.Register(SecondaryRasterPipeline);
					}

					AddRasterBin(*PrimitiveSceneInfo, PrimaryRasterBin, SecondaryRasterBin, ENaniteMeshPass::Type(MeshPass), uint8(MaterialSectionIndex));

					if (RasterBins)
					{
						RasterBins->Add(FNaniteVisibility::FRasterBin{ PrimaryRasterBin.BinIndex, SecondaryRasterBin.BinIndex });
					}
				}

				// Register shading bin
				if (bUseComputeMaterials)
				{
					const FNaniteShadingPipeline& ShadingPipeline = PipelinesCommand.ShadingPipelines[MaterialSectionIndex];
					const FNaniteShadingBin ShadingBin = ShadingPipelines.Register(ShadingPipeline);
					AddShadingBin(*PrimitiveSceneInfo, ShadingBin, ENaniteMeshPass::Type(MeshPass), uint8(MaterialSectionIndex));

					if (ShadingBins)
					{
						ShadingBins->Add(FNaniteVisibility::FShadingBin{ ShadingBin.BinIndex });
					}
				}
			}

			// This will register the primitive's raster bins for custom depth, if necessary
			PrimitiveSceneInfo->RefreshNaniteRasterBins();
		}
	}
}

void SubmitNaniteIndirectMaterial(
	const FNaniteMaterialPassCommand& MaterialPassCommand,
	const TShaderMapRef<FNaniteIndirectMaterialVS>& VertexShader,
	const FGraphicsMinimalPipelineStateSet& GraphicsMinimalPipelineStateSet,
	const uint32 InstanceFactor,
	FRHICommandList& RHICmdList,
	FRHIBuffer* MaterialIndirectArgs,
	FMeshDrawCommandStateCache& StateCache)
{
	const FMeshDrawCommand& MeshDrawCommand	= MaterialPassCommand.MeshDrawCommand;
	const float MaterialDepth				= MaterialPassCommand.MaterialDepth;
	const int32 MaterialSlot				= MaterialPassCommand.MaterialSlot;

#if WANTS_DRAW_MESH_EVENTS
	FMeshDrawCommand::FMeshDrawEvent MeshEvent(MeshDrawCommand, InstanceFactor, RHICmdList);
#endif

	FMeshDrawCommandSceneArgs SceneArgs;
	bool bAllowSkipDrawCommand = true;
	if (!FMeshDrawCommand::SubmitDrawIndirectBegin(MeshDrawCommand, GraphicsMinimalPipelineStateSet, SceneArgs, InstanceFactor, RHICmdList, StateCache, bAllowSkipDrawCommand))
	{
		return;
	}

	// All Nanite mesh draw commands are using the same vertex shader, which has a material depth parameter we assign at render time.
	{
		FNaniteIndirectMaterialVS::FParameters Parameters;
		Parameters.MaterialDepth = MaterialDepth;
		Parameters.MaterialSlot = uint32(MaterialSlot);
		Parameters.TileRemapCount = FMath::DivideAndRoundUp(InstanceFactor, 32u);
		SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), Parameters);
	}

	check(MaterialIndirectArgs == nullptr || MaterialSlot != INDEX_NONE);
	const uint32 IndirectArgSize = sizeof(FRHIDrawIndexedIndirectParameters) + sizeof(FRHIDispatchIndirectParametersNoPadding);
	const uint32 MaterialSlotIndirectOffset = MaterialIndirectArgs != nullptr ? IndirectArgSize * uint32(MaterialSlot) : 0;
	
	SceneArgs.IndirectArgsBuffer = MaterialIndirectArgs;
	SceneArgs.IndirectArgsByteOffset = MaterialSlotIndirectOffset;
	FMeshDrawCommand::SubmitDrawIndirectEnd(MeshDrawCommand, SceneArgs, InstanceFactor, RHICmdList);
}

void SubmitNaniteMultiViewMaterial(
	const FMeshDrawCommand& MeshDrawCommand,
	const float MaterialDepth,
	const TShaderMapRef<FNaniteMultiViewMaterialVS>& VertexShader,
	const FGraphicsMinimalPipelineStateSet& GraphicsMinimalPipelineStateSet,
	const uint32 InstanceFactor,
	FRHICommandList& RHICmdList,
	FMeshDrawCommandStateCache& StateCache,
	uint32 InstanceBaseOffset)
{
#if WANTS_DRAW_MESH_EVENTS
	FMeshDrawCommand::FMeshDrawEvent MeshEvent(MeshDrawCommand, InstanceFactor, RHICmdList);
#endif

	FMeshDrawCommandSceneArgs SceneArgs;
	bool bAllowSkipDrawCommand = true;
	if (!FMeshDrawCommand::SubmitDrawBegin(MeshDrawCommand, GraphicsMinimalPipelineStateSet, SceneArgs, InstanceFactor, RHICmdList, StateCache, bAllowSkipDrawCommand))
	{
		return;
	}

	// All Nanite mesh draw commands are using the same vertex shader, which has a material depth parameter we assign at render time.
	{
		FNaniteMultiViewMaterialVS::FParameters Parameters;
		Parameters.MaterialDepth = MaterialDepth;
		Parameters.InstanceBaseOffset = InstanceBaseOffset;
		SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), Parameters);
	}

	FMeshDrawCommand::SubmitDrawEnd(MeshDrawCommand, SceneArgs, InstanceFactor, RHICmdList);
}

static const TCHAR* NaniteMeshPassName = TEXT("NaniteMesh");

FNaniteMeshProcessor::FNaniteMeshProcessor(
	const FScene* InScene,
	ERHIFeatureLevel::Type InFeatureLevel,
	const FSceneView* InViewIfDynamicMeshCommand,
	const FMeshPassProcessorRenderState& InDrawRenderState,
	FMeshPassDrawListContext* InDrawListContext
)
	: FMeshPassProcessor(NaniteMeshPassName, InScene, InFeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext)
	, PassDrawRenderState(InDrawRenderState)
{
	check(DoesPlatformSupportNanite(GMaxRHIShaderPlatform));
}

void FNaniteMeshProcessor::AddMeshBatch(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	int32 StaticMeshId /*= -1 */
)
{
	LLM_SCOPE_BYTAG(Nanite);

	// this is now checking before we even attempt to add mesh batch
	checkf(MeshBatch.bUseForMaterial, TEXT("Logic in BuildNaniteMaterialBins() should not have allowed a mesh batch without bUseForMaterial to be added"));

	const FMaterialRenderProxy* FallbackMaterialRenderProxyPtr = MeshBatch.MaterialRenderProxy;
	if (!NaniteLegacyMaterialsSupported())
	{
		FallbackMaterialRenderProxyPtr = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
	}

	while (FallbackMaterialRenderProxyPtr)
	{
		const FMaterial* Material = FallbackMaterialRenderProxyPtr->GetMaterialNoFallback(FeatureLevel);
		if (Material && TryAddMeshBatch(MeshBatch, BatchElementMask, PrimitiveSceneProxy, StaticMeshId, *FallbackMaterialRenderProxyPtr, *Material))
		{
			break;
		}
		FallbackMaterialRenderProxyPtr = FallbackMaterialRenderProxyPtr->GetFallback(FeatureLevel);
	}
}

bool FNaniteMeshProcessor::TryAddMeshBatch(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	int32 StaticMeshId,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const FMaterial& Material
)
{
	const bool bIsTranslucent = IsTranslucentBlendMode(Material);
	const FMaterialShadingModelField ShadingModels = Material.GetShadingModels();

	check(Nanite::IsSupportedBlendMode(Material));
	check(Nanite::IsSupportedMaterialDomain(Material.GetMaterialDomain()));

	const bool bRenderSkylight = Scene && Scene->ShouldRenderSkylightInBasePass(bIsTranslucent) && ShadingModels != MSM_Unlit;
	ELightMapPolicyType LightMapPolicyType = FBasePassMeshProcessor::GetUniformLightMapPolicyType(FeatureLevel, Scene, MeshBatch.LCI, PrimitiveSceneProxy, Material);

	const EGBufferLayout GBufferLayout = Nanite::GetGBufferLayoutForMaterial(Material.MaterialUsesWorldPositionOffset_RenderThread());

	TShaderMapRef<FNaniteIndirectMaterialVS> NaniteVertexShader(GetGlobalShaderMap(FeatureLevel));
	TShaderRef<TBasePassPixelShaderPolicyParamType<FUniformLightMapPolicy>> BasePassPixelShader;

	bool b128BitRequirement = false;
	bool bShadersValid = GetBasePassShaders<FUniformLightMapPolicy>(
		Material,
		MeshBatch.VertexFactory->GetType(),
		FUniformLightMapPolicy(LightMapPolicyType),
		FeatureLevel,
		bRenderSkylight,
		b128BitRequirement,
		GBufferLayout,
		nullptr, // vertex shader
		&BasePassPixelShader
		);
	if (!bShadersValid)
	{
		return false;
	}

	TMeshProcessorShaders
		<
		FNaniteIndirectMaterialVS,
		TBasePassPixelShaderPolicyParamType<FUniformLightMapPolicy>
		>
		PassShaders;

	PassShaders.VertexShader = NaniteVertexShader;
	PassShaders.PixelShader = BasePassPixelShader;

	TBasePassShaderElementData<FUniformLightMapPolicy> ShaderElementData(MeshBatch.LCI);
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, nullptr, MeshBatch, INDEX_NONE, false);

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		nullptr,
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

void FNaniteMeshProcessor::CollectPSOInitializers(
	const FSceneTexturesConfig& SceneTexturesConfig,
	const FMaterial& Material,
	const FPSOPrecacheVertexFactoryData& VertexFactoryData,
	const FPSOPrecacheParams& PreCacheParams,
	TArray<FPSOPrecacheData>& PSOInitializers
)
{
	EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform(FeatureLevel);

	// Make sure Nanite rendering is supported.
	if (!UseNanite(ShaderPlatform))
	{
		return;
	}

	// Only support the Nanite vertex factory type.
	if (VertexFactoryData.VertexFactoryType != &Nanite::FVertexFactory::StaticType &&
		VertexFactoryData.VertexFactoryType != &FNaniteVertexFactory::StaticType)
	{
		return;
	}

	// Check if Nanite can be used by this material
	const FMaterialShadingModelField ShadingModels = Material.GetShadingModels();
	bool bShouldDraw = Nanite::IsSupportedBlendMode(Material) && Nanite::IsSupportedMaterialDomain(Material.GetMaterialDomain());
	if (!bShouldDraw)
	{
		return;
	}

	// Nanite passes always use the forced fixed vertex element and not custom default vertex declaration even if it's provided
	FPSOPrecacheVertexFactoryData NaniteVertexFactoryData = VertexFactoryData;
	NaniteVertexFactoryData.CustomDefaultVertexDeclaration = nullptr;
		
	if (VertexFactoryData.VertexFactoryType == &FNaniteVertexFactory::StaticType)
	{
		Nanite::CollectShadingPSOInitializers(SceneTexturesConfig, NaniteVertexFactoryData, Material, PreCacheParams, FeatureLevel, ShaderPlatform, PSOCollectorIndex, PSOInitializers);
	}
	else
	{
		{
			// generate for both skylight enabled/disabled? Or can this be known already at this point?
			bool bRenderSkyLight = true;
			CollectPSOInitializersForSkyLight(SceneTexturesConfig, NaniteVertexFactoryData, Material, bRenderSkyLight, PSOInitializers);

			bRenderSkyLight = false;
			CollectPSOInitializersForSkyLight(SceneTexturesConfig, NaniteVertexFactoryData, Material, bRenderSkyLight, PSOInitializers);
		}

		Nanite::CollectRasterPSOInitializers(SceneTexturesConfig, Material, PreCacheParams, ShaderPlatform, PSOCollectorIndex, PSOInitializers);
	}
}

void FNaniteMeshProcessor::CollectPSOInitializersForSkyLight(
	const FSceneTexturesConfig& SceneTexturesConfig,
	const FPSOPrecacheVertexFactoryData& VertexFactoryData,
	const FMaterial& RESTRICT Material,
	const bool bRenderSkylight,
	TArray<FPSOPrecacheData>& PSOInitializers
)
{
	TArray<ELightMapPolicyType, TInlineAllocator<2>> UniformLightMapPolicyTypes = FBasePassMeshProcessor::GetUniformLightMapPolicyTypeForPSOCollection(FeatureLevel, Material);
	for (ELightMapPolicyType UniformLightMapPolicyType : UniformLightMapPolicyTypes)
	{	
		TShaderMapRef<FNaniteIndirectMaterialVS> NaniteVertexShader(GetGlobalShaderMap(FeatureLevel));
		TShaderRef<TBasePassPixelShaderPolicyParamType<FUniformLightMapPolicy>> BasePassPixelShader;

		const EGBufferLayout GBufferLayout = Nanite::GetGBufferLayoutForMaterial(Material.MaterialUsesWorldPositionOffset_GameThread());

		bool b128BitRequirement = false;
		bool bShadersValid = GetBasePassShaders<FUniformLightMapPolicy>(
			Material,
			VertexFactoryData.VertexFactoryType,
			FUniformLightMapPolicy(UniformLightMapPolicyType),
			FeatureLevel,
			bRenderSkylight,
			b128BitRequirement,
			GBufferLayout,
			nullptr, // vertex shader
			&BasePassPixelShader
		);

		if (!bShadersValid)
		{
			continue;
		}

		TMeshProcessorShaders
			<
			FNaniteIndirectMaterialVS,
			TBasePassPixelShaderPolicyParamType<FUniformLightMapPolicy>
			>
			PassShaders;
		PassShaders.VertexShader = NaniteVertexShader;
		PassShaders.PixelShader = BasePassPixelShader;

		// Setup the render target info for basepass
		FGraphicsPipelineRenderTargetsInfo RenderTargetsInfo;
		RenderTargetsInfo.NumSamples = 1;
		SetupGBufferRenderTargetInfo(SceneTexturesConfig, RenderTargetsInfo, true /*bSetupDepthStencil*/);

		AddGraphicsPipelineStateInitializer(
			VertexFactoryData,
			Material,
			PassDrawRenderState,
			RenderTargetsInfo,
			PassShaders,
			FM_Solid,
			CM_None,
			PT_TriangleList,
			EMeshPassFeatures::Default,
			true /*bRequired*/,
			PSOInitializers);
	}
}

FMeshPassProcessor* CreateNaniteMeshProcessor(
	ERHIFeatureLevel::Type FeatureLevel,
	const FScene* Scene,
	const FSceneView* InViewIfDynamicMeshCommand,
	FMeshPassDrawListContext* InDrawListContext
)
{
	FMeshPassProcessorRenderState PassDrawRenderState;
	SetupBasePassState(FExclusiveDepthStencil::DepthWrite_StencilNop, false, PassDrawRenderState);
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Equal>::GetRHI());
	PassDrawRenderState.SetDepthStencilAccess(FExclusiveDepthStencil::DepthWrite_StencilNop);

	return new FNaniteMeshProcessor(Scene, FeatureLevel, InViewIfDynamicMeshCommand, PassDrawRenderState, InDrawListContext);
}

IPSOCollector* CreateNaniteMeshProcessorForPSOCollection(ERHIFeatureLevel::Type FeatureLevel)
{
	if (DoesPlatformSupportNanite(GetFeatureLevelShaderPlatform(FeatureLevel)))
	{
		return CreateNaniteMeshProcessor(FeatureLevel, nullptr, nullptr, nullptr);
	}
	else
	{
		return nullptr;
	}
}

// Only register for PSO Collection
FRegisterPSOCollectorCreateFunction RegisterPSOCollectorNaniteMeshPass(&CreateNaniteMeshProcessorForPSOCollection, EShadingPath::Deferred, NaniteMeshPassName);

class FSubmitNaniteMaterialPassCommandsAnyThreadTask : public FRenderTask
{
	FRHICommandList& RHICmdList;
	FRHIBuffer* MaterialIndirectArgs = nullptr;
	TArrayView<FNaniteMaterialPassCommand const> NaniteMaterialPassCommands;
	TShaderMapRef<FNaniteIndirectMaterialVS> NaniteVertexShader;
	FIntRect ViewRect;
	uint32 TileCount;
	int32 TaskIndex;
	int32 TaskNum;

public:

	FSubmitNaniteMaterialPassCommandsAnyThreadTask(
		FRHICommandList& InRHICmdList,
		FRHIBuffer* InMaterialIndirectArgs,
		TArrayView<FNaniteMaterialPassCommand const> InNaniteMaterialPassCommands,
		TShaderMapRef<FNaniteIndirectMaterialVS> InNaniteVertexShader,
		FIntRect InViewRect,
		uint32 InTileCount,
		int32 InTaskIndex,
		int32 InTaskNum
	)
		: RHICmdList(InRHICmdList)
		, MaterialIndirectArgs(InMaterialIndirectArgs)
		, NaniteMaterialPassCommands(InNaniteMaterialPassCommands)
		, NaniteVertexShader(InNaniteVertexShader)
		, ViewRect(InViewRect)
		, TileCount(InTileCount)
		, TaskIndex(InTaskIndex)
		, TaskNum(InTaskNum)
	{}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FSubmitNaniteMaterialPassCommandsAnyThreadTask, STATGROUP_TaskGraphTasks);
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		FOptionalTaskTagScope Scope(ETaskTag::EParallelRenderingThread);
		TRACE_CPUPROFILER_EVENT_SCOPE(SubmitNaniteMaterialPassCommandsAnyThreadTask);
		checkSlow(RHICmdList.IsInsideRenderPass());

		// check for the multithreaded shader creation has been moved to FShaderCodeArchive::CreateShader() 

		// Recompute draw range.
		const int32 DrawNum = NaniteMaterialPassCommands.Num();
		const int32 NumDrawsPerTask = TaskIndex < DrawNum ? FMath::DivideAndRoundUp(DrawNum, TaskNum) : 0;
		const int32 StartIndex = TaskIndex * NumDrawsPerTask;
		const int32 NumDraws = FMath::Min(NumDrawsPerTask, DrawNum - StartIndex);

		RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, ViewRect.Max.X, ViewRect.Max.Y, 1.0f);

		FMeshDrawCommandStateCache StateCache;
		FGraphicsMinimalPipelineStateSet GraphicsMinimalPipelineStateSet;
		for (int32 IterIndex = 0; IterIndex < NumDraws; ++IterIndex)
		{
			const FNaniteMaterialPassCommand& MaterialPassCommand = NaniteMaterialPassCommands[StartIndex + IterIndex];
			SubmitNaniteIndirectMaterial(MaterialPassCommand, NaniteVertexShader, GraphicsMinimalPipelineStateSet, TileCount, RHICmdList, MaterialIndirectArgs, StateCache);
		}

		RHICmdList.EndRenderPass();
		RHICmdList.FinishRecording();
	}
};

void BuildNaniteMaterialPassCommands(
	const TConstArrayView<FGraphicsPipelineRenderTargetsInfo> RenderTargetsInfo,
	const FNaniteMaterialCommands& MaterialCommands,
	const FNaniteVisibilityResults* VisibilityResults,
	TArray<FNaniteMaterialPassCommand, SceneRenderingAllocator>& OutNaniteMaterialPassCommands,
	TArrayView<FNaniteMaterialPassInfo> OutMaterialPassInfo)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(BuildNaniteMaterialPassCommands);

	const uint32 NumPasses = RenderTargetsInfo.Num();
	check(NumPasses > 0);
	check(NumPasses <= (uint32)ENaniteMaterialPass::Max);
	check(NumPasses == OutMaterialPassInfo.Num());

	// Initialize the pass info
	for (FNaniteMaterialPassInfo& PassInfo : OutMaterialPassInfo)
	{
		PassInfo.CommandOffset = 0;
		PassInfo.NumCommands = 0;
	}
	
	const bool bVelocityPassEnabled = NumPasses > (uint32)ENaniteMaterialPass::EmitGBufferWithVelocity;
	auto GetMaterialPass = [bVelocityPassEnabled](const FNaniteMaterialEntry& MaterialEntry)
	{
		// If the material has WPO enabled, we have to render this material in the velocity pass so velocity will
		// be properly written
		return (bVelocityPassEnabled && MaterialEntry.bWPOEnabled) ?
			ENaniteMaterialPass::EmitGBufferWithVelocity : ENaniteMaterialPass::EmitGBuffer;
	};
	auto InsertPassCommand = [&OutNaniteMaterialPassCommands, OutMaterialPassInfo, NumPasses] (FNaniteMaterialPassCommand& PassCommand, uint32 PassIndex)
	{
		// The sort key will force it to the right position for the pass, so just add it anywhere in the list
		OutNaniteMaterialPassCommands.Emplace(PassCommand);
		FNaniteMaterialPassInfo& PassInfo = OutMaterialPassInfo[PassIndex];
		++PassInfo.NumCommands;

		// push out the offset of subsequent passes
		for (uint32 OtherPassIndex = PassIndex + 1; OtherPassIndex < NumPasses; ++OtherPassIndex)
		{
			++OutMaterialPassInfo[OtherPassIndex].CommandOffset;
		}
	};

	const FNaniteMaterialEntryMap& BucketMap = MaterialCommands.GetCommands();
	checkf(OutNaniteMaterialPassCommands.Max() >= BucketMap.Num(), TEXT("Nanite mesh commands must be resized on the render thread prior to calling this method."));

	// Pull into local here so another thread can't change the sort values mid-iteration.
	const int32 MaterialSortMode = GNaniteMaterialSortMode;
	for (auto Iter = BucketMap.begin(); Iter != BucketMap.end(); ++Iter)
	{
		auto& Command = *Iter;
		const FMeshDrawCommand& MeshDrawCommand = Command.Key;

		if (VisibilityResults && !VisibilityResults->IsShadingDrawVisible(FNaniteMaterialEntryMap::ComputeHash(MeshDrawCommand).AsUInt()))
		{
			continue;
		}

		const uint32 PassIndex = (uint32)GetMaterialPass(Command.Value);
		FNaniteMaterialPassCommand PassCommand(MeshDrawCommand);
		const int32 MaterialId = Iter.GetElementId().GetIndex();

		PassCommand.MaterialId = FNaniteCommandInfo::GetMaterialId(MaterialId);
		PassCommand.MaterialDepth = FNaniteCommandInfo::GetDepthId(MaterialId);
		PassCommand.MaterialSlot  = Command.Value.MaterialSlot;

		if (MaterialSortMode == 2)
		{
			PassCommand.SortKey = MeshDrawCommand.GetPipelineStateSortingKey(RenderTargetsInfo[PassIndex]);
		}
		else if (MaterialSortMode == 3)
		{
			// Use reference count as the sort key
			PassCommand.SortKey = uint64(Command.Value.ReferenceCount);
		}
		else if(MaterialSortMode == 4)
		{
			// TODO: Remove other sort modes and just use 4 (needs more optimization/profiling)?
			// Sort by pipeline state, but use hash of MaterialId for randomized tie-breaking.
			// This spreads out the empty draws inside the pipeline buckets and improves overall utilization.
			const uint64 PipelineSortKey = MeshDrawCommand.GetPipelineStateSortingKey(RenderTargetsInfo[PassIndex]);
			const uint32 PipelineSortKeyHash = GetTypeHash(PipelineSortKey);
			const uint32 MaterialHash = MurmurFinalize32(MaterialId);
			PassCommand.SortKey = ((uint64)PipelineSortKeyHash << 32) | MaterialHash;
		}

		if (bVelocityPassEnabled)
		{
			// Use the pass index as the highest order bits to keep them in pass order
			static const uint32 PassIndexBits = 1u;
			static_assert((1u << PassIndexBits) <= (uint32)ENaniteMaterialPass::Max);

			static const uint64 PassIndexShift = 64ull - uint64(PassIndexBits);
			static const uint64 SortKeyMask = ((1ull << PassIndexShift) - 1ull);
			PassCommand.SortKey = (PassCommand.SortKey & SortKeyMask) | (uint64(PassIndex) << PassIndexShift);
		}

		InsertPassCommand(PassCommand, PassIndex);
	}

	if (MaterialSortMode != 0)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Sort);
		OutNaniteMaterialPassCommands.Sort();
	}
}

void DrawNaniteMaterialPass(
	FRDGParallelCommandListSet* ParallelCommandListSet,
	FRHICommandList& RHICmdList,
	const FIntRect ViewRect,
	const uint32 TileCount,
	TShaderMapRef<FNaniteIndirectMaterialVS> VertexShader,
	FRDGBuffer* MaterialIndirectArgs,
	TArrayView<FNaniteMaterialPassCommand const> MaterialPassCommands)
{
	if (MaterialPassCommands.IsEmpty())
	{
		return;
	}
	check(!MaterialPassCommands.IsEmpty());

	MaterialIndirectArgs->MarkResourceAsUsed();

	if (ParallelCommandListSet)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ParallelSubmitNaniteMaterialPassCommands);

		// Distribute work evenly to the available task graph workers based on NumPassCommands.
		const int32 NumPassCommands = MaterialPassCommands.Num();
		const int32 NumThreads = FMath::Min<int32>(FTaskGraphInterface::Get().GetNumWorkerThreads(), ParallelCommandListSet->Width);
		const int32 NumTasks = FMath::Min<int32>(NumThreads, FMath::DivideAndRoundUp(NumPassCommands, ParallelCommandListSet->MinDrawsPerCommandList));
		const int32 NumDrawsPerTask = FMath::DivideAndRoundUp(NumPassCommands, NumTasks);

		const ENamedThreads::Type RenderThread = ENamedThreads::GetRenderThread();

		// Assume on demand shader creation is enabled for platforms supporting Nanite
		// otherwise there might be issues with PSO creation on a task which is not running on the RenderThread
		// So task prerequisites can be empty (MeshDrawCommands task has prereq on FMeshDrawCommandInitResourcesTask which calls LazilyInitShaders on all shader)
		ensure(FParallelMeshDrawCommandPass::IsOnDemandShaderCreationEnabled());
		FGraphEventArray EmptyPrereqs;

		for (int32 TaskIndex = 0; TaskIndex < NumTasks; TaskIndex++)
		{
			const int32 StartIndex = TaskIndex * NumDrawsPerTask;
			const int32 NumDraws = FMath::Min(NumDrawsPerTask, NumPassCommands - StartIndex);
			checkSlow(NumDraws > 0);

			FRHICommandList* CmdList = ParallelCommandListSet->NewParallelCommandList();

			FGraphEventRef AnyThreadCompletionEvent = TGraphTask<FSubmitNaniteMaterialPassCommandsAnyThreadTask>::CreateTask(&EmptyPrereqs, RenderThread).
				ConstructAndDispatchWhenReady(*CmdList, MaterialIndirectArgs->GetRHI(), MaterialPassCommands, VertexShader, ViewRect, TileCount, TaskIndex, NumTasks);

			ParallelCommandListSet->AddParallelCommandList(CmdList, AnyThreadCompletionEvent, NumDraws);
		}
	}
	else
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SubmitNaniteMaterialPassCommands);

		RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, ViewRect.Max.X, ViewRect.Max.Y, 1.0f);

		FGraphicsMinimalPipelineStateSet GraphicsMinimalPipelineStateSet;
		FMeshDrawCommandStateCache StateCache;
		for (const FNaniteMaterialPassCommand& Command : MaterialPassCommands)
		{
			SubmitNaniteIndirectMaterial(
				Command,
				VertexShader,
				GraphicsMinimalPipelineStateSet,
				TileCount,
				RHICmdList,
				MaterialIndirectArgs->GetRHI(),
				StateCache
			);
		}
	}
}
