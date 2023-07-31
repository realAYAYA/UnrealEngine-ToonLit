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

//////////////////////////////////////////////////////////////////////////
// SplineMeshSceneProxy

/** Scene proxy for SplineMesh instance */
class FSplineMeshSceneProxy final : public FStaticMeshSceneProxy
{
protected:
	struct FLODResources
	{
		/** Pointer to vertex factory object */
		FSplineMeshVertexFactory* VertexFactory;

		FLODResources(FSplineMeshVertexFactory* InVertexFactory) :
			VertexFactory(InVertexFactory)
		{
		}
	};
public:
	SIZE_T GetTypeHash() const override;

	FSplineMeshSceneProxy(USplineMeshComponent* InComponent);

	void InitVertexFactory(USplineMeshComponent* InComponent, int32 InLODIndex, FColorVertexBuffer*);

	/** Sets up a shadow FMeshBatch for a specific LOD. */
	virtual bool GetShadowMeshElement(int32 LODIndex, int32 BatchIndex, uint8 InDepthPriorityGroup, FMeshBatch& OutMeshBatch, bool bDitheredLODTransition) const override;

	/** Sets up a FMeshBatch for a specific LOD and element. */
	virtual bool GetMeshElement(int32 LODIndex, int32 BatchIndex, int32 ElementIndex, uint8 InDepthPriorityGroup, bool bUseSelectionOutline, bool bAllowPreCulledIndices, FMeshBatch& OutMeshBatch) const override;

	/** Sets up a wireframe FMeshBatch for a specific LOD. */
	virtual bool GetWireframeMeshElement(int32 LODIndex, int32 BatchIndex, const FMaterialRenderProxy* WireframeRenderProxy, uint8 InDepthPriorityGroup, bool bAllowPreCulledIndices, FMeshBatch& OutMeshBatch) const override;

	/** Sets up a collision FMeshBatch for a specific LOD and element. */
	virtual bool GetCollisionMeshElement(int32 LODIndex, int32 BatchIndex, int32 ElementIndex, uint8 InDepthPriorityGroup, const FMaterialRenderProxy* RenderProxy, FMeshBatch& OutMeshBatch) const override;

#if RHI_RAYTRACING
	virtual bool HasRayTracingRepresentation() const override { return false; }
	virtual bool IsRayTracingRelevant() const override final { return false; }
	virtual bool IsRayTracingStaticRelevant() const override final { return false; }
#endif // RHI_RAYTRACING

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		return FStaticMeshSceneProxy::GetViewRelevance(View);
	}

	// 	  virtual uint32 GetMemoryFootprint( void ) const { return 0; }

private:
	void SetupMeshBatchForSpline(int32 InLODIndex, FMeshBatch& OutMeshBatch) const;

public:
	/** Parameters that define the spline, used to deform mesh */
	FSplineMeshParams SplineParams;
	/** Axis (in component space) that is used to determine X axis for co-ordinates along spline */
	FVector SplineUpDir;
	/** Smoothly (cubic) interpolate the Roll and Scale params over spline. */
	bool bSmoothInterpRollScale;
	/** Chooses the forward axis for the spline mesh orientation */
	ESplineMeshAxis::Type ForwardAxis;

	/** Minimum Z value of the entire mesh */
	float SplineMeshMinZ;
	/** Range of Z values over entire mesh */
	float SplineMeshScaleZ;

protected:
	TArray<FLODResources> LODResources;
};
