// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaEaseCurvePresetDragDropOp.h"
#include "EaseCurveTool/AvaEaseCurvePreset.h"
#include "EaseCurveTool/Widgets/SAvaEaseCurvePresetGroup.h"
#include "EaseCurveTool/Widgets/SAvaEaseCurvePresetGroupItem.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"

FCursorReply FAvaEaseCurvePresetDragDropOperation::OnCursorQuery()
{
	return FCursorReply::Cursor(EMouseCursor::GrabHandClosed);
}

TSharedPtr<SWidget> FAvaEaseCurvePresetDragDropOperation::GetDefaultDecorator() const
{
	if (!WidgetWeak.IsValid())
	{
		return FDragDropOperation::GetDefaultDecorator();
	}

	return SNew(SBorder)
		.Padding(2.f)
		.BorderImage(FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Border")))
		.Content()
		[
			WidgetWeak.Pin().ToSharedRef()
		];
}

void FAvaEaseCurvePresetDragDropOperation::OnDrop(bool bInDropWasHandled, const FPointerEvent& InMouseEvent)
{
	FDragDropOperation::OnDrop(bInDropWasHandled, InMouseEvent);

	if (const TSharedPtr<SAvaEaseCurvePresetGroupItem> Widget = WidgetWeak.Pin())
	{
		Widget->TriggerEndMove();
	}

	for (const TWeakPtr<SAvaEaseCurvePresetGroup>& GroupWidgetWeak : HoveredGroupWidgets)
	{
		if (const TSharedPtr<SAvaEaseCurvePresetGroup> GroupWidget = GroupWidgetWeak.Pin())
		{
			GroupWidget->ResetDragBorder();
		}
	}
}

void FAvaEaseCurvePresetDragDropOperation::AddHoveredGroup(const TSharedRef<SAvaEaseCurvePresetGroup>& InGroupWidget)
{
	HoveredGroupWidgets.Add(InGroupWidget);
}
