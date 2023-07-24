// Copyright Epic Games, Inc. All Rights Reserved.

#include "CurveEditorDragOperation_Pan.h"

#include "CurveEditor.h"
#include "CurveEditorScreenSpace.h"
#include "ICurveEditorBounds.h"
#include "Input/Events.h"
#include "Math/UnrealMathUtility.h"
#include "SCurveEditorPanel.h"
#include "SCurveEditorView.h"

FCurveEditorDragOperation_PanView::FCurveEditorDragOperation_PanView(FCurveEditor* InCurveEditor, TSharedPtr<SCurveEditorView> InView)
	: CurveEditor(InCurveEditor)
	, View(InView)
	, bIsDragging(false)
{}

void FCurveEditorDragOperation_PanView::OnBeginDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	FCurveEditorScreenSpace ViewSpace = View->GetViewSpace();

	InitialInputMin = ViewSpace.GetInputMin();
	InitialInputMax = ViewSpace.GetInputMax();
	InitialOutputMin = ViewSpace.GetOutputMin();
	InitialOutputMax = ViewSpace.GetOutputMax();

	bIsDragging = true;

	SnappingState.Reset();
}

void FCurveEditorDragOperation_PanView::OnDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	FVector2D PixelDelta = CurveEditor->GetAxisSnap().GetSnappedPosition(InitialPosition, LastMousePosition,CurrentPosition, MouseEvent, SnappingState, true) - InitialPosition;
	LastMousePosition = CurrentPosition;

	FCurveEditorScreenSpace ViewSpace = View->GetViewSpace();

	double InputMin = InitialInputMin - PixelDelta.X / ViewSpace.PixelsPerInput();
	double InputMax = InitialInputMax - PixelDelta.X / ViewSpace.PixelsPerInput();

	double OutputMin = InitialOutputMin + PixelDelta.Y / ViewSpace.PixelsPerOutput();
	double OutputMax = InitialOutputMax + PixelDelta.Y / ViewSpace.PixelsPerOutput();

	CurveEditor->GetBounds().SetInputBounds(InputMin, InputMax);
	View->SetOutputBounds(OutputMin, OutputMax);
}

FReply FCurveEditorDragOperation_PanView::OnMouseWheel(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	if (bIsDragging)
	{
		FCurveEditorScreenSpace ViewSpace = View->GetViewSpace();
		float ZoomDelta = 1.f - FMath::Clamp(0.1f * MouseEvent.GetWheelDelta(), -0.9f, 0.9f);

		double CurrentTime = ViewSpace.ScreenToSeconds(CurrentPosition.X);
		double CurrentValue = ViewSpace.ScreenToValue(CurrentPosition.Y);

		View->ZoomAround(FVector2D(ZoomDelta, ZoomDelta), CurrentTime, CurrentValue);

		// Adjust the stored initial bounds by the zoom delta so delta calculations work properly
		InitialInputMin = CurrentTime - (CurrentTime - InitialInputMin) * ZoomDelta;
		InitialInputMax = CurrentTime + (InitialInputMax - CurrentTime) * ZoomDelta;
		InitialOutputMin = CurrentValue - (CurrentValue - InitialOutputMin) * ZoomDelta;
		InitialOutputMax = CurrentValue + (InitialOutputMax - CurrentValue) * ZoomDelta;

		return FReply::Handled();
	}
	return FReply::Unhandled();
}


FCurveEditorDragOperation_PanInput::FCurveEditorDragOperation_PanInput(FCurveEditor* InCurveEditor)
	: CurveEditor(InCurveEditor)
{}

void FCurveEditorDragOperation_PanInput::OnBeginDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	FCurveEditorScreenSpaceH InputSpace = CurveEditor->GetPanelInputSpace();
	InitialInputMin = InputSpace.GetInputMin();
	InitialInputMax = InputSpace.GetInputMax();
	SnappingState.Reset();
	LastMousePosition = CurrentPosition;

}

void FCurveEditorDragOperation_PanInput::OnDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	FVector2D PixelDelta = CurveEditor->GetAxisSnap().GetSnappedPosition(InitialPosition, LastMousePosition, CurrentPosition, MouseEvent, SnappingState, true) - InitialPosition;
	LastMousePosition = CurrentPosition;

	FCurveEditorScreenSpaceH InputSpace = CurveEditor->GetPanelInputSpace();

	double InputMin = InitialInputMin - PixelDelta.X / InputSpace.PixelsPerInput();
	double InputMax = InitialInputMax - PixelDelta.X / InputSpace.PixelsPerInput();

	CurveEditor->GetBounds().SetInputBounds(InputMin, InputMax);

	CurveEditor->GetPanel()->ScrollBy(-MouseEvent.GetCursorDelta().Y);
}