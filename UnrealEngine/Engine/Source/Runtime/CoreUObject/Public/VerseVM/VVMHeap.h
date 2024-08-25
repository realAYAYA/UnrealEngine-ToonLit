// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"
#include "Templates/AlignmentTemplates.h"
#include "Templates/TypeCompatibleBytes.h"
#include "VVMLog.h"
#include "verse_heap_config_ue.h"
#include <atomic>
#include <cstddef>

class FThread;

namespace UE
{
class FConditionVariable;
class FMutex;
} // namespace UE

extern "C" struct pas_heap;

namespace Verse
{
class FHeapIterationSet;
struct FCollectionCycleRequest;
struct FGlobalHeapRoot;
struct FGlobalHeapCensusRoot;
struct FHeapPageHeader;
struct FIOContext;
struct FMarkStack;
class FSubspace;
struct VEmergentType;

template <typename T>
struct alignas(alignof(T)) TNeverDestroyed;

enum class EWeakBarrierState
{
	Inactive,
	MarkOnRead,
	AttemptingToTerminate,
	CheckMarkedOnRead
};

class FHeap final
{
public:
	// Must call this before doing anything with the heap.
	COREUOBJECT_API static void Initialize();

	// Space for objects that are fast to GC: they have no destructors and do not require census. Ideally most
	// objects we allocate dynamically are fast.
	COREUOBJECT_API static FSubspace* FastSpace;

	// Same as FastSpace above, except allocations in this space are NOT put onto the mark stack.
	// It is up to the callers who allocated into this space to handle marking anything those allocations contain.
	COREUOBJECT_API static FSubspace* AuxSpace;

	// Space for objects that require destructors. It's fine for objects to have destructors so long as those objects
	// are relatively infrequently allocated (they have low churn rate). It's fine for this space to get large so long
	// as churn rate stays low.
	//
	// Destructors will be called from any thread. It's possible for destructors to be called on the slow path of
	// allocation. It's possible for them to be called from some GC worker thread.
	COREUOBJECT_API static FSubspace* DestructorSpace;

	// Space for objects that require census. Census is a heap iteration that happens after marking but before
	// sweeping, where every live object in the census space has some callback called. This is mostly for weak handles.
	// It's ideal for performance if this space stays small, but it's fine for the churn rate to be high.
	COREUOBJECT_API static FSubspace* CensusSpace;

	// Space for objects that require both destruction and census.
	COREUOBJECT_API static FSubspace* DestructorAndCensusSpace;

	// Special space for emergent types. It's a limited to make a 32 bit offset enough.
	COREUOBJECT_API static FSubspace* EmergentSpace;

	// Use this set to iterate destructors. In the future, if we have spaces that require destruction other than the
	// destructor space (like if we need a DestructorAndCensusSpace), then this set will contain all of them.
	// That's important because there is some constant overhead to iterating any iteration set, but iteration sets
	// can contain any number of spaces.
	COREUOBJECT_API static FHeapIterationSet* DestructorIterationSet;

	// Census set, for now just containing the CensusSpace. If we have multiple spaces that require Census (like if
	// we need a DestructorAndCensusSpace), then this will contain all of them.
	COREUOBJECT_API static FHeapIterationSet* CensusIterationSet;

	COREUOBJECT_API static size_t MinimumTrigger;

	static bool IsMarking()
	{
		return bIsMarking;
	}

	static bool IsCollecting()
	{
		return bIsCollecting;
	}

	static EWeakBarrierState GetWeakBarrierState()
	{
		return WeakBarrierState;
	}

	static std::atomic<uint32>* GetMarkBitWord(const void* Ptr)
	{
		uintptr_t Address = reinterpret_cast<uintptr_t>(Ptr);
		uintptr_t ChunkBase = Address & ~(static_cast<uintptr_t>(VERSE_HEAP_CHUNK_SIZE) - 1);
		uintptr_t ChunkOffset = Address & (static_cast<uintptr_t>(VERSE_HEAP_CHUNK_SIZE) - 1);
		uintptr_t BitIndex = ChunkOffset >> VERSE_HEAP_MIN_ALIGN_SHIFT;
		uintptr_t WordIndex = BitIndex >> 5;
		return reinterpret_cast<std::atomic<uint32>*>(ChunkBase) + WordIndex;
	}

	static uint32 GetMarkBitIndex(const void* Ptr)
	{
		uintptr_t Address = reinterpret_cast<uintptr_t>(Ptr);
		checkSlow(!(Address & (static_cast<uintptr_t>(VERSE_HEAP_MIN_ALIGN) - 1)));
		return static_cast<uint32>(Address >> VERSE_HEAP_MIN_ALIGN_SHIFT);
	}

	static uint32 GetMarkBitMask(const void* Ptr)
	{
		return static_cast<uint32>(1) << (GetMarkBitIndex(Ptr) & 31);
	}

	static bool IsMarked(const void* Ptr)
	{
		std::atomic<uint32>* Word = GetMarkBitWord(Ptr);
		uint32 Mask = GetMarkBitMask(Ptr);
		return Word->load(std::memory_order_relaxed) & Mask;
	}

	static uint32 EmergentTypePtrToOffset(const VEmergentType* EmergentType)
	{
		uint32 Offset = (BitCast<uint8*>(EmergentType) - BitCast<uint8*>(FHeap::EmergentTypeBase)) / FHeap::EmergentAlignment;
		checkf(EmergentType == EmergentTypeOffsetToPtr(Offset),
			TEXT("EmergentType could not be translated to an offset (pointer 0x%p, offset 0x%x => 0x%p)."),
			EmergentType,
			Offset,
			EmergentTypeOffsetToPtr(Offset));
		return Offset;
	}

	static VEmergentType* EmergentTypeOffsetToPtr(uint32 Offset)
	{
		return BitCast<VEmergentType*>(BitCast<uint8*>(FHeap::EmergentTypeBase) + static_cast<size_t>(Offset) * FHeap::EmergentAlignment);
	}

	static void ReportAllocatedNativeBytes(ptrdiff_t Bytes)
	{
		if (Bytes)
		{
			LiveNativeBytes += static_cast<size_t>(Bytes);
			V_DIE_IF(static_cast<ptrdiff_t>(LiveNativeBytes) < 0);
		}
	}

	static void ReportDeallocatedNativeBytes(size_t Bytes)
	{
		size_t AllocatedBytes = static_cast<size_t>(-static_cast<ptrdiff_t>(Bytes));
		V_DIE_IF(AllocatedBytes == Bytes);
		ReportAllocatedNativeBytes(AllocatedBytes);
	}

	static size_t GetLiveNativeBytes()
	{
		return LiveNativeBytes;
	}

	// If a collection cycle is going right now, then do nothing except return a request object that
	// allows us to wait for when it's done.
	//
	// If a collection cycle is not going right now, then start one, and return a request object that
	// allows us to wait for when it's done.
	//
	// The fact that this won't start a GC cycle if one is already going is quite significant. GC
	// cycles float garbage: anything allocated in this cycle will not be freed in this cycle. So, if
	// you create some garbage, call this function, and then wait, then you're not guaranteed that the
	// garbage you created will be deleted. It might float!
	//
	// This function forms the basis of GC triggers. If you want to trigger GC when there is too much
	// of something (like live bytes in the heap), then use this.
	COREUOBJECT_API static FCollectionCycleRequest StartCollectingIfNotCollecting();

	// If a collection cycle is going right now, then request another collection cycle to start
	// immediately after this one. Note that this cycle might be unique to us, since if multiple calls
	// happen to this function within a cycle, then they'll all request the same fresh cycle.
	//
	// If a collection cycle is not going right now, then start one.
	//
	// Either way, return a request object that allows us to wait for the cycle we started.
	//
	// The point of this function is that it allows you to handle floating garbage. If you allocate
	// some garbage, call this function, and then wait, then you're guaranteed that the garbage you
	// created will get collected. It may float in the *current* cycle (if there was one), but it
	// cannot possibly float in the fresh cycle that this requested.
	//
	// Hence this function forms the basis of STW "GC now" equivalents. Like, if you wanted to
	// "synchronously GC" in a concurrent GC, then that means calling this function and then waiting
	// for the request.
	COREUOBJECT_API static FCollectionCycleRequest RequestFreshCollectionCycle();

	static uint64 GetCompletedCycleVersion()
	{
		return CompletedCycleVersion;
	}

	// Request that the collector allow external control. This will crash if the GC is already externally
	// controlled.
	COREUOBJECT_API static void EnableExternalControl(FIOContext Context);

	// Disable external control.
	COREUOBJECT_API static void DisableExternalControl();

	static bool IsExternallyControlled()
	{
		return bIsExternallyControlled;
	}

	COREUOBJECT_API static bool IsGCStartPendingExternalSignal();
	COREUOBJECT_API static void ExternallySynchronouslyStartGC(FIOContext Context);

	// To be called from an external control system: Check if the GC wants to terminate. This means that the
	// Verse GC has no more objects to mark. If the Verse GC has no more objects to mark, and other GCs in
	// the system also have no more objects to mark, then it's safe to permit GC termination.
	//
	// This can only be called if the GC is marking. Otherwise it crashes.
	COREUOBJECT_API static bool IsGCTerminationPendingExternalSignal(FIOContext Context);

	// To be called from an external control system: Permit the GC to terminate. This crashes if the Verse
	// GC is not pending termination.
	//
	// NOTE: It's only safe to use this if there is no concurrent mutator activity while you're trying to do
	// this.
	COREUOBJECT_API static void ExternallySynchronouslyTerminateGC(FIOContext Context);

	// To be called from an external control system: Permit the GC to terminate. This returns true if the Verse
	// GC was pending termination and was able to terminate, false otherwise.
	COREUOBJECT_API static bool TryToExternallySynchronouslyTerminateGC(FIOContext Context);

	// External GCs are expected to create their own FMarkStack instance(s) and mark VCells using that API.
	// Then, eventually, passing those MarkStacks to Verse using this AddExternalMarkStack() function. This
	// ensures that the Verse GC is aware of external marking activity in its heap.
	//
	// Only use this function with EnableExternalControl().
	//
	// Never call this function before ExternallySignalGCStartAndWaitUntilItDoes().
	//
	// Never pass a MarkStack that had objects from a previous collection (i.e. from before
	// ExternallySignalGCStartAndWaitUntilItDoes()).
	//
	// Never call ExternallySynchronouslyTerminateGC() if you have any MarkStacks that you haven't yet passed to
	// the Verse GC using this AddExternalMarkStack() function.
	//
	// This function clears the contents of the MarkStack you give it.
	COREUOBJECT_API static void AddExternalMarkStack(FMarkStack&&);

	// It doesn't mean it's actually a valid cell, but it does mean
	// that libpas owns this address.
	COREUOBJECT_API static bool OwnsAddress(void*);

	static double GetTotalTimeSpentCollecting()
	{
		return TotalTimeSpentCollecting;
	}

private:
	friend struct FCollectionCycleRequest;
	friend struct FContextImpl; // Usually, FContextImpl just uses FHeap API, but sometimes it's not practical.
	friend struct FGlobalHeapRoot;
	friend struct FGlobalHeapCensusRoot;
	friend struct FMarkStack;
	friend struct FWeakKeyMapGuard;

	FHeap() = delete;

	static bool IsWithoutThreadingDuringCollection();

	// If we have threading, this returns false. If we don't have threading, it asserts we aren't GCing right now and checks if we should
	// turn on threading.
	static bool NormalizeWithoutThreadingAtCollectionStart();

	static void CollectorThreadMain();
	static void CollectorThreadBody(FIOContext Context);

	static void WaitForTrigger(FIOContext Context);

	static void RunCollectionCycle(FIOContext Context);
	static void RunPreMarking(FIOContext Context);
	static void RunPostMarking(FIOContext Context);
	static void MarkForExternalControlWithoutThreading(FIOContext Context);

	static void Terminate();
	static void CancelTermination();

	static void BeginCollection(FIOContext Context);
	static void MarkRoots(FIOContext Context);
	static void Mark(FIOContext Context);
	static bool AttemptToTerminate(FIOContext Context); // Returns true if we did terminate.
	static void ConductCensus(FIOContext Context);
	static void RunDestructors(FIOContext Context);
	static void Sweep(FIOContext Context);
	static void EndCollection(FIOContext Context);

	static void LiveBytesTriggerCallback();
	static void CensusCallback(void* Object, void* Arg);
	static void DestructorCallback(void* Object, void* Arg);

	static void CheckCycleTriggerInvariants();
	static FCollectionCycleRequest RequestCollectionCycle(uint64 DesiredRequestCompleteDelta);

	static bool IsGCTerminationPendingExternalSignalImpl();

	static void ReportMarkedNativeBytes(size_t Bytes)
	{
		MarkedNativeBytes += Bytes;
		V_DIE_IF(static_cast<ptrdiff_t>(MarkedNativeBytes) < 0);
	}

	static bool bWithoutThreading;

	static FThread* CollectorThread;

	static UE::FMutex GlobalRootMutex;
	static TNeverDestroyed<TArray<FGlobalHeapRoot*>> GlobalRoots;
	static UE::FMutex GlobalCensusRootMutex;
	static TNeverDestroyed<TArray<FGlobalHeapCensusRoot*>> GlobalCensusRoots;

	static UE::FMutex WeakKeyMapsMutex;
	static TNeverDestroyed<TArray<FHeapPageHeader*>> WeakKeyMapsByHeader;

	// Controls all of the fields below.
	COREUOBJECT_API static UE::FMutex Mutex;
	static UE::FConditionVariable ConditionVariable;

	// Invariant: RequestedCycleVersion >= CompletedCycleVersion
	//
	// If RequestedCycleVersion > CompletedCycleVersion, then we should run a collection, and increment
	// CompletedCycleVersion once finished.
	//
	// If RequestedCycleVersion == CompletedCycleVersion, then we should not run a collection.
	//
	// To request a collection, increment RequestedCycleVersion.
	//
	// To wait for our requested collection to finish, wait for CompletedCycleVersion to catch up to
	// the value we incremented RequestedCycleVersion to.
	static uint64 RequestedCycleVersion;
	static uint64 CompletedMarkingCycleVersion;
	static uint64 CompletedCycleVersion;

	COREUOBJECT_API static bool bIsCollecting;
	COREUOBJECT_API static bool bIsMarking;
	COREUOBJECT_API static EWeakBarrierState WeakBarrierState;

	static size_t LiveCellBytesAtStart;

	static std::atomic<size_t> MarkedNativeBytes;
	COREUOBJECT_API static std::atomic<size_t> LiveNativeBytes;

	static TNeverDestroyed<FMarkStack> MarkStack; // Must hold Mutex to access safely.
	static unsigned NumThreadsToScanStackManually;

	static bool bIsExternallyControlled;
	static bool bIsGCReadyForExternalMarking;
	static bool bIsGCMarkingExternallySignaled;
	static bool bIsGCTerminationWaitingForExternalSignal;
	static bool bIsGCTerminatingExternally;

	static bool bIsTerminated;

	static bool bIsInitialized;

	COREUOBJECT_API static double TotalTimeSpentCollecting;
	static double TimeOfPreMarking;

	// Cached value of the base of the EmergentSpace (i.e. EmergentSpace->GetBase()).
	COREUOBJECT_API static std::byte* EmergentTypeBase;

	static constexpr size_t EmergentAlignment = 16;
	static constexpr size_t EmergentReservationSize = 16 * 1024 * 1024;
};

} // namespace Verse
#endif // WITH_VERSE_VM
