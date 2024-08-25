// Copyright Epic Games, Inc. All Rights Reserved.
#include "AudioMeterAnalyzer.h"

#include "AudioDeviceManager.h"
#include "AudioInsightsModule.h"
#include "AudioMeter.h"
#include "AudioMixerDevice.h"

namespace UE::Audio::Insights
{
	namespace FAudioMeterAnalyzerPrivate
	{
		TSharedRef<AudioWidgets::FAudioMeter> CreateAudioMeter(TWeakObjectPtr<UAudioBus> InExternalAudioBus)
		{
			using namespace ::Audio;

			const IAudioInsightsModule& InsightsModule = FModuleManager::GetModuleChecked<IAudioInsightsModule>(IAudioInsightsModule::GetName());
			const FDeviceId AudioDeviceId = InsightsModule.GetDeviceId();

			if (const FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get())
			{
				if (const FMixerDevice* MixerDevice = static_cast<const FMixerDevice*>(AudioDeviceManager->GetAudioDeviceRaw(AudioDeviceId)))
				{
					return MakeShared<AudioWidgets::FAudioMeter>(InExternalAudioBus.IsValid() ? InExternalAudioBus->GetNumChannels() : MixerDevice->GetNumDeviceChannels(),
						AudioDeviceId,
						InExternalAudioBus.Get());
				}
			}
			
			return MakeShared<AudioWidgets::FAudioMeter>(1, AudioDeviceId);
		}
	}

	FAudioMeterAnalyzer::FAudioMeterAnalyzer(TWeakObjectPtr<UAudioBus> InExternalAudioBus)
		: AudioMeter(FAudioMeterAnalyzerPrivate::CreateAudioMeter(InExternalAudioBus))
	{
		
	}

	void FAudioMeterAnalyzer::RebuildAudioMeter(TWeakObjectPtr<UAudioBus> InExternalAudioBus)
	{
		using namespace ::Audio;

		FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get();
		if (!AudioDeviceManager)
		{
			return;
		}

		const IAudioInsightsModule& InsightsModule = FModuleManager::GetModuleChecked<IAudioInsightsModule>(IAudioInsightsModule::GetName());
		const FDeviceId AudioDeviceId = InsightsModule.GetDeviceId();

		const FMixerDevice* MixerDevice = static_cast<const FMixerDevice*>(AudioDeviceManager->GetAudioDeviceRaw(AudioDeviceId));
		if (!MixerDevice)
		{
			return;
		}

		AudioMeter->Init(InExternalAudioBus.IsValid() ? InExternalAudioBus->GetNumChannels() : MixerDevice->GetNumDeviceChannels(), AudioDeviceId, InExternalAudioBus.Get());
	}
} // namespace UE::Audio::Insights
