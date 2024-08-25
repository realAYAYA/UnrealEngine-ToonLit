// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/HttpRequestImpl.h"
#include "Interfaces/IHttpResponse.h"

/**
 * Null (mock) implementation of an HTTP request
 */
class FNullHttpRequest : public FHttpRequestImpl
{
public:

	// IHttpBase
	virtual FString GetURL() const override;
	virtual FString GetURLParameter(const FString& ParameterName) const override;
	virtual FString GetHeader(const FString& HeaderName) const override;
	virtual TArray<FString> GetAllHeaders() const override;	
	virtual FString GetContentType() const override;
	virtual uint64 GetContentLength() const override;
	virtual const TArray<uint8>& GetContent() const override;
	// IHttpRequest 
	virtual FString GetVerb() const override;
	virtual void SetVerb(const FString& InVerb) override;
	virtual void SetURL(const FString& InURL) override;
	virtual void SetContent(const TArray<uint8>& ContentPayload) override;
	virtual void SetContent(TArray<uint8>&& ContentPayload) override;
	virtual void SetContentAsString(const FString& ContentString) override;
    virtual bool SetContentAsStreamedFile(const FString& Filename) override;
	virtual bool SetContentFromStream(TSharedRef<FArchive, ESPMode::ThreadSafe> Stream) override;
	virtual bool SetResponseBodyReceiveStream(TSharedRef<FArchive> Stream) override;
	virtual void SetHeader(const FString& HeaderName, const FString& HeaderValue) override;
	virtual void AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue) override;
	virtual bool ProcessRequest() override;
	virtual void CancelRequest() override;
	virtual EHttpRequestStatus::Type GetStatus() const override;
	virtual EHttpFailureReason GetFailureReason() const override;
	virtual const FString& GetEffectiveURL() const override;
	virtual const FHttpResponsePtr GetResponse() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float GetElapsedTime() const override;
	virtual void SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy InThreadPolicy) override;
	virtual EHttpRequestDelegateThreadPolicy GetDelegateThreadPolicy() const override;
	virtual void SetTimeout(float InTimeoutSecs) override;
	virtual void ClearTimeout() override;
	virtual TOptional<float> GetTimeout() const override;
	virtual void SetActivityTimeout(float InTimeoutSecs) override;
	virtual void ProcessRequestUntilComplete() override;

	FNullHttpRequest()
		: CompletionStatus(EHttpRequestStatus::NotStarted)
		, FailureReason(EHttpFailureReason::None)
		, ElapsedTime(0)
	{}
	virtual ~FNullHttpRequest() {}

private:
	void FinishedRequest();

	FString Url;
	FString EffectiveUrl;
	FString Verb;
	TArray<uint8> Payload;
	EHttpRequestStatus::Type CompletionStatus;
	EHttpFailureReason FailureReason;
	TMap<FString, FString> Headers;
	float ElapsedTime;
	TOptional<float> TimeoutSecs;
};

/**
 * Null (mock) implementation of an HTTP request
 */
class FNullHttpResponse : public IHttpResponse
{
	// IHttpBase 
	virtual FString GetURL() const override;
	virtual FString GetURLParameter(const FString& ParameterName) const override;
	virtual FString GetHeader(const FString& HeaderName) const override;
	virtual TArray<FString> GetAllHeaders() const override;	
	virtual FString GetContentType() const override;
	virtual uint64 GetContentLength() const override;
	virtual const TArray<uint8>& GetContent() const override;
	//~ Begin IHttpResponse Interface
	virtual int32 GetResponseCode() const override;
	virtual FString GetContentAsString() const override;

	FNullHttpResponse() {}
	virtual ~FNullHttpResponse() {}

private:
	TArray<uint8> Payload;
};
