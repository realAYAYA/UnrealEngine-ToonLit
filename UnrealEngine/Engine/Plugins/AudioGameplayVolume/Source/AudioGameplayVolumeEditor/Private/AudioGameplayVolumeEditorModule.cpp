// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioGameplayVolumeEditorModule.h"
#include "Modules/ModuleManager.h"
#include "AssetToolsModule.h"
#include "PropertyEditorModule.h"
#include "ToolMenus.h"
#include "DetailCustomizations/ReverbVolumeComponentDetail.h"

void FAudioGameplayVolumeEditorModule::StartupModule()
{
	constexpr bool bRegisterLayouts = true;
	HandleCustomPropertyLayouts(bRegisterLayouts);
}

void FAudioGameplayVolumeEditorModule::ShutdownModule()
{
	constexpr bool bRegisterLayouts = false;
	HandleCustomPropertyLayouts(bRegisterLayouts);
}

void FAudioGameplayVolumeEditorModule::HandleCustomPropertyLayouts(bool bRegisterLayouts)
{
	// Register detail customizations
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	if (bRegisterLayouts)
	{
		PropertyModule.RegisterCustomClassLayout("ReverbVolumeComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FReverbVolumeComponentDetail::MakeInstance));
	}
	else
	{
		PropertyModule.UnregisterCustomClassLayout("ReverbVolumeComponent");
	}
}

IMPLEMENT_MODULE(FAudioGameplayVolumeEditorModule, AudioGameplayVolumeEditor);
