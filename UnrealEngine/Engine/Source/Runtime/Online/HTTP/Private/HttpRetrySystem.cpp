// Copyright Epic Games, Inc. All Rights Reserved.

#include "HttpRetrySystem.h"
#include "GenericPlatform/HttpRequestCommon.h"
#include "HAL/ConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "HAL/LowLevelMemTracker.h"
#include "Math/RandomStream.h"
#include "HttpModule.h"
#include "Http.h"
#include "HttpManager.h"
#include "HttpThread.h"
#include "Stats/Stats.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"

TAutoConsoleVariable<bool> CVarHttpRetrySystemNonGameThreadSupportEnabled(
	TEXT("Http.RetrySystemNonGameThreadSupportEnabled"),
	false,
	TEXT("Enable retry system non-game thread support")
);

LLM_DEFINE_TAG(HTTP);

namespace FHttpRetrySystem
{

TOptional<double> ReadThrottledTimeFromResponseInSeconds(FHttpResponsePtr Response)
{
	TOptional<double> LockoutPeriod;
	// Check if there was a Retry-After header
	if (Response.IsValid())
	{
		int32 ResponseCode = Response->GetResponseCode();
		if (ResponseCode == EHttpResponseCodes::TooManyRequests || ResponseCode == EHttpResponseCodes::ServiceUnavail)
		{
			FString RetryAfter = Response->GetHeader(TEXT("Retry-After"));
			if (!RetryAfter.IsEmpty())
			{
				if (RetryAfter.IsNumeric())
				{
					// seconds
					LockoutPeriod.Emplace(FCString::Atof(*RetryAfter));
				}
				else
				{
					// http date
					FDateTime UTCServerTime;
					if (FDateTime::ParseHttpDate(RetryAfter, UTCServerTime))
					{
						const FDateTime UTCNow = FDateTime::UtcNow();
						LockoutPeriod.Emplace((UTCServerTime - UTCNow).GetTotalSeconds());
					}
				}
			}
			else
			{
				FString RateLimitReset = Response->GetHeader(TEXT("X-Rate-Limit-Reset"));
				if (!RateLimitReset.IsEmpty())
				{
					// UTC seconds
					const FDateTime UTCServerTime = FDateTime::FromUnixTimestamp(FCString::Atoi64(*RateLimitReset));
					const FDateTime UTCNow = FDateTime::UtcNow();
					LockoutPeriod.Emplace((UTCServerTime - UTCNow).GetTotalSeconds());
				}
			}
		}
	}
	return LockoutPeriod;
}

bool FHttpRetrySystem::FExponentialBackoffCurve::IsValid() const
{
	return Base > 1.0f
		&& ExponentBias >= 0.0f
		&& MinCoefficient <= MaxCoefficient
		&& MaxCoefficient > 0.001f
		&& MinCoefficient >= 0.0f;
}

float FHttpRetrySystem::FExponentialBackoffCurve::Compute(uint32 RetryNumber) const
{
	float BackOff = FMath::Pow(Base, static_cast<float>(RetryNumber) + ExponentBias);
	const float Coefficient = IsValid() ? FMath::RandRange(MinCoefficient, MaxCoefficient) : 1.0f;
	return FMath::Min(BackOff * Coefficient, MaxBackoffSeconds);
}

}

FHttpRetrySystem::FRequest::FRequest(
	TSharedRef<FManager> InManager,
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& HttpRequest,
	const FHttpRetrySystem::FRetryLimitCountSetting& InRetryLimitCountOverride,
	const FHttpRetrySystem::FRetryTimeoutRelativeSecondsSetting& InRetryTimeoutRelativeSecondsOverride,
	const FHttpRetrySystem::FRetryResponseCodes& InRetryResponseCodes,
	const FHttpRetrySystem::FRetryVerbs& InRetryVerbs,
	const FHttpRetrySystem::FRetryDomainsPtr& InRetryDomains,
	const FRetryLimitCountSetting& InRetryLimitCountForConnectionErrorOverride,
	const FExponentialBackoffCurve& InExponentialBackoffCurve
)
	: FHttpRequestAdapterBase(HttpRequest)
	, RetryStatus(FHttpRetrySystem::FRequest::EStatus::NotStarted)
	, RetryLimitCountOverride(InRetryLimitCountOverride)
	, RetryLimitCountForConnectionErrorOverride(InRetryLimitCountForConnectionErrorOverride)
	, RetryTimeoutRelativeSecondsOverride(InRetryTimeoutRelativeSecondsOverride)
	, RetryResponseCodes(InRetryResponseCodes)
	, RetryVerbs(InRetryVerbs)
	, RetryDomains(InRetryDomains)
	, RetryManager(InManager)
	, RetryExponentialBackoffCurve(InExponentialBackoffCurve)
{
    // if the InRetryTimeoutRelativeSecondsOverride override is being used the value cannot be negative
    check(!(InRetryTimeoutRelativeSecondsOverride.IsSet()) || (InRetryTimeoutRelativeSecondsOverride.GetValue() >= 0.0));

	if (RetryDomains.IsValid())
	{
		if (RetryDomains->Domains.Num() == 0)
		{
			// If there are no domains to cycle through, go through the simpler path
			RetryDomains.Reset();
		}
		else
		{
			// Start with the active index
			RetryDomainsIndex = RetryDomains->ActiveIndex;
			check(RetryDomains->Domains.IsValidIndex(RetryDomainsIndex));
		}
	}
}

bool FHttpRetrySystem::FRequest::ProcessRequest()
{
	TSharedRef<FRequest> RetryRequest = StaticCastSharedRef<FRequest>(AsShared());

	OriginalUrl = HttpRequest->GetURL();
	if (RetryDomains.IsValid())
	{
		SetUrlFromRetryDomains();
	}

	HttpRequest->OnRequestProgress64().BindThreadSafeSP(RetryRequest, &FHttpRetrySystem::FRequest::HttpOnRequestProgress);
	HttpRequest->OnProcessRequestComplete().BindThreadSafeSP(RetryRequest, &FHttpRetrySystem::FRequest::HttpOnProcessRequestComplete);
	HttpRequest->OnStatusCodeReceived().BindThreadSafeSP(RetryRequest, &FHttpRetrySystem::FRequest::HttpOnStatusCodeReceived);
	HttpRequest->OnHeaderReceived().BindThreadSafeSP(RetryRequest, &FHttpRetrySystem::FRequest::HttpOnHeaderReceived);

	TSharedPtr<FManager> RetryManagerPtr = RetryManager.Pin();

	if (ensure(RetryManagerPtr))
	{
		return RetryManagerPtr->ProcessRequest(RetryRequest);
	}
	else
	{
		return false;
	}
}

void FHttpRetrySystem::FRequest::SetUrlFromRetryDomains()
{
	check(RetryDomains.IsValid());
	FString OriginalUrlDomainAndPort = FPlatformHttp::GetUrlDomainAndPort(OriginalUrl);
	if (!OriginalUrlDomainAndPort.IsEmpty())
	{
		const FString Url(OriginalUrl.Replace(*OriginalUrlDomainAndPort, *RetryDomains->Domains[RetryDomainsIndex]));
		HttpRequest->SetURL(Url);
	}
}

void FHttpRetrySystem::FRequest::MoveToNextRetryDomain()
{
	check(RetryDomains.IsValid());
	const int32 NextDomainIndex = (RetryDomainsIndex + 1) % RetryDomains->Domains.Num();
	if (RetryDomains->ActiveIndex.CompareExchange(RetryDomainsIndex, NextDomainIndex))
	{
		RetryDomainsIndex = NextDomainIndex;
	}
	SetUrlFromRetryDomains();
}

void FHttpRetrySystem::FRequest::CancelRequest() 
{ 
	TSharedRef<FRequest, ESPMode::ThreadSafe> RetryRequest = StaticCastSharedRef<FRequest>(AsShared());

	if (TSharedPtr<FManager> RetryManagerPtr = RetryManager.Pin())
	{
		RetryManagerPtr->CancelRequest(RetryRequest);
	}
	else
	{
		HttpRequest->CancelRequest();
	}
}

void FHttpRetrySystem::FRequest::HttpOnRequestProgress(FHttpRequestPtr InHttpRequest, uint64 BytesSent, uint64 BytesRcv)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	OnRequestProgress().ExecuteIfBound(AsShared(), BytesSent, BytesRcv);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	OnRequestProgress64().ExecuteIfBound(AsShared(), BytesSent, BytesRcv);
}

void FHttpRetrySystem::FRequest::HttpOnProcessRequestComplete(FHttpRequestPtr InHttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	if (!CVarHttpRetrySystemNonGameThreadSupportEnabled.GetValueOnAnyThread())
	{
		return;
	}

	TSharedPtr<FManager> RetryManagerPtr = RetryManager.Pin();
	if (!RetryManagerPtr)
	{
		return;
	}

	TSharedRef<FRequest> SelfPtr = StaticCastSharedRef<FRequest>(AsShared()); // In case no ref after removing from RetryManager


	{
		FScopeLock ScopeLock(&RetryManagerPtr->RequestListLock);

		uint32 EntryIndex = RetryManagerPtr->RequestList.IndexOfByPredicate([this](const FManager::FHttpRetryRequestEntry& Entry) { return Entry.Request == AsShared(); });
		check(EntryIndex != INDEX_NONE);
		FManager::FHttpRetryRequestEntry& HttpRetryRequestEntry = RetryManagerPtr->RequestList[EntryIndex];


		if (RetryStatus == FHttpRetrySystem::FRequest::EStatus::Cancelled)
		{
			// Do nothing here
		}
		else if (GetStatus() == EHttpRequestStatus::Failed)
		{
			if (GetFailureReason() == EHttpFailureReason::ConnectionError && RetryDomains.IsValid())
			{
				MoveToNextRetryDomain();
			}

			if (GetFailureReason() == EHttpFailureReason::TimedOut)
			{
				RetryStatus = FHttpRetrySystem::FRequest::EStatus::FailedTimeout;
			}
			else
			{
				RetryStatus = FHttpRetrySystem::FRequest::EStatus::FailedRetry;
			}
		}
		else
		{
			RetryStatus = FHttpRetrySystem::FRequest::EStatus::Succeeded;
		}

		if (RetryStatus != FHttpRetrySystem::FRequest::EStatus::Cancelled && 
			RetryStatus != FHttpRetrySystem::FRequest::EStatus::FailedTimeout && 
			RetryManagerPtr->ShouldRetry(HttpRetryRequestEntry) && 
			RetryManagerPtr->CanRetry(HttpRetryRequestEntry))
		{
			const double NowAbsoluteSeconds = FPlatformTime::Seconds();
			float LockoutPeriod = RetryManagerPtr->GetLockoutPeriodSeconds(HttpRetryRequestEntry);

			RetryStatus = FHttpRetrySystem::FRequest::EStatus::ProcessingLockout;

			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_FHttpRetrySystem_FManager_Update_OnRequestWillRetry);
				OnRequestWillRetry().ExecuteIfBound(HttpRetryRequestEntry.Request, GetResponse(), LockoutPeriod);
			}

			RetryManagerPtr->RetryHttpRequestWithDelay(HttpRetryRequestEntry, LockoutPeriod, bSucceeded);
			return;
		}

		if (HttpRetryRequestEntry.CurrentRetryCount > 0)
		{
			FHttpRetrySystem::FManager::FHttpLogVerbosityTracker::Get().DecrementRetriedRequests();
		}

		RetryManagerPtr->RequestList.RemoveAtSwap(EntryIndex);
	}

	FHttpResponsePtr ResultResponse = HttpResponse;
	bool bResultSucceeded = bSucceeded;
	if (RetryStatus == FHttpRetrySystem::FRequest::EStatus::FailedTimeout && LastResponse != nullptr)
	{
		// Last response is better than nothing when it's timeout
		ResultResponse = LastResponse;
		LastResponse.Reset();

		bResultSucceeded = bLastSucceeded;
	}

	LLM_SCOPE_BYTAG(HTTP);
	OnProcessRequestComplete().ExecuteIfBound(SelfPtr, ResultResponse, bResultSucceeded);

	ClearTimeout();
}

void FHttpRetrySystem::FRequest::HttpOnStatusCodeReceived(FHttpRequestPtr Request, int32 StatusCode)
{
	TSharedRef<FRequest> SelfPtr = StaticCastSharedRef<FRequest>(AsShared());
	OnStatusCodeReceived().ExecuteIfBound(SelfPtr, StatusCode);
}

void FHttpRetrySystem::FRequest::HttpOnHeaderReceived(FHttpRequestPtr Request, const FString& HeaderName, const FString& NewHeaderValue)
{
	if (!CVarHttpRetrySystemNonGameThreadSupportEnabled.GetValueOnAnyThread())
	{
		return;
	}

	TSharedRef<FRequest> SelfPtr = StaticCastSharedRef<FRequest>(AsShared());
	OnHeaderReceived().ExecuteIfBound(SelfPtr, HeaderName, NewHeaderValue);
}

FHttpRetrySystem::FManager::FManager(
	const FRetryLimitCountSetting& InRetryLimitCountDefault,
	const FRetryTimeoutRelativeSecondsSetting& InRetryTimeoutRelativeSecondsDefault,
	const FRetryLimitCountSetting& InRetryLimitCountForConnectionErrorDefault
)
    : RandomFailureRate(FRandomFailureRateSetting())
    , RetryLimitCountDefault(InRetryLimitCountDefault)
    , RetryLimitCountForConnectionErrorDefault(InRetryLimitCountForConnectionErrorDefault)
	, RetryTimeoutRelativeSecondsDefault(InRetryTimeoutRelativeSecondsDefault)
{
	check(FHttpModule::Get().GetHttpManager().GetThread());
}

FHttpRetrySystem::FManager::~FManager()
{
	FScopeLock ScopeLock(&RequestListLock);

	// Decrement retried request for log verbosity tracker
	for (const FHttpRetryRequestEntry& Request : RequestList)
	{
		if (Request.CurrentRetryCount > 0)
		{
			FHttpLogVerbosityTracker::Get().DecrementRetriedRequests();
		}
	}
}

TSharedRef<FHttpRetrySystem::FRequest, ESPMode::ThreadSafe> FHttpRetrySystem::FManager::CreateRequest(
	const FRetryLimitCountSetting& InRetryLimitCountOverride,
	const FRetryTimeoutRelativeSecondsSetting& InRetryTimeoutRelativeSecondsOverride,
	const FRetryResponseCodes& InRetryResponseCodes,
	const FRetryVerbs& InRetryVerbs,
	const FRetryDomainsPtr& InRetryDomains,
	const FRetryLimitCountSetting& InRetryLimitCountForConnectionErrorOverride
)
{
	return MakeShareable(new FRequest(
		AsShared(),
		FHttpModule::Get().CreateRequest(),
		InRetryLimitCountOverride,
		InRetryTimeoutRelativeSecondsOverride,
		InRetryResponseCodes,
		InRetryVerbs,
		InRetryDomains,
		InRetryLimitCountForConnectionErrorOverride
		));
}

bool FHttpRetrySystem::FManager::ShouldRetry(const FHttpRetryRequestEntry& HttpRetryRequestEntry)
{
    bool bResult = false;

	FHttpResponsePtr Response = HttpRetryRequestEntry.Request->GetResponse();
	// invalid response means connection or network error but we need to know which one
	if (!Response.IsValid())
	{
		// ONLY retry bad responses if they are connection errors (NOT protocol errors or unknown) otherwise request may be sent (and processed!) twice
		if (HttpRetryRequestEntry.Request->GetStatus() == EHttpRequestStatus::Failed)
		{
			if (HttpRetryRequestEntry.Request->GetFailureReason() == EHttpFailureReason::ConnectionError)
			{
				bResult = true;
			}
			else
			{
				const FName Verb = FName(*HttpRetryRequestEntry.Request->GetVerb());

				// Be default, we will also allow retry for GET and HEAD requests even if they may duplicate on the server
				static const TSet<FName> DefaultRetryVerbs(TArray<FName>({ FName(TEXT("GET")), FName(TEXT("HEAD")) }));

				const TSet<FName>* RetryVerbsContainer = (
					HttpRetryRequestEntry.Request->RetryVerbs.Num() == 0
					? &DefaultRetryVerbs // Use the default list of retry verbs if the request doesn't have a specific set
					: &HttpRetryRequestEntry.Request->RetryVerbs // Otherwise use the specific set on the request
				);
				bResult = RetryVerbsContainer->Contains(Verb);
			}
		}
	}
	else
	{
		// this may be a successful response with one of the explicitly listed response codes we want to retry on
		bResult = HttpRetryRequestEntry.Request->RetryResponseCodes.Contains(Response->GetResponseCode());
	}

    return bResult;
}

bool FHttpRetrySystem::FManager::RetryLimitForConnectionErrorIsSet(const FHttpRetryRequestEntry& HttpRetryRequestEntry)
{
	return HttpRetryRequestEntry.Request->RetryLimitCountForConnectionErrorOverride.IsSet() || RetryLimitCountForConnectionErrorDefault.IsSet();
}

bool FHttpRetrySystem::FManager::CanRetryForConnectionError(const FHttpRetryRequestEntry& HttpRetryRequestEntry)
{
	uint32 RetryLimitForConnectionError = HttpRetryRequestEntry.Request->RetryLimitCountForConnectionErrorOverride.Get(RetryLimitCountForConnectionErrorDefault.Get(0));
	return HttpRetryRequestEntry.CurrentRetryCountForConnectionError < RetryLimitForConnectionError;
}

bool FHttpRetrySystem::FManager::CanRetryInGeneral(const FHttpRetryRequestEntry& HttpRetryRequestEntry)
{
	uint32 RetryLimit = HttpRetryRequestEntry.Request->RetryLimitCountOverride.Get(RetryLimitCountDefault.Get(0));
	return HttpRetryRequestEntry.CurrentRetryCount < RetryLimit;
}

bool FHttpRetrySystem::FManager::CanRetry(const FHttpRetryRequestEntry& HttpRetryRequestEntry)
{
	if (HttpRetryRequestEntry.Request->GetFailureReason() == EHttpFailureReason::ConnectionError && RetryLimitForConnectionErrorIsSet(HttpRetryRequestEntry))
	{
		return CanRetryForConnectionError(HttpRetryRequestEntry);
	}

	return CanRetryInGeneral(HttpRetryRequestEntry);
}

bool FHttpRetrySystem::FManager::HasTimedOut(const FHttpRetryRequestEntry& HttpRetryRequestEntry, const double NowAbsoluteSeconds)
{
    bool bResult = false;

    bool bShouldTestRetryTimeout = false;
    double RetryTimeoutAbsoluteSeconds = HttpRetryRequestEntry.RequestStartTimeAbsoluteSeconds;
    if (HttpRetryRequestEntry.Request->RetryTimeoutRelativeSecondsOverride.IsSet())
    {
        bShouldTestRetryTimeout = true;
        RetryTimeoutAbsoluteSeconds += HttpRetryRequestEntry.Request->RetryTimeoutRelativeSecondsOverride.GetValue();
    }
    else if (RetryTimeoutRelativeSecondsDefault.IsSet())
    {
        bShouldTestRetryTimeout = true;
        RetryTimeoutAbsoluteSeconds += RetryTimeoutRelativeSecondsDefault.GetValue();
    }

    if (bShouldTestRetryTimeout)
    {
        if (NowAbsoluteSeconds >= RetryTimeoutAbsoluteSeconds)
        {
            bResult = true;
        }
    }

    return bResult;
}

void FHttpRetrySystem::FManager::RetryHttpRequest(FHttpRetryRequestEntry& RequestEntry)
{
	// if this fails the HttpRequest's state will be failed which will cause the retry logic to kick(as expected)
	if (RequestEntry.CurrentRetryCount == 0)
	{
		FHttpLogVerbosityTracker::Get().IncrementRetriedRequests();
	}
	++RequestEntry.CurrentRetryCount;
	if (RequestEntry.Request->GetFailureReason() == EHttpFailureReason::ConnectionError)
	{
		++RequestEntry.CurrentRetryCountForConnectionError;
	}
	RequestEntry.Request->RetryStatus = FRequest::EStatus::Processing;
	UE_LOG(LogHttp, Warning, TEXT("Retry %d on %s"), RequestEntry.CurrentRetryCount, *(RequestEntry.Request->GetURL()));
	RequestEntry.Request->HttpRequest->ProcessRequest();
}

void FHttpRetrySystem::FManager::RetryHttpRequestWithDelay(FManager::FHttpRetryRequestEntry& RequestEntry, float InDelay, bool bWasSucceeded)
{
	// Timeout during lock out to keep existing behavior
	float TimeoutOrDefault = RequestEntry.Request->GetTimeout().Get(FHttpModule::Get().GetHttpTotalTimeout());
	if (TimeoutOrDefault != 0)
	{
		float TimeElapsedForTheRequest = FPlatformTime::Seconds() - RequestEntry.RequestStartTimeAbsoluteSeconds;
		float WillTimeoutInDelay = TimeoutOrDefault - TimeElapsedForTheRequest;
		if (WillTimeoutInDelay < InDelay)
		{
			HttpRequestTimeoutAfterDelay(RequestEntry, bWasSucceeded, TimeoutOrDefault);
			return;
		}
	}

	// Delay and start
	TWeakPtr<FRequest> RequestWeakPtr(RequestEntry.Request);
	FHttpModule::Get().GetHttpManager().AddHttpThreadTask([RequestWeakPtr, bWasSucceeded]() {
		if (TSharedPtr<FRequest> RequestPtr = RequestWeakPtr.Pin())
		{
			if (TSharedPtr<FManager> RetryManagerPtr = RequestPtr->RetryManager.Pin())
			{
				FScopeLock ScopeLock(&RetryManagerPtr->RequestListLock);
				// Check if it's still there in case it has been cancelled during the delay period
				uint32 EntryIndex = RetryManagerPtr->RequestList.IndexOfByPredicate([RequestPtr](const FManager::FHttpRetryRequestEntry& Entry) { return Entry.Request == RequestPtr; });
				if (EntryIndex != INDEX_NONE)
				{
					FManager::FHttpRetryRequestEntry* HttpRetryRequestEntry = &RetryManagerPtr->RequestList[EntryIndex];
					// TODO: Move this into RetryHttpRequest after stablizing the flow with CVarHttpRetrySystemNonGameThreadSupportEnabled on
					HttpRetryRequestEntry->Request->LastResponse = HttpRetryRequestEntry->Request->GetResponse();
					HttpRetryRequestEntry->Request->bLastSucceeded = bWasSucceeded;
					RetryManagerPtr->RetryHttpRequest(*HttpRetryRequestEntry);
				}
			}
		}
	}, InDelay);
}

void FHttpRetrySystem::FManager::HttpRequestTimeoutAfterDelay(FManager::FHttpRetryRequestEntry& RequestEntry, bool bWasSucceeded, float Delay)
{
	TWeakPtr<FRequest> RequestWeakPtr(RequestEntry.Request);
	FHttpModule::Get().GetHttpManager().AddHttpThreadTask([RequestWeakPtr, bWasSucceeded]() {
		if (TSharedPtr<FRequest> RequestPtr = RequestWeakPtr.Pin())
		{
			if (TSharedPtr<FManager> RetryManagerPtr = RequestPtr->RetryManager.Pin())
			{
				FScopeLock ScopeLock(&RetryManagerPtr->RequestListLock);
				uint32 EntryIndex = RetryManagerPtr->RequestList.IndexOfByPredicate([RequestPtr](const FManager::FHttpRetryRequestEntry& Entry) { return Entry.Request == RequestPtr; });
				if (EntryIndex != INDEX_NONE)
				{
					FManager::FHttpRetryRequestEntry* HttpRetryRequestEntry = &RetryManagerPtr->RequestList[EntryIndex];
					if (HttpRetryRequestEntry->CurrentRetryCount > 0)
					{
						FHttpRetrySystem::FManager::FHttpLogVerbosityTracker::Get().DecrementRetriedRequests();
					}
					RetryManagerPtr->RequestList.RemoveAtSwap(EntryIndex);
				}
			}

			// Same as existing behavior, when timeout during lock out period, it fails with result of last request before lockout
			RequestPtr->OnProcessRequestComplete().ExecuteIfBound(RequestPtr, RequestPtr->GetResponse(), bWasSucceeded);
		}
	}, Delay);
}

float FHttpRetrySystem::FManager::GetLockoutPeriodSeconds(const FHttpRetryRequestEntry& HttpRetryRequestEntry)
{
	float LockoutPeriod = 0.0f;
	TOptional<double> ResponseLockoutPeriod = FHttpRetrySystem::ReadThrottledTimeFromResponseInSeconds(HttpRetryRequestEntry.Request->GetResponse());
	if (ResponseLockoutPeriod.IsSet())
	{
		LockoutPeriod = static_cast<float>(ResponseLockoutPeriod.GetValue());
	}

	if (LockoutPeriod <= 0.0f)
	{
		const bool bFailedToConnect = (HttpRetryRequestEntry.Request->GetStatus() == EHttpRequestStatus::Failed && HttpRetryRequestEntry.Request->GetFailureReason() == EHttpFailureReason::ConnectionError);
		const bool bHasRetryDomains = HttpRetryRequestEntry.Request->RetryDomains.IsValid();
		// Skip the lockout period if we failed to connect to a domain and we have other domains to try
		if (bFailedToConnect && bHasRetryDomains)
		{
			return 0.0f;
		}
		// The first time through this function, the CurrentRetryCount is 0, the second time it's 1, etc. We automatically add 1 to make the input into the backoff function line up with expectations.
		LockoutPeriod = HttpRetryRequestEntry.Request->RetryExponentialBackoffCurve.Compute(HttpRetryRequestEntry.CurrentRetryCount+1);
	}

	return LockoutPeriod;
}

static FRandomStream TempRandomStream(4435261);

bool FHttpRetrySystem::FManager::Update(uint32* FileCount, uint32* FailingCount, uint32* FailedCount, uint32* CompletedCount)
{
	if (CVarHttpRetrySystemNonGameThreadSupportEnabled.GetValueOnAnyThread())
	{
		return true;
	}

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FHttpRetrySystem_FManager_Update);
	LLM_SCOPE_BYTAG(HTTP);

	FScopeLock ScopeLock(&RequestListLock);

	bool bIsGreen = true;

	if (FileCount != nullptr)
	{
		*FileCount = RequestList.Num();
	}

	const double NowAbsoluteSeconds = FPlatformTime::Seconds();

	// Basic algorithm
	// for each managed item
	//    if the item hasn't timed out
	//       if the item's retry state is NotStarted
	//          if the item's request's state is not NotStarted
	//             move the item's retry state to Processing
	//          endif
	//       endif
	//       if the item's retry state is Processing
	//          if the item's request's state is Failed
	//             flag return code to false
	//             if the item can be retried
	//                increment FailingCount if applicable
	//                retry the item's request
	//                increment the item's retry count
	//             else
	//                increment FailedCount if applicable
	//                set the item's retry state to FailedRetry
	//             endif
	//          else if the item's request's state is Succeeded
	//          endif
	//       endif
	//    else
	//       flag return code to false
	//       set the item's retry state to FailedTimeout
	//       increment FailedCount if applicable
	//    endif
	//    if the item's retry state is FailedRetry
	//       do stuff
	//    endif
	//    if the item's retry state is FailedTimeout
	//       do stuff
	//    endif
	//    if the item's retry state is Succeeded
	//       do stuff
	//    endif
	// endfor

	int32 RequestIndex = 0;
	while (RequestIndex < RequestList.Num())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FHttpRetrySystem_FManager_Update_RequestListItem);

		FHttpRetryRequestEntry* HttpRetryRequestEntry = &RequestList[RequestIndex];
		TSharedRef<FHttpRetrySystem::FRequest, ESPMode::ThreadSafe> HttpRetryRequest = HttpRetryRequestEntry->Request;
		// Delegates fired in this loop can resize the array if new requests are added, invalidating HttpRetryRequestEntry and HttpRetryRequest.  Call this when the array may have been modified.
		auto ResetCurrentIterationVariables = [this, &HttpRetryRequestEntry, &HttpRetryRequest, RequestIndex]()
		{
			HttpRetryRequestEntry = &RequestList[RequestIndex];
		};

		const EHttpRequestStatus::Type RequestStatus = HttpRetryRequest->GetStatus();

		if (HttpRetryRequestEntry->bShouldCancel)
		{
			UE_LOG(LogHttp, Warning, TEXT("Request cancelled on %s"), *(HttpRetryRequest->GetURL()));
			HttpRetryRequest->RetryStatus = FHttpRetrySystem::FRequest::EStatus::Cancelled;
		}
		else
		{
			if (!HasTimedOut(*HttpRetryRequestEntry, NowAbsoluteSeconds))
			{
				if (HttpRetryRequest->RetryStatus == FHttpRetrySystem::FRequest::EStatus::NotStarted)
				{
					if (RequestStatus != EHttpRequestStatus::NotStarted)
					{
						HttpRetryRequest->RetryStatus = FHttpRetrySystem::FRequest::EStatus::Processing;
					}
				}

				if (HttpRetryRequest->RetryStatus == FHttpRetrySystem::FRequest::EStatus::Processing)
				{
					bool forceFail = false;

					// Code to simulate request failure
					if (RequestStatus == EHttpRequestStatus::Succeeded && RandomFailureRate.IsSet())
					{
						float random = TempRandomStream.GetFraction();
						if (random < RandomFailureRate.GetValue())
						{
							forceFail = true;
						}
					}

					// If we failed to connect, try the next domain in the list
					if (HttpRetryRequest->GetStatus() == EHttpRequestStatus::Failed && HttpRetryRequest->GetFailureReason() == EHttpFailureReason::ConnectionError)
					{
						if (HttpRetryRequest->RetryDomains.IsValid())
						{
							HttpRetryRequest->MoveToNextRetryDomain();
						}
					}
					// Save these for failure case retry checks if we hit a completion state
					bool bShouldRetry = false;
					bool bCanRetry = false;
					if (EHttpRequestStatus::IsFinished(RequestStatus))
					{
						bShouldRetry = ShouldRetry(*HttpRetryRequestEntry);
						bCanRetry = CanRetry(*HttpRetryRequestEntry);
					}

					if (RequestStatus == EHttpRequestStatus::Failed || forceFail || (bShouldRetry && bCanRetry))
					{
						bIsGreen = false;

						if (forceFail || (bShouldRetry && bCanRetry))
						{
							float LockoutPeriod = GetLockoutPeriodSeconds(*HttpRetryRequestEntry);

							if (LockoutPeriod > 0.0f)
							{
								UE_LOG(LogHttp, Warning, TEXT("Lockout of %fs on %s"), LockoutPeriod, *(HttpRetryRequest->GetURL()));
							}

							HttpRetryRequestEntry->LockoutEndTimeAbsoluteSeconds = NowAbsoluteSeconds + LockoutPeriod;
							HttpRetryRequest->RetryStatus = FHttpRetrySystem::FRequest::EStatus::ProcessingLockout;
							
							QUICK_SCOPE_CYCLE_COUNTER(STAT_FHttpRetrySystem_FManager_Update_OnRequestWillRetry);
							HttpRetryRequest->OnRequestWillRetry().ExecuteIfBound(HttpRetryRequest, HttpRetryRequest->GetResponse(), LockoutPeriod);
							ResetCurrentIterationVariables();
						}
						else
						{
							UE_LOG(LogHttp, Warning, TEXT("Retry exhausted on %s"), *(HttpRetryRequest->GetURL()));
							if (FailedCount != nullptr)
							{
								++(*FailedCount);
							}
							HttpRetryRequest->RetryStatus = FHttpRetrySystem::FRequest::EStatus::FailedRetry;
						}
					}
					else if (RequestStatus == EHttpRequestStatus::Succeeded)
					{
						if (HttpRetryRequestEntry->CurrentRetryCount > 0)
						{
							UE_LOG(LogHttp, Warning, TEXT("Success on %s"), *(HttpRetryRequest->GetURL()));
						}

						if (CompletedCount != nullptr)
						{
							++(*CompletedCount);
						}

						HttpRetryRequest->RetryStatus = FHttpRetrySystem::FRequest::EStatus::Succeeded;
					}
				}

				if (HttpRetryRequest->RetryStatus == FHttpRetrySystem::FRequest::EStatus::ProcessingLockout)
				{
					if (NowAbsoluteSeconds >= HttpRetryRequestEntry->LockoutEndTimeAbsoluteSeconds)
					{
						RetryHttpRequest(*HttpRetryRequestEntry);
						ResetCurrentIterationVariables();
					}

					if (FailingCount != nullptr)
					{
						++(*FailingCount);
					}
				}
			}
			else
			{
				UE_LOG(LogHttp, Warning, TEXT("Timeout on retry %d: %s"), HttpRetryRequestEntry->CurrentRetryCount + 1, *(HttpRetryRequest->GetURL()));
				bIsGreen = false;
				HttpRetryRequest->RetryStatus = FHttpRetrySystem::FRequest::EStatus::FailedTimeout;
				if (FailedCount != nullptr)
				{
					++(*FailedCount);
				}
			}
		}

		bool bWasCompleted = false;
		bool bWasSuccessful = false;

        if (HttpRetryRequest->RetryStatus == FHttpRetrySystem::FRequest::EStatus::Cancelled ||
            HttpRetryRequest->RetryStatus == FHttpRetrySystem::FRequest::EStatus::FailedRetry ||
            HttpRetryRequest->RetryStatus == FHttpRetrySystem::FRequest::EStatus::FailedTimeout ||
            HttpRetryRequest->RetryStatus == FHttpRetrySystem::FRequest::EStatus::Succeeded)
		{
			bWasCompleted = true;
            bWasSuccessful = HttpRetryRequest->RetryStatus == FHttpRetrySystem::FRequest::EStatus::Succeeded;
		}

		if (bWasCompleted)
		{
			if (bWasSuccessful)
			{
				HttpRetryRequest->BroadcastResponseHeadersReceived();
				ResetCurrentIterationVariables();
			}
			
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FHttpRetrySystem_FManager_Update_OnProcessRequestComplete);
			HttpRetryRequest->OnProcessRequestComplete().ExecuteIfBound(HttpRetryRequest, HttpRetryRequest->GetResponse(), bWasSuccessful);
			ResetCurrentIterationVariables();
		}

        if (bWasSuccessful)
        {
            if (CompletedCount != nullptr)
            {
                ++(*CompletedCount);
            }
        }

		if (bWasCompleted)
		{
			if (RequestList[RequestIndex].CurrentRetryCount > 0)
			{
				FHttpLogVerbosityTracker::Get().DecrementRetriedRequests();
			}
			RequestList.RemoveAtSwap(RequestIndex);
		}
		else
		{
			++RequestIndex;
		}
	}

	return bIsGreen;
}

FHttpRetrySystem::FManager::FHttpRetryRequestEntry::FHttpRetryRequestEntry(TSharedRef<FHttpRetrySystem::FRequest, ESPMode::ThreadSafe>& InRequest)
	: bShouldCancel(false)
	, CurrentRetryCount(0)
	, CurrentRetryCountForConnectionError(0)
	, RequestStartTimeAbsoluteSeconds(FPlatformTime::Seconds())
	, Request(InRequest)
{}

bool FHttpRetrySystem::FManager::ProcessRequest(TSharedRef<FHttpRetrySystem::FRequest, ESPMode::ThreadSafe>& HttpRetryRequest)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FHttpRetrySystem_FManager_ProcessRequest);

	if (CVarHttpRetrySystemNonGameThreadSupportEnabled.GetValueOnAnyThread())
	{
		// Let the request trigger timeout by itself, instead of ticking it in retry system
		if (HttpRetryRequest->RetryTimeoutRelativeSecondsOverride.IsSet())
		{
			HttpRetryRequest->SetTimeout(HttpRetryRequest->RetryTimeoutRelativeSecondsOverride.GetValue());
		}
		else if (RetryTimeoutRelativeSecondsDefault.IsSet())
		{
			HttpRetryRequest->SetTimeout(RetryTimeoutRelativeSecondsDefault.GetValue());
		}
	}

	FScopeLock ScopeLock(&RequestListLock);
	RequestList.Add(FHttpRetryRequestEntry(HttpRetryRequest));
	HttpRetryRequest->RetryStatus = FHttpRetrySystem::FRequest::EStatus::Processing;
	HttpRetryRequest->HttpRequest->ProcessRequest();

	return true;
}

void FHttpRetrySystem::FManager::CancelRequest(TSharedRef<FHttpRetrySystem::FRequest, ESPMode::ThreadSafe>& HttpRetryRequest)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FHttpRetrySystem_FManager_CancelRequest);

	FScopeLock ScopeLock(&RequestListLock);

	// Find the existing request entry if is was previously processed.
	bool bFound = false;
	for (int32 i = 0; i < RequestList.Num(); ++i)
	{
		FHttpRetryRequestEntry& EntryRef = RequestList[i];

		if (EntryRef.Request == HttpRetryRequest)
		{
			EntryRef.bShouldCancel = true;
			bFound = true;
		}
	}
	// If we did not find the entry, likely auth failed for the request, in which case ProcessRequest does not get called.
	// Adding it to the list and flagging as cancel will process it on next tick.
	if (!bFound)
	{
		FHttpRetryRequestEntry RetryRequestEntry(HttpRetryRequest);
		RetryRequestEntry.bShouldCancel = true;
		RequestList.Add(RetryRequestEntry);
	}

	HttpRetryRequest->HttpRequest->CancelRequest();
	HttpRetryRequest->RetryStatus = FHttpRetrySystem::FRequest::EStatus::Cancelled;
}

/* This should only be used when shutting down or suspending, to make sure 
	all pending HTTP requests are flushed to the network */
void FHttpRetrySystem::FManager::BlockUntilFlushed(float InTimeoutSec)
{
	const float SleepInterval = 0.016;
	float TimeElapsed = 0.0f;
	uint32 FileCount, FailingCount, FailedCount, CompleteCount;

	while (TimeElapsed < InTimeoutSec)
	{
		{
			FScopeLock ScopeLock(&RequestListLock);
			if (RequestList.IsEmpty())
			{
				break;
			}
		}

		FHttpModule::Get().GetHttpManager().Tick(SleepInterval);

PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Update(&FileCount, &FailingCount, &FailedCount, &CompleteCount);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

		FPlatformProcess::Sleep(SleepInterval);
		TimeElapsed += SleepInterval;
	}
}

FHttpRetrySystem::FManager::FHttpLogVerbosityTracker& FHttpRetrySystem::FManager::FHttpLogVerbosityTracker::Get()
{
	static FHttpLogVerbosityTracker Tracker;
	return Tracker;
}

FHttpRetrySystem::FManager::FHttpLogVerbosityTracker::FHttpLogVerbosityTracker()
{
	UpdateSettingsFromConfig();
	FCoreDelegates::TSOnConfigSectionsChanged().AddRaw(this, &FHttpLogVerbosityTracker::OnConfigSectionsChanged);
}

FHttpRetrySystem::FManager::FHttpLogVerbosityTracker::~FHttpLogVerbosityTracker()
{
	FCoreDelegates::TSOnConfigSectionsChanged().RemoveAll(this);
}

void FHttpRetrySystem::FManager::FHttpLogVerbosityTracker::IncrementRetriedRequests()
{
	FScopeLock ScopeLock(&NumRetriedRequestsLock);

	++NumRetriedRequests;
	if (NumRetriedRequests == 1)
	{
		OriginalVerbosity = UE_GET_LOG_VERBOSITY(LogHttp);
		if (TargetVerbosity != ELogVerbosity::NoLogging)
		{
			UE_LOG(LogHttp, Warning, TEXT("HttpRetry: Increasing log verbosity from %s to %s due to requests being retried"), ToString(OriginalVerbosity), ToString(TargetVerbosity));
			//UE_SET_LOG_VERBOSITY(LogHttp, TargetVerbosity); // Macro requires the value to be a ELogVerbosity constant
#if !NO_LOGGING
			LogHttp.SetVerbosity(TargetVerbosity);
#endif
		}
	}
}

void FHttpRetrySystem::FManager::FHttpLogVerbosityTracker::DecrementRetriedRequests()
{
	FScopeLock ScopeLock(&NumRetriedRequestsLock);

	--NumRetriedRequests;
	check(NumRetriedRequests >= 0);
	if (NumRetriedRequests == 0)
	{
		UE_LOG(LogHttp, Warning, TEXT("HttpRetry: Resetting log verbosity to %s due to requests being retried"), ToString(OriginalVerbosity));
		//UE_SET_LOG_VERBOSITY(LogHttp, OriginalVerbosity); // Macro requires the value to be a ELogVerbosity constant
#if !NO_LOGGING
		LogHttp.SetVerbosity(OriginalVerbosity);
#endif
	}
}

void FHttpRetrySystem::FManager::FHttpLogVerbosityTracker::UpdateSettingsFromConfig()
{
	FString TargetVerbosityAsString;
	if (GConfig->GetString(TEXT("HTTP.Retry"), TEXT("RetryManagerVerbosityLevel"), TargetVerbosityAsString, GEngineIni))
	{
		TargetVerbosity = ParseLogVerbosityFromString(TargetVerbosityAsString);
	}
	else
	{
		TargetVerbosity = ELogVerbosity::NoLogging;
	}
}

void FHttpRetrySystem::FManager::FHttpLogVerbosityTracker::OnConfigSectionsChanged(const FString& IniFilename, const TSet<FString>& SectionName)
{
	if (IniFilename == GEngineIni && SectionName.Contains(TEXT("HTTP.Retry")))
	{
		UpdateSettingsFromConfig();
	}
}
