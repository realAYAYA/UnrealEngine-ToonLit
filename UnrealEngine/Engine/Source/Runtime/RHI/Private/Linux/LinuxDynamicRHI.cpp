// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/App.h"
#include "DynamicRHI.h"
#include "Misc/MessageDialog.h"
#include "RHI.h"
#include "Modules/ModuleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformApplicationMisc.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RHIStrings.h"


static TOptional<ERHIFeatureLevel::Type> GetForcedFeatureLevel()
{
	TOptional<ERHIFeatureLevel::Type> ForcedFeatureLevel{};

	if (FParse::Param(FCommandLine::Get(), TEXT("es31")) || FParse::Param(FCommandLine::Get(), TEXT("FeatureLevelES31")) || FParse::Param(FCommandLine::Get(), TEXT("FeatureLevelES3_1")))
	{
		ForcedFeatureLevel = ERHIFeatureLevel::ES3_1;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("sm5")))
	{
		ForcedFeatureLevel = ERHIFeatureLevel::SM5;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("sm6")))
	{
		ForcedFeatureLevel = ERHIFeatureLevel::SM6;
	}

	return ForcedFeatureLevel;
}

FDynamicRHI* PlatformCreateDynamicRHI()
{
	ERHIFeatureLevel::Type RequestedFeatureLevel = ERHIFeatureLevel::SM5;
	FDynamicRHI* DynamicRHI = nullptr;

	const bool bForceVulkan = FParse::Param(FCommandLine::Get(), TEXT("vulkan"));
	bool bForceOpenGL = false;
	if (!bForceVulkan)
	{
		// OpenGL can only be used for mobile preview.
		bForceOpenGL = FParse::Param(FCommandLine::Get(), TEXT("opengl"));
		ERHIFeatureLevel::Type PreviewFeatureLevel;
		bool bUsePreviewFeatureLevel = RHIGetPreviewFeatureLevel(PreviewFeatureLevel);
		if (bForceOpenGL && !bUsePreviewFeatureLevel)
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("LinuxDynamicRHI", "OpenGLRemoved", "Warning: OpenGL is no longer supported for desktop platforms. Vulkan will be used instead."));
			bForceOpenGL = false;
		}
	}

	bool bVulkanFailed = false;
	bool bOpenGLFailed = false;

	IDynamicRHIModule* DynamicRHIModule = nullptr;

	// Read in command line params to force a given feature level
	const TOptional<ERHIFeatureLevel::Type> ForcedFeatureLevel = GetForcedFeatureLevel();
	if (ForcedFeatureLevel.IsSet())
	{
		RequestedFeatureLevel = ForcedFeatureLevel.GetValue();
		DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("VulkanRHI"));
		if (DynamicRHIModule->IsSupported(RequestedFeatureLevel))
		{
			const FString RHIName = FString::Printf(TEXT("Vulkan (%s)"), *LexToString(RequestedFeatureLevel));
			FApp::SetGraphicsRHI(RHIName);
			FPlatformApplicationMisc::UsingVulkan();
		}
		else
		{
			DynamicRHIModule = nullptr;
		}
	}
	else
	{
		TArray<FString> TargetedShaderFormats;
		GConfig->GetArray(TEXT("/Script/LinuxTargetPlatform.LinuxTargetSettings"), TEXT("TargetedRHIs"), TargetedShaderFormats, GEngineIni);

		// First come first serve
		for (int32 SfIdx = 0; SfIdx < TargetedShaderFormats.Num(); ++SfIdx)
		{
			// If we are not forcing opengl and we havent failed to create a VulkanRHI try to again with a different TargetedRHI
			if (!bForceOpenGL && !bVulkanFailed && TargetedShaderFormats[SfIdx].StartsWith(TEXT("SF_VULKAN_")))
			{
				DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("VulkanRHI"));
				if (!DynamicRHIModule->IsSupported())
				{
					DynamicRHIModule = nullptr;
					bVulkanFailed = true;
				}
				else
				{
					FPlatformApplicationMisc::UsingVulkan();

					FName ShaderFormatName(*TargetedShaderFormats[SfIdx]);
					EShaderPlatform TargetedPlatform = ShaderFormatToLegacyShaderPlatform(ShaderFormatName);
					RequestedFeatureLevel = GetMaxSupportedFeatureLevel(TargetedPlatform);

					FString FeatureLevelName;
					GetFeatureLevelName(RequestedFeatureLevel, FeatureLevelName);
					FApp::SetGraphicsRHI(FString::Printf(TEXT("Vulkan (%s)"), *FeatureLevelName));

					break;
				}
			}
			else if (!bForceVulkan && !bOpenGLFailed && TargetedShaderFormats[SfIdx].StartsWith(TEXT("GLSL_")))
			{
				DynamicRHIModule = &FModuleManager::LoadModuleChecked<IDynamicRHIModule>(TEXT("OpenGLDrv"));
				if (!DynamicRHIModule->IsSupported())
				{
					DynamicRHIModule = nullptr;
					bOpenGLFailed = true;
				}
				else
				{
					FApp::SetGraphicsRHI(TEXT("OpenGL"));
					FPlatformApplicationMisc::UsingOpenGL();

					FName ShaderFormatName(*TargetedShaderFormats[SfIdx]);
					EShaderPlatform TargetedPlatform = ShaderFormatToLegacyShaderPlatform(ShaderFormatName);
					RequestedFeatureLevel = GetMaxSupportedFeatureLevel(TargetedPlatform);
					break;
				}
			}
		}
	}

	// Create the dynamic RHI.
	if (DynamicRHIModule)
	{
		DynamicRHI = DynamicRHIModule->CreateRHI(RequestedFeatureLevel);
	}
	else
	{
		if (ForcedFeatureLevel.IsSet())
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("LinuxDynamicRHI", "UnsupportedVulkanTargetedRHI", "Trying to force specific Vulkan feature level but it is not supported."));
			FPlatformMisc::RequestExitWithStatus(true, 1);
			// unreachable
			return nullptr;	
		}
		else if (bForceVulkan)
		{
			if (bVulkanFailed)
			{
				FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("LinuxDynamicRHI", "RequiredVulkan", "Vulkan Driver is required to run the engine."));
			}
			else
			{
				FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("LinuxDynamicRHI", "NoVulkanTargetedRHI", "Trying to force Vulkan RHI but the project does not have it in TargetedRHIs list."));
			}

			FPlatformMisc::RequestExitWithStatus(true, 1);
			// unreachable
			return nullptr;
		}
		else if (bForceOpenGL)
		{
			if (bOpenGLFailed)
			{
				FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("LinuxDynamicRHI", "RequiredOpenGL", "OpenGL 4.3 is required to run the engine."));
			}
			else
			{
				FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("LinuxDynamicRHI", "NoOpenGLTargetedRHI", "Trying to force OpenGL RHI but the project does not have it in TargetedRHIs list."));
			}

			FPlatformMisc::RequestExitWithStatus(true, 1);
			// unreachable
			return nullptr;
		}
		else
		{
			if (bVulkanFailed && bOpenGLFailed)
			{
				FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("LinuxDynamicRHI", "NoVulkanNoGL", "Vulkan or OpenGL (4.3) support is required to run the engine."));
			}
			else
			{
				if (bVulkanFailed)
				{
					FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("LinuxDynamicRHI", "NoVulkanDriver", "Failed to load Vulkan Driver which is required to run the engine.\nThe engine no longer fallbacks to OpenGL4 which has been deprecated."));
				}
				else if (bOpenGLFailed)
				{
					FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("LinuxDynamicRHI", "NoOpenGLDriver", "Failed to load OpenGL Driver which is required to run the engine.\nOpenGL4 has been deprecated and should use Vulkan."));
				}
				else
				{
					FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("LinuxDynamicRHI", "NoTargetedRHI", "The project does not target Vulkan or OpenGL RHIs, check project settings or pass -nullrhi."));
				}
			}

			FPlatformMisc::RequestExitWithStatus(true, 1);
			// unreachable
			return nullptr;
		}
	}

	return DynamicRHI;
}
