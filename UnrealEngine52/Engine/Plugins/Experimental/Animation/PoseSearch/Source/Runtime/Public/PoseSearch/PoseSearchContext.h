// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearch/PoseSearchDefines.h"
#include "PoseSearch/PoseSearchIndex.h"
#include "PoseSearch/PoseSearchResult.h"

struct FGameplayTagContainer;
struct FTrajectorySampleRange;
class UPoseSearchDatabase;
class UPoseSearchSchema;

namespace UE::PoseSearch
{

class FPoseHistory;
struct FPoseIndicesHistory;

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
	DiscardedBy_PoseFilter = 1 << 6,

	AnyDiscardedMask = DiscardedBy_PoseJumpThresholdTime | DiscardedBy_PoseReselectHistory | DiscardedBy_BlockTransition | DiscardedBy_PoseFilter,
};
ENUM_CLASS_FLAGS(EPoseCandidateFlags);

template<typename FTransformType>
struct POSESEARCH_API FCachedTransform
{
	FCachedTransform()
		: SampleTime(0.f)
		, BoneIndexType(-1)
		, Transform()
	{
	}

	FCachedTransform(float InSampleTime, FBoneIndexType InBoneIndexType, const FTransformType& InTransform)
		: SampleTime(InSampleTime)
		, BoneIndexType(InBoneIndexType)
		, Transform(InTransform)
	{
	}

	float SampleTime;
	
	// if -1 it represents the root bone
	FBoneIndexType BoneIndexType;

	// associated transform to BoneIndexType in ComponentSpace (except for the root bone stored in global space)
	FTransformType Transform;
};

template<typename FTransformType>
struct FCachedTransforms
{
	const FCachedTransform<FTransformType>* Find(float SampleTime, FBoneIndexType BoneIndexType) const
	{
		// @todo: use an hashmap if we end up having too many entries
		return CachedTransforms.FindByPredicate([SampleTime, BoneIndexType](const FCachedTransform<FTransformType>& CachedTransform)
			{
				return CachedTransform.SampleTime == SampleTime && CachedTransform.BoneIndexType == BoneIndexType;
			});
	}

	void Add(float SampleTime, FBoneIndexType BoneIndexType, const FTransformType& Transform)
	{
		CachedTransforms.Emplace(SampleTime, BoneIndexType, Transform);
	}

	void Reset()
	{
		CachedTransforms.Reset();
	}

private:
	TArray<FCachedTransform<FTransformType>, TInlineAllocator<64>> CachedTransforms;
};

// @todo: FDebugDrawParams should be enclosed with #if ENABLE_DRAW_DEBUG
struct POSESEARCH_API FDebugDrawParams
{
	const UWorld* World = nullptr;
	const UPoseSearchDatabase* Database = nullptr;
	EDebugDrawFlags Flags = EDebugDrawFlags::None;

	float DefaultLifeTime = 5.f;
	float PointSize = 1.f;

	FTransform RootTransform = FTransform::Identity;

	// Optional Mesh for gathering SocketTransform(s)
	TWeakObjectPtr<const USkinnedMeshComponent> Mesh = nullptr;

	bool CanDraw() const;
	FColor GetColor(int32 ColorPreset) const;
	const FPoseSearchIndex* GetSearchIndex() const;
	const UPoseSearchSchema* GetSchema() const;

	void ClearCachedPositions();
	void AddCachedPosition(float TimeOffset, int8 SchemaBoneIdx, const FVector& Position);
	FVector GetCachedPosition(float TimeOffset, int8 SchemaBoneIdx = -1) const;

private:
	FCachedTransforms<FVector> CachedPositions;
};

POSESEARCH_API void DrawFeatureVector(FDebugDrawParams& DrawParams, TConstArrayView<float> PoseVector);
POSESEARCH_API void DrawFeatureVector(FDebugDrawParams& DrawParams, int32 PoseIdx);

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

	FTransform TryGetTransformAndCacheResults(float SampleTime, const UPoseSearchSchema* Schema, int8 SchemaBoneIdx);
	void ClearCachedEntries();

	void ResetCurrentBestCost();
	void UpdateCurrentBestCost(const FPoseSearchCost& PoseSearchCost);
	float GetCurrentBestTotalCost() const { return CurrentBestTotalCost; }

	void GetOrBuildQuery(const UPoseSearchDatabase* Database, FPoseSearchFeatureVectorBuilder& FeatureVectorBuilder);
	const FPoseSearchFeatureVectorBuilder* GetCachedQuery(const UPoseSearchDatabase* Database) const;

	bool IsCurrentResultFromDatabase(const UPoseSearchDatabase* Database) const;

	TConstArrayView<float> GetCurrentResultPrevPoseVector() const;
	TConstArrayView<float> GetCurrentResultPoseVector() const;
	TConstArrayView<float> GetCurrentResultNextPoseVector() const;

	static constexpr int8 SchemaRootBoneIdx = -1;

	const FPoseIndicesHistory* PoseIndicesHistory = nullptr;

private:
	FCachedTransforms<FTransform> CachedTransforms;
	
	struct FCachedQuery
	{
		// @todo: why do we need to hold on to a Database? isn't the FeatureVectorBuilder.Schema enough?
		const UPoseSearchDatabase* Database = nullptr;
		FPoseSearchFeatureVectorBuilder FeatureVectorBuilder;
	};

	TArray<FCachedQuery, TInlineAllocator<8>> CachedQueries;

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

} // namespace UE::PoseSearch
