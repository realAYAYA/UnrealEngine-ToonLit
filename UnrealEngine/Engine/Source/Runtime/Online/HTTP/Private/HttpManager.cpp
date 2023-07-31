// Copyright Epic Games, Inc. All Rights Reserved.

#include "HttpManager.h"
#include "HttpModule.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeLock.h"
#include "Http.h"
#include "Misc/App.h"
#include "Misc/Guid.h"
#include "Misc/Fork.h"
#include "HttpThread.h"
#include "IHttpThreadedRequest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"

#include "Stats/Stats.h"
#include "Containers/BackgroundableTicker.h"

// FHttpManager

FCriticalSection FHttpManager::RequestLock;

const TCHAR* LexToString(const EHttpFlushReason& FlushReason)
{
	switch (FlushReason)
	{
	case EHttpFlushReason::Default:		return TEXT("Default");
	case EHttpFlushReason::Background:	return TEXT("Background");
	case EHttpFlushReason::Shutdown:	return TEXT("Shutdown");
	case EHttpFlushReason::FullFlush:	return TEXT("FullFlush");
	}

	checkNoEntry();
	return TEXT("Invalid");
}

FHttpManager::FHttpManager()
	: FTSTickerObjectBase(0.0f, FTSBackgroundableTicker::GetCoreTicker())
	, Thread(nullptr)
	, CorrelationIdMethod(FHttpManager::GetDefaultCorrelationIdMethod())
{
	bFlushing = false;
}

FHttpManager::~FHttpManager()
{
	if (Thread)
	{
		Thread->StopThread();
		delete Thread;
	}
}

void FHttpManager::Initialize()
{
	if (FPlatformHttp::UsesThreadedHttp())
	{
		Thread = CreateHttpThread();
		Thread->StartThread();
	}

	UpdateConfigs();
}

void FHttpManager::ReloadFlushTimeLimits()
{
	FlushTimeLimitsMap.Reset();

	//Save int values of Default and FullFlush?
	for (EHttpFlushReason Reason : TEnumRange<EHttpFlushReason>())
	{
		double SoftLimitSeconds = 2.0;
		double HardLimitSeconds = 4.0;

		// We default the time limits to generous values, keeping the Hard limits always greater than the soft ones, and -1 for the unlimited
		switch (Reason)
		{
		case EHttpFlushReason::Default:
			GConfig->GetDouble(TEXT("HTTP"), TEXT("FlushSoftTimeLimitDefault"), SoftLimitSeconds, GEngineIni);
			GConfig->GetDouble(TEXT("HTTP"), TEXT("FlushHardTimeLimitDefault"), HardLimitSeconds, GEngineIni);
			break;
		case EHttpFlushReason::Background:
			GConfig->GetDouble(TEXT("HTTP"), TEXT("FlushSoftTimeLimitBackground"), SoftLimitSeconds, GEngineIni);
			GConfig->GetDouble(TEXT("HTTP"), TEXT("FlushHardTimeLimitBackground"), HardLimitSeconds, GEngineIni);
			break;
		case EHttpFlushReason::Shutdown:
			GConfig->GetDouble(TEXT("HTTP"), TEXT("FlushSoftTimeLimitShutdown"), SoftLimitSeconds, GEngineIni);
			GConfig->GetDouble(TEXT("HTTP"), TEXT("FlushHardTimeLimitShutdown"), HardLimitSeconds, GEngineIni);
			
			if ((HardLimitSeconds >= 0) && ((SoftLimitSeconds < 0) || (SoftLimitSeconds >= HardLimitSeconds)))
			{
				UE_CLOG(!IsRunningCommandlet(), LogHttp, Warning, TEXT("Soft limit[%.02f] is higher than the hard limit set[%.02f] in file [%s]. Please change the soft limit to a value lower than the hard limit for Flush to work correctly. - 1 is unlimited and therefore the highest possible value."), static_cast<float>(SoftLimitSeconds), static_cast<float>(HardLimitSeconds), *GEngineIni);
				// we need to be absolutely sure that SoftLimitSeconds is always strictly less than HardLimitSeconds so remaining requests (if any) can be canceled before exiting
				if (HardLimitSeconds > 0.0)
				{
					SoftLimitSeconds = HardLimitSeconds / 2.0;	// clamping SoftLimitSeconds to a reasonable value
				}
				else
				{
					// HardLimitSeconds should never be 0.0 while shutting down otherwise we can't cancel the remaining requests
					HardLimitSeconds = 0.05;	// using a non zero value 
					SoftLimitSeconds = 0.0;		// cancelling request immediately
				}
			}

			break;
		case EHttpFlushReason::FullFlush:
			SoftLimitSeconds = -1.0;
			HardLimitSeconds = -1.0;
			GConfig->GetDouble(TEXT("HTTP"), TEXT("FlushSoftTimeLimitFullFlush"), SoftLimitSeconds, GEngineIni);
			GConfig->GetDouble(TEXT("HTTP"), TEXT("FlushHardTimeLimitFullFlush"), HardLimitSeconds, GEngineIni);
			break;
		}

		FHttpFlushTimeLimit TimeLimit(SoftLimitSeconds, HardLimitSeconds);

		FlushTimeLimitsMap.Add(Reason, TimeLimit);
	}
}

void FHttpManager::SetCorrelationIdMethod(TFunction<FString()> InCorrelationIdMethod)
{
	check(InCorrelationIdMethod);
	CorrelationIdMethod = MoveTemp(InCorrelationIdMethod);
}

FString FHttpManager::CreateCorrelationId() const
{
	return CorrelationIdMethod();
}

bool FHttpManager::IsDomainAllowed(const FString& Url) const
{
#if !UE_BUILD_SHIPPING
#if !(UE_GAME || UE_SERVER)
	// Allowed domain filtering is opt-in in non-shipping non-game/server builds
	static const bool bForceUseAllowList = FParse::Param(FCommandLine::Get(), TEXT("EnableHttpDomainRestrictions"));
	if (!bForceUseAllowList)
	{
		return true;
	}
#else
	// The check is on by default but allow non-shipping game/server builds to disable the filtering
	static const bool bIgnoreAllowList = FParse::Param(FCommandLine::Get(), TEXT("DisableHttpDomainRestrictions"));
	if (bIgnoreAllowList)
	{
		return true;
	}
#endif
#endif // !UE_BUILD_SHIPPING

	// check to see if the Domain is allowed (either on the list or the list was empty)
	const TArray<FString>& AllowedDomains = FHttpModule::Get().GetAllowedDomains();
	if (AllowedDomains.Num() > 0)
	{
		const FString Domain = FPlatformHttp::GetUrlDomain(Url);
		for (const FString& AllowedDomain : AllowedDomains)
		{
			if (Domain.EndsWith(AllowedDomain))
			{
				return true;
			}
		}
		return false;
	}
	return true;
}

/*static*/
TFunction<FString()> FHttpManager::GetDefaultCorrelationIdMethod()
{
	return []{ return FGuid::NewGuid().ToString(); };
}

void FHttpManager::OnBeforeFork()
{
	Flush(EHttpFlushReason::Default);
}

void FHttpManager::OnAfterFork()
{

}

void FHttpManager::OnEndFramePostFork()
{
	// nothing
}


void FHttpManager::UpdateConfigs()
{
	ReloadFlushTimeLimits();

	if (Thread)
	{
		Thread->UpdateConfigs();
	}
}

void FHttpManager::AddGameThreadTask(TFunction<void()>&& Task)
{
	if (Task)
	{
		GameThreadQueue.Enqueue(MoveTemp(Task));
	}
}

FHttpThread* FHttpManager::CreateHttpThread()
{
	return new FHttpThread();
}

void FHttpManager::Flush(bool bShutdown)
{
	Flush(bShutdown ? EHttpFlushReason::Shutdown : EHttpFlushReason::Default);
}

namespace
{
	bool ShouldOutputHttpWarnings()
	{
		return !IsRunningCommandlet() && !FApp::IsUnattended();
	}
}

void FHttpManager::Flush(EHttpFlushReason FlushReason)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FHttpManager_Flush);

	FScopeLock ScopeLock(&RequestLock);
	
	// This variable is set to indicate that flush is happening.
	// While flushing is in progress, the RequestLock is held and threads are blocked when trying to submit new requests.
	bFlushing = true;

	double FlushTimeSoftLimitSeconds = FlushTimeLimitsMap[FlushReason].SoftLimitSeconds;
	double FlushTimeHardLimitSeconds = FlushTimeLimitsMap[FlushReason].HardLimitSeconds;

	// this specifies how long to sleep between calls to tick.
	// The smaller the value, the more quickly we may find out that all requests have completed, but the more work may be done in the meantime.
	float SecondsToSleepForOutstandingThreadedRequests = 0.5f;
	GConfig->GetFloat(TEXT("HTTP"), TEXT("RequestCleanupDelaySec"), SecondsToSleepForOutstandingThreadedRequests, GEngineIni);

	UE_CLOG(!IsRunningCommandlet(), LogHttp, Verbose, TEXT("[FHttpManager::Flush] FlushReason [%s] FlushTimeSoftLimitSeconds [%.3fs] FlushTimeHardLimitSeconds [%.3fs] SecondsToSleepForOutstandingThreadedRequests [%.3fs]"), LexToString(FlushReason), FlushTimeSoftLimitSeconds, FlushTimeHardLimitSeconds, SecondsToSleepForOutstandingThreadedRequests);

	// Clear all delegates bound to ongoing Http requests
	if (FlushReason == EHttpFlushReason::Shutdown)
	{
		// Don't emit these tracking logs in commandlet runs. Build system traps warnings during cook, and these are not truly fatal, but useful for tracking down shutdown issues.
		UE_CLOG(ShouldOutputHttpWarnings() && Requests.Num(), LogHttp, Warning, TEXT("[FHttpManager::Flush] FlushReason was Shutdown. Unbinding delegates for %d outstanding Http Requests:"), Requests.Num());

		// Clear delegates since they may point to deleted instances
		for (TArray<FHttpRequestRef>::TIterator It(Requests); It; ++It)
		{
			FHttpRequestRef& Request = *It;
			Request->OnProcessRequestComplete().Unbind();
			Request->OnRequestProgress().Unbind();
			Request->OnHeaderReceived().Unbind();

			// Don't emit these tracking logs in commandlet runs. Build system traps warnings during cook, and these are not truly fatal, but useful for tracking down shutdown issues.
			UE_CLOG(ShouldOutputHttpWarnings(), LogHttp, Warning, TEXT("	verb=[%s] url=[%s] refs=[%d] status=%s"), *Request->GetVerb(), *Request->GetURL(), Request.GetSharedReferenceCount(), EHttpRequestStatus::ToString(Request->GetStatus()));
		}
	}

	UE_CLOG(!IsRunningCommandlet() && Requests.Num(), LogHttp, Verbose, TEXT("[FHttpManager::Flush] Cleanup starts for %d outstanding Http Requests."), Requests.Num());

	double BeginWaitTime = FPlatformTime::Seconds();
	double LastFlushTickTime = BeginWaitTime;
	double StallWarnTime = BeginWaitTime + 0.5;
	double AppTime = FPlatformTime::Seconds();

	// For a duration equal to FlushTimeHardLimitSeconds, we wait for ongoing http requests to complete
	while (Requests.Num() > 0 && (FlushTimeHardLimitSeconds < 0 || (AppTime - BeginWaitTime < FlushTimeHardLimitSeconds)))
	{
		SCOPED_ENTER_BACKGROUND_EVENT(STAT_FHttpManager_Flush_Iteration);

		// If time equal to FlushTimeSoftLimitSeconds has passed and there's still ongoing http requests, we cancel them (setting FlushTimeSoftLimitSeconds to 0 does this immediately)
		if (FlushTimeSoftLimitSeconds >= 0 && (AppTime - BeginWaitTime >= FlushTimeSoftLimitSeconds))
		{
			// Don't emit these tracking logs in commandlet runs. Build system traps warnings during cook, and these are not truly fatal, but useful for tracking down shutdown issues.
			UE_CLOG(ShouldOutputHttpWarnings(), LogHttp, Warning, TEXT("[FHttpManager::Flush] FlushTimeSoftLimitSeconds [%.3fs] exceeded. Cancelling %d outstanding HTTP requests:"), FlushTimeSoftLimitSeconds, Requests.Num());

			for (TArray<FHttpRequestRef>::TIterator It(Requests); It; ++It)
			{
				FHttpRequestRef& Request = *It;

				// Don't emit these tracking logs in commandlet runs. Build system traps warnings during cook, and these are not truly fatal, but useful for tracking down shutdown issues.
				UE_CLOG(ShouldOutputHttpWarnings(), LogHttp, Warning, TEXT("	verb=[%s] url=[%s] refs=[%d] status=%s"), *Request->GetVerb(), *Request->GetURL(), Request.GetSharedReferenceCount(), EHttpRequestStatus::ToString(Request->GetStatus()));

				FScopedEnterBackgroundEvent(*Request->GetURL());

				Request->CancelRequest();
			}
		}

		// Process ongoing Http Requests
		FlushTick(AppTime - LastFlushTickTime);
		LastFlushTickTime = AppTime;

		// Process threaded Http Requests
		if (Requests.Num() > 0)
		{
			if (Thread)
			{
				if( Thread->NeedsSingleThreadTick() )
				{
					if (AppTime >= StallWarnTime)
					{
						// Don't emit these tracking logs in commandlet runs. Build system traps warnings during cook, and these are not truly fatal, but useful for tracking down shutdown issues.
						UE_CLOG(ShouldOutputHttpWarnings(), LogHttp, Warning, TEXT("	Ticking HTTPThread for %d outstanding Http requests."), Requests.Num());
						StallWarnTime = AppTime + 0.5;
					}
					Thread->Tick();
				}
				else
				{
					// Don't emit these tracking logs in commandlet runs. Build system traps warnings during cook, and these are not truly fatal, but useful for tracking down shutdown issues.
					UE_CLOG(ShouldOutputHttpWarnings(), LogHttp, Warning, TEXT("	Sleeping %.3fs to wait for %d outstanding Http Requests."), SecondsToSleepForOutstandingThreadedRequests, Requests.Num());
					FPlatformProcess::Sleep(SecondsToSleepForOutstandingThreadedRequests);
				}
			}
			else
			{
				check(!FPlatformHttp::UsesThreadedHttp());
			}
		}

		AppTime = FPlatformTime::Seconds();
	}

	UE_CLOG(!IsRunningCommandlet(), LogHttp, Verbose, TEXT("[FHttpManager::Flush] Cleanup ended after %.3fs. %d outstanding Http Requests."), AppTime - BeginWaitTime, Requests.Num());

	// Don't emit these tracking logs in commandlet runs. Build system traps warnings during cook, and these are not truly fatal, but useful for tracking down shutdown issues.
	if (Requests.Num() > 0 && (FlushTimeHardLimitSeconds > 0 && (AppTime - BeginWaitTime > FlushTimeHardLimitSeconds)) && ShouldOutputHttpWarnings())
	{
		UE_LOG(LogHttp, Warning, TEXT("[FHttpManager::Flush] FlushTimeHardLimitSeconds [%.3fs] exceeded. The following requests are being abandoned without being flushed:"), FlushTimeHardLimitSeconds);

		for (TArray<FHttpRequestRef>::TIterator It(Requests); It; ++It)
		{
			FHttpRequestRef& Request = *It;
			//List the outstanding requests that are being abandoned without being canceled.
			UE_LOG(LogHttp, Warning, TEXT("	verb=[%s] url=[%s] refs=[%d] status=%s"), *Request->GetVerb(), *Request->GetURL(), Request.GetSharedReferenceCount(), EHttpRequestStatus::ToString(Request->GetStatus()));
		}
	}

	bFlushing = false;
}

bool FHttpManager::Tick(float DeltaSeconds)
{
    QUICK_SCOPE_CYCLE_COUNTER(STAT_FHttpManager_Tick);

	// Run GameThread tasks
	TFunction<void()> Task = nullptr;
	while (GameThreadQueue.Dequeue(Task))
	{
		check(Task);
		Task();
	}

	FScopeLock ScopeLock(&RequestLock);

	// Tick each active request
	for (TArray<FHttpRequestRef>::TIterator It(Requests); It; ++It)
	{
		FHttpRequestRef Request = *It;
		Request->Tick(DeltaSeconds);
	}

	if (Thread)
	{
		TArray<IHttpThreadedRequest*> CompletedThreadedRequests;
		Thread->GetCompletedRequests(CompletedThreadedRequests);

		// Finish and remove any completed requests
		for (IHttpThreadedRequest* CompletedRequest : CompletedThreadedRequests)
		{
			FHttpRequestRef CompletedRequestRef = CompletedRequest->AsShared();
			Requests.Remove(CompletedRequestRef);
			CompletedRequest->FinishRequest();
		}
	}
	// keep ticking
	return true;
}

void FHttpManager::FlushTick(float DeltaSeconds)
{
	Tick(DeltaSeconds);
}

void FHttpManager::AddRequest(const FHttpRequestRef& Request)
{
	FScopeLock ScopeLock(&RequestLock);
	check(!bFlushing);
	Requests.Add(Request);
	RequestAddedDelegate.ExecuteIfBound(Request);
}

void FHttpManager::RemoveRequest(const FHttpRequestRef& Request)
{
	FScopeLock ScopeLock(&RequestLock);

	Requests.Remove(Request);
}

void FHttpManager::AddThreadedRequest(const TSharedRef<IHttpThreadedRequest, ESPMode::ThreadSafe>& Request)
{
	check(Thread);
	{
		AddRequest(Request);
	}
	Thread->AddRequest(&Request.Get());
}

void FHttpManager::CancelThreadedRequest(const TSharedRef<IHttpThreadedRequest, ESPMode::ThreadSafe>& Request)
{
	check(Thread);
	Thread->CancelRequest(&Request.Get());
}

bool FHttpManager::IsValidRequest(const IHttpRequest* RequestPtr) const
{
	FScopeLock ScopeLock(&RequestLock);

	bool bResult = false;
	for (const FHttpRequestRef& Request : Requests)
	{
		if (&Request.Get() == RequestPtr)
		{
			bResult = true;
			break;
		}
	}

	return bResult;
}

void FHttpManager::DumpRequests(FOutputDevice& Ar) const
{
	FScopeLock ScopeLock(&RequestLock);

	Ar.Logf(TEXT("------- (%d) Http Requests"), Requests.Num());
	for (const FHttpRequestRef& Request : Requests)
	{
		Ar.Logf(TEXT("	verb=[%s] url=[%s] status=%s"),
			*Request->GetVerb(), *Request->GetURL(), EHttpRequestStatus::ToString(Request->GetStatus()));
	}
}

bool FHttpManager::SupportsDynamicProxy() const
{
	return false;
}
