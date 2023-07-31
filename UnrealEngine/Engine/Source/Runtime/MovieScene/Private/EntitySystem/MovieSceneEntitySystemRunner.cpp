// Copyright Epic Games, Inc. All Rights Reserved.

#include "EntitySystem/MovieSceneEntitySystemRunner.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneEntityMutations.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedCaptureSource.h"
#include "ProfilingDebugging/CountersTrace.h"

DECLARE_CYCLE_STAT(TEXT("Runner Flush"), 				MovieSceneEval_RunnerFlush, 				STATGROUP_MovieSceneEval);

DECLARE_CYCLE_STAT(TEXT("Spawn Phase"),                 MovieSceneEval_SpawnPhase,              	STATGROUP_MovieSceneECS);
DECLARE_CYCLE_STAT(TEXT("Instantiation Phase"), 		MovieSceneEval_InstantiationPhase, 			STATGROUP_MovieSceneECS);
DECLARE_CYCLE_STAT(TEXT("Instantiation Async Tasks"), 	MovieSceneEval_AsyncInstantiationTasks,		STATGROUP_MovieSceneECS);
DECLARE_CYCLE_STAT(TEXT("Post Instantiation"), 			MovieSceneEval_PostInstantiation, 			STATGROUP_MovieSceneECS);

DECLARE_CYCLE_STAT(TEXT("Evaluation Phase"), 			MovieSceneEval_EvaluationPhase, 			STATGROUP_MovieSceneECS);
DECLARE_CYCLE_STAT(TEXT("Finalization Phase"),          MovieSceneEval_FinalizationPhase,       	STATGROUP_MovieSceneECS);
DECLARE_CYCLE_STAT(TEXT("Post Evaluation Phase"),       MovieSceneEval_PostEvaluationPhase,     	STATGROUP_MovieSceneECS);


/**
 * Structure for making it possible to make re-entrant evaluation on a linker.
 */
struct FMovieSceneEntitySystemEvaluationReentrancyWindow
{
	FMovieSceneEntitySystemRunner* Runner;
	UMovieSceneEntitySystemLinker* Linker;
	int32 RunnerIndex;
	UE::MovieScene::ERunnerFlushState CachedFlushState;
	UE::MovieScene::ERunnerFlushState CachedCurrentFlushState;

	FMovieSceneEntitySystemEvaluationReentrancyWindow(FMovieSceneEntitySystemRunner* InRunner, UMovieSceneEntitySystemLinker* InLinker)
		: Runner(InRunner)
		, Linker(InLinker)
		, RunnerIndex(Linker->ActiveRunners.Num() - 1)
		, CachedFlushState(Runner->FlushState)
		, CachedCurrentFlushState(Runner->CurrentFlushState)
	{
		check(RunnerIndex >= 0);
		checkf(Linker->ActiveRunnerReentrancyFlags[RunnerIndex] == false, TEXT("Nested FMovieSceneEntitySystemEvaluationReentrancyWindows are not supported for the same active runner"));

		Linker->ActiveRunnerReentrancyFlags[RunnerIndex] = true;

		// Clear the current flush state to prevent inner scopes from flushing
		//      our pending states and set the flag that allows us to be re-entrant
		Runner->FlushState = UE::MovieScene::ERunnerFlushState::None;
		Runner->CurrentFlushState = UE::MovieScene::ERunnerFlushState::None;
	}

	~FMovieSceneEntitySystemEvaluationReentrancyWindow()
	{
		// If a re-entrant call left some evaluation still outstanding, we have to flush that before we can continue
		if (Runner->FlushState != UE::MovieScene::ERunnerFlushState::None)
		{
			Runner->FlushOutstanding();
		}

		Runner->FlushState = CachedFlushState;
		Runner->CurrentFlushState = CachedCurrentFlushState;
		if (ensure(Linker->ActiveRunnerReentrancyFlags.IsValidIndex(RunnerIndex)))
		{
			Linker->ActiveRunnerReentrancyFlags[RunnerIndex] = false;
		}
	}
};


FMovieSceneEntitySystemRunner::FMovieSceneEntitySystemRunner()
	: GameThread(ENamedThreads::GameThread_Local)
	, CurrentPhase(UE::MovieScene::ESystemPhase::None)
	, FlushState(UE::MovieScene::ERunnerFlushState::None)
	, CurrentFlushState(UE::MovieScene::ERunnerFlushState::None)
	, bRequireFullFlush(false)
{
}

FMovieSceneEntitySystemRunner::~FMovieSceneEntitySystemRunner()
{
	if (IsAttachedToLinker())
	{
		DetachFromLinker();
	}
}

void FMovieSceneEntitySystemRunner::AttachToLinker(UMovieSceneEntitySystemLinker* InLinker)
{
	if (!ensureMsgf(InLinker, TEXT("Can't attach to a null linker!")))
	{
		return;
	}
	if (!ensureMsgf(WeakLinker.IsExplicitlyNull(), TEXT("This runner is already attached to a linker")))
	{
		if (ensureMsgf(WeakLinker.IsValid(), TEXT("Our previous linker isn't valid anymore! We will permit attaching to a new one.")))
		{
			return;
		}
	}

	WeakLinker = InLinker;
	InLinker->Events.AbandonLinker.AddRaw(this, &FMovieSceneEntitySystemRunner::OnLinkerAbandon);
}

bool FMovieSceneEntitySystemRunner::IsAttachedToLinker() const
{
	return !WeakLinker.IsExplicitlyNull();
}

void FMovieSceneEntitySystemRunner::DetachFromLinker()
{
	if (ensureMsgf(!WeakLinker.IsExplicitlyNull(), TEXT("This runner is not attached to any linker")))
	{
		if (WeakLinker.IsValid())
		{
			OnLinkerAbandon(WeakLinker.Get());
		}
	}
}

UMovieSceneEntitySystemLinker* FMovieSceneEntitySystemRunner::GetLinker() const
{
	return WeakLinker.Get();
}

UE::MovieScene::FEntityManager* FMovieSceneEntitySystemRunner::GetEntityManager() const
{
	if (UMovieSceneEntitySystemLinker* Linker = GetLinker())
	{
		return &Linker->EntityManager;
	}
	return nullptr;
}

UE::MovieScene::FInstanceRegistry* FMovieSceneEntitySystemRunner::GetInstanceRegistry() const
{
	if (UMovieSceneEntitySystemLinker* Linker = GetLinker())
	{
		return Linker->GetInstanceRegistry();
	}
	return nullptr;
}

bool FMovieSceneEntitySystemRunner::IsCurrentlyEvaluating() const
{
	return FlushState != UE::MovieScene::ERunnerFlushState::None;
}

int32 FMovieSceneEntitySystemRunner::GetQueuedUpdateCount() const
{
	return UpdateQueue.Num() + DissectedUpdates.Num();
}

bool FMovieSceneEntitySystemRunner::HasQueuedUpdates() const
{
	if (UpdateQueue.Num() != 0 || DissectedUpdates.Num() != 0)
	{
		return true;
	}
	if (const UMovieSceneEntitySystemLinker* Linker = GetLinker())
	{
		return Linker->HasStructureChangedSinceLastRun();
	}
	return false;
}

bool FMovieSceneEntitySystemRunner::HasQueuedUpdates(FInstanceHandle InInstanceHandle) const
{
	using namespace UE::MovieScene;

	return Algo::FindBy(UpdateQueue, InInstanceHandle, [](const FUpdateParamsAndContext& In){ return In.Params.InstanceHandle; }) != nullptr ||
		Algo::FindBy(DissectedUpdates, InInstanceHandle, &FDissectedUpdate::InstanceHandle) != nullptr;
}

void FMovieSceneEntitySystemRunner::QueueUpdate(const FMovieSceneContext& Context, FInstanceHandle Instance, UE::MovieScene::ERunnerUpdateFlags Flags)
{
	if (EnumHasAnyFlags(Flags, UE::MovieScene::ERunnerUpdateFlags::Flush))
	{
		bRequireFullFlush = true;
	}
	UpdateQueue.Add(FUpdateParamsAndContext{ FSimpleDelegate(), Context, { Instance, Flags } });
}

void FMovieSceneEntitySystemRunner::QueueUpdate(const FMovieSceneContext& Context, FInstanceHandle Instance, FSimpleDelegate&& OnFlushedDelegate, UE::MovieScene::ERunnerUpdateFlags Flags)
{
	if (EnumHasAnyFlags(Flags, UE::MovieScene::ERunnerUpdateFlags::Flush))
	{
		bRequireFullFlush = true;
	}

	UpdateQueue.Add(FUpdateParamsAndContext{ OnFlushedDelegate, Context, { Instance, Flags } });
}

bool FMovieSceneEntitySystemRunner::QueueFinalUpdate(FInstanceHandle InInstanceHandle)
{
	return QueueFinalUpdateImpl(InInstanceHandle, FSimpleDelegate(), false);
}

bool FMovieSceneEntitySystemRunner::QueueFinalUpdate(FInstanceHandle InInstanceHandle, FSimpleDelegate&& InOnLastFlushDelegate)
{
	return QueueFinalUpdateImpl(InInstanceHandle, MoveTemp(InOnLastFlushDelegate), false);
}

bool FMovieSceneEntitySystemRunner::QueueFinalUpdateAndDestroy(FInstanceHandle InInstanceHandle)
{
	return QueueFinalUpdateImpl(InInstanceHandle, FSimpleDelegate(), true);
}

bool CanFinishImmediately(UMovieSceneEntitySystemLinker* Linker, UE::MovieScene::FInstanceHandle InstanceHandle)
{
	using namespace UE::MovieScene;

	const UE::MovieScene::FInstanceRegistry* Registry = Linker->GetInstanceRegistry();
	const FSequenceInstance& Instance = Registry->GetInstance(InstanceHandle);

	ensure(Instance.IsRootSequence());

	if (!Registry->GetInstance(InstanceHandle).Ledger.IsEmpty())
	{
		return false;
	}

	for (const FSequenceInstance& OtherInstance : Registry->GetSparseInstances())
	{
		if (OtherInstance.GetRootInstanceHandle() == Instance.GetRootInstanceHandle())
		{
			if (!OtherInstance.Ledger.IsEmpty())
			{
				return false;
			}
		}
	}

	return true;
}

bool FMovieSceneEntitySystemRunner::QueueFinalUpdateImpl(FInstanceHandle InInstanceHandle, FSimpleDelegate&& InOnLastFlushDelegate, bool bDestroyInstance)
{
	using namespace UE::MovieScene;

	UMovieSceneEntitySystemLinker* Linker = GetLinker();
	if (!Linker)
	{
		return false;
	}

	FInstanceRegistry* InstanceRegistry = GetInstanceRegistry();
	if (!InstanceRegistry->IsHandleValid(InInstanceHandle))
	{
		return false;
	}

	FSequenceInstance& Instance = InstanceRegistry->MutateInstance(InInstanceHandle);

	// If the following conditions are met, we can destroy this instance right away:
	// 
	// 1. the instance has no entities left to unlink
	// 2. we're not in the middle of an update loop 
	// 3. the instance has no current updates
	//
	const bool bCanFinishImmediately = CanFinishImmediately(Linker, InInstanceHandle);
	
	const ERunnerFlushState UnsafeDestroyMask = ERunnerFlushState::Everything & ~(ERunnerFlushState::PostEvaluation | ERunnerFlushState::End);
	const bool bSafeToDestroyNow = !EnumHasAnyFlags(FlushState, UnsafeDestroyMask);
	if (bCanFinishImmediately && bSafeToDestroyNow && !HasQueuedUpdates(InInstanceHandle))
	{
		Instance.Finish(Linker);
		Instance.PostEvaluation(Linker);

		InOnLastFlushDelegate.ExecuteIfBound();

		// PostEvaluation could have prompted a bunch of other logic that may have destroyed our instance handle so we have to check it for validity again
		if (bDestroyInstance && InstanceRegistry->IsHandleValid(InInstanceHandle))
		{
			InstanceRegistry->DestroyInstance(InInstanceHandle);
		}

		return false;
	}

	// We queue up one last update that unlinks all of this instance's entities and lets systems tear down
	// anything that has notable side-effects.
	// It's possible that the instance already had an update request queued. We leave it in so it gets honored,
	// and this second update request will be fulfilled in a second flush pass.
	FUpdateParamsAndContext LastUpdate;
	LastUpdate.Params.InstanceHandle = InInstanceHandle;
	LastUpdate.Params.UpdateFlags = (bDestroyInstance ? (ERunnerUpdateFlags::Finish | ERunnerUpdateFlags::Destroy) : ERunnerUpdateFlags::Finish);
	LastUpdate.OnFlushed = InOnLastFlushDelegate;
	UpdateQueue.Add(LastUpdate);

	return true;
}

void FMovieSceneEntitySystemRunner::AbandonAndDestroyInstance(FInstanceHandle Instance)
{
	if (QueueFinalUpdateAndDestroy(Instance))
	{
		Flush();
	}
}

bool FMovieSceneEntitySystemRunner::StartEvaluation(UMovieSceneEntitySystemLinker* Linker)
{
	using namespace UE::MovieScene;

	// We specifically only check whether the entity manager has changed since the last instantiation once
	// to ensure that we are not vulnerable to infinite loops where components are added/removed in post-evaluation
	const bool bStructureHadChanged = Linker->HasStructureChangedSinceLastRun();

	if (!bStructureHadChanged && UpdateQueue.Num() == 0 && DissectedUpdates.Num() == 0)
	{
		// If nothing has changed, and we have no pending updates, we don't run any evaluation
		FlushState = ERunnerFlushState::None;
		return false;
	}

	if (!Linker->StartEvaluation(*this))
	{
		return false;
	}

	EnterFlushState(ERunnerFlushState::Start);

	// Our entity manager cannot be locked down for us to continue. Something must have left it locked if 
	// this check fails
	FEntityManager& EntityManager = Linker->EntityManager;
	check(!EntityManager.IsLockedDown());

	EntityManager.SetDispatchThread(ENamedThreads::GameThread_Local);
	EntityManager.SetGatherThread(ENamedThreads::GameThread_Local);

	return true;
}

void FMovieSceneEntitySystemRunner::EndEvaluation(UMovieSceneEntitySystemLinker* Linker)
{
	using namespace UE::MovieScene;
	Linker->EndEvaluation(*this);
}

bool FMovieSceneEntitySystemRunner::FlushNext(UMovieSceneEntitySystemLinker* Linker)
{
	using namespace UE::MovieScene;

	// The evaluation loop is set up as a crude state machine which allows us to break out of the
	// loop and pick up where we left off next frame. Each bit within the FlushState bitmask defines
	// a particular task that needs to execute. In this way we can loop the evaluation while there
	// are updates pending by simply setting the necessary flags for each task. For convenience, the
	// required tasks to perform another iteration are defined in ERunnerFlushState::LoopEval

	// Guard the value of the CurrentPhase so it always returns back to ::None
	TGuardValue<ESystemPhase> CurrentPhaseGuard(CurrentPhase, ESystemPhase::None);
	TGuardValue<ERunnerFlushState> CurrentFlushStateGuard(CurrentFlushState, ERunnerFlushState::None);

	// Step 1: Initialize all external flags and broadcast 'begin eval' events.
	//         NOTE: This is only ever called once, regardless of how many iterations we perform
	if (EnumHasAnyFlags(FlushState, ERunnerFlushState::Start))
	{
		// EnterFlushState is called by StartEvaluation itself since it can fail
		return StartEvaluation(Linker);
	}

	// Step 2: Update sequence instances and import entities
	//         This is the entry-point for for each iteration
	if (EnumHasAnyFlags(FlushState, ERunnerFlushState::Import))
	{
		EnterFlushState(ERunnerFlushState::Import);
		return GameThread_UpdateSequenceInstances(Linker);
	}

	// Step 3: Conditionally run the spawn phase of the system graph
	//
	if (EnumHasAnyFlags(FlushState, ERunnerFlushState::Spawn))
	{
		EnterFlushState(ERunnerFlushState::Spawn);
		return GameThread_SpawnPhase(Linker);
	}

	// Step 4: Conditionally run the instantiation phase of the system graph
	//
	if (EnumHasAnyFlags(FlushState, ERunnerFlushState::Instantiation))
	{
		EnterFlushState(ERunnerFlushState::Instantiation);
		return GameThread_InstantiationPhase(Linker);
	}

	// Step 5: Run the evaluation phase of the system graph
	//
	if (EnumHasAnyFlags(FlushState, ERunnerFlushState::Evaluation))
	{
		EnterFlushState(ERunnerFlushState::Evaluation);
		return GameThread_EvaluationPhase(Linker);
	}

	// Step 6: Run the finalization phase of the system graph, including legacy templates
	//
	if (EnumHasAnyFlags(FlushState, ERunnerFlushState::Finalization))
	{
		EnterFlushState(ERunnerFlushState::Finalization);
		GameThread_EvaluationFinalizationPhase(Linker);
		return true;
	}

	// Step 7: Trigger events
	//
	if (EnumHasAnyFlags(FlushState, ERunnerFlushState::EventTriggers))
	{
		EnterFlushState(ERunnerFlushState::EventTriggers);
		GameThread_EventTriggerPhase(Linker);
		return true;
	}

	// Step 7: Call PostEvaluation on all current sequence instances
	//
	if (EnumHasAnyFlags(FlushState, ERunnerFlushState::PostEvaluation))
	{
		EnterFlushState(ERunnerFlushState::PostEvaluation);
		GameThread_PostEvaluationPhase(Linker);
		return true;
	}

	// Step 7: Perform any clean up and broadcast 'end eval' events
	//         NOTE: Only ever called once regardless of how many iterations we perform
	if (EnumHasAnyFlags(FlushState, ERunnerFlushState::End))
	{
		EnterFlushState(ERunnerFlushState::End);
		EndEvaluation(Linker);
		return true;
	}

	return false;
}

void FMovieSceneEntitySystemRunner::Flush(double BudgetMs)
{
	using namespace UE::MovieScene;

	// If we're not currently evaluating, start by flushing everything
	if (FlushState == ERunnerFlushState::None)
	{
		FlushState = ERunnerFlushState::Everything;
	}

	FlushOutstanding(BudgetMs);
}

void FMovieSceneEntitySystemRunner::FlushOutstanding(double BudgetMs, UE::MovieScene::ERunnerFlushState TargetState)
{
	using namespace UE::MovieScene;

	// Simple case is we have nothing outstanding, so just early return
	if (FlushState == ERunnerFlushState::None)
	{
		return;
	}

	UMovieSceneEntitySystemLinker* Linker = GetLinker();

	// Check that we are attached to a linker that allows starting a new evaluation.
	if (!ensureMsgf(Linker, TEXT("Runner isn't attached to a valid linker")))
	{
		return;
	}

	if (!ensureMsgf(CurrentFlushState == ERunnerFlushState::None, TEXT("Cannot flush this runner while it is already being flushed outside of a re-entrancy window")))
	{
		return;
	}

	SCOPE_CYCLE_COUNTER(MovieSceneEval_RunnerFlush);

	// We need to run the system from the game thread so we know we can fire events and callbacks from here.
	check(IsInGameThread());

	// Set the debug vizualizer's entity manager pointer, so all debugging happening here will show
	// relevant information. We need to set it here instead of higher up because we could have, say,
	// a blocking sequence triggering another blocking sequence via an event track. The nested call stack
	// of the second sequence needs to show debug information relevant to its private linker, but when
	// we return back up to the first sequence, which might still have another update round (such as
	// the other side of the dissected update range around the event), we need to set the pointer back
	// again.
	TGuardValue<FEntityManager*> DebugVizGuard(GEntityManagerForDebuggingVisualizers, GetEntityManager());

	// For the purposes of this function, None means that everything must be flushed
	if (TargetState == ERunnerFlushState::None)
	{
		TargetState = ERunnerFlushState::Everything;
	}

	const double BudgetSeconds = BudgetMs / 1000.f;
	if (!bRequireFullFlush && BudgetSeconds > 0.0)
	{
		double StartTime = FPlatformTime::Seconds();
		do
		{
			if (!FlushNext(Linker))
			{
				break;
			}
		}
		while (FPlatformTime::Seconds() - StartTime < BudgetSeconds && EnumHasAnyFlags(FlushState, TargetState));
	}
	else
	{
		while (EnumHasAnyFlags(FlushState, TargetState))
		{
			if (!FlushNext(Linker))
			{
				break;
			}
		}
	}

	bRequireFullFlush = false;
}

void FMovieSceneEntitySystemRunner::ResetFlushState()
{
	using namespace UE::MovieScene;

	constexpr ERunnerFlushState StatesThatTriggerReset = ERunnerFlushState::Instantiation | ERunnerFlushState::Evaluation | ERunnerFlushState::Finalization | ERunnerFlushState::EventTriggers;
	constexpr ERunnerFlushState StatesThatDontTriggerReset = ERunnerFlushState::Import | ERunnerFlushState::Spawn;

	// We only need to reset the flush state if we are far enough through an evaluation
	if (EnumHasAnyFlags(FlushState, StatesThatTriggerReset) && !EnumHasAnyFlags(FlushState, StatesThatDontTriggerReset))
	{
		// When resetting - we don't need to (or want to) re-import anything, we just want to re-run the current
		// frame of updates 
		FlushState = UE::MovieScene::ERunnerFlushState::LoopEval & ~UE::MovieScene::ERunnerFlushState::Import;
	}
}

void FMovieSceneEntitySystemRunner::DiscardQueuedUpdates(FInstanceHandle Instance)
{
	using namespace UE::MovieScene;

	for (int32 Index = UpdateQueue.Num()-1; Index >= 0; --Index)
	{
		if (UpdateQueue[Index].Params.InstanceHandle == Instance)
		{
			UpdateQueue.RemoveAt(Index, 1, false);
		}
	}
	for (int32 Index = DissectedUpdates.Num()-1; Index >= 0; --Index)
	{
		if (DissectedUpdates[Index].InstanceHandle == Instance)
		{
			DissectedUpdates.RemoveAt(Index, 1, false);
		}
	}

	for (int32 Index = CurrentInstances.Num()-1; Index >= 0; --Index)
	{
		if (CurrentInstances[Index].InstanceHandle == Instance)
		{
			CurrentInstances.RemoveAt(Index, 1, false);
		}
	}
}

void FMovieSceneEntitySystemRunner::EnterFlushState(UE::MovieScene::ERunnerFlushState EnteredFlushState)
{
	// Stop this state from being run again and allow the next one to run
	FlushState &= ~EnteredFlushState;
	// Mark this as the current state
	CurrentFlushState = EnteredFlushState;
}

void FMovieSceneEntitySystemRunner::SkipFlushState(UE::MovieScene::ERunnerFlushState FlushStateToSkip)
{
	FlushState &= ~FlushStateToSkip;
}

bool FMovieSceneEntitySystemRunner::GameThread_UpdateSequenceInstances(UMovieSceneEntitySystemLinker* Linker)
{
	using namespace UE::MovieScene;

	TRACE_CPUPROFILER_EVENT_SCOPE(FMovieSceneEntitySystemRunner::GameThread_UpdateSequenceInstances);

	// Also reset the capture source scope so that each group of sequences tied to a given linker starts
	// with a clean slate.
	TGuardValue<FScopedPreAnimatedCaptureSource*> CaptureSourceGuard(FScopedPreAnimatedCaptureSource::GetCaptureSourcePtr(), nullptr);

	FInstanceRegistry* InstanceRegistry = GetInstanceRegistry();

	if (DissectedUpdates.Num() == 0)
	{
		TArray<TRange<FFrameTime>> Dissections;
		TBitArray<> UpdatedSequenceInstances;

		TArray<FUpdateParamsAndContext> TempUpdateQueue;
		Swap(TempUpdateQueue, UpdateQueue);

		DissectedUpdates.Reserve(TempUpdateQueue.Num());

		for (int32 UpdateIndex = 0; UpdateIndex < TempUpdateQueue.Num(); ++UpdateIndex)
		{
			FUpdateParamsAndContext& Request = TempUpdateQueue[UpdateIndex];
			const int32 InstanceID = Request.Params.InstanceHandle.InstanceID;

			if (!InstanceRegistry->IsHandleValid(Request.Params.InstanceHandle))
			{
				continue;
			}
			else if (UpdatedSequenceInstances.IsValidIndex(InstanceID) && UpdatedSequenceInstances[InstanceID] == true)
			{
				// We already have an update for this instance, so we queue it up again for the next flush
				UpdateQueue.Add(Request);
				continue;
			}

			// Set the bit to indicate this sequence is being updated
			UpdatedSequenceInstances.PadToNum(InstanceID + 1, false);
			UpdatedSequenceInstances[InstanceID] = true;

			// If the request is to destroy the instance, we must assume it is no longer valid and should
			// not be updated
			if (EnumHasAnyFlags(Request.Params.UpdateFlags, ERunnerUpdateFlags::Destroy))
			{
				DissectedUpdates.Add(FDissectedUpdate{ MoveTemp(Request.OnFlushed), Request.Context, Request.Params.InstanceHandle, MAX_int32, Request.Params.UpdateFlags });
				MarkForUpdate(Request.Params.InstanceHandle, Request.Params.UpdateFlags);
				continue;
			}

			// Give the instance an opportunity to dissect the range into distinct evaluations
			FSequenceInstance& Instance = InstanceRegistry->MutateInstance(Request.Params.InstanceHandle);
			Instance.DissectContext(Linker, Request.Context, Dissections);

			if (Dissections.Num() != 0)
			{
				for (int32 Index = 0; Index < Dissections.Num() - 1; ++Index)
				{
					// Never finish or destroy sequence instances until the _last_ dissected update
					ERunnerUpdateFlags Flags = Request.Params.UpdateFlags & ~ERunnerUpdateFlags::FinalDissectionMask;

					FDissectedUpdate Dissection{
						FSimpleDelegate(), // Never trigger the OnFlushed delegate until the final dissection
						FMovieSceneContext(FMovieSceneEvaluationRange(Dissections[Index], Request.Context.GetFrameRate(), Request.Context.GetDirection()), Request.Context.GetStatus()),
						Request.Params.InstanceHandle,
						Index,
						Flags
					};
					DissectedUpdates.Add(Dissection);

					// Only mark the first dissection for update this round
					if (Index == 0)
					{
						MarkForUpdate(Request.Params.InstanceHandle, Flags);
					}
				}

				// Add the last one with MAX_int32 so it gets evaluated with all the others in this flush
				FDissectedUpdate Dissection{
					MoveTemp(Request.OnFlushed),
					FMovieSceneContext(FMovieSceneEvaluationRange(Dissections.Last(), Request.Context.GetFrameRate(), Request.Context.GetDirection()), Request.Context.GetStatus()),
					Request.Params.InstanceHandle,
					MAX_int32,
					Request.Params.UpdateFlags
				};
				DissectedUpdates.Add(Dissection);

				Dissections.Reset();
			}
			else
			{
				DissectedUpdates.Add(FDissectedUpdate{ MoveTemp(Request.OnFlushed), Request.Context, Request.Params.InstanceHandle, MAX_int32, Request.Params.UpdateFlags });
				MarkForUpdate(Request.Params.InstanceHandle, Request.Params.UpdateFlags);
			}
		}
	}
	else
	{
		// Look for the next batch of updates, and mark the respective sequence instances as currently updating.
		const int32 PredicateOrder = DissectedUpdates[0].Order;
		for (int32 Index = 0; Index < DissectedUpdates.Num() && DissectedUpdates[Index].Order == PredicateOrder; ++Index)
		{
			FDissectedUpdate& Update = DissectedUpdates[Index];
			MarkForUpdate(Update.InstanceHandle, Update.UpdateFlags);
		}
	}

	// If we have no instances marked for update, we are running an evaluation probably because some
	// structural changes have occurred in the entity manager (out of date instantiation serial number
	// in the linker). So we mark everything for update, so that PreEvaluation/PostEvaluation callbacks
	// and legacy templates are correctly executed.
	if (CurrentInstances.Num() == 0)
	{
		for (const FSequenceInstance& Instance : InstanceRegistry->GetSparseInstances())
		{
			MarkForUpdate(Instance.GetInstanceHandle(), ERunnerUpdateFlags::None);
		}
	}

	// Let sequence instances do any pre-evaluation work.
	for (const FQueuedUpdateParams& UpdatedInstance : CurrentInstances)
	{
		if (!EnumHasAnyFlags(UpdatedInstance.UpdateFlags, ERunnerUpdateFlags::Destroy))
		{
			InstanceRegistry->MutateInstance(UpdatedInstance.InstanceHandle).PreEvaluation(Linker);
		}
	}

	// NOTE: IncrementSystemSerial must be called before any instance updates are made
	//       to ensure that up-to-date versions are used inside FEntityManager::OnStructureChanged
	Linker->EntityManager.IncrementSystemSerial();

	// Update all systems
	if (DissectedUpdates.Num() != 0)
	{
		const int32 PredicateOrder = DissectedUpdates[0].Order;
		int32 Index = 0;
		for (; Index < DissectedUpdates.Num() && DissectedUpdates[Index].Order == PredicateOrder; ++Index)
		{
			FDissectedUpdate& Update = DissectedUpdates[Index];

			// Always forward the OnFlushed delegate to be called at the end of the frame, even if the instance is no longer valid
			if (Update.OnFlushed.IsBound())
			{
				OnFlushedDelegates.Add(MoveTemp(Update.OnFlushed));
			}

			if (EnumHasAnyFlags(Update.UpdateFlags, ERunnerUpdateFlags::Destroy))
			{
				continue;
			}

			if (ensure(InstanceRegistry->IsHandleValid(Update.InstanceHandle)))
			{
				FSequenceInstance& Instance = InstanceRegistry->MutateInstance(Update.InstanceHandle);

				if (EnumHasAnyFlags(Update.UpdateFlags, ERunnerUpdateFlags::Finish))
				{
					// Context is irrelevant for Finishing sequences
					Instance.Finish(Linker);
				}
				else
				{
					Instance.Update(Linker, Update.Context);
				}
			}
		}
		DissectedUpdates.RemoveAt(0, Index);
	}

#if DO_GUARD_SLOW
	// Check that there are no duplicates in the CurrentInstances array
	{
		TBitArray<> VisitedBits;
		VisitedBits.Reserve(InstanceRegistry->GetSparseInstances().GetMaxIndex());
		for (const FQueuedUpdateParams& Update : CurrentInstances)
		{
			const int32 Index = Update.InstanceHandle.InstanceID;

			checkf(!VisitedBits.IsValidIndex(Index) || VisitedBits[Index] == false, TEXT("Multiple updates exist for the same sequence instance. This indicates a bookkeeping error with the CurrentInstances array."));

			VisitedBits.PadToNum(Index + 1, false);
			VisitedBits[Index] = true;
		}
	}
#endif

	return true;
}

bool FMovieSceneEntitySystemRunner::GameThread_SpawnPhase(UMovieSceneEntitySystemLinker* Linker)
{
	using namespace UE::MovieScene;

	check(GameThread == ENamedThreads::GameThread || GameThread == ENamedThreads::GameThread_Local);


	CurrentPhase = ESystemPhase::Spawn;

	FInstanceRegistry* InstanceRegistry = GetInstanceRegistry();

	const bool bInstantiationDirty = Linker->HasStructureChangedSinceLastRun() || InstanceRegistry->HasInvalidatedBindings();

	FGraphEventArray AllTasks;

	Linker->AutoLinkRelevantSystems();

	// --------------------------------------------------------------------------------------------------------------------------------------------
	// Run the spawn phase if there were any changes to the current entity instantiations, and wait on the result
	{
		SCOPE_CYCLE_COUNTER(MovieSceneEval_SpawnPhase);
		if (bInstantiationDirty)
		{
			// The spawn phase can queue events to trigger from the event tracks.
			bCanQueueEventTriggers = true;
			{
				Linker->SystemGraph.ExecutePhase(ESystemPhase::Spawn, Linker, AllTasks);
			}
			bCanQueueEventTriggers = false;

			// We don't open a re-entrancy window, however, because there's no way we can recursively evaluate things at this point... too many
			// things are in an intermediate state. So events triggered as PreSpawn/PostSpawn can't be wired to something that starts a sequence.
			if (EventTriggers.IsBound())
			{
				EventTriggers.Broadcast();
				EventTriggers.Clear();
			}
		}

		// If there were any tasks created, we need to wait on them before proceeding. This is rare for the spawn phase.
		if (AllTasks.Num() != 0)
		{
			FGraphEventRef SpawnEvent = TGraphTask<FNullGraphTask>::CreateTask(&AllTasks, ENamedThreads::GameThread)
			.ConstructAndDispatchWhenReady(TStatId(), GameThread);

			FTaskGraphInterface::Get().WaitUntilTaskCompletes(SpawnEvent, ENamedThreads::GameThread_Local);
		}
	}


	Linker->Events.PostSpawnEvent.Broadcast(Linker);

	// --------------------------------------------------------------------------------------------------------------------------------------------
	// Only run the instantiation phase if there is anything to instantiate. This must come after the spawn phase because new instantiations may
	// be created during the spawn phase
	const bool bAnyPending = Linker->EntityManager.ContainsAnyComponent(FBuiltInComponentTypes::Get()->RequiresInstantiationMask) || InstanceRegistry->HasInvalidatedBindings();
	if (bInstantiationDirty == false && bAnyPending == false)
	{
		SkipFlushState(ERunnerFlushState::Instantiation);
	}

	return true;
}

bool FMovieSceneEntitySystemRunner::GameThread_InstantiationPhase(UMovieSceneEntitySystemLinker* Linker)
{
	using namespace UE::MovieScene;

	SCOPE_CYCLE_COUNTER(MovieSceneEval_InstantiationPhase);

	check(GameThread == ENamedThreads::GameThread || GameThread == ENamedThreads::GameThread_Local);

	CurrentPhase = ESystemPhase::Instantiation;

	FGraphEventArray AllTasks;
	Linker->SystemGraph.ExecutePhase(ESystemPhase::Instantiation, Linker, AllTasks);

	// If there were any tasks created, we need to wait on them before proceeding. This is rare for the instantiation phase.
	if (AllTasks.Num() != 0)
	{
		FGraphEventRef InstantiationEvent = TGraphTask<FNullGraphTask>::CreateTask(&AllTasks, ENamedThreads::GameThread)
		.ConstructAndDispatchWhenReady(TStatId(), GameThread);

		FTaskGraphInterface::Get().WaitUntilTaskCompletes(InstantiationEvent, ENamedThreads::GameThread_Local);
	}

	return GameThread_PostInstantiation(Linker);
}

bool FMovieSceneEntitySystemRunner::GameThread_PostInstantiation(UMovieSceneEntitySystemLinker* Linker)
{
	using namespace UE::MovieScene;

	SCOPE_CYCLE_COUNTER(MovieSceneEval_PostInstantiation);

	check(GameThread == ENamedThreads::GameThread || GameThread == ENamedThreads::GameThread_Local);

	Linker->PostInstantation(*this);

	FEntityManager& EntityManager = Linker->EntityManager;
	FBuiltInComponentTypes* BuiltInComponentTypes = FBuiltInComponentTypes::Get();

	// Nothing needs linking, caching or restoring any more
	FRemoveMultipleMutation Mutation;
	Mutation.MaskToRemove = BuiltInComponentTypes->RequiresInstantiationMask;
	// Inverse the filter because MaskToRemove is applied as a binary AND against all matching allocations.
	// Therefore, any set bits in RequiresInstantiationMask will be removed from components
	Mutation.MaskToRemove.BitwiseNOT();

	FEntityComponentFilter Filter = FEntityComponentFilter().Any(BuiltInComponentTypes->RequiresInstantiationMask);
	EntityManager.MutateAll(Filter, Mutation);

	// Free anything that has been unlinked
	EntityManager.FreeEntities(FEntityComponentFilter().All({ BuiltInComponentTypes->Tags.NeedsUnlink }));

	Linker->AutoUnlinkIrrelevantSystems();

	EntityManager.Compact();
	return true;
}

bool FMovieSceneEntitySystemRunner::GameThread_EvaluationPhase(UMovieSceneEntitySystemLinker* Linker)
{
	using namespace UE::MovieScene;

	SCOPE_CYCLE_COUNTER(MovieSceneEval_EvaluationPhase);

	CurrentPhase = ESystemPhase::Evaluation;

	FGraphEventArray AllTasks;
	// --------------------------------------------------------------------------------------------------------------------------------------------
	// Step 2: Run the evaluation phase. The entity manager is locked down for this phase, meaning no changes to entity-component structure is allowed
	//         This vastly simplifies the concurrent handling of entity component allocations
	Linker->EntityManager.LockDown();

	checkf(!Linker->EntityManager.ContainsComponent(FBuiltInComponentTypes::Get()->Tags.NeedsUnlink), TEXT("Stale entities remain in the entity manager during evaluation - these should have been destroyed during the instantiation phase. Did it run?"));

	Linker->SystemGraph.ExecutePhase(ESystemPhase::Evaluation, Linker, AllTasks);

	if (AllTasks.Num() != 0)
	{
		FGraphEventRef EvaluationEvent = TGraphTask<FNullGraphTask>::CreateTask(&AllTasks, ENamedThreads::GameThread)
		.ConstructAndDispatchWhenReady(TStatId(), GameThread);

		FTaskGraphInterface::Get().WaitUntilTaskCompletes(EvaluationEvent, ENamedThreads::GameThread_Local);
	}

	Linker->EntityManager.ReleaseLockDown();

	return true;
}

void FMovieSceneEntitySystemRunner::GameThread_EvaluationFinalizationPhase(UMovieSceneEntitySystemLinker* Linker)
{
	using namespace UE::MovieScene;

	check(GameThread == ENamedThreads::GameThread || GameThread == ENamedThreads::GameThread_Local);

	CurrentPhase = ESystemPhase::Finalization;

	// Post-eval events can be queued during the finalization phase so let's open that up.
	// The events are actually executed a bit later, in GameThread_EventTriggerPhase.
	bCanQueueEventTriggers = true;
	{
		SCOPE_CYCLE_COUNTER(MovieSceneEval_FinalizationPhase);

		FInstanceRegistry* InstanceRegistry = GetInstanceRegistry();

		// Iterate on a copy of our current instances, since LegacyEvaluator->Evaluate() could change the instance handle, which would affect PostEvaluationPhase
		TArray<FQueuedUpdateParams> CurrentInstancesCopy(CurrentInstances);
		for (const FQueuedUpdateParams& UpdateParams : CurrentInstancesCopy)
		{
			if (InstanceRegistry->IsHandleValid(UpdateParams.InstanceHandle))
			{
				FSequenceInstance& Instance = InstanceRegistry->MutateInstance(UpdateParams.InstanceHandle);
				if (Instance.IsRootSequence())
				{
					Instance.RunLegacyTrackTemplates();
				}
			}
		}

		FGraphEventArray Tasks;
		Linker->SystemGraph.ExecutePhase(ESystemPhase::Finalization, Linker, Tasks);
		checkf(Tasks.Num() == 0, TEXT("Cannot dispatch new tasks during finalization"));
	}
	bCanQueueEventTriggers = false;

	if (!EventTriggers.IsBound())
	{
		// Skip event triggers if there are none
		SkipFlushState(ERunnerFlushState::EventTriggers);
	}
}

void FMovieSceneEntitySystemRunner::GameThread_EventTriggerPhase(UMovieSceneEntitySystemLinker* Linker)
{
	using namespace UE::MovieScene;

	// Execute any queued events from the evaluation finalization phase.
	if (EventTriggers.IsBound())
	{
		// Event triggers are allowed to be re-entrant
		FMovieSceneEntitySystemEvaluationReentrancyWindow Window(this, Linker);

		// Trigger from a temporary delegate to ensure that re-entrant evaluations do not re-trigger events
		FMovieSceneEntitySystemEventTriggers TmpEventTriggers;

		Swap(TmpEventTriggers, EventTriggers);
		EventTriggers.Clear();

		TmpEventTriggers.Broadcast();
	}
}

void FMovieSceneEntitySystemRunner::GameThread_PostEvaluationPhase(UMovieSceneEntitySystemLinker* Linker)
{
	using namespace UE::MovieScene;

	SCOPE_CYCLE_COUNTER(MovieSceneEval_PostEvaluationPhase);

	// Now run the post-evaluation logic so that we can safely handle broadcast events (like OnFinished)
	// that trigger some new evaluations (such as connecting it to another sequence's Play in Blueprint).
	//
	// If we are the global linker (and not a "private" linker, as is the case with "blocking" sequences),
	// we may find ourselves in a re-entrant call, which means we need to save our state here and restore
	// it afterwards. We also iterate on a copy of our current instances, since a re-entrant call would
	// modify that array.

	// Temporarily cache the pending updates and dissected updates so we can preserve re-entrant ordering
	//    by appending the arrays afterwards. This ensures that any updates queued during a re-entrant call
	//    get evaluated _before_ the updates that are specified in the outer scope, even if the evaluation was
	//    budgeted and didn't fully flush, whilst simultaneously correctly only flushing inner updates if a
	//    full flush is performed.
	TArray<FQueuedUpdateParams> TmpCurrentInstances;
	TArray<FUpdateParamsAndContext> TmpUpdateQueue;
	TArray<FDissectedUpdate> TmpDissectedUpdates;
	TArray<FInstanceHandle> TmpDestroyInstances;
	TArray<FSimpleDelegate> TmpOnFlushedDelegates;

	Swap(UpdateQueue, TmpUpdateQueue);
	Swap(DissectedUpdates, TmpDissectedUpdates);
	Swap(CurrentInstances, TmpCurrentInstances);
	Swap(OnFlushedDelegates, TmpOnFlushedDelegates);

	FInstanceRegistry* InstanceRegistry = Linker->GetInstanceRegistry();

	{
		FMovieSceneEntitySystemEvaluationReentrancyWindow Window(this, Linker);

		for (const FQueuedUpdateParams& UpdateParams : TmpCurrentInstances)
		{
			// We must check for validity here because the cache handles may have become invalid
			// during this iteration (since there is a re-entrancy window open)
			if (InstanceRegistry->IsHandleValid(UpdateParams.InstanceHandle))
			{
				FSequenceInstance& Instance = InstanceRegistry->MutateInstance(UpdateParams.InstanceHandle);
				Instance.PostEvaluation(Linker);

				if (EnumHasAnyFlags(UpdateParams.UpdateFlags, ERunnerUpdateFlags::Destroy))
				{
					TmpDestroyInstances.Add(UpdateParams.InstanceHandle);
				}
			}
		}

		for (const FSimpleDelegate& OnFlushed : TmpOnFlushedDelegates)
		{
			OnFlushed.Execute();
		}
	}

	// Destroy any instances that need destroying
	for (FInstanceHandle Handle : TmpDestroyInstances)
	{
		if (InstanceRegistry->IsHandleValid(Handle))
		{
			InstanceRegistry->DestroyInstance(Handle);
		}
	}

	// If we had any updates _before_ we called the external callbacks (which are stored in TmpUpdateQueue), make sure those are maintained first
	Swap(TmpUpdateQueue, UpdateQueue);

	// TmpUpdateQueue now contains any updates that were queued _during_ the external callbacks, so we append those to our old queue (which is now stored back in UpdateQueue)
	if (TmpUpdateQueue.Num() > 0)
	{
		UpdateQueue.Append(TmpUpdateQueue);
	}

	// Do the same with the dissected updates
	Swap(TmpDissectedUpdates, DissectedUpdates);
	if (TmpDissectedUpdates.Num() > 0)
	{
		DissectedUpdates.Append(TmpDissectedUpdates);
	}

	// If we have any pending updates, we need to run another evaluation.
	// This will cause us to effectively loop back to ERunnerFlushState::Import
	// which will pick up the next updates
	if (UpdateQueue.Num() > 0 || DissectedUpdates.Num() > 0)
	{
		FlushState = ERunnerFlushState::LoopEval;
	}
}

void FMovieSceneEntitySystemRunner::MarkForUpdate(FInstanceHandle InInstanceHandle, UE::MovieScene::ERunnerUpdateFlags UpdateFlags)
{
	CurrentInstances.Add(FQueuedUpdateParams{ InInstanceHandle, UpdateFlags });
}

void FMovieSceneEntitySystemRunner::OnLinkerAbandon(UMovieSceneEntitySystemLinker* InLinker)
{
	if (ensure(InLinker))
	{
		InLinker->Events.AbandonLinker.RemoveAll(this);
	}
	WeakLinker.Reset();
}

FMovieSceneEntitySystemEventTriggers& FMovieSceneEntitySystemRunner::GetQueuedEventTriggers()
{
	checkf(bCanQueueEventTriggers, TEXT("Can't queue event triggers at this point in the update loop."));
	return EventTriggers;
}

