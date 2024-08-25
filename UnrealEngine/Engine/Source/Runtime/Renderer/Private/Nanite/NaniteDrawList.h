// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NaniteShared.h"
#include "NaniteMaterials.h"
#include "PrimitiveSceneInfo.h"

struct MeshDrawCommandKeyFuncs;
class FParallelCommandListBindings;
class FRDGParallelCommandListSet;

class FNaniteDrawListContext : public FMeshPassDrawListContext
{
public:
	struct FDeferredCommand
	{
		FPrimitiveSceneInfo* PrimitiveSceneInfo;
		FMeshDrawCommand MeshDrawCommand;
		FNaniteMaterialCommands::FCommandHash CommandHash;
#if WITH_DEBUG_VIEW_MODES
		uint32 InstructionCount;
#endif
		uint8 SectionIndex;
		bool bWPOEnabled;
	};

	struct FDeferredPipelines
	{
		FPrimitiveSceneInfo* PrimitiveSceneInfo;
		TArray<FNaniteRasterPipeline, TInlineAllocator<4>> RasterPipelines;
		TArray<FNaniteShadingPipeline, TInlineAllocator<4>> ShadingPipelines;
	};

public:
	struct FPrimitiveSceneInfoScope
	{
		FPrimitiveSceneInfoScope(const FPrimitiveSceneInfoScope&) = delete;
		FPrimitiveSceneInfoScope& operator=(const FPrimitiveSceneInfoScope&) = delete;

		inline FPrimitiveSceneInfoScope(FNaniteDrawListContext& InContext, FPrimitiveSceneInfo& PrimitiveSceneInfo)
			: Context(InContext)
		{
			Context.BeginPrimitiveSceneInfo(PrimitiveSceneInfo);
		}

		inline ~FPrimitiveSceneInfoScope()
		{
			Context.EndPrimitiveSceneInfo();
		}

	private:
		FNaniteDrawListContext& Context;
	};

	struct FMeshPassScope
	{
		FMeshPassScope(const FMeshPassScope&) = delete;
		FMeshPassScope& operator=(const FMeshPassScope&) = delete;

		inline FMeshPassScope(FNaniteDrawListContext& InContext, ENaniteMeshPass::Type MeshPass)
			: Context(InContext)
		{
			Context.BeginMeshPass(MeshPass);
		}

		inline ~FMeshPassScope()
		{
			Context.EndMeshPass();
		}
		
	private:
		FNaniteDrawListContext& Context;
	};

	virtual FMeshDrawCommand& AddCommand(FMeshDrawCommand& Initializer, uint32 NumElements) override final;

	virtual void FinalizeCommand(
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
	) override final;

	void BeginPrimitiveSceneInfo(FPrimitiveSceneInfo& PrimitiveSceneInfo);
	void EndPrimitiveSceneInfo();

	void BeginMeshPass(ENaniteMeshPass::Type MeshPass);
	void EndMeshPass();

	void Apply(FScene& Scene);

private:
	FNaniteMaterialSlot& GetMaterialSlotForWrite(FPrimitiveSceneInfo& PrimitiveSceneInfo, ENaniteMeshPass::Type MeshPass, uint8 SectionIndex);
	void AddShadingCommand(FPrimitiveSceneInfo& PrimitiveSceneInfo, const FNaniteCommandInfo& CommandInfo, ENaniteMeshPass::Type MeshPass, uint8 SectionIndex);
	void AddRasterBin(FPrimitiveSceneInfo& PrimitiveSceneInfo, const FNaniteRasterBin& PrimaryRasterBin, const FNaniteRasterBin& SecondaryRasterBin, ENaniteMeshPass::Type MeshPass, uint8 SectionIndex);
	void AddShadingBin(FPrimitiveSceneInfo& PrimitiveSceneInfo, const FNaniteShadingBin& ShadingBin, ENaniteMeshPass::Type MeshPass, uint8 SectionIndex);

private:
	FMeshDrawCommand MeshDrawCommandForStateBucketing;
	FPrimitiveSceneInfo* CurrentPrimitiveSceneInfo = nullptr;
	ENaniteMeshPass::Type CurrentMeshPass = ENaniteMeshPass::Num;

public:
	TArray<FDeferredCommand> DeferredCommands[ENaniteMeshPass::Num];
	TArray<FDeferredPipelines> DeferredPipelines[ENaniteMeshPass::Num];

	FMaterialRelevance CombinedRelevance;
};

class FNaniteMeshProcessor : public FSceneRenderingAllocatorObject<FNaniteMeshProcessor>, public FMeshPassProcessor
{
public:
	FNaniteMeshProcessor(
		const FScene* InScene,
		ERHIFeatureLevel::Type InFeatureLevel,
		const FSceneView* InViewIfDynamicMeshCommand,
		const FMeshPassProcessorRenderState& InDrawRenderState,
		FMeshPassDrawListContext* InDrawListContext
	);

	virtual void AddMeshBatch(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		int32 StaticMeshId = -1
	) override final;

	virtual void CollectPSOInitializers(
		const FSceneTexturesConfig& SceneTexturesConfig, 
		const FMaterial& Material, 
		const FPSOPrecacheVertexFactoryData& VertexFactoryData,
		const FPSOPrecacheParams& PreCacheParams, 
		TArray<FPSOPrecacheData>& PSOInitializers
	) override final;

private:
	bool TryAddMeshBatch(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		int32 StaticMeshId,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material
	);

	void CollectPSOInitializersForSkyLight(
		const FSceneTexturesConfig& SceneTexturesConfig,
		const FPSOPrecacheVertexFactoryData& VertexFactoryData,
		const FMaterial& RESTRICT Material,
		const bool bRenderSkylight,
		TArray<FPSOPrecacheData>& PSOInitializers
	);

private:
	FMeshPassProcessorRenderState PassDrawRenderState;
};

FMeshPassProcessor* CreateNaniteMeshProcessor(
	ERHIFeatureLevel::Type FeatureLevel,
	const FScene* Scene,
	const FSceneView* InViewIfDynamicMeshCommand,
	FMeshPassDrawListContext* InDrawListContext
);

enum class ENaniteMaterialPass : uint32
{
	// Standard per-material draws that fill the GBuffer
	EmitGBuffer,
	// When !IsUsingBasePassVelocity, fills the GBuffer and velocity for materials with programmable deformation
	EmitGBufferWithVelocity,

	Max
};

struct FNaniteMaterialPassInfo
{
	uint32 CommandOffset = 0;
	uint32 NumCommands = 0;
};

void BuildNaniteMaterialPassCommands(
	const TConstArrayView<FGraphicsPipelineRenderTargetsInfo> RenderTargetsInfo,
	const FNaniteMaterialCommands& MaterialCommands,
	const FNaniteVisibilityResults* VisibilityResults,
	TArray<FNaniteMaterialPassCommand, SceneRenderingAllocator>& OutNaniteMaterialPassCommands,
	TArrayView<FNaniteMaterialPassInfo> OutMaterialPassInfo);

void DrawNaniteMaterialPass(
	FRDGParallelCommandListSet* ParallelCommandListSet,
	FRHICommandList& RHICmdList,
	const FIntRect ViewRect,
	const uint32 TileCount,
	TShaderMapRef<FNaniteIndirectMaterialVS> VertexShader,
	FRDGBuffer* MaterialIndirectArgs,
	TArrayView<FNaniteMaterialPassCommand const> MaterialPassCommands
);

void SubmitNaniteIndirectMaterial(
	const FNaniteMaterialPassCommand& MaterialPassCommand,
	const TShaderMapRef<FNaniteIndirectMaterialVS>& VertexShader,
	const FGraphicsMinimalPipelineStateSet& GraphicsMinimalPipelineStateSet,
	const uint32 InstanceFactor,
	FRHICommandList& RHICmdList,
	FRHIBuffer* MaterialIndirectArgs,
	FMeshDrawCommandStateCache& StateCache
);

void SubmitNaniteMultiViewMaterial(
	const FMeshDrawCommand& MeshDrawCommand,
	const float MaterialDepth,
	const TShaderMapRef<FNaniteMultiViewMaterialVS>& VertexShader,
	const FGraphicsMinimalPipelineStateSet& GraphicsMinimalPipelineStateSet,
	const uint32 InstanceFactor,
	FRHICommandList& RHICmdList,
	FMeshDrawCommandStateCache& StateCache,
	uint32 InstanceBaseOffset
);
