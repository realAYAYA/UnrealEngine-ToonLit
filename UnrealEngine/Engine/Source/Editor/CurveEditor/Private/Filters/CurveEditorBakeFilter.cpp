// Copyright Epic Games, Inc. All Rights Reserved.

#include "Filters/CurveEditorBakeFilter.h"

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "CurveDataAbstraction.h"
#include "CurveEditor.h"
#include "CurveEditorSelection.h"
#include "CurveEditorSnapMetrics.h"
#include "CurveEditorTypes.h"
#include "CurveModel.h"
#include "Curves/KeyHandle.h"
#include "Curves/RealCurve.h"
#include "HAL/PlatformCrt.h"
#include "Math/NumericLimits.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/FrameRate.h"
#include "Misc/Optional.h"
#include "Templates/Tuple.h"
#include "Templates/UnrealTemplate.h"

void UCurveEditorBakeFilter::ApplyFilter_Impl(TSharedRef<FCurveEditor> InCurveEditor, const TMap<FCurveModelID, FKeyHandleSet>& InKeysToOperateOn, TMap<FCurveModelID, FKeyHandleSet>& OutKeysToSelect)
{
	TArray<FKeyHandle> KeyHandles;
	TArray<FKeyPosition> SelectedKeyPositions;

	TArray<FKeyPosition> NewKeyPositions;
	TArray<FKeyAttributes> NewKeyAttributes;

	for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : InKeysToOperateOn)
	{
		FCurveModel* Curve = InCurveEditor->FindCurve(Pair.Key);

		if (!Curve)
		{
			continue;
		}

		// Look at our Input Snap rate to determine how far apart the baked keys are.
		FFrameRate BakeRate = InCurveEditor->GetCurveSnapMetrics(Pair.Key).InputSnapRate;

		KeyHandles.Reset(Pair.Value.Num());
		KeyHandles.Append(Pair.Value.AsArray().GetData(), Pair.Value.Num());

		// Get all the selected keys
		SelectedKeyPositions.SetNum(KeyHandles.Num());
		Curve->GetKeyPositions(KeyHandles, SelectedKeyPositions);

		// Find the hull of the range of the selected keys
		double MinKey = TNumericLimits<double>::Max(), MaxKey = TNumericLimits<double>::Lowest();
		for (FKeyPosition Key : SelectedKeyPositions)
		{
			MinKey = FMath::Min(Key.InputValue, MinKey);
			MaxKey = FMath::Max(Key.InputValue, MaxKey);
		}

		// Get all keys that exist between the time range
		KeyHandles.Reset();
		Curve->GetKeys(*InCurveEditor, MinKey, MaxKey, TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), KeyHandles);

		if (KeyHandles.Num() > 1)
		{
			// Determine new times for new keys
			double Interval = bUseFrameBake ? (double)(BakeIntervalInFrames.Value) * BakeRate.AsInterval() : BakeIntervalInSeconds;

			int32 NumKeysToAdd = FMath::FloorToInt((MaxKey - MinKey) / Interval) + 1;

			NewKeyPositions.Reset(NumKeysToAdd);
			NewKeyAttributes.Reset(NumKeysToAdd);

			for (int32 KeyIndex = 1; KeyIndex < NumKeysToAdd; ++KeyIndex)
			{
				double NewTime = MinKey + KeyIndex * Interval;

				// Don't create the last key if it has the same value as the last key. This avoids duplicates of the last
				// key, but improves behavior when the interval doesn't align.
				if (KeyIndex == NumKeysToAdd - 1)
				{
					if (FMath::IsNearlyEqual(NewTime, SelectedKeyPositions.Last().InputValue))
					{
						continue;
					}
				}
				FKeyPosition CurrentKey(NewTime, 0.0);
				if (Curve->Evaluate(CurrentKey.InputValue, CurrentKey.OutputValue))
				{
					NewKeyPositions.Add(CurrentKey);
					NewKeyAttributes.Add(FKeyAttributes().SetInterpMode(RCIM_Linear));
				}
			}

			Curve->Modify();

			// We want to store the newly added ones so we can add them to the users selection to mimic
			// their selection pre-baking.
			TArray<TOptional<FKeyHandle>> NewKeyHandles;
			NewKeyHandles.SetNumUninitialized(NewKeyPositions.Num());

			TArrayView<TOptional<FKeyHandle>> NewKeyHandlesView = MakeArrayView(NewKeyHandles);
			FKeyHandleSet& OutHandleSet = OutKeysToSelect.Add(Pair.Key);

			// Manually add the first and last keys to the out set before we remove them below.
			OutHandleSet.Add(KeyHandles[0], ECurvePointType::Key);
			OutHandleSet.Add(KeyHandles.Last(0), ECurvePointType::Key);

			// We need to leave the first and the last key of the selection alone for two reasons;
			// 1. Undo/Redo works better as selections aren't transacted so they don't handle keys being removed/re-added.
			// by leaving the keys alone, they survive undo/redo operations.
			// 2. With high interval settings, the shape of the curve can change drastically because the next interval would fall right
			// outside the last key. If we remove the keys, then the shape changes in these cases. We avoid putting a new key on the last
			// key (if it exists), to avoid any duplication. This preserves the shape of the curves better in most test cases.
			KeyHandles.RemoveAt(0);
			KeyHandles.RemoveAt(KeyHandles.Num() - 1);

			// Remove all the old in-between keys and add the new ones
			Curve->RemoveKeys(KeyHandles);
			Curve->AddKeys(NewKeyPositions, NewKeyAttributes, &NewKeyHandlesView);

			for (const TOptional<FKeyHandle>& Handle : NewKeyHandlesView)
			{
				if (Handle.IsSet())
				{
					OutHandleSet.Add(Handle.GetValue(), ECurvePointType::Key);
				}
			}
		}
	}
}
