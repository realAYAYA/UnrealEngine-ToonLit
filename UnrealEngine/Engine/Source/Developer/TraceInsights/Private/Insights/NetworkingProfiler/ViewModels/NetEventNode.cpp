// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetEventNode.h"

#define LOCTEXT_NAMESPACE "FNetEventNode"

INSIGHTS_IMPLEMENT_RTTI(FNetEventNode)

////////////////////////////////////////////////////////////////////////////////////////////////////

void FNetEventNode::ResetAggregatedStats()
{
	AggregatedStats = TraceServices::FNetProfilerAggregatedStats();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FNetEventNode::SetAggregatedStats(const TraceServices::FNetProfilerAggregatedStats& InAggregatedStats)
{
	AggregatedStats = InAggregatedStats;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
