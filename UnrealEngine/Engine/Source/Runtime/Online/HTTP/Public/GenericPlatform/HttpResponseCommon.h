// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/IHttpResponse.h"

class FHttpRequestCommon;

/**
 * Contains implementation of some common functions that don't vary between implementations of different platforms
 */
class FHttpResponseCommon : public IHttpResponse
{
	friend FHttpRequestCommon;

public:
	HTTP_API FHttpResponseCommon(const FHttpRequestCommon& HttpRequest);

	// IHttpBase
	HTTP_API virtual FString GetURLParameter(const FString& ParameterName) const override;
	HTTP_API virtual FString GetURL() const override;
	HTTP_API virtual const FString& GetEffectiveURL() const override;
	HTTP_API virtual EHttpRequestStatus::Type GetStatus() const override;
	HTTP_API virtual EHttpFailureReason GetFailureReason() const override;

protected:
	HTTP_API void SetRequestStatus(EHttpRequestStatus::Type InCompletionStatus);
	HTTP_API void SetRequestFailureReason(EHttpFailureReason InFailureReason);
	HTTP_API void SetEffectiveURL(const FString& InEffectiveURL);

	FString URL;
	FString EffectiveURL;
	EHttpRequestStatus::Type CompletionStatus;
	EHttpFailureReason FailureReason;
};
