// Copyright Epic Games, Inc. All Rights Reserved.

#include "Policy/VIOSO/Windows/DisplayClusterProjectionVIOSOLibrary.h"
#include "DisplayClusterProjectionLog.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"

#define DeclareVIOSOFunc(LocalFunc, DLLFunc)\
	FLibVIOSO::DLLFunc##Proc     FLibVIOSO::LocalFunc = nullptr;

#define GetVIOSOFunc(LocalFunc,DLLFunc)\
	LocalFunc = (DLLFunc##Proc)FPlatformProcess::GetDllExport((HMODULE)DllHandle, TEXT(#DLLFunc));\
	if(LocalFunc==nullptr)\
		{\
			UE_LOG(LogDisplayClusterProjectionVIOSO, Error, TEXT("Couldn't initialize VIOSO API func '%s'."), TEXT(#DLLFunc));\
			FLibVIOSO::Release();\
			return false;\
		}

bool FLibVIOSO::bInitialized = false;
void* FLibVIOSO::DllHandle = nullptr;
FCriticalSection FLibVIOSO::CritSec;

DeclareVIOSOFunc(Create,      VWB_CreateA);
DeclareVIOSOFunc(Destroy,     VWB_Destroy);
DeclareVIOSOFunc(Init,        VWB_Init);
DeclareVIOSOFunc(GetViewClip, VWB_getViewClip)
DeclareVIOSOFunc(Render,      VWB_render);

bool FLibVIOSO::Initialize()
{
	if (!DllHandle && !bInitialized)
	{
		FScopeLock lock(&CritSec);

		// call once
		bInitialized = true;

		if (!DllHandle)
		{
			const FString LibName   = TEXT("ViosoWarpBlend64.dll");
			const FString PluginDir = IPluginManager::Get().FindPlugin(TEXT("nDisplay"))->GetBaseDir();
			const FString DllPath   = FPaths::Combine(PluginDir, TEXT("ThirdParty/VIOSO/DLL"), LibName);
			
			// Try to load DLL
			DllHandle = FPlatformProcess::GetDllHandle(*DllPath);

			if (DllHandle)
			{
				GetVIOSOFunc(Create,      VWB_CreateA);
				GetVIOSOFunc(Destroy,     VWB_Destroy);
				GetVIOSOFunc(Init,        VWB_Init);
				GetVIOSOFunc(GetViewClip, VWB_getViewClip);
				GetVIOSOFunc(Render,      VWB_render);
			}
			else
			{
				UE_LOG(LogDisplayClusterProjectionVIOSO, Error, TEXT("Couldn't initialize VIOSO API. No <%s> library found."), *DllPath);
				return false;
			}
		}
	}

	return DllHandle != nullptr;
}

void FLibVIOSO::Release()
{
	if (DllHandle)
	{
		FScopeLock lock(&CritSec);

		if (DllHandle)
		{
			Create = nullptr;
			Destroy = nullptr;
			Init = nullptr;
			GetViewClip = nullptr;
			Render = nullptr;

			FPlatformProcess::FreeDllHandle(DllHandle);
			DllHandle = nullptr;
		}
	}
}
