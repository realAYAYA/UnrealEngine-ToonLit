// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Geometry.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Rendering/RenderingCommon.h"

#include "EventHandlers/ISignedObjectEventHandler.h"
#include "MVVM/ViewModels/SectionModel.h"
#include "MVVM/Extensions/ITrackLaneExtension.h"

class FPaintArgs;
class FSequencer;
class FSequencerSectionPainter;
class FSlateWindowElementList;

namespace UE
{
namespace Sequencer
{

class SCompoundTrackLaneView;
class FSectionModel;
struct ITrackAreaHotspot;

class SSequencerSection : public SCompoundWidget, public ITrackLaneWidget, public UE::MovieScene::ISignedObjectEventHandler
{
public:
	SLATE_BEGIN_ARGS( SSequencerSection )
	{}
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, TSharedPtr<FSequencer> Sequencer, TSharedPtr<FSectionModel> InSectionModel, TSharedPtr<STrackLane> InOwningTrackLane);
	~SSequencerSection();

	TSharedPtr<ISequencerSection> GetSectionInterface() const { return SectionInterface; }

	virtual FVector2D ComputeDesiredSize(float) const override;

private:

	/**
	 * Checks for user interaction (via the mouse) with the left and right edge of a section
	 *
	 * @param MousePosition		The current screen space position of the mouse
	 * @param SectionGeometry	The geometry of the section
	 */
	bool CheckForEdgeInteraction( const FPointerEvent& MousePosition, const FGeometry& SectionGeometry );

	bool CheckForEasingHandleInteraction( const FPointerEvent& MousePosition, const FGeometry& SectionGeometry );

	bool CheckForEasingAreaInteraction( const FPointerEvent& MousePosition, const FGeometry& SectionGeometry );

	/** SWidget interface */
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave( const FPointerEvent& MouseEvent ) override;

	/*~ ITrackLaneWidget Interface */
	virtual FTrackLaneScreenAlignment GetAlignment(const FTimeToPixel& TimeToPixel, const FGeometry& InParentGeometry) const override;
	virtual int32 GetOverlapPriority() const override;
	virtual void ReportParentGeometry(const FGeometry& InParentGeometry) override;
	virtual TSharedRef<const SWidget> AsWidget() const override { return AsShared(); }
	virtual bool AcceptsChildren() const override { return true; }
	virtual void AddChildView(TSharedPtr<ITrackLaneWidget> ChildWidget, TWeakPtr<STrackLane> InWeakOwningLane) override;

	/**
	 * Paint the easing handles for this section
	 */
	void PaintEasingHandles( FSequencerSectionPainter& InPainter, FLinearColor SelectionColor, TSharedPtr<ITrackAreaHotspot> Hotspot ) const;

	/**
	 * Draw the section resize handles.
	 */
	void DrawSectionHandles( const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, ESlateDrawEffect DrawEffects, FLinearColor SelectionColor, TSharedPtr<ITrackAreaHotspot> Hotspot ) const;

	/** @return the sequencer interface */
	FSequencer& GetSequencer() const;

	/** 
	 * Creates geometry for a section without space for the handles
	 */
	FGeometry MakeSectionGeometryWithoutHandles( const FGeometry& AllottedGeometry, const TSharedPtr<ISequencerSection>& InSectionInterface ) const;

	/**
	 * Ensure that the cached array of underlapping sections is up to date
	 */
	void UpdateUnderlappingSegments();

	/**
	 * Retrieve the tooltip text for this section
	 */
	FText GetToolTipText() const;

	/**
	 * Check whether this section widget is enabled or not
	 */
	bool IsEnabled() const;

	void OnModifiedIndirectly(UMovieSceneSignedObject* Object) override;

	EVisibility GetTopLevelChannelGroupVisibility() const;

	FLinearColor GetTopLevelKeyBarColor() const;

	FMargin GetHandleOffsetPadding() const;

public:

	/** Indicate that the current section selection should throb the specified number of times. A single throb takes 0.2s. */
	static void ThrobSectionSelection(int32 ThrobCount = 1);

	/** Indicate that the current key selection should throb the specified number of times. A single throb takes 0.2s. */
	static void ThrobKeySelection(int32 ThrobCount = 1);

	/** Get a value between 0 and 1 that indicates the amount of throb-scale to apply to the currently selected keys */
	static float GetKeySelectionThrobValue();

	/** Get a value between 0 and 1 that indicates the amount of throb-scale to apply to the currently selected sections */
	static float GetSectionSelectionThrobValue();

	/**
	 * Check to see whether the specified section is highlighted
	 */
	static bool IsSectionHighlighted(UMovieSceneSection* InSection, TSharedPtr<ITrackAreaHotspot> Hotspot);

private:
	TWeakPtr<FSequencer> Sequencer;
	/** Interface to section data */
	TSharedPtr<ISequencerSection> SectionInterface;
	/** Section model */
	TWeakPtr<FSectionModel> WeakSectionModel;
	/** Widget container for child lanes */
	TSharedPtr<SCompoundTrackLaneView> ChildLaneWidgets;
	/** The track lane that this widget is on */
	TWeakPtr<STrackLane> WeakOwningTrackLane;
	/** Cached parent geometry to pass down to any section interfaces that need it during tick */
	FGeometry ParentGeometry;
	/** The end time for a throbbing animation for selected sections */
	static double SectionSelectionThrobEndTime;
	/** The end time for a throbbing animation for selected keys */
	static double KeySelectionThrobEndTime;
	/** Handle offset amount in pixels */
	float HandleOffsetPx;
	/** Array of segments that define other sections that reside below this one */
	TArray<FOverlappingSections> UnderlappingSegments;
	/** Array of segments that define other sections that reside below this one */
	TArray<FOverlappingSections> UnderlappingEasingSegments;

	MovieScene::TNonIntrusiveEventHandler<MovieScene::ISignedObjectEventHandler> TrackModifiedBinding;


	friend struct FSequencerSectionPainterImpl;
};

} // namespace Sequencer
} // namespace UE

