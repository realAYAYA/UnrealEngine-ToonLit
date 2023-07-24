// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Build.h"

/** When enabled (and -AnalyticsTrackPerf is specified on the command line, will log out analytics flush timings on a regular basis to Saved/AnalyticsTiming.csv. */
#define ANALYTICS_PERF_TRACKING_ENABLED !UE_BUILD_SHIPPING
#if ANALYTICS_PERF_TRACKING_ENABLED

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Misc/OutputDeviceFile.h"
#include "Misc/Paths.h"
#include "Misc/LazySingleton.h"
#include "HAL/PlatformTime.h"

/** Measures analytics bandwidth. Only active when -AnalyticsTrackPerf is specified on the command line. */
struct FAnalyticsPerfTracker : FTSTickerObjectBase
{
	FAnalyticsPerfTracker();

	/** Called once per flush */
	void RecordFlush(uint64 Bytes, uint64 NumEvents, double TimeSec);

	bool IsEnabled() const;

	void SetRunID(const FString& InRunID);

private:
	/** Check to see if we need to log another window of time. */
	virtual bool Tick(float DeltaTime) override;

	/** Helper to reset our window in Tick. */
	bool WindowExpired(double Now);

	/** Helper to reset our window in Tick. */
	void ResetWindow(double Now);

	/** log file to use. */
	FOutputDeviceFile LogFile{ *FPaths::Combine(*FPaths::ProjectSavedDir(), TEXT("AnalyticsTiming.csv")) };
	FString StartDate;
	FString CL;
	FString RunID = FGuid().ToString().ToLower();
	// Window tracking data
	double LastSubmitTime = 0.0;
	double TimeThisWindow = 0.0;
	uint64 BytesThisWindow = 0;
	uint64 NumEventsThisWindow = 0;
	int FlushesThisWindow = 0;
	int FramesThisWindow = 0;
	// time when the first measurement was made.
	double StartTime = FPlatformTime::Seconds();
	/** Controls whether metrics gathering is enabled. */
	bool bEnabled = false;
};

/** Used to set the RunID between matches in game code. Must be carefully called only in situations where ANALYTICS_PERF_TRACKING_ENABLED = 1 */
ANALYTICSET_API void SetAnayticsETPerfTrackingRunID(const FString& RunID);

/** Used to get the analytics perf tracker singleton */
ANALYTICSET_API FAnalyticsPerfTracker& GetAnalyticsPerfTracker();

/** Used to tear down the analytics perf tracker singleton */
ANALYTICSET_API void TearDownAnalyticsPerfTracker();

#define ANALYTICS_FLUSH_TRACKING_BEGIN() double FlushStartTime = FPlatformTime::Seconds()
#define ANALYTICS_FLUSH_TRACKING_END(NumBytes, NumEvents) GetAnalyticsPerfTracker().RecordFlush(NumBytes, NumEvents, FPlatformTime::Seconds() - FlushStartTime)

#else

#define ANALYTICS_FLUSH_TRACKING_BEGIN(...)
#define ANALYTICS_FLUSH_TRACKING_END(...)

#endif