// Copyright Epic Games, Inc. All Rights Reserved.

#include "Net/NetworkMetricsDatabase.h"
#include "EngineStats.h"
#include "ProfilingDebugging/CsvProfiler.h"

#if USE_SERVER_PERF_COUNTERS
#include "PerfCountersModule.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(NetworkMetricsDatabase)

void UNetworkMetricsDatabase::CreateFloat(const FName MetricName, float DefaultValue)
{
	if (ensureMsgf(!MetricTypes.Contains(MetricName), TEXT("Metric %s already exists in the database."), *MetricName.ToString()))
	{
		UE::Net::FNetworkMetric<float> Metric;
		Metric.Name = MetricName;
		Metric.Value = DefaultValue;
		MetricFloats.Add(MetricName, Metric);
		MetricTypes.Add(MetricName, EMetricType::Float);
	}
}

void UNetworkMetricsDatabase::CreateInt(const FName MetricName, int64 DefaultValue)
{
	if (ensureMsgf(!MetricTypes.Contains(MetricName), TEXT("Metric %s already exists in the database."), *MetricName.ToString()))
	{
		UE::Net::FNetworkMetric<int64> Metric;
		Metric.Name = MetricName;
		Metric.Value = DefaultValue;
		MetricInts.Add(MetricName, Metric);
		MetricTypes.Add(MetricName, EMetricType::Integer);
	}
}

void UNetworkMetricsDatabase::SetFloat(const FName MetricName, float Value)
{
	UE::Net::FNetworkMetric<float>* Metric = MetricFloats.Find(MetricName);
	if (ensureMsgf(Metric, TEXT("Cannot find float metric %s."), *MetricName.ToString()))
	{
		Metric->Value = Value;
	}
}

void UNetworkMetricsDatabase::SetMinFloat(const FName MetricName, float Value)
{
	UE::Net::FNetworkMetric<float>* Metric = MetricFloats.Find(MetricName);
	if (ensureMsgf(Metric, TEXT("Cannot find float metric %s."), *MetricName.ToString()))
	{
		Metric->Value = FMath::Min<float>(Value, Metric->Value);
	}
}

void UNetworkMetricsDatabase::SetMaxFloat(const FName MetricName, float Value)
{
	UE::Net::FNetworkMetric<float>* Metric = MetricFloats.Find(MetricName);
	if (ensureMsgf(Metric, TEXT("Cannot find float metric %s."), *MetricName.ToString()))
	{
		Metric->Value = FMath::Max<float>(Value, Metric->Value);
	}
}

void UNetworkMetricsDatabase::SetInt(const FName MetricName, int64 Value)
{
	UE::Net::FNetworkMetric<int64>* Metric = MetricInts.Find(MetricName);
	if (ensureMsgf(Metric, TEXT("Cannot find integer metric %s."), *MetricName.ToString()))
	{
		Metric->Value = Value;
	}
}

void UNetworkMetricsDatabase::SetMinInt(const FName MetricName, int64 Value)
{
	UE::Net::FNetworkMetric<int64>* Metric = MetricInts.Find(MetricName);
	if (ensureMsgf(Metric, TEXT("Cannot find integer metric %s."), *MetricName.ToString()))
	{
		Metric->Value = FMath::Min<int64>(Value, Metric->Value);
	}
}

void UNetworkMetricsDatabase::SetMaxInt(const FName MetricName, int64 Value)
{
	UE::Net::FNetworkMetric<int64>* Metric = MetricInts.Find(MetricName);
	if (ensureMsgf(Metric, TEXT("Cannot find integer metric %s."), *MetricName.ToString()))
	{
		Metric->Value = FMath::Max<int64>(Value, Metric->Value);
	}
}

void UNetworkMetricsDatabase::IncrementInt(const FName MetricName, int64 Value)
{
	UE::Net::FNetworkMetric<int64>* Metric = MetricInts.Find(MetricName);
	if (ensureMsgf(Metric, TEXT("Cannot find integer metric %s."), *MetricName.ToString()))
	{
		Metric->Value += Value;
	}
}

bool UNetworkMetricsDatabase::Contains(const FName MetricName) const
{
	return MetricTypes.Contains(MetricName);
}

void UNetworkMetricsDatabase::ProcessListeners()
{
	double CurrentTimeSeconds = FPlatformTime::Seconds();

	UE::Net::FNetworkMetricSnapshot Snapshot;
	Snapshot.MetricFloats.Reserve(MetricFloats.Num());
	Snapshot.MetricInts.Reserve(MetricInts.Num());

	for (const TPair<TWeakObjectPtr<UNetworkMetricsBaseListener>, TSet<FNameAndType>>& Elem: ListenersToMetrics)
	{	
		UNetworkMetricsBaseListener* Listener = Elem.Key.Get();
		const TSet<FNameAndType>& ListenerMetrics = Elem.Value;

		if (ensure(Listener))
		{
			if (!HasReportIntervalPassed(CurrentTimeSeconds, Listener))
			{
				continue;
			}

			Snapshot.Reset();

			for (const FNameAndType& NameAndType : ListenerMetrics)
			{
				const FName& MetricName = NameAndType.Key;
				const EMetricType& MetricType = NameAndType.Value;

				if (MetricType == EMetricType::Integer)
				{
					const UE::Net::FNetworkMetric<int64>* Metric = MetricInts.Find(MetricName);
					if (ensureMsgf(Metric, TEXT("Unable to find metric %s when reporting to listeners."), *MetricName.ToString()))
					{
						Snapshot.MetricInts.Add(*Metric);
					}
				}
				else if (MetricType == EMetricType::Float)
				{
					const UE::Net::FNetworkMetric<float>* Metric = MetricFloats.Find(MetricName);
					if (ensureMsgf(Metric, TEXT("Unable to find metric %s when reporting to listeners."), *MetricName.ToString()))
					{
						Snapshot.MetricFloats.Add(*Metric);
					}
				}
			}

			Listener->Report(Snapshot);
		}
	}
}

void UNetworkMetricsDatabase::Reset()
{
	MetricInts.Reset();
	MetricFloats.Reset();
	MetricTypes.Reset();
	LastReportListener.Reset();
	ListenersToMetrics.Reset();
}

void UNetworkMetricsDatabase::Register(const FName MetricName, TWeakObjectPtr<UNetworkMetricsBaseListener> Reporter)
{
	const EMetricType* MetricType = MetricTypes.Find(MetricName);

	if (ensureMsgf(MetricType, TEXT("Cannot find metric %s to register listener."), *MetricName.ToString()))
	{
		TSet<FNameAndType>& ListenerMetrics = ListenersToMetrics.FindOrAdd(Reporter);
		ListenerMetrics.Add(FNameAndType(MetricName, *MetricType));
		LastReportListener.Add(Reporter, 0.0);
	}
}

bool UNetworkMetricsDatabase::HasReportIntervalPassed(double CurrentTimeSeconds, UNetworkMetricsBaseListener* Listener)
{
	if (ensureMsgf(Listener->GetInterval() >= 0, TEXT("Listener has a negative reporting time interval.")))
	{
		double* LastReportSeconds = LastReportListener.Find(Listener);

		if (ensure(LastReportSeconds))
		{
			double DurationSeconds = CurrentTimeSeconds - *LastReportSeconds;

			if (DurationSeconds >= Listener->GetInterval())
			{
				*LastReportSeconds = CurrentTimeSeconds;
				return true;
			}
		}
	}

	return false;
}

UNetworkMetricsBaseListener::UNetworkMetricsBaseListener() :
	UObject(),
	IntervalSeconds(0)
{

}

UNetworkMetricsCSV::UNetworkMetricsCSV() :
	CategoryIndex(-1)
{
	// The default CSV category for networking metrics.
	SetCategory("Networking");
}

void UNetworkMetricsCSV::SetCategory(const FString& CategoryName)
{
#if CSV_PROFILER
	int32 Index = FCsvProfiler::GetCategoryIndex(CategoryName);
	if (ensureMsgf(Index != 1, TEXT("Unable to find CSV category %s"), *CategoryName))
	{
		CategoryIndex = Index;
	}
#endif
}

void UNetworkMetricsCSV::Report(const UE::Net::FNetworkMetricSnapshot& Snapshot)
{
#if CSV_PROFILER
	if (ensureMsgf(CategoryIndex != -1, TEXT("SetCategory() must be called before being registered as a listener.")))
	{
		for (const UE::Net::FNetworkMetric<int64>& Metric : Snapshot.MetricInts)
		{
			if (ensureMsgf(IntFitsIn<int32>(Metric.Value), TEXT("Integer metric %s truncated when reporting to CSV."), *Metric.Name.ToString()))
			{
				FCsvProfiler::RecordCustomStat(Metric.Name, CategoryIndex, static_cast<int32>(Metric.Value), ECsvCustomStatOp::Set);
			}
		}

		for (const UE::Net::FNetworkMetric<float>& Metric : Snapshot.MetricFloats)
		{
			FCsvProfiler::RecordCustomStat(Metric.Name, CategoryIndex, Metric.Value, ECsvCustomStatOp::Set);
		}
	}
#endif
}

void UNetworkMetricsPerfCounters::Report(const UE::Net::FNetworkMetricSnapshot& Snapshot)
{
#if USE_SERVER_PERF_COUNTERS
	IPerfCounters* PerfCounters = IPerfCountersModule::Get().GetPerformanceCounters();

	if (PerfCounters)
	{
		for (const UE::Net::FNetworkMetric<int64>& Metric : Snapshot.MetricInts)
		{
			if (ensureMsgf(IntFitsIn<int32>(Metric.Value), TEXT("Integer metric %s truncated when reporting to PerfCounters."), *Metric.Name.ToString()))
			{
				PerfCounters->Set(Metric.Name.ToString(), static_cast<uint32>(Metric.Value));
			}
		}

		for (const UE::Net::FNetworkMetric<float>& Metric : Snapshot.MetricFloats)
		{
			PerfCounters->Set(Metric.Name.ToString(), Metric.Value);
		}
	}
#endif
}

UNetworkMetricsStats::UNetworkMetricsStats()
{

}

void UNetworkMetricsStats::Report(const UE::Net::FNetworkMetricSnapshot& Snapshot)
{
#if STATS
	if (!FThreadStats::IsCollectingData())
	{
		return;
	}

	const int32 TotalMetrics = Snapshot.MetricFloats.Num() + Snapshot.MetricInts.Num();

	// An instance of UNetworkMetricStats is bound to a specific Stat value defined with the DEFINE_STAT macro 
	// so there should only be one metric provided to this function.
	if (ensureMsgf(TotalMetrics <= 1, TEXT("UNetworkMetricsStats should only be registered to listen to one metric.")))
	{
		if (LIKELY(TotalMetrics == 1))
		{
			for (const UE::Net::FNetworkMetric<int64>& Metric : Snapshot.MetricInts)
			{
				SET_DWORD_STAT_FName(StatName, Metric.Value);
			}

			for (const UE::Net::FNetworkMetric<float>& Metric : Snapshot.MetricFloats)
			{
				SET_FLOAT_STAT_FName(StatName, Metric.Value);
			}
		}
	}
#endif
}
