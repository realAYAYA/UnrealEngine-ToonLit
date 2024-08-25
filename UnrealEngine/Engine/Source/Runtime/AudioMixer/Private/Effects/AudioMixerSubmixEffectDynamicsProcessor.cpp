// Copyright Epic Games, Inc. All Rights Reserved.

#include "SubmixEffects/AudioMixerSubmixEffectDynamicsProcessor.h"

#include "AudioBusSubsystem.h"
#include "AudioDeviceManager.h"
#include "AudioMixerDevice.h"
#include "AudioMixerSubmix.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AudioMixerSubmixEffectDynamicsProcessor)

// Link to "Audio" profiling category
CSV_DECLARE_CATEGORY_MODULE_EXTERN(AUDIOMIXERCORE_API, Audio);

DEFINE_STAT(STAT_AudioMixerSubmixDynamics);

static int32 bBypassSubmixDynamicsProcessor = 0;
FAutoConsoleVariableRef CVarBypassDynamicsProcessor(
	TEXT("au.Submix.Effects.DynamicsProcessor.Bypass"),
	bBypassSubmixDynamicsProcessor,
	TEXT("If non-zero, bypasses all submix dynamics processors currently active.\n"),
	ECVF_Default);

FSubmixEffectDynamicsProcessor::FSubmixEffectDynamicsProcessor()
{
	DeviceCreatedHandle = FAudioDeviceManagerDelegates::OnAudioDeviceCreated.AddRaw(this, &FSubmixEffectDynamicsProcessor::OnDeviceCreated);
	DeviceDestroyedHandle = FAudioDeviceManagerDelegates::OnAudioDeviceDestroyed.AddRaw(this, &FSubmixEffectDynamicsProcessor::OnDeviceDestroyed);
}

FSubmixEffectDynamicsProcessor::~FSubmixEffectDynamicsProcessor()
{
	ResetKey();

	FAudioDeviceManagerDelegates::OnAudioDeviceCreated.Remove(DeviceCreatedHandle);
	FAudioDeviceManagerDelegates::OnAudioDeviceDestroyed.Remove(DeviceDestroyedHandle);
}

Audio::FDeviceId FSubmixEffectDynamicsProcessor::GetDeviceId() const
{
	return DeviceId;
}

void FSubmixEffectDynamicsProcessor::Init(const FSoundEffectSubmixInitData& InitData)
{
	static const int32 ProcessorScratchNumChannels = 8;

	DynamicsProcessor.Init(InitData.SampleRate, ProcessorScratchNumChannels);

	DeviceId = InitData.DeviceID;

	if (USubmixEffectDynamicsProcessorPreset* ProcPreset = Cast<USubmixEffectDynamicsProcessorPreset>(Preset.Get()))
	{
		switch (ProcPreset->Settings.KeySource)
		{
			case ESubmixEffectDynamicsKeySource::AudioBus:
			{
				if (UAudioBus* AudioBus = ProcPreset->Settings.ExternalAudioBus)
				{
					KeySource.Update(ESubmixEffectDynamicsKeySource::AudioBus, AudioBus->GetUniqueID(), static_cast<int32>(AudioBus->AudioBusChannels) + 1);
				}
			}
			break;

			case ESubmixEffectDynamicsKeySource::Submix:
			{
				if (USoundSubmix* Submix = ProcPreset->Settings.ExternalSubmix)
				{
					KeySource.Update(ESubmixEffectDynamicsKeySource::Submix, Submix->GetUniqueID());
				}
			}
			break;

			default:
			{
				// KeySource is this effect's submix/input, so do nothing
			}
			break;
		}
	}
}

void FSubmixEffectDynamicsProcessor::ResetKey()
{
	KeySource.Reset();
}

void FSubmixEffectDynamicsProcessor::OnPresetChanged()
{
	GET_EFFECT_SETTINGS(SubmixEffectDynamicsProcessor);

	bBypass = Settings.bBypass;

	switch (Settings.DynamicsProcessorType)
	{
	default:
	case ESubmixEffectDynamicsProcessorType::Compressor:
		DynamicsProcessor.SetProcessingMode(Audio::EDynamicsProcessingMode::Compressor);
		break;

	case ESubmixEffectDynamicsProcessorType::Limiter:
		DynamicsProcessor.SetProcessingMode(Audio::EDynamicsProcessingMode::Limiter);
		break;

	case ESubmixEffectDynamicsProcessorType::Expander:
		DynamicsProcessor.SetProcessingMode(Audio::EDynamicsProcessingMode::Expander);
		break;

	case ESubmixEffectDynamicsProcessorType::Gate:
		DynamicsProcessor.SetProcessingMode(Audio::EDynamicsProcessingMode::Gate);
		break;

	case ESubmixEffectDynamicsProcessorType::UpwardsCompressor:
		DynamicsProcessor.SetProcessingMode(Audio::EDynamicsProcessingMode::UpwardsCompressor);
		break;
	}

	switch (Settings.PeakMode)
	{
	default:
	case ESubmixEffectDynamicsPeakMode::MeanSquared:
		DynamicsProcessor.SetPeakMode(Audio::EPeakMode::MeanSquared);
		break;

	case ESubmixEffectDynamicsPeakMode::RootMeanSquared:
		DynamicsProcessor.SetPeakMode(Audio::EPeakMode::RootMeanSquared);
		break;

	case ESubmixEffectDynamicsPeakMode::Peak:
		DynamicsProcessor.SetPeakMode(Audio::EPeakMode::Peak);
		break;
	}

	DynamicsProcessor.SetLookaheadMsec(Settings.LookAheadMsec);
	DynamicsProcessor.SetAttackTime(Settings.AttackTimeMsec);
	DynamicsProcessor.SetReleaseTime(Settings.ReleaseTimeMsec);
	DynamicsProcessor.SetThreshold(Settings.ThresholdDb);
	DynamicsProcessor.SetRatio(Settings.Ratio);
	DynamicsProcessor.SetKneeBandwidth(Settings.KneeBandwidthDb);
	DynamicsProcessor.SetInputGain(Settings.InputGainDb);
	DynamicsProcessor.SetOutputGain(Settings.OutputGainDb);
	DynamicsProcessor.SetAnalogMode(Settings.bAnalogMode);

	DynamicsProcessor.SetKeyAudition(Settings.bKeyAudition);
	DynamicsProcessor.SetKeyGain(Settings.KeyGainDb);
	DynamicsProcessor.SetKeyHighshelfCutoffFrequency(Settings.KeyHighshelf.Cutoff);
	DynamicsProcessor.SetKeyHighshelfEnabled(Settings.KeyHighshelf.bEnabled);
	DynamicsProcessor.SetKeyHighshelfGain(Settings.KeyHighshelf.GainDb);
	DynamicsProcessor.SetKeyLowshelfCutoffFrequency(Settings.KeyLowshelf.Cutoff);
	DynamicsProcessor.SetKeyLowshelfEnabled(Settings.KeyLowshelf.bEnabled);
	DynamicsProcessor.SetKeyLowshelfGain(Settings.KeyLowshelf.GainDb);

	static_assert(static_cast<int32>(ESubmixEffectDynamicsChannelLinkMode::Count) == static_cast<int32>(Audio::EDynamicsProcessorChannelLinkMode::Count), "Enumerations must match");
	DynamicsProcessor.SetChannelLinkMode(static_cast<Audio::EDynamicsProcessorChannelLinkMode>(Settings.LinkMode));

	UpdateKeyFromSettings(Settings);
}

Audio::FMixerDevice* FSubmixEffectDynamicsProcessor::GetMixerDevice()
{
	if (FAudioDeviceManager* DeviceManager = FAudioDeviceManager::Get())
	{
		return static_cast<Audio::FMixerDevice*>(DeviceManager->GetAudioDeviceRaw(DeviceId));
	}

	return nullptr;
}

bool FSubmixEffectDynamicsProcessor::UpdateKeySourcePatch()
{
	// Default (input as key) does not use source patch, so don't
	// continue checking or updating state.
	if (KeySource.GetType() == ESubmixEffectDynamicsKeySource::Default)
	{
		return false;
	}

	if (KeySource.Patch.IsValid())
	{
		return true;
	}

	switch (KeySource.GetType())
	{
		case ESubmixEffectDynamicsKeySource::AudioBus:
		{
			// Retrieving/mutating the MixerDevice is only safe during OnProcessAudio calls if
			// it is not called during Teardown.  The DynamicsProcessor should be Reset via
			// the OnDeviceDestroyed callback (prior to FAudioDevice::Teardown), so this call
			// should never be hit during Teardown.
			if (Audio::FMixerDevice* MixerDevice = GetMixerDevice())
			{
				const uint32 ObjectId = KeySource.GetObjectId();
				if (ObjectId != INDEX_NONE)
				{
					UAudioBusSubsystem* AudioBusSubsystem = MixerDevice->GetSubsystem<UAudioBusSubsystem>();
					if (AudioBusSubsystem)
					{
						const int32 NumChannels = KeySource.GetNumChannels();
						AudioBusSubsystem->StartAudioBus(Audio::FAudioBusKey(ObjectId), NumChannels, /*bInIsAutomatic=*/false);
						KeySource.Patch = AudioBusSubsystem->AddPatchOutputForAudioBus(Audio::FAudioBusKey(ObjectId), MixerDevice->GetNumOutputFrames(), NumChannels);
						DynamicsProcessor.SetKeyNumChannels(NumChannels);
					}
				}
			}
		}
		break;

		case ESubmixEffectDynamicsKeySource::Submix:
		{
			// Retrieving/mutating the MixerDevice is only safe during OnProcessAudio calls if
			// it is not called during Teardown.  The DynamicsProcessor should be Reset via
			// the OnDeviceDestroyed callback (prior to FAudioDevice::Teardown), so this call
			// should never be hit during Teardown.
			if (Audio::FMixerDevice* MixerDevice = GetMixerDevice())
			{
				const uint32 ObjectId = KeySource.GetObjectId();
				if (ObjectId != INDEX_NONE)
				{
					KeySource.Patch = MixerDevice->AddPatchForSubmix(ObjectId, 1.0f /* PatchGain */);
					if (KeySource.Patch.IsValid())
					{
						Audio::FMixerSubmixPtr SubmixPtr = MixerDevice->FindSubmixInstanceByObjectId(KeySource.GetObjectId());
						if (SubmixPtr.IsValid())
						{
							const int32 SubmixNumChannels = SubmixPtr->GetNumOutputChannels();
							KeySource.SetNumChannels(SubmixNumChannels);
							DynamicsProcessor.SetKeyNumChannels(SubmixNumChannels);
							return true;
						}
					}
				}	
			}
		}
		break;

		case ESubmixEffectDynamicsKeySource::Default:
		default:
		{
		}
		break;
	}

	return false;
}

void FSubmixEffectDynamicsProcessor::OnProcessAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData)
{
	CSV_SCOPED_TIMING_STAT(Audio, SubmixDynamics);
	SCOPE_CYCLE_COUNTER(STAT_AudioMixerSubmixDynamics);
	TRACE_CPUPROFILER_EVENT_SCOPE(FSubmixEffectDynamicsProcessor::OnProcessAudio);

	ensure(InData.NumChannels == OutData.NumChannels);

	const Audio::FAlignedFloatBuffer& InBuffer = *InData.AudioBuffer;
	Audio::FAlignedFloatBuffer& OutBuffer = *OutData.AudioBuffer;

	if (bBypassSubmixDynamicsProcessor || bBypass)
	{
		FMemory::Memcpy(OutBuffer.GetData(), InBuffer.GetData(), sizeof(float) * InBuffer.Num());
		return;
	}

	int32 NumKeyChannels = DynamicsProcessor.GetKeyNumChannels();
	int32 NumKeySamples = InData.NumFrames * NumKeyChannels;

	AudioExternal.Reset();
	
	// If set to default, enforce num key channels to always be number of input channels.
	// If either unset or KeySource was changed between frames back to 'Default', NumKeyChannels
	// could be stale or left as initialized number of scratch channels.
	if (KeySource.GetType() == ESubmixEffectDynamicsKeySource::Default)
	{
		if (InData.NumChannels != NumKeyChannels)
		{
			NumKeyChannels = InData.NumChannels;
			NumKeySamples = InData.NumFrames * NumKeyChannels;
			DynamicsProcessor.SetKeyNumChannels(NumKeyChannels);
		}
	}
	else
	{
		AudioExternal.AddZeroed(NumKeySamples);
	}

	if (UpdateKeySourcePatch())
	{
		KeySource.Patch->PopAudio(AudioExternal.GetData(), NumKeySamples, true /* bUseLatestAudio */);
	}

	if (InData.NumChannels != DynamicsProcessor.GetNumChannels())
	{
		DynamicsProcessor.SetNumChannels(InData.NumChannels);
	}

	// No key assigned (Uses input buffer as key)
	if (KeySource.GetType() == ESubmixEffectDynamicsKeySource::Default)
	{
		DynamicsProcessor.ProcessAudio(InBuffer.GetData(), InData.NumChannels * InData.NumFrames, OutBuffer.GetData());
	}
	// Key assigned
	else
	{
		DynamicsProcessor.ProcessAudio(InBuffer.GetData(), InData.NumChannels * InData.NumFrames, OutBuffer.GetData(), AudioExternal.GetData());
	}
}

void FSubmixEffectDynamicsProcessor::UpdateKeyFromSettings(const FSubmixEffectDynamicsProcessorSettings& InSettings)
{
	uint32 ObjectId = INDEX_NONE;
	int32 SourceNumChannels = 0;
	switch (InSettings.KeySource)
	{
		case ESubmixEffectDynamicsKeySource::AudioBus:
		{
			if (InSettings.ExternalAudioBus)
			{
				ObjectId = InSettings.ExternalAudioBus->GetUniqueID();
				SourceNumChannels = static_cast<int32>(InSettings.ExternalAudioBus->AudioBusChannels) + 1;
			}
		}
		break;

		case ESubmixEffectDynamicsKeySource::Submix:
		{
			if (InSettings.ExternalSubmix)
			{
				ObjectId = InSettings.ExternalSubmix->GetUniqueID();
			}
		}
		break;

		default:
		{
		}
		break;
	}

	KeySource.Update(InSettings.KeySource, ObjectId, SourceNumChannels);
}

void FSubmixEffectDynamicsProcessor::OnDeviceCreated(Audio::FDeviceId InDeviceId)
{
	if (InDeviceId == DeviceId)
	{
		GET_EFFECT_SETTINGS(SubmixEffectDynamicsProcessor);
		UpdateKeyFromSettings(Settings);

		FAudioDeviceManagerDelegates::OnAudioDeviceCreated.Remove(DeviceCreatedHandle);
	}
}

void FSubmixEffectDynamicsProcessor::OnDeviceDestroyed(Audio::FDeviceId InDeviceId)
{
	if (InDeviceId == DeviceId)
	{
		// Reset the key on device destruction to avoid reinitializing
		// it during FAudioDevice::Teardown via ProcessAudio.
		ResetKey();
		FAudioDeviceManagerDelegates::OnAudioDeviceDestroyed.Remove(DeviceDestroyedHandle);
	}
}

void USubmixEffectDynamicsProcessorPreset::OnInit()
{
	switch (Settings.KeySource)
	{
		case ESubmixEffectDynamicsKeySource::AudioBus:
		{
			SetAudioBus(Settings.ExternalAudioBus);
		}
		break;

		case ESubmixEffectDynamicsKeySource::Submix:
		{
			SetExternalSubmix(Settings.ExternalSubmix);
		}
		break;

		default:
		{
		}
		break;
	}
}

#if WITH_EDITOR
void USubmixEffectDynamicsProcessorPreset::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& InChainEvent)
{
	if (InChainEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FSubmixEffectDynamicsProcessorSettings, KeySource))
	{
		switch (Settings.KeySource)
		{
		case ESubmixEffectDynamicsKeySource::AudioBus:
		{
			Settings.ExternalSubmix = nullptr;
		}
		break;

		case ESubmixEffectDynamicsKeySource::Submix:
		{
			Settings.ExternalAudioBus = nullptr;
		}
		break;

		case ESubmixEffectDynamicsKeySource::Default:
		default:
		{
			Settings.ExternalSubmix = nullptr;
			Settings.ExternalAudioBus = nullptr;
			static_assert(static_cast<int32>(ESubmixEffectDynamicsKeySource::Count) == 3, "Possible missing KeySource switch case coverage");
		}
		break;
		}
	}

	Super::PostEditChangeChainProperty(InChainEvent);
}
#endif // WITH_EDITOR

void USubmixEffectDynamicsProcessorPreset::Serialize(FStructuredArchive::FRecord Record)
{
	FArchive& UnderlyingArchive = Record.GetUnderlyingArchive();
	if (UnderlyingArchive.IsLoading())
	{
		if (Settings.bChannelLinked_DEPRECATED)
		{
			Settings.LinkMode = ESubmixEffectDynamicsChannelLinkMode::Average;
			Settings.bChannelLinked_DEPRECATED = 0;
		}
	}

	Super::Serialize(Record);
}

void USubmixEffectDynamicsProcessorPreset::ResetKey()
{
	EffectCommand<FSubmixEffectDynamicsProcessor>([](FSubmixEffectDynamicsProcessor& Instance)
	{
		Instance.ResetKey();
	});
}

void USubmixEffectDynamicsProcessorPreset::SetAudioBus(UAudioBus* InAudioBus)
{
	int32 BusChannels = 0;
	if (InAudioBus)
	{
		BusChannels = static_cast<int32>(InAudioBus->AudioBusChannels) + 1;
		SetKey(ESubmixEffectDynamicsKeySource::AudioBus, InAudioBus, BusChannels);
	}
	else
	{
		ResetKey();
	}
}

void USubmixEffectDynamicsProcessorPreset::SetExternalSubmix(USoundSubmix* InSubmix)
{
	if (InSubmix)
	{
		SetKey(ESubmixEffectDynamicsKeySource::Submix, InSubmix);
	}
	else
	{
		ResetKey();
	}
}

void USubmixEffectDynamicsProcessorPreset::SetKey(ESubmixEffectDynamicsKeySource InKeySource, UObject* InObject, int32 InNumChannels)
{
	if (InObject)
	{
		EffectCommand<FSubmixEffectDynamicsProcessor>([this, ObjectId = InObject->GetUniqueID(), InKeySource, InNumChannels](FSubmixEffectDynamicsProcessor& Instance)
		{
			Instance.KeySource.Update(InKeySource, ObjectId, InNumChannels);
		});
	}
}

void USubmixEffectDynamicsProcessorPreset::SetSettings(const FSubmixEffectDynamicsProcessorSettings& InSettings)
{
	UpdateSettings(InSettings);

	IterateEffects<FSubmixEffectDynamicsProcessor>([&](FSubmixEffectDynamicsProcessor& Instance)
	{
		Instance.UpdateKeyFromSettings(InSettings);
	});
}

