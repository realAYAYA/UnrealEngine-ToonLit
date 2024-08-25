// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/Trace/PoseSearchTraceLogger.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "Serialization/MemoryWriter.h"
#include "Trace/Trace.inl"
#include "TraceFilter.h"

UE_TRACE_CHANNEL_DEFINE(PoseSearchChannel);

UE_TRACE_EVENT_BEGIN(PoseSearch, MotionMatchingState)
	UE_TRACE_EVENT_FIELD(uint8[], Data)
UE_TRACE_EVENT_END()

namespace UE::PoseSearch
{

const FName FTraceLogger::Name("PoseSearch");
const FName FTraceMotionMatchingStateMessage::Name("MotionMatchingState");

bool IsTracing(const FAnimationBaseContext& InContext)
{
#if UE_POSE_SEARCH_TRACE_ENABLED
	const bool bChannelEnabled = UE_TRACE_CHANNELEXPR_IS_ENABLED(PoseSearchChannel);
	if (!bChannelEnabled)
	{
		return false;
	}

	if (InContext.GetCurrentNodeId() == INDEX_NONE)
	{
		return false;
	}

	check(InContext.AnimInstanceProxy);
	return !CANNOT_TRACE_OBJECT(InContext.AnimInstanceProxy->GetSkelMeshComponent());
#else // UE_POSE_SEARCH_TRACE_ENABLED
	return false;
#endif // UE_POSE_SEARCH_TRACE_ENABLED
}

FArchive& operator<<(FArchive& Ar, FTraceMessage& State)
{
	Ar << State.Cycle;
	Ar << State.AnimInstanceId;
	Ar << State.NodeId;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FTraceMotionMatchingStatePoseEntry& Entry)
{
	Ar << Entry.DbPoseIdx;
	FPoseSearchCost::StaticStruct()->SerializeItem(Ar, &Entry.Cost, nullptr);
	Ar << Entry.PoseCandidateFlags;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FTraceMotionMatchingStateDatabaseEntry& Entry)
{
	Ar << Entry.DatabaseId;
	Ar << Entry.QueryVector;
	Ar << Entry.PoseEntries;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FTraceMotionMatchingStateMessage& State)
{
	Ar << static_cast<FTraceMessage&>(State);

	Ar << State.ElapsedPoseSearchTime;
	Ar << State.AssetPlayerTime;
	Ar << State.DeltaTime;
	Ar << State.SimLinearVelocity;
	Ar << State.SimAngularVelocity;
	Ar << State.AnimLinearVelocity;
	Ar << State.AnimAngularVelocity;
	Ar << State.RecordingTime;
	Ar << State.SearchBestCost;
	Ar << State.SearchBruteForceCost;
	Ar << State.SearchBestPosePos;
	Ar << State.SkeletalMeshComponentIds;
	Ar << State.Roles;
	Ar << State.DatabaseEntries;
	Ar << State.PoseHistories;	
	Ar << State.CurrentDbEntryIdx;
	Ar << State.CurrentPoseEntryIdx;
	return Ar;
}

void FTraceMotionMatchingStateMessage::Output()
{
#if OBJECT_TRACE_ENABLED
	TArray<uint8> ArchiveData;
	FMemoryWriter Archive(ArchiveData);
	Archive << *this;
	UE_TRACE_LOG(PoseSearch, MotionMatchingState, PoseSearchChannel) << MotionMatchingState.Data(ArchiveData.GetData(), ArchiveData.Num());
#endif
}

const UPoseSearchDatabase* FTraceMotionMatchingStateMessage::GetCurrentDatabase() const
{
	const UPoseSearchDatabase* Database = nullptr;
	if (DatabaseEntries.IsValidIndex(CurrentDbEntryIdx))
	{
		Database = GetObjectFromId<UPoseSearchDatabase>(DatabaseEntries[CurrentDbEntryIdx].DatabaseId);
	}
	return Database;
}

int32 FTraceMotionMatchingStateMessage::GetCurrentDatabasePoseIndex() const
{
	if (const FTraceMotionMatchingStatePoseEntry* PoseEntry = GetCurrentPoseEntry())
	{
		return PoseEntry->DbPoseIdx;
	}
	return INDEX_NONE;
}

const FTraceMotionMatchingStatePoseEntry* FTraceMotionMatchingStateMessage::GetCurrentPoseEntry() const
{
	if (DatabaseEntries.IsValidIndex(CurrentDbEntryIdx))
	{
		const FTraceMotionMatchingStateDatabaseEntry& DbEntry = DatabaseEntries[CurrentDbEntryIdx];
		if (DbEntry.PoseEntries.IsValidIndex(CurrentPoseEntryIdx))
		{
			return &DbEntry.PoseEntries[CurrentPoseEntryIdx];
		}
	}
	return nullptr;
}

} // namespace UE::PoseSearch