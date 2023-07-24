// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/IBackgroundHttpRequest.h"
#include "Interfaces/IHttpRequest.h"

#include "BackgroundHttpRequestImpl.h"

/**
 * Contains implementation of Apple specific background http requests
 */
class BACKGROUNDHTTP_API FApplePlatformBackgroundHttpRequest 
	: public FBackgroundHttpRequestImpl
{
public:
	FApplePlatformBackgroundHttpRequest();
	virtual ~FApplePlatformBackgroundHttpRequest() {}

	virtual void CompleteWithExistingResponseData(FBackgroundHttpResponsePtr BackgroundResponse) override;
    virtual void PauseRequest() override;
    virtual void ResumeRequest() override;
    
    virtual bool IsTaskComplete() const;

    //Used to provide some extra debug information over normal GetRequestID()
    //Returns string in format of X.Y where X is the underlying Task Identifier if set and Y is what was set in the SetRequestID() call if this reqeust has associated with a task.
    //Returns the same as GetRequestID() if no task has been associated yet.
	const FString& GetRequestDebugID() const;

private:
    //Super simple linked list with volatile pointers to next element
    struct FTaskNode
    {
        NSURLSessionTask* OurTask;
        volatile FTaskNode* NextNode;
    };
    
private:
    const FString& GetURLForRetry(bool bShouldIncrementRetryCountFirst);
    bool AssociateWithTask(NSURLSessionTask* ExistingTask);
	void SetRequestAsSuccess(const FString& CompletedTempDownloadLocation);
    void SetRequestAsFailed();
    void CompleteRequest_Internal(bool bWasRequestSuccess, const FString& CompletedTempDownloadLocation);
	void CancelActiveTask();
	
    //on iOS we need to delay when we send progress updates as they can come from a background thread
    //save off results in UpdateDownloadProgress to send later when we get a SendDownloadProgressUpdate
    void UpdateDownloadProgress(int64_t TotalDownloaded, int64_t DownloadedSinceLastUpdate);
    void SendDownloadProgressUpdate();
    void ResetProgressTracking();
    
    void ActivateUnderlyingTask();
    bool IsUnderlyingTaskActive();
    void PauseUnderlyingTask();
    bool IsUnderlyingTaskPaused();
    bool TickTimeOutTimer(float DeltaTime);
    void ResetTimeOutTimer();
    
    FString CompletedTempDownloadLocation;
    volatile float ActiveTimeOutTimer;
    
	FThreadSafeCounter RetryCount;
	FThreadSafeCounter ResumeDataRetryCount;

	volatile FTaskNode* FirstTask;
	
    volatile int FirstTaskIdentifier;
    FString CombinedRequestID;

    volatile int32 bIsTaskActive;
    volatile int32 bIsTaskPaused;
	volatile int32 bIsCompleted;
	volatile int32 bIsFailed;
	volatile int32 bIsRequestSwitchingTasks;
	volatile int32 bWasTaskStartedInBG;
    volatile int32 bHasAlreadyFinishedRequest;
    volatile int32 bIsPendingCancel;
    
	volatile int64 DownloadProgress;
    volatile int64 DownloadProgressSinceLastUpdateSent;

    friend class FApplePlatformBackgroundHttpManager;
};

typedef TSharedPtr<class FApplePlatformBackgroundHttpRequest, ESPMode::ThreadSafe> FAppleBackgroundHttpRequestPtr;
