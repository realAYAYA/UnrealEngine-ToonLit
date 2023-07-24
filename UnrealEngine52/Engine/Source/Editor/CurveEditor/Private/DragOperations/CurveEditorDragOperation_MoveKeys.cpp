// Copyright Epic Games, Inc. All Rights Reserved.

#include "DragOperations/CurveEditorDragOperation_MoveKeys.h"

#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "CoreTypes.h"
#include "CurveEditor.h"
#include "CurveEditorScreenSpace.h"
#include "CurveEditorSelection.h"
#include "CurveModel.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/NumericLimits.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/AssertionMacros.h"
#include "SCurveEditorView.h"
#include "ScopedTransaction.h"
#include "Templates/Tuple.h"
#include "Templates/UniquePtr.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/UnrealType.h"

struct FPointerEvent;

void FCurveEditorDragOperation_MoveKeys::OnInitialize(FCurveEditor* InCurveEditor, const TOptional<FCurvePointHandle>& InCardinalPoint)
{
	CurveEditor = InCurveEditor;
	CardinalPoint = InCardinalPoint;
}

void FCurveEditorDragOperation_MoveKeys::OnBeginDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	int32 NumKeys = CurveEditor->GetSelection().Count();
	Transaction = MakeUnique<FScopedTransaction>(FText::Format(NSLOCTEXT("CurveEditor", "MoveKeysFormat", "Move {0}|plural(one=Key, other=Keys)"), NumKeys));

	KeysByCurve.Reset();
	CurveEditor->SuppressBoundTransformUpdates(true);

	LastMousePosition = CurrentPosition;
	for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : CurveEditor->GetSelection().GetAll())
	{
		FCurveModelID CurveID = Pair.Key;
		FCurveModel*  Curve   = CurveEditor->FindCurve(CurveID);

		if (ensureAlways(Curve))
		{
			Curve->Modify();

			TArrayView<const FKeyHandle> Handles = Pair.Value.AsArray();

			FKeyData& KeyData = KeysByCurve.Emplace_GetRef(CurveID);
			KeyData.Handles = TArray<FKeyHandle>(Handles.GetData(), Handles.Num());

			KeyData.StartKeyPositions.SetNumZeroed(KeyData.Handles.Num());
			Curve->GetKeyPositions(KeyData.Handles, KeyData.StartKeyPositions);
			KeyData.LastDraggedKeyPositions = KeyData.StartKeyPositions;
		}
	}

	SnappingState.Reset();
}

void FCurveEditorDragOperation_MoveKeys::OnDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	TArray<FKeyPosition> NewKeyPositionScratch;
	FVector2D MousePosition = CurveEditor->GetAxisSnap().GetSnappedPosition(InitialPosition,CurrentPosition, LastMousePosition, MouseEvent, SnappingState);
	LastMousePosition = CurrentPosition;

	for (FKeyData& KeyData : KeysByCurve)
	{
		const SCurveEditorView* View = CurveEditor->FindFirstInteractiveView(KeyData.CurveID);
		if (!View)
		{
			continue;
		}

		FCurveModel* Curve = CurveEditor->FindCurve(KeyData.CurveID);
		if (!ensureAlways(Curve))
		{
			continue;
		}

		FCurveEditorScreenSpace CurveSpace = View->GetCurveSpace(KeyData.CurveID);

		double DeltaInput = (MousePosition.X - InitialPosition.X) / CurveSpace.PixelsPerInput();
		double DeltaOutput = -(MousePosition.Y - InitialPosition.Y) / CurveSpace.PixelsPerOutput();

		NewKeyPositionScratch.Reset();
		NewKeyPositionScratch.Reserve(KeyData.StartKeyPositions.Num());

		FCurveSnapMetrics SnapMetrics = CurveEditor->GetCurveSnapMetrics(KeyData.CurveID);

		if (CardinalPoint.IsSet())
		{
			for (int KeyIndex = 0; KeyIndex < KeyData.StartKeyPositions.Num(); ++KeyIndex)
			{
				FKeyHandle KeyHandle = KeyData.Handles[KeyIndex];
				if (CardinalPoint->KeyHandle == KeyHandle)
				{
					FKeyPosition StartPosition = KeyData.StartKeyPositions[KeyIndex];

					if (View->IsTimeSnapEnabled())
					{
						DeltaInput = SnapMetrics.SnapInputSeconds(StartPosition.InputValue + DeltaInput) - StartPosition.InputValue;
					}

					// If view is not absolute, snap based on the key that was grabbed, not all keys individually.
					if (View->IsValueSnapEnabled() && View->ViewTypeID != ECurveEditorViewID::Absolute)
					{
						DeltaOutput = SnapMetrics.SnapOutput(StartPosition.OutputValue + DeltaOutput) - StartPosition.OutputValue;
					}
				}
			}
		}

		for (FKeyPosition StartPosition : KeyData.StartKeyPositions)
		{
			StartPosition.InputValue  += DeltaInput;
			StartPosition.OutputValue += DeltaOutput;

			StartPosition.InputValue = View->IsTimeSnapEnabled() ? SnapMetrics.SnapInputSeconds(StartPosition.InputValue) : StartPosition.InputValue;

			// Snap value keys individually if view mode is absolute.
			if (View->ViewTypeID == ECurveEditorViewID::Absolute)
			{
				StartPosition.OutputValue = View->IsValueSnapEnabled() ? SnapMetrics.SnapOutput(StartPosition.OutputValue) : StartPosition.OutputValue;
			}
			
			NewKeyPositionScratch.Add(StartPosition);
		}

		Curve->SetKeyPositions(KeyData.Handles, NewKeyPositionScratch, EPropertyChangeType::Interactive);

		// Make sure the last dragged key positions are up to date
		Curve->GetKeyPositions(KeyData.Handles, KeyData.LastDraggedKeyPositions);
	}
}

void FCurveEditorDragOperation_MoveKeys::OnCancelDrag()
{
	ICurveEditorKeyDragOperation::OnCancelDrag();

	for (const FKeyData& KeyData : KeysByCurve)
	{
		if (FCurveModel* Curve = CurveEditor->FindCurve(KeyData.CurveID))
		{
			Curve->SetKeyPositions(KeyData.Handles, KeyData.StartKeyPositions, EPropertyChangeType::ValueSet);
		}
	}

	CurveEditor->SuppressBoundTransformUpdates(false);
}

void FCurveEditorDragOperation_MoveKeys::OnEndDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	ICurveEditorKeyDragOperation::OnEndDrag(InitialPosition, CurrentPosition, MouseEvent);

	for (const FKeyData& KeyData : KeysByCurve)
	{
		if (FCurveModel* Curve = CurveEditor->FindCurve(KeyData.CurveID))
		{
			// Gather all the final key time positions
			TArray<FKeyPosition> KeyTimes;
			for (int32 KeyIndex = 0; KeyIndex < KeyData.Handles.Num(); ++KeyIndex)
			{
				const FKeyHandle& KeyHandle = KeyData.Handles[KeyIndex];
				FKeyPosition KeyTime = KeyData.LastDraggedKeyPositions[KeyIndex];
				KeyTimes.Add(KeyTime);
			}

			// For each key time, look for all the keys that match
			TArray<FKeyHandle> KeysToRemove;
			for (const FKeyPosition& KeyTime : KeyTimes)
			{
				TArray<FKeyHandle> KeysInRange;
				Curve->GetKeys(*CurveEditor, KeyTime.InputValue, KeyTime.InputValue, TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), KeysInRange);

				// If there's more than 1 key at this time, remove all but the keys that moved the largest amount
				if (KeysInRange.Num() > 1)
				{
					double MaxDist = TNumericLimits<double>::Min();
					FKeyHandle MaxKeyHandle;
					for (const FKeyHandle& KeyInRange : KeysInRange)
					{
						int32 KeyDataIndex = KeyData.Handles.Find(KeyInRange);
						if (KeyDataIndex != INDEX_NONE)
						{
							double Dist = FMath::Abs(KeyData.StartKeyPositions[KeyDataIndex].InputValue - KeyData.LastDraggedKeyPositions[KeyDataIndex].InputValue);
							if (Dist > MaxDist)
							{
								MaxKeyHandle = KeyInRange;
								MaxDist = Dist;
							}
						}
					}

					for (const FKeyHandle& KeyInRange : KeysInRange)
					{
						if (KeyInRange != MaxKeyHandle)
						{
							KeysToRemove.AddUnique(KeyInRange);
						}
					}
				}
			}

			// Remove any keys that overlap before moving new keys on top
			Curve->RemoveKeys(KeysToRemove);
			// Then, move the keys
			Curve->SetKeyPositions(KeyData.Handles, KeyData.LastDraggedKeyPositions, EPropertyChangeType::ValueSet);
		}
	}

	CurveEditor->SuppressBoundTransformUpdates(false);
}
