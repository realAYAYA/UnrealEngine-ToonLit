// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Materials/MaterialInterface.h"
#include "PrimitiveSceneProxy.h"
#include "Materials/MaterialRelevance.h"
#include "WaterQuadTree.h"
#include "WaterVertexFactory.h"
#include "RayTracingGeometry.h"
#include "WaterQuadTreeGPU.h"

class FMeshElementCollector;
struct FRayTracingMaterialGatheringContext;

class UWaterMeshComponent;

/** Water mesh scene proxy */

class FWaterMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	FWaterMeshSceneProxy(UWaterMeshComponent* Component);

	virtual ~FWaterMeshSceneProxy();

	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override;

	virtual void DestroyRenderThreadResources() override;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	virtual const TArray<FBoxSphereBounds>* GetOcclusionQueries(const FSceneView* View) const override;

	virtual void AcceptOcclusionResults(const FSceneView* View, TArray<bool>* Results, int32 ResultsStart, int32 NumResults) override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	virtual bool HasSubprimitiveOcclusionQueries() const override;

	virtual bool CanBeOccluded() const override
	{
		return !MaterialRelevance.bDisableDepthTest;
	}

	virtual uint32 GetMemoryFootprint() const override
	{
		return(sizeof(*this) + GetAllocatedSize());
	}

	uint32 GetAllocatedSize() const
	{
		return(FPrimitiveSceneProxy::GetAllocatedSize() + (WaterVertexFactories.GetAllocatedSize() + WaterVertexFactories.Num() * sizeof(FWaterVertexFactoryType)) + WaterQuadTree.GetAllocatedSize());
	}

#if WITH_WATER_SELECTION_SUPPORT
	virtual HHitProxy* CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies) override;
#endif // WITH_WATER_SELECTION_SUPPORT

	// At runtime, we only ever need one version of the vertex factory : with selection support (editor) or without : 
	using FWaterVertexFactoryType = TWaterVertexFactory<WITH_WATER_SELECTION_SUPPORT, EWaterVertexFactoryDrawMode::NonIndirect>;
	using FWaterVertexFactoryIndirectDrawType = TWaterVertexFactory<WITH_WATER_SELECTION_SUPPORT, EWaterVertexFactoryDrawMode::Indirect>;
	using FWaterVertexFactoryIndirectDrawISRType = TWaterVertexFactory<WITH_WATER_SELECTION_SUPPORT, EWaterVertexFactoryDrawMode::IndirectInstancedStereo>;
	using FWaterInstanceDataBuffersType = TWaterInstanceDataBuffers<WITH_WATER_SELECTION_SUPPORT>;
	using FWaterMeshUserDataBuffersType = TWaterMeshUserDataBuffers<WITH_WATER_SELECTION_SUPPORT>;

#if RHI_RAYTRACING
	virtual void GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances) override final;
	virtual bool HasRayTracingRepresentation() const override { return true; }
	virtual bool IsRayTracingRelevant() const override { return true; }
#endif

private:
	struct FWaterLODParams
	{
		int32 LowestLOD;
		float HeightLODFactor;
		float WaterHeightForLOD;
	};

#if RHI_RAYTRACING
	struct FRayTracingWaterData
	{
		FRayTracingGeometry Geometry;
		FRWBuffer DynamicVertexBuffer;
	};
#endif

	bool HasWaterData() const 
	{
		return WaterQuadTree.GetNodeCount() != 0 && DensityCount != 0;
	}

	FWaterLODParams GetWaterLODParams(const FVector& Position) const;

#if RHI_RAYTRACING
	void SetupRayTracingInstances(FRHICommandListBase& RHICmdList, int32 NumInstances, uint32 DensityIndex);
#endif

	FMaterialRelevance MaterialRelevance;

	// One vertex factory per LOD
	TArray<FWaterVertexFactoryType*> WaterVertexFactories;
	FWaterVertexFactoryIndirectDrawType* WaterVertexFactoryIndirectDraw = nullptr;
	FWaterVertexFactoryIndirectDrawISRType* WaterVertexFactoryIndirectDrawISR = nullptr;

	/** Tiles containing water, stored in a quad tree */
	FWaterQuadTree WaterQuadTree;

	/** GPU quad tree instance. Only initialized and used if WaterQuadTree.IsGPUQuadTree() is true. */
	FWaterQuadTreeGPU QuadTreeGPU;

	/** Unique Instance data buffer shared accross water batch draw calls */	
	FWaterInstanceDataBuffersType* WaterInstanceDataBuffers = nullptr;

	/** Per-"water render group" user data (the number of groups might vary depending on whether we're in the editor or not) */
	FWaterMeshUserDataBuffersType* WaterMeshUserDataBuffers = nullptr;

	double WaterQuadTreeMinHeight = DBL_MAX;
	double WaterQuadTreeMaxHeight = -DBL_MAX;

	/** The world-space bounds of the current water info texture coverage. The Water mesh should only render tiles within this bounding box. */
	FBox2D WaterInfoBounds = FBox2D(ForceInit);

	/** Scale of the concentric LOD squares  */
	float LODScale = -1.0f;

	/** Number of quads per side of a water quad tree tile at LOD0 */
	int32 NumQuadsLOD0 = 0;

	int32 NumQuadsPerIndirectDrawTile = 0;

	/** Number of densities (same as number of grid index/vertex buffers) */
	int32 DensityCount = 0;

	int32 ForceCollapseDensityLevel = TNumericLimits<int32>::Max();

	mutable int32 HistoricalMaxViewInstanceCount = 0;

#if RHI_RAYTRACING
	// Per density array of ray tracing geometries.
	TArray<TArray<FRayTracingWaterData>> RayTracingWaterData;	
#endif

	struct FOcclusionCullingResults
	{
		uint32 FrameNumber;
		TArray<bool> Results;
	};

	TArray<FBoxSphereBounds> OcclusionCullingBounds;
	TArray<FBoxSphereBounds> EmptyOcclusionCullingBounds;
	TMap<uint32, FOcclusionCullingResults> OcclusionResults;
	UE::FMutex OcclusionResultsMutex;
	int32 OcclusionResultsFarMeshOffset = INT32_MAX;
	uint32 SceneProxyCreatedFrameNumberRenderThread = INDEX_NONE;

	mutable FWaterQuadTreeGPU::FTraverseParams WaterQuadTreeGPUTraverseParams;
	mutable bool bNeedToTraverseGPUQuadTree = false;

	/** Initializes the GPU quad tree */
	void BuildGPUQuadTree(FRDGBuilder& GraphBuilder);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Materials/Material.h"
#endif
