// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SDMXReadOnlyFixturePatchListRow.h"

class UDMXControlConsoleEditorModel;


namespace UE::DMX::Private
{
	class FDMXControlConsoleFixturePatchListRowModel;

	/** Entity Fixture Patch as a row in a list in DMX Control Console */
	class SDMXControlConsoleFixturePatchListRow
		: public SDMXReadOnlyFixturePatchListRow
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleFixturePatchListRow)
			{}
			/** Delegate broadcast when the fader group muted state changed */
			SLATE_EVENT(FSimpleDelegate, OnFaderGroupMutedChanged)

		SLATE_END_ARGS()

		/** Constructs this widget */
		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, const TSharedRef<FDMXReadOnlyFixturePatchListItem>& InItem, const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel);

	protected:
		//~ Begin SMultiColumnTableRow interface
		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;
		//~ End SMultiColumnTableRow interface

	private:
		/** Generates the row that displays the check box for Fixture Patch active state */
		TSharedRef<SWidget> GenerateCheckBoxRow();

		/** Model for this row */
		TSharedPtr<FDMXControlConsoleFixturePatchListRowModel> RowModel;

		/** Weak reference to the Control Console editor model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> WeakEditorModel;

		// Slate arguments
		FSimpleDelegate OnFaderGroupMutedChanged;
	};
}
