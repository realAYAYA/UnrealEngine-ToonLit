// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphResources.h"

class FRDGBuffer;
class FRHIShaderResourceView;
class FSkeletalMeshObject;

/** Functions that expose some internal functionality of FSkeletalMeshObject required by MeshDeformer systems. */
class FSkeletalMeshDeformerHelpers
{
public:

#pragma region GetInternals

	/** Get direct access to bone matrix buffer SRV. */
	ENGINE_API static FRHIShaderResourceView* GetBoneBufferForReading(
		FSkeletalMeshObject const* InMeshObject,
		int32 InLodIndex,
		int32 InSectionIndex,
		bool bInPreviousFrame);

	/** Get direct access to morph target buffer SRV. */
	ENGINE_API static FRHIShaderResourceView* GetMorphTargetBufferForReading(
		FSkeletalMeshObject const* InMeshObject,
		int32 InLodIndex,
		int32 InSectionIndex,
		uint32 InFrameNumber,
		bool bInPreviousFrame);

	/** Buffer SRVs from the cloth system. */
	struct FClothBuffers
	{
		int32 ClothInfluenceBufferOffset = 0;
		FRHIShaderResourceView* ClothInfluenceBuffer = nullptr;
		FRHIShaderResourceView* ClothSimulatedPositionAndNormalBuffer = nullptr;
		FMatrix44f ClothToLocal = FMatrix44f::Identity;
	};

	/** Get direct access to cloth buffer SRVs. */
	ENGINE_API static FClothBuffers GetClothBuffersForReading(
		FSkeletalMeshObject const* InMeshObject,
		int32 InLodIndex,
		int32 InSectionIndex,
		uint32 InFrameNumber,
		bool bInPreviousFrame);

#pragma endregion GetInternals

#pragma region SetInternals

	/** 
	 * Allocate and bind a new position buffer and return it for writing.
	 * Ownership is handled by the MeshObject.
	 * If we call this more than once for the same MeshObject in the same frame then we return the allocation from the first call.
	 */
	ENGINE_API static FRDGBuffer* AllocateVertexFactoryPositionBuffer(FRDGBuilder& GraphBuilder, FSkeletalMeshObject* InMeshObject, int32 InLodIndex, TCHAR const* InBufferName);

	/**
	 * Allocate and bind a new tangent buffer and return it for writing.
	 * Ownership is handled by the MeshObject.
	 * If we call this more than once for the same MeshObject in the same frame then we return the allocation from the first call.
	 */
	ENGINE_API static FRDGBuffer* AllocateVertexFactoryTangentBuffer(FRDGBuilder& GraphBuilder,	FSkeletalMeshObject* InMeshObject, int32 InLodIndex, TCHAR const* InBufferName);

	/**
	 * Allocate and bind a new color buffer and return it for writing.
	 * Ownership is handled by the MeshObject.
	 * If we call this more than once for the same MeshObject in the same frame then we return the allocation from the first call.
	 */
	ENGINE_API static FRDGBuffer* AllocateVertexFactoryColorBuffer(FRDGBuilder& GraphBuilder, FSkeletalMeshObject* InMeshObject, int32 InLodIndex, TCHAR const* InBufferName);

	/** 
	 * Update all of the MeshObject's passthrough vertex factories with the currently allocated vertex buffers. 
	 * Usually call this after all AllocateVertexFactory*() functions for a frame.
	 */
	ENGINE_API static void UpdateVertexFactoryBufferOverrides(FRHICommandListBase& RHICmdList, FSkeletalMeshObject* InMeshObject, int32 InLodIndex);

	UE_DEPRECATED(5.4, "UpdateVertexFactoryBufferOverrides requires a command list.")
	ENGINE_API static void UpdateVertexFactoryBufferOverrides(FSkeletalMeshObject* InMeshObject, int32 InLodIndex);

	/** 
	 * Release all of the the buffers that have been allocated through the AllocateVertexFactory*() functions.
	 * Reset all of the MeshObject's passthrough vertex factories.  
	 */
	ENGINE_API static void ResetVertexFactoryBufferOverrides(FSkeletalMeshObject* InMeshObject, int32 InLodIndex);

#pragma endregion SetInternals
};
