// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/NumericTypeInterface.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ITimeSlider.h"
#include "ITransportControl.h"
#include "Framework/Commands/Commands.h"
#include "MLDeformerEditorToolkit.h"

struct FPaintPlaybackRangeArgs;
class SSearchBox;
class UAnimSingleNodeInstance;
enum class EViewRangeInterpolation;
enum class EFrameNumberDisplayFormats : uint8;

namespace UE::MLDeformer
{
	class FMLDeformerEditorToolkit;
	class FMLDeformerEditorModel;
	class FMLTimeSliderController;
	class SAnimTimelineTransportControls; 

	/**
	 * Implements a timeline widget for the ML Deformer editor.
	 */
	class SMLDeformerTimeline : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SMLDeformerTimeline) {}
			/** The current total range of frame indices. */
			SLATE_ATTRIBUTE(FInt32Interval, ViewIndices)
			/** Called when any widget contained within the anim timeline has received focus. */
			SLATE_EVENT(FSimpleDelegate, OnReceivedFocus)
		SLATE_END_ARGS()

	public:
		/**
		 * Construct this widget.
		 * @param InArgs The declaration data for this widget.
		 * @param InModel The model for the anim timeline.
		 */
		void Construct(const FArguments& InArgs, FMLDeformerEditorToolkit* InEditor);

		// SWidget interface.
		virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
		// End SWidget interface.

		/** Compute a major grid interval and number of minor divisions to display. */
		bool GetGridMetrics(float PhysicalWidth, double& OutMajorInterval, int32& OutMinorDivisions) const;

		/** Get the time slider controller. */
		TSharedPtr<ITimeSliderController> GetTimeSliderController() const;

		void SetModel(TWeakPtr<FMLDeformerEditorModel> InModel);

	private:
		/**
		 * @return The fill percentage of the animation outliner.
		 */
		float GetColumnFillCoefficient(int32 ColumnIndex) const { return ColumnFillCoefficients[ColumnIndex]; }

		/** Get numeric Type interface for converting between frame numbers and display formats. */
		TSharedRef<INumericTypeInterface<double>> GetNumericTypeInterface() const;

		/** Called when a column fill percentage is changed by a splitter slot. */
		void OnColumnFillCoefficientChanged(float FillCoefficient, int32 ColumnIndex);

		/** Handles an additive layer key being set. */
		void HandleScrubPositionChanged(FFrameTime NewScrubPosition, bool bIsScrubbing, bool bEvaluate);
		void HandleViewRangeChanged(TRange<double> InRange, EViewRangeInterpolation InInterpolation);

		double GetSpinboxDelta() const;
		void SetPlayTime(double InFrameTime);
		void SetDisplayFormat(EFrameNumberDisplayFormats InFormat);
		bool IsDisplayFormatChecked(EFrameNumberDisplayFormats InFormat) const;

	private:
		/** Anim timeline model. */
		TWeakPtr<FMLDeformerEditorModel> Model;

		/** The currently active editor model. */
		UE::MLDeformer::FMLDeformerEditorToolkit* Editor = nullptr;

		/** The time slider controller. */
		TSharedPtr<FMLTimeSliderController> TimeSliderController;

		/** The anim timeline transport controls, which contains the forward, backward and step actions. */
		TSharedPtr<SAnimTimelineTransportControls> TransportControls;

		/** The top time slider widget. */
		TSharedPtr<ITimeSlider> TopTimeSlider;

		/** The fill coefficients of each column in the grid. */
		float ColumnFillCoefficients[2] = { 0.2f, 0.8f };

		/** Called when the user has begun dragging the selection selection range. */
		FSimpleDelegate OnSelectionRangeBeginDrag;

		/** Called when the user has finished dragging the selection selection range. */
		FSimpleDelegate OnSelectionRangeEndDrag;

		/** Called when any widget contained within the anim timeline has received focus. */
		FSimpleDelegate OnReceivedFocus;

		/** Numeric Type interface for converting between frame numbers and display formats. */
		TSharedPtr<INumericTypeInterface<double>> NumericTypeInterface;

		/** The view range. */
		TAttribute<FAnimatedRange> ViewRange;

		/** Filter text used to search the tree. */
		FText FilterText;
	};

}// namespace UE::MLDeformer
