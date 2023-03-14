// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"

#if PLATFORM_HOLOLENS

#include "HttpIXMLSupport.h"
#include "Interfaces/IHttpResponse.h"
#include "GenericPlatform/HttpRequestImpl.h"
#include "GenericPlatform/HttpRequestPayload.h"

// Default user agent string
static const WCHAR USER_AGENT[] = L"UEHTTPIXML\r\n";


/**
 * IXML implementation of an Http request
 */
class FHttpRequestIXML : public FHttpRequestImpl
{
public:

	// IHttpBase

	virtual FString GetURL() const override;
	virtual FString GetURLParameter(const FString& ParameterName) const override;
	virtual FString GetHeader(const FString& HeaderName) const override;
	virtual TArray<FString> GetAllHeaders() const override;
	virtual FString GetContentType() const override;
	virtual int32 GetContentLength() const override;
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
	virtual void SetHeader(const FString& HeaderName, const FString& HeaderValue) override;
	virtual void AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue) override;
	virtual bool ProcessRequest() override;
	virtual void CancelRequest() override;
	virtual EHttpRequestStatus::Type GetStatus() const override;
	virtual const FHttpResponsePtr GetResponse() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float GetElapsedTime() const override;

	/**
	 * Constructor
	 */
	FHttpRequestIXML();

	/**
	 * Destructor. Clean up any connection/request handles
	 */
	virtual ~FHttpRequestIXML();

private:

	uint32		CreateRequest();
	uint32		ApplyHeaders();
	uint32		SendRequest();
	void		FinishedRequest();
	void		CleanupRequest();

private:
	TMap<FString, FString>				Headers;
	TUniquePtr<FRequestPayload>			Payload;
	FString								URL;
	FString								Verb;

	EHttpRequestStatus::Type			RequestStatus;
	float								ElapsedTime;

	ComPtr<IXMLHTTPRequest2>			XHR;
	ComPtr<IXMLHTTPRequest2Callback>	XHRCallback;
	ComPtr<HttpCallback>				HttpCB;
	ComPtr<RequestStream>				SendStream;

	TSharedPtr<class FHttpResponseIXML, ESPMode::ThreadSafe> Response;

	friend class FHttpResponseIXML;
};

/**
 * IXML implementation of an Http response
 */
class FHttpResponseIXML : public IHttpResponse
{

public:

	// IHttpBase

	virtual FString GetURL() const override;
	virtual FString GetURLParameter(const FString& ParameterName) const override;
	virtual FString GetHeader(const FString& HeaderName) const override;
	virtual TArray<FString> GetAllHeaders() const override;
	virtual FString GetContentType() const override;
	virtual int32 GetContentLength() const override;
	virtual const TArray<uint8>& GetContent() const override;

	// IHttpResponse

	virtual int32 GetResponseCode() const override;
	virtual FString GetContentAsString() const override;

	// FHttpResponseIXML

	FHttpResponseIXML(FHttpRequestIXML& InRequest, ComPtr<HttpCallback> InHttpCB);
	virtual ~FHttpResponseIXML();

	bool	Succeeded();

private:

	FHttpRequestIXML&				Request;
	FString							RequestURL;
	ComPtr<HttpCallback>			HttpCB;

};

#endif
