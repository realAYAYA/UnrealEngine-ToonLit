// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorModule.h"
#include "RemoteControlProtocolMIDI.h"
#include "RemoteControlProtocolMIDISettings.h"
#include "DetailCustomizations/RemoteControlMIDIDeviceCustomization.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FRemoteControlProtocolMIDIEditorModule"

/**
 * MIDI remote control editor module
 */
class FRemoteControlProtocolMIDIEditorModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyEditorModule.RegisterCustomPropertyTypeLayout(FRemoteControlMIDIDevice::StaticStruct()->GetFName(),
			FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FRemoteControlMIDIDeviceCustomization::MakeInstance));
	}

	virtual void ShutdownModule() override
	{
		if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
		{
			FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyEditorModule.UnregisterCustomPropertyTypeLayout(FRemoteControlMIDIDevice::StaticStruct()->GetFName());
			PropertyEditorModule.NotifyCustomizationModuleChanged();
		}
	}
	//~ End IModuleInterface
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRemoteControlProtocolMIDIEditorModule, RemoteControlProtocolMIDIEditor);
