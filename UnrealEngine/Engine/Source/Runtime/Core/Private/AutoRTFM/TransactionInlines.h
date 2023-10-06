// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Context.h"
#include "Transaction.h"

namespace AutoRTFM
{

UE_AUTORTFM_FORCEINLINE void FTransaction::RecordWriteMaxPageSized(void* LogicalAddress, size_t Size)
{
    void* CopyAddress = WriteLogBumpAllocator.Allocate(Size);
    memcpy(CopyAddress, LogicalAddress, Size);

    WriteLog.Push(FWriteLogEntry(LogicalAddress, Size, CopyAddress));
}

inline void FTransaction::RecordWrite(void* LogicalAddress, size_t Size)
{
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
    }

    Stats.Collect<EStatsKind::HitSetMiss>();

    uint8_t* const Address = reinterpret_cast<uint8_t*>(LogicalAddress);

    size_t I = 0;

    for (; (I + FWriteLogBumpAllocator::MaxSize) < Size; I += FWriteLogBumpAllocator::MaxSize)
    {
        RecordWriteMaxPageSized(Address + I, FWriteLogBumpAllocator::MaxSize);
    }

    // Remainder at the end of the memcpy.
    RecordWriteMaxPageSized(Address + I, Size - I);
}

inline void FTransaction::DidAllocate(void* LogicalAddress, size_t Size)
{
    if ((0 < Size) && (Size <= FWriteLogBumpAllocator::MaxSize))
    {
        FMemoryLocation Key(LogicalAddress);
        Key.SetTopTag(static_cast<uint16_t>(Size));
        // Otherwise we need to record the write.
        const bool DidInsert = HitSet.Insert(Key);
        ASSERT(DidInsert); 
    }
}

inline void FTransaction::DeferUntilCommit(TFunction<void()>&& Callback)
{
    CommitTasks.Add(MoveTemp(Callback));
}

inline void FTransaction::DeferUntilAbort(TFunction<void()>&& Callback)
{
    AbortTasks.Add(MoveTemp(Callback));
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
}

} // namespace AutoRTFM
