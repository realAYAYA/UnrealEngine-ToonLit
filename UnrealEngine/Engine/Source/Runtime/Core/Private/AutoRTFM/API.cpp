// Copyright Epic Games, Inc. All Rights Reserved.


#include "AutoRTFM/AutoRTFM.h"
#include "HAL/IConsoleManager.h"

#if UE_AUTORTFM
bool GAutoRTFMRuntimeEnabled = true;
static FAutoConsoleVariableRef CVarAutoRTFMRuntimeEnabled(
	TEXT("AutoRTFMRuntimeEnabled"),
	GAutoRTFMRuntimeEnabled,
	TEXT("Enables the AutoRTFM runtime"),
	ECVF_Default
);
#else
static constexpr bool GAutoRTFMRuntimeEnabled = false;
#endif

#if (defined(__AUTORTFM) && __AUTORTFM)
#include "AutoRTFM/AutoRTFMConstants.h"
#include "CallNest.h"
#include "Context.h"
#include "ContextInlines.h"
#include "ContextStatus.h"
#include "FunctionMapInlines.h"
#include "TransactionInlines.h"
#include "Utils.h"

#include "Templates/Tuple.h"

// This is the implementation of the AutoRTFM.h API. Ideally, functions here should just delegate to some internal API.
// For now, I have these functions also perform some error checking.

namespace AutoRTFM
{

UE_AUTORTFM_FORCEINLINE autortfm_result TransactThenOpenImpl(void (*Work)(void* Arg), void* Arg)
{
	return static_cast<autortfm_result>(
		AutoRTFM::Transact([&]
		{
			AutoRTFM::Open([&]
			{
				Work(Arg);
			});
		}));
}

FORCENOINLINE extern "C" bool autortfm_is_transactional()
{
	if (GAutoRTFMRuntimeEnabled)
	{
		return FContext::Get()->IsTransactional();
	}

	return false;
}

FORCENOINLINE extern "C" bool autortfm_is_closed()
{
    return false;
}

// First Part - the API exposed outside transactions.
FORCENOINLINE extern "C" autortfm_result autortfm_transact(void (*Work)(void* Arg), void* Arg)
{
	if (GAutoRTFMRuntimeEnabled)
	{
	    return static_cast<autortfm_result>(FContext::Get()->Transact(Work, Arg));
	}

	Work(Arg);
	return autortfm_committed;
}

FORCENOINLINE extern "C" autortfm_result autortfm_transact_then_open(void (*Work)(void* Arg), void* Arg)
{
    return TransactThenOpenImpl(Work, Arg);
}

FORCENOINLINE extern "C" void autortfm_commit(void (*Work)(void* Arg), void* Arg)
{
    autortfm_result Result = autortfm_transact(Work, Arg);
	UE_CLOG(Result != autortfm_committed, LogAutoRTFM, Fatal, TEXT("Unexpected transaction result: %u."), Result);
}

FORCENOINLINE extern "C" void autortfm_abort()
{
	UE_CLOG(!FContext::IsTransactional(), LogAutoRTFM, Fatal, TEXT("The function `autortfm_abort` was called from outside a transaction."));
	FContext::Get()->AbortByRequestAndThrow();
}

FORCENOINLINE extern "C" bool autortfm_start_transaction()
{
	UE_CLOG(!FContext::IsTransactional(), LogAutoRTFM, Fatal, TEXT("The function `autortfm_start_transaction` was called from outside a transact."));
	return FContext::Get()->StartTransaction();
}

FORCENOINLINE extern "C" autortfm_result autortfm_commit_transaction()
{
	UE_CLOG(!FContext::IsTransactional(), LogAutoRTFM, Fatal, TEXT("The function `autortfm_commit_transaction` was called from outside a transact."));
	return static_cast<autortfm_result>(FContext::Get()->CommitTransaction());
}

FORCENOINLINE extern "C" autortfm_result autortfm_abort_transaction()
{
	UE_CLOG(!FContext::IsTransactional(), LogAutoRTFM, Fatal, TEXT("The function `autortfm_abort_transaction` was called from outside a transact."));
	return static_cast<autortfm_result>(FContext::Get()->AbortTransaction(false));
}

FORCENOINLINE extern "C" void autortfm_clear_transaction_status()
{
	ASSERT(FContext::Get()->IsAborting());
	FContext::Get()->ClearTransactionStatus();
}

FORCENOINLINE extern "C" bool autortfm_is_aborting()
{
	if (GAutoRTFMRuntimeEnabled)
	{
		return FContext::Get()->IsAborting();
	}

	return false;
}

FORCENOINLINE extern "C" bool autortfm_current_nest_throw()
{
	FContext::Get()->Throw();
	return true;
}

FORCENOINLINE extern "C" void autortfm_abort_if_transactional()
{
	UE_CLOG(FContext::IsTransactional(), LogAutoRTFM, Fatal, TEXT("The function `autortfm_abort_if_transactional` was called from an open inside a transaction."));
}

FORCENOINLINE extern "C" void autortfm_abort_if_closed()
{
}

FORCENOINLINE extern "C" void autortfm_open(void (*Work)(void* Arg), void* Arg)
{
	Work(Arg);
}

FORCENOINLINE extern "C" autortfm_status autortfm_close(void (*Work)(void* Arg), void* Arg)
{
	autortfm_status Result = autortfm_status_ontrack;

	if (GAutoRTFMRuntimeEnabled)
	{
		UE_CLOG(!FContext::IsTransactional(), LogAutoRTFM, Fatal, TEXT("Close called from an outside a transaction."));

		FContext* Context = FContext::Get();
		void (*WorkClone)(void* Arg, FContext* Context) = FunctionMapLookup(Work, Context, "autortfm_close");
		if (WorkClone)
		{
			Result = static_cast<autortfm_status>(Context->CallClosedNest(WorkClone, Arg));
		}
	}
	else
	{
		Work(Arg);
	}

	return Result;
}

FORCENOINLINE extern "C" void autortfm_record_open_write(void* Ptr, size_t Size)
{
    FContext::Get()->CheckOpenRecordWrite(Ptr);
	FContext::Get()->RecordWrite(Ptr, Size);
}

FORCENOINLINE extern "C" void autortfm_register_open_function(void* OriginalFunction, void* NewFunction)
{
	UE_LOG(LogAutoRTFM, Verbose, TEXT("Registering open %p->%p"), OriginalFunction, NewFunction);
    FunctionMapAdd(OriginalFunction, NewFunction);
}

void OpenCommit(TFunction<void()>&& Work)
{
    Work();
}

void OpenAbort(TFunction<void()>&& Work)
{
}

FORCENOINLINE extern "C" void autortfm_open_commit(void (*Work)(void* Arg), void* Arg)
{
    Work(Arg);
}

FORCENOINLINE extern "C" void autortfm_open_abort(void (*Work)(void* arg), void* Arg)
{
}

FORCENOINLINE extern "C" void* autortfm_did_allocate(void* Ptr, size_t Size)
{
    return Ptr;
}

FORCENOINLINE extern "C" void autortfm_check_consistency_assuming_no_races()
{
    if (FContext::IsTransactional())
    {
        AutoRTFM::Unreachable();
    }
}

FORCENOINLINE extern "C" void autortfm_check_abi(void* const Ptr, const size_t Size)
{
    struct FConstants final
    {
        const size_t LogLineBytes = Constants::LogLineBytes;
        const size_t LineBytes = Constants::LineBytes;
        const size_t LineTableSize = Constants::LineTableSize;
        const size_t Offset_Context_CurrentTransaction = Constants::Offset_Context_CurrentTransaction;
        const size_t Offset_Context_LineTable = Constants::Offset_Context_LineTable;
        const size_t Offset_Context_Status = Constants::Offset_Context_Status;
        const size_t LogSize_LineEntry = Constants::LogSize_LineEntry;
        const size_t Size_LineEntry = Constants::Size_LineEntry;
        const size_t Offset_LineEntry_LogicalLine = Constants::Offset_LineEntry_LogicalLine;
        const size_t Offset_LineEntry_ActiveLine = Constants::Offset_LineEntry_ActiveLine;
        const size_t Offset_LineEntry_LoggingTransaction = Constants::Offset_LineEntry_LoggingTransaction;
        const size_t Offset_LineEntry_AccessMask = Constants::Offset_LineEntry_AccessMask;
        const uint32_t Context_Status_OnTrack = Constants::Context_Status_OnTrack;

		// This is messy - but we want to do comparisons but without comparing any padding bytes.
		// Before C++20 we cannot use a default created operator== and operator!=, so we use this
		// ugly trick to just compare the members.
	private:
		auto Tied() const
		{
			return Tie(LogLineBytes, LineBytes, LineTableSize, Offset_Context_CurrentTransaction, Offset_Context_LineTable, Offset_Context_Status, LogSize_LineEntry, Size_LineEntry, Offset_LineEntry_LogicalLine, Offset_LineEntry_ActiveLine, Offset_LineEntry_LoggingTransaction, Offset_LineEntry_AccessMask, Context_Status_OnTrack);
		}

	public:
		bool operator==(const FConstants& Other) const
		{
			return Tied() == Other.Tied();
		}

		bool operator!=(const FConstants& Other) const
		{
			return !(*this == Other);
		}
    } RuntimeConstants;

	UE_CLOG(sizeof(FConstants) != Size, LogAutoRTFM, Fatal, TEXT("ABI error between AutoRTFM compiler and runtime."));

    const FConstants* const CompilerConstants = static_cast<FConstants*>(Ptr);

	UE_CLOG(RuntimeConstants != *CompilerConstants, LogAutoRTFM, Fatal, TEXT("ABI error between AutoRTFM compiler and runtime."));
}

// Second Part - the same API exposed inside transactions. Note that we don't expose all of the API
// to transactions! That's intentional. However, things like autortfm_defer_until_commit can be called
// from an open nest in a transaction.
bool RTFM_autortfm_is_transactional(FContext* Context)
{
    return true;
}
UE_AUTORTFM_REGISTER_OPEN_FUNCTION(autortfm_is_transactional);

bool RTFM_autortfm_is_closed(FContext* Context)
{
    return true;
}
UE_AUTORTFM_REGISTER_OPEN_FUNCTION(autortfm_is_closed);

autortfm_result RTFM_autortfm_transact(void (*Work)(void* Arg), void* Arg, FContext* Context)
{
    return static_cast<autortfm_result>(Context->Transact(Work, Arg));
}
UE_AUTORTFM_REGISTER_OPEN_FUNCTION(autortfm_transact);

autortfm_result RTFM_autortfm_transact_then_open(void (*Work)(void* Arg), void* Arg, FContext*)
{
    return TransactThenOpenImpl(Work, Arg);
}
UE_AUTORTFM_REGISTER_OPEN_FUNCTION(autortfm_transact_then_open);

void RTFM_autortfm_commit(void (*Work)(void* Arg), void* Arg, FContext*)
{
    autortfm_result Result = autortfm_transact(Work, Arg);
	UE_CLOG(Result != autortfm_committed, LogAutoRTFM, Fatal, TEXT("Unexpected transaction result: %u."), Result);
}
UE_AUTORTFM_REGISTER_OPEN_FUNCTION(autortfm_commit);

void RTFM_autortfm_abort(FContext* Context)
{
    Context->AbortByRequestAndThrow();
}
UE_AUTORTFM_REGISTER_OPEN_FUNCTION(autortfm_abort);

void RTFM_autortfm_start_transaction(FContext* Context)
{
	UE_LOG(LogAutoRTFM, Fatal, TEXT("The function `autortfm_start_transaction` was called from closed code."));
}
UE_AUTORTFM_REGISTER_OPEN_FUNCTION(autortfm_start_transaction);

void RTFM_autortfm_commit_transaction(FContext* Context)
{
	UE_LOG(LogAutoRTFM, Fatal, TEXT("The function `RTFM_autortfm_commit_transaction` was called from closed code."));
}
UE_AUTORTFM_REGISTER_OPEN_FUNCTION(autortfm_commit_transaction);

autortfm_result RTFM_autortfm_abort_transaction(FContext* Context)
{
	UE_CLOG(!FContext::IsTransactional(), LogAutoRTFM, Fatal, TEXT("The function `autortfm_abort_transaction` was called from outside a transaction"));
	return static_cast<autortfm_result>(Context->AbortTransaction(true));
}
UE_AUTORTFM_REGISTER_OPEN_FUNCTION(autortfm_abort_transaction);

void RTFM_autortfm_clear_transaction_status(FContext* Context)
{
	UE_LOG(LogAutoRTFM, Fatal, TEXT("The function `autortfm_clear_transaction_status` was called from closed code."));
}

void RTFM_autortfm_abort_if_transactional(FContext* Context)
{
    UE_LOG(LogAutoRTFM, Verbose, TEXT("The function `autortfm_abort_if_transactional` was called from inside a transaction."));
    Context->AbortByRequestAndThrow();
}
UE_AUTORTFM_REGISTER_OPEN_FUNCTION(autortfm_abort_if_transactional);

void RTFM_autortfm_abort_if_closed(FContext* Context)
{
	UE_LOG(LogAutoRTFM, Verbose, TEXT("The function `autortfm_abort_if_closed` was called from closed inside a transaction."));
    Context->AbortByRequestAndThrow();
}
UE_AUTORTFM_REGISTER_OPEN_FUNCTION(autortfm_abort_if_closed);

void RTFM_autortfm_open(void (*Work)(void* Arg), void* Arg, FContext* Context)
{
	Work(Arg);

	if (Context->IsAborting())
	{
		Context->Throw();
	}
}
UE_AUTORTFM_REGISTER_OPEN_FUNCTION(autortfm_open);

autortfm_status RTFM_autortfm_close(void (*Work)(void* Arg), void* Arg, FContext* Context)
{
    void (*WorkClone)(void* Arg, FContext* Context) = FunctionMapLookup(Work, Context, "RTFM_autortfm_close");
    if (WorkClone)
    {
        WorkClone(Arg, Context);
    }

	return static_cast<autortfm_status>(FContext::Get()->GetStatus());
}
UE_AUTORTFM_REGISTER_OPEN_FUNCTION(autortfm_close);

extern "C" void RTFM_autortfm_record_open_write(void*, size_t, FContext*)
{
	UE_LOG(LogAutoRTFM, Fatal, TEXT("The function `autortfm_record_open_write` was called from closed code."));
}
UE_AUTORTFM_REGISTER_OPEN_FUNCTION(autortfm_record_open_write);

void RTFM_OpenCommit(TFunction<void()>&& Work, FContext* Context)
{
    ASSERT(Context->GetStatus() == EContextStatus::OnTrack);
    Context->GetCurrentTransaction()->DeferUntilCommit(MoveTemp(Work));
}
UE_AUTORTFM_REGISTER_OPEN_FUNCTION(OpenCommit);

void RTFM_OpenAbort(TFunction<void()>&& Work, FContext* Context)
{
    ASSERT(Context->GetStatus() == EContextStatus::OnTrack);
    Context->GetCurrentTransaction()->DeferUntilAbort(MoveTemp(Work));
}
UE_AUTORTFM_REGISTER_OPEN_FUNCTION(OpenAbort);

extern "C" void RTFM_autortfm_open_commit(void (*Work)(void* Arg), void* Arg, FContext* Context)
{
    RTFM_OpenCommit([Work, Arg] { Work(Arg); }, Context);
}
UE_AUTORTFM_REGISTER_OPEN_FUNCTION(autortfm_open_commit);

extern "C" void RTFM_autortfm_open_abort(void (*Work)(void* arg), void* Arg, FContext* Context)
{
    RTFM_OpenAbort([Work, Arg] { Work(Arg); }, Context);
}
UE_AUTORTFM_REGISTER_OPEN_FUNCTION(autortfm_open_abort);

void* RTFM_autortfm_did_allocate(void* Ptr, size_t Size, FContext* Context)
{
    Context->DidAllocate(Ptr, Size);
    return Ptr;
}
UE_AUTORTFM_REGISTER_OPEN_FUNCTION(autortfm_did_allocate);

void RTFM_autortfm_check_consistency_assuming_no_races(FContext* Context)
{
}
UE_AUTORTFM_REGISTER_OPEN_FUNCTION(autortfm_check_consistency_assuming_no_races);

} // namespace AutoRTFM

#endif // defined(__AUTORTFM) && __AUTORTFM
