// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DrawDebugHelpers.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "PoseSearch/PoseSearchFeatureChannel.h"
#include "PoseSearch/PoseSearchIndex.h"
#include "PoseSearch/PoseSearchResult.h"

struct FPoseSearchQueryTrajectory;
class UPoseSearchDatabase;
class UPoseSearchFeatureChannel_Position;

namespace UE::PoseSearch
{

struct FPoseIndicesHistory;
struct IPoseHistory;

enum class EDebugDrawFlags : uint32
{
	None = 0,

	// Draw using Query colors form the schema / config
	DrawQuery = 1 << 1,
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
	DiscardedBy_Search = 1 << 7,

	AnyDiscardedMask = DiscardedBy_PoseJumpThresholdTime | DiscardedBy_PoseReselectHistory | DiscardedBy_BlockTransition | DiscardedBy_PoseFilter | DiscardedBy_Search,
};
ENUM_CLASS_FLAGS(EPoseCandidateFlags);

template<typename FTransformType>
struct POSESEARCH_API FCachedTransform
{
	FCachedTransform()
		: SampleTime(0.f)
		, BoneIndexType(RootBoneIndexType)
		, Transform(FTransformType::Identity)
	{
	}

	FCachedTransform(float InSampleTime, FBoneIndexType InBoneIndexType, const FTransformType& InTransform)
		: SampleTime(InSampleTime)
		, BoneIndexType(InBoneIndexType)
		, Transform(InTransform)
	{
	}

	float SampleTime = 0.f;
	
	FBoneIndexType BoneIndexType = RootBoneIndexType;

	// associated transform to BoneIndexType in ComponentSpace (except for the root bone stored in global space)
	FTransformType Transform = FTransformType::Identity;
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

	bool IsEmpty() const
	{
		return CachedTransforms.IsEmpty();
	}

private:
	TArray<FCachedTransform<FTransformType>, TInlineAllocator<64>> CachedTransforms;
};

#if ENABLE_DRAW_DEBUG
struct POSESEARCH_API FDebugDrawParams
{
	FDebugDrawParams(FAnimInstanceProxy* InAnimInstanceProxy, const FTransform& InRootMotionTransform, const UPoseSearchDatabase* InDatabase, EDebugDrawFlags InFlags = EDebugDrawFlags::None);
	FDebugDrawParams(const UWorld* InWorld, const USkinnedMeshComponent* InMesh, const FTransform& InRootMotionTransform, const UPoseSearchDatabase* InDatabase, EDebugDrawFlags InFlags = EDebugDrawFlags::None);

	const FSearchIndex* GetSearchIndex() const;
	const UPoseSearchSchema* GetSchema() const;

	FVector ExtractPosition(TConstArrayView<float> PoseVector, const UPoseSearchFeatureChannel_Position* Position) const;
	FVector ExtractPosition(TConstArrayView<float> PoseVector, float SampleTimeOffset, int8 SchemaBoneIdx = RootSchemaBoneIdx, EPermutationTimeType PermutationTimeType = EPermutationTimeType::UseSampleTime) const;
	const FTransform& GetRootTransform() const;

	void DrawLine(const FVector& LineStart, const FVector& LineEnd, const FColor& Color, float Thickness = 0.f) const;
	void DrawPoint(const FVector& Position, const FColor& Color, float Thickness = 6.f) const;
	void DrawCircle(const FMatrix& TransformMatrix, float Radius, int32 Segments, const FColor& Color, float Thickness = 1.f) const;
	void DrawCentripetalCatmullRomSpline(TConstArrayView<FVector> Points, TConstArrayView<FColor> Colors, float Alpha, int32 NumSamplesPerSegment, float Thickness = 1.f) const;
	
	void DrawFeatureVector(TConstArrayView<float> PoseVector);
	void DrawFeatureVector(int32 PoseIdx);

private:
	bool CanDraw() const;

	FAnimInstanceProxy* AnimInstanceProxy = nullptr;
	const UWorld* World = nullptr;
	const USkinnedMeshComponent* Mesh = nullptr;
	const FTransform* RootMotionTransform = nullptr;
	const UPoseSearchDatabase* Database = nullptr;
	EDebugDrawFlags Flags = EDebugDrawFlags::None;
};

#endif // ENABLE_DRAW_DEBUG

struct POSESEARCH_API FSearchContext
{
	FSearchContext(const FPoseSearchQueryTrajectory* InTrajectory, const IPoseHistory* InHistory, float InDesiredPermutationTimeOffset, const FPoseIndicesHistory* InPoseIndicesHistory = nullptr,
		const FSearchResult& InCurrentResult = FSearchResult(), float InPoseJumpThresholdTime = 0.f, bool bInForceInterrupt = false);

	FQuat GetSampleRotation(float SampleTimeOffset, const UPoseSearchSchema* Schema, int8 SchemaSampleBoneIdx = RootSchemaBoneIdx, int8 SchemaOriginBoneIdx = RootSchemaBoneIdx, bool bUseHistoryRoot = false, EPermutationTimeType PermutationTimeType = EPermutationTimeType::UseSampleTime);
	FVector GetSamplePosition(float SampleTimeOffset, const UPoseSearchSchema* Schema, int8 SchemaSampleBoneIdx = RootSchemaBoneIdx, int8 SchemaOriginBoneIdx = RootSchemaBoneIdx, bool bUseHistoryRoot = false, EPermutationTimeType PermutationTimeType = EPermutationTimeType::UseSampleTime);
	FVector GetSampleVelocity(float SampleTimeOffset, const UPoseSearchSchema* Schema, int8 SchemaSampleBoneIdx = RootSchemaBoneIdx, int8 SchemaOriginBoneIdx = RootSchemaBoneIdx, bool bUseCharacterSpaceVelocities = true, bool bUseHistoryRoot = false, EPermutationTimeType PermutationTimeType = EPermutationTimeType::UseSampleTime);

	void ClearCachedEntries();

	void ResetCurrentBestCost();
	void UpdateCurrentBestCost(const FPoseSearchCost& PoseSearchCost);
	float GetCurrentBestTotalCost() const { return CurrentBestTotalCost; }

	const FFeatureVectorBuilder& GetOrBuildQuery(const UPoseSearchSchema* Schema);
	const FFeatureVectorBuilder* GetCachedQuery(const UPoseSearchSchema* Schema) const;

	bool IsCurrentResultFromDatabase(const UPoseSearchDatabase* Database) const;

	TConstArrayView<float> GetCurrentResultPoseVector() const { return CurrentResultPoseVector; }

	const FSearchResult& GetCurrentResult() const { return CurrentResult; }
	float GetPoseJumpThresholdTime() const { return PoseJumpThresholdTime; }
	const FPoseIndicesHistory* GetPoseIndicesHistory() const { return PoseIndicesHistory; }
	bool IsHistoryValid() const { return History != nullptr; }
	float GetDesiredPermutationTimeOffset() const { return DesiredPermutationTimeOffset; }
	bool IsTrajectoryValid() const { return Trajectory != nullptr; }
	bool IsForceInterrupt() const { return bForceInterrupt; }
	FTransform GetRootAtTime(float Time, bool bUseHistoryRoot = false, bool bExtrapolate = true) const;

private:
	FTransform GetTransform(float SampleTime, const UPoseSearchSchema* Schema, int8 SchemaBoneIdx = RootSchemaBoneIdx, bool bUseHistoryRoot = false);
	FTransform GetComponentSpaceTransform(float SampleTime, const UPoseSearchSchema* Schema, int8 SchemaSampleBoneIdx = RootSchemaBoneIdx);
	FVector GetSamplePositionInternal(float SampleTime, float OriginTime, const UPoseSearchSchema* Schema, int8 SchemaSampleBoneIdx = RootSchemaBoneIdx, int8 SchemaOriginBoneIdx = RootSchemaBoneIdx, bool bUseHistoryRoot = false);
	FQuat GetSampleRotationInternal(float SampleTime, float OriginTime, const UPoseSearchSchema* Schema, int8 SchemaSampleBoneIdx = RootSchemaBoneIdx, int8 SchemaOriginBoneIdx = RootSchemaBoneIdx, bool bUseHistoryRoot = false);

	const FPoseSearchQueryTrajectory* Trajectory = nullptr;
	const IPoseHistory* History = nullptr;
	float DesiredPermutationTimeOffset = 0.f;
	const FPoseIndicesHistory* PoseIndicesHistory = nullptr;
	FSearchResult CurrentResult;
	float PoseJumpThresholdTime = 0.f;
	bool bForceInterrupt = false;

	TConstArrayView<float> CurrentResultPoseVector;
	TStackAlignedArray<float> CurrentResultPoseVectorData;

	// transforms cached in component space
	FCachedTransforms<FTransform> CachedTransforms;
	TArray<FFeatureVectorBuilder, TInlineAllocator<PreallocatedCachedQueriesNum>> CachedQueries;

	float CurrentBestTotalCost = MAX_flt;
	
#if UE_POSE_SEARCH_TRACE_ENABLED

public:
	struct FPoseCandidateBase
	{
		FPoseSearchCost Cost;
		int32 PoseIdx = 0;
		const UPoseSearchDatabase* Database = nullptr;

		bool operator<(const FPoseCandidateBase& Other) const { return Other.Cost < Cost; } // Reverse compare because BestCandidates is a max heap
		bool operator==(const FSearchResult& SearchResult) const { return (PoseIdx == SearchResult.PoseIdx) && (Database == SearchResult.Database.Get()); }
	};

	struct FPoseCandidate : public FPoseCandidateBase
	{
		EPoseCandidateFlags PoseCandidateFlags = EPoseCandidateFlags::None;
	};

	struct FBestPoseCandidates
	{
		void Add(const UPoseSearchDatabase* Database)
		{
			SearchedDatabases.Add(Database);
		}

		void Add(const FPoseSearchCost& Cost, int32 PoseIdx, const UPoseSearchDatabase* Database, EPoseCandidateFlags PoseCandidateFlags)
		{
			if (EPoseCandidateFlags* PoseIdxPoseCandidateFlags = PoseIdxToFlags.Find(PoseIdx))
			{
				*PoseIdxPoseCandidateFlags |= PoseCandidateFlags;
			}
			else if (PoseCandidateHeap.Num() < MaxPoseCandidates || Cost < PoseCandidateHeap.HeapTop().Cost)
			{
				bool bPoppedContinuingPoseCandidate = false;
				FPoseCandidate ContinuingPoseCandidate;
				while (PoseCandidateHeap.Num() >= MaxPoseCandidates)
				{
					FPoseCandidate PoppedPoseCandidate;
					Pop(PoppedPoseCandidate);

					if (EnumHasAnyFlags(PoppedPoseCandidate.PoseCandidateFlags, EPoseCandidateFlags::Valid_ContinuingPose))
					{
						// we can only have one continuing pose candidate
						check(!bPoppedContinuingPoseCandidate);
						ContinuingPoseCandidate = PoppedPoseCandidate;
						bPoppedContinuingPoseCandidate = true;
					}					
				}

				if (bPoppedContinuingPoseCandidate)
				{
					// if we popped the continuing pose candidate, we make some space fir it and push it back
					FPoseCandidate PoppedPoseCandidate;
					Pop(PoppedPoseCandidate);
					PoseCandidateHeap.HeapPush(ContinuingPoseCandidate);
					PoseIdxToFlags.Add(ContinuingPoseCandidate.PoseIdx, PoppedPoseCandidate.PoseCandidateFlags);
				}

				FPoseCandidate PoseCandidate;
				PoseCandidate.Cost = Cost;
				PoseCandidate.PoseIdx = PoseIdx;
				PoseCandidate.Database = Database;
				
				PoseCandidateHeap.HeapPush(PoseCandidate);
				PoseIdxToFlags.Add(PoseIdx, PoseCandidateFlags);
			}

			Add(Database);
		}

		void Pop(FPoseCandidate& OutItem)
		{
			PoseCandidateHeap.HeapPop(OutItem, false);
			OutItem.PoseCandidateFlags = PoseIdxToFlags.FindAndRemoveChecked(OutItem.PoseIdx);
		}

		bool IsEmpty() const
		{
			return PoseCandidateHeap.IsEmpty();
		}

		void SetMaxPoseCandidates(int32 Value)
		{
			MaxPoseCandidates = Value;
		}

		const TSet<const UPoseSearchDatabase*>& GetSearchedDatabases() const
		{
			return SearchedDatabases;
		}

	private:
		TSet<const UPoseSearchDatabase*> SearchedDatabases;
		TArray<FPoseCandidateBase> PoseCandidateHeap;
		TMap<int32, EPoseCandidateFlags> PoseIdxToFlags;
		int32 MaxPoseCandidates = 200;
	};
	
	FBestPoseCandidates BestCandidates;
#endif
};

POSESEARCH_API FTransform MirrorTransform(const FTransform& InTransform, EAxis::Type MirrorAxis, const FQuat& ReferenceRotation);

} // namespace UE::PoseSearch
