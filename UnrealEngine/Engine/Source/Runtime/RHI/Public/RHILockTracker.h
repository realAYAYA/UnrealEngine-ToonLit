// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHICommandList.h"

struct FRHILockTracker
{
	struct FLockParams
	{
		void* RHIBuffer;
		void* Buffer;
		uint32 BufferSize;
		uint32 Offset;
		EResourceLockMode LockMode;
		bool bDirectLock; //did we call the normal flushing/updating lock?
		bool bCreateLock; //did we lock to immediately initialize a newly created buffer?

		FORCEINLINE_DEBUGGABLE FLockParams(void* InRHIBuffer, void* InBuffer, uint32 InOffset, uint32 InBufferSize, EResourceLockMode InLockMode, bool bInbDirectLock, bool bInCreateLock)
			: RHIBuffer(InRHIBuffer)
			, Buffer(InBuffer)
			, BufferSize(InBufferSize)
			, Offset(InOffset)
			, LockMode(InLockMode)
			, bDirectLock(bInbDirectLock)
			, bCreateLock(bInCreateLock)
		{
		}
	};

	struct FUnlockFenceParams
	{
		FUnlockFenceParams(void* InRHIBuffer, FGraphEventRef InUnlockEvent)
			: RHIBuffer(InRHIBuffer)
			, UnlockEvent(InUnlockEvent)
		{

		}
		void* RHIBuffer;
		FGraphEventRef UnlockEvent;
	};

	TArray<FLockParams, TInlineAllocator<16> > OutstandingLocks;
	uint32 TotalMemoryOutstanding;
	TArray<FUnlockFenceParams, TInlineAllocator<16> > OutstandingUnlocks;

	FRHILockTracker()
	{
		TotalMemoryOutstanding = 0;
	}

	FORCEINLINE_DEBUGGABLE void Lock(void* RHIBuffer, void* Buffer, uint32 Offset, uint32 SizeRHI, EResourceLockMode LockMode, bool bInDirectBufferWrite = false, bool bInCreateLock = false)
	{
#if DO_CHECK
		for (auto& Parms : OutstandingLocks)
		{
			check((Parms.RHIBuffer != RHIBuffer) || (Parms.bDirectLock && bInDirectBufferWrite) || Parms.Offset != Offset);
		}
#endif
		OutstandingLocks.Add(FLockParams(RHIBuffer, Buffer, Offset, SizeRHI, LockMode, bInDirectBufferWrite, bInCreateLock));
		TotalMemoryOutstanding += SizeRHI;
	}
	FORCEINLINE_DEBUGGABLE FLockParams Unlock(void* RHIBuffer, uint32 Offset = 0)
	{
		for (int32 Index = 0; Index < OutstandingLocks.Num(); Index++)
		{
			if (OutstandingLocks[Index].RHIBuffer == RHIBuffer && OutstandingLocks[Index].Offset == Offset)
			{
				FLockParams Result = OutstandingLocks[Index];
				OutstandingLocks.RemoveAtSwap(Index, 1, EAllowShrinking::No);
				return Result;
			}
		}
		RaiseMismatchError();
		return FLockParams(nullptr, nullptr, 0, 0, RLM_WriteOnly, false, false);
	}

	template<class TIndexOrVertexBufferPointer>
	FORCEINLINE_DEBUGGABLE void AddUnlockFence(TIndexOrVertexBufferPointer* Buffer, FRHICommandListImmediate& RHICmdList, const FLockParams& LockParms)
	{
		if (LockParms.LockMode != RLM_WriteOnly || !(Buffer->GetUsage() & BUF_Volatile))
		{
			OutstandingUnlocks.Emplace(Buffer, RHICmdList.RHIThreadFence(true));
		}
	}

	FORCEINLINE_DEBUGGABLE void WaitForUnlock(void* RHIBuffer)
	{
		for (int32 Index = 0; Index < OutstandingUnlocks.Num(); Index++)
		{
			if (OutstandingUnlocks[Index].RHIBuffer == RHIBuffer)
			{
				FRHICommandListExecutor::WaitOnRHIThreadFence(OutstandingUnlocks[Index].UnlockEvent);
				OutstandingUnlocks.RemoveAtSwap(Index, 1, EAllowShrinking::No);
				return;
			}
		}
	}

	FORCEINLINE_DEBUGGABLE void FlushCompleteUnlocks()
	{
		uint32 Count = OutstandingUnlocks.Num();
		for (uint32 Index = 0; Index < Count; Index++)
		{
			if (OutstandingUnlocks[Index].UnlockEvent->IsComplete())
			{
				OutstandingUnlocks.RemoveAt(Index, 1);
				--Count;
				--Index;
			}
		}
	}

	RHI_API void RaiseMismatchError();
};

extern RHI_API FRHILockTracker GRHILockTracker;
