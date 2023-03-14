// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassNavigationEditorModule.h"
#include "Modules/ModuleManager.h"
#include "AssetToolsModule.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "MassNavigationEditor"

IMPLEMENT_MODULE(FMassNavigationEditorModule, MassNavigationEditor)

void FMassNavigationEditorModule::StartupModule()
{
	// Register the details customizer
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
}

void FMassNavigationEditorModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	}
}

#undef LOCTEXT_NAMESPACE
