// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamCore.h"

#include "LogVCamCore.h"
#include "UI/VCamWidget.h"
#include "Util/WidgetSnapshotUtils.h"
#include "VCamCoreUserSettings.h"

#include "EnhancedInputDeveloperSettings.h"
#if WITH_EDITOR
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#endif
#include "Misc/CoreDelegates.h"

#define LOCTEXT_NAMESPACE "FVCamCoreModule"

namespace UE::VCamCore::Private
{
	void FVCamCoreModule::StartupModule()
	{
		RegisterSettings();

		FCoreDelegates::OnPostEngineInit.AddLambda([]()
		{
			UEnhancedInputDeveloperSettings* Settings = GetMutableDefault<UEnhancedInputDeveloperSettings>();
			UE_CLOG(!Settings->bEnableUserSettings, LogVCamCore, Log, TEXT("Overriding Settings->bEnableUserSettings = true because it is required for VCam to work properly."));
			Settings->bEnableUserSettings = true;
		});
	}

	void FVCamCoreModule::ShutdownModule()
	{
		UnregisterSettings();
	}

	void FVCamCoreModule::RegisterSettings()
	{
#if WITH_EDITOR
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule != nullptr)
		{
			ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Project", "Plugins", "VirtualCameraCore",
				LOCTEXT("VirtualCameraUserSettingsName", "Virtual Camera Core"),
				LOCTEXT("VirtualCameraUserSettingsDescription", "Configure the Virtual Camera Core settings."),
				GetMutableDefault<UVirtualCameraCoreUserSettings>());
		}
#endif
	}

	void FVCamCoreModule::UnregisterSettings()
	{
#if WITH_EDITOR
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "VirtualCameraCore");
		}
#endif
	}

	WidgetSnapshotUtils::Private::FWidgetSnapshotSettings FVCamCoreModule::GetSnapshotSettings() const
	{
		// In the future this could be exposed via project settings or via registration functions on IVCamCoreModule
		const TSet<TSubclassOf<UWidget>> AllowedWidgetClasses { UVCamWidget::StaticClass() };
		const TSet<const FProperty*> AllowedProperties { UVCamWidget::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UVCamWidget, Connections)) };
		return WidgetSnapshotUtils::Private::FWidgetSnapshotSettings{
			AllowedWidgetClasses,
			AllowedProperties
		};
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(UE::VCamCore::Private::FVCamCoreModule, VCamCore)