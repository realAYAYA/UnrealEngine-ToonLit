// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "HairStrandsInterface.h"
#include "RenderGraphResources.h"
#include "Shader.h"


class FSkeletalMeshRenderData;
struct FHairStrandsRestRootResource;
struct FHairStrandsDeformedRootResource;
struct FHairMeshesRestResource;
struct FHairMeshesDeformedResource;
struct FHairCardsRestResource;
struct FHairCardsDeformedResource;
struct FHairStrandsDeformedResource;

struct FHairStrandsProjectionMeshData
{
	struct Section
	{
		FTransform LocalToWorld;
		FRDGBufferSRVRef RDGPositionBuffer = nullptr;
		FRDGBufferSRVRef RDGPreviousPositionBuffer = nullptr;
		FRHIShaderResourceView* PositionBuffer = nullptr;
		FRHIShaderResourceView* PreviousPositionBuffer = nullptr;
		FRHIShaderResourceView* UVsBuffer = nullptr;
		FRHIShaderResourceView* IndexBuffer = nullptr;
		uint32 UVsChannelCount = 0;
		uint32 UVsChannelOffset = 0;
		uint32 NumPrimitives = 0;
		uint32 NumVertices = 0;
		uint32 VertexBaseIndex = 0;
		uint32 IndexBaseIndex = 0;
		uint32 TotalVertexCount = 0;
		uint32 TotalIndexCount = 0;
		uint32 SectionIndex = 0;
		int32 LODIndex = 0;
	};

	struct LOD
	{
		TArray<Section> Sections;
	};
	TArray<LOD> LODs;
};

// Return the max number of section/triangle a skeletal mesh can have. After this count, binding will be disabled
uint32 GetHairStrandsMaxSectionCount();
uint32 GetHairStrandsMaxTriangleCount();

enum class HairStrandsTriangleType
{
	RestPose,
	DeformedPose,
};

/* Update the triangles information on which hair stands have been projected */
void AddHairStrandUpdateMeshTrianglesPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const int32 LODIndex,
	const HairStrandsTriangleType Type,
	const FHairStrandsProjectionMeshData::LOD& MeshData,
	FHairStrandsRestRootResource* RestResources,
	FHairStrandsDeformedRootResource* DeformedResources);

/* Init the samples information to be used for interpolation*/
void AddHairStrandInitMeshSamplesPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const int32 LODIndex,
	const HairStrandsTriangleType Type,
	const FHairStrandsProjectionMeshData::LOD& MeshData,
	FHairStrandsRestRootResource* RestResources,
	FHairStrandsDeformedRootResource* DeformedResources);

/* Update the samples information to be used for interpolation*/
void AddHairStrandUpdateMeshSamplesPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const int32 LODIndex,
	const FHairStrandsProjectionMeshData::LOD& ProjectionMeshData,
	FHairStrandsRestRootResource* RestResources,
	FHairStrandsDeformedRootResource* DeformedResources);

void GenerateFolliculeMask(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const EPixelFormat Format,
	const FIntPoint Resolution,
	const uint32 MipCount,
	const uint32 KernelSizeInPixels,
	const uint32 Channel,
	const int32 LODIndex,
	FHairStrandsRestRootResource* RestResources,
	FRDGTextureRef& OutTexture);

void GenerateFolliculeMask(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const EPixelFormat Format,
	const FIntPoint Resolution,
	const uint32 MipCount,
	const uint32 KernelSizeInPixels,
	const uint32 Channel,
	const TArray<FRDGBufferRef>& RootUVBuffers,
	FRDGTextureRef& OutTexture);

void AddComputeMipsPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	FRDGTextureRef& OutTexture);

void AddSkinUpdatePass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	uint32 SectionIndex,
	uint32 BonesOffset, 
	class FSkinWeightVertexBuffer* SkinWeight,
	class FSkeletalMeshLODRenderData& RenderData,
	FRHIShaderResourceView* BoneMatrices,
	FRHIShaderResourceView* PrevBoneMatrices,
	FRDGBufferRef OutDeformedosition,
	FRDGBufferRef OutPreviousDeformedosition);

void AddHairMeshesRBFInterpolationPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const int32 MeshLODIndex,
	FHairMeshesRestResource* RestResources,
	FHairMeshesDeformedResource* DeformedResources,
	FHairStrandsRestRootResource* RestRootResources,
	FHairStrandsDeformedRootResource* DeformedRootResources);

void AddHairCardsRBFInterpolationPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const int32 MeshLODIndex,
	FHairCardsRestResource* RestResources,
	FHairCardsDeformedResource* DeformedResources,
	FHairStrandsRestRootResource* RestRootResources,
	FHairStrandsDeformedRootResource* DeformedRootResources);

void AddHairStrandUpdatePositionOffsetPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const int32 LODIndex,
	FHairStrandsDeformedRootResource* DeformedRootResources,
	FHairStrandsDeformedResource* DeformedResources);

FHairStrandsProjectionMeshData ExtractMeshData(FSkeletalMeshRenderData* RenderData);