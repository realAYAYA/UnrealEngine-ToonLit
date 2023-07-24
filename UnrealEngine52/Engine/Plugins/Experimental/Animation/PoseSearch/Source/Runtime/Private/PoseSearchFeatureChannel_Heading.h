// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearch/PoseSearchFeatureChannel.h"
#include "BoneContainer.h"
#include "PoseSearchFeatureChannel_Heading.generated.h"

UENUM(BlueprintType)
enum class EHeadingAxis : uint8
{
	X,
	Y,
	Z,

	Num UMETA(Hidden),
	Invalid = Num UMETA(Hidden)
};

UCLASS(BlueprintType, EditInlineNew, meta = (DisplayName = "Heading Channel"), CollapseCategories)
class POSESEARCH_API UPoseSearchFeatureChannel_Heading : public UPoseSearchFeatureChannel
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference Bone;

	UPROPERTY(EditAnywhere, Category = "Settings")
	float Weight = 1.f;

	UPROPERTY(EditAnywhere, Category = "Settings")
	float SampleTimeOffset = 0.f;

	UPROPERTY(EditAnywhere, Category = "Settings")
	EHeadingAxis HeadingAxis = EHeadingAxis::X;	

	UPROPERTY()
	int8 SchemaBoneIdx = 0;

	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ExcludeFromHash))
	int32 ColorPresetIndex = 0;

	UPROPERTY(EditAnywhere, Category = "Settings")
	EInputQueryPose InputQueryPose = EInputQueryPose::UseContinuingPose;

	// UPoseSearchFeatureChannel interface
	virtual void Finalize(UPoseSearchSchema* Schema) override;
	virtual void FillWeights(TArray<float>& Weights) const override;
	virtual void IndexAsset(UE::PoseSearch::IAssetIndexer& Indexer, TArrayView<float> FeatureVectorTable) const override;
	virtual void BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, FPoseSearchFeatureVectorBuilder& InOutQuery) const override;
	virtual void DebugDraw(const UE::PoseSearch::FDebugDrawParams& DrawParams, TConstArrayView<float> PoseVector) const override;
#if WITH_EDITOR
	virtual FString GetLabel() const;
#endif

	FVector GetAxis(const FQuat& Rotation) const;
};