// Copyright Epic Games, Inc. All Rights Reserved.

#include "HttpRequestProxyObject.h"

#include "HttpBlueprintFunctionLibrary.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"

UHttpRequestProxyObject* UHttpRequestProxyObject::CreateHttpRequestProxyObject(
	const FString& InUrl,
	const FString& InVerb,
	FHttpHeader InHeader,
	const FString& InBody)
{
	UHttpRequestProxyObject* const Proxy = NewObject<UHttpRequestProxyObject>();
	Proxy->SetFlags(RF_StrongRefOnFrame);
	Proxy->CachedHeader = InHeader;
	Proxy->ProcessRequest(InUrl, InVerb, MoveTemp(InHeader), InBody);
	return Proxy;
}

void UHttpRequestProxyObject::ProcessRequest(
	const FString& InUrl,
	const FString& InVerb,
	FHttpHeader&& InHeader,
	const FString& InBody)
{
	FHttpModule& HttpModule = FHttpModule::Get();
	const TSharedRef<IHttpRequest> Request = HttpModule.CreateRequest();
	Request->SetURL(InUrl);
	Request->SetVerb(InVerb);
	Request->SetContentAsString(InBody);
	InHeader.AssignHeadersToRequest(Request);
	Request->ProcessRequest();

	Request->OnProcessRequestComplete().BindUObject(this, &ThisClass::ProcessComplete);
}

void UHttpRequestProxyObject::ProcessComplete(FHttpRequestPtr InRequest, FHttpResponsePtr InResponse, bool bInSuccessful)
{
	OnRequestComplete.Broadcast(InResponse->GetContentAsString(), bInSuccessful, MoveTemp(CachedHeader));
}
