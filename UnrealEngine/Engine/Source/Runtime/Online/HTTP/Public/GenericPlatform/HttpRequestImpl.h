// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/IHttpRequest.h"
#include "Misc/Optional.h"

/**
 * Contains implementation of some common functions that don't vary between implementation
 */
class FHttpRequestImpl : public IHttpRequest
{
public:
	// IHttpRequest
	HTTP_API virtual FHttpRequestCompleteDelegate& OnProcessRequestComplete() override;
	HTTP_API virtual FHttpRequestProgressDelegate& OnRequestProgress() override;
	HTTP_API virtual FHttpRequestHeaderReceivedDelegate& OnHeaderReceived() override;
	HTTP_API virtual FHttpRequestWillRetryDelegate& OnRequestWillRetry() override;

	HTTP_API virtual void SetTimeout(float InTimeoutSecs) override;
	HTTP_API virtual void ClearTimeout() override;
	HTTP_API virtual TOptional<float> GetTimeout() const override;

	HTTP_API float GetTimeoutOrDefault() const;

protected:
	/** 
	 * Broadcast all of our response's headers as having been received
	 * Used when we don't know when we receive headers in our HTTP implementation
	 */
	HTTP_API void BroadcastResponseHeadersReceived();

protected:
	/** Delegate that will get called once request completes or on any error */
	FHttpRequestCompleteDelegate RequestCompleteDelegate;

	/** Delegate that will get called once per tick with bytes downloaded so far */
	FHttpRequestProgressDelegate RequestProgressDelegate;

	/** Delegate that will get called for each new header received */
	FHttpRequestHeaderReceivedDelegate HeaderReceivedDelegate;
	
	/** Delegate that will get called when request will be retried */
	FHttpRequestWillRetryDelegate OnRequestWillRetryDelegate;

	/** Timeout in seconds for the entire HTTP request to complete */
	TOptional<float> TimeoutSecs;
};
