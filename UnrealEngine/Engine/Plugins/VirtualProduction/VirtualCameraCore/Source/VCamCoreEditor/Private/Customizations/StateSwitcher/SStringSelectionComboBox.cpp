// Copyright Epic Games, Inc. All Rights Reserved.

#include "SStringSelectionComboBox.h"

#include "Algo/Find.h"
#include "SSearchableComboBox.h"
#include "Algo/Compare.h"
#include "Widgets/Text/STextBlock.h"

namespace UE::VCamCoreEditor::Private
{
	void SStringSelectionComboBox::Construct(const FArguments& InArgs)
	{
		ItemListAttribute = InArgs._ItemList;
		OnItemSelected = InArgs._OnItemSelected;
		Font = InArgs._Font;

		RefreshItemList();
		const FString SelectedItemName = InArgs._SelectedItem.IsSet() ? InArgs._SelectedItem.Get() : FString{};
		const TSharedPtr<FString>* InitiallySelectedItem = Algo::FindByPredicate(ItemList, [SelectedItemName](const TSharedPtr<FString>& Item)
		{
			return Item && *Item.Get() == SelectedItemName;
		});

		SSearchableComboBox::Construct(SSearchableComboBox::FArguments()
			.HasDownArrow(true)
			.OptionsSource(&ItemList)
			.OnGenerateWidget(this, &SStringSelectionComboBox::OnGenerateComboWidget)
			.OnSelectionChanged(this, &SStringSelectionComboBox::OnSelectionChangedInternal)
			.OnComboBoxOpening(this, &SStringSelectionComboBox::RefreshItemList)
			.InitiallySelectedItem(InitiallySelectedItem ? *InitiallySelectedItem : nullptr)
			.SearchVisibility(this, &SStringSelectionComboBox::GetSearchVisibility)
			.Content()
			[
				SNew(STextBlock)
				.Text_Lambda([GetSelectedItemAttr = InArgs._SelectedItem](){ return GetSelectedItemAttr.IsSet() ? FText::FromString(GetSelectedItemAttr.Get()) : FText::GetEmpty(); })
				.Font(Font)
			]
			);
	}

	void SStringSelectionComboBox::RefreshItemList()
	{
		TArray<FString> NewItemList = ItemListAttribute.Get();
		const bool bHasChanged = [this, &NewItemList]()
		{
			if (ItemList.Num() != NewItemList.Num())
			{
				return true;
			}

			for (int32 i = 0; i < ItemList.Num(); ++i)
			{
				if (ItemList[i] && *ItemList[i].Get() != NewItemList[i])
				{
					return true;
				}
			}
			return false;
		}();
		if (!bHasChanged)
		{
			return;
		}
		
		ItemList.Empty(NewItemList.Num());
		for (const FString& Item : NewItemList)
		{
			ItemList.Emplace(MakeShared<FString>(Item));
		}
	}

	void SStringSelectionComboBox::OnSelectionChangedInternal(TSharedPtr<FString> InSelectedItem, ESelectInfo::Type SelectInfo)
	{
		if (InSelectedItem)
		{
			OnItemSelected.ExecuteIfBound(*InSelectedItem);
		}
	}

	TSharedRef<SWidget> SStringSelectionComboBox::OnGenerateComboWidget(TSharedPtr<FString> InComboString)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(*InComboString))
			.Font(Font);
	}

	EVisibility SStringSelectionComboBox::GetSearchVisibility() const
	{
		constexpr uint32 ShowSearchForItemCount = 5;
		return ItemList.Num() >= ShowSearchForItemCount
			? EVisibility::Visible
			: EVisibility::Collapsed;
	}
}
