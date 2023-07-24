// Copyright Epic Games, Inc. All Rights Reserved.

#include "GoogleARCoreBaseModule.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "Features/IModularFeatures.h"
#include "Features/IModularFeature.h"
#include "ARSessionConfig.h"

#include "GoogleARCoreMotionController.h"
#include "GoogleARCoreXRTrackingSystem.h"
#include "GoogleARCoreDevice.h"
#include "GoogleARCoreCookSupport.h"

#define LOCTEXT_NAMESPACE "GoogleARCore"

class FGoogleARCoreBaseModule : public IGoogleARCoreBaseModule
{
public:
	/** IHeadMountedDisplayModule implementation*/
	/** Returns the key into the HMDPluginPriority section of the config file for this module */
	virtual FString GetModuleKeyName() const override
	{
		return TEXT("GoogleARCoreHMD");
	}

	/**
	 *
	 */
	virtual bool IsHMDConnected()
	{
		// @todo arcore do we have an API for querying this?
		return true;
	}

	/**
	 * Attempts to create a new head tracking device interface
	 *
	 * @return	Interface to the new head tracking device, if we were able to successfully create one
	 */
	virtual TSharedPtr< class IXRTrackingSystem, ESPMode::ThreadSafe > CreateTrackingSystem() override;

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	FGoogleARCoreMotionController ControllerInstance;

#if WITH_EDITORONLY_DATA
	FGoogleARCoreSessionConfigCookSupport SessionConfigCookSupport;
#endif

};

IMPLEMENT_MODULE(FGoogleARCoreBaseModule, GoogleARCoreBase)

TSharedPtr< class IXRTrackingSystem, ESPMode::ThreadSafe > FGoogleARCoreBaseModule::CreateTrackingSystem()
{
#if PLATFORM_ANDROID
	auto ARCoreSystem = MakeShared<FGoogleARCoreXRTrackingSystem, ESPMode::ThreadSafe>();
	ARCoreSystem->GetARCompositionComponent()->InitializeARSystem();
	FGoogleARCoreDevice::GetInstance()->SetARSystem(ARCoreSystem->GetARCompositionComponent());
	return ARCoreSystem;
#else
	return TSharedPtr<class IXRTrackingSystem, ESPMode::ThreadSafe>();
#endif
}

void FGoogleARCoreBaseModule::StartupModule()
{
	ensureMsgf(FModuleManager::Get().LoadModule("AugmentedReality"), TEXT("ARCore depends on the AugmentedReality module.") );

	// Register editor settings:
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (SettingsModule != nullptr)
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "GoogleARCore",
			LOCTEXT("GoogleARCoreSetting", "GoogleARCore"),
			LOCTEXT("GoogleARCoreSettingDescription", "Settings of the GoogleARCore plugin"),
			GetMutableDefault<UGoogleARCoreEditorSettings>());
	}

	// Complete ARCore setup.
	FGoogleARCoreDevice::GetInstance()->OnModuleLoaded();

	// Register VR-like controller interface.
	ControllerInstance.RegisterController();

#if WITH_EDITORONLY_DATA
	SessionConfigCookSupport.RegisterModuleFeature();
#endif

	// Register IHeadMountedDisplayModule
	IHeadMountedDisplayModule::StartupModule();

	FModuleManager::LoadModulePtr<IModuleInterface>("AugmentedReality");
}

void FGoogleARCoreBaseModule::ShutdownModule()
{
	// Unregister IHeadMountedDisplayModule
	IHeadMountedDisplayModule::ShutdownModule();

	// Unregister VR-like controller interface.
	ControllerInstance.UnregisterController();

#if WITH_EDITORONLY_DATA
	SessionConfigCookSupport.UnregisterModuleFeature();
#endif

	// Complete ARCore teardown.
	FGoogleARCoreDevice::GetInstance()->OnModuleUnloaded();

	// Unregister editor settings.
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if (SettingsModule != nullptr)
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "GoogleARCore");
	}
}

#undef LOCTEXT_NAMESPACE
