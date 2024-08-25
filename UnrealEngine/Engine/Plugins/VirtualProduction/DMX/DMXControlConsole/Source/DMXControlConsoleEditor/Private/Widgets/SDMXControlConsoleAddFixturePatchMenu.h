// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class FUICommandList;
class UDMXControlConsoleEditorModel;
class UDMXControlConsoleFaderGroup;
class UDMXEntityFixturePatch;


/** 
 * A menu to add fixture patches to the control console.
 * Construct or use SetFixturePatches to specify the patches to add.
 */
class SDMXControlConsoleAddFixturePatchMenu
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMXControlConsoleAddFixturePatchMenu)
	{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> InFixturePatches, UDMXControlConsoleEditorModel* InEditorModel);

	/** Sets the fixture patches which can be added from this widget */
	void SetFixturePatches(TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> FixturePatches);

private:
	/** Registers commands for the menu */
	void RegisterCommands();

	/** Returns true if the 'add patches to right' option is avialable */
	bool CanAddPatchesToTheRight() const;

	/** Adds patches as fixture groups to the right */
	void AddPatchesToTheRight();

	/** Returns true if the 'add patches on a new row' option is avialable */
	bool CanAddPatchesOnNewRow() const;

	/** Adds patches as fixture groups on a new row */
	void AddPatchesOnNewRow();

	/** Returns true if the 'set patch on fader group' option is avialable */
	bool CanSetPatchOnFaderGroup() const;

	/** Sets the fixture patch on the selected fader group */
	void SetPatchOnFaderGroup();

	/** Groups patches in a fader group controller to the right */
	void GroupPatchesToTheRight();

	/** Returns true if the 'group patches to right' option is avialable */
	bool CanGroupPatchesToTheRight() const;

	/** Groups patches in a fader group controller on a new row */
	void GroupPatchesOnNewRow();

	/** Returns true if the 'group patches on a new row' option is avialable */
	bool CanGroupPatchesOnNewRow() const;

	/** Gets all available fader groups according to the current selected fixture patches */
	TArray<UDMXControlConsoleFaderGroup*> GetFaderGroupsFromFixturePatches();

	/** Fixture patches available when adding from this menu */
	TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> FixturePatches;

	/** Command list for this widget */
	TSharedPtr<FUICommandList> CommandList;

	/** Weak reference to the Control Console editor model */
	TWeakObjectPtr<UDMXControlConsoleEditorModel> EditorModel;
};
