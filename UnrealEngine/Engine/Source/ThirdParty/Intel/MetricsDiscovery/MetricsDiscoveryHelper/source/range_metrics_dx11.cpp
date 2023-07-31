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

#include "metrics_discovery_helper_dx11.h"
#include <assert.h>
#include <d3d11.h>

bool MDH_RangeMetricsDX11::Initialize(
    MetricsDiscovery::IMetricsDevice_1_0* mdDevice,
    MetricsDiscovery::IConcurrentGroup_1_0* mdConcurrentGroup,
    MetricsDiscovery::IMetricSet_1_0* mdMetricSet,
    ID3D11Device* d3dDevice,
    uint32_t numRangesToAllocate)
{
    assert(mdDevice != nullptr);
    assert(mdConcurrentGroup != nullptr);
    assert(mdMetricSet != nullptr);
    assert(d3dDevice != nullptr);
    assert(numRangesToAllocate != 0);

    D3D11Async = new ID3D11Asynchronous* [numRangesToAllocate];
    if (D3D11Async == nullptr) {
        return false;
    }
    memset(D3D11Async, 0, sizeof(ID3D11Asynchronous*) * numRangesToAllocate);

    MDDevice = mdDevice;
    MDConcurrentGroup = mdConcurrentGroup;
    MDMetricSet = mdMetricSet;

    // Check if this metric set supports v1.1 API.  If so, we'll use the
    // internal equation calculations (which require SetApiFiltering() to be
    // set)
    CanCallCalculateMetrics = MDH_DriverSupportsMDVersion(mdDevice, 1, 1, 0); // IMetricsSet_1_1::CalculateMetrics()
    if (CanCallCalculateMetrics) {
        auto mdMetricSet11 = (MetricsDiscovery::IMetricSet_1_1*) mdMetricSet;
        auto cc = mdMetricSet11->SetApiFiltering(MetricsDiscovery::API_TYPE_DX11);
        if (cc != MetricsDiscovery::CC_OK) {
            Finalize();
            return false;
        }
    }

    // Get metric set parameters that we'll cache
    auto metricSetParams = mdMetricSet->GetParams();
    assert(metricSetParams != nullptr);
    auto counterId = metricSetParams->ApiSpecificId.D3D1XDevDependentId;
    auto queryId = metricSetParams->ApiSpecificId.D3D1XQueryId;
    auto reportByteSize = metricSetParams->QueryReportSize;

    IsQuery = queryId != 0;

    // Create the D3D11 counter
    auto cc = MDMetricSet->Activate();
    if (cc != MetricsDiscovery::CC_OK) {
        Finalize();
        return false;
    }

    for (uint32_t i = 0; i < numRangesToAllocate; ++i) {
        HRESULT hr = S_OK;
        if (queryId == 0) {
            D3D11_COUNTER_DESC desc = {};
            desc.Counter = (D3D11_COUNTER) counterId;
            hr = d3dDevice->CreateCounter(&desc, (ID3D11Counter**) &D3D11Async[i]);
        } else {
            D3D11_QUERY_DESC desc = {};
            desc.Query = (D3D11_QUERY) queryId;
            hr = d3dDevice->CreateQuery(&desc, (ID3D11Query**) &D3D11Async[i]);
        }

        if (D3D11Async[i] == nullptr) {
            cc = MDMetricSet->Deactivate();
            Finalize();
            return false;
        }

        auto dataSize = D3D11Async[i]->GetDataSize();
        auto expectedDataSize = IsQuery ? reportByteSize : (uint32_t) sizeof(void*);
        assert(expectedDataSize == dataSize);
    }

    cc = MDMetricSet->Deactivate();
    (void) cc;

    ReportMemory.Initialize(mdMetricSet, numRangesToAllocate, MDH_RANGE_METRICS_REPORT);
    ReportValues.Initialize(mdMetricSet, numRangesToAllocate);
    return true;
}

void MDH_RangeMetricsDX11::Finalize()
{
    auto numRangesAllocated = ReportMemory.NumReportsAllocated;
    for (uint32_t i = 0; i < numRangesAllocated; ++i) {
        if (D3D11Async[i] != nullptr) {
            D3D11Async[i]->Release();
        }
    }
    delete[] D3D11Async;

    ReportMemory.Finalize();
    ReportValues.Finalize();

    MDDevice = nullptr;
    MDConcurrentGroup = nullptr;
    MDMetricSet = nullptr;
    D3D11Async = nullptr;
    CanCallCalculateMetrics = false;
    IsQuery = false;
}

void MDH_RangeMetricsDX11::BeginRange(
    ID3D11DeviceContext* deviceCtxt,
    uint32_t rangeIndex) const
{
    assert(deviceCtxt != nullptr);
    assert(rangeIndex < ReportMemory.NumReportsAllocated);
    assert(D3D11Async != nullptr);
    assert(D3D11Async[rangeIndex] != nullptr);

    deviceCtxt->Begin(D3D11Async[rangeIndex]);
}

void MDH_RangeMetricsDX11::EndRange(
    ID3D11DeviceContext* deviceCtxt,
    uint32_t rangeIndex) const
{
    assert(deviceCtxt != nullptr);
    assert(rangeIndex < ReportMemory.NumReportsAllocated);
    assert(D3D11Async != nullptr);
    assert(D3D11Async[rangeIndex] != nullptr);

    deviceCtxt->End(D3D11Async[rangeIndex]);
}

void MDH_RangeMetricsDX11::GetRangeReports(
    ID3D11DeviceContext* deviceCtxt,
    uint32_t firstRangeIndex,
    uint32_t numRanges) const
{
    assert(deviceCtxt != nullptr);
    assert(firstRangeIndex + numRanges <= ReportMemory.NumReportsAllocated);
    assert(D3D11Async != nullptr);

    auto reportData = ReportMemory.GetReportData(firstRangeIndex);
    assert(reportData != nullptr);

    // If IsQuery==true, the driver will return a pointer to the report data.
    // Otherwise, the driver returns the report data directly.

    void* dataPtr;
    uint32_t dataByteSize;
    void* driverAddr = nullptr;
    if (IsQuery) {
        dataPtr      = reportData;
        dataByteSize = ReportMemory.ReportByteSize;
    } else {
        dataPtr      = &driverAddr;
        dataByteSize = sizeof(void*);
    }

    for (uint32_t i = 0; i < numRanges; ++i) {
        auto rangeIndex = firstRangeIndex + i;

        assert(D3D11Async[rangeIndex] != nullptr);
        if (deviceCtxt->GetData(D3D11Async[rangeIndex], dataPtr, dataByteSize, 0) != S_OK) {
            do {
                Sleep(1);
            } while (deviceCtxt->GetData(D3D11Async[rangeIndex], dataPtr, dataByteSize, D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK);
        }
    }

    if (!IsQuery) {
        memcpy(reportData, driverAddr, ReportMemory.ReportByteSize * numRanges);
    }
}

void MDH_RangeMetricsDX11::ExecuteRangeEquations(
    ID3D11DeviceContext* deviceCtxt,
    uint32_t firstRangeIndex,
    uint32_t numRanges) const
{
    assert(MDDevice != nullptr);
    assert(MDMetricSet != nullptr);
    assert(firstRangeIndex + numRanges <= ReportMemory.NumReportsAllocated);

    if (CanCallCalculateMetrics) {
        auto mdMetricSet11 = (MetricsDiscovery::IMetricSet_1_1*) MDMetricSet;
        for (uint32_t i = 0; i < numRanges; ++i) {
            auto rangeIndex = firstRangeIndex + i;
            auto cc = mdMetricSet11->CalculateMetrics(
                (unsigned char const*) ReportMemory.GetReportData(rangeIndex),
                ReportMemory.ReportByteSize,
                ReportValues.GetReportValues(rangeIndex),
                ReportValues.NumReportValues * sizeof(*ReportValues.ReportValues),
                nullptr,
                false);
            MDH_CHECK_CC(cc);
        }
    } else {
        for (uint32_t i = 0; i < numRanges; ++i) {
            auto rangeIndex = firstRangeIndex + i;
            MDH_ExecuteEquations(
                MDDevice,
                MDMetricSet,
                nullptr,
                ReportMemory.GetReportData(rangeIndex),
                ReportValues.GetReportValues(rangeIndex),
                MDH_EQUATION_READ_RANGE | MDH_EQUATION_READ_INFORMATION | MDH_EQUATION_NORMALIZE);
        }
    }
}
