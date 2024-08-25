// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Async/Mutex.h"
#include "Async/UniqueLock.h"
#include "Containers/Array.h"
#include "Containers/Set.h"
#include "Experimental/Async/ConditionVariable.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"
#include "Templates/AlignmentTemplates.h"
#include "Templates/Function.h"
#include "Templates/TypeCompatibleBytes.h"
#include "VerseVM/VVMLog.h"

#include "VVMLocalAllocator.h"
#include "VVMMarkStack.h"
#include "VVMSubspace.h"
#include "pas_thread_local_cache_node_ue.h"
#include "verse_heap_config_ue.h"
#include "verse_heap_ue.h"
#include <array>
#include <atomic>
#include <cstddef>

namespace Verse
{
struct FConservativeStackEntryFrame;
struct FConservativeStackExitFrame;
struct FHandshakeContext;
struct FHardHandshakeContext;
struct FIOContext;
struct FRunningContext;
struct FStoppedWorld;
struct FThreadLocalContextHolder;
struct FTransaction;
struct VCell;
struct VValue;

template <typename T>
struct TWriteBarrier;

// The size of this must be matched to what is going on in FContextImpl::FContextImpl().
typedef std::array<FLocalAllocator, (416 >> VERSE_HEAP_MIN_ALIGN_SHIFT) + 1> FastAllocatorArray;

enum class EContextHeapRole
{
	Mutator,
	Collector
};

enum class EContextLifecycleState
{
	// The context is in FreeContexts and is not associated with any thread.
	Free,

	// The context is in LiveContexts, is associated with a thread, but that thread is not beneath either
	// FIOContext::Create() or FRunningContext::Create() - so from the user's perspective, this context
	// doesn't exist. We maintain the context-thread mapping to make libpas happy and because it's likely
	// to make other aspects of handshakes easier to implement.
	LiveButUnused,

	// The context is in LiveContexts, is associated with a thread, and that thread is beneath either
	// FIOContext::Create() or FRunningContext::Create() - so from the user's perspective, this context
	// really is live.
	LiveAndInUse
};

// One must have a FContext to talk to Verse VM objects on some thread. Each thread should only have one
// FContext.
struct FContextImpl
{
	typedef uint8 TState;

	COREUOBJECT_API void EnterConservativeStack(TFunctionRef<void()>);
	COREUOBJECT_API void ExitConservativeStack(TFunctionRef<void()>);

	bool InConservativeStack() const
	{
		if (TopEntryFrame)
		{
			if (TopExitFrame)
			{
				return reinterpret_cast<void*>(TopEntryFrame) < reinterpret_cast<void*>(TopExitFrame);
			}
			else
			{
				return true;
			}
		}
		else
		{
			check(!TopExitFrame);
			return false;
		}
	}

	void AcquireAccess()
	{
		// Changing whether or not you have access can only happen while not in conservative stack.
		// That's to ensure that changes to the conservative stack data structures can only happen
		// when you do have access, and changes to access can only happen when you don't have
		// conservative stack.
		check(!InConservativeStack());
		TState Expected = 0;
		if (!State.compare_exchange_weak(Expected, HasAccessBit, std::memory_order_acquire))
		{
			AcquireAccessSlow();
		}
	}

	void RelinquishAccess()
	{
		check(!InConservativeStack());
		TState Expected = HasAccessBit;
		if (!State.compare_exchange_weak(Expected, 0, std::memory_order_release))
		{
			RelinquishAccessSlow();
		}
	}

	void CheckForHandshake()
	{
		std::atomic_signal_fence(std::memory_order_seq_cst);
		if (State != HasAccessBit)
		{
			CheckForHandshakeSlow();
		}
	}

	COREUOBJECT_API void RequestStop();
	COREUOBJECT_API void WaitForStop();
	COREUOBJECT_API void CancelStop();

	COREUOBJECT_API void PairHandshake(FContextImpl* TargetContext, TFunctionRef<void(FHandshakeContext)> HandshakeAction);
	COREUOBJECT_API void SoftHandshake(TFunctionRef<void(FHandshakeContext)> HandshakeAction);
	COREUOBJECT_API void HardHandshake(TFunctionRef<void(FHardHandshakeContext)> HandshakeAction);

	COREUOBJECT_API FStoppedWorld StopTheWorld();

	bool IsLive() const
	{
		return LifecycleState != EContextLifecycleState::Free;
	}

	bool HasAccess() const
	{
		return State.load(std::memory_order_relaxed) & HasAccessBit;
	}
	bool IsHandshakeRequested() const
	{
		return State.load(std::memory_order_relaxed) & HandshakeRequestedBit;
	}
	bool IsStopRequested() const
	{
		return State.load(std::memory_order_relaxed) & StopRequestedBit;
	}

	void RunWriteBarrierNonNull(const VCell* Cell)
	{
		if (FHeap::IsMarking())
		{
			MarkStack.MarkNonNull(Cell);
		}
	}

	void RunWriteBarrier(VCell* Cell)
	{
		if (Cell)
		{
			RunWriteBarrierNonNull(Cell);
		}
	}

	void RunWriteBarrierNonNullDuringMarking(VCell* Cell)
	{
		checkSlow(FHeap::IsMarking());
		MarkStack.MarkNonNull(Cell);
	}

	void RunWriteBarrierDuringMarking(VCell* Cell)
	{
		checkSlow(FHeap::IsMarking());
		if (Cell)
		{
			MarkStack.MarkNonNull(Cell);
		}
	}

	VCell* RunWeakReadBarrier(VCell* Cell)
	{
		if (!Cell || FHeap::GetWeakBarrierState() == EWeakBarrierState::Inactive)
		{
			return Cell;
		}
		else
		{
			return RunWeakReadBarrierNonNullSlow(Cell);
		}
	}

	void RunAuxWriteBarrierNonNull(const void* Aux)
	{
		if (FHeap::IsMarking())
		{
			MarkStack.MarkAuxNonNull(Aux);
		}
	}

	void RunAuxWriteBarrier(void* Aux)
	{
		if (Aux)
		{
			RunAuxWriteBarrierNonNull(Aux);
		}
	}

	void RunAuxWriteBarrierNonNullDuringMarking(void* Aux)
	{
		checkSlow(FHeap::IsMarking());
		MarkStack.MarkAuxNonNull(Aux);
	}

	void RunAuxWriteBarrierDuringMarking(void* Aux)
	{
		checkSlow(FHeap::IsMarking());
		if (Aux)
		{
			MarkStack.MarkAuxNonNull(Aux);
		}
	}

	void* RunAuxWeakReadBarrier(void* Aux)
	{
		if (!Aux || FHeap::GetWeakBarrierState() == EWeakBarrierState::Inactive)
		{
			return Aux;
		}
		else
		{
			return RunAuxWeakReadBarrierNonNullSlow(Aux);
		}
	}

	template <typename T, typename MarkFunction>
	T* RunWeakReadBarrierUnmarkedWhenActive(T* Cell, MarkFunction&& MarkFunc)
	{
		using namespace UE;

		EWeakBarrierState WeakBarrierState = FHeap::GetWeakBarrierState();
		if (WeakBarrierState == EWeakBarrierState::Inactive)
		{
			return Cell;
		}

		if (WeakBarrierState == EWeakBarrierState::CheckMarkedOnRead)
		{
			return nullptr;
		}

		if (WeakBarrierState == EWeakBarrierState::MarkOnRead)
		{
			MarkFunc(Cell);
			return Cell;
		}

		V_DIE_UNLESS(WeakBarrierState == EWeakBarrierState::AttemptingToTerminate); // means that `AttemptToTerminate()` was called
		TUniqueLock Lock(FHeap::Mutex);
		WeakBarrierState = FHeap::GetWeakBarrierState();

		/*
		 * We can hit this TOCTOU race on the following conditions:
		 * - The GC is attempting to terminate. We've read the weak barrier state by this point, but haven't yet acquired the lock.
		 * - During the soft handshake with mutator threads, one of them ends up marking items and thus cancels termination. The weak barrier state is now inactive.
		 * - We then acquire the lock and read the barrier state which is inactive.
		 * In this case, we should be safe to return the cell because the GC is inactive and the memory should be safe to read from.
		 */
		if (WeakBarrierState == EWeakBarrierState::Inactive)
		{
			return Cell;
		}

		if (WeakBarrierState == EWeakBarrierState::CheckMarkedOnRead)
		{
			// This means that we've since terminated, so the object could have become marked.
			if (FHeap::IsMarked(Cell))
			{
				return Cell;
			}
			else
			{
				return nullptr;
			}
		}

		if (WeakBarrierState == EWeakBarrierState::MarkOnRead)
		{
			MarkFunc(Cell);
			return Cell;
		}

		V_DIE_UNLESS(WeakBarrierState == EWeakBarrierState::AttemptingToTerminate);
		FHeap::WeakBarrierState = EWeakBarrierState::MarkOnRead;
		MarkFunc(Cell);
		return Cell;
	}

	std::byte* AllocateFastCell(size_t NumBytes)
	{
		return AllocateFastCell_Internal(NumBytes, FastSpaceAllocators, FHeap::FastSpace);
	}

	std::byte* TryAllocateFastCell(size_t NumBytes)
	{
		return TryAllocateFastCell_Internal(NumBytes, FastSpaceAllocators, FHeap::FastSpace);
	}

	std::byte* AllocateAuxCell(size_t NumBytes)
	{
		return AllocateFastCell_Internal(NumBytes, AuxSpaceAllocators, FHeap::AuxSpace);
	}

	std::byte* TryAllocateAuxCell(size_t NumBytes)
	{
		return TryAllocateFastCell_Internal(NumBytes, AuxSpaceAllocators, FHeap::AuxSpace);
	}

	COREUOBJECT_API void StopAllocators();

	// If we enable manual stack scan, it stays enabled until we destroy the context.
	void EnableManualStackScanning()
	{
		bUsesManualStackScanning = true;
	}

	bool UsesManualStackScanning() const
	{
		return bUsesManualStackScanning;
	}

	bool ManualStackScanRequested() const
	{
		return bManualStackScanRequested;
	}

	bool IsInManuallyEmptyStack() const
	{
		return bIsInManuallyEmptyStack;
	}

	COREUOBJECT_API void SetIsInManuallyEmptyStack(bool bInIsInManuallyEmptyStack);

	COREUOBJECT_API void MarkReferencedCells();

	COREUOBJECT_API void ClearManualStackScanRequest();

	FMarkStack MarkStack;

	EContextHeapRole GetHeapRole() const
	{
		return HeapRole;
	}

	FTransaction* CurrentTransaction()
	{
		return _CurrentTransaction;
	}
	void SetCurrentTransaction(FTransaction* Transaction)
	{
		_CurrentTransaction = Transaction;
	}

private:
	friend struct FAccessContext;
	friend struct FAllocationContext;
	friend struct FIOContext;
	friend class FHeap;
	friend struct FRunningContext;
	friend struct FScopedThreadContext;
	friend struct FStoppedWorld;
	friend struct FThreadLocalContextHolder;
	friend struct TWriteBarrier<VValue>;

	std::byte* AllocateFastCell_Internal(size_t NumBytes, FastAllocatorArray& Allocators, FSubspace* Subspace)
	{
		// What's the point?
		//
		// This allows us to fold away two parts of an allocation that are expensive:
		//
		// 1) The size class lookup. The common case is that we're allocating something that has a compile-time known size. In that case,
		//    this computation happens at compile-time and we just directly access the right allocator (or call the slow path). Even if the
		//    size is not known at compile-time, this reduces the size class lookup to a shift.
		//
		// 2) The TLC lookup. Normally when allocating with libpas there's a TLC lookup using some OS TLS mechanism. This eliminates that
		//    lookup entirely because we're using FContextImpl as the TLC.
		//
		// In microbenchmarks, this gives a 2x speed-up on allocation throughput!

		size_t Index = (NumBytes + VERSE_HEAP_MIN_ALIGN - 1) >> VERSE_HEAP_MIN_ALIGN_SHIFT;
		std::byte* Result;
		if (Index >= Allocators.size())
		{
			Result = Subspace->Allocate(NumBytes);
		}
		else
		{
			Result = Allocators[Index].Allocate();
		}
		return Result;
	}

	std::byte* TryAllocateFastCell_Internal(size_t NumBytes, FastAllocatorArray& Allocators, FSubspace* Subspace)
	{
		size_t Index = (NumBytes + VERSE_HEAP_MIN_ALIGN - 1) >> VERSE_HEAP_MIN_ALIGN_SHIFT;
		std::byte* Result;
		if (Index >= Allocators.size())
		{
			Result = Subspace->TryAllocate(NumBytes);
		}
		else
		{
			Result = Allocators[Index].TryAllocate();
		}
		return Result;
	}

	COREUOBJECT_API static FContextImpl* ClaimOrAllocateContext(EContextHeapRole);
	COREUOBJECT_API void ReleaseContext();
	COREUOBJECT_API void FreeContextDueToThreadDeath();

	void ValidateUnclaimedContext();
	void ValidateContextHasEmptyStack();

	COREUOBJECT_API FContextImpl();
	COREUOBJECT_API ~FContextImpl();

	COREUOBJECT_API static FContextImpl* GetCurrentImpl();

	COREUOBJECT_API void AcquireAccessSlow();
	COREUOBJECT_API void RelinquishAccessSlow();
	COREUOBJECT_API void CheckForHandshakeSlow();

	COREUOBJECT_API VCell* RunWeakReadBarrierNonNullSlow(VCell* Cell);
	COREUOBJECT_API void* RunAuxWeakReadBarrierNonNullSlow(void* Aux);

	COREUOBJECT_API void RequestHandshake(TUniqueFunction<void(FHandshakeContext)>&& HandshakeAction);
	COREUOBJECT_API bool AttemptHandshakeAcknowledgement();
	COREUOBJECT_API void WaitForHandshakeAcknowledgement();
	COREUOBJECT_API void AcknowledgeHandshakeRequest();
	COREUOBJECT_API void AcknowledgeHandshakeRequestWhileHoldingLock();

	COREUOBJECT_API void ClearManualStackScanRequestWhileHoldingLock();

	EContextLifecycleState LifecycleState = EContextLifecycleState::Free;

	static constexpr TState HasAccessBit = 1;
	static constexpr TState HandshakeRequestedBit = 2;
	static constexpr TState StopRequestedBit = 4;

	// This variable has a very particular synchronization story:
	// - The thread that this context belongs to may CAS this without holding the StateMutex.
	// - Any other thread may CAS this only while holding the StateMutex.
	std::atomic<TState> State = 0;

	size_t StopRequestCount = 0;

	// Threads can request to use manual stack scan. Stack scan is special to the GC; it's when the GC determines
	// termination. Therefore, a thread that uses manual stack scan can block GC termination. This is great for
	// binding the GC to another GC (like the UE GC).
	bool bUsesManualStackScanning = false;

	// This being set indicates that a manual stack scan has been requested.
	bool bManualStackScanRequested = false;

	// If this is true and we're doing manual stack scans then it means that there's no need to scan the stack.
	// This is intended to be set to true by the engine at the end of a tick.
	bool bIsInManuallyEmptyStack = false;

	// This mutex protects the State, but only partly, since the thread that owns the context may CAS the
	// State directly.
	//
	// The idea is that if a handshake is requested then the thread will end up processing the handshake while
	// holding this lock. So, if you hold this lock, then you're running exclusively to the thread's handling of
	// handshakes.
	UE::FMutex StateMutex;

	// This condition variable achieves two things:
	// - If we want a thread to stop, like if we're doing a hard handshake, then we have it wait on this
	//   condition variable.
	// - If we want to wait for a thread to acknowledge that we asked it for something, then we can wait on this
	//   condition variable.
	// We should always use NotifyAll() and never NotifyOne() on this condition variable!
	UE::FConditionVariable StateConditionVariable;

	// This mutex protects the act of handshaking with this thread. If any thread wants to do a handshake, then
	// it must hold its own handshake lock (if it has one) and the handshake lock of any thread it is handshaking
	// with. When acquiring multiple handshake locks, acquire them in address order.
	UE::FMutex HandshakeMutex;

	// When a handshake is requested, the thread acquires the StateMutex, clears the handshake request, runs
	// this callback, and then does a broadcast.
	TUniqueFunction<void(FHandshakeContext)> RequestedHandshakeAction;

	// The libpas TLC node for this thread. Useful for stopping allocators, even from a different thread, so long
	// as we're handshaking. TLC nodes are immortal and point to the actual TLC, which may resize (and move) at any
	// time.
	pas_thread_local_cache_node* ThreadLocalCacheNode = nullptr;

	// We could be in this weird state where libpas has already destroyed its TLC node, and the node could
	// even be associated with a different thread, but we still have it because we haven't destroyed our
	// context. For this reason, libpas requires us to refer to the TLC node using both the node and its
	// version. The version is guaranteed to increment when the TLC node is reclaimed.
	uint64_t ThreadLocalCacheNodeVersion = 0;

	FConservativeStackEntryFrame* TopEntryFrame = nullptr;
	FConservativeStackExitFrame* TopExitFrame = nullptr;

	FTransaction* _CurrentTransaction = nullptr;

	EContextHeapRole HeapRole;

	FastAllocatorArray FastSpaceAllocators;
	FastAllocatorArray AuxSpaceAllocators;

	// The context for the current thread. Even if the thread stops passing around a context and forgets
	// it has one, we must remember that it had one, because of the affinity between our notion of
	// context and other notions of context in the system (libpas's TLC, AutoRTFM's context, etc).
	static thread_local FThreadLocalContextHolder CurrentThreadContext;

	// We need to be able to enumerate over all of the currently live contexts. This is a set so that it's easy
	// for us to remove ourselves from it (but obviously that could be accomplished with an array if we were even
	// a bit smart, FIXME).
	COREUOBJECT_API static TNeverDestroyed<TSet<FContextImpl*>> LiveContexts;

	// Contexts must be pooled to avoid nasty races in handshakes. Basically, if we request a handshake and the
	// thread decides to die, then we should be able to handshake it anyway. Worst case, we'll either:
	// - Handshake a newly created thread (who cares).
	// - Realize that we're handshaking a dead context (we have to defend against this explicitly, and we do).
	COREUOBJECT_API static TNeverDestroyed<TArray<FContextImpl*>> FreeContexts;

	// This mutex protects the LiveContexts/FreeContexts data structures.
	COREUOBJECT_API static UE::FMutex LiveAndFreeContextsMutex;

	// This mutex protects context creation. Should be held before LiveAndFreeContextsMutex. Holding this mutex
	// ensures that no new contexts can be created. By "created" we mean both:
	// - Allocating a new FContextImpl.
	// - Pulling a FContextImpl from FreeContexts.
	COREUOBJECT_API static UE::FMutex ContextCreationMutex;

	// Mutex to hold when stopping the world so that only one component is stopping the world at any time. It
	// should be held before LiveAndFreeContextsMutex/ContextCreationMutex. Note that context creation holds this
	// mutex also, so if you hold this one, no new contexts can be created.
	//
	// NOTE: This is separate from ContextCreationMutex because we want to be able to perform soft handshakes
	// while the world is stopped, and soft handshakes need to lock out context creation right at the beginning of
	// the soft handshake.
	COREUOBJECT_API static UE::FMutex StoppedWorldMutex;
};

} // namespace Verse
#endif // WITH_VERSE_VM
