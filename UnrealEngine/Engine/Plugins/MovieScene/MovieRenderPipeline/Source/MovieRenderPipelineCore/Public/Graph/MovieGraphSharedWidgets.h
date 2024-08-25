// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/ITableRow.h"
#include "Widgets/Views/SListView.h"

// NOTE: There should not be widgetry defined in core. To fix this, the condition group queries in the render layer subsystem need to be refactored to
// expose their custom widgetry in MovieRenderPipelineEditor.

/**
 * A widget which lists items in rows w/ an alternating row color, where each item has an icon and text. A summary row appears at the end of the
 * list indicating how many items are in the list.
 */
template<typename ListType>
class SMovieGraphSimpleList final : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_RetVal_OneParam(const FSlateBrush*, FGetRowIcon, ListType);
	DECLARE_DELEGATE_RetVal_OneParam(FText, FGetRowText, ListType);
	DECLARE_DELEGATE_OneParam(FOnDelete, ListType);
	
	SLATE_BEGIN_ARGS(SMovieGraphSimpleList<ListType>)
		{}
		/** The source of data that the list will display. */
		SLATE_ATTRIBUTE(TArray<ListType>*, DataSource)

		/** The name of the data type that will be shown in the summary row. */
		SLATE_ATTRIBUTE(FText, DataType)

		/** The plural of the DataType attribute. */
		SLATE_ATTRIBUTE(FText, DataTypePlural)

		/** Gets the icon for a row in the list. */
		SLATE_EVENT(FGetRowIcon, OnGetRowIcon);

		/** Gets the text for a row in the list. */
		SLATE_EVENT(FGetRowText, OnGetRowText);

		/** Invoked when a delete operation is performed. */
		SLATE_EVENT(FOnDelete, OnDelete)
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs)
	{
		DataSource = InArgs._DataSource.Get();
		DataType = InArgs._DataType.Get();
		DataTypePlural = InArgs._DataTypePlural.Get();
		OnGetRowIcon = InArgs._OnGetRowIcon;
		OnGetRowText = InArgs._OnGetRowText;
		OnDelete = InArgs._OnDelete;

		ChildSlot
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew(ListView, SListView<ListType>)
				.ListItemsSource(DataSource)
				.SelectionMode(ESelectionMode::Single)
				.OnKeyDownHandler(this, &SMovieGraphSimpleList<ListType>::HandleDelete)
				.OnGenerateRow(this, &SMovieGraphSimpleList<ListType>::GenerateRow)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Fill)
				.Padding(FMargin(14, 4))
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.Text_Lambda([this]()
					{
						return FText::FromString(FString::Printf(TEXT("%i %s"), DataSource->Num(), DataSource->Num() == 1 ? *DataType.ToString() : *DataTypePlural.ToString()));
					})
				]
			]
		];
	}

	/** Refreshes the contents of the list. */
	void Refresh()
	{
		if (ListView)
		{
			ListView->RequestListRefresh();
		}
	}

private:
	/** Gets the row icon associated with the given list data. */
	const FSlateBrush* GetRowIcon(const ListType& InListData) const
	{
		if (OnGetRowIcon.IsBound())
		{
			return OnGetRowIcon.Execute(InListData);
		}
	
		return nullptr;
	}

	/** Gets the row text associated with the given list data. */
	FText GetRowText(const ListType& InListData) const
	{
		if (OnGetRowText.IsBound())
		{
			return OnGetRowText.Execute(InListData);
		}
	
		return FText();
	}

	/** Handles the delete operation. */
	FReply HandleDelete(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) const
	{
		TArray<ListType> SelectedItems;
		ListView->GetSelectedItems(SelectedItems);
				
		if ((InKeyEvent.GetKey() == EKeys::Delete) && (SelectedItems.Num() == 1))
		{
			if (OnDelete.IsBound())
			{
				OnDelete.Execute(SelectedItems[0]);
				return FReply::Handled();
			}
		}

		return FReply::Unhandled();
	}

	/** Generates a row in the list for the specified data. */
	TSharedRef<ITableRow> GenerateRow(ListType InListData, const TSharedRef<STableViewBase>& InOwnerTable) const
	{
		return
			SNew(STableRow<ListType>, InOwnerTable)
			.Style(FAppStyle::Get(), "TableView.AlternatingRow")
			.ShowWires(false)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(7.f, 5.f, 7.f, 5.f)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(GetRowIcon(InListData))
				]
				
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Fill)
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					.Text(GetRowText(InListData))
				]
			];
	}

private:
	TSharedPtr<SListView<ListType>> ListView;
	FGetRowIcon OnGetRowIcon;
	FGetRowText OnGetRowText;
	FOnDelete OnDelete;
	FText DataType;
	FText DataTypePlural;
	TArray<ListType>* DataSource = nullptr;
};