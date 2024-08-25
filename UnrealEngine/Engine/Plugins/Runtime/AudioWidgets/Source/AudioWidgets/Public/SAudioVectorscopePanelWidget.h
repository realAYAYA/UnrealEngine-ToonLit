// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioVectorscopePanelStyle.h"
#include "AudioWidgetsEnums.h"
#include "Brushes/SlateColorBrush.h"
#include "IFixedSampledSequenceViewReceiver.h"
#include "SampledSequenceDrawingUtils.h"
#include "SSampledSequenceValueGridOverlay.h"
#include "Widgets/SCompoundWidget.h"

class SAudioRadialSlider;
class SBorder;
class SFixedSampledSequenceViewer;
class SFixedSampledSequenceVectorViewer;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTimeWindowValueChanged, const float /*InTimeWindowMs*/)

class AUDIOWIDGETS_API SAudioVectorscopePanelWidget : public SCompoundWidget, public IFixedSampledSequenceViewReceiver
{
public:
	SLATE_BEGIN_ARGS(SAudioVectorscopePanelWidget)
		: _HideGrid(false)
		, _ValueGridMaxDivisionParameter(2)
		, _PanelLayoutType(EAudioPanelLayoutType::Basic)
		, _PanelStyle(&FAudioVectorscopePanelStyle::GetDefault())
	{}
		
		SLATE_DEFAULT_SLOT(FArguments, InArgs)

		/** Whether the value grid should be drawn or not */
		SLATE_ATTRIBUTE(bool, HideGrid)

		/** Maximum number of divisions in the value grid */
		SLATE_ARGUMENT(uint32, ValueGridMaxDivisionParameter)

		/** If we want to set the basic or advanced layout */
		SLATE_ARGUMENT(EAudioPanelLayoutType, PanelLayoutType)

		/** Vectorscope Panel widget style */
		SLATE_STYLE_ARGUMENT(FAudioVectorscopePanelStyle, PanelStyle)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FFixedSampledSequenceView& InData);

	void BuildWidget(const FFixedSampledSequenceView& InData, const EAudioPanelLayoutType InPanelLayoutType);

	// IFixedSampledSequenceViewReceiver Interface
	virtual void ReceiveSequenceView(const FFixedSampledSequenceView InData, const uint32 FirstSampleIndex) override;

	void SetGridVisibility(const bool InbIsVisible);
	void SetValueGridOverlayMaxNumDivisions(const uint32 InGridMaxNumDivisions);

	void SetVectorViewerScaleFactor(const float InScaleFactor);

	void UpdateValueGridOverlayStyle(const FSampledSequenceValueGridOverlayStyle UpdatedValueGridOverlayStyle);
	void UpdateSequenceVectorViewerStyle(const FSampledSequenceVectorViewerStyle UpdatedSequenceVectorViewerStyle);

	EAudioPanelLayoutType GetPanelLayoutType() { return PanelLayoutType; }

	FOnTimeWindowValueChanged OnTimeWindowValueChanged;

private:
	void CreateLayout();

	// Basic panel methods
	void CreateBackground(const FSampledSequenceVectorViewerStyle& VectorViewerStyle);

	TSharedPtr<SSampledSequenceValueGridOverlay> CreateValueGridOverlay(const uint32 MaxDivisionParameter,
		const SampledSequenceValueGridOverlay::EGridDivideMode DivideMode,
		const FSampledSequenceValueGridOverlayStyle& ValueGridStyle,
		const SampledSequenceDrawingUtils::ESampledSequenceDrawOrientation GridOrientation);

	void CreateSequenceVectorViewer(const FFixedSampledSequenceView& InData, const FSampledSequenceVectorViewerStyle& VectorViewerStyle);

	// Advanced panel methods
	void CreateTimeWindowKnob();
	void CreateScaleKnob();
	void CreateVectorscopeControls();

	const FAudioVectorscopePanelStyle* PanelStyle;

	// Basic panel widgets
	TSharedPtr<SBorder> BackgroundBorder;
	TSharedPtr<SFixedSampledSequenceVectorViewer> SequenceVectorViewer;
	TSharedPtr<SSampledSequenceValueGridOverlay> ValueGridOverlayXAxis;
	TSharedPtr<SSampledSequenceValueGridOverlay> ValueGridOverlayYAxis;

	// Advanced panel widgets
	TSharedPtr<SAudioRadialSlider> TimeWindowKnob;
	TSharedPtr<SAudioRadialSlider> ScaleKnob;

	EAudioPanelLayoutType PanelLayoutType = EAudioPanelLayoutType::Basic;

	uint32 ValueGridMaxDivisionParameter = 2;

	bool bIsInputWidgetTransacting = false;

	float TimeWindowValue = 0.0f;
	float ScaleValue      = 0.0f;

	float VectorscopeViewProportion = 1.0f;

	FFixedSampledSequenceView DataView;

	bool bHideValueGrid = false;
};
