// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <Metal/Metal.h>
#include "MetalProfiler.h"

class FMetalCommandQueue;

/**
 * FMetalCommandList:
 * Encapsulates multiple command-buffers into an ordered list for submission. 
 * For the immediate context this is irrelevant and is merely a pass-through into the CommandQueue, but
 * for deferred/parallel contexts it is required as they must queue their command buffers until they can 
 * be committed to the command-queue in the proper order which is only known at the end of parallel encoding.
 */
class FMetalCommandList
{
public:
#pragma mark - Public C++ Boilerplate -

	/**
	 * Constructor
	 * @param InCommandQueue The command-queue to which the command-list's buffers will be submitted.
	 */
	FMetalCommandList(FMetalCommandQueue& InCommandQueue);
	
	/** Destructor */
	~FMetalCommandList(void);
	
	/**
	 * Command buffer failure reporting function.
	 * @param CompletedBuffer The buffer to check for failure.
	 */
	static void HandleMetalCommandBufferFailure(MTL::CommandBuffer* CompletedBuffer);
	
#pragma mark - Public Command List Mutators -

	/** 
	 * Commits the provided buffer to the command-list for execution. When parallel encoding this will be submitted later.
	 * @param Buffer The buffer to submit to the command-list.
	 * @param CompletionHandlers The completion handlers that should be attached to this command-buffer.
	 * @param bWait Whether to wait for the command buffer to complete - it is an error to set this to true on a deferred command-list.
	 * @param bIsLastCommandBuffer True if this is the final command buffer in a frame.
	 */
	void Commit(FMetalCommandBuffer* Buffer, TArray<FMetalCommandBufferCompletionHandler> CompletionHandlers, bool const bWait, bool const bIsLastCommandBuffer);
	
	/**
	 * Submits all outstanding command-buffers in the proper commit order to the command-queue.
	 * When more than one command-list is active the command-queue will buffer the command-lists until all are committed to guarantee order of submission to the GPU.
	 * @param Index The command-list's intended index in the command-queue.
	 * @param Count The number of command-lists that will be committed to the command-queue.
	 */
	void Submit(uint32 Index, uint32 Count);
	
#pragma mark - Public Command List Accessors -
	
	/**
	 * The index of this command-list within the parallel pass.
	 * @returns The index of this command-list within the parallel pass, 0 when IsImmediate() is true.
	 */
	uint32 GetParallelIndex(void) const { return 0; }

	/** @returns The command queue to which this command-list submits command-buffers. */
	FMetalCommandQueue& GetCommandQueue(void) const { return CommandQueue; }
	
private:
#pragma mark - Private Member Variables -
	FMetalCommandQueue& CommandQueue;
	TSharedPtr<TArray<FMetalCommandBufferTiming>, ESPMode::ThreadSafe> FrameCommitedBufferTimings;
	TSharedPtr<FMetalCommandBufferTiming, ESPMode::ThreadSafe> LastCompletedBufferTiming;
};
