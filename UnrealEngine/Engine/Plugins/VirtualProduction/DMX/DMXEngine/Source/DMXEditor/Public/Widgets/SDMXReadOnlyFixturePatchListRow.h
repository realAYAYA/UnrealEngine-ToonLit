// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Library/DMXEntityReference.h"
#include "SDMXReadOnlyFixturePatchList.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"


/** Entity Fixture Patch as a row in a list */
class DMXEDITOR_API SDMXReadOnlyFixturePatchListRow
	: public SMultiColumnTableRow<TSharedPtr<FDMXEntityFixturePatchRef>>
{
public:
	SLATE_BEGIN_ARGS(SDMXReadOnlyFixturePatchListRow)
	{}

		SLATE_EVENT(FOnDragDetected, OnRowDragDetected)

	SLATE_END_ARGS()
			
	/** Constructs this widget */
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, const TSharedRef<FDMXEntityFixturePatchRef>& InFixturePatchRef);

	/** Gets the Fixture Patch this widget is based on */
	UDMXEntityFixturePatch* GetFixturePatch() const { return FixturePatchRef.GetFixturePatch(); }

protected:
	//~ Begin SMultiColumnTableRow interface
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;
	//~ End SMultiColumnTableRow interface

private:
	/** Generates the row that displays the Fixture Patch Editor Color */
	TSharedRef<SWidget> GenerateEditorColorRow();

	/** Generates the row that displays the Fixture Patch Name */
	TSharedRef<SWidget> GenerateFixturePatchNameRow();

	/** Generates the row that displays the Fixture ID */
	TSharedRef<SWidget> GenerateFixtureIDRow();

	/** Generates the row that displays the Fixture Type */
	TSharedRef<SWidget> GenerateFixtureTypeRow();

	/** Generates the row that displays the Mode */
	TSharedRef<SWidget> GenerateModeRow();

	/** Generates the row that displays the Patch */
	TSharedRef<SWidget> GeneratePatchRow();

	/** Gets Fixture Patch editor color */
	FSlateColor GetFixtureEditorColor() const;

	/** Gets Fixture Patch Name as text */
	FText GetFixturePatchNameText() const;

	/** Gets Fixture ID as text */
	FText GetFixtureIDText() const;

	/** Gets Fixture Type as text */
	FText GetFixtureTypeText() const;

	/** Gets Fixture Mode as text */
	FText GetModeText() const;

	/** Gets Fixture Universe and Address as text */
	FText GetPatchText() const;

	/** Reference to the FixturePatchRef this widget is based on */
	FDMXEntityFixturePatchRef FixturePatchRef;
};
