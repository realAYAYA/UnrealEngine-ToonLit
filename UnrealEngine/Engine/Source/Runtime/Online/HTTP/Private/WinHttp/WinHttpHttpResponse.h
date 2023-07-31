// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_WINHTTP

#include "CoreMinimal.h"
#include "Interfaces/IHttpResponse.h"
#include "Containers/Queue.h"

/**
 * WinHttp implementation of an HTTP response
 */
class FWinHttpHttpResponse : public IHttpResponse
{
public:
	FWinHttpHttpResponse(const FString& InUrl, const EHttpResponseCodes::Type InHttpStatusCode, TMap<FString, FString>&& InHeaders, TArray<uint8>&& InPayload);
	virtual ~FWinHttpHttpResponse() = default;

	//~ Begin IHttpBase Interface
	virtual FString GetURL() const override;
	virtual FString GetURLParameter(const FString& ParameterName) const override;
	virtual FString GetHeader(const FString& HeaderName) const override;
	virtual TArray<FString> GetAllHeaders() const override;	
	virtual FString GetContentType() const override;
	virtual int32 GetContentLength() const override;
	virtual const TArray<uint8>& GetContent() const override;
	//~ End IHttpBase Interface

	//~ Begin IHttpResponse Interface
	virtual int32 GetResponseCode() const override;
	virtual FString GetContentAsString() const override;
	//~ End IHttpResponse Interface

	void AppendHeader(const FString& HeaderKey, const FString& HeaderValue) { Headers.Add(HeaderKey, HeaderValue); }
	void AppendPayload(const TArray<uint8>& InPayload) { Payload.Append(InPayload); }

protected:
	/** The URL we requested data from*/
	FString Url;
	/** Cached code from completed response */
	EHttpResponseCodes::Type HttpStatusCode;
	/** Cached key/value header pairs. Parsed once request completes. Only accessible on the game thread. */
	TMap<FString, FString> Headers;
	/** Byte array of the data we received */
	TArray<uint8> Payload;
};

#endif // WITH_WINHTTP
