// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/WidgetStyle.h"

enum class ESlateDrawEffect : uint8;

struct FDrawContext;
struct FGeometry;
struct FSlateBrush;

class FFrameTrackViewport;
class FSlateWindowElementList;

namespace TraceServices
{
	struct FFrame;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

struct FFrameTrackSample
{
	int32 NumFrames;
	double TotalDuration; // sum of durations of all frames in this sample
	double StartTime; // min start time of all frames in this sample
	double EndTime; // max end time of all frames in this sample
	int32 LargestFrameIndex; // index of the largest frame
	double LargestFrameStartTime; // start time of the largest frame
	double LargestFrameDuration; // duration of the largest frame

	FFrameTrackSample()
		: NumFrames(0)
		, TotalDuration(0.0)
		, StartTime(DBL_MAX)
		, EndTime(-DBL_MAX)
		, LargestFrameIndex(0)
		, LargestFrameStartTime(0.0)
		, LargestFrameDuration(0.0)
	{}

	FFrameTrackSample(const FFrameTrackSample&) = default;
	FFrameTrackSample& operator=(const FFrameTrackSample&) = default;

	FFrameTrackSample(FFrameTrackSample&&) = default;
	FFrameTrackSample& operator=(FFrameTrackSample&&) = default;

	bool Equals(const FFrameTrackSample& Other) const
	{
		return NumFrames == Other.NumFrames
			&& TotalDuration == Other.TotalDuration
			&& StartTime == Other.StartTime
			&& EndTime == Other.EndTime
			&& LargestFrameIndex == Other.LargestFrameIndex
			&& LargestFrameStartTime == Other.LargestFrameStartTime
			&& LargestFrameDuration == Other.LargestFrameDuration;
	}

	static bool AreEquals(const FFrameTrackSample& A, const FFrameTrackSample& B)
	{
		return A.Equals(B);
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////

enum class EFrameTrackSeriesType : uint32
{
	Frame,
	TimerFrameStats
};

////////////////////////////////////////////////////////////////////////////////////////////////////

struct FFrameTrackSeries
{
	int32 FrameType;
	EFrameTrackSeriesType Type;
	bool bIsVisible;
	int32 NumAggregatedFrames; // total number of frames aggregated in samples; i.e. sum of all Sample.NumFrames
	TArray<FFrameTrackSample> Samples; // the aggregated samples
	FLinearColor Color;
	FText Name;

	explicit FFrameTrackSeries(int32 InFrameType, EFrameTrackSeriesType InType)
		: FrameType(InFrameType)
		, Type(InType)
		, bIsVisible(true)
		, NumAggregatedFrames(0)
		, Samples()
	{
	}

	void Reset()
	{
		NumAggregatedFrames = 0;
		Samples.Reset();
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////

struct FTimerFrameStatsTrackSeries : public FFrameTrackSeries
{
	FTimerFrameStatsTrackSeries(int32 InFrameType, uint32 InTimerId)
		: FFrameTrackSeries(InFrameType, EFrameTrackSeriesType::TimerFrameStats)
		, TimerId(InTimerId)
	{
	}

	uint32 TimerId;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FFrameTrackSeriesBuilder
{
public:
	explicit FFrameTrackSeriesBuilder(FFrameTrackSeries& InSeries, const FFrameTrackViewport& InViewport);

	/**
	 * Non-copyable
	 */
	FFrameTrackSeriesBuilder(const FFrameTrackSeriesBuilder&) = delete;
	FFrameTrackSeriesBuilder& operator=(const FFrameTrackSeriesBuilder&) = delete;

	void AddFrame(const TraceServices::FFrame& Frame);

	int32 GetNumAddedFrames() const { return NumAddedFrames; }

private:
	FFrameTrackSeries& Series; // series to update
	const FFrameTrackViewport& Viewport;

	float SampleW; // width of a sample, in Slate units
	int32 FramesPerSample; // number of frames in a sample
	int32 FirstFrameIndex; // index of first frame in first sample; can be negative
	int32 NumSamples; // total number of samples

	// Debug stats.
	int32 NumAddedFrames; // counts total number of added frame events
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FFrameTrackDrawHelper
{
public:
	explicit FFrameTrackDrawHelper(const FDrawContext& InDrawContext, const FFrameTrackViewport& InViewport);

	/**
	 * Non-copyable
	 */
	FFrameTrackDrawHelper(const FFrameTrackDrawHelper&) = delete;
	FFrameTrackDrawHelper& operator=(const FFrameTrackDrawHelper&) = delete;

	void DrawBackground() const;
	void DrawCached(const FFrameTrackSeries& Series) const;
	void DrawHoveredSample(const FFrameTrackSample& Sample) const;
	void DrawHighlightedInterval(const FFrameTrackSeries& Series, const double StartTime, const double EndTime) const;

	static const TCHAR* FrameTypeToString(int32 FrameType);
	static uint32 GetColor32ByFrameType(int32 FrameType);
	static FLinearColor GetColorByFrameType(int32 FrameType);

	int32 GetNumFrames() const { return NumFrames; }
	int32 GetNumDrawSamples() const { return NumDrawSamples; }

private:
	const FDrawContext& DrawContext;
	const FFrameTrackViewport& Viewport;

	const FSlateBrush* WhiteBrush;
	const FSlateBrush* HoveredFrameBorderBrush;

	// Debug stats.
	mutable int32 NumFrames;
	mutable int32 NumDrawSamples;
};
