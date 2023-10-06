// Copyright Epic Games, Inc. All Rights Reserved.
#include "OculusAudioDllManager.h"
#include "Misc/Paths.h"
#include "OculusAudioSettings.h"
#include "Stats/Stats.h"


// forward decleration should match OAP_Globals
extern "C" ovrResult ovrAudio_GetPluginContext(ovrAudioContext* context, unsigned clientType);
extern "C" ovrResult OSP_FMOD_SetUnitScale(float unitScale);

#define OVRA_CLIENT_TYPE_FMOD          5
#define OVRA_CLIENT_TYPE_WWISE_UNKNOWN 12

FOculusAudioLibraryManager& FOculusAudioLibraryManager::Get()
{
	static FOculusAudioLibraryManager* sInstance;
	if (sInstance == nullptr)
	{
		sInstance = new FOculusAudioLibraryManager();
	}

	check(sInstance != nullptr);
	return *sInstance;
}

FOculusAudioLibraryManager::FOculusAudioLibraryManager()
	: OculusAudioDllHandle(nullptr), NumInstances(0),
	bInitialized(false), CachedPluginContext(nullptr)
{
}

FOculusAudioLibraryManager::~FOculusAudioLibraryManager()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
}

void FOculusAudioLibraryManager::Initialize()
{
	if (NumInstances == 0)
	{
		if (!LoadDll())
		{
			UE_LOG(LogAudio, Error, TEXT("Oculus Audio: Failed to load OVR Audio dll"));
			check(false);
			return;
		}
	}

	NumInstances++;
	if (!bInitialized)
	{
		// Check the version number
		int32 MajorVersionNumber = -1;
		int32 MinorVersionNumber = -1;
		int32 PatchNumber = -1;

		const char* OvrVersionString = OVRA_CALL(ovrAudio_GetVersion)(&MajorVersionNumber, &MinorVersionNumber, &PatchNumber);
		if (MajorVersionNumber != OVR_AUDIO_MAJOR_VERSION || MinorVersionNumber != OVR_AUDIO_MINOR_VERSION)
		{
			UE_LOG(LogAudio, Warning, TEXT("Oculus Audio: Using mismatched OVR Audio SDK Version! %d.%d vs. %d.%d"), OVR_AUDIO_MAJOR_VERSION, OVR_AUDIO_MINOR_VERSION, MajorVersionNumber, MinorVersionNumber);
			return;
		}
		bInitialized = true;
	}
}

void FOculusAudioLibraryManager::Shutdown()
{
	if (NumInstances == 0)
	{
		// This means we failed to load the OVR Audio module during initialization and there's nothing to shutdown.
		return;
	}

	NumInstances--;

	if (NumInstances == 0)
	{
		// Shutdown OVR audio
		ReleaseDll();
		bInitialized = false;
	}
}


bool FOculusAudioLibraryManager::LoadDll()
{
	if (OculusAudioDllHandle == nullptr)
	{
		const TCHAR* WWISE_DLL_NAME = TEXT("OculusSpatializerWwise");
		const TCHAR* FMOD_DLL_NAME = TEXT("OculusSpatializerFMOD");
		const TCHAR* UE_DLL_NAME = (sizeof(void*) == 4) ? TEXT("ovraudio32") : TEXT("ovraudio64");

#if PLATFORM_WINDOWS 

#if WITH_EDITOR
		const FString WwisePath = FPaths::ProjectDir() / FString::Printf(TEXT("Binaries/Win64/"));
		const FString FMODPath = FPaths::ProjectPluginsDir() / FString::Printf(TEXT("FMODStudio/Binaries/Win64/"));
		const FString UEPath = FPaths::EngineDir() / FString::Printf(TEXT("Binaries/ThirdParty/Oculus/Audio/Win64/"));
#else
		const FString WwisePath = FPaths::ProjectDir() / FString::Printf(TEXT("Binaries/Win64/"));
		const FString FMODPath = FPaths::ProjectPluginsDir() / FString::Printf(TEXT("FMODStudio/Binaries/Win64/")); //TODO verify this
		const FString UEPath = FPaths::ProjectDir() / FString::Printf(TEXT("../Engine/Binaries/ThirdParty/Oculus/Audio/Win64/"));
#endif

		FString Path;
		const TCHAR* DLL_NAME = nullptr;
		if (FPaths::FileExists(WwisePath + WWISE_DLL_NAME + ".dll"))
		{
			ClientType = OVRA_CLIENT_TYPE_WWISE_UNKNOWN;
			Path = WwisePath;
			DLL_NAME = WWISE_DLL_NAME;
			UE_LOG(LogAudio, Display, TEXT("Oculus Audio: OculusSpatializerWwise.dll found, using the Wwise version of the Oculus Audio UE integration"));
		}
		else if (FPaths::FileExists(FMODPath + FMOD_DLL_NAME + ".dll"))
		{
			ClientType = OVRA_CLIENT_TYPE_FMOD;
			Path = FMODPath;
			DLL_NAME = FMOD_DLL_NAME;
			UE_LOG(LogAudio, Display, TEXT("Oculus Audio: OculusSpatializerFMOD.dll found, using the FMOD version of the Oculus Audio UE integration"));
		}
		else
		{
			ClientType = -1;
			Path = UEPath;
			DLL_NAME = UE_DLL_NAME;
			UE_LOG(LogAudio, Display, TEXT("Oculus Audio: Middleware plugins not found, assuming native UE AudioMixer"));
		}


		UE_LOG(LogAudio, Display, TEXT("Oculus Audio: Attempting to load Oculus Spatializer DLL: %s (from %s)"), *Path, DLL_NAME);

		FPlatformProcess::PushDllDirectory(*Path);
		OculusAudioDllHandle = FPlatformProcess::GetDllHandle(*(Path + DLL_NAME));
		FPlatformProcess::PopDllDirectory(*Path);

#elif PLATFORM_ANDROID
		FString Path = TEXT("lib");
		OculusAudioDllHandle = FPlatformProcess::GetDllHandle(*(Path + WWISE_DLL_NAME + ".so"));
		if (OculusAudioDllHandle != nullptr)
		{
			UE_LOG(LogAudio, Display, TEXT("Oculus Audio: %s found, using the Wwise version of the Oculus Audio UE integration"), WWISE_DLL_NAME);
			return true;
		}
		OculusAudioDllHandle = FPlatformProcess::GetDllHandle(*(Path + FMOD_DLL_NAME + ".so"));
		if (OculusAudioDllHandle != nullptr)
		{
			UE_LOG(LogAudio, Display, TEXT("Oculus Audio: %s found, using the FMOD version of the Oculus Audio UE integration"), FMOD_DLL_NAME);
			return true;
		}
		OculusAudioDllHandle = FPlatformProcess::GetDllHandle(*(Path + UE_DLL_NAME + ".so"));
		if (OculusAudioDllHandle != nullptr)
		{
			UE_LOG(LogAudio, Display, TEXT("Oculus Audio: Middleware plugins not found, %s found, assuming native UE AudioMixer"), UE_DLL_NAME);
			return true;
		}
		else
		{
			UE_LOG(LogAudio, Error, TEXT("Oculus Audio: Unable to load Oculus Audio UE integratiton"), UE_DLL_NAME);
		}
#endif

		return (OculusAudioDllHandle != nullptr);
	}
	return true;
}

void FOculusAudioLibraryManager::ReleaseDll()
{
#if PLATFORM_WINDOWS
	if (NumInstances == 0 && OculusAudioDllHandle)
	{
		FPlatformProcess::FreeDllHandle(OculusAudioDllHandle);
		OculusAudioDllHandle = nullptr;
	}
#endif
}

bool FOculusAudioLibraryManager::UpdatePluginContext(float DeltaTime)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FOculusAudioLibraryManager_UpdatePluginContext);

	ovrAudioContext Context = GetPluginContext();
	ovrResult Result = OVRA_CALL(ovrAudio_UpdateRoomModel)(Context, 1.0f);
	check(Result == ovrSuccess || Result == ovrError_AudioUninitialized);

	UOculusAudioSettings* settings = GetMutableDefault<UOculusAudioSettings>();
	Result = OVRA_CALL(ovrAudio_SetPropagationQuality)(Context, settings->PropagationQuality);
	check(Result == ovrSuccess);

	return true;
}

ovrAudioContext FOculusAudioLibraryManager::GetPluginContext()
{
	if (CachedPluginContext == nullptr)
	{
		auto GetPluginContext = OVRA_CALL(ovrAudio_GetPluginContext);
		if (GetPluginContext != nullptr)
		{
			ovrResult Result = GetPluginContext(&CachedPluginContext, ClientType);
			check(Result == ovrSuccess);

			// FMOD plugin needs to know about the unit scale
			auto SetFMODUnitScale = OVRA_CALL(OSP_FMOD_SetUnitScale);
			if (SetFMODUnitScale != nullptr)
			{
				Result = SetFMODUnitScale(0.01f);
				check(Result == ovrSuccess);
			}

			// Tick the scene from here since there is no listener
			auto TickDelegate = FTickerDelegate::CreateRaw(this, &FOculusAudioLibraryManager::UpdatePluginContext);
			TickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(TickDelegate);
		}
	}
	return CachedPluginContext;
}