// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/Views/STileView.h"

template <typename ItemType>
class COMMONUI_API SCommonTileView : public STileView<ItemType>
{
public:
	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent) override
	{
		if (bScrollToSelectedOnFocus && (InFocusEvent.GetCause() == EFocusCause::Navigation || InFocusEvent.GetCause() == EFocusCause::SetDirectly))
		{
			if (this->ItemsSource && this->ItemsSource->Num() > 0)
			{
				if (this->GetNumItemsSelected() == 0)
				{
					ItemType FirstItem = (*this->ItemsSource)[0];
					this->SetSelection(FirstItem, ESelectInfo::OnNavigation);
					this->RequestNavigateToItem(FirstItem, InFocusEvent.GetUser());
				}
				else
				{
					TArray<ItemType> ItemArray;
					this->GetSelectedItems(ItemArray);

					ItemType FirstSelected = ItemArray[0];
					this->SetSelection(FirstSelected, ESelectInfo::OnNavigation);
					this->RequestNavigateToItem(FirstSelected, InFocusEvent.GetUser());
				}
			}
		}
		bScrollToSelectedOnFocus = true;
		return STileView<ItemType>::OnFocusReceived(MyGeometry, InFocusEvent);
	}

	virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override
	{
		STileView<ItemType>::OnFocusLost(InFocusEvent);

		// if we've lost focus, even if we're already scrolling a tile into view, refrain from navigating to that tile and thus stealing focus from another widget
		this->bNavigateOnScrollIntoView = false;
	}

	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override
	{
		STileView<ItemType>::OnMouseLeave(MouseEvent);

		if (MouseEvent.IsTouchEvent() && this->HasMouseCapture())
		{
			// Regular list views will clear this flag when the pointer leaves the list. To
			// continue scrolling outside the list, we need this to remain on.
			this->bStartedTouchInteraction = true;
		}
	}

	virtual FReply OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override
	{
		FReply Reply = STileView<ItemType>::OnTouchMoved(MyGeometry, InTouchEvent);

		if (Reply.IsEventHandled() && this->HasMouseCapture())
		{
			bScrollToSelectedOnFocus = false;
			Reply.SetUserFocus(this->AsShared());
		}
		return Reply;
	}

private:
	bool bScrollToSelectedOnFocus = true;
};
