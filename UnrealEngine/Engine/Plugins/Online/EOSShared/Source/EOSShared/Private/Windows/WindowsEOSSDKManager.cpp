// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EOS_SDK

#include "WindowsEOSSDKManager.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "EOSShared.h"
#include "Windows/eos_Windows.h"

FWindowsEOSSDKManager::FWindowsEOSSDKManager()
{
	PlatformSteamOptions.ApiVersion = 3; //EOS_INTEGRATEDPLATFORM_STEAM_OPTIONS_API_LATEST;
	PlatformSteamOptions.OverrideLibraryPath = nullptr;
	PlatformSteamOptions.SteamMajorVersion = 1;
	PlatformSteamOptions.SteamMinorVersion = 57;
	PlatformSteamOptions.SteamApiInterfaceVersionsArray = nullptr;
	PlatformSteamOptions.SteamApiInterfaceVersionsArrayBytes = 0;

	UE_EOS_CHECK_API_MISMATCH(EOS_INTEGRATEDPLATFORM_STEAM_OPTIONS_API_LATEST, 3);
}

FWindowsEOSSDKManager::~FWindowsEOSSDKManager()
{

}

IEOSPlatformHandlePtr FWindowsEOSSDKManager::CreatePlatform(const FEOSSDKPlatformConfig& PlatformConfig, EOS_Platform_Options& PlatformOptions)
{
	if (PlatformConfig.bWindowsEnableOverlayD3D9) PlatformOptions.Flags |= EOS_PF_WINDOWS_ENABLE_OVERLAY_D3D9;
	if (PlatformConfig.bWindowsEnableOverlayD3D10) PlatformOptions.Flags |= EOS_PF_WINDOWS_ENABLE_OVERLAY_D3D10;
	if (PlatformConfig.bWindowsEnableOverlayOpenGL) PlatformOptions.Flags |= EOS_PF_WINDOWS_ENABLE_OVERLAY_OPENGL;

	if (PlatformConfig.bEnableRTC)
	{
		static const FString XAudioPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/ThirdParty/Windows/XAudio2_9"), PLATFORM_64BITS ? TEXT("x64") : TEXT("x86"), TEXT("xaudio2_9redist.dll")));
		static const FTCHARToUTF8 Utf8XAudioPath(*XAudioPath);

		static EOS_Windows_RTCOptions WindowsRTCOptions = {};
		WindowsRTCOptions.ApiVersion = 1;
		UE_EOS_CHECK_API_MISMATCH(EOS_WINDOWS_RTCOPTIONS_API_LATEST, 1);
		WindowsRTCOptions.XAudio29DllPath = Utf8XAudioPath.Get();

		const_cast<EOS_Platform_RTCOptions*>(PlatformOptions.RTCOptions)->PlatformSpecificOptions = &WindowsRTCOptions;
	}

	return FEOSSDKManager::CreatePlatform(PlatformConfig, PlatformOptions);
}

const void* FWindowsEOSSDKManager::GetIntegratedPlatformOptions()
{
	return &PlatformSteamOptions;
}

EOS_IntegratedPlatformType FWindowsEOSSDKManager::GetIntegratedPlatformType()
{
	return EOS_IPT_Steam;
}


#endif // WITH_EOS_SDK