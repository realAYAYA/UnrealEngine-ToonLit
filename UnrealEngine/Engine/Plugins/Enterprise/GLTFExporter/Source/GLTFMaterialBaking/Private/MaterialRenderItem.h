// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CanvasTypes.h"
#include "MaterialRenderItemData.h"
#include "DynamicMeshBuilder.h"

class FSceneViewFamily;
class FMaterialRenderProxy;
class FSceneView;
class FRHICommandListImmediate;
struct FGLTFMaterialData;
struct FGLTFMeshRenderData;
struct FMeshPassProcessorRenderState;

// This will hold onto the resources until not needed anymore.
// Move constructor makes it easier to send the destruction to another thread (render thread).
class FGLTFMeshBuilderResources : public FMeshBuilderOneFrameResources
{
public:
	FGLTFMeshBuilderResources() = default;
	FGLTFMeshBuilderResources(FGLTFMeshBuilderResources&& Other)
	{
		if (this != &Other)
		{
			VertexBuffer = Other.VertexBuffer;
			IndexBuffer = Other.IndexBuffer;
			VertexFactory = Other.VertexFactory;
			PrimitiveUniformBuffer = Other.PrimitiveUniformBuffer;

			Other.VertexBuffer = nullptr;
			Other.IndexBuffer = nullptr;
			Other.VertexFactory = nullptr;
			Other.PrimitiveUniformBuffer = nullptr;
		}
	}

	void Clear()
	{
		FMeshBuilderOneFrameResources::~FMeshBuilderOneFrameResources();
		VertexBuffer = nullptr;
		IndexBuffer = nullptr;
		VertexFactory = nullptr;
		PrimitiveUniformBuffer = nullptr;
	}
};

class FGLTFMeshMaterialRenderItem : public FCanvasBaseRenderItem
{
public:
	FGLTFMeshMaterialRenderItem(const FIntPoint& InTextureSize, const FGLTFMeshRenderData* InMeshSettings, FDynamicMeshBufferAllocator* InDynamicMeshBufferAllocator = nullptr);
	virtual ~FGLTFMeshMaterialRenderItem();

	/** Begin FCanvasBaseRenderItem overrides */
	virtual bool Render_RenderThread(FCanvasRenderContext& RenderContext, FMeshPassProcessorRenderState& DrawRenderState, const FCanvas* Canvas) final;
	virtual bool Render_GameThread(const FCanvas* Canvas, FCanvasRenderThreadScope& RenderScope) final;
	/** End FCanvasBaseRenderItem overrides */

	/** Populate vertices and indices according to available mesh data and otherwise uses simple quad */
	void GenerateRenderData();
protected:
	/** Enqueues the current material to be rendered */
	void QueueMaterial(FCanvasRenderContext& RenderContext, FMeshPassProcessorRenderState& DrawRenderState, const FSceneView* View);

	/** Helper functions to populate render data using either mesh data or a simple quad */
	void PopulateWithQuadData();
	void PopulateWithMeshData();
public:
	/** Mesh settings to use while baking out the material */
	const FGLTFMeshRenderData* MeshSettings;
	/** The texture size to use while baking */
	FIntPoint TextureSize;
	/** Material render proxy (material/shader) to use while baking */
	FMaterialRenderProxy* MaterialRenderProxy;
	/** Vertex and index data representing the mesh or a quad */
	TArray<FDynamicMeshVertex> Vertices;
	TArray<uint32> Indices;
	/** Light cache interface object to simulate lightmap behavior in case the material used prebaked ambient occlusion */
	FLightCacheInterface* LCI;
	/** View family to use while baking */
	FSceneViewFamily* ViewFamily;
private:
	FMeshBatch MeshElement;
	bool bMeshElementDirty;
	FGLTFMeshBuilderResources MeshBuilderResources;
	FDynamicMeshBufferAllocator* DynamicMeshBufferAllocator;
};
