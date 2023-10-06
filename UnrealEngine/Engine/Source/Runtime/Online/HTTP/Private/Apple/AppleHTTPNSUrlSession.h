// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IHttpThreadedRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "PlatformHttp.h"


/**
 * Delegate invoked when in progress NSUrlSessionTask completes. It is invoked in an out of our control thread
 *
 */
DECLARE_DELEGATE(FTaskCompleteDelegate);

/**
 * Apple implementation of an Http request
 */
class FAppleHttpNSUrlSessionRequest : public IHttpThreadedRequest
{
public:
	// implementation friends
	friend class FAppleHttpNSUrlSessionResponse;


	//~ Begin IHttpBase Interface
	virtual FString GetURL() const override;
	virtual FString GetURLParameter(const FString& ParameterName) const override;
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
	virtual bool SetResponseBodyReceiveStream(TSharedRef<FArchive> Stream) override;
	virtual void SetHeader(const FString& HeaderName, const FString& HeaderValue) override;
	virtual void AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue) override;
	virtual void SetTimeout(float InTimeoutSecs) override;
	virtual void ClearTimeout() override;
	virtual TOptional<float> GetTimeout() const override;
	virtual bool ProcessRequest() override;
	virtual void CancelRequest() override;
	virtual const FHttpResponsePtr GetResponse() const override;
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
	explicit FAppleHttpNSUrlSessionRequest(NSURLSession* InSession);

	/**
	 * Destructor. Clean up any connection/request handles
	 */
	virtual ~FAppleHttpNSUrlSessionRequest();


private:

	/**
	 * Create the session connection and initiate the web request
	 *
	 * @return true if the request was started
	 */
	bool StartRequest();

	/**
	 * Close session/request handles and unregister callbacks
	 */
	void CleanupRequest();

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

	/** The response object which we will use to pair with this request */
	TSharedPtr<class FAppleHttpNSUrlSessionResponse,ESPMode::ThreadSafe> Response;

	/** Array used to retrieve back content set on the ObjC request when calling GetContent*/
	mutable TArray<uint8> StorageForGetContent;

	/** The stream to receive response body */
	TSharedPtr<FArchive> ResponseBodyReceiveStream;

	/** Time taken to complete/cancel the request. */
	float ElapsedTime;

	/** Last reported bytes written */
	int32 LastReportedBytesWritten;

	/** Last reported bytesread */
	int32 LastReportedBytesRead;
};

@class FAppleHttpNSUrlSessionResponseDelegate;

/**
 * Apple implementation of an Http response
 */
class FAppleHttpNSUrlSessionResponse : public IHttpResponse
{
private:
	// Delegate implementation. Keeps the response state and data
	FAppleHttpNSUrlSessionResponseDelegate* ResponseDelegate;

	/** Request that owns this response */
	const FAppleHttpNSUrlSessionRequest& Request;


public:
	// implementation friends
	friend class FAppleHttpNSUrlSessionRequest;


	//~ Begin IHttpBase Interface
	virtual FString GetURL() const override;
	virtual FString GetURLParameter(const FString& ParameterName) const override;
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
	 * Check whether headers are available.
	 */
	bool AreHeadersAvailable() const;

	/**
	 * Check whether a response is ready or not.
	 */
	bool IsReady() const;
	
	/**
	 * Check whether a response had an error.
	 */
	bool HadError() const;

	/**
	 * Check whether a response had a connection error.
	 */
	bool HadConnectionError() const;

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
	 * Sets delegate invoked after processing URLSession:task:didCompleteWithError:
	 * Should be set right before task is started 
	*/
	void SetInternalTaskCompleteDelegate(FTaskCompleteDelegate&& Delegate);

	/**
	 * Constructor
	 *
	 * @param InRequest - original request that created this response
	 */
	FAppleHttpNSUrlSessionResponse(const FAppleHttpNSUrlSessionRequest& InRequest);

	/**
	 * Destructor
	 */
	virtual ~FAppleHttpNSUrlSessionResponse();
};
