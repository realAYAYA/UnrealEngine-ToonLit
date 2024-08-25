// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMContextImpl.h"
#include "Experimental/Async/MultiUniqueLock.h"
#include "VerseVM/VVMConservativeStackEntryFrame.h"
#include "VerseVM/VVMConservativeStackExitFrame.h"
#include "VerseVM/VVMHardHandshakeContext.h"
#include "VerseVM/VVMLog.h"
#include "VerseVM/VVMNeverDestroyed.h"
#include "VerseVM/VVMSanitizers.h"
#include "VerseVM/VVMThreadLocalContextHolder.h"
#include "VerseVM/VVMTransaction.h"
#include "VerseVM/VVMTrue.h"
#include "verse_heap_ue.h"

namespace Verse
{
thread_local FThreadLocalContextHolder FContextImpl::CurrentThreadContext;
TNeverDestroyed<TSet<FContextImpl*>> FContextImpl::LiveContexts;
TNeverDestroyed<TArray<FContextImpl*>> FContextImpl::FreeContexts;
UE::FMutex FContextImpl::LiveAndFreeContextsMutex;
UE::FMutex FContextImpl::ContextCreationMutex;
UE::FMutex FContextImpl::StoppedWorldMutex;

FContextImpl* FContextImpl::GetCurrentImpl()
{
	return CurrentThreadContext.Get();
}

FORCENOINLINE void FContextImpl::EnterConservativeStack(TFunctionRef<void()> Func)
{
	check(!InConservativeStack());
	// Changes to conservative stack state can only happen when you have access, so that we don't race
	// with conservative marking, which only runs when you don't have access.
	check(HasAccess());
	FConservativeStackEntryFrame Frame;
	Frame.ExitFrame = TopExitFrame;
	TopEntryFrame = &Frame;
	Func();
	check(TopEntryFrame == &Frame);
	check(TopExitFrame == Frame.ExitFrame);
	TopEntryFrame = TopExitFrame ? TopExitFrame->EntryFrame : nullptr;
}

FORCENOINLINE void FContextImpl::ExitConservativeStack(TFunctionRef<void()> Func)
{
	check(TopEntryFrame);
	check(InConservativeStack());
	check(HasAccess());
	FConservativeStackExitFrameWithJmpBuf Frame;
#if PLATFORM_WINDOWS
#pragma warning(push)
#pragma warning(disable : 4611)
	setjmp(Frame.JmpBuf);
#pragma warning(pop)
#else
	_setjmp(Frame.JmpBuf);
#endif
	Frame.EntryFrame = TopEntryFrame;
	TopExitFrame = &Frame;
	Func();
	check(TopExitFrame == &Frame);
	check(TopEntryFrame == Frame.EntryFrame);
	TopExitFrame = TopEntryFrame->ExitFrame;
}

FContextImpl::FContextImpl()
{
	// This table is directly from pas_designated_intrinsic_heap.c. That's been optimized to death. Note that it allows some similar sizes to share the same
	// size class, which reduces fragmentation and improves allocator performance. It's a net space and time win.
	TArray<size_t> FastAllocatorSizes = {
		16,
		16,
		32,
		48,
		64,
		80,
		96,
		128,
		128,
		160,
		160,
		192,
		192,
		224,
		224,
		256,
		256,
		304,
		304,
		304,
		352,
		352,
		352,
		416,
		416,
		416,
		416};

	auto InitializeFastAllocators = [&FastAllocatorSizes](FastAllocatorArray& Allocators, FSubspace* Subspace) {
		V_DIE_UNLESS(FastAllocatorSizes.Num() == Allocators.size());

		size_t LastSizeByIndex = 0;
		size_t LastSizeByTable = 0;
		for (size_t Index = 0; Index < Allocators.size(); ++Index)
		{
			size_t SizeByIndex = Index << VERSE_HEAP_MIN_ALIGN_SHIFT;
			V_DIE_UNLESS(SizeByIndex <= FastAllocatorSizes[Index]);

			if (FastAllocatorSizes[Index] != LastSizeByTable)
			{
				// Make sure that the sizes aren't unnecessarily sloppy. In particular, an allocator at a particular index should only point
				// to a size that is larger than that index if there is also some larger index that matches that size exactly.
				check(LastSizeByIndex == LastSizeByTable);
			}

			LastSizeByIndex = SizeByIndex;
			LastSizeByTable = FastAllocatorSizes[Index];

			Allocators[Index].Initialize(Subspace, FastAllocatorSizes[Index]);
		}
	};

	InitializeFastAllocators(FastSpaceAllocators, FHeap::FastSpace);
	InitializeFastAllocators(AuxSpaceAllocators, FHeap::AuxSpace);
}

FContextImpl::~FContextImpl()
{
	if (bGTrue)
	{
		V_DIE("Contexts should be pooled, never destroyed");
	}
}

void FContextImpl::ValidateUnclaimedContext()
{
	V_DIE_IF(LifecycleState == EContextLifecycleState::LiveAndInUse);
	V_DIE_IF(bUsesManualStackScanning);
	V_DIE_IF(ManualStackScanRequested());
	V_DIE_IF(bIsInManuallyEmptyStack);
	ValidateContextHasEmptyStack();
}

void FContextImpl::ValidateContextHasEmptyStack()
{
	V_DIE_IF(HasAccess());
	V_DIE_IF(TopEntryFrame);
	V_DIE_IF(TopExitFrame);
	V_DIE_IF(State & HasAccessBit);
}

FContextImpl* FContextImpl::ClaimOrAllocateContext(EContextHeapRole HeapRole)
{
	using namespace UE;
	FContextImpl* Result;
	Result = CurrentThreadContext.Get();
	if (Result)
	{
		TUniqueLock StateLock(Result->StateMutex);
		Result->ValidateUnclaimedContext();
		V_DIE_UNLESS(Result->LifecycleState == EContextLifecycleState::LiveButUnused);
		V_DIE_UNLESS(Result->HeapRole == HeapRole);
		Result->LifecycleState = EContextLifecycleState::LiveAndInUse;
	}
	else
	{
		TUniqueLock StopTheWorldLock(StoppedWorldMutex);
		TUniqueLock CreationLock(ContextCreationMutex);
		TUniqueLock Lock(LiveAndFreeContextsMutex);
		if (FreeContexts->IsEmpty())
		{
			Result = new FContextImpl();
		}
		else
		{
			Result = FreeContexts->Pop();
		}
		Result->ValidateUnclaimedContext();
		{
			TUniqueLock StateLock(Result->StateMutex);
			V_DIE_UNLESS(Result->LifecycleState == EContextLifecycleState::Free);
			Result->LifecycleState = EContextLifecycleState::LiveAndInUse;
			Result->ThreadLocalCacheNode = verse_heap_get_thread_local_cache_node();
			Result->ThreadLocalCacheNodeVersion = pas_thread_local_cache_node_version(Result->ThreadLocalCacheNode);
			Result->HeapRole = HeapRole;
		}
		LiveContexts->Add(Result);
		CurrentThreadContext.Set(Result);
	}
	return Result;
}

void FContextImpl::ReleaseContext()
{
	using namespace UE;
	TUniqueLock StateLock(StateMutex);
	V_DIE_UNLESS(LifecycleState == EContextLifecycleState::LiveAndInUse);
	V_DIE_UNLESS(CurrentThreadContext.Get() == this);
	if (ManualStackScanRequested())
	{
		ClearManualStackScanRequestWhileHoldingLock();
		V_DIE_IF(ManualStackScanRequested());
	}
	ValidateContextHasEmptyStack();
	LifecycleState = EContextLifecycleState::LiveButUnused;
	bUsesManualStackScanning = false;
	bIsInManuallyEmptyStack = false;
}

void FContextImpl::FreeContextDueToThreadDeath()
{
	using namespace UE;
	ValidateUnclaimedContext();
	TUniqueLock Lock(LiveAndFreeContextsMutex);
	{
		TUniqueLock StateLock(StateMutex);
		V_DIE_UNLESS(LifecycleState == EContextLifecycleState::LiveButUnused);
		// Must stop allocators before handshakes lose the ability to. Otherwise, we'll have some iteration
		// start while this thread still has allocators claiming some memory.
		StopAllocators();
		LifecycleState = EContextLifecycleState::Free;
		ThreadLocalCacheNode = nullptr;
		ThreadLocalCacheNodeVersion = 0;
	}
	LiveContexts->Remove(this);
	FreeContexts->Push(this);
}

void FContextImpl::RequestHandshake(TUniqueFunction<void(FHandshakeContext)>&& HandshakeAction)
{
	using namespace UE;
	V_DIE_UNLESS(HandshakeMutex.IsLocked());
	TUniqueLock Lock(StateMutex);
	V_DIE_IF(State & HandshakeRequestedBit);
	State |= HandshakeRequestedBit;

	RequestedHandshakeAction = MoveTemp(HandshakeAction);

	// Now that we've set the HandshakeRequestedBit, the following could happen:
	// - If State = HasAccessBit | HandshakeRequestedBit, then the target thread might take a handshake slow
	//   path at any time, and then we will grab the StateMutex and execute the RequestedHandshakeAction.
	// - If State = HandshakeRequestedBit, then the target thread might not do anything for a while. Or, it might
	//   try to acquire access, in which case it'll execute the RequestedHandshakeAction.
}

bool FContextImpl::AttemptHandshakeAcknowledgement()
{
	using namespace UE;
	V_DIE_UNLESS(HandshakeMutex.IsLocked());
	TUniqueLock Lock(StateMutex);
	if (State & HandshakeRequestedBit)
	{
		// The handshake hasn't been acknowledged yet.
		if (State & HasAccessBit)
		{
			return false;
		}
		else
		{
			AcknowledgeHandshakeRequestWhileHoldingLock();
		}
	}
	V_DIE_IF(State & HandshakeRequestedBit);
	return true;
}

void FContextImpl::WaitForHandshakeAcknowledgement()
{
	using namespace UE;
	V_DIE_UNLESS(HandshakeMutex.IsLocked());
	TUniqueLock Lock(StateMutex);
	if (State & HandshakeRequestedBit)
	{
		// The handshake hasn't been acknowledged yet.
		if (State & HasAccessBit)
		{
			while (State & HandshakeRequestedBit)
			{
				StateConditionVariable.Wait(StateMutex);
			}
		}
		else
		{
			AcknowledgeHandshakeRequestWhileHoldingLock();
		}
	}
	V_DIE_IF(State & HandshakeRequestedBit);
}

void FContextImpl::AcknowledgeHandshakeRequest()
{
	using namespace UE;
	TUniqueLock Lock(StateMutex);
	AcknowledgeHandshakeRequestWhileHoldingLock();
}

void FContextImpl::AcknowledgeHandshakeRequestWhileHoldingLock()
{
	using namespace UE;
	V_DIE_UNLESS(StateMutex.IsLocked());
	if (State & HandshakeRequestedBit)
	{
		V_DIE_UNLESS(HandshakeMutex.IsLocked());
		TUniqueFunction<void(FHandshakeContext)> HandshakeAction = MoveTemp(RequestedHandshakeAction);
		V_DIE_UNLESS(HandshakeAction);
		V_DIE_IF(RequestedHandshakeAction);

		HandshakeAction(FHandshakeContext(this));

		// We must clear the handshake bit last. As soon as we clear the bit, the victim thread will think that
		// it's free to run!
		State &= ~HandshakeRequestedBit;
		StateConditionVariable.NotifyAll();
	}
}

void FContextImpl::SetIsInManuallyEmptyStack(bool bInIsInManuallyEmptyStack)
{
	using namespace UE;
	V_DIE_UNLESS(bUsesManualStackScanning);
	TUniqueLock Lock(StateMutex);
	V_DIE_IF(bManualStackScanRequested && bIsInManuallyEmptyStack);
	bIsInManuallyEmptyStack = bInIsInManuallyEmptyStack;
	if (bIsInManuallyEmptyStack && bManualStackScanRequested)
	{
		ClearManualStackScanRequestWhileHoldingLock();
	}
}

void FContextImpl::ClearManualStackScanRequest()
{
	using namespace UE;
	V_DIE_UNLESS(bUsesManualStackScanning);
	if (bManualStackScanRequested)
	{
		TUniqueLock Lock(StateMutex);
		ClearManualStackScanRequestWhileHoldingLock();
	}
}

void FContextImpl::ClearManualStackScanRequestWhileHoldingLock()
{
	using namespace UE;
	V_DIE_UNLESS(bUsesManualStackScanning);
	V_DIE_UNLESS(StateMutex.IsLocked());
	if (bManualStackScanRequested)
	{
		TUniqueLock Lock(FHeap::Mutex);
		V_DIE_UNLESS(FHeap::NumThreadsToScanStackManually);
		V_DIE_UNLESS(bManualStackScanRequested);

		// FIXME: Need a test for this. This is needed whenever a barrier runs.
		FHeap::MarkStack->Append(MoveTemp(MarkStack));

		FHeap::NumThreadsToScanStackManually--;
		bManualStackScanRequested = false;
		if (!FHeap::NumThreadsToScanStackManually)
		{
			FHeap::ConditionVariable.NotifyAll();
		}
	}
}

void FContextImpl::RequestStop()
{
	using namespace UE;
	TUniqueLock Lock(StateMutex);
	StopRequestCount++;
	State |= StopRequestedBit;
}

void FContextImpl::WaitForStop()
{
	using namespace UE;
	TUniqueLock Lock(StateMutex);
	V_DIE_UNLESS(State & StopRequestedBit);
	V_DIE_UNLESS(StopRequestCount);
	while (State & HasAccessBit)
	{
		StateConditionVariable.Wait(StateMutex);
	}
}

void FContextImpl::CancelStop()
{
	using namespace UE;
	TUniqueLock Lock(StateMutex);
	V_DIE_UNLESS(State & StopRequestedBit);
	V_DIE_UNLESS(StopRequestCount);
	StopRequestCount--;
	if (!StopRequestCount)
	{
		State &= ~StopRequestedBit;
	}
	StateConditionVariable.NotifyAll();
}

void FContextImpl::PairHandshake(FContextImpl* TargetContext, TFunctionRef<void(FHandshakeContext)> HandshakeAction)
{
	using namespace UE;
	V_DIE_IF(State & HasAccessBit);
	TUniqueLock Lock(TargetContext->HandshakeMutex);
	TargetContext->RequestHandshake([HandshakeAction](FHandshakeContext Context) {
		HandshakeAction(Context);
	});
	TargetContext->WaitForHandshakeAcknowledgement();
}

void FContextImpl::SoftHandshake(TFunctionRef<void(FHandshakeContext)> HandshakeAction)
{
	using namespace UE;
	V_DIE_IF(State & HasAccessBit);
	TUniqueLock CreationLock(ContextCreationMutex);

	TSet<FContextImpl*> Contexts;
	{
		TUniqueLock Lock(LiveAndFreeContextsMutex);
		Contexts = *LiveContexts;
	}
	TArray<FMutex*> HandshakeMutexes;
	for (FContextImpl* Context : Contexts)
	{
		HandshakeMutexes.Push(&Context->HandshakeMutex);
	}

	TMultiUniqueLock<FMutex> HandshakeLock(MoveTemp(HandshakeMutexes));

	// Now we know that:
	// - No new thread can be created.
	// - `Contexts` has all the threads.
	// - No thread can do any kind of handshake but us.

	// This loop just requests handshakes from all threads. It's important that it only requests and doesn't
	// wait or do any work, because we urgently want to get to a state where all threads know that a handshake
	// has been requested.
	for (FContextImpl* Context : Contexts)
	{
		// It's weirdly OK for each of the contexts to get a function that closes over HandshakeAction, which
		// is on our stack. That's because we know that all of these threads will acknowledge our handshake
		// request - and call the function we've given them - before we pop this stack frame.
		Context->RequestHandshake([HandshakeAction](FHandshakeContext Context) {
			HandshakeAction(Context);
		});
	}

	// Now all threads know that a handshake has been requested. We could just wait for each thread in turn at
	// this point. But it's more scalable to first try to catch threads in a state where they have either
	// acknowledged the handshake, or we can acknowledge it on their behalf.
	TArray<FContextImpl*> ContextsToRemove;
	do
	{
		ContextsToRemove.Reset();
		for (FContextImpl* Context : Contexts)
		{
			if (Context->AttemptHandshakeAcknowledgement())
			{
				ContextsToRemove.Push(Context);
			}
		}
		for (FContextImpl* Context : ContextsToRemove)
		{
			Contexts.Remove(Context);
		}
	}
	while (!ContextsToRemove.IsEmpty());

	// Now we're down to threads that we have to wait on. So let's wait on them. We hope that in the fast case,
	// this Contexts is empty by now.
	for (FContextImpl* Context : Contexts)
	{
		Context->WaitForHandshakeAcknowledgement();
	}
}

FStoppedWorld FContextImpl::StopTheWorld()
{
	using namespace UE;
	V_DIE_IF(State & HasAccessBit);

	StoppedWorldMutex.Lock();

	TUniqueLock CreationLock(ContextCreationMutex);

	TSet<FContextImpl*> Contexts;
	{
		TUniqueLock Lock(LiveAndFreeContextsMutex);
		Contexts = *LiveContexts;
	}

	FStoppedWorld Result;
	Result.bHoldingStoppedWorldMutex = true;
	for (FContextImpl* Context : Contexts)
	{
		if (Context != this)
		{
			Result.Contexts.Push(FAccessContext(Context, EIsInHandshake::Yes));
			Context->RequestStop();
		}
	}
	for (FAccessContext Context : Result.Contexts)
	{
		Context.GetImpl()->WaitForStop();
	}
	return Result;
}

void FContextImpl::HardHandshake(TFunctionRef<void(FHardHandshakeContext)> HandshakeAction)
{
	FStoppedWorld StoppedWorld = StopTheWorld();
	HandshakeAction(FHardHandshakeContext(this, &StoppedWorld));
	StoppedWorld.CancelStop();
}

void FContextImpl::StopAllocators()
{
	V_DIE_UNLESS(StateMutex.IsLocked());
	if (IsLive())
	{
		for (FLocalAllocator& Allocator : FastSpaceAllocators)
		{
			Allocator.Stop();
		}

		for (FLocalAllocator& Allocator : AuxSpaceAllocators)
		{
			Allocator.Stop();
		}

		V_DIE_UNLESS(ThreadLocalCacheNode);
		V_DIE_UNLESS(ThreadLocalCacheNodeVersion);
		verse_heap_thread_local_cache_node_stop_local_allocators(ThreadLocalCacheNode, ThreadLocalCacheNodeVersion);
	}
}

V_NO_SANITIZE_ADDRESS void FContextImpl::MarkReferencedCells()
{
	// The thread that this context belongs to might not be the thread that calls this function. But no
	// matter whether that's the case or not, it must be the case that the thread is not in a conservative
	// stack. Only threads that have access may enter/exit the conservative stack. This function may only
	// be called for threads that do not have access.
	V_DIE_IF(InConservativeStack());
	size_t NumMarkedBefore = MarkStack.Num();
	FConservativeStackExitFrame* ExitFrame = TopExitFrame;
	while (ExitFrame)
	{
		FConservativeStackEntryFrame* EntryFrame = ExitFrame->EntryFrame;
		V_DIE_UNLESS(EntryFrame);
		V_DIE_UNLESS(reinterpret_cast<void*>(EntryFrame) > reinterpret_cast<void*>(ExitFrame));

		uintptr_t* Begin = reinterpret_cast<uintptr_t*>(ExitFrame + 1);
		uintptr_t* End = reinterpret_cast<uintptr_t*>(EntryFrame);

		for (uintptr_t* Ptr = Begin; Ptr < End; ++Ptr)
		{
			uintptr_t Value = *Ptr;

			if (uintptr_t CellBase = verse_heap_find_allocated_object_start(Value))
			{
				// This needs to be a fenced mark because we need it to happen after we think we find the
				// start of an allocated object.
				//
				// Why?
				//
				// Because we are racing with allocation and construction. The find_allocated_object_start
				// function will find objects that:
				//
				// - Were *just* allocated but not yet constructed. We don't want to scan those objects!
				// - Aren't allocated yet, but are cached for future allocation by some local allocator.
				//
				// Luckily, all such objects will have been allocated black; i.e. they would have had their
				// mark bit set before they were allocated. Libpas ensures that the setting of the mark bit
				// for black-allocated objects happens before the allocation bits are set (i.e. before
				// find_allocated_object_start would see those objects). Hence, if we ensure that we see the
				// mark bit after we find_allocated_object_start, then we will not mark those objects,
				// because we will see that their mark bit is already set.
				//
				// See the comment above find_allocated_object_start in verse_heap_inlines.h for more
				// details.

				if (reinterpret_cast<FSubspace*>(verse_heap_get_heap(CellBase)) == FHeap::AuxSpace)
				{
					MarkStack.FencedMarkAuxNonNull(reinterpret_cast<void*>(CellBase));
				}
				else
				{
					MarkStack.FencedMarkNonNull(reinterpret_cast<VCell*>(CellBase));
				}
			}
		}

		ExitFrame = EntryFrame->ExitFrame;
	}
	// TODO: Report aux memory marked as well.
	size_t NumMarkedAfter = MarkStack.Num();
	size_t NumMarked = NumMarkedAfter - NumMarkedBefore;
	if (NumMarked)
	{
		UE_LOG(LogVerseGC, Verbose, TEXT("Conservatively marked %zu cells in %p"), NumMarked, this);
	}

	if (FTransaction* Transaction = CurrentTransaction())
	{
		FTransaction::MarkReferencedCells(*Transaction, MarkStack);
	}
}

void FContextImpl::AcquireAccessSlow()
{
	using namespace UE;
	V_DIE_IF(InConservativeStack());
	for (;;)
	{
		TState OldState = State;
		V_DIE_IF(OldState & HasAccessBit);
		if (OldState & HandshakeRequestedBit)
		{
			AcknowledgeHandshakeRequest();
		}
		else if (OldState & StopRequestedBit)
		{
			TUniqueLock Lock(StateMutex);
			while (StopRequestCount)
			{
				// If the count is non-zero, then the StopRequestedBit should be set.
				V_DIE_UNLESS(State & StopRequestedBit);
				StateConditionVariable.Wait(StateMutex);
			}
			// CancelStop() should have simultaneously cleared this bit whenever it decremented the stop
			// request count to zero.
			V_DIE_IF(State & StopRequestedBit);
		}
		else
		{
			TState NewState = OldState | HasAccessBit;
			V_DIE_IF(OldState & HandshakeRequestedBit);
			V_DIE_IF(OldState & StopRequestedBit);
			V_DIE_IF(NewState & HandshakeRequestedBit);
			V_DIE_IF(NewState & StopRequestedBit);
			if (State.compare_exchange_weak(OldState, NewState))
			{
				return;
			}
		}
	}
}

void FContextImpl::RelinquishAccessSlow()
{
	using namespace UE;
	V_DIE_IF(InConservativeStack());
	for (;;)
	{
		TState OldState = State;
		V_DIE_UNLESS(OldState & HasAccessBit);
		if (OldState & HandshakeRequestedBit)
		{
			AcknowledgeHandshakeRequest();
		}
		else if (OldState & StopRequestedBit)
		{
			TUniqueLock Lock(StateMutex);
			if (!(State & HandshakeRequestedBit) && (State & StopRequestedBit))
			{
				V_DIE_UNLESS(StopRequestCount);
				StateConditionVariable.NotifyAll();

				V_DIE_IF(State & HandshakeRequestedBit);
				V_DIE_UNLESS(State & StopRequestedBit);
				V_DIE_UNLESS(StopRequestCount);
				V_DIE_UNLESS(State & HasAccessBit);

				State &= ~HasAccessBit;
				return;
			}
		}
		else
		{
			TState NewState = OldState & ~HasAccessBit;
			V_DIE_IF(OldState & HandshakeRequestedBit);
			V_DIE_IF(OldState & StopRequestedBit);
			V_DIE_IF(NewState & HandshakeRequestedBit);
			V_DIE_IF(NewState & StopRequestedBit);
			if (State.compare_exchange_weak(OldState, NewState))
			{
				return;
			}
		}
	}
}

void FContextImpl::CheckForHandshakeSlow()
{
	using namespace UE;
	V_DIE_UNLESS(State & HasAccessBit);
	ExitConservativeStack([this]() {
		if (State & HandshakeRequestedBit)
		{
			AcknowledgeHandshakeRequest();
		}
		if (State & StopRequestedBit)
		{
			RelinquishAccess();
			AcquireAccess();
		}
	});
}

VCell* FContextImpl::RunWeakReadBarrierNonNullSlow(VCell* Cell)
{
	V_DIE_UNLESS(Cell);

	if (FHeap::IsMarked(Cell))
	{
		return Cell;
	}
	return RunWeakReadBarrierUnmarkedWhenActive(Cell, [this](const VCell* Cell) { MarkStack.MarkNonNull(Cell); });
}

void* FContextImpl::RunAuxWeakReadBarrierNonNullSlow(void* Aux)
{
	V_DIE_UNLESS(Aux);

	if (FHeap::IsMarked(Aux))
	{
		return Aux;
	}
	return RunWeakReadBarrierUnmarkedWhenActive(Aux, [this](const void* Aux) { MarkStack.MarkAuxNonNull(Aux); });
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
