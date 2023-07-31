// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RenderGraphBuilder.h"
#include "RenderGraphResources.h"

#include "RHIGPUReadback.h"

#include "CoreMinimal.h"

class FGlobalShaderMap;

namespace Nanite
{
	struct FStreamOutRequest
	{
		uint32 PrimitiveId;
		uint32 NumMaterials;
		uint32 NumSegments;
		uint32 SegmentMappingOffset;
		uint32 AuxiliaryDataOffset;
		uint32 MeshDataOffset;
	};

	/*
	* Stream out nanite mesh data into buffers in a uncompressed format
	* 
	* (if the NodesAndClusterBatchesBuffer reference is null, a new buffer will be created which can be reused for subsequent calls)
	*/
	void StreamOutData(
		FRDGBuilder& GraphBuilder,
		FGlobalShaderMap* ShaderMap,
		FShaderResourceViewRHIRef GPUScenePrimitiveBufferSRV,
		TRefCountPtr<FRDGPooledBuffer>& NodesAndClusterBatchesBuffer,
		float CutError,
		uint32 NumRequests,
		FRDGBufferRef RequestBuffer,
		FRDGBufferRef SegmentMappingBuffer,
		FRDGBufferRef MeshDataBuffer,
		FRDGBufferRef AuxiliaryDataBuffer,
		FRDGBufferRef VertexBuffer,
		uint32 MaxNumVertices,
		FRDGBufferRef IndexBuffer,
		uint32 MaxNumIndices
	);
} // namespace Nanite
