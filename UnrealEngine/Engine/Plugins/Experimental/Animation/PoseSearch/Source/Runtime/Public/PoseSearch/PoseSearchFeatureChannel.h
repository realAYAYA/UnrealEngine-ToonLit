// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IO/IoHash.h"
#include "Interfaces/Interface_BoneReferenceSkeletonProvider.h"
#include "DrawDebugHelpers.h"
#include "PoseSearchFeatureChannel.generated.h"

class UPoseSearchSchema;

UENUM()
enum class EComponentStrippingVector : uint8
{
	// No component stripping.
	None,

	// Stripping X and Y components (matching only on the horizontal plane).
	StripXY,

	// Stripping Z (matching only vertically - caring only about the height of the feature).
	StripZ,
};

UENUM()
enum class EInputQueryPose : uint8
{
	// Use character pose to compose the query.
	UseCharacterPose,

	// If available reuse continuing pose from the database to compose the query, or else UseCharacterPose.
	UseContinuingPose,

	// If available reuse and interpolate continuing pose from the database to compose the query, or else UseCharacterPose.
	UseInterpolatedContinuingPose,
};

// this enumeration controls the channel sampling time:
// for example if a channel specifies a bone and an origin bone (used to generate the reference system of the features associated to the bone),
// bone and origin bone will be evaluated at potentially different times:
UENUM()
enum class EPermutationTimeType : uint8
{
	// Bone and origin bone are sampled at the same sample time (plus eventual SampleTimeOffset for the bone):
	// it's defined as the current animation evaluation time.
	UseSampleTime,

	// Bone and origin bone are sampled at the same permutation time (plus eventual SampleTimeOffset for the bone):
	// it's defined as SamplingTime (as UseSampleTime) + Schema->PermutationsTimeOffset + PermutationIndex / Schema->PermutationsSampleRate
	// where PermutationIndex is in range [0, Schema->NumberOfPermutations).
	UsePermutationTime,

	// Bone is evaluated at sample time (and plus eventual SampleTimeOffset) and origin bone is evaluated at permutation time.
	UseSampleToPermutationTime,
};

namespace UE::PoseSearch
{

struct FDebugDrawParams;
struct FFeatureVectorBuilder;
struct FSearchContext;
struct FPoseMetadata;

#if WITH_EDITOR
class FAssetIndexer;
#endif // WITH_EDITOR

/** Helper class for extracting and encoding features into a float buffer */
class POSESEARCH_API FFeatureVectorHelper
{
public:
	static int32 GetVectorCardinality(EComponentStrippingVector ComponentStrippingVector);
	static void EncodeVector(TArrayView<float> Values, int32 DataOffset, const FVector& Vector, EComponentStrippingVector ComponentStrippingVector);
	static FVector DecodeVector(TConstArrayView<float> Values, int32 DataOffset, EComponentStrippingVector ComponentStrippingVector);

	static void EncodeVector2D(TArrayView<float> Values, int32 DataOffset, const FVector2D& Vector2D);
	static FVector2D DecodeVector2D(TConstArrayView<float> Values, int32 DataOffset);

	static void EncodeFloat(TArrayView<float> Values, int32 DataOffset, const float Value);
	static float DecodeFloat(TConstArrayView<float> Values, int32 DataOffset);

	static void Copy(TArrayView<float> Values, int32 DataOffset, int32 DataCardinality, TConstArrayView<float> OriginValues);
};

} // namespace UE::PoseSearch

class POSESEARCH_API IPoseSearchFilter
{
public:
	virtual ~IPoseSearchFilter() {}

	// if true this filter will be evaluated
	virtual bool IsFilterActive() const { return false; }
	
	// if it returns false the pose candidate will be discarded
	virtual bool IsFilterValid(TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues, int32 PoseIdx, const UE::PoseSearch::FPoseMetadata& Metadata) const { return true; }
};

//////////////////////////////////////////////////////////////////////////
// Feature channels interface
UCLASS(Abstract, BlueprintType, EditInlineNew)
class POSESEARCH_API UPoseSearchFeatureChannel : public UObject, public IBoneReferenceSkeletonProvider, public IPoseSearchFilter
{
	GENERATED_BODY()

public:
	int32 GetChannelCardinality() const { checkSlow(ChannelCardinality >= 0); return ChannelCardinality; }
	int32 GetChannelDataOffset() const { checkSlow(ChannelDataOffset >= 0); return ChannelDataOffset; }

	// Called during UPoseSearchSchema::Finalize to prepare the schema for this channel
	virtual void Finalize(UPoseSearchSchema* Schema) PURE_VIRTUAL(UPoseSearchFeatureChannel::Finalize, );
	
	// Called at runtime to add this channel's data to the query pose vector
	virtual void BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, UE::PoseSearch::FFeatureVectorBuilder& InOutQuery) const PURE_VIRTUAL(UPoseSearchFeatureChannel::BuildQuery, );

	// UPoseSearchFeatureChannels can hold sub channels
	virtual TArrayView<TObjectPtr<UPoseSearchFeatureChannel>> GetSubChannels() { return TArrayView<TObjectPtr<UPoseSearchFeatureChannel>>(); }
	virtual TConstArrayView<TObjectPtr<UPoseSearchFeatureChannel>> GetSubChannels() const { return TConstArrayView<TObjectPtr<UPoseSearchFeatureChannel>>(); }

	// @todo: should this API be under ENABLE_DRAW_DEBUG?
	virtual void AddDependentChannels(UPoseSearchSchema* Schema) const {}

	virtual EPermutationTimeType GetPermutationTimeType() const { return EPermutationTimeType::UseSampleTime; }
	static void GetPermutationTimeOffsets(EPermutationTimeType PermutationTimeType, float DesiredPermutationTimeOffset, float& OutPermutationSampleTimeOffset, float& OutPermutationOriginTimeOffset);

#if ENABLE_DRAW_DEBUG
	// Draw this channel's data for the given pose vector
	virtual void DebugDraw(const UE::PoseSearch::FDebugDrawParams& DrawParams, TConstArrayView<float> PoseVector) const {}
#endif //ENABLE_DRAW_DEBUG

#if WITH_EDITOR
	// Called at database build time to collect feature weights.
	// Weights is sized to the cardinality of the schema and the feature channel should write
	// its weights at the channel's data offset. Channels should provide a weight for each dimension.
	virtual void FillWeights(TArrayView<float> Weights) const PURE_VIRTUAL(UPoseSearchFeatureChannel::FillWeights, );

	// Called at database build time to populate pose vectors with this channel's data
	virtual void IndexAsset(UE::PoseSearch::FAssetIndexer& Indexer) const PURE_VIRTUAL(UPoseSearchFeatureChannel::IndexAsset, );

	// returns the FString used editor side to identify this UPoseSearchFeatureChannel (for instance in the pose search debugger)
	virtual FString GetLabel() const;
	virtual bool CanBeNormalizedWith(const UPoseSearchFeatureChannel* Other) const;
	const UPoseSearchSchema* GetSchema() const;
#endif

private:
	// IBoneReferenceSkeletonProvider interface
	// Note this function is exclusively for FBoneReference details customization
	USkeleton* GetSkeleton(bool& bInvalidSkeletonIsError, const IPropertyHandle* PropertyHandle) override;

protected:
	friend class ::UPoseSearchSchema;

	UPROPERTY(Transient)
	int32 ChannelDataOffset = INDEX_NONE;

	UPROPERTY(Transient)
	int32 ChannelCardinality = INDEX_NONE;
};