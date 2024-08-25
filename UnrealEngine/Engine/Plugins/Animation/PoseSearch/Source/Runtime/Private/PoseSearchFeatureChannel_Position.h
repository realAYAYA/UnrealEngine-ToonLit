// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearch/PoseSearchFeatureChannel.h"
#include "BoneContainer.h"
#include "PoseSearchFeatureChannel_Position.generated.h"

UCLASS(EditInlineNew, Blueprintable, meta = (DisplayName = "Position Channel"), CollapseCategories)
class POSESEARCH_API UPoseSearchFeatureChannel_Position : public UPoseSearchFeatureChannel
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference Bone;

	UPROPERTY(EditAnywhere, Category = "Settings")
	FName SampleRole = UE::PoseSearch::DefaultRole;

	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference OriginBone;

	UPROPERTY(EditAnywhere, Category = "Settings")
	FName OriginRole = UE::PoseSearch::DefaultRole;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Settings")
	float Weight = 1.f;
#endif // WITH_EDITORONLY_DATA

	// if SamplingAttributeId >= 0, ALL the animations contained in the pose search database referencing the schema containing this channel are expected to have 
	// UAnimNotifyState_PoseSearchSamplingAttribute notify state with a matching SamplingAttributeId, and the UAnimNotifyState_PoseSearchSamplingAttribute properties
	// will be used as source of data instead of this channel "Bone". UAnimNotifyState_PoseSearchSamplingAttribute properties will be then converted into OriginBone space
	UPROPERTY(EditAnywhere, Category = "Settings")
	int32 SamplingAttributeId = INDEX_NONE;
	
	// the data relative to the sampling time associated to this channel will be offsetted by SampleTimeOffset seconds.
	// For example, if Bone is the head bone, and SampleTimeOffset is 0.5, this channel will try to match the future position of the character head bone 0.5 seconds ahead
	UPROPERTY(EditAnywhere, Category = "Settings")
	float SampleTimeOffset = 0.f;

	// the data relative to the sampling time associated to this channel origin (root / trajectory bone) will be offsetted by OriginTimeOffset seconds.
	// For example, if Bone is the head bone, SampleTimeOffset is 0.5, and OriginTimeOffset is 0.5, this channel will try to match 
	// the future position of the character head bone 0.5 seconds ahead, relative to the future root bone 0.5 seconds ahead
	UPROPERTY(EditAnywhere, Category = "Settings")
	float OriginTimeOffset = 0.f;

	// index referencing the associated bone in UPoseSearchSchema::BoneReferences
	UPROPERTY(Transient)
	int8 SchemaBoneIdx = 0;

	UPROPERTY(Transient)
	int8 SchemaOriginBoneIdx = 0;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ExcludeFromHash, DisplayPriority = 0))
	FLinearColor DebugColor = FLinearColor::Blue;
#endif // WITH_EDITORONLY_DATA

	UPROPERTY(EditAnywhere, Category = "Settings")
	EInputQueryPose InputQueryPose = EInputQueryPose::UseContinuingPose;

	UPROPERTY(EditAnywhere, Category = "Settings")
	EComponentStrippingVector ComponentStripping = EComponentStrippingVector::None;

	UPROPERTY(EditAnywhere, Category = "Settings")
	EPermutationTimeType PermutationTimeType = EPermutationTimeType::UseSampleTime;

#if WITH_EDITORONLY_DATA
	// if set, all the channels of the same class with the same cardinality, and the same NormalizationGroup, will be normalized together.
	// for example in a locomotion database of a character holding a weapon, containing non mirrorable animations, you'd still want to normalize togeter 
	// left foot and right foot position and velocity
	UPROPERTY(EditAnywhere, Category = "Settings")
	FName NormalizationGroup;
#endif //WITH_EDITORONLY_DATA

	UFUNCTION(BlueprintPure, BlueprintImplementableEvent, meta=(BlueprintThreadSafe, DisplayName = "Get World Position"), Category = "Settings")
	FVector BP_GetWorldPosition(const UAnimInstance* AnimInstance) const;

	bool bUseBlueprintQueryOverride = false;

	UPoseSearchFeatureChannel_Position();

	// UPoseSearchFeatureChannel interface
	virtual bool Finalize(UPoseSearchSchema* Schema) override;
	virtual void BuildQuery(UE::PoseSearch::FSearchContext& SearchContext) const override;

	virtual EPermutationTimeType GetPermutationTimeType() const override { return PermutationTimeType; }
	virtual void AddDependentChannels(UPoseSearchSchema* Schema) const override;

#if ENABLE_DRAW_DEBUG
	virtual void DebugDraw(const UE::PoseSearch::FDebugDrawParams& DrawParams, TConstArrayView<float> PoseVector) const override;
#endif // ENABLE_DRAW_DEBUG

#if WITH_EDITOR
	virtual void FillWeights(TArrayView<float> Weights) const override;
	virtual bool IndexAsset(UE::PoseSearch::FAssetIndexer& Indexer) const override;
	virtual UE::PoseSearch::TLabelBuilder& GetLabel(UE::PoseSearch::TLabelBuilder& LabelBuilder, UE::PoseSearch::ELabelFormat LabelFormat = UE::PoseSearch::ELabelFormat::Full_Horizontal) const override;
	virtual FName GetNormalizationGroup() const override { return NormalizationGroup; }

	// IBoneReferenceSkeletonProvider interface
	USkeleton* GetSkeleton(bool& bInvalidSkeletonIsError, const IPropertyHandle* PropertyHandle) override;
#endif

	static void FindOrAddToSchema(UPoseSearchSchema* Schema, float SampleTimeOffset, const FName& BoneName, const UE::PoseSearch::FRole& Role, EPermutationTimeType PermutationTimeType = EPermutationTimeType::UseSampleTime);
};
