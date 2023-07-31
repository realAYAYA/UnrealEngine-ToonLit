// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Styling/SlateTypes.h"

class FToolBarBuilder;
class FMenuBuilder;

class FNewGizmoEnableModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Action Callbacks */
	void ToggleNewGizmosActive();
	ECheckBoxState AreNewGizmosActive();
	bool CanToggleNewGizmos();
	static void ShowGizmoOptions();
private:

	void RegisterMenus();

	void OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

private:
	TSharedPtr<class FUICommandList> PluginCommands;
	bool bGizmosEnabled = false;
	/** Whether currently in testing mode or not. */
	static bool bTestingModeEnabled;
};
