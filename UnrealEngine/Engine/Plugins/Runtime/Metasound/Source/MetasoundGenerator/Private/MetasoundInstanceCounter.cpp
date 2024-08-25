// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundInstanceCounter.h"

namespace Metasound
{
	FConcurrentInstanceCounterManager::FConcurrentInstanceCounterManager(const FString InCategoryName)
	: CategoryName(InCategoryName)
	{
	}

	void FConcurrentInstanceCounterManager::Increment(const FName& InstanceName)
	{
		FScopeLock Lock(&MapCritSec);
		GetOrAddStats(InstanceName).Increment();
	}

	void FConcurrentInstanceCounterManager::Decrement(const FName& InstanceName)
	{
		FScopeLock Lock(&MapCritSec);
		GetOrAddStats(InstanceName).Decrement();
	}

	int64 FConcurrentInstanceCounterManager::GetCountForName(const FName& InName)
	{
		FScopeLock Lock(&MapCritSec);
		if (FStats* Stats = StatsMap.Find(InName))
		{
			return Stats->GetCount();
		}

		return 0;
	}

	int64 FConcurrentInstanceCounterManager::GetPeakCountForName(const FName& InName)
	{
		FScopeLock Lock(&MapCritSec);
		if (FStats* Stats = StatsMap.Find(InName))
		{
			return Stats->GetPeakCount();
		}

		return 0;
	}

#if COUNTERSTRACE_ENABLED
	FConcurrentInstanceCounterManager::FStats::FStats(const FString& InName)
	: TraceCounter(MakeUnique<FCountersTrace::FCounterInt>(TraceCounterNameType_Dynamic, *InName, TraceCounterDisplayHint_None))
	{
	}
#endif

	void FConcurrentInstanceCounterManager::FStats::Increment()
	{
		ensure(TraceCounter);
		TraceCounter->Increment();
		PeakCount = FMath::Max(PeakCount, GetCount());
	}


	void FConcurrentInstanceCounterManager::FStats::Decrement()
	{
		ensure(TraceCounter);
		TraceCounter->Decrement();
	}

	int64 FConcurrentInstanceCounterManager::FStats::GetCount()
	{
		ensure(TraceCounter);
#if COUNTERSTRACE_ENABLED
		return TraceCounter->Get();
#else
		return TraceCounter->GetValue();
#endif
	}

	int64 FConcurrentInstanceCounterManager::FStats::GetPeakCount()
	{
		return PeakCount;
	}

	FConcurrentInstanceCounterManager::FStats& FConcurrentInstanceCounterManager::GetOrAddStats(const FName& InstanceName)
	{
		FScopeLock Lock(&MapCritSec);

		// avoid re-constructing the trace counter string unless it's new
		if (StatsMap.Contains(InstanceName))
		{
			return StatsMap[InstanceName];
		}

#if COUNTERSTRACE_ENABLED
		return StatsMap.Emplace(InstanceName, FString::Printf(TEXT("%s - %s"), *CategoryName, *InstanceName.ToString()));
#else
		return StatsMap.Add(InstanceName);
#endif
	}

	FConcurrentInstanceCounter::FConcurrentInstanceCounter(TSharedPtr<FConcurrentInstanceCounterManager> InCounterManager)
	: ManagerPtr(InCounterManager)
	{
		check(ManagerPtr);
	}

	FConcurrentInstanceCounter::FConcurrentInstanceCounter(const FName& InName, TSharedPtr<FConcurrentInstanceCounterManager> InCounterManager)
	: InstanceName(InName)
	, ManagerPtr(InCounterManager)
	{
		check(ManagerPtr);
		GetManagerChecked().Increment(InstanceName);
	}

	FConcurrentInstanceCounter::FConcurrentInstanceCounter(const FString& InName, TSharedPtr<FConcurrentInstanceCounterManager> InCounterManager)
	: InstanceName(InName)
	, ManagerPtr(InCounterManager)
	{
		GetManagerChecked().Increment(InstanceName);
	}

	// dtor
	FConcurrentInstanceCounter::~FConcurrentInstanceCounter()
	{
		GetManagerChecked().Decrement(InstanceName);
	}

	void FConcurrentInstanceCounter::Init(const FName& InName)
	{
		InstanceName = InName;
		GetManagerChecked().Increment(InstanceName);
	}

	void FConcurrentInstanceCounter::Init(const FString& InName)
	{
		InstanceName = FName(InName);
		GetManagerChecked().Increment(InstanceName);
	}

	FConcurrentInstanceCounterManager& FConcurrentInstanceCounter::GetManagerChecked()
	{
		check(ManagerPtr);
		return *ManagerPtr;
	}

	// static interface
} // namespace Metasound
