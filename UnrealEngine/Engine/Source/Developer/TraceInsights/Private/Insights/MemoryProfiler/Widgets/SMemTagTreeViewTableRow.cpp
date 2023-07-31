// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMemTagTreeViewTableRow.h"

#include "SlateOptMacros.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"

// Insights
#include "Insights/Common/TimeUtils.h"
#include "Insights/InsightsStyle.h"
#include "Insights/Table/ViewModels/Table.h"
#include "Insights/Table/ViewModels/TableColumn.h"
#include "Insights/MemoryProfiler/Widgets/SMemTagTreeViewTableCell.h"
#include "Insights/MemoryProfiler/Widgets/SMemTagTreeViewTooltip.h"

#define LOCTEXT_NAMESPACE "SMemTagTreeViewView"

////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SMemTagTreeViewTableRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	OnShouldBeEnabled = InArgs._OnShouldBeEnabled;
	IsColumnVisibleDelegate = InArgs._OnIsColumnVisible;
	GetColumnOutlineHAlignmentDelegate = InArgs._OnGetColumnOutlineHAlignmentDelegate;
	SetHoveredCellDelegate = InArgs._OnSetHoveredCell;

	HighlightText = InArgs._HighlightText;
	HighlightedNodeName = InArgs._HighlightedNodeName;

	TablePtr = InArgs._TablePtr;
	MemTagNodePtr = InArgs._MemTagNodePtr;

	RowToolTip = MakeShared<SMemCounterTableRowToolTip>(MemTagNodePtr);

	SetEnabled(TAttribute<bool>(this, &SMemTagTreeViewTableRow::HandleShouldBeEnabled));

	SMultiColumnTableRow<FMemTagNodePtr>::Construct(SMultiColumnTableRow<FMemTagNodePtr>::FArguments(), InOwnerTableView);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> SMemTagTreeViewTableRow::GenerateWidgetForColumn(const FName& ColumnId)
{
	TSharedPtr<Insights::FTableColumn> ColumnPtr = TablePtr->FindColumnChecked(ColumnId);

	return
		SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)

		+SOverlay::Slot()
		.Padding(0.0f)
		[
			SNew(SImage)
			.Image(FInsightsStyle::GetBrush("TreeTable.RowBackground"))
			.ColorAndOpacity(this, &SMemTagTreeViewTableRow::GetBackgroundColorAndOpacity)
		]

		+SOverlay::Slot()
		.Padding(0.0f)
		[
			SNew(SImage)
			.Image(this, &SMemTagTreeViewTableRow::GetOutlineBrush, ColumnId)
			.ColorAndOpacity(this, &SMemTagTreeViewTableRow::GetOutlineColorAndOpacity)
		]

		+SOverlay::Slot()
		[
			SNew(SMemTagTreeViewTableCell, SharedThis(this))
			.Visibility(this, &SMemTagTreeViewTableRow::IsColumnVisible, ColumnId)
			.TablePtr(TablePtr)
			.ColumnPtr(ColumnPtr)
			.MemTagNodePtr(MemTagNodePtr)
			.HighlightText(HighlightText)
			.IsNameColumn(ColumnPtr->IsHierarchy())
			.OnSetHoveredCell(this, &SMemTagTreeViewTableRow::OnSetHoveredCell)
		];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

FReply SMemTagTreeViewTableRow::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return SMultiColumnTableRow<FMemTagNodePtr>::OnDragDetected(MyGeometry, MouseEvent);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<IToolTip> SMemTagTreeViewTableRow::GetRowToolTip() const
{
	return RowToolTip.ToSharedRef();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeViewTableRow::InvalidateContent()
{
	RowToolTip->InvalidateWidget();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FSlateColor SMemTagTreeViewTableRow::GetBackgroundColorAndOpacity() const
{
	return GetBackgroundColorAndOpacity(MemTagNodePtr->GetAggregatedStats().Max);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FSlateColor SMemTagTreeViewTableRow::GetBackgroundColorAndOpacity(uint64 MemValue) const
{
	constexpr uint64 KiB = 1024ULL;
	constexpr uint64 MiB = 1024ULL * 1024ULL;
	constexpr uint64 GiB = 1024ULL * 1024ULL * 1024ULL;
	const FLinearColor Color =	MemValue > 100 * MiB ? FLinearColor(0.3f, 0.0f, 0.0f, 1.0f) :
								MemValue >  10 * MiB ? FLinearColor(0.3f, 0.1f, 0.0f, 1.0f) :
								MemValue >   1 * MiB ? FLinearColor(0.0f, 0.1f, 0.0f, 1.0f) :
								                       FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
	return Color;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FSlateColor SMemTagTreeViewTableRow::GetOutlineColorAndOpacity() const
{
	const FLinearColor NoColor(0.0f, 0.0f, 0.0f, 0.0f);
	const bool bShouldBeHighlighted = MemTagNodePtr->GetName() == HighlightedNodeName.Get();
	const FLinearColor OutlineColorAndOpacity = bShouldBeHighlighted ? FLinearColor(FColorList::SlateBlue) : NoColor;
	return OutlineColorAndOpacity;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const FSlateBrush* SMemTagTreeViewTableRow::GetOutlineBrush(const FName ColumnId) const
{
	EHorizontalAlignment HAlign = HAlign_Center;
	if (IsColumnVisibleDelegate.IsBound())
	{
		HAlign = GetColumnOutlineHAlignmentDelegate.Execute(ColumnId);
	}
	return FInsightsStyle::GetOutlineBrush(HAlign);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool SMemTagTreeViewTableRow::HandleShouldBeEnabled() const
{
	bool bResult = false;

	if (MemTagNodePtr->IsGroup())
	{
		bResult = true;
	}
	else
	{
		if (OnShouldBeEnabled.IsBound())
		{
			bResult = OnShouldBeEnabled.Execute(MemTagNodePtr);
		}
	}

	return bResult;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

EVisibility SMemTagTreeViewTableRow::IsColumnVisible(const FName ColumnId) const
{
	EVisibility Result = EVisibility::Collapsed;

	if (IsColumnVisibleDelegate.IsBound())
	{
		Result = IsColumnVisibleDelegate.Execute(ColumnId) ? EVisibility::Visible : EVisibility::Collapsed;
	}

	return Result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SMemTagTreeViewTableRow::OnSetHoveredCell(TSharedPtr<Insights::FTable> InTablePtr, TSharedPtr<Insights::FTableColumn> InColumnPtr, FMemTagNodePtr InMemTagNodePtr)
{
	SetHoveredCellDelegate.ExecuteIfBound(InTablePtr, InColumnPtr, InMemTagNodePtr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
