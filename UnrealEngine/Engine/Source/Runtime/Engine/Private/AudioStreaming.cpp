// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
AudioStreaming.cpp: Implementation of audio streaming classes.
=============================================================================*/

#include "AudioStreaming.h"
#include "Audio.h"
#include "Misc/CoreStats.h"
#include "DerivedDataCacheInterface.h"
#include "Serialization/MemoryReader.h"
#include "HAL/PlatformFile.h"
#include "Async/AsyncFileHandle.h"
#include "AudioDecompress.h"

static int32 SpoofFailedStreamChunkLoad = 0;
FAutoConsoleVariableRef CVarSpoofFailedStreamChunkLoad(
	TEXT("au.SpoofFailedStreamChunkLoad"),
	SpoofFailedStreamChunkLoad,
	TEXT("Forces failing to load streamed chunks.\n")
	TEXT("0: Not Enabled, 1: Enabled"),
	ECVF_Default);

static int32 MaxConcurrentStreamsCvar = 0;
FAutoConsoleVariableRef CVarMaxConcurrentStreams(
	TEXT("au.MaxConcurrentStreams"),
	MaxConcurrentStreamsCvar,
	TEXT("Overrides the max concurrent streams.\n")
	TEXT("0: Not Overridden, >0 Overridden"),
	ECVF_Default);


/*------------------------------------------------------------------------------
	Streaming chunks from the derived data cache.
------------------------------------------------------------------------------*/

#if WITH_EDITORONLY_DATA

/** Initialization constructor. */
FAsyncStreamDerivedChunkWorker::FAsyncStreamDerivedChunkWorker(
	const FString& InDerivedDataKey,
	void* InDestChunkData,
	int32 InChunkSize,
	FThreadSafeCounter* InThreadSafeCounter,
	TFunction<void(bool)> InOnLoadCompleted
	)
	: DerivedDataKey(InDerivedDataKey)
	, DestChunkData(InDestChunkData)
	, ExpectedChunkSize(InChunkSize)
	, bRequestFailed(false)
	, ThreadSafeCounter(InThreadSafeCounter)
	, OnLoadCompleted(InOnLoadCompleted)
{
}

/** Retrieves the derived chunk from the derived data cache. */
void FAsyncStreamDerivedChunkWorker::DoWork()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FAsyncStreamDerivedChunkWorker::DoWork"), STAT_AsyncStreamDerivedChunkWorker_DoWork, STATGROUP_StreamingDetails);

	UE_LOG(LogAudio, Verbose, TEXT("Start of ASync DDC Chunk read for key: %s"), *DerivedDataKey);

	TArray<uint8> DerivedChunkData;

	if (GetDerivedDataCacheRef().GetSynchronous(*DerivedDataKey, DerivedChunkData, TEXTVIEW("Unknown Audio")))
	{
		FMemoryReader Ar(DerivedChunkData, true);
		int32 ChunkSize = 0;
		int32 AudioDataSize = 0;
		Ar << ChunkSize;
		Ar << AudioDataSize;

		// Currently, the legacy streaming manager loads in the entire, zero padded chunk, while the cached streaming manager only reads the audio data itself.
		checkf(AudioDataSize == ExpectedChunkSize || ChunkSize == ExpectedChunkSize, TEXT("Neither the padded chunk size (%d) nor the actual audio data size (%d) was equivalent to the ExpectedSize(%d)"), ChunkSize, AudioDataSize, ExpectedChunkSize);
		Ar.Serialize(DestChunkData, ExpectedChunkSize);
	}
	else
	{
		bRequestFailed = true;
	}
	FPlatformMisc::MemoryBarrier();
	ThreadSafeCounter->Decrement();

	OnLoadCompleted(bRequestFailed);
	UE_LOG(LogAudio, Verbose, TEXT("End of Async DDC Chunk Load. DDC Key: %s"), *DerivedDataKey);
}

#endif // #if WITH_EDITORONLY_DATA

////////////////////////
// FStreamingWaveData //
////////////////////////

FStreamingWaveData::FStreamingWaveData()
	: SoundWave(NULL)
	, AudioStreamingManager(nullptr)
{
}

FStreamingWaveData::~FStreamingWaveData()
{

}

void FStreamingWaveData::FreeResources()
{
	// Make sure there are no pending requests in flight.
	for (int32 Pass = 0; Pass < 3; Pass++)
	{
		BlockTillAllRequestsFinished();
		if (!UpdateStreamingStatus())
		{
			break;
		}
		check(Pass < 2); // we should be done after two passes. Pass 0 will start anything we need and pass 1 will complete those requests
	}

	for (FLoadedAudioChunk& LoadedChunk : LoadedChunks)
	{
		FreeLoadedChunk(LoadedChunk);
	}
}

bool FStreamingWaveData::Initialize(const FSoundWaveProxyPtr& InSoundWave, FLegacyAudioStreamingManager* InAudioStreamingManager)
{
	if (!ensure(InSoundWave.IsValid()) && !InSoundWave->GetNumChunks())
	{
#if WITH_EDITOR
		UE_LOG(LogAudio, Display, TEXT("Failed to initialize streaming wave data due to lack of serialized stream chunks. Error during stream cooking."));
#else
		UE_LOG(LogAudio, Warning, TEXT("Failed to initialize streaming wave data due to lack of serialized stream chunks. Error during stream cooking."));
#endif
		return false;
	}

	SoundWave = InSoundWave;
	AudioStreamingManager = InAudioStreamingManager;

	// Always get the first chunk of data so we can play immediately
	check(LoadedChunks.Num() == 0);
	check(LoadedChunkIndices.Num() == 0);

	// Prepare 4 chunks of streaming wave data in loaded chunks array
	LoadedChunks.Reset(4);

	const FStreamedAudioChunk& Chunk = SoundWave->GetChunk(0);
	const int32 DataSize = Chunk.DataSize;
	const int32 AudioDataSize = Chunk.AudioDataSize;
	const int32 FirstLoadedChunkIndex = AddNewLoadedChunk(DataSize, AudioDataSize);

	FLoadedAudioChunk* FirstChunk = &LoadedChunks[FirstLoadedChunkIndex];
	FirstChunk->Index = 0;

	// Make sure we have loaded the 0th chunk before proceeding:
	if (!SoundWave->GetChunkData(0, &FirstChunk->Data, true))
	{
		// Error/warning logging will have already been performed in the GetChunkData function
		return false;
	}

	// Set up the loaded/requested indices to be identical
	LoadedChunkIndices.Add(0);
	CurrentRequest.RequiredIndices.Add(0);

	return true;
}

bool FStreamingWaveData::UpdateStreamingStatus()
{
	bool	bHasPendingRequestInFlight = true;
	int32	RequestStatus = PendingChunkChangeRequestStatus.GetValue();
	TArray<uint32> IndicesToLoad;
	TArray<uint32> IndicesToFree;

	if (!HasPendingRequests(IndicesToLoad, IndicesToFree))
	{
		check(RequestStatus == AudioState_ReadyFor_Requests);
		bHasPendingRequestInFlight = false;
	}
	// Pending request in flight, though we might be able to finish it.
	else
	{
		if (RequestStatus == AudioState_ReadyFor_Finalization)
		{
			if (UE_LOG_ACTIVE(LogAudio, Log) && IndicesToLoad.Num() > 0)
			{
				FString LogString = FString::Printf(TEXT("Finalised loading of chunk(s) %d"), IndicesToLoad[0]);
				for (int32 Index = 1; Index < IndicesToLoad.Num(); ++Index)
				{
					LogString += FString::Printf(TEXT(", %d"), IndicesToLoad[Index]);
				}
				LogString += FString::Printf(TEXT(" from SoundWave'%s'"), *SoundWave->GetFName().ToString());
				UE_LOG(LogAudio, Log, TEXT("%s"), *LogString);
			}

			bool bFailedRequests = false;
#if WITH_EDITORONLY_DATA
			bFailedRequests = FinishDDCRequests();
#endif //WITH_EDITORONLY_DATA

			// could maybe iterate over the things we know are done, but I couldn't tell if that was IndicesToLoad or not.
			for (FLoadedAudioChunk& LoadedChunk : LoadedChunks)
			{
				if (LoadedChunk.IORequest != nullptr)
				{
					LoadedChunk.IORequest->WaitCompletion();

					delete LoadedChunk.IORequest;
					LoadedChunk.IORequest = nullptr;
				}
			}

			PendingChunkChangeRequestStatus.Decrement();
			bHasPendingRequestInFlight = false;
			LoadedChunkIndices = CurrentRequest.RequiredIndices;
		}
		else if (RequestStatus == AudioState_ReadyFor_Requests) // odd that this is an else, probably we should start requests right now
		{
			BeginPendingRequests(IndicesToLoad, IndicesToFree);
		}
	}

	return bHasPendingRequestInFlight;
}

void FStreamingWaveData::UpdateChunkRequests(FWaveRequest& InWaveRequest)
{
	// Might change this but ensures chunk 0 stays loaded for now
	check(InWaveRequest.RequiredIndices.Contains(0));
	check(PendingChunkChangeRequestStatus.GetValue() == AudioState_ReadyFor_Requests);

	CurrentRequest = InWaveRequest;

}

bool FStreamingWaveData::HasPendingRequests(TArray<uint32>& IndicesToLoad, TArray<uint32>& IndicesToFree) const
{
	IndicesToLoad.Empty();
	IndicesToFree.Empty();

	// Find indices that aren't loaded
	for (auto NeededIndex : CurrentRequest.RequiredIndices)
	{
		if (!LoadedChunkIndices.Contains(NeededIndex))
		{
			IndicesToLoad.AddUnique(NeededIndex);
		}
	}

	// Find indices that aren't needed anymore
	for (auto CurrentIndex : LoadedChunkIndices)
	{
		if (!CurrentRequest.RequiredIndices.Contains(CurrentIndex))
		{
			IndicesToFree.AddUnique(CurrentIndex);
		}
	}

	return IndicesToLoad.Num() > 0 || IndicesToFree.Num() > 0;
}

void FStreamingWaveData::BeginPendingRequests(const TArray<uint32>& IndicesToLoad, const TArray<uint32>& IndicesToFree)
{
	if (UE_LOG_ACTIVE(LogAudio, Log) && IndicesToLoad.Num() > 0)
	{
		FString LogString = FString::Printf(TEXT("Requesting ASync load of chunk(s) %d"), IndicesToLoad[0]);
		for (int32 Index = 1; Index < IndicesToLoad.Num(); ++Index)
		{
			LogString += FString::Printf(TEXT(", %d"), IndicesToLoad[Index]);
		}
		LogString += FString::Printf(TEXT(" from SoundWave'%s'"), *SoundWave->GetFName().ToString());
		UE_LOG(LogAudio, Log, TEXT("%s"), *LogString);
	}

	TArray<uint32> FreeChunkIndices;

	// Mark Chunks for removal in case they can be reused
	{
		for (auto Index : IndicesToFree)
		{
			for (int32 ChunkIndex = 0; ChunkIndex < LoadedChunks.Num(); ++ChunkIndex)
			{
				check(Index != 0);
				if (LoadedChunks[ChunkIndex].Index == Index)
				{
					FreeLoadedChunk(LoadedChunks[ChunkIndex]);
					LoadedChunks.RemoveAt(ChunkIndex);
					break;
				}
			}
		}
	}

	if (IndicesToLoad.Num() > 0)
	{
		PendingChunkChangeRequestStatus.Set(AudioState_InProgress_Loading);

		// Set off all IO Requests
		for (auto Index : IndicesToLoad)
		{
			const FStreamedAudioChunk& Chunk = SoundWave->GetChunk(Index);
			int32 ChunkSize = Chunk.DataSize;

			int32 LoadedChunkStorageIndex = AddNewLoadedChunk(ChunkSize, Chunk.AudioDataSize);
			FLoadedAudioChunk* ChunkStorage = &LoadedChunks[LoadedChunkStorageIndex];

			ChunkStorage->Index = Index;

			check(LoadedChunkStorageIndex != INDEX_NONE);

			// Pass the request on to the async io manager after increasing the request count. The request count 
			// has been pre-incremented before fielding the update request so we don't have to worry about file
			// I/O immediately completing and the game thread kicking off again before this function
			// returns.
			PendingChunkChangeRequestStatus.Increment();

			EAsyncIOPriorityAndFlags AsyncIOPriority = AIOP_High | AIOP_FLAG_DONTCACHE;

			// Load and decompress async.
#if WITH_EDITORONLY_DATA
			if (Chunk.DerivedDataKey.IsEmpty() == false)
			{
				ChunkStorage->Data = static_cast<uint8*>(FMemory::Malloc(ChunkSize));
				INC_DWORD_STAT_BY(STAT_AudioMemorySize, ChunkSize);
				INC_DWORD_STAT_BY(STAT_AudioMemory, ChunkSize);

				// This streaming manager does not use the callback when the DDC load
				// is completed:
				TFunction<void(bool)> NullOnLoadCompletedCallback = [](bool) {};

				FAsyncStreamDerivedChunkTask* Task = new FAsyncStreamDerivedChunkTask(
					Chunk.DerivedDataKey,
					ChunkStorage->Data,
					ChunkSize,
					&PendingChunkChangeRequestStatus,
					MoveTemp(NullOnLoadCompletedCallback)
				);
				PendingAsyncStreamDerivedChunkTasks.Add(Task);
				// This task may perform a long synchronous DDC request. Using DoNotRunInsideBusyWait prevents potentially delaying foreground tasks.
				Task->StartBackgroundTask(GThreadPool, EQueuedWorkPriority::Normal, EQueuedWorkFlags::DoNotRunInsideBusyWait);
			}
			else
#endif // #if WITH_EDITORONLY_DATA
			{
				check(!ChunkStorage->IORequest); // Make sure we do not already have a request
				check(!ChunkStorage->Data); // Make sure we do not already have data
				check(Chunk.BulkData.GetBulkDataSize() == ChunkStorage->DataSize); // Make sure that the bulkdata size matches

				FBulkDataIORequestCallBack AsyncFileCallBack =
					[this, LoadedChunkStorageIndex](bool bWasCancelled, IBulkDataIORequest* Req)
				{
					AudioStreamingManager->OnAsyncFileCallback(this, LoadedChunkStorageIndex, Req);

					PendingChunkChangeRequestStatus.Decrement();
				};

				ChunkStorage->IORequest = Chunk.BulkData.CreateStreamingRequest(AsyncIOPriority, &AsyncFileCallBack, nullptr);

				if (!ChunkStorage->IORequest)
				{
					UE_LOG(LogAudio, Error, TEXT("Audio streaming read request failed."));

					// we failed for some reason; file not found I guess.
					PendingChunkChangeRequestStatus.Decrement();
				}
			}
		}

		// Decrement the state to AudioState_InProgress_Loading + NumChunksCurrentLoading - 1.
		PendingChunkChangeRequestStatus.Decrement();
	}
	else
	{
		// Skip straight to finalisation
		PendingChunkChangeRequestStatus.Set(AudioState_ReadyFor_Finalization);
	}

}

bool FStreamingWaveData::BlockTillAllRequestsFinished(float TimeLimit)
{
	QUICK_SCOPE_CYCLE_COUNTER(FStreamingWaveData_BlockTillAllRequestsFinished);
	if (TimeLimit == 0.0f)
	{
		for (FLoadedAudioChunk& LoadedChunk : LoadedChunks)
		{
			if (LoadedChunk.IORequest)
			{
				LoadedChunk.IORequest->WaitCompletion();

				delete LoadedChunk.IORequest;
				LoadedChunk.IORequest = nullptr;
			}
		}
	}
	else
	{
		double EndTime = FPlatformTime::Seconds() + TimeLimit;
		for (FLoadedAudioChunk& LoadedChunk : LoadedChunks)
		{
			if (LoadedChunk.IORequest)
			{
				float ThisTimeLimit = EndTime - FPlatformTime::Seconds();
				if (ThisTimeLimit < .001f || // one ms is the granularity of the platform event system
					!LoadedChunk.IORequest->WaitCompletion(ThisTimeLimit))
				{
					return false;
				}
				delete LoadedChunk.IORequest;
				LoadedChunk.IORequest = nullptr;
			}
		}
	}
	return true;
}

#if WITH_EDITORONLY_DATA
bool FStreamingWaveData::FinishDDCRequests()
{
	bool bRequestFailed = false;
	if (PendingAsyncStreamDerivedChunkTasks.Num())
	{
		for (int32 TaskIndex = 0; TaskIndex < PendingAsyncStreamDerivedChunkTasks.Num(); ++TaskIndex)
		{
			FAsyncStreamDerivedChunkTask& Task = PendingAsyncStreamDerivedChunkTasks[TaskIndex];
			Task.EnsureCompletion();
			bRequestFailed |= Task.GetTask().DidRequestFail();
		}
		PendingAsyncStreamDerivedChunkTasks.Empty();
	}
	return bRequestFailed;
}
#endif //WITH_EDITORONLY_DATA

int32 FStreamingWaveData::AddNewLoadedChunk(int32 ChunkSize, int32 AudioSize)
{
	int32 NewIndex = LoadedChunks.Num();
	LoadedChunks.AddDefaulted();

	LoadedChunks[NewIndex].DataSize = ChunkSize;
	LoadedChunks[NewIndex].AudioDataSize = AudioSize;

	return NewIndex;
}

void FStreamingWaveData::FreeLoadedChunk(FLoadedAudioChunk& LoadedChunk)
{
	if (LoadedChunk.IORequest)
	{
		LoadedChunk.IORequest->Cancel();
		LoadedChunk.IORequest->WaitCompletion();

		delete LoadedChunk.IORequest;
		LoadedChunk.IORequest = nullptr;

		// Process pending async requests after iorequest finishes
		AudioStreamingManager->ProcessPendingAsyncFileResults();
	}

	if (LoadedChunk.Data != NULL)
	{
		FMemory::Free(LoadedChunk.Data);

		DEC_DWORD_STAT_BY(STAT_AudioMemorySize, LoadedChunk.DataSize);
		DEC_DWORD_STAT_BY(STAT_AudioMemory, LoadedChunk.DataSize);
	}
	LoadedChunk.Data = NULL;
	LoadedChunk.AudioDataSize = 0;
	LoadedChunk.DataSize = 0;
	LoadedChunk.Index = 0;
}

////////////////////////////
// FAudioStreamingManager //
////////////////////////////

FLegacyAudioStreamingManager::FLegacyAudioStreamingManager()
{
}

FLegacyAudioStreamingManager::~FLegacyAudioStreamingManager()
{
}

void FLegacyAudioStreamingManager::OnAsyncFileCallback(FStreamingWaveData* StreamingWaveData, int32 LoadedAudioChunkIndex, IBulkDataIORequest* ReadRequest)
{
	// Check to see if we successfully managed to load anything
	uint8* Mem = ReadRequest->GetReadResults();
	if (Mem)
	{
		// Create a new chunk load result object. Will be deleted on audio thread when TQueue is pumped.
		FASyncAudioChunkLoadResult* NewAudioChunkLoadResult = new FASyncAudioChunkLoadResult();

		// Copy the ptr which we will use to place the results of the read on the audio thread upon pumping.
		NewAudioChunkLoadResult->StreamingWaveData = StreamingWaveData;

		// Grab the loaded chunk memory ptr since it will be invalid as soon as this callback finishes
		NewAudioChunkLoadResult->DataResults = Mem;

		// The chunk index to load the results into
		NewAudioChunkLoadResult->LoadedAudioChunkIndex = LoadedAudioChunkIndex;

		// Safely add the results of the async file callback into an array to be pumped on audio thread
		FScopeLock StreamChunkResults(&ChunkResultCriticalSection);
		AsyncAudioStreamChunkResults.Add(NewAudioChunkLoadResult);
	}
}

void FLegacyAudioStreamingManager::ProcessPendingAsyncFileResults()
{
	// Pump the results of any async file loads in a protected critical section 
	FScopeLock StreamChunkResults(&ChunkResultCriticalSection);

	for (FASyncAudioChunkLoadResult* AudioChunkLoadResult : AsyncAudioStreamChunkResults)
	{
		// Copy the results to the chunk storage safely
		const int32 LoadedAudioChunkIndex = AudioChunkLoadResult->LoadedAudioChunkIndex;

		check(AudioChunkLoadResult->StreamingWaveData != nullptr);

		if (LoadedAudioChunkIndex == INDEX_NONE || LoadedAudioChunkIndex >= AudioChunkLoadResult->StreamingWaveData->LoadedChunks.Num())
		{
			UE_LOG(LogAudio, Warning, TEXT("Loaded streaming chunk index %d is invalid. Current number of loaded chunks is: %d"), LoadedAudioChunkIndex, AudioChunkLoadResult->StreamingWaveData->LoadedChunks.Num());

			delete AudioChunkLoadResult;
			AudioChunkLoadResult = nullptr;
			continue;
		}

		FLoadedAudioChunk* ChunkStorage = &AudioChunkLoadResult->StreamingWaveData->LoadedChunks[LoadedAudioChunkIndex];

		checkf(!ChunkStorage->Data, TEXT("Chunk storage already has data. (0x%p), datasize: %d"), ChunkStorage->Data, ChunkStorage->DataSize);

		ChunkStorage->Data = AudioChunkLoadResult->DataResults;

		INC_DWORD_STAT_BY(STAT_AudioMemorySize, ChunkStorage->DataSize);
		INC_DWORD_STAT_BY(STAT_AudioMemory, ChunkStorage->DataSize);

		// Cleanup the chunk load results
		delete AudioChunkLoadResult;
		AudioChunkLoadResult = nullptr;
	}

	AsyncAudioStreamChunkResults.Reset();
}

void FLegacyAudioStreamingManager::UpdateResourceStreaming(float DeltaTime, bool bProcessEverything /*= false*/)
{
	LLM_SCOPE(ELLMTag::Audio);

	FScopeLock Lock(&CriticalSection);

	for (auto& WavePair : StreamingSoundWaves)
	{
		WavePair.Value->UpdateStreamingStatus();
	}

	// Process any async file requests after updating the stream status
	ProcessPendingAsyncFileResults();

	for (auto Source : StreamingSoundSources)
	{
		const FWaveInstance* WaveInstance = Source->GetWaveInstance();
		USoundWave* Wave = WaveInstance ? WaveInstance->WaveData : nullptr;
		if (Wave)
		{
			FStreamingWaveData** WaveDataPtr = StreamingSoundWaves.Find(Wave);

			if (WaveDataPtr && (*WaveDataPtr)->PendingChunkChangeRequestStatus.GetValue() == AudioState_ReadyFor_Requests)
			{
				FStreamingWaveData* WaveData = *WaveDataPtr;
				// Request the chunk the source is using and the one after that
				FWaveRequest& WaveRequest = GetWaveRequest(Wave);
				const FSoundBuffer* SoundBuffer = Source->GetBuffer();
				if (SoundBuffer)
				{
					int32 SourceChunk = SoundBuffer->GetCurrentChunkIndex();
					if (SourceChunk >= 0 && SourceChunk < (int32)Wave->GetNumChunks())
					{
						WaveRequest.RequiredIndices.AddUnique(SourceChunk);
						WaveRequest.RequiredIndices.AddUnique((SourceChunk + 1) % Wave->GetNumChunks());
						WaveRequest.bPrioritiseRequest = true;
					}
					else
					{
						UE_LOG(LogAudio, Log, TEXT("Invalid chunk request curIndex=%d numChunks=%d\n"), SourceChunk, Wave->GetNumChunks());
					}
				}
			}
		}
	}

	for (ICompressedAudioInfo* Decoder : CompressedAudioInfos)
	{
		const FSoundWaveProxyPtr& SoundWave = Decoder->GetStreamingSoundWave();
		FStreamingWaveData** WaveDataPtr = StreamingSoundWaves.Find(SoundWave->GetFObjectKey());
		if (WaveDataPtr && (*WaveDataPtr)->PendingChunkChangeRequestStatus.GetValue() == AudioState_ReadyFor_Requests)
		{
			FStreamingWaveData* WaveData = *WaveDataPtr;
			// Request the chunk the source is using and the one after that
			FWaveRequest& WaveRequest = GetWaveRequest(SoundWave->GetFObjectKey());
			int32 SourceChunk = Decoder->GetCurrentChunkIndex();

			// If there's a seek request, use that as our current chunk index.				
			if (Decoder->IsStreamedCompressedInfo())
			{
				// This is awk because the decoder thread catches the seek to
				// advance, so we have to read this atomically. However, if it changes,
				// that means that the chunk is already in memory (because StreamCompressedData
				// only updates it when the chunk is present).
				// ... and also, GetCurrentChunkIndex has the same problem. So it's layers
				// on layers.
				IStreamedCompressedInfo* Streamed = (IStreamedCompressedInfo*)Decoder;
				int32 StreamSeekToBlockIndex = Streamed->GetStreamSeekBlockIndex();
				if (StreamSeekToBlockIndex != INDEX_NONE)
				{
					UE_LOG(LogAudio, Log, TEXT("Diverting streaming chunk due to seek -- from %d to %d"), SourceChunk, StreamSeekToBlockIndex);
					SourceChunk = StreamSeekToBlockIndex;
				}
			}

			if (SourceChunk >= 0 && SourceChunk < (int32)SoundWave->GetNumChunks())
			{
				WaveRequest.RequiredIndices.AddUnique(SourceChunk);
				WaveRequest.RequiredIndices.AddUnique((SourceChunk + 1) % SoundWave->GetNumChunks());
				WaveRequest.bPrioritiseRequest = true;
			}
			else
			{
				UE_LOG(LogAudio, Log, TEXT("Invalid chunk request curIndex=%d numChunks=%d\n"), SourceChunk, SoundWave->GetNumChunks());
			}
		}
	}

	for (auto Iter = WaveRequests.CreateIterator(); Iter; ++Iter)
	{
		FStreamingWaveData* WaveData = StreamingSoundWaves.FindRef(Iter.Key());

		if (WaveData && WaveData->PendingChunkChangeRequestStatus.GetValue() == AudioState_ReadyFor_Requests)
		{
			WaveData->UpdateChunkRequests(Iter.Value());
			WaveData->UpdateStreamingStatus();
			Iter.RemoveCurrent();
		}
	}
 
	// Process any async file requests after updating the streaming wave data stream statuses
	ProcessPendingAsyncFileResults();
}

int32 FLegacyAudioStreamingManager::BlockTillAllRequestsFinished(float TimeLimit, bool)
{
	{
		FScopeLock Lock(&CriticalSection);

		QUICK_SCOPE_CYCLE_COUNTER(FAudioStreamingManager_BlockTillAllRequestsFinished);
		int32 Result = 0;

		if (TimeLimit == 0.0f)
		{
			for (auto& WavePair : StreamingSoundWaves)
			{
				WavePair.Value->BlockTillAllRequestsFinished();
			}
		}
		else
		{
			double EndTime = FPlatformTime::Seconds() + TimeLimit;
			for (auto& WavePair : StreamingSoundWaves)
			{
				float ThisTimeLimit = EndTime - FPlatformTime::Seconds();
				if (ThisTimeLimit < .001f || // one ms is the granularity of the platform event system
					!WavePair.Value->BlockTillAllRequestsFinished(ThisTimeLimit))
				{
					Result = 1; // we don't report the actual number, just 1 for any number of outstanding requests
					break;
				}
			}
		}
		
		// After blocking to process all requests, pump the queue
		ProcessPendingAsyncFileResults();

		return Result;
	}
}

void FLegacyAudioStreamingManager::CancelForcedResources()
{
}

void FLegacyAudioStreamingManager::NotifyLevelChange()
{
}

void FLegacyAudioStreamingManager::SetDisregardWorldResourcesForFrames(int32 NumFrames)
{
}

void FLegacyAudioStreamingManager::AddLevel(class ULevel* Level)
{
}

void FLegacyAudioStreamingManager::RemoveLevel(class ULevel* Level)
{
}

void FLegacyAudioStreamingManager::NotifyLevelOffset(class ULevel* Level, const FVector& Offset)
{
}

void FLegacyAudioStreamingManager::AddStreamingSoundWave(const FSoundWaveProxyPtr& SoundWave)
{
	if (ensure(SoundWave.IsValid()) && FPlatformProperties::SupportsAudioStreaming() && SoundWave->IsStreaming())
	{
		FScopeLock Lock(&CriticalSection);

		if (StreamingSoundWaves.FindRef(SoundWave->GetFObjectKey()) == nullptr)
		{
			FStreamingWaveData* NewStreamingWaveData = new FStreamingWaveData;
			if (NewStreamingWaveData->Initialize(SoundWave, this))
			{
				StreamingSoundWaves.Add(SoundWave->GetFObjectKey(), NewStreamingWaveData);
			}
			else
			{
				// Failed to initialize, don't add to list of streaming sound waves
				delete NewStreamingWaveData;
			}
		}
	}
}

void FLegacyAudioStreamingManager::RemoveStreamingSoundWave(const FSoundWaveProxyPtr& SoundWave)
{
	if (!ensure(SoundWave.IsValid()))
	{
		return;
	}

	FScopeLock Lock(&CriticalSection);
	FStreamingWaveData* WaveData = StreamingSoundWaves.FindRef(SoundWave->GetFObjectKey());
	if (WaveData)
	{
		StreamingSoundWaves.Remove(SoundWave->GetFObjectKey());

		// Free the resources of the streaming wave data. This blocks pending IO requests
		WaveData->FreeResources();

		{
			// Then we need to remove any results from those pending requests before we delete so that we don't process them
			FScopeLock StreamChunkResults(&ChunkResultCriticalSection);
			for (int32 i = AsyncAudioStreamChunkResults.Num() - 1 ; i >= 0; --i)
			{
				FASyncAudioChunkLoadResult* LoadResult = AsyncAudioStreamChunkResults[i];
				FStreamingWaveData* StreamingWaveData = LoadResult->StreamingWaveData;
				if (StreamingWaveData == WaveData)
				{
					delete LoadResult;
					
					AsyncAudioStreamChunkResults.RemoveAtSwap(i, 1, EAllowShrinking::No);
				}
			}
		}
		delete WaveData;
	}
	WaveRequests.Remove(SoundWave->GetFObjectKey());
}

void FLegacyAudioStreamingManager::AddDecoder(ICompressedAudioInfo* InCompressedAudioInfo)
{
	FScopeLock Lock(&CriticalSection);
	CompressedAudioInfos.AddUnique(InCompressedAudioInfo);
}

void FLegacyAudioStreamingManager::RemoveDecoder(ICompressedAudioInfo* InCompressedAudioInfo)
{
	FScopeLock Lock(&CriticalSection);
	CompressedAudioInfos.Remove(InCompressedAudioInfo);
}

bool FLegacyAudioStreamingManager::IsManagedStreamingSoundWave(const FSoundWaveProxyPtr& SoundWave) const
{
	FScopeLock Lock(&CriticalSection);
	return ensure(SoundWave.IsValid()) && (StreamingSoundWaves.FindRef(SoundWave->GetFObjectKey()) != NULL);
}

bool FLegacyAudioStreamingManager::IsStreamingInProgress(const FSoundWaveProxyPtr&  SoundWave)
{
	FScopeLock Lock(&CriticalSection);
	if (ensure(SoundWave.IsValid()))
	{
		FStreamingWaveData* WaveData = StreamingSoundWaves.FindRef(SoundWave->GetFObjectKey());
		if (WaveData)
		{
			return WaveData->UpdateStreamingStatus();
		}
	}

	return false;
}

bool FLegacyAudioStreamingManager::CanCreateSoundSource(const FWaveInstance* WaveInstance) const
{
	check(WaveInstance);
	check(WaveInstance->IsStreaming());

	int32 MaxStreams = MaxConcurrentStreamsCvar;
	if (!MaxStreams)
	{
		MaxStreams = GetDefault<UAudioSettings>()->MaximumConcurrentStreams;
	}

	FScopeLock Lock(&CriticalSection);

	// If the sound wave hasn't been added, or failed when trying to add during sound wave post load, we can't create a streaming sound source with this sound wave
	if (!WaveInstance->WaveData || !StreamingSoundWaves.Contains(WaveInstance->WaveData))
	{
		return false;
	}

	if ( StreamingSoundSources.Num() < MaxStreams )
	{
		return true;
	}

	for (int32 Index = 0; Index < StreamingSoundSources.Num(); ++Index)
	{
		const FSoundSource* ExistingSource = StreamingSoundSources[Index];
		const FWaveInstance* ExistingWaveInst = ExistingSource->GetWaveInstance();
		if (!ExistingWaveInst || !ExistingWaveInst->WaveData
			|| ExistingWaveInst->WaveData->StreamingPriority < WaveInstance->WaveData->StreamingPriority)
		{
			return Index < MaxStreams;
		}
	}

	return false;
}

void FLegacyAudioStreamingManager::AddStreamingSoundSource(FSoundSource* SoundSource)
{
	const FWaveInstance* WaveInstance = SoundSource->GetWaveInstance();
	if (WaveInstance && WaveInstance->IsStreaming())
	{
		int32 MaxStreams = MaxConcurrentStreamsCvar;
		if (!MaxStreams)
		{
			MaxStreams = GetDefault<UAudioSettings>()->MaximumConcurrentStreams;
		}

		FScopeLock Lock(&CriticalSection);

		// Add source sorted by priority so we can easily iterate over the amount of streams
		// that are allowed
		int32 OrderedIndex = -1;
		for (int32 Index = 0; Index < StreamingSoundSources.Num() && Index < MaxStreams; ++Index)
		{
			const FSoundSource* ExistingSource = StreamingSoundSources[Index];
			const FWaveInstance* ExistingWaveInst = ExistingSource->GetWaveInstance();
			if (!ExistingWaveInst || !ExistingWaveInst->WaveData
				|| ExistingWaveInst->WaveData->StreamingPriority < WaveInstance->WaveData->StreamingPriority)
			{
				OrderedIndex = Index;
				break;
			}
		}
		if (OrderedIndex != -1)
		{
			StreamingSoundSources.Insert(SoundSource, OrderedIndex);
		}
		else if (StreamingSoundSources.Num() < MaxStreams)
		{
			StreamingSoundSources.AddUnique(SoundSource);
		}

		for (int32 Index = StreamingSoundSources.Num()-1; Index >= MaxStreams; --Index)
		{
			StreamingSoundSources[Index]->Stop();
		}
	}
}

void FLegacyAudioStreamingManager::RemoveStreamingSoundSource(FSoundSource* SoundSource)
{
	const FWaveInstance* WaveInstance = SoundSource->GetWaveInstance();
	if (WaveInstance && WaveInstance->WaveData && WaveInstance->WaveData->IsStreaming(nullptr) && !WaveInstance->WaveData->ShouldUseStreamCaching())
	{
		FScopeLock Lock(&CriticalSection);

		// Make sure there is a request so that unused chunks
		// can be cleared if this was the last playing instance
		GetWaveRequest(FObjectKey(WaveInstance->WaveData));
		StreamingSoundSources.Remove(SoundSource);
	}
}

bool FLegacyAudioStreamingManager::IsManagedStreamingSoundSource(const FSoundSource* SoundSource) const
{
	FScopeLock Lock(&CriticalSection);
	return StreamingSoundSources.FindByKey(SoundSource) != NULL;
}

bool FLegacyAudioStreamingManager::RequestChunk(const FSoundWaveProxyPtr&, uint32 ChunkIndex, TFunction<void(EAudioChunkLoadResult)> OnLoadCompleted, ENamedThreads::Type ThreadToCallOnLoadCompletedOn, bool bForImmediatePlayback)
{
	UE_LOG(LogAudio, Warning, TEXT("RequestChunk is only supported in Stream Caching."));
	return false;
}

FAudioChunkHandle FLegacyAudioStreamingManager::GetLoadedChunk(const FSoundWaveProxyPtr&  SoundWave, uint32 ChunkIndex, bool bBlockForLoad, bool bForImmediatePlayback) const
{
	// Check for the spoof of failing to load a stream chunk
	if (ensure(SoundWave.IsValid()) && SpoofFailedStreamChunkLoad > 0)
	{
		return FAudioChunkHandle();
	}

	// If we fail at getting the critical section here, early out. 
	if (!CriticalSection.TryLock())
	{
		return FAudioChunkHandle();
	}

	const FStreamingWaveData* WaveData = StreamingSoundWaves.FindRef(SoundWave->GetFObjectKey());
	if (WaveData)
	{
		if (WaveData->LoadedChunkIndices.Contains(ChunkIndex))
		{
			for (int32 Index = 0; Index < WaveData->LoadedChunks.Num(); ++Index)
			{
				if (WaveData->LoadedChunks[Index].Index == ChunkIndex)
				{
					CriticalSection.Unlock();
					return BuildChunkHandle(WaveData->LoadedChunks[Index].Data, WaveData->LoadedChunks[Index].AudioDataSize, SoundWave, SoundWave->GetFName(), ChunkIndex, InvalidAudioStreamCacheLookupID);
				}
			}
		}
	}

	CriticalSection.Unlock();
	return FAudioChunkHandle();
}

FWaveRequest& FLegacyAudioStreamingManager::GetWaveRequest(FObjectKey Key)
{
	FWaveRequest* WaveRequest = WaveRequests.Find(Key);
	if (!WaveRequest)
	{
		// Setup the new request so it always asks for chunk 0
		WaveRequest = &WaveRequests.Add(Key);
		WaveRequest->RequiredIndices.AddUnique(0);
		WaveRequest->bPrioritiseRequest = false;
	}
	return *WaveRequest;
}

FString FLegacyAudioStreamingManager::GenerateMemoryReport()
{
	return FString(TEXT("Only supported when stream caching is enabled.\n"));
}

uint64 FLegacyAudioStreamingManager::TrimMemory(uint64 NumBytesToFree)
{
	UE_LOG(LogAudio, Warning, TEXT("IAudioStreamingManager::TrimMemory is only supported when Stream Caching is enabled."));
	return 0;
}

int32 FLegacyAudioStreamingManager::RenderStatAudioStreaming(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
{
	// Only supported by stream caching.
	return Y;
}
