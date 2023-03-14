// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveTableCurveEditorViewStacked.h"

#include "Curves/CurveFloat.h"
#include "CurveEditor.h"
#include "CurveModel.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "SCurveEditorPanel.h"
#include "SGraphActionMenu.h"
#include "Styling/AppStyle.h"
#include "Views/SInteractiveCurveEditorView.h"
#include "WaveTableTransform.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WaveTableEditor"


namespace WaveTable
{
	namespace Editor
	{
		FWaveTableCurveModel::FWaveTableCurveModel(FRichCurve& InRichCurve, UObject* InOwner, EWaveTableCurveSource InSource)
			: FRichCurveEditorModelRaw(&InRichCurve, InOwner)
			, ParentObject(InOwner)
			, Source(InSource)
		{
			check(InOwner);
		}

		void FWaveTableCurveModel::Refresh(const FWaveTableTransform& InTransform, int32 InCurveIndex, bool bInIsBipolar)
		{
			// Must be set prior to remainder of refresh to avoid
			// child class implementation acting on incorrect cached index
			// (Editor may re-purpose models resulting in index change).
			CurveIndex = InCurveIndex;

			Color = GetCurveColor();

			FText OutputAxisName;
			RefreshCurveDescriptorText(InTransform, ShortDisplayName, InputAxisName, OutputAxisName);

			AxesDescriptor = FText::Format(LOCTEXT("WaveTableCurveDisplayTitle_AxesFormat", "X = {0}, Y = {1}"), InputAxisName, OutputAxisName);

			IntentionName = TEXT("AudioControlValue");
			SupportedViews = GetViewId();

			bKeyDrawEnabled = false;

			bIsBipolar = bInIsBipolar;

			if (InTransform.Curve == EWaveTableCurve::File)
			{
				NumSamples = InTransform.WaveTable.Num();
				FadeInRatio = InTransform.WaveTableSettings.FadeIn;
				FadeOutRatio = InTransform.WaveTableSettings.FadeOut;
			}
			else
			{
				NumSamples = 0;
				FadeInRatio = 0.0f;
				FadeOutRatio = 0.0f;
			}

			const bool bIsDisabled = GetPropertyEditorDisabled();
			if (bIsDisabled)
			{
				LongDisplayName = FText::Format(LOCTEXT("WaveTableCurveDisplay_BypassedNameFormat", "{0} ({1})"), ShortDisplayName, GetPropertyEditorDisabledText());
			}
			else
			{
				LongDisplayName = FText::Format(LOCTEXT("WaveTableCurveDisplayTitle_NameFormat", "{0}: {1}"), FText::AsNumber(CurveIndex), ShortDisplayName);

				if (Source == EWaveTableCurveSource::Custom)
				{
					bKeyDrawEnabled = true;
				}
				else if (Source == EWaveTableCurveSource::Shared)
				{
					bKeyDrawEnabled = true;

					if (const UCurveFloat* SharedCurve = InTransform.CurveShared)
					{
						const FText CurveSourceText = FText::FromString(SharedCurve->GetName());
						LongDisplayName = FText::Format(LOCTEXT("WaveTableCurveDisplayTitle_NameSharedFormat", "{0}: {1}, Shared Curve '{2}'"), FText::AsNumber(CurveIndex), ShortDisplayName, CurveSourceText);
					}
				}
			}
		}

		FLinearColor FWaveTableCurveModel::GetColor() const
		{
			return !GetPropertyEditorDisabled() && (Source == EWaveTableCurveSource::Custom)
				? Color
				: Color.Desaturate(0.35f);
		}

		void FWaveTableCurveModel::GetValueRange(double& MinValue, double& MaxValue) const
		{
			if (FadeInRatio > 0.0f || FadeOutRatio > 0.0f)
			{
				MaxValue = 1.0f;

				MinValue = bIsBipolar ? -1.0f : 0.0f;
			}
			else
			{
				FRichCurveEditorModelRaw::GetValueRange(MinValue, MaxValue);
			}
		}

		const FText& FWaveTableCurveModel::GetAxesDescriptor() const
		{
			return AxesDescriptor;
		}

		EWaveTableCurveSource FWaveTableCurveModel::GetSource() const
		{
			return Source;
		}

		const UObject* FWaveTableCurveModel::GetParentObject() const
		{
			return ParentObject.Get();
		}

		bool FWaveTableCurveModel::IsReadOnly() const
		{
			return Source != EWaveTableCurveSource::Custom;
		}

		ECurveEditorViewID FWaveTableCurveModel::WaveTableViewId = ECurveEditorViewID::Invalid;

		void FWaveTableCurveModel::RefreshCurveDescriptorText(const FWaveTableTransform& InTransform, FText& OutShortDisplayName, FText& OutInputAxisName, FText& OutOutputAxisName)
		{
			UEnum* Enum = StaticEnum<EWaveTableCurve>();
			check(Enum);

			Enum->GetDisplayValueAsText(InTransform.Curve, OutShortDisplayName);
			OutInputAxisName = LOCTEXT("WaveTableCurveModel_DefaultInputAxisName", "Unit Phase (Table Index)");
			OutOutputAxisName = LOCTEXT("WaveTableCurveModel_DefaultOutputAxisName", "Amplitude");
		}

		FColor FWaveTableCurveModel::GetCurveColor() const
		{
			return FColor { 232, 122, 0, 255 };
		}
		
		bool FWaveTableCurveModel::GetPropertyEditorDisabled() const
		{
			return false;
		}

		FText FWaveTableCurveModel::GetPropertyEditorDisabledText() const
		{
			return LOCTEXT("WaveTableCurveModel_Disabled", "Disabled");
		}

		void SViewStacked::Construct(const FArguments& InArgs, TWeakPtr<FCurveEditor> InCurveEditor)
		{
			SCurveEditorViewStacked::Construct(InArgs, InCurveEditor);

			StackedHeight = 300.0f;
			StackedPadding = 60.0f;
		}

		void SViewStacked::PaintView(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 BaseLayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
		{
			TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
			if (CurveEditor)
			{
				const ESlateDrawEffect DrawEffects = ShouldBeEnabled(bParentEnabled) ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

				DrawBackground(AllottedGeometry, OutDrawElements, BaseLayerId, DrawEffects);
				DrawViewGrids(AllottedGeometry, MyCullingRect, OutDrawElements, BaseLayerId, DrawEffects);
				DrawLabels(AllottedGeometry, MyCullingRect, OutDrawElements, BaseLayerId, DrawEffects);
				DrawCurves(CurveEditor.ToSharedRef(), AllottedGeometry, MyCullingRect, OutDrawElements, BaseLayerId, InWidgetStyle, DrawEffects);
			}
		}

		void SViewStacked::DrawLabels(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 BaseLayerId, ESlateDrawEffect DrawEffects) const
		{
			TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
			if (!CurveEditor)
			{
				return;
			}

			const double ValuePerPixel = 1.0 / StackedHeight;
			const double ValueSpacePadding = StackedPadding * ValuePerPixel;

			const FSlateFontInfo FontInfo = FAppStyle::GetFontStyle("CurveEd.LabelFont");
			const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
			const FCurveEditorScreenSpace ViewSpace = GetViewSpace();

			// Draw the curve labels for each view
			for (auto It = CurveInfoByID.CreateConstIterator(); It; ++It)
			{
				FCurveModel* Curve = CurveEditor->FindCurve(It.Key());
				if (!ensureAlways(Curve))
				{
					continue;
				}

				const int32  CurveIndexFromBottom = CurveInfoByID.Num() - It->Value.CurveIndex - 1;
				const double PaddingToBottomOfView = (CurveIndexFromBottom + 1) * ValueSpacePadding;

				const float PixelBottom = ViewSpace.ValueToScreen(CurveIndexFromBottom + PaddingToBottomOfView);
				const float PixelTop = ViewSpace.ValueToScreen(CurveIndexFromBottom + PaddingToBottomOfView + 1.0);

				if (!FSlateRect::DoRectanglesIntersect(MyCullingRect, TransformRect(AllottedGeometry.GetAccumulatedLayoutTransform(), FSlateRect(0, PixelTop, LocalSize.X, PixelBottom))))
				{
					continue;
				}

				const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

				// Render label
				FText Label = Curve->GetLongDisplayName();

				static const float LabelOffsetX = 15.0f;
				static const float LabelOffsetY = 35.0f;
				FVector2D LabelPosition(LabelOffsetX, PixelTop - LabelOffsetY);
				FPaintGeometry LabelGeometry = AllottedGeometry.ToPaintGeometry(FSlateLayoutTransform(LabelPosition));
				const FVector2D LabelSize = FontMeasure->Measure(Label, FontInfo);

				const uint32 LabelLayerId = BaseLayerId + CurveViewConstants::ELayerOffset::GridLabels;
				FSlateDrawElement::MakeText(OutDrawElements, LabelLayerId + 1, LabelGeometry, Label, FontInfo, DrawEffects, Curve->GetColor());

				// Render axes descriptor
				FText Descriptor = static_cast<FWaveTableCurveModel*>(Curve)->GetAxesDescriptor();

				const FVector2D DescriptorSize = FontMeasure->Measure(Descriptor, FontInfo);

				static const float LabelBufferX = 20.0f; // Keeps label and axes descriptor visually separated
				static const float GutterBufferX = 20.0f; // Accounts for potential scroll bar
				const float ViewWidth = ViewSpace.GetPhysicalWidth();
				const float FloatingDescriptorX = ViewWidth - DescriptorSize.X;
				LabelPosition = FVector2D(FMath::Max(LabelSize.X + LabelBufferX, FloatingDescriptorX - GutterBufferX), PixelTop - LabelOffsetY);
				LabelGeometry = AllottedGeometry.ToPaintGeometry(FSlateLayoutTransform(LabelPosition));

				FSlateDrawElement::MakeText(OutDrawElements, LabelLayerId + 1, LabelGeometry, Descriptor, FontInfo, DrawEffects, Curve->GetColor());
			}
		}

		void SViewStacked::DrawViewGrids(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 BaseLayerId, ESlateDrawEffect DrawEffects) const
		{
			TSharedPtr<FCurveEditor> CurveEditor = WeakCurveEditor.Pin();
			if (!CurveEditor)
			{
				return;
			}

			// Rendering info
			const float          Width = AllottedGeometry.GetLocalSize().X;
			const float          Height = AllottedGeometry.GetLocalSize().Y;
			const FSlateBrush*   WhiteBrush = FAppStyle::GetBrush("WhiteBrush");

			FGridDrawInfo DrawInfo(&AllottedGeometry, GetViewSpace(), CurveEditor->GetPanel()->GetGridLineTint(), BaseLayerId);

			TArray<float> MajorGridLinesX, MinorGridLinesX;
			TArray<FText> MajorGridLabelsX;

			GetGridLinesX(CurveEditor.ToSharedRef(), MajorGridLinesX, MinorGridLinesX, &MajorGridLabelsX);

			const double ValuePerPixel = 1.0 / StackedHeight;
			const double ValueSpacePadding = StackedPadding * ValuePerPixel;

			if (CurveInfoByID.IsEmpty())
			{
				return;
			}

			for (auto It = CurveInfoByID.CreateConstIterator(); It; ++It)
			{
				DrawInfo.SetCurveModel(CurveEditor->FindCurve(It.Key()));
				const FCurveModel* CurveModel = DrawInfo.GetCurveModel();
				if (!ensureAlways(CurveModel))
				{
					continue;
				}

				TArray<FText> CurveModelGridLabelsX = MajorGridLabelsX;
				check(MajorGridLinesX.Num() == CurveModelGridLabelsX.Num());

				if (const FWaveTableCurveModel* EditorModel = static_cast<const FWaveTableCurveModel*>(CurveModel))
				{
					for (int32 i = 0; i < CurveModelGridLabelsX.Num(); ++i)
					{
						FText& InputLabel = CurveModelGridLabelsX[i];
						FormatInputLabel(*EditorModel, DrawInfo.LabelFormat, InputLabel);
					}
				}

				const int32 Index = CurveInfoByID.Num() - It->Value.CurveIndex - 1;
				double Padding = (Index + 1) * ValueSpacePadding;
				DrawInfo.SetLowerValue(Index + Padding);

				FSlateRect BoundsRect(0, DrawInfo.GetPixelTop(), Width, DrawInfo.GetPixelBottom());
				if (!FSlateRect::DoRectanglesIntersect(MyCullingRect, TransformRect(AllottedGeometry.GetAccumulatedLayoutTransform(), BoundsRect)))
				{
					continue;
				}

				// Tint the views based on their curve color
				{
					FLinearColor CurveColorTint = DrawInfo.GetCurveModel()->GetColor().CopyWithNewOpacity(0.05f);
					const FPaintGeometry BoxGeometry = AllottedGeometry.ToPaintGeometry(
						FVector2D(Width, StackedHeight),
						FSlateLayoutTransform(FVector2D(0.f, DrawInfo.GetPixelTop()))
					);

					const int32 GridOverlayLayerId = DrawInfo.GetBaseLayerId() + CurveViewConstants::ELayerOffset::GridOverlays;
					FSlateDrawElement::MakeBox(OutDrawElements, GridOverlayLayerId, BoxGeometry, WhiteBrush, DrawEffects, CurveColorTint);
				}

				// Horizontal grid lines
				DrawInfo.LinePoints[0].X = 0.0;
				DrawInfo.LinePoints[1].X = Width;

				DrawViewGridLineX(OutDrawElements, DrawInfo, DrawEffects, 1.0  /* OffsetAlpha */, true  /* bIsMajor */);
				DrawViewGridLineX(OutDrawElements, DrawInfo, DrawEffects, 0.75 /* OffsetAlpha */, false /* bIsMajor */);
				DrawViewGridLineX(OutDrawElements, DrawInfo, DrawEffects, 0.5  /* OffsetAlpha */, true  /* bIsMajor */);
				DrawViewGridLineX(OutDrawElements, DrawInfo, DrawEffects, 0.25 /* OffsetAlpha */, false /* bIsMajor */);
				DrawViewGridLineX(OutDrawElements, DrawInfo, DrawEffects, 0.0  /* OffsetAlpha */, true  /* bIsMajor */);

				// Vertical grid lines
				{
					DrawInfo.LinePoints[0].Y = DrawInfo.GetPixelTop();
					DrawInfo.LinePoints[1].Y = DrawInfo.GetPixelBottom();

					// Draw major vertical grid lines
					for (int i = 0; i < MajorGridLinesX.Num(); ++i)
					{
						const float VerticalLine = FMath::RoundToFloat(MajorGridLinesX[i]);
						const FText& Label = CurveModelGridLabelsX[i];

						DrawViewGridLineY(VerticalLine, OutDrawElements, DrawInfo, DrawEffects, &Label, true /* bIsMajor */);
					}

					// Now draw the minor vertical lines which are drawn with a lighter color.
					for (float VerticalLine : MinorGridLinesX)
					{
						VerticalLine = FMath::RoundToFloat(VerticalLine);
						DrawViewGridLineY(VerticalLine, OutDrawElements, DrawInfo, DrawEffects, nullptr /* Label */, false /* bIsMajor */);
					}
				}

				if (const FWaveTableCurveModel* EditorModel = static_cast<const FWaveTableCurveModel*>(CurveModel))
				{
					auto DrawFade = [this, &CurveEditor, &DrawInfo, &OutDrawElements, DrawEffects, BaseLayerId, ModelID = It.Key()](float StartPosX, float EndPosX, bool bFadeIn, bool bIsBipolar)
					{
						const SCurveEditorView* View = CurveEditor->FindFirstInteractiveView(ModelID);
						FCurveEditorScreenSpace CurveSpace = View->GetCurveSpace(ModelID);

						StartPosX = CurveSpace.SecondsToScreen(StartPosX);
						EndPosX = CurveSpace.SecondsToScreen(EndPosX);
						static const FLinearColor FadeColor = FLinearColor::White.CopyWithNewOpacity(0.75f);

						DrawInfo.LinePoints[0].X = StartPosX;
						DrawInfo.LinePoints[0].Y = CurveSpace.ValueToScreen(0.0f);
						DrawInfo.LinePoints[1].X = EndPosX;
						DrawInfo.LinePoints[1].Y = CurveSpace.ValueToScreen(1.0f);
						const int32 FadeLayerId = BaseLayerId + CurveViewConstants::ELayerOffset::Curves;

						FSlateDrawElement::MakeLines(OutDrawElements, FadeLayerId, DrawInfo.PaintGeometry, DrawInfo.LinePoints, DrawEffects, FadeColor, false);

						if (bIsBipolar)
						{
							DrawInfo.LinePoints[1].Y = CurveSpace.ValueToScreen(-1.0f);
							FSlateDrawElement::MakeLines(OutDrawElements, FadeLayerId, DrawInfo.PaintGeometry, DrawInfo.LinePoints, DrawEffects, FadeColor, false);
						}

						const FSlateBrush* WhiteBrush = FAppStyle::GetBrush("WhiteBrush");

						const float Width = FMath::Abs(DrawInfo.LinePoints[1].X - DrawInfo.LinePoints[0].X);
						const float BoxStart = bFadeIn ? StartPosX : StartPosX - Width;
						const FPaintGeometry BoxGeometry = DrawInfo.AllottedGeometry->ToPaintGeometry(
							FVector2D(Width, StackedHeight),
							FSlateLayoutTransform(FVector2D(BoxStart, DrawInfo.GetPixelTop()))
						);

						const int32 GridOverlayLayerId = DrawInfo.GetBaseLayerId() + CurveViewConstants::ELayerOffset::GridOverlays;
						FSlateDrawElement::MakeBox(OutDrawElements, GridOverlayLayerId, BoxGeometry, WhiteBrush, DrawEffects, FadeColor.CopyWithNewOpacity(0.15f));
					};

					const bool bIsBipolar = EditorModel->GetIsBipolar();
					float FadeInRatio = EditorModel->GetFadeInRatio();
					if (FadeInRatio > 0.0f)
					{
						DrawFade(0.0f, FadeInRatio, true, bIsBipolar);
					}

					float FadeOutRatio = EditorModel->GetFadeOutRatio();
					if (FadeOutRatio > 0.0f)
					{
						DrawFade(1.0f, 1.0f - FadeOutRatio, false, bIsBipolar);
					}
				}
			}
		}

		void SViewStacked::DrawViewGridLineX(FSlateWindowElementList& OutDrawElements, FGridDrawInfo& DrawInfo, ESlateDrawEffect DrawEffects, double OffsetAlpha, bool bIsMajor) const
		{
			double ValueMin = 0.0f;
			double ValueMax = 0.0f;
			DrawInfo.GetCurveModel()->GetValueRange(ValueMin, ValueMax);

			const double LowerValue = DrawInfo.GetLowerValue();
			const double PixelBottom = DrawInfo.GetPixelBottom();
			const double PixelTop = DrawInfo.GetPixelTop();

			const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			const FSlateFontInfo FontInfo = FCoreStyle::Get().GetFontStyle("CurveEd.InfoFont");

			const uint32 LineLayerId = DrawInfo.GetBaseLayerId() + CurveViewConstants::ELayerOffset::GridLines;

			FLinearColor Color = bIsMajor ? DrawInfo.GetMajorGridColor() : DrawInfo.GetMinorGridColor();

			DrawInfo.LinePoints[0].Y = DrawInfo.LinePoints[1].Y = DrawInfo.ScreenSpace.ValueToScreen(LowerValue + OffsetAlpha);
			FSlateDrawElement::MakeLines(OutDrawElements, LineLayerId, DrawInfo.PaintGeometry, DrawInfo.LinePoints, DrawEffects, Color, false);

			FText Label = FText::AsNumber(FMath::Lerp(ValueMin, ValueMax, OffsetAlpha), &DrawInfo.LabelFormat);

			if (const FWaveTableCurveModel* CurveModel = static_cast<const FWaveTableCurveModel*>(DrawInfo.GetCurveModel()))
			{
				FormatOutputLabel(*CurveModel, DrawInfo.LabelFormat, Label);
			}

			const FVector2D LabelSize = FontMeasure->Measure(Label, FontInfo);
			double LabelY = FMath::Lerp(PixelBottom, PixelTop, OffsetAlpha);
	
			// Offset label Y position below line only if the top gridline,
			// otherwise push above
			if (FMath::IsNearlyEqual(OffsetAlpha, 1.0))
			{
				LabelY += 5.0;
			}
			else
			{
				LabelY -= 15.0;
			}

			const FPaintGeometry LabelGeometry = DrawInfo.AllottedGeometry->ToPaintGeometry(FSlateLayoutTransform(FVector2D(CurveViewConstants::CurveLabelOffsetX, LabelY)));
			const uint32 LabelLayerId = DrawInfo.GetBaseLayerId() + CurveViewConstants::ELayerOffset::GridLabels;

			FSlateDrawElement::MakeText(
				OutDrawElements,
				LabelLayerId,
				LabelGeometry,
				Label,
				FontInfo,
				DrawEffects,
				DrawInfo.GetLabelColor()
			);
		}

		void SViewStacked::DrawViewGridLineY(const float VerticalLine, FSlateWindowElementList& OutDrawElements, FGridDrawInfo& DrawInfo, ESlateDrawEffect DrawEffects, const FText* Label, bool bIsMajor) const
		{
			const float Width = DrawInfo.AllottedGeometry->GetLocalSize().X;
			if (VerticalLine >= 0 || VerticalLine <= FMath::RoundToFloat(Width))
			{
				const FLinearColor LineColor = bIsMajor ? DrawInfo.GetMajorGridColor() : DrawInfo.GetMinorGridColor();

				DrawInfo.LinePoints[0].X = DrawInfo.LinePoints[1].X = VerticalLine;
				const uint32 LineLayerId = DrawInfo.GetBaseLayerId() + CurveViewConstants::ELayerOffset::GridLines;
				FSlateDrawElement::MakeLines(OutDrawElements, LineLayerId, DrawInfo.PaintGeometry, DrawInfo.LinePoints, DrawEffects, LineColor, false);

				if (Label)
				{
					const FSlateFontInfo FontInfo = FCoreStyle::Get().GetFontStyle("CurveEd.InfoFont");

					const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
					const FVector2D LabelSize = FontMeasure->Measure(*Label, FontInfo);
					const FPaintGeometry LabelGeometry = DrawInfo.AllottedGeometry->ToPaintGeometry(FSlateLayoutTransform(FVector2D(VerticalLine, DrawInfo.LinePoints[0].Y - LabelSize.Y * 1.2f)));
					const uint32 LabelLayerId = DrawInfo.GetBaseLayerId() + CurveViewConstants::ELayerOffset::GridLabels;

					FSlateDrawElement::MakeText(
						OutDrawElements,
						LabelLayerId,
						LabelGeometry,
						*Label,
						FontInfo,
						DrawEffects,
						DrawInfo.GetLabelColor()
					);
				}
			}
		}

		void SViewStacked::FormatInputLabel(const FWaveTableCurveModel& EditorModel, const FNumberFormattingOptions& InLabelFormat, FText& InOutLabel) const
		{
			const int32 NumSamples = EditorModel.GetNumSamples();
			if (NumSamples != 0)
			{
				const float Value = FCString::Atof(*InOutLabel.ToString());
				if (Value >= 0 && Value <= 1.0f)
				{
					const int32 ValueTrunc = FMath::FloorToInt32(NumSamples * Value);
					InOutLabel = FText::Format(LOCTEXT("WaveTableCurveEditor_InputLabelFormat", "{0} ({1})"), InOutLabel, FText::AsNumber(ValueTrunc));
				}
			}
		}

		SViewStacked::FGridDrawInfo::FGridDrawInfo(const FGeometry* InAllottedGeometry, const FCurveEditorScreenSpace& InScreenSpace, FLinearColor InGridColor, int32 InBaseLayerId)
			: AllottedGeometry(InAllottedGeometry)
			, ScreenSpace(InScreenSpace)
			, BaseLayerId(InBaseLayerId)
			, CurveModel(nullptr)
			, LowerValue(0)
			, PixelBottom(0)
			, PixelTop(0)
		{
			check(AllottedGeometry);

			// Pre-allocate an array of line points to draw our vertical lines. Each major grid line
			// will overwrite the X value of both points but leave the Y value untouched so they draw from the bottom to the top.
			LinePoints.Add(FVector2D(0.f, 0.f));
			LinePoints.Add(FVector2D(0.f, 0.f));

			MajorGridColor = InGridColor;
			MinorGridColor = InGridColor.CopyWithNewOpacity(InGridColor.A * 0.5f);

			PaintGeometry = InAllottedGeometry->ToPaintGeometry();

			LabelFormat.SetMaximumFractionalDigits(2);
		}

		void SViewStacked::FGridDrawInfo::SetCurveModel(const FCurveModel* InCurveModel)
		{
			CurveModel = InCurveModel;
		}

		const FCurveModel* SViewStacked::FGridDrawInfo::GetCurveModel() const
		{
			return CurveModel;
		}

		void SViewStacked::FGridDrawInfo::SetLowerValue(double InLowerValue)
		{
			LowerValue = InLowerValue;
			PixelBottom = ScreenSpace.ValueToScreen(InLowerValue);
			PixelTop = ScreenSpace.ValueToScreen(InLowerValue + 1.0);
		}

		int32 SViewStacked::FGridDrawInfo::GetBaseLayerId() const
		{
			return BaseLayerId;
		}

		FLinearColor SViewStacked::FGridDrawInfo::GetLabelColor() const
		{
			check(CurveModel);
			return GetCurveModel()->GetColor().CopyWithNewOpacity(0.7f);
		}

		double SViewStacked::FGridDrawInfo::GetLowerValue() const
		{
			return LowerValue;
		}

		FLinearColor SViewStacked::FGridDrawInfo::GetMajorGridColor() const
		{
			return MajorGridColor;
		}

		FLinearColor SViewStacked::FGridDrawInfo::GetMinorGridColor() const
		{
			return MinorGridColor;
		}

		double SViewStacked::FGridDrawInfo::GetPixelBottom() const
		{
			return PixelBottom;
		}

		double SViewStacked::FGridDrawInfo::GetPixelTop() const
		{
			return PixelTop;
		}
	} // namespace Editor
} // namespace WaveTable

#undef LOCTEXT_NAMESPACE