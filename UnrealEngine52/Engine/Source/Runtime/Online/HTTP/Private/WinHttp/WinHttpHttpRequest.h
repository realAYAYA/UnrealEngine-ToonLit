// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_WINHTTP

#include "CoreMinimal.h"
#include "GenericPlatform/HttpRequestImpl.h"
#include "Interfaces/IHttpResponse.h"
#include "IHttpThreadedRequest.h"

class FRequestPayload;
class FWinHttpHttpResponse;
class FWinHttpConnectionHttp;

using FStringKeyValueMap = TMap<FString, FString>;

class FWinHttpHttpRequest
	: public IHttpThreadedRequest
{
public:
	FWinHttpHttpRequest();
	virtual ~FWinHttpHttpRequest();

	//~ Begin IHttpBase Interface
	virtual FString GetURL() const override;
	virtual FString GetURLParameter(const FString& ParameterName) const override;
	virtual FString GetHeader(const FString& HeaderName) const override;
	virtual TArray<FString> GetAllHeaders() const override;	
	virtual FString GetContentType() const override;
	virtual int32 GetContentLength() const override;
	virtual const TArray<uint8>& GetContent() const override;
	//~ End IHttpBase Interface

	//~ Begin IHttpRequest Interface
	virtual FString GetVerb() const override;
	virtual void SetVerb(const FString& InVerb) override;
	virtual void SetURL(const FString& InURL) override;
	virtual void SetContent(const TArray<uint8>& ContentPayload) override;
	virtual void SetContent(TArray<uint8>&& ContentPayload) override;
	virtual void SetContentAsString(const FString& ContentString) override;
	virtual bool SetContentAsStreamedFile(const FString& Filename) override;
	virtual bool SetContentFromStream(TSharedRef<FArchive, ESPMode::ThreadSafe> Stream) override;
	virtual void SetHeader(const FString& HeaderName, const FString& HeaderValue) override;
	virtual void AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue) override;
	virtual bool ProcessRequest() override;
	virtual void CancelRequest() override;
	virtual EHttpRequestStatus::Type GetStatus() const override;
	virtual const FHttpResponsePtr GetResponse() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float GetElapsedTime() const override;
	//~ End IHttpRequest Interface

	//~ Begin IHttpRequestThreaded Interface
	/** Called on HTTP thread */
	virtual bool StartThreadedRequest() override;
	/** Called on HTTP thread */
	virtual bool IsThreadedRequestComplete() override;
	/** Called on HTTP thread */
	virtual void TickThreadedRequest(float DeltaSeconds) override;

	/** Called on Game thread */
	virtual void FinishRequest() override;
	//~ End IHttpRequestThreaded Interface

protected:
	void HandleDataTransferred(int32 BytesSent, int32 BytesReceived);
	void HandleHeaderReceived(const FString& HeaderKey, const FString& HeaderValue);
	void HandleRequestComplete(EHttpRequestStatus::Type CompletionStatusUpdate);

	void UpdateResponseBody(bool bForceResponseExist = false);

	void OnWinHttpRequestComplete();
private:
	struct FWinHttpHttpRequestData
	{
		/** */
		FString Url;

		/** */
		TMap<FString, FString> Headers;

		/** */
		FString ContentType;

		/** */
		FString Verb;

		/** Payload to use with the request. Typically for POST, PUT, or PATCH */
		TSharedPtr<FRequestPayload, ESPMode::ThreadSafe> Payload;
	} RequestData;

	/** */
	bool bRequestCancelled = false;

	/** The time this request was started, or unset if there is no request in progress */
	TOptional<double> RequestStartTimeSeconds;

	TOptional<double> RequestFinishTimeSeconds;

	/** Current status of request being processed */
	EHttpRequestStatus::Type State = EHttpRequestStatus::NotStarted;

	/** Status of request available via GetStatus */
	EHttpRequestStatus::Type CompletionStatus = EHttpRequestStatus::NotStarted;

	/** */
	TSharedPtr<FWinHttpConnectionHttp, ESPMode::ThreadSafe> Connection;

	/** */
	TSharedPtr<FWinHttpHttpResponse, ESPMode::ThreadSafe> Response;

	/** */
	int32 TotalBytesSent = 0;

	/** */
	int32 TotalBytesReceived = 0;
};

#endif // WITH_WINHTTP
