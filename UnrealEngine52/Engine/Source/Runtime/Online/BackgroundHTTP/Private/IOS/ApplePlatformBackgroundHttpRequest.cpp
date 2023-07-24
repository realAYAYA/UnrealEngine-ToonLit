// Copyright Epic Games, Inc. All Rights Reserved.

#include "IOS/ApplePlatformBackgroundHttpRequest.h"
#include "IOS/ApplePlatformBackgroundHttpManager.h"
#include "Interfaces/IBackgroundHttpResponse.h"
#include "BackgroundHttpModule.h"

FApplePlatformBackgroundHttpRequest::FApplePlatformBackgroundHttpRequest()
    : CompletedTempDownloadLocation()
    , ActiveTimeOutTimer(30.f)
    , RetryCount(0)
	, ResumeDataRetryCount(0)
    , FirstTask(nullptr)
	, FirstTaskIdentifier(0)
    , CombinedRequestID("")
    , bIsTaskActive(false)
    , bIsTaskPaused(false)
    , bIsCompleted(false)
    , bIsFailed(false)
	, bIsRequestSwitchingTasks(false)
    , bWasTaskStartedInBG(false)
    , bHasAlreadyFinishedRequest(false)
    , bIsPendingCancel(false)
    , DownloadProgress(0)
    , DownloadProgressSinceLastUpdateSent(0)
{    
}

void FApplePlatformBackgroundHttpRequest::CompleteWithExistingResponseData(FBackgroundHttpResponsePtr BackgroundResponse)
{
    if (ensureAlwaysMsgf(BackgroundResponse.IsValid(), TEXT("Call to CompleteWithExistingResponseData with an invalid response!")))
    {
        FBackgroundHttpRequestImpl::CompleteWithExistingResponseData(BackgroundResponse);
        CompleteRequest_Internal(true, BackgroundResponse->GetTempContentFilePath());
    }
}

void FApplePlatformBackgroundHttpRequest::SetRequestAsSuccess(const FString& CompletedTempDownloadLocationIn)
{
    CompleteRequest_Internal(true, CompletedTempDownloadLocationIn);
}

void FApplePlatformBackgroundHttpRequest::SetRequestAsFailed()
{
    CompleteRequest_Internal(false, FString());
}

void FApplePlatformBackgroundHttpRequest::CompleteRequest_Internal(bool bWasRequestSuccess, const FString& CompletedTempDownloadLocationIn)
{
    UE_LOG(LogBackgroundHttpRequest, Display, TEXT("Marking Request Complete -- RequestDebugID:%s | bWasRequestSuccess:%d | CompletedTempDownloadLocation:%s"), *GetRequestDebugID(), (int)bWasRequestSuccess, *CompletedTempDownloadLocationIn);
    
	//Purposefully avoid setting bIsTaskActive to false here as we still expect that to be set until the request is deleted.
	
	FPlatformAtomics::InterlockedExchange(&bIsCompleted, true);
	FPlatformAtomics::InterlockedExchange(&bIsFailed, !bWasRequestSuccess);
    FPlatformAtomics::InterlockedExchange(&bIsPendingCancel, false);
    
    if (!CompletedTempDownloadLocationIn.IsEmpty())
    {
        CompletedTempDownloadLocation = CompletedTempDownloadLocationIn;
    }
    
	NotifyNotificationObjectOfComplete(bWasRequestSuccess);
}

const FString& FApplePlatformBackgroundHttpRequest::GetURLForRetry(bool bShouldIncrementRetryCountFirst)
{
	const int NewRetryCount = bShouldIncrementRetryCountFirst ? RetryCount.Increment() : RetryCount.GetValue();
	
	//If we are out of Retries, just send an empty string
	if (NewRetryCount > NumberOfTotalRetries)
	{
		UE_LOG(LogBackgroundHttpRequest, Display, TEXT("GetURLForRetry is out of Retries for Request -- RequestDebugID:%s"), *GetRequestDebugID());
        
        static FString EmptyResponse = TEXT("");
        return EmptyResponse;
	}
	//Still have remaining retries
	else
	{
		const int URLIndex = NewRetryCount % URLList.Num();
		const FString& URLToReturn = URLList[URLIndex];

		UE_LOG(LogBackgroundHttpRequest, Display, TEXT("GetURLForRetry found valid URL for current retry -- RequestDebugID:%s | NewRetryCount:%d | URLToReturn:%s"), *GetRequestDebugID(), NewRetryCount, *URLToReturn);
        return URLToReturn;
    }
}

void FApplePlatformBackgroundHttpRequest::ResetProgressTracking()
{
    FPlatformAtomics::InterlockedExchange(&DownloadProgress, 0);
}

void FApplePlatformBackgroundHttpRequest::ActivateUnderlyingTask()
{
    volatile FTaskNode* ActiveTaskNode = FirstTask;
    if (ensureAlwaysMsgf((nullptr != ActiveTaskNode), TEXT("Call to ActivateUnderlyingTask with an invalid node! Need to create underlying task(and node) before activating!")))
    {
        NSURLSessionTask* UnderlyingTask = ActiveTaskNode->OurTask;
        if (ensureAlwaysMsgf((nullptr != UnderlyingTask), TEXT("Call to ActivateUnderlyingTask with an invalid task! Need to create underlying task before activating!")))
        {
            FString TaskURL = [[[UnderlyingTask currentRequest] URL] absoluteString];
            int TaskIdentifier = (int)[UnderlyingTask taskIdentifier];
            
            UE_LOG(LogBackgroundHttpRequest, Display, TEXT("Activating Task for Request -- RequestDebugID:%s | TaskIdentifier:%d | TaskURL:%s"), *GetRequestDebugID(), TaskIdentifier, *TaskURL);
        
            FPlatformAtomics::InterlockedExchange(&bIsTaskActive, true);
            FPlatformAtomics::InterlockedExchange(&bIsTaskPaused, false);
            FPlatformAtomics::InterlockedExchange(&bIsPendingCancel, false);
            
            [UnderlyingTask resume];
            
            ResetTimeOutTimer();
            ResetProgressTracking();
        }
    }
}

void FApplePlatformBackgroundHttpRequest::PauseUnderlyingTask()
{
    volatile FTaskNode* ActiveTaskNode = FirstTask;
    if (ensureAlwaysMsgf((nullptr != ActiveTaskNode), TEXT("Call to PauseUnderlyingTask with an invalid node! Need to create underlying task(and node) before trying to pause!")))
    {
        NSURLSessionTask* UnderlyingTask = ActiveTaskNode->OurTask;
        if (ensureAlwaysMsgf((nullptr != UnderlyingTask), TEXT("Call to PauseUnderlyingTask with an invalid task! Need to create underlying task before trying to pause!")))
        {
            FString TaskURL = [[[UnderlyingTask currentRequest] URL] absoluteString];
            int TaskIdentifier = (int)[UnderlyingTask taskIdentifier];
            
            UE_LOG(LogBackgroundHttpRequest, Display, TEXT("Pausing Task for Request -- RequestDebugID:%s | TaskIdentifier:%d | TaskURL:%s"), *GetRequestDebugID(), TaskIdentifier, *TaskURL);
            
            FPlatformAtomics::InterlockedExchange(&bIsPendingCancel, false);
            FPlatformAtomics::InterlockedExchange(&bIsTaskPaused, true);
            
            [UnderlyingTask suspend];
            
            ResetTimeOutTimer();
            ResetProgressTracking();
        }
    }
}

bool FApplePlatformBackgroundHttpRequest::IsUnderlyingTaskActive()
{
    return FPlatformAtomics::AtomicRead(&bIsTaskActive);
}

bool FApplePlatformBackgroundHttpRequest::IsUnderlyingTaskPaused()
{
    return FPlatformAtomics::AtomicRead(&bIsTaskPaused);
}

bool FApplePlatformBackgroundHttpRequest::TickTimeOutTimer(float DeltaTime)
{
    ActiveTimeOutTimer -= DeltaTime;
    return (ActiveTimeOutTimer <= 0.f) ? true : false;
}

void FApplePlatformBackgroundHttpRequest::ResetTimeOutTimer()
{
    ActiveTimeOutTimer = FApplePlatformBackgroundHttpManager::ActiveTimeOutSetting;
}

bool FApplePlatformBackgroundHttpRequest::AssociateWithTask(NSURLSessionTask* ExistingTask)
{
	bool bDidAssociate = false;

	if (ensureAlwaysMsgf((nullptr != ExistingTask), TEXT("Call to AssociateWithTask with an invalid Task! RequestDebugID:%s"), *GetRequestDebugID()))
	{
		const FString TaskURL = [[[ExistingTask currentRequest] URL] absoluteString];
        int TaskIdentifier = (int)[ExistingTask taskIdentifier];
        
		const bool bWasAlreadySwitching = FPlatformAtomics::InterlockedExchange(&bIsRequestSwitchingTasks, true);
		if (!bWasAlreadySwitching)
		{
			volatile FTaskNode* NewNode = new FTaskNode();
			NewNode->OurTask = ExistingTask;

			//Add a count to our task's reference list so it doesn't get deleted while in our Request's task list
			[ExistingTask retain];

			//Swap our new node and the first one in the list
			NewNode->NextNode = (FTaskNode*)FPlatformAtomics::InterlockedExchangePtr((void**)(&FirstTask), (void*)NewNode);

			//Save off our first task's identifier and our CombinedRequestID that includes it
			FirstTaskIdentifier = (int)[(FirstTask->OurTask) taskIdentifier];
            CombinedRequestID = FString::Printf(TEXT("%d.%s"), FirstTaskIdentifier, *RequestID);
            
            UE_LOG(LogBackgroundHttpRequest, Display, TEXT("Associated Request With New Task -- RequestDebugID:%s | TaskIdentifier:%d | TaskURL:%s"), *GetRequestDebugID(), TaskIdentifier, *TaskURL);

			FPlatformAtomics::InterlockedExchange(&bIsPendingCancel, false);

			ResetTimeOutTimer();
			ResetProgressTracking();

			bDidAssociate = true;
            FPlatformAtomics::InterlockedExchange(&bIsRequestSwitchingTasks, false);
		}
		else
		{
            UE_LOG(LogBackgroundHttpRequest, Display, TEXT("Failed to Associate Request with new Task as there was already a pending AssociateWithTask running! -- RequestDebugID:%s | TaskIdentifier:%d | TaskURL:%s"), *GetRequestDebugID(), TaskIdentifier, *TaskURL);
		}
	}

	return bDidAssociate;
}

void FApplePlatformBackgroundHttpRequest::PauseRequest()
{
    PauseUnderlyingTask();
}

void FApplePlatformBackgroundHttpRequest::ResumeRequest()
{
	FPlatformAtomics::InterlockedExchange(&bIsTaskPaused, false);
	
	//We only want to re-activate tasks that have already been flagged as active.
	//Otherwise let our BackgroundHTTP Manager handle activating us on a tick now that we aren't paused.
	if (IsUnderlyingTaskActive())
	{
		ActivateUnderlyingTask();
	}
	else
	{
		UE_LOG(LogBackgroundHttpRequest, Display, TEXT("ResumeRequest called on a task that wasn't active -- RequestDebugID:%s"), *GetRequestDebugID());
	}
}

void FApplePlatformBackgroundHttpRequest::CancelActiveTask()
{
    volatile FTaskNode* TaskNodeWeAreCancelling = FirstTask;
	if (nullptr != TaskNodeWeAreCancelling)
	{
        if (nullptr != TaskNodeWeAreCancelling->OurTask)
        {
            FString TaskURL = [[[TaskNodeWeAreCancelling->OurTask currentRequest] URL] absoluteString];
            int TaskIdentifier = (int)[TaskNodeWeAreCancelling->OurTask taskIdentifier];
            
            UE_LOG(LogBackgroundHttpRequest, Display, TEXT("Cancelling Task -- RequestDebugID:%s | TaskIdentifier:%d | TaskURL:%s"), *GetRequestDebugID(), TaskIdentifier, *TaskURL);
            
            FPlatformAtomics::InterlockedExchange(&bIsPendingCancel, true);
            
            [TaskNodeWeAreCancelling->OurTask cancel];
        }
	}
}

void FApplePlatformBackgroundHttpRequest::UpdateDownloadProgress(int64_t TotalDownloaded,int64_t DownloadedSinceLastUpdate)
{
    UE_LOG(LogBackgroundHttpRequest, VeryVerbose, TEXT("Request Update Progress -- RequestDebugID:%s | OldProgress:%lld | NewProgress:%lld | ProgressSinceLastUpdate:%lld"), *GetRequestDebugID(), DownloadProgress, TotalDownloaded, DownloadedSinceLastUpdate);
	
    FPlatformAtomics::AtomicStore(&DownloadProgress, TotalDownloaded);
    FPlatformAtomics::InterlockedAdd(&DownloadProgressSinceLastUpdateSent, DownloadedSinceLastUpdate);
    
    ResetTimeOutTimer();
}

void FApplePlatformBackgroundHttpRequest::SendDownloadProgressUpdate()
{
    volatile int64 DownloadProgressCopy = FPlatformAtomics::AtomicRead(&DownloadProgress);
    
    //Don't send any updates if we haven't updated anything since we last sent an update
    if (DownloadProgressCopy > 0)
    {
        //Reset our DownloadProgressSinceLastUpdateSent to 0 now that we are sending a progress update
        volatile int64 DownloadProgressSinceLastUpdateSentCopy = FPlatformAtomics::InterlockedExchange(&DownloadProgressSinceLastUpdateSent, 0);
        
        OnProgressUpdated().ExecuteIfBound(SharedThis(this), DownloadProgressCopy, DownloadProgressSinceLastUpdateSentCopy);
    }
}

const FString& FApplePlatformBackgroundHttpRequest::GetRequestDebugID() const
{
    //We use CombinedRequestID to append a TaskIdentifier on the end of the RequestID. If we have one, use that. Otherwise fallback on what is already set
    return CombinedRequestID.IsEmpty() ? RequestID : CombinedRequestID;
}

bool FApplePlatformBackgroundHttpRequest::IsTaskComplete() const
{
    const bool bDidRequestFail = FPlatformAtomics::AtomicRead(&bIsFailed);
    const bool bDidRequestComplete = FPlatformAtomics::AtomicRead(&bIsCompleted);
    return (bDidRequestFail || bDidRequestComplete);
}
