// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "HAL/Runnable.h"
#include "HttpPackage.h"
#include "Misc/SingleThreadRunnable.h"
#include "Containers/Queue.h"
#include "Containers/SpscQueue.h"

#include <atomic>

class IHttpThreadedRequest;

/**
 * Manages Http thread
 * Assumes any requests entering the system will remain valid (not deleted) until they exit the system
 */
class FHttpThread
	: FRunnable, FSingleThreadRunnable
{
public:

	FHttpThread();
	virtual ~FHttpThread();

	/** 
	 * Start the HTTP thread.
	 */
	void StartThread();

	/** 
	 * Stop the HTTP thread.  Blocks until thread has stopped.
	 */
	void StopThread();

	/**
	 * Is the HTTP thread started or stopped.
	 */
	bool IsStopped() const { return bIsStopped; }
	/** 
	 * Add a request to begin processing on HTTP thread.
	 *
	 * @param Request the request to be processed on the HTTP thread
	 */
	void AddRequest(IHttpThreadedRequest* Request);

	/** 
	 * Mark a request as cancelled.    Called on non-HTTP thread.
	 *
	 * @param Request the request to be processed on the HTTP thread
	 */
	void CancelRequest(IHttpThreadedRequest* Request);

	/** 
	 * Get completed requests.  Clears internal arrays.  Called on non-HTTP thread.
	 *
	 * @param OutCompletedRequests array of requests that have been completed
	 */
	void GetCompletedRequests(TArray<IHttpThreadedRequest*>& OutCompletedRequests);

	//~ Begin FSingleThreadRunnable Interface
	// Cannot be overriden to ensure identical behavior with the threaded tick
	virtual void Tick() override final;
	//~ End FSingleThreadRunnable Interface

	/**
	 * When true the owner of the HTTPThread needs to manually call Tick() since no automomous threads are
	 * executing the runnable object
	 */
	bool NeedsSingleThreadTick() const;

	/**
	 * Update configuration. Called when config has been updated and we need to apply any changes.
	 */
	virtual void UpdateConfigs();

protected:

	/**
	 * Tick on http thread
	 */
	virtual void HttpThreadTick(float DeltaSeconds);
	
	/** 
	 * Start processing a request on the http thread
	 */
	virtual bool StartThreadedRequest(IHttpThreadedRequest* Request);

	/** 
	 * Complete a request on the http thread
	 */
	virtual void CompleteThreadedRequest(IHttpThreadedRequest* Request);


protected:
	// Threading functions

	//~ Begin FRunnable Interface
	virtual bool Init() override;
	virtual uint32 Run() override final;
	virtual void Stop() override;
	virtual void Exit() override;
	//~ End FRunnable Interface

	/**
	*  FSingleThreadRunnable accessor for ticking this FRunnable when multi-threading is disabled.
	*  @return FSingleThreadRunnable Interface for this FRunnable object.
	*/
	virtual class FSingleThreadRunnable* GetSingleThreadInterface() override { return this; }

	void Process(TArray<IHttpThreadedRequest*>& RequestsToCancel, TArray<IHttpThreadedRequest*>& RequestsToComplete);

	/** signal request to stop and exit thread */
	FThreadSafeCounter ExitRequest;

	/** Time in seconds to use as frame time when actively processing requests. 0 means no frame time. */
	double HttpThreadActiveFrameTimeInSeconds;
	/** Time in seconds to sleep minimally when actively processing requests. */
	double HttpThreadActiveMinimumSleepTimeInSeconds;
	/** Time in seconds to use as frame time when idle, waiting for requests. 0 means no frame time. */
	double HttpThreadIdleFrameTimeInSeconds;
	/** Time in seconds to sleep minimally when idle, waiting for requests. */
	double HttpThreadIdleMinimumSleepTimeInSeconds;
	/** Last time the thread has been processed. Used in the non-game thread. */
	double LastTime;

protected:
	/** 
	 * Threaded requests that are waiting to be processed on the http thread.
	 * Added to on (any) non-HTTP thread, processed then cleared on HTTP thread.
	 */
	TQueue<IHttpThreadedRequest*, EQueueMode::Mpsc> NewThreadedRequests;

	/**
	 * Threaded requests that are waiting to be cancelled on the http thread.
	 * Added to on (any) non-HTTP thread, processed then cleared on HTTP thread.
	 */
	TQueue<IHttpThreadedRequest*, EQueueMode::Mpsc> CancelledThreadedRequests;

	/**
	 * Threaded requests that are ready to run, but waiting due to the running request limit (not in any of the other lists, except potentially CancelledThreadedRequests).
	 * Only accessed on the HTTP thread.
	 */
	TArray<IHttpThreadedRequest*> RateLimitedThreadedRequests;

	/**
	 * Currently running threaded requests (not in any of the other lists, except potentially CancelledThreadedRequests).
	 * Only accessed on the HTTP thread.
	 */
	TArray<IHttpThreadedRequest*> RunningThreadedRequests;

	/**
	 * Threaded requests that have completed and are waiting for the game thread to process.
	 * Added to on HTTP thread, processed then cleared on game thread (Single producer, single consumer)
	 */
	TSpscQueue<IHttpThreadedRequest*> CompletedThreadedRequests;

	/** Pointer to Runnable Thread */
	FRunnableThread* Thread;

private:

	/** Are we holding a fake thread and we need to be ticked manually when Flushing */
	bool bIsSingleThread;

	/** Tells if the runnable thread is running or stopped */
	bool bIsStopped;

	/** Limit for threaded http requests running at the same time. If not specified through configuration values, there will be no limit */
	std::atomic<int32> RunningThreadedRequestLimit = INT_MAX;
};
