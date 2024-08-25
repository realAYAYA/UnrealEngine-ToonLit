// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/HttpRequestCommon.h"
#include "GenericPlatform/HttpResponseCommon.h"
#include "HAL/Event.h"
#include "Http.h"
#include "HttpManager.h"
#include "Misc/CommandLine.h"
#include "Stats/Stats.h"

FHttpRequestCommon::FHttpRequestCommon()
	: RequestStartTimeAbsoluteSeconds(FPlatformTime::Seconds())
	, ActivityTimeoutAt(0.0)
{
}

FString FHttpRequestCommon::GetURLParameter(const FString& ParameterName) const
{
	FString ReturnValue;
	if (TOptional<FString> OptionalParameterValue = FGenericPlatformHttp::GetUrlParameter(GetURL(), ParameterName))
	{
		ReturnValue = MoveTemp(OptionalParameterValue.GetValue());
	}
	return ReturnValue;
}

EHttpRequestStatus::Type FHttpRequestCommon::GetStatus() const
{
	return CompletionStatus;
}

const FString& FHttpRequestCommon::GetEffectiveURL() const
{
	return EffectiveURL;
}

EHttpFailureReason FHttpRequestCommon::GetFailureReason() const
{
	return FailureReason;
}

bool FHttpRequestCommon::PreCheck() const
{
	// Disabled http request processing
	if (!FHttpModule::Get().IsHttpEnabled())
	{
		UE_LOG(LogHttp, Verbose, TEXT("Http disabled. Skipping request. url=%s"), *GetURL());
		return false;
	}

	// Prevent overlapped requests using the same instance
	if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. Still processing last request."));
		return false;
	}

	// Nothing to do without a valid URL
	if (GetURL().IsEmpty())
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. No URL was specified."));
		return false;
	}

	if (GetVerb().IsEmpty())
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. No Verb was specified."));
		return false;
	}

	if (!FHttpModule::Get().GetHttpManager().IsDomainAllowed(GetURL()))
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. URL '%s' is not using an allowed domain."), *GetURL());
		return false;
	}

	if (bTimedOut)
	{
		UE_LOG(LogHttp, Warning, TEXT("ProcessRequest failed. Request with URL '%s' already timed out."), *GetURL());
		return false;
	}

	return true;
}

bool FHttpRequestCommon::PreProcess()
{
	ClearInCaseOfRetry();

	if (!PreCheck() || !SetupRequest())
	{
		FinishRequestNotInHttpManager();
		return false;
	}

	StartTotalTimeoutTimer();

	UE_LOG(LogHttp, Verbose, TEXT("%p: Verb='%s' URL='%s'"), this, *GetVerb(), *GetURL());

	return true;
}

void FHttpRequestCommon::PostProcess()
{
	CleanupRequest();
}

void FHttpRequestCommon::ClearInCaseOfRetry()
{
	bActivityTimedOut = false;
	FailureReason = EHttpFailureReason::None;
	bCanceled = false;
	EffectiveURL = GetURL();
	ResponseCommon.Reset();
}

void FHttpRequestCommon::FinishRequestNotInHttpManager()
{
	if (IsInGameThread())
	{
		if (DelegateThreadPolicy == EHttpRequestDelegateThreadPolicy::CompleteOnGameThread)
		{
			FinishRequest();
		}
		else
		{
			FHttpModule::Get().GetHttpManager().AddHttpThreadTask([StrongThis = StaticCastSharedRef<FHttpRequestCommon>(AsShared())]()
			{
				StrongThis->FinishRequest();
			});
		}
	}
	else
	{
		if (DelegateThreadPolicy == EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread)
		{
			FinishRequest();
		}
		else
		{
			FHttpModule::Get().GetHttpManager().AddGameThreadTask([StrongThis = StaticCastSharedRef<FHttpRequestCommon>(AsShared())]()
			{
				StrongThis->FinishRequest();
			});
		}
	}
}

void FHttpRequestCommon::SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy InDelegateThreadPolicy)
{ 
	DelegateThreadPolicy = InDelegateThreadPolicy; 
}

EHttpRequestDelegateThreadPolicy FHttpRequestCommon::GetDelegateThreadPolicy() const
{ 
	return DelegateThreadPolicy; 
}

void FHttpRequestCommon::HandleRequestSucceed(TSharedPtr<IHttpResponse> InResponse)
{
	SetStatus(EHttpRequestStatus::Succeeded);
	OnProcessRequestComplete().ExecuteIfBound(SharedThis(this), InResponse, true);
	FHttpModule::Get().GetHttpManager().RecordStatTimeToConnect(ConnectTime);
}

void FHttpRequestCommon::SetStatus(EHttpRequestStatus::Type InCompletionStatus)
{
	CompletionStatus = InCompletionStatus;

	if (ResponseCommon)
	{
		ResponseCommon->SetRequestStatus(InCompletionStatus);
	}
}

void FHttpRequestCommon::SetFailureReason(EHttpFailureReason InFailureReason)
{
	UE_CLOG(FailureReason != EHttpFailureReason::None, LogHttp, Warning, TEXT("FailureReason had been set to %s, now setting to %s"), LexToString(FailureReason), LexToString(InFailureReason));
	FailureReason = InFailureReason;

	if (ResponseCommon)
	{
		ResponseCommon->SetRequestFailureReason(InFailureReason);
	}
}

void FHttpRequestCommon::SetTimeout(float InTimeoutSecs)
{
	TimeoutSecs = InTimeoutSecs;
}

void FHttpRequestCommon::ClearTimeout()
{
	TimeoutSecs.Reset();
	StopTotalTimeoutTimer();
}

TOptional<float> FHttpRequestCommon::GetTimeout() const
{
	return TimeoutSecs;
}

float FHttpRequestCommon::GetTimeoutOrDefault() const
{
	return GetTimeout().Get(FHttpModule::Get().GetHttpTotalTimeout());
}

void FHttpRequestCommon::SetActivityTimeout(float InTimeoutSecs)
{
	ActivityTimeoutSecs = InTimeoutSecs;
}

const FHttpResponsePtr FHttpRequestCommon::GetResponse() const
{
	return ResponseCommon;
}

void FHttpRequestCommon::CancelRequest()
{
	bool bWasCanceled = bCanceled.exchange(true);
	if (bWasCanceled)
	{
		return;
	}

	StopActivityTimeoutTimer();

	StopPassingReceivedData();

	UE_LOG(LogHttp, Verbose, TEXT("HTTP request canceled. URL=%s"), *GetURL());

	FHttpModule::Get().GetHttpManager().AddHttpThreadTask([StrongThis = StaticCastSharedRef<FHttpRequestCommon>(AsShared())]()
	{
		// Run AbortRequest in HTTP thread to avoid potential concurrency issue
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FHttpRequestCommon_AbortRequest);
		StrongThis->AbortRequest();
	});
}

void FHttpRequestCommon::StartActivityTimeoutTimer()
{
	const FScopeLock CacheLock(&HttpTaskTimerHandleCriticalSection);

	if (bUsePlatformActivityTimeout)
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	static const bool bNoTimeouts = FParse::Param(FCommandLine::Get(), TEXT("NoTimeouts"));
	if (bNoTimeouts)
	{
		return;
	}
#endif

	if (bActivityTimedOut)
	{
		return;
	}

	float HttpActivityTimeout = GetActivityTimeoutOrDefault();
	if (HttpActivityTimeout == 0)
	{
		return;
	}

	StartActivityTimeoutTimerBy(HttpActivityTimeout);

	ResetActivityTimeoutTimer(TEXTVIEW("Connected"));
}

void FHttpRequestCommon::StartActivityTimeoutTimerBy(double DelayToTrigger)
{
	if (ActivityTimeoutHttpTaskTimerHandle != nullptr)
	{
		UE_LOG(LogHttp, Warning, TEXT("Request %p already started activity timeout timer"), this);
		return;
	}

	TWeakPtr<IHttpRequest> RequestWeakPtr(AsShared());
	ActivityTimeoutHttpTaskTimerHandle = FHttpModule::Get().GetHttpManager().AddHttpThreadTask([RequestWeakPtr]() {
		if (TSharedPtr<IHttpRequest> RequestPtr = RequestWeakPtr.Pin())
		{
			TSharedPtr<FHttpRequestCommon> RequestCommonPtr = StaticCastSharedPtr<FHttpRequestCommon>(RequestPtr);
			RequestCommonPtr->OnActivityTimeoutTimerTaskTrigger();
		}
	}, DelayToTrigger + 0.05);
}

void FHttpRequestCommon::OnActivityTimeoutTimerTaskTrigger()
{
	const FScopeLock CacheLock(&HttpTaskTimerHandleCriticalSection);

	ActivityTimeoutHttpTaskTimerHandle.Reset();

	if (EHttpRequestStatus::IsFinished(GetStatus()))
	{
		UE_LOG(LogHttp, Warning, TEXT("Request %p had finished when activity timeout timer trigger at [%s]"), this, *FDateTime::Now().ToString(TEXT("%H:%M:%S:%s")));
		return;
	}

	if (FPlatformTime::Seconds() < ActivityTimeoutAt)
	{
		// Check back later
		UE_LOG(LogHttp, VeryVerbose, TEXT("Request %p check response timeout at [%s], will check again in %.5f seconds"), this, *FDateTime::Now().ToString(TEXT("%H:%M:%S:%s")), ActivityTimeoutAt - FPlatformTime::Seconds());
		StartActivityTimeoutTimerBy(ActivityTimeoutAt - FPlatformTime::Seconds());
		return;
	}

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FHttpRequestCommon_AbortRequest);
	bActivityTimedOut = true;
	AbortRequest();
	UE_LOG(LogHttp, Log, TEXT("Request [%s] timed out at [%s] because of no responding for %0.2f seconds"), *GetURL(), *FDateTime::Now().ToString(TEXT("%H:%M:%S:%s")), GetActivityTimeoutOrDefault());
}

void FHttpRequestCommon::ResetActivityTimeoutTimer(FStringView Reason)
{
	const FScopeLock CacheLock(&HttpTaskTimerHandleCriticalSection);

	if (bUsePlatformActivityTimeout)
	{
		return;
	}

	if (!ActivityTimeoutHttpTaskTimerHandle)
	{
		return;
	}

	ActivityTimeoutAt = FPlatformTime::Seconds() + GetActivityTimeoutOrDefault();
	UE_LOG(LogHttp, VeryVerbose, TEXT("Request [%p] reset response timeout timer at %s: %s"), this, *FDateTime::Now().ToString(TEXT("%H:%M:%S:%s")), Reason.GetData());
}

void FHttpRequestCommon::StopActivityTimeoutTimer()
{
	const FScopeLock CacheLock(&HttpTaskTimerHandleCriticalSection);

	if (bUsePlatformActivityTimeout)
	{
		return;
	}

	if (!ActivityTimeoutHttpTaskTimerHandle)
	{
		return;
	}

	FHttpModule::Get().GetHttpManager().RemoveHttpThreadTask(ActivityTimeoutHttpTaskTimerHandle);
	ActivityTimeoutHttpTaskTimerHandle.Reset();
}

void FHttpRequestCommon::StartTotalTimeoutTimer()
{
	const FScopeLock CacheLock(&HttpTaskTimerHandleCriticalSection);

#if !UE_BUILD_SHIPPING
	static const bool bNoTimeouts = FParse::Param(FCommandLine::Get(), TEXT("NoTimeouts"));
	if (bNoTimeouts)
	{
		return;
	}
#endif

	float TimeoutOrDefault = GetTimeoutOrDefault();
	if (TimeoutOrDefault == 0)
	{
		return;
	}

	if (bTimedOut)
	{
		return;
	}

	// Timeout include retries, so if it's already started before, check this to prevent from adding timer multiple times
	if (TotalTimeoutHttpTaskTimerHandle)
	{
		return;
	}

	TWeakPtr<IHttpRequest> RequestWeakPtr(AsShared());
	TotalTimeoutHttpTaskTimerHandle = FHttpModule::Get().GetHttpManager().AddHttpThreadTask([RequestWeakPtr]() {
		if (TSharedPtr<IHttpRequest> RequestPtr = RequestWeakPtr.Pin())
		{
			TSharedPtr<FHttpRequestCommon> RequestCommonPtr = StaticCastSharedPtr<FHttpRequestCommon>(RequestPtr);
			RequestCommonPtr->OnTotalTimeoutTimerTaskTrigger();
		}
	}, TimeoutOrDefault);
}

void FHttpRequestCommon::OnTotalTimeoutTimerTaskTrigger()
{
	const FScopeLock CacheLock(&HttpTaskTimerHandleCriticalSection);
	bTimedOut = true;

	if (EHttpRequestStatus::IsFinished(GetStatus()))
	{
		return;
	}

	StopActivityTimeoutTimer();

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FHttpRequestCommon_AbortRequest);
	UE_LOG(LogHttp, Warning, TEXT("HTTP request timed out after %0.2f seconds URL=%s"), GetTimeoutOrDefault(), *GetURL());

	AbortRequest();
}

void FHttpRequestCommon::StopTotalTimeoutTimer()
{
	const FScopeLock CacheLock(&HttpTaskTimerHandleCriticalSection);

	if (TotalTimeoutHttpTaskTimerHandle)
	{
		FHttpModule::Get().GetHttpManager().RemoveHttpThreadTask(TotalTimeoutHttpTaskTimerHandle);
		TotalTimeoutHttpTaskTimerHandle.Reset();
	}
}

void FHttpRequestCommon::Shutdown()
{
	FHttpRequestImpl::Shutdown();

	StopPassingReceivedData();
	StopActivityTimeoutTimer();
	StopTotalTimeoutTimer();
}

void FHttpRequestCommon::ProcessRequestUntilComplete()
{
	checkf(!OnProcessRequestComplete().IsBound(), TEXT("OnProcessRequestComplete is not supported for sync call"));

	SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread);

	FEvent* Event = FPlatformProcess::GetSynchEventFromPool(true);
	OnProcessRequestComplete().BindLambda([Event](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) {
		Event->Trigger();
	});
	ProcessRequest();
	Event->Wait();
	FPlatformProcess::ReturnSynchEventToPool(Event);
}

void FHttpRequestCommon::TriggerStatusCodeReceivedDelegate(int32 StatusCode)
{
	if (DelegateThreadPolicy == EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread)
	{
		OnStatusCodeReceived().ExecuteIfBound(SharedThis(this), StatusCode);
	}
	else if (OnStatusCodeReceived().IsBound())
	{
		FHttpModule::Get().GetHttpManager().AddGameThreadTask([StrongThis = AsShared(), StatusCode]()
		{
			StrongThis->OnStatusCodeReceived().ExecuteIfBound(StrongThis, StatusCode);
		});
	}
}

void FHttpRequestCommon::SetEffectiveURL(const FString& InEffectiveURL)
{
	EffectiveURL = InEffectiveURL;

	if (ResponseCommon)
	{
		ResponseCommon->SetEffectiveURL(EffectiveURL);
	}
}

bool FHttpRequestCommon::SetResponseBodyReceiveStream(TSharedRef<FArchive> Stream)
{
	const FScopeLock StreamLock(&ResponseBodyReceiveStreamCriticalSection);

	ResponseBodyReceiveStream = Stream;
	bInitializedWithValidStream = true;
	return true;
}

bool FHttpRequestCommon::PassReceivedDataToStream(void* Ptr, int64 Length)
{
	const FScopeLock StreamLock(&ResponseBodyReceiveStreamCriticalSection);

	if (!ResponseBodyReceiveStream)
	{
		return false;
	}

	ResponseBodyReceiveStream->Serialize(Ptr, Length);

	return !ResponseBodyReceiveStream->GetError();
}

void FHttpRequestCommon::StopPassingReceivedData()
{
	const FScopeLock StreamLock(&ResponseBodyReceiveStreamCriticalSection);

	ResponseBodyReceiveStream = nullptr;
}

float FHttpRequestCommon::GetActivityTimeoutOrDefault() const
{
	return ActivityTimeoutSecs.Get(FHttpModule::Get().GetHttpActivityTimeout());
}

bool FHttpRequestCommon::SetContentAsStreamedFileDefaultImpl(const FString& Filename)
{
	UE_LOG(LogHttp, Verbose, TEXT("FHttpRequestCommon::SetContentAsStreamedFileDefaultImpl() - %s"), *Filename);

	if (CompletionStatus == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("FHttpRequestCommon::SetContentAsStreamedFileDefaultImpl() - attempted to set content on a request that is inflight"));
		return false;
	}

	RequestPayload = MakeUnique<FRequestPayloadInFileStream>(*Filename);
	return true;
}

bool FHttpRequestCommon::OpenRequestPayloadDefaultImpl()
{
	if (!RequestPayload)
	{
		return true;
	}

	if (!RequestPayload->Open())
	{
		return false;
	}

	if ((GetVerb().IsEmpty() || GetVerb().Equals(TEXT("GET"), ESearchCase::IgnoreCase)) && RequestPayload->GetContentLength() > 0)
	{
		UE_LOG(LogHttp, Warning, TEXT("An HTTP Get request cannot contain a payload."));
		return false;
	}

	return true;
}

void FHttpRequestCommon::CloseRequestPayloadDefaultImpl()
{
	if (RequestPayload.IsValid())
	{
		RequestPayload->Close();
	}
}
