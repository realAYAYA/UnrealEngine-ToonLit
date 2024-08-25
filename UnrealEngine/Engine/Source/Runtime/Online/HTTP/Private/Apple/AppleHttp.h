// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/HttpResponseCommon.h"
#include "IHttpThreadedRequest.h"
#include "PlatformHttp.h"
#include "HttpPackage.h"

/**
 * Delegate invoked when in progress Task completes. It is invoked in an out of our control thread
 *
 */
DECLARE_DELEGATE(FNewAppleHttpEventDelegate);

/**
 * Apple implementation of an Http request
 */
class FAppleHttpRequest : public IHttpThreadedRequest
{
public:
	// implementation friends
	friend class FAppleHttpResponse;

	//~ Begin IHttpBase Interface
	virtual FString GetURL() const override;
	virtual FString GetHeader(const FString& HeaderName) const override;
	virtual TArray<FString> GetAllHeaders() const override;	
	virtual FString GetContentType() const override;
	virtual uint64 GetContentLength() const override;
	virtual const TArray<uint8>& GetContent() const override;
	//~ End IHttpBase Interface

	//~ Begin IHttpRequest Interface
	virtual FString GetVerb() const override;
	virtual void SetVerb(const FString& Verb) override;
	virtual void SetURL(const FString& URL) override;
	virtual void SetContent(const TArray<uint8>& ContentPayload) override;
	virtual void SetContent(TArray<uint8>&& ContentPayload) override;
	virtual void SetContentAsString(const FString& ContentString) override;
    virtual bool SetContentAsStreamedFile(const FString& Filename) override;
	virtual bool SetContentFromStream(TSharedRef<FArchive, ESPMode::ThreadSafe> Stream) override;
	virtual void SetHeader(const FString& HeaderName, const FString& HeaderValue) override;
	virtual void AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue) override;
	virtual bool ProcessRequest() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float GetElapsedTime() const override;
	//~ End IHttpRequest Interface

	//~ Begin IHttpRequestThreaded Interface
	virtual bool StartThreadedRequest() override;
	virtual void FinishRequest() override;
	virtual bool IsThreadedRequestComplete() override;
	virtual void TickThreadedRequest(float DeltaSeconds) override;
	//~ End IHttpRequestThreaded Interface

	/**
	 * Constructor
	 *
	 * @param InSession - NSURLSession session used to create NSURLSessionTask to retrieve the response
	 */
	explicit FAppleHttpRequest(NSURLSession* InSession);

	/**
	 * Destructor. Clean up any connection/request handles
	 */
	virtual ~FAppleHttpRequest();

PACKAGE_SCOPE:
	using FHttpRequestCommon::BroadcastResponseHeadersReceived;
	using FHttpRequestCommon::TriggerStatusCodeReceivedDelegate;
	using FHttpRequestCommon::SetEffectiveURL;
	using FHttpRequestCommon::PassReceivedDataToStream;

	bool IsInitializedWithValidStream() const;

private:
	/**
	 * Trigger the request progress delegate if progress has changed
	 */
	void CheckProgressDelegate();

	/**
	 * Create the session connection and initiate the web request
	 *
	 * @return true if the request was started
	 */
	virtual bool SetupRequest() override;

	virtual void AbortRequest() override;

	/**
	 * Close session/request handles and unregister callbacks
	 */
	virtual void CleanupRequest() override;

private:
	/** This is the NSMutableURLRequest, all our Apple functionality will deal with this. */
	NSMutableURLRequest* Request;

	/** This is the session our request belongs to */
	NSURLSession* Session;
	
	/** This is the Task associated to the sessionin charge of our request */
	NSURLSessionTask* Task;
    
	/** Flag whether the request payload source is a file */
	bool bIsPayloadFile;

	/** The request payload length in bytes. This must be tracked separately for a file stream */
	uint64 ContentBytesLength;

	/** Array used to retrieve back content set on the ObjC request when calling GetContent*/
	mutable TArray<uint8> StorageForGetContent;

	/** Time taken to complete/cancel the request. */
	float ElapsedTime;

	/** Last reported bytes written */
	int32 LastReportedBytesWritten;

	/** Last reported bytesread */
	int32 LastReportedBytesRead;
};

@class FAppleHttpResponseDelegate;

/**
 * Apple implementation of an Http response
 */
class FAppleHttpResponse : public FHttpResponseCommon
{
public:
	//~ Begin IHttpBase Interface
	virtual FString GetHeader(const FString& HeaderName) const override;
	virtual TArray<FString> GetAllHeaders() const override;	
	virtual FString GetContentType() const override;
	virtual uint64 GetContentLength() const override;
	virtual const TArray<uint8>& GetContent() const override;
	//~ End IHttpBase Interface

	//~ Begin IHttpResponse Interface
	virtual int32 GetResponseCode() const override;
	virtual FString GetContentAsString() const override;
	//~ End IHttpResponse Interface

	/**
	 * Check whether a response is ready or not.
	 */
	bool IsReady() const;

	/**
	 * Get the number of bytes received so far
	 */
	const uint64 GetNumBytesReceived() const;

	/**
	* Get the number of bytes sent so far
	*/
	const uint64 GetNumBytesWritten() const;

	/**
	 * Cleans internal shared objects between request and response
	 */
	void CleanSharedObjects();

	/**
	 * Sets delegate invoked when  URLSession:dataTask:didReceiveData or URLSession:task:didCompleteWithError: are triggered
	 * Should be set right before task is started 
	*/
	void SetNewAppleHttpEventDelegate(FNewAppleHttpEventDelegate&& Delegate);

	/**
	 * Constructor
	 *
	 * @param InRequest - original request that created this response
	 */
	FAppleHttpResponse(FAppleHttpRequest& InRequest);

	/**
	 * Destructor
	 */
	virtual ~FAppleHttpResponse();

private:
	// implementation friends
	friend class FAppleHttpRequest;

	/**
	 * Get status from the internal delegate
	 */
	EHttpRequestStatus::Type GetStatusFromDelegate() const;

	/**
	 * Get reason of failure from the internal delegate
	 */
	EHttpFailureReason GetFailureReasonFromDelegate() const;

	// Delegate implementation. Keeps the response state and data
	FAppleHttpResponseDelegate* ResponseDelegate;
};
