// Copyright Epic Games, Inc. All Rights Reserved.

#include "Policy/EasyBlend/Windows/DX11/DisplayClusterProjectionEasyBlendLibraryDX11.h"
#include "DisplayClusterProjectionLog.h"

#include "IDisplayClusterProjection.h"

#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

THIRD_PARTY_INCLUDES_START
#include "EasyBlendSDKDXApi.h"
THIRD_PARTY_INCLUDES_END


void* DisplayClusterProjectionEasyBlendLibraryDX11::DllHandle = nullptr;
FCriticalSection DisplayClusterProjectionEasyBlendLibraryDX11::CritSec;
bool DisplayClusterProjectionEasyBlendLibraryDX11::bInitializeOnce = false;

//Dynamic DLL API:
DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendInitializeProc DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendInitializeFunc = nullptr;
DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendUninitializeProc DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendUninitializeFunc = nullptr;
DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendInitDeviceObjectsProc DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendInitDeviceObjectsFunc = nullptr;
DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendDXRenderProc DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendDXRenderFunc = nullptr;
DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendSetEyepointProc DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendSetEyepointFunc = nullptr;
DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendSDK_GetHeadingPitchRollProc DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendSDK_GetHeadingPitchRollFunc = nullptr;
DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendSetInputTexture2DProc DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendSetInputTexture2DFunc = nullptr;
DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendSetOutputTexture2DProc DisplayClusterProjectionEasyBlendLibraryDX11::EasyBlendSetOutputTexture2DFunc = nullptr;


void DisplayClusterProjectionEasyBlendLibraryDX11::Release()
{
	if (DllHandle)
	{
		FScopeLock lock(&CritSec);

		if (DllHandle)
		{
			EasyBlendInitializeFunc = nullptr;
			EasyBlendUninitializeFunc = nullptr;
			EasyBlendInitDeviceObjectsFunc = nullptr;
			EasyBlendDXRenderFunc = nullptr;
			EasyBlendSetEyepointFunc = nullptr;
			EasyBlendSetInputTexture2DFunc = nullptr;
			EasyBlendSetOutputTexture2DFunc = nullptr;

			FPlatformProcess::FreeDllHandle(DllHandle);
			DllHandle = nullptr;
			bInitializeOnce = false;
		}
	}
}

bool DisplayClusterProjectionEasyBlendLibraryDX11::Initialize()
{
	if (!DllHandle)
	{
		FScopeLock lock(&CritSec);

		if (!DllHandle && !bInitializeOnce)
		{
			bInitializeOnce = true;

			const FString LibName   = TEXT("mplEasyBlendSDKDX1164.dll");
			const FString PluginDir = IPluginManager::Get().FindPlugin(TEXT("nDisplay"))->GetBaseDir();
			const FString DllPath   = FPaths::Combine(PluginDir, TEXT("ThirdParty/EasyBlend/DLL"), LibName);
			
			// Try to load DLL
			DllHandle = FPlatformProcess::GetDllHandle(*DllPath);

			if (DllHandle)
			{
				const FString StrOk(TEXT("ok"));
				const FString StrFail(TEXT("NOT FOUND!"));

				// EasyBlendInitialize
				EasyBlendInitializeFunc = (EasyBlendInitializeProc)FPlatformProcess::GetDllExport((HMODULE)DllHandle, TEXT("EasyBlendInitialize"));
				check(EasyBlendInitializeFunc);
				UE_LOG(LogDisplayClusterProjectionEasyBlend, Verbose, TEXT("EasyBlend API: %s - %s."), TEXT("EasyBlendInitialize"), EasyBlendInitializeFunc ? *StrOk : *StrFail);

				// EasyBlendUninitialize
				EasyBlendUninitializeFunc = (EasyBlendUninitializeProc)FPlatformProcess::GetDllExport((HMODULE)DllHandle, TEXT("EasyBlendUninitialize"));
				check(EasyBlendUninitializeFunc);
				UE_LOG(LogDisplayClusterProjectionEasyBlend, Verbose, TEXT("EasyBlend API: %s - %s."), TEXT("EasyBlendUninitialize"), EasyBlendUninitializeFunc ? *StrOk : *StrFail);

				// EasyBlendInitDeviceObjects
				EasyBlendInitDeviceObjectsFunc = (EasyBlendInitDeviceObjectsProc)FPlatformProcess::GetDllExport((HMODULE)DllHandle, TEXT("EasyBlendInitDeviceObjects"));
				check(EasyBlendInitDeviceObjectsFunc);
				UE_LOG(LogDisplayClusterProjectionEasyBlend, Verbose, TEXT("EasyBlend API: %s - %s."), TEXT("EasyBlendInitDeviceObjects"), EasyBlendInitDeviceObjectsFunc ? *StrOk : *StrFail);

				// EasyBlendDXRender
				EasyBlendDXRenderFunc = (EasyBlendDXRenderProc)FPlatformProcess::GetDllExport((HMODULE)DllHandle, TEXT("EasyBlendDXRender"));
				check(EasyBlendDXRenderFunc);
				UE_LOG(LogDisplayClusterProjectionEasyBlend, Verbose, TEXT("EasyBlend API: %s - %s."), TEXT("EasyBlendDXRender"), EasyBlendDXRenderFunc ? *StrOk : *StrFail);

				// EasyBlendSetEyepoint
				EasyBlendSetEyepointFunc = (EasyBlendSetEyepointProc)FPlatformProcess::GetDllExport((HMODULE)DllHandle, TEXT("EasyBlendSetEyepoint"));
				check(EasyBlendSetEyepointFunc);
				UE_LOG(LogDisplayClusterProjectionEasyBlend, Verbose, TEXT("EasyBlend API: %s - %s."), TEXT("EasyBlendSetEyepoint"), EasyBlendSetEyepointFunc ? *StrOk : *StrFail);

				// EasyBlendSetInputTexture2D
				EasyBlendSetInputTexture2DFunc = (EasyBlendSetInputTexture2DProc)FPlatformProcess::GetDllExport((HMODULE)DllHandle, TEXT("EasyBlendSetInputTexture2D"));
				check(EasyBlendSetInputTexture2DFunc);
				UE_LOG(LogDisplayClusterProjectionEasyBlend, Verbose, TEXT("EasyBlend API: %s - %s."), TEXT("EasyBlendSetInputTexture2D"), EasyBlendSetInputTexture2DFunc ? *StrOk : *StrFail);

				// EasyBlendSetOutputTexture2D
				EasyBlendSetOutputTexture2DFunc = (EasyBlendSetOutputTexture2DProc)FPlatformProcess::GetDllExport((HMODULE)DllHandle, TEXT("EasyBlendSetOutputTexture2D"));
				check(EasyBlendSetOutputTexture2DFunc);
				UE_LOG(LogDisplayClusterProjectionEasyBlend, Verbose, TEXT("EasyBlend API: %s - %s."), TEXT("EasyBlendSetOutputTexture2D"), EasyBlendSetOutputTexture2DFunc ? *StrOk : *StrFail);

				// EasyBlendSDK_GetHeadingPitchRoll
				EasyBlendSDK_GetHeadingPitchRollFunc = (EasyBlendSDK_GetHeadingPitchRollProc)FPlatformProcess::GetDllExport((HMODULE)DllHandle, TEXT("EasyBlendSDK_GetHeadingPitchRoll"));
				check(EasyBlendSDK_GetHeadingPitchRollFunc);
				UE_LOG(LogDisplayClusterProjectionEasyBlend, Verbose, TEXT("EasyBlend API: %s - %s."), TEXT("EasyBlendSDK_GetHeadingPitchRoll"), EasyBlendSDK_GetHeadingPitchRollFunc ? *StrOk : *StrFail);
			}
			else
			{
				UE_LOG(LogDisplayClusterProjectionEasyBlend, Error, TEXT("Couldn't initialize EasyBlend API. No <%s> library found."), *DllPath);
				return false;
			}
		}
	}

	return DllHandle != nullptr;
}
