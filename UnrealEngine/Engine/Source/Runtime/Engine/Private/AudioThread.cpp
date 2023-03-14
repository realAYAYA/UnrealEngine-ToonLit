// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AudioThread.cpp: Audio thread implementation.
=============================================================================*/

#include "AudioThread.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "HAL/ExceptionHandling.h"
#include "Misc/CoreStats.h"
#include "UObject/UObjectGlobals.h"
#include "Audio.h"
#include "HAL/LowLevelMemTracker.h"
#include "Async/Async.h"
#include "Tasks/Pipe.h"

//
// Globals
//

extern CORE_API UE::Tasks::FPipe GAudioPipe;
extern CORE_API std::atomic<bool> GIsAudioThreadRunning;
extern CORE_API std::atomic<bool> GIsAudioThreadSuspended;

static int32 GCVarSuspendAudioThread = 0;
FAutoConsoleVariableRef CVarSuspendAudioThread(TEXT("AudioThread.SuspendAudioThread"), GCVarSuspendAudioThread, TEXT("0=Resume, 1=Suspend"), ECVF_Cheat);

static int32 GCVarAboveNormalAudioThreadPri = 0;
FAutoConsoleVariableRef CVarAboveNormalAudioThreadPri(TEXT("AudioThread.AboveNormalPriority"), GCVarAboveNormalAudioThreadPri, TEXT("0=Normal, 1=AboveNormal"), ECVF_Default);

static int32 GCVarEnableAudioCommandLogging = 0;
FAutoConsoleVariableRef CVarEnableAudioCommandLogging(TEXT("AudioThread.EnableAudioCommandLogging"), GCVarEnableAudioCommandLogging, TEXT("0=Disbaled, 1=Enabled"), ECVF_Default);

static int32 GCVarEnableBatchProcessing = 1;
FAutoConsoleVariableRef CVarEnableBatchProcessing(
	TEXT("AudioThread.EnableBatchProcessing"),
	GCVarEnableBatchProcessing,
	TEXT("Enables batch processing audio thread commands.\n")
	TEXT("0: Not Enabled, 1: Enabled"),
	ECVF_Default); 

static int32 GBatchAudioAsyncBatchSize = 128;
static FAutoConsoleVariableRef CVarBatchAudioAsyncBatchSize(
	TEXT("AudioThread.BatchAsyncBatchSize"),
	GBatchAudioAsyncBatchSize,
	TEXT("When AudioThread.EnableBatchProcessing = 1, controls the number of audio commands grouped together for threading.")
);

static int32 GAudioCommandFenceWaitTimeMs = 35;
FAutoConsoleVariableRef  CVarAudioCommandFenceWaitTimeMs(
	TEXT("AudioCommand.FenceWaitTimeMs"),
	GAudioCommandFenceWaitTimeMs, 
	TEXT("Sets number of ms for fence wait"), 
	ECVF_Default);

struct FAudioThreadInteractor
{
	static void UseAudioThreadCVarSinkFunction()
	{
		static bool bLastSuspendAudioThread = false;
		const bool bSuspendAudioThread = GCVarSuspendAudioThread != 0;

		if (bLastSuspendAudioThread != bSuspendAudioThread)
		{
			bLastSuspendAudioThread = bSuspendAudioThread;
			if (bSuspendAudioThread && IsAudioThreadRunning())
			{
				FAudioThread::SuspendAudioThread();
			}
			else if (GIsAudioThreadSuspended)
			{
				FAudioThread::ResumeAudioThread();
			}
			else if (GIsEditor)
			{
				UE_LOG(LogAudio, Warning, TEXT("Audio threading is disabled in the editor."));
			}
			else if (!FAudioThread::IsUsingThreadedAudio())
			{
				UE_LOG(LogAudio, Warning, TEXT("Cannot manipulate audio thread when disabled by platform or ini."));
			}
		}
	}
};

UE::Tasks::ETaskPriority GAudioTaskPriority = UE::Tasks::ETaskPriority::Normal;

static void SetAudioTaskPriority(const TArray<FString>& Args)
{
	UE_LOG(LogConsoleResponse, Display, TEXT("AudioTaskPriority was %s."), LowLevelTasks::ToString(GAudioTaskPriority));

	if (Args.Num() > 1)
	{
		UE_LOG(LogConsoleResponse, Display, TEXT("WARNING: This command requires a single argument while %d were provided, all extra arguments will be ignored."), Args.Num());
	}
	else if (Args.IsEmpty())
	{
		UE_LOG(LogConsoleResponse, Display, TEXT("ERROR: Please provide a new priority value."));
		return;
	}

	if (!LowLevelTasks::ToTaskPriority(*Args[0], GAudioTaskPriority))
	{
		UE_LOG(LogConsoleResponse, Display, TEXT("ERROR: Invalid priority: %s."), *Args[0]);
	}

	UE_LOG(LogConsoleResponse, Display, TEXT("Audio Task Priority was set to %s."), LowLevelTasks::ToString(GAudioTaskPriority));
}

static FAutoConsoleCommand AudioThreadPriorityConsoleCommand(
	TEXT("AudioThread.TaskPriority"),
	TEXT("Takes a single parameter of value `High`, `Normal`, `BackgroundHigh`, `BackgroundNormal` or `BackgroundLow`."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&SetAudioTaskPriority)
);

static FAutoConsoleVariableSink CVarUseAudioThreadSink(FConsoleCommandDelegate::CreateStatic(&FAudioThreadInteractor::UseAudioThreadCVarSinkFunction));

bool FAudioThread::bUseThreadedAudio = false;

FCriticalSection FAudioThread::CurrentAudioThreadStatIdCS;
TStatId FAudioThread::CurrentAudioThreadStatId;
TStatId FAudioThread::LongestAudioThreadStatId;
double FAudioThread::LongestAudioThreadTimeMsec = 0.0;

#if UE_AUDIO_THREAD_AS_PIPE

TUniquePtr<UE::Tasks::FTaskEvent> FAudioThread::ResumeEvent;
int32 FAudioThread::SuspendCount{ 0 };

void FAudioThread::SuspendAudioThread()
{
	check(IsInGameThread()); // thread-safe version would be much more complicated

	if (!GIsAudioThreadRunning)
	{
		return; // nothing to suspend
	}

	if (++SuspendCount != 1)
	{
		return; // recursive scope
	}

	check(!GIsAudioThreadSuspended.load(std::memory_order_relaxed));

	FEventRef SuspendEvent;

	FAudioThread::RunCommandOnAudioThread(
		[&SuspendEvent]
		{
			GIsAudioThreadSuspended.store(true, std::memory_order_release);
			UE::Tasks::AddNested(*ResumeEvent);
			SuspendEvent->Trigger();
		}
	);

	// release batch processing so the task above will be executed
	FAudioThread::ProcessAllCommands();

	// wait for the command above to block audio processing
	SuspendEvent->Wait();
}

void FAudioThread::ResumeAudioThread()
{
	check(IsInGameThread());

	if (!GIsAudioThreadRunning)
	{
		return; // nothing to resume
	}

	if (--SuspendCount != 0)
	{
		return; // recursive scope
	}

	check(GIsAudioThreadSuspended.load(std::memory_order_relaxed));
	GIsAudioThreadSuspended.store(false, std::memory_order_release);

	check(!ResumeEvent->IsCompleted());
	ResumeEvent->Trigger();
	ResumeEvent = MakeUnique<UE::Tasks::FTaskEvent>(UE_SOURCE_LOCATION);
}

#else // UE_AUDIO_THREAD_AS_PIPE

FRunnable* FAudioThread::AudioThreadRunnable = nullptr;

/** The audio thread main loop */
void AudioThreadMain( FEvent* TaskGraphBoundSyncEvent )
{
	FTaskGraphInterface::Get().AttachToThread(ENamedThreads::AudioThread);
	FPlatformMisc::MemoryBarrier();

	// Inform main thread that the audio thread has been attached to the taskgraph and is ready to receive tasks
	if( TaskGraphBoundSyncEvent != nullptr )
	{
		TaskGraphBoundSyncEvent->Trigger();
	}

	FTaskGraphInterface::Get().ProcessThreadUntilRequestReturn(ENamedThreads::AudioThread);
	FPlatformMisc::MemoryBarrier();
}

FAudioThread::FAudioThread()
{
	TaskGraphBoundSyncEvent	= FPlatformProcess::GetSynchEventFromPool(true);

	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddRaw(this, &FAudioThread::OnPreGarbageCollect);
	FCoreUObjectDelegates::GetPostGarbageCollect().AddRaw(this, &FAudioThread::OnPostGarbageCollect);

	FCoreUObjectDelegates::PreGarbageCollectConditionalBeginDestroy.AddRaw(this, &FAudioThread::OnPreGarbageCollect);
	FCoreUObjectDelegates::PostGarbageCollectConditionalBeginDestroy.AddRaw(this, &FAudioThread::OnPostGarbageCollect);
}

FAudioThread::~FAudioThread()
{
	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().RemoveAll(this);
	FCoreUObjectDelegates::GetPostGarbageCollect().RemoveAll(this);

	FCoreUObjectDelegates::PreGarbageCollectConditionalBeginDestroy.RemoveAll(this);
	FCoreUObjectDelegates::PostGarbageCollectConditionalBeginDestroy.RemoveAll(this);

	FPlatformProcess::ReturnSynchEventToPool(TaskGraphBoundSyncEvent);
	TaskGraphBoundSyncEvent = nullptr;
}

static int32 AudioThreadSuspendCount = 0;

void FAudioThread::SuspendAudioThread()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SuspendAudioThread);
	check(FPlatformTLS::GetCurrentThreadId() == GGameThreadId);
	check(!GIsAudioThreadSuspended.load() || GCVarSuspendAudioThread != 0);

	if (IsAudioThreadRunning())
	{
		// Make GC wait on the audio thread finishing processing
		FAudioCommandFence AudioFence;
		AudioFence.BeginFence();
		AudioFence.Wait();

		GIsAudioThreadSuspended = true;
		FPlatformMisc::MemoryBarrier();
	}
	check(!IsAudioThreadRunning());
}

void FAudioThread::ResumeAudioThread()
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS

	check(FPlatformTLS::GetCurrentThreadId() == GGameThreadId);
	if(GIsAudioThreadSuspended.load() && GCVarSuspendAudioThread == 0)
	{
		GIsAudioThreadSuspended = false;
		FPlatformMisc::MemoryBarrier();
	}
	ProcessAllCommands();

PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FAudioThread::OnPreGarbageCollect()
{
	AudioThreadSuspendCount++;
	if (AudioThreadSuspendCount == 1)
	{
		SuspendAudioThread();
	}
}

void FAudioThread::OnPostGarbageCollect()
{
	AudioThreadSuspendCount--;
	if (AudioThreadSuspendCount == 0)
	{
		ResumeAudioThread();
	}
}

#if !UE_AUDIO_THREAD_AS_PIPE

bool FAudioThread::Init()
{ 
	GIsAudioThreadRunning.store(true, std::memory_order_release);
	return true;
}

void FAudioThread::Exit()
{
	GIsAudioThreadRunning.store(false, std::memory_order_release);
}

#endif

uint32 FAudioThread::Run()
{
	LLM_SCOPE(ELLMTag::AudioMisc);
	SCOPED_NAMED_EVENT(FAudioThread_Run, FColor::Blue);

	FMemory::SetupTLSCachesOnCurrentThread();
	FPlatformProcess::SetupAudioThread();
	AudioThreadMain( TaskGraphBoundSyncEvent );
	FMemory::ClearAndDisableTLSCachesOnCurrentThread();
	return 0;
}

#endif // UE_AUDIO_THREAD_AS_PIPE

void FAudioThread::SetUseThreadedAudio(const bool bInUseThreadedAudio)
{
	if (IsAudioThreadRunning() && !bInUseThreadedAudio)
	{
		UE_LOG(LogAudio, Error, TEXT("You cannot disable using threaded audio once the thread has already begun running."));
	}
	else
	{
		bUseThreadedAudio = bInUseThreadedAudio;
	}
}

bool FAudioThread::IsUsingThreadedAudio()
{
	return bUseThreadedAudio;
}

#if UE_AUDIO_THREAD_AS_PIPE

// batching audio commands allows to avoid the overhead of launching a task per command when resources are limited.
// We assume that resources are limited if the previous batch is not completed yet (potentially waiting for execution due to the CPU being busy
// with something else). Otherwise we don't wait until we collect a full batch.
struct FAudioAsyncBatcher
{
	using FWork = TUniqueFunction<void()>;
	TArray<FWork> WorkItems;

	UE::Tasks::FTask LastBatch;

	void Add(FWork&& Work)
	{
		check(IsInGameThread());

#if !WITH_EDITOR
		if (GCVarEnableBatchProcessing)
		{
			if (WorkItems.Num() >= GBatchAudioAsyncBatchSize) // collected enough work
			{
				Flush();
			}
			WorkItems.Add(Forward<FWork>(Work));
		}
#else
		LastBatch = GAudioPipe.Launch(UE_SOURCE_LOCATION, MoveTemp(Work));
#endif
	}

	void Flush()
	{
		check(IsInGameThread());

		if (WorkItems.IsEmpty())
		{
			return;
		}

		LastBatch = GAudioPipe.Launch(TEXT("AudioBatch"),
			[WorkItems = MoveTemp(WorkItems)]() mutable
			{
				LLM_SCOPE(ELLMTag::AudioMisc);

				for (FWork& Work : WorkItems)
				{
					Work();
				}
			},
			GAudioTaskPriority
		);
		WorkItems.Reset();
	}
};

#else // UE_AUDIO_THREAD_AS_PIPE

struct FAudioAsyncBatcher
{
	FGraphEventArray DispatchEvent;
	int32 NumBatched = 0;


	FGraphEventArray* GetAsyncPrereq()
	{
		check(IsInGameThread());
#if !WITH_EDITOR
		if (GCVarEnableBatchProcessing)
		{
			if (NumBatched >= GBatchAudioAsyncBatchSize || !DispatchEvent.Num() || !DispatchEvent[0].GetReference() || DispatchEvent[0]->IsComplete())
			{
				Flush();
			}
			if (DispatchEvent.Num() == 0)
			{
				check(NumBatched == 0);
				DispatchEvent.Add(FGraphEvent::CreateGraphEvent());
			}
			NumBatched++;
			return &DispatchEvent;
		}
#endif
		return nullptr;
	}

	void Flush()
	{
		check(IsInGameThread());
		if (NumBatched)
		{
			check(DispatchEvent.Num() && DispatchEvent[0].GetReference() && !DispatchEvent[0]->IsComplete());
			FGraphEventRef Dispatch = DispatchEvent[0];
			TFunction<void()> FlushAudioCommands = [Dispatch]()
			{
				Dispatch->DispatchSubsequents();
			};

			FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(FlushAudioCommands), TStatId(), nullptr, ENamedThreads::AudioThread);

			DispatchEvent.Empty();
			NumBatched = 0;
		}
	}

};

#endif // UE_AUDIO_THREAD_AS_PIPE

static FAudioAsyncBatcher GAudioAsyncBatcher;

TUniqueFunction<void()> FAudioThread::GetCommandWrapper(TUniqueFunction<void()> InFunction, const TStatId InStatId)
{
	if (GCVarEnableAudioCommandLogging == 1)
	{
		return [Function = MoveTemp(InFunction), InStatId]()
		{
#if !UE_AUDIO_THREAD_AS_PIPE
			FTaskTagScope Scope(ETaskTag::EAudioThread);
#endif
			FScopeCycleCounter ScopeCycleCounter(InStatId);
			FAudioThread::SetCurrentAudioThreadStatId(InStatId);

			// Time the execution of the function
			const double StartTime = FPlatformTime::Seconds();

			// Execute the function
			Function();

			// Track the longest one
			const double DeltaTime = (FPlatformTime::Seconds() - StartTime) * 1000.0f;
			if (DeltaTime > GetCurrentLongestTime())
			{
				SetLongestTimeAndId(InStatId, DeltaTime);
			}
		};
	}
	else
	{
		return [Function = MoveTemp(InFunction), InStatId]()
		{
#if !UE_AUDIO_THREAD_AS_PIPE
			FTaskTagScope Scope(ETaskTag::EAudioThread);
#endif
			FScopeCycleCounter ScopeCycleCounter(InStatId);
			Function();
		};
	}
}

void FAudioThread::RunCommandOnAudioThread(TUniqueFunction<void()> InFunction, const TStatId InStatId)
{
#if UE_AUDIO_THREAD_AS_PIPE

	TUniqueFunction<void()> CommandWrapper{ GetCommandWrapper(MoveTemp(InFunction), InStatId) };
	if (IsInAudioThread())
	{
		// it's audio-thread-safe so execute the command in-place
		CommandWrapper();
	}
	else if (IsInGameThread())
	{
		// batch commands to minimise game thread overhead
		GAudioAsyncBatcher.Add(MoveTemp(CommandWrapper));
	}
	// we are on an unknown thread
	else if (IsUsingThreadedAudio())
	{
		GAudioPipe.Launch(TEXT("AudioCommand"), MoveTemp(CommandWrapper), GAudioTaskPriority);
	}
	else
	{
		// the command must be executed on the game thread
		AsyncTask(ENamedThreads::GameThread, MoveTemp(CommandWrapper));
	}

#else

	if (IsInAudioThread())
	{
		// it's audio-thread-safe so execute the command in-place
		FScopeCycleCounter ScopeCycleCounter(InStatId);
		InFunction();
		return;
	}

	TUniqueFunction<void()> CommandWrapper{ GetCommandWrapper(MoveTemp(InFunction), InStatId) };
	if (IsInGameThread())
	{
		// batch commands to minimise game thread overhead
		FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(CommandWrapper), TStatId{}, GAudioAsyncBatcher.GetAsyncPrereq(), ENamedThreads::AudioThread);
	}
	// we are on an unknown thread
	else if (IsUsingThreadedAudio())
	{
		FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(CommandWrapper), TStatId{}, nullptr, ENamedThreads::AudioThread);
	}
	else
	{
		// the command must be executed on the game thread
		FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(CommandWrapper), TStatId{}, nullptr, ENamedThreads::GameThread);
	}

#endif
}

void FAudioThread::SetCurrentAudioThreadStatId(TStatId InStatId)
{
	FScopeLock Lock(&CurrentAudioThreadStatIdCS);
	CurrentAudioThreadStatId = InStatId;
}

FString FAudioThread::GetCurrentAudioThreadStatId()
{
	FScopeLock Lock(&CurrentAudioThreadStatIdCS);
#if STATS
	return FString(CurrentAudioThreadStatId.GetStatDescriptionANSI());
#else
	return FString(TEXT("NoStats"));
#endif
}

void FAudioThread::ResetAudioThreadTimers()
{
	FScopeLock Lock(&CurrentAudioThreadStatIdCS);
	LongestAudioThreadStatId = TStatId();
	LongestAudioThreadTimeMsec = 0.0;
}

void FAudioThread::SetLongestTimeAndId(TStatId NewLongestId, double LongestTimeMsec)
{
	FScopeLock Lock(&CurrentAudioThreadStatIdCS);
	LongestAudioThreadTimeMsec = LongestTimeMsec;
	LongestAudioThreadStatId = NewLongestId;
}

void FAudioThread::GetLongestTaskInfo(FString& OutLongestTask, double& OutLongestTaskTimeMs)
{
	FScopeLock Lock(&CurrentAudioThreadStatIdCS);
#if STATS
	OutLongestTask = FString(LongestAudioThreadStatId.GetStatDescriptionANSI());
#else
	OutLongestTask = FString(TEXT("NoStats"));
#endif
	OutLongestTaskTimeMs = LongestAudioThreadTimeMsec;
}

void FAudioThread::ProcessAllCommands()
{
	if (IsAudioThreadRunning())
	{
		GAudioAsyncBatcher.Flush();
	}
	else
	{
#if UE_AUDIO_THREAD_AS_PIPE
		check(GAudioAsyncBatcher.WorkItems.IsEmpty());
#else
		check(FPlatformTLS::GetCurrentThreadId() == GGameThreadId);
#endif
	}
}

void FAudioThread::RunCommandOnGameThread(TUniqueFunction<void()> InFunction, const TStatId InStatId)
{
	if (IsAudioThreadRunning())
	{
		check(IsInAudioThread());
		FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(InFunction), InStatId, nullptr, ENamedThreads::GameThread);
	}
	else
	{
		check(IsInGameThread());
		FScopeCycleCounter ScopeCycleCounter(InStatId);
		InFunction();
	}
}

#if UE_AUDIO_THREAD_AS_PIPE

FDelegateHandle FAudioThread::PreGC;
FDelegateHandle FAudioThread::PostGC;
FDelegateHandle FAudioThread::PreGCDestroy;
FDelegateHandle FAudioThread::PostGCDestroy;

void FAudioThread::StartAudioThread()
{
	check(IsInGameThread());

	check(!GIsAudioThreadRunning.load(std::memory_order_relaxed));

	if (!bUseThreadedAudio)
	{
		return;
	}

	PreGC = FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddStatic(&FAudioThread::SuspendAudioThread);
	PostGC = FCoreUObjectDelegates::GetPostGarbageCollect().AddStatic(&FAudioThread::ResumeAudioThread);

	PreGCDestroy = FCoreUObjectDelegates::PreGarbageCollectConditionalBeginDestroy.AddStatic(&FAudioThread::SuspendAudioThread);
	PostGCDestroy = FCoreUObjectDelegates::PostGarbageCollectConditionalBeginDestroy.AddStatic(&FAudioThread::ResumeAudioThread);

	check(!ResumeEvent.IsValid());
	ResumeEvent = MakeUnique<UE::Tasks::FTaskEvent>(UE_SOURCE_LOCATION);

	GIsAudioThreadRunning.store(true, std::memory_order_release);
}

void FAudioThread::StopAudioThread()
{
	if (!IsAudioThreadRunning())
	{
		return;
	}

	GAudioAsyncBatcher.Flush();
	GAudioAsyncBatcher.LastBatch.Wait();
	GAudioAsyncBatcher.LastBatch = UE::Tasks::FTask{}; // release the task as it can hold some references

	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().Remove(PreGC);
	FCoreUObjectDelegates::GetPostGarbageCollect().Remove(PostGC);
	FCoreUObjectDelegates::PreGarbageCollectConditionalBeginDestroy.Remove(PreGCDestroy);
	FCoreUObjectDelegates::PostGarbageCollectConditionalBeginDestroy.Remove(PostGCDestroy);

	GIsAudioThreadRunning.store(false, std::memory_order_release);

	check(ResumeEvent.IsValid());
	ResumeEvent->Trigger(); // every FTaskEvent must be triggered before destruction to pass the check for completion
	ResumeEvent.Reset();
}

FAudioCommandFence::~FAudioCommandFence()
{
	check(IsInGameThread());
	Fence.Wait();
}

void FAudioCommandFence::BeginFence()
{
	check(IsInGameThread());

	if (!IsAudioThreadRunning())
	{
		return;
	}

	Fence.Wait();
	FAudioThread::ProcessAllCommands();
	Fence = GAudioAsyncBatcher.LastBatch;
}

bool FAudioCommandFence::IsFenceComplete() const
{
	check(IsInGameThread());
	return Fence.IsCompleted();
}

/**
 * Waits for pending fence commands to retire.
 */
void FAudioCommandFence::Wait(bool bProcessGameThreadTasks) const
{
	check(IsInGameThread());
	Fence.Wait();
	Fence = UE::Tasks::FTask{}; // release the task as it can hold some references
}

#else // UE_AUDIO_THREAD_AS_PIPE

void FAudioThread::StartAudioThread()
{
	check(FPlatformTLS::GetCurrentThreadId() == GGameThreadId);

	check(!IsAudioThreadRunning());
	check(!GIsAudioThreadSuspended);
	if (bUseThreadedAudio)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		check(GAudioThread == nullptr);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

		static uint32 ThreadCount = 0;
		check(!ThreadCount); // we should not stop and restart the audio thread; it is complexity we don't need.

		// Create the audio thread.
		AudioThreadRunnable = new FAudioThread();

PRAGMA_DISABLE_DEPRECATION_WARNINGS
		GAudioThread = 
PRAGMA_ENABLE_DEPRECATION_WARNINGS
			FRunnableThread::Create(AudioThreadRunnable, *FName(NAME_AudioThread).GetPlainNameString(), 0, (GCVarAboveNormalAudioThreadPri == 0) ? TPri_BelowNormal : TPri_AboveNormal, FPlatformAffinity::GetAudioThreadMask());

		// Wait for audio thread to have taskgraph bound before we dispatch any tasks for it.
		((FAudioThread*)AudioThreadRunnable)->TaskGraphBoundSyncEvent->Wait();

		// ensure the thread has actually started and is idling
		FAudioCommandFence Fence;
		Fence.BeginFence();
		Fence.Wait();

		ThreadCount++;
		
		if (GCVarSuspendAudioThread != 0)
		{
			SuspendAudioThread();
		}
	}
}

void FAudioThread::StopAudioThread()
{
	check(FPlatformTLS::GetCurrentThreadId() == GGameThreadId);
	check(!GIsAudioThreadSuspended.load() || GCVarSuspendAudioThread != 0);

	if (!IsAudioThreadRunning())
	{
		return;
	}

	FAudioCommandFence Fence;
	Fence.BeginFence();
	Fence.Wait();
	FGraphEventRef QuitTask = TGraphTask<FReturnGraphTask>::CreateTask(nullptr, ENamedThreads::GameThread).ConstructAndDispatchWhenReady(ENamedThreads::AudioThread);

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_StopAudioThread);
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(QuitTask, ENamedThreads::GameThread_Local);
	}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	// Wait for the audio thread to return.
	GAudioThread->WaitForCompletion();

	// Destroy the audio thread objects.
	delete GAudioThread;
	GAudioThread = nullptr;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	delete AudioThreadRunnable;
	AudioThreadRunnable = nullptr;
}

FAudioCommandFence::FAudioCommandFence()
	: FenceDoneEvent(nullptr)
{
}

FAudioCommandFence::~FAudioCommandFence()
{
	if (FenceDoneEvent)
	{
		FenceDoneEvent->Wait();

		FPlatformProcess::ReturnSynchEventToPool(FenceDoneEvent);
		FenceDoneEvent = nullptr;
	}
}

void FAudioCommandFence::BeginFence()
{
	if (IsAudioThreadRunning())
	{
		DECLARE_CYCLE_STAT(TEXT("FNullGraphTask.FenceAudioCommand"),
			STAT_FNullGraphTask_FenceAudioCommand,
			STATGROUP_TaskGraphTasks);

		CompletionEvent = TGraphTask<FNullGraphTask>::CreateTask(GAudioAsyncBatcher.GetAsyncPrereq(), ENamedThreads::GameThread).ConstructAndDispatchWhenReady(
			GET_STATID(STAT_FNullGraphTask_FenceAudioCommand), ENamedThreads::AudioThread);

		if (FenceDoneEvent)
		{
			FenceDoneEvent->Wait();

			FPlatformProcess::ReturnSynchEventToPool(FenceDoneEvent);
			FenceDoneEvent = nullptr;
		}

		FenceDoneEvent = FPlatformProcess::GetSynchEventFromPool(true);

		FTaskGraphInterface::Get().TriggerEventWhenTaskCompletes(FenceDoneEvent, CompletionEvent, ENamedThreads::GameThread, ENamedThreads::AudioThread);

		FAudioThread::ProcessAllCommands();
	}
	else
	{
		CompletionEvent = nullptr;
	}
}

bool FAudioCommandFence::IsFenceComplete() const
{
	FAudioThread::ProcessAllCommands();
	
	if (!CompletionEvent.GetReference() || CompletionEvent->IsComplete())
	{
		CompletionEvent = nullptr; // this frees the handle for other uses, the NULL state is considered completed
		return true;
	}

	check(IsAudioThreadRunning());

	return FenceDoneEvent->Wait(0);
}

/**
 * Waits for pending fence commands to retire.
 */
void FAudioCommandFence::Wait(bool bProcessGameThreadTasks) const
{
	FAudioThread::ProcessAllCommands();

	if (!IsFenceComplete()) // this checks the current thread
	{
		const double StartTime = FPlatformTime::Seconds();
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FAudioCommandFence_Wait);

		bool bDone = false;

		do
		{
			if (FenceDoneEvent)
			{
				bDone = FenceDoneEvent->Wait(GAudioCommandFenceWaitTimeMs);
			}
			else
			{
				bDone = true;
			}
			
			if(bDone && FenceDoneEvent)
			{
				FPlatformProcess::ReturnSynchEventToPool(FenceDoneEvent);
				FenceDoneEvent = nullptr;
			}

			// Log how long we've been waiting for the audio thread:
			float ThisTime = FPlatformTime::Seconds() - StartTime;
 			if (ThisTime > static_cast<float>(GAudioCommandFenceWaitTimeMs) / 1000.0f + SMALL_NUMBER)
			{
				if (GCVarEnableAudioCommandLogging == 1)
				{
					FString CurrentTask = FAudioThread::GetCurrentAudioThreadStatId();

					FString LongestTask;
					double LongestTaskTimeMs;
					FAudioThread::GetLongestTaskInfo(LongestTask, LongestTaskTimeMs);

					UE_LOG(LogAudio, Display, TEXT("Waited %.2f ms for audio thread. (Current Task: %s, Longest task: %s %.2f ms)"), ThisTime * 1000.0f, *CurrentTask, *LongestTask, LongestTaskTimeMs);
				}
				else
				{
					UE_LOG(LogAudio, Display,  TEXT("Waited %f ms for audio thread."), ThisTime * 1000.0f);
				}
			}
		} while (!bDone);

		FAudioThread::ResetAudioThreadTimers();
	}
}

#endif // UE_AUDIO_THREAD_AS_PIPE