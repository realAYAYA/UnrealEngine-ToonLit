// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCO/CustomizableObjectSystemPrivate.h"

namespace mu { class FMeshBufferSet; }
struct FReferenceSkeleton;

class FSkeletalMeshLODRenderData;
class USkeletalMesh;
class USkeleton;


namespace UnrealConversionUtils
{
	/*
	 * Shared methods between mutable instance update and viewport mesh generation
	 * These are the result of stripping up parts of the reference code to be able to share it on the two
	 * current pipelines (instance update and USkeletal mesh generation for a mesh viewport)
	 */
	
	/**
	 * Prepares the render sections found on the InSkeletalMesh and sets them up accordingly what the InMutableMesh requires
	 * @param OutSkeletalMesh - The Skeletal mesh whose sections are ought to be updated
	 * @param InMutableMesh - Mutable mesh to be used as reference for the section data update on the Skeletal Mesh
	 * @param MeshLODIndex - The Mesh lod to be working with
	 * @param InBoneMap - The bones to be set as part of the sections.
	 */
	CUSTOMIZABLEOBJECT_API void SetupRenderSections(
		const USkeletalMesh* OutSkeletalMesh,
		const mu::MeshPtrConst InMutableMesh,
		const int32 InMeshLODIndex,
		const TArray<uint16>& InBoneMap,
		const int32 InFirstBoneMapIndex);


	/** Performs a copy of the data found on the vertex buffers on the mutable mesh to the buffers of the skeletal mesh
	 * @param OutSkeletalMesh - Skeletal Mesh to be updated with new buffer data
	 * @param InMutableMesh - Mutable mesh to be used as reference for the section data update on the Skeletal Mesh
	 * @param MeshLODIndex - The LOD index we are working with.
	 */
	CUSTOMIZABLEOBJECT_API void CopyMutableVertexBuffers(
		USkeletalMesh* OutSkeletalMesh,
		const mu::MeshPtrConst InMutableMesh,
		const int32 InMeshLODIndex);

	
	/**
	 *Performs a copy of the data found on the index buffers on the mutable mesh to the buffers of the skeletal mesh
	 * @param InMutableMesh - The mutable mesh whose index buffers you want to work with
	 * @param OutLODModel - The LOD model to be updated.
	 * @return True if the operation could be performed successfully, false if not.
	 */
	CUSTOMIZABLEOBJECT_API bool CopyMutableIndexBuffers(mu::MeshPtrConst InMutableMesh,
	                                                    FSkeletalMeshLODRenderData& OutLODModel);
	

	/**
	 *Performs a copy of the data found on the index buffers on the mutable mesh to the buffers of the skeletal mesh
	 * @param OutLODModel - The LOD model to be updated.
	 * @param InProfileName - Name of the profile to generate.
	 * @param InMutableMeshVertexBuffers - The mutable buffers to be reading data from
	 * @param InBoneIndexBuffer - The buffer containing the indices for the bones.
	 * @return True if the operation could be performed successfully,	 false if not.
	 */
	CUSTOMIZABLEOBJECT_API void CopyMutableSkinWeightProfilesBuffers(
		FSkeletalMeshLODRenderData& OutLODModel,
		const FName InProfileName,
		const mu::FMeshBufferSet& InMutableMeshVertexBuffers,
		const int32 InBoneIndexBuffer);


	/**
	 *Performs a copy of the render data of a specific Skeletal Mesh LOD to another Skeletal Mesh
	 * @param SrcSkeletalMesh - Skeletal Mesh with the data to copy.
	 * @param DestSkeletalMesh - Skeletal Mesh where the data will be copied.
	 * @param SrcLODIndex - Index of the LOD to copy.
	 * @param DestSkeletalMesh - Index of the LOD that will be copyed to.
	 */
	CUSTOMIZABLEOBJECT_API void CopySkeletalMeshLODRenderData(
		const USkeletalMesh* SrcSkeletalMesh,
		USkeletalMesh* DestSkeletalMesh,
		int32 SrcLODIndex,
		int32 DestLODIndex);

}

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "ReferenceSkeleton.h"
#endif
