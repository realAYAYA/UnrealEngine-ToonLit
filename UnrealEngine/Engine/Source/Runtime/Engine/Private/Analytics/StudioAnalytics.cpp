// Copyright Epic Games, Inc. All Rights Reserved.

#include "StudioAnalytics.h"
#include "Misc/Guid.h"
#include "Stats/Stats.h"
#include "Misc/ConfigCacheIni.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Misc/EngineBuildSettings.h"
#include "AnalyticsBuildType.h"
#include "AnalyticsEventAttribute.h"
#include "IAnalyticsProviderET.h"
#include "AnalyticsET.h"
#include "GeneralProjectSettings.h"
#include "Misc/EngineVersion.h"
#include "RHI.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "Templates/SharedPointer.h"
#include "HAL/PlatformProcess.h"
#include "DerivedDataCacheInterface.h"
#include "DerivedDataCacheUsageStats.h"
#include "Virtualization/VirtualizationSystem.h"

#if WITH_EDITOR
#include "ZenServerInterface.h"
#endif

bool FStudioAnalytics::bInitialized = false;
volatile double FStudioAnalytics::TimeEstimation = 0;
FThread FStudioAnalytics::TimerThread;
TSharedPtr<IAnalyticsProviderET> FStudioAnalytics::Analytics;
TArray<FAnalyticsEventAttribute> FStudioAnalytics::DefaultAttributes;

void FStudioAnalytics::SetProvider(TSharedRef<IAnalyticsProviderET> InAnalytics)
{
	checkf(!Analytics.IsValid(), TEXT("FStudioAnalytics::SetProvider called more than once."));

	bInitialized = true;

	Analytics = InAnalytics;

	ApplyDefaultEventAttributes();

	TimeEstimation = FPlatformTime::Seconds();

	if (FPlatformProcess::SupportsMultithreading())
	{
		TimerThread = FThread(TEXT("Studio Analytics Timer Thread"), []() { RunTimer_Concurrent(); });
	}
}

void FStudioAnalytics::ApplyDefaultEventAttributes()
{
	if (Analytics.IsValid())
	{
		// Get the current attributes
		TArray<FAnalyticsEventAttribute> CurrentDefaultAttributes = Analytics->GetDefaultEventAttributesSafe();

		// Append any new attributes to our current ones
		CurrentDefaultAttributes += MoveTemp(DefaultAttributes);
		DefaultAttributes.Reset();

		// Set the new default attributes in the provider
		Analytics->SetDefaultEventAttributes(MoveTemp(CurrentDefaultAttributes));
	}	
}

void FStudioAnalytics::AddDefaultEventAttribute(const FAnalyticsEventAttribute& Attribute)
{
	// Append a single default attribute to the existing list
	DefaultAttributes.Emplace(Attribute);
}

void FStudioAnalytics::AddDefaultEventAttributes(TArray<FAnalyticsEventAttribute>&& Attributes)
{
	// Append attributes list to the existing list
	DefaultAttributes += MoveTemp(Attributes);
}

IAnalyticsProviderET& FStudioAnalytics::GetProvider()
{
	checkf(IsAvailable(), TEXT("FStudioAnalytics::GetProvider called outside of Initialize/Shutdown."));

	return *Analytics.Get();
}

void FStudioAnalytics::RunTimer_Concurrent()
{
	TimeEstimation = FPlatformTime::Seconds();

	const double FixedInterval = 0.0333333333334;
	const double BreakpointHitchTime = 1;

	while (bInitialized)
	{
		const double StartTime = FPlatformTime::Seconds();
		FPlatformProcess::Sleep((float)FixedInterval);
		const double EndTime = FPlatformTime::Seconds();
		const double DeltaTime = EndTime - StartTime;

		if (DeltaTime > BreakpointHitchTime)
		{
			TimeEstimation += FixedInterval;
		}
		else
		{
			TimeEstimation += DeltaTime;
		}
	}
}

void FStudioAnalytics::Tick(float DeltaSeconds)
{

}

void FStudioAnalytics::Shutdown()
{
	ensure(!Analytics.IsValid() || Analytics.IsUnique());
	Analytics.Reset();

	bInitialized = false;

	if (TimerThread.IsJoinable())
	{
		TimerThread.Join();
	}
}

double FStudioAnalytics::GetAnalyticSeconds()
{
	return bInitialized ? TimeEstimation : FPlatformTime::Seconds();
}

void FStudioAnalytics::RecordEvent(const FString& EventName)
{
	RecordEvent(EventName, TArray<FAnalyticsEventAttribute>());
}

void FStudioAnalytics::RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (FStudioAnalytics::IsAvailable())
	{
		FStudioAnalytics::GetProvider().RecordEvent(EventName, Attributes);
	}
}

void FStudioAnalytics::FireEvent_Loading(const FString& LoadingName, double SecondsSpentLoading, const TArray<FAnalyticsEventAttribute>& InAttributes)
{
	using namespace UE::Virtualization;

	// Ignore anything less than a 1/4th a second.
	if (SecondsSpentLoading < 0.250)
	{
		return;
	}

	// Throw out anything over 10 hours - 
	if (SecondsSpentLoading > 36000)
	{
		return;
	}

	if (FStudioAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> Attributes;

		
		Attributes.Emplace(TEXT("LoadingName"), LoadingName);
		Attributes.Emplace(TEXT("LoadingSeconds"), SecondsSpentLoading);
		Attributes.Append(InAttributes);

		FStudioAnalytics::GetProvider().RecordEvent(TEXT("Performance.Loading"), Attributes);

#if ENABLE_COOK_STATS

		TArray<FDerivedDataCacheResourceStat> DDCResourceStats;

		// Grab the latest resource stats
		GetDerivedDataCacheRef().GatherResourceStats(DDCResourceStats);

		FDerivedDataCacheResourceStat DDCResourceStatsTotal(TEXT("Total"));

		// Accumulate Totals
		for (const FDerivedDataCacheResourceStat& Stat : DDCResourceStats)
		{
			DDCResourceStatsTotal += Stat;
		}

		DDCResourceStats.Emplace(DDCResourceStatsTotal);

		for (const FDerivedDataCacheResourceStat& Stat : DDCResourceStats)
		{
			FString BaseName = TEXT("DDC.Resource.") + Stat.AssetType;

			BaseName = BaseName.Replace(TEXT("("), TEXT("")).Replace(TEXT(")"), TEXT(""));
			
			{
				FString AttrName = BaseName + TEXT(".BuildCount");
				Attributes.Emplace(MoveTemp(AttrName), Stat.BuildCount);
			}
			
			{
				FString AttrName = BaseName + TEXT(".BuildTimeSec");
				Attributes.Emplace(MoveTemp(AttrName), Stat.BuildTimeSec);
			}

			{
				FString AttrName = BaseName + TEXT(".BuildSizeMB");
				Attributes.Emplace(MoveTemp(AttrName), Stat.BuildSizeMB);
			}

			{
				FString AttrName = BaseName + TEXT(".LoadCount");
				Attributes.Emplace(MoveTemp(AttrName), Stat.LoadCount);
			}

			{
				FString AttrName = BaseName + TEXT(".LoadTimeSec");
				Attributes.Emplace(MoveTemp(AttrName), Stat.LoadTimeSec);
			}

			{
				FString AttrName = BaseName + TEXT(".LoadSizeMB");
				Attributes.Emplace(MoveTemp(AttrName), Stat.LoadSizeMB);
			}
		}

		// Grab the DDC summary stats
		{
			FDerivedDataCacheSummaryStats DDCSummaryStats;
			GetDerivedDataCacheRef().GatherSummaryStats(DDCSummaryStats);

			for (const FDerivedDataCacheSummaryStat& Stat : DDCSummaryStats.Stats)
			{
				FString FormattedAttrName = "DDC.Summary." + Stat.Key;
				Attributes.Emplace(FormattedAttrName, Stat.Value);
			}

			// Grab the Virtualization stats
			if (IVirtualizationSystem::IsInitialized())
			{
				IVirtualizationSystem& System = IVirtualizationSystem::Get();

				FPayloadActivityInfo PayloadActivityInfo = System.GetAccumualtedPayloadActivityInfo();

				const FString BaseName = TEXT("Virtualization");

				{
					FString AttrName = BaseName + TEXT(".Enabled");
					Attributes.Emplace(MoveTemp(AttrName), System.IsEnabled());
				}

				{
					FString AttrName = BaseName + TEXT(".Cache.TimeSpent");
					Attributes.Emplace(MoveTemp(AttrName), (double)PayloadActivityInfo.Cache.CyclesSpent * FPlatformTime::GetSecondsPerCycle());
				}

				{
					FString AttrName = BaseName + TEXT(".Cache.PayloadCount");
					Attributes.Emplace(MoveTemp(AttrName), (double)PayloadActivityInfo.Cache.PayloadCount);
				}

				{
					FString AttrName = BaseName + TEXT(".Cache.TotalBytes");
					Attributes.Emplace(MoveTemp(AttrName), (double)PayloadActivityInfo.Cache.TotalBytes);
				}

				{
					FString AttrName = BaseName + TEXT(".Push.TimeSpent");
					Attributes.Emplace(MoveTemp(AttrName), (double)PayloadActivityInfo.Push.CyclesSpent * FPlatformTime::GetSecondsPerCycle());
				}

				{
					FString AttrName = BaseName + TEXT(".Push.PayloadCount");
					Attributes.Emplace(MoveTemp(AttrName), (double)PayloadActivityInfo.Push.PayloadCount);
				}

				{
					FString AttrName = BaseName + TEXT(".Push.TotalBytes");
					Attributes.Emplace(MoveTemp(AttrName), (double)PayloadActivityInfo.Push.TotalBytes);
				}

				{
					FString AttrName = BaseName + TEXT(".Pull.TimeSpent");
					Attributes.Emplace(MoveTemp(AttrName), (double)PayloadActivityInfo.Pull.CyclesSpent * FPlatformTime::GetSecondsPerCycle());
				}

				{
					FString AttrName = BaseName + TEXT(".Pull.PayloadCount");
					Attributes.Emplace(MoveTemp(AttrName), (double)PayloadActivityInfo.Pull.PayloadCount);
				}

				{
					FString AttrName = BaseName + TEXT(".Pull.TotalBytes");
					Attributes.Emplace(MoveTemp(AttrName), (double)PayloadActivityInfo.Pull.TotalBytes);
				}
			}
		}

#if UE_WITH_ZEN
		if (UE::Zen::IsDefaultServicePresent())
		{
			// Grab the Zen summary stats
			UE::Zen::FZenStats ZenStats;
			UE::Zen::GetDefaultServiceInstance().GetStats(ZenStats);

			const FString BaseName = TEXT("Zen");

			{
				FString AttrName = BaseName + TEXT(".Enabled");
				Attributes.Emplace(MoveTemp(AttrName), ZenStats.IsValid);
			}

			{
				FString AttrName = BaseName + TEXT(".Cache.HitRatio");
				Attributes.Emplace(MoveTemp(AttrName), ZenStats.CacheStats.HitRatio);
			}

			{
				FString AttrName = BaseName + TEXT(".Cache.Hits");
				Attributes.Emplace(MoveTemp(AttrName), ZenStats.CacheStats.Hits);
			}

			{
				FString AttrName = BaseName + TEXT(".Cache.Misses");
				Attributes.Emplace(MoveTemp(AttrName), ZenStats.CacheStats.Misses);
			}

			{
				FString AttrName = BaseName + TEXT(".Cache.Size.Disk");
				Attributes.Emplace(MoveTemp(AttrName), ZenStats.CacheStats.Size.Disk);
			}

			{
				FString AttrName = BaseName + TEXT(".Cache.Size.Memory");
				Attributes.Emplace(MoveTemp(AttrName), ZenStats.CacheStats.Size.Memory);
			}

			{
				FString AttrName = BaseName + TEXT(".Cache.UpstreamHits");
				Attributes.Emplace(MoveTemp(AttrName), ZenStats.CacheStats.UpstreamHits);
			}

			{
				FString AttrName = BaseName + TEXT(".Cache.UpstreamRatio");
				Attributes.Emplace(MoveTemp(AttrName), ZenStats.CacheStats.UpstreamRatio);
			}

			{
				FString AttrName = BaseName + TEXT(".Cache.TotalUploadedMB");
				Attributes.Emplace(MoveTemp(AttrName), ZenStats.UpstreamStats.TotalUploadedMB);
			}

			{
				FString AttrName = BaseName + TEXT(".Upstream.TotalDownloadedMB");
				Attributes.Emplace(MoveTemp(AttrName), ZenStats.UpstreamStats.TotalDownloadedMB);
			}

			{
				FString AttrName = BaseName + TEXT(".Upstream.TotalUploadedMB");
				Attributes.Emplace(MoveTemp(AttrName), ZenStats.UpstreamStats.TotalUploadedMB);
			}

			{
				FString AttrName = BaseName + TEXT(".Cas.Size.Large");
				Attributes.Emplace(MoveTemp(AttrName), ZenStats.CASStats.Size.Large);
			}

			{
				FString AttrName = BaseName + TEXT(".Cas.Size.Small");
				Attributes.Emplace(MoveTemp(AttrName), ZenStats.CASStats.Size.Small);
			}

			{
				FString AttrName = BaseName + TEXT(".Cas.Size.Tiny");
				Attributes.Emplace(MoveTemp(AttrName), ZenStats.CASStats.Size.Tiny);
			}

			{
				FString AttrName = BaseName + TEXT(".Cas.Size.Total");
				Attributes.Emplace(MoveTemp(AttrName), ZenStats.CASStats.Size.Total);
			}
		}
#endif

		// Store it all in the loading event
		FStudioAnalytics::GetProvider().RecordEvent(TEXT("Core.Loading"), Attributes);
#endif
	}
}