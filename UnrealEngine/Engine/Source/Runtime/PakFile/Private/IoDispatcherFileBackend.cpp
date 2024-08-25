// Copyright Epic Games, Inc. All Rights Reserved.

#include "IoDispatcherFileBackend.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/StringBuilder.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "GenericPlatformIoDispatcher.h"
#include "HAL/IConsoleManager.h"
#include "Async/AsyncWork.h"
#include "Async/MappedFileHandle.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"
#include "Misc/Paths.h"
#include "Algo/AllOf.h"
#include "Algo/IsSorted.h"
#include "Algo/MinElement.h"
#include "Templates/Greater.h"
#include "Containers/Ticker.h"
#include "Modules/ModuleManager.h"
#include "IO/IoContainerHeader.h"
#include "Serialization/MemoryReader.h"
#include "FileCache/FileCache.h"

//PRAGMA_DISABLE_OPTIMIZATION

int32 GIoDispatcherBufferSizeKB = 256;
static FAutoConsoleVariableRef CVar_IoDispatcherBufferSizeKB(
	TEXT("s.IoDispatcherBufferSizeKB"),
	GIoDispatcherBufferSizeKB,
	TEXT("IoDispatcher read buffer size (in kilobytes).")
);

int32 GIoDispatcherBufferAlignment = 4096;
static FAutoConsoleVariableRef CVar_IoDispatcherBufferAlignment(
	TEXT("s.IoDispatcherBufferAlignment"),
	GIoDispatcherBufferAlignment,
	TEXT("IoDispatcher read buffer alignment.")
);

int32 GIoDispatcherBufferMemoryMB = 8;
static FAutoConsoleVariableRef CVar_IoDispatcherBufferMemoryMB(
	TEXT("s.IoDispatcherBufferMemoryMB"),
	GIoDispatcherBufferMemoryMB,
	TEXT("IoDispatcher buffer memory size (in megabytes).")
);

int32 GIoDispatcherDecompressionWorkerCount = 4;
static FAutoConsoleVariableRef CVar_IoDispatcherDecompressionWorkerCount(
	TEXT("s.IoDispatcherDecompressionWorkerCount"),
	GIoDispatcherDecompressionWorkerCount,
	TEXT("IoDispatcher decompression worker count.")
);

int32 GIoDispatcherCacheSizeMB = 0;
static FAutoConsoleVariableRef CVar_IoDispatcherCacheSizeMB(
	TEXT("s.IoDispatcherCacheSizeMB"),
	GIoDispatcherCacheSizeMB,
	TEXT("IoDispatcher cache memory size (in megabytes).")
);

int32 GIoDispatcherSortRequestsByOffset = 1;
static FAutoConsoleVariableRef CVar_IoDispatcherSortRequestsByOffset(
	TEXT("s.IoDispatcherSortRequestsByOffset"),
	GIoDispatcherSortRequestsByOffset,
	TEXT("If > 0, io dispatcher sorts the outstanding request queue by offset rather than sequence.")
);

int32 GIoDispatcherMaintainSortingOnPriorityChange = 1;
static FAutoConsoleVariableRef CVar_IoDispatcherMaintainSortingOnPriorityChange(
	TEXT("s.IoDispatcherMaintainSortingOnPriorityChange"),
	GIoDispatcherMaintainSortingOnPriorityChange,
	TEXT("If s.IoDispatcherSortRequestsByOffset > 0 and this > 0, io dispatcher remembers the last file handle/offset read from even when switching priority levels.")
);

int32 GIoDispatcherMaxForwardSeekKB = 0;
static FAutoConsoleVariableRef CVar_IoDispatcherMaxForwardSeekKB(
	TEXT("s.IoDispatcherMaxForwardSeekKB"),
	GIoDispatcherMaxForwardSeekKB,
	TEXT("If s.IoDispatcherSortRequestsByOffset is enabled and this is > 0, if the next sequential read is further than this offset from the last one, read the oldest request instead")
);

int32 GIoDispatcherRequestLatencyCircuitBreakerMS = 0;
static FAutoConsoleVariableRef CVar_IoDispatcherRequestLatencyCircuitBreakerMS(
	TEXT("s.IoDispatcherRequestLatencyCircuitBreakerMS"),
	GIoDispatcherRequestLatencyCircuitBreakerMS,
	TEXT("If s.IoDispatcherSortRequestsByOffset is enabled and this is >0, if the oldest request has been in the queue for this long, read it instead of the most optimal read")
);

int32 GIoDispatcherTocsEnablePerfectHashing = 1;
static FAutoConsoleVariableRef CVar_IoDispatcherTocsEnablePerfectHashing(
	TEXT("s.IoDispatcherTocsEnablePerfectHashing"),
	GIoDispatcherTocsEnablePerfectHashing,
	TEXT("Enable perfect hashmap lookups for iostore tocs")
);

int32 GIoDispatcherForceSynchronousScatter = 0;
static FAutoConsoleVariableRef CVar_IoDispatcherForceSynchronousScatter(
	TEXT("s.IoDispatcherForceSynchronousScatter"),
	GIoDispatcherForceSynchronousScatter,
	TEXT("Force scatter jobs to be synchronous on the IODispatcher thread.\n")
	TEXT("This can avoid deadlocks in cases where background tasks end up waiting on I/O and we don't have enough background task threads to fulfill decompression requests.")
);


uint32 FFileIoStoreReadRequest::NextSequence = 0;
#if CHECK_IO_STORE_READ_REQUEST_LIST_MEMBERSHIP
uint32 FFileIoStoreReadRequestList::NextListCookie = 0;
#endif
TAtomic<uint32> FFileIoStoreReader::GlobalPartitionIndex{ 0 };
TAtomic<uint32> FFileIoStoreReader::GlobalContainerInstanceId{ 0 };

class FMappedFileProxy final : public IMappedFileHandle
{
public:
	FMappedFileProxy(IMappedFileHandle* InSharedMappedFileHandle, uint64 InSize)
		: IMappedFileHandle(InSize)
		, SharedMappedFileHandle(InSharedMappedFileHandle)
	{
	}

	virtual ~FMappedFileProxy() { }

	virtual IMappedFileRegion* MapRegion(int64 Offset = 0, int64 BytesToMap = MAX_int64, bool bPreloadHint = false) override
	{
		return SharedMappedFileHandle != nullptr ? SharedMappedFileHandle->MapRegion(Offset, BytesToMap, bPreloadHint) : nullptr;
	}
private:
	IMappedFileHandle* SharedMappedFileHandle;
};

void FFileIoStoreBufferAllocator::Initialize(uint64 InMemorySize, uint64 InBufferSize, uint32 InBufferAlignment)
{
	uint64 BufferCount = InMemorySize / InBufferSize;
	uint64 MemorySize = BufferCount * InBufferSize;
	BufferMemory = reinterpret_cast<uint8*>(FMemory::Malloc(MemorySize, InBufferAlignment));
	BufferSize = InBufferSize;
	for (uint64 BufferIndex = 0; BufferIndex < BufferCount; ++BufferIndex)
	{
		FFileIoStoreBuffer* Buffer = new FFileIoStoreBuffer();
		Buffer->Memory = BufferMemory + BufferIndex * BufferSize;
		Buffer->Next = FirstFreeBuffer;
		FirstFreeBuffer = Buffer;
		Stats.OnBufferReleased();
	}
}

FFileIoStoreBuffer* FFileIoStoreBufferAllocator::AllocBuffer()
{
	FFileIoStoreBuffer* Buffer;
	{
		FScopeLock Lock(&BuffersCritical);
		Buffer = FirstFreeBuffer;
		if (!Buffer)
		{
			return nullptr;
		}
		FirstFreeBuffer = Buffer->Next;
	}
	Stats.OnBufferAllocated();
	return Buffer;
}

void FFileIoStoreBufferAllocator::FreeBuffer(FFileIoStoreBuffer* Buffer)
{
	check(Buffer);
	{
		FScopeLock Lock(&BuffersCritical);
		Buffer->Next = FirstFreeBuffer;
		FirstFreeBuffer = Buffer;
	}
	Stats.OnBufferReleased();
}

FFileIoStoreBlockCache::FFileIoStoreBlockCache(FFileIoStoreStats& InStats)
	: Stats(InStats)
{
	CacheLruHead.LruNext = &CacheLruTail;
	CacheLruTail.LruPrev = &CacheLruHead;
}

FFileIoStoreBlockCache::~FFileIoStoreBlockCache()
{
	FCachedBlock* CachedBlock = CacheLruHead.LruNext;
	while (CachedBlock != &CacheLruTail)
	{
		FCachedBlock* Next = CachedBlock->LruNext;
		delete CachedBlock;
		CachedBlock = Next;
	}
	FMemory::Free(CacheMemory);
}

void FFileIoStoreBlockCache::Initialize(uint64 InCacheMemorySize, uint64 InReadBufferSize)
{
	ReadBufferSize = InReadBufferSize;
	uint64 CacheBlockCount = InCacheMemorySize / InReadBufferSize;
	if (CacheBlockCount)
	{
		InCacheMemorySize = CacheBlockCount * InReadBufferSize;
		CacheMemory = reinterpret_cast<uint8*>(FMemory::Malloc(InCacheMemorySize));
		FCachedBlock* Prev = &CacheLruHead;
		for (uint64 CacheBlockIndex = 0; CacheBlockIndex < CacheBlockCount; ++CacheBlockIndex)
		{
			FCachedBlock* CachedBlock = new FCachedBlock();
			CachedBlock->Key = uint64(-1);
			CachedBlock->Buffer = CacheMemory + CacheBlockIndex * InReadBufferSize;
			Prev->LruNext = CachedBlock;
			CachedBlock->LruPrev = Prev;
			Prev = CachedBlock;
		}
		Prev->LruNext = &CacheLruTail;
		CacheLruTail.LruPrev = Prev;
	}
}

bool FFileIoStoreBlockCache::Read(FFileIoStoreReadRequest* Block)
{
	if (!CacheMemory)
	{
		return false;
	}
	check(Block->Buffer);
	FCachedBlock* CachedBlock = CachedBlocks.FindRef(Block->Key.Hash);
	if (!CachedBlock)
	{
		Stats.OnBlockCacheMiss(ReadBufferSize);
		return false;
	}
	
	CachedBlock->LruPrev->LruNext = CachedBlock->LruNext;
	CachedBlock->LruNext->LruPrev = CachedBlock->LruPrev;

	CachedBlock->LruPrev = &CacheLruHead;
	CachedBlock->LruNext = CacheLruHead.LruNext;

	CachedBlock->LruPrev->LruNext = CachedBlock;
	CachedBlock->LruNext->LruPrev = CachedBlock;

	check(CachedBlock->Buffer);
	Stats.OnBlockCacheHit(ReadBufferSize);
	FMemory::Memcpy(Block->Buffer->Memory, CachedBlock->Buffer, ReadBufferSize);

	return true;
}

void FFileIoStoreBlockCache::Store(const FFileIoStoreReadRequest* Block)
{
	bool bIsCacheableBlock = CacheMemory != nullptr && Block->BytesUsed < Block->Size;
	if (!bIsCacheableBlock)
	{
		return;
	}
	check(Block->Buffer);
	check(Block->Buffer->Memory);
	FCachedBlock* BlockToReplace = CacheLruTail.LruPrev;
	if (BlockToReplace == &CacheLruHead)
	{
		return;
	}
	check(BlockToReplace);
	CachedBlocks.Remove(BlockToReplace->Key);
	BlockToReplace->Key = Block->Key.Hash;

	BlockToReplace->LruPrev->LruNext = BlockToReplace->LruNext;
	BlockToReplace->LruNext->LruPrev = BlockToReplace->LruPrev;

	BlockToReplace->LruPrev = &CacheLruHead;
	BlockToReplace->LruNext = CacheLruHead.LruNext;

	BlockToReplace->LruPrev->LruNext = BlockToReplace;
	BlockToReplace->LruNext->LruPrev = BlockToReplace;
	
	check(BlockToReplace->Buffer);
	FMemory::Memcpy(BlockToReplace->Buffer, Block->Buffer->Memory, ReadBufferSize);
	Stats.OnBlockCacheStore(ReadBufferSize);
	CachedBlocks.Add(BlockToReplace->Key, BlockToReplace);
}

#define UE_FILEIOSTORE_DETAILED_QUEUE_COUNTERS_ENABLED 0
#if UE_FILEIOSTORE_DETAILED_QUEUE_COUNTERS_ENABLED
TRACE_DECLARE_INT_COUNTER(IoDispatcherLatencyCircuitBreaks, TEXT("IoDispatcher/LatencyCircuitBreaks"));
TRACE_DECLARE_INT_COUNTER(IoDispatcherSeekDistanceCircuitBreaks, TEXT("IoDispatcher/SeekDistanceCircuitBreaks"));
TRACE_DECLARE_INT_COUNTER(IoDispatcherNumPriorityQueues, TEXT("IoDispatcher/NumPriorityQueues"));
#endif

bool FFileIoStoreOffsetSortedRequestQueue::RequestSortPredicate(const FFileIoStoreReadRequestSortKey& A, const FFileIoStoreReadRequestSortKey& B)
{
	if (A.Handle == B.Handle)
	{
		return A.Offset < B.Offset;
	}
	return A.Handle < B.Handle;
}

FFileIoStoreOffsetSortedRequestQueue::FFileIoStoreOffsetSortedRequestQueue(int32 InPriority)
	: Priority(InPriority)
{
}

TArray<FFileIoStoreReadRequest*> FFileIoStoreOffsetSortedRequestQueue::StealRequests()
{
	RequestsBySequence.Clear();
	PeekRequestIndex = INDEX_NONE;
	return MoveTemp(Requests); 
}

// This could be potentially optimized if the higher level keeps track of which requests it changes the priority of, or even just the old priorty levels
TArray<FFileIoStoreReadRequest*> FFileIoStoreOffsetSortedRequestQueue::RemoveMisprioritizedRequests()
{
	PeekRequestIndex = INDEX_NONE;
	TArray<FFileIoStoreReadRequest*> RequestsToReturn;
	for (int32 i = Requests.Num()-1; i >= 0; --i)
	{
		if (Requests[i]->Priority != Priority)
		{
			RequestsToReturn.Add(Requests[i]);
			RequestsBySequence.Remove(Requests[i]);
			Requests.RemoveAt(i, 1, EAllowShrinking::No);
		}
	}

	return RequestsToReturn;
}

FFileIoStoreReadRequest* FFileIoStoreOffsetSortedRequestQueue::GetNextInternal(FFileIoStoreReadRequestSortKey LastSortKey, bool bPop)
{
	if (Requests.Num() == 0)
	{
		return nullptr;
	}

	int32 RequestIndex = INDEX_NONE;
	if (PeekRequestIndex != INDEX_NONE)
	{
		RequestIndex = PeekRequestIndex;
	}
	else 
	{
		bool bHeadRequestTooOld = false;
		if (GIoDispatcherRequestLatencyCircuitBreakerMS > 0)
		{
			// If our oldest request has been unserviced for too long, grab that instead of the next sequential read
			uint64 ThresholdCycles = uint64((GIoDispatcherRequestLatencyCircuitBreakerMS * 1000.0) / FPlatformTime::GetSecondsPerCycle64());
			bHeadRequestTooOld = (FPlatformTime::Cycles64() - RequestsBySequence.PeekHead()->CreationTime) >= ThresholdCycles;

#if UE_FILEIOSTORE_DETAILED_QUEUE_COUNTERS_ENABLED
			if (bPop)
			{
				TRACE_COUNTER_INCREMENT(IoDispatcherLatencyCircuitBreaks);
			}
#endif
		}

		const bool bChooseByOffset = 
				LastSortKey.Handle != 0 
			&&	!bHeadRequestTooOld 
			&&  (GIoDispatcherMaintainSortingOnPriorityChange || LastSortKey.Priority == Priority);
		if (bChooseByOffset)
		{
			// Pick the request with the closest offset to the last thing that we read
			RequestIndex = Algo::LowerBoundBy(Requests, LastSortKey, RequestSortProjection, RequestSortPredicate);
			if (Requests.IsValidIndex(RequestIndex)) // If all requests are before LastOffset we get back out-of-bounds
			{
				if (Requests[RequestIndex]->ContainerFilePartition->FileHandle != LastSortKey.Handle)
				{
					// Changing file handle so switch back to the oldest outstanding request 
					RequestIndex = INDEX_NONE;
				}
				else if (GIoDispatcherMaxForwardSeekKB > 0 && (LastSortKey.Offset - Requests[RequestIndex]->Offset) > uint64(GIoDispatcherMaxForwardSeekKB) * 1024)
				{
					// Large forward seek so switch back to the oldest outstanding request 
					RequestIndex = INDEX_NONE;

#if UE_FILEIOSTORE_DETAILED_QUEUE_COUNTERS_ENABLED
					if (bPop)
					{
						TRACE_COUNTER_INCREMENT(IoDispatcherSeekDistanceCircuitBreaks);
					}
#endif
				}
			}
		}

		if (!Requests.IsValidIndex(RequestIndex))
		{
			RequestIndex = Requests.Find(RequestsBySequence.PeekHead());
			check(Requests[RequestIndex] == RequestsBySequence.PeekHead());
		}
	}

	check(Requests.IsValidIndex(RequestIndex));

	FFileIoStoreReadRequest* Request = Requests[RequestIndex];
	if (bPop)
	{
		Requests.RemoveAt(RequestIndex);
		RequestsBySequence.Remove(Request);
		PeekRequestIndex = INDEX_NONE;
	}
	else
	{
		PeekRequestIndex = RequestIndex;
	}
	return Request;
}

FFileIoStoreReadRequest* FFileIoStoreOffsetSortedRequestQueue::Pop(FFileIoStoreReadRequestSortKey LastSortKey)
{
	return GetNextInternal(LastSortKey, true);
}

void FFileIoStoreOffsetSortedRequestQueue::Push(FFileIoStoreReadRequest* Request)
{
	// Insert sorted by file handle & offset
	int32 InsertIndex = Algo::UpperBoundBy(Requests, RequestSortProjection(Request), RequestSortProjection, RequestSortPredicate);
	Requests.Insert(Request, InsertIndex);
	
	// Insert sorted by age
	RequestsBySequence.Add(Request);

	PeekRequestIndex = INDEX_NONE;
}

int32 HandleContainerUnmounted(const TArrayView<FFileIoStoreReadRequest*> Requests, const FFileIoStoreContainerFile& ContainerFile)
{
	static FFileIoStoreContainerFilePartition UnmountedPartition;

	int32 FailedRequestsCount = 0;

	for (const FFileIoStoreContainerFilePartition& Partition : ContainerFile.Partitions)
	{
		for (FFileIoStoreReadRequest* Request : Requests)
		{
			if (Request->ContainerFilePartition == &Partition)
			{
				Request->bFailed = true;
				Request->ContainerFilePartition = &UnmountedPartition;
				++FailedRequestsCount;
			}
		}
	}

	return FailedRequestsCount;
}

int32 FFileIoStoreOffsetSortedRequestQueue::HandleContainerUnmounted(const FFileIoStoreContainerFile& ContainerFile)
{
	return ::HandleContainerUnmounted(Requests, ContainerFile);
}

void FFileIoStoreRequestQueue::UpdateSortRequestsByOffset()
{
	// Must hold CriticalSection here
	if (bSortRequestsByOffset == bool(GIoDispatcherSortRequestsByOffset))
	{
		return;
	}

	bSortRequestsByOffset = bool(GIoDispatcherSortRequestsByOffset);
	if (bSortRequestsByOffset)
	{
		// Split things into separate heaps
		for (FFileIoStoreReadRequest* Request : Heap)
		{
			Push(*Request);
		}
		Heap.Empty();
	}
	else
	{
		// Put things back into the main heap
		TArray< FFileIoStoreReadRequest*> AllRequests;
		for (FFileIoStoreOffsetSortedRequestQueue& SubQueue : SortedPriorityQueues)
		{
			AllRequests.Append(SubQueue.StealRequests());
		}
		Algo::SortBy(AllRequests, [](FFileIoStoreReadRequest* Request) { return Request->Sequence; });
		for (FFileIoStoreReadRequest* Request : AllRequests)
		{
			Push(*Request);
		}
		check(Algo::AllOf(SortedPriorityQueues, [](const FFileIoStoreOffsetSortedRequestQueue& Q) { return Q.IsEmpty(); }));
		SortedPriorityQueues.Empty();
	}
}

FFileIoStoreReadRequest* FFileIoStoreRequestQueue::Pop()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(RequestQueuePop);
	FScopeLock _(&CriticalSection);
	UpdateSortRequestsByOffset();
	FFileIoStoreReadRequest* Result = nullptr;
	if (bSortRequestsByOffset)
	{
		if (SortedPriorityQueues.Num() == 0)
		{
			return nullptr;
		}

		FFileIoStoreOffsetSortedRequestQueue& SubQueue = SortedPriorityQueues.Last();
		check(!SubQueue.IsEmpty());
		Result = SubQueue.Pop(LastSortKey);
		check(Result);
		LastSortKey = Result;
		if (SubQueue.IsEmpty())
		{
			SortedPriorityQueues.Pop();
			// SubQueue is invalid here
#if UE_FILEIOSTORE_DETAILED_QUEUE_COUNTERS_ENABLED
			TRACE_COUNTER_DECREMENT(IoDispatcherNumPriorityQueues);
#endif
		}
	}
	else
	{
		if (Heap.Num() == 0)
		{
			return nullptr;
		}
		Heap.HeapPop(Result, QueueSortFunc, EAllowShrinking::No);
	}
	
	check(Result->QueueStatus == FFileIoStoreReadRequest::QueueStatus_InQueue);
	Result->QueueStatus = FFileIoStoreReadRequest::QueueStatus_Started;
	Result->ContainerFilePartition->StartedReadRequestsCount.fetch_add(1, std::memory_order_release);
	return Result;
}

void FFileIoStoreRequestQueue::PushToPriorityQueues(FFileIoStoreReadRequest* Request)
{
	int32 QueueIndex = Algo::LowerBoundBy(SortedPriorityQueues, Request->Priority, QueuePriorityProjection, TLess<int32>());
	if (!SortedPriorityQueues.IsValidIndex(QueueIndex) || SortedPriorityQueues[QueueIndex].GetPriority() != Request->Priority)
	{
		SortedPriorityQueues.Insert(FFileIoStoreOffsetSortedRequestQueue(Request->Priority), QueueIndex);
#if UE_FILEIOSTORE_DETAILED_QUEUE_COUNTERS_ENABLED
		TRACE_COUNTER_INCREMENT(IoDispatcherNumPriorityQueues);
#endif
	}
	check(Algo::IsSortedBy(SortedPriorityQueues, QueuePriorityProjection, TLess<int32>()));
	FFileIoStoreOffsetSortedRequestQueue& Queue = SortedPriorityQueues[QueueIndex];
	check(Queue.GetPriority() == Request->Priority);
	Queue.Push(Request);
}

void FFileIoStoreRequestQueue::Push(FFileIoStoreReadRequest& Request)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(RequestQueuePush);
	FScopeLock _(&CriticalSection);
	UpdateSortRequestsByOffset();
	
	check(Request.QueueStatus == FFileIoStoreReadRequest::QueueStatus_NotInQueue);
	Request.QueueStatus = FFileIoStoreReadRequest::QueueStatus_InQueue;

	if (bSortRequestsByOffset)
	{
		PushToPriorityQueues(&Request);
	}
	else
	{
		Heap.HeapPush(&Request, QueueSortFunc);
	}
}

void FFileIoStoreRequestQueue::Push(FFileIoStoreReadRequestList& Requests)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(RequestQueuePush);
	FScopeLock _(&CriticalSection);
	UpdateSortRequestsByOffset();

	for (auto It = Requests.Steal(); It; ++It)
	{
		check(It->QueueStatus == FFileIoStoreReadRequest::QueueStatus_NotInQueue);
		It->QueueStatus = FFileIoStoreReadRequest::QueueStatus_InQueue;

		if (bSortRequestsByOffset)
		{
			PushToPriorityQueues(*It);
		}
		else
		{
			Heap.HeapPush(*It, QueueSortFunc);
		}
	}
}

void FFileIoStoreRequestQueue::UpdateOrder()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(RequestQueueUpdateOrder);
	FScopeLock _(&CriticalSection);
	UpdateSortRequestsByOffset();
	if (bSortRequestsByOffset)
	{
		TArray<FFileIoStoreReadRequest*> Requests;
		for (FFileIoStoreOffsetSortedRequestQueue& SubQueue : SortedPriorityQueues)
		{
			TArray<FFileIoStoreReadRequest*> RequestsRemoved = SubQueue.RemoveMisprioritizedRequests();
			Requests.Append(RequestsRemoved);
		}

		// Pop/Peek rely on empty queues being culled
		SortedPriorityQueues.RemoveAll([](FFileIoStoreOffsetSortedRequestQueue& SubQueue) { return SubQueue.IsEmpty(); });

		Algo::SortBy(Requests, [](FFileIoStoreReadRequest* Request) { return Request->Sequence;  });
		for (FFileIoStoreReadRequest* Request : Requests)
		{
			PushToPriorityQueues(Request);
		}
	}
	else
	{
		Heap.Heapify(QueueSortFunc);
	}
}

void FFileIoStoreRequestQueue::Lock()
{
	CriticalSection.Lock();
}

void FFileIoStoreRequestQueue::Unlock()
{
	CriticalSection.Unlock();
}

int32 FFileIoStoreRequestQueue::HandleContainerUnmounted(const FFileIoStoreContainerFile& ContainerFile)
{
	FScopeLock _(&CriticalSection);
	
	int32 FailedRequestsCount = 0;

	if (bSortRequestsByOffset)
	{
		for (FFileIoStoreOffsetSortedRequestQueue& SubQueue : SortedPriorityQueues)
		{
			FailedRequestsCount += SubQueue.HandleContainerUnmounted(ContainerFile);
		}
	}
	else
	{
		FailedRequestsCount += ::HandleContainerUnmounted(Heap, ContainerFile);
	}

	return FailedRequestsCount;
}

FFileIoStoreReader::FFileIoStoreReader(IPlatformFileIoStore& InPlatformImpl, FFileIoStoreStats& InStats)
	: PlatformImpl(InPlatformImpl)
	, Stats(InStats)
{
}

FFileIoStoreReader::~FFileIoStoreReader()
{
	Close();
}

uint64 FFileIoStoreReader::GetTocAllocatedSize() const
{
	return TocImperfectHashMapFallback.GetAllocatedSize() +
		PerfectHashMap.TocOffsetAndLengths.GetAllocatedSize() +
		PerfectHashMap.TocChunkIds.GetAllocatedSize() +
		PerfectHashMap.TocChunkHashSeeds.GetAllocatedSize() +
		ContainerFile.CompressionBlocks.GetAllocatedSize() +
		(ContainerFile.BlockSignatureTable.IsValid() ? ContainerFile.BlockSignatureTable->Hashes.GetAllocatedSize() : 0);
}

FIoStatus FFileIoStoreReader::Initialize(const TCHAR* InTocFilePath, int32 InOrder)
{
	FStringView ContainerPathView(InTocFilePath);
	if (!ContainerPathView.EndsWith(TEXT(".utoc")))
	{
		return FIoStatusBuilder(EIoErrorCode::FileOpenFailed) << TEXT("Expected .utoc extension on container path '") << InTocFilePath << TEXT("'");
	}
	FStringView BasePathView = ContainerPathView.LeftChop(5);
	ContainerFile.FilePath = BasePathView;

	IPlatformFile& Ipf = FPlatformFileManager::Get().GetPlatformFile();

	UE_LOG(LogIoDispatcher, Display, TEXT("Reading toc: %s"), InTocFilePath);

	TUniquePtr<FIoStoreTocResource> TocResourcePtr = MakeUnique<FIoStoreTocResource>();
	FIoStoreTocResource& TocResource = *TocResourcePtr;
	FIoStatus Status = FIoStoreTocResource::Read(InTocFilePath, EIoStoreTocReadOptions::Default, TocResource);
	if (!Status.IsOk())
	{
		return Status;
	}

	ContainerFile.PartitionSize = TocResource.Header.PartitionSize;
	ContainerFile.Partitions.SetNum(TocResource.Header.PartitionCount);
	for (uint32 PartitionIndex = 0; PartitionIndex < TocResource.Header.PartitionCount; ++PartitionIndex)
	{
		FFileIoStoreContainerFilePartition& Partition = ContainerFile.Partitions[PartitionIndex];
		TStringBuilder<256> ContainerFilePath;
		ContainerFilePath.Append(BasePathView);
		if (PartitionIndex > 0)
		{
			ContainerFilePath.Appendf(TEXT("_s%d"), PartitionIndex);
		}
		ContainerFilePath.Append(TEXT(".ucas"));
		Partition.FilePath = ContainerFilePath;
		if (!PlatformImpl.OpenContainer(*ContainerFilePath, Partition.FileHandle, Partition.FileSize))
		{
			return FIoStatusBuilder(EIoErrorCode::FileOpenFailed) << TEXT("Failed to open IoStore container file '") << *ContainerFilePath << TEXT("'");
		}
		Partition.ContainerFileIndex = GlobalPartitionIndex++;
	}

	if (GIoDispatcherTocsEnablePerfectHashing && !TocResource.ChunkPerfectHashSeeds.IsEmpty())
	{
		for (int32 ChunkIndexWithoutPerfectHash : TocResource.ChunkIndicesWithoutPerfectHash)
		{
			TocImperfectHashMapFallback.Add(TocResource.ChunkIds[ChunkIndexWithoutPerfectHash], TocResource.ChunkOffsetLengths[ChunkIndexWithoutPerfectHash]);
		}

		PerfectHashMap.TocChunkHashSeeds = MoveTemp(TocResource.ChunkPerfectHashSeeds);
		PerfectHashMap.TocOffsetAndLengths = MoveTemp(TocResource.ChunkOffsetLengths);
		PerfectHashMap.TocChunkIds = MoveTemp(TocResource.ChunkIds);
		bHasPerfectHashMap = true;
	}
	else
	{
		UE_LOG(LogIoDispatcher, Warning, TEXT("Falling back to imperfect hashmap for container '%s'"), InTocFilePath);
		for (uint32 ChunkIndex = 0; ChunkIndex < TocResource.Header.TocEntryCount; ++ChunkIndex)
		{
			TocImperfectHashMapFallback.Add(TocResource.ChunkIds[ChunkIndex], TocResource.ChunkOffsetLengths[ChunkIndex]);
		}
		bHasPerfectHashMap = false;
	}
	
	ContainerFile.CompressionMethods	= MoveTemp(TocResource.CompressionMethods);
	ContainerFile.CompressionBlockSize	= TocResource.Header.CompressionBlockSize;
	ContainerFile.CompressionBlocks		= MoveTemp(TocResource.CompressionBlocks);
	ContainerFile.ContainerFlags		= TocResource.Header.ContainerFlags;
	ContainerFile.EncryptionKeyGuid		= TocResource.Header.EncryptionKeyGuid;
	ContainerFile.BlockSignatureTable	= new FFileIoStoreBlockSignatureTable();
	ContainerFile.BlockSignatureTable->Hashes = MoveTemp(TocResource.ChunkBlockSignatures);
	ContainerFile.ContainerInstanceId	= ++GlobalContainerInstanceId;

	Stats.OnTocMounted(GetTocAllocatedSize());

	UE_LOG(LogIoDispatcher, Display, TEXT("Toc signature hash: %s"), *TocResource.SignatureHash.ToString());
	
	ContainerId = TocResource.Header.ContainerId;
	Order = InOrder;
	return FIoStatus::Ok;
}

FIoStatus FFileIoStoreReader::Close()
{
	if (bClosed)
	{
		return FIoStatus::Ok;
	}

	for (FFileIoStoreContainerFilePartition& Partition : ContainerFile.Partitions)
	{
		PlatformImpl.CloseContainer(Partition.FileHandle);
	}

	Stats.OnTocUnmounted(GetTocAllocatedSize());

	PerfectHashMap.TocChunkHashSeeds.Empty();
	PerfectHashMap.TocChunkIds.Empty();
	PerfectHashMap.TocOffsetAndLengths.Empty();
	TocImperfectHashMapFallback.Empty();
	ContainerFile = FFileIoStoreContainerFile();
	ContainerId = FIoContainerId();
	Order = INDEX_NONE;
	bClosed = true;

	return FIoStatus::Ok;
}

const FIoOffsetAndLength* FFileIoStoreReader::FindChunkInternal(const FIoChunkId& ChunkId) const
{
	if (bHasPerfectHashMap)
	{
		// See FIoStoreWriterImpl::GeneratePerfectHashes
		const uint32 ChunkCount = PerfectHashMap.TocChunkIds.Num();
		if (!ChunkCount)
		{
			return nullptr;
		}
		const uint32 SeedCount = PerfectHashMap.TocChunkHashSeeds.Num();
		uint32 SeedIndex = FIoStoreTocResource::HashChunkIdWithSeed(0, ChunkId) % SeedCount;
		const int32 Seed = PerfectHashMap.TocChunkHashSeeds[SeedIndex];
		if (Seed == 0)
		{
			return nullptr;
		}
		uint32 Slot;
		if (Seed < 0)
		{
			uint32 SeedAsIndex = static_cast<uint32>(-Seed - 1);
			if (SeedAsIndex < ChunkCount)
			{
				Slot = static_cast<uint32>(SeedAsIndex);
			}
			else
			{
				// Entry without perfect hash
				return TocImperfectHashMapFallback.Find(ChunkId);
			}
		}
		else
		{
			Slot = FIoStoreTocResource::HashChunkIdWithSeed(static_cast<uint32>(Seed), ChunkId) % ChunkCount;
		}
		if (PerfectHashMap.TocChunkIds[Slot] == ChunkId)
		{
			return &PerfectHashMap.TocOffsetAndLengths[Slot];
		}
		return nullptr;
	}
	else
	{
		return TocImperfectHashMapFallback.Find(ChunkId);
	}
}

bool FFileIoStoreReader::DoesChunkExist(const FIoChunkId& ChunkId) const
{
	check(!bClosed);
	return FindChunkInternal(ChunkId) != nullptr;
}

TIoStatusOr<uint64> FFileIoStoreReader::GetSizeForChunk(const FIoChunkId& ChunkId) const
{
	check(!bClosed);
	const FIoOffsetAndLength* OffsetAndLength = FindChunkInternal(ChunkId);
	if (OffsetAndLength)
	{
		return OffsetAndLength->GetLength();
	}
	else
	{
		return FIoStatus(EIoErrorCode::NotFound);
	}
}

const FIoOffsetAndLength* FFileIoStoreReader::Resolve(const FIoChunkId& ChunkId) const
{
	check(!bClosed);
	return FindChunkInternal(ChunkId);
}

IMappedFileHandle* FFileIoStoreReader::GetMappedContainerFileHandle(uint64 TocOffset)
{
	check(!bClosed);
	int32 PartitionIndex = int32(TocOffset / ContainerFile.PartitionSize);
	FFileIoStoreContainerFilePartition& Partition = ContainerFile.Partitions[PartitionIndex];
	if (!Partition.MappedFileHandle &&
		// Can't map encrypted files, compression should be disabled for bulk data when memory mapping is required
		!EnumHasAnyFlags(ContainerFile.ContainerFlags, EIoContainerFlags::Encrypted))
	{
		IPlatformFile& Ipf = FPlatformFileManager::Get().GetPlatformFile();
		Partition.MappedFileHandle.Reset(Ipf.OpenMapped(*Partition.FilePath));
	}

	check(Partition.FileSize > 0);
	return new FMappedFileProxy(Partition.MappedFileHandle.Get(), Partition.FileSize);
}

TIoStatusOr<FIoContainerHeader> FFileIoStoreReader::ReadContainerHeader() const
{
	LLM_SCOPE(ELLMTag::AsyncLoading);
	TRACE_CPUPROFILER_EVENT_SCOPE(ReadContainerHeader);
	FIoChunkId HeaderChunkId = CreateIoChunkId(ContainerId.Value(), 0, EIoChunkType::ContainerHeader);
	const FIoOffsetAndLength* OffsetAndLength = FindChunkInternal(HeaderChunkId);
	if (!OffsetAndLength)
	{
		return FIoStatus(FIoStatusBuilder(EIoErrorCode::NotFound) << TEXT("Container header chunk not found"));
	}
	
	const uint64 CompressionBlockSize = ContainerFile.CompressionBlockSize;
	const uint64 Offset = OffsetAndLength->GetOffset();
	const uint64 Size = OffsetAndLength->GetLength();
	const uint64 RequestEndOffset = Offset + Size;
	const int32 RequestBeginBlockIndex = int32(Offset / CompressionBlockSize);
	const int32 RequestEndBlockIndex = int32((RequestEndOffset - 1) / CompressionBlockSize);
	
	// Assumes that the container header is uncompressed and placed in its own blocks in the same partition without padding
	const FIoStoreTocCompressedBlockEntry* CompressionBlockEntry = &ContainerFile.CompressionBlocks[RequestBeginBlockIndex];
	const int32 PartitionIndex = int32(CompressionBlockEntry->GetOffset() / ContainerFile.PartitionSize);
	const FFileIoStoreContainerFilePartition& Partition = ContainerFile.Partitions[PartitionIndex];
	const uint64 RawOffset = CompressionBlockEntry->GetOffset() % ContainerFile.PartitionSize;

#if !UE_BUILD_SHIPPING
	// Check for flag - compressed containers still have CompressionBlockSize != 0 and CompressionMethod "None".
	if (EnumHasAnyFlags(ContainerFile.ContainerFlags, EIoContainerFlags::Compressed)) 
	{
		FileCache_PostIoStoreCompressionBlockSize(IntCastChecked<int32>(CompressionBlockSize), Partition.FilePath);
	}
#endif

	FIoBuffer IoBuffer(Align(Size, FAES::AESBlockSize));
	IPlatformFile& Ipf = FPlatformFileManager::Get().GetPlatformFile();
	TUniquePtr<IFileHandle> ContainerFileHandle(Ipf.OpenRead(*Partition.FilePath));
	if (!ContainerFileHandle)
	{
		return FIoStatus(FIoStatusBuilder(EIoErrorCode::FileOpenFailed) << TEXT("Failed to open container file ") << Partition.FilePath);
	}
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ReadFromContainerFile);
		if (!ContainerFileHandle->Seek(RawOffset))
		{
			return FIoStatus(FIoStatusBuilder(EIoErrorCode::ReadError) << FString::Printf(TEXT("Failed seeking to offset %llu in container file"), RawOffset));
		}
		if (!ContainerFileHandle->Read(IoBuffer.Data(), IoBuffer.DataSize()))
		{
			return FIoStatus(FIoStatusBuilder(EIoErrorCode::ReadError) << FString::Printf(TEXT("Failed reading %llu bytes at offset %llu"), IoBuffer.DataSize(), RawOffset));
		}
	}

	const bool bSigned = EnumHasAnyFlags(ContainerFile.ContainerFlags, EIoContainerFlags::Signed);
	const bool bEncrypted = ContainerFile.EncryptionKey.IsValid();
	if (bSigned || bEncrypted)
	{
		uint8* BlockData = IoBuffer.Data();
		for (int32 CompressedBlockIndex = RequestBeginBlockIndex; CompressedBlockIndex <= RequestEndBlockIndex; ++CompressedBlockIndex)
		{
			CompressionBlockEntry = &ContainerFile.CompressionBlocks[CompressedBlockIndex];
			check(ContainerFile.CompressionMethods[CompressionBlockEntry->GetCompressionMethodIndex()].IsNone());
			const uint64 BlockSize = Align(CompressionBlockEntry->GetCompressedSize(), FAES::AESBlockSize);
			if (bSigned)
			{
				const FSHAHash& SignatureHash = ContainerFile.BlockSignatureTable->Hashes[CompressedBlockIndex];
				FSHAHash BlockHash;
				FSHA1::HashBuffer(BlockData, BlockSize, BlockHash.Hash);
				if (SignatureHash != BlockHash)
				{
					return FIoStatus(FIoStatusBuilder(EIoErrorCode::SignatureError) << TEXT("Signature error detected when reading container header"));
				}
			}
			if (bEncrypted)
			{
				FAES::DecryptData(BlockData, uint32(BlockSize), ContainerFile.EncryptionKey);
			}
			BlockData += BlockSize;
		}
	}
	FMemoryReaderView Ar(MakeArrayView(IoBuffer.Data(), static_cast<int32>(IoBuffer.DataSize())));
	FIoContainerHeader ContainerHeader;
	Ar << ContainerHeader;
	if (Ar.IsError())
	{
		UE_LOG(LogIoDispatcher, Warning, TEXT("Invalid container header in file '%s'"), *ContainerFile.FilePath);
		ContainerHeader = FIoContainerHeader();
	}
	return ContainerHeader;
}

void FFileIoStoreReader::ReopenAllFileHandles()
{
	for (FFileIoStoreContainerFilePartition& Partition : ContainerFile.Partitions)
	{
		PlatformImpl.CloseContainer(Partition.FileHandle);
		PlatformImpl.OpenContainer(*Partition.FilePath, Partition.FileHandle, Partition.FileSize);
	}
}

FFileIoStoreResolvedRequest::FFileIoStoreResolvedRequest(
	FIoRequestImpl& InDispatcherRequest,
	FFileIoStoreContainerFile* InContainerFile,
	uint64 InResolvedOffset,
	uint64 InResolvedSize,
	int32 InPriority)
	: DispatcherRequest(&InDispatcherRequest)
	, ContainerFile(InContainerFile)
	, ResolvedOffset(InResolvedOffset)
	, ResolvedSize(InResolvedSize)
	, Priority(InPriority)
{

}

void FFileIoStoreResolvedRequest::AddReadRequestLink(FFileIoStoreReadRequestLink* ReadRequestLink)
{
	check(!ReadRequestLink->Next);
	if (ReadRequestsTail)
	{
		ReadRequestsTail->Next = ReadRequestLink;
	}
	else
	{
		ReadRequestsHead = ReadRequestLink;
	}
	ReadRequestsTail = ReadRequestLink;
}

FFileIoStoreRequestTracker::FFileIoStoreRequestTracker(FFileIoStoreRequestAllocator& InRequestAllocator, FFileIoStoreRequestQueue& InRequestQueue)
	: RequestAllocator(InRequestAllocator)
	, RequestQueue(InRequestQueue)
{

}

FFileIoStoreRequestTracker::~FFileIoStoreRequestTracker()
{

}

FFileIoStoreCompressedBlock* FFileIoStoreRequestTracker::FindOrAddCompressedBlock(FFileIoStoreBlockKey Key, bool& bOutWasAdded)
{
	bOutWasAdded = false;
	FFileIoStoreCompressedBlock*& Result = CompressedBlocksMap.FindOrAdd(Key);
	if (!Result)
	{
		Result = RequestAllocator.AllocCompressedBlock();
		Result->Key = Key;
		bOutWasAdded = true;
	}
	return Result;
}

FFileIoStoreReadRequest* FFileIoStoreRequestTracker::FindOrAddRawBlock(FFileIoStoreBlockKey Key, bool& bOutWasAdded)
{
	bOutWasAdded = false;
	FFileIoStoreReadRequest*& Result = RawBlocksMap.FindOrAdd(Key);
	if (!Result)
	{
		Result = RequestAllocator.AllocReadRequest();
		Result->Key = Key;
		bOutWasAdded = true;
	}
	return Result;
}

void FFileIoStoreRequestTracker::RemoveRawBlock(const FFileIoStoreReadRequest* RawBlock, bool bRemoveFromCancel)
{
	if (!RawBlock->bCancelled || bRemoveFromCancel)
	{
		RawBlocksMap.Remove(RawBlock->Key);
		if (RawBlocksMap.IsEmpty())
		{
			RawBlocksMap.Empty(128);
		}
	}
}

void FFileIoStoreRequestTracker::AddReadRequestsToResolvedRequest(FFileIoStoreCompressedBlock* CompressedBlock, FFileIoStoreResolvedRequest& ResolvedRequest)
{
	//TRACE_CPUPROFILER_EVENT_SCOPE(AddReadRequestsToResolvedRequest);
	bool bUpdateQueueOrder = false;
	++ResolvedRequest.UnfinishedReadsCount;
	for (FFileIoStoreReadRequest* ReadRequest : CompressedBlock->RawBlocks)
	{
		FFileIoStoreReadRequestLink* Link = RequestAllocator.AllocRequestLink(ReadRequest);
		++ReadRequest->RefCount;
		ResolvedRequest.AddReadRequestLink(Link);
		if (ResolvedRequest.GetPriority() > ReadRequest->Priority)
		{
			ReadRequest->Priority = ResolvedRequest.GetPriority();
			bUpdateQueueOrder = true;
		}
	}
	if (bUpdateQueueOrder)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RequestTrackerAddIoRequestUpdateOrder);
		RequestQueue.UpdateOrder();
	}
}

void FFileIoStoreRequestTracker::AddReadRequestsToResolvedRequest(const FFileIoStoreReadRequestList& Requests, FFileIoStoreResolvedRequest& ResolvedRequest)
{
	//TRACE_CPUPROFILER_EVENT_SCOPE(RequestTrackerAddIoRequest);
	for (FFileIoStoreReadRequest* Request : Requests)
	{
		++ResolvedRequest.UnfinishedReadsCount;
		FFileIoStoreReadRequestLink* Link = RequestAllocator.AllocRequestLink(Request);
		++Request->RefCount;
		ResolvedRequest.AddReadRequestLink(Link);
		check(ResolvedRequest.GetPriority() == Request->Priority);
	}
}

void FFileIoStoreRequestTracker::RemoveCompressedBlock(const FFileIoStoreCompressedBlock* CompressedBlock, bool bRemoveFromCancel)
{
	if (!CompressedBlock->bCancelled || bRemoveFromCancel)
	{
		CompressedBlocksMap.Remove(CompressedBlock->Key);
		if (CompressedBlocksMap.IsEmpty())
		{
			CompressedBlocksMap.Empty(512);
		}
	}
}

bool FFileIoStoreRequestTracker::CancelIoRequest(FFileIoStoreResolvedRequest& ResolvedRequest)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(RequestTrackerCancelIoRequest);
	check(!ResolvedRequest.bCancelled);
	bool bShouldComplete = true;
	RequestQueue.Lock();
	FFileIoStoreReadRequestLink* Link = ResolvedRequest.ReadRequestsHead;
	while (Link)
	{
		FFileIoStoreReadRequest& ReadRequest = Link->ReadRequest;
		Link = Link->Next;

		if (ReadRequest.bCancelled)
		{
			continue;
		}

		if (ReadRequest.QueueStatus >= FFileIoStoreReadRequest::QueueStatus_Started)
		{
			bShouldComplete = false;
			continue;
		}

		bool bCancelReadRequest = true;
		for (FFileIoStoreCompressedBlock* CompressedBlock : ReadRequest.CompressedBlocks)
		{
			if (CompressedBlock->bCancelled)
			{
				continue;
			}
			bool bCancelCompressedBlock = true;
			for (FFileIoStoreBlockScatter& Scatter : CompressedBlock->ScatterList)
			{
				if (Scatter.Size > 0 && Scatter.Request != &ResolvedRequest)
				{
					bCancelCompressedBlock = false;
					bCancelReadRequest = false;
				}
				else
				{
					Scatter.Size = 0;
				}
			}
			if (bCancelCompressedBlock)
			{
				CompressedBlock->bCancelled = true;
				RemoveCompressedBlock(CompressedBlock, /*bRemoveFromCancel*/ true);
			}
		}
		if (bCancelReadRequest)
		{
			ReadRequest.bCancelled = true;
			if (!ReadRequest.ImmediateScatter.Request)
			{
				RemoveRawBlock(&ReadRequest, /*bRemoveFromCancel*/ true);
			}
#if DO_CHECK
			for (FFileIoStoreCompressedBlock* CompressedBlock : ReadRequest.CompressedBlocks)
			{
				check(CompressedBlock->bCancelled);
				for (FFileIoStoreBlockScatter& Scatter : CompressedBlock->ScatterList)
				{
					check(!Scatter.Request->DispatcherRequest || Scatter.Request->DispatcherRequest->IsCancelled());
				}
			}
#endif
		}
	}
	RequestQueue.Unlock();

	return bShouldComplete;
}

void FFileIoStoreRequestTracker::UpdatePriorityForIoRequest(FFileIoStoreResolvedRequest& ResolvedRequest)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(RequestTrackerUpdatePriorityForIoRequest);
	bool bUpdateOrder = false;
	FFileIoStoreReadRequestLink* Link = ResolvedRequest.ReadRequestsHead;
	while (Link)
	{
		FFileIoStoreReadRequest& ReadRequest = Link->ReadRequest;
		Link = Link->Next;
		if (ResolvedRequest.GetPriority() > ReadRequest.Priority)
		{
			ReadRequest.Priority = ResolvedRequest.GetPriority();
			bUpdateOrder = true;
		}
	}
	if (bUpdateOrder)
	{
		RequestQueue.UpdateOrder();
	}
}

void FFileIoStoreRequestTracker::ReleaseIoRequestReferences(FFileIoStoreResolvedRequest& ResolvedRequest)
{
	FFileIoStoreReadRequestLink* Link = ResolvedRequest.ReadRequestsHead;
	while (Link)
	{
		FFileIoStoreReadRequestLink* Next = Link->Next;
		check(Link->ReadRequest.RefCount > 0);
		if (--Link->ReadRequest.RefCount == 0)
		{
			for (FFileIoStoreCompressedBlock* CompressedBlock : Link->ReadRequest.CompressedBlocks)
			{
				check(CompressedBlock->RefCount > 0);
				if (--CompressedBlock->RefCount == 0)
				{
					RequestAllocator.Free(CompressedBlock);
				}
			}
			RequestAllocator.Free(&Link->ReadRequest);
		}
		RequestAllocator.Free(Link);
		Link = Next;
	}
	ResolvedRequest.ReadRequestsHead = nullptr;
	ResolvedRequest.ReadRequestsTail = nullptr;
	RequestAllocator.Free(&ResolvedRequest);
}

int64 FFileIoStoreRequestTracker::GetLiveReadRequestsCount() const
{
	return RequestAllocator.GetLiveReadRequestsCount();
}

FFileIoStore::FFileIoStore(TUniquePtr<IPlatformFileIoStore>&& InPlatformImpl)
	: BlockCache(Stats)
	, BufferAllocator(Stats)
	, RequestTracker(RequestAllocator, RequestQueue)
	, PlatformImpl(MoveTemp(InPlatformImpl))
{
}

FFileIoStore::~FFileIoStore()
{
	StopThread();
}

void FFileIoStore::Initialize(TSharedRef<const FIoDispatcherBackendContext> InContext)
{
	check(!Thread);

	BackendContext = InContext;
	bIsMultithreaded = InContext->bIsMultiThreaded;

	ReadBufferSize = (GIoDispatcherBufferSizeKB > 0 ? uint64(GIoDispatcherBufferSizeKB) << 10 : 256 << 10);

	uint64 BufferMemorySize = uint64(GIoDispatcherBufferMemoryMB) << 20ull;
	uint64 BufferSize = uint64(GIoDispatcherBufferSizeKB) << 10ull;
	uint32 BufferAlignment = uint32(GIoDispatcherBufferAlignment);
	BufferAllocator.Initialize(BufferMemorySize, BufferSize, BufferAlignment);

	uint64 CacheMemorySize = uint64(GIoDispatcherCacheSizeMB) << 20ull;
	BlockCache.Initialize(CacheMemorySize, BufferSize);

	PlatformImpl->Initialize({
		&BackendContext->WakeUpDispatcherThreadDelegate,
		&RequestAllocator,
		&BufferAllocator,
		&BlockCache,
		&Stats
	});

	int32 DecompressionContextCount = int32(GIoDispatcherDecompressionWorkerCount > 0 ? GIoDispatcherDecompressionWorkerCount : 4);
	CompressionContexts.SetNum(DecompressionContextCount);
	for (TUniquePtr<FFileIoStoreCompressionContext>& CompressionContext : CompressionContexts)
	{
		CompressionContext = MakeUnique<FFileIoStoreCompressionContext>();
		CompressionContext->Next = FirstFreeCompressionContext;
		FirstFreeCompressionContext = CompressionContext.Get();
	}

	Thread = FRunnableThread::Create(this, TEXT("IoService"), 0, TPri_AboveNormal);
}

void FFileIoStore::StopThread()
{
	if (Thread)
	{
		delete Thread;
		Thread = nullptr;
	}
}

void FFileIoStore::Shutdown()
{
	StopThread();
}

TIoStatusOr<FIoContainerHeader> FFileIoStore::Mount(const TCHAR* InTocPath, int32 Order, const FGuid& EncryptionKeyGuid, const FAES::FAESKey& EncryptionKey)
{
	TUniquePtr<FFileIoStoreReader> Reader(new FFileIoStoreReader(*PlatformImpl, Stats));
	FIoStatus IoStatus = Reader->Initialize(InTocPath, Order);
	if (!IoStatus.IsOk())
	{
		return IoStatus;
	}

	if (Reader->IsEncrypted())
	{
		if (Reader->GetEncryptionKeyGuid() == EncryptionKeyGuid && EncryptionKey.IsValid())
		{
			Reader->SetEncryptionKey(EncryptionKey);
		}
		else
		{
			return FIoStatus(EIoErrorCode::InvalidEncryptionKey, *FString::Printf(TEXT("Invalid encryption key '%s' (container '%s', encryption key '%s')"),
				*EncryptionKeyGuid.ToString(), InTocPath, *Reader->GetEncryptionKeyGuid().ToString()));
		}
	}

	TIoStatusOr<FIoContainerHeader> ContainerHeaderReadResult = Reader->ReadContainerHeader();
	FIoContainerHeader ContainerHeader;
	if (ContainerHeaderReadResult.IsOk())
	{
		ContainerHeader = ContainerHeaderReadResult.ConsumeValueOrDie();
	}
	else if (EIoErrorCode::NotFound != ContainerHeaderReadResult.Status().GetErrorCode())
	{
		return ContainerHeaderReadResult;
	}

	int32 InsertionIndex;
	{
		FWriteScopeLock _(IoStoreReadersLock);
		InsertionIndex = Algo::UpperBound(IoStoreReaders, Reader, [](const TUniquePtr<FFileIoStoreReader>& A, const TUniquePtr<FFileIoStoreReader>& B)
		{
			if (A->GetOrder() != B->GetOrder())
			{
				return A->GetOrder() > B->GetOrder();
			}
			return A->GetContainerInstanceId() > B->GetContainerInstanceId();
		});
		IoStoreReaders.Insert(MoveTemp(Reader), InsertionIndex);
		UE_LOG(LogIoDispatcher, Display, TEXT("Mounting container '%s' in location slot %d"), InTocPath, InsertionIndex);
	}

	return ContainerHeader;
}

bool FFileIoStore::Unmount(const TCHAR* InTocPath)
{
	TUniquePtr<FFileIoStoreReader> ReaderToUnmount;
	{
		FWriteScopeLock _(IoStoreReadersLock);

		for (int32 Idx = 0; Idx < IoStoreReaders.Num(); ++Idx)
		{
			if (IoStoreReaders[Idx]->GetContainerFile()->FilePath == InTocPath)
			{
				ReaderToUnmount = MoveTemp(IoStoreReaders[Idx]);
				IoStoreReaders.RemoveAt(Idx);

				break;
			}
		}
	}
	if (ReaderToUnmount)
	{
		UE_LOG(LogIoDispatcher, Display, TEXT("Unmounting container '%s'"), InTocPath);

		int32 FailedRequestsCount = RequestQueue.HandleContainerUnmounted(*ReaderToUnmount->GetContainerFile());

		UE_CLOG(FailedRequestsCount > 0, LogIoDispatcher, Warning, TEXT("Marking %d queued requests from unmounted container as failed"), FailedRequestsCount);

		bool bHasWarned = false;
		for (const FFileIoStoreContainerFilePartition& Partition : ReaderToUnmount->GetContainerFile()->Partitions)
		{
			if (Partition.StartedReadRequestsCount.load(std::memory_order_acquire) != 0)
			{
				UE_CLOG(!bHasWarned, LogIoDispatcher, Warning, TEXT("Waiting for read requests to finish before unmounting container"));
				bHasWarned = true;
				while (Partition.StartedReadRequestsCount.load(std::memory_order_acquire) != 0)
				{
					FPlatformProcess::Sleep(0);
				}
			}
		}
	}
	else
	{
		UE_LOG(LogIoDispatcher, Display, TEXT("Failed to unmount container '%s'"), InTocPath);
	}
	
	return false;
}

bool FFileIoStore::Resolve(FIoRequestImpl* Request)
{
	FReadScopeLock _(IoStoreReadersLock);
	for (const TUniquePtr<FFileIoStoreReader>& Reader : IoStoreReaders)
	{
		if (const FIoOffsetAndLength* OffsetAndLength = Reader->Resolve(Request->ChunkId))
		{
			uint64 RequestedOffset = Request->Options.GetOffset();
			uint64 ResolvedOffset = OffsetAndLength->GetOffset() + RequestedOffset;
			uint64 ResolvedSize = 0;
			if (RequestedOffset <= OffsetAndLength->GetLength())
			{
				ResolvedSize = FMath::Min(Request->Options.GetSize(), OffsetAndLength->GetLength() - RequestedOffset);
			}

			FFileIoStoreResolvedRequest* ResolvedRequest = RequestAllocator.AllocResolvedRequest(
				*Request,
				Reader->GetContainerFile(),
				ResolvedOffset,
				ResolvedSize,
				Request->Priority);
			Request->BackendData = ResolvedRequest;

			if (ResolvedSize > 0)
			{
				FFileIoStoreReadRequestList CustomRequests;
				if (PlatformImpl->CreateCustomRequests(*ResolvedRequest, CustomRequests))
				{
					Stats.OnReadRequestsQueued(CustomRequests);
					RequestTracker.AddReadRequestsToResolvedRequest(CustomRequests, *ResolvedRequest);
					RequestQueue.Push(CustomRequests);
					OnNewPendingRequestsAdded();
				}
				else
				{
					ReadBlocks(*ResolvedRequest);
				}
			}
			else
			{
				// Nothing to read
				if (RequestedOffset > OffsetAndLength->GetLength())
				{
					ResolvedRequest->bFailed = true;
				}
				else
				{
					ResolvedRequest->CreateBuffer(0);
				}
				CompleteDispatcherRequest(ResolvedRequest);
				RequestTracker.ReleaseIoRequestReferences(*ResolvedRequest);
			}

			return true;
		}
	}

	return false;
}

void FFileIoStore::CancelIoRequest(FIoRequestImpl* Request)
{
	if (Request->BackendData)
	{
		FFileIoStoreResolvedRequest* ResolvedRequest = static_cast<FFileIoStoreResolvedRequest*>(Request->BackendData);
		bool bShouldComplete = RequestTracker.CancelIoRequest(*ResolvedRequest);
		if (bShouldComplete)
		{
			ResolvedRequest->bCancelled = true;
			CompleteDispatcherRequest(ResolvedRequest);
		}
	}
}

void FFileIoStore::UpdatePriorityForIoRequest(FIoRequestImpl* Request)
{
	if (Request->BackendData)
	{
		FFileIoStoreResolvedRequest* ResolvedRequest = static_cast<FFileIoStoreResolvedRequest*>(Request->BackendData);
		ResolvedRequest->Priority = Request->Priority;
		RequestTracker.UpdatePriorityForIoRequest(*ResolvedRequest);
	}
}

bool FFileIoStore::DoesChunkExist(const FIoChunkId& ChunkId) const
{
	FReadScopeLock _(IoStoreReadersLock);
	for (const TUniquePtr<FFileIoStoreReader>& Reader : IoStoreReaders)
	{
		if (Reader->DoesChunkExist(ChunkId))
		{
			return true;
		}
	}
	return false;
}

TIoStatusOr<uint64> FFileIoStore::GetSizeForChunk(const FIoChunkId& ChunkId) const
{
	FReadScopeLock _(IoStoreReadersLock);
	for (const TUniquePtr<FFileIoStoreReader>& Reader : IoStoreReaders)
	{
		TIoStatusOr<uint64> ReaderResult = Reader->GetSizeForChunk(ChunkId);
		if (ReaderResult.IsOk())
		{
			return ReaderResult;
		}
	}
	return FIoStatus(EIoErrorCode::NotFound);
}

FAutoConsoleTaskPriority CPrio_IoDispatcherTaskPriority(
	TEXT("TaskGraph.TaskPriorities.IoDispatcherAsyncTasks"),
	TEXT("Task and thread priority for IoDispatcher decompression."),
	ENamedThreads::BackgroundThreadPriority, // if we have background priority task threads, then use them...
	ENamedThreads::NormalTaskPriority, // .. at normal task priority
	ENamedThreads::NormalTaskPriority // if we don't have background threads, then use normal priority threads at normal task priority instead
);

ENamedThreads::Type FFileIoStore::FDecompressAsyncTask::GetDesiredThread()
{
	return CPrio_IoDispatcherTaskPriority.Get();
}

void FFileIoStore::ScatterBlock(FFileIoStoreCompressedBlock* CompressedBlock, bool bIsAsync)
{
	LLM_SCOPE(ELLMTag::FileSystem);
	TRACE_CPUPROFILER_EVENT_SCOPE(IoDispatcherScatter);
	
	check(!CompressedBlock->bFailed);

	FFileIoStoreCompressionContext* CompressionContext = CompressedBlock->CompressionContext;
	check(CompressionContext);
	uint8* CompressedBuffer;
	if (CompressedBlock->RawBlocks.Num() > 1)
	{
		check(CompressedBlock->CompressedDataBuffer);
		CompressedBuffer = CompressedBlock->CompressedDataBuffer;
	}
	else
	{
		FFileIoStoreReadRequest* RawBlock = CompressedBlock->RawBlocks[0];
		check(CompressedBlock->RawOffset >= RawBlock->Offset);
		uint64 OffsetInBuffer = CompressedBlock->RawOffset - RawBlock->Offset;
		CompressedBuffer = RawBlock->Buffer->Memory + OffsetInBuffer;
	}
	if (!CompressedBlock->bFailed)
	{
		if (CompressedBlock->SignatureHash)
		{
			FSHAHash BlockHash;
			FSHA1::HashBuffer(CompressedBuffer, CompressedBlock->RawSize, BlockHash.Hash);
			if (*CompressedBlock->SignatureHash != BlockHash)
			{
				FIoSignatureError Error;
				{
					FReadScopeLock _(IoStoreReadersLock);
					for (const TUniquePtr<FFileIoStoreReader>& Reader : IoStoreReaders)
					{
						if (CompressedBlock->Key.FileIndex == Reader->GetContainerInstanceId())
						{
							Error.ContainerName = FPaths::GetBaseFilename(Reader->GetContainerFile()->FilePath);
						}
					}
					Error.BlockIndex = CompressedBlock->Key.BlockIndex;
					Error.ExpectedHash = *CompressedBlock->SignatureHash;
					Error.ActualHash = BlockHash;
				}

				UE_LOG(LogIoDispatcher, Warning, TEXT("Signature error detected in container '%s' at block index '%d'"), *Error.ContainerName, Error.BlockIndex);

				check(BackendContext);
				if (BackendContext->SignatureErrorDelegate.IsBound())
				{
					BackendContext->SignatureErrorDelegate.Broadcast(Error);
				}
			}
		}

		if (CompressedBlock->EncryptionKey.IsValid())
		{
			FAES::DecryptData(CompressedBuffer, CompressedBlock->RawSize, CompressedBlock->EncryptionKey);
		}
		uint8* UncompressedBuffer;
		if (CompressedBlock->CompressionMethod.IsNone())
		{
			UncompressedBuffer = CompressedBuffer;
		}
		else
		{
			if (CompressionContext->UncompressedBufferSize < CompressedBlock->UncompressedSize)
			{
				FMemory::Free(CompressionContext->UncompressedBuffer);
				CompressionContext->UncompressedBuffer = reinterpret_cast<uint8*>(FMemory::Malloc(CompressedBlock->UncompressedSize));
				CompressionContext->UncompressedBufferSize = CompressedBlock->UncompressedSize;
			}
			UncompressedBuffer = CompressionContext->UncompressedBuffer;

			bool bFailed = !FCompression::UncompressMemory(CompressedBlock->CompressionMethod, UncompressedBuffer, int32(CompressedBlock->UncompressedSize), CompressedBuffer, int32(CompressedBlock->CompressedSize));
			if (bFailed)
			{
				UE_LOG(LogIoDispatcher, Warning, TEXT("Failed decompressing block"));
				CompressedBlock->bFailed = true;
			}
		}

		for (FFileIoStoreBlockScatter& Scatter : CompressedBlock->ScatterList)
		{
			if (Scatter.Size)
			{
				check(Scatter.DstOffset + Scatter.Size <= Scatter.Request->GetBuffer().DataSize());
				check(Scatter.SrcOffset + Scatter.Size <= CompressedBlock->UncompressedSize);
				FMemory::Memcpy(Scatter.Request->GetBuffer().Data() + Scatter.DstOffset, UncompressedBuffer + Scatter.SrcOffset, Scatter.Size);
			}
		}
	}

	if (bIsAsync)
	{
		FScopeLock Lock(&DecompressedBlocksCritical);
		CompressedBlock->Next = FirstDecompressedBlock;
		FirstDecompressedBlock = CompressedBlock;

		BackendContext->WakeUpDispatcherThreadDelegate.Execute();
	}
}

void FFileIoStore::CompleteDispatcherRequest(FFileIoStoreResolvedRequest* ResolvedRequest)
{
	check(ResolvedRequest);
	check(ResolvedRequest->DispatcherRequest);
	FIoRequestImpl* DispatcherRequest = ResolvedRequest->DispatcherRequest;
	ResolvedRequest->DispatcherRequest = nullptr;
	if (ResolvedRequest->bFailed)
	{
		DispatcherRequest->SetFailed();
	}
	DispatcherRequest->BackendData = nullptr;
	if (!CompletedRequestsTail)
	{
		CompletedRequestsHead = CompletedRequestsTail = DispatcherRequest;
	}
	else
	{
		CompletedRequestsTail->NextRequest = DispatcherRequest;
		CompletedRequestsTail = DispatcherRequest;
	}
	CompletedRequestsTail->NextRequest = nullptr;
}

void FFileIoStore::FinalizeCompressedBlock(FFileIoStoreCompressedBlock* CompressedBlock)
{
	Stats.OnDecompressComplete(CompressedBlock); 

	if (CompressedBlock->RawBlocks.Num() > 1)
	{
		check(CompressedBlock->CompressedDataBuffer || CompressedBlock->bCancelled || CompressedBlock->bFailed);
		if (CompressedBlock->CompressedDataBuffer)
		{
			FMemory::Free(CompressedBlock->CompressedDataBuffer);
		}
	}
	else
	{
		FFileIoStoreReadRequest* RawBlock = CompressedBlock->RawBlocks[0];
		check(RawBlock->BufferRefCount > 0);
		if (--RawBlock->BufferRefCount == 0)
		{
			check(RawBlock->Buffer || RawBlock->bCancelled || RawBlock->bFailed);
			if (RawBlock->Buffer)
			{
				FreeBuffer(*RawBlock->Buffer);
				RawBlock->Buffer = nullptr;
			}
		}
	}
	check(CompressedBlock->CompressionContext || CompressedBlock->bCancelled || CompressedBlock->bFailed);
	if (CompressedBlock->CompressionContext)
	{
		FreeCompressionContext(CompressedBlock->CompressionContext);
	}
	for (int32 ScatterIndex = 0, ScatterCount = CompressedBlock->ScatterList.Num(); ScatterIndex < ScatterCount; ++ScatterIndex)
	{
		FFileIoStoreBlockScatter& Scatter = CompressedBlock->ScatterList[ScatterIndex];
		Stats.OnBytesScattered(Scatter.Size);
		Scatter.Request->bFailed |= CompressedBlock->bFailed;
		check(!CompressedBlock->bCancelled || !Scatter.Request->DispatcherRequest || Scatter.Request->DispatcherRequest->IsCancelled());
		check(Scatter.Request->UnfinishedReadsCount > 0);
		if (--Scatter.Request->UnfinishedReadsCount == 0)
		{
			if (!Scatter.Request->bCancelled)
			{
				CompleteDispatcherRequest(Scatter.Request);
			}
			RequestTracker.ReleaseIoRequestReferences(*Scatter.Request);
		}
	}
}

FIoRequestImpl* FFileIoStore::GetCompletedRequests()
{
	LLM_SCOPE(ELLMTag::FileSystem);
	//TRACE_CPUPROFILER_EVENT_SCOPE(GetCompletedRequests);
	
	if (!bIsMultithreaded)
	{
		while (PlatformImpl->StartRequests(RequestQueue));
	}

	FFileIoStoreReadRequestList CompletedRequests;
	PlatformImpl->GetCompletedRequests(CompletedRequests);
	Stats.OnReadRequestsCompleted(CompletedRequests);
	for (auto It = CompletedRequests.Steal(); It; ++It)
	{
		FFileIoStoreReadRequest* CompletedRequest = *It;

		check(CompletedRequest->QueueStatus == FFileIoStoreReadRequest::QueueStatus_Started);
		CompletedRequest->QueueStatus = FFileIoStoreReadRequest::QueueStatus_Completed;
		int32 PreviousStartedReadRequestsCount = CompletedRequest->ContainerFilePartition->StartedReadRequestsCount.fetch_sub(1, std::memory_order_release);
		check(PreviousStartedReadRequestsCount >= 1);

		if (!CompletedRequest->ImmediateScatter.Request)
		{
			check(CompletedRequest->Buffer || CompletedRequest->bCancelled || CompletedRequest->bFailed);
			RequestTracker.RemoveRawBlock(CompletedRequest);
			
			//TRACE_CPUPROFILER_EVENT_SCOPE(ProcessCompletedBlock);
			for (FFileIoStoreCompressedBlock* CompressedBlock : CompletedRequest->CompressedBlocks)
			{
				CompressedBlock->bFailed |= CompletedRequest->bFailed;
				CompressedBlock->bCancelled |= CompletedRequest->bCancelled;
				if (CompressedBlock->RawBlocks.Num() > 1)
				{
					//TRACE_CPUPROFILER_EVENT_SCOPE(HandleComplexBlock);
					if (!(CompressedBlock->bCancelled | CompressedBlock->bFailed))
					{
						check(CompletedRequest->Buffer);
						if (!CompressedBlock->CompressedDataBuffer)
						{
							CompressedBlock->CompressedDataBuffer = reinterpret_cast<uint8*>(FMemory::Malloc(CompressedBlock->RawSize));
						}

						uint8* Src = CompletedRequest->Buffer->Memory;
						uint8* Dst = CompressedBlock->CompressedDataBuffer;
						uint64 CopySize = CompletedRequest->Size;
						int64 CompletedBlockOffsetInBuffer = int64(CompletedRequest->Offset) - int64(CompressedBlock->RawOffset);
						if (CompletedBlockOffsetInBuffer < 0)
						{
							Src -= CompletedBlockOffsetInBuffer;
							CopySize += CompletedBlockOffsetInBuffer;
						}
						else
						{
							Dst += CompletedBlockOffsetInBuffer;
						}
						uint64 CompressedBlockRawEndOffset = CompressedBlock->RawOffset + CompressedBlock->RawSize;
						uint64 CompletedBlockEndOffset = CompletedRequest->Offset + CompletedRequest->Size;
						if (CompletedBlockEndOffset > CompressedBlockRawEndOffset)
						{
							CopySize -= CompletedBlockEndOffset - CompressedBlockRawEndOffset;
						}
						FMemory::Memcpy(Dst, Src, CopySize);
					}
					check(CompletedRequest->BufferRefCount > 0);
					if (--CompletedRequest->BufferRefCount == 0)
					{
						if (CompletedRequest->Buffer)
						{
							FreeBuffer(*CompletedRequest->Buffer);
							CompletedRequest->Buffer = nullptr;
						}
					}
				}

				check(CompressedBlock->UnfinishedRawBlocksCount > 0);
				if (--CompressedBlock->UnfinishedRawBlocksCount == 0)
				{
					Stats.OnDecompressQueued(CompressedBlock);
					RequestTracker.RemoveCompressedBlock(CompressedBlock);
					if (!ReadyForDecompressionTail)
					{
						ReadyForDecompressionHead = ReadyForDecompressionTail = CompressedBlock;
					}
					else
					{
						ReadyForDecompressionTail->Next = CompressedBlock;
						ReadyForDecompressionTail = CompressedBlock;
					}
					CompressedBlock->Next = nullptr;
				}
			}
		}
		else
		{
			check(!CompletedRequest->Buffer);
			Stats.OnBytesScattered(CompletedRequest->ImmediateScatter.Size);
			FFileIoStoreResolvedRequest* CompletedResolvedRequest = CompletedRequest->ImmediateScatter.Request;
			CompletedResolvedRequest->bFailed |= CompletedRequest->bFailed;
			check(!CompletedRequest->bCancelled || !CompletedResolvedRequest->DispatcherRequest || CompletedResolvedRequest->DispatcherRequest->IsCancelled());
			check(CompletedResolvedRequest->UnfinishedReadsCount > 0);
			if (--CompletedResolvedRequest->UnfinishedReadsCount == 0)
			{
				if (!CompletedResolvedRequest->bCancelled)
				{
					CompleteDispatcherRequest(CompletedResolvedRequest);
				}
				RequestTracker.ReleaseIoRequestReferences(*CompletedResolvedRequest);
			}
		}
	}
	
	FFileIoStoreCompressedBlock* BlockToReap;
	{
		FScopeLock Lock(&DecompressedBlocksCritical);
		BlockToReap = FirstDecompressedBlock;
		FirstDecompressedBlock = nullptr;
	}

	while (BlockToReap)
	{
		FFileIoStoreCompressedBlock* Next = BlockToReap->Next;
		FinalizeCompressedBlock(BlockToReap);
		BlockToReap = Next;
	}

	FFileIoStoreCompressedBlock* BlockToDecompress = ReadyForDecompressionHead;
	while (BlockToDecompress)
	{
		FFileIoStoreCompressedBlock* Next = BlockToDecompress->Next;
		if (BlockToDecompress->bFailed | BlockToDecompress->bCancelled)
		{
			FinalizeCompressedBlock(BlockToDecompress);
			BlockToDecompress = Next;
			continue;
		}
		
		BlockToDecompress->CompressionContext = AllocCompressionContext();
		if (!BlockToDecompress->CompressionContext)
		{
			break;
		}

		for (const FFileIoStoreBlockScatter& Scatter : BlockToDecompress->ScatterList)
		{
			if (Scatter.Size)
			{
				FIoRequestImpl* DispatcherRequest = Scatter.Request->DispatcherRequest;
				check(DispatcherRequest);
				if (!DispatcherRequest->HasBuffer())
				{
					DispatcherRequest->CreateBuffer(Scatter.Request->ResolvedSize);
				}
			}
		}

		// Scatter block asynchronous when the block is compressed, encrypted or signed
		bool bScatterAsync = bIsMultithreaded && GIoDispatcherForceSynchronousScatter==0 && (!BlockToDecompress->CompressionMethod.IsNone() || BlockToDecompress->EncryptionKey.IsValid() || BlockToDecompress->SignatureHash);
		if (bScatterAsync)
		{
			TGraphTask<FDecompressAsyncTask>::CreateTask().ConstructAndDispatchWhenReady(*this, BlockToDecompress);
		}
		else
		{
			ScatterBlock(BlockToDecompress, false);
			FinalizeCompressedBlock(BlockToDecompress);
		}
		BlockToDecompress = Next;
	}
	ReadyForDecompressionHead = BlockToDecompress;
	if (!ReadyForDecompressionHead)
	{
		ReadyForDecompressionTail = nullptr;
	}

	FIoRequestImpl* Result = CompletedRequestsHead;
	CompletedRequestsHead = CompletedRequestsTail = nullptr;
	return Result;
}

TIoStatusOr<FIoMappedRegion> FFileIoStore::OpenMapped(const FIoChunkId& ChunkId, const FIoReadOptions& Options)
{
	if (!FPlatformProperties::SupportsMemoryMappedFiles())
	{
		return FIoStatus(EIoErrorCode::Unknown, TEXT("Platform does not support memory mapped files"));
	}

	if (Options.GetTargetVa() != nullptr)
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid read options"));
	}

	FReadScopeLock _(IoStoreReadersLock);
	for (TUniquePtr<FFileIoStoreReader>& Reader : IoStoreReaders)
	{
		if (const FIoOffsetAndLength* OffsetAndLength = Reader->Resolve(ChunkId))
		{
			uint64 ResolvedOffset = OffsetAndLength->GetOffset();
			uint64 ResolvedSize = FMath::Min(Options.GetSize(), OffsetAndLength->GetLength());
			
			const FFileIoStoreContainerFile* ContainerFile = Reader->GetContainerFile();
			
			int32 BlockIndex = int32(ResolvedOffset / ContainerFile->CompressionBlockSize);
			const FIoStoreTocCompressedBlockEntry& CompressionBlockEntry = ContainerFile->CompressionBlocks[BlockIndex];
			const int64 BlockOffset = (int64)CompressionBlockEntry.GetOffset();
			check(BlockOffset > 0 && IsAligned(BlockOffset, FPlatformProperties::GetMemoryMappingAlignment()));

			IMappedFileHandle* MappedFileHandle = Reader->GetMappedContainerFileHandle(BlockOffset);
			IMappedFileRegion* MappedFileRegion = MappedFileHandle->MapRegion(BlockOffset + Options.GetOffset(), ResolvedSize);
			if (MappedFileRegion != nullptr)
			{
				check(IsAligned(MappedFileRegion->GetMappedPtr(), FPlatformProperties::GetMemoryMappingAlignment()));
				return FIoMappedRegion{ MappedFileHandle, MappedFileRegion };
			}
			else
			{
				return FIoStatus(EIoErrorCode::ReadError);
			}
		}
	}

	// We didn't find any entry for the ChunkId.
	return FIoStatus(EIoErrorCode::NotFound);
}

void FFileIoStore::ReopenAllFileHandles()
{
	UE_CLOG(RequestTracker.GetLiveReadRequestsCount(), LogIoDispatcher, Warning, TEXT("Calling ReopenAllFileHandles with read requests in flight"));
	FWriteScopeLock _(IoStoreReadersLock);
	for (const TUniquePtr<FFileIoStoreReader>& Reader : IoStoreReaders)
	{
		Reader->ReopenAllFileHandles();
	}
}

void FFileIoStore::OnNewPendingRequestsAdded()
{
	if (bIsMultithreaded)
	{
		PlatformImpl->ServiceNotify();
	}
}

void FFileIoStore::ReadBlocks(FFileIoStoreResolvedRequest& ResolvedRequest)
{
	/*TStringBuilder<256> ScopeName;
	ScopeName.Appendf(TEXT("ReadBlock %d"), BlockIndex);
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*ScopeName);*/

	FFileIoStoreContainerFile* ContainerFile = ResolvedRequest.GetContainerFile();
	const uint64 CompressionBlockSize = ContainerFile->CompressionBlockSize;
	const uint64 RequestEndOffset = ResolvedRequest.ResolvedOffset + ResolvedRequest.ResolvedSize;
	int32 RequestBeginBlockIndex = int32(ResolvedRequest.ResolvedOffset / CompressionBlockSize);
	int32 RequestEndBlockIndex = int32((RequestEndOffset - 1) / CompressionBlockSize);

	FFileIoStoreReadRequestList NewBlocks;

	uint64 RequestStartOffsetInBlock = ResolvedRequest.ResolvedOffset - RequestBeginBlockIndex * CompressionBlockSize;
	uint64 RequestRemainingBytes = ResolvedRequest.ResolvedSize;
	uint64 OffsetInRequest = 0;
	for (int32 CompressedBlockIndex = RequestBeginBlockIndex; CompressedBlockIndex <= RequestEndBlockIndex; ++CompressedBlockIndex)
	{
		FFileIoStoreBlockKey CompressedBlockKey;
		CompressedBlockKey.FileIndex = ContainerFile->ContainerInstanceId;
		CompressedBlockKey.BlockIndex = CompressedBlockIndex;
		bool bCompressedBlockWasAdded;
		FFileIoStoreCompressedBlock* CompressedBlock = RequestTracker.FindOrAddCompressedBlock(CompressedBlockKey, bCompressedBlockWasAdded);
		check(CompressedBlock);
		check(!CompressedBlock->bCancelled);
		if (bCompressedBlockWasAdded)
		{
			CompressedBlock->EncryptionKey = ContainerFile->EncryptionKey;
			const FIoStoreTocCompressedBlockEntry& CompressionBlockEntry = ContainerFile->CompressionBlocks[CompressedBlockIndex];
			CompressedBlock->UncompressedSize = CompressionBlockEntry.GetUncompressedSize();
			CompressedBlock->CompressedSize = CompressionBlockEntry.GetCompressedSize();
			CompressedBlock->CompressionMethod = ContainerFile->CompressionMethods[CompressionBlockEntry.GetCompressionMethodIndex()];
			if (EnumHasAnyFlags(ContainerFile->ContainerFlags, EIoContainerFlags::Signed))
			{
				check(ContainerFile->BlockSignatureTable);
				CompressedBlock->BlockSignatureTable = ContainerFile->BlockSignatureTable;
				CompressedBlock->SignatureHash = &ContainerFile->BlockSignatureTable->Hashes[CompressedBlockIndex];
			}
			CompressedBlock->RawSize = Align(CompressionBlockEntry.GetCompressedSize(), FAES::AESBlockSize); // The raw blocks size is always aligned to AES blocks size;

			int32 PartitionIndex = int32(CompressionBlockEntry.GetOffset() / ContainerFile->PartitionSize);
			FFileIoStoreContainerFilePartition& Partition = ContainerFile->Partitions[PartitionIndex];
			uint64 PartitionRawOffset = CompressionBlockEntry.GetOffset() % ContainerFile->PartitionSize;
			CompressedBlock->RawOffset = PartitionRawOffset;
			const uint32 RawBeginBlockIndex = uint32(PartitionRawOffset / ReadBufferSize);
			const uint32 RawEndBlockIndex = uint32((PartitionRawOffset + CompressedBlock->RawSize - 1) / ReadBufferSize);
			const uint32 RawBlockCount = RawEndBlockIndex - RawBeginBlockIndex + 1;
			check(RawBlockCount > 0);
			for (uint32 RawBlockIndex = RawBeginBlockIndex; RawBlockIndex <= RawEndBlockIndex; ++RawBlockIndex)
			{
				FFileIoStoreBlockKey RawBlockKey;
				RawBlockKey.BlockIndex = RawBlockIndex;
				RawBlockKey.FileIndex = Partition.ContainerFileIndex;

				bool bRawBlockWasAdded;
				FFileIoStoreReadRequest* RawBlock = RequestTracker.FindOrAddRawBlock(RawBlockKey, bRawBlockWasAdded);
				check(RawBlock);
				check(!RawBlock->bCancelled);
				if (bRawBlockWasAdded)
				{
					RawBlock->Priority = ResolvedRequest.GetPriority();
					RawBlock->ContainerFilePartition = &Partition;
					RawBlock->Offset = RawBlockIndex * ReadBufferSize;
					uint64 ReadSize = FMath::Min(Partition.FileSize, RawBlock->Offset + ReadBufferSize) - RawBlock->Offset;
					RawBlock->Size = ReadSize;
					NewBlocks.Add(RawBlock);
				}
				RawBlock->BytesUsed += 
					uint32(FMath::Min(CompressedBlock->RawOffset + CompressedBlock->RawSize, RawBlock->Offset + RawBlock->Size) -
						   FMath::Max(CompressedBlock->RawOffset, RawBlock->Offset));
				CompressedBlock->RawBlocks.Add(RawBlock);
				++CompressedBlock->UnfinishedRawBlocksCount;
				++CompressedBlock->RefCount;
				RawBlock->CompressedBlocks.Add(CompressedBlock);
				++RawBlock->BufferRefCount;
			}
		}
		check(CompressedBlock->UncompressedSize > RequestStartOffsetInBlock);
		uint64 RequestSizeInBlock = FMath::Min<uint64>(CompressedBlock->UncompressedSize - RequestStartOffsetInBlock, RequestRemainingBytes);
		check(OffsetInRequest + RequestSizeInBlock <= ResolvedRequest.ResolvedSize);
		check(RequestStartOffsetInBlock + RequestSizeInBlock <= CompressedBlock->UncompressedSize);

		FFileIoStoreBlockScatter& Scatter = CompressedBlock->ScatterList.AddDefaulted_GetRef();
		Scatter.Request = &ResolvedRequest;
		Scatter.DstOffset = OffsetInRequest;
		Scatter.SrcOffset = RequestStartOffsetInBlock;
		Scatter.Size = RequestSizeInBlock;

		RequestRemainingBytes -= RequestSizeInBlock;
		OffsetInRequest += RequestSizeInBlock;
		RequestStartOffsetInBlock = 0;

		RequestTracker.AddReadRequestsToResolvedRequest(CompressedBlock, ResolvedRequest);
	}

	if (!NewBlocks.IsEmpty())
	{
		Stats.OnReadRequestsQueued(NewBlocks);
		RequestQueue.Push(NewBlocks);
		OnNewPendingRequestsAdded();
	}
}

void FFileIoStore::FreeBuffer(FFileIoStoreBuffer& Buffer)
{
	BufferAllocator.FreeBuffer(&Buffer);
	PlatformImpl->ServiceNotify();
}

FFileIoStoreCompressionContext* FFileIoStore::AllocCompressionContext()
{
	FFileIoStoreCompressionContext* Result = FirstFreeCompressionContext;
	if (Result)
	{
		FirstFreeCompressionContext = FirstFreeCompressionContext->Next;
	}
	return Result;
}

void FFileIoStore::FreeCompressionContext(FFileIoStoreCompressionContext* CompressionContext)
{
	CompressionContext->Next = FirstFreeCompressionContext;
	FirstFreeCompressionContext = CompressionContext;
}

bool FFileIoStore::Init()
{
	return true;
}

void FFileIoStore::Stop()
{
	bStopRequested = true;
	PlatformImpl->ServiceNotify();
}

uint32 FFileIoStore::Run()
{
	while (!bStopRequested)
	{
		if (!PlatformImpl->StartRequests(RequestQueue))
		{
			PlatformImpl->ServiceWait();
		}
	}
	return 0;
}

TSharedRef<FFileIoStore> CreateIoDispatcherFileBackend()
{
	bool bCheckForPlatformImplementation = true;
	if (!FGenericPlatformProcess::SupportsMultithreading())
	{
		bCheckForPlatformImplementation = false;
	}
#if !UE_BUILD_SHIPPING
	if (FParse::Param(FCommandLine::Get(), TEXT("forcegenericio")))
	{
		bCheckForPlatformImplementation = false;
	}
#endif

	if (bCheckForPlatformImplementation)
	{
		if (FModuleManager::Get().ModuleExists(ANSI_TO_TCHAR(PLATFORM_IODISPATCHER_MODULE)))
		{
			IPlatformFileIoStoreModule* PlatformModule = FModuleManager::LoadModulePtr<IPlatformFileIoStoreModule>(ANSI_TO_TCHAR(PLATFORM_IODISPATCHER_MODULE));
			if (PlatformModule)
			{
				TUniquePtr<IPlatformFileIoStore> PlatformImpl = PlatformModule->CreatePlatformFileIoStore();
				if (PlatformImpl.IsValid())
				{
					return MakeShared<FFileIoStore>(MoveTemp(PlatformImpl));
				}
			}
		}
#if PLATFORM_IMPLEMENTS_IO
		{
			TUniquePtr<IPlatformFileIoStore> PlatformImpl = CreatePlatformFileIoStore();
			if (PlatformImpl.IsValid())
			{
				return MakeShared<FFileIoStore>(MoveTemp(PlatformImpl));
			}
		}
#endif
	}
	return MakeShared<FFileIoStore>(MakeUnique<FGenericFileIoStoreImpl>());
}

uint32 FFileIoStore::GetThreadId() const
{
	return Thread ? Thread->GetThreadID() : 0; 
}

CSV_DEFINE_CATEGORY(IoDispatcherFileBackend, true);
CSV_DEFINE_CATEGORY(IoDispatcherFileBackendVerbose, false);

// These stats go to both insights and csv by default
// TODO: Ideally these should go to insights even if CSV is not capturing, but not be doubled-up if both CSV and Insights are capturing
// TODO: It would also be nice to send these to insights as int64 without unit conversion where appropriate
// IoDispatcher thread
CSV_DEFINE_STAT(IoDispatcherFileBackend,			FrameBytesScatteredKB);
CSV_DEFINE_STAT(IoDispatcherFileBackend,			QueuedFilesystemReadMB);
CSV_DEFINE_STAT(IoDispatcherFileBackendVerbose,	QueuedFilesystemReads);
CSV_DEFINE_STAT(IoDispatcherFileBackendVerbose,	QueuedUncompressBlocks);	
CSV_DEFINE_STAT(IoDispatcherFileBackendVerbose,	QueuedUncompressInMB);		
CSV_DEFINE_STAT(IoDispatcherFileBackendVerbose,	QueuedUncompressOutMB);	
CSV_DEFINE_STAT(IoDispatcherFileBackendVerbose,	FrameBytesReadKB);				
CSV_DEFINE_STAT(IoDispatcherFileBackendVerbose,	FrameBytesUncompressedInKB);	
CSV_DEFINE_STAT(IoDispatcherFileBackendVerbose,	FrameBytesUncompressedOutKB);	

// FileIoStore thread			
CSV_DEFINE_STAT(IoDispatcherFileBackend,			FrameFilesystemBytesReadKB);	
CSV_DEFINE_STAT(IoDispatcherFileBackend,			FrameSequentialReads);
CSV_DEFINE_STAT(IoDispatcherFileBackend,			FrameSeeks);
CSV_DEFINE_STAT(IoDispatcherFileBackendVerbose,	FrameFilesystemReads);
CSV_DEFINE_STAT(IoDispatcherFileBackendVerbose,	FrameForwardSeeks);
CSV_DEFINE_STAT(IoDispatcherFileBackendVerbose,	FrameBackwardSeeks);
CSV_DEFINE_STAT(IoDispatcherFileBackendVerbose,	FrameHandleChangeSeeks);
CSV_DEFINE_STAT(IoDispatcherFileBackendVerbose,	FrameSeekDistanceMB);	
CSV_DEFINE_STAT(IoDispatcherFileBackendVerbose,	FrameBlockCacheStores);
CSV_DEFINE_STAT(IoDispatcherFileBackendVerbose,	FrameBlockCacheStoresKB);
CSV_DEFINE_STAT(IoDispatcherFileBackendVerbose,	FrameBlockCacheHits);	
CSV_DEFINE_STAT(IoDispatcherFileBackendVerbose,	FrameBlockCacheHitKB);	
CSV_DEFINE_STAT(IoDispatcherFileBackendVerbose,	FrameBlockCacheMisses);
CSV_DEFINE_STAT(IoDispatcherFileBackendVerbose,	FrameBlockCacheMissKB);

#if UE_FILEIOSTORE_STATS_ENABLED

FFileIoStoreStats::FFileIoStoreStats()
#if COUNTERSTRACE_ENABLED
	: QueuedReadRequestsSizeCounter(TEXT("FileIoStore/QueuedReadRequestsSize"), TraceCounterDisplayHint_Memory)
	, CompletedReadRequestsSizeCounter(TEXT("FileIoStore/CompletedReadRequestsSize"), TraceCounterDisplayHint_Memory)
	, QueuedCompressedSizeCounter(TEXT("FileIoStore/QueuedCompressedSize"), TraceCounterDisplayHint_Memory)
	, QueuedUncompressedSizeCounter(TEXT("FileIoStore/QueuedUncompressedSize"), TraceCounterDisplayHint_Memory)
	, CompletedCompressedSizeCounter(TEXT("FileIoStore/CompletedCompressedSize"), TraceCounterDisplayHint_Memory)
	, CompletedUncompressedSizeCounter(TEXT("FileIoStore/CompletedUncompressedSize"), TraceCounterDisplayHint_Memory)
	, FileSystemSeeksTotalDistanceCounter(TEXT("FileIoStore/FileSystemSeeksTotalDistance"), TraceCounterDisplayHint_Memory)
	, FileSystemSeeksForwardCountCounter(TEXT("FileIoStore/FileSystemSeeksForwardCount"), TraceCounterDisplayHint_None)
	, FileSystemSeeksBackwardCountCounter(TEXT("FileIoStore/FileSystemSeeksBackwardCount"), TraceCounterDisplayHint_None)
	, FileSystemSeeksChangeHandleCountCounter(TEXT("FileIoStore/FileSystemSeeksChangeHandleCount"), TraceCounterDisplayHint_None)
	, FileSystemCompletedRequestsSizeCounter(TEXT("FileIoStore/FileSystemCompletedRequestsSize"), TraceCounterDisplayHint_Memory)
	, BlockCacheStoredSizeCounter(TEXT("FileIoStore/BlockCacheStoredSize"), TraceCounterDisplayHint_Memory)
	, BlockCacheHitSizeCounter(TEXT("FileIoStore/BlockCacheHitSize"), TraceCounterDisplayHint_Memory)
	, BlockCacheMissedSizeCounter(TEXT("FileIoStore/BlockCacheMissedSize"), TraceCounterDisplayHint_Memory)
	, ScatteredSizeCounter(TEXT("FileIoStore/ScatteredSize"), TraceCounterDisplayHint_Memory)
	, TocMemoryCounter(TEXT("FileIoStore/TocMemory"), TraceCounterDisplayHint_Memory)
	, AvailableBuffersCounter(TEXT("FileIoStore/AvailableBuffers"), TraceCounterDisplayHint_None)
#endif
{
#if CSV_PROFILER
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FFileIoStoreStats::CsvTick));
#endif
}

FFileIoStoreStats::~FFileIoStoreStats()
{
#if CSV_PROFILER
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
#endif
}

bool FFileIoStoreStats::CsvTick(float DeltaTime)
{
#if CSV_PROFILER
	CSV_CUSTOM_STAT_DEFINED(QueuedFilesystemReadMB, BytesToApproxMB(QueuedFilesystemReadBytes), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT_DEFINED(QueuedFilesystemReads, (int32)QueuedFilesystemReads, ECsvCustomStatOp::Set);

	CSV_CUSTOM_STAT_DEFINED(QueuedUncompressBlocks, int32(QueuedUncompressBlocks), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT_DEFINED(QueuedUncompressInMB, BytesToApproxMB(QueuedUncompressBytesIn), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT_DEFINED(QueuedUncompressOutMB, BytesToApproxMB(QueuedUncompressBytesOut), ECsvCustomStatOp::Set);
#endif

	return true;
}

void FFileIoStoreStats::OnReadRequestsQueued(const FFileIoStoreReadRequestList& Requests)
{
	uint64 TotalBytes = 0;
	int32 NumReads = 0;
	for (const FFileIoStoreReadRequest* Request : Requests)
	{
		++NumReads;
		TotalBytes += Request->Size;
	}

#if CSV_PROFILER
	QueuedFilesystemReadBytes += TotalBytes;
	QueuedFilesystemReads += NumReads;
#endif

#if COUNTERSTRACE_ENABLED
	QueuedReadRequestsSizeCounter.Add(TotalBytes);
#endif
}

void FFileIoStoreStats::OnFilesystemReadStarted(const FFileIoStoreReadRequest* Request)
{
	CSV_CUSTOM_STAT_DEFINED(FrameFilesystemReads, 1, ECsvCustomStatOp::Accumulate);

	if (LastContainerFilePartition != Request->ContainerFilePartition)
	{
		OnHandleChangeSeek();
	}
	else if (LastOffset == Request->Offset)
	{
		OnSequentialRead();
	}
	else
	{
		OnSeek(LastOffset, Request->Offset);
	}
	LastOffset = Request->Offset + Request->Size;
	LastContainerFilePartition = Request->ContainerFilePartition;

	CSV_CUSTOM_STAT_DEFINED(FrameFilesystemReads, 1, ECsvCustomStatOp::Accumulate);
}

void FFileIoStoreStats::OnFilesystemReadsStarted(const FFileIoStoreReadRequestList& Requests)
{
	int64 TotalBytes = 0;
	int32 NumReads = 0;
	for (const FFileIoStoreReadRequest* Request : Requests)
	{
		if (LastContainerFilePartition != Request->ContainerFilePartition)
		{
			OnHandleChangeSeek();
		}
		else if (LastOffset == Request->Offset)
		{
			OnSequentialRead();
		}
		else
		{
			OnSeek(LastOffset, Request->Offset);
		}

		LastOffset = Request->Offset + Request->Size;
		LastContainerFilePartition = Request->ContainerFilePartition;

		++NumReads;
		TotalBytes += Request->Size;
	}

	CSV_CUSTOM_STAT_DEFINED(FrameFilesystemReads, NumReads, ECsvCustomStatOp::Accumulate);
}

void FFileIoStoreStats::OnFilesystemReadCompleted(const FFileIoStoreReadRequest* CompletedRequest)
{
	CSV_CUSTOM_STAT_DEFINED(FrameFilesystemBytesReadKB, BytesToApproxKB(CompletedRequest->Size), ECsvCustomStatOp::Accumulate);
#if COUNTERSTRACE_ENABLED
	FileSystemCompletedRequestsSizeCounter.Add(CompletedRequest->Size);
#endif
}

void FFileIoStoreStats::OnFilesystemReadsCompleted(const FFileIoStoreReadRequestList& CompletedRequests)
{
	int64 TotalBytes = 0;
	int32 NumReads = 0;
	for (const FFileIoStoreReadRequest* Request : CompletedRequests)
	{
		++NumReads;
		TotalBytes += Request->Size;
	}

	CSV_CUSTOM_STAT_DEFINED(FrameFilesystemBytesReadKB, BytesToApproxKB(TotalBytes), ECsvCustomStatOp::Accumulate);
#if COUNTERSTRACE_ENABLED
	FileSystemCompletedRequestsSizeCounter.Add(TotalBytes);
#endif
}

void FFileIoStoreStats::OnReadRequestsCompleted(const FFileIoStoreReadRequestList& CompletedRequests)
{
	int64 TotalBytes = 0;
	int32 NumReads = 0;
	for (const FFileIoStoreReadRequest* Request : CompletedRequests)
	{
		++NumReads;
		TotalBytes += Request->Size;
	}

#if CSV_PROFILER
	QueuedFilesystemReadBytes -= TotalBytes;
	QueuedFilesystemReads -= NumReads;
#endif

	CSV_CUSTOM_STAT_DEFINED(FrameBytesReadKB, BytesToApproxKB(TotalBytes), ECsvCustomStatOp::Accumulate); // TODO: Send to insights if CSV is disabled

#if COUNTERSTRACE_ENABLED
	QueuedReadRequestsSizeCounter.Subtract(TotalBytes);
	CompletedReadRequestsSizeCounter.Add(TotalBytes);
#endif
}

void FFileIoStoreStats::OnDecompressQueued(const FFileIoStoreCompressedBlock* CompressedBlock)
{
#if CSV_PROFILER
	++QueuedUncompressBlocks;
	QueuedUncompressBytesIn += CompressedBlock->CompressedSize;
	QueuedUncompressBytesOut += CompressedBlock->UncompressedSize;
#endif

#if COUNTERSTRACE_ENABLED
	QueuedCompressedSizeCounter.Add(CompressedBlock->CompressedSize);
	QueuedUncompressedSizeCounter.Add(CompressedBlock->UncompressedSize);
#endif
}

void FFileIoStoreStats::OnDecompressComplete(const FFileIoStoreCompressedBlock* CompressedBlock)
{
#if CSV_PROFILER
	--QueuedUncompressBlocks;
	QueuedUncompressBytesIn -= CompressedBlock->CompressedSize;
	QueuedUncompressBytesOut -= CompressedBlock->UncompressedSize;
#endif

	CSV_CUSTOM_STAT_DEFINED(FrameBytesUncompressedInKB, BytesToApproxKB(CompressedBlock->CompressedSize), ECsvCustomStatOp::Accumulate); 
	CSV_CUSTOM_STAT_DEFINED(FrameBytesUncompressedOutKB, BytesToApproxKB(CompressedBlock->UncompressedSize), ECsvCustomStatOp::Accumulate);

#if COUNTERSTRACE_ENABLED
	QueuedCompressedSizeCounter.Subtract(CompressedBlock->CompressedSize);
	QueuedUncompressedSizeCounter.Subtract(CompressedBlock->UncompressedSize);

	CompletedCompressedSizeCounter.Add(CompressedBlock->CompressedSize);
	CompletedUncompressedSizeCounter.Add(CompressedBlock->UncompressedSize);
#endif
}

void FFileIoStoreStats::OnBytesScattered(int64 NumBytes)
{
	CSV_CUSTOM_STAT_DEFINED(FrameBytesScatteredKB, BytesToApproxKB(NumBytes), ECsvCustomStatOp::Accumulate);
#if COUNTERSTRACE_ENABLED
	ScatteredSizeCounter.Add(NumBytes);
#endif
}

void FFileIoStoreStats::OnSequentialRead()
{
	CSV_CUSTOM_STAT_DEFINED(FrameSequentialReads, 1, ECsvCustomStatOp::Accumulate);
}

void FFileIoStoreStats::OnSeek(uint64 PrevOffset, uint64 NewOffset)
{
	if (NewOffset > PrevOffset)
	{
		int64 Delta = NewOffset - PrevOffset;

		CSV_CUSTOM_STAT_DEFINED(FrameForwardSeeks, 1, ECsvCustomStatOp::Accumulate);
		CSV_CUSTOM_STAT_DEFINED(FrameSeekDistanceMB, BytesToApproxMB(Delta), ECsvCustomStatOp::Accumulate);

#if COUNTERSTRACE_ENABLED
		FileSystemSeeksTotalDistanceCounter.Add(Delta);
		FileSystemSeeksForwardCountCounter.Increment();
#endif
	}
	else
	{
		int64 Delta = PrevOffset - NewOffset;
		CSV_CUSTOM_STAT_DEFINED(FrameBackwardSeeks, 1, ECsvCustomStatOp::Accumulate);
		CSV_CUSTOM_STAT_DEFINED(FrameSeekDistanceMB, BytesToApproxMB(Delta), ECsvCustomStatOp::Accumulate);

#if COUNTERSTRACE_ENABLED
		FileSystemSeeksTotalDistanceCounter.Add(Delta);
		FileSystemSeeksBackwardCountCounter.Increment();
#endif
	}

	CSV_CUSTOM_STAT_DEFINED(FrameSeeks, 1, ECsvCustomStatOp::Accumulate);
}

void FFileIoStoreStats::OnHandleChangeSeek()
{
	CSV_CUSTOM_STAT_DEFINED(FrameHandleChangeSeeks, 1, ECsvCustomStatOp::Accumulate);
	CSV_CUSTOM_STAT_DEFINED(FrameSeeks, 1, ECsvCustomStatOp::Accumulate);

#if COUNTERSTRACE_ENABLED
	FileSystemSeeksChangeHandleCountCounter.Increment();
#endif
}

void FFileIoStoreStats::OnBlockCacheStore(uint64 NumBytes)
{
	CSV_CUSTOM_STAT_DEFINED(FrameBlockCacheStores, 1, ECsvCustomStatOp::Accumulate);
	CSV_CUSTOM_STAT_DEFINED(FrameBlockCacheStoresKB, BytesToApproxKB(NumBytes), ECsvCustomStatOp::Accumulate);

#if COUNTERSTRACE_ENABLED
	BlockCacheStoredSizeCounter.Add(NumBytes);
#endif
}

void FFileIoStoreStats::OnBlockCacheHit(uint64 NumBytes)
{
	CSV_CUSTOM_STAT_DEFINED(FrameBlockCacheHits, 1, ECsvCustomStatOp::Accumulate);
	CSV_CUSTOM_STAT_DEFINED(FrameBlockCacheHitKB, BytesToApproxKB(NumBytes), ECsvCustomStatOp::Accumulate);

#if COUNTERSTRACE_ENABLED
	BlockCacheHitSizeCounter.Add(NumBytes);
#endif
}

void FFileIoStoreStats::OnBlockCacheMiss(uint64 NumBytes)
{
	CSV_CUSTOM_STAT_DEFINED(FrameBlockCacheMisses, 1, ECsvCustomStatOp::Accumulate);
	CSV_CUSTOM_STAT_DEFINED(FrameBlockCacheMissKB, BytesToApproxKB(NumBytes), ECsvCustomStatOp::Accumulate);

#if COUNTERSTRACE_ENABLED
	BlockCacheMissedSizeCounter.Add(NumBytes);
#endif
}

void FFileIoStoreStats::OnTocMounted(uint64 AllocatedSize)
{
#if COUNTERSTRACE_ENABLED
	TocMemoryCounter.Add(AllocatedSize);
#endif
}

void FFileIoStoreStats::OnTocUnmounted(uint64 AllocatedSize)
{
#if COUNTERSTRACE_ENABLED
	TocMemoryCounter.Subtract(AllocatedSize);
#endif
}

void FFileIoStoreStats::OnBufferReleased()
{
#if COUNTERSTRACE_ENABLED
	AvailableBuffersCounter.Increment();
#endif
}

void FFileIoStoreStats::OnBufferAllocated()
{
#if COUNTERSTRACE_ENABLED
	AvailableBuffersCounter.Decrement();
#endif
}

#endif