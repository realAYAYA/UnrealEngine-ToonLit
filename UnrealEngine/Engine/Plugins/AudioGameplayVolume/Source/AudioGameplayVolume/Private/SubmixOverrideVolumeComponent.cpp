// Copyright Epic Games, Inc. All Rights Reserved.

#include "SubmixOverrideVolumeComponent.h"
#include "AudioDevice.h"
#include "AudioGameplayFlags.h"
#include "AudioGameplayVolumeListener.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SubmixOverrideVolumeComponent)

constexpr TCHAR FProxyMutator_SubmixOverride::MutatorSubmixOverrideName[];

FProxyMutator_SubmixOverride::FProxyMutator_SubmixOverride()
{
	MutatorName = MutatorSubmixOverrideName;
}

void FProxyMutator_SubmixOverride::Apply(FAudioGameplayVolumeListener& Listener) const
{
	check(IsInAudioThread());

	FAudioDeviceHandle AudioDeviceHandle = FAudioDeviceManager::Get()->GetAudioDevice(Listener.GetOwningDeviceId());
	if (AudioDeviceHandle.IsValid() && AudioDeviceHandle->IsAudioMixerEnabled())
	{
		FSoundEffectSubmixInitData InitData;
		InitData.DeviceID = AudioDeviceHandle.GetDeviceID();
		InitData.SampleRate = AudioDeviceHandle->GetSampleRate();
		TArray<FSoundEffectSubmixPtr> SubmixEffectPresetChainOverride;

		for (const FAudioVolumeSubmixOverrideSettings& OverrideSettings : SubmixOverrideSettings)
		{
			if (OverrideSettings.Submix && OverrideSettings.SubmixEffectChain.Num() > 0)
			{
				// Build the instances of the new submix preset chain override
				for (USoundEffectSubmixPreset* SubmixEffectPreset : OverrideSettings.SubmixEffectChain)
				{
					if (SubmixEffectPreset)
					{
						InitData.PresetSettings = nullptr;
						InitData.ParentPresetUniqueId = SubmixEffectPreset->GetUniqueID();

						TSoundEffectSubmixPtr SoundEffectSubmix = USoundEffectPreset::CreateInstance<FSoundEffectSubmixInitData, FSoundEffectSubmix>(InitData, *SubmixEffectPreset);
						SoundEffectSubmix->SetEnabled(true);
						SubmixEffectPresetChainOverride.Add(SoundEffectSubmix);
					}
				}

				AudioDeviceHandle->SetSubmixEffectChainOverride(OverrideSettings.Submix, SubmixEffectPresetChainOverride, OverrideSettings.CrossfadeTime);
				SubmixEffectPresetChainOverride.Reset();
			}
		}
	}
}

void FProxyMutator_SubmixOverride::Remove(FAudioGameplayVolumeListener& Listener) const
{
	check(IsInAudioThread());

	FAudioDeviceHandle AudioDeviceHandle = FAudioDeviceManager::Get()->GetAudioDevice(Listener.GetOwningDeviceId());
	if (AudioDeviceHandle.IsValid() && AudioDeviceHandle->IsAudioMixerEnabled())
	{
		// Clear out any previous submix effect chain overrides
		for (const FAudioVolumeSubmixOverrideSettings& OverrideSettings : SubmixOverrideSettings)
		{
			AudioDeviceHandle->ClearSubmixEffectChainOverride(OverrideSettings.Submix, OverrideSettings.CrossfadeTime);
		}
	}
}

USubmixOverrideVolumeComponent::USubmixOverrideVolumeComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PayloadType = AudioGameplay::EComponentPayload::AGCP_Listener;
	bAutoActivate = true;
}

void USubmixOverrideVolumeComponent::SetSubmixOverrideSettings(const TArray<FAudioVolumeSubmixOverrideSettings>& NewSubmixOverrideSettings)
{
	SubmixOverrideSettings = NewSubmixOverrideSettings;

	// Let the parent volume know we've changed
	NotifyDataChanged();
}

TSharedPtr<FProxyVolumeMutator> USubmixOverrideVolumeComponent::FactoryMutator() const
{
	return MakeShared<FProxyMutator_SubmixOverride>();
}

void USubmixOverrideVolumeComponent::CopyAudioDataToMutator(TSharedPtr<FProxyVolumeMutator>& Mutator) const
{
	TSharedPtr<FProxyMutator_SubmixOverride> SubmixMutator = StaticCastSharedPtr<FProxyMutator_SubmixOverride>(Mutator);
	SubmixMutator->SubmixOverrideSettings = SubmixOverrideSettings;
}

