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

#include <metrics_discovery/metrics_discovery_helper.h>

#include <algorithm>
#include <stdint.h>
#include <stdio.h>

namespace {

void usage()
{
    fprintf(stderr, "usage: periodic_sample.exe concurrentGroupName metricSetName metricName\n");
}

}

int main(
    int argc,
    char** argv)
{
    // Read command line arguments

    char const* mdConcurrentGroupName = "OA";
    char const* mdMetricSetName = "RenderBasic";
    char const* mdMetricName = "EuActive";
    if (argc == 4) {
        mdConcurrentGroupName = argv[1];
        mdMetricSetName = argv[2];
        mdMetricName = argv[3];
    }

    // Initialize the MDH context

    MDH_Context mdhContext;
    auto result = mdhContext.Initialize();
    if (result != MDH_Context::RESULT_OK) {
        fprintf(stderr, "error: failed to initialize MDH_Context\n");
        return 1;
    }

    // Search for the specified Metric

    auto mdConcurrentGroup = MDH_FindConcurrentGroup(mdhContext.MDDevice, mdConcurrentGroupName);
    if (mdConcurrentGroup == nullptr) {
        mdhContext.Finalize();
        fprintf(stderr, "error: failed to find concurrent group '%s'\n", mdConcurrentGroupName);
        usage();
        return 1;
    }

    if (!MDH_PeriodicMetricsSupported(mdConcurrentGroup)) {
        fprintf(stderr, "error: concurrent group '%s' does not support periodic sampling\n", mdConcurrentGroupName);
        mdhContext.Finalize();
        return 1;
    }

    auto mdMetricSet = MDH_FindMetricSet(mdConcurrentGroup, mdMetricSetName);
    if (mdMetricSet == nullptr) {
        mdhContext.Finalize();
        fprintf(stderr, "error: failed to find metric set '%s'\n", mdMetricSetName);
        usage();
        return 1;
    }

    auto metricIndex = MDH_FindMetric(mdMetricSet, mdMetricName);
    if (metricIndex == (uint32_t) -1) {
        mdhContext.Finalize();
        fprintf(stderr, "error: failed to find metric '%s'\n", mdMetricName);
        usage();
        return 1;
    }

    // Parameters for the collection: collect information on all processes,
    // sampling every millisecond, and buffering about one second's worth of
    // reports.

    uint32_t targetProcessId            = 0; // 0 means system-wide
    uint32_t samplePeriodNs             = 1000*1000;
    uint32_t numReportsToAllocate       = 1024;
    uint32_t numReportsBufferedByDriver = 1024;

    // Allocate memory to act as a ring buffer for raw reports.
    // reportReadIndex represents the next report that we will read, and
    // reportWriteIndex represents the next report that
    // MDH_CopyDriverBufferdPeriodicReports() will write to.

    uint32_t reportReadIndex = 0;
    uint32_t reportWriteIndex = 0;
    MDH_ReportMemory mdhReportMemory;
    mdhReportMemory.Initialize(mdMetricSet, numReportsToAllocate, MDH_PERIODIC_METRICS_REPORT);

    MDH_ReportValues mdhReportValues;
    mdhReportValues.Initialize(mdMetricSet, 1);
    auto reportValues = mdhReportValues.GetReportValues(0);

    // Start collecting reports every millisecond across all processes
    if (MDH_StartSamplingPeriodicMetrics(mdConcurrentGroup, mdMetricSet, mdhReportMemory,
        targetProcessId, &samplePeriodNs, &numReportsBufferedByDriver)) {

        // Report collection settings, since implementation may change
        // samplePeriodNs and numReportsBufferedByDriver.

        printf("Sampling started...\n");
        printf("    target process ID              = %u\n", targetProcessId);
        printf("    sample period                  = %u ns\n", samplePeriodNs);
        printf("    num reports allocated (user)   = %u\n", numReportsToAllocate);
        printf("    num reports buffered by driver = %u\n", numReportsBufferedByDriver);

        // Copy collected reports for 5 seconds, or until the mdhReportMemory
        // is full (whichever is first).
        //
        // A report will not necessarily be generated every millisecond.  e.g.,
        // if the GPU powers down due to inactivity then periodic metrics stop
        // getting collected (especially common if the Intel GPU is not the
        // primary adapter).

        LARGE_INTEGER freq = {};
        LARGE_INTEGER t0 = {};
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&t0);
        for (;;) {
            printf(".");

            auto readCount = MDH_CopyDriverBufferedPeriodicReports(mdConcurrentGroup, &mdhReportMemory, reportReadIndex, reportWriteIndex);
            reportWriteIndex += readCount;

            for (uint32_t i = 0; i < readCount; ++i) {
                printf("+");
            }

            LARGE_INTEGER t = {};
            QueryPerformanceCounter(&t);
            auto timeMs = 1000.f * (t.QuadPart - t0.QuadPart) / freq.QuadPart;

            if (reportWriteIndex >= numReportsToAllocate) {
                printf("\nAllocated report memory is full!\n");
                break;
            }

            if (timeMs >= 5000.f) {
                printf("\n5 second capture complete!\n");
                break;
            }

            Sleep(100);
        }

        // Disable the periodic sample collection.

        MDH_StopSamplingPeriodicMetrics(mdConcurrentGroup);
    } else {
        fprintf(stderr, "error: failed to start sampling\n");
    }

    // Process the collected reports, and output the specified metric values

    printf("%u reports collected\n", reportWriteIndex);
    if (reportWriteIndex > 0) {
        printf("    TIMESTAMP        %s\n", mdMetricName);

        MDH_ExtendPeriodicReportTimestamps(&mdhReportMemory, 0, reportWriteIndex, 0);

        uint8_t* prevReportData = nullptr;
        auto reportPrintCount = std::min(15u, reportWriteIndex);
        for (uint32_t reportIdx = 0; reportIdx < reportPrintCount; ++reportIdx) {
            auto reportData = mdhReportMemory.GetReportData(reportIdx);

            if (prevReportData != nullptr) {
                MDH_ExecuteEquations(mdhContext.MDDevice, mdMetricSet, prevReportData, reportData,
                    reportValues, MDH_EQUATION_READ_PERIODIC | MDH_EQUATION_NORMALIZE);

                auto timestamp = *(uint64_t*) (reportData + 4);
                printf("    %016llx ", timestamp);

                auto value = &reportValues[metricIndex];
                switch (value->ValueType) {
                case MetricsDiscovery::VALUE_TYPE_UINT32: printf("%u",   value->ValueUInt32); break;
                case MetricsDiscovery::VALUE_TYPE_UINT64: printf("%llu", value->ValueUInt64); break;
                case MetricsDiscovery::VALUE_TYPE_FLOAT:  printf("%f",   value->ValueFloat);  break;
                }

                printf("\n");
            }

            prevReportData = reportData;
        }
    }

    // Clean up the metric memory and context

    mdhReportMemory.Finalize();
    mdhReportValues.Finalize();
    mdhContext.Finalize();
    return 0;
}
