// Copyright Epic Games, Inc. All Rights Reserved.

#include "MemTagNode.h"

// Insights
#include "Insights/MemoryProfiler/MemoryProfilerManager.h"

#define LOCTEXT_NAMESPACE "FMemTagNode"

INSIGHTS_IMPLEMENT_RTTI(FMemTagNode)

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FMemTagNode::GetTrackerText() const
{
	FMemorySharedState* SharedState = FMemoryProfilerManager::Get()->GetSharedState();
	if (SharedState)
	{
		Insights::FMemoryTrackerId TrackerId = GetMemTrackerId();
		const Insights::FMemoryTracker* Tracker = SharedState->GetTrackerById(TrackerId);
		if (Tracker)
		{
			return FText::FromString(Tracker->GetName());
		}
	}

	return FText::GetEmpty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FMemTagNode::ResetAggregatedStats()
{
	//AggregatedStats = TraceServices::FMemoryProfilerAggregatedStats();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/*
void FMemTagNode::SetAggregatedStats(const TraceServices::FMemoryProfilerAggregatedStats& InAggregatedStats)
{
	AggregatedStats = InAggregatedStats;
}
*/
////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
