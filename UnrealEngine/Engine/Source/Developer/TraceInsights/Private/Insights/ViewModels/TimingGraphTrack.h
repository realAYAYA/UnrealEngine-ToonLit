// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Insights
#include "Insights/ViewModels/GraphSeries.h"
#include "Insights/ViewModels/GraphTrack.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

class FTimingGraphSeries : public FGraphSeries
{
public:
	enum class ESeriesType
	{
		Frame,
		Timer,
		StatsCounter
	};

	struct FSimpleTimingEvent
	{
		double StartTime;
		double Duration;
	};

public:
	explicit FTimingGraphSeries(FTimingGraphSeries::ESeriesType Type);
	virtual ~FTimingGraphSeries();

	virtual FString FormatValue(double Value) const override;

	static bool CompareEventsByStartTime(const FSimpleTimingEvent& EventA, const FSimpleTimingEvent& EventB)
	{
		return EventA.StartTime < EventB.StartTime;
	}

public:
	ESeriesType Type;
	union
	{
		ETraceFrameType FrameType;
		uint32 TimerId;
		uint32 CounterId;
	};

	double CachedSessionDuration;
	TArray<FSimpleTimingEvent> CachedEvents; // used by Timer series

	bool bIsTime; // the unit for values is [second]
	bool bIsMemory; // the unit for value is [byte]
	bool bIsFloatingPoint; // for stats counters
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FTimingGraphTrack : public FGraphTrack
{
	INSIGHTS_DECLARE_RTTI(FTimingGraphTrack, FGraphTrack)

public:
	FTimingGraphTrack();
	virtual ~FTimingGraphTrack();

	virtual void Update(const ITimingTrackUpdateContext& Context) override;

	void AddDefaultFrameSeries();

	TSharedPtr<FTimingGraphSeries> GetTimerSeries(uint32 TimerId);
	TSharedPtr<FTimingGraphSeries> AddTimerSeries(uint32 TimerId, FLinearColor Color);
	void RemoveTimerSeries(uint32 TimerId);

	TSharedPtr<FTimingGraphSeries> GetStatsCounterSeries(uint32 CounterId);
	TSharedPtr<FTimingGraphSeries> AddStatsCounterSeries(uint32 CounterId, FLinearColor Color);
	void RemoveStatsCounterSeries(uint32 CounterId);

protected:
	void UpdateFrameSeries(FTimingGraphSeries& Series, const FTimingTrackViewport& Viewport);
	void UpdateTimerSeries(FTimingGraphSeries& Series, const FTimingTrackViewport& Viewport);
	void UpdateStatsCounterSeries(FTimingGraphSeries& Series, const FTimingTrackViewport& Viewport);

	virtual void DrawVerticalAxisGrid(const ITimingTrackDrawContext& Context) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
