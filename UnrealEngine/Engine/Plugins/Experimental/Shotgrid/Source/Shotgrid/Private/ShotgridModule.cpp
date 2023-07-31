// Copyright Epic Games, Inc. All Rights Reserved.

#include "IShotgridModule.h"
#include "ShotgridSettings.h"
#include "ShotgridUIManager.h"

#include "ISettingsModule.h"
#include "ISettingsSection.h"

#define LOCTEXT_NAMESPACE "Shotgrid"

class FShotgridModule : public IShotgridModule
{
public:
	virtual void StartupModule() override
	{
		if (GIsEditor && !IsRunningCommandlet())
		{
			RegisterSettings();

			FShotgridUIManager::Initialize();
		}
	}

	virtual void ShutdownModule() override
	{
		if ((GIsEditor && !IsRunningCommandlet()))
		{
			FShotgridUIManager::Shutdown();

			UnregisterSettings();
		}
	}

protected:
	void RegisterSettings()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule != nullptr)
		{
			ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Project", "Plugins", "Shotgrid",
				LOCTEXT("ShotgridSettingsName", "ShotGrid"),
				LOCTEXT("ShotgridSettingsDescription", "Configure the ShotGrid plugin."),
				GetMutableDefault<UShotgridSettings>()
			);
		}
	}

	void UnregisterSettings()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "Shotgrid");
		}
	}
};

IMPLEMENT_MODULE(FShotgridModule, Shotgrid);

#undef LOCTEXT_NAMESPACE
