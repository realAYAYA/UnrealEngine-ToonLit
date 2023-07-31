// Copyright Epic Games, Inc. All Rights Reserved.

#include "SCurveEditorView.h"

#include "Containers/Map.h"
#include "CurveDataAbstraction.h"
#include "CurveDrawInfo.h"
#include "CurveEditor.h"
#include "CurveEditorHelpers.h"
#include "CurveEditorScreenSpace.h"
#include "CurveEditorSelection.h"
#include "CurveEditorSettings.h"
#include "CurveModel.h"
#include "Curves/KeyHandle.h"
#include "Curves/RichCurve.h"
#include "HAL/PlatformCrt.h"
#include "ICurveEditorBounds.h"
#include "Layout/Geometry.h"
#include "Math/Color.h"
#include "Misc/AssertionMacros.h"
#include "SCurveEditorPanel.h"
#include "Templates/Tuple.h"
#include "Templates/UnrealTemplate.h"

SCurveEditorView::SCurveEditorView()
	: bPinned(0)
	, bInteractive(1)
	, bFixedOutputBounds(0)
	, bAutoSize(1)
	, bAllowEmpty(0)
{}

FVector2D SCurveEditorView::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	FVector2D ContentDesiredSize = SCompoundWidget::ComputeDesiredSize(LayoutScaleMultiplier);
	return FVector2D(ContentDesiredSize.X, FixedHeight.Get(ContentDesiredSize.Y));
}

void SCurveEditorView::GetInputBounds(double& OutInputMin, double& OutInputMax) const
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	if (CurveEditor)
	{
		CurveEditor->GetBounds().GetInputBounds(OutInputMin, OutInputMax);

		// This code assumes no scaling between the container and the view (which is a pretty safe assumption to make)
		const FGeometry& ViewGeometry      = GetCachedGeometry();
		const FGeometry ContainerGeometry = CurveEditor->GetPanel().IsValid() ? CurveEditor->GetPanel()->GetViewContainerGeometry() : ViewGeometry;

		const float ContainerWidth = ContainerGeometry.GetLocalSize().X;
		const float ViewWidth      = ViewGeometry.GetLocalSize().X;

		if (ViewWidth > 0.f)
		{
			const float LeftPixelCrop = ViewGeometry.LocalToAbsolute(FVector2D(0.f, 0.f)).X - ContainerGeometry.LocalToAbsolute(FVector2D(0.f, 0.f)).X;
			const float RightPixelCrop = ContainerGeometry.LocalToAbsolute(FVector2D(ContainerWidth, 0.f)).X - ViewGeometry.LocalToAbsolute(FVector2D(ViewWidth, 0.f)).X;

			const double ContainerInputPerPixel = (OutInputMax - OutInputMin) / ContainerWidth;

			// Offset by the total range first
			OutInputMin += ContainerInputPerPixel * LeftPixelCrop;
			OutInputMax -= ContainerInputPerPixel * RightPixelCrop;
		}
	}
}

FCurveEditorScreenSpace SCurveEditorView::GetViewSpace() const
{
	double InputMin = 0.0, InputMax = 1.0;
	GetInputBounds(InputMin, InputMax);

	return FCurveEditorScreenSpace(GetCachedGeometry().GetLocalSize(), InputMin, InputMax, OutputMin, OutputMax);
}

void SCurveEditorView::AddCurve(FCurveModelID CurveID)
{
	CurveInfoByID.Add(CurveID, FCurveInfo{CurveInfoByID.Num()});
	OnCurveListChanged();
}

void SCurveEditorView::RemoveCurve(FCurveModelID CurveID)
{
	if (FCurveInfo* InfoToRemove = CurveInfoByID.Find(CurveID))
	{
		const int32 CurveIndex = InfoToRemove->CurveIndex;
		InfoToRemove = nullptr;

		CurveInfoByID.Remove(CurveID);

		for (TTuple<FCurveModelID, FCurveInfo>& Info : CurveInfoByID)
		{
			if (Info.Value.CurveIndex > CurveIndex)
			{
				--Info.Value.CurveIndex;
			}
		}

		OnCurveListChanged();
	}
}

void SCurveEditorView::SetOutputBounds(double InOutputMin, double InOutputMax)
{
	if (!bFixedOutputBounds)
	{
		OutputMin = InOutputMin;
		OutputMax = InOutputMax;
	}
}

void SCurveEditorView::Zoom(const FVector2D& Amount)
{
	FCurveEditorScreenSpace ViewSpace = GetViewSpace();

	const double InputOrigin  = (ViewSpace.GetInputMax()  - ViewSpace.GetInputMin())  * 0.5;
	const double OutputOrigin = (ViewSpace.GetOutputMax() - ViewSpace.GetOutputMin()) * 0.5;

	ZoomAround(Amount, InputOrigin, OutputOrigin);
}

void SCurveEditorView::ZoomAround(const FVector2D& Amount, double InputOrigin, double OutputOrigin)
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	check(CurveEditor.IsValid());

	if (Amount.X != 0.f && CurveEditor.IsValid())
	{
		double InputMin = 0.0, InputMax = 1.0;
		CurveEditor->GetBounds().GetInputBounds(InputMin, InputMax);

		InputMin = InputOrigin - (InputOrigin - InputMin) * Amount.X;
		InputMax = InputOrigin + (InputMax - InputOrigin) * Amount.X;

		CurveEditor->GetBounds().SetInputBounds(InputMin, InputMax);
	}

	if (Amount.Y != 0.f)
	{
		OutputMin = OutputOrigin - (OutputOrigin - OutputMin) * Amount.Y;
		OutputMax = OutputOrigin + (OutputMax - OutputOrigin) * Amount.Y;
	}
}

void SCurveEditorView::GetCurveDrawParams(TArray<FCurveDrawParams>& OutDrawParams) const
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	if (!CurveEditor)
	{
		return;
	}

	// Get the Min/Max values on the X axis, for Time
	double InputMin = 0, InputMax = 1;
	GetInputBounds(InputMin, InputMax);

	ECurveEditorTangentVisibility TangentVisibility = CurveEditor->GetSettings()->GetTangentVisibility();
	OutDrawParams.Reserve(CurveInfoByID.Num());

	for (const TTuple<FCurveModelID, FCurveInfo>& Pair : CurveInfoByID)
	{
		FCurveModel* CurveModel = CurveEditor->FindCurve(Pair.Key);
		if (!ensureAlways(CurveModel))
		{
			continue;
		}

		FCurveEditorScreenSpace CurveSpace = GetCurveSpace(Pair.Key);

		const float DisplayRatio = (CurveSpace.PixelsPerOutput() / CurveSpace.PixelsPerInput());

		const FKeyHandleSet* SelectedKeys = CurveEditor->GetSelection().GetAll().Find(Pair.Key);

		// Create a new set of Curve Drawing Parameters to represent this particular Curve
		FCurveDrawParams Params(Pair.Key);
		Params.Color = CurveModel->GetColor();
		Params.bKeyDrawEnabled = CurveModel->IsKeyDrawEnabled();

		// Gather the display metrics to use for each key type. This allows a Curve Model to override
		// whether or not the curve supports Keys, Arrive/Leave Tangents, etc. If the Curve Model doesn't
		// support a particular capability we can skip drawing them.
		CurveModel->GetKeyDrawInfo(ECurvePointType::ArriveTangent, FKeyHandle::Invalid(), Params.ArriveTangentDrawInfo);
		CurveModel->GetKeyDrawInfo(ECurvePointType::LeaveTangent, FKeyHandle::Invalid(), Params.LeaveTangentDrawInfo);

		// Gather the interpolating points in input/output space
		TArray<TTuple<double, double>> InterpolatingPoints;

		CurveModel->DrawCurve(*CurveEditor, CurveSpace, InterpolatingPoints);
		Params.InterpolatingPoints.Reserve(InterpolatingPoints.Num());

		// An Input Offset allows for a fixed offset to all keys, such as displaying them in the middle of a frame instead of at the start.
		double InputOffset = CurveModel->GetInputDisplayOffset();

		// Convert the interpolating points to screen space
		for (TTuple<double, double> Point : InterpolatingPoints)
		{
			Params.InterpolatingPoints.Add(
				FVector2D(
					CurveSpace.SecondsToScreen(Point.Get<0>() + InputOffset),
					CurveSpace.ValueToScreen(Point.Get<1>())
				)
			);
		}

		TArray<FKeyHandle> VisibleKeys;
		CurveModel->GetKeys(*CurveEditor, InputMin, InputMax, TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), VisibleKeys);

		if (VisibleKeys.Num())
		{
			TArray<FKeyPosition> AllKeyPositions;
			TArray<FKeyAttributes> AllKeyAttributes;

			AllKeyPositions.SetNum(VisibleKeys.Num());
			AllKeyAttributes.SetNum(VisibleKeys.Num());

			CurveModel->GetKeyPositions(VisibleKeys, AllKeyPositions);
			CurveModel->GetKeyAttributes(VisibleKeys, AllKeyAttributes);

			for (int32 Index = 0; Index < VisibleKeys.Num(); ++Index)
			{
				const FKeyHandle      KeyHandle = VisibleKeys[Index];
				const FKeyPosition&   KeyPosition = AllKeyPositions[Index];
				const FKeyAttributes& Attributes = AllKeyAttributes[Index];

				bool bShowTangents = TangentVisibility == ECurveEditorTangentVisibility::AllTangents || 
					(TangentVisibility == ECurveEditorTangentVisibility::SelectedKeys && SelectedKeys && 
					 (SelectedKeys->Contains(VisibleKeys[Index], ECurvePointType::Any) ) );

				float TimeScreenPos = CurveSpace.SecondsToScreen(KeyPosition.InputValue + InputOffset);
				float ValueScreenPos = CurveSpace.ValueToScreen(KeyPosition.OutputValue);

				// Add this key
				FCurvePointInfo Key(KeyHandle);
				Key.ScreenPosition = FVector2D(TimeScreenPos, ValueScreenPos);
				Key.LayerBias = 2;

				// Add draw info for the specific key
				CurveModel->GetKeyDrawInfo(ECurvePointType::Key, KeyHandle, /*Out*/ Key.DrawInfo);
				Params.Points.Add(Key);

				if (bShowTangents && Attributes.HasArriveTangent())
				{
					float ArriveTangent = Attributes.GetArriveTangent();

					FCurvePointInfo ArriveTangentPoint(KeyHandle);
					ArriveTangentPoint.Type = ECurvePointType::ArriveTangent;


					if (Attributes.HasTangentWeightMode() && Attributes.HasArriveTangentWeight() &&
						(Attributes.GetTangentWeightMode() == RCTWM_WeightedBoth || Attributes.GetTangentWeightMode() == RCTWM_WeightedArrive))
					{
						FVector2D TangentOffset = CurveEditor::ComputeScreenSpaceTangentOffset(CurveSpace, ArriveTangent, -Attributes.GetArriveTangentWeight());
						ArriveTangentPoint.ScreenPosition = Key.ScreenPosition + TangentOffset;
					}
					else
					{
						float PixelLength = 60.0f;
						ArriveTangentPoint.ScreenPosition = Key.ScreenPosition + CurveEditor::GetVectorFromSlopeAndLength(ArriveTangent * -DisplayRatio, -PixelLength);
					}
					ArriveTangentPoint.LineDelta = Key.ScreenPosition - ArriveTangentPoint.ScreenPosition;
					ArriveTangentPoint.LayerBias = 1;

					// Add draw info for the specific tangent
					FKeyDrawInfo TangentDrawInfo;
					CurveModel->GetKeyDrawInfo(ECurvePointType::ArriveTangent, KeyHandle, /*Out*/ ArriveTangentPoint.DrawInfo);

					Params.Points.Add(ArriveTangentPoint);
				}

				if (bShowTangents && Attributes.HasLeaveTangent())
				{
					float LeaveTangent = Attributes.GetLeaveTangent();

					FCurvePointInfo LeaveTangentPoint(KeyHandle);
					LeaveTangentPoint.Type = ECurvePointType::LeaveTangent;

					if (Attributes.HasTangentWeightMode() && Attributes.HasLeaveTangentWeight() &&
						(Attributes.GetTangentWeightMode() == RCTWM_WeightedBoth || Attributes.GetTangentWeightMode() == RCTWM_WeightedLeave))
					{
						FVector2D TangentOffset = CurveEditor::ComputeScreenSpaceTangentOffset(CurveSpace, LeaveTangent, Attributes.GetLeaveTangentWeight());

						LeaveTangentPoint.ScreenPosition = Key.ScreenPosition + TangentOffset;
					}
					else
					{
						float PixelLength = 60.0f;
						LeaveTangentPoint.ScreenPosition = Key.ScreenPosition + CurveEditor::GetVectorFromSlopeAndLength(LeaveTangent * -DisplayRatio, PixelLength);

					}

					LeaveTangentPoint.LineDelta = Key.ScreenPosition - LeaveTangentPoint.ScreenPosition;
					LeaveTangentPoint.LayerBias = 1;

					// Add draw info for the specific tangent
					FKeyDrawInfo TangentDrawInfo;
					CurveModel->GetKeyDrawInfo(ECurvePointType::LeaveTangent, KeyHandle, /*Out*/ LeaveTangentPoint.DrawInfo);

					Params.Points.Add(LeaveTangentPoint);
				}
			}
		}

		OutDrawParams.Add(MoveTemp(Params));
	}
}
