// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#define UE_API DERIVEDDATACACHE_API

class FQueuedThreadPool;

template <typename FuncType> class TUniqueFunction;

namespace UE::DerivedData { class IRequestOwner; }

namespace UE::DerivedData
{

/**
 * Launches a task in the thread pool that tracks the priority of the request owner.
 *
 * Waiting on or canceling the task before it has started executing in the thread pool will cause
 * it to be retracted and executed on the thread that asked to wait or cancel.
 *
 * Passing a null thread pool will execute the task immediately on the calling thread.
 *
 * TaskBody is guaranteed to return before the request is considered complete.
 * TaskBody is responsible for checking for cancellation by checking Owner.IsCanceled().
 *
 * @param MemoryEstimate   The estimated peak memory required to execute TaskBody.
 * @param Owner            The owner of the request that will execute TaskBody.
 * @param ThreadPool       The thread pool within which to execute the task, if not retracted.
 * @param TaskBody         The task body that will be unconditionally executed exactly once.
 */
UE_API void LaunchTaskInThreadPool(
	uint64 MemoryEstimate,
	IRequestOwner& Owner,
	FQueuedThreadPool* ThreadPool,
	TUniqueFunction<void ()>&& TaskBody);
UE_API void LaunchTaskInThreadPool(
	IRequestOwner& Owner,
	FQueuedThreadPool* ThreadPool,
	TUniqueFunction<void ()>&& TaskBody);

} // UE::DerivedData

#undef UE_API
