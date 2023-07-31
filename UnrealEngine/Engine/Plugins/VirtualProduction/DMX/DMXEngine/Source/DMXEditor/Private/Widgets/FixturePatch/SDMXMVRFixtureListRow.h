// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Library/DMXEntityReference.h"

#include "CoreMinimal.h"
#include "Widgets/Views/STableRow.h"

enum class EDMXMVRFixtureListEditMode : uint8;
class UDMXEntityFixturePatch;
class FDMXMVRFixtureListItem;

class SInlineEditableTextBlock;


/** MVR Fixture view as a row in a list */
class SDMXMVRFixtureListRow
	: public SMultiColumnTableRow<TSharedPtr<FDMXMVRFixtureListItem>>
{
public:
	SLATE_BEGIN_ARGS(SDMXMVRFixtureListRow)
	{}
		/** Delegate executed when the row requests to refresh the statuses */
		SLATE_EVENT(FSimpleDelegate, OnRowRequestsStatusRefresh)

		/** Delegate executed when the row requests to refresh the whole list */
		SLATE_EVENT(FSimpleDelegate, OnRowRequestsListRefresh)

		/** Callback to check if the row is selected (should be hooked up if a parent widget is handling selection or focus) */
		SLATE_EVENT(FIsSelected, IsSelected)

	SLATE_END_ARGS()
			
	/** Constructs this widget */
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, const TSharedRef<FDMXMVRFixtureListItem>& InItem);

	/** Enters editing mode for the Fixture Patch Name */
	void EnterFixturePatchNameEditingMode();

	/** Returns the Item of this row */
	TSharedPtr<FDMXMVRFixtureListItem> GetItem() const { return Item; };

protected:
	//~ Begin SMultiColumnTableRow interface
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;
	//~ End SMultiColumnTableRow interface

private:
	/** Generates the row that displays the Fixture Patch Name */
	TSharedRef<SWidget> GenerateFixturePatchNameRow();

	/** Called when the Fixture Patch Name Border was double-clicked */
	FReply OnFixturePatchNameBorderDoubleClicked(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent);

	/** Called when a Fixture Patch Name was committed */
	void OnFixturePatchNameCommitted(const FText& InNewText, ETextCommit::Type InTextCommit);

	/** Generates the row that displays the Status */
	TSharedRef<SWidget> GenerateStatusRow();

	/** Generates the row that displays the Fixture ID */
	TSharedRef<SWidget> GenerateFixtureIDRow();

	/** Called when the Fixture ID Border was double-clicked */
	FReply OnFixtureIDBorderDoubleClicked(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent);

	/** Called when a Fixture ID was committed */
	void OnFixtureIDCommitted(const FText& InNewText, ETextCommit::Type InTextCommit);

	/** Generates the row that displays the Fixture Type */
	TSharedRef<SWidget> GenerateFixtureTypeRow();

	/** Called when a Fixture Type was selected */
	void OnFixtureTypeSelected(UDMXEntityFixtureType* SelectedFixtureType);

	/** Generates the row that displays the Mode */
	TSharedRef<SWidget> GenerateModeRow();

	/** Called when a Mode was selected */
	void OnModeSelected(int32 SelectedModeIndex);

	/** Generates the row that displays the Patch */
	TSharedRef<SWidget> GeneratePatchRow();

	/** Called when the Patch Border was double-clicked */
	FReply OnPatchBorderDoubleClicked(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent);

	/** Called when a Patch was committed */
	void OnPatchCommitted(const FText& InNewText, ETextCommit::Type InTextCommit);

	/** The Edit Mode the Widget should present */
	EDMXMVRFixtureListEditMode EditMode;

	/** The outermost border around the the Fixture Patch Name Column */
	TSharedPtr<SBorder> FixturePatchNameBorder;

	/** The text block to edit the Fixture Patch Name */
	TSharedPtr<SInlineEditableTextBlock> FixturePatchNameTextBlock;

	/** The text block to edit the Fixture ID */
	TSharedPtr<SInlineEditableTextBlock> FixtureIDTextBlocK;

	/** The text block to edit the Name */
	TSharedPtr<SInlineEditableTextBlock> NameTextBlock;

	/** The text block to edit the Name */
	TSharedPtr<SInlineEditableTextBlock> PatchTextBlock;

	/** The MVR Fixture List Item this row displays */
	TSharedPtr<FDMXMVRFixtureListItem> Item;

	// Slate arguments
	FSimpleDelegate OnRowRequestsStatusRefresh;
	FSimpleDelegate OnRowRequestsListRefresh;
	FIsSelected IsSelected;
};
