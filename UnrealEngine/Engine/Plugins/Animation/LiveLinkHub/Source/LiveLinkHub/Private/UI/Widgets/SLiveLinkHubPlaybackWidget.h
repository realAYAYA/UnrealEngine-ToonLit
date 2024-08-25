// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FrameNumberDisplayFormat.h"
#include "Misc/FrameRate.h"
#include "Misc/QualifiedFrameTime.h"
#include "Widgets/SCompoundWidget.h"

class FLiveLinkHubPlaybackController;
class ULiveLinkRecording;
struct FFrameNumberInterface;

/**
 * The playback widget for controlling live link hub animations.
 */
class SLiveLinkHubPlaybackWidget : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_RetVal(bool, FOnGetChecked);
	DECLARE_DELEGATE_OneParam(FOnSetChecked, bool);
	DECLARE_DELEGATE_RetVal(FQualifiedFrameTime, FOnGetTime);
	DECLARE_DELEGATE_OneParam(FOnSetTime, FQualifiedFrameTime);
	DECLARE_DELEGATE_RetVal(FFrameRate, FOnGetFrame);
	DECLARE_DELEGATE_OneParam(FOnSetViewRange, const TRange<double>&)
	DECLARE_DELEGATE_RetVal(TRange<double>, FOnGetViewRange)

	DECLARE_DELEGATE(FOnButtonPressed);
	
	SLATE_BEGIN_ARGS(SLiveLinkHubPlaybackWidget) { }
	/** When forward play is pressed. */
	SLATE_EVENT(FOnButtonPressed, OnPlayForward)
	/** When reverse play is pressed. */
	SLATE_EVENT(FOnButtonPressed, OnPlayReverse)
	/** When go to first frame is pressed. */
	SLATE_EVENT(FOnButtonPressed, OnFirstFrame)
	/** When go to last frame is pressed. */
	SLATE_EVENT(FOnButtonPressed, OnLastFrame)
	/** When previous frame is pressed. */
	SLATE_EVENT(FOnButtonPressed, OnPreviousFrame)
	/** When next frame is pressed. */
	SLATE_EVENT(FOnButtonPressed, OnNextFrame)
	/** Is the recording looping? */
	SLATE_EVENT(FOnGetChecked, IsLooping)
	/** Set the loop state of the recording. */
	SLATE_EVENT(FOnSetChecked, OnSetLooping)
	/** Is the recording paused? */
	SLATE_EVENT(FOnGetChecked, IsPaused)
	/** Is the recording playing in reverse? */
	SLATE_EVENT(FOnGetChecked, IsInReverse)
	/** Get the current time of the recording. */
	SLATE_EVENT(FOnGetTime, GetCurrentTime)
	/** Set the current time of the recording. */
	SLATE_EVENT(FOnSetTime, SetCurrentTime)
	/** Get the total length of the recording. */
	SLATE_EVENT(FOnGetTime, GetTotalLength)
	/** Get the selection start of the recording. */
	SLATE_EVENT(FOnGetTime, GetSelectionStartTime)
	/** Set the selection start of the recording. */
	SLATE_EVENT(FOnSetTime, SetSelectionStartTime)
	/** Get the selection end of the recording. */
	SLATE_EVENT(FOnGetTime, GetSelectionEndTime)
	/** Set the selection end of the recording. */
	SLATE_EVENT(FOnSetTime, SetSelectionEndTime)
	/** Retrieve the frame rate for the recording. */
	SLATE_EVENT(FOnGetFrame, GetFrameRate)
	
	/** Get the view range (visible selection range). */
	SLATE_EVENT(FOnSetViewRange, SetViewRange)
	/** Set the view range (visible selection range). */
	SLATE_EVENT(FOnGetViewRange, GetViewRange)
	
	SLATE_END_ARGS()

	/**
	 * @param InArgs 
	 */
	void Construct(const FArguments& InArgs);

private:
	/** When play forward is pressed. */
	FReply OnPlayForwardPressed();
	/** When play reverse is pressed. */
	FReply OnPlayReversePressed();
	/** When first frame is pressed. */
	FReply OnFirstFramePressed();
	/** When last frame is pressed. */
	FReply OnLastFramePressed();
	/** When previous frame is pressed. */
	FReply OnPreviousFramePressed();
	/** When next frame is pressed. */
	FReply OnNextFramePressed();
	/** When loop recording is pressed. */
	FReply OnLoopPressed();

	/** Sets the current playhead time. */
	void SetCurrentTime(double InTime);

	/** When the playhead time is committed by a user change. */
	void OnCurrentTimeCommitted(double InTime, ETextCommit::Type InTextCommit);
	/** The current playhead time. */
	double GetCurrentTime() const;
	/** The total length of the recording. */
	double GetTotalLength() const;

	/** Retrieve the selection start time. */
	double GetSelectionStartTime() const;
	/** When the selection start time is committed by a user change. */
	void OnSelectionStartTimeCommitted(double InTime, ETextCommit::Type InTextCommit);
	/** Set the selection start time. */
	void SetSelectionStartTime(double InTime);

	/** Retrieve the selection end time. */
	double GetSelectionEndTime() const;
	/** When the selection end time is committed by a user change. */
	void OnSelectionEndTimeCommitted(double InTime, ETextCommit::Type InTextCommit);
	/** Set the selection end time. */
	void SetSelectionEndTime(double InTime);

	/** Retrieve the visible range of the scrubber. */
	TRange<double> GetViewRange() const;
	/** Set the visible range of the scrubber. */
	void SetViewRange(TRange<double> InRange);

	/** Retrieve the range to clamp playback to (the selection). */
	TRange<double> GetClampRange() const;

	/** Is playback paused? */
	bool IsPaused() const;
	/** Is playback in reverse? */
	bool IsPlayingInReverse() const;

	/** Retrieve the delta used when dragging the spinbox. */
	double GetSpinboxDelta() const;

	/** Frame or seconds to display for text and input. */
	EFrameNumberDisplayFormats GetDisplayFormat() const;

	/** Sets the display format. */
	void SetDisplayFormat(EFrameNumberDisplayFormats InDisplayFormat);

	/** Validates the current display format is set to the given format. */
	bool CompareDisplayFormat(EFrameNumberDisplayFormats InDisplayFormat) const;

	/** Retrieve the text version of the display format. */
	FText GetDisplayFormatAsText() const;
	
	/** The frame rate, used for number inputs. */
	FFrameRate GetFrameRate() const;

	/** Convert raw seconds to frame time. */
	FQualifiedFrameTime SecondsToFrameTime(double InTime) const;

	/** The forward icon to use for the play forward button. */
	const FSlateBrush* GetPlayForwardIcon() const;
	/** The reverse icon to use for the play reverse button. */
	const FSlateBrush* GetPlayReverseIcon() const;
	/** The loop icon to use for the loop button. */
	const FSlateBrush* GetLoopIcon() const;
	/** The tooltip for the loop button. */
	FText GetLoopTooltip() const;

	/** Create the dropdown box for playback settings. */
	TSharedRef<SWidget> MakePlaybackSettingsDropdown();
private:
	TWeakPtr<FLiveLinkHubPlaybackController> PlaybackController;
	/** Delegate for pressing play forward. */
	FOnButtonPressed OnPlayForwardDelegate;
	
	/** Delegate for pressing play reverse. */
	FOnButtonPressed OnPlayReverseDelegate;

	/** Delegate for pressing go to first frame. */
	FOnButtonPressed OnFirstFrameDelegate;

	/** Delegate for pressing go to last frame. */
	FOnButtonPressed OnLastFrameDelegate;

	/** Delegate for going to the previous frame. */
	FOnButtonPressed OnPreviousFrameDelegate;
	
	/** Delegate for going to the next frame. */
	FOnButtonPressed OnNextFrameDelegate;

	/** Delegate for checking if the recording playback is paused. */
	FOnGetChecked OnGetPausedDelegate;
	
	/** Delegate for checking if the recording playback is playing in reverse. */
	FOnGetChecked OnGetIsInReverseDelegate;

	/** Delegate for checking if the recording playback should loop. */
	FOnGetChecked OnGetLoopingDelegate;
	
	/** Delegate to set the looping option. */
	FOnSetChecked OnSetLoopingDelegate;
	
	/** Delegate for getting the total length of the recording. */
	FOnGetTime OnGetTotalLengthDelegate;

	/** Delegate for checking the playhead time. */
	FOnGetTime OnGetCurrentTimeDelegate;
	
	/** Delegate for setting the current playhead time. */
	FOnSetTime OnSetCurrentTimeDelegate;

	/** Delegate for getting the view range. */
	FOnGetViewRange OnGetViewRangeDelegate;
	
	/** Delegate for setting the view range. */
	FOnSetViewRange OnSetViewRangeDelegate;

	/** Delegate for getting the selection start. */
	FOnGetTime OnGetSelectionStartTimeDelegate;

	/** Delegate for setting the selection start. */
	FOnSetTime OnSetSelectionStartTimeDelegate;

	/** Delegate for getting the selection end. */
	FOnGetTime OnGetSelectionEndTimeDelegate;

	/** Delegate for setting the selection start. */
	FOnSetTime OnSetSelectionEndTimeDelegate;

	/** Retrieve the frame rate. */
	FOnGetFrame OnGetFrameRate;

	/** The number interface for displaying the correct frame format. */
	TSharedPtr<FFrameNumberInterface> NumberInterface;

	/** The format to display/edit values in. */
	EFrameNumberDisplayFormats DisplayFormat = EFrameNumberDisplayFormats::NonDropFrameTimecode;
};