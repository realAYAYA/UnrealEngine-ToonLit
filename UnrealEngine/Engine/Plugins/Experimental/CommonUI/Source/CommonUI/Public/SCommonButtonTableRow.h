// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Slate/SObjectTableRow.h"
#include "CommonButtonBase.h"

/** 
 * A CommonUI version of the object table row that is aware of UCommonButtonBase.
 * Instead of bothering with handling mouse events directly, we rely on the entry being a button itself and respond to events from it.
 */
template <typename ItemType>
class SCommonButtonTableRow : public SObjectTableRow<ItemType>
{
public:
	SLATE_BEGIN_ARGS(SCommonButtonTableRow<ItemType>)
		:_bAllowDragging(true)
	{}
		SLATE_ARGUMENT(bool, bAllowDragging)
		SLATE_EVENT(FOnRowHovered, OnHovered)
		SLATE_EVENT(FOnRowHovered, OnUnhovered)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, UUserWidget& InChildWidget, UListViewBase* InOwnerListView)
	{
		SObjectTableRow<ItemType>::Construct(
			typename SObjectTableRow<ItemType>::FArguments()
			.bAllowDragging(InArgs._bAllowDragging)
			.OnHovered(InArgs._OnHovered)
			.OnUnhovered(InArgs._OnUnhovered)
			[
				InArgs._Content.Widget
			], 
			InOwnerTableView, InChildWidget, InOwnerListView);

		UCommonButtonBase* CommonButton = Cast<UCommonButtonBase>(&InChildWidget);
		if (ensureMsgf(CommonButton, TEXT("The widget object attached to an SCommonButtonTableRow is always expected to be a UCommonButtonBase.")))
		{
			// We override whatever settings this button claimed to have - the owning list still has the authority on the selection and toggle behavior of its rows
			ESelectionMode::Type SelectionMode = this->GetSelectionMode();
			CommonButton->SetIsToggleable(SelectionMode == ESelectionMode::SingleToggle || SelectionMode == ESelectionMode::Multi);
			CommonButton->SetIsSelectable(SelectionMode != ESelectionMode::None);
			CommonButton->SetIsInteractableWhenSelected(SelectionMode != ESelectionMode::None);

			CommonButton->OnClicked().AddSP(this, &SCommonButtonTableRow::HandleButtonClicked);
			CommonButton->OnDoubleClicked().AddSP(this, &SCommonButtonTableRow::HandleButtonDoubleClicked);
			CommonButton->OnHovered().AddSP(this, &SCommonButtonTableRow::HandleButtonHovered);
			CommonButton->OnUnhovered().AddSP(this, &SCommonButtonTableRow::HandleButtonUnhovered);
			CommonButton->OnIsSelectedChanged().AddSP(this, &SCommonButtonTableRow::HandleButtonSelectionChanged);

			CommonButton->SetTouchMethod(EButtonTouchMethod::PreciseTap);
		}
	}

	// We rely on the button to handle all of these things for us
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override { SObjectWidget::OnMouseEnter(MyGeometry, MouseEvent); }
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override { SObjectWidget::OnMouseLeave(MouseEvent); }
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override { return SObjectWidget::OnMouseButtonDown(MyGeometry, MouseEvent); }
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override { return SObjectWidget::OnMouseButtonUp(MyGeometry, MouseEvent); }
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override { return FReply::Handled(); }

protected:
	virtual void InitializeObjectRow() override
	{
		SObjectTableRow<ItemType>::InitializeObjectRow();

		if (UCommonButtonBase* CommonButton = Cast<UCommonButtonBase>(this->WidgetObject))
		{
			if (this->IsItemSelectable())
			{
				const bool bIsItemSelected = this->IsItemSelected();
				if (bIsItemSelected != CommonButton->GetSelected())
				{
					// Quietly set the button to reflect the item selection
					CommonButton->SetSelectedInternal(bIsItemSelected, false, false);
				}
			}
			else
			{
				CommonButton->SetIsSelectable(false);
			}
		}
	}

	virtual void ResetObjectRow() override
	{
		SObjectTableRow<ItemType>::ResetObjectRow();

		UCommonButtonBase* CommonButton = Cast<UCommonButtonBase>(this->WidgetObject);
		if (CommonButton && CommonButton->GetSelected())
		{
			// Quietly deselect the button to reset its visual state
			CommonButton->SetSelectedInternal(false, false, false);
		}
	}

	virtual void OnItemSelectionChanged(bool bIsItemSelected) override
	{
		SObjectTableRow<ItemType>::OnItemSelectionChanged(bIsItemSelected);

		// Selection changes at the list level can happen directly or in response to another item being selected.
		// Regardless, just make sure the button's selection state is inline with the item's
		UCommonButtonBase* CommonButton = Cast<UCommonButtonBase>(this->WidgetObject);
		if (CommonButton && CommonButton->GetSelected() != bIsItemSelected)
		{
			CommonButton->SetSelectedInternal(bIsItemSelected, false);
		}
	}

private:
	void HandleButtonClicked()
	{
		TSharedRef<ITypedTableView<ItemType>> OwnerTable = this->OwnerTablePtr.Pin().ToSharedRef();

		if (const ItemType* MyItemPtr = this->GetItemForThis(OwnerTable))
		{
			OwnerTable->Private_OnItemClicked(*MyItemPtr);
		}
	}

	void HandleButtonDoubleClicked()
	{
		TSharedRef<ITypedTableView<ItemType>> OwnerTable = this->OwnerTablePtr.Pin().ToSharedRef();

		if (const ItemType* MyItemPtr = this->GetItemForThis(OwnerTable))
		{
			OwnerTable->Private_OnItemDoubleClicked(*MyItemPtr);
		}

		// Do we want to behave differently or normally on double clicks?
		// HandleButtonClicked();
	}

	void HandleButtonHovered()
	{
		this->OnHovered.ExecuteIfBound(*this->WidgetObject);
	}

	void HandleButtonUnhovered()
	{
		this->OnUnhovered.ExecuteIfBound(*this->WidgetObject);
	}

	void HandleButtonSelectionChanged(bool bIsButtonSelected)
	{
		const ESelectionMode::Type SelectionMode = this->GetSelectionMode();
		if (ensure(SelectionMode != ESelectionMode::None) && bIsButtonSelected != this->IsItemSelected())
		{
			TSharedRef<ITypedTableView<ItemType>> OwnerTable = this->OwnerTablePtr.Pin().ToSharedRef();

			if (const ItemType* MyItemPtr = this->GetItemForThis(OwnerTable))
			{
				if (bIsButtonSelected)
				{
					if (SelectionMode != ESelectionMode::Multi)
					{
						OwnerTable->Private_ClearSelection();
					}
					OwnerTable->Private_SetItemSelection(*MyItemPtr, true, true);
				}
				else
				{
					OwnerTable->Private_SetItemSelection(*MyItemPtr, false, true);
				}

				OwnerTable->Private_SignalSelectionChanged(ESelectInfo::Direct);
			}
		}
	}
};
