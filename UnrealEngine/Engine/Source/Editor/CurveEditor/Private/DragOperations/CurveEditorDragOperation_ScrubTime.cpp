// Copyright Epic Games, Inc. All Rights Reserved.

#include "CurveEditorDragOperation_ScrubTime.h"

#include "CurveEditor.h"
#include "CurveEditorScreenSpace.h"
#include "ICurveEditorBounds.h"
#include "Input/Events.h"
#include "Math/UnrealMathUtility.h"
#include "SCurveEditorPanel.h"
#include "SCurveEditorView.h"

FCurveEditorDragOperation_ScrubTime::FCurveEditorDragOperation_ScrubTime(FCurveEditor* InCurveEditor)
	: CurveEditor(InCurveEditor) 
{}

void FCurveEditorDragOperation_ScrubTime::OnBeginDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	TSharedPtr<ITimeSliderController> TimeSliderController = CurveEditor->GetTimeSliderController();
	if (TimeSliderController)
	{
		SnappingState.Reset();
		LastMousePosition = CurrentPosition;
		const FFrameRate TickResolution = TimeSliderController->GetTickResolution();

		InitialTime = TickResolution.AsSeconds(TimeSliderController->GetScrubPosition());
		TimeSliderController->SetPlaybackStatus(ETimeSliderPlaybackStatus::Scrubbing);
	}
}

void FCurveEditorDragOperation_ScrubTime::OnDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	TSharedPtr<ITimeSliderController> TimeSliderController = CurveEditor->GetTimeSliderController();
	if (TimeSliderController)
	{
		FVector2D PixelDelta = CurveEditor->GetAxisSnap().GetSnappedPosition(InitialPosition, LastMousePosition, CurrentPosition, MouseEvent, SnappingState, true) - InitialPosition;
		LastMousePosition = CurrentPosition;

		FCurveEditorScreenSpaceH InputSpace = CurveEditor->GetPanelInputSpace();
		const double InitialMouseTime = InputSpace.ScreenToSeconds(InitialPosition.X);
		const double CurrentMouseTime = InputSpace.ScreenToSeconds(LastMousePosition.X);
		const double DiffSeconds = CurrentMouseTime - InitialMouseTime;
		const FFrameRate TickResolution = TimeSliderController->GetTickResolution();
		double TimeToSet = (InitialTime + DiffSeconds);
		if (CurveEditor->InputSnapEnabledAttribute.Get())
		{
			FCurveSnapMetrics SnapMetrics;
			SnapMetrics.bSnapInputValues = true;
			SnapMetrics.InputSnapRate = CurveEditor->InputSnapRateAttribute.Get();
			TimeToSet = SnapMetrics.SnapInputSeconds(TimeToSet);
		}
		TimeSliderController->SetScrubPosition((TimeToSet)*TickResolution, /*bEvaluate*/ true);
	}
}

void FCurveEditorDragOperation_ScrubTime::OnEndDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	TSharedPtr<ITimeSliderController> TimeSliderController = CurveEditor->GetTimeSliderController();
	if (TimeSliderController)
	{
		TimeSliderController->SetPlaybackStatus(ETimeSliderPlaybackStatus::Stopped);
	}
}

void FCurveEditorDragOperation_ScrubTime::OnCancelDrag()
{
	TSharedPtr<ITimeSliderController> TimeSliderController = CurveEditor->GetTimeSliderController();
	if (TimeSliderController)
	{
		TimeSliderController->SetPlaybackStatus(ETimeSliderPlaybackStatus::Stopped);
	}
}