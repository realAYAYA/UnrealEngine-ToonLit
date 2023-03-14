// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#include "PropertyEditorModule.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#include "RemoteControlProtocolDMX.h"
#include "RemoteControlProtocolDMXSettings.h"
#include "RemoteControlProtocolDMXSettingsDetails.h"
#include "RemoteControlDMXProtocolEntityExtraSettingCustomization.h"

/**
 * Remote control protocol DMX editor that allows have editor functionality for the protocol
 */
class FRemoteControlProtocolDMXEditorModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface interface
};

void FRemoteControlProtocolDMXEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyEditorModule.RegisterCustomPropertyTypeLayout(
		FRemoteControlDMXProtocolEntityExtraSetting::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(FRemoteControlDMXProtocolEntityExtraSettingCustomization::MakeInstance));

	PropertyEditorModule.RegisterCustomClassLayout(
		URemoteControlProtocolDMXSettings::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(FRemoteControlProtocolDMXSettingsDetails::MakeInstance));
}

void FRemoteControlProtocolDMXEditorModule::ShutdownModule()
{
	if (UObjectInitialized() && FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyEditorModule.UnregisterCustomPropertyTypeLayout(FRemoteControlDMXProtocolEntityExtraSetting::StaticStruct()->GetFName());
		PropertyEditorModule.UnregisterCustomClassLayout(URemoteControlProtocolDMXSettings::StaticClass()->GetFName());
	}
}


IMPLEMENT_MODULE(FRemoteControlProtocolDMXEditorModule, RemoteControlProtocolDMXEditorModule);
