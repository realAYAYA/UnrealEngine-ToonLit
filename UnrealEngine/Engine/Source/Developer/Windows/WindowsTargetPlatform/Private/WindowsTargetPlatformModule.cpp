// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "ISettingsModule.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "GenericWindowsTargetPlatform.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/WeakObjectPtr.h"
#if WITH_ENGINE
#include "CookedEditorTargetPlatform.h"
#endif

#define LOCTEXT_NAMESPACE "FWindowsTargetPlatformModule"


/**
 * Implements the Windows target platform module.
 */
class FWindowsTargetPlatformModule
	: public ITargetPlatformModule
{
public:

	/** Destructor. */
	~FWindowsTargetPlatformModule( )
	{

	}

public:

	// this is an example of a hotfix, declared here for no particular reason. Once we have other examples, it can be deleted.
#if 0
	void HotfixTest( void *InPayload, int PayloadSize )
	{
		check(sizeof(FTestHotFixPayload) == PayloadSize);
		
		FTestHotFixPayload* Payload = (FTestHotFixPayload*)InPayload;
		UE_LOG(LogTemp, Log, TEXT("Hotfix Test %s"), *Payload->Message);
		Payload->Result = Payload->ValueToReturn;
	}
#endif

public:

	virtual void GetTargetPlatforms(TArray<ITargetPlatform*>& TargetPlatforms) override
	{
		// Game TP
		TargetPlatforms.Add(new TGenericWindowsTargetPlatform<FWindowsPlatformProperties<false, false, false>>());
		// Editor TP
		TargetPlatforms.Add(new TGenericWindowsTargetPlatform<FWindowsPlatformProperties<true, false, false>>());
		// Server TP
		TargetPlatforms.Add(new TGenericWindowsTargetPlatform<FWindowsPlatformProperties<false, true, false>>());
		// Client TP
		TargetPlatforms.Add(new TGenericWindowsTargetPlatform<FWindowsPlatformProperties<false, false, true>>());

#if WITH_ENGINE
		// currently this TP requires the engine for allowing GameDelegates usage
		bool bSupportCookedEditor;
		if (GConfig->GetBool(TEXT("CookedEditorSettings"), TEXT("bSupportCookedEditor"), bSupportCookedEditor, GGameIni) && bSupportCookedEditor)
		{
			TargetPlatforms.Add(new TCookedEditorTargetPlatform<FWindowsEditorTargetPlatformParent>());
			TargetPlatforms.Add(new TCookedCookerTargetPlatform<FWindowsEditorTargetPlatformParent>());
		}
#endif
	}

public:

	// IModuleInterface interface

	virtual void StartupModule() override
	{
		// this is an example of a hotfix, declared here for no particular reason. Once we have other examples, it can be deleted.
#if 0
		FCoreDelegates::GetHotfixDelegate(EHotfixDelegates::Test).BindRaw(this, &FWindowsTargetPlatformModule::HotfixTest);
#endif
	}

	virtual void ShutdownModule() override
	{
		// this is an example of a hotfix, declared here for no particular reason. Once we have other examples, it can be deleted.
#if 0
		FCoreDelegates::GetHotfixDelegate(EHotfixDelegates::Test).Unbind();
#endif

		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	}

};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE(FWindowsTargetPlatformModule, WindowsTargetPlatform);
