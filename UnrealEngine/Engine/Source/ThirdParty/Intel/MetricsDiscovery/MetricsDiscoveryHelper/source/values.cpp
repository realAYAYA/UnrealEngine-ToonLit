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

#define NOMINMAX
#include "metrics_discovery_helper.h"
#include <assert.h>
#include <algorithm>

uint32_t MDH_U32(MetricsDiscovery::TTypedValue_1_0 const& value)
{
    assert(value.ValueType == MetricsDiscovery::VALUE_TYPE_UINT32);
    return value.ValueUInt32;
}

uint64_t MDH_U64(MetricsDiscovery::TTypedValue_1_0 const& value)
{
    assert(value.ValueType == MetricsDiscovery::VALUE_TYPE_UINT64);
    return value.ValueUInt64;
}

float MDH_F32(MetricsDiscovery::TTypedValue_1_0 const& value)
{
    assert(value.ValueType == MetricsDiscovery::VALUE_TYPE_FLOAT);
    return value.ValueFloat;
}

bool MDH_Bool(MetricsDiscovery::TTypedValue_1_0 const& value)
{
    assert(value.ValueType == MetricsDiscovery::VALUE_TYPE_BOOL);
    return value.ValueBool;
}

float MDH_ConvertTypedValueToFloat(
    MetricsDiscovery::TTypedValue_1_0 const& value)
{
    switch (value.ValueType) {
    case MetricsDiscovery::VALUE_TYPE_UINT32: return (float) value.ValueUInt32;
    case MetricsDiscovery::VALUE_TYPE_UINT64: return (float) value.ValueUInt64;
    case MetricsDiscovery::VALUE_TYPE_FLOAT:  return         value.ValueFloat;
    case MetricsDiscovery::VALUE_TYPE_BOOL:   return (float) value.ValueBool;
    default:
        assert(false);
        break;
    }
    return 0.f;
}

void MDH_MaximumValue::Initialize(
    MetricsDiscovery::IMetricsDevice_1_0* mdDevice,
    MetricsDiscovery::IMetricSet_1_0* mdMetricSet,
    MetricsDiscovery::IMetric_1_0* mdMetric)
{
    assert(mdMetricSet != nullptr);
    assert(mdMetric != nullptr);

    MDMetric = mdMetric;

    auto metricParams = mdMetric->GetParams();
    assert(metricParams != nullptr);

    auto equation = metricParams->MaxValueEquation;

    /** BEGIN WORKAROUND: MD metrics have *sEuActivePerThread MaxValueEquation returning 100 incorrectly **/
    if (strcmp(metricParams->SymbolName + 1, "sEuActivePerThread") == 0) {
        equation = nullptr;
    }
    /** END WORKAROUND **/

    if (equation == nullptr) {
        Type = UNKNOWN_MAX_VALUE;

        switch (metricParams->ResultType) {
        case MetricsDiscovery::RESULT_UINT32:
            MaxValue.ValueType = MetricsDiscovery::VALUE_TYPE_UINT32;
            MaxValue.ValueUInt32 = 0;
            break;
        case MetricsDiscovery::RESULT_UINT64:
            MaxValue.ValueType = MetricsDiscovery::VALUE_TYPE_UINT64;
            MaxValue.ValueUInt64 = 0;
            break;
        case MetricsDiscovery::RESULT_BOOL:
            MaxValue.ValueType = MetricsDiscovery::VALUE_TYPE_BOOL;
            MaxValue.ValueBool = false;
            break;
        case MetricsDiscovery::RESULT_FLOAT:
            MaxValue.ValueType = MetricsDiscovery::VALUE_TYPE_FLOAT;
            MaxValue.ValueFloat = 100.f;
            break;
        }
        return;
    }

    bool constant = true;
    for (uint32_t i = 0, N = equation->GetEquationElementsCount(); constant && i < N; ++i) {
        auto element = equation->GetEquationElement(i);
        constant =
            element->Type != MetricsDiscovery::EQUATION_ELEM_RD_BITFIELD &&
            element->Type != MetricsDiscovery::EQUATION_ELEM_RD_UINT8 &&
            element->Type != MetricsDiscovery::EQUATION_ELEM_RD_UINT16 &&
            element->Type != MetricsDiscovery::EQUATION_ELEM_RD_UINT32 &&
            element->Type != MetricsDiscovery::EQUATION_ELEM_RD_UINT64 &&
            element->Type != MetricsDiscovery::EQUATION_ELEM_RD_FLOAT &&
            element->Type != MetricsDiscovery::EQUATION_ELEM_RD_40BIT_CNTR &&
            element->Type != MetricsDiscovery::EQUATION_ELEM_SELF_COUNTER_VALUE &&
            element->Type != MetricsDiscovery::EQUATION_ELEM_LOCAL_COUNTER_SYMBOL &&
            element->Type != MetricsDiscovery::EQUATION_ELEM_OTHER_SET_COUNTER_SYMBOL &&
            element->Type != MetricsDiscovery::EQUATION_ELEM_LOCAL_METRIC_SYMBOL &&
            element->Type != MetricsDiscovery::EQUATION_ELEM_OTHER_SET_METRIC_SYMBOL &&
            element->Type != MetricsDiscovery::EQUATION_ELEM_INFORMATION_SYMBOL &&
            element->Type != MetricsDiscovery::EQUATION_ELEM_STD_NORM_GPU_DURATION &&
            element->Type != MetricsDiscovery::EQUATION_ELEM_STD_NORM_EU_AGGR_DURATION;
    }

    if (constant) {
        Type = CONSTANT_MAX_VALUE;
        MaxValue = MDH_CalculateMaxValue(mdDevice, mdMetricSet, mdMetric, nullptr);
        return;
    }

    Type = DYNAMIC_MAX_VALUE;
}

void MDH_MaximumValue::Update(
    MetricsDiscovery::IMetricsDevice_1_0* mdDevice,
    MetricsDiscovery::IMetricSet_1_0* mdMetricSet,
    MetricsDiscovery::TTypedValue_1_0 const& currentValue,
    MetricsDiscovery::TTypedValue_1_0 const* reportValues)
{
    assert(Type != DYNAMIC_MAX_VALUE || MDMetric != nullptr);

    switch (Type) {
    case UNKNOWN_MAX_VALUE:
        switch (MaxValue.ValueType) {
        case MetricsDiscovery::VALUE_TYPE_UINT32: MaxValue.ValueUInt32 = std::max(MaxValue.ValueUInt32, currentValue.ValueUInt32); break;
        case MetricsDiscovery::VALUE_TYPE_UINT64: MaxValue.ValueUInt64 = std::max(MaxValue.ValueUInt64, currentValue.ValueUInt64); break;
        case MetricsDiscovery::VALUE_TYPE_FLOAT:  MaxValue.ValueFloat  = std::max(MaxValue.ValueFloat , currentValue.ValueFloat ); break;
        default:
            assert(false);
            break;
        }
        break;

    case CONSTANT_MAX_VALUE:
        break;

    case DYNAMIC_MAX_VALUE:
        MaxValue = MDH_CalculateMaxValue(mdDevice, mdMetricSet, MDMetric, reportValues);
        break;
    }
}
