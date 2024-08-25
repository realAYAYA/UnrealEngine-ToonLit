// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearch/PoseSearchFeatureChannel.h"
#include "PoseSearchFeatureChannel_TimeToEvent.generated.h"

UCLASS(EditInlineNew, Blueprintable, meta = (DisplayName = "Time To Event Channel"), CollapseCategories)
class POSESEARCH_API UPoseSearchFeatureChannel_TimeToEvent : public UPoseSearchFeatureChannel
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	float Weight = 0.f;

	UPROPERTY(EditAnywhere, Category = "Settings")
	int32 SamplingAttributeId = 0;

	UFUNCTION(BlueprintPure, BlueprintImplementableEvent, meta=(BlueprintThreadSafe, DisplayName = "Get Time To Event"), Category = "Settings")
	float BP_GetTimeToEvent(const UAnimInstance* AnimInstance) const;

	bool bUseBlueprintQueryOverride = false;

	UPoseSearchFeatureChannel_TimeToEvent();

	// UPoseSearchFeatureChannel interface
	virtual bool Finalize(UPoseSearchSchema* Schema) override;
	virtual void BuildQuery(UE::PoseSearch::FSearchContext& SearchContext) const override;

#if WITH_EDITOR
	virtual void FillWeights(TArrayView<float> Weights) const override;
	virtual bool IndexAsset(UE::PoseSearch::FAssetIndexer& Indexer) const override;
	virtual UE::PoseSearch::TLabelBuilder& GetLabel(UE::PoseSearch::TLabelBuilder& LabelBuilder, UE::PoseSearch::ELabelFormat LabelFormat = UE::PoseSearch::ELabelFormat::Full_Horizontal) const override;
#endif
};
