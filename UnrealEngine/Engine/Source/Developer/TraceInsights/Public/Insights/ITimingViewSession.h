// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FBaseTimingTrack;
class FTimingEventsTrack;
class ITimingEvent;
class SWidget;
enum class TRACEINSIGHTS_API ETimingTrackLocation : uint32; // Insights/ViewModels/BaseTimingTrack.h

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

enum class ETimeChangedFlags : int32
{
	None,

	// The event fired in response to an interactive change from the user. Will be followed by a non-interactive change finished.
	Interactive = (1 << 0)
};
ENUM_CLASS_FLAGS(ETimeChangedFlags);

////////////////////////////////////////////////////////////////////////////////////////////////////

class TRACEINSIGHTS_API ITimeMarker
{
public:
	virtual ~ITimeMarker() = default;

	virtual double GetTime() const = 0;
	virtual void SetTime(const double  InTime) = 0;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

/** The delegate to be invoked when the selection have been changed */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FSelectionChangedDelegate, ETimeChangedFlags /*InFlags*/, double /*StartTime*/, double /*EndTime*/);

/** The delegate to be invoked when the time marker has changed */
DECLARE_MULTICAST_DELEGATE_TwoParams(FTimeMarkerChangedDelegate, ETimeChangedFlags /*InFlags*/, double /*TimeMarker*/);

/** The delegate to be invoked when a custom time marker has changed */
DECLARE_MULTICAST_DELEGATE_TwoParams(FCustomTimeMarkerChangedDelegate, ETimeChangedFlags /*InFlags*/, TSharedRef<ITimeMarker> /*TimeMarker*/);

/** The delegate to be invoked when the timing track being hovered by the mouse has changed */
DECLARE_MULTICAST_DELEGATE_OneParam(FHoveredTrackChangedDelegate, const TSharedPtr<FBaseTimingTrack> /*InTrack*/);

/** The delegate to be invoked when the timing event being hovered by the mouse has changed */
DECLARE_MULTICAST_DELEGATE_OneParam(FHoveredEventChangedDelegate, const TSharedPtr<const ITimingEvent> /*InEvent*/);

/** The delegate to be invoked when the selected timing track has changed */
DECLARE_MULTICAST_DELEGATE_OneParam(FSelectedTrackChangedDelegate, const TSharedPtr<FBaseTimingTrack> /*InTrack*/);

/** The delegate to be invoked when the selected timing event has changed */
DECLARE_MULTICAST_DELEGATE_OneParam(FSelectedEventChangedDelegate, const TSharedPtr<const ITimingEvent> /*InEvent*/);

/** The delegate to be invoked when a track visibility has changed */
DECLARE_MULTICAST_DELEGATE(FTrackVisibilityChangedDelegate);

/** The delegate to be invoked when a track is added. */
DECLARE_MULTICAST_DELEGATE_OneParam(FTrackAddedDelegate, const TSharedPtr<const FBaseTimingTrack> /*Track*/);

/** The delegate to be invoked when a track is removed. */
DECLARE_MULTICAST_DELEGATE_OneParam(FTrackRemovedDelegate, const TSharedPtr<const FBaseTimingTrack> /*Track*/);

////////////////////////////////////////////////////////////////////////////////////////////////////

/** Hosts a number of timing view visualizers, represents a session of the timing view. */
class TRACEINSIGHTS_API ITimingViewSession
{
public:
	virtual ~ITimingViewSession() = default;

	/** Gets the name of the view. */
	virtual const FName& GetName() const = 0;

	/** Adds a new top docked track. */
	virtual void AddTopDockedTrack(TSharedPtr<FBaseTimingTrack> Track) = 0;
	/** Removes a top docked track. Returns whether the track was removed or not. */
	virtual bool RemoveTopDockedTrack(TSharedPtr<FBaseTimingTrack> Track) = 0;

	/** Adds a new bottom docked track. */
	virtual void AddBottomDockedTrack(TSharedPtr<FBaseTimingTrack> Track) = 0;
	/** Removes a bottom docked track. Returns whether the track was removed or not. */
	virtual bool RemoveBottomDockedTrack(TSharedPtr<FBaseTimingTrack> Track) = 0;

	/** Adds a new scrollable track. */
	virtual void AddScrollableTrack(TSharedPtr<FBaseTimingTrack> Track) = 0;
	/** Removes a scrollable track. Returns whether the track was removed or not. */
	virtual bool RemoveScrollableTrack(TSharedPtr<FBaseTimingTrack> Track) = 0;
	/** Marks the scrollable tracks as not being in the correct order, so they will be re-sorted. */
	virtual void InvalidateScrollableTracksOrder() = 0;

	/** Adds a new foreground track. */
	virtual void AddForegroundTrack(TSharedPtr<FBaseTimingTrack> Track) = 0;
	/** Removes a foreground track. Returns whether the track was removed or not. */
	virtual bool RemoveForegroundTrack(TSharedPtr<FBaseTimingTrack> Track) = 0;

	/** Adds a new track, specifying the location. */
	virtual void AddTrack(TSharedPtr<FBaseTimingTrack> Track, ETimingTrackLocation Location) = 0;
	/** Removes a track. Returns whether the track was removed or not. */
	virtual bool RemoveTrack(TSharedPtr<FBaseTimingTrack> Track) = 0;

	/** Finds a track has been added via Add*Track(). */
	virtual TSharedPtr<FBaseTimingTrack> FindTrack(uint64 InTrackId) = 0;

	//////////////////////////////////////////////////

	/** Gets the current marker time. */
	virtual double GetTimeMarker() const = 0;
	/** Sets the current marker time. */
	virtual void SetTimeMarker(double InTimeMarker) = 0;
	/** Sets the current marker time and center the view on it */
	virtual void SetAndCenterOnTimeMarker(double InTimeMarker) = 0;

	//////////////////////////////////////////////////

	/** Gets the delegate to be invoked when the selection have been changed. */
	virtual FSelectionChangedDelegate& OnSelectionChanged() = 0;
	/** Gets the delegate to be invoked when the time marker has changed. */
	virtual FTimeMarkerChangedDelegate& OnTimeMarkerChanged() = 0;
	/** Gets the delegate to be invoked when a custom time marker has changed. */
	virtual FCustomTimeMarkerChangedDelegate& OnCustomTimeMarkerChanged() = 0;

	/** Gets the delegate to be invoked when the timing track being hovered by the mouse has changed. */
	virtual FHoveredTrackChangedDelegate& OnHoveredTrackChanged() = 0;
	/** Gets the delegate to be invoked when the timing event being hovered by the mouse has changed. */
	virtual FHoveredEventChangedDelegate& OnHoveredEventChanged() = 0;

	/** Gets the delegate to be invoked when the selected timing track has changed. */
	virtual FSelectedTrackChangedDelegate& OnSelectedTrackChanged() = 0;
	/** Gets the delegate to be invoked when the selected timing event has changed. */
	virtual FSelectedEventChangedDelegate& OnSelectedEventChanged() = 0;

	/** Gets the delegate to be invoked when the track visibility has changed. */
	virtual FTrackVisibilityChangedDelegate& OnTrackVisibilityChanged() = 0;

	/** Gets the delegate to be invoked when a new track is added. */
	virtual Insights::FTrackAddedDelegate& OnTrackAdded() = 0;

	/** Gets the delegate to be invoked when a track is removed. */
	virtual Insights::FTrackRemovedDelegate& OnTrackRemoved() = 0;

	//////////////////////////////////////////////////

	/** Resets the selected event back to empty. */
	virtual void ResetSelectedEvent() = 0;

	/** Resets the event filter back to empty. */
	virtual void ResetEventFilter() = 0;

	//////////////////////////////////////////////////

	/** Prevents mouse movements from throttling application updates. */
	virtual void PreventThrottling() = 0;

	/** Adds a slot to the overlay. */
	virtual void AddOverlayWidget(const TSharedRef<SWidget>& InWidget) = 0;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
