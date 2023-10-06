// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ReferencePose.h"
#include "BoneIndices.h"
#include "TransformArray.h"
#include "LODPose.generated.h"

namespace UE::AnimNext
{

enum class ELODPoseFlags : uint8
{
	None				= 0,
	Additive			= 1 << 0,
	DisableRetargeting	= 1 << 1,
	UseRawData			= 1 << 2,
	UseSourceData		= 1 << 3
};

ENUM_CLASS_FLAGS(ELODPoseFlags);

template <typename AllocatorType>
struct TLODPose
{
	static constexpr int32 INVALID_LOD_LEVEL = -1;

	TTransformArray<AllocatorType> LocalTransforms;
	const FReferencePose* RefPose = nullptr;
	int32 LODLevel = INVALID_LOD_LEVEL;
	ELODPoseFlags Flags = ELODPoseFlags::None;

	TLODPose() = default;

	TLODPose(const FReferencePose& InRefPose, int32 InLODLevel, bool bSetRefPose = true, bool bAdditive = false)
	{
		PrepareForLOD(InRefPose, InLODLevel, bSetRefPose, bAdditive);
	}

	void PrepareForLOD(const FReferencePose& InRefPose, int32 InLODLevel, bool bSetRefPose = true, bool bAdditive = false)
	{
		LODLevel = InLODLevel;
		RefPose = &InRefPose;

		const int32 NumTransforms = InRefPose.GetNumBonesForLOD(InLODLevel);
		LocalTransforms.SetNum(NumTransforms);
		Flags = (ELODPoseFlags)(bAdditive ? (Flags | ELODPoseFlags::Additive) : Flags & ELODPoseFlags::Additive);

		if (bSetRefPose && NumTransforms > 0)
		{
			SetRefPose(bAdditive);
		}
	}

	void SetRefPose(bool bAdditive = false)
	{
		const int32 NumTransforms = LocalTransforms.Num();

		if (NumTransforms > 0)
		{
			if (bAdditive)
			{
				SetIdentity(bAdditive);
			}
			else
			{
				check(RefPose != nullptr);
				LocalTransforms.CopyTransforms(RefPose->ReferenceLocalTransforms, 0, NumTransforms);
			}
		}

		Flags = (ELODPoseFlags)(bAdditive ? (Flags | ELODPoseFlags::Additive) : Flags & ELODPoseFlags::Additive);
	}

	const FReferencePose& GetRefPose() const
	{
		check(RefPose != nullptr);
		return *RefPose;
	}

	void SetIdentity(bool bAdditive = false)
	{
		LocalTransforms.SetIdentity(bAdditive);
	}

	int32 GetNumBones() const
	{
		return RefPose != nullptr ? RefPose->GetNumBonesForLOD(LODLevel) : 0;
	}

	const TArrayView<const FBoneIndexType> GetLODBoneIndexes() const
	{
		if (LODLevel != INVALID_LOD_LEVEL && RefPose != nullptr)
		{
			return RefPose->GetLODBoneIndexes(LODLevel);
		}
		else
		{
			return TArrayView<const FBoneIndexType>();
		}
	}

	const TArrayView<const FBoneIndexType> GetSkeletonToLODBoneIndexes() const
	{
		if (LODLevel != INVALID_LOD_LEVEL && RefPose != nullptr)
		{
			return RefPose->GetSkeletonToLODBoneIndexes(LODLevel);
		}
		else
		{
			return TArrayView<const FBoneIndexType>();
		}
	}

	const USkeleton* GetSkeletonAsset() const
	{
		return RefPose != nullptr ? RefPose->Skeleton.Get() : nullptr;
	}

	/** Disable Retargeting */
	void SetDisableRetargeting(bool bDisableRetargeting)
	{
		Flags = (ELODPoseFlags)(bDisableRetargeting ? (Flags | ELODPoseFlags::DisableRetargeting) : Flags & ELODPoseFlags::DisableRetargeting);
	}

	/** True if retargeting is disabled */
	bool GetDisableRetargeting() const
	{
		return (Flags & ELODPoseFlags::DisableRetargeting) != ELODPoseFlags::None;
	}

	/** Ignore compressed data and use RAW data instead, for debugging. */
	void SetUseRAWData(bool bUseRAWData)
	{
		Flags = (ELODPoseFlags)(bUseRAWData ? (Flags | ELODPoseFlags::UseRawData) : Flags & ELODPoseFlags::UseRawData);
	}

	/** True if we're requesting RAW data instead of compressed data. For debugging. */
	bool ShouldUseRawData() const
	{
		return (Flags & ELODPoseFlags::UseRawData) != ELODPoseFlags::None;
	}

	/** Use Source data instead.*/
	void SetUseSourceData(bool bUseSourceData)
	{
		Flags = (ELODPoseFlags)(bUseSourceData ? (Flags | ELODPoseFlags::UseSourceData) : Flags & ELODPoseFlags::UseSourceData);
	}

	/** True if we're requesting Source data instead of RawAnimationData. For debugging. */
	bool ShouldUseSourceData() const
	{
		return (Flags & ELODPoseFlags::UseSourceData) != ELODPoseFlags::None;
	}

};

using FLODPoseHeap = TLODPose<FDefaultAllocator>;
using FLODPoseStack = TLODPose<FAnimStackAllocator>;

using FLODPose = FLODPoseHeap;

}

// USTRUCT wrapper for LOD pose
USTRUCT()
struct FAnimNextLODPose
#if CPP
	: UE::AnimNext::FLODPose
#endif
{
	GENERATED_BODY()
};