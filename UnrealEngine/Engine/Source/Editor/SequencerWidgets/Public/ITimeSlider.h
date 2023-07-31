// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Input/CursorReply.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/NumericTypeInterface.h"
#include "ISequencerInputHandler.h"
#include "IMovieScenePlayer.h"
#include "MovieScene.h"

class FSlateWindowElementList;

/** Enum specifying how to interpolate to a new view range */
enum class EViewRangeInterpolation
{
	/** Use an externally defined animated interpolation */
	Animated,
	/** Set the view range immediately */
	Immediate,
};

/** Enum specifying how to find the nearest key */
enum class ENearestKeyOption : uint8
{
	NKO_None = 0x00,

	/* Search keys */
	NKO_SearchKeys = 0x01,

	/* Search sections */
	NKO_SearchSections = 0x02,

	/* Search markers */
	NKO_SearchMarkers = 0x04,

	/** Search all tracks */
	NKO_SearchAllTracks = 0x08
};
ENUM_CLASS_FLAGS(ENearestKeyOption);

DECLARE_DELEGATE_ThreeParams( FOnScrubPositionChanged, FFrameTime, bool, bool )
DECLARE_DELEGATE_TwoParams( FOnViewRangeChanged, TRange<double>, EViewRangeInterpolation )
DECLARE_DELEGATE_OneParam( FOnTimeRangeChanged, TRange<double> )
DECLARE_DELEGATE_OneParam( FOnFrameRangeChanged, TRange<FFrameNumber> )
DECLARE_DELEGATE_TwoParams(FOnSetMarkedFrame, int32, FFrameNumber)
DECLARE_DELEGATE_OneParam(FOnAddMarkedFrame, FFrameNumber)
DECLARE_DELEGATE_OneParam(FOnDeleteMarkedFrame, int32)
DECLARE_DELEGATE_RetVal_TwoParams( FFrameNumber, FOnGetNearestKey, FFrameTime, ENearestKeyOption )
DECLARE_DELEGATE_OneParam(FOnScrubPositionParentChanged, FMovieSceneSequenceID)

/** Structure used to wrap up a range, and an optional animation target */
struct FAnimatedRange : public TRange<double>
{
	/** Default Construction */
	FAnimatedRange() : TRange() {}
	/** Construction from a lower and upper bound */
	FAnimatedRange( double LowerBound, double UpperBound ) : TRange( LowerBound, UpperBound ) {}
	/** Copy-construction from simple range */
	FAnimatedRange( const TRange<double>& InRange ) : TRange(InRange) {}

	/** Helper function to wrap an attribute to an animated range with a non-animated one */
	static TAttribute<TRange<double>> WrapAttribute( const TAttribute<FAnimatedRange>& InAttribute )
	{
		typedef TAttribute<TRange<double>> Attr;
		return Attr::Create(Attr::FGetter::CreateLambda([=](){ return InAttribute.Get(); }));
	}

	/** Helper function to wrap an attribute to a non-animated range with an animated one */
	static TAttribute<FAnimatedRange> WrapAttribute( const TAttribute<TRange<double>>& InAttribute )
	{
		typedef TAttribute<FAnimatedRange> Attr;
		return Attr::Create(Attr::FGetter::CreateLambda([=](){ return InAttribute.Get(); }));
	}

	/** Get the current animation target, or the whole view range when not animating */
	const TRange<double>& GetAnimationTarget() const
	{
		return AnimationTarget.IsSet() ? AnimationTarget.GetValue() : *this;
	}
	
	/** The animation target, if animating */
	TOptional<TRange<double>> AnimationTarget;
};

struct FTimeSliderArgs
{
	FTimeSliderArgs()
		: ScrubPosition(0)
		, ViewRange( FAnimatedRange(0.0f, 5.0f) )
		, ClampRange( FAnimatedRange(-FLT_MAX/2.f, FLT_MAX/2.f) )
		, AllowZoom(true)
	{}

	/** The scrub position */
	TAttribute<FFrameTime> ScrubPosition;

	/** The scrub position text */
	TAttribute<FString> ScrubPositionText;

	/** The parent sequence that the scrub position display text is relative to */
	TAttribute<FMovieSceneSequenceID> ScrubPositionParent;

	/** Called when the scrub position parent sequence is changed */
	FOnScrubPositionParentChanged OnScrubPositionParentChanged;

	/** Attribute for the parent sequence chain of the current sequence */
	TAttribute<TArray<FMovieSceneSequenceID>> ScrubPositionParentChain;

	/** View time range */
	TAttribute< FAnimatedRange > ViewRange;

	/** Clamp time range */
	TAttribute< FAnimatedRange > ClampRange;

	/** Called when the scrub position changes */
	FOnScrubPositionChanged OnScrubPositionChanged;

	/** Called right before the scrubber begins to move */
	FSimpleDelegate OnBeginScrubberMovement;

	/** Called right after the scrubber handle is released by the user */
	FSimpleDelegate OnEndScrubberMovement;

	/** Called when the view range changes */
	FOnViewRangeChanged OnViewRangeChanged;

	/** Called when the clamp range changes */
	FOnTimeRangeChanged OnClampRangeChanged;

	/** Delegate that is called when getting the nearest key */
	FOnGetNearestKey OnGetNearestKey;

	/** Attribute defining the active sub-sequence range for this controller */
	TAttribute<TOptional<TRange<FFrameNumber>>> SubSequenceRange;

	/** Attribute defining the playback range for this controller */
	TAttribute<TRange<FFrameNumber>> PlaybackRange;

	/** Attribute for the current sequence's display rate */
	TAttribute<FFrameRate> DisplayRate;

	/** Attribute for the current sequence's tick resolution */
	TAttribute<FFrameRate> TickResolution;

	/** Delegate that is called when the playback range wants to change */
	FOnFrameRangeChanged OnPlaybackRangeChanged;

	/** Called right before the playback range starts to be dragged */
	FSimpleDelegate OnPlaybackRangeBeginDrag;

	/** Called right after the playback range has finished being dragged */
	FSimpleDelegate OnPlaybackRangeEndDrag;

	/** Attribute defining the selection range for this controller */
	TAttribute<TRange<FFrameNumber>> SelectionRange;

	/** Delegate that is called when the selection range wants to change */
	FOnFrameRangeChanged OnSelectionRangeChanged;

	/** Called right before the selection range starts to be dragged */
	FSimpleDelegate OnSelectionRangeBeginDrag;

	/** Called right after the selection range has finished being dragged */
	FSimpleDelegate OnSelectionRangeEndDrag;

	/** Called right before a mark starts to be dragged */
	FSimpleDelegate OnMarkBeginDrag;

	/** Called right after a mark has finished being dragged */
	FSimpleDelegate OnMarkEndDrag;

	/** Attribute for the current sequence's vertical frames */
	TAttribute<TSet<FFrameNumber>> VerticalFrames;

	/** Attribute for the current sequence's marked frames */
	TAttribute<TArray<FMovieSceneMarkedFrame>> MarkedFrames;

	/** Attribute for the marked frames that might need to be shown, but do not belong to the current sequence*/
	TAttribute<TArray<FMovieSceneMarkedFrame>> GlobalMarkedFrames;

	/** Called when the marked frame needs to be set */
	FOnSetMarkedFrame OnSetMarkedFrame;

	/** Called when a marked frame is added */
	FOnAddMarkedFrame OnAddMarkedFrame;

	/** Called when a marked frame is deleted */
	FOnDeleteMarkedFrame OnDeleteMarkedFrame;

	/** Called when all marked frames should be deleted */
	FSimpleDelegate OnDeleteAllMarkedFrames;

	/** Round the scrub position to an integer during playback */
	TAttribute<EMovieScenePlayerStatus::Type> PlaybackStatus;

	/** Attribute defining whether the playback range is locked */
	TAttribute<bool> IsPlaybackRangeLocked;

	/** Attribute defining the time snap interval */
	TAttribute<float> TimeSnapInterval;

	/** Called when toggling the playback range lock */
	FSimpleDelegate OnTogglePlaybackRangeLocked;

	/** If we are allowed to zoom */
	bool AllowZoom;

	/** Numeric Type interface for converting between frame numbers and display formats. */
	TSharedPtr<INumericTypeInterface<double>> NumericTypeInterface;
};

struct FPaintPlaybackRangeArgs
{
	FPaintPlaybackRangeArgs()
		: StartBrush(nullptr), EndBrush(nullptr), BrushWidth(0.f), SolidFillOpacity(0.f)
	{}

	FPaintPlaybackRangeArgs(const FSlateBrush* InStartBrush, const FSlateBrush* InEndBrush, float InBrushWidth)
		: StartBrush(InStartBrush), EndBrush(InEndBrush), BrushWidth(InBrushWidth)
	{}
	/** Brush to use for the start bound */
	const FSlateBrush* StartBrush;
	/** Brush to use for the end bound */
	const FSlateBrush* EndBrush;
	/** The width of the above brushes, in slate units */
	float BrushWidth;
	/** level of opacity for the fill color between the range markers */
	float SolidFillOpacity;
};

struct FPaintViewAreaArgs
{
	FPaintViewAreaArgs()
		: bDisplayTickLines(false), bDisplayScrubPosition(false), bDisplayMarkedFrames(false)
	{}

	/** Whether to display tick lines */
	bool bDisplayTickLines;
	/** Whether to display the scrub position */
	bool bDisplayScrubPosition;
	/** Whether to display the marked frames */
	bool bDisplayMarkedFrames;
	/** Optional Paint args for the playback range*/
	TOptional<FPaintPlaybackRangeArgs> PlaybackRangeArgs;
};


class ITimeSliderController : public ISequencerInputHandler
{
public:
	virtual ~ITimeSliderController(){}

	virtual int32 OnPaintTimeSlider( bool bMirrorLabels, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const = 0;

	virtual int32 OnPaintViewArea( const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bEnabled, const FPaintViewAreaArgs& Args ) const = 0;

	virtual FCursorReply OnCursorQuery( TSharedRef<const SWidget> WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const = 0;

	/** Get the current play rate for this controller */
	virtual FFrameRate GetDisplayRate() const = 0;

	/** Get the current tick resolution for this controller */
	virtual FFrameRate GetTickResolution() const = 0;

	/** Get the current view range for this controller */
	virtual FAnimatedRange GetViewRange() const { return FAnimatedRange(); }

	/** Get the current clamp range for this controller */
	virtual FAnimatedRange GetClampRange() const { return FAnimatedRange(); }

	/** Get the current play range for this controller */
	virtual TRange<FFrameNumber> GetPlayRange() const { return TRange<FFrameNumber>(); }

	/** Get the current selection range for this controller */
	virtual TRange<FFrameNumber> GetSelectionRange() const { return TRange<FFrameNumber>(); }

	/** Get the current time for the Scrub handle which indicates what range is being evaluated. */
	virtual FFrameTime GetScrubPosition() const { return FFrameTime(); }

	/** Set the current time for the Scrub handle which indicates what range is being evaluated. */
	virtual void SetScrubPosition(FFrameTime InTime, bool bEvaluate) {}

	/**
	 * Set a new range based on a min, max and an interpolation mode
	 * 
	 * @param NewRangeMin		The new lower bound of the range
	 * @param NewRangeMax		The new upper bound of the range
	 * @param Interpolation		How to set the new range (either immediately, or animated)
	 */
	virtual void SetViewRange( double NewRangeMin, double NewRangeMax, EViewRangeInterpolation Interpolation ) {}

	/**
	 * Set a new clamp range based on a min, max
	 * 
	 * @param NewRangeMin		The new lower bound of the clamp range
	 * @param NewRangeMax		The new upper bound of the clamp range
	 */
	virtual void SetClampRange( double NewRangeMin, double NewRangeMax) {}

	/**
	 * Set a new playback range based on a min, max
	 * 
	 * @param RangeStart		The new lower bound of the playback range
	 * @param RangeDuration		The total number of frames that we play for
	 */
	virtual void SetPlayRange( FFrameNumber RangeStart, int32 RangeDuration ) {}

	/**
	 * Set a new selection range
	 * 
	 * @param NewRange		The new selection range
	 */
	virtual void SetSelectionRange(const TRange<FFrameNumber>& NewRange) {}
};

/**
 * Base class for a widget that scrubs time or frames
 */
class ITimeSlider : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(ITimeSlider){}
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()
};
