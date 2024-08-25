// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MaterialEditingLibrary.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SCompoundWidget.h"

class SDMEditor;
class UDynamicMaterialModel;

class SDMStatusBar : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SDMStatusBar) {}
	SLATE_END_ARGS()

public:
	virtual ~SDMStatusBar() {}

	void Construct(const FArguments& InArgs, const TWeakObjectPtr<UDynamicMaterialModel>& InEditorModel, const TSharedRef<SDMEditor>& InEditorWidget);

protected:
	TWeakObjectPtr<UDynamicMaterialModel> EditorModelWeak;
	TWeakPtr<SDMEditor> EditorWidgetWeak;

	int32 CachedSlotCount = 0;
	int32 CachedCurrentLayerCount = 0;
	int32 CachedTotalLayerCount = 0;

	FMaterialStatistics CachedMaterialStats;

	SWrapBox::FSlot::FSlotArguments CreateStatsWrapBoxEntry(TAttribute<FText> InText, const FText& InTooltipText);

	FText GetNumMaterialSlotsText() const;
	FText GetNumTotalLayersText() const;
	FText GetNumCurrentSlotLayersText() const;

	FText GetNumPixelShaderInstructionsText() const;
	FText GetNumVertexShaderInstructionsText() const;
	FText GetNumSamplersText() const;
	FText GetNumPixelTextureSamplesText() const;
	FText GetNumVertexTextureSamplesText() const;
	FText GetNumVirtualTextureSamplesText() const;
	FText GetNumUVScalarsText() const;
	FText GetNumInterpolatorScalarsText() const;
};
