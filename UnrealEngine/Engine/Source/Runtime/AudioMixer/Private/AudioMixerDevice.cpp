// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioMixerDevice.h"

#include "AudioAnalytics.h"
#include "AudioMixerSource.h"
#include "AudioMixerSourceManager.h"
#include "AudioMixerSourceDecode.h"
#include "AudioMixerSubmix.h"
#include "AudioMixerSourceVoice.h"
#include "AudioPluginUtilities.h"
#include "AudioMixerEffectsManager.h"
#include "DSP/Noise.h"
#include "DSP/SinOsc.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundSubmix.h"
#include "Sound/SoundSubmixSend.h"
#include "SubmixEffects/AudioMixerSubmixEffectEQ.h"
#include "SubmixEffects/AudioMixerSubmixEffectDynamicsProcessor.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "IHeadMountedDisplayModule.h"
#include "Misc/App.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Async/Async.h"
#include "AudioDeviceNotificationSubsystem.h"
#include "Sound/AudioFormatSettings.h"

#if WITH_EDITOR
#include "AudioEditorModule.h"
#endif // WITH_EDITOR


static int32 DisableSubmixEffectEQCvar = 1;
FAutoConsoleVariableRef CVarDisableSubmixEQ(
	TEXT("au.DisableSubmixEffectEQ"),
	DisableSubmixEffectEQCvar,
	TEXT("Disables the eq submix (true by default as of 5.0).\n")
	TEXT("0: Not Disabled, 1: Disabled"),
	ECVF_Default);

static int32 DisableSubmixMutationLockCVar = 0;
FAutoConsoleVariableRef CVarDisableSubmixMutationLock(
	TEXT("au.DisableSubmixMutationLock"),
	DisableSubmixMutationLockCVar,
	TEXT("Disables the submix mutation lock.\n")
	TEXT("0: Not Disabled (Default), 1: Disabled"),
	ECVF_Default);

static int32 DebugGeneratorEnableCVar = 0;
FAutoConsoleVariableRef CVarDebugGeneratorEnable(
	TEXT("au.Debug.Generator"),
	DebugGeneratorEnableCVar,
	TEXT("Enables/disables debug sound generation.\n")
	TEXT("0: Disabled, 1: SinTone, 2: WhiteNoise"),
	ECVF_Default);

static float DebugGeneratorAmpCVar = 0.2f;
FAutoConsoleVariableRef CVarDebugGeneratorAmp(
	TEXT("au.Debug.Generator.Amp"),
	DebugGeneratorAmpCVar,
	TEXT("Sets.\n")
	TEXT("Default: 0.2f"),
	ECVF_Default);

static int32 DebugGeneratorChannelCVar = 0;
FAutoConsoleVariableRef CVarDebugGeneratorChannel(
	TEXT("au.Debug.Generator.Channel"),
	DebugGeneratorChannelCVar,
	TEXT("Sets channel output index of debug audio.  If number provided is above supported number, uses left.\n")
	TEXT("0: Left, 1: Right, etc."),
	ECVF_Default);

static float DebugGeneratorFreqCVar = 440.0f;
FAutoConsoleVariableRef CVarDebugGeneratorFreq(
	TEXT("au.Debug.Generator.Freq"),
	DebugGeneratorFreqCVar,
	TEXT("Sets debug sound generation frequency.\n")
	TEXT("0: Not Disabled, 1: SinTone, 2: WhiteNoise"),
	ECVF_Default);

// Link to "Audio" profiling category
CSV_DECLARE_CATEGORY_MODULE_EXTERN(AUDIOMIXERCORE_API, Audio);

namespace Audio
{
	void FSubmixMap::Add(const FSubmixMap::FObjectId InObjectId, FMixerSubmixPtr InMixerSubmix)
	{
		if (DisableSubmixMutationLockCVar)
		{
			SubmixMap.Add(InObjectId, InMixerSubmix);
		}
		else
		{
			FScopeLock ScopeLock(&MutationLock);
			SubmixMap.Add(InObjectId, InMixerSubmix);
		}
	}

	void FSubmixMap::Iterate(FIterFunc InFunction)
	{
		if (DisableSubmixMutationLockCVar)
		{
			for (const FPair& Pair : SubmixMap)
			{
				InFunction(Pair);
			}
		}
		else
		{
			FScopeLock ScopeLock(&MutationLock);
			for (const FPair& Pair : SubmixMap)
			{
				InFunction(Pair);
			}
		}
	}

	FMixerSubmixPtr FSubmixMap::FindRef(const FSubmixMap::FObjectId InObjectId)
	{
		if (DisableSubmixMutationLockCVar)
		{
			return SubmixMap.FindRef(InObjectId);
		}
		else
		{
			FScopeLock ScopeLock(&MutationLock);
			return SubmixMap.FindRef(InObjectId);
		}
	}

	int32 FSubmixMap::Remove(const FSubmixMap::FObjectId InObjectId)
	{
		if (DisableSubmixMutationLockCVar)
		{
			return SubmixMap.Remove(InObjectId);
		}
		else
		{
			FScopeLock ScopeLock(&MutationLock);
			return SubmixMap.Remove(InObjectId);
		}
	}

	void FSubmixMap::Reset()
	{
		if (DisableSubmixMutationLockCVar)
		{
			SubmixMap.Reset();
		}
		else
		{
			FScopeLock ScopeLock(&MutationLock);
			SubmixMap.Reset();
		}
	}

	FMixerDevice::FMixerDevice(IAudioMixerPlatformInterface* InAudioMixerPlatform)
		: QuantizedEventClockManager(this)
		, AudioMixerPlatform(InAudioMixerPlatform)
		, AudioClockDelta(0.0)
		, PreviousPrimaryVolume((float)INDEX_NONE)
		, GameOrAudioThreadId(INDEX_NONE)
		, AudioPlatformThreadId(INDEX_NONE)
		, bDebugOutputEnabled(false)
		, bSubmixRegistrationDisabled(true)
	{
		// This audio device is the audio mixer
		bAudioMixerModuleLoaded = true;

		SourceManager = MakeUnique<FMixerSourceManager>(this);

		// Register AudioLink Factory. 	
		TArray<FName> Factories = IAudioLinkFactory::GetAllRegisteredFactoryNames();
		if(Factories.Num() > 0)
		{
			// Allow only a single registered factory instance for now.
			check(Factories.Num()==1);
			AudioLinkFactory=IAudioLinkFactory::FindFactory(Factories[0]);
		}
	}

	FMixerDevice::~FMixerDevice()
	{
		AUDIO_MIXER_CHECK_GAME_THREAD(this);

		// Shutdown all pending clock events, as they may have references to 
		// the FMixerSourceManager that is about to be destroyed
		QuantizedEventClockManager.Shutdown();

		if (AudioMixerPlatform != nullptr)
		{
			delete AudioMixerPlatform;
		}
	}

	void FMixerDevice::AddReferencedObjects(FReferenceCollector& Collector)
	{
	}

	void FMixerDevice::CheckAudioThread() const
	{
#if AUDIO_MIXER_ENABLE_DEBUG_MODE
		// "Audio Thread" is the game/audio thread ID used above audio rendering thread.
		AUDIO_MIXER_CHECK(IsInAudioThread());
#endif
	}

	void FMixerDevice::OnListenerUpdated(const TArray<FListener>& InListeners)
	{
		LLM_SCOPE(ELLMTag::AudioMixer);

		ListenerTransforms.Reset(InListeners.Num());

		for (const FListener& Listener : InListeners)
		{
			ListenerTransforms.Add(Listener.Transform);
		}

		SourceManager->SetListenerTransforms(ListenerTransforms);
	}

	void FMixerDevice::ResetAudioRenderingThreadId()
	{
		AudioPlatformThreadId = INDEX_NONE;
		CheckAudioRenderingThread();
	}

	void FMixerDevice::CheckAudioRenderingThread() const
	{
		if (AudioPlatformThreadId == INDEX_NONE)
		{
			AudioPlatformThreadId = FPlatformTLS::GetCurrentThreadId();
		}
		int32 CurrentThreadId = FPlatformTLS::GetCurrentThreadId();
		AUDIO_MIXER_CHECK(CurrentThreadId == AudioPlatformThreadId);
	}

	bool FMixerDevice::IsAudioRenderingThread() const
	{
		int32 CurrentThreadId = FPlatformTLS::GetCurrentThreadId();
		return CurrentThreadId == AudioPlatformThreadId;
	}

	bool FMixerDevice::IsNonRealtime() const
	{
		return AudioMixerPlatform && AudioMixerPlatform->IsNonRealtime();
	}

	TArray<Audio::FChannelPositionInfo>* FMixerDevice::GetDefaultPositionMap(int32 NumChannels)
	{
		const Audio::FChannelPositionInfo* SpeakerPositions = GetDefaultChannelPositions();

		if (!SpeakerPositions) // speaker maps are not yet initialized
		{
			return nullptr;
		}

		switch (NumChannels)
		{
			// Mono speaker directly in front of listener:
			case 1:
			{
				// force angle on single channel if we are mono
				static TArray<Audio::FChannelPositionInfo> MonoMap = { {EAudioMixerChannel::FrontCenter, 0, 0} };
				return &MonoMap;
			}

			// Stereo speakers to front left and right of listener:
			case 2:
			{
				static TArray<Audio::FChannelPositionInfo> StereoMap = { SpeakerPositions[EAudioMixerChannel::FrontLeft], SpeakerPositions[EAudioMixerChannel::FrontRight] };
				return &StereoMap;
			}

			// Quadrophonic speakers at each corner.
			case 4:
			{
				static TArray<Audio::FChannelPositionInfo> QuadMap = {
														 SpeakerPositions[EAudioMixerChannel::FrontLeft] //left
														,SpeakerPositions[EAudioMixerChannel::FrontRight] // right
														,SpeakerPositions[EAudioMixerChannel::SideLeft] //Left Surround
														,SpeakerPositions[EAudioMixerChannel::SideRight] //Right Surround
				};
				return &QuadMap;
			}

			// 5.1 speakers.
			case 6:
			{
				static TArray<Audio::FChannelPositionInfo> FiveDotOneMap = {
														 SpeakerPositions[EAudioMixerChannel::FrontLeft] //left
														,SpeakerPositions[EAudioMixerChannel::FrontRight] // right
														,SpeakerPositions[EAudioMixerChannel::FrontCenter] //center
														,SpeakerPositions[EAudioMixerChannel::LowFrequency] //LFE
														,SpeakerPositions[EAudioMixerChannel::SideLeft] //Left Rear
														,SpeakerPositions[EAudioMixerChannel::SideRight] //Right Rear
				};
				return &FiveDotOneMap;
			}

			// 7.1 speakers.
			case 8:
			{
				static TArray<Audio::FChannelPositionInfo> SevenDotOneMap = {
														 SpeakerPositions[EAudioMixerChannel::FrontLeft] // left
														,SpeakerPositions[EAudioMixerChannel::FrontRight] // right
														,SpeakerPositions[EAudioMixerChannel::FrontCenter] //center
														,SpeakerPositions[EAudioMixerChannel::LowFrequency] //LFE
														,SpeakerPositions[EAudioMixerChannel::BackLeft] // Left Rear
														,SpeakerPositions[EAudioMixerChannel::BackRight] // Right Rear
														,SpeakerPositions[EAudioMixerChannel::SideLeft] // Left Surround
														,SpeakerPositions[EAudioMixerChannel::SideRight] // Right Surround
				};


				return &SevenDotOneMap;
			}

			case 0:
			default:
			{
				return nullptr;
			}
		}
	}

	bool FMixerDevice::IsEndpointSubmix(const USoundSubmixBase* InSubmix)
	{
		return InSubmix && (InSubmix->IsA<UEndpointSubmix>() || InSubmix->IsA<USoundfieldEndpointSubmix>());
	}

	void FMixerDevice::UpdateDeviceDeltaTime()
	{
		DeviceDeltaTime = GetGameDeltaTime();
	}

	void FMixerDevice::GetAudioDeviceList(TArray<FString>& OutAudioDeviceNames) const
	{
		if (AudioMixerPlatform && AudioMixerPlatform->IsInitialized())
		{
			uint32 NumOutputDevices;
			if (AudioMixerPlatform->GetNumOutputDevices(NumOutputDevices))
			{
				for (uint32 i = 0; i < NumOutputDevices; ++i)
				{
					FAudioPlatformDeviceInfo DeviceInfo;
					if (AudioMixerPlatform->GetOutputDeviceInfo(i, DeviceInfo))
					{
						OutAudioDeviceNames.Add(DeviceInfo.Name);
					}
				}
			}
		}
	}

	bool FMixerDevice::InitializeHardware()
	{
		ensure(IsInGameThread());
	
		LLM_SCOPE(ELLMTag::AudioMixer);


		if (AudioMixerPlatform && AudioMixerPlatform->InitializeHardware())
		{
			UE_LOG(LogAudioMixer, Display, TEXT("Initializing audio mixer using platform API: '%s'"), *AudioMixerPlatform->GetPlatformApi());

			const UAudioSettings* AudioSettings = GetDefault<UAudioSettings>();
			MonoChannelUpmixMethod = AudioSettings->MonoChannelUpmixMethod;
			PanningMethod = AudioSettings->PanningMethod;

			// Set whether we're the main audio mixer
			bIsMainAudioMixer = IsMainAudioDevice();

			AUDIO_MIXER_CHECK(SampleRate != 0.0f);

			AudioMixerPlatform->RegisterDeviceChangedListener();

			// Allow platforms to override the platform settings callback buffer frame size (i.e. restrict to particular values, etc)
			PlatformSettings.CallbackBufferFrameSize = AudioMixerPlatform->GetNumFrames(PlatformSettings.CallbackBufferFrameSize);

			OpenStreamParams.NumBuffers = PlatformSettings.NumBuffers;
			OpenStreamParams.NumFrames = PlatformSettings.CallbackBufferFrameSize;
			OpenStreamParams.OutputDeviceIndex = AUDIO_MIXER_DEFAULT_DEVICE_INDEX; // TODO: Support overriding which audio device user wants to open, not necessarily default.
			OpenStreamParams.SampleRate = SampleRate;
			OpenStreamParams.AudioMixer = this;
			OpenStreamParams.MaxSources = GetMaxSources();

			FString DefaultDeviceName = AudioMixerPlatform->GetDefaultDeviceName();

			// Allow HMD to specify audio device, if one was not specified in settings
			if (DefaultDeviceName.IsEmpty() && FAudioDevice::CanUseVRAudioDevice() && IHeadMountedDisplayModule::IsAvailable())
			{
				DefaultDeviceName = IHeadMountedDisplayModule::Get().GetAudioOutputDevice();
			}

			if (!DefaultDeviceName.IsEmpty())
			{
				uint32 NumOutputDevices = 0;
				AudioMixerPlatform->GetNumOutputDevices(NumOutputDevices);

				for (uint32 i = 0; i < NumOutputDevices; ++i)
				{
					FAudioPlatformDeviceInfo DeviceInfo;
					AudioMixerPlatform->GetOutputDeviceInfo(i, DeviceInfo);

					if (DeviceInfo.Name == DefaultDeviceName || DeviceInfo.DeviceId == DefaultDeviceName)
					{
						OpenStreamParams.OutputDeviceIndex = i;

						// If we're intentionally selecting an audio device (and not just using the default device) then 
						// lets try to restore audio to that device if it's removed and then later is restored
						OpenStreamParams.bRestoreIfRemoved = true;
						break;
					}
				}
			}

			if (AudioMixerPlatform->OpenAudioStream(OpenStreamParams))
			{
				// Get the platform device info we're using
				PlatformInfo = AudioMixerPlatform->GetPlatformDeviceInfo();
				UE_LOG(LogAudioMixer, Display, TEXT("Using Audio Hardware Device %s"), *PlatformInfo.Name);

				// Initialize some data that depends on speaker configuration, etc.
				InitializeChannelAzimuthMap(PlatformInfo.NumChannels);

				FSourceManagerInitParams SourceManagerInitParams;
				SourceManagerInitParams.NumSources = GetMaxSources();

				// TODO: Migrate this to project settings properly
				SourceManagerInitParams.NumSourceWorkers = 4;


				AudioClock = 0.0;
				AudioClockDelta = (double)OpenStreamParams.NumFrames / OpenStreamParams.SampleRate;


				PluginInitializationParams.NumSources = SourceManagerInitParams.NumSources;
				PluginInitializationParams.SampleRate = SampleRate;
				PluginInitializationParams.BufferLength = OpenStreamParams.NumFrames;
				PluginInitializationParams.AudioDevicePtr = this;

				{
					LLM_SCOPE(ELLMTag::AudioMixerPlugins);

					// Initialize any plugins if they exist
					// spatialization
					SetCurrentSpatializationPlugin(AudioPluginUtilities::GetDesiredSpatializationPluginName());

					if (OcclusionInterface.IsValid())
					{
						OcclusionInterface->Initialize(PluginInitializationParams);
					}

					if (ReverbPluginInterface.IsValid())
					{
						ReverbPluginInterface->Initialize(PluginInitializationParams);
					}

					if (SourceDataOverridePluginInterface.IsValid())
 					{
 						SourceDataOverridePluginInterface->Initialize(PluginInitializationParams);
 					}
				}

				// initialize the source manager after our plugins are spun up (cached by sources)
				SourceManager->Init(SourceManagerInitParams);

				// Need to set these up before we start the audio stream.
				InitSoundSubmixes();

				AudioMixerPlatform->PostInitializeHardware();

				// Initialize the data used for audio thread sub-frame timing.
				AudioThreadTimingData.StartTime = FPlatformTime::Seconds();
				AudioThreadTimingData.AudioThreadTime = 0.0;
				AudioThreadTimingData.AudioRenderThreadTime = 0.0;

				// Create synchronized Audio Task Queue for this device...
				CreateSynchronizedAudioTaskQueue((Audio::AudioTaskQueueId)DeviceID);

				Audio::Analytics::RecordEvent_Usage(TEXT("ProjectSettings"), MakeAnalyticsEventAttributeArray(
					TEXT("SampleRate"), PlatformSettings.SampleRate,
					TEXT("BufferSize"), PlatformSettings.CallbackBufferFrameSize,
					TEXT("NumBuffers"), PlatformSettings.NumBuffers,
					TEXT("NumSources"), PlatformSettings.MaxChannels,
					TEXT("NumOutputChannels"), PlatformInfo.NumChannels));

				// Start streaming audio
				return AudioMixerPlatform->StartAudioStream();
			}
		}
		else if (AudioMixerPlatform)
		{
			UE_LOG(LogAudioMixer, Warning, TEXT("Failed to initialize audio mixer for platform API: '%s'"), *AudioMixerPlatform->GetPlatformApi());
		}

		return false;
	}

	void FMixerDevice::FadeIn()
	{
		AudioMixerPlatform->FadeIn();
	}

	void FMixerDevice::FadeOut()
	{
		// In editor builds, we aren't going to fade out the main audio device.
#if WITH_EDITOR
		if (!IsMainAudioDevice())
#endif
		{
			AudioMixerPlatform->FadeOut();
		}
	}

	void FMixerDevice::TeardownHardware()
	{
		ensure(IsInGameThread());

		if (IsInitialized())
		{
			for (TObjectIterator<USoundSubmix> It; It; ++It)
			{
				UnregisterSoundSubmix(*It);
			}
			
			// Destroy the synchronized Audio Task Queue for this device
			DestroySynchronizedAudioTaskQueue((Audio::AudioTaskQueueId)DeviceID);
		}
		
		// reset all the sound effect presets loaded
#if WITH_EDITOR
		for (TObjectIterator<USoundEffectPreset> It; It; ++It)
		{
			USoundEffectPreset* SoundEffectPreset = *It;
			SoundEffectPreset->Init();
		}
#endif

		if (AudioMixerPlatform)
		{
			SourceManager->Update();

			AudioMixerPlatform->UnregisterDeviceChangedListener();
			AudioMixerPlatform->StopAudioStream();
			AudioMixerPlatform->CloseAudioStream();
			AudioMixerPlatform->TeardownHardware();
		}

		// Reset existing submixes if they exist
		MasterSubmixInstances.Reset();
		Submixes.Reset();
	}

	void FMixerDevice::UpdateHardwareTiming()
	{
		// Get the relative audio thread time (from start of audio engine)
		// Add some jitter delta to account for any audio thread timing jitter.
		const double AudioThreadJitterDelta = AudioClockDelta;
		AudioThreadTimingData.AudioThreadTime = FPlatformTime::Seconds() - AudioThreadTimingData.StartTime + AudioThreadJitterDelta;
	}

	void FMixerDevice::UpdateGameThread()
	{
		LLM_SCOPE(ELLMTag::AudioMixer);

		// Pump our command queue sending commands to the game thread
		PumpGameThreadCommandQueue();
	}

	void FMixerDevice::UpdateHardware()
	{
		LLM_SCOPE(ELLMTag::AudioMixer);

		// If we're in editor, re-query these in case they changed. 
		if (GIsEditor)
		{
			const UAudioSettings* AudioSettings = GetDefault<UAudioSettings>();
			MonoChannelUpmixMethod = AudioSettings->MonoChannelUpmixMethod;
			PanningMethod = AudioSettings->PanningMethod;
		}

		SourceManager->Update();

		AudioMixerPlatform->OnHardwareUpdate();

		if (AudioMixerPlatform->CheckAudioDeviceChange())
		{
			// Get the platform device info we're using
			PlatformInfo = AudioMixerPlatform->GetPlatformDeviceInfo();

			// Initialize some data that depends on speaker configuration, etc.
			InitializeChannelAzimuthMap(PlatformInfo.NumChannels);

			// Update the channel device count in case it changed
			SourceManager->UpdateDeviceChannelCount(PlatformInfo.NumChannels);

			// Reset rendering thread ID to this thread ID so that commands can
			// be flushed. Audio Rendering Thread ID will be reset again in call
			// to FMixerDevice::OnProcessAudio
			ResetAudioRenderingThreadId();

			// Force source manager to incorporate device channel count change.
			FlushAudioRenderingCommands(true /* bPumpSynchronously */);

			if (UAudioDeviceNotificationSubsystem* AudioDeviceNotifSubsystem = UAudioDeviceNotificationSubsystem::Get())
			{
				AudioDeviceNotifSubsystem->OnDeviceSwitched(PlatformInfo.DeviceId);
			}

			// Audio rendering was suspended in CheckAudioDeviceChange if it changed.
			AudioMixerPlatform->ResumePlaybackOnNewDevice();
		}

		// Device must be initialized prior to call as submix graph may not be ready yet otherwise.
		if (IsInitialized())
		{
			// Loop through any envelope-following submixes and perform any broadcasting of envelope data if needed
			TArray<float> SubmixEnvelopeData;
			for (USoundSubmix* SoundSubmix : DelegateBoundSubmixes)
			{
				if (SoundSubmix)
				{
					// Retrieve the submix instance and the envelope data and broadcast on the audio thread.
					Audio::FMixerSubmixWeakPtr SubmixPtr = GetSubmixInstance(SoundSubmix);
					if (SubmixPtr.IsValid())
					{
						FAudioThread::RunCommandOnGameThread([this, SubmixPtr]()
							{
								Audio::FMixerSubmixPtr ThisSubmixPtr = SubmixPtr.Pin();
								if (ThisSubmixPtr.IsValid())
								{
									ThisSubmixPtr->BroadcastDelegates();
								}
							});
					}
				}
			}

			// Check if the background mute changed state and update the submixes which are enabled to do background muting.
			const float CurrentPrimaryVolume = GetPrimaryVolume();
			if (!FMath::IsNearlyEqual(PreviousPrimaryVolume, CurrentPrimaryVolume))
			{
				PreviousPrimaryVolume = CurrentPrimaryVolume;
				bool IsMuted = FMath::IsNearlyZero(CurrentPrimaryVolume);

				for (TObjectIterator<USoundSubmix> It; It; ++It)
				{
					if (*It && It->bMuteWhenBackgrounded)
					{
						FMixerSubmixPtr SubmixInstance = GetSubmixInstance(*It).Pin();
						if (SubmixInstance.IsValid())
						{
							SubmixInstance->SetBackgroundMuted(IsMuted);
						}
					}
				}
			}
		}
	}

	double FMixerDevice::GetAudioTime() const
	{
		return AudioClock;
	}

	FAudioEffectsManager* FMixerDevice::CreateEffectsManager()
	{
		return new FAudioMixerEffectsManager(this);
	}

	FSoundSource* FMixerDevice::CreateSoundSource()
	{
		return new FMixerSource(this);
	}

	FName FMixerDevice::GetRuntimeFormat(const USoundWave* InSoundWave) const
	{
		return InSoundWave->GetRuntimeFormat();
    }

	bool FMixerDevice::HasCompressedAudioInfoClass(USoundWave* InSoundWave)
	{
		check(InSoundWave);
		check(AudioMixerPlatform);
		// Every platform has compressed audio. 
		return true;
	}

	bool FMixerDevice::SupportsRealtimeDecompression() const
	{
		// Every platform supports realtime decompression.
		return true;
	}

	bool FMixerDevice::DisablePCMAudioCaching() const
	{
		return AudioMixerPlatform->DisablePCMAudioCaching();
	}
	
	ICompressedAudioInfo* FMixerDevice::CreateAudioInfo(FName InFormat) const
	{
		if (IAudioInfoFactory* Factory = IAudioInfoFactoryRegistry::Get().Find(InFormat))
		{
			return Factory->Create();
		}
		return nullptr;
	}

	ICompressedAudioInfo* FMixerDevice::CreateCompressedAudioInfo(const USoundWave* InSoundWave) const
	{
		return CreateAudioInfo(GetRuntimeFormat(InSoundWave));
	}

	class ICompressedAudioInfo* FMixerDevice::CreateCompressedAudioInfo(const FSoundWaveProxyPtr& InSoundWaveProxy) const
	{
		return CreateAudioInfo(InSoundWaveProxy->GetRuntimeFormat());
	}

	bool FMixerDevice::ValidateAPICall(const TCHAR* Function, uint32 ErrorCode)
	{
		return false;
	}

	bool FMixerDevice::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
	{
		if (FAudioDevice::Exec(InWorld, Cmd, Ar))
		{
			return true;
		}

		return false;
	}

	void FMixerDevice::CountBytes(FArchive& InArchive)
	{
		FAudioDevice::CountBytes(InArchive);
	}

	bool FMixerDevice::IsExernalBackgroundSoundActive()
	{
		return false;
	}

	void FMixerDevice::ResumeContext()
	{
        AudioMixerPlatform->ResumeContext();
	}

	void FMixerDevice::SuspendContext()
	{
        AudioMixerPlatform->SuspendContext();
	}

	void FMixerDevice::EnableDebugAudioOutput()
	{
		bDebugOutputEnabled = true;
	}

	bool FMixerDevice::OnProcessAudioStream(FAlignedFloatBuffer& Output)
	{
		LLM_SCOPE(ELLMTag::AudioMixer);

		// This function could be called in a task manager, which means the thread ID may change between calls.
		ResetAudioRenderingThreadId();

		// Update the audio render thread time at the head of the render
		AudioThreadTimingData.AudioRenderThreadTime = FPlatformTime::Seconds() - AudioThreadTimingData.StartTime;

		// notify interested parties
		FAudioDeviceRenderInfo RenderInfo;
		RenderInfo.NumFrames = SourceManager->GetNumOutputFrames();
		NotifyAudioDevicePreRender(RenderInfo);

		// Pump the command queue to the audio render thread
		PumpCommandQueue();

		// update the clock manager
		QuantizedEventClockManager.Update(SourceManager->GetNumOutputFrames());

		// Compute the next block of audio in the source manager
		SourceManager->ComputeNextBlockOfSamples();

		FMixerSubmixWeakPtr MasterSubmix = GetMasterSubmix();
		{
			CSV_SCOPED_TIMING_STAT(Audio, Submixes);
			SCOPE_CYCLE_COUNTER(STAT_AudioMixerSubmixes);

			FMixerSubmixPtr MasterSubmixPtr = MasterSubmix.Pin();
			if (MasterSubmixPtr.IsValid())
			{
				// Process the audio output from the master submix
				MasterSubmixPtr->ProcessAudio(Output);
			}
		}

		{
			CSV_SCOPED_TIMING_STAT(Audio, EndpointSubmixes);
			SCOPE_CYCLE_COUNTER(STAT_AudioMixerEndpointSubmixes);

			FScopeLock ScopeLock(&EndpointSubmixesMutationLock);
			for (FMixerSubmixPtr& Submix : DefaultEndpointSubmixes)
			{
				// If this hit, a submix was added to the default submix endpoint array
				// even though it's not an endpoint, or a parent was set on an endpoint submix
				// and it wasn't removed from DefaultEndpointSubmixes.
				ensure(Submix->IsDefaultEndpointSubmix());

				// Any endpoint submixes that don't specify an endpoint
				// are summed into our master output.
				Submix->ProcessAudio(Output);
			}
			
			for (FMixerSubmixPtr& Submix : ExternalEndpointSubmixes)
			{
				// If this hit, a submix was added to the external submix endpoint array
				// even though it's not an endpoint, or a parent was set on an endpoint submix
				// and it wasn't removed from ExternalEndpointSubmixes.
				ensure(Submix->IsExternalEndpointSubmix());

				Submix->ProcessAudioAndSendToEndpoint();
			}
		}

		// Reset stopping sounds and clear their state after submixes have been mixed
		SourceManager->ClearStoppingSounds();

		// Do any debug output performing
		if (bDebugOutputEnabled || DebugGeneratorEnableCVar > 0)
		{
			if (DebugGeneratorEnableCVar < 2)
			{
				SineOscTest(Output);
			}
			else
			{
				WhiteNoiseTest(Output);
			}
		}

		// Update the audio clock
		AudioClock += AudioClockDelta;

		// notify interested parties
		NotifyAudioDevicePostRender(RenderInfo);

		KickQueuedTasks((Audio::AudioTaskQueueId)DeviceID);
		return true;
	}

	void FMixerDevice::OnAudioStreamShutdown()
	{
		// Make sure the source manager pumps any final commands on shutdown. These allow for cleaning up sources, interfacing with plugins, etc.
		// Because we double buffer our command queues, we call this function twice to ensure all commands are successfully pumped.
		SourceManager->PumpCommandQueue();
		SourceManager->PumpCommandQueue();

		// Make sure we force any pending release data to happen on shutdown
		SourceManager->UpdatePendingReleaseData(true);
	}

	void FMixerDevice::LoadMasterSoundSubmix(EMasterSubmixType::Type InType, const FString& InDefaultName, bool bInDefaultMuteWhenBackgrounded, FSoftObjectPath& InObjectPath)
	{
		check(IsInGameThread());

		const int32 MasterSubmixCount = static_cast<int32>(EMasterSubmixType::Type::Count);
		if(MasterSubmixes.Num() < MasterSubmixCount)
		{
			MasterSubmixes.AddZeroed(MasterSubmixCount - MasterSubmixes.Num());
		}

		if (MasterSubmixInstances.Num() < MasterSubmixCount)
		{
			MasterSubmixInstances.AddZeroed(MasterSubmixCount - MasterSubmixInstances.Num());
		}

		const int32 TypeIndex = static_cast<int32>(InType);
		if (USoundSubmix* OldSubmix = MasterSubmixes[TypeIndex])
		{
			// Don't bother swapping if new path is invalid...
			if (!InObjectPath.IsValid())
			{
				return;
			}

			// or is same object already initialized.
			if (InObjectPath.GetAssetPathString() == OldSubmix->GetPathName())
			{
				return;
			}
			OldSubmix->RemoveFromRoot();
			FMixerSubmixPtr OldSubmixPtr = MasterSubmixInstances[TypeIndex];
			if (OldSubmixPtr.IsValid())
			{
				FMixerSubmixPtr ParentSubmixPtr = MasterSubmixInstances[TypeIndex]->GetParentSubmix().Pin();
				if (ParentSubmixPtr.IsValid())
				{
					ParentSubmixPtr->RemoveChildSubmix(MasterSubmixInstances[TypeIndex]);
				}
			}
		}

		// 1. Try loading from Developer Audio Settings
		USoundSubmix* NewSubmix = Cast<USoundSubmix>(InObjectPath.TryLoad());

		// 2. If Unset or not found, fallback to engine asset
		if (!NewSubmix)
		{
			static const FString EngineSubmixDir = TEXT("/Engine/EngineSounds/Submixes");

			InObjectPath = FString::Printf(TEXT("%s/%s.%s"), *EngineSubmixDir, *InDefaultName, *InDefaultName);
			NewSubmix = Cast<USoundSubmix>(InObjectPath.TryLoad());
			UE_LOG(LogAudioMixer, Display, TEXT("Submix unset or invalid in 'AudioSettings': Using engine asset '%s'"),
				*InDefaultName,
				*InObjectPath.GetAssetPathString());
		}

		// 3. If engine version not found, dynamically spawn and post error
		if (!NewSubmix)
		{
			UE_LOG(LogAudioMixer, Error, TEXT("Failed to load submix from engine asset path '%s'. Creating '%s' as a stub."),
				*InObjectPath.GetAssetPathString(),
				*InDefaultName);

			NewSubmix = NewObject<USoundSubmix>(USoundSubmix::StaticClass(), *InDefaultName);
			// Make the master reverb mute when backgrounded
			NewSubmix->bMuteWhenBackgrounded = bInDefaultMuteWhenBackgrounded;
		}

		check(NewSubmix);
		NewSubmix->AddToRoot();

		// If sharing submix with other explicitly defined MasterSubmix, create
		// shared pointer directed to already existing submix instance. Otherwise,
		// create a new version.
		FMixerSubmixPtr NewMixerSubmix = GetMasterSubmixInstance(NewSubmix);
		if (!NewMixerSubmix.IsValid())
		{
			UE_LOG(LogAudioMixer, Display, TEXT("Creating Master Submix '%s'"), *NewSubmix->GetName());
			NewMixerSubmix = MakeShared<FMixerSubmix, ESPMode::ThreadSafe>(this);
		}

		// Ensure that master submixes are ONLY tracked in master submix array.
		// MasterSubmixes array can share instances, but should not be duplicated in Submixes Map.
		if (Submixes.Remove(NewSubmix->GetUniqueID()) > 0)
		{
			UE_LOG(LogAudioMixer, Display, TEXT("Submix '%s' has been promoted to master array."), *NewSubmix->GetName());
		}

		// Update/add new submix and instance to respective master arrays
		MasterSubmixes[TypeIndex] = NewSubmix;
		MasterSubmixInstances[TypeIndex] = NewMixerSubmix;

		//Note: If we support using endpoint/soundfield submixes as a master submix in the future, we will need to call NewMixerSubmix->SetSoundfieldFactory here.
		NewMixerSubmix->Init(NewSubmix, false /* bAllowReInit */);
	}

	void FMixerDevice::LoadPluginSoundSubmixes()
	{
		check(IsInGameThread());

		if (IsReverbPluginEnabled() && ReverbPluginInterface)
		{
			LLM_SCOPE(ELLMTag::AudioMixerPlugins);
			USoundSubmix* ReverbPluginSubmix = ReverbPluginInterface->GetSubmix();
			check(ReverbPluginSubmix);
			ReverbPluginSubmix->AddToRoot();

			LoadSoundSubmix(*ReverbPluginSubmix);
			InitSoundfieldAndEndpointDataForSubmix(*ReverbPluginSubmix, GetSubmixInstance(ReverbPluginSubmix).Pin(), false);

			// Plugin must provide valid effect to enable reverb
			FSoundEffectSubmixPtr ReverbPluginEffectSubmix = ReverbPluginInterface->GetEffectSubmix();
			

			if (ReverbPluginEffectSubmix.IsValid())
			{
				if (USoundEffectPreset* Preset = ReverbPluginEffectSubmix->GetPreset())
				{
					FMixerSubmixPtr ReverbPluginMixerSubmixPtr = GetSubmixInstance(ReverbPluginSubmix).Pin();
					check(ReverbPluginMixerSubmixPtr.IsValid());

					const TWeakObjectPtr<USoundSubmix> ReverbPluginSubmixPtr = ReverbPluginSubmix;
					FMixerSubmixWeakPtr ReverbPluginMixerSubmixWeakPtr = ReverbPluginMixerSubmixPtr;
					AudioRenderThreadCommand([ReverbPluginMixerSubmixWeakPtr, ReverbPluginSubmixPtr, ReverbPluginEffectSubmix]()
					{
						FMixerSubmixPtr PluginSubmixPtr = ReverbPluginMixerSubmixWeakPtr.Pin();
						if (PluginSubmixPtr.IsValid() && ReverbPluginSubmixPtr.IsValid())
						{
							PluginSubmixPtr->ReplaceSoundEffectSubmix(0, ReverbPluginEffectSubmix);
						}
					});
				}
			}
			else
			{
				UE_LOG(LogAudioMixer, Error, TEXT("Reverb plugin failed to provide valid effect submix.  Plugin audio processing disabled."));
			}
		}
	}

	void FMixerDevice::InitSoundSubmixes()
	{
		if (IsInGameThread())
		{
			bSubmixRegistrationDisabled = true;

			UAudioSettings* AudioSettings = GetMutableDefault<UAudioSettings>();
			check(AudioSettings);

			if (MasterSubmixes.Num() > 0)
			{
				UE_LOG(LogAudioMixer, Display, TEXT("Re-initializing Sound Submixes..."));
			}
			else
			{
				UE_LOG(LogAudioMixer, Display, TEXT("Initializing Sound Submixes..."));
			}

			// 1. Load or reload all sound submixes/instances
			LoadMasterSoundSubmix(EMasterSubmixType::Master, TEXT("MasterSubmixDefault"), false /* DefaultMuteWhenBackgrounded */, AudioSettings->MasterSubmix);

			// BaseDefaultSubmix is an optional master submix type set by project settings
			if (AudioSettings->BaseDefaultSubmix.IsValid())
			{
				LoadMasterSoundSubmix(EMasterSubmixType::BaseDefault, TEXT("BaseDefault"), false /* DefaultMuteWhenBackgrounded */, AudioSettings->BaseDefaultSubmix);
			}

			LoadMasterSoundSubmix(EMasterSubmixType::Reverb, TEXT("MasterReverbSubmixDefault"), true /* DefaultMuteWhenBackgrounded */, AudioSettings->ReverbSubmix);

			if (!DisableSubmixEffectEQCvar)
			{
				LoadMasterSoundSubmix(EMasterSubmixType::EQ, TEXT("MasterEQSubmixDefault"), false /* DefaultMuteWhenBackgrounded */, AudioSettings->EQSubmix);
			}

			LoadPluginSoundSubmixes();

			for (TObjectIterator<USoundSubmixBase> It; It; ++It)
			{
				USoundSubmixBase* SubmixToLoad = *It;
				check(SubmixToLoad);

				if (!IsMasterSubmixType(SubmixToLoad))
				{
					LoadSoundSubmix(*SubmixToLoad);
					InitSoundfieldAndEndpointDataForSubmix(*SubmixToLoad, GetSubmixInstance(SubmixToLoad).Pin(), false);
				}
			}
			bSubmixRegistrationDisabled = false;
		}

		if (!IsInAudioThread())
		{
			DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.InitSoundSubmixes"), STAT_InitSoundSubmixes, STATGROUP_AudioThreadCommands);

			FAudioThread::RunCommandOnAudioThread([this]()
			{
				CSV_SCOPED_TIMING_STAT(Audio, InitSubmix);
				InitSoundSubmixes();
			}, GET_STATID(STAT_InitSoundSubmixes));
			return;
		}

		for (int32 i = 0; i < static_cast<int32>(EMasterSubmixType::Count); ++i)
		{
			if (DisableSubmixEffectEQCvar && i == static_cast<int32>(EMasterSubmixType::EQ))
			{
				continue;
			}

			USoundSubmixBase* SoundSubmix = MasterSubmixes[i];
			if (SoundSubmix && SoundSubmix != MasterSubmixes[static_cast<int32>(EMasterSubmixType::Master)])
			{
				FMixerSubmixPtr& MasterSubmixInstance = MasterSubmixInstances[i];

				RebuildSubmixLinks(*SoundSubmix, MasterSubmixInstance);
			}
		}

		for (TObjectIterator<const USoundSubmixBase> It; It; ++It)
		{
			if (const USoundSubmixBase* SubmixBase = *It)
			{
				if (IsMasterSubmixType(SubmixBase))
				{
					continue;
				}

				FMixerSubmixPtr SubmixPtr = Submixes.FindRef(SubmixBase->GetUniqueID());
				if (SubmixPtr.IsValid())
				{
					RebuildSubmixLinks(*SubmixBase, SubmixPtr);
				}
			}
		}
	}

	void FMixerDevice::RebuildSubmixLinks(const USoundSubmixBase& SoundSubmix, FMixerSubmixPtr& SubmixInstance)
	{
		// Setup up the submix instance's parent and add the submix instance as a child
		FMixerSubmixPtr ParentSubmixInstance;
		if (const USoundSubmixWithParentBase* SubmixWithParent = Cast<const USoundSubmixWithParentBase>(&SoundSubmix))
		{
			if (SubmixWithParent->ParentSubmix)
			{
				ParentSubmixInstance = GetSubmixInstance(SubmixWithParent->ParentSubmix).Pin();
			}
			else
			{
				// If this submix is itself the broadcast submix, set its parent to the master submix
				if (SubmixInstance == MasterSubmixInstances[static_cast<int32>(EMasterSubmixType::BaseDefault)])
				{
					ParentSubmixInstance = GetMasterSubmix().Pin();
				}
				else
				{
					ParentSubmixInstance = GetBaseDefaultSubmix().Pin();
				}
			}
		}

		if (ParentSubmixInstance.IsValid())
		{
			SubmixInstance->SetParentSubmix(ParentSubmixInstance);
			ParentSubmixInstance->AddChildSubmix(SubmixInstance);
		}
	}

 	FAudioPlatformSettings FMixerDevice::GetPlatformSettings() const
 	{
		FAudioPlatformSettings Settings = AudioMixerPlatform->GetPlatformSettings();

		UE_LOG(LogAudioMixer, Display, TEXT("Audio Mixer Platform Settings:"));
		UE_LOG(LogAudioMixer, Display, TEXT("	Sample Rate:						  %d"), Settings.SampleRate);
		UE_LOG(LogAudioMixer, Display, TEXT("	Callback Buffer Frame Size Requested: %d"), Settings.CallbackBufferFrameSize);
		UE_LOG(LogAudioMixer, Display, TEXT("	Callback Buffer Frame Size To Use:	  %d"), AudioMixerPlatform->GetNumFrames(Settings.CallbackBufferFrameSize));
		UE_LOG(LogAudioMixer, Display, TEXT("	Number of buffers to queue:			  %d"), Settings.NumBuffers);
		UE_LOG(LogAudioMixer, Display, TEXT("	Max Channels (voices):				  %d"), Settings.MaxChannels);
		UE_LOG(LogAudioMixer, Display, TEXT("	Number of Async Source Workers:		  %d"), Settings.NumSourceWorkers);

 		return Settings;
 	}

	FMixerSubmixWeakPtr FMixerDevice::GetMasterSubmix()
	{
		return MasterSubmixInstances[EMasterSubmixType::Master];
	}

	FMixerSubmixWeakPtr FMixerDevice::GetBaseDefaultSubmix()
	{
		if (MasterSubmixInstances[EMasterSubmixType::BaseDefault].IsValid())
		{
			return MasterSubmixInstances[EMasterSubmixType::BaseDefault];
		}
		return GetMasterSubmix();
	}

	FMixerSubmixWeakPtr FMixerDevice::GetMasterReverbSubmix()
	{
		return MasterSubmixInstances[EMasterSubmixType::Reverb];
	}

	FMixerSubmixWeakPtr FMixerDevice::GetMasterEQSubmix()
	{
		return MasterSubmixInstances[EMasterSubmixType::EQ];
	}

	void FMixerDevice::AddMasterSubmixEffect(FSoundEffectSubmixPtr SoundEffectSubmix)
	{
		AudioRenderThreadCommand([this, SoundEffectSubmix]()
		{
			MasterSubmixInstances[EMasterSubmixType::Master]->AddSoundEffectSubmix(SoundEffectSubmix);
		});
	}

	void FMixerDevice::RemoveMasterSubmixEffect(uint32 SubmixEffectId)
	{
		AudioRenderThreadCommand([this, SubmixEffectId]()
		{
			MasterSubmixInstances[EMasterSubmixType::Master]->RemoveSoundEffectSubmix(SubmixEffectId);
		});
	}

	void FMixerDevice::ClearMasterSubmixEffects()
	{
		AudioRenderThreadCommand([this]()
		{
			MasterSubmixInstances[EMasterSubmixType::Master]->ClearSoundEffectSubmixes();
		});
	}

	int32 FMixerDevice::AddSubmixEffect(USoundSubmix* InSoundSubmix, FSoundEffectSubmixPtr SoundEffect)
	{
		FMixerSubmixPtr MixerSubmixPtr = GetSubmixInstance(InSoundSubmix).Pin();
		if (MixerSubmixPtr.IsValid())
		{
			int32 NumEffects = MixerSubmixPtr->GetNumEffects();

			AudioRenderThreadCommand([this, MixerSubmixPtr, SoundEffect]()
				{
					MixerSubmixPtr->AddSoundEffectSubmix(SoundEffect);
				});

			return ++NumEffects;
		}
		else
		{
			UE_LOG(LogAudio, Warning, TEXT("Submix instance %s not found."), *InSoundSubmix->GetName());
		}
		return 0;
	}

	void FMixerDevice::RemoveSubmixEffect(USoundSubmix* InSoundSubmix, uint32 SubmixEffectId)
	{
		FMixerSubmixPtr MixerSubmixPtr = GetSubmixInstance(InSoundSubmix).Pin();
		if (MixerSubmixPtr.IsValid())
		{
			AudioRenderThreadCommand([MixerSubmixPtr, SubmixEffectId]()
			{
				MixerSubmixPtr->RemoveSoundEffectSubmix(SubmixEffectId);
			});
		}
	}

	void FMixerDevice::RemoveSubmixEffectAtIndex(USoundSubmix* InSoundSubmix, int32 SubmixChainIndex)
	{
		FMixerSubmixPtr MixerSubmixPtr = GetSubmixInstance(InSoundSubmix).Pin();
		if (MixerSubmixPtr.IsValid())
		{
			AudioRenderThreadCommand([MixerSubmixPtr, SubmixChainIndex]()
			{
				MixerSubmixPtr->RemoveSoundEffectSubmixAtIndex(SubmixChainIndex);
			});
		}
	}

	void FMixerDevice::ReplaceSoundEffectSubmix(USoundSubmix* InSoundSubmix, int32 InSubmixChainIndex, FSoundEffectSubmixPtr SoundEffect)
	{
		FMixerSubmixPtr MixerSubmixPtr = GetSubmixInstance(InSoundSubmix).Pin();
		if (MixerSubmixPtr.IsValid())
		{
			AudioRenderThreadCommand([MixerSubmixPtr, InSubmixChainIndex, SoundEffect]()
			{
				MixerSubmixPtr->ReplaceSoundEffectSubmix(InSubmixChainIndex, SoundEffect);
			});
		}
	}

	void FMixerDevice::ClearSubmixEffects(USoundSubmix* InSoundSubmix)
	{
		FMixerSubmixPtr MixerSubmixPtr = GetSubmixInstance(InSoundSubmix).Pin();
		if (MixerSubmixPtr.IsValid())
		{
			AudioRenderThreadCommand([MixerSubmixPtr]()
			{
				MixerSubmixPtr->ClearSoundEffectSubmixes();
			});
		}
	}

	void FMixerDevice::SetSubmixEffectChainOverride(USoundSubmix* InSoundSubmix, const TArray<FSoundEffectSubmixPtr>& InSubmixEffectPresetChain, float InFadeTimeSec)
	{
		FMixerSubmixPtr MixerSubmixPtr = GetSubmixInstance(InSoundSubmix).Pin();
		if (MixerSubmixPtr.IsValid())
		{
			AudioRenderThreadCommand([MixerSubmixPtr, InSubmixEffectPresetChain, InFadeTimeSec]()
			{
				MixerSubmixPtr->SetSubmixEffectChainOverride(InSubmixEffectPresetChain, InFadeTimeSec);
			});
		}
	}

	void FMixerDevice::ClearSubmixEffectChainOverride(USoundSubmix* InSoundSubmix, float InFadeTimeSec)
	{
		FMixerSubmixPtr MixerSubmixPtr = GetSubmixInstance(InSoundSubmix).Pin();
		if (MixerSubmixPtr.IsValid())
		{
			AudioRenderThreadCommand([MixerSubmixPtr, InFadeTimeSec]()
			{
				MixerSubmixPtr->ClearSubmixEffectChainOverride(InFadeTimeSec);
			});
		}
	}

	void FMixerDevice::UpdateSourceEffectChain(const uint32 SourceEffectChainId, const TArray<FSourceEffectChainEntry>& SourceEffectChain, const bool bPlayEffectChainTails)
	{
		TArray<FSourceEffectChainEntry>* ExistingOverride = SourceEffectChainOverrides.Find(SourceEffectChainId);
		if (ExistingOverride)
		{
			*ExistingOverride = SourceEffectChain;
		}
		else
		{
			SourceEffectChainOverrides.Add(SourceEffectChainId, SourceEffectChain);
		}

		SourceManager->UpdateSourceEffectChain(SourceEffectChainId, SourceEffectChain, bPlayEffectChainTails);
	}

	void FMixerDevice::UpdateSubmixProperties(USoundSubmixBase* InSoundSubmix)
	{
		check(InSoundSubmix);

		// Output volume is only supported on USoundSubmixes.
		USoundSubmix* CastedSubmix = Cast<USoundSubmix>(InSoundSubmix);

		if (!CastedSubmix)
		{
			return;
		}

#if WITH_EDITOR
		check(IsInAudioThread());

		FMixerSubmixPtr MixerSubmix = GetSubmixInstance(InSoundSubmix).Pin();
		if (MixerSubmix.IsValid())
		{
			const float NewVolume = CastedSubmix->OutputVolume;
			AudioRenderThreadCommand([MixerSubmix, NewVolume]()
			{
				MixerSubmix->SetOutputVolume(NewVolume);
			});
		}
#endif // WITH_EDITOR
	}

	void FMixerDevice::SetSubmixWetDryLevel(USoundSubmix* InSoundSubmix, float InOutputVolume, float InWetLevel, float InDryLevel)
	{
		if (!IsInAudioThread())
		{
			FMixerDevice* MixerDevice = this;
			FAudioThread::RunCommandOnAudioThread([MixerDevice, InSoundSubmix, InOutputVolume, InWetLevel, InDryLevel]()
			{
				MixerDevice->SetSubmixWetDryLevel(InSoundSubmix, InOutputVolume, InWetLevel, InDryLevel);
			});
			return;
		}

		FMixerSubmixPtr MixerSubmixPtr = GetSubmixInstance(InSoundSubmix).Pin();
		if (MixerSubmixPtr.IsValid())
		{
			AudioRenderThreadCommand([MixerSubmixPtr, InOutputVolume, InWetLevel, InDryLevel]()
			{
				MixerSubmixPtr->SetOutputVolume(InOutputVolume);
				MixerSubmixPtr->SetWetLevel(InWetLevel);
				MixerSubmixPtr->SetDryLevel(InDryLevel);
			});
		}
	}

	void FMixerDevice::SetSubmixOutputVolume(USoundSubmix* InSoundSubmix, float InOutputVolume)
	{
		if (!IsInAudioThread())
		{
			FMixerDevice* MixerDevice = this;
			FAudioThread::RunCommandOnAudioThread([MixerDevice, InSoundSubmix, InOutputVolume]()
			{
				MixerDevice->SetSubmixOutputVolume(InSoundSubmix, InOutputVolume);
			});
			return;
		}

		FMixerSubmixPtr MixerSubmixPtr = GetSubmixInstance(InSoundSubmix).Pin();
		if (MixerSubmixPtr.IsValid())
		{
			AudioRenderThreadCommand([MixerSubmixPtr, InOutputVolume]()
			{
				MixerSubmixPtr->SetOutputVolume(InOutputVolume);
			});
		}
	}

	void FMixerDevice::SetSubmixWetLevel(USoundSubmix* InSoundSubmix, float InWetLevel)
	{
		if (!IsInAudioThread())
		{
			FMixerDevice* MixerDevice = this;
			FAudioThread::RunCommandOnAudioThread([MixerDevice, InSoundSubmix, InWetLevel]()
			{
				MixerDevice->SetSubmixWetLevel(InSoundSubmix, InWetLevel);
			});
			return;
		}

		FMixerSubmixPtr MixerSubmixPtr = GetSubmixInstance(InSoundSubmix).Pin();
		if (MixerSubmixPtr.IsValid())
		{
			AudioRenderThreadCommand([MixerSubmixPtr, InWetLevel]()
			{
				MixerSubmixPtr->SetWetLevel(InWetLevel);
			});
		}
	}

	void FMixerDevice::SetSubmixDryLevel(USoundSubmix* InSoundSubmix, float InDryLevel)
	{
		if (!IsInAudioThread())
		{
			FMixerDevice* MixerDevice = this;
			FAudioThread::RunCommandOnAudioThread([MixerDevice, InSoundSubmix, InDryLevel]()
			{
				MixerDevice->SetSubmixDryLevel(InSoundSubmix, InDryLevel);
			});
			return;
		}

		FMixerSubmixPtr MixerSubmixPtr = GetSubmixInstance(InSoundSubmix).Pin();
		if (MixerSubmixPtr.IsValid())
		{
			AudioRenderThreadCommand([MixerSubmixPtr, InDryLevel]()
			{
				MixerSubmixPtr->SetDryLevel(InDryLevel);
			});
		}
	}

	void FMixerDevice::SetSubmixAutoDisable(USoundSubmix* InSoundSubmix, bool bInAutoDisable)
	{
		if (!IsInAudioThread())
		{
			FMixerDevice* MixerDevice = this;
			FAudioThread::RunCommandOnAudioThread([MixerDevice, InSoundSubmix, bInAutoDisable]()
			{
				MixerDevice->SetSubmixAutoDisable(InSoundSubmix, bInAutoDisable);
			});
			return;
		}

		FMixerSubmixPtr MixerSubmixPtr = GetSubmixInstance(InSoundSubmix).Pin();
		if (MixerSubmixPtr.IsValid())
		{
			AudioRenderThreadCommand([MixerSubmixPtr, bInAutoDisable]()
			{
				MixerSubmixPtr->SetAutoDisable(bInAutoDisable);
			});
		}
	}

	void FMixerDevice::SetSubmixAutoDisableTime(USoundSubmix* InSoundSubmix, float InDisableTime)
	{
		if (!IsInAudioThread())
		{
			FMixerDevice* MixerDevice = this;
			FAudioThread::RunCommandOnAudioThread([MixerDevice, InSoundSubmix, InDisableTime]()
			{
				MixerDevice->SetSubmixAutoDisableTime(InSoundSubmix, InDisableTime);
			});
			return;
		}

		FMixerSubmixPtr MixerSubmixPtr = GetSubmixInstance(InSoundSubmix).Pin();
		if (MixerSubmixPtr.IsValid())
		{
			AudioRenderThreadCommand([MixerSubmixPtr, InDisableTime]()
			{
				MixerSubmixPtr->SetAutoDisableTime(InDisableTime);
			});
		}
	}

	void FMixerDevice::UpdateSubmixModulationSettings(USoundSubmix* InSoundSubmix, const TSet<TObjectPtr<USoundModulatorBase>>& InOutputModulation, const TSet<TObjectPtr<USoundModulatorBase>>& InWetLevelModulation, const TSet<TObjectPtr<USoundModulatorBase>>& InDryLevelModulation)
	{
		if (!IsInAudioThread())
		{
			FMixerDevice* MixerDevice = this;

			FAudioThread::RunCommandOnAudioThread([MixerDevice, InSoundSubmix, OutMod = InOutputModulation, WetMod = InWetLevelModulation, DryMod = InDryLevelModulation]()
			{
				MixerDevice->UpdateSubmixModulationSettings(InSoundSubmix, OutMod, WetMod, DryMod);
			});
			return;
		}

		if (IsModulationPluginEnabled() && ModulationInterface.IsValid())
		{
			FMixerSubmixPtr MixerSubmixPtr = GetSubmixInstance(InSoundSubmix).Pin();

			if (MixerSubmixPtr.IsValid())
			{
				AudioRenderThreadCommand([MixerSubmixPtr, VolumeMod = InOutputModulation, WetMod = InOutputModulation, DryMod = InOutputModulation]()
				{
					MixerSubmixPtr->UpdateModulationSettings(VolumeMod, WetMod, DryMod);
				});
			}
		}
	}

	void FMixerDevice::SetSubmixModulationBaseLevels(USoundSubmix* InSoundSubmix, float InVolumeModBase, float InWetModBase, float InDryModBase)
	{
		if (!IsInAudioThread())
		{
			FMixerDevice* MixerDevice = this;

			FAudioThread::RunCommandOnAudioThread([MixerDevice, InSoundSubmix, InVolumeModBase, InWetModBase, InDryModBase]() 
			{
				MixerDevice->SetSubmixModulationBaseLevels(InSoundSubmix, InVolumeModBase, InWetModBase, InDryModBase);
			});
			return;
		}

		FMixerSubmixPtr MixerSubmixPtr = GetSubmixInstance(InSoundSubmix).Pin();
		if (MixerSubmixPtr.IsValid())
		{
			AudioRenderThreadCommand([MixerSubmixPtr, InVolumeModBase, InWetModBase, InDryModBase]() 
			{
				MixerSubmixPtr->SetModulationBaseLevels(InVolumeModBase, InWetModBase, InDryModBase);
			});
		}
	}

	bool FMixerDevice::GetCurrentSourceEffectChain(const uint32 SourceEffectChainId, TArray<FSourceEffectChainEntry>& OutCurrentSourceEffectChainEntries)
	{
		TArray<FSourceEffectChainEntry>* ExistingOverride = SourceEffectChainOverrides.Find(SourceEffectChainId);
		if (ExistingOverride)
		{
			OutCurrentSourceEffectChainEntries = *ExistingOverride;
			return true;
		}
		return false;
	}

	void FMixerDevice::AudioRenderThreadCommand(TFunction<void()> Command)
	{
		CommandQueue.Enqueue(MoveTemp(Command));
	}

	void FMixerDevice::GameThreadMPSCCommand(TFunction<void()> InCommand)
	{
		GameThreadCommandQueue.Enqueue(MoveTemp(InCommand));
	}
	
	void FMixerDevice::PumpCommandQueue()
	{
		// Execute the pushed lambda functions
		TFunction<void()> Command;
		while (CommandQueue.Dequeue(Command))
		{
			Command();
		}
	}

	void FMixerDevice::PumpGameThreadCommandQueue()
	{
		TOptional Opt { GameThreadCommandQueue.Dequeue() };
		while (Opt.IsSet())
		{
			TFunction<void()> Command = MoveTemp(Opt.GetValue());
			Command();
				
			Opt = GameThreadCommandQueue.Dequeue();
		}
	}
	
	void FMixerDevice::FlushAudioRenderingCommands(bool bPumpSynchronously)
	{
		if (IsInitialized() && (FPlatformProcess::SupportsMultithreading() && !AudioMixerPlatform->IsNonRealtime()))
		{
			SourceManager->FlushCommandQueue(bPumpSynchronously);
		}
		else if (AudioMixerPlatform->IsNonRealtime())
		{
			SourceManager->FlushCommandQueue(true);
		}
		else
		{
			// Pump the audio device's command queue
			PumpCommandQueue();

			// And also directly pump the source manager command queue
			SourceManager->PumpCommandQueue();
			SourceManager->PumpCommandQueue();

			SourceManager->UpdatePendingReleaseData(true);
		}
	}

	bool FMixerDevice::IsMasterSubmixType(const USoundSubmixBase* InSubmix) const
	{
		for (int32 i = 0; i < EMasterSubmixType::Count; ++i)
		{
			if (InSubmix == MasterSubmixes[i])
			{
				return true;
			}
		}
		return false;
	}

	FMixerSubmixPtr FMixerDevice::GetMasterSubmixInstance(uint32 InObjectId)
	{
		check(MasterSubmixes.Num() == EMasterSubmixType::Count);
		for (int32 i = 0; i < EMasterSubmixType::Count; ++i)
		{
			if (InObjectId == MasterSubmixes[i]->GetUniqueID())
			{
				return MasterSubmixInstances[i];
			}
		}
		return nullptr;
	}

	FMixerSubmixPtr FMixerDevice::GetMasterSubmixInstance(const USoundSubmixBase* InSubmix)
	{
		check(MasterSubmixes.Num() == EMasterSubmixType::Count);
		for (int32 i = 0; i < EMasterSubmixType::Count; ++i)
		{
			if (InSubmix == MasterSubmixes[i])
			{
				return MasterSubmixInstances[i];
			}
		}
		return nullptr;
	}

	void FMixerDevice::RegisterSoundSubmix(USoundSubmixBase* InSoundSubmix, bool bInit)
	{
		if (InSoundSubmix && bSubmixRegistrationDisabled)
		{
			UE_LOG(LogAudioMixer, Warning, TEXT("Attempted register Submix %s before the submix graph was initialized."), *InSoundSubmix->GetFullName());
			return;
		}

		if (!InSoundSubmix)
		{
			return;
		}

		if (!IsInAudioThread())
		{
			DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.RegisterSoundSubmix"), STAT_AudioRegisterSoundSubmix, STATGROUP_AudioThreadCommands);

			FMixerDevice* MixerDevice = this;
			FAudioThread::RunCommandOnAudioThread([MixerDevice, InSoundSubmix, bInit]()
			{
				CSV_SCOPED_TIMING_STAT(Audio, RegisterSubmix);
				MixerDevice->RegisterSoundSubmix(InSoundSubmix, bInit);
			}, GET_STATID(STAT_AudioRegisterSoundSubmix));
			return;
		}

		UE_LOG(LogAudioMixer, Display, TEXT("Registering submix %s."), *InSoundSubmix->GetFullName());

		const bool bIsMasterSubmix = IsMasterSubmixType(InSoundSubmix);

		if (!bIsMasterSubmix)
		{
			// Ensure parent structure is registered prior to current submix if missing
			const USoundSubmixWithParentBase* SubmixWithParent = Cast<const USoundSubmixWithParentBase>(InSoundSubmix);
			if (SubmixWithParent && SubmixWithParent->ParentSubmix)
			{
				FMixerSubmixPtr ParentSubmix = GetSubmixInstance(SubmixWithParent->ParentSubmix).Pin();
				if (!ParentSubmix.IsValid())
				{
					RegisterSoundSubmix(SubmixWithParent->ParentSubmix, bInit);
				}
			}

			LoadSoundSubmix(*InSoundSubmix);
		}
		else
		{
			UE_LOG(LogAudioMixer, Display, TEXT("Submix %s was already registered as one of the master submixes."), *InSoundSubmix->GetFullName());
		}

		FMixerSubmixPtr SubmixPtr = GetSubmixInstance(InSoundSubmix).Pin();

		if (bInit)
		{
			InitSoundfieldAndEndpointDataForSubmix(*InSoundSubmix, SubmixPtr, true);
		}

		if (!bIsMasterSubmix)
		{
			RebuildSubmixLinks(*InSoundSubmix, SubmixPtr);
		}
	}

	void FMixerDevice::LoadSoundSubmix(USoundSubmixBase& InSoundSubmix)
	{
		// If submix not already found, load it.
		FMixerSubmixPtr MixerSubmix = GetSubmixInstance(&InSoundSubmix).Pin();
		if (!MixerSubmix.IsValid())
		{
			InSoundSubmix.AddToRoot();

			MixerSubmix = MakeShared<FMixerSubmix, ESPMode::ThreadSafe>(this);
			Submixes.Add(InSoundSubmix.GetUniqueID(), MixerSubmix);
		}
	}

	void FMixerDevice::InitSoundfieldAndEndpointDataForSubmix(const USoundSubmixBase& InSoundSubmix, FMixerSubmixPtr MixerSubmix, bool bAllowReInit)
	{
		{
			FScopeLock ScopeLock(&EndpointSubmixesMutationLock);

			// Check to see if this is an endpoint or soundfield submix:
			if (const USoundfieldSubmix* SoundfieldSubmix = Cast<const USoundfieldSubmix>(&InSoundSubmix))
			{
				MixerSubmix->SetSoundfieldFactory(SoundfieldSubmix->GetSoundfieldFactoryForSubmix());
			}
			else if (const USoundfieldEndpointSubmix* SoundfieldEndpointSubmix = Cast<const USoundfieldEndpointSubmix>(&InSoundSubmix))
			{
				MixerSubmix->SetSoundfieldFactory(SoundfieldEndpointSubmix->GetSoundfieldEndpointForSubmix());
			}

			if (DefaultEndpointSubmixes.Contains(MixerSubmix))
			{
				DefaultEndpointSubmixes.RemoveSwap(MixerSubmix);
			}

			if (ExternalEndpointSubmixes.Contains(MixerSubmix))
			{
				ExternalEndpointSubmixes.RemoveSwap(MixerSubmix);
			}

			MixerSubmix->Init(&InSoundSubmix, bAllowReInit);

			if (IsEndpointSubmix(&InSoundSubmix) && MixerSubmix->IsDefaultEndpointSubmix())
			{
				DefaultEndpointSubmixes.Add(MixerSubmix);
			}
			else if (MixerSubmix->IsExternalEndpointSubmix())
			{
				ExternalEndpointSubmixes.Add(MixerSubmix);
			}
		}
	}

	void FMixerDevice::UnregisterSoundSubmix(const USoundSubmixBase* InSoundSubmix)
	{
		if (!InSoundSubmix || bSubmixRegistrationDisabled || IsMasterSubmixType(InSoundSubmix))
		{
			return;
		}

		if (!IsInAudioThread())
		{
			DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.UnregisterSoundSubmix"), STAT_AudioUnregisterSoundSubmix, STATGROUP_AudioThreadCommands);

			const TWeakObjectPtr<const USoundSubmixBase> SubmixToUnload = InSoundSubmix;
			FAudioThread::RunCommandOnAudioThread([this, SubmixToUnload]()
			{
				CSV_SCOPED_TIMING_STAT(Audio, UnregisterSubmix);
				if (SubmixToUnload.IsValid())
				{
					UnloadSoundSubmix(*SubmixToUnload.Get());
				}
			}, GET_STATID(STAT_AudioUnregisterSoundSubmix));
			return;
		}

		UnloadSoundSubmix(*InSoundSubmix);
	}

	void FMixerDevice::UnloadSoundSubmix(const USoundSubmixBase& InSoundSubmix)
	{
		check(IsInAudioThread());

		FMixerSubmixWeakPtr MasterSubmix = GetMasterSubmix();

		// Check if this is a submix type that has a parent.
		FMixerSubmixPtr ParentSubmixInstance;
		if (const USoundSubmixWithParentBase* InSoundSubmixWithParent = Cast<const USoundSubmixWithParentBase>(&InSoundSubmix))
		{
			ParentSubmixInstance = InSoundSubmixWithParent->ParentSubmix
				? GetSubmixInstance(InSoundSubmixWithParent->ParentSubmix).Pin()
				: MasterSubmix.Pin();
		}

		if (ParentSubmixInstance.IsValid())
		{
			ParentSubmixInstance->RemoveChildSubmix(GetSubmixInstance(&InSoundSubmix));
		}

		for (USoundSubmixBase* ChildSubmix : InSoundSubmix.ChildSubmixes)
		{
			FMixerSubmixPtr ChildSubmixPtr = GetSubmixInstance(ChildSubmix).Pin();
			if (ChildSubmixPtr.IsValid())
			{
				ChildSubmixPtr->SetParentSubmix(ParentSubmixInstance.IsValid()
					? ParentSubmixInstance
					: MasterSubmix);
			}
		}

		FMixerSubmixWeakPtr MixerSubmixWeakPtr = GetSubmixInstance(&InSoundSubmix);
		FMixerSubmixPtr MixerSubmix = MixerSubmixWeakPtr.Pin();

		if (MixerSubmix && MixerSubmix->IsDefaultEndpointSubmix())
		{
			FScopeLock ScopeLock(&EndpointSubmixesMutationLock);
			DefaultEndpointSubmixes.Remove(MixerSubmix);
		}
		else if (MixerSubmix && MixerSubmix->IsExternalEndpointSubmix())
		{
			FScopeLock ScopeLock(&EndpointSubmixesMutationLock);
			ExternalEndpointSubmixes.Remove(MixerSubmix);
		}

		Submixes.Remove(InSoundSubmix.GetUniqueID());
	}

	void FMixerDevice::InitSoundEffectPresets()
	{
#if WITH_EDITOR
		IAudioEditorModule* AudioEditorModule = &FModuleManager::LoadModuleChecked<IAudioEditorModule>("AudioEditor");
		AudioEditorModule->RegisterEffectPresetAssetActions();
#endif
	}

	FMixerSubmixPtr FMixerDevice::FindSubmixInstanceByObjectId(uint32 InObjectId)
	{
		for (int32 i = 0; i < MasterSubmixes.Num(); i++)
		{
			if (const USoundSubmix* MasterSubmix = MasterSubmixes[i])
			{
				if (MasterSubmix->GetUniqueID() == InObjectId)
				{
					return GetMasterSubmixInstance(MasterSubmix);
				}
			}
			else
			{
				const EMasterSubmixType::Type SubmixType = static_cast<EMasterSubmixType::Type>(i);
				ensureAlwaysMsgf(EMasterSubmixType::Master != SubmixType,
					TEXT("Top-level master has to be registered before anything else, and is required for the lifetime of the application.")
				);

				if (!DisableSubmixEffectEQCvar && EMasterSubmixType::EQ == SubmixType)
				{
					UE_LOG(LogAudioMixer, Warning, TEXT("Failed to query EQ Submix when it was expected to be loaded."));
				}
			}
		}

		return Submixes.FindRef(InObjectId);
	}

	void FMixerDevice::InitDefaultAudioBuses()
	{
		if (!ensure(IsInGameThread()))
		{
			return;
		}

		if (const UAudioSettings* AudioSettings = GetDefault<UAudioSettings>())
		{
			TArray<TStrongObjectPtr<UAudioBus>> StaleBuses = DefaultAudioBuses;
			DefaultAudioBuses.Reset();

			for (const FDefaultAudioBusSettings& BusSettings : AudioSettings->DefaultAudioBuses)
			{
				if (UObject* BusObject = BusSettings.AudioBus.TryLoad())
				{
					if (UAudioBus* AudioBus = Cast<UAudioBus>(BusObject))
					{
						const int32 NumChannels = static_cast<int32>(AudioBus->AudioBusChannels) + 1;
						StartAudioBus(AudioBus->GetUniqueID(), NumChannels, false /* bInIsAutomatic */);

						TStrongObjectPtr<UAudioBus>AddedBus(AudioBus);
						DefaultAudioBuses.AddUnique(AddedBus);
						StaleBuses.Remove(AddedBus);
					}
				}
			}

			for (TStrongObjectPtr<UAudioBus>& Bus : StaleBuses)
			{
				if (Bus.IsValid())
				{
					StopAudioBus(Bus->GetUniqueID());
				}
			}
		}
		else
		{
			UE_LOG(LogAudioMixer, Error, TEXT("Failed to initialized Default Audio Buses. Audio Settings not found."));
		}
	}

	void FMixerDevice::ShutdownDefaultAudioBuses()
	{
		if (!ensure(IsInGameThread()))
		{
			return;
		}

		for (TObjectIterator<UAudioBus> It; It; ++It)
		{
			UAudioBus* AudioBus = *It;
			if (AudioBus)
			{
				StopAudioBus(AudioBus->GetUniqueID());
			}
		}

		DefaultAudioBuses.Reset();
	}

	FMixerSubmixWeakPtr FMixerDevice::GetSubmixInstance(const USoundSubmixBase* SoundSubmix)
	{
		LLM_SCOPE(ELLMTag::AudioMixer);

		FMixerSubmixPtr MixerSubmix = GetMasterSubmixInstance(SoundSubmix);
		if (MixerSubmix.IsValid())
		{
			return MixerSubmix;
		}
		
		if (SoundSubmix)
		{
			return Submixes.FindRef(SoundSubmix->GetUniqueID());
		}

		return nullptr;
	}

	ISoundfieldFactory* FMixerDevice::GetFactoryForSubmixInstance(USoundSubmix* SoundSubmix)
	{
		FMixerSubmixWeakPtr WeakSubmixPtr = GetSubmixInstance(SoundSubmix);
		return GetFactoryForSubmixInstance(WeakSubmixPtr);
	}

	ISoundfieldFactory* FMixerDevice::GetFactoryForSubmixInstance(FMixerSubmixWeakPtr& SoundSubmixPtr)
	{
		FMixerSubmixPtr SubmixPtr = SoundSubmixPtr.Pin();
		if (ensure(SubmixPtr.IsValid()))
		{
			return SubmixPtr->GetSoundfieldFactory();
		}
		else
		{
			return nullptr;
		}
	}

	FMixerSourceVoice* FMixerDevice::GetMixerSourceVoice()
	{
		LLM_SCOPE(ELLMTag::AudioMixer);

		FMixerSourceVoice* Voice = nullptr;
		if (!SourceVoices.Dequeue(Voice))
		{
			Voice = new FMixerSourceVoice();
		}

		Voice->Reset(this);
		return Voice;
	}

	void FMixerDevice::ReleaseMixerSourceVoice(FMixerSourceVoice* InSourceVoice)
	{
		SourceVoices.Enqueue(InSourceVoice);
	}

	int32 FMixerDevice::GetNumSources() const
	{
		return Sources.Num();
	}

	IAudioLinkFactory* FMixerDevice::GetAudioLinkFactory() const
	{
		return AudioLinkFactory;
	}

	int32 FMixerDevice::GetNumActiveSources() const
	{
		return SourceManager->GetNumActiveSources();
	}

	void FMixerDevice::Get3DChannelMap(const int32 InSubmixNumChannels, const FWaveInstance* InWaveInstance, float EmitterAzimith, float NormalizedOmniRadius, Audio::FAlignedFloatBuffer& OutChannelMap)
	{
		// If we're center-channel only, then no need for spatial calculations, but need to build a channel map
		if (InWaveInstance->bCenterChannelOnly)
		{
			int32 NumOutputChannels = InSubmixNumChannels;
			const TArray<EAudioMixerChannel::Type>& ChannelArray = GetChannelArray();

			// If we are only spatializing to stereo output
			if (NumOutputChannels == 2)
			{
				// Equal volume in left + right channel with equal power panning
				static const float Pan = 1.0f / FMath::Sqrt(2.0f);
				OutChannelMap.Add(Pan);
				OutChannelMap.Add(Pan);
			}
			else
			{
				for (EAudioMixerChannel::Type Channel : ChannelArray)
				{
					float Pan = (Channel == EAudioMixerChannel::FrontCenter) ? 1.0f : 0.0f;
					OutChannelMap.Add(Pan);
				}
			}

			return;
		}

		float Azimuth = EmitterAzimith;

		const FChannelPositionInfo* PrevChannelInfo = nullptr;
		const FChannelPositionInfo* NextChannelInfo = nullptr;

		for (int32 i = 0; i < DeviceChannelAzimuthPositions.Num(); ++i)
		{
			const FChannelPositionInfo& ChannelPositionInfo = DeviceChannelAzimuthPositions[i];

			if (Azimuth <= ChannelPositionInfo.Azimuth)
			{
				NextChannelInfo = &DeviceChannelAzimuthPositions[i];

				int32 PrevIndex = i - 1;
				if (PrevIndex < 0)
				{
					PrevIndex = DeviceChannelAzimuthPositions.Num() - 1;
				}

				PrevChannelInfo = &DeviceChannelAzimuthPositions[PrevIndex];
				break;
			}
		}

		// If we didn't find anything, that means our azimuth position is at the top of the mapping
		if (PrevChannelInfo == nullptr)
		{
			PrevChannelInfo = &DeviceChannelAzimuthPositions[DeviceChannelAzimuthPositions.Num() - 1];
			NextChannelInfo = &DeviceChannelAzimuthPositions[0];
			AUDIO_MIXER_CHECK(PrevChannelInfo != NextChannelInfo);
		}

		float NextChannelAzimuth = NextChannelInfo->Azimuth;
		float PrevChannelAzimuth = PrevChannelInfo->Azimuth;

		if (NextChannelAzimuth < PrevChannelAzimuth)
		{
			NextChannelAzimuth += 360.0f;
		}

		if (Azimuth < PrevChannelAzimuth)
		{
			Azimuth += 360.0f;
		}

		AUDIO_MIXER_CHECK(NextChannelAzimuth > PrevChannelAzimuth);
		AUDIO_MIXER_CHECK(Azimuth > PrevChannelAzimuth);
		float Fraction = (Azimuth - PrevChannelAzimuth) / (NextChannelAzimuth - PrevChannelAzimuth);
		AUDIO_MIXER_CHECK(Fraction >= 0.0f && Fraction <= 1.0f);

		// Compute the panning values using equal-power panning law
		float PrevChannelPan; 
		float NextChannelPan;

		if (PanningMethod == EPanningMethod::EqualPower)
		{
			FMath::SinCos(&NextChannelPan, &PrevChannelPan, Fraction * 0.5f * PI);

			// Note that SinCos can return values slightly greater than 1.0 when very close to PI/2
			NextChannelPan = FMath::Clamp(NextChannelPan, 0.0f, 1.0f);
			PrevChannelPan = FMath::Clamp(PrevChannelPan, 0.0f, 1.0f);
		}
		else
		{
			NextChannelPan = Fraction;
			PrevChannelPan = 1.0f - Fraction;
		}

		float NormalizedOmniRadSquared = NormalizedOmniRadius * NormalizedOmniRadius;
		float OmniAmount = 0.0f;

		if (NormalizedOmniRadSquared > 1.0f)
		{
			OmniAmount = 1.0f - 1.0f / NormalizedOmniRadSquared;
		}

		// Build the output channel map based on the current platform device output channel array 

		int32 NumSpatialChannels = DeviceChannelAzimuthPositions.Num();
		if (DeviceChannelAzimuthPositions.Num() > 4)
		{
			NumSpatialChannels--;
		}
		float OmniPanFactor = 1.0f / NumSpatialChannels;

		float DefaultEffectivePan = !OmniAmount ? 0.0f : FMath::Lerp(0.0f, OmniPanFactor, OmniAmount);
		const TArray<EAudioMixerChannel::Type>& ChannelArray = GetChannelArray();

		for (EAudioMixerChannel::Type Channel : ChannelArray)
		{
			float EffectivePan = DefaultEffectivePan;

			// Check for manual channel mapping parameters (LFE and Front Center)
			if (Channel == EAudioMixerChannel::LowFrequency)
			{
				EffectivePan = InWaveInstance->LFEBleed;
			}
			else if (Channel == PrevChannelInfo->Channel)
			{
				EffectivePan = !OmniAmount ? PrevChannelPan : FMath::Lerp(PrevChannelPan, OmniPanFactor, OmniAmount);
			}
			else if (Channel == NextChannelInfo->Channel)
			{
				EffectivePan = !OmniAmount ? NextChannelPan : FMath::Lerp(NextChannelPan, OmniPanFactor, OmniAmount);
			}

			if (Channel == EAudioMixerChannel::FrontCenter)
			{
				EffectivePan = FMath::Max(InWaveInstance->VoiceCenterChannelVolume, EffectivePan);
			}

			AUDIO_MIXER_CHECK(EffectivePan >= 0.0f && EffectivePan <= 1.0f);
			OutChannelMap.Add(EffectivePan);
		}
	}

	const TArray<FTransform>* FMixerDevice::GetListenerTransforms()
	{
		return SourceManager->GetListenerTransforms();
	}

	void FMixerDevice::StartRecording(USoundSubmix* InSubmix, float ExpectedRecordingDuration)
	{
		if (!IsInAudioThread())
		{
			DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.PauseRecording"), STAT_StartRecording, STATGROUP_AudioThreadCommands);

			FAudioThread::RunCommandOnAudioThread([this, InSubmix, ExpectedRecordingDuration]()
			{
				CSV_SCOPED_TIMING_STAT(Audio, StartRecording);
				StartRecording(InSubmix, ExpectedRecordingDuration);
			}, GET_STATID(STAT_StartRecording));
			return;
		}

		// if we can find the submix here, record that submix. Otherwise, just record the master submix.
		FMixerSubmixPtr FoundSubmix = GetSubmixInstance(InSubmix).Pin();
		if (FoundSubmix.IsValid())
		{
			FoundSubmix->OnStartRecordingOutput(ExpectedRecordingDuration);
		}
		else
		{
			FMixerSubmixWeakPtr MasterSubmix = GetMasterSubmix();
			FMixerSubmixPtr MasterSubmixPtr = MasterSubmix.Pin();
			check(MasterSubmixPtr.IsValid());

			MasterSubmixPtr->OnStartRecordingOutput(ExpectedRecordingDuration);
		}
	}

	Audio::FAlignedFloatBuffer& FMixerDevice::StopRecording(USoundSubmix* InSubmix, float& OutNumChannels, float& OutSampleRate)
	{
		// if we can find the submix here, record that submix. Otherwise, just record the master submix.
		FMixerSubmixPtr FoundSubmix = GetSubmixInstance(InSubmix).Pin();
		if (FoundSubmix.IsValid())
		{
			return FoundSubmix->OnStopRecordingOutput(OutNumChannels, OutSampleRate);
		}
		else
		{
			FMixerSubmixPtr MasterSubmixPtr = GetMasterSubmix().Pin();
			check(MasterSubmixPtr.IsValid());

			return MasterSubmixPtr->OnStopRecordingOutput(OutNumChannels, OutSampleRate);
		}
	}

	void FMixerDevice::PauseRecording(USoundSubmix* InSubmix)
	{
		if (!IsInAudioThread())
		{
			DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.PauseRecording"), STAT_PauseRecording, STATGROUP_AudioThreadCommands);

			FAudioThread::RunCommandOnAudioThread([this, InSubmix]()
			{
				CSV_SCOPED_TIMING_STAT(Audio, PauseRecording);
				PauseRecording(InSubmix);
			}, GET_STATID(STAT_PauseRecording));
			return;
		}

		// if we can find the submix here, pause that submix. Otherwise, just pause the master submix.
		FMixerSubmixPtr FoundSubmix = GetSubmixInstance(InSubmix).Pin();
		if (FoundSubmix.IsValid())
		{
			FoundSubmix->PauseRecordingOutput();
		}
		else
		{
			FMixerSubmixWeakPtr MasterSubmix = GetMasterSubmix();
			FMixerSubmixPtr MasterSubmixPtr = MasterSubmix.Pin();
			check(MasterSubmixPtr.IsValid());

			MasterSubmixPtr->PauseRecordingOutput();
		}
	}

	void FMixerDevice::ResumeRecording(USoundSubmix* InSubmix)
	{
		if (!IsInAudioThread())
		{
			DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.ResumeRecording"), STAT_ResumeRecording, STATGROUP_AudioThreadCommands);

			FAudioThread::RunCommandOnAudioThread([this, InSubmix]()
			{
				CSV_SCOPED_TIMING_STAT(Audio, ResumeRecording);
				ResumeRecording(InSubmix);
			}, GET_STATID(STAT_ResumeRecording));
			return;
		}

		// if we can find the submix here, resume that submix. Otherwise, just resume the master submix.
		FMixerSubmixPtr FoundSubmix = GetSubmixInstance(InSubmix).Pin();
		if (FoundSubmix.IsValid())
		{
			FoundSubmix->ResumeRecordingOutput();
		}
		else
		{
			FMixerSubmixWeakPtr MasterSubmix = GetMasterSubmix();
			FMixerSubmixPtr MasterSubmixPtr = MasterSubmix.Pin();
			check(MasterSubmixPtr.IsValid());

			MasterSubmixPtr->ResumeRecordingOutput();
		}
	}

	void FMixerDevice::StartEnvelopeFollowing(USoundSubmix* InSubmix)
	{
		if (!IsInAudioThread())
		{
			DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.StartEnvelopeFollowing"), STAT_StartEnvelopeFollowing, STATGROUP_AudioThreadCommands);

			FAudioThread::RunCommandOnAudioThread([this, InSubmix]()
			{
				CSV_SCOPED_TIMING_STAT(Audio, StartEnvelopeFollowing);
				StartEnvelopeFollowing(InSubmix);
			}, GET_STATID(STAT_StartEnvelopeFollowing));
			return;
		}

		// if we can find the submix here, record that submix. Otherwise, just record the master submix.
		FMixerSubmixPtr FoundSubmix = GetSubmixInstance(InSubmix).Pin();
		if (FoundSubmix.IsValid())
		{
			FoundSubmix->StartEnvelopeFollowing(InSubmix->EnvelopeFollowerAttackTime, InSubmix->EnvelopeFollowerReleaseTime);
		}
		else
		{
			FMixerSubmixWeakPtr MasterSubmix = GetMasterSubmix();
			FMixerSubmixPtr MasterSubmixPtr = MasterSubmix.Pin();
			check(MasterSubmixPtr.IsValid());

			MasterSubmixPtr->StartEnvelopeFollowing(InSubmix->EnvelopeFollowerAttackTime, InSubmix->EnvelopeFollowerReleaseTime);
		}

		DelegateBoundSubmixes.AddUnique(InSubmix);
	}

	void FMixerDevice::StopEnvelopeFollowing(USoundSubmix* InSubmix)
	{
		if (!IsInAudioThread())
		{
			DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.StopEnvelopeFollowing"), STAT_StopEnvelopeFollowing, STATGROUP_AudioThreadCommands);

			FAudioThread::RunCommandOnAudioThread([this, InSubmix]()
			{
				CSV_SCOPED_TIMING_STAT(Audio, StopEnvelopeFollowing);
				StopEnvelopeFollowing(InSubmix);
			}, GET_STATID(STAT_StopEnvelopeFollowing));
			return;
		}

		// if we can find the submix here, record that submix. Otherwise, just record the master submix.
		FMixerSubmixPtr FoundSubmix = GetSubmixInstance(InSubmix).Pin();
		if (FoundSubmix.IsValid())
		{
			FoundSubmix->StopEnvelopeFollowing();
		}
		else
		{
			FMixerSubmixWeakPtr MasterSubmix = GetMasterSubmix();
			FMixerSubmixPtr MasterSubmixPtr = MasterSubmix.Pin();
			check(MasterSubmixPtr.IsValid());

			MasterSubmixPtr->StopEnvelopeFollowing();
		}

		DelegateBoundSubmixes.RemoveSingleSwap(InSubmix);
	}

	void FMixerDevice::AddEnvelopeFollowerDelegate(USoundSubmix* InSubmix, const FOnSubmixEnvelopeBP& OnSubmixEnvelopeBP)
	{
		if (!IsInAudioThread())
		{
			DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.AddEnvelopeFollowerDelegate"), STAT_AddEnvelopeFollowerDelegate, STATGROUP_AudioThreadCommands);

			FAudioThread::RunCommandOnAudioThread([this, InSubmix, OnSubmixEnvelopeBP]()
			{
				CSV_SCOPED_TIMING_STAT(Audio, AddEnvelopeFollowerDelegate);
				AddEnvelopeFollowerDelegate(InSubmix, OnSubmixEnvelopeBP);
			}, GET_STATID(STAT_AddEnvelopeFollowerDelegate));
			return;
		}

		// if we can find the submix here, record that submix. Otherwise, just record the master submix.
		FMixerSubmixPtr FoundSubmix = GetSubmixInstance(InSubmix).Pin();
		if (FoundSubmix.IsValid())
		{
			FoundSubmix->AddEnvelopeFollowerDelegate(OnSubmixEnvelopeBP);
		}
		else
		{
			FMixerSubmixWeakPtr MasterSubmix = GetMasterSubmix();
			FMixerSubmixPtr MasterSubmixPtr = MasterSubmix.Pin();
			check(MasterSubmixPtr.IsValid());

			MasterSubmixPtr->AddEnvelopeFollowerDelegate(OnSubmixEnvelopeBP);
		}
	}


	void FMixerDevice::StartSpectrumAnalysis(USoundSubmix* InSubmix, const FSoundSpectrumAnalyzerSettings& InSettings)
	{
		if (!IsInAudioThread())
		{
			DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.StartSpectrumAnalysis"), STAT_StartSpectrumAnalysis, STATGROUP_AudioThreadCommands);

			FAudioThread::RunCommandOnAudioThread([this, InSubmix, InSettings]()
			{
				CSV_SCOPED_TIMING_STAT(Audio, StartSpectrumAnalysis);
				StartSpectrumAnalysis(InSubmix, InSettings);
			}, GET_STATID(STAT_StartSpectrumAnalysis));
			return;
		}

		FMixerSubmixPtr FoundSubmix = GetSubmixInstance(InSubmix).Pin();
		if (FoundSubmix.IsValid())
		{
			FoundSubmix->StartSpectrumAnalysis(InSettings);
		}
		else
		{
			FMixerSubmixWeakPtr MasterSubmix = GetMasterSubmix();
			FMixerSubmixPtr MasterSubmixPtr = MasterSubmix.Pin();
			check(MasterSubmixPtr.IsValid());

			MasterSubmixPtr->StartSpectrumAnalysis(InSettings);
		}

		DelegateBoundSubmixes.AddUnique(InSubmix);
	}

	void FMixerDevice::StopSpectrumAnalysis(USoundSubmix* InSubmix)
	{
		if (!IsInAudioThread())
		{
			DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.StopSpectrumAnalysis"), STAT_StopSpectrumAnalysis, STATGROUP_AudioThreadCommands);

			FAudioThread::RunCommandOnAudioThread([this, InSubmix]()
			{
				CSV_SCOPED_TIMING_STAT(Audio, StopSpectrumAnalysis);
				StopSpectrumAnalysis(InSubmix);
			}, GET_STATID(STAT_StopSpectrumAnalysis));
			return;
		}

		FMixerSubmixPtr FoundSubmix = GetSubmixInstance(InSubmix).Pin();
		if (FoundSubmix.IsValid())
		{
			FoundSubmix->StopSpectrumAnalysis();
		}
		else
		{
			FMixerSubmixWeakPtr MasterSubmix = GetMasterSubmix();
			FMixerSubmixPtr MasterSubmixPtr = MasterSubmix.Pin();
			check(MasterSubmixPtr.IsValid());

			MasterSubmixPtr->StopSpectrumAnalysis();
		}

		DelegateBoundSubmixes.RemoveSingleSwap(InSubmix);

	}

	void FMixerDevice::GetMagnitudesForFrequencies(USoundSubmix* InSubmix, const TArray<float>& InFrequencies, TArray<float>& OutMagnitudes)
	{
		FMixerSubmixPtr FoundSubmix = GetSubmixInstance(InSubmix).Pin();
		if (FoundSubmix.IsValid())
		{
			FoundSubmix->GetMagnitudeForFrequencies(InFrequencies, OutMagnitudes);
		}
		else
		{
			FMixerSubmixWeakPtr MasterSubmix = GetMasterSubmix();
			FMixerSubmixPtr MasterSubmixPtr = MasterSubmix.Pin();
			check(MasterSubmixPtr.IsValid());

			MasterSubmixPtr->GetMagnitudeForFrequencies(InFrequencies, OutMagnitudes);
		}
	}

	void FMixerDevice::GetPhasesForFrequencies(USoundSubmix* InSubmix, const TArray<float>& InFrequencies, TArray<float>& OutPhases)
	{
		FMixerSubmixPtr FoundSubmix = GetSubmixInstance(InSubmix).Pin();
		if (FoundSubmix.IsValid())
		{
			FoundSubmix->GetPhaseForFrequencies(InFrequencies, OutPhases);
		}
		else
		{
			FMixerSubmixWeakPtr MasterSubmix = GetMasterSubmix();
			FMixerSubmixPtr MasterSubmixPtr = MasterSubmix.Pin();
			check(MasterSubmixPtr.IsValid());

			MasterSubmixPtr->GetPhaseForFrequencies(InFrequencies, OutPhases);
		}
	}

	void FMixerDevice::AddSpectralAnalysisDelegate(USoundSubmix* InSubmix, const FSoundSpectrumAnalyzerDelegateSettings& InDelegateSettings, const FOnSubmixSpectralAnalysisBP& OnSubmixSpectralAnalysisBP)
	{

		if (!IsInAudioThread())
		{
			DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.AddSpectralAnalysisDelegate"), STAT_AddSpectralAnalysisDelegate, STATGROUP_AudioThreadCommands);

			FAudioThread::RunCommandOnAudioThread([this, InSubmix, InDelegateSettings, OnSubmixSpectralAnalysisBP]()
			{
				CSV_SCOPED_TIMING_STAT(Audio, AddSpectralAnalysisDelegate);
				AddSpectralAnalysisDelegate(InSubmix, InDelegateSettings, OnSubmixSpectralAnalysisBP);
			}, GET_STATID(STAT_AddSpectralAnalysisDelegate));
			return;
		}

		// get submix if it is available.
		FMixerSubmixPtr FoundSubmix = GetSubmixInstance(InSubmix).Pin();

		if (!FoundSubmix.IsValid())
		{
			// If can't find the submix isntance, use master submix.
			FMixerSubmixWeakPtr MasterSubmix = GetMasterSubmix();
			FoundSubmix = MasterSubmix.Pin();
		}

		if (ensure(FoundSubmix.IsValid()))
		{
			FoundSubmix->AddSpectralAnalysisDelegate(InDelegateSettings, OnSubmixSpectralAnalysisBP);
		}
	}

	void FMixerDevice::RemoveSpectralAnalysisDelegate(USoundSubmix* InSubmix, const FOnSubmixSpectralAnalysisBP& InDelegate)
	{
		if (!IsInAudioThread())
		{
			DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.RemoveSpectralAnalysisDelegate"), STAT_RemoveSpectralAnalysisDelegate, STATGROUP_AudioThreadCommands);

			FAudioThread::RunCommandOnAudioThread([this, InSubmix, InDelegate]()
			{
				CSV_SCOPED_TIMING_STAT(Audio, RemoveSpectralAnalysisDelegate);
				RemoveSpectralAnalysisDelegate(InSubmix, InDelegate);
			}, GET_STATID(STAT_RemoveSpectralAnalysisDelegate));
			return;
		}

		// get submix if it is available.
		FMixerSubmixPtr FoundSubmix = GetSubmixInstance(InSubmix).Pin();

		if (!FoundSubmix.IsValid())
		{
			// If can't find the submix isntance, use master submix.
			FMixerSubmixWeakPtr MasterSubmix = GetMasterSubmix();
			FoundSubmix = MasterSubmix.Pin();
		}

		if (ensure(FoundSubmix.IsValid()))
		{
			FoundSubmix->RemoveSpectralAnalysisDelegate(InDelegate);
		}
	}

	void FMixerDevice::RegisterSubmixBufferListener(ISubmixBufferListener* InSubmixBufferListener, USoundSubmix* InSubmix)
	{
		DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.RegisterSubmixBufferListener"), STAT_RegisterSubmixBufferListener, STATGROUP_AudioThreadCommands);

		const bool bUseMaster = InSubmix == nullptr;
		const TWeakObjectPtr<USoundSubmix> SubmixPtr(InSubmix);

		auto RegisterLambda = [this, InSubmixBufferListener, bUseMaster, SubmixPtr]()
		{
			CSV_SCOPED_TIMING_STAT(Audio, RegisterSubmixBufferListener);

			FMixerSubmixPtr FoundSubmix = bUseMaster
				? GetMasterSubmix().Pin()
				: GetSubmixInstance(SubmixPtr.Get()).Pin();

			// Attempt to register submix if instance not found and is not master (i.e. default) submix
			if (!bUseMaster && !FoundSubmix.IsValid() && SubmixPtr.IsValid())
			{
				RegisterSoundSubmix(SubmixPtr.Get(), true /* bInit */);
				FoundSubmix = GetSubmixInstance(SubmixPtr.Get()).Pin();
			}

			if (FoundSubmix.IsValid())
			{
				FoundSubmix->RegisterBufferListener(InSubmixBufferListener);
			}
			else
			{
				UE_LOG(LogAudioMixer, Warning, TEXT("Submix buffer listener not registered. Submix not loaded."));
			}
		};

		FAudioThread::RunCommandOnAudioThread(MoveTemp(RegisterLambda));
	}

	void FMixerDevice::UnregisterSubmixBufferListener(ISubmixBufferListener* InSubmixBufferListener, USoundSubmix* InSubmix)
	{
		DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.UnregisterSubmixBufferListener"), STAT_UnregisterSubmixBufferListener, STATGROUP_AudioThreadCommands);

		const bool bUseMaster = InSubmix == nullptr;
		const TWeakObjectPtr<USoundSubmix> SubmixPtr(InSubmix);

		auto UnregisterLambda = [this, InSubmixBufferListener, bUseMaster, SubmixPtr]()
		{
			CSV_SCOPED_TIMING_STAT(Audio, UnregisterSubmixBufferListener);

			FMixerSubmixPtr FoundSubmix = bUseMaster
				? GetMasterSubmix().Pin()
				: GetSubmixInstance(SubmixPtr.Get()).Pin();

			if (FoundSubmix.IsValid())
			{
				FoundSubmix->UnregisterBufferListener(InSubmixBufferListener);
			}
			else
			{
				UE_LOG(LogAudioMixer, Display, TEXT("Submix buffer listener not unregistered. Submix not loaded."));
			}
		};

		FAudioThread::RunCommandOnAudioThread(MoveTemp(UnregisterLambda));
	}

	void FMixerDevice::FlushExtended(UWorld* WorldToFlush, bool bClearActivatedReverb)
	{
		QuantizedEventClockManager.Flush();
	}

	void FMixerDevice::StartAudioBus(uint32 InAudioBusId, int32 InNumChannels, bool bInIsAutomatic)
	{
		if (IsInGameThread())
		{
			if (ActiveAudioBuses_GameThread.Contains(InAudioBusId))
			{
				return;
			}

			FActiveBusData BusData;
			BusData.BusId = InAudioBusId;
			BusData.NumChannels = InNumChannels;
			BusData.bIsAutomatic = bInIsAutomatic;

			ActiveAudioBuses_GameThread.Add(InAudioBusId, BusData);

			FAudioThread::RunCommandOnAudioThread([this, InAudioBusId, InNumChannels, bInIsAutomatic]()
			{
				SourceManager->StartAudioBus(InAudioBusId, InNumChannels, bInIsAutomatic);
			});
		}
		else
		{
			// If we're not the game thread, this needs to be on the game thread, so queue up a command to execute it on the game thread
			GameThreadMPSCCommand([this, InAudioBusId, InNumChannels, bInIsAutomatic]
			{
				StartAudioBus(InAudioBusId, InNumChannels, bInIsAutomatic);	
			});
		}
	}

	void FMixerDevice::StopAudioBus(uint32 InAudioBusId)
	{
		if (IsInGameThread())
		{
			if (!ActiveAudioBuses_GameThread.Contains(InAudioBusId))
			{
				return;
			}

			ActiveAudioBuses_GameThread.Remove(InAudioBusId);

			FAudioThread::RunCommandOnAudioThread([this, InAudioBusId]()
			{
				SourceManager->StopAudioBus(InAudioBusId);
			});
		}
		else
		{
			// If we're not the game thread, this needs to be on the game thread, so queue up a command to execute it on the game thread
			GameThreadMPSCCommand([this, InAudioBusId]
			{
				StopAudioBus(InAudioBusId);	
			});
		}
	}

	bool FMixerDevice::IsAudioBusActive(uint32 InAudioBusId) const
	{
		if (IsInGameThread())
		{
			return ActiveAudioBuses_GameThread.Contains(InAudioBusId);
		}

		check(IsInAudioThread());
		return SourceManager->IsAudioBusActive(InAudioBusId);
	}


	FPatchOutputStrongPtr FMixerDevice::AddPatchForAudioBus(uint32 InAudioBusId, float InPatchGain)
	{
		// This function is supporting adding audio bus patches from multiple threads (AT, ART, GT, and tasks) and is currently
		// depending on a number of places where data lives, which accounts for the complexity here.
		// The key idea here is to create and return a strong patch output ptr at roughly the size we expect to need then 
		// pass the strong ptr down to the audio render thread that then is registered to the audio bus to start feeding audio to.
		// This code needs a clean up to refactor everything into a true MPSC model, along with an MPSC refactor of the source manager
		// and our command queues. Once we do that we can remove the code which branches based on the thread the request is coming from. 

		if (IsInGameThread())
		{
			if (ActiveAudioBuses_GameThread.Find(InAudioBusId))
			{
				const int32 NumOutputFrames = SourceManager->GetNumOutputFrames();
				FPatchOutputStrongPtr StrongOutputPtr = MakeShareable(new FPatchOutput(2 * NumOutputFrames, InPatchGain));
				FAudioThread::RunCommandOnAudioThread([this, StrongOutputPtr, InAudioBusId]() mutable
				{
					SourceManager->AddPatchOutputForAudioBus_AudioThread(InAudioBusId, StrongOutputPtr);
				});
				return StrongOutputPtr;
			}
			UE_LOG(LogAudioMixer, Warning, TEXT("Unable to add a patch output for audio bus because audio bus id '%d' is not active."), InAudioBusId);
			return nullptr;
		}
		else if (IsInAudioThread())
		{
			int32 NumOutputFrames = SourceManager->GetNumOutputFrames();
			FPatchOutputStrongPtr StrongOutputPtr = MakeShareable(new FPatchOutput(2 * NumOutputFrames, InPatchGain));

			SourceManager->AddPatchOutputForAudioBus_AudioThread(InAudioBusId, StrongOutputPtr);
			return StrongOutputPtr;
		}
		else if (IsAudioRenderingThread())
		{
			check(SourceManager);

			const int32 NumChannels = SourceManager->GetAudioBusNumChannels(InAudioBusId);
			if (NumChannels > 0)
			{
				const int32 NumOutputFrames = SourceManager->GetNumOutputFrames();
				FPatchOutputStrongPtr StrongOutputPtr = MakeShareable(new FPatchOutput(NumOutputFrames * NumChannels, InPatchGain));
				SourceManager->AddPatchOutputForAudioBus(InAudioBusId, StrongOutputPtr);
				return StrongOutputPtr;
			}

			return nullptr;
		}
		else
		{
			// Need to make a strong output patch even if this is not going to ever write to it since the bus may not be running
			const int32 NumOutputFrames = SourceManager->GetNumOutputFrames();
			FPatchOutputStrongPtr StrongOutputPtr = MakeShareable(new FPatchOutput(3 * NumOutputFrames, InPatchGain));

			GameThreadMPSCCommand([this, StrongOutputPtr, InAudioBusId]
			{
				if (ActiveAudioBuses_GameThread.Find(InAudioBusId))
				{
					FAudioThread::RunCommandOnAudioThread([this, InAudioBusId, StrongOutputPtr]() mutable
					{
						SourceManager->AddPatchOutputForAudioBus_AudioThread(InAudioBusId, StrongOutputPtr);
					});
				}
				else
				{
					UE_LOG(LogAudioMixer, Warning, TEXT("Unable to add a patch output for audio bus because audio bus id '%d' is not active."), InAudioBusId);
				}
			});
			return StrongOutputPtr;
		}
	}

	FPatchOutputStrongPtr FMixerDevice::AddPatchForAudioBus_GameThread(uint32 InAudioBusId, float InPatchGain)
	{
		FPatchOutputStrongPtr StrongOutputPtr;
		
		if (IsInGameThread())
		{
			FActiveBusData* BusData = ActiveAudioBuses_GameThread.Find(InAudioBusId);
			if (BusData)
			{
				int32 NumOutputFrames = SourceManager->GetNumOutputFrames();
				int32 BusNumChannels = BusData->NumChannels;
				StrongOutputPtr = MakeShareable(new FPatchOutput(NumOutputFrames * BusData->NumChannels, InPatchGain));

				FAudioThread::RunCommandOnAudioThread([this, InAudioBusId, StrongOutputPtr]() mutable
				{
					SourceManager->AddPatchOutputForAudioBus_AudioThread(InAudioBusId, StrongOutputPtr);
				});
			}
			else
			{
				UE_LOG(LogAudioMixer, Warning, TEXT("Unable to add a patch output for audio bus because audio bus id '%d' is not active."), InAudioBusId);
			}
		}
		else
		{
			UE_LOG(LogAudioMixer, Warning, TEXT("AddPatchForAudioBus can only be called from the game thread."));
		}
		return StrongOutputPtr;
	}

	Audio::FPatchOutputStrongPtr FMixerDevice::AddPatchForSubmix(uint32 InObjectId, float InPatchGain)
	{
		if (!ensure(IsAudioRenderingThread()))
		{
			return nullptr;
		}

		FMixerSubmixPtr SubmixPtr = FindSubmixInstanceByObjectId(InObjectId);
		if (SubmixPtr.IsValid())
		{
			return SubmixPtr->AddPatch(InPatchGain);
		}

		return nullptr;
	}

	int32 FMixerDevice::GetDeviceSampleRate() const
	{
		return SampleRate;
	}

	int32 FMixerDevice::GetDeviceOutputChannels() const
	{
		return PlatformInfo.NumChannels;
	}

	FMixerSourceManager* FMixerDevice::GetSourceManager()
	{
		return SourceManager.Get();
	}

	bool FMixerDevice::IsMainAudioDevice() const
	{
		bool bIsMain = (this == FAudioDeviceManager::Get()->GetMainAudioDeviceRaw());
		return bIsMain;
	}

	void FMixerDevice::WhiteNoiseTest(FAlignedFloatBuffer& Output)
	{
		const int32 NumFrames = OpenStreamParams.NumFrames;
		const int32 NumChannels = PlatformInfo.NumChannels;

		static FWhiteNoise WhiteNoise;

		for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
		{
			for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
			{
				int32 Index = FrameIndex * NumChannels + ChannelIndex;
				Output[Index] += WhiteNoise.Generate(DebugGeneratorAmpCVar, 0.f);
			}
		}
	}

	void FMixerDevice::SineOscTest(FAlignedFloatBuffer& Output)
	{
		const int32 NumFrames = OpenStreamParams.NumFrames;
		const int32 NumChannels = PlatformInfo.NumChannels;

		check(NumChannels > 0);

		// Constrain user setting if channel index not supported
		const int32 ChannelIndex = FMath::Clamp(DebugGeneratorChannelCVar, 0, NumChannels - 1);

		static FSineOsc SineOscLeft(PlatformInfo.SampleRate, DebugGeneratorFreqCVar, DebugGeneratorAmpCVar);
		static FSineOsc SineOscRight(PlatformInfo.SampleRate, DebugGeneratorFreqCVar / 2.0f, DebugGeneratorAmpCVar);

		SineOscLeft.SetFrequency(DebugGeneratorFreqCVar);
		SineOscLeft.SetScale(DebugGeneratorAmpCVar);

		if (!DebugGeneratorEnableCVar)
		{
			SineOscRight.SetFrequency(DebugGeneratorFreqCVar / 2.0f);
			SineOscRight.SetScale(DebugGeneratorAmpCVar);
		}

		for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
		{
			int32 Index = FrameIndex * NumChannels;

			Output[Index + ChannelIndex] += SineOscLeft.ProcessAudio();

			// Using au. commands for debug only supports discrete channel
			if (!DebugGeneratorEnableCVar)
			{
				if (NumChannels > 1 && DebugGeneratorChannelCVar == 0)
				{
					Output[Index + 1] += SineOscRight.ProcessAudio();
				}
			}
		}
	}

	void FMixerDevice::CreateSynchronizedAudioTaskQueue(AudioTaskQueueId QueueId)
	{
		Audio::CreateSynchronizedAudioTaskQueue(QueueId);
	}

	void FMixerDevice::DestroySynchronizedAudioTaskQueue(AudioTaskQueueId QueueId, bool RunCurrentQueue)
	{
		Audio::DestroySynchronizedAudioTaskQueue(QueueId, RunCurrentQueue);
	}

	int FMixerDevice::KickQueuedTasks(AudioTaskQueueId QueueId)
	{
		return Audio::KickQueuedTasks(QueueId);
	}

}
