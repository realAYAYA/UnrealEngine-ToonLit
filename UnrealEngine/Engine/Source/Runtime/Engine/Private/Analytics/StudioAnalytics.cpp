// Copyright Epic Games, Inc. All Rights Reserved.

#include "StudioAnalytics.h"
#include "HAL/PlatformTime.h"
#include "IAnalyticsProviderET.h"
#include "HAL/PlatformProcess.h"
#include "HAL/Thread.h"
#include "DerivedDataCacheInterface.h"
#include "DerivedDataCacheUsageStats.h"
#include "Virtualization/VirtualizationSystem.h"

#if WITH_EDITOR
#include "Experimental/ZenServerInterface.h"
#endif

bool FStudioAnalytics::bInitialized = false;
std::atomic<double> FStudioAnalytics::TimeEstimation { 0 };
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
			TimeEstimation.store(TimeEstimation.load(std::memory_order_relaxed) + FixedInterval);
		}
		else
		{
			TimeEstimation.store(TimeEstimation.load(std::memory_order_relaxed) + DeltaTime);
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
	return bInitialized ? TimeEstimation.load(std::memory_order_relaxed) : FPlatformTime::Seconds();
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
	const int SchemaVersion = 2;

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

	TArray<FAnalyticsEventAttribute> Attributes;

	Attributes.Emplace(TEXT("SchemaVersion"), SchemaVersion);
	Attributes.Emplace(TEXT("LoadingName"), LoadingName);
	Attributes.Emplace(TEXT("LoadingSeconds"), SecondsSpentLoading);
	Attributes.Append(InAttributes);

	if (FStudioAnalytics::IsAvailable())
	{
		FStudioAnalytics::GetProvider().RecordEvent(TEXT("Performance.Loading"), Attributes);
	}

#if ENABLE_COOK_STATS

	// Gather DDC analytics
	GetDerivedDataCacheRef().GatherAnalytics(Attributes);

	// Gather Virtualization analytics
	UE::Virtualization::IVirtualizationSystem::Get().GatherAnalytics(Attributes);

#if UE_WITH_ZEN
	// Gather Zen analytics
	if (UE::Zen::IsDefaultServicePresent())
	{
		UE::Zen::GetDefaultServiceInstance().GatherAnalytics(Attributes);
	}
#endif

	if (FStudioAnalytics::IsAvailable())
	{
		// Store it all in the loading event
		FStudioAnalytics::GetProvider().RecordEvent(TEXT("Core.Loading"), Attributes);
	}
#endif
}
