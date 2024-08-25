// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
RenderAssetUpdate.h: Base class of helpers to stream in and out texture/mesh LODs
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "Async/AsyncWork.h"
#include "RenderingThread.h"
#include "Streaming/StreamableRenderResourceState.h"

class UStreamableRenderAsset;

/** SRA stands for StreamableRenderAsset */
#define SRA_UPDATE_CALLBACK(FunctionName) [this](const FContext& C){ FunctionName(C); }

ENGINE_API bool IsAssetStreamingSuspended();

/**
* This class provides a framework for loading and unloading the texture/mesh LODs.
* Each thread essentially calls Tick() until the job is done.
* The object can be safely deleted when IsCompleted() returns true.
*/
class ENGINE_API FRenderAssetUpdate : public IRefCountedObject
{
public:

	/** A thread type used for doing a part of the update process.  */
	enum EThreadType
	{
		TT_None,	// No thread.
		TT_Render,	// The render thread.
		TT_Async,	// An async work thread.
		TT_GameThread, // The game thread
		TT_GameRunningAsync	// The game thread running async work.
	};

	/**  The state of scheduled work for the update process. */
	enum ETaskState
	{
		TS_Done,		// Nothing to do.
		TS_Suspended,	// Waiting for external events, like IO requests, or when the renderthread is suspended.
		TS_InProgress,	// Callbacks are about to be executed.
		TS_Locked,		// The object is locked, and no one is allowed to process or look at the next task.
		TS_Init			// The object is in initialization
	};

	FRenderAssetUpdate(const UStreamableRenderAsset* InAsset);

	/**
	* Do or schedule any pending work for a given texture.
	*
	* @param InAsset - the texture/mesh being updated, this must be the same texture/mesh as the texture/mesh used to create this object.
	* @param InCurrentThread - the thread from which the tick is being called. Using TT_None ensures that no work will be immediately performed.
	*/
	void Tick(EThreadType InCurrentThread);

	/** Returns whether the task has finished executing and there is no other thread possibly accessing it. */
	bool IsCompleted() const
	{
		return TaskState == TS_Done;
	}

	/**
	* Cancel the current update. Will also attempt to cancel pending IO requests, see FTexture2DStreamIn_IO::Abort().
	* This is called outside of the update loop when a cancelation is required for external events.
	*/
	virtual void Abort()
	{
		MarkAsCancelled();
	}

	/** Returns whether the task was aborted through Abort() or cancelled.  */
	bool IsCancelled() const
	{
		return bIsCancelled;
	}

	/** Returns whether this update has finished successfully. */
	bool IsSuccessfullyFinished() const
	{
		return bSuccess;
	}

	/** Perform a lock on the object, preventing any other thread from processing a pending task in Tick(). */
	ETaskState DoLock();

	/** Release any lock on the object, allowing other thread to modify it. */
	void DoUnlock(ETaskState PreviousTaskState);

	bool IsLocked() const
	{
		return TaskState == TS_Locked;
	}

	/** Return the thread relevant to the next step of execution. */
	virtual EThreadType GetRelevantThread() const = 0;

	//****************** IRefCountedObject ****************/

	uint32 AddRef() const final override 
	{ 
		return (uint32)NumRefs.Increment(); 
	}

	uint32 Release() const final override;

	uint32 GetRefCount() const final override
	{
		return (uint32)NumRefs.GetValue();
	}

protected:

	virtual ~FRenderAssetUpdate();

	virtual ETaskState TickInternal(EThreadType InCurrentThread, bool bCheckForSuspension) = 0;

	/** Set the task state as cancelled. This is internally called in Abort() and when any critical conditions are not met when performing the update. */
	void MarkAsCancelled()
	{
		// StreamableAsset = nullptr; // TODO once Cancel supports it!

		if (TaskState != TS_Done) // do not cancel if we have already completed, see FORT-345212.
		{
			bIsCancelled = true;
		}
	}

	void MarkAsSuccessfullyFinished()
	{
		bSuccess = true;
	}

	void ScheduleGTTask();
	void ScheduleRenderTask();
	void ScheduleAsyncTask();

	friend class FRenderAssetUpdateTickGTTask;

	/** An async task used to call tick on the pending update. */
	class FMipUpdateTask : public FNonAbandonableTask
	{
	public:
		FMipUpdateTask(FRenderAssetUpdate* InPendingUpdate) : PendingUpdate(InPendingUpdate) {}

		void DoWork();

		FORCEINLINE TStatId GetStatId() const
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FMipUpdateTask, STATGROUP_ThreadPoolAsyncTasks);
		}

	protected:
		TRefCountPtr<FRenderAssetUpdate> PendingUpdate;
	};

	/** The async task to update this object, only one can be active at anytime. It just calls Tick(). */
	typedef FAutoDeleteAsyncTask<FMipUpdateTask> FAsyncMipUpdateTask;

	/** The streamable state requested. */
	const FStreamableRenderResourceState ResourceState;
	// The resident first LOD resource index. With domain = [0, ResourceState.NumLODs[. NOT THE ASSET LOD INDEX!
	const int32 CurrentFirstLODIdx = INDEX_NONE;
	// The requested first LOD resource index. With domain = [0, ResourceState.NumLODs[. NOT THE ASSET LOD INDEX!
	const int32 PendingFirstLODIdx = INDEX_NONE;
	
	/** Critical Section. */
	FCriticalSection CS;

	/** Number of Ticks scheduled on the game thread. */
	int32 ScheduledGTTasks;

	/** Number of Ticks scheduled on the render thread. */
	int32 ScheduledRenderTasks;

	/** Number of Ticks scheduled on async tasks. */
	int32 ScheduledAsyncTasks;

	/** The asset updated **/
	const UStreamableRenderAsset* StreamableAsset = nullptr;

	/** Synchronization used for trigger the task next step execution. */
	FThreadSafeCounter	TaskSynchronization;

	/** Whether the task has been cancelled because the update could not proceed or because the user called Abort(). */
	bool bIsCancelled;

	/** Defer execution even if a task pushes a new task on the same thread. */
	bool bDeferExecution;

	/** Whether this update finished successfully. True means that it is TS_Done and not cancelled mid way. */
	bool bSuccess;

	/** The state of the work yet to be performed to complete the update or cancelation. */
	volatile ETaskState TaskState;

private:

	/** Ref couting **/
	mutable FThreadSafeCounter NumRefs;
};

/**
* This class provides a framework for loading and unloading the texture/mesh LODs.
* Each thread essentially calls Tick() until the job is done.
* The object can be safely deleted when IsCompleted() returns true.
*/
template <typename TContext>
class TRenderAssetUpdate : public FRenderAssetUpdate
{
public:

	typedef TContext FContext;

	/** A callback used to perform a task in the update process. Each task must be executed on a specific thread. */
	typedef TFunction<void(const FContext& Context)> FCallback;

	TRenderAssetUpdate(const UStreamableRenderAsset* InAsset);

	/**
	* Defines the next step to be executed. The next step will be executed by calling the callback on the specified thread.
	* The callback (for both success and cancelation) will only be executed if TaskSynchronization reaches 0.
	* If all requirements are immediately satisfied when calling the PushTask the relevant callback will be called immediately.
	*
	* @param Context - The context defining which texture is being updated and on which thread this is being called.
	* @param InTaskThread - The thread on which to call the next step of the update, being TaskCallback.
	* @param InTaskCallback - The callback that will perform the next step of the update.
	* @param InCancelationThread - The thread on which to call the cancellation of the update (only if the update gets cancelled).
	* @param InCancelationCallback - The callback handling the cancellation of the update (only if the update gets cancelled).
	*/
	void PushTask(const FContext& Context, EThreadType InTaskThread, const FCallback& InTaskCallback, EThreadType InCancelationThread, const FCallback& InCancelationCallback);

	/** Return the thread relevant to the next step of execution. */
	EThreadType GetRelevantThread() const final override { return bIsCancelled ? CancelationThread : TaskThread;  }

protected:

	/** The thread on which to call the next step of the update, being TaskCallback. */
	EThreadType TaskThread;
	/** The callback that will perform the next step of the update. */
	FCallback TaskCallback;
	/** The thread on which to call the cancellation of the update (only if the update gets cancelled). */
	EThreadType CancelationThread; // The thread on which the callbacks should be called.
	/** The callback handling the cancellation of the update (only if the update gets cancelled). */
	FCallback CancelationCallback;

	ETaskState TickInternal(EThreadType InCurrentThread, bool bCheckForSuspension) final override;

	void ClearCallbacks()
	{
		TaskThread = TT_None;
		TaskCallback = nullptr;
		CancelationThread = TT_None;
		CancelationCallback = nullptr;
	}
};

void SuspendRenderAssetStreaming();
void ResumeRenderAssetStreaming();
