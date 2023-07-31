// Copyright Epic Games, Inc. All Rights Reserved.

#include "AndroidPlatformBackgroundHttpRequest.h"
#include "AndroidPlatformBackgroundHttpManager.h"
#include "Interfaces/IBackgroundHttpResponse.h"
#include "BackgroundHttpModule.h"

#include "Serialization/JsonSerializerMacros.h"
#include "Serialization/JsonWriter.h"

typedef  TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>> FAndroidHttpRequestJsonWriter;
typedef  TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>> FAndroidHttpRequestJsonWriterFactory;

const FString FAndroidPlatformBackgroundHttpRequest::URLKey = "URLs";
const FString FAndroidPlatformBackgroundHttpRequest::RequestIDKey = "RequestID";
const FString FAndroidPlatformBackgroundHttpRequest::DestinationLocationKey = "DestLocation";
const FString FAndroidPlatformBackgroundHttpRequest::MaxRetryCountKey = "MaxRetryCount";
const FString FAndroidPlatformBackgroundHttpRequest::IndividualURLRetryCountKey = "IndividualURLRetryCount";
const FString FAndroidPlatformBackgroundHttpRequest::RequestPriorityKey = "RequestPriority";
const FString FAndroidPlatformBackgroundHttpRequest::GroupIDKey = "GroupId";
const FString FAndroidPlatformBackgroundHttpRequest::bHasCompletedKey = "bHasCompleted";

FAndroidPlatformBackgroundHttpRequest::FAndroidPlatformBackgroundHttpRequest()
	: bIsCompleted(false)
	, bIsPaused(false)
	, DownloadProgress(0)
	, DownloadProgressSinceLastUpdateSent(0)
{
}

void FAndroidPlatformBackgroundHttpRequest::PauseRequest()
{
	FAndroidPlatformBackgroundHttpManagerPtr AndroidBGManagerPtr = StaticCastSharedPtr<FAndroidPlatformBackgroundHttpManager>(FBackgroundHttpModule::Get().GetBackgroundHttpManager());
	if (ensureAlwaysMsgf(AndroidBGManagerPtr.IsValid(), TEXT("Found a BackgroundHttpManager that wasn't the AndroidBackgroundHttpManager! This is not supported or expected!")))
	{
		AndroidBGManagerPtr->PauseRequest(SharedThis(this));
	}
}

void FAndroidPlatformBackgroundHttpRequest::ResumeRequest()
{
	FAndroidPlatformBackgroundHttpManagerPtr AndroidBGManagerPtr = StaticCastSharedPtr<FAndroidPlatformBackgroundHttpManager>(FBackgroundHttpModule::Get().GetBackgroundHttpManager());
	if (ensureAlwaysMsgf(AndroidBGManagerPtr.IsValid(), TEXT("Found a BackgroundHttpManager that wasn't the AndroidBackgroundHttpManager! This is not supported or expected!")))
	{
		AndroidBGManagerPtr->ResumeRequest(SharedThis(this));
	}
}

FString FAndroidPlatformBackgroundHttpRequest::ToJSon() const
{
	FString JsonOutput;
	TSharedRef<FAndroidHttpRequestJsonWriter> Writer = FAndroidHttpRequestJsonWriterFactory::Create(&JsonOutput);
	Writer->WriteObjectStart();
	{
		Writer->WriteValue(RequestIDKey, RequestID);
		Writer->WriteValue(RequestPriorityKey, GetPriorityAsAndroidPriority());
		Writer->WriteValue(MaxRetryCountKey, NumberOfTotalRetries);
		Writer->WriteValue(DestinationLocationKey, GetDestinationLocation());
		
		//Write this bool as either true/false so it can be parsed in JSON as a java Boolean object
		const bool bIsCompletedCopy = FPlatformAtomics::AtomicRead(&bIsCompleted);
		const FString HasCompletedString = bIsCompletedCopy ? TEXT("true") : TEXT("false");
		Writer->WriteValue(bHasCompletedKey, HasCompletedString);

		//TODO: The intent of this key is to allow multiple download notifications to be active at once and this group would mean all notifications with the same key
		//are lumped together. For now everything is just expected to be in the same group, but if we want to support this we can implement a more meaningful groupID here.
		Writer->WriteValue(GroupIDKey, 0);
		
		//TODO: Should pull this from the .ini. See how we handle ApplePlatformBackgroundHttPManager::RetryResumeDataLimitSetting
		static const int DefaultIndividualURLRetryCount = 3;
		Writer->WriteValue(IndividualURLRetryCountKey, DefaultIndividualURLRetryCount);

		Writer->WriteArrayStart(URLKey);
		{
			for (const FString& URL : URLList)
			{
				Writer->WriteValue(URL);
			}
		}
		Writer->WriteArrayEnd();
	}
	Writer->WriteObjectEnd();
	Writer->Close();
	
	return JsonOutput;
}

FString FAndroidPlatformBackgroundHttpRequest::GetDestinationLocation() const
{
	//For Android we just use the first URL as the destination location as we have to supply this information up front instead of at completion
	if (URLList.Num() > 0)
	{
		const FString FirstURL = URLList[0];
		return FBackgroundHttpModule::Get().GetBackgroundHttpManager()->GetTempFileLocationForURL(FirstURL);
	}

	return TEXT("");
}

int FAndroidPlatformBackgroundHttpRequest::GetPriorityAsAndroidPriority() const
{
	switch (RequestPriority)
	{
		case EBackgroundHTTPPriority::High:
			return 1;
			break;

		case EBackgroundHTTPPriority::Low:
			return -1;
			break;

		case EBackgroundHTTPPriority::Normal:
			return 0;
			break;

		default:
			ensureAlwaysMsgf(false, TEXT("Missing EBackgroundHTTPPriority in GetPriorityAsAndroidPriority!"));
			return 0;
			break;
	}
}

void FAndroidPlatformBackgroundHttpRequest::UpdateDownloadProgress(int64_t TotalDownloaded, int64_t DownloadedSinceLastUpdate)
{
	UE_LOG(LogBackgroundHttpRequest, VeryVerbose, TEXT("Request Update Progress -- RequestDebugID:%s | OldProgress:%lld | NewProgress:%lld | ProgressSinceLastUpdate:%lld"), *GetRequestID(), DownloadProgress, TotalDownloaded, DownloadedSinceLastUpdate);

	FPlatformAtomics::AtomicStore(&DownloadProgress, TotalDownloaded);
	FPlatformAtomics::InterlockedAdd(&DownloadProgressSinceLastUpdateSent, DownloadedSinceLastUpdate);
}

void FAndroidPlatformBackgroundHttpRequest::SendDownloadProgressUpdate()
{
	//The download progress delegate should only be firing on the game thread 
	//so that requestors don't have to worry about thread safety unexpectidly
	ensureAlwaysMsgf(IsInGameThread(), TEXT("Called from un-expected thread! Potential error in an implementation of background downloads!"));

	volatile int64 DownloadProgressCopy = FPlatformAtomics::AtomicRead(&DownloadProgress);

	//Don't send any updates if we haven't updated anything since we last sent an update
	if (DownloadProgressCopy > 0)
	{
		//Reset our DownloadProgressSinceLastUpdateSent to 0 now that we are sending a progress update
		volatile int64 DownloadProgressSinceLastUpdateSentCopy = FPlatformAtomics::InterlockedExchange(&DownloadProgressSinceLastUpdateSent, 0);

		OnProgressUpdated().ExecuteIfBound(SharedThis(this), DownloadProgressCopy, DownloadProgressSinceLastUpdateSentCopy);
	}
}