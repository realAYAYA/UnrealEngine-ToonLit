// Copyright Epic Games, Inc. All Rights Reserved.
#include "Serialization/BulkData.h"
#include "Async/ManualResetEvent.h"
#include "Async/MappedFileHandle.h"
#include "Containers/ChunkedArray.h"
#include "HAL/CriticalSection.h"
#include "IO/IoDispatcher.h"
#include "IO/IoOffsetLength.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Timespan.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "Serialization/MemoryReader.h"
#include "Templates/RefCounting.h"
#include "UObject/LinkerLoad.h"
#include "UObject/Package.h"
#include "UObject/PackageResourceIoDispatcherBackend.h"

//////////////////////////////////////////////////////////////////////////////

TRACE_DECLARE_ATOMIC_INT_COUNTER(BulkDataBatchRequest_Count, TEXT("BulkData/BatchRequest/Count"));
TRACE_DECLARE_ATOMIC_INT_COUNTER(BulkDataBatchRequest_PendingCount, TEXT("BulkData/BatchRequest/Pending"));

/**
 * When enabled calls to FChunkReadFileHandle::ReadRequest will validate that the request
 * is within the bulkdata payload bounds. Currently disabled as FFileCache still uses the
 * handle to represent the entire .ubulk file rather than the specific bulkdata payload.
 */
#define UE_ENABLE_BULKDATA_RANGE_TEST 0

FBulkDataIORequest::FBulkDataIORequest(IAsyncReadFileHandle* InFileHandle)
	: FileHandle(InFileHandle)
	, ReadRequest(nullptr)
	, Size(INDEX_NONE)
{
}

FBulkDataIORequest::FBulkDataIORequest(IAsyncReadFileHandle* InFileHandle, IAsyncReadRequest* InReadRequest, int64 BytesToRead)
	: FileHandle(InFileHandle)
	, ReadRequest(InReadRequest)
	, Size(BytesToRead)
{

}

FBulkDataIORequest::~FBulkDataIORequest()
{
	delete ReadRequest;
	delete FileHandle;
}

bool FBulkDataIORequest::MakeReadRequest(int64 Offset, int64 BytesToRead, EAsyncIOPriorityAndFlags PriorityAndFlags, FBulkDataIORequestCallBack* CompleteCallback, uint8* UserSuppliedMemory)
{
	check(ReadRequest == nullptr);

	FBulkDataIORequestCallBack LocalCallback = *CompleteCallback;
	FAsyncFileCallBack AsyncFileCallBack = [LocalCallback, BytesToRead, this](bool bWasCancelled, IAsyncReadRequest* InRequest)
	{
		// In some cases the call to ReadRequest can invoke the callback immediately (if the requested data is cached 
		// in the pak file system for example) which means that FBulkDataIORequest::ReadRequest might not actually be
		// set correctly, so we need to make sure it is assigned before we invoke LocalCallback!
		ReadRequest = InRequest;

		Size = BytesToRead;
		LocalCallback(bWasCancelled, this);
	};

	ReadRequest = FileHandle->ReadRequest(Offset, BytesToRead, PriorityAndFlags, &AsyncFileCallBack, UserSuppliedMemory);
	
	if (ReadRequest != nullptr)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool FBulkDataIORequest::PollCompletion() const
{
	return ReadRequest->PollCompletion();
}

bool FBulkDataIORequest::WaitCompletion(float TimeLimitSeconds)
{
	return ReadRequest->WaitCompletion(TimeLimitSeconds);
}

uint8* FBulkDataIORequest::GetReadResults()
{
	return ReadRequest->GetReadResults();
}

int64 FBulkDataIORequest::GetSize() const
{
	return Size;
}

void FBulkDataIORequest::Cancel()
{
	ReadRequest->Cancel();
}

//////////////////////////////////////////////////////////////////////////////

namespace UE::BulkData::Private
{

enum class EChunkRequestStatus : uint32
{
	None				= 0,
	Pending				= 1 << 0,
	Canceled			= 1 << 1,
	DataReady			= 1 << 2,
	CallbackTriggered	= 1 << 3,
};
ENUM_CLASS_FLAGS(EChunkRequestStatus);

class FChunkRequest
{
public:
	virtual ~FChunkRequest();
	
	void Issue(FIoChunkId ChunkId, FIoReadOptions Options, int32 Priority);

protected:
	FChunkRequest(FIoBuffer&& InBuffer);
	
	inline EChunkRequestStatus GetStatus() const
	{
		return static_cast<EChunkRequestStatus>(Status.load(std::memory_order_consume));
	}

	virtual void HandleChunkResult(TIoStatusOr<FIoBuffer>&& Result) = 0;
	bool WaitForChunkRequest(float TimeLimitSeconds = 0.0f);
	void CancelChunkRequest();
	int64 GetSizeResult() const { return SizeResult; }
	
	FIoBuffer			Buffer;

private:

	UE::FManualResetEvent	DoneEvent;
	FIoRequest				Request;
	int64					SizeResult;
	std::atomic<uint32>		Status;
};

FChunkRequest::FChunkRequest(FIoBuffer&& InBuffer)
	: Buffer(MoveTemp(InBuffer))
	, SizeResult(-1)
	, Status{uint32(EChunkRequestStatus::None)}
{
}

FChunkRequest::~FChunkRequest()
{
	DoneEvent.Wait();
}

void FChunkRequest::Issue(FIoChunkId ChunkId, FIoReadOptions Options, int32 Priority)
{
	Status.store(uint32(EChunkRequestStatus::Pending), std::memory_order_release); 

	check(Options.GetSize() == Buffer.GetSize());
	Options.SetTargetVa(Buffer.GetData());

	FIoBatch IoBatch = FIoDispatcher::Get().NewBatch();
	Request = IoBatch.ReadWithCallback(ChunkId, Options, Priority, [this](TIoStatusOr<FIoBuffer> Result)
	{
		EChunkRequestStatus ReadyOrCanceled = EChunkRequestStatus::Canceled;

		if (Result.IsOk())
		{
			SizeResult = Result.ValueOrDie().GetSize();
			ReadyOrCanceled = EChunkRequestStatus::DataReady;
		}

		Status.store(uint32(ReadyOrCanceled), std::memory_order_release); 
		HandleChunkResult(MoveTemp(Result));
		Status.store(uint32(ReadyOrCanceled | EChunkRequestStatus::CallbackTriggered), std::memory_order_release); 

		DoneEvent.Notify();
	});

	IoBatch.Issue();
}

bool FChunkRequest::WaitForChunkRequest(float TimeLimitSeconds)
{
	checkf(GetStatus() != EChunkRequestStatus::None, TEXT("The request must be issued before waiting for completion"));

	return DoneEvent.WaitFor(TimeLimitSeconds <= 0.0f ? FMonotonicTimeSpan::Infinity() : FMonotonicTimeSpan::FromSeconds(TimeLimitSeconds));
}

void FChunkRequest::CancelChunkRequest()
{
	checkf(GetStatus() != EChunkRequestStatus::None, TEXT("The request must be issued before it can be canceled"));

	uint32 Expected = uint32(EChunkRequestStatus::Pending);
	if (Status.compare_exchange_strong(Expected, uint32(EChunkRequestStatus::Canceled)))
	{
		Request.Cancel();
	}
}

//////////////////////////////////////////////////////////////////////////////

class FChunkReadFileRequest final : public FChunkRequest, public IAsyncReadRequest
{
public:
	FChunkReadFileRequest(FAsyncFileCallBack* Callback, FIoBuffer&& InBuffer);
	virtual ~FChunkReadFileRequest();
	
	virtual void WaitCompletionImpl(float TimeLimitSeconds) override;

	virtual void CancelImpl() override;
	virtual void ReleaseMemoryOwnershipImpl() override;
	virtual void HandleChunkResult(TIoStatusOr<FIoBuffer>&& Result) override;
};

FChunkReadFileRequest::FChunkReadFileRequest(FAsyncFileCallBack* Callback, FIoBuffer&& InBuffer)
	: FChunkRequest(MoveTemp(InBuffer))
	, IAsyncReadRequest(Callback, false, nullptr)
{
	Memory = Buffer.GetData();
}

FChunkReadFileRequest::~FChunkReadFileRequest()
{
	WaitForChunkRequest();

	// Calling GetReadResult transfers ownership of the read buffer
	if (Memory == nullptr && Buffer.IsMemoryOwned())
	{
		const bool bReleased = Buffer.Release().IsOk();
		check(bReleased);
	}

	Memory = nullptr;
}
	
void FChunkReadFileRequest::WaitCompletionImpl(float TimeLimitSeconds)
{
	WaitForChunkRequest(TimeLimitSeconds);
}

void FChunkReadFileRequest::CancelImpl()
{
	bCanceled = true;
	CancelChunkRequest();
}

void FChunkReadFileRequest::ReleaseMemoryOwnershipImpl()
{
}

void FChunkReadFileRequest::HandleChunkResult(TIoStatusOr<FIoBuffer>&& Result)
{
	bCanceled = Result.Status().IsOk() == false;
	SetDataComplete();
	SetAllComplete();
}

//////////////////////////////////////////////////////////////////////////////

class FChunkFileSizeRequest : public IAsyncReadRequest
{
public:
	FChunkFileSizeRequest(const FIoChunkId& ChunkId, uint64 ChunkSize, FAsyncFileCallBack* Callback)
		: IAsyncReadRequest(Callback, true, nullptr)
	{
		if (ChunkSize > 0)
		{
			Size = static_cast<int64>(ChunkSize);
		}
		SetComplete();
	}

	virtual ~FChunkFileSizeRequest() = default;

private:

	virtual void WaitCompletionImpl(float TimeLimitSeconds) override
	{
		// Even though SetComplete called in the constructor and sets bCompleteAndCallbackCalled=true, we still need to implement WaitComplete as
		// the CompleteCallback can end up starting async tasks that can overtake the constructor execution and need to wait for the constructor to finish.
		while (!*(volatile bool*)&bCompleteAndCallbackCalled);
	}

	virtual void CancelImpl() override
	{
	}

	virtual void ReleaseMemoryOwnershipImpl() override
	{
	}
};

//////////////////////////////////////////////////////////////////////////////

class FChunkReadFileHandle : public IAsyncReadFileHandle
{
public:
	FChunkReadFileHandle(const FIoChunkId& InChunkId, const FIoOffsetAndLength& InChunkRange, uint64 InChunkSize, uint64 InAvailableChunkSize) 
		: ChunkId(InChunkId)
		, ChunkRange(InChunkRange)
		, ChunkSize(InChunkSize)
		, AvailableChunkSize(InAvailableChunkSize)
	{
	}

	virtual ~FChunkReadFileHandle() = default;

	virtual IAsyncReadRequest* SizeRequest(FAsyncFileCallBack* CompleteCallback = nullptr) override;

	virtual IAsyncReadRequest* ReadRequest(
		int64 Offset,
		int64 BytesToRead,
		EAsyncIOPriorityAndFlags PriorityAndFlags = AIOP_Normal,
		FAsyncFileCallBack* CompleteCallback = nullptr,
		uint8* UserSuppliedMemory = nullptr) override;

private:
	FIoChunkId ChunkId;
	FIoOffsetAndLength ChunkRange;
	uint64 ChunkSize;
	uint64 AvailableChunkSize;
};

IAsyncReadRequest* FChunkReadFileHandle::SizeRequest(FAsyncFileCallBack* CompleteCallback)
{
	return new FChunkFileSizeRequest(ChunkId, ChunkSize, CompleteCallback);
}

IAsyncReadRequest* FChunkReadFileHandle::ReadRequest(
	int64 Offset, 
	int64 BytesToRead, 
	EAsyncIOPriorityAndFlags PriorityAndFlags,
	FAsyncFileCallBack* CompleteCallback,
	uint8* UserSuppliedMemory)
{
#if UE_ENABLE_BULKDATA_RANGE_TEST
	const bool bIsOutsideBulkDataRange =
		(Offset < static_cast<int64>(ChunkRange.GetOffset())) ||
		((Offset + BytesToRead) > static_cast<int64>(ChunkRange.GetOffset() + AvailableChunkSize));


	UE_CLOG(bIsOutsideBulkDataRange, LogSerialization, Warning,
		TEXT("Reading outside of bulk data range, RequestRange='%lld, %lld', BulkDataRange='%llu, %llu', ChunkId='%s'"),
		Offset, BytesToRead, ChunkRange.GetOffset(), ChunkRange.GetLength(), *LexToString(ChunkId));
#endif //UE_ENABLE_BULKDATA_RANGE_TEST

	FIoBuffer Buffer = UserSuppliedMemory ? FIoBuffer(FIoBuffer::Wrap, UserSuppliedMemory, BytesToRead) : FIoBuffer(BytesToRead);
	FChunkReadFileRequest* Request = new FChunkReadFileRequest(CompleteCallback, MoveTemp(Buffer));

	Request->Issue(ChunkId, FIoReadOptions(Offset, BytesToRead), ConvertToIoDispatcherPriority(PriorityAndFlags));

	return Request;
}

//////////////////////////////////////////////////////////////////////////////

class FChunkBulkDataRequest final : public FChunkRequest, public IBulkDataIORequest
{
public:
	FChunkBulkDataRequest(FBulkDataIORequestCallBack* InCallback, FIoBuffer&& InBuffer);
	
	virtual ~FChunkBulkDataRequest() = default;
	
	inline virtual bool PollCompletion() const override
	{
		checkf(GetStatus() != EChunkRequestStatus::None, TEXT("The request must be issued before polling for completion"));
		return EnumHasAnyFlags(GetStatus(), EChunkRequestStatus::CallbackTriggered);
	}

	virtual bool WaitCompletion(float TimeLimitSeconds = 0.0f) override
	{
		checkf(GetStatus() != EChunkRequestStatus::None, TEXT("The request must be issued before waiting for completion"));
		return WaitForChunkRequest(TimeLimitSeconds);
	}

	virtual uint8* GetReadResults() override;
	
	inline virtual int64 GetSize() const override
	{
		checkf(GetStatus() != EChunkRequestStatus::None, TEXT("The request must be issued before polling for size"));
		return EnumHasAnyFlags(GetStatus(), EChunkRequestStatus::DataReady) ? GetSizeResult() : -1;
	}

	virtual void Cancel() override
	{
		CancelChunkRequest();
	}
	
private:

	virtual void HandleChunkResult(TIoStatusOr<FIoBuffer>&& Result) override
	{
		if (Callback)
		{
			const bool bCanceled = Result.IsOk() == false;
			Callback(bCanceled, this);
		}
	}

	FBulkDataIORequestCallBack Callback;
};

FChunkBulkDataRequest::FChunkBulkDataRequest(FBulkDataIORequestCallBack* InCallback, FIoBuffer&& InBuffer)
	: FChunkRequest(MoveTemp(InBuffer))
{
	if (InCallback)
	{
		Callback = *InCallback;
	}
}

uint8* FChunkBulkDataRequest::GetReadResults()
{
	uint8* ReadResult = nullptr;

	if (EnumHasAnyFlags(GetStatus(), EChunkRequestStatus::DataReady))
	{
		if (Buffer.IsMemoryOwned())
		{
			ReadResult = Buffer.Release().ConsumeValueOrDie();
		}
		else
		{
			ReadResult = Buffer.GetData();
		}
	}

	return ReadResult;
}

//////////////////////////////////////////////////////////////////////////////

TUniquePtr<IBulkDataIORequest> CreateBulkDataIoDispatcherRequest(
	const FIoChunkId& ChunkId,
	int64 Offset,
	int64 Size,
	FBulkDataIORequestCallBack* Callback,
	uint8* UserSuppliedMemory,
	int32 Priority)
{
	FIoBuffer Buffer = UserSuppliedMemory ? FIoBuffer(FIoBuffer::Wrap, UserSuppliedMemory, Size) : FIoBuffer(Size);

	TUniquePtr<FChunkBulkDataRequest> Request = MakeUnique<FChunkBulkDataRequest>(Callback, MoveTemp(Buffer));
	Request->Issue(ChunkId, FIoReadOptions(Offset, Size), Priority);

	return Request;
}

//////////////////////////////////////////////////////////////////////////////

bool OpenReadBulkData(
	const FBulkMetaData& BulkMeta,
	const FIoChunkId& BulkChunkId,
	int64 Offset,
	int64 Size,
	EAsyncIOPriorityAndFlags Priority,
	TFunction<void(FArchive& Ar)>&& Read)
{
	if (BulkChunkId.IsValid() == false)
	{
		return false;
	}

	FIoBatch Batch = FIoDispatcher::Get().NewBatch();
	FIoRequest Request = Batch.Read(BulkChunkId, FIoReadOptions(Offset, Size), ConvertToIoDispatcherPriority(Priority));
	FEventRef Event;
	Batch.IssueAndTriggerEvent(Event.Get());
	Event->Wait();

	if (const FIoBuffer* Buffer = Request.GetResult())
	{
		FMemoryReaderView Ar(Buffer->GetView());
		Read(Ar);

		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////

TUniquePtr<IAsyncReadFileHandle> OpenAsyncReadBulkData(
	const FBulkMetaData& BulkMeta,
	const FIoChunkId& BulkChunkId,
	uint64 ChunkSize,
	uint64 AvailableChunkSize)
{
	if (BulkChunkId.IsValid() == false)
	{
		return TUniquePtr<IAsyncReadFileHandle>();
	}

	return MakeUnique<FChunkReadFileHandle>(BulkChunkId, BulkMeta.GetOffsetAndLength(), ChunkSize, AvailableChunkSize);
}

TUniquePtr<IAsyncReadFileHandle> OpenAsyncReadBulkData(const FBulkMetaData& BulkMeta, const FIoChunkId& BulkChunkId)
{
	if (BulkChunkId.IsValid() == false)
	{
		return TUniquePtr<IAsyncReadFileHandle>();
	}

	TIoStatusOr<uint64> Status = FIoDispatcher::Get().GetSizeForChunk(BulkChunkId); 
	const uint64 ChunkSize = Status.IsOk() ? Status.ValueOrDie() : 0;
	return MakeUnique<FChunkReadFileHandle>(BulkChunkId, BulkMeta.GetOffsetAndLength(), ChunkSize, ChunkSize);
}

//////////////////////////////////////////////////////////////////////////////

TUniquePtr<IBulkDataIORequest> CreateStreamingRequest(
	const FBulkMetaData& BulkMeta,
	const FIoChunkId& BulkChunkId,
	int64 Offset,
	int64 Size,
	EAsyncIOPriorityAndFlags Priority,
	FBulkDataIORequestCallBack* Callback,
	uint8* UserSuppliedMemory)
{
	if (BulkChunkId.IsValid() == false)
	{
		return TUniquePtr<IBulkDataIORequest>();
	}

	FIoBuffer Buffer = UserSuppliedMemory ? FIoBuffer(FIoBuffer::Wrap, UserSuppliedMemory, Size) : FIoBuffer(Size);
	FChunkBulkDataRequest* Request = new FChunkBulkDataRequest(Callback, MoveTemp(Buffer));
	Request->Issue(BulkChunkId, FIoReadOptions(Offset, Size), ConvertToIoDispatcherPriority(Priority));
	
	return TUniquePtr<IBulkDataIORequest>(Request);
}

//////////////////////////////////////////////////////////////////////////////

bool TryMemoryMapBulkData(
	const FBulkMetaData& BulkMeta,
	const FIoChunkId& BulkChunkId,
	int64 Offset,
	int64 Size,
	FIoMappedRegion& OutRegion)
{
	TIoStatusOr<FIoMappedRegion> Status = FIoDispatcher::Get().OpenMapped(BulkChunkId, FIoReadOptions(Offset, Size));

	if (Status.IsOk())
	{
		OutRegion = Status.ConsumeValueOrDie();
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////

class FAsyncBulkDataRequests
{
public:
	void AddPendingRequest(
		FBulkData* Owner,
		TUniquePtr<IAsyncReadFileHandle>&& FileHandle,
		TUniquePtr<IAsyncReadRequest>&& ReadRequest)
	{
		FScopeLock _(&RequestsCS);
		PendingRequests.Add(Owner, FPendingRequest { MoveTemp(FileHandle), MoveTemp(ReadRequest) });
	}

	void Flush(FBulkData* Owner)
	{
		FPendingRequest PendingRequest;

		{
			FScopeLock _(&RequestsCS);
			PendingRequest = MoveTemp(PendingRequests.FindChecked(Owner));
			PendingRequests.Remove(Owner);
		}
		
		PendingRequest.ReadRequest->WaitCompletion();
	}

	static FAsyncBulkDataRequests& Get()
	{
		static FAsyncBulkDataRequests Instance;
		return Instance;
	}

private:
	struct FPendingRequest
	{
		TUniquePtr<IAsyncReadFileHandle> FileHandle;
		TUniquePtr<IAsyncReadRequest> ReadRequest;
	};

	TMap<FBulkData*, FPendingRequest> PendingRequests;
	FCriticalSection RequestsCS;
};

bool StartAsyncLoad(
	FBulkData* Owner,
	const FBulkMetaData& BulkMeta,
	const FIoChunkId& BulkChunkId,
	int64 Offset,
	int64 Size,
	EAsyncIOPriorityAndFlags Priority,
	TFunction<void(TIoStatusOr<FIoBuffer>)>&& Callback)
{
	if (TUniquePtr<IAsyncReadFileHandle> FileHandle = OpenAsyncReadBulkData(BulkMeta, BulkChunkId))
	{
		FAsyncFileCallBack FileReadCallback = [Callback = MoveTemp(Callback), Size = Size]
			(bool bCanceled, IAsyncReadRequest* Request)
			{
				if (bCanceled)
				{
					Callback(FIoStatus(EIoErrorCode::Cancelled));
				}
				else if (uint8* Data = Request->GetReadResults())
				{
					Callback(FIoBuffer(FIoBuffer::AssumeOwnership, Data, Size));
				}
				else
				{
					Callback(FIoStatus(EIoErrorCode::ReadError));
				}
			};
		
		if (IAsyncReadRequest* Request = FileHandle->ReadRequest(Offset, Size, Priority, &FileReadCallback))
		{
			FAsyncBulkDataRequests::Get().AddPendingRequest(Owner, MoveTemp(FileHandle), TUniquePtr<IAsyncReadRequest>(Request));
			return true;
		}
	}

	return false;
}

void FlushAsyncLoad(FBulkData* Owner)
{
	FAsyncBulkDataRequests::Get().Flush(Owner);
}

} // namespace UE::BulkData

//////////////////////////////////////////////////////////////////////////////

class FBulkDataRequest::IHandle
{
public:
	virtual ~IHandle() = default;

	virtual void AddRef() = 0;
	virtual void Release() = 0;
	virtual uint32 GetRefCount() const = 0;

	virtual FBulkDataRequest::EStatus GetStatus() const = 0;
	virtual bool Cancel() = 0;
	virtual bool Wait(uint32 Milliseconds) = 0;
};

//////////////////////////////////////////////////////////////////////////////

class FHandleBase : public FBulkDataRequest::IHandle
{
public:
	FHandleBase() = default;
	FHandleBase(FBulkDataRequest::EStatus InStatus)
		: Status(uint32(InStatus))
	{ }
	
	virtual ~FHandleBase() = default;

	virtual void AddRef() override final
	{
		RefCount.fetch_add(1, std::memory_order_relaxed);
	}

	virtual void Release() override final
	{
		if (RefCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
		{
			delete this;
		}
	}

	virtual uint32 GetRefCount() const override final
	{
		return uint32(RefCount.load(std::memory_order_relaxed));
	}

	virtual FBulkDataBatchRequest::EStatus GetStatus() const override final
	{
		return FBulkDataBatchRequest::EStatus(Status.load(std::memory_order_consume));
	}

	void SetStatus(FBulkDataBatchRequest::EStatus InStatus)
	{
		Status.store(uint32(InStatus), std::memory_order_release);
	}

	virtual bool Cancel() override
	{
		return false;
	}

	virtual bool Wait(uint32 Milliseconds) override
	{
		return false;
	}

private:
	std::atomic<int32>	RefCount{0};
	std::atomic<uint32> Status{0};
};

//////////////////////////////////////////////////////////////////////////////

static FBulkDataRequest::EStatus GetStatusFromIoErrorCode(const EIoErrorCode ErrorCode)
{
	if (ErrorCode == EIoErrorCode::Unknown)
	{
		return FBulkDataRequest::EStatus::Pending;
	}
	else if (ErrorCode == EIoErrorCode::Ok)
	{
		return FBulkDataRequest::EStatus::Ok;
	}
	else if (ErrorCode == EIoErrorCode::Cancelled)
	{
		return FBulkDataRequest::EStatus::Cancelled;
	}
	else
	{
		return FBulkDataRequest::EStatus::Error;
	}
}

class FChunkBatchReadRequest final : public FBulkDataRequest::IHandle 
{
public:
	FChunkBatchReadRequest() = default;
	FChunkBatchReadRequest(FBulkDataRequest::IHandle* InBatch)
		: Batch(InBatch)
	{ }

	virtual void AddRef() override
	{
		Batch->AddRef();
	}

	virtual void Release() override
	{
		Batch->Release();
	}

	virtual uint32 GetRefCount() const override
	{
		return Batch->GetRefCount();
	}

	virtual FBulkDataRequest::EStatus GetStatus() const override
	{
		return GetStatusFromIoErrorCode(IoHandle.Status().GetErrorCode());
	}

	virtual bool Cancel() override
	{
		if (IoHandle.Status().GetErrorCode() == EIoErrorCode::Unknown)
		{
			IoHandle.Cancel();
			return true;
		}
		return false;
	}

	virtual bool Wait(uint32 Milliseconds) override
	{
		checkNoEntry(); // Bulk read requests is currently not awaitable
		return true;
	}

	FBulkDataRequest::IHandle* Batch = nullptr;
	FIoRequest IoHandle;
};

class FBulkDataBatchRequest::FBatchHandle
	: public FHandleBase
{
public:
	FBatchHandle(int32 BatchMaxCount)
	{
		if (BatchMaxCount > 0)
		{
			Requests.Reserve(BatchMaxCount);
		}

		TRACE_COUNTER_INCREMENT(BulkDataBatchRequest_Count);
	}
	
	~FBatchHandle()
	{
		Cancel();
		Wait(MAX_uint32);
		
		TRACE_COUNTER_DECREMENT(BulkDataBatchRequest_Count);
	}

	virtual bool Cancel() override final
	{
		if (FBulkDataRequest::EStatus::None == GetStatus())
		{
			CompleteBatch(FBulkDataRequest::EStatus::Cancelled);
			return true;
		}
		else if (FBulkDataRequest::EStatus::Pending == GetStatus())
		{
			for (FChunkBatchReadRequest& Request : Requests)
			{
				Request.Cancel();
			}

			return true;
		}

		return false;
	}
	
	virtual bool Wait(uint32 Milliseconds) override final
	{
		check(GetStatus() != FBulkDataRequest::EStatus::None);
		return DoneEvent.WaitFor(UE::FMonotonicTimeSpan::FromMilliseconds(Milliseconds));
	}

	void Read(
		const FIoChunkId& BulkChunkId,
		const FIoReadOptions& Options,
		EAsyncIOPriorityAndFlags Priority,
		FIoReadCallback&& Callback,
		FBulkDataBatchReadRequest* OutRequest) 
	{
		const int32 IoPriority = ConvertToIoDispatcherPriority(Priority);

		FChunkBatchReadRequest* Request = new (Requests) FChunkBatchReadRequest(this);
		Request->IoHandle = IoBatch.ReadWithCallback(BulkChunkId, Options, IoPriority, MoveTemp(Callback));

		if (OutRequest)
		{
			*OutRequest = FBulkDataBatchReadRequest(Request);
		}
	}

	bool Issue(FBulkDataRequest::FCompletionCallback&& Callback) 
	{
		CompletionCallback = MoveTemp(Callback);

		if (Requests.IsEmpty())
		{
			CompleteBatch(FBulkDataRequest::EStatus::Ok);
			return false;
		}

		TRACE_COUNTER_INCREMENT(BulkDataBatchRequest_PendingCount);

		SetStatus(FBulkDataRequest::EStatus::Pending);

		IoBatch.IssueWithCallback([this]()
		{
			FBulkDataRequest::EStatus BatchStatus = FBulkDataRequest::EStatus::Ok;

			for (FChunkBatchReadRequest& Request : Requests)
			{
				if (EIoErrorCode ErrorCode = Request.IoHandle.Status().GetErrorCode(); ErrorCode != EIoErrorCode::Ok)
				{
					BatchStatus = ErrorCode == EIoErrorCode::Cancelled ? FBulkDataRequest::EStatus::Cancelled : FBulkDataRequest::EStatus::Error;
					break;
				}
			}

			CompleteBatch(BatchStatus);

			TRACE_COUNTER_DECREMENT(BulkDataBatchRequest_PendingCount);
		});

		return true;
	}

private:
	void CompleteBatch(FBulkDataRequest::EStatus CompletionStatus)
	{
		if (CompletionCallback)
		{
			CompletionCallback(CompletionStatus);
		}

		SetStatus(CompletionStatus);
		DoneEvent.Notify();
	}

	static constexpr int32 TargetBytesPerChunk = sizeof(FChunkBatchReadRequest) * 8;
	using FRequests = TChunkedArray<FChunkBatchReadRequest, TargetBytesPerChunk>;

	FIoBatch				IoBatch;
	FRequests				Requests; //TODO: Optimize if there's only a single read request 
	UE::FManualResetEvent	DoneEvent;
	FBulkDataRequest::FCompletionCallback CompletionCallback;
};

//////////////////////////////////////////////////////////////////////////////

FBulkDataRequest::FBulkDataRequest()
{
}

FBulkDataRequest::FBulkDataRequest(FBulkDataRequest::IHandle* InHandle)
	: Handle(InHandle)
{
	check(InHandle != nullptr);
}

FBulkDataRequest::~FBulkDataRequest()
{
}

FBulkDataRequest::FBulkDataRequest(FBulkDataRequest&& Other)
	: Handle(MoveTemp(Other.Handle))
{
}

FBulkDataRequest& FBulkDataRequest::operator=(FBulkDataRequest&& Other)
{
	Handle = MoveTemp(Other.Handle);

	return *this;
}

FBulkDataRequest::EStatus FBulkDataRequest::GetStatus() const
{
	return Handle != nullptr ? Handle->GetStatus() : EStatus::None;
}

bool FBulkDataRequest::IsNone() const
{
	return FBulkDataRequest::EStatus::None == GetStatus();
}

bool FBulkDataRequest::IsPending() const
{
	return FBulkDataRequest::EStatus::Pending == GetStatus();
}

bool FBulkDataRequest::IsOk() const
{
	return FBulkDataRequest::EStatus::Ok == GetStatus();
}

bool FBulkDataRequest::IsCompleted() const
{
	const uint32 Status = static_cast<uint32>(GetStatus());
	return Status > static_cast<uint32>(EStatus::Pending);
}

void FBulkDataRequest::Cancel()
{
	check(Handle);
	Handle->Cancel();
}

void FBulkDataRequest::Reset()
{
	Handle.SafeRelease();
}

int32 FBulkDataRequest::GetRefCount() const
{
	if (Handle)
	{
		return Handle->GetRefCount();
	}

	return -1;
}

//////////////////////////////////////////////////////////////////////////////

void FBulkDataBatchRequest::Wait()
{
	check(Handle);
	Handle->Wait(MAX_uint32);
}

bool FBulkDataBatchRequest::WaitFor(uint32 Milliseconds)
{
	check(Handle);
	return Handle->Wait(Milliseconds);
}

bool FBulkDataBatchRequest::WaitFor(const FTimespan& WaitTime)
{
	return WaitFor((uint32)FMath::Clamp<int64>(WaitTime.GetTicks() / ETimespan::TicksPerMillisecond, 0, MAX_uint32));
}

//////////////////////////////////////////////////////////////////////////////

FBulkDataBatchRequest::FBuilder::FBuilder(int32 MaxCount)
	: BatchMax(MaxCount)
{
}

FBulkDataBatchRequest::FBuilder::~FBuilder()
{
}

FBulkDataBatchRequest::FBatchHandle& FBulkDataBatchRequest::FBuilder::GetBatch()
{
	if (Batch.IsValid() == false)
	{
		Batch = new FBulkDataBatchRequest::FBatchHandle(BatchMax);
	}

	return *Batch;
}

FBulkDataRequest::EStatus FBulkDataBatchRequest::FBuilder::IssueBatch(FBulkDataBatchRequest* OutRequest, FCompletionCallback&& Callback)
{
	check(Batch.IsValid());
	checkf(OutRequest != nullptr || Batch->GetRefCount() > 1, TEXT("At least one request handle needs to be used when creating a batch request"));

	TRefCountPtr<FBulkDataBatchRequest::FBatchHandle> NewBatch = MoveTemp(Batch);
	const bool bOk = NewBatch->Issue(MoveTemp(Callback));

	if (OutRequest)
	{
		if (bOk)
		{
			*OutRequest = FBulkDataBatchRequest(NewBatch.GetReference());
		}
		else
		{
			*OutRequest = FBulkDataBatchRequest(new FHandleBase(EStatus::Error));
		}
	}

	return bOk ? EStatus::Ok : EStatus::Error;
}

FBulkDataBatchRequest::FBatchBuilder::FBatchBuilder(int32 MaxCount)
	: FBuilder(MaxCount)
{
}

FBulkDataBatchRequest::FBatchBuilder& FBulkDataBatchRequest::FBatchBuilder::Read(FBulkData& BulkData, EAsyncIOPriorityAndFlags Priority)
{
	if (BulkData.IsBulkDataLoaded())
	{
		++NumLoaded;
		return *this;
	}

	GetBatch().Read(
		BulkData.BulkChunkId,
		FIoReadOptions(BulkData.GetBulkDataOffsetInFile(), BulkData.GetBulkDataSize()),
		Priority,
		[BulkData = &BulkData](TIoStatusOr<FIoBuffer> Status)
		{
			if (Status.IsOk())
			{
				FIoBuffer Buffer = Status.ConsumeValueOrDie();
				void* Data = BulkData->ReallocateData(Buffer.GetSize());

				FMemoryReaderView Ar(Buffer.GetView(), true);
				BulkData->SerializeBulkData(Ar, Data, Buffer.GetSize(), EBulkDataFlags(BulkData->GetBulkDataFlags()));
			}
		},
		nullptr);

	++BatchCount;

	return *this;
}

FBulkDataBatchRequest::FBatchBuilder& FBulkDataBatchRequest::FBatchBuilder::Read(
	const FBulkData& BulkData,
	uint64 Offset,
	uint64 Size,
	EAsyncIOPriorityAndFlags Priority,
	FIoBuffer& Dst, 
	FBulkDataBatchReadRequest* OutRequest)
{
	check(Size == MAX_uint64 || Size <= uint64(BulkData.GetBulkDataSize()));

	const uint64 ReadOffset = BulkData.GetBulkDataOffsetInFile() + Offset;
	const uint64 ReadSize	= FMath::Min(uint64(BulkData.GetBulkDataSize()), Size);

	check(Dst.GetSize() == 0 || Dst.GetSize() == ReadSize);

	if (Dst.GetSize() == 0)
	{
		Dst = FIoBuffer(ReadSize);
	}

	GetBatch().Read(
		BulkData.BulkChunkId,
		FIoReadOptions(ReadOffset, ReadSize, Dst.GetData()),
		Priority,
		FIoReadCallback(),
		OutRequest);
	
	++BatchCount;
	
	return *this;
}

FBulkDataRequest::EStatus FBulkDataBatchRequest::FBatchBuilder::Issue(FBulkDataBatchRequest& OutRequest)
{
	if (NumLoaded > 0 && BatchCount == 0)
	{
		OutRequest = FBulkDataBatchRequest(new FHandleBase(EStatus::Ok));
		return EStatus::Ok;
	}

	return IssueBatch(&OutRequest, FCompletionCallback());
}

FBulkDataRequest::EStatus FBulkDataBatchRequest::FBatchBuilder::Issue()
{
	check(NumLoaded > 0 || BatchCount > 0);

	if (NumLoaded > 0 && BatchCount == 0)
	{
		return EStatus::Ok;	
	}

	return IssueBatch(nullptr, FCompletionCallback());
}

FBulkDataBatchRequest::FScatterGatherBuilder::FScatterGatherBuilder(int32 MaxCount)
	: FBuilder(MaxCount)
{
	if (MaxCount > 0)
	{
		Requests.Reserve(MaxCount);
	}
}

FBulkDataBatchRequest::FScatterGatherBuilder& FBulkDataBatchRequest::FScatterGatherBuilder::Read(const FBulkData& BulkData, uint64 Offset, uint64 Size)
{
	check(Size == MAX_uint64 || Size <= uint64(BulkData.GetBulkDataSize()));

	const uint64 ReadOffset = BulkData.GetBulkDataOffsetInFile() + Offset;
	const uint64 ReadSize	= FMath::Min(uint64(BulkData.GetBulkDataSize()), Size);

	if (Requests.Num() > 0)
	{
		FRequest& Last = Requests.Last();

		const bool bContiguous =
			Last.Offset + Last.Size == ReadOffset &&
			Last.BulkData->GetBulkDataFlags() == BulkData.GetBulkDataFlags() &&
			Last.BulkData->BulkChunkId == BulkData.BulkChunkId;

		if (bContiguous)
		{
			Last.Size += ReadSize; 
			return *this;
		}
	}

	Requests.Add(FRequest {&BulkData, ReadOffset, ReadSize});

	return *this;
}

FBulkDataRequest::EStatus FBulkDataBatchRequest::FScatterGatherBuilder::Issue(FIoBuffer& Dst, EAsyncIOPriorityAndFlags Priority, FCompletionCallback&& Callback, FBulkDataBatchRequest& OutRequest)
{
	check(Requests.IsEmpty() == false);

	uint64 TotalSize = 0;
	for (const FRequest& Request : Requests)
	{
		TotalSize += Request.Size;
	}

	check(Dst.GetSize() == 0 || Dst.GetSize() == TotalSize);

	if (Dst.GetSize() != TotalSize)
	{
		Dst = FIoBuffer(TotalSize);
	}

	FMutableMemoryView DstView = Dst.GetMutableView();
	for (const FRequest& Request : Requests)
	{
		GetBatch().Read(
			Request.BulkData->BulkChunkId,
			FIoReadOptions(Request.Offset, Request.Size, DstView.GetData()),
			Priority,
			FIoReadCallback(),
			nullptr);
		
		DstView.RightChopInline(Request.Size);
	}

	return IssueBatch(&OutRequest, MoveTemp(Callback));
}

//////////////////////////////////////////////////////////////////////////////

#undef UE_ENABLE_BULKDATA_RANGE_TEST
