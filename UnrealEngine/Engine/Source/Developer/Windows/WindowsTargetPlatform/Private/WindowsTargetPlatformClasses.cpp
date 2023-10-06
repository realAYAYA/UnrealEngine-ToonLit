// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindowsTargetSettings.h"

/* UWindowsTargetSettings structors
 *****************************************************************************/

UWindowsTargetSettings::UWindowsTargetSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
	, CacheSizeKB(65536)
	, MaxSampleRate(48000)
	, HighSampleRate(32000)
	, MedSampleRate(24000)
	, LowSampleRate(12000)
	, MinSampleRate(8000)
	, CompressionQualityModifier(1)
{
	// Default windows settings
	AudioSampleRate = 48000;
	AudioCallbackBufferFrameSize = 1024;
	AudioNumBuffersToEnqueue = 1;
	AudioNumSourceWorkers = 4;
}

static bool FilterShaderPlatform_D3D12(const FString& InShaderPlatform)
{
	return InShaderPlatform == TEXT("PCD3D_SM6") || InShaderPlatform == TEXT("PCD3D_SM5") || InShaderPlatform == TEXT("PCD3D_ES31");
}

static bool FilterShaderPlatform_D3D11(const FString& InShaderPlatform)
{
	return InShaderPlatform == TEXT("PCD3D_SM5") || InShaderPlatform == TEXT("PCD3D_ES31");
}

static bool FilterShaderPlatform_Vulkan(const FString& InShaderPlatform)
{
	return InShaderPlatform == TEXT("SF_VULKAN_SM5") || InShaderPlatform == TEXT("SF_VULKAN_SM6");
}

template<typename TFilterFunc>
static void AddToShaderFormatList(TArray<FString>& Dest, const TArray<FString>& Source, TFilterFunc FilterFunc)
{
	for (const FString& ShaderFormat : Source)
	{
		if (FilterFunc(ShaderFormat))
		{
			Dest.AddUnique(ShaderFormat);
		}
	}
}

void UWindowsTargetSettings::PostInitProperties()
{
	Super::PostInitProperties();

	if (!TargetedRHIs_DEPRECATED.IsEmpty())
	{
		AddToShaderFormatList(D3D12TargetedShaderFormats, TargetedRHIs_DEPRECATED, &FilterShaderPlatform_D3D12);
		AddToShaderFormatList(D3D11TargetedShaderFormats, TargetedRHIs_DEPRECATED, &FilterShaderPlatform_D3D11);
		AddToShaderFormatList(VulkanTargetedShaderFormats, TargetedRHIs_DEPRECATED, &FilterShaderPlatform_Vulkan);

		TargetedRHIs_DEPRECATED.Empty();
	}
}
