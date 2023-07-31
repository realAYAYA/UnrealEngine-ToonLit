// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CurveModel.h"
#include "Curves/RichCurve.h"
#include "Internationalization/Text.h"
#include "RichCurveEditorModel.h"
#include "SGraphActionMenu.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Views/SCurveEditorViewStacked.h"
#include "Views/SInteractiveCurveEditorView.h"


// Forward Declarations
class UCurveFloat;
struct FWaveTableTransform;


UENUM()
enum class EWaveTableCurveSource : uint8
{
	Custom,
	Expression,
	Shared,
	Unset
};


namespace WaveTable
{
	namespace Editor
	{
		class WAVETABLEEDITOR_API FWaveTableCurveModel : public FRichCurveEditorModelRaw
		{
		public:
			static ECurveEditorViewID WaveTableViewId;

			FWaveTableCurveModel(FRichCurve& InRichCurve, UObject* InOwner, EWaveTableCurveSource InSource);

			const FText& GetAxesDescriptor() const;
			const UObject* GetParentObject() const;
			EWaveTableCurveSource GetSource() const;
			void Refresh(const FWaveTableTransform& InTransform, int32 InCurveIndex, bool bInIsBipolar);

			virtual ECurveEditorViewID GetViewId() const { return WaveTableViewId; }
			virtual bool IsReadOnly() const override;
			virtual FLinearColor GetColor() const override;
			virtual void GetValueRange(double& MinValue, double& MaxValue) const override;

			int32 GetCurveIndex() const { return CurveIndex; }
			bool GetIsBipolar() const { return bIsBipolar; }
			float GetFadeInRatio() const { return FadeInRatio; }
			float GetFadeOutRatio() const { return FadeOutRatio; }
			int32 GetNumSamples() const { return NumSamples; }

		protected:
			virtual void RefreshCurveDescriptorText(const FWaveTableTransform& InTransform, FText& OutShortDisplayName, FText& OutInputAxisName, FText& OutOutputAxisName);
			virtual FColor GetCurveColor() const;
			virtual bool GetPropertyEditorDisabled() const;
			virtual FText GetPropertyEditorDisabledText() const;

			TWeakObjectPtr<UObject> ParentObject;

		private:
			int32 CurveIndex = INDEX_NONE;
			int32 NumSamples = 0;

			EWaveTableCurveSource Source = EWaveTableCurveSource::Unset;

			bool bIsBipolar = false;

			float FadeInRatio = 0.0f;
			float FadeOutRatio = 0.0f;

			FText InputAxisName;
			FText AxesDescriptor;
		};

		class WAVETABLEEDITOR_API SViewStacked : public SCurveEditorViewStacked
		{
		public:
			void Construct(const FArguments& InArgs, TWeakPtr<FCurveEditor> InCurveEditor);

		protected:
			virtual void PaintView(
				const FPaintArgs& Args,
				const FGeometry& AllottedGeometry,
				const FSlateRect& MyCullingRect,
				FSlateWindowElementList& OutDrawElements,
				int32 BaseLayerId, 
				const FWidgetStyle& InWidgetStyle,
				bool bParentEnabled) const override;

			virtual void DrawViewGrids(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 BaseLayerId, ESlateDrawEffect DrawEffects) const override;
			virtual void DrawLabels(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 BaseLayerId, ESlateDrawEffect DrawEffects) const override;

			virtual void FormatInputLabel(const FWaveTableCurveModel& EditorModel, const FNumberFormattingOptions& InLabelFormat, FText& InOutLabel) const;
			virtual void FormatOutputLabel(const FWaveTableCurveModel& EditorModel, const FNumberFormattingOptions& InLabelFormat, FText& InOutLabel) const { }

		private:
			struct FGridDrawInfo
			{
				const FGeometry* AllottedGeometry;

				FPaintGeometry PaintGeometry;

				ESlateDrawEffect DrawEffects;
				FNumberFormattingOptions LabelFormat;

				TArray<FVector2D> LinePoints;

				FCurveEditorScreenSpace ScreenSpace;

			private:
				FLinearColor MajorGridColor;
				FLinearColor MinorGridColor;

				int32 BaseLayerId = INDEX_NONE;

				const FCurveModel* CurveModel = nullptr;

				double LowerValue = 0.0;
				double PixelBottom = 0.0;
				double PixelTop = 0.0;

			public:
				FGridDrawInfo(const FGeometry* InAllottedGeometry, const FCurveEditorScreenSpace& InScreenSpace, FLinearColor InGridColor, int32 InBaseLayerId);

				void SetCurveModel(const FCurveModel* InCurveModel);
				const FCurveModel* GetCurveModel() const;
				void SetLowerValue(double InLowerValue);
				int32 GetBaseLayerId() const;
				FLinearColor GetLabelColor() const;
				double GetLowerValue() const;
				FLinearColor GetMajorGridColor() const;
				FLinearColor GetMinorGridColor() const;
				double GetPixelBottom() const;
				double GetPixelTop() const;
			};

			void DrawViewGridLineX(FSlateWindowElementList& OutDrawElements, FGridDrawInfo& DrawInfo, ESlateDrawEffect DrawEffect, double OffsetAlpha, bool bIsMajor) const;
			void DrawViewGridLineY(const float VerticalLine, FSlateWindowElementList& OutDrawElements, FGridDrawInfo &DrawInfo, ESlateDrawEffect DrawEffects, const FText* Label, bool bIsMajor) const;
		};
	} // namespace Editor
} // namespace WaveTable
