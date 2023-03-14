// Copyright Epic Games, Inc. All Rights Reserved.

#include "MicrosoftCommon.h"

#if PLATFORM_WINDOWS

#if WMFMEDIA_SUPPORTED_PLATFORM
	#pragma comment(lib, "mfplat")
	#pragma comment(lib, "mfuuid")
	#pragma comment(lib, "Mfreadwrite")
#endif

namespace AVEncoder
{

//
// Windows only code
//
//#if PLATFORM_WINDOWS
//ID3D11Device* GetUE4DxDevice()
//{
//	auto Device = static_cast<ID3D11Device*>(GDynamicRHI->RHIGetNativeDevice());
//	checkf(Device != nullptr, TEXT("Failed to get UE4's ID3D11Device"));
//	return Device;
//}
//#endif
//


} // namespace AVEncoder

#endif // PLATFORM_WINDOWS

