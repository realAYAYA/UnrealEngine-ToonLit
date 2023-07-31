// Copyright Epic Games, Inc. All Rights Reserved.

#include "SteamSharedModule.h"
#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Modules/ModuleManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ConfigCacheIni.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"

#ifndef STEAM_SDK_INSTALLED
#error Steam SDK not located! Expected to be found in Engine/Source/ThirdParty/Steamworks/{SteamVersion}
#endif // STEAM_SDK_INSTALLED

// Steam API for Initialization
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
#include "steam/steam_gameserver.h"
THIRD_PARTY_INCLUDES_END

DEFINE_LOG_CATEGORY_STATIC(LogSteamShared, Log, All);

IMPLEMENT_MODULE(FSteamSharedModule, SteamShared);

FString FSteamSharedModule::GetSteamModulePath() const
{
	const FString STEAM_SDK_ROOT_PATH(TEXT("Binaries/ThirdParty/Steamworks"));
#if PLATFORM_WINDOWS

	#if PLATFORM_64BITS
		return FPaths::EngineDir() / STEAM_SDK_ROOT_PATH / STEAM_SDK_VER_PATH / TEXT("Win64/");
	#else
		return FPaths::EngineDir() / STEAM_SDK_ROOT_PATH / STEAM_SDK_VER_PATH / TEXT("Win32/");
	#endif	//PLATFORM_64BITS

#elif PLATFORM_LINUX

	#if PLATFORM_64BITS
		return FPaths::EngineDir() / STEAM_SDK_ROOT_PATH / STEAM_SDK_VER_PATH / TEXT("x86_64-unknown-linux-gnu/");
	#else
		return FPaths::EngineDir() / STEAM_SDK_ROOT_PATH / STEAM_SDK_VER_PATH / TEXT("i686-unknown-linux-gnu/");
	#endif	//PLATFORM_64BITS
	
#elif PLATFORM_MAC
	return FPaths::EngineDir() / STEAM_SDK_ROOT_PATH / STEAM_SDK_VER_PATH / TEXT("Mac/");
#else

	return FString();

#endif	//PLATFORM_WINDOWS
}

bool FSteamSharedModule::CanLoadClientDllsOnServer() const
{
#if PLATFORM_WINDOWS
	if (IsRunningDedicatedServer())
	{
		return true;
	}
#endif

	return false;
}

void FSteamSharedModule::StartupModule()
{
	// On startup load the modules. Anyone who uses this shared library can guarantee
	// that Steamworks will be started and be ready to go for them.
	LoadSteamModules();
}

void FSteamSharedModule::ShutdownModule()
{
	// Check to see if anyone has a DLL handle still.
	if (AreSteamDllsLoaded())
	{
		// Make sure everyone cleaned up their instances properly.
		if (SteamClientObserver.IsValid()) // If our weakptr is still valid, that means there are cases that didn't clean up somehow.
		{
			// If they have not, warn to them they need to clean up in the future and force the deletion.
			uint32 NumSharedReferences = SteamClientObserver.Pin().GetSharedReferenceCount();
			// Make sure to subtract 1 here as we just created a new sharedptr in order to get the reference count
			// (this sharedptr is out of scope so it does not matter)
			UE_LOG(LogSteamShared, Warning, TEXT("There are still %d additional Steam instances in use. These must be shutdown before unloading the module!"), NumSharedReferences - 1);
		}
		SteamClientObserver.Reset(); // Force the clearing of our weakptr.

		// Do the above but this time with the server observer.
		if (SteamServerObserver.IsValid())
		{
			uint32 NumSharedReferences = SteamServerObserver.Pin().GetSharedReferenceCount();
			UE_LOG(LogSteamShared, Warning, TEXT("There are still %d additional Steam server instances in use. These must be shutdown before unloading the module!"), NumSharedReferences - 1);
		}
		SteamServerObserver.Reset();
	}

	// Here we are no longer loaded, so we need to override any DLL handles still open and unlink the DLLs.
	UnloadSteamModules();
}

TSharedPtr<class FSteamClientInstanceHandler> FSteamSharedModule::ObtainSteamClientInstanceHandle()
{
	if (SteamClientObserver.IsValid())
	{
		return SteamClientObserver.Pin();
	}
	else
	{
		// Create the original base object, and store our weakptrs.
		TSharedPtr<FSteamClientInstanceHandler> BaseInstance = MakeShareable(new FSteamClientInstanceHandler(this));
		SteamClientObserver = BaseInstance;
		if (BaseInstance->IsInitialized()) // Make sure the SteamAPI was initialized properly.
		{
			// This is safe because we'll end up incrementing the BaseInstance refcounter before we go out of scope.
			// Thus the InstanceHandler will not be deleted before we return.
			return SteamClientObserver.Pin();
		}
		else
		{
			// We don't want to hold any references to failed instances.
			SteamClientObserver = nullptr;
		}
	}

	return nullptr;
}

TSharedPtr<class FSteamServerInstanceHandler> FSteamSharedModule::ObtainSteamServerInstanceHandle()
{
	if (SteamServerObserver.IsValid())
	{
		return SteamServerObserver.Pin();
	}
	else
	{
		// Create the original base object, and store our weakptrs.
		TSharedPtr<FSteamServerInstanceHandler> BaseInstance = MakeShareable(new FSteamServerInstanceHandler(this));
		SteamServerObserver = BaseInstance;
		if (BaseInstance->IsInitialized()) // Make sure the SteamAPI was initialized properly.
		{
			// This is safe because we'll end up incrementing the BaseInstance refcounter before we go out of scope.
			// Thus the InstanceHandler will not be deleted before we return.
			return SteamServerObserver.Pin();
		}
		else
		{
			// We don't want to hold any references to failed instances.
			SteamServerObserver = nullptr;
		}
	}

	return nullptr;
}

bool FSteamSharedModule::AreSteamDllsLoaded() const
{
	bool bLoadedClientDll = true;
	bool bLoadedServerDll = true;

#if LOADING_STEAM_LIBRARIES_DYNAMICALLY
	bLoadedClientDll = (SteamDLLHandle != nullptr) ? true : false;
#endif // LOADING_STEAM_LIBRARIES_DYNAMICALLY
#if LOADING_STEAM_SERVER_LIBRARY_DYNAMICALLY
	bLoadedServerDll = IsRunningDedicatedServer() ? ((SteamServerDLLHandle != nullptr || !bForceLoadSteamClientDll) ? true : false) : true;
#endif // LOADING_STEAM_SERVER_LIBRARY_DYNAMICALLY

	return bLoadedClientDll && bLoadedServerDll;
}

void FSteamSharedModule::LoadSteamModules()
{
	if (AreSteamDllsLoaded())
	{
		return;
	}
		
	UE_LOG(LogSteamShared, Display, TEXT("Loading Steam SDK %s"), STEAM_SDK_VER);

#if PLATFORM_WINDOWS

#if PLATFORM_64BITS
	FString Suffix("64");
#else
	FString Suffix;
#endif // PLATFORM_64BITS

	FString RootSteamPath = GetSteamModulePath();
	FPlatformProcess::PushDllDirectory(*RootSteamPath);
	SteamDLLHandle = FPlatformProcess::GetDllHandle(*(RootSteamPath + "steam_api" + Suffix + ".dll"));
	if (IsRunningDedicatedServer() && FCommandLine::IsInitialized() && FParse::Param(FCommandLine::Get(), TEXT("force_steamclient_link")))
	{
		FString SteamClientDLL("steamclient" + Suffix + ".dll"),
			SteamTierDLL("tier0_s" + Suffix + ".dll"),
			SteamVSTDDLL("vstdlib_s" + Suffix + ".dll");

		UE_LOG(LogSteamShared, Log, TEXT("Attempting to force linking the steam client dlls."));
		bForceLoadSteamClientDll = true;
		SteamServerDLLHandle = FPlatformProcess::GetDllHandle(*(RootSteamPath + SteamClientDLL));
		if(!SteamServerDLLHandle)
		{
			UE_LOG(LogSteamShared, Error, TEXT("Could not find the %s, %s and %s DLLs, make sure they are all located at %s! These dlls can be located in your Steam install directory."),
				*SteamClientDLL, *SteamTierDLL, *SteamVSTDDLL, *RootSteamPath);
		}
	}
	FPlatformProcess::PopDllDirectory(*RootSteamPath);
#elif PLATFORM_MAC || (PLATFORM_LINUX && LOADING_STEAM_LIBRARIES_DYNAMICALLY)

#if PLATFORM_MAC
	FString SteamModuleFileName("libsteam_api.dylib");
#else
	FString SteamModuleFileName("libsteam_api.so");
#endif // PLATFORM_MAC

	SteamDLLHandle = FPlatformProcess::GetDllHandle(*SteamModuleFileName);
	if (SteamDLLHandle == nullptr)
	{
		// try bundled one
#if PLATFORM_MAC
		if (FParse::Param(FCommandLine::Get(), TEXT("dllerrors")))
#endif // PLATFORM_MAC
		{
			UE_LOG(LogSteamShared, Warning, TEXT("Could not find system one, loading bundled %s."), *SteamModuleFileName);
		}
		FString RootSteamPath = GetSteamModulePath();
		SteamDLLHandle = FPlatformProcess::GetDllHandle(*(RootSteamPath + SteamModuleFileName));
	}

	if (SteamDLLHandle)
	{
		UE_LOG(LogSteamShared, Display, TEXT("Loaded %s at %p"), *SteamModuleFileName, SteamDLLHandle);
	}
	else
	{
		UE_LOG(LogSteamShared, Warning, TEXT("Unable to load %s, Steam functionality will not work"), *SteamModuleFileName);
		return;
	}


#elif PLATFORM_LINUX
	UE_LOG(LogSteamShared, Log, TEXT("libsteam_api.so is linked explicitly and should be already loaded."));
	return;
#endif // PLATFORM_WINDOWS
	UE_LOG(LogSteamShared, Log, TEXT("Steam SDK Loaded!"));
}

void FSteamSharedModule::UnloadSteamModules()
{
	// Only free the handles if no one is using them anymore.
	// There's no need to check AreSteamDllsLoaded as this is done individually below.
	if (!SteamClientObserver.IsValid() && !SteamServerObserver.IsValid())
	{
#if LOADING_STEAM_LIBRARIES_DYNAMICALLY
		UE_LOG(LogSteamShared, Log, TEXT("Freeing the Steam Loaded Modules..."));

		if (SteamDLLHandle != nullptr)
		{
			FPlatformProcess::FreeDllHandle(SteamDLLHandle);
			SteamDLLHandle = nullptr;
		}

		if (SteamServerDLLHandle != nullptr)
		{
			FPlatformProcess::FreeDllHandle(SteamServerDLLHandle);
			SteamServerDLLHandle = nullptr;
		}
#endif	//LOADING_STEAM_LIBRARIES_DYNAMICALLY
	}
}

FSteamInstanceHandlerBase::FSteamInstanceHandlerBase() : 
	bInitialized(false)
{
	// Grab the gameport for game communications.
	if (FParse::Value(FCommandLine::Get(), TEXT("Port="), GamePort) == false)
	{
		GConfig->GetInt(TEXT("URL"), TEXT("Port"), GamePort, GEngineIni);
	}
}

FSteamClientInstanceHandler::FSteamClientInstanceHandler(FSteamSharedModule* SteamInitializer) :
	FSteamInstanceHandlerBase()
{
	// A module must be loaded in order for us to initialize the Steam API.
	if (SteamInitializer != nullptr && SteamInitializer->AreSteamDllsLoaded())
	{
		if (SteamAPI_Init())
		{
			UE_LOG(LogSteamShared, Verbose, TEXT("SteamAPI initialized"));
			bInitialized = true;
			return;
		}

		// The conditions mentioned in this print can be found at https://partner.steamgames.com/doc/sdk/api#initialization_and_shutdown
		UE_LOG(LogSteamShared, Warning, TEXT("SteamAPI failed to initialize, conditions not met."));
		return;
	}
	UE_LOG(LogSteamShared, Warning, TEXT("SteamAPI failed to initialize as the Dlls are not loaded."));
}

void FSteamClientInstanceHandler::InternalShutdown()
{
	UE_LOG(LogSteamShared, Log, TEXT("Unloading the Steam API..."));
	SteamAPI_Shutdown();
}


FSteamServerInstanceHandler::FSteamServerInstanceHandler(FSteamSharedModule* SteamInitializer) :
	FSteamInstanceHandlerBase()
{
	if (SteamInitializer == nullptr || !SteamInitializer->AreSteamDllsLoaded())
	{
		UE_LOG(LogSteamShared, Warning, TEXT("Steam Server API failed to initialize as the Dlls are not loaded."));
		return;
	}

	// Get the multihome address. If there isn't one, this server will be set to listen on the any address.
	uint32 LocalServerIP = 0;
	FString MultiHome;
	if (FParse::Value(FCommandLine::Get(), TEXT("MULTIHOME="), MultiHome) && !MultiHome.IsEmpty())
	{
		TSharedPtr<FInternetAddr> MultiHomeIP = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetAddressFromString(MultiHome);
		if (MultiHomeIP.IsValid())
		{
			MultiHomeIP->GetIp(LocalServerIP);
		}
	}

	// Allow the command line to override the default query port for master server communications
	if (FParse::Value(FCommandLine::Get(), TEXT("QueryPort="), QueryPort) == false)
	{
		if (!GConfig->GetInt(TEXT("OnlineSubsystemSteam"), TEXT("GameServerQueryPort"), QueryPort, GEngineIni))
		{
			QueryPort = 27015;
		}
	}

	// Set VAC
	bool bVACEnabled = false;
	GConfig->GetBool(TEXT("OnlineSubsystemSteam"), TEXT("bVACEnabled"), bVACEnabled, GEngineIni);

	// Set GameVersions
	FString GameVersion;
	GConfig->GetString(TEXT("OnlineSubsystemSteam"), TEXT("GameVersion"), GameVersion, GEngineIni);
	if (GameVersion.Len() == 0)
	{
		UE_LOG(LogSteamShared, Warning, TEXT("[OnlineSubsystemSteam].GameVersion is not set. Server advertising will fail"));
	}

	UE_LOG(LogSteamShared, Verbose, TEXT("Initializing Steam Game Server IP: 0x%08X Port: %d QueryPort: %d"), LocalServerIP, GamePort, QueryPort);

	if (SteamGameServer_Init(LocalServerIP, GamePort, QueryPort,
		(bVACEnabled ? eServerModeAuthenticationAndSecure : eServerModeAuthentication),
		TCHAR_TO_UTF8(*GameVersion)))
	{
		UE_LOG(LogSteamShared, Verbose, TEXT("Steam Dedicated Server API initialized."));
		bInitialized = true;
	}
	else
	{
		UE_LOG(LogSteamShared, Warning, TEXT("Steam Dedicated Server API failed to initialize."));
	}
}

void FSteamServerInstanceHandler::InternalShutdown()
{
	// Log off of the steam master servers, removes this server immediately from the backend.
	if (SteamGameServer() && SteamGameServer()->BLoggedOn())
	{
		SteamGameServer()->LogOff();
	}

	// By getting here, we must be the last instance of the object, thus we should be deleted.
	// If no one is using the API, we can shut it down.
	UE_LOG(LogSteamShared, Log, TEXT("Unloading the Steam server API..."));
	SteamGameServer_Shutdown();
}

bool FSteamInstanceHandlerBase::CanCleanUp() const
{
	return bInitialized && FSteamSharedModule::IsAvailable() && FSteamSharedModule::Get().AreSteamDllsLoaded();
}

void FSteamInstanceHandlerBase::Destroy()
{
	if (CanCleanUp())
	{
		InternalShutdown();
	}
}

