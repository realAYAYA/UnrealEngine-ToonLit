// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/HttpRequestImpl.h"

/** 
  * Adapter class for IHttpRequest abstract interface
  * does not fully expose the wrapped interface in the base. This allows client defined marshalling of the requests when end point permissions are at issue.
  */

class FHttpRequestAdapterBase : public FHttpRequestImpl
{
public:
    FHttpRequestAdapterBase(const TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& InHttpRequest) 
		: HttpRequest(InHttpRequest)
    {}

	// IHttpRequest interface
    virtual FString                       GetURL() const override                                                  { return HttpRequest->GetURL(); }
	virtual FString                       GetURLParameter(const FString& ParameterName) const override             { return HttpRequest->GetURLParameter(ParameterName); }
	virtual FString                       GetHeader(const FString& HeaderName) const override                      { return HttpRequest->GetHeader(HeaderName); }
	virtual TArray<FString>               GetAllHeaders() const override                                           { return HttpRequest->GetAllHeaders(); }
	virtual FString                       GetContentType() const override                                          { return HttpRequest->GetContentType(); }
	virtual int32                         GetContentLength() const override                                        { return HttpRequest->GetContentLength(); }
	virtual const TArray<uint8>&          GetContent() const override                                              { return HttpRequest->GetContent(); }
	virtual FString                       GetVerb() const override                                                 { return HttpRequest->GetVerb(); }
	virtual void                          SetVerb(const FString& Verb) override                                    { HttpRequest->SetVerb(Verb); }
	virtual void                          SetURL(const FString& URL) override                                      { HttpRequest->SetURL(URL); }
	virtual void                          SetContent(const TArray<uint8>& ContentPayload) override                 { HttpRequest->SetContent(ContentPayload); }
	virtual void                          SetContent(TArray<uint8>&& ContentPayload) override                      { HttpRequest->SetContent(MoveTemp(ContentPayload)); }
	virtual void                          SetContentAsString(const FString& ContentString) override                { HttpRequest->SetContentAsString(ContentString); }
    virtual bool                          SetContentAsStreamedFile(const FString& Filename) override               { return HttpRequest->SetContentAsStreamedFile(Filename); }
	virtual bool                          SetContentFromStream(TSharedRef<FArchive, ESPMode::ThreadSafe> Stream) override { return HttpRequest->SetContentFromStream(Stream); }
	virtual void                          SetHeader(const FString& HeaderName, const FString& HeaderValue) override { HttpRequest->SetHeader(HeaderName, HeaderValue); }
	virtual void                          AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue) override { HttpRequest->AppendToHeader(HeaderName, AdditionalHeaderValue); }
	virtual void                          SetTimeout(float InTimeoutSecs) override                                 { HttpRequest->SetTimeout(InTimeoutSecs); }
	virtual void                          ClearTimeout() override                                                  { HttpRequest->ClearTimeout(); }
	virtual TOptional<float>              GetTimeout() const override                                              { return HttpRequest->GetTimeout(); }
	virtual const FHttpResponsePtr        GetResponse() const override                                             { return HttpRequest->GetResponse(); }
	virtual float                         GetElapsedTime() const override                                          { return HttpRequest->GetElapsedTime(); }
	virtual EHttpRequestStatus::Type	  GetStatus() const override                                               { return HttpRequest->GetStatus(); }
	virtual void                          Tick(float DeltaSeconds) override                                        { HttpRequest->Tick(DeltaSeconds); }

protected:
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest;
};

