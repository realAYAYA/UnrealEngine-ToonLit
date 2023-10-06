// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "PoseSearch/PoseSearchAssetSampler.h"
#include "PoseSearch/PoseSearchCost.h"
#include "PoseSearch/PoseSearchIndex.h"
#include "PoseSearch/PoseSearchResult.h"
#include "PoseSearchDatabase.generated.h"

struct FInstancedStruct;
class UAnimationAsset;
class UAnimComposite;
class UAnimMontage;
class UBlendSpace;
class UPoseSearchNormalizationSet;

namespace UE::PoseSearch
{
	struct FSearchContext;
} // namespace UE::PoseSearch

UENUM()
enum class EPoseSearchMode : int32
{
	// Database searches will be evaluated extensively. the system will evaluate all the indexed poses to search for the best one.
	BruteForce,

	// Optimized search mode: the database projects the poses into a PCA space using only the most significant "NumberOfPrincipalComponents" dimensions, and construct a kdtree to facilitate the search.
	PCAKDTree,

	// Debug functionality performing diagnostics and validation during searches using PCAKDTree.
	PCAKDTree_Validate,
	
	// Debug functionality to compare BruteForce vs PCAKDTree.
	PCAKDTree_Compare,

	Num UMETA(Hidden),
	Invalid = Num UMETA(Hidden)
};

UENUM()
enum class EPoseSearchMirrorOption : int32
{
	UnmirroredOnly UMETA(DisplayName = "Original Only"),
	MirroredOnly UMETA(DisplayName = "Mirrored Only"),
	UnmirroredAndMirrored UMETA(DisplayName = "Original and Mirrored"),

	Num UMETA(Hidden),
	Invalid = Num UMETA(Hidden)
};

USTRUCT()
struct FPoseSearchExcludeFromDatabaseParameters
{
	GENERATED_BODY()

	// Determines how much of the start of an animation segment is preserved for blending in seconds.
	// Excluding the beginning of animation segments can help ensure an exact past trajectory is used when building the channels.
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (DisplayName = "Anim Start Interval"))
	float SequenceStartInterval = 0.0f;

	// Determines how much of the end of an animation segment is preserved for blending in seconds.
	// Excluding the end of animation segments helps ensure an exact future trajectory,
	// and also prevents the selection of a anim segment which will end too soon to be worth selecting.
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (DisplayName = "Anim End Interval"))
	float SequenceEndInterval = 0.3f;
};

USTRUCT()
struct POSESEARCH_API FPoseSearchDatabaseAnimationAssetBase
{
	GENERATED_BODY()
	virtual ~FPoseSearchDatabaseAnimationAssetBase() = default;

	virtual UAnimationAsset* GetAnimationAsset() const { return nullptr; }
	virtual UClass* GetAnimationAssetStaticClass() const { return nullptr; }
	virtual bool IsLooping() const { return false; }
	virtual const FString GetName() const { return FString(); }
	virtual bool IsEnabled() const { return false; }
	virtual void SetIsEnabled(bool bInIsEnabled) {}
	virtual bool IsRootMotionEnabled() const { return false; }
	virtual EPoseSearchMirrorOption GetMirrorOption() const { return EPoseSearchMirrorOption::Invalid; }
	// [0, 0] represents the entire frame range of the original animation.
	virtual FFloatInterval GetSamplingRange() const { return FFloatInterval(0.f, 0.f); }
};

/** A sequence entry in a UPoseSearchDatabase. */
USTRUCT(BlueprintType, Category = "Animation|Pose Search")
struct POSESEARCH_API FPoseSearchDatabaseSequence : public FPoseSearchDatabaseAnimationAssetBase
{
	GENERATED_BODY()
	virtual ~FPoseSearchDatabaseSequence() = default;

	UPROPERTY(EditAnywhere, Category="Sequence", meta = (DisplayPriority = 0))
	TObjectPtr<UAnimSequence> Sequence;

	// This allows users to enable or exclude animations from this database. Useful for debugging.
	UPROPERTY(EditAnywhere, Category = "Sequence", meta = (DisplayPriority = 3))
	bool bEnabled = true;

	// It allows users to set a time range to an individual animation sequence in the database. 
	// This is effectively trimming the beginning and end of the animation in the database (not in the original sequence).
	// If set to [0, 0] it will be the entire frame range of the original sequence.
	UPROPERTY(EditAnywhere, Category="Sequence", meta = (DisplayPriority = 1))
	FFloatInterval SamplingRange = FFloatInterval(0.f, 0.f);

	// This allows users to set if this animation is original only (no mirrored data), original and mirrored, or only the mirrored version of this animation.
	// It requires the mirror table to be set up in the config file.
	UPROPERTY(EditAnywhere, Category = "Sequence", meta = (DisplayPriority = 2))
	EPoseSearchMirrorOption MirrorOption = EPoseSearchMirrorOption::UnmirroredOnly;

	UAnimationAsset* GetAnimationAsset() const override;
	UClass* GetAnimationAssetStaticClass() const override;
	bool IsLooping() const override;
	const FString GetName() const override;
	bool IsEnabled() const override { return bEnabled; }
	void SetIsEnabled(bool bInIsEnabled) override { bEnabled = bInIsEnabled; }
	bool IsRootMotionEnabled() const override;
	EPoseSearchMirrorOption GetMirrorOption() const override { return MirrorOption; }
	FFloatInterval GetSamplingRange() const override { return SamplingRange; }
};

/** An blend space entry in a UPoseSearchDatabase. */
USTRUCT(BlueprintType, Category = "Animation|Pose Search")
struct POSESEARCH_API FPoseSearchDatabaseBlendSpace : public FPoseSearchDatabaseAnimationAssetBase
{
	GENERATED_BODY()
	virtual ~FPoseSearchDatabaseBlendSpace() = default;

	UPROPERTY(EditAnywhere, Category = "BlendSpace", meta = (DisplayPriority = 0))
	TObjectPtr<UBlendSpace> BlendSpace;

	// This allows users to set if this animation is original only (no mirrored data), original and mirrored, or only the mirrored version of this animation.
	// It requires the mirror table to be set up in the config file.
	UPROPERTY(EditAnywhere, Category = "BlendSpace", meta = (DisplayPriority = 1))
	EPoseSearchMirrorOption MirrorOption = EPoseSearchMirrorOption::UnmirroredOnly;

	// If true this BlendSpace will output a single segment in the database.
	UPROPERTY(EditAnywhere, Category = "BlendSpace", meta = (DisplayPriority = 2))
	bool bUseSingleSample = false;

	// When turned on, this will use the set grid samples of the blend space asset for sampling. This will override the Number of Horizontal/Vertical Samples.
	UPROPERTY(EditAnywhere, Category = "BlendSpace", meta = (EditCondition = "!bUseSingleSample", EditConditionHides, DisplayPriority = 3))
	bool bUseGridForSampling = false;

	// Sets the number of horizontal samples in the blend space to pull the animation data coverage from. The larger the samples the more the data, but also the more memory and performance it takes.
	UPROPERTY(EditAnywhere, Category = "BlendSpace", meta = (EditCondition = "!bUseSingleSample && !bUseGridForSampling", EditConditionHides, ClampMin = "1", UIMin = "1", UIMax = "25", DisplayPriority = 4))
	int32 NumberOfHorizontalSamples = 9;
	
	// Sets the number of vertical samples in the blend space to pull the animation data coverage from.The larger the samples the more the data, but also the more memory and performance it takes.
	UPROPERTY(EditAnywhere, Category = "BlendSpace", meta = (EditCondition = "!bUseSingleSample && !bUseGridForSampling", EditConditionHides, ClampMin = "1", UIMin = "1", UIMax = "25", DisplayPriority = 5))
	int32 NumberOfVerticalSamples = 2;

	// BlendParams used to sample this BlendSpace
	UPROPERTY(EditAnywhere, Category = "BlendSpace", meta = (EditCondition = "bUseSingleSample", EditConditionHides, DisplayPriority = 6))
	float BlendParamX = 0.f;

	// BlendParams used to sample this BlendSpace
	UPROPERTY(EditAnywhere, Category = "BlendSpace", meta = (EditCondition = "bUseSingleSample", EditConditionHides, DisplayPriority = 7))
	float BlendParamY = 0.f;

	// This allows users to enable or exclude animations from this database. Useful for debugging.
	UPROPERTY(EditAnywhere, Category = "BlendSpace", meta = (DisplayPriority = 8))
	bool bEnabled = true;

	UAnimationAsset* GetAnimationAsset() const override;
	UClass* GetAnimationAssetStaticClass() const override;
	bool IsLooping() const override;
	const FString GetName() const override;
	bool IsEnabled() const override { return bEnabled; }
	void SetIsEnabled(bool bInIsEnabled) override { bEnabled = bInIsEnabled; }
	bool IsRootMotionEnabled() const override;
	EPoseSearchMirrorOption GetMirrorOption() const override { return MirrorOption; }

	void GetBlendSpaceParameterSampleRanges(int32& HorizontalBlendNum, int32& VerticalBlendNum) const;
	FVector BlendParameterForSampleRanges(int32 HorizontalBlendIndex, int32 VerticalBlendIndex) const;
};

/** An entry in a UPoseSearchDatabase. */
USTRUCT(BlueprintType, Category = "Animation|Pose Search")
struct POSESEARCH_API FPoseSearchDatabaseAnimComposite : public FPoseSearchDatabaseAnimationAssetBase
{
	GENERATED_BODY()
	virtual ~FPoseSearchDatabaseAnimComposite() = default;

	UPROPERTY(EditAnywhere, Category = "AnimComposite", meta = (DisplayPriority = 0))
	TObjectPtr<UAnimComposite> AnimComposite;

	// This allows users to enable or exclude animations from this database. Useful for debugging.
	UPROPERTY(EditAnywhere, Category = "AnimComposite", meta = (DisplayPriority = 3))
	bool bEnabled = true;

	// It allows users to set a time range to an individual animation sequence in the database. 
	// This is effectively trimming the beginning and end of the animation in the database (not in the original sequence).
	// If set to [0, 0] it will be the entire frame range of the original sequence.
	UPROPERTY(EditAnywhere, Category = "AnimComposite", meta = (DisplayPriority = 1))
	FFloatInterval SamplingRange = FFloatInterval(0.f, 0.f);

	// This allows users to set if this animation is original only (no mirrored data), original and mirrored, or only the mirrored version of this animation.
	// It requires the mirror table to be set up in the config file.
	UPROPERTY(EditAnywhere, Category = "AnimComposite", meta = (DisplayPriority = 2))
	EPoseSearchMirrorOption MirrorOption = EPoseSearchMirrorOption::UnmirroredOnly;

	UAnimationAsset* GetAnimationAsset() const override;
	UClass* GetAnimationAssetStaticClass() const override;
	bool IsLooping() const override;
	const FString GetName() const override;
	bool IsEnabled() const override { return bEnabled; }
	void SetIsEnabled(bool bInIsEnabled) override { bEnabled = bInIsEnabled; }
	bool IsRootMotionEnabled() const override;
	EPoseSearchMirrorOption GetMirrorOption() const override { return MirrorOption; }
	FFloatInterval GetSamplingRange() const override { return SamplingRange; }
};

/** An anim montage entry in a UPoseSearchDatabase. */
USTRUCT(BlueprintType, Category = "Animation|Pose Search")
struct POSESEARCH_API FPoseSearchDatabaseAnimMontage : public FPoseSearchDatabaseAnimationAssetBase
{
	GENERATED_BODY()
	virtual ~FPoseSearchDatabaseAnimMontage() = default;

	UPROPERTY(EditAnywhere, Category="AnimMontage", meta = (DisplayPriority = 0))
	TObjectPtr<UAnimMontage> AnimMontage;

	// This allows users to enable or exclude animations from this database. Useful for debugging.
	UPROPERTY(EditAnywhere, Category = "AnimMontage", meta = (DisplayPriority = 3))
	bool bEnabled = true;

	// It allows users to set a time range to an individual animation sequence in the database. 
	// This is effectively trimming the beginning and end of the animation in the database (not in the original sequence).
	// If set to [0, 0] it will be the entire frame range of the original sequence.
	UPROPERTY(EditAnywhere, Category="AnimMontage", meta = (DisplayPriority = 1))
	FFloatInterval SamplingRange = FFloatInterval(0.f, 0.f);

	// This allows users to set if this animation is original only (no mirrored data), original and mirrored, or only the mirrored version of this animation.
	// It requires the mirror table to be set up in the config file.
	UPROPERTY(EditAnywhere, Category = "AnimMontage", meta = (DisplayPriority = 2))
	EPoseSearchMirrorOption MirrorOption = EPoseSearchMirrorOption::UnmirroredOnly;

	UAnimationAsset* GetAnimationAsset() const override;
	UClass* GetAnimationAssetStaticClass() const override;
	bool IsLooping() const override;
	const FString GetName() const override;
	bool IsEnabled() const override { return bEnabled; }
	void SetIsEnabled(bool bInIsEnabled) override { bEnabled = bInIsEnabled; }
	bool IsRootMotionEnabled() const override;
	EPoseSearchMirrorOption GetMirrorOption() const override { return MirrorOption; }
	FFloatInterval GetSamplingRange() const override { return SamplingRange; }
};

/** A data asset for indexing a collection of animation sequences. */
UCLASS(BlueprintType, Category = "Animation|Pose Search", Experimental, meta = (DisplayName = "Motion Database"))
class POSESEARCH_API UPoseSearchDatabase : public UDataAsset
{
	GENERATED_BODY()
public:

	// The Motion Database Config sets what channels this database will use to match against (bones, trajectory and what properties of those you’re interested in, such as position and velocity).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Database", DisplayName="Config")
	TObjectPtr<const UPoseSearchSchema> Schema;

	// These settings allow users to trim the start and end of animations in the database to preserve start/end frames for blending, and prevent the system from selecting the very last frames before it blends out.
	UPROPERTY(EditAnywhere, Category = "Database")
	FPoseSearchExcludeFromDatabaseParameters ExcludeFromDatabaseParameters;

#if WITH_EDITORONLY_DATA
	// Sequences and Blendspaces are deprecated and its data will be part of the AnimationAssets.
	// All sequences and blend spaces will be added to the AnimationAssets in PostLoad().
	UPROPERTY()
	TArray<FPoseSearchDatabaseSequence> Sequences_DEPRECATED;
	UPROPERTY()
	TArray<FPoseSearchDatabaseBlendSpace> BlendSpaces_DEPRECATED;

	// If null, the default preview mesh for the skeleton will be used. Otherwise, this will be used in preview scenes.
	// @todo: Move this to be a setting in the Pose Search Database editor. 
	UPROPERTY(EditAnywhere, Category = "Preview")
	TObjectPtr<USkeletalMesh> PreviewMesh = nullptr;
#endif // WITH_EDITORONLY_DATA

	UPROPERTY(EditAnywhere, Category="Database")
	TArray<FInstancedStruct> AnimationAssets;

	// This dictates how the database will perform the search.
	UPROPERTY(EditAnywhere, Category = "Performance")
	EPoseSearchMode PoseSearchMode = EPoseSearchMode::PCAKDTree;

	// Number of dimensions used to create the kdtree. More dimensions allows a better explanation of the variance of the dataset that usually translates in better search results, but will imply more memory usage and worse performances.
	UPROPERTY(EditAnywhere, Category = "Performance", meta = (EditCondition = "PoseSearchMode != EPoseSearchMode::BruteForce", EditConditionHides, ClampMin = "1", ClampMax = "64", UIMin = "1", UIMax = "64"))
	int32 NumberOfPrincipalComponents = 4;

	UPROPERTY(EditAnywhere, Category = "Performance", meta = (EditCondition = "PoseSearchMode != EPoseSearchMode::BruteForce", EditConditionHides, ClampMin = "1", ClampMax = "256", UIMin = "1", UIMax = "256"))
	int32 KDTreeMaxLeafSize = 16;
	
	// Out of a kdtree search, results will have only an approximate cost, so the database search will select the best “KDTree Query Num Neighbors” poses to perform the full cost analysis, and be able to elect the best pose.
	UPROPERTY(EditAnywhere, Category = "Performance", meta = (EditCondition = "PoseSearchMode != EPoseSearchMode::BruteForce", EditConditionHides, ClampMin = "1", ClampMax = "600", UIMin = "1", UIMax = "600"))
	int32 KDTreeQueryNumNeighbors = 200;

	// When evaluating multiple searches, including the continuing pose search, the system keeps track of the best pose and associated cost.
	// if the current database cannot possibly improve the current cost, the database search will be skipped entirely.
	UPROPERTY(EditAnywhere, Category = "Performance")
	bool bSkipSearchIfPossible = true;

#if WITH_EDITORONLY_DATA
	// This optional asset defines a list of databases you want to normalize together. Without it, it would be difficult to compare costs from separately normalized databases containing different types of animation,
	// like only idles versus only runs animations, given that the range of movement would be dramatically different.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Database")
	TObjectPtr<const UPoseSearchNormalizationSet> NormalizationSet;
#endif // WITH_EDITORONLY_DATA

private:
	// Do not use it directly. Use GetSearchIndex / SetSearchIndex interact with it and validate that is ok to do so.
	UE::PoseSearch::FSearchIndex SearchIndexPrivate;

public:
	virtual ~UPoseSearchDatabase();

	void SetSearchIndex(const UE::PoseSearch::FSearchIndex& SearchIndex);
	const UE::PoseSearch::FSearchIndex& GetSearchIndex() const;
	
	bool GetSkipSearchIfPossible() const;

	int32 GetPoseIndexFromTime(float AssetTime, const UE::PoseSearch::FSearchIndexAsset& SearchIndexAsset) const;

	const FInstancedStruct& GetAnimationAssetStruct(int32 AnimationAssetIndex) const;
	const FInstancedStruct& GetAnimationAssetStruct(const UE::PoseSearch::FSearchIndexAsset& SearchIndexAsset) const;
	FInstancedStruct& GetMutableAnimationAssetStruct(int32 AnimationAssetIndex);
	FInstancedStruct& GetMutableAnimationAssetStruct(const UE::PoseSearch::FSearchIndexAsset& SearchIndexAsset);
	const FPoseSearchDatabaseAnimationAssetBase* GetAnimationAssetBase(int32 AnimationAssetIndex) const;
	const FPoseSearchDatabaseAnimationAssetBase* GetAnimationAssetBase(const UE::PoseSearch::FSearchIndexAsset& SearchIndexAsset) const;
	FPoseSearchDatabaseAnimationAssetBase* GetMutableAnimationAssetBase(int32 AnimationAssetIndex);
	FPoseSearchDatabaseAnimationAssetBase* GetMutableAnimationAssetBase(const UE::PoseSearch::FSearchIndexAsset& SearchIndexAsset);
	const bool IsSourceAssetLooping(const UE::PoseSearch::FSearchIndexAsset& SearchIndexAsset) const;
	const FString GetSourceAssetName(const UE::PoseSearch::FSearchIndexAsset& SearchIndexAsset) const;
	int32 GetNumberOfPrincipalComponents() const;
	float GetRealAssetTime(int32 PoseIdx) const;
	float GetNormalizedAssetTime(int32 PoseIdx) const;

	// Begin UObject
	virtual void PostLoad() override;
	virtual void PostSaveRoot(FObjectPostSaveRootContext ObjectSaveContext) override;
	virtual void Serialize(FArchive& Ar) override;
	// End UObject
	
	UE::PoseSearch::FSearchResult Search(UE::PoseSearch::FSearchContext& SearchContext) const;
	FPoseSearchCost SearchContinuingPose(UE::PoseSearch::FSearchContext& SearchContext) const;

#if WITH_EDITOR
	virtual void BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform) override;
	virtual bool IsCachedCookedPlatformDataLoaded(const ITargetPlatform* TargetPlatform) override;

private:
	DECLARE_MULTICAST_DELEGATE(FOnDerivedDataRebuildMulticaster);
	FOnDerivedDataRebuildMulticaster OnDerivedDataRebuild;
public:
	typedef FOnDerivedDataRebuildMulticaster::FDelegate FOnDerivedDataRebuild;
	void RegisterOnDerivedDataRebuild(const FOnDerivedDataRebuild& Delegate);
	void UnregisterOnDerivedDataRebuild(void* Unregister);
	void NotifyDerivedDataRebuild() const;
#endif // WITH_EDITOR

private:
	UE::PoseSearch::FSearchResult SearchPCAKDTree(UE::PoseSearch::FSearchContext& SearchContext) const;
	UE::PoseSearch::FSearchResult SearchBruteForce(UE::PoseSearch::FSearchContext& SearchContext) const;
};