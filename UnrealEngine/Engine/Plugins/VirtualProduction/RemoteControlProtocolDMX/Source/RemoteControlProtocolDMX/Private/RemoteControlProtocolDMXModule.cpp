// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

#include "IRemoteControlProtocolModule.h"
#include "RemoteControlProtocolDMX.h"
#include "RemoteControlProtocolDMXSettings.h"

#if WITH_EDITOR
#include "ISettingsModule.h"
#endif // WITH_EDITOR

#define LOCTEXT_NAMESPACE "RemoteControlProtocolDMXModule"

/**
 * DMX remote control module
 */
class FRemoteControlProtocolDMXModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override
	{
		const IRemoteControlProtocolModule& RemoteControlProtocolModule = IRemoteControlProtocolModule::Get();
		if (!RemoteControlProtocolModule.IsRCProtocolsDisable())
		{
			IRemoteControlProtocolModule::Get().AddProtocol(FRemoteControlProtocolDMX::ProtocolName, MakeShared<FRemoteControlProtocolDMX>());
		}
		
#if WITH_EDITOR
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		// Register DMX Remote Control global settings
		if (SettingsModule)
		{
			SettingsModule->RegisterSettings("Project", "Plugins", "Remote Control DMX Protocol",
				LOCTEXT("ProjectSettings_Label", "Remote Control DMX Protocol"),
				LOCTEXT("ProjectSettings_Description", "Configure MIDI remote control plugin global settings"),
				GetMutableDefault<URemoteControlProtocolDMXSettings>()
			);
		}
#endif // WITH_EDITOR
	}

	virtual void ShutdownModule() override
	{
#if WITH_EDITOR
		// Unregister MIDI Remote Control global settings
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule)
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "Remote Control DMX Protocol");
		}
#endif // WITH_EDITOR
	}

	//~ End IModuleInterface
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRemoteControlProtocolDMXModule, RemoteControlProtocolDMX);
