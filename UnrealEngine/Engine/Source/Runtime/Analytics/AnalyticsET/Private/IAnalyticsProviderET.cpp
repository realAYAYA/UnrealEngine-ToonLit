// Copyright Epic Games, Inc. All Rights Reserved.

#include "IAnalyticsProviderET.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeLock.h"
#include "Stats/Stats.h"
#include "Containers/Ticker.h"
#include "Misc/App.h"
#include "Misc/TimeGuard.h"

#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"
#include "AnalyticsProviderETEventCache.h"
#include "AnalyticsET.h"
#include "Analytics.h"
#include "Interfaces/IHttpResponse.h"
#include "Interfaces/IHttpRequest.h"
#include "HttpModule.h"
#include "PlatformHttp.h"
#include "Misc/EngineVersion.h"
#include "HttpRetrySystem.h"
#include "HAL/IConsoleManager.h"

#include "AnalyticsPerfTracker.h"

namespace AnalyticsProviderETCvars
{
	static bool PreventMultipleFlushesInOneFrame = true;
	FAutoConsoleVariableRef CvarPreventMultipleFlushesInOneFrame(
		TEXT("AnalyticsET.PreventMultipleFlushesInOneFrame"),
		PreventMultipleFlushesInOneFrame,
		TEXT("When true, prevents more than one AnalyticsProviderET instance from flushing in the same frame, allowing the flush and HTTP cost to be amortized.")
	);

	TAutoConsoleVariable<bool> CVarDefaultUserAgentCommentsEnabled(
		TEXT("AnalyticsET.UserAgentCommentsEnabled"),
		true,
		TEXT("Whether comments are supported in the analytics user agent string"),
		ECVF_SaveForNextBoot
	);
}

// Want to avoid putting the project name into the User-Agent, because for some apps (like the editor), the project name is private info.
// The analytics User-Agent uses the default User-Agent, but with project name removed.
class FAnalyticsUserAgentCache
{
public:
	FAnalyticsUserAgentCache()
		: CachedUserAgent()
		, CachedAgentVersion(0)
	{
	}

	FString GetUserAgent()
	{
		if (CachedUserAgent.IsEmpty() || CachedAgentVersion != FPlatformHttp::GetDefaultUserAgentVersion())
		{
			UpdateUserAgent();
		}

		return CachedUserAgent;
	}

private:
	void UpdateUserAgent()
	{
		static TSet<FString> AllowedProjectComments(GetAllowedProjectComments());
		static TSet<FString> AllowedPlatformComments(GetAllowedPlatformComments());

		FDefaultUserAgentBuilder Builder = FPlatformHttp::GetDefaultUserAgentBuilder();
		Builder.SetProjectName(TEXT("PROJECTNAME"));
		CachedUserAgent = Builder.BuildUserAgentString(&AllowedProjectComments, &AllowedPlatformComments);
		CachedAgentVersion = Builder.GetAgentVersion();
	}

	static TSet<FString> GetAllowedProjectComments()
	{
		TArray<FString> AllowedProjectComments;
		if (AnalyticsProviderETCvars::CVarDefaultUserAgentCommentsEnabled.GetValueOnAnyThread())
		{
			GConfig->GetArray(TEXT("Analytics"), TEXT("AllowedUserAgentProjectComments"), AllowedProjectComments, GEngineIni);
		}
		return TSet<FString>(MoveTemp(AllowedProjectComments));
	}

	static TSet<FString> GetAllowedPlatformComments()
	{
		TArray<FString> AllowedPlatformComments;
		if (AnalyticsProviderETCvars::CVarDefaultUserAgentCommentsEnabled.GetValueOnAnyThread())
		{
			GConfig->GetArray(TEXT("Analytics"), TEXT("AllowedUserAgentPlatformComments"), AllowedPlatformComments, GEngineIni);
		}
		return TSet<FString>(MoveTemp(AllowedPlatformComments));
	}

	static TSet<FString> ParseCommentSet(const FString& CommentBlob)
	{
		TArray<FString> Comments;
		CommentBlob.ParseIntoArray(Comments, TEXT(";"));
		return TSet<FString>(MoveTemp(Comments));
	}

	FString CachedUserAgent;
	uint32 CachedAgentVersion;
};

/**
 * Implementation of analytics for Epic Telemetry.
 * Supports caching events and flushing them periodically (currently hardcoded limits).
 * Also supports a set of default attributes that will be added to every event.
 * For efficiency, this set of attributes is added directly into the set of cached events
 * with a special flag to indicate its purpose. This allows the set of cached events to be used like
 * a set of commands to be executed on flush, and allows us to inject the default attributes
 * efficiently into many events without copying the array at all.
 * If Config.APIServerET is empty, this will act as a NULL provider by forcing ShouldRecordEvent() to return false all the time.
 */
class FAnalyticsProviderET :
	public IAnalyticsProviderET,
	public FTSTickerObjectBase,
	public TSharedFromThis<FAnalyticsProviderET>
{
public:
	FAnalyticsProviderET(const FAnalyticsET::Config& ConfigValues);

	// FTSTickerObjectBase

	bool Tick(float DeltaSeconds) override;

	// IAnalyticsProvider

	virtual bool StartSession(FString InSessionID, const TArray<FAnalyticsEventAttribute>& Attributes) override;
	virtual void EndSession() override;
	virtual void FlushEvents() override;

	virtual void SetAppID(FString&& AppID) override;
	virtual void SetAppVersion(FString&& AppVersion) override;
	virtual void SetUserID(const FString& InUserID) override;
	virtual FString GetUserID() const override;

	virtual FString GetSessionID() const override;
	virtual bool SetSessionID(const FString& InSessionID) override;

	virtual bool ShouldRecordEvent(const FString& EventName) const override;
	virtual void RecordEvent(FString&& EventName, const TArray<FAnalyticsEventAttribute>& Attributes) override;
	virtual void SetDefaultEventAttributes(TArray<FAnalyticsEventAttribute>&& Attributes) override;
	virtual TArray<FAnalyticsEventAttribute> GetDefaultEventAttributesSafe() const override;
	virtual int32 GetDefaultEventAttributeCount() const override;
	virtual FAnalyticsEventAttribute GetDefaultEventAttribute(int AttributeIndex) const override;
	virtual void SetEventCallback(const OnEventRecorded& Callback) override;

	virtual void SetURLEndpoint(const FString& UrlEndpoint, const TArray<FString>& AltDomains) override;
	virtual void SetHeader(const FString& HeaderName, const FString& HeaderValue) override;
	virtual void BlockUntilFlushed(float InTimeoutSec) override;
	virtual void SetShouldRecordEventFunc(const ShouldRecordEventFunction& InShouldRecordEventFunc) override;
	virtual ~FAnalyticsProviderET();

	virtual const FAnalyticsET::Config& GetConfig() const override { return Config; }

private:
	void FlushEventsOnce();
	void FlushEventLegacy(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes);
	bool IsActingAsNullProvider() const
	{
		// if we don't have a primary APIKey then we are essentially acting as a NULL provider and will suppress all events.
		// Don't bother checking the retry domains because the primary domain being empty is enough to tell us we have nowhere to send as a primary destination.
		return Config.APIServerET.IsEmpty();
	}

	/** Create a request utilizing HttpRetry domains */
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> CreateRequest();

	bool bSessionInProgress;
	/** The current configuration (might be updated with respect to the one provided at construction). */
	FAnalyticsET::Config Config;
	/** the unique UserID as passed to ET. */
	FString UserID;
	/** The session ID */
	FString SessionID;
	/** Default flush interval, when one is not explicitly given. */
	const float DefaultFlushIntervalSec = 60.0f;
	/** interval which to ensure events are flushed to the server. An event should not sit in the cache longer than this. It may be flushed sooner, but not longer (unless there is a hitch) */
	float FlushIntervalSec;
	/** Allows events to not be cached when -AnalyticsDisableCaching is used. This should only be used for debugging as caching significantly reduces bandwidth overhead per event. */
	bool bShouldCacheEvents;
	/** Current timer to keep track of FlushIntervalSec flushes */
	double NextEventFlushTime;
	/** Track destructing for unbinding callbacks when firing events at shutdown */
	bool bInDestructor;

	FAnalyticsProviderETEventCache EventCache;

	TArray<OnEventRecorded> EventRecordedCallbacks;

	/** Event filter function */
	ShouldRecordEventFunction ShouldRecordEventFunc;

	/**
	* Delegate called when an event Http request completes
	*/
	void EventRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded);

	TSharedPtr<class FHttpRetrySystem::FManager> HttpRetryManager;
	FHttpRetrySystem::FRetryDomainsPtr RetryServers;
	/** Http headers to add to requests */
	TMap<FString, FString> HttpHeaders;

	FAnalyticsUserAgentCache UserAgentCache;
};

TSharedPtr<IAnalyticsProviderET> FAnalyticsET::CreateAnalyticsProvider(const Config& ConfigValues) const
{
	// If we didn't have a proper APIKey, return NULL
	if (ConfigValues.APIKeyET.IsEmpty())
	{
		UE_LOG(LogAnalytics, Warning, TEXT("CreateAnalyticsProvider config not contain required parameter %s"), *Config::GetKeyNameForAPIKey());
		return NULL;
	}
	return MakeShared<FAnalyticsProviderET>(ConfigValues);
}

/**
 * Perform any initialization.
 */
FAnalyticsProviderET::FAnalyticsProviderET(const FAnalyticsET::Config& ConfigValues)
	: bSessionInProgress(false)
	, Config(ConfigValues)
	, FlushIntervalSec(ConfigValues.FlushIntervalSec < 0 ? DefaultFlushIntervalSec : ConfigValues.FlushIntervalSec)
	, bShouldCacheEvents(true)
	, NextEventFlushTime(FPlatformTime::Seconds() + FlushIntervalSec)
	, bInDestructor(false)
	// avoid preallocating space if we are using the legacy protocol.
	, EventCache(ConfigValues.MaximumPayloadSize, ConfigValues.UseLegacyProtocol ? 0 : ConfigValues.PreallocatedPayloadSize)
{
	if (Config.APIKeyET.IsEmpty())
	{
		UE_LOG(LogAnalytics, Fatal, TEXT("AnalyticsET: APIKey (%s) cannot be empty!"), *Config.APIKeyET);
	}

	// Set the number of retries to the number of retry URLs that have been passed in.
	uint32 RetryLimitCount = ConfigValues.AltAPIServersET.Num();

	HttpRetryManager = MakeShared<FHttpRetrySystem::FManager>(
		FHttpRetrySystem::FRetryLimitCountSetting(RetryLimitCount),
		FHttpRetrySystem::FRetryTimeoutRelativeSecondsSetting()
		);

	// If we have retry domains defined, insert the default domain into the list
	if (RetryLimitCount > 0)
	{
		TArray<FString> TmpAltAPIServers = ConfigValues.AltAPIServersET;

		FString DefaultUrlDomain = FPlatformHttp::GetUrlDomain(Config.APIServerET);
		if (!TmpAltAPIServers.Contains(DefaultUrlDomain))
		{
			TmpAltAPIServers.Insert(DefaultUrlDomain, 0);
		}

		RetryServers = MakeShared<FHttpRetrySystem::FRetryDomains, ESPMode::ThreadSafe>(MoveTemp(TmpAltAPIServers));
	}

	const bool bTestingMode = FParse::Param(FCommandLine::Get(), TEXT("TELEMETRYTESTING"));
	if (bTestingMode)
	{
		UE_SET_LOG_VERBOSITY(LogAnalytics, VeryVerbose);
		bShouldCacheEvents = false;
	}

	// force very verbose logging if we are force-disabling events.
	bool bForceDisableCaching = FParse::Param(FCommandLine::Get(), TEXT("ANALYTICSDISABLECACHING"));
	if (bForceDisableCaching)
	{
		UE_SET_LOG_VERBOSITY(LogAnalytics, VeryVerbose);
		bShouldCacheEvents = false;
	}

	UE_LOG(LogAnalytics, Verbose, TEXT("[%s] Initializing ET Analytics provider"), *Config.APIKeyET);

	// default to FEngineVersion::Current() if one is not provided, append FEngineVersion::Current() otherwise.
	FString ConfigAppVersion = ConfigValues.AppVersionET;
	// Allow the cmdline to force a specific AppVersion so it can be set dynamically.
	FParse::Value(FCommandLine::Get(), TEXT("ANALYTICSAPPVERSION="), ConfigAppVersion, false);
	Config.AppVersionET = ConfigAppVersion.IsEmpty()
		? FString(FApp::GetBuildVersion())
		: ConfigAppVersion.Replace(TEXT("%VERSION%"), FApp::GetBuildVersion(), ESearchCase::CaseSensitive);

	if ( Config.APIEndpointET.IsEmpty() )
	{
		// Set a default API Endpoint
		Config.APIEndpointET = TEXT("datarouter/api/v1/public/data");
	}

	UE_LOG(LogAnalytics, Display, TEXT("[%s] APIServer = %s%s. AppVersion = %s"), *Config.APIKeyET, *Config.APIServerET, *Config.APIEndpointET, *Config.AppVersionET);

	if (Config.APIServerET.IsEmpty())
	{
		UE_LOG(LogAnalytics, Warning, TEXT("AnalyticsET: APIServerET is empty for APIKey (%s), creating as a NULL provider!"), *Config.APIKeyET);
	}

	// only need these if we are using the data router protocol.
	if (!Config.UseLegacyProtocol)
	{
		Config.AppEnvironment = ConfigValues.AppEnvironment.IsEmpty()
			? FAnalyticsET::Config::GetDefaultAppEnvironment()
			: ConfigValues.AppEnvironment;
		Config.UploadType = ConfigValues.UploadType.IsEmpty()
			? FAnalyticsET::Config::GetDefaultUploadType()
			: ConfigValues.UploadType;
	}

	// see if there is a cmdline supplied UserID.
#if !UE_BUILD_SHIPPING
	FString ConfigUserID;
	if (FParse::Value(FCommandLine::Get(), TEXT("ANALYTICSUSERID="), ConfigUserID, false))
	{
		SetUserID(ConfigUserID);
	}
#endif // !UE_BUILD_SHIPPING
}

bool FAnalyticsProviderET::Tick(float DeltaSeconds)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FAnalyticsProviderET_Tick);

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	HttpRetryManager->Update();
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// hold a lock the entire time here because we're making several calls to the event cache that we need to be consistent when we decide to flush.
	// With more care, we can likely avoid holding this lock the entire time.
	FAnalyticsProviderETEventCache::Lock EventCacheLock(EventCache);

	// Countdown to flush
	const double Now = FPlatformTime::Seconds();

	// Never tick-flush more than one provider in a single frame. There's non-trivial overhead to flushing events.
	// On servers where there may be dozens of provider instances, this will spread out the cost a bit.
	// If caching is disabled, we still want events to be flushed immediately, so we are only guarding the flush calls from tick,
	// any other calls to flush are allowed to happen in the same frame.
	static uint64 LastFrameCounterFlushed = 0;

	const bool bHadFlushesQueued = EventCache.HasFlushesQueued();
	const bool bShouldFlush = bHadFlushesQueued || (EventCache.CanFlush() && Now >= NextEventFlushTime);

	if (bShouldFlush)
	{
		if (GFrameCounter == LastFrameCounterFlushed && AnalyticsProviderETCvars::PreventMultipleFlushesInOneFrame)
		{
			UE_LOG(LogAnalytics, Verbose, TEXT("[%s] Tried to flush, but another analytics provider has already flushed this frame. Deferring until next frame."), *Config.APIKeyET);
		}
		else
		{
			// Just flush one payload, even if we may have more than one queued.
			FlushEventsOnce();
			LastFrameCounterFlushed = GFrameCounter;
			// If we aren't flushing up a previous queued payload, then this was a regular interval flush, so we need to reset the timer.
			// try to keep on the same cadence when flushing, since we could miss our window by several frames.
			if (!bHadFlushesQueued && Now >= NextEventFlushTime)
			{
				const double Multiplier = FMath::Floor((Now - NextEventFlushTime) / FlushIntervalSec) + 1.;
				NextEventFlushTime += Multiplier * FlushIntervalSec;
			}
		}
	}
	return true;
}

FAnalyticsProviderET::~FAnalyticsProviderET()
{
	UE_LOG(LogAnalytics, Display, TEXT("[%s] Destroying ET Analytics provider"), *Config.APIKeyET);
	bInDestructor = true;
	EndSession();
	UE_LOG(LogAnalytics, Display, TEXT("[%s] Destroyed ET Analytics provider"), *Config.APIKeyET);
}

bool FAnalyticsProviderET::StartSession(FString InSessionID, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	UE_LOG(LogAnalytics, Log, TEXT("[%s] AnalyticsET::StartSession"), *Config.APIKeyET);

	// end/flush previous session before staring new one
	if (bSessionInProgress)
	{
		EndSession();
	}
	SessionID = MoveTemp(InSessionID);
	// always ensure we send a few specific attributes on session start.
	TArray<FAnalyticsEventAttribute> AttributesWithPlatform = Attributes;
	AttributesWithPlatform.Emplace(TEXT("Platform"), FString(FPlatformProperties::IniPlatformName()));

	RecordEvent(TEXT("SessionStart"), AttributesWithPlatform);
	bSessionInProgress = true;
	return bSessionInProgress;
}

/**
 * End capturing stats and queue the upload
 */
void FAnalyticsProviderET::EndSession()
{
	if (bSessionInProgress)
	{
		RecordEvent(TEXT("SessionEnd"), TArray<FAnalyticsEventAttribute>());
		UE_LOG(LogAnalytics, Display, TEXT("[%s] Ended ET Analytics provider session"), *Config.APIKeyET);
	}

	FlushEvents();
	SessionID.Empty();

	bSessionInProgress = false;
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> FAnalyticsProviderET::CreateRequest()
{
	if (!ensure(FModuleManager::Get().IsModuleLoaded("HTTP")))
	{
		UE_LOG(LogAnalytics, Display, TEXT("[%s] ET Analytics provider tried to create a new HTTP request when HTTP was shutdown"), *Config.APIKeyET);
	}

	// TODO add config values for retries, for now, using default
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = HttpRetryManager->CreateRequest(FHttpRetrySystem::FRetryLimitCountSetting(),
		FHttpRetrySystem::FRetryTimeoutRelativeSecondsSetting(),
		FHttpRetrySystem::FRetryResponseCodes(),
		FHttpRetrySystem::FRetryVerbs(),
		RetryServers);
	for (const TPair<FString, FString>& HttpHeader : HttpHeaders)
	{
		HttpRequest->SetHeader(HttpHeader.Key, HttpHeader.Value);
	}

	return HttpRequest;
}

void FAnalyticsProviderET::FlushEvents()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FAnalyticsProviderET_FlushEvents);
	// Warn if this takes more than 2 ms
	SCOPE_TIME_GUARD_MS(TEXT("FAnalyticsProviderET::FlushEvents"), 2);

	// keep flushing until the event cache has cleared its queue.
	while (EventCache.CanFlush())
	{
		FlushEventsOnce();
	}
}

void FAnalyticsProviderET::FlushEventsOnce()
{
	// Make sure we don't try to flush too many times. When we are not caching events it's possible this can be called when there are no events in the array.
	if (!EventCache.CanFlush())
	{
		return;
	}

	ANALYTICS_FLUSH_TRACKING_BEGIN();
	int EventCount = 0;
	int PayloadSize = 0;

	if(ensure(FModuleManager::Get().IsModuleLoaded("HTTP")))
	{
		TArray<uint8> Payload = EventCache.FlushCacheUTF8();
		// UrlEncode NOTE: need to concatenate everything
		FString URLPath = Config.APIEndpointET;
				URLPath += TEXT("?SessionID=") + FPlatformHttp::UrlEncode(SessionID);
				URLPath += TEXT("&AppID=") + FPlatformHttp::UrlEncode(Config.APIKeyET);
				URLPath += TEXT("&AppVersion=") + FPlatformHttp::UrlEncode(Config.AppVersionET);
				URLPath += TEXT("&UserID=") + FPlatformHttp::UrlEncode(UserID);
				URLPath += TEXT("&AppEnvironment=") + FPlatformHttp::UrlEncode(Config.AppEnvironment);
				URLPath += TEXT("&UploadType=") + FPlatformHttp::UrlEncode(Config.UploadType);
		PayloadSize = URLPath.Len() + Payload.Num();

		// This should never be done in production. MUCH slower!
		if (UE_LOG_ACTIVE(LogAnalytics, VeryVerbose))
		{
			// need to null terminate to load the payload.
			Payload.Add(TEXT('\0'));
			// Recreate the URLPath for logging because we do not want to escape the parameters when logging.
			// We cannot simply UrlEncode the entire Path after logging it because UrlEncode(Params) != UrlEncode(Param1) & UrlEncode(Param2) ...
			FString LogString = FString::Printf(TEXT("[%s] AnalyticsET URL:%s?SessionID=%s&AppID=%s&AppVersion=%s&UserID=%s&AppEnvironment=%s&UploadType=%s. Payload:%s"),
				*Config.APIKeyET,
				*Config.APIEndpointET,
				*SessionID,
				*Config.APIKeyET,
				*Config.AppVersionET,
				*UserID,
				*Config.AppEnvironment,
				*Config.UploadType,
				UTF8_TO_TCHAR(Payload.GetData()));
			UE_LOG(LogAnalytics, VeryVerbose, TEXT("%s"), *LogString);
			Payload.SetNum(Payload.Num()-1);
		}

		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FlushEventsHttpRequest);
			// Create/send Http request for an event
			TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = CreateRequest();
			HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json; charset=utf-8"));
			// Want to avoid putting the project name into the User-Agent, because for some apps (like the editor), the project name is private info.
			// The analytics User-Agent uses the default User-Agent, but with project name removed.
			HttpRequest->SetHeader(TEXT("User-Agent"), UserAgentCache.GetUserAgent());
			HttpRequest->SetURL(Config.APIServerET / URLPath);
			HttpRequest->SetVerb(TEXT("POST"));
			HttpRequest->SetContent(MoveTemp(Payload));

			// Don't set a response callback if we are in our destructor, as the instance will no longer be there to call.
			if (!bInDestructor)
			{
				HttpRequest->OnProcessRequestComplete().BindSP(this, &FAnalyticsProviderET::EventRequestComplete);
			}

			HttpRequest->ProcessRequest();
		}
	}
	ANALYTICS_FLUSH_TRACKING_END(PayloadSize, EventCount);
}

void FAnalyticsProviderET::SetAppID(FString&& InAppID)
{
	if (Config.APIKeyET != InAppID)
	{
		// Flush any cached events that would be using the old AppID.
		FlushEvents();
		Config.APIKeyET = MoveTemp(InAppID);
	}
}

void FAnalyticsProviderET::SetAppVersion(FString&& InAppVersion)
{
	// make sure to do the version replacement if the given string is parameterized.
	InAppVersion = InAppVersion.IsEmpty()
		? FString(FApp::GetBuildVersion())
		: InAppVersion.Replace(TEXT("%VERSION%"), FApp::GetBuildVersion(), ESearchCase::CaseSensitive);

	if (Config.AppVersionET != InAppVersion)
	{
		UE_LOG(LogAnalytics, Log, TEXT("[%s] Updating AppVersion to %s from old value of %s"), *Config.APIKeyET, *InAppVersion, *Config.AppVersionET);
		// Flush any cached events that would be using the old AppVersion.
		FlushEvents();
		Config.AppVersionET = MoveTemp(InAppVersion);
	}
}

void FAnalyticsProviderET::SetUserID(const FString& InUserID)
{
	// command-line specified user ID overrides all attempts to reset it.
	if (!FParse::Value(FCommandLine::Get(), TEXT("ANALYTICSUSERID="), UserID, false))
	{
		UE_LOG(LogAnalytics, Log, TEXT("[%s] SetUserId %s"), *Config.APIKeyET, *InUserID);
		// Flush any cached events that would be using the old UserID.
		FlushEvents();
		UserID = InUserID;
	}
	else if (UserID != InUserID)
	{
		UE_LOG(LogAnalytics, Log, TEXT("[%s] Overriding SetUserId %s with cmdline UserId of %s."), *Config.APIKeyET, *InUserID, *UserID);
	}
}

FString FAnalyticsProviderET::GetUserID() const
{
	return UserID;
}

FString FAnalyticsProviderET::GetSessionID() const
{
	return SessionID;
}

bool FAnalyticsProviderET::SetSessionID(const FString& InSessionID)
{
	if (SessionID != InSessionID)
	{
		// Flush any cached events that would be using the old SessionID.
		FlushEvents();
		SessionID = InSessionID;
		UE_LOG(LogAnalytics, Log, TEXT("[%s] Forcing SessionID to %s."), *Config.APIKeyET, *SessionID);
	}
	return true;
}

bool FAnalyticsProviderET::ShouldRecordEvent(const FString& EventName) const
{
	return !IsActingAsNullProvider() && (!ShouldRecordEventFunc || ShouldRecordEventFunc(*this, EventName));
}

void FAnalyticsProviderET::RecordEvent(FString&& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	// let higher level code filter the decision of whether to send the event
	if (ShouldRecordEvent(EventName))
	{
		// fire any callbacks
		for (const auto& Cb : EventRecordedCallbacks)
		{
			// we no longer track if the event was Json, each attribute does.
			Cb(EventName, Attributes, false);
		}

		if (!Config.UseLegacyProtocol)
		{
			EventCache.AddToCache(MoveTemp(EventName), Attributes);

			// if we aren't caching events, flush immediately. This is really only for debugging as it will significantly affect bandwidth.
			if (!bShouldCacheEvents)
			{
				FlushEvents();
			}
		}
		else
		{
			FlushEventLegacy(EventName, Attributes);
		}
	}
	else
	{
		UE_LOG(LogAnalytics, Verbose, TEXT("Ignoring event named '%s' due to ShouldRecordEvent check"), *EventName);
	}
}

void FAnalyticsProviderET::SetDefaultEventAttributes(TArray<FAnalyticsEventAttribute>&& Attributes)
{
	EventCache.SetDefaultAttributes(MoveTemp(Attributes));
}

TArray<FAnalyticsEventAttribute> FAnalyticsProviderET::GetDefaultEventAttributesSafe() const
{
	return EventCache.GetDefaultAttributes();
}

int32 FAnalyticsProviderET::GetDefaultEventAttributeCount() const
{
	return EventCache.GetDefaultAttributeCount();
}


FAnalyticsEventAttribute FAnalyticsProviderET::GetDefaultEventAttribute(int AttributeIndex) const
{
	return EventCache.GetDefaultAttribute(AttributeIndex);
}

void FAnalyticsProviderET::SetEventCallback(const OnEventRecorded& Callback)
{
	EventRecordedCallbacks.Add(Callback);
}

void FAnalyticsProviderET::EventRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool)
{
	// process responses
	bool bEventsDelivered = false;
	if (HttpResponse.IsValid())
	{
		UE_LOG(LogAnalytics, VeryVerbose, TEXT("[%s] ET response for [%s]. Code: %d. Payload: %s"), *Config.APIKeyET, *HttpRequest->GetURL(), HttpResponse->GetResponseCode(), *HttpResponse->GetContentAsString());
		if (EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
		{
			bEventsDelivered = true;
		}
	}
	else
	{
		UE_LOG(LogAnalytics, VeryVerbose, TEXT("[%s] ET response for [%s]. No response"), *Config.APIKeyET, *HttpRequest->GetURL());
	}
}

void FAnalyticsProviderET::SetURLEndpoint(const FString& UrlEndpoint, const TArray<FString>& AltDomains)
{
	// See if anything is actually changing before going through the work to flush and reset the URLs.
	if (Config.APIServerET == UrlEndpoint && Config.AltAPIServersET == AltDomains)
	{
		return;
	}

	// flush existing events before changing URL domains.
	FlushEvents();

	Config.APIServerET = UrlEndpoint;
	Config.AltAPIServersET = AltDomains;

	// Set the number of retries to the number of retry URLs that have been passed in.
	uint32 RetryLimitCount = AltDomains.Num();

	HttpRetryManager->SetDefaultRetryLimit(RetryLimitCount);

	TArray<FString> TmpAltAPIServers = AltDomains;

	// If we have retry domains defined, insert the default domain into the list
	if (RetryLimitCount > 0)
	{
		FString DefaultUrlDomain = FPlatformHttp::GetUrlDomain(Config.APIServerET);
		if (!TmpAltAPIServers.Contains(DefaultUrlDomain))
		{
			TmpAltAPIServers.Insert(DefaultUrlDomain, 0);
		}

		RetryServers = MakeShared<FHttpRetrySystem::FRetryDomains, ESPMode::ThreadSafe>(MoveTemp(TmpAltAPIServers));
	}
	else
	{
		RetryServers.Reset();
	}

	if (Config.APIServerET.IsEmpty())
	{
		UE_LOG(LogAnalytics, Warning, TEXT("AnalyticsET: APIServerET is empty for APIKey (%s), converting to a NULL provider!"), *Config.APIKeyET);
	}
}

void FAnalyticsProviderET::SetHeader(const FString& HeaderName, const FString& HeaderValue)
{
	if (HeaderValue.IsEmpty())
	{
		HttpHeaders.Remove(HeaderName);
	}
	else
	{
		HttpHeaders.Emplace(HeaderName, HeaderValue);
	}
}

void FAnalyticsProviderET::BlockUntilFlushed(float InTimeoutSec)
{
	FlushEvents();
	HttpRetryManager->BlockUntilFlushed(InTimeoutSec);
}

void FAnalyticsProviderET::SetShouldRecordEventFunc(const ShouldRecordEventFunction& InShouldRecordEventFunc)
{
	ShouldRecordEventFunc = InShouldRecordEventFunc;
}

static inline void AnalyticsProviderETFlushEventLegacyHelper(FString& EventParams, int PayloadNdx, const FAnalyticsEventAttribute& Attribute)
{
	EventParams += FString::Printf(TEXT("&AttributeName%d=%s&AttributeValue%d=%s"),
		PayloadNdx,
		*FPlatformHttp::UrlEncode(Attribute.GetName()),
		PayloadNdx,
		*FPlatformHttp::UrlEncode(Attribute.GetValue()));
}

void FAnalyticsProviderET::FlushEventLegacy(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	// this is a legacy pathway that doesn't accept batch payloads of cached data. We'll just send one request for each event, which will be slow for a large batch of requests at once.
	if (ensure(FModuleManager::Get().IsModuleLoaded("HTTP")))
	{
		// first generate a payload from the eventand attributes
		FString EventParams;
		int PayloadNdx = 0;
		for (int DefaultNdx = 0, NumDefaults = EventCache.GetDefaultAttributeCount(); DefaultNdx < NumDefaults; ++DefaultNdx)
		{
			AnalyticsProviderETFlushEventLegacyHelper(EventParams, PayloadNdx, EventCache.GetDefaultAttribute(DefaultNdx));
			++PayloadNdx;
		}
		for (int AttrNdx = 0; AttrNdx < Attributes.Num(); ++AttrNdx)
		{
			AnalyticsProviderETFlushEventLegacyHelper(EventParams, PayloadNdx, Attributes[AttrNdx]);
			++PayloadNdx;
		}

		// log out the un-encoded values to make reading the log easier.
		UE_LOG(LogAnalytics, VeryVerbose, TEXT("[%s] AnalyticsET URL:SendEvent.1?SessionID=%s&AppID=%s&AppVersion=%s&UserID=%s&EventName=%s%s"),
			*Config.APIKeyET,
			*SessionID,
			*Config.APIKeyET,
			*Config.AppVersionET,
			*UserID,
			*EventName,
			*EventParams);

		// Create/send Http request for an event
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = CreateRequest();
		HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("text/plain"));

		// Don't need to URL encode the APIServer or the EventParams, which are already encoded, and contain parameter separaters that we DON'T want encoded.
		FString URLPath = Config.APIServerET+Config.APIEndpointET;
		URLPath += TEXT("SendEvent.1?SessionID=") + FPlatformHttp::UrlEncode(SessionID);
		URLPath += TEXT("&AppID=") + FPlatformHttp::UrlEncode(Config.APIKeyET);
		URLPath += TEXT("&AppVersion=") + FPlatformHttp::UrlEncode(Config.AppVersionET);
		URLPath += TEXT("&UserID=") + FPlatformHttp::UrlEncode(UserID);
		URLPath += TEXT("&EventName=") + FPlatformHttp::UrlEncode(EventName);
		URLPath += EventParams;
		HttpRequest->SetURL(URLPath);
		HttpRequest->SetVerb(TEXT("GET"));
		if (!bInDestructor)
		{
			HttpRequest->OnProcessRequestComplete().BindSP(this, &FAnalyticsProviderET::EventRequestComplete);
		}
		HttpRequest->ProcessRequest();
	}
}
