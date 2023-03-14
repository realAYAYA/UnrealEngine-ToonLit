// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderGraphResources.h"

class FSkeletalMeshObject;
class FRDGPooledBuffer;
class FRHIShaderResourceView;

/** Functions that expose some internal functionality of FSkeletalMeshObject required by MeshDeformer systems. */
class FSkeletalMeshDeformerHelpers
{
public:

#pragma region GetInternals

	/** Get direct access to bone matrix buffer SRV. */
	ENGINE_API static FRHIShaderResourceView* GetBoneBufferForReading(
		FSkeletalMeshObject const* MeshObject,
		int32 LODIndex,
		int32 SectionIndex,
		bool bPreviousFrame);

	/** Get direct access to morph target buffer SRV. */
	ENGINE_API static FRHIShaderResourceView* GetMorphTargetBufferForReading(
		FSkeletalMeshObject const* MeshObject,
		int32 LODIndex,
		int32 SectionIndex,
		uint32 FrameNumber,
		bool bPreviousFrame);

	/** Buffer SRVs from the cloth system. */
	struct ENGINE_API FClothBuffers
	{
		int32 ClothInfluenceBufferOffset = 0;
		FRHIShaderResourceView* ClothInfluenceBuffer = nullptr;
		FRHIShaderResourceView* ClothSimulatedPositionAndNormalBuffer = nullptr;
		FMatrix44f ClothToLocal = FMatrix44f::Identity;
	};

	/** Get direct access to cloth buffer SRVs. */
	ENGINE_API static FClothBuffers GetClothBuffersForReading(
		FSkeletalMeshObject const* MeshObject,
		int32 LODIndex,
		int32 SectionIndex,
		uint32 FrameNumber,
		bool bPreviousFrame);

#pragma endregion GetInternals

#pragma region SetInternals

	/** Buffer override behavior for SetVertexFactoryBufferOverrides. */
	enum class EOverrideType
	{
		All,		// Clear overrides for each buffer input that is null.
		Partial,	// Leave existing overrides for each buffer input that is null.
	};

	/** Apply buffer overrides to the pass through vertex factory. */
	ENGINE_API static void SetVertexFactoryBufferOverrides(
		FSkeletalMeshObject* MeshObject,
		int32 LODIndex, 
		EOverrideType OverrideType,
		TRefCountPtr<FRDGPooledBuffer> const& PositionBuffer, 
		TRefCountPtr<FRDGPooledBuffer> const& TangentBuffer, 
		TRefCountPtr<FRDGPooledBuffer> const& ColorBuffer);

	/** Reset all buffer overrides that were applied through SetVertexFactoryBufferOverrides. */
	ENGINE_API static void ResetVertexFactoryBufferOverrides_GameThread(FSkeletalMeshObject* MeshObject, int32 LODIndex);

#if RHI_RAYTRACING

	/** Update ray tracing geometry with new position vertex buffer. */
	ENGINE_API static void UpdateRayTracingGeometry(
		FSkeletalMeshObject* MeshObject,
		int32 LODIndex,
		TRefCountPtr<FRDGPooledBuffer> const& PositionBuffer);

#endif // RHI_RAYTRACING

#pragma endregion SetInternals
};
