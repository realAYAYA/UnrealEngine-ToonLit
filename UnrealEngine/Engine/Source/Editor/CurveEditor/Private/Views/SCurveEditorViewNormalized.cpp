// Copyright Epic Games, Inc. All Rights Reserved.

#include "Views/SCurveEditorViewNormalized.h"

#include "Containers/SortedMap.h"
#include "CurveEditor.h"
#include "CurveEditorScreenSpace.h"
#include "CurveEditorSettings.h"
#include "CurveModel.h"
#include "Delegates/Delegate.h"
#include "Fonts/SlateFontInfo.h"
#include "HAL/PlatformCrt.h"
#include "IBufferedCurveModel.h"
#include "Layout/Children.h"
#include "Layout/Geometry.h"
#include "Layout/Margin.h"
#include "Math/Color.h"
#include "Math/TransformCalculus2D.h"
#include "Math/Vector2D.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "Rendering/DrawElements.h"
#include "SCurveEditorView.h"
#include "Styling/CoreStyle.h"
#include "Styling/ISlateStyle.h"
#include "Templates/Tuple.h"
#include "Templates/UniquePtr.h"
#include "Types/SlateEnums.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

class FPaintArgs;
class FSlateRect;
class FText;
class FWidgetStyle;

constexpr float NormalizedPadding = 10.f;

void SCurveEditorViewNormalized::Construct(const FArguments& InArgs, TWeakPtr<FCurveEditor> InCurveEditor)
{
	//when created we set the output bounds to be fixed since otherwise it may get fitted
	bFixedOutputBounds = true;
	FrameVertical(-0.1, 1.1);

	SInteractiveCurveEditorView::Construct(InArgs, InCurveEditor);

	ChildSlot
	.HAlign(HAlign_Right)
	.VAlign(VAlign_Top)
	.Padding(FMargin(0.f, CurveViewConstants::CurveLabelOffsetY, CurveViewConstants::CurveLabelOffsetX, 0.f))
	[
		SNew(STextBlock)
		.Font(FCoreStyle::Get().GetFontStyle("ToolTip.LargerFont"))
		.ColorAndOpacity(this, &SCurveEditorViewNormalized::GetCurveCaptionColor)
		.Text(this, &SCurveEditorViewNormalized::GetCurveCaption)
	];
}

FTransform2D CalculateViewToCurveTransform(const double OutputMin, const double OutputMax, const double InCurveOutputMin, const double InCurveOutputMax)
{
	const double Scale = (InCurveOutputMax - InCurveOutputMin);
	if (InCurveOutputMax > InCurveOutputMin)
	{
		return FTransform2D(FScale2D(1.0, Scale), FVector2D(0.0, InCurveOutputMin));
	}
	else
	{
		return FVector2D(0.f, InCurveOutputMin - 0.5);
	}
}

void SCurveEditorViewNormalized::DrawBufferedCurves(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 BaseLayerId, ESlateDrawEffect DrawEffects) const
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	if (!CurveEditor)
	{
		return;
	}

	if (!CurveEditor->GetSettings()->GetShowBufferedCurves())
	{
		return;
	}

	const float BufferedCurveThickness = 1.f;
	const bool  bAntiAliasCurves = true;
	const FLinearColor CurveColor = CurveViewConstants::BufferedCurveColor;
	const int32 CurveLayerId = BaseLayerId + CurveViewConstants::ELayerOffset::Curves;

	const double ValuePerPixel = 1.0 / AllottedGeometry.GetLocalSize().Y;
	const double ValueSpacePadding = NormalizedPadding * ValuePerPixel;

	// Calculate the normalized view to curve transform for each buffered curve, then draw
	const TArray<TUniquePtr<IBufferedCurveModel>>& BufferedCurves = CurveEditor->GetBufferedCurves();
	for (const TUniquePtr<IBufferedCurveModel>& BufferedCurve : BufferedCurves)
	{
		if (!CurveEditor->IsActiveBufferedCurve(BufferedCurve))
		{
			continue;
		}

		FTransform2D ViewToBufferedCurveTransform;
		double CurveOutputMin = BufferedCurve->GetValueMin(), CurveOutputMax = BufferedCurve->GetValueMax();

		ViewToBufferedCurveTransform = CalculateViewToCurveTransform(OutputMin, OutputMax,CurveOutputMin, CurveOutputMax);

		TArray<TTuple<double, double>> CurveSpaceInterpolatingPoints;
		FCurveEditorScreenSpace CurveSpace = GetViewSpace().ToCurveSpace(ViewToBufferedCurveTransform);

		BufferedCurve->DrawCurve(*CurveEditor, CurveSpace, CurveSpaceInterpolatingPoints);

		TArray<FVector2D> ScreenSpaceInterpolatingPoints;
		for (TTuple<double, double> Point : CurveSpaceInterpolatingPoints)
		{
			ScreenSpaceInterpolatingPoints.Add(FVector2D(
				CurveSpace.SecondsToScreen(Point.Get<0>()),
				CurveSpace.ValueToScreen(Point.Get<1>())
			));
		}

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			CurveLayerId,
			AllottedGeometry.ToPaintGeometry(),
			ScreenSpaceInterpolatingPoints,
			DrawEffects,
			CurveColor,
			bAntiAliasCurves,
			BufferedCurveThickness
		);
	}
}

void SCurveEditorViewNormalized::PaintView(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 BaseLayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	if (CurveEditor)
	{
		const ESlateDrawEffect DrawEffects = ShouldBeEnabled(bParentEnabled) ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

		DrawBackground(AllottedGeometry, OutDrawElements, BaseLayerId, DrawEffects);
		DrawGridLines(CurveEditor.ToSharedRef(), AllottedGeometry, OutDrawElements, BaseLayerId, DrawEffects);
		DrawBufferedCurves(AllottedGeometry, MyCullingRect, OutDrawElements, BaseLayerId, DrawEffects);
		DrawCurves(CurveEditor.ToSharedRef(), AllottedGeometry, MyCullingRect, OutDrawElements, BaseLayerId, InWidgetStyle, DrawEffects);
	}
}

void SCurveEditorViewNormalized::FrameVertical(double InOutputMin, double InOutputMax)
{
	OutputMin = -0.1;
	OutputMax = 1.1;;
}

void SCurveEditorViewNormalized::UpdateViewToTransformCurves(double InputMin, double InputMax) 
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	if (!CurveEditor)
	{
		return;
	}

	for (auto It = CurveInfoByID.CreateIterator(); It; ++It)
	{
		FCurveModel* Curve = CurveEditor->FindCurve(It.Key());
		if (!ensureAlways(Curve))
		{
			continue;
		}

		double CurveOutputMin = 0, CurveOutputMax = 1;
		Curve->GetValueRange(InputMin, InputMax, CurveOutputMin, CurveOutputMax);

		It->Value.ViewToCurveTransform = CalculateViewToCurveTransform(OutputMin, OutputMax, CurveOutputMin, CurveOutputMax);
	}
}


void SCurveEditorViewNormalized::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	if (!CurveEditor)
	{
		return;
	}

	if (!CurveEditor->AreBoundTransformUpdatesSuppressed())
	{
		bFixedOutputBounds = false;
		double InputMin = 0, InputMax = 1;
		GetInputBounds(InputMin, InputMax);
		UpdateViewToTransformCurves(InputMin, InputMax);
	}

	SInteractiveCurveEditorView::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}