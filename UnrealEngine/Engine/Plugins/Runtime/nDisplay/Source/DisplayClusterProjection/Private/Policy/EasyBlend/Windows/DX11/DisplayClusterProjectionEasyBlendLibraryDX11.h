// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ID3D11DynamicRHI.h"

THIRD_PARTY_INCLUDES_START
#include "EasyBlendSDKDXVer.h"
#include "EasyBlendSDKDXErrors.h"
#include "EasyBlendSDKFrustum.h"
#include "EasyBlendSDKDXStructs.h"
THIRD_PARTY_INCLUDES_END


struct EasyBlendSDKDX_Mesh;

struct DisplayClusterProjectionEasyBlendLibraryDX11
{
	static bool Initialize();
	static void Release();

	typedef EasyBlendSDKDXError(__stdcall *EasyBlendInitializeProc)(const char* szFileName, EasyBlendSDKDX_Mesh* msm);
	static EasyBlendInitializeProc EasyBlendInitializeFunc;
	
	typedef EasyBlendSDKDXError(__stdcall *EasyBlendUninitializeProc)(EasyBlendSDKDX_Mesh* msm);
	static EasyBlendUninitializeProc EasyBlendUninitializeFunc;
	
	typedef EasyBlendSDKDXError(__stdcall *EasyBlendInitDeviceObjectsProc)(EasyBlendSDKDX_Mesh* msm, ID3D1XDevice* pDevice, ID3D1XDeviceContext* pDeviceContext /* can be null in DX10 */, IDXGISwapChain* pSwapChain);
	static EasyBlendInitDeviceObjectsProc EasyBlendInitDeviceObjectsFunc;
	
	typedef EasyBlendSDKDXError(__stdcall *EasyBlendDXRenderProc)(EasyBlendSDKDX_Mesh* msm, ID3D1XDevice* id3d, ID3D1XDeviceContext* pDeviceContext /* can be null in DX10 */, IDXGISwapChain* iswapChain, bool doPresent);
	static EasyBlendDXRenderProc EasyBlendDXRenderFunc;
	
	typedef EasyBlendSDKDXError(__stdcall *EasyBlendSetEyepointProc)(EasyBlendSDKDX_Mesh* msm, const double& eyeX, const double& eyeY, const double& eyeZ);
	static EasyBlendSetEyepointProc EasyBlendSetEyepointFunc;

	typedef EasyBlendSDKDXError(__stdcall *EasyBlendSDK_GetHeadingPitchRollProc)(double& rdDegreesHeading, double& rdDegreesPitch, double& rdDegreesRoll, EasyBlendSDKDX_Mesh* msm);
	static EasyBlendSDK_GetHeadingPitchRollProc EasyBlendSDK_GetHeadingPitchRollFunc;
	
	typedef EasyBlendSDKDXError(__stdcall *EasyBlendSetInputTexture2DProc)(EasyBlendSDKDX_Mesh* msm, ID3D1XTexture2D* texture);
	static EasyBlendSetInputTexture2DProc EasyBlendSetInputTexture2DFunc;
	
	typedef EasyBlendSDKDXError(__stdcall *EasyBlendSetOutputTexture2DProc)(EasyBlendSDKDX_Mesh* msm, ID3D1XTexture2D* texture);
	static EasyBlendSetOutputTexture2DProc EasyBlendSetOutputTexture2DFunc;

private:
	static void* DllHandle;
	static FCriticalSection CritSec;
	static bool bInitializeOnce;
};
