// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenXRCore.h"
#include "HeadMountedDisplayTypes.h"

#define DEFINE_XR_ENTRYPOINTS(Type,Func) Type OpenXRDynamicAPI::Func = nullptr;
ENUM_XR_ENTRYPOINTS_GLOBAL(DEFINE_XR_ENTRYPOINTS);
ENUM_XR_ENTRYPOINTS(DEFINE_XR_ENTRYPOINTS);
DEFINE_XR_ENTRYPOINTS(PFN_xrGetInstanceProcAddr,xrGetInstanceProcAddr);

#define GET_XR_ENTRYPOINTS(Type,Func) if (!XR_ENSURE(xrGetInstanceProcAddr(Instance, #Func, (PFN_xrVoidFunction*)&Func))) \
		{ UE_LOG(LogHMD, Warning, TEXT("Failed to find entry point for %s"), TEXT(#Func)); return false; }

bool PreInitOpenXRCore(PFN_xrGetInstanceProcAddr InGetProcAddr)
{
	if (!InGetProcAddr)
	{
		return false;
	}
	xrGetInstanceProcAddr = InGetProcAddr;

	XrInstance Instance = XR_NULL_HANDLE;
	ENUM_XR_ENTRYPOINTS_GLOBAL(GET_XR_ENTRYPOINTS);
	return true;
}

bool InitOpenXRCore(XrInstance Instance)
{
	if (!Instance)
	{
		return false;
	}

	ENUM_XR_ENTRYPOINTS(GET_XR_ENTRYPOINTS);
	return true;
}

#undef GET_XR_ENTRYPOINTS
