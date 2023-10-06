// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CurveEditorSnapMetrics.h"
#include "ICurveEditorDragOperation.h"
#include "Input/Reply.h"
#include "Math/Vector2D.h"
#include "Templates/SharedPointer.h"

class FCurveEditor;
struct FPointerEvent;
class ITimeSliderController;

class FCurveEditorDragOperation_ScrubTime : public ICurveEditorDragOperation
{
public:
	FCurveEditorDragOperation_ScrubTime(FCurveEditor* CurveEditor);

	virtual void OnBeginDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent) override;
	virtual void OnDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent) override;
	virtual void OnEndDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent) override;

	virtual void OnCancelDrag() override;

private:

	FCurveEditor* CurveEditor;
	FCurveEditorAxisSnap::FSnapState SnappingState;
	FVector2D LastMousePosition;
	double InitialTime;
};