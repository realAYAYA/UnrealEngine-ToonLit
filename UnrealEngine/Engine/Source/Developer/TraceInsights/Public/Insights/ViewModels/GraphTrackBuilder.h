// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Insights
class FGraphTrack;
class FGraphSeries;
class FTimingTrackViewport;

////////////////////////////////////////////////////////////////////////////////////////////////////

class TRACEINSIGHTS_API FGraphTrackBuilder
{
private:
	struct FPointInfo
	{
		bool bValid;
		float X;
		float Y;

		FPointInfo() : bValid(false) {}
	};

public:
	explicit FGraphTrackBuilder(FGraphTrack& InTrack, FGraphSeries& InSeries, const FTimingTrackViewport& InViewport);
	~FGraphTrackBuilder();

	/**
	 * Non-copyable
	 */
	FGraphTrackBuilder(const FGraphTrackBuilder&) = delete;
	FGraphTrackBuilder& operator=(const FGraphTrackBuilder&) = delete;

	FGraphTrack& GetTrack() const { return Track; }
	FGraphSeries& GetSeries() const { return Series; }
	const FTimingTrackViewport& GetViewport() const { return Viewport; }

	void AddEvent(double Time, double Duration, double Value, bool bConnected = true);

private:
	void BeginPoints();
	bool AddPoint(double Time, double Value);
	void FlushPoints();
	void EndPoints();

	void BeginConnectedLines();
	void AddConnectedLine(double Time, double Value, bool bNewBatch);
	void FlushConnectedLine();
	void AddConnectedLinePoint(float X, float Y);
	void EndConnectedLines();

	void BeginBoxes();
	void AddBox(double Time, double Duration, double Value);
	void FlushBox();
	void EndBoxes();

private:
	FGraphTrack& Track;
	FGraphSeries& Series;
	const FTimingTrackViewport& Viewport;

	// Used by the point reduction algorithm.
	double PointsCurrentX;
	TArray<FPointInfo> PointsAtCurrentX;

	// Used by the line reduction algorithm.
	float LinesCurrentX;
	float LinesMinY;
	float LinesMaxY;
	float LinesFirstY;
	float LinesLastY;
	bool bIsLastLineAdded;

	// Used by the box reduction algorithm.
	//...
};

////////////////////////////////////////////////////////////////////////////////////////////////////
