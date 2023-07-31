// Copyright Epic Games, Inc. All Rights Reserved.

#include "RenderGraphAllocator.h"
#include "RenderGraphPrivate.h"

FRDGAllocator& FRDGAllocator::Get()
{
	static FRDGAllocator Instance;
	return Instance;
}

FRDGAllocator::~FRDGAllocator()
{
	Context.ReleaseAll();
	ContextForTasks.ReleaseAll();
}

void FRDGAllocator::FContext::ReleaseAll()
{
	for (int32 Index = TrackedAllocs.Num() - 1; Index >= 0; --Index)
	{
#if RDG_USE_MALLOC
		delete TrackedAllocs[Index];
#else
		TrackedAllocs[Index]->~FTrackedAlloc();
#endif
	}
	TrackedAllocs.Reset();

#if RDG_USE_MALLOC
	for (void* RawAlloc : RawAllocs)
	{
		FMemory::Free(RawAlloc);
	}
	RawAllocs.Reset();
#else
	MemStack.Flush();
#endif
}

void FRDGAllocator::ReleaseAll()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FRDGAllocator::ReleaseAll);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE_CONDITIONAL(RDGAllocator_Clear, GRDGVerboseCSVStats != 0);
	Context.ReleaseAll();
	ContextForTasks.ReleaseAll();
}

FRDGAllocatorScope::~FRDGAllocatorScope()
{
	if (AsyncDeleteFunction)
	{
		FFunctionGraphTask::CreateAndDispatchWhenReady(
			[Allocator = MoveTemp(Allocator), AsyncDeleteFunction = MoveTemp(AsyncDeleteFunction)] () mutable
		{
			SCOPED_NAMED_EVENT(FRDGAllocatorScope_AsyncDelete, FColor::Emerald);
			AsyncDeleteFunction();
			AsyncDeleteFunction = {};

		}, TStatId(), nullptr, ENamedThreads::AnyThread);
	}
	else
	{
		Allocator.ReleaseAll();
	}
}
