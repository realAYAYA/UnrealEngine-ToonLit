// Copyright Epic Games, Inc. All Rights Reserved.
#include "Serialization/BulkData.h"
#include "Algo/AllOf.h"
#include "Async/MappedFileHandle.h"
#include "Containers/ChunkedArray.h"
#include "Experimental/Async/LazyEvent.h"
#include "HAL/CriticalSection.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Timespan.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "Serialization/MemoryReader.h"
#include "Templates/RefCounting.h"
#include "UObject/PackageResourceManager.h"

//////////////////////////////////////////////////////////////////////////////

TRACE_DECLARE_INT_COUNTER(BulkDataBatchRequest_Count, TEXT("BulkData/BatchRequest/Count"));
TRACE_DECLARE_INT_COUNTER(BulkDataBatchRequest_PendingCount, TEXT("BulkData/BatchRequest/Pending"));

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

namespace UE::BulkData::Private
{

//////////////////////////////////////////////////////////////////////////////

/**
 * Triggers an ensure when trying to stream inline bulk data. This is to prevent
 * inconsistencies between the Zen loader loading from I/O store (utoc/ucas) and the legacy EDL loader using
 * the file system (.pak). The Zen loader currenlty does not allow reloading of inline bulk data.
 * The trigger can be ignored by setting [Core.System]IgnoreInlineBulkDataReloadEnsures to true in the config file.
 * Ignoring this ensure will most likely break loading of bulk data when packaging with Zen/IO store (.utoc/.ucas).
 */
void EnsureCanStreamBulkData(const FBulkMetaData& BulkMeta)
{
#if !WITH_EDITOR && !UE_KEEP_INLINE_RELOADING_CONSISTENT	
	static struct FIgnoreInlineDataReloadEnsures
	{
		bool bEnabled = false;

		FIgnoreInlineDataReloadEnsures()
		{
			FConfigFile PlatformEngineIni;
			FConfigCacheIni::LoadLocalIniFile(PlatformEngineIni, TEXT("Engine"), true, ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName()));

			PlatformEngineIni.GetBool(TEXT("Core.System"), TEXT("IgnoreInlineBulkDataReloadEnsures"), bEnabled);

			UE_LOG(LogSerialization, Display, TEXT("IgnoreInlineDataReloadEnsures: '%s'"), bEnabled ? TEXT("true") : TEXT("false"));
		}
	} IgnoreInlineDataReloadEnsures;

	const bool bIsInlined = BulkMeta.HasAnyFlags(BULKDATA_PayloadAtEndOfFile) == false;
	// Note that we only want the ensure to trigger if we have a valid offset (the bulkdata references data from disk)
	ensureMsgf(!bIsInlined || BulkMeta.GetOffset() == INDEX_NONE || IgnoreInlineDataReloadEnsures.bEnabled,
				TEXT("Attempting to stream inline BulkData! This operation is not supported by the IoDispatcher and so will eventually stop working."
				" The calling code should be fixed to retain the inline data in memory and re-use it rather than discard it and then try to reload from disk!"));
#endif 
}

FIoChunkId CreateBulkDataIoChunkId(const FBulkMetaData& BulkMeta, const FPackageId& PackageId)
{
	if (PackageId.IsValid() == false)
	{
		return FIoChunkId();
	}

	const EBulkDataFlags BulkDataFlags = BulkMeta.GetFlags();

	const EIoChunkType ChunkType = BulkDataFlags & BULKDATA_OptionalPayload
		? EIoChunkType::OptionalBulkData
		: BulkDataFlags & BULKDATA_MemoryMappedPayload
			? EIoChunkType::MemoryMappedBulkData
			: EIoChunkType::BulkData;

	const uint16 ChunkIndex = EnumHasAnyFlags(BulkMeta.GetMetaFlags(), FBulkMetaData::EMetaFlags::OptionalPackage) ? 1 : 0;
	return CreateIoChunkId(PackageId.Value(), ChunkIndex, ChunkType);
}

//////////////////////////////////////////////////////////////////////////////

EPackageSegment GetPackageSegmentFromFlags(const FBulkMetaData& BulkMeta)
{
	const EBulkDataFlags BulkDataFlags = BulkMeta.GetFlags();

	if ((BulkDataFlags & BULKDATA_PayloadInSeperateFile) == 0)
	{
		const bool bLoadingFromCookedPackage = EnumHasAnyFlags(BulkMeta.GetMetaFlags(), FBulkMetaData::EMetaFlags::CookedPackage);
		if (bLoadingFromCookedPackage)
		{
			// Cooked packages are split into EPackageSegment::Header (summary and linker tables) and
			// EPackageSegment::Exports (serialized UObject bytes and the bulk data section)
			// Inline and end-of-file bulk data is in the Exports section
			return EPackageSegment::Exports;
		}
		else
		{
			return EPackageSegment::Header;
		}
	}
	else if (BulkDataFlags & BULKDATA_OptionalPayload )
	{
		return EPackageSegment::BulkDataOptional;
	}
	else if (BulkDataFlags & BULKDATA_MemoryMappedPayload)
	{
		return EPackageSegment::BulkDataMemoryMapped;
	}
	else
	{
		return EPackageSegment::BulkDataDefault;
	}
}

//////////////////////////////////////////////////////////////////////////////

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

	UE::FLazyEvent		DoneEvent;
	FIoRequest			Request;
	int64				SizeResult;
	std::atomic<uint32>	Status;
};

FChunkRequest::FChunkRequest(FIoBuffer&& InBuffer)
	: Buffer(MoveTemp(InBuffer))
	, DoneEvent(EEventMode::ManualReset)
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

		DoneEvent.Trigger();
	});

	IoBatch.Issue();
}

bool FChunkRequest::WaitForChunkRequest(float TimeLimitSeconds)
{
	checkf(GetStatus() != EChunkRequestStatus::None, TEXT("The request must be issued before waiting for completion"));

	const uint32 TimeLimitMilliseconds = TimeLimitSeconds <= 0.0f ? MAX_uint32 : (uint32)(TimeLimitSeconds * 1000.0f);
	return DoneEvent.Wait(TimeLimitMilliseconds);
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
	FChunkFileSizeRequest(const FIoChunkId& ChunkId, FAsyncFileCallBack* Callback)
		: IAsyncReadRequest(Callback, true, nullptr)
	{
		TIoStatusOr<uint64> Result = FIoDispatcher::Get().GetSizeForChunk(ChunkId);
		if (Result.IsOk())
		{
			Size = Result.ValueOrDie();
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
};

//////////////////////////////////////////////////////////////////////////////

class FChunkReadFileHandle : public IAsyncReadFileHandle
{
public:
	FChunkReadFileHandle(const FIoChunkId& InChunkId) 
		: ChunkId(InChunkId)
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
};

IAsyncReadRequest* FChunkReadFileHandle::SizeRequest(FAsyncFileCallBack* CompleteCallback)
{
	return new FChunkFileSizeRequest(ChunkId, CompleteCallback);
}

IAsyncReadRequest* FChunkReadFileHandle::ReadRequest(
	int64 Offset, 
	int64 BytesToRead, 
	EAsyncIOPriorityAndFlags PriorityAndFlags,
	FAsyncFileCallBack* CompleteCallback,
	uint8* UserSuppliedMemory)
{
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
	const FBulkDataChunkId& BulkChunkId,
	int64 Offset,
	int64 Size,
	EAsyncIOPriorityAndFlags Priority,
	TFunction<void(FArchive& Ar)>&& Read)
{
	if (BulkChunkId.IsValid() == false)
	{
		return false;
	}

	EnsureCanStreamBulkData(BulkMeta);

	if (FPackageId PackageId = BulkChunkId.GetPackageId(); PackageId.IsValid())
	{
		const FIoChunkId ChunkId = CreateBulkDataIoChunkId(BulkMeta, PackageId);
		FIoBatch Batch = FIoDispatcher::Get().NewBatch();

		FIoRequest Request = Batch.Read(ChunkId, FIoReadOptions(Offset, Size), ConvertToIoDispatcherPriority(Priority));
		FEventRef Event;
		Batch.IssueAndTriggerEvent(Event.Get());
		Event->Wait();

		if (const FIoBuffer* Buffer = Request.GetResult())
		{
			FMemoryReaderView Ar(Buffer->GetView());
			Read(Ar);

			return true;
		}
	}
	else
	{
		IPackageResourceManager& ResourceMgr = IPackageResourceManager::Get();
		
		const bool bExternalResource = BulkMeta.HasAllFlags(static_cast<EBulkDataFlags>(BULKDATA_PayloadInSeperateFile | BULKDATA_WorkspaceDomainPayload));
		const FPackagePath& Path = BulkChunkId.GetPackagePath();

		TUniquePtr<FArchive> Ar;
		if (bExternalResource)
		{
			Ar = ResourceMgr.OpenReadExternalResource(EPackageExternalResource::WorkspaceDomainFile, Path.GetPackageName());
		}
		else
		{
			const EPackageSegment Segment = GetPackageSegmentFromFlags(BulkMeta);
			Ar = ResourceMgr.OpenReadPackage(Path, Segment).Archive;
		}

		if (Ar)
		{
			Ar->Seek(Offset);
			Read(*Ar);

			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////

TUniquePtr<IAsyncReadFileHandle> OpenAsyncReadBulkData(const FBulkMetaData& BulkMeta, const FBulkDataChunkId& BulkChunkId)
{
	if (BulkChunkId.IsValid() == false)
	{
		return TUniquePtr<IAsyncReadFileHandle>();
	}

	EnsureCanStreamBulkData(BulkMeta);

	if (FPackageId PackageId = BulkChunkId.GetPackageId(); PackageId.IsValid())
	{
		return MakeUnique<FChunkReadFileHandle>(CreateBulkDataIoChunkId(BulkMeta, PackageId));
	}
	else
	{
		IPackageResourceManager& ResourceMgr = IPackageResourceManager::Get();
		
		const bool bExternalResource = BulkMeta.HasAllFlags(static_cast<EBulkDataFlags>(BULKDATA_PayloadInSeperateFile | BULKDATA_WorkspaceDomainPayload));
		const FPackagePath& Path = BulkChunkId.GetPackagePath();

		if (bExternalResource)
		{
			return ResourceMgr.OpenAsyncReadExternalResource(EPackageExternalResource::WorkspaceDomainFile, Path.GetPackageName()).Handle;
		}
		else
		{
			const EPackageSegment Segment = GetPackageSegmentFromFlags(BulkMeta);
			return ResourceMgr.OpenAsyncReadPackage(Path, Segment).Handle;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////

TUniquePtr<IBulkDataIORequest> CreateStreamingRequest(
	const FBulkMetaData& BulkMeta,
	const FBulkDataChunkId& BulkChunkId,
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

	EnsureCanStreamBulkData(BulkMeta);

	if (FPackageId PackageId = BulkChunkId.GetPackageId(); PackageId.IsValid())
	{
		FIoBuffer Buffer = UserSuppliedMemory ? FIoBuffer(FIoBuffer::Wrap, UserSuppliedMemory, Size) : FIoBuffer(Size);
		const FIoChunkId ChunkId = CreateBulkDataIoChunkId(BulkMeta, PackageId);

		FChunkBulkDataRequest* Request = new FChunkBulkDataRequest(Callback, MoveTemp(Buffer));
		Request->Issue(ChunkId, FIoReadOptions(Offset, Size), ConvertToIoDispatcherPriority(Priority));
		
		return TUniquePtr<IBulkDataIORequest>(Request);
	}
	else if (TUniquePtr<IAsyncReadFileHandle> FileHandle = OpenAsyncReadBulkData(BulkMeta, BulkChunkId); FileHandle.IsValid())
	{
		TUniquePtr<FBulkDataIORequest> Request = MakeUnique<FBulkDataIORequest>(FileHandle.Release());

		if (Request->MakeReadRequest(Offset, Size, Priority, Callback, UserSuppliedMemory))
		{
			return Request;
		}
	}

	return TUniquePtr<IBulkDataIORequest>();
}

//////////////////////////////////////////////////////////////////////////////

bool DoesBulkDataExist(const FBulkMetaData& BulkMeta, const FBulkDataChunkId& BulkChunkId)
{
	if (BulkChunkId.IsValid() == false)
	{
		return false;
	}

	if (FPackageId Id = BulkChunkId.GetPackageId(); Id.IsValid())
	{
		const FIoChunkId ChunkId = CreateBulkDataIoChunkId(BulkMeta, Id);
		return FIoDispatcher::Get().DoesChunkExist(ChunkId);
	}
	else
	{
		IPackageResourceManager& ResourceMgr = IPackageResourceManager::Get();
		const FPackagePath& PackagePath = BulkChunkId.GetPackagePath();
		const bool bExternalResource = BulkMeta.HasAllFlags(static_cast<EBulkDataFlags>(BULKDATA_PayloadInSeperateFile | BULKDATA_WorkspaceDomainPayload));

		if (bExternalResource)
		{
			return ResourceMgr.DoesExternalResourceExist(
				EPackageExternalResource::WorkspaceDomainFile, PackagePath.GetPackageName());
		}

		const EPackageSegment PackageSegment = GetPackageSegmentFromFlags(BulkMeta);
		return ResourceMgr.DoesPackageExist(PackagePath, PackageSegment);
	}
}

//////////////////////////////////////////////////////////////////////////////

bool TryMemoryMapBulkData(
	const FBulkMetaData& BulkMeta,
	const FBulkDataChunkId& BulkChunkId,
	int64 Offset,
	int64 Size,
	FIoMappedRegion& OutRegion)
{
	if (FPackageId Id = BulkChunkId.GetPackageId(); Id.IsValid())
	{
		const FIoChunkId ChunkId = CreateBulkDataIoChunkId(BulkMeta, Id);
		TIoStatusOr<FIoMappedRegion> Status = FIoDispatcher::Get().OpenMapped(ChunkId, FIoReadOptions(Offset, Size));

		if (Status.IsOk())
		{
			OutRegion = Status.ConsumeValueOrDie();

			return true;
		}
	}
	else
	{
		IPackageResourceManager& ResourceMgr = IPackageResourceManager::Get();
		const FPackagePath& Path = BulkChunkId.GetPackagePath();
		const EPackageSegment Segment = EPackageSegment::BulkDataMemoryMapped;

		TUniquePtr<IMappedFileHandle> MappedFile;
		MappedFile.Reset(IPackageResourceManager::Get().OpenMappedHandleToPackage(Path, Segment));

		if (!MappedFile)
		{
			return false;
		}

		if (IMappedFileRegion* MappedRegion = MappedFile->MapRegion(Offset, Size, true))
		{
			OutRegion.MappedFileHandle = MappedFile.Release();
			OutRegion.MappedFileRegion = MappedRegion;

			return true;
		}
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
	const FBulkDataChunkId& BulkChunkId,
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

class FBulkDataBatchRequest::IHandle : public FBulkDataRequest::IHandle
{
public:
	virtual ~IHandle() = default;

	virtual void Read(
		const UE::BulkData::Private::FBulkMetaData& BulkMeta,
		const UE::BulkData::Private::FBulkDataChunkId& BulkChunkId,
		const FIoReadOptions& Options,
		EAsyncIOPriorityAndFlags Priority,
		FIoReadCallback&& Callback,
		FBulkDataBatchReadRequest* OutRequest) = 0; 

	virtual bool Issue(FBulkDataRequest::FCompletionCallback&& Callback) = 0;
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

class FBatchHandleBase : public FBulkDataBatchRequest::IHandle
{
public:
	FBatchHandleBase() = default;
	virtual ~FBatchHandleBase() = default;

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
	
	virtual bool Wait(uint32 Milliseconds) override final
	{
		return DoneEvent.Wait(Milliseconds);
	}

	void SetStatus(FBulkDataBatchRequest::EStatus InStatus)
	{
		Status.store(uint32(InStatus), std::memory_order_release);
	}

protected:
	void CompleteBatch(FBulkDataRequest::EStatus CompletionStatus)
	{
		if (CompletionCallback)
		{
			CompletionCallback(CompletionStatus);
		}

		SetStatus(CompletionStatus);
		DoneEvent.Trigger();
	}

private:
	std::atomic<int32>	RefCount{0};
	std::atomic<uint32> Status{0};
	UE::FLazyEvent		DoneEvent{EEventMode::ManualReset};

protected:
	FBulkDataRequest::FCompletionCallback CompletionCallback;
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
	FChunkBatchReadRequest(FBulkDataBatchRequest::IHandle* InBatch, FIoRequest&& InIoHandle)
		: Batch(InBatch)
		, IoHandle(MoveTemp(InIoHandle))
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

	FBulkDataBatchRequest::IHandle* Batch = nullptr;
	FIoRequest IoHandle;
};

class FChunkBatchRequest final : public FBatchHandleBase
{
public:
	FChunkBatchRequest(int32 BatchMaxCount)
	{
		if (BatchMaxCount > 0)
		{
			Requests.Reserve(BatchMaxCount);
		}

		TRACE_COUNTER_INCREMENT(BulkDataBatchRequest_Count);
	}
	
	~FChunkBatchRequest()
	{
		Cancel();
		Wait(MAX_uint32);
		
		TRACE_COUNTER_DECREMENT(BulkDataBatchRequest_Count);
	}

	virtual bool Cancel() override
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

	virtual void Read(
		const UE::BulkData::Private::FBulkMetaData& BulkMeta,
		const UE::BulkData::Private::FBulkDataChunkId& BulkChunkId,
		const FIoReadOptions& Options,
		EAsyncIOPriorityAndFlags Priority,
		FIoReadCallback&& Callback,
		FBulkDataBatchReadRequest* OutRequest) override
	{
		const FIoChunkId ChunkId	= CreateBulkDataIoChunkId(BulkMeta, BulkChunkId.GetPackageId());
		const int32 IoPriority		= ConvertToIoDispatcherPriority(Priority);

		FIoRequest IoRequest = IoBatch.ReadWithCallback(ChunkId, Options, IoPriority, MoveTemp(Callback));
		FChunkBatchReadRequest* Request = new (Requests) FChunkBatchReadRequest(this, MoveTemp(IoRequest));

		if (OutRequest)
		{
			*OutRequest = FBulkDataBatchReadRequest(Request);
		}
	}

	virtual bool Issue(FBulkDataRequest::FCompletionCallback&& Callback) override
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

			for (auto& Request : Requests)
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

	static constexpr int32 TargetBytesPerChunk = sizeof(FChunkBatchReadRequest) * 8;
	using FRequests = TChunkedArray<FChunkBatchReadRequest, TargetBytesPerChunk>;

	FIoBatch	IoBatch;
	FRequests	Requests; //TOOD: Optimize if there's only a single read request 
};

//////////////////////////////////////////////////////////////////////////////

class FFileSystemBatchReadRequest final : public FBulkDataRequest::IHandle 
{
public:
	FFileSystemBatchReadRequest() = default;
	FFileSystemBatchReadRequest(FBulkDataBatchRequest::IHandle* InBatch, FIoReadCallback&& InCallback)
		: BatchHandle(InBatch)
		, Callback(MoveTemp(InCallback))
	{ }

	virtual void AddRef() override
	{
		BatchHandle->AddRef();
	}

	virtual void Release() override
	{
		BatchHandle->Release();
	}

	virtual uint32 GetRefCount() const override
	{
		return BatchHandle->GetRefCount();
	}

	virtual FBulkDataRequest::EStatus GetStatus() const override
	{
		check(RequestHandle);
		
		if (bWasCancelled)
		{
			return FBulkDataRequest::EStatus::Cancelled;
		}

		return RequestHandle->PollCompletion() ? FBulkDataRequest::EStatus::Ok : FBulkDataRequest::EStatus::Pending;
	}

	virtual bool Cancel() override
	{
		check(RequestHandle);

		if (GetStatus() == FBulkDataRequest::EStatus::Pending)
		{
			RequestHandle->Cancel();
			return true;
		}

		return false;
	}

	virtual bool Wait(uint32 Milliseconds) override
	{
		checkNoEntry(); // Bulk read requests is currently not awaitable
		return true;
	}

	FBulkDataBatchRequest::IHandle* BatchHandle = nullptr;
	TUniquePtr<IAsyncReadRequest>	RequestHandle;
	FIoReadCallback					Callback;
	std::atomic<bool>				bWasCancelled{false};
};

class FFileSystemBatchRequest final : public FBatchHandleBase
{
	struct FRequestParams
	{
		FFileSystemBatchReadRequest*			Request;
		FPackagePath							PackagePath;
		FIoReadOptions							ReadOptions;
		UE::BulkData::Private::FBulkMetaData	BulkMeta;
		EAsyncIOPriorityAndFlags				Priority;
	};

public:
	FFileSystemBatchRequest(int32 BatchMaxCount)
	{
		if (BatchMaxCount > 0)
		{
			Requests.Reserve(BatchMaxCount);
			RequestParams.Reserve(BatchMaxCount);
		}
	}

	~FFileSystemBatchRequest()
	{
		Cancel();
		Wait(MAX_uint32);
		Requests.Empty();
	}

	virtual void Read(
		const UE::BulkData::Private::FBulkMetaData& BulkMeta,
		const UE::BulkData::Private::FBulkDataChunkId& BulkChunkId,
		const FIoReadOptions& Options,
		EAsyncIOPriorityAndFlags Priority,
		FIoReadCallback&& Callback,
		FBulkDataBatchReadRequest* OutRequest) override
	{
		FFileSystemBatchReadRequest* Request = new (Requests) FFileSystemBatchReadRequest(this, MoveTemp(Callback));

		RequestParams.Add(FRequestParams {Request, BulkChunkId.GetPackagePath(), Options, BulkMeta, Priority});

		if (OutRequest)
		{
			*OutRequest = FBulkDataBatchReadRequest(Request);
		}
	}

	virtual bool Issue(FBulkDataRequest::FCompletionCallback&& Callback) override
	{
		CompletionCallback = MoveTemp(Callback);

		if (Requests.IsEmpty())
		{
			CompleteBatch(FBulkDataRequest::EStatus::Ok);
			return false;
		}

		SetStatus(FBulkDataRequest::EStatus::Pending);

		PendingRequestCount.store(Requests.Num(), std::memory_order_relaxed);

		for (FRequestParams& Params : RequestParams)
		{
			IAsyncReadFileHandle* FileHandle = OpenFile(Params.PackagePath, Params.BulkMeta);

			if (FileHandle == nullptr)
			{
				CompleteBatch(FBulkDataRequest::EStatus::Error);
				return false;
			}

			const bool bMemoryIsOwned = Params.ReadOptions.GetTargetVa() != nullptr;

			FAsyncFileCallBack ReadFileCallback = [
				this,
				Request = Params.Request,
				bMemoryIsOwned,
				Size = Params.ReadOptions.GetSize()](bool bWasCancelled, IAsyncReadRequest* ReadRequest)
				{
					if (Request->Callback)
					{
						if (bWasCancelled)
						{
							Request->Callback(FIoStatus(EIoErrorCode::Cancelled));
						}
						else
						{
							FIoBuffer Buffer = bMemoryIsOwned
								? FIoBuffer(FIoBuffer::Wrap, ReadRequest->GetReadResults(), Size)
								: FIoBuffer(FIoBuffer::AssumeOwnership, ReadRequest->GetReadResults(), Size);
							
							Request->Callback(Buffer);
						}
					}

					if (bWasCancelled)
					{
						Request->bWasCancelled = true;
						BatchStatus.store(uint32(FBulkDataRequest::EStatus::Cancelled), std::memory_order_release);
					}

					if (1 == PendingRequestCount.fetch_sub(1))
					{
						CompleteBatch(FBulkDataRequest::EStatus(BatchStatus.load(std::memory_order_consume)));
					}
				};

			Params.Request->RequestHandle.Reset(FileHandle->ReadRequest(
				Params.ReadOptions.GetOffset(),
				Params.ReadOptions.GetSize(),
				Params.Priority,
				&ReadFileCallback,
				reinterpret_cast<uint8*>(Params.ReadOptions.GetTargetVa())));
		}
		
		RequestParams.Empty();

		return true;
	}

	virtual bool Cancel() override
	{
		if (GetStatus() == FBulkDataRequest::EStatus::Pending)
		{
			for (FFileSystemBatchReadRequest& Request : Requests)
			{
				Request.Cancel();
			}

			return true;
		}

		return false;
	}

private:
	IAsyncReadFileHandle* OpenFile(const FPackagePath& Path, const UE::BulkData::Private::FBulkMetaData& BulkMeta)
	{
		TUniquePtr<IAsyncReadFileHandle>& FileHandle = PathToFileHandle.FindOrAdd(Path.GetPackageFName());

		if (FileHandle.IsValid() == false)
		{
			IPackageResourceManager& ResourceMgr = IPackageResourceManager::Get();

			const bool bExternalResource = BulkMeta.HasAllFlags(static_cast<EBulkDataFlags>(BULKDATA_PayloadInSeperateFile | BULKDATA_WorkspaceDomainPayload));

			if (bExternalResource)
			{
				FileHandle = ResourceMgr.OpenAsyncReadExternalResource(EPackageExternalResource::WorkspaceDomainFile, Path.GetPackageName()).Handle;
			}
			else
			{
				const EPackageSegment Segment = GetPackageSegmentFromFlags(BulkMeta);
				FileHandle = ResourceMgr.OpenAsyncReadPackage(Path, Segment).Handle;
			}
		}

		return FileHandle.Get();
	}

	using FPathToFileHandle = TMap<FName, TUniquePtr<IAsyncReadFileHandle>>;
	using FRequests			= TChunkedArray<FFileSystemBatchReadRequest>;

	FPathToFileHandle			PathToFileHandle;
	FRequests					Requests;
	TArray<FRequestParams>		RequestParams;
	std::atomic<uint32>			BatchStatus;
	std::atomic<int32>			PendingRequestCount;
};

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

FBulkDataBatchRequest::FBuilder::FBuilder()
{
}

FBulkDataBatchRequest::FBuilder::FBuilder(int32 MaxCount)
	: BatchMax(MaxCount)
{
}

FBulkDataBatchRequest::FBuilder::~FBuilder()
{
}

FBulkDataBatchRequest::IHandle& FBulkDataBatchRequest::FBuilder::GetBatch(const FBulkData& BulkData)
{
	if (Batch.IsValid() == false)
	{
		check(BulkData.BulkChunkId.IsValid());

		if (FPackageId PackageId = BulkData.BulkChunkId.GetPackageId(); PackageId.IsValid())
		{
			Batch = new FChunkBatchRequest(BatchMax);
		}
		else
		{
			Batch = new FFileSystemBatchRequest(BatchMax);
		}
	}

	return *Batch;
}

FBulkDataRequest::EStatus FBulkDataBatchRequest::FBuilder::IssueBatch(FBulkDataBatchRequest* OutRequest, FCompletionCallback&& Callback)
{
	check(Batch.IsValid());
	checkf(OutRequest != nullptr || Batch->GetRefCount() > 1, TEXT("At least one request handle needs to be used when creating a batch request"));

	TRefCountPtr<FBulkDataBatchRequest::IHandle> NewBatch = MoveTemp(Batch);
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
	check(BatchMax == -1 || BatchCount < BatchMax);

	if (BulkData.IsBulkDataLoaded())
	{
		++NumLoaded;
		return *this;
	}

	GetBatch(BulkData).Read(
		BulkData.BulkMeta,
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

	check(BatchMax == -1 || BatchCount < BatchMax);
	check(Dst.GetSize() == 0 || Dst.GetSize() == ReadSize);

	if (Dst.GetSize() == 0)
	{
		Dst = FIoBuffer(ReadSize);
	}

	GetBatch(BulkData).Read(
		BulkData.BulkMeta,
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

	FBulkDataBatchRequest::IHandle& BatchRequest = GetBatch(*Requests[0].BulkData);

	FMutableMemoryView DstView = Dst.GetMutableView();
	for (const FRequest& Request : Requests)
	{
		BatchRequest.Read(
			Request.BulkData->BulkMeta,
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
