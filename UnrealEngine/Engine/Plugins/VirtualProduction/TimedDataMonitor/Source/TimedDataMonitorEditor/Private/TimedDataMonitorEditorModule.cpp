// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#include "Framework/Docking/WorkspaceItem.h"
#include "ISettingsModule.h"
#include "STimedDataMonitorPanel.h"
#include "TimedDataMonitorEditorSettings.h"
#include "WorkspaceMenuStructureModule.h"
#include "WorkspaceMenuStructure.h"


#define LOCTEXT_NAMESPACE "TimedDataMonitorEditorModule"


class FTimedDataMonitorEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		STimedDataMonitorPanel::RegisterNomadTabSpawner(WorkspaceMenu::GetMenuStructure().GetLevelEditorVirtualProductionCategory());

		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->RegisterSettings("Editor", "Plugins", "Timed Data Monitor",
				LOCTEXT("SettingsName", "Timed Data Monitor"),
				LOCTEXT("Description", "Configure the Timed Data Monitor panel."),
				GetMutableDefault<UTimedDataMonitorEditorSettings>()
			);
		}
	}

	virtual void ShutdownModule() override
	{
		if (!IsRunningCommandlet() && UObjectInitialized() && !IsEngineExitRequested())
		{
			ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
			if (SettingsModule != nullptr)
			{
				SettingsModule->UnregisterSettings("Editor", "Plugins", "Timed Data Monitor");
			}
			STimedDataMonitorPanel::UnregisterNomadTabSpawner();
		}
	}
};

IMPLEMENT_MODULE(FTimedDataMonitorEditorModule, TimedDataMonitorEditor);

#undef LOCTEXT_NAMESPACE
