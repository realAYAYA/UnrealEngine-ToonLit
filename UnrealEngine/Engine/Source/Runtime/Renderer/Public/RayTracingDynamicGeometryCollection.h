// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if RHI_RAYTRACING

#include "RHI.h"
#include "RHIUtilities.h"

class FPrimitiveSceneProxy;
class FScene;
class FSceneView;
struct FMeshComputeDispatchCommand;
struct FRayTracingDynamicGeometryUpdateParams;

class FRayTracingDynamicGeometryCollection
{
public:
	RENDERER_API FRayTracingDynamicGeometryCollection();
	RENDERER_API ~FRayTracingDynamicGeometryCollection();

	RENDERER_API void AddDynamicMeshBatchForGeometryUpdate(
		FRHICommandListBase& RHICmdList,
		const FScene* Scene, 
		const FSceneView* View, 
		const FPrimitiveSceneProxy* PrimitiveSceneProxy, 
		const FRayTracingDynamicGeometryUpdateParams& Params,
		uint32 PrimitiveId 
	);

	UE_DEPRECATED(5.3, "AddDynamicMeshBatchForGeometryUpdate now requires a command list.")
	RENDERER_API void AddDynamicMeshBatchForGeometryUpdate(
		const FScene* Scene, 
		const FSceneView* View, 
		const FPrimitiveSceneProxy* PrimitiveSceneProxy, 
		const FRayTracingDynamicGeometryUpdateParams& Params,
		uint32 PrimitiveId 
	);

	// Starts an update batch and returns the current shared buffer generation ID which is used for validation.
	RENDERER_API int64 BeginUpdate();
	RENDERER_API void DispatchUpdates(FRHICommandListImmediate& ParentCmdList, FRHIBuffer* ScratchBuffer);
	RENDERER_API void EndUpdate(FRHICommandListImmediate& RHICmdList);

	// Clears the working arrays to not hold any references.
	RENDERER_API void Clear();

	RENDERER_API uint32 ComputeScratchBufferSize();

private:

	TArray<FMeshComputeDispatchCommand> DispatchCommands;
	TArray<FRayTracingGeometryBuildParams> BuildParams;
	TArray<FRayTracingGeometrySegment> Segments;

	struct FVertexPositionBuffer
	{
		FRWBuffer RWBuffer;
		uint32 UsedSize = 0;
		uint32 LastUsedGenerationID = 0;
	};
	TArray<FVertexPositionBuffer*> VertexPositionBuffers;

	// Any uniform buffers that must be kept alive until EndUpdate (after DispatchUpdates is called)
	TArray<FUniformBufferRHIRef> ReferencedUniformBuffers;

	// Generation ID when the shared vertex buffers have been reset. The current generation ID is stored in the FRayTracingGeometry to keep track
	// if the vertex buffer data is still valid for that frame - validated before generation the TLAS
	int64 SharedBufferGenerationID = 0;
};

#endif // RHI_RAYTRACING
