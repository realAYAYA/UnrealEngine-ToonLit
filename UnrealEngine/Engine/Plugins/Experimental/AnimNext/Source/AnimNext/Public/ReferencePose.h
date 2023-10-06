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
	TTransformArray<AllocatorType> ReferenceLocalTransforms;
	TArray<TArray<FBoneIndexType, AllocatorType>, AllocatorType> LODBoneIndexes;
	TArray<TArray<FBoneIndexType, AllocatorType>, AllocatorType> SkeletonToLODBoneIndexes;
	TArray<int32, AllocatorType> LODNumBones;

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
		, const TArray<TArray<FBoneIndexType>>& InLODBoneIndexes
		, const TArray<TArray<FBoneIndexType>>& InSkeletonToLODBoneIndexes
		, const TArray<int32, AllocatorType>& InLODNumBones
		, bool bFastPath = false)
	{
		const int32 NumBonesLOD0 = (InLODNumBones.Num()) > 0 ? InLODNumBones[0] : 0;

		ReferenceLocalTransforms.SetNum(NumBonesLOD0);
		LODBoneIndexes = InLODBoneIndexes;
		SkeletonToLODBoneIndexes = InSkeletonToLODBoneIndexes;
		LODNumBones = InLODNumBones;
		
		const TArray<FTransform>& RefBonePose = RefSkeleton.GetRefBonePose();
		const TArray<FBoneIndexType>& InBoneIndexes = InLODBoneIndexes[0]; // Fill the transforms with the LOD0 indexes

		for (int32 i = 0; i < NumBonesLOD0; ++i)
		{
			ReferenceLocalTransforms[i] = RefBonePose[InBoneIndexes[i]]; // TODO : For SoA this is un-optimal, as we are using a TransformAdapter. Evaluate using a specific SoA iterator
		}

		GenerationFlags = bFastPath ? EReferencePoseGenerationFlags::FastPath : EReferencePoseGenerationFlags::None;
	}

	const TArrayView<const FBoneIndexType> GetLODBoneIndexes(int32 LODLevel) const
	{
		TArrayView<const FBoneIndexType> ArrayView;

		if (LODLevel >= 0 && (IsFastPath() || LODLevel < LODBoneIndexes.Num()))
		{
			const int32 NumBonesForLOD = GetNumBonesForLOD(LODLevel);

			if (IsFastPath())
			{
				ArrayView = MakeArrayView(LODBoneIndexes[0].GetData(), NumBonesForLOD);
			}
			else
			{
				ArrayView = MakeArrayView(LODBoneIndexes[LODLevel].GetData(), NumBonesForLOD);
			}
		}

		return ArrayView;
	}

	const TArrayView<const FBoneIndexType> GetSkeletonToLODBoneIndexes(int32 LODLevel) const
	{
		TArrayView<const FBoneIndexType> ArrayView;

		if (LODLevel >= 0 && (IsFastPath() || LODLevel < SkeletonToLODBoneIndexes.Num()))
		{
			const int32 NumBonesForLOD = GetNumBonesForLOD(LODLevel);

			if (IsFastPath())
			{
				return MakeArrayView(SkeletonToLODBoneIndexes[0].GetData(), NumBonesForLOD);
			}
			else
			{
				return MakeArrayView(SkeletonToLODBoneIndexes[LODLevel].GetData(), NumBonesForLOD);
			}
		}

		return ArrayView;
	}

	int32 GetSkeletonBoneIndexFromLODBoneIndex(int32 LODBoneIndex) const
	{
		const int32 NumBonesLOD0 = LODBoneIndexes[0].Num();
		check(LODBoneIndex < NumBonesLOD0);
		return LODBoneIndexes[0][LODBoneIndex];
	}

	int32 GetLODBoneIndexFromSkeletonBoneIndex(int32 SkeletionBoneIndex) const
	{
		const int32 NumBonesLOD0 = SkeletonToLODBoneIndexes[0].Num();
		check(SkeletionBoneIndex < NumBonesLOD0);
		return SkeletonToLODBoneIndexes[0][SkeletionBoneIndex];
	}

	FTransform GetRefPoseTransform(int32 LODBoneIndex) const
	{
		const int32 NumBonesLOD0 = LODBoneIndexes[0].Num();
		check(LODBoneIndex < NumBonesLOD0);

		return ReferenceLocalTransforms[LODBoneIndex];
	}

	const FQuat& GetRefPoseRotation(int32 LODBoneIndex) const
	{
		const int32 NumBonesLOD0 = LODBoneIndexes[0].Num();
		check(LODBoneIndex < NumBonesLOD0);

		return ReferenceLocalTransforms[LODBoneIndex].GetRotation();
	}

	const FVector& GetRefPoseTranslation(int32 LODBoneIndex) const
	{
		const int32 NumBonesLOD0 = LODBoneIndexes[0].Num();
		check(LODBoneIndex < NumBonesLOD0);

		return ReferenceLocalTransforms[LODBoneIndex].GetTranslation();
	}

	const FVector& GetRefPoseScale3D(int32 LODBoneIndex) const
	{
		const int32 NumBonesLOD0 = LODBoneIndexes[0].Num();
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
