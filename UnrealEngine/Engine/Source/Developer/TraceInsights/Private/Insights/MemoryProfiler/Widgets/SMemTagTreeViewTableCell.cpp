// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMemTagTreeViewTableCell.h"

#include "SlateOptMacros.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SExpanderArrow.h"

// Insights
#include "Insights/InsightsStyle.h"
#include "Insights/Table/ViewModels/Table.h"
#include "Insights/Table/ViewModels/TableColumn.h"
#include "Insights/MemoryProfiler/Widgets/SMemTagTreeViewTableRow.h"

#define LOCTEXT_NAMESPACE "SMemTagTreeViewView"

////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SMemTagTreeViewTableCell::Construct(const FArguments& InArgs, const TSharedRef<ITableRow>& TableRow)
{
	TablePtr = InArgs._TablePtr;
	ColumnPtr = InArgs._ColumnPtr;
	MemTagNodePtr = InArgs._MemTagNodePtr;

	ensure(TablePtr.IsValid());
	ensure(ColumnPtr.IsValid());
	ensure(MemTagNodePtr.IsValid());

	SetHoveredCellDelegate = InArgs._OnSetHoveredCell;

	ChildSlot
	[
		GenerateWidgetForColumn(InArgs, TableRow)
	];
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> SMemTagTreeViewTableCell::GenerateWidgetForColumn(const FArguments& InArgs, const TSharedRef<ITableRow>& TableRow)
{
	if (InArgs._IsNameColumn)
	{
		return GenerateWidgetForNameColumn(InArgs, TableRow);
	}
	else
	{
		return GenerateWidgetForStatsColumn(InArgs, TableRow);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> SMemTagTreeViewTableCell::GenerateWidgetForNameColumn(const FArguments& InArgs, const TSharedRef<ITableRow>& TableRow)
{
	return
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		[
			SNew(SExpanderArrow, TableRow)
		]

		// Info icon + tooltip
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Visibility(this, &SMemTagTreeViewTableCell::GetHintIconVisibility)
			.Image(FInsightsStyle::GetBrush("Icons.Hint.TreeItem"))
			.ToolTip(GetRowToolTip(TableRow))
		]

		// Color box
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.Visibility(this, &SMemTagTreeViewTableCell::GetBoxVisibility)
			.WidthOverride(14.0f)
			.HeightOverride(14.0f)
			[
				SNew(SBorder)
				.BorderImage(FInsightsStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(this, &SMemTagTreeViewTableCell::GetBoxColorAndOpacity)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
			]
		]

		// Name
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		.Padding(FMargin(2.0f, 0.0f))
		[
			SNew(STextBlock)
			.Text(this, &SMemTagTreeViewTableCell::GetDisplayName)
			.HighlightText(InArgs._HighlightText)
			.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.Tooltip"))
			.ColorAndOpacity(this, &SMemTagTreeViewTableCell::GetColorAndOpacity)
			.ShadowColorAndOpacity(this, &SMemTagTreeViewTableCell::GetShadowColorAndOpacity)
		]

		// Name Suffix
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		.Padding(FMargin(2.0f, 0.0f))
		[
			SNew(STextBlock)
			.Visibility(this, &SMemTagTreeViewTableCell::HasExtraDisplayName)
			.Text(this, &SMemTagTreeViewTableCell::GetExtraDisplayName)
			.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.Tooltip"))
			.ColorAndOpacity(this, &SMemTagTreeViewTableCell::GetExtraColorAndOpacity)
			.ShadowColorAndOpacity(this, &SMemTagTreeViewTableCell::GetShadowColorAndOpacity)
		]
	;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<IToolTip> SMemTagTreeViewTableCell::GetRowToolTip(const TSharedRef<ITableRow>& TableRow) const
{
	TSharedRef<SMemTagTreeViewTableRow> Row = StaticCastSharedRef<SMemTagTreeViewTableRow, ITableRow>(TableRow);
	return Row->GetRowToolTip();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText SMemTagTreeViewTableCell::GetValueAsText() const
{
	return MemTagNodePtr->IsGroup() ? FText::GetEmpty() : ColumnPtr->GetValueAsText(*MemTagNodePtr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> SMemTagTreeViewTableCell::GenerateWidgetForStatsColumn(const FArguments& InArgs, const TSharedRef<ITableRow>& TableRow)
{
	return
		SNew(SHorizontalBox)

		// Value
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		.HAlign(ColumnPtr->GetHorizontalAlignment())
		.Padding(FMargin(2.0f, 0.0f))
		[
			SNew(STextBlock)
			.Text(this, &SMemTagTreeViewTableCell::GetValueAsText)
			.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.Tooltip"))
			.ColorAndOpacity(this, &SMemTagTreeViewTableCell::GetStatsColorAndOpacity)
			.ShadowColorAndOpacity(this, &SMemTagTreeViewTableCell::GetShadowColorAndOpacity)
		]
	;
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
