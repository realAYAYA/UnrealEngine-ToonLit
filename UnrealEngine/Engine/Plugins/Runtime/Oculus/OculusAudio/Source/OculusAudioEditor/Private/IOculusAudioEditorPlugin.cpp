// Copyright Epic Games, Inc. All Rights Reserved.

#include "IOculusAudioEditorPlugin.h"
#include "OculusAudioSettings.h"
#include "OculusAudioSourceSettingsFactory.h"
#include "OculusAmbisonicSettingsFactory.h"

#include "Modules/ModuleManager.h"
#include "ISettingsModule.h"

void FOculusAudioEditorPlugin::StartupModule()
{
    // Register asset types
    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
    {
		AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_OculusAudioSourceSettings));
		AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_OculusAmbisonicsSettings));

        ISettingsModule* SettingsModule = FModuleManager::Get().GetModulePtr<ISettingsModule>("Settings");
        if (SettingsModule)
        {
            SettingsModule->RegisterSettings("Project", "Plugins", "Oculus Audio", NSLOCTEXT("OculusAudio", "Oculus Audio", "Oculus Audio"),
                NSLOCTEXT("OculusAudio", "Configure Oculus Audio settings", "Configure Oculus Audio settings"), GetMutableDefault<UOculusAudioSettings>());
        }
    }
}

void FOculusAudioEditorPlugin::ShutdownModule()
{
}

IMPLEMENT_MODULE(FOculusAudioEditorPlugin, OculusAudioEditor)
