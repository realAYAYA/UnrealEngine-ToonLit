// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if RHI_RAYTRACING

class RENDERER_API FRayTracingDynamicGeometryCollection
{
public:
	FRayTracingDynamicGeometryCollection();
	~FRayTracingDynamicGeometryCollection();

	void AddDynamicMeshBatchForGeometryUpdate(
		const FScene* Scene, 
		const FSceneView* View, 
		const FPrimitiveSceneProxy* PrimitiveSceneProxy, 
		FRayTracingDynamicGeometryUpdateParams Params,
		uint32 PrimitiveId 
	);

	// Starts an update batch and returns the current shared buffer generation ID which is used for validation.
	int64 BeginUpdate();
	void DispatchUpdates(FRHICommandListImmediate& ParentCmdList, FRHIBuffer* ScratchBuffer);
	void EndUpdate(FRHICommandListImmediate& RHICmdList);

	uint32 ComputeScratchBufferSize();

private:

	TArray<struct FMeshComputeDispatchCommand> DispatchCommands;
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
