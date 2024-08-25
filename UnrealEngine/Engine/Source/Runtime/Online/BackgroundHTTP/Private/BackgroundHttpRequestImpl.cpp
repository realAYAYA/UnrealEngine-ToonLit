// Copyright Epic Games, Inc. All Rights Reserved.

#include "BackgroundHttpRequestImpl.h"

#include "BackgroundHttpModule.h"
#include "Interfaces/IBackgroundHttpResponse.h"

DEFINE_LOG_CATEGORY(LogBackgroundHttpRequest);

FBackgroundHttpRequestImpl::FBackgroundHttpRequestImpl()
	: DownloadCompleteNotificationObject(nullptr)
	, Response(nullptr)
	, URLList()
	, RequestID()
	, NumberOfTotalRetries(0)
	, RequestPriority(EBackgroundHTTPPriority::Normal)
	, HttpRequestCompleteDelegate()
	, HttpProgressUpdateDelegate()
{
}

bool FBackgroundHttpRequestImpl::ProcessRequest()
{
	UE_LOG(LogBackgroundHttpRequest, Verbose, TEXT("Processing Request - RequestID:%s"), *GetRequestID());
	FBackgroundHttpModule::Get().GetBackgroundHttpManager()->AddRequest(SharedThis(this));

	return true;
}

void FBackgroundHttpRequestImpl::CancelRequest()
{
	UE_LOG(LogBackgroundHttpRequest, Display, TEXT("Cancelling Request - RequestID:%s"), *GetRequestID());
	FBackgroundHttpModule::Get().GetBackgroundHttpManager()->RemoveRequest(SharedThis(this));
}

void FBackgroundHttpRequestImpl::PauseRequest()
{
    //for now a pause is just wrapping a cancel in the general case
    UE_LOG(LogBackgroundHttpRequest, Display, TEXT("Pausing Request (through cancel) - RequestID:%s"), *GetRequestID());
    CancelRequest();
}

void FBackgroundHttpRequestImpl::ResumeRequest()
{
    //for now a resume is just wrapping a restart in the general case
    UE_LOG(LogBackgroundHttpRequest, Display, TEXT("Resuming Request (through restart) - RequestID:%s"), *GetRequestID());
    ProcessRequest();
}

void FBackgroundHttpRequestImpl::OnBackgroundDownloadComplete()
{
	//The complete delegate should only be firing on the game thread 
	//so that requestors don't have to worry about thread safety unexpectedly
	ensureAlwaysMsgf(IsInGameThread(), TEXT("Called from un-expected thread! Potential error in an implementation of background downloads!"));

	FBackgroundHttpModule::Get().GetBackgroundHttpManager()->RemoveRequest(SharedThis(this));

	//Determine if this was a success or not
	FBackgroundHttpResponsePtr SetResponse = GetResponse();
	const bool bWasSuccess = SetResponse.IsValid() ? EHttpResponseCodes::IsOk(SetResponse->GetResponseCode()) : false;
	const FString ResponseTempLocation = Response.IsValid() ? Response->GetTempContentFilePath() : TEXT("None");

	UE_LOG(LogBackgroundHttpRequest, Display, TEXT("Download Complete - RequestID:%s | bWasSuccess:%d | ResponseTempLocation:%s"), *GetRequestID(), (int)(bWasSuccess), *ResponseTempLocation);

	//First, send a delegate out for this request completing
	OnProcessRequestComplete().ExecuteIfBound(SharedThis(this), bWasSuccess);

	//Second notify our download complete notification of the success/failure of this request. Then remove our reference
	//so a notification will send without having to delete this request. Do this after Complete Delegate so we can ensure
	//whatever kicked this off doesn't want to react to this download complete before we send this notification (IE: Kick off another download
	//using the same NotificationObject, etc.)
	NotifyNotificationObjectOfComplete(bWasSuccess);
	
	//Now that we have called our completion delegates and done everything else for this request, allow us to clean up data for it
	FBackgroundHttpModule::Get().GetBackgroundHttpManager()->CleanUpDataAfterCompletingRequest(SharedThis(this));
}

void FBackgroundHttpRequestImpl::NotifyNotificationObjectOfComplete(bool bWasSuccess)
{	
	if (DownloadCompleteNotificationObject.IsValid())
	{
		const int32 SharedRefCount = DownloadCompleteNotificationObject.GetSharedReferenceCount();
		UE_LOG(LogBackgroundHttpRequest, VeryVerbose, TEXT("Removing Reference to DownloadCompleteNotificationObject - bWasSuccess:%d | CurrentSharedRefCount:%d"), (int)(bWasSuccess), SharedRefCount);

		if (SharedRefCount == 1)
		{
			UE_LOG(LogBackgroundHttpRequest, Display, TEXT("Removing Final Reference to DownloadCompleteNotificationObject!"));
		}

		DownloadCompleteNotificationObject->NotifyOfDownloadResult(bWasSuccess);
		DownloadCompleteNotificationObject.Reset();
	}
}

void FBackgroundHttpRequestImpl::SetURLAsList(const TArray<FString>& URLs, int NumRetriesIn)
{
	NumberOfTotalRetries = NumRetriesIn;
	for (const FString& URL : URLs)
	{
		URLList.Add(URL);
	}
}

const TArray<FString>& FBackgroundHttpRequestImpl::GetURLList() const
{
	return URLList;
}

void FBackgroundHttpRequestImpl::SetCompleteNotification(FBackgroundHttpNotificationObjectPtr DownloadCompleteNotificationObjectIn)
{
	DownloadCompleteNotificationObject = DownloadCompleteNotificationObjectIn;
}

void FBackgroundHttpRequestImpl::CompleteWithExistingResponseData(FBackgroundHttpResponsePtr BackgroundResponse)
{
	Response = BackgroundResponse;

	const bool bHasValidResponse = Response.IsValid();
	const FString ResponseTempLocation = Response.IsValid() ? Response->GetTempContentFilePath() : TEXT("None");

	UE_LOG(LogBackgroundHttpRequest, Verbose, TEXT("Completing Download With Existing Response Data - RequestID:%s | bHasValidResponse:%d | ResponseTempDownloadLocation:%s"), *GetRequestID(), (int)(bHasValidResponse), *ResponseTempLocation);

	OnBackgroundDownloadComplete();
}

FBackgroundHttpRequestCompleteDelegate& FBackgroundHttpRequestImpl::OnProcessRequestComplete()
{
	return HttpRequestCompleteDelegate;
}

FBackgroundHttpProgressUpdateDelegate& FBackgroundHttpRequestImpl::OnProgressUpdated()
{
	return HttpProgressUpdateDelegate;
}

const FBackgroundHttpResponsePtr FBackgroundHttpRequestImpl::GetResponse() const
{
	return Response;
}

const FString& FBackgroundHttpRequestImpl::GetRequestID() const
{
	return RequestID;
}

void FBackgroundHttpRequestImpl::SetRequestID(const FString& NewRequestID)
{
	RequestID = NewRequestID;
}

bool FBackgroundHttpRequestImpl::HandleDelayedProcess()
{
	//By default we don't provide an implementation for this. If this is called, it should be overriden by the platform specific
	//BackgroundHttpRequest if the platform expects to make use of this.
	return ensureAlwaysMsgf(false, TEXT("Platform expects an implementation of HandleDelayedProcess on the BackgroundHttpRequest, but none found!"));
}

EBackgroundHTTPPriority FBackgroundHttpRequestImpl::GetRequestPriority() const
{
	return RequestPriority;
}

void FBackgroundHttpRequestImpl::SetRequestPriority(EBackgroundHTTPPriority NewPriority)
{
	RequestPriority = NewPriority;
}
