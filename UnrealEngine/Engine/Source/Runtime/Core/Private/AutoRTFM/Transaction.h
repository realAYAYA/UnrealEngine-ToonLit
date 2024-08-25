// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HitSet.h"
#include "IntervalTree.h"
#include "LongJump.h"
#include "Stats.h"
#include "TaggedPtr.h"
#include "TaskArray.h"
#include "Templates/Function.h"
#include "WriteLog.h"
#include "WriteLogBumpAllocator.h"

namespace AutoRTFM
{
class FContext;

class FTransaction final
{
public:
    FTransaction(FContext* Context);
    
    bool IsNested() const { return !!Parent; }
    FTransaction* GetParent() const { return Parent; }

	void SetParent(FTransaction* NewParent)
    {
        Parent = NewParent;

        // For stats, record the nested depth of the transaction.
		if (NewParent)
		{
			StatDepth = NewParent->StatDepth + 1;
		}

		Stats.Collect<EStatsKind::AverageTransactionDepth>(StatDepth);
		Stats.Collect<EStatsKind::MaximumTransactionDepth>(StatDepth);
    }

    // This should just use type displays or ranges. Maybe ranges could even work out great.
    bool IsNestedWithin(const FTransaction* Other) const
    {
        for (const FTransaction* Current = this; ; Current = Current->Parent)
        {
            if (!Current)
            {
                return false;
            }
            else if (Current == Other)
            {
                return true;
            }
        }
    }

    bool IsFresh() const;
    bool IsDone() const { return bIsDone; }
    void SetIsDone() { bIsDone = true; }
    
	inline bool IsScopedTransaction() const { return bIsStackScoped; }
	inline void SetIsScopedTransaction() { bIsStackScoped = true; }

    void DeferUntilCommit(TFunction<void()>&&);
    void DeferUntilAbort(TFunction<void()>&&);

    void AbortAndThrow();
    void AbortWithoutThrowing();
    bool AttemptToCommit();

    // Record that a write is about to occur at the given LogicalAddress of Size bytes.
    void RecordWrite(void* LogicalAddress, size_t Size);
    void RecordWriteMaxPageSized(void* LogicalAddress, size_t Size);
    template<unsigned SIZE> void RecordWrite(void* LogicalAddress);

    void DidAllocate(void* LogicalAddress, size_t Size);
    void DidFree(void* LogicalAddress);

private:
    void Undo();
    void AbortNested();
    void AbortOuterNest();
    
    void CommitNested();
    bool AttemptToCommitOuterNest();

    void Reset(); // Frees memory and sets us up for possible retry.

    void CollectStats() const;
    
    FContext* Context;
    
    // Are we nested? Then this is the parent.
    FTransaction* Parent{nullptr};

    // Commit tasks run on commit in forward order. Abort tasks run on abort in reverse order.
    TTaskArray<TFunction<void()>> CommitTasks;
    TTaskArray<TFunction<void()>> AbortTasks;

    bool bIsDone{false};
	bool bIsStackScoped{false};

    FHitSet HitSet;
    FIntervalTree NewMemoryTracker;
    FWriteLog WriteLog;
    FWriteLogBumpAllocator WriteLogBumpAllocator;
    TStatStorage<uint64_t> StatDepth = 1;
};

} // namespace AutoRTFM
