// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

enum class ECheckBoxState : uint8;
struct FDMXEntityFixturePatchRef;
class FReply;
class FUICommandList;
class SDMXControlConsoleReadOnlyFixturePatchList;
class UDMXControlConsoleFaderGroup;
class UDMXEntityFixturePatch;


/** A container for FixturePatchRow widgets */
class SDMXControlConsoleEditorFixturePatchVerticalBox
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMXControlConsoleEditorFixturePatchVerticalBox)
	{}

	SLATE_END_ARGS()

	/** Destructor */
	~SDMXControlConsoleEditorFixturePatchVerticalBox();

	/** Constructs the widget */
	void Construct(const FArguments& InArgs);

	/** Refreshes the widget */
	void ForceRefresh();

protected:
	//~ Begin SWidget interface
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override { return FReply::Handled(); }
	//~ End SWidget interface

private:
	/** Registers commands for this widget */
	void RegisterCommands();

	/** Generates a toolbar for FixturePatchList widget */
	TSharedRef<SWidget> GenerateFixturePatchListToolbar();

	/** Creates a context menu for a row in the FixturePatchList widget */
	TSharedPtr<SWidget> CreateRowContextMenu();

	/** Creates a menu for the Add Patch combo button */
	TSharedRef<SWidget> CreateAddPatchMenu();

	/** Edits the given Fader Group according to the given Fixture Patch */
	void GenerateFaderGroupFromFixturePatch(UDMXControlConsoleFaderGroup* FaderGroup, UDMXEntityFixturePatch* FixturePatch);

	/** Called when row selection changes in FixturePatchList */
	void OnRowSelectionChanged(const TSharedPtr<FDMXEntityFixturePatchRef> NewSelection, ESelectInfo::Type SelectInfo);

	/** Called when a row is clicked in FixturePatchList */
	void OnRowClicked(const TSharedPtr<FDMXEntityFixturePatchRef> ItemClicked);

	/** Called when a row is double clicked in FixturePatchList */
	void OnRowDoubleClicked(const TSharedPtr<FDMXEntityFixturePatchRef> ItemClicked);

	/** Called when checkbox state of the whole FixturePatchList changes */
	void OnListCheckBoxStateChanged(ECheckBoxState CheckBoxState);

	/** Called when checkbox state of a row changes in FixturePatchList */
	void OnRowCheckBoxStateChanged(ECheckBoxState CheckBoxState, const TSharedPtr<FDMXEntityFixturePatchRef> InFixturePatchRef);

	/** Called to generate fader groups from fixture patches on last row */
	void OnGenerateFromFixturePatchOnLastRow();

	/** Called to generate fader group from fixture patches on a new row */
	void OnGenerateFromFixturePatchOnNewRow();

	/** Called to generate the selected fader group from a fixture patch */
	void OnGenerateSelectedFaderGroupFromFixturePatch();

	/** Called on Add All Patches button click to generate Fader Groups form a Library */
	FReply OnAddAllPatchesClicked();

	/** Called to get the checkbox state of the whole FixturePatchList */
	ECheckBoxState IsListChecked() const;

	/** Called to get wheter a row is checked or not in FixturePatchList */
	ECheckBoxState IsRowChecked(const TSharedPtr<FDMXEntityFixturePatchRef> InFixturePatchRef) const;

	/** Called to mute/unmute all Fader Groups in current Control Console */
	void OnMuteAllFaderGroups(bool bMute, bool bOnlyActive = false) const;

	/** Gets wheter any Fader Group is muted/unmuted */
	bool IsAnyFaderGroupsMuted(bool bMute, bool bOnlyActive = false) const;

	/** Gets whether the given FixturePatchRef should be enabled or not */
	bool IsFixturePatchListRowEnabled(const TSharedPtr<FDMXEntityFixturePatchRef> InFixturePatchRef) const;

	/** Gets whether the given FixturePatchRef should be visible or not */
	bool IsFixturePatchListRowVisible(const TSharedPtr<FDMXEntityFixturePatchRef> InFixturePatchRef) const;

	/** True if a FixturePatchList row is visble in Default Layout */
	bool IsRowVisibleInDefaultLayout(const TSharedPtr<FDMXEntityFixturePatchRef> InFixturePatchRef) const;

	/** True if a FixturePatchList row is visble in User Layout */
	bool IsRowVisibleInUserLayout(const TSharedPtr<FDMXEntityFixturePatchRef> InFixturePatchRef) const;

	/** Gets enable state for Add All Patches button when a DMX Library is selected */
	bool IsAddAllPatchesButtonEnabled() const;

	/** Gets wheter add next action is executable or not */
	bool CanExecuteAddNext() const;

	/** Gets wheter add row action is executable or not */
	bool CanExecuteAddRow() const;

	/** Gets wheter add selected action is executable or not */
	bool CanExecuteAddSelected() const;

	/** Gets visibility for FixturePatchList toolbar  */
	EVisibility GetFixturePatchListToolbarVisibility() const;

	/** Reference to FixturePatchList widget */
	TSharedPtr<SDMXControlConsoleReadOnlyFixturePatchList> FixturePatchList;

	/** Command list for the Asset Combo Button */
	TSharedPtr<FUICommandList> CommandList;
};
