// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioMixerSourceBuffer.h"
#include "AudioMixerSourceDecode.h"
#include "ContentStreaming.h"
#include "AudioDecompress.h"
#include "Misc/ScopeTryLock.h"

namespace Audio
{
	bool FRawPCMDataBuffer::GetNextBuffer(FMixerSourceVoiceBuffer* OutSourceBufferPtr, const uint32 NumSampleToGet)
	{
		// TODO: support loop counts
		float* OutBufferPtr = OutSourceBufferPtr->AudioData.GetData();
		int16* DataPtr = (int16*)Data;

		if (LoopCount == Audio::LOOP_FOREVER)
		{
			bool bIsFinishedOrLooped = false;
			for (uint32 Sample = 0; Sample < NumSampleToGet; ++Sample)
			{
				OutBufferPtr[Sample] = DataPtr[CurrentSample++] / 32768.0f;

				// Loop around if we're looping
				if (CurrentSample >= NumSamples)
				{
					CurrentSample = 0;
					bIsFinishedOrLooped = true;
				}
			}
			return bIsFinishedOrLooped;
		}
		else if (CurrentSample < NumSamples)
		{
			uint32 Sample = 0;
			while (Sample < NumSampleToGet && CurrentSample < NumSamples)
			{
				OutBufferPtr[Sample++] = (float)DataPtr[CurrentSample++] / 32768.0f;
			}

			// Zero out the rest of the buffer
			while (Sample < NumSampleToGet)
			{
				OutBufferPtr[Sample++] = 0.0f;
			}
		}
		else
		{
			for (uint32 Sample = 0; Sample < NumSampleToGet; ++Sample)
			{
				OutBufferPtr[Sample] = 0.0f;
			}
		}

		// If the current sample is greater or equal to num samples we hit the end of the buffer
		return CurrentSample >= NumSamples;
	}

	TSharedPtr<FMixerSourceBuffer, ESPMode::ThreadSafe> FMixerSourceBuffer::Create(FMixerSourceBufferInitArgs& InArgs, TArray<FAudioParameter>&& InDefaultParams)
	{
		LLM_SCOPE(ELLMTag::AudioMixer);

		TSharedPtr<FMixerSourceBuffer, ESPMode::ThreadSafe> NewSourceBuffer = MakeShareable(new FMixerSourceBuffer(InArgs, MoveTemp(InDefaultParams)));

		return NewSourceBuffer;
	}

	FMixerSourceBuffer::FMixerSourceBuffer(FMixerSourceBufferInitArgs& InArgs, TArray<FAudioParameter>&& InDefaultParams)
		: NumBuffersQeueued(0)
		, CurrentBuffer(0)
		, SoundWave(InArgs.SoundWave)
		, AsyncRealtimeAudioTask(nullptr)
		, DecompressionState(nullptr)
		, LoopingMode(InArgs.LoopingMode)
		, NumChannels(InArgs.Buffer->NumChannels)
		, BufferType(InArgs.Buffer->GetType())
		, NumPrecacheFrames(InArgs.SoundWave->NumPrecacheFrames)
		, AuioDeviceID(InArgs.AudioDeviceID)
		, bInitialized(false)
		, bBufferFinished(false)
		, bPlayedCachedBuffer(false)
		, bIsSeeking(InArgs.bIsSeeking)
		, bLoopCallback(false)
		, bProcedural(InArgs.SoundWave->bProcedural)
		, bIsBus(InArgs.SoundWave->bIsSourceBus)
		, bForceSyncDecode(InArgs.bForceSyncDecode)
		, bHasError(false)
	{
		// TODO: remove the need to do this here. 1) remove need for decoders to depend on USoundWave and 2) remove need for procedural sounds to use USoundWaveProcedural
		InArgs.SoundWave->AddPlayingSource(this);

		// Retrieve a sound generator if this is a procedural sound wave
		if (bProcedural)
		{
			FSoundGeneratorInitParams InitParams;
			InitParams.AudioDeviceID = InArgs.AudioDeviceID;
			InitParams.SampleRate = InArgs.SampleRate;
			InitParams.AudioMixerNumOutputFrames = InArgs.AudioMixerNumOutputFrames;
			InitParams.NumChannels = NumChannels;
			InitParams.NumFramesPerCallback = MONO_PCM_BUFFER_SAMPLES;
			InitParams.InstanceID = InArgs.InstanceID;
			InitParams.bIsPreviewSound = InArgs.bIsPreviewSound;

			SoundGenerator = InArgs.SoundWave->CreateSoundGenerator(InitParams, MoveTemp(InDefaultParams));

			// In the case of procedural audio generation, the mixer source buffer will never "loop" -- i.e. when it's done, it's done
			LoopingMode = LOOP_Never;
		}

		const uint32 TotalSamples = MONO_PCM_BUFFER_SAMPLES * NumChannels;
		for (int32 BufferIndex = 0; BufferIndex < Audio::MAX_BUFFERS_QUEUED; ++BufferIndex)
		{
			SourceVoiceBuffers.Add(MakeShared<FMixerSourceVoiceBuffer, ESPMode::ThreadSafe>());

			// Prepare the memory to fit the max number of samples
			SourceVoiceBuffers[BufferIndex]->AudioData.Reset(TotalSamples);
			SourceVoiceBuffers[BufferIndex]->bRealTimeBuffer = true;
			SourceVoiceBuffers[BufferIndex]->LoopCount = 0;
		}
	}

	FMixerSourceBuffer::~FMixerSourceBuffer()
	{
		// GC methods may get called from the game thread during the destructor
		// These methods will trylock and early exit if we have this lock
		FScopeLock Lock(&SoundWaveCritSec);

		// OnEndGenerate calls EnsureTaskFinishes,
		// which will make sure we have completed our async realtime task before deleting the decompression state
		OnEndGenerate();

		// Clean up decompression state after things have been finished using it
		DeleteDecoder();

		if (SoundWave)
		{
			SoundWave->RemovePlayingSource(this);
		}
	}

	void FMixerSourceBuffer::SetDecoder(ICompressedAudioInfo* InCompressedAudioInfo)
	{
		if (DecompressionState == nullptr)
		{
			DecompressionState = InCompressedAudioInfo;
			if (BufferType == EBufferType::Streaming)
			{
				IStreamingManager::Get().GetAudioStreamingManager().AddDecoder(DecompressionState);
			}
		}
	}

	void FMixerSourceBuffer::SetPCMData(const FRawPCMDataBuffer& InPCMDataBuffer)
	{
		check(BufferType == EBufferType::PCM || BufferType == EBufferType::PCMPreview);
		RawPCMDataBuffer = InPCMDataBuffer;
	}

	void FMixerSourceBuffer::SetCachedRealtimeFirstBuffers(TArray<uint8>&& InPrecachedBuffers)
	{
		CachedRealtimeFirstBuffer = MoveTemp(InPrecachedBuffers);
	}

	bool FMixerSourceBuffer::Init()
	{
		// We have successfully initialized which means our SoundWave has been flagged as bIsActive
		// GC can run between PreInit and Init so when cleaning up FMixerSourceBuffer, we don't want to touch SoundWave unless bInitailized is true.
		// SoundWave->bIsSoundActive will prevent GC until it is released in audio render thread
		bInitialized = true;

		switch (BufferType)
		{
		case EBufferType::PCM:
		case EBufferType::PCMPreview:
			SubmitInitialPCMBuffers();
			break;

		case EBufferType::PCMRealTime:
		case EBufferType::Streaming:
			SubmitInitialRealtimeBuffers();
			break;

		case EBufferType::Invalid:
			break;
		}

		return true;
	}

	void FMixerSourceBuffer::OnBufferEnd()
	{
		FScopeTryLock Lock(&SoundWaveCritSec);

		if (!Lock.IsLocked() || (NumBuffersQeueued == 0 && bBufferFinished) || (bProcedural && !SoundWave) || (bHasError))
		{
			return;
		}

		ProcessRealTimeSource();
	}

	int32 FMixerSourceBuffer::GetNumBuffersQueued() const
	{
		FScopeTryLock Lock(&SoundWaveCritSec);
		if (Lock.IsLocked())
		{
			return NumBuffersQeueued;
		}

		return 0;
	}

	TSharedPtr<FMixerSourceVoiceBuffer, ESPMode::ThreadSafe> FMixerSourceBuffer::GetNextBuffer()
	{
		FScopeTryLock Lock(&SoundWaveCritSec);
		if (!Lock.IsLocked())
		{
			return nullptr;
		}

		TSharedPtr<FMixerSourceVoiceBuffer, ESPMode::ThreadSafe> NewBufferPtr;
		BufferQueue.Dequeue(NewBufferPtr);
		--NumBuffersQeueued;
		return NewBufferPtr;
	}

	void FMixerSourceBuffer::SubmitInitialPCMBuffers()
	{
		CurrentBuffer = 0;

		RawPCMDataBuffer.NumSamples = RawPCMDataBuffer.DataSize / sizeof(int16);
		RawPCMDataBuffer.CurrentSample = 0;

		// Only submit data if we've successfully loaded it
		if (!RawPCMDataBuffer.Data || !RawPCMDataBuffer.DataSize)
		{
			return;
		}

		RawPCMDataBuffer.LoopCount = (LoopingMode != LOOP_Never) ? Audio::LOOP_FOREVER : 0;

		// Submit the first two format-converted chunks to the source voice
		const uint32 NumSamplesPerBuffer = MONO_PCM_BUFFER_SAMPLES * NumChannels;
		int16* RawPCMBufferDataPtr = (int16*)RawPCMDataBuffer.Data;

		// Prepare the buffer for the PCM submission
		SourceVoiceBuffers[0]->AudioData.Reset(NumSamplesPerBuffer);
		SourceVoiceBuffers[0]->AudioData.AddZeroed(NumSamplesPerBuffer);

		RawPCMDataBuffer.GetNextBuffer(SourceVoiceBuffers[0].Get(), NumSamplesPerBuffer);

		SubmitBuffer(SourceVoiceBuffers[0]);

		CurrentBuffer = 1;
	}

	void FMixerSourceBuffer::SubmitInitialRealtimeBuffers()
	{
		static_assert(PLATFORM_NUM_AUDIODECOMPRESSION_PRECACHE_BUFFERS <= 2 && PLATFORM_NUM_AUDIODECOMPRESSION_PRECACHE_BUFFERS >= 0, "Unsupported number of precache buffers.");

		CurrentBuffer = 0;

		bPlayedCachedBuffer = false;
		if (!bIsSeeking && CachedRealtimeFirstBuffer.Num() > 0)
		{
			bPlayedCachedBuffer = true;

			const uint32 NumSamples = NumPrecacheFrames * NumChannels;
			const uint32 BufferSize = NumSamples * sizeof(int16);

			// Format convert the first cached buffers
#if (PLATFORM_NUM_AUDIODECOMPRESSION_PRECACHE_BUFFERS == 2)
			{
				// Prepare the precache buffer memory
				for (int32 i = 0; i < 2; ++i)
				{
					SourceVoiceBuffers[i]->AudioData.Reset();
					SourceVoiceBuffers[i]->AudioData.AddZeroed(NumSamples);
				}

				int16* CachedBufferPtr0 = (int16*)CachedRealtimeFirstBuffer.GetData();
				int16* CachedBufferPtr1 = (int16*)(CachedRealtimeFirstBuffer.GetData() + BufferSize);
				float* AudioData0 = SourceVoiceBuffers[0]->AudioData.GetData();
				float* AudioData1 = SourceVoiceBuffers[1]->AudioData.GetData();
				for (uint32 Sample = 0; Sample < NumSamples; ++Sample)
				{
					AudioData0[Sample] = CachedBufferPtr0[Sample] / 32768.0f;
				}

				for (uint32 Sample = 0; Sample < NumSamples; ++Sample)
				{
					AudioData1[Sample] = CachedBufferPtr1[Sample] / 32768.0f;
				}

				// Submit the already decoded and cached audio buffers
				SubmitBuffer(SourceVoiceBuffers[0]);
				SubmitBuffer(SourceVoiceBuffers[1]);

				CurrentBuffer = 2;
			}
#elif (PLATFORM_NUM_AUDIODECOMPRESSION_PRECACHE_BUFFERS == 1)
			{
				SourceVoiceBuffers[0]->AudioData.Reset();
				SourceVoiceBuffers[0]->AudioData.AddZeroed(NumSamples);

				int16* CachedBufferPtr0 = (int16*)CachedRealtimeFirstBuffer.GetData();

				float* AudioData0 = SourceVoiceBuffers[0]->AudioData.GetData();
				for (uint32 Sample = 0; Sample < NumSamples; ++Sample)
				{
					AudioData0[Sample] = CachedBufferPtr0[Sample] / 32768.0f;
				}

				// Submit the already decoded and cached audio buffers
				SubmitBuffer(SourceVoiceBuffers[0]);

				CurrentBuffer = 1;
			}
#endif
		}
		else if (!bIsBus)
		{
			ProcessRealTimeSource();
		}
	}

	bool FMixerSourceBuffer::ReadMoreRealtimeData(ICompressedAudioInfo* InDecoder, const int32 BufferIndex, EBufferReadMode BufferReadMode)
	{
		const int32 MaxSamples = MONO_PCM_BUFFER_SAMPLES * NumChannels;

		SourceVoiceBuffers[BufferIndex]->AudioData.Reset();
		SourceVoiceBuffers[BufferIndex]->AudioData.AddZeroed(MaxSamples);

		if (bProcedural)
		{
			FScopeTryLock Lock(&SoundWaveCritSec);
			
			if (Lock.IsLocked() && SoundWave)
			{
				FProceduralAudioTaskData NewTaskData;

				// Pass the generator instance to the async task
				if (SoundGenerator.IsValid())
				{
					NewTaskData.SoundGenerator = SoundGenerator;
				}
				else
				{
					// Otherwise pass the raw sound wave procedural ptr.
					check(SoundWave && SoundWave->bProcedural);
					NewTaskData.ProceduralSoundWave = SoundWave;
				}

				NewTaskData.AudioData = SourceVoiceBuffers[BufferIndex]->AudioData.GetData();
				NewTaskData.NumSamples = MaxSamples;
				NewTaskData.NumChannels = NumChannels;
				check(!AsyncRealtimeAudioTask);
				AsyncRealtimeAudioTask = CreateAudioTask(AuioDeviceID, NewTaskData);
			}

			return false;
		}
		else if (BufferType != EBufferType::PCMRealTime && BufferType != EBufferType::Streaming)
		{
			check(RawPCMDataBuffer.Data != nullptr);

			// Read the next raw PCM buffer into the source buffer index. This converts raw PCM to float.
			return RawPCMDataBuffer.GetNextBuffer(SourceVoiceBuffers[BufferIndex].Get(), MaxSamples);
		}

		// Handle the case that the decoder has an error and can't continue.
		if (InDecoder && InDecoder->HasError())
		{			
			UE_LOG(LogAudioMixer, Warning, TEXT("Decoder Error, stopping source [%s]"), 
				*InDecoder->GetStreamingSoundWave()->GetFName().ToString());
			bHasError = true;
			bBufferFinished = true;
			return false;	
		}

		check(InDecoder != nullptr);

		FDecodeAudioTaskData NewTaskData;
		NewTaskData.AudioData = SourceVoiceBuffers[BufferIndex]->AudioData.GetData();
		NewTaskData.DecompressionState = InDecoder;
		NewTaskData.BufferType = BufferType;
		NewTaskData.NumChannels = NumChannels;
		NewTaskData.bLoopingMode = LoopingMode != LOOP_Never;
		NewTaskData.bSkipFirstBuffer = (BufferReadMode == EBufferReadMode::AsynchronousSkipFirstFrame);
		NewTaskData.NumFramesToDecode = MONO_PCM_BUFFER_SAMPLES;
		NewTaskData.NumPrecacheFrames = NumPrecacheFrames;
		NewTaskData.bForceSyncDecode = bForceSyncDecode;

		FScopeLock Lock(&DecodeTaskCritSec);
		check(!AsyncRealtimeAudioTask);
		AsyncRealtimeAudioTask = CreateAudioTask(AuioDeviceID, NewTaskData);

		return false;
	}

	void FMixerSourceBuffer::SubmitRealTimeSourceData(const bool bInIsFinishedOrLooped)
	{
		// Have we reached the end of the sound
		if (bInIsFinishedOrLooped)
		{
			switch (LoopingMode)
			{
				case LOOP_Never:
					// Play out any queued buffers - once there are no buffers left, the state check at the beginning of IsFinished will fire
					bBufferFinished = true;
					break;

				case LOOP_WithNotification:
					// If we have just looped, and we are looping, send notification
					// This will trigger a WaveInstance->NotifyFinished() in the FXAudio2SoundSournce::IsFinished() function on main thread.
					bLoopCallback = true;
					break;

				case LOOP_Forever:
					// Let the sound loop indefinitely
					break;
			}
		}

		if (SourceVoiceBuffers[CurrentBuffer]->AudioData.Num() > 0)
		{
			SubmitBuffer(SourceVoiceBuffers[CurrentBuffer]);
		}
	}

	void FMixerSourceBuffer::ProcessRealTimeSource()
	{
		FScopeLock Lock(&DecodeTaskCritSec);
		if (AsyncRealtimeAudioTask)
		{
			AsyncRealtimeAudioTask->EnsureCompletion();

			bool bIsFinishedOrLooped = false;

			switch (AsyncRealtimeAudioTask->GetType())
			{
				case EAudioTaskType::Decode:
				{
					FDecodeAudioTaskResults TaskResult;
					AsyncRealtimeAudioTask->GetResult(TaskResult);
					bIsFinishedOrLooped = TaskResult.bIsFinishedOrLooped;
				}
				break;

				case EAudioTaskType::Procedural:
				{
					FProceduralAudioTaskResults TaskResult;
					AsyncRealtimeAudioTask->GetResult(TaskResult);

					SourceVoiceBuffers[CurrentBuffer]->AudioData.SetNum(TaskResult.NumSamplesWritten);
					bIsFinishedOrLooped = TaskResult.bIsFinished;
				}
				break;
			}

			delete AsyncRealtimeAudioTask;
			AsyncRealtimeAudioTask = nullptr;

			SubmitRealTimeSourceData(bIsFinishedOrLooped);
		}

		if (!AsyncRealtimeAudioTask)
		{
			// Update the buffer index
			if (++CurrentBuffer > 2)
			{
				CurrentBuffer = 0;
			}

			EBufferReadMode DataReadMode;
			if (bPlayedCachedBuffer)
			{
				bPlayedCachedBuffer = false;
				DataReadMode = EBufferReadMode::AsynchronousSkipFirstFrame;
			}
			else
			{
				DataReadMode = EBufferReadMode::Asynchronous;
			}

			const bool bIsFinishedOrLooped = ReadMoreRealtimeData(DecompressionState, CurrentBuffer, DataReadMode);

			// If this was a synchronous read, then immediately write it
			if (AsyncRealtimeAudioTask == nullptr && !bHasError)
			{
				SubmitRealTimeSourceData(bIsFinishedOrLooped);
			}
		}
	}

	void FMixerSourceBuffer::SubmitBuffer(TSharedPtr<FMixerSourceVoiceBuffer, ESPMode::ThreadSafe> InSourceVoiceBuffer)
	{
		NumBuffersQeueued++;
		BufferQueue.Enqueue(InSourceVoiceBuffer);
	}

	void FMixerSourceBuffer::DeleteDecoder()
	{
		// Clean up decompression state after things have been finished using it
		if (DecompressionState)
		{
			if (BufferType == EBufferType::Streaming)
			{
				IStreamingManager::Get().GetAudioStreamingManager().RemoveDecoder(DecompressionState);
			}

			delete DecompressionState;
			DecompressionState = nullptr;
		}
	}

	bool FMixerSourceBuffer::OnBeginDestroy(USoundWave* /*Wave*/)
	{
		FScopeTryLock Lock(&SoundWaveCritSec);

		// if we don't have the lock, it means we are in ~FMixerSourceBuffer() on another thread
		if (Lock.IsLocked() && SoundWave)
		{
			EnsureAsyncTaskFinishes();
			DeleteDecoder();
			ClearWave();
			return true;
		}

		return false;
	}

	bool FMixerSourceBuffer::OnIsReadyForFinishDestroy(USoundWave* /*Wave*/) const
	{
		return false;
	}

	void FMixerSourceBuffer::OnFinishDestroy(USoundWave* /*Wave*/)
	{
		EnsureAsyncTaskFinishes();
		FScopeTryLock Lock(&SoundWaveCritSec);

		// if we don't have the lock, it means we are in ~FMixerSourceBuffer() on another thread
		if (Lock.IsLocked() && SoundWave)
		{
			DeleteDecoder();
			ClearWave();
		}
	}

	bool FMixerSourceBuffer::IsAsyncTaskInProgress() const
	{ 
		FScopeLock Lock(&DecodeTaskCritSec);
		return AsyncRealtimeAudioTask != nullptr;
	}

	bool FMixerSourceBuffer::IsAsyncTaskDone() const
	{
		FScopeLock Lock(&DecodeTaskCritSec);
		if (AsyncRealtimeAudioTask)
		{
			return AsyncRealtimeAudioTask->IsDone();
		}
		return true;
	}

	bool FMixerSourceBuffer::IsGeneratorFinished() const
	{
		return bProcedural && SoundGenerator.IsValid() && SoundGenerator->IsFinished();
	}

	void FMixerSourceBuffer::EnsureAsyncTaskFinishes()
	{
		FScopeLock Lock(&DecodeTaskCritSec);
		if (AsyncRealtimeAudioTask)
		{
			AsyncRealtimeAudioTask->CancelTask();

			delete AsyncRealtimeAudioTask;
			AsyncRealtimeAudioTask = nullptr;
		}
	}

	void FMixerSourceBuffer::OnBeginGenerate()
	{
		FScopeTryLock Lock(&SoundWaveCritSec);
		if (!Lock.IsLocked())
		{
			return;
		}

		if (SoundGenerator.IsValid())
		{
			SoundGenerator->OnBeginGenerate();
		}
		else
		{
			if (SoundWave && bProcedural)
			{
				check(SoundWave && SoundWave->bProcedural);
				SoundWave->OnBeginGenerate();
			}
		}
	}

	void FMixerSourceBuffer::OnEndGenerate()
	{
		// Make sure the async task finishes!
		EnsureAsyncTaskFinishes();

		FScopeTryLock Lock(&SoundWaveCritSec);
		if (!Lock.IsLocked())
		{
			return;
		}

		if (SoundGenerator.IsValid())
		{
			SoundGenerator->OnEndGenerate();
		}
		else
		{
			// Only need to call OnEndGenerate and access SoundWave here if we successfully initialized
			if (SoundWave && bInitialized && bProcedural)
			{
				check(SoundWave && SoundWave->bProcedural);
				SoundWave->OnEndGenerate();
			}
		}
	}

}
