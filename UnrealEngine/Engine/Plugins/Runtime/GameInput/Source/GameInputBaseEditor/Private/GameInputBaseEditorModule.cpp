// Copyright Epic Games, Inc. All Rights Reserved.

#include "ISettingsModule.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "GameInputDeveloperSettings.h"

#define LOCTEXT_NAMESPACE "GameInputBaseEditor"

/**
* Editor module for Game Input that will register the UGameInputDeveloperSettings so they
* show up as editable project settings in the editor.
*/
class FGameInputBaseEditorModule final : public IModuleInterface
{
private:
	virtual void StartupModule() override
	{
		// register settings
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->RegisterSettings("Project", "Plugins", "Input",
				LOCTEXT("ControllerSettingsName", "Game Input Device Settings"),
				LOCTEXT("ControllerSettingsDescription", "Settings for the Game Input plugin."),
				GetMutableDefault<UGameInputDeveloperSettings>()
			);
		}
	}

	virtual void ShutdownModule() override
	{	
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "Input");
		}
	}
};

IMPLEMENT_MODULE(FGameInputBaseEditorModule, GameInputBaseEditor);

#undef LOCTEXT_NAMESPACE