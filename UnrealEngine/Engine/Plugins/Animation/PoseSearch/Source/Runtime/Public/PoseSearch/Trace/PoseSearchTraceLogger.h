// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearch/PoseSearchContext.h"
#include "PoseSearch/PoseSearchHistory.h"
#include "PoseSearch/PoseSearchTrajectoryTypes.h"

UE_TRACE_CHANNEL_EXTERN(PoseSearchChannel, POSESEARCH_API);

namespace UE::PoseSearch
{

bool IsTracing(const FAnimationBaseContext& InContext);

struct POSESEARCH_API FTraceLogger
{
	/** Used for reading trace data */
    static const FName Name;
};

// Message types for appending / reading to the timeline
/** Base message type for common data */
struct POSESEARCH_API FTraceMessage
{
	uint64 Cycle = 0;

	uint64 AnimInstanceId = 0;

	/** Node Id of the motion matching node associated with this message */
	int32 NodeId = 0;
};
POSESEARCH_API FArchive& operator<<(FArchive& Ar, FTraceMessage& State);

struct POSESEARCH_API FTraceMotionMatchingStatePoseEntry
{
	int32 DbPoseIdx = INDEX_NONE;
	EPoseCandidateFlags PoseCandidateFlags = EPoseCandidateFlags::None;
	FPoseSearchCost Cost;

	bool operator==(const FTraceMotionMatchingStatePoseEntry& Other) const { return DbPoseIdx == Other.DbPoseIdx; }
};
POSESEARCH_API FArchive& operator<<(FArchive& Ar, FTraceMotionMatchingStatePoseEntry& Entry);

struct POSESEARCH_API FTraceMotionMatchingStateDatabaseEntry
{
	uint64 DatabaseId = 0;
	TArray<float> QueryVector;
	TArray<FTraceMotionMatchingStatePoseEntry> PoseEntries;
	
	bool operator==(const FTraceMotionMatchingStateDatabaseEntry& Other) const { return DatabaseId == Other.DatabaseId; }
};
POSESEARCH_API FArchive& operator<<(FArchive& Ar, FTraceMotionMatchingStateDatabaseEntry& Entry);

/**
 * Used to trace motion matching state data via the logger, which is then placed into a timeline
 */
struct POSESEARCH_API FTraceMotionMatchingStateMessage : public FTraceMessage
{
	/** Amount of time since the last pose switch */
	float ElapsedPoseSearchTime = 0.f;

	float AssetPlayerTime = 0.f;
	float DeltaTime = 0.f;
	float SimLinearVelocity = 0.f;
	float SimAngularVelocity = 0.f;
	float AnimLinearVelocity = 0.f;
	float AnimAngularVelocity = 0.f;
	
	float RecordingTime = 0.f;
	float SearchBestCost = 0.f;
	float SearchBruteForceCost = 0.f;
	int32 SearchBestPosePos = 0;

	TArray<uint64> SkeletalMeshComponentIds;
	
	TArray<FRole> Roles;

	TArray<FTraceMotionMatchingStateDatabaseEntry> DatabaseEntries;

	TArray<UE::PoseSearch::FArchivedPoseHistory> PoseHistories;

	/** Index of the current database in DatabaseEntries */
	int32 CurrentDbEntryIdx = INDEX_NONE;

	/** Index of the current pose in DatabaseEntries[CurrentDbEntryIdx].PoseEntries */
	int32 CurrentPoseEntryIdx = INDEX_NONE;

	/** Output the current state info to the logger */
	void Output();

	const UPoseSearchDatabase* GetCurrentDatabase() const;
	int32 GetCurrentDatabasePoseIndex() const;
	const FTraceMotionMatchingStatePoseEntry* GetCurrentPoseEntry() const;

	template<typename T>
	static const T* GetObjectFromId(uint64 ObjectId)
	{
#if OBJECT_TRACE_ENABLED
		if (ObjectId)
		{
			UObject* Object = FObjectTrace::GetObjectFromId(ObjectId);
			if (Object)
			{
				return CastChecked<T>(Object);
			}
		}
#endif

		return nullptr;
	}

	static uint64 GetIdFromObject(const UObject* Object)
	{
#if OBJECT_TRACE_ENABLED
		return FObjectTrace::GetObjectId(Object);
#else
		return 0;
#endif
	}
	
	static const FName Name;
};

POSESEARCH_API FArchive& operator<<(FArchive& Ar, FTraceMotionMatchingStateMessage& State);

} // namespace UE::PoseSearch
