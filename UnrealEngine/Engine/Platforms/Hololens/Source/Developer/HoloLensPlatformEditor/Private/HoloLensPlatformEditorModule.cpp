// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyEditorModule.h"
#include "HoloLensTargetSettingsCustomization.h"
#include "HoloLensLocalizedResourcesCustomization.h"
#include "HoloLensImageResourcesCustomization.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "HoloLensTargetSettings.h"


#define LOCTEXT_NAMESPACE "FHoloLensPlatformEditorModule"

/**
 * Module for the HoloLens platform editor module.
 */
class FHoloLensPlatformEditorModule
	: public IModuleInterface
{
public:

	/** Default constructor. */
	FHoloLensPlatformEditorModule( )
	{ }

	/** Destructor. */
	~FHoloLensPlatformEditorModule( )
	{
	}

public:

	// IModuleInterface interface

	virtual void StartupModule() override
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Project", "Platforms", "HoloLens",
				LOCTEXT("TargetSettingsName", "HoloLens"),
				LOCTEXT("TargetSettingsDescription", "Settings for HoloLens"),
				GetMutableDefault<UHoloLensTargetSettings>()
			);
		}

		// register settings detail panel customization
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout(
			"HoloLensTargetSettings",
			FOnGetDetailCustomizationInstance::CreateStatic(&FHoloLensTargetSettingsCustomization::MakeInstance)
			);

		PropertyModule.RegisterCustomPropertyTypeLayout(
			"HoloLensCorePackageLocalizedResources",
			FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FHoloLensLocalizedResourcesCustomization::MakeInstance)
			);

		PropertyModule.RegisterCustomPropertyTypeLayout(
			"HoloLensCorePackageImageResources",
			FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FHoloLensCorePackageImagesCustomization::MakeInstance)
			);
	}

	virtual void ShutdownModule() override
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Platforms", "HoloLens");
		}
	}

private:

};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE(FHoloLensPlatformEditorModule, HoloLensPlatformEditor);

