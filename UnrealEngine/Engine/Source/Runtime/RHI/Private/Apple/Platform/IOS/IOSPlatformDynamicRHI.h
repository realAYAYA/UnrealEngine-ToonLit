// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


//------------------------------------------------------------------------------
// MARK: - iOS/tvOS Platform Dynamic RHI Defines
//


//------------------------------------------------------------------------------
// NOTE: Unreal Engine uses the same headers for iOS and tvOS; consequenty, we
//       use PLATFORM defines in this _rare_ instance to differentiate shader
//       platform types.
//------------------------------------------------------------------------------


#if PLATFORM_TVOS
#define SHADER_PLATFORM_METAL_MRT	SP_METAL_MRT_TVOS
#define SHADER_PLATFORM_METAL		SP_METAL_TVOS
#elif WITH_IOS_SIMULATOR
#define SHADER_PLATFORM_METAL		SP_METAL_SIM
#define SHADER_PLATFORM_METAL_MRT	SP_METAL_MRT
#else
#define SHADER_PLATFORM_METAL_MRT	SP_METAL_MRT
#define SHADER_PLATFORM_METAL		SP_METAL
#endif

//------------------------------------------------------------------------------
// MARK: - iOS/tvOS Platform Dynamic RHI Routines
//

namespace UE
{
namespace FIOSPlatformDynamicRHI
{

bool ShouldPreferFeatureLevelES31()
{
	return FParse::Param(FCommandLine::Get(), TEXT("metal"));
}

FORCEINLINE bool ShouldSupportMetalMRT()
{
	bool bSupportsMetalMRT = false;
	GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bSupportsMetalMRT"), bSupportsMetalMRT, GEngineIni);
	return (bSupportsMetalMRT || FParse::Param(FCommandLine::Get(), TEXT("metalmrt"))) && !ShouldPreferFeatureLevelES31();
}

void AddTargetedShaderFormats(TArray<FString>& TargetedShaderFormats)
{
	if (ShouldSupportMetalMRT())
	{
		TargetedShaderFormats.Add(LegacyShaderPlatformToShaderFormat(SHADER_PLATFORM_METAL_MRT).ToString());
	}
	TargetedShaderFormats.Add(LegacyShaderPlatformToShaderFormat(SHADER_PLATFORM_METAL).ToString());
}

} // namespace FIOSPlatformDynamicRHI
} // namespace UE

namespace FPlatformDynamicRHI = UE::FIOSPlatformDynamicRHI;
