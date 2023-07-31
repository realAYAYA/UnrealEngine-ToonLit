// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

THIRD_PARTY_INCLUDES_START
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/PreWindowsApi.h"
#include <mftransform.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <codecapi.h>
#include <shlwapi.h>
#include <mfreadwrite.h>
#include <dxgi1_4.h>
#include "VWBTypes.h"
#include "Windows/PostWindowsApi.h"
#include "Windows/HideWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_END


class FLibVIOSO
{
public:
	static bool Initialize();
	static void Release();

	typedef VWB_ERROR(__stdcall * VWB_CreateAProc)(void* pDxDevice, char const* szConfigFile, char const* szChannelName, VWB_Warper** ppWarper, VWB_int logLevel, char const* szLogFile);
	static VWB_CreateAProc Create;
	
	typedef void(__stdcall * VWB_DestroyProc)(VWB_Warper* pWarper);
	static VWB_DestroyProc Destroy;

	typedef VWB_ERROR(__stdcall* VWB_InitProc)(VWB_Warper* pWarper);
	static VWB_InitProc Init;

	typedef VWB_ERROR(__stdcall* VWB_getViewClipProc)(VWB_Warper* pWarper, VWB_float* pEye, VWB_float* pRot, VWB_float* pView, VWB_float* pClip);
	static VWB_getViewClipProc GetViewClip;

	typedef VWB_ERROR(__stdcall* VWB_renderProc)(VWB_Warper* pWarper, VWB_param src, VWB_uint stateMask);
	static VWB_renderProc Render;

private:
	static bool bInitialized;
	static void* DllHandle;
	static FCriticalSection CritSec;
};
