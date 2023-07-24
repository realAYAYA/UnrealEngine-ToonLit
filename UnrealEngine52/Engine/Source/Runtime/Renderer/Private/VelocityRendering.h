// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "HitProxies.h"
#include "RendererInterface.h"
#include "DepthRendering.h"

class FPrimitiveSceneInfo;
class FPrimitiveSceneProxy;
class FScene;
class FStaticMeshBatch;
class FViewInfo;

struct FPrimitiveViewRelevance;

enum class EVelocityPass : uint32
{
	// Renders a separate velocity pass for opaques.
	Opaque = 0,

	// Renders a separate velocity / depth pass for translucency AFTER the translucent pass.
	Translucent,

	Count
};

EMeshPass::Type GetMeshPassFromVelocityPass(EVelocityPass VelocityPass);

// Group Velocity Rendering accessors, types, etc.
struct FVelocityRendering
{
	/** Returns the texture format for the velocity buffer. */
	static EPixelFormat GetFormat(EShaderPlatform ShaderPlatform);

	/** Returns the texture create flags for the velocity buffer. */
	static ETextureCreateFlags GetCreateFlags(EShaderPlatform ShaderPlatform);
	
	/** Returns the render target description for the velocity buffer. */
	static FRDGTextureDesc GetRenderTargetDesc(EShaderPlatform ShaderPlatform, FIntPoint Extent);

	/** Returns true if a velocity pass is supported. */
	static bool IsVelocityPassSupported(EShaderPlatform ShaderPlatform);

	/** Returns true if the velocity can be output during depth pass. */
	static bool DepthPassCanOutputVelocity(ERHIFeatureLevel::Type FeatureLevel);

	/** Returns true if the velocity can be output in the BasePass. */
	static bool BasePassCanOutputVelocity(EShaderPlatform ShaderPlatform);

	/** Returns true if the velocity pass is using parallel dispatch. */
	static bool IsParallelVelocity(EShaderPlatform ShaderPlatform);

	/** Returns true if we wait for outstanding tasks in velocity pass. */
	static bool IsVelocityWaitForTasksEnabled(EShaderPlatform ShaderPlatform);
};

/**
 * Base velocity mesh pass processor class. Used for both opaque and translucent velocity passes.
 */
class FVelocityMeshProcessor : public FMeshPassProcessor
{
public:
	FVelocityMeshProcessor(
		EMeshPass::Type MeshPassType,
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FSceneView* InViewIfDynamicMeshCommand,
		const FMeshPassProcessorRenderState& InPassDrawRenderState,
		FMeshPassDrawListContext* InDrawListContext);

	FMeshPassProcessorRenderState PassDrawRenderState;

	/* Checks whether the primitive should emit velocity for the current view by comparing screen space size against a threshold. */
	static bool PrimitiveHasVelocityForView(const FViewInfo& View, const FPrimitiveSceneProxy* PrimitiveSceneProxy);

protected:
	bool Process(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		int32 StaticMeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode);

	bool CollectPSOInitializersInternal(
		const FSceneTexturesConfig& SceneTexturesConfig,
		const FPSOPrecacheVertexFactoryData& VertexFactoryData,
		const FMaterial& RESTRICT MaterialResource,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode,
		TArray<FPSOPrecacheData>& PSOInitializers);
};

/**
 * Velocity pass processor for rendering opaques into a separate velocity pass (i.e. separate from the base pass).
 */
class FOpaqueVelocityMeshProcessor : public FSceneRenderingAllocatorObject<FOpaqueVelocityMeshProcessor>, public FVelocityMeshProcessor
{
public:
	FOpaqueVelocityMeshProcessor(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FSceneView* InViewIfDynamicMeshCommand,
		const FMeshPassProcessorRenderState& InPassDrawRenderState,
		FMeshPassDrawListContext* InDrawListContext);

	/** Returns true if the object is capable of having velocity for any frame. */
	static bool PrimitiveCanHaveVelocity(EShaderPlatform ShaderPlatform, const FPrimitiveSceneProxy* PrimitiveSceneProxy);
	static bool PrimitiveCanHaveVelocity(EShaderPlatform ShaderPlatform, bool bDrawVelocity, bool bHasStaticLighting);

	/** Returns true if the primitive has velocity for the current frame. */
	static bool PrimitiveHasVelocityForFrame(const FPrimitiveSceneProxy* PrimitiveSceneProxy);

private:
	bool TryAddMeshBatch(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		int32 StaticMeshId,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FMaterial* Material);

	virtual void AddMeshBatch(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		int32 StaticMeshId = -1) override final;

	virtual void CollectPSOInitializers(
		const FSceneTexturesConfig& SceneTexturesConfig,
		const FMaterial& Material,
		const FPSOPrecacheVertexFactoryData& VertexFactoryData,
		const FPSOPrecacheParams& PreCacheParams,
		TArray<FPSOPrecacheData>& PSOInitializers) override final;
};

/**
 * Velocity pass processor for rendering translucent object velocity and depth. This pass is rendered AFTER the
 * translucent pass so that depth can safely be written.
 */
class FTranslucentVelocityMeshProcessor : public FSceneRenderingAllocatorObject<FTranslucentVelocityMeshProcessor>, public FVelocityMeshProcessor
{
public:
	FTranslucentVelocityMeshProcessor(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FSceneView* InViewIfDynamicMeshCommand,
		const FMeshPassProcessorRenderState& InPassDrawRenderState,
		FMeshPassDrawListContext* InDrawListContext);

	/** Returns true if the object is capable of having velocity for any frame. */
	static bool PrimitiveCanHaveVelocity(EShaderPlatform ShaderPlatform, const FPrimitiveSceneProxy* PrimitiveSceneProxy);

	/** Returns true if the primitive has velocity for the current frame. */
	static bool PrimitiveHasVelocityForFrame(const FPrimitiveSceneProxy* PrimitiveSceneProxy);

private:
	bool TryAddMeshBatch(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		int32 StaticMeshId,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FMaterial* Material);

	virtual void AddMeshBatch(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		int32 StaticMeshId = -1) override final;

	virtual void CollectPSOInitializers(
		const FSceneTexturesConfig& SceneTexturesConfig,
		const FMaterial& Material, 
		const FPSOPrecacheVertexFactoryData& VertexFactoryData,
		const FPSOPrecacheParams& PreCacheParams,
		TArray<FPSOPrecacheData>& PSOInitializers) override final;
};