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
class BACKGROUNDHTTP_API FBackgroundHttpRequestImpl 
	: public IBackgroundHttpRequest
{
public:
	FBackgroundHttpRequestImpl();
	virtual ~FBackgroundHttpRequestImpl() {}

	//This should be called from the platform level when a BG download finishes.
	virtual void OnBackgroundDownloadComplete();

	// IBackgroundHttpRequest
	virtual bool ProcessRequest() override;
	virtual void CancelRequest() override;
    virtual void PauseRequest() override;
    virtual void ResumeRequest() override;
    virtual void SetURLAsList(const TArray<FString>& URLs, int NumRetriesToAttempt) override;
	virtual const TArray<FString>& GetURLList() const override;
	virtual void SetCompleteNotification(FBackgroundHttpNotificationObjectPtr DownloadCompleteNotificationObjectIn) override;
	virtual void CompleteWithExistingResponseData(FBackgroundHttpResponsePtr BackgroundResponse) override;
	virtual FBackgroundHttpRequestCompleteDelegate& OnProcessRequestComplete() override;
	virtual FBackgroundHttpProgressUpdateDelegate& OnProgressUpdated() override;
	virtual const FBackgroundHttpResponsePtr GetResponse() const override;
	virtual const FString& GetRequestID() const override;
	virtual void SetRequestID(const FString& NewRequestID) override;
	virtual bool HandleDelayedProcess() override;
	virtual EBackgroundHTTPPriority GetRequestPriority() const override;
	virtual void SetRequestPriority(EBackgroundHTTPPriority NewPriority) override;
	
	virtual void NotifyNotificationObjectOfComplete(bool bWasSuccess);
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
