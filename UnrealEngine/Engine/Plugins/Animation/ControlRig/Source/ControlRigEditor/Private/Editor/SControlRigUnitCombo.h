// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SComboButton.h"
#include "Editor/ControlRigEditor.h"

DECLARE_DELEGATE_OneParam(FOnRigUnitSelected, UStruct* /*SelectedUnitStruct*/);

class SSearchBox;

class FRigUnitTypeItem : public TSharedFromThis<FRigUnitTypeItem>
{
public:
	FRigUnitTypeItem(UStruct* InStruct)
		: Struct(InStruct)
	{
		static const FName DisplayName(TEXT("DisplayName"));
		DisplayText = FText::FromString(Struct->GetMetaData(DisplayName));
	}

	/** The name to display in the UI */
	FText DisplayText;

	/** The struct of the rig unit */
	UStruct* Struct;
};

class SControlRigUnitCombo : public SComboButton
{
public:
	SLATE_BEGIN_ARGS(SControlRigUnitCombo) {}

	SLATE_EVENT(FOnRigUnitSelected, OnRigUnitSelected)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FControlRigEditor> InControlRigEditor);

private:
	TSharedRef<ITableRow> GenerateListRow(TSharedPtr<FRigUnitTypeItem> InItem, const TSharedRef<STableViewBase>& InOwningTable);

	FText GetCurrentSearchString() const;

	void OnSelectionChanged(TSharedPtr<FRigUnitTypeItem> InItem, ESelectInfo::Type InSelectInfo);

	void BuildUnitTypesList();

	TSharedRef<SWidget> HandleGetMenuContent();

private:
	/** Our owning control rig editor */
	TWeakPtr<FControlRigEditor> ControlRigEditor;

	/** Delegate fired on rig unit selection */
	FOnRigUnitSelected OnRigUnitSelected;

	/** The list of rig types to choose from */
	TArray<TSharedPtr<FRigUnitTypeItem>> UnitTypeList;

	/** Search box widget */
	TSharedPtr<SSearchBox> FilterBox;

	/** The list view widget */
	TSharedPtr<SListView<TSharedPtr<FRigUnitTypeItem>>> ListView;
};

