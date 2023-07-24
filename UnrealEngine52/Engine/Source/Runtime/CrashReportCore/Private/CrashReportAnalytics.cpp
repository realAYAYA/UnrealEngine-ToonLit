// Copyright Epic Games, Inc. All Rights Reserved.

#include "CrashReportAnalytics.h"
#include "Misc/Guid.h"
#include "Misc/EngineBuildSettings.h"

#include "AnalyticsET.h"
#include "IAnalyticsProviderET.h"
#include "AnalyticsBuildType.h"

bool FCrashReportAnalytics::bIsInitialized;
TSharedPtr<IAnalyticsProviderET> FCrashReportAnalytics::Analytics;

/**
 * Default config func that essentially tells the crash reporter to disable analytics.
 */
FAnalyticsET::Config DefaultAnalyticsConfigFunc()
{
	return FAnalyticsET::Config();
}

/**
* Engine analytics config to initialize the crash reporter analytics provider.
* External code should bind this delegate if crash reporter analytics are desired,
* preferably in private code that won't be redistributed.
*/
TFunction<FAnalyticsET::Config()>& GetCrashReportAnalyticsConfigFunc()
{
	static TFunction<FAnalyticsET::Config()> Config = &DefaultAnalyticsConfigFunc;
	return Config;
}

/**
 * On-demand construction of the singleton. 
 */
IAnalyticsProviderET& FCrashReportAnalytics::GetProvider()
{
	checkf(bIsInitialized && Analytics.IsValid(), TEXT("FCrashReportAnalytics::GetProvider called outside of Initialize/Shutdown."));
	return *Analytics.Get();
}
 
void FCrashReportAnalytics::Initialize()
{
	checkf(!bIsInitialized, TEXT("FCrashReportAnalytics::Initialize called more than once."));

	// Allow build machines to force CRC to enable internal telemetry.
	#if defined(CRC_TELEMETRY_URL) && defined(CRC_TELEMETRY_KEY_DEV) && defined(CRC_TELEMETRY_KEY_RELEASE)

		// We always use the "Release" analytics account unless we're running in analytics test mode (usually with
		// a command-line parameter), or we're an internal Epic build
		bool bUseReleaseAccount =
			(GetAnalyticsBuildType() == EAnalyticsBuildType::Development || GetAnalyticsBuildType() == EAnalyticsBuildType::Release) &&
			!FEngineBuildSettings::IsInternalBuild();	// Internal Epic build

		FAnalyticsET::Config Config;
		Config.APIServerET = TEXT(CRC_TELEMETRY_URL);
		Config.APIKeyET = bUseReleaseAccount ? TEXT(CRC_TELEMETRY_KEY_RELEASE) : TEXT(CRC_TELEMETRY_KEY_DEV);

	#else

		FAnalyticsET::Config Config = GetCrashReportAnalyticsConfigFunc()();

	#endif

	if (!Config.APIServerET.IsEmpty())
	{
		// Connect the engine analytics provider (if there is a configuration delegate installed)
		Analytics = FAnalyticsET::Get().CreateAnalyticsProvider(Config);
		if( Analytics.IsValid() )
		{
			Analytics->SetUserID(FString::Printf(TEXT("%s|%s|%s"), *FPlatformMisc::GetLoginId(), *FPlatformMisc::GetEpicAccountId(), *FPlatformMisc::GetOperatingSystemId()));
			Analytics->StartSession();
		}
	}
	bIsInitialized = true;
}


void FCrashReportAnalytics::Shutdown()
{
	checkf(bIsInitialized, TEXT("FCrashReportAnalytics::Shutdown called outside of Initialize."));
	Analytics.Reset();
	bIsInitialized = false;
}
