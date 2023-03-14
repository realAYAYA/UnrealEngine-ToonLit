// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IO/IoDispatcherBackend.h"
#include "HAL/Runnable.h"

#if !UE_BUILD_SHIPPING

class FStorageServerConnection;
struct FStorageServerSerializationContext;

class FStorageServerIoDispatcherBackend final
	: public FRunnable
	, public IIoDispatcherBackend
{
public:
	FStorageServerIoDispatcherBackend(FStorageServerConnection& Connection);
	~FStorageServerIoDispatcherBackend();

	void Initialize(TSharedRef<const FIoDispatcherBackendContext> Context) override;
	bool Resolve(FIoRequestImpl* Request) override;
	void CancelIoRequest(FIoRequestImpl* Request) override {};
	void UpdatePriorityForIoRequest(FIoRequestImpl* Request) override {};
	bool DoesChunkExist(const FIoChunkId& ChunkId) const override;
	TIoStatusOr<uint64> GetSizeForChunk(const FIoChunkId& ChunkId) const override;
	FIoRequestImpl* GetCompletedRequests() override;
	TIoStatusOr<FIoMappedRegion> OpenMapped(const FIoChunkId& ChunkId, const FIoReadOptions& Options) override
	{
		return FIoStatus(EIoErrorCode::NotFound);
	}

	virtual bool Init() override
	{
		return true;
	}
	virtual uint32 Run() override;
	virtual void Stop() override;

private:
	class FRequestQueue
	{
	public:
		FIoRequestImpl* Pop();
		void Push(FIoRequestImpl& Request);
		void UpdateOrder();

	private:
		static bool QueueSortFunc(const FIoRequestImpl& A, const FIoRequestImpl& B)
		{
			uintptr_t ASequence = reinterpret_cast<uintptr_t>(A.BackendData);
			uintptr_t BSequence = reinterpret_cast<uintptr_t>(B.BackendData);
			if (A.Priority == B.Priority)
			{
				return ASequence < BSequence;
			}
			return A.Priority > B.Priority;
		}

		TArray<FIoRequestImpl*> Heap;
		FCriticalSection CriticalSection;
	};

	struct FBatch
		: public IQueuedWork
	{
		FBatch(FStorageServerIoDispatcherBackend& InOwner, TUniquePtr<FStorageServerSerializationContext> InSerializationContext)
			: Owner(InOwner)
			, SerializationContext(MoveTemp(InSerializationContext))
		{

		}

		virtual void DoThreadedWork() override;
		virtual void Abandon() override {};

		FStorageServerIoDispatcherBackend& Owner;
		FBatch* Next = nullptr;
		FIoRequestImpl* RequestsHead = nullptr;
		FIoRequestImpl* RequestsTail = nullptr;
		uint64 RequestsCount = 0;
		TUniquePtr<FStorageServerSerializationContext> SerializationContext;
	};

	void SubmitBatch(FBatch* Batch);
	void OnBatchCompleted(FBatch* Batch);
	void WaitForBatchToComplete();

	FStorageServerConnection& Connection;
	TSharedPtr<const FIoDispatcherBackendContext> BackendContext;
	FRunnableThread* Thread = nullptr;
	FEvent* NewRequestEvent;
	FRequestQueue RequestQueue;
	FBatch* FirstAvailableBatch = nullptr;
	int32 SubmittedBatchesCount = 0;
	FCriticalSection CompletedBatchesCritical;
	FBatch* FirstCompletedBatch = nullptr;
	FEvent* BatchCompletedEvent;
	FCriticalSection CompletedRequestsCritical;
	FIoRequestImpl* CompletedRequestsHead = nullptr;
	FIoRequestImpl* CompletedRequestsTail = nullptr;
	TAtomic<bool> bStopRequested{ false };
};

#endif
