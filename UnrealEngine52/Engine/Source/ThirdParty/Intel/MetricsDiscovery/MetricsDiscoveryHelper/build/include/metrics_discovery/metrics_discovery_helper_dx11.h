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

#ifndef METRICS_DISCOVERY_HELPER_DX11_H
#define METRICS_DISCOVERY_HELPER_DX11_H

#include "metrics_discovery_helper.h"

struct ID3D11Asynchronous;
struct ID3D11Device;
struct ID3D11DeviceContext;

// Range metrics is the collection of a MetricSet across a range of API calls
// specified by a begin and end point.  MDH_RangeMetricsDX11 allocates the
// necessary resources and storage to collect and process range metrics for
// DX11.
//
// Initialize the MDH_RangeMetricsDX11 instance by calling Initialize() with
// the MDAPI device corresponding to the DX11 device that the commands will be
// submitted to, the concurrent group and metric set that you wish to collect,
// and the number of ranges to allocate.
//
// No other member functions protect against being called before successful
// initialization: do not call any member functions before calling Initialize()
// or if Initialize() fails.
//
// To collect metrics across D3D11 commands wrap the target commands with
// BeginRange() and EndRange() calls, specifying a range index within [0,
// numRangesToAllocate-1] to store the results to.
//
// GetRangeReports() waits for the specified ranges to complete and copies the
// raw metric data from the driver into the instance's ReportMemory.
//
// ExecuteRangeEquations() executes the read and normalization equations for
// the specified ranges to compute the final metric values in ReportValues.

struct MDH_RangeMetricsDX11 {
    MetricsDiscovery::IMetricsDevice_1_0* MDDevice;
    MetricsDiscovery::IConcurrentGroup_1_0* MDConcurrentGroup;
    MetricsDiscovery::IMetricSet_1_0* MDMetricSet;
    ID3D11Asynchronous** D3D11Async;
    MDH_ReportMemory ReportMemory;
    MDH_ReportValues ReportValues;
    bool CanCallCalculateMetrics;
    bool IsQuery;

    MDH_RangeMetricsDX11()
        : MDDevice(nullptr)
        , MDConcurrentGroup(nullptr)
        , MDMetricSet(nullptr)
        , D3D11Async(nullptr)
        , ReportMemory()
        , ReportValues()
        , CanCallCalculateMetrics(false)
        , IsQuery(false)
    {
    }

    bool Initialize(
        MetricsDiscovery::IMetricsDevice_1_0* mdDevice,
        MetricsDiscovery::IConcurrentGroup_1_0* mdConcurrentGroup,
        MetricsDiscovery::IMetricSet_1_0* mdMetricSet,
        ID3D11Device* d3dDevice,
        uint32_t numRangesToAllocate);
    void Finalize();

    void BeginRange(ID3D11DeviceContext* deviceCtxt, uint32_t rangeIndex) const;
    void EndRange(ID3D11DeviceContext* deviceCtxt, uint32_t rangeIndex) const;
    void GetRangeReports(ID3D11DeviceContext* deviceCtxt, uint32_t firstRangeIndex, uint32_t rangeCount) const;
    void ExecuteRangeEquations(ID3D11DeviceContext* deviceCtxt, uint32_t firstRangeIndex, uint32_t rangeCount) const;
};

#endif // ifndef METRICS_DISCOVERY_HELPER_DX11_H
