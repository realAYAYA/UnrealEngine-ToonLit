// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once
#include <iterator>

#include <unknwn.h>
#include <winrt/windows.foundation.h>
#include <winrt/windows.foundation.collections.h>
#include "winrt/windows.media.h"
#include "winrt/windows.media.core.h"
#include "winrt/windows.media.mediaproperties.h"
#include "winrt/windows.media.playback.h"

#include <windows.h>
#include <strsafe.h>
#include <combaseapi.h>

#include <mfidl.h>
#pragma comment(lib, "mfuuid")

#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
EXTERN_GUID(MFSampleExtension_DeviceTimestamp, 0x8f3e35e7, 0x2dcd, 0x4887, 0x86, 0x22, 0x2a, 0x58, 0xba, 0xa6, 0x52, 0xb0);
EXTERN_GUID(MFSampleExtension_Spatial_CameraCoordinateSystem, 0x9d13c82f, 0x2199, 0x4e67, 0x91, 0xcd, 0xd1, 0xa4, 0x18, 0x1f, 0x25, 0x34);
EXTERN_GUID(MFSampleExtension_Spatial_CameraViewTransform, 0x4e251fa4, 0x830f, 0x4770, 0x85, 0x9a, 0x4b, 0x8d, 0x99, 0xaa, 0x80, 0x9b);
EXTERN_GUID(MFSampleExtension_Spatial_CameraProjectionTransform, 0x47f9fcb5, 0x2a02, 0x4f26, 0xa4, 0x77, 0x79, 0x2f, 0xdf, 0x95, 0x88, 0x6a);
#endif

inline void __stdcall Log(
    _In_ _Printf_format_string_ STRSAFE_LPCWSTR pszFormat,
    ...)
{
    wchar_t szTextBuf[2048];

    va_list args;
    va_start(args, pszFormat);

    StringCchVPrintf(szTextBuf, _countof(szTextBuf), pszFormat, args);

    va_end(args);

    OutputDebugStringW(szTextBuf);
}

#ifndef IFR
#define IFR(HR) { HRESULT hrTest = HR; if (FAILED(hrTest)) { return hrTest; } } 
#endif

#ifndef NULL_CHK_HR
#define NULL_CHK_HR(pointer, HR) if (nullptr == pointer) { IFR(HR); }
#endif

#ifndef IFV
#define IFV(HR) { HRESULT hrTest = HR; if (FAILED(hrTest)) { return; } }
#endif

#ifndef IFT
#define IFT(HR) { HRESULT hrTest = HR; if (FAILED(hrTest)) { winrt::throw_hresult(hrTest); } }
#endif

#ifndef IFG
#define IFG(HR, marker) { HRESULT hrTest = HR; if (FAILED(hrTest)) { hr = hrTest; goto marker; } }
#endif

#ifndef NULL_CHK_R
#define NULL_CHK_R(pointer) if (nullptr == pointer) { return; }
#endif