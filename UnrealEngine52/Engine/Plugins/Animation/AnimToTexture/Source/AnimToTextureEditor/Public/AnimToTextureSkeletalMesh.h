// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimToTextureDataAsset.h"
#include "CoreMinimal.h"
#include "Containers/StaticArray.h"
#include "GPUSkinPublicDefs.h" 

namespace AnimToTexture_Private
{

template <uint16 NumInfluences>
struct TVertexSkinWeight
{
	TStaticArray<uint16, NumInfluences> MeshBoneIndices;
	TStaticArray<uint8, NumInfluences>  BoneWeights;
};

using VertexSkinWeightMax  = TVertexSkinWeight<MAX_TOTAL_INFLUENCES>;
using VertexSkinWeightFour = TVertexSkinWeight<4>;

/* Returns StaticMesh Vertex Positions */
int32 GetVertices(const UStaticMesh* StaticMesh, const int32 LODIndex,
	TArray<FVector3f>& OutPositions, TArray<FVector3f>& OutNormals);

/* Returns RefPose Vertex Positions */
int32 GetVertices(const USkeletalMesh* SkeletalMesh, const int32 LODIndex,
	TArray<FVector3f>& OutPositions);

/* Returns Triangles vertex indices */
int32 GetTriangles(const USkeletalMesh* SkeletalMesh, const int32 LODIndex,
	TArray<FIntVector3>& OutTriangles);

/* Computes CPUSkinning at Pose */
void GetSkinnedVertices(const USkeletalMeshComponent* SkeletalMeshComponent, const int32 LODIndex,
	TArray<FVector3f>& OutPositions);

/** Gets Skin Weights Data from SkeletalMeshComponent */
void GetSkinWeights(const USkeletalMesh* SkeletalMesh, const int32 LODIndex, 
	TArray<VertexSkinWeightMax>& OutSkinWeights);

/** Reduce Weights from MAX_TOTAL_INFLUENCES to 4 */
void ReduceSkinWeights(const TArray<VertexSkinWeightMax>& InSkinWeights, TArray<VertexSkinWeightFour>& OutSkinWeights);

/* Returns Number of RawBones (no virtual bones)*/
int32 GetNumBones(const USkeletalMesh* SkeletalMesh);

/* Returns RefPose Bone Transforms.
   Only the RawBones are returned (no virtual bones)
   The returned transforms are in ComponentSpace */
void GetRefBoneTransforms(const USkeletalMesh* SkeletalMesh, TArray<FTransform>& OutTransforms);

/* Returns Bone exist in the RawBone list (no virtual bones) */
bool HasBone(const USkeletalMesh* SkeletalMesh, const FName& Bone);

/* Returns Bone Names. 
   Only the RawBones are returned (no virtual bones) */
void GetBoneNames(const USkeletalMesh* SkeletalMesh, TArray<FName>& OutNames);

} // end namespace AnimToTexture_Private
