// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Views/TableViewTypeTraits.h"
#include "Layout/Margin.h"
#include "Widgets/Input/SComboButton.h"

class FAvaFontView;
class FReply;
class IPropertyHandle;
class SFontSelectorSeparator;
class SScrollBar;
class SSearchBox;
class STableViewBase;
class SWidget;
template <typename ArgumentType> class TSlateDelegates;
template <typename ItemType> class SListView;
template <typename ItemType> class TTextFilter;

#if WITH_ACCESSIBILITY

#endif

DECLARE_DELEGATE(FOnAvaComboBoxOpening)

class SAvaFontSelector : public SComboButton
{
public:
	using ListTypeTraits = TListTypeTraits<TSharedPtr<FAvaFontView>>;
	using NullableOptionType = TListTypeTraits<TSharedPtr<FAvaFontView>>::NullableType;

	/** Type of list used for showing menu options. */
	using SComboListType = SListView<TSharedPtr<FAvaFontView>>;

	/** Type of text filter used for FAvaFontView */
	using FontViewTextFilter = TTextFilter<TSharedPtr<FAvaFontView>>;

	/** Delegate types used to generate widgets that represent Options */
	using FOnGenerateWidget = TSlateDelegates<const TSharedPtr<FAvaFontView>&>::FOnGenerateWidget;
	using FOnSelectionChanged = TSlateDelegates<NullableOptionType>::FOnSelectionChanged;

	SLATE_BEGIN_ARGS(SAvaFontSelector)
		: _Content()
		, _ComboBoxStyle(&FAppStyle::Get().GetWidgetStyle< FComboBoxStyle >("ComboBox"))
		, _ButtonStyle(nullptr)
		, _ItemStyle(&FAppStyle::Get().GetWidgetStyle< FTableRowStyle >("ComboBox.Row"))
		, _ContentPadding(_ComboBoxStyle->ContentPadding)
		, _ForegroundColor(FSlateColor::UseStyle())
		, _FontPropertyHandle()
		, _OptionsSource()
		, _OnSelectionChanged()
		, _OnGenerateWidget()
		, _InitiallySelectedItem(ListTypeTraits::MakeNullPtr())
		, _Method()
		, _MaxListHeight(200.0f)
		, _HasDownArrow(true)
		, _EnableGamepadNavigationMode(false)
		, _IsFocusable(true)
		, _ShowMonospacedFontsOnly(false)
		, _ShowBoldFontsOnly(false)
		, _ShowItalicFontsOnly(false)
		{}

		/** Slot for this button's content (optional) */
		SLATE_DEFAULT_SLOT(FArguments, Content)

		SLATE_STYLE_ARGUMENT(FComboBoxStyle, ComboBoxStyle)

		/** The visual style of the button part of the combo box (overrides ComboBoxStyle) */
		SLATE_STYLE_ARGUMENT(FButtonStyle, ButtonStyle)

		SLATE_STYLE_ARGUMENT(FTableRowStyle, ItemStyle)

		SLATE_ATTRIBUTE(FMargin, ContentPadding)
		SLATE_ATTRIBUTE(FSlateColor, ForegroundColor)

		/** The Property Handle for the font being customized */
		SLATE_ATTRIBUTE(TSharedPtr<IPropertyHandle>, FontPropertyHandle)

		/** Options source for the fonts list available to this font selector widget */
		SLATE_ARGUMENT(const TArray<TSharedPtr<FAvaFontView>>*, OptionsSource)

		/** Event called whenever the current font selection is changed */
		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)

		SLATE_EVENT(FOnGenerateWidget, OnGenerateWidget)

		/** Called when combo box is opened, before list is actually created */
		SLATE_EVENT(FOnAvaComboBoxOpening, OnComboBoxOpening)

		/** The custom scrollbar to use in the ListView */
		SLATE_ARGUMENT(TSharedPtr<SScrollBar>, CustomScrollbar)

		/** The option that should be selected when the combo box is first created */
		SLATE_ARGUMENT(NullableOptionType, InitiallySelectedItem)

		SLATE_ARGUMENT(TOptional<EPopupMethod>, Method)

		/** The max height of the combo box menu */
		SLATE_ARGUMENT(float, MaxListHeight)

		/**
		 * When false, the down arrow is not generated and it is up to the API consumer
		 * to make their own visual hint that this is a drop down.
		 */
		SLATE_ARGUMENT(bool, HasDownArrow)

		/**
		 *  When false, directional keys will change the selection. When true, FontSelector
		 *	must be activated and will only capture arrow input while activated.
		*/
		SLATE_ARGUMENT(bool, EnableGamepadNavigationMode)

		/** When true, allows the combo box to receive keyboard focus */
		SLATE_ARGUMENT(bool, IsFocusable)

		/** True if this combo's menu should be collapsed when our parent receives focus, false (default) otherwise */
		SLATE_ARGUMENT(bool, CollapseMenuOnParentFocus)

		/** When true, will only show monospaced fonts */
		SLATE_ARGUMENT(bool, ShowMonospacedFontsOnly)

		/** When true, will only show bold fonts */
		SLATE_ARGUMENT(bool, ShowBoldFontsOnly)

		/** When true, will only show italic fonts */
		SLATE_ARGUMENT(bool, ShowItalicFontsOnly)

	SLATE_END_ARGS()

	/**
	 * Construct the widget from a declaration
	 *
	 * @param InArgs   Declaration from which to construct the combo box
	 */
	void Construct(const FArguments& InArgs);

	SAvaFontSelector()
		: ItemStyle(nullptr), EnableGamepadNavigationMode(false)
		, bControllerInputCaptured(false), FontsOptionsSource(nullptr)
		, bOptionsNeedRefresh(false), bShowMonospacedOnly(false)
		, bShowBoldOnly(false), bShowItalicOnly(false)
	{
	}

	/** 
	 * Requests a list refresh after updating options 
	 * Call SetSelectedItem to update the selected item if required
	 * @see SetSelectedItem
	 */
	void RefreshOptions();

	void SetShowMonospacedOnly(bool bMonospacedOnly);
	void SetShowBoldOnly(bool bBoldOnly);
	void SetShowItalicOnly(bool bItalicOnly);

protected:
	FText GetFilterHighlightText() const;

	/** Clears the current selection from all fonts lists*/
	void ClearSelection() const;

	/** Updates favorites list */
	void UpdateFavoriteFonts();

	/** Updates imported fonts list */
	void UpdateImportedFonts();

	/** Updates project fonts list */
	void UpdateProjectFonts();

	void UpdateSeparatorsVisibility() const;

	/** Create the row's display name using the delegate. */
	void TransformElementToString(TSharedPtr<FAvaFontView> InElement, TArray<FString>& OutStrings);

	/** Filter text changed handler. */
	void OnFilterTextChanged(const FText& Text);

	/** Sets the currently selected item */
	void SetSelectedItem(const NullableOptionType& InSelectedItem) const;

	/** Updates the list of filtered fonts */
	void UpdateFilteredFonts();

	/**
	 * Checks if the Font shown to the user as select matches the one handled by the Property Handler.
	 * If not, the widgets will update accordingly.
	 */
	void RefreshUIFacingFont();

	/** @return the item currently selected by the combo box. */
	NullableOptionType GetSelectedItem();

	//~ Begin SWidget
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End SWidget

	/** Handles key down events in filter search (e.g. Enter, Escape) */
	FReply OnSearchFieldKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);

	virtual bool SupportsKeyboardFocus() const override { return bIsFocusable; }

	virtual bool IsInteractable() const override { return IsEnabled(); }

	FText GetCurrentFontTooltipText() const;

private:
	/** Generate a row for the InItem in the combo box's list (passed in as OwnerTable). Do this by calling the user-specified OnGenerateWidget */
	TSharedRef<ITableRow> GenerateMenuItemRow(TSharedPtr<FAvaFontView> InItem, const TSharedRef<STableViewBase>& OwnerTable) const;

	/** Called if the menu is closed */
	void OnMenuOpenChanged(bool bOpen);

	/** Invoked when the selection in the list changes */
	void OnSelectionChanged_Internal(NullableOptionType ProposedSelection, ESelectInfo::Type SelectInfo);

	/** Handle clicking on the content menu */
	virtual FReply OnButtonClicked() override;

	/** The item style to use. */
	const FTableRowStyle* ItemStyle;

	/** The padding around each menu row */
	FMargin MenuRowPadding;

private:
	/** Delegate that is invoked when the selected item in the combo box changes */
	FOnSelectionChanged OnSelectionChanged;

	/** The item currently selected in the combo box */
	NullableOptionType SelectedItem;

	/** The ListView that we pop up; visualized the available options. */
	TSharedPtr<SComboListType> FontsListView;

	/** The ListView that we pop up; visualized the available options. */
	TSharedPtr<SComboListType> FavoriteFontsListView;

	/** The ListView that we pop up; visualized the available options. */
	TSharedPtr<SComboListType> ImportedFontsListView;

	/** The Scrollbar used in the ListView. */
	TSharedPtr<SScrollBar> CustomScrollbar;

	/** Delegate to invoke before the combo box is opening. */
	FOnAvaComboBoxOpening OnComboBoxOpening;

	/** Delegate to invoke when we need to visualize an option as a widget. */
	FOnGenerateWidget OnGenerateWidget;

	/** Use activate button to toggle ListView when enabled. */
	bool EnableGamepadNavigationMode;

	/**
	 * Holds a flag indicating whether a controller/keyboard is manipulating the combobox's value.
	 * When true, navigation away from the widget is prevented until a new value has been accepted or canceled.
	 */
	bool bControllerInputCaptured;

	const TArray<TSharedPtr<FAvaFontView>>* FontsOptionsSource;

	TArray<TSharedPtr<FAvaFontView>> FavoriteFontsOptions;
	TArray<TSharedPtr<FAvaFontView>> ImportedFontsOptions;
	TArray<TSharedPtr<FAvaFontView>> EngineFontOptions;

	TSharedPtr<SFontSelectorSeparator> FavoritesSeparator;
	TSharedPtr<SFontSelectorSeparator> ImportedSeparator;
	TSharedPtr<SFontSelectorSeparator> AllFontsSeparator;

	TSharedPtr<SSearchBox> SearchBox;

	/** Holds the search box filter. */
	TSharedPtr<FontViewTextFilter> SearchBoxFilter;

	/** Whether the list should be refreshed on the next tick. */
	bool bOptionsNeedRefresh;

	bool bShowMonospacedOnly;
	bool bShowBoldOnly;
	bool bShowItalicOnly;

	TArray<TSharedPtr<FAvaFontView>> FilteredFonts;

	TSharedPtr<IPropertyHandle> FontPropertyHandlePtr;
};
