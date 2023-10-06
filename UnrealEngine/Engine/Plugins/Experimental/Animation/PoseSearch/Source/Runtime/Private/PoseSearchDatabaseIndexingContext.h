// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "PoseSearch/PoseSearchAssetIndexer.h"
#include "PoseSearch/PoseSearchAssetSampler.h"

class UPoseSearchDatabase;
namespace UE::DerivedData { class FRequestOwner; }

namespace UE::PoseSearch
{
struct FSearchIndexBase;

struct FAssetSamplingContext
{
	// Time delta used for computing pose derivatives
	static constexpr float FiniteDelta = 1 / 60.0f;

	// Mirror data table pointer copied from Schema for convenience
	TObjectPtr<const UMirrorDataTable> MirrorDataTable;

	// Compact pose format of Mirror Bone Map
	TCustomBoneIndexArray<FCompactPoseBoneIndex, FCompactPoseBoneIndex> CompactPoseMirrorBones;

	// Pre-calculated component space rotations of reference pose, which allows mirror to work with any joint orientation
	// Only initialized and used when a mirroring table is specified
	TCustomBoneIndexArray<FQuat, FCompactPoseBoneIndex> ComponentSpaceRefRotations;

	void Init(const UMirrorDataTable* InMirrorDataTable, const FBoneContainer& BoneContainer);
	FTransform MirrorTransform(const FTransform& Transform) const;
};

struct FDatabaseIndexingContext
{
	bool IndexDatabase(FSearchIndexBase& SearchIndexBase, const UPoseSearchDatabase& Database, UE::DerivedData::FRequestOwner& Owner);

private:
	FAssetSamplingContext SamplingContext;
	TArray<FAnimationAssetSampler> Samplers;
	TArray<FAssetIndexer> Indexers;
};

} // namespace UE::PoseSearch

#endif // WITH_EDITOR