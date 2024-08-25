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
#if CAN_TOGGLE_COMMAND_LIST_BYPASS
	check(!IsImmediate() || IsInRenderingThread() || IsInRHIThread());
	return GRHICommandList.Bypass() && IsImmediate();
#else
	return false;
#endif
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
			const bool bFlushingResources = FlushType >= EImmediateFlushType::FlushRHIThreadFlushResources;
			ExecuteAndReset(bFlushingResources);
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

			// RHIPerFrameRHIFlushComplete was originally called from FlushPendingDeletes, which used to be here, so it was
			// running as part of the next command list. FlushPendingDeletes now runs as part of the command list being finalized
			// and submitted by this function, as it should be, but moving RHIPerFrameRHIFlushComplete there is risky, because
			// the RHIs which use it do weird things in there (e.g. D3D11 does blocking query resolve calls in that function which
			// happen to not stall because this runs when the next command list is executed, so they have time to be executed).
			EnqueueLambda([](FRHICommandListImmediate& RHICmdList)
			{
				if (GDynamicRHI)
				{
					GDynamicRHI->RHIPerFrameRHIFlushComplete();
				}
			});
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

inline FRHICommandListScopedPipelineGuard::FRHICommandListScopedPipelineGuard(FRHICommandListBase& RHICmdList)
	: RHICmdList(RHICmdList)
{
	if (RHICmdList.GetPipeline() == ERHIPipeline::None)
	{
		RHICmdList.SwitchPipeline(ERHIPipeline::Graphics);
		bPipelineSet = true;
	}
}

inline FRHICommandListScopedPipelineGuard::~FRHICommandListScopedPipelineGuard()
{
	if (bPipelineSet)
	{
		RHICmdList.SwitchPipeline(ERHIPipeline::None);
	}
}
