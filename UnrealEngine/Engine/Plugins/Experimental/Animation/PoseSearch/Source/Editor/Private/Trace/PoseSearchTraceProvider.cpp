// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchTraceProvider.h"

namespace UE::PoseSearch
{

const FName FTraceProvider::ProviderName("PoseSearchTraceProvider");

FTraceProvider::FTraceProvider(TraceServices::IAnalysisSession& InSession) : Session(InSession)
{
}

TSet<int32> FTraceProvider::GetMotionMatchingNodeIds(uint64 InAnimInstanceId) const
{
	return MotionMatchingStateTimelineStorage.GetNodeIds(InAnimInstanceId);
}

bool FTraceProvider::ReadMotionMatchingStateTimeline(uint64 InAnimInstanceId, int32 InNodeId, TFunctionRef<void(const FMotionMatchingStateTimeline&)> Callback) const
{
	Session.ReadAccessCheck();
	return MotionMatchingStateTimelineStorage.ReadTimeline(InAnimInstanceId, InNodeId, Callback);
}

bool FTraceProvider::EnumerateMotionMatchingStateTimelines(uint64 InAnimInstanceId, TFunctionRef<void(const FMotionMatchingStateTimeline&)> Callback) const
{
	Session.ReadAccessCheck();
	return MotionMatchingStateTimelineStorage.EnumerateNodeTimelines(InAnimInstanceId, Callback);
}


void FTraceProvider::AppendMotionMatchingState(const FTraceMotionMatchingStateMessage& InMessage, double InTime)
{
	Session.WriteAccessCheck();

	TSharedRef<TraceServices::TPointTimeline<FTraceMotionMatchingStateMessage>> Timeline = MotionMatchingStateTimelineStorage.GetTimeline(Session, InMessage.AnimInstanceId, InMessage.NodeId);
	Timeline->AppendEvent(InTime, InMessage);
	
	Session.UpdateDurationSeconds(InTime);
}

FArchive& operator<<(FArchive& Ar, FTraceMotionMatchingStateMessage& State)
{
	Ar << static_cast<FTraceMessage&>(State);
	Ar << static_cast<FTraceMotionMatchingState&>(State);
	return Ar;
}

} // namespace UE::PoseSearch
