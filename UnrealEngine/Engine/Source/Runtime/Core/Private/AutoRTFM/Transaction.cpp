// Copyright Epic Games, Inc. All Rights Reserved.

#if (defined(__AUTORTFM) && __AUTORTFM)
#include "Transaction.h"
#include "TransactionInlines.h"
#include "CallNestInlines.h"
#include "GlobalData.h"

namespace AutoRTFM
{

FTransaction::FTransaction(FContext* Context)
    : Context(Context)
{
}

bool FTransaction::IsFresh() const
{
    return HitSet.IsEmpty()
        && NewMemoryTracker.IsEmpty()
        && WriteLog.IsEmpty()
        && CommitTasks.IsEmpty()
        && AbortTasks.IsEmpty()
        && !bIsDone;
}

void FTransaction::AbortWithoutThrowing()
{
	UE_LOG(LogAutoRTFM, Verbose, TEXT("Aborting '%hs'!"), GetContextStatusName(Context->GetStatus()));

    ASSERT(Context->IsAborting());
    ASSERT(Context->GetCurrentTransaction() == this);

    Stats.Collect<EStatsKind::Abort>();
    CollectStats();

    if (IsNested())
    {
        AbortNested();
    }
    else
    {
        AbortOuterNest();
    }
    Reset();
}

void FTransaction::AbortAndThrow()
{
    AbortWithoutThrowing();
	Context->Throw();
}

bool FTransaction::AttemptToCommit()
{
    ASSERT(Context->GetStatus() == EContextStatus::Committing);
    ASSERT(Context->GetCurrentTransaction() == this);

    Stats.Collect<EStatsKind::Commit>();
    CollectStats();

    bool bResult;
    if (IsNested())
    {
        CommitNested();
        bResult = true;
    }
    else
    {
        bResult = AttemptToCommitOuterNest();
    }
    Reset();
    return bResult;
}

void FTransaction::Undo()
{
	UE_LOG(LogAutoRTFM, Verbose, TEXT("Undoing a transaction..."));

	int VerboseCounter = 0;
	int Num = WriteLog.Num();
	for(auto Iter = WriteLog.rbegin(); Iter != WriteLog.rend(); ++Iter)
    {
		FWriteLogEntry& Entry = *Iter;

        // Skip writes to our current transaction nest if we're scoped. We're about to
		// leave so the changes don't matter. 
        if (IsScopedTransaction() && Context->IsInnerTransactionStack(Entry.OriginalAndSize.Get()))
        {
            continue;
        }

        void* const Original = Entry.OriginalAndSize.Get();
        const size_t Size = Entry.OriginalAndSize.GetTopTag();
        void* const Copy = Entry.Copy;

		if (UE_LOG_ACTIVE(LogAutoRTFM, Verbose))
		{
			TStringBuilder<1024> Builder;

			Builder.Appendf(TEXT("%4d [UNDO] %p %4llu : [ "), Num - VerboseCounter - 1, Original, Size);

			unsigned char* Current = (unsigned char*)Original;
			unsigned char* Old = (unsigned char*)Copy;

			for (size_t i = 0; i < Size; i++)
			{
				Builder.Appendf(TEXT("%02X "), Current[i]);
			}

			Builder << TEXT("] -> [ ");

			for (size_t i = 0; i < Size; i++)
			{
				Builder.Appendf(TEXT("%02X "), Old[i]);
			}

			Builder << TEXT("]");

			UE_LOG(LogAutoRTFM, Verbose, TEXT("%s"), Builder.ToString());
		}

        memcpy(Original, Copy, Size);

		VerboseCounter++;
    }

	UE_LOG(LogAutoRTFM, Verbose, TEXT("Undone a transaction!"));
}

void FTransaction::AbortNested()
{
    ASSERT(Parent);

    Undo();

	// We need to add the abort tasks in reverse order to the parent, as they need to be run in the reverse order.
	AbortTasks.ForEachBackward([&](const TFunction<void()>& Task) -> bool { Parent->CommitTasks.Add(Task); return true; });

    Parent->AbortTasks.AddAll(MoveTemp(AbortTasks));
}

void FTransaction::AbortOuterNest()
{
    Undo();

    AbortTasks.ForEachBackward([] (const TFunction<void()>& Task) -> bool { Task(); return true; });

	ASSERT(Context->IsAborting());
}

void FTransaction::CommitNested()
{
    ASSERT(Parent);

    // We need to pass our write log to our parent transaction, but with care!
    // We need to discard any writes to locations within the stack of our
    // current transaction, which could be placed there if a child of the
    // current transaction had written to stack local memory in the parent.

    for (FWriteLogEntry& Write : WriteLog)
    {
        // Skip writes that are into our current transactions stack.
        if (IsScopedTransaction() && Context->IsInnerTransactionStack(Write.OriginalAndSize.Get()))
        {
            continue;
        }

        Parent->WriteLog.Push(Write);
        Parent->HitSet.Insert(Write.OriginalAndSize);
    }

    Parent->WriteLogBumpAllocator.Merge(MoveTemp(WriteLogBumpAllocator));

    Parent->CommitTasks.AddAll(MoveTemp(CommitTasks));
    Parent->AbortTasks.AddAll(MoveTemp(AbortTasks));

    Parent->NewMemoryTracker.Merge(NewMemoryTracker);
}

bool FTransaction::AttemptToCommitOuterNest()
{
    ASSERT(!Parent);

	UE_LOG(LogAutoRTFM, Verbose, TEXT("About to run commit tasks!"));
	Context->DumpState();
	UE_LOG(LogAutoRTFM, Verbose, TEXT("Running commit tasks..."));

    CommitTasks.ForEachForward([] (const TFunction<void()>& Task) -> bool { Task(); return true; });

    return true;
}

void FTransaction::Reset()
{
    CommitTasks.Reset();
    AbortTasks.Reset();
	HitSet.Reset();
    NewMemoryTracker.Reset();
	WriteLog.Reset();
	WriteLogBumpAllocator.Reset();
}

} // namespace AutoRTFM

#endif // defined(__AUTORTFM) && __AUTORTFM
