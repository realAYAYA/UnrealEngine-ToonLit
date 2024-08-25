// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/DragAndDrop.h"
#include "Templates/SharedPointer.h"

class SAvaEaseCurvePresetGroup;
class SAvaEaseCurvePresetGroupItem;
struct FAvaEaseCurvePreset;

class FAvaEaseCurvePresetDragDropOperation : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FAvaEaseCurvePresetDragDropOperation, FDragDropOperation)

	FAvaEaseCurvePresetDragDropOperation(const TSharedPtr<SAvaEaseCurvePresetGroupItem>& InWidget, const TSharedPtr<FAvaEaseCurvePreset>& InPreset)
		: WidgetWeak(InWidget), Preset(InPreset)
	{}

	TSharedPtr<SAvaEaseCurvePresetGroupItem> GetWidget() const { return WidgetWeak.Pin(); }
	TSharedPtr<FAvaEaseCurvePreset> GetPreset() const { return Preset; }

	//~ Begin FDragDropOperation
	virtual FCursorReply OnCursorQuery() override;
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override;
	virtual void OnDrop(bool bInDropWasHandled, const FPointerEvent& InMouseEvent) override;
	//~ End FDragDropOperation

	void AddHoveredGroup(const TSharedRef<SAvaEaseCurvePresetGroup>& InGroupWidget);

protected:
	TWeakPtr<SAvaEaseCurvePresetGroupItem> WidgetWeak;
	TSharedPtr<FAvaEaseCurvePreset> Preset;

	TSet<TWeakPtr<SAvaEaseCurvePresetGroup>> HoveredGroupWidgets;
};
