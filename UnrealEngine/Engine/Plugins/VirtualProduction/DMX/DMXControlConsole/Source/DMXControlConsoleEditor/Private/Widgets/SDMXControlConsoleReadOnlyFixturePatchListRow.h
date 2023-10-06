// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SDMXControlConsoleReadOnlyFixturePatchList.h"
#include "Widgets/SDMXReadOnlyFixturePatchListRow.h"

class UDMXControlConsoleFaderGroup;


/** Entity Fixture Patch as a row in a list in DMX Control Console */
class SDMXControlConsoleReadOnlyFixturePatchListRow
	: public SDMXReadOnlyFixturePatchListRow
{
public:
	SLATE_BEGIN_ARGS(SDMXControlConsoleReadOnlyFixturePatchListRow)
	{}

		/** Called to get the CheckBox state of this row */
		SLATE_EVENT(FDMXFixturePatchListCheckBoxStateRetValDelegate, IsChecked)

		/** Called when CheckBox state of this row has changed */
		SLATE_EVENT(FOnCheckStateChanged, OnCheckStateChanged)

	SLATE_END_ARGS()

	/** Constructs this widget */
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, const TSharedRef<FDMXEntityFixturePatchRef>& InFixturePatchRef);

protected:
	//~ Begin SMultiColumnTableRow interface
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;
	//~ End SMultiColumnTableRow interface

private:
	/** Generates the row that displays the check box for Fixture Patch active state */
	TSharedRef<SWidget> GenerateCheckBoxRow();

	/** Gets current CheckBox state of the row */
	ECheckBoxState IsChecked() const;

	// Slate Arguments
	FOnCheckStateChanged OnCheckStateChanged;
	FDMXFixturePatchListCheckBoxStateRetValDelegate IsCheckedDelegate;
};
