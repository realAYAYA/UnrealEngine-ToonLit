// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "HAL/PlatformAtomics.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Stats/Stats.h"
#include "Templates/UniquePtr.h"
#include "UObject/NameTypes.h"

// todo: comments

namespace Metasound
{
#if COUNTERSTRACE_ENABLED
	using CounterType = FCountersTrace::FCounterInt;
#else
	using CounterType = FThreadSafeCounter64;
#endif

	// forward
	class FConcurrentInstanceCounter;

	class FConcurrentInstanceCounterManager
	{
	public:
		FConcurrentInstanceCounterManager(const FString InCategoryName);

		void Increment(const FName& InstanceName);
		void Decrement(const FName& IntsanceName);

		int64 GetCountForName(const FName& InName);
		int64 GetPeakCountForName(const FName& InName);

	private:
		struct FStats
		{
		public:
			// ctor
#if COUNTERSTRACE_ENABLED
			FStats(const FString& InName);
#else
			FStats() = default;
#endif
	
			void Increment();
			void Decrement();
	
			int64 GetCount();
			int64 GetPeakCount();
	
		private:
			TUniquePtr<CounterType> TraceCounter;
			int64 PeakCount;
	
		}; // struct FStats

		FStats& GetOrAddStats(const FName& InstanceName);

		TMap<FName, FStats> StatsMap;
		FCriticalSection MapCritSec;
		FString CategoryName;

	}; // class FConcurrentInstanceCounterManager



	class FConcurrentInstanceCounter
	{
	public:
		// ctor
		FConcurrentInstanceCounter() = delete; // must init ManagerPtr
		FConcurrentInstanceCounter(TSharedPtr<FConcurrentInstanceCounterManager> InCounterManager);
		FConcurrentInstanceCounter(const FName& InName, TSharedPtr<FConcurrentInstanceCounterManager> InCounterManager);
		FConcurrentInstanceCounter(const FString& InName, TSharedPtr<FConcurrentInstanceCounterManager> InCounterManager);
	
		// dtor
		virtual ~FConcurrentInstanceCounter();
	
		// for non-RAII clients
		void Init(const FName& InName);
		void Init(const FString& InName);	
	
	private:
		FConcurrentInstanceCounterManager& GetManagerChecked();

		FName InstanceName;
		TSharedPtr<FConcurrentInstanceCounterManager> ManagerPtr;
	}; // class FConcurrentInstanceCounter
} // namespace Metasound
