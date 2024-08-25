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
	UPROPERTY(EditAnywhere, Category = "Settings")
	FName SampleRole = UE::PoseSearch::DefaultRole;

	// UPoseSearchFeatureChannel interface
	virtual bool Finalize(UPoseSearchSchema* Schema) override;
	virtual void BuildQuery(UE::PoseSearch::FSearchContext& SearchContext) const override;
	virtual void AddDependentChannels(UPoseSearchSchema* Schema) const override; 

#if ENABLE_DRAW_DEBUG
	virtual void DebugDraw(const UE::PoseSearch::FDebugDrawParams& DrawParams, TConstArrayView<float> PoseVector) const override;
#endif // ENABLE_DRAW_DEBUG

#if WITH_EDITOR
	virtual void FillWeights(TArrayView<float> Weights) const override;
	virtual bool IndexAsset(UE::PoseSearch::FAssetIndexer& Indexer) const override;
	virtual const UE::PoseSearch::FRole GetDefaultRole() const override { return SampleRole; }
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
	virtual UE::PoseSearch::TLabelBuilder& GetLabel(UE::PoseSearch::TLabelBuilder& LabelBuilder, UE::PoseSearch::ELabelFormat LabelFormat = UE::PoseSearch::ELabelFormat::Full_Horizontal) const override;
#endif

public:
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "SubChannels")
	TArray<TObjectPtr<UPoseSearchFeatureChannel>> SubChannels;
};