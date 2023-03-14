// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "GenericMacTargetPlatform.h"
#include "Modules/ModuleManager.h"
#include "ISettingsModule.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "Modules/ModuleManager.h"
#include "MacTargetSettings.h"
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
	}


public:

	// Begin IModuleInterface interface

	virtual void StartupModule() override
	{
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

		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Project", "Platforms", "Mac",
				LOCTEXT("TargetSettingsName", "Mac"),
				LOCTEXT("TargetSettingsDescription", "Settings and resources for Mac platform"),
				TargetSettings
			);
		}
	}

	virtual void ShutdownModule() override
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Platforms", "Mac");
		}

		if (!GExitPurge)
		{
			// If we're in exit purge, this object has already been destroyed
			TargetSettings->RemoveFromRoot();
		}
		else
		{
			TargetSettings = NULL;
		}
	}

	// End IModuleInterface interface


private:

	// Holds the target settings.
	UMacTargetSettings* TargetSettings;
};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE( FMacTargetPlatformModule, MacTargetPlatform);
