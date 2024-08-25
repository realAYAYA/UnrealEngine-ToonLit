// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScriptableToolsEditorModeModule.h"
#include "Modules/ModuleManager.h"
#include "ScriptableToolsEditorModeManagerCommands.h"
#include "ScriptableToolsEditorModeStyle.h"
#include "Misc/CoreDelegates.h"

#define LOCTEXT_NAMESPACE "FScriptableToolsEditorModeModule"

void FScriptableToolsEditorModeModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FScriptableToolsEditorModeModule::OnPostEngineInit);
}

void FScriptableToolsEditorModeModule::ShutdownModule()
{
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);

	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	FScriptableToolsEditorModeManagerCommands::Unregister();
	FScriptableToolsEditorModeStyle::Shutdown();
}

void FScriptableToolsEditorModeModule::OnPostEngineInit()
{
	FScriptableToolsEditorModeStyle::Initialize();
	FScriptableToolsEditorModeManagerCommands::Register();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FScriptableToolsEditorModeModule, ScriptableToolsEditorMode)
