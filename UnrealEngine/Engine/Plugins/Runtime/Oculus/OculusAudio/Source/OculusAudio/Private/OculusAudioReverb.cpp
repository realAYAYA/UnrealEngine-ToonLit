// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusAudioReverb.h"
#include "OculusAudioMixer.h"
#include "OculusAudioSettings.h"
#include "OculusAudioContextManager.h"

#include "Sound/SoundSubmix.h"


namespace
{
	TSharedPtr<FSubmixEffectOculusReverbPlugin, ESPMode::ThreadSafe> CastEffectToPluginSharedPtr(FSoundEffectSubmixPtr InSubmixEffect)
	{
		return StaticCastSharedPtr<FSubmixEffectOculusReverbPlugin, FSoundEffectSubmix, ESPMode::ThreadSafe>(InSubmixEffect);
	}
} // namespace <>


void FSubmixEffectOculusReverbPlugin::OnNewDeviceCreated(Audio::FDeviceId DeviceId)
{
	FAudioDeviceManagerDelegates::OnAudioDeviceCreated.Remove(DeviceCreatedHandle);
	DeviceCreatedHandle.Reset();

	if (GEngine)
	{
		FAudioDevice* AudioDevice = GEngine->GetAudioDeviceManager()->GetAudioDeviceRaw(DeviceId);

		if (AudioDevice)
		{
			Context = FOculusAudioContextManager::GetContextForAudioDevice(AudioDevice);

			if (!Context)
			{
				Context = FOculusAudioContextManager::CreateContextForAudioDevice(AudioDevice);
			}
		}
	}
}

void FSubmixEffectOculusReverbPlugin::ClearContext()
{
	Context = nullptr;
}

void FSubmixEffectOculusReverbPlugin::Init(const FSoundEffectSubmixInitData& InInitData)
{
	DeviceCreatedHandle = FAudioDeviceManagerDelegates::OnAudioDeviceCreated.AddRaw(this, &FSubmixEffectOculusReverbPlugin::OnNewDeviceCreated);
}

FSubmixEffectOculusReverbPlugin::FSubmixEffectOculusReverbPlugin()
	: Context(nullptr)
{
}

FSubmixEffectOculusReverbPlugin::~FSubmixEffectOculusReverbPlugin()
{
	if (DeviceCreatedHandle.IsValid())
	{
		FAudioDeviceManagerDelegates::OnAudioDeviceCreated.Remove(DeviceCreatedHandle);
		DeviceCreatedHandle.Reset();
	}
}

void FSubmixEffectOculusReverbPlugin::OnProcessAudio(const FSoundEffectSubmixInputData& InputData, FSoundEffectSubmixOutputData& OutputData)
{
	if (Context)
	{
		int Enabled = 0;
		ovrResult Result = OVRA_CALL(ovrAudio_IsEnabled)(Context, ovrAudioEnable_LateReverberation, &Enabled);
		OVR_AUDIO_CHECK(Result, "Failed to check if reverb is Enabled");

		if (Enabled != 0)
		{
			uint32_t Status = 0;
			Result = OVRA_CALL(ovrAudio_MixInSharedReverbInterleaved)(Context, &Status, OutputData.AudioBuffer->GetData());
			OVR_AUDIO_CHECK(Result, "Failed to process reverb");
		}
	}
}

void OculusAudioReverb::ClearContext()
{
	Context = nullptr;
	if (SubmixEffect.IsValid())
	{
		CastEffectToPluginSharedPtr(SubmixEffect)->ClearContext();
	}
}

FSoundEffectSubmixPtr OculusAudioReverb::GetEffectSubmix()
{
	if (!SubmixEffect.IsValid())
	{
		if (!ReverbPreset)
		{
			ReverbPreset = NewObject<USubmixEffectOculusReverbPluginPreset>();
			ReverbPreset->AddToRoot();
		}

		SubmixEffect = USoundEffectPreset::CreateInstance<FSoundEffectSubmixInitData, FSoundEffectSubmix>(FSoundEffectSubmixInitData(), *ReverbPreset);
		SubmixEffect->SetEnabled(true);
	}

	return SubmixEffect;
}

USoundSubmix* OculusAudioReverb::GetSubmix()
{
	const UOculusAudioSettings* Settings = GetDefault<UOculusAudioSettings>();
	check(Settings);
	USoundSubmix* ReverbSubmix = Cast<USoundSubmix>(Settings->OutputSubmix.TryLoad());
	if (!ReverbSubmix)
	{
		static const FString DefaultSubmixName = TEXT("Oculus Reverb Submix");
		UE_LOG(LogAudio, Error, TEXT("Failed to load Oculus Reverb Submix from object path '%s' in OculusSettings. Creating '%s' as stub."),
			*Settings->OutputSubmix.GetAssetPathString(),
			*DefaultSubmixName);

		ReverbSubmix = NewObject<USoundSubmix>(USoundSubmix::StaticClass(), *DefaultSubmixName);
		ReverbSubmix->bMuteWhenBackgrounded = true;
	}
	ReverbSubmix->bAutoDisable = false;

	return ReverbSubmix;
}