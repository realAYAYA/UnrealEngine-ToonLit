// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/MirrorDataTable.h"

class UMirrorDataTable;
struct FBoneContainer;
struct FCompactPose;

namespace UE::PoseSearch
{

struct POSESEARCH_API FMirrorDataCache
{
	FMirrorDataCache();
	FMirrorDataCache(const UMirrorDataTable* InMirrorDataTable, const FBoneContainer& BoneContainer);
	
	void Init(const UMirrorDataTable* InMirrorDataTable, const FBoneContainer& BoneContainer);
	void Reset();

	FTransform MirrorTransform(const FTransform& InTransform) const;
	void MirrorPose(FCompactPose& Pose) const;
	const UMirrorDataTable* GetMirrorDataTable() const { return MirrorDataTable.Get(); }

private:
	// Mirror data table pointer copied from Schema for convenience
	TWeakObjectPtr<const UMirrorDataTable> MirrorDataTable;

	// Compact pose format of Mirror Bone Map
	TCustomBoneIndexArray<FCompactPoseBoneIndex, FCompactPoseBoneIndex> CompactPoseMirrorBones;

	// Pre-calculated component space rotations of reference pose, which allows mirror to work with any joint orientation
	// Only initialized and used when a mirroring table is specified
	TCustomBoneIndexArray<FQuat, FCompactPoseBoneIndex> ComponentSpaceRefRotations;
};

} // namespace UE::PoseSearch

