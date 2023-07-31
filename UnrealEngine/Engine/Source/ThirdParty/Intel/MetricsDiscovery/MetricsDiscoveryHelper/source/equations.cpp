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
#include <md_calculation.h>

#include <assert.h>
#include <float.h>

void MDH_ExecuteEquations(
    MetricsDiscovery::IMetricsDevice_1_0* mdDevice,
    MetricsDiscovery::IMetricSet_1_0*     mdMetricSet,
    void*                                 prevReportMemory,
    void*                                 reportMemory,
    MetricsDiscovery::TTypedValue_1_0*    reportValues,
    uint32_t                              equations)
{
    assert(reportMemory != nullptr);
    assert(reportValues != nullptr);

    MetricsDiscoveryInternal::CMetricsCalculator calculator(mdDevice);
    calculator.Reset();

    if (equations & MDH_EQUATION_READ_RANGE) {
        assert((equations & MDH_EQUATION_READ_PERIODIC) == 0);
        assert(prevReportMemory == nullptr);
        calculator.ReadMetricsFromQueryReport((unsigned char const*) reportMemory, reportValues, mdMetricSet);
    }
    if (equations & MDH_EQUATION_READ_PERIODIC) {
        assert((equations & MDH_EQUATION_READ_RANGE) == 0);
        assert(prevReportMemory != nullptr);
        calculator.ReadMetricsFromIoReport((unsigned char const*) reportMemory, (unsigned char const*) prevReportMemory, reportValues, mdMetricSet);
    }
    if (equations & MDH_EQUATION_NORMALIZE) {
        calculator.NormalizeMetrics(reportValues, reportValues, mdMetricSet);
    }
    if (equations & MDH_EQUATION_READ_INFORMATION) {
        assert(mdMetricSet != nullptr);
        assert(mdMetricSet->GetParams() != nullptr);
        calculator.ReadInformation((unsigned char const*) reportMemory, reportValues + mdMetricSet->GetParams()->MetricsCount, mdMetricSet);
    }
}

MetricsDiscovery::TTypedValue_1_0 MDH_CalculateMaxValue(
    MetricsDiscovery::IMetricsDevice_1_0* mdDevice,
    MetricsDiscovery::IMetricSet_1_0* metricSet,
    MetricsDiscovery::IMetric_1_0* metric,
    MetricsDiscovery::TTypedValue_1_0 const* metricValues)
{
    assert(metricSet != nullptr);
    assert(metric != nullptr);

    using namespace MetricsDiscovery;

    auto metricParams = metric->GetParams();
    auto equation = metricParams->MaxValueEquation;

    TTypedValue_1_0 value = {};
    switch (metricParams->ResultType) {
    case RESULT_UINT32:
        value.ValueType = VALUE_TYPE_UINT32;
        value.ValueUInt32 = UINT32_MAX;
        break;
    case RESULT_UINT64:
        value.ValueType = VALUE_TYPE_UINT64;
        value.ValueUInt64 = UINT64_MAX;
        break;
    case RESULT_BOOL:
        value.ValueType = VALUE_TYPE_BOOL;
        value.ValueBool = true;
        break;
    case RESULT_FLOAT:
        value.ValueType = VALUE_TYPE_FLOAT;
        value.ValueFloat = FLT_MAX;
        break;
    }

    /** BEGIN WORKAROUND: MD metrics have VsEuStall ResultType UINT64 in
     * RenderMetricsSlice and ComputeBasic on HSW **/
    if (strcmp(metricParams->SymbolName, "VsEuStall") == 0) {
        value.ValueType = VALUE_TYPE_FLOAT;
        value.ValueFloat = FLT_MAX;
    }
    /** END WORKAROUND **/

    if (equation != nullptr) {
        // Do final calculation, may refer to global symbols, local delta results and local normalization results.
        // Normalization equation function is used because NormalizationEquation has the same restrictions as MaxValueEquation.
        MetricsDiscoveryInternal::CMetricsCalculator calculator(mdDevice);
        calculator.Reset();
        auto calcValue = calculator.CalculateLocalNormalizationEquation(
            equation,
            metricValues,
            metricValues,
            metricSet,
            metricParams->IdInSet);

        // Cast to ResultType
        switch (value.ValueType) {
        case VALUE_TYPE_UINT32:
            switch (calcValue.ValueType) {
            case VALUE_TYPE_UINT32: value.ValueUInt32 = calcValue.ValueUInt32; break;
            case VALUE_TYPE_UINT64: value.ValueUInt32 = (uint32_t) calcValue.ValueUInt64; break;
            case VALUE_TYPE_BOOL:   value.ValueUInt32 = calcValue.ValueBool; break;
            case VALUE_TYPE_FLOAT:  value.ValueUInt32 = (uint32_t) calcValue.ValueFloat; break;
            default: break;
            }
            break;
        case VALUE_TYPE_UINT64:
            switch (calcValue.ValueType) {
            case VALUE_TYPE_UINT32: value.ValueUInt64 = calcValue.ValueUInt32; break;
            case VALUE_TYPE_UINT64: value.ValueUInt64 = calcValue.ValueUInt64; break;
            case VALUE_TYPE_BOOL:   value.ValueUInt64 = calcValue.ValueBool; break;
            case VALUE_TYPE_FLOAT:  value.ValueUInt64 = (uint64_t) calcValue.ValueFloat; break;
            default: break;
            }
            break;
        case VALUE_TYPE_BOOL:
            switch (calcValue.ValueType) {
            case VALUE_TYPE_UINT32: value.ValueBool = calcValue.ValueUInt32 != 0; break;
            case VALUE_TYPE_UINT64: value.ValueBool = calcValue.ValueUInt64 != 0; break;
            case VALUE_TYPE_BOOL:   value.ValueBool = calcValue.ValueBool; break;
            case VALUE_TYPE_FLOAT:  value.ValueBool = calcValue.ValueFloat != 0.f; break;
            default: break;
            }
            break;
        case VALUE_TYPE_FLOAT:
            switch (calcValue.ValueType) {
            case VALUE_TYPE_UINT32: value.ValueFloat = (float) calcValue.ValueUInt32; break;
            case VALUE_TYPE_UINT64: value.ValueFloat = (float) calcValue.ValueUInt64; break;
            case VALUE_TYPE_BOOL:   value.ValueFloat = (float) calcValue.ValueBool; break;
            case VALUE_TYPE_FLOAT:  value.ValueFloat = calcValue.ValueFloat; break;
            default: break;
            }
            break;
        default: break;
        }
    }

    return value;
}

