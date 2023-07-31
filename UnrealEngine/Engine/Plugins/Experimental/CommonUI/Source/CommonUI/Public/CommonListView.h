// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ListView.h"
#include "CommonListView.generated.h"

//////////////////////////////////////////////////////////////////////////
// SCommonListView
//////////////////////////////////////////////////////////////////////////

template <typename ItemType>
class SCommonListView : public SListView<ItemType>
{
public:
	virtual FReply OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent) override
	{
		if (bScrollToSelectedOnFocus && (InFocusEvent.GetCause() == EFocusCause::Navigation || InFocusEvent.GetCause() == EFocusCause::SetDirectly))
		{
			// Set selection to the first item in a list if no items are selected.
			// If bReturnFocusToSelection is true find the last selected object and focus on that.
			if (this->ItemsSource && this->ItemsSource->Num() > 0)
			{
				typename TListTypeTraits<ItemType>::NullableType ItemNavigatedTo(nullptr);
				if (this->GetNumItemsSelected() == 0)
				{
					ItemNavigatedTo = (*this->ItemsSource)[0];
				}
				else if (this->bReturnFocusToSelection && TListTypeTraits<ItemType>::IsPtrValid(this->SelectorItem))
				{
					ItemNavigatedTo = this->SelectorItem;
				}

				if (TListTypeTraits<ItemType>::IsPtrValid(ItemNavigatedTo))
				{
					ItemType SelectedItem = TListTypeTraits<ItemType>::NullableItemTypeConvertToItemType(ItemNavigatedTo);

					// Preselect the first valid widget so these calls do not internally select something different
					TOptional<ItemType> FirstValidItem = this->Private_FindNextSelectableOrNavigable(SelectedItem);
					if (FirstValidItem.IsSet())
					{
						this->SetSelection(FirstValidItem.GetValue(), ESelectInfo::OnNavigation);
						this->RequestNavigateToItem(FirstValidItem.GetValue(), InFocusEvent.GetUser());
					}
				}
			}
		}
		bScrollToSelectedOnFocus = true;

		return SListView<ItemType>::OnFocusReceived(MyGeometry, InFocusEvent);
	}

	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override
	{
		SListView<ItemType>::OnMouseLeave(MouseEvent);

		if (MouseEvent.IsTouchEvent() && this->HasMouseCapture())
		{
			// Regular list views will clear this flag when the pointer leaves the list. To
			// continue scrolling outside the list, we need this to remain on.
			this->bStartedTouchInteraction = true;
		}
	}

	virtual FReply OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override
	{
		FReply Reply = SListView<ItemType>::OnTouchMoved(MyGeometry, InTouchEvent);

		if (Reply.IsEventHandled() && this->HasMouseCapture())
		{
			bScrollToSelectedOnFocus = false;
			Reply.SetUserFocus(this->AsShared());
		}

		return Reply;
	}

	virtual FReply OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override
	{
		return SListView<ItemType>::OnTouchEnded(MyGeometry, InTouchEvent);
	}

protected:
	bool bScrollToSelectedOnFocus = true;
};

//////////////////////////////////////////////////////////////////////////
// UCommonListView
//////////////////////////////////////////////////////////////////////////

UCLASS()
class COMMONUI_API UCommonListView : public UListView
{
	GENERATED_BODY()

public:
	UCommonListView(const FObjectInitializer& ObjectInitializer);
	
	UFUNCTION(BlueprintCallable, Category = ListView)
	void SetEntrySpacing(float InEntrySpacing);

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	virtual TSharedRef<STableViewBase> RebuildListWidget() override;
	virtual UUserWidget& OnGenerateEntryWidgetInternal(UObject* Item, TSubclassOf<UUserWidget> DesiredEntryClass, const TSharedRef<STableViewBase>& OwnerTable) override;
};