// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearch/PoseSearchFeatureChannel.h"
#include "PoseSearchFeatureChannel_SamplingTime.generated.h"

// this channel is mainly for debug purposes to augment the features data with the sampling time (default weight is set to zero to be irrelevant during searches)
UCLASS(EditInlineNew, meta = (DisplayName = "Sampling Time Channel"), CollapseCategories)
class POSESEARCH_API UPoseSearchFeatureChannel_SamplingTime : public UPoseSearchFeatureChannel
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	float Weight = 0.f;

	UPROPERTY(EditAnywhere, Category = "Settings")
	float TimeToMatch = 0.f;

	// UPoseSearchFeatureChannel interface
	virtual bool Finalize(UPoseSearchSchema* Schema) override;
	virtual void BuildQuery(UE::PoseSearch::FSearchContext& SearchContext) const override;

#if WITH_EDITOR
	virtual void FillWeights(TArrayView<float> Weights) const override;
	virtual bool IndexAsset(UE::PoseSearch::FAssetIndexer& Indexer) const override;
	virtual UE::PoseSearch::TLabelBuilder& GetLabel(UE::PoseSearch::TLabelBuilder& LabelBuilder, UE::PoseSearch::ELabelFormat LabelFormat = UE::PoseSearch::ELabelFormat::Full_Horizontal) const override;
#endif
};
