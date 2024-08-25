// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IO/IoOffsetLength.h"
#include "IoDispatcher.h"
#include "Async/InheritedContext.h"
#include "Misc/Optional.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"

#define UE_IODISPATCHER_STATS_ENABLED (COUNTERSTRACE_ENABLED || CSV_PROFILER)

struct FIoContainerHeader;

class FIoRequestImpl : private UE::FInheritedContextBase
{
public:
	FIoRequestImpl* NextRequest = nullptr;
	void* BackendData = nullptr;
	FIoChunkId ChunkId;
	FIoReadOptions Options;
	int32 Priority = 0;

	FIoRequestImpl(class FIoRequestAllocator& InAllocator)
		: Allocator(InAllocator)
	{
		CaptureInheritedContext();
	}

	bool IsCancelled() const
	{
		return bCancelled;
	}

	void SetFailed()
	{
		bFailed = true;
	}

	bool HasBuffer() const
	{
		return Buffer.IsSet();
	}

	CORE_API void CreateBuffer(uint64 Size);

	FIoBuffer& GetBuffer()
	{
		return Buffer.GetValue();
	}

	void SetResult(FIoBuffer InBuffer)
	{
		Buffer.Emplace(InBuffer);
	}

	uint64 GetStartTime() const
	{
#if UE_IODISPATCHER_STATS_ENABLED
		return StartTime;
#else
		return 0;
#endif
	}

private:
	friend class FIoDispatcherImpl;
	friend class FIoRequest;
	friend class FIoBatch;
	friend class FIoRequestStats;

	void AddRef()
	{
		RefCount.IncrementExchange();
	}

	void ReleaseRef()
	{
		if (RefCount.DecrementExchange() == 1)
		{
			FreeRequest();
		}
	}

	void FreeRequest();

	FIoRequestAllocator& Allocator;
	struct IIoDispatcherBackend* Backend = nullptr;
	FIoBatchImpl* Batch = nullptr;
#if UE_IODISPATCHER_STATS_ENABLED
	uint64 StartTime = 0;
#endif
	TOptional<FIoBuffer> Buffer;
	FIoReadCallback Callback;
	TAtomic<uint32> RefCount{ 0 };
	TAtomic<EIoErrorCode> ErrorCode{ EIoErrorCode::Unknown };
	bool bCancelled = false;
	bool bFailed = false;
};

DECLARE_DELEGATE(FWakeUpIoDispatcherThreadDelegate);

struct FIoDispatcherBackendContext
{
	FWakeUpIoDispatcherThreadDelegate WakeUpDispatcherThreadDelegate;
	FIoSignatureErrorDelegate SignatureErrorDelegate;
	bool bIsMultiThreaded;
};

struct IIoDispatcherBackend
{
	virtual void Initialize(TSharedRef<const FIoDispatcherBackendContext> Context) = 0;
	virtual void Shutdown() {};
	virtual bool Resolve(FIoRequestImpl* Request) = 0;
	virtual void CancelIoRequest(FIoRequestImpl* Request) = 0;
	virtual void UpdatePriorityForIoRequest(FIoRequestImpl* Request) = 0;
	virtual bool DoesChunkExist(const FIoChunkId& ChunkId) const = 0;
	virtual bool DoesChunkExist(const FIoChunkId& ChunkId, const FIoOffsetAndLength& ChunkRange) const { return DoesChunkExist(ChunkId); }
	virtual TIoStatusOr<uint64> GetSizeForChunk(const FIoChunkId& ChunkId) const = 0;
	virtual TIoStatusOr<uint64> GetSizeForChunk(const FIoChunkId& ChunkId, const FIoOffsetAndLength& ChunkRange, uint64& OutAvailable) const
	{
		TIoStatusOr<uint64> ChunkSize = GetSizeForChunk(ChunkId);
		OutAvailable = ChunkSize.IsOk() ? ChunkSize.ValueOrDie() : 0;
		return ChunkSize;
	}
	virtual FIoRequestImpl* GetCompletedRequests() = 0;
	virtual TIoStatusOr<FIoMappedRegion> OpenMapped(const FIoChunkId& ChunkId, const FIoReadOptions& Options) = 0;
};
