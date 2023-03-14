// Copyright Epic Games, Inc. All Rights Reserved.

#include "HttpRetrySystem.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "HAL/LowLevelMemTracker.h"
#include "Math/RandomStream.h"
#include "HttpModule.h"
#include "Http.h"
#include "HttpManager.h"
#include "Stats/Stats.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"

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
}

FHttpRetrySystem::FRequest::FRequest(
	FManager& InManager,
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& HttpRequest, 
	const FHttpRetrySystem::FRetryLimitCountSetting& InRetryLimitCountOverride,
	const FHttpRetrySystem::FRetryTimeoutRelativeSecondsSetting& InRetryTimeoutRelativeSecondsOverride,
	const FHttpRetrySystem::FRetryResponseCodes& InRetryResponseCodes,
	const FHttpRetrySystem::FRetryVerbs& InRetryVerbs,
	const FHttpRetrySystem::FRetryDomainsPtr& InRetryDomains
	)
    : FHttpRequestAdapterBase(HttpRequest)
    , Status(FHttpRetrySystem::FRequest::EStatus::NotStarted)
    , RetryLimitCountOverride(InRetryLimitCountOverride)
    , RetryTimeoutRelativeSecondsOverride(InRetryTimeoutRelativeSecondsOverride)
	, RetryResponseCodes(InRetryResponseCodes)
	, RetryVerbs(InRetryVerbs)
	, RetryDomains(InRetryDomains)
	, RetryManager(InManager)
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
	TSharedRef<FRequest, ESPMode::ThreadSafe> RetryRequest = StaticCastSharedRef<FRequest>(AsShared());

	OriginalUrl = HttpRequest->GetURL();
	if (RetryDomains.IsValid())
	{
		SetUrlFromRetryDomains();
	}

	HttpRequest->OnRequestProgress().BindThreadSafeSP(RetryRequest, &FHttpRetrySystem::FRequest::HttpOnRequestProgress);

	return RetryManager.ProcessRequest(RetryRequest);
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

	RetryManager.CancelRequest(RetryRequest);
}

void FHttpRetrySystem::FRequest::HttpOnRequestProgress(FHttpRequestPtr InHttpRequest, int32 BytesSent, int32 BytesRcv)
{
	OnRequestProgress().ExecuteIfBound(AsShared(), BytesSent, BytesRcv);
}

FHttpRetrySystem::FManager::FManager(const FRetryLimitCountSetting& InRetryLimitCountDefault, const FRetryTimeoutRelativeSecondsSetting& InRetryTimeoutRelativeSecondsDefault)
    : RandomFailureRate(FRandomFailureRateSetting())
    , RetryLimitCountDefault(InRetryLimitCountDefault)
	, RetryTimeoutRelativeSecondsDefault(InRetryTimeoutRelativeSecondsDefault)
{}

FHttpRetrySystem::FManager::~FManager()
{
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
	const FRetryDomainsPtr& InRetryDomains)
{
	return MakeShareable(new FRequest(
		*this,
		FHttpModule::Get().CreateRequest(),
		InRetryLimitCountOverride,
		InRetryTimeoutRelativeSecondsOverride,
		InRetryResponseCodes,
		InRetryVerbs,
		InRetryDomains
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
		EHttpRequestStatus::Type Status = HttpRetryRequestEntry.Request->GetStatus();
		if (Status == EHttpRequestStatus::Failed_ConnectionError)
		{
			bResult = true;
		}
		else if (Status == EHttpRequestStatus::Failed)
		{
			const FName Verb = FName(*HttpRetryRequestEntry.Request->GetVerb());

			// Be default, we will also allow retry for GET and HEAD requests even if they may duplicate on the server
			static const TSet<FName> DefaultRetryVerbs(TArray<FName>({ FName(TEXT("GET")), FName(TEXT("HEAD")) }));

			const bool bIsRetryVerbsEmpty = HttpRetryRequestEntry.Request->RetryVerbs.Num() == 0;
			if (bIsRetryVerbsEmpty && DefaultRetryVerbs.Contains(Verb))
			{
				bResult = true;
			}
			// If retry verbs are specified, only allow retrying the specified list of verbs
			else if (HttpRetryRequestEntry.Request->RetryVerbs.Contains(Verb))
			{
				bResult = true;
			}
		}
	}
	else
	{
		// this may be a successful response with one of the explicitly listed response codes we want to retry on
		if (HttpRetryRequestEntry.Request->RetryResponseCodes.Contains(Response->GetResponseCode()))
		{
			bResult = true;
		}
	}

    return bResult;
}

bool FHttpRetrySystem::FManager::CanRetry(const FHttpRetryRequestEntry& HttpRetryRequestEntry)
{
    bool bResult = false;

    bool bShouldTestCurrentRetryCount = false;
    double RetryLimitCount = 0;
    if (HttpRetryRequestEntry.Request->RetryLimitCountOverride.IsSet())
    {
        bShouldTestCurrentRetryCount = true;
        RetryLimitCount = HttpRetryRequestEntry.Request->RetryLimitCountOverride.GetValue();
    }
    else if (RetryLimitCountDefault.IsSet())
    {
        bShouldTestCurrentRetryCount = true;
        RetryLimitCount = RetryLimitCountDefault.GetValue();
    }

    if (bShouldTestCurrentRetryCount)
    {
        if (HttpRetryRequestEntry.CurrentRetryCount < RetryLimitCount)
        {
            bResult = true;
        }
    }

    return bResult;
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
	const bool bProcessRequestSuccess = RequestEntry.Request->HttpRequest->ProcessRequest();
	if (bProcessRequestSuccess)
	{
		UE_LOG(LogHttp, Warning, TEXT("Retry %d on %s"), RequestEntry.CurrentRetryCount + 1, *(RequestEntry.Request->GetURL()));

		if (RequestEntry.CurrentRetryCount == 0)
		{
			FHttpLogVerbosityTracker::Get().IncrementRetriedRequests();
		}
		++RequestEntry.CurrentRetryCount;
		RequestEntry.Request->Status = FRequest::EStatus::Processing;
	}
}

float FHttpRetrySystem::FManager::GetLockoutPeriodSeconds(const FHttpRetryRequestEntry& HttpRetryRequestEntry)
{
	float LockoutPeriod = 0.0f;
	TOptional<double> ResponseLockoutPeriod = FHttpRetrySystem::ReadThrottledTimeFromResponseInSeconds(HttpRetryRequestEntry.Request->GetResponse());
	if (ResponseLockoutPeriod.IsSet())
	{
		LockoutPeriod = static_cast<float>(ResponseLockoutPeriod.GetValue());
	}

	if (HttpRetryRequestEntry.CurrentRetryCount >= 1)
	{
		if (LockoutPeriod <= 0.0f)
		{
			const bool bFailedToConnect = HttpRetryRequestEntry.Request->GetStatus() == EHttpRequestStatus::Failed_ConnectionError;
			const bool bHasRetryDomains = HttpRetryRequestEntry.Request->RetryDomains.IsValid();
			// Skip the lockout period if we failed to connect to a domain and we have other domains to try
			const bool bSkipLockoutPeriod = (bFailedToConnect && bHasRetryDomains);
			if (!bSkipLockoutPeriod)
			{
				constexpr const float LockoutPeriodMinimumSeconds = 5.0f;
				constexpr const float LockoutPeriodEscalationSeconds = 2.5f;
				constexpr const float LockoutPeriodMaxSeconds = 30.0f;
				LockoutPeriod = LockoutPeriodMinimumSeconds + LockoutPeriodEscalationSeconds * (HttpRetryRequestEntry.CurrentRetryCount - 1);
				LockoutPeriod = FMath::Min(LockoutPeriod, LockoutPeriodMaxSeconds);
			}
		}
	}

	return LockoutPeriod;
}

static FRandomStream TempRandomStream(4435261);

bool FHttpRetrySystem::FManager::Update(uint32* FileCount, uint32* FailingCount, uint32* FailedCount, uint32* CompletedCount)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FHttpRetrySystem_FManager_Update);
	LLM_SCOPE_BYTAG(HTTP);

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
			HttpRetryRequest->Status = FHttpRetrySystem::FRequest::EStatus::Cancelled;
		}
		else
		{
			if (!HasTimedOut(*HttpRetryRequestEntry, NowAbsoluteSeconds))
			{
				if (HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::NotStarted)
				{
					if (RequestStatus != EHttpRequestStatus::NotStarted)
					{
						HttpRetryRequest->Status = FHttpRetrySystem::FRequest::EStatus::Processing;
					}
				}

				if (HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::Processing)
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
					if (RequestStatus == EHttpRequestStatus::Failed_ConnectionError)
					{
						if (HttpRetryRequest->RetryDomains.IsValid())
						{
							HttpRetryRequest->MoveToNextRetryDomain();
						}
					}
					// Save these for failure case retry checks if we hit a completion state
					bool bShouldRetry = false;
					bool bCanRetry = false;
					if (RequestStatus == EHttpRequestStatus::Failed || RequestStatus == EHttpRequestStatus::Failed_ConnectionError || RequestStatus == EHttpRequestStatus::Succeeded)
					{
						bShouldRetry = ShouldRetry(*HttpRetryRequestEntry);
						bCanRetry = CanRetry(*HttpRetryRequestEntry);
					}

					if (RequestStatus == EHttpRequestStatus::Failed || RequestStatus == EHttpRequestStatus::Failed_ConnectionError || forceFail || (bShouldRetry && bCanRetry))
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
							HttpRetryRequest->Status = FHttpRetrySystem::FRequest::EStatus::ProcessingLockout;
							
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
							HttpRetryRequest->Status = FHttpRetrySystem::FRequest::EStatus::FailedRetry;
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

						HttpRetryRequest->Status = FHttpRetrySystem::FRequest::EStatus::Succeeded;
					}
				}

				if (HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::ProcessingLockout)
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
				HttpRetryRequest->Status = FHttpRetrySystem::FRequest::EStatus::FailedTimeout;
				if (FailedCount != nullptr)
				{
					++(*FailedCount);
				}
			}
		}

		bool bWasCompleted = false;
		bool bWasSuccessful = false;

        if (HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::Cancelled ||
            HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::FailedRetry ||
            HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::FailedTimeout ||
            HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::Succeeded)
		{
			bWasCompleted = true;
            bWasSuccessful = HttpRetryRequest->Status == FHttpRetrySystem::FRequest::EStatus::Succeeded;
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
	, RequestStartTimeAbsoluteSeconds(FPlatformTime::Seconds())
	, Request(InRequest)
{}

bool FHttpRetrySystem::FManager::ProcessRequest(TSharedRef<FHttpRetrySystem::FRequest, ESPMode::ThreadSafe>& HttpRetryRequest)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FHttpRetrySystem_FManager_ProcessRequest);

	bool bResult = HttpRetryRequest->HttpRequest->ProcessRequest();

	if (bResult)
	{
		RequestList.Add(FHttpRetryRequestEntry(HttpRetryRequest));
	}

	return bResult;
}

void FHttpRetrySystem::FManager::CancelRequest(TSharedRef<FHttpRetrySystem::FRequest, ESPMode::ThreadSafe>& HttpRetryRequest)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FHttpRetrySystem_FManager_CancelRequest);

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
}

/* This should only be used when shutting down or suspending, to make sure 
	all pending HTTP requests are flushed to the network */
void FHttpRetrySystem::FManager::BlockUntilFlushed(float InTimeoutSec)
{
	const float SleepInterval = 0.016;
	float TimeElapsed = 0.0f;
	uint32 FileCount, FailingCount, FailedCount, CompleteCount;
	while (RequestList.Num() > 0 && TimeElapsed < InTimeoutSec)
	{
		FHttpModule::Get().GetHttpManager().Tick(SleepInterval);
		Update(&FileCount, &FailingCount, &FailedCount, &CompleteCount);
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
	FCoreDelegates::OnConfigSectionsChanged.AddRaw(this, &FHttpLogVerbosityTracker::OnConfigSectionsChanged);
}

FHttpRetrySystem::FManager::FHttpLogVerbosityTracker::~FHttpLogVerbosityTracker()
{
	FCoreDelegates::OnConfigSectionsChanged.RemoveAll(this);
}

void FHttpRetrySystem::FManager::FHttpLogVerbosityTracker::IncrementRetriedRequests()
{
	check(IsInGameThread());
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
	check(IsInGameThread());
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
