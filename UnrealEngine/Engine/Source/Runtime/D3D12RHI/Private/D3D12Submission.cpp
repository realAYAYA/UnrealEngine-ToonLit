// Copyright Epic Games, Inc. All Rights Reserved.

#include "D3D12RHIPrivate.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "IRenderCaptureProvider.h"

#ifndef D3D12_PLATFORM_SUPPORTS_BLOCKING_FENCES
#define D3D12_PLATFORM_SUPPORTS_BLOCKING_FENCES 1
#endif

// These defines control which threads are enabled in the GPU submission pipeline.
#define D3D12_USE_SUBMISSION_THREAD (1)
#define D3D12_USE_INTERRUPT_THREAD  (1 && D3D12_PLATFORM_SUPPORTS_BLOCKING_FENCES)

// When enabled, GPU timestamp queries are adjusted to remove idle time caused by CPU bubbles.
#define D3D12_ENABLE_ADJUSTED_TIMESTAMPS 0		// Adjusted timestamps break Unreal Insights, disabling for now

static bool GD3D12AsyncPayloadMerge = true;
static FAutoConsoleVariableRef CVarD3D12AsyncPayloadMerge(
	TEXT("r.D3D12.AllowPayloadMerge"),
	GD3D12AsyncPayloadMerge,
	TEXT("Whether to attempt to merge command lists into a single payload, saving perf.  Mainly applies to QueueAsyncCommandListSubmit.  (Default = 1)\n"),
	ECVF_RenderThreadSafe
);

// @todo mgpu - fix crashes when submission thread is enabled)
static TAutoConsoleVariable<int32> CVarRHIUseSubmissionThread(
	TEXT("rhi.UseSubmissionThread"),
	2,
	TEXT("Whether to enable the RHI submission thread.\n")
	TEXT("  0: No\n")
	TEXT("  1: Yes, but not when running with multi-gpu.\n")
	TEXT("  2: Yes, always\n"),
	ECVF_ReadOnly);

DECLARE_CYCLE_STAT(TEXT("Submit"), STAT_D3D12Submit, STATGROUP_D3D12RHI);

DECLARE_CYCLE_STAT(TEXT("GPU Total Time [All Queues]"), STAT_RHI_GPUTotalTime, STATGROUP_D3D12RHI);
DECLARE_CYCLE_STAT(TEXT("GPU Total Time [Hardware Timer]"), STAT_RHI_GPUTotalTimeHW, STATGROUP_D3D12RHI);
DECLARE_CYCLE_STAT(TEXT("GPU Total Time [Graphics]"), STAT_RHI_GPUTotalTimeGraphics, STATGROUP_D3D12RHI);
DECLARE_CYCLE_STAT(TEXT("GPU Total Time [Async Compute]"), STAT_RHI_GPUTotalTimeAsyncCompute, STATGROUP_D3D12RHI);
DECLARE_CYCLE_STAT(TEXT("GPU Total Time [Copy]"), STAT_RHI_GPUTotalTimeCopy, STATGROUP_D3D12RHI);

DECLARE_STATS_GROUP(TEXT("D3D12RHIPipeline"), STATGROUP_D3D12RHIPipeline, STATCAT_Advanced);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("GPU IA Vertices"   ), STAT_D3D12RHI_IAVertices   , STATGROUP_D3D12RHIPipeline);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("GPU IA Primitives" ), STAT_D3D12RHI_IAPrimitives , STATGROUP_D3D12RHIPipeline);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("GPU VS Invocations"), STAT_D3D12RHI_VSInvocations, STATGROUP_D3D12RHIPipeline);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("GPU GS Invocations"), STAT_D3D12RHI_GSInvocations, STATGROUP_D3D12RHIPipeline);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("GPU GS Primitives" ), STAT_D3D12RHI_GSPrimitives , STATGROUP_D3D12RHIPipeline);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("GPU C Invocations" ), STAT_D3D12RHI_CInvocations , STATGROUP_D3D12RHIPipeline);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("GPU C Primitives"  ), STAT_D3D12RHI_CPrimitives  , STATGROUP_D3D12RHIPipeline);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("GPU PS Invocations"), STAT_D3D12RHI_PSInvocations, STATGROUP_D3D12RHIPipeline);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("GPU HS Invocations"), STAT_D3D12RHI_HSInvocations, STATGROUP_D3D12RHIPipeline);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("GPU DS Invocations"), STAT_D3D12RHI_DSInvocations, STATGROUP_D3D12RHIPipeline);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("GPU CS Invocations"), STAT_D3D12RHI_CSInvocations, STATGROUP_D3D12RHIPipeline);

static float GD3D12SubmissionTimeout = 5.0;
static FAutoConsoleVariableRef CVarD3D12SubmissionTimeout(
	TEXT("r.D3D12.SubmissionTimeout"),
	GD3D12SubmissionTimeout,
	TEXT("The maximum time, in seconds, that a submitted GPU command list is allowed to take before the RHI reports a GPU hang"),
	ECVF_RenderThreadSafe);

class FD3D12Thread final : private FRunnable
{
public:
	typedef FD3D12DynamicRHI::FProcessResult(FD3D12DynamicRHI::*FQueueFunc)();

	FD3D12Thread(TCHAR const* Name, EThreadPriority Priority, FD3D12DynamicRHI* RHI, FQueueFunc QueueFunc)
		: RHI(RHI)
		, QueueFunc(QueueFunc)
		, Event(CreateEvent(nullptr, false, false, nullptr))
		, Thread(FRunnableThread::Create(this, Name, 0, Priority))
	{}

	virtual ~FD3D12Thread()
	{
		bExit = true;
		SetEvent(Event);

		Thread->WaitForCompletion();
		delete Thread;

		CloseHandle(Event);
	}

	void Kick()
	{
		SetEvent(Event);
	}

private:
	virtual uint32 Run() override
	{
		while (!bExit)
		{
			// Process the queue until no more progress is made
			FD3D12DynamicRHI::FProcessResult Result;
			do { Result = (RHI->*QueueFunc)(); }
			while (EnumHasAllFlags(Result.Status, FD3D12DynamicRHI::EQueueStatus::Processed));

			WaitForSingleObject(Event, Result.WaitTimeout);
		}

		// Drain any remaining work in the queue
		while (EnumHasAllFlags((RHI->*QueueFunc)().Status, FD3D12DynamicRHI::EQueueStatus::Pending)) {}

		return 0;
	}

	FD3D12DynamicRHI* RHI;
	FQueueFunc QueueFunc;
	bool bExit = false;

public:
	// Can't use FEvent here since we need to be able to get the underlying HANDLE
	// for the ID3D12Fences to signal via ID3D12Fence::SetEventOnCompletion().
	HANDLE const Event;

private:
	FRunnableThread* Thread = nullptr;
};

void FD3D12DynamicRHI::InitializeSubmissionPipe()
{
	if (FPlatformProcess::SupportsMultithreading())
	{
#if D3D12_USE_INTERRUPT_THREAD
		InterruptThread = new FD3D12Thread(TEXT("RHIInterruptThread"), TPri_Highest, this, &FD3D12DynamicRHI::ProcessInterruptQueue);
#endif

#if D3D12_USE_SUBMISSION_THREAD
		bool bUseSubmissionThread = false;
		switch (CVarRHIUseSubmissionThread.GetValueOnAnyThread())
		{
		case 1: bUseSubmissionThread = FRHIGPUMask::All().HasSingleIndex(); break;
		case 2: bUseSubmissionThread = true; break;
		}

		// Currently RenderDoc can't make programmatic captures when we use a submission thread.
		bUseSubmissionThread &= !IRenderCaptureProvider::IsAvailable() || IRenderCaptureProvider::Get().CanSupportSubmissionThread();

		if (bUseSubmissionThread)
		{
			SubmissionThread = new FD3D12Thread(TEXT("RHISubmissionThread"), TPri_Highest, this, &FD3D12DynamicRHI::ProcessSubmissionQueue);
		}
#endif
	}

	FlushTiming(true);
}

void FD3D12DynamicRHI::ShutdownSubmissionPipe()
{
	FlushTiming(false);

	delete SubmissionThread;
	SubmissionThread = nullptr;

	delete InterruptThread;
	InterruptThread = nullptr;

	if (EopTask)
	{
		ProcessInterruptQueueUntil(EopTask);
		EopTask = nullptr;
	}
}

// A finalized set of command payloads. This type is used to implement the RHI command list submission API.
struct FD3D12FinalizedCommands : public IRHIPlatformCommandList, public TArray<FD3D12Payload*>
{};

IRHIPlatformCommandList* FD3D12DynamicRHI::RHIFinalizeContext(IRHIComputeContext* Context)
{
	FD3D12FinalizedCommands Result;
	auto FinalizeContext = [&](FD3D12CommandContext* CmdContext)
	{
		CmdContext->Finalize(Result);

		if (!CmdContext->IsDefaultContext())
		{
			CmdContext->ClearState();
			CmdContext->GetParentDevice()->ReleaseContext(CmdContext);
		}
	};

	FD3D12CommandContextBase* CmdContextBase = static_cast<FD3D12CommandContextBase*>(Context);
	if (FD3D12CommandContextRedirector* Redirector = CmdContextBase->AsRedirector())
	{
		for (uint32 GPUIndex : Redirector->GetPhysicalGPUMask())
			FinalizeContext(Redirector->GetContext(GPUIndex));

		if (!Redirector->bIsDefaultContext)
		{
			delete Redirector;
		}
	}
	else
	{
		FD3D12CommandContext* CmdContext = static_cast<FD3D12CommandContext*>(CmdContextBase);
		FinalizeContext(CmdContext);
	}

	if (Result.Num())
	{
		return new FD3D12FinalizedCommands(MoveTemp(Result));
	}
	else
	{
		return nullptr;
	}
}

void FD3D12DynamicRHI::RHISubmitCommandLists(TArrayView<IRHIPlatformCommandList*> CommandLists, bool bFlushResources)
{
	SubmitCommands(MakeArrayView(reinterpret_cast<FD3D12FinalizedCommands**>(CommandLists.GetData()), CommandLists.Num()));

	if (bFlushResources)
	{
		ProcessDeferredDeletionQueue();
	}
}

void FD3D12DynamicRHI::SubmitCommands(TConstArrayView<FD3D12FinalizedCommands*> Commands)
{
	SCOPED_NAMED_EVENT_TEXT("CommandList_Submit", FColor::Magenta);

	if ((Commands.Num() > 1) && GD3D12AsyncPayloadMerge)
	{
		// See if we can merge the payloads.  All need to be on the direct queue, have no waits, no fence signals
		// (SyncPointsToSignal are OK), and not involve events or timing.  Trivial cases where the payload
		// contents can't be dependent on one another.  Intended to optimize groups of command lists generated by
		// QueueAsyncCommandListSubmit.
		bool bCanMergePayloads = true;
		for (IRHIPlatformCommandList* Ptr : Commands)
		{
			FD3D12FinalizedCommands* Payloads = static_cast<FD3D12FinalizedCommands*>(Ptr);
			for (FD3D12Payload* Payload : *Payloads)
			{
				if (Payload->Queue.QueueType != ED3D12QueueType::Direct ||
					Payload->SyncPointsToWait.Num() ||
					Payload->FencesToWait.Num() ||
					Payload->FencesToSignal.Num() ||
					Payload->Timing.IsSet() ||
					Payload->CompletionFenceValue ||
					Payload->SubmissionEvent ||
					Payload->SubmissionTime.IsSet() ||
					Payload->TimestampQueries.Num() ||
					Payload->OcclusionQueries.Num() ||
					Payload->PipelineStatsQueries.Num())
				{
					bCanMergePayloads = false;
					break;
				}
			}
		}

		if (bCanMergePayloads)
		{
			TArray<FD3D12Payload*> MergedPayloadPerGPU;
			MergedPayloadPerGPU.AddZeroed(GNumExplicitGPUsForRendering);

			for (IRHIPlatformCommandList* Ptr : Commands)
			{
				FD3D12FinalizedCommands* Payloads = static_cast<FD3D12FinalizedCommands*>(Ptr);
				for (FD3D12Payload* Payload : *Payloads)
				{
					uint32 GPUIndex = Payload->Queue.Device->GetGPUIndex();
					if (!MergedPayloadPerGPU[GPUIndex])
					{
						MergedPayloadPerGPU[GPUIndex] = new FD3D12Payload(Payload->Queue.Device, ED3D12QueueType::Direct);
					}
					FD3D12Payload* MergedPayload = MergedPayloadPerGPU[GPUIndex];

					MergedPayload->ReservedResourcesToCommit.Append(Payload->ReservedResourcesToCommit);
					MergedPayload->CommandListsToExecute.Append(Payload->CommandListsToExecute);
					MergedPayload->SyncPointsToSignal.Append(Payload->SyncPointsToSignal);
					MergedPayload->AllocatorsToRelease.Append(Payload->AllocatorsToRelease);
					MergedPayload->QueryRanges.Append(Payload->QueryRanges);
					MergedPayload->BreadcrumbStacks.Append(Payload->BreadcrumbStacks);
					Payload->BreadcrumbStacks.Empty();

					// Need to clear out allocator array, so Payload destructor doesn't delete them
					Payload->AllocatorsToRelease.Empty();
					delete Payload;
				}
				delete Payloads;
			}

			// Remove any NULL elements from the per-GPU array
			for (int32 PayloadIndex = 0; PayloadIndex < MergedPayloadPerGPU.Num();)
			{
				if (!MergedPayloadPerGPU[PayloadIndex])
				{
					// Removing current element, don't increment index
					MergedPayloadPerGPU.RemoveAt(PayloadIndex);
				}
				else
				{
					// Otherwise, increment index
					PayloadIndex++;
				}
			}

			SubmitPayloads(MergedPayloadPerGPU);

			return;
		}
	}

	for (IRHIPlatformCommandList* Ptr : Commands)
	{
		FD3D12FinalizedCommands* Payloads = static_cast<FD3D12FinalizedCommands*>(Ptr);
		SubmitPayloads(static_cast<TArray<FD3D12Payload*>&>(*Payloads));
		delete Payloads;
	}
}

void FD3D12DynamicRHI::SubmitPayloads(TArrayView<FD3D12Payload*> Payloads)
{
	// Push all payloads into the ordered per-device, per-pipe pending queues
	for (FD3D12Payload* Payload : Payloads)
	{
		Payload->Queue.PendingSubmission.Enqueue(Payload);
	}

	if (SubmissionThread)
	{
		SubmissionThread->Kick();
	}
	else
	{
		// Since we're processing directly on the calling thread, we need to take a scope lock.
		// Multiple engine threads might be calling Submit().
		{
			FScopeLock Lock(&SubmissionCS);

			// Process the submission queue until no further progress is being made.
			while (EnumHasAnyFlags(ProcessSubmissionQueue().Status, EQueueStatus::Processed)) {}
		}
	}

	// Use this opportunity to pump the interrupt queue
	ProcessInterruptQueueUntil(nullptr);
}

static int32 GetMaxExecuteBatchSize()
{
	return
#if UE_BUILD_DEBUG
		GRHIGlobals.IsDebugLayerEnabled ? 1 :
#endif
		TNumericLimits<int32>::Max();
}

FD3D12DynamicRHI::FProcessResult FD3D12DynamicRHI::ProcessSubmissionQueue()
{
	SCOPED_NAMED_EVENT_TEXT("SubmissionQueue_Process", FColor::Turquoise);
	SCOPE_CYCLE_COUNTER(STAT_D3D12Submit);
	LLM_SCOPE_BYNAME(TEXT("RHIMisc/ProcessSubmissionQueue"));
	FProcessResult Result;

	//
	// Fence values for FD3D12SyncPoint are determined on the submission thread,
	// where each queue has a monotonically incrementing fence value.
	//
	// We might receive work that waits on a sync point which has not yet been submitted
	// to the queue that will signal it, so we need to delay processing of those
	// payloads until the fence value is known.
	//

	// Process all queues (across all devices and adapters) to flush work.
	// Any sync point waits where the fence value is unknown will be left in the 
	// appropriate queue, to be processed the next time commands are submitted.
	TArray<FD3D12Payload*, TInlineAllocator<64>> PayloadsToHandDown;
	ForEachQueue([&](FD3D12Queue& CurrentQueue)
	{
		while (true)
		{
			TArray<FD3D12Queue*, TInlineAllocator<GD3D12MaxNumQueues>> QueuesWithPayloads;
			{
				FD3D12Payload* Payload = CurrentQueue.PendingSubmission.Peek();
				if (!Payload)
					break;

				// Accumulate the list of fences to await, and their maximum values
				while (Payload->SyncPointsToWait.Index < Payload->SyncPointsToWait.Num())
				{
					FD3D12SyncPointRef& SyncPoint = Payload->SyncPointsToWait[Payload->SyncPointsToWait.Index];
					if (!SyncPoint->ResolvedFence.IsSet())
					{
						// Need to wait on a sync point, but the fence value has not been resolved yet
						// (no other payloads have signaled the sync point yet).
						
						// Skip processing this queue, and move on to the next. We will retry later when
						// further work is submitted, which may contain the sync point we need.
						Result.Status |= EQueueStatus::Pending;
						return;
					}

					CurrentQueue.EnqueueFenceWait(
						SyncPoint->ResolvedFence->Fence,
						SyncPoint->ResolvedFence->Value
					);

					Payload->SyncPointsToWait.Index++;
				}
				
				// All necessary sync points have been resolved.
				Payload->SyncPointsToWait = {};
				CurrentQueue.PendingSubmission.Pop();

				check(!CurrentQueue.PayloadToSubmit);
				CurrentQueue.PayloadToSubmit = Payload;
				QueuesWithPayloads.Add(&CurrentQueue);
				Result.Status |= EQueueStatus::Processed;

				//
				// Now we generate any required barrier command lists. These may require
				// executing on a different queue (e.g. graphics-only transitions required 
				// before async compute work), so we gather potential work across all
				// queues for this device.
				//
				const uint32 MaxBatchSize = GetMaxExecuteBatchSize();
				auto AccumulateQueries = [&](FD3D12CommandList* CommandList)
				{
					FD3D12Queue& TargetQueue = CommandList->Device->GetQueue(CommandList->QueueType);

					// Occlusion + Pipeline Stats Queries
					TargetQueue.PendingOcclusionQueries.Append(MoveTemp(CommandList->State.OcclusionQueries));
					TargetQueue.PendingPipelineStatsQueries.Append(MoveTemp(CommandList->State.PipelineStatsQueries));

					// Timestamp Queries
					// Keep only the first Begin() in the batch
					if (TargetQueue.NumCommandListsInBatch++ == 0)
					{
						TargetQueue.PendingTimestampQueries.Emplace(MoveTemp(CommandList->State.BeginTimestamp));
					}
					else
					{
						// Remove the previous End() timestamp, to join the range together.
						check(TargetQueue.PendingTimestampQueries.Last().Type == ED3D12QueryType::CommandListEnd);
						TargetQueue.PendingTimestampQueries.RemoveAt(TargetQueue.PendingTimestampQueries.Num() - 1);
					}

					TargetQueue.PendingTimestampQueries.Append(MoveTemp(CommandList->State.TimestampQueries));
					TargetQueue.PendingTimestampQueries.Emplace(MoveTemp(CommandList->State.EndTimestamp));

					if (TargetQueue.NumCommandListsInBatch >= MaxBatchSize)
					{
						// Start a new batch
						TargetQueue.NumCommandListsInBatch = 0;
					}
				};

				for (int32 Index = 0; Index < Payload->CommandListsToExecute.Num(); Index++)
				{
					FD3D12CommandList* CurrentCommandList = Payload->CommandListsToExecute[Index];
					if (FD3D12CommandList* BarrierCommandList = GenerateBarrierCommandListAndUpdateState(CurrentCommandList))
					{
						FD3D12Queue& BarrierQueue = BarrierCommandList->Device->GetQueue(BarrierCommandList->QueueType);

						if (&BarrierQueue == &CurrentQueue)
						{
							// Barrier command list will run on the current queue.
							// Insert it immediately before the corresponding command list that generated it.
							check(BarrierQueue.PayloadToSubmit);
							BarrierQueue.PayloadToSubmit->CommandListsToExecute.Insert(BarrierCommandList, Index++);
						}
						else
						{
							// Barrier command list will run on a different queue.
							if (!BarrierQueue.PayloadToSubmit)
							{
								BarrierQueue.PayloadToSubmit = new FD3D12Payload(BarrierCommandList->Device, BarrierCommandList->QueueType);
								QueuesWithPayloads.Add(&BarrierQueue);
							}
								
							BarrierQueue.PayloadToSubmit->CommandListsToExecute.Add(BarrierCommandList);
						}

						// Append the barrier cmdlist begin/end timestamps to the other queue.
						AccumulateQueries(BarrierCommandList);
					}

					AccumulateQueries(CurrentCommandList);
				}

				CurrentQueue.PendingQueryRanges.Append(MoveTemp(Payload->QueryRanges));
			}
				
			// Prepare the command lists from each payload for submission
			for (FD3D12Queue* Queue : QueuesWithPayloads)
			{
				FD3D12Payload* Payload = Queue->PayloadToSubmit;
				check(Payload->SyncPointsToWait.Num() == 0);

				Queue->BarrierTimestamps.CloseAndReset(Queue->PendingQueryRanges);
				Queue->NumCommandListsInBatch = 0;

				check(Payload->QueryRanges.Num() == 0);
				check(Payload->TimestampQueries.Num() == 0);
				check(Payload->OcclusionQueries.Num() == 0);
				check(Payload->PipelineStatsQueries.Num() == 0);

				if (Queue->PendingQueryRanges.Num())
				{
					// If this payload will signal a CPU-visible sync point, we need to resolve queries.
					// This makes sure that the query data has reached the CPU before the sync point the CPU is waiting on is signaled.
					bool bResolveQueries = false;
					for (FD3D12SyncPoint* SyncPoint : Payload->SyncPointsToSignal)
					{
						if (SyncPoint->GetType() == ED3D12SyncPointType::GPUAndCPU)
						{
							bResolveQueries = true;
							break;
						}
					}

					if (bResolveQueries)
					{
						FD3D12CommandList* ResolveCommandList = nullptr;
						{
							auto GetResolveCommandList = [&]() -> FD3D12CommandList*
							{
								if (ResolveCommandList)
									return ResolveCommandList;

								if (!Queue->BarrierAllocator)
									Queue->BarrierAllocator = Queue->Device->ObtainCommandAllocator(Queue->QueueType); 

								return ResolveCommandList = Queue->Device->ObtainCommandList(Queue->BarrierAllocator, nullptr, nullptr);
							};

							// We've got queries to resolve. Allocate a command list.
							for (FD3D12QueryRange const& Range : Queue->PendingQueryRanges)
							{
								check(Range.End > Range.Start);

	#if ENABLE_RESIDENCY_MANAGEMENT
								TArray<FD3D12ResidencyHandle*, TInlineAllocator<2>> ResidencyHandles;
								ResidencyHandles.Add(&Range.Heap->GetHeapResidencyHandle());
								ResidencyHandles.Append(Range.Heap->GetResultBuffer()->GetResidencyHandles());
								GetResolveCommandList()->UpdateResidency(ResidencyHandles);
	#endif // ENABLE_RESIDENCY_MANAGEMENT

								if (Range.Heap->GetD3DQueryHeap())
								{
									GetResolveCommandList()->GraphicsCommandList()->ResolveQueryData(
										Range.Heap->GetD3DQueryHeap(),
										Range.Heap->QueryType,
										Range.Start,
										Range.End - Range.Start,
										Range.Heap->GetResultBuffer()->GetResource(),
										Range.Start * Range.Heap->GetResultSize()
									);
								}
							}
						}

						Payload->QueryRanges          = MoveTemp(Queue->PendingQueryRanges         );
						Payload->TimestampQueries     = MoveTemp(Queue->PendingTimestampQueries    );
						Payload->OcclusionQueries     = MoveTemp(Queue->PendingOcclusionQueries    );
						Payload->PipelineStatsQueries = MoveTemp(Queue->PendingPipelineStatsQueries);

						if (ResolveCommandList)
						{
							ResolveCommandList->Close();
							Payload->CommandListsToExecute.Add(ResolveCommandList);
						}
					}
				}

				if (Queue->BarrierAllocator)
				{
					Payload->AllocatorsToRelease.Add(Queue->BarrierAllocator);
					Queue->BarrierAllocator = nullptr;
				}

				// Hand payload down to the interrupt thread.
				PayloadsToHandDown.Add(Payload);
			}

			// Queues with work to submit other than the current one (CurrentQueue) are performing barrier operations.
			// Submit this work first, followed by a fence signal + enqueued wait.
			for (FD3D12Queue* OtherQueue : QueuesWithPayloads)
			{
				if (OtherQueue != &CurrentQueue)
				{
					uint64 ValueSignaled = OtherQueue->ExecutePayload();
					CurrentQueue.EnqueueFenceWait(&OtherQueue->Fence, ValueSignaled);
				}
			}

			// Wait on the previous sync point and barrier command list fences.
			CurrentQueue.FlushFenceWaits();

			// Execute the command lists + signal for completion
			CurrentQueue.ExecutePayload();
		}
	});

	for (FD3D12Payload* Payload : PayloadsToHandDown)
	{
		Payload->Queue.PendingInterrupt.Enqueue(Payload);
	}

	if (InterruptThread && EnumHasAnyFlags(Result.Status, EQueueStatus::Processed))
	{
		InterruptThread->Kick();
	}

	return Result;
}

FD3D12CommandList* FD3D12DynamicRHI::GenerateBarrierCommandListAndUpdateState(FD3D12CommandList* SourceCommandList)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GenerateBarrierCommandListAndUpdateState);

	FD3D12ResourceBarrierBatcher Batcher;

#if ENABLE_RESIDENCY_MANAGEMENT
	TArray<FD3D12ResidencyHandle*> ResidencyHandles;
	ResidencyHandles.Reserve(SourceCommandList->State.PendingResourceBarriers.Num());
#endif

	bool bHasGraphicStates = false;
	for (const FD3D12PendingResourceBarrier& PRB : SourceCommandList->State.PendingResourceBarriers)
	{
		// Should only be doing this for the few resources that need state tracking
		check(PRB.Resource->RequiresResourceStateTracking());

		CResourceState& ResourceState = PRB.Resource->GetResourceState_OnResource();

		const D3D12_RESOURCE_STATES Before = ResourceState.GetSubresourceState(PRB.SubResource);
		const D3D12_RESOURCE_STATES After = PRB.State;

		// We shouldn't have any TBD / CORRUPT states here
		check(Before != D3D12_RESOURCE_STATE_TBD && Before != D3D12_RESOURCE_STATE_CORRUPT);
		check(After  != D3D12_RESOURCE_STATE_TBD && After  != D3D12_RESOURCE_STATE_CORRUPT);
		
		if (Before != After)
		{
			if (SourceCommandList->QueueType != ED3D12QueueType::Direct)
			{
				check(!IsDirectQueueExclusiveD3D12State(After));
				if (IsDirectQueueExclusiveD3D12State(Before))
				{
					// If we are transitioning to a subset of the already set state on the resource (SRV_MASK -> SRV_COMPUTE for example)
					// and there are no other transitions done on this resource in the command list then this transition can be ignored
					// (otherwise a transition from SRV_COMPUTE as before state might already be recorded in the command list)
					CResourceState& ResourceState_OnCommandList = SourceCommandList->GetResourceState_OnCommandList(PRB.Resource);
					if (ResourceState_OnCommandList.HasInternalTransition() || !EnumHasAllFlags(Before, After))
					{
						bHasGraphicStates = true;
					}
					else
					{
						// should be the same final state as well
						check(After == ResourceState_OnCommandList.GetSubresourceState(PRB.SubResource));
						
						// Force the original state again if we're skipping this transition
						ResourceState_OnCommandList.SetSubresourceState(PRB.SubResource, Before);
						continue;
					}
				}
			}

			if (PRB.Resource->IsBackBuffer() && EnumHasAnyFlags(After, BackBufferBarrierWriteTransitionTargets))
			{
				Batcher.AddTransition(PRB.Resource, Before, After, PRB.SubResource);
			}
			// Special case for UAV access resources transitioning from UAV (then they need to transition from the cache hidden state instead)
			else if (PRB.Resource->GetUAVAccessResource() && EnumHasAnyFlags(Before | After, D3D12_RESOURCE_STATE_UNORDERED_ACCESS))
			{
				// After state should never be UAV here
				check(!EnumHasAnyFlags(After, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
					
				// Add the aliasing barrier
				Batcher.AddAliasingBarrier(PRB.Resource->GetUAVAccessResource(), PRB.Resource->GetResource());

				D3D12_RESOURCE_STATES UAVState = ResourceState.GetUAVHiddenResourceState();
				check(UAVState != D3D12_RESOURCE_STATE_TBD && !EnumHasAnyFlags(UAVState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
				if (UAVState != After)
				{
					Batcher.AddTransition(PRB.Resource, UAVState, After, PRB.SubResource);
				}
			}
			else
			{
				Batcher.AddTransition(PRB.Resource, Before, After, PRB.SubResource);
			}
		}

#if ENABLE_RESIDENCY_MANAGEMENT
		for (FD3D12ResidencyHandle* Handle : PRB.Resource->GetResidencyHandles())
		{
			ResidencyHandles.Add(Handle);
		}
#endif // ENABLE_RESIDENCY_MANAGEMENT
	}

	// Update the tracked resource states with the final states from the command list
	for (auto const& [Resource, CommandListState] : SourceCommandList->State.TrackedResourceState)
	{
		CResourceState& ResourceState = Resource->GetResourceState_OnResource();
		if (CommandListState.AreAllSubresourcesSame())
		{
			D3D12_RESOURCE_STATES State = CommandListState.GetSubresourceState(0);
			if (State != D3D12_RESOURCE_STATE_TBD)
			{
				ResourceState.SetResourceState(State);
			}

			D3D12_RESOURCE_STATES UAVHiddenState = CommandListState.GetUAVHiddenResourceState();
			if (UAVHiddenState != D3D12_RESOURCE_STATE_TBD)
			{
				check(Resource->GetUAVAccessResource());
				ResourceState.SetUAVHiddenResourceState(UAVHiddenState);
			}
		}
		else
		{
			for (uint32 Subresource = 0; Subresource < Resource->GetSubresourceCount(); ++Subresource)
			{
				D3D12_RESOURCE_STATES State = CommandListState.GetSubresourceState(Subresource);
				if (State != D3D12_RESOURCE_STATE_TBD)
				{
					ResourceState.SetSubresourceState(Subresource, State);
				}
			}
		}
	}

	if (Batcher.Num() == 0)
		return nullptr;

	// This command list requires a separate barrier command list to fix up tracked resource states.
	TRACE_CPUPROFILER_EVENT_SCOPE(GetResourceBarrierCommandList);

	FD3D12Queue& Queue = SourceCommandList->Device->GetQueue(
		bHasGraphicStates
			? ED3D12QueueType::Direct
			: SourceCommandList->QueueType
	);

	// Get an allocator if we don't have one
	if (!Queue.BarrierAllocator)
		Queue.BarrierAllocator = Queue.Device->ObtainCommandAllocator(Queue.QueueType);

	// Get a new command list
	FD3D12CommandList* BarrierCommandList = Queue.Device->ObtainCommandList(Queue.BarrierAllocator, &Queue.BarrierTimestamps, nullptr);

#if ENABLE_RESIDENCY_MANAGEMENT
	BarrierCommandList->UpdateResidency(ResidencyHandles);
#endif

	Batcher.FlushIntoCommandList(*BarrierCommandList, Queue.BarrierTimestamps);
	BarrierCommandList->Close();

	return BarrierCommandList;
}

uint64 FD3D12Queue::ExecutePayload()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ExecuteCommandList);
	LLM_SCOPE_BYNAME(TEXT("RHIMisc/ExecuteCommandLists"));
	check(PayloadToSubmit && this == &PayloadToSubmit->Queue);

	// Wait for manual fences.
	for (auto& [ManualFence, Value] : PayloadToSubmit->FencesToWait)
	{
		VERIFYD3D12RESULT(D3DCommandQueue->Wait(ManualFence, Value));
	}

	PayloadToSubmit->PreExecute();

	if (!PayloadToSubmit->ReservedResourcesToCommit.IsEmpty())
	{
		// On some devices, some queues cannot perform tile remapping operations.
		// We can work around this limitation by running the remapping in lockstep on another queue:
		// - tile mapping queue waits for commands on this queue to finish
		// - tile mapping queue performs the commit/decommit operations
		// - this queue waits for tile mapping queue to finish
		// The extra sync is not required when the current queue is capable of the remapping operations.

		ID3D12CommandQueue* TileMappingQueue = (bSupportsTileMapping ? D3DCommandQueue : Device->TileMappingQueue).GetReference();
		FD3D12Fence& TileMappingFence = Device->TileMappingFence;

		const bool bCrossQueueSyncRequired = TileMappingQueue != D3DCommandQueue.GetReference();

		if (bCrossQueueSyncRequired)
		{
			// tile mapping queue waits for commands on this queue to finish
			D3DCommandQueue->Signal(TileMappingFence.D3DFence, ++TileMappingFence.LastSignaledValue);
			TileMappingQueue->Wait(TileMappingFence.D3DFence, TileMappingFence.LastSignaledValue);
		}

		for (const FD3D12CommitReservedResourceDesc& CommitDesc : PayloadToSubmit->ReservedResourcesToCommit)
		{
			CommitDesc.Resource->CommitReservedResource(TileMappingQueue, CommitDesc.CommitSizeInBytes);
		}

		if (bCrossQueueSyncRequired)
		{
			// this queue waits for tile mapping operations to finish
			TileMappingQueue->Signal(TileMappingFence.D3DFence, ++TileMappingFence.LastSignaledValue);
			D3DCommandQueue->Wait(TileMappingFence.D3DFence, TileMappingFence.LastSignaledValue);
		}
	}

	if (const int32 NumCommandLists = PayloadToSubmit->CommandListsToExecute.Num())
	{
		// Build SOA layout needed to call ExecuteCommandLists().
		TArray<ID3D12CommandList*> D3DCommandLists;
		TArray<FD3D12ResidencySet*> ResidencySets;

		D3DCommandLists.Reserve(NumCommandLists);
		ResidencySets.Reserve(NumCommandLists);

		for (FD3D12CommandList* CommandList : PayloadToSubmit->CommandListsToExecute)
		{
			check(CommandList->IsClosed());
			D3DCommandLists.Add(CommandList->Interfaces.CommandList);
			ResidencySets.Add(CommandList->ResidencySet);
		}

		const int32 MaxBatchSize = GetMaxExecuteBatchSize();

		for (int32 DispatchNum, Offset = 0; Offset < NumCommandLists; Offset += DispatchNum)
		{
			DispatchNum = FMath::Min(NumCommandLists - Offset, MaxBatchSize);

			extern int32 GD3D12MaxCommandsPerCommandList;
			if (GD3D12MaxCommandsPerCommandList > 0)
			{
				// Limit the dispatch group based on the total number of commands each command list contains, so that we
				// don't submit more than approx "GD3D12MaxCommandsPerCommandList" commands per call to ExecuteCommandLists().
				int32 Index = 0;
				for (int32 NumCommands = 0; Index < DispatchNum && NumCommands < GD3D12MaxCommandsPerCommandList; ++Index)
				{
					NumCommands += PayloadToSubmit->CommandListsToExecute[Offset + Index]->State.NumCommands;
				}

				DispatchNum = Index;
			}

#if ENABLE_RESIDENCY_MANAGEMENT
			if (GEnableResidencyManagement)
			{
				VERIFYD3D12RESULT(Device->GetResidencyManager().ExecuteCommandLists(
					D3DCommandQueue,
					&D3DCommandLists[Offset],
					&ResidencySets[Offset],
					DispatchNum
				));
			}
			else
#endif
			{
				D3DCommandQueue->ExecuteCommandLists(
					DispatchNum,
					&D3DCommandLists[Offset]
				);
			}

#if LOG_EXECUTE_COMMAND_LISTS
			LogExecuteCommandLists(DispatchNum, &D3DCommandLists[Offset]);
#endif
		}

		// Release the FD3D12CommandList instance back to the parent device object pool.
		for (FD3D12CommandList* CommandList : PayloadToSubmit->CommandListsToExecute)
			CommandList->Device->ReleaseCommandList(CommandList);

		PayloadToSubmit->CommandListsToExecute.Reset();

		// We've executed command lists on the queue, so any 
		// future sync points need a new signaled fence value.
		bRequiresSignal = true;
	}

	bRequiresSignal |= PayloadToSubmit->bAlwaysSignal;

	// Keep the latest fence value in the submitted payload.
	// The interrupt thread uses this to determine when work has completed.
	uint64 FenceValue = SignalFence();
	PayloadToSubmit->CompletionFenceValue = FenceValue;

	// Signal any manual fences
	for (auto& [ManualFence, Value] : PayloadToSubmit->FencesToSignal)
	{
		VERIFYD3D12RESULT(D3DCommandQueue->Signal(ManualFence, Value));
	}

	// Set the fence/value pair into any sync points we need to signal.
	for (FD3D12SyncPointRef& SyncPoint : PayloadToSubmit->SyncPointsToSignal)
	{
		check(!SyncPoint->ResolvedFence.IsSet());
		SyncPoint->ResolvedFence.Emplace(&Fence, PayloadToSubmit->CompletionFenceValue);
	}

	// Submission of this payload is completed. Signal the event if one was provided.
	if (PayloadToSubmit->SubmissionEvent)
	{
		PayloadToSubmit->SubmissionEvent->DispatchSubsequents();
	}

	// Used for GPU timeout detection
	PayloadToSubmit->SubmissionTime = FPlatformTime::Cycles64();

	PayloadToSubmit = nullptr;

	return FenceValue;
}

void FD3D12SyncPoint::Wait() const
{
	checkf(GraphEvent, TEXT("This sync point was not created with a CPU event. Cannot wait for completion on the CPU."));

	if (!GraphEvent->IsComplete())
	{
		// Block the calling thread until the graph event is signaled by the interrupt thread.
		SCOPED_NAMED_EVENT_TEXT("SyncPoint_Wait", FColor::Turquoise);
		FD3D12DynamicRHI::GetD3DRHI()->ProcessInterruptQueueUntil(GraphEvent);
	}

	check(GraphEvent->IsComplete());
}

void FD3D12DynamicRHI::ProcessInterruptQueueUntil(FGraphEvent* GraphEvent)
{
	if (InterruptThread)
	{
		if (GraphEvent && !GraphEvent->IsComplete())
		{
			GraphEvent->Wait();
		}
	}
	else
	{
		// Use the current thread to process the interrupt queue until the sync point we're waiting for is signaled.
		// If GraphEvent is nullptr, process the queue until no further progress is made (assuming we can acquire the lock), then return.
		if (!GraphEvent || !GraphEvent->IsComplete())
		{
			// If we're waiting for a sync point, accumulate the idle time
			FThreadIdleStats::FScopeIdle IdleScope(/* bIgnore = */GraphEvent == nullptr);

		Retry:
			if (InterruptCS.TryLock())
			{
				FProcessResult Result;
				do { Result = ProcessInterruptQueue(); }
				// If we have a sync point, keep processing until the sync point is signaled.
				// Otherwise, process until no more progress is being made.
				while (GraphEvent
					? !GraphEvent->IsComplete()
					: EnumHasAllFlags(Result.Status, EQueueStatus::Processed)
				);

				InterruptCS.Unlock();
			}
			else if (GraphEvent && !GraphEvent->IsComplete())
			{
				// Failed to get the lock. Another thread is processing the interrupt queue. Try again...
				FPlatformProcess::SleepNoStats(0);
				goto Retry;
			}
		}
	}
}

D3D12_QUERY_DATA_PIPELINE_STATISTICS& operator += (D3D12_QUERY_DATA_PIPELINE_STATISTICS& LHS, D3D12_QUERY_DATA_PIPELINE_STATISTICS const& RHS)
{
	LHS.IAVertices	  += RHS.IAVertices;
	LHS.IAPrimitives  += RHS.IAPrimitives;
	LHS.VSInvocations += RHS.VSInvocations;
	LHS.GSInvocations += RHS.GSInvocations;
	LHS.GSPrimitives  += RHS.GSPrimitives;
	LHS.CInvocations  += RHS.CInvocations;
	LHS.CPrimitives	  += RHS.CPrimitives;
	LHS.PSInvocations += RHS.PSInvocations;
	LHS.HSInvocations += RHS.HSInvocations;
	LHS.DSInvocations += RHS.DSInvocations;
	LHS.CSInvocations += RHS.CSInvocations;
	return LHS;
}

FD3D12DynamicRHI::FProcessResult FD3D12DynamicRHI::ProcessInterruptQueue()
{
	SCOPED_NAMED_EVENT_TEXT("InterruptQueue_Process", FColor::Yellow);
	LLM_SCOPE_BYNAME(TEXT("RHIMisc/ProcessInterruptQueue"));

	FProcessResult Result;
	ForEachQueue([&](FD3D12Queue& CurrentQueue)
	{
		while (FD3D12Payload* Payload = CurrentQueue.PendingInterrupt.Peek())
		{
			// Check for GPU completion
			uint64 CompletedFenceValue = CurrentQueue.Fence.D3DFence->GetCompletedValue();

			if (CompletedFenceValue == UINT64_MAX)
			{
				// If the GPU crashes or hangs, the driver will signal all fences to UINT64_MAX. If we get an error code here, we can't pass it directly to 
				// VERIFYD3D12RESULT, because that expects DXGI_ERROR_DEVICE_REMOVED, DXGI_ERROR_DEVICE_RESET etc. and wants to obtain the reason code itself
				// by calling GetDeviceRemovedReason (again).
				HRESULT DeviceRemovedReason = CurrentQueue.Device->GetDevice()->GetDeviceRemovedReason();
				if (DeviceRemovedReason != S_OK)
				{
					VerifyD3D12Result(DXGI_ERROR_DEVICE_REMOVED, "CurrentQueue.Fence.D3DFence->GetCompletedValue()", __FILE__, __LINE__, CurrentQueue.Device->GetDevice());
				}
			}

			if (CompletedFenceValue < Payload->CompletionFenceValue)
			{
				// Command list batch has not yet completed on this queue.
				// Ask the driver to wake this thread again when the required value is reached.
				if (InterruptThread && !CurrentQueue.Fence.bInterruptAwaited)
				{
					SCOPED_NAMED_EVENT_TEXT("SetEventOnCompletion", FColor::Red);
					VERIFYD3D12RESULT(CurrentQueue.Fence.D3DFence->SetEventOnCompletion(Payload->CompletionFenceValue, InterruptThread->Event));
					CurrentQueue.Fence.bInterruptAwaited = true;
				}

				// Skip processing this queue and move on to the next.
				Result.Status |= EQueueStatus::Pending;

				// Detect a hung GPU
				if (Payload->SubmissionTime.IsSet())
				{
					static const uint64 TimeoutCycles = FMath::TruncToInt64(GD3D12SubmissionTimeout / FPlatformTime::GetSecondsPerCycle64());
					static const double CyclesPerSecond = 1.0 / FPlatformTime::GetSecondsPerCycle64();

					uint64 ElapsedCycles = FPlatformTime::Cycles64() - Payload->SubmissionTime.GetValue();

					if (ElapsedCycles > TimeoutCycles)
					{
						// The last submission on this pipe did not complete within the timeout period. Assume the GPU has hung.
						HandleGpuTimeout(Payload, ElapsedCycles * FPlatformTime::GetSecondsPerCycle64());
						Payload->SubmissionTime.Reset();
					}
					else
					{
						// Adjust the event wait timeout to cause the interrupt thread to wake automatically when
						// the timeout for this payload is reached, assuming it hasn't been woken by the GPU already.
						uint64 RemainingCycles = TimeoutCycles - ElapsedCycles;
						uint32 RemainingMilliseconds = FMath::TruncToInt(RemainingCycles * FPlatformTime::GetSecondsPerCycle64() * 1000.0);
						Result.WaitTimeout = FMath::Min(Result.WaitTimeout, RemainingMilliseconds);
					}
				}

				return;
			}

			// At this point, the current command list has completed on the GPU.
			CurrentQueue.Fence.bInterruptAwaited = false;
			CurrentQueue.PendingInterrupt.Pop();
			Result.Status |= EQueueStatus::Processed;

			// Resolve query results
			{
				if (Payload->Timing.IsSet())
				{
					// Switch the new timing struct into the queue. This redirects timestamp results to separate each frame's work.
					CurrentQueue.Timing = Payload->Timing.GetValue();
				}

				for (FD3D12QueryLocation& Query : Payload->OcclusionQueries)
				{
					check(Query.Target);
					Query.CopyResultTo(Query.Target);
				}

				for (FD3D12QueryLocation& Query : Payload->PipelineStatsQueries)
				{
					if (Query.Target)
					{
						Query.CopyResultTo(Query.Target);
					}
					else
					{
						// Pipeline stats queries without targets are the ones that surround whole command lists.
						CurrentQueue.Timing->PipelineStats += Query.GetResult<D3D12_QUERY_DATA_PIPELINE_STATISTICS>();
					}
				}

				if (Payload->TimestampQueries.Num())
				{
					FD3D12QueryLocation* IdleBegin = nullptr;
					FD3D12QueryLocation* ListBegin = nullptr;

					// Some timestamp queries report in microseconds
					const double MicrosecondsScale = 1000000.0 / CurrentQueue.Device->GetTimestampFrequency(CurrentQueue.QueueType);

					for (FD3D12QueryLocation& Query : Payload->TimestampQueries)
					{
						if (Query.Target)
						{
							Query.CopyResultTo(Query.Target);
						}

						switch (Query.Type)
						{
						case ED3D12QueryType::CommandListBegin:
						case ED3D12QueryType::CommandListEnd:
						case ED3D12QueryType::IdleBegin:
						case ED3D12QueryType::IdleEnd:
							check(CurrentQueue.Timing);
							CurrentQueue.Timing->Timestamps.Add(Query.GetResult<uint64>());
							break;
						}

						switch (Query.Type)
						{
						case ED3D12QueryType::CommandListBegin:
							check(!ListBegin && !IdleBegin);
							ListBegin = &Query;
							break;

						case ED3D12QueryType::CommandListEnd:
							check(ListBegin != nullptr && IdleBegin == nullptr);
							// Accumulate the number of ticks that have elapsed between the end of the previous command list, and the start of this one.
							CurrentQueue.CumulativeIdleTicks += (CurrentQueue.LastEndTime != 0) ? ListBegin->GetResult<uint64>() - CurrentQueue.LastEndTime : 0; //-V522
							CurrentQueue.LastEndTime = Query.GetResult<uint64>();
							ListBegin = nullptr;
							break;

						case ED3D12QueryType::IdleBegin:
							check(ListBegin && !IdleBegin);
							IdleBegin = &Query;
							break;

						case ED3D12QueryType::IdleEnd:
							check(ListBegin != nullptr && IdleBegin != nullptr);
							// Accumulate the time this pipe spent in an idle scope. This includes vsync and waiting on other pipes.
							CurrentQueue.CumulativeIdleTicks += Query.GetResult<uint64>() - IdleBegin->GetResult<uint64>(); //-V522
							IdleBegin = nullptr;
							break;

						case ED3D12QueryType::AdjustedMicroseconds:
						case ED3D12QueryType::AdjustedRaw:
							check(ListBegin && !IdleBegin && Query.Target);
#if D3D12_ENABLE_ADJUSTED_TIMESTAMPS
							// Adjust the time such that the ticks reported only advance when this pipe is busy
							*Query.Target -= CurrentQueue.CumulativeIdleTicks;
#endif
							if (Query.Type == ED3D12QueryType::AdjustedMicroseconds)
							{
								// Convert to microseconds
								*static_cast<uint64*>(Query.Target) = FPlatformMath::TruncToInt(double(*static_cast<uint64*>(Query.Target)) * MicrosecondsScale);
							}
							break;
						}
					}

					check(!ListBegin && !IdleBegin);
				}
			}

			// Signal the CPU events of all sync points associated with this batch.
			for (FD3D12SyncPointRef& SyncPoint : Payload->SyncPointsToSignal)
			{
				if (SyncPoint->GraphEvent)
				{
					SyncPoint->GraphEvent->DispatchSubsequents();
				}
			}

			// We're done with this payload now.

			// GPU resources the payload is holding a reference to will be cleaned up here.
			// E.g. command list allocators, which get recycled on the parent device.
			delete Payload;
		}
	});

	return Result;
}

FD3D12PayloadBase::FD3D12PayloadBase(FD3D12Device* const Device, ED3D12QueueType const QueueType)
	: Queue(Device->GetQueue(QueueType))
{}

FD3D12PayloadBase::~FD3D12PayloadBase()
{
	for (FD3D12CommandAllocator* Allocator : AllocatorsToRelease)
	{
		Queue.Device->ReleaseCommandAllocator(Allocator);
	}
}

void FD3D12PayloadBase::PreExecute()
{
	if (PreExecuteCallback)
	{
		PreExecuteCallback(Queue.D3DCommandQueue);
	}
}

void FBreadcrumbStack::Initialize(TUniquePtr<FD3D12DiagnosticBuffer>& DiagnosticBuffer)
{
	{
		FScopeLock Lock(&DiagnosticBuffer->CriticalSection);

		if (DiagnosticBuffer->FreeContextIds.Num() == 0)
		{
			ContextId = 0;
			return;
		}

		ContextId = DiagnosticBuffer->FreeContextIds.Pop();
	}

	check(ContextId > 0);
	{
		const uint32 ContextSize = DiagnosticBuffer->BreadCrumbsContextSize;
		const uint32 Offset = DiagnosticBuffer->BreadCrumbsContextSize * (ContextId - 1);

		MaxMarkers = ContextSize / sizeof(uint32);
		WriteAddress = DiagnosticBuffer->GpuAddress + Offset;
		CPUAddress = (uint8*)(DiagnosticBuffer->CpuAddress) + Offset;

		FMemory::Memzero(CPUAddress, DiagnosticBuffer->BreadCrumbsContextSize);
	}
}

FBreadcrumbStack::FBreadcrumbStack()
{
	Scopes.Reserve(2048);
	ScopeStack.Reserve(128);
}

FBreadcrumbStack::~FBreadcrumbStack()
{
	TUniquePtr<FD3D12DiagnosticBuffer>& DiagnosticBuffer = Queue->DiagnosticBuffer;
	if (ContextId > 0 && DiagnosticBuffer.IsValid())
	{
		{
			FScopeLock Lock(&DiagnosticBuffer->CriticalSection);
			DiagnosticBuffer->FreeContextIds.Push(ContextId);
		}
		ContextId = 0;
	}
}

#ifndef D3D12_PREFER_QUERIES_FOR_GPU_TIME
#define D3D12_PREFER_QUERIES_FOR_GPU_TIME 0
#endif

static TAutoConsoleVariable<int32> CVarGPUTimeFromTimestamps(
	TEXT("r.D3D12.GPUTimeFromTimestamps"),
	D3D12_PREFER_QUERIES_FOR_GPU_TIME,
	TEXT("Prefer timestamps instead of GetHardwareGPUFrameTime to compute GPU frame time"),
	ECVF_RenderThreadSafe);

void FD3D12DynamicRHI::ProcessTimestamps(TIndirectArray<FD3D12Timing>& Timing)
{
	// The total number of cycles where at least one GPU pipe was busy during the frame.
	uint64 UnionBusyCycles = 0;
	int32 BusyPipes = 0;

	uint64 LastMinCycles = 0;
	bool bFirst = true;

	// Process the time ranges from each pipe.
	while (true)
	{
		// Find the next minimum timestamp
		FD3D12Timing* NextMin = nullptr;
		for (FD3D12Timing& Current : Timing)
		{
			if (Current.HasMoreTimestamps() && (!NextMin || Current.GetCurrentTimestamp() < NextMin->GetCurrentTimestamp()))
			{
				NextMin = &Current;
			}
		}

		if (!NextMin)
			break; // No more timestamps to process

		if (!bFirst)
		{
			if (BusyPipes > 0 && NextMin->GetCurrentTimestamp() > LastMinCycles)
			{
				// Accumulate the union busy time across all pipes
				UnionBusyCycles += NextMin->GetCurrentTimestamp() - LastMinCycles;
			}

			if (!NextMin->IsStartingWork())
			{
				// Accumulate the busy time for this pipe specifically.
				NextMin->BusyCycles += NextMin->GetCurrentTimestamp() - NextMin->GetPreviousTimestamp();
			}
		}

		LastMinCycles = NextMin->GetCurrentTimestamp();

		BusyPipes += NextMin->IsStartingWork() ? 1 : -1;
		check(BusyPipes >= 0);

		NextMin->AdvanceTimestamp();
		bFirst = false;
	}

	check(BusyPipes == 0);
	
	D3D12_QUERY_DATA_PIPELINE_STATISTICS PipelineStats{};
	for (FD3D12Timing& Current : Timing)
	{
		PipelineStats += Current.PipelineStats;
	}

	SET_DWORD_STAT(STAT_D3D12RHI_IAVertices   , PipelineStats.IAVertices   );
	SET_DWORD_STAT(STAT_D3D12RHI_IAPrimitives , PipelineStats.IAPrimitives );
	SET_DWORD_STAT(STAT_D3D12RHI_VSInvocations, PipelineStats.VSInvocations);
	SET_DWORD_STAT(STAT_D3D12RHI_GSInvocations, PipelineStats.GSInvocations);
	SET_DWORD_STAT(STAT_D3D12RHI_GSPrimitives , PipelineStats.GSPrimitives );
	SET_DWORD_STAT(STAT_D3D12RHI_CInvocations , PipelineStats.CInvocations );
	SET_DWORD_STAT(STAT_D3D12RHI_CPrimitives  , PipelineStats.CPrimitives  );
	SET_DWORD_STAT(STAT_D3D12RHI_PSInvocations, PipelineStats.PSInvocations);
	SET_DWORD_STAT(STAT_D3D12RHI_HSInvocations, PipelineStats.HSInvocations);
	SET_DWORD_STAT(STAT_D3D12RHI_DSInvocations, PipelineStats.DSInvocations);
	SET_DWORD_STAT(STAT_D3D12RHI_CSInvocations, PipelineStats.CSInvocations);

	// @todo mgpu - how to handle multiple devices / queues with potentially different timestamp frequencies?
	FD3D12Device* Device = GetAdapter().GetDevice(0);
	double Frequency = Device->GetTimestampFrequency(ED3D12QueueType::Direct);

	const double Scale32 = 1.0 / (Frequency * FPlatformTime::GetSecondsPerCycle());
	const double Scale64 = 1.0 / (Frequency * FPlatformTime::GetSecondsPerCycle64());

	// Update the global GPU frame time stats
	SET_CYCLE_COUNTER(STAT_RHI_GPUTotalTime, FPlatformMath::TruncToInt(double(UnionBusyCycles) * Scale64));
	 
	double HardwareGPUTime = 0.0;
	if (GetHardwareGPUFrameTime(HardwareGPUTime) && CVarGPUTimeFromTimestamps.GetValueOnAnyThread() == 0)
	{
		SET_CYCLE_COUNTER(STAT_RHI_GPUTotalTimeHW, HardwareGPUTime);
		GGPUFrameTime = HardwareGPUTime;
	}
	else
	{
		SET_CYCLE_COUNTER(STAT_RHI_GPUTotalTimeHW, 0);
		GGPUFrameTime = FPlatformMath::TruncToInt(double(UnionBusyCycles) * Scale32);
	}

	for (FD3D12Timing& Current : Timing)
	{
		switch (Current.Queue.QueueType)
		{
		case ED3D12QueueType::Direct: SET_CYCLE_COUNTER(STAT_RHI_GPUTotalTimeGraphics    , FPlatformMath::TruncToInt(double(Current.BusyCycles) * Scale64)); break;
		case ED3D12QueueType::Async : SET_CYCLE_COUNTER(STAT_RHI_GPUTotalTimeAsyncCompute, FPlatformMath::TruncToInt(double(Current.BusyCycles) * Scale64)); break;
		case ED3D12QueueType::Copy  : SET_CYCLE_COUNTER(STAT_RHI_GPUTotalTimeCopy        , FPlatformMath::TruncToInt(double(Current.BusyCycles) * Scale64)); break;
		}
	}
}
