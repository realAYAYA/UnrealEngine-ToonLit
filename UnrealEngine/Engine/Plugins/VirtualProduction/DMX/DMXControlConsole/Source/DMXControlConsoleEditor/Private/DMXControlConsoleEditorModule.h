// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDMXControlConsoleEditorModule.h"

class FMenuBuilder;
class FSpawnTabArgs;
class IAssetTypeActions;
class SDockTab;


/** Editor Module for DMXControlConsole */
class FDMXControlConsoleEditorModule
	: public IDMXControlConsoleEditorModule
{
public:
	//~ Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~End IModuleInterface interface

	/** Opens the ControlConsole */
	static void OpenControlConsole();

private:
	/** Registers Control Console commands in the Level Editor */
	void RegisterLevelEditorCommands();

	/** Registers an extender for the Level Editor Toolbar DMX Menu */
	static void RegisterDMXMenuExtender();

	/** Extends the the Level Editor Toolbar DMX Menu */
	static void ExtendDMXMenu(FMenuBuilder& MenuBuilder);

	/** Spawns the ControlConsole Tab */
	static TSharedRef<SDockTab> OnSpawnControlConsoleTab(const FSpawnTabArgs& InSpawnTabArgs);

	/** Name of the ControlConsole Tab  */
	static const FName ControlConsoleTabName;
};
