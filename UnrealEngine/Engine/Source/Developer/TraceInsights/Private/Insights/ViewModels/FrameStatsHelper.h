// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"

#include <atomic>

namespace Insights
{
struct FFrameStatsCachedEvent
{
	FFrameStatsCachedEvent()
		: FrameStartTime(0.0f)
		, FrameEndTime(0.0f)
	{
		Duration.store(0.0f);
	}

	FFrameStatsCachedEvent(const FFrameStatsCachedEvent& Other)
		: FrameStartTime(Other.FrameStartTime)
		, FrameEndTime(Other.FrameEndTime)
	{
		Duration.store(Other.Duration.load());
	}

	double FrameStartTime;
	double FrameEndTime;

	// The sum of the all the instances of a timing event in a frame.
	std::atomic<double> Duration;
};

class FFrameStatsHelper
{
public:
	static void ComputeFrameStatsForTimer(TArray<FFrameStatsCachedEvent>& FrameStatsEvents, uint32 TimerId, const TSet<uint32>& Timelines);
	static void ComputeFrameStatsForTimer(TArray<FFrameStatsCachedEvent>& FrameStatsEvents, uint32 TimerId);

private:
	static void ProcessTimeline(TArray<FFrameStatsCachedEvent>& FrameStatsEvents, uint32 TimerId, uint32 TimelineIndex);
};

// namespace Insights
}
