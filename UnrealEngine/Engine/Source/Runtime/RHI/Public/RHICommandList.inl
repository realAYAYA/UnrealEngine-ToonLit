// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RHICommandList.inl: RHI Command List inline definitions.
=============================================================================*/

#pragma once

class FRHICommandListBase;
class FRHICommandListExecutor;
class FRHICommandListImmediate;
class FRHIResource;
class FScopedRHIThreadStaller;
struct FRHICommandBase;

FORCEINLINE_DEBUGGABLE bool FRHICommandListBase::IsImmediate() const
{
	return PersistentState.bImmediate;
}

FORCEINLINE_DEBUGGABLE FRHICommandListImmediate& FRHICommandListBase::GetAsImmediate()
{
	checkf(IsImmediate(), TEXT("This operation expects the immediate command list."));
	return static_cast<FRHICommandListImmediate&>(*this);
}

FORCEINLINE_DEBUGGABLE bool FRHICommandListBase::Bypass() const
{
	check(!IsImmediate() || IsInRenderingThread() || IsInRHIThread());
	return GRHICommandList.Bypass()
#if CAN_TOGGLE_COMMAND_LIST_BYPASS
		&& PersistentState.RecordingThread == ERecordingThread::Render
#endif
		;
}

FORCEINLINE_DEBUGGABLE FScopedRHIThreadStaller::FScopedRHIThreadStaller(class FRHICommandListImmediate& InImmed, bool bDoStall)
	: Immed(nullptr)
{
	if (bDoStall && IsRunningRHIInSeparateThread())
	{
		check(IsInRenderingThread());
		if (InImmed.StallRHIThread())
		{
			Immed = &InImmed;
		}
	}
}

FORCEINLINE_DEBUGGABLE FScopedRHIThreadStaller::~FScopedRHIThreadStaller()
{
	if (Immed)
	{
		Immed->UnStallRHIThread();
	}
}

namespace PipelineStateCache
{
	/* Evicts unused state entries based on r.pso.evictiontime time. Called in RHICommandList::BeginFrame */
	extern RHI_API void FlushResources();
}

inline void FRHIComputeCommandList::SubmitCommandsHint()
{
	if (IsImmediate())
	{
		static_cast<FRHICommandListImmediate&>(*this).SubmitCommandsHint();
	}
}

FORCEINLINE_DEBUGGABLE void FRHICommandListImmediate::ImmediateFlush(EImmediateFlushType::Type FlushType)
{
	if (FlushType == EImmediateFlushType::WaitForOutstandingTasksOnly)
	{
		WaitForTasks();
	}
	else
	{
		if (FlushType >= EImmediateFlushType::DispatchToRHIThread)
		{
			// Execution and initialization are separate functions because initializing the immediate contexts
			// may enqueue a lambda call to SwitchPipeline, which needs special handling in LatchBypass().
			ExecuteAndReset();
			InitializeImmediateContexts();
		}

		if (FlushType >= EImmediateFlushType::FlushRHIThread)
		{
			CSV_SCOPED_TIMING_STAT(RHITFlushes, FlushRHIThreadTotal);
			WaitForRHIThreadTasks();
		}

		if (FlushType >= EImmediateFlushType::FlushRHIThreadFlushResources)
		{
			CSV_SCOPED_TIMING_STAT(RHITFlushes, FlushRHIThreadFlushResourcesTotal);
			// @todo: do this before the dispatch, since FlushResources enqueues a lambda to hand down the list of RHI resources to the RHI thread.
			// Also work out when the PSO cache needs to be flushed (before dispatch, or after thread flush?)
			PipelineStateCache::FlushResources();
			FRHIResource::FlushPendingDeletes(*this);
		}
	}
}

// Helper class for traversing a FRHICommandList
class FRHICommandListIterator
{
public:
	FRHICommandListIterator(FRHICommandListBase& CmdList)
	{
		CmdPtr = CmdList.Root;
#if RHI_COUNT_COMMANDS
		NumCommands = 0;
		CmdListNumCommands = CmdList.NumCommands;
#endif
	}
	~FRHICommandListIterator()
	{
#if RHI_COUNT_COMMANDS
		checkf(CmdListNumCommands == NumCommands, TEXT("Missed %d Commands!"), CmdListNumCommands - NumCommands);
#endif
	}

	FORCEINLINE_DEBUGGABLE bool HasCommandsLeft() const
	{
		return !!CmdPtr;
	}

	FORCEINLINE_DEBUGGABLE FRHICommandBase* NextCommand()
	{
		FRHICommandBase* RHICmd = CmdPtr;
		CmdPtr = RHICmd->Next;
#if RHI_COUNT_COMMANDS
		NumCommands++;
#endif
		return RHICmd;
	}

private:
	FRHICommandBase* CmdPtr;

#if RHI_COUNT_COMMANDS
	uint32 NumCommands;
	uint32 CmdListNumCommands;
#endif
};

