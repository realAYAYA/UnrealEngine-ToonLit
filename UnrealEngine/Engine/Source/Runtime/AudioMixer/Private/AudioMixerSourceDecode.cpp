// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioMixerSourceDecode.h"
#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "AudioMixer.h"
#include "Sound/SoundWaveProcedural.h"
#include "HAL/RunnableThread.h"
#include "AudioMixerBuffer.h"
#include "Async/Async.h"
#include "AudioDecompress.h"
#include "Sound/SoundGenerator.h"
#include "DSP/FloatArrayMath.h"

static int32 ForceSyncAudioDecodesCvar = 0;
FAutoConsoleVariableRef CVarForceSyncAudioDecodes(
	TEXT("au.ForceSyncAudioDecodes"),
	ForceSyncAudioDecodesCvar,
	TEXT("Disables using async tasks for processing sources.\n")
	TEXT("0: Not Disabled, 1: Disabled"),
	ECVF_Default);

static int32 ForceSynchronizedAudioTaskKickCvar = 0;
FAutoConsoleVariableRef CVarForceSynchronizedAudioTaskKick(
	TEXT("au.ForceSynchronizedAudioTaskKick"),
	ForceSynchronizedAudioTaskKickCvar,
	TEXT("Force all Audio Tasks created in one \"audio render frame\" to be queued until they can all be \"kicked\" at once at the end of the frame.\n")
	TEXT("0: Don't Force, 1: Force"),
	ECVF_Default);

namespace Audio
{

class FAsyncDecodeWorker : public FNonAbandonableTask
{
#if ENABLE_AUDIO_DEBUG
	struct FScopeDecodeTimer
	{
		FScopeDecodeTimer(double* OutResultSeconds)
		: Result(OutResultSeconds)
		{
			StartCycle = FPlatformTime::Cycles64();
		}
		~FScopeDecodeTimer()
		{
			uint64 EndCycle = FPlatformTime::Cycles64();
			if (Result)
			{
				*Result = static_cast<double>(EndCycle - StartCycle) * FPlatformTime::GetSecondsPerCycle64();
			}
		}

		double* Result = nullptr;
		uint64 StartCycle = 0;
	};

#endif // if ENABLE_AUDIO_DEBUG

public:
	FAsyncDecodeWorker(const FHeaderParseAudioTaskData& InTaskData)
		: HeaderParseAudioData(InTaskData)
		, TaskType(EAudioTaskType::Header)
		, bIsDone(false)
	{
	}

	FAsyncDecodeWorker(const FProceduralAudioTaskData& InTaskData)
		: ProceduralTaskData(InTaskData)
		, TaskType(EAudioTaskType::Procedural)
		, bIsDone(false)
	{
	}

	FAsyncDecodeWorker(const FDecodeAudioTaskData& InTaskData)
		: DecodeTaskData(InTaskData)
		, TaskType(EAudioTaskType::Decode)
		, bIsDone(false)
	{
	}

	~FAsyncDecodeWorker()
	{
	}

	void DoWork()
	{
		switch (TaskType)
		{
			case EAudioTaskType::Procedural:
			{
#if ENABLE_AUDIO_DEBUG
				FScopeDecodeTimer Timer(&ProceduralResult.CPUDuration);
#endif // if ENABLE_AUDIO_DEBUG
				QUICK_SCOPE_CYCLE_COUNTER(STAT_FAsyncDecodeWorker_Procedural);
				if (ProceduralTaskData.SoundGenerator.IsValid())
				{
					// Generators are responsible to zero memory in case they can't generate the requested amount of samples
					ProceduralResult.NumSamplesWritten = ProceduralTaskData.SoundGenerator->GetNextBuffer(ProceduralTaskData.AudioData, ProceduralTaskData.NumSamples);
					ProceduralResult.bIsFinished = ProceduralTaskData.SoundGenerator->IsFinished();
				}
				else
				{
					// Make sure we've been flagged as active
					if (!ProceduralTaskData.ProceduralSoundWave || !ProceduralTaskData.ProceduralSoundWave->IsGeneratingAudio())
					{
						// Act as if we generated audio, but return silence.
						FMemory::Memzero(ProceduralTaskData.AudioData, ProceduralTaskData.NumSamples * sizeof(float));
						ProceduralResult.NumSamplesWritten = ProceduralTaskData.NumSamples;
						return;
					}

					// If we're not a float format, we need to convert the format to float
					const EAudioMixerStreamDataFormat::Type FormatType = ProceduralTaskData.ProceduralSoundWave->GetGeneratedPCMDataFormat();
					if (FormatType != EAudioMixerStreamDataFormat::Float)
					{
						check(FormatType == EAudioMixerStreamDataFormat::Int16);

						int32 NumChannels = ProceduralTaskData.NumChannels;
						int32 ByteSize = NumChannels * ProceduralTaskData.NumSamples * sizeof(int16);

						TArray<uint8> DecodeBuffer;
						DecodeBuffer.AddUninitialized(ByteSize);

						const int32 NumBytesWritten = ProceduralTaskData.ProceduralSoundWave->GeneratePCMData(DecodeBuffer.GetData(), ProceduralTaskData.NumSamples);

						check(NumBytesWritten <= ByteSize);

						ProceduralResult.NumSamplesWritten = NumBytesWritten / sizeof(int16);
						Audio::ArrayPcm16ToFloat(MakeArrayView((int16*)DecodeBuffer.GetData(), ProceduralResult.NumSamplesWritten)
							, MakeArrayView(ProceduralTaskData.AudioData, ProceduralResult.NumSamplesWritten));
					}
					else
					{
						const int32 NumBytesWritten = ProceduralTaskData.ProceduralSoundWave->GeneratePCMData((uint8*)ProceduralTaskData.AudioData, ProceduralTaskData.NumSamples);
						ProceduralResult.NumSamplesWritten = NumBytesWritten / sizeof(float);
					}
				}
			}
			break;

			case EAudioTaskType::Header:
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_FAsyncDecodeWorker_Header);
				HeaderParseAudioData.MixerBuffer->ReadCompressedInfo(HeaderParseAudioData.SoundWave);
			}
			break;

			case EAudioTaskType::Decode:
			{
#if ENABLE_AUDIO_DEBUG
				FScopeDecodeTimer Timer(&DecodeResult.CPUDuration);
#endif // if ENABLE_AUDIO_DEBUG
				QUICK_SCOPE_CYCLE_COUNTER(STAT_FAsyncDecodeWorker_Decode);
				int32 NumChannels = DecodeTaskData.NumChannels;
				int32 ByteSize = NumChannels * DecodeTaskData.NumFramesToDecode * sizeof(int16);

				// Create a buffer to decode into that's of the appropriate size
				TArray<uint8> DecodeBuffer;
				DecodeBuffer.AddZeroed(ByteSize);

				// skip the first buffers if we've already decoded them during Precache:
				if (DecodeTaskData.bSkipFirstBuffer)
				{
					const int32 kPCMBufferSize = NumChannels * DecodeTaskData.NumPrecacheFrames * sizeof(int16);
					int32 NumBytesStreamed = kPCMBufferSize;
					if (DecodeTaskData.BufferType == EBufferType::Streaming)
					{
						for (int32 NumberOfBuffersToSkip = 0; NumberOfBuffersToSkip < PLATFORM_NUM_AUDIODECOMPRESSION_PRECACHE_BUFFERS; NumberOfBuffersToSkip++)
						{
							DecodeTaskData.DecompressionState->StreamCompressedData(DecodeBuffer.GetData(), DecodeTaskData.bLoopingMode, kPCMBufferSize, NumBytesStreamed);
						}
					}
					else
					{
						for (int32 NumberOfBuffersToSkip = 0; NumberOfBuffersToSkip < PLATFORM_NUM_AUDIODECOMPRESSION_PRECACHE_BUFFERS; NumberOfBuffersToSkip++)
						{
							DecodeTaskData.DecompressionState->ReadCompressedData(DecodeBuffer.GetData(), DecodeTaskData.bLoopingMode, kPCMBufferSize);
						}
					}
				}

				const int32 kPCMBufferSize = NumChannels * DecodeTaskData.NumFramesToDecode * sizeof(int16);
				int32 NumBytesStreamed = kPCMBufferSize;
				if (DecodeTaskData.BufferType == EBufferType::Streaming)
				{
					DecodeResult.bIsFinishedOrLooped = DecodeTaskData.DecompressionState->StreamCompressedData(DecodeBuffer.GetData(), DecodeTaskData.bLoopingMode, kPCMBufferSize, NumBytesStreamed);
				}
				else
				{
					DecodeResult.bIsFinishedOrLooped = DecodeTaskData.DecompressionState->ReadCompressedData(DecodeBuffer.GetData(), DecodeTaskData.bLoopingMode, kPCMBufferSize);
				}

				// Convert the decoded PCM data into a float buffer while still in the async task
				Audio::ArrayPcm16ToFloat(MakeArrayView((int16*)DecodeBuffer.GetData(), DecodeTaskData.NumFramesToDecode * NumChannels)
					, MakeArrayView(DecodeTaskData.AudioData, DecodeTaskData.NumFramesToDecode* NumChannels));
			}
			break;
		}
		bIsDone = true;
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncDecodeWorker, STATGROUP_ThreadPoolAsyncTasks);
	}

	FHeaderParseAudioTaskData HeaderParseAudioData;
	FDecodeAudioTaskData DecodeTaskData;
	FDecodeAudioTaskResults DecodeResult;
	FProceduralAudioTaskData ProceduralTaskData;
	FProceduralAudioTaskResults ProceduralResult;
	EAudioTaskType TaskType;
	FThreadSafeBool bIsDone;
};

class FDecodeHandleBase : public IAudioTask
{
public:
	FDecodeHandleBase()
		: Task(nullptr)
	{}

	virtual ~FDecodeHandleBase()
	{
		if (Task)
		{
			Task->EnsureCompletion(/*bIsLatencySensitive =*/ true);
			delete Task;
		}
	}

	virtual bool IsDone() const override
	{
		if (Task)
		{
			return Task->IsDone();
		}
		return true;
	}

	virtual void EnsureCompletion() override
	{
		if (Task)
		{
			Task->EnsureCompletion(/*bIsLatencySensitive =*/ true);
		}
	}

	virtual void CancelTask() override
	{
		if (Task)
		{
			// If Cancel returns false, it means we weren't able to cancel. So lets then fallback to ensure complete.
			if (!Task->Cancel())
			{
				Task->EnsureCompletion(/*bIsLatencySensitive =*/ true);
			}
		}
	}

protected:

	FAsyncTask<FAsyncDecodeWorker>* Task;
};

class FHeaderDecodeHandle : public FDecodeHandleBase
{
public:
	FHeaderDecodeHandle(const FHeaderParseAudioTaskData& InJobData)
	{
		Task = new FAsyncTask<FAsyncDecodeWorker>(InJobData);
        if (ForceSyncAudioDecodesCvar)
        {
            Task->StartSynchronousTask();
            return;
        }
        
		Task->StartBackgroundTask();
	}

	virtual EAudioTaskType GetType() const override
	{
		return EAudioTaskType::Header;
	}
};

class FProceduralDecodeHandle : public FDecodeHandleBase
{
public:
	FProceduralDecodeHandle(const FProceduralAudioTaskData& InJobData)
	{
		Task = new FAsyncTask<FAsyncDecodeWorker>(InJobData);
        if (ForceSyncAudioDecodesCvar || InJobData.bForceSyncDecode)
        {
            Task->StartSynchronousTask();
            return;
        }
        
		// We tried using the background priority thread pool
		// like other async audio decodes
		// but that resulted in underruns, see FORT-700578
		Task->StartBackgroundTask();
	}

	virtual EAudioTaskType GetType() const override
	{ 
		return EAudioTaskType::Procedural; 
	}

	virtual void GetResult(FProceduralAudioTaskResults& OutResult) override
	{
		Task->EnsureCompletion();
		const FAsyncDecodeWorker& DecodeWorker = Task->GetTask();
		OutResult = DecodeWorker.ProceduralResult;
	}
};

class FSynchronizedProceduralDecodeHandle : public FDecodeHandleBase
{
public:
	FSynchronizedProceduralDecodeHandle(const FProceduralAudioTaskData& InJobData, AudioTaskQueueId InQueueId)
	{
		Task = new FAsyncTask<FAsyncDecodeWorker>(InJobData);
		QueueId = InQueueId; 
		{
			FScopeLock Lock(&SynchronizationQuequesLockCs);
			TArray<FAsyncTask<FAsyncDecodeWorker>*>* Queue = ProceduralRenderingSynchronizationQueues.Find(QueueId);
			if (Queue)
			{
				Queue->Add(Task);
				return;
			}
		}

		// failed to queue it up, so do a normal start...
		QueueId = 0;
		if (ForceSyncAudioDecodesCvar || InJobData.bForceSyncDecode)
		{
			Task->StartSynchronousTask();
			return;
		}
		Task->StartBackgroundTask();
	}

	virtual EAudioTaskType GetType() const override
	{
		return EAudioTaskType::Procedural;
	}

	virtual bool IsDone() const override
	{
		if (IsQueued())
		{
			return false;
		}
		return FDecodeHandleBase::IsDone();
	}

	bool IsQueued() const
	{
		if (!QueueId)
		{
			return false;
		}
		FScopeLock Lock(&SynchronizationQuequesLockCs);
		TArray<FAsyncTask<FAsyncDecodeWorker>*>* Queue = ProceduralRenderingSynchronizationQueues.Find(QueueId);
		return (Queue && Queue->Find(Task) != INDEX_NONE);
	}

	bool Dequeue(bool Run)
	{
		FScopeLock Lock(&SynchronizationQuequesLockCs);
		TArray<FAsyncTask<FAsyncDecodeWorker>*>* Queue = ProceduralRenderingSynchronizationQueues.Find(QueueId);
		if (!Queue)
		{
			return false;
		}
		int NumRemoved = Queue->Remove(Task);
		if (NumRemoved > 0)
		{
			if (Run)
			{
				Task->StartBackgroundTask();
			}
			return true;
		}
		return false;
	}

	virtual void EnsureCompletion() override
	{
		{
			FScopeLock Lock(&SynchronizationQuequesLockCs);
			// For now, if this is in the queue still (not kicked)
			// we're going to pull it out of the queue and run it now.
			// This seems to only happen when first starting a sound as 
			// the system tries to decode the first chunk of audio 
			// asynchronously to the audio thread. 
			Dequeue(/* Run */ true);
			// Now we can wait on it to complete...
		}
		if (Task)
		{
			Task->EnsureCompletion(/*bIsLatencySensitive =*/ true);
		}
	}

	virtual void CancelTask() override
	{
		{
			FScopeLock Lock(&SynchronizationQuequesLockCs);
			// If we dequeue it then it never ran and we are done. Otherwise...
			if (!Dequeue(/* Run */ false))
			{
				FDecodeHandleBase::CancelTask();
			}
		}
	}

	virtual void GetResult(FProceduralAudioTaskResults& OutResult) override
	{
		Task->EnsureCompletion();
		const FAsyncDecodeWorker& DecodeWorker = Task->GetTask();
		OutResult = DecodeWorker.ProceduralResult;
	}

	static void CreateSynchronizedRenderQueued(AudioTaskQueueId QueueId)
	{
		FScopeLock Lock(&SynchronizationQuequesLockCs);
		TArray<FAsyncTask<FAsyncDecodeWorker>*>* Queue = ProceduralRenderingSynchronizationQueues.Find(QueueId);
		if (!Queue)
		{
			ProceduralRenderingSynchronizationQueues.Add(QueueId);
			ProceduralRenderingSynchronizationQueues[QueueId].Reserve(128);
		}
	}

	static void DestroySynchronizedRenderQueued(AudioTaskQueueId QueueId, bool RunCurrentQueue = false)
	{
		FScopeLock Lock(&SynchronizationQuequesLockCs);
		
		if (RunCurrentQueue)
		{
			KickQueuedTasks(QueueId);
		}

		TArray<FAsyncTask<FAsyncDecodeWorker>*>* Queue = ProceduralRenderingSynchronizationQueues.Find(QueueId);
		if (Queue)
		{
			ProceduralRenderingSynchronizationQueues.Remove(QueueId);
		}
	}

	static int KickQueuedTasks(AudioTaskQueueId QueueId)
	{
		FScopeLock Lock(&SynchronizationQuequesLockCs);
		int NumStarted = 0;
		TArray<FAsyncTask<FAsyncDecodeWorker>*>* Queue = ProceduralRenderingSynchronizationQueues.Find(QueueId);
		if (Queue)
		{
			for (auto Task : *Queue)
			{ 
				Task->StartBackgroundTask();
			}
			NumStarted = Queue->Num();
			Queue->Empty();
		}
		return NumStarted;
	}
private:
	AudioTaskQueueId QueueId = 0;

	static TMap <AudioTaskQueueId, TArray<FAsyncTask<FAsyncDecodeWorker>*>> ProceduralRenderingSynchronizationQueues;
	static FCriticalSection SynchronizationQuequesLockCs;
};

TMap <AudioTaskQueueId, TArray<FAsyncTask<FAsyncDecodeWorker>*>> FSynchronizedProceduralDecodeHandle::ProceduralRenderingSynchronizationQueues;
FCriticalSection FSynchronizedProceduralDecodeHandle::SynchronizationQuequesLockCs;

class FDecodeHandle : public FDecodeHandleBase
{
public:
	FDecodeHandle(const FDecodeAudioTaskData& InJobData)
	{
		Task = new FAsyncTask<FAsyncDecodeWorker>(InJobData);
        if (ForceSyncAudioDecodesCvar || InJobData.bForceSyncDecode)
        {
            Task->StartSynchronousTask();
            return;
        }
        
		const bool bUseBackground = ShouldUseBackgroundPoolFor_FAsyncRealtimeAudioTask();
		Task->StartBackgroundTask(bUseBackground ? GBackgroundPriorityThreadPool : GThreadPool);
	}

	virtual EAudioTaskType GetType() const override
	{ 
		return EAudioTaskType::Decode; 
	}

	virtual void GetResult(FDecodeAudioTaskResults& OutResult) override
	{
		Task->EnsureCompletion();
		const FAsyncDecodeWorker& DecodeWorker = Task->GetTask();
		OutResult = DecodeWorker.DecodeResult;
	}
};

IAudioTask* CreateAudioTask(Audio::FDeviceId InDeviceId, const FProceduralAudioTaskData& InJobData)
{
	if (ForceSynchronizedAudioTaskKickCvar || (InJobData.SoundGenerator && InJobData.SoundGenerator->GetSynchronizedRenderQueueId()))
	{
		AudioTaskQueueId QueueId = InJobData.SoundGenerator->GetSynchronizedRenderQueueId();
		if (!QueueId)
		{
			// Only use the audio device ID as the task queue id if both 
			// ForceSynchronizedAudioTaskKickCvar is true AND the caller has
			// not specified a specific task queue id in their SoundGenerator.
			QueueId = (AudioTaskQueueId)InDeviceId;
		}
		return new FSynchronizedProceduralDecodeHandle(InJobData, QueueId);
	}

	return new FProceduralDecodeHandle(InJobData);
}

IAudioTask* CreateAudioTask(Audio::FDeviceId InDeviceId, const FHeaderParseAudioTaskData& InJobData)
{
	return new FHeaderDecodeHandle(InJobData);
}

IAudioTask* CreateAudioTask(Audio::FDeviceId InDeviceId, const FDecodeAudioTaskData& InJobData)
{
	return new FDecodeHandle(InJobData);
}

void CreateSynchronizedAudioTaskQueue(AudioTaskQueueId QueueId)
{
	FSynchronizedProceduralDecodeHandle::CreateSynchronizedRenderQueued(QueueId);
}

void DestroySynchronizedAudioTaskQueue(AudioTaskQueueId QueueId, bool RunCurrentQueue)
{
	FSynchronizedProceduralDecodeHandle::DestroySynchronizedRenderQueued(QueueId, RunCurrentQueue);
}

int KickQueuedTasks(AudioTaskQueueId QueueId)
{
	return FSynchronizedProceduralDecodeHandle::KickQueuedTasks(QueueId);
}

}
