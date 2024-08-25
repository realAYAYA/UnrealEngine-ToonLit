// Copyright Epic Games, Inc. All Rights Reserved.
#include "AudioOscilloscopeAnalyzer.h"

#include "AudioBusSubsystem.h"
#include "AudioDefines.h"
#include "AudioDeviceManager.h"
#include "AudioInsightsModule.h"
#include "AudioOscilloscope.h"
#include "AudioMixerDevice.h"
#include "AudioMixerSubmix.h"

namespace UE::Audio::Insights
{
	namespace FAudioOscilloscopeAnalyzerPrivate
	{
		TSharedRef<AudioWidgets::FAudioOscilloscope> CreateAudioOscilloscope(const float InTimeWindowMs,
			const float InMaxTimeWindowMs,
			const float InAnalysisPeriodMs,
			const EAudioPanelLayoutType InPanelLayoutType)
		{
			using namespace ::Audio;

			const IAudioInsightsModule& InsightsModule = FModuleManager::GetModuleChecked<IAudioInsightsModule>(IAudioInsightsModule::GetName());
			const FDeviceId AudioDeviceId = InsightsModule.GetDeviceId();

			if (FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get())
			{
				if (const FMixerDevice* MixerDevice = static_cast<const FMixerDevice*>(AudioDeviceManager->GetAudioDeviceRaw(AudioDeviceId)))
				{
					return MakeShared<AudioWidgets::FAudioOscilloscope>(AudioDeviceId,
						MixerDevice->GetNumDeviceChannels(),
						InTimeWindowMs,
						InMaxTimeWindowMs,
						InAnalysisPeriodMs,
						InPanelLayoutType);
				}
			}
			
			return MakeShared<AudioWidgets::FAudioOscilloscope>(AudioDeviceId, 1, InTimeWindowMs, InMaxTimeWindowMs, InAnalysisPeriodMs, InPanelLayoutType);
		}
	}

	FAudioOscilloscopeAnalyzer::FAudioOscilloscopeAnalyzer(TWeakObjectPtr<USoundSubmix> InSoundSubmix)
		: AudioOscilloscope(FAudioOscilloscopeAnalyzerPrivate::CreateAudioOscilloscope(TimeWindowMs, MaxTimeWindowMs, AnalysisPeriodMs, PanelLayoutType))
	{
		RebuildAudioOscilloscope(InSoundSubmix);
	}

	FAudioOscilloscopeAnalyzer::~FAudioOscilloscopeAnalyzer()
	{
		CleanupAudioOscilloscope();
	}

	void FAudioOscilloscopeAnalyzer::RebuildAudioOscilloscope(TWeakObjectPtr<USoundSubmix> InSoundSubmix)
	{
		using namespace ::Audio;

		if (SoundSubmix.IsValid())
		{
			CleanupAudioOscilloscope();
		}

		SoundSubmix = InSoundSubmix;

		FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get();
		if (!AudioDeviceManager)
		{
			return;
		}

		const IAudioInsightsModule& InsightsModule = FModuleManager::GetModuleChecked<IAudioInsightsModule>(IAudioInsightsModule::GetName());
		const FDeviceId AudioDeviceId = InsightsModule.GetDeviceId();

		const FMixerDevice* MixerDevice = static_cast<FMixerDevice*>(AudioDeviceManager->GetAudioDeviceRaw(AudioDeviceId));
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

		AudioOscilloscope->CreateDataProvider(AudioDeviceId, TimeWindowMs, MaxTimeWindowMs, AnalysisPeriodMs, PanelLayoutType);
		AudioOscilloscope->CreateOscilloscopeWidget(MixerDevice->GetNumDeviceChannels(), PanelLayoutType);

		// Start processing
		AudioOscilloscope->StartProcessing();
		
		// Register audio bus in submix
		const TObjectPtr<UAudioBus> AudioBus = AudioOscilloscope->GetAudioBus();
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

	void FAudioOscilloscopeAnalyzer::CleanupAudioOscilloscope()
	{
		using namespace ::Audio;

		FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get();
		if (!AudioDeviceManager)
		{
			return;
		}

		const IAudioInsightsModule& InsightsModule = FModuleManager::GetModuleChecked<IAudioInsightsModule>(IAudioInsightsModule::GetName());
		const FDeviceId AudioDeviceId = InsightsModule.GetDeviceId();

		const FMixerDevice* MixerDevice = static_cast<FMixerDevice*>(AudioDeviceManager->GetAudioDeviceRaw(AudioDeviceId));
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
		const TObjectPtr<UAudioBus> AudioBus = AudioOscilloscope->GetAudioBus();
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

		// Stop processing
		AudioOscilloscope->StopProcessing();

		SoundSubmix.Reset();
	}
} // namespace UE::Audio::Insights
