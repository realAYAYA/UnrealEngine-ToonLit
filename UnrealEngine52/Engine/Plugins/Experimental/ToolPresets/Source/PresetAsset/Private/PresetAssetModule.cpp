// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "PresetAsset.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "FPresetAssetModule"


/**
 * Implements the PresetAsset module.
 */
class FPresetAssetModule
	: public IModuleInterface
{
public:

	//~ IModuleInterface interface

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual void OnPostEngineInit();
	virtual bool SupportsDynamicReloading() override
	{
		return true;
	}

protected:

	TArray<FName> ClassesToUnregisterOnShutdown;
};


// This function will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
void FPresetAssetModule::StartupModule()
{
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FPresetAssetModule::OnPostEngineInit);
}

// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
// we call this function before unloading the module.
void FPresetAssetModule::ShutdownModule()
{
	// Unregister customizations
	FPropertyEditorModule* PropertyEditorModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor");
	if (PropertyEditorModule)
	{
		for (FName ClassName : ClassesToUnregisterOnShutdown)
		{
			PropertyEditorModule->UnregisterCustomClassLayout(ClassName);
		}
	}
}

void FPresetAssetModule::OnPostEngineInit()
{
	ClassesToUnregisterOnShutdown.Reset();

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
}


IMPLEMENT_MODULE(FPresetAssetModule, PresetAsset);

#undef LOCTEXT_NAMESPACE