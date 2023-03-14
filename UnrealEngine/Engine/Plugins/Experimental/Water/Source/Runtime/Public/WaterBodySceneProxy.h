// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Scene.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "StaticMeshResources.h"
#include "DynamicMeshBuilder.h"

class UWaterBodyComponent;
class UWaterBodyLakeComponent;
class UWaterBodyOceanComponent;
class UWaterBodyRiverComponent;
class FMaterialRenderProxy;
struct FWaterBodyMeshSection;
struct FWaterBodySectionedMeshProxy;

enum class EWaterInfoPass
{
	None,
	Depth,
	Color,
	Dilation,
};

class FWaterBodySceneProxy final : public FPrimitiveSceneProxy
{
public:
	FWaterBodySceneProxy(UWaterBodyComponent* Component);
	virtual ~FWaterBodySceneProxy();
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual SIZE_T GetTypeHash() const override;
	virtual uint32 GetMemoryFootprint() const override;
	uint32 GetAllocatedSize() const;

	bool IsShown(const FSceneView* View) const;

	bool IsWithinWaterInfoPass(EWaterInfoPass InPass) const { return CurrentWaterInfoPass == InPass; }
	void SetWithinWaterInfoPass(EWaterInfoPass InPass) { CurrentWaterInfoPass = InPass; }

	void OnTessellatedWaterMeshBoundsChanged_GameThread(const FBox2D& TessellatedWaterMeshBounds);
private:
	void OnTessellatedWaterMeshBoundsChanged_RenderThread(const FBox2D& TessellatedWaterMeshBounds);

	struct FWaterBodySectionedLODMesh
	{
	public:
		FWaterBodySectionedLODMesh(ERHIFeatureLevel::Type InFeatureLevel) : VertexFactory(InFeatureLevel, "WaterBodySectionedMesh"), Sections() {}

		void InitFromSections(const TArray<FWaterBodyMeshSection>& MeshSections);
		void ReleaseResources();
		void RebuildIndexBuffer(const FBox2D& TessellatedWaterMeshBounds);
		uint32 GetAllocatedSize() const;

		bool GetMeshElements(FMeshBatch& OutMeshBatch, uint8 DepthPriorityGroup, bool bUseReverseCulling) const;

		FStaticMeshVertexBuffers VertexBuffers;
		FDynamicMeshIndexBuffer32 IndexBuffer;
		FLocalVertexFactory VertexFactory;

		struct FWaterBodySectionedMeshProxy
		{
		public:
			FWaterBodySectionedMeshProxy(const FBox2D& InBounds)
				: Bounds(InBounds)
				, Indices()
			{
				Indices.Reserve(6); // Most sections are quads with 6 indices.
			}

			FBox2D Bounds;
			TArray<uint32> Indices;
		};

		TArray<FWaterBodySectionedMeshProxy> Sections;

		bool bInitialized = false;
	};

	struct FWaterBodyMesh
	{
	public:
		FWaterBodyMesh(ERHIFeatureLevel::Type InFeatureLevel) : VertexFactory(InFeatureLevel, "WaterBodyMesh") {}

		void Init(TArray<FDynamicMeshVertex>& Vertices, TArray<uint32>& Indices);
		void ReleaseResources();

		bool GetMeshElements(FMeshBatch& OutMeshBatch, uint8 DepthPriorityGroup, bool bUseReverseCulling) const;

		FStaticMeshVertexBuffers VertexBuffers;
		FDynamicMeshIndexBuffer32 IndexBuffer;
		FLocalVertexFactory VertexFactory;
	};

	FWaterBodySectionedLODMesh WaterBodySectionedLODMesh;

	FWaterBodyMesh WaterBodyInfoMesh;
	FWaterBodyMesh WaterBodyInfoDilatedMesh;

	/** Material to use when rendering this scene proxy into the water info texture */
	FMaterialRenderProxy* WaterInfoMaterial = nullptr;
	/** Material to use when rendering this scene proxy in the main scene */
	FMaterialRenderProxy* WaterLODMaterial = nullptr;

	FMaterialRelevance WaterInfoMaterialRelevance;
	FMaterialRelevance WaterLODMaterialRelevance;

	EWaterInfoPass CurrentWaterInfoPass = EWaterInfoPass::None;
};

