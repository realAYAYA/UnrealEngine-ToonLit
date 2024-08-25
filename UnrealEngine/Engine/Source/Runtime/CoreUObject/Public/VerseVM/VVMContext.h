// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMContextImpl.h"
#include "VVMHeap.h"
#include "VVMSubspace.h"

namespace Verse
{
// Threads must always have a reference to the context. But we never point at the context directly. Instead,
// we use capability objects:
//
// - FContext: knows the context but can't do anything with it.
// - FIOContext: cannot access the heap or allocate; but it can reacquire access.
// - FAccessContext: can access the heap, but cannot allocate or relinquish access.
// - FRunningContext: can access the heap, relinquish access, check for handshakes, and switch to allocating.
// - FAllocationContext: can access the heap, allocate, but cannot relinquish access or check for handshakes.
// - FRunningContextPromise: for use in UE (and like clients) where the default state of a thread is running, but
//   the context isn't being passed around (like for VInt::operator+).
//
// The reason for all of this trouble is that we want the code to be clear about its contract with the GC at
// any point in the program. You can tell what a function can do to the heap by looking at what kind of context
// it takes.

struct FContext;
struct FIOContext;
struct FAccessContext;
struct FRunningContext;
struct FAllocationContext;
struct FHandshakeContext;
struct FStopRequest;
struct FHardHandshakeContext;
struct VValue;

template <typename T>
struct TWriteBarrier;

enum class EIsInHandshake
{
	No,
	Yes
};

struct FContext
{
	FContext() = default;

	FContext(const FContext& Other)
		: EncodedWord(Other.EncodedWord)
	{
		CheckBaseInvariants();
	}

	FContext& operator=(const FContext&) = default;

	EIsInHandshake IsInHandshake() const
	{
		return (EncodedWord & IsInHandshakeBit) ? EIsInHandshake::Yes : EIsInHandshake::No;
	}

	bool operator==(const FContext& Other) const
	{
		return GetImpl() == Other.GetImpl();
	}

	explicit operator bool() const
	{
		return !!GetImpl();
	}

	EContextHeapRole GetHeapRole() const
	{
		return GetImpl()->GetHeapRole();
	}

	void EnableManualStackScanning() const
	{
		GetImpl()->EnableManualStackScanning();
	}

	bool UsesManualStackScanning() const
	{
		return GetImpl()->UsesManualStackScanning();
	}

	// You can use this to guard calling ClearManualStackScanRequest(). But you don't have to, because that function will return
	// quickly if no manual stack scan has been requested!
	bool ManualStackScanRequested() const
	{
		return GetImpl()->ManualStackScanRequested();
	}

	// If a thread is using manual stack scanning, then it should periodically call this function.
	//
	// - It's not OK to call this function if you haven't enabled manual stack scanning. Calling this function if manual stack
	//   scanning is not enabled for this context will crash!
	// - It's OK to call this function if GC is not running. This function will quickly return in that case.
	// - It's OK to call this function if manual stack scanning hasn't been requested. This function will quickly return in that
	//   case.
	// - If you call this function, you have to make sure you have already somehow marked all roots associated with your context.
	//   The way UE GC does that is by calling this function at end of tick, when there are no roots associated with the main
	//   thread context!
	void ClearManualStackScanRequest() const
	{
		GetImpl()->ClearManualStackScanRequest();
	}

	bool IsInManuallyEmptyStack() const
	{
		return GetImpl()->IsInManuallyEmptyStack();
	}

	void SetIsInManuallyEmptyStack(bool bInIsInManuallyEmptyStack) const
	{
		GetImpl()->SetIsInManuallyEmptyStack(bInIsInManuallyEmptyStack);
	}

protected:
	friend class FHeap; // FHeap should try to use FContext API whenever possible, but sometimes it isn't practical.
	friend struct FIOContext;
	friend struct FStoppedWorld;

	FContext(FContextImpl* Impl, EIsInHandshake IsInHandshake)
	{
		SetImpl(Impl);
		SetIsInHandshake(IsInHandshake);
		CheckBaseInvariants();
	}

	void CheckBaseInvariants() const
	{
		checkSlow(GetImpl()->IsLive() || IsInHandshake() == EIsInHandshake::Yes);
	}

	FContextImpl* GetImpl() const
	{
		return reinterpret_cast<FContextImpl*>(EncodedWord & ~IsInHandshakeBit);
	}

	void SetImpl(FContextImpl* Impl)
	{
		EncodedWord = (EncodedWord & IsInHandshakeBit) | reinterpret_cast<uintptr_t>(Impl);
	}

	void SetIsInHandshake(EIsInHandshake IsInHandshake)
	{
		EncodedWord = (EncodedWord & ~IsInHandshakeBit) | (IsInHandshake == EIsInHandshake::Yes ? IsInHandshakeBit : 0);
	}

	static constexpr uintptr_t IsInHandshakeBit = 1;

	uintptr_t EncodedWord = 0;
};

// Having an IO context means:
//
// - You cannot access the heap.
// - You cannot allocate.
// - You can reacquire heap access (and then you'll have a FRunningContext).
//
// IO contexts are the default for external native code. When we support a public native API, native code will
// run in the IO context.
//
// IO contexts ARE NOT the default for engine code! Engine code is in a running context by default.
struct FIOContext : FContext
{
	FIOContext(const FIOContext& Other)
		: FContext(Other)
	{
		CheckIOInvariants();
	}

	// Create the context for this thread and run some code in it without heap access. You can only have one
	// context created per thread.
	template <typename TFunc>
	static void Create(const TFunc& Func, EContextHeapRole HeapRole = EContextHeapRole::Mutator)
	{
		FIOContext Context(FContextImpl::ClaimOrAllocateContext(HeapRole), EIsInHandshake::No);
		Func(Context);
		Context.GetImpl()->ReleaseContext();
	}

	template <typename TFunc>
	void AcquireAccess(const TFunc& Func) const;

	// Wait until the target context runs the given function. If that context doesn't have access, the action
	// runs immediately and on the calling thread.
	COREUOBJECT_API void PairHandshake(FContext Context, TFunctionRef<void(FHandshakeContext)> HandshakeAction) const;

	// Wait until all currently running contexts run the given function. For contexts that don't have access,
	// the action runs immediately and on the calling thread. Also calls the function for the calling thread.
	//
	// The soft handshake is the basis for the Verse concurrent GC.
	COREUOBJECT_API void SoftHandshake(TFunctionRef<void(FHandshakeContext)> HandshakeAction) const;

	// This is the structured stop-the-world API. Threads are stopped before HandshakeAction runs and resumed
	// after it returns. HandshakeAction is a FHardHandshakeContext, which is a subclass of FIOContext - so you
	// can do anything an IO context can do while inside the handshake, including:
	// - Acquiring access and then doing anything a running context lets you do
	// - Start new threads (which aren't going to be stopped)
	// - Do soft and pair handshakes (stopped threads show up as not having access)
	//
	// Of course, that's super dangerous, since who knows what locks are being held by stopped threads. For sure,
	// you won't want to acquire locks that would be held in a running context while inside a hard handshake.
	//
	// Only one hard handshake can happen at any time. Attempting to hard handshake while one is already
	// happening blocks until that one finishes. That's because hard handshakes use StopTheWorld under the
	// hood, and StopTheWorld grabs a lock.
	//
	// FIXME: We should allow hard handshaking a subset of threads. For example, we want to be able to hard
	// handshake all but the GC threads.
	COREUOBJECT_API void HardHandshake(TFunctionRef<void(FHardHandshakeContext)> HandshakeAction) const;

	// This is the unstructured stop-the-world API. Threads are resumed whenever the FStoppedWorld object dies. It
	// has move semantics, so you can keep it alive by moving it around. Additionally, the FStoppedWorld can tell
	// you about all of the threads that were stopped and it gives you an access context to each of them.
	//
	// The calling thread is not stopped.
	//
	// It's legal to stop the world and then execute anything that an IO context would let you execute,
	// including:
	// - Acquiring access and then doing anything a running context lets you do
	// - Start new threads (which aren't going to be stopped)
	// - Do soft and pair handshakes (stopped threads show up as not having access)
	//
	// Of course, that's super dangerous, since who knows what locks are being held by stopped threads. For sure,
	// you won't want to stop the world and acquire locks that would be held in a running context.
	//
	// Only one thread can stop the world at any time. Hard handshakes use the StopTheWorld mechanism, so if
	// a hard handshake is happening then stop-the-world blocks and vice-versa.
	//
	// FIXME: We should allow stopping just a subset of threads. For example, we want to be able to hard
	// handshake all but the GC threads.
	COREUOBJECT_API FStoppedWorld StopTheWorld() const;

protected:
	FIOContext() = delete;
	FIOContext& operator=(const FIOContext&) = delete;

	void CheckIOInvariants() const
	{
		checkSlow(!GetImpl()->HasAccess());
	}

	void DieIfInvariantsBroken() const;
	void CheckInvariants() const
	{
		CheckBaseInvariants();
		CheckIOInvariants();
	}

	FIOContext(FContextImpl* Impl, EIsInHandshake IsInHandshake)
		: FContext(Impl, IsInHandshake)
	{
		CheckIOInvariants();
	}

private:
	friend struct FRunningContext;

	FIOContext(const FRunningContext& Other);
};

// Our barriers need to be able to run without being passed an FAccessContext in some cases, like copy constructors and operator=.
struct FAccessContextPromise
{
	FAccessContextPromise() = default;
};

// Having an access context means:
//
// - You can access the heap.
// - You cannot allocate.
// - You cannot handshake.
// - You cannot relinquish access.
struct FAccessContext : FContext
{
	FAccessContext(const FAccessContext& Other)
		: FContext(Other)
	{
		CheckAccessInvariants();
	}

	FAccessContext(const FAccessContextPromise& Other)
		: FContext(FContextImpl::GetCurrentImpl(), EIsInHandshake::No)
	{
		CheckAccessInvariants();
	}

	void StopAllocators() const
	{
		GetImpl()->StopAllocators();
	}

	FMarkStack& GetMarkStack() const
	{
		return GetImpl()->MarkStack;
	}

	void RunWriteBarrier(VCell* Cell) const
	{
		GetImpl()->RunWriteBarrier(Cell);
	}

	void RunWriteBarrierNonNull(const VCell* Cell) const
	{
		GetImpl()->RunWriteBarrierNonNull(Cell);
	}

	void RunWriteBarrierNonNullDuringMarking(VCell* Cell) const
	{
		GetImpl()->RunWriteBarrierNonNullDuringMarking(Cell);
	}

	void RunWriteBarrierDuringMarking(VCell* Cell) const
	{
		GetImpl()->RunWriteBarrierDuringMarking(Cell);
	}

	VCell* RunWeakReadBarrier(VCell* Cell) const
	{
		return GetImpl()->RunWeakReadBarrier(Cell);
	}

	VCell* RunWeakReadBarrierUnmarkedWhenActive(VCell* Cell) const
	{
		return GetImpl()->RunWeakReadBarrierUnmarkedWhenActive(Cell,
			[this](const VCell* Cell) { GetImpl()->MarkStack.MarkNonNull(Cell); });
	}

	void RunAuxWriteBarrier(void* Aux) const
	{
		GetImpl()->RunAuxWriteBarrier(Aux);
	}

	void RunAuxWriteBarrierNonNull(const void* Aux) const
	{
		GetImpl()->RunAuxWriteBarrierNonNull(Aux);
	}

	void RunAuxWriteBarrierNonNullDuringMarking(void* Aux) const
	{
		GetImpl()->RunAuxWriteBarrierNonNullDuringMarking(Aux);
	}

	void RunAuxWriteBarrierDuringMarking(void* Aux) const
	{
		GetImpl()->RunAuxWriteBarrierDuringMarking(Aux);
	}

	void* RunAuxWeakReadBarrier(void* Aux) const
	{
		return GetImpl()->RunAuxWeakReadBarrier(Aux);
	}

	void* RunAuxWeakReadBarrierUnmarkedWhenActive(void* Aux) const
	{
		return GetImpl()->RunWeakReadBarrierUnmarkedWhenActive(Aux,
			[this](const void* Aux) { GetImpl()->MarkStack.MarkAuxNonNull(Aux); });
	}

	FTransaction* CurrentTransaction() const
	{
		return GetImpl()->CurrentTransaction();
	}

	void SetCurrentTransaction(FTransaction* Transaction) const
	{
		GetImpl()->SetCurrentTransaction(Transaction);
	}

protected:
	friend struct FContextImpl;

	FAccessContext() = delete;
	FAccessContext& operator=(const FAccessContext&) = delete;

	FAccessContext(FContextImpl* Impl, EIsInHandshake IsInHandshake)
		: FContext(Impl, IsInHandshake)
	{
		CheckAccessInvariants();
	}
	void CheckAccessInvariants() const
	{
		checkSlow(GetImpl()->HasAccess() || IsInHandshake() == EIsInHandshake::Yes);
	}
};

// Engine code that doesn't carry around the context but knows that it's in a running state can use this.
// Sadly, it means that a TLS lookup is required if we really need the context. Code can postpone the context
// lookup by carrying around `FRunningContextPromise`. It's the coercion from the promise to an actual context
// that does the TLS lookup.
struct FRunningContextPromise
{
	FRunningContextPromise() = default;
};

// Having a running context means:
//
// - You can access the heap.
// - You cannot allocate, but you can get yourself an allocation context and then allocate.
// - You can handshake.
// - You can relinquish access.
//
// Running contexts are the default for engine code, so that we only relinquish/acquire heap access around
// blocking operations.
struct FRunningContext : FAccessContext
{
	FRunningContext(const FRunningContext& Other)
		: FAccessContext(Other)
	{
		CheckRunningInvariants();
	}

	FRunningContext(const FRunningContextPromise& Promise)
		: FAccessContext(FContextImpl::GetCurrentImpl(), EIsInHandshake::No)
	{
		CheckRunningInvariants();
	}

	// We don't want RunningContexts to be coerced from AccessContexts, because RunningContexts have higher privilege (they can handshake and allocate via
	// AllocationContexts).
	FRunningContext(const FAccessContext&) = delete;

	// We don't want RunningContexts to be coerced from AllocationContexts, because RunningContexts have higher privilege (they can handshake).
	FRunningContext(const FAllocationContext&) = delete;

	// Create the context for this thread and run some code in it with heap access. You can only have one
	// context created per thread.
	template <typename TFunc>
	static void Create(const TFunc& Func)
	{
		FIOContext::Create([&Func](FIOContext Context) {
			Context.AcquireAccess(Func);
		});
	}

	template <typename TFunc>
	void RelinquishAccess(const TFunc& Func) const
	{
		CheckInvariants();
		GetImpl()->ExitConservativeStack([this, &Func]() {
			GetImpl()->RelinquishAccess();
			Func(FIOContext(*this));
			GetImpl()->AcquireAccess();
		});
		CheckInvariants();
	}

	void CheckForHandshake() const
	{
		CheckInvariants();
		GetImpl()->CheckForHandshake();
	}

private:
	friend struct FIOContext;

	FRunningContext(const FIOContext& Other)
		: FAccessContext(Other.GetImpl(), EIsInHandshake::No)
	{
		CheckRunningInvariants();
	}

protected:
	friend struct FContextImpl;

	FRunningContext(FContextImpl* Impl, EIsInHandshake IsInHandshake)
		: FAccessContext(Impl, IsInHandshake)
	{
		CheckRunningInvariants();
	}

	void CheckRunningInvariants() const
	{
		checkSlow(IsInHandshake() == EIsInHandshake::No);
	}

	void CheckInvariants() const
	{
		CheckBaseInvariants();
		CheckAccessInvariants();
		CheckRunningInvariants();
	}
};

// Having an allocation context means:
//
// - You can access the heap.
// - You can allocate.
// - You cannot handshake.
// - You cannot relinquish access.
//
// It's important to stay in an allocation context from the point where the object is allocated to the point
// where it is fully constructed. So, T::New functions and VCell constructors should always take an allocation
// context!
struct FAllocationContext : FAccessContext
{
	FAllocationContext(const FAllocationContext& Other)
		: FAccessContext(Other)
	{
		CheckAllocationInvariants();
	}
	FAllocationContext(const FRunningContext& Other)
		: FAccessContext(Other)
	{
		CheckAllocationInvariants();
	}

	// We don't want AllocationContexts to be coerced from AccessContexts, because AllocationContexts have higher privilege (they can allocate).
	FAllocationContext(const FAccessContext&) = delete;

	~FAllocationContext()
	{
		CheckInvariants();
	}

	// It's preferable to call this instead of using Allocate(FHeap::FastSpace) because it's faster. However, it's not wrong to say
	// Allocate(FHeap::FastSpace).
	std::byte* AllocateFastCell(size_t NumBytes) const
	{
		CheckInvariants();
		return GetImpl()->AllocateFastCell(NumBytes);
	}

	std::byte* TryAllocateFastCell(size_t NumBytes) const
	{
		CheckInvariants();
		return GetImpl()->TryAllocateFastCell(NumBytes);
	}

	std::byte* AllocateAuxCell(size_t NumBytes) const
	{
		CheckInvariants();
		return GetImpl()->AllocateAuxCell(NumBytes);
	}

	std::byte* TryAllocateAuxCell(size_t NumBytes) const
	{
		CheckInvariants();
		return GetImpl()->TryAllocateAuxCell(NumBytes);
	}

	// Special reservation for EmergentTypes where offset fits in 32 bits.
	std::byte* AllocateEmergentType(size_t NumBytes) const
	{
		CheckInvariants();
		return FHeap::EmergentSpace->Allocate(NumBytes);
	}

	std::byte* TryAllocate(FSubspace* Subspace, size_t NumBytes) const
	{
		CheckInvariants();
		return Subspace->TryAllocate(NumBytes);
	}
	std::byte* TryAllocate(FSubspace* Subspace, size_t NumBytes, size_t Alignment) const
	{
		CheckInvariants();
		return Subspace->TryAllocate(NumBytes, Alignment);
	}
	std::byte* Allocate(FSubspace* Subspace, size_t NumBytes) const
	{
		CheckInvariants();
		return Subspace->Allocate(NumBytes);
	}
	std::byte* Allocate(FSubspace* Subspace, size_t NumBytes, size_t Alignment) const
	{
		CheckInvariants();
		return Subspace->Allocate(NumBytes, Alignment);
	}

protected:
	void CheckAllocationInvariants() const
	{
		checkSlow(IsInHandshake() == EIsInHandshake::No);
	}

	void CheckInvariants() const
	{
		CheckBaseInvariants();
		CheckAccessInvariants();
		CheckAllocationInvariants();
	}
};

struct FHandshakeContext : FAccessContext
{
	FHandshakeContext(const FHandshakeContext& Other) = default;
	COREUOBJECT_API FStopRequest RequestStop() const;

	void MarkReferencedCells() const
	{
		GetImpl()->MarkReferencedCells();
	}

private:
	friend struct FContextImpl;
	FHandshakeContext(FContextImpl* Impl)
		: FAccessContext(Impl, EIsInHandshake::Yes)
	{
	}
};

struct FStopRequest : FAccessContext
{
	void CancelStop() const
	{
		GetImpl()->CancelStop();
	}

private:
	friend struct FHandshakeContext;
	FStopRequest(FHandshakeContext Context)
		: FAccessContext(Context)
	{
	}
};

inline FIOContext::FIOContext(const FRunningContext& Other)
	: FContext(Other)
{
	checkSlow(!GetImpl()->HasAccess());
}

template <typename TFunc>
void FIOContext::AcquireAccess(const TFunc& Func) const
{
	GetImpl()->AcquireAccess();
	GetImpl()->EnterConservativeStack([this, &Func]() {
		Func(FRunningContext(*this));
	});
	GetImpl()->RelinquishAccess();
}

} // namespace Verse
#endif // WITH_VERSE_VM
