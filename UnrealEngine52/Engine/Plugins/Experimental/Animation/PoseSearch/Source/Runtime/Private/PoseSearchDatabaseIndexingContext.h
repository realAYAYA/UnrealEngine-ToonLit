// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "PoseSearch/PoseSearchAssetSampler.h"
#include "PoseSearchAssetIndexer.h"

struct FPoseSearchIndexBase;
class UPoseSearchDatabase;

namespace UE::PoseSearch
{

struct FDatabaseIndexingContext
{
	FPoseSearchIndexBase* SearchIndexBase = nullptr;

	FAssetSamplingContext SamplingContext;
	TArray<FSequenceBaseSampler> SequenceSamplers; // Composite and sequence samplers
	TArray<FBlendSpaceSampler> BlendSpaceSamplers;

	TArray<FAssetIndexer> Indexers;

	void Prepare(const UPoseSearchDatabase* Database);
	bool IndexAssets();
	void JoinIndex();
	float CalculateMinCostAddend() const;
};

} // namespace UE::PoseSearch

#endif // WITH_EDITOR