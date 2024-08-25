// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "ITimeSlider.h"
#include "ISequencerModule.h"
#include "TimeSliderArgs.h"

class FSlateWindowElementList;
struct FContextMenuSuppressor;
struct FSlateBrush;
class FSlateFontMeasure;
class FSequencer;
class IPropertyTypeCustomization;

/**
 * A time slider controller for sequencer
 * Draws and manages time data for a Sequencer
 */
class FSequencerTimeSliderController : public ITimeSliderController, public TSharedFromThis<FSequencerTimeSliderController>
{
public:
	FSequencerTimeSliderController( const FTimeSliderArgs& InArgs, TWeakPtr<FSequencer> InWeakSequencer );
	~FSequencerTimeSliderController();

	/**
	* Determines the optimal spacing between tick marks in the slider for a given pixel density
	* Increments until a minimum amount of slate units specified by MinTick is reached
	*
	* @param InPixelsPerInput	The density of pixels between each input
	* @param MinTick			The minimum slate units per tick allowed
	* @param MinTickSpacing	The minimum tick spacing in time units allowed
	* @return the optimal spacing in time units
	*/
	float DetermineOptimalSpacing(float InPixelsPerInput, uint32 MinTick, float MinTickSpacing) const;

	/** ITimeSliderController Interface */
	virtual int32 OnPaintTimeSlider( bool bMirrorLabels, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual FReply OnMouseButtonDown( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseWheel( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnTimeSliderMouseMove( SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FCursorReply OnCursorQuery( TSharedRef<const SWidget> WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const override;
	virtual FReply OnMouseButtonDoubleClick( TSharedRef<const SWidget> WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	/** End ITimeSliderController Interface */

	/** Get the current play rate for this controller */
	virtual FFrameRate GetDisplayRate() const override { return TimeSliderArgs.DisplayRate.Get(); }

	/** Get the current tick resolution for this controller */
	virtual FFrameRate GetTickResolution() const override { return TimeSliderArgs.TickResolution.Get(); }

	/** Get the current view range for this controller */
	virtual FAnimatedRange GetViewRange() const override { return TimeSliderArgs.ViewRange.Get(); }

	/** Get the current clamp range for this controller in seconds. */
	virtual FAnimatedRange GetClampRange() const override { return TimeSliderArgs.ClampRange.Get(); }

	/** Get the current play range for this controller */
	virtual TRange<FFrameNumber> GetPlayRange() const override { return TimeSliderArgs.PlaybackRange.Get(TRange<FFrameNumber>()); }

	/** Get the time bounds for this controller. The time bounds should be a subset of the playback range. */
	virtual TRange<FFrameNumber> GetTimeBounds() const override { return TimeSliderArgs.TimeBounds.Get(TRange<FFrameNumber>()); }

	/** Get the selection range */
	virtual TRange<FFrameNumber> GetSelectionRange() const override { return TimeSliderArgs.SelectionRange.Get(TRange<FFrameNumber>()); }

	/** Get the current time for the Scrub handle which indicates what range is being evaluated. */
	virtual FFrameTime GetScrubPosition() const override { return TimeSliderArgs.ScrubPosition.Get(FFrameTime()); }

	/** Get the current time for the Scrub handle which indicates what range is being evaluated. */
	virtual void SetScrubPosition(FFrameTime InTime, bool bEvaluate) override { CommitScrubPosition(InTime, GetPlaybackStatus() == ETimeSliderPlaybackStatus::Scrubbing, bEvaluate); }

	/** Set the playback status for the controller*/
	virtual void SetPlaybackStatus(ETimeSliderPlaybackStatus InStatus) override;

	/** Get the playback status for the controller, by default it is ETimeSliderPlaybackStatus::Stopped */
	virtual ETimeSliderPlaybackStatus GetPlaybackStatus() const override;

	/**
	 * Clamp the given range to the clamp range 
	 *
	 * @param NewRangeMin		The new lower bound of the range
	 * @param NewRangeMax		The new upper bound of the range
	 */	
	virtual void ClampViewRange(double& NewRangeMin, double& NewRangeMax);

	/**
	 * Set a new range based on a min, max and an interpolation mode
	 * 
	 * @param NewRangeMin		The new lower bound of the range
	 * @param NewRangeMax		The new upper bound of the range
	 * @param Interpolation		How to set the new range (either immediately, or animated)
	 */
	virtual void SetViewRange( double NewRangeMin, double NewRangeMax, EViewRangeInterpolation Interpolation ) override;

	/**
	 * Set a new clamp range based on a min, max
	 * 
	 * @param NewRangeMin		The new lower bound of the clamp range
	 * @param NewRangeMax		The new upper bound of the clamp range
	 */
	virtual void SetClampRange( double NewRangeMin, double NewRangeMax ) override;

	/**
	 * Set a new playback range based on a min, max
	 * 
	 * @param RangeStart		The new lower bound of the playback range
	 * @param RangeDuration		The total number of frames that we play for
	 */
	virtual void SetPlayRange( FFrameNumber RangeStart, int32 RangeDuration ) override;

	/**
	 * Set a new selection range
	 * 
	 * @param NewRange		The new selection range
	 */
	virtual void SetSelectionRange(const TRange<FFrameNumber>& NewRange) override;

	/**
	 * Zoom the range by a given delta.
	 * 
	 * @param InDelta		The total amount to zoom by (+ve = zoom out, -ve = zoom in)
	 * @param ZoomBias		Bias to apply to lower/upper extents of the range. (0 = lower, 0.5 = equal, 1 = upper)
	 */
	bool ZoomByDelta( float InDelta, float ZoomBias = 0.5f );

	/**
	 * Pan the range by a given delta
	 * 
	 * @param InDelta		The total amount to pan by (+ve = pan forwards in time, -ve = pan backwards in time)
	 */
	void PanByDelta( float InDelta );

	/**
	 * Draws major tick lines in the section view                                                              
	 */
	int32 OnPaintViewArea( const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bEnabled, const FPaintViewAreaArgs& Args ) const;



public:

	struct FScrubberMetrics
	{
		/** The extents of the current frame that the scrubber is on, in pixels */
		TRange<float> FrameExtentsPx;
		/** The pixel range that the scrubber handle (thumb) occupies */
		TRange<float> HandleRangePx;
		/** The style of the scrubber handle */
		ESequencerScrubberStyle Style;
		/** The style of the scrubber handle */
		bool bDrawExtents;
	};

	/** Utility struct for converting between scrub range space and local/absolute screen space */
	struct FScrubRangeToScreen
	{
		double ViewStart;
		float  PixelsPerInput;

		FScrubRangeToScreen(const TRange<double>& InViewInput, const FVector2D& InWidgetSize)
		{
			float ViewInputRange = InViewInput.Size<double>();

			ViewStart = InViewInput.GetLowerBoundValue();
			PixelsPerInput = ViewInputRange > 0 ? (InWidgetSize.X / ViewInputRange) : 0;
		}

		/** Local Widget Space -> Curve Input domain. */
		double LocalXToInput(float ScreenX) const
		{
			return PixelsPerInput > 0 ? (ScreenX / PixelsPerInput) + ViewStart : ViewStart;
		}

		/** Local Widget Space -> Curve Input domain. */
		double LocalDeltaXToDeltaInput(float ScreenDeltaX) const
		{
			return PixelsPerInput > 0 ? (ScreenDeltaX / PixelsPerInput) : 0;
		}

		/** Curve Input domain -> local Widget Space */
		float InputToLocalX(double Input) const
		{
			return (Input - ViewStart) * PixelsPerInput;
		}
	};

	/** Set that's evaluating */
	void SetIsEvaluating()
	{
		bIsEvaluating = true;
	}

private:

	FReply OnMouseMoveImpl( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bFromTimeSlider );

private:
	// forward declared as class members to prevent name collision with similar types defined in other units
	struct FDrawTickArgs;

	/**
	 * Call this method when the user's interaction has changed the scrub position
	 *
	 * @param NewValue				Value resulting from the user's interaction
	 * @param bIsScrubbing			True if done via scrubbing, false if just releasing scrubbing
	 * @param bEvaluate				If true evaluate, if not just change time
	 */
	void CommitScrubPosition( FFrameTime NewValue, bool bIsScrubbing, bool bEvaluate);

	/**
	 * Draw time tick marks
	 *
	 * @param OutDrawElements	List to add draw elements to
	 * @param ViewRange			The currently visible time range in seconds
	 * @param RangeToScreen		Time range to screen space converter
	 * @param InArgs			Parameters for drawing the tick lines
	 */
	void DrawTicks( FSlateWindowElementList& OutDrawElements, const TRange<double>& ViewRange, const FScrubRangeToScreen& RangeToScreen, FDrawTickArgs& InArgs ) const;

	/**
	 * Draw the selection range.
	 *
	 * @return The new layer ID.
	 */
	int32 DrawSelectionRange(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FScrubRangeToScreen& RangeToScreen, const FPaintPlaybackRangeArgs& Args) const;

	/**
	 * Draw the playback range.
	 *
	 * @return the new layer ID
	 */
	int32 DrawPlaybackRange(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FScrubRangeToScreen& RangeToScreen, const FPaintPlaybackRangeArgs& Args) const;

	/**
	 * Draw the playback range.
	 *
	 * @return the new layer ID
	 */
	int32 DrawSubSequenceRange(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FScrubRangeToScreen& RangeToScreen, const FPaintPlaybackRangeArgs& Args) const;

	/**
	 * Draw the vertical frames.
	 *
	 * @return the new layer ID
	 */
	int32 DrawVerticalFrames(const FGeometry& AllottedGeometry, const FScrubRangeToScreen& RangeToScreen, FSlateWindowElementList& OutDrawElements, int32 LayerId, const ESlateDrawEffect& DrawEffects) const;

	/**
	 * Draw the marked frames.
	 *
	 * @return the new layer ID
	 */
	int32 DrawMarkedFrames(const FGeometry& AllottedGeometry, const FScrubRangeToScreen& RangeToScreen, FSlateWindowElementList& OutDrawElements, int32 LayerId, const ESlateDrawEffect& DrawEffects, const FWidgetStyle& InWidgetStyle, bool bDrawLabels) const;

private:

	/**
	 * Hit test the lower bound of a range
	 */
	bool HitTestRangeStart(const FScrubRangeToScreen& RangeToScreen, const TRange<double>& Range, float HitPixel) const;

	/**
	 * Hit test the upper bound of a range
	 */
	bool HitTestRangeEnd(const FScrubRangeToScreen& RangeToScreen, const TRange<double>& Range, float HitPixel) const;

	/**
	 * Hit test marks
	 *
	 * @return The mark index hit
	 */
	bool HitTestMark(const FGeometry& AllottedGeometry, const FScrubRangeToScreen& RangeToScreen, float HitPixel, bool bTestLabelBox, int32* OutMarkIndex = nullptr, FFrameNumber* OutMarkFrameNumber = nullptr) const;

	/**
	 * Get marked frame label box size
	 */
	void GetMarkLabelGeometry(const FGeometry& AllottedGeometry, const FScrubRangeToScreen& RangeToScreen, const FMovieSceneMarkedFrame& MarkedFrame, FVector2D& OutPosition, FVector2D& OutSize, bool& bIsDrawLeft) const;

	FFrameTime SnapTimeToNearestKey(const FPointerEvent& MouseEvent, const FScrubRangeToScreen& RangeToScreen, float CursorPos, FFrameTime InTime) const;

	void SetPlaybackRangeStart(FFrameNumber NewStart);
	void SetPlaybackRangeEnd(FFrameNumber NewEnd);

	void SetSelectionRangeStart(FFrameNumber NewStart);
	void SetSelectionRangeEnd(FFrameNumber NewEnd);

	void SetMark(FFrameNumber DiffFrame);

	TSharedRef<SWidget> OpenSetPlaybackRangeMenu(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	FFrameTime ComputeScrubTimeFromMouse(const FGeometry& Geometry, const FPointerEvent& MouseEvent, FScrubRangeToScreen RangeToScreen) const;
	FFrameTime ComputeFrameTimeFromMouse(const FGeometry& Geometry, FVector2D ScreenSpacePosition, FScrubRangeToScreen RangeToScreen, bool CheckSnapping = true) const;

	void HandleMarkSelection(int32 InMarkIndex);
	void AddMarkAtFrame(FFrameNumber FrameNumber);
	void DeleteMarkAtIndex(int32 InMarkIndex);
	void DeleteAllMarks();

private:

	/**
	 * Get the pixel matrics of the Scrubber
	 * @param ScrubTime			The qualified time of the scrubber
	 * @param RangeToScreen		Range to screen helper
	 * @param DilationPixels	Number of pixels to dilate the handle by
	 * return FScrubberMetrics struct
	 */
	FScrubberMetrics GetScrubPixelMetrics(const FQualifiedFrameTime& ScrubTime, const FScrubRangeToScreen& RangeToScreen, float DilationPixels = 0.f) const;

	FScrubberMetrics GetHitTestScrubPixelMetrics(const FScrubRangeToScreen& RangeToScreen) const;

private:

	/** Pointer back to the sequencer object */
	TWeakPtr<FSequencer> WeakSequencer;

	FTimeSliderArgs TimeSliderArgs;

	/** Brush for drawingthe fill area on the scrubber */
	const FSlateBrush* ScrubFillBrush;

	/** Brush for drawing an upwards facing scrub handles */
	const FSlateBrush* FrameBlockScrubHandleUpBrush, *VanillaScrubHandleUpBrush;
	
	/** Brush for drawing a downwards facing scrub handle */
	const FSlateBrush* FrameBlockScrubHandleDownBrush, *VanillaScrubHandleDownBrush;

	/** Font measure service */
	TSharedPtr<FSlateFontMeasure> FontMeasureService;

	/** Font info for the marked frames labels */
	FSlateFontInfo SmallLayoutFont;
	FSlateFontInfo SmallBoldLayoutFont;
	
	/** Total mouse delta during dragging **/
	float DistanceDragged;
	
	/** If we are dragging a scrubber or dragging to set the time range */
	enum DragType
	{
		DRAG_SCRUBBING_TIME,
		DRAG_SETTING_RANGE,
		DRAG_PLAYBACK_START,
		DRAG_PLAYBACK_END,
		DRAG_SELECTION_START,
		DRAG_SELECTION_END,
		DRAG_MARK,
		DRAG_NONE
	};
	
	DragType MouseDragType;
	
	/** If mouse down was in time scrubbing region, only allow setting time when mouse is pressed down in the region */
	bool bMouseDownInRegion;

	/** If we are currently panning the panel */
	bool bPanning;

	/** Mouse down position range */
	TOptional<FVector2D> MouseDownPosition[2];

	/** Geometry on mouse down */
	FGeometry MouseDownGeometry;

	/** Playback range when the mouse is first pressed down */
	TRange<FFrameNumber> MouseDownPlaybackRange;

	/** Selection range when the mouse is first pressed down */
	TRange<FFrameNumber> MouseDownSelectionRange;

	/** Range stack */
	TArray<TRange<double>> ViewRangeStack;

	/** Index of mark being hovered */
	int32 HoverMarkIndex;

	/** Map of the indices of the marks being edited and their initial frame numbers when pressed */
	TMap<int32, FFrameNumber> DragMarkMap;

	/** When > 0, we should not show context menus */
	int32 ContextMenuSuppression;
	
	/** If evaluating, if not we draw the time box to be yellow not default*/
	bool bIsEvaluating = true;

	friend FContextMenuSuppressor;
	
};

struct FContextMenuSuppressor
{
	FContextMenuSuppressor(TSharedRef<FSequencerTimeSliderController> InTimeSliderController)
		: TimeSliderController(InTimeSliderController)
	{
		++TimeSliderController->ContextMenuSuppression;
	}
	~FContextMenuSuppressor()
	{
		--TimeSliderController->ContextMenuSuppression;
	}

private:
	FContextMenuSuppressor(const FContextMenuSuppressor&);
	FContextMenuSuppressor& operator=(const FContextMenuSuppressor&);

	TSharedRef<FSequencerTimeSliderController> TimeSliderController;
};
