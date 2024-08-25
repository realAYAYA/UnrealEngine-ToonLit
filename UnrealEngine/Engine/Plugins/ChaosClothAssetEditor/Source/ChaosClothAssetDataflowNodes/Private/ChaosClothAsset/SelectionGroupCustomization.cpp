// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SelectionGroupCustomization.h"
#include "ChaosClothAsset/AttributeNode.h"
#include "ChaosClothAsset/ClothDataflowTools.h"  // For MakeCollectionName
#include "ChaosClothAsset/DeleteElementNode.h"
#include "ChaosClothAsset/SelectionNode.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "DataflowNodeSelectionGroupCustomization"

namespace UE::Chaos::ClothAsset
{
	TSharedRef<IPropertyTypeCustomization> FSelectionGroupCustomization::MakeInstance()
	{
		return MakeShareable(new FSelectionGroupCustomization);
	}

	void FSelectionGroupCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
	{
		uint32 NumChildren;
		const FPropertyAccess::Result Result = PropertyHandle->GetNumChildren(NumChildren);

		ChildPropertyHandle = (Result == FPropertyAccess::Success && NumChildren) ? PropertyHandle->GetChildHandle(0) : TSharedPtr<IPropertyHandle>();

		GroupNames.Reset();

		HeaderRow
			.NameContent()
			[
				PropertyHandle->CreatePropertyNameWidget(PropertyHandle->GetPropertyDisplayName())
			]
			.ValueContent()
			.MinDesiredWidth(250)
			.MaxDesiredWidth(350.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.MaxWidth(145.f)
				[
					SAssignNew(ComboButton, SComboButton)
					.ButtonStyle(FAppStyle::Get(), "NoBorder")
					.ContentPadding(0)
					.OnGetMenuContent(this, &FSelectionGroupCustomization::OnGetMenuContent)
					.ButtonContent()
					[
						SNew(SEditableTextBox)
						.Text_Raw(this, &FSelectionGroupCustomization::GetText)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.OnTextCommitted(this, &FSelectionGroupCustomization::OnTextCommitted)
					]
				]
			];
	}

	FText FSelectionGroupCustomization::GetText() const
	{
		FText Text;
		ChildPropertyHandle->GetValueAsFormattedText(Text);
		return Text;
	}

	void FSelectionGroupCustomization::OnTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
	{
		FText CurrentText;
		ChildPropertyHandle->GetValueAsFormattedText(CurrentText);

		if (!NewText.ToString().Equals(CurrentText.ToString(), ESearchCase::CaseSensitive))
		{
			FString String = NewText.ToString();
			FClothDataflowTools::MakeCollectionName(String);
			ChildPropertyHandle->SetValueFromFormattedString(String);
		}
	}

	void FSelectionGroupCustomization::OnSelectionChanged(TSharedPtr<FText> ItemSelected, ESelectInfo::Type /*SelectInfo*/)
	{
		// Set the child property's value
		if (ItemSelected)
		{
			FText CurrentText;
			ChildPropertyHandle->GetValueAsFormattedText(CurrentText);

			if (!ItemSelected->EqualTo(CurrentText))
			{
				ChildPropertyHandle->SetValueFromFormattedString(ItemSelected->ToString());
			}

			ComboButton->SetIsOpen(false);
		}
	}

	TSharedRef<ITableRow> FSelectionGroupCustomization::MakeCategoryViewWidget(TSharedPtr<FText> Item, const TSharedRef< STableViewBase >& OwnerTable)
	{
		return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
			[
				SNew(STextBlock).Text(*Item.Get())
			];
	}

	TSharedRef<SWidget> FSelectionGroupCustomization::OnGetMenuContent()
	{
		// Find all group names in the parent selection node's collection
		GroupNames.Reset();

		TArray<FName> CollectionGroupNames;
		if (const FChaosClothAssetSelectionNode* const SelectionNode = GetOwnerStruct<FChaosClothAssetSelectionNode>())
		{
			CollectionGroupNames = SelectionNode->GetCachedCollectionGroupNames();

		}
		else if (const FChaosClothAssetDeleteElementNode* const DeleteNode =
			GetOwnerStruct<FChaosClothAssetDeleteElementNode>())
		{
			CollectionGroupNames = DeleteNode->GetCachedCollectionGroupNames();
		}
		else if (const FChaosClothAssetAttributeNode* const AttributeNode =
			GetOwnerStruct<FChaosClothAssetAttributeNode>())
		{
			CollectionGroupNames = AttributeNode->GetCachedCollectionGroupNames();
		}

		for (const FName& CollectionGroupName : CollectionGroupNames)
		{
			GroupNames.Add(MakeShareable(new FText(FText::FromName(CollectionGroupName))));
		}

		return SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(400.0f)
			[
				SNew(SListView<TSharedPtr<FText>>)
				.ListItemsSource(&GroupNames)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow(this, &FSelectionGroupCustomization::MakeCategoryViewWidget)
				.OnSelectionChanged(this, &FSelectionGroupCustomization::OnSelectionChanged)
			];
	}

	template<typename T>
	T* FSelectionGroupCustomization::GetOwnerStruct() const
	{
		if (const TSharedPtr<IPropertyHandle> PropertyHandle = ChildPropertyHandle->GetParentHandle())
		{
			if (const TSharedPtr<IPropertyHandle> OwnerHandle = PropertyHandle->GetParentHandle())  // Assume that the group struct is only used at the node struct level
			{
				if (const TSharedPtr<IPropertyHandleStruct> OwnerHandleStruct = OwnerHandle->AsStruct())
				{
					if (TSharedPtr<FStructOnScope> StructOnScope = OwnerHandleStruct->GetStructData())
					{
						if (StructOnScope->GetStruct() == T::StaticStruct())
						{
							return reinterpret_cast<T*>(StructOnScope->GetStructMemory());
						}
					}
				}
			}
		}
		return nullptr;
	}
}  // End namespace UE::Chaos::ClothAsset

#undef LOCTEXT_NAMESPACE
