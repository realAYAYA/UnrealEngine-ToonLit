// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ReferencePose.h"
#include "BoneIndices.h"
#include "TransformArray.h"
#include "TransformArrayOperations.h"
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

struct FLODPose
{
	static constexpr int32 INVALID_LOD_LEVEL = -1;

	FTransformArrayView LocalTransformsView;
	const FReferencePose* RefPose = nullptr;
	int32 LODLevel = INVALID_LOD_LEVEL;
	ELODPoseFlags Flags = ELODPoseFlags::None;

	FLODPose() = default;

	void CopyFrom(const FLODPose& Other)
	{
		check(LODLevel == Other.LODLevel);
		check(RefPose == Other.RefPose);
		check(LocalTransformsView.Num() == Other.LocalTransformsView.Num());

		// Copy over the flags and transforms from our source
		Flags = Other.Flags;
		CopyTransforms(LocalTransformsView, Other.LocalTransformsView);
	}

	void SetRefPose(bool bAdditive = false)
	{
		const int32 NumTransforms = LocalTransformsView.Num();

		if (NumTransforms > 0)
		{
			if (bAdditive)
			{
				SetIdentity(bAdditive);
			}
			else
			{
				check(RefPose != nullptr);
				CopyTransforms(LocalTransformsView, RefPose->ReferenceLocalTransforms.GetConstView(), 0, NumTransforms);
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
		UE::AnimNext::SetIdentity(LocalTransformsView, bAdditive);
	}

	int32 GetNumBones() const
	{
		return RefPose != nullptr ? RefPose->GetNumBonesForLOD(LODLevel) : 0;
	}

	const TArrayView<const FBoneIndexType> GetLODBoneIndexToMeshBoneIndexMap() const
	{
		if (LODLevel != INVALID_LOD_LEVEL && RefPose != nullptr)
		{
			return RefPose->GetLODBoneIndexToMeshBoneIndexMap(LODLevel);
		}
		else
		{
			return TArrayView<const FBoneIndexType>();
		}
	}

	const TArrayView<const FBoneIndexType> GetLODBoneIndexToSkeletonBoneIndexMap() const
	{
		if (LODLevel != INVALID_LOD_LEVEL && RefPose != nullptr)
		{
			return RefPose->GetLODBoneIndexToSkeletonBoneIndexMap(LODLevel);
		}
		else
		{
			return TArrayView<const FBoneIndexType>();
		}
	}

	const TArrayView<const FBoneIndexType> GetSkeletonBoneIndexToLODBoneIndexMap() const
	{
		if (LODLevel != INVALID_LOD_LEVEL && RefPose != nullptr)
		{
			return RefPose->GetSkeletonBoneIndexToLODBoneIndexMap(LODLevel);
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

	bool IsValid() const
	{
		return RefPose != nullptr;
	}

	bool IsAdditive() const
	{
		return (Flags & ELODPoseFlags::Additive) != ELODPoseFlags::None;
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

template <typename AllocatorType>
struct TLODPose : public FLODPose
{
	TTransformArray<AllocatorType> LocalTransforms;

	TLODPose() = default;

	TLODPose(const FReferencePose& InRefPose, int32 InLODLevel, bool bSetRefPose = true, bool bAdditive = false)
	{
		PrepareForLOD(InRefPose, InLODLevel, bSetRefPose, bAdditive);
	}

	bool ShouldPrepareForLOD(const FReferencePose& InRefPose, int32 InLODLevel, bool bAdditive = false) const
	{
		return
			LODLevel != InLODLevel ||
			RefPose != &InRefPose ||
			bAdditive != EnumHasAnyFlags(Flags, ELODPoseFlags::Additive);
	}
	
	void PrepareForLOD(const FReferencePose& InRefPose, int32 InLODLevel, bool bSetRefPose = true, bool bAdditive = false)
	{
		LODLevel = InLODLevel;
		RefPose = &InRefPose;

		const int32 NumTransforms = InRefPose.GetNumBonesForLOD(InLODLevel);
		LocalTransforms.SetNumUninitialized(NumTransforms);
		LocalTransformsView = LocalTransforms.GetView();

		Flags = (ELODPoseFlags)(bAdditive ? (Flags | ELODPoseFlags::Additive) : Flags & ELODPoseFlags::Additive);

		if (bSetRefPose && NumTransforms > 0)
		{
			SetRefPose(bAdditive);
		}
	}
};

using FLODPoseHeap = TLODPose<FDefaultAllocator>;
using FLODPoseStack = TLODPose<FAnimStackAllocator>;

}

// USTRUCT wrapper for LOD pose
USTRUCT()
struct FAnimNextLODPose
#if CPP
	: UE::AnimNext::FLODPoseHeap
#endif
{
	GENERATED_BODY()
};