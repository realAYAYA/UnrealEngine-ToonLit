// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearch/PoseSearchFeatureChannel.h"
#include "BoneContainer.h"
#include "PoseSearchFeatureChannel_Group.generated.h"

// Feature channels interface
UCLASS(Abstract, BlueprintType, EditInlineNew)
class POSESEARCH_API UPoseSearchFeatureChannel_GroupBase : public UPoseSearchFeatureChannel
{
	GENERATED_BODY()

public:
	// UPoseSearchFeatureChannel interface
	virtual void Finalize(UPoseSearchSchema* Schema) override;
	virtual void BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, UE::PoseSearch::FFeatureVectorBuilder& InOutQuery) const override;
	virtual void AddDependentChannels(UPoseSearchSchema* Schema) const override; 

#if ENABLE_DRAW_DEBUG
	virtual void DebugDraw(const UE::PoseSearch::FDebugDrawParams& DrawParams, TConstArrayView<float> PoseVector) const override;
#endif // ENABLE_DRAW_DEBUG

#if WITH_EDITOR
	virtual void FillWeights(TArrayView<float> Weights) const override;
	virtual void IndexAsset(UE::PoseSearch::FAssetIndexer& Indexer) const override;
#endif //WITH_EDITOR

	// IPoseSearchFilter interface
	virtual bool IsFilterActive() const override;
	virtual bool IsFilterValid(TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues, int32 PoseIdx, const UE::PoseSearch::FPoseMetadata& Metadata) const override;
};

UCLASS(BlueprintType, EditInlineNew, meta = (DisplayName = "Group Channel"), CollapseCategories)
class POSESEARCH_API UPoseSearchFeatureChannel_Group : public UPoseSearchFeatureChannel_GroupBase
{
	GENERATED_BODY()

	virtual TArrayView<TObjectPtr<UPoseSearchFeatureChannel>> GetSubChannels() override { return SubChannels; }
	virtual TConstArrayView<TObjectPtr<UPoseSearchFeatureChannel>> GetSubChannels() const override { return SubChannels; }

#if WITH_EDITOR
	virtual FString GetLabel() const override;
#endif

public:
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "SubChannels")
	TArray<TObjectPtr<UPoseSearchFeatureChannel>> SubChannels;
};