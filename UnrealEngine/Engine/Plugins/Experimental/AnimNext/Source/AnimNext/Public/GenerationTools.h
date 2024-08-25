// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BoneIndices.h"
#include "Containers/IndirectArray.h"
#include "ReferencePose.h"
#include "LODPose.h"

class USkeletalMeshComponent;
class USkeletalMesh;
class FSkeletalMeshLODRenderData;
struct FPoseContext;
class UPhysicsAsset;

namespace UE::AnimNext
{

struct FGenerationLODData
{
	TArray<FBoneIndexType> RequiredBones;									// All the bones required for the LOD
	TArray<FBoneIndexType> ExcludedBones;									// List of bones excluded from LOD 0
	TArray<FBoneIndexType> ExcludedBonesFromPrevLOD;						// list of bones excluded from previous LOD
};

class ANIMNEXT_API FGenerationTools
{
public:

	// Generates the reference pose data from a SkeletalMeshComponent and / or Skeletal Mesh asset
	// If no SkeletalMeshComponent is passed, the reference pose will not exclude invisible bones and will not include shadow shapes required bones
	// If no SkeletalMesh asset is passed, there will be no generation
	static bool GenerateReferencePose(const USkeletalMeshComponent* SkeletalMeshComponent
		, const USkeletalMesh* SkeletalMesh
		, UE::AnimNext::FReferencePose& OutAnimationReferencePose);

	// Generates the full list of bones required by a LOD, based on the SkeletalRetrieve RequiredBones
	// Compute ExcludedBones versus LOD0 and PreviousLOD
	// Remove ExcludedBonesFromPrevLOD from BonesInAllLODS
	static void GenerateRawLODData(const USkeletalMeshComponent* SkeletalMeshComponent
		, const USkeletalMesh* SkeletalMesh
		, const int32 LODIndex
		, const TIndirectArray<FSkeletalMeshLODRenderData>& LODRenderData
		, TArray<FBoneIndexType>& OutRequiredBones
		, TArray<FBoneIndexType>& OutFillComponentSpaceTransformsRequiredBones);

	// For each LOD > 0:
	// Retrieve RequiredBones
	// Compute ExcludedBones versus LOD0 and PreviousLOD
	static void GenerateLODData(const USkeletalMeshComponent* SkeletalMeshComponent
		, const USkeletalMesh* SkeletalMesh
		, const int32 StartLOD
		, const int32 NumLODs
		, const TIndirectArray<FSkeletalMeshLODRenderData>& LODRenderData
		, const TArray<FBoneIndexType>& RequiredBones_LOD0
		, TArray<FGenerationLODData>& GenerationLODData
		, TArray<FGenerationLODData>& GenerationComponentSpaceLODData);

	// Calculate the bone indexes difference from LOD0 for LODIndex
	static void CalculateDifferenceFromParentLOD(int32 LODIndex, TArray<FGenerationLODData>& GenerationLODData);

	// For each LOD :
	// Check the excluded bones in LOD(N) contain all the bones excluded in LOD(N-1)
	static bool CheckExcludedBones(const int32 NumLODs
		, const TArray<FGenerationLODData>& GenerationLODData
		, const USkeletalMesh* SkeletalMesh);

	// For each LOD :
	// Generates an unified list of bones, in LOD order (if possible)
	// Returns true if the unified list could be created, false otherwise
	static bool GenerateOrderedBoneList(const USkeletalMesh* SkeletalMesh
		, TArray<FGenerationLODData>& GenerationLODData
		, TArray<FBoneIndexType>& OrderedBoneList);

	/**
	 *	Utility for taking two arrays of bone indices, which must be strictly increasing, and finding the A - B.
	 *	That is - any items left in A, after removing B.
	 */
	static void DifferenceBoneIndexArrays(const TArray<FBoneIndexType>& A, const TArray<FBoneIndexType>& B, TArray<FBoneIndexType>& Output);

	// Checks if all sockets of a skeletal mesh are set to always animate, as it is a requirement for generating a single reference pose,
	// where the local space pose and the component space pose use the same bone indexes
	static bool CheckSkeletalAllMeshSocketsAlwaysAnimate(const USkeletalMesh* SkeletalMesh);

	// Converts AnimBP pose to AnimNext Pose
	// This function expects both poses to have the same LOD (number of bones and indexes)
	// The target pose should be assigned to the correct reference pose prior to this call
	static void RemapPose(const FPoseContext& SourcePose, FLODPose& TargetPose);

	// Converts AnimNext pose to AnimBP Pose
	// This function expects both poses to have the same LOD (number of bones and indexes)
	// The target pose should be assigned to the correct reference pose prior to this call
	static void RemapPose(const FLODPose& SourcePose, FPoseContext& TargetPose);

	// Converts AnimNext pose to local space transform array
	// This function expects the output pose to have the same or a greater number of bones (as it may be being calculated
	// for a lower LOD)
	// The target pose should be assigned to the correct reference pose prior to this call, as transforms will not be filled
	// in by this call if they are not affected by the current LOD.
	static void RemapPose(const FLODPose& SourcePose, TArrayView<FTransform> TargetTransforms);

	// Converts a local space to component space buffer given a number of required bones
	static void ConvertLocalSpaceToComponentSpace(TConstArrayView<FBoneIndexType> InParentIndices, TConstArrayView<FTransform> InBoneSpaceTransforms, TConstArrayView<FBoneIndexType> InRequiredBoneIndices, TArrayView<FTransform> OutComponentSpaceTransforms);
};

} // namespace UE::AnimNext
