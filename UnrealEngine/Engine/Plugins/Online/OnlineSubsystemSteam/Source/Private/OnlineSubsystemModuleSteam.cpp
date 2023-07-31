// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Modules/ModuleManager.h"
#include "OnlineSubsystemSteamModule.h"
#include "OnlineSubsystemModule.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemSteamPrivate.h"
#include "OnlineSubsystemSteam.h"
#include "SteamSharedModule.h"
#include "HAL/PlatformProcess.h"

IMPLEMENT_MODULE(FOnlineSubsystemSteamModule, OnlineSubsystemSteam);

//HACKTASTIC (Needed to keep delete function from being stripped out and crashing when protobuffers deallocate memory)
void* HackDeleteFunctionPointer = (void*)(void(*)(void*))(::operator delete[]);

/**
 * Class responsible for creating instance(s) of the subsystem
 */
class FOnlineFactorySteam final : public IOnlineFactory
{

private:

	/** Single instantiation of the STEAM interface */
	static FOnlineSubsystemSteamPtr SteamSingleton;

	void DestroySubsystem()
	{
		if (SteamSingleton.IsValid())
		{
			SteamSingleton->Shutdown();
			SteamSingleton = nullptr;
		}
	}

public:

	FOnlineFactorySteam() {}
	virtual ~FOnlineFactorySteam() 
	{
		DestroySubsystem();
	}

	virtual IOnlineSubsystemPtr CreateSubsystem(FName InstanceName)
	{
		if (!SteamSingleton.IsValid())
		{
			SteamSingleton = MakeShared<FOnlineSubsystemSteam, ESPMode::ThreadSafe>(InstanceName);
			if (SteamSingleton->IsEnabled())
			{
				if(!SteamSingleton->Init())
				{
					UE_LOG_ONLINE(Warning, TEXT("Steam API failed to initialize!"));
					DestroySubsystem();
				}
			}
			else
			{
				UE_CLOG_ONLINE(IsRunningDedicatedServer() || IsRunningGame(), Warning, TEXT("Steam API disabled!"));
				DestroySubsystem();
			}

			return SteamSingleton;
		}

		UE_LOG_ONLINE(Warning, TEXT("Can't create more than one instance of Steam online subsystem!"));
		return nullptr;
	}
};

FOnlineSubsystemSteamPtr FOnlineFactorySteam::SteamSingleton = nullptr;


void FOnlineSubsystemSteamModule::StartupModule()
{
	FSteamSharedModule& SharedModule = FSteamSharedModule::Get();

	// Load the Steam modules before first call to API
	if (SharedModule.AreSteamDllsLoaded())
	{
		// Create and register our singleton factory with the main online subsystem for easy access
		SteamFactory = new FOnlineFactorySteam();

		FOnlineSubsystemModule& OSS = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
		OSS.RegisterPlatformService(STEAM_SUBSYSTEM, SteamFactory);
	}
	else
	{
		UE_LOG_ONLINE(Warning, TEXT("Steam SDK %s libraries not present at %s or failed to load!"), STEAM_SDK_VER, *SharedModule.GetSteamModulePath());
	}
}

void FOnlineSubsystemSteamModule::ShutdownModule()
{
	FOnlineSubsystemModule& OSS = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
	OSS.UnregisterPlatformService(STEAM_SUBSYSTEM);

	delete SteamFactory;
	SteamFactory = nullptr;
}
