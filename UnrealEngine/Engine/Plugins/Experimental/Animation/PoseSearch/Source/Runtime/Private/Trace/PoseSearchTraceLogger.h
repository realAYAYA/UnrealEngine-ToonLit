// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearch/PoseSearch.h"

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

	/** Skeletal Mesh Component Id, outer class of the AnimInstance.
	 *  Used for retrieval of traced root transforms from the animation provider.
	 */
	uint64 SkeletalMeshComponentId = 0;

	/** Node Id of the motion matching node associated with this message */
	int32 NodeId = 0;

	uint16 FrameCounter = 0;
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
	// @todo: can we use UPoseSearchDatabase* instead of DatabaseId?
	//UPoseSearchDatabase const* Database = nullptr;
	uint64 DatabaseId = 0;
	TArray<float> QueryVector;
	TArray<FTraceMotionMatchingStatePoseEntry> PoseEntries;
	
	bool operator==(const FTraceMotionMatchingStateDatabaseEntry& Other) const { return DatabaseId == Other.DatabaseId; }
};
POSESEARCH_API FArchive& operator<<(FArchive& Ar, FTraceMotionMatchingStateDatabaseEntry& Entry);

/**
 * Used to trace motion matching state data via the logger, which is then placed into a timeline
 */
struct POSESEARCH_API FTraceMotionMatchingState
{
	/** Bitfield for various state booleans */
	enum class EFlags : uint32
	{
		None = 0u,

		/** Whether the last animation was a forced follow-up animation due to expended animation runway */
		FollowupAnimation = 1u << 0
	};

	/** ObjectId of active searchable asset */
	uint64 SearchableAssetId = 0;
	
	/** Amount of time since the last pose switch */
	float ElapsedPoseJumpTime = 0.0f;

	/** Storage container for state booleans */
	EFlags Flags = EFlags::None;

	float AssetPlayerTime = 0.0f;
	float DeltaTime = 0.0f;
	float SimLinearVelocity = 0.0f;
	float SimAngularVelocity = 0.0f;
	float AnimLinearVelocity = 0.0f;
	float AnimAngularVelocity = 0.0f;
	
	TArray<FTraceMotionMatchingStateDatabaseEntry> DatabaseEntries;

	/** Index of the current database in DatabaseEntries */
	int32 CurrentDbEntryIdx = INDEX_NONE;

	/** Index of the current pose in DatabaseEntries[CurrentDbEntryIdx].PoseEntries */
	int32 CurrentPoseEntryIdx = INDEX_NONE;

	/** Output the current state info to the logger */
	void Output(const FAnimationBaseContext& InContext);

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
ENUM_CLASS_FLAGS(FTraceMotionMatchingState::EFlags)

POSESEARCH_API FArchive& operator<<(FArchive& Ar, FTraceMotionMatchingState& State);

} // namespace UE::PoseSearch
