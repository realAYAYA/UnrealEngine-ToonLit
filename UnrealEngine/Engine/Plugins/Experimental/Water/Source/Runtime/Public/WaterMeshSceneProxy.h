// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Materials/Material.h"
#include "PrimitiveSceneProxy.h"
#include "UObject/ObjectMacros.h"
#include "WaterInstanceDataBuffer.h"
#include "WaterQuadTree.h"
#include "WaterVertexFactory.h"

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

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	virtual bool CanBeOccluded() const override
	{
		return !MaterialRelevance.bDisableDepthTest;
	}

	virtual uint32 GetMemoryFootprint() const
	{
		return(sizeof(*this) + GetAllocatedSize());
	}

	uint32 GetAllocatedSize() const
	{
		return(FPrimitiveSceneProxy::GetAllocatedSize() + (WaterVertexFactories.GetAllocatedSize() + WaterVertexFactories.Num() * sizeof(WaterVertexFactoryType)) + WaterQuadTree.GetAllocatedSize());
	}

#if WITH_WATER_SELECTION_SUPPORT
	virtual HHitProxy* CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies) override;
#endif // WITH_WATER_SELECTION_SUPPORT

	// At runtime, we only ever need one version of the vertex factory : with selection support (editor) or without : 
	using WaterVertexFactoryType = TWaterVertexFactory<WITH_WATER_SELECTION_SUPPORT>;
	using WaterInstanceDataBuffersType = TWaterInstanceDataBuffers<WITH_WATER_SELECTION_SUPPORT>;
	using WaterMeshUserDataBuffersType = TWaterMeshUserDataBuffers<WITH_WATER_SELECTION_SUPPORT>;

#if RHI_RAYTRACING
	virtual void GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances) override final;
	virtual bool HasRayTracingRepresentation() const override { return true; }
	virtual bool IsRayTracingRelevant() const override { return true; }
#endif

	void OnTessellatedWaterMeshBoundsChanged_GameThread(const FBox2D& InTessellatedWaterMeshBounds);

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
	void SetupRayTracingInstances(int32 NumInstances, uint32 DensityIndex);
#endif

	void OnTessellatedWaterMeshBoundsChanged_RenderThread(const FBox2D& InTessellatedWaterMeshBounds);

	FMaterialRelevance MaterialRelevance;

	// One vertex factory per LOD
	TArray<WaterVertexFactoryType*> WaterVertexFactories;

	/** Tiles containing water, stored in a quad tree */
	FWaterQuadTree WaterQuadTree;

	/** Unique Instance data buffer shared accross water batch draw calls */	
	WaterInstanceDataBuffersType* WaterInstanceDataBuffers;

	/** Per-"water render group" user data (the number of groups might vary depending on whether we're in the editor or not) */
	WaterMeshUserDataBuffersType* WaterMeshUserDataBuffers;

	/** The world-space bounds of the current water info texture coverage. The Water mesh should only render tiles within this bounding box. */
	FBox2D TessellatedWaterMeshBounds = FBox2D(ForceInit);

	/** Scale of the concentric LOD squares  */
	float LODScale = -1.0f;

	/** Number of densities (same as number of grid index/vertex buffers) */
	int32 DensityCount = 0;

	int32 ForceCollapseDensityLevel = TNumericLimits<int32>::Max();

	mutable int32 HistoricalMaxViewInstanceCount = 0;

#if RHI_RAYTRACING
	// Per density array of ray tracing geometries.
	TArray<TArray<FRayTracingWaterData>> RayTracingWaterData;	
#endif
};
