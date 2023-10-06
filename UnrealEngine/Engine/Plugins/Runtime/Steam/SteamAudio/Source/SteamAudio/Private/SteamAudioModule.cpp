//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "SteamAudioModule.h"
#include "Misc/Paths.h"
#include "Features/IModularFeatures.h"

DEFINE_LOG_CATEGORY(LogSteamAudio);

IMPLEMENT_MODULE(SteamAudio::FSteamAudioModule, SteamAudio)

namespace SteamAudio
{
	static bool bModuleStartedUp = false;

	FSteamAudioModule::FSteamAudioModule()
	{
	}

	FSteamAudioModule::~FSteamAudioModule()
	{
	}

	void FSteamAudioModule::StartupModule()
	{
		check(bModuleStartedUp == false);

		bModuleStartedUp = true;

		UE_LOG(LogSteamAudio, Log, TEXT("FSteamAudioModule Startup"));

		UE_LOG(LogSteamAudio, Warning, TEXT("The Steam Audio plugin is deprecated and will be removed in a future engine release. Get Valve's version here: https://valvesoftware.github.io/steam-audio/doc/unreal/getting-started.html"))
	}

	void FSteamAudioModule::ShutdownModule()
	{
		UE_LOG(LogSteamAudio, Log, TEXT("FSteamAudioModule Shutdown"));

		check(bModuleStartedUp == true);

		bModuleStartedUp = false;
	}

}

