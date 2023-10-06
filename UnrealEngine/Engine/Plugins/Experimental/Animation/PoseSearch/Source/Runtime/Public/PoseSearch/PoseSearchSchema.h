// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BoneContainer.h"
#include "Engine/DataAsset.h"
#include "Interfaces/Interface_BoneReferenceSkeletonProvider.h"
#include "PoseSearch/PoseSearchFeatureChannel.h"
#include "PoseSearchSchema.generated.h"

struct FBoneReference;
class UMirrorDataTable;

UENUM()
enum class EPoseSearchDataPreprocessor : int32
{
	// The data will be left untouched.
	None,

	// The data will be normalized against its deviation, and the user weights will be normalized to be a unitary vector.
	Normalize,

	// The data will be normalized against its deviation
	NormalizeOnlyByDeviation,

	Num UMETA(Hidden),
	Invalid = Num UMETA(Hidden)
};

/**
* Specifies the format of a pose search index. At runtime, queries are built according to the schema for searching.
*/
UCLASS(BlueprintType, Category = "Animation|Pose Search", Experimental, meta = (DisplayName = "Motion Database Config"), CollapseCategories)
class POSESEARCH_API UPoseSearchSchema : public UDataAsset, public IBoneReferenceSkeletonProvider
{
	GENERATED_BODY()

public:
	// Skeleton Reference for Motion Matching Database assets. Must be set to a compatible skeleton to the animation data in the database.
	UPROPERTY(EditAnywhere, Category = "Schema", meta = (DisplayPriority = 0))
	TObjectPtr<USkeleton> Skeleton;

	// The update rate at which we sample the animation data in the database. The higher the SampleRate the more refined your searches will be, but the more memory will be required
	UPROPERTY(EditAnywhere, Category = "Schema", meta = (DisplayPriority = 3, ClampMin = "1", ClampMax = "240"))
	int32 SampleRate = 30;

private:
	// Channels itemize the cost breakdown of the config in simpler parts such as position or velocity of a bones, or phase of limbs. The total cost of a query against an indexed database pose will be the sum of the combined channel costs
	UPROPERTY(EditAnywhere, Instanced, Category = "Schema")
	TArray<TObjectPtr<UPoseSearchFeatureChannel>> Channels;

	// FinalizedChannels gets populated with UPoseSearchFeatureChannel(s) from Channels and additional injected ones during the Finalize.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPoseSearchFeatureChannel>> FinalizedChannels;

public:
	// Setting up and assigning a mirror data table will allow all your assets in your database to access the mirrored version of the data. This is required for mirroring to work with Motion Matching.
	UPROPERTY(EditAnywhere, Category = "Schema", meta = (DisplayPriority = 1))
	TObjectPtr<UMirrorDataTable> MirrorDataTable;

	// Type of operation performed to the full pose features dataset
	UPROPERTY(EditAnywhere, Category = "Schema", meta = (DisplayPriority = 2))
	EPoseSearchDataPreprocessor DataPreprocessor = EPoseSearchDataPreprocessor::Normalize;

	UPROPERTY(Transient)
	int32 SchemaCardinality = 0;

	UPROPERTY(Transient)
	TArray<FBoneReference> BoneReferences;

	UPROPERTY(Transient)
	TArray<uint16> BoneIndicesWithParents;

	// Cost added to the continuing pose from databases that uses this config. This allows users to apply a cost bias (positive or negative) to the continuing pose.
	// This is useful to help the system stay in one animation segment longer, or shorter depending on how you set this bias.
	// Negative values make it more likely to be picked, or stayed in, positive values make it less likely to be picked or stay in.
	UPROPERTY(EditAnywhere, Category = "Bias")
	float ContinuingPoseCostBias = -0.01f;

	// Base Cost added or removed to all poses from databases that use this config. It can be overridden by Anim Notify: Pose Search Modify Cost at the frame level of animation data.
	// Negative values make it more likely to be picked, or stayed in, Positive values make it less likely to be picked or stay in.
	UPROPERTY(EditAnywhere, Category = "Bias")
	float BaseCostBias = 0.f;

	// Cost added to all looping animation assets in a database that uses this config. This allows users to make it more or less likely to pick the looping animation segments.
	// Negative values make it more likely to be picked, or stayed in, Positive values make it less likely to be picked or stay in.
	UPROPERTY(EditAnywhere, Category = "Bias")
	float LoopingCostBias = -0.005f;

	// How many times the animation assets of the database using this schema will be indexed.
	UPROPERTY(EditAnywhere, Category = "Permutations", meta = (ClampMin = "1"))
	int32 NumberOfPermutations = 1;

	// Delta time between every permutation indexing.
	UPROPERTY(EditAnywhere, Category = "Permutations", meta = (ClampMin = "1", ClampMax = "240", EditCondition = "NumberOfPermutations > 1", EditConditionHides))
	int32 PermutationsSampleRate = 30;

	// Starting offset of the "PermutationTime" from the "SamplingTime" of the first permutation.
	// subsequent permutations will have PermutationTime = SamplingTime + PermutationsTimeOffset + PermutationIndex / PermutationsSampleRate.
	UPROPERTY(EditAnywhere, Category = "Permutations")
	float PermutationsTimeOffset = 0.f;

	// if true a padding channel will be added to make sure the data is 16 bytes (aligned) and padded, to facilitate performance improvements at cost of eventual additional memory
	UPROPERTY(EditAnywhere, Category = "Performance")
	bool bAddDataPadding = false;

	// If bInjectAdditionalDebugChannels is true, channels will be asked to inject additional channels into this schema.
	// the original intent is to add UPoseSearchFeatureChannel_Position(s) to help with the complexity of the debug drawing
	// (the database will have all the necessary positions to draw lines at the right location and time).
	UPROPERTY(EditAnywhere, Category = "Debug")
	bool bInjectAdditionalDebugChannels;

	bool IsValid () const;

	TConstArrayView<TObjectPtr<UPoseSearchFeatureChannel>> GetChannels() const { return FinalizedChannels; }

	void AddChannel(UPoseSearchFeatureChannel* Channel);
	void AddTemporaryChannel(UPoseSearchFeatureChannel* DependentChannel);

	template <typename FindPredicateType>
	const UPoseSearchFeatureChannel* FindChannel(FindPredicateType FindPredicate) const
	{
		return FindChannelRecursive(GetChannels(), FindPredicate);
	}

	template<typename ChannelType>
	const ChannelType* FindFirstChannelOfType() const
	{
		return static_cast<const ChannelType*>(FindChannel([this](const UPoseSearchFeatureChannel* Channel) -> const UPoseSearchFeatureChannel* { return Cast<ChannelType>(Channel); }));
	}

	// UObject
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
	virtual void PostLoad() override;

	int8 AddBoneReference(const FBoneReference& BoneReference);

	// IBoneReferenceSkeletonProvider
	USkeleton* GetSkeleton(bool& bInvalidSkeletonIsError, const IPropertyHandle* PropertyHandle) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	void BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, UE::PoseSearch::FFeatureVectorBuilder& InOutQuery) const;

	FBoneIndexType GetBoneIndexType(int8 SchemaBoneIdx) const;

	bool IsRootBone(int8 SchemaBoneIdx) const;
	
private:
	template <typename FindPredicateType>
	static const UPoseSearchFeatureChannel* FindChannelRecursive(TConstArrayView<TObjectPtr<UPoseSearchFeatureChannel>> Channels, FindPredicateType FindPredicate)
	{
		for (const TObjectPtr<UPoseSearchFeatureChannel>& ChannelPtr : Channels)
		{
			if (ChannelPtr)
			{
				if (const UPoseSearchFeatureChannel* Channel = FindPredicate(ChannelPtr))
				{
					return Channel;
				}

				if (const UPoseSearchFeatureChannel* Channel = FindChannelRecursive(ChannelPtr->GetSubChannels(), FindPredicate))
				{
					return Channel;
				}
			}
		}
		return nullptr;
	}

	void Finalize();
};