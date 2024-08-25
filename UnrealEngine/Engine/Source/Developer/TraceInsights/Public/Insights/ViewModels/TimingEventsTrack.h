// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"

// Insights
#include "Insights/ViewModels/BaseTimingTrack.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

struct FSlateBrush;

class TRACEINSIGHTS_API ITimingEventsTrackDrawStateBuilder
{
public:
	typedef TFunctionRef<const FString(float /*AvailableWidth*/)> GetEventNameCallback;

public:
	virtual ~ITimingEventsTrackDrawStateBuilder() = default;

	virtual void AddEvent(double InEventStartTime, double InEventEndTime, uint32 InEventDepth, const TCHAR* InEventName, uint64 InEventType = 0, uint32 InEventColor = 0) = 0;
	virtual void AddEvent(double InEventStartTime, double InEventEndTime, uint32 InEventDepth, uint32 InEventColor, GetEventNameCallback InGetEventNameCallback) = 0;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class TRACEINSIGHTS_API FTimingEventsTrack : public FBaseTimingTrack
{
	INSIGHTS_DECLARE_RTTI(FTimingEventsTrack, FBaseTimingTrack)

public:
	explicit FTimingEventsTrack();
	explicit FTimingEventsTrack(const FString& InTrackName);
	virtual ~FTimingEventsTrack();

	//////////////////////////////////////////////////
	// FBaseTimingTrack

	virtual void Reset() override;

	virtual void PreUpdate(const ITimingTrackUpdateContext& Context) override;
	virtual void PostUpdate(const ITimingTrackUpdateContext& Context) override;

	virtual void Draw(const ITimingTrackDrawContext& Context) const override;
	virtual void PostDraw(const ITimingTrackDrawContext& Context) const override;
	virtual void DrawEvent(const ITimingTrackDrawContext& Context, const ITimingEvent& InTimingEvent, EDrawEventMode InDrawMode) const override;

	virtual const TSharedPtr<const ITimingEvent> GetEvent(float InPosX, float InPosY, const FTimingTrackViewport& Viewport) const override;

	virtual TSharedPtr<ITimingEventFilter> GetFilterByEvent(const TSharedPtr<const ITimingEvent> InTimingEvent) const override;

	//////////////////////////////////////////////////

	virtual void BuildDrawState(ITimingEventsTrackDrawStateBuilder& Builder, const ITimingTrackUpdateContext& Context) = 0;
	virtual void BuildFilteredDrawState(ITimingEventsTrackDrawStateBuilder& Builder, const ITimingTrackUpdateContext& Context) {}

protected:
	int32 GetNumLanes() const { return NumLanes; }
	void SetNumLanes(int32 InNumLanes) { NumLanes = InNumLanes; }

	const struct FTimingEventsTrackDrawState& GetDrawState() const { return *DrawState; }
	const struct FTimingEventsTrackDrawState& GetFilteredDrawState() const { return *FilteredDrawState; }

	float GetFilteredDrawStateOpacity() const { return FilteredDrawStateInfo.Opacity; }
	bool UpdateFilteredDrawStateOpacity() const
	{
		if (FilteredDrawStateInfo.Opacity == 1.0f)
		{
			return true;
		}
		else
		{
			FilteredDrawStateInfo.Opacity = FMath::Min(1.0f, FilteredDrawStateInfo.Opacity + 0.05f);
			return false;
		}
	}

	void UpdateTrackHeight(const ITimingTrackUpdateContext& Context);

	void DrawEvents(const ITimingTrackDrawContext& Context, const float OffsetY = 1.0f) const;
	void DrawHeader(const ITimingTrackDrawContext& Context) const;

	void DrawMarkers(const ITimingTrackDrawContext& Context, float LineY, float LineH) const;

	void DrawSelectedEventInfo(const FString& InText, const FTimingTrackViewport& Viewport, const FDrawContext& DrawContext, const FSlateBrush* WhiteBrush, const FSlateFontInfo& Font) const;
	void DrawSelectedEventInfoEx(const FString& InText, const FString& InLeftText, const FString& InTopText, const FTimingTrackViewport& Viewport, const FDrawContext& DrawContext, const FSlateBrush* WhiteBrush, const FSlateFontInfo& Font) const;

	int32 GetHeaderBackgroundLayerId(const ITimingTrackDrawContext& Context) const;
	int32 GetHeaderTextLayerId(const ITimingTrackDrawContext& Context) const;

	virtual const TSharedPtr<const ITimingEvent> GetEvent(double InTime, double SecondsPerPixel, int32 Depth) const;

	virtual bool HasCustomFilter() const { return false; }

	/* Can be overridden to force a max depth for the track. */
	virtual int32 GetMaxDepth() const { return -1; }

private:
	int32 NumLanes; // number of lanes (sub-tracks)
	TSharedRef<struct FTimingEventsTrackDrawState> DrawState;
	TSharedRef<struct FTimingEventsTrackDrawState> FilteredDrawState;

	struct FFilteredDrawStateInfo
	{
		double ViewportStartTime = 0.0;
		double ViewportScaleX = 0.0;
		double LastBuildDuration = 0.0;
		TWeakPtr<ITimingEventFilter> LastEventFilter;
		uint32 LastFilterChangeNumber = 0;
		uint32 Counter = 0;
		mutable float Opacity = 0.0f;
	};
	FFilteredDrawStateInfo FilteredDrawStateInfo;

public:
	static bool bUseDownSampling; // toggle to enable/disable downsampling, for debugging purposes only
};

////////////////////////////////////////////////////////////////////////////////////////////////////
