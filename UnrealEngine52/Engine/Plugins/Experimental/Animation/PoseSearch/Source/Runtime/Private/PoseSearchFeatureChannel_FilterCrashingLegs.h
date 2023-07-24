// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearch/PoseSearchFeatureChannel.h"
#include "BoneContainer.h"
#include "PoseSearchFeatureChannel_FilterCrashingLegs.generated.h"

// the idea is to calculate the angle between the direction from LeftThigh position to RightThigh position and the direction from LeftFoot position to RightFoot position, and divide it by PI to have values in range [-1,1]
// the number (called 'CrashingLegsValue' calculated in ComputeCrashingLegsValue) is gonna be
// 0 if the feet are aligned with the thighs (for example in an idle standing position)
// 0.5 if the right foot is exactly in front of the left foot (for example when a character is running  following a line)
// -0.5 if the left foot is exactly in front of the right foot
// close to 1 or -1 if the feet (and so the legs) are completely crossed
// at runtime we'll match the CrashingLegsValue and also filter by discarding pose candidates that don't respect the 'AllowedTolerance' between query and database values (happening in IsPoseValid)
UCLASS(BlueprintType, EditInlineNew, meta = (DisplayName = "CrashingLegs Channel"), CollapseCategories)
class POSESEARCH_API UPoseSearchFeatureChannel_FilterCrashingLegs : public UPoseSearchFeatureChannel
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference LeftThigh;
	
	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference RightThigh;

	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference LeftFoot;
	
	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference RightFoot;

	UPROPERTY(EditAnywhere, Category = "Settings")
	float Weight = 0.2f;

	UPROPERTY()
	int8 LeftThighIdx;

	UPROPERTY()
	int8 RightThighIdx;

	UPROPERTY()
	int8 LeftFootIdx;

	UPROPERTY()
	int8 RightFootIdx;

	UPROPERTY(EditAnywhere, Category = "Settings")
	EInputQueryPose InputQueryPose = EInputQueryPose::UseContinuingPose;

	UPROPERTY(EditAnywhere, Category = "Settings")
	float AllowedTolerance = 0.3f;

	// UPoseSearchFeatureChannel interface
	virtual void Finalize(UPoseSearchSchema* Schema) override;
	virtual void FillWeights(TArray<float>& Weights) const override;
	virtual void IndexAsset(UE::PoseSearch::IAssetIndexer& Indexer, TArrayView<float> FeatureVectorTable) const override;
	virtual void BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, FPoseSearchFeatureVectorBuilder& InOutQuery) const override;
	virtual void DebugDraw(const UE::PoseSearch::FDebugDrawParams& DrawParams, TConstArrayView<float> PoseVector) const override;
#if WITH_EDITOR
	virtual FString GetLabel() const;
#endif

	// IPoseFilter interface
	virtual bool IsPoseFilterActive() const override;
	virtual bool IsPoseValid(TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues, int32 PoseIdx, const FPoseSearchPoseMetadata& Metadata) const override;
};
