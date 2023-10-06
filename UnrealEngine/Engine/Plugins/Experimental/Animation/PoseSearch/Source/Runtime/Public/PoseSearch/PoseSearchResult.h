// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearch/PoseSearchCost.h"
#include "PoseSearch/PoseSearchDefines.h"

class UPoseSearchDatabase;
class UPoseSearchSchema;

namespace UE::PoseSearch
{
struct FSearchIndexAsset;

/**
* float buffer of features according to a UPoseSearchSchema layout.
* FFeatureVectorBuilder is used to build search queries at runtime and for adding samples during search index construction.
*/
struct FFeatureVectorBuilder
{
public:
	explicit FFeatureVectorBuilder(const UPoseSearchSchema* Schema);
	const UPoseSearchSchema* GetSchema() const { return Schema.Get(); }

	TArrayView<float> EditValues() { return Values; }
	TConstArrayView<float> GetValues() const { return Values; }

private:
	TStackAlignedArray<float> Values;
	TObjectPtr<const UPoseSearchSchema> Schema;
};
	
struct FSearchResult
{
	// best cost of the currently selected PoseIdx (it could be equal to ContinuingPoseCost)
	FPoseSearchCost PoseCost;
	int32 PoseIdx = INDEX_NONE;

	TObjectPtr<const UPoseSearchDatabase> Database;

	float AssetTime = 0.0f;

#if UE_POSE_SEARCH_TRACE_ENABLED
	FPoseSearchCost BruteForcePoseCost;
	int32 BestPosePos = 0;
#endif // UE_POSE_SEARCH_TRACE_ENABLED

	// Attempts to set the internal state to match the provided asset time including updating the internal DbPoseIdx. 
	// If the provided asset time is out of bounds for the currently playing asset then this function will reset the 
	// state back to the default state.
	void Update(float NewAssetTime);

	bool IsValid() const;

	void Reset();

	const FSearchIndexAsset* GetSearchIndexAsset(bool bMandatory = false) const;
	
	bool CanAdvance(float DeltaTime) const;
};

} // namespace UE::PoseSearch