// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMHeap.h"
#include "Async/UniqueLock.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/Thread.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Fork.h"
#include "Misc/Parse.h"
#include "UObject/GarbageCollection.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMAbstractVisitor.h"
#include "VerseVM/VVMCell.h"
#include "VerseVM/VVMCollectionCycleRequest.h"
#include "VerseVM/VVMContext.h"
#include "VerseVM/VVMGlobalHeapCensusRoot.h"
#include "VerseVM/VVMGlobalHeapRoot.h"
#include "VerseVM/VVMHeapIterationSet.h"
#include "VerseVM/VVMLog.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMNeverDestroyed.h"
#include "VerseVM/VVMSubspace.h"
#include "pas_scavenger_ue.h"
#include "verse_heap_mark_bits_page_commit_controller_ue.h"
#include "verse_heap_ue.h"

namespace Verse
{

FSubspace* FHeap::FastSpace;
FSubspace* FHeap::AuxSpace;
FSubspace* FHeap::DestructorSpace;
FSubspace* FHeap::CensusSpace;
FSubspace* FHeap::DestructorAndCensusSpace;
FSubspace* FHeap::EmergentSpace;
FHeapIterationSet* FHeap::DestructorIterationSet;
FHeapIterationSet* FHeap::CensusIterationSet;
std::byte* FHeap::EmergentTypeBase;
size_t FHeap::MinimumTrigger = 0;
bool FHeap::bWithoutThreading;
FThread* FHeap::CollectorThread;
UE::FMutex FHeap::GlobalRootMutex;
TNeverDestroyed<TArray<FGlobalHeapRoot*>> FHeap::GlobalRoots;
UE::FMutex FHeap::GlobalCensusRootMutex;
TNeverDestroyed<TArray<FGlobalHeapCensusRoot*>> FHeap::GlobalCensusRoots;
UE::FMutex FHeap::WeakKeyMapsMutex;
TNeverDestroyed<TArray<FHeapPageHeader*>> FHeap::WeakKeyMapsByHeader;
UE::FMutex FHeap::Mutex;
UE::FConditionVariable FHeap::ConditionVariable;
uint64 FHeap::RequestedCycleVersion;
uint64 FHeap::CompletedMarkingCycleVersion;
uint64 FHeap::CompletedCycleVersion;
bool FHeap::bIsCollecting;
bool FHeap::bIsMarking;
EWeakBarrierState FHeap::WeakBarrierState = EWeakBarrierState::Inactive;
size_t FHeap::LiveCellBytesAtStart;
std::atomic<size_t> FHeap::LiveNativeBytes;
std::atomic<size_t> FHeap::MarkedNativeBytes;
TNeverDestroyed<FMarkStack> FHeap::MarkStack;
unsigned FHeap::NumThreadsToScanStackManually;
bool FHeap::bIsExternallyControlled;
bool FHeap::bIsGCReadyForExternalMarking;
bool FHeap::bIsGCMarkingExternallySignaled;
bool FHeap::bIsGCTerminationWaitingForExternalSignal;
bool FHeap::bIsGCTerminatingExternally;
bool FHeap::bIsTerminated;
bool FHeap::bIsInitialized;
double FHeap::TotalTimeSpentCollecting;
double FHeap::TimeOfPreMarking;

void FHeap::Initialize()
{
	if (!bIsInitialized)
	{
		FastSpace = FSubspace::Create();
		AuxSpace = FSubspace::Create();
		DestructorSpace = FSubspace::Create();
		CensusSpace = FSubspace::Create();
		DestructorAndCensusSpace = FSubspace::Create();
		EmergentSpace = FSubspace::Create(EmergentAlignment, EmergentReservationSize);

		DestructorIterationSet = FHeapIterationSet::Create();
		CensusIterationSet = FHeapIterationSet::Create();

		DestructorIterationSet->Add(DestructorSpace);
		DestructorIterationSet->Add(DestructorAndCensusSpace);
		CensusIterationSet->Add(CensusSpace);
		CensusIterationSet->Add(DestructorAndCensusSpace);
		EmergentTypeBase = EmergentSpace->GetBase();

		verse_heap_live_bytes_trigger_threshold = MinimumTrigger;
		verse_heap_live_bytes_trigger_callback = LiveBytesTriggerCallback;

		LiveNativeBytes = 0;
		MarkedNativeBytes = 0;

		verse_heap_did_become_ready_for_allocation();
		bIsInitialized = true;

		if (!FPlatformProcess::SupportsMultithreading())
		{
			bWithoutThreading = true;

			// If we don't have threads then we don't want the scavenger thread starting. Note that this is safe to do even if we somehow started the thread
			// already. This function will return after the scavenger thread is on a path of no return to exiting and after it no longer it holds any locks.
			// So, if UE does the forking hacks after this point, we're good to go.
			pas_scavenger_suspend();
		}

		CollectorThread = new FThread(TEXT("Verse GC Thread"), CollectorThreadMain, 0, TPri_Normal, FThreadAffinity(), FThread::Forkable);

		// Fetch the FrankenGC mode directly
		check(GConfig);
		bool bEnableFrankenGC = true;
		GConfig->GetBool(TEXT("/Script/Engine.GarbageCollectionSettings"), TEXT("gc.EnableFrankenGC"), bEnableFrankenGC, GEngineIni);

		// Check for some quick command line options
		if (FParse::Param(FCommandLine::Get(), TEXT("DisableFrankenGC")))
		{
			bEnableFrankenGC = false;
		}
		else if (FParse::Param(FCommandLine::Get(), TEXT("EnableFrankenGC")))
		{
			bEnableFrankenGC = true;
		}

		// Enable/Disable franken GC before cells are created.
		UE::GC::EnableFrankenGCMode(bEnableFrankenGC);
	}
}

bool FHeap::IsWithoutThreadingDuringCollection()
{
	if (bWithoutThreading)
	{
		// We shouldn't have forked during a collection cycle. So, there's no chance that bWithoutThreading is still set and we have become a forked multithread
		// instance.
		V_DIE_IF(FForkProcessHelper::IsForkedMultithreadInstance());
		return true;
	}
	else
	{
		return false;
	}
}

bool FHeap::NormalizeWithoutThreadingAtCollectionStart()
{
	using namespace UE;

	if (bWithoutThreading)
	{
		V_DIE_IF(bIsMarking);
		V_DIE_IF(bIsCollecting);

		// It's possible that we've forked but the GC thread hasn't actually started running yet (it has been started, but its body hasn't had a chance to execute).
		// In that case, we need to ensure that we *don't* go into the GC cycle thinking that we are unthreaded. This logic forces the GC to realize that it's now
		// threaded even if the GC thread hasn't started.
		if (FForkProcessHelper::IsForkedMultithreadInstance())
		{
			TUniqueLock Lock(Mutex);
			V_DIE_IF(bIsMarking);
			V_DIE_IF(bIsCollecting);
			if (bWithoutThreading)
			{
				pas_scavenger_resume();
				bWithoutThreading = false;
			}
			return false;
		}
		else
		{
			return true;
		}
	}
	else
	{
		return false;
	}
}

void FHeap::CollectorThreadMain()
{
	FIOContext::Create(CollectorThreadBody, EContextHeapRole::Collector);
}

void FHeap::CollectorThreadBody(FIOContext Context)
{
	V_DIE_IF(bIsMarking);
	V_DIE_IF(bIsCollecting);

	NormalizeWithoutThreadingAtCollectionStart();

	UE_LOG(LogVerseGC, Verbose, TEXT("GC thread starting!"));
	for (;;)
	{
		WaitForTrigger(Context);
		RunCollectionCycle(Context);
	}
}

void FHeap::WaitForTrigger(FIOContext Context)
{
	using namespace UE;
	TUniqueLock Lock(Mutex);
	V_DIE_UNLESS(RequestedCycleVersion >= CompletedCycleVersion);
	V_DIE_IF(bIsMarking);
	V_DIE_IF(bIsCollecting);
	V_DIE_IF(bIsGCReadyForExternalMarking);
	V_DIE_IF(bIsGCTerminationWaitingForExternalSignal);
	while (RequestedCycleVersion == CompletedCycleVersion)
	{
		ConditionVariable.Wait(Mutex);
	}
}

void FHeap::RunCollectionCycle(FIOContext Context)
{
	V_DIE_IF(bIsMarking);
	V_DIE_IF(bIsCollecting);

	RunPreMarking(Context);

	do
	{
		Mark(Context);
	}
	while (!AttemptToTerminate(Context));

	RunPostMarking(Context);

	V_DIE_IF(bIsMarking);
	V_DIE_IF(bIsCollecting);
}

void FHeap::RunPreMarking(FIOContext Context)
{
	TimeOfPreMarking = FPlatformTime::Seconds();
	BeginCollection(Context);
	MarkRoots(Context);
}

void FHeap::RunPostMarking(FIOContext Context)
{
	UE_LOG(LogVerseGC, Verbose, TEXT("GC marking terminated!"));
	ConductCensus(Context);
	RunDestructors(Context);
	Sweep(Context);
	EndCollection(Context);
	TotalTimeSpentCollecting += FPlatformTime::Seconds() - TimeOfPreMarking;
}

void FHeap::BeginCollection(FIOContext Context)
{
	using namespace UE;

	UE_LOG(LogVerseGC, Verbose, TEXT("GC received trigger for GC %llu"), CompletedCycleVersion + 1);

	V_DIE_UNLESS(RequestedCycleVersion > CompletedCycleVersion);
	V_DIE_IF(bIsMarking);
	V_DIE_IF(bIsCollecting);
	V_DIE_IF(bIsGCReadyForExternalMarking);
	V_DIE_IF(bIsGCTerminationWaitingForExternalSignal);
	V_DIE_UNLESS(WeakBarrierState == EWeakBarrierState::Inactive);

	if (bIsExternallyControlled && !IsWithoutThreadingDuringCollection())
	{
		TUniqueLock Lock(Mutex);
		if (bIsExternallyControlled)
		{
			while (!bIsGCMarkingExternallySignaled && bIsExternallyControlled)
			{
				ConditionVariable.Wait(Mutex);
			}
		}
	}

	// Make sure mark bits are locked (i.e. committed and prevented from being decommitted by the libpas scavenger) before we tell folks to start using them.
	verse_heap_mark_bits_page_commit_controller_lock();

	{
		TUniqueLock Lock(Mutex);
		bIsMarking = true;
		bIsCollecting = true;
		bIsTerminated = false;
	}

	Context.SoftHandshake([](FHandshakeContext) {});

	{
		TUniqueLock Lock(Mutex);
		if (bIsExternallyControlled)
		{
			V_DIE_UNLESS(bIsGCMarkingExternallySignaled);
			bIsGCReadyForExternalMarking = true;
			ConditionVariable.NotifyAll();
		}
	}

	verse_heap_start_allocating_black_before_handshake();
	Context.SoftHandshake([](FHandshakeContext TargetContext) {
		TargetContext.StopAllocators();
	});

	LiveCellBytesAtStart = verse_heap_live_bytes;
	MarkedNativeBytes = 0;

	size_t CurrentLiveNativeBytes = LiveNativeBytes;

	UE_LOG(LogVerseGC, Verbose, TEXT("Starting with live cell bytes %zu, native bytes %zu, total bytes %zu"), LiveCellBytesAtStart, CurrentLiveNativeBytes, LiveCellBytesAtStart + CurrentLiveNativeBytes);
}

void FHeap::MarkRoots(FIOContext Context)
{
	using namespace UE;
	V_DIE_UNLESS(bIsMarking);
	V_DIE_UNLESS(bIsCollecting);
	V_DIE_UNLESS(WeakBarrierState == EWeakBarrierState::Inactive);
	V_DIE_IF(bIsTerminated);
	FMarkStack MyMarkStack;
	{
		FMarkStackVisitor Visitor(MyMarkStack);
		TUniqueLock Lock(GlobalRootMutex);
		for (FGlobalHeapRoot* Root : *GlobalRoots)
		{
			Root->Visit(Visitor);
		}
	}
	{
		TUniqueLock Lock(Mutex);
		MarkStack->Append(MoveTemp(MyMarkStack));
	}
}

void FHeap::Mark(FIOContext Context)
{
	using namespace UE;
	V_DIE_UNLESS(bIsMarking);
	V_DIE_UNLESS(bIsCollecting);
	V_DIE_UNLESS(WeakBarrierState == EWeakBarrierState::Inactive);
	V_DIE_IF(bIsTerminated);
	V_DIE_IF(bIsGCTerminationWaitingForExternalSignal);
	for (;;)
	{
		FMarkStack MyMarkStack;
		{
			TUniqueLock Lock(Mutex);
			MyMarkStack.Append(MoveTemp(*MarkStack));
		}
		if (MyMarkStack.IsEmpty())
		{
			break;
		}
		FMarkStackVisitor Visitor(MyMarkStack);
		while (VCell* Cell = MyMarkStack.Pop())
		{
			Cell->VisitReferences(Visitor);
		}
	}
}

void FHeap::Terminate()
{
	V_DIE_IF(bIsTerminated);
	V_DIE_UNLESS(Mutex.IsLocked());
	V_DIE_UNLESS(MarkStack->IsEmpty());

	// There's no way for more threads to decide to block termination, since that's only something we could have initiated.
	V_DIE_IF(NumThreadsToScanStackManually);
	V_DIE_IF(bIsGCTerminationWaitingForExternalSignal);
	V_DIE_IF(bIsGCReadyForExternalMarking);
	V_DIE_UNLESS(WeakBarrierState == EWeakBarrierState::AttemptingToTerminate);
	// NOTE: It's possible that bIsGCMarkingExternallySignalled is already set.

	bIsMarking = false;
	bIsGCTerminatingExternally = false;
	WeakBarrierState = EWeakBarrierState::CheckMarkedOnRead;

	CheckCycleTriggerInvariants();
	V_DIE_UNLESS(CompletedMarkingCycleVersion == CompletedCycleVersion);
	CompletedMarkingCycleVersion++;
	CheckCycleTriggerInvariants();
	V_DIE_UNLESS(CompletedMarkingCycleVersion == CompletedCycleVersion + 1);

	bIsTerminated = true;
	ConditionVariable.NotifyAll();
}

void FHeap::CancelTermination()
{
	bIsGCTerminationWaitingForExternalSignal = false;
	WeakBarrierState = EWeakBarrierState::Inactive;
}

bool FHeap::AttemptToTerminate(FIOContext Context)
{
	using namespace UE;
	V_DIE_IF(bIsTerminated);
	V_DIE_UNLESS(bIsMarking);
	V_DIE_UNLESS(bIsCollecting);
	V_DIE_IF(bIsGCTerminationWaitingForExternalSignal);

	{
		TUniqueLock Lock(Mutex);
		V_DIE_UNLESS(WeakBarrierState == EWeakBarrierState::Inactive);
		WeakBarrierState = EWeakBarrierState::AttemptingToTerminate;
	}

	Context.SoftHandshake([](FHandshakeContext TargetContext) {
		if (TargetContext.UsesManualStackScanning())
		{
			if (IsWithoutThreadingDuringCollection())
			{
				// If UE is in -nothreading mode before fork, then we assume that:
				//
				// - There is only one context.
				// - That context uses manual stack scanning (since it's the UE context and that one always uses manual stack scanning).
				// - The GC is only triggered at points where there is no stack.
				// - We aren't in a transaction.
				V_DIE_IF(TargetContext.CurrentTransaction());
			}
			else
			{
				// FIXME: Really, what we want, is that this thread isn't even part of the soft handshake. We just tell the thread to do this
				// using normal locking protocol. But this is good enough for now!
				TUniqueLock Lock(Mutex);
				FContextImpl* Impl = TargetContext.GetImpl();
				V_DIE_UNLESS(Impl->StateMutex.IsLocked());
				V_DIE_IF(Impl->bManualStackScanRequested && Impl->bIsInManuallyEmptyStack);
				if (!Impl->bManualStackScanRequested && !Impl->bIsInManuallyEmptyStack)
				{
					NumThreadsToScanStackManually++;
					Impl->bManualStackScanRequested = true;
				}
			}
		}
		else
		{
			TargetContext.MarkReferencedCells();
		}

		// FIXME: Need a test for this in the UsesManualStackScanning() case (i.e. barrier execution).
		TUniqueLock Lock(Mutex);
		MarkStack->Append(MoveTemp(TargetContext.GetMarkStack()));
		ConditionVariable.NotifyAll();
	});

	TUniqueLock Lock(Mutex);
	if (MarkStack->IsEmpty() && WeakBarrierState == EWeakBarrierState::AttemptingToTerminate)
	{
		// If the mark stack is empty, we need to check if there are any threads that are going to scan their stack manually. This is
		// quite powerful. It means that those threads get to block us from terminating, which is useful if there is a foreign GC (like
		// the UE GC) running, and that GC might still find more pointers. It also allows us to block termination until we have a chance
		// to reach end-of-tick (which is great for threads that don't want to have their stack scanned at all, but have end-of-tick
		// points where there is no stack).
		//
		// Note that "threads scanning stack manually" and "threads blocking termination" are the same thing. We will use the terms
		// interchangeably.
		//
		// We want to wait until either there is more work to do in the mark stack, or no threads are blocking termination. Let's double
		// check the conditions here:
		// - Mark stack not empty and there are threads blocking termination.
		//       => We should not wait, since there is work to do on the mark stack, so we wouldn't terminate anyway.
		// - Mark stack not empty and there are no threads blocking termination.
		//       => We should not wait.
		// - Mark stack empty and there are threads blocking termination.
		//       => *We should wait*, since we have no work to do (mark stack is empty) but some threads want us to hold off on terminating.
		// - Mark stack empty and there are no threads blocking termination.
		//       => We should not wait, since there is no more work to do, so we should just terminate.
		if (IsWithoutThreadingDuringCollection())
		{
			V_DIE_IF(NumThreadsToScanStackManually);

			if (bIsExternallyControlled)
			{
				bIsGCTerminationWaitingForExternalSignal = true;
				return false;
			}
		}
		else
		{
			if (bIsExternallyControlled)
			{
				bIsGCTerminationWaitingForExternalSignal = true;
				V_DIE_IF(bIsGCTerminatingExternally);
			}

			// This loop will break early as soon as there is anything on the mark stack. This is OK.
			//
			// This is OK in the case that there are still threads that need to scan stack manually. We're fine with threads being "stuck"
			// in that situation from one AttemptToTerminate to another. Part of the mechanism that makes this fine is that each thread
			// has a bManualStackScanRequested bit that tells it if it is stuck in this way. AttemptToTerminate won't ask a thread for a
			// second manual stack scan if that bit is already set. Also, NumThreadsToScanStackManually will stay nonzero, so
			// AttemptToTerminate will know that it cannot really terminate even if the mark stack ever becomes empty.
			//
			// This is OK in the case that bIsGCTerminationWaitingForExternalSignal is true. This is subtle. The external GC controller will only
			// be led to believe that the Verse GC is ready to terminate if IsGCTerminationPendingExternalSignal() returns true. But that will
			// return false if the MarkStack is not empty, even if bIsGCTerminationWaitingForExternalSignal is true. So,
			// bIsGCTerminationWaitingForExternalSignal being true just means that it's *possible* for IsGCTerminationPendingExternalSignal() to return
			// true if it *also* sees an empty MarkStack. So, the moment that the mark stack becomes nonempty, the external controller will
			// know that we're not terminating. Also, if we fall out of this loop because of a nonempty MarkStack, we will clear
			// bIsGCTerminationWaitingForExternalSignal. That ensures that if the mark stack becomes nonempty, and we return from AttemptToTerminate,
			// then even if the MarkStack becomes empty again as part of normal GC workflow, then the external GC controller will not think
			// that we are terminating (since bIsGCTerminationWaitingForExternalSignal will be false).
			while (!bIsTerminated && MarkStack->IsEmpty() && (NumThreadsToScanStackManually || bIsGCTerminationWaitingForExternalSignal) && WeakBarrierState == EWeakBarrierState::AttemptingToTerminate)
			{
				ConditionVariable.Wait(Mutex);
			}
		}
	}
	if (bIsTerminated)
	{
		// External control does termination from the main thread so that there's no chance of a concurrent mutator undoing termination once
		// the external controller has made up their minds. That's mostly for the benefit of weak barrier state.
		return true;
	}
	else if (MarkStack->IsEmpty() && WeakBarrierState == EWeakBarrierState::AttemptingToTerminate)
	{
		Terminate();
		return true;
	}
	else
	{
		CancelTermination();
		return false;
	}
}

void FHeap::ConductCensus(FIOContext Context)
{
	using namespace UE;

	V_DIE_IF(bIsMarking);
	V_DIE_UNLESS(bIsCollecting);
	V_DIE_UNLESS(bIsTerminated);
	V_DIE_UNLESS(WeakBarrierState == EWeakBarrierState::CheckMarkedOnRead);

	// This part could have been factored out into a global census root, except I anticipate we're likely to do special
	// crazy optimizations for weak keys (like parallelizing this loop).
	//
	// We need to do this at some point after marking but before sweeping, ideally before destructors, so that means
	// doing it at some point during census. It's OK for this loop to happen after the rest of census though. In common
	// cases, we should expect this loop to be very fast because there aren't that many things in it.
	{
		TArray<FHeapPageHeader*> MyWeakKeyMapsByHeader;
		{
			TUniqueLock Lock(WeakKeyMapsMutex);
			MyWeakKeyMapsByHeader.Append(MoveTemp(*WeakKeyMapsByHeader));
		}
		MyWeakKeyMapsByHeader.RemoveAll([](FHeapPageHeader* Header) -> bool {
			FWeakKeyMapGuard Guard(Header);
			return Guard.ConductCensus();
		});
		if (!MyWeakKeyMapsByHeader.IsEmpty())
		{
			TUniqueLock Lock(WeakKeyMapsMutex);
			MyWeakKeyMapsByHeader.Append(*WeakKeyMapsByHeader);
			*WeakKeyMapsByHeader = MoveTemp(MyWeakKeyMapsByHeader);
		}
	}

	{
		TUniqueLock Lock(GlobalCensusRootMutex);
		for (FGlobalHeapCensusRoot* Root : *GlobalCensusRoots)
		{
			Root->ConductCensus();
		}
	}

	CensusIterationSet->StartIterateBeforeHandshake();
	Context.SoftHandshake([](FHandshakeContext TargetContext) {
		TargetContext.StopAllocators();
	});
	size_t IterationSize = CensusIterationSet->StartIterateAfterHandshake();
	// FIXME: This should be parallel. It's easy to make this hella parallel because the only requirement is
	// that we IterateRange for all indices, using however many calls to IterateRange we want. It would be
	// efficient even if each IterateRange call was for a range of size 1 and we used a global counter to hand
	// out work. It would also be efficient to shard. It's like, whatever.
	CensusIterationSet->IterateRange(0, IterationSize, verse_heap_iterate_marked, CensusCallback, nullptr);
	CensusIterationSet->EndIterate();
}

void FHeap::RunDestructors(FIOContext Context)
{
	V_DIE_IF(bIsMarking);
	V_DIE_UNLESS(bIsCollecting);
	V_DIE_UNLESS(bIsTerminated);
	V_DIE_UNLESS(WeakBarrierState == EWeakBarrierState::CheckMarkedOnRead);
	DestructorIterationSet->StartIterateBeforeHandshake();
	Context.SoftHandshake([](FHandshakeContext TargetContext) {
		TargetContext.StopAllocators();
	});
	size_t IterationSize = DestructorIterationSet->StartIterateAfterHandshake();
	// FIXME: This should be parallel.
	DestructorIterationSet->IterateRange(
		0, IterationSize, verse_heap_iterate_unmarked, DestructorCallback, nullptr);
	DestructorIterationSet->EndIterate();
}

void FHeap::Sweep(FIOContext Context)
{
	using namespace UE;
	V_DIE_IF(bIsMarking);
	V_DIE_UNLESS(bIsCollecting);
	V_DIE_UNLESS(bIsTerminated);
	{
		TUniqueLock Lock(Mutex);
		V_DIE_UNLESS(WeakBarrierState == EWeakBarrierState::CheckMarkedOnRead);
		WeakBarrierState = EWeakBarrierState::Inactive;
	}
	verse_heap_start_sweep_before_handshake();
	Context.SoftHandshake([](FHandshakeContext TargetContext) {
		TargetContext.StopAllocators();
	});
	size_t SweepSize = verse_heap_start_sweep_after_handshake();
	// FIXME: This should be parallel.
	verse_heap_sweep_range(0, SweepSize);
	verse_heap_end_sweep();
}

void FHeap::EndCollection(FIOContext Context)
{
	using namespace UE;
	V_DIE_IF(bIsMarking);
	V_DIE_UNLESS(bIsCollecting);
	V_DIE_UNLESS(bIsTerminated);

	// Make it possible for the libpas scavenger to decommit the mark bits.
	verse_heap_mark_bits_page_commit_controller_unlock();

	TUniqueLock Lock(Mutex);
	V_DIE_UNLESS(RequestedCycleVersion > CompletedCycleVersion);
	V_DIE_UNLESS(verse_heap_live_bytes_trigger_threshold == SIZE_MAX);
	CompletedCycleVersion++;

	// NOTE: It's super tempting to try to debug MarkedNativeBytes by adding some kind of assertion here. But it's not possible to assert
	// anything about it.
	//
	// For example, you might think that MarkedNativeBytes <= LiveNativeBytesAtStart, but you'd be wrong, since it's possible for native data
	// structures to grow in size between the start of collection and when they are marked.
	//
	// Native data structures can also shrink, so that rules out a bunch of other assertions you might think of.

	size_t SweptBytes = verse_heap_swept_bytes;
	size_t SurvivingCellBytes = LiveCellBytesAtStart - SweptBytes;
	size_t SurvivingBytes = SurvivingCellBytes + MarkedNativeBytes;
	size_t LiveBytes = verse_heap_live_bytes + LiveNativeBytes;

	V_DIE_UNLESS((intptr_t)SurvivingCellBytes >= 0);
	V_DIE_UNLESS((intptr_t)SurvivingBytes >= 0);

	if (CompletedCycleVersion == RequestedCycleVersion)
	{
		size_t NewTrigger = static_cast<size_t>(static_cast<double>(SurvivingBytes) * 1.5);
		V_DIE_UNLESS(NewTrigger >= SurvivingBytes);
		NewTrigger = std::max(NewTrigger, MinimumTrigger);
		if (LiveBytes >= NewTrigger)
		{
			RequestedCycleVersion++;
			UE_LOG(
				LogVerseGC, Verbose,
				TEXT("Swept bytes %zu, kept native %zu, survived %zu, new live %zu, beginning another GC immediately (due to new trigger %zu)"),
				SweptBytes, MarkedNativeBytes.load(), SurvivingBytes, LiveBytes, NewTrigger);
		}
		else
		{
			verse_heap_live_bytes_trigger_threshold = NewTrigger;
			UE_LOG(
				LogVerseGC, Verbose,
				TEXT("Swept bytes %zu, kept native %zu, survived %zu, new live %zu, new trigger %zu"),
				SweptBytes, MarkedNativeBytes.load(), SurvivingBytes, LiveBytes, verse_heap_live_bytes_trigger_threshold);
		}
	}
	else
	{
		UE_LOG(
			LogVerseGC, Verbose,
			TEXT("Swept bytes %zu, kept native %zu, survived %zu, new live %zu, beginning another GC immediately (per pending request)"),
			SweptBytes, MarkedNativeBytes.load(), SurvivingBytes, LiveBytes);
	}
	CheckCycleTriggerInvariants();
	bIsCollecting = false;
	ConditionVariable.NotifyAll();
}

void FHeap::CheckCycleTriggerInvariants()
{
	V_DIE_UNLESS(RequestedCycleVersion >= CompletedCycleVersion);
	V_DIE_UNLESS(RequestedCycleVersion >= CompletedMarkingCycleVersion);
	V_DIE_UNLESS(CompletedMarkingCycleVersion == CompletedCycleVersion || CompletedMarkingCycleVersion == CompletedCycleVersion + 1);
	if (RequestedCycleVersion > CompletedCycleVersion)
	{
		// This needs to be set to SIZE_MAX anytime we start the GC, because otherwise, libpas would be firing the
		// LiveBytesTriggerCallback during a GC, which would be pointless.
		V_DIE_UNLESS(verse_heap_live_bytes_trigger_threshold == SIZE_MAX);
	}
	else
	{
		// If we're not currently collecting, then we better have a trigger set so we collect if we use more
		// memory.
		V_DIE_UNLESS(verse_heap_live_bytes_trigger_threshold < SIZE_MAX);
	}
}

FCollectionCycleRequest FHeap::StartCollectingIfNotCollecting()
{
	return RequestCollectionCycle(1);
}

FCollectionCycleRequest FHeap::RequestFreshCollectionCycle()
{
	return RequestCollectionCycle(2);
}

FCollectionCycleRequest FHeap::RequestCollectionCycle(uint64 DesiredRequestCompleteDelta)
{
	using namespace UE;
	TUniqueLock Lock(Mutex);
	CheckCycleTriggerInvariants();
	if (RequestedCycleVersion < CompletedCycleVersion + DesiredRequestCompleteDelta)
	{
		RequestedCycleVersion++;
		verse_heap_live_bytes_trigger_threshold = SIZE_MAX;
		ConditionVariable.NotifyAll();
	}
	CheckCycleTriggerInvariants();
	UE_LOG(LogVerseGC, Verbose, TEXT("Requested cycle %llu"), RequestedCycleVersion);
	return FCollectionCycleRequest(RequestedCycleVersion);
}

void FHeap::LiveBytesTriggerCallback()
{
	if (!bIsExternallyControlled)
	{
		UE_LOG(LogVerseGC, Verbose, TEXT("Trigger callback called with live bytes %zu, trigger %zu"),
			verse_heap_live_bytes,
			verse_heap_live_bytes_trigger_threshold);
		StartCollectingIfNotCollecting();
	}
}

void FHeap::CensusCallback(void* Object, void* Arg)
{
	VCell* Cell = static_cast<VCell*>(Object);
	Cell->ConductCensus();
}

void FHeap::DestructorCallback(void* Object, void* Arg)
{
	VCell* Cell = static_cast<VCell*>(Object);
	Cell->RunDestructor();
}

void FHeap::EnableExternalControl(FIOContext Context)
{
	using namespace UE;
	TUniqueLock Lock(Mutex);
	V_DIE_IF(bIsExternallyControlled);
	NormalizeWithoutThreadingAtCollectionStart();
	// Spin on the cycle version to allow for calls to IsGCStartPendingExternalSignal to
	// provide reliable results after enabling.  Spinning on bIsMarking results in a small
	// window of time where bIsMarking goes false prior to the cycle count updating.
	while (RequestedCycleVersion > CompletedCycleVersion)
	{
		ConditionVariable.Wait(Mutex);
	}
	bIsExternallyControlled = true;
	V_DIE_IF(bIsGCReadyForExternalMarking);
	V_DIE_IF(bIsGCMarkingExternallySignaled);
	V_DIE_IF(bIsGCTerminationWaitingForExternalSignal);
	V_DIE_IF(bIsMarking);
}

void FHeap::DisableExternalControl()
{
	using namespace UE;
	TUniqueLock Lock(Mutex);
	V_DIE_UNLESS(bIsExternallyControlled);
	NormalizeWithoutThreadingAtCollectionStart();
	bIsExternallyControlled = false;
	bIsGCReadyForExternalMarking = false;
	bIsGCMarkingExternallySignaled = false;
	bIsGCTerminationWaitingForExternalSignal = false;
	bIsGCTerminatingExternally = false;
	ConditionVariable.NotifyAll();
}

void FHeap::MarkForExternalControlWithoutThreading(FIOContext Context)
{
	V_DIE_UNLESS(bIsExternallyControlled);
	V_DIE_UNLESS(IsWithoutThreadingDuringCollection());
	V_DIE_UNLESS(bIsMarking);
	V_DIE_UNLESS(bIsCollecting);

	do
	{
		Mark(Context);

		// This cannot terminate if we're under external control. It will fail to terminate for one of two reasons:
		//
		// - It marked more stuff.
		// - It didn't mark anything and realized that GC termination is now waiting for an external signal until it can terminate.
		//
		// We want to reloop if it marked more stuff.
		bool bDidTerminate = AttemptToTerminate(Context);
		V_DIE_IF(bDidTerminate);
		V_DIE_UNLESS(MarkStack->IsEmpty() == bIsGCTerminationWaitingForExternalSignal);
	}
	while (!bIsGCTerminationWaitingForExternalSignal);

	V_DIE_UNLESS(bIsExternallyControlled);
	V_DIE_UNLESS(IsWithoutThreadingDuringCollection());
	V_DIE_UNLESS(bIsMarking);
	V_DIE_UNLESS(bIsCollecting);
}

bool FHeap::IsGCStartPendingExternalSignal()
{
	V_DIE_UNLESS(bIsExternallyControlled);
	return RequestedCycleVersion > CompletedCycleVersion;
}

void FHeap::ExternallySynchronouslyStartGC(FIOContext Context)
{
	using namespace UE;

	NormalizeWithoutThreadingAtCollectionStart();

	{
		TUniqueLock Lock(Mutex);
		V_DIE_UNLESS(bIsExternallyControlled);
		V_DIE_IF(bIsGCMarkingExternallySignaled);
		V_DIE_IF(bIsGCTerminationWaitingForExternalSignal);

		if (IsWithoutThreadingDuringCollection())
		{
			V_DIE_IF(bIsGCTerminatingExternally);
		}
		else
		{
			// Protect against a rare race where we signal GC start while the GC thread still hasn't acknowledged that we permitted it to terminate the
			// last time.
			while (bIsGCTerminatingExternally)
			{
				ConditionVariable.Wait(Mutex);
			}
		}

		bIsGCMarkingExternallySignaled = true;

		CheckCycleTriggerInvariants();
		if (RequestedCycleVersion == CompletedMarkingCycleVersion)
		{
			UE_LOG(LogVerseGC, Verbose, TEXT("RequestedCycleVersion = %llu, CompletedMarkingCycleVersion = %llu, CompletedCycleVersion = %llu, incrementing RequestedCycleVersion"), RequestedCycleVersion, CompletedMarkingCycleVersion, CompletedCycleVersion);
			RequestedCycleVersion++;
			verse_heap_live_bytes_trigger_threshold = SIZE_MAX;
		}
		else
		{
			UE_LOG(LogVerseGC, Verbose, TEXT("RequestedCycleVersion = %llu, CompletedMarkingCycleVersion = %llu, CompletedCycleVersion = %llu, not incrementing anything"), RequestedCycleVersion, CompletedMarkingCycleVersion, CompletedCycleVersion);
		}
		CheckCycleTriggerInvariants();

		ConditionVariable.NotifyAll();

		if (!IsWithoutThreadingDuringCollection())
		{
			while (!bIsGCReadyForExternalMarking)
			{
				ConditionVariable.Wait(Mutex);
			}
		}
	}

	if (IsWithoutThreadingDuringCollection())
	{
		RunPreMarking(Context);
		MarkForExternalControlWithoutThreading(Context);
	}
}

bool FHeap::IsGCTerminationPendingExternalSignalImpl()
{
	V_DIE_UNLESS(Mutex.IsLocked());
	V_DIE_UNLESS(bIsExternallyControlled);
	return bIsGCTerminationWaitingForExternalSignal && MarkStack->IsEmpty() && !NumThreadsToScanStackManually && WeakBarrierState == EWeakBarrierState::AttemptingToTerminate;
}

bool FHeap::IsGCTerminationPendingExternalSignal(FIOContext Context)
{
	using namespace UE;

	V_DIE_UNLESS(bIsExternallyControlled);

	auto IsTerminationPending = []() {
		TUniqueLock Lock(Mutex);
		return IsGCTerminationPendingExternalSignalImpl();
	};

	// We need to attempt an increment of marking here, since the external GC might have added stuff to our MarkStack since the last time we ran.
	// But we do not want to attempt an increment if the GC is in the terminating state. That would be just too weird.
	if (IsWithoutThreadingDuringCollection() && !IsTerminationPending())
	{
		MarkForExternalControlWithoutThreading(Context);
	}

	return IsTerminationPending();
}

bool FHeap::TryToExternallySynchronouslyTerminateGC(FIOContext Context)
{
	using namespace UE;

	{
		TUniqueLock Lock(Mutex);
		if (!IsGCTerminationPendingExternalSignalImpl())
		{
			return false;
		}
		V_DIE_UNLESS(bIsExternallyControlled);
		V_DIE_UNLESS(bIsGCReadyForExternalMarking);
		V_DIE_UNLESS(bIsGCTerminationWaitingForExternalSignal);
		V_DIE_UNLESS(bIsGCMarkingExternallySignaled);
		V_DIE_UNLESS(MarkStack->IsEmpty());
		V_DIE_IF(NumThreadsToScanStackManually);
		bIsGCTerminationWaitingForExternalSignal = false;
		bIsGCReadyForExternalMarking = false;
		bIsGCMarkingExternallySignaled = false;
		bIsGCTerminatingExternally = true;
		ConditionVariable.NotifyAll();

		Terminate();
	}

	if (IsWithoutThreadingDuringCollection())
	{
		RunPostMarking(Context);
	}

	return true;
}

void FHeap::ExternallySynchronouslyTerminateGC(FIOContext Context)
{
	V_DIE_UNLESS(TryToExternallySynchronouslyTerminateGC(Context));
}

void FHeap::AddExternalMarkStack(FMarkStack&& InMarkStack)
{
	using namespace UE;

	if (InMarkStack.IsEmpty())
	{
		return;
	}

	TUniqueLock Lock(Mutex);
	V_DIE_UNLESS(bIsMarking);
	V_DIE_UNLESS(bIsCollecting);
	V_DIE_UNLESS(bIsExternallyControlled);

	MarkStack->Append(MoveTemp(InMarkStack));
	V_DIE_IF(MarkStack->IsEmpty());
	CancelTermination();
	ConditionVariable.NotifyAll();
}

bool FHeap::OwnsAddress(void* Ptr)
{
	return verse_heap_owns_address(BitCast<uintptr_t>(Ptr));
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
