// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <Metal/Metal.h>

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
	FMetalCommandList(FMetalCommandQueue& InCommandQueue, bool const bInImmediate);
	
	/** Destructor */
	~FMetalCommandList(void);
	
	/**
	 * Command buffer failure reporting function.
	 * @param CompletedBuffer The buffer to check for failure.
	 */
	static void HandleMetalCommandBufferFailure(mtlpp::CommandBuffer const& CompletedBuffer);
	
#pragma mark - Public Command List Mutators -
	
	/** 
	 * Set the number of parallel command-lists and the index of this command-list within the pass.
	 * @param Index The index of this command-list within the parallel pass.
	 * @param Num The number of command-lists within the parallel pass.
	 */
	void SetParallelIndex(uint32 Index, uint32 Num);

	/** 
	 * Commits the provided buffer to the command-list for execution. When parallel encoding this will be submitted later.
	 * @param Buffer The buffer to submit to the command-list.
	 * @param CompletionHandlers The completion handlers that should be attached to this command-buffer.
	 * @param bWait Whether to wait for the command buffer to complete - it is an error to set this to true on a deferred command-list.
	 * @param bIsLastCommandBuffer True if this is the final command buffer in a frame.
	 */
	void Commit(mtlpp::CommandBuffer& Buffer, TArray<ns::Object<mtlpp::CommandBufferHandler>> CompletionHandlers, bool const bWait, bool const bIsLastCommandBuffer);
	
	/**
	 * Submits all outstanding command-buffers in the proper commit order to the command-queue.
	 * When more than one command-list is active the command-queue will buffer the command-lists until all are committed to guarantee order of submission to the GPU.
	 * @param Index The command-list's intended index in the command-queue.
	 * @param Count The number of command-lists that will be committed to the command-queue.
	 */
	void Submit(uint32 Index, uint32 Count);
	
#pragma mark - Public Command List Accessors -
	
	/**
	 * True iff the command-list submits immediately to the command-queue, false if it performs any buffering.
	 * @returns True iff the command-list submits immediately to the command-queue, false if it performs any buffering.
	 */
	bool IsImmediate(void) const { return bImmediate; }
	
	/**
	 * True iff the command-list is part of an MTLParallelRenderCommandEncoder pass.
	 * @returns True iff the command-list is part of an MTLParallelRenderCommandEncoder pass, false for immediate and parallel-command-buffer contexts.
	 */
	bool IsParallel(void) const { return !bImmediate && Num > 0 && FMetalCommandQueue::SupportsFeature(EMetalFeaturesParallelRenderEncoders); }
	

	/**
	 * The index of this command-list within the parallel pass.
	 * @returns The index of this command-list within the parallel pass, 0 when IsImmediate() is true.
	 */
	uint32 GetParallelIndex(void) const { return Index; }
	
	/**
	 * The number of command-lists within the parallel pass.
	 * @returns The number of command-lists within the parallel pass, 0 when IsImmediate() is true.
	 */
	uint32 GetParallelNum(void) const { return Num; }

	/** @returns The command queue to which this command-list submits command-buffers. */
	FMetalCommandQueue& GetCommandQueue(void) const { return CommandQueue; }
	
private:
#pragma mark - Private Member Variables -
	FMetalCommandQueue& CommandQueue;
	TSharedPtr<TArray<FMetalCommandBufferTiming>, ESPMode::ThreadSafe> FrameCommitedBufferTimings;
	TSharedPtr<FMetalCommandBufferTiming, ESPMode::ThreadSafe> LastCompletedBufferTiming;
	TArray<mtlpp::CommandBuffer> SubmittedBuffers;
	uint32 Index;
	uint32 Num;
	bool bImmediate;
};
