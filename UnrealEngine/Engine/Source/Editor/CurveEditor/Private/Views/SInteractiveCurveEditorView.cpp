// Copyright Epic Games, Inc. All Rights Reserved.

#include "Views/SInteractiveCurveEditorView.h"

#include "Algo/Sort.h"
#include "AnimatedRange.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/SortedMap.h"
#include "Containers/SparseArray.h"
#include "CurveDataAbstraction.h"
#include "CurveDrawInfo.h"
#include "CurveEditor.h"
#include "CurveEditorCommands.h"
#include "CurveEditorContextMenu.h"
#include "CurveEditorHelpers.h"
#include "CurveEditorScreenSpace.h"
#include "CurveEditorSelection.h"
#include "CurveEditorSettings.h"
#include "CurveEditorSnapMetrics.h"
#include "CurveEditorTypes.h"
#include "CurveModel.h"
#include "Curves/KeyHandle.h"
#include "Curves/RichCurve.h"
#include "Delegates/Delegate.h"
#include "DragOperations/CurveEditorDragOperation_MoveKeys.h"
#include "DragOperations/CurveEditorDragOperation_Pan.h"
#include "DragOperations/CurveEditorDragOperation_Tangent.h"
#include "DragOperations/CurveEditorDragOperation_Zoom.h"
#include "Fonts/FontMeasure.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformCrt.h"
#include "IBufferedCurveModel.h"
#include "ICurveEditorToolExtension.h"
#include "ITimeSlider.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Internationalization/Internationalization.h"
#include "Layout/Geometry.h"
#include "Layout/PaintGeometry.h"
#include "Layout/SlateRect.h"
#include "Layout/WidgetPath.h"
#include "Math/Box.h"
#include "Math/NumericLimits.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "Misc/AssertionMacros.h"
#include "Misc/FrameRate.h"
#include "Misc/FrameTime.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateLayoutTransform.h"
#include "Rendering/SlateRenderer.h"
#include "SCurveEditorPanel.h"
#include "ScopedTransaction.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/ISlateStyle.h"
#include "Templates/UniquePtr.h"
#include "Templates/UnrealTemplate.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"

class FPaintArgs;
class FWidgetStyle;

TAutoConsoleVariable<bool> CVarDrawCurveLines(TEXT("CurveEditor.DrawCurveLines"), true, TEXT("When true we draw curve lines, when false we do not."));
TAutoConsoleVariable<bool> CVarDrawCurveKeys(TEXT("CurveEditor.DrawCurveKeys"), true, TEXT("When true we draw curve keys, when false we do not."));

namespace CurveViewConstants
{
	/** The number of pixels to offset Labels from the Left/Right size. */
	constexpr float LabelOffsetPixels = 2.f;

	/** The number of pixels away the mouse can be and still be considering hovering over a curve. */
	constexpr float HoverProximityThresholdPx = 5.f;
}

#define LOCTEXT_NAMESPACE "SInteractiveCurveEditorView"

TUniquePtr<ICurveEditorKeyDragOperation> CreateKeyDrag(ECurvePointType KeyType)
{
	switch (KeyType)
	{
	case ECurvePointType::ArriveTangent:
	case ECurvePointType::LeaveTangent:
		return MakeUnique<FCurveEditorDragOperation_Tangent>();

	default:
		return MakeUnique<FCurveEditorDragOperation_MoveKeys>();
	}
}

class SDynamicToolTip : public SToolTip
{
public:
	TAttribute<bool> bIsEnabled;
	virtual bool IsEmpty() const override { return !bIsEnabled.Get(); }
};

void SInteractiveCurveEditorView::Construct(const FArguments& InArgs, TWeakPtr<FCurveEditor> InCurveEditor)
{

	FixedHeight = InArgs._FixedHeight;
	BackgroundTint = InArgs._BackgroundTint;
	MaximumCapacity = InArgs._MaximumCapacity;
	bAutoSize = InArgs._AutoSize;

	WeakCurveEditor = InCurveEditor;
	CachedValues.CachedTangentVisibility = InCurveEditor.Pin()->GetSettings()->GetTangentVisibility();

	InCurveEditor.Pin()->OnActiveToolChangedDelegate.AddSP(this, &SInteractiveCurveEditorView::OnCurveEditorToolChanged);

	TSharedRef<SDynamicToolTip> ToolTipWidget =
		SNew(SDynamicToolTip)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolTip.BrightBackground"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				SNew(STextBlock)
				.Text(this, &SInteractiveCurveEditorView::GetToolTipCurveName)
				.Font(FCoreStyle::Get().GetFontStyle("ToolTip.LargerFont"))
				.ColorAndOpacity(FLinearColor::Black)
			]
		+ SVerticalBox::Slot()
		[
			SNew(STextBlock)
			.Text(this, &SInteractiveCurveEditorView::GetToolTipTimeText)
			.Font(FCoreStyle::Get().GetFontStyle("ToolTip.LargerFont"))
			.ColorAndOpacity(FLinearColor::Black)
		]
		+ SVerticalBox::Slot()
		[
			SNew(STextBlock)
			.Text(this, &SInteractiveCurveEditorView::GetToolTipValueText)
			.Font(FCoreStyle::Get().GetFontStyle("ToolTip.LargerFont"))
			.ColorAndOpacity(FLinearColor::Black)
		]
	];

	ToolTipWidget->bIsEnabled = MakeAttributeSP(this, &SInteractiveCurveEditorView::IsToolTipEnabled);
	SetToolTip(ToolTipWidget);
}

FText SInteractiveCurveEditorView::GetCurveCaption() const
{
	FText CurveCaption;

	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	if (CurveEditor && CurveInfoByID.Num() == 1)
	{
		for (const TTuple<FCurveModelID, FCurveInfo>& Pair : CurveInfoByID)
		{
			if (const FCurveModel* Curve = CurveEditor->FindCurve(Pair.Key))
			{
				CurveCaption = Curve->GetLongDisplayName();
				break;
			}
		}
	}

	if (!CurveCaption.IdenticalTo(CachedCurveCaption))
	{
		CachedCurveCaption = CurveCaption;
		bNeedsRefresh = true;
	}

	return CurveCaption;
}

FSlateColor SInteractiveCurveEditorView::GetCurveCaptionColor() const
{
	FSlateColor CurveCaptionColor = BackgroundTint.CopyWithNewOpacity(1.f);

	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	if (CurveEditor && CurveInfoByID.Num() == 1)
	{
		for (const TTuple<FCurveModelID, FCurveInfo>& Pair : CurveInfoByID)
		{
			if (const FCurveModel* Curve = CurveEditor->FindCurve(Pair.Key))
			{
				CurveCaptionColor = Curve->GetColor();
				break;
			}
		}
	}

	if (CurveCaptionColor != CachedCurveCaptionColor)
	{
		CachedCurveCaptionColor = CurveCaptionColor;
		bNeedsRefresh = true;
	}

	return CurveCaptionColor;
}

void SInteractiveCurveEditorView::GetGridLinesX(TSharedRef<const FCurveEditor> CurveEditor, TArray<float>& MajorGridLines, TArray<float>& MinorGridLines, TArray<FText>* MajorGridLabels) const
{
	CurveEditor->GetGridLinesX(MajorGridLines, MinorGridLines, MajorGridLabels);

	FCurveEditorScreenSpaceH PanelSpace = CurveEditor->GetPanelInputSpace();
	FCurveEditorScreenSpaceH ViewSpace  = GetViewSpace();

	double InputOffset = ViewSpace.GetInputMin() - PanelSpace.GetInputMin();
	if (InputOffset != 0.0)
	{
		const float PixelDifference = InputOffset * PanelSpace.PixelsPerInput();
		for (float& Line : MajorGridLines)
		{
			Line -= PixelDifference;
		}
		for (float& Line : MinorGridLines)
		{
			Line -= PixelDifference;
		}
	}
}

void SInteractiveCurveEditorView::GetGridLinesY(TSharedRef<const FCurveEditor> CurveEditor, TArray<float>& MajorGridLines, TArray<float>& MinorGridLines, TArray<FText>* MajorGridLabels) const
{
	const TOptional<float> GridLineSpacing = CurveEditor->GetGridSpacing();
	if (!GridLineSpacing)
	{
		CurveEditor::ConstructYGridLines(GetViewSpace(), 4, MajorGridLines, MinorGridLines, CurveEditor->GetGridLineLabelFormatYAttribute().Get(), MajorGridLabels);
	}
	else
	{
		CurveEditor::ConstructFixedYGridLines(GetViewSpace(), 4, GridLineSpacing.GetValue(), MajorGridLines, MinorGridLines, CurveEditor->GetGridLineLabelFormatYAttribute().Get(), MajorGridLabels, TOptional<double>(), TOptional<double>());
	}
}


void SInteractiveCurveEditorView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (bNeedsRefresh)
	{
		bNeedsRefresh = false;

		RefreshRetainer();
	}

	SCurveEditorView::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

int32 SInteractiveCurveEditorView::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 BaseLayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	PaintView(Args, AllottedGeometry, MyCullingRect, OutDrawElements, BaseLayerId, InWidgetStyle, bParentEnabled);
	SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, BaseLayerId + CurveViewConstants::ELayerOffset::WidgetContent, InWidgetStyle, bParentEnabled);

	return BaseLayerId;
}

void SInteractiveCurveEditorView::PaintView(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 BaseLayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	if (CurveEditor)
	{
		const ESlateDrawEffect DrawEffects = ShouldBeEnabled(bParentEnabled) ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

		DrawBackground(AllottedGeometry, OutDrawElements, BaseLayerId, DrawEffects);
		DrawGridLines(CurveEditor.ToSharedRef(), AllottedGeometry, OutDrawElements, BaseLayerId, DrawEffects);
		DrawBufferedCurves(CurveEditor.ToSharedRef(), AllottedGeometry, MyCullingRect, OutDrawElements, BaseLayerId, InWidgetStyle, DrawEffects);
		DrawCurves(CurveEditor.ToSharedRef(), AllottedGeometry, MyCullingRect, OutDrawElements, BaseLayerId, InWidgetStyle, DrawEffects);
	}
}

void SInteractiveCurveEditorView::DrawBackground(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 BaseLayerId, ESlateDrawEffect DrawEffects) const
{
	if (BackgroundTint != FLinearColor::White)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, BaseLayerId + CurveViewConstants::ELayerOffset::Background, AllottedGeometry.ToPaintGeometry(),
			FAppStyle::GetBrush("ToolPanel.GroupBorder"), DrawEffects, BackgroundTint);
	}
}

void SInteractiveCurveEditorView::DrawGridLines(TSharedRef<FCurveEditor> CurveEditor, const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 BaseLayerId, ESlateDrawEffect DrawEffects) const
{
	// Rendering info
	const float          Width = AllottedGeometry.GetLocalSize().X;
	const float          Height = AllottedGeometry.GetLocalSize().Y;
	const float          RoundedWidth = FMath::RoundToFloat(Width);
	const float          RoundedHeight = FMath::RoundToFloat(Height);
	const FLinearColor   MajorGridColor = CurveEditor->GetPanel()->GetGridLineTint();
	const FLinearColor   MinorGridColor = MajorGridColor.CopyWithNewOpacity(MajorGridColor.A * .5f);
	const FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry();
	const FLinearColor	 LabelColor = FLinearColor::White.CopyWithNewOpacity(0.65f);
	const FSlateFontInfo FontInfo = FCoreStyle::Get().GetFontStyle("ToolTip.LargerFont");

	// Get our viewing range bounds. We go through the GetBounds() interface on the curve editor because it's more aware of what our range is than the original widget is.
	double InputValueMin, InputValueMax;
	GetInputBounds(InputValueMin, InputValueMax);

	TArray<float> MajorGridLines, MinorGridLines;
	TArray<FText> MajorGridLabels;

	GetGridLinesX(CurveEditor, MajorGridLines, MinorGridLines, &MajorGridLabels);
	ensureMsgf(MajorGridLabels.Num() == 0 || MajorGridLines.Num() == MajorGridLabels.Num(), TEXT("If grid labels are specified, one must be specified for every major grid line, even if it is just an empty FText."));

	// Pre-allocate an array of line points to draw our vertical lines. Each major grid line
	// will overwrite the X value of both points but leave the Y value untouched so they draw from the bottom to the top.
	TArray<FVector2D> LinePoints;
	LinePoints.Add(FVector2D(0.f, 0.f));
	LinePoints.Add(FVector2D(0.f, Height));

	const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	// Draw major vertical grid lines
	for (int32 i = 0; i < MajorGridLines.Num(); i++)
	{
		const float RoundedLine = FMath::RoundToFloat(MajorGridLines[i]);
		if (RoundedLine < 0 || RoundedLine > RoundedWidth)
		{
			continue;
		}

		// Vertical Grid Line
		LinePoints[0].X = LinePoints[1].X = RoundedLine;

		if (MajorGridLabels.IsValidIndex(i))
		{
			FText Label = MajorGridLabels[i];
			const FVector2D LabelSize = FontMeasure->Measure(Label, FontInfo);
			const FPaintGeometry LabelGeometry = AllottedGeometry.ToPaintGeometry(FSlateLayoutTransform(FVector2D(LinePoints[0].X - LabelSize.X*.5f, CurveViewConstants::LabelOffsetPixels)));

			LinePoints[0].Y = LabelSize.Y + CurveViewConstants::LabelOffsetPixels*2.f;

			FSlateDrawElement::MakeText(
				OutDrawElements,
				BaseLayerId + CurveViewConstants::ELayerOffset::GridLabels,
				LabelGeometry,
				Label,
				FontInfo,
				DrawEffects,
				LabelColor
			);
		}
		else
		{
			LinePoints[0].Y = 0.f;
		}

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			BaseLayerId + CurveViewConstants::ELayerOffset::GridLines,
			PaintGeometry,
			LinePoints,
			DrawEffects,
			MajorGridColor,
			false
		);
	}

	LinePoints[0].Y = 0.f;

	// Now draw the minor vertical lines which are drawn with a lighter color.
	for (float PosX : MinorGridLines)
	{
		if (PosX < 0 || PosX > Width)
		{
			continue;
		}

		LinePoints[0].X = LinePoints[1].X = PosX;

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			BaseLayerId + CurveViewConstants::ELayerOffset::GridLines,
			PaintGeometry,
			LinePoints,
			DrawEffects,
			MinorGridColor,
			false
		);
	}

	MajorGridLines.Reset();
	MinorGridLines.Reset();
	MajorGridLabels.Reset();
	GetGridLinesY(CurveEditor, MajorGridLines, MinorGridLines, &MajorGridLabels);
	ensureMsgf(MajorGridLabels.Num() == 0 || MajorGridLines.Num() == MajorGridLabels.Num(), TEXT("If grid labels are specified, one must be specified for every major grid line, even if it is just an empty FText."));

	// Reset our cached Line to draw from left to right
	LinePoints[0].X = 0.f;
	LinePoints[1].X = Width;

	// Draw our major horizontal lines
	for (int32 i = 0; i < MajorGridLines.Num(); i++)
	{
		const float RoundedLine = FMath::RoundToFloat(MajorGridLines[i]);
		if (RoundedLine < 0 || RoundedLine > RoundedHeight)
		{
			continue;
		}

		// Overwrite the height of the line we're drawing to draw the different grid lines.
		LinePoints[0].Y = LinePoints[1].Y = RoundedLine;

		if (MajorGridLabels.IsValidIndex(i))
		{
			FText Label = MajorGridLabels[i];
			const FVector2D LabelSize = FontMeasure->Measure(Label, FontInfo);
			const FPaintGeometry LabelGeometry = AllottedGeometry.ToPaintGeometry(FSlateLayoutTransform(FVector2D(CurveViewConstants::LabelOffsetPixels, LinePoints[0].Y - LabelSize.Y*.5f)));

			LinePoints[0].X = LabelSize.X + CurveViewConstants::LabelOffsetPixels*2.f;

			FSlateDrawElement::MakeText(
				OutDrawElements,
				BaseLayerId + CurveViewConstants::ELayerOffset::GridLabels,
				LabelGeometry,
				Label,
				FontInfo,
				DrawEffects,
				LabelColor
			);
		}
		else
		{
			LinePoints[0].X = 0.f;
		}

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			BaseLayerId + CurveViewConstants::ELayerOffset::GridLines,
			PaintGeometry,
			LinePoints,
			DrawEffects,
			MajorGridColor,
			false
		);
	}

	LinePoints[0].X = 0.f;

	// Draw our minor horizontal lines
	for (float PosY : MinorGridLines)
	{
		if (PosY < 0 || PosY > Height)
		{
			continue;
		}

		LinePoints[0].Y = LinePoints[1].Y = PosY;

		// Now draw the minor grid lines with a lighter color.
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			BaseLayerId + CurveViewConstants::ELayerOffset::GridLines,
			PaintGeometry,
			LinePoints,
			DrawEffects,
			MinorGridColor,
			false
		);
	}
}

void SInteractiveCurveEditorView::DrawCurves(TSharedRef<FCurveEditor> CurveEditor, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 BaseLayerId, const FWidgetStyle& InWidgetStyle, ESlateDrawEffect DrawEffects) const
{
	FLinearColor SelectionColor = CurveEditor->GetSettings()->GetSelectionColor();

	const FVector2D      VisibleSize = AllottedGeometry.GetLocalSize();
	const FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry();

	const float HoveredCurveThickness = 3.5f;
	const float UnHoveredCurveThickness = 2.f;
	const bool  bAntiAliasCurves = true;

	const bool bDrawLines = CVarDrawCurveLines.GetValueOnGameThread();
	const bool bDrawKeys = CVarDrawCurveKeys.GetValueOnGameThread();

	TOptional<FCurveModelID> HoveredCurve = GetHoveredCurve();
	for (const FCurveDrawParams& Params : CachedDrawParams)
	{
		const bool bIsCurveHovered = HoveredCurve.IsSet() && HoveredCurve.GetValue() == Params.GetID();
		const float Thickness = bIsCurveHovered ? HoveredCurveThickness : UnHoveredCurveThickness;
		const int32 CurveLayerId = bIsCurveHovered ? BaseLayerId + CurveViewConstants::ELayerOffset::Curves : BaseLayerId + CurveViewConstants::ELayerOffset::HoveredCurves;

		if (bDrawLines)
		{

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				CurveLayerId,
				PaintGeometry,
				Params.InterpolatingPoints,
				DrawEffects,
				Params.Color,
				bAntiAliasCurves,
				Thickness
			);
		}
		
		TArray<FVector2D> LinePoints;
		LinePoints.SetNum(2);

		// Draw tangents
		if (bDrawKeys && Params.bKeyDrawEnabled)
		{
			for (int32 PointIndex = 0; PointIndex < Params.Points.Num(); PointIndex++)
			{
				const FCurvePointInfo& Point = Params.Points[PointIndex];
				const FKeyDrawInfo& PointDrawInfo = Params.GetKeyDrawInfo(Point.Type, PointIndex);
				const bool          bSelected = CurveEditor->GetSelection().IsSelected(FCurvePointHandle(Params.GetID(), Point.Type, Point.KeyHandle));
				FLinearColor  PointTint = PointDrawInfo.Tint.IsSet() ? PointDrawInfo.Tint.GetValue() : Params.Color;

				if (bSelected)
				{
					PointTint = SelectionColor;
				}
				else
				{
					// Brighten and saturate the points a bit so they pop
					FLinearColor HSV = PointTint.LinearRGBToHSV();
					HSV.G = FMath::Clamp(HSV.G * 1.1f, 0.f, 255.f);
					HSV.B = FMath::Clamp(HSV.B * 2.f, 0.f, 255.f);
					PointTint = HSV.HSVToLinearRGB();
				}

				const int32 KeyLayerId = BaseLayerId + Point.LayerBias + (bSelected ? CurveViewConstants::ELayerOffset::SelectedKeys : CurveViewConstants::ELayerOffset::Keys);

				if (Point.LineDelta.X != 0.f || Point.LineDelta.Y != 0.f)
				{
					LinePoints[0] = Point.ScreenPosition + Point.LineDelta.GetSafeNormal() * (PointDrawInfo.ScreenSize.X*.5f);
					LinePoints[1] = Point.ScreenPosition + Point.LineDelta;

					// Draw the connecting line - connecting lines are always drawn below everything else
					FSlateDrawElement::MakeLines(OutDrawElements, BaseLayerId + CurveViewConstants::ELayerOffset::Keys - 1, PaintGeometry, LinePoints, DrawEffects, PointTint, true);
				}

				FPaintGeometry PointGeometry = AllottedGeometry.ToPaintGeometry(
					PointDrawInfo.ScreenSize,
					FSlateLayoutTransform(Point.ScreenPosition - (PointDrawInfo.ScreenSize * 0.5f))
				);

				FSlateDrawElement::MakeBox(OutDrawElements, KeyLayerId, PointGeometry, PointDrawInfo.Brush, DrawEffects, PointTint);
			}
		}
	}
}

void SInteractiveCurveEditorView::DrawBufferedCurves(TSharedRef<FCurveEditor> CurveEditor, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 BaseLayerId, const FWidgetStyle& InWidgetStyle, ESlateDrawEffect DrawEffects) const
{
	if (!CurveEditor->GetSettings()->GetShowBufferedCurves())
	{
		return;
	}

	const float BufferedCurveThickness = 1.f;
	const bool  bAntiAliasCurves = true;
	const FLinearColor CurveColor = CurveViewConstants::BufferedCurveColor;
	const TArray<TUniquePtr<IBufferedCurveModel>>& BufferedCurves = CurveEditor->GetBufferedCurves();

	const int32 CurveLayerId = BaseLayerId + CurveViewConstants::ELayerOffset::Curves;

	// Draw each buffered curve using the view space transform since the curve space for all curves is the same
	for (const TUniquePtr<IBufferedCurveModel>& BufferedCurve : BufferedCurves)
	{
		if (!CurveEditor->IsActiveBufferedCurve(BufferedCurve))
		{
			continue;
		}

		TArray<TTuple<double, double>> CurveSpaceInterpolatingPoints;
		FCurveEditorScreenSpace CurveSpace = GetViewSpace();

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

bool SInteractiveCurveEditorView::GetPointsWithinWidgetRange(const FSlateRect& WidgetRectangle, TArray<FCurvePointHandle>* OutPoints) const
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	if (!CurveEditor)
	{
		return false;
	}

	FVector2D LinePoints[2];
	FVector Start, End, StartToEnd;
	FBox WidgetRectangleBox(FVector(WidgetRectangle.Left, WidgetRectangle.Top, 0), FVector(WidgetRectangle.Right, WidgetRectangle.Bottom, 0));

	// Iterate through all of our points and see which points the marquee overlaps. Both of these coordinate systems
	// are in screen space pixels.  Also check tangent lines
	bool bFound = false;
	for (const FCurveDrawParams& DrawParams : CachedDrawParams)
	{
		for (int32 PointIndex = 0; PointIndex < DrawParams.Points.Num(); PointIndex++)
		{
			const FCurvePointInfo& Point = DrawParams.Points[PointIndex];

			const FKeyDrawInfo& DrawInfo = DrawParams.GetKeyDrawInfo(Point.Type, PointIndex);
			FSlateRect PointRect = FSlateRect::FromPointAndExtent(Point.ScreenPosition - DrawInfo.ScreenSize/2, DrawInfo.ScreenSize);

			if (FSlateRect::DoRectanglesIntersect(PointRect, WidgetRectangle))
			{
				OutPoints->Add(FCurvePointHandle(DrawParams.GetID(), Point.Type, Point.KeyHandle));
				bFound = true;
			}
			else if (Point.LineDelta.X != 0.f || Point.LineDelta.Y != 0.f) //if tangent hit test line
			{
				LinePoints[0] = Point.ScreenPosition + Point.LineDelta.GetSafeNormal() * (DrawInfo.ScreenSize.X * .5f);
				LinePoints[1] = Point.ScreenPosition + Point.LineDelta;

				Start = FVector(LinePoints[0].X, LinePoints[0].Y, 0);
				End = FVector(LinePoints[1].X, LinePoints[1].Y, 0);
				StartToEnd = End - Start;

				if (FMath::LineBoxIntersection(WidgetRectangleBox, Start, End, StartToEnd))
				{
					OutPoints->Add(FCurvePointHandle(DrawParams.GetID(), Point.Type, Point.KeyHandle));
					bFound = true;
				}
			}
		}
	}

	return bFound;
}

bool SInteractiveCurveEditorView::GetCurveWithinWidgetRange(const FSlateRect& WidgetRectangle, TArray<FCurvePointHandle>* OutPoints) const
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	if (!CurveEditor)
	{
		return false;
	}

	FBox WidgetRectangleBox(FVector(WidgetRectangle.Left, WidgetRectangle.Top, 0), FVector(WidgetRectangle.Right, WidgetRectangle.Bottom, 0));

	// Iterate through all of our interpolating points and terminates if one overlaps the marquee. Both of these coordinate systems
	// are in screen space pixels.
	TSet<FCurveModelID> CurveIDs;
	for (const FCurveDrawParams& DrawParams : CachedDrawParams)
	{
		for (int32 InterpolatingPointIndex = 1; InterpolatingPointIndex < DrawParams.InterpolatingPoints.Num(); InterpolatingPointIndex++)
		{
			FVector2D InterpolatingPointPrev = DrawParams.InterpolatingPoints[InterpolatingPointIndex-1];
			FVector2D InterpolatingPointNext = DrawParams.InterpolatingPoints[InterpolatingPointIndex];
			FVector Start(InterpolatingPointPrev.X, InterpolatingPointPrev.Y, 0);
			FVector End(InterpolatingPointNext.X, InterpolatingPointNext.Y, 0);
			FVector StartToEnd = End - Start;

			if (FMath::LineBoxIntersection(WidgetRectangleBox, Start, End, StartToEnd))
			{
				CurveIDs.Add(DrawParams.GetID());
			}
		}
	}

	bool bPointsAdded = false;
	for (const FCurveModelID& CurveID : CurveIDs)
	{
		if (const FCurveModel* Curve = CurveEditor->FindCurve(CurveID))
		{
			TArray<FKeyHandle> KeyHandles;
			Curve->GetKeys(*CurveEditor, TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), KeyHandles);

			for (const FKeyHandle& KeyHandle : KeyHandles)
			{
				OutPoints->Add(FCurvePointHandle(CurveID, ECurvePointType::Key, KeyHandle));
				bPointsAdded = true;
			}
		}
	}

	return bPointsAdded;
}

void SInteractiveCurveEditorView::UpdateCurveProximities(FVector2D MousePixel)
{
	CurveProximities.Reset();
	CachedToolTipData.Reset();

	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	if (!CurveEditor)
	{
		return;
	}

	TOptional<FCurvePointHandle> MousePoint = HitPoint(MousePixel);
	if (MousePoint.IsSet())
	{
		// If the mouse is over a point, that curve is always the closest, so just add that directly and don't
		// bother adding the others
		CurveProximities.Add(MakeTuple(MousePoint->CurveID, 0.f));
	}
	else for (const TTuple<FCurveModelID, FCurveInfo>& Pair : CurveInfoByID)
	{
		const FCurveModel* CurveModel = CurveEditor->FindCurve(Pair.Key);
		if (!ensureAlways(CurveModel))
		{
			continue;
		}

		FCurveEditorScreenSpace CurveSpace = GetCurveSpace(Pair.Key);

		double MinMouseTime = CurveSpace.ScreenToSeconds(MousePixel.X - CurveViewConstants::HoverProximityThresholdPx);
		double MaxMouseTime = CurveSpace.ScreenToSeconds(MousePixel.X + CurveViewConstants::HoverProximityThresholdPx);
		double MouseValue = CurveSpace.ScreenToValue(MousePixel.Y);
		float  PixelsPerOutput = CurveSpace.PixelsPerOutput();

		FVector2D MinPos(MousePixel.X - CurveViewConstants::HoverProximityThresholdPx, 0.0f);
		FVector2D MaxPos(MousePixel.X + CurveViewConstants::HoverProximityThresholdPx, 0.0f);

		double InputOffset = CurveModel->GetInputDisplayOffset();
		double MinEvalTime = MinMouseTime - InputOffset;
		double MaxEvalTime = MaxMouseTime - InputOffset;

		double MinValue = 0.0, MaxValue = 0.0;
		if (CurveModel->Evaluate(MinEvalTime, MinValue) && CurveModel->Evaluate(MaxEvalTime, MaxValue))
		{
			MinPos.Y = CurveSpace.ValueToScreen(MinValue);
			MaxPos.Y = CurveSpace.ValueToScreen(MaxValue);

			float Distance = (FMath::ClosestPointOnSegment2D(MousePixel, MinPos, MaxPos) - MousePixel).Size();
			CurveProximities.Add(MakeTuple(Pair.Key, Distance));
		}
	}

	Algo::SortBy(CurveProximities, [](TTuple<FCurveModelID, float> In) { return In.Get<1>(); });

	// Also, set the cached tooltips if dragging because the curve proximity might not be updated during the drag
	if (CurveProximities.Num() > 0 && (CurveProximities[0].Get<1>() < CurveViewConstants::HoverProximityThresholdPx || DragOperation.IsSet()))
	{
		const FCurveModel* HoveredCurve = CurveEditor->FindCurve(CurveProximities[0].Get<0>());
		if (HoveredCurve)
		{
			FCurveEditorScreenSpace CurveSpace = GetCurveSpace(CurveProximities[0].Get<0>());
			double MouseTime = CurveSpace.ScreenToSeconds(MousePixel.X) - HoveredCurve->GetInputDisplayOffset();
			double EvaluatedTime = CurveEditor->GetCurveSnapMetrics(CurveProximities[0].Get<0>()).SnapInputSeconds(MouseTime);

			double EvaluatedValue = 0.0;
			HoveredCurve->Evaluate(EvaluatedTime, EvaluatedValue);

			FCachedToolTipData ToolTipData;
			ToolTipData.Text = FormatToolTipCurveName(*HoveredCurve);
			ToolTipData.EvaluatedTime = FormatToolTipTime(*HoveredCurve, EvaluatedTime);
			ToolTipData.EvaluatedValue = FormatToolTipValue(*HoveredCurve, EvaluatedValue);
			
			CachedToolTipData = ToolTipData;
		}
	}
}

FText SInteractiveCurveEditorView::FormatToolTipCurveName(const FCurveModel& CurveModel) const
{
	return FText::Format(LOCTEXT("CurveEditorTooltipName", "Name: {0}"), CurveModel.GetLongDisplayName());
}

FText SInteractiveCurveEditorView::FormatToolTipTime(const FCurveModel& CurveModel, double EvaluatedTime) const
{
	return FText::Format(LOCTEXT("CurveEditorTime", "Time: {0}"), EvaluatedTime);
}

FText SInteractiveCurveEditorView::FormatToolTipValue(const FCurveModel& CurveModel, double EvaluatedValue) const
{
	return FText::Format(LOCTEXT("CurveEditorValue", "Value: {0}"), EvaluatedValue);
}

void SInteractiveCurveEditorView::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	//CurveProximities.Reset();
	SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
}

void SInteractiveCurveEditorView::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	// Don't allow hover highlights when we've exited this view as clicking won't be routed to us to select it anyways.
	CurveProximities.Reset();
	SCompoundWidget::OnMouseLeave(MouseEvent);
}

FReply SInteractiveCurveEditorView::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	TSharedPtr<SCurveEditorPanel> EditorPanel = CurveEditor.IsValid() ? CurveEditor->GetPanel() : nullptr;
	if (!CurveEditor || !EditorPanel)
	{
		return FReply::Unhandled();
	}

	// Don't handle updating if we have a context menu open.
	if (ActiveContextMenu.Pin())
	{
		return FReply::Unhandled();
	}

	// Update our Curve Proximities for hover states and context actions. This also updates our cached hovered curve.
	FVector2D MousePixel = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	UpdateCurveProximities(MousePixel);

	// Cache the mouse position so that commands such as add key can work from command bindings 
	CachedMousePosition = MousePixel;

	if (DragOperation.IsSet())
	{
		FVector2D InitialPosition = DragOperation->GetInitialPosition();

		if (!DragOperation->IsDragging() && DragOperation->AttemptDragStart(MouseEvent))
		{
			DragOperation->DragImpl->BeginDrag(InitialPosition, MousePixel, MouseEvent);
			return FReply::Handled().CaptureMouse(AsShared());
		}
		else if (DragOperation->IsDragging())
		{
			DragOperation->DragImpl->Drag(InitialPosition, MousePixel, MouseEvent);
		}
		return FReply::Handled();
	}

	// We don't absorb this event as we're just updating hover states anyways.
	return FReply::Unhandled();
}

FReply SInteractiveCurveEditorView::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	if (!CurveEditor || bFixedOutputBounds)
	{
		return FReply::Unhandled();
	}

	FCurveEditorScreenSpace ViewSpace = GetViewSpace();

	FVector2D MousePixel   = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	double    CurrentTime  = ViewSpace.ScreenToSeconds(MousePixel.X);
	double    CurrentValue = ViewSpace.ScreenToValue(MousePixel.Y);

	// If currently in a drag operation, allow it first chance at handling mouse wheel input
	if (DragOperation.IsSet())
	{
		FVector2D InitialPosition = DragOperation->GetInitialPosition();
		FReply Reply = DragOperation->DragImpl->MouseWheel(InitialPosition, MousePixel, MouseEvent);
		if (Reply.IsEventHandled())
		{
			return Reply;
		}
	}

	// Attempt to zoom around the current time if settings specify it and there is a valid time.
	if (CurveEditor->GetSettings()->GetZoomPosition() == ECurveEditorZoomPosition::CurrentTime)
	{
		if (CurveEditor->GetTimeSliderController().IsValid())
		{
			FFrameTime ScrubPosition = CurveEditor->GetTimeSliderController()->GetScrubPosition();
			double PlaybackPosition = ScrubPosition / CurveEditor->GetTimeSliderController()->GetTickResolution();
			if (CurveEditor->GetTimeSliderController()->GetViewRange().Contains(PlaybackPosition))
			{
				CurrentTime = PlaybackPosition;
			}
		}
	}

	float ZoomDelta = 1.f - FMath::Clamp(0.1f * MouseEvent.GetWheelDelta(), -0.9f, 0.9f);
	ZoomAround(FVector2D(ZoomDelta, ZoomDelta), CurrentTime, CurrentValue);

	return FReply::Handled();
}

TOptional<FCurveModelID> SInteractiveCurveEditorView::GetHoveredCurve() const
{
	if (CurveProximities.Num() > 0 && CurveProximities[0].Get<1>() < CurveViewConstants::HoverProximityThresholdPx)
	{
		return CurveProximities[0].Get<0>();
	}

	return TOptional<FCurveModelID>();
}

bool SInteractiveCurveEditorView::IsToolTipEnabled() const
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	if (CurveEditor)
	{
		return (CachedToolTipData.IsSet() && CurveEditor->GetSettings()->GetShowCurveEditorCurveToolTips());
	}

	return false;
}

FText SInteractiveCurveEditorView::GetToolTipCurveName() const
{
	return CachedToolTipData.IsSet() ? CachedToolTipData->Text : FText();
}

FText SInteractiveCurveEditorView::GetToolTipTimeText() const
{
	return CachedToolTipData.IsSet() ? CachedToolTipData->EvaluatedTime : FText();
}

FText SInteractiveCurveEditorView::GetToolTipValueText() const
{
	return CachedToolTipData.IsSet() ? CachedToolTipData->EvaluatedValue : FText();
}

double SInteractiveCurveEditorView::GetTangentValue(const double InTime, const double InValue, FCurveModel* CurveToAddTo, double DeltaTime) const
{
	// Data
	double TargetTime = InTime + DeltaTime;				// The time to get tangent value. Could be left or right depending on is DeltaTime is negative or positive
	double TargetValue = 0.0;							// The helper value to get Tangent value
	CurveToAddTo->Evaluate(TargetTime, TargetValue);	// Initialize TargetValue by TargetTime
	double TangentValue = (TargetValue - InValue) / FMath::Abs(DeltaTime);	// The tangent value to return
	double PrevTangent = DBL_MAX;						// Used for determine whether the tangent is close to the limit
	int32 Count = 10;									// Preventing we stuck in this function for too long

	// Logic
	// While the tangents not close enough and we haven't reach the max iteration time
	while (!FMath::IsNearlyEqual(FMath::Abs(TangentValue), FMath::Abs(PrevTangent)) && Count > 0)
	{
		// Update previous tangent value and make delta time smaller
		PrevTangent = TangentValue;
		DeltaTime /= 2.0;
		TargetTime = InTime + DeltaTime;

		// Calculate a more precise tangent value
		CurveToAddTo->Evaluate(TargetTime, TargetValue);
		TangentValue = (TargetValue - InValue) / FMath::Abs(DeltaTime);

		--Count;
	}
	return TangentValue;
}

void SInteractiveCurveEditorView::HandleDirectKeySelectionByMouse(TSharedPtr<FCurveEditor> CurveEditor, const FPointerEvent& MouseEvent, TOptional<FCurvePointHandle> MouseDownPoint)
{
	if(!MouseDownPoint.IsSet())
	{
		CurveEditor->GetSelection().Clear();
		return;
	}

	const bool bIsShiftDown = MouseEvent.IsShiftDown();
	const bool bIsAltDown = MouseEvent.IsAltDown();
	const bool bIsControlDown = MouseEvent.IsControlDown();
	
	if (bIsShiftDown)
	{
		CurveEditor->GetSelection().Add(MouseDownPoint.GetValue());
	}
	else if (bIsAltDown)
	{
		CurveEditor->GetSelection().Remove(MouseDownPoint.GetValue());
	}
	else if (bIsControlDown)
	{
		CurveEditor->GetSelection().Toggle(MouseDownPoint.GetValue());
	}
	else
	{
		const bool bKeySelected = CurveEditor->GetSelection().Contains(MouseDownPoint->CurveID, MouseDownPoint->KeyHandle, ECurvePointType::Key);
		const bool bLeaveTangentSelected = CurveEditor->GetSelection().Contains(MouseDownPoint->CurveID, MouseDownPoint->KeyHandle, ECurvePointType::LeaveTangent);
		const bool bArriveTangentSelected = CurveEditor->GetSelection().Contains(MouseDownPoint->CurveID, MouseDownPoint->KeyHandle, ECurvePointType::ArriveTangent);

		if (bKeySelected || bLeaveTangentSelected || bArriveTangentSelected)
		{
			// If the picked key handle is already selected in any way, select all of the same point type for the selected points
			if (MouseDownPoint->PointType == ECurvePointType::LeaveTangent)
			{
				TArray<FCurvePointHandle> CurvePointHandles;
				for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : CurveEditor->GetSelection().GetAll())
				{
					for (FKeyHandle Handle : Pair.Value.AsArray())
					{
						// If this isn't the opposite of the clicked on LeaveTangent, select the LeaveTangent so it can be moved as well
						if (Pair.Value.PointType(Handle) != ECurvePointType::ArriveTangent)
						{
							FCurvePointHandle CurvePointHandle(Pair.Key, MouseDownPoint->PointType, Handle);
							CurvePointHandles.Add(CurvePointHandle);
						}
					}
				}

				if (!bLeaveTangentSelected)
				{
					CurveEditor->GetSelection().Clear();
				}
				for (FCurvePointHandle CurvePointHandle : CurvePointHandles)
				{
					CurveEditor->GetSelection().Add(CurvePointHandle);
				}
				CurveEditor->GetSelection().Add(MouseDownPoint.GetValue());
			}
			else if (MouseDownPoint->PointType == ECurvePointType::ArriveTangent)
			{
				TArray<FCurvePointHandle> CurvePointHandles;
				for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : CurveEditor->GetSelection().GetAll())
				{
					for (FKeyHandle Handle : Pair.Value.AsArray())
					{
						// If this isn't the opposite of the clicked on ArriveTangent, select the ArriveTangent so it can be moved as well
						if (Pair.Value.PointType(Handle) != ECurvePointType::LeaveTangent)
						{
							FCurvePointHandle CurvePointHandle(Pair.Key, MouseDownPoint->PointType, Handle);
							CurvePointHandles.Add(CurvePointHandle);
						}
					}
				}

				if (!bArriveTangentSelected)
				{
					CurveEditor->GetSelection().Clear();
				}
				for (FCurvePointHandle CurvePointHandle : CurvePointHandles)
				{
					CurveEditor->GetSelection().Add(CurvePointHandle);
				}
				CurveEditor->GetSelection().Add(MouseDownPoint.GetValue());
			}
			else if (MouseDownPoint->PointType == ECurvePointType::Key)
			{
				TArray<FCurvePointHandle> CurvePointHandles;
				for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : CurveEditor->GetSelection().GetAll())
				{
					for (FKeyHandle Handle : Pair.Value.AsArray())
					{
						FCurvePointHandle CurvePointHandle(Pair.Key, MouseDownPoint->PointType, Handle);
						CurvePointHandles.Add(CurvePointHandle);
					}
				}

				CurveEditor->GetSelection().Clear();
				for (FCurvePointHandle CurvePointHandle : CurvePointHandles)
				{
					CurveEditor->GetSelection().Add(CurvePointHandle);
				}
				CurveEditor->GetSelection().Add(MouseDownPoint.GetValue());
			}
		}
			// If this isn't already selected, treat this as a new selection (clear selection)
		else 
		{
			CurveEditor->GetSelection().Clear();
			CurveEditor->GetSelection().Add(MouseDownPoint.GetValue());
		}
	}
}

FReply SInteractiveCurveEditorView::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	TSharedPtr<SCurveEditorPanel> EditorPanel = CurveEditor.IsValid() ? CurveEditor->GetPanel() : nullptr;
	if (!CurveEditor || !EditorPanel)
	{
		return FReply::Unhandled();
	}
	
	FVector2D MousePixel = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	// Cache the mouse position so that commands such as add key can work from command bindings 
	CachedMousePosition = MousePixel;

	// Rebind our context actions so that shift click commands use the right position.
	RebindContextualActions(MousePixel);

	// // Marquee Selection
	// if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	// {
	// 	DragOperation = FCurveEditorDelayedDrag(MousePixel, MouseEvent.GetEffectingButton());
	// 	DragOperation->DragImpl = MakeUnique<FCurveEditorDragOperation_Marquee>(CurveEditor.Get(), this);
	// 	return FReply::Handled();
	// }
	// Middle Click + Alt Pan
	//else
	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		if (MouseEvent.IsAltDown())
		{
			DragOperation = FCurveEditorDelayedDrag(MousePixel, MouseEvent.GetEffectingButton());
			DragOperation->DragImpl = MakeUnique<FCurveEditorDragOperation_PanInput>(CurveEditor.Get());
			return FReply::Handled();
		}
		else
		{
			// Middle Mouse can try to create keys on curves.
			TOptional<FCurvePointHandle> NewPoint;

			// Add a key to the closest curve to the mouse
			if (TOptional<FCurveModelID> HoveredCurve = GetHoveredCurve())
			{
				// Don't allow adding keys when shift is held down with selected keys since that is for dragging keys in a constrained axis
				const bool bDraggingKeys = MouseEvent.IsShiftDown() && !CurveEditor->GetSelection().GetAll().IsEmpty();

				FCurveModel* CurveToAddTo = CurveEditor->FindCurve(HoveredCurve.GetValue());
				if (CurveToAddTo && !CurveToAddTo->IsReadOnly() && !bDraggingKeys)
				{
					FScopedTransaction Transaction(LOCTEXT("InsertKey", "Insert Key"));

					FCurveEditorScreenSpace CurveSpace = GetCurveSpace(HoveredCurve.GetValue());
					double MouseTime = CurveSpace.ScreenToSeconds(MousePixel.X);
					double MouseValue = CurveSpace.ScreenToValue(MousePixel.Y);

					FKeyAttributes KeyAttributes = GetDefaultKeyAttributesForCurveTime(*CurveEditor, *CurveToAddTo, MouseTime);

					FCurveSnapMetrics SnapMetrics = CurveEditor->GetCurveSnapMetrics(HoveredCurve.GetValue());
					MouseTime = SnapMetrics.SnapInputSeconds(MouseTime);
					MouseValue = SnapMetrics.SnapOutput(MouseValue);

					// If control is pressed. Keep the curve unchanged
					if (MouseEvent.IsControlDown())
					{
						KeyAttributes.SetTangentMode(RCTM_User);

						// Estimated delta time to compute right and left tangents
						double DeltaTime = 0.1;

						// Make mouse value more accurate 
						CurveToAddTo->Evaluate(MouseTime, MouseValue);

						// Compute right tangent
						double RightTangent = GetTangentValue(MouseTime, MouseValue, CurveToAddTo, DeltaTime);
						KeyAttributes.SetLeaveTangent(RightTangent);

						// Left
						double LeftTangent = GetTangentValue(MouseTime, MouseValue, CurveToAddTo, -DeltaTime);
						KeyAttributes.SetArriveTangent(LeftTangent);
					}

					// When adding to a curve with no variance, add it with the same value so that
					// curves don't pop wildly in normalized views due to a slight difference between the keys
					double CurveOutputMin = 0, CurveOutputMax = 1;
					CurveToAddTo->GetValueRange(CurveOutputMin, CurveOutputMax);
					if (CurveOutputMin == CurveOutputMax)
					{
						MouseValue = CurveOutputMin;
					}

					CurveToAddTo->Modify();

					// Add a key on this curve
					TOptional<FKeyHandle> NewKey = CurveToAddTo->AddKey(FKeyPosition(MouseTime, MouseValue), KeyAttributes);
					if (NewKey.IsSet())
					{
						NewPoint = FCurvePointHandle(HoveredCurve.GetValue(), ECurvePointType::Key, NewKey.GetValue());

						CurveEditor->GetSelection().Clear();
						CurveEditor->GetSelection().Add(NewPoint.GetValue());
					}
					else
					{
						Transaction.Cancel();
					}
				}
			}

			// If there are any tangent handles selected, prefer to drag those instead of keys
			ECurvePointType PointType = ECurvePointType::Key;
			if (!NewPoint.IsSet())
			{
				for (const TTuple<FCurveModelID, FKeyHandleSet>& Pair : CurveEditor->GetSelection().GetAll())
				{
					for (FKeyHandle Handle : Pair.Value.AsArray())
					{
						if (Pair.Value.Contains(Handle, ECurvePointType::ArriveTangent) || Pair.Value.Contains(Handle, ECurvePointType::LeaveTangent))
						{
							PointType = ECurvePointType::ArriveTangent;
							break;
						}
					}
				}
			}

			TUniquePtr<ICurveEditorKeyDragOperation> KeyDrag = CreateKeyDrag(PointType);

			KeyDrag->Initialize(CurveEditor.Get(), NewPoint);

			DragOperation = FCurveEditorDelayedDrag(MousePixel, MouseEvent.GetEffectingButton());
			DragOperation->DragImpl = MoveTemp(KeyDrag);

			return FReply::Handled().PreventThrottling();
		}
	}
	else if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		// Zoom Timeline
		if (MouseEvent.IsAltDown())
		{
			DragOperation = FCurveEditorDelayedDrag(MousePixel, MouseEvent.GetEffectingButton());
			DragOperation->DragImpl = MakeUnique<FCurveEditorDragOperation_Zoom>(CurveEditor.Get(), SharedThis(this));
			return FReply::Handled();
		}
		// Pan Timeline if we have flexible output bounds
		else if (!bFixedOutputBounds)
		{
			DragOperation = FCurveEditorDelayedDrag(MousePixel, MouseEvent.GetEffectingButton());
			DragOperation->DragImpl = MakeUnique<FCurveEditorDragOperation_PanView>(CurveEditor.Get(), SharedThis(this));
			return FReply::Handled();
		}
	}

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Key Selection Testing
		TOptional<FCurvePointHandle> MouseDownPoint = HitPoint(MousePixel);
		if (MouseDownPoint.IsSet())
		{
			if (FCurveModel* CurveModel = CurveEditor->FindCurve(MouseDownPoint->CurveID))
			{
				if (!CurveModel->IsReadOnly())
				{
					HandleDirectKeySelectionByMouse(CurveEditor, MouseEvent, MouseDownPoint);

					TUniquePtr<ICurveEditorKeyDragOperation> KeyDrag = CreateKeyDrag(MouseDownPoint->PointType);

					KeyDrag->Initialize(CurveEditor.Get(), MouseDownPoint);

					DragOperation = FCurveEditorDelayedDrag(MousePixel, MouseEvent.GetEffectingButton());
					DragOperation->DragImpl = MoveTemp(KeyDrag);

					return FReply::Handled().PreventThrottling();
				}
			}
		}
	}

	return FReply::Unhandled();
}

FReply SInteractiveCurveEditorView::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	TSharedPtr<SCurveEditorPanel> EditorPanel = CurveEditor.IsValid() ? CurveEditor->GetPanel() : nullptr;
	if (!CurveEditor || !EditorPanel)
	{
		return FReply::Unhandled();
	}

	const bool bDragOperationWasSet = DragOperation.IsSet();
	FVector2D MousePosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	if (DragOperation.IsSet())
	{
		if (DragOperation->IsDragging())
		{
			FVector2D InitialPosition = DragOperation->GetInitialPosition();
			DragOperation->DragImpl->EndDrag(InitialPosition, MousePosition, MouseEvent);

			DragOperation.Reset();
			return FReply::Handled().ReleaseMouseCapture();
		}
	}

	DragOperation.Reset();

	// Select the curve on mouse release if no key or tangent was clicked on
	if (!bDragOperationWasSet && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const bool bIsShiftDown = MouseEvent.IsShiftDown();
		const bool bIsAltDown = MouseEvent.IsAltDown();
		const bool bIsControlDown = MouseEvent.IsControlDown();

		// Curve Selection Testing.
		TOptional<FCurveModelID> HitCurve = GetHoveredCurve();
		if (!HitPoint(MousePosition).IsSet() && HitCurve.IsSet())
		{
			FCurveModel* CurveModel = CurveEditor->FindCurve(HitCurve.GetValue());

			TArray<FKeyHandle> KeyHandles;
			KeyHandles.Reserve(CurveModel->GetNumKeys());
			CurveModel->GetKeys(*CurveEditor, TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), KeyHandles);

			// Add or remove all keys from the curve.
			if (bIsShiftDown)
			{
				CurveEditor->GetSelection().Add(HitCurve.GetValue(), ECurvePointType::Key, KeyHandles);
			}
			else if (bIsAltDown)
			{
				CurveEditor->GetSelection().Remove(HitCurve.GetValue(), ECurvePointType::Key, KeyHandles);
			}
			else if (bIsControlDown)
			{
				CurveEditor->GetSelection().Toggle(HitCurve.GetValue(), ECurvePointType::Key, KeyHandles);
			}
			else
			{
				CurveEditor->GetSelection().Clear();
				CurveEditor->GetSelection().Add(HitCurve.GetValue(), ECurvePointType::Key, KeyHandles);
			}

			return FReply::Handled();
		}
	}

	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		FVector2D MousePixel = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		TOptional<FCurvePointHandle> MouseDownPoint = HitPoint(MousePixel);

		if(MouseDownPoint.IsSet())
		{
			if (FCurveModel* CurveModel = CurveEditor->FindCurve(MouseDownPoint->CurveID))
			{
				if (!CurveModel->IsReadOnly())
				{
					HandleDirectKeySelectionByMouse(CurveEditor, MouseEvent, MouseDownPoint);
				}
			}
		}
		
		CreateContextMenu(MyGeometry, MouseEvent);
		return FReply::Handled();
	}

	// If we hit a curve or another UI element, do not allow mouse input to bubble
	if (HitPoint(MousePosition) || GetHoveredCurve())
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SInteractiveCurveEditorView::CreateContextMenu(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	TSharedPtr<SCurveEditorPanel> EditorPanel = CurveEditor.IsValid() ? CurveEditor->GetPanel() : nullptr;
	if (!CurveEditor || !EditorPanel)
	{
		return;
	}

	FVector2D MousePosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	TOptional<FCurvePointHandle> MouseUpPoint = HitPoint(MousePosition);
	
	// We need to update our curve proximities (again) because OnMouseLeave is called (which clears them) 
	// before this menu is created due to the parent widget capturing mouse focus. The context menu needs
	// to know which curve you have highlighted for buffering curves.
	UpdateCurveProximities(MousePosition);

	// Rebind our context menu actions based on the results of hit-testing
	RebindContextualActions(MousePosition);

	// if (!MouseUpPoint.IsSet())
	// {
	// 	CurveEditor->Selection.Clear();
	// }

	const bool bCloseAfterSelection = true;
	FMenuBuilder MenuBuilder(bCloseAfterSelection, EditorPanel->GetCommands());
	
	BuildContextMenu(MenuBuilder, MouseUpPoint, GetHoveredCurve());

	// Push the context menu
	FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
	ActiveContextMenu = FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, MenuBuilder.MakeWidget(), FSlateApplication::Get().GetCursorPos(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
}

void SInteractiveCurveEditorView::BuildContextMenu(FMenuBuilder& MenuBuilder, TOptional<FCurvePointHandle> ClickedPoint, TOptional<FCurveModelID> HoveredCurveID)
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	if (CurveEditor)
	{
		FCurveEditorContextMenu::BuildMenu(MenuBuilder, CurveEditor.ToSharedRef(), ClickedPoint, HoveredCurveID);
	}
}

TOptional<FCurvePointHandle> SInteractiveCurveEditorView::HitPoint(FVector2D MousePixel) const
{
	TOptional<FCurvePointHandle> HitPoint;
	TOptional<float> ClosestDistance;

	TOptional<FCurveModelID> HoveredCurve = GetHoveredCurve();

	// Find all keys within the current hit test time
	for (const FCurveDrawParams& Params : CachedDrawParams)
	{
		// If we have a hovered curve, only hit a point within that curve
		if (HoveredCurve.IsSet() && Params.GetID() != HoveredCurve.GetValue())
		{
			continue;
		}
	
		for (int32 PointIndex = 0; PointIndex < Params.Points.Num(); PointIndex++)
		{
			const FCurvePointInfo& Point = Params.Points[PointIndex];
			const FKeyDrawInfo& PointDrawInfo = Params.GetKeyDrawInfo(Point.Type, PointIndex);

			// We artificially inflate the hit testing region for keys by a few pixels to make them easier to hit. The PointDrawInfo.ScreenSize specifies their drawn size,
			// so we need to inflate here when doing the actual hit testing. We subtract by half the extent to center it on the drawing.
			FVector2D HitTestSize = PointDrawInfo.ScreenSize + FVector2D(4.f, 4.f);

			FSlateRect KeyRect = FSlateRect::FromPointAndExtent(Point.ScreenPosition - (HitTestSize / 2.f), HitTestSize);

			if (KeyRect.ContainsPoint(MousePixel))
			{
				float DistanceSquared = (KeyRect.GetCenter() - MousePixel).SizeSquared();
				if (DistanceSquared <= ClosestDistance.Get(DistanceSquared))
				{
					ClosestDistance = DistanceSquared;
					HitPoint = FCurvePointHandle(Params.GetID(), Point.Type, Point.KeyHandle);
				}
			}
		}
	}

	return HitPoint;
}

void SInteractiveCurveEditorView::RebindContextualActions(FVector2D InMousePosition)
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	TSharedPtr<SCurveEditorPanel> CurveEditorPanel = CurveEditor.IsValid() ? CurveEditor->GetPanel() : nullptr;
	if (!CurveEditorPanel)
	{
		return;
	}

	TSharedPtr<FUICommandList> CommandList = CurveEditorPanel->GetCommands();

	CommandList->UnmapAction(FCurveEditorCommands::Get().AddKeyHovered);
	CommandList->UnmapAction(FCurveEditorCommands::Get().PasteKeysHovered);
	CommandList->UnmapAction(FCurveEditorCommands::Get().AddKeyToAllCurves);

	CommandList->UnmapAction(FCurveEditorCommands::Get().BufferVisibleCurves);
	CommandList->UnmapAction(FCurveEditorCommands::Get().SwapBufferedCurves);
	CommandList->UnmapAction(FCurveEditorCommands::Get().ApplyBufferedCurves);
	

	TOptional<FCurveModelID> HoveredCurve = GetHoveredCurve();
	if (HoveredCurve.IsSet())
	{
		TSet<FCurveModelID> HoveredCurveSet;
		HoveredCurveSet.Add(HoveredCurve.GetValue());

		CommandList->MapAction(FCurveEditorCommands::Get().AddKeyHovered, FExecuteAction::CreateSP(this, &SInteractiveCurveEditorView::AddKeyAtMousePosition, HoveredCurveSet));
		CommandList->MapAction(FCurveEditorCommands::Get().PasteKeysHovered, FExecuteAction::CreateSP(this, &SInteractiveCurveEditorView::PasteKeys, HoveredCurveSet));
	}

	CommandList->MapAction(FCurveEditorCommands::Get().AddKeyToAllCurves, FExecuteAction::CreateSP(this, &SInteractiveCurveEditorView::AddKeyAtScrubTime, TSet<FCurveModelID>()));

	// Buffer Curves. Can only act on buffered curves if curves are selected in the tree or the curve has selected keys.
	CommandList->MapAction(FCurveEditorCommands::Get().BufferVisibleCurves, FExecuteAction::CreateSP(this, &SInteractiveCurveEditorView::BufferCurves), FCanExecuteAction::CreateSP(this, &SInteractiveCurveEditorView::CanBufferedCurves));
	CommandList->MapAction(FCurveEditorCommands::Get().SwapBufferedCurves, FExecuteAction::CreateSP(this, &SInteractiveCurveEditorView::ApplyBufferCurves, true), FCanExecuteAction::CreateSP(this, &SInteractiveCurveEditorView::CanApplyBufferedCurves));
	CommandList->MapAction(FCurveEditorCommands::Get().ApplyBufferedCurves, FExecuteAction::CreateSP(this, &SInteractiveCurveEditorView::ApplyBufferCurves, false), FCanExecuteAction::CreateSP(this, &SInteractiveCurveEditorView::CanApplyBufferedCurves));
}

void SInteractiveCurveEditorView::BufferCurves()
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	if (CurveEditor.IsValid())
	{
		CurveEditor->AddBufferedCurves(CurveEditor->GetSelectionFromTreeAndKeys());
	}
}

void SInteractiveCurveEditorView::ApplyBufferCurves(const bool bSwapBufferCurves)
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	if (CurveEditor.IsValid())
	{
		CurveEditor->ApplyBufferedCurves(CurveEditor->GetSelectionFromTreeAndKeys(), bSwapBufferCurves);
	}
}

bool SInteractiveCurveEditorView::CanBufferedCurves() const
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	if (CurveEditor.IsValid())
	{
		return CurveEditor->GetSelectionFromTreeAndKeys().Num() > 0;
	}

	return false;
}

bool SInteractiveCurveEditorView::CanApplyBufferedCurves() const
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	if (CurveEditor.IsValid())
	{
		return CurveEditor->GetSelectionFromTreeAndKeys().Num() > 0 && CurveEditor->GetBufferedCurves().Num() > 0;
	}

	return false;
}

FKeyAttributes SInteractiveCurveEditorView::GetDefaultKeyAttributesForCurveTime(const FCurveEditor& CurveEditor, const FCurveModel& CurveModel, double EvalTime) const
{
	FKeyAttributes KeyAttributes = CurveEditor.GetDefaultKeyAttribute().Get();

	TPair<ERichCurveInterpMode, ERichCurveTangentMode> Modes = CurveModel.GetInterpolationMode(EvalTime, ERichCurveInterpMode::RCIM_Linear, ERichCurveTangentMode::RCTM_Auto);
	if (Modes.Key != ERichCurveInterpMode::RCIM_Linear)
	{
		KeyAttributes.SetInterpMode(Modes.Key);
	}
	if (Modes.Value != ERichCurveTangentMode::RCTM_Auto)
	{
		KeyAttributes.SetTangentMode(Modes.Value);
	}

	return KeyAttributes;
}

void SInteractiveCurveEditorView::AddKeyAtScrubTime(TSet<FCurveModelID> ForCurves)
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	if (!CurveEditor.IsValid())
	{
		return;
	}

	TSet<FCurveModelID> CurvesToAddTo;
	if (ForCurves.Num() == 0)
	{
		CurvesToAddTo = CurveEditor->GetEditedCurves();
	}
	else
	{
		CurvesToAddTo = ForCurves;
	}

	// If they don't have a time slider controller then we fall back to using mouse position.
	TSharedPtr<ITimeSliderController> TimeSliderController = CurveEditor->GetTimeSliderController();
	if (!TimeSliderController)
	{
		AddKeyAtMousePosition(CurvesToAddTo);
		return;
	}

	// Snapping of the time will be done inside AddKeyAtTime.
	double ScrubTime = TimeSliderController->GetScrubPosition() / TimeSliderController->GetTickResolution();
	AddKeyAtTime(CurvesToAddTo, ScrubTime);
}

void SInteractiveCurveEditorView::AddKeyAtMousePosition(TSet<FCurveModelID> ForCurves)
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	if (!CurveEditor.IsValid())
	{
		return;
	}

	// Snapping will be done inside AddKeyAtTime
	double MouseTime = GetViewSpace().ScreenToSeconds(CachedMousePosition.X);
	AddKeyAtTime(ForCurves, MouseTime);
}

void SInteractiveCurveEditorView::AddKeyAtTime(const TSet<FCurveModelID>& ToCurves, double InTime)
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	if (!CurveEditor.IsValid())
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("AddKeyAtTime", "Add Key"));
	bool bAddedKey = false;

	// Clear the selection set as we will be selecting all the new keys created.
	CurveEditor->GetSelection().Clear();

	for (const FCurveModelID& CurveModelID : ToCurves)
	{
		FCurveModel* CurveModel = CurveEditor->FindCurve(CurveModelID);
		check(CurveModel);

		if (CurveModel->IsReadOnly())
		{
			continue;
		}

		// Ensure the time is snapped if needed
		FCurveSnapMetrics SnapMetrics = CurveEditor->GetCurveSnapMetrics(CurveModelID);
		double SnappedTime = SnapMetrics.SnapInputSeconds(InTime);

		// Support optional input display offsets 
		double EvalTime = SnappedTime - CurveModel->GetInputDisplayOffset();

		double CurveValue = 0.0;
		if (CurveModel->Evaluate(EvalTime, CurveValue))
		{
			CurveModel->Modify();
			CurveValue = SnapMetrics.SnapOutput(CurveValue);

			// Curve Models allow us to create new keys ontop of existing keys which works, but causes some user confusion
			// Before we create a key, we instead check to see if there is already a key at this time, and if there is, we
			// add that key to the selection set instead. This solves issues with snapping causing keys to be created adjacent
			// to the mouse cursor (sometimes by a large amount).
			TArray<FKeyHandle> ExistingKeys;
			CurveModel->GetKeys(*CurveEditor, EvalTime - KINDA_SMALL_NUMBER, EvalTime + KINDA_SMALL_NUMBER, TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), ExistingKeys);
			
			TOptional<FKeyHandle> NewKey;

			if (ExistingKeys.Num() > 0)
			{
				NewKey = ExistingKeys[0];
			}
			else
			{
				const FKeyAttributes& KeyAttributes = GetDefaultKeyAttributesForCurveTime(*CurveEditor, *CurveModel, EvalTime);

				// Add a key on this curve
				NewKey = CurveModel->AddKey(FKeyPosition(EvalTime, CurveValue), KeyAttributes);
			}

			// Add the key to the selection set.
			if (NewKey.IsSet())
			{
				bAddedKey = true;
				CurveEditor->GetSelection().Add(FCurvePointHandle(CurveModelID, ECurvePointType::Key, NewKey.GetValue()));
			}
		}
	}

	if (!bAddedKey)
	{
		Transaction.Cancel();
	}
}

void SInteractiveCurveEditorView::PasteKeys(TSet<FCurveModelID> ToCurves)
{
	TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
	if (!CurveEditor.IsValid())
	{
		return;
	}

	CurveEditor->PasteKeys(ToCurves);
}

void SInteractiveCurveEditorView::OnCurveEditorToolChanged(FCurveEditorToolID InToolId)
{
	// We need to end drag-drop operations if they switch tools. Otherwise they can start
	// a marquee select, use the keyboard to switch to a diferent tool, and then the marquee
	// select finishes after the tool has had a chance to activate.
	if (DragOperation.IsSet())
	{
		// We have to cancel it instead of ending it because ending it needs mouse position and some other stuff.
		DragOperation->DragImpl->CancelDrag();
		DragOperation.Reset();
	}
}

#undef LOCTEXT_NAMESPACE // "SInteractiveCurveEditorView"
