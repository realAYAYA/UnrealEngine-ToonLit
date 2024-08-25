// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tasks/Task.h"

////////////////////////////////////
// Render fences
////////////////////////////////////

 /**
 * Used to track pending rendering commands from the game thread.
 */
class FRenderCommandFence
{
public:

	/**
	 * Adds a fence command to the rendering command queue.
	 * Conceptually, the pending fence count is incremented to reflect the pending fence command.
	 * Once the rendering thread has executed the fence command, it decrements the pending fence count.
	 * @param bSyncToRHIAndGPU, true if we should wait for the RHI thread or GPU, otherwise we only wait for the render thread.
	 */
	RENDERCORE_API void BeginFence(bool bSyncToRHIAndGPU = false);

	/**
	 * Waits for pending fence commands to retire.
	 * @param bProcessGameThreadTasks, if true we are on a short callstack where it is safe to process arbitrary game thread tasks while we wait
	 */
	RENDERCORE_API void Wait(bool bProcessGameThreadTasks = false) const;

	// return true if the fence is complete
	RENDERCORE_API bool IsFenceComplete() const;

	// Ctor/dtor
	RENDERCORE_API FRenderCommandFence();
	RENDERCORE_API ~FRenderCommandFence();

private:
	/** Task that represents completion of this fence **/
	mutable UE::Tasks::FTask CompletionTask;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
