// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaFontSelector.h"
#include "AvaFontSearchSettingsMenu.h"
#include "Font/AvaFontManagerSubsystem.h"
#include "Font/AvaFontView.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Framework/SlateDelegates.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "Misc/TextFilter.h"
#include "PropertyHandle.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateTypes.h"
#include "Styling/StyleColors.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"

#define LOCTEXT_NAMESPACE "AvaFontSelector"

class SFontSelectorRow : public STableRow<TSharedPtr<FAvaFontView>>
{
public:
	SLATE_BEGIN_ARGS(SFontSelectorRow)
		: _Style(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("ComboBox.Row"))
		, _Content()
		, _Padding(FMargin(0))
	{}
	SLATE_STYLE_ARGUMENT(FTableRowStyle, Style)
	SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_ATTRIBUTE(FMargin, Padding)
SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable);

	//~ Begin SWidget
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	//~ End SWidget
};


class SFontSelectorSeparator: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFontSelectorSeparator)
		:_Text("Separator")
	{
	}
	SLATE_ARGUMENT(FString, Text)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs)
	{
		Text = InArgs._Text;

		ChildSlot
		[
			SNew(SBox)
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			[
				SNew(STextBlock)
				.Text(FText::FromString(InArgs._Text))
			]
		];
	}

private:
	FString Text;
};

void SFontSelectorRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable)
{
	STableRow<TSharedPtr<FAvaFontView>>::Construct(
			typename STableRow<TSharedPtr<FAvaFontView>>::FArguments()
			.Style(InArgs._Style)
			.Padding(InArgs._Padding)
			.Content()
			[
				InArgs._Content.Widget
			]
			, InOwnerTable
		);
}

// Handle case where user clicks on an existing selected item
FReply SFontSelectorRow::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const TSharedPtr<ITypedTableView<TSharedPtr<FAvaFontView>>> OwnerWidget = this->OwnerTablePtr.Pin();

		const TSharedPtr<FAvaFontView>* MyItem = OwnerWidget->Private_ItemFromWidget(this);
		const bool bIsSelected = OwnerWidget->Private_IsItemSelected(*MyItem);

		if (bIsSelected)
		{
			// Reselect content to ensure selection is taken
			OwnerWidget->Private_SignalSelectionChanged(ESelectInfo::Direct);
			return FReply::Handled();
		}
	}
	return STableRow<TSharedPtr<FAvaFontView>>::OnMouseButtonDown(MyGeometry, MouseEvent);
}

void SAvaFontSelector::Construct(const FArguments& InArgs)
{
	check(InArgs._ComboBoxStyle)

	ItemStyle = InArgs._ItemStyle;
	MenuRowPadding = InArgs._ComboBoxStyle->MenuRowPadding;

	// Work out which values we should use based on whether we were given an override, or should use the style's version
	const FComboButtonStyle& OurComboButtonStyle = InArgs._ComboBoxStyle->ComboButtonStyle;
	const FButtonStyle* const OurButtonStyle = InArgs._ButtonStyle ? InArgs._ButtonStyle : &OurComboButtonStyle.ButtonStyle;

	OnComboBoxOpening = InArgs._OnComboBoxOpening;
	OnSelectionChanged = InArgs._OnSelectionChanged;
	OnGenerateWidget = InArgs._OnGenerateWidget;
	EnableGamepadNavigationMode = InArgs._EnableGamepadNavigationMode;
	bControllerInputCaptured = false;

	bShowMonospacedOnly = InArgs._ShowMonospacedFontsOnly;
	bShowBoldOnly = InArgs._ShowBoldFontsOnly;
	bShowItalicOnly = InArgs._ShowItalicFontsOnly;

	FontPropertyHandlePtr = InArgs._FontPropertyHandle.Get();

	FontsOptionsSource = InArgs._OptionsSource;
	CustomScrollbar = InArgs._CustomScrollbar;

	SearchBoxFilter = MakeShared<FontViewTextFilter>(
		FontViewTextFilter::FItemToStringArray::CreateSP(this, &SAvaFontSelector::TransformElementToString)
		);

	bOptionsNeedRefresh = true;

	const TSharedPtr<SWidget> ComboBoxMenuContent = SNew(SBorder)
		.Padding(2.0f)
		.BorderBackgroundColor(FSlateColor(EStyleColor::AccentBlue))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(7.0f, 6.0f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(SearchBox, SSearchBox)
					.OnTextChanged(this, &SAvaFontSelector::OnFilterTextChanged)
					.OnKeyDownHandler(this, &SAvaFontSelector::OnSearchFieldKeyDown)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SAvaFontSearchSettingsMenu)
					.AvaFontSelector(SharedThis(this))
				]
			]
			// FAVORITE FONTS SECTION //
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Center)
			.Padding(2.0f)
			[
				SAssignNew(FavoritesSeparator, SFontSelectorSeparator)
				.Text("Favorites")
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.MaxDesiredHeight(InArgs._MaxListHeight)
				.VAlign(VAlign_Fill)
				[
					SAssignNew(FavoriteFontsListView, SComboListType)
					.ListItemsSource(&FavoriteFontsOptions)
					.OnGenerateRow(this, &SAvaFontSelector::GenerateMenuItemRow)
					.OnSelectionChanged(this, &SAvaFontSelector::OnSelectionChanged_Internal)
					.SelectionMode(ESelectionMode::Single)
					.ExternalScrollbar(InArgs._CustomScrollbar)
				]
			]

			// IMPORTED FONTS SECTION //
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Center)
			.Padding(2.0f)
			[
				SAssignNew(ImportedSeparator, SFontSelectorSeparator)
				.Text("Project Fonts")
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.MaxDesiredHeight(InArgs._MaxListHeight)
				.VAlign(VAlign_Fill)
				[
					SAssignNew(this->ImportedFontsListView, SComboListType)
					.ListItemsSource(&ImportedFontsOptions)
					.OnGenerateRow(this, &SAvaFontSelector::GenerateMenuItemRow)
					.OnSelectionChanged(this, &SAvaFontSelector::OnSelectionChanged_Internal)
					.SelectionMode(ESelectionMode::Single)
					.ExternalScrollbar(InArgs._CustomScrollbar)
				]
			]

			// AVAILABLE SYSTEM FONTS SECTION //
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Center)
			.Padding(2.0f)
			[
				SAssignNew(AllFontsSeparator, SFontSelectorSeparator)
				.Text("System Fonts")
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.MaxDesiredHeight(InArgs._MaxListHeight)
				.VAlign(VAlign_Fill)
				[
					SAssignNew(FontsListView, SComboListType)
					.ListItemsSource(&FilteredFonts)
					.OnGenerateRow(this, &SAvaFontSelector::GenerateMenuItemRow)
					.OnSelectionChanged(this, &SAvaFontSelector::OnSelectionChanged_Internal)
					.SelectionMode(ESelectionMode::Single)
					.ExternalScrollbar(InArgs._CustomScrollbar)
				]
			]
		];

	// Set up content
	TSharedPtr<SWidget> ButtonContent = InArgs._Content.Widget;
	if (InArgs._Content.Widget == SNullWidget::NullWidget)
	{
		 SAssignNew(ButtonContent, STextBlock)
		.Text(NSLOCTEXT("SAvaFontSelector", "ContentWarning", "No Content Provided"))
		.ColorAndOpacity(FLinearColor::Red);
	}

	SComboButton::Construct(SComboButton::FArguments()
		.ComboButtonStyle(&OurComboButtonStyle)
		.ButtonStyle(OurButtonStyle)
		.Method(InArgs._Method)
		.ButtonContent()
		[
			ButtonContent.ToSharedRef()
		]
		.MenuContent()
		[
			ComboBoxMenuContent.ToSharedRef()
		]
		.HasDownArrow(InArgs._HasDownArrow)
		.ContentPadding(InArgs._ContentPadding)
		.ForegroundColor(InArgs._ForegroundColor)
		.OnMenuOpenChanged(this, &SAvaFontSelector::OnMenuOpenChanged)
		.IsFocusable(InArgs._IsFocusable)
		.CollapseMenuOnParentFocus(InArgs._CollapseMenuOnParentFocus)
		.ToolTipText(this, &SAvaFontSelector::GetCurrentFontTooltipText)
	);

	SetMenuContentWidgetToFocus(SearchBox);

	// Need to establish the selected item at point of construction so its available for querying
	// NB: If you need a selection to fire use SetItemSelection rather than setting an InitiallySelectedItem
	SelectedItem = InArgs._InitiallySelectedItem;
	if (ListTypeTraits::IsPtrValid(SelectedItem))
	{
		const TSharedPtr<FAvaFontView> ValidatedItem = ListTypeTraits::NullableItemTypeConvertToItemType(SelectedItem);
		FontsListView->Private_SetItemSelection(ValidatedItem, true);
		FontsListView->RequestScrollIntoView(ValidatedItem, 0);
	}
}

void SAvaFontSelector::ClearSelection() const
{
	FontsListView->ClearSelection();
	FavoriteFontsListView->ClearSelection();
	ImportedFontsListView->ClearSelection();
}

void SAvaFontSelector::SetSelectedItem(const NullableOptionType& InSelectedItem) const
{
	if (ListTypeTraits::IsPtrValid(InSelectedItem))
	{
		const TSharedPtr<FAvaFontView> InSelected = ListTypeTraits::NullableItemTypeConvertToItemType(InSelectedItem);
		FontsListView->SetSelection(InSelected);
	}
	else
	{
		ClearSelection();
	}
}

SAvaFontSelector::NullableOptionType SAvaFontSelector::GetSelectedItem()
{
	return SelectedItem;
}

void SAvaFontSelector::UpdateFilteredFonts()
{
	// Based on search results, update the filtered fonts list to shown to the user
	// Other lists will update consequently

	FilteredFonts.Empty();

	for (const TSharedPtr<FAvaFontView>& FontView : *FontsOptionsSource)
	{
		if (!FontView->HasValidFont())
		{
			continue;
		}

		if (bShowMonospacedOnly && !FontView->IsMonospaced())
		{
			continue;
		}

		if (bShowBoldOnly && !FontView->IsBold())
		{
			continue;
		}

		if (bShowItalicOnly && !FontView->IsItalic())
		{
			continue;
		}

		if (SearchBoxFilter->PassesFilter(FontView))
		{
			FilteredFonts.Add(FontView);
		}
	}
}

void SAvaFontSelector::UpdateFavoriteFonts()
{
	TArray<TSharedPtr<FAvaFontView>> Favorites;
	for (TSharedPtr<FAvaFontView> Font : FilteredFonts)
	{
		if (Font->IsFavorite())
		{
			Favorites.AddUnique(Font);
		}
	}

	FavoriteFontsOptions.Empty();
	FavoriteFontsOptions = Favorites;
}

void SAvaFontSelector::UpdateImportedFonts()
{
	TArray<TSharedPtr<FAvaFontView>> Imported;
	for (TSharedPtr<FAvaFontView> Font : FilteredFonts)
	{
		if (Font->GetFontSource() == EAvaFontSource::Project)
		{
			Imported.AddUnique(Font);
		}
	}

	ImportedFontsOptions.Empty();
	ImportedFontsOptions = Imported;
	UpdateProjectFonts();

	ImportedFontsOptions.Sort([](const TSharedPtr<FAvaFontView>& A, const TSharedPtr<FAvaFontView>& B)
	{
		return *A.Get() < *B.Get();
	});
}

void SAvaFontSelector::UpdateProjectFonts()
{
	TArray<TSharedPtr<FAvaFontView>> EngineFonts;
	for (const TSharedPtr<FAvaFontView>& Font : FilteredFonts)
	{
		if (Font->GetFontSource() == EAvaFontSource::Project)
		{
			if (!ImportedFontsOptions.Contains(Font))
			{
				EngineFonts.AddUnique(Font);
			}
		}
	}
	EngineFontOptions.Empty();
	EngineFontOptions = EngineFonts;
	ImportedFontsOptions.Append(EngineFontOptions);
}

void SAvaFontSelector::UpdateSeparatorsVisibility() const
{
	if (FilteredFonts.IsEmpty())
	{
		AllFontsSeparator->SetVisibility(EVisibility::Collapsed);
	}
	else
	{
		AllFontsSeparator->SetVisibility(EVisibility::Visible);
	}

	if (FavoriteFontsOptions.IsEmpty())
	{
		FavoritesSeparator->SetVisibility(EVisibility::Collapsed);
	}
	else
	{
		FavoritesSeparator->SetVisibility(EVisibility::Visible);
	}

	if (ImportedFontsOptions.IsEmpty())
	{
		ImportedSeparator->SetVisibility(EVisibility::Collapsed);
	}
	else
	{
		ImportedSeparator->SetVisibility(EVisibility::Visible);
	}
}

void SAvaFontSelector::RefreshOptions()
{
	UpdateFilteredFonts();

	UpdateFavoriteFonts();
	FavoriteFontsListView->RequestListRefresh();

	UpdateImportedFonts();
	ImportedFontsListView->RequestListRefresh();

	// Remove duplicates from list
	TArray<TSharedPtr<FAvaFontView>> FontsToRemove;
	for (const TSharedPtr<FAvaFontView>& Font : FilteredFonts)
	{
		if (ImportedFontsOptions.Contains(Font))
		{
			FontsToRemove.Add(Font);
		}
	}

	for (const TSharedPtr<FAvaFontView>& Font : FontsToRemove)
	{
		if (FilteredFonts.Contains(Font))
		{
			FilteredFonts.Remove(Font);
		}
	}

	FontsListView->RequestListRefresh();

	// We might want to hide some separators, in case their list is empty
	UpdateSeparatorsVisibility();
}

void SAvaFontSelector::SetShowMonospacedOnly(bool bMonospacedOnly)
{
	bShowMonospacedOnly = bMonospacedOnly;

	// Trigger a refresh
	bOptionsNeedRefresh = true;
}

void SAvaFontSelector::SetShowBoldOnly(bool bBoldOnly)
{
	bShowBoldOnly = bBoldOnly;

	// Trigger a refresh
	bOptionsNeedRefresh = true;
}

void SAvaFontSelector::SetShowItalicOnly(bool bItalicOnly)
{
	bShowItalicOnly = bItalicOnly;

	// Trigger a refresh
	bOptionsNeedRefresh = true;
}

FReply SAvaFontSelector::OnSearchFieldKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		FSlateApplication::Get().DismissAllMenus();
		return FReply::Handled();
	}
	else if (InKeyEvent.GetKey() == EKeys::Enter)
	{
		if (!FilteredFonts.IsEmpty())
		{
			OnSelectionChanged_Internal(FilteredFonts[0], ESelectInfo::Type::Direct);
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SAvaFontSelector::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (IsInteractable())
	{
		const EUINavigationAction NavAction = FSlateApplication::Get().GetNavigationActionFromKey(InKeyEvent);
		const EUINavigation NavDirection = FSlateApplication::Get().GetNavigationDirectionFromKey(InKeyEvent);
		if (EnableGamepadNavigationMode)
		{
			// The controller's bottom face button must be pressed once to begin manipulating the combobox's value.
			// Navigation away from the widget is prevented until the button has been pressed again or focus is lost.
			if (NavAction == EUINavigationAction::Accept)
			{
				if (bControllerInputCaptured == false)
				{
					// Begin capturing controller input and open the ListView
					bControllerInputCaptured = true;
					OnComboBoxOpening.ExecuteIfBound();
					return SComboButton::OnButtonClicked();
				}
				else
				{
					// Set selection to the selected item on the list and close
					bControllerInputCaptured = false;

					// Re-select first selected item, just in case it was selected by navigation previously
					TArray<TSharedPtr<FAvaFontView>> SelectedItems = FontsListView->GetSelectedItems();
					if (!SelectedItems.IsEmpty())
					{
						OnSelectionChanged_Internal(SelectedItems[0], ESelectInfo::Direct);
					}

					// Set focus back to FontSelector
					FReply Reply = FReply::Handled();
					Reply.SetUserFocus(this->AsShared(), EFocusCause::SetDirectly);
					return Reply;
				}

			}
			else if (NavAction == EUINavigationAction::Back || InKeyEvent.GetKey() == EKeys::BackSpace)
			{
				const bool bWasInputCaptured = bControllerInputCaptured;

				OnMenuOpenChanged(false);
				if (bWasInputCaptured)
				{
					return FReply::Handled();
				}
			}
			else
			{
				if (bControllerInputCaptured)
				{
					return FReply::Handled();
				}
			}
		}
		else
		{
			if (NavDirection == EUINavigation::Up)
			{
				const NullableOptionType NullableSelected = GetSelectedItem();
				if (ListTypeTraits::IsPtrValid(NullableSelected))
				{
					const TSharedPtr<FAvaFontView> ActuallySelected = ListTypeTraits::NullableItemTypeConvertToItemType(NullableSelected);
					const int32 SelectionIndex = FontsOptionsSource->Find(ActuallySelected);
					if (SelectionIndex >= 1)
					{
						// Select an item on the prev row
						SetSelectedItem((*FontsOptionsSource)[SelectionIndex - 1]);
					}
				}

				return FReply::Handled();
			}
			else if (NavDirection == EUINavigation::Down)
			{
				const NullableOptionType NullableSelected = GetSelectedItem();
				if (ListTypeTraits::IsPtrValid(NullableSelected))
				{
					const TSharedPtr<FAvaFontView> ActuallySelected = ListTypeTraits::NullableItemTypeConvertToItemType(NullableSelected);
					const int32 SelectionIndex = FontsOptionsSource->Find(ActuallySelected);
					if (SelectionIndex < FontsOptionsSource->Num() - 1)
					{
						// Select an item on the next row
						SetSelectedItem((*FontsOptionsSource)[SelectionIndex + 1]);
					}
				}

				return FReply::Handled();
			}

			return SComboButton::OnKeyDown(MyGeometry, InKeyEvent);
		}
	}
	return SWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FText SAvaFontSelector::GetCurrentFontTooltipText() const
{
	if (!FontsListView->GetSelectedItems().IsEmpty())
	{
		const TSharedPtr<FAvaFontView> Selected = FontsListView->GetSelectedItems()[0];
		return Selected->GetFontNameAsText();
	}

	return LOCTEXT("FontSelector_NoFont_Tooltip", "No Font is currently selected.");
}

TSharedRef<ITableRow> SAvaFontSelector::GenerateMenuItemRow(TSharedPtr<FAvaFontView> InItem, const TSharedRef<STableViewBase>& OwnerTable) const
{
	if (OnGenerateWidget.IsBound() && InItem.IsValid())
	{
		return SNew(SFontSelectorRow, OwnerTable)
			.Style(ItemStyle)
			.Padding(MenuRowPadding)
			[
				OnGenerateWidget.Execute(InItem)
			];
	}
	else
	{
		return SNew(SFontSelectorRow, OwnerTable)
			[
				SNew(STextBlock).Text(NSLOCTEXT("SlateCore", "ComboBoxMissingOnGenerateWidgetMethod", "Please provide a .OnGenerateWidget() handler."))
			];
	}
}

void SAvaFontSelector::OnMenuOpenChanged(bool bOpen)
{
	if (bOpen == false)
	{
		bControllerInputCaptured = false;

		if (ListTypeTraits::IsPtrValid(SelectedItem))
		{
			// Ensure the ListView selection is set back to the last committed selection
			const TSharedPtr<FAvaFontView> ActuallySelected = ListTypeTraits::NullableItemTypeConvertToItemType(SelectedItem);

			FontsListView->SetSelection(ActuallySelected, ESelectInfo::OnNavigation);
			FontsListView->RequestScrollIntoView(ActuallySelected, 0);
		}

		// Set focus back to FontSelector for users focusing the ListView that just closed
		TSharedRef<SWidget> ThisRef = AsShared();
		FSlateApplication::Get().ForEachUser([&ThisRef](FSlateUser& User)
		{
			if (User.HasFocusedDescendants(ThisRef))
			{
				User.SetFocus(ThisRef, EFocusCause::SetDirectly);
			}
		});
	}
}

void SAvaFontSelector::OnSelectionChanged_Internal(NullableOptionType ProposedSelection, ESelectInfo::Type SelectInfo)
{
	if (!ProposedSelection)
	{
		return;
	}

	// Ensure that the proposed selection is different
	if (SelectInfo != ESelectInfo::OnNavigation)
	{
		// Ensure that the proposed selection is different from selected
		if (ProposedSelection != SelectedItem)
		{
			SelectedItem = ProposedSelection;
			OnSelectionChanged.ExecuteIfBound(ProposedSelection, SelectInfo);
		}

		// Close combo even if user reselected item
		SetIsOpen(false);
	}
}

FReply SAvaFontSelector::OnButtonClicked()
{
	// If user clicked to close the combo menu
	if (IsOpen())
	{
		// Re-select first selected item, just in case it was selected by navigation previously
		TArray<TSharedPtr<FAvaFontView>> SelectedItems = FontsListView->GetSelectedItems();
		if (!SelectedItems.IsEmpty())
		{
			OnSelectionChanged_Internal(SelectedItems[0], ESelectInfo::Direct);
		}
	}
	else
	{
		OnComboBoxOpening.ExecuteIfBound();
	}

	return SComboButton::OnButtonClicked();
}

void SAvaFontSelector::OnFilterTextChanged(const FText& Text)
{
	SearchBoxFilter->SetRawFilterText(Text);
	bOptionsNeedRefresh = true;
}

void SAvaFontSelector::TransformElementToString(TSharedPtr<FAvaFontView> InElement, TArray<FString>& OutStrings)
{
	if (InElement)
	{
		OutStrings.Add(InElement->GetFontNameAsString());
	}
}

FText SAvaFontSelector::GetFilterHighlightText() const
{
	if (SearchBoxFilter)
	{
		return SearchBoxFilter->GetRawFilterText();
	}

	return {};
}

void SAvaFontSelector::RefreshUIFacingFont()
{
	UAvaFontManagerSubsystem* FontManagerSubsystem = UAvaFontManagerSubsystem::Get();
	if (!FontManagerSubsystem)
	{
		return;
	}

	if (!FontPropertyHandlePtr)
	{
		return;
	}

	const FAvaFontView* UIFacingFont = SelectedItem.Get();
	if (!UIFacingFont)
	{
		return;
	}

	FPropertyAccess::Result AccessResult;
	const TSharedPtr<FAvaFontView> ActualFontPtr = FontManagerSubsystem->GetFontViewFromPropertyHandle(FontPropertyHandlePtr, AccessResult);
	if (!ActualFontPtr)
	{
		return;
	}

	// Only perform the selection action if the fonts are different
	if (ActualFontPtr.Get() != UIFacingFont)
	{
		OnSelectionChanged_Internal(ActualFontPtr, ESelectInfo::Direct);
	}
}

void SAvaFontSelector::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SMenuAnchor::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (bOptionsNeedRefresh)
	{
		bOptionsNeedRefresh = false;
		RefreshOptions();
	}

	RefreshUIFacingFont();
}

#undef LOCTEXT_NAMESPACE
