// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/RingBuffer.h"
#include "Engine/DataAsset.h"
#include "Modules/ModuleManager.h"
#include "Misc/EnumClassFlags.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Templates/TypeHash.h"
#include "Animation/AnimMetaData.h"
#include "Animation/AnimNodeMessages.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimSequence.h"
#include "Animation/MotionTrajectoryTypes.h"
#include "AlphaBlend.h"
#include "BoneIndices.h"
#include "GameplayTagContainer.h"
#include "Interfaces/Interface_BoneReferenceSkeletonProvider.h"
#include "PoseSearch/KDTree.h"
#include "PoseSearch/PoseSearchDerivedDataKey.h"
#include "ObjectTrace.h"
#include "PoseSearch.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPoseSearch, Log, All);

// Enable this if object tracing is enabled, mimics animation tracing
#define UE_POSE_SEARCH_TRACE_ENABLED OBJECT_TRACE_ENABLED

//////////////////////////////////////////////////////////////////////////
// Forward declarations

class UAnimSequence;
class UBlendSpace;
struct FCompactPose;
struct FPoseContext;
struct FReferenceSkeleton;
struct FPoseSearchDatabaseDerivedData;
class UAnimNotifyState_PoseSearchBase;
class UPoseSearchSchema;
class FBlake3;

namespace UE::PoseSearch
{
class FPoseHistory;
struct FPoseSearchDatabaseAsyncCacheTask;
struct FDebugDrawParams;
struct FSchemaInitializer;
struct FQueryBuildingContext;
struct FSearchContext;
} // namespace UE::PoseSearch

// eigen forward declaration
namespace Eigen
{
	template<typename _Scalar, int _Rows, int _Cols, int _Options, int _MaxRows, int _MaxCols>
	class Matrix;
	using MatrixXd = Matrix<double, -1, -1, 0, -1, -1>;
	using VectorXd = Matrix<double, -1, 1, 0, -1, 1>;
}

//////////////////////////////////////////////////////////////////////////
// Constants

UENUM()
enum class EPoseSearchBooleanRequest : uint8
{
	FalseValue,
	TrueValue,
	Indifferent, // if this is used, there will be no cost difference between true and false results

	Num UMETA(Hidden),
	Invalid = Num UMETA(Hidden)
};

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
enum class EPoseSearchDataPreprocessor : int32
{
	None,
	Normalize,
	NormalizeOnlyByDeviation,

	Num UMETA(Hidden),
	Invalid = Num UMETA(Hidden)
};

UENUM()
enum class EPoseSearchPoseFlags : uint32
{
	None = 0,

	// Don't return this pose as a search result
	BlockTransition = 1 << 0,
};
ENUM_CLASS_FLAGS(EPoseSearchPoseFlags);

UENUM()
enum class ESearchIndexAssetType : int32
{
	Invalid,
	Sequence,
	BlendSpace,
};

namespace UE::PoseSearch
{
	
enum class EPoseComparisonFlags : int32
{
	None = 0,
	ContinuingPose = 1 << 0,
};
ENUM_CLASS_FLAGS(EPoseComparisonFlags);

} // namespace UE::PoseSearch

UENUM()
enum class EPoseSearchMirrorOption : int32
{
	UnmirroredOnly UMETA(DisplayName = "Original Only"),
	MirroredOnly UMETA(DisplayName = "Mirrored Only"),
	UnmirroredAndMirrored UMETA(DisplayName = "Original and Mirrored"),

	Num UMETA(Hidden),
	Invalid = Num UMETA(Hidden)
};


//////////////////////////////////////////////////////////////////////////
// Common structs

USTRUCT()
struct POSESEARCH_API FPoseSearchExtrapolationParameters
{
	GENERATED_BODY()

	// If the angular root motion speed in degrees is below this value, it will be treated as zero.
	UPROPERTY(EditAnywhere, Category = "Settings")
	float AngularSpeedThreshold = 1.0f;

	// If the root motion linear speed is below this value, it will be treated as zero.
	UPROPERTY(EditAnywhere, Category = "Settings")
	float LinearSpeedThreshold = 1.0f;

	// Time from sequence start/end used to extrapolate the trajectory.
	UPROPERTY(EditAnywhere, Category = "Settings")
	float SampleTime = 0.05f;
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

//////////////////////////////////////////////////////////////////////////
// Asset sampling and indexing

namespace UE::PoseSearch
{

struct POSESEARCH_API FAssetSamplingContext
{
	// Time delta used for computing pose derivatives
	static constexpr float FiniteDelta = 1 / 60.0f;

	FBoneContainer BoneContainer;

	// Mirror data table pointer copied from Schema for convenience
	TObjectPtr<UMirrorDataTable> MirrorDataTable = nullptr;

	// Compact pose format of Mirror Bone Map
	TCustomBoneIndexArray<FCompactPoseBoneIndex, FCompactPoseBoneIndex> CompactPoseMirrorBones;

	// Pre-calculated component space rotations of reference pose, which allows mirror to work with any joint orientation
	// Only initialized and used when a mirroring table is specified
	TCustomBoneIndexArray<FQuat, FCompactPoseBoneIndex> ComponentSpaceRefRotations;

	void Init(const UPoseSearchSchema* Schema);
	FTransform MirrorTransform(const FTransform& Transform) const;
};

/**
 * Helper interface for sampling data from animation assets
 */
class POSESEARCH_API IAssetSampler
{
public:
	virtual ~IAssetSampler() {};

	virtual float GetPlayLength() const = 0;
	virtual bool IsLoopable() const = 0;

	// Gets the time associated with a particular root distance traveled
	virtual float GetTimeFromRootDistance(float Distance) const = 0;

	// Gets the total root distance traveled 
	virtual float GetTotalRootDistance() const = 0;

	// Gets the final root transformation at the end of the asset's playback time
	virtual FTransform GetTotalRootTransform() const = 0;

	// Extracts pose for this asset for a given context
	virtual void ExtractPose(const FAnimExtractContext& ExtractionCtx, FAnimationPoseData& OutAnimPoseData) const = 0;

	// Extracts the accumulated root distance at the given time, using the extremities of the sequence to extrapolate 
	// beyond the sequence limits when Time is less than zero or greater than the sequence length
	virtual float ExtractRootDistance(float Time) const = 0;

	// Extracts root transform at the given time, using the extremities of the sequence to extrapolate beyond the 
	// sequence limits when Time is less than zero or greater than the sequence length.
	virtual FTransform ExtractRootTransform(float Time) const = 0;

	// Extracts notify states inheriting from UAnimNotifyState_PoseSearchBase present in the sequence at Time.
	// The function does not empty NotifyStates before adding new notifies!
	virtual void ExtractPoseSearchNotifyStates(float Time, TArray<UAnimNotifyState_PoseSearchBase*>& NotifyStates) const = 0;

	virtual const UAnimationAsset* GetAsset() const = 0;
};

/**
 * Inputs for asset indexing
 */
struct FAssetIndexingContext
{
	const FAssetSamplingContext* SamplingContext = nullptr;
	const UPoseSearchSchema* Schema = nullptr;
	const IAssetSampler* MainSampler = nullptr;
	const IAssetSampler* LeadInSampler = nullptr;
	const IAssetSampler* FollowUpSampler = nullptr;
	bool bMirrored = false;
	FFloatInterval RequestedSamplingRange = FFloatInterval(0.0f, 0.0f);
	
	// Index this asset's data from BeginPoseIdx up to but not including EndPoseIdx
	int32 BeginSampleIdx = 0;
	int32 EndSampleIdx = 0;
};

/**
 * Output of indexer data for this asset
 */
struct FAssetIndexingOutput
{
	// Channel data should be written to this array of feature vector builders
	// Size is EndPoseIdx - BeginPoseIdx and PoseVectors[0] contains data for BeginPoseIdx
	const TArrayView<FPoseSearchFeatureVectorBuilder> PoseVectors;
};

class POSESEARCH_API IAssetIndexer
{
public:
	struct FSampleInfo
	{
		const IAssetSampler* Clip = nullptr;
		FTransform RootTransform;
		float ClipTime = 0.0f;
		float RootDistance = 0.0f;
		bool bClamped = false;

		bool IsValid() const { return Clip != nullptr; }
	};

	virtual ~IAssetIndexer() {}

	virtual const FAssetIndexingContext& GetIndexingContext() const = 0;
	virtual FSampleInfo GetSampleInfo(float SampleTime) const = 0;
	virtual FSampleInfo GetSampleInfoRelative(float SampleTime, const FSampleInfo& Origin) const = 0;
	virtual const float GetSampleTimeFromDistance(float Distance) const = 0;
	virtual FTransform MirrorTransform(const FTransform& Transform) const = 0;
	virtual FTransform GetTransformAndCacheResults(float SampleTime, float OriginTime, int8 SchemaBoneIdx, bool& Clamped) = 0;
};

class POSESEARCH_API ICostBreakDownData
{
public:
	virtual ~ICostBreakDownData() {}

	// returns the size of the dataset
	virtual int32 Num() const = 0;

	// returns true if Index-th cost data vector is associated with Schema
	virtual bool IsCostVectorFromSchema(int32 Index, const UPoseSearchSchema* Schema) const = 0;

	// returns the Index-th cost data vector
	virtual TConstArrayView<float> GetCostVector(int32 Index, const UPoseSearchSchema* Schema) const = 0;

	// every breakdown section start by calling BeginBreakDownSection...
	virtual void BeginBreakDownSection(const FText& Label) = 0;

	// ...then add as many SetCostBreakDown into the section...
	virtual void SetCostBreakDown(float CostBreakDown, int32 Index, const UPoseSearchSchema* Schema) = 0;

	// ...to finally wrap the section up by calling EndBreakDownSection
	virtual void EndBreakDownSection(const FText& Label) = 0;

	// true if want the channel to be verbose and generate the cost breakdown labels
	virtual bool IsVerbose() const { return true; }

	// most common case implementation
	void AddEntireBreakDownSection(const FText& Label, const UPoseSearchSchema* Schema, int32 DataOffset, int32 Cardinality);
};

} // namespace UE::PoseSearch

//////////////////////////////////////////////////////////////////////////
// Feature channels interface

UCLASS(Abstract, BlueprintType, EditInlineNew)
class POSESEARCH_API UPoseSearchFeatureChannel : public UObject, public IBoneReferenceSkeletonProvider
{
	GENERATED_BODY()

public:
	int32 GetChannelIndex() const { checkSlow(ChannelIdx >= 0); return ChannelIdx; }
	int32 GetChannelCardinality() const { checkSlow(ChannelCardinality >= 0); return ChannelCardinality; }
	int32 GetChannelDataOffset() const { checkSlow(ChannelDataOffset >= 0); return ChannelDataOffset; }

	// Called during UPoseSearchSchema::Finalize to prepare the schema for this channel
	virtual void InitializeSchema(UE::PoseSearch::FSchemaInitializer& Initializer);
	
	// Called at database build time to collect feature weights.
	// Weights is sized to the cardinality of the schema and the feature channel should write
	// its weights at the channel's data offset. Channels should provide a weight for each dimension.
	virtual void FillWeights(TArray<float>& Weights) const PURE_VIRTUAL(UPoseSearchFeatureChannel::FillWeights, );

	// Called at database build time to populate pose vectors with this channel's data
	virtual void IndexAsset(UE::PoseSearch::IAssetIndexer& Indexer, UE::PoseSearch::FAssetIndexingOutput& IndexingOutput) const PURE_VIRTUAL(UPoseSearchFeatureChannel::IndexAsset, );

	// Called at database build time to calculate normalization values
	virtual void ComputeMeanDeviations(const Eigen::MatrixXd& CenteredPoseMatrix, Eigen::VectorXd& MeanDeviations) const;

	// Called at runtime to add this channel's data to the query pose vector
	virtual bool BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, FPoseSearchFeatureVectorBuilder& InOutQuery) const PURE_VIRTUAL(UPoseSearchFeatureChannel::BuildQuery, return false;);

	// Draw this channel's data for the given pose vector
	virtual void DebugDraw(const UE::PoseSearch::FDebugDrawParams& DrawParams, TArrayView<const float> PoseVector) const PURE_VIRTUAL(UPoseSearchFeatureChannel::DebugDraw, );

#if WITH_EDITOR
	virtual void ComputeCostBreakdowns(UE::PoseSearch::ICostBreakDownData& CostBreakDownData, const UPoseSearchSchema* Schema) const;
#endif

	// Used during data normalization. If a feature has less than this amount of deviation from the mean across all poses in a database, then it will not be normalized.
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ClampMin = "0.0001"))
	float MinimumMeanDeviation = 1.0f;

private:
	// IBoneReferenceSkeletonProvider interface
	// Note this function is exclusively for FBoneReference details customization
	class USkeleton* GetSkeleton(bool& bInvalidSkeletonIsError, const IPropertyHandle* PropertyHandle) override;

	friend class ::UPoseSearchSchema;

	UPROPERTY()
	int32 ChannelIdx = -1;

protected:
	UPROPERTY()
	int32 ChannelDataOffset = -1;

	UPROPERTY()
	int32 ChannelCardinality = -1;
};

//////////////////////////////////////////////////////////////////////////
// Schema

namespace UE::PoseSearch
{

struct POSESEARCH_API FSchemaInitializer
{
public:
	int32 AddBoneReference(const FBoneReference& BoneReference);

	// Gets the index into the schema's channel array for the channel currently being initialized
	int32 GetCurrentChannelIdx() const { return CurrentChannelIdx; }

	int32 GetCurrentChannelDataOffset() const { return CurrentChannelDataOffset; }
	void SetCurrentChannelDataOffset(int32 DataOffset) { CurrentChannelDataOffset = DataOffset; }

private:
	friend class ::UPoseSearchSchema;

	int32 CurrentChannelIdx = 0;
	int32 CurrentChannelDataOffset = 0;
	TArray<FBoneReference> BoneReferences;
};

} // namespace UE::PoseSearch

USTRUCT()
struct FPoseSearchSchemaColorPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Colors")
	FLinearColor Query = FLinearColor::Blue;

	UPROPERTY(EditAnywhere, Category = "Colors")
	FLinearColor Result = FLinearColor::Yellow;
};

/**
* Specifies the format of a pose search index. At runtime, queries are built according to the schema for searching.
*/
UCLASS(BlueprintType, Category = "Animation|Pose Search", Experimental, meta = (DisplayName = "Motion Database Config"))
class POSESEARCH_API UPoseSearchSchema : public UDataAsset, public IBoneReferenceSkeletonProvider
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Schema")
	TObjectPtr<USkeleton> Skeleton = nullptr;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "1", ClampMax = "240"), Category = "Schema")
	int32 SampleRate = 10;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Schema")
	TArray<TObjectPtr<UPoseSearchFeatureChannel>> Channels;

	// If set, this schema will support mirroring pose search databases
	UPROPERTY(EditAnywhere, Category = "Schema")
	TObjectPtr<UMirrorDataTable> MirrorDataTable;

	UPROPERTY(EditAnywhere, Category = "Schema")
	EPoseSearchDataPreprocessor DataPreprocessor = EPoseSearchDataPreprocessor::Normalize;

	UPROPERTY()
	int32 SchemaCardinality = 0;

	UPROPERTY()
	TArray<FBoneReference> BoneReferences;

	UPROPERTY(Transient)
	TArray<uint16> BoneIndices;

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

	UPROPERTY(EditAnywhere, Category = "Schema")
	TArray<FPoseSearchSchemaColorPreset> ColorPresets;
	
	bool IsValid () const;

	int32 GetNumBones () const { return BoneIndices.Num(); }

	float GetSamplingInterval() const { return 1.0f / SampleRate; }

	// UObject
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
	virtual void PostLoad() override;

	// IBoneReferenceSkeletonProvider
	class USkeleton* GetSkeleton(bool& bInvalidSkeletonIsError, const IPropertyHandle* PropertyHandle) override { bInvalidSkeletonIsError = false; return Skeleton; }

#if WITH_EDITOR
	void ComputeCostBreakdowns(UE::PoseSearch::ICostBreakDownData& CostBreakDownData) const;

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void Finalize(bool bRemoveEmptyChannels = true);
	void ResolveBoneReferences();

public:
	bool BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, FPoseSearchFeatureVectorBuilder& InOutQuery) const;
};

//////////////////////////////////////////////////////////////////////////
// Search index

/**
 * This is kept for each pose in the search index along side the feature vector values and is used to influence the search.
 */
USTRUCT()
struct POSESEARCH_API FPoseSearchPoseMetadata
{
	GENERATED_BODY()

	UPROPERTY()
	EPoseSearchPoseFlags Flags = EPoseSearchPoseFlags::None;

	// @todo: consider float16
	UPROPERTY()
	float CostAddend = 0.0f;

	// @todo: consider float16
	UPROPERTY()
	float ContinuingPoseCostAddend = 0.0f;
};

/**
* Information about a source animation asset used by a search index.
* Some source animation entries may generate multiple FPoseSearchIndexAsset entries.
**/
USTRUCT()
struct POSESEARCH_API FPoseSearchIndexAsset
{
	GENERATED_BODY()
public:
	FPoseSearchIndexAsset()
	{}

	FPoseSearchIndexAsset(
		ESearchIndexAssetType InType,
		int32 InSourceAssetIdx, 
		bool bInMirrored, 
		const FFloatInterval& InSamplingInterval,
		FVector InBlendParameters = FVector::Zero())
		: Type(InType)
		, SourceAssetIdx(InSourceAssetIdx)
		, bMirrored(bInMirrored)
		, BlendParameters(InBlendParameters)
		, SamplingInterval(InSamplingInterval)
	{}

	// Default to Sequence for now for backward compatibility but
	// at some point we might want to change this to Invalid.
	UPROPERTY()
	ESearchIndexAssetType Type = ESearchIndexAssetType::Sequence;

	// Index of the source asset in search index's container (i.e. UPoseSearchDatabase)
	UPROPERTY()
	int32 SourceAssetIdx = INDEX_NONE;

	UPROPERTY()
	bool bMirrored = false;

	UPROPERTY()
	FVector BlendParameters = FVector::Zero();

	UPROPERTY()
	FFloatInterval SamplingInterval;

	UPROPERTY()
	int32 FirstPoseIdx = INDEX_NONE;

	UPROPERTY()
	int32 NumPoses = 0;

	bool IsPoseInRange(int32 PoseIdx) const
	{
		return (PoseIdx >= FirstPoseIdx) && (PoseIdx < FirstPoseIdx + NumPoses);
	}
};

/**
* A search index for animation poses. The structure of the search index is determined by its UPoseSearchSchema.
* May represent a single animation (see UPoseSearchSequenceMetaData) or a collection (see UPoseSearchDatabase).
*/
USTRUCT()
struct POSESEARCH_API FPoseSearchIndex
{
	GENERATED_BODY()

	UPROPERTY(Category = Info, VisibleAnywhere)
	int32 NumPoses = 0;

	UPROPERTY()
	TArray<float> Values;

	UPROPERTY()
	TArray<float> PCAValues;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Category = Info, VisibleAnywhere)
	float PCAExplainedVariance = 0.f;
	
	UPROPERTY(Category = Info, VisibleAnywhere)
	TArray<float> Deviation;
#endif // WITH_EDITORONLY_DATA

	UE::PoseSearch::FKDTree KDTree;

	UPROPERTY(Category = Info, VisibleAnywhere)
	TArray<float> PCAProjectionMatrix;

	UPROPERTY(Category = Info, VisibleAnywhere)
	TArray<float> Mean;

	// we store weights square roots to reduce numerical errors when CompareFeatureVectors 
	// ((VA - VB) * VW).square().sum()
	// instead of
	// ((VA - VB).square() * VW).sum()
	// since (VA - VB).square() could lead to big numbers, and VW being multiplied by the variance of the dataset
	UPROPERTY(Category = Info, VisibleAnywhere) 
	TArray<float> WeightsSqrt;

	UPROPERTY()
	TArray<FPoseSearchPoseMetadata> PoseMetadata;

	UPROPERTY()
	TObjectPtr<const UPoseSearchSchema> Schema = nullptr;

	UPROPERTY()
	TArray<FPoseSearchIndexAsset> Assets;

	// minimum of the database metadata CostAddend: it represents the minimum cost of any search for the associated database (we'll skip the search in case the search result total cost is already less than MinCostAddend)
	UPROPERTY(Category = Info, VisibleAnywhere)
	float MinCostAddend = -MAX_FLT;

	bool IsValid() const;
	bool IsValidPoseIndex(int32 PoseIdx) const { return PoseIdx < NumPoses; }
	bool IsEmpty() const;

	TConstArrayView<float> GetPoseValues(int32 PoseIdx) const;

	const FPoseSearchIndexAsset* FindAssetForPose(int32 PoseIdx) const;
	float GetAssetTime(int32 PoseIdx, const FPoseSearchIndexAsset* Asset) const;

	void Reset();

	// individual cost addends calculation methods
	float ComputeMirrorMismatchAddend(int32 PoseIdx, UE::PoseSearch::FSearchContext& SearchContext) const;
	float ComputeNotifyAddend(int32 PoseIdx) const;
	float ComputeContinuingPoseCostAddend(int32 PoseIdx, UE::PoseSearch::EPoseComparisonFlags PoseComparisonFlags) const;
};

//////////////////////////////////////////////////////////////////////////
// Database

USTRUCT()
struct POSESEARCH_API FPoseSearchDatabaseAnimationAssetBase
{
	GENERATED_BODY()

	virtual ~FPoseSearchDatabaseAnimationAssetBase() {}
	virtual UAnimationAsset* GetAnimationAsset() const { return nullptr; }
	virtual bool IsLooping() const { return false; }
#if WITH_EDITOR
	virtual void BuildDerivedDataKey(UE::PoseSearch::FDerivedDataKeyBuilder& KeyBuilder) {}
#endif
};

/** An entry in a UPoseSearchDatabase. */
USTRUCT(BlueprintType, Category = "Animation|Pose Search")
struct POSESEARCH_API FPoseSearchDatabaseSequence : public FPoseSearchDatabaseAnimationAssetBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Sequence")
	TObjectPtr<UAnimSequence> Sequence = nullptr;

	UPROPERTY(EditAnywhere, Category = "Sequence")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Category="Sequence")
	FFloatInterval SamplingRange = FFloatInterval(0.0f, 0.0f);

	UPROPERTY(EditAnywhere, Category = "Sequence")
	EPoseSearchMirrorOption MirrorOption = EPoseSearchMirrorOption::UnmirroredOnly;

	// Used for sampling past pose information at the beginning of the main sequence.
	// This setting is intended for transitions between cycles. It is optional and only used
	// for one shot anims with past sampling. When past sampling is used without a lead in sequence,
	// the sampling range of the main sequence will be clamped if necessary.
	UPROPERTY(EditAnywhere, Category="Sequence")
	TObjectPtr<UAnimSequence> LeadInSequence = nullptr;

	// Used for sampling future pose information at the end of the main sequence.
	// This setting is intended for transitions between cycles. It is optional and only used
	// for one shot anims with future sampling. When future sampling is used without a follow up sequence,
	// the sampling range of the main sequence will be clamped if necessary.
	UPROPERTY(EditAnywhere, Category="Sequence")
	TObjectPtr<UAnimSequence> FollowUpSequence = nullptr;

	virtual UAnimationAsset* GetAnimationAsset() const override { return Sequence; }
	virtual bool IsLooping() const override { return Sequence->bLoop; }
#if WITH_EDITOR
	virtual void BuildDerivedDataKey(UE::PoseSearch::FDerivedDataKeyBuilder& KeyBuilder) override;
#endif
};

/** An blend space entry in a UPoseSearchDatabase. */
USTRUCT(BlueprintType, Category = "Animation|Pose Search")
struct POSESEARCH_API FPoseSearchDatabaseBlendSpace : public FPoseSearchDatabaseAnimationAssetBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "BlendSpace")
	TObjectPtr<UBlendSpace> BlendSpace = nullptr;

	UPROPERTY(EditAnywhere, Category = "BlendSpace")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, Category = "BlendSpace")
	EPoseSearchMirrorOption MirrorOption = EPoseSearchMirrorOption::UnmirroredOnly;

	// If to use the blendspace grid locations as parameter sample locations.
	// When enabled, NumberOfHorizontalSamples and NumberOfVerticalSamples are ignored.
	UPROPERTY(EditAnywhere, Category = "BlendSpace")
	bool bUseGridForSampling = true;

	UPROPERTY(EditAnywhere, Category = "BlendSpace", meta = (EditCondition = "!bUseGridForSampling", EditConditionHides, ClampMin = "1", UIMin = "1", UIMax = "25"))
	int32 NumberOfHorizontalSamples = 5;

	UPROPERTY(EditAnywhere, Category = "BlendSpace", meta = (EditCondition = "!bUseGridForSampling", EditConditionHides, ClampMin = "1", UIMin = "1", UIMax = "25"))
	int32 NumberOfVerticalSamples = 5;

	virtual UAnimationAsset* GetAnimationAsset() const override;
	virtual bool IsLooping() const override;
#if WITH_EDITOR
	virtual void BuildDerivedDataKey(UE::PoseSearch::FDerivedDataKeyBuilder& KeyBuilder) override;
#endif
public:

	void GetBlendSpaceParameterSampleRanges(
		int32& HorizontalBlendNum,
		int32& VerticalBlendNum,
		float& HorizontalBlendMin,
		float& HorizontalBlendMax,
		float& VerticalBlendMin,
		float& VerticalBlendMax) const;
};

USTRUCT()
struct POSESEARCH_API FPoseSearchCost
{
	GENERATED_BODY()
public:
	FPoseSearchCost() = default;
	FPoseSearchCost(float InDissimilarityCost, float InNotifyCostAddend, float InMirrorMismatchCostAddend, float InContinuingPoseCostAddend)
	: TotalCost(InDissimilarityCost + InNotifyCostAddend + InMirrorMismatchCostAddend + InContinuingPoseCostAddend)
	{
#if WITH_EDITORONLY_DATA
		NotifyCostAddend = InNotifyCostAddend;
		MirrorMismatchCostAddend = InMirrorMismatchCostAddend;
		ContinuingPoseCostAddend = InContinuingPoseCostAddend;
#endif // WITH_EDITORONLY_DATA
	}

	bool IsValid() const { return TotalCost != MAX_flt; }

	float GetTotalCost() const { return TotalCost; }

	bool operator<(const FPoseSearchCost& Other) const { return TotalCost < Other.TotalCost; }

protected:
	UPROPERTY()
	float TotalCost = MAX_flt;

#if WITH_EDITORONLY_DATA
public:

	float GetCostAddend() const
	{
		return NotifyCostAddend + MirrorMismatchCostAddend + ContinuingPoseCostAddend;
	}

	// Contribution from ModifyCost anim notify
	UPROPERTY()
	float NotifyCostAddend = 0.f;

	// Contribution from mirroring cost
	UPROPERTY()
	float MirrorMismatchCostAddend = 0.f;

	UPROPERTY()
	float ContinuingPoseCostAddend = 0.f;

#endif // WITH_EDITORONLY_DATA
};

/**
* Helper object for writing features into a float buffer according to a feature vector layout.
* Keeps track of which features are present, allowing the feature vector to be built up piecemeal.
* FFeatureVectorBuilder is used to build search queries at runtime and for adding samples during search index construction.
*/
USTRUCT(BlueprintType, Category = "Animation|Pose Search")
struct POSESEARCH_API FPoseSearchFeatureVectorBuilder
{
	GENERATED_BODY()

public:
	void Init(const UPoseSearchSchema* Schema);
	void Reset();
	void ResetFeatures();

	const UPoseSearchSchema* GetSchema() const { return Schema.Get(); }

	TArray<float>& EditValues() { return Values; }
	TArrayView<const float> GetValues() const { return Values; }

	void CopyFromSearchIndex(const FPoseSearchIndex& SearchIndex, int32 PoseIdx);

	bool IsInitialized() const;
	bool IsInitializedForSchema(const UPoseSearchSchema* Schema) const;
	bool IsCompatible(const FPoseSearchFeatureVectorBuilder& OtherBuilder) const;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<const UPoseSearchSchema> Schema = nullptr;

	TArray<float> Values;
};

namespace UE::PoseSearch
{

enum class EDebugDrawFlags : uint32
{
	None = 0,

	// Draw the entire search index as a point cloud
	DrawSearchIndex = 1 << 0,
	
	// Draw using Query colors form the schema / config
	DrawQuery = 1 << 1,

	/**
	 * Keep rendered data until the next call to FlushPersistentDebugLines().
	 * Combine with DrawSearchIndex to draw the search index only once.
	 */
	Persistent = 1 << 2,
	
	// Label samples with their indices
	DrawSampleLabels = 1 << 3,

	// Draw Bone Names
	DrawBoneNames = 1 << 5,

	// Draws simpler shapes to improve performance
	DrawFast = 1 << 6,
};
ENUM_CLASS_FLAGS(EDebugDrawFlags);

struct POSESEARCH_API FDebugDrawParams
{
	const UWorld* World = nullptr;
	const UPoseSearchDatabase* Database = nullptr;
	const UPoseSearchSequenceMetaData* SequenceMetaData = nullptr;
	EDebugDrawFlags Flags = EDebugDrawFlags::None;
	uint32 ChannelMask = (uint32)-1;

	float DefaultLifeTime = 5.f;
	float PointSize = 1.f;

	FTransform RootTransform = FTransform::Identity;

	// Optional Mesh for gathering SocketTransform(s)
	TWeakObjectPtr<const USkinnedMeshComponent> Mesh = nullptr;

	bool CanDraw() const;
	FColor GetColor(int32 ColorPreset) const;
	const FPoseSearchIndex* GetSearchIndex() const;
	const UPoseSearchSchema* GetSchema() const;
};

class IPoseHistoryProvider : public UE::Anim::IGraphMessage
{
	DECLARE_ANIMGRAPH_MESSAGE(IPoseHistoryProvider);

public:

	virtual const FPoseHistory& GetPoseHistory() const = 0;
	virtual FPoseHistory& GetPoseHistory() = 0;
};

struct FSearchResult
{
	// best cost of the currently selected PoseIdx (it could be equal to ContinuingPoseCost)
	FPoseSearchCost PoseCost;
	int32 PoseIdx = INDEX_NONE;

	int32 PrevPoseIdx = INDEX_NONE;
	int32 NextPoseIdx = INDEX_NONE;

	// lerp value to find AssetTime from PrevPoseIdx -> AssetTime -> NextPoseIdx, within range [-0.5, 0.5]
	float LerpValue = 0.f;

	// @todo: it should be a weak pointer
	const FPoseSearchIndexAsset* SearchIndexAsset = nullptr;
	TWeakObjectPtr<const UPoseSearchDatabase> Database = nullptr;
	FPoseSearchFeatureVectorBuilder ComposedQuery;

	// cost of the current pose with the query from database in the result, if possible
	FPoseSearchCost ContinuingPoseCost; 

	float AssetTime = 0.0f;

#if WITH_EDITOR
	FIoHash SearchIndexHash = FIoHash::Zero;
#endif // WITH_EDITOR
#if WITH_EDITORONLY_DATA
	FPoseSearchCost BruteForcePoseCost;
#endif // WITH_EDITORONLY_DATA

	// Attempts to set the internal state to match the provided asset time including updating the internal DbPoseIdx. 
	// If the provided asset time is out of bounds for the currently playing asset then this function will reset the 
	// state back to the default state.
	void Update(float NewAssetTime);

	bool IsValid() const;

	void Reset();
};

} // namespace UE::PoseSearch

/** A data asset for indexing a collection of animation sequences. */
UCLASS(Abstract, BlueprintType, Experimental)
class POSESEARCH_API UPoseSearchSearchableAsset : public UDataAsset
{
	GENERATED_BODY()
public:

	virtual UE::PoseSearch::FSearchResult Search(UE::PoseSearch::FSearchContext& SearchContext) const PURE_VIRTUAL(UPoseSearchSearchableAsset::Search, return UE::PoseSearch::FSearchResult(););
};

/** A data asset for indexing a collection of animation sequences. */
UCLASS(BlueprintType, Category = "Animation|Pose Search", Experimental, meta = (DisplayName = "Motion Database"))
class POSESEARCH_API UPoseSearchDatabase : public UPoseSearchSearchableAsset
{
	GENERATED_BODY()
public:
	// Motion Database Config asset to use with this database.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Database", DisplayName="Config")
	TObjectPtr<const UPoseSearchSchema> Schema = nullptr;

	UPROPERTY(EditAnywhere, Category = "Database")
	FPoseSearchExtrapolationParameters ExtrapolationParameters;

	UPROPERTY(EditAnywhere, Category = "Database")
	FPoseSearchExcludeFromDatabaseParameters ExcludeFromDatabaseParameters;

	// Drag and drop animations here to add them in bulk to Sequences
	UPROPERTY(EditAnywhere, Category = "Database", DisplayName="Drag And Drop Anims Here")
	TArray<TObjectPtr<UAnimSequence>> SimpleSequences;

	UPROPERTY(EditAnywhere, Category="Database")
	TArray<FPoseSearchDatabaseSequence> Sequences;

	// Drag and drop blendspaces here to add them in bulk to Blend Spaces
	UPROPERTY(EditAnywhere, Category = "Database", DisplayName = "Drag And Drop Blend Spaces Here")
	TArray<TObjectPtr<UBlendSpace>> SimpleBlendSpaces;

	UPROPERTY(EditAnywhere, Category = "Database")
	TArray<FPoseSearchDatabaseBlendSpace> BlendSpaces;

	UPROPERTY(EditAnywhere, Category = "Performance")
	EPoseSearchMode PoseSearchMode = EPoseSearchMode::BruteForce;

	UPROPERTY(EditAnywhere, Category = "Performance", meta = (EditCondition = "PoseSearchMode != EPoseSearchMode::BruteForce", EditConditionHides, ClampMin = "1", ClampMax = "64", UIMin = "1", UIMax = "64"))
	int32 NumberOfPrincipalComponents = 4;

	UPROPERTY(EditAnywhere, Category = "Performance", meta = (EditCondition = "PoseSearchMode != EPoseSearchMode::BruteForce", EditConditionHides, ClampMin = "1", ClampMax = "256", UIMin = "1", UIMax = "256"))
	int32 KDTreeMaxLeafSize = 8;
	
	UPROPERTY(EditAnywhere, Category = "Performance", meta = (EditCondition = "PoseSearchMode != EPoseSearchMode::BruteForce", EditConditionHides, ClampMin = "1", ClampMax = "600", UIMin = "1", UIMax = "600"))
	int32 KDTreeQueryNumNeighbors = 100;

	// if true, this database search will be skipped if cannot decrease the pose cost, and poses will not be listed into the PoseSearchDebugger
	UPROPERTY(EditAnywhere, Category = "Performance")
	bool bSkipSearchIfPossible = false;

	virtual ~UPoseSearchDatabase();

	const FPoseSearchIndex* GetSearchIndex() const;
	const FPoseSearchIndex* GetSearchIndexSafe() const;
	
	bool GetSkipSearchIfPossible() const;
	bool IsValidForIndexing() const;
	bool IsValidForSearch() const;
	bool IsValidPoseIndex(int32 PoseIdx) const;

	float GetAssetTime(int32 PoseIdx, const FPoseSearchIndexAsset* SearchIndexAsset = nullptr) const;
	int32 GetPoseIndexFromTime(float AssetTime, const FPoseSearchIndexAsset* SearchIndexAsset) const;
	bool GetPoseIndicesAndLerpValueFromTime(float Time, const FPoseSearchIndexAsset* SearchIndexAsset, int32& PrevPoseIdx, int32& PoseIdx, int32& NextPoseIdx, float& LerpValue) const;

	const FPoseSearchDatabaseAnimationAssetBase& GetAnimationSourceAsset(const FPoseSearchIndexAsset* SearchIndexAsset) const;
	const FPoseSearchDatabaseSequence& GetSequenceSourceAsset(const FPoseSearchIndexAsset* SearchIndexAsset) const;
	const FPoseSearchDatabaseBlendSpace& GetBlendSpaceSourceAsset(const FPoseSearchIndexAsset* SearchIndexAsset) const;
	const bool IsSourceAssetLooping(const FPoseSearchIndexAsset* SearchIndexAsset) const;
	const FString GetSourceAssetName(const FPoseSearchIndexAsset* SearchIndexAsset) const;
	int32 GetNumberOfPrincipalComponents() const;
	
	// UObject
	virtual void PostLoad() override;
	virtual void PostSaveRoot(FObjectPostSaveRootContext ObjectSaveContext) override;
	virtual void Serialize(FArchive& Ar) override;

	// Populates the FPoseSearchIndex::Assets array by evaluating the data in the Sequences array
	bool TryInitSearchIndexAssets(FPoseSearchIndex& OutSearchIndex) const;

#if WITH_EDITOR
	void BuildDerivedDataKey(UE::PoseSearch::FDerivedDataKeyBuilder& KeyBuilder);
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform) override;
	virtual bool IsCachedCookedPlatformDataLoaded(const ITargetPlatform* TargetPlatform) override;
#endif // WITH_EDITOR

private:
	FPoseSearchIndex* GetSearchIndex();
	void CollectSimpleSequences();
	void CollectSimpleBlendSpaces();
	void FindValidSequenceIntervals(const FPoseSearchDatabaseSequence& DbSequence, TArray<FFloatRange>& ValidRanges) const;
	
	FPoseSearchDatabaseDerivedData* PrivateDerivedData;

#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE(FOnDerivedDataRebuildMulticaster);
	FOnDerivedDataRebuildMulticaster OnDerivedDataRebuild;

	DECLARE_MULTICAST_DELEGATE(FOnAssetChangeMulticaster);
	FOnDerivedDataRebuildMulticaster OnAssetChange;

	DECLARE_MULTICAST_DELEGATE(FOnGroupChangeMulticaster);
	FOnDerivedDataRebuildMulticaster OnGroupChange;
#endif // WITH_EDITOR

public:
#if WITH_EDITOR

	typedef FOnDerivedDataRebuildMulticaster::FDelegate FOnDerivedDataRebuild;
	void RegisterOnDerivedDataRebuild(const FOnDerivedDataRebuild& Delegate);
	void UnregisterOnDerivedDataRebuild(void* Unregister);
	void NotifyDerivedDataBuildStarted();

	typedef FOnAssetChangeMulticaster::FDelegate FOnAssetChange;
	void RegisterOnAssetChange(const FOnAssetChange& Delegate);
	void UnregisterOnAssetChange(void* Unregister);
	void NotifyAssetChange();

	typedef FOnGroupChangeMulticaster::FDelegate FOnGroupChange;
	void RegisterOnGroupChange(const FOnGroupChange& Delegate);
	void UnregisterOnGroupChange(void* Unregister);
	void NotifyGroupChange();

	void BeginCacheDerivedData();

	FIoHash GetSearchIndexHash() const;

	bool IsDerivedDataBuildPending() const;
#endif // WITH_EDITOR

	bool IsDerivedDataValid();

public:
	virtual UE::PoseSearch::FSearchResult Search(UE::PoseSearch::FSearchContext& SearchContext) const override;

	void BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, FPoseSearchFeatureVectorBuilder& OutQuery) const;

	FPoseSearchCost ComparePoses(UE::PoseSearch::FSearchContext& SearchContext, int32 PoseIdx, UE::PoseSearch::EPoseComparisonFlags PoseComparisonFlags, TConstArrayView<float> QueryValues) const;

protected:
	UE::PoseSearch::FSearchResult SearchPCAKDTree(UE::PoseSearch::FSearchContext& SearchContext) const;
	UE::PoseSearch::FSearchResult SearchBruteForce(UE::PoseSearch::FSearchContext& SearchContext) const;
};

//////////////////////////////////////////////////////////////////////////
// Sequence metadata

/** Animation metadata object for indexing a single animation. */
UCLASS(BlueprintType, Category = "Animation|Pose Search", Experimental)
class POSESEARCH_API UPoseSearchSequenceMetaData : public UAnimMetaData
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = "Settings")
	TObjectPtr<const UPoseSearchSchema> Schema = nullptr;

	UPROPERTY(EditAnywhere, Category = "Settings")
	FFloatInterval SamplingRange = FFloatInterval(0.0f, 0.0f);

	UPROPERTY(EditAnywhere, Category = "Settings")
	FPoseSearchExtrapolationParameters ExtrapolationParameters;

	UPROPERTY()
	FPoseSearchIndex SearchIndex;

	bool IsValidForIndexing() const;
	bool IsValidForSearch() const;

	UE::PoseSearch::FSearchResult Search(UE::PoseSearch::FSearchContext& SearchContext) const;

protected:
	FPoseSearchCost ComparePoses(int32 PoseIdx, UE::PoseSearch::EPoseComparisonFlags PoseComparisonFlags, TConstArrayView<float> QueryValues) const;

public: // UObject
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;

};

//////////////////////////////////////////////////////////////////////////
// Feature vector reader and builder

namespace UE::PoseSearch
{

enum class EPoseCandidateFlags : uint8
{
	None = 0,

	Valid_Pose = 1 << 0,
	Valid_ContinuingPose = 1 << 1,
	Valid_CurrentPose = 1 << 2,

	AnyValidMask = Valid_Pose | Valid_ContinuingPose | Valid_CurrentPose,

	DiscardedBy_PoseJumpThresholdTime = 1 << 3,
	DiscardedBy_PoseReselectHistory = 1 << 4,
	DiscardedBy_BlockTransition = 1 << 5,

	AnyDiscardedMask = DiscardedBy_PoseJumpThresholdTime | DiscardedBy_PoseReselectHistory | DiscardedBy_BlockTransition,
};
ENUM_CLASS_FLAGS(EPoseCandidateFlags);

/** Helper class for extracting and encoding features into a float buffer */
class POSESEARCH_API FFeatureVectorHelper
{
public:
	enum { EncodeQuatCardinality = 6 };
	static void EncodeQuat(TArrayView<float> Values, int32& DataOffset, const FQuat& Quat);
	static void EncodeQuat(TArrayView<float> Values, int32& DataOffset, TArrayView<const float> PrevValues, TArrayView<const float> CurValues, TArrayView<const float> NextValues, float LerpValue);
	static FQuat DecodeQuat(TArrayView<const float> Values, int32& DataOffset);

	enum { EncodeVectorCardinality = 3 };
	static void EncodeVector(TArrayView<float> Values, int32& DataOffset, const FVector& Vector);
	static void EncodeVector(TArrayView<float> Values, int32& DataOffset, TArrayView<const float> PrevValues, TArrayView<const float> CurValues, TArrayView<const float> NextValues, float LerpValue, bool bNormalize = false);
	static FVector DecodeVector(TArrayView<const float> Values, int32& DataOffset);

	enum { EncodeVector2DCardinality = 2 };
	static void EncodeVector2D(TArrayView<float> Values, int32& DataOffset, const FVector2D& Vector2D);
	static void EncodeVector2D(TArrayView<float> Values, int32& DataOffset, TArrayView<const float> PrevValues, TArrayView<const float> CurValues, TArrayView<const float> NextValues, float LerpValue);
	static FVector2D DecodeVector2D(TArrayView<const float> Values, int32& DataOffset);

	// populates MeanDeviations[DataOffset] ... MeanDeviations[DataOffset + Cardinality] with a single value the mean deviation calculated from a centered matrix
	static void ComputeMeanDeviations(float MinMeanDeviation, const Eigen::MatrixXd& CenteredPoseMatrix, Eigen::VectorXd& MeanDeviations, int32& DataOffset, int32 Cardinality);

	// populates MeanDeviations[DataOffset] ... MeanDeviations[DataOffset + Cardinality] with a single value
	static void SetMeanDeviations(float Deviation, Eigen::VectorXd& MeanDeviations, int32& DataOffset, int32 Cardinality);

private:
	static FQuat DecodeQuatInternal(TArrayView<const float> Values, int32 DataOffset);
	static FVector DecodeVectorInternal(TArrayView<const float> Values, int32 DataOffset);
	static FVector2D DecodeVector2DInternal(TArrayView<const float> Values, int32 DataOffset);
};

/**
* Records poses over time in a ring buffer.
* FFeatureVectorBuilder uses this to sample from the present or past poses according to the search schema.
*/
class POSESEARCH_API FPoseHistory
{
public:

	enum class ERootUpdateMode
	{
		RootMotionDelta,
		ComponentTransformDelta,
	};

	void Init(int32 InNumPoses, float InTimeHorizon);
	void Init(const FPoseHistory& History);

	bool Update(
		float SecondsElapsed,
		const FPoseContext& PoseContext,
		FTransform ComponentTransform,
		FText* OutError,
		ERootUpdateMode UpdateMode = ERootUpdateMode::RootMotionDelta);

	float GetSampleTimeInterval() const;
	float GetTimeHorizon() const { return TimeHorizon; }
	bool TrySampleLocalPose(float Time, const TArray<FBoneIndexType>* RequiredBones, TArray<FTransform>* LocalPose, FTransform* RootTransform) const;

private:

	struct FPose
	{
		FTransform RootTransform;
		TArray<FTransform> LocalTransforms;
		float Time = 0.0f;
	};
	TRingBuffer<FPose> Poses;
	float TimeHorizon = 0.0f;
};

struct FHistoricalPoseIndex
{
	bool operator==(const FHistoricalPoseIndex& Index) const
	{
		return PoseIndex == Index.PoseIndex && DatabaseKey == Index.DatabaseKey;
	}

	friend FORCEINLINE uint32 GetTypeHash(const FHistoricalPoseIndex& Index)
	{
		return HashCombineFast(::GetTypeHash(Index.PoseIndex), GetTypeHash(Index.DatabaseKey));
	}

	int32 PoseIndex = -1;
	FObjectKey DatabaseKey;
};

struct FPoseIndicesHistory
{
	void Update(const FSearchResult& SearchResult, float DeltaTime, float MaxTime);
	void Reset() { IndexToTime.Reset(); }
	TMap<FHistoricalPoseIndex, float> IndexToTime;
};

struct POSESEARCH_API FSearchContext
{
	EPoseSearchBooleanRequest QueryMirrorRequest = EPoseSearchBooleanRequest::Indifferent;
	UE::PoseSearch::FDebugDrawParams DebugDrawParams;
	UE::PoseSearch::FPoseHistory* History = nullptr;
	const FTrajectorySampleRange* Trajectory = nullptr;
	TObjectPtr<const USkeletalMeshComponent> OwningComponent = nullptr;
	UE::PoseSearch::FSearchResult CurrentResult;
	const FBoneContainer* BoneContainer = nullptr;
	const FGameplayTagContainer* ActiveTagsContainer = nullptr;
	float PoseJumpThresholdTime = 0.f;
	bool bIsTracing = false;
	bool bForceInterrupt = false;
	// can the continuing pose advance? (if not we skip evaluating it)
	bool bCanAdvance = true;

	FTransform TryGetTransformAndCacheResults(float SampleTime, const UPoseSearchSchema* Schema, int8 SchemaBoneIdx, bool& Error);
	void ClearCachedEntries();

	void ResetCurrentBestCost();
	void UpdateCurrentBestCost(const FPoseSearchCost& PoseSearchCost);
	float GetCurrentBestTotalCost() const { return CurrentBestTotalCost; }

	bool GetOrBuildQuery(const UPoseSearchDatabase* Database, FPoseSearchFeatureVectorBuilder& FeatureVectorBuilder);
	const FPoseSearchFeatureVectorBuilder* GetCachedQuery(const UPoseSearchDatabase* Database) const;

	bool IsCurrentResultFromDatabase(const UPoseSearchDatabase* Database) const;

	TConstArrayView<float> GetCurrentResultPrevPoseVector() const;
	TConstArrayView<float> GetCurrentResultPoseVector() const;
	TConstArrayView<float> GetCurrentResultNextPoseVector() const;

	static constexpr int8 SchemaRootBoneIdx = -1;

	const FPoseIndicesHistory* PoseIndicesHistory = nullptr;

private:
	struct FCachedEntry
	{
		float SampleTime = 0.f;

		// associated transform to BoneIndexType in ComponentSpace (except for the root bone stored in global space)
		FTransform Transform;

		// if -1 it represents the root bone
		FBoneIndexType BoneIndexType = -1;
	};

	// @todo: make it a fixed size array (or hash map if we end up having many CachedEntry) to avoid allocations
	TArray<FCachedEntry> CachedEntries;
	
	struct FCachedQuery
	{
		const UPoseSearchDatabase* Database = nullptr;
		FPoseSearchFeatureVectorBuilder FeatureVectorBuilder;
	};

	TArray<FCachedQuery> CachedQueries;

	float CurrentBestTotalCost = MAX_flt;

#if UE_POSE_SEARCH_TRACE_ENABLED

public:
	struct FPoseCandidate
	{
		FPoseSearchCost Cost;
		int32 PoseIdx = 0;
		const UPoseSearchDatabase* Database = nullptr;
		EPoseCandidateFlags PoseCandidateFlags = EPoseCandidateFlags::None;

		bool operator<(const FPoseCandidate& Other) const { return Other.Cost < Cost; } // Reverse compare because BestCandidates is a max heap
		bool operator==(const FSearchResult& SearchResult) const { return (PoseIdx == SearchResult.PoseIdx) && (Database == SearchResult.Database.Get()); }
	};

	struct FBestPoseCandidates : private TArray<FPoseCandidate>
	{
		typedef TArray<FPoseCandidate> Super;
		using Super::IsEmpty;

		int32 MaxPoseCandidates = 100;

		void Add(const FPoseSearchCost& Cost, int32 PoseIdx, const UPoseSearchDatabase* Database, EPoseCandidateFlags PoseCandidateFlags)
		{
			if (Num() < MaxPoseCandidates || Cost < HeapTop().Cost)
			{
				while (Num() >= MaxPoseCandidates)
				{
					ElementType Unused;
					Pop(Unused);
				}

				FSearchContext::FPoseCandidate PoseCandidate;
				PoseCandidate.Cost = Cost;
				PoseCandidate.PoseIdx = PoseIdx;
				PoseCandidate.Database = Database;
				PoseCandidate.PoseCandidateFlags = PoseCandidateFlags;
				HeapPush(PoseCandidate);
			}
		}

		void Pop(FPoseCandidate& OutItem)
		{
			HeapPop(OutItem, false);
		}
	};
	
	FBestPoseCandidates BestCandidates;
#endif
};

//////////////////////////////////////////////////////////////////////////
// Main PoseSearch API

POSESEARCH_API void DrawFeatureVector(const FDebugDrawParams& DrawParams, TArrayView<const float> PoseVector);
POSESEARCH_API void DrawFeatureVector(const FDebugDrawParams& DrawParams, int32 PoseIdx);
POSESEARCH_API void DrawSearchIndex(const FDebugDrawParams& DrawParams);
POSESEARCH_API void CompareFeatureVectors(TConstArrayView<float> A, TConstArrayView<float> B, TConstArrayView<float> WeightsSqrt, TArrayView<float> Result);

/**
* Creates a pose search index for an animation sequence
* 
* @param Sequence			The input sequence create a search index for
* @param SequenceMetaData	The input sequence indexing info and output search index
* 
* @return Whether the index was built successfully
*/
POSESEARCH_API bool BuildIndex(const UAnimSequence* Sequence, UPoseSearchSequenceMetaData* SequenceMetaData);

/**
* Creates a pose search index for a collection of animations
* 
* @param Database	The input collection of animations and output search index
* 
* @return Whether the index was built successfully
*/
POSESEARCH_API bool BuildIndex(const UPoseSearchDatabase* Database, FPoseSearchIndex& OutSearchIndex);

} // namespace UE::PoseSearch

UENUM()
enum class EPoseSearchPostSearchStatus : uint8
{
	// Continue looking for results 
	Continue,

	// Halt and return the best result
	Stop
};

USTRUCT(BlueprintType, Category = "Animation|Pose Search")
struct POSESEARCH_API FPoseSearchDatabaseSetEntry
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Settings)
	TObjectPtr<UPoseSearchSearchableAsset> Searchable = nullptr;

	UPROPERTY(EditAnywhere, Category = Settings)
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, Category = Settings)
	EPoseSearchPostSearchStatus PostSearchStatus = EPoseSearchPostSearchStatus::Continue;
};

/** A data asset which holds a collection searchable assets. */
UCLASS(BlueprintType, Category = "Animation|Pose Search", Experimental)
class POSESEARCH_API UPoseSearchDatabaseSet : public UPoseSearchSearchableAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Settings)
	TArray<FPoseSearchDatabaseSetEntry> AssetsToSearch;

	// if there's a valid continuing pose and bEvaluateContinuingPoseFirst is true, the continuing pose will be evaluated as first search,
	// otherwise it'll be evaluated with the related database: if the database is not active the continuing pose evaluation will be skipped
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bEvaluateContinuingPoseFirst = true;

public:
	virtual UE::PoseSearch::FSearchResult Search(UE::PoseSearch::FSearchContext& SearchContext) const override;
};
