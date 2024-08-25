// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Context.h"
#include "Transaction.h"

#include "HAL/Platform.h"

#if PLATFORM_HAS_ASAN_INCLUDE && USING_ADDRESS_SANITISER
#include <sanitizer/asan_interface.h>
#endif

namespace AutoRTFM
{

AUTORTFM_NO_ASAN UE_AUTORTFM_FORCEINLINE void FTransaction::RecordWriteMaxPageSized(void* LogicalAddress, size_t Size)
{
#if PLATFORM_HAS_ASAN_INCLUDE && USING_ADDRESS_SANITISER
    // TODO(SOL-5123): Can we detect shadow memory locations at compile-time instead?
	const char* const Location = __asan_locate_address(LogicalAddress, nullptr, 0, nullptr, nullptr);

    if (strstr(Location, "shadow"))
	{
		return;
	}
#endif

    void* CopyAddress = WriteLogBumpAllocator.Allocate(Size);
    memcpy(CopyAddress, LogicalAddress, Size);

    WriteLog.Push(FWriteLogEntry(LogicalAddress, Size, CopyAddress));
}

AUTORTFM_NO_ASAN UE_AUTORTFM_FORCEINLINE void FTransaction::RecordWrite(void* LogicalAddress, size_t Size)
{
    if (0 == Size)
    {
        return;
    }

    // If we are recording a stack address that is relative to our current
    // transactions stack location, we do not need to record the data in the
    // write log because if that transaction aborted, that memory will cease to
    // be meaningful anyway!
    if (Context->IsInnerTransactionStack(LogicalAddress))
    {
        Stats.Collect<EStatsKind::HitSetSkippedBecauseOfStackLocalMemory>();
        return;
    }

    if (Size <= FWriteLogBumpAllocator::MaxSize)
    {
        FMemoryLocation Key(LogicalAddress);
        Key.SetTopTag(static_cast<uint16_t>(Size));

        if (!HitSet.Insert(Key))
        {
            Stats.Collect<EStatsKind::HitSetHit>();
            return;
        }
    
        Stats.Collect<EStatsKind::HitSetMiss>();
    }

	if (NewMemoryTracker.Contains(LogicalAddress, Size))
	{
		Stats.Collect<EStatsKind::NewMemoryTrackerHit>();
		return;
	}

	Stats.Collect<EStatsKind::NewMemoryTrackerMiss>();

    uint8_t* const Address = reinterpret_cast<uint8_t*>(LogicalAddress);

    size_t I = 0;

    for (; (I + FWriteLogBumpAllocator::MaxSize) < Size; I += FWriteLogBumpAllocator::MaxSize)
    {
        RecordWriteMaxPageSized(Address + I, FWriteLogBumpAllocator::MaxSize);
    }

    // Remainder at the end of the memcpy.
    RecordWriteMaxPageSized(Address + I, Size - I);
}

template<unsigned SIZE> AUTORTFM_NO_ASAN UE_AUTORTFM_FORCEINLINE void FTransaction::RecordWrite(void* LogicalAddress)
{
    static_assert(SIZE <= FWriteLogBumpAllocator::MaxSize);

    // If we are recording a stack address that is relative to our current
    // transactions stack location, we do not need to record the data in the
    // write log because if that transaction aborted, that memory will cease to
    // be meaningful anyway!
    if (Context->IsInnerTransactionStack(LogicalAddress))
    {
        Stats.Collect<EStatsKind::HitSetSkippedBecauseOfStackLocalMemory>();
        return;
    }

    FMemoryLocation Key(LogicalAddress);
    Key.SetTopTag(static_cast<uint16_t>(SIZE));

    if (!HitSet.Insert(Key))
    {
        Stats.Collect<EStatsKind::HitSetHit>();
        return;
    }

    Stats.Collect<EStatsKind::HitSetMiss>();

	if (NewMemoryTracker.Contains(LogicalAddress, SIZE))
	{
		Stats.Collect<EStatsKind::NewMemoryTrackerHit>();
		return;
	}

	Stats.Collect<EStatsKind::NewMemoryTrackerMiss>();

    RecordWriteMaxPageSized(LogicalAddress, SIZE);
}

UE_AUTORTFM_FORCEINLINE void FTransaction::DidAllocate(void* LogicalAddress, const size_t Size)
{
	if (0 == Size)
	{
		return;
	}

    const bool DidInsert = NewMemoryTracker.Insert(LogicalAddress, Size);
    ASSERT(DidInsert);
}

UE_AUTORTFM_FORCEINLINE void FTransaction::DidFree(void* LogicalAddress)
{
    ASSERT(bTrackAllocationLocations);
    
    // Checking if one byte is in the interval map is enough to ascertain if it
    // is new memory and we should be worried.
    ASSERT(!NewMemoryTracker.Contains(LogicalAddress, 1));
}

UE_AUTORTFM_FORCEINLINE void FTransaction::DeferUntilCommit(TFunction<void()>&& Callback)
{
	// We explicitly must copy the function here because the original was allocated
	// within a transactional context, and thus the memory is allocating under
	// transactionalized conditions. By copying, we create an open copy of the callback.
	TFunction<void()> Copy(Callback);
    CommitTasks.Add(MoveTemp(Copy));
}

UE_AUTORTFM_FORCEINLINE void FTransaction::DeferUntilAbort(TFunction<void()>&& Callback)
{
	// We explicitly must copy the function here because the original was allocated
	// within a transactional context, and thus the memory is allocating under
	// transactionalized conditions. By copying, we create an open copy of the callback.
	TFunction<void()> Copy(Callback);
    AbortTasks.Add(MoveTemp(Copy));
}

UE_AUTORTFM_FORCEINLINE void FTransaction::CollectStats() const
{
    Stats.Collect<EStatsKind::AverageWriteLogEntries>(WriteLog.Num());
    Stats.Collect<EStatsKind::MaximumWriteLogEntries>(WriteLog.Num());

    Stats.Collect<EStatsKind::AverageWriteLogBytes>(WriteLogBumpAllocator.StatTotalSize);
    Stats.Collect<EStatsKind::MaximumWriteLogBytes>(WriteLogBumpAllocator.StatTotalSize);

    Stats.Collect<EStatsKind::AverageCommitTasks>(CommitTasks.Num());
    Stats.Collect<EStatsKind::MaximumCommitTasks>(CommitTasks.Num());

    Stats.Collect<EStatsKind::AverageAbortTasks>(AbortTasks.Num());
    Stats.Collect<EStatsKind::MaximumAbortTasks>(AbortTasks.Num());

    Stats.Collect<EStatsKind::AverageHitSetSize>(HitSet.GetSize());
    Stats.Collect<EStatsKind::AverageHitSetCapacity>(HitSet.GetCapacity());
}

} // namespace AutoRTFM
