// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module.h"
#include "XRCreativeSettings.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "XRCreativeLog.h"


#define LOCTEXT_NAMESPACE "FXRCreativeModule"


DEFINE_LOG_CATEGORY(LogXRCreative);


void FXRCreativeModule::StartupModule()
{
	if(ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "XRCreativeSettings",
			LOCTEXT("RuntimeSettingsName", "XRCreative Settings"), LOCTEXT("RuntimeSettingsDescription", "XRCreative Settings"),
			GetMutableDefault<UXRCreativeSettings>());

		SettingsModule->RegisterSettings("Editor", "Plugins", "XRCreativeEditorSettings",
			LOCTEXT("EditorSettingsName", "XRCreative Editor Settings"), LOCTEXT("EditorSettingsDescription", "XRCreative Editor Settings"),
			GetMutableDefault<UXRCreativeEditorSettings>());
	}
}


void FXRCreativeModule::ShutdownModule()
{

	if(ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "XRCreativeSettings");
		SettingsModule->UnregisterSettings("Editor", "Plugins", "XRCreativeEditorSettings");
	}
}


IMPLEMENT_MODULE(FXRCreativeModule, XRCreative)


#undef LOCTEXT_NAMESPACE
