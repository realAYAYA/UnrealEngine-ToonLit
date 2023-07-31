// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_WINHTTP

#include "WinHttp/WinHttpHttpResponse.h"
#include "Http.h"

FWinHttpHttpResponse::FWinHttpHttpResponse(const FString& InUrl, const EHttpResponseCodes::Type InHttpStatusCode, TMap<FString, FString>&& InHeaders, TArray<uint8>&& InPayload)
	: Url(InUrl)
	, HttpStatusCode(InHttpStatusCode)
	, Headers(MoveTemp(InHeaders))
	, Payload(MoveTemp(InPayload))
{
}

FString FWinHttpHttpResponse::GetURL() const
{
	return Url;
}

FString FWinHttpHttpResponse::GetURLParameter(const FString& ParameterName) const
{
	FString ReturnValue;
	if (TOptional<FString> OptionalParameterValue = FGenericPlatformHttp::GetUrlParameter(Url, ParameterName))
	{
		ReturnValue = MoveTemp(OptionalParameterValue.GetValue());
	}
	return ReturnValue;
}

FString FWinHttpHttpResponse::GetHeader(const FString& HeaderName) const
{
	FString Result;
	if (const FString* Header = Headers.Find(HeaderName))
	{
		Result = *Header;
	}
	return Result;
}

TArray<FString> FWinHttpHttpResponse::GetAllHeaders() const
{
	TArray<FString> Result;
	Result.Reserve(Headers.Num());
	for (const TPair<FString, FString>& It : Headers)
	{
		Result.Emplace(FString::Printf(TEXT("%s: %s"), *It.Key, *It.Value));
	}
	return Result;
}

FString FWinHttpHttpResponse::GetContentType() const
{
	return GetHeader(TEXT("Content-Type"));
}

int32 FWinHttpHttpResponse::GetContentLength() const
{
	return Payload.Num();
}

const TArray<uint8>& FWinHttpHttpResponse::GetContent() const
{
	return Payload;
}

int32 FWinHttpHttpResponse::GetResponseCode() const
{
	return HttpStatusCode;
}

FString FWinHttpHttpResponse::GetContentAsString() const
{
	// Content is NOT null-terminated; we need to specify lengths here
	FUTF8ToTCHAR TCHARData(reinterpret_cast<const ANSICHAR*>(Payload.GetData()), Payload.Num());
	return FString(TCHARData.Length(), TCHARData.Get());
}

#endif // WITH_WINHTTP
