// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "Sound/SoundWaveProcedural.h"
#include "Sound/SoundGenerator.h"
#include "AudioDecompress.h"
#include "AudioMixerBuffer.h"

class USoundWave;

namespace Audio
{
	class FMixerBuffer;

	// Data needed for a procedural audio task
	struct FProceduralAudioTaskData
	{
		// The procedural sound wave ptr to use to generate audio with
		// TODO: remove the need for this
		USoundWave* ProceduralSoundWave;

		// The sound generator to use to generate audio
		ISoundGeneratorPtr SoundGenerator;

		// The audio buffer to fill from the results of the generation
		float* AudioData;

		// The size of the audio buffer
		int32 NumSamples;

		// The number of channels of the procedural buffer
		int32 NumChannels;

		// Force decodes to execute synchronously
		bool bForceSyncDecode;

		FProceduralAudioTaskData()
			: ProceduralSoundWave(nullptr)
			, AudioData(nullptr)
			, NumSamples(0)
			, NumChannels(0)
			, bForceSyncDecode(false)
		{}
	};

	// Data needed for a decode audio task
	struct FDecodeAudioTaskData
	{
		// A pointer to a buffer of audio which will be decoded to
		float* AudioData;

		// Decompression state for decoder
		ICompressedAudioInfo* DecompressionState;

		// The buffer type for the decoder
		Audio::EBufferType::Type BufferType;

		// Number of channels of the decoder
		int32 NumChannels;

		// The number of frames which are precached
		int32 NumPrecacheFrames;

		// The number of frames to decode
		int32 NumFramesToDecode;

		// Whether or not this sound is intending to be looped
		bool bLoopingMode;

		// Whether or not to skip the first buffer
		bool bSkipFirstBuffer;

		// Force this decoding operation to occur synchronously,
		// regardless of the value of au.ForceSyncAudioDecodes. (used by time synth)
		bool bForceSyncDecode;

		FDecodeAudioTaskData()
			: AudioData(nullptr)
			, DecompressionState(nullptr)
			, BufferType(Audio::EBufferType::Invalid)
			, NumChannels(0)
			, NumPrecacheFrames(0)
			, NumFramesToDecode(0)
			, bLoopingMode(false)
			, bSkipFirstBuffer(false)
			, bForceSyncDecode(false)
		{}
	};

	// Data needed for a header parse audio task
	struct FHeaderParseAudioTaskData
	{
		// The mixer buffer object which results will be written to
		FMixerBuffer* MixerBuffer;

		// The sound wave object which contains the encoded file
		USoundWave* SoundWave;

		FHeaderParseAudioTaskData()
			: MixerBuffer(nullptr)
			, SoundWave(nullptr)
		{}
	};

	// Results from procedural audio task
	struct FProceduralAudioTaskResults
	{
		int32 NumSamplesWritten;
		bool bIsFinished;

#if ENABLE_AUDIO_DEBUG
		double CPUDuration = 0.0;
#endif // if ENABLE_AUDIO_DEBUG

		FProceduralAudioTaskResults()
			: NumSamplesWritten(0)
			, bIsFinished(false)
		{}
	};

	// Results from decode audio task
	struct FDecodeAudioTaskResults
	{

		// Whether or not the audio buffer looped
		bool bIsFinishedOrLooped;

#if ENABLE_AUDIO_DEBUG
		double CPUDuration = 0;
#endif // if ENABLE_AUDIO_DEBUG

		FDecodeAudioTaskResults()
			: bIsFinishedOrLooped(false)
		{}
	};

	// The types of audio tasks
	enum class EAudioTaskType
	{
		// The job is a procedural sound wave job to generate more audio
		Procedural,

		// The job is a header decode job
		Header,

		// The job is a decode job
		Decode,

		// The job is invalid (or unknown)
		Invalid,
	};

	// Handle to an in-flight decode job. Can be queried and used on any thread.
	class IAudioTask
	{
	public:
		virtual ~IAudioTask() {}

		// Queries if the decode job has finished.
		virtual bool IsDone() const = 0;

		// Returns the job type of the handle.
		virtual EAudioTaskType GetType() const = 0;

		// Ensures the completion of the decode operation.
		virtual void EnsureCompletion() = 0;

		// Cancel the decode operation
		virtual void CancelTask() = 0;

		// Returns the result of a procedural sound generate job
		virtual void GetResult(FProceduralAudioTaskResults& OutResult) {};

		// Returns the result of a decode job
		virtual void GetResult(FDecodeAudioTaskResults& OutResult) {};
	};

	// Creates a task to decode a decoded file header
	IAudioTask* CreateAudioTask(Audio::FDeviceId InDeviceId, const FHeaderParseAudioTaskData& InJobData);

	// Creates a task for a procedural sound wave generation
	IAudioTask* CreateAudioTask(Audio::FDeviceId InDeviceId, const FProceduralAudioTaskData& InJobData);

	// Creates a task to decode a chunk of audio
	IAudioTask* CreateAudioTask(Audio::FDeviceId InDeviceId, const FDecodeAudioTaskData& InJobData);

	// Creates a queue for audio decode requests with a specific Id. Tasks
	// created with this Id will not be started immediately upon creation,
	// but will instead be queued up to await a start "kick" later. NOTE:
	// "kicking" the queue is the responsibility of the system that creates 
	// the queue, typically someplace like in a FOnAudioDevicePostRender delegate! 
	void CreateSynchronizedAudioTaskQueue(AudioTaskQueueId QueueId);

	// Destroys an audio decode task queue. Tasks currently queued up are 
	// optionally started.
	void DestroySynchronizedAudioTaskQueue(AudioTaskQueueId QueueId, bool RunCurrentQueue = false);

	// "Kicks" all of the audio decode tasks currentlyt in the specified queue.
	int KickQueuedTasks(AudioTaskQueueId QueueId);

}
