// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "PoseSearch/PoseSearchResult.h"
#include "PoseSearchSearchableAsset.generated.h"

namespace UE::PoseSearch
{
	struct FSearchContext;
} // namespace UE::PoseSearch

/** A data asset for indexing a collection of animation sequences. */
UCLASS(Abstract, BlueprintType, Experimental)
class POSESEARCH_API UPoseSearchSearchableAsset : public UDataAsset
{
	GENERATED_BODY()
public:

	virtual UE::PoseSearch::FSearchResult Search(UE::PoseSearch::FSearchContext& SearchContext) const PURE_VIRTUAL(UPoseSearchSearchableAsset::Search, return UE::PoseSearch::FSearchResult(););
};