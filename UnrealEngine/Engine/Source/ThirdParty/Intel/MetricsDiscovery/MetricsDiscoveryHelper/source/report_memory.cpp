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

#include "metrics_discovery_helper.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

void MDH_ReportMemory::Initialize(
    MetricsDiscovery::IMetricSet_1_0* mdMetricSet,
    uint32_t numReportsToAllocate,
    MDH_ReportType reportType)
{
    assert(mdMetricSet != nullptr);
    assert(reportType == MDH_RANGE_METRICS_REPORT || reportType == MDH_PERIODIC_METRICS_REPORT);

    auto metricSetParams = mdMetricSet->GetParams();
    assert(metricSetParams != nullptr);

    NumReportsAllocated = numReportsToAllocate;

    switch (reportType) {
    case MDH_RANGE_METRICS_REPORT:
        ReportByteSize = metricSetParams->QueryReportSize;
        break;
    case MDH_PERIODIC_METRICS_REPORT:
        ReportByteSize = metricSetParams->RawReportSize;
        break;
    }

    ReportData = new uint8_t [numReportsToAllocate * ReportByteSize];
    assert(ReportData != nullptr);
}

void MDH_ReportMemory::Finalize()
{
    delete[] ReportData;
    ReportData = nullptr;
    NumReportsAllocated = 0;
    ReportByteSize = 0;
}

uint8_t* MDH_ReportMemory::GetReportData(
    uint32_t reportIndex) const
{
    assert(ReportData != nullptr);
    assert(reportIndex < NumReportsAllocated);

    return ReportData + reportIndex * ReportByteSize;
}

void MDH_ReportValues::Initialize(
    MetricsDiscovery::IMetricSet_1_0* mdMetricSet,
    uint32_t numReportsToAllocate)
{
    assert(mdMetricSet != nullptr);

    auto metricSetParams = mdMetricSet->GetParams();
    assert(metricSetParams != nullptr);

    auto metricsCount = metricSetParams->MetricsCount;
    auto informationCount = metricSetParams->InformationCount;
    NumReportsAllocated = numReportsToAllocate;
    NumReportValues = metricsCount + informationCount;

    ReportValues = new MetricsDiscovery::TTypedValue_1_0 [numReportsToAllocate * NumReportValues];
    assert(ReportValues != nullptr);

    for (uint32_t metricIdx = 0; metricIdx < metricsCount; ++metricIdx) {
        auto metric = mdMetricSet->GetMetric(metricIdx);
        assert(metric != nullptr);

        auto metricParams = metric->GetParams();
        assert(metricParams != nullptr);

        MetricsDiscovery::TValueType valueType = MetricsDiscovery::VALUE_TYPE_LAST;
        switch (metricParams->ResultType) {
        case MetricsDiscovery::RESULT_UINT32: valueType = MetricsDiscovery::VALUE_TYPE_UINT32; break;
        case MetricsDiscovery::RESULT_UINT64: valueType = MetricsDiscovery::VALUE_TYPE_UINT64; break;
        case MetricsDiscovery::RESULT_BOOL:   valueType = MetricsDiscovery::VALUE_TYPE_BOOL;   break;
        case MetricsDiscovery::RESULT_FLOAT:  valueType = MetricsDiscovery::VALUE_TYPE_FLOAT;  break;
        }

        ReportValues[metricIdx].ValueType = valueType;
    }
    for (uint32_t i = 1; i < numReportsToAllocate; ++i) {
        memcpy(ReportValues + i * NumReportValues, ReportValues, NumReportValues * sizeof(MetricsDiscovery::TTypedValue_1_0));
    }
}

void MDH_ReportValues::Finalize()
{
    delete[] ReportValues;
    ReportValues = nullptr;
    NumReportsAllocated = 0;
    NumReportValues = 0;
}

MetricsDiscovery::TTypedValue_1_0* MDH_ReportValues::GetReportValues(
    uint32_t reportIndex) const
{
    assert(ReportValues != nullptr);
    assert(reportIndex < NumReportsAllocated);

    return ReportValues + reportIndex * NumReportValues;
}

MetricsDiscovery::TTypedValue_1_0 MDH_ReportValues::GetValue(
    uint32_t reportIndex,
    uint32_t metricIndex) const
{
    assert(metricIndex < NumReportValues);
    return GetReportValues(reportIndex)[metricIndex];
}
