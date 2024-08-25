// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeCategories.h"
#include "IDMXControlConsoleEditorModule.h"
#include "Misc/AssetCategoryPath.h"

class FMenuBuilder;


/** Editor Module for DMXControlConsole */
class FDMXControlConsoleEditorModule
	: public IDMXControlConsoleEditorModule
{
public:
	/** Constructor */
	FDMXControlConsoleEditorModule();

	//~ Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~End IModuleInterface interface

	//~ Begin IDMXControlConsoleEditorModule interface
	virtual FAssetCategoryPath GetControlConsoleCategory() const override { return ControlConsoleCategory; }
	//~End IDMXControlConsoleEditorModule interface

	/** Gets the DMX Editor asset category */
	static EAssetTypeCategories::Type GetDMXEditorAssetCategory() { return DMXEditorAssetCategory; }

	/** Opens the ControlConsole */
	static void OpenControlConsole();

	/** Name identifier for the Control Console Editor app */
	static const FName ControlConsoleEditorAppIdentifier;

private:
	/** Registers Control Console commands in the Level Editor */
	void RegisterLevelEditorCommands();

	/** Registers an extender for the Level Editor Toolbar DMX Menu */
	static void RegisterDMXMenuExtender();

	/** Extends the Level Editor Toolbar DMX Menu */
	static void ExtendDMXMenu(FMenuBuilder& MenuBuilder);

	// Called at the end of UEngine::Init, right before loading PostEngineInit modules for both normal execution and commandlets
	void OnPostEnginInit();

	/** The category path under which Control Console assets are nested. */
	FAssetCategoryPath ControlConsoleCategory;

	/** The DMX Editor asset category */
	static EAssetTypeCategories::Type DMXEditorAssetCategory;

	/** Name of the Control Console Editor Tab  */
	static const FName ControlConsoleEditorTabName;
};
