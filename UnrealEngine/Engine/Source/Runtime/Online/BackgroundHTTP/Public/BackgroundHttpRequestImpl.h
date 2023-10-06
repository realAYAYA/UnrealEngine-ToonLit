// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BackgroundHttpNotificationObject.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Interfaces/IBackgroundHttpRequest.h"
#include "Logging/LogMacros.h"
#include "Templates/SharedPointer.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBackgroundHttpRequest, Log, All)

/**
 * Contains implementation of some common functions that don't have to vary between implementation
 */
class FBackgroundHttpRequestImpl 
	: public IBackgroundHttpRequest
{
public:
	BACKGROUNDHTTP_API FBackgroundHttpRequestImpl();
	virtual ~FBackgroundHttpRequestImpl() {}

	//This should be called from the platform level when a BG download finishes.
	BACKGROUNDHTTP_API virtual void OnBackgroundDownloadComplete();

	// IBackgroundHttpRequest
	BACKGROUNDHTTP_API virtual bool ProcessRequest() override;
	BACKGROUNDHTTP_API virtual void CancelRequest() override;
    BACKGROUNDHTTP_API virtual void PauseRequest() override;
    BACKGROUNDHTTP_API virtual void ResumeRequest() override;
    BACKGROUNDHTTP_API virtual void SetURLAsList(const TArray<FString>& URLs, int NumRetriesToAttempt) override;
	BACKGROUNDHTTP_API virtual const TArray<FString>& GetURLList() const override;
	BACKGROUNDHTTP_API virtual void SetCompleteNotification(FBackgroundHttpNotificationObjectPtr DownloadCompleteNotificationObjectIn) override;
	BACKGROUNDHTTP_API virtual void CompleteWithExistingResponseData(FBackgroundHttpResponsePtr BackgroundResponse) override;
	BACKGROUNDHTTP_API virtual FBackgroundHttpRequestCompleteDelegate& OnProcessRequestComplete() override;
	BACKGROUNDHTTP_API virtual FBackgroundHttpProgressUpdateDelegate& OnProgressUpdated() override;
	BACKGROUNDHTTP_API virtual const FBackgroundHttpResponsePtr GetResponse() const override;
	BACKGROUNDHTTP_API virtual const FString& GetRequestID() const override;
	BACKGROUNDHTTP_API virtual void SetRequestID(const FString& NewRequestID) override;
	BACKGROUNDHTTP_API virtual bool HandleDelayedProcess() override;
	BACKGROUNDHTTP_API virtual EBackgroundHTTPPriority GetRequestPriority() const override;
	BACKGROUNDHTTP_API virtual void SetRequestPriority(EBackgroundHTTPPriority NewPriority) override;
	
	BACKGROUNDHTTP_API virtual void NotifyNotificationObjectOfComplete(bool bWasSuccess);
protected:
	TSharedPtr<FBackgroundHttpNotificationObject, ESPMode::ThreadSafe> DownloadCompleteNotificationObject;
	FBackgroundHttpResponsePtr Response;
	TArray<FString> URLList;
	FString RequestID;
	int NumberOfTotalRetries;
	EBackgroundHTTPPriority RequestPriority;
	
	FBackgroundHttpRequestCompleteDelegate HttpRequestCompleteDelegate;
	FBackgroundHttpProgressUpdateDelegate HttpProgressUpdateDelegate;
};
