/*
Copyright 2015-2018 Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef METRICS_DISCOVERY_HELPER_H
#define METRICS_DISCOVERY_HELPER_H

#include <stdint.h>
#include <windows.h>
#include "metrics_discovery_api.h"

#ifdef NDEBUG
#define MDH_CHECK_CC(_CC) (void) _CC
#else
#define MDH_CHECK_CC(_CC) assert(_CC == MetricsDiscovery::CC_OK)
#endif

// MetricsDiscoveryHelper is intended as a minimal helper layer to be used in
// conjunction with the MetricsDiscovery API, not to replace it.  Therefore, it
// is a C++ API and all MetricsDiscovery objects are exposed, allowing the user
// to replace any MetricsDiscoveryHelper API with her own code.
//
// Usage validation is largely handled via assert() so that the release build
// is as unencumbered as possible.


// MDH_Context::Initialize() initializes a MetricsDiscoveryHelper context and
// connects to the driver's MetricsDiscovery interface.  When MetricsDiscovery
// is no longer needed, the application should call MDH_Context::Finalize()
// (after which time, all MDH objects become unusable).
//
// You must call Initialize() before creating a D3D11 device.

struct MDH_Context {
    MetricsDiscovery::IMetricsDevice_1_5* MDDevice;
    void* DLLHandle;

    MDH_Context()
        : MDDevice(nullptr)
        , DLLHandle(nullptr)
    {
    }

    enum Result {
        RESULT_OK,
        RESULT_MD_DLL_NOT_FOUND,
        RESULT_MD_VERSION_MISMATCH,
    };

    Result Initialize();
    void Finalize();
};


// Obtain the MetricsDiscovery API version used to compile
// MetricsDiscoveryHelper ("API") or by the system's driver ("Driver").
//
// MDH_Context::Initialize() does not require that the versions match and the
// MDAPI itself is backwards compatible.  However, the driver may not support
// all the functionality used by this version of the API.  For example, don't
// call IMetricsDevice_1_2-specific functionality if the driver is less than
// version 1.2.
//
// MDH_GetDriverVersion() will return { 0, 0, 0 } if mdDevice is incompatible
// or not properly initialized.
//
// MDH_DriverSupportsMDVersion() will return true if the specified device
// (driver) supports the specified version or greater.

struct MDH_Version {
    uint32_t MajorVersion;
    uint32_t MinorVersion;
    uint32_t BuildVersion;
};

MDH_Version MDH_GetAPIVersion();
MDH_Version MDH_GetDriverVersion(MetricsDiscovery::IMetricsDevice_1_0* mdDevice);

bool MDH_DriverSupportsMDVersion(
    MetricsDiscovery::IMetricsDevice_1_0* mdDevice,
    uint32_t minMajorVersion,
    uint32_t minMinorVersion,
    uint32_t minBuildVersion);


// The following functions search for MetricsDiscovery objects by symbol name:
//
//     - A GlobalSymbol is a constant, but architecture-dependent value.
//
//     - A Metric is a value either measured or computed by the system's
//     performance counter infrastructure.
//
//     - A MetricSet is a set of Metrics that are all collected at the same
//     time.
//
//     - A ConcurrentGroup is a group of MetricSets that cannot be used at the
//     same time.
//
//     - An Override is a function that changes the system's default behaviour.

// returns nullptr if not found
MetricsDiscovery::IOverride_1_2* MDH_FindOverride(
    MetricsDiscovery::IMetricsDevice_1_0* device,
    char const* symbolName);
MetricsDiscovery::IConcurrentGroup_1_0* MDH_FindConcurrentGroup(
    MetricsDiscovery::IMetricsDevice_1_0* device,
    char const* symbolName);
MetricsDiscovery::IMetricSet_1_0* MDH_FindMetricSet(
    MetricsDiscovery::IConcurrentGroup_1_0* concurrentGroup,
    char const* symbolName);

// returns (uint32_t) -1 if not found
uint32_t MDH_FindMetric(
    MetricsDiscovery::IMetricSet_1_0* mdMetricSet,
    char const* desiredMetricSymbolName);

// returns a TTypedValue_1_0 with .ValueType=MetricsDiscovery::VALUE_TYPE_LAST
// if not found
MetricsDiscovery::TTypedValue_1_0 MDH_FindGlobalSymbol(
    MetricsDiscovery::IMetricsDevice_1_0* device,
    char const* desiredGlobalSymbolName);


// Helper functions to access useful Metric parameters

// Get metrics associated with the metric
char const* MDH_GetMetricUnits(
    MetricsDiscovery::IMetricSet_1_0* mdMetricSet,
    uint32_t metricIndex);

// Run the max-value equation associated with the metric (if there is one).
MetricsDiscovery::TTypedValue_1_0 MDH_CalculateMaxValue(
    MetricsDiscovery::IMetricsDevice_1_0* mdDevice,
    MetricsDiscovery::IMetricSet_1_0* mdMetricSet,
    MetricsDiscovery::IMetric_1_0* mdMetric,
    MetricsDiscovery::TTypedValue_1_0 const* reportValues);

// Maintains the maximum value of a metric.  This could be derived based on an
// architectural constant (CONSTANT_MAX_VALUE), it oculd be dynamic but
// computed from other instantaneous metrics (DYNAMIC_MAX_VALUE), or there may
// be no equation to compute it in which case Update() will track the maximum
// observed value (UNKNOWN_MAX_VALUE).
struct MDH_MaximumValue {
    enum MaxValueType {
        UNKNOWN_MAX_VALUE,
        CONSTANT_MAX_VALUE,
        DYNAMIC_MAX_VALUE,
    };

    MetricsDiscovery::IMetric_1_0* MDMetric;
    MetricsDiscovery::TTypedValue_1_0 MaxValue;
    MaxValueType Type;

    MDH_MaximumValue()
        : MDMetric(nullptr)
        , Type(UNKNOWN_MAX_VALUE)
    {
    }

    void Initialize(
        MetricsDiscovery::IMetricsDevice_1_0* mdDevice,
        MetricsDiscovery::IMetricSet_1_0* mdMetricSet,
        MetricsDiscovery::IMetric_1_0* mdMetric);
    void Update(
        MetricsDiscovery::IMetricsDevice_1_0* mdDevice,
        MetricsDiscovery::IMetricSet_1_0* mdMetricSet,
        MetricsDiscovery::TTypedValue_1_0 const& currentValue,
        MetricsDiscovery::TTypedValue_1_0 const* reportValues);
};

// Return the value from a TTypedValue_1_0 (these do not convert, but assert
// that the TTypedValue_1_0 is of the appropriate type).
uint32_t MDH_U32(MetricsDiscovery::TTypedValue_1_0 const& value);
uint64_t MDH_U64(MetricsDiscovery::TTypedValue_1_0 const& value);
float MDH_F32(MetricsDiscovery::TTypedValue_1_0 const& value);
bool MDH_Bool(MetricsDiscovery::TTypedValue_1_0 const& value);
float MDH_ConvertTypedValueToFloat(MetricsDiscovery::TTypedValue_1_0 const& value);


// MDH_ReportMemory and MDH_ReportValues maintains the memory storage required
// for gathering and processing metrics.  Data is typically collected in a raw
// format in MDH_ReportMemory::ReportData, and then processed into
// TTypedValue_1_0 instances in MDH_ReportValues::ReportValues.
//
// ReportData is an array of numReportsToAllocate raw reports (i.e., uint8_t
// [numReportsToAllocate][ReportByteSize]) used to store raw metric data
// collected by the hardware.
//
// ReportValues is an array of NumReportValues calculated values (i.e.,
// TTypedValue_1_0* [NumReportValues]) used to compute and store final values
// for all the metrics in the MetricSet.
//
// GetReportData() returns a pointer to the raw report data for the specified
// report.
//
// GetReportValues() returns a pointer to the first value for the specified
// report.
//
// GetValue() returns the specified value.

enum MDH_ReportType {
    MDH_RANGE_METRICS_REPORT,
    MDH_PERIODIC_METRICS_REPORT,
};

struct MDH_ReportMemory {
    uint8_t* ReportData;
    uint32_t NumReportsAllocated;
    uint32_t ReportByteSize;

    MDH_ReportMemory()
        : ReportData(nullptr)
        , NumReportsAllocated(0)
        , ReportByteSize(0)
    {
    }

    void Initialize(
        MetricsDiscovery::IMetricSet_1_0* mdMetricSet,
        uint32_t numReportsToAllocate,
        MDH_ReportType reportType);

    void Finalize();

    uint8_t* GetReportData(uint32_t reportIndex) const;
};

struct MDH_ReportValues {
    MetricsDiscovery::TTypedValue_1_0* ReportValues;
    uint32_t NumReportsAllocated;
    uint32_t NumReportValues;

    MDH_ReportValues()
        : ReportValues(nullptr)
        , NumReportsAllocated(0)
        , NumReportValues(0)
    {
    }

    void Initialize(
        MetricsDiscovery::IMetricSet_1_0* mdMetricSet,
        uint32_t numReportsToAllocate);

    void Finalize();

    MetricsDiscovery::TTypedValue_1_0* GetReportValues(uint32_t reportIndex) const;
    MetricsDiscovery::TTypedValue_1_0 GetValue(uint32_t reportIndex, uint32_t metricIndex) const;
};


// Processing is required to convert raw report data into the final metrics
// values.  When using one of the MDH_RangeMetrics implementations, this is
// done by the ExecuteRangeEquations() member function, but this can also be
// done explicity using MDH_ExecuteEquations().
//
// prevReportMemory and reportMemory point to two raw reports representing the
// start and end point over which the metrics have changed.
//
// reportValues must point to an array of TTypedValue_1_0 instances, at least
// one per Metric in the MetricSet, where computed values will be written to.
//
// equations is a bitmask of MDH_EquationFlag values specifying the equations
// to perform (typically one of the MDH_EQUATION_READ values depending on the
// type of collection as well as MDH_EQUATION_NORMALIZE).

enum MDH_EquationFlag {
    MDH_EQUATION_READ_RANGE       = 1<<0,
    MDH_EQUATION_READ_PERIODIC    = 1<<1,
    MDH_EQUATION_READ_INFORMATION = 1<<2,
    MDH_EQUATION_NORMALIZE        = 1<<3,
};

void MDH_ExecuteEquations(
    MetricsDiscovery::IMetricsDevice_1_0* mdDevice,
    MetricsDiscovery::IMetricSet_1_0*     mdMetricSet,
    void*                                 prevReportMemory,
    void*                                 reportMemory,
    MetricsDiscovery::TTypedValue_1_0*    reportValues,
    uint32_t                              equations);


// There are two ways to sample metric data.
//
// 1) By explicitly specifying a begin and end point over which metric changes
// are to be observed. These are called range metrics, and their API is
// specified in the graphics API-specific header files (e.g.,
// metrics_discovery_helper_dx11.h).
//
// 2) By specifying a time period that metrics will be sampled on.  These are
// called periodic metrics, and are accessed by using the following API.
//
// MDH_StartSamplingPeriodicMetrics() starts collecting periodic metrics.
// mdhReportMemory should be initialized as PERIODIC_METRICS_REPORT type.
// targetProcessId specifies a process ID to restrict metrics to; a value of 0
// causes metrics to be collected across all processes.  samplePeriodNs and
// numReportsBufferedByDriver are input-output parameters that may need to be
// adjusted by the implementation due to architecture-dependent HW constraints.
//
// You must ensure that MDH_StopSamplingPeriodicMetrics() is *always* called
// after a successful MDH_StartSamplingPeriodicMetrics() call.  Failure to do
// so will prevent future calls to MDH_StartSamplingPeriodicMetrics() from
// succeeding (even after the application terminates, or in other applications)
// until the machine is rebooted.
//
// Call MDH_CopyDriverBufferedReports() to copy collected metrics from the
// driver's buffer into the supplied mdhReportMemory.  reportReadIndex and
// reportWriteIndex specify a circular buffer within mdhReportMemory where
// reportReadIndex is the next report that the caller will read and
// reportWriteIndex is the next report that
// MDH_CopyDriverBufferedPeriodicReports() will write; reports between
// [reportReadIndex, reportWriteIndex) will not be overwritten.
// reportReadIndex==reportWriteIndex is considered an empty buffer.  If you
// don't call MDH_CopyDriverBufferedReports() often enough, the driver's
// internal buffer might fill up causing reports to be lost.
// MDH_CopyDriverBufferedReports() will return the number of reports copied.
//
// High bits of the HW timestamp can be unreliable.
// MDH_ExtendPeriodicReportTimestamps() attempts to determine appropraite high
// bits for the timestamps in the range [reportBeginIndex, reportEndIndex).
// Pass in the timestamp of the previous processed report (returned from this
// function) or 0 initially.

bool MDH_PeriodicMetricsSupported(
    MetricsDiscovery::IConcurrentGroup_1_0* mdConcurrentGroup);

bool MDH_StartSamplingPeriodicMetrics(
    MetricsDiscovery::IConcurrentGroup_1_0* mdConcurrentGroup,
    MetricsDiscovery::IMetricSet_1_0* mdMetricSet,
    MDH_ReportMemory const& mdhReportMemory,
    uint32_t targetProcessId,
    uint32_t* samplePeriodNs,
    uint32_t* numReportsBufferedByDriver);

void MDH_StopSamplingPeriodicMetrics(
    MetricsDiscovery::IConcurrentGroup_1_0* mdConcurrentGroup);

uint32_t MDH_CopyDriverBufferedPeriodicReports(
    MetricsDiscovery::IConcurrentGroup_1_0* mdConcurrentGroup,
    MDH_ReportMemory* mdhReportMemory,
    uint32_t reportReadIndex,
    uint32_t reportWriteIndex);

uint64_t MDH_ExtendPeriodicReportTimestamps(
    MDH_ReportMemory* mdhReportMemory,
    uint32_t reportBeginIndex,
    uint32_t reportEndIndex,
    uint64_t latestReportTimestamp);

#endif // ifndef METRICS_DISCOVERY_HELPER_H
