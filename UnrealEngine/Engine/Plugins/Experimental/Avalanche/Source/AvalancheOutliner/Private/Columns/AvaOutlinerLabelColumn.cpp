// Copyright Epic Games, Inc. All Rights Reserved.

#include "Columns/AvaOutlinerLabelColumn.h"
#include "Item/IAvaOutlinerItem.h"

#define LOCTEXT_NAMESPACE "AvaOutlinerLabelColumn"

FText FAvaOutlinerLabelColumn::GetColumnDisplayNameText() const
{
	return LOCTEXT("LabelColumn", "Label");
}

SHeaderRow::FColumn::FArguments FAvaOutlinerLabelColumn::ConstructHeaderRowColumn()
{
	return SHeaderRow::Column(GetColumnId())
		.FillWidth(5.0f)
		.DefaultLabel(GetColumnDisplayNameText())
		.ShouldGenerateWidget(true);
}

TSharedRef<SWidget> FAvaOutlinerLabelColumn::ConstructRowWidget(FAvaOutlinerItemPtr InItem
	, const TSharedRef<FAvaOutlinerView>& InOutlinerView
	, const TSharedRef<SAvaOutlinerTreeRow>& InRow)
{
	return InItem->GenerateLabelWidget(InRow);
}

#undef LOCTEXT_NAMESPACE
