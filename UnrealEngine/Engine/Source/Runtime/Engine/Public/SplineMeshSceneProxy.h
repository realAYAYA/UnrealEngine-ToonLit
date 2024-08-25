// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderingThread.h"
#include "ShaderParameters.h"
#include "VertexFactory.h"
#include "LocalVertexFactory.h"
#include "PrimitiveViewRelevance.h"
#include "Components/SplineMeshComponent.h"
#include "StaticMeshResources.h"
#include "StaticMeshSceneProxy.h"
#include "SplineMeshShaderParams.h"
#include "NaniteSceneProxy.h"
#include "InstanceDataSceneProxy.h"

//////////////////////////////////////////////////////////////////////////
// SplineMeshVertexFactory

/** A vertex factory for spline-deformed static meshes */
struct FSplineMeshVertexFactory : public FLocalVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FSplineMeshVertexFactory);
public:
	FSplineMeshVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
		: FLocalVertexFactory(InFeatureLevel, "FSplineMeshVertexFactory")
	{
	}

	/** Should we cache the material's shadertype on this platform with this vertex factory? */
	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);

	/** Modify compile environment to enable spline deformation */
	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	/** Get vertex elements used when during PSO precaching materials using this vertex factory type */
	static void GetPSOPrecacheVertexFetchElements(EVertexInputStreamType VertexInputStreamType, FVertexDeclarationElementList& Elements);

	/** Copy the data from another vertex factory */
	void Copy(const FSplineMeshVertexFactory& Other)
	{
		FSplineMeshVertexFactory* VertexFactory = this;
		const FDataType* DataCopy = &Other.Data;
		ENQUEUE_RENDER_COMMAND(FSplineMeshVertexFactoryCopyData)(
			[VertexFactory, DataCopy](FRHICommandListImmediate& RHICmdList)
			{
				VertexFactory->Data = *DataCopy;
			});
		BeginUpdateResourceRHI(this);
	}
};

//////////////////////////////////////////////////////////////////////////
// FSplineMeshVertexFactoryShaderParameters

/** Factory specific params */
class FSplineMeshVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FSplineMeshVertexFactoryShaderParameters, NonVirtual);
public:
	void Bind(const FShaderParameterMap& ParameterMap);

	void GetElementShaderBindings(
		const class FSceneInterface* Scene,
		const FSceneView* View,
		const class FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const FVertexFactory* VertexFactory,
		const FMeshBatchElement& BatchElement,
		class FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams
		) const;

private:
	LAYOUT_FIELD(FShaderParameter, SplineMeshParams);
};

struct FSplineMeshSceneInstanceDataBuffers : public FSingleInstanceDataBuffers
{
	ENGINE_API void Setup(const FSplineMeshShaderParams& InSplineMeshShaderParams);
	ENGINE_API bool Update(const FSplineMeshShaderParams& InSplineMeshShaderParams);
};

//////////////////////////////////////////////////////////////////////////
// SplineMeshSceneProxy

/**
 * This interface ties common functionality to the two different spline mesh scene proxies without duplicating code or
 * introducing diamond inheritance.
 */
template<typename TDerived>
class TSplineMeshSceneProxyCommon
{
public:
	TSplineMeshSceneProxyCommon()
	{
		FMemory::Memzero(&SplineParams, sizeof(SplineParams));
		SplineParams.TextureCoord = FUintVector2(INDEX_NONE, INDEX_NONE);
	}
	virtual ~TSplineMeshSceneProxyCommon() {}

	const FSplineMeshShaderParams& GetSplineMeshParams() const { return SplineParams; }

	void SetSplineTextureCoord_RenderThread(uint32 SplineIndex, FUintVector2 TexCoord)
	{
		check(SplineIndex == 0); // currently only support one spline type, but could do instanced later
		SplineParams.TextureCoord = TexCoord;
		RepackSplineMeshParams();
	}
	
	void UpdateSplineMeshParams_RenderThread(const FSplineMeshShaderParams& Params)
	{
		// Don't replace the texture coord that was assigned to us in SetSplineTextureCoord
		FUintVector2 TexCoord = SplineParams.TextureCoord;
		SplineParams = Params;
		SplineParams.TextureCoord = TexCoord;
		RepackSplineMeshParams();
	}

protected:
	void RepackSplineMeshParams(bool bUpdateGPUScene = true)
	{
		auto& SceneProxy = Downcast();
		
		// NOTE: If the payload extension was not initialized, we're probably not using GPU Scene
		if (SplineMeshInstanceData.Update(SplineParams))
		{
			if (bUpdateGPUScene)
			{
				// Request a GPU Scene update for this primitive so it updates its instance data
				SceneProxy.GetScene().RequestGPUSceneUpdate(
					*SceneProxy.GetPrimitiveSceneInfo(),
					EPrimitiveDirtyState::ChangedOther
				);
			}
		}
	}

protected:
	/** Parameters that define the spline, used to deform mesh */
	FSplineMeshShaderParams SplineParams;

	FSplineMeshSceneInstanceDataBuffers SplineMeshInstanceData;
private:
	/** implemented by derived to provide access by parent */
	virtual TDerived& Downcast() = 0;
};

#define IMPL_SPLINE_MESH_SCENE_PROXY_COMMON(classname) \
	friend class TSplineMeshSceneProxyCommon<classname>; \
	private: \
		virtual classname& Downcast() override { return *this; }

/** Scene proxy for SplineMesh instance */
class FSplineMeshSceneProxy final : public FStaticMeshSceneProxy, public TSplineMeshSceneProxyCommon<FSplineMeshSceneProxy>
{
	IMPL_SPLINE_MESH_SCENE_PROXY_COMMON(FSplineMeshSceneProxy)

public:
	FSplineMeshSceneProxy(USplineMeshComponent* InComponent);

	// FPrimitiveSceneProxy interface
	virtual SIZE_T GetTypeHash() const override;
	virtual bool GetShadowMeshElement(int32 LODIndex, int32 BatchIndex, uint8 InDepthPriorityGroup, FMeshBatch& OutMeshBatch, bool bDitheredLODTransition) const override;
	virtual bool GetMeshElement(int32 LODIndex, int32 BatchIndex, int32 ElementIndex, uint8 InDepthPriorityGroup, bool bUseSelectionOutline, bool bAllowPreCulledIndices, FMeshBatch& OutMeshBatch) const override;
	virtual bool GetWireframeMeshElement(int32 LODIndex, int32 BatchIndex, const FMaterialRenderProxy* WireframeRenderProxy, uint8 InDepthPriorityGroup, bool bAllowPreCulledIndices, FMeshBatch& OutMeshBatch) const override;
	virtual bool GetCollisionMeshElement(int32 LODIndex, int32 BatchIndex, int32 ElementIndex, uint8 InDepthPriorityGroup, const FMaterialRenderProxy* RenderProxy, FMeshBatch& OutMeshBatch) const override;
#if RHI_RAYTRACING
	virtual bool HasRayTracingRepresentation() const override { return true; }
	virtual bool IsRayTracingRelevant() const override { return true; }
	virtual void GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances) override;	
#endif // RHI_RAYTRACING
	virtual void OnTransformChanged(FRHICommandListBase& RHICmdList) override;

private:
	struct FLODResources
	{
		/** Pointer to vertex factory object */
		FSplineMeshVertexFactory* VertexFactory;

		FLODResources(FSplineMeshVertexFactory* InVertexFactory) :
			VertexFactory(InVertexFactory)
		{
		}
	};

	void SetupMeshBatchForSpline(int32 InLODIndex, FMeshBatch& OutMeshBatch) const;

private:
	TArray<FLODResources> LODResources;
};

/** Scene proxy for SplineMesh instance for Nanite */
class FNaniteSplineMeshSceneProxy final : public Nanite::FSceneProxy, public TSplineMeshSceneProxyCommon<FNaniteSplineMeshSceneProxy>
{
	IMPL_SPLINE_MESH_SCENE_PROXY_COMMON(FNaniteSplineMeshSceneProxy)

public:
	FNaniteSplineMeshSceneProxy(const Nanite::FMaterialAudit& MaterialAudit, USplineMeshComponent* InComponent);

	// FPrimitiveSceneProxy interface
	virtual SIZE_T GetTypeHash() const override;
	virtual void OnTransformChanged(FRHICommandListBase& RHICmdList) override;
#if RHI_RAYTRACING
	virtual bool IsRayTracingStaticRelevant() const override { return false; }
	virtual ERayTracingPrimitiveFlags GetCachedRayTracingInstance(FRayTracingInstance& OutRayTracingInstance) override;
	virtual void GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances) override;
	virtual void SetupFallbackRayTracingMaterials(int32 LODIndex, TArray<FMeshBatch>& OutMaterials) const override;
#endif
};

/** Helper to update the parameters of the specified spline mesh scene proxy */
ENGINE_API void UpdateSplineMeshParams_RenderThread(FPrimitiveSceneProxy* SceneProxy, const FSplineMeshShaderParams& Params);



