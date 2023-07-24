// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "WaveformEditorDisplayUnit.h"
#include "WaveformEditorGridData.h"
#include "WaveformEditorSlateTypes.h"
#include "Widgets/SCompoundWidget.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTimeUnitMenuSelection, const EWaveformEditorDisplayUnit /* Requested Display Unit */);

class FWaveformEditorRenderData;
class FWaveformEditorTransportCoordinator;

class SWaveformEditorTimeRuler : public SCompoundWidget
{

	SLATE_BEGIN_ARGS(SWaveformEditorTimeRuler)
		: _DisplayUnit(EWaveformEditorDisplayUnit::Seconds)
	{
	}

	SLATE_ARGUMENT(EWaveformEditorDisplayUnit, DisplayUnit)

	SLATE_STYLE_ARGUMENT(FWaveformEditorTimeRulerStyle, Style)
	
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, TSharedRef<FWaveformEditorTransportCoordinator> InTransportCoordinator, TSharedRef<FWaveformEditorRenderData> InRenderData);
	void UpdateGridMetrics(const FWaveEditorGridMetrics& InMetrics);
	void UpdateDisplayUnit(const EWaveformEditorDisplayUnit InDisplayUnit);

	void OnStyleUpdated(const FWaveformEditorWidgetStyleBase* UpdatedStyle);

	/** Delegate sent when the user selects a new display unit from the RMB menu*/
	FOnTimeUnitMenuSelection OnTimeUnitMenuSelection;

private:
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	void DrawPlayheadHandle(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const;
	void DrawRulerTicks(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const;
	void DrawTickTimeString(float TickTimeSeconds, const double TickX, const double TickY, FSlateWindowElementList& OutDrawElements, int32& LayerId, const FGeometry& AllottedGeometry) const;

	TSharedRef<SWidget> MakeContextMenu();
	void MakeTimeUnitsSubMenu(FMenuBuilder& SubMenuBuilder);
	void NotifyTimeUnitMenuSelection(const EWaveformEditorDisplayUnit SelectedDisplayUnit) const;

	FWaveEditorGridMetrics GridMetrics;

	const FWaveformEditorTimeRulerStyle* Style = nullptr;

	FSlateColor BackgroundColor = FLinearColor::Black;
	FSlateBrush BackgroundBrush;
	FSlateBrush HandleBrush;
	FSlateColor HandleColor = FLinearColor(255.f, 0.1f, 0.2f, 1.f);
	FSlateColor TicksColor = FLinearColor(1.f, 1.f, 1.f, 0.9f);
	FSlateColor TicksTextColor = FLinearColor(1.f, 1.f, 1.f, 0.9f);
	float DesiredHeight = 0.f;
	float DesiredWidth = 0.f;
	float HandleWidth = 15.f;
	float TicksTextOffset = 5.f;
	FSlateFontInfo TicksTextFont;

	TSharedPtr<FWaveformEditorTransportCoordinator> TransportCoordinator = nullptr;
	TSharedPtr<FWaveformEditorRenderData> RenderData = nullptr;

	EWaveformEditorDisplayUnit DisplayUnit;
};

