// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "RenderGraphResources.h"
#include "RenderResource.h"

class FViewInfo;
class FSortedIndexBuffer;
struct FMeshBatchElementDynamicIndexBuffer;
class FViewInfo;

enum class EOITSortingType
{
	SortedTriangles,
	SortedPixels,
};

enum EOITPassType
{
	OITPass_None = 0,
	OITPass_RegularTranslucency = 1,
	OITPass_SeperateTranslucency = 2
};

enum class FTriangleSortingOrder
{
	FrontToBack,
	BackToFront,
};

// Sorted triangles data
struct FSortedTriangleData
{
	const FIndexBuffer* SourceIndexBuffer = nullptr;
	FIndexBuffer* SortedIndexBuffer = nullptr;

	FShaderResourceViewRHIRef  SourceIndexSRV = nullptr;
	FUnorderedAccessViewRHIRef SortedIndexUAV = nullptr;

	uint32 SortedFirstIndex = 0;
	uint32 SourceFirstIndex	= 0;
	uint32 NumPrimitives = 0;
	uint32 NumIndices = 0;

	EPrimitiveType SourcePrimitiveType = PT_TriangleList;
	EPrimitiveType SortedPrimitiveType = PT_TriangleList;

	bool IsValid() const { return SortedIndexBuffer != nullptr; }
};

// Order Independent Translucency pass data for a given translucent pass
struct FOITData
{
	EOITPassType PassType = OITPass_None;
	uint32 SupportedPass = 0;
	uint32 Method = 0;
	uint32 MaxSamplePerPixel = 0;
	uint32 MaxSideSamplePerPixel = 0;
	float TransmittanceThreshold = 0.f;

	FRDGTextureRef SampleColorTexture = nullptr;
	FRDGTextureRef SampleTransTexture = nullptr;
	FRDGTextureRef SampleDepthTexture = nullptr;
	FRDGTextureRef SampleCountTexture = nullptr;
};

// Order Independent Translucency scene data
struct FOITSceneData
{
	/* Allocate sorted-triangle data for a instance */
	FSortedTriangleData Allocate(const FIndexBuffer* InSource, EPrimitiveType PrimitiveType, uint32 InFirstIndex, uint32 InNumPrimitives);

	/* Deallocate sorted-triangle data */
	void Deallocate(FIndexBuffer* IndexBuffer);

	TArray<FSortedTriangleData> Allocations;
	TArray<FSortedIndexBuffer*> FreeBuffers;
	TQueue<uint32> FreeSlots;
	uint32 FrameIndex = 0;
};

namespace OIT
{
	/* Return true if OIT techniques are enabled/supported */
	bool IsEnabled(EOITSortingType Type, const FViewInfo& View);
	bool IsEnabled(EOITSortingType Type, EShaderPlatform ShaderPlatform);

	/* Return true if the current MeshBatch is compatible with per-instance sorted triangle */
	bool IsCompatible(const FMeshBatch& Mesh, ERHIFeatureLevel::Type InFeatureLevel);

	/* Sort triangles of all instances whose has the sorted triangle option enabled */
	void AddSortTrianglesPass(FRDGBuilder& GraphBuilder, const FViewInfo& View, FOITSceneData& OITSceneData, FTriangleSortingOrder SortType);

	/* Convert FSortedTriangleData into FMeshBatchElementDynamicIndexBuffer */
	void ConvertSortedIndexToDynamicIndex(FSortedTriangleData* In, FMeshBatchElementDynamicIndexBuffer* Out);

	/* Create OIT data for translucent pass */
	FOITData CreateOITData(FRDGBuilder& GraphBuilder, const FViewInfo& View, EOITPassType PassType);

	/* Compose all OIT samples into the target color buffer */
	void AddOITComposePass(FRDGBuilder& GraphBuilder, const FViewInfo& View, FOITData& OITData, FRDGTextureRef SceneColorTexture);
}
