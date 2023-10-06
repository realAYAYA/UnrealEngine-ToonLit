// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

enum class ECheckBoxState : uint8;
class ITableRow;
class SCheckBox;
class STableViewBase;

template<typename T>
class SListView;

namespace CheckBoxList
{
	struct FItemPair;
}

DECLARE_DELEGATE_OneParam( FOnCheckListItemStateChanged, int );

/** A widget that can be used inside a CustomDialog to display a list of checkboxes */
class TOOLWIDGETS_API SCheckBoxList: public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SCheckBoxList)
		: _CheckBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FCheckBoxStyle>("Checkbox"))
		, _IncludeGlobalCheckBoxInHeaderRow(true)
		{}
		/** The styling of the CheckBox */
		SLATE_STYLE_ARGUMENT(FCheckBoxStyle, CheckBoxStyle)
		/** The label of the item column header */
		SLATE_ARGUMENT(FText, ItemHeaderLabel)
		/** Optionally display a checkbox by the column header that toggles all items */
		SLATE_ARGUMENT(bool, IncludeGlobalCheckBoxInHeaderRow)
		/** Callback when any checkbox is changed. Parameter is the index of the item, or -1 if it was the "All"/Global checkbox */
		SLATE_EVENT( FOnCheckListItemStateChanged, OnItemCheckStateChanged )
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& Arguments);
	void Construct(const FArguments& Arguments, const TArray<FText>& Items, bool bIsChecked);
	void Construct(const FArguments& Arguments, const TArray<TSharedRef<SWidget>>& Items, bool bIsChecked);

	int32 AddItem(const FText& Text, bool bIsChecked);
	int32 AddItem(TSharedRef<SWidget> Widget, bool bIsChecked);
	void RemoveItem(int32 Index);
	bool IsItemChecked(int32 Index) const;

	TArray<bool> GetValues() const;
	int32 GetNumCheckboxes() const;

private:
	void UpdateAllChecked();
	ECheckBoxState GetToggleSelectedState() const;
	void OnToggleSelectedCheckBox(ECheckBoxState InNewState);
	void OnItemCheckBox(TSharedRef<CheckBoxList::FItemPair> InItem);

	TSharedRef<ITableRow> HandleGenerateRow(TSharedRef<CheckBoxList::FItemPair> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	ECheckBoxState bAllCheckedState;
	TArray<TSharedRef<CheckBoxList::FItemPair>> Items;

	const FCheckBoxStyle* CheckBoxStyle;
	TSharedPtr<SListView<TSharedRef<CheckBoxList::FItemPair>>> ListView;

	FOnCheckListItemStateChanged OnItemCheckStateChanged;
};
