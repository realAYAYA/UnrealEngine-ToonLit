// Copyright Epic Games, Inc. All Rights Reserved.

#include "StorageServerIoDispatcherBackend.h"
#include "Misc/QueuedThreadPool.h"
#include "StorageServerConnection.h"
#include "Misc/ScopeLock.h"
#include "HAL/RunnableThread.h"

#if !UE_BUILD_SHIPPING

int32 GStorageServerIoDispatcherMaxActiveBatchCount = 4;
static FAutoConsoleVariableRef CVar_StorageServerIoDispatcherMaxActiveBatchCount(
	TEXT("s.StorageServerIoDispatcherMaxActiveBatchCount"),
	GStorageServerIoDispatcherMaxActiveBatchCount,
	TEXT("StorageServer IoDispatcher max submitted batches count.")
);

int32 GStorageServerIoDispatcherBatchSize = 16;
static FAutoConsoleVariableRef CVar_StorageServerIoDispatcherBatchSize(
	TEXT("s.StorageServerIoDispatcherBatchSize"),
	GStorageServerIoDispatcherBatchSize,
	TEXT("StorageServer IoDispatcher batch size.")
);

FStorageServerIoDispatcherBackend::FStorageServerIoDispatcherBackend(FStorageServerConnection& InConnection)
	: Connection(InConnection)
	, NewRequestEvent(FPlatformProcess::GetSynchEventFromPool(false))
	, BatchCompletedEvent(FPlatformProcess::GetSynchEventFromPool(false))
{

}

FStorageServerIoDispatcherBackend::~FStorageServerIoDispatcherBackend()
{
	Shutdown();
	FPlatformProcess::ReturnSynchEventToPool(NewRequestEvent);
	FPlatformProcess::ReturnSynchEventToPool(BatchCompletedEvent);
}

void FStorageServerIoDispatcherBackend::Shutdown()
{
	if (Thread)
	{
		delete Thread;
		Thread = nullptr;
	}
}

void FStorageServerIoDispatcherBackend::Initialize(TSharedRef<const FIoDispatcherBackendContext> InContext)
{
	BackendContext = InContext;
	Thread = FRunnableThread::Create(this, TEXT("IoService"), 0, TPri_AboveNormal);
}

void FStorageServerIoDispatcherBackend::Stop()
{
	bStopRequested = true;
	NewRequestEvent->Trigger();
}

uint32 FStorageServerIoDispatcherBackend::Run()
{
	LLM_SCOPE(ELLMTag::FileSystem);
	const int32 BatchCount = GStorageServerIoDispatcherMaxActiveBatchCount;
	for (int32 BatchIndex = 0; BatchIndex < BatchCount; ++BatchIndex)
	{
		FBatch* Batch = new FBatch(*this, MakeUnique<FStorageServerSerializationContext>());
		Batch->Next = FirstAvailableBatch;
		FirstAvailableBatch = Batch;
	}

	FBatch* CurrentBatch = nullptr;
	while (!bStopRequested)
	{
		for (;;)
		{
			if (!CurrentBatch)
			{
				CurrentBatch = FirstAvailableBatch;
				if (!CurrentBatch)
				{
					WaitForBatchToComplete();
					CurrentBatch = FirstAvailableBatch;
					check(CurrentBatch);
				}
				FirstAvailableBatch = CurrentBatch->Next;
				CurrentBatch->Next = nullptr;
			}

			FIoRequestImpl* Request = RequestQueue.Pop();
			if (!Request)
			{
				break;
			}
			
			check(!Request->NextRequest);
			if (CurrentBatch->RequestsTail)
			{
				CurrentBatch->RequestsTail->NextRequest = Request;
			}
			else
			{
				CurrentBatch->RequestsHead = Request;
			}
			CurrentBatch->RequestsTail = Request;
			++CurrentBatch->RequestsCount;

			if (CurrentBatch->RequestsCount == GStorageServerIoDispatcherBatchSize)
			{
				SubmitBatch(CurrentBatch);
				CurrentBatch = nullptr;
			}
		}
		if (CurrentBatch && CurrentBatch->RequestsCount > 0)
		{
			SubmitBatch(CurrentBatch);
			CurrentBatch = nullptr;
		}
		NewRequestEvent->Wait();
	}

	for (int32 BatchIndex = 0; BatchIndex < BatchCount; ++BatchIndex)
	{
		if (!FirstAvailableBatch)
		{
			if (!WaitForBatchToComplete(10000))
			{
				UE_LOG(LogIoDispatcher, Warning, TEXT("Outstanding requests when shutting down storage server backend"));
				return 0;
			}
		}
		check(FirstAvailableBatch);
		FBatch* Batch = FirstAvailableBatch;
		FirstAvailableBatch = FirstAvailableBatch->Next;
		delete Batch;
	}
	return 0;
}

bool FStorageServerIoDispatcherBackend::Resolve(FIoRequestImpl* Request)
{
	check(Request);
	if (BackendContext->bIsMultiThreaded)
	{
		RequestQueue.Push(*Request);
		NewRequestEvent->Trigger();
	}
	else
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(StorageServerIoDispatcherReadChunk);
		bool bSuccess = Connection.ReadChunkRequest(Request->ChunkId, Request->Options.GetOffset(), Request->Options.GetSize(), [&Request](FStorageServerResponse& Response)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SerializeResponse);
			FIoBuffer Chunk;
			if (Response.SerializeChunk(Chunk, Request->Options.GetTargetVa(), Request->Options.GetOffset(), Request->Options.GetSize()))
			{
				Request->SetResult(Chunk);
			}
			else
			{
				Request->SetFailed();
			}
		});
		if (!bSuccess)
		{
			Request->SetFailed();
		}
		if (CompletedRequestsTail)
		{
			CompletedRequestsTail->NextRequest = Request;
		}
		else
		{
			CompletedRequestsHead = Request;
		}
		CompletedRequestsTail = Request;
	}
	return true;
}

bool FStorageServerIoDispatcherBackend::DoesChunkExist(const FIoChunkId& ChunkId) const
{
	return GetSizeForChunk(ChunkId).IsOk();
}

TIoStatusOr<uint64> FStorageServerIoDispatcherBackend::GetSizeForChunk(const FIoChunkId& ChunkId) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(StorageServerIoDispatcherGetSizeForChunk);
	int64 ChunkSize = Connection.ChunkSizeRequest(ChunkId);
	if (ChunkSize >= 0)
	{
		return ChunkSize;
	}
	else
	{
		return FIoStatus(EIoErrorCode::NotFound);
	}
}

FIoRequestImpl* FStorageServerIoDispatcherBackend::GetCompletedRequests()
{
	FScopeLock Lock(&CompletedRequestsCritical);
	FIoRequestImpl* Result = CompletedRequestsHead;
	CompletedRequestsHead = CompletedRequestsTail = nullptr;
	return Result;
}

void FStorageServerIoDispatcherBackend::SubmitBatch(FBatch* Batch)
{
 	GIOThreadPool->AddQueuedWork(Batch);
	++SubmittedBatchesCount;
}

bool FStorageServerIoDispatcherBackend::WaitForBatchToComplete(uint32 WaitTime)
{
	bool bAtLeastOneCompleted = false;
	while (!bAtLeastOneCompleted)
	{
		if (!BatchCompletedEvent->Wait(WaitTime))
		{
			return false;
		}

		FBatch* LocalCompletedBatches;
		{
			FScopeLock _(&CompletedBatchesCritical);
			LocalCompletedBatches = FirstCompletedBatch;
			FirstCompletedBatch = nullptr;
		}
		while (LocalCompletedBatches)
		{
			check(SubmittedBatchesCount > 0);
			--SubmittedBatchesCount;
			FBatch* CompletedBatch = LocalCompletedBatches;
			LocalCompletedBatches = LocalCompletedBatches->Next;
			CompletedBatch->Next = FirstAvailableBatch;
			FirstAvailableBatch = CompletedBatch;
			bAtLeastOneCompleted = true;
		}
	}
	return true;
}

void FStorageServerIoDispatcherBackend::OnBatchCompleted(FBatch* Batch)
{
	{
		FScopeLock Lock(&CompletedRequestsCritical);
		if (CompletedRequestsTail)
		{
			CompletedRequestsTail->NextRequest = Batch->RequestsHead;
		}
		else
		{
			CompletedRequestsHead = Batch->RequestsHead;
		}
		CompletedRequestsTail = Batch->RequestsTail;
	}
	BackendContext->WakeUpDispatcherThreadDelegate.Execute();

	Batch->RequestsHead = Batch->RequestsTail = nullptr;
	Batch->RequestsCount = 0;
	{
		FScopeLock _(&CompletedBatchesCritical);
		Batch->Next = FirstCompletedBatch;
		FirstCompletedBatch = Batch;
	}
	BatchCompletedEvent->Trigger();
}

FStorageServerIoDispatcherBackend::FBatch::FBatch(FStorageServerIoDispatcherBackend& InOwner, TUniquePtr<FStorageServerSerializationContext> InSerializationContext)
	: Owner(InOwner)
	, SerializationContext(MoveTemp(InSerializationContext))
{

}

void FStorageServerIoDispatcherBackend::FBatch::DoThreadedWork()
{
	LLM_SCOPE(ELLMTag::FileSystem);
	TRACE_CPUPROFILER_EVENT_SCOPE(StorageServerIoDispatcherProcessBatch);
	FIoRequestImpl* Request = RequestsHead;
#if 0
	TArray<FIoRequestImpl*, TInlineAllocator<64>> RequestsArray;
	FStorageServerChunkBatchRequest ChunkBatchRequest = Owner.Connection.NewChunkBatchRequest();
	while (Request)
	{
		ChunkBatchRequest.AddChunk(Request->ChunkId, Request->Options.GetOffset(), Request->Options.GetSize());
		RequestsArray.Add(Request);
		Request = Request->NextRequest;
	}

	bool bSuccess = ChunkBatchRequest.Issue([&RequestsArray](uint32 ChunkCount, uint32* ChunkIndices, uint64* ChunkSizes, FStorageServerResponse& ChunkDataStream)
		{
			check(ChunkCount == RequestsArray.Num());
			for (uint32 ChunkIndex = 0; ChunkIndex < ChunkCount; ++ChunkIndex)
			{
				uint32 RequestIndex = ChunkIndices[ChunkIndex];
				FIoRequestImpl* Request = RequestsArray[RequestIndex];
				uint64 RequestSize = ChunkSizes[ChunkIndex];
				if (RequestSize == uint64(-1))
				{
					Request->SetFailed();
					continue;
				}
				if (void* TargetVa = Request->Options.GetTargetVa())
				{
					Request->IoBuffer = FIoBuffer(FIoBuffer::Wrap, TargetVa, RequestSize);
				}
				else
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(AllocMemoryForRequest);
					Request->IoBuffer = FIoBuffer(RequestSize);
				}
				TRACE_CPUPROFILER_EVENT_SCOPE(SerializeResponse);
				ChunkDataStream.Serialize(Request->IoBuffer.Data(), RequestSize);
			}
		});
	if (!bSuccess)
	{
		for (FIoRequestImpl* FailedRequest : RequestsArray)
		{
			FailedRequest->SetFailed();
		}
	}
#else
	while (Request)
	{
		FIoRequestImpl* NextRequest = Request->NextRequest;
		TRACE_CPUPROFILER_EVENT_SCOPE(StorageServerIoDispatcherReadChunk);
		bool bSuccess = Owner.Connection.ReadChunkRequest(Request->ChunkId, Request->Options.GetOffset(), Request->Options.GetSize(), [this, &Request](FStorageServerResponse& Response)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(SerializeResponse);
				FIoBuffer Chunk;
				if (Response.SerializeChunk(*SerializationContext, Chunk, Request->Options.GetTargetVa(), Request->Options.GetOffset(), Request->Options.GetSize()))
				{
					Request->SetResult(Chunk);
				}
				else
				{
					Request->SetFailed();
				}
			});
		if (!bSuccess)
		{
			Request->SetFailed();
		}
		Request = NextRequest;
	}
#endif
	Owner.OnBatchCompleted(this);
}

FIoRequestImpl* FStorageServerIoDispatcherBackend::FRequestQueue::Pop()
{
	FScopeLock _(&CriticalSection);
	if (Heap.Num() == 0)
	{
		return nullptr;
	}
	FIoRequestImpl* Result;
	Heap.HeapPop(Result, QueueSortFunc, EAllowShrinking::No);
	return Result;
}

void FStorageServerIoDispatcherBackend::FRequestQueue::Push(FIoRequestImpl& Request)
{
	FScopeLock _(&CriticalSection);
	Heap.HeapPush(&Request, QueueSortFunc);
}

void FStorageServerIoDispatcherBackend::FRequestQueue::UpdateOrder()
{
	FScopeLock _(&CriticalSection);
	Heap.Heapify(QueueSortFunc);
}

#endif
