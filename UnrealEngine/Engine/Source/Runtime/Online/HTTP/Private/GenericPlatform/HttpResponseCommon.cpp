// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/HttpResponseCommon.h"
#include "GenericPlatform/HttpRequestCommon.h"
#include "GenericPlatform/GenericPlatformHttp.h"

FHttpResponseCommon::FHttpResponseCommon(const FHttpRequestCommon& HttpRequest)
	: URL(HttpRequest.GetURL())
	, EffectiveURL(HttpRequest.GetEffectiveURL())
	, CompletionStatus(HttpRequest.GetStatus())
	, FailureReason(HttpRequest.GetFailureReason())
{
}

FString FHttpResponseCommon::GetURLParameter(const FString& ParameterName) const
{
	FString ReturnValue;
	if (TOptional<FString> OptionalParameterValue = FGenericPlatformHttp::GetUrlParameter(URL, ParameterName))
	{
		ReturnValue = MoveTemp(OptionalParameterValue.GetValue());
	}
	return ReturnValue;
}

FString FHttpResponseCommon::GetURL() const
{
	return URL;
}

const FString& FHttpResponseCommon::GetEffectiveURL() const
{
	return EffectiveURL;
}

void FHttpResponseCommon::SetRequestStatus(EHttpRequestStatus::Type InCompletionStatus)
{
	CompletionStatus = InCompletionStatus;
}

EHttpRequestStatus::Type FHttpResponseCommon::GetStatus() const
{
	return CompletionStatus;
}

void FHttpResponseCommon::SetRequestFailureReason(EHttpFailureReason InFailureReason)
{
	FailureReason = InFailureReason;
}

EHttpFailureReason FHttpResponseCommon::GetFailureReason() const
{
	return FailureReason;
}

void FHttpResponseCommon::SetEffectiveURL(const FString& InEffectiveURL)
{
	EffectiveURL = InEffectiveURL;
}
