// Copyright Epic Games, Inc. All Rights Reserved.

#include "GooglePADEditor.h"
#include "ISettingsModule.h"
#include "GooglePADRuntimeSettings.h"

/**
 * Implements the GooglePADEditor module.
 */

#define LOCTEXT_NAMESPACE "GooglePAD"

void FGooglePADEditorModule::StartupModule()
{
	// register settings
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if (SettingsModule != nullptr)	
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "GooglePAD",
			LOCTEXT("GooglePADSettingsName", "GooglePAD"),
			LOCTEXT("GooglePADSettingsDescription", "Project settings for GooglePAD plugin"),
			GetMutableDefault<UGooglePADRuntimeSettings>()
		);
	}
}

void FGooglePADEditorModule::ShutdownModule()
{
	// unregister settings
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if (SettingsModule != nullptr)
	{
       		SettingsModule->UnregisterSettings("Project", "Plugins", "GooglePAD");
	}
}


IMPLEMENT_MODULE(FGooglePADEditorModule, GooglePADEditor);

#undef LOCTEXT_NAMESPACE
