// Copyright Epic Games, Inc. All Rights Reserved.

#include "CurveEditorDragOperation_Tangent.h"

#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "CoreTypes.h"
#include "CurveEditor.h"
#include "CurveEditorHelpers.h"
#include "CurveEditorScreenSpace.h"
#include "CurveEditorSelection.h"
#include "CurveModel.h"
#include "Curves/RichCurve.h"
#include "HAL/PlatformCrt.h"
#include "Input/Events.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/AssertionMacros.h"
#include "Misc/EnumClassFlags.h"
#include "SCurveEditorView.h"
#include "ScopedTransaction.h"
#include "Templates/Tuple.h"
#include "Templates/UniquePtr.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/UnrealType.h"

namespace CurveEditorDragOperation
{
	/** If they're within this many pixels of crossing over the straight up/down boundary, we clamp the handles. */
	constexpr float TangentCrossoverThresholdPx = 1.f;
}

void FCurveEditorDragOperation_Tangent::OnInitialize(FCurveEditor* InCurveEditor, const TOptional<FCurvePointHandle>& InCardinalPoint)
{
	CurveEditor = InCurveEditor;
}

void FCurveEditorDragOperation_Tangent::OnBeginDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	int32 NumKeys = CurveEditor->GetSelection().Count();

	FText Description = FText::Format(NSLOCTEXT("CurveEditor", "DragTangentsFormat", "Drag {0}|plural(one=Tangent, other=Tangents)"), NumKeys);
	Transaction = MakeUnique<FScopedTransaction>(Description);
	CurveEditor->SuppressBoundTransformUpdates(true);

	KeysByCurve.Reset();
	for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : CurveEditor->GetSelection().GetAll())
	{
		FCurveModelID CurveID = Pair.Key;
		FCurveModel* Curve    = CurveEditor->FindCurve(CurveID);

		if (ensureAlways(Curve))
		{
			Curve->Modify();

			TArrayView<const FKeyHandle> Handles = Pair.Value.AsArray();

			FKeyData& KeyData = KeysByCurve.Emplace_GetRef(CurveID);
			KeyData.Handles = TArray<FKeyHandle>(Handles.GetData(), Handles.Num());

			for (FKeyHandle Handle : Handles)
			{
				KeyData.PointTypes.Add(Pair.Value.PointType(Handle));
			}

			KeyData.Attributes.SetNum(KeyData.Handles.Num());
			Curve->GetKeyAttributes(KeyData.Handles, KeyData.Attributes);
		}
	}
}

void FCurveEditorDragOperation_Tangent::OnDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	FVector2D PixelDelta = CurrentPosition - InitialPosition;

	TArray<FKeyAttributes> NewKeyAttributesScratch;
	TArray<FKeyPosition> KeyPositions;

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
		const float DisplayRatio = (CurveSpace.PixelsPerOutput() / CurveSpace.PixelsPerInput());

		NewKeyAttributesScratch.Reset();
		NewKeyAttributesScratch.Reserve(KeyData.Attributes.Num());
		KeyPositions.SetNumUninitialized(KeyData.Attributes.Num());
		Curve->GetKeyPositions(KeyData.Handles, KeyPositions);

		for (int32 i = 0; i < KeyData.Handles.Num(); ++i)
		{
			const FKeyAttributes Attributes = KeyData.Attributes[i];

			FKeyAttributes NewAttributes;

			if (Attributes.HasArriveTangent() && EnumHasAnyFlags(KeyData.PointTypes[i], ECurvePointType::ArriveTangent))
			{
				const float ArriveTangent = Attributes.GetArriveTangent();
				if (Attributes.HasTangentWeightMode() && Attributes.HasArriveTangentWeight() &&
					(Attributes.GetTangentWeightMode() == RCTWM_WeightedBoth || Attributes.GetTangentWeightMode() == RCTWM_WeightedArrive))
				{
					FVector2D TangentOffset = CurveEditor::ComputeScreenSpaceTangentOffset(CurveSpace, ArriveTangent, -Attributes.GetArriveTangentWeight());
					TangentOffset += PixelDelta;

					if (MouseEvent.IsShiftDown())
					{
						TangentOffset = RoundTrajectory(TangentOffset);
					}

					// We prevent the handle from crossing over the 0 point... the code handles it but it creates an ugly pop in your curve and it lets your Arrive tangents become Leave tangents which defeats the point.
					TangentOffset.X = FMath::Min(TangentOffset.X, -CurveEditorDragOperation::TangentCrossoverThresholdPx);

					float Tangent, Weight;
					CurveEditor::TangentAndWeightFromOffset(CurveSpace, TangentOffset, Tangent, Weight);

					NewAttributes.SetArriveTangent(Tangent);
					NewAttributes.SetArriveTangentWeight(Weight);
				}
				else
				{
					const float PixelLength = 60.0f;
					FVector2D TangentOffset = CurveEditor::GetVectorFromSlopeAndLength(ArriveTangent * -DisplayRatio, -PixelLength);
					TangentOffset += PixelDelta;

					if (MouseEvent.IsShiftDown())
					{
						TangentOffset = RoundTrajectory(TangentOffset);
					}

					// We prevent the handle from crossing over the 0 point... the code handles it but it creates an ugly pop in your curve and it lets your Arrive tangents become Leave tangents which defeats the point.
					TangentOffset.X = FMath::Min(TangentOffset.X, -CurveEditorDragOperation::TangentCrossoverThresholdPx);

					const float Tangent = (-TangentOffset.Y / TangentOffset.X) / DisplayRatio;

					NewAttributes.SetArriveTangent(Tangent);
				}
			}

			if (Attributes.HasLeaveTangent() && EnumHasAnyFlags(KeyData.PointTypes[i], ECurvePointType::LeaveTangent))
			{
				const float LeaveTangent = Attributes.GetLeaveTangent();

				if (Attributes.HasTangentWeightMode() && Attributes.HasLeaveTangentWeight() &&
					(Attributes.GetTangentWeightMode() == RCTWM_WeightedBoth || Attributes.GetTangentWeightMode() == RCTWM_WeightedLeave))
				{
					FVector2D TangentOffset = CurveEditor::ComputeScreenSpaceTangentOffset(CurveSpace, LeaveTangent, Attributes.GetLeaveTangentWeight());
					TangentOffset += PixelDelta;

					if (MouseEvent.IsShiftDown())
					{
						TangentOffset = RoundTrajectory(TangentOffset);
					}

					// We prevent the handle from crossing over the 0 point... the code handles it but it creates an ugly pop in your curve and it lets your Arrive tangents become Leave tangents which defeats the point.
					TangentOffset.X = FMath::Max(TangentOffset.X, CurveEditorDragOperation::TangentCrossoverThresholdPx);

					float Tangent, Weight;
					CurveEditor::TangentAndWeightFromOffset(CurveSpace, TangentOffset, Tangent, Weight);

					NewAttributes.SetLeaveTangent(Tangent);
					NewAttributes.SetLeaveTangentWeight(Weight);
				}
				else
				{
					const float PixelLength = 60.0f;
					FVector2D TangentOffset = CurveEditor::GetVectorFromSlopeAndLength(LeaveTangent * -DisplayRatio, PixelLength);
					TangentOffset += PixelDelta;

					if (MouseEvent.IsShiftDown())
					{
						TangentOffset = RoundTrajectory(TangentOffset);
					}

					// We prevent the handle from crossing over the 0 point... the code handles it but it creates an ugly pop in your curve and it lets your Arrive tangents become Leave tangents which defeats the point.
					TangentOffset.X = FMath::Max(TangentOffset.X, CurveEditorDragOperation::TangentCrossoverThresholdPx);

					const float Tangent = (-TangentOffset.Y / TangentOffset.X) / DisplayRatio;

					NewAttributes.SetLeaveTangent(Tangent);
				}
			}
				
			NewKeyAttributesScratch.Add(NewAttributes);
		}
		Curve->SetKeyAttributes(KeyData.Handles, NewKeyAttributesScratch, EPropertyChangeType::Interactive);
		KeyData.LastDraggedAttributes = NewKeyAttributesScratch;
	}
}

void FCurveEditorDragOperation_Tangent::OnCancelDrag()
{
	ICurveEditorKeyDragOperation::OnCancelDrag();

	for (const FKeyData& KeyData : KeysByCurve)
	{
		if (FCurveModel* Curve = CurveEditor->FindCurve(KeyData.CurveID))
		{
			Curve->SetKeyAttributes(KeyData.Handles, KeyData.Attributes, EPropertyChangeType::ValueSet);
		}
	}

	CurveEditor->SuppressBoundTransformUpdates(false);
}

void FCurveEditorDragOperation_Tangent::OnEndDrag(FVector2D InitialPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent)
{
	ICurveEditorKeyDragOperation::OnEndDrag(InitialPosition, CurrentPosition, MouseEvent);

	for (const FKeyData& KeyData : KeysByCurve)
	{
		if (FCurveModel* Curve = CurveEditor->FindCurve(KeyData.CurveID))
		{
			if (KeyData.LastDraggedAttributes.IsSet())
			{
				Curve->SetKeyAttributes(KeyData.Handles, KeyData.LastDraggedAttributes.GetValue(), EPropertyChangeType::ValueSet);
			}
		}
	}
	
	CurveEditor->SuppressBoundTransformUpdates(false);
}

FVector2D FCurveEditorDragOperation_Tangent::RoundTrajectory(FVector2D Delta)
{
	float Distance = Delta.Size();
	float Theta = FMath::Atan2(Delta.Y, Delta.X) + PI/2;

	float RoundTo = PI / 4;
	Theta = FMath::RoundToInt(Theta / RoundTo) * RoundTo - PI/2;
	return FVector2D(Distance * FMath::Cos(Theta), Distance * FMath::Sin(Theta));
}