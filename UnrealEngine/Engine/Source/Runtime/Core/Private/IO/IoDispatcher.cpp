// Copyright Epic Games, Inc. All Rights Reserved.

#include "IO/IoDispatcher.h"
#include "IO/IoDispatcherPrivate.h"
#include "IO/IoStore.h"
#include "IO/IoOffsetLength.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Math/RandomStream.h"
#include "Misc/ScopeLock.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/PlatformProcess.h"
#include "Serialization/LargeMemoryReader.h"
#include "GenericPlatform/GenericPlatformChunkInstall.h"
#include "HAL/Event.h"
#include "Async/MappedFileHandle.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Containers/Ticker.h"
#include "IO/IoDispatcherBackend.h"
#include "Templates/Greater.h"

DEFINE_LOG_CATEGORY(LogIoDispatcher);

const FIoChunkId FIoChunkId::InvalidChunkId = FIoChunkId::CreateEmptyId();

TUniquePtr<FIoDispatcher> GIoDispatcher;

#if UE_BUILD_DEVELOPMENT || UE_BUILD_DEBUG
//PRAGMA_DISABLE_OPTIMIZATION
#endif

CSV_DEFINE_CATEGORY(IoDispatcher, true);
CSV_DEFINE_STAT(IoDispatcher, PendingIoRequests);

static const TCHAR* const GetIoErrorText_ErrorCodeTextArray[] =
{
	TEXT("OK"),
	TEXT("Unknown Status"),
	TEXT("Invalid Code"),
	TEXT("Cancelled"),
	TEXT("FileOpen Failed"),
	TEXT("File Not Open"),
	TEXT("Read Error"),
	TEXT("Write Error"),
	TEXT("Not Found"),
	TEXT("Corrupt Toc"),
	TEXT("Unknown ChunkID"),
	TEXT("Invalid Parameter"),
	TEXT("Signature Error"),
	TEXT("Invalid Encryption Key"),
	TEXT("Compression Error")
};

CORE_API const TCHAR* const* GetIoErrorText_ErrorCodeText = GetIoErrorText_ErrorCodeTextArray;

#if UE_IODISPATCHER_STATS_ENABLED
class FIoRequestStats
{
public:
	FIoRequestStats()
#if COUNTERSTRACE_ENABLED
		: PendingIoRequestsCounter(TEXT("IoDispatcher/PendingIoRequests"), TraceCounterDisplayHint_None)
#endif
	{
		Categories.Reserve(4);
		int32 PackageDataIndex = Categories.Emplace(TEXT("PackageData"));
		int32 BulkDataIndex = Categories.Emplace(TEXT("BulkData"));
		int32 ShadersIndex = Categories.Emplace(TEXT("Shaders"));
		int32 MiscIndex = Categories.Emplace(TEXT("Misc"));

		for (int32 Index = 0; Index < UE_ARRAY_COUNT(ChunkTypeToCategoryMap); ++Index)
		{
			ChunkTypeToCategoryMap[Index] = &Categories[MiscIndex];
		}
		ChunkTypeToCategoryMap[static_cast<int32>(EIoChunkType::ExportBundleData)] = &Categories[PackageDataIndex];
		ChunkTypeToCategoryMap[static_cast<int32>(EIoChunkType::BulkData)] = &Categories[BulkDataIndex];
		ChunkTypeToCategoryMap[static_cast<int32>(EIoChunkType::OptionalBulkData)] = &Categories[BulkDataIndex];
		ChunkTypeToCategoryMap[static_cast<int32>(EIoChunkType::MemoryMappedBulkData)] = &Categories[BulkDataIndex];
		ChunkTypeToCategoryMap[static_cast<int32>(EIoChunkType::ShaderCode)] = &Categories[ShadersIndex];

#if CSV_PROFILER
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FIoRequestStats::TickCsv));
#endif
	}

	~FIoRequestStats()
	{
#if CSV_PROFILER
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
#endif
	}

	void OnBatchIssued(FIoBatch& Batch)
	{
		const uint64 StartTime = FPlatformTime::Cycles64();
		FIoRequestImpl* Request = Batch.HeadRequest;
		while (Request)
		{
			Request->StartTime = StartTime;
			Request = Request->NextRequest;
		}
	}

	void OnRequestStarted(FIoRequestImpl& Request)
	{
		++PendingIoRequests;
#if COUNTERSTRACE_ENABLED
		PendingIoRequestsCounter.Set(PendingIoRequests);
#endif
	}

	void OnRequestCompleted(FIoRequestImpl& Request)
	{
		--PendingIoRequests;
#if COUNTERSTRACE_ENABLED
		PendingIoRequestsCounter.Set(PendingIoRequests);
#endif
		if (!Request.HasBuffer())
		{
			return;
		}
		FRequestCategory* Category = ChunkTypeToCategoryMap[static_cast<int32>(Request.ChunkId.GetChunkType())];
		++Category->TotaRequestsCount;
		const double Duration = FPlatformTime::ToSeconds64(FPlatformTime::Cycles64() - Request.GetStartTime());
		Category->TotalRequestsTime += Duration;
#if COUNTERSTRACE_ENABLED
		Category->TotalLoadedCounter.Add(Request.GetBuffer().DataSize());
		Category->AverageDurationCounter.Set(Category->TotalRequestsTime / double(Category->TotaRequestsCount));
#endif
	}

private:
	struct FRequestCategory
	{
		FRequestCategory(const TCHAR* Name)
#if COUNTERSTRACE_ENABLED
			: TotalLoadedCounter(TraceCounterNameType_Dynamic, *FString::Printf(TEXT("IoDispatcher/TotalLoaded (%s)"), Name), TraceCounterDisplayHint_Memory)
			, AverageDurationCounter(TraceCounterNameType_Dynamic, *FString::Printf(TEXT("IoDispatcher/AverageDuration (%s)"), Name), TraceCounterDisplayHint_None)
#endif
		{

		}

#if COUNTERSTRACE_ENABLED
		FCountersTrace::FCounterInt TotalLoadedCounter;
		FCountersTrace::FCounterFloat AverageDurationCounter;
#endif
		uint64 TotaRequestsCount = 0;
		double TotalRequestsTime = 0.0;
	};

#if CSV_PROFILER
	bool TickCsv(float DeltaTime)
	{
		CSV_CUSTOM_STAT_DEFINED(PendingIoRequests, static_cast<int32>(PendingIoRequests), ECsvCustomStatOp::Set);
		return true; // Keep ticking
	}
#endif

	int64 PendingIoRequests = 0;
#if COUNTERSTRACE_ENABLED
	FCountersTrace::FCounterInt PendingIoRequestsCounter;
#endif
	TArray<FRequestCategory> Categories;
	FRequestCategory* ChunkTypeToCategoryMap[static_cast<int32>(EIoChunkType::MAX)];
#if CSV_PROFILER
	FTSTicker::FDelegateHandle TickerHandle;
#endif
};
#else
class FIoRequestStats
{
public:
	void OnBatchIssued(FIoBatch& Batch) {}
	void OnRequestStarted(FIoRequestImpl& Request) {}
	void OnRequestCompleted(FIoRequestImpl& Request) {}
};
#endif

template <typename T, uint32 BlockSize = 128>
class TBlockAllocator
{
public:
	~TBlockAllocator()
	{
		FreeBlocks();
	}

	FORCEINLINE T* Alloc()
	{
		FScopeLock _(&CriticalSection);

		if (!NextFree)
		{
			LLM_SCOPE(ELLMTag::FileSystem);
			//TODO: Virtual alloc
			FBlock* Block = new FBlock;

			for (int32 ElementIndex = 0; ElementIndex < BlockSize; ++ElementIndex)
			{
				FElement* Element = &Block->Elements[ElementIndex];
				Element->Next = NextFree;
				NextFree = Element;
			}

			Block->Next = Blocks;
			Blocks = Block;
		}

		FElement* Element = NextFree;
		NextFree = Element->Next;

		++NumElements;

		return Element->Buffer.GetTypedPtr();
	}

	FORCEINLINE void Free(T* Ptr)
	{
		FScopeLock _(&CriticalSection);

		FElement* Element = reinterpret_cast<FElement*>(Ptr);
		Element->Next = NextFree;
		NextFree = Element;

		--NumElements;
	}

	template <typename... ArgsType>
	T* Construct(ArgsType&&... Args)
	{
		return new(Alloc()) T(Forward<ArgsType>(Args)...);
	}

	void Destroy(T* Ptr)
	{
		Ptr->~T();
		Free(Ptr);
	}

	void Trim()
	{
		FScopeLock _(&CriticalSection);
		if (!NumElements)
		{
			FreeBlocks();
		}
	}

private:
	void FreeBlocks()
	{
		FBlock* Block = Blocks;
		while (Block)
		{
			FBlock* Tmp = Block;
			Block = Block->Next;
			delete Tmp;
		}

		Blocks = nullptr;
		NextFree = nullptr;
		NumElements = 0;
	}

	struct FElement
	{
		TTypeCompatibleBytes<T> Buffer;
		FElement* Next;
	};

	struct FBlock
	{
		FElement Elements[BlockSize];
		FBlock* Next = nullptr;
	};

	FBlock*				Blocks = nullptr;
	FElement*			NextFree = nullptr;
	int32				NumElements = 0;
	FCriticalSection	CriticalSection;
};

class FIoRequestAllocator
{
public:
	void AddRef()
	{
		RefCount.IncrementExchange();
	}

	void ReleaseRef()
	{
		if (RefCount.DecrementExchange() == 1)
		{
			delete this;
		}
	}

	FIoRequestImpl* AllocRequest(const FIoChunkId& ChunkId, FIoReadOptions Options)
	{
		FIoRequestImpl* Request = BlockAllocator.Construct(*this);
		AddRef();

		Request->ChunkId = ChunkId;
		Request->Options = Options;

		return Request;
	}

	void FreeRequest(FIoRequestImpl* Request)
	{
		BlockAllocator.Destroy(Request);
		ReleaseRef();
	}

	void Trim()
	{
		BlockAllocator.Trim();
	}

private:
	TAtomic<int32> RefCount;
	TBlockAllocator<FIoRequestImpl, 4096> BlockAllocator;
};

class FIoDispatcherImpl
	: public FRunnable
{
public:
	FIoDispatcherImpl(bool bInIsMultithreaded)
		: BackendContext(MakeShared<FIoDispatcherBackendContext>())
	{
		RequestAllocator = new FIoRequestAllocator();
		RequestAllocator->AddRef();
		DispatcherEvent = FPlatformProcess::GetSynchEventFromPool(false);
		BackendContext->WakeUpDispatcherThreadDelegate.BindRaw(this, &FIoDispatcherImpl::WakeUpDispatcherThread);
		BackendContext->bIsMultiThreaded = bInIsMultithreaded;
		MemoryTrimDelegateHandle = FCoreDelegates::GetMemoryTrimDelegate().AddLambda([this]()
		{
			RequestAllocator->Trim();
			BatchAllocator.Trim();
		});
	}

	~FIoDispatcherImpl()
	{
		delete Thread;
		for (const FBackendAndPriority& Backend : Backends)
		{
			Backend.Value->Shutdown();
		}
		FCoreDelegates::GetMemoryTrimDelegate().Remove(MemoryTrimDelegateHandle);
		BackendContext->WakeUpDispatcherThreadDelegate.Unbind();
		FPlatformProcess::ReturnSynchEventToPool(DispatcherEvent);
		RequestAllocator->ReleaseRef();
	}

	void Initialize()
	{
		if (bIsInitialized)
		{
			return;
		}
		bIsInitialized = true;
		if (!Backends.IsEmpty())
		{
			for (const FBackendAndPriority& Backend : Backends)
			{
				Backend.Value->Initialize(BackendContext);
			}
			// If there are no mounted backends the resolve thread is not needed
			StartThread();
		}
	}

	FIoBatchImpl* AllocBatch()
	{
		LLM_SCOPE(ELLMTag::FileSystem);
		FIoBatchImpl* Batch = BatchAllocator.Construct();

		return Batch;
	}

	void WakeUpDispatcherThread()
	{
		if (BackendContext->bIsMultiThreaded)
		{
			DispatcherEvent->Trigger();
		}
	}

	void Cancel(FIoRequestImpl* Request)
	{
		if (!BackendContext->bIsMultiThreaded)
		{
			return;
		}
		Request->AddRef();
		{
			FScopeLock _(&UpdateLock);
			RequestsToCancel.Add(Request);
		}
		DispatcherEvent->Trigger();
	}

	void Reprioritize(FIoRequestImpl* Request)
	{
		if (!BackendContext->bIsMultiThreaded)
		{
			return;
		}
		Request->AddRef();
		{
			FScopeLock _(&UpdateLock);
			RequestsToReprioritize.Add(Request);
		}
		DispatcherEvent->Trigger();
	}

	TIoStatusOr<FIoMappedRegion> OpenMapped(const FIoChunkId& ChunkId, const FIoReadOptions& Options)
	{
		if (ChunkId.IsValid())
		{
			FReadScopeLock _(BackendsLock);
			for (const FBackendAndPriority& Backend : Backends)
			{
				TIoStatusOr<FIoMappedRegion> Result = Backend.Value->OpenMapped(ChunkId, Options);
				if (Result.IsOk())
				{
					return Result;
				}
			}
			return FIoStatus(EIoErrorCode::NotFound);
		}
		else
		{
			return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("FIoChunkId is not valid"));
		}
	}

	void Mount(TSharedRef<IIoDispatcherBackend> Backend, int32 Priority)
	{
		check(IsInGameThread());

		if (bIsInitialized)
		{
			Backend->Initialize(BackendContext);
		}
		{
			FWriteScopeLock _(BackendsLock);
			int32 Index = Algo::LowerBoundBy(Backends, Priority, &FBackendAndPriority::Key, TGreater<>());
			Backends.Insert(MakeTuple(Priority, Backend), Index);
		}
		if (bIsInitialized && !Thread)
		{
			StartThread();
		}
	}

	bool DoesChunkExist(const FIoChunkId& ChunkId) const
	{
		FReadScopeLock _(BackendsLock);
		for (const FBackendAndPriority& Backend : Backends)
		{
			if (Backend.Value->DoesChunkExist(ChunkId))
			{
				return true;
			}
		}
		return false;
	}

	bool DoesChunkExist(const FIoChunkId& ChunkId, const FIoOffsetAndLength& ChunkRange) const
	{
		FReadScopeLock _(BackendsLock);
		for (const FBackendAndPriority& Backend : Backends)
		{
			if (Backend.Value->DoesChunkExist(ChunkId, ChunkRange))
			{
				return true;
			}
		}
		return false;
	}

	TIoStatusOr<uint64> GetSizeForChunk(const FIoChunkId& ChunkId) const
	{
		// Only attempt to find the size if the FIoChunkId is valid
		if (ChunkId.IsValid())
		{
			FReadScopeLock _(BackendsLock);
			for (const FBackendAndPriority& Backend : Backends)
			{
				TIoStatusOr<uint64> Result = Backend.Value->GetSizeForChunk(ChunkId);
				if (Result.IsOk())
				{
					return Result;
				}
			}
			return FIoStatus(EIoErrorCode::NotFound);
		}
		else
		{
			return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("FIoChunkId is not valid"));
		}	
	}
	
	TIoStatusOr<uint64> GetSizeForChunk(const FIoChunkId& ChunkId, const FIoOffsetAndLength& ChunkRange, uint64& OutAvailable) const
	{
		// Only attempt to find the size if the FIoChunkId is valid
		if (ChunkId.IsValid())
		{
			FReadScopeLock _(BackendsLock);
			for (const FBackendAndPriority& Backend : Backends)
			{
				TIoStatusOr<uint64> Result = Backend.Value->GetSizeForChunk(ChunkId, ChunkRange, OutAvailable);
				if (Result.IsOk())
				{
					return Result;
				}
			}
			return FIoStatus(EIoErrorCode::NotFound);
		}
		else
		{
			return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("FIoChunkId is not valid"));
		}	
	}

	FIoSignatureErrorDelegate& OnSignatureError()
	{
		return BackendContext->SignatureErrorDelegate;
	}

	void IssueBatchInternal(FIoBatch& Batch, FIoBatchImpl* BatchImpl)
	{
		if (!Batch.HeadRequest)
		{
			if (BatchImpl)
			{
				CompleteBatch(BatchImpl);
			}
			return;
		}
		check(Batch.TailRequest);

		if (!HasMountedBackend())
		{
			FIoRequestImpl* Request = Batch.HeadRequest;
			while (Request)
			{
				FIoRequestImpl* NextRequest = Request->NextRequest;
				CompleteRequest(Request, EIoErrorCode::NotFound);
				Request->ReleaseRef();
				Request = NextRequest;
			}
			Batch.HeadRequest = Batch.TailRequest = nullptr;
			if (BatchImpl)
			{
				CompleteBatch(BatchImpl);
			}
			return;
		}

		uint32 RequestCount = 0;
		FIoRequestImpl* Request = Batch.HeadRequest;
		while (Request)
		{
			Request->Batch = BatchImpl;
			Request = Request->NextRequest;
			++RequestCount;
		}
		if (BatchImpl)
		{
			BatchImpl->UnfinishedRequestsCount += RequestCount;
		}

		RequestStats.OnBatchIssued(Batch);
		
		{
			FScopeLock _(&WaitingLock);
			if (!WaitingRequestsHead)
			{
				WaitingRequestsHead = Batch.HeadRequest;
			}
			else
			{
				WaitingRequestsTail->NextRequest = Batch.HeadRequest;
			}
			WaitingRequestsTail = Batch.TailRequest;
		}
		Batch.HeadRequest = Batch.TailRequest = nullptr;
		if (BackendContext->bIsMultiThreaded)
		{
			DispatcherEvent->Trigger();
		}
		else
		{
			LLM_SCOPE(ELLMTag::FileSystem);
			ProcessIncomingRequests();
			while (PendingIoRequestsCount > 0)
			{
				ProcessCompletedRequests();
			}
		}
	}

	void IssueBatch(FIoBatch& Batch)
	{
		IssueBatchInternal(Batch, nullptr);
	}

	void IssueBatchWithCallback(FIoBatch& Batch, TFunction<void()>&& Callback)
	{
		FIoBatchImpl* Impl = AllocBatch();
		Impl->Callback = MoveTemp(Callback);
		IssueBatchInternal(Batch, Impl);
	}

	void IssueBatchAndTriggerEvent(FIoBatch& Batch, FEvent* Event)
	{
		FIoBatchImpl* Impl = AllocBatch();
		Impl->Event = Event;
		IssueBatchInternal(Batch, Impl);
	}

	void IssueBatchAndDispatchSubsequents(FIoBatch& Batch, FGraphEventRef GraphEvent)
	{
		FIoBatchImpl* Impl = AllocBatch();
		Impl->GraphEvent = MoveTemp(GraphEvent);
		IssueBatchInternal(Batch, Impl);
	}

	int64 GetTotalLoaded() const
	{
		return TotalLoaded;
	}

	bool HasMountedBackend() const
	{
		FReadScopeLock _(BackendsLock);
		return Backends.Num() > 0;
	}

private:
	friend class FIoBatch;
	friend class FIoRequest;

	void StartThread()
	{
		check(!Thread);
		Thread = FRunnableThread::Create(this, TEXT("IoDispatcher"), 0, TPri_AboveNormal, FPlatformAffinity::GetIoDispatcherThreadMask());
	}

	void ProcessCompletedRequests()
	{
		//TRACE_CPUPROFILER_EVENT_SCOPE(ProcessCompletedRequests);

		FReadScopeLock _(BackendsLock);
		for (const FBackendAndPriority& Backend : Backends)
		{
			FIoRequestImpl* CompletedRequestsHead = Backend.Value->GetCompletedRequests();
			while (CompletedRequestsHead)
			{
				FIoRequestImpl* NextRequest = CompletedRequestsHead->NextRequest;
				if (CompletedRequestsHead->bCancelled)
				{
					CompleteRequest(CompletedRequestsHead, EIoErrorCode::Cancelled);
				}
				else if (CompletedRequestsHead->bFailed)
				{
					CompleteRequest(CompletedRequestsHead, EIoErrorCode::ReadError);
				}
				else
				{
					UE_CLOG(!CompletedRequestsHead->HasBuffer(), LogStreaming, Fatal, TEXT("Backend provided a completed request without an IoBuiffer. Requests that are not failed or cancelled must have an IoBuffer"));
					FPlatformAtomics::InterlockedAdd(&TotalLoaded, CompletedRequestsHead->GetBuffer().DataSize());
					CompleteRequest(CompletedRequestsHead, EIoErrorCode::Ok);
				}
				CompletedRequestsHead->ReleaseRef();
				CompletedRequestsHead = NextRequest;
				--PendingIoRequestsCount;
			}
		}
	}

	void CompleteBatch(FIoBatchImpl* Batch)
	{
		if (Batch->Callback)
		{
			Batch->Callback();
		}
		if (Batch->Event)
		{
			Batch->Event->Trigger();
		}
		if (Batch->GraphEvent)
		{
			Batch->GraphEvent->DispatchSubsequents();
		}
		BatchAllocator.Destroy(Batch);
	}

	bool CompleteRequest(FIoRequestImpl* Request, EIoErrorCode Status)
	{
		//TRACE_CPUPROFILER_EVENT_SCOPE(CompleteRequest);

		EIoErrorCode ExpectedStatus = EIoErrorCode::Unknown;
		if (!Request->ErrorCode.CompareExchange(ExpectedStatus, Status))
		{
			return false;
		}

		RequestStats.OnRequestCompleted(*Request);

		FIoBatchImpl* Batch = Request->Batch;
		if (Request->Callback)
		{
			TIoStatusOr<FIoBuffer> Result;
			if (Status == EIoErrorCode::Ok)
			{
				Result = Request->GetBuffer();
			}
			else
			{
				Result = Status;
			}
			Request->Callback(Result);
			Request->Callback = {};
		}
		if (Batch)
		{
			check(Batch->UnfinishedRequestsCount);
			if (--Batch->UnfinishedRequestsCount == 0)
			{
				CompleteBatch(Batch);
			}
		}
		return true;
	}

	void ProcessIncomingRequests()
	{
		FIoRequestImpl* RequestsToSubmitHead = nullptr;
		FIoRequestImpl* RequestsToSubmitTail = nullptr;
		//TRACE_CPUPROFILER_EVENT_SCOPE(ProcessIncomingRequests);
		for (;;)
		{
			{
				FScopeLock _(&WaitingLock);
				if (WaitingRequestsHead)
				{
					if (RequestsToSubmitTail)
					{
						RequestsToSubmitTail->NextRequest = WaitingRequestsHead;
						RequestsToSubmitTail = WaitingRequestsTail;
					}
					else
					{
						RequestsToSubmitHead = WaitingRequestsHead;
						RequestsToSubmitTail = WaitingRequestsTail;
					}
					WaitingRequestsHead = WaitingRequestsTail = nullptr;
				}
			}
			TArray<FIoRequestImpl*> LocalRequestsToCancel;
			TArray<FIoRequestImpl*> LocalRequestsToReprioritize;
			{
				FScopeLock _(&UpdateLock);
				Swap(LocalRequestsToCancel, RequestsToCancel);
				Swap(LocalRequestsToReprioritize, RequestsToReprioritize);
			}
			for (FIoRequestImpl* RequestToCancel : LocalRequestsToCancel)
			{
				if (!RequestToCancel->bCancelled)
				{
					RequestToCancel->bCancelled = true;
					if (RequestToCancel->Backend)
					{
						RequestToCancel->Backend->CancelIoRequest(RequestToCancel);
					}
				}
				RequestToCancel->ReleaseRef();
			}
			for (FIoRequestImpl* RequestToRePrioritize : LocalRequestsToReprioritize)
			{
				if (RequestToRePrioritize->Backend)
				{
					RequestToRePrioritize->Backend->UpdatePriorityForIoRequest(RequestToRePrioritize);
				}
				RequestToRePrioritize->ReleaseRef();
			}
			if (!RequestsToSubmitHead)
			{
				return;
			}

			FIoRequestImpl* Request = RequestsToSubmitHead;
			RequestsToSubmitHead = RequestsToSubmitHead->NextRequest;
			Request->NextRequest = nullptr;

			if (!RequestsToSubmitHead)
			{
				RequestsToSubmitTail = nullptr;
			}

			RequestStats.OnRequestStarted(*Request);
			if (Request->bCancelled)
			{
				CompleteRequest(Request, EIoErrorCode::Cancelled);
				Request->ReleaseRef();
				continue;
			}
			
			// Make sure that the FIoChunkId in the request is valid before we try to do anything with it.
			if (Request->ChunkId.IsValid())
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(ResolveRequest);
				bool bResolved = false;
				FReadScopeLock _(BackendsLock);
				for (const FBackendAndPriority& Backend : Backends)
				{
					if (Backend.Value->Resolve(Request))
					{
						bResolved = true;
						Request->Backend = &Backend.Value.Get();
						break;
					}
				}
				if (!bResolved)
				{
					CompleteRequest(Request, EIoErrorCode::NotFound);
					Request->ReleaseRef();
					continue;
				}
			}
			else
			{
				CompleteRequest(Request, EIoErrorCode::InvalidParameter);
				Request->ReleaseRef();
				continue;
			}
			
			++PendingIoRequestsCount;
		}
	}

	virtual bool Init()
	{
		return true;
	}

	virtual uint32 Run()
	{
		FMemory::SetupTLSCachesOnCurrentThread();

		LLM_SCOPE(ELLMTag::FileSystem);
		while (!bStopRequested)
		{
			if (PendingIoRequestsCount)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(IoDispatcherWaitForIo);
				DispatcherEvent->Wait();
			}
			else
			{
				DispatcherEvent->Wait();
			}
			ProcessIncomingRequests();
			ProcessCompletedRequests();
		}

		ProcessIncomingRequests();
		while (PendingIoRequestsCount > 0)
		{
			ProcessCompletedRequests();
		}
		return 0;
	}

	virtual void Stop()
	{
		bStopRequested = true;
		DispatcherEvent->Trigger();
	}

	using FBatchAllocator = TBlockAllocator<FIoBatchImpl, 4096>;
	using FBackendAndPriority = TTuple<int32, TSharedRef<IIoDispatcherBackend>>;

	TSharedRef<FIoDispatcherBackendContext> BackendContext;
	FDelegateHandle MemoryTrimDelegateHandle;
	mutable FRWLock BackendsLock;
	TArray<FBackendAndPriority> Backends;
	FIoRequestAllocator* RequestAllocator = nullptr;
	FBatchAllocator BatchAllocator;
	FRunnableThread* Thread = nullptr;
	FEvent* DispatcherEvent = nullptr;
	FCriticalSection WaitingLock;
	FIoRequestImpl* WaitingRequestsHead = nullptr;
	FIoRequestImpl* WaitingRequestsTail = nullptr;
	FCriticalSection UpdateLock;
	TArray<FIoRequestImpl*> RequestsToCancel;
	TArray<FIoRequestImpl*> RequestsToReprioritize;
	TAtomic<bool> bStopRequested { false };
	FIoDispatcher::FIoContainerUnmountedEvent ContainerUnmountedEvent;
	uint64 PendingIoRequestsCount = 0;
	int64 TotalLoaded = 0;
	FIoRequestStats RequestStats;
	bool bIsInitialized = false;
};

FIoDispatcher::FIoDispatcher()
	: Impl(new FIoDispatcherImpl(FGenericPlatformProcess::SupportsMultithreading()))
{
}

FIoDispatcher::~FIoDispatcher()
{
	delete Impl;
}

void
FIoDispatcher::Mount(TSharedRef<IIoDispatcherBackend> Backend, int32 Priority)
{
	Impl->Mount(Backend, Priority);
}

FIoBatch
FIoDispatcher::NewBatch()
{
	return FIoBatch(*Impl);
}

TIoStatusOr<FIoMappedRegion>
FIoDispatcher::OpenMapped(const FIoChunkId& ChunkId, const FIoReadOptions& Options)
{
	return Impl->OpenMapped(ChunkId, Options);
}

// Polling methods
bool
FIoDispatcher::DoesChunkExist(const FIoChunkId& ChunkId) const
{
	return Impl->DoesChunkExist(ChunkId);
}

bool
FIoDispatcher::DoesChunkExist(const FIoChunkId& ChunkId, const FIoOffsetAndLength& ChunkRange) const
{
	return Impl->DoesChunkExist(ChunkId, ChunkRange);
}

TIoStatusOr<uint64>
FIoDispatcher::GetSizeForChunk(const FIoChunkId& ChunkId) const
{
	return Impl->GetSizeForChunk(ChunkId);
}

TIoStatusOr<uint64>
FIoDispatcher::GetSizeForChunk(const FIoChunkId& ChunkId, const FIoOffsetAndLength& ChunkRange, uint64& OutAvailable) const
{
	return Impl->GetSizeForChunk(ChunkId, ChunkRange, OutAvailable);
}

int64
FIoDispatcher::GetTotalLoaded() const
{
	return Impl->GetTotalLoaded();
}

FIoSignatureErrorDelegate&
FIoDispatcher::OnSignatureError()
{
	return Impl->OnSignatureError();
}

static bool
HasScriptObjectsChunk(FIoDispatcher& Dispatcher)
{
	static bool bHasScriptObjectsChunk = Dispatcher.DoesChunkExist(CreateIoChunkId(0, 0, EIoChunkType::ScriptObjects));
	return bHasScriptObjectsChunk;
}

bool
FIoDispatcher::IsInitialized()
{
	return GIoDispatcher.IsValid();
}

FIoStatus
FIoDispatcher::Initialize()
{
	LLM_SCOPE(ELLMTag::FileSystem);
	check(!GIoDispatcher);
	GIoDispatcher = MakeUnique<FIoDispatcher>();
	return FIoStatus::Ok;
}

void
FIoDispatcher::InitializePostSettings()
{
	LLM_SCOPE(ELLMTag::FileSystem);
	check(GIoDispatcher);
	GIoDispatcher->Impl->Initialize();
}

void
FIoDispatcher::Shutdown()
{
	TUniquePtr<FIoDispatcher> LocalIoDispatcher(MoveTemp(GIoDispatcher));
}

FIoDispatcher&
FIoDispatcher::Get()
{
	check(GIoDispatcher);
	return *GIoDispatcher;
}

//////////////////////////////////////////////////////////////////////////

FIoBatch::FIoBatch(FIoDispatcherImpl& InDispatcher)
	: Dispatcher(&InDispatcher)
{
}

FIoBatch::FIoBatch()
	: Dispatcher(GIoDispatcher.IsValid() ? GIoDispatcher->Impl : nullptr)
{
}

FIoBatch::FIoBatch(FIoBatch&& Other)
{
	Dispatcher = Other.Dispatcher;
	HeadRequest = Other.HeadRequest;
	TailRequest = Other.TailRequest;
	Other.HeadRequest = nullptr;
	Other.TailRequest = nullptr;
}

FIoBatch::~FIoBatch()
{
	FIoRequestImpl* Request = HeadRequest;
	while (Request)
	{
		FIoRequestImpl* NextRequest = Request->NextRequest;
		Request->ReleaseRef();
		Request = NextRequest;
	}
}

FIoBatch&
FIoBatch::operator=(FIoBatch&& Other)
{
	if (&Other == this)
	{
		return *this;
	}
	FIoRequestImpl* Request = HeadRequest;
	while (Request)
	{
		FIoRequestImpl* NextRequest = Request->NextRequest;
		Request->ReleaseRef();
		Request = NextRequest;
	}
	Dispatcher = Other.Dispatcher;
	HeadRequest = Other.HeadRequest;
	TailRequest = Other.TailRequest;
	Other.HeadRequest = nullptr;
	Other.TailRequest = nullptr;
	return *this;
}

FIoRequestImpl*
FIoBatch::ReadInternal(const FIoChunkId& ChunkId, const FIoReadOptions& Options, int32 Priority)
{
	FIoRequestImpl* Request = Dispatcher->RequestAllocator->AllocRequest(ChunkId, Options);
	Request->Priority = Priority;
	Request->AddRef();
	if (!HeadRequest)
	{
		check(!TailRequest);
		HeadRequest = TailRequest = Request;
	}
	else
	{
		check(TailRequest);
		TailRequest->NextRequest = Request;
		TailRequest = Request;
	}
	return Request;
}

FIoRequest
FIoBatch::Read(const FIoChunkId& ChunkId, FIoReadOptions Options, int32 Priority)
{
	FIoRequestImpl* Request = ReadInternal(ChunkId, Options, Priority);
	return FIoRequest(Request);
}

FIoRequest
FIoBatch::ReadWithCallback(const FIoChunkId& ChunkId, const FIoReadOptions& Options, int32 Priority, FIoReadCallback&& Callback)
{
	FIoRequestImpl* Request = ReadInternal(ChunkId, Options, Priority);
	Request->Callback = MoveTemp(Callback);
	return FIoRequest(Request);
}

void
FIoBatch::Issue()
{
	Dispatcher->IssueBatch(*this);
}

void
FIoBatch::IssueWithCallback(TFunction<void()>&& Callback)
{
	Dispatcher->IssueBatchWithCallback(*this, MoveTemp(Callback));
}

void
FIoBatch::IssueAndTriggerEvent(FEvent* Event)
{
	Dispatcher->IssueBatchAndTriggerEvent(*this, Event);
}

void
FIoBatch::IssueAndDispatchSubsequents(FGraphEventRef Event)
{
	Dispatcher->IssueBatchAndDispatchSubsequents(*this, MoveTemp(Event));
}

//////////////////////////////////////////////////////////////////////////

void FIoRequestImpl::CreateBuffer(uint64 Size)
{
	if (void* TargetVa = Options.GetTargetVa())
	{
		Buffer.Emplace(FIoBuffer::Wrap, TargetVa, Size);
	}
	else
	{
		UE::FInheritedContextScope InheritedContextScope = RestoreInheritedContext();
		TRACE_CPUPROFILER_EVENT_SCOPE(AllocMemoryForRequest);
		Buffer.Emplace(Size);
	}
}

void FIoRequestImpl::FreeRequest()
{
	Allocator.FreeRequest(this);
}

FIoRequest::FIoRequest(FIoRequestImpl* InImpl)
	: Impl(InImpl)
{
	if (Impl)
	{
		Impl->AddRef();
	}
}

FIoRequest::FIoRequest(const FIoRequest& Other)
{
	if (Other.Impl)
	{
		Impl = Other.Impl;
		Impl->AddRef();
	}
}

FIoRequest::FIoRequest(FIoRequest&& Other)
{
	Impl = Other.Impl;
	Other.Impl = nullptr;
}

FIoRequest& FIoRequest::operator=(const FIoRequest& Other)
{
	if (Other.Impl)
	{
		Other.Impl->AddRef();
	}
	if (Impl)
	{
		Impl->ReleaseRef();
	}
	Impl = Other.Impl;
	return *this;
}

FIoRequest& FIoRequest::operator=(FIoRequest&& Other)
{
	if (Impl)
	{
		Impl->ReleaseRef();
	}
	Impl = Other.Impl;
	Other.Impl = nullptr;
	return *this;
}

FIoRequest::~FIoRequest()
{
	if (Impl)
	{
		Impl->ReleaseRef();
	}
}

FIoStatus
FIoRequest::Status() const
{
	if (Impl)
	{
		return Impl->ErrorCode.Load();
	}
	else
	{
		return FIoStatus::Invalid;
	}
}

const FIoBuffer*
FIoRequest::GetResult() const
{
	if (!Impl)
	{
		return nullptr;
	}
	FIoStatus Status(Impl->ErrorCode.Load());
	check(Status.IsCompleted());
	TIoStatusOr<FIoBuffer> Result;
	if (Status.IsOk())
	{
		return &Impl->GetBuffer();
	}
	else
	{
		return nullptr;
	}
}

const FIoBuffer&
FIoRequest::GetResultOrDie() const
{
	const FIoBuffer* Result = GetResult();
	if (!Result)
	{
		UE_LOG(LogIoDispatcher, Fatal, TEXT("I/O Error '%s'"), *Status().ToString());
	}
	return *Result;
}

void
FIoRequest::Cancel()
{
	if (!GIoDispatcher || !Impl)
	{
		return;
	}
	//TRACE_BOOKMARK(TEXT("FIoRequest::Cancel()"));
	GIoDispatcher->Impl->Cancel(Impl);
}

void
FIoRequest::UpdatePriority(uint32 NewPriority)
{
	if (!GIoDispatcher || !Impl || Impl->Priority == NewPriority)
	{
		return;
	}
	Impl->Priority = NewPriority;
	GIoDispatcher->Impl->Reprioritize(Impl);
}

void
FIoRequest::Release()
{
	if (Impl)
	{
		Impl->ReleaseRef();
		Impl = nullptr;
	}
}
