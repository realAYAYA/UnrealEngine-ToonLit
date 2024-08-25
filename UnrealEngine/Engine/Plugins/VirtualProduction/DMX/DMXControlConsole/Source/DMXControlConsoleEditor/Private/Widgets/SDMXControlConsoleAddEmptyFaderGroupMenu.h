// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class FUICommandList;
class UDMXControlConsoleEditorModel;
class UDMXControlConsoleFaderGroup;


/**  A menu to add empty fader groups to the control console. */
class SDMXControlConsoleAddEmptyFaderGroupMenu
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMXControlConsoleAddEmptyFaderGroupMenu)
	{}

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, UDMXControlConsoleEditorModel* InEditorModel);

private:
	/** Registers commands for the menu */
	void RegisterCommands();

	/** Returns true if the 'add empty to right' option is avialable */
	bool CanAddEmptyToTheRight() const;

	/** Adds an empty fader group to the right */
	void AddEmptyToTheRight();

	/** Returns true if the 'add empty on a new row' option is avialable */
	bool CanAddEmptyOnNewRow() const;

	/** Adds an empty fader group on a new row */
	void AddEmptyOnNewRow();

	/** Adds an empty fader group to the Control Console */
	UDMXControlConsoleFaderGroup* AddEmptyFaderGroup() const;

	/** Command list for this widget */
	TSharedPtr<FUICommandList> CommandList;

	/** Weak reference to the Control Console editor model */
	TWeakObjectPtr<UDMXControlConsoleEditorModel> EditorModel;
};
