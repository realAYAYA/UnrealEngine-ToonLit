// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "GenericMacTargetPlatform.h"
#include "Modules/ModuleManager.h"
#include "ISettingsModule.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "Modules/ModuleManager.h"
#include "MacTargetSettings.h"
#include "XcodeProjectSettings.h"
#include "UObject/Package.h"
#include "UObject/WeakObjectPtr.h"

#define LOCTEXT_NAMESPACE "FMacTargetPlatformModule"

/**
 * Module for Mac as a target platform
 */
class FMacTargetPlatformModule
	: public ITargetPlatformModule
{
public:

	virtual void GetTargetPlatforms(TArray<ITargetPlatform*>& TargetPlatforms) override
	{
		// Game TP
		TargetPlatforms.Add(new TGenericMacTargetPlatform<false, false, false>());
		// Editor TP
		TargetPlatforms.Add(new TGenericMacTargetPlatform<true, false, false>());
		// Server TP
		TargetPlatforms.Add(new TGenericMacTargetPlatform<false, true, false>());
		// Client TP
		TargetPlatforms.Add(new TGenericMacTargetPlatform<false, false, true>());

#if PLATFORM_WINDOWS
		// we added Mac to Windows so that the Xcode Project settings show up, but we don't
		// want to see the Mac in the Platforms dropdown
		FDataDrivenPlatformInfoRegistry::SetPlatformHiddenFromUI("Mac");
#endif
	}


public:

	// Begin IModuleInterface interface

	virtual void StartupModule() override
	{
#if WITH_ENGINE
		TargetSettings = NewObject<UMacTargetSettings>(GetTransientPackage(), "MacTargetSettings", RF_Standalone);
		
		// We need to manually load the config properties here, as this module is loaded before the UObject system is setup to do this
        GConfig->GetArray(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("TargetedRHIs"), TargetSettings->TargetedRHIs, GEngineIni);
       
        if (!GConfig->GetInt(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("MetalLanguageVersion"), TargetSettings->MetalLanguageVersion, GEngineIni))
        {
            TargetSettings->MetalLanguageVersion = 0;
        }
        
		if (!GConfig->GetBool(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("UseFastIntrinsics"), TargetSettings->UseFastIntrinsics, GEngineIni))
		{
			TargetSettings->UseFastIntrinsics = false;
		}
		
		if (!GConfig->GetBool(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("EnableMathOptimisations"), TargetSettings->EnableMathOptimisations, GEngineIni))
		{
			TargetSettings->EnableMathOptimisations = true;
		}
		
		if (!GConfig->GetInt(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("IndirectArgumentTier"), TargetSettings->IndirectArgumentTier, GEngineIni))
		{
			TargetSettings->IndirectArgumentTier = 0;
		}
		
		TargetSettings->AddToRoot();
        
        ProjectSettings = NewObject<UXcodeProjectSettings>(GetTransientPackage(), "XcodeProjectSettings", RF_Standalone);
        ProjectSettings->AddToRoot();

		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Project", "Platforms", "Mac",
				LOCTEXT("MacTargetSettingsName", "Mac"),
				LOCTEXT("MacTargetSettingsDescription", "Settings and resources for Mac platform"),
				TargetSettings
			);
            SettingsModule->RegisterSettings("Project", "Platforms", "Xcode",
                LOCTEXT("XcodeProjectSettingsName", "Xcode Projects"),
                LOCTEXT("XcodeProjectSettingsDescription", "Settings for Xcode projects"),
                ProjectSettings
            );
		}
#endif
	}

	virtual void ShutdownModule() override
	{
#if WITH_ENGINE
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Platforms", "Mac");
            SettingsModule->UnregisterSettings("Project", "Platforms", "Xcode");
		}

		if (!GExitPurge)
		{
			// If we're in exit purge, this object has already been destroyed
			TargetSettings->RemoveFromRoot();
            ProjectSettings->RemoveFromRoot();
		}
		else
		{
			TargetSettings = NULL;
            ProjectSettings = NULL;
		}
#endif
	}

	// End IModuleInterface interface


private:

	// Holds the target settings.
	UMacTargetSettings* TargetSettings;
    
    UXcodeProjectSettings* ProjectSettings;
};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE( FMacTargetPlatformModule, MacTargetPlatform);
