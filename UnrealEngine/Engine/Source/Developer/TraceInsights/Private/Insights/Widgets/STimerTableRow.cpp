// Copyright Epic Games, Inc. All Rights Reserved.

#include "STimerTableRow.h"

#include "SlateOptMacros.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Images/SImage.h"

// Insights
#include "Insights/Common/TimeUtils.h"
#include "Insights/InsightsStyle.h"
#include "Insights/Table/ViewModels/Table.h"
#include "Insights/Table/ViewModels/TableColumn.h"
#include "Insights/Widgets/STimersViewTooltip.h"
#include "Insights/Widgets/STimerTableCell.h"

#define LOCTEXT_NAMESPACE "STimersView"

////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void STimerTableRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	OnShouldBeEnabled = InArgs._OnShouldBeEnabled;
	IsColumnVisibleDelegate = InArgs._OnIsColumnVisible;
	GetColumnOutlineHAlignmentDelegate = InArgs._OnGetColumnOutlineHAlignmentDelegate;
	SetHoveredCellDelegate = InArgs._OnSetHoveredCell;

	HighlightText = InArgs._HighlightText;
	HighlightedNodeName = InArgs._HighlightedNodeName;

	TablePtr = InArgs._TablePtr;
	TimerNodePtr = InArgs._TimerNodePtr;

	RowToolTip = MakeShared<STimerTableRowToolTip>(TimerNodePtr);

	SetEnabled(TAttribute<bool>(this, &STimerTableRow::HandleShouldBeEnabled));

	SMultiColumnTableRow<FTimerNodePtr>::Construct(SMultiColumnTableRow<FTimerNodePtr>::FArguments(), InOwnerTableView);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> STimerTableRow::GenerateWidgetForColumn(const FName& ColumnId)
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
			.ColorAndOpacity(this, &STimerTableRow::GetBackgroundColorAndOpacity)
		]

		+SOverlay::Slot()
		.Padding(0.0f)
		[
			SNew(SImage)
			.Image(this, &STimerTableRow::GetOutlineBrush, ColumnId)
			.ColorAndOpacity(this, &STimerTableRow::GetOutlineColorAndOpacity)
		]

		+SOverlay::Slot()
		[
			SNew(STimerTableCell, SharedThis(this))
			.Visibility(this, &STimerTableRow::IsColumnVisible, ColumnId)
			.TablePtr(TablePtr)
			.ColumnPtr(ColumnPtr)
			.TimerNodePtr(TimerNodePtr)
			.HighlightText(HighlightText)
			.IsNameColumn(ColumnPtr->IsHierarchy())
			.OnSetHoveredCell(this, &STimerTableRow::OnSetHoveredCell)
		];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

FReply STimerTableRow::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	//im:TODO
	//if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	//{
	//	if (TimerNode->IsGroup())
	//	{
	//		// Add all timer Ids for the group.
	//		TArray<int32> TimerIds;
	//		const TArray<FTimerNodePtr>& FilteredChildren = TimerNode->GetFilteredChildren();
	//		const int32 NumFilteredChildren = FilteredChildren.Num();
	//
	//		TimerIds.Reserve(NumFilteredChildren);
	//		for (int32 Nx = 0; Nx < NumFilteredChildren; ++Nx)
	//		{
	//			TimerIds.Add(FilteredChildren[Nx]->GetId());
	//		}
	//
	//		return FReply::Handled().BeginDragDrop(FStatIDDragDropOp::NewGroup(TimerIds, TimerNode->GetName().GetPlainNameString()));
	//	}
	//	else
	//	{
	//		return FReply::Handled().BeginDragDrop(FStatIDDragDropOp::NewSingle(TimerNode->GetId(), TimerNode->GetName().GetPlainNameString()));
	//	}
	//}

	return SMultiColumnTableRow<FTimerNodePtr>::OnDragDetected(MyGeometry, MouseEvent);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<IToolTip> STimerTableRow::GetRowToolTip() const
{
	return RowToolTip.ToSharedRef();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTableRow::InvalidateContent()
{
	RowToolTip->InvalidateWidget();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FSlateColor STimerTableRow::GetBackgroundColorAndOpacity() const
{
	if (TimerNodePtr->GetType() == ETimerNodeType::Group)
	{
		return FLinearColor(0.0f, 0.0f, 0.5f, 1.0f);
	}
	else
	{
		return GetBackgroundColorAndOpacity(TimerNodePtr->GetAggregatedStats().TotalInclusiveTime);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FSlateColor STimerTableRow::GetBackgroundColorAndOpacity(double Time) const
{
	const FLinearColor Color =	Time > TimeUtils::Second      ? FLinearColor(0.3f, 0.0f, 0.0f, 1.0f) :
								Time > TimeUtils::Milisecond  ? FLinearColor(0.3f, 0.1f, 0.0f, 1.0f) :
								Time > TimeUtils::Microsecond ? FLinearColor(0.0f, 0.1f, 0.0f, 1.0f) :
								                                FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
	return Color;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FSlateColor STimerTableRow::GetOutlineColorAndOpacity() const
{
	const FLinearColor NoColor(0.0f, 0.0f, 0.0f, 0.0f);
	const bool bShouldBeHighlighted = TimerNodePtr->GetName() == HighlightedNodeName.Get();
	const FLinearColor OutlineColorAndOpacity = bShouldBeHighlighted ? FLinearColor(FColorList::SlateBlue) : NoColor;
	return OutlineColorAndOpacity;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const FSlateBrush* STimerTableRow::GetOutlineBrush(const FName ColumnId) const
{
	EHorizontalAlignment HAlign = HAlign_Center;
	if (IsColumnVisibleDelegate.IsBound())
	{
		HAlign = GetColumnOutlineHAlignmentDelegate.Execute(ColumnId);
	}
	return FInsightsStyle::GetOutlineBrush(HAlign);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STimerTableRow::HandleShouldBeEnabled() const
{
	bool bResult = false;

	if (TimerNodePtr->IsGroup())
	{
		bResult = true;
	}
	else
	{
		if (OnShouldBeEnabled.IsBound())
		{
			bResult = OnShouldBeEnabled.Execute(TimerNodePtr);
		}
	}

	return bResult;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

EVisibility STimerTableRow::IsColumnVisible(const FName ColumnId) const
{
	EVisibility Result = EVisibility::Collapsed;

	if (IsColumnVisibleDelegate.IsBound())
	{
		Result = IsColumnVisibleDelegate.Execute(ColumnId) ? EVisibility::Visible : EVisibility::Collapsed;
	}

	return Result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void STimerTableRow::OnSetHoveredCell(TSharedPtr<Insights::FTable> InTablePtr, TSharedPtr<Insights::FTableColumn> InColumnPtr, FTimerNodePtr InTimerNodePtr)
{
	SetHoveredCellDelegate.ExecuteIfBound(InTablePtr, InColumnPtr, InTimerNodePtr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
