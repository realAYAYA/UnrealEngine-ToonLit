// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BoneContainer.h"
#include "Engine/DataAsset.h"
#include "Interfaces/Interface_BoneReferenceSkeletonProvider.h"
#include "PoseSearchSchema.generated.h"

struct FBoneReference;
struct FPoseSearchFeatureVectorBuilder;
class UMirrorDataTable;
class UPoseSearchFeatureChannel;

namespace UE::PoseSearch
{
	struct FSearchContext;
} // namespace UE::PoseSearch

UENUM()
enum class EPoseSearchDataPreprocessor : int32
{
	None,
	Normalize,
	NormalizeOnlyByDeviation,

	Num UMETA(Hidden),
	Invalid = Num UMETA(Hidden)
};

USTRUCT()
struct FPoseSearchSchemaColorPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Colors", meta = (ExcludeFromHash))
	FLinearColor Query = FLinearColor::Blue;

	UPROPERTY(EditAnywhere, Category = "Colors", meta = (ExcludeFromHash))
	FLinearColor Result = FLinearColor::Yellow;
};

/**
* Specifies the format of a pose search index. At runtime, queries are built according to the schema for searching.
*/
UCLASS(BlueprintType, Category = "Animation|Pose Search", Experimental, meta = (DisplayName = "Motion Database Config"), CollapseCategories)
class POSESEARCH_API UPoseSearchSchema : public UDataAsset, public IBoneReferenceSkeletonProvider
{
	GENERATED_BODY()

public:
	// @todo: used only for indexing: cache it somewhere else
	UPROPERTY(EditAnywhere, Category = "Schema")
	TObjectPtr<USkeleton> Skeleton;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "1", ClampMax = "240"), Category = "Schema")
	int32 SampleRate = 30;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Schema")
	TArray<TObjectPtr<UPoseSearchFeatureChannel>> Channels;

	// If set, this schema will support mirroring pose search databases
	UPROPERTY(EditAnywhere, Category = "Schema")
	TObjectPtr<UMirrorDataTable> MirrorDataTable;

	UPROPERTY(EditAnywhere, Category = "Schema")
	EPoseSearchDataPreprocessor DataPreprocessor = EPoseSearchDataPreprocessor::Normalize;

	UPROPERTY(Transient)
	int32 SchemaCardinality = 0;

	// @todo: used only for indexing: cache it somewhere else
	UPROPERTY(Transient)
	TArray<FBoneReference> BoneReferences;

	UPROPERTY(Transient)
	TArray<uint16> BoneIndicesWithParents;

	// cost added to the continuing pose from databases that uses this schema
	UPROPERTY(EditAnywhere, Category = "Schema")
	float ContinuingPoseCostBias = 0.f;

	// base cost added to all poses from databases that uses this schema. it can be overridden by UAnimNotifyState_PoseSearchModifyCost
	UPROPERTY(EditAnywhere, Category = "Schema")
	float BaseCostBias = 0.f;

	// If there's a mirroring mismatch between the currently playing asset and a search candidate, this cost will be 
	// added to the candidate, making it less likely to be selected
	UPROPERTY(EditAnywhere, Category = "Schema")
	float MirrorMismatchCostBias = 0.f;

	UPROPERTY(EditAnywhere, Category = "Schema", meta = (ExcludeFromHash))
	TArray<FPoseSearchSchemaColorPreset> ColorPresets;
	
	bool IsValid () const;

	float GetSamplingInterval() const { return 1.0f / SampleRate; }

	template<typename ChannelType>
	const ChannelType* FindFirstChannelOfType() const
	{
		for (const TObjectPtr<UPoseSearchFeatureChannel>& ChannelPtr : Channels)
		{
			if (const ChannelType* Channel = Cast<const ChannelType>(ChannelPtr.Get()))
			{
				return Channel;
			}
		}
		return nullptr;
	}

	// UObject
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
	virtual void PostLoad() override;

	int32 AddBoneReference(const FBoneReference& BoneReference);

	// IBoneReferenceSkeletonProvider
	USkeleton* GetSkeleton(bool& bInvalidSkeletonIsError, const IPropertyHandle* PropertyHandle) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	void BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, FPoseSearchFeatureVectorBuilder& InOutQuery) const;

	static constexpr FBoneIndexType RootBoneIdx = 0xFFFF;
	FBoneIndexType GetBoneIndexType(int8 SchemaBoneIdx) const;

private:
	void Finalize();
	void ResolveBoneReferences();
};
