// Copyright Epic Games, Inc. All Rights Reserved.
#include "AudioMeterSubmixAnalyzer.h"

#include "AudioBusSubsystem.h"
#include "AudioDefines.h"
#include "AudioDeviceManager.h"
#include "AudioInsightsModule.h"
#include "AudioMeter.h"
#include "AudioMixerDevice.h"
#include "AudioMixerSubmix.h"
#include "Editor.h"

namespace UE::Audio::Insights
{
	FAudioMeterSubmixAnalyzer::FAudioMeterSubmixAnalyzer(TWeakObjectPtr<USoundSubmix> InSoundSubmix)
	{
		SetSubmix(InSoundSubmix);
	}

	FAudioMeterSubmixAnalyzer::~FAudioMeterSubmixAnalyzer()
	{
		UnregisterAudioBusFromSubmix();
	}

	void FAudioMeterSubmixAnalyzer::SetSubmix(TWeakObjectPtr<USoundSubmix> InSoundSubmix)
	{
		using namespace ::Audio;

		if (SoundSubmix.IsValid())
		{
			UnregisterAudioBusFromSubmix();
			SoundSubmix.Reset();
		}

		AudioMeterAnalyzer.RebuildAudioMeter();

		SoundSubmix = InSoundSubmix;

		if (!SoundSubmix.IsValid())
		{
			return;
		}
		
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

		FMixerSubmixWeakPtr MixerSubmixWeakPtr = MixerDevice->GetSubmixInstance(SoundSubmix.Get());
		if (!MixerSubmixWeakPtr.IsValid())
		{
			return;
		}

		// Register audio bus in submix
		const TObjectPtr<UAudioBus> AudioBus = AudioMeterAnalyzer.GetAudioMeter()->GetAudioBus();
		if (!AudioBus)
		{
			return;
		}

		const FAudioBusKey AudioBusKey(AudioBus->GetUniqueID());
		const int32 AudioBusNumChannels = AudioBus->GetNumChannels();

		FAudioThread::RunCommandOnAudioThread([MixerDevice, MixerSubmixWeakPtr, AudioBusKey, AudioBusNumChannels]()
		{
			TObjectPtr<UAudioBusSubsystem> AudioBusSubsystem = MixerDevice->GetSubsystem<UAudioBusSubsystem>();
			check(AudioBusSubsystem);

			if (FMixerSubmixPtr MixerSubmix = MixerSubmixWeakPtr.Pin();
				MixerSubmix.IsValid())
			{
				MixerSubmix->RegisterAudioBus(AudioBusKey, AudioBusSubsystem->AddPatchInputForAudioBus(AudioBusKey, MixerDevice->GetNumOutputFrames(), AudioBusNumChannels));
			}
		});
	}

	void FAudioMeterSubmixAnalyzer::UnregisterAudioBusFromSubmix()
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

		if (!SoundSubmix.IsValid())
		{
			return;
		}

		FMixerSubmixWeakPtr MixerSubmixWeakPtr = MixerDevice->GetSubmixInstance(SoundSubmix.Get());
		if (!MixerSubmixWeakPtr.IsValid())
		{
			return;
		}

		// Unregister audio bus from submix
		const TObjectPtr<UAudioBus> AudioBus = AudioMeterAnalyzer.GetAudioMeter()->GetAudioBus();
		if (!AudioBus)
		{
			return;
		}

		const FAudioBusKey AudioBusKey(AudioBus->GetUniqueID());

		FAudioThread::RunCommandOnAudioThread([MixerSubmixWeakPtr, AudioBusKey]()
		{
			if (FMixerSubmixPtr MixerSubmix = MixerSubmixWeakPtr.Pin();
				MixerSubmix.IsValid())
			{
				MixerSubmix->UnregisterAudioBus(AudioBusKey);
			}
		});
	}

	TSharedRef<SAudioMeter> FAudioMeterSubmixAnalyzer::GetWidget()
	{
		return AudioMeterAnalyzer.GetAudioMeter()->GetWidget();
	}
} // namespace UE::Audio::Insights
