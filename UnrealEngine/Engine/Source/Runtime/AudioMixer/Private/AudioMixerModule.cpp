// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioMixerModule.h"
#include "Modules/ModuleManager.h"

class FAudioMixerModule : public IModuleInterface
{
public:

	virtual void StartupModule() override
	{
		FModuleManager::Get().LoadModuleChecked(TEXT("AudioMixerCore"));
		FModuleManager::Get().LoadModuleChecked(TEXT("SignalProcessing"));
	}
};

IMPLEMENT_MODULE(FAudioMixerModule, AudioMixer);
