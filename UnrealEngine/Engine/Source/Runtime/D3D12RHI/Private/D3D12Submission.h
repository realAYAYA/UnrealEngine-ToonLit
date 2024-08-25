// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/TaskGraphInterfaces.h"
#include "D3D12RHICommon.h"
#include "Templates/RefCounting.h"
#include "D3D12BindlessDescriptors.h"

enum class ED3D12QueueType;

class FD3D12CommandAllocator;
class FD3D12CommandList;
class FD3D12DynamicRHI;
class FD3D12QueryHeap;
class FD3D12Queue;
class FD3D12Timing;
class FD3D12Buffer;
class FD3D12Resource;

class FD3D12SyncPoint;
using FD3D12SyncPointRef = TRefCountPtr<FD3D12SyncPoint>;

enum class ED3D12SyncPointType
{
	// Sync points of this type do not include an FGraphEvent, so cannot
	// report completion to the CPU (via either IsComplete() or Wait())
	GPUOnly,

	// Sync points of this type include an FGraphEvent. The IsComplete() and Wait() functions
	// can be used to poll for completion from the CPU, or block the CPU, respectively.
	GPUAndCPU,
};

// Fence type used by the device queues to manage GPU completion
struct FD3D12Fence
{
	TRefCountPtr<ID3D12Fence> D3DFence;
	uint64 LastSignaledValue = 0;
	bool bInterruptAwaited = false;
};

// Used by FD3D12SyncPoint and the submission thread to fix up signaled fence values at the end-of-pipe
struct FD3D12ResolvedFence
{
	FD3D12Fence* Fence;
	uint64 Value = 0;

	FD3D12ResolvedFence() = default;
	FD3D12ResolvedFence(FD3D12Fence* Fence, uint64 Value)
		: Fence(Fence)
		, Value(Value)
	{}
};

//
// A sync point is a logical point on a GPU queue's timeline that can be awaited by other queues, or the CPU.
// These are used throughout the RHI as a way to abstract the underlying D3D12 fences. The submission thread 
// manages the underlying fences and signaled values, and reports completion to the relevant sync points via 
// an FGraphEvent.
//
// Sync points are one-shot, meaning they represent a single timeline point, and are released after use, via ref-counting.
// Use FD3D12SyncPoint::Create() to make a new sync point and hold a reference to it via a FD3D12SyncPointRef object.
//
class FD3D12SyncPoint final : public FThreadSafeRefCountedObject
{
	friend FD3D12DynamicRHI;
	friend FD3D12Queue;

	static TLockFreePointerListUnordered<void, PLATFORM_CACHE_LINE_SIZE> MemoryPool;

	// No copying or moving
	FD3D12SyncPoint(FD3D12SyncPoint const&) = delete;
	FD3D12SyncPoint(FD3D12SyncPoint&&) = delete;

	TOptional<FD3D12ResolvedFence> ResolvedFence;
	FGraphEventRef GraphEvent;

	FD3D12SyncPoint(ED3D12SyncPointType Type)
	{
		if (Type == ED3D12SyncPointType::GPUAndCPU)
		{
			GraphEvent = FGraphEvent::CreateGraphEvent();
		}
	}

public:
	static FD3D12SyncPointRef Create(ED3D12SyncPointType Type)
	{
		LLM_SCOPE_BYNAME(TEXT("RHIMisc/CreateSyncPoint"));
		return new FD3D12SyncPoint(Type);
	}

	bool IsComplete() const
	{
		checkf(GraphEvent, TEXT("This sync point was not created with a CPU event. Cannot check completion on the CPU."));
		return GraphEvent->IsComplete();
	}

	void Wait() const;

	FGraphEvent* GetGraphEvent() const
	{
		checkf(GraphEvent, TEXT("This sync point was not created with a CPU event."));
		return GraphEvent;
	}

	ED3D12SyncPointType GetType() const
	{
		return GraphEvent != nullptr
			? ED3D12SyncPointType::GPUAndCPU
			: ED3D12SyncPointType::GPUOnly;
	}

	void* operator new(size_t Size)
	{
		check(Size == sizeof(FD3D12SyncPoint));

		void* Memory = MemoryPool.Pop();
		if (!Memory)
		{
			Memory = FMemory::Malloc(sizeof(FD3D12SyncPoint), alignof(FD3D12SyncPoint));
		}
		return Memory;
	}

	void operator delete(void* Pointer)
	{
		MemoryPool.Push(Pointer);
	}
};

enum class ED3D12QueryType
{
	None,
	CommandListBegin,
	CommandListEnd,
	PipelineStats,
	IdleBegin,
	IdleEnd,
	AdjustedMicroseconds,
	AdjustedRaw,
	Occlusion
};

// The location of a single (timestamp or occlusion) query result.
struct FD3D12QueryLocation
{
	// The heap in which the result is contained.
	FD3D12QueryHeap* Heap = nullptr;

	// The index of the query within the heap.
	uint32 Index = 0;

	ED3D12QueryType Type = ED3D12QueryType::None;

	// The location into which the result is written by the interrupt thread.
	void* Target = nullptr;

	// Reads the query result from the heap
	inline void CopyResultTo(void* Dst) const;

	template <typename TValueType>
	inline TValueType GetResult() const;

	FD3D12QueryLocation() = default;
	FD3D12QueryLocation(FD3D12QueryHeap* Heap, uint32 Index, ED3D12QueryType Type, void* Target)
		: Heap	(Heap  )
		, Index	(Index )
		, Type	(Type  )
		, Target(Target)
	{}

	operator bool() const { return Heap != nullptr; }
};

struct FBreadcrumbStack
{
	struct FScope
	{
		uint32 NameCRC;
		uint32 MarkerIndex;
		uint32 Child;
		uint32 Sibling;
	};

	FD3D12Queue* Queue = nullptr;
	uint32 NextIdx{ 0 };
	int32 ContextId;
	uint32 MaxMarkers{ 0 };
	D3D12_GPU_VIRTUAL_ADDRESS WriteAddress;
	void* CPUAddress;

	TArray<FScope> Scopes;
	TArray<uint32> ScopeStack;
	bool bTopIsOpen{ false };

	FBreadcrumbStack();
	~FBreadcrumbStack();

	void Initialize(TUniquePtr<struct FD3D12DiagnosticBuffer>& DiagnosticBuffer);
};

struct FD3D12QueryRange
{
	TRefCountPtr<FD3D12QueryHeap> Heap;
	uint32 Start = 0, End = 0;

	inline bool IsFull() const;
};

struct FD3D12CommitReservedResourceDesc
{
	FD3D12Resource* Resource = nullptr;
	uint64 CommitSizeInBytes = 0;
};

// A single unit of work (specific to a single GPU node and queue type) to be processed by the submission thread.
struct FD3D12PayloadBase
{
	// Used to signal FD3D12ManualFence instances on the submission thread.
	struct FManualFence
	{
		// The D3D fence to signal
		TRefCountPtr<ID3D12Fence> Fence;

		// The value to signal the fence with.
		uint64 Value;

		FManualFence() = default;
		FManualFence(TRefCountPtr<ID3D12Fence>&& Fence, uint64 Value)
			: Fence(MoveTemp(Fence))
			, Value(Value)
		{}
	};

	// Constants
	FD3D12Queue& Queue;

	// Wait
	struct : public TArray<FD3D12SyncPointRef>
	{
		// Used to pause / resume iteration of the sync point array on the
		// submission thread when we find a sync point that is unresolved.
		int32 Index = 0;

	} SyncPointsToWait;

	virtual void PreExecute();

	// Wait
	TArray<FManualFence> FencesToWait;

	// UpdateReservedResources
	TArray<FD3D12CommitReservedResourceDesc> ReservedResourcesToCommit;

	// Execute
	TArray<FD3D12CommandList*> CommandListsToExecute;

	// Signal
	TArray<FManualFence> FencesToSignal;
	TOptional<FD3D12Timing*> Timing;
	TArray<FD3D12SyncPointRef> SyncPointsToSignal;
	uint64 CompletionFenceValue = 0;
	FGraphEventRef SubmissionEvent;
	TOptional<uint64> SubmissionTime;

	// Flags.
	bool bAlwaysSignal = false;

	// Cleanup
	TArray<FD3D12CommandAllocator*> AllocatorsToRelease;
	TArray<FD3D12QueryLocation> TimestampQueries;
	TArray<FD3D12QueryLocation> OcclusionQueries;
	TArray<FD3D12QueryLocation> PipelineStatsQueries;
	TArray<FD3D12QueryRange> QueryRanges;

	// GPU crash breadcrumbs stack
	TArray<TSharedPtr<FBreadcrumbStack>> BreadcrumbStacks;

	virtual ~FD3D12PayloadBase();

	// Used by RHIRunOnQueue
	TFunction<void(ID3D12CommandQueue*)> PreExecuteCallback;

protected:
	FD3D12PayloadBase(FD3D12Device* Device, ED3D12QueueType QueueType);
};

#include COMPILED_PLATFORM_HEADER(D3D12Submission.h)
