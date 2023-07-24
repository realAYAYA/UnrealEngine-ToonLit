// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimNodeMessages.h"
#include "BoneIndices.h"
#include "Containers/RingBuffer.h"
#include "UObject/ObjectKey.h"

struct FPoseContext;

namespace UE::PoseSearch
{

struct FSearchResult;

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
	bool Update(float SecondsElapsed, const FPoseContext& PoseContext, FTransform ComponentTransform, FText* OutError, ERootUpdateMode UpdateMode = ERootUpdateMode::RootMotionDelta);
	float GetSampleTimeInterval() const;
	float GetTimeHorizon() const { return TimeHorizon; }
	bool TrySampleLocalPose(float Time, const TArray<FBoneIndexType>* RequiredBones, TArray<FTransform>* LocalPose, FTransform* RootTransform) const;

private:

	struct FPose
	{
		FTransform RootTransform; // @todo: remove RootTransform: this should be unnecessary, since FTrajectorySampleRange already contains the past as well as the prediction for the RootTransform
		TArray<FTransform> LocalTransforms;
		float Time = 0.0f;
	};
	TRingBuffer<FPose> Poses;
	float TimeHorizon = 0.0f;
};

class IPoseHistoryProvider : public UE::Anim::IGraphMessage
{
	DECLARE_ANIMGRAPH_MESSAGE(IPoseHistoryProvider);
public:
	virtual const FPoseHistory& GetPoseHistory() const = 0;
	virtual FPoseHistory& GetPoseHistory() = 0;
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

	int32 PoseIndex = INDEX_NONE;
	FObjectKey DatabaseKey;
};

struct FPoseIndicesHistory
{
	void Update(const FSearchResult& SearchResult, float DeltaTime, float MaxTime);
	void Reset() { IndexToTime.Reset(); }
	TMap<FHistoricalPoseIndex, float> IndexToTime;
};

} // namespace UE::PoseSearch


