// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencerTimeSliderController.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "MVVM/Selection/Selection.h"
#include "Fonts/SlateFontInfo.h"
#include "Rendering/DrawElements.h"
#include "Misc/Paths.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Fonts/FontMeasure.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "SequencerCommonHelpers.h"
#include "SequencerSettings.h"
#include "Misc/QualifiedFrameTime.h"
#include "MovieSceneTimeHelpers.h"
#include "CommonFrameRates.h"
#include "Sequencer.h"
#include "Modules/ModuleManager.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "Compilation/MovieSceneCompiledDataManager.h"
#include "Evaluation/MovieSceneSequenceHierarchy.h"

#include "Misc/NotifyHook.h"
#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "ISinglePropertyView.h"
#include "IStructureDetailsView.h"
#include "FrameNumberDetailsCustomization.h"

#define LOCTEXT_NAMESPACE "TimeSlider"

namespace ScrubConstants
{
	/** The minimum amount of pixels between each major ticks on the widget */
	const int32 MinPixelsPerDisplayTick = 12;

	/**The smallest number of units between between major tick marks */
	const float MinDisplayTickSpacing = 0.001f;

	/**The fraction of the current view range to scroll per unit delta  */
	const float ScrollPanFraction = 0.1f;

	/** Marked frame label box margin */
	const int32 MarkLabelBoxMargin = 2;

	/** Marked frame label box margin on the opposite side of the marker time */
	const int32 MarkLabelBoxWideMargin = 4;
}

FSequencerTimeSliderController::FSequencerTimeSliderController( const FTimeSliderArgs& InArgs, TWeakPtr<FSequencer> InWeakSequencer )
	: WeakSequencer(InWeakSequencer)
	, TimeSliderArgs( InArgs )
	, DistanceDragged( 0.0f )
	, MouseDragType( DRAG_NONE )
	, bMouseDownInRegion(false)
	, bPanning( false )
	, HoverMarkIndex( INDEX_NONE )
{
	ScrubFillBrush              = FAppStyle::GetBrush( TEXT( "Sequencer.Timeline.ScrubFill" ) );
	FrameBlockScrubHandleUpBrush   = FAppStyle::GetBrush( TEXT( "Sequencer.Timeline.FrameBlockScrubHandleUp" ) ); 
	FrameBlockScrubHandleDownBrush = FAppStyle::GetBrush( TEXT( "Sequencer.Timeline.FrameBlockScrubHandleDown" ) );
	VanillaScrubHandleUpBrush      = FAppStyle::GetBrush( TEXT( "Sequencer.Timeline.VanillaScrubHandleUp" ) ); 
	VanillaScrubHandleDownBrush    = FAppStyle::GetBrush( TEXT( "Sequencer.Timeline.VanillaScrubHandleDown" ) );

	FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService().ToSharedPtr();
	SmallLayoutFont = FCoreStyle::GetDefaultFontStyle("Regular", 10);
	SmallBoldLayoutFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);

	ContextMenuSuppression = 0;

	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (Sequencer.IsValid())
	{
		Sequencer->OnGlobalTimeChanged().AddRaw(this, &FSequencerTimeSliderController::SetIsEvaluating);
	}

}

FSequencerTimeSliderController::~FSequencerTimeSliderController()
{
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (Sequencer.IsValid())
	{
		Sequencer->OnGlobalTimeChanged().RemoveAll(this);
	}
}

FFrameTime FSequencerTimeSliderController::ComputeScrubTimeFromMouse(const FGeometry& Geometry, const FPointerEvent& MouseEvent, FScrubRangeToScreen RangeToScreen) const
{
	FVector2D           ScreenSpacePosition = MouseEvent.GetScreenSpacePosition();
	FVector2D           CursorPos     = Geometry.AbsoluteToLocal( ScreenSpacePosition );
	double              MouseSeconds  = RangeToScreen.LocalXToInput( CursorPos.X );
	FFrameTime          ScrubTime     = MouseSeconds * GetTickResolution();

	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return ScrubTime;
	}
	
	// Clamp first, snap to frame last
	if (Sequencer->GetSequencerSettings()->ShouldKeepCursorInPlayRangeWhileScrubbing())
	{
		TOptional<TRange<FFrameNumber>> RangeValue;
		RangeValue = TimeSliderArgs.SubSequenceRange.Get(RangeValue);

		if (RangeValue.IsSet())
		{
			ScrubTime = UE::MovieScene::ClampToDiscreteRange(ScrubTime, RangeValue.GetValue());
		}
		else
		{
			ScrubTime = UE::MovieScene::ClampToDiscreteRange(ScrubTime, TimeSliderArgs.PlaybackRange.Get());
		}
	}

	if ( Sequencer->GetSequencerSettings()->GetIsSnapEnabled() || MouseEvent.IsShiftDown() )
	{
		if (Sequencer->GetSequencerSettings()->GetSnapPlayTimeToInterval())
		{
			// Set the style of the scrub handle
			if (Sequencer->GetScrubStyle() == ESequencerScrubberStyle::FrameBlock)
			{
				// Floor to the display frame
				ScrubTime = ConvertFrameTime(ConvertFrameTime(ScrubTime, GetTickResolution(), GetDisplayRate()).FloorToFrame(), GetDisplayRate(), GetTickResolution());
			}
			else
			{
				// Snap (round) to display rate
				ScrubTime = FFrameRate::Snap(ScrubTime, GetTickResolution(), GetDisplayRate());
			}
		}

		// SnapTimeToNearestKey will return ScrubTime unmodified if there is no key within range.
		ScrubTime = SnapTimeToNearestKey(MouseEvent, RangeToScreen, CursorPos.X, ScrubTime);
	}

	return ScrubTime;
}

FFrameTime FSequencerTimeSliderController::ComputeFrameTimeFromMouse(const FGeometry& Geometry, FVector2D ScreenSpacePosition, FScrubRangeToScreen RangeToScreen, bool CheckSnapping) const
{
	FVector2D CursorPos  = Geometry.AbsoluteToLocal( ScreenSpacePosition );
	double    MouseValue = RangeToScreen.LocalXToInput( CursorPos.X );

	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return MouseValue * GetTickResolution();
	}

	if (CheckSnapping && Sequencer->GetSequencerSettings()->GetIsSnapEnabled())
	{
		FFrameNumber        SnappedFrameNumber = (MouseValue * GetDisplayRate()).FloorToFrame();
		FQualifiedFrameTime RoundedPlayFrame   = FQualifiedFrameTime(SnappedFrameNumber, GetDisplayRate());
		return RoundedPlayFrame.ConvertTo(GetTickResolution());
	}
	else
	{
		return MouseValue * GetTickResolution();
	}
}

FSequencerTimeSliderController::FScrubberMetrics FSequencerTimeSliderController::GetHitTestScrubPixelMetrics(const FScrubRangeToScreen& RangeToScreen) const
{
	static const float DragToleranceSlateUnits = 2.f, MouseTolerance = 2.f;
	return GetScrubPixelMetrics(FQualifiedFrameTime(TimeSliderArgs.ScrubPosition.Get(), GetTickResolution()), RangeToScreen, DragToleranceSlateUnits + MouseTolerance);
}

FSequencerTimeSliderController::FScrubberMetrics FSequencerTimeSliderController::GetScrubPixelMetrics(const FQualifiedFrameTime& ScrubTime, const FScrubRangeToScreen& RangeToScreen, float DilationPixels) const
{
	FFrameRate DisplayRate = GetDisplayRate();

	FScrubberMetrics Metrics;

	static float MinScrubSize = 14.f;

	const FFrameNumber Frame = ScrubTime.ConvertTo(DisplayRate).FloorToFrame();

	float FrameStartPixel = RangeToScreen.InputToLocalX(  Frame    / DisplayRate );
	float FrameEndPixel   = RangeToScreen.InputToLocalX( (Frame+1) / DisplayRate ) - 1;

	{
		float RoundedStartPixel = FMath::RoundToInt(FrameStartPixel);
		FrameEndPixel -= (FrameStartPixel - RoundedStartPixel);

		FrameStartPixel = RoundedStartPixel;
		FrameEndPixel   = FMath::Max(FrameEndPixel, FrameStartPixel + 1);
	}

	// Store off the pixel width of the frame
	Metrics.FrameExtentsPx = TRange<float>(FrameStartPixel - DilationPixels, FrameEndPixel + DilationPixels);

	// Set the style of the scrub handle
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	Metrics.Style = Sequencer.IsValid() ? Sequencer->GetScrubStyle() : ESequencerScrubberStyle::Vanilla;

	// Always draw the extents on the section area for frame block styles
	Metrics.bDrawExtents = Metrics.Style == ESequencerScrubberStyle::FrameBlock;

	// If it's vanilla style or too small to show the frame width, set that up
	if (Metrics.Style == ESequencerScrubberStyle::Vanilla || FrameEndPixel - FrameStartPixel < MinScrubSize)
	{
		Metrics.Style = ESequencerScrubberStyle::Vanilla;

		float ScrubPixel = RangeToScreen.InputToLocalX(ScrubTime.AsSeconds());
		Metrics.HandleRangePx = TRange<float>(ScrubPixel - MinScrubSize*.5f - DilationPixels, ScrubPixel + MinScrubSize*.5f + DilationPixels);
	}
	else
	{
		Metrics.HandleRangePx = Metrics.FrameExtentsPx;
	}

	return Metrics;
}

struct FSequencerTimeSliderController::FDrawTickArgs
{
	/** Geometry of the area */
	FGeometry AllottedGeometry;
	/** Culling rect of the area */
	FSlateRect CullingRect;
	/** Color of each tick */
	FLinearColor TickColor;
	/** Offset in Y where to start the tick */
	float TickOffset;
	/** Height in of major ticks */
	float MajorTickHeight;
	/** Start layer for elements */
	int32 StartLayer;
	/** Draw effects to apply */
	ESlateDrawEffect DrawEffects;
	/** Whether or not to only draw major ticks */
	bool bOnlyDrawMajorTicks;
	/** Whether or not to mirror labels */
	bool bMirrorLabels;
	
};

void FSequencerTimeSliderController::DrawTicks( FSlateWindowElementList& OutDrawElements, const TRange<double>& ViewRange, const FScrubRangeToScreen& RangeToScreen, FDrawTickArgs& InArgs ) const
{
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return;
	}

	FFrameRate     TickResolution  = GetTickResolution();
	FFrameRate     DisplayRate     = GetDisplayRate();
	FPaintGeometry PaintGeometry   = InArgs.AllottedGeometry.ToPaintGeometry();
	FSlateFontInfo TickFrameFont = FCoreStyle::GetDefaultFontStyle("Regular", 8);

	double MajorGridStep  = 0.0;
	int32  MinorDivisions = 0;
	if (!Sequencer->GetGridMetrics(InArgs.AllottedGeometry.Size.X, ViewRange.GetLowerBoundValue(), ViewRange.GetUpperBoundValue(), MajorGridStep, MinorDivisions))
	{
		return;
	}

	if (InArgs.bOnlyDrawMajorTicks)
	{
		MinorDivisions = 0;
	}

	TArray<FVector2D> LinePoints;
	LinePoints.SetNumUninitialized(2);

	const bool bAntiAliasLines = false;

	const double FirstMajorLine = FMath::FloorToDouble(ViewRange.GetLowerBoundValue() / MajorGridStep) * MajorGridStep;
	const double LastMajorLine  = FMath::CeilToDouble(ViewRange.GetUpperBoundValue() / MajorGridStep) * MajorGridStep;

	const float  FlooredScrubPx  = RangeToScreen.InputToLocalX(ConvertFrameTime(TimeSliderArgs.ScrubPosition.Get(), TickResolution, GetDisplayRate()).FloorToFrame() / DisplayRate);

	for (double CurrentMajorLine = FirstMajorLine; CurrentMajorLine < LastMajorLine; CurrentMajorLine += MajorGridStep)
	{
		float MajorLinePx = RangeToScreen.InputToLocalX( CurrentMajorLine );

		LinePoints[0] = FVector2D( MajorLinePx, InArgs.TickOffset );
		LinePoints[1] = FVector2D( MajorLinePx, InArgs.TickOffset + InArgs.MajorTickHeight );

		// Draw each tick mark
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			InArgs.StartLayer,
			PaintGeometry,
			LinePoints,
			InArgs.DrawEffects,
			InArgs.TickColor,
			bAntiAliasLines
			);

		if (!InArgs.bOnlyDrawMajorTicks && !FMath::IsNearlyEqual(MajorLinePx, FlooredScrubPx, 3.f))
		{
			FString FrameString = TimeSliderArgs.NumericTypeInterface->ToString((CurrentMajorLine * TickResolution).RoundToFrame().Value);

			// Space the text between the tick mark but slightly above
			FVector2D TextOffset( MajorLinePx + 5.f, InArgs.bMirrorLabels ? 1.f : FMath::Abs( InArgs.AllottedGeometry.Size.Y - (InArgs.MajorTickHeight+3.f) ) );
			FSlateDrawElement::MakeText(
				OutDrawElements,
				InArgs.StartLayer+1, 
				InArgs.AllottedGeometry.ToPaintGeometry( InArgs.AllottedGeometry.Size, FSlateLayoutTransform(TextOffset) ), 
				FrameString, 
				TickFrameFont,
				InArgs.DrawEffects,
				InArgs.TickColor*0.65f 
			);
		}

		for (int32 Step = 1; Step < MinorDivisions; ++Step)
		{
			// Compute the size of each tick mark.  If we are half way between to visible values display a slightly larger tick mark
			const float MinorTickHeight = ( (MinorDivisions % 2 == 0) && (Step % (MinorDivisions/2)) == 0 ) ? 6.0f : 2.0f;
			const float MinorLinePx = RangeToScreen.InputToLocalX( CurrentMajorLine + Step*MajorGridStep/MinorDivisions );

			LinePoints[0] = FVector2D(MinorLinePx, InArgs.bMirrorLabels ? 0.0f : FMath::Abs( InArgs.AllottedGeometry.Size.Y - MinorTickHeight ) );
			LinePoints[1] = FVector2D(MinorLinePx, LinePoints[0].Y + MinorTickHeight);

			// Draw each sub mark
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				InArgs.StartLayer,
				PaintGeometry,
				LinePoints,
				InArgs.DrawEffects,
				InArgs.TickColor,
				bAntiAliasLines
			);
		}
	}
}

int32 FSequencerTimeSliderController::DrawMarkedFrames( const FGeometry& AllottedGeometry, const FScrubRangeToScreen& RangeToScreen, FSlateWindowElementList& OutDrawElements, int32 LayerId, const ESlateDrawEffect& DrawEffects, const FWidgetStyle& InWidgetStyle, bool bDrawLabels ) const
{
	using namespace UE::Sequencer;

	const TArray<FMovieSceneMarkedFrame> & MarkedFrames = TimeSliderArgs.MarkedFrames.Get();
	const TArray<FMovieSceneMarkedFrame> & GlobalMarkedFrames = TimeSliderArgs.GlobalMarkedFrames.Get();
	if (MarkedFrames.Num() < 1 && GlobalMarkedFrames.Num() < 1)
	{
		return LayerId;
	}

	const FMarkedFrameSelection& SelectedMarkedFrames = WeakSequencer.Pin()->GetViewModel()->GetSelection()->MarkedFrames;
	//const FLinearColor SelectedColor = FLinearColor::White; //FAppStyle::GetSlateColor("SelectionColor").GetColor(InWidgetStyle);
	const FLinearColor WhiteColorHSV = FLinearColor::White.LinearRGBToHSV();

	auto DrawFrameMarkers = ([=, this](const TArray<FMovieSceneMarkedFrame> & InMarkedFrames, FSlateWindowElementList& DrawElements, bool bIsGlobal)
	{
		for (int32 MarkIndex = 0; MarkIndex < InMarkedFrames.Num(); ++MarkIndex)
		{
			const FMovieSceneMarkedFrame& MarkedFrame(InMarkedFrames[MarkIndex]);
			double Seconds = MarkedFrame.FrameNumber / GetTickResolution();

			const bool bIsHovered = (!bIsGlobal && HoverMarkIndex == MarkIndex);
			const bool bIsSelected = (!bIsGlobal && SelectedMarkedFrames.IsSelected(MarkIndex));

			// Get a selected color that's the marked frame color but at full opacity, full brightness, and a bit desaturated if it's
			// already bright to begin with.
			const FLinearColor MarkedFrameColorHSV = MarkedFrame.Color.CopyWithNewOpacity(1.f).LinearRGBToHSV();
			const FLinearColor SelectedColor = FLinearColor::LerpUsingHSV(
				MarkedFrameColorHSV, 
				FLinearColor(
					MarkedFrameColorHSV.R, 
					MarkedFrameColorHSV.G * (1.f - MarkedFrameColorHSV.B),
					1.f),
				0.5f)
				.HSVToLinearRGB();

			FLinearColor DrawColor = bIsGlobal ? MarkedFrame.Color.Desaturate(0.25f) : (bIsSelected ? SelectedColor : MarkedFrame.Color);
			const float  LinePos = RangeToScreen.InputToLocalX(Seconds);
			TArray<FVector2D> LinePoints;
			LinePoints.AddUninitialized(2);
			LinePoints[0] = FVector2D(LinePos, 0.0f);
			LinePoints[1] = FVector2D(LinePos, FMath::FloorToFloat(AllottedGeometry.Size.Y));

			FSlateDrawElement::MakeLines(
				DrawElements,
				LayerId + 1,
				AllottedGeometry.ToPaintGeometry(),
				LinePoints,
				DrawEffects,
				DrawColor,
				false,
				(bIsHovered || bIsSelected) ? 2.f : 1.f
			);

			FString LabelString = MarkedFrame.Label;
			if (bDrawLabels && !LabelString.IsEmpty())
			{
				// Draw the label next to the marked frame line
				bool bDrawLeft;
				FVector2D TextPosition, TextSize;
				GetMarkLabelGeometry(AllottedGeometry, RangeToScreen, MarkedFrame, TextPosition, TextSize, bDrawLeft);

				const FSlateBrush* LabelBrush = bDrawLeft ?
					FAppStyle::GetBrush("Sequencer.MarkedFrame.LabelLeft") :
					FAppStyle::GetBrush("Sequencer.MarkedFrame.LabelRight");

				if (bIsHovered || bIsSelected)
				{
					FSlateDrawElement::MakeBox(
						DrawElements,
						LayerId + 1,
						AllottedGeometry.ToPaintGeometry(
							TextSize + ScrubConstants::MarkLabelBoxMargin + ScrubConstants::MarkLabelBoxWideMargin,
							FSlateLayoutTransform(FVector2D(LinePos, 0.f))),
						LabelBrush,
						DrawEffects,
						DrawColor.CopyWithNewOpacity(0.3f)
					);
				}

				FSlateDrawElement::MakeText(
					DrawElements,
					LayerId + 1,
					AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(TextPosition)),
					LabelString,
					(bIsHovered || bIsSelected) ? SmallBoldLayoutFont : SmallLayoutFont,
					DrawEffects,
					DrawColor
				);
			}
		}
	});
	
	DrawFrameMarkers(GlobalMarkedFrames, OutDrawElements, true);
	DrawFrameMarkers(MarkedFrames, OutDrawElements, false);
	
	return LayerId + 1;
}

int32 FSequencerTimeSliderController::DrawVerticalFrames(const FGeometry& AllottedGeometry, const FScrubRangeToScreen& RangeToScreen, FSlateWindowElementList& OutDrawElements, int32 LayerId, const ESlateDrawEffect& DrawEffects) const
{
	TSet<FFrameNumber> VerticalFrames = TimeSliderArgs.VerticalFrames.Get();
	if (VerticalFrames.Num() < 1)
	{
		return LayerId;
	}

	for (FFrameNumber TickFrame : VerticalFrames)
	{
		double Seconds = TickFrame / GetTickResolution();

		const float  LinePos = RangeToScreen.InputToLocalX(Seconds);
		TArray<FVector2D> LinePoints;
		LinePoints.AddUninitialized(2);
		LinePoints[0] = FVector2D(LinePos, 0.0f);
		LinePoints[1] = FVector2D(LinePos, FMath::FloorToFloat(AllottedGeometry.Size.Y));

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			DrawEffects,
			FLinearColor(0.7f, 0.7f, 0.f, 0.4f),
			false
		);
	}

	return LayerId + 1;
}

int32 FSequencerTimeSliderController::OnPaintTimeSlider( bool bMirrorLabels, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return LayerId;
	}

	const bool bEnabled = bParentEnabled;
	const ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	TRange<double> LocalViewRange      = GetViewRange();
	const float    LocalViewRangeMin   = LocalViewRange.GetLowerBoundValue();
	const float    LocalViewRangeMax   = LocalViewRange.GetUpperBoundValue();
	const float    LocalSequenceLength = LocalViewRangeMax-LocalViewRangeMin;
	
	FVector2D Scale = FVector2D(1.0f,1.0f);
	if ( LocalSequenceLength > 0)
	{
		FScrubRangeToScreen RangeToScreen( LocalViewRange, AllottedGeometry.Size );

		// draw tick marks
		const float MajorTickHeight = 9.0f;
	
		FDrawTickArgs Args;
		{
			Args.AllottedGeometry = AllottedGeometry;
			Args.bMirrorLabels = bMirrorLabels;
			Args.bOnlyDrawMajorTicks = false;
			Args.TickColor = FLinearColor::White;
			Args.CullingRect = MyCullingRect;
			Args.DrawEffects = DrawEffects;
			Args.StartLayer = LayerId;
			Args.TickOffset = bMirrorLabels ? 0.0f : FMath::Abs( AllottedGeometry.Size.Y - MajorTickHeight );
			Args.MajorTickHeight = MajorTickHeight;
		}

		DrawTicks( OutDrawElements, LocalViewRange, RangeToScreen, Args );

		// draw playback & selection range
		FPaintPlaybackRangeArgs PlaybackRangeArgs(
			bMirrorLabels ? FAppStyle::GetBrush("Sequencer.Timeline.PlayRange_Bottom_L") : FAppStyle::GetBrush("Sequencer.Timeline.PlayRange_Top_L"),
			bMirrorLabels ? FAppStyle::GetBrush("Sequencer.Timeline.PlayRange_Bottom_R") : FAppStyle::GetBrush("Sequencer.Timeline.PlayRange_Top_R"),
			6.f
		);

		LayerId = DrawPlaybackRange(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, RangeToScreen, PlaybackRangeArgs);
		LayerId = DrawSubSequenceRange(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, RangeToScreen, PlaybackRangeArgs);

		PlaybackRangeArgs.SolidFillOpacity = 0.05f;
		LayerId = DrawSelectionRange(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, RangeToScreen, PlaybackRangeArgs);

		// Draw the scrub handle
		FQualifiedFrameTime ScrubPosition = FQualifiedFrameTime(TimeSliderArgs.ScrubPosition.Get(), GetTickResolution());
		FScrubberMetrics    ScrubMetrics  = GetScrubPixelMetrics(ScrubPosition, RangeToScreen);
		const float         HandleStart   = ScrubMetrics.HandleRangePx.GetLowerBoundValue();
		const float         HandleEnd     = ScrubMetrics.HandleRangePx.GetUpperBoundValue();

		const int32 ArrowLayer = LayerId + 2;
		FPaintGeometry MyGeometry =	AllottedGeometry.ToPaintGeometry( FVector2f( HandleEnd - HandleStart, AllottedGeometry.Size.Y ), FSlateLayoutTransform(FVector2f( HandleStart, 0.f )) );
		FLinearColor ScrubColor = InWidgetStyle.GetColorAndOpacityTint();
		if(bIsEvaluating)
		{
			// @todo Sequencer this color should be specified in the style
			ScrubColor.A = ScrubColor.A * 0.75f;
			ScrubColor.B *= 0.1f;
			ScrubColor.G *= 0.2f;
		}
		else
		{
			ScrubColor.A = ScrubColor.A * 0.75f;
			ScrubColor.R = 0.7f;
			ScrubColor.B = 0.1f;
			ScrubColor.G = 0.7f;
		}
		const FSlateBrush* Brush = ScrubMetrics.Style == ESequencerScrubberStyle::Vanilla
			? ( bMirrorLabels ? VanillaScrubHandleUpBrush    : VanillaScrubHandleDownBrush )
			: ( bMirrorLabels ? FrameBlockScrubHandleUpBrush : FrameBlockScrubHandleDownBrush );

		
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			ArrowLayer,
			MyGeometry,
			Brush,
			DrawEffects,
			ScrubColor
		);
		
		LayerId = DrawMarkedFrames(AllottedGeometry, RangeToScreen, OutDrawElements, LayerId, DrawEffects, InWidgetStyle, true);

		{
			// Draw the current time next to the scrub handle
			FString FrameString;
			FLinearColor TextColor = Args.TickColor;
			if (TimeSliderArgs.ScrubPositionText.IsSet())
			{
				FrameString = TimeSliderArgs.ScrubPositionText.Get();
			}
			else
			{
				FrameString = TimeSliderArgs.NumericTypeInterface->ToString(TimeSliderArgs.ScrubPosition.Get().GetFrame().Value);
			}

			if (TimeSliderArgs.ScrubPositionParent.Get() != MovieSceneSequenceID::Invalid)
			{
				TextColor = FLinearColor::Yellow;
			}

			FVector2D TextSize = FontMeasureService->Measure(FrameString, SmallLayoutFont);

			// Flip the text position if getting near the end of the view range
			static const float TextOffsetPx  = 2.f;
			bool  bDrawLeft    = (AllottedGeometry.Size.X - HandleEnd) < (TextSize.X + 14.f) - TextOffsetPx;
			float TextPosition = bDrawLeft ? HandleStart - TextSize.X - TextOffsetPx : HandleEnd + TextOffsetPx;

			FVector2D TextOffset( TextPosition, Args.bMirrorLabels ? Args.AllottedGeometry.Size.Y - TextSize.Y : 0.f );

			FSlateDrawElement::MakeText(
				OutDrawElements,
				Args.StartLayer+1, 
				Args.AllottedGeometry.ToPaintGeometry( TextSize, FSlateLayoutTransform(TextOffset) ), 
				FrameString, 
				SmallLayoutFont,
				Args.DrawEffects,
				TextColor 
			);
		}
		
		if (MouseDragType == DRAG_SETTING_RANGE && MouseDownPosition[0].IsSet() && MouseDownPosition[1].IsSet())
		{
			FFrameRate Resolution = GetTickResolution();
			FFrameTime MouseDownTime[2];

			FScrubRangeToScreen MouseDownRange(GetViewRange(), MouseDownGeometry.Size);
			MouseDownTime[0] = ComputeFrameTimeFromMouse(MouseDownGeometry, MouseDownPosition[0].GetValue(), MouseDownRange);
			MouseDownTime[1] = ComputeFrameTimeFromMouse(MouseDownGeometry, MouseDownPosition[1].GetValue(), MouseDownRange);

			float      MouseStartPosX = RangeToScreen.InputToLocalX(MouseDownTime[0] / Resolution);
			float      MouseEndPosX   = RangeToScreen.InputToLocalX(MouseDownTime[1] / Resolution);

			float RangePosX = MouseStartPosX < MouseEndPosX ? MouseStartPosX : MouseEndPosX;
			float RangeSizeX = FMath::Abs(MouseStartPosX - MouseEndPosX);

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId+1,
				AllottedGeometry.ToPaintGeometry( FVector2f(RangeSizeX, AllottedGeometry.Size.Y), FSlateLayoutTransform(FVector2f(RangePosX, 0.f)) ),
				bMirrorLabels ? VanillaScrubHandleDownBrush : VanillaScrubHandleUpBrush,
				DrawEffects,
				MouseStartPosX < MouseEndPosX ? FLinearColor(0.5f, 0.5f, 0.5f) : FLinearColor(0.25f, 0.3f, 0.3f)
			);
		}

		return ArrowLayer;
	}

	return LayerId;
}


int32 FSequencerTimeSliderController::DrawSelectionRange(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FScrubRangeToScreen& RangeToScreen, const FPaintPlaybackRangeArgs& Args) const
{
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return LayerId;
	}

	TRange<double> SelectionRange = TimeSliderArgs.SelectionRange.Get() / GetTickResolution();

	if (!SelectionRange.IsEmpty())
	{
		const float SelectionRangeL = RangeToScreen.InputToLocalX(SelectionRange.GetLowerBoundValue());
		const float SelectionRangeR = RangeToScreen.InputToLocalX(SelectionRange.GetUpperBoundValue()) - 1;
		const auto DrawColor = FAppStyle::GetSlateColor("SelectionColor").GetColor(FWidgetStyle());

		if (Args.SolidFillOpacity > 0.f)
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 1,
				AllottedGeometry.ToPaintGeometry(FVector2f(SelectionRangeR - SelectionRangeL, AllottedGeometry.Size.Y), FSlateLayoutTransform(FVector2f(SelectionRangeL, 0.f))),
				FAppStyle::GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				DrawColor.CopyWithNewOpacity(Args.SolidFillOpacity)
			);
		}

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(FVector2f(Args.BrushWidth, AllottedGeometry.Size.Y), FSlateLayoutTransform(FVector2f(SelectionRangeL, 0.f))),
			Args.StartBrush,
			ESlateDrawEffect::None,
			DrawColor
		);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 1,
			AllottedGeometry.ToPaintGeometry(FVector2f(Args.BrushWidth, AllottedGeometry.Size.Y), FSlateLayoutTransform(FVector2f(SelectionRangeR - Args.BrushWidth, 0.f))),
			Args.EndBrush,
			ESlateDrawEffect::None,
			DrawColor
		);
	}

	return LayerId + 1;
}


int32 FSequencerTimeSliderController::DrawPlaybackRange(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FScrubRangeToScreen& RangeToScreen, const FPaintPlaybackRangeArgs& Args) const
{
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return LayerId;
	}

	if (!TimeSliderArgs.PlaybackRange.IsSet())
	{
		return LayerId;
	}

	const uint8 OpacityBlend = TimeSliderArgs.SubSequenceRange.Get().IsSet() ? 128 : 255;

	TRange<FFrameNumber> PlaybackRange = TimeSliderArgs.PlaybackRange.Get();
	FFrameRate TickResolution = GetTickResolution();
	const float PlaybackRangeL = RangeToScreen.InputToLocalX(PlaybackRange.GetLowerBoundValue() / TickResolution);
	const float PlaybackRangeR = RangeToScreen.InputToLocalX(PlaybackRange.GetUpperBoundValue() / TickResolution) - 1;

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2f(Args.BrushWidth, AllottedGeometry.Size.Y), FSlateLayoutTransform(FVector2f(PlaybackRangeL, 0.f))),
		Args.StartBrush,
		ESlateDrawEffect::None,
		FColor(32, 128, 32, OpacityBlend)	// 120, 75, 50 (HSV)
	);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2f(Args.BrushWidth, AllottedGeometry.Size.Y), FSlateLayoutTransform(FVector2f(PlaybackRangeR - Args.BrushWidth, 0.f))),
		Args.EndBrush,
		ESlateDrawEffect::None,
		FColor(128, 32, 32, OpacityBlend)	// 0, 75, 50 (HSV)
	);

	// Black tint for excluded regions
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2f(PlaybackRangeL, AllottedGeometry.Size.Y), FSlateLayoutTransform()),
		FAppStyle::GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		FLinearColor::Black.CopyWithNewOpacity(0.3f * OpacityBlend / 255.f)
	);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2f(AllottedGeometry.Size.X - PlaybackRangeR, AllottedGeometry.Size.Y), FSlateLayoutTransform(FVector2f(PlaybackRangeR, 0.f))),
		FAppStyle::GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		FLinearColor::Black.CopyWithNewOpacity(0.3f * OpacityBlend / 255.f)
	);

	return LayerId + 1;
}

int32 FSequencerTimeSliderController::DrawSubSequenceRange(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FScrubRangeToScreen& RangeToScreen, const FPaintPlaybackRangeArgs& Args) const
{
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return LayerId;
	}

	TOptional<TRange<FFrameNumber>> RangeValue;
	RangeValue = TimeSliderArgs.SubSequenceRange.Get(RangeValue);

	if (!RangeValue.IsSet() || RangeValue->IsEmpty())
	{
		return LayerId;
	}

	const FFrameRate   Resolution = GetTickResolution();
	const FFrameNumber LowerFrame = RangeValue.GetValue().GetLowerBoundValue();
	const FFrameNumber UpperFrame = RangeValue.GetValue().GetUpperBoundValue();

	const float SubSequenceRangeL = RangeToScreen.InputToLocalX(LowerFrame / Resolution) - 1;
	const float SubSequenceRangeR = RangeToScreen.InputToLocalX(UpperFrame / Resolution) + 1;

	static const FSlateBrush* LineBrushL(FAppStyle::GetBrush("Sequencer.Timeline.PlayRange_L"));
	static const FSlateBrush* LineBrushR(FAppStyle::GetBrush("Sequencer.Timeline.PlayRange_R"));

	FColor GreenTint(32, 128, 32);	// 120, 75, 50 (HSV)
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2f(Args.BrushWidth, AllottedGeometry.Size.Y), FSlateLayoutTransform(FVector2f(SubSequenceRangeL, 0.f))),
		LineBrushL,
		ESlateDrawEffect::None,
		GreenTint
	);

	FColor RedTint(128, 32, 32);	// 0, 75, 50 (HSV)
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2f(Args.BrushWidth, AllottedGeometry.Size.Y), FSlateLayoutTransform(FVector2f(SubSequenceRangeR - Args.BrushWidth, 0.f))),
		LineBrushR,
		ESlateDrawEffect::None,
		RedTint
	);

	// Black tint for excluded regions
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2f(SubSequenceRangeL, AllottedGeometry.Size.Y), FSlateLayoutTransform()),
		FAppStyle::GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		FLinearColor::Black.CopyWithNewOpacity(0.3f)
	);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2f(AllottedGeometry.Size.X - SubSequenceRangeR, AllottedGeometry.Size.Y), FSlateLayoutTransform(FVector2f(SubSequenceRangeR, 0.f))),
		FAppStyle::GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		FLinearColor::Black.CopyWithNewOpacity(0.3f)
	);

	// Hash applied to the left and right of the sequence bounds
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2f(16.f, AllottedGeometry.Size.Y), FSlateLayoutTransform(FVector2f(SubSequenceRangeL - 16.f, 0.f))),
		FAppStyle::GetBrush("Sequencer.Timeline.SubSequenceRangeHashL"),
		ESlateDrawEffect::None,
		GreenTint
	);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId+1,
		AllottedGeometry.ToPaintGeometry(FVector2f(16.f, AllottedGeometry.Size.Y), FSlateLayoutTransform(FVector2f(SubSequenceRangeR, 0.f))),
		FAppStyle::GetBrush("Sequencer.Timeline.SubSequenceRangeHashR"),
		ESlateDrawEffect::None,
		RedTint
	);

	return LayerId + 1;
}

FReply FSequencerTimeSliderController::OnMouseButtonDown( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	MouseDragType = DRAG_NONE;
	DistanceDragged = 0;
	MouseDownPlaybackRange = TimeSliderArgs.PlaybackRange.Get();
	MouseDownSelectionRange = TimeSliderArgs.SelectionRange.Get();
	MouseDownPosition[0] = MouseDownPosition[1] = MouseEvent.GetScreenSpacePosition();
	MouseDownGeometry = MyGeometry;
	bMouseDownInRegion = false;

	FVector2D CursorPos = MouseEvent.GetScreenSpacePosition();
	FVector2D LocalPos = MouseDownGeometry.AbsoluteToLocal(CursorPos);
	if (LocalPos.Y >= 0 && LocalPos.Y < MouseDownGeometry.GetLocalSize().Y)
	{
		bMouseDownInRegion = true;
	}

	return FReply::Unhandled();
}

FReply FSequencerTimeSliderController::OnMouseButtonUp( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	bool bHandleLeftMouseButton  = MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton  && WidgetOwner.HasMouseCapture();
	bool bHandleRightMouseButton = MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && WidgetOwner.HasMouseCapture() && TimeSliderArgs.AllowZoom ;
	bool bHandleMiddleMouseButton = MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton && WidgetOwner.HasMouseCapture();
	FScrubRangeToScreen RangeToScreen = FScrubRangeToScreen(GetViewRange(), MyGeometry.Size);
	FFrameTime          MouseTime     = ComputeFrameTimeFromMouse(MyGeometry, MouseEvent.GetScreenSpacePosition(), RangeToScreen);

	if ( bHandleRightMouseButton )
	{
		if (!bPanning)
		{
			// Open a context menu if allowed
			if (ContextMenuSuppression == 0 && TimeSliderArgs.PlaybackRange.IsSet())
			{
				TSharedRef<SWidget> MenuContent = OpenSetPlaybackRangeMenu(MyGeometry, MouseEvent);
				FSlateApplication::Get().PushMenu(
					WidgetOwner.AsShared(),
					MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath(),
					MenuContent,
					MouseEvent.GetScreenSpacePosition(),
					FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu )
					);

				return FReply::Handled().SetUserFocus(MenuContent, EFocusCause::SetDirectly).ReleaseMouseCapture();
			}

			// return unhandled in case our parent wants to use our right mouse button to open a context menu
			if (DistanceDragged == 0.f)
			{
				return FReply::Unhandled().ReleaseMouseCapture();
			}
		}
		
		bPanning = false;
		bMouseDownInRegion = false;
		MouseDownPosition[0].Reset();
		MouseDownPosition[1].Reset();
		
		return FReply::Handled().ReleaseMouseCapture();
	}
	else if ( bHandleLeftMouseButton || bHandleMiddleMouseButton )
	{
		if (MouseDragType == DRAG_PLAYBACK_START)
		{
			TimeSliderArgs.OnPlaybackRangeEndDrag.ExecuteIfBound();
		}
		else if (MouseDragType == DRAG_PLAYBACK_END)
		{
			TimeSliderArgs.OnPlaybackRangeEndDrag.ExecuteIfBound();
		}
		else if (MouseDragType == DRAG_SELECTION_START)
		{
			TimeSliderArgs.OnSelectionRangeEndDrag.ExecuteIfBound();
		}
		else if (MouseDragType == DRAG_SELECTION_END)
		{
			TimeSliderArgs.OnSelectionRangeEndDrag.ExecuteIfBound();
		}
		else if (MouseDragType == DRAG_MARK)
		{
			TimeSliderArgs.OnMarkEndDrag.ExecuteIfBound();
		}
		else if (MouseDragType == DRAG_SETTING_RANGE && MouseDownPosition[0].IsSet())
		{
			// Zooming
			FFrameTime MouseDownStart = ComputeFrameTimeFromMouse(MyGeometry, MouseDownPosition[0].GetValue(), RangeToScreen);

			const bool bCanZoomIn  = MouseTime > MouseDownStart;
			const bool bCanZoomOut = ViewRangeStack.Num() > 0;
			if (bCanZoomIn || bCanZoomOut)
			{
				TRange<double> ViewRange = GetViewRange();
				if (!bCanZoomIn)
				{
					ViewRange = ViewRangeStack.Pop();
				}

				if (bCanZoomIn)
				{
					// push the current value onto the stack
					ViewRangeStack.Add(ViewRange);

					ViewRange = TRange<double>(MouseDownStart.FrameNumber / GetTickResolution(), MouseTime.FrameNumber / GetTickResolution());
				}
				
				TimeSliderArgs.OnViewRangeChanged.ExecuteIfBound(ViewRange, EViewRangeInterpolation::Immediate);
				if( !TimeSliderArgs.ViewRange.IsBound() )
				{
					// The output is not bound to a delegate so we'll manage the value ourselves
					TimeSliderArgs.ViewRange.Set(ViewRange);
				}
			}
		}
		else if (bMouseDownInRegion)
		{
			TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();

			if (HoverMarkIndex == INDEX_NONE)
			{
				// Teleport the playhead to the clicked time.
				TimeSliderArgs.OnEndScrubberMovement.ExecuteIfBound();

				FFrameTime ScrubTime = MouseTime;
				FVector2D CursorPos  = MouseEvent.GetScreenSpacePosition();

				if (MouseDragType == DRAG_SCRUBBING_TIME)
				{
					ScrubTime = ComputeScrubTimeFromMouse(MyGeometry, MouseEvent, RangeToScreen);
				}
				else if (Sequencer.IsValid())
				{
					ScrubTime = SnapTimeToNearestKey(MouseEvent, RangeToScreen, CursorPos.X, ScrubTime);

					// We weren't scrubbing, so this is a single click in the time slider. This should
					// also act as a deselection of any selected markers.
					Sequencer->GetViewModel()->GetSelection()->MarkedFrames.Empty();
				}

				// If middle mouse button down we don't evaluate on the time change
				CommitScrubPosition( ScrubTime, /*bIsScrubbing=*/false , /*bEvaluate*/ !bHandleMiddleMouseButton);
			}
			else if (Sequencer.IsValid())
			{
				// Select the clicked marker, and optionally teleport the playhead.
				HandleMarkSelection(HoverMarkIndex);

				FModifierKeysState ModifierKeys = FSlateApplication::Get().GetModifierKeys();
				const bool bSnapPlayTimeToMark = ModifierKeys.AreModifersDown(EModifierKey::Shift);
				if (bSnapPlayTimeToMark || Sequencer->GetSequencerSettings()->GetSnapPlayTimeToPressedKey())
				{
					const TArray<FMovieSceneMarkedFrame>& MarkedFrames = TimeSliderArgs.MarkedFrames.Get();
					const FFrameNumber MarkTime = MarkedFrames[HoverMarkIndex].FrameNumber;

					// If middle mouse button down we don't evaluate on the time change
					CommitScrubPosition(MarkTime, /*bIsScrubbing=*/ false , /*bEvaluate*/ !bHandleMiddleMouseButton);
				}
			}
		}

		MouseDragType = DRAG_NONE;
		DistanceDragged = 0.f;
		bMouseDownInRegion = false;
		MouseDownPosition[0].Reset();
		MouseDownPosition[1].Reset();

		return FReply::Handled().ReleaseMouseCapture();
	}

	bMouseDownInRegion = false;
	MouseDownPosition[0].Reset();
	MouseDownPosition[1].Reset();
	return FReply::Unhandled();
}

FReply FSequencerTimeSliderController::OnMouseButtonDoubleClick(TSharedRef<const SWidget> WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (HoverMarkIndex != INDEX_NONE)
	{
		// Teleport the playhead to the double-clicked marker.
		bool bHandleMiddleMouseButton = MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton && WidgetOwner->HasMouseCapture();
		const TArray<FMovieSceneMarkedFrame>& MarkedFrames = TimeSliderArgs.MarkedFrames.Get();
		const FFrameNumber MarkTime = MarkedFrames[HoverMarkIndex].FrameNumber;
		CommitScrubPosition(MarkTime, /*bIsScrubbing=*/ false , /*bEvaluate*/ !bHandleMiddleMouseButton);
	}
	return FReply::Unhandled();
}

FReply FSequencerTimeSliderController::OnMouseMove( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	return OnMouseMoveImpl(WidgetOwner, MyGeometry, MouseEvent, false);
}

FReply FSequencerTimeSliderController::OnTimeSliderMouseMove(SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return OnMouseMoveImpl(WidgetOwner, MyGeometry, MouseEvent, true);
}

FReply FSequencerTimeSliderController::OnMouseMoveImpl( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bFromTimeSlider )
{
	using namespace UE::Sequencer;

	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return FReply::Unhandled();
	}

	bool bHandleLeftMouseButton  = MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton  );
	bool bHandleRightMouseButton = MouseEvent.IsMouseButtonDown( EKeys::RightMouseButton ) && TimeSliderArgs.AllowZoom;
	bool bHandleMiddleMouseButton = MouseEvent.IsMouseButtonDown(EKeys::MiddleMouseButton);

	const bool bLockedMarkedFrames = TimeSliderArgs.AreMarkedFramesLocked.Get();

	HoverMarkIndex = INDEX_NONE;
	int32 DragMarkIndex = INDEX_NONE;

	if (bHandleRightMouseButton)
	{
		if (!bPanning)
		{
			DistanceDragged += FMath::Abs( MouseEvent.GetCursorDelta().X );
			if ( DistanceDragged > 0.f /*FSlateApplication::Get().GetDragTriggerDistance()*/ )
			{
				bPanning = true;
			}
		}
		else if (MouseEvent.IsShiftDown() && MouseEvent.IsAltDown())
		{
			float MouseFractionX = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X / MyGeometry.GetLocalSize().X;

			// If zooming on the current time, adjust mouse fractionX
			if (Sequencer->GetSequencerSettings()->GetZoomPosition() == ESequencerZoomPosition::SZP_CurrentTime)
			{
				const double ScrubPosition = TimeSliderArgs.ScrubPosition.Get() / GetTickResolution();
				if (GetViewRange().Contains(ScrubPosition))
				{
					FScrubRangeToScreen RangeToScreen(GetViewRange(), MyGeometry.Size);
					float TimePosition = RangeToScreen.InputToLocalX(ScrubPosition);
					MouseFractionX = TimePosition / MyGeometry.GetLocalSize().X;
				}
			}

			const float ZoomDelta = -0.01f * MouseEvent.GetCursorDelta().X;
			ZoomByDelta(ZoomDelta, MouseFractionX);
		}
		else 
		{
			TRange<double> LocalViewRange = GetViewRange();
			double LocalViewRangeMin = LocalViewRange.GetLowerBoundValue();
			double LocalViewRangeMax = LocalViewRange.GetUpperBoundValue();

			FScrubRangeToScreen ScaleInfo( LocalViewRange, MyGeometry.Size );
			FVector2D ScreenDelta = MouseEvent.GetCursorDelta();
			FVector2D InputDelta;
			InputDelta.X = ScreenDelta.X/ScaleInfo.PixelsPerInput;

			double NewViewOutputMin = LocalViewRangeMin - InputDelta.X;
			double NewViewOutputMax = LocalViewRangeMax - InputDelta.X;

			ClampViewRange(NewViewOutputMin, NewViewOutputMax);
			SetViewRange(NewViewOutputMin, NewViewOutputMax, EViewRangeInterpolation::Immediate);
		}
	}
	else if ((bHandleLeftMouseButton || bHandleMiddleMouseButton) && MouseDownPosition[0].IsSet())
	{
		TRange<double> LocalViewRange = GetViewRange();
		FScrubRangeToScreen RangeToScreen(LocalViewRange, MyGeometry.Size);
		DistanceDragged += FMath::Abs( MouseEvent.GetCursorDelta().X );

		if ( MouseDragType == DRAG_NONE )
		{
			if ( DistanceDragged > 0.f /*FSlateApplication::Get().GetDragTriggerDistance()*/ )
			{
				FFrameTime MouseDownFree = ComputeFrameTimeFromMouse(MyGeometry, MouseDownPosition[0].GetValue(), RangeToScreen, false);

				const bool       bReadOnly          = Sequencer->IsReadOnly();
				const FFrameRate TickResolution     = GetTickResolution();
				const bool       bLockedPlayRange   = TimeSliderArgs.IsPlaybackRangeLocked.Get();
				const float      MouseDownPixel     = RangeToScreen.InputToLocalX(MouseDownFree / TickResolution);
				const bool       bHitScrubber       = GetHitTestScrubPixelMetrics(RangeToScreen).HandleRangePx.Contains(MouseDownPixel);

				TRange<double>   SelectionRange   = TimeSliderArgs.SelectionRange.Get() / TickResolution;
				TRange<double>   PlaybackRange    = TimeSliderArgs.PlaybackRange.Get()  / TickResolution;

				// Disable selection range test if it's empty so that the playback range scrubbing gets priority
				if (!SelectionRange.IsEmpty() && !bHitScrubber && HitTestRangeEnd(RangeToScreen, SelectionRange, MouseDownPixel) && bHandleMiddleMouseButton == false)
				{
					// selection range end scrubber
					MouseDragType = DRAG_SELECTION_END;
					TimeSliderArgs.OnSelectionRangeBeginDrag.ExecuteIfBound();
				}
				else if (!SelectionRange.IsEmpty() && !bHitScrubber && HitTestRangeStart(RangeToScreen, SelectionRange, MouseDownPixel) && bHandleMiddleMouseButton == false)
				{
					// selection range start scrubber
					MouseDragType = DRAG_SELECTION_START;
					TimeSliderArgs.OnSelectionRangeBeginDrag.ExecuteIfBound();
				}
				else if (!bLockedPlayRange && !bHitScrubber && HitTestRangeEnd(RangeToScreen, PlaybackRange, MouseDownPixel) && bHandleMiddleMouseButton == false)
				{
					// playback range end scrubber
					MouseDragType = DRAG_PLAYBACK_END;
					TimeSliderArgs.OnPlaybackRangeBeginDrag.ExecuteIfBound();
				}
				else if (!bLockedPlayRange && !bHitScrubber && HitTestRangeStart(RangeToScreen, PlaybackRange, MouseDownPixel) && bHandleMiddleMouseButton == false)
				{
					// playback range start scrubber
					MouseDragType = DRAG_PLAYBACK_START;
					TimeSliderArgs.OnPlaybackRangeBeginDrag.ExecuteIfBound();
				}
				else if (!bLockedMarkedFrames && !bHitScrubber && HitTestMark(MyGeometry, RangeToScreen, MouseDownPixel, bFromTimeSlider, &DragMarkIndex) && bHandleMiddleMouseButton == false)
				{
					MouseDragType = DRAG_MARK;
					HandleMarkSelection(DragMarkIndex);

					DragMarkMap.Empty();
					const FMarkedFrameSelection& SelectedMarkedFrames = Sequencer->GetViewModel()->GetSelection()->MarkedFrames;
					const TArray<FMovieSceneMarkedFrame>& MarkedFrames = TimeSliderArgs.MarkedFrames.Get();
					for (TSet<int32>::TConstIterator It = SelectedMarkedFrames.GetSelected(); It; ++It)
					{
						const int32 MarkIndex = *It;		
						const FMovieSceneMarkedFrame& MarkedFrame = MarkedFrames[MarkIndex];
						DragMarkMap.Add(MarkIndex, MarkedFrame.FrameNumber);
					}

					TimeSliderArgs.OnMarkBeginDrag.ExecuteIfBound();
				}
				else if (FSlateApplication::Get().GetModifierKeys().AreModifersDown(EModifierKey::Control) && bHandleMiddleMouseButton == false)
				{
					MouseDragType = DRAG_SETTING_RANGE;
				}
				else if (bMouseDownInRegion)
				{
					MouseDragType = DRAG_SCRUBBING_TIME;
					TimeSliderArgs.OnBeginScrubberMovement.ExecuteIfBound();
				}
			}
		}
		else
		{
			FFrameTime MouseTime = ComputeFrameTimeFromMouse(MyGeometry, MouseEvent.GetScreenSpacePosition(), RangeToScreen);
			FFrameTime ScrubTime = ComputeScrubTimeFromMouse(MyGeometry, MouseEvent, RangeToScreen);
			FFrameTime MouseDownTime = ComputeFrameTimeFromMouse(MyGeometry, MouseDownPosition[0].GetValue(), RangeToScreen);
			FFrameNumber DiffFrame = MouseTime.FrameNumber - MouseDownTime.FrameNumber;

			// Set the start range time?
			if (MouseDragType == DRAG_PLAYBACK_START)
			{
				if (MouseEvent.IsShiftDown())
				{
					SetPlaybackRangeStart(MouseDownPlaybackRange.GetLowerBoundValue() + DiffFrame);
					SetPlaybackRangeEnd(MouseDownPlaybackRange.GetUpperBoundValue() + DiffFrame);
				}
				else
				{
					SetPlaybackRangeStart(MouseTime.FrameNumber);
				}
			}
			// Set the end range time?
			else if(MouseDragType == DRAG_PLAYBACK_END)
			{
				if (MouseEvent.IsShiftDown())
				{
					SetPlaybackRangeStart(MouseDownPlaybackRange.GetLowerBoundValue() + DiffFrame);
					SetPlaybackRangeEnd(MouseDownPlaybackRange.GetUpperBoundValue() + DiffFrame);
				}
				else
				{		
					SetPlaybackRangeEnd(MouseTime.FrameNumber);
				}
			}
			else if (MouseDragType == DRAG_SELECTION_START)
			{
				if (MouseEvent.IsShiftDown())
				{
					SetSelectionRangeStart(MouseDownSelectionRange.GetLowerBoundValue() + DiffFrame);
					SetSelectionRangeEnd(MouseDownSelectionRange.GetUpperBoundValue() + DiffFrame);
				}
				else
				{
					SetSelectionRangeStart(MouseTime.FrameNumber);
				}
			}
			// Set the end range time?
			else if(MouseDragType == DRAG_SELECTION_END)
			{
				if (MouseEvent.IsShiftDown())
				{
					SetSelectionRangeStart(MouseDownSelectionRange.GetLowerBoundValue() + DiffFrame);
					SetSelectionRangeEnd(MouseDownSelectionRange.GetUpperBoundValue() + DiffFrame);
				}
				else 
				{
					SetSelectionRangeEnd(MouseTime.FrameNumber);
				}
			}
			else if (MouseDragType == DRAG_MARK)
			{
				SetMark(DiffFrame);
			}
			else if (MouseDragType == DRAG_SCRUBBING_TIME)
			{
				// Delegate responsibility for clamping to the current viewrange to the client
				CommitScrubPosition( ScrubTime, /*bIsScrubbing=*/true, /*bEvaluate*/ !bHandleMiddleMouseButton); //if middle mouse button down we don't evaluate on the time change
			}
			else if (MouseDragType == DRAG_SETTING_RANGE)
			{
				MouseDownPosition[1] = MouseEvent.GetScreenSpacePosition();
			}
		}
	}
	else if (bFromTimeSlider && !bLockedMarkedFrames && DragMarkIndex == INDEX_NONE)
	{
		// Update hover state of marked frames.
		TRange<double> LocalViewRange = GetViewRange();
		FScrubRangeToScreen RangeToScreen(LocalViewRange, MyGeometry.Size);
		const FVector2D LocalMousePostion = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		HitTestMark(MyGeometry, RangeToScreen, LocalMousePostion.X, bFromTimeSlider, &HoverMarkIndex);
	}

	if ( DistanceDragged != 0.f && (bHandleLeftMouseButton || bHandleRightMouseButton) )
	{
		return FReply::Handled().CaptureMouse(WidgetOwner.AsShared());
	}


	return FReply::Handled();
}


void FSequencerTimeSliderController::CommitScrubPosition( FFrameTime NewValue, bool bIsScrubbing, bool bEvaluate)
{
	bIsEvaluating = bEvaluate;
	// The user can scrub past the viewing range of the time slider controller, so we clamp it to the view range.
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if(Sequencer.IsValid() && bIsScrubbing)
	{
		FAnimatedRange ViewRange = GetViewRange();
		
		FFrameRate DisplayRate = Sequencer->GetFocusedDisplayRate();
		FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();

		FFrameTime LowerBound = (ViewRange.GetLowerBoundValue() * TickResolution).CeilToFrame();
		FFrameTime UpperBound = (ViewRange.GetUpperBoundValue() * TickResolution).FloorToFrame();

		if (Sequencer->GetSequencerSettings()->GetIsSnapEnabled() && Sequencer->GetSequencerSettings()->GetSnapPlayTimeToInterval())
		{
			LowerBound = FFrameRate::Snap(LowerBound, TickResolution, DisplayRate);
			UpperBound = FFrameRate::Snap(UpperBound, TickResolution, DisplayRate);
		}

		NewValue = FMath::Clamp(NewValue, LowerBound, UpperBound);
	}

	// Manage the scrub position ourselves if its not bound to a delegate
	if ( !TimeSliderArgs.ScrubPosition.IsBound() )
	{
		TimeSliderArgs.ScrubPosition.Set( NewValue );
	}

	TimeSliderArgs.OnScrubPositionChanged.ExecuteIfBound( NewValue, bIsScrubbing, bEvaluate);
}

FReply FSequencerTimeSliderController::OnMouseWheel( SWidget& WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	TOptional<TRange<float>> NewTargetRange;

	if ( TimeSliderArgs.AllowZoom && MouseEvent.IsControlDown() )
	{
		float MouseFractionX = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X / MyGeometry.GetLocalSize().X;

		TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
			
		// If zooming on the current time, adjust mouse fractionX
		if (Sequencer.IsValid() && Sequencer->GetSequencerSettings()->GetZoomPosition() == ESequencerZoomPosition::SZP_CurrentTime)
		{
			const double ScrubPosition = TimeSliderArgs.ScrubPosition.Get() / GetTickResolution();
			if (GetViewRange().Contains(ScrubPosition))
			{
				FScrubRangeToScreen RangeToScreen(GetViewRange(), MyGeometry.Size);
				float TimePosition = RangeToScreen.InputToLocalX(ScrubPosition);
				MouseFractionX = TimePosition / MyGeometry.GetLocalSize().X;
			}
		}

		const float ZoomDelta = -0.2f * MouseEvent.GetWheelDelta();
		if (ZoomByDelta(ZoomDelta, MouseFractionX))
		{
			return FReply::Handled();
		}
	}
	else if (MouseEvent.IsShiftDown())
	{
		PanByDelta(-MouseEvent.GetWheelDelta());
		return FReply::Handled();
	}
	
	return FReply::Unhandled();
}

FCursorReply FSequencerTimeSliderController::OnCursorQuery( TSharedRef<const SWidget> WidgetOwner, const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const
{
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return FCursorReply::Unhandled();
	}

	FScrubRangeToScreen RangeToScreen(GetViewRange(), MyGeometry.Size);

	const bool       bReadOnly        = Sequencer->IsReadOnly();
	const FFrameRate TickResolution   = GetTickResolution();
	const bool       bLockedPlayRange = TimeSliderArgs.IsPlaybackRangeLocked.Get();
	const bool       bLockedMarkedFrames = TimeSliderArgs.AreMarkedFramesLocked.Get();
	const float      HitTestPixel     = MyGeometry.AbsoluteToLocal(CursorEvent.GetScreenSpacePosition()).X;
	const bool       bHitScrubber     = GetHitTestScrubPixelMetrics(RangeToScreen).HandleRangePx.Contains(HitTestPixel);

	TRange<double>   SelectionRange   = TimeSliderArgs.SelectionRange.Get() / TickResolution;
	TRange<double>   PlaybackRange    = TimeSliderArgs.PlaybackRange.Get()  / TickResolution;

	if (MouseDragType == DRAG_SCRUBBING_TIME)
	{
		return FCursorReply::Unhandled();
	}

	// Use L/R resize cursor if we're dragging or hovering a playback range bound
	if ((MouseDragType == DRAG_PLAYBACK_END) ||
		(MouseDragType == DRAG_PLAYBACK_START) ||
		(MouseDragType == DRAG_SELECTION_START) ||
		(MouseDragType == DRAG_SELECTION_END) ||
		(!bLockedPlayRange         && !bHitScrubber && HitTestRangeStart(RangeToScreen, PlaybackRange,  HitTestPixel)) ||
		(!bLockedPlayRange         && !bHitScrubber && HitTestRangeEnd(  RangeToScreen, PlaybackRange,  HitTestPixel)) ||
		(!SelectionRange.IsEmpty() && !bHitScrubber && HitTestRangeStart(RangeToScreen, SelectionRange, HitTestPixel)) ||
		(!SelectionRange.IsEmpty() && !bHitScrubber && HitTestRangeEnd(  RangeToScreen, SelectionRange, HitTestPixel)))
	{
		return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
	}

	const bool bFromTimeSlider = true;
	if (MouseDragType == DRAG_MARK || (!bLockedMarkedFrames && !bHitScrubber && HitTestMark(MyGeometry, RangeToScreen, HitTestPixel, bFromTimeSlider)))
	{
		return FCursorReply::Cursor(EMouseCursor::CardinalCross);
	}

	return FCursorReply::Unhandled();
}

int32 FSequencerTimeSliderController::OnPaintViewArea( const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, bool bEnabled, const FPaintViewAreaArgs& Args ) const
{
	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return LayerId;
	}

	const ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	TRange<double> LocalViewRange = GetViewRange();
	FScrubRangeToScreen RangeToScreen( LocalViewRange, AllottedGeometry.Size );

	if (Args.PlaybackRangeArgs.IsSet())
	{
		FPaintPlaybackRangeArgs PaintArgs = Args.PlaybackRangeArgs.GetValue();
		LayerId = DrawPlaybackRange(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, RangeToScreen, PaintArgs);
		LayerId = DrawSubSequenceRange(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, RangeToScreen, PaintArgs);
		PaintArgs.SolidFillOpacity = 0.f;
		LayerId = DrawSelectionRange(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, RangeToScreen, PaintArgs);
	}

	if( Args.bDisplayTickLines )
	{
		static FLinearColor TickColor(0.f, 0.f, 0.f, 0.3f);

		// Draw major tick lines in the section area
		FDrawTickArgs DrawTickArgs;
		{
			DrawTickArgs.AllottedGeometry = AllottedGeometry;
			DrawTickArgs.bMirrorLabels = false;
			DrawTickArgs.bOnlyDrawMajorTicks = true;
			DrawTickArgs.TickColor = TickColor;
			DrawTickArgs.CullingRect = MyCullingRect;
			DrawTickArgs.DrawEffects = DrawEffects;
			// Draw major ticks under sections
			DrawTickArgs.StartLayer = LayerId-1;
			// Draw the tick the entire height of the section area
			DrawTickArgs.TickOffset = 0.0f;
			DrawTickArgs.MajorTickHeight = AllottedGeometry.Size.Y;
		}

		DrawTicks( OutDrawElements, LocalViewRange, RangeToScreen, DrawTickArgs );
	}

	if (Args.bDisplayMarkedFrames)
	{
		LayerId = DrawMarkedFrames(AllottedGeometry, RangeToScreen, OutDrawElements, LayerId, DrawEffects, FWidgetStyle(), false);
	}

	LayerId = DrawVerticalFrames(AllottedGeometry, RangeToScreen, OutDrawElements, LayerId, DrawEffects);

	if( Args.bDisplayScrubPosition )
	{
		FQualifiedFrameTime ScrubPosition = FQualifiedFrameTime(TimeSliderArgs.ScrubPosition.Get(), GetTickResolution());
		FScrubberMetrics    ScrubMetrics = GetScrubPixelMetrics(ScrubPosition, RangeToScreen);

		if (ScrubMetrics.bDrawExtents)
		{
			// Draw a box for the scrub position
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 1,
				AllottedGeometry.ToPaintGeometry(FVector2f(ScrubMetrics.FrameExtentsPx.Size<float>(), AllottedGeometry.Size.Y), FSlateLayoutTransform(FVector2f(ScrubMetrics.FrameExtentsPx.GetLowerBoundValue(), 0.0f))),
				ScrubFillBrush,
				DrawEffects,
				FLinearColor::White.CopyWithNewOpacity(0.5f)
			);
		}

			// Draw a line for the scrub position
			TArray<FVector2D> LinePoints;
			{
			float LinePos = RangeToScreen.InputToLocalX(ScrubPosition.AsSeconds());

				LinePoints.AddUninitialized(2);
			LinePoints[0] = FVector2D( LinePos, 0.0f );
			LinePoints[1] = FVector2D( LinePos, FMath::FloorToFloat( AllottedGeometry.Size.Y ) );
			}

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId+1,
			AllottedGeometry.ToPaintGeometry(),
				LinePoints,
				DrawEffects,
				FLinearColor(1.f, 1.f, 1.f, .5f),
				false
			);
		}

	return LayerId;
}

void FSequencerTimeSliderController::SetPlaybackStatus(ETimeSliderPlaybackStatus InStatus)
{
	using namespace UE::Sequencer;
	if (!WeakSequencer.IsValid())
	{
		return;
	}
	switch (InStatus)
	{
		case ETimeSliderPlaybackStatus::Jumping:
		{
			WeakSequencer.Pin()->SetPlaybackStatus(EMovieScenePlayerStatus::Jumping);
			break;
		}
		case ETimeSliderPlaybackStatus::Paused:
		{
			WeakSequencer.Pin()->SetPlaybackStatus(EMovieScenePlayerStatus::Paused);
			break;
		}
		case ETimeSliderPlaybackStatus::Playing:
		{
			WeakSequencer.Pin()->SetPlaybackStatus(EMovieScenePlayerStatus::Playing);
			break;
		}
		case ETimeSliderPlaybackStatus::Scrubbing:
		{
			WeakSequencer.Pin()->SetPlaybackStatus(EMovieScenePlayerStatus::Scrubbing);
			break;
		}
		case ETimeSliderPlaybackStatus::Stepping:
		{
			WeakSequencer.Pin()->SetPlaybackStatus(EMovieScenePlayerStatus::Stepping);
			break;
		}
		default:
		case ETimeSliderPlaybackStatus::Stopped:
		{
			WeakSequencer.Pin()->SetPlaybackStatus(EMovieScenePlayerStatus::Stopped);
			break;
		}
	}
}

ETimeSliderPlaybackStatus FSequencerTimeSliderController::GetPlaybackStatus() const
{
	using namespace UE::Sequencer;
	if (!WeakSequencer.IsValid())
	{
		return ETimeSliderPlaybackStatus::Stopped;
	}
	EMovieScenePlayerStatus::Type Status = WeakSequencer.Pin()->GetPlaybackStatus();
	switch (Status)
	{
		case EMovieScenePlayerStatus::Jumping:
		{
			return ETimeSliderPlaybackStatus::Jumping;
		}
		case EMovieScenePlayerStatus::Paused:
		{
			return ETimeSliderPlaybackStatus::Paused;
		}
		case EMovieScenePlayerStatus::Playing:
		{
			return ETimeSliderPlaybackStatus::Playing;
		}
		case EMovieScenePlayerStatus::Scrubbing:
		{
			return ETimeSliderPlaybackStatus::Scrubbing;
		}
		case EMovieScenePlayerStatus::Stepping:
		{
			return ETimeSliderPlaybackStatus::Stepping;
		}
		case EMovieScenePlayerStatus::Stopped:
		{
			return ETimeSliderPlaybackStatus::Stopped;
		}

	}
	return ETimeSliderPlaybackStatus::Stopped;

}

TSharedRef<SWidget> FSequencerTimeSliderController::OpenSetPlaybackRangeMenu(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	using namespace UE::Sequencer;

	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	const bool bReadOnly = Sequencer && Sequencer->IsReadOnly();
	
	FScrubRangeToScreen RangeToScreen = FScrubRangeToScreen(GetViewRange(), MyGeometry.Size);
	const float MousePixel = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X;
	FFrameNumber FrameNumber = ComputeFrameTimeFromMouse(MyGeometry, MouseEvent.GetScreenSpacePosition(), RangeToScreen).FrameNumber;

	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);

	FText CurrentTimeText;
	CurrentTimeText = FText::FromString(TimeSliderArgs.NumericTypeInterface->ToString(FrameNumber.Value));
	
	TRange<FFrameNumber> PlaybackRange = TimeSliderArgs.PlaybackRange.Get();
	TOptional<TRange<FFrameNumber>> SubSequenceRange = TimeSliderArgs.SubSequenceRange.Get();

	MenuBuilder.BeginSection("SequencerPlaybackRangeMenu", FText::Format(LOCTEXT("PlaybackRangeTextFormat", "Playback Range ({0}):"), CurrentTimeText));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetPlaybackStart", "Set Start Time"),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, FrameNumber]{ SetPlaybackRangeStart(FrameNumber); }),
				FCanExecuteAction::CreateLambda([this, FrameNumber, PlaybackRange]{ return !TimeSliderArgs.IsPlaybackRangeLocked.Get() && FrameNumber < UE::MovieScene::DiscreteExclusiveUpper(PlaybackRange); })
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetPlaybackEnd", "Set End Time"),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, FrameNumber]{ SetPlaybackRangeEnd(FrameNumber); }),
				FCanExecuteAction::CreateLambda([this, FrameNumber, PlaybackRange]{ return !TimeSliderArgs.IsPlaybackRangeLocked.Get() && FrameNumber >= UE::MovieScene::DiscreteInclusiveLower(PlaybackRange); })
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ConformToSubsequenceRange", "Conform to Range"),
			LOCTEXT("ConformToSubsequenceRangeTooltip", "Conform the start and end time to the extents of the subsequence range"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, SubSequenceRange] { SetPlaybackRangeStart(SubSequenceRange.GetValue().GetLowerBoundValue()); SetPlaybackRangeEnd(SubSequenceRange.GetValue().GetUpperBoundValue()); }),
				FCanExecuteAction::CreateLambda([this, SubSequenceRange] { return !TimeSliderArgs.IsPlaybackRangeLocked.Get() && SubSequenceRange.IsSet(); })
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ToggleLocked", "Locked"),
			LOCTEXT("ToggleLockedTooltip", "Lock/Unlock the playback range"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this] { TimeSliderArgs.OnTogglePlaybackRangeLocked.ExecuteIfBound(); }),
				FCanExecuteAction::CreateLambda([bReadOnly]{ return !bReadOnly; }),
				FIsActionChecked::CreateLambda([this] { return TimeSliderArgs.IsPlaybackRangeLocked.Get(); })
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	}
	MenuBuilder.EndSection(); // SequencerPlaybackRangeMenu

	TRange<FFrameNumber> SelectionRange = TimeSliderArgs.SelectionRange.Get();
	MenuBuilder.BeginSection("SequencerSelectionRangeMenu", FText::Format(LOCTEXT("SelectionRangeTextFormat", "Selection Range ({0}):"), CurrentTimeText));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetSelectionStart", "Set Selection Start"),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, FrameNumber]{ SetSelectionRangeStart(FrameNumber); }),
				FCanExecuteAction::CreateLambda([=]{ return SelectionRange.IsEmpty() || FrameNumber < UE::MovieScene::DiscreteExclusiveUpper(SelectionRange); })
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SetSelectionEnd", "Set Selection End"),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, FrameNumber]{ SetSelectionRangeEnd(FrameNumber); }),
				FCanExecuteAction::CreateLambda([=]{ return SelectionRange.IsEmpty() || FrameNumber >= UE::MovieScene::DiscreteInclusiveLower(SelectionRange); })
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ClearSelectionRange", "Clear Selection Range"),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this]{ TimeSliderArgs.OnSelectionRangeChanged.ExecuteIfBound(TRange<FFrameNumber>::Empty()); }),
				FCanExecuteAction::CreateLambda([=]{ return !SelectionRange.IsEmpty(); })
			)
		);
	}
	MenuBuilder.EndSection(); // SequencerPlaybackRangeMenu

	UMovieSceneCompiledDataManager* CompiledDataManager = Sequencer->GetEvaluationTemplate().GetCompiledDataManager();
	const FMovieSceneSequenceHierarchy* Hierarchy = CompiledDataManager->FindHierarchy(Sequencer->GetEvaluationTemplate().GetCompiledDataID());

	if (TimeSliderArgs.ScrubPositionParentChain.IsSet() && Hierarchy)
	{
		MenuBuilder.BeginSection("SequencerParentChainMenu");
		{
			TArray<FMovieSceneSequenceID> ParentChain = TimeSliderArgs.ScrubPositionParentChain.Get();
			for (FMovieSceneSequenceID ParentID : ParentChain)
			{
				FText ParentText = Sequencer->GetRootMovieSceneSequence()->GetDisplayName();

				for (const TTuple<FMovieSceneSequenceID, FMovieSceneSubSequenceData>& Pair : Hierarchy->AllSubSequenceData())
				{
					if (Pair.Key == ParentID && Pair.Value.GetSequence())
					{
						ParentText = Pair.Value.GetSequence()->GetDisplayName();
						break;
					}
				}

				MenuBuilder.AddMenuEntry(
					ParentText,
					FText::Format(LOCTEXT("DisplayTimeSpace", "Display time in the space of {0}"), ParentText),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda([this, ParentID] { TimeSliderArgs.OnScrubPositionParentChanged.ExecuteIfBound(ParentID); }),
						FCanExecuteAction(),
						FIsActionChecked::CreateLambda([this, ParentID] { return TimeSliderArgs.ScrubPositionParent.Get() == MovieSceneSequenceID::Invalid ? ParentID == TimeSliderArgs.ScrubPositionParentChain.Get().Last() : TimeSliderArgs.ScrubPositionParent.Get() == ParentID; })
					),
					NAME_None,
					EUserInterfaceActionType::RadioButton
				);
			}
		}
		MenuBuilder.EndSection(); // Sequencer Parent Chain
	}

	MenuBuilder.BeginSection("SequencerMarkMenu", FText::Format(LOCTEXT("MarkTextFormat", "Mark ({0}):"), CurrentTimeText));
	{
		UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
		bool bHasMarks = MovieScene->GetMarkedFrames().Num() > 0;

		int32 MarkedIndex = INDEX_NONE;
		const bool bTestLabelBox = true;
		HitTestMark(MyGeometry, RangeToScreen, MousePixel, bTestLabelBox, &MarkedIndex);

		FSequencerSelection& SequencerSelection = Sequencer->GetSelection();
		SequencerSelection.Empty();

		if (MarkedIndex != INDEX_NONE)
		{
			SequencerSelection.MarkedFrames.Empty();
			SequencerSelection.MarkedFrames.Select(MarkedIndex);

			class SMarkedFramePropertyWidget : public SCompoundWidget, public FNotifyHook
			{
			public:
				UMovieScene* MovieSceneToModify;
				TSharedPtr<IStructureDetailsView> DetailsView;
				TWeakPtr<FSequencer> WeakSequencer;

				SLATE_BEGIN_ARGS(SMarkedFramePropertyWidget){}
				SLATE_END_ARGS()

				void Construct(const FArguments& InArgs, UMovieScene* InMovieScene, int32 InMarkedFrameIndex, TWeakPtr<FSequencer> InWeakSequencer)
				{
					MovieSceneToModify = InMovieScene;
					WeakSequencer = InWeakSequencer;

					FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
					FDetailsViewArgs DetailsViewArgs;
					DetailsViewArgs.bAllowSearch = false;
					DetailsViewArgs.bShowScrollBar = false;
					DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
					DetailsViewArgs.NotifyHook = this;

					FStructureDetailsViewArgs StructureDetailsViewArgs;
					StructureDetailsViewArgs.bShowObjects = true;
					StructureDetailsViewArgs.bShowAssets = true;
					StructureDetailsViewArgs.bShowClasses = true;
					StructureDetailsViewArgs.bShowInterfaces = true;

					TSharedPtr<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(FMovieSceneMarkedFrame::StaticStruct(), (uint8 *)&InMovieScene->GetMarkedFrames()[InMarkedFrameIndex]);

					DetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructureDetailsViewArgs, nullptr);
					DetailsView->GetDetailsView()->RegisterInstancedCustomPropertyTypeLayout("FrameNumber", FOnGetPropertyTypeCustomizationInstance::CreateLambda([this]() {
						return MakeShared<FFrameNumberDetailsCustomization>(WeakSequencer.Pin()->GetNumericTypeInterface()); }));
					DetailsView->SetStructureData(StructOnScope);

					ChildSlot
					[
						DetailsView->GetWidget().ToSharedRef()
					];
				}

				virtual void NotifyPreChange( FProperty* PropertyAboutToChange ) override
				{
					MovieSceneToModify->Modify();
				}

				virtual void NotifyPreChange( class FEditPropertyChain* PropertyAboutToChange ) override
				{
					MovieSceneToModify->Modify();
				}
			};

			const bool bLockedMarkedFrames = TimeSliderArgs.AreMarkedFramesLocked.Get();

			TSharedRef<SMarkedFramePropertyWidget> Widget = SNew(SMarkedFramePropertyWidget, MovieScene, MarkedIndex, WeakSequencer);
			Widget->SetEnabled(!bLockedMarkedFrames);
			MenuBuilder.AddWidget(Widget, FText::GetEmpty(), false);
		}

		MenuBuilder.AddMenuEntry( 
			LOCTEXT("AddMark", "Add Mark"),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda( [this, FrameNumber]{ AddMarkAtFrame(FrameNumber); }),
				FCanExecuteAction::CreateLambda([this, MarkedIndex]{ return !TimeSliderArgs.AreMarkedFramesLocked.Get() && MarkedIndex == INDEX_NONE; }))
		);

		MenuBuilder.AddMenuEntry( 
			LOCTEXT("DeleteMark", "Delete Mark"),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, MarkedIndex]{ DeleteMarkAtIndex(MarkedIndex); }),
				FCanExecuteAction::CreateLambda([this, MarkedIndex]{ return !TimeSliderArgs.AreMarkedFramesLocked.Get() && MarkedIndex != INDEX_NONE; }))
		);

		MenuBuilder.AddMenuEntry( 
			LOCTEXT("DeleteAllMarks", "Delete All Marks"),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this]{ DeleteAllMarks(); }),
				FCanExecuteAction::CreateLambda([this, bHasMarks]{ return !TimeSliderArgs.AreMarkedFramesLocked.Get() && bHasMarks; }))
			);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ToggleLockedMarks", "Locked"),
			LOCTEXT("ToggleLockedMarksTooltip", "Lock/Unlock all marked frames"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this]{ TimeSliderArgs.OnToggleMarkedFramesLocked.ExecuteIfBound(); }),
				FCanExecuteAction::CreateLambda([=]{ return !bReadOnly; }),
				FIsActionChecked::CreateLambda([this]{ return TimeSliderArgs.AreMarkedFramesLocked.Get(); })),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
			);
	}
	MenuBuilder.EndSection(); // SequencerMarkMenu

	return MenuBuilder.MakeWidget();
}

void FSequencerTimeSliderController::ClampViewRange(double& NewRangeMin, double& NewRangeMax)
{
	bool bNeedsClampSet = false;
	double NewClampRangeMin = TimeSliderArgs.ClampRange.Get().GetLowerBoundValue();
	if ( NewRangeMin < TimeSliderArgs.ClampRange.Get().GetLowerBoundValue() )
	{
		NewClampRangeMin = NewRangeMin;
		bNeedsClampSet = true;
	}

	double NewClampRangeMax = TimeSliderArgs.ClampRange.Get().GetUpperBoundValue();
	if ( NewRangeMax > TimeSliderArgs.ClampRange.Get().GetUpperBoundValue() )
	{
		NewClampRangeMax = NewRangeMax;
		bNeedsClampSet = true;
	}

	if (bNeedsClampSet)
	{
		SetClampRange(NewClampRangeMin, NewClampRangeMax);
	}
}

void FSequencerTimeSliderController::SetViewRange( double NewRangeMin, double NewRangeMax, EViewRangeInterpolation Interpolation )
{
	// Clamp to a minimum size to avoid zero-sized or negative visible ranges
	double MinVisibleTimeRange = FFrameNumber(1) / GetTickResolution();
	TRange<double> ExistingViewRange  = GetViewRange();
	TRange<double> ExistingClampRange = TimeSliderArgs.ClampRange.Get();

	if (NewRangeMax == ExistingViewRange.GetUpperBoundValue())
	{
		if (NewRangeMin > NewRangeMax - MinVisibleTimeRange)
		{
			NewRangeMin = NewRangeMax - MinVisibleTimeRange;
		}
	}
	else if (NewRangeMax < NewRangeMin + MinVisibleTimeRange)
	{
		NewRangeMax = NewRangeMin + MinVisibleTimeRange;
	}

	// Clamp to the clamp range
	const TRange<double> NewRange = TRange<double>::Intersection(TRange<double>(NewRangeMin, NewRangeMax), ExistingClampRange);
	TimeSliderArgs.OnViewRangeChanged.ExecuteIfBound( NewRange, Interpolation );

	if( !TimeSliderArgs.ViewRange.IsBound() )
	{
		// The  output is not bound to a delegate so we'll manage the value ourselves (no animation)
		TimeSliderArgs.ViewRange.Set( NewRange );
	}
}

void FSequencerTimeSliderController::SetClampRange( double NewRangeMin, double NewRangeMax )
{
	const TRange<double> NewRange(NewRangeMin, NewRangeMax);

	TimeSliderArgs.OnClampRangeChanged.ExecuteIfBound(NewRange);

	if( !TimeSliderArgs.ClampRange.IsBound() )
	{	
		// The  output is not bound to a delegate so we'll manage the value ourselves (no animation)
		TimeSliderArgs.ClampRange.Set(NewRange);
	}
}

void FSequencerTimeSliderController::SetPlayRange( FFrameNumber RangeStart, int32 RangeDuration )
{
	check(RangeDuration >= 0);

	const TRange<FFrameNumber> NewRange(RangeStart, RangeStart + RangeDuration);

	TimeSliderArgs.OnPlaybackRangeChanged.ExecuteIfBound(NewRange);

	if( !TimeSliderArgs.PlaybackRange.IsBound() )
	{
		// The  output is not bound to a delegate so we'll manage the value ourselves (no animation)
		TimeSliderArgs.PlaybackRange.Set(NewRange);
	}
}

void FSequencerTimeSliderController::SetSelectionRange(const TRange<FFrameNumber>& NewRange)
{
	TimeSliderArgs.OnSelectionRangeChanged.ExecuteIfBound(NewRange);
}

bool FSequencerTimeSliderController::ZoomByDelta( float InDelta, float MousePositionFraction )
{
	TRange<double> LocalViewRange = GetViewRange().GetAnimationTarget();
	double LocalViewRangeMax = LocalViewRange.GetUpperBoundValue();
	double LocalViewRangeMin = LocalViewRange.GetLowerBoundValue();
	const double OutputViewSize = LocalViewRangeMax - LocalViewRangeMin;
	const double OutputChange = OutputViewSize * InDelta;

	double NewViewOutputMin = LocalViewRangeMin - (OutputChange * MousePositionFraction);
	double NewViewOutputMax = LocalViewRangeMax + (OutputChange * (1.f - MousePositionFraction));

	if( NewViewOutputMin < NewViewOutputMax )
	{
		ClampViewRange(NewViewOutputMin, NewViewOutputMax);
		SetViewRange(NewViewOutputMin, NewViewOutputMax, EViewRangeInterpolation::Animated);
		return true;
	}

	return false;
}

void FSequencerTimeSliderController::PanByDelta( float InDelta )
{
	TRange<double> LocalViewRange = GetViewRange().GetAnimationTarget();

	double CurrentMin = LocalViewRange.GetLowerBoundValue();
	double CurrentMax = LocalViewRange.GetUpperBoundValue();

	// Adjust the delta to be a percentage of the current range
	InDelta *= ScrubConstants::ScrollPanFraction * (CurrentMax - CurrentMin);

	double NewViewOutputMin = CurrentMin + InDelta;
	double NewViewOutputMax = CurrentMax + InDelta;

	ClampViewRange(NewViewOutputMin, NewViewOutputMax);
	SetViewRange(NewViewOutputMin, NewViewOutputMax, EViewRangeInterpolation::Animated);
}


bool FSequencerTimeSliderController::HitTestRangeStart(const FScrubRangeToScreen& RangeToScreen, const TRange<double>& Range, float HitPixel) const
{
	static float BrushSizeInStateUnits = 6.f, DragToleranceSlateUnits = 2.f, MouseTolerance = 2.f;
	const float  RangeStartPixel = RangeToScreen.InputToLocalX(Range.GetLowerBoundValue());

	// Hit test against the brush region to the right of the playback start position, +/- DragToleranceSlateUnits
	return HitPixel >= RangeStartPixel - MouseTolerance - DragToleranceSlateUnits &&
		HitPixel <= RangeStartPixel + MouseTolerance + BrushSizeInStateUnits + DragToleranceSlateUnits;
}

bool FSequencerTimeSliderController::HitTestRangeEnd(const FScrubRangeToScreen& RangeToScreen, const TRange<double>& Range, float HitPixel) const
{
	static float BrushSizeInStateUnits = 6.f, DragToleranceSlateUnits = 2.f, MouseTolerance = 2.f;
	const float  RangeEndPixel = RangeToScreen.InputToLocalX(Range.GetUpperBoundValue());

	// Hit test against the brush region to the left of the playback end position, +/- DragToleranceSlateUnits
	return HitPixel >= RangeEndPixel - MouseTolerance - BrushSizeInStateUnits - DragToleranceSlateUnits &&
		HitPixel <= RangeEndPixel + MouseTolerance + DragToleranceSlateUnits;
}

bool FSequencerTimeSliderController::HitTestMark(const FGeometry& AllottedGeometry, const FScrubRangeToScreen& RangeToScreen, float HitPixel, bool bTestLabelBox, int32* OutMarkIndex, FFrameNumber* OutMarkFrameNumber) const
{
	if (OutMarkIndex)
	{
		*OutMarkIndex = INDEX_NONE;
	}
	if (OutMarkFrameNumber)
	{
		*OutMarkFrameNumber = 0;
	}

	const TArray<FMovieSceneMarkedFrame> & MarkedFrames = TimeSliderArgs.MarkedFrames.Get();
	if (MarkedFrames.Num() < 1)
	{
		return false;
	}

	static float BrushSizeInStateUnits = 3.f, DragToleranceSlateUnits = 2.f, MouseTolerance = 2.f;

	for (int32 MarkIndex = 0; MarkIndex < MarkedFrames.Num(); ++MarkIndex)
	{
		double Seconds = MarkedFrames[MarkIndex].FrameNumber / GetTickResolution();

		float MarkPixel = RangeToScreen.InputToLocalX(Seconds);

		// Hit test against the brush region to the left/right of the mark position, +/- DragToleranceSlateUnits
		if ((HitPixel >= MarkPixel - MouseTolerance - DragToleranceSlateUnits &&
			 HitPixel <= MarkPixel + MouseTolerance + BrushSizeInStateUnits + DragToleranceSlateUnits) ||
			(HitPixel >= MarkPixel - MouseTolerance - BrushSizeInStateUnits - DragToleranceSlateUnits &&
			 HitPixel <= MarkPixel + MouseTolerance + DragToleranceSlateUnits))
		{
			if (OutMarkIndex)
			{
				*OutMarkIndex = MarkIndex;
			}
			if (OutMarkFrameNumber)
			{
				*OutMarkFrameNumber = MarkedFrames[MarkIndex].FrameNumber;
			}
			return true;
		}

		// Hit test against the label box, which is the text size offset and grown by the margin.
		if (bTestLabelBox)
		{
			FVector2D TextPosition, TextSize;
			bool bDrawLeft;
			GetMarkLabelGeometry(AllottedGeometry, RangeToScreen, MarkedFrames[MarkIndex], TextPosition, TextSize, bDrawLeft);
			FVector2D TextBoxSize = TextSize + ScrubConstants::MarkLabelBoxMargin + ScrubConstants::MarkLabelBoxWideMargin;
			if (HitPixel >= MarkPixel && HitPixel <= MarkPixel + TextBoxSize.X)
			{
				if (OutMarkIndex)
				{
					*OutMarkIndex = MarkIndex;
				}
				if (OutMarkFrameNumber)
				{
					*OutMarkFrameNumber = MarkedFrames[MarkIndex].FrameNumber;
				}
				return true;
			}
		}
	}

	return false;
}

void FSequencerTimeSliderController::GetMarkLabelGeometry(const FGeometry& AllottedGeometry, const FScrubRangeToScreen& RangeToScreen, const FMovieSceneMarkedFrame& MarkedFrame, FVector2D& OutPosition, FVector2D& OutSize, bool& bIsDrawLeft) const
{
	double Seconds = MarkedFrame.FrameNumber / GetTickResolution();
	float MarkPixel = RangeToScreen.InputToLocalX(Seconds);

	const FString& LabelString = MarkedFrame.Label;
	FVector2D TextSize = FontMeasureService->Measure(LabelString, SmallLayoutFont);

	// Flip the text position if getting near the end of the view range
	bool bDrawLeft = (AllottedGeometry.Size.X - MarkPixel) < (TextSize.X + ScrubConstants::MarkLabelBoxMargin + ScrubConstants::MarkLabelBoxWideMargin);
	float TextPosition = bDrawLeft ? 
		MarkPixel - TextSize.X - ScrubConstants::MarkLabelBoxMargin : 
		MarkPixel + ScrubConstants::MarkLabelBoxMargin;

	OutPosition = FVector2D(TextPosition, 0);
	OutSize = TextSize;
	bIsDrawLeft = bDrawLeft;
}

FFrameTime FSequencerTimeSliderController::SnapTimeToNearestKey(const FPointerEvent& MouseEvent, const FScrubRangeToScreen& RangeToScreen, float CursorPos, FFrameTime InTime) const
{
	using namespace UE::Sequencer;

	if (!WeakSequencer.IsValid())
	{
		return InTime;
	}

	if (TimeSliderArgs.OnGetNearestKey.IsBound())
	{
		ENearestKeyOption NearestKeyOption = ENearestKeyOption::NKO_None;

		if (WeakSequencer.Pin()->GetSequencerSettings()->GetSnapPlayTimeToKeys() || MouseEvent.IsShiftDown())
		{
			EnumAddFlags(NearestKeyOption, ENearestKeyOption::NKO_SearchKeys);
		}

		if (WeakSequencer.Pin()->GetSequencerSettings()->GetSnapPlayTimeToSections() || MouseEvent.IsShiftDown())
		{
			EnumAddFlags(NearestKeyOption, ENearestKeyOption::NKO_SearchSections);
		}

		if (WeakSequencer.Pin()->GetSequencerSettings()->GetSnapPlayTimeToMarkers() || MouseEvent.IsShiftDown())
		{
			EnumAddFlags(NearestKeyOption, ENearestKeyOption::NKO_SearchMarkers);
		}

		FFrameNumber NearestKey = TimeSliderArgs.OnGetNearestKey.Execute(InTime, NearestKeyOption);

		float LocalKeyPos = RangeToScreen.InputToLocalX( NearestKey / GetTickResolution() );
		static float MouseTolerance = 20.f;

		if (FMath::IsNearlyEqual(LocalKeyPos, CursorPos, MouseTolerance))
		{
			return NearestKey;
		}
	}

	return InTime;
}

void FSequencerTimeSliderController::SetPlaybackRangeStart(FFrameNumber NewStart)
{
	TRange<FFrameNumber> PlaybackRange = TimeSliderArgs.PlaybackRange.Get();

	if (NewStart <= UE::MovieScene::DiscreteExclusiveUpper(PlaybackRange))
	{
		TimeSliderArgs.OnPlaybackRangeChanged.ExecuteIfBound(TRange<FFrameNumber>(NewStart, PlaybackRange.GetUpperBound()));
	}
}

void FSequencerTimeSliderController::SetPlaybackRangeEnd(FFrameNumber NewEnd)
{
	TRange<FFrameNumber> PlaybackRange = TimeSliderArgs.PlaybackRange.Get();

	if (NewEnd >= UE::MovieScene::DiscreteInclusiveLower(PlaybackRange))
	{
		TimeSliderArgs.OnPlaybackRangeChanged.ExecuteIfBound(TRange<FFrameNumber>(PlaybackRange.GetLowerBound(), TRangeBound<FFrameNumber>::Exclusive(NewEnd)));
	}
}

void FSequencerTimeSliderController::SetSelectionRangeStart(FFrameNumber NewStart)
{
	TRange<FFrameNumber> SelectionRange = TimeSliderArgs.SelectionRange.Get();

	if (SelectionRange.IsEmpty())
	{
		TimeSliderArgs.OnSelectionRangeChanged.ExecuteIfBound(TRange<FFrameNumber>(NewStart, NewStart + 1));
	}
	else if (NewStart <= UE::MovieScene::DiscreteExclusiveUpper(SelectionRange))
	{
		TimeSliderArgs.OnSelectionRangeChanged.ExecuteIfBound(TRange<FFrameNumber>(NewStart, SelectionRange.GetUpperBound()));
	}
}

void FSequencerTimeSliderController::SetSelectionRangeEnd(FFrameNumber NewEnd)
{
	TRange<FFrameNumber> SelectionRange = TimeSliderArgs.SelectionRange.Get();

	if (SelectionRange.IsEmpty())
	{
		TimeSliderArgs.OnSelectionRangeChanged.ExecuteIfBound(TRange<FFrameNumber>(NewEnd - 1, NewEnd));
	}
	else if (NewEnd >= UE::MovieScene::DiscreteInclusiveLower(SelectionRange))
	{
		TimeSliderArgs.OnSelectionRangeChanged.ExecuteIfBound(TRange<FFrameNumber>(SelectionRange.GetLowerBound(), NewEnd));
	}
}

void FSequencerTimeSliderController::HandleMarkSelection(int32 InMarkIndex)
{
	using namespace UE::Sequencer;

	TSharedPtr<FSequencer> Sequencer = WeakSequencer.Pin();
	FSequencerSelection& SequencerSelection = Sequencer->GetSelection();

	FModifierKeysState ModifierKeys = FSlateApplication::Get().GetModifierKeys();
	const bool bToggleSelection = ModifierKeys.AreModifersDown(EModifierKey::Control);
	const bool bAddToSelection = ModifierKeys.AreModifersDown(EModifierKey::Shift);

	if (!bToggleSelection && !bAddToSelection)
	{
		if (!SequencerSelection.MarkedFrames.IsSelected(InMarkIndex))
		{
			SequencerSelection.Empty();
			SequencerSelection.MarkedFrames.Select(InMarkIndex);
		}
	}
	else if (bAddToSelection || !SequencerSelection.MarkedFrames.IsSelected(InMarkIndex))
	{
		SequencerSelection.MarkedFrames.Select(InMarkIndex);
	}
	else
	{
		SequencerSelection.MarkedFrames.Deselect(InMarkIndex);
	}
}

void FSequencerTimeSliderController::SetMark(FFrameNumber DiffFrame)
{
	for (TMap<int32, FFrameNumber>::TConstIterator It = DragMarkMap.CreateConstIterator(); It; ++It)
	{
		int32 MarkIndex = It.Key();
		FFrameNumber FrameNumber = It.Value() + DiffFrame;
		TimeSliderArgs.OnSetMarkedFrame.ExecuteIfBound(MarkIndex, FrameNumber);
	}
}

void FSequencerTimeSliderController::AddMarkAtFrame(FFrameNumber FrameNumber)
{
	TimeSliderArgs.OnAddMarkedFrame.ExecuteIfBound(FrameNumber);
}

void FSequencerTimeSliderController::DeleteMarkAtIndex(int32 InMarkIndex)
{
	TimeSliderArgs.OnDeleteMarkedFrame.ExecuteIfBound(InMarkIndex);
}

void FSequencerTimeSliderController::DeleteAllMarks()
{
	TimeSliderArgs.OnDeleteAllMarkedFrames.ExecuteIfBound();
}
	

#undef LOCTEXT_NAMESPACE
