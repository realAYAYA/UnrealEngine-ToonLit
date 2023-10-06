// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SDMXReadOnlyFixturePatchList.h"

enum class ECheckBoxState : uint8;
struct FDMXEntityFixturePatchRef;
class UDMXControlConsoleFaderGroup;


DECLARE_DELEGATE_RetVal(ECheckBoxState, FDMXFixturePatchListCheckBoxStateRetValDelegate)
DECLARE_DELEGATE_RetVal_OneParam(ECheckBoxState, FDMXFixturePatchListEntityRetValDelegate, const TSharedPtr<FDMXEntityFixturePatchRef>)
DECLARE_DELEGATE_TwoParams(FDMXFixturePatchListCheckBoxStateDelegate, ECheckBoxState, const TSharedPtr<FDMXEntityFixturePatchRef>)

/** Collumn IDs in the Fixture Patch List */
struct FDMXControlConsoleReadOnlyFixturePatchListCollumnIDs
{
	static const FName CheckBox;
};

/** List of Fixture Patches in a DMX library for read only purposes in DMX Control Console */
class SDMXControlConsoleReadOnlyFixturePatchList
	: public SDMXReadOnlyFixturePatchList
{
public:
	SLATE_BEGIN_ARGS(SDMXControlConsoleReadOnlyFixturePatchList)
		: _ListDescriptor(FDMXReadOnlyFixturePatchListDescriptor())
	{}

	SLATE_ARGUMENT(FDMXReadOnlyFixturePatchListDescriptor, ListDescriptor)

		SLATE_ARGUMENT(UDMXLibrary*, DMXLibrary)

		/** Called when a row of the list is right clicked */
		SLATE_EVENT(FOnContextMenuOpening, OnContextMenuOpening)

		/** Called when row selection has changed */
		SLATE_EVENT(FDMXFixturePatchListRowSelectionDelegate, OnRowSelectionChanged)

		/** Called when a row is clicked */
		SLATE_EVENT(FDMXFixturePatchListRowDelegate, OnRowClicked)

		/** Called when a row is double clicked */
		SLATE_EVENT(FDMXFixturePatchListRowDelegate, OnRowDoubleClicked)

		/** Called to get the enable state of each row of the list */
		SLATE_EVENT(FDMXFixturePatchListRowRetValDelegate, IsRowEnabled)

		/** Called to get the visibility state of each row of the list */
		SLATE_EVENT(FDMXFixturePatchListRowRetValDelegate, IsRowVisibile)

		/** Called when checkbox state of the list is changed */
		SLATE_EVENT(FOnCheckStateChanged, OnCheckBoxStateChanged)

		/** Called when checkbox state is changed in a row */
		SLATE_EVENT(FDMXFixturePatchListCheckBoxStateDelegate, OnRowCheckBoxStateChanged)

		/** Called to get the checkbox state of the list */
		SLATE_EVENT(FDMXFixturePatchListCheckBoxStateRetValDelegate, IsChecked)

		/** Called to get the checkbox state of each row of the list */
		SLATE_EVENT(FDMXFixturePatchListEntityRetValDelegate, IsRowChecked)

	SLATE_END_ARGS()

	/** Constructs this widget */
	void Construct(const FArguments& InArgs);

protected:
	//~ Begin SDMXReadOnlyFixturePatchList interface
	virtual void InitializeByListDescriptor(const FDMXReadOnlyFixturePatchListDescriptor& InListDescriptor) override;
	virtual void RefreshList() override;
	virtual TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FDMXEntityFixturePatchRef> InItem, const TSharedRef<STableViewBase>& OwnerTable) override;
	virtual EVisibility GetRowVisibility(const TSharedPtr<FDMXEntityFixturePatchRef> InFixturePatchRef) const override;
	virtual TSharedRef<SHeaderRow> GenerateHeaderRow() override;
	//~ End of SDMXReadOnlyFixturePatchList interface

private:
	/** Syncs selection to current active Fader Groups */
	void SyncSelection();

	/** Registers this widget */
	void Register();

	/** Called when Control Console Data have been changed by adding/removing Fader Groups */
	void OnEditorConsoleDataChanged(const UDMXControlConsoleFaderGroup* FaderGroup);

	/** Called when the CheckBox state of a row on the list has changed */
	void OnRowCheckBoxStateChanged(ECheckBoxState CheckBoxState, const TSharedPtr<FDMXEntityFixturePatchRef> InFixturePatchRef);

	/** Called to get wheter the whole list is checked or not */
	ECheckBoxState IsChecked() const;

	/** Called to get wheter the ChackBox of a row from the list is checked or not */
	ECheckBoxState IsRowChecked(const TSharedPtr<FDMXEntityFixturePatchRef> InFixturePatchRef) const;

	// Slate Arguments
	FOnCheckStateChanged OnCheckBoxStateChangedDelegate;
	FDMXFixturePatchListCheckBoxStateDelegate OnRowCheckBoxStateChangedDelegate;
	FDMXFixturePatchListCheckBoxStateRetValDelegate IsCheckedDelegate;
	FDMXFixturePatchListEntityRetValDelegate IsRowCheckedDelegate;
};
