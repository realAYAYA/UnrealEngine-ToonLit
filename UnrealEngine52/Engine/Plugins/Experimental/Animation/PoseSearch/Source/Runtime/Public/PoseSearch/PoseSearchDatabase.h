// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearch/PoseSearchAssetSampler.h"
#include "PoseSearch/PoseSearchIndex.h"
#include "PoseSearch/PoseSearchSearchableAsset.h"
#include "PoseSearchDatabase.generated.h"

struct FInstancedStruct;
class UAnimComposite;

UENUM()
enum class EPoseSearchMode : int32
{
	BruteForce,
	PCAKDTree,
	PCAKDTree_Validate,	// runs PCAKDTree and performs validation tests
	PCAKDTree_Compare,	// compares BruteForce vs PCAKDTree

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

	// Excluding the beginning of sequences can help ensure an exact past trajectory is used when building the features
	UPROPERTY(EditAnywhere, Category = "Settings")
	float SequenceStartInterval = 0.0f;

	// Excluding the end of sequences help ensure an exact future trajectory, and also prevents the selection of
	// a sequence which will end too soon to be worth selecting.
	UPROPERTY(EditAnywhere, Category = "Settings")
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
	virtual const FString GetName() const { return {}; }
	virtual bool IsEnabled() const { return false; }
	virtual void SetIsEnabled(bool bInIsEnabled) {}
	virtual bool IsRootMotionEnabled() const { return false; }
	virtual EPoseSearchMirrorOption GetMirrorOption() const { return EPoseSearchMirrorOption::Invalid; }
	virtual ESearchIndexAssetType GetSearchIndexType() const { return ESearchIndexAssetType::Invalid; }
};

/** An entry in a UPoseSearchDatabase. */
USTRUCT(BlueprintType, Category = "Animation|Pose Search")
struct POSESEARCH_API FPoseSearchDatabaseSequence : public FPoseSearchDatabaseAnimationAssetBase
{
	GENERATED_BODY()
	virtual ~FPoseSearchDatabaseSequence() = default;

	UPROPERTY(EditAnywhere, Category="Sequence")
	TObjectPtr<UAnimSequence> Sequence;

	UPROPERTY(EditAnywhere, Category = "Sequence")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Category="Sequence")
	FFloatInterval SamplingRange = FFloatInterval(0.0f, 0.0f);

	UPROPERTY(EditAnywhere, Category = "Sequence")
	EPoseSearchMirrorOption MirrorOption = EPoseSearchMirrorOption::UnmirroredOnly;

	UAnimationAsset* GetAnimationAsset() const override;
	UClass* GetAnimationAssetStaticClass() const override;
	bool IsLooping() const override;
	const FString GetName() const override;
	bool IsEnabled() const override { return bEnabled; }
	void SetIsEnabled(bool bInIsEnabled) override { bEnabled = bInIsEnabled; }
	bool IsRootMotionEnabled() const override;
	EPoseSearchMirrorOption GetMirrorOption() const override { return MirrorOption; }
	ESearchIndexAssetType GetSearchIndexType() const override { return ESearchIndexAssetType::Sequence; }
};

/** An blend space entry in a UPoseSearchDatabase. */
USTRUCT(BlueprintType, Category = "Animation|Pose Search")
struct POSESEARCH_API FPoseSearchDatabaseBlendSpace : public FPoseSearchDatabaseAnimationAssetBase
{
	GENERATED_BODY()
	virtual ~FPoseSearchDatabaseBlendSpace() = default;

	UPROPERTY(EditAnywhere, Category = "BlendSpace")
	TObjectPtr<UBlendSpace> BlendSpace;

	UPROPERTY(EditAnywhere, Category = "BlendSpace")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Category = "BlendSpace")
	EPoseSearchMirrorOption MirrorOption = EPoseSearchMirrorOption::UnmirroredOnly;

	// If to use the blendspace grid locations as parameter sample locations.
	// When enabled, NumberOfHorizontalSamples and NumberOfVerticalSamples are ignored.
	UPROPERTY(EditAnywhere, Category = "BlendSpace")
	bool bUseGridForSampling = false;

	UPROPERTY(EditAnywhere, Category = "BlendSpace", meta = (EditCondition = "!bUseGridForSampling", EditConditionHides, ClampMin = "1", UIMin = "1", UIMax = "25"))
	int32 NumberOfHorizontalSamples = 9;

	UPROPERTY(EditAnywhere, Category = "BlendSpace", meta = (EditCondition = "!bUseGridForSampling", EditConditionHides, ClampMin = "1", UIMin = "1", UIMax = "25"))
	int32 NumberOfVerticalSamples = 2;

	UAnimationAsset* GetAnimationAsset() const override;
	UClass* GetAnimationAssetStaticClass() const override;
	bool IsLooping() const override;
	const FString GetName() const override;
	bool IsEnabled() const override { return bEnabled; }
	void SetIsEnabled(bool bInIsEnabled) override { bEnabled = bInIsEnabled; }
	bool IsRootMotionEnabled() const override;
	EPoseSearchMirrorOption GetMirrorOption() const override { return MirrorOption; }
	ESearchIndexAssetType GetSearchIndexType() const override { return ESearchIndexAssetType::BlendSpace; }

	void GetBlendSpaceParameterSampleRanges(int32& HorizontalBlendNum, int32& VerticalBlendNum) const;
	FVector BlendParameterForSampleRanges(int32 HorizontalBlendIndex, int32 VerticalBlendIndex) const;
};

/** An entry in a UPoseSearchDatabase. */
USTRUCT(BlueprintType, Category = "Animation|Pose Search")
struct POSESEARCH_API FPoseSearchDatabaseAnimComposite : public FPoseSearchDatabaseAnimationAssetBase
{
	GENERATED_BODY()
	virtual ~FPoseSearchDatabaseAnimComposite() = default;

	UPROPERTY(EditAnywhere, Category = "AnimComposite")
	TObjectPtr<UAnimComposite> AnimComposite;

	UPROPERTY(EditAnywhere, Category = "AnimComposite")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Category = "AnimComposite")
	FFloatInterval SamplingRange = FFloatInterval(0.0f, 0.0f);

	UPROPERTY(EditAnywhere, Category = "AnimComposite")
	EPoseSearchMirrorOption MirrorOption = EPoseSearchMirrorOption::UnmirroredOnly;

	UAnimationAsset* GetAnimationAsset() const override;
	UClass* GetAnimationAssetStaticClass() const override;
	bool IsLooping() const override;
	const FString GetName() const override;
	bool IsEnabled() const override { return bEnabled; }
	void SetIsEnabled(bool bInIsEnabled) override { bEnabled = bInIsEnabled; }
	bool IsRootMotionEnabled() const override;
	EPoseSearchMirrorOption GetMirrorOption() const override { return MirrorOption; }
	ESearchIndexAssetType GetSearchIndexType() const override { return ESearchIndexAssetType::AnimComposite; }
};

/** A data asset for indexing a collection of animation sequences. */
UCLASS(BlueprintType, Category = "Animation|Pose Search", Experimental, meta = (DisplayName = "Normalization Set"))
class POSESEARCH_API UNormalizationSetAsset : public UDataAsset
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NormalizationSet")
	TArray<TObjectPtr<const UPoseSearchDatabase>> Databases; // @todo: it should be a UPoseSearchSearchableAsset, and have the UPoseSearchSearchableAsset iterate over all it's contained UPoseSearchSearchableAsset recursively (without duplicates)
};

/** A data asset for indexing a collection of animation sequences. */
UCLASS(BlueprintType, Category = "Animation|Pose Search", Experimental, meta = (DisplayName = "Motion Database"))
class POSESEARCH_API UPoseSearchDatabase : public UPoseSearchSearchableAsset
{
	GENERATED_BODY()
public:
	// Motion Database Config asset to use with this database.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Database", DisplayName="Config")
	TObjectPtr<const UPoseSearchSchema> Schema;

	UPROPERTY(EditAnywhere, Category = "Database")
	FPoseSearchExtrapolationParameters ExtrapolationParameters;

	UPROPERTY(EditAnywhere, Category = "Database")
	FPoseSearchExcludeFromDatabaseParameters ExcludeFromDatabaseParameters;

#if WITH_EDITORONLY_DATA
	// Sequences and Blendspaces are deprecated and its data will be part of the AnimationAssets.
	// All sequences and blend spaces will be added to the AnimationAssets in PostLoad().
	UPROPERTY()
	TArray<FPoseSearchDatabaseSequence> Sequences_DEPRECATED;
	UPROPERTY()
	TArray<FPoseSearchDatabaseBlendSpace> BlendSpaces_DEPRECATED;
#endif // WITH_EDITORONLY_DATA

	UPROPERTY(EditAnywhere, Category="Database")
	TArray<FInstancedStruct> AnimationAssets;

	UPROPERTY(EditAnywhere, Category = "Performance")
	EPoseSearchMode PoseSearchMode = EPoseSearchMode::PCAKDTree;

	UPROPERTY(EditAnywhere, Category = "Performance", meta = (EditCondition = "PoseSearchMode != EPoseSearchMode::BruteForce", EditConditionHides, ClampMin = "1", ClampMax = "64", UIMin = "1", UIMax = "64"))
	int32 NumberOfPrincipalComponents = 4;

	UPROPERTY(EditAnywhere, Category = "Performance", meta = (EditCondition = "PoseSearchMode != EPoseSearchMode::BruteForce", EditConditionHides, ClampMin = "1", ClampMax = "256", UIMin = "1", UIMax = "256"))
	int32 KDTreeMaxLeafSize = 16;
	
	UPROPERTY(EditAnywhere, Category = "Performance", meta = (EditCondition = "PoseSearchMode != EPoseSearchMode::BruteForce", EditConditionHides, ClampMin = "1", ClampMax = "600", UIMin = "1", UIMax = "600"))
	int32 KDTreeQueryNumNeighbors = 200;

	// if true, this database search will be skipped if cannot decrease the pose cost, and poses will not be listed into the PoseSearchDebugger
	UPROPERTY(EditAnywhere, Category = "Performance")
	bool bSkipSearchIfPossible = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Database")
	TObjectPtr<const UNormalizationSetAsset> NormalizationSet;

private:
	UPROPERTY(Transient, meta = (ExcludeFromHash))
	FPoseSearchIndex SearchIndexPrivate; // do not use it directly. use GetSearchIndex / SetSearchIndex interact with it and validate that is ok to do so

public:
	virtual ~UPoseSearchDatabase();

	void SetSearchIndex(const FPoseSearchIndex& SearchIndex);
	const FPoseSearchIndex& GetSearchIndex() const;
	
	bool GetSkipSearchIfPossible() const;

	int32 GetPoseIndexFromTime(float AssetTime, const FPoseSearchIndexAsset& SearchIndexAsset) const;
	bool GetPoseIndicesAndLerpValueFromTime(float Time, const FPoseSearchIndexAsset& SearchIndexAsset, int32& PrevPoseIdx, int32& PoseIdx, int32& NextPoseIdx, float& LerpValue) const;

	const FInstancedStruct& GetAnimationAssetStruct(int32 AnimationAssetIndex) const;
	const FInstancedStruct& GetAnimationAssetStruct(const FPoseSearchIndexAsset& SearchIndexAsset) const;
	FInstancedStruct& GetMutableAnimationAssetStruct(int32 AnimationAssetIndex);
	FInstancedStruct& GetMutableAnimationAssetStruct(const FPoseSearchIndexAsset& SearchIndexAsset);
	const FPoseSearchDatabaseAnimationAssetBase* GetAnimationAssetBase(int32 AnimationAssetIndex) const;
	const FPoseSearchDatabaseAnimationAssetBase* GetAnimationAssetBase(const FPoseSearchIndexAsset& SearchIndexAsset) const;
	FPoseSearchDatabaseAnimationAssetBase* GetMutableAnimationAssetBase(int32 AnimationAssetIndex);
	FPoseSearchDatabaseAnimationAssetBase* GetMutableAnimationAssetBase(const FPoseSearchIndexAsset& SearchIndexAsset);
	const bool IsSourceAssetLooping(const FPoseSearchIndexAsset& SearchIndexAsset) const;
	const FString GetSourceAssetName(const FPoseSearchIndexAsset& SearchIndexAsset) const;
	int32 GetNumberOfPrincipalComponents() const;
	void BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, FPoseSearchFeatureVectorBuilder& OutQuery) const;

	// Begin UObject
	virtual void PostLoad() override;
	virtual void PostSaveRoot(FObjectPostSaveRootContext ObjectSaveContext) override;
	virtual void Serialize(FArchive& Ar) override;
	// End UObject
	
	// Begin UPoseSearchSearchableAsset
	virtual UE::PoseSearch::FSearchResult Search(UE::PoseSearch::FSearchContext& SearchContext) const override;
	// End UPoseSearchSearchableAsset

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