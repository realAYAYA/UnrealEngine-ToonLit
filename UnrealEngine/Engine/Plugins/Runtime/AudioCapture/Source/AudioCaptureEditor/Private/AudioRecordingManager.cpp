// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioRecordingManager.h"
#include "AudioCaptureEditor.h"
#include "Sound/AudioSettings.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Factories/SoundFactory.h"
#include "Misc/ScopeLock.h"
#include "Sound/SoundWave.h"
#include "AudioDeviceManager.h"
#include "Engine/Engine.h"
#include "Components/AudioComponent.h"
#include "DSP/Dsp.h"
#include "Audio.h"

DEFINE_LOG_CATEGORY(LogMicManager);

namespace Audio
{

/**
* Callback Function For the Microphone Capture for RtAudio
*/
static int32 OnAudioCaptureCallback(void *OutBuffer, void* InBuffer, uint32 InBufferFrames, double StreamTime, uint32 AudioStreamStatus, void* InUserData)
{
	// Check for stream overflow (i.e. we're feeding audio faster than we're consuming)
	const bool bIsOverflow = 0 != AudioStreamStatus;

	// Cast the user data to the singleton mic recorder
	FAudioRecordingManager* AudioRecordingManager = (FAudioRecordingManager*)InUserData;

	// Call the mic capture callback function
	return AudioRecordingManager->OnAudioCapture(InBuffer, InBufferFrames, StreamTime, bIsOverflow);
}

/**
* FAudioRecordingManager Implementation
*/

FAudioRecordingManager::FAudioRecordingManager()
	: NumRecordedSamples(0)
	, NumFramesToRecord()
	, RecordingBlockSize(0)
	, RecordingSampleRate(44100.0f)
	, NumInputChannels(1)
	, InputGain(1.0f)
	, bRecording(false)
	, bError(false)
{
}

FAudioRecordingManager::~FAudioRecordingManager()
{
	if (ADC.IsStreamOpen())
	{
		ADC.AbortStream();
	}
}

FAudioRecordingManager& FAudioRecordingManager::Get()
{
	// Static isngleton instance
	static FAudioRecordingManager MicInputManager;
	return MicInputManager;
}

void FAudioRecordingManager::StartRecording(const FRecordingSettings& InSettings, TArray<USoundWave*>& OutSoundWaves)
{
	if (bError)
	{
		return;
	}

	// Stop any recordings currently going on (if there is one)
	StopRecording(OutSoundWaves);

	// Default to 1024 blocks
	RecordingBlockSize = 1024;

	// If we have a stream open close it (reusing streams can cause a blip of previous recordings audio)
	if (ADC.IsStreamOpen())
	{
		ADC.StopStream();
		ADC.CloseStream();
	}

	UE_LOG(LogMicManager, Log, TEXT("Starting mic recording."));

	// Copy the settings
	Settings = InSettings;

	// Convert input gain decibels into linear scale
	InputGain = Audio::ConvertToLinear(Settings.GainDb);

	// Get the default mic input device info
	FRtAudioInputWrapper::FDeviceInfo Info = ADC.GetDeviceInfo(StreamParams.DeviceID);
	RecordingSampleRate = Info.PreferredSampleRate;
	NumInputChannels = Info.NumChannels;	

	if (NumInputChannels == 0)
	{
		UE_LOG(LogMicManager, Warning, TEXT("No audio channels to record."));
		return;
	}

	// Reserve enough space in our current recording buffer for 10 seconds of audio to prevent slowing down due to allocations
	if (Settings.RecordingDuration > 0.0f)
	{
		int32 NumSamplesToReserve = Settings.RecordingDuration * RecordingSampleRate * NumInputChannels;
		CurrentRecordedPCMData.Reset(NumSamplesToReserve);

		// Set a limit on the number of frames to record
		NumFramesToRecord = RecordingSampleRate * Settings.RecordingDuration;
	}
	else
	{
		// if not given a duration, resize array to 60 seconds of audio anyway
		int32 NumSamplesToReserve = 60 * RecordingSampleRate * NumInputChannels;
		CurrentRecordedPCMData.Reset(NumSamplesToReserve);

		// Set to be INDEX_NONE otherwise
		NumFramesToRecord = INDEX_NONE;
	}

	// If we have more than 2 channels, we're going to force splitting up the assets since we don't propertly support multi-channel USoundWave assets
	if (NumInputChannels > 2)
	{
		Settings.bSplitChannels = true;
	}

	NumRecordedSamples = 0;
	NumOverflowsDetected = 0;

	// Publish to the mic input thread that we're ready to record...
	bRecording = true;

	StreamParams.DeviceID = ADC.GetDefaultInputDevice(); // Only use the default input device for now

	StreamParams.NumChannels = NumInputChannels;

	uint32 BufferFrames = FMath::Max(RecordingBlockSize, 256);

	UE_LOG(LogMicManager, Log, TEXT("Initialized mic recording manager at %d hz sample rate, %d channels, and %d Recording Block Size"), RecordingSampleRate, StreamParams.NumChannels, BufferFrames);

	// RtAudio uses exceptions for error handling... 
	// Open up new audio stream
	bool bSuccess = ADC.OpenStream(StreamParams, RecordingSampleRate, &BufferFrames, &OnAudioCaptureCallback, this);
	bError = !bSuccess;

	if (bSuccess)
	{
		ADC.StartStream();
	}
}

static void SampleRateConvert(float CurrentSR, float TargetSR, int32 NumChannels, const TArray<int16>& CurrentRecordedPCMData, int32 NumSamplesToConvert, TArray<int16>& OutConverted)
{
	int32 NumInputSamples = CurrentRecordedPCMData.Num();
	int32 NumOutputSamples = NumInputSamples * TargetSR / CurrentSR;

	OutConverted.Reset(NumOutputSamples);

	float SrFactor = (double)CurrentSR / TargetSR;
	float CurrentFrameIndexInterpolated = 0.0f;
	check(NumSamplesToConvert <= CurrentRecordedPCMData.Num());
	int32 NumFramesToConvert = NumSamplesToConvert / NumChannels;
	int32 CurrentFrameIndex = 0;

	for (;;)
	{
		int32 NextFrameIndex = CurrentFrameIndex + 1;
		if (CurrentFrameIndex >= NumFramesToConvert || NextFrameIndex >= NumFramesToConvert)
		{
			break;
		}

		for (int32 Channel = 0; Channel < NumChannels; ++Channel)
		{
			int32 CurrentSampleIndex = CurrentFrameIndex * NumChannels + Channel;
			int32 NextSampleIndex = CurrentSampleIndex + NumChannels;

			int16 CurrentSampleValue = CurrentRecordedPCMData[CurrentSampleIndex];
			int16 NextSampleValue = CurrentRecordedPCMData[NextSampleIndex];

			int16 NewSampleValue = FMath::Lerp(CurrentSampleValue, NextSampleValue, CurrentFrameIndexInterpolated);

			OutConverted.Add(NewSampleValue);
		}

		CurrentFrameIndexInterpolated += SrFactor;

		// Wrap the interpolated frame between 0.0 and 1.0 to maintain float precision
		while (CurrentFrameIndexInterpolated >= 1.0f)
		{
			CurrentFrameIndexInterpolated -= 1.0f;
			
			// Every time it wraps, we increment the frame index
			++CurrentFrameIndex;
		}
	}
}

void FAudioRecordingManager::StopRecording(TArray<USoundWave*>& OutSoundWaves)
{
	FScopeLock Lock(&CriticalSection);

	// If we're currently recording, stop the recording
	if (bRecording)
	{
		bRecording = false;
		ADC.StopStream();
		ADC.CloseStream();

		if (CurrentRecordedPCMData.Num() > 0)
		{
			if (NumFramesToRecord != INDEX_NONE)
			{
				NumRecordedSamples = FMath::Min(NumFramesToRecord * NumInputChannels, CurrentRecordedPCMData.Num());
			}
			else
			{
				NumRecordedSamples = CurrentRecordedPCMData.Num();
			}

			UE_LOG(LogMicManager, Log, TEXT("Stopping mic recording. Recorded %d frames of audio (%.4f seconds). Detected %d buffer overflows."), 
				NumRecordedSamples, 
				((float)NumRecordedSamples / RecordingSampleRate) / (float)NumInputChannels,
				NumOverflowsDetected);

			// Get a ptr to the buffer we're actually going to serialize
			TArray<int16>* PCMDataToSerialize = nullptr;

			// If our sample rate isn't 44100, then we need to do a SRC
			if (RecordingSampleRate != 44100.0f)
			{
				UE_LOG(LogMicManager, Log, TEXT("Converting sample rate from %d hz to 44100 hz."), (int32)RecordingSampleRate);

				SampleRateConvert(RecordingSampleRate, (float)WAVE_FILE_SAMPLERATE, NumInputChannels, CurrentRecordedPCMData, NumRecordedSamples, ConvertedPCMData);

				// Update the recorded samples to the converted buffer samples
				NumRecordedSamples = ConvertedPCMData.Num();
				PCMDataToSerialize = &ConvertedPCMData;
			}
			else
			{
				// Just use the original recorded buffer to serialize
				PCMDataToSerialize = &CurrentRecordedPCMData;
			}

			// Scale by the linear gain if it's been set to something
			if (InputGain != 1.0f)
			{
				UE_LOG(LogMicManager, Log, TEXT("Scaling gain of recording by %.2f linear gain."), InputGain);

				for (int32 i = 0; i < NumRecordedSamples; ++i)
				{
					// Scale by input gain, clamp to prevent integer overflow when casting back to int16. Will still clip.
					(*PCMDataToSerialize)[i] = (int16)FMath::Clamp(InputGain * (float)(*PCMDataToSerialize)[i], -32767.0f, 32767.0f);
				}
			}

			uint8* RawData = nullptr;
			int32 NumBytes = 0;
			int32 NumChannelsToSerialize = 0;
			int32 NumWavesToSerialize = 0;

			if (Settings.bSplitChannels)
			{
				// We're going to serialize several waves, one for each input channel
				NumWavesToSerialize = NumInputChannels;

				// We're only going to serialize 1 channel of audio at a time
				NumChannelsToSerialize = 1;

				// De-interleaved buffer size will be the number of frames of audio
				int32 NumFrames = NumRecordedSamples / NumInputChannels;

				// This is the num bytes per de-interleaved audio buffer
				NumBytes = NumFrames * sizeof(int16);

				// Reset our deinterleaved audio buffer
				DeinterleavedAudio.Reset(NumInputChannels);

				// Get ptr to the interleaved buffer for speed in non-release builds
				int16* InterleavedBufferPtr = PCMDataToSerialize->GetData();

				// For every input channel, create a new buffer
				for (int32 Channel = 0; Channel < NumInputChannels; ++Channel)
				{
					// Prepare a new deinterleaved buffer
					DeinterleavedAudio.Add(FDeinterleavedAudio());
					FDeinterleavedAudio& DeinterleavedChannelAudio = DeinterleavedAudio[Channel];

					DeinterleavedChannelAudio.PCMData.Reset();
					DeinterleavedChannelAudio.PCMData.AddUninitialized(NumFrames);

					// Get a ptr to the buffer for speed
					int16* DeinterleavedBufferPtr = DeinterleavedChannelAudio.PCMData.GetData();

					// Copy every N channel to the deinterleaved buffer, starting with the current channel
					int32 CurrentSample = Channel;
					for (int32 Frame = 0; Frame < NumFrames; ++Frame)
					{
						// Simply copy the interleaved value to the deinterleaved value
						DeinterleavedBufferPtr[Frame] = InterleavedBufferPtr[CurrentSample];

						// Increment the stride according the num channels
						CurrentSample += NumInputChannels;
					}
				}
			}
			else
			{
				// Only need to serialize one channel
				NumWavesToSerialize = 1;
				RawData = (uint8*)PCMDataToSerialize->GetData();
				NumBytes = NumRecordedSamples * sizeof(int16);
				NumChannelsToSerialize = NumInputChannels;
			}

			// Loop through the number of waves we're going to serialize
			for (int32 i = 0; i < NumWavesToSerialize; ++i)
			{
				USoundWave* NewSoundWave = nullptr;
				USoundWave* ExistingSoundWave = nullptr;
				TArray<UAudioComponent*> ComponentsToRestart;
				bool bCreatedPackage = false;

				if (Settings.Directory.Path.IsEmpty() || Settings.AssetName.IsEmpty())
				{
					// Create a new sound wave object from the transient package
					NewSoundWave = NewObject<USoundWave>(GetTransientPackage(), *Settings.AssetName);
				}
				else
				{
					FString PackageName;
					FString AssetName = Settings.AssetName;
					if (NumWavesToSerialize != 1)
					{
						AssetName = FString::Printf(TEXT("%s_Channel_%d"), *AssetName, i);
						PackageName = Settings.Directory.Path / AssetName;

						// Get the raw data from the deinterleaved audio location.
						RawData = (uint8*)DeinterleavedAudio[i].PCMData.GetData();
					}
					else
					{
						// Create a new package
						PackageName = Settings.Directory.Path / AssetName;
					}

					UPackage* Package = CreatePackage(*PackageName);

					// Create a raw .wav file to stuff the raw PCM data in so when we create the sound wave asset it's identical to a normal imported asset
					check(RawData != nullptr);
					SerializeWaveFile(RawWaveData, RawData, NumBytes, NumChannelsToSerialize, WAVE_FILE_SAMPLERATE);

					// Check to see if a sound wave already exists at this location
					ExistingSoundWave = FindObject<USoundWave>(Package, *AssetName);

					// See if it's currently being played right now
					FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
					if (AudioDeviceManager && ExistingSoundWave)
					{
						AudioDeviceManager->StopSoundsUsingResource(ExistingSoundWave, &ComponentsToRestart);
					}

					// Create a new sound wave object
					if (ExistingSoundWave)
					{
						NewSoundWave = ExistingSoundWave;
						NewSoundWave->FreeResources();
					}
					else
					{
						NewSoundWave = NewObject<USoundWave>(Package, *AssetName, RF_Public | RF_Standalone);
					}

					// Compressed data is now out of date.
					NewSoundWave->InvalidateCompressedData(true, false);

					// Copy the raw wave data file to the sound wave for storage. Will allow the recording to be exported.
					FSharedBuffer UpdatedBuffed = FSharedBuffer::Clone(RawWaveData.GetData(), RawWaveData.Num());
					NewSoundWave->RawData.UpdatePayload(UpdatedBuffed);

					bCreatedPackage = true;
				}

				if (NewSoundWave)
				{
					// Copy the recorded data to the sound wave so we can quickly preview it
					NewSoundWave->RawPCMDataSize = NumBytes;
					NewSoundWave->RawPCMData = (uint8*)FMemory::Malloc(NewSoundWave->RawPCMDataSize);
					FMemory::Memcpy(NewSoundWave->RawPCMData, RawData, NumBytes);

					// Calculate the duration of the sound wave
					// Note: We use the NumInputChannels for duration calculation since NumChannelsToSerialize may be 1 channel while NumInputChannels is 2 for the "split stereo" feature.
					NewSoundWave->Duration = (float)(NumRecordedSamples / NumInputChannels) / WAVE_FILE_SAMPLERATE;
					NewSoundWave->SetSampleRate(WAVE_FILE_SAMPLERATE);
					NewSoundWave->NumChannels = NumChannelsToSerialize;
					NewSoundWave->SetSoundAssetCompressionType(ESoundAssetCompressionType::BinkAudio);

					if (bCreatedPackage)
					{
						//GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, NewSoundWave);

						FAssetRegistryModule::AssetCreated(NewSoundWave);
						NewSoundWave->MarkPackageDirty();

						// Restart any audio components if they need to be restarted
						for (int32 ComponentIndex = 0; ComponentIndex < ComponentsToRestart.Num(); ++ComponentIndex)
						{
							ComponentsToRestart[ComponentIndex]->Play();
						}
					}

					OutSoundWaves.Add(NewSoundWave);
				}
			}
		}
	}
}

int32 FAudioRecordingManager::OnAudioCapture(void* InBuffer, uint32 InBufferFrames, double StreamTime, bool bOverflow)
{
	FScopeLock Lock(&CriticalSection);

	if (bRecording)
	{
		if (bOverflow) 
			++NumOverflowsDetected;

		CurrentRecordedPCMData.Append((int16*)InBuffer, InBufferFrames * NumInputChannels);
		return 0;
	}

	return 1;
}

} // namespace Audio
