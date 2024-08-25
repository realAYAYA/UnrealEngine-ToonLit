// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

//! Order of the unreal vertex buffers when in mutable data
#define MUTABLE_VERTEXBUFFER_POSITION	0
#define MUTABLE_VERTEXBUFFER_TANGENT	1
#define MUTABLE_VERTEXBUFFER_TEXCOORDS	2

#include "MuR/Ptr.h"
#include "Containers/Array.h"

namespace mu
{
	class Mesh;
	class FMeshBufferSet;
}
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
	 * @param LODResource - LODRenderData whose sections are ought to be updated
	 * @param InMutableMesh - Mutable mesh to be used as reference for the section data update on the Skeletal Mesh
	 * @param InBoneMap - Bones to be set as part of the sections.
	 * @param InFirstBoneMapIndex - Index to the first BoneMap bone that belongs to this LODResource.
	 */
	CUSTOMIZABLEOBJECT_API void SetupRenderSections(
		FSkeletalMeshLODRenderData& LODResource,
		const mu::Ptr<const mu::Mesh> InMutableMesh,
		const TArray<uint16>& InBoneMap,
		const int32 InFirstBoneMapIndex);


	/** Initializes the LODResource's VertexBuffers with dummy data to prepare it for streaming.
	 * @param LODResource - LODRenderData to update
	 * @param InMutableMesh - Mutable mesh to be used as reference for the section data update on the Skeletal Mesh
	 * @param bAllowCPUAccess - Keeps this LODs data on the CPU so it can be used for things such as sampling in FX.
	 */
	CUSTOMIZABLEOBJECT_API void InitVertexBuffersWithDummyData(
		FSkeletalMeshLODRenderData& LODResource,
		const mu::Ptr<const mu::Mesh> InMutableMesh,
		const bool bAllowCPUAccess);

	/** Performs a copy of the data found on the vertex buffers on the mutable mesh to the buffers of the skeletal mesh
	 * @param LODResource - LODRenderData to update
	 * @param InMutableMesh - Mutable mesh to be used as reference for the section data update on the Skeletal Mesh
	 * @param bAllowCPUAccess - Keeps this LODs data on the CPU so it can be used for things such as sampling in FX.
	 */
	CUSTOMIZABLEOBJECT_API void CopyMutableVertexBuffers(
		FSkeletalMeshLODRenderData& LODResource,
		const mu::Ptr<const mu::Mesh> InMutableMesh,
		const bool bAllowCPUAccess);

	
	/**
	 * Initializes the LODResource's IndexBuffers with dummy data to prepare it for streaming.
	 * @param LODResource - LODRenderData to be updated.
	 * @param InMutableMesh - The mutable mesh whose index buffers you want to work with
	 */
	CUSTOMIZABLEOBJECT_API void InitIndexBuffersWithDummyData(
		FSkeletalMeshLODRenderData& LODResource,
		const mu::Ptr<const mu::Mesh> InMutableMesh);

	/**
	 *Performs a copy of the data found on the index buffers on the mutable mesh to the buffers of the skeletal mesh
	 * @param LODResource - LODRenderData to be updated.
	 * @param InMutableMesh - The mutable mesh whose index buffers you want to work with
	 * @return True if the operation could be performed successfully, false if not.
	 */
	CUSTOMIZABLEOBJECT_API bool CopyMutableIndexBuffers(
		FSkeletalMeshLODRenderData& LODResource,
		const mu::Ptr<const mu::Mesh> InMutableMesh);
	

	/**
	 *Performs a copy of the data found on the index buffers on the mutable mesh to the buffers of the skeletal mesh
	 * @param LODResource - LODRenderData to be updated.
	 * @param InProfileName - Name of the profile to generate.
	 * @param InMutableMeshVertexBuffers - The mutable buffers to be reading data from
	 * @param InBoneIndexBuffer - The buffer containing the indices for the bones.
	 * @return True if the operation could be performed successfully,	 false if not.
	 */
	CUSTOMIZABLEOBJECT_API void CopyMutableSkinWeightProfilesBuffers(
		FSkeletalMeshLODRenderData& LODResource,
		const FName InProfileName,
		const mu::FMeshBufferSet& InMutableMeshVertexBuffers,
		const int32 InBoneIndexBuffer);


	/**
	 *Performs a copy of the render data of a specific Skeletal Mesh LOD to another Skeletal Mesh
	 * @param LODResource - LODRenderData to copy to.
	 * @param SourceLODResource - LODRenderData to copy from.
	 * @param SkeletalMesh - Owner of both LODResource and SourceLODResource. 
	 * @param bAllowCPUAccess - Keeps this LODs data on the CPU so it can be used for things such as sampling in FX.
	 */
	CUSTOMIZABLEOBJECT_API void CopySkeletalMeshLODRenderData(
		FSkeletalMeshLODRenderData& LODResource,
		FSkeletalMeshLODRenderData& SourceLODResource,
		const USkeletalMesh& SkeletalMesh,
		const bool bAllowCPUAccess
	);

	/**
	 * Update SkeletalMeshLODRenderData buffers size.
	 * @param LODResource - LODRenderData to be updated.
	 */
	CUSTOMIZABLEOBJECT_API void UpdateSkeletalMeshLODRenderDataBuffersSize(
		FSkeletalMeshLODRenderData& LODResource
	);
}

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "ReferenceSkeleton.h"
#endif
