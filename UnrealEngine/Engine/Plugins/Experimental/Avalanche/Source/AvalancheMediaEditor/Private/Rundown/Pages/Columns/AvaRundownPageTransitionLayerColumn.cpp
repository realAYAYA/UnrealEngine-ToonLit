// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaRundownPageTransitionLayerColumn.h"

#include "Rundown/Pages/PageViews/IAvaRundownPageView.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "AvaRundownPageTransitionLayerColumn"

FText FAvaRundownPageTransitionLayerColumn::GetColumnDisplayNameText() const
{
	return LOCTEXT("TransitionLayerColumn_LayerName", "Layer");
}

FText FAvaRundownPageTransitionLayerColumn::GetColumnToolTipText() const
{
	return LOCTEXT("TransitionLayerColumn_ToolTip", "Transition layer name for the page");
}

SHeaderRow::FColumn::FArguments FAvaRundownPageTransitionLayerColumn::ConstructHeaderRowColumn()
{
	return SHeaderRow::Column(GetColumnId())
		.DefaultLabel(GetColumnDisplayNameText())
		.DefaultTooltip(GetColumnToolTipText())
		.FillWidth(0.25f)
		.ShouldGenerateWidget(true)
		.VAlignCell(EVerticalAlignment::VAlign_Center)
	;
}

TSharedRef<SWidget> FAvaRundownPageTransitionLayerColumn::ConstructRowWidget(const FAvaRundownPageViewRef& InPageView, const TSharedPtr<SAvaRundownPageViewRow>& InRow)
{
	return SNew(SBox)
		.Padding(UE::AvaRundown::FEditorMetrics::ColumnLeftOffset, 0.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(InPageView, &IAvaRundownPageView::GetPageTransitionLayerNameText)
		];
}

#undef LOCTEXT_NAMESPACE
