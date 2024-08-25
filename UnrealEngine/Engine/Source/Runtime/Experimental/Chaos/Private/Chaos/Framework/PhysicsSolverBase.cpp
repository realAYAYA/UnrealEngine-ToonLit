// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/Framework/PhysicsSolverBase.h"
#include "Chaos/Framework/PhysicsProxyBase.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "ChaosStats.h"
#include "Chaos/PendingSpatialData.h"
#include "PBDRigidsSolver.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Chaos/Framework/ChaosResultsManager.h"
#include "ChaosSolversModule.h"
#include "Framework/Threading.h"
#include "RewindData.h"

#include "ChaosVisualDebugger/ChaosVisualDebuggerTrace.h"
#include "ChaosVisualDebugger/ChaosVDContextProvider.h"

DEFINE_STAT(STAT_AsyncPullResults);
DEFINE_STAT(STAT_AsyncInterpolateResults);
DEFINE_STAT(STAT_SyncPullResults);
DEFINE_STAT(STAT_ProcessSingleProxy);
DEFINE_STAT(STAT_ProcessGCProxy);
DEFINE_STAT(STAT_ProcessClusterUnionProxy);
DEFINE_STAT(STAT_PullConstraints);

CSV_DEFINE_CATEGORY(ChaosPhysicsSolver, true);

namespace Chaos
{	
	extern int GSingleThreadedPhysics;
	void FPhysicsSolverBase::ChangeBufferMode(EMultiBufferMode InBufferMode)
	{
		BufferMode = InBufferMode;
	}

	FDelegateHandle FPhysicsSolverEvents::AddPreAdvanceCallback(FSolverPreAdvance::FDelegate InDelegate)
	{
		return EventPreSolve.Add(InDelegate);
	}

	bool FPhysicsSolverEvents::RemovePreAdvanceCallback(FDelegateHandle InHandle)
	{
		return EventPreSolve.Remove(InHandle);
	}

	FDelegateHandle FPhysicsSolverEvents::AddPreBufferCallback(FSolverPreBuffer::FDelegate InDelegate)
	{
		return EventPreBuffer.Add(InDelegate);
	}

	bool FPhysicsSolverEvents::RemovePreBufferCallback(FDelegateHandle InHandle)
	{
		return EventPreBuffer.Remove(InHandle);
	}

	FDelegateHandle FPhysicsSolverEvents::AddPostAdvanceCallback(FSolverPostAdvance::FDelegate InDelegate)
	{
		return EventPostSolve.Add(InDelegate);
	}

	bool FPhysicsSolverEvents::RemovePostAdvanceCallback(FDelegateHandle InHandle)
	{
		return EventPostSolve.Remove(InHandle);
	}

	FDelegateHandle FPhysicsSolverEvents::AddTeardownCallback(FSolverTeardown::FDelegate InDelegate)
	{
		return EventTeardown.Add(InDelegate);
	}

	bool FPhysicsSolverEvents::RemoveTeardownCallback(FDelegateHandle InHandle)
	{
		return EventTeardown.Remove(InHandle);
	}

	FAutoConsoleTaskPriority CPrio_FPhysicsTickTask(
		TEXT("TaskGraph.TaskPriorities.PhysicsTickTask"),
		TEXT("Task and thread priotiry for Chaos physics tick"),
		ENamedThreads::HighThreadPriority, // if we have high priority task threads, then use them...
		ENamedThreads::NormalTaskPriority, // .. at normal task priority
		ENamedThreads::HighTaskPriority // if we don't have hi pri threads, then use normal priority threads at high task priority instead
	);

	int32 PhysicsRunsOnGT = 0;
	FAutoConsoleVariableRef CVarPhysicsRunsOnGT(TEXT("p.PhysicsRunsOnGT"), PhysicsRunsOnGT, TEXT("If true the physics thread runs on the game thread, but will still go wide on tasks like collision detection"));

	ENamedThreads::Type FPhysicsSolverProcessPushDataTask::GetDesiredThread()
	{
		return CPrio_FPhysicsTickTask.Get();
	}

	ENamedThreads::Type FPhysicsSolverAdvanceTask::GetDesiredThread()
	{
		return PhysicsRunsOnGT == 0 ? CPrio_FPhysicsTickTask.Get() : ENamedThreads::GameThread;
	}

	void FPhysicsSolverProcessPushDataTask::ProcessPushData()
	{
		using namespace Chaos;

		LLM_SCOPE(ELLMTag::ChaosUpdate);
		SCOPE_CYCLE_COUNTER(STAT_ChaosTick);
		CSV_SCOPED_TIMING_STAT_EXCLUSIVE(Physics);
		CSV_SCOPED_TIMING_STAT(PhysicsVerbose, StepSolver);
		
		CVD_SCOPE_CONTEXT(Solver.GetChaosVDContextData());

#if PHYSICS_THREAD_CONTEXT
		FPhysicsThreadContextScope Scope(/*IsPhysicsThreadContext=*/true);
#endif

		Solver.SetExternalTimestampConsumed_Internal(PushData->ExternalTimestamp);
		Solver.ProcessPushedData_Internal(*PushData);
		
		Solver.PrepareAdvanceBy(PushData->ExternalDt);

	}

	void FPhysicsSolverFrozenGTPreSimCallbacks::GTPreSimCallbacks()
	{
		LLM_SCOPE(ELLMTag::ChaosUpdate);
		SCOPE_CYCLE_COUNTER(STAT_ChaosTick);
		CSV_SCOPED_TIMING_STAT_EXCLUSIVE(Physics);
		CSV_SCOPED_TIMING_STAT(PhysicsVerbose, StepSolver);

		//We are on GT, but we know PhysicsThread is waiting so we're actually going to operate on PT data
#if PHYSICS_THREAD_CONTEXT
		FPhysicsThreadContextScope Scope(/*IsPhysicsThreadContext=*/true);
		FFrozenGameThreadContextScope FrozenScope;	//Make sure we fire ensures if any physics GT data is used
#endif

		Solver.SetGameThreadFrozen(true);
		Solver.ApplyCallbacks_Internal();
		Solver.SetGameThreadFrozen(false);
		
	}


	FPhysicsSolverAdvanceTask::FPhysicsSolverAdvanceTask(FPhysicsSolverBase& InSolver, FPushPhysicsData* InPushData)
		: Solver(InSolver)
		, PushData(InPushData)
	{
		CVD_GET_CURRENT_CONTEXT(CVDContext);
		Solver.NumPendingSolverAdvanceTasks++;
	}

	void FPhysicsSolverAdvanceTask::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		CVD_SCOPE_CONTEXT(CVDContext);

		AdvanceSolver();
	}


	void FPhysicsSolverAdvanceTask::AdvanceSolver()
	{
		CVD_SCOPE_TRACE_SOLVER_FRAME(FPhysicsSolverBase, Solver);
		LLM_SCOPE(ELLMTag::ChaosUpdate);
		SCOPE_CYCLE_COUNTER(STAT_ChaosTick);
		CSV_SCOPED_TIMING_STAT_EXCLUSIVE(Physics);
		CSV_SCOPED_TIMING_STAT(PhysicsVerbose, StepSolver);
		PHYSICS_CSV_CUSTOM_EXPENSIVE(PhysicsCounters, NumPendingSolverAdvanceTasks, NumPendingSolverAdvanceTasks, ECsvCustomStatOp::Max);

#if PHYSICS_THREAD_CONTEXT
		FPhysicsThreadContextScope Scope(/*IsPhysicsThreadContext=*/true);
#endif

		// StepFraction: how much of the remaining time this step represents, used to interpolate kinematic targets
		// E.g., for 4 steps this will be: 1/4, 1/3, 1/2, 1
		const FReal PseudoFraction = (FReal)1 / (FReal)(PushData->IntervalNumSteps - PushData->IntervalStep);

		Solver.AdvanceSolverBy(FSubStepInfo{ PseudoFraction, PushData->IntervalStep, PushData->IntervalNumSteps, PushData->bSolverSubstepped });

		{
			SCOPE_CYCLE_COUNTER(STAT_ResetMarshallingData);
			Solver.GetMarshallingManager().FreeDataToHistory_Internal(PushData);	//cannot use push data after this point
			PushData = nullptr;
		}

		{
			SCOPE_CYCLE_COUNTER(STAT_ConditionalApplyRewind);
			Solver.ConditionalApplyRewind_Internal();
		}

		Solver.NumPendingSolverAdvanceTasks--;
	}

	CHAOS_API int32 UseAsyncInterpolation = 1;
	FAutoConsoleVariableRef CVarUseAsyncInterpolation(TEXT("p.UseAsyncInterpolation"), UseAsyncInterpolation, TEXT("Whether to interpolate when async mode is enabled"));

	CHAOS_API int32 ForceDisableAsyncPhysics = 0;
	FAutoConsoleVariableRef CVarForceDisableAsyncPhysics(TEXT("p.ForceDisableAsyncPhysics"), ForceDisableAsyncPhysics, TEXT("Whether to force async physics off regardless of other settings"));

	auto LambdaMul = FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
		{
			for (FPhysicsSolverBase* Solver : FChaosSolversModule::GetModule()->GetAllSolvers())
			{
				Solver->SetAsyncInterpolationMultiplier(InVariable->GetFloat());
			}
		});

	auto LambdaAsyncMode = FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
		{
			for (FPhysicsSolverBase* Solver : FChaosSolversModule::GetModule()->GetAllSolvers())
			{
				Solver->SetAsyncPhysicsBlockMode(EAsyncBlockMode(InVariable->GetInt()));
			}
		});

	CHAOS_API FRealSingle AsyncInterpolationMultiplier = 2.f;
	FAutoConsoleVariableRef CVarAsyncInterpolationMultiplier(TEXT("p.AsyncInterpolationMultiplier"), AsyncInterpolationMultiplier, TEXT("How many multiples of the fixed dt should we look behind for interpolation"), LambdaMul);

	// 0 blocks on any physics steps generated from past GT Frames, and blocks on none of the tasks from current frame.
	// 1 blocks on everything except the single most recent task (including tasks from current frame)
	// 1 should guarantee we will always have a future output for interpolation from 2 frames in the past
	// 2 doesn't block the game thread. Physics steps could be eventually be dropped if taking too much time.
	int32 AsyncPhysicsBlockMode = 0;
	FAutoConsoleVariableRef CVarAsyncPhysicsBlockMode(TEXT("p.AsyncPhysicsBlockMode"), AsyncPhysicsBlockMode, TEXT("Setting to 0 blocks on any physics steps generated from past GT Frames, and blocks on none of the tasks from current frame."
		" 1 blocks on everything except the single most recent task (including tasks from current frame). 1 should gurantee we will always have a future output for interpolation from 2 frames in the past."
		" 2 doesn't block the game thread, physics steps could be eventually be dropped if taking too much time."), LambdaAsyncMode);

	FPhysicsSolverBase::FPhysicsSolverBase(const EMultiBufferMode BufferingModeIn,const EThreadingModeTemp InThreadingMode,UObject* InOwner, Chaos::FReal InAsyncDt)
		: BufferMode(BufferingModeIn)
		, ThreadingMode(!!GSingleThreadedPhysics ? EThreadingModeTemp::SingleThread : InThreadingMode)
#if CHAOS_DEBUG_NAME
		, DebugName(NAME_None)
#endif
		, PullResultsManager(MakeUnique<FChaosResultsManager>(MarshallingManager))
		, PendingSpatialOperations_External(MakeUnique<FPendingSpatialDataQueue>())
		, bUseCollisionResimCache(false)
		, NumPendingSolverAdvanceTasks(0)
		, bPaused_External(false)
		, Owner(InOwner)
		, ExternalDataLock_External(new FPhysSceneLock())
		, bIsShuttingDown(false)
		, AsyncDt(InAsyncDt)
		, AccumulatedTime(0)
		, MMaxDeltaTime(0.0)
		, MMinDeltaTime(UE_SMALL_NUMBER)
		, MMaxSubSteps(1)
		, ExternalSteps(0)
		, AsyncBlockMode(EAsyncBlockMode(AsyncPhysicsBlockMode))
		, AsyncMultiplier(AsyncInterpolationMultiplier)
#if !UE_BUILD_SHIPPING
		, bStealAdvanceTasksForTesting(false)
#endif
	{
		UE_LOG(LogChaos, Verbose, TEXT("FPhysicsSolverBase::AsyncDt:%f"), IsUsingAsyncResults() ? AsyncDt : -1);

		//If user is running with -PhysicsRunsOnGT override the cvar (doing it here to avoid parsing every time task is scheduled)
		if(FParse::Param(FCommandLine::Get(), TEXT("PhysicsRunsOnGT")))
		{
			PhysicsRunsOnGT = 1;
		}
	}

#if CHAOS_DEBUG_NAME
	void FPhysicsSolverBase::SetDebugName(const FName& Name)
	{
		DebugName = Name;
		OnDebugNameChanged();
	}
#endif

	FName FPhysicsSolverBase::GetDebugName() const
	{
#if CHAOS_DEBUG_NAME
		return DebugName;
#else
		return NAME_None;
#endif
	}

	void FPhysicsSolverBase::EnableAsyncMode(FReal FixedDt)
	{
		AsyncDt = FixedDt;
		if (AsyncDt != FixedDt)
		{
			AccumulatedTime = 0;
			UE_LOG(LogChaos, Verbose, TEXT("FPhysicsSolverBase::AsyncDt:%f"), IsUsingAsyncResults() ? AsyncDt : -1);
		}
	}

	void FPhysicsSolverBase::DisableAsyncMode()
	{
		AsyncDt = -1;
		UE_LOG(LogChaos, Verbose, TEXT("FPhysicsSolverBase::AsyncDt:%f"), AsyncDt);
	}


	FPhysicsSolverBase::~FPhysicsSolverBase()
	{
		//reset history buffer before freeing any unremoved callback objects
		MarshallingManager.SetHistoryLength_Internal(0);

		//if any callback objects are still registered, just delete them here
		for(ISimCallbackObject* CallbackObject : SimCallbackObjects)
		{
			delete CallbackObject;
		}
	}

	void FPhysicsSolverBase::DestroySolver(FPhysicsSolverBase& InSolver)
	{
		// Please read the comments this is a minefield.
				
		const bool bIsSingleThreadEnvironment = FPlatformProcess::SupportsMultithreading() == false;
		if (bIsSingleThreadEnvironment == false)
		{
			// In Multithreaded: DestroySolver should only be called if we are not waiting on async work.
			// This should be called when World/Scene are cleaning up, World implements IsReadyForFinishDestroy() and returns false when async work is still going.
			// This means that garbage collection should not cleanup world and this solver until this async work is complete.
			// We do it this way because it is unsafe for us to block on async task in this function, as it is unsafe to block on a task during GC, as this may schedule
			// another task that may be unsafe during GC, and cause crashes.
			ensure(InSolver.IsPendingTasksComplete());
		}
		else
		{
			// In Singlethreaded: We cannot wait for any tasks in IsReadyForFinishDestroy() (on World) so it always returns true in single threaded.
			// Task will never complete during GC in single theading, as there are no threads to do it.
			// so we have this wait below to allow single threaded to complete pending tasks before solver destroy.

			InSolver.WaitOnPendingTasks_External();
		}

		// GeometryCollection particles do not always remove collision constraints on unregister,
		// explicitly clear constraints so we will not crash when filling collision events in advance.
		// @todo(chaos): fix this and remove
		{
			auto* Evolution = static_cast<FPBDRigidsSolver&>(InSolver).GetEvolution();
			if (Evolution)
			{
				Evolution->ResetConstraints();
			}
		}

		// Advance in single threaded because we cannot block on an async task here if in multi threaded mode. see above comments.
		InSolver.SetThreadingMode_External(EThreadingModeTemp::SingleThread);
		InSolver.MarkShuttingDown();
		{
			InSolver.AdvanceAndDispatch_External(0);	//flush any pending commands are executed (for example unregister object)
		}

		// verify callbacks have been processed and we're not leaking.
		// TODO: why is this still firing in 14.30? (Seems we're still leaking)
		//ensure(InSolver.SimCallbacks.Num() == 0);

		delete &InSolver;
	}

	void FPhysicsSolverBase::UpdateParticleInAccelerationStructure_External(FGeometryParticle* Particle, EPendingSpatialDataOperation InOperation)
	{
		//mark it as pending for async structure being built
		FAccelerationStructureHandle AccelerationHandle(Particle);
		FPendingSpatialData& SpatialData = PendingSpatialOperations_External->FindOrAdd(Particle->UniqueIdx());

		//make sure any new operations (i.e not currently being consumed by sim) are not acting on a deleted object
		ensure(SpatialData.SyncTimestamp < MarshallingManager.GetExternalTimestamp_External() || SpatialData.Operation != EPendingSpatialDataOperation::Delete);

		SpatialData.Operation = InOperation;
		SpatialData.SpatialIdx = Particle->SpatialIdx();
		SpatialData.AccelerationHandle = AccelerationHandle;
		SpatialData.SyncTimestamp = MarshallingManager.GetExternalTimestamp_External();
	}

	void FPhysicsSolverBase::EnqueueSimcallbackRewindRegisteration(ISimCallbackObject* Callback)
	{
		EnqueueCommandImmediate([this, Callback]()
		{
			if (ensure(MRewindCallback.IsValid()))
			{
				MRewindCallback->RegisterRewindableSimCallback_Internal(Callback);
			}
		});
	}

#if !UE_BUILD_SHIPPING
	void FPhysicsSolverBase::SetStealAdvanceTasks_ForTesting(bool bInStealAdvanceTasksForTesting)
	{
		bStealAdvanceTasksForTesting = bInStealAdvanceTasksForTesting;
	}

	void FPhysicsSolverBase::PopAndExecuteStolenAdvanceTask_ForTesting()
	{
		ensure(ThreadingMode == EThreadingModeTemp::SingleThread);
		if (ensure(StolenSolverAdvanceTasks.Num() > 0))
		{
			StolenSolverAdvanceTasks[0].AdvanceSolver();
			StolenSolverAdvanceTasks.RemoveAt(0);
		}
	}
#endif

	void FPhysicsSolverBase::TrackGTParticle_External(FGeometryParticle& Particle)
	{
		const int32 Idx = Particle.UniqueIdx().Idx;
		const int32 SlotsNeeded = Idx + 1 - UniqueIdxToGTParticles.Num();
		if (SlotsNeeded > 0)
		{
			UniqueIdxToGTParticles.AddZeroed(SlotsNeeded);
		}

		UniqueIdxToGTParticles[Idx] = &Particle;
	}

	void FPhysicsSolverBase::ClearGTParticle_External(FGeometryParticle& Particle)
	{
		const int32 Idx = Particle.UniqueIdx().Idx;
		if (ensure(Idx < UniqueIdxToGTParticles.Num()))
		{
			UniqueIdxToGTParticles[Idx] = nullptr;
		}
	}

	void FPhysicsSolverBase::SetRewindCallback(TUniquePtr<IRewindCallback>&& RewindCallback)
	{
		ensure(RewindCallback);
		MRewindCallback = MoveTemp(RewindCallback);

		if (MRewindData.IsValid())
		{
			MRewindCallback->RewindData = MRewindData.Get();
		}
	}

	FGraphEventRef FPhysicsSolverBase::AdvanceAndDispatch_External(FReal InDt)
	{
		const bool bSubstepping = MMaxSubSteps > 1;
		SetSolverSubstep_External(bSubstepping);
		const FReal DtWithPause = bPaused_External ? 0.0f : InDt;
		FReal InternalDt = DtWithPause;
		int32 NumSteps = 1;

		if(IsUsingFixedDt())
		{
			AccumulatedTime += DtWithPause;
			if(InDt == 0)	//this is a special flush case
			{
				//just use any remaining time and sync up to latest no matter what
				InternalDt = AccumulatedTime;
				NumSteps = 1;
				AccumulatedTime = 0;
			}
			else
			{

				InternalDt = AsyncDt;
				NumSteps = FMath::FloorToInt32(AccumulatedTime / InternalDt);
				AccumulatedTime -= InternalDt * static_cast<FReal>(NumSteps);
			}
		}
		else if (bSubstepping && InDt > 0)
		{
			NumSteps = FMath::CeilToInt32(DtWithPause / MMaxDeltaTime);
			if (NumSteps > MMaxSubSteps)
			{
				// Hitting this case means we're losing time, given the constraints of MaxSteps and MaxDt we can't
				// fully handle the Dt requested, the simulation will appear to the viewer to run slower than realtime
				NumSteps = MMaxSubSteps;
				InternalDt = MMaxDeltaTime;
			}
			else
			{
				InternalDt = DtWithPause / static_cast<FReal>(NumSteps);
			}
		}

		if(InDt > 0)
		{
			ExternalSteps++;	//we use this to average forces. It assumes external dt is about the same. 0 dt should be ignored as it typically has nothing to do with force
		}

		// Eventually drop physics steps in mode 2
		if (AsyncBlockMode == EAsyncBlockMode::DoNoBlock)
		{
			// Make sure not to accumulate too many physics solver tasks.
			constexpr int32 MaxPhysicsStepToKeep = 3;
			const int32 MaxNumSteps = MaxPhysicsStepToKeep - NumPendingSolverAdvanceTasks;
			if (NumSteps > MaxNumSteps)
			{
				CSV_CUSTOM_STAT(ChaosPhysicsSolver, PhysicsFrameDropped, NumSteps - MaxNumSteps, ECsvCustomStatOp::Accumulate);
				// NumSteps + NumPendingSolverAdvanceTasks shouldn't be bigger than MaxPhysicsStepToKeep
				NumSteps = FMath::Min<int32>(NumSteps, MaxNumSteps);
			}
		}
			
		if (NumSteps > 0)
		{
			//make sure any GT state is pushed into necessary buffer
			PushPhysicsState(InternalDt, NumSteps, FMath::Max(ExternalSteps, 1));
			ExternalSteps = 0;
		}

		// Ensures we block on any tasks generated from previous frames
		FGraphEventRef BlockingTasks = PendingTasks;

		while(FPushPhysicsData* PushData = MarshallingManager.StepInternalTime_External())
		{
			if(ShouldApplyRewindCallbacks() && !bIsShuttingDown)
			{
				MRewindCallback->ProcessInputs_External(PushData->InternalStep, PushData->SimCallbackInputs);
			}

			if(ThreadingMode == EThreadingModeTemp::SingleThread)
			{
				ensure(!PendingTasks || PendingTasks->IsComplete());	//if mode changed we should have already blocked
				FAllSolverTasks ImmediateTask(*this, PushData);
#if !UE_BUILD_SHIPPING
				if(bStealAdvanceTasksForTesting)
				{
					StolenSolverAdvanceTasks.Emplace(MoveTemp(ImmediateTask));
				}
				else
				{
					ImmediateTask.AdvanceSolver();
				}
#else
				ImmediateTask.AdvanceSolver();
#endif
			}
			else
			{
				// If enabled, block on all but most recent physics task, even tasks generated this frame.
				if (AsyncBlockMode == EAsyncBlockMode::BlockForBestInterpolation)
				{
					BlockingTasks = PendingTasks;
				}

				FGraphEventArray Prereqs;
				if (PendingTasks && !PendingTasks->IsComplete())
				{
					Prereqs.Add(PendingTasks);
				}

				PendingTasks = TGraphTask<FPhysicsSolverProcessPushDataTask>::CreateTask(&Prereqs).ConstructAndDispatchWhenReady(*this, PushData);
				Prereqs.Add(PendingTasks);

				if (bSolverHasFrozenGameThreadCallbacks)
				{
					PendingTasks = TGraphTask<FPhysicsSolverFrozenGTPreSimCallbacks>::CreateTask(&Prereqs).ConstructAndDispatchWhenReady(*this);
					Prereqs.Add(PendingTasks);
				}

				PendingTasks = TGraphTask<FPhysicsSolverAdvanceTask>::CreateTask(&Prereqs).ConstructAndDispatchWhenReady(*this, PushData);

				if (IsUsingAsyncResults() == false)
				{
					BlockingTasks = PendingTasks;	//block right away
				}
			}

			// This break is mainly here to satisfy unit testing. The call to StepInternalTime_External will decrement the
			// delay in the marshaling manager and throw of tests that are explicitly testing for propagation delays
			if (IsUsingAsyncResults() == false && !bSubstepping)
			{
				break;
			}
		}
		if (AsyncBlockMode == EAsyncBlockMode::DoNoBlock)
		{
			return {};
		}
		return BlockingTasks;
	}


	void FAllSolverTasks::AdvanceSolver()
	{
		ProcessPushData.ProcessPushData();
		GTPreSimCallbacks.GTPreSimCallbacks();
		AdvanceTask.AdvanceSolver();
	}

	void FSolverTasksPTOnly::AdvanceSolver()
	{
		ProcessPushData.ProcessPushData();
		AdvanceTask.AdvanceSolver();
	}
}
