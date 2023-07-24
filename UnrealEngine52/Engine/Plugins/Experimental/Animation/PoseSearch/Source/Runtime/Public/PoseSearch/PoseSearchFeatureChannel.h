// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IO/IoHash.h"
#include "Interfaces/Interface_BoneReferenceSkeletonProvider.h"
#include "PoseSearchFeatureChannel.generated.h"

class UPoseSearchSchema;
struct FPoseSearchPoseMetadata;
struct FPoseSearchFeatureVectorBuilder;

UENUM(BlueprintType)
enum class EInputQueryPose : uint8
{
	// use character pose to compose the query
	UseCharacterPose,

	// if available reuse continuing pose from the database to compose the query or else UseCharacterPose
	UseContinuingPose,

	// if available reuse and interpolate continuing pose from the database to compose the query or else UseCharacterPose
	UseInterpolatedContinuingPose,

	Num UMETA(Hidden),
	Invalid = Num UMETA(Hidden)
};

namespace UE::PoseSearch
{

struct FDebugDrawParams;
struct FSearchContext;
class IAssetIndexer;

/** Helper class for extracting and encoding features into a float buffer */
class POSESEARCH_API FFeatureVectorHelper
{
public:
	enum { EncodeQuatCardinality = 6 };
	static void EncodeQuat(TArrayView<float> Values, int32& DataOffset, const FQuat& Quat);
	static void EncodeQuat(TArrayView<float> Values, int32& DataOffset, TConstArrayView<float> PrevValues, TConstArrayView<float> CurValues, TConstArrayView<float> NextValues, float LerpValue);
	static FQuat DecodeQuat(TConstArrayView<float> Values, int32& DataOffset);
	static FQuat DecodeQuatAtOffset(TConstArrayView<float> Values, int32 DataOffset);

	enum { EncodeVectorCardinality = 3 };
	static void EncodeVector(TArrayView<float> Values, int32& DataOffset, const FVector& Vector);
	static void EncodeVector(TArrayView<float> Values, int32& DataOffset, TConstArrayView<float> PrevValues, TConstArrayView<float> CurValues, TConstArrayView<float> NextValues, float LerpValue, bool bNormalize = false);
	static FVector DecodeVector(TConstArrayView<float> Values, int32& DataOffset);
	static FVector DecodeVectorAtOffset(TConstArrayView<float> Values, int32 DataOffset);

	enum { EncodeVector2DCardinality = 2 };
	static void EncodeVector2D(TArrayView<float> Values, int32& DataOffset, const FVector2D& Vector2D);
	static void EncodeVector2D(TArrayView<float> Values, int32& DataOffset, TConstArrayView<float> PrevValues, TConstArrayView<float> CurValues, TConstArrayView<float> NextValues, float LerpValue);
	static FVector2D DecodeVector2D(TConstArrayView<float> Values, int32& DataOffset);
	static FVector2D DecodeVector2DAtOffset(TConstArrayView<float> Values, int32 DataOffset);

	enum { EncodeFloatCardinality = 1 };
	static void EncodeFloat(TArrayView<float> Values, int32& DataOffset, const float Value);
	static void EncodeFloat(TArrayView<float> Values, int32& DataOffset, TConstArrayView<float> PrevValues, TConstArrayView<float> CurValues, TConstArrayView<float> NextValues, float LerpValue);
	static float DecodeFloat(TConstArrayView<float> Values, int32& DataOffset);
	static float DecodeFloatAtOffset(TConstArrayView<float> Values, int32 DataOffset);
};

} // namespace UE::PoseSearch

class POSESEARCH_API IPoseFilter
{
public:
	virtual ~IPoseFilter() {}

	// if true this filter will be evaluated
	virtual bool IsPoseFilterActive() const { return false; }
	
	// if it returns false the pose candidate will be discarded
	virtual bool IsPoseValid(TConstArrayView<float> PoseValues, TConstArrayView<float> QueryValues, int32 PoseIdx, const FPoseSearchPoseMetadata& Metadata) const { return true; }
};

//////////////////////////////////////////////////////////////////////////
// Feature channels interface
UCLASS(Abstract, BlueprintType, EditInlineNew)
class POSESEARCH_API UPoseSearchFeatureChannel : public UObject, public IBoneReferenceSkeletonProvider, public IPoseFilter
{
	GENERATED_BODY()

public:
	int32 GetChannelCardinality() const { checkSlow(ChannelCardinality >= 0); return ChannelCardinality; }
	int32 GetChannelDataOffset() const { checkSlow(ChannelDataOffset >= 0); return ChannelDataOffset; }

	// Called during UPoseSearchSchema::Finalize to prepare the schema for this channel
	virtual void Finalize(UPoseSearchSchema* Schema) PURE_VIRTUAL(UPoseSearchFeatureChannel::Finalize, );
	
	// Called at database build time to collect feature weights.
	// Weights is sized to the cardinality of the schema and the feature channel should write
	// its weights at the channel's data offset. Channels should provide a weight for each dimension.
	virtual void FillWeights(TArray<float>& Weights) const PURE_VIRTUAL(UPoseSearchFeatureChannel::FillWeights, );

	// Called at database build time to populate pose vectors with this channel's data
	virtual void IndexAsset(UE::PoseSearch::IAssetIndexer& Indexer, TArrayView<float> FeatureVectorTable) const PURE_VIRTUAL(UPoseSearchFeatureChannel::IndexAsset, );

	// Called at runtime to add this channel's data to the query pose vector
	virtual void BuildQuery(UE::PoseSearch::FSearchContext& SearchContext, FPoseSearchFeatureVectorBuilder& InOutQuery) const PURE_VIRTUAL(UPoseSearchFeatureChannel::BuildQuery, );

	// API called before DebugDraw to collect shared channel informations such as decoded positions form the PoseVector
	virtual void PreDebugDraw(UE::PoseSearch::FDebugDrawParams& DrawParams, TConstArrayView<float> PoseVector) const {}

	// Draw this channel's data for the given pose vector
	virtual void DebugDraw(const UE::PoseSearch::FDebugDrawParams& DrawParams, TConstArrayView<float> PoseVector) const PURE_VIRTUAL(UPoseSearchFeatureChannel::DebugDraw, );

	// UPoseSearchFeatureChannels can hold sub channels
	virtual TArrayView<TObjectPtr<UPoseSearchFeatureChannel>> GetSubChannels() { return TArrayView<TObjectPtr<UPoseSearchFeatureChannel>>(); }
	virtual TConstArrayView<TObjectPtr<UPoseSearchFeatureChannel>> GetSubChannels() const { return TConstArrayView<TObjectPtr<UPoseSearchFeatureChannel>>(); }

#if WITH_EDITOR
	// returns the FString used editor side to identify this UPoseSearchFeatureChannel (for instance in the pose search debugger)
	virtual FString GetLabel() const;
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