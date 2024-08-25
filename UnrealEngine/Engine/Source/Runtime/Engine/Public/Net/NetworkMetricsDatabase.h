// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NetworkMetricsDatabase.generated.h"

namespace UE::Net
{

template<class MetricType>
struct FNetworkMetric
{
	FName Name;
	MetricType Value;
};

struct FNetworkMetricSnapshot
{
	TArray<FNetworkMetric<int64>> MetricInts;
	TArray<FNetworkMetric<float>> MetricFloats;

	void Reset()
	{
		MetricInts.Reset();
		MetricFloats.Reset();
	}
};

} // namespace UE::Net

UCLASS()
class ENGINE_API UNetworkMetricsDatabase : public UObject
{
	GENERATED_BODY()

public:
	/* Add a floating point metric. */
	void CreateFloat(const FName MetricName, float DefaultValue);
	/* Add an integer metric. */
	void CreateInt(const FName MetricName, int64 DefaultValue);
	/* Set the value of an existing floating point metric. */
	void SetFloat(const FName MetricName, float Value);
	/* Set the value of a floating point metric if it's smaller than the existing value. */
	void SetMinFloat(const FName MetricName, float Value);
	/* Set the value of a floating point metric if it's bigger than the existing value. */
	void SetMaxFloat(const FName MetricName, float Value);
	/* Set the value of an existing integer metric. */
	void SetInt(const FName MetricName, int64 Value);
	/* Set the value of an integer metric if it's smaller than the existing value. */
	void SetMinInt(const FName MetricName, int64 Value);
	/* Set the value of an integer metric if it's bigger than the existing value. */
	void SetMaxInt(const FName MetricName, int64 Value);
	/* Increment the value of an existing integer metric. */
	void IncrementInt(const FName MetricName, int64 Value);
	/* Returns true if a metric has been created in the database. */
	bool Contains(const FName MetricName) const;

	/* Call all registered listeners.*/
	void ProcessListeners();
	/* Remove all registered metrics and listeners. */
	void Reset();

	/* Register a listener to be called for a given metric. */
	void Register(const FName MetricName, TWeakObjectPtr<UNetworkMetricsBaseListener> Reporter);

private:
	/* Return true if the report interval time for a listener has elapsed. */
	bool HasReportIntervalPassed(double CurrentTimeSeconds, UNetworkMetricsBaseListener* Listener);

	enum class EMetricType { Integer, Float };

	TMap<FName, EMetricType> MetricTypes;

	TMap<FName, UE::Net::FNetworkMetric<int64>> MetricInts;
	TMap<FName, UE::Net::FNetworkMetric<float>> MetricFloats;

	using FNameAndType = TPair<FName, EMetricType>;

	/* The time, in seconds, metrics were reported to a listener. */
	TMap<TWeakObjectPtr<UNetworkMetricsBaseListener>, double> LastReportListener;

	TMap<TWeakObjectPtr<UNetworkMetricsBaseListener>, TSet<FNameAndType>> ListenersToMetrics;
};

/** 
 * An abstract class for metrics listeners that are registered with FNetworkMetricsDatabase.
 * 
 * Listeners are the recommended method for reading the current value of metrics from FNetworkMetricsDatabase.
 * 
 * Begin by creating a sub-class of UNetworkMetricsBaseListener that overrides the Report() function. This function will be called 
 * by FNetworkMetricsDatabase::ProcessListeners() once a frame and will be provided an array of metrics that are registered 
 * with FNetworkMetricsDatabase::Register():
 *
 * UCLASS()
 * class UNetworkMetricsMyListener : public UNetworkMetricsBaseListener
 * {
 *		GENERATED_BODY()
 * public:
 *		virtual ~UNetworkMetricsMyListener() = default;
 * 
 *		void Report(const TArray<UE::Net::FNetworkMetricSnapshot>& Stats)
 *		{
 *			for (const UE::Net::FNetworkMetric<int64>& Metric : Snapshot.MetricInts)
 *			{
 *				// Do something with integer metrics...
 *			}
 *
 *			for (const UE::Net::FNetworkMetric<float>& Metric : Snapshot.MetricFloats)
 *			{
 *				// Do something with floating point metrics...
 *			}
 *		}
 * };
 *
 * Listeners can either be registered explicitly using FNetworkMetricsDatabase::Register() or through the engine configuration files. A configuration 
 * file is the prefered way to register a listener because it allows metrics reporting to be configured without rebuilding the application.
 *
 * This is an example configuration from an ini file (e.g. DefaultEngine.ini) that registers metrics with the example listener above:
 * 
 * [/Script/Engine.NetworkMetricsConfig]
 * +Listeners=(MetricName=ExampleMetric1, ClassName=/Script/Engine.NetworkMetricsMyListener)
 * 
 * All sub-classes of UNetworkMetricsBaseListener can set a time interval between calls to Report(). This is a useful method for limiting the rate
 * at which metrics need to be recorded (e.g. you may only want to report metrics to an external analytics services every 60 seconds). This time interval
 * can be set by calling UNetworkMetricsBaseListener::SetInterval() or in a configuration file by setting the IntervalSeconds property on the
 * listener sub-class.
 * 
 * This is an example configuration from an ini file (e.g. DefaultEngine.ini) that sets the interval between calling UNetworkMetricsMyListener::Report()
 * to 1 second:
 * 
 * [/Script/Engine.NetworkMetricsMyListener]
 * IntervalSeconds=1
 */
UCLASS(abstract, Config=Engine)
class ENGINE_API UNetworkMetricsBaseListener : public UObject
{
	GENERATED_BODY()

public:
	UNetworkMetricsBaseListener();
	virtual ~UNetworkMetricsBaseListener() = default;

	/* Set the interval, in seconds, between calling Report(). */
	void SetInterval(double Seconds)
	{
		if (ensureMsgf(Seconds >= 0, TEXT("SetInterval() called with a negative time interval.")))
		{
			IntervalSeconds = Seconds;
		}
	}

	/* Get the interval, in seconds, between calling Report(). */
	double GetInterval() const
	{
		return IntervalSeconds;
	}

	virtual void Report(const UE::Net::FNetworkMetricSnapshot& Snapshot) {};

private:
	UPROPERTY(Config)
	double IntervalSeconds;
};

/**
 * A metrics listener that reports an array of metrics to CSV.
 * 
 * The function SetCategory() is expected to be called before the listener is registered with
 * FNetworkMetricsDatabase::Register(). This function will associate the instance of UNetworkMetricsCSV
 * with a category of values in CSV.
 * 
 * To use UNetworkMetricsCSV in a configuration file, a sub-class of UNetworkMetricsCSV must be 
 * created that calls SetCategory() from the constructor to provide the CSV category to use.
 * 
 * UCLASS()
 * class UNetworkMetricsCSV_ExampleCategory : public UNetworkMetricsCSV
 * {
 *		GENERATED_BODY()
 * public:
 *		virtual ~UNetworkMetricsCSV_ExampleCategory() = default;
 * 
 *		UNetworkMetricsCSV_ExampleCategory() : UNetworkMetricsCSV()
 *		{
 *			SetCategory("ExampleCategory");
 *		}
 * };
 * 
 * This sub-class can then be used in the configuration file when registering a listener:
 * 
 *	[/Script/Engine.NetworkMetricsConfig]
 *	+Listeners=(MetricName=ExampleMetric, ClassName=/Script/Engine.NetworkMetricsCSV_ExampleCategory)
 * 
 * If the base UNetworkMetricsCSV class is used in the configuration file, CSV stats will be recorded to the default 'Networking' category.
 */
UCLASS()
class ENGINE_API UNetworkMetricsCSV : public UNetworkMetricsBaseListener
{
	GENERATED_BODY()

public:
	UNetworkMetricsCSV();
	virtual ~UNetworkMetricsCSV() = default;

	/* Set the CSV category. */
	void SetCategory(const FString& CategoryName);

	virtual void Report(const UE::Net::FNetworkMetricSnapshot& Snapshot);

private:
	int32 CategoryIndex;
};

/**
 * A metrics listener that reports an array of metrics to PerfCounters.
 * 
 * To use UNetworkMetricsPerfCounters in a configuration file, the class must be used when registering a listener:
 * 
 *	[/Script/Engine.NetworkMetricsConfig]
 *	+Listeners=(MetricName=ExampleMetric, ClassName=/Script/Engine.NetworkMetricsPerfCounters)
 */
UCLASS()
class ENGINE_API UNetworkMetricsPerfCounters : public UNetworkMetricsBaseListener
{
	GENERATED_BODY()

public:
	virtual ~UNetworkMetricsPerfCounters() = default;

	virtual void Report(const UE::Net::FNetworkMetricSnapshot& Snapshot);
};

/**
 * A metrics listener that reports a metric to a single Stat. 
 * 
 * The function SetStatName() is expected to be called before the listener is registered with
 * FNetworkMetricsDatabase::Register(). This function will associate the instance of UNetworkMetricStats
 * with a specific Stat.
 * 
 * Since each instance of this class is associated with a single Stat it can only be registered
 * as a listener to a single metric in FNetworkMetricsDatabase.
 * 
 * UNetworkMetricsStats is not intended to be used from configuration files!
 */
UCLASS()
class ENGINE_API UNetworkMetricsStats : public UNetworkMetricsBaseListener
{
	GENERATED_BODY()

public:
	UNetworkMetricsStats();
	virtual ~UNetworkMetricsStats() = default;

	/** Set the name of the pre-defined Stat (normally defined with DEFINE_STAT()). */
	void SetStatName(const FName Name)
	{
		StatName = Name;
	}

	virtual void Report(const UE::Net::FNetworkMetricSnapshot& Snapshot);

private:
	FName StatName;
};