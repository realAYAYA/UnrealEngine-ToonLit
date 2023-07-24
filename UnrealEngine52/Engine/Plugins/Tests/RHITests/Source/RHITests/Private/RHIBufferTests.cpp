// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHIBufferTests.h"

bool FRHIBufferTests::VerifyBufferContents(const TCHAR* TestName, FRHICommandListImmediate& RHICmdList, TArrayView<FRHIBuffer*> Buffers, TFunctionRef<bool(int32 BufferIndex, void* Ptr, uint32 NumBytes)> VerifyCallback)
{
	TArray<FStagingBufferRHIRef, TInlineAllocator<1>> StagingBuffers;

	for (FRHIBuffer* Buffer : Buffers)
	{
		FStagingBufferRHIRef StagingBuffer = RHICreateStagingBuffer();
		RHICmdList.CopyToStagingBuffer(Buffer, StagingBuffer, 0, Buffer->GetSize());
		StagingBuffers.Emplace(StagingBuffer);
	}

	// @todo - readback API is inconsistent across RHIs
	RHICmdList.BlockUntilGPUIdle();
	RHICmdList.FlushResources();

	ON_SCOPE_EXIT
	{
		// Immediate flush to clean up the staging buffer / other resources
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
	};

	for (int32 Index = 0; Index < Buffers.Num(); ++Index)
	{
		FRHIBuffer* Buffer = Buffers[Index];
		FRHIStagingBuffer* StagingBuffer = StagingBuffers[Index];
		uint32 NumBytes = Buffer->GetSize();

		void* Memory = RHILockStagingBuffer(StagingBuffer, 0, NumBytes);
		bool Result = VerifyCallback(Index, Memory, NumBytes);
		RHIUnlockStagingBuffer(StagingBuffer);

		if (!Result)
		{
			UE_LOG(LogRHIUnitTestCommandlet, Error, TEXT("Test failed (buffer index %d) \"%s\""), Index, TestName);
			return false;
		}
	}

	UE_LOG(LogRHIUnitTestCommandlet, Display, TEXT("Test passed. \"%s\""), TestName);
	return true;
}

// Copies data in the specified vertex buffer back to the CPU, and passes a pointer to that data to the provided verification lambda.
bool FRHIBufferTests::VerifyBufferContents(const TCHAR* TestName, FRHICommandListImmediate& RHICmdList, FRHIBuffer* Buffer, TFunctionRef<bool(void* Ptr, uint32 NumBytes)> VerifyCallback)
{
	bool Result;
	{
		uint32 NumBytes = Buffer->GetSize();

		FStagingBufferRHIRef StagingBuffer = RHICreateStagingBuffer();
		RHICmdList.CopyToStagingBuffer(Buffer, StagingBuffer, 0, NumBytes);

		// @todo - readback API is inconsistent across RHIs
		RHICmdList.BlockUntilGPUIdle();
		RHICmdList.FlushResources();

		void* Memory = RHILockStagingBuffer(StagingBuffer, 0, NumBytes);
		Result = VerifyCallback(Memory, NumBytes);
		RHIUnlockStagingBuffer(StagingBuffer);
	}

	// Immediate flush to clean up the staging buffer / other resources
	RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);

	if (!Result)
	{
		UE_LOG(LogRHIUnitTestCommandlet, Error, TEXT("Test failed. \"%s\""), TestName);
	}
	else
	{
		UE_LOG(LogRHIUnitTestCommandlet, Display, TEXT("Test passed. \"%s\""), TestName);
	}

	return Result;
}
