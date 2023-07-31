// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

#include "IAudioCodecRegistry.h"
#include "PcmCodec.h"

class FAudioExtensionsModule : public IModuleInterface
{
public:
	void StartupModule() override
	{
		FModuleManager::Get().LoadModuleChecked(TEXT("SignalProcessing"));
		FModuleManager::Get().LoadModuleChecked(TEXT("AudioMixerCore"));

		// Built in Codecs.
		Audio::ICodecRegistry::Get().RegisterCodec(Audio::Create_PcmCodec());
	}
};

IMPLEMENT_MODULE(FAudioExtensionsModule, AudioExtensions);
