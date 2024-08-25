// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ContainersFwd.h"
#include "UObject/WeakObjectPtr.h"
#include "DataRegistry.h"
#include <algorithm>
#include "Animation/AnimTypes.h"
#include "ReferenceSkeleton.h"
#include "BoneIndices.h"
#include "TransformArray.h"
#include "ReferencePose.generated.h"

class USkeletalMesh;
class USkeleton;

namespace UE::AnimNext
{

enum class EReferencePoseGenerationFlags : uint8
{
	None = 0,
	FastPath = 1 << 0
};

ENUM_CLASS_FLAGS(EReferencePoseGenerationFlags);

template <typename AllocatorType>
struct TReferencePose
{
	// Transform array of our bind pose sorted by LOD, allows us to truncate the array for a specific LOD
	// Higher LOD come first
	TTransformArray<AllocatorType> ReferenceLocalTransforms;

	// A mapping of LOD sorted bone indices to skeletal mesh indices per LOD
	// Each list of bone indices is a mapping of: LODSortedBoneIndex -> SkeletalMeshBoneIndex
	// When fast path is enabled, we have a single LOD entry that we truncate to the number of bones for each LOD
	TArray<TArray<FBoneIndexType, AllocatorType>, AllocatorType> LODBoneIndexToMeshBoneIndexMapPerLOD;

	// A mapping of LOD sorted bone indices to skeleton indices per LOD
	// Each list of bone indices is a mapping of: LODSortedBoneIndex -> SkeletonBoneIndex
	// When fast path is enabled, we have a single LOD entry that we truncate to the number of bones for each LOD
	TArray<TArray<FBoneIndexType, AllocatorType>, AllocatorType> LODBoneIndexToSkeletonBoneIndexMapPerLOD;

	// List of skeleton bone indices for each LOD
	// Each list of skeleton bone indices is a mapping of: SkeletonBoneIndex -> LODSortedBoneIndex
	// When fast path is enabled, we have a single LOD entry that we truncate to the number of bones for each LOD
	TArray<TArray<FBoneIndexType, AllocatorType>, AllocatorType> SkeletonBoneIndexToLODBoneIndexMapPerLOD;

	// Number of bones for each LOD
	TArray<int32, AllocatorType> LODNumBones;

	// Mapping of mesh bone indices to mesh parent indices for each bone
	TArray<FBoneIndexType, AllocatorType> ParentIndices;

	TWeakObjectPtr<const USkeletalMesh> SkeletalMesh = nullptr;
	TWeakObjectPtr<const USkeleton> Skeleton = nullptr;
	EReferencePoseGenerationFlags GenerationFlags = EReferencePoseGenerationFlags::None;

	TReferencePose() = default;

	bool IsValid() const
	{
		return ReferenceLocalTransforms.Num() > 0;
	}

	int32 GetNumBonesForLOD(int32 LODLevel) const
	{
		const int32 NumLODS = LODNumBones.Num();

		return (LODLevel < NumLODS) ? LODNumBones[LODLevel] : (NumLODS > 0) ? LODNumBones[0] : 0;
	}

	bool IsFastPath() const
	{
		return (GenerationFlags & EReferencePoseGenerationFlags::FastPath) != EReferencePoseGenerationFlags::None;
	}

	void Initialize(const FReferenceSkeleton& RefSkeleton
		, const TArray<TArray<FBoneIndexType>>& InLODBoneIndexToMeshBoneIndexMapPerLOD
		, const TArray<TArray<FBoneIndexType>>& InLODBoneIndexToSkeletonBoneIndexMapPerLOD
		, const TArray<TArray<FBoneIndexType>>& InSkeletonBoneIndexToLODBoneIndexMapPerLOD
		, const TArray<int32, AllocatorType>& InLODNumBones
		, bool bFastPath = false)
	{
		const int32 NumBonesLOD0 = !InLODNumBones.IsEmpty() ? InLODNumBones[0] : 0;
		const int32 NumBonesMesh = RefSkeleton.GetRefBoneInfo().Num();
		
		ParentIndices.SetNum(NumBonesMesh);
		ReferenceLocalTransforms.SetNum(NumBonesLOD0);
		LODBoneIndexToMeshBoneIndexMapPerLOD = InLODBoneIndexToMeshBoneIndexMapPerLOD;
		LODBoneIndexToSkeletonBoneIndexMapPerLOD = InLODBoneIndexToSkeletonBoneIndexMapPerLOD;
		SkeletonBoneIndexToLODBoneIndexMapPerLOD = InSkeletonBoneIndexToLODBoneIndexMapPerLOD;
		LODNumBones = InLODNumBones;

		const TArray<FTransform>& RefBonePose = RefSkeleton.GetRefBonePose();
		const TArray<FBoneIndexType>& BoneLODIndexToMeshIndexMap0 = InLODBoneIndexToMeshBoneIndexMapPerLOD[0]; // Fill the transforms with the LOD0 indexes
		const TArray<FMeshBoneInfo>& RefBoneInfo = RefSkeleton.GetRefBoneInfo();

		for (int32 LODBoneIndex = 0; LODBoneIndex < NumBonesLOD0; ++LODBoneIndex)
		{
			// TODO : For SoA this is un-optimal, as we are using a TransformAdapter. Evaluate using a specific SoA iterator
			ReferenceLocalTransforms[LODBoneIndex] = RefBonePose[BoneLODIndexToMeshIndexMap0[LODBoneIndex]];
		}

		for(int32 BoneIndex = 0; BoneIndex < NumBonesMesh; ++BoneIndex)
		{
			ParentIndices[BoneIndex] = RefBoneInfo[BoneIndex].ParentIndex;
		}

		GenerationFlags = bFastPath ? EReferencePoseGenerationFlags::FastPath : EReferencePoseGenerationFlags::None;
	}

	// Returns a list of LOD sorted skeletal mesh bone indices, a mapping of: LODSortedBoneIndex -> SkeletalMeshBoneIndex
	const TArrayView<const FBoneIndexType> GetLODBoneIndexToMeshBoneIndexMap(int32 LODLevel) const
	{
		TArrayView<const FBoneIndexType> ArrayView;

		if (LODLevel >= 0 && (IsFastPath() || LODLevel < LODBoneIndexToMeshBoneIndexMapPerLOD.Num()))
		{
			const int32 NumBonesForLOD = GetNumBonesForLOD(LODLevel);
			const int32 LODIndex = IsFastPath() ? 0 : LODLevel;

			ArrayView = MakeArrayView(LODBoneIndexToMeshBoneIndexMapPerLOD[LODIndex].GetData(), NumBonesForLOD);
		}

		return ArrayView;
	}

	// Returns a list of LOD sorted skeleton bone indices, a mapping of: LODSortedBoneIndex -> SkeletonBoneIndex
	const TArrayView<const FBoneIndexType> GetLODBoneIndexToSkeletonBoneIndexMap(int32 LODLevel) const
	{
		TArrayView<const FBoneIndexType> ArrayView;

		if (LODLevel >= 0 && (IsFastPath() || LODLevel < LODBoneIndexToSkeletonBoneIndexMapPerLOD.Num()))
		{
			const int32 NumBonesForLOD = GetNumBonesForLOD(LODLevel);
			const int32 LODIndex = IsFastPath() ? 0 : LODLevel;

			ArrayView = MakeArrayView(LODBoneIndexToSkeletonBoneIndexMapPerLOD[LODIndex].GetData(), NumBonesForLOD);
		}

		return ArrayView;
	}

	// Returns a list of LOD bone indices, a mapping of: SkeletonBoneIndex -> LODSortedBoneIndex
	const TArrayView<const FBoneIndexType> GetSkeletonBoneIndexToLODBoneIndexMap(int32 LODLevel) const
	{
		TArrayView<const FBoneIndexType> ArrayView;

		if (LODLevel >= 0 && (IsFastPath() || LODLevel < SkeletonBoneIndexToLODBoneIndexMapPerLOD.Num()))
		{
			const int32 NumBonesForLOD = GetNumBonesForLOD(LODLevel);
			const int32 LODIndex = IsFastPath() ? 0 : LODLevel;

			ArrayView = MakeArrayView(SkeletonBoneIndexToLODBoneIndexMapPerLOD[LODIndex].GetData(), NumBonesForLOD);
		}

		return ArrayView;
	}

	int32 GetMeshBoneIndexFromLODBoneIndex(int32 LODBoneIndex) const
	{
		const int32 NumBonesLOD0 = LODBoneIndexToMeshBoneIndexMapPerLOD[0].Num();
		check(LODBoneIndex < NumBonesLOD0);
		return LODBoneIndexToMeshBoneIndexMapPerLOD[0][LODBoneIndex];
	}

	// Returns a mapping of mesh bone indices to mesh parent indices for each bone
	TConstArrayView<FBoneIndexType> GetParentIndices() const
	{
		return ParentIndices;
	}

	int32 GetSkeletonBoneIndexFromLODBoneIndex(int32 LODBoneIndex) const
	{
		const int32 NumBonesLOD0 = LODBoneIndexToSkeletonBoneIndexMapPerLOD[0].Num();
		check(LODBoneIndex < NumBonesLOD0);
		return LODBoneIndexToSkeletonBoneIndexMapPerLOD[0][LODBoneIndex];
	}

	int32 GetLODBoneIndexFromSkeletonBoneIndex(int32 SkeletionBoneIndex) const
	{
		const int32 NumBonesLOD0 = SkeletonBoneIndexToLODBoneIndexMapPerLOD[0].Num();
		check(SkeletionBoneIndex < NumBonesLOD0);
		return SkeletonBoneIndexToLODBoneIndexMapPerLOD[0][SkeletionBoneIndex];
	}

	FTransform GetRefPoseTransform(int32 LODBoneIndex) const
	{
		const int32 NumBonesLOD0 = LODBoneIndexToMeshBoneIndexMapPerLOD[0].Num();
		check(LODBoneIndex < NumBonesLOD0);

		return ReferenceLocalTransforms[LODBoneIndex];
	}

	const FQuat GetRefPoseRotation(int32 LODBoneIndex) const
	{
		const int32 NumBonesLOD0 = LODBoneIndexToMeshBoneIndexMapPerLOD[0].Num();
		check(LODBoneIndex < NumBonesLOD0);

		return ReferenceLocalTransforms[LODBoneIndex].GetRotation();
	}

	const FVector GetRefPoseTranslation(int32 LODBoneIndex) const
	{
		const int32 NumBonesLOD0 = LODBoneIndexToMeshBoneIndexMapPerLOD[0].Num();
		check(LODBoneIndex < NumBonesLOD0);

		return ReferenceLocalTransforms[LODBoneIndex].GetTranslation();
	}

	const FVector GetRefPoseScale3D(int32 LODBoneIndex) const
	{
		const int32 NumBonesLOD0 = LODBoneIndexToMeshBoneIndexMapPerLOD[0].Num();
		check(LODBoneIndex < NumBonesLOD0);

		return ReferenceLocalTransforms[LODBoneIndex].GetScale3D();
	}
};

/**
* Hide the allocator usage
*/
using FReferencePose = TReferencePose<FDefaultAllocator>;

} // namespace UE::AnimNext

// USTRUCT wrapper for reference pose
USTRUCT()
struct FAnimNextReferencePose
#if CPP
	: UE::AnimNext::FReferencePose
#endif
{
	GENERATED_BODY()
};
