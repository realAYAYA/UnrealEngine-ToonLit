// Copyright Epic Games, Inc. All Rights Reserved.

#include "RenderGraphAllocator.h"
#include "Async/TaskGraphInterfaces.h"
#include "RenderCore.h"
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
	for (int32 Index = Objects.Num() - 1; Index >= 0; --Index)
	{
#if RDG_USE_MALLOC
		delete Objects[Index];
#else
		Objects[Index]->~FObject();
#endif
	}
	Objects.Reset();

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

FORCENOINLINE void UE::RenderCore::Private::OnInvalidRDGAllocatorNum(int32 NewNum, SIZE_T NumBytesPerElement)
{
	UE_LOG(LogRendererCore, Fatal, TEXT("Trying to resize TRDGArrayAllocator to an invalid size of %d with element size %" SIZE_T_FMT), NewNum, NumBytesPerElement);
	for (;;);
}