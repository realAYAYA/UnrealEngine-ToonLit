// Copyright Epic Games, Inc. All Rights Reserved.

#include "CurveEditorDragOperation_Zoom.h"

#include "CurveEditor.h"
#include "CurveEditorScreenSpace.h"
#include "ICurveEditorBounds.h"
#include "Input/Events.h"
#include "Math/UnrealMathUtility.h"
#include "SCurveEditorView.h"

FCurveEditorDragOperation_Zoom::FCurveEditorDragOperation_Zoom(FCurveEditor* InCurveEditor, TSharedPtr<SCurveEditorView> InOptionalView)
	: CurveEditor(InCurveEditor)
	, OptionalView(InOptionalView)
{}

void FCurveEditorDragOperation_Zoom::OnBeginDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	FCurveEditorScreenSpaceH InputSpace  = OptionalView ? OptionalView->GetViewSpace() : CurveEditor->GetPanelInputSpace();

	ZoomFactor.X = InitialPosition.X / InputSpace.GetPhysicalWidth();
	OriginalInputRange = (InputSpace.GetInputMax() - InputSpace.GetInputMin());
	ZoomOriginX = InputSpace.GetInputMin() + OriginalInputRange * ZoomFactor.X;

	if (OptionalView)
	{
		FCurveEditorScreenSpaceV OutputSpace = OptionalView->GetViewSpace();

		ZoomFactor.Y = InitialPosition.Y / OutputSpace.GetPhysicalHeight();
		OriginalOutputRange = (OutputSpace.GetOutputMax() - OutputSpace.GetOutputMin());
		ZoomOriginY = OutputSpace.GetOutputMin() + OriginalOutputRange * (1.f - ZoomFactor.Y);
	}
}

void FCurveEditorDragOperation_Zoom::OnDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	FVector2D PixelDelta = CurrentPosition - InitialPosition;

	double ClampRange = 1e9f;

	// Zoom input range
	FCurveEditorScreenSpaceH InputSpace  = OptionalView ? OptionalView->GetViewSpace() : CurveEditor->GetPanelInputSpace();

	double DiffX = double(PixelDelta.X) / (InputSpace.GetPhysicalWidth() / OriginalInputRange);

	// This flips the horizontal zoom to match existing DCC tools
	double NewInputRange = OriginalInputRange - DiffX;
	double InputMin = FMath::Clamp<double>(ZoomOriginX - NewInputRange * ZoomFactor.X, -ClampRange, ClampRange);
	double InputMax = FMath::Clamp<double>(ZoomOriginX + NewInputRange * (1.f - ZoomFactor.X), InputMin, ClampRange);

	CurveEditor->GetBounds().SetInputBounds(InputMin, InputMax);

	// Zoom output range
	if (OptionalView.IsValid())
	{
		FCurveEditorScreenSpaceV ViewSpace = OptionalView->GetViewSpace();

		double DiffY = double(PixelDelta.Y) / (ViewSpace.GetPhysicalHeight() / OriginalOutputRange);
		double NewOutputRange = OriginalOutputRange + DiffY;

		// If they're holding a shift they can scale on both axis at once, non-proportionally
		if (!MouseEvent.IsShiftDown())
		{
			// By default, do proportional zoom
			NewOutputRange = (NewInputRange / OriginalInputRange) * OriginalOutputRange;
		}

		double OutputMin = FMath::Clamp<double>(ZoomOriginY - NewOutputRange * (1.f - ZoomFactor.Y), -ClampRange, ClampRange);
		double OutputMax = FMath::Clamp<double>(ZoomOriginY + NewOutputRange * (ZoomFactor.Y), OutputMin, ClampRange);

		OptionalView->SetOutputBounds(OutputMin, OutputMax);
	}
}
