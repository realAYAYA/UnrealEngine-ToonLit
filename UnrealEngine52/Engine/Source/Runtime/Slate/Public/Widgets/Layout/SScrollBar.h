// Copyright Epic Games, Inc. All Rights Reserved.
 
#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Styling/SlateColor.h"
#include "Input/Reply.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/SlateWidgetStyleAsset.h"

class SScrollBarTrack;
class SSpacer;
class SImage;

DECLARE_DELEGATE_OneParam(
	FOnUserScrolled,
	float );	/** ScrollOffset as a fraction between 0 and 1 */


class SScrollBarTrack;

class SLATE_API SScrollBar : public SBorder
{
public:

	SLATE_BEGIN_ARGS( SScrollBar )
		: _Style( &FAppStyle::Get().GetWidgetStyle<FScrollBarStyle>("Scrollbar") )
		, _OnUserScrolled()
		, _AlwaysShowScrollbar(false)
		, _AlwaysShowScrollbarTrack(true)
#if PLATFORM_UI_HAS_MOBILE_SCROLLBARS
		, _HideWhenNotInUse(true)
#else
		, _HideWhenNotInUse(false)
#endif
		, _PreventThrottling(false)
		, _Orientation( Orient_Vertical )
		, _DragFocusCause( EFocusCause::Mouse )
		, _Thickness()
		, _Padding( 2.0f )
		{}

		/** The style to use for this scrollbar */
		SLATE_STYLE_ARGUMENT( FScrollBarStyle, Style )
		SLATE_EVENT( FOnUserScrolled, OnUserScrolled )
		SLATE_ARGUMENT( bool, AlwaysShowScrollbar )
		SLATE_ARGUMENT( bool, AlwaysShowScrollbarTrack )
		SLATE_ARGUMENT( bool, HideWhenNotInUse )
		SLATE_ARGUMENT( bool, PreventThrottling )
		SLATE_ARGUMENT( EOrientation, Orientation )
		SLATE_ARGUMENT( EFocusCause, DragFocusCause )
		/** The thickness of the scrollbar thumb */
		SLATE_ATTRIBUTE( FVector2D, Thickness )
		/** The margin around the scrollbar */
		SLATE_ATTRIBUTE( FMargin, Padding )
	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	void Construct(const FArguments& InArgs);

	/**
	 * Set the handler to be invoked when the user scrolls.
	 *
	 * @param InHandler   Method to execute when the user scrolls the scrollbar
	 */
	void SetOnUserScrolled( const FOnUserScrolled& InHandler );

	/**
	 * Set the offset and size of the track's thumb.
	 * Note that the maximum offset is 1.0-ThumbSizeFraction.
	 * If the user can view 1/3 of the items in a single page, the maximum offset will be ~0.667f
	 *
	 * @param InOffsetFraction     Offset of the thumbnail from the top as a fraction of the total available scroll space.
	 * @param InThumbSizeFraction  Size of thumbnail as a fraction of the total available scroll space.
	 * @param bCallOnUserScrolled  If true, OnUserScrolled will be called with InOffsetFraction
	 */
	virtual void SetState( float InOffsetFraction, float InThumbSizeFraction, bool bCallOnUserScrolled = false);

	// SWidget
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	// SWidget

	/** @return true if scrolling is possible; false if the view is big enough to fit all the content */
	bool IsNeeded() const;

	/** @return normalized distance from top */
	float DistanceFromTop() const;

	/** @return normalized distance from bottom */
	float DistanceFromBottom() const;

	/** @return normalized percentage of track covered by thumb bar */
	float ThumbSizeFraction() const;

	/** @return the scrollbar's visibility as a product of internal rules and user-specified visibility */
	EVisibility ShouldBeVisible() const;

	/** @return True if the user is scrolling by dragging the scroll bar thumb. */
	bool IsScrolling() const;

	/** @return the orientation in which the scrollbar is scrolling. */
	EOrientation GetOrientation() const; 

	/** Set argument Style */
	void SetStyle(const FScrollBarStyle* InStyle);

	/** Invalidate the style */
	void InvalidateStyle();

	/** Set UserVisibility attribute */
	void SetUserVisibility(TAttribute<EVisibility> InUserVisibility) { UserVisibility = InUserVisibility; }

	/** Set DragFocusCause attribute */
	void SetDragFocusCause(EFocusCause InDragFocusCause);

	/** Set Thickness attribute */
	void SetThickness(TAttribute<FVector2D> InThickness);

	/** Set ScrollBarAlwaysVisible attribute */
	void SetScrollBarAlwaysVisible(bool InAlwaysVisible);

	/** Set ScrollBarTrackAlwaysVisible attribute */
	void SetScrollBarTrackAlwaysVisible(bool InAlwaysVisible);

	/** Returns True when the scrollbar should always be shown, else False */
	bool AlwaysShowScrollbar() const;

	/** Allows external scrolling panels to notify the scrollbar when scrolling begins. */
	virtual void BeginScrolling();

	/** Allows external scrolling panels to notify the scrollbar when scrolling ends. */
	virtual void EndScrolling();

	SScrollBar();

protected:
	
	/** Execute the on user scrolled delegate */
	void ExecuteOnUserScrolled( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent );

	/** We fade the scroll track unless it is being hovered*/
	FSlateColor GetTrackOpacity() const;

	/** We always show a subtle scroll thumb, but highlight it extra when dragging */
	FLinearColor GetThumbOpacity() const;

	/** @return the name of an image for the scrollbar thumb based on whether the user is dragging or hovering it. */
	const FSlateBrush* GetDragThumbImage() const;

	/** The scrollbar's visibility as specified by the user. Will be compounded with internal visibility rules. */
	TAttribute<EVisibility> UserVisibility;

	TSharedPtr<SImage> TopImage;
	TSharedPtr<SImage> BottomImage;
	TSharedPtr<SBorder> DragThumb;
	TSharedPtr<SSpacer> ThicknessSpacer;
	bool bDraggingThumb;
	TSharedPtr<SScrollBarTrack> Track;
	FOnUserScrolled OnUserScrolled;
	float DragGrabOffset;
	EOrientation Orientation;
	bool bAlwaysShowScrollbar;
	bool bAlwaysShowScrollbarTrack;
	EFocusCause DragFocusCause;
	bool bHideWhenNotInUse;
	/*
	 * Holds whether or not to prevent throttling during mouse capture
	 * When true, the viewport will be updated with every single change to the value during dragging
	 */
	bool bPreventThrottling;
	bool bIsScrolling;
	double LastInteractionTime;

	/** Image to use when the scrollbar thumb is in its normal state */
	const FSlateBrush* NormalThumbImage;
	/** Image to use when the scrollbar thumb is in its hovered state */
	const FSlateBrush* HoveredThumbImage;
	/** Image to use when the scrollbar thumb is in its dragged state */
	const FSlateBrush* DraggedThumbImage;
	/** Background brush */
	const FSlateBrush* BackgroundBrush;
	/** Top brush */
	const FSlateBrush* TopBrush;
	/** Bottom brush */
	const FSlateBrush* BottomBrush;
};
