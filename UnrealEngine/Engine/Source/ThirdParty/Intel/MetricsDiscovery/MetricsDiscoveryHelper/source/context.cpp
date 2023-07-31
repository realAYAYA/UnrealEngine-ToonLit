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
#include <map>
#include <string>
#include <vector>

#define INITGUID
#include <devpkey.h>
#include <initguid.h>
#include <ntddvdeo.h>
#include <shlwapi.h>
#include <setupapi.h>

namespace {

void* OpenDllHandle()
{
#ifdef _WIN64
    wchar_t const* dllFilename = L"igdmd64.dll";
#else
    wchar_t const* dllFilename = L"igdmd32.dll";
#endif

    // First, try to load dll from the path
    static_assert(sizeof(void*) >= sizeof(HMODULE), "Can't store HMODULE into void*");
    auto handle = (void*) LoadLibraryW(dllFilename);
    if (handle != nullptr) {
        return handle;
    }

    // If that failed, try to load it from the DriverStore
    GUID guid = GUID_DISPLAY_DEVICE_ARRIVAL;
    auto devices = SetupDiGetClassDevs(&guid, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    if (devices == INVALID_HANDLE_VALUE) {
        return nullptr;
    }

    for (uint32_t deviceIndex = 0; ; ++deviceIndex) {
        SP_DEVINFO_DATA devInfo = {};
        devInfo.cbSize = sizeof(SP_DEVINFO_DATA);
        if (!SetupDiEnumDeviceInfo(devices, deviceIndex, &devInfo)) {
            break;
        }

        DWORD propertyType = 0;
        wchar_t hardwareIds[MAX_PATH] = {};
        if (!SetupDiGetDevicePropertyW(devices, &devInfo, &DEVPKEY_Device_HardwareIds,
                &propertyType, (PBYTE) hardwareIds, sizeof(hardwareIds), nullptr, 0)) {
            break;
        }

        if (wcsncmp(hardwareIds, L"PCI\\VEN_8086&DEV_", 17) != 0) {
            continue;
        }

        wchar_t path[MAX_PATH] = {};
        if (!SetupDiGetDevicePropertyW(devices, &devInfo, &DEVPKEY_Device_DriverInfPath,
                &propertyType, (PBYTE) path, sizeof(path), nullptr, 0) ||
            !SetupGetInfDriverStoreLocationW(path, nullptr, nullptr, path, MAX_PATH, nullptr)) {
            break;
        }

		PathRemoveFileSpecW(path);

		if (PathAppendW(path, dllFilename) != TRUE) {
			break;
		}

        handle = (void*) LoadLibraryW(path);
        break;
    }

    SetupDiDestroyDeviceInfoList(devices);
    return handle;
}

void CloseDllHandle(
    void* dllHandle)
{
    assert(dllHandle != nullptr);
    FreeLibrary((HMODULE) dllHandle);
}

void* GetDllFnPtr(
    void* dllHandle,
    char const* functionName)
{
    assert(dllHandle != nullptr);
    return GetProcAddress((HMODULE) dllHandle, functionName);
}

}

MDH_Context::Result MDH_Context::Initialize()
{
    assert(MDDevice == nullptr);
    assert(DLLHandle == nullptr);

    DLLHandle = OpenDllHandle();
    if (DLLHandle == nullptr) {
        return RESULT_MD_DLL_NOT_FOUND;
    }

    auto OpenMetricsDevice = (MetricsDiscovery::OpenMetricsDevice_fn) GetDllFnPtr(DLLHandle, "OpenMetricsDevice");
    if (OpenMetricsDevice == nullptr) {
        Finalize();
        return RESULT_MD_VERSION_MISMATCH;
    }

    auto cc = OpenMetricsDevice(&MDDevice);
    if (cc != MetricsDiscovery::CC_OK || MDDevice == nullptr) {
        Finalize();
        return RESULT_MD_VERSION_MISMATCH;
    }

    return RESULT_OK;
}

void MDH_Context::Finalize()
{
    if (DLLHandle != nullptr) {
        if (MDDevice != nullptr) {
            auto CloseMetricsDevice = (MetricsDiscovery::CloseMetricsDevice_fn) GetDllFnPtr(DLLHandle, "CloseMetricsDevice");
            if (CloseMetricsDevice != nullptr) {
                auto cc = CloseMetricsDevice(MDDevice);
                (void) cc;
            }
        }

        CloseDllHandle(DLLHandle);
    }

    MDDevice  = nullptr;
    DLLHandle = nullptr;
}

MDH_Version MDH_GetAPIVersion()
{
    MDH_Version version = {};
    version.MajorVersion = MetricsDiscovery::MD_API_MAJOR_NUMBER_CURRENT;
    version.MinorVersion = MetricsDiscovery::MD_API_MINOR_NUMBER_CURRENT;
    version.BuildVersion =                   MD_API_BUILD_NUMBER_CURRENT;
    return version;
}

MDH_Version MDH_GetDriverVersion(
    MetricsDiscovery::IMetricsDevice_1_0* mdDevice)
{
    assert(mdDevice != nullptr);

    auto deviceParams = mdDevice->GetParams();
    assert(deviceParams != nullptr);

    MDH_Version version = {};
    version.MajorVersion = deviceParams->Version.MajorNumber;
    version.MinorVersion = deviceParams->Version.MinorNumber;
    version.BuildVersion = deviceParams->Version.BuildNumber;
    return version;
}

bool MDH_DriverSupportsMDVersion(
    MetricsDiscovery::IMetricsDevice_1_0* mdDevice,
    uint32_t minMajorVersion,
    uint32_t minMinorVersion,
    uint32_t minBuildVersion)
{
    auto driverVersion = MDH_GetDriverVersion(mdDevice);
    return
        driverVersion.MajorVersion > minMajorVersion || (driverVersion.MajorVersion == minMajorVersion && (
        driverVersion.MinorVersion > minMinorVersion || (driverVersion.MinorVersion == minMinorVersion && (
        driverVersion.BuildVersion >= minBuildVersion))));
}

MetricsDiscovery::IConcurrentGroup_1_0* MDH_FindConcurrentGroup(
    MetricsDiscovery::IMetricsDevice_1_0* device,
    char const* symbolName)
{
    if (device == nullptr) {
        return nullptr;
    }

    auto deviceParams = device->GetParams();
    if (deviceParams == nullptr) {
        return nullptr;
    }

    for (uint32_t i = 0, N = deviceParams->ConcurrentGroupsCount; i < N; ++i) {
        auto concurrentGroup = device->GetConcurrentGroup(i);
        if (concurrentGroup == nullptr) {
            continue;
        }

        auto concurrentGroupParams = concurrentGroup->GetParams();
        if (concurrentGroupParams == nullptr) {
            continue;
        }

        if (strcmp(concurrentGroupParams->SymbolName, symbolName) == 0) {
            return concurrentGroup;
        }
    }

    return nullptr;
}

MetricsDiscovery::IMetricSet_1_0* MDH_FindMetricSet(
    MetricsDiscovery::IConcurrentGroup_1_0* concurrentGroup,
    char const* symbolName)
{
    if (concurrentGroup == nullptr) {
        return nullptr;
    }

    auto concurrentGroupParams = concurrentGroup->GetParams();
    if (concurrentGroupParams == nullptr) {
        return nullptr;
    }

    auto metricSetsCount = concurrentGroupParams->MetricSetsCount;
    for (uint32_t i = 0; i < metricSetsCount; ++i) {
        auto metricSet = concurrentGroup->GetMetricSet(i);
        if (metricSet == nullptr) {
            continue;
        }

        auto metricSetParams = metricSet->GetParams();
        if (metricSetParams == nullptr) {
            continue;
        }

        if (strcmp(metricSetParams->SymbolName, symbolName) == 0) {
            return metricSet;
        }
    }

    return nullptr;
}

uint32_t MDH_FindMetric(
    MetricsDiscovery::IMetricSet_1_0* mdMetricSet,
    char const* desiredMetricSymbolName)
{
    if (mdMetricSet == nullptr) {
        return UINT32_MAX;
    }

    auto mdMetricSetParams = mdMetricSet->GetParams();
    if (mdMetricSetParams == nullptr) {
        return UINT32_MAX;
    }

    auto mdMetricsCount = mdMetricSetParams->MetricsCount;
    auto mdInfoCount = mdMetricSetParams->InformationCount;

    for (uint32_t mdMetricIdx = 0; mdMetricIdx < mdMetricsCount; ++mdMetricIdx) {
        auto mdMetric = mdMetricSet->GetMetric(mdMetricIdx);
        if (mdMetric == nullptr) {
            continue;
        }

        auto mdMetricParams = mdMetric->GetParams();
        if (mdMetricParams == nullptr) {
            continue;
        }

        if (strcmp(desiredMetricSymbolName, mdMetricParams->SymbolName) == 0) {
            return mdMetricIdx;
        }
    }

    for (uint32_t mdInfoIdx = 0; mdInfoIdx < mdInfoCount; ++mdInfoIdx) {
        auto mdInfo = mdMetricSet->GetInformation(mdInfoIdx);
        if (mdInfo == nullptr) {
            continue;
        }

        auto mdInfoParams = mdInfo->GetParams();
        if (mdInfoParams == nullptr) {
            continue;
        }

        if (strcmp(desiredMetricSymbolName, mdInfoParams->SymbolName) == 0) {
            return mdMetricsCount + mdInfoIdx;
        }
    }

    return UINT32_MAX;
}

MetricsDiscovery::TTypedValue_1_0 MDH_FindGlobalSymbol(
    MetricsDiscovery::IMetricsDevice_1_0* mdDevice,
    char const* desiredGlobalSymbolName)
{
    MetricsDiscovery::TTypedValue_1_0 notFound = {};
    notFound.ValueType=MetricsDiscovery::VALUE_TYPE_LAST;

    if (mdDevice == nullptr) {
        return notFound;
    }

    auto mdDeviceParams = mdDevice->GetParams();
    if (mdDeviceParams == nullptr) {
        return notFound;
    }

    auto globalSymbolsCount = mdDeviceParams->GlobalSymbolsCount;
    for (uint32_t i = 0; i < globalSymbolsCount; ++i) {
        auto globalSymbol = mdDevice->GetGlobalSymbol(i);
        if (globalSymbol != nullptr &&
            strcmp(globalSymbol->SymbolName, desiredGlobalSymbolName) == 0) {
            return globalSymbol->SymbolTypedValue;
        }
    }

    return notFound;
}

MetricsDiscovery::IOverride_1_2* MDH_FindOverride(
    MetricsDiscovery::IMetricsDevice_1_0* mdDevice,
    char const* desiredOverrideName)
{
    if (mdDevice == nullptr) {
        return nullptr;
    }

    auto version = MDH_GetDriverVersion(mdDevice);
    if ((version.MajorVersion == 0) ||
        (version.MajorVersion == 1 && version.MinorVersion < 2)) {
        return nullptr;
    }

    auto mdDevice12 = (MetricsDiscovery::IMetricsDevice_1_2*) mdDevice;

    auto mdDeviceParams = mdDevice12->GetParams();
    if (mdDeviceParams == nullptr) {
        return nullptr;
    }

    auto overrideCount = mdDeviceParams->OverrideCount;
    for (uint32_t i = 0; i < overrideCount; ++i) {
        auto ovrride = mdDevice12->GetOverride(i);
        if (ovrride != nullptr &&
            strcmp(ovrride->GetParams()->SymbolName, desiredOverrideName) == 0) {
            return ovrride;
        }
    }

    return nullptr;
}

char const* MDH_GetMetricUnits(
    MetricsDiscovery::IMetricSet_1_0* mdMetricSet,
    uint32_t metricIndex)
{
    assert(mdMetricSet != nullptr);
    assert(mdMetricSet->GetParams() != nullptr);
    assert(metricIndex < mdMetricSet->GetParams()->MetricsCount);

    auto metric = mdMetricSet->GetMetric(metricIndex);
    assert(metric != nullptr);

    auto metricParams = metric->GetParams();
    assert(metricParams != nullptr);

    return metricParams->MetricResultUnits;
}

