// Copyright Epic Games, Inc. All Rights Reserved.

#include "AndroidTargetPlatformSettings.h"
#include "CoreMinimal.h"
#include "UObject/NameTypes.h"
#include "CoreTypes.h"
#include "AnalyticsEventAttribute.h"
#include "HAL/IConsoleManager.h"

FAndroidTargetPlatformSettings::FAndroidTargetPlatformSettings(const TCHAR* CookFlavor, const TCHAR* OverrideIniPlatformName)
	: TTargetPlatformSettingsBase(CookFlavor, OverrideIniPlatformName)
	, MobileShadingPath(0)
	, bDistanceField(false)
	, bMobileForwardEnableClusteredReflections(false)
	, bMobileVirtualTextures(false)
{
#if WITH_ENGINE
	TextureLODSettings = nullptr; // These are registered by the device profile system.
	StaticMeshLODSettings.Initialize(this);
	GetConfigSystem()->GetBool(TEXT("/Script/Engine.RendererSettings"), TEXT("r.DistanceFields"), bDistanceField, GEngineIni);
	GetConfigSystem()->GetInt(TEXT("/Script/Engine.RendererSettings"), TEXT("r.Mobile.ShadingPath"), MobileShadingPath, GEngineIni);
	GetConfigSystem()->GetBool(TEXT("/Script/Engine.RendererSettings"), TEXT("r.Mobile.Forward.EnableClusteredReflections"), bMobileForwardEnableClusteredReflections, GEngineIni);
	GetConfigSystem()->GetBool(TEXT("/Script/Engine.RendererSettings"), TEXT("r.Mobile.VirtualTextures"), bMobileVirtualTextures, GEngineIni);
#endif
}

bool FAndroidTargetPlatformSettings::SupportsES31() const
{
	// default no support for ES31
	bool bBuildForES31 = false;
#if WITH_ENGINE
	GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bBuildForES31"), bBuildForES31, GEngineIni);
#endif
	return bBuildForES31;
}

bool FAndroidTargetPlatformSettings::SupportsVulkan() const
{
	// default to not supporting Vulkan
	bool bSupportsVulkan = false;
#if WITH_ENGINE
	GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bSupportsVulkan"), bSupportsVulkan, GEngineIni);
#endif
	return bSupportsVulkan;
}

bool FAndroidTargetPlatformSettings::SupportsVulkanSM5() const
{
	// default to no support for VulkanSM5
	bool bSupportsMobileVulkanSM5 = false;
#if WITH_ENGINE
	GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bSupportsVulkanSM5"), bSupportsMobileVulkanSM5, GEngineIni);
#endif
	return bSupportsMobileVulkanSM5;
}

bool FAndroidTargetPlatformSettings::SupportsFeature(ETargetPlatformFeatures Feature) const
{
	switch (Feature)
	{
	case ETargetPlatformFeatures::Packaging:
	case ETargetPlatformFeatures::DeviceOutputLog:
		return true;

	case ETargetPlatformFeatures::LowQualityLightmaps:
	case ETargetPlatformFeatures::MobileRendering:
		return SupportsES31() || SupportsVulkan();

	case ETargetPlatformFeatures::HighQualityLightmaps:
	case ETargetPlatformFeatures::DeferredRendering:
		return SupportsVulkanSM5();

	case ETargetPlatformFeatures::VirtualTextureStreaming:
		// TODO: should it check r.VirtualTextures for SM5 renderer ?
		return bMobileVirtualTextures;

	case ETargetPlatformFeatures::DistanceFieldAO:
		return UsesDistanceFields();


	case ETargetPlatformFeatures::NormalmapLAEncodingMode:
	{
		static IConsoleVariable* CompressorCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("cook.ASTCTextureCompressor"));
		const bool bUsesARMCompressor = (CompressorCVar ? (CompressorCVar->GetInt() != 0) : false);
		return SupportsTextureFormatCategory(EAndroidTextureFormatCategory::ASTC) && bUsesARMCompressor;
	}

	default:
		break;
	}

	return TTargetPlatformSettingsBase<FAndroidPlatformProperties>::SupportsFeature(Feature);
}

#if WITH_ENGINE
void FAndroidTargetPlatformSettings::GetReflectionCaptureFormats(TArray<FName>& OutFormats) const
{
	const bool bMobileDeferredShading = (MobileShadingPath == 1);

	if (SupportsVulkanSM5() || bMobileDeferredShading || bMobileForwardEnableClusteredReflections)
	{
		// use Full HDR with SM5 and Mobile Deferred
		OutFormats.Add(FName(TEXT("FullHDR")));
	}

	// always emit encoded
	OutFormats.Add(FName(TEXT("EncodedHDR")));
}
const UTextureLODSettings& FAndroidTargetPlatformSettings::GetTextureLODSettings() const
{
	return *TextureLODSettings;
}
#endif

void FAndroidTargetPlatformSettings::GetAllPossibleShaderFormats(TArray<FName>& OutFormats) const
{
	static FName NAME_SF_VULKAN_ES31_ANDROID(TEXT("SF_VULKAN_ES31_ANDROID"));
	static FName NAME_GLSL_ES3_1_ANDROID(TEXT("GLSL_ES3_1_ANDROID"));
	static FName NAME_SF_VULKAN_SM5_ANDROID(TEXT("SF_VULKAN_SM5_ANDROID"));

	if (SupportsVulkan())
	{
		OutFormats.AddUnique(NAME_SF_VULKAN_ES31_ANDROID);
	}

	if (SupportsVulkanSM5())
	{
		OutFormats.AddUnique(NAME_SF_VULKAN_SM5_ANDROID);
	}

	if (SupportsES31())
	{
		OutFormats.AddUnique(NAME_GLSL_ES3_1_ANDROID);
	}
}

void FAndroidTargetPlatformSettings::GetAllTargetedShaderFormats(TArray<FName>& OutFormats) const
{
	GetAllPossibleShaderFormats(OutFormats);
}


#if WITH_ENGINE
const FStaticMeshLODSettings& FAndroidTargetPlatformSettings::GetStaticMeshLODSettings() const
{
	return StaticMeshLODSettings;
}

#endif