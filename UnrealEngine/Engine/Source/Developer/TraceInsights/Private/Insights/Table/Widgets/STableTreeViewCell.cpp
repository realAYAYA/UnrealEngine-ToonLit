// Copyright Epic Games, Inc. All Rights Reserved.

#include "STableTreeViewCell.h"

#include "SlateOptMacros.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SExpanderArrow.h"

// Insights
#include "Insights/InsightsStyle.h"
#include "Insights/Table/ViewModels/Table.h"
#include "Insights/Table/ViewModels/TableCellValueFormatter.h"
#include "Insights/Table/ViewModels/TableColumn.h"
#include "Insights/Table/Widgets/STableTreeViewRow.h"

#define LOCTEXT_NAMESPACE "Insights::STableTreeView"

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void STableTreeViewCell::Construct(const FArguments& InArgs, const TSharedRef<ITableRow>& InTableRow)
{
	TableRow = InTableRow;

	TablePtr = InArgs._TablePtr;
	ColumnPtr = InArgs._ColumnPtr;
	TableTreeNodePtr = InArgs._TableTreeNodePtr;

	ensure(TablePtr.IsValid());
	ensure(ColumnPtr.IsValid());
	ensure(TableTreeNodePtr.IsValid());

	SetHoveredCellDelegate = InArgs._OnSetHoveredCell;

	ChildSlot
	[
		GenerateWidgetForColumn(InArgs)
	];
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> STableTreeViewCell::GenerateWidgetForColumn(const FArguments& InArgs)
{
	if (InArgs._IsNameColumn)
	{
		return GenerateWidgetForNameColumn(InArgs);
	}
	else
	{
		return GenerateWidgetForTableColumn(InArgs);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> STableTreeViewCell::GenerateWidgetForNameColumn(const FArguments& InArgs)
{
	TSharedPtr<STableTreeViewRow> Row = StaticCastSharedPtr<STableTreeViewRow, ITableRow>(TableRow);
	TSharedPtr<IToolTip> RowToolTip = Row->GetRowToolTip();

	return
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		[
			SNew(SExpanderArrow, TableRow)
		]

		// Icon + tooltip
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Image(this, &STableTreeViewCell::GetIcon)
			.ColorAndOpacity(this, &STableTreeViewCell::GetIconColorAndOpacity)
			.ToolTip(RowToolTip)
		]

		// Name
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(FMargin(2.0f, 0.0f, 2.0f, 0.0f))
		[
			SNew(STextBlock)
			.Text(this, &STableTreeViewCell::GetDisplayName)
			.HighlightText(InArgs._HighlightText)
			.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.NameText"))
			.ColorAndOpacity(this, &STableTreeViewCell::GetDisplayNameColorAndOpacity)
			.ShadowColorAndOpacity(this, &STableTreeViewCell::GetShadowColorAndOpacity)
		]

		// Name Suffix
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(FMargin(2.0f, 0.0f, 2.0f, 0.0f))
		[
			SNew(STextBlock)
			.Visibility(this, &STableTreeViewCell::HasExtraDisplayName)
			.Text(this, &STableTreeViewCell::GetExtraDisplayName)
			.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.NameText"))
			.ColorAndOpacity(this, &STableTreeViewCell::GetExtraDisplayNameColorAndOpacity)
			.ShadowColorAndOpacity(this, &STableTreeViewCell::GetShadowColorAndOpacity)
		]
	;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText STableTreeViewCell::GetValueAsText() const
{
	return ColumnPtr->GetValueAsText(*TableTreeNodePtr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> STableTreeViewCell::GenerateWidgetForTableColumn(const FArguments& InArgs)
{
	TSharedRef<STextBlock> TextBox = SNew(STextBlock)
		.TextStyle(FInsightsStyle::Get(), TEXT("TreeTable.NormalText"))
		.ColorAndOpacity(this, &STableTreeViewCell::GetNormalTextColorAndOpacity)
		.ShadowColorAndOpacity(this, &STableTreeViewCell::GetShadowColorAndOpacity);

	if (ColumnPtr->IsDynamic())
	{
		TextBox->SetText(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateRaw(this, &STableTreeViewCell::GetValueAsText)));
	}
	else
	{
		FText CellText = ColumnPtr->GetValueAsText(*TableTreeNodePtr);
		TextBox->SetText(CellText);
	}

	TSharedPtr<IToolTip> ColumnToolTip = ColumnPtr->GetValueFormatter()->GetCustomTooltip(*ColumnPtr, *TableTreeNodePtr);

	return
		SNew(SHorizontalBox)
		.ToolTip(ColumnToolTip)

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.HAlign(ColumnPtr->GetHorizontalAlignment())
		.VAlign(VAlign_Center)
		.Padding(FMargin(2.0f, 0.0f, 2.0f, 0.0f))
		[
			TextBox
		];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

bool STableTreeViewCell::IsSelected() const
{
	return TableRow->IsItemSelected();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FSlateColor STableTreeViewCell::GetIconColorAndOpacity() const
{
	bool bIsHoveredOrSelected = IsHovered() || IsSelected();

	return bIsHoveredOrSelected ?
		TableTreeNodePtr->GetColor() :
		TableTreeNodePtr->GetColor().CopyWithNewOpacity(0.5f);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FSlateColor STableTreeViewCell::GetDisplayNameColorAndOpacity() const
{
	bool bIsHoveredOrSelected = IsHovered() || IsSelected();

	if (TableTreeNodePtr->IsFiltered())
	{
		return bIsHoveredOrSelected ?
			TableTreeNodePtr->GetColor().CopyWithNewOpacity(0.5f) :
			TableTreeNodePtr->GetColor().CopyWithNewOpacity(0.4f);
	}
	else
	{
		return bIsHoveredOrSelected ?
			TableTreeNodePtr->GetColor() :
			TableTreeNodePtr->GetColor().CopyWithNewOpacity(0.8f);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FSlateColor STableTreeViewCell::GetExtraDisplayNameColorAndOpacity() const
{
	bool bIsHoveredOrSelected = IsHovered() || IsSelected();

	if (TableTreeNodePtr->IsFiltered())
	{
		return bIsHoveredOrSelected ?
			FLinearColor(0.3f, 0.3f, 0.3f, 0.5f) :
			FLinearColor(0.3f, 0.3f, 0.3f, 0.4f);
	}
	else
	{
		return bIsHoveredOrSelected ?
			FLinearColor(0.3f, 0.3f, 0.3f, 1.0f) :
			FLinearColor(0.3f, 0.3f, 0.3f, 0.8f);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FSlateColor STableTreeViewCell::GetNormalTextColorAndOpacity() const
{
	bool bIsHoveredOrSelected = IsHovered() || IsSelected();

	if (TableTreeNodePtr->IsGroup())
	{
		if (TableTreeNodePtr->IsFiltered())
		{
			return bIsHoveredOrSelected ?
				FStyleColors::ForegroundHover.GetSpecifiedColor().CopyWithNewOpacity(0.4f) :
				FStyleColors::Foreground.GetSpecifiedColor().CopyWithNewOpacity(0.4f);
		}
		else
		{
			return bIsHoveredOrSelected ?
				FStyleColors::ForegroundHover.GetSpecifiedColor().CopyWithNewOpacity(0.8f) :
				FStyleColors::Foreground.GetSpecifiedColor().CopyWithNewOpacity(0.8f);
		}
	}
	else
	{
		if (TableTreeNodePtr->IsFiltered())
		{
			return bIsHoveredOrSelected ?
				FStyleColors::ForegroundHover.GetSpecifiedColor().CopyWithNewOpacity(0.5f) :
				FStyleColors::Foreground.GetSpecifiedColor().CopyWithNewOpacity(0.5f);
		}
		else
		{
			return bIsHoveredOrSelected ?
				FStyleColors::ForegroundHover :
				FStyleColors::Foreground;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FLinearColor STableTreeViewCell::GetShadowColorAndOpacity() const
{
	const FLinearColor ShadowColor =
		TableTreeNodePtr->IsFiltered() ?
		FLinearColor(0.0f, 0.0f, 0.0f, 0.25f) :
		FLinearColor(0.0f, 0.0f, 0.0f, 0.5f);
	return ShadowColor;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights

#undef LOCTEXT_NAMESPACE
