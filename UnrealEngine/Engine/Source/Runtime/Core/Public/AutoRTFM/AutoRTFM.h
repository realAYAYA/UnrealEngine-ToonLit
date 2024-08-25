// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if (defined(__AUTORTFM) && __AUTORTFM)
#define UE_AUTORTFM 1
#else
#define UE_AUTORTFM 0
#endif

#if UE_AUTORTFM
#define UE_AUTORTFM_AUTORTFM(F) [[clang::autortfm(F), clang::noinline]]
#define UE_AUTORTFM_NOAUTORTFM [[clang::noautortfm, clang::noinline]]
#else
#define UE_AUTORTFM_AUTORTFM(F)
#define UE_AUTORTFM_NOAUTORTFM
#endif

#if defined(UE_AUTORTFM_STANDALONE)
#include <stdlib.h>
#include <memory.h>
#include <type_traits>
#define UE_AUTORTFM_API
#define UE_AUTORTFM_FORCEINLINE inline
#define UE_AUTORTFM_MEMCPY ::memcpy
#else
#include <HAL/Platform.h>
#include <HAL/PlatformMemory.h>
#define UE_AUTORTFM_API CORE_API
#define UE_AUTORTFM_FORCEINLINE FORCEINLINE
#define UE_AUTORTFM_MEMCPY FPlatformMemory::Memcpy
#endif

#if UE_AUTORTFM
template <typename FuncType> class TFunction;
#endif

#define UE_AUTORTFM_UNUSED(UNUSEDVAR) (void)UNUSEDVAR

#ifdef __cplusplus
extern "C"
{
#endif

// The C API exists for a few reasons:
//
// - It makes linking easy. AutoRTFM has to deal with a weird kind of linking
//   where the compiler directly emits calls to functions with a given name.
//   It's easiest to do that in llvm if the functions have C linkage and C ABI.
// - It makes testing easy. Even seemingly simple C++ code introduces pitfalls
//   for AutoRTFM. So very focused tests work best when written in C.
// - It makes compiler optimizations much easier as there is no mangling to
// 	 consider when looking for functions in the runtime we can optimize.
// 
// We use snake_case for C API surface area to make it easy to distinguish.
//
// The C API should not be used directly - it is here purely as an
// implementation detail.

// This must match AutoRTFM::ETransactionResult.
typedef enum
{
    autortfm_aborted_by_request = 0,
    autortfm_aborted_by_language,
    autortfm_committed,
	autortfm_aborted_by_transact_in_on_commit,
	autortfm_aborted_by_transact_in_on_abort,
	autortfm_aborted_by_cascade,
	autortfm_aborted_by_transact_in_open_commit [[deprecated("Use autortfm_aborted_by_transact_in_on_commit instead.")]] = autortfm_aborted_by_transact_in_on_commit,
	autortfm_aborted_by_transact_in_open_abort [[deprecated("Use autortfm_aborted_by_transact_in_on_abort instead.")]] = autortfm_aborted_by_transact_in_on_abort,
} autortfm_result;

// This must match AutoRTFM::EContextStatus.
typedef enum
{
	autortfm_status_idle,
	autortfm_status_ontrack,
	autortfm_status_aborted_by_failed_lock_aquisition,
	autortfm_status_aborted_by_language,
	autortfm_status_aborted_by_request,
	autortfm_status_committing,
	autortfm_status_aborted_by_cascade
} autortfm_status;

#if UE_AUTORTFM
UE_AUTORTFM_API bool autortfm_is_transactional(void);
#else
UE_AUTORTFM_FORCEINLINE bool autortfm_is_transactional(void)
{
    return false;
}
#endif

#if UE_AUTORTFM
UE_AUTORTFM_API bool autortfm_is_closed(void);
#else
UE_AUTORTFM_FORCEINLINE bool autortfm_is_closed(void)
{
    return false;
}
#endif

#if UE_AUTORTFM
UE_AUTORTFM_API autortfm_result autortfm_transact(void (*work)(void* arg), void* arg);
#else
UE_AUTORTFM_FORCEINLINE autortfm_result autortfm_transact(void (*work)(void* arg), void* arg)
{
	work(arg);
	return autortfm_committed;
}
#endif

#if UE_AUTORTFM
UE_AUTORTFM_API autortfm_result autortfm_transact_then_open(void (*work)(void* arg), void* arg);
#else
UE_AUTORTFM_FORCEINLINE autortfm_result autortfm_transact_then_open(void (*work)(void* arg), void* arg)
{
	work(arg);
    return autortfm_committed;
}
#endif

#if UE_AUTORTFM
UE_AUTORTFM_API void autortfm_commit(void (*work)(void* arg), void* arg);
#else
UE_AUTORTFM_FORCEINLINE void autortfm_commit(void (*work)(void* arg), void* arg)
{
	UE_AUTORTFM_UNUSED(work);
	UE_AUTORTFM_UNUSED(arg);
    abort();
}
#endif

#if UE_AUTORTFM
UE_AUTORTFM_API autortfm_result autortfm_abort_transaction();
#else
UE_AUTORTFM_FORCEINLINE autortfm_result autortfm_abort_transaction() { return autortfm_aborted_by_request; }
#endif

#if UE_AUTORTFM
UE_AUTORTFM_API autortfm_result autortfm_cascading_abort_transaction();
#else
UE_AUTORTFM_FORCEINLINE autortfm_result autortfm_cascading_abort_transaction() { return autortfm_aborted_by_cascade; }
#endif

#if UE_AUTORTFM
UE_AUTORTFM_API bool autortfm_start_transaction();
#else
UE_AUTORTFM_FORCEINLINE bool autortfm_start_transaction() { return false; }
#endif

#if UE_AUTORTFM
UE_AUTORTFM_API autortfm_result autortfm_commit_transaction();
#else
UE_AUTORTFM_FORCEINLINE autortfm_result autortfm_commit_transaction() { return autortfm_aborted_by_language; }
#endif

#if UE_AUTORTFM
UE_AUTORTFM_API void autortfm_clear_transaction_status();
#else
UE_AUTORTFM_FORCEINLINE void autortfm_clear_transaction_status() {}
#endif

#if UE_AUTORTFM
UE_AUTORTFM_API void autortfm_abort_if_transactional(void);
#else
UE_AUTORTFM_FORCEINLINE void autortfm_abort_if_transactional(void) { }
#endif

#if UE_AUTORTFM
void autortfm_abort_if_closed(void);
#else
UE_AUTORTFM_FORCEINLINE void autortfm_abort_if_closed(void) { }
#endif

#if UE_AUTORTFM
UE_AUTORTFM_API void autortfm_open(void (*work)(void* arg), void* arg);
#else
UE_AUTORTFM_FORCEINLINE void autortfm_open(void (*work)(void* arg), void* arg) { work(arg); }
#endif

#if UE_AUTORTFM
[[nodiscard]] UE_AUTORTFM_API autortfm_status autortfm_close(void (*work)(void* arg), void* arg);
#else
PRAGMA_DISABLE_UNREACHABLE_CODE_WARNINGS
[[nodiscard]] UE_AUTORTFM_FORCEINLINE autortfm_status autortfm_close(void (*work)(void* arg), void* arg)
{
	UE_AUTORTFM_UNUSED(work);
	UE_AUTORTFM_UNUSED(arg);
    abort();
	return autortfm_status_aborted_by_language;
}
PRAGMA_RESTORE_UNREACHABLE_CODE_WARNINGS
#endif

#if UE_AUTORTFM
UE_AUTORTFM_API void autortfm_record_open_write(void* Ptr, size_t Size);
#else
UE_AUTORTFM_FORCEINLINE void autortfm_record_open_write(void* Ptr, size_t Size)
{
	UE_AUTORTFM_UNUSED(Ptr);
	UE_AUTORTFM_UNUSED(Size);
}
#endif

#if UE_AUTORTFM
UE_AUTORTFM_API void autortfm_register_open_function(void* original_function, void* new_function);
#else
UE_AUTORTFM_FORCEINLINE void autortfm_register_open_function(void* original_function, void* new_function) 
{ 
	UE_AUTORTFM_UNUSED(original_function);
	UE_AUTORTFM_UNUSED(new_function);
}
#endif

#if UE_AUTORTFM
UE_AUTORTFM_API void autortfm_on_commit(void (*work)(void* arg), void* arg);
#else
UE_AUTORTFM_FORCEINLINE void autortfm_on_commit(void (*work)(void* arg), void* arg)
{
    work(arg);
}
#endif

[[deprecated("Use autortfm_on_commit instead.")]]
UE_AUTORTFM_FORCEINLINE void autortfm_open_commit(void (*work)(void* arg), void* arg)
{
	autortfm_on_commit(work, arg);
}

#if UE_AUTORTFM
UE_AUTORTFM_API void autortfm_on_abort(void (*work)(void* arg), void* arg);
#else
UE_AUTORTFM_FORCEINLINE void autortfm_on_abort(void (*work)(void* arg), void* arg)
{
	UE_AUTORTFM_UNUSED(work);
	UE_AUTORTFM_UNUSED(arg);
}
#endif

[[deprecated("Use autortfm_on_abort instead.")]]
UE_AUTORTFM_FORCEINLINE void autortfm_open_abort(void (*work)(void* arg), void* arg)
{
	autortfm_on_abort(work, arg);
}

#if UE_AUTORTFM
UE_AUTORTFM_API void* autortfm_did_allocate(void* ptr, size_t size);
#else
UE_AUTORTFM_FORCEINLINE void* autortfm_did_allocate(void* ptr, size_t size)
{
	UE_AUTORTFM_UNUSED(size);
    return ptr;
}
#endif

#if UE_AUTORTFM
UE_AUTORTFM_API void autortfm_did_free(void* ptr);
#else
UE_AUTORTFM_FORCEINLINE void autortfm_did_free(void* ptr)
{
	UE_AUTORTFM_UNUSED(ptr);
}
#endif

#if UE_AUTORTFM
UE_AUTORTFM_API void autortfm_check_consistency_assuming_no_races(void);
#else
UE_AUTORTFM_FORCEINLINE void autortfm_check_consistency_assuming_no_races(void) { }
#endif

// If running with AutoRTFM enabled, then perform an ABI check between the
// AutoRTFM compiler and the AutoRTFM runtime, to ensure that memory is being
// laid out in an identical manner between the AutoRTFM runtime and the AutoRTFM
// compiler pass. Should not be called manually by the user, a call to this will
// be injected by the compiler into a global constructor in the AutoRTFM compiled
// code.
#if UE_AUTORTFM
UE_AUTORTFM_API void autortfm_check_abi(void* ptr, size_t size);
#else
UE_AUTORTFM_FORCEINLINE void autortfm_check_abi(void* ptr, size_t size)
{
	UE_AUTORTFM_UNUSED(ptr);
	UE_AUTORTFM_UNUSED(size);
}
#endif

#ifdef __cplusplus
}

namespace AutoRTFM
{

// The transaction result provides information on how a transaction completed. This is either Committed,
// or one of the various AbortedBy* variants to show why an abort occurred.
enum class ETransactionResult
{
	// The transaction aborted because of an explicit call to AbortTransaction.
    AbortedByRequest = autortfm_aborted_by_request,

	// The transaction aborted because of unhandled constructs in the code (atomics, unhandled function calls, etc).
    AbortedByLanguage = autortfm_aborted_by_language,

	// The transaction committed successfully. For a nested transaction this does not mean that the transaction effects
	// cannot be undone later if the parent transaction is aborted for any reason.
    Committed = autortfm_committed,

	// The transaction aborted because in a call to OnCommit, a new transaction nest was attempted which is not allowed.
	AbortedByTransactInOnCommit = autortfm_aborted_by_transact_in_on_commit,

	// The transaction aborted because in a call to OnAbort, a new transaction nest was attempted which is not allowed.
	AbortedByTransactInOnAbort = autortfm_aborted_by_transact_in_on_abort,

	// Deprecated use AbortedByTransactInOnCommit instead.
	AbortedByTransactInOpenCommit [[deprecated("Use AbortedByTransactInOnCommit instead.")]] = autortfm_aborted_by_transact_in_on_commit,

	// Deprecated use AbortedByTransactInOnAbort instead.
	AbortedByTransactInOpenAbort [[deprecated("Use AbortedByTransactInOnAbort instead.")]] = autortfm_aborted_by_transact_in_on_abort,

	// The transaction aborted because of an explicit call to CascadingAbortTransaction.
	AbortedByCascade = autortfm_aborted_by_cascade
};

// The context status shows what state the AutoRTFM context is currently in. 
enum class EContextStatus
{
	// An Idle status means we are not in transactional code.
	Idle = autortfm_status_idle,

	// An OnTrack status means we are in transactional code.
	OnTrack = autortfm_status_ontrack,

	// Reserved for a full STM future.
	AbortedByFailedLockAcquisition = autortfm_status_aborted_by_failed_lock_aquisition,

	// An AbortedByLanguage status means that we found some unhandled constructs in the code
	// (atomics, unhandled function calls, etc) and are currently aborting because of it.
	AbortedByLanguage = autortfm_status_aborted_by_language,

	// An AbortedByRequest status means that a call to AbortTransaction occurred and we are
	// currently aborting because of it.
	AbortedByRequest = autortfm_status_aborted_by_request,

	// A Committing status means we are currently attempting to commit a transaction.
	Committing = autortfm_status_committing,

	// An AbortedByCascade status means that a call to CascadingAbortTransaction occurred and
	// we are currently aborting because of it.
	AbortedByCascade = autortfm_status_aborted_by_cascade
};

// Tells if we are currently running in a transaction. This will return true in an
// open nest (see Open).
UE_AUTORTFM_FORCEINLINE bool IsTransactional() { return autortfm_is_transactional(); }

// Tells if we are currently running in the closed nest of a transaction. By
// default, transactional code is in a closed nest; the only way to be in an open
// nest is to request it via Open.
//
// The advantages of this function over IsTransactional are:
//
// - It's faster, the compiler will constant-fold this.
// - Usually, if you are doing special things for transactions, it's to work around
//   the transactional openation in a closed nest. So, it's often more correct
//   to test IsClosed than IsTransactional.
UE_AUTORTFM_FORCEINLINE bool IsClosed() { return autortfm_is_closed(); }

// Run the functor in a transaction. Memory writes and other side effects get instrumented
// and will be reversed if the transaction aborts.
// 
// If this begins a nested transaction, the instrumented effects are logged onto the root
// transaction, so the effects can be reversed later if the root transaction aborts, even
// if this nested transaction succeeds.
//
// If AutoRTFM is disabled, the code will be ran non-transactionally.
template<typename TFunctor>
UE_AUTORTFM_FORCEINLINE ETransactionResult Transact(const TFunctor& Functor)
{
    return static_cast<ETransactionResult>(autortfm_transact(
        [] (void* Arg) { (*static_cast<const TFunctor*>(Arg))(); },
        const_cast<void*>(static_cast<const void*>(&Functor))));
}

// This is just like calling Transact([&] { Open([&] { Functor(); }); });  
// The reason we expose it is that it allows the caller's module to not
// be compiled with the AutoRTFM instrumentation of functions if the only
// thing that's being invoked is a function in the open.
template<typename TFunctor>
UE_AUTORTFM_FORCEINLINE ETransactionResult TransactThenOpen(const TFunctor& Functor)
{
	return static_cast<ETransactionResult>(autortfm_transact_then_open(
		[] (void* Arg) { (*static_cast<const TFunctor*>(Arg))(); },
		const_cast<void*>(static_cast<const void*>(&Functor))));
}

// Run the callback in a transaction like Transact, but abort program
// execution if the result is anything other than autortfm_committed.
// Useful for testing.
template<typename TFunctor>
UE_AUTORTFM_FORCEINLINE void Commit(const TFunctor& Functor)
{
    autortfm_commit(
        [] (void* Arg) { (*static_cast<const TFunctor*>(Arg))(); },
        const_cast<void*>(static_cast<const void*>(&Functor)));
}

// End a transaction and discard all effects.
UE_AUTORTFM_FORCEINLINE ETransactionResult AbortTransaction()
{
	return static_cast<ETransactionResult>(autortfm_abort_transaction());
}

// End a transaction nest and discard all effects. This cascades, meaning
// an abort of a nested transaction will cause all transactions in the
// nest to abort.
UE_AUTORTFM_FORCEINLINE ETransactionResult CascadingAbortTransaction()
{
	return static_cast<ETransactionResult>(autortfm_cascading_abort_transaction());
}

// Abort if running in a transaction.
UE_AUTORTFM_FORCEINLINE void AbortIfTransactional()
{
    autortfm_abort_if_transactional();
}

// Abort if running in closed code.
UE_AUTORTFM_FORCEINLINE void AbortIfClosed()
{
    autortfm_abort_if_closed();
}

// Executes the given code non-transactionally regardless of whether we are in
// a transaction or not.
template<typename TFunctor> UE_AUTORTFM_FORCEINLINE void Open(const TFunctor& Functor)
{
    autortfm_open(
        [] (void* Arg) { (*static_cast<const TFunctor*>(Arg))(); },
        const_cast<void*>(static_cast<const void*>(&Functor)));
}

// Always executes the given code transactionally when called from a transaction nest
// (whether we are in open or closed code).
//
// Will crash if called outside of a transaction nest.
template<typename TFunctor> [[nodiscard]] UE_AUTORTFM_FORCEINLINE EContextStatus Close(const TFunctor& Functor)
{
    return static_cast<EContextStatus>(autortfm_close(
        [] (void* Arg) { (*static_cast<const TFunctor*>(Arg))(); },
        const_cast<void*>(static_cast<const void*>(&Functor))));
}

#if UE_AUTORTFM
// Have some work happen when this transaction commits. For nested transactions,
// this just adds the work to the work deferred until the outer nest's commit.
// If this is called outside a transaction or from an open nest then the work
// happens immediately.
UE_AUTORTFM_API void OnCommit(TFunction<void()>&& Work);
#else
// Have some work happen when this transaction commits. For nested transactions,
// this just adds the work to the work deferred until the outer nest's commit.
// If this is called outside a transaction or from an open nest then the work
// happens immediately.
template<typename TFunctor> UE_AUTORTFM_FORCEINLINE void OnCommit(const TFunctor& Work) { Work(); }
#endif

#if UE_AUTORTFM
// Have some work happen when this transaction aborts. If this is called
// outside a transaction or from an open nest then the work is ignored.
UE_AUTORTFM_API void OnAbort(TFunction<void()>&& Work);
#else
// Have some work happen when this transaction aborts. If this is called
// outside a transaction or from an open nest then the work is ignored.
template<typename TFunctor> UE_AUTORTFM_FORCEINLINE void OnAbort(const TFunctor&) {}
#endif

#if UE_AUTORTFM
[[deprecated("Use OnCommit instead.")]]
UE_AUTORTFM_API void OpenCommit(TFunction<void()>&& Work);
#else
template<typename TFunctor>
[[deprecated("Use OnCommit instead.")]]
UE_AUTORTFM_FORCEINLINE void OpenCommit(const TFunctor& Work) { Work(); }
#endif

#if UE_AUTORTFM
[[deprecated("Use OnAbort instead.")]]
UE_AUTORTFM_API void OpenAbort(TFunction<void()>&& Work);
#else
template<typename TFunctor>
[[deprecated("Use OnAbort instead.")]]
UE_AUTORTFM_FORCEINLINE void OpenAbort(const TFunctor& Work) {}
#endif

// Inform the runtime that we have performed a new object allocation. It's only
// necessary to call this inside of custom malloc implementations. As an
// optimization, you can choose to then only have your malloc return the pointer
// returned by this function. It's guaranteed to be equal to the pointer you
// passed, but it's blessed specially from the compiler's perspective, leading
// to some nice optimizations. This does nothing when called from open code.
UE_AUTORTFM_FORCEINLINE void* DidAllocate(void* Ptr, size_t Size)
{
    return autortfm_did_allocate(Ptr, Size);
}

// Inform the runtime that we have free'd a given memory location.
UE_AUTORTFM_FORCEINLINE void DidFree(void* Ptr)
{
    autortfm_did_free(Ptr);
}

// A collection of power-user functions that are reserved for use by the AutoRTFM runtime only.
namespace ForTheRuntime
{
	[[deprecated("This macro is deprecated. Use UE_AUTORTFM_ONABORT instead!")]] UE_AUTORTFM_FORCEINLINE void DeprecatedUseOnAbortMacro() {}
	[[deprecated("This macro is deprecated. Use UE_AUTORTFM_ONCOMMIT instead!")]] UE_AUTORTFM_FORCEINLINE void DeprecatedUseOnCommitMacro() {}

	// An enum to represent the various ways we want to enable/disable the AutoRTFM runtime.
	enum EAutoRTFMEnabledState
	{
		// Disable AutoRTFM.
		AutoRTFM_Disabled = 0,

		// Enable AutoRTFM.
		AutoRTFM_Enabled,

		// Force disable AutoRTFM - once set the AutoRTFM runtime cannot be re-enabled.
		AutoRTFM_ForcedDisabled,

		// Force enable AutoRTFM - once set the AutoRTFM runtime cannot be re-enabled.
		AutoRTFM_ForcedEnabled,
	};

	// Set whether the AutoRTFM runtime is enabled or disabled.
	UE_AUTORTFM_API bool SetAutoRTFMRuntime(EAutoRTFMEnabledState bEnabled);

	// Query whether the AutoRTFM runtime is enabled.
	UE_AUTORTFM_API bool IsAutoRTFMRuntimeEnabled();

	// Manually create a new transaction from open code and push it as a transaction nest.
	// Can only be called within an already active parent transaction (EG. this cannot start
	// a transaction nest itself).
	UE_AUTORTFM_FORCEINLINE bool StartTransaction()
	{
		return autortfm_start_transaction();
	}
	
	// Manually commit the top transaction nest, popping it from the execution scope.
	// Can only be called within an already active parent transaction (EG. this cannot end
	// a transaction nest itself).
	UE_AUTORTFM_FORCEINLINE ETransactionResult CommitTransaction()
	{
		return static_cast<ETransactionResult>(autortfm_commit_transaction());
	}

	// Manually clear the status of a user abort from the top transaction in a nest.
	UE_AUTORTFM_FORCEINLINE void ClearTransactionStatus()
	{
		autortfm_clear_transaction_status();
	}

	// Register a transactional version of a function that wasn't compiled by the
	// autortfm compiler. Normally, code is transactionalized by the compiler by
	// emitting a clone that has transactional openation, with some magic to
	// redirect all function calls within a transaction to the transactional clone.
	// This allows you to hook in your own transactionalized implementations of
	// functions that the compiler did not see.
	//
	// Use with great caution!
	//
	// This results in calls to ClosedVariant to happen in open mode. We will call
	// ClosedVariant's nontransactional version within the transaction. This happens
	// with the additional caveat that the function signatures must match.
	UE_AUTORTFM_FORCEINLINE void RegisterOpenFunction(void* const OpenFunction, void* const ClosedVariant)
	{
		autortfm_register_open_function(OpenFunction, ClosedVariant);
	}

	struct FRegisterOpenFunction
	{
		FRegisterOpenFunction(void* const OriginalFunction, void* const NewFunction)
		{
			RegisterOpenFunction(OriginalFunction, NewFunction);
		}
	};

	// Manually records that the memory span was written in the current transaction.
	UE_AUTORTFM_FORCEINLINE void RecordOpenWrite(void* Ptr, size_t Size)
	{
		autortfm_record_open_write(Ptr, Size);
	}

	// Manually records that the memory span was written in the current transaction.
	template<typename TTYPE> UE_AUTORTFM_FORCEINLINE void RecordOpenWrite(TTYPE* Ptr)
	{
		autortfm_record_open_write(Ptr, sizeof(TTYPE));
	}

	// Reserved for future.
	UE_AUTORTFM_FORCEINLINE void RecordOpenRead(void const*, size_t) {}

	// Reserved for future.
	template<typename TTYPE> UE_AUTORTFM_FORCEINLINE void RecordOpenRead(TTYPE*) {}

	// WriteMemory first records the memory span as written (see RecordOpenWrite) and then copies the specified value into it.
	UE_AUTORTFM_FORCEINLINE void WriteMemory(void* DestPtr, void const* SrcPtr, size_t Size)
	{
		RecordOpenWrite(DestPtr, Size);
		UE_AUTORTFM_MEMCPY(DestPtr, SrcPtr, Size);
	}

	// WriteMemory first records the memory span as written (see RecordOpenWrite) and then copies the specified value into it.
	template<typename TTYPE> UE_AUTORTFM_FORCEINLINE void WriteMemory(TTYPE* DestPtr, TTYPE const* SrcPtr)
	{
		if constexpr (std::is_trivially_copyable<TTYPE>::value)
		{
			RecordOpenWrite(DestPtr, sizeof(TTYPE));
			*DestPtr = *SrcPtr;
		}
		else
		{
			WriteMemory(DestPtr, SrcPtr, sizeof(TTYPE));
		}
	}

	// WriteMemory first records the memory span as written (see RecordOpenWrite) and then copies the specified value into it.
	template<typename TTYPE> UE_AUTORTFM_FORCEINLINE void WriteMemory(TTYPE* DestPtr, TTYPE const SrcValue)
	{
		if constexpr (std::is_trivially_copyable<TTYPE>::value)
		{
			RecordOpenWrite(DestPtr, sizeof(TTYPE));
			*DestPtr = SrcValue;
		}
		else
		{
			WriteMemory(DestPtr, &SrcValue, sizeof(TTYPE));
		}
	}

	// If running in a transaction, then perform a consistency check of the
	// transaction's read-write set. If possible, this compares the read-write set's
	// expected values with the actual values in global memory. Does nothing when
	// called outside of a transaction. May do nothing if debugging features aren't
	// enabled in the autortfm runtime.
	UE_AUTORTFM_FORCEINLINE void CheckConsistencyAssumingNoRaces()
	{
		autortfm_check_consistency_assuming_no_races();
	}

} // namespace ForTheRuntime

enum EAutoRTFMEnabledState
{
	AutoRTFM_Disabled = ForTheRuntime::AutoRTFM_Disabled,
	AutoRTFM_Enabled = ForTheRuntime::AutoRTFM_Enabled,
	AutoRTFM_ForcedDisabled = ForTheRuntime::AutoRTFM_ForcedDisabled,
};

// Deprecated use AutoRTFM::ForTheRuntime::SetAutoRTFMRuntime instead.
[[deprecated("Use AutoRTFM::ForTheRuntime::SetAutoRTFMRuntime instead.")]]
UE_AUTORTFM_FORCEINLINE bool SetAutoRTFMRuntime(EAutoRTFMEnabledState bEnabled) { return ForTheRuntime::SetAutoRTFMRuntime(static_cast<ForTheRuntime::EAutoRTFMEnabledState>(bEnabled)); }

// Deprecated use AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled instead.
[[deprecated("Use AutoRTFM::ForTheRuntime::IsAutoRTFMRuntimeEnabled instead.")]]
UE_AUTORTFM_FORCEINLINE bool IsAutoRTFMRuntimeEnabled() { return ForTheRuntime::IsAutoRTFMRuntimeEnabled(); }

// Deprecated use AutoRTFM::ForTheRuntime::StartTransaction instead.
[[deprecated("Use AutoRTFM::ForTheRuntime::StartTransaction instead.")]]
UE_AUTORTFM_FORCEINLINE bool StartTransaction() { return ForTheRuntime::StartTransaction(); }

// Deprecated use AutoRTFM::ForTheRuntime::CommitTransaction instead.
[[deprecated("Use AutoRTFM::ForTheRuntime::CommitTransaction instead.")]]
UE_AUTORTFM_FORCEINLINE ETransactionResult CommitTransaction() { return ForTheRuntime::CommitTransaction(); }

// Deprecated use AutoRTFM::ForTheRuntime::ClearTransactionStatus instead.
[[deprecated("Use AutoRTFM::ForTheRuntime::ClearTransactionStatus instead.")]]
UE_AUTORTFM_FORCEINLINE void ClearTransactionStatus() { return ForTheRuntime::ClearTransactionStatus(); }

// Deprecated use AutoRTFM::ForTheRuntime::RegisterOpenFunction instead.
[[deprecated("Use AutoRTFM::ForTheRuntime::RegisterOpenFunction instead.")]]
UE_AUTORTFM_FORCEINLINE void RegisterOpenFunction(void* OriginalFunction, void* NewFunction) { return ForTheRuntime::RegisterOpenFunction(OriginalFunction, NewFunction); }

// Deprecated use AutoRTFM::ForTheRuntime::RecordOpenWrite instead.
[[deprecated("Use AutoRTFM::ForTheRuntime::RecordOpenWrite instead.")]]
UE_AUTORTFM_FORCEINLINE void RecordOpenWrite(void* Ptr, size_t Size) { ForTheRuntime::RecordOpenWrite(Ptr, Size); }

// Deprecated use AutoRTFM::ForTheRuntime::RecordOpenWrite instead.
template<typename TTYPE> [[deprecated("Use AutoRTFM::ForTheRuntime::RecordOpenWrite instead.")]] UE_AUTORTFM_FORCEINLINE void RecordOpenWrite(TTYPE* Ptr) { return ForTheRuntime::RecordOpenWrite(Ptr); }

// Deprecated use AutoRTFM::ForTheRuntime::RecordOpenRead instead.
[[deprecated("Use AutoRTFM::ForTheRuntime::RecordOpenRead instead.")]]
UE_AUTORTFM_FORCEINLINE void RecordOpenRead(void const* Ptr, size_t Size) { return ForTheRuntime::RecordOpenRead(Ptr, Size); }

// Deprecated use AutoRTFM::ForTheRuntime::RecordOpenRead instead.
template<typename TTYPE> [[deprecated("Use AutoRTFM::ForTheRuntime::RecordOpenRead instead.")]] UE_AUTORTFM_FORCEINLINE void RecordOpenRead(TTYPE* Ptr) { return ForTheRuntime::RecordOpenRead(Ptr); }

// Deprecated use AutoRTFM::ForTheRuntime::WriteMemory instead.
[[deprecated("Use AutoRTFM::ForTheRuntime::WriteMemory instead.")]]
UE_AUTORTFM_FORCEINLINE void WriteMemory(void* D, void const* S, size_t Size) { return ForTheRuntime::WriteMemory(D, S, Size); }

// Deprecated use AutoRTFM::ForTheRuntime::WriteMemory instead.
template<typename TTYPE> [[deprecated("Use AutoRTFM::ForTheRuntime::WriteMemory instead.")]] UE_AUTORTFM_FORCEINLINE void WriteMemory(TTYPE* D, TTYPE const* S) { return ForTheRuntime::WriteMemory(D, S); }

// Deprecated use AutoRTFM::ForTheRuntime::WriteMemory instead.
template<typename TTYPE> [[deprecated("Use AutoRTFM::ForTheRuntime::WriteMemory instead.")]] UE_AUTORTFM_FORCEINLINE void WriteMemory(TTYPE* D, TTYPE const S) { return ForTheRuntime::WriteMemory(D, S); }

// Deprecated use AutoRTFM::ForTheRuntime::CheckConsistencyAssumingNoRaces instead.
[[deprecated("Use AutoRTFM::ForTheRuntime::CheckConsistencyAssumingNoRaces instead.")]]
UE_AUTORTFM_FORCEINLINE void CheckConsistencyAssumingNoRaces() { return ForTheRuntime::CheckConsistencyAssumingNoRaces(); }

} // namespace AutoRTFM

// Macro-based variants so we completely compile away when not in use, even in debug builds
#if UE_AUTORTFM

#if defined(__clang__) && __has_warning("-Wdeprecated-this-capture")
#define UE_AUTORTFM_BEGIN_DISABLE_WARNINGS _Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Wdeprecated-this-capture\"")
#define UE_AUTORTFM_END_DISABLE_WARNINGS _Pragma("clang diagnostic pop")
#else
#define UE_AUTORTFM_BEGIN_DISABLE_WARNINGS
#define UE_AUTORTFM_END_DISABLE_WARNINGS
#endif

#define UE_AUTORTFM_OPEN_IMPL(...) AutoRTFM::Open([&]() { __VA_ARGS__ })
#define UE_AUTORTFM_ONABORT_IMPL(...) UE_AUTORTFM_BEGIN_DISABLE_WARNINGS AutoRTFM::OnAbort([=]() { __VA_ARGS__ }) UE_AUTORTFM_END_DISABLE_WARNINGS
#define UE_AUTORTFM_ONCOMMIT_IMPL(...) UE_AUTORTFM_BEGIN_DISABLE_WARNINGS AutoRTFM::OnCommit([=]() { __VA_ARGS__ }) UE_AUTORTFM_END_DISABLE_WARNINGS
#define UE_AUTORTFM_TRANSACT_IMPL(...) AutoRTFM::Transact([&]() { __VA_ARGS__ })
#else
#define UE_AUTORTFM_OPEN_IMPL(...) do { __VA_ARGS__ } while (false)
#define UE_AUTORTFM_ONABORT_IMPL(...) do { /* do nothing */ } while (false)
#define UE_AUTORTFM_ONCOMMIT_IMPL(...) do { __VA_ARGS__ } while (false)
#define UE_AUTORTFM_TRANSACT_IMPL(...) do { __VA_ARGS__ } while (false)
#endif

// Runs a block of code in the open, non-transactionally. Anything performed in the open will not be undone if a transaction fails.
#define UE_AUTORTFM_OPEN(...) UE_AUTORTFM_OPEN_IMPL(__VA_ARGS__)

// Runs a block of code if a transaction aborts.
// In non-transactional code paths the block of code will not be executed at all.
// This captures any used variables from the parent function by-value.
#define UE_AUTORTFM_ONABORT(...) UE_AUTORTFM_ONABORT_IMPL(__VA_ARGS__)

// Runs a block of code if a transaction commits successfully.
// In non-transactional code paths the block of code will be executed immediately.
// This captures any used variables from the parent function by-value.
#define UE_AUTORTFM_ONCOMMIT(...) UE_AUTORTFM_ONCOMMIT_IMPL(__VA_ARGS__)

// Runs a block of code in the closed, transactionally, within a new transaction.
#define UE_AUTORTFM_TRANSACT(...) UE_AUTORTFM_TRANSACT_IMPL(__VA_ARGS__)

// Deprecated. Use UE_AUTORTFM_ONABORT instead.
#define UE_AUTORTFM_OPENABORT(...) UE_AUTORTFM_ONABORT(AutoRTFM::ForTheRuntime::DeprecatedUseOnAbortMacro(); __VA_ARGS__)

// Deprecated. Use UE_AUTORTFM_ONCOMMIT instead.
#define UE_AUTORTFM_OPENCOMMIT(...) UE_AUTORTFM_ONCOMMIT(AutoRTFM::ForTheRuntime::DeprecatedUseOnCommitMacro(); __VA_ARGS__)

#define UE_AUTORTFM_CONCAT_IMPL(A, B) A ## B
#define UE_AUTORTFM_CONCAT(A, B) UE_AUTORTFM_CONCAT_IMPL(A, B)

#if UE_AUTORTFM
#define UE_AUTORTFM_REGISTER_OPEN_FUNCTION_EXPLICIT_IMPL(OriginalFunction, NewFunction) static const ForTheRuntime::FRegisterOpenFunction UE_AUTORTFM_CONCAT(AutoRTFMFunctionRegistration, __COUNTER__)(reinterpret_cast<void*>(OriginalFunction), reinterpret_cast<void*>(NewFunction))
#define UE_AUTORTFM_REGISTER_OPEN_FUNCTION_IMPL(OriginalFunction) UE_AUTORTFM_REGISTER_OPEN_FUNCTION_EXPLICIT(OriginalFunction, RTFM_ ## OriginalFunction)
#define UE_AUTORTFM_REGISTER_SELF_FUNCTION_IMPL(OriginalFunction) UE_AUTORTFM_REGISTER_OPEN_FUNCTION_EXPLICIT(OriginalFunction, OriginalFunction)
#else
#define UE_AUTORTFM_REGISTER_OPEN_FUNCTION_EXPLICIT_IMPL(OriginalFunction, NewFunction)
#define UE_AUTORTFM_REGISTER_OPEN_FUNCTION_IMPL(OriginalFunction)
#define UE_AUTORTFM_REGISTER_SELF_FUNCTION_IMPL(OriginalFunction)
#endif

// Register that a specific OpenFunction maps to a closed variant when called in closed code.
#define UE_AUTORTFM_REGISTER_OPEN_FUNCTION_EXPLICIT(OpenFunction, ClosedVariant) UE_AUTORTFM_REGISTER_OPEN_FUNCTION_EXPLICIT_IMPL(OpenFunction, ClosedVariant)

// Tells the runtime that OpenFunction maps to an explicit closed variant with the "RTFM_" prefix.
#define UE_AUTORTFM_REGISTER_OPEN_FUNCTION(OpenFunction) UE_AUTORTFM_REGISTER_OPEN_FUNCTION_IMPL(OpenFunction)

// Tells the runtime that OpenFunction maps to itself in closed code (EG. it has no transactional semantics).
#define UE_AUTORTFM_REGISTER_SELF_FUNCTION(OpenFunction) UE_AUTORTFM_REGISTER_SELF_FUNCTION_IMPL(OpenFunction)

#endif // __cplusplus
