// Copyright Epic Games, Inc. All Rights Reserved.
#include "AudioMixerSource.h"

#include "AudioDefines.h"
#include "AudioMixerSourceBuffer.h"
#include "ActiveSound.h"
#include "AudioMixerSourceBuffer.h"
#include "AudioMixerDevice.h"
#include "AudioMixerSourceVoice.h"
#include "AudioMixerTrace.h"
#include "ContentStreaming.h"
#include "IAudioExtensionPlugin.h"
#include "IAudioModulation.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundModulationDestination.h"
#include "Misc/ScopeRWLock.h"
#include "Templates/Function.h"
#include "Trace/Trace.h"
#include "Engine/Engine.h"


CSV_DECLARE_CATEGORY_MODULE_EXTERN(AUDIOMIXERCORE_API, Audio);

#if UE_AUDIO_PROFILERTRACE_ENABLED
UE_TRACE_EVENT_BEGIN(Audio, MixerSourceStart)
	UE_TRACE_EVENT_FIELD(uint32, DeviceId)
	UE_TRACE_EVENT_FIELD(uint64, Timestamp)
	UE_TRACE_EVENT_FIELD(uint32, PlayOrder)
	UE_TRACE_EVENT_FIELD(int32, SourceId)
	UE_TRACE_EVENT_FIELD(uint64, ComponentId)
	UE_TRACE_EVENT_FIELD(UE::Trace::WideString, Name)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(Audio, MixerSourceStop)
	UE_TRACE_EVENT_FIELD(uint32, DeviceId)
	UE_TRACE_EVENT_FIELD(uint64, Timestamp)
	UE_TRACE_EVENT_FIELD(uint32, PlayOrder)
UE_TRACE_EVENT_END()
#endif // UE_AUDIO_PROFILERTRACE_ENABLED


static int32 UseListenerOverrideForSpreadCVar = 0;
FAutoConsoleVariableRef CVarUseListenerOverrideForSpread(
	TEXT("au.UseListenerOverrideForSpread"),
	UseListenerOverrideForSpreadCVar,
	TEXT("Zero attenuation override distance stereo panning\n")
	TEXT("0: Use actual distance, 1: use listener override"),
	ECVF_Default);

static uint32 AudioMixerSourceFadeMinCVar = 512;
static FAutoConsoleCommand GSetAudioMixerSourceFadeMin(
	TEXT("au.SourceFadeMin"),
	TEXT("Sets the length (in samples) of minimum fade when a sound source is stopped. Must be divisible by 4 (vectorization requirement). Ignored for some procedural source types. (Default: 512, Min: 4). \n"),
	FConsoleCommandWithArgsDelegate::CreateStatic(
		[](const TArray<FString>& Args)
		{
			if (Args.Num() > 0)
			{
				const int32 SourceFadeMin = FMath::Max(FCString::Atoi(*Args[0]), 4);
				AudioMixerSourceFadeMinCVar = AlignArbitrary(SourceFadeMin, 4);
			}
		}
	)
);

namespace Audio
{
	namespace MixerSourcePrivate
	{
		EMixerSourceSubmixSendStage SubmixSendStageToMixerSourceSubmixSendStage(ESubmixSendStage InSendStage)
		{
			switch(InSendStage)
			{
				case ESubmixSendStage::PreDistanceAttenuation:
					return EMixerSourceSubmixSendStage::PreDistanceAttenuation;

				case ESubmixSendStage::PostDistanceAttenuation:
				default:
					return EMixerSourceSubmixSendStage::PostDistanceAttenuation;
			}
		}

		const USoundClass* GetFallbackSoundClass(const FActiveSound& InActiveSound, const FWaveInstance& InWaveInstance)
		{
			const USoundClass* SoundClass = InActiveSound.GetSoundClass();
			if (InWaveInstance.SoundClass)
			{
				SoundClass = InWaveInstance.SoundClass;
			}

			return SoundClass;
		}

		template<typename SendInfo>
		void ClearPreviousSubmixSends(const TArray<SendInfo>& InPreviousSendInfos, const TArray<SendInfo>& InNewSendInfos, FMixerDevice* InMixerDevice, FMixerSourceVoice* InMixerSourceVoice)
		{
			// Loop through every previous send setting
			for (const SendInfo& PreviousSendSetting : InPreviousSendInfos)
			{
				bool bFound = false;

				// See if it's in the current send list
				for (const SendInfo&  CurrentSendSettings : InNewSendInfos)
				{
					if (CurrentSendSettings.SoundSubmix == PreviousSendSetting.SoundSubmix)
					{
						bFound = true;
						break;
					}
				}

				// If it's not in the current send list, add to submixes to clear
				if (!bFound)
				{
					FMixerSubmixPtr SubmixPtr = InMixerDevice->GetSubmixInstance(PreviousSendSetting.SoundSubmix).Pin();
					InMixerSourceVoice->ClearSubmixSendInfo(SubmixPtr);
				}
			}
		}
		
	} // namespace MixerSourcePrivate

	namespace ModulationUtils
	{
		void MixInRoutedValue(const FModulationParameter& InParam, float& InOutValueA, float InValueB)
		{
			if (InParam.bRequiresConversion)
			{
				InParam.NormalizedFunction(InOutValueA);
				InParam.NormalizedFunction(InValueB);
			}
			InParam.MixFunction(InOutValueA, InValueB);
			if (InParam.bRequiresConversion)
			{
				InParam.UnitFunction(InOutValueA);
			}
		}

		FSoundModulationDestinationSettings InitRoutedDestinationSettings(
			const EModulationRouting& InActiveSoundRouting,
			const FSoundModulationDestinationSettings& InActiveSoundSettings,
			const EModulationRouting& InWaveRouting,
			const FSoundModulationDestinationSettings& InWaveSettings,
			const USoundClass* InSoundClass,
			const FModulationParameter& InParam,
			TFunctionRef<const FSoundModulationDestinationSettings* (const USoundClass&)> InGetSoundClassDestinationFunction)
		{
			auto UnionSoundClassSettings = [&](FSoundModulationDestinationSettings& InOutSettings)
			{
				if (InSoundClass)
				{
					const FSoundModulationDestinationSettings& ClassSettings = *InGetSoundClassDestinationFunction(*InSoundClass);
					MixInRoutedValue(InParam, InOutSettings.Value, ClassSettings.Value);
					InOutSettings.Modulators = InOutSettings.Modulators.Union(ClassSettings.Modulators);
				}
			};

			switch (InActiveSoundRouting)
			{
				case EModulationRouting::Union:
				{
					FSoundModulationDestinationSettings UnionSettings = InActiveSoundSettings;
					switch (InWaveRouting)
					{
						case EModulationRouting::Union:
						{
							MixInRoutedValue(InParam, UnionSettings.Value, InWaveSettings.Value);
							UnionSettings.Modulators = UnionSettings.Modulators.Union(InWaveSettings.Modulators);
							UnionSoundClassSettings(UnionSettings);
							return UnionSettings;
						}
						break;

						case EModulationRouting::Inherit:
						{
							UnionSoundClassSettings(UnionSettings);
						}
						break;

						case EModulationRouting::Override:
						{
							MixInRoutedValue(InParam, UnionSettings.Value, InWaveSettings.Value);
							UnionSettings.Modulators = UnionSettings.Modulators.Union(InWaveSettings.Modulators);
						}
						break;

						case EModulationRouting::Disable:
						default:
						break;
					}

					return UnionSettings;
				}
				break;

				case EModulationRouting::Inherit:
				{
					switch (InWaveRouting)
					{
						case EModulationRouting::Union:
						{
							FSoundModulationDestinationSettings UnionSettings = InWaveSettings;
							UnionSoundClassSettings(UnionSettings);
							return UnionSettings;
						}
						break;

						case EModulationRouting::Inherit:
						{
							if (InSoundClass)
							{
								return *InGetSoundClassDestinationFunction(*InSoundClass);
							}
						}
						break;

						case EModulationRouting::Override:
						{
							return InWaveSettings;
						}
						break;

						case EModulationRouting::Disable:
						default:
						break;
					}
				}
				break;

				case EModulationRouting::Override:
				{
					return InActiveSoundSettings;
				}
				break;

				case EModulationRouting::Disable:
				default:
				break;
			}

			return { };
		}

		float GetRoutedDestinationValue(
			const EModulationRouting& InActiveSoundRouting,
			const FSoundModulationDestinationSettings& InActiveSoundSettings,
			const EModulationRouting& InWaveRouting,
			const FSoundModulationDestinationSettings& InWaveSettings,
			const USoundClass* InSoundClass,
			const FModulationParameter& InParam,
			TFunctionRef<const FSoundModulationDestinationSettings* (const USoundClass&)> InGetSoundClassDestinationFunction)
		{
			auto MixInSoundClassValue = [&](float& InOutValue)
			{
				if (InSoundClass)
				{
					const FSoundModulationDestinationSettings& ClassSettings = *InGetSoundClassDestinationFunction(*InSoundClass);
					MixInRoutedValue(InParam, InOutValue, ClassSettings.Value);
				}
			};

			switch (InActiveSoundRouting)
			{
				case EModulationRouting::Union:
				{
					float UnionValue = InActiveSoundSettings.Value;
					switch (InWaveRouting)
					{
						case EModulationRouting::Union:
						{
							MixInRoutedValue(InParam, UnionValue, InWaveSettings.Value);
							MixInSoundClassValue(UnionValue);
							return UnionValue;
						}
						break;

						case EModulationRouting::Inherit:
						{
							MixInSoundClassValue(UnionValue);
						}
						break;

						case EModulationRouting::Override:
						{
							MixInRoutedValue(InParam, UnionValue, InWaveSettings.Value);
						}
						break;

						case EModulationRouting::Disable:
						default:
						break;
					}

					return UnionValue;
				}
				break;

				case EModulationRouting::Inherit:
				{
					switch (InWaveRouting)
					{
						case EModulationRouting::Union:
						{
							float UnionValue = InWaveSettings.Value;
							MixInSoundClassValue(UnionValue);
							return UnionValue;
						}
						break;

						case EModulationRouting::Inherit:
						{
							if (InSoundClass)
							{
								return InGetSoundClassDestinationFunction(*InSoundClass)->Value;
							}
						}
						break;

						case EModulationRouting::Override:
						{
							return InWaveSettings.Value;
						}
						break;

						case EModulationRouting::Disable:
						default:
						break;
					}
				}
				break;

				case EModulationRouting::Override:
				{
					return InActiveSoundSettings.Value;
				}
				break;

				case EModulationRouting::Disable:
				default:
				break;
			}

			return 1.0f;
		}

		FSoundModulationDestinationSettings InitRoutedVolumeModulation(const FWaveInstance& InWaveInstance, const USoundWave& InWaveData, const FActiveSound& InActiveSound)
		{
			const EModulationRouting& ActiveSoundRouting = InActiveSound.ModulationRouting.VolumeRouting;
			const FSoundModulationDestinationSettings& ActiveSoundSettings = InActiveSound.ModulationRouting.VolumeModulationDestination;

			const EModulationRouting& WaveRouting = InWaveData.ModulationSettings.VolumeRouting;
			const FSoundModulationDestinationSettings& WaveSettings = InWaveData.ModulationSettings.VolumeModulationDestination;

			return InitRoutedDestinationSettings(
				ActiveSoundRouting,
				ActiveSoundSettings,
				WaveRouting,
				WaveSettings,
				MixerSourcePrivate::GetFallbackSoundClass(InActiveSound, InWaveInstance),
				Audio::GetModulationParameter("Volume"),
				[](const USoundClass& InSoundClass) { return &InSoundClass.Properties.ModulationSettings.VolumeModulationDestination; }
			);
		}

		float GetRoutedVolume(const FWaveInstance& InWaveInstance, const USoundWave& InWaveData, const FActiveSound& InActiveSound)
		{
			const EModulationRouting& ActiveSoundRouting = InActiveSound.ModulationRouting.VolumeRouting;
			const FSoundModulationDestinationSettings& ActiveSoundSettings = InActiveSound.ModulationRouting.VolumeModulationDestination;

			const EModulationRouting& WaveRouting = InWaveData.ModulationSettings.VolumeRouting;
			const FSoundModulationDestinationSettings& WaveSettings = InWaveData.ModulationSettings.VolumeModulationDestination;

			return GetRoutedDestinationValue(
				ActiveSoundRouting,
				ActiveSoundSettings,
				WaveRouting,
				WaveSettings,
				MixerSourcePrivate::GetFallbackSoundClass(InActiveSound, InWaveInstance),
				Audio::GetModulationParameter("Volume"),
				[](const USoundClass& InSoundClass) { return &InSoundClass.Properties.ModulationSettings.VolumeModulationDestination; }
			);
		}

		FSoundModulationDestinationSettings InitRoutedPitchModulation(const FWaveInstance& InWaveInstance, const USoundWave& InWaveData, const FActiveSound& InActiveSound)
		{
			const EModulationRouting& ActiveSoundRouting = InActiveSound.ModulationRouting.PitchRouting;
			const FSoundModulationDestinationSettings& ActiveSoundSettings = InActiveSound.ModulationRouting.PitchModulationDestination;

			const EModulationRouting& WaveRouting = InWaveData.ModulationSettings.PitchRouting;
			const FSoundModulationDestinationSettings& WaveSettings = InWaveData.ModulationSettings.PitchModulationDestination;

			return InitRoutedDestinationSettings(
				ActiveSoundRouting,
				ActiveSoundSettings,
				WaveRouting,
				WaveSettings,
				MixerSourcePrivate::GetFallbackSoundClass(InActiveSound, InWaveInstance),
				Audio::GetModulationParameter("Pitch"),
				[](const USoundClass& InSoundClass) { return &InSoundClass.Properties.ModulationSettings.PitchModulationDestination; }
			);
		}

		float GetRoutedPitch(const FWaveInstance& InWaveInstance, const USoundWave& InWaveData, const FActiveSound& InActiveSound)
		{
			const EModulationRouting& ActiveSoundRouting = InActiveSound.ModulationRouting.PitchRouting;
			const FSoundModulationDestinationSettings& ActiveSoundSettings = InActiveSound.ModulationRouting.PitchModulationDestination;

			const EModulationRouting& WaveRouting = InWaveData.ModulationSettings.PitchRouting;
			const FSoundModulationDestinationSettings& WaveSettings = InWaveData.ModulationSettings.PitchModulationDestination;

			return GetRoutedDestinationValue(
				ActiveSoundRouting,
				ActiveSoundSettings,
				WaveRouting,
				WaveSettings,
				MixerSourcePrivate::GetFallbackSoundClass(InActiveSound, InWaveInstance),
				Audio::GetModulationParameter("Pitch"),
				[](const USoundClass& InSoundClass) { return &InSoundClass.Properties.ModulationSettings.PitchModulationDestination; }
			);
		}

		FSoundModulationDestinationSettings InitRoutedHighpassModulation(const FWaveInstance& InWaveInstance, const USoundWave& InWaveData, const FActiveSound& InActiveSound)
		{
			const EModulationRouting& ActiveSoundRouting = InActiveSound.ModulationRouting.HighpassRouting;
			const FSoundModulationDestinationSettings& ActiveSoundSettings = InActiveSound.ModulationRouting.HighpassModulationDestination;

			const EModulationRouting& WaveRouting = InWaveData.ModulationSettings.HighpassRouting;
			const FSoundModulationDestinationSettings& WaveSettings = InWaveData.ModulationSettings.HighpassModulationDestination;

			return InitRoutedDestinationSettings(
				ActiveSoundRouting,
				ActiveSoundSettings,
				WaveRouting,
				WaveSettings,
				MixerSourcePrivate::GetFallbackSoundClass(InActiveSound, InWaveInstance),
				Audio::GetModulationParameter("HPFCutoffFrequency"),
				[](const USoundClass& InSoundClass) { return &InSoundClass.Properties.ModulationSettings.HighpassModulationDestination; }
			);
		}

		float GetRoutedHighpass(const FWaveInstance& InWaveInstance, const USoundWave& InWaveData, const FActiveSound& InActiveSound)
		{
			const EModulationRouting& ActiveSoundRouting = InActiveSound.ModulationRouting.HighpassRouting;
			const FSoundModulationDestinationSettings& ActiveSoundSettings = InActiveSound.ModulationRouting.HighpassModulationDestination;

			const EModulationRouting& WaveRouting = InWaveData.ModulationSettings.HighpassRouting;
			const FSoundModulationDestinationSettings& WaveSettings = InWaveData.ModulationSettings.HighpassModulationDestination;

			return GetRoutedDestinationValue(
				ActiveSoundRouting,
				ActiveSoundSettings,
				WaveRouting,
				WaveSettings,
				MixerSourcePrivate::GetFallbackSoundClass(InActiveSound, InWaveInstance),
				Audio::GetModulationParameter("HPFCutoffFrequency"),
				[](const USoundClass& InSoundClass) { return &InSoundClass.Properties.ModulationSettings.HighpassModulationDestination; }
			);
		}

		FSoundModulationDestinationSettings InitRoutedLowpassModulation(const FWaveInstance& InWaveInstance, const USoundWave& InWaveData, const FActiveSound& InActiveSound)
		{
			const EModulationRouting& ActiveSoundRouting = InActiveSound.ModulationRouting.LowpassRouting;
			const FSoundModulationDestinationSettings& ActiveSoundSettings = InActiveSound.ModulationRouting.LowpassModulationDestination;

			const EModulationRouting& WaveRouting = InWaveData.ModulationSettings.LowpassRouting;
			const FSoundModulationDestinationSettings& WaveSettings = InWaveData.ModulationSettings.LowpassModulationDestination;

			return InitRoutedDestinationSettings(
				ActiveSoundRouting,
				ActiveSoundSettings,
				WaveRouting,
				WaveSettings,
				MixerSourcePrivate::GetFallbackSoundClass(InActiveSound, InWaveInstance),
				Audio::GetModulationParameter("LPFCutoffFrequency"),
				[](const USoundClass& InSoundClass) { return &InSoundClass.Properties.ModulationSettings.LowpassModulationDestination; }
			);
		}

		float GetRoutedLowpass(const FWaveInstance& InWaveInstance, const USoundWave& InWaveData, const FActiveSound& InActiveSound)
		{
			const EModulationRouting& ActiveSoundRouting = InActiveSound.ModulationRouting.LowpassRouting;
			const FSoundModulationDestinationSettings& ActiveSoundSettings = InActiveSound.ModulationRouting.LowpassModulationDestination;

			const EModulationRouting& WaveRouting = InWaveData.ModulationSettings.LowpassRouting;
			const FSoundModulationDestinationSettings& WaveSettings = InWaveData.ModulationSettings.LowpassModulationDestination;

			return GetRoutedDestinationValue(
				ActiveSoundRouting,
				ActiveSoundSettings,
				WaveRouting,
				WaveSettings,
				MixerSourcePrivate::GetFallbackSoundClass(InActiveSound, InWaveInstance),
				Audio::GetModulationParameter("LPFCutoffFrequency"),
				[](const USoundClass& InSoundClass) { return &InSoundClass.Properties.ModulationSettings.LowpassModulationDestination; }
			);
		}

		FSoundModulationDefaultSettings InitRoutedModulation(const FWaveInstance& InWaveInstance, const USoundWave& InWaveData, FActiveSound* InActiveSound)
		{
			FSoundModulationDefaultSettings Settings;
			if (InActiveSound)
			{
				Settings.VolumeModulationDestination = InitRoutedVolumeModulation(InWaveInstance, InWaveData, *InActiveSound);
				Settings.PitchModulationDestination = InitRoutedPitchModulation(InWaveInstance, InWaveData, *InActiveSound);
				Settings.HighpassModulationDestination = InitRoutedHighpassModulation(InWaveInstance, InWaveData, *InActiveSound);
				Settings.LowpassModulationDestination = InitRoutedLowpassModulation(InWaveInstance, InWaveData, *InActiveSound);
			}

			return Settings;
		}
	} // namespace ModulationUtils

	FMixerSource::FMixerSource(FAudioDevice* InAudioDevice)
		: FSoundSource(InAudioDevice)
		, MixerDevice(static_cast<FMixerDevice*>(InAudioDevice))
		, MixerBuffer(nullptr)
		, MixerSourceVoice(nullptr)
		, bBypassingSubmixModulation(false)
		, bPreviousBusEnablement(false)
		, bPreviousBaseSubmixEnablement(false)
		, PreviousAzimuth(-1.0f)
		, PreviousPlaybackPercent(0.0f)
		, InitializationState(EMixerSourceInitializationState::NotInitialized)
		, bPlayedCachedBuffer(false)
		, bPlaying(false)
		, bLoopCallback(false)
		, bIsDone(false)
		, bIsEffectTailsDone(false)
		, bIsPlayingEffectTails(false)
		, bEditorWarnedChangedSpatialization(false)
		, bIs3D(false)
		, bDebugMode(false)
		, bIsVorbis(false)
		, bIsStoppingVoicesEnabled(InAudioDevice->IsStoppingVoicesEnabled())
		, bSendingAudioToBuses(false)
		, bPrevAllowedSpatializationSetting(false)		
	{
	}

	FMixerSource::~FMixerSource()
	{
		FreeResources();
	}

	bool FMixerSource::Init(FWaveInstance* InWaveInstance)
	{
		AUDIO_MIXER_TRACE_CPUPROFILER_EVENT_SCOPE(AudioMixerSource::Init);
		AUDIO_MIXER_CHECK(MixerBuffer);
		AUDIO_MIXER_CHECK(MixerBuffer->IsRealTimeSourceReady());

		// We've already been passed the wave instance in PrepareForInitialization, make sure we have the same one
		AUDIO_MIXER_CHECK(WaveInstance && WaveInstance == InWaveInstance);

		LLM_SCOPE(ELLMTag::AudioMixer);

		FSoundSource::InitCommon();

		if (!ensure(InWaveInstance))
		{
			return false;
		}

		USoundWave* WaveData = WaveInstance->WaveData;
		check(WaveData);

		if (WaveData->NumChannels == 0)
		{
			UE_LOG(LogAudioMixer, Warning, TEXT("Soundwave %s has invalid compressed data."), *(WaveData->GetName()));
			FreeResources();
			return false;
		}

		// Get the number of frames before creating the buffer
		int32 NumFrames = INDEX_NONE;
		if (WaveData->DecompressionType != DTYPE_Procedural)
		{
			check(!WaveData->RawPCMData || WaveData->RawPCMDataSize);
			const int32 NumBytes = WaveData->RawPCMDataSize;
			if (WaveInstance->WaveData->NumChannels > 0)
			{
				NumFrames = NumBytes / (WaveData->NumChannels * sizeof(int16));
			}
		}

		// Unfortunately, we need to know if this is a vorbis source since channel maps are different for 5.1 vorbis files
		bIsVorbis = WaveData->bDecompressedFromOgg;

		bIsStoppingVoicesEnabled = AudioDevice->IsStoppingVoicesEnabled();

		bIsStopping = false;
		bIsEffectTailsDone = true;
		bIsDone = false;

		bBypassingSubmixModulation = false;

		FSoundBuffer* SoundBuffer = static_cast<FSoundBuffer*>(MixerBuffer);
		if (SoundBuffer->NumChannels > 0)
		{
			CSV_SCOPED_TIMING_STAT(Audio, InitSources);
			SCOPE_CYCLE_COUNTER(STAT_AudioSourceInitTime);

			AUDIO_MIXER_CHECK(MixerDevice);
			MixerSourceVoice = MixerDevice->GetMixerSourceVoice();
			if (!MixerSourceVoice)
			{
				FreeResources();
				UE_LOG(LogAudioMixer, Warning, TEXT("Failed to get a mixer source voice for sound %s."), *InWaveInstance->GetName());
				return false;
			}

			// Initialize the source voice with the necessary format information
			FMixerSourceVoiceInitParams InitParams;
			InitParams.SourceListener = this;
			InitParams.NumInputChannels = WaveData->NumChannels;
			InitParams.NumInputFrames = NumFrames;
			InitParams.SourceVoice = MixerSourceVoice;
			InitParams.bUseHRTFSpatialization = UseObjectBasedSpatialization();

			// in this file once spat override is implemented
			InitParams.bIsExternalSend = MixerDevice->GetCurrentSpatializationPluginInterfaceInfo().bSpatializationIsExternalSend;
			InitParams.bIsSoundfield = WaveInstance->bIsAmbisonics && (WaveData->NumChannels == 4);

			FActiveSound* ActiveSound = WaveInstance->ActiveSound;
			InitParams.ModulationSettings = ModulationUtils::InitRoutedModulation(*WaveInstance, *WaveData, ActiveSound);

			// Copy quantization request data
			if (WaveInstance->QuantizedRequestData)
			{
				InitParams.QuantizedRequestData = *WaveInstance->QuantizedRequestData;
			}

			if (WaveInstance->bIsAmbisonics && (WaveData->NumChannels != 4))
			{
				UE_LOG(LogAudioMixer, Warning, TEXT("Sound wave %s was flagged as being ambisonics but had a channel count of %d. Currently the audio engine only supports FOA sources that have four channels."), *InWaveInstance->GetName(), WaveData->NumChannels);
			}
			if (ActiveSound)
			{
				InitParams.AudioComponentUserID = WaveInstance->ActiveSound->GetAudioComponentUserID();
				InitParams.AudioComponentID = WaveInstance->ActiveSound->GetAudioComponentID();
			}

			InitParams.EnvelopeFollowerAttackTime = WaveInstance->EnvelopeFollowerAttackTime;
			InitParams.EnvelopeFollowerReleaseTime = WaveInstance->EnvelopeFollowerReleaseTime;

			InitParams.SourceEffectChainId = 0;

			InitParams.SourceBufferListener = WaveInstance->SourceBufferListener;
			InitParams.bShouldSourceBufferListenerZeroBuffer = WaveInstance->bShouldSourceBufferListenerZeroBuffer;

			if (WaveInstance->bShouldUseAudioLink)
			{
				if (IAudioLinkFactory* LinkFactory = MixerDevice->GetAudioLinkFactory())
				{				
					IAudioLinkFactory::FAudioLinkSourcePushedCreateArgs CreateArgs;					
					if (WaveInstance->AudioLinkSettingsOverride)
					{
						CreateArgs.Settings = WaveInstance->AudioLinkSettingsOverride->GetProxy();
					}
					else
					{
						CreateArgs.Settings = GetDefault<UAudioLinkSettingsAbstract>(LinkFactory->GetSettingsClass())->GetProxy();
					}
					
					CreateArgs.OwnerName = *WaveInstance->GetName();			// <-- FIXME: String FName conversion.
					CreateArgs.NumChannels = SoundBuffer->NumChannels;
					CreateArgs.NumFramesPerBuffer = MixerDevice->GetBufferLength();
					CreateArgs.SampleRate = MixerDevice->GetSampleRate();
					CreateArgs.TotalNumFramesInSource = NumTotalFrames;
					AudioLink = LinkFactory->CreateSourcePushedAudioLink(CreateArgs);
					InitParams.AudioLink = AudioLink;
				}
			}

			// Source manager needs to know if this is a vorbis source for rebuilding speaker maps
			InitParams.bIsVorbis = bIsVorbis;

			// Support stereo by default
			// Check the min number of channels the source effect chain supports
			// We don't want to instantiate the effect chain if it has an effect that doesn't support its channel count
			// E.g. we shouldn't instantiate a chain on a quad source if there is an effect that only supports stereo
			InitParams.SourceEffectChainMaxSupportedChannels = WaveInstance->SourceEffectChain ? 
				WaveInstance->SourceEffectChain->GetSupportedChannelCount() :
				USoundEffectSourcePreset::DefaultSupportedChannels;

			if (InitParams.NumInputChannels <= InitParams.SourceEffectChainMaxSupportedChannels)
			{
				if (WaveInstance->SourceEffectChain)
				{
					InitParams.SourceEffectChainId = WaveInstance->SourceEffectChain->GetUniqueID();

					for (int32 i = 0; i < WaveInstance->SourceEffectChain->Chain.Num(); ++i)
					{
						InitParams.SourceEffectChain.Add(WaveInstance->SourceEffectChain->Chain[i]);
						InitParams.bPlayEffectChainTails = WaveInstance->SourceEffectChain->bPlayEffectChainTails;
					}
				}

				// Only need to care about effect chain tails finishing if we're told to play them
				if (InitParams.bPlayEffectChainTails)
				{
					bIsEffectTailsDone = false;
				}

				// Setup the bus Id if this source is a bus
				if (WaveData->bIsSourceBus)
				{
					// We need to check if the source bus has an audio bus specified
					USoundSourceBus* SoundSourceBus = CastChecked<USoundSourceBus>(WaveData);

					// If it does, we will use that audio bus as the source of the audio data for the source bus
					if (SoundSourceBus->AudioBus)
					{
						InitParams.AudioBusId = SoundSourceBus->AudioBus->GetUniqueID();
						InitParams.AudioBusChannels = (int32)SoundSourceBus->AudioBus->GetNumChannels();
					}
					else
					{
						InitParams.AudioBusId = WaveData->GetUniqueID();
						InitParams.AudioBusChannels = WaveData->NumChannels;
					}

					if (!WaveData->IsLooping())
					{
						InitParams.SourceBusDuration = WaveData->GetDuration();
					}
				}
			}

			// Toggle muting the source if sending only to output bus.
			// This can get set even if the source doesn't have bus sends since bus sends can be dynamically enabled.
			InitParams.bEnableBusSends = WaveInstance->bEnableBusSends;
			InitParams.bEnableBaseSubmix = WaveInstance->bEnableBaseSubmix;
			InitParams.bEnableSubmixSends = WaveInstance->bEnableSubmixSends;
			InitParams.PlayOrder = WaveInstance->GetPlayOrder();
			bPreviousBusEnablement = WaveInstance->bEnableBusSends;
			DynamicBusSendInfos.Reset();

			SetupBusData(InitParams.AudioBusSends, InitParams.bEnableBusSends);

			// Don't set up any submixing if we're set to output to bus only
	
			// If we're spatializing using HRTF and its an external send, don't need to setup a default/base submix send to master or EQ submix
			// We'll only be using non-default submix sends (e.g. reverb).
			if (!(InitParams.bUseHRTFSpatialization && InitParams.bIsExternalSend))
			{
				FMixerSubmixWeakPtr SubmixPtr;
				// If a sound specifies a base submix manually, always use that
				if (WaveInstance->SoundSubmix)
				{
					SubmixPtr = MixerDevice->GetSubmixInstance(WaveInstance->SoundSubmix);
				}
				else
				{
					// Retrieve the base default submix if one is not explicitly set
					SubmixPtr = MixerDevice->GetBaseDefaultSubmix();
				}

				FMixerSourceSubmixSend SubmixSend;
				SubmixSend.Submix = SubmixPtr;
				SubmixSend.SubmixSendStage = EMixerSourceSubmixSendStage::PostDistanceAttenuation;
				SubmixSend.SendLevel = InitParams.bEnableBaseSubmix;
				SubmixSend.bIsMainSend = true;
				SubmixSend.SoundfieldFactory = MixerDevice->GetFactoryForSubmixInstance(SubmixSend.Submix);
				InitParams.SubmixSends.Add(SubmixSend);
				bPreviousBaseSubmixEnablement = InitParams.bEnableBaseSubmix;
			}
			else
			{
				// Warn about sending a source marked as Binaural directly to a soundfield submix:
				// This is a bit of a gray area as soundfield submixes are intended to be their own spatial format
				// So to send a source to this, and also flagging the source as Binaural are probably conflicting forms of spatialazition.
				FMixerSubmixWeakPtr SubmixWeakPtr = MixerDevice->GetSubmixInstance(WaveInstance->SoundSubmix);

				if (FMixerSubmixPtr SubmixPtr = SubmixWeakPtr.Pin())
				{
					if ((SubmixPtr->IsSoundfieldSubmix() || SubmixPtr->IsSoundfieldEndpointSubmix()))
					{
						UE_LOG(LogAudioMixer, Warning, TEXT("Ignoring soundfield Base Submix destination being set on SoundWave (%s) because spatialization method is set to Binaural.")
							, *InWaveInstance->GetName());
					}
					
					bBypassingSubmixModulation = true;
				}
			}

			// Add submix sends for this source
			for (FSoundSubmixSendInfo& SendInfo : WaveInstance->SoundSubmixSends)
			{
				if (SendInfo.SoundSubmix != nullptr)
				{
					FMixerSourceSubmixSend SubmixSend;
					SubmixSend.Submix = MixerDevice->GetSubmixInstance(SendInfo.SoundSubmix);

					SubmixSend.SubmixSendStage = EMixerSourceSubmixSendStage::PostDistanceAttenuation;
					if (SendInfo.SendStage == ESubmixSendStage::PreDistanceAttenuation)
					{
						SubmixSend.SubmixSendStage = EMixerSourceSubmixSendStage::PreDistanceAttenuation;
					}
					if (!WaveInstance->bEnableSubmixSends)
					{
						SubmixSend.SendLevel = 0.0f;
					}
					else
					{
						SubmixSend.SendLevel = SendInfo.SendLevel;
					}
					
					SubmixSend.bIsMainSend = false;
					SubmixSend.SoundfieldFactory = MixerDevice->GetFactoryForSubmixInstance(SubmixSend.Submix);
					InitParams.SubmixSends.Add(SubmixSend);
				}
			}

			// Loop through all submix sends to figure out what speaker maps this source is using
			for (FMixerSourceSubmixSend& Send : InitParams.SubmixSends)
			{
				FMixerSubmixPtr SubmixPtr = Send.Submix.Pin();
				if (SubmixPtr.IsValid())
				{
					FRWScopeLock Lock(ChannelMapLock, SLT_Write);
					ChannelMap.Reset();
				}
			}

			// Check to see if this sound has been flagged to be in debug mode
#if AUDIO_MIXER_ENABLE_DEBUG_MODE
			InitParams.DebugName = WaveInstance->GetName();

			bool bIsDebug = false;
			FString WaveInstanceName = WaveInstance->GetName(); //-V595
			FString TestName = GEngine->GetAudioDeviceManager()->GetDebugger().GetAudioMixerDebugSoundName();
			if (!TestName.IsEmpty() && WaveInstanceName.Contains(TestName))
			{
				bDebugMode = true;
				InitParams.bIsDebugMode = bDebugMode;
			}
#endif

			// Whether or not we're 3D
			bIs3D = !UseObjectBasedSpatialization() && WaveInstance->GetUseSpatialization() && SoundBuffer->NumChannels < 3;

			// Pass on the fact that we're 3D to the init params
			InitParams.bIs3D = bIs3D;

			// Grab the source's reverb plugin settings
			InitParams.SpatializationPluginSettings = UseSpatializationPlugin() ? WaveInstance->SpatializationPluginSettings : nullptr;

			// Grab the source's occlusion plugin settings
			InitParams.OcclusionPluginSettings = UseOcclusionPlugin() ? WaveInstance->OcclusionPluginSettings : nullptr;

			// Grab the source's reverb plugin settings
			InitParams.ReverbPluginSettings = UseReverbPlugin() ? WaveInstance->ReverbPluginSettings : nullptr;

			// Grab the source's source data override plugin settings
			InitParams.SourceDataOverridePluginSettings = UseSourceDataOverridePlugin() ? WaveInstance->SourceDataOverridePluginSettings : nullptr;

			// Update the buffer sample rate to the wave instance sample rate in case it was serialized incorrectly
			MixerBuffer->InitSampleRate(WaveData->GetSampleRateForCurrentPlatform());

			// Retrieve the raw pcm buffer data and the precached buffers before initializing so we can avoid having USoundWave ptrs in audio renderer thread
			EBufferType::Type BufferType = MixerBuffer->GetType();
			if (BufferType == EBufferType::PCM || BufferType == EBufferType::PCMPreview)
			{
				FRawPCMDataBuffer RawPCMDataBuffer;
				MixerBuffer->GetPCMData(&RawPCMDataBuffer.Data, &RawPCMDataBuffer.DataSize);
				MixerSourceBuffer->SetPCMData(RawPCMDataBuffer);
			}
#if PLATFORM_NUM_AUDIODECOMPRESSION_PRECACHE_BUFFERS > 0
			else if (BufferType == EBufferType::PCMRealTime || BufferType == EBufferType::Streaming)
			{
				if (WaveData->CachedRealtimeFirstBuffer)
				{
					const uint32 NumPrecacheSamples = (uint32)(WaveData->NumPrecacheFrames * WaveData->NumChannels);
					const uint32 BufferSize = NumPrecacheSamples * sizeof(int16) * PLATFORM_NUM_AUDIODECOMPRESSION_PRECACHE_BUFFERS;

					TArray<uint8> PrecacheBufferCopy;
					PrecacheBufferCopy.AddUninitialized(BufferSize);

					FMemory::Memcpy(PrecacheBufferCopy.GetData(), WaveData->CachedRealtimeFirstBuffer, BufferSize);

					MixerSourceBuffer->SetCachedRealtimeFirstBuffers(MoveTemp(PrecacheBufferCopy));
				}
			}
#endif

			// Pass the decompression state off to the mixer source buffer if it hasn't already done so
			ICompressedAudioInfo* Decoder = MixerBuffer->GetDecompressionState(true);
			MixerSourceBuffer->SetDecoder(Decoder);

			// Hand off the mixer source buffer decoder
			InitParams.MixerSourceBuffer = MixerSourceBuffer;
			MixerSourceBuffer = nullptr;

			if (MixerSourceVoice->Init(InitParams))
			{
				// Initialize the propagation interface as soon as we have a valid source id
				if (AudioDevice->SourceDataOverridePluginInterface)
				{
					uint32 SourceId = MixerSourceVoice->GetSourceId();
					AudioDevice->SourceDataOverridePluginInterface->OnInitSource(SourceId, InitParams.AudioComponentUserID, InitParams.SourceDataOverridePluginSettings);
				}

				InitializationState = EMixerSourceInitializationState::Initialized;

				Update();

				return true;
			}
			else
			{
				InitializationState = EMixerSourceInitializationState::NotInitialized;
				UE_LOG(LogAudioMixer, Warning, TEXT("Failed to initialize mixer source voice '%s'."), *InWaveInstance->GetName());
			}
		}
		else
		{
			UE_LOG(LogAudioMixer, Warning, TEXT("Num channels was 0 for sound buffer '%s'."), *InWaveInstance->GetName());
		}

		FreeResources();
		return false;
	}

	void FMixerSource::SetupBusData(TArray<FInitAudioBusSend>* OutAudioBusSends, bool bEnableBusSends)
	{
		for (int32 BusSendType = 0; BusSendType < (int32)EBusSendType::Count; ++BusSendType)
		{
			// And add all the source bus sends
			for (FSoundSourceBusSendInfo& SendInfo : WaveInstance->BusSends[BusSendType])
			{
				// Avoid redoing duplicate code for sending audio to source bus or audio bus. Most of it is the same other than the bus id.
				auto SetupBusSend = [this](TArray<FInitAudioBusSend>* AudioBusSends, const FSoundSourceBusSendInfo& InSendInfo, int32 InBusSendType, uint32 InBusId, bool bEnableBusSends, int32 InBusChannels)
				{
					FInitAudioBusSend BusSend;
					BusSend.AudioBusId = InBusId;
					BusSend.BusChannels = InBusChannels;
					
					if(bEnableBusSends)
					{
						BusSend.SendLevel = InSendInfo.SendLevel;
					}
					else
					{
						BusSend.SendLevel = 0;
					}
					
					if (AudioBusSends)
					{
						AudioBusSends[InBusSendType].Add(BusSend);
					}

					FDynamicBusSendInfo NewDynamicBusSendInfo;
					NewDynamicBusSendInfo.SendLevel = InSendInfo.SendLevel;
					NewDynamicBusSendInfo.BusId = BusSend.AudioBusId;
					NewDynamicBusSendInfo.BusSendLevelControlMethod = InSendInfo.SourceBusSendLevelControlMethod;
					NewDynamicBusSendInfo.BusSendType = (EBusSendType)InBusSendType;
					NewDynamicBusSendInfo.MinSendLevel = InSendInfo.MinSendLevel;
					NewDynamicBusSendInfo.MaxSendLevel = InSendInfo.MaxSendLevel;
					NewDynamicBusSendInfo.MinSendDistance = InSendInfo.MinSendDistance;
					NewDynamicBusSendInfo.MaxSendDistance = InSendInfo.MaxSendDistance;
					NewDynamicBusSendInfo.CustomSendLevelCurve = InSendInfo.CustomSendLevelCurve;

					// Copy the bus SourceBusSendInfo structs to a local copy so we can update it in the update tick
					bool bIsNew = true;
					for (FDynamicBusSendInfo& BusSendInfo : DynamicBusSendInfos)
					{
						if (BusSendInfo.BusId == NewDynamicBusSendInfo.BusId)
						{
							BusSendInfo = NewDynamicBusSendInfo;
							BusSendInfo.bIsInit = false;
							bIsNew = false;
							break;
						}
					}

					if (bIsNew)
					{
						DynamicBusSendInfos.Add(NewDynamicBusSendInfo);
					}

					// Flag that we're sending audio to buses so we can check for updates to send levels
					bSendingAudioToBuses = true;
				};

				// Retrieve bus id of the audio bus to use
				if (SendInfo.SoundSourceBus)
				{						
					uint32 BusId;
					int32 BusChannels;

					// Either use the bus id of the source bus's audio bus id if it was specified
					if (SendInfo.SoundSourceBus->AudioBus)
					{
						BusId = SendInfo.SoundSourceBus->AudioBus->GetUniqueID();
						BusChannels = (int32)SendInfo.SoundSourceBus->AudioBus->GetNumChannels();
					}
					else
					{
						// otherwise, use the id of the source bus itself (for an automatic source bus)
						BusId = SendInfo.SoundSourceBus->GetUniqueID();
						BusChannels = SendInfo.SoundSourceBus->NumChannels;
					}

					// Call lambda w/ the correctly derived bus id
					SetupBusSend(OutAudioBusSends, SendInfo, BusSendType, BusId, bEnableBusSends, BusChannels);
				}

				if (SendInfo.AudioBus)
				{
					// Only need to send audio to just the specified audio bus
					uint32 BusId = SendInfo.AudioBus->GetUniqueID();
					int32 BusChannels = (int32)SendInfo.AudioBus->AudioBusChannels + 1;

					// Note we will be sending audio to both the specified source bus and the audio bus with the same send level
					SetupBusSend(OutAudioBusSends, SendInfo, BusSendType, BusId, bEnableBusSends, BusChannels);
				}
			}
		}
	}

	void FMixerSource::Update()
	{
		CSV_SCOPED_TIMING_STAT(Audio, UpdateSources);
		SCOPE_CYCLE_COUNTER(STAT_AudioUpdateSources);

		LLM_SCOPE(ELLMTag::AudioMixer);

		if (!WaveInstance || !MixerSourceVoice || Paused || InitializationState == EMixerSourceInitializationState::NotInitialized)
		{
			return;
		}

		AUDIO_MIXER_TRACE_CPUPROFILER_EVENT_SCOPE(FMixerSource::Update);

		// if MarkAsGarbage() was called, WaveInstance->WaveData is null
		if (!WaveInstance->WaveData)
		{
			StopNow();
			return;
		}

		++TickCount;

		// Allow plugins to override any data in a waveinstance
		if (AudioDevice->SourceDataOverridePluginInterface && WaveInstance->bEnableSourceDataOverride)
		{
			uint32 SourceId = MixerSourceVoice->GetSourceId();
			int32 ListenerIndex = WaveInstance->ActiveSound->GetClosestListenerIndex();

			FTransform ListenerTransform;
			AudioDevice->GetListenerTransform(ListenerIndex, ListenerTransform);

			AudioDevice->SourceDataOverridePluginInterface->GetSourceDataOverrides(SourceId, ListenerTransform, WaveInstance);
		}

		// AudioLink, push state if we're enabled and 3d.
		if (bIs3D && AudioLink.IsValid())
		{
			IAudioLinkSourcePushed::FOnUpdateWorldStateParams Params;
			Params.WorldTransform = WaveInstance->ActiveSound->Transform;
			AudioLink->OnUpdateWorldState(Params);
		}

		UpdateModulation();

		UpdatePitch();

		UpdateVolume();

		UpdateSpatialization();

		UpdateEffects();

		UpdateSourceBusSends();

		UpdateChannelMaps();

#if ENABLE_AUDIO_DEBUG
		UpdateCPUCoreUtilization();

		Audio::FAudioDebugger::DrawDebugInfo(*this);
#endif // ENABLE_AUDIO_DEBUG
	}

	bool FMixerSource::PrepareForInitialization(FWaveInstance* InWaveInstance)
	{
		LLM_SCOPE(ELLMTag::AudioMixer);

		if (!ensure(InWaveInstance))
		{
			return false;
		}

		// We are currently not supporting playing audio on a controller
		if (InWaveInstance->OutputTarget == EAudioOutputTarget::Controller)
		{
			return false;
		}

		AUDIO_MIXER_TRACE_CPUPROFILER_EVENT_SCOPE(AudioMixerSource::PrepareForInitialization);

		// We are not initialized yet. We won't be until the sound file finishes loading and parsing the header.
		InitializationState = EMixerSourceInitializationState::Initializing;

		//  Reset so next instance will warn if algorithm changes in-flight
		bEditorWarnedChangedSpatialization = false;

		const bool bIsSeeking = InWaveInstance->StartTime > 0.0f;

		check(InWaveInstance);
		check(AudioDevice);

		check(!MixerBuffer);
		MixerBuffer = FMixerBuffer::Init(AudioDevice, InWaveInstance->WaveData, bIsSeeking /* bForceRealtime */);

		if (!MixerBuffer)
		{
			FreeResources(); // APM: maybe need to call this here too? 
			return false;
		}

		// WaveData must be valid beyond this point, otherwise MixerBuffer
		// would have failed to init.
		check(InWaveInstance->WaveData);
		USoundWave& SoundWave = *InWaveInstance->WaveData;

		Buffer = MixerBuffer;
		WaveInstance = InWaveInstance;

		LPFFrequency = MAX_FILTER_FREQUENCY;
		LastLPFFrequency = FLT_MAX;

		HPFFrequency = 0.0f;
		LastHPFFrequency = FLT_MAX;

		bIsDone = false;

		// Not all wave data types have a non-zero duration
		if (SoundWave.Duration > 0.0f)
		{
			if (!SoundWave.bIsSourceBus)
			{
				NumTotalFrames = SoundWave.Duration * SoundWave.GetSampleRateForCurrentPlatform();
				check(NumTotalFrames > 0);
			}
			else if (!SoundWave.IsLooping())
			{
				NumTotalFrames = SoundWave.Duration * AudioDevice->GetSampleRate();
				check(NumTotalFrames > 0);
			}

			StartFrame = FMath::Clamp<int32>((InWaveInstance->StartTime / SoundWave.Duration) * NumTotalFrames, 0, NumTotalFrames);
		}

		check(!MixerSourceBuffer.IsValid());

		// Active sound instance ID is the audio component ID of active sound.
		uint64 InstanceID = 0;
		uint32 PlayOrder = 0;
		bool bActiveSoundIsPreviewSound = false;
		TArray<FAudioParameter> DefaultParameters;
		FActiveSound* ActiveSound = WaveInstance->ActiveSound;
		if (ActiveSound)
		{
			InstanceID = ActiveSound->GetAudioComponentID();
			PlayOrder = ActiveSound->GetPlayOrder();
			bActiveSoundIsPreviewSound = ActiveSound->bIsPreviewSound;
			if (Audio::IParameterTransmitter* Transmitter = ActiveSound->GetTransmitter())
			{
				DefaultParameters = Transmitter->GetParameters();
				SoundWave.InitParameters(DefaultParameters);
			}
		}

		FMixerSourceBufferInitArgs BufferInitArgs;
		BufferInitArgs.AudioDeviceID = AudioDevice->DeviceID;
		BufferInitArgs.AudioComponentID = InstanceID;
		BufferInitArgs.InstanceID = GetTransmitterID(InstanceID, WaveInstance->WaveInstanceHash, PlayOrder);
		BufferInitArgs.SampleRate = AudioDevice->GetSampleRate();
		BufferInitArgs.AudioMixerNumOutputFrames = MixerDevice->GetNumOutputFrames();
		BufferInitArgs.Buffer = MixerBuffer;
		BufferInitArgs.SoundWave = &SoundWave;
		BufferInitArgs.LoopingMode = InWaveInstance->LoopingMode;
		BufferInitArgs.bIsSeeking = bIsSeeking;
		BufferInitArgs.bIsPreviewSound = bActiveSoundIsPreviewSound;

		MixerSourceBuffer = FMixerSourceBuffer::Create(BufferInitArgs, MoveTemp(DefaultParameters));
		
		if (!MixerSourceBuffer.IsValid())
		{
			FreeResources();

			// Guarantee that this wave instance does not try to replay by disabling looping.
			WaveInstance->LoopingMode = LOOP_Never;

			if (ensure(ActiveSound))
			{
				ActiveSound->bShouldRemainActiveIfDropped = false;
			}
		}
		
		return MixerSourceBuffer.IsValid();
	}

	bool FMixerSource::IsPreparedToInit()
	{
		LLM_SCOPE(ELLMTag::AudioMixer);
		AUDIO_MIXER_TRACE_CPUPROFILER_EVENT_SCOPE(AudioMixerSource::IsPreparedToInit);

		if (MixerBuffer && MixerBuffer->IsRealTimeSourceReady())
		{
			check(MixerSourceBuffer.IsValid());

			// Check if we have a realtime audio task already (doing first decode)
			if (MixerSourceBuffer->IsAsyncTaskInProgress())
			{
				// not ready
				return MixerSourceBuffer->IsAsyncTaskDone();
			}
			else if (WaveInstance)
			{
				if (WaveInstance->WaveData->bIsSourceBus)
				{
					// Buses don't need to do anything to play audio
					return true;
				}
				else
				{
					// Now check to see if we need to kick off a decode the first chunk of audio
					const EBufferType::Type BufferType = MixerBuffer->GetType();
					if ((BufferType == EBufferType::PCMRealTime || BufferType == EBufferType::Streaming) && WaveInstance->WaveData)
					{
						// If any of these conditions meet, we need to do an initial async decode before we're ready to start playing the sound
						if (WaveInstance->StartTime > 0.0f || WaveInstance->WaveData->bProcedural || WaveInstance->WaveData->bIsSourceBus || !WaveInstance->WaveData->CachedRealtimeFirstBuffer)
						{
							// Before reading more PCMRT data, we first need to seek the buffer
							if (WaveInstance->IsSeekable())
							{
								MixerBuffer->Seek(WaveInstance->StartTime);
							}

							check(MixerSourceBuffer.IsValid());

							ICompressedAudioInfo* Decoder = MixerBuffer->GetDecompressionState(false);
							if (BufferType == EBufferType::Streaming)
							{
								IStreamingManager::Get().GetAudioStreamingManager().AddDecoder(Decoder);
							}

							MixerSourceBuffer->ReadMoreRealtimeData(Decoder, 0, EBufferReadMode::Asynchronous);

							// not ready
							return false;
						}
					}
				}
			}

			return true;
		}

		return false;
	}

	bool FMixerSource::IsInitialized() const
	{
		return InitializationState == EMixerSourceInitializationState::Initialized;
	}

	void FMixerSource::Play()
	{
		if (!WaveInstance)
		{
			return;
		}

		// Don't restart the sound if it was stopping when we paused, just stop it.
		if (Paused && (bIsStopping || bIsDone))
		{
			StopNow();
			return;
		}

		if (bIsStopping)
		{
			UE_LOG(LogAudioMixer, Warning, TEXT("Restarting a source which was stopping. Stopping now."));
			return;
		}

		AUDIO_MIXER_TRACE_CPUPROFILER_EVENT_SCOPE(AudioMixerSource::Play);

		// It's possible if Pause and Play are called while a sound is async initializing. In this case
		// we'll just not actually play the source here. Instead we'll call play when the sound finishes loading.
		if (MixerSourceVoice && InitializationState == EMixerSourceInitializationState::Initialized)
		{
			MixerSourceVoice->Play();

#if UE_AUDIO_PROFILERTRACE_ENABLED
			const bool bChannelEnabled = UE_TRACE_CHANNELEXPR_IS_ENABLED(AudioMixerChannel);
			if (bChannelEnabled && WaveInstance)
			{
				if (const FActiveSound* ActiveSound = WaveInstance->ActiveSound)
				{
					int32 TraceSourceId = INDEX_NONE;
					if (MixerSourceVoice)
					{
						TraceSourceId = MixerSourceVoice->GetSourceId();
					}

					UE_TRACE_LOG(Audio, MixerSourceStart, AudioMixerChannel)
						<< MixerSourceStart.DeviceId(MixerDevice->DeviceID)
						<< MixerSourceStart.Timestamp(FPlatformTime::Cycles64())
						<< MixerSourceStart.PlayOrder(WaveInstance->GetPlayOrder())
						<< MixerSourceStart.SourceId(TraceSourceId)
						<< MixerSourceStart.ComponentId(ActiveSound->GetAudioComponentID())
						<< MixerSourceStart.Name(*WaveInstance->WaveData->GetPathName());
				}
			}
#endif // UE_AUDIO_PROFILERTRACE_ENABLED
		}

		bIsStopping = false;
		Paused = false;
		Playing = true;
		bLoopCallback = false;
		bIsDone = false;
	}

	void FMixerSource::Stop()
	{
		LLM_SCOPE(ELLMTag::AudioMixer);

		if (InitializationState == EMixerSourceInitializationState::NotInitialized)
		{
			return;
		}

		if (!MixerSourceVoice)
		{
			StopNow();
			return;
		}

		USoundWave* SoundWave = WaveInstance ? WaveInstance->WaveData : nullptr;

		// If MarkAsGarbage() was called, SoundWave can be null
		if (!SoundWave)
		{
			StopNow();
			return;
		}

		// Stop procedural sounds immediately that don't require fade
		if (SoundWave->bProcedural && !SoundWave->bRequiresStopFade)
		{
			StopNow();
			return;
		}

		if (bIsDone)
		{
			StopNow();
			return;
		}

		if (Playing && !bIsStoppingVoicesEnabled)
		{
			StopNow();
			return;
		}

		// Otherwise, we need to do a quick fade-out of the sound and put the state
		// of the sound into "stopping" mode. This prevents this source from
		// being put into the "free" pool and prevents the source from freeing its resources
		// until the sound has finished naturally (i.e. faded all the way out)

		// Let the wave instance know it's stopping
		if (!bIsStopping)
		{
			WaveInstance->SetStopping(true);

			MixerSourceVoice->StopFade(AudioMixerSourceFadeMinCVar);
			bIsStopping = true;
			Paused = false;
		}
	}

	void FMixerSource::StopNow()
	{
		LLM_SCOPE(ELLMTag::AudioMixer);

		// Immediately stop the sound source

		InitializationState = EMixerSourceInitializationState::NotInitialized;

		IStreamingManager::Get().GetAudioStreamingManager().RemoveStreamingSoundSource(this);

		bIsStopping = false;

		if (WaveInstance)
		{
			if (MixerSourceVoice && Playing)
			{
#if UE_AUDIO_PROFILERTRACE_ENABLED
				const bool bChannelEnabled = UE_TRACE_CHANNELEXPR_IS_ENABLED(AudioMixerChannel);
				if (bChannelEnabled)
				{
					int32 TraceSourceId = INDEX_NONE;
					if (MixerSourceVoice)
					{
						TraceSourceId = MixerSourceVoice->GetSourceId();
					}

					UE_TRACE_LOG(Audio, MixerSourceStop, AudioMixerChannel)
						<< MixerSourceStop.DeviceId(MixerDevice->DeviceID)
						<< MixerSourceStop.Timestamp(FPlatformTime::Cycles64())
						<< MixerSourceStop.PlayOrder(WaveInstance->GetPlayOrder());
				}
#endif // UE_AUDIO_PROFILERTRACE_ENABLED

				MixerSourceVoice->Stop();
			}

			Paused = false;
			Playing = false;

			FreeResources();
		}

		FSoundSource::Stop();
	}

	void FMixerSource::Pause()
	{
		if (!WaveInstance)
		{
			return;
		}

		if (bIsStopping)
		{
			return;
		}

		if (MixerSourceVoice)
		{
			MixerSourceVoice->Pause();
		}

		Paused = true;
	}

	bool FMixerSource::IsFinished()
	{
		// A paused source is not finished.
		if (Paused)
		{
			return false;
		}

		if (InitializationState == EMixerSourceInitializationState::NotInitialized)
		{
			return true;
		}

		if (InitializationState == EMixerSourceInitializationState::Initializing)
		{
			return false;
		}

		if (WaveInstance && MixerSourceVoice)
		{
			if (bIsDone && bIsEffectTailsDone)
			{
				WaveInstance->NotifyFinished();
				bIsStopping = false;
				return true;
			}
			else if (bLoopCallback && WaveInstance->LoopingMode == LOOP_WithNotification)
			{
				WaveInstance->NotifyFinished();
				bLoopCallback = false;
			}

			return false;
		}
		return true;
	}

	float FMixerSource::GetPlaybackPercent() const
	{
		if (InitializationState != EMixerSourceInitializationState::Initialized)
		{
			return PreviousPlaybackPercent;
		}

		if (MixerSourceVoice && NumTotalFrames > 0)
		{
			int64 NumFrames = StartFrame + MixerSourceVoice->GetNumFramesPlayed();
			AUDIO_MIXER_CHECK(NumTotalFrames > 0);
			PreviousPlaybackPercent = (float)NumFrames / NumTotalFrames;
			if (WaveInstance->LoopingMode == LOOP_Never)
			{
				PreviousPlaybackPercent = FMath::Min(PreviousPlaybackPercent, 1.0f);
			}
			return PreviousPlaybackPercent;
		}
		else
		{
			// If we don't have any frames, that means it's a procedural sound wave, which means
			// that we're never going to have a playback percentage.
			return 1.0f;
		}
	}

	int64 FMixerSource::GetNumFramesPlayed() const
	{
		if (InitializationState == EMixerSourceInitializationState::Initialized && MixerSourceVoice != nullptr)
		{
			return MixerSourceVoice->GetNumFramesPlayed();
		}

		return 0;
	}

	float FMixerSource::GetEnvelopeValue() const
	{
		if (MixerSourceVoice)
		{
			return MixerSourceVoice->GetEnvelopeValue();
		}
		return 0.0f;
	}

	void FMixerSource::OnBeginGenerate()
	{
	}

	void FMixerSource::OnDone()
	{
		bIsDone = true;
	}

	void FMixerSource::OnEffectTailsDone()
	{
		bIsEffectTailsDone = true;
	}

	void FMixerSource::FreeResources()
	{
		LLM_SCOPE(ELLMTag::AudioMixer);

		if (MixerBuffer)
		{
			MixerBuffer->EnsureHeaderParseTaskFinished();
		}

		check(!bIsStopping);
		check(!Playing);

		if (AudioLink.IsValid())
		{
			AudioLink.Reset();
		}

		// Make a new pending release data ptr to pass off release data
		if (MixerSourceVoice)
		{
			// Release the source using the propagation interface
			if (AudioDevice->SourceDataOverridePluginInterface)
			{
				uint32 SourceId = MixerSourceVoice->GetSourceId();
				AudioDevice->SourceDataOverridePluginInterface->OnReleaseSource(SourceId);
			}

			// We're now "releasing" so don't recycle this voice until we get notified that the source has finished
			bIsReleasing = true;

			// This will trigger FMixerSource::OnRelease from audio render thread.
			MixerSourceVoice->Release();
			MixerSourceVoice = nullptr;
		}

		MixerSourceBuffer.Reset();
		Buffer = nullptr;
		bLoopCallback = false;
		NumTotalFrames = 0;

		if (MixerBuffer)
		{
			EBufferType::Type BufferType = MixerBuffer->GetType();
			if (BufferType == EBufferType::PCMRealTime || BufferType == EBufferType::Streaming)
			{
				delete MixerBuffer;
			}

			MixerBuffer = nullptr;
		}

		// Reset the source's channel maps
		FRWScopeLock Lock(ChannelMapLock, SLT_Write);
		ChannelMap.Reset();

		InitializationState = EMixerSourceInitializationState::NotInitialized;
	}

	void FMixerSource::UpdatePitch()
	{
		AUDIO_MIXER_CHECK(MixerBuffer);

		check(WaveInstance);

		FActiveSound* ActiveSound = WaveInstance->ActiveSound;
		check(ActiveSound);

		Pitch = WaveInstance->GetPitch();

		// Don't apply global pitch scale to UI sounds
		if (!WaveInstance->bIsUISound)
		{
			Pitch *= AudioDevice->GetGlobalPitchScale().GetValue();
		}

		Pitch = AudioDevice->ClampPitch(Pitch);

		// Scale the pitch by the ratio of the audio buffer sample rate and the actual sample rate of the hardware
		if (MixerBuffer)
		{
			const float MixerBufferSampleRate = MixerBuffer->GetSampleRate();
			const float AudioDeviceSampleRate = AudioDevice->GetSampleRate();
			Pitch *= MixerBufferSampleRate / AudioDeviceSampleRate;

			MixerSourceVoice->SetPitch(Pitch);
		}

		USoundWave* WaveData = WaveInstance->WaveData;
		check(WaveData);
		const float ModPitchBase = ModulationUtils::GetRoutedPitch(*WaveInstance, *WaveData, *ActiveSound);
		MixerSourceVoice->SetModPitch(ModPitchBase);
	}

	float FMixerSource::GetInheritedSubmixVolumeModulation() const
	{
		if (!MixerDevice)
		{
			return 1.0f;
		}

		FAudioDevice::FAudioSpatializationInterfaceInfo SpatializationInfo = MixerDevice->GetCurrentSpatializationPluginInterfaceInfo();
		// We only hit this condition if, while the sound is playing, the spatializer changes from an external send to a non-external one.
		// If that happens, the submix will catch all modulation so this function's logic is not needed.
		if (!SpatializationInfo.bSpatializationIsExternalSend)
		{
			return 1.0f;
		}

		// if there is a return submix, we need to figure out where to stop manually attenuating
		// Because the submix will modulate itself later
		// Since the graph has tree-like structure, we can create a list of the return submix's ancestors
		// to use while traversing the other submix's ancestors
		TArray<uint32> ReturnSubmixAncestors;
		if (SpatializationInfo.bReturnsToSubmixGraph)
		{
			if (MixerDevice && MixerDevice->ReverbPluginInterface)
			{
				USoundSubmix* ReturnSubmix = MixerDevice->ReverbPluginInterface->GetSubmix();
				if (ReturnSubmix)
				{
					FMixerSubmixWeakPtr CurrReturnSubmixWeakPtr = MixerDevice->GetSubmixInstance(ReturnSubmix);
					FMixerSubmixPtr CurrReturnSubmixPtr = CurrReturnSubmixWeakPtr.Pin();
					while (CurrReturnSubmixPtr && CurrReturnSubmixPtr->IsValid())
					{
						ReturnSubmixAncestors.Add(CurrReturnSubmixPtr->GetId());

						CurrReturnSubmixWeakPtr = CurrReturnSubmixPtr->GetParent();
						CurrReturnSubmixPtr = CurrReturnSubmixWeakPtr.Pin();
					}
				}
			}
		}

		float SubmixModVolume = 1.0f;

		FMixerSubmixWeakPtr CurrSubmixWeakPtr = MixerDevice->GetSubmixInstance(WaveInstance->SoundSubmix);
		FMixerSubmixPtr CurrSubmixPtr = CurrSubmixWeakPtr.Pin();
		// Check the submix and all its parents in the graph for active modulation
		while (CurrSubmixPtr && CurrSubmixPtr->IsValid())
		{
			// Matching ID means the external spatializer has returned to the submix graph at this point,
			// so we no longer need to manually apply volume modulation
			if (SpatializationInfo.bReturnsToSubmixGraph && ReturnSubmixAncestors.Contains(CurrSubmixPtr->GetId()))
			{
				break;
			}

			FModulationDestination* SubmixOutVolDest = CurrSubmixPtr->GetOutputVolumeDestination();
			FModulationDestination* SubmixWetVolDest = CurrSubmixPtr->GetWetVolumeDestination();
			if (SubmixOutVolDest)
			{
				SubmixModVolume *= SubmixOutVolDest->GetValue();
			}
			if (SubmixWetVolDest)
			{
				SubmixModVolume *= SubmixWetVolDest->GetValue();
			}

			CurrSubmixWeakPtr = CurrSubmixPtr->GetParent();
			CurrSubmixPtr = CurrSubmixWeakPtr.Pin();
		}

		return SubmixModVolume;
	}

	void FMixerSource::UpdateVolume()
	{
		// TODO: investigate if occlusion should be split from raw distance attenuation
		MixerSourceVoice->SetDistanceAttenuation(WaveInstance->GetDistanceAndOcclusionAttenuation());

		float CurrentVolume = 0.0f;
		if (!AudioDevice->IsAudioDeviceMuted())
		{
			// 1. Apply device gain stage(s)
			CurrentVolume = WaveInstance->ActiveSound->bIsPreviewSound ? 1.0f : AudioDevice->GetPrimaryVolume();
			CurrentVolume *= AudioDevice->GetPlatformAudioHeadroom();

			// 2. Apply instance gain stage(s)
			CurrentVolume *= WaveInstance->GetVolume();
			CurrentVolume *= WaveInstance->GetDynamicVolume();

			// 3. Submix Volume Modulation (this only happens if the asset is binaural and we're sending to an external submix)
			if (bBypassingSubmixModulation)
			{
				CurrentVolume *= GetInheritedSubmixVolumeModulation();
			}

			// 4. Apply editor gain stage(s)
			CurrentVolume = FMath::Clamp<float>(GetDebugVolume(CurrentVolume), 0.0f, MAX_VOLUME);

			FActiveSound* ActiveSound = WaveInstance->ActiveSound;
			check(ActiveSound);

			USoundWave* WaveData = WaveInstance->WaveData;
			check(WaveData);
			const float ModVolumeBase = ModulationUtils::GetRoutedVolume(*WaveInstance, *WaveData, *ActiveSound);
			MixerSourceVoice->SetModVolume(ModVolumeBase);
		}
		MixerSourceVoice->SetVolume(CurrentVolume);
	}

	void FMixerSource::UpdateSpatialization()
	{
		FQuat LastEmitterWorldRotation = SpatializationParams.EmitterWorldRotation;
		SpatializationParams = GetSpatializationParams();
		SpatializationParams.LastEmitterWorldRotation = LastEmitterWorldRotation;

		if (WaveInstance->GetUseSpatialization() || WaveInstance->bIsAmbisonics)
		{
			MixerSourceVoice->SetSpatializationParams(SpatializationParams);
		}
	}
	
	void FMixerSource::UpdateSubmixSendLevels(const FSoundSubmixSendInfoBase& InSendInfo, const EMixerSourceSubmixSendStage InSendStage)
	{
		if (InSendInfo.SoundSubmix != nullptr)
		{
			const FMixerSubmixWeakPtr SubmixInstance = MixerDevice->GetSubmixInstance(InSendInfo.SoundSubmix);
			float SendLevel = 1.0f;

			// calculate send level based on distance if that method is enabled
			if (!WaveInstance->bEnableSubmixSends)
			{
				SendLevel = 0.0f;
			}
			else if (InSendInfo.SendLevelControlMethod == ESendLevelControlMethod::Manual)
			{
				if (InSendInfo.DisableManualSendClamp)
				{
					SendLevel = InSendInfo.SendLevel;
				}
				else
				{
					SendLevel = FMath::Clamp(InSendInfo.SendLevel, 0.0f, 1.0f);
				}
			}
			else
			{
				// The alpha value is determined identically between manual and custom curve methods
				const FVector2D SendRadialRange = { InSendInfo.MinSendDistance, InSendInfo.MaxSendDistance};
				const FVector2D SendLevelRange = { InSendInfo.MinSendLevel, InSendInfo.MaxSendLevel };
				const float Denom = FMath::Max(SendRadialRange.Y - SendRadialRange.X, 1.0f);
				const float Alpha = FMath::Clamp((WaveInstance->ListenerToSoundDistance - SendRadialRange.X) / Denom, 0.0f, 1.0f);

				if (InSendInfo.SendLevelControlMethod == ESendLevelControlMethod::Linear)
				{
					SendLevel = FMath::Clamp(FMath::Lerp(SendLevelRange.X, SendLevelRange.Y, Alpha), 0.0f, 1.0f);
				}
				else // use curve
				{
					SendLevel = FMath::Clamp(InSendInfo.CustomSendLevelCurve.GetRichCurveConst()->Eval(Alpha), 0.0f, 1.0f);
				}
			}

			// set the level and stage for this send
			MixerSourceVoice->SetSubmixSendInfo(SubmixInstance, SendLevel, InSendStage);
		}
	}

	void FMixerSource::UpdateEffects()
	{
		// Update the default LPF filter frequency
		SetFilterFrequency();

		if (LastLPFFrequency != LPFFrequency)
		{
			MixerSourceVoice->SetLPFFrequency(LPFFrequency);
			LastLPFFrequency = LPFFrequency;
		}

		if (LastHPFFrequency != HPFFrequency)
		{
			MixerSourceVoice->SetHPFFrequency(HPFFrequency);
			LastHPFFrequency = HPFFrequency;
		}

		check(WaveInstance);
		FActiveSound* ActiveSound = WaveInstance->ActiveSound;
		check(ActiveSound);

		USoundWave* WaveData = WaveInstance->WaveData;
		check(WaveData);

		float ModHighpassBase = ModulationUtils::GetRoutedHighpass(*WaveInstance, *WaveData, *ActiveSound);
		MixerSourceVoice->SetModHPFFrequency(ModHighpassBase);

		float ModLowpassBase = ModulationUtils::GetRoutedLowpass(*WaveInstance, *WaveData, *ActiveSound);
		MixerSourceVoice->SetModLPFFrequency(ModLowpassBase);

		// If reverb is applied, figure out how of the source to "send" to the reverb.
		if (WaveInstance->bReverb)
		{
			// Send the source audio to the reverb plugin if enabled
			if (UseReverbPlugin() && AudioDevice->ReverbPluginInterface)
			{
				check(MixerDevice);
				FMixerSubmixPtr ReverbPluginSubmixPtr = MixerDevice->GetSubmixInstance(AudioDevice->ReverbPluginInterface->GetSubmix()).Pin();
				if (ReverbPluginSubmixPtr.IsValid())
				{
					MixerSourceVoice->SetSubmixSendInfo(ReverbPluginSubmixPtr, WaveInstance->ReverbSendLevel);
				}
			}

			// Send the source audio to the master reverb
			MixerSourceVoice->SetSubmixSendInfo(MixerDevice->GetMasterReverbSubmix(), WaveInstance->ReverbSendLevel);
		}

		// Safely track if the submix has changed between updates.
		bool bSubmixHasChanged = false;
		TObjectKey<USoundSubmixBase> SubmixKey(WaveInstance->SoundSubmix);
		if (SubmixKey != PrevousSubmix )
		{
			bSubmixHasChanged = true;
		}

		// This will reattempt to resolve a submix each update if there's a valid input
		if ((!WaveInstance->SoundSubmix && PreviousSubmixResolved.IsValid()) || 
		     (WaveInstance->SoundSubmix && !PreviousSubmixResolved.IsValid()) )
		{
			bSubmixHasChanged = true;
		}

		//Check whether the base submix send has been enabled or disabled since the last update
		//Or if the submix has now been registered with the world.
		if (WaveInstance->bEnableBaseSubmix != bPreviousBaseSubmixEnablement || bSubmixHasChanged)
		{
			// set the level for this send
			FMixerSubmixWeakPtr SubmixPtr;
			if (WaveInstance->SoundSubmix)
			{
				SubmixPtr = MixerDevice->GetSubmixInstance(WaveInstance->SoundSubmix);
			}
			else if (!WaveInstance->bIsDynamic) // Dynamic submixes don't auto connect.
			{
				SubmixPtr = MixerDevice->GetBaseDefaultSubmix(); // This will try base default and fall back to master if that fails.
			}


			MixerSourceVoice->SetSubmixSendInfo(SubmixPtr, WaveInstance->bEnableBaseSubmix);
			bPreviousBaseSubmixEnablement = WaveInstance->bEnableBaseSubmix;
			PreviousSubmixResolved = SubmixPtr;
			PrevousSubmix = SubmixKey;
		}

		// Attenuation Submix Sends. (these come from Attenuation assets).
		// These are largely identical to SoundSubmix Sends, but don't specify a send stage, so we pass one here.
		for (const FAttenuationSubmixSendSettings& SendSettings : WaveInstance->AttenuationSubmixSends)
		{
			UpdateSubmixSendLevels(SendSettings, EMixerSourceSubmixSendStage::PostDistanceAttenuation);
		}
		// Clear any previous sends that may not exist now.
		MixerSourcePrivate::ClearPreviousSubmixSends(PreviousAttenuationSendSettings, WaveInstance->AttenuationSubmixSends, MixerDevice, MixerSourceVoice);
		PreviousAttenuationSendSettings = WaveInstance->AttenuationSubmixSends; 
		
		// Sound submix Sends. (these come from SoundBase derived assets).
		for (FSoundSubmixSendInfo& SendInfo : WaveInstance->SoundSubmixSends)
		{
			UpdateSubmixSendLevels(SendInfo, MixerSourcePrivate::SubmixSendStageToMixerSourceSubmixSendStage(SendInfo.SendStage));
		}
		// Again, Clear any sends that maybe not exist now.
		MixerSourcePrivate::ClearPreviousSubmixSends(PreviousSubmixSendSettings, WaveInstance->SoundSubmixSends, MixerDevice, MixerSourceVoice);
		PreviousSubmixSendSettings = WaveInstance->SoundSubmixSends;

		MixerSourceVoice->SetEnablement(WaveInstance->bEnableBusSends, WaveInstance->bEnableBaseSubmix, WaveInstance->bEnableSubmixSends);

		MixerSourceVoice->SetSourceBufferListener(WaveInstance->SourceBufferListener, WaveInstance->bShouldSourceBufferListenerZeroBuffer);
	}

	void FMixerSource::UpdateModulation()
	{
		check(WaveInstance);

		FActiveSound* ActiveSound = WaveInstance->ActiveSound;
		check(ActiveSound);

		if (ActiveSound->bModulationRoutingUpdated)
		{
			MixerSourceVoice->SetModulationRouting(ActiveSound->ModulationRouting);
		}

		ActiveSound->bModulationRoutingUpdated = false;
	}

	void FMixerSource::UpdateSourceBusSends()
	{
		// 1) loop through all bus sends
		// 2) check for any bus sends that are set to update non-manually
		// 3) Cache previous send level and only do update if it's changed in any significant amount

		SetupBusData();

		FActiveSound* ActiveSound = WaveInstance->ActiveSound;
		check(ActiveSound);

		// Check if the user actively called a function that alters bus sends since the last update
		bool bHasNewBusSends = ActiveSound->HasNewBusSends();

		if (!bSendingAudioToBuses && !bHasNewBusSends && !DynamicBusSendInfos.Num())
		{
			return;
		}

		if (bHasNewBusSends)
		{
			TArray<TTuple<EBusSendType, FSoundSourceBusSendInfo>> NewBusSends = ActiveSound->GetNewBusSends();
			for (TTuple<EBusSendType, FSoundSourceBusSendInfo>& NewSend : NewBusSends)
			{
				if (NewSend.Value.SoundSourceBus)
				{
					MixerSourceVoice->SetAudioBusSendInfo(NewSend.Key, NewSend.Value.SoundSourceBus->GetUniqueID(), NewSend.Value.SendLevel);
					bSendingAudioToBuses = true;
				}

				if (NewSend.Value.AudioBus)
				{
					MixerSourceVoice->SetAudioBusSendInfo(NewSend.Key, NewSend.Value.AudioBus->GetUniqueID(), NewSend.Value.SendLevel);
					bSendingAudioToBuses = true;
				}
			}

			ActiveSound->ResetNewBusSends();
		}

		// If this source is sending its audio to a bus, we need to check if it needs to be updated
		for (FDynamicBusSendInfo& DynamicBusSendInfo : DynamicBusSendInfos)
		{
			float SendLevel = 0.0f;

			if (DynamicBusSendInfo.BusSendLevelControlMethod == ESourceBusSendLevelControlMethod::Manual)
			{
				SendLevel = FMath::Clamp(DynamicBusSendInfo.SendLevel, 0.0f, 1.0f);
			}
			else
			{
				// The alpha value is determined identically between linear and custom curve methods
				const FVector2D SendRadialRange = { DynamicBusSendInfo.MinSendDistance, DynamicBusSendInfo.MaxSendDistance };
				const FVector2D SendLevelRange = { DynamicBusSendInfo.MinSendLevel, DynamicBusSendInfo.MaxSendLevel };
				const float Denom = FMath::Max(SendRadialRange.Y - SendRadialRange.X, 1.0f);
				const float Alpha = FMath::Clamp((WaveInstance->ListenerToSoundDistance - SendRadialRange.X) / Denom, 0.0f, 1.0f);

				if (DynamicBusSendInfo.BusSendLevelControlMethod == ESourceBusSendLevelControlMethod::Linear)
				{
					SendLevel = FMath::Clamp(FMath::Lerp(SendLevelRange.X, SendLevelRange.Y, Alpha), 0.0f, 1.0f);
				}
				else // use curve
				{
					SendLevel = FMath::Clamp(DynamicBusSendInfo.CustomSendLevelCurve.GetRichCurveConst()->Eval(Alpha), 0.0f, 1.0f);
				}
			}

			// If the send level changed, then we need to send an update to the audio render thread
			const bool bSendLevelChanged = !FMath::IsNearlyEqual(SendLevel, DynamicBusSendInfo.SendLevel);
			const bool bBusEnablementChanged = bPreviousBusEnablement != WaveInstance->bEnableBusSends;

			if (bSendLevelChanged || bBusEnablementChanged)
			{
				DynamicBusSendInfo.SendLevel = SendLevel;
				DynamicBusSendInfo.bIsInit = false;

				MixerSourceVoice->SetAudioBusSendInfo(DynamicBusSendInfo.BusSendType, DynamicBusSendInfo.BusId, SendLevel);

				bPreviousBusEnablement = WaveInstance->bEnableBusSends;
			}

		}
	}

	void FMixerSource::UpdateChannelMaps()
	{
		SetLFEBleed();

		int32 NumOutputDeviceChannels = MixerDevice->GetNumDeviceChannels();
		const FAudioPlatformDeviceInfo& DeviceInfo = MixerDevice->GetPlatformDeviceInfo();

		// Compute a new speaker map for each possible output channel mapping for the source
		const uint32 NumChannels = Buffer->NumChannels;
		bool bShouldSetMap = false;
		{
			FRWScopeLock Lock(ChannelMapLock, SLT_Write);
			bShouldSetMap = ComputeChannelMap(Buffer->NumChannels, ChannelMap);
		}
		if(bShouldSetMap)
		{			
			FRWScopeLock Lock(ChannelMapLock, SLT_ReadOnly);
			MixerSourceVoice->SetChannelMap(NumChannels, ChannelMap, bIs3D, WaveInstance->bCenterChannelOnly);
		}

		bPrevAllowedSpatializationSetting = IsSpatializationCVarEnabled();
	}

#if ENABLE_AUDIO_DEBUG
	void FMixerSource::UpdateCPUCoreUtilization()
	{
		if (MixerSourceVoice)
		{
			if (DebugInfo.IsValid())
			{
				FScopeLock DebugInfoLock(&DebugInfo->CS);
				DebugInfo->CPUCoreUtilization = MixerSourceVoice->GetCPUCoreUtilization();
			}
		}
	}
#endif // if ENABLE_AUDIO_DEBUG

	bool FMixerSource::ComputeMonoChannelMap(Audio::FAlignedFloatBuffer& OutChannelMap)
	{
		if (IsUsingObjectBasedSpatialization())
		{
			if (WaveInstance->SpatializationMethod != ESoundSpatializationAlgorithm::SPATIALIZATION_HRTF && !bEditorWarnedChangedSpatialization)
			{
				bEditorWarnedChangedSpatialization = true;
				UE_LOG(LogAudioMixer, Warning, TEXT("Changing the spatialization method on a playing sound is not supported (WaveInstance: %s)"), *WaveInstance->WaveData->GetFullName());
			}

			// Treat the source as if it is a 2D stereo source:
			return ComputeStereoChannelMap(OutChannelMap);
		}
		else if (WaveInstance->GetUseSpatialization() && (!FMath::IsNearlyEqual(WaveInstance->AbsoluteAzimuth, PreviousAzimuth, 0.01f) || MixerSourceVoice->NeedsSpeakerMap()))
		{
			// Don't need to compute the source channel map if the absolute azimuth hasn't changed much
			PreviousAzimuth = WaveInstance->AbsoluteAzimuth;
			OutChannelMap.Reset();

			int32 NumDeviceChannels = MixerDevice->GetNumDeviceChannels();
			float DefaultOmniAmount = 1.0f / NumDeviceChannels;
			MixerDevice->Get3DChannelMap(NumDeviceChannels, WaveInstance, WaveInstance->AbsoluteAzimuth, SpatializationParams.NonSpatializedAmount, nullptr, DefaultOmniAmount, OutChannelMap);
			return true;
		}
		else if (!OutChannelMap.Num() || (IsSpatializationCVarEnabled() != bPrevAllowedSpatializationSetting))
		{
			// Only need to compute the 2D channel map once
			MixerDevice->Get2DChannelMap(bIsVorbis, 1, WaveInstance->bCenterChannelOnly, OutChannelMap);
			return true;
		}

		// Return false means the channel map hasn't changed
		return false;
	}

	bool FMixerSource::ComputeStereoChannelMap(Audio::FAlignedFloatBuffer& OutChannelMap)
	{
		// Only recalculate positional data if the source has moved a significant amount:
		if (WaveInstance->GetUseSpatialization() && (!FMath::IsNearlyEqual(WaveInstance->AbsoluteAzimuth, PreviousAzimuth, 0.01f) || MixerSourceVoice->NeedsSpeakerMap()))
		{
			// Make sure our stereo emitter positions are updated relative to the sound emitter position
			if (Buffer->NumChannels == 2)
			{
				UpdateStereoEmitterPositions();
			}

			// Check whether voice is currently using 
			if (!IsUsingObjectBasedSpatialization())
			{
				float AzimuthOffset = 0.0f;

				float LeftAzimuth = 90.0f;
				float RightAzimuth = 270.0f;

				const float DistanceToUse = UseListenerOverrideForSpreadCVar ? WaveInstance->ListenerToSoundDistance : WaveInstance->ListenerToSoundDistanceForPanning;

				if (DistanceToUse > KINDA_SMALL_NUMBER)
				{
					AzimuthOffset = FMath::Atan(0.5f * WaveInstance->StereoSpread / DistanceToUse);
					AzimuthOffset = FMath::RadiansToDegrees(AzimuthOffset);

					LeftAzimuth = WaveInstance->AbsoluteAzimuth - AzimuthOffset;
					if (LeftAzimuth < 0.0f)
					{
						LeftAzimuth += 360.0f;
					}

					RightAzimuth = WaveInstance->AbsoluteAzimuth + AzimuthOffset;
					if (RightAzimuth > 360.0f)
					{
						RightAzimuth -= 360.0f;
					}
				}

				// Reset the channel map, the stereo spatialization channel mapping calls below will append their mappings
				OutChannelMap.Reset();

				int32 NumOutputChannels = MixerDevice->GetNumDeviceChannels();

				if (WaveInstance->NonSpatializedRadiusMode == ENonSpatializedRadiusSpeakerMapMode::OmniDirectional)
				{
					float DefaultOmniAmount = 1.0f / NumOutputChannels;
					MixerDevice->Get3DChannelMap(NumOutputChannels, WaveInstance, LeftAzimuth, SpatializationParams.NonSpatializedAmount, nullptr, DefaultOmniAmount, OutChannelMap);
					MixerDevice->Get3DChannelMap(NumOutputChannels, WaveInstance, RightAzimuth, SpatializationParams.NonSpatializedAmount, nullptr, DefaultOmniAmount, OutChannelMap);
				}
				else if (WaveInstance->NonSpatializedRadiusMode == ENonSpatializedRadiusSpeakerMapMode::Direct2D)
				{
					// Create some omni maps for left and right channels
					auto CreateLeftOmniMap = []() -> TMap<EAudioMixerChannel::Type, float>
					{
						TMap<EAudioMixerChannel::Type, float> LeftOmniMap;
						LeftOmniMap.Add(EAudioMixerChannel::FrontLeft, 1.0f);
						return LeftOmniMap;
					};

					auto CreateRightOmniMap = []() -> TMap<EAudioMixerChannel::Type, float>
					{
						TMap<EAudioMixerChannel::Type, float> RightOmniMap;
						RightOmniMap.Add(EAudioMixerChannel::FrontRight, 1.0f);
						return RightOmniMap;
					};

					static const TMap<EAudioMixerChannel::Type, float> LeftOmniMap = CreateLeftOmniMap();
					static const TMap<EAudioMixerChannel::Type, float> RightOmniMap = CreateRightOmniMap();

					MixerDevice->Get3DChannelMap(NumOutputChannels, WaveInstance, LeftAzimuth, SpatializationParams.NonSpatializedAmount, &LeftOmniMap, 0.0f, OutChannelMap);
					MixerDevice->Get3DChannelMap(NumOutputChannels, WaveInstance, RightAzimuth, SpatializationParams.NonSpatializedAmount, &RightOmniMap, 0.0f, OutChannelMap);
				}
				else
				{
					// If we are in 5.1, we need to use the side-channel speakers
					if (NumOutputChannels == 6)
					{
						// Create some omni maps for left and right channels
						auto CreateLeftOmniMap = []() -> TMap<EAudioMixerChannel::Type, float>
						{
							TMap<EAudioMixerChannel::Type, float> LeftOmniMap;
							LeftOmniMap.Add(EAudioMixerChannel::FrontLeft, 1.0f);
							LeftOmniMap.Add(EAudioMixerChannel::SideLeft, 1.0f);

							return LeftOmniMap;
						};

						auto CreateRightOmniMap = []() -> TMap<EAudioMixerChannel::Type, float>
						{
							TMap<EAudioMixerChannel::Type, float> RightOmniMap;
							RightOmniMap.Add(EAudioMixerChannel::FrontRight, 1.0f);
							RightOmniMap.Add(EAudioMixerChannel::SideRight, 1.0f);

							return RightOmniMap;
						};

						static const TMap<EAudioMixerChannel::Type, float> LeftOmniMap = CreateLeftOmniMap();
						static const TMap<EAudioMixerChannel::Type, float> RightOmniMap = CreateRightOmniMap();

						MixerDevice->Get3DChannelMap(NumOutputChannels, WaveInstance, LeftAzimuth, SpatializationParams.NonSpatializedAmount, &LeftOmniMap, 0.0f, OutChannelMap);
						MixerDevice->Get3DChannelMap(NumOutputChannels, WaveInstance, RightAzimuth, SpatializationParams.NonSpatializedAmount, &RightOmniMap, 0.0f, OutChannelMap);

					}
					// If we are in 7.1 we need to use the back-channel speakers
					else if (NumOutputChannels == 8)
					{
						// Create some omni maps for left and right channels
						auto CreateLeftOmniMap = []() -> TMap<EAudioMixerChannel::Type, float>
						{
							TMap<EAudioMixerChannel::Type, float> LeftOmniMap;
							LeftOmniMap.Add(EAudioMixerChannel::FrontLeft, 1.0f);
							LeftOmniMap.Add(EAudioMixerChannel::BackLeft, 1.0f);

							return LeftOmniMap;
						};

						auto CreateRightOmniMap = []() -> TMap<EAudioMixerChannel::Type, float>
						{
							TMap<EAudioMixerChannel::Type, float> RightOmniMap;
							RightOmniMap.Add(EAudioMixerChannel::FrontRight, 1.0f);
							RightOmniMap.Add(EAudioMixerChannel::BackRight, 1.0f);

							return RightOmniMap;
						};

						static const TMap<EAudioMixerChannel::Type, float> LeftOmniMap = CreateLeftOmniMap();
						static const TMap<EAudioMixerChannel::Type, float> RightOmniMap = CreateRightOmniMap();

						MixerDevice->Get3DChannelMap(NumOutputChannels, WaveInstance, LeftAzimuth, SpatializationParams.NonSpatializedAmount, &LeftOmniMap, 0.0f, OutChannelMap);
						MixerDevice->Get3DChannelMap(NumOutputChannels, WaveInstance, RightAzimuth, SpatializationParams.NonSpatializedAmount, &RightOmniMap, 0.0f, OutChannelMap);
					}
				}		

				return true;
			}
		}

		if (!OutChannelMap.Num() || (IsSpatializationCVarEnabled() != bPrevAllowedSpatializationSetting))
		{
			MixerDevice->Get2DChannelMap(bIsVorbis, 2, WaveInstance->bCenterChannelOnly, OutChannelMap);
			return true;
		}

		return false;
	}

	bool FMixerSource::ComputeChannelMap(const int32 NumSourceChannels, Audio::FAlignedFloatBuffer& OutChannelMap)
	{
		if (NumSourceChannels == 1)
		{
			return ComputeMonoChannelMap(OutChannelMap);
		}
		else if (NumSourceChannels == 2)
		{
			return ComputeStereoChannelMap(OutChannelMap);
		}
		else if (!OutChannelMap.Num())
		{
			MixerDevice->Get2DChannelMap(bIsVorbis, NumSourceChannels, WaveInstance->bCenterChannelOnly, OutChannelMap);
			return true;
		}
		return false;
	}

	bool FMixerSource::UseObjectBasedSpatialization() const
	{
		return (Buffer->NumChannels <= MixerDevice->GetCurrentSpatializationPluginInterfaceInfo().MaxChannelsSupportedBySpatializationPlugin &&
				AudioDevice->IsSpatializationPluginEnabled() &&
				WaveInstance->SpatializationMethod == ESoundSpatializationAlgorithm::SPATIALIZATION_HRTF);
	}

	bool FMixerSource::IsUsingObjectBasedSpatialization() const
	{
		bool bIsUsingObjectBaseSpatialization = UseObjectBasedSpatialization();

		if (MixerSourceVoice)
		{
			// If it is currently playing, check whether it actively uses HRTF spatializer.
			// HRTF spatialization cannot be altered on currently playing source. So this handles
			// the case where the source was initialized without HRTF spatialization before HRTF
			// spatialization is enabled. 
			bool bDefaultIfNoSourceId = true;
			bIsUsingObjectBaseSpatialization &= MixerSourceVoice->IsUsingHRTFSpatializer(bDefaultIfNoSourceId);
		}
		return bIsUsingObjectBaseSpatialization;
	}

	bool FMixerSource::UseSpatializationPlugin() const
	{
		return (Buffer->NumChannels <= MixerDevice->GetCurrentSpatializationPluginInterfaceInfo().MaxChannelsSupportedBySpatializationPlugin) &&
			AudioDevice->IsSpatializationPluginEnabled() &&
			WaveInstance->SpatializationPluginSettings != nullptr;
	}

	bool FMixerSource::UseOcclusionPlugin() const
	{
		return (Buffer->NumChannels == 1 || Buffer->NumChannels == 2) &&
			AudioDevice->IsOcclusionPluginEnabled() &&
			WaveInstance->OcclusionPluginSettings != nullptr;
	}

	bool FMixerSource::UseReverbPlugin() const
	{
		return (Buffer->NumChannels == 1 || Buffer->NumChannels == 2) &&
			AudioDevice->IsReverbPluginEnabled() &&
			WaveInstance->ReverbPluginSettings != nullptr;
	}

	bool FMixerSource::UseSourceDataOverridePlugin() const
	{
		return (Buffer->NumChannels == 1 || Buffer->NumChannels == 2) &&
			AudioDevice->IsSourceDataOverridePluginEnabled() &&
			WaveInstance->SourceDataOverridePluginSettings != nullptr;
	}
}
