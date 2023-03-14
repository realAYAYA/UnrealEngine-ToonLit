// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeBool.h"
#include "Engine/EngineTypes.h"
#include "RtAudioWrapper.h"

class USoundWave;

DECLARE_LOG_CATEGORY_EXTERN(LogMicManager, Log, All);

namespace Audio
{
	// Settings used for starting a recording with the recording manager
	struct FRecordingSettings
	{
		// The place to put the audio file recording
		FDirectoryPath Directory;

		// The name of the asset you want to record
		FString AssetName;

		// The length of the recording. Set to 0.0 or less for unbounded recording duration.
		float RecordingDuration;

		// The output gain of the recording. Allows you to boost the amplitude of your recording.
		float GainDb;

		// Whether or not to split the asset into separate assets for each microphone input channel
		bool bSplitChannels;

		FRecordingSettings()
			: RecordingDuration(0.0f)
			, GainDb(0.0f)
			, bSplitChannels(false)
		{}
	};

	/**
	 * FAudioRecordingManager
	 * Singleton Mic Recording Manager -- generates recordings, stores the recorded data and plays them back
	 */
	class FAudioRecordingManager
	{
	public:
		// Retrieves the singleton recording manager
		static FAudioRecordingManager& Get();

		// Starts a new recording with the given name and optional duration. 
		// If set to -1.0f, a duration won't be used and the recording length will be determined by StopRecording().
		void StartRecording(const FRecordingSettings& Settings, TArray<USoundWave*>& OutSoundWaves);

		// Stops recording if the recording manager is recording. If not recording but has recorded data (due to set duration), it will just return the generated USoundWave.
		void StopRecording(TArray<USoundWave*>& OutSoundWaves);

		// Called by RtAudio when a new audio buffer is ready to be supplied.
		int32 OnAudioCapture(void* InBuffer, uint32 InBufferFrames, double StreamTime, bool bOverflow);

	private:

		// Private Constructor
		FAudioRecordingManager();

		// Private Destructor
		~FAudioRecordingManager();

		// The default mic sample rate
		const int32 WAVE_FILE_SAMPLERATE = 44100;

		// Copy of the input recording settings
		FRecordingSettings Settings;

		// RtAudio ADC object -- used to interact with low-level audio device.
		FRtAudioInputWrapper ADC;

		// Stream parameters to initialize the ADC
		FRtAudioInputWrapper::FStreamParameters StreamParams;

		// Critical section used to stop and retrieve finished audio buffers.
		FCriticalSection CriticalSection;

		// The data which is currently being recorded to, if the manager is actively recording. This is not safe to access while recording.
		TArray<int16> CurrentRecordedPCMData;

		// Buffer to store sample rate converted PCM data
		TArray<int16> ConvertedPCMData;

		// Buffers to de-interleave recorded audio
		struct FDeinterleavedAudio
		{
			TArray<int16> PCMData;
		};
		TArray<FDeinterleavedAudio> DeinterleavedAudio;

		// Reusable raw wave data buffer to generate .wav file formats
		TArray<uint8> RawWaveData;

		// The number of frames that have been recorded
		int32 NumRecordedSamples;

		// The number of frames to record if recording a set duration
		int32 NumFramesToRecord;

		// Recording block size (number of frames per callback block)
		int32 RecordingBlockSize;

		// The sample rate used in the recording
		float RecordingSampleRate;

		// Num input channels
		int32 NumInputChannels;

		// A linear gain to apply on mic input
		float InputGain;

		// Whether or not the manager is actively recording.
		FThreadSafeBool bRecording;

		// Number of overflows detected while recording
		int32 NumOverflowsDetected;

		// Whether or not we have an error
		uint32 bError : 1;

	};

}
