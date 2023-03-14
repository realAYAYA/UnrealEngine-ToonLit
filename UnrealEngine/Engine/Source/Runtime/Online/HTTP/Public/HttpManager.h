// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Queue.h"
#include "Containers/Ticker.h"
#include "Containers/Ticker.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "HAL/CriticalSection.h"
#include "HttpPackage.h"
#include "Interfaces/IHttpRequest.h"
#include "Misc/EnumRange.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"

class FHttpThread;
class FOutputDevice;
class IHttpThreadedRequest;

enum class EHttpFlushReason : uint8
{
	/** Reasonable, typically higher time limits */
	Default,
	/** Shorter time limits depending on platform requirements */
	Background,
	/** Shorter time limits depending on platform requirements */
	Shutdown,
	/** Infinite wait, should only be used in non-game scenarios where longer waits are acceptable */
	FullFlush,
	Count
};
ENUM_RANGE_BY_COUNT(EHttpFlushReason, EHttpFlushReason::Count)
const TCHAR* LexToString(const EHttpFlushReason& FlushReason);

/**
 * Delegate called when an Http request added
 *
 * @param Request Http request that start things
 */
DECLARE_DELEGATE_OneParam(FHttpManagerRequestAddedDelegate, const FHttpRequestRef& /*Request*/);

/**
 * Manages Http request that are currently being processed
 */
class HTTP_API FHttpManager
	: public FTSTickerObjectBase
{
public:

	// FHttpManager

	/**
	 * Constructor
	 */
	FHttpManager();

	/**
	 * Destructor
	 */
	virtual ~FHttpManager();

	/**
	 * Initialize
	 */
	void Initialize();

	/**
	 * Adds an Http request instance to the manager for tracking/ticking
	 * Manager should always have a list of requests currently being processed
	 *
	 * @param Request - the request object to add
	 */
	void AddRequest(const FHttpRequestRef& Request);

	/** Delegate that will get called once request added */
	FHttpManagerRequestAddedDelegate RequestAddedDelegate;

	/**
	 * Removes an Http request instance from the manager
	 * Presumably it is done being processed
	 *
	 * @param Request - the request object to remove
	 */
	void RemoveRequest(const FHttpRequestRef& Request);

	/**
	* Find an Http request in the lists of current valid requests
	*
	* @param RequestPtr - ptr to the http request object to find
	*
	* @return true if the request is being tracked, false if not
	*/
	bool IsValidRequest(const IHttpRequest* RequestPtr) const;

	/**
	 * Block until all pending requests are finished processing
	 *
	 * @param bShutdown true if final flush during shutdown
	 */
	UE_DEPRECATED(5.0, "This method is deprecated. Please use Flush(EHttpFlushReason FlushReason) instead.")
	void Flush(bool bShutdown);

	/**
	 * Block until all pending requests are finished processing
	 *
	 * @param FlushReason the flush reason will influence times waited to cancel ongoing http requests
	 */
	void Flush(EHttpFlushReason FlushReason);

	/**
	 * FTSTicker callback
	 *
	 * @param DeltaSeconds - time in seconds since the last tick
	 *
	 * @return false if no longer needs ticking
	 */
	bool Tick(float DeltaSeconds) override;

	/**
	 * Tick called during Flush
	 *
	 * @param DeltaSeconds - time in seconds since the last tick
	 */
	virtual void FlushTick(float DeltaSeconds);

	/** 
	 * Add a http request to be executed on the http thread
	 *
	 * @param Request - the request object to add
	 */
	void AddThreadedRequest(const TSharedRef<IHttpThreadedRequest, ESPMode::ThreadSafe>& Request);

	/**
	 * Mark a threaded http request as cancelled to be removed from the http thread
	 *
	 * @param Request - the request object to cancel
	 */
	void CancelThreadedRequest(const TSharedRef<IHttpThreadedRequest, ESPMode::ThreadSafe>& Request);

	/**
	 * List all of the Http requests currently being processed
	 *
	 * @param Ar - output device to log with
	 */
	void DumpRequests(FOutputDevice& Ar) const;

	/**
	 * Method to check dynamic proxy setting support.
	 *
	 * @returns Whether this http implementation supports dynamic proxy setting.
	 */
	virtual bool SupportsDynamicProxy() const;

	/**
	 * Set the method used to set a Correlation id on each request, if one is not already specified.
	 *
	 * This method allows you to override the Engine default method.
	 *
	 * @param InCorrelationIdMethod The method to use when sending a request, if no Correlation id is already set
	 */
	void SetCorrelationIdMethod(TFunction<FString()> InCorrelationIdMethod);

	/**
	 * Create a new correlation id for a request
	 *
	 * @return The new correlationid string
	 */
	FString CreateCorrelationId() const;

	/**
	 * Determine if the domain is allowed to be accessed
	 *
	 * @param Url the path to check domain on
	 *
	 * @return true if domain is allowed
	 */
	bool IsDomainAllowed(const FString& Url) const;

	/**
	 * Get the default method for creating new correlation ids for a request
	 *
	 * @return The default correlationid creation method
	 */
	static TFunction<FString()> GetDefaultCorrelationIdMethod();

	/**
	 * Inform that HTTP Manager that we are about to fork(). Will block to flush all outstanding http requests
	 */
	virtual void OnBeforeFork();

	/**
	 * Inform that HTTP Manager that we have completed a fork(). Must be called in both the client and parent process
	 */
	virtual void OnAfterFork();

	/**
	 * Inform the HTTP Manager that we finished ticking right after forking. Only called on the forked process
	 */
	virtual void OnEndFramePostFork();


	/**
	 * Update configuration. Called when config has been updated and we need to apply any changes.
	 */
	virtual void UpdateConfigs();

	/**
	 * Add task to be ran on the game thread next tick
	 *
	 * @param Task The task to be ran next tick
	 */
	void AddGameThreadTask(TFunction<void()>&& Task);

protected:
	/** 
	 * Create HTTP thread object
	 *
	 * @return the HTTP thread object
	 */
	virtual FHttpThread* CreateHttpThread();

	void ReloadFlushTimeLimits();

protected:
	/** List of Http requests that are actively being processed */
	TArray<FHttpRequestRef> Requests;

	FHttpThread* Thread;

	/** This method will be called to generate a CorrelationId on all requests being sent if one is not already set */
	TFunction<FString()> CorrelationIdMethod;

	/** Queue of tasks to run on the game thread */
	TQueue<TFunction<void()>, EQueueMode::Mpsc> GameThreadQueue;

	// This variable is set to true in Flush(EHttpFlushReason), and prevents new Http requests from being launched
	bool bFlushing;

	struct FHttpFlushTimeLimit
	{
		/**
		 * Designates the amount of time we will wait during a flush before we try to cancel the request.
		 * This MUST be strictly < FlushTimeHardLimitSeconds for the logic to work and actually cancel the request, since we must Tick at least one time for the cancel to work.
		 * Setting this to 0 will immediately cancel all ongoing requests. A hard limit is still required for this to work.
		 * Setting this to < 0 will disable the cancel, but FlushTimeHardLimitSeconds can still be used to stop waiting on requests.
		 */		
		double SoftLimitSeconds;

		/**
		 * After we hit the soft time limit and cancel the requests, we wait some additional time for the canceled requests to go away.
		 * If they don't go away in time, we will hit this "hard" time limit that will just stop waiting.
		 * If we are shutting down, this is probably fine. If we are flushing for other reasons,
		 * This could indicate things lying around, and we'll put out some warning log messages to indicate this.
		 * Setting this to < 0 will disable all time limits and the code will wait infinitely for all requests to complete.
		 */
		double HardLimitSeconds;

		FHttpFlushTimeLimit(double InSoftLimitSeconds, double InHardLimitSeconds)
			: SoftLimitSeconds(InSoftLimitSeconds), HardLimitSeconds(InHardLimitSeconds)
		{}
	};

	TMap<EHttpFlushReason, FHttpFlushTimeLimit> FlushTimeLimitsMap;

PACKAGE_SCOPE:

	/** Used to lock access to add/remove/find requests */
	static FCriticalSection RequestLock;
};
