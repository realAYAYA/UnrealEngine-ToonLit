// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "ISparseSampledSequenceTransportCoordinator.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDisplayRangeUpdated, const TRange<double> /* New Display Range */);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnFocusPointMoved, const float /* New Position */);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnFocusPointScrubUpdate, const float /* Focused Playback Ratio */, const bool /*Playhead is Moving*/);

class AUDIOWIDGETS_API FSparseSampledSequenceTransportCoordinator : public ISparseSampledSequenceTransportCoordinator
{
public:
	FSparseSampledSequenceTransportCoordinator() = default;
	virtual ~FSparseSampledSequenceTransportCoordinator() = default;

	/** Called when the focus point is set to a new location */
	FOnFocusPointMoved OnFocusPointMoved;

	/** Called when the focus point is scrubbed */
	FOnFocusPointScrubUpdate OnFocusPointScrubUpdate;

	/** Called when the display range is updated */
	FOnDisplayRangeUpdated OnDisplayRangeUpdated;

	/** ISparseSampledSequenceTransportCoordinator interface */
	const TRange<double> GetDisplayRange() const;
	const float GetFocusPoint() const override;
	void ScrubFocusPoint(const float InTargetFocusPoint, const bool bIsMoving) override;
	const bool IsScrubbing() const;
	void SetProgressRatio(const float NewRatio) override;
	void SetZoomRatio(const float NewRatio) override;
	float ConvertAbsoluteRatioToZoomed(const float InAbsoluteRatio) const override;
	float ConvertZoomedRatioToAbsolute(const float InZoomedRatio) const override;
	void UpdatePlaybackRange(const TRange<double>& NewRange);
	void Stop();

private:
	FORCEINLINE void MoveFocusPoint(const double InFocusPoint);
	void UpdateZoomRatioAndDisplayRange(const float NewZoomRatio);

	void UpdateDisplayRange(const double MinValue, const double MaxValue);
	bool IsRatioWithinDisplayRange(const double Ratio) const;
	double GetPlayBackRatioFromFocusPoint(const double InFocusPoint) const;

	double CurrentPlaybackRatio = 0.f;
	double FocusPointLockPosition = 0.95f;
	double FocusPoint = 0;
	float ZoomRatio = 1.f;

	/* The currently displayed render data range */
	TRange<double> DisplayRange = TRange<double>::Inclusive(0, 1);

	/* Progress range to scale the incoming progress ratio with*/
	TRange<double> ProgressRange = TRange<double>::Inclusive(0, 1);

	bool bIsScrubbing = false;
};