// Copyright Epic Games, Inc. All Rights Reserved.

#include "DragOperations/CurveEditorDragOperation_Marquee.h"

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "CurveEditor.h"
#include "CurveEditorSelection.h"
#include "CurveEditorTypes.h"
#include "Curves/KeyHandle.h"
#include "HAL/PlatformCrt.h"
#include "Input/Events.h"
#include "Layout/Geometry.h"
#include "Math/UnrealMathUtility.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateLayoutTransform.h"
#include "SCurveEditorPanel.h"
#include "SCurveEditorView.h"
#include "Styling/AppStyle.h"
#include "Templates/SharedPointer.h"
#include "Templates/Tuple.h"

FCurveEditorDragOperation_Marquee::FCurveEditorDragOperation_Marquee(FCurveEditor* InCurveEditor)
	: LockedToView(nullptr)
{
	CurveEditor = InCurveEditor;
}

FCurveEditorDragOperation_Marquee::FCurveEditorDragOperation_Marquee(FCurveEditor* InCurveEditor, SCurveEditorView* InLockedToView)
	: LockedToView(InLockedToView)
{
	CurveEditor = InCurveEditor;
}

void FCurveEditorDragOperation_Marquee::OnBeginDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	RealInitialPosition = CurrentPosition;

	Marquee = FSlateRect(
		FMath::Min(RealInitialPosition.X, CurrentPosition.X),
		FMath::Min(RealInitialPosition.Y, CurrentPosition.Y),
		FMath::Max(RealInitialPosition.X, CurrentPosition.X),
		FMath::Max(RealInitialPosition.Y, CurrentPosition.Y)
		);
}

void FCurveEditorDragOperation_Marquee::OnDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	Marquee = FSlateRect(
		FMath::Min(RealInitialPosition.X, CurrentPosition.X),
		FMath::Min(RealInitialPosition.Y, CurrentPosition.Y),
		FMath::Max(RealInitialPosition.X, CurrentPosition.X),
		FMath::Max(RealInitialPosition.Y, CurrentPosition.Y)
		);
}

void FCurveEditorDragOperation_Marquee::OnEndDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	TArray<FCurvePointHandle> AllPoints;

	if (LockedToView)
	{
		if (!LockedToView->GetPointsWithinWidgetRange(Marquee, &AllPoints))
		{
			LockedToView->GetCurveWithinWidgetRange(Marquee, &AllPoints);
		}
	}
	else
	{
		TSharedPtr<SCurveEditorPanel> CurveEditorPanel = CurveEditor->GetPanel();

		FGeometry ViewContainerGeometry = CurveEditorPanel->GetViewContainerGeometry();
		FSlateLayoutTransform InverseContainerTransform = ViewContainerGeometry.GetAccumulatedLayoutTransform().Inverse();
		for (TSharedPtr<SCurveEditorView> View : CurveEditorPanel->GetViews())
		{
			const FGeometry& LocalGeometry = View->GetCachedGeometry();
			FSlateLayoutTransform ContainerToView = InverseContainerTransform.Concatenate(LocalGeometry.GetAccumulatedLayoutTransform()).Inverse();

			FSlateRect UnclippedLocalMarquee = FSlateRect(ContainerToView.TransformPoint(Marquee.GetTopLeft2f()), ContainerToView.TransformPoint(Marquee.GetBottomRight2f()));
			FSlateRect ClippedLocalMarquee = UnclippedLocalMarquee.IntersectionWith(FSlateRect(FVector2D(0.f,0.f), LocalGeometry.GetLocalSize()));

			if (ClippedLocalMarquee.IsValid() && !ClippedLocalMarquee.IsEmpty())
			{
				if (!View->GetPointsWithinWidgetRange(ClippedLocalMarquee, &AllPoints))
				{
					View->GetCurveWithinWidgetRange(ClippedLocalMarquee, &AllPoints);
				}
			}
		}
	}

	const bool bIsShiftDown = MouseEvent.IsShiftDown();
	const bool bIsAltDown = MouseEvent.IsAltDown();
	const bool bIsControlDown = MouseEvent.IsControlDown();

	if (!bIsShiftDown && !bIsAltDown && !bIsControlDown)
	{
		CurveEditor->Selection.Clear();
	}

	// If there are any points to be selected, prefer selecting points over tangents
	bool bPreferPointSelection = false;
	for (const FCurvePointHandle& Point : AllPoints)
	{
		if (Point.PointType == ECurvePointType::Key)
		{
			bPreferPointSelection = true;
			break;
		}
	}

	// When adding to the existing selection, ensure only either points or tangents are selected
	TArray<FCurvePointHandle> CurvePointsToRemove;
	if (bIsShiftDown)
	{
		for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : CurveEditor->GetSelection().GetAll())
		{
			for (FKeyHandle Handle : Pair.Value.AsArray())
			{
				ECurvePointType PointType = Pair.Value.PointType(Handle);

				if (bPreferPointSelection)
				{
					// If selecting points, deselect tangen handles (ie. anything that's not a point/key)
					if (PointType != ECurvePointType::Key)
					{
						CurvePointsToRemove.Add(FCurvePointHandle(Pair.Key, PointType, Handle));
					}
				}
				else
				{
					// Otherwise when selecting tangent handles, deselect anything that's a key
					if (PointType == ECurvePointType::Key)
					{
						CurvePointsToRemove.Add(FCurvePointHandle(Pair.Key, PointType, Handle));
					}
				}
			}
		}
	}

	for (const FCurvePointHandle& Point : CurvePointsToRemove)
	{
		CurveEditor->Selection.Remove(Point);
	}

	// Now that we've gathered the overlapping points, perform the relevant selection
	for (const FCurvePointHandle& Point : AllPoints)
	{
		if (bIsAltDown)
		{
			CurveEditor->Selection.Remove(Point);
		}
		else if (bIsControlDown)
		{
			CurveEditor->Selection.Toggle(Point);
		}
		else
		{
			if (bPreferPointSelection)
			{
				if (Point.PointType == ECurvePointType::Key)
				{
					CurveEditor->Selection.Add(Point);
				}
			}
			else
			{
				CurveEditor->Selection.Add(Point);
			}
		}
	}
}

void FCurveEditorDragOperation_Marquee::OnPaint(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 PaintOnLayerId)
{
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		PaintOnLayerId,
		AllottedGeometry.ToPaintGeometry(Marquee.GetBottomRight() - Marquee.GetTopLeft(), FSlateLayoutTransform(Marquee.GetTopLeft())),
		FAppStyle::GetBrush(TEXT("MarqueeSelection"))
		);
}
