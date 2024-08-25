// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Layout/Geometry.h"
#include "Rendering/RenderingCommon.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

// Insights
#include "Insights/Common/FixedCircularBuffer.h"
#include "Insights/ViewModels/FrameTrackHelper.h"
#include "Insights/ViewModels/FrameTrackViewport.h"

class SScrollBar;
class STimingView;

////////////////////////////////////////////////////////////////////////////////////////////////////

struct FFrameTrackSampleRef
{
	TSharedPtr<FFrameTrackSeries> Series;
	TSharedPtr<FFrameTrackSample> Sample;

	FFrameTrackSampleRef()
		: Series(), Sample()
	{
	}

	FFrameTrackSampleRef(TSharedPtr<FFrameTrackSeries> InSeries, TSharedPtr<FFrameTrackSample> InSample)
		: Series(InSeries), Sample(InSample)
	{
	}

	FFrameTrackSampleRef(const FFrameTrackSampleRef& Other)
		: Series(Other.Series), Sample(Other.Sample)
	{
	}

	FFrameTrackSampleRef& operator=(const FFrameTrackSampleRef& Other)
	{
		Series = Other.Series;
		Sample = Other.Sample;
		return *this;
	}

	void Reset()
	{
		Series.Reset();
		Sample.Reset();
	}

	bool IsValid() const
	{
		return Series.IsValid() && Sample.IsValid();
	}

	bool Equals(const FFrameTrackSampleRef& Other) const
	{
		return Series == Other.Series
			&& ((Sample == Other.Sample) || (Sample.IsValid() && Other.Sample.IsValid() && Sample->Equals(*Other.Sample)));
	}

	static bool AreEquals(const FFrameTrackSampleRef& A, const FFrameTrackSampleRef& B)
	{
		return A.Equals(B);
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Widget used to present frames data in a bar track.
 */
class SFrameTrack : public SCompoundWidget
{
public:
	/** Number of pixels. */
	static constexpr float MOUSE_SNAP_DISTANCE = 2.0f;

	enum class ECursorType
	{
		Default,
		Arrow,
		Hand,
	};

public:
	/** Default constructor. */
	SFrameTrack();

	/** Virtual destructor. */
	virtual ~SFrameTrack();

	/** Resets internal widget's data to the default one. */
	void Reset();

	SLATE_BEGIN_ARGS(SFrameTrack)
	{
		_Clipping = EWidgetClipping::ClipToBounds;
	}
	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 * @param InArgs The declaration data for this widget
	 */
	void Construct(const FArguments& InArgs);

	/**
	 * Ticks this widget. Override in derived classes, but always call the parent implementation.
	 * @param  AllottedGeometry The space allotted for this widget
	 * @param  InCurrentTime  Current absolute real time
	 * @param  InDeltaTime  Real time passed since last tick
	 */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

	bool HasFrameStatSeries(ETraceFrameType FrameType, uint32 TimerId);
	TSharedPtr<FTimerFrameStatsTrackSeries> AddTimerFrameStatSeries(ETraceFrameType FrameType, uint32 TimerId, FLinearColor Color, FText Name);
	bool RemoveTimerFrameStatSeries(ETraceFrameType FrameType, uint32 TimerId);

	uint32 GetNumSeriesForTimer(uint32 TimerId);

protected:
	TSharedRef<FFrameTrackSeries> FindOrAddSeries(ETraceFrameType FrameType);
	TSharedPtr<FFrameTrackSeries> FindSeries(ETraceFrameType FrameType) const;
	TSharedPtr<FFrameTrackSeries> FindFrameStatsSeries(ETraceFrameType FrameType, uint32 TimerId) const;
	void UpdateState();

	void DrawHorizontalAxisGrid(FDrawContext& DrawContext, const FSlateBrush* Brush, const FSlateFontInfo& Font, bool bDrawBackgroundLayer) const;
	void DrawVerticalAxisGrid(FDrawContext& DrawContext, const FSlateBrush* Brush, const FSlateFontInfo& Font) const;

	FFrameTrackSampleRef GetSampleAtMousePosition(double X, double Y);
	void SelectFrameAtMousePosition(double X, double Y, bool JoinCurrentSelection);

	void ShowContextMenu(const FPointerEvent& MouseEvent);

	void ContextMenu_ShowGameFrames_Execute();
	bool ContextMenu_ShowGameFrames_CanExecute();
	bool ContextMenu_ShowGameFrames_IsChecked();

	void ContextMenu_ShowRenderingFrames_Execute();
	bool ContextMenu_ShowRenderingFrames_CanExecute();
	bool ContextMenu_ShowRenderingFrames_IsChecked();

	void ContextMenu_ShowFrameStats_Execute(ETraceFrameType FrameType, uint32 TimerId);
	bool ContextMenu_ShowFrameStats_CanExecute(ETraceFrameType FrameType, uint32 TimerId);
	bool ContextMenu_ShowFrameStats_IsChecked(ETraceFrameType FrameType, uint32 TimerId);

	void ContextMenu_AutoZoom_Execute();
	bool ContextMenu_AutoZoom_CanExecute();
	bool ContextMenu_AutoZoom_IsChecked();
	void AutoZoom();

	void ContextMenu_ZoomTimingViewOnFrameSelection_Execute();
	bool ContextMenu_ZoomTimingViewOnFrameSelection_CanExecute();
	bool ContextMenu_ZoomTimingViewOnFrameSelection_IsChecked();

	/** Binds our UI commands to delegates. */
	void BindCommands();

	/**
	 * Called when the user scrolls the horizontal scrollbar.
	 * @param ScrollOffset Scroll offset as a fraction between 0 and 1.
	 */
	void HorizontalScrollBar_OnUserScrolled(float ScrollOffset);
	void UpdateHorizontalScrollBar();

	void ZoomHorizontally(const float Delta, const float X);

protected:
	/** The track's viewport. Encapsulates info about position and scale. */
	FFrameTrackViewport Viewport;
	bool bIsViewportDirty;

	/** Cached info for all frame series. */
	TArray<TSharedPtr<FFrameTrackSeries>> AllSeries;

	bool bIsStateDirty;

	bool bIsAutoZoomEnabled;

	float AutoZoomViewportPos;
	float AutoZoomViewportScale;
	float AutoZoomViewportSize;

	bool bZoomTimingViewOnFrameSelection;

	uint64 AnalysisSyncNextTimestamp;

	//////////////////////////////////////////////////

	TSharedPtr<SScrollBar> HorizontalScrollBar;

	//////////////////////////////////////////////////
	// Panning and Zooming behaviors

	/** The current mouse position. */
	FVector2D MousePosition;

	/** Mouse position during the call on mouse button down. */
	FVector2D MousePositionOnButtonDown;
	float ViewportPosXOnButtonDown;

	/** Mouse position during the call on mouse button up. */
	FVector2D MousePositionOnButtonUp;

	bool bIsLMB_Pressed;
	bool bIsRMB_Pressed;

	/** True, if the user is currently interactively scrolling the view (ex.: by holding the left mouse button and dragging). */
	bool bIsScrolling;

	mutable bool bDrawVerticalAxisLabelsOnLeftSide;

	//////////////////////////////////////////////////
	// Selection

	FFrameTrackSampleRef HoveredSample;

	mutable float TooltipOpacity;
	mutable float TooltipSizeX;

	//////////////////////////////////////////////////
	// Misc

	FGeometry ThisGeometry;

	/** Cursor type. */
	ECursorType CursorType;

	STimingView* RegisteredTimingView = nullptr; // For pointer comparison only, do not dereferentiate.
	FDelegateHandle OnTrackVisibilityChangedHandle;
	FDelegateHandle OnTrackAddedHandle;
	FDelegateHandle OnTrackRemovedHandle;

	// Debug stats
	int32 NumUpdatedFrames;
	TFixedCircularBuffer<uint64, 32> UpdateDurationHistory;
	mutable TFixedCircularBuffer<uint64, 32> DrawDurationHistory;
	mutable TFixedCircularBuffer<uint64, 32> OnPaintDurationHistory;
	mutable uint64 LastOnPaintTime;
};
