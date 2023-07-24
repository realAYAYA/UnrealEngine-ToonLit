// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"
#include "PoseSearch/PoseSearchSearchableAsset.h"
#include "PoseSearchDatabaseSet.generated.h"

UENUM()
enum class EPoseSearchPostSearchStatus : uint8
{
	// Continue looking for results 
	Continue,

	// Halt and return the best result
	Stop
};

USTRUCT(BlueprintType, Category = "Animation|Pose Search")
struct POSESEARCH_API FPoseSearchDatabaseSetEntry
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Settings)
	TObjectPtr<UPoseSearchSearchableAsset> Searchable;

	UPROPERTY(EditAnywhere, Category = Settings)
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, Category = Settings)
	EPoseSearchPostSearchStatus PostSearchStatus = EPoseSearchPostSearchStatus::Continue;
};

/** A data asset which holds a collection searchable assets. */
UCLASS(BlueprintType, Category = "Animation|Pose Search", Experimental)
class POSESEARCH_API UPoseSearchDatabaseSet : public UPoseSearchSearchableAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Settings)
	TArray<FPoseSearchDatabaseSetEntry> AssetsToSearch;

	// if there's a valid continuing pose and bEvaluateContinuingPoseFirst is true, the continuing pose will be evaluated as first search,
	// otherwise it'll be evaluated with the related database: if the database is not active the continuing pose evaluation will be skipped
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bEvaluateContinuingPoseFirst = true;

public:
	virtual UE::PoseSearch::FSearchResult Search(UE::PoseSearch::FSearchContext& SearchContext) const override;
};
