// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetStatsCounterNode.h"

// Insights
#include "Insights/Common/TimeUtils.h"

#define LOCTEXT_NAMESPACE "FNetStatsCounterNode"

INSIGHTS_IMPLEMENT_RTTI(FNetStatsCounterNode)

////////////////////////////////////////////////////////////////////////////////////////////////////

void FNetStatsCounterNode::ResetAggregatedStats()
{
	AggregatedStats.Reset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const FText FNetStatsCounterNode::FormatAggregatedStatsValue(uint32 Value) const
{
	if (AggregatedStats.Count > 0)
	{
		return FText::AsNumber(Value);
	}

	return LOCTEXT("AggregatedStatsNA", "N/A");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const FText FNetStatsCounterNode::GetTextForAggregatedStatsSum(bool bForTooltip) const
{
	return FormatAggregatedStatsValue(AggregatedStats.Sum);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const FText FNetStatsCounterNode::GetTextForAggregatedStatsMin(bool bForTooltip) const
{
	return FormatAggregatedStatsValue(AggregatedStats.Min);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const FText FNetStatsCounterNode::GetTextForAggregatedStatsMax(bool bForTooltip) const
{
	return FormatAggregatedStatsValue(AggregatedStats.Max);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const FText FNetStatsCounterNode::GetTextForAggregatedStatsAverage(bool bForTooltip) const
{
	return FormatAggregatedStatsValue(AggregatedStats.Average);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FNetStatsCounterNode::SetAggregatedStats(const FNetProfilerAggregatedStatsCounterStats& InAggregatedStats)
{
	ensure(InAggregatedStats.StatsCounterTypeIndex == AggregatedStats.StatsCounterTypeIndex);
	AggregatedStats = InAggregatedStats;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
