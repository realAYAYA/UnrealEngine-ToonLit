// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchTraceLogger.h"
#include "Animation/AnimInstanceProxy.h"
#include "Trace/Trace.inl"
#include "Animation/AnimNodeBase.h"


UE_TRACE_CHANNEL_DEFINE(PoseSearchChannel);

UE_TRACE_EVENT_BEGIN(PoseSearch, MotionMatchingState)
	UE_TRACE_EVENT_FIELD(uint8[], Data)
UE_TRACE_EVENT_END()

namespace UE::PoseSearch
{

const FName FTraceLogger::Name("PoseSearch");
const FName FTraceMotionMatchingState::Name("MotionMatchingState");

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
#else
	return false;
#endif
}

FArchive& operator<<(FArchive& Ar, FTraceMessage& State)
{
	Ar << State.Cycle;
	Ar << State.AnimInstanceId;
	Ar << State.SkeletalMeshComponentId;
	Ar << State.NodeId;
	Ar << State.FrameCounter;
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

FArchive& operator<<(FArchive& Ar, FTraceMotionMatchingState& State)
{
	Ar << State.SearchableAssetId;
	Ar << State.ElapsedPoseJumpTime;
	Ar << State.Flags;
	Ar << State.AssetPlayerTime;
	Ar << State.DeltaTime;
	Ar << State.SimLinearVelocity;
	Ar << State.SimAngularVelocity;
	Ar << State.AnimLinearVelocity;
	Ar << State.AnimAngularVelocity;
	Ar << State.DatabaseEntries;
	Ar << State.CurrentDbEntryIdx;
	Ar << State.CurrentPoseEntryIdx;
	return Ar;
}

void FTraceMotionMatchingState::Output(const FAnimationBaseContext& InContext)
{
#if OBJECT_TRACE_ENABLED
	if (!IsTracing(InContext))
	{
		return;
	}

	TArray<uint8> ArchiveData;
	FMemoryWriter Archive(ArchiveData);

	UObject* AnimInstance = InContext.AnimInstanceProxy->GetAnimInstanceObject();
	TRACE_OBJECT(AnimInstance);
	UObject* SkeletalMeshComponent = AnimInstance->GetOuter();

	FTraceMessage TraceMessage;
	TraceMessage.Cycle = FPlatformTime::Cycles64();
	TraceMessage.AnimInstanceId = FObjectTrace::GetObjectId(AnimInstance);
	TraceMessage.SkeletalMeshComponentId = FObjectTrace::GetObjectId(SkeletalMeshComponent);
	TraceMessage.NodeId = InContext.GetCurrentNodeId();
	TraceMessage.FrameCounter = FObjectTrace::GetObjectWorldTickCounter(AnimInstance);

	Archive << TraceMessage;
	Archive << *this;

	UE_TRACE_LOG(PoseSearch, MotionMatchingState, PoseSearchChannel)
		<< MotionMatchingState.Data(ArchiveData.GetData(), ArchiveData.Num());
#endif
}

const UPoseSearchDatabase* FTraceMotionMatchingState::GetCurrentDatabase() const
{
	if (CurrentDbEntryIdx == INDEX_NONE)
	{
		return nullptr;
	}

	const FTraceMotionMatchingStateDatabaseEntry& DbEntry = DatabaseEntries[CurrentDbEntryIdx];
	const UPoseSearchDatabase* Database = GetObjectFromId<UPoseSearchDatabase>(DbEntry.DatabaseId);
	return Database;
}

int32 FTraceMotionMatchingState::GetCurrentDatabasePoseIndex() const
{
	const FTraceMotionMatchingStatePoseEntry* PoseEntry = GetCurrentPoseEntry();
	return PoseEntry ? PoseEntry->DbPoseIdx : INDEX_NONE;
}

const FTraceMotionMatchingStatePoseEntry* FTraceMotionMatchingState::GetCurrentPoseEntry() const
{
	if ((CurrentDbEntryIdx == INDEX_NONE) || (CurrentPoseEntryIdx == INDEX_NONE))
	{
		return nullptr;
	}

	const FTraceMotionMatchingStateDatabaseEntry& DbEntry = DatabaseEntries[CurrentDbEntryIdx];
	return &DbEntry.PoseEntries[CurrentPoseEntryIdx];
}

} // namespace UE::PoseSearch