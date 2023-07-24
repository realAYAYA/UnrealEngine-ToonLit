// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

#include "AudioExtensionsLog.h"
#include "HAL/LowLevelMemStats.h"
#include "IAudioCodecRegistry.h"
#include "IAudioExtensionPlugin.h"
#include "PcmCodec.h"

DECLARE_LLM_MEMORY_STAT(TEXT("AudioSpatializationPlugins"), STAT_AudioSpatializationPluginsLLM, STATGROUP_LLMFULL);
LLM_DEFINE_TAG(Audio_SpatializationPlugins, NAME_None, TEXT("Audio"), GET_STATFNAME(STAT_AudioSpatializationPluginsLLM), GET_STATFNAME(STAT_AudioSummaryLLM));

DEFINE_LOG_CATEGORY(LogAudioExtensions)

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
