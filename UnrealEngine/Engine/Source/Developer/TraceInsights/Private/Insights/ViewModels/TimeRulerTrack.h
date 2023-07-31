// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

// Insights
#include "Insights/ViewModels/BaseTimingTrack.h"
#include "Insights/ITimingViewSession.h"

struct FDrawContext;
class FMenuBuilder;
struct FSlateBrush;
class FTimingTrackViewport;

namespace Insights
{

class ITimingViewExtender;

class FTimeMarker : public  ITimeMarker
{
public:
	FTimeMarker()
		: Time(0.0)
		, Name(TEXT("T"))
		, Color(FLinearColor(1.0f, 1.0f, 1.0f, 0.5f))
		, bIsVisible(true)
		, bIsHighlighted(false)
		, CrtTextWidth(0.0f)
	{}

	virtual ~FTimeMarker() {}

	double GetTime() const override { return Time; }
	void SetTime(const double  InTime) override { Time = InTime; }

	const FString& GetName() const { return Name; }
	void SetName(const FString& InName) { Name = InName; }

	const FLinearColor& GetColor() const { return Color; }
	void SetColor(const FLinearColor& InColor) { Color = InColor; }

	bool IsVisible() const { return bIsVisible; }
	void SetVisibility(bool bOnOff) { bIsVisible = bOnOff; }

	bool IsHighlighted() const { return bIsHighlighted; }
	void SetHighlighted(bool bOnOff) { bIsHighlighted = bOnOff; }

	float GetCrtTextWidth() const { return CrtTextWidth; }
	void SetCrtTextWidthAnimated(const float InTextWidth) const { CrtTextWidth = CrtTextWidth * 0.6f + InTextWidth * 0.4f; }

private:
	double Time;
	FString Name;
	FLinearColor Color;
	bool bIsVisible;
	bool bIsHighlighted;

	// Smoothed time marker text width to avoid flickering
	mutable float CrtTextWidth;
};

} // namespace Insights

////////////////////////////////////////////////////////////////////////////////////////////////////

class FTimeRulerTrack : public FBaseTimingTrack
{
	INSIGHTS_DECLARE_RTTI(FTimeRulerTrack, FBaseTimingTrack)

public:
	FTimeRulerTrack();
	virtual ~FTimeRulerTrack();

	virtual void Reset() override;

	void SetSelection(const bool bInIsSelecting, const double InSelectionStartTime, const double InSelectionEndTime);

	TArray<TSharedRef<Insights::FTimeMarker>>& GetTimeMarkers() { return TimeMarkers; }
	const TArray<TSharedRef<Insights::FTimeMarker>>& GetTimeMarkers() const { return TimeMarkers; }
	void AddTimeMarker(TSharedRef<Insights::FTimeMarker> InTimeMarker);
	void RemoveTimeMarker(TSharedRef<Insights::FTimeMarker> InTimeMarker);
	void RemoveAllTimeMarkers();

	TSharedPtr<Insights::FTimeMarker> GetTimeMarkerByName(const FString& InTimeMarkerName);
	TSharedPtr<Insights::FTimeMarker> GetTimeMarkerAtPos(const FVector2D& InPosition, const FTimingTrackViewport& InViewport);

	bool IsScrubbing() const { return bIsScrubbing; }
	TSharedRef<Insights::FTimeMarker> GetScrubbingTimeMarker() { return TimeMarkers.Last(); }
	void StartScrubbing(TSharedRef<Insights::FTimeMarker> InTimeMarker);
	void StopScrubbing();

	virtual void PostUpdate(const ITimingTrackUpdateContext& Context) override;
	void Draw(const ITimingTrackDrawContext& Context) const override;
	void PostDraw(const ITimingTrackDrawContext& Context) const override;

	virtual void BuildContextMenu(FMenuBuilder& MenuBuilder) override;

private:
	void DrawTimeMarker(const ITimingTrackDrawContext& Context, const Insights::FTimeMarker& TimeMarker) const;
	void ContextMenu_MoveTimeMarker_Execute(TSharedRef<Insights::FTimeMarker> InTimeMarker);

private:
	// Slate resources
	const FSlateBrush* WhiteBrush;
	const FSlateFontInfo Font;

	bool bIsSelecting;
	double SelectionStartTime;
	double SelectionEndTime;

	// The last time value at mouse postion. Updated in PostDraw.
	mutable double CrtMousePosTime;

	// The smoothed width of "the text at mouse position" to avoid flickering. Updated in PostDraw.
	mutable float CrtMousePosTextWidth;

	/**
	 * The sorted list of all registered time markers. It defines the draw order of time markers.
	 * The time marker currently scrubbing will be moved at the end of the list in order to be displayed on top of other markers.
	 */
	TArray<TSharedRef<Insights::FTimeMarker>> TimeMarkers;

	/** True if the user is currently dragging a time marker. */
	bool bIsScrubbing;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
