// Copyright Epic Games, Inc. All Rights Reserved.

#if (defined(__AUTORTFM) && __AUTORTFM)
#include "Context.h"
#include "CallNestInlines.h"
#include "ContextInlines.h"
#include "FunctionMap.h"
#include "GlobalData.h"
#include "ScopedGuard.h"
#include "Stats.h"
#include "TransactionInlines.h"

#include "Templates/UniquePtr.h"
#include "Containers/StringConv.h"

namespace AutoRTFM
{

thread_local TUniquePtr<FContext> ContextTls;

void FContext::InitializeGlobalData()
{
}

FContext* FContext::TryGet()
{
    return ContextTls.Get();
}

void FContext::Set()
{
    ContextTls.Reset(this);
}

FContext* FContext::Get()
{
    FContext* Result = TryGet();

    if (!Result)
    {
        Result = new FContext();
        Result->Set();
    }

    return Result;
}

bool FContext::IsTransactional()
{
    FContext* Context = TryGet();
    if (!Context)
    {
        return false;
    }

    if (Context->GetStatus() != EContextStatus::Idle)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool FContext::StartTransaction()
{
	FTransaction* NewTransaction = new FTransaction(this);

	void* TransactStackAddress = &NewTransaction; // get the stack-local position pointer.
	ASSERT(TransactStackAddress > StackBegin);
	ASSERT(TransactStackAddress < StackEnd);
	ASSERT(TransactStackAddress < CurrentTransactStackAddress);

	// This form of transaction is always ultimately within a scoped Transact 
	ASSERT(Status == EContextStatus::OnTrack);
	PushTransaction(NewTransaction);

	return true;
}

ETransactionResult FContext::CommitTransaction()
{
	constexpr bool bVerbose = false;

	ASSERT(Status == EContextStatus::OnTrack);

	// Scoped transactions commit on return, so committing explicitly isn't allowed
	ASSERT(CurrentTransaction->IsScopedTransaction() == false);

	ETransactionResult Result = ETransactionResult::Committed;

	if (CurrentTransaction->IsNested())
	{
		Result = ResolveNestedTransaction(CurrentTransaction);
	}
	else
	{
		UE_LOG(LogAutoRTFM, Verbose, TEXT("About to commit; my state is:"));
		DumpState();
		UE_LOG(LogAutoRTFM, Verbose, TEXT("Committing..."));

		if (CurrentTransaction->AttemptToCommit())
		{
			Result = ETransactionResult::Committed;
		}
		else
		{
			UE_LOG(LogAutoRTFM, Verbose, TEXT("Commit failed!"));
			ASSERT(Status != EContextStatus::OnTrack);
			ASSERT(Status != EContextStatus::Idle);
		}
	}

	// Parent transaction is now the current transaction
	PopTransaction();

	return Result;
}

ETransactionResult FContext::AbortTransaction(bool bIsClosed)
{
	ETransactionResult Result = ETransactionResult::AbortedByRequest;
	ASSERT(Status == EContextStatus::OnTrack);
	Status = EContextStatus::AbortedByRequest;

	ASSERT(CurrentTransaction != nullptr);

	// Sort out how aborts work
	CurrentTransaction->AbortWithoutThrowing();

	// Non-scoped transactions are ended immediately, but scoped need to get to the end scope before being popped
	if (!CurrentTransaction->IsScopedTransaction())
	{
		Result = ResolveNestedTransaction(CurrentTransaction);
		PopTransaction();
	}
	
	if (bIsClosed)
	{
		Throw();
	}

	return Result;
}

bool FContext::IsAborting() const
{
	return Status != EContextStatus::OnTrack && Status != EContextStatus::Idle;
}

EContextStatus FContext::CallClosedNest(void (*ClosedFunction)(void* Arg, FContext* Context), void* Arg)
{
	TScopedGuard<void*> CurrentNestStackAddressGuard(CurrentTransactStackAddress, &CurrentNestStackAddressGuard);

	PushCallNest(new FCallNest(this));

	CurrentNest->Try([&]() { ClosedFunction(Arg, this); });

	PopCallNest();

	return GetStatus();
}

void FContext::PushCallNest(FCallNest* NewCallNest)
{
	ASSERT(NewCallNest != nullptr);
	ASSERT(NewCallNest->Parent == nullptr);

	NewCallNest->Parent = CurrentNest;
	CurrentNest = NewCallNest;
}

void FContext::PopCallNest()
{
	ASSERT(CurrentNest != nullptr);
	FCallNest* OldCallNest = CurrentNest;
	CurrentNest = CurrentNest->Parent;

	delete OldCallNest;
}

void FContext::PushTransaction(FTransaction* NewTransaction)
{
	ASSERT(NewTransaction != nullptr);
	ASSERT(!NewTransaction->IsDone());
	ASSERT(NewTransaction->GetParent() == nullptr);

	ASSERT(CurrentTransaction == nullptr || !CurrentTransaction->IsDone());
	
	NewTransaction->SetParent(CurrentTransaction);
	CurrentTransaction = NewTransaction;

	// Collect stats that we've got a new transaction.
	Stats.Collect<EStatsKind::Transaction>();
}

void FContext::PopTransaction()
{
	ASSERT(CurrentTransaction != nullptr);
	ASSERT(CurrentTransaction->IsDone());
	FTransaction* OldTransaction = CurrentTransaction;
	CurrentTransaction = CurrentTransaction->GetParent();
	delete OldTransaction;
}

void FContext::ClearTransactionStatus()
{
	switch (Status)
	{
	case EContextStatus::OnTrack:
		break;
	case EContextStatus::AbortedByLanguage:
		Status = EContextStatus::OnTrack;
		break;
	case EContextStatus::AbortedByRequest:
		Status = EContextStatus::OnTrack;
		break;
	default:
		ASSERT(!"Should not be reached");
	}
}

ETransactionResult FContext::ResolveNestedTransaction(FTransaction* NewTransaction)
{
	// We just use this bit to help assertions for now (though we could use it more strongly). Because of how we use this right now,
	// it's OK that it's set before we commit but after we abort.
	ASSERT(!NewTransaction->IsDone());
	NewTransaction->SetIsDone();

	if (Status == EContextStatus::OnTrack)
	{
		bool bCommitResult = NewTransaction->AttemptToCommit();
		ASSERT(bCommitResult);
		ASSERT(Status == EContextStatus::OnTrack);
		return ETransactionResult::Committed;
	}

	switch (Status)
	{
	case EContextStatus::AbortedByRequest:
		return ETransactionResult::AbortedByRequest;
	case EContextStatus::AbortedByLanguage:
		return ETransactionResult::AbortedByLanguage;
	default:
		ASSERT(!"Should not be reached");
		return ETransactionResult::AbortedByLanguage;
	}
}

ETransactionResult FContext::Transact(void (*Function)(void* Arg), void* Arg)
{
    constexpr bool bVerbose = false;
    
    ASSERT(Status == EContextStatus::Idle || Status == EContextStatus::OnTrack);

    void (*ClonedFunction)(void* Arg, FContext* Context) = FunctionMapTryLookup(Function);
    if (!ClonedFunction)
    {
		UE_LOG(LogAutoRTFM, Warning, TEXT("Could not find function %p (%s) in AutoRTFM::FContext::Transact."), Function, *GetFunctionDescription(Function));
        return ETransactionResult::AbortedByLanguage;
    }
    
	//TUniquePtr<FTransaction> NewTransactionUniquePtr(new FTransaction(this));
	FTransaction* NewTransaction = new FTransaction(this);
	FCallNest* NewNest = new FCallNest(this);

	// Transact requires a return from the lambda to commit the results
	NewTransaction->SetIsScopedTransaction();

	void* TransactStackAddress = &NewTransaction;
	ASSERT(TransactStackAddress > StackBegin);
	ASSERT(TransactStackAddress < StackEnd);
	TScopedGuard<void*> CurrentNestStackAddressGuard(CurrentTransactStackAddress, TransactStackAddress);

	ETransactionResult Result = ETransactionResult::Committed; // Initialize to something to make the compiler happy.

    if (!CurrentTransaction)
    {
        ASSERT(Status == EContextStatus::Idle);

		PushTransaction(NewTransaction);
		PushCallNest(NewNest);
        OuterTransactStackAddress = TransactStackAddress;

        for (;;)
        {
            Status = EContextStatus::OnTrack;
            ASSERT(CurrentTransaction->IsFresh());
			CurrentNest->Try([&] () { ClonedFunction(Arg, this); });
			ASSERT(CurrentTransaction == NewTransaction); // The transaction lambda should have unwound any nested transactions.
            ASSERT(Status != EContextStatus::Idle);

            if (Status == EContextStatus::OnTrack)
            {
				UE_LOG(LogAutoRTFM, Verbose, TEXT("About to commit; my state is:"));
				DumpState();
				UE_LOG(LogAutoRTFM, Verbose, TEXT("Committing..."));

                if (CurrentTransaction->AttemptToCommit())
                {
                    Result = ETransactionResult::Committed;
                    break;
                }

				UE_LOG(LogAutoRTFM, Verbose, TEXT("Commit failed!"));

                ASSERT(Status != EContextStatus::OnTrack);
                ASSERT(Status != EContextStatus::Idle);
            }

            if (Status == EContextStatus::AbortedByRequest)
            {
                Result = ETransactionResult::AbortedByRequest;
                break;
            }

            if (Status == EContextStatus::AbortedByLanguage)
            {
                Result = ETransactionResult::AbortedByLanguage;
                break;
            }

            ASSERT(Status == EContextStatus::AbortedByFailedLockAcquisition);
        }

		NewTransaction->SetIsDone();

		PopCallNest();
		PopTransaction();
		ClearTransactionStatus();

		ASSERT(CurrentNest == nullptr);
		ASSERT(CurrentTransaction == nullptr);

        Reset();
	}
    else
    {
		// This transaction is within another transaction
		ASSERT(Status == EContextStatus::OnTrack);
		PushTransaction(NewTransaction);
		PushCallNest(NewNest);

		CurrentNest->Try([&]() { ClonedFunction(Arg, this); });
		ASSERT(CurrentTransaction == NewTransaction);

		Result = ResolveNestedTransaction(NewTransaction);
		
		PopCallNest();
		PopTransaction();
		ClearTransactionStatus();

		ASSERT(CurrentNest != nullptr);
		ASSERT(CurrentTransaction != nullptr);
	}

	return Result;
}

void FContext::AbortByRequestAndThrow()
{
    ASSERT(Status == EContextStatus::OnTrack);
    Status = EContextStatus::AbortedByRequest;
    CurrentTransaction->AbortAndThrow();
}

void FContext::AbortByRequestWithoutThrowing()
{
	ASSERT(Status == EContextStatus::OnTrack);
	Status = EContextStatus::AbortedByRequest;
	CurrentTransaction->AbortWithoutThrowing();
}

void FContext::AbortByLanguageAndThrow()
{
	UE_DEBUG_BREAK();
    ASSERT(Status == EContextStatus::OnTrack);
    Status = EContextStatus::AbortedByLanguage;
    CurrentTransaction->AbortAndThrow();
}

#if PLATFORM_WINDOWS
extern "C" __declspec(dllimport) void __stdcall GetCurrentThreadStackLimits(void**, void**);
#endif

FContext::FContext()
{
#if PLATFORM_WINDOWS
    GetCurrentThreadStackLimits(&StackBegin, &StackEnd);
#elif defined(__APPLE__)         
   StackEnd = pthread_get_stackaddr_np(pthread_self());   
   size_t StackSize = pthread_get_stacksize_np(pthread_self());    
   StackBegin = static_cast<char*>(StackEnd) - StackSize;
#else
    pthread_attr_t Attr;
    pthread_getattr_np(pthread_self(), &Attr);
    size_t StackSize;
    pthread_attr_getstack(&Attr, &StackBegin, &StackSize);
    StackEnd = static_cast<char*>(StackBegin) + StackSize;
#endif
    ASSERT(StackEnd > StackBegin);
}

void FContext::Reset()
{
    OuterTransactStackAddress = nullptr;
	CurrentTransactStackAddress = nullptr;
    CurrentTransaction = nullptr;
	CurrentNest = nullptr;
    Status = EContextStatus::Idle;
}

void FContext::Throw()
{
	GetCurrentNest()->AbortJump.Throw();
}

void FContext::DumpState() const
{
	UE_LOG(LogAutoRTFM, Verbose, TEXT("Context at %p, transaction stack: %p..%p."), this, StackBegin, OuterTransactStackAddress);
}

} // namespace AutoRTFM

#endif // defined(__AUTORTFM) && __AUTORTFM
