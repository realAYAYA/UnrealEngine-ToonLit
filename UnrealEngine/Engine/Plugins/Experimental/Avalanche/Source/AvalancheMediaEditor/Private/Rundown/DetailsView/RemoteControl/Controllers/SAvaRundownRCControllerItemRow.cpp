// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaRundownRCControllerItemRow.h"

#include "AvaRundownRCControllerItem.h"
#include "Widgets/Layout/SScissorRectBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

void SAvaRundownRCControllerItemRow::Construct(const FArguments& InArgs, TSharedRef<SAvaRundownRCControllerPanel> InControllerPanel,
	const TSharedRef<STableViewBase>& InOwnerTableView, const TSharedPtr<const FAvaRundownRCControllerItem>& InRowItem)
{
	ItemPtrWeak = InRowItem;
	ControllerPanelWeak = InControllerPanel;

	SMultiColumnTableRow<FAvaRundownRCControllerItemPtr>::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

TSharedRef<SWidget> SAvaRundownRCControllerItemRow::GenerateWidgetForColumn(const FName& InColumnName)
{
	TSharedPtr<const FAvaRundownRCControllerItem> ItemPtr = ItemPtrWeak.Pin();

	if (ItemPtr.IsValid())
	{
		if (InColumnName == SAvaRundownRCControllerPanel::ControllerColumnName)
		{
			return SNew(SScissorRectBox)
				[
					SNew(SBox)
					.VAlign(EVerticalAlignment::VAlign_Center)
					.Padding(3.f, 2.f, 3.f, 2.f)
					[
						SNew(STextBlock)
						.Text(ItemPtr->GetDisplayName())
					]
				];
		}
		else if (InColumnName == SAvaRundownRCControllerPanel::ValueColumnName)
		{
			if (ItemPtr->GetNodeWidgets().ValueWidget.IsValid())
			{
				return ItemPtr->GetNodeWidgets().ValueWidget.ToSharedRef();
			}
			else if (ItemPtr->GetNodeWidgets().WholeRowWidget.IsValid())
			{
				return ItemPtr->GetNodeWidgets().WholeRowWidget.ToSharedRef();
			}
		}
		else
		{
			TSharedPtr<SAvaRundownRCControllerPanel> ControllerPanel = ControllerPanelWeak.Pin();

			if (ControllerPanel.IsValid())
			{
				TSharedPtr<SWidget> Cell = nullptr;
				const TArray<FAvaRundownRCControllerTableRowExtensionDelegate>& TableRowExtensionDelegates = ControllerPanel->GetTableRowExtensionDelegates(InColumnName);

				for (const FAvaRundownRCControllerTableRowExtensionDelegate& TableRowExtensionDelegate : TableRowExtensionDelegates)
				{
					TableRowExtensionDelegate.ExecuteIfBound(ControllerPanel.ToSharedRef(), ItemPtr.ToSharedRef(), Cell);
				}

				if (Cell.IsValid())
				{
					return Cell.ToSharedRef();
				}
			}
		}
	}

	return SNullWidget::NullWidget;
}
