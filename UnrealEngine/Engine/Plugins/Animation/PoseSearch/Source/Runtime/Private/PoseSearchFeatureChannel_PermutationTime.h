// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearch/PoseSearchFeatureChannel.h"
#include "PoseSearchFeatureChannel_PermutationTime.generated.h"

UCLASS(Experimental, EditInlineNew, meta = (DisplayName = "Permutation Time Channel"), CollapseCategories)
class POSESEARCH_API UPoseSearchFeatureChannel_PermutationTime : public UPoseSearchFeatureChannel
{
	GENERATED_BODY()

public:
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Settings")
	float Weight = 1.f;
#endif // WITH_EDITORONLY_DATA

	// UPoseSearchFeatureChannel interface
	virtual bool Finalize(UPoseSearchSchema* Schema) override;
	virtual void BuildQuery(UE::PoseSearch::FSearchContext& SearchContext) const override;

#if WITH_EDITOR
	virtual void FillWeights(TArrayView<float> Weights) const override;
	virtual bool IndexAsset(UE::PoseSearch::FAssetIndexer& Indexer) const override;
	virtual UE::PoseSearch::TLabelBuilder& GetLabel(UE::PoseSearch::TLabelBuilder& LabelBuilder, UE::PoseSearch::ELabelFormat LabelFormat = UE::PoseSearch::ELabelFormat::Full_Horizontal) const override;
#endif

	float GetPermutationTime(TConstArrayView<float> PoseVector) const;

	static void FindOrAddToSchema(UPoseSearchSchema* Schema);
};
