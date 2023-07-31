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

bool MDH_PeriodicMetricsSupported(
    MetricsDiscovery::IConcurrentGroup_1_0* mdConcurrentGroup)
{
    assert(mdConcurrentGroup != nullptr);
    auto params = mdConcurrentGroup->GetParams();
    auto measurementTypeMask = params->MeasurementTypeMask;
    return (measurementTypeMask & MetricsDiscovery::MEASUREMENT_TYPE_SNAPSHOT_IO) != 0;
}

bool MDH_StartSamplingPeriodicMetrics(
    MetricsDiscovery::IConcurrentGroup_1_0* mdConcurrentGroup,
    MetricsDiscovery::IMetricSet_1_0* mdMetricSet,
    MDH_ReportMemory const& mdhReportMemory,
    uint32_t targetProcessId,
    uint32_t* samplePeriodNs,
    uint32_t* numReportsBufferedByDriver)
{
    assert(mdConcurrentGroup != nullptr);
    assert(mdMetricSet != nullptr);
    assert(mdhReportMemory.ReportByteSize != 0);
    assert(samplePeriodNs != nullptr);
    assert(numReportsBufferedByDriver != nullptr);

    auto oaBufferByteSize = *numReportsBufferedByDriver * mdhReportMemory.ReportByteSize;

    auto cc = mdConcurrentGroup->OpenIoStream(
        mdMetricSet,
        targetProcessId,
        samplePeriodNs,
        &oaBufferByteSize);
    if (cc != MetricsDiscovery::CC_OK) {
        // Most likely this was left open by another application.
        //
        // It seems like there's no way out of this situation besides rebooting
        // the PC!
        return false;
    }

    *numReportsBufferedByDriver = oaBufferByteSize / mdhReportMemory.ReportByteSize;
    return true;
}

void MDH_StopSamplingPeriodicMetrics(
    MetricsDiscovery::IConcurrentGroup_1_0* mdConcurrentGroup)
{
    assert(mdConcurrentGroup != nullptr);
    auto cc = mdConcurrentGroup->CloseIoStream();
    MDH_CHECK_CC(cc);
}

uint32_t MDH_CopyDriverBufferedPeriodicReports(
    MetricsDiscovery::IConcurrentGroup_1_0* mdConcurrentGroup,
    MDH_ReportMemory* mdhReportMemory,
    uint32_t reportReadIndex,
    uint32_t reportWriteIndex)
{
    assert(mdhReportMemory != nullptr);
    assert(mdConcurrentGroup != nullptr);
    assert(mdhReportMemory->ReportData != nullptr);
    assert(mdhReportMemory->NumReportsAllocated != 0);
    assert(reportReadIndex <= reportWriteIndex);

    uint32_t numReportsRead = 0;
    for (;;) {
        // Determine the largets contiguous block of writable memory in the
        // circular buffer. The copy is allowed to overwrite reports starting
        // at reportWriteIndex (W) up to but not including the report at
        // reportReadIndex (R).
        //
        //  Case 1: reportReadBufferIdx > reportWriteBufferIdx
        //      ---------------------
        //      |xxxxW         Rxxxx|    numReportsToRead = reportReadBufferIdx - reportWriteBufferIdx
        //      ---------------------
        //
        //  Case 2: reportReadBufferIdx < reportWriteBufferIdx:
        //      ---------------------
        //      |    RxxxxxxxxxW    |    numReportsToRead = mdhReportMemory->NumReportsAllocated - reportWriteBufferIdx
        //      ---------------------
        //
        //  Case 3: reportReadBufferIdx == reportWriteBufferIdx (EMPTY: reportReadIndex == reportWriteIndex)
        //      ---------------------
        //      |         W         |    numReportsToRead = mdhReportMemory->NumReportsAllocated - reportWriteBufferIdx
        //      ---------------------
        //
        //  Case 4: reportReadBufferIdx == reportWriteBufferIdx (FULL: reportReadIndex + mdhReportMemory->NumReportsAllocated == reportWriteIndex)
        //      ---------------------
        //      |xxxxxxxxxWxxxxxxxxx|    numReportsToRead = 0
        //      ---------------------

        auto reportWriteBufferIdx = reportWriteIndex % mdhReportMemory->NumReportsAllocated;
        auto reportReadBufferIdx  = reportReadIndex  % mdhReportMemory->NumReportsAllocated;

        uint32_t numReportsToRead = 0;
        if (reportReadBufferIdx > reportWriteBufferIdx) { // Case 1
            numReportsToRead = reportReadBufferIdx - reportWriteBufferIdx;
        } else if (reportReadBufferIdx < reportWriteBufferIdx || reportReadIndex == reportWriteIndex) { // Case 2 || Case 3
            numReportsToRead = mdhReportMemory->NumReportsAllocated - reportWriteBufferIdx;
        } else { // Case 4
            break; // out of space, hope caller will consume a bunch
                   // TODO: probably want a warning or error, but DON'T assert
                   // without closing IoStream or you have to reboot
        }

        uint32_t readFlags = 0;
        auto cc = mdConcurrentGroup->ReadIoStream(
            &numReportsToRead,
            (char*) (mdhReportMemory->ReportData + reportWriteBufferIdx * mdhReportMemory->ReportByteSize),
            readFlags);

        // CC_OK and CC_READ_PENDING are both successful return codes
        //
        // Currently, I only seem to be getting CC_READ_PENDING returned.
        //
        // So, for now, just ignore return code and use numReportsToRead == 0
        // as exit condition.

        (void) cc;
        if (numReportsToRead == 0) {
            break; // there wern't any reports left to read at last ReadIoStream() call
        }

        // TODO: check for driver-side missed reports

        // Update reportWriteIndex and try again to cover cases 2 and 3, or
        // where driver didn't return as many reports as we have space for.

        assert(reportWriteIndex + numReportsToRead - reportReadIndex <= mdhReportMemory->NumReportsAllocated);
        reportWriteIndex += numReportsToRead;
        numReportsRead += numReportsToRead;
    }

    return numReportsRead;
}

uint64_t MDH_ExtendPeriodicReportTimestamps(
    MDH_ReportMemory* mdhReportMemory,
    uint32_t reportBeginIndex,
    uint32_t reportEndIndex,
    uint64_t latestReportTimestamp)
{
    assert(mdhReportMemory != nullptr);
    assert(mdhReportMemory->ReportData != nullptr);
    assert(mdhReportMemory->NumReportsAllocated != 0);
    assert(reportBeginIndex <= reportEndIndex);

    for (uint32_t reportIdx = reportBeginIndex; reportIdx < reportEndIndex; ++reportIdx) {
        auto reportBufferIdx = reportIdx % mdhReportMemory->NumReportsAllocated;
        auto pReport = mdhReportMemory->GetReportData(reportBufferIdx);
        auto pReportTimestamp = (uint64_t*)(pReport + 4);

        auto latestLo = latestReportTimestamp & 0x00000000ffffffffull;
        auto reportLo = *pReportTimestamp & 0x00000000ffffffffull;

        latestReportTimestamp &= 0xffffffff00000000ull;
        if (reportLo < latestLo) {
            latestReportTimestamp += 0x0000000100000000ull;
        }
        latestReportTimestamp += reportLo;
        *pReportTimestamp = latestReportTimestamp;
    }

    return latestReportTimestamp;
}
